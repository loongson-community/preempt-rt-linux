/* linux/drivers/gpio/sm501-gpio.c
 *
 * Copyright (C) 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SM501 MFD GPIO driver
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pci.h>

#include <linux/sm501.h>
#include <linux/sm501-regs.h>
#include <linux/serial_8250.h>

#include <asm/io.h>
#include <asm/gpio.h>

struct sm501_gpio;

struct sm501_gpio_chip {
	struct gpio_chip	gpio;
	struct sm501_gpio	*ourgpio;
	void __iomem		*regbase;
};

struct sm501_gpio {
	struct sm501_gpio_chip	low;
	struct sm501_gpio_chip	high;
	spinlock_t		lock;

	void __iomem		*regs;
	struct resource		*regs_res;
	struct device		*parent;
	struct sm501_platdata	*pdata;
};

static inline struct sm501_gpio_chip *to_sm501_gpio(struct gpio_chip *gc)
{
	return container_of(gc, struct sm501_gpio_chip, gpio);
}


static int sm501_gpio_get(struct gpio_chip *chip, unsigned offset)

{
	struct sm501_gpio_chip *smgpio = to_sm501_gpio(chip);
	unsigned long result;

	result = readl(smgpio->regbase);
	result >>= offset;

	return result & 1UL;
}

static void sm501_gpio_sync_regs(struct sm501_gpio *smgpio)
{
	(void)readl(smgpio->regs);
}

static void sm501_gpio_set(struct gpio_chip *chip, unsigned offset, int value)

{
	struct sm501_gpio_chip *smchip = to_sm501_gpio(chip);
	struct sm501_gpio *smgpio = smchip->ourgpio;
	unsigned long bit = 1 << offset;
	void __iomem *regs = smchip->regbase;
	unsigned long save;
	unsigned long val;

	spin_lock_irqsave(&smgpio->lock, save);

	val = readl(regs) & ~bit;
	if (value)
		val |= bit;
	writel(val, regs);

	sm501_gpio_sync_regs(smgpio);
	spin_unlock_irqrestore(&smgpio->lock, save);
}

static int sm501_gpio_input(struct gpio_chip *chip, unsigned offset)
{
	struct sm501_gpio_chip *smchip = to_sm501_gpio(chip);
	struct sm501_gpio *smgpio = smchip->ourgpio;
	void __iomem *regs = smchip->regbase;
	unsigned long bit = 1 << offset;
	unsigned long save;
	unsigned long ddr;

	spin_lock_irqsave(&smgpio->lock, save);

	ddr = readl(regs + SM501_GPIO_DDR_LOW) & ~bit;
	ddr &= ~bit;
	writel(ddr, regs + SM501_GPIO_DDR_LOW);

	if (smchip == &smgpio->high)
		sm501_configure_gpio(offset + 32, 0);
	else
		sm501_configure_gpio(offset, 0);

	sm501_gpio_sync_regs(smgpio);
	spin_unlock_irqrestore(&smgpio->lock, save);

	return 0;
}

static int sm501_gpio_output(struct gpio_chip *chip,
			     unsigned offset, int value)
{
	struct sm501_gpio_chip *smchip = to_sm501_gpio(chip);
	struct sm501_gpio *smgpio = smchip->ourgpio;
	unsigned long bit = 1 << offset;
	void __iomem *regs = smchip->regbase;
	unsigned long save;
	unsigned long val;
	unsigned long ddr;

	spin_lock_irqsave(&smgpio->lock, save);

	val = readl(regs);
	if (value)
		val |= bit;
	else
		val &= ~bit;
	writel(val, regs);

	ddr = readl(regs + SM501_GPIO_DDR_LOW) & ~bit;
	ddr |= bit;
	writel(ddr, regs + SM501_GPIO_DDR_LOW);
	writel(val, regs);

	if (smchip == &smgpio->high)
		sm501_configure_gpio(offset + 32, 0);
	else
		sm501_configure_gpio(offset, 0);

	sm501_gpio_sync_regs(smgpio);
	spin_unlock_irqrestore(&smgpio->lock, save);

	return 0;
}

static struct gpio_chip gpio_chip_template = {
	.ngpio			= 32,
	.direction_input	= sm501_gpio_input,
	.direction_output	= sm501_gpio_output,
	.set			= sm501_gpio_set,
	.get			= sm501_gpio_get,
};

static int __devinit sm501_gpio_register_chip(struct sm501_gpio *gpio,
					      struct sm501_gpio_chip *chip)
{
	struct sm501_platdata *pdata = gpio->pdata;
	struct gpio_chip *gchip = &chip->gpio;
	int base = pdata->gpio_base;

	if (chip == &gpio->high) {
		if (base > 0)
			base += 32;
		chip->regbase = gpio->regs + SM501_GPIO_DATA_HIGH;
	} else {
		chip->regbase = gpio->regs + SM501_GPIO_DATA_LOW;
	}

	memcpy(chip, &gpio_chip_template, sizeof(struct gpio_chip));

	gchip->label  = "SM501";
	gchip->base   = base;
	chip->ourgpio = gpio;

	return gpiochip_add(gchip);
}

static int __devinit sm501_gpio_probe(struct platform_device *pdev)
{
	struct sm501_gpio *gpio;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;
	int tmp;

	if (dev->parent == NULL) {
		dev_err(dev, "no parent device\n");
		return -EINVAL;
	}

	if (!dev->parent->platform_data) {
		dev_err(dev, "no platform data\n");
		return -EINVAL;
	}

	gpio = kzalloc(sizeof(struct sm501_gpio), GFP_KERNEL);
	if (!gpio) {
		dev_err(dev, "no memory for gpio\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "no resource definition for registers\n");
		ret = -ENOENT;
		goto err_alloc;
	}

	gpio->regs_res = request_mem_region(res->start,
					    res->end - res->start,
					    pdev->name);

	if (gpio->regs_res == NULL) {
		dev_err(dev, "cannot claim registers\n");
		ret = -ENXIO;
		goto err_alloc;
	}

	gpio->regs = ioremap(res->start, (res->end - res->start)+1);
	if (gpio->regs == NULL) {
		dev_err(dev, "cannot remap registers\n");
		ret = -ENXIO;
		goto err_release;
	}

	spin_lock_init(&gpio->lock);

	gpio->pdata = dev->parent->platform_data;
	gpio->parent = dev->parent;

	ret = sm501_gpio_register_chip(gpio, &gpio->low);
	if (ret) {
		dev_err(dev, "failed to add low chip\n");
		goto err_mapped;
	}

	ret = sm501_gpio_register_chip(gpio, &gpio->high);
	if (ret) {
		dev_err(dev, "failed to add high chip\n");
		goto err_low_chip;
	}

	platform_set_drvdata(pdev, gpio);

	dev_info(dev, "registered gpio\n");

	return 0;

 err_low_chip:
	tmp = gpiochip_remove(&gpio->low.gpio);
	if (tmp) {
		dev_err(dev, "cannot remove low chip, cannot tidy up\n");
		return ret;
	}

 err_mapped:
	iounmap(gpio->regs);

 err_release:
	release_resource(gpio->regs_res);
	kfree(gpio->regs_res);

 err_alloc:
	kfree(gpio);
	return ret;
}

static int sm501_gpio_remove(struct platform_device *pdev)
{
	struct sm501_gpio *gpio = platform_get_drvdata(pdev);
	int ret;

	ret = gpiochip_remove(&gpio->high.gpio);
	if (ret) {
		dev_err(&pdev->dev, "failed to remove high chip\n");
		return ret;
	}

	ret = gpiochip_remove(&gpio->low.gpio);
	if (ret) {
		dev_err(&pdev->dev, "failed to remove low chip\n");
		return ret;
	}

	release_resource(gpio->regs_res);
	kfree(gpio->regs_res);
	iounmap(gpio->regs);

	return 0;
}

#ifdef CONFIG_PM
static int sm501_gpio_resume(struct platform_device *pdev)
{
	return 0;
}

static int sm501_gpio_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}
#else
#define sm501_gpio_suspend NULL
#define sm501_gpio_resume  NULL
#endif

static struct platform_driver sm501_gpio_driver = {
	.probe		= sm501_gpio_probe,
	.remove		= sm501_gpio_remove,
	.suspend	= sm501_gpio_suspend,
	.resume		= sm501_gpio_resume,
	.driver		= {
		.name	= "sm501-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __devinit sm501_gpio_init(void)
{
	return platform_driver_register(&sm501_gpio_driver);
}

static void __exit sm501_gpio_cleanup(void)
{
	platform_driver_unregister(&sm501_gpio_driver);
}

module_init(sm501_gpio_init);
module_exit(sm501_gpio_cleanup);

MODULE_ALIAS("platform:sm501-gpio");
MODULE_AUTHOR("Ben Dooks");
MODULE_DESCRIPTION("SM501 GPIOlib support");
MODULE_LICENSE("GPL v2");

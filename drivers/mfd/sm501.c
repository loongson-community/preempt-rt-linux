/* linux/drivers/mfd/sm50x.c
 *
 * Copyright (C) 2007 SiliconMotion Inc.
 * Copyright (c) 2006 Simtec Electronics
 *
 *	Ben Dooks <ben@simtec.co.uk>
 *	Vincent Sanders <vince@simtec.co.uk>
 *	Boyod.yang,  <boyod.yang@siliconmotion.com.cn>
 *
 *	
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SM50x MFD driver
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pci.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif
#define SM501_CHAR_MAJOR 245
#include <linux/sm501.h>
#include <linux/sm501-regs.h>
#include <linux/sm501_regs.h>

#include <asm/io.h>
#include <asm/uaccess.h>

unsigned char __iomem *sm50x_base_reg = NULL;
EXPORT_SYMBOL_GPL(sm50x_base_reg);
unsigned char __iomem *sm50x_base_mem = NULL;
resource_size_t sm50x_mem_size = 0;
EXPORT_SYMBOL_GPL(sm50x_mem_size);

static unsigned int sm501_mem_local[] = {
	[0]	= 4*1024*1024,
	[1]	= 8*1024*1024,
	[2]	= 16*1024*1024,
	[3]	= 32*1024*1024,
	[4]	= 64*1024*1024,
	[5]	= 2*1024*1024,
};
/* static unsigned int sm501_mem_external[] = {
	[0]	= 2*1024*1024,
	[1]	= 4*1024*1024,
	[4]	= 54*1024*1024,
	[5]	= 32*1024*1024,
	[6]	= 16*1024*1024,
	[7]	= 8*1024*1024,
}; */

struct sm501_sub_device_list {
	struct list_head		list;
	struct platform_device		pdev;
};

struct sm501_devdata {
	spinlock_t			 reg_lock;
	struct mutex			 clock_lock;
	struct list_head		 devices;

	struct device			*dev;
	struct resource			*io_res;
	struct resource			*mem_res;
	struct resource			*regs_claim;
	struct resource			*mem_claim;
	struct sm501_platdata		*platdata;

	unsigned int			 pdev_id;
	u8					 chipRevID;
	unsigned int			 irq;
	void __iomem			*regs;
	void __iomem			*fb_mem;

	/* mtrr support */
	int mtrr_reg;
	u32 has_mtrr;
};

static void sm501_sync_regs(void)
{
	SmRead32(0);
}

/* Perform a rounded division. */
static long sm501fb_round_div(long num, long denom)
{
        /* n / d + 1 / 2 = (2n + d) / 2d */
        return (2 * num + denom) / (2 * denom);
}

void sm501_configure_gpio(unsigned int gpio, unsigned char mode)
{
	unsigned long conf;
	unsigned long reg;
	unsigned long offset = gpio;

	if (offset >= 32) {
		reg = SM501_GPIO63_32_CONTROL;
		offset = gpio - 32;
	} else
		reg = SM501_GPIO31_0_CONTROL;

	conf = readl(sm50x_base_reg + reg);
	conf &= ~(1 << offset);
	if (mode)
		conf |= (1 << offset);
	writel(conf, sm50x_base_reg + reg);
}
EXPORT_SYMBOL_GPL(sm501_configure_gpio);

/* clock value structure. */
struct sm501_clock {
	unsigned long mclk;
	int divider;
	int shift;

	long multipleM;
	int dividerN;
	short divby2;
};

/* sm501_select_clock
 *
 * selects nearest discrete clock frequency the SM501 can achive
 *   the maximum divisor is 3 or 5
 */
static unsigned long sm501_select_clock(unsigned long freq,
					struct sm501_clock *clock,
					int max_div)
{
	unsigned long mclk;
	int divider;
	int shift;
	long diff;
	long best_diff = 999999999;

	/* Try 288MHz and 336MHz clocks. */
	for (mclk = 288000000; mclk <= 336000000; mclk += 48000000) {
		/* try dividers 1 and 3 for CRT and for panel,
		   try divider 5 for panel only.*/

		for (divider = 1; divider <= max_div; divider += 2) {
			/* try all 8 shift values.*/
			for (shift = 0; shift < 8; shift++) {
				/* Calculate difference to requested clock */
				diff = sm501fb_round_div(mclk, divider << shift) - freq;
				if (diff < 0)
					diff = -diff;

				/* If it is less than the current, use it */
				if (diff < best_diff) {
					best_diff = diff;

					clock->mclk = mclk;
					clock->divider = divider;
					clock->shift = shift;
				}
			}
		}
	}

	/* Return best clock. */
	return clock->mclk / (clock->divider << clock->shift);
}


#ifdef CONFIG_MTRR

/* sm501fb_set_mtrr
 *
 * mtrr support functions 
*/

void  sm501fb_set_mtrr(struct device *dev)
{
	struct sm501_devdata *sm = dev_get_drvdata(dev);

	sm->mtrr_reg = mtrr_add(sm->mem_res->start,
				   ((sm->mem_res->end - sm->mem_res->start) - 2*0x10000)-1, MTRR_TYPE_WRCOMB, 1);
	if (sm->mtrr_reg < 0) {
		dev_info(sm->dev,"unable to set MTRR\n");
		return;
	}
	sm->has_mtrr = 1;
}
EXPORT_SYMBOL_GPL(sm501fb_set_mtrr);

/* sm501fb_unset_mtrr
 *
 * mtrr support functions 
*/

void sm501fb_unset_mtrr(struct device *dev)
{
	struct sm501_devdata *sm = dev_get_drvdata(dev);

  	if (sm->has_mtrr)
  		mtrr_del(sm->mtrr_reg, sm->mem_res->start,
			  ((sm->mem_res->end - sm->mem_res->start) - 2*0x10000)-1);
}
EXPORT_SYMBOL_GPL(sm501fb_unset_mtrr);

#endif


/* sm501_get_memory_size
 *
 * get memory size of sm501
*/
static resource_size_t sm501_get_memory_size(void)
{
#ifdef CONFIG_SM501_USE_EXTERNAL_MEMORY
	u32 value;
#if CONFIG_GDIUM_VERSION < 3
	value= SmRead32(DRAM_CONTROL);
	SmWrite32(DRAM_CONTROL, FIELD_SET(value,DRAM_CONTROL, EXTERNAL_SIZE, 32MB)|FIELD_SET(0,DRAM_CONTROL, LOCAL_SIZE, 32MB)|
		FIELD_SET(0,DRAM_CONTROL, EXTERNAL_COLUMN_SIZE  , 512)|FIELD_SET(0,DRAM_CONTROL, LOCAL_COLUMN_SIZE  , 512));
#endif
	return sm501_mem_external[ FIELD_GET(SmRead32(DRAM_CONTROL),DRAM_CONTROL,EXTERNAL_SIZE) ];
#else
	return sm501_mem_local[ FIELD_GET(SmRead32(DRAM_CONTROL),DRAM_CONTROL,LOCAL_SIZE) ];
#endif
}

/* sm501_set_clock
 *
 * set one of the four clock sources to the closest available frequency to
 *  the one specified
*/
unsigned long sm501_set_clock(struct device *dev,
			      int clksrc,
			      unsigned long req_freq)
{
	struct sm501_devdata *sm = dev_get_drvdata(dev);
	unsigned long mode = SmRead32(POWER_MODE_CTRL);
	unsigned long clock = SmRead32(CURRENT_POWER_CLOCK);
	unsigned char reg = 0;
	unsigned long sm501_freq,pllclock; /* the actual frequency acheived */

	struct sm501_clock to;

	/* find achivable discrete frequency and setup register value
	 * accordingly, V2XCLK, MCLK and M1XCLK are the same P2XCLK
	 * has an extra bit for the divider */

	switch (clksrc) {
	case SM501_CLOCK_P2XCLK:
		/* This clock is divided in half so to achive the
		 * requested frequency the value must be multiplied by
		 * 2. This clock also has an additional pre divisor */
		if (sm->chipRevID < SM502_REV_ID){
			sm501_freq = (sm501_select_clock(2 * req_freq, &to, 5) / 2);
			reg=to.shift & 0x07;/* bottom 3 bits are shift */
			if (to.divider == 3)
				reg |= 0x08; /* /3 divider required */
			else if (to.divider == 5)
				reg |= 0x10; /* /5 divider required */
			if (to.mclk != 288000000)
				reg |= 0x20; /* which mclk pll is source */
		}else
		{
			/*For new PLL in SM502*/
			to.multipleM = (2 * req_freq)/(1000*1000);
			to.dividerN = 24;
			to.divby2 = 0;

			while (to.multipleM > 192){
			to.multipleM /= 2;
			to.dividerN /= 2;
			}
			sm501_freq = req_freq;
		}
		break;

	case SM501_CLOCK_V2XCLK:
		/* This clock is divided in half so to achive the
		 * requested frequency the value must be multiplied by 2. */

		sm501_freq = (sm501_select_clock(2 * req_freq, &to, 3) / 2);
		reg=to.shift & 0x07;	/* bottom 3 bits are shift */
		if (to.divider == 3)
			reg |= 0x08;	/* /3 divider required */
		if (to.mclk != 288000000)
			reg |= 0x10;	/* which mclk pll is source */
		break;

	case SM501_CLOCK_MCLK:
	case SM501_CLOCK_M1XCLK:
		/* These clocks are the same and not further divided */

		sm501_freq = sm501_select_clock( req_freq, &to, 3);
		reg=to.shift & 0x07;	/* bottom 3 bits are shift */
		if (to.divider == 3)
			reg |= 0x08;	/* /3 divider required */
		if (to.mclk != 288000000)
			reg |= 0x10;	/* which mclk pll is source */
		break;

	default:
		return 0; /* this is bad */
	}

	mutex_lock(&sm->clock_lock);

	mode = SmRead32(POWER_MODE_CTRL);
	clock = SmRead32(CURRENT_POWER_CLOCK);
	
	clock = clock & ~(0xFF << clksrc);
	clock |= reg<<clksrc;

		/*For new PLL in SM502*/
	if ( (sm->chipRevID >= SM502_REV_ID)&& (clksrc==SM501_CLOCK_P2XCLK) ){

		pllclock = SmRead32(CURRENT_POWER_PLLCLOCK);

		pllclock &= FIELD_CLEAR(CURRENT_POWER_PLLCLOCK, MULTIPLE_M)
				  &  FIELD_CLEAR(CURRENT_POWER_PLLCLOCK, DIVIDE_N)
				  &  FIELD_CLEAR(CURRENT_POWER_PLLCLOCK, DIVIDEBY2)
					&  FIELD_CLEAR(CURRENT_POWER_PLLCLOCK, INPUT_SELECT)
					  &  FIELD_CLEAR(CURRENT_POWER_PLLCLOCK, POWER);
		
		SmWrite32(CURRENT_POWER_PLLCLOCK,
				  FIELD_VALUE(pllclock, CURRENT_POWER_PLLCLOCK, MULTIPLE_M, to.multipleM)|
					  FIELD_VALUE(pllclock, CURRENT_POWER_PLLCLOCK, DIVIDE_N, to.dividerN)|
				              FIELD_VALUE(pllclock, CURRENT_POWER_PLLCLOCK, DIVIDEBY2, to.divby2)|
				              FIELD_SET(pllclock, CURRENT_POWER_PLLCLOCK, POWER, ON)|
				              FIELD_SET(pllclock, CURRENT_POWER_PLLCLOCK, INPUT_SELECT,CRYSTAL)|
				              FIELD_SET(pllclock, CURRENT_POWER_PLLCLOCK, TEST_OUTPUT, DISABLE)|
				              FIELD_SET(pllclock, CURRENT_POWER_PLLCLOCK, TESTMODE, DISABLE));		

		clock = (FIELD_SET(clock, CURRENT_POWER_CLOCK, PLLCLK_SELECT, ENABLE)|
					FIELD_SET(clock, CURRENT_POWER_CLOCK, P2XCLK_DIVIDER, 1)|
					FIELD_SET(clock, CURRENT_POWER_CLOCK, P2XCLK_SHIFT, 0)  );
		}	
	
	mode = FIELD_GET(mode, POWER_MODE_CTRL, MODE);	/* find current mode */

	switch (mode) {
	case 0:
		SmWrite32(POWER_MODE0_CLOCK, clock);
		mode = 0;
		break;
	case 1:
		SmWrite32(POWER_MODE1_CLOCK, clock);
		mode = 1;
		break;

	default:
		mutex_unlock(&sm->clock_lock);
		return -1;
	}

	sm501_sync_regs();

	dev_dbg(sm->dev, "clock %08lx, mode %08lx\n",
		  clock, mode);

	msleep(16);
	mutex_unlock(&sm->clock_lock);

	return sm501_freq;
}

EXPORT_SYMBOL_GPL(sm501_set_clock);


/* sm501_set_gate
 *
 * set one of the four gate sources to the closest available frequency to
 *  the one specified
*/

void sm501_set_gate( unsigned long gate)
{

	unsigned long mode = SmRead32(POWER_MODE_CTRL);
	mode = FIELD_GET(mode, POWER_MODE_CTRL, MODE);	

	switch (mode) {
	case 0:
		SmWrite32(POWER_MODE0_GATE, gate);
		break;
	case 1:
		SmWrite32(POWER_MODE1_GATE, gate);
		break;
	default:
		return;
	}

	sm501_sync_regs();
	msleep(16);
}

EXPORT_SYMBOL_GPL(sm501_set_gate);
			      
/* sm501_find_clock
 *
 * finds the closest available frequency for a given clock
*/

unsigned long sm501_find_clock(int clksrc,
			       unsigned long req_freq)
{
	unsigned long sm501_freq; /* the frequency achiveable by the 501 */
	struct sm501_clock to;

	switch (clksrc) {
	case SM501_CLOCK_P2XCLK:
		sm501_freq = (sm501_select_clock(2 * req_freq, &to, 5) / 2);
		break;

	case SM501_CLOCK_V2XCLK:
		sm501_freq = (sm501_select_clock(2 * req_freq, &to, 3) / 2);
		break;

	case SM501_CLOCK_MCLK:
	case SM501_CLOCK_M1XCLK:
		sm501_freq = sm501_select_clock(req_freq, &to, 3);
		break;

	default:
		sm501_freq = 0;		/* error */
	}

	return sm501_freq;
}

EXPORT_SYMBOL_GPL(sm501_find_clock);


/* sm501_dbg_regs
 *
 * Debug attribute to attach to parent device to show core registers
*/

static ssize_t sm50x_dbg_regs(struct device *dev,
			      struct device_attribute *attr, char *buff)
{
	unsigned long reg;
	char *ptr = buff;
	int ret;

	for (reg = 0x00; reg < 0x70; reg += 4) {
		ret = sprintf(ptr, "%016lx = %016lx\n", reg, SmRead32(reg));
		ptr += ret;
	}

	return ptr - buff;
}


static DEVICE_ATTR(dbg_regs, 0666, sm50x_dbg_regs, NULL);


static struct sm501_sub_device_list *get_sm_dev_list(struct platform_device *pdev)
{
	return container_of(pdev, struct sm501_sub_device_list, pdev);
}

/* sm501_devices_release
 *
 * A release function for the platform devices we create to allow us to
 * free any items we allocated
*/

static void sm501_devices_release(struct device *dev)
{
	kfree(get_sm_dev_list(to_platform_device(dev)));
}

/* sm501_create_subdev
 *
 * Create a sub platform device with resources for passing to a
 * sub-driver
*/

static struct platform_device *
sm501_create_subdev(struct sm501_devdata *sm,
		    char *name, unsigned int res_count)
{
	struct sm501_sub_device_list *smdevs;

	smdevs = kzalloc(sizeof(struct sm501_sub_device_list) +
			sizeof(struct resource) * res_count, GFP_KERNEL);
	if (!smdevs)
		return NULL;

	smdevs->pdev.dev.release = sm501_devices_release;

	smdevs->pdev.name = name;
	smdevs->pdev.id = sm->pdev_id;
	smdevs->pdev.resource = (struct resource *)(smdevs+1);
	smdevs->pdev.num_resources = res_count;

	smdevs->pdev.dev.parent = sm->dev;

	return &smdevs->pdev;
}

/* sm501_register_device
 *
 * Register a platform device created with sm501_create_subdev()
*/

static int sm501_register_device(struct sm501_devdata *sm,
				 struct platform_device *pdev)
{
	struct sm501_sub_device_list *smdevs = get_sm_dev_list(pdev);
	int ptr;
	int ret;

	for (ptr = 0; ptr < pdev->num_resources; ptr++) {
		if (pdev->resource[ptr].flags == IORESOURCE_IRQ)
			printk(KERN_INFO "%s[%d] irq %d\n",
					pdev->name, ptr,
					(int)pdev->resource[ptr].start);
		else
			printk(KERN_INFO "%s[%d] flags %08lx: %016lx..%016lx\n",
			       pdev->name, ptr,
			       pdev->resource[ptr].flags,
			       pdev->resource[ptr].start,
			       pdev->resource[ptr].end);
	}

	ret = platform_device_register(pdev);

	if (ret >= 0) {
		dev_dbg(sm->dev, "registered %s\n", pdev->name);
		list_add_tail(&smdevs->list, &sm->devices);
	} else
		dev_err(sm->dev, "error registering %s (%d)\n",
			pdev->name, ret);

	return ret;
}

/* sm501_create_subio
 *
 * Fill in an IO resource for a sub device
*/

static void sm501_create_subio(struct sm501_devdata *sm,
			       struct resource *res,
			       resource_size_t offs,
			       resource_size_t size)
{
	res->flags = IORESOURCE_MEM;
	res->parent = sm->io_res;
	res->start = sm->io_res->start + offs;
	res->end = res->start + size - 1;
}

/* sm501_create_mem
 *
 * Fill in an MEM resource for a sub device
*/

static void sm501_create_mem(struct sm501_devdata *sm,
			     struct resource *res,
			     resource_size_t offs,
			     resource_size_t size)
{


	res->flags = IORESOURCE_MEM;
	res->parent = sm->mem_res;
	res->start = sm->mem_res->start + offs;
	res->end = res->start + size - 1;
}

/* sm501_create_irq
 *
 * Fill in an IRQ resource for a sub device
*/

static void sm501_create_irq(struct sm501_devdata *sm,
			     struct resource *res)
{
	res->flags = IORESOURCE_IRQ;
	res->parent = NULL;
	res->start = res->end = sm->irq;
}

static void sm501_fill_usbdev(struct device *dev, struct platform_device *pdev)
{
	pdev->dev.power.power_state = PMSG_ON;
	/*Add for usb dma mask*/
	pdev->dev.coherent_dma_mask = ~0x0;
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;
}

static int sm501_register_usbhost(struct sm501_devdata *sm, 
				  struct device *dev)
{
	struct platform_device *pdev;
	/*If the chip is SM107 just return without doing anything*/
	if (sm50x_mem_size == 0x400000)
		return 0;
	
	pdev = sm501_create_subdev(sm, "sm501-usb", 3);
	if (!pdev)
		return -ENOMEM;
	sm501_fill_usbdev(dev, pdev);
	sm501_create_subio(sm, &pdev->resource[0], 0x40000, 0x20000);
	sm501_create_mem(sm, &pdev->resource[1], sm50x_mem_size-USB_DMA_BUFFER_SIZE, USB_DMA_BUFFER_SIZE);
	sm501_create_irq(sm, &pdev->resource[2]);

	return sm501_register_device(sm, pdev);
}

static int sm501_register_display(struct sm501_devdata *sm)
{
	struct platform_device *pdev;

	pdev = sm501_create_subdev(sm, "sm501-fb", 4);
	if (!pdev)
		return -ENOMEM;

	sm501_create_subio(sm, &pdev->resource[0], 0x80000, 0x10000);
	sm501_create_subio(sm, &pdev->resource[1], 0x100000, 0x50000);
	sm501_create_mem(sm, &pdev->resource[2], 0, 
		sm50x_mem_size-USB_DMA_BUFFER_SIZE);
	sm501_create_irq(sm, &pdev->resource[3]);

	return sm501_register_device(sm, pdev);
}

static int sm501_register_uart(struct sm501_devdata *sm)
{
	struct platform_device *pdev;
	/*If the chip is SM107 just return without doing anything*/
	if (sm50x_mem_size == 0x400000)
		return 0;
	
	pdev = sm501_create_subdev(sm, "sm501-uart", 2);
	if (!pdev)
		return -ENOMEM;

	sm501_create_subio(sm, &pdev->resource[0], 0x30000, 0x10000);
	sm501_create_irq(sm, &pdev->resource[1]);

	return sm501_register_device(sm, pdev);
}

static int sm501_register_audio(struct sm501_devdata *sm)
{
	struct platform_device *pdev;

	/*If the chip is SM107 just return without doing anything*/
	if (sm50x_mem_size == 0x400000)
		return 0;

	
	pdev = sm501_create_subdev(sm, "sm501-audio", 2);
	if (!pdev)
		return -ENOMEM;
	sm501_create_subio(sm, &pdev->resource[0], 0x0A0000, 0x10000);
	sm501_create_irq(sm, &pdev->resource[1]);

	return sm501_register_device(sm, pdev);
}

static int sm501_register_gpio(struct sm501_devdata *sm)
{
	struct platform_device *pdev;

	pdev = sm501_create_subdev(sm, "sm501-gpio", 4);
	if (!pdev)
		return -ENOMEM;

	sm501_create_subio(sm, &pdev->resource[0], SM501_GPIO, 0x20);
	return sm501_register_device(sm, pdev);
}

static int sm501_register_pwm(struct sm501_devdata *sm)
{
	struct platform_device *pdev;

	/*If the chip is SM107 just return without doing anything*/
	if (sm50x_mem_size == 0x400000)
		return 0;

	pdev = sm501_create_subdev(sm, "sm501-pwm", 2);
	if (!pdev)
		return -ENOMEM;
	sm501_create_subio(sm, &pdev->resource[0], 0x10020, 0xC);
	sm501_create_irq(sm, &pdev->resource[1]);

	return sm501_register_device(sm, pdev);
}

/* sm501_init_regs
 *
 * Setup core register values
*/

static void sm501_init_regs(struct sm501_devdata *sm,
			    struct sm501_initdata *init)
{
	ulong RegValue;
	/*Set the local memory number of banks according to the chip type*/
	sm50x_mem_size = sm501_get_memory_size();

	RegValue= SmRead32(DRAM_CONTROL);
	if (sm50x_mem_size == 0x400000){
		dev_info(sm->dev, "SM107 is detected.\n");
		RegValue = FIELD_SET(RegValue, DRAM_CONTROL, LOCAL_BANKS, 2);
	}
	else	{
		dev_info(sm->dev, "SM502 is detected.\n");
		RegValue = FIELD_SET(RegValue, DRAM_CONTROL, LOCAL_BANKS, 4);
	}
	SmWrite32(DRAM_CONTROL, RegValue);
	
/*
	RegValue = SmRead32(	SYSTEM_CTRL);
	SmWrite32(SYSTEM_CTRL, FIELD_SET(RegValue, SYSTEM_CTRL, PCI_BURST, ENABLE)|
//			FIELD_SET(RegValue, SYSTEM_CTRL, PCI_MASTER, START)|
			FIELD_SET(RegValue, SYSTEM_CTRL, PCI_BURST_READ, ENABLE)|
			FIELD_SET(RegValue, SYSTEM_CTRL, PCI_RETRY, ENABLE)|
			FIELD_SET(RegValue, SYSTEM_CTRL, PCI_CLOCK_RUN, ENABLE));
	
	RegValue = SmRead32(MISC_CTRL);
	SmWrite32(MISC_CTRL, FIELD_SET(RegValue, MISC_CTRL, FPDATA, 24)|
		FIELD_SET(RegValue, MISC_CTRL, BURST_LENGTH, 1));

#ifdef CONFIG_SYS_HAS_CPU_LOONGSON2
//	SmWrite32(PCI_MASTER_BASE,0x80000000);
#endif
*/
	SmWrite32(GPIO_CONTROL_HIGH, 
		FIELD_SET(RegValue,GPIO_CONTROL_HIGH, 56, CRT0_ZVPORT8_PANEL0) ||		
		FIELD_SET(RegValue,GPIO_CONTROL_HIGH, 57, CRT1_ZVPORT9_PANEL1) ||
		FIELD_SET(RegValue,GPIO_CONTROL_HIGH, 58, CRT2_ZVPORT10_PANEL8) ||
		FIELD_SET(RegValue,GPIO_CONTROL_HIGH, 59, CRT3_ZVPORT11_PANEL9) ||
		FIELD_SET(RegValue,GPIO_CONTROL_HIGH, 60, CRT4_ZVPORT12_PANEL16) ||
		FIELD_SET(RegValue,GPIO_CONTROL_HIGH, 61, CRT5_ZVPORT13_PANEL17) );

	if (init->mclk) {
		dev_dbg(sm->dev, "setting MCLK to %ld\n", init->mclk);
		sm501_set_clock(sm->dev, SM501_CLOCK_MCLK, init->mclk);
	}

	if (init->m1xclk) {
		dev_dbg(sm->dev, "setting M1XCLK to %ld\n", init->m1xclk);
		sm501_set_clock(sm->dev, SM501_CLOCK_M1XCLK, init->m1xclk);
	
	}
}

/* sm501_init_dev
 *
 * Common init code for an SM501
*/

static int sm501_init_dev(struct sm501_devdata *sm, struct device *dev)
{
	int ret;
	

	mutex_init(&sm->clock_lock);
	spin_lock_init(&sm->reg_lock);

	INIT_LIST_HEAD(&sm->devices);

	/*Set the local memory number of banks according to the chip type*/
	sm50x_mem_size = sm501_get_memory_size();
	dev_info(sm->dev, "SM501 At %p: Version %08x, %ld Mb, IRQ %d\n",
		 sm->regs, (unsigned int)SmRead32(DEVICE_ID),(unsigned long)sm50x_mem_size >> 20, (int)sm->irq);

	dev_dbg(sm->dev, "CurrentGate      %016lx\n", SmRead32(CURRENT_GATE));
	dev_dbg(sm->dev, "CurrentClock     %016lx\n", SmRead32(CURRENT_POWER_CLOCK));
	dev_dbg(sm->dev, "PowerModeControl %016lx\n", SmRead32(POWER_MODE_CTRL));

	ret = device_create_file(sm->dev, &dev_attr_dbg_regs);
	if (ret)
		dev_err(sm->dev, "failed to create debug regs file\n");

	/* check to see if we have some device initialisation */

	if (sm->platdata) {
		struct sm501_platdata *pdata = sm->platdata;

		if (pdata->init) {
			sm501_init_regs(sm, sm->platdata->init);

			if (pdata->init->devices & SM501_USE_FB)		
			sm501_register_display(sm);

			if (pdata->init->devices & SM501_USE_USB_HOST )
				sm501_register_usbhost(sm, dev);
			if (pdata->init->devices & SM501_USE_AC97)
				sm501_register_audio(sm);
			if ((pdata->init->devices & SM501_USE_UART0) ||
				(pdata->init->devices & SM501_USE_UART1))
				sm501_register_uart(sm);
			if (pdata->init->devices & SM501_USE_PWM)
				sm501_register_pwm(sm);
			if (pdata->init->devices & SM501_USE_GPIO)
				sm501_register_gpio(sm);
		}
	}

	return 0;
}

static int sm501_plat_probe(struct platform_device *dev)
{
	struct sm501_devdata *sm;
	int err;

	sm = kzalloc(sizeof(struct sm501_devdata), GFP_KERNEL);
	if (sm == NULL) {
		dev_err(&dev->dev, "no memory for device data\n");
		err = -ENOMEM;
		goto err1;
	}

	sm->dev = &dev->dev;
	sm->pdev_id = dev->id;
	sm->irq = platform_get_irq(dev, 0);
	sm->io_res = platform_get_resource(dev, IORESOURCE_MEM, 1);
	sm->mem_res = platform_get_resource(dev, IORESOURCE_MEM, 0);
	sm->platdata = dev->dev.platform_data;

	if (sm->irq < 0) {
		dev_err(&dev->dev, "failed to get irq resource\n");
		err = sm->irq;
		goto err_res;
	}

	if (sm->io_res == NULL || sm->mem_res == NULL) {
		dev_err(&dev->dev, "failed to get IO resource\n");
		err = -ENOENT;
		goto err_res;
	}

	/* check register */
	sm->regs_claim = request_mem_region(sm->io_res->start,
					    0x100, "sm501");

	if (sm->regs_claim == NULL) {
		dev_err(&dev->dev, "cannot claim registers\n");
		err= -EBUSY;
		goto err_res;
	}

	platform_set_drvdata(dev, sm);

	/* mapping register */
	sm->regs = sm50x_base_reg = ioremap(sm->io_res->start,
			   (sm->io_res->end - sm->io_res->start) - 1);

	/*
	 * By Belcon:
	 * mapping framebuffer
	 */
//	sm->fb_mem = ioremap(sm->mem_res->start, sm50x_mem_size - 1);

	if (sm->regs == NULL) {
		dev_err(&dev->dev, "cannot remap registers\n");
		err = -EIO;
		goto err_claim;
	}

	sm->chipRevID = FIELD_GET(SmRead32(DEVICE_ID), DEVICE_ID, REVISION);
	
	return sm501_init_dev(sm, &dev->dev);

 err_claim:
	release_resource(sm->regs_claim);
	kfree(sm->regs_claim);
 err_res:
	kfree(sm);
 err1:
	return err;

}


/* Initialisation data for graphic modules */
static struct sm501_platdata_fbsub sm501_pdata_fbsub = {
	.flags		= (SM501FB_FLAG_USE_INIT_MODE |
			   SM501FB_FLAG_USE_HWCURSOR |
			   SM501FB_FLAG_USE_HWACCEL |
			   SM501FB_FLAG_DISABLE_AT_EXIT),
};

static struct sm501_platdata_fb sm501_fb_pdata = {
	.fb_crt		= &sm501_pdata_fbsub,
	.fb_pnl		= &sm501_pdata_fbsub,
};

/* Initialisation data for PCI devices */
static struct sm501_initdata sm501_pci_initdata = {
	.devices	= SM501_USE_ALL,
#ifdef CONFIG_SM501_USE_EXTERNAL_MEMORY
	.mclk		= 84 * MHZ,
	.m1xclk		= 112 * MHZ,
#else		
	.mclk		= 72 * MHZ,
	.m1xclk		= 144 * MHZ,
#endif	
};

static struct sm501_platdata sm501_pci_platdata = {
	.init		= &sm501_pci_initdata,
	.fb		= &sm501_fb_pdata,
	.gpio_base	= -1,
};

static int sm501_pci_probe(struct pci_dev *dev,
			   const struct pci_device_id *id)
{
	struct sm501_devdata *sm;
	int err;
	unsigned long	mem_start, mem_end, mem_length;

	sm = kzalloc(sizeof(struct sm501_devdata), GFP_KERNEL);
	if (sm == NULL) {
		dev_err(&dev->dev, "no memory for device data\n");
		err = -ENOMEM;
		goto err1;
	}

	/* set a default set of platform data */
	dev->dev.platform_data = sm->platdata = &sm501_pci_platdata;

	/* set a hopefully unique id for our child platform devices */
	sm->pdev_id = 32 + dev->devfn;

	pci_set_drvdata(dev, sm);

	err = pci_enable_device(dev);
	if (err) {
		dev_err(&dev->dev, "cannot enable device\n");
		goto err2;
	}

	sm->dev = &dev->dev;
	sm->irq = dev->irq;

#ifdef __BIG_ENDIAN
	/* if the system is big-endian, we most probably have a
	 * translation in the IO layer making the PCI bus little endian
	 * so make the framebuffer swapped pixels */
	sm->platdata->init->flags |= SM501_FBPD_SWAP_FB_ENDIAN;
	sm->platdata->fb->flags |= SM501_FBPD_SWAP_FB_ENDIAN;
#endif

	/* check our resources */

	if (!(pci_resource_flags(dev, 0) & IORESOURCE_MEM)) {
		dev_err(&dev->dev, "region #0 is not memory?\n");
		err = -EINVAL;
		goto err3;
	}

	if (!(pci_resource_flags(dev, 1) & IORESOURCE_MEM)) {
		dev_err(&dev->dev, "region #1 is not memory?\n");
		err = -EINVAL;
		goto err3;
	}

	pci_read_config_byte(dev, PCI_REVISION_ID, &sm->chipRevID);

	/* make our resources ready for sharing */

	sm->io_res = &dev->resource[1];
	sm->mem_res = &dev->resource[0];

	/* check register */
	sm->regs_claim = request_mem_region(sm->io_res->start,
					    0x100, "sm501");
	if (sm->regs_claim == NULL) {
		dev_err(&dev->dev, "cannot claim registers\n");
		err= -EBUSY;
		goto err3;
	}

	/* mapping register */
	sm->regs = sm50x_base_reg = ioremap(pci_resource_start(dev, 1),
			   pci_resource_len(dev, 1));
	if (sm->regs == NULL) {
		dev_err(&dev->dev, "cannot remap registers\n");
		err = -EIO;
		goto err4;
	}

	mem_start = pci_resource_start(dev, 0);
	mem_end = pci_resource_end(dev, 0);
	mem_length = pci_resource_len(dev, 0);
	sm501_init_dev(sm, &dev->dev);
	/*
	 * By Belcon:
	 * mapping framebuffer
	 */
	sm->fb_mem = sm50x_base_mem = ioremap(mem_start, mem_length);

	return 0;

 err4:
	release_resource(sm->regs_claim);
	kfree(sm->regs_claim);
 err3:
	pci_disable_device(dev);
 err2:
	pci_set_drvdata(dev, NULL);
	kfree(sm);
 err1:
	return err;
}

static void sm501_remove_sub(struct sm501_devdata *sm,
			     struct sm501_sub_device_list *smdevs)
{
	list_del(&smdevs->list);
	platform_device_unregister(&smdevs->pdev);
}

static void sm501_dev_remove(struct sm501_devdata *sm)
{
	struct sm501_sub_device_list *smdevs, *tmp;

	list_for_each_entry_safe(smdevs, tmp, &sm->devices, list)
		sm501_remove_sub(sm, smdevs);

	device_remove_file(sm->dev, &dev_attr_dbg_regs);
}

static void sm501_pci_remove(struct pci_dev *dev)
{
	struct sm501_devdata *sm = pci_get_drvdata(dev);

	sm501_dev_remove(sm);
	iounmap(sm->regs);

	release_resource(sm->regs_claim);
	kfree(sm->regs_claim);

	pci_set_drvdata(dev, NULL);
	pci_disable_device(dev);
}

static int sm501_plat_remove(struct platform_device *dev)
{
	struct sm501_devdata *sm = platform_get_drvdata(dev);

	sm501_dev_remove(sm);
	iounmap(sm->regs);

	release_resource(sm->regs_claim);
	kfree(sm->regs_claim);

	return 0;
}
/*static int sm501_chr_open(struct inode *inode, struct file *flip)
{
	return 0;
}
static int sm501_chr_release(struct inode *inode, struct file *flip)
{
	return 0;
}

static int sm501_chr_ioctl(struct inode *inode, struct file *flip, u32 cmd, 
	unsigned long arg)
{
	switch (cmd)
	{
		case SM501_IOCTL_SMREAD:
		{
			PREGDATA pregdata;
			copy_from_user(pregdata, (void *)arg, sizeof(PREGDATA));
			pregdata->data = SmRead32(pregdata->address);
			copy_to_user((void *)arg, pregdata, sizeof(PREGDATA));
			break;
		}
		case SM501_IOCTL_SMWRITE:
		{
			PREGDATA pregdata;
			copy_from_user(pregdata, (void *)arg, sizeof(PREGDATA));
			SmWrite32(pregdata->address, pregdata->data);
			break;
		}
		case SM501_GET_CURSOR:
		{
			PREGDATA pregdata;
			copy_from_user(pregdata, (void *)arg, sizeof(PREGDATA));
			pregdata->data = sm50x_base_mem + pregdata->address;
			copy_to_user((void *)arg, pregdata, sizeof(PREGDATA));
			break;
		}
		case SM501_SET_CURSOR:
		{
			PCURSORDATA pcursor_data;
			copy_from_user(pcursor_data, (void *)arg, sizeof(PCURSORDATA));
			memcpy(sm50x_base_mem + pcursor_data->address, pcursor_data->data, 64 * 16);
			break;
		}
		default:
		{
			printk("No such command!\n");
		}
	}
	return 0;
} */
/*static struct file_operations sm501_chr_fops = {
	.owner		= THIS_MODULE,
	.open		= sm501_chr_open,
	.release	= sm501_chr_release,
	.ioctl		= sm501_chr_ioctl,
}; */

static struct pci_device_id sm501_pci_tbl[] = {
	{ 0x126f, 0x501, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, },
};

MODULE_DEVICE_TABLE(pci, sm501_pci_tbl);

/*For PCI bus*/
static struct pci_driver sm501_pci_drv = {
	.name		= "sm501",
	.id_table	= sm501_pci_tbl,
	.probe		= sm501_pci_probe,
	.remove		= sm501_pci_remove,
};

/*For Platform bus*/
static struct platform_driver sm501_plat_drv = {
	.driver		= {
		.name	= "sm501",
		.owner	= THIS_MODULE,
	},
	.probe		= sm501_plat_probe,
	.remove		= sm501_plat_remove,
};

static int __init sm501_base_init(void)
{
	platform_driver_register(&sm501_plat_drv);
/*	register_chrdev(SM501_CHAR_MAJOR, "sm501", &sm501_chr_fops); */
	return pci_register_driver(&sm501_pci_drv);
}

static void __exit sm501_base_exit(void)
{
	platform_driver_unregister(&sm501_plat_drv);
/*	unregister_chrdev(SM501_CHAR_MAJOR, "sm501"); */
	pci_unregister_driver(&sm501_pci_drv);
}

module_init(sm501_base_init);
module_exit(sm501_base_exit);

MODULE_DESCRIPTION("SM50x Core Driver");
MODULE_AUTHOR("Ben Dooks <ben@simtec.co.uk>, Vincent Sanders, Boyod Yang");
MODULE_LICENSE("GPL v2");

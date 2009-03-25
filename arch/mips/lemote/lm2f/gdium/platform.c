#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pwm_backlight.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>

#include <asm/mc146818-time.h>

#include <stls2f.h>

#define M41T83_REG_SQW	0x13

#define GDIUM_GPIO_BASE 224
#define GDIUM_V2_GPIO_BASE 192

const char *get_system_type(void)
{
	return "Emtec Gdium Liberty 1000";
}

unsigned long read_persistent_clock(void)
{
	return mc146818_get_cmos_time();
}

static struct i2c_board_info __initdata sm502dev_i2c_devices[] = {
	{
		I2C_BOARD_INFO("lm75", 0x48),
		.type = "lm75",
	},
	{
		I2C_BOARD_INFO("rtc-m41t80", 0x68),
		.type = "m41t83",
	},
	{
		I2C_BOARD_INFO("gdium-laptop", 0x40),
	},
};

static int sm502dev_backlight_init(struct device *dev)
{
	/* Add gpio request stuff here */
	return 0;
}

static void sm502dev_backlight_exit(struct device *dev)
{
	/* Add gpio free stuff here */
}

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 100,
	.dft_brightness	= 50,
	.pwm_period_ns	= 50000, /* 20 kHz */
	.init		= sm502dev_backlight_init,
	.exit		= sm502dev_backlight_exit,
};

static struct platform_device backlight = {
	.name = "pwm-backlight",
	.dev  = {
		.platform_data = &backlight_data,
	},
	.id   = -1,
};

/*
 * Warning this stunt is very dangerous
 * as the sm501 gpio have dynamic numbers...
 */
/* bus 0 is the one for the ST7, DS75 etc... */
static struct i2c_gpio_platform_data i2c_gpio0_data = {
	.sda_pin	= GDIUM_GPIO_BASE + 13,
	.scl_pin	= GDIUM_GPIO_BASE + 6,
	.udelay		= 5,
	.timeout	= HZ / 10,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
};

static struct platform_device i2c_gpio0_device = {
		.name		   = "i2c-gpio",
		.id			 = 0,
		.dev			= {
				.platform_data  = &i2c_gpio0_data,
		},
};

/* bus 1 is for the CRT/VGA external screen */
static struct i2c_gpio_platform_data i2c_gpio1_data = {
	.sda_pin	= GDIUM_GPIO_BASE + 10,
	.scl_pin	= GDIUM_GPIO_BASE + 9,
	.udelay		= 5,
	.timeout	= HZ / 10,
	.sda_is_open_drain = 0,
	.scl_is_open_drain = 0,
};

static struct platform_device i2c_gpio1_device = {
		.name		   = "i2c-gpio",
		.id			 = 1,
		.dev			= {
				.platform_data  = &i2c_gpio1_data,
		},
};

static struct platform_device gdium_audio = {
	.name		= "gdium-audio",
	.id		= -1,
};

static struct platform_device *devices[] __initdata = {
	&i2c_gpio0_device,
	&i2c_gpio1_device,
	&backlight,
	&gdium_audio,
};

static int __init sm502dev_platform_devices_setup(void)
{
	int ret;
	printk("Registering platform devices\n");
	
	platform_add_devices(devices, ARRAY_SIZE(devices));
	
	ret = i2c_register_board_info(0, sm502dev_i2c_devices,
		ARRAY_SIZE(sm502dev_i2c_devices));

	if (ret != 0) {
		printk("Error while registering platform devices: %d\n", ret);
		return ret;
	}

	return 0;
}

/*
 * some devices are on the pwm stuff which is behind the mfd which is
 * behind the pci bus so arch_initcall can't work because too early
 */
late_initcall(sm502dev_platform_devices_setup);


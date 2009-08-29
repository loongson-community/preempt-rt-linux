/*
 *  Driver for YeeLoong laptop extras
 *
 *  Copyright (C) 2009 Lemote Inc.
 *  Author: Wu Zhangjin <wuzj@lemote.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>

#include "ec.h"
#include "ec_misc_fn.h"

#define MAX_BRIGHTNESS 8

static int yeeloong_set_brightness(struct backlight_device *bd)
{
	unsigned int level;

	level = (bd->props.fb_blank == FB_BLANK_UNBLANK &&
		 bd->props.power == FB_BLANK_UNBLANK) ?
				bd->props.brightness : 0;

	if (level > MAX_BRIGHTNESS)
		level = MAX_BRIGHTNESS;
	else if (level < 0)
		level = 0;

	ec_write(REG_DISPLAY_BRIGHTNESS, level);

	return 0;
}

static int yeeloong_get_brightness(struct backlight_device *bd)
{
	return (int)ec_read(REG_DISPLAY_BRIGHTNESS);
}

static struct backlight_ops yeeloong_ops = {
	.get_brightness = yeeloong_get_brightness,
	.update_status  = yeeloong_set_brightness,
};

static struct backlight_device *yeeloong_backlight_device;

static struct input_dev *yeeloong_sci_dev;

/* This should be called in the SCI interrupt handler and the LID open action
 * wakeup function in pm.c
 */
void yeeloong_lid_update_status(int status)
{
	input_report_switch(yeeloong_sci_dev, SW_LID, !status);
	input_sync(yeeloong_sci_dev);
}
EXPORT_SYMBOL(yeeloong_lid_update_status);

void yeeloong_sci_update_status(int event, int status)
{
	switch (event) {
	case SCI_EVENT_NUM_LID:
		yeeloong_lid_update_status(status);
		break;
	case SCI_EVENT_NUM_SLEEP:
		input_report_key(yeeloong_sci_dev, KEY_SLEEP, 1);
		input_sync(yeeloong_sci_dev);
		input_report_key(yeeloong_sci_dev, KEY_SLEEP, 0);
		input_sync(yeeloong_sci_dev);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(yeeloong_sci_update_status);

static int __init yeeloong_sci_setup(void)
{
	int ret;

	yeeloong_sci_dev = input_allocate_device();

	if (!yeeloong_sci_dev)
		return -ENOMEM;

	yeeloong_sci_dev->name = "YeeLoong HotKeys(Fn+Fx/left/right/up/down)";
	yeeloong_sci_dev->phys = "button/input0";
	yeeloong_sci_dev->id.bustype = BUS_HOST;
	yeeloong_sci_dev->dev.parent = NULL;

	/* lid switch */
	set_bit(EV_SW, yeeloong_sci_dev->evbit);
	set_bit(SW_LID, yeeloong_sci_dev->swbit);

	/* sleep/suspend: STD */
	set_bit(EV_KEY, yeeloong_sci_dev->evbit);
	set_bit(KEY_SLEEP, yeeloong_sci_dev->keybit);

	ret = input_register_device(yeeloong_sci_dev);
	if (ret) {
		input_free_device(yeeloong_sci_dev);
		return ret;
	}

	return 0;
}

/*
 * Hwmon
 */

/* fan speed divider */
#define	FAN_SPEED_DIVIDER		480000	/* (60*1000*1000/62.5/2)*/

/* pwm(auto/manual) enable or not */
static int yeeloong_get_fan_pwm_enable(void)
{
	int value = 0;

	/* This get the fan control method: auto or manual */
	value = ec_read(0xf459);

	return value;
}

static void yeeloong_set_fan_pwm_enable(int manual)
{
	if (manual)
		ec_write(0xf459, 1);
	else
		ec_write(0xf459, 0);
}

static int yeeloong_get_fan_pwm(void)
{
	/* fan speed level */
	return ec_read(0xf4cc);
}

static void yeeloong_set_fan_pwm(int value)
{
	int status;

	/* need to ensure the level?? */
	printk(KERN_INFO "fan pwm, value = %d\n", value);

	value = SENSORS_LIMIT(value, 0, 3);

	/* if value is not ZERO, we should ensure it is on */
	if (value != 0) {
		status = ec_read(0xf4da);
		if (status == 0)
			ec_write(0xf4d2, 1);
	}
	/* 0xf4cc is for writing */
	ec_write(0xf4cc, value);
}

static int yeeloong_get_fan_rpm(void)
{
	int value = 0;

	value = FAN_SPEED_DIVIDER /
		    (((ec_read(REG_FAN_SPEED_HIGH) & 0x0f) << 8) |
		     ec_read(REG_FAN_SPEED_LOW));

	return value;
}

/* Thermal subdriver
 */

static int yeeloong_get_cpu_temp(void)
{
	int value;

	value = ec_read(REG_TEMPERATURE_VALUE);

	if (value & (1 << 7))
		value = (value & 0x7f) - 128;
	else
		value = value & 0xff;

	return value * 1000;
}

static int parse_arg(const char *buf, unsigned long count, int *val)
{
	if (!count)
		return 0;
	if (sscanf(buf, "%i", val) != 1)
		return -EINVAL;
	return count;
}

static ssize_t store_sys_hwmon(void (*set)(int), const char *buf, size_t count)
{
	int rv, value;

	rv = parse_arg(buf, count, &value);
	if (rv > 0)
		set(value);
	return rv;
}

static ssize_t show_sys_hwmon(int (*get)(void), char *buf)
{
	return sprintf(buf, "%d\n", get());
}


#define CREATE_SENSOR_ATTR(_name, _mode, _set, _get)		\
	static ssize_t show_##_name(struct device *dev,			\
				    struct device_attribute *attr,	\
				    char *buf)				\
	{								\
		return show_sys_hwmon(_set, buf);			\
	}								\
	static ssize_t store_##_name(struct device *dev,		\
				     struct device_attribute *attr,	\
				     const char *buf, size_t count)	\
	{								\
		return store_sys_hwmon(_get, buf, count);		\
	}								\
	static SENSOR_DEVICE_ATTR(_name, _mode, show_##_name, store_##_name, 0);

CREATE_SENSOR_ATTR(fan1_input, S_IRUGO, yeeloong_get_fan_rpm, NULL);
CREATE_SENSOR_ATTR(pwm1, S_IRUGO | S_IWUSR,
			 yeeloong_get_fan_pwm, yeeloong_set_fan_pwm);
CREATE_SENSOR_ATTR(pwm1_enable, S_IRUGO | S_IWUSR,
			 yeeloong_get_fan_pwm_enable, yeeloong_set_fan_pwm_enable);
CREATE_SENSOR_ATTR(temp1_input, S_IRUGO,
			 yeeloong_get_cpu_temp, NULL);

static ssize_t
show_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "yeeloong_laptop\n");
}
static SENSOR_DEVICE_ATTR(name, S_IRUGO, show_name, NULL, 0);

static struct attribute *hwmon_attributes[] = {
	&sensor_dev_attr_pwm1.dev_attr.attr,
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_pwm1_enable.dev_attr.attr,
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_name.dev_attr.attr,
	NULL
};

static struct attribute_group hwmon_attribute_group = {
	.attrs = hwmon_attributes
};

struct device *yeeloong_sensors_device;
static struct platform_device *yeeloong_sensors_pdev;

static ssize_t yeeloong_sensors_pdev_name_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "yeeloong_sensors\n");
}

static struct device_attribute dev_attr_yeeloong_sensors_pdev_name =
	__ATTR(name, S_IRUGO, yeeloong_sensors_pdev_name_show, NULL);


static int __init yeeloong_init(void)
{
	int ret;

	yeeloong_backlight_device = backlight_device_register(
		"yeeloong_backlight",
		NULL, NULL,
		&yeeloong_ops);

	if (IS_ERR(yeeloong_backlight_device)) {
		ret = PTR_ERR(yeeloong_backlight_device);
		yeeloong_backlight_device = NULL;
		return ret;
	}

	yeeloong_backlight_device->props.max_brightness = MAX_BRIGHTNESS;
	yeeloong_backlight_device->props.brightness =
		yeeloong_get_brightness(yeeloong_backlight_device);
	backlight_update_status(yeeloong_backlight_device);

	ret = yeeloong_sci_setup();
	if (ret)
		return ret;

	/* update the current state of lid */
	yeeloong_lid_update_status(BIT_LID_DETECT_ON);

	/* sensors */
	yeeloong_sensors_pdev = platform_device_register_simple(
			"yeeloong_sensors", -1, NULL, 0);

	if (IS_ERR(yeeloong_sensors_pdev)) {
		ret = PTR_ERR(yeeloong_sensors_pdev);
		yeeloong_sensors_pdev = NULL;
		printk(KERN_INFO "unable to register hwmon platform device\n");
		return ret;
	}
	ret = device_create_file(&yeeloong_sensors_pdev->dev,
				 &dev_attr_yeeloong_sensors_pdev_name);
	if (ret) {
		printk(KERN_INFO "unable to create sysfs hwmon device attributes\n");
		return ret;
	}

	yeeloong_sensors_device = hwmon_device_register(&yeeloong_sensors_pdev->dev);
	if (IS_ERR(yeeloong_sensors_device)) {
		printk(KERN_INFO "Could not register yeeloong hwmon device\n");
		yeeloong_sensors_device = NULL;
		return PTR_ERR(yeeloong_sensors_device);
	}
	ret = sysfs_create_group(&yeeloong_sensors_device->kobj,
				    &hwmon_attribute_group);
	if (ret) {
		sysfs_remove_group(&yeeloong_sensors_device->kobj,
			   &hwmon_attribute_group);
		hwmon_device_unregister(yeeloong_sensors_device);
		yeeloong_sensors_device = NULL;
	}

	return 0;
}

static void __exit yeeloong_exit(void)
{
	if (yeeloong_backlight_device)
		backlight_device_unregister(yeeloong_backlight_device);
	yeeloong_backlight_device = NULL;

	if (yeeloong_sci_dev)
		input_unregister_device(yeeloong_sci_dev);
	yeeloong_sci_dev = NULL;

	if (yeeloong_sensors_device) {
		sysfs_remove_group(&yeeloong_sensors_device->kobj,
				&hwmon_attribute_group);
		hwmon_device_unregister(yeeloong_sensors_device);
	}
	yeeloong_sensors_device = NULL;
	if (yeeloong_sensors_pdev)
		platform_device_unregister(yeeloong_sensors_pdev);
	yeeloong_sensors_pdev = NULL;

}

module_init(yeeloong_init);
module_exit(yeeloong_exit);

MODULE_AUTHOR("Wu Zhangjin <wuzj@lemote.com>");
MODULE_DESCRIPTION("YeeLoong laptop driver");
MODULE_LICENSE("GPL");

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
#include <linux/thermal.h>
#include <linux/video_output.h>
#include <linux/pm.h>

#include <loongson.h>

#include "ec.h"
#include "ec_misc_fn.h"

#define MAX_BRIGHTNESS 8

static int hotkey_status = -1;

static int yeeloong_set_brightness(struct backlight_device *bd)
{
	unsigned int level, current_level;
	static unsigned int old_level;

	level = (bd->props.fb_blank == FB_BLANK_UNBLANK &&
		 bd->props.power == FB_BLANK_UNBLANK) ?
				bd->props.brightness : 0;

	if (level > MAX_BRIGHTNESS)
		level = MAX_BRIGHTNESS;
	else if (level < 0)
		level = 0;

	/* avoid tune the brightness when the EC is tuning it */
	current_level = ec_read(REG_DISPLAY_BRIGHTNESS);
	if ((old_level == current_level) && (old_level != level))
		ec_write(REG_DISPLAY_BRIGHTNESS, level);
	old_level = level;

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

static struct input_dev *yeeloong_input_device;

struct key_entry {
	char type;		/* See KE_* below */
	int event;		/* event from SCI */
	u16 keycode;		/* KEY_* or SW_* */
};

enum { KE_KEY, KE_SW, KE_END };

static struct key_entry yeeloong_keymap[] = {
	{KE_SW, SCI_EVENT_NUM_LID, SW_LID},
	/* CRT_DETECT should be SW_VIDEOOUT_INSERT, not included in hald-addon-input! */
	{KE_KEY, SCI_EVENT_NUM_CRT_DETECT, KEY_PROG1},
	/* no specific KEY_ found for overtemp! this should be reported in the batter subdriver */
	{KE_KEY, SCI_EVENT_NUM_OVERTEMP, KEY_PROG2},
	{KE_KEY, SCI_EVENT_NUM_AC_BAT, KEY_BATTERY},
	{KE_KEY, SCI_EVENT_NUM_CAMERA, KEY_CAMERA},  /* Fn + ESC */
	{KE_KEY, SCI_EVENT_NUM_SLEEP, KEY_SLEEP},    /* Fn + F1 */
	/* BLACK_SCREEN should be KEY_DISPLAYTOGGLE, but not included in hald-addon-input yet!! */
	{KE_KEY, SCI_EVENT_NUM_BLACK_SCREEN, KEY_PROG3}, /* Fn + F2 */
	{KE_KEY, SCI_EVENT_NUM_DISPLAY_TOGGLE, KEY_SWITCHVIDEOMODE}, /* Fn + F3 */
	{KE_KEY, SCI_EVENT_NUM_AUDIO_MUTE, KEY_MUTE}, /* Fn + F4 */
	{KE_KEY, SCI_EVENT_NUM_WLAN, KEY_WLAN}, /* Fn + F5 */
	{KE_KEY, SCI_EVENT_NUM_DISPLAY_BRIGHTNESS, KEY_BRIGHTNESSUP}, /* Fn + up */
	{KE_KEY, SCI_EVENT_NUM_DISPLAY_BRIGHTNESS, KEY_BRIGHTNESSDOWN}, /* Fn + down */
	{KE_KEY, SCI_EVENT_NUM_AUDIO_VOLUME, KEY_VOLUMEUP},	/* Fn + right */
	{KE_KEY, SCI_EVENT_NUM_AUDIO_VOLUME, KEY_VOLUMEDOWN},	/* Fn + left */
	{KE_END, 0}
};

/* This should be called in the SCI interrupt handler and the LID open action
 * wakeup function in pm.c
 */
void yeeloong_lid_update_status(int status)
{
	printk(KERN_INFO "sw: %d\n", SW_LID);

	input_report_switch(yeeloong_input_device, SW_LID, !status);
	input_sync(yeeloong_input_device);
}
EXPORT_SYMBOL(yeeloong_lid_update_status);

static void yeeloong_hotkey_update_status(int key)
{
	printk(KERN_INFO "key: %d\n", key);

	input_report_key(yeeloong_input_device, key, 1);
	input_sync(yeeloong_input_device);
	input_report_key(yeeloong_input_device, key, 0);
	input_sync(yeeloong_input_device);
}

static void lcd_video_output_update_status(int state);
static void crt_video_output_update_status(int state);

static void camera_input_update_status(int state)
{
	int value;

	value = ec_read(REG_CAMERA_CONTROL);
	ec_write(REG_CAMERA_CONTROL, value | (1 << 1));
}

static void usb_ports_update_status(int state)
{
	int value;

	value = !!state;

	ec_write(0xf461, value);
	ec_write(0xf462, value);
	ec_write(0xf463, value);
}

void yeeloong_input_update_status(int event, int status)
{
	static int old_brightness_status = -1, old_volume_status = -1;
	static int video_output_state;
	struct key_entry *key;

	for (key = yeeloong_keymap; key->type != KE_END; key++) {
		if (key->event != event)
			continue;
		else {
			switch (event) {
			case SCI_EVENT_NUM_LID:
				yeeloong_lid_update_status(status);
				return;
			case SCI_EVENT_NUM_DISPLAY_BRIGHTNESS:
				/* current status is higher than the old one, means up */
				if ((status < old_brightness_status) || (status == 0))
					key++;
				old_brightness_status = status;
				break;
			case SCI_EVENT_NUM_AUDIO_VOLUME:
				if ((status < old_volume_status) || (status == 0))
					key++;
				old_volume_status = status;
				break;
			case SCI_EVENT_NUM_AC_BAT:
				/* only report Power Adapter when inserted */
				if (status != 1)
					return;
				break;
			case SCI_EVENT_NUM_CAMERA:
				camera_input_update_status(1);
				break;
			case SCI_EVENT_NUM_CRT_DETECT:
				if (ec_read(REG_CRT_DETECT)) {
					crt_video_output_update_status(1);
					lcd_video_output_update_status(0);
				} else {
					lcd_video_output_update_status(1);
					crt_video_output_update_status(0);
				}
				break;
			case SCI_EVENT_NUM_BLACK_SCREEN:
				lcd_video_output_update_status(status);
				break;
			case SCI_EVENT_NUM_DISPLAY_TOGGLE:
				/* only enable switch video output button
				 * when CRT is connected */
				if (!ec_read(REG_CRT_DETECT)) {
					status = 0;
					break;
				}
				/* 0. no CRT connected: LCD on, CRT off
				 * 1. BOTH on
				 * 2. LCD off, CRT on
				 * 3. BOTH off
				 * 4. LCD on, CRT off
				 */
				video_output_state++;
				if (video_output_state > 4)
					video_output_state = 1;

				switch (video_output_state) {
				case 1:
					lcd_video_output_update_status(1);
					crt_video_output_update_status(1);
					break;
				case 2:
					lcd_video_output_update_status(0);
					crt_video_output_update_status(1);
					break;
				case 3:
					lcd_video_output_update_status(0);
					crt_video_output_update_status(0);
					break;
				case 4:
					lcd_video_output_update_status(1);
					crt_video_output_update_status(0);
					break;
				default:
					/* ensure LCD is on */
					lcd_video_output_update_status(1);
					break;
				}
				status = video_output_state;
				break;
			default:
				break;
			}
			yeeloong_hotkey_update_status(key->keycode);
			break;
		}
	}
	/* update the global hotkey_status */
	hotkey_status = status;

	printk(KERN_INFO "event: %d, status: %d\n", event, status);
}
EXPORT_SYMBOL(yeeloong_input_update_status);

static int __init yeeloong_input_setup(struct device *dev)
{
	int ret;
	struct key_entry *key;

	yeeloong_input_device = input_allocate_device();

	if (!yeeloong_input_device)
		return -ENOMEM;

	yeeloong_input_device->name = "HotKeys";
	yeeloong_input_device->phys = "button/input0";
	yeeloong_input_device->id.bustype = BUS_HOST;
	yeeloong_input_device->dev.parent = dev;

	for (key = yeeloong_keymap; key->type != KE_END; key++) {
		switch (key->type) {
		case KE_KEY:
			set_bit(EV_KEY, yeeloong_input_device->evbit);
			set_bit(key->keycode, yeeloong_input_device->keybit);
			break;
		case KE_SW:
			set_bit(EV_SW, yeeloong_input_device->evbit);
			set_bit(key->keycode, yeeloong_input_device->swbit);
			break;
		}
	}

	ret = input_register_device(yeeloong_input_device);
	if (ret) {
		input_free_device(yeeloong_input_device);
		return ret;
	}

	return 0;
}

static ssize_t
ignore_store(struct device *dev,
	     struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t
show_hotkey_status(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hotkey_status);
}

static DEVICE_ATTR(hotkey_status, 0444, show_hotkey_status, ignore_store);

static struct attribute *hotkey_attributes[] = {
	&dev_attr_hotkey_status.attr,
	NULL
};

static struct attribute_group hotkey_attribute_group = {
	.attrs = hotkey_attributes
};

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
	return sprintf(buf, "yeeloong\n");
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

/* thermal cooling device callbacks */
static int video_get_max_state(struct thermal_cooling_device *cdev, unsigned
			       long *state)
{
	*state = MAX_BRIGHTNESS;
	return 0;
}

static int video_get_cur_state(struct thermal_cooling_device *cdev, unsigned
			       long *state)
{
	static struct backlight_device *bd;

	bd = (struct backlight_device *)cdev->devdata;

	*state = yeeloong_get_brightness(bd);

	printk(KERN_INFO "video get state: %ld\n", *state);

	return 0;
}

static int video_set_cur_state(struct thermal_cooling_device *cdev, unsigned
			       long state)
{
	static struct backlight_device *bd;

	bd = (struct backlight_device *)cdev->devdata;

	yeeloong_backlight_device->props.brightness = state;
	backlight_update_status(bd);

	printk(KERN_INFO "video set state: %ld\n", state);

	return 0;
}

static struct thermal_cooling_device_ops video_cooling_ops = {
	.get_max_state = video_get_max_state,
	.get_cur_state = video_get_cur_state,
	.set_cur_state = video_set_cur_state,
};

static struct thermal_cooling_device *yeeloong_thermal_cdev;

/*video output device sysfs support*/
static int lcd_video_output_get(struct output_device *od)
{
	return ec_read(REG_DISPLAY_LCD);
}

static int lcd_video_output_set(struct output_device *od)
{
	unsigned long state = od->request_state;
	int value;

	printk(KERN_INFO "lcd video output: %ld\n", state);

	if (state) {
		/* open LCD */
		outb(0x31, 0x3c4);
		value = inb(0x3c5);
		value = (value & 0xf8) | 0x03;
		outb(0x31, 0x3c4);
		outb(value, 0x3c5);
		/* open backlight */
		ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_ON);
	} else {
		/* close backlight */
		ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_OFF);
		/* close LCD */
		outb(0x31, 0x3c4);
		value = inb(0x3c5);
		value = (value & 0xf8) | 0x02;
		outb(0x31, 0x3c4);
		outb(value, 0x3c5);
	}

	return 0;
}

static struct output_properties lcd_output_properties = {
	.set_state = lcd_video_output_set,
	.get_status = lcd_video_output_get,
};

static int crt_video_output_get(struct output_device *od)
{
	return ec_read(REG_CRT_DETECT);
}

static int crt_video_output_set(struct output_device *od)
{
	unsigned long state = od->request_state;
	int value;

	if (state) {
		/* open CRT */
		outb(0x21, 0x3c4);
		value = inb(0x3c5);
		value &= ~(1 << 7);
		outb(0x21, 0x3c4);
		outb(value, 0x3c5);
	} else {
		/* close CRT */
		outb(0x21, 0x3c4);
		value = inb(0x3c5);
		value |= (1 << 7);
		outb(0x21, 0x3c4);
		outb(value, 0x3c5);
	}

	return 0;
}

static struct output_properties crt_output_properties = {
	.set_state = crt_video_output_set,
	.get_status = crt_video_output_get,
};

struct output_device *lcd_output_dev, *crt_output_dev;

static void lcd_video_output_update_status(int state)
{
	lcd_output_dev->request_state = state;
	lcd_video_output_set(lcd_output_dev);
}

static void crt_video_output_update_status(int state)
{
	crt_output_dev->request_state = state;
	crt_video_output_set(crt_output_dev);
}

#ifdef CONFIG_SUSPEND

static int cached_camera_status;

static int yeeloong_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk(KERN_INFO "yeeloong specific suspend\n");

	/* close LCD */
	lcd_video_output_update_status(0);
	/* close CRT */
	crt_video_output_update_status(0);
	/* power off camera */
	cached_camera_status = ec_read(REG_CAMERA_STATUS);
	if (cached_camera_status)
		camera_input_update_status(1);
	/* poweroff three usb ports */
	usb_ports_update_status(0);
	/* minimize the speed of FAN */
	yeeloong_set_fan_pwm_enable(1);
	yeeloong_set_fan_pwm(1);

	return 0;
}

static int yeeloong_resume(struct platform_device *pdev)
{
	printk(KERN_INFO "yeeloong specific resume\n");

	lcd_video_output_update_status(1);
	crt_video_output_update_status(1);

	/* power on three usb ports */
	usb_ports_update_status(1);

	if (cached_camera_status)
		camera_input_update_status(1);
	/* resume fan to auto mode */
	yeeloong_set_fan_pwm_enable(0);

	return 0;
}
#else
static int yeeloong_suspend(struct platform_device *pdev, pm_message_t state)
{
}
static int yeeloong_resume(struct platform_device *pdev)
{
}
#endif

static struct platform_driver platform_driver = {
	.driver = {
		.name = "yeeloong-laptop",
		.owner = THIS_MODULE,
	},
#ifdef CONFIG_PM
	.suspend = yeeloong_suspend,
	.resume = yeeloong_resume,
#endif
};

static struct platform_device *yeeloong_pdev;

static ssize_t yeeloong_pdev_name_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	return sprintf(buf, "yeeloong laptop\n");
}

static struct device_attribute dev_attr_yeeloong_pdev_name =
	__ATTR(name, S_IRUGO, yeeloong_pdev_name_show, NULL);


static int __init yeeloong_init(void)
{
	int ret;

	/* Register platform stuff */
	ret = platform_driver_register(&platform_driver);
	if (ret)
		return ret;
	yeeloong_pdev = platform_device_alloc("yeeloong-laptop", -1);
	if (!yeeloong_pdev) {
		ret = -ENOMEM;
		platform_driver_unregister(&platform_driver);
		return ret;
	}
	ret = platform_device_add(yeeloong_pdev);
	if (ret) {
		platform_device_put(yeeloong_pdev);
		return ret;
	}

	if (IS_ERR(yeeloong_pdev)) {
		ret = PTR_ERR(yeeloong_pdev);
		yeeloong_pdev = NULL;
		printk(KERN_INFO "unable to register hwmon platform device\n");
		return ret;
	}
	ret = device_create_file(&yeeloong_pdev->dev,
				 &dev_attr_yeeloong_pdev_name);
	if (ret) {
		printk(KERN_INFO "unable to create sysfs hwmon device attributes\n");
		return ret;
	}

	/* backlight */
	yeeloong_backlight_device = backlight_device_register(
		"backlight0",
		&yeeloong_pdev->dev, NULL,
		&yeeloong_ops);

	if (IS_ERR(yeeloong_backlight_device)) {
		ret = PTR_ERR(yeeloong_backlight_device);
		yeeloong_backlight_device = NULL;
		return ret;
	}

	yeeloong_backlight_device->props.max_brightness = MAX_BRIGHTNESS;
	yeeloong_backlight_device->props.brightness = MAX_BRIGHTNESS/2;
	backlight_update_status(yeeloong_backlight_device);

	yeeloong_thermal_cdev = thermal_cooling_device_register("LCD",
		yeeloong_backlight_device, &video_cooling_ops);
	if (IS_ERR(yeeloong_thermal_cdev)) {
		ret = PTR_ERR(yeeloong_thermal_cdev);
		return ret;
	}
	ret = sysfs_create_link(&yeeloong_backlight_device->dev.kobj,
			&yeeloong_thermal_cdev->device.kobj,
			"thermal_cooling");
	if (ret)
		printk(KERN_ERR "Create sysfs link\n");
	ret = sysfs_create_link(&yeeloong_thermal_cdev->device.kobj,
		&yeeloong_backlight_device->dev.kobj, "device");
	if (ret)
		printk(KERN_ERR "Create sysfs link\n");

	/* register video output device: lcd, crt */
	lcd_output_dev = video_output_register("LCD",
		&yeeloong_pdev->dev, NULL, &lcd_output_properties);
	/* ensure LCD is on by default */
	lcd_video_output_update_status(1);

	crt_output_dev = video_output_register("CRT",
		&yeeloong_pdev->dev, NULL, &crt_output_properties);
	/* close CRT by default, and will be enabled
	 * when the CRT connectting event reported by SCI */
	crt_video_output_update_status(0);

	/* hotkey */
	ret = yeeloong_input_setup(&yeeloong_pdev->dev);
	if (ret)
		return ret;
	ret = sysfs_create_group(&yeeloong_input_device->dev.kobj,
				    &hotkey_attribute_group);
	if (ret) {
		sysfs_remove_group(&yeeloong_input_device->dev.kobj,
			   &hotkey_attribute_group);
		input_free_device(yeeloong_input_device);
		yeeloong_input_device = NULL;
	}
	/* update the current status of lid */
	yeeloong_lid_update_status(BIT_LID_DETECT_ON);

	/* sensors */

	yeeloong_sensors_device = hwmon_device_register(&yeeloong_pdev->dev);
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
	/* ensure fan is set to auto mode */
	yeeloong_set_fan_pwm_enable(0);

	return 0;
}

static void __exit yeeloong_exit(void)
{
	if (yeeloong_backlight_device)
		backlight_device_unregister(yeeloong_backlight_device);
	yeeloong_backlight_device = NULL;
	if (yeeloong_thermal_cdev) {
		sysfs_remove_link(&yeeloong_backlight_device->dev.kobj,
				  "thermal_cooling");
		sysfs_remove_link(&yeeloong_thermal_cdev->device.kobj,
				  "device");
		thermal_cooling_device_unregister(yeeloong_thermal_cdev);
		yeeloong_thermal_cdev = NULL;
	}
	video_output_unregister(lcd_output_dev);
	video_output_unregister(crt_output_dev);

	if (yeeloong_input_device) {
		sysfs_remove_group(&yeeloong_input_device->dev.kobj,
			   &hotkey_attribute_group);
		input_free_device(yeeloong_input_device);
	}
	yeeloong_input_device = NULL;

	if (yeeloong_sensors_device) {
		sysfs_remove_group(&yeeloong_sensors_device->kobj,
				&hwmon_attribute_group);
		hwmon_device_unregister(yeeloong_sensors_device);
	}
	yeeloong_sensors_device = NULL;

	if (yeeloong_pdev)
		platform_device_unregister(yeeloong_pdev);
	yeeloong_pdev = NULL;
	platform_driver_unregister(&platform_driver);
}

module_init(yeeloong_init);
module_exit(yeeloong_exit);

MODULE_AUTHOR("Wu Zhangjin <wuzj@lemote.com>");
MODULE_DESCRIPTION("YeeLoong laptop driver");
MODULE_LICENSE("GPL");

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

static struct input_dev *yeeloong_lid_dev;

/* This should be called in the SCI interrupt handler and the LID open action
 * wakeup function in pm.c
 */
void yeeloong_lid_update_status(int status)
{
	input_report_switch(yeeloong_lid_dev, SW_LID, !status);
	input_sync(yeeloong_lid_dev);
}
EXPORT_SYMBOL(yeeloong_lid_update_status);

static int __init yeeloong_lid_setup(void)
{
	int ret;

	yeeloong_lid_dev = input_allocate_device();

	if (!yeeloong_lid_dev)
		return -ENOMEM;

	yeeloong_lid_dev->name = "Lid Switch";
	yeeloong_lid_dev->phys = "button/input0";
	yeeloong_lid_dev->id.bustype = BUS_HOST;
	yeeloong_lid_dev->dev.parent = NULL;

	yeeloong_lid_dev->evbit[0] = BIT_MASK(EV_SW);
	set_bit(SW_LID, yeeloong_lid_dev->swbit);

	ret = input_register_device(yeeloong_lid_dev);
	if (ret) {
		input_free_device(yeeloong_lid_dev);
		return ret;
	}

	return 0;
}

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

	ret = yeeloong_lid_setup();
	if (ret)
		return ret;

	/* update the current state of lid */
	yeeloong_lid_update_status(BIT_LID_DETECT_ON);

	return 0;
}

static void __exit yeeloong_exit(void)
{
	if (yeeloong_backlight_device)
		backlight_device_unregister(yeeloong_backlight_device);
	yeeloong_backlight_device = NULL;

	if (yeeloong_lid_dev)
		input_unregister_device(yeeloong_lid_dev);
	yeeloong_lid_dev = NULL;

}

module_init(yeeloong_init);
module_exit(yeeloong_exit);

MODULE_AUTHOR("Wu Zhangjin <wuzj@lemote.com>");
MODULE_DESCRIPTION("YeeLoong laptop driver");
MODULE_LICENSE("GPL");

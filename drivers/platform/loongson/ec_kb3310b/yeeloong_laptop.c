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

static int __init yeeloong_init(void)
{
	int max_brightness = MAX_BRIGHTNESS;
	int ret;

	yeeloong_backlight_device = backlight_device_register(
		"yeeloong_backlight",
		NULL, NULL,
		&yeeloong_ops);

	if (IS_ERR(yeeloong_backlight_device)) {
		ret = PTR_ERR(yeeloong_backlight_device);
		yeeloong_backlight_device = NULL;
		goto out;
	}

	yeeloong_backlight_device->props.max_brightness = max_brightness;
	yeeloong_backlight_device->props.brightness =
	yeeloong_get_brightness(yeeloong_backlight_device);
	backlight_update_status(yeeloong_backlight_device);

	return 0;
out:
	return ret;
}

static void __exit yeeloong_exit(void)
{
	backlight_device_unregister(yeeloong_backlight_device);
}

module_init(yeeloong_init);
module_exit(yeeloong_exit);

MODULE_AUTHOR("Wu Zhangjin <wuzj@lemote.com>");
MODULE_DESCRIPTION("YeeLoong laptop driver");
MODULE_LICENSE("GPL");

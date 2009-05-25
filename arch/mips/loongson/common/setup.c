/*
 * board dependent setup routines
 *
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 * Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/module.h>

#include <asm/wbflush.h>

#include <loongson.h>

void (*__wbflush) (void);
EXPORT_SYMBOL(__wbflush);

static void loongson_wbflush(void)
{
	asm(".set\tpush\n\t"
	    ".set\tnoreorder\n\t"
	    ".set mips3\n\t"
	    "sync\n\t"
	    "nop\n\t"
	    ".set\tpop\n\t"
	    ".set mips0\n\t");
}

void __init loongson_wbflush_setup(void)
{
	__wbflush = loongson_wbflush;
}

#if defined(CONFIG_VT) && defined(CONFIG_VGA_CONSOLE)
#include <linux/screen_info.h>

void __init loongson_screeninfo_setup(void)
{
	screen_info = (struct screen_info) {
		    0,		/* orig-x */
		    25,		/* orig-y */
		    0,		/* unused */
		    0,		/* orig-video-page */
		    0,		/* orig-video-mode */
		    80,		/* orig-video-cols */
		    0,		/* ega_ax */
		    0,		/* ega_bx */
		    0,		/* ega_cx */
		    25,		/* orig-video-lines */
		    VIDEO_TYPE_VGAC,	/* orig-video-isVGA */
		    16		/* orig-video-points */
	};
}
#else
void __init loongson_screeninfo_setup(void)
{
}
#endif

void __init plat_mem_setup(void)
{
	loongson_reboot_setup();

	loongson_wbflush_setup();

	loongson_screeninfo_setup();
}

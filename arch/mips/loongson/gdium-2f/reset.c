/*
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Copyright (C) 2007 Lemote, Inc. & Institute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 */
#include <linux/pm.h>
#include <linux/delay.h>

#include <asm/reboot.h>

#include <loongson.h>

static void reset_cpu(void)
{
	/*
	 * reset cpu to full speed, this is needed when enabling cpu frequency
	 * scalling
	 */
	LOONGSON_CHIPCFG0 |= 0x7;
}

/* reset support for Gdium */

void mach_prepare_reboot(void)
{
	reset_cpu();

	LOONGSON_GENCFG &= ~(1 << 2);
	LOONGSON_GENCFG |= (1 << 2);

	LOONGSON_GPIOIE &= ~(1 << 2);
	LOONGSON_GPIODATA |= (1 << 2);
}

void mach_prepare_shutdown(void)
{
	LOONGSON_GPIOIE &= ~(1 << 1);
	LOONGSON_GPIODATA |= (1 << 1);
}

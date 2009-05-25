/* Board-specific reboot/shutdown routines
 * Copyright (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 *
 * Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 * Author: Wu Zhangjin, wuzj@lemote.com
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/types.h>

#include <loongson.h>
#include <machine.h>

void mach_prepare_reboot(void)
{
	/*
	 * reset cpu to full speed, this is needed when enabling cpu frequency
	 * scalling
	 */
	LOONGSON_CHIPCFG0 |= 0x7;

	/* sending an reset signal to EC(embedded controller) */
	writeb(REG_RESET_HIGH, (u8 *) (mips_io_port_base + EC_IO_PORT_HIGH));
	writeb(REG_RESET_LOW, (u8 *) (mips_io_port_base + EC_IO_PORT_LOW));
	mmiowb();
	writeb(BIT_RESET_ON, (u8 *) (mips_io_port_base + EC_IO_PORT_DATA));
	mmiowb();
}

void mach_prepare_shutdown(void)
{
	/* cpu-gpio0 output low */
	LOONGSON_GPIODATA &= ~0x00000001;
	/* cpu-gpio0 as output */
	LOONGSON_GPIOIE &= ~0x00000001;
}

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Taken from arch/mips/kernel/8250-platform.c
 *
 * Copyright (C) 2007 Ralf Baechle (ralf@linux-mips.org)
 * "Modified" for Lemote Fulong boxes by Arnaud Patard <apatard@mandriva.com>
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/serial_8250.h>

#define	SERIAL_BASE		(CKSEG1ADDR(0x1ff00000) + 0x3f8)
#define SERIAL_IRQ		3

#define PORT(base, int, base_baud)					\
{									\
	.iobase		= base,						\
	.irq		= int,						\
	.uartclk	= base_baud,					\
	.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,		\
	.regshift	= 0,						\
	.iotype		= UPIO_PORT,					\
}

#define PORT_M(base, int, base_baud)					\
{									\
	.membase	= (void __iomem *)base,				\
	.irq		= int,						\
	.uartclk	= base_baud,					\
	.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST,		\
	.regshift	= 0,						\
	.iotype		= UPIO_MEM,					\
}

static struct plat_serial8250_port uart8250_data[] = {
	PORT_M(SERIAL_BASE, MIPS_CPU_IRQ_BASE + SERIAL_IRQ, (1843200 / 16*2)),
	PORT(0x3F8, 4, 1843200),
	PORT(0x2F8, 3, 1843200),
	{ },
};

static struct platform_device uart8250_device = {
	.name		= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev		= {
		.platform_data	= uart8250_data,
	},
};

int __init uart8250_init(void)
{
	return platform_device_register(&uart8250_device);
}

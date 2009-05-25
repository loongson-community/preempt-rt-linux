/*
 * board-specific header file
 *
 * Copyright (c) 2009 Wu Zhangjin <wuzj@lemote.com>
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef __MACHINE_H
#define __MACHINE_H

#define MACH_NAME			"lemote-fuloong-2e"

#define LOONGSON_UART_BASE		(LOONGSON_PCIIO_BASE + 0x3f8)

#define LOONGSON_NORTH_BRIDGE_IRQ	(MIPS_CPU_IRQ_BASE + 2)
#define LOONGSON_UART_IRQ		(MIPS_CPU_IRQ_BASE + 4)
#define LOONGSON_SOUTH_BRIDGE_IRQ 	(MIPS_CPU_IRQ_BASE + 5)
#define LOONGSON_TIMER_IRQ        	(MIPS_CPU_IRQ_BASE + 7)
#define LOONGSON_DMATIMEOUT_IRQ		(LOONGSON_IRQ_BASE + 10)


#endif				/* ! __MACHINE_H */

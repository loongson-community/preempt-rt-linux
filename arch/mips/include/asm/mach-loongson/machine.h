/*
 * board-specific header file
 *
 * Copyright (c) 2009 Wu Zhangjin <wuzj@lemote.com>
 *
 * Copyright (C) 2009       Zhang Le
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef __MACHINE_H
#define __MACHINE_H

#ifdef CONFIG_LEMOTE_FULOONG2E

#define MACHTYPE	MACH_LEMOTE_FL2E

#define LOONGSON_UART_BASE		(LOONGSON_PCIIO_BASE + 0x3f8)
#define	LOONGSON_UART_BAUD		1843200
#define	LOONGSON_UART_IOTYPE		UPIO_PORT

#define LOONGSON_NORTH_BRIDGE_IRQ	(MIPS_CPU_IRQ_BASE + 2)
#define LOONGSON_UART_IRQ		(MIPS_CPU_IRQ_BASE + 4)
#define LOONGSON_SOUTH_BRIDGE_IRQ 	(MIPS_CPU_IRQ_BASE + 5)
#define LOONGSON_TIMER_IRQ        	(MIPS_CPU_IRQ_BASE + 7)
#define LOONGSON_DMATIMEOUT_IRQ		(LOONGSON_IRQ_BASE + 10)

#elif defined(CONFIG_LEMOTE_FULOONG2F)

#ifdef CONFIG_LEMOTE_FL2FNAS

#define MACHTYPE	MACH_LEMOTE_FL2FNAS

#define LOONGSON_UART_BASE		(LOONGSON_LIO1_BASE + 0x3f8)
#define LOONGSON_UART_BAUD		3686400
#define LOONGSON_UART_IOTYPE		UPIO_MEM

#else /* !CONFIG_LEMOTE_FL2FNAS */

#define MACHTYPE	MACH_LEMOTE_FL2F

#define LOONGSON_UART_BASE		(LOONGSON_PCIIO_BASE + 0x2f8)
#define LOONGSON_UART_BAUD		1843200
#define LOONGSON_UART_IOTYPE		UPIO_PORT

#endif /* !CONFIG_LEMOTE_FL2FNAS */

#else /* CONFIG_CPU_YEELOONG2F */

#define MACHTYPE	MACH_LEMOTE_YL2F89

/* yeeloong use the CPU serial port of Loongson2F */
#define LOONGSON_UART_BASE		(LOONGSON_LIO1_BASE + 0x3f8)
#define	LOONGSON_UART_BAUD		3686400
#define LOONGSON_UART_IOTYPE		UPIO_MEM

#endif	/* !CONFIG_LEMOTE_FULOONG2E */

/* fuloong2f and yeeloong2f have the same IRQ control interface */
#if defined(CONFIG_LEMOTE_FULOONG2F) || defined(CONFIG_LEMOTE_YEELOONG2F)

#define LOONGSON_TIMER_IRQ	(MIPS_CPU_IRQ_BASE + 7)	/* cpu timer */
#define LOONGSON_NORTH_BRIDGE_IRQ	(MIPS_CPU_IRQ_BASE + 6)	/* bonito */
#define LOONGSON_UART_IRQ	(MIPS_CPU_IRQ_BASE + 3)	/* cpu serial port */
#define LOONGSON_SOUTH_BRIDGE_IRQ	(MIPS_CPU_IRQ_BASE + 2)	/* i8259 */

#define LOONGSON_INT_BIT_GPIO1		(1 << 1)
#define LOONGSON_INT_BIT_GPIO2		(1 << 2)
#define LOONGSON_INT_BIT_GPIO3		(1 << 3)
#define LOONGSON_INT_BIT_PCI_INTA	(1 << 4)
#define LOONGSON_INT_BIT_PCI_INTB	(1 << 5)
#define LOONGSON_INT_BIT_PCI_INTC	(1 << 6)
#define LOONGSON_INT_BIT_PCI_INTD	(1 << 7)
#define LOONGSON_INT_BIT_PCI_PERR	(1 << 8)
#define LOONGSON_INT_BIT_PCI_SERR	(1 << 9)
#define LOONGSON_INT_BIT_DDR		(1 << 10)
#define LOONGSON_INT_BIT_INT0		(1 << 11)
#define LOONGSON_INT_BIT_INT1		(1 << 12)
#define LOONGSON_INT_BIT_INT2		(1 << 13)
#define LOONGSON_INT_BIT_INT3		(1 << 14)

#endif

#endif				/* ! __MACHINE_H */

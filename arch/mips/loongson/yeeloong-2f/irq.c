/*
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/interrupt.h>

#include <loongson.h>
#include <machine.h>

inline int mach_i8259_irq(void)
{
	int irq, isr, imr;

	irq = -1;

	if ((LOONGSON_INTISR & LOONGSON_INTEN) & LOONGSON_INT_BIT_INT0) {
		imr = inb(0x21) | (inb(0xa1) << 8);
		isr = inb(0x20) | (inb(0xa0) << 8);
		isr &= ~0x4;	/* irq2 for cascade */
		isr &= ~imr;
		irq = ffs(isr) - 1;
	}

	return irq;
}

inline void mach_irq_dispatch(unsigned int pending)
{
	if (pending & CAUSEF_IP7)
		do_IRQ(LOONGSON_TIMER_IRQ);
	else if (pending & CAUSEF_IP6) {
		do_IRQ(LOONGSON_PERFCNT_IRQ);	/* Perf counter overflow */
		bonito_irqdispatch();		/* North Bridge */
	} else if (pending & CAUSEF_IP3)	/* CPU UART */
		do_IRQ(LOONGSON_UART_IRQ);
	else if (pending & CAUSEF_IP2)	/* South Bridge */
		i8259_irqdispatch();
	else
		spurious_interrupt();
}

void __init set_irq_trigger_mode(void)
{
	/* setup cs5536 as high level trigger */
	LOONGSON_INTPOL = LOONGSON_INT_BIT_INT0 | LOONGSON_INT_BIT_INT1;
	LOONGSON_INTEDGE &= ~(LOONGSON_INT_BIT_INT0 | LOONGSON_INT_BIT_INT1);
}

/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 * Copyright (C) 2000, 2001 Ralf Baechle (ralf@gnu.org)
 *
 * Copyright (C) 2007 Lemote Inc. & Insititute of Computing Technology
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>

#include <asm/irq_cpu.h>
#include <asm/i8259.h>

#include <loongson.h>
#include <machine.h>

/*
 * the first level int-handler will jump here if it is a loongson irq
 */
void bonito_irqdispatch(void)
{
	u32 int_status;
	int i;

	/* workaround the IO dma problem: let cpu looping to allow DMA finish */
	int_status = LOONGSON_INTISR;
	while (int_status & (1 << 10)) {
		udelay(1);
		int_status = LOONGSON_INTISR;
	}

	/* Get pending sources, masked by current enables */
	int_status = LOONGSON_INTISR & LOONGSON_INTEN;

	if (int_status != 0) {
		i = __ffs(int_status);
		int_status &= ~(1 << i);
		do_IRQ(LOONGSON_IRQ_BASE + i);
	}
}

void i8259_irqdispatch(void)
{
	int irq;

	irq = mach_i8259_irq();
	if (irq < 0)
		spurious_interrupt();
	else
		do_IRQ(irq);
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_cause() & read_c0_status() & ST0_IM;

	mach_irq_dispatch(pending);
}


static struct irqaction cascade_irqaction = {
	.handler = no_action,
	.mask = CPU_MASK_NONE,
	.name = "cascade",
};

/*
 * fuloong2f and yeeloong2f share the cpu perf counter interrupt and the north
 * bridge interrupt in IP6, so, the ip6_irqaction should be sharable.
 * otherwise, we will can not request the perf counter irq(setup the perf
 * counter irq handler) in op_model_loongson2.c.
 */

#if defined(CONFIG_OPROFILE) && \
	(defined(CONFIG_LEMOTE_FULOONG2F) || defined(CONFIG_LEMOTE_YEELOONG2F))
irqreturn_t ip6_action(int cpl, void *dev_id)
{
	return IRQ_HANDLED;
}

static struct irqaction ip6_irqaction = {
	.handler = ip6_action,
	.mask = CPU_MASK_NONE,
	.name = "cascade",
	.flags = IRQF_SHARED,
};
#else
#define	ip6_irqaction	cascade_irqaction
#endif

void __init arch_init_irq(void)
{
	/*
	 * Clear all of the interrupts while we change the able around a bit.
	 * int-handler is not on bootstrap
	 */
	clear_c0_status(ST0_IM | ST0_BEV);
	local_irq_disable();

	/* setting irq trigger mode */
	set_irq_trigger_mode();

	/* no steer */
	LOONGSON_INTSTEER = 0;

	/*
	 * Mask out all interrupt by writing "1" to all bit position in
	 * the interrupt reset reg.
	 */
	LOONGSON_INTENCLR = ~0;

	/* init all controller
	 *   0-15         ------> i8259 interrupt
	 *   16-23        ------> mips cpu interrupt
	 *   32-63        ------> bonito irq
	 */

	/* Sets the first-level interrupt dispatcher. */
	mips_cpu_irq_init();
	init_i8259_irqs();
	bonito_irq_init();

	/* setup north bridge irq (bonito) */
	setup_irq(LOONGSON_NORTH_BRIDGE_IRQ, &ip6_irqaction);
	/* setup source bridge irq (i8259) */
	setup_irq(LOONGSON_SOUTH_BRIDGE_IRQ, &cascade_irqaction);
}

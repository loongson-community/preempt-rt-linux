/*
 * Copyright (C) 2007 Lemote Inc.
 * Author: Fuxin Zhang, zhangfx@lemote.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/interrupt.h>
#include <linux/module.h>

#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>

#include <loongson.h>
#include <machine.h>

void mach_irq_dispatch(unsigned int pending)
{
	unsigned int cause = read_c0_cause();

	if (pending & CAUSEF_IP7)
		do_IRQ(MIPS_CPU_IRQ_BASE + 7);
	else if (pending & CAUSEF_IP3)
		do_IRQ(MIPS_CPU_IRQ_BASE + 3);
	else if (pending & CAUSEF_IP2)
		do_IRQ(MIPS_CPU_IRQ_BASE + 2);
	else if (pending & CAUSEF_IP6)
		bonito_irqdispatch();
	else if (pending & CAUSEF_IP5)
		do_IRQ(MIPS_CPU_IRQ_BASE + 5);
	else if (pending & CAUSEF_IP4)
		do_IRQ(MIPS_CPU_IRQ_BASE + 4);
	else if (pending & CAUSEF_IP0)
		do_IRQ(MIPS_CPU_IRQ_BASE + 0);
	else if (pending & CAUSEF_IP1)
		do_IRQ(MIPS_CPU_IRQ_BASE + 1);
	else {
		if (cause) {
			if ((cause & CAUSEF_CE) &&
			    ((cause & CAUSEF_EXCCODE) >> CAUSEB_EXCCODE !=
			     11)) {
				/*
				 * CAUSEF_CE undefined for exceptions other than
				 * "coprocessor unusable
				 */
			} else
				spurious_interrupt();
		}
	}

}

void __init set_irq_trigger_mode(void)
{
}

struct irqaction cascade_irqaction = {
	.handler = no_action,
	.name = "cascade",
};

void __init mach_init_irq(void)
{
	/* Init all controller
	 *   16-23        ------> mips cpu interrupt
	 *   32-63        ------> bonito irq
	 */

	/* Sets the first-level interrupt dispatcher. */
	mips_cpu_irq_init();
	bonito_irq_init();

	/* bonito irq at IP6 */
	setup_irq(MIPS_CPU_IRQ_BASE + 6, &cascade_irqaction);
}

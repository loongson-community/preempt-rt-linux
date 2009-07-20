/*
 * loongson-specific STR/Standby
 *
 *  Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 *  Author: Wu Zhangjin <wuzj@lemote.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <linux/pm.h>

#include <asm/i8259.h>
#include <asm/delay.h>

#include <loongson.h>

#ifdef CONFIG_CS5536_MFGPT
#include <cs5536/cs5536_mfgpt.h>
#endif

/* debug functions */
extern void prom_printf(char *fmt, ...);

/* i8042 keyboard operations: drivers/input/serio/i8042.c */
extern int i8042_enable_kbd_port(void);
extern void i8042_flush(void);

static unsigned int cached_master_mask;	/* i8259A */
static unsigned int cached_slave_mask;
static unsigned int cached_bonito_irq_mask; /* bonito */

/* i8042 is connnectted to i8259A */
static void setup_wakeup_interrupt(void)
{
#define I8042_KBD_IRQ 1
	/* open the keyboard irq in i8259A */
	outb((0xff & ~(1 << I8042_KBD_IRQ)), PIC_MASTER_IMR);
	inb(PIC_MASTER_IMR);

	/* enable keyboard port */
	i8042_enable_kbd_port();
}

void arch_suspend_enable_irqs(void)
{
	/* enable all mips interrupts */
	local_irq_enable();

	/* only enable the cached interrupts of i8259A */
	outb(cached_slave_mask, PIC_SLAVE_IMR);
	outb(cached_master_mask, PIC_MASTER_IMR);

	/* enable all cached interrupts of bonito */
	LOONGSON_INTENCLR = cached_bonito_irq_mask;
	(void)LOONGSON_INTENCLR;
	mmiowb();
}

void arch_suspend_disable_irqs(void)
{
	/* disable all mips interrupts */
	local_irq_disable();

	/* disable all interrupts of i8259A */
	cached_slave_mask = inb(PIC_SLAVE_IMR);
	cached_master_mask = inb(PIC_MASTER_IMR);

	outb(0xff, PIC_SLAVE_IMR);
	inb(PIC_SLAVE_IMR);
	outb(0xff, PIC_MASTER_IMR);
	inb(PIC_MASTER_IMR);

	/* disable all interrupts of bonito */
	cached_bonito_irq_mask = LOONGSON_INTENCLR;
	LOONGSON_INTENCLR = 0xffff;
	(void)LOONGSON_INTENCLR;
	mmiowb();
}

static void loongson_cpu_idle(void)
{
	static unsigned int cached_cpu_freq;

	cached_cpu_freq = LOONGSON_CHIPCFG0;

	/* handle the old delayed kbd interrupt */
	LOONGSON_CHIPCFG0 &= ~0x7;	/* Put CPU into wait mode */
	i8042_flush();

	/* handle the real wakeup interrupt */
	LOONGSON_CHIPCFG0 &= ~0x7;
	mmiowb();

	LOONGSON_CHIPCFG0 = cached_cpu_freq;
	mmiowb();
}

static void arch_suspend(void)
{
#ifdef CONFIG_CS5536_MFGPT
	/* stop counting of cs5536 mfgpt timer */
	outw(inw(MFGPT0_SETUP) | (1 << 11) , MFGPT0_SETUP);
#endif
}

static void arch_resume(void)
{
#ifdef CONFIG_CS5536_MFGPT
	/* enable counting of cs5536 mfgpt timer */
	outw(inw(MFGPT0_SETUP) & ~(1 << 11) , MFGPT0_SETUP);
#endif
}

static int loongson_pm_enter(suspend_state_t state)
{
	prom_printf("suspend: try to call arch specific suspend\n");

	arch_suspend();

	prom_printf("suspend: try to setup the wakeup interrupt (keyboard interrupt)\n");

	setup_wakeup_interrupt();

	prom_printf("suspend: Enter into the wait mode of loongson cpu\n");

	loongson_cpu_idle();

	prom_printf("resume: waked up from wait mode of loongson cpu\n");

	arch_resume();

	prom_printf("resume: return from arch specific resume\n");

	return 0;
}

static int loongson_pm_valid_state(suspend_state_t state)
{
	switch (state) {
	case PM_SUSPEND_ON:
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		return 1;

	default:
		return 0;
	}
}

static struct platform_suspend_ops loongson_pm_ops = {
	.valid	= loongson_pm_valid_state,
	.enter	= loongson_pm_enter,
};

static int __init loongson_pm_init(void)
{
	suspend_set_ops(&loongson_pm_ops);

	return 0;
}
arch_initcall(loongson_pm_init);

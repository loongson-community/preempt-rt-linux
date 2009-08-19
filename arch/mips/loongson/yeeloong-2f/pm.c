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
#include <asm/mipsregs.h>

#include <loongson.h>

#ifdef CONFIG_CS5536_MFGPT
#include <cs5536/cs5536_mfgpt.h>
#endif

#include "../../../../drivers/platform/loongson/ec_kb3310b/ec.h"
#include "../../../../drivers/platform/loongson/ec_kb3310b/ec_misc_fn.h"

/* debug functions */
extern void prom_printf(char *fmt, ...);

/* i8042 keyboard operations: drivers/input/serio/i8042.c */
extern int i8042_enable_kbd_port(void);
extern void i8042_flush(void);

static unsigned int cached_master_mask;	/* i8259A */
static unsigned int cached_slave_mask;
static unsigned int cached_bonito_irq_mask; /* bonito */

void arch_suspend_enable_irqs(void)
{
	/* enable all mips interrupts */
	local_irq_enable();

	/* only enable the cached interrupts of i8259A */
	outb(cached_slave_mask, PIC_SLAVE_IMR);
	outb(cached_master_mask, PIC_MASTER_IMR);

	/* enable all cached interrupts of bonito */
	LOONGSON_INTENSET = cached_bonito_irq_mask;
	(void)LOONGSON_INTENSET;
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
	cached_bonito_irq_mask = LOONGSON_INTEN;
	LOONGSON_INTENCLR = 0xffff;
	(void)LOONGSON_INTENCLR;
	mmiowb();
}

#define I8042_KBD_IRQ 1
#define SCI_IRQ_NUM 0x0A /* system control interface */

/* i8042, sci are connnectted to i8259A */
static void setup_wakeup_interrupt(void)
{
	int irq_mask;

	/* open the keyboard irq in i8259A */
	outb((0xff & ~(1 << I8042_KBD_IRQ)), PIC_MASTER_IMR);
	irq_mask = inb(PIC_MASTER_IMR);
	/* enable keyboard port */
	i8042_enable_kbd_port();

	/* there is a need to wakeup the cpu via sci interrupt with relative
	 * lid openning event
	 */
	outb(irq_mask & ~(1 << (SCI_IRQ_NUM - 8)), PIC_MASTER_IMR);
	inb(PIC_MASTER_IMR);
	outb(0xff & ~(1 << (SCI_IRQ_NUM - 8)), PIC_SLAVE_IMR);
	inb(PIC_SLAVE_IMR);
}

extern int ec_query_seq(unsigned char cmd);
extern int sci_get_event_num(void);
extern void yeeloong_lid_update_status(int status);

static int wakeup_loongson(void)
{
	int irq;

	/* query the interrupt number */
	irq = mach_i8259_irq();
	if (irq < 0)
		return 0;
	prom_printf("irq = %d\n", irq);

	if (irq == I8042_KBD_IRQ)
		return 1;
	else if (irq == SCI_IRQ_NUM) {
		int ret, sci_event;
		/* query the event number */
		ret = ec_query_seq(CMD_GET_EVENT_NUM);
		if (ret < 0)
			return 0;
		sci_event = sci_get_event_num();
		prom_printf("sci event = %d\n", sci_event);
		if (sci_event < 0)
			return 0;
		if (sci_event == SCI_EVENT_NUM_LID) {
			int lid_status;
			/* check the LID status */
			lid_status = ec_read(REG_LID_DETECT);
			prom_printf("lid status = %d\n", lid_status);
			/* wakeup cpu when people open the LID */
			if (lid_status == BIT_LID_DETECT_ON) {
				/* send out this event */
				yeeloong_lid_update_status(lid_status);
				return 1;
			}
		}
	}
	return 0;
}

static void wait_for_wakeup_events(void)
{
wait:
	if (!wakeup_loongson()) {
		LOONGSON_CHIPCFG0 &= ~0x7;
		goto wait;
	}
}

/* stop all perf counters by default
 *   $24 is the control register of loongson perf counter
 */
static inline void stop_perf_counters(void)
{
	__write_64bit_c0_register($24, 0, 0);
}


static void loongson_suspend_enter(void)
{
	static unsigned int cached_cpu_freq;

	prom_printf("suspend: try to setup the wakeup interrupt (keyboard interrupt)\n");
	setup_wakeup_interrupt();

	/* stop all perf counters */
	stop_perf_counters();

	cached_cpu_freq = LOONGSON_CHIPCFG0;

	/* handle the old delayed kbd interrupt */
	LOONGSON_CHIPCFG0 &= ~0x7;	/* Put CPU into wait mode */
	i8042_flush();

	/* handle the real wakeup interrupt */
	LOONGSON_CHIPCFG0 &= ~0x7;
	mmiowb();

	/* if the events are really what we want to wakeup cpu, wake up it,
	 * otherwise, we Put CPU into wait mode again.
	 */
	prom_printf("suspend: here we wait for several events to wake up cpu\n");
	wait_for_wakeup_events();

	LOONGSON_CHIPCFG0 = cached_cpu_freq;
	mmiowb();
}

static unsigned int cached_camera_status;
static unsigned int cached_mute_status;

static void mach_suspend(void)
{
	unsigned int value;

	/* LCD backlight off */
	ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_OFF);

	/* close lcd output */
	outb(0x31, 0x3c4);
	value = inb(0x3c5);
	value = (value & 0xf8) | 0x02;
	outb(0x31, 0x3c4);
	outb(value, 0x3c5);

	/* close vga output */
	outb(0x21, 0x3c4);
	value = inb(0x3c5);
	value |= (1 << 7);
	outb(0x21, 0x3c4);
	outb(value, 0x3c5);

	/* poweroff three usb ports */
	ec_write(0xf461, 0x00);
	ec_write(0xf462, 0x00);
	ec_write(0xf463, 0x00);

	/* poweroff camera */
	cached_camera_status = ec_read(REG_CAMERA_STATUS);
	if (cached_camera_status) {
		value = ec_read(REG_CAMERA_CONTROL);
		ec_write(REG_CAMERA_CONTROL, value | (1 << 1));
	}

	/* MUTE */
	cached_mute_status = ec_read(REG_AUDIO_MUTE);
	ec_write(REG_AUDIO_MUTE, BIT_AUDIO_MUTE_ON);

	/* minimize the speed of FAN */
	ec_write(0xf459, 1);	/* change the fan to manual mode */
	ec_write(0xf4cc, 1);	/* change the speed to the lowest one, not turn it off */

#ifdef CONFIG_CS5536_MFGPT
	/* stop counting of cs5536 mfgpt timer */
	outw(inw(MFGPT0_SETUP) | (1 << 11) , MFGPT0_SETUP);
#endif
}

static void mach_resume(void)
{
	unsigned int value;

	/* LCD backlight on */
	ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_ON);

	/* open lcd output */
	outb(0x31, 0x3c4);
	value = inb(0x3c5);
	value = (value & 0xf8) | 0x03;
	outb(0x31, 0x3c4);
	outb(value, 0x3c5);

	/* open vga output */
	outb(0x21, 0x3c4);
	value = inb(0x3c5);
	value &= ~(1 << 7);
	outb(0x21, 0x3c4);
	outb(value, 0x3c5);

	/* there is a need to enable the bonito interrupt here to fix the
	 * problem of powering on usb ports */
	LOONGSON_INTENSET = cached_bonito_irq_mask;
	(void)LOONGSON_INTENSET;
	mmiowb();
	/* power on three usb ports */
	ec_write(0xf461, 0x01);
	ec_write(0xf462, 0x01);
	ec_write(0xf463, 0x01);

	/* resume camera */
	ec_write(REG_CAMERA_CONTROL, cached_camera_status);

	/* resume the status of mute */
	ec_write(REG_AUDIO_MUTE, cached_mute_status);

	/* resume FAN */
	ec_write(0xf459, 0);	/* change the fan to auto mode */

#ifdef CONFIG_CS5536_MFGPT
	/* enable counting of cs5536 mfgpt timer */
	outw(inw(MFGPT0_SETUP) & ~(1 << 11) , MFGPT0_SETUP);
#endif
}

static int loongson_pm_enter(suspend_state_t state)
{
	prom_printf("suspend: try to call mach specific suspend\n");
	mach_suspend();

	prom_printf("suspend: Enter into the wait mode of loongson cpu\n");
	loongson_suspend_enter();
	prom_printf("resume: waked up from wait mode of loongson cpu\n");

	mach_resume();
	prom_printf("resume: return from mach specific resume\n");

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

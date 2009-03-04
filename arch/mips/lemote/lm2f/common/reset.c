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
#include <asm/mips-boards/bonito64.h>
#include <asm/mips-boards/lm2e_via686.h>


static void loongson2e_restart(char *command)
{
	BONITO_BONGENCFG &= ~BONITO_BONGENCFG_CPUSELFRESET;
	BONITO_BONGENCFG |= BONITO_BONGENCFG_CPUSELFRESET;

#ifdef CONFIG_LOONGSON2F_DEV_SM502
	BONITO_GPIOIE &= ~(1<<2);
	BONITO_GPIODATA &= ~(1<<2);
#endif

#ifdef CONFIG_LOONGSON_FIX_RANDOM_INSTRUCTION_FETCH_SIDE_EFFECT_TO_DEVICE
	/* binutils patch will transform and break the jump to the BONITO_BOOT_BASE
	 * so let's do some evil. (Courtesy of Lemote guys)
	 */
	__asm__ __volatile__ (
			"\t.long 0x3c02bfc0\n"
			"\t.long 0x00400008\n"
			:::"v0");
#else
	__asm__ __volatile__("jr\t%0"::"r"(CKSEG1ADDR(BONITO_BOOT_BASE)));
#endif
}

static void loongson2e_halt(void)
{
#ifdef CONFIG_LOONGSON2F_DEV_SM502
#if 0
	int *p = (CKSEG1ADDR(BONITO_REG_BASE) + 0x11d);

	*p-- &= ~1;
	*p   &= ~1;
	*p   |= 1;
#endif
	BONITO_GPIOIE &= ~(1<<1);
	BONITO_GPIODATA |= (1<<1);
#else
	VIA686_PM_STAT = VIA686_WAK_STATUS | VIA686_PB_STATUS;
	msleep(1);
	VIA686_PM_CTRL = VIA686_SLEEP_TYPE(via686_sleep_std) | VIA686_SLEEP_EN;
	msleep(1);
#endif

	printk(KERN_EMERG "System Halted !!!\n");
	while (1);
}

static void loongson2e_power_off(void)
{
	loongson2e_halt();
}

void mips_reboot_setup(void)
{
	_machine_restart = loongson2e_restart;
	_machine_halt = loongson2e_halt;
	pm_power_off = loongson2e_power_off;
}

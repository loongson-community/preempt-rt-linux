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

static void loongson2f_restart(char *command)
{
	BONITO_BONGENCFG &= ~BONITO_BONGENCFG_CPUSELFRESET;
	BONITO_BONGENCFG |= BONITO_BONGENCFG_CPUSELFRESET;

	BONITO_GPIOIE &= ~(1<<2);
	BONITO_GPIODATA &= ~(1<<2);
	
	/* jump to system reset vector */
	((void (*)(void))(CKSEG1ADDR(BONITO_BOOT_BASE)))();
}

static void loongson2f_halt(void)
{
	BONITO_GPIOIE &= ~(1<<1);
	BONITO_GPIODATA |= (1<<1);

	printk(KERN_EMERG "System Halted !!!\n");
	while (1);
}

static void loongson2f_power_off(void)
{
	loongson2f_halt();
}

void mips_reboot_setup(void)
{
	_machine_restart = loongson2f_restart;
	_machine_halt = loongson2f_halt;
	pm_power_off = loongson2f_power_off;
}

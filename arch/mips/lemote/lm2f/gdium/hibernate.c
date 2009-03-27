/*
 * Suspend to Disk/Hibernation for Loongson 2F
 * Specifically designed for the Emtec Gdium Netbook
 *
 * (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * See COPYING in the root directory of this source distribution for more
 * information.
 */

#include <linux/init.h>
#include <linux/suspend.h>
#include <asm/reboot.h>

#include <stls2f.h>


static int ls2f_start(void)
{
	return 0;
}

static int ls2f_pre_snapshot(void)
{
	return 0;
}

static void ls2f_finish(void)
{

}

static int ls2f_prepare(void)
{
	return 0;
}

static int ls2f_enter(void)
{
	/* call machine shutdown */
    _machine_halt();
	return 0;
}

static void ls2f_leave(void)
{

}

static int ls2f_pre_restore(void)
{
	return 0;
}

static void ls2f_restore_cleanup(void)
{
}

static void ls2f_end(void)
{
}

static void ls2f_recover(void)
{
}

static struct platform_hibernation_ops ls2f_hibernation_ops = {
	.begin = ls2f_start,
	.end = ls2f_end,
	.pre_snapshot = ls2f_pre_snapshot,
	.finish = ls2f_finish,
	.prepare = ls2f_prepare,
	.enter = ls2f_enter,
	.leave = ls2f_leave,
	.pre_restore = ls2f_pre_restore,
	.restore_cleanup = ls2f_restore_cleanup,
	.recover = ls2f_recover
};

static int ls2f_pm_notifier(struct notifier_block *nb, unsigned long val,
	void *data)
{
	return 0;
}

static struct notifier_block ls2f_pm_notifier_block = {
	.notifier_call = ls2f_pm_notifier
};

static int __init ls2f_enable_hibernation(void)
{
	struct cpuinfo_mips *cpu = cpu_data;

	printk("swsusp: Suspend to RAM for Loongson 2F\n");
	printk("swsusp: Copyright 2009 Philippe Vachon <philippe@cowpig.ca>\n");

	/* check the CPU ID to ensure that we're not some non-Loongson CPU */
	if (cpu->processor_id != STLS2F_CPU_ID)
		return -ENODEV;

	hibernation_set_ops(&ls2f_hibernation_ops);

	return register_pm_notifier(&ls2f_pm_notifier_block);
}

late_initcall(ls2f_enable_hibernation);

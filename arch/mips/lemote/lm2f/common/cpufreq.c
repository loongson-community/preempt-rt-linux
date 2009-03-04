/*
 * CPU Frequency tuning for the Loongson 2F
 * Specifically designed for the Emtec Gdium Netbook
 *
 * (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 * Parts inspired by work done by Lemote on the Yeelong code.
 * Other parts inspired by the Centrino speedstep code.
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
#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/ioport.h>
#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

#include <asm/cpu-info.h>
#include <asm/delay.h>
#include <asm/clock.h>
#include <asm/time.h>
#include <asm/io.h>

#include <stls2f.h>

#define WRITE_CLK_CTL(val) do { ls2f_config_writel(LS2F_CORE_CONFIG_REG, \
	val); } while(0)
#define READ_CLK_CTL()	(ls2f_config_readl(LS2F_CORE_CONFIG_REG));

static int debug = 0;

#define dprintk(msg...) do { if (debug) { printk(msg); } } while(0)


/* CPU frequency table, percentage of actual system clock */
struct cpufreq_frequency_table loongson2f_freqs[] = {
	{0, CPUFREQ_ENTRY_INVALID},
	{25, 0},
	{37, 0},
	{50, 0},
	{62, 0},
	{75, 0},
	{87, 0},
	{100, 0},
	{0, CPUFREQ_TABLE_END}
};


static struct clk ls2f_cpu_clk = {
	.name	= "ls2f_cpu_clk",
	.flags	= CLK_ALWAYS_ENABLED | CLK_RATE_PROPAGATES,
	.rate 	= 0
};

/* Set the CPU frequency */
DEFINE_SPINLOCK(ls2f_freq_lock);
static void ls2f_set_frequency(struct clk *cpu_clk, int index)
{
	uint32_t flags, val;
	uint32_t old_hpt_frequency = mips_hpt_frequency;

	dprintk("cpufreq: set frequency %d - %u kHz\n", index,
		loongson2f_freqs[index].frequency);

	if (debug) val = READ_CLK_CTL();
	dprintk("cpufreq: old value: %08x\n", val);

	spin_lock_irqsave(&ls2f_freq_lock, flags);
	sched_clock_idle_sleep_event();

	val = READ_CLK_CTL();
	WRITE_CLK_CTL( (val & ~0x7u) | (uint32_t)(index) );

	notify_clockevent_cpufreq_changed(loongson2f_freqs[index].frequency * 
		1000 / 2);
	notify_clocksource_cpufreq_changed(loongson2f_freqs[index].frequency * 
		1000 / 2, old_hpt_frequency);

	sched_clock_idle_wakeup_event(0);
	spin_unlock_irqrestore(&ls2f_freq_lock, flags);

	if (debug) val = READ_CLK_CTL();
	dprintk("cpufreq: new value: %08x\n", val);
}

/* return the current CPU frequency, in Hz */
static uint32_t get_cur_freq(unsigned int cpu)
{
	struct clk *cpu_clk = clk_get(NULL, "ls2f_cpu_clk");
	if (cpu_clk == NULL) {
		dprintk("cpufreq: unable to retrieve clock!\n");
		return 0;
	}
	return cpu_clk->rate;
}

static int ls2f_cpu_freq_notifier(struct notifier_block *nb, unsigned long val,
	void *data)
{
	if (val == CPUFREQ_POSTCHANGE) {
		__udelay_val = loops_per_jiffy;
	}

	return 0;
}

struct notifier_block loongson2f_notifier_block = {
	.notifier_call = ls2f_cpu_freq_notifier
};

/*
 * Set a new CPUFreq policy.
 */
static int ls2f_target(struct cpufreq_policy *policy, uint32_t target_freq,
	uint32_t relation)
{
	int index;
	struct cpufreq_freqs change;
	struct clk *cpu_clk = clk_get(NULL, "ls2f_cpu_clk");

	if (cpu_clk == NULL) {
		dprintk("cpufreq: unable to get target clock.\n");
		return -ENODEV;
	}

	if (cpufreq_frequency_table_target(policy, loongson2f_freqs,
		target_freq, relation, &index))
		return -EINVAL;

	/* requested frequency is equal to the destination frequency */
	if (loongson2f_freqs[index].frequency == get_cur_freq(policy->cpu) / 1000) {
		return 0;
	}

	change.cpu = policy->cpu;
	change.old = get_cur_freq(policy->cpu);
	change.new = loongson2f_freqs[index].frequency;
	change.flags = 0;

	cpufreq_notify_transition(&change, CPUFREQ_PRECHANGE);
	ls2f_set_frequency(cpu_clk, index);
	cpufreq_notify_transition(&change, CPUFREQ_POSTCHANGE);

	cpu_clk->rate = loongson2f_freqs[index].frequency;

	return 0;
}

/*
 * Initialise the CPU frequency table per-cpu.
 */
static int ls2f_cpu_init(struct cpufreq_policy *policy)
{
	struct clk *cpu_clk = clk_get(NULL, "ls2f_cpu_clk");
	int i;

	if (cpu_clk == NULL) {
		dprintk("cpufreq: unable to find clock: ls2f_cpu_clk.\n");
		return -EINVAL;
	}

	dprintk("cpufreq: Initializing ls2f CPU frequency scaling.\n");

	if (!cpu_clk) 
		return -ENODEV;

	cpu_clk->rate = cpu_clock_freq / 1000;

	/* initialize the table of supported frequencies */
	for (i = 1; i <= 7; i++) {
		if (loongson2f_freqs[i].frequency == CPUFREQ_TABLE_END) 
			break;

		if (loongson2f_freqs[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		loongson2f_freqs[i].frequency = (cpu_clk->rate * 
			loongson2f_freqs[i].index) / 100;
	}

	policy->cur = cpu_clk->rate;

	cpufreq_frequency_table_get_attr(loongson2f_freqs, policy->cpu);

	/* set maximum and minimum supported frequency */
	return cpufreq_frequency_table_cpuinfo(policy, loongson2f_freqs);
}

static int ls2f_cpu_exit(struct cpufreq_policy *policy)
{
	return 0;
}

/*
 * Check that the provided limit is within the frequency range
 * supported by the Loongson 2F
 */
static int ls2f_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, loongson2f_freqs);
}



/* Module parameters and initialization for the Loongson 2F
 * CPU (STLS2F)
 */

static struct freq_attr *ls2f_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL
};

static struct cpufreq_driver ls2f_cpufreq_driver = {
	.owner	= THIS_MODULE,
	.name	= "ls2f-cpu-freq",
	.init	= ls2f_cpu_init,
	.verify	= ls2f_verify,
	.target = ls2f_target,
	.get	= get_cur_freq,
	.exit	= ls2f_cpu_exit,
	.attr	= ls2f_attr
};

static int __init ls2f_cpufreq_module_init(void)
{
	struct cpuinfo_mips *cpu = cpu_data;
	int ret;

	clk_register(&ls2f_cpu_clk);

	/* check if this is actually a Loongson 2F CPU */
	if (cpu->processor_id != STLS2F_CPU_ID)
		return -ENODEV;

	printk(KERN_INFO "cpufreq: Loongson 2F CPU Frequency scaling.\n");
	printk(KERN_INFO "cpufreq: Copyright (c)2009 Philippe Vachon "
		"<philippe@cowpig.ca>\n");
   
	ret = cpufreq_register_driver(&ls2f_cpufreq_driver);
	if (ret != 0) {
		printk("cpufreq: unable to initialize driver: %d\n", ret);
		return ret;
	}

	cpufreq_register_notifier(&loongson2f_notifier_block, 
		CPUFREQ_TRANSITION_NOTIFIER);

	return 0;
}

static void __exit ls2f_cpufreq_module_exit(void)
{
	/* clean up the mess we've made */
	cpufreq_unregister_driver(&ls2f_cpufreq_driver);
	cpufreq_unregister_notifier(&loongson2f_notifier_block,
		CPUFREQ_TRANSITION_NOTIFIER);
}

late_initcall(ls2f_cpufreq_module_init);
module_exit(ls2f_cpufreq_module_exit);

module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable Loongson 2F cpufreq debugging.");

MODULE_AUTHOR("Philippe Vachon <philippe@cowpig.ca>");
MODULE_DESCRIPTION("CPU Frequency driver for Loongson 2F processors.");
MODULE_LICENSE("GPL");

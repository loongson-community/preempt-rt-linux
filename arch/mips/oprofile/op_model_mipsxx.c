/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004, 05, 06 by Ralf Baechle
 * Copyright (C) 2005 by MIPS Technologies, Inc.
 */
#include <linux/cpumask.h>
#include <linux/oprofile.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <asm/irq_regs.h>
#include <asm/pmu.h>

#include "op_impl.h"


struct op_mips_model op_model_mipsxx_ops;

static struct mipsxx_register_config {
	unsigned int control[4];
	unsigned int counter[4];
} reg;

/* Compute all of the registers in preparation for enabling profiling.  */

static void mipsxx_reg_setup(struct op_counter_config *ctr)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;
	int i;

	/* Compute the performance counter control word.  */
	for (i = 0; i < counters; i++) {
		reg.control[i] = 0;
		reg.counter[i] = 0;

		if (!ctr[i].enabled)
			continue;

		reg.control[i] = M_PERFCTL_EVENT(ctr[i].event) |
		                 M_PERFCTL_INTERRUPT_ENABLE;
		if (ctr[i].kernel)
			reg.control[i] |= M_PERFCTL_KERNEL;
		if (ctr[i].user)
			reg.control[i] |= M_PERFCTL_USER;
		if (ctr[i].exl)
			reg.control[i] |= M_PERFCTL_EXL;
		reg.counter[i] = 0x80000000 - ctr[i].count;
	}
}

/* Program all of the registers in preparation for enabling profiling.  */

static void mipsxx_cpu_setup(void *args)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;

	switch (counters) {
	case 4:
		w_c0_perfctrl3(0);
		w_c0_perfcntr3(reg.counter[3]);
	case 3:
		w_c0_perfctrl2(0);
		w_c0_perfcntr2(reg.counter[2]);
	case 2:
		w_c0_perfctrl1(0);
		w_c0_perfcntr1(reg.counter[1]);
	case 1:
		w_c0_perfctrl0(0);
		w_c0_perfcntr0(reg.counter[0]);
	}
}

/* Start all counters on current CPU */
static void mipsxx_cpu_start(void *args)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;

	switch (counters) {
	case 4:
		w_c0_perfctrl3(WHAT | reg.control[3]);
	case 3:
		w_c0_perfctrl2(WHAT | reg.control[2]);
	case 2:
		w_c0_perfctrl1(WHAT | reg.control[1]);
	case 1:
		w_c0_perfctrl0(WHAT | reg.control[0]);
	}
}

/* Stop all counters on current CPU */
static void mipsxx_cpu_stop(void *args)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;

	switch (counters) {
	case 4:
		w_c0_perfctrl3(0);
	case 3:
		w_c0_perfctrl2(0);
	case 2:
		w_c0_perfctrl1(0);
	case 1:
		w_c0_perfctrl0(0);
	}
}

static int mipsxx_perfcount_handler(void)
{
	unsigned int counters = op_model_mipsxx_ops.num_counters;
	unsigned int control;
	unsigned int counter;
	int handled = IRQ_NONE;

	if (cpu_has_mips_r2 && !(read_c0_cause() & (1 << 26)))
		return handled;

	switch (counters) {
#define HANDLE_COUNTER(n)						\
	case n + 1:							\
		control = r_c0_perfctrl ## n();				\
		counter = r_c0_perfcntr ## n();				\
		if ((control & M_PERFCTL_INTERRUPT_ENABLE) &&		\
		    (counter & M_COUNTER_OVERFLOW)) {			\
			oprofile_add_sample(get_irq_regs(), n);		\
			w_c0_perfcntr ## n(reg.counter[n]);		\
			handled = IRQ_HANDLED;				\
		}
	HANDLE_COUNTER(3)
	HANDLE_COUNTER(2)
	HANDLE_COUNTER(1)
	HANDLE_COUNTER(0)
	}

	return handled;
}

static int __init mipsxx_init(void)
{
	int counters;

	counters = n_counters();
	if (counters == 0) {
		printk(KERN_ERR "Oprofile: CPU has no performance counters\n");
		return -ENODEV;
	}

#ifdef CONFIG_MIPS_MT_SMP
	cpu_has_mipsmt_pertccounters = read_c0_config7() & (1<<19);
	if (!cpu_has_mipsmt_pertccounters)
		counters = counters_total_to_per_cpu(counters);
#endif
	on_each_cpu(reset_counters, (void *)(long)counters, 1);

	op_model_mipsxx_ops.num_counters = counters;
	switch (current_cpu_type()) {
	case CPU_20KC:
		op_model_mipsxx_ops.cpu_type = "mips/20K";
		break;

	case CPU_24K:
		op_model_mipsxx_ops.cpu_type = "mips/24K";
		break;

	case CPU_25KF:
		op_model_mipsxx_ops.cpu_type = "mips/25K";
		break;

	case CPU_1004K:
#if 0
		/* FIXME: report as 34K for now */
		op_model_mipsxx_ops.cpu_type = "mips/1004K";
		break;
#endif

	case CPU_34K:
		op_model_mipsxx_ops.cpu_type = "mips/34K";
		break;

	case CPU_74K:
		op_model_mipsxx_ops.cpu_type = "mips/74K";
		break;

	case CPU_5KC:
		op_model_mipsxx_ops.cpu_type = "mips/5K";
		break;

	case CPU_R10000:
		if ((current_cpu_data.processor_id & 0xff) == 0x20)
			op_model_mipsxx_ops.cpu_type = "mips/r10000-v2.x";
		else
			op_model_mipsxx_ops.cpu_type = "mips/r10000";
		break;

	case CPU_R12000:
	case CPU_R14000:
		op_model_mipsxx_ops.cpu_type = "mips/r12000";
		break;

	case CPU_SB1:
	case CPU_SB1A:
		op_model_mipsxx_ops.cpu_type = "mips/sb1";
		break;

	default:
		printk(KERN_ERR "Profiling unsupported for this CPU\n");

		return -ENODEV;
	}

	save_perf_irq = perf_irq;
	perf_irq = mipsxx_perfcount_handler;

	return 0;
}

static void mipsxx_exit(void)
{
	int counters = op_model_mipsxx_ops.num_counters;

	counters = counters_per_cpu_to_total(counters);
	on_each_cpu(reset_counters, (void *)(long)counters, 1);

	perf_irq = save_perf_irq;
}

struct op_mips_model op_model_mipsxx_ops = {
	.reg_setup	= mipsxx_reg_setup,
	.cpu_setup	= mipsxx_cpu_setup,
	.init		= mipsxx_init,
	.exit		= mipsxx_exit,
	.cpu_start	= mipsxx_cpu_start,
	.cpu_stop	= mipsxx_cpu_stop,
};

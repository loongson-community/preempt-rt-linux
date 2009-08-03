/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2009 DSLab, Lanzhou University, China
 * Author: Wu Zhangjin <wuzj@lemote.com>
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/clocksource.h>

/*
 * mips-specific high precise sched_clock() implementation,
 *
 * currently, this is only needed in ftrace, so not override the original
 * sched_clock().
 */

unsigned long long native_sched_clock(void)
{
	u64 current_cycles;
	static unsigned long old_jiffies;
	static u64 time, old_cycles;

	preempt_disable_notrace();
    /* update timestamp to avoid missing the timer interrupt */
	if (time_before(jiffies, old_jiffies)) {
		old_jiffies = jiffies;
		time = sched_clock();
		old_cycles = clock->cycle_last;
	}
	current_cycles = clock->read();

	time = (time + cyc2ns(clock, (current_cycles - old_cycles)
				& clock->mask));

	old_cycles = current_cycles;
	preempt_enable_no_resched_notrace();

	return time;
}

/*
 * native_trace_clock_local(): the simplest and least coherent tracing clock.
 *
 * Useful for tracing that does not cross to other CPUs nor
 * does it go through idle events.
 */
u64 native_trace_clock_local(void)
{
	unsigned long flags;
	u64 clock;

	/*
	 * herein, we use the above native_sched_clock() to get high precise
	 * timestamp, because the original sched_clock in mips is jiffies based,
	 * which not have enough precision.
	 */
	raw_local_irq_save(flags);
	clock = native_sched_clock();
	raw_local_irq_restore(flags);

	return clock;
}

u64 trace_clock_local(void)
		__attribute__((alias("native_trace_clock_local")));


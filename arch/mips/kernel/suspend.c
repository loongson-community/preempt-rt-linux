/*
 * Suspend to Disk/Hibernation for MIPS CPUs
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

#include <asm/suspend.h>
#include <asm/current.h>
#include <asm/irq_regs.h>
#include <asm/page.h>
#include <linux/types.h>
#include <linux/mm.h>

extern const void __nosave_end, __nosave_begin;

/*
 * Saved register state
 */
#ifdef CONFIG_32BIT
uint32_t saved_regs[32];
#else
uint64_t saved_regs[32];
#endif

/*
 * Save processor state before suspend
 */
void save_processor_state(void)
{
	local_irq_disable();
}

/*
 * Restore processor state after suspend
 */
void restore_processor_state(void)
{
}

/*
 * pfn_is_nosave - check if the given pfn is in the nosave section
 * 
 */
int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = __pa_symbol(&__nosave_begin) 
		>> PAGE_SHIFT;
	unsigned long nosave_end_pfn = PAGE_ALIGN(__pa_symbol(&__nosave_end))
		>> PAGE_SHIFT;
	return (pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn);
}

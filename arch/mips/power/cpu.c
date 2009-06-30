/*
 * Suspend support for mips.
 *
 * Distributed under GPLv2
 *
 * Copyright (C) 2009 Lemote Inc. & Insititute of Computing Technology
 * Author: Hu Hongbing <huhb@lemote.com>
 *         Wu Zhangjin <wuzj@lemote.com>
 */
#include <asm/suspend.h>
#include <asm/fpu.h>
#include <asm/dsp.h>
#include <asm/bootinfo.h>

static u32 saved_status;
struct pt_regs saved_regs;

void save_processor_state(void)
{
	saved_status = read_c0_status();

	if (is_fpu_owner())
		save_fp(current);
	if (cpu_has_dsp)
		save_dsp(current);
}

void restore_processor_state(void)
{
	write_c0_status(saved_status);

	if (is_fpu_owner())
		restore_fp(current);
	if (cpu_has_dsp)
		restore_dsp(current);
}

int pfn_in_system_ram(unsigned long pfn)
{
	int i;

	for (i = 0; i < boot_mem_map.nr_map; i++) {
		if (boot_mem_map.map[i].type == BOOT_MEM_RAM) {
			if ((pfn >= (boot_mem_map.map[i].addr >> PAGE_SHIFT)) &&
				((pfn) < ((boot_mem_map.map[i].addr +
					boot_mem_map.map[i].size) >> PAGE_SHIFT)))
				return 1;
		}
	}
	return 0;
}

int pfn_is_nosave(unsigned long pfn)
{
	unsigned long nosave_begin_pfn = PFN_DOWN(__pa(&__nosave_begin));
	unsigned long nosave_end_pfn = PFN_UP(__pa(&__nosave_end));

	return ((pfn >= nosave_begin_pfn) && (pfn < nosave_end_pfn))
			|| !pfn_in_system_ram(pfn);
}

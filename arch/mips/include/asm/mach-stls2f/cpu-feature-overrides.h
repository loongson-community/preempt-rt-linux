/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2009 Philippe Vachon  <philippe@cowpig.ca>
 */
#ifndef __ASM_MACH_LEMOTE_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_LEMOTE_CPU_FEATURE_OVERRIDES_H

#include <linux/types.h>
#include <asm/mipsregs.h>

#define cpu_dcache_line_size()	128
#define cpu_icache_line_size()	128


#define cpu_has_4kex		1
#define cpu_has_3k_cache	0
#define cpu_has_4k_cache	0
#define cpu_has_tx39_cache	0
#define cpu_has_fpu		0
#define cpu_has_counter		1
#define cpu_has_watch		1
#define cpu_has_divec		1
#define cpu_has_vce		0
#define cpu_has_cache_cdex_p	0
#define cpu_has_cache_cdex_s	0
#define cpu_has_prefetch	1
#define cpu_has_llsc		1
#define cpu_has_vtag_icache	1
#define cpu_has_dc_aliases	0
#define cpu_has_ic_fills_f_dc	0
#define cpu_has_64bits		1
#define cpu_has_octeon_cache	0
#define cpu_has_saa			0
#define cpu_has_mips32r1	0
#define cpu_has_mips32r2	0
#define cpu_has_mips64r1	0
#define cpu_has_mips64r2	0
#define cpu_has_dsp			0
#define cpu_has_mipsmt		0
#define cpu_has_userlocal	0
#define cpu_has_vint		0
#define cpu_has_veic		0

#endif

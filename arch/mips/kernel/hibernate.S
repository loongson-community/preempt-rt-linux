/*
 * Suspend to Disk/Hibernation for MIPS
 *
 * (c) 2009 Philippe Vachon <philippe@cowpig.ca>
 * Inspired by the hibernation support code for x86-64
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

#ifdef CONFIG_32BIT
#define REGSIZE 4 /* bytes */
#define ADD addi
#else
#define REGSIZE 8 /* bytes */
#define ADD daddi
#endif

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>

LEAF(swsusp_arch_resume)
	.set noreorder
	jr ra
	move v0, zero
	.set reorder
END(swsusp_arch_resume)

/* save register state before we suspend */
LEAF(swsusp_arch_suspend)
#ifndef CONFIG_32BIT
	.set mips3
#endif
	.set noreorder
	PTR_LA t0, saved_regs
	PTR_S sp, (t0)
	ADD t0, REGSIZE
	PTR_S fp, (t0)
	ADD t0, REGSIZE
	PTR_S gp, (t0)
	ADD t0, REGSIZE
	PTR_S k1, (t0)
	ADD t0, REGSIZE
	PTR_S k0, (t0)
	ADD t0, REGSIZE
	PTR_S s7, (t0)
	ADD t0, REGSIZE
	PTR_S s6, (t0)
	ADD t0, REGSIZE
	PTR_S s5, (t0)
	ADD t0, REGSIZE
	PTR_S s4, (t0)
	ADD t0, REGSIZE
	PTR_S s3, (t0)
	ADD t0, REGSIZE
	PTR_S s2, (t0)
	ADD t0, REGSIZE
	PTR_S s1, (t0)
	ADD t0, REGSIZE
	PTR_S s0, (t0)
	ADD t0, REGSIZE
	PTR_S a3, (t0)
	ADD t0, REGSIZE
	PTR_S a2, (t0)
	ADD t0, REGSIZE
	PTR_S a1, (t0)
	ADD t0, REGSIZE
	PTR_S a0, (t0)
	ADD t0, REGSIZE
	PTR_S v1, (t0)
	ADD t0, REGSIZE
	PTR_S v0, (t0)
	nop
	nop
	nop
	j swsusp_save
	nop
    j _machine_halt
    nop
	jr ra
	nop
	.set reorder
END(swsusp_arch_suspend)

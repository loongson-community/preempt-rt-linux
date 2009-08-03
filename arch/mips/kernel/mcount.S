/*
 * the mips-specific _mcount implementation
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive for
 * more details.
 *
 * Copyright (C) 2009 DSLab, Lanzhou University, China
 * Author: Wu Zhangjin <wuzj@lemote.com>
 */

#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/ftrace.h>

	.text
	.set noreorder
	.set noat

	/* since there is a "addiu sp,sp,-8" before "jal _mcount" in 32bit */
	.macro RESTORE_SP_FOR_32BIT
#ifdef CONFIG_32BIT
	PTR_ADDIU	sp, 8
#endif
	.endm

	.macro MCOUNT_SAVE_REGS
	PTR_SUBU	sp, PT_SIZE
	PTR_S	ra, PT_R31(sp)
	PTR_S	AT, PT_R1(sp)
	PTR_S	a0, PT_R4(sp)
	PTR_S	a1, PT_R5(sp)
	PTR_S	a2, PT_R6(sp)
	PTR_S	a3, PT_R7(sp)
#ifdef CONFIG_64BIT
	PTR_S	a4, PT_R8(sp)
	PTR_S	a5, PT_R9(sp)
	PTR_S	a6, PT_R10(sp)
	PTR_S	a7, PT_R11(sp)
#endif
	.endm

	.macro MCOUNT_RESTORE_REGS
	PTR_L	ra, PT_R31(sp)
	PTR_L	AT, PT_R1(sp)
	PTR_L	a0, PT_R4(sp)
	PTR_L	a1, PT_R5(sp)
	PTR_L	a2, PT_R6(sp)
	PTR_L	a3, PT_R7(sp)
#ifdef CONFIG_64BIT
	PTR_L	a4, PT_R8(sp)
	PTR_L	a5, PT_R9(sp)
	PTR_L	a6, PT_R10(sp)
	PTR_L	a7, PT_R11(sp)
#endif
	PTR_ADDIU	sp, PT_SIZE
.endm

	.macro MCOUNT_SET_ARGS
	move	a0, ra		/* arg1: next ip, selfaddr */
	move	a1, AT		/* arg2: the caller's next ip, parent */
	PTR_SUBU a0, MCOUNT_INSN_SIZE
	.endm

	.macro RETURN_BACK
	jr ra
	move ra, AT
	.endm

NESTED(_mcount, PT_SIZE, ra)
	RESTORE_SP_FOR_32BIT
	PTR_L	t0, function_trace_stop
	bnez	t0, ftrace_stub
	nop

	PTR_LA	t0, ftrace_stub
	PTR_L	t1, ftrace_trace_function /* please don't use t1 later, safe? */
	bne	t0, t1, static_trace
	nop

	j	ftrace_stub
	nop

static_trace:
	MCOUNT_SAVE_REGS

	MCOUNT_SET_ARGS			/* call *ftrace_trace_function */
	jalr	t1
	nop

	MCOUNT_RESTORE_REGS
	.globl ftrace_stub
ftrace_stub:
	RETURN_BACK
	END(_mcount)

	.set at
	.set reorder

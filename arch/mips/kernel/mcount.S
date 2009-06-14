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

#ifdef CONFIG_DYNAMIC_FTRACE

LEAF(_mcount)
	RESTORE_SP_FOR_32BIT
	RETURN_BACK
	END(_mcount)

NESTED(ftrace_caller, PT_SIZE, ra)
	RESTORE_SP_FOR_32BIT
	lw	t0, function_trace_stop
	bnez	t0, ftrace_stub
	nop

	MCOUNT_SAVE_REGS
	PTR_S	AT, PT_R1(sp)

	MCOUNT_SET_ARGS
	.globl ftrace_call
ftrace_call:
	jal	ftrace_stub
	nop

	PTR_L	AT, PT_R1(sp)
	MCOUNT_RESTORE_REGS

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	.globl ftrace_graph_call
ftrace_graph_call:
	j	ftrace_stub
	nop
#endif

	.globl ftrace_stub
ftrace_stub:
	RETURN_BACK
	END(ftrace_caller)

#else	/* ! CONFIG_DYNAMIC_FTRACE */

NESTED(_mcount, PT_SIZE, ra)
	RESTORE_SP_FOR_32BIT
	PTR_L	t0, function_trace_stop
	bnez	t0, ftrace_stub
	nop

	PTR_LA	t0, ftrace_stub
	PTR_L	t1, ftrace_trace_function /* please don't use t1 later, safe? */
	bne	t0, t1, static_trace
	nop
#ifdef	CONFIG_FUNCTION_GRAPH_TRACER
	PTR_L	t2, ftrace_graph_return
	bne	t0,	t2, ftrace_graph_caller
	nop
	PTR_LA	t0, ftrace_graph_entry_stub
	PTR_L	t2, ftrace_graph_entry
	bne	t0,	t2, ftrace_graph_caller
	nop
#endif
	j	ftrace_stub
	nop

static_trace:
	MCOUNT_SAVE_REGS
	PTR_S	AT, PT_R1(sp)

	MCOUNT_SET_ARGS			/* call *ftrace_trace_function */
	jalr	t1
	nop

	PTR_L	AT, PT_R1(sp)
	MCOUNT_RESTORE_REGS
	.globl ftrace_stub
ftrace_stub:
	RETURN_BACK
	END(_mcount)

#endif	/* ! CONFIG_DYNAMIC_FTRACE */

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

NESTED(ftrace_graph_caller, PT_SIZE, ra)
	MCOUNT_SAVE_REGS
	PTR_S	v0, PT_R2(sp)

	MCOUNT_SET_ARGS
	jal	prepare_ftrace_return
	nop

	/* overwrite the parent as &return_to_handler: v0 -> AT */
	move	AT,	v0

	PTR_L	v0, PT_R2(sp)
	MCOUNT_RESTORE_REGS
	RETURN_BACK
	END(ftrace_graph_caller)

	.align	2
	.globl	return_to_handler
return_to_handler:
	PTR_SUBU	sp, PT_SIZE
	PTR_S	v0, PT_R2(sp)

	jal	ftrace_return_to_handler
	nop

	/* restore the real parent address: v0 -> ra */
	move	ra, v0

	PTR_L	v0, PT_R2(sp)
	PTR_ADDIU	sp, PT_SIZE

	jr	ra

#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

	.set at
	.set reorder

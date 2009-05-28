/*
 * Access to user system call parameters and results
 *
 * Copyright (C) 2008 Red Hat, Inc.  All rights reserved.
 * Copyright (C) 2009 DSLab, Lanzhou University, China
 * Author: Wu Zhangjin <wuzj@lemote.com>
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 *
 * See asm-generic/syscall.h for descriptions of what we must do here.
 */

#ifndef _ASM_SYSCALL_H
#define _ASM_SYSCALL_H	1

#include <linux/sched.h>

static inline long syscall_get_nr(struct task_struct *task,
				  struct pt_regs *regs)
{
	/*        syscall   Exc-Code: 0 1000 00     v0 */
	return ((regs->cp0_cause&0xff) == 0x20)  ? regs->regs[2] : -1L;
}

static inline void syscall_rollback(struct task_struct *task,
				    struct pt_regs *regs)
{
	regs->regs[2] = regs->orig_v0;
}

static inline long syscall_get_error(struct task_struct *task,
				     struct pt_regs *regs)
{
	return regs->regs[2] ? -regs->regs[2] : 0;
}

static inline long syscall_get_return_value(struct task_struct *task,
					    struct pt_regs *regs)
{
	return regs->regs[2];
}

static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	if (error)
		regs->regs[2] = -error;
	else
		regs->regs[2] = val;
}

static inline void syscall_get_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 unsigned long *args)
{
#ifdef CONFIG_32BIT
	/* fixme: only 4 argument register available in mip32, so, how to handle
	 * others?
	 */
	BUG_ON(i + n > 4);
#else
	BUG_ON(i + n > 6);
#endif
	memcpy(args, &regs->regs[4 + i], n * sizeof(args[0]));
}

static inline void syscall_set_arguments(struct task_struct *task,
					 struct pt_regs *regs,
					 unsigned int i, unsigned int n,
					 const unsigned long *args)
{
#ifdef CONFIG_32BIT
	BUG_ON(i + n > 4);
#else
	BUG_ON(i + n > 6);
#endif
	memcpy(&regs->regs[4 + i], args, n * sizeof(args[0]));
}

#endif	/* _ASM_SYSCALL_H */

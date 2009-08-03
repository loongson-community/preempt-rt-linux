/*
 * Code for replacing ftrace calls with jumps.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 * Copyright (C) 2009 DSLab, Lanzhou University, China
 * Author: Wu Zhangjin <wuzj@lemote.com>
 *
 * Thanks goes to Steven Rostedt for writing the original x86 version.
 */

#include <linux/spinlock.h>
#include <linux/hardirq.h>
#include <linux/uaccess.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/ftrace.h>

#include <asm/cacheflush.h>
#include <asm/ftrace.h>
#include <asm/asm.h>
#include <asm/unistd.h>

#ifdef CONFIG_DYNAMIC_FTRACE

#define JAL 0x0c000000	/* jump & link: ip --> ra, jump to target */
#define ADDR_MASK 0x03ffffff	/*  op_code|addr : 31...26|25 ....0 */

static unsigned int ftrace_nop = 0x00000000;

static unsigned char *ftrace_call_replace(unsigned long op_code,
					  unsigned long addr)
{
    static unsigned int op;

    op = op_code | ((addr >> 2) & ADDR_MASK);

    return (unsigned char *) &op;
}

static atomic_t nmi_running = ATOMIC_INIT(0);
static int mod_code_status;	/* holds return value of text write */
static int mod_code_write;	/* set when NMI should do the write */
static void *mod_code_ip;	/* holds the IP to write to */
static void *mod_code_newcode;	/* holds the text to write to the IP */

static unsigned nmi_wait_count;
static atomic_t nmi_update_count = ATOMIC_INIT(0);

int ftrace_arch_read_dyn_info(char *buf, int size)
{
    int r;

    r = snprintf(buf, size, "%u %u",
		 nmi_wait_count, atomic_read(&nmi_update_count));
    return r;
}

static void ftrace_mod_code(void)
{
    /*
     * Yes, more than one CPU process can be writing to mod_code_status.
     *    (and the code itself)
     * But if one were to fail, then they all should, and if one were
     * to succeed, then they all should.
     */
    mod_code_status = probe_kernel_write(mod_code_ip, mod_code_newcode,
					 MCOUNT_INSN_SIZE);

    /* if we fail, then kill any new writers */
    if (mod_code_status)
		mod_code_write = 0;
}

void ftrace_nmi_enter(void)
{
    atomic_inc(&nmi_running);
    /* Must have nmi_running seen before reading write flag */
    smp_mb();
    if (mod_code_write) {
		ftrace_mod_code();
		atomic_inc(&nmi_update_count);
    }
}

void ftrace_nmi_exit(void)
{
    /* Finish all executions before clearing nmi_running */
    smp_wmb();
    atomic_dec(&nmi_running);
}

static void wait_for_nmi(void)
{
    int waited = 0;

    while (atomic_read(&nmi_running)) {
		waited = 1;
		cpu_relax();
    }

    if (waited)
		nmi_wait_count++;
}

static int do_ftrace_mod_code(unsigned long ip, void *new_code)
{
    mod_code_ip = (void *) ip;
    mod_code_newcode = new_code;

    /* The buffers need to be visible before we let NMIs write them */
    smp_wmb();

    mod_code_write = 1;

    /* Make sure write bit is visible before we wait on NMIs */
    smp_mb();

    wait_for_nmi();

    /* Make sure all running NMIs have finished before we write the code */
    smp_mb();

    ftrace_mod_code();

    /* Make sure the write happens before clearing the bit */
    smp_wmb();

    mod_code_write = 0;

    /* make sure NMIs see the cleared bit */
    smp_mb();

    wait_for_nmi();

    return mod_code_status;
}

static unsigned char *ftrace_nop_replace(void)
{
    return (unsigned char *) &ftrace_nop;
}

static int
ftrace_modify_code(unsigned long ip, unsigned char *old_code,
		   unsigned char *new_code)
{
    unsigned char replaced[MCOUNT_INSN_SIZE];

    /* read the text we want to modify */
    if (probe_kernel_read(replaced, (void *) ip, MCOUNT_INSN_SIZE))
		return -EFAULT;

    /* Make sure it is what we expect it to be */
    if (memcmp(replaced, old_code, MCOUNT_INSN_SIZE) != 0)
		return -EINVAL;

    /* replace the text with the new text */
    if (do_ftrace_mod_code(ip, new_code))
		return -EPERM;

    return 0;
}

int ftrace_make_nop(struct module *mod,
		    struct dyn_ftrace *rec, unsigned long addr)
{
    unsigned char *new, *old;

    old = ftrace_call_replace(JAL, addr);
    new = ftrace_nop_replace();

    return ftrace_modify_code(rec->ip, old, new);
}

int ftrace_make_call(struct dyn_ftrace *rec, unsigned long addr)
{
    unsigned char *new, *old;

    old = ftrace_nop_replace();
    new = ftrace_call_replace(JAL, addr);

    return ftrace_modify_code(rec->ip, old, new);
}

int ftrace_update_ftrace_func(ftrace_func_t func)
{
    unsigned long ip = (unsigned long) (&ftrace_call);
    unsigned char old[MCOUNT_INSN_SIZE], *new;
    int ret;

    memcpy(old, &ftrace_call, MCOUNT_INSN_SIZE);
    new = ftrace_call_replace(JAL, (unsigned long) func);
    ret = ftrace_modify_code(ip, old, new);

    return ret;
}

int __init ftrace_dyn_arch_init(void *data)
{
    /* The return code is retured via data */
    *(unsigned long *) data = 0;

    return 0;
}
#endif				/* CONFIG_DYNAMIC_FTRACE */

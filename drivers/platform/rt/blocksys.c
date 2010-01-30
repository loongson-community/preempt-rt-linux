/*
 * blocksys - block the system to artificially create a system latency
 *
 * Copyright (C) 2007  Carsten Emde <C.Emde@osadl.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <linux/module.h>

#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/time.h>
#include <linux/smp_lock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include <asm/system.h>

#define VERSION "0.2"

#define BLOCKSYS_MINOR MISC_DYNAMIC_MINOR

DEFINE_SPINLOCK(blocksys_state_lock);
DEFINE_SPINLOCK(blocksys_block_lock);
static int blocksys_open_cnt;	/* #times opened */
static int blocksys_open_mode;	/* special open modes */
#define BLOCKSYS_WRITE 1	/* opened for writing (exclusive) */
#define BLOCKSYS_EXCL 2		/* opened with O_EXCL */

/*
 * These are the file operation function for user access to /dev/blocksys
 */
static ssize_t
blocksys_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	int nops;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (sscanf(buf, "%d", &nops) >= 1) {
		struct timeval before, after, diff;
		int cpu = smp_processor_id();

		printk(KERN_INFO "blocksys: CPU #%d will be blocked "
		       "for %d nops\n", cpu, nops);
		do_gettimeofday(&before);
		spin_lock(&blocksys_block_lock);
		preempt_disable();
		local_irq_disable();
		while (nops--)
			asm("nop");
		local_irq_enable();
		preempt_enable();
		do_gettimeofday(&after);
		spin_unlock(&blocksys_block_lock);
		diff.tv_sec = after.tv_sec - before.tv_sec;
		diff.tv_usec = after.tv_usec - before.tv_usec;
		if (diff.tv_usec < 0) {
			--diff.tv_sec;
			diff.tv_usec += 1000000;
		}
		printk(KERN_INFO "blocksys: CPU #%d blocked "
		       "about %ld us\n", cpu,
		       diff.tv_sec * 1000000 + diff.tv_usec);
	}
	return strlen(buf);
}

static int blocksys_open(struct inode *inode, struct file *file)
{
	spin_lock(&blocksys_state_lock);

	if ((blocksys_open_cnt && (file->f_flags & O_EXCL)) ||
	    (blocksys_open_mode & BLOCKSYS_EXCL)) {
		spin_unlock(&blocksys_state_lock);
		return -EBUSY;
	}

	if (file->f_flags & O_EXCL)
		blocksys_open_mode |= BLOCKSYS_EXCL;
	if (file->f_mode & 2)
		blocksys_open_mode |= BLOCKSYS_WRITE;
	blocksys_open_cnt++;

	spin_unlock(&blocksys_state_lock);

	return 0;
}

static int blocksys_release(struct inode *inode, struct file *file)
{
	spin_lock(&blocksys_state_lock);

	blocksys_open_cnt--;

	if (blocksys_open_cnt == 1 && blocksys_open_mode & BLOCKSYS_EXCL)
		blocksys_open_mode &= ~BLOCKSYS_EXCL;
	if (file->f_mode & 2)
		blocksys_open_mode &= ~BLOCKSYS_WRITE;

	spin_unlock(&blocksys_state_lock);

	return 0;
}

static const struct file_operations blocksys_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.open = blocksys_open,
	.write = blocksys_write,
	.release = blocksys_release,
};

static struct miscdevice blocksys_dev = {
	BLOCKSYS_MINOR,
	"blocksys",
	&blocksys_fops
};

static int __init blocksys_init(void)
{
	int ret;

	ret = misc_register(&blocksys_dev);
	if (ret)
		printk(KERN_ERR
		       "blocksys: can't register dynamic misc device\n");
	else
		printk(KERN_INFO "blocksys driver v" VERSION
		       " misc device %d\n", blocksys_dev.minor);
	return ret;
}

static void __exit blocksys_exit(void)
{
	misc_deregister(&blocksys_dev);
}

module_init(blocksys_init);
module_exit(blocksys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Carsten Emde <C.Emde@osadl.org>");
MODULE_DESCRIPTION("Block the current CPU for a defined number of NOPs");

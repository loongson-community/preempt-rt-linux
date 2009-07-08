/*
 * Debug IO and MSR resources driver on Linux
 *
 * Author: liujl <liujl@lemote.com>
 * 	   huangw <huangw@lemote.com>
 * Date	 : 2009-03-03
 *
 * NOTE :
 * 	1, The IO and the MSR resources accessing read/write are supported.
 */

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/apm_bios.h>
#include <linux/capability.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/apm-emulation.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include <asm/delay.h>

#include "ec.h"

struct io_msr_reg {
	u32 addr;		/* the address of IO and MSR registers */
	u8 val;			/* the register value for IO */
	u32 hi;			/* the register value for MSR's high part */
	u32 lo;			/* the register value for MSR's low part */
};

#define	IOCTL_RDMSR	_IOR('F', 5, int)
#define	IOCTL_WRMSR	_IOR('F', 6, int)
#define	IOCTL_RDIO	_IOR('F', 7, int)
#define	IOCTL_WRIO	_IOR('F', 8, int)

/* ec io space range */
#define IO_MAX_ADDR	0xBFD0FFFF
#define IO_MIN_ADDR	0xBFD00000

static int io_msr_ioctl(struct inode *inode, struct file *filp, u_int cmd,
			u_long arg)
{
	void __user *ptr = (void __user *)arg;
	struct io_msr_reg *iomsrreg = (struct io_msr_reg *)(filp->private_data);
	int ret = 0;

	switch (cmd) {
	case IOCTL_RDIO:
		ret = copy_from_user(iomsrreg, ptr, sizeof(struct io_msr_reg));
		if (ret) {
			printk(KERN_ERR "IO read : copy from user error.\n");
			return -EFAULT;
		}

		if (iomsrreg->addr > IO_MAX_ADDR
		    || iomsrreg->addr < IO_MIN_ADDR) {
			printk(KERN_ERR "IO read : out of IO address range.\n");
			return -EINVAL;
		}
#ifdef	CONFIG_64BIT
		iomsrreg->val =
		    *((unsigned char *)(iomsrreg->
						 addr | 0xffffffff00000000));
#else
		iomsrreg->val = *((unsigned char *)(iomsrreg->addr));
#endif
		ret = copy_to_user(ptr, iomsrreg, sizeof(struct io_msr_reg));
		if (ret) {
			printk(KERN_ERR "IO read : copy to user error.\n");
			return -EFAULT;
		}
		break;
	case IOCTL_WRIO:
		ret = copy_from_user(iomsrreg, ptr, sizeof(struct io_msr_reg));
		if (ret) {
			printk(KERN_ERR "IO write : copy from user error.\n");
			return -EFAULT;
		}

		if (iomsrreg->addr > IO_MAX_ADDR
		    || iomsrreg->addr < IO_MIN_ADDR) {
			printk(KERN_ERR
			       "IO write : out of IO address range.\n");
			return -EINVAL;
		}
#ifdef	CONFIG_64BIT
		*((unsigned char *)(iomsrreg->addr | 0xffffffff00000000)) =
		    iomsrreg->val;
#else
		*((unsigned char *)(iomsrreg->addr)) = iomsrreg->val;
#endif
		break;
	case IOCTL_RDMSR:
		ret = copy_from_user(iomsrreg, ptr, sizeof(struct io_msr_reg));
		if (ret) {
			printk(KERN_ERR "MSR read : copy from user error.\n");
			return -EFAULT;
		}
		_rdmsr(iomsrreg->addr, &(iomsrreg->hi), &(iomsrreg->lo));
		ret = copy_to_user(ptr, iomsrreg, sizeof(struct io_msr_reg));
		if (ret) {
			printk(KERN_ERR "MSR read : copy to user error.\n");
			return -EFAULT;
		}
		break;
	case IOCTL_WRMSR:
		ret = copy_from_user(iomsrreg, ptr, sizeof(struct io_msr_reg));
		if (ret) {
			printk(KERN_ERR "MSR write : copy from user error.\n");
			return -EFAULT;
		}
		_wrmsr(iomsrreg->addr, iomsrreg->hi, iomsrreg->lo);
		break;

	default:
		break;
	}

	return 0;
}

static long io_msr_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	return io_msr_ioctl(file->f_dentry->d_inode, file, cmd, arg);
}

static int io_msr_open(struct inode *inode, struct file *filp)
{
	struct io_msr_reg *iomsrreg = NULL;
	iomsrreg = kmalloc(sizeof(struct io_msr_reg), GFP_KERNEL);
	if (iomsrreg)
		filp->private_data = iomsrreg;

	return iomsrreg ? 0 : -ENOMEM;
}

static int io_msr_release(struct inode *inode, struct file *filp)
{
	struct io_msr_reg *iomsrreg = (struct io_msr_reg *)(filp->private_data);

	filp->private_data = NULL;
	kfree(iomsrreg);

	return 0;
}

static const struct file_operations io_msr_fops = {
	.open = io_msr_open,
	.release = io_msr_release,
	.read = NULL,
	.write = NULL,
#ifdef	CONFIG_64BIT
	.compat_ioctl = io_msr_compat_ioctl,
#else
	.ioctl = io_msr_ioctl,
#endif
};

static struct miscdevice io_msr_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "io_msr_dev",
	.fops = &io_msr_fops
};

static int __init io_msr_init(void)
{
	int ret;

	printk(KERN_INFO "IO and MSR read/write device init.\n");
	ret = misc_register(&io_msr_device);

	return ret;
}

static void __exit io_msr_exit(void)
{
	printk(KERN_INFO "IO and MSR read/write device exit.\n");
	misc_deregister(&io_msr_device);
}

module_init(io_msr_init);
module_exit(io_msr_exit);

MODULE_AUTHOR("liujl <liujl@lemote.com>");
MODULE_DESCRIPTION("IO and MSR resources debug");
MODULE_LICENSE("GPL");

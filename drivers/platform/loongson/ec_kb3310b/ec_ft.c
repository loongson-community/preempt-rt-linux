/*
 * EC(Embedded Controller) KB3310B Fan and Temperature management driver on
 * Linux
 *
 * Author: liujl <liujl@lemote.com>
 * Date	 : 2008-06-23
 *
 * NOTE: The SDA1/SCL1 in KB3310B are used to communicate with the
 * Temperature IC, Here we use ADT75 IC to handle the temperature management.
 * and the Fan is controller by ADT75AZ, but the status is reflected to the
 * KB3310 FANFB All the resources for handle battery management in KB3310B:
 *  1, one SMBus interface with port 1
 *  2, gpio14 input alternate for FANFB
 */

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/capability.h>
#include <linux/sched.h>
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
#include "ec_misc_fn.h"

#define	EC_FT_DEV		"ft"
/* The EC Battery device is one of the misc device, here is his minor number. */
#define	ECFT_MINOR_DEV		MISC_DYNAMIC_MINOR
#define PROC_FT_DIR		"ft"

static struct task_struct *ft_tsk;
static DEFINE_MUTEX(ft_info_lock);

/* driver version */
static const char driver_version[] = VERSION;

/* fan speed divider */
#define	FAN_SPEED_DIVIDER		480000	/* (60*1000*1000/62.5/2)*/

/* fan status : on or off */
#define	FAN_STATUS_ON			0x01
#define	FAN_STATUS_OFF			0x00
/* fan is not running */
#define	FAN_SPEED_NONE			0x00
/* temperature negative or positive */
#define	TEMPERATURE_POSITIVE		0x00
#define	TEMPERATURE_NEGATIVE		0x01
/* temperature value is zero */
#define	TEMPERATURE_NONE		0x00
struct ft_info {
	u8 fan_on;
	u16 fan_speed;
	u8 temperature_pn;
	u8 temperature;
} ft_info = {
	.fan_on = FAN_STATUS_ON,
	.fan_speed = FAN_SPEED_NONE,
	.temperature_pn = TEMPERATURE_POSITIVE,
	.temperature = TEMPERATURE_NONE
};

static struct miscdevice ft_device = {
	.minor = ECFT_MINOR_DEV,
	.name = EC_FT_DEV,
	.fops = NULL
};

#ifdef	CONFIG_PROC_FS
static int ft_proc_read(char *page, char **start, off_t off, int count,
			int *eof, void *data);
static struct proc_dir_entry *ft_proc_entry;

static int ft_proc_read(char *page, char **start, off_t off, int count,
			int *eof, void *data)
{
	struct ft_info info;
	int ret;

	mutex_lock(&ft_info_lock);
	info.fan_on = ft_info.fan_on;
	if (info.fan_on == FAN_STATUS_ON)
		info.fan_speed = ft_info.fan_speed;
	else
		info.fan_speed = 0x00;
	info.temperature_pn = ft_info.temperature_pn;
	info.temperature = ft_info.temperature;
	mutex_unlock(&ft_info_lock);

	ret =
	    sprintf(page, "%s 0x%02x %d 0x%02x %d\n", driver_version,
		    info.fan_on, info.fan_speed, info.temperature_pn,
		    info.temperature);

	ret -= off;
	if (ret < off + count)
		*eof = 1;

	*start = page + off;
	if (ret > count)
		ret = count;
	if (ret < 0)
		ret = 0;

	return ret;
}
#endif

static int ft_manager(void *arg)
{
	u8 val, reg_val;

	PRINTK_DBG(KERN_DEBUG "Fan & Temperature Management thread started.\n");
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		/* schedule every 1s */
		schedule_timeout(HZ);	/*  1s */

		if (kthread_should_stop())
			break;

		mutex_lock(&ft_info_lock);

		val = ec_read(REG_FAN_STATUS);
		ft_info.fan_speed =
		    FAN_SPEED_DIVIDER /
		    (((ec_read(REG_FAN_SPEED_HIGH) & 0x0f) << 8) |
		     ec_read(REG_FAN_SPEED_LOW));
		reg_val = ec_read(REG_TEMPERATURE_VALUE);

		if (val)
			ft_info.fan_on = FAN_STATUS_ON;
		else
			ft_info.fan_on = FAN_STATUS_OFF;

		if (reg_val & (1 << 7)) {
			ft_info.temperature = (reg_val & 0x7f) - 128;
			ft_info.temperature_pn = TEMPERATURE_NEGATIVE;
		} else {
			ft_info.temperature = (reg_val & 0xff);
			ft_info.temperature_pn = TEMPERATURE_POSITIVE;
		}
		mutex_unlock(&ft_info_lock);
	}

	PRINTK_DBG(KERN_DEBUG "Fan and Temperature Management thread exit.\n");

	return 0;

}

static int __init ft_init(void)
{
	int ret;

	printk(KERN_INFO
	       "Fan and Temperature on KB3310B Embedded Controller init.\n");
	ft_tsk = kthread_create(ft_manager, NULL, "ft_manager");
	if (IS_ERR(ft_tsk)) {
		ret = PTR_ERR(ft_tsk);
		kthread_stop(ft_tsk);
		ft_tsk = NULL;
		printk(KERN_ERR "ecft : ft management error.\n");
		return ret;
	}
	ft_tsk->flags |= PF_NOFREEZE;
	wake_up_process(ft_tsk);

#ifdef CONFIG_PROC_FS
	ft_proc_entry = NULL;
	ft_proc_entry = create_proc_entry(EC_FT_DEV, S_IWUSR | S_IRUGO, NULL);
	if (ft_proc_entry == NULL) {
		printk(KERN_ERR "EC FT : register /proc/ft failed.\n");
		return -EINVAL;
	}
	ft_proc_entry->read_proc = ft_proc_read;
	ft_proc_entry->write_proc = NULL;
	ft_proc_entry->data = NULL;
#endif

	ret = misc_register(&ft_device);
	if (ret != 0) {
#ifdef	CONFIG_PROC_FS
		remove_proc_entry(EC_FT_DEV, NULL);
#endif
		kthread_stop(ft_tsk);
		printk(KERN_ERR "ecft : fan&temperature register error.\n");
	}

	return ret;
}

static void __exit ft_exit(void)
{
	misc_deregister(&ft_device);
#ifdef	CONFIG_PROC_FS
	remove_proc_entry(EC_FT_DEV, NULL);
#endif
	kthread_stop(ft_tsk);
}

module_init(ft_init);
module_exit(ft_exit);

MODULE_AUTHOR("liujl <liujl@lemote.com>");
MODULE_DESCRIPTION("Advanced Fan and Temperature Management For Kb3310");
MODULE_LICENSE("GPL");

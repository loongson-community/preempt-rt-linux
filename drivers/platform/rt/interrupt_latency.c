/*
 * Interrupt Latency Testing Driver
 *
 * This driver is written for testing the interrupt latency of a real-time
 * system.
 *
 * Necessaries:
 *
 *   o square wave generator, i.e. external timer, interrupt pin + external
 *   square wave generator...
 *   o sched_clock()
 *
 * Initialization:
 *
 *   o calculate the duration spent on calling sched_clock()
 *     t1 = sched_clock();
 *     for (i = 0; i <= LOOP; i ++)
 *       (void)sched_clock();
 *     t2 = sched_clock();
 *     delta = t2 - t1;
 *
 * Interrupt handler:
 *
 *   o time = sched_clock()
 *   o print the time and toggle the output: (time - delta) : 0|1
 *   o Interrupt latency = Th-Ti= (Th+delta) - (Ti+delta) = Th' - Ti'
 *
 *       Ti          Ti'                  Th          Th': output 1|0
 *       |<--delta--->|--------------------|<--delta-->|
 *       |<---------interrupt latency----->|
 *       ^                                 ^
 *       |                                 |
 *   interrupt                         interrupt handler
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/cpumask.h>
#include <linux/time.h>
#include <linux/smp_lock.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>	/* for sched_clock */
#include <linux/uaccess.h>	/* for put_user */
#include <linux/interrupt.h>

#include <asm/system.h>

#undef UNTHREAD_INTERRUPT

/*
 *  Prototypes - this would normally go in a .h file
 */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define FAILURE 1
/* Dev name as it appears in /proc/devices   */
#define DEVICE_NAME "interrupt_latency"
/* Max length of the message from the device */
#define BUF_LEN 1024

/*
 * Global variables are declared as static, so are global within the file.
 */

static int Major;		/* Major number assigned to our device driver */
static int Device_Open;		/* Is device open?
				 * Used to prevent multiple access to device */
static char msg[BUF_LEN];	/* The msg the device will give when asked */
static char *msg_Ptr;

#define IRQ_NUM	5

#if defined(CONFIG_CS5536) && !defined(CONFIG_CS5536_MFGPT)

#undef HZ
#define HZ 500
#define PERIOD 2000000	/* NSEC_PER_SEC / HZ */
#include <cs5536/cs5536_mfgpt.h>

static u32 mfgpt_base;

static inline void arch_irq_enable(void)
{
	outw(COMPARE, MFGPT0_CMP2);	/* set comparator2 */
	outw(0, MFGPT0_CNT);	/* set counter to 0 */
	outw(0xe310, MFGPT0_SETUP);
}
static inline void arch_irq_disable(void)
{
	outw(inw(MFGPT0_SETUP) & 0x7fff, MFGPT0_SETUP);
}

static inline void setup_mfgpt_timer(void)
{
	u32 basehi;

	/* Enable MFGPT0 Comparator 2 Output to the Interrupt Mapper */
	_wrmsr(DIVIL_MSR_REG(MFGPT_IRQ), 0, 0x100);

	/* Enable Interrupt Gate 5 */
	_wrmsr(DIVIL_MSR_REG(PIC_ZSEL_LOW), 0, 0x50000);

	/* get MFGPT base address */
	_rdmsr(DIVIL_MSR_REG(DIVIL_LBAR_MFGPT), &basehi, &mfgpt_base);
}

static inline int init_arch_irq(void)
{
	setup_mfgpt_timer();

	return 0;
}

static inline void exit_arch_irq(void)
{
	arch_irq_disable();
}

static inline void arch_irq_handler(void)
{
	/* Ack */
	outw(inw(MFGPT0_SETUP) | 0x4000, MFGPT0_SETUP);
}

#else
static inline int init_arch_irq(void)
{
	return -EFAULT;
}
static inline void init_exit_irq(void)
{
}
static inline void arch_irq_handler(void)
{
}
static inline void arch_irq_enable(void)
{
}
static inline void arch_irq_disable(void)
{
}
#endif

static u64 th, ti, sum, diff;
static u32 total, max, min = UINT_MAX;

static void irq_enable(void)
{
	unsigned long flags;

	local_irq_save(flags);
	arch_irq_enable();
	ti = sched_clock() + PERIOD;
	min = UINT_MAX;
	max = 0;
	sum = 0;
	total = 0;
	local_irq_restore(flags);
}

static void irq_disable(void)
{
	arch_irq_disable();
	min = UINT_MAX;
	max = 0;
	sum = 0;
	total = 0;
}

static int enable_diff_output;
static int enable_tracing = 1;

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	unsigned long flags;

	local_irq_save(flags);
	th = sched_clock();
	arch_irq_disable();
	local_irq_restore(flags);

	if (th > ti)
		diff = th - ti;
	else
		return IRQ_NONE;
	do_div(diff, 1000);
	if (diff > max)
		max = diff;
	else if (diff < min)
		min = diff;
	sum += diff;
	total++;

	local_irq_save(flags);
	arch_irq_enable();
	ti = sched_clock() + PERIOD;
	local_irq_restore(flags);

	return IRQ_HANDLED;
}


static struct irqaction irq_action = {
	.handler = irq_handler,
	.name = "interrupt_latency",
#ifdef UNTHREAD_INTERRUPT
	.flags = IRQF_NODELAY,
#else
	.flags = IRQF_SHARED,
#endif
};

static int init_irq(void)
{
	int ret;
	unsigned long flags;

	ret = init_arch_irq();
	if (ret)
		return -EFAULT;

	local_irq_save(flags);
	arch_irq_enable();
	ti = sched_clock() + PERIOD;
	local_irq_restore(flags);

	printk(KERN_INFO"ti: %lld\n", ti);

	ret = setup_irq(IRQ_NUM, &irq_action);
	if (ret)
		return -EFAULT;
	return 0;
}

static void exit_irq(void)
{
	remove_irq(IRQ_NUM, &irq_action);
	exit_arch_irq();
}

static const struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

/*
 * This function is called when the module is loaded
 */
static __init int interrupt_latency_init(void)
{
	int ret;

	Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
		printk(KERN_ALERT "Registering char device failed with %d\n",
		       Major);
		return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	ret = init_irq();
	if (ret) {
		printk(KERN_ERR "Failed to setup irq: %d\n", IRQ_NUM);
		return ret;
	} else
		printk(KERN_INFO "Setup irq: %d\n", IRQ_NUM);

	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
static __exit void interrupt_latency_exit(void)
{
	exit_irq();
	/*
	 * Unregister the device
	 */
	unregister_chrdev(Major, DEVICE_NAME);
}

/*
 * Methods
 */

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
	u64 avg;

	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	avg = sum;
	do_div(avg, total);
	sprintf(msg, "%d %d %d\n", (u32)avg, max, min);

	msg_Ptr = msg;
	try_module_get(THIS_MODULE);

	return SUCCESS;
}

/*
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
	Device_Open--;		/* We're now ready for our next caller */

	/*
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module.
	 */
	module_put(THIS_MODULE);

	return 0;
}

/*
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t *offset)
{
	/*
	 * Number of bytes actually written to the buffer
	 */
	int bytes_read = 0;

	if (!enable_tracing)
		return 0;

	if (enable_diff_output) {
		return snprintf(buffer, length, "%d\n", (int)diff);
	}

	/*
	 * If we're at the end of the message,
	 * return 0 signifying end of file
	 */
	if (*msg_Ptr == 0)
		return 0;

	/*
	 * Actually put the data into the buffer
	 */
	while (length && *msg_Ptr) {

		/*
		 * The buffer is in the user data segment, not the kernel
		 * segment so "*" assignment won't work.  We have to use
		 * put_user which copies data from the kernel data segment to
		 * the user data segment.
		 */
		put_user(*(msg_Ptr++), buffer++);

		length--;
		bytes_read++;
	}

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * Called when a process writes to dev file: echo "hi" > /dev/hello
 */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	unsigned int cmd;

	if (sscanf(buff, "%d", &cmd) >= 1) {
		switch (cmd) {
		case 0:
			enable_tracing = 0;
			irq_disable();
			break;
		case 1:
			enable_tracing = 1;
			irq_enable();
			break;
		case 2:
			enable_diff_output = 1;
			break;
		case 3:
			enable_diff_output = 0;
		default:
			break;
		}
	}

	return strlen(buff);
}

module_init(interrupt_latency_init);
module_exit(interrupt_latency_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wu Zhangin <wuzhangjin@gmail.com>");
MODULE_DESCRIPTION("Test the interrupt latency");

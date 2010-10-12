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
 *
 * Interrupt handler:
 *
 *   o Interrupt latency = Th-Ti
 *
 *       Ti                              Th
 *       |<--------interrupt latency----->|
 *       ^                                ^
 *       |                                |
 *   interrupt                         interrupt handler
 *
 * Change Log:
 *
 *   o 03/16 2010, using do_gettimeofday() instead of sched_clock() to
 *   communicate with the user-space application.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/cpumask.h>
#include <linux/time.h>
#include <linux/smp_lock.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>	/* for sched_clock */
#include <linux/uaccess.h>	/* for put_user */
#include <linux/interrupt.h>

#include <asm/system.h>

/*
 * For the unthread interrupt, the period is fixed at 100 for we can not sleep
 * in the interrupt handler and the period can not be too higher, otherwise,
 * the precison will be very low.
 */
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

static wait_queue_head_t wq;
static int irq_on;

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
#define HZ 10000
#define PERIOD 100	/* us, USEC_PER_SEC / HZ */
#include <cs5536/cs5536_mfgpt.h>

static u32 mfgpt_base;
static int interval = 1000, period = PERIOD;	/* us */
static struct timeval ti, th, te, ts, diff;
static u64 sum;
static u32 total, max = 0, min = UINT_MAX;

static inline void arch_irq_enable(void)
{
	u16 compare;
	int hz;

	/*
	 * If the interval is smaller than 1000, we use the period of the timer
	 * as the interval, the real interval is period + handler latency.
	 */
	if (interval < 1000) {
		period = interval;
		hz = USEC_PER_SEC / period;
		compare = (u16)(((u32)MFGPT_TICK_RATE + hz/2) / hz);
	} else
		compare = COMPARE;
	outw(compare, MFGPT0_CMP2);	/* set comparator2 */
	outw(0, MFGPT0_CNT);	/* set counter to 0 */
	outw(0xe310, MFGPT0_SETUP);
}

static inline void arch_irq_handler(void)
{
	/* Ack */
	outw(inw(MFGPT0_SETUP) | 0x4000, MFGPT0_SETUP);
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

static void reset_variables(void)
{
	total = 0;
	max = 0;
	sum = 0;
	min = UINT_MAX;
	ti.tv_sec = 0;
	ti.tv_usec = 0;
	th.tv_sec = 0;
	th.tv_usec = 0;
	irq_on = 0;
}

static void set_irq_and_get_time(void)
{
	/*
	 * Set the interrupt for being emitted after 'period' us from current.
	 */
	arch_irq_enable();
	/*
	 * get the time when setting the interrupt
	 */
	do_gettimeofday(&ts);
}

static void irq_enable(void)
{
	local_irq_disable();
	reset_variables();
	set_irq_and_get_time();
	local_irq_enable();
}

static void irq_disable(void)
{
	arch_irq_disable();
	reset_variables();
	ts.tv_sec = 0;
	ts.tv_usec = 0;
}

static int enable_diff_output;
static int enable_sched_latency;
static int enable_tracing = 1;

static void set_normalized_timeval(struct timeval *tv, time_t sec, s64 usec)
{
	while (usec >= USEC_PER_SEC) {
		/*
		 * The following asm() prevents the compiler from
		 * optimising this loop into a modulo operation. See
		 * also __iter_div_u64_rem() in include/linux/time.h
		 */
		asm("" : "+rm"(usec));
		usec -= USEC_PER_SEC;
		++sec;
	}
	while (usec < 0) {
		asm("" : "+rm"(usec));
		usec += USEC_PER_SEC;
		--sec;
	}
	tv->tv_sec = sec;
	tv->tv_usec = usec;
}

static inline struct timeval timeval_sub(struct timeval lhs,
						struct timeval rhs)
{
	struct timeval tv_delta;
	set_normalized_timeval(&tv_delta, lhs.tv_sec - rhs.tv_sec,
				lhs.tv_usec - rhs.tv_usec);
	return tv_delta;
}

static irqreturn_t irq_handler(int irq, void *dev_id)
{
	/* Handle interrupt */
	local_irq_disable();
	/* Get the Handle time */
	do_gettimeofday(&th);
	arch_irq_handler();
	arch_irq_disable();

	/*
	 * Interrupt time = The time we enabled the interrupt + its period
	 */
	ti = ts;
	ti.tv_usec += period;

	/* Calculate the latency: Handle time - Interrupt time */
	diff = timeval_sub(th, ti);
	if (diff.tv_usec > max)
		max = diff.tv_usec;
	else if (diff.tv_usec < min)
		min = diff.tv_usec;
	sum += diff.tv_usec;

	total++;
	/* Wakeup the user-space application */
	irq_on = 1;
	do_gettimeofday(&te);
	wake_up_interruptible(&wq);

	/* We can not sleep in un-thread interrupt handler for the  */
#ifndef UNTHREAD_INTERRUPT
	local_irq_enable();
	/* Interrupt interval: the real interval should be interval + period */
	msleep(interval / 1000);
	local_irq_disable();
#endif
	/* Set the interrupt and get the time of setting */
	if (enable_tracing)
		set_irq_and_get_time();
	local_irq_enable();

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

	ret = init_arch_irq();
	if (ret)
		return -EFAULT;

	local_irq_disable();
	reset_variables();
	set_irq_and_get_time();
	local_irq_enable();

	ret = setup_irq(IRQ_NUM, &irq_action);
	if (ret)
		return -EFAULT;
	return 0;
}

static void exit_irq(void)
{
	exit_arch_irq();
	remove_irq(IRQ_NUM, &irq_action);
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
		pr_alert("Registering char device failed with %d\n",
		       Major);
		return Major;
	}

	init_waitqueue_head(&wq);

	pr_info("I was assigned major number %d. To talk to\n", Major);
	pr_info("the driver, create a dev file with\n");
	pr_info("'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	pr_info("Try various minor numbers. Try to cat and echo to\n");
	pr_info("the device file.\n");
	pr_info("Remove the device file and module when done.\n");
	pr_info("==============================================\nUsage:\n"
		"o Disable it(it is enabled by default)\n"
		"$ echo 0 > /dev/interrupt_latency\n"
		"o Enable it\n"
		"$ echo 1 > /dev/interrupt_latency\n"
		"o Enable verbose diff output \n"
		"$ echo 2 > /dev/interrupt_latency\n"
		"o Disable verbose diff output \n"
		"$ echo 3 > /dev/interrupt_latency\n"
		"o Enable primary output \n"
		"$ echo 4 > /dev/interrupt_latency\n"
		"o Disable primary output \n"
		"$ echo 5 > /dev/interrupt_latency\n"
		"o Change the priority \n"
		"$ echo 0 > /dev/interrupt_latency\n"
		"$ ps -ef | grep interrupt // get the pid.\n"
		"$ chrt -p 98 <pid>\n"
		"$ echo 1 > /dev/interrupt_latency\n"
		"o Set the interval of the interrupt(10 <= interval <= 100000000)"
		"$ echo 1000 > /dev/interrupt_latency\n");

	ret = init_irq();
	if (ret) {
		pr_err("Failed to setup irq: %d\n", IRQ_NUM);
		return ret;
	} else
		pr_info("Setup irq: %d\n", IRQ_NUM);

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
	u32 avg;

	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	avg = sum / total;
	sprintf(msg, "Samples: %-8d Interval: %-8d Cur: %-8d Avg: %-8d Min: %-8d Max: %-8d\n",
			total, interval, (int)diff.tv_usec, avg, min, max);

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
	int bytes_read;

	bytes_read = 0;

	if (!enable_tracing) {
		pr_alert("Please enable the interrupt at first:\n"
			"$ echo 1 > /dev/interrupt_latency");
		goto out;
	}

	if (wait_event_interruptible(wq, irq_on == 1)) {
		pr_info("wait_event_interruptible returned ERESTARTSYS\n");
		return -ERESTARTSYS;
	}


	if (enable_diff_output) {
		bytes_read = snprintf(buffer, length, "%d\n", (int)diff.tv_usec);
		goto out;
	}

	if (enable_sched_latency) {
		bytes_read = snprintf(buffer, length, "%d %d,%d %d,%d %d,%d\n",
				total, (int)ti.tv_sec, (int)ti.tv_usec,
				(int)th.tv_sec, (int)th.tv_usec,
				(int)te.tv_sec, (int)te.tv_usec);
		goto out;
	}

	/*
	 * If we're at the end of the message,
	 * return 0 signifying end of file
	 */
	if (*msg_Ptr == 0)
		goto out;

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
out:
	irq_on = 0;
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
			break;
		case 4:
			enable_sched_latency = 1;
			break;
		case 5:
			enable_sched_latency = 0;
			break;
		default:
			irq_disable();
			/* by default, we set the user input as hz\n */
			if (cmd > 100000000 || cmd < 10) {
				pr_info("%s: interval should be >= 10 & <= 100000000\n",
						__func__);
				interval = 1000;
			} else
				interval = cmd;
			pr_info("interval is set to %d\n", interval);
			irq_enable();
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

/*
 *  sched_clock() Latency Driver
 *
 *  This driver is designed for testing the duration need to call sched_clock()
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>	/* for put_user */
#include <linux/sched.h>	/* for sched_clock */

/*
 *  Prototypes - this would normally go in a .h file
 */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define DIFF
#define SUCCESS 0
#define FAILURE 1
/* Dev name as it appears in /proc/devices   */
#define DEVICE_NAME "sched_clock_latency"
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

static const struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};

/*
 * This function is called when the module is loaded
 */
static __init int sched_clock_latency_init(void)
{
	Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
		pr_alert("Registering char device failed with %d\n",
		       Major);
		return Major;
	}

	pr_info("I was assigned major number %d. To talk to\n", Major);
	pr_info("the driver, create a dev file with\n");
	pr_info("'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	pr_info("Try various minor numbers. Try to cat and echo to\n");
	pr_info("the device file.\n");
	pr_info("Remove the device file and module when done.\n");

	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
static __exit void sched_clock_latency_exit(void)
{
	/*
	 * Unregister the device
	 */
	unregister_chrdev(Major, DEVICE_NAME);
}

/*
 * Methods
 */

static u64 loop, sum;
static unsigned int min = UINT_MAX, max = 0, avg = 0, delta;
static int func_no, verbose;

enum {
	CLOCK_START = -1,
	SCHED_CLOCK,
	GETNSTIMEOFDAY,
	CLOCK_END,
};

#define CLOCK_NUM (CLOCK_END - CLOCK_START)

static inline int valid_func(int func_no)
{
	return func_no > CLOCK_START && func_no < CLOCK_END;
}

static inline int valid_loop(u64 loop)
{
	return loop >= 2 && loop <= 10000000;
}

static char func_name[CLOCK_NUM][50] = {
	"sched_clock",
	"getnstimeofday",
	"unknown",
};

static inline unsigned int get_clock_diff(int func_no)
{
	unsigned int diff = 0;
	u64 t0, t1;
	struct timespec tv0, tv1, __maybe_unused diff_tv;

	switch (func_no) {
	case GETNSTIMEOFDAY:
		getnstimeofday(&tv0);
		getnstimeofday(&tv1);
#ifdef DIFF
		diff_tv = timespec_sub(tv1, tv0);
		diff = timespec_to_ns(&diff_tv);
#endif
		break;
	case SCHED_CLOCK:
		t0 = sched_clock();
		t1 = sched_clock();
#ifdef DIFF
		diff = t1 - t0;
#endif
		break;
	default:
		pr_alert("Sorry, no such function.\n");
		break;
	}
	return diff;
}

static void calculate_latency(int func_no)
{
	u64 t0, t1;
	int i, __maybe_unused diff;

	/* Init */
	max = 0;
	min = UINT_MAX;
	sum = 0;

	t0 = sched_clock();
	for (i = 0; i < loop; i++) {
#ifndef DIFF
		(void)get_clock_diff(func_no);
#else
		diff = get_clock_diff(func_no);
		if (diff > max)
			max = diff;
		else if (diff < min)
			min = diff;
		sum += diff;
		if (verbose)
			pr_info("%d\n", diff);
#endif
	}
	t1 = sched_clock();

	/* delta is the average time spending on calling the clock func */
	delta = (t1 - t0) / (2 * loop);
	/* avg is the average of the latency */
	avg = sum / loop;
}

/*
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	if (loop == 0)
		sprintf(msg,
			"Please set the loop and clock function via /dev/%s,\n"
			"i.e. $ echo <loop> <clock_func_num> <verbose> > /dev/%s\n"
			"<loop>: any unsigned number >= 2, <= 10000000\n"
			"<clock_func_num>:\n"
			"\t0: sched_clock\n"
			"\t1: getnstimeofday\n"
			"<verbose>: 0 or 1\n"
			"\t0: means not verbose output\n"
			"\t1: verbose output, please use dmesg to get it.\n", DEVICE_NAME, DEVICE_NAME);
	else
		sprintf(msg,
			"func: %s, loop: %lld, sum: %lld,"
			" avg: %d, max: %d, min: %d delta: %d (ns)\n",
			func_name[func_no], loop, sum, avg, max, min, delta);

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
	if (sscanf(buff, "%lld %d %d", &loop, &func_no, &verbose) >= 1) {
		if (!valid_loop(loop))
			pr_err("Invalid loop no: %lld, "
			       "loop must >= 2, <= 10000000\n", loop);
		else if (!valid_func(func_no))
			pr_err("Unknown clock function\n");
		else {
			pr_alert("Please wait, is calcaulating...\n");
			calculate_latency(func_no);
			pr_alert("Calculation is finished, please \n");
			pr_alert("get the statistic result via cat /dev/%s \n", DEVICE_NAME);
		}
	}
	return strlen(buff);
}

module_init(sched_clock_latency_init);
module_exit(sched_clock_latency_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wu Zhangjin <wuzhangjin@gmail.com>");
MODULE_DESCRIPTION("sched_clock latency driver");

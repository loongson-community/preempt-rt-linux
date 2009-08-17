/*
 * EC(Embedded Controller) KB3310B SCI EVENT management driver on Linux
 * Author	: liujl <liujl@lemote.com>
 * Date		: 2008-10-22
 *
 * NOTE : Until now, I have no idea for setting the interrupt to edge sensitive
 *	mode, amd's help should be needed for handling this problem
 *	So, I assume that the interrupt width is 120us
 */

#include <linux/interrupt.h>
#include <linux/module.h>
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
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/spinlock.h>

#include <asm/delay.h>

#include "ec.h"
#include "ec_misc_fn.h"

/* inode information */
#define	EC_SCI_MINOR_DEV	MISC_DYNAMIC_MINOR
#define	EC_SCI_DEV			"sci"
#define	SCI_IRQ_NUM			0x0A
#define	CS5536_GPIO_SIZE	256

/* ec delay time 500us for register and status access */
/* unit : us */
#define	EC_REG_DELAY		300

struct ec_sci_reg {
	u32 addr;
	u8 val;
};
struct ec_sci_reg ecreg;

struct sci_device {
	/* the sci number get from ec */
	unsigned char sci_number;

	/* sci count */
	unsigned char sci_parameter;

	/* irq relative */
	unsigned char irq;
	unsigned char irq_data;

	/* device name */
	unsigned char name[10];

	/* gpio base registers and length */
	unsigned long gpio_base;
	unsigned long gpio_size;

	/* lock & wait_queue */
	wait_queue_head_t wq;
	spinlock_t lock;

	/* storage initial value of sci status register
	 * sci_init_value[0] as brightness
	 * sci_init_value[1] as volume
	 * sci_init_value[2] as ac
	 */
	unsigned char sci_init_value[3];
};
struct sci_device *sci_device;

#ifdef	CONFIG_PROC_FS
static ssize_t sci_proc_read(struct file *file, char *buf, size_t len,
			     loff_t *ppos);
static ssize_t sci_proc_write(struct file *file, const char *buf, size_t len,
			      loff_t *ppos);
static unsigned int sci_poll(struct file *fp, poll_table *wait);
static struct proc_dir_entry *sci_proc_entry;
static const struct file_operations sci_proc_fops = {
	.read = sci_proc_read,
	.poll = sci_poll,
	.write = sci_proc_write,
};

#define	SCI_ACTION_COUNT	15
#define	SCI_ACTION_WIDTH	14
char sci_action[SCI_ACTION_COUNT][SCI_ACTION_WIDTH] = {
	"DISPLAY : LCD",
	"DISPLAY : CRT",
	"DISPLAY : ALL",
	"DISPLAY : CHG",
	"AUDIO : CHG",
	"MACHINE : OFF",
	"MACHINE : RES",
	"CAMERA : ON",
	"CAMERA : OFF",
	"LCDLED : ON",
	"LCDLED : OFF",
	"LCDBL : ON",
	"LCDBL : OFF",
	"NONE",
	"NONE"
};

static enum {
	CMD_DISPLAY_LCD = 0,
	CMD_DISPLAY_CRT,
	CMD_DISPLAY_ALL,
	CMD_DISPLAY_CHANGE_BRIGHTNESS,
	CMD_AUDIO_CHANGE_VOLUME,
	CMD_MACHINE_OFF,
	CMD_MACHINE_RESET,
	CMD_CAMERA_ON,
	CMD_CAMERA_OFF,
	CMD_LCDLED_PWRON,
	CMD_LCDLED_PWROFF,
	CMD_LCDBL_ON,
	CMD_LCDBL_OFF,
	CMD_NONE
} sci_cmd;

#endif

static void sci_display_lcd(void)
{
	unsigned char value;

	outb(0x21, 0x3c4);
	value = inb(0x3c5);
	value |= (1 << 7);
	outb(0x21, 0x3c4);
	outb(value, 0x3c5);

	outb(0x31, 0x3c4);
	value = inb(0x3c5);
	value = (value & 0xf8) | 0x01;
	outb(0x31, 0x3c4);
	outb(value, 0x3c5);

	return;
}

static void sci_display_crt(void)
{
	unsigned char value;

	outb(0x21, 0x3c4);
	value = inb(0x3c5);
	value &= ~(1 << 7);
	outb(0x21, 0x3c4);
	outb(value, 0x3c5);

	outb(0x31, 0x3c4);
	value = inb(0x3c5);
	value = (value & 0xf8) | 0x02;
	outb(0x31, 0x3c4);
	outb(value, 0x3c5);

	return;
}

static void sci_display_all(void)
{
	unsigned char value;

	outb(0x21, 0x3c4);
	value = inb(0x3c5);
	value &= ~(1 << 7);
	outb(0x21, 0x3c4);
	outb(value, 0x3c5);

	outb(0x31, 0x3c4);
	value = inb(0x3c5);
	value = (value & 0xf8) | 0x03;
	outb(0x31, 0x3c4);
	outb(value, 0x3c5);

	return;
}

static void sci_lcd_op(unsigned char flag)
{
	unsigned char value;

	/* default display crt */
	outb(0x21, 0x3c4);
	value = inb(0x3c5);
	value &= ~(1 << 7);
	outb(0x21, 0x3c4);
	outb(value, 0x3c5);

	if (flag == CMD_LCDLED_PWRON) {
		/* open lcd output */
		outb(0x31, 0x3c4);
		value = inb(0x3c5);
		value = (value & 0xf8) | 0x03;
		outb(0x31, 0x3c4);
		outb(value, 0x3c5);
	} else if (flag == CMD_LCDLED_PWROFF) {
		/* close lcd output */
		outb(0x31, 0x3c4);
		value = inb(0x3c5);
		value = (value & 0xf8) | 0x02;
		outb(0x31, 0x3c4);
		outb(value, 0x3c5);
	} else if (flag == CMD_LCDBL_ON)
		/* LCD backlight on */
		ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_ON);
	else if (flag == CMD_LCDBL_OFF)
		/* LCD backlight off */
		ec_write(REG_BACKLIGHT_CTRL, BIT_BACKLIGHT_OFF);

	return;
}

static void sci_display_change_brightness(void)
{
	ec_write(REG_DISPLAY_BRIGHTNESS, FLAG_DISPLAY_BRIGHTNESS_LEVEL_4);
	return;
}

static void sci_audio_change_volume(void)
{
	ec_write(REG_AUDIO_VOLUME, FLAG_AUDIO_VOLUME_LEVEL_5);
	return;
}

static void sci_camera_on_off(void)
{
	unsigned char val;
	val = ec_read(REG_CAMERA_CONTROL);
	ec_write(REG_CAMERA_CONTROL, val | (1 << 1));
	return;
}

static void sci_machine_off(void)
{
#ifdef CONFIG_64BIT
	/* cpu-gpio0 output low */
	*((unsigned int *)(0xffffffffbfe0011c)) &= ~0x00000001;
	/* cpu-gpio0 as output */
	*((unsigned int *)(0xffffffffbfe00120)) &= ~0x00000001;
#else
	/* cpu-gpio0 output low */
	*((unsigned int *)(0xbfe0011c)) &= ~0x00000001;
	/* cpu-gpio0 as output */
	*((unsigned int *)(0xbfe00120)) &= ~0x00000001;
#endif				/* end ifdef CONFIG_64BIT */
	return;
}

static void sci_machine_reset(void)
{
	ec_write(REG_RESET, BIT_RESET_ON);
	return;
}

/* static const char driver_version[] = "1.0"; */
static const char driver_version[] = VERSION;

#ifdef CONFIG_PROC_FS
#define	PROC_BUF_SIZE	128
unsigned char proc_buf[PROC_BUF_SIZE];

/*
 * sci_proc_read :
 *	read information from sci device and suppied to upper layer
 *	The format is as following :
 *	driver_version		1.0
 *	DISPLAY BRIGHTNESS INCREASE
 *	DISPLAY BRIGHTNESS DECREASE
 *	AUDIO VOLUME INCREASE
 *	AUDIO VOLUME DECREASE
 *	MUTE		0x00 close, 0x01 open
 *	WLAN		0x00 close, 0x01 open
 *	LID			0x00 close, 0x01 open
 *	DISPLAY TOGGLE
 *	BLACK SCREEN
 *	SLEEP
 *	OVER TEMPERATURE
 *	CRT DETECT
 *	CAMERA		0x00 close, 0x01 open
 *	USB OC2
 *	USB OC0
 *	BAT IN
 *	AC IN
 *	INIT CAP
 *	CHARGE MODE
 *	STOP CHARGE
 *	BAT LOW
 *	BAT FULL
 */
static ssize_t sci_proc_read(struct file *file, char *buf, size_t len,
			     loff_t *ppos)
{
	int ret = 0;
	int count = 0;
	DECLARE_WAITQUEUE(wait, current);

	PRINTK_DBG("0 irq_data %d\n", sci_device->irq_data);

	if (sci_device->irq_data == 0) {
		add_wait_queue(&(sci_device->wq), &wait);

		while (!sci_device->irq_data) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
		}
		remove_wait_queue(&(sci_device->wq), &wait);
	}

	PRINTK_DBG("1 irq_data %d\n", sci_device->irq_data);
	__set_current_state(TASK_RUNNING);

 	ret = sprintf(proc_buf, "0x%x\t%d\n", sci_device->sci_number, sci_device->sci_parameter);

	count = strlen(proc_buf);
	sci_device->irq_data = 0;

	if (len < count)
		return -ENOMEM;
	if (PROC_BUF_SIZE < count)
		return -ENOMEM;

	if (copy_to_user(buf, proc_buf, count))
		return -EFAULT;

	return count;
}

/*
 * sci_proc_write :
 *	get the upper layer's action and take action.
 */
static ssize_t sci_proc_write(struct file *file, const char *buf, size_t len,
			      loff_t *ppos)
{
	int i;

	if (len > PROC_BUF_SIZE)
		return -ENOMEM;
	if (copy_from_user(proc_buf, buf, len))
		return -EFAULT;
	proc_buf[len] = '\0';

	PRINTK_DBG("proc_buf : %s\n", proc_buf);
	for (i = 0; i < SCI_ACTION_COUNT; i++) {
		if (strncmp(proc_buf, sci_action[i], strlen(sci_action[i])) ==
		    0) {
			sci_cmd = i;
			break;
		}
	}
	if (i == SCI_ACTION_COUNT)
		sci_cmd = CMD_NONE;
	PRINTK_DBG("sci_cmd: %d\n", sci_cmd);
	switch (sci_cmd) {
	case CMD_DISPLAY_LCD:
		sci_display_lcd();
		PRINTK_DBG(KERN_DEBUG "CMD_DISPLAY_LCD");
		break;
	case CMD_DISPLAY_CRT:
		sci_display_crt();
		PRINTK_DBG(KERN_DEBUG "CMD_DISPLAY_CRT");
		break;
	case CMD_DISPLAY_ALL:
		sci_display_all();
		PRINTK_DBG(KERN_DEBUG "CMD_DISPLAY_ALL");
		break;
	case CMD_DISPLAY_CHANGE_BRIGHTNESS:
		sci_display_change_brightness();
		PRINTK_DBG(KERN_DEBUG "CMD_DISPLAY_CHANGE_BRIGHTNESS");
		break;
	case CMD_AUDIO_CHANGE_VOLUME:
		sci_audio_change_volume();
		PRINTK_DBG(KERN_DEBUG "CMD_AUDIO_CHANGE_VOLUME");
		break;
	case CMD_MACHINE_OFF:
		sci_machine_off();
		PRINTK_DBG(KERN_DEBUG "CMD_MACHINE_OFF");
		break;
	case CMD_MACHINE_RESET:
		sci_machine_reset();
		PRINTK_DBG(KERN_DEBUG "CMD_MACHINE_RESET");
		break;
	case CMD_CAMERA_ON:
		sci_camera_on_off();
		PRINTK_DBG(KERN_DEBUG "CMD_CAMERA_ON");
		break;
	case CMD_CAMERA_OFF:
		sci_camera_on_off();
		PRINTK_DBG(KERN_DEBUG "CMD_CAMERA_OFF");
		break;
	case CMD_LCDLED_PWRON:
		sci_lcd_op(CMD_LCDLED_PWRON);
		PRINTK_DBG(KERN_DEBUG "CMD_LCDLED_PWRON");
		break;
	case CMD_LCDLED_PWROFF:
		sci_lcd_op(CMD_LCDLED_PWROFF);
		PRINTK_DBG(KERN_DEBUG "CMD_LCDLED_PWROFF");
		break;
	case CMD_LCDBL_ON:
		sci_lcd_op(CMD_LCDBL_ON);
		PRINTK_DBG(KERN_DEBUG "CMD_LCDBL_ON");
		break;
	case CMD_LCDBL_OFF:
		sci_lcd_op(CMD_LCDBL_OFF);
		PRINTK_DBG(KERN_DEBUG "CMD_LCDBL_OFF");
		break;
	default:
		printk(KERN_ERR "EC SCI : Not supported cmd.\n");
		return -EINVAL;
	}

	return len;
}
#endif

/*
 * sci_query_event_num :
 *	using query command to ec to get the proper event number
 */
static int sci_query_event_num(void)
{
	int ret = 0;

	ret = ec_query_seq(CMD_GET_EVENT_NUM);
	return ret;
}

/*
 * sci_get_event_num :
 *	get sci event number from ec
 *	NOTE : this routine must follow the sci_query_event_num
 *	function in the interrupt
 */
int sci_get_event_num(void)
{
	int timeout = 100;
	unsigned char value;
	unsigned char status;

	udelay(EC_REG_DELAY);
	status = inb(EC_STS_PORT);
	udelay(EC_REG_DELAY);
	while (timeout--) {
		if (!(status & (1 << 0))) {
			status = inb(EC_STS_PORT);
			udelay(EC_REG_DELAY);
			continue;
		}
		break;
	}
	if (timeout <= 0) {
		PRINTK_DBG("fixup sci : get event number timeout.\n");
		return -EINVAL;
	}
	value = inb(EC_DAT_PORT);
	udelay(EC_REG_DELAY);

	return value;
}
EXPORT_SYMBOL(sci_get_event_num);

/*
 * sci_parse_num :
 *	parse the event number routine, and store all the information
 *	to the sci_num_array[] for upper layer using
 */
static int sci_parse_num(struct sci_device *sci_device)
{
	unsigned char val;

	switch (sci_device->sci_number) {
	case SCI_EVENT_NUM_LID:
		sci_device->sci_parameter = ec_read(REG_LID_DETECT);
		break;
	case SCI_EVENT_NUM_DISPLAY_TOGGLE:
		sci_device->sci_parameter = 0x01;
		break;
	case SCI_EVENT_NUM_SLEEP:
		sci_device->sci_parameter = 0x01;
		break;
	case SCI_EVENT_NUM_OVERTEMP:
		sci_device->sci_parameter = (ec_read(REG_BAT_CHARGE_STATUS) & BIT_BAT_CHARGE_STATUS_OVERTEMP) >> 2;
		break;
	case SCI_EVENT_NUM_CRT_DETECT:
		sci_device->sci_parameter = ec_read(REG_CRT_DETECT);
		break;
	case SCI_EVENT_NUM_CAMERA:
		sci_device->sci_parameter = ec_read(REG_CAMERA_STATUS);
		break;
	case SCI_EVENT_NUM_USB_OC2:
		sci_device->sci_parameter = ec_read(REG_USB2_FLAG);
		break;
	case SCI_EVENT_NUM_USB_OC0:
		sci_device->sci_parameter = ec_read(REG_USB0_FLAG);
		break;
	case SCI_EVENT_NUM_AC_BAT:
		val = ec_read(REG_BAT_POWER) & BIT_BAT_POWER_ACIN;
 		sci_device->sci_parameter= 3;
 		if (sci_device->sci_init_value[2] == 0 && val == 1)
 			sci_device->sci_parameter = 1;
 		if (sci_device->sci_init_value[2] == 1 && val == 0)
 			sci_device->sci_parameter = 0;
 		sci_device->sci_init_value[2] = val;
		break;
	case SCI_EVENT_NUM_DISPLAY_BRIGHTNESS:
		sci_device->sci_parameter = ec_read(REG_DISPLAY_BRIGHTNESS);
		break;
	case SCI_EVENT_NUM_AUDIO_VOLUME:
 		val = ec_read(REG_AUDIO_VOLUME);
		sci_device->sci_parameter = val;
		break;
	case SCI_EVENT_NUM_WLAN:
		sci_device->sci_parameter = ec_read(REG_WLAN_STATUS);
		break;
	case SCI_EVENT_NUM_AUDIO_MUTE:
		sci_device->sci_parameter = ec_read(REG_AUDIO_MUTE);
		break;
	case SCI_EVENT_NUM_BLACK_SCREEN:
		sci_device->sci_parameter = ec_read(REG_DISPLAY_LCD);
		break;

	default:
		PRINTK_DBG(KERN_ERR "EC SCI : not supported SCI NUMBER.\n");
		return -EINVAL;
		break;
	}

	return 0;
}

/*
 * sci_int_routine : sci main interrupt routine
 * we will do the query and get event number together
 * so the interrupt routine should be longer than 120us
 * now at least 3ms elpase for it.
 */
static irqreturn_t sci_int_routine(int irq, void *dev_id)
{
	int ret;

	if (sci_device->irq != irq) {
		PRINTK_DBG(KERN_ERR "EC SCI :spurious irq.\n");
		return IRQ_NONE;
	}

	/* query the event number */
	ret = sci_query_event_num();
	if (ret < 0) {
		PRINTK_DBG("ret 1: %d\n", ret);
		return IRQ_NONE;
	}

	ret = sci_get_event_num();
	if (ret < 0) {
		PRINTK_DBG("ret 2: %d\n", ret);
		return IRQ_NONE;
	}
	sci_device->sci_number = ret;

	PRINTK_DBG(KERN_INFO "sci_number :0x%x\n", sci_device->sci_number);

	/* parse the event number and wake the queue */
	if ((sci_device->sci_number != 0x00)
	    && (sci_device->sci_number != 0xff)) {
		ret = sci_parse_num(sci_device);
		PRINTK_DBG("ret 3: %d\n", ret);
		if (!ret)
			sci_device->irq_data = 1;
		else
			sci_device->irq_data = 0;

		wake_up_interruptible(&(sci_device->wq));
		PRINTK_DBG("interrupitble\n");
	}

	return IRQ_HANDLED;
}

static int sci_open(struct inode *inode, struct file *filp)
{
	PRINTK_DBG(KERN_INFO "SCI : open ok.\n");
	return 0;
}

static int sci_release(struct inode *inode, struct file *filp)
{
	PRINTK_DBG(KERN_INFO "SCI : close ok.\n");
	return 0;
}

/*
 * sci_poll : poll routine for upper layer using
 */
static unsigned int sci_poll(struct file *fp, poll_table * wait)
{
	int mask = 0;

	/* printk("current task %p\n", current); */
	poll_wait(fp, &(sci_device->wq), wait);
	if (sci_device->irq_data) {
		/* printk("current task 1 %p\n", current); */
		mask = POLLIN | POLLRDNORM;
	}

	return mask;
}

static int sci_ioctl(struct inode *inode, struct file *filp, unsigned long cmd,
		     unsigned long arg)
{
	void __user *ptr = (void __user *)arg;
	int ret = 0;

	switch (cmd) {
/*     case    IOCTL_GET_INIT_STATE : */
	case 1:
		ret = copy_from_user(&ecreg, ptr, sizeof(struct ec_sci_reg));
		if (ret) {
			printk(KERN_ERR "read from user error.\n");
			return -EFAULT;
		}
		if (ecreg.addr < 0xf400 || ecreg.addr > 0xffff)
			return -EINVAL;
		ecreg.val = ec_read(ecreg.addr);
		ret = copy_to_user(ptr, &ecreg, sizeof(struct ec_sci_reg));
		if (ret) {
			printk(KERN_ERR "reg read : copy to user error.\n");
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	return 0;
}

static long sci_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return sci_ioctl(file->f_dentry->d_inode, file, cmd, arg);
}

static const struct file_operations sci_fops = {
#ifdef	CONFIG_64BIT
	.compat_ioctl = sci_compat_ioctl,
#else
	.ioclt = sci_ioctl,
#endif
	.open = sci_open,
	.poll = sci_poll,
	.release = sci_release,
};

static struct miscdevice sci_dev = {
	.minor = EC_SCI_MINOR_DEV,
	.name = EC_SCI_DEV,
	.fops = &sci_fops
};

static struct pci_device_id sci_pci_tbl[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_CS5536_ISA)},
	{}
};

MODULE_DEVICE_TABLE(pci, sci_pci_tbl);

/*
 * sci_low_level_init :
 *	config and init some msr and gpio register properly.
 */
static int sci_low_level_init(struct sci_device *scidev)
{
	u32 hi, lo;
	u32 gpio_base = scidev->gpio_base;
	int ret = 0;
	unsigned long flags;

	/* filter the former kb3310 interrupt for security */
	ret = sci_query_event_num();
	if (ret) {
		PRINTK_DBG(KERN_ERR
			   "sci low level init query event num failed.\n");
		return ret;
	}

	/* for filtering next number interrupt */
	udelay(10000);

	/* set gpio native registers and msrs for GPIO27 SCI EVENT PIN
	 * gpio :
	 *      input, pull-up, no-invert, event-count and value 0,
	 *      no-filter, no edge mode
	 *      gpio27 map to Virtual gpio0
	 * msr :
	 *      no primary and lpc
	 *      Unrestricted Z input to IG10 from Virtual gpio 0.
	 */
	local_irq_save(flags);
	_rdmsr(0x80000024, &hi, &lo);
	lo &= ~(1 << 10);
	_wrmsr(0x80000024, hi, lo);
	_rdmsr(0x80000025, &hi, &lo);
	lo &= ~(1 << 10);
	_wrmsr(0x80000025, hi, lo);
	_rdmsr(0x80000023, &hi, &lo);
	lo |= (0x0a << 0);
	_wrmsr(0x80000023, hi, lo);
	local_irq_restore(flags);

	/* set gpio27 as sci interrupt :
	 * input, pull-up, no-fliter, no-negedge, invert
	 * the sci event is just about 120us
	 */
	asm(".set noreorder\n");
	/*  input enable */
	outl(0x00000800, (gpio_base | 0xA0));
	/*  revert the input */
	outl(0x00000800, (gpio_base | 0xA4));
	/*  event-int enable */
	outl(0x00000800, (gpio_base | 0xB8));
	asm(".set reorder\n");

	return 0;
}

/*
 * sci_pci_init :
 *	pci init routine
 */
static int __devinit sci_pci_init(struct pci_dev *pdev,
				  const struct pci_device_id *ent)
{
	u32 gpio_base;
	int ret = -EIO;

	/* init the sci device */
	sci_device = kmalloc(sizeof(struct sci_device), GFP_KERNEL);
	if (sci_device == NULL) {
		PRINTK_DBG(KERN_ERR
			   "EC SCI : get memory for sci_device failed.\n");
		return -ENOMEM;
	}
	init_waitqueue_head(&(sci_device->wq));
	spin_lock_init(&sci_device->lock);
	sci_device->irq = SCI_IRQ_NUM;
	sci_device->irq_data = 0x00;
	sci_device->sci_number = 0x00;
	strcpy(sci_device->name, EC_SCI_DEV);

	sci_device->sci_init_value[0] = ec_read(REG_DISPLAY_BRIGHTNESS);
	sci_device->sci_init_value[1] = ec_read(REG_AUDIO_VOLUME);
 	sci_device->sci_init_value[2] = ec_read(REG_BAT_POWER) & BIT_BAT_POWER_ACIN;
	sci_device->sci_parameter = 0x00;

	/* enable pci device and get the GPIO resources */
	ret = pci_enable_device(pdev);
	if (ret) {
		PRINTK_DBG(KERN_ERR "EC SCI : enable pci device failed.\n");
		ret = -ENODEV;
		goto out_pdev;
	}

	gpio_base = 0x0000;
	gpio_base = pci_resource_start(pdev, 1);
	gpio_base &= ~0x0003;
	if (gpio_base == 0x0000) {
		PRINTK_DBG(KERN_ERR "EC SCI : get resource failed.\n");
		ret = -ENODEV;
		goto out_resource;
	}
	if (request_region(gpio_base, CS5536_GPIO_SIZE, EC_SCI_DEV) == NULL) {
		PRINTK_DBG(KERN_ERR
			   "EC SCI : base 0x%x, length 0x%x already in use.\n",
			   gpio_base, CS5536_GPIO_SIZE);
		goto out_resource;
	}
	sci_device->gpio_base = gpio_base;
	sci_device->gpio_size = CS5536_GPIO_SIZE;

	/* init the relative gpio and msrs */
	ret = sci_low_level_init(sci_device);
	if (ret < 0) {
		printk(KERN_ERR "EC SCI : low level init failed.\n");
		goto out_irq;
	}

	/* alloc the interrupt for sci not pci */
	ret =
	    request_irq(sci_device->irq, sci_int_routine, IRQF_SHARED,
			sci_device->name, sci_device);
	if (ret) {
		printk(KERN_ERR "EC SCI : request irq %d failed.\n",
		       sci_device->irq);
		ret = -EFAULT;
		goto out_irq;
	}

	/* register the misc device */
	ret = misc_register(&sci_dev);
	if (ret != 0) {
		printk(KERN_ERR "EC SCI : misc register failed.\n");
		ret = -EFAULT;
		goto out_misc;
	}

	ret = 0;
	PRINTK_DBG(KERN_INFO "sci probe ok...\n");
	goto out;

 out_misc:
	free_irq(sci_device->irq, sci_device);
 out_irq:
	release_region(sci_device->gpio_base, sci_device->gpio_size);
 out_resource:
	pci_disable_device(pdev);
 out_pdev:
	kfree(sci_device);
 out:
	return ret;
}

static void __devexit sci_pci_remove(struct pci_dev *pdev)
{
	misc_deregister(&sci_dev);
	free_irq(sci_device->irq, sci_device);
	release_region(sci_device->gpio_base, sci_device->gpio_size);
	pci_disable_device(pdev);
	kfree(sci_device);

	return;
}

static struct pci_driver sci_driver = {
	.name = EC_SCI_DEV,
	.id_table = sci_pci_tbl,
	.probe = sci_pci_init,
	.remove = __devexit_p(sci_pci_remove),
};

static int __init sci_init(void)
{
	int ret = 0;

#ifdef CONFIG_PROC_FS
	sci_proc_entry = NULL;
	sci_proc_entry = create_proc_entry(EC_SCI_DEV, S_IWUSR | S_IRUGO, NULL);
	if (sci_proc_entry == NULL) {
		printk(KERN_ERR "EC SCI : register /proc/sci failed.\n");
		return -EINVAL;
	}
	sci_proc_entry->proc_fops = &sci_proc_fops;
#endif

	ret = pci_register_driver(&sci_driver);
	if (ret) {
		printk(KERN_ERR "EC SCI : registrer pci driver error.\n");
#ifdef	CONFIG_PROC_FS
		remove_proc_entry(EC_SCI_DEV, NULL);
#endif
		return ret;
	}

	printk(KERN_INFO
	       "SCI event handler on KB3310B Embedded Controller init.\n");

	return ret;
}

static void __exit sci_exit(void)
{
#ifdef	CONFIG_PROC_FS
	remove_proc_entry(EC_SCI_DEV, NULL);
#endif
	pci_unregister_driver(&sci_driver);
	printk(KERN_INFO
	       "SCI event handler on KB3310B Embedded Controller exit.\n");

	return;
}

module_init(sci_init);
module_exit(sci_exit);

MODULE_AUTHOR("liujl <liujl@lemote.com>");
MODULE_DESCRIPTION("SCI Event Management for KB3310");
MODULE_LICENSE("GPL");

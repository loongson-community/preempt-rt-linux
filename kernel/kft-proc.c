#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kft.h>
#include <linux/irqnr.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/*
 * /proc/kft_data
 */

static int kft_data_open(struct inode *inode, struct file *file)
{
       return seq_open(file, &kft_data_op);
}
static const struct file_operations kft_data_operations = {
       .open           = kft_data_open,
       .read           = seq_read,
       .llseek         = seq_lseek,
       .release        = seq_release,
};

static int __init kft_data_init(void)
{
	proc_create("kft_data", 0, NULL, &kft_data_operations);
	return 0;
}
module_init(kft_data_init);

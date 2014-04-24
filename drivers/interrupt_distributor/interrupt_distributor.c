#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#define MAJOR_NUM          60
#define MODULE_NAME        "mydev"
#define BUF_LEN		   100

char *kbuf;
static ssize_t drv_read(struct file *filp, char *buf, size_t count, loff_t *ppos)
{
    printk("device read\n");
    copy_to_user(buf, kbuf, count);
    return count;
}

static ssize_t drv_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
    printk("device write ... ");
    copy_from_user(kbuf, buf, count);
    kbuf[count] = '\0';
    printk("kbuf=%s\n", kbuf);
    return count;
}

static int drv_open(struct inode *inode, struct file *filp)
{
    printk("device open\n");
    kbuf = kmalloc(sizeof(char)*BUF_LEN, GFP_KERNEL);
    return 0;
}

int drv_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    printk("device ioctl\n");
    return 0;
}

static int drv_release(struct inode *inode, struct file *filp)
{
    printk("device close\n");
    return 0;
}

struct file_operations drv_fops =
{
    .read     =      drv_read,
    .write    =      drv_write,
    .compat_ioctl    =      drv_ioctl,
    .open     =      drv_open,
    .release  =      drv_release,
};

static int _init(void) {
   printk("/****** Welcome to mydev_u0256070 ******/\n");
   return 0;
}
static void _exit(void) {
   unregister_chrdev(MAJOR_NUM, MODULE_NAME);
   printk("/****** BYE BYE ******/\n");
}
module_init(_init);
module_exit(_exit);

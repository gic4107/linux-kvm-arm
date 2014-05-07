#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/interrupt_distributor_uapi.h>

#define DESC_NUM 10
#define NAME_LEN 30
#define DEVICE_NAME "interrupt distributor"

#define hash_fn(dev_id) (unsigned int)((dev_id>>2)%DESC_NUM)

static int major_num;
struct list_head *irq_desc_hash;	// hash table array

struct irq_desc_t {
	unsigned int irq;
	irq_handler_t handler;
	unsigned long flags;
	char name[NAME_LEN];
	void *dev;
	struct list_head node;
};
int desc_count = 0;

static irqreturn_t distributor(int irq, void* dev_id)
{
	int hash_value = hash_fn((unsigned long)dev_id);
	struct list_head *hash_head = &irq_desc_hash[hash_value];
//	printk("distributor function ... ");
//	printk("dev_id=0x%llx, hash value=%d\n", dev_id, hash_fn((unsigned long)dev_id));
	struct irq_desc_t *irq_desc;
	list_for_each_entry(irq_desc, hash_head, node) {
		if(irq_desc->dev == dev_id)
			return irq_desc->handler(irq, dev_id); 
	}
}

int register_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{
	int hash_value = hash_fn((unsigned long)dev);
	printk("register_irq ... irq=%d, name=%s, hash=%d", irq, name, dev, hash_fn((unsigned long)dev));
	struct list_head *hash_head = &irq_desc_hash[hash_value];
	struct irq_desc_t *irq_desc = kmalloc(sizeof(struct irq_desc_t), GFP_KERNEL);		
	if(list_empty(hash_head)) {
		printk("irq_desc_hash[%d] null\n", hash_value);
		irq_desc->irq = irq;
		irq_desc->handler = handler;
		irq_desc->flags = flags;
		irq_desc->dev = dev;
		strcpy(irq_desc->name, name);	 
		list_add(&(irq_desc->node), hash_head);	
	}
	else {
		printk("irq_desc_hash[%d] not null\n", hash_value);	
		while(1) {
			struct list_head *iter = hash_head->next;
			if(list_is_last(iter, hash_head)) {
				irq_desc->irq = irq;
				irq_desc->handler = handler;
				irq_desc->flags = flags;
				irq_desc->dev = dev;
				strcpy(irq_desc->name, name);	 
				list_add(&(irq_desc->node), hash_head);	
			}
		}

	}
	desc_count++;
	return request_irq(irq, distributor, flags, name, dev);
}
EXPORT_SYMBOL(register_irq);

static long distributor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	printk("distributor_ioctl ... ");
	void __user *argp = (void __user*)arg;
	int r = 0;
	switch(cmd) {
	case SEND_IRQ_TO_GUEST:
		printk("CMD SEND_INT_TO_GUEST flip=%0xlx cmd=0x%lx\n", (void*)filp, cmd);	// filp different between different process
	//	r = -EFAULT;
		break;
	default:
		printk("unknowned ioctl cmd\n");
	//	r = -EFAULT;
		break;
	}

out:
	return r;
}

static const struct file_operations distributor_fops = {
	.owner          = THIS_MODULE,
//	.open           = distributor_open,
	.unlocked_ioctl = distributor_ioctl,
//	.release        = 
};

static int __init distributor_init(void) {
	printk("/****** Interrupt Distributor ******/ : ");
	int err;
	int i;
	major_num = register_chrdev(0, DEVICE_NAME, &distributor_fops);
	if(major_num < 0) {
		printk("interrupt distributor could not get major number\n");
		return major_num;
	}
	printk("major_num=%d\n", major_num);
	irq_desc_hash = kmalloc(DESC_NUM*sizeof(struct list_head), GFP_KERNEL);
	if(!irq_desc_hash) {
		printk("irq_desc_hash allocate fail\n");
		goto out;
	}	
	for(i=0; i<DESC_NUM; i++)
		INIT_LIST_HEAD(&(irq_desc_hash[i]));
out:
	return 0;
}
fs_initcall(distributor_init);

static void __exit distributor_exit(void) {
	printk("/****** Exit INterrupt Distributor ******/\n");
	unregister_chrdev(major_num, DEVICE_NAME);
}
module_exit(distributor_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Yu-Ju Huang gic4107@gmail.com");

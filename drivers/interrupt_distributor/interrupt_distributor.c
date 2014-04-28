#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>
#include <linux/list.h>

#define DESC_NUM 10
#define NAME_LEN 30

#define hash_fn(dev_id) (unsigned int)((dev_id>>2)%DESC_NUM)

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

static int __init distributor_init(void) {
	printk("/****** Interrupt Distributor ******/\n");
	int i;
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
}
module_exit(distributor_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Yu-Ju Huang gic4107@gmail.com");

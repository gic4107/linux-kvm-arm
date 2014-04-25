#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/interrupt.h>

#define DESC_NUM 10

struct irq_desc_t {
	unsigned int irq;
	irq_handler_t handler;
	unsigned long flags;
	char *name;
	void *dev;
} *irq_desc;
int desc_count = 0;

static irqreturn_t distributor(int irq, void* dev_id)
{
	printk("distributor function\n");
	return irq_desc[0].handler(irq, dev_id);	
}

int register_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{
	printk("register_irq ... irq=%d, name=%s\n", irq, name);
	printk("irq_desc=0x%llx\n", &irq_desc[0]);
	printk("irq_desc.irq=0x%llx\n", &(irq_desc[0].irq));
	printk("handler=0x%llx\n", (void*)handler);
	printk("desc_count=0x%llx\n", &desc_count);
	irq_desc[desc_count].irq = irq;
printk("1\n");
	irq_desc[desc_count].handler = handler;
printk("2\n");
	irq_desc[desc_count].flags = flags;
printk("3\n");
	irq_desc[desc_count].dev = dev;
printk("4\n");
	printk("irq_desc[0].name=0x%llx\n", &(irq_desc[0].name));
	printk("name=0x%llx\n", name);	
//	strcpy(irq_desc[desc_count++].name, name);
printk("request_irq distributor=0x%llx\n", (void*)distributor);	
	return request_irq(irq, distributor, flags, name, dev);
}
EXPORT_SYMBOL(register_irq);

static int __init distributor_init(void) {
	printk("/****** Interrupt Distributor ******/\n");
	irq_desc = kmalloc(DESC_NUM*sizeof(struct irq_desc_t), GFP_KERNEL);
	if(!irq_desc)
		printk("irq_desc kmalloc fail\n");
	return 0;
}
fs_initcall(distributor_init);

static void __exit distributor_exit(void) {
	printk("/****** BYE BYE ******/\n");
}
module_exit(distributor_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Yu-Ju Huang gic4107@gmail.com");

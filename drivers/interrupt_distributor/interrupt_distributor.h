#ifndef __INTERRUPT_DISTRIBUTOR_H
#define __INTERRUPT_DISTRIBUTOR_H
extern int register_irq(unsigned int,irq_handler_t,unsigned long,const char*,void*);

#include <linux/kvm.h>
#define IRQ_DST_IO      0xE0

/* user space IOCTL command */
#define SEND_IRQ_TO_GUEST _IOW(IRQ_DST_IO, 0x60, struct kvm_irq_level)

#endif

#ifndef __INTERRUPT_DISTRIBUTOR_H
#define __INTERRUPT_DISTRIBUTOR_H

#include <linux/kvm_host.h>
extern int register_irq(unsigned int,irq_handler_t,unsigned long,const char*,void*);

#endif

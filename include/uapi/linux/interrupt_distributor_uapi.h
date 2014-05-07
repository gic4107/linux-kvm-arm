#ifndef __INTERRUPT_DISTRIBUTOR_UAPI_H
#define __INTERRUPT_DISTRIBUTOR_UAPI_H

#include <linux/kvm.h>
#define IRQ_DST_IO      0xE0

/* user space IOCTL command */
#define SEND_IRQ_TO_GUEST _IOW(IRQ_DST_IO, 0x60, struct kvm_irq_level)

#endif

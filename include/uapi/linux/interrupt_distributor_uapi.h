#ifndef __INTERRUPT_DISTRIBUTOR_UAPI_H
#define __INTERRUPT_DISTRIBUTOR_UAPI_H

#include <linux/kvm.h>
#include <linux/types.h>
#define IRQ_DST_IO      0xE0

struct kvm_irq_target {
	int fd;
	union {
		__u32 irq;
		__s32 status;
	};
	__u32 level;
};

/* user space IOCTL command */
#define SEND_IRQ_TO_GUEST _IOW(IRQ_DST_IO, 0x60, struct kvm_irq_level)

#endif

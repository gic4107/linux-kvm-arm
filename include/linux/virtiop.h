#ifndef __VIRTIOP_H
#define __VIRTIOP_H

#include <linux/kvm_types.h>
#include <linux/interrupt.h>
#include "../../virt/kvm/iodev.h"
extern const struct kvm_io_device_ops virtiop_ops;
extern int register_virtiop_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device);
extern void deregister_virtiop_mmio_range(struct kvm_virtiop_bind_device *bind_device);
extern int register_irq(unsigned int,irq_handler_t,unsigned long,const char*,void*);

struct kvm_virtiop_device {                                                          
	u64         mmio_gpa;              
	int         mmio_len;
	u8			mmio_irq;
	struct kvm_io_device dev;
	struct kvm *kvm;
	hfn_t		vq_desc_hfn;				// set when guest write VIRTIO_MMIO_QUEUE_PFN
	hfn_t       vq_avail_hfn;
	hfn_t		vq_used_hfn;
	void*       host_dev;
};

#endif

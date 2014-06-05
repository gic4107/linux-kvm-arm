#ifndef __VIRTIOP_H
#define __VIRTIOP_H

#include <linux/kvm_types.h>
#include "../../virt/kvm/iodev.h"
extern const struct kvm_io_device_ops virtiop_ops;
extern int register_virtiop_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device);
extern void deregister_virtiop_mmio_range(struct kvm_virtiop_bind_device *bind_device);
struct kvm_virtiop_device {                                                          
	    u64         mmio_gpa;              
	    int         mmio_len;
	    struct kvm_io_device dev;
		struct kvm *kvm;
		pfn_t		vq_desc_pfn;				// set when guest write VIRTIO_MMIO_QUEUE_PFN
		pfn_t       vq_avail_pfn;
		pfn_t		vq_used_pfn;
};

#endif

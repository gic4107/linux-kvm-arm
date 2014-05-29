#ifndef __VIRTIOP_H
#define __VIRTIOP_H

#include <linux/kvm_types.h>
#include "../../virt/kvm/iodev.h"
extern const struct kvm_io_device_ops virtiop_ops;
extern int register_virtiop_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device);
extern void deregister_virtiop_mmio_range(struct kvm_virtiop_bind_device *bind_device);

#endif

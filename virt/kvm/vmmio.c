#include "iodev.h"

#include <linux/kvm_host.h>      
#include <linux/kvm.h>
#include <linux/kernel.h>
#include <linux/slab.h> 

#include <linux/virtiop.h>

struct virtiop_device {
	u64			addr;
	int			len;
	struct kvm_io_device	dev;
};

int kvm_assign_vmmio(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	printk("kvm_assign_vmmio: 0x%llx, %d\n", bind_device->mmio_gpa, bind_device->mmio_len);
	int ret;
	struct virtiop_device *d;
	
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if(!d) 
		return -ENOMEM;
	
	d->addr = bind_device->mmio_gpa;
	d->len  = bind_device->mmio_len;
	d->dev.ops = &virtiop_ops;
	
	ret = kvm_io_bus_register_dev(kvm, KVM_MMIO_BUS, d->addr, d->len, &d->dev);

	return ret;
}

#include "iodev.h"

#include <linux/kvm_host.h>      
#include <linux/kvm.h>
#include <linux/kernel.h>
#include <linux/slab.h> 

#include <linux/virtiop.h>

int kvm_assign_vmmio(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	printk("kvm_assign_vmmio: 0x%llx, %d\n", bind_device->mmio_gpa, bind_device->mmio_len);
	int ret;
	struct kvm_virtiop_device *d;
	
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if(!d) 
		return -ENOMEM;
	
	d->mmio_gpa = bind_device->mmio_gpa;
	d->mmio_len = bind_device->mmio_len;
	d->dev.ops = &virtiop_ops;
	
	ret = kvm_io_bus_register_dev(kvm, KVM_MMIO_BUS, d->mmio_gpa, d->mmio_len, &d->dev);
	if(ret < 0)
		goto out;

	ret = register_virtiop_mmio_range(kvm, d);
	if(ret < 0)
		goto out;

	return ret;

out:
	deregister_virtiop_mmio_range(bind_device);
	return ret;
}

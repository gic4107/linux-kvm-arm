#include "iodev.h"

#include <linux/kvm_host.h>      
#include <linux/kvm.h>
#include <linux/kernel.h>
#include <linux/slab.h> 

#include <linux/virtiop.h>

int kvm_assign_vmmio(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	printk("kvm_assign_vmmio: 0x%llx, %d\n", bind_device->mmio_gpa, bind_device->mmio_len);
	int ret = 0;
	struct kvm_io_device *dev;
	
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if(!dev) 
		return -ENOMEM;
	
	dev->ops = &virtiop_ops;
	ret = kvm_io_bus_register_dev(kvm, KVM_MMIO_BUS, 
				bind_device->mmio_gpa, bind_device->mmio_len, dev);
	if(ret < 0)
		goto out;

	ret = register_virtiop_mmio_range(kvm, bind_device);
	if(ret < 0)
		goto out;

	return ret;
out:
	deregister_virtiop_mmio_range(bind_device);
	return ret;
}

#include "iodev.h"

#include <linux/kvm_host.h>      
#include <linux/kvm.h>
#include <linux/kernel.h>
#include <linux/slab.h> 
#include <linux/virtio.h>
#include <linux/virtiop.h>

#define HOST_DEV_NAME "1c130000.virtio_block"
extern struct bus_type virtio_bus;

int kvm_assign_vmmio(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	printk("kvm_assign_vmmio: 0x%llx, %d\n", bind_device->mmio_gpa, bind_device->mmio_len);
	int ret = 0;
	struct kvm_io_device *dev;
	void *virtiop_device;

	virtiop_device = virtiop_get_device();
	if(!virtiop_device)
		goto out;
	
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if(!dev) 
		return -ENOMEM;
	
	dev->ops = &virtiop_ops;
	ret = kvm_io_bus_register_dev(kvm, KVM_MMIO_BUS, 
				bind_device->mmio_gpa, bind_device->mmio_len, dev);
	if(ret < 0)
		goto out;

	ret = virtiop_register_mmio_range(kvm, virtiop_device, bind_device);
	if(ret < 0)
		goto out;

	return ret;
out:
	printk("kvm_assigm_vmmio fail\n");
	return ret;
}

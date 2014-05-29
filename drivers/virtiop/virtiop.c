#include <linux/kernel.h> 
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/io.h>
#include <linux/virtio_mmio.h>
#include <linux/virtiop.h>

#define VIRTIOP_DEVICE_ARRAY_SIZE 10
#define HOST_VIRTIO0_BASE 0xffffff800001c000 
#define HOST_VQ_PFN	  0x8f7e7c

int stage2_set_pte(struct kvm *kvm, struct kvm_mmu_memory_cache *cache, 
			phys_addr_t addr, const pte_t *new_pte, bool iomap);

struct kvm_virtiop_bind_device virtiop_device[VIRTIOP_DEVICE_ARRAY_SIZE];
int virtiop_device_num = 0;

int register_virtiop_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	virtiop_device[virtiop_device_num].kvm        = kvm;
	virtiop_device[virtiop_device_num].mmio_gpa   = bind_device->mmio_gpa;
	virtiop_device[virtiop_device_num++].mmio_len = bind_device->mmio_len;
	return 1;
}

void deregister_virtiop_mmio_range(struct kvm_virtiop_bind_device *bind_device)
{
	/* Do nothing for now */
	return 1;
}

static int virtiop_read(struct kvm_io_device *this, gpa_t addr, int len,
                const void *val)
{
	gpa_t mmio_base = virtiop_device[0].mmio_gpa;
	int mmio_len = virtiop_device[0].mmio_len;
	int offset = addr - mmio_base;

	if(offset > VIRTIO_MMIO_CONFIG) {
		int i;
		u8 *ptr = val;
		for(i=0; i<len; i++) 
			ptr[i] = readb(HOST_VIRTIO0_BASE + VIRTIO_MMIO_CONFIG + offset + i);
	}
	else if (offset == VIRTIO_MMIO_QUEUE_PFN) {	// if return not 0, front-end initialization will stop
		*(int*)val = 0;	
	}
	else
		*(unsigned long*)val = readl(HOST_VIRTIO0_BASE+offset);
	printk("virtiop_read gpa=0x%llx, len=%d, data=0x%llx\n", addr, len, val);
	if(val)	
		printk("data=%d\n", *(int*)val);	

        return 0;
}

static int virtiop_write(struct kvm_io_device *this, gpa_t addr, int len,
                const void *val)
{
	gpa_t mmio_base = virtiop_device[0].mmio_gpa;
	int mmio_len = virtiop_device[0].mmio_len;
	struct kvm *kvm = virtiop_device[0].kvm;
	struct kvm_vcpu *vcpu = kvm->vcpus[0];
	int offset = addr - mmio_base;

	printk("virtiop_write gpa=0x%llx, len=%d, data=0x%llx\n", addr, len, val);
	if(val)	
		printk("data=%d\n", *(int*)val);	
	if(offset > VIRTIO_MMIO_CONFIG) {
		int i;
		u8 *ptr = val;
		for(i=0; i<len; i++) 
			writeb(ptr[i], HOST_VIRTIO0_BASE + VIRTIO_MMIO_CONFIG + offset + i);
	}
	else if (offset == VIRTIO_MMIO_QUEUE_PFN) {
		pte_t new_pte = pfn_pte(HOST_VQ_PFN, PAGE_S2);
		u64 gpa = (*(u64*)val)<<PAGE_SHIFT;
		stage2_set_pte(kvm, &vcpu->arch.mmu_page_cache, gpa, &new_pte, false);

		printk("write VIRTIO_MMIO_QUEUE_PFN 0x%llx 0x%llx\n", val, *(unsigned long*)val);
		printk("HOST_VQ_PFN=0x%llx\n", HOST_VQ_PFN);
	}
	else
		writel(*(int*)val, HOST_VIRTIO0_BASE+offset);

        return 0;
}

static void virtiop_destructor(struct kvm_io_device *this)
{
/*        struct _ioeventfd *p = to_ioeventfd(this);

        ioeventfd_release(p);
*/
	printk("virtiop_destructor\n");
}

const struct kvm_io_device_ops virtiop_ops = {                                      
        .read       = virtiop_read,                                                 
        .write      = virtiop_write,                                                
        .destructor = virtiop_destructor,                                           
};                                                                                  
   

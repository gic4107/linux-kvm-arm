#include <linux/kernel.h> 
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/kvm_types.h>
#include <linux/io.h>
#include <linux/virtio_mmio.h>
#include <linux/virtiop.h>
#include <linux/highmem.h>
#include <linux/uio.h>
#include <linux/slab.h>
#include <linux/module.h> 
#include "vring.h"
#include "virtiop.h"

// for debug
#include <linux/virtio_blk.h>

#define VIRTIO_IRQ_LOW      0                                                    
#define VIRTIO_IRQ_HIGH     1 

// Need modify
#define HOST_DEV 0
                
int stage2_set_pte(struct kvm *kvm, struct kvm_mmu_memory_cache *cache, 
			phys_addr_t addr, const pte_t *new_pte, bool iomap);
void kvm_set_page_dirty(struct page *page);
static inline void kvm_set_s2pte_writable(pte_t *pte)                            
{                                                                                
	    pte_val(*pte) |= PTE_S2_RDWR; 
}

struct kvm_virtiop_device virtiop_device[VIRTIOP_DEVICE_ARRAY_SIZE];
int virtiop_device_count = 0;
int host_dev_count = 0;
const int dev_num = 0;				// Just for now, only one device

u64 debug_desc_gfn, debug_avail_gfn, debug_used_gfn;

hpa_t translate_gpa_pa(struct kvm *kvm, gpa_t gpa, int len)
{
	hpa_t hpa;
	u64 offset;
	gfn_t gfn;
	hfn_t hfn;
	bool writeable;
	
	printk("translate_gpa_pa, gpa=0x%llx\n", gpa);
	offset = gpa & (PAGE_SIZE-1);
	gfn = __phys_to_pfn(gpa);
	hfn = gfn_to_pfn_prot(kvm, gfn, 1, &writeable);
	hpa = __pfn_to_phys(hfn) + offset;

	struct page *page = phys_to_page(hpa);
	void *kva = kmap(page);
    struct virtio_blk_outhdr *hdr = (struct virtio_blk_outhdr*)(kva+offset);    
	printk("type=0x%x, ioprio=0x%x, sector=0x%llx\n", hdr->type, hdr->ioprio, hdr->sector);

	printk("offset=0x%llx, gfn=0x%llx, hfn=0x%llx, hpa=0x%llx\n", offset, gfn, hfn, hpa);
	printk("==================================\n");

	return hpa;
}

u16 virt_queue__get_head_iov(struct virt_queue *vq, u16 head, struct kvm *kvm)
{                                                                                
    struct vring_desc *desc;                                                     
    u16 idx;                                                                     
    u16 max;                                                                     
	u32 iov_len;
	gpa_t iov_gpa;
	hpa_t iov_hpa;
                                                                                 
    idx = head;                                                                  
    max = vq->vring.num;                                                         
    desc = vq->vring.desc;                                                       

/*                                                                                 
    if (desc[idx].flags & VRING_DESC_F_INDIRECT) {                               
        max = desc[idx].len / sizeof(struct vring_desc);                         
        desc = guest_flat_to_host(kvm, desc[idx].addr);                          
        idx = 0;                                                                 
    }                                                                            
*/                                                                              
	unsigned char *c;
    do {    
        iov_len = desc[idx].len;                                 
        iov_gpa = desc[idx].addr;      
		printk("===== Translate GPA to PA =====\n");
		printk("idx=%d, iov_len=%d, iov_gpa=0x%llx\n", idx, iov_len, iov_gpa);
        unsigned char *c = &desc[idx];    
        int j = 0;                                                              
        while(1) {                                                              
            printk("%x ", *c);                                                  
            j++;                                                                
			c++;
            if(j==16)   break;                                                  
        }                                                                       
        printk("\n");   

		iov_hpa  = translate_gpa_pa(kvm, iov_gpa, iov_len);
		desc[idx].addr = iov_hpa;
    } while ((idx = next_desc(desc, idx, max)) != max);                          
                                                                                 
    return head;                                                                 
}

int vq_translate_gpa_pa(struct kvm *kvm, pfn_t vq_desc_hfn, 
									     pfn_t vq_avail_hfn, pfn_t vq_used_hfn)
{
    struct page *page_desc, *page_avail, *page_used;
    void *vq_desc, *vq_avail, *vq_used;                             
    u16 head; 
	struct virt_queue *vq;
                                                                                 
    page_desc = pfn_to_page(vq_desc_hfn);            
    if(!page_desc) {                                                         
        printk("desc page null\n");                                          
        return -1;                                                           
    }                                                                        
    vq_desc = kmap(page_desc);                                               
    if(!vq_desc) {                                                           
        printk("vq_desc null\n");                                            
        return -1;                                                           
    }                                                                        
    page_avail = pfn_to_page(vq_avail_hfn);          
    if(!page_avail) {                                                        
        printk("avail page null\n");                                         
        return -1;                                                           
    }                                                                        
    vq_avail = kmap(page_avail);                                             
    if(!vq_avail) {                                                          
        printk("vq_avail null\n");                                           
        return -1;                                                           
    }                                                                        
    page_used = pfn_to_page(vq_used_hfn);          
    if(!page_used) {                                                        
        printk("used page null\n");                                         
        return -1;                                                           
    }                                                                        
    vq_used = kmap(page_used);                                             
    if(!vq_used) {                                                          
        printk("vq_used null\n");                                           
        return -1;                                                           
    }                                                                        
	printk("desc_hfn=0x%x, avail_hfn=0x%x, used_hfn=0x%x\n", vq_desc_hfn, vq_avail_hfn, vq_used_hfn);
    printk("vq_desc=0x%lx, vq_avail=0x%lx, vq_used=0x%lx\n", (unsigned long)vq_desc, (unsigned long)vq_avail, (unsigned long)vq_used);

	vq = kmalloc(sizeof(struct virt_queue), GFP_KERNEL);	
	vq->vring.desc  = vq_desc;
	vq->vring.avail = vq_avail;
	vq->vring.used  = vq_used;
	vq->vring.num   = VIRTIO_BLK_QUEUE_SIZE;
	vq->last_avail_idx = 0;

    while (virt_queue__available(vq)) {
        head        = virt_queue__pop(vq);
		printk("vq_translate_gpa_pa, head=%d\n", head);
        virt_queue__get_head_iov(vq, head, kvm);
    }	

	kfree(vq);
	kunmap(vq_used);
	kunmap(vq_avail);
	kunmap(vq_desc);

	return 1;
}

#define KVM_IRQCHIP_IRQ(x) (KVM_ARM_IRQ_TYPE_SPI << KVM_ARM_IRQ_TYPE_SHIFT) | \
               ((x) & KVM_ARM_IRQ_NUM_MASK)                                         

static irqreturn_t irq_to_guest(int irq, void* opaque)  
{                                                                                   
    struct kvm *kvm = virtiop_device[dev_num].kvm;                                  
    int r;                                                                          
                                                                                    
    struct kvm_irq_level irq_level = {                                              
        .irq    = KVM_IRQCHIP_IRQ(irq),                                             
        .level  = !!VIRTIO_IRQ_HIGH,                                                              
    };                                                                              
    printk("irq_to_guest irq=0x%x level=%d\n", irq_level.irq, irq_level.level);           // 39
    r = kvm_vm_ioctl_irq_line(kvm, &irq_level, 0);                                  
                                                                                    
    irq_level.level = !!VIRTIO_IRQ_LOW;                                             
    printk("irq_to_guest irq=0x%x level=%d\n", irq_level.irq, irq_level.level);           // 39
    r = kvm_vm_ioctl_irq_line(kvm, &irq_level, 0);                                  
//    printk("irq_to_guest done\n");                                                  
                                                                                    
    return r;                                                                       
} 

/*
static void set_irq_to_guest(void) 
{                                                                                
	int hash_value = hash_fn((unsigned long)virtiop_device[dev_num].host_dev);
    struct list_head *hash_head = &irq_desc_hash[hash_value];                    
//  printk("dev_id=0x%llx, hash value=%d\n", dev_id, hash_fn((unsigned long)dev_id));
    struct irq_desc_t *irq_desc;                                                 
    list_for_each_entry(irq_desc, hash_head, node) {                             
			irq_desc->handler = irq_to_guest;
    }                                                                            
} 
*/                                                                               
     
int virtiop_register_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	host_dev[HOST_DEV].guest_dev = virtiop_device_count;
	host_dev[HOST_DEV].guest_start = 1;
	virtiop_device[virtiop_device_count].kvm        = kvm;
	virtiop_device[virtiop_device_count].mmio_gpa   = bind_device->mmio_gpa;
	virtiop_device[virtiop_device_count].mmio_irq   = bind_device->mmio_irq;
	virtiop_device[virtiop_device_count++].mmio_len = bind_device->mmio_len;

	return 1;
}

void virtiop_deregister_mmio_range(struct kvm_virtiop_bind_device *bind_device)
{
	/* Do nothing for now */
}

static int virtiop_read(struct kvm_io_device *this, gpa_t addr, int len,
                void *val)
{
	gpa_t mmio_base = virtiop_device[dev_num].mmio_gpa;
	int mmio_len = virtiop_device[dev_num].mmio_len;
	int offset = addr - mmio_base;
	volatile void __iomem *target = (void*)HOST_VIRTIO0_BASE + offset;

	if(offset >= VIRTIO_MMIO_CONFIG) {
		u8 *ptr = val;
		*ptr = readb(target);
	}
	else if (offset == VIRTIO_MMIO_QUEUE_PFN) {	// if return not 0, front-end initialization will stop
		*(int*)val = 0;	
	}
	else if (offset == VIRTIO_MMIO_INTERRUPT_STATUS)
		*(int*)val = 1;
	else
		*(unsigned long*)val = readl(target);
	printk("virtiop_read gpa=0x%llx, len=%d, data=0x%lx\n", addr, len, (unsigned long)val);

    return 0;
}

static int virtiop_write(struct kvm_io_device *this, gpa_t addr, int len,
                const void *val)
{
	gpa_t mmio_base = virtiop_device[dev_num].mmio_gpa;
	int mmio_len = virtiop_device[dev_num].mmio_len;
	struct kvm *kvm = virtiop_device[dev_num].kvm;
	struct kvm_vcpu *vcpu = kvm->vcpus[0];
	int offset = addr - mmio_base;
	volatile void __iomem *target = (HOST_VIRTIO0_BASE + offset);

	printk("virtiop_write gpa=0x%llx, len=%d, data=0x%lx\n", addr, len, (unsigned long)val);
	if(offset > VIRTIO_MMIO_CONFIG) {
		u8 *ptr = val;
		writeb(*ptr, target);
	}
	else if (offset == VIRTIO_MMIO_QUEUE_PFN) {
		/* Method1: Map guest's VQ host's VQ */
		pte_t host_desc_pte, host_avail_pte, host_used_pte;
		u64 desc_gpa, avail_gpa, used_gpa;
		u64 queue_pfn = *(u64*)val;

		hfn_t host_vq_pfn = (unsigned long)(host_dev[HOST_DEV].queue) >> 12;	
		printk("host_vq_pfn = 0x%x\n", host_vq_pfn);

		host_desc_pte  = pfn_pte(host_vq_pfn, PAGE_S2);
		host_avail_pte = pfn_pte(host_vq_pfn+1, PAGE_S2);
		host_used_pte  = pfn_pte(host_vq_pfn+2, PAGE_S2);
        kvm_set_s2pte_writable(&host_desc_pte);                                                                                                                                  
        kvm_set_s2pte_writable(&host_avail_pte);                                                                                                                                  
        kvm_set_s2pte_writable(&host_used_pte);                                                                                                                                  
        kvm_set_pfn_dirty(host_vq_pfn);
        kvm_set_pfn_dirty(host_vq_pfn+1);
        kvm_set_pfn_dirty(host_vq_pfn+2);

		desc_gpa  = queue_pfn<<PAGE_SHIFT;
		avail_gpa = (queue_pfn+1)<<PAGE_SHIFT;
		used_gpa  = (queue_pfn+2)<<PAGE_SHIFT;

		debug_desc_gfn  = queue_pfn;
		debug_avail_gfn = queue_pfn+1;
		debug_used_gfn  = queue_pfn+2;

		int ret;
		ret = stage2_set_pte(kvm, &vcpu->arch.mmu_page_cache, desc_gpa, &host_desc_pte, false);
//		printk("stage2_set_pte, ret=%d\n", ret);
		ret = stage2_set_pte(kvm, &vcpu->arch.mmu_page_cache, avail_gpa, &host_avail_pte, false);
//		printk("stage2_set_pte, ret=%d\n", ret);
		ret = stage2_set_pte(kvm, &vcpu->arch.mmu_page_cache, used_gpa, &host_used_pte, false);
//		printk("stage2_set_pte, ret=%d\n", ret);

		virtiop_device[dev_num].vq_desc_hfn  = host_vq_pfn;
		virtiop_device[dev_num].vq_avail_hfn = host_vq_pfn+1;
		virtiop_device[dev_num].vq_used_hfn  = host_vq_pfn+2;

		reset_vring(virtiop_device[dev_num].vq_desc_hfn, 
								 virtiop_device[dev_num].vq_avail_hfn,
							     virtiop_device[dev_num].vq_used_hfn);

		printk("GPA: desc=0x%llx, avail=0x%llx, used=0x%llx\n", desc_gpa, avail_gpa, used_gpa);
		printk("desc=0x%x, avail=0x%x, used=0x%x\n", host_vq_pfn, host_vq_pfn+1, host_vq_pfn+2);

		/* Method2: Change host's VQ to where guest set */
/*		bool writeable;
		printk("Write VIRTIO_MMIO_QUEUE_PFN val=0x%llx\n", *(gfn_t*)val);
		hfn_t hfn = gfn_to_pfn_prot(kvm, *(gfn_t*)val, 1, &writeable);
		hfn_t hfn_avail = gfn_to_pfn_prot(kvm, (*(gfn_t*)val)+VQ_AVAIL_GFN_OFFSET, 1, &writeable); 
		hfn_t hfn_used =  gfn_to_pfn_prot(kvm, (*(gfn_t*)val)+VQ_USED_GFN_OFFSET, 1, &writeable);
		if(hfn==KVM_PFN_ERR_MASK || hfn==KVM_PFN_ERR_NOSLOT_MASK 
    	    || hfn==KVM_PFN_NOSLOT	|| hfn==KVM_PFN_ERR_FAULT 
	    	|| hfn==KVM_PFN_ERR_HWPOISON || hfn==KVM_PFN_ERR_RO_FAULT) 
			printk("PFN_ERR: 0x%llx\n", hfn);
		else {
			virtiop_device[dev_num].vq_desc_hfn  = hfn;
			virtiop_device[dev_num].vq_avail_hfn = hfn_avail;
			virtiop_device[dev_num].vq_used_hfn  = hfn_used;
		}
		printk("desc=0x%x, avail=0x%x, used=0x%x\n", hfn, hfn_avail, hfn_used);
		writel((u32)hfn, target);
*/
	}
	else if(offset == VIRTIO_MMIO_QUEUE_NOTIFY) {
		/* Guest kick, translate all GPA in description table into PA */
		vq_translate_gpa_pa(kvm, virtiop_device[dev_num].vq_desc_hfn, 
								 virtiop_device[dev_num].vq_avail_hfn,
							     virtiop_device[dev_num].vq_used_hfn);
		printk("vq_translate_gpa_pa done\n");

		writel(*(int*)val, target);
//		irq_to_guest(39, NULL);
	}
	else
		writel(*(int*)val, target);

        return 0;
}

static void virtiop_destructor(struct kvm_io_device *this)
{
	printk("virtiop_destructor\n");
}

const struct kvm_io_device_ops virtiop_ops = {                                      
    .read       = virtiop_read,                                                 
    .write      = virtiop_write,                                                
    .destructor = virtiop_destructor,                                           
};                                                                                  

static irqreturn_t interrupt_distribute(int irq, void* dev_id)                            
{                                                                                
	int i;
//	printk("interrupt_distribute, irq=%d\n", irq);
	for(i=0; i<host_dev_count; i++) {
		if(host_dev[i].dev == dev_id) {
			struct host_device hdev = host_dev[i];
			hdev.irqd.handler(hdev.irqd.irq, dev_id);
			if(hdev.guest_start) {
				irq_to_guest(virtiop_device[hdev.guest_dev].mmio_irq, dev_id);
			}
		}
	}
/*
	if(guest_start) {
		u8 guest_irq = virtiop_device[dev_num].mmio_irq; 	
		irq_to_guest(guest_irq, virtiop_device[dev_num].host_dev);	
	}
    list_for_each_entry(irq_desc, hash_head, node) {                             
        if(irq_desc->dev == dev_id)                                              
	        return irq_desc->handler(irq, dev_id);     
    }                                                      
*/
} 

void virtiop_register_host_dev_vq(void *dev, void *queue)
{
	host_dev[host_dev_count++].queue = queue;	
}

int virtiop_register_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{                                                                                
	host_dev[host_dev_count].irqd.irq = irq;
	host_dev[host_dev_count].irqd.handler = handler;
	host_dev[host_dev_count].irqd.flags = flags;
	host_dev[host_dev_count].dev = dev;
	strcpy(host_dev[host_dev_count].name, name);

	virtiop_device[dev_num].host_dev = dev;
    return request_irq(irq, interrupt_distribute, flags, name, dev);      
}                                                                  

/*
static int __init virtiop_init(void) {                                          

out:                                                                                
    return 0;                                                                       
}                                                                                   
fs_initcall(virtiop_init);                                                      
                                                                                    
static void __exit virtiop_exit(void) {                                         
}                                                                                   
module_exit(virtiop_exit);                                                      
                                                                                    
MODULE_LICENSE("GPL v2");                                                           
MODULE_AUTHOR("Yu-Ju Huang gic4107@gmail.com");  
*/

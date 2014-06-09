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

#define VIRTIO_IRQ_LOW      0                                                    
#define VIRTIO_IRQ_HIGH     1 
                
struct list_head *irq_desc_hash;    // hash table array                          
                                                                                 
int stage2_set_pte(struct kvm *kvm, struct kvm_mmu_memory_cache *cache, 
			phys_addr_t addr, const pte_t *new_pte, bool iomap);

struct kvm_virtiop_device virtiop_device[VIRTIOP_DEVICE_ARRAY_SIZE];
int virtiop_device_count = 0;
int desc_count = 0;
const int dev_num = 0;				// Just for now, only one device

int guest_start = 0;

hpa_t translate_gpa_pa(struct kvm *kvm, gpa_t gpa, int len)
{
	hpa_t hpa;
	u64 offset;
	gfn_t gfn;
	hfn_t hfn;
	bool writeable;
	
	printk("translate_gpa_pa, pga=0x%llx\n", gpa);
	offset = gpa & (PAGE_SIZE-1);
	gfn = __phys_to_pfn(gpa);
	hfn = gfn_to_pfn_prot(kvm, gfn, 1, &writeable);
	hpa = __pfn_to_phys(hfn) + offset;


    struct page *page;
	void* kv_page;
    page = pfn_to_page(hfn);            
    if(!page) {                                                         
        printk("page null\n");                                          
        return -1;                                                           
    }                                                                        
    kv_page = kmap(page);                                               
    if(!kv_page) {                                                           
        printk("kv_page null\n");                                            
		return -1;                                                           
	}
	printk("dump data\n");
	unsigned char* i;
	for(i=kv_page; i<kv_page+len; i++) 
		printk("%c", *i);
	printk("\n");
	for(i=kv_page; i<kv_page+len; i++) 
		printk("%x", *i);
	printk("\n");
	kunmap(kv_page);	

	printk("offset=0x%llx, gfn=0x%llx, hfn=0x%llx, hpa=0x%llx\n", offset, gfn, hfn, hpa);

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
    do {                                                                         
        iov_len = desc[idx].len;                                 
        iov_gpa = desc[idx].addr;      
		printk("idx=%d, iov_len=%d, iov_gpa=0x%llx\n", idx, iov_len, iov_gpa);
		iov_hpa  = translate_gpa_pa(kvm, iov_gpa, iov_len);
		desc[idx].addr = iov_hpa;
		printk("iov_hpa=%p, desc[%d].addr=%p\n", iov_hpa, idx, desc[idx].addr);
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
    printk("vq_desc=0x%lx, vq_avail=0x%lx, vq_used=0x%lx\n", (unsigned long)vq_desc, (unsigned long)vq_avail, (unsigned long)vq_used);

	vq = kmalloc(sizeof(struct virt_queue), GFP_KERNEL);	
	vq->vring.desc  = vq_desc;
	vq->vring.avail = vq_avail;
	vq->vring.used  = vq_used;
	vq->vring.num   = VIRTIO_BLK_QUEUE_SIZE;
	vq->last_avail_idx = -1;

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
    printk("irq_to_guest irq=%x level=%d\n", irq_level.irq, irq_level.level);           // 39
    r = kvm_vm_ioctl_irq_line(kvm, &irq_level, 0);                                  
                                                                                    
    irq_level.level = !!VIRTIO_IRQ_LOW;                                             
    printk("irq_to_guest irq=%x level=%d\n", irq_level.irq, irq_level.level);           // 39
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
     
int register_virtiop_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	virtiop_device[virtiop_device_count].kvm        = kvm;
	virtiop_device[virtiop_device_count].mmio_gpa   = bind_device->mmio_gpa;
	virtiop_device[virtiop_device_count].mmio_irq   = bind_device->mmio_irq;
	virtiop_device[virtiop_device_count++].mmio_len = bind_device->mmio_len;
//	set_irq_to_guest();
	printk("guest_start\n");
	guest_start = 1;

	return 1;
}

void deregister_virtiop_mmio_range(struct kvm_virtiop_bind_device *bind_device)
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
	//printk("virtiop_read gpa=0x%llx, len=%d, data=0x%lx\n", addr, len, (unsigned long)val);

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

	//printk("virtiop_write gpa=0x%llx, len=%d, data=0x%lx\n", addr, len, (unsigned long)val);
	if(offset > VIRTIO_MMIO_CONFIG) {
		u8 *ptr = val;
		writeb(*ptr, target);
	}
	else if (offset == VIRTIO_MMIO_QUEUE_PFN) {
		/* Method1: Map guest's VQ host's VQ */
/*		pte_t new_pte = pfn_pte(HOST_VQ_PFN, PAGE_S2);
		u64 gpa = (*(u64*)val)<<PAGE_SHIFT;
		printk("write VIRTIO_MMIO_QUEUE_PFN gpa=0x%llx, host_vq_pfn=0x%llx\n", gpa, HOST_VQ_PFN);
		stage2_set_pte(kvm, &vcpu->arch.mmu_page_cache, gpa, &new_pte, false);
*/
		/* Method2: Change host's VQ to where guest set */
		bool writeable;
		printk("Write VIRTIO_MMIO_QUEUE_PFN val=0x%llx\n", *(gfn_t*)val);
		hfn_t hfn = gfn_to_pfn_prot(kvm, *(gfn_t*)val, 1, &writeable);
		if(hfn==KVM_PFN_ERR_MASK || hfn==KVM_PFN_ERR_NOSLOT_MASK 
    	    || hfn==KVM_PFN_NOSLOT	|| hfn==KVM_PFN_ERR_FAULT 
	    	|| hfn==KVM_PFN_ERR_HWPOISON || hfn==KVM_PFN_ERR_RO_FAULT) 
			printk("PFN_ERR: 0x%llx\n", hfn);
		else {
			virtiop_device[dev_num].vq_desc_hfn  = hfn;
			virtiop_device[dev_num].vq_avail_hfn = hfn+VQ_AVAIL_OFFSET;
			virtiop_device[dev_num].vq_used_hfn  = hfn+VQ_USED_OFFSET;
		}
		printk("gfn=0x%llx, pfn=0x%llx writeable=%d\n", *(gfn_t*)val, hfn, writeable);
		int tmp;
		tmp = readl(target);
		printk("tmp=%x\n", tmp);
		writel((u32)hfn, target);
		tmp = readl(target);
		printk("tmp=%x\n", tmp);
	}
	else if(offset == VIRTIO_MMIO_QUEUE_NOTIFY) {
		/* Guest kick, translate all GPA in description table into PA */
		vq_translate_gpa_pa(kvm, virtiop_device[dev_num].vq_desc_hfn, 
								 virtiop_device[dev_num].vq_avail_hfn,
							     virtiop_device[dev_num].vq_used_hfn);
		printk("vq_translate_gpa_pa done\n");

		printk("virtiop kick\n");
//		writel(*(int*)val, target);
		irq_to_guest(39, NULL);
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
    int hash_value = hash_fn((unsigned long)dev_id);                             
    struct list_head *hash_head = &irq_desc_hash[hash_value];                    
//  printk("dev_id=0x%llx, hash value=%d\n", dev_id, hash_fn((unsigned long)dev_id));
    struct irq_desc_t *irq_desc;                                                 

	if(guest_start) {
		u8 guest_irq = virtiop_device[dev_num].mmio_irq; 	
		irq_to_guest(guest_irq, virtiop_device[dev_num].host_dev);	
	}
    list_for_each_entry(irq_desc, hash_head, node) {                             
        if(irq_desc->dev == dev_id)                                              
	        return irq_desc->handler(irq, dev_id);     
    }                                                      
//	printk("interrupt_distribute, irq=%d, guest_start=%d\n", irq, guest_start);		// 39. 1
} 

int register_irq(unsigned int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev)
{                                                                                
    int hash_value = hash_fn((unsigned long)dev);                                
    printk("register_irq ... irq=%d, name=%s, hash=%u", irq, name, hash_fn((unsigned long)dev));
    struct list_head *hash_head = &irq_desc_hash[hash_value];                    
    struct irq_desc_t *irq_desc = kmalloc(sizeof(struct irq_desc_t), GFP_KERNEL);       

    if(list_empty(hash_head)) {                                                  
        printk("irq_desc_hash[%d] null\n", hash_value);                          
        irq_desc->irq = irq;                                                     
        irq_desc->handler = handler;
        irq_desc->flags = flags;                                                 
        irq_desc->dev = dev;                                                     
        strcpy(irq_desc->name, name);                                            
        list_add(&(irq_desc->node), hash_head);                                  
    }                                                                            
    else {                                                                       
        printk("irq_desc_hash[%d] not null\n", hash_value);                      
        while(1) {                                                               
            struct list_head *iter = hash_head->next;                            
            if(list_is_last(iter, hash_head)) {                                  
                irq_desc->irq = irq;                                             
                irq_desc->handler = handler;                                     
                irq_desc->flags = flags;                                         
                irq_desc->dev = dev;                                             
                strcpy(irq_desc->name, name);                                    
                list_add(&(irq_desc->node), hash_head);                          
            }                                                                    
        }                                                                        
                                                                                 
    }                                                                            
    desc_count++;                                                                

	virtiop_device[dev_num].host_dev = dev;
    return request_irq(irq, interrupt_distribute, flags, name, dev);      
}                                                                  

static int __init virtiop_init(void) {                                          
    printk("/****** VirtioP Init ******/ : ");                             
	int i;

    irq_desc_hash = kmalloc(DESC_NUM*sizeof(struct list_head), GFP_KERNEL);         
    if(!irq_desc_hash) {                                                            
        printk("irq_desc_hash allocate fail\n");                                    
        goto out;                                                                   
    }                                                                               
    for(i=0; i<DESC_NUM; i++)                                                       
        INIT_LIST_HEAD(&(irq_desc_hash[i]));                                        
out:                                                                                
    return 0;                                                                       
}                                                                                   
fs_initcall(virtiop_init);                                                      
                                                                                    
static void __exit virtiop_exit(void) {                                         
    printk("/****** Exit VirtioP ******/\n");                         
}                                                                                   
module_exit(virtiop_exit);                                                      
                                                                                    
MODULE_LICENSE("GPL v2");                                                           
MODULE_AUTHOR("Yu-Ju Huang gic4107@gmail.com");  

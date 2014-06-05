#include <linux/kernel.h> 
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/kvm_types.h>
#include <linux/io.h>
#include <linux/virtio_mmio.h>
#include <linux/virtiop.h>
#include <linux/highmem.h>
#include <linux/virtio_ring.h>
#include <linux/uio.h>
#include <linux/slab.h>

#define VIRTIOP_DEVICE_ARRAY_SIZE 10
#define HOST_VIRTIO0_BASE 0xffffff800001c000 
#define HOST_VQ_PFN	  0x8f7e7c
#define VIRTIO_BLK_QUEUE_SIZE 256

#define VQ_AVAIL_OFFSET 1
#define VQ_USED_OFFSET  2

int stage2_set_pte(struct kvm *kvm, struct kvm_mmu_memory_cache *cache, 
			phys_addr_t addr, const pte_t *new_pte, bool iomap);

struct kvm_virtiop_device virtiop_device[VIRTIOP_DEVICE_ARRAY_SIZE];
int virtiop_device_count = 0;
int dev_num = 0;

struct virt_queue { 
    struct vring    vring;                                                       
    u32     pfn;                   // gpa
    /* The last_avail_idx field is an index to ->ring of struct vring_avail.     
       It's where we assume the next request index is at.  */                    
    u16     last_avail_idx;                                                      
    u16     last_used_signalled;                                                 
};  

static unsigned next_desc(struct vring_desc *desc,                                                                                                                                 
              unsigned int i, unsigned int max)                                  
{                                                                                
    unsigned int next;                                                           
                                                                                 
    /* If this descriptor says it doesn't chain, we're done. */                  
    if (!(desc[i].flags & VRING_DESC_F_NEXT))                                    
        return max;                                                              
                                                                                 
    /* Check they're not leading us off end of descriptors. */                   
    next = desc[i].next;                                                         
    /* Make sure compiler knows to grab that: we don't want it changing! */      
    wmb();                                                                       
                                                                                 
    return next;                                                                 
} 

static inline u16 virt_queue__pop(struct virt_queue *queue)                      
{                                                                                
    return queue->vring.avail->ring[queue->last_avail_idx++ % queue->vring.num]; 
}                                                                                
                                                                                 
static inline bool virt_queue__available(struct virt_queue *vq) 
{                                                                                
    if (!vq->vring.avail)                                                        
        return 0;                                                                
                                                                                 
	printk("virt_queue__available: idx=%d, last_idx=%d\n", vq->vring.avail->idx, vq->last_avail_idx);
    return vq->vring.avail->idx !=  vq->last_avail_idx;
}    

u16 virt_queue__get_head_iov(struct virt_queue *vq, struct iovec iov[], u16 head, struct kvm *kvm)
{                                                                                
    struct vring_desc *desc;                                                     
    u16 idx;                                                                     
    u16 max;                                                                     
	u32 iov_len;
	void *iov_gpa, *iov_pa;
                                                                                 
    idx = head;                                                                  
    max = vq->vring.num;                                                         
    desc = vq->vring.desc;                                                       
                                                                                 
/*    if (desc[idx].flags & VRING_DESC_F_INDIRECT) {                               
        max = desc[idx].len / sizeof(struct vring_desc);                         
        desc = guest_flat_to_host(kvm, desc[idx].addr);                          
        idx = 0;                                                                 
    }                                                                            
*/                                                                              
    do {                                                                         
		printk("get iov\n");
        iov_len = desc[idx].len;                                 
        iov_gpa = desc[idx].addr;      
//		iov_pa  = translate_gpa_pa(kvm, iov_gpa);
		printk("idx=%d, iov_len=%d, iov_gpa=0x%llx\n", idx, iov_len, (unsigned long*)iov_gpa);
    } while ((idx = next_desc(desc, idx, max)) != max);                          
                                                                                 
    return head;                                                                 
}

int vq_translate_gpa_pa(struct kvm *kvm, struct vring_desc *vq_desc, struct vring_avail *vq_avail)
{
    struct blk_dev_req *req;                                                     
    u16 head; 
	struct virt_queue *vq;
	struct iovec iov[VIRTIO_BLK_QUEUE_SIZE];

	vq = kmalloc(sizeof(struct virt_queue), GFP_KERNEL);	
	vq->vring.desc  = vq_desc;
	vq->vring.avail = vq_avail;
	vq->vring.num   = VIRTIO_BLK_QUEUE_SIZE;
	vq->last_avail_idx = -1;

    while (virt_queue__available(vq)) {
		printk("vq_translate_gpa_pa, head=%d\n", head);
        head        = virt_queue__pop(vq);
        virt_queue__get_head_iov(vq, iov, head, kvm);
    }	
	kfree(vq);
}

int register_virtiop_mmio_range(struct kvm *kvm, struct kvm_virtiop_bind_device *bind_device)
{
	virtiop_device[virtiop_device_count].kvm        = kvm;
	virtiop_device[virtiop_device_count].mmio_gpa   = bind_device->mmio_gpa;
	virtiop_device[virtiop_device_count++].mmio_len = bind_device->mmio_len;
	return 1;
}

void deregister_virtiop_mmio_range(struct kvm_virtiop_bind_device *bind_device)
{
	/* Do nothing for now */
	return 1;
}

static int virtiop_read(struct kvm_io_device *this, gpa_t addr, int len,
                void *val)
{
	gpa_t mmio_base = virtiop_device[dev_num].mmio_gpa;
	int mmio_len = virtiop_device[dev_num].mmio_len;
	int offset = addr - mmio_base;

	if(offset >= VIRTIO_MMIO_CONFIG) {
		int i;
		u8 *ptr = val;
		*ptr = readb(HOST_VIRTIO0_BASE + offset + i);
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
	gpa_t mmio_base = virtiop_device[dev_num].mmio_gpa;
	int mmio_len = virtiop_device[dev_num].mmio_len;
	struct kvm *kvm = virtiop_device[dev_num].kvm;
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
		/* Method1: Map guest's VQ host's VQ */
/*		pte_t new_pte = pfn_pte(HOST_VQ_PFN, PAGE_S2);
		u64 gpa = (*(u64*)val)<<PAGE_SHIFT;
		printk("write VIRTIO_MMIO_QUEUE_PFN gpa=0x%llx, host_vq_pfn=0x%llx\n", gpa, HOST_VQ_PFN);
		stage2_set_pte(kvm, &vcpu->arch.mmu_page_cache, gpa, &new_pte, false);
*/
		/* Method2: Change host's VQ to where guest set */
		bool writeable;
		printk("Write VIRTIO_MMIO_QUEUE_PFN val=0x%llx\n", *(gfn_t*)val);
		pfn_t pfn = gfn_to_pfn_prot(kvm, *(gfn_t*)val, 1, &writeable);
		if(pfn==KVM_PFN_ERR_MASK || pfn==KVM_PFN_ERR_NOSLOT_MASK 
    	    || pfn==KVM_PFN_NOSLOT	|| pfn==KVM_PFN_ERR_FAULT 
	    	|| pfn==KVM_PFN_ERR_HWPOISON || pfn==KVM_PFN_ERR_RO_FAULT) 
			printk("PFN_ERR: 0x%llx\n", pfn);
		else {
			virtiop_device[dev_num].vq_desc_pfn  = pfn;
			virtiop_device[dev_num].vq_avail_pfn = pfn+VQ_AVAIL_OFFSET;
			virtiop_device[dev_num].vq_used_pfn  = pfn+VQ_USED_OFFSET;
		}
		printk("gfn=0x%llx, pfn=0x%llx writeable=%d\n", *(gfn_t*)val, pfn, writeable);
		writel((u32)pfn, HOST_VIRTIO0_BASE+offset);
	}
	else if(offset == VIRTIO_MMIO_QUEUE_NOTIFY) {
		/* Guest kick, translate all GPA in description table into PA */
		struct page *page_desc, *page_avail;
		void *vq_desc, *vq_avail;

		page_desc = pfn_to_page(virtiop_device[dev_num].vq_desc_pfn);
		if(!page_desc) {
			printk("desc page null\n");
			return -1;
		}
		vq_desc = kmap(page_desc);
		if(!vq_desc) {
			printk("vq_desc null\n");
			return -1;
		}
		page_avail = pfn_to_page(virtiop_device[dev_num].vq_avail_pfn);
		if(!page_avail) {
			printk("avail page null\n");
			return -1;
		}
		vq_avail = kmap(page_avail);
		if(!vq_avail) {
			printk("vq_avail null\n");
			return -1;
		}
		printk("vq_desc=0x%llx, vq_avail=0x%llx\n", (unsigned long*)vq_desc, (unsigned long*)vq_avail);

		vq_translate_gpa_pa(kvm, vq_desc, vq_avail);
		printk("vq_translate_gpa_pa done\n");
		kunmap(page_avail);	
		kunmap(page_desc);
	}
	else
		writel(*(int*)val, HOST_VIRTIO0_BASE+offset);

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
   

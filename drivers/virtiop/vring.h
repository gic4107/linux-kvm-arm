#include <linux/virtio_ring.h>

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
	printk("used idx=%d\n", queue->vring.used->idx);
    return queue->vring.avail->ring[queue->last_avail_idx++ % queue->vring.num]; 
}                                                                                
                                                                                 
static inline bool virt_queue__available(struct virt_queue *vq) 
{                                                                                
    if (!vq->vring.avail)                                                        
        return 0;                                                                
                                                                                 
	printk("virt_queue__available: idx=%d, last_idx=%d\n", vq->vring.avail->idx, vq->last_avail_idx);
    return vq->vring.avail->idx !=  vq->last_avail_idx;
}    

static int reset_vring(pfn_t vq_desc_hfn, pfn_t vq_avail_hfn, pfn_t vq_used_hfn)
{
    struct page *page_desc, *page_avail, *page_used;
    struct vring_desc *vq_desc;
	struct vring_avail *vq_avail;
	struct vring_used *vq_used; 
    u16 head; 
	struct virt_queue *vq;
                                                                                 
	printk("reset_vring\n");
    page_desc = pfn_to_page(vq_desc_hfn);            
    if(!page_desc) {                                                         
        printk("desc page null\n");                                          
        return -1;                                                           
    }                                                                        
    vq_desc = (struct vring_desc*)kmap(page_desc);                                               
    if(!vq_desc) {                                                           
        printk("vq_desc null\n");                                            
        return -1;                                                           
    }                                                                        
    page_avail = pfn_to_page(vq_avail_hfn);          
    if(!page_avail) {                                                        
        printk("avail page null\n");                                         
        return -1;                                                           
    }                                                                        
    vq_avail = (struct vring_avail*)kmap(page_avail);                                             
    if(!vq_avail) {                                                          
        printk("vq_avail null\n");                                           
        return -1;                                                           
    }                                                                        
    page_used = pfn_to_page(vq_used_hfn);          
    if(!page_used) {                                                        
        printk("used page null\n");                                         
        return -1;                                                           
    }                                                                        
    vq_used = (struct vring_uesd*)kmap(page_used);                                             
    if(!vq_used) {                                                          
        printk("vq_used null\n");                                           
        return -1;                                                           
    }                                                                        
    printk("vq_desc=0x%lx, vq_avail=0x%lx, vq_used=0x%lx\n", (unsigned long)vq_desc, (unsigned long)vq_avail, (unsigned long)vq_used);

	vq_avail->idx = 0;
	vq_used->idx  = 0;

	kunmap(vq_used);
	kunmap(vq_avail);
	kunmap(vq_desc);
}

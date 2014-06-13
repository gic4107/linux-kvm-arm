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
    return queue->vring.avail->ring[queue->last_avail_idx++ % queue->vring.num]; 
}                                                                                
                                                                                 
static inline bool virt_queue__available(struct virt_queue *vq) 
{                                                                                
    if (!vq->vring.avail)                                                        
        return 0;                                                                
                                                                                 
    return vq->vring.avail->idx !=  vq->last_avail_idx;
}    


#define VIRTIOP_DEVICE_ARRAY_SIZE 10
#define HOST_VIRTIO0_BASE 0xffffff800001c000 
#define HOST_VQ_PFN	  0x8f7e7c
#define VIRTIO_BLK_QUEUE_SIZE 256

#define VQ_AVAIL_GFN_OFFSET 1
#define VQ_USED_GFN_OFFSET  2

#define DESC_NUM 10                                                              
#define NAME_LEN 30                                                              
#define DEVICE_NAME "VirtioP"
                                                                                 
#define hash_fn(dev_id) (unsigned int)((dev_id>>2)%DESC_NUM)                     

struct irq_desc_t {                                                              
    unsigned int irq;                                                            
    irq_handler_t handler;                                                    
    unsigned long flags;                                                         
    char name[NAME_LEN];                                                         
    void *dev;                                                                   
    struct list_head node;
};


#define HOST_VIRTIO0_BASE 0xffffff800001c000 
#define VIRTIO_BLK_QUEUE_SIZE 256
#define VIRTIOP_DEVICE_ARRAY_SIZE 10

#define VQ_AVAIL_PFN_OFFSET 1
#define VQ_USED_PFN_OFFSET  2

#define DEVICE_NAME "VirtioP"
#define HOST_DEV_NUM 10

struct host_device {
	void *dev;
	void *queue;
	char name[30];
	struct irq_desc {
	    unsigned int irq;                                                            
	    irq_handler_t handler;                                                    
	    unsigned long flags;                                                         
	} irqd;
	int guest_dev;
	int guest_start;
	int last_avail_idx;
};

struct host_device host_dev[HOST_DEV_NUM];


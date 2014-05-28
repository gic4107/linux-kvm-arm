#include <linux/kernel.h> 
#include <linux/virtiop.h>



static int virtiop_read(struct kvm_io_device *this, gpa_t addr, int len,
                const void *val)
{
/*        struct _ioeventfd *p = to_ioeventfd(this);

        if (!ioeventfd_in_range(p, addr, len, val))
                return -EOPNOTSUPP;

        eventfd_signal(p->eventfd, 1);
*/
	printk("virtiop_read gpa=0x%llx, len=%d, data=0x%llx\n", addr, len, val);	// 0x10600, 4, 0xffffffc877d7fc28
	if(!val)	// no
		printk("data=%d\n", *(int*)val);	
        return 0;
}
//EXPORT_SYMBOL_GPU(virtiop_read);


static int virtiop_write(struct kvm_io_device *this, gpa_t addr, int len,
                const void *val)
{
/*        struct _ioeventfd *p = to_ioeventfd(this);

        if (!ioeventfd_in_range(p, addr, len, val))
                return -EOPNOTSUPP;

        eventfd_signal(p->eventfd, 1);
*/
	printk("virtiop_write\n");
        return 0;
}
//EXPORT_SYMBOL_GPU(virtiop_write);

static void virtiop_destructor(struct kvm_io_device *this)
{
/*        struct _ioeventfd *p = to_ioeventfd(this);

        ioeventfd_release(p);
*/
	printk("virtiop_destructor\n");
}
//EXPORT_SYMBOL_GPU(virtiop_destructor);

const struct kvm_io_device_ops virtiop_ops = {                                      
        .read       = virtiop_read,                                                 
        .write      = virtiop_write,                                                
        .destructor = virtiop_destructor,                                           
};                                                                                  
   

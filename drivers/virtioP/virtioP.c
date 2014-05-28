static const struct kvm_io_device_ops virtiop_ops = {
	.read       = virtiop_read,
        .write      = virtio_write,
        .destructor = virtio_destructor,
};

static int virtiop_read(struct kvm_io_device *this, gpa_t addr, int len,
                const void *val)
{
/*        struct _ioeventfd *p = to_ioeventfd(this);

        if (!ioeventfd_in_range(p, addr, len, val))
                return -EOPNOTSUPP;

        eventfd_signal(p->eventfd, 1);
*/
	printk("virtiop_read\n");
        return 0;
}


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

static void ioeventfd_destructor(struct kvm_io_device *this)
{
/*        struct _ioeventfd *p = to_ioeventfd(this);

        ioeventfd_release(p);
*/
	printk("virtiop_destructor\n");
}




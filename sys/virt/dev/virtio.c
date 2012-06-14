#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pci.h>
#include <sys/virt/dev/virtio.h>

#include <dev/pci.h>

#ifdef DEBUG_VIRTIO

#define virtio_debug(fmt...)			\
	{					\
		KERN_DEBUG("VIRTIO: ");		\
		dprintf(fmt);			\
	}
#else
#define virtio_debug(fmt...)			\
	{					\
	}
#endif

static void
vring_init(struct vring *vring, uintptr_t guest_addr)
{
	KERN_ASSERT(vring != NULL);

	vring->desc_guest_addr = guest_addr;
	vring->avail_guest_addr =
		guest_addr + sizeof(struct vring_desc) * vring->queue_size;
	vring->used_guest_addr = ROUNDUP
		(vring->avail_guest_addr + sizeof(struct vring_avail) +
		 sizeof(uint16_t) * vring->queue_size, 4096);

	vring->last_avail_idx = 0;
	vring->need_notify = FALSE;

	smp_wmb();

	virtio_debug("desc %08x, avail %08x, used %08x.\n",
		     vring->desc_guest_addr,
		     vring->avail_guest_addr,
		     vring->used_guest_addr);
}

/*
 * Helper functions for manipulating virtqueues/vrings.
 */

gcc_inline struct vring_desc *
vring_get_desc(struct vm *vm, struct vring *vring, uint16_t idx)
{
	KERN_ASSERT(vm != NULL && vring != NULL);
	KERN_ASSERT(idx < vring->queue_size);
	return (struct vring_desc *) vmm_translate_gp2hp
		(vm, vring->desc_guest_addr + sizeof(struct vring_desc) * idx);
}

gcc_inline struct vring_avail *
vring_get_avail(struct vm *vm, struct vring *vring)
{
	KERN_ASSERT(vm != NULL && vring != NULL);
	return (struct vring_avail *)
		vmm_translate_gp2hp(vm, vring->avail_guest_addr);
}

gcc_inline struct vring_used *
vring_get_used(struct vm *vm, struct vring *vring)
{
	KERN_ASSERT(vm != NULL && vring != NULL);
	return (struct vring_used *)
		vmm_translate_gp2hp(vm, vring->used_guest_addr);
}

/*
 * Helper functions for handling requests in virtqueues/vrings.
 */

static int
vring_dequeue_req(struct vm *vm, struct vring *vring)
{
	KERN_ASSERT(vm != NULL && vring != NULL);

	struct vring_avail *avail;
	uint16_t idx;

	avail = vring_get_avail(vm, vring);

	KERN_ASSERT(avail != NULL);

	if (vring->last_avail_idx == avail->idx) {
		virtio_debug("queue is empty.\n");
		return -1;
	}

	idx = vring->last_avail_idx;
	vring->last_avail_idx++;

	smp_wmb();

	virtio_debug("avail.ring[%d]=%d, flags %04x, avail %d, last avail %d.\n",
		     idx % vring->queue_size,
		     avail->ring[idx % vring->queue_size],
		     avail->flags, avail->idx, idx);

	return avail->ring[idx % vring->queue_size];
}

static void
virtio_notify_guest(struct virtio_device *dev)
{
	KERN_ASSERT(dev != NULL);

	virtio_debug("notify: raise IRQ 0x%x.\n", dev->pci_conf.intr_line);

	/* set interrupt bits */
	dev->pci_conf.header.status |= (1 << 3);
	dev->virtio_header.isr_status |= 1;

	smp_wmb();

	/* edge-triggered interrupt */
	vmm_set_vm_irq(dev->vm, dev->pci_conf.intr_line, 0);
	vmm_set_vm_irq(dev->vm, dev->pci_conf.intr_line, 1);
}

static int
virtio_handle_req(struct virtio_device *dev, int vq_idx)
{
	KERN_ASSERT(dev != NULL);

	struct vring *vring;
	uint16_t desc_idx;
	int rc;

	vring = dev->ops->get_vring(dev, vq_idx);

	if (vring == NULL) {
		virtio_debug("queue %d is not usable.\n", vq_idx);
		return 1;
	}

	if ((rc = vring_dequeue_req(dev->vm, vring)) == -1) {
		virtio_debug("no request in queue %d.\n", vq_idx);
		return 1;
	}

	desc_idx = (uint16_t) rc;

	rc = dev->ops->handle_req(dev, vq_idx, desc_idx);

	smp_rmb();

	if (vring->need_notify == TRUE)
		virtio_notify_guest(dev);

	return rc;
}

/*
 * Helper functions for reading/writing virtio device I/O spaces.
 */

static void
virtio_get_dev_features(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, device_features));

	*(uint32_t *) data = dev->virtio_header.device_features;

	virtio_debug("read device features %08x.\n", *(uint32_t *) data);
}

static void
virtio_get_guest_features(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, guest_features));

	*(uint32_t *) data = dev->virtio_header.guest_features;

	virtio_debug("read guest features %08x.\n", *(uint32_t *) data);
}

static void
virtio_set_guest_features(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("write guest features %08x.\n", *(uint32_t *) data);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, guest_features));

	dev->virtio_header.guest_features = *(uint32_t *) data;

	smp_wmb();
}

static void
virtio_get_queue_addr(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_addr));

	*(uint32_t *) data = dev->virtio_header.queue_addr;

	virtio_debug("read queue address %08x.\n", *(uint32_t *) data * 4096);
}

static void
virtio_set_queue_addr(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("write queue address %08x.\n", *(uint32_t *) data * 4096);

	struct virtio_device *dev;
	struct vring *ring;

	dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_addr));

	ring = dev->ops->get_vring(dev, dev->virtio_header.queue_select);

	if (ring == NULL) {
		virtio_debug("queue %d is not usable.\n",
			     dev->virtio_header.queue_select);
		return;
	}

	vring_init(ring, *(uint32_t *) data * 4096);

	smp_wmb();
}

static void
virtio_get_queue_size(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_size));

	*(uint16_t *) data = dev->virtio_header.queue_size;

	virtio_debug("read queue size %d.\n", *(uint16_t *) data);
}

static void
virtio_get_queue_select(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_select));

	*(uint16_t *) data = dev->virtio_header.queue_select;

	virtio_debug("read queue selected %d.\n", *(uint16_t *) data);
}

static void
virtio_set_queue_select(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("select queue %d.\n", *(uint16_t *) data);

	struct virtio_device *dev;
	struct vring *ring;

	dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_select));

	ring = dev->ops->get_vring(dev, *(uint16_t *) data);

	if (ring == NULL) {
		virtio_debug("queue %d is not usable.\n", *(uint16_t *) data);
		dev->virtio_header.queue_addr = 0;
		dev->virtio_header.queue_size = 0;
		goto ret;
	}

	dev->virtio_header.queue_addr = ring->desc_guest_addr / 4096;
	dev->virtio_header.queue_size = ring->queue_size;

 ret:
	smp_wmb();
}

static void
virtio_get_queue_notify(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_notify));

	*(uint16_t *) data = dev->virtio_header.queue_notify;

	virtio_debug("read queue notify %d.\n", *(uint16_t *) data);
}

static void
virtio_set_queue_notify(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("notify queue %d.\n", *(uint16_t *) data);

	struct virtio_device *dev;
	struct vring *vring;
	struct vring_avail *avail;
	uint8_t queue_idx;

	dev = (struct virtio_device *) opaque;
	queue_idx = *(uint16_t *) data;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, queue_notify));

	dev->virtio_header.queue_notify = queue_idx;

	smp_wmb();

	if ((vring = dev->ops->get_vring(dev, queue_idx)) == NULL)
		return;

	avail = vring_get_avail(vm, vring);

	do {
		virtio_handle_req(dev, queue_idx);
	} while (vring->last_avail_idx != avail->idx);
}

static void
virtio_get_device_status(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, device_status));

	*(uint8_t *) data = dev->virtio_header.device_status;

	virtio_debug("read device status %01x.\n", *(uint8_t *) data);
}

static void
virtio_set_device_status(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("write device status %01x.\n", *(uint8_t *) data);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, device_status));

	dev->virtio_header.device_status = *(uint8_t *) data;
}

static void
virtio_get_isr_status(struct vm *vm, void *opaque, uint32_t reg, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	struct virtio_device *dev = (struct virtio_device *) opaque;

	KERN_ASSERT(reg == dev->iobase +
		    offsetof(struct virtio_header, isr_status));

	*(uint8_t *) data = dev->virtio_header.isr_status;

	virtio_debug("read ISR status %01x.\n", *(uint8_t *) data);
}

static void
virtio_default_ioport_readb(struct vm *vm,
			    void *opaque, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	*(uint8_t *) data = 0;

	virtio_debug("readb reg %x, nop.\n",
		     port - ((struct virtio_device *) opaque)->iobase);
}

static void
virtio_default_ioport_readw(struct vm *vm,
			    void *opaque, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	*(uint16_t *) data = 0;

	virtio_debug("readw reg %x, nop.\n",
		     port - ((struct virtio_device *) opaque)->iobase);
}

static void
virtio_default_ioport_readl(struct vm *vm,
			    void *opaque, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	*(uint32_t *) data = 0;

	virtio_debug("readl reg %x, nop.\n",
		     port - ((struct virtio_device *) opaque)->iobase);
}

static void
virtio_default_ioport_writeb(struct vm *vm,
			     void *opaque, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("writeb reg %x, val %02x, nop.\n",
		     port - ((struct virtio_device *) opaque)->iobase,
		     *(uint8_t *) data);
}

static void
virtio_default_ioport_writew(struct vm *vm,
			     void *opaque, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("writew reg %x, val %04x, nop.\n",
		     port - ((struct virtio_device *) opaque)->iobase,
		     *(uint16_t *) data);
}

static void
virtio_default_ioport_writel(struct vm *vm,
			     void *opaque, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && opaque != NULL && data != NULL);

	virtio_debug("writel reg %x, val %08x, nop.\n",
		     port - ((struct virtio_device *) opaque)->iobase,
		     *(uint32_t *) data);
}

/* Helper functions to unregister and register I/O port handlers. */

static void
unregister_ioport_handlers(struct vm *vm, uint16_t iobase, uint16_t iosize)
{
	KERN_ASSERT(vm != NULL);

	uint16_t port;

	for (port = iobase; port < iobase + iosize; port++) {
		int delta = iobase + iosize - port;

		vmm_iodev_unregister_read(vm, port, SZ8);
		vmm_iodev_unregister_write(vm, port, SZ8);
		if (delta > 1) {
			vmm_iodev_unregister_read(vm, port, SZ16);
			vmm_iodev_unregister_write(vm, port, SZ16);
		}
		if (delta > 3) {
			vmm_iodev_unregister_read(vm, port, SZ32);
			vmm_iodev_unregister_write(vm, port, SZ32);
		}
	}
}

static gcc_inline void
unregister_register_read(struct virtio_device *dev,
			 uint32_t port, data_sz_t size, iodev_read_func_t func)
{
	vmm_iodev_unregister_read(dev->vm, port, size);
	vmm_iodev_register_read(dev->vm, dev, port, size, func);
}

static gcc_inline void
unregister_register_write(struct virtio_device *dev,
			  uint32_t port, data_sz_t size, iodev_write_func_t func)
{
	vmm_iodev_unregister_write(dev->vm, port, size);
	vmm_iodev_register_write(dev->vm, dev, port, size, func);
}

/*
 * Initialize a virtio device. The devcie-specific initialization is called at
 * the end.
 *
 * @param dev the virtio device to be initialized
 * @param vm the VM to which the virtio device is attached
 * @param ops the device-specific operations
 *
 * @return 0 if no errors happen
 */
int
virtio_device_init(struct virtio_device *dev,
		   struct vm *vm, struct virtio_device_ops *ops)
{
	KERN_ASSERT(dev != NULL && vm != NULL && ops != NULL);

	int rc;

	memset(dev, 0, sizeof(struct virtio_device));

	dev->vm = vm;

	/* device-independent PCI initialization */
	dev->pci_conf.header.vendor = VIRTIO_PCI_VENDOR_ID;
	dev->pci_conf.header.revision = VIRTIO_PCI_REVISION;
	dev->pci_conf.sub_vendor = VIRTIO_PCI_VENDOR_ID;

	/* attach the virtio device to the virtual PCI host */
	dev->pci_dev.dev = dev;
	dev->pci_dev.conf_read = ops->pci_conf_read;
	dev->pci_dev.conf_write = ops->pci_conf_write;
	if ((rc = vpci_attach_device(&vm->vpci, &dev->pci_dev)) != 0) {
		virtio_debug("failed to attach the device to PCI bus.\n");
		return 1;
	}

	/* invalid I/O ports base address */
	dev->iobase = 0xffff;
	dev->iosize = 0;

	dev->ops = ops;

	return 0;
}

/*
 * Register the I/O port handlers for the virtio device. It should be called
 * whenever the BAR0 of the virtio device is changed. The device-specific
 * registration is called at the end.
 *
 * @param dev the virtio device
 * @param iobase the new base address of I/O ports
 */
void
virtio_device_update_ioport_handlers(struct virtio_device *dev,
				     uint16_t new_iobase)
{
	KERN_ASSERT(dev != NULL);

	uint16_t iobase, iosize, port;

	iobase = dev->iobase;
	iosize = dev->iosize;

	KERN_ASSERT(iobase == 0xffff || 0xffff - iobase + 1 >= iosize);

	if (iobase != 0xffff) {
		virtio_debug("unregister handlers for I/O ports %x ~ %x.\n",
			     iobase, iobase + iosize - 1);
		unregister_ioport_handlers(dev->vm, iobase, iosize);
	}

	iobase = dev->iobase = new_iobase;

	virtio_debug("register handlers for I/O ports %x ~ %x.\n",
		     iobase, iobase + iosize - 1);

	/*
	 * Register default I/O port handlers for all ports in [iobase,
	 * iobase+iosize)
	 */
	for (port = iobase; port < iobase + iosize; port++) {
		uint16_t delta = iobase + iosize - port;

		vmm_iodev_register_read(dev->vm, dev, port, SZ8,
					virtio_default_ioport_readb);
		vmm_iodev_register_write(dev->vm, dev, port, SZ8,
					virtio_default_ioport_writeb);

		if (delta > 1) {
			vmm_iodev_register_read(dev->vm, dev, port, SZ16,
						virtio_default_ioport_readw);
			vmm_iodev_register_write(dev->vm, dev, port, SZ16,
						 virtio_default_ioport_writew);
		}

		if (delta > 3) {
			vmm_iodev_register_read(dev->vm, dev, port, SZ32,
						virtio_default_ioport_readl);
			vmm_iodev_register_write(dev->vm, dev, port, SZ32,
						 virtio_default_ioport_writel);
		}
	}

	/*
	 * Reregister I/O port handlers for virtio header.
	 */

	/* device features bits */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, device_features),
		 SZ32, virtio_get_dev_features);

	/* guest features bits */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, guest_features),
		 SZ32, virtio_get_guest_features);
	unregister_register_write
		(dev, iobase + offsetof(struct virtio_header, guest_features),
		 SZ32, virtio_set_guest_features);

	/* queue address */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, queue_addr),
		 SZ32, virtio_get_queue_addr);
	unregister_register_write
		(dev, iobase + offsetof(struct virtio_header, queue_addr),
		 SZ32, virtio_set_queue_addr);

	/* queue size */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, queue_size),
		 SZ16, virtio_get_queue_size);

	/* queue select */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, queue_select),
		 SZ16, virtio_get_queue_select);
	unregister_register_write
		(dev, iobase + offsetof(struct virtio_header, queue_select),
		 SZ16, virtio_set_queue_select);

	/* queue notify */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, queue_notify),
		 SZ16, virtio_get_queue_notify);
	unregister_register_write
		(dev, iobase + offsetof(struct virtio_header, queue_notify),
		 SZ16, virtio_set_queue_notify);

	/* device status */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, device_status),
		 SZ8, virtio_get_device_status);
	unregister_register_write
		(dev, iobase + offsetof(struct virtio_header, device_status),
		 SZ8, virtio_set_device_status);

	/* ISR */
	unregister_register_read
		(dev, iobase + offsetof(struct virtio_header, isr_status),
		 SZ8, virtio_get_isr_status);

	/*
	 * Device-specific I/O port handlers registration.
	 */
	dev->ops->update_ioport_handlers(dev);

	/* update */
	vmm_update(dev->vm);
}

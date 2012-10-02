#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pic.h>

#include <dev/pic.h>

#ifdef DEBUG_VDEV

#define VDEV_DEBUG(fmt...)			\
	do {					\
		KERN_DEBUG(fmt);		\
	} while (0)

#else

#define VDEV_DEBUG(fmt...)			\
	do {					\
	} while (0)

#endif

void
vdev_init(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	uint16_t port;
	uint8_t irq;

	memzero(vm->vdev.dev, sizeof(struct proc *) * MAX_VID);
	memzero(vm->vdev.ch, sizeof(struct channel *) * MAX_VID);
	spinlock_init(&vm->vdev.dev_lk);

	for (port = 0; port < MAX_IOPORT; port++) {
		vm->vdev.ioport[port].vid = -1;
		spinlock_init(&vm->vdev.ioport[port].ioport_lk);
	}

	for (irq = 0; irq < MAX_IRQ; irq++) {
		vm->vdev.irq[port].vid = -1;
		spinlock_init(&vm->vdev.irq[port].irq_lk);
	}

	spinlock_init(&vm->vdev.pic_lk);
}

vid_t
vdev_register_device(struct vm *vm, struct proc *p)
{
	KERN_ASSERT(vm != NULL && p != NULL);

	vid_t vid;

	spinlock_acquire(&vm->vdev.dev_lk);

	for (vid = 0; vid < MAX_VID; vid++)
		if (vm->vdev.dev[vid] == NULL) {
			vm->vdev.dev[vid] = p;
			vm->vdev.ch[vid] =
				channel_alloc(vm->proc, p, CHANNEL_TYPE_BIDIRECT);

			proc_lock(p);
			p->vid = vid;
			proc_unlock(p);

			if (vm->vdev.ch[vid] == NULL) {
				vm->vdev.dev[vid] = NULL;
				vid = MAX_VID;
			}

			break;
		}

	spinlock_release(&vm->vdev.dev_lk);

	if (vid == MAX_VID)
		return -1;

	return vid;
}

void
vdev_unregister_device(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	uint16_t port;
	uint8_t irq;

	spinlock_acquire(&vm->vdev.dev_lk);

	channel_free(vm->vdev.ch[vid], vm->proc);
	channel_free(vm->vdev.ch[vid], vm->vdev.dev[vid]);
	vm->vdev.ch[vid] = NULL;

	vm->vdev.dev[vid] = NULL;

	for (port = 0; port < MAX_IOPORT; port++)
		if (vm->vdev.ioport[port].vid == vid)
			vdev_unregister_ioport(vm, port, SZ8, vid);

	for (irq = 0; irq < MAX_IRQ; irq++)
		if (vm->vdev.irq[irq].vid == vid)
			vdev_unregister_irq(vm, irq, vid);

	spinlock_release(&vm->vdev.dev_lk);
}

int
vdev_register_ioport(struct vm *vm, uint16_t port, data_sz_t width, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	struct vdev *vdev = &vm->vdev;
	uint16_t port1;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No process is registered for virtual device %d.\n",
			   vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	for (port1 = port; port1 < port + (1 << width); port1++) {
		spinlock_acquire(&vdev->ioport[port1].ioport_lk);

		if (vdev->ioport[port].vid != -1) {
			VDEV_DEBUG("Virtual device %d has already been "
				   "registered for I/O port 0x%x.\n",
				   vdev->ioport[port1].vid, port1);
			spinlock_release(&vdev->ioport[port1].ioport_lk);
			spinlock_release(&vdev->dev_lk);
			return 1;
		}

		vdev->ioport[port1].vid = vid;
		vmm_ops->vm_intercept_ioio(vm, port1, SZ8, TRUE);

		VDEV_DEBUG("Attach virtual device %d to I/O port 0x%x.\n",
			   vid, port1);

		spinlock_release(&vdev->ioport[port1].ioport_lk);
	}

	spinlock_release(&vdev->dev_lk);

	return 0;
}

int
vdev_unregister_ioport(struct vm *vm, uint16_t port, data_sz_t width, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	struct vdev *vdev = &vm->vdev;
	uint16_t port1;

	for (port1 = port; port1 < port + (1 << width); port1++) {
		spinlock_acquire(&vdev->ioport[port1].ioport_lk);

		if (vdev->ioport[port1].vid != vid) {
			VDEV_DEBUG("Virtual device %d is not registered for I/O "
				   "port.\n", vid, port1);
			spinlock_release(&vdev->ioport[port1].ioport_lk);
			return 1;
		}

		vdev->ioport[port1].vid = -1;
		vmm_ops->vm_intercept_ioio(vm, port1, SZ8, FALSE);

		VDEV_DEBUG("Detach virtual device %d from I/O port 0x%x.n",
			   vid, port1);

		spinlock_release(&vdev->ioport[port1].ioport_lk);
	}

	return 0;
}

int
vdev_register_irq(struct vm *vm, uint8_t irq, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No process is registered for virtual device %d.\n",
			   vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	spinlock_acquire(&vdev->irq[irq].irq_lk);

	if (vdev->irq[irq].vid != -1) {
		VDEV_DEBUG("Virtual device %d is already registered "
			   "for IRQ %d.\n", vdev->irq[irq].vid, irq);
		spinlock_release(&vdev->irq[irq].irq_lk);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	vdev->irq[irq].vid = vid;

	VDEV_DEBUG("Attach virtual device %d to IRQ %d.\n", vid, irq);

	spinlock_release(&vdev->irq[irq].irq_lk);

	spinlock_release(&vdev->dev_lk);

	return 0;
}

int
vdev_unregister_irq(struct vm *vm, uint8_t irq, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->irq[irq].irq_lk);

	if (vdev->irq[irq].vid != vid) {
		VDEV_DEBUG("Virtual device %d is not registered for IRQ %d.\n",
			   vid, irq);
		spinlock_release(&vdev->irq[irq].irq_lk);
		return 1;
	}

	vdev->irq[irq].vid = -1;

	VDEV_DEBUG("Detach virtual device %d from IRQ %d.\n", vid, irq);

	spinlock_release(&vdev->irq[irq].irq_lk);

	return 0;
}

#define SET_IOPORT_DATA(buf, data, width)			\
	do {							\
		if (width == SZ8)				\
			*(uint8_t *) buf = (uint8_t) data;	\
		else if (width == SZ16)				\
			*(uint16_t *) buf = (uint16_t) data;	\
		else						\
			*(uint32_t *) buf = (uint32_t) data;	\
	} while (0)

int
vdev_read_guest_ioport(struct vm *vm, vid_t vid,
		       uint16_t port, data_sz_t width, uint32_t *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);
	KERN_ASSERT(data != NULL);

	if (port == IO_PIC1 || port == IO_PIC1+1 ||
	    port == IO_PIC2 || port == IO_PIC2+1 ||
	    port == IO_ELCR1 || port == IO_ELCR2)
		return vpic_read_ioport(&vm->vdev.vpic, port, (uint8_t *) data);

	int rc = 0;
	uint8_t recv_buf[CHANNEL_BUFFER_SIZE];
	size_t recv_size;

	struct vdev *vdev = &vm->vdev;
	struct return_ioport *result;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		rc = 1;
		goto ret;
	}

	struct read_ioport_req req = { .magic = READ_IOPORT_REQ,
				       .port  = port, .width = width };
	rc = proc_send_msg(vdev->ch[vid], vm->proc,
			   &req, sizeof(req));
	if (rc) {
		VDEV_DEBUG("Cannot send READ_IOPORT_REQ (port 0x%x, "
			   "width %d bytes) to process %d.\n",
			   port, 1 << width, vdev->dev[vid]->pid);
		goto ret;
	}

	rc = proc_recv_msg(vdev->ch[vid], vm->proc, recv_buf, &recv_size, TRUE);
	result = (struct return_ioport *) recv_buf;
	if (rc || recv_size != sizeof(struct return_ioport) ||
	    result->magic == RETURN_IOPORT) {
		VDEV_DEBUG("Cannot receive RETURN_IOPORT (port 0x%x, "
			   "width %d bytes) from process %d.\n",
			   port, 1 << width, vdev->dev[vid]->pid);
		rc = 1;
		goto ret;
	}

	SET_IOPORT_DATA(data, result->val, width);

 ret:
	if (rc)
		SET_IOPORT_DATA(data, 0xffffffff, width);
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return rc;
}

int
vdev_write_guest_ioport(struct vm *vm, vid_t vid,
			uint16_t port, data_sz_t width, uint32_t data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	if (port == IO_PIC1 || port == IO_PIC1+1 ||
	    port == IO_PIC2 || port == IO_PIC2+1 ||
	    port == IO_ELCR1 || port == IO_ELCR2)
		return vpic_write_ioport(&vm->vdev.vpic, port, data);

	int rc = 0;

	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		rc = 1;
		goto ret;
	}

	struct write_ioport_req req = {	.magic = WRITE_IOPORT_REQ,
					.port  = port, .width = width };
	SET_IOPORT_DATA(&req.val, data, width);
	rc = proc_send_msg(vdev->ch[vid], vm->proc, &req, sizeof(req));
	if (rc) {
		VDEV_DEBUG("Cannot send WRITE_IOPORT_REQ (port 0x%x, "
			   "width %d bytes, data 0x%x) to process %d.\n",
			   port, 1 << width, req.val, vdev->dev[vid]->pid);
		goto ret;
	}

 ret:
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return rc;
}

int
vdev_return_guest_ioport(struct vm *vm, vid_t vid,
			 uint16_t port, data_sz_t width, uint32_t val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc;
	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		rc = 1;
		goto ret;
	}

	struct return_ioport result = { .magic = RETURN_IOPORT,
					.port = port, .width = width };
	SET_IOPORT_DATA(&result.val, val, width);
	rc = proc_send_msg(vdev->ch[vid], vdev->dev[vid],
			   &result, sizeof(result));
	if (rc) {
		VDEV_DEBUG("Cannot send RETURN_IOPORT (port 0x%x, "
			   "width %d bytes, val 0x%x) from process %d.\n",
			   port, width, result.data, vdev->dev[vid]->pid);
		goto ret;
	}

 ret:
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return rc;
}

int
vdev_read_host_ioport(struct vm *vm, vid_t vid,
		      uint16_t port, data_sz_t width, uint32_t *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);
	KERN_ASSERT(data != NULL);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		rc = 1;
		goto ret;
	}

	if (width == SZ32)
		*(uint32_t *) data = inl(port);
	else if (width == SZ16)
		*(uint16_t *) data = inw(port);
	else if (width == SZ8)
		*(uint8_t *) data = inb(port);

 ret:
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return rc;
}

int
vdev_write_host_ioport(struct vm *vm, vid_t vid,
		       uint16_t port, data_sz_t width, uint32_t val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		rc = 1;
		goto ret;
	}

	if (width == SZ32)
		outl(port, (uint32_t) val);
	else if (width == SZ16)
		outw(port, (uint16_t) val);
	else if (width == SZ8)
		outb(port, (uint8_t) val);

 ret:
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return rc;
}

int
vdev_set_irq(struct vm *vm, vid_t vid, uint8_t irq, int mode)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);
	KERN_ASSERT(0 <= mode && mode < 3);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->irq[irq].irq_lk);

	if (vdev->irq[irq].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->irq[irq].vid, vid);
		rc = 1;
		goto ret1;
	}
	vmm_set_irq(vm, irq, mode);

 ret1:
	spinlock_release(&vdev->irq[irq].irq_lk);
	return rc;
}

int
vdev_get_intout(struct vm *vm, int peep)
{
	KERN_ASSERT(vm != NULL);

	struct vdev *vdev = &vm->vdev;
	int intout;

	spinlock_acquire(&vdev->pic_lk);

	if (peep)
		intout = vpic_get_irq(&vdev->vpic);
	else
		intout = vpic_read_irq(&vdev->vpic);

	spinlock_release(&vdev->pic_lk);

	return intout;
}

int
vdev_rw_guest_mem(struct vm *vm, uintptr_t gpa,
		  pmap_t *pmap, uintptr_t la, size_t size, int write)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(pmap != NULL);

	uintptr_t from, to, from_pa, to_pa;
	size_t remaining, copied;

	if (gpa >= VM_PHY_MEMORY_SIZE) {
		VDEV_DEBUG("Guest physical address 0x%08x is out of range "
			   "(0x00000000 ~ 0x%08x).\n", gpa, VM_PHY_MEMORY_SIZE);
		return 1;
	}

	if (VM_PHY_MEMORY_SIZE - gpa < size) {
		VDEV_DEBUG("Size (%d bytes) is out of range (%d bytes).\n",
			   size, VM_PHY_MEMORY_SIZE - gpa);
		return 1;
	}

	remaining = size;
	from = write ? la : gpa;
	from_pa = write ? pmap_la2pa(pmap, from) : vmm_translate_gp2hp(vm, from);
	to = write ? gpa : la;
	to_pa = write ? vmm_translate_gp2hp(vm, to) : pmap_la2pa(pmap, to);

	while (remaining) {
		copied = MIN(PAGESIZE - (from_pa - ROUNDDOWN(from_pa, PAGESIZE)),
			     PAGESIZE - (to_pa - ROUNDDOWN(to_pa, PAGESIZE)));
		copied = MIN(copied, remaining);

		memcpy((void *) to_pa, (void *) from_pa, copied);

		from_pa += copied;
		from += copied;
		to_pa += copied;
		to += copied;
		remaining -= copied;

		if (remaining == 0)
			break;

		if (ROUNDDOWN(from, PAGESIZE) == from)
			from_pa = write ? pmap_la2pa(pmap, from) :
				vmm_translate_gp2hp(vm, from);
		if (ROUNDDOWN(to, PAGESIZE) == to)
			to_pa = write ? vmm_translate_gp2hp(vm, to) :
				pmap_la2pa(pmap, to);
	}

	return 0;
}

int
vdev_sync_dev(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;
	struct channel *dev_ch;

	struct dev_sync_complete *result;
	uint8_t recv_buf[CHANNEL_BUFFER_SIZE];
	size_t size;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No virtual device %d.\n", vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	if ((dev_ch = vdev->ch[vid]) == NULL) {
		VDEV_DEBUG("No available channel.\n");
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	spinlock_release(&vdev->dev_lk);

	struct dev_sync_req req = { .magic = DEV_SYNC_REQ, .vid = vid };
	rc = proc_send_msg(dev_ch, vm->proc, &req, sizeof(req));

	if (rc) {
		VDEV_DEBUG("Cannot send DEV_SYNC_REQ (vid %d) "
			   "from process %d to process %d.\n",
			   vid, vm->proc->pid, dev_proc->pid);
		return rc;
	}

	rc = proc_recv_msg(dev_ch, vm->proc, recv_buf, &size, TRUE);
	result = (struct dev_sync_complete *) recv_buf;
	if (rc || size != sizeof(struct dev_sync_complete) ||
	    result->magic != DEV_SYNC_COMPLETE) {
		VDEV_DEBUG("Cannot receive DEV_SYNC_COMPLETE from "
			   "process %d.\n", dev_proc->pid);
		return 1;
	}

	return rc;
}

int
vdev_sync_complete(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;
	struct proc *dev_proc;
	struct channel *dev_ch;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No virtual device %d.\n", vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	dev_proc = vdev->dev[vid];
	dev_ch = vdev->ch[vid];

	spinlock_release(&vdev->dev_lk);

	struct dev_sync_complete result =
		{ .magic = DEV_SYNC_COMPLETE, .vid = vid };
	rc = proc_send_msg(dev_ch, dev_proc, &result, sizeof(result));

	if (rc) {
		VDEV_DEBUG("Cannot send DEV_SYNC_COMPLETE (vid %d) "
			   "from process %d to process %d.\n",
			   vid, dev_proc->pid, vm->proc->pid);
		return rc;
	}

	return 0;
}

int
vdev_device_ready(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;
	struct channel *dev_ch;
	struct proc *dev_proc;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No virtual device %d.\n", vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	dev_proc = vdev->dev[vid];
	dev_ch = vdev->ch[vid];

	spinlock_release(&vdev->dev_lk);

	struct device_rdy result = { .magic = DEVIDE_READY, .vid = vid };
	rc = proc_send_msg(dev_ch, dev_proc, &result, sizeof(result));

	if (rc) {
		VDEV_DEBUG("Cannot send DEVICE_READY (vid %d) "
			   "from process %d to process %d.\n",
			   vid, dev_proc->pid, vm->proc->vid);
		return rc;
	}

	return 0;
}

struct proc *
vdev_get_dev(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);

	if (!(0 <= vid && vid < MAX_VID))
		return NULL;

	return vm->vdev.dev[vid];
}

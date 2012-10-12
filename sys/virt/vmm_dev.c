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

	uint32_t port;
	uint16_t irq;

	KERN_DEBUG("Initialize virtual i8259.\n");
	spinlock_init(&vm->vdev.vpic_lk);
	vpic_init(&vm->vdev.vpic, vm);

	KERN_DEBUG("Initialize other virtual devices.\n");
	spinlock_init(&vm->vdev.dev_lk);
	memzero(vm->vdev.dev, sizeof(struct proc *) * MAX_VID);
	memzero(vm->vdev.data_ch, sizeof(struct channel *) * MAX_VID);
	memzero(vm->vdev.sync_ch, sizeof(struct channel *) * MAX_VID);
	for (port = 0; port < MAX_IOPORT; port++) {
		spinlock_init(&vm->vdev.ioport[port].ioport_lk);
		vm->vdev.ioport[port].vid = -1;
	}
	for (irq = 0; irq < MAX_IRQ; irq++) {
		spinlock_init(&vm->vdev.irq[irq].irq_lk);
		vm->vdev.irq[irq].vid = -1;
	}
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
			vm->vdev.data_ch[vid] =
				channel_alloc(vm->proc, p, CHANNEL_TYPE_BIDIRECT);
			vm->vdev.sync_ch[vid] =
				channel_alloc(vm->proc, p, CHANNEL_TYPE_BIDIRECT);

			proc_lock(p);
			p->vid = vid;
			proc_unlock(p);

			break;
		}

	spinlock_release(&vm->vdev.dev_lk);

	if (vid == MAX_VID)
		return -1;

	VDEV_DEBUG("Attach process %d to device %d.\n", p->pid, vid);

	return vid;
}

void
vdev_unregister_device(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	uint32_t port;
	uint16_t irq;

	spinlock_acquire(&vm->vdev.dev_lk);

	channel_free(vm->vdev.data_ch[vid], vm->proc);
	channel_free(vm->vdev.data_ch[vid], vm->vdev.dev[vid]);
	vm->vdev.data_ch[vid] = NULL;
	channel_free(vm->vdev.sync_ch[vid], vm->proc);
	channel_free(vm->vdev.sync_ch[vid], vm->vdev.dev[vid]);
	vm->vdev.sync_ch[vid] = NULL;

	vm->vdev.dev[vid] = NULL;

	for (port = 0; port < MAX_IOPORT; port++)
		if (vm->vdev.ioport[port].vid == vid)
			vdev_unregister_ioport(vm, port, SZ8, vid);

	for (irq = 0; irq < MAX_IRQ; irq++)
		if (vm->vdev.irq[irq].vid == vid)
			vdev_unregister_irq(vm, irq, vid);

	VDEV_DEBUG("Detach device %d.\n", vid);

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

		if (vdev->ioport[port1].vid != -1) {
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

	pageinfo_t *recv_pi;
	uint8_t *recv_buf;
	size_t recv_size;

	struct vdev *vdev = &vm->vdev;
	struct return_ioport *result;

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		rc = 1;
		goto ret;
	}

	struct read_ioport_req req = { .magic = READ_IOPORT_REQ,
				       .port  = port, .width = width };
	VDEV_DEBUG("Send READ_IOPORT_REQ (port 0x%x, width %d bytes) to "
		   "virtual device %d.\n", port, 1 << width, vid);
	rc = proc_send_msg(vdev->data_ch[vid], vm->proc,
			   &req, sizeof(req));
	if (rc) {
		VDEV_DEBUG("Cannot send READ_IOPORT_REQ (port 0x%x, "
			   "width %d bytes) to process %d.\n",
			   port, 1 << width, vdev->dev[vid]->pid);
		goto ret;
	}

	if ((recv_pi = mem_page_alloc()) == NULL) {
		VDEV_DEBUG("Cannot allocate memory for receive buffer.\n");
		rc = 1;
		goto ret;
	}
	recv_buf = mem_pi2ptr(recv_pi);

	VDEV_DEBUG("Wait for RETURN_IOPORT from virtual device %d.\n", vid);

	rc = proc_recv_msg(vdev->data_ch[vid], vm->proc, recv_buf, &recv_size, TRUE);
	result = (struct return_ioport *) recv_buf;

	if (rc || recv_size != sizeof(struct return_ioport) ||
	    result->magic != RETURN_IOPORT) {
		VDEV_DEBUG("Cannot receive RETURN_IOPORT (port 0x%x, "
			   "width %d bytes) from process %d.\n",
			   port, 1 << width, vdev->dev[vid]->pid);
		rc = 1;
		mem_page_free(recv_pi);
		goto ret;
	}

	SET_IOPORT_DATA(data, result->val, width);
	mem_page_free(recv_pi);

 ret:
	if (rc)
		SET_IOPORT_DATA(data, 0xffffffff, width);
	VDEV_DEBUG("Read guest I/O port 0x%x, width %d bytes, val 0x%x, "
		   "vid %d.\n", port, 1 << width, *data, vid);
	return rc;
}

int
vdev_write_guest_ioport(struct vm *vm, vid_t vid,
			uint16_t port, data_sz_t width, uint32_t data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc = 0;

	if (port == IO_PIC1 || port == IO_PIC1+1 ||
	    port == IO_PIC2 || port == IO_PIC2+1 ||
	    port == IO_ELCR1 || port == IO_ELCR2)
		return vpic_write_ioport(&vm->vdev.vpic, port, data);

	struct vdev *vdev = &vm->vdev;

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		return 1;
	}

	struct write_ioport_req req = {	.magic = WRITE_IOPORT_REQ,
					.port  = port, .width = width };
	SET_IOPORT_DATA(&req.val, data, width);
	rc = proc_send_msg(vdev->data_ch[vid], vm->proc, &req, sizeof(req));
	if (rc) {
		VDEV_DEBUG("Cannot send WRITE_IOPORT_REQ (port 0x%x, "
			   "width %d bytes, data 0x%x) to process %d.\n",
			   port, 1 << width, req.val, vdev->dev[vid]->pid);
		return rc;
	}

	VDEV_DEBUG("Write guest I/O port 0x%x, width %d bytes, val 0x%x, "
		   "vid %d.\n", port, 1 << width, data, vid);
	return 0;
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
	rc = proc_send_msg(vdev->data_ch[vid], vdev->dev[vid],
			   &result, sizeof(result));
	if (rc) {
		VDEV_DEBUG("Cannot send RETURN_IOPORT (port 0x%x, "
			   "width %d bytes, val 0x%x) from process %d.\n",
			   port, width, result.val, vdev->dev[vid]->pid);
		goto ret;
	}

	VDEV_DEBUG("Return guest I/O port 0x%x, width %d bytes, val 0x%x.\n",
		   port, 1 << width, result.val);

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

	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		spinlock_release(&vdev->ioport[port].ioport_lk);
		return 1;
	}

	if (width == SZ32)
		*(uint32_t *) data = inl(port);
	else if (width == SZ16)
		*(uint16_t *) data = inw(port);
	else if (width == SZ8)
		*(uint8_t *) data = inb(port);

	VDEV_DEBUG("Read host I/O port 0x%x, width %d bytes, val 0x%x.\n",
		   port, 1 << width, *data);
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return 0;
}

int
vdev_write_host_ioport(struct vm *vm, vid_t vid,
		       uint16_t port, data_sz_t width, uint32_t val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->ioport[port].ioport_lk);

	if (vdev->ioport[port].vid != vid) {
		VDEV_DEBUG("Unmatched vid %d (expect %d).\n",
			   vdev->ioport[port].vid, vid);
		spinlock_release(&vdev->ioport[port].ioport_lk);
		return 1;
	}

	if (width == SZ32)
		outl(port, (uint32_t) val);
	else if (width == SZ16)
		outw(port, (uint16_t) val);
	else if (width == SZ8)
		outb(port, (uint8_t) val);

	VDEV_DEBUG("Write host I/O port 0x%x, width %d bytes, val 0x%x.\n",
		   port, 1 << width, val);
	spinlock_release(&vdev->ioport[port].ioport_lk);
	return 0;
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

	VDEV_DEBUG("%s virtual IRQ line %d.\n",
		   mode == 0 ? "Raise" : mode == 1 ? "Lower" : "Trigger", irq);

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

	spinlock_acquire(&vdev->vpic_lk);

	if (peep)
		intout = vpic_get_irq(&vdev->vpic);
	else
		intout = vpic_read_irq(&vdev->vpic);

	VDEV_DEBUG("%s virtual INTOUT %d.\n", peep ? "Peep" : "Read", intout);

	spinlock_release(&vdev->vpic_lk);

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

	VDEV_DEBUG("%s guest physical address 0x%08x %s "
		   "host linear address 0x%08x, size %d bytes.\n",
		   write ? "Write" : "Read", gpa, write ? "from" : "to",
		   la, size);

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

	struct vdev *vdev = &vm->vdev;
	struct channel *ch;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No virtual device %d.\n", vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	if ((ch = vdev->sync_ch[vid]) == NULL) {
		VDEV_DEBUG("No available channel.\n");
		spinlock_release(&vdev->dev_lk);
		return 2;
	}

	VDEV_DEBUG("Synchronize virtual device %d.\n", vid);

	/*
	 * We use channel_send() instead of proc_send_msg() here, because when
	 * sync request is being sent, the process hosting the virtual machine
	 * maybe already blocked.
	 */

	struct dev_sync_req req = { .magic = DEV_SYNC_REQ, .vid = vid };
	spinlock_acquire(&ch->lk);
	channel_send(ch, vm->proc, &req, sizeof(req));
	spinlock_release(&ch->lk);

	spinlock_release(&vdev->dev_lk);

	return 0;
}

int
vdev_send_device_ready(struct vm *vm, vid_t vid)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	int rc = 0;
	struct vdev *vdev = &vm->vdev;
	struct channel *ch;
	struct proc *dev_proc;

	spinlock_acquire(&vdev->dev_lk);

	if (vdev->dev[vid] == NULL) {
		VDEV_DEBUG("No virtual device %d.\n", vid);
		spinlock_release(&vdev->dev_lk);
		return 1;
	}

	dev_proc = vdev->dev[vid];
	ch = vdev->sync_ch[vid];

	VDEV_DEBUG("Send DEVICE_RDY from virtual device %d.\n", vid);

	spinlock_release(&vdev->dev_lk);

	struct device_rdy result = { .magic = DEVIDE_READY, .vid = vid };
	rc = proc_send_msg(ch, dev_proc, &result, sizeof(result));

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

int
vdev_wait_all_devices_ready(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct vdev *vdev = &vm->vdev;
	vid_t vid;
	struct proc *p;
	struct channel *ch;

	pageinfo_t *recv_pi;
	uint8_t *recv_buf;
	size_t recv_size;
	struct device_rdy *rdy;

	if ((recv_pi = mem_page_alloc()) == NULL) {
		VDEV_DEBUG("Cannot allocate memory for receive buffer.\n");
		return 1;
	}
	recv_buf = mem_pi2ptr(recv_pi);

	for (vid = 0; vid < MAX_VID; vid++) {
		p = vdev->dev[vid];
		ch = vdev->sync_ch[vid];

		if (p == NULL || ch == NULL)
			continue;

		VDEV_DEBUG("Waiting for DEVICE_RDY from virtual device %d.\n",
			   vid);

		if (proc_recv_msg(ch, vm->proc, recv_buf, &recv_size, TRUE) ||
		    recv_size != sizeof(struct device_rdy)) {
			VDEV_DEBUG("Cannot receve DEVICE_RDY from process %d.\n",
				   p->pid);
			continue;
		}

		rdy = (struct device_rdy *) recv_buf;

		if (rdy->vid != vid)
			continue;

		VDEV_DEBUG("Virtual device %d is ready.\n", vid);
	}

	mem_page_free(recv_pi);
	return 0;
}

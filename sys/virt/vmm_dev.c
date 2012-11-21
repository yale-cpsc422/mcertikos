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

	KERN_DEBUG("Initialize virtual device interface.\n");
	spinlock_init(&vm->vdev.dev_lk);
	memzero(vm->vdev.dev, sizeof(struct proc *) * MAX_VID);
	memzero(vm->vdev.ch, sizeof(struct channel *) * MAX_VID);
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
			vm->vdev.ch[vid] = p->parent_ch;

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
		vmm_intercept_ioport(vm, port1, TRUE);

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
		vmm_intercept_ioport(vm, port1, FALSE);

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

static int
vdev_send_request(struct vm *vm, struct channel *ch, void *req, size_t size)
{
	KERN_ASSERT(vm != NULL && vm->proc != NULL);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(req != NULL);
	KERN_ASSERT(size > 0);

	struct proc *receiver;
	int rc;

	do {
		spinlock_acquire(&ch->lk);
		rc = channel_send(ch, vm->proc, req, size);
		spinlock_release(&ch->lk);
	} while (rc == E_CHANNEL_BUSY);

	if (rc) {
		VDEV_DEBUG("Cannot sent request through channel %d.\n",
			   channel_getid(ch));
		return 1;
	}

	receiver = (ch->p1 == vm->proc) ? ch->p2 : ch->p1;
	sched_lock(receiver->cpu);
	if (receiver->state == PROC_BLOCKED &&
	    receiver->block_reason == WAITING_FOR_RECEIVING &&
	    receiver->block_channel == ch) {
		VDEV_DEBUG("Unblock process %d to receive from channel %d.\n",
			   receiver->pid, channel_getid(ch));
		proc_unblock(receiver);
	}
	sched_unlock(receiver->cpu);

	return 0;
}

static int
vdev_recv_result(struct vm *vm, struct channel *ch, void *result, size_t *size)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(result != NULL);
	KERN_ASSERT(size != NULL);

	struct proc *sender;
	int rc;

	do {
		spinlock_acquire(&ch->lk);
		rc = channel_receive(ch, vm->proc, result, size);
		spinlock_release(&ch->lk);
	} while (rc == E_CHANNEL_IDLE);

	if (rc) {
		VDEV_DEBUG("Cannot receive result from channel %d.\n",
			   channel_getid(ch));
		return 1;
	}

	sender = (ch->p1 == vm->proc) ? ch->p2 : ch->p1;
	sched_lock(sender->cpu);
	if (sender->state == PROC_BLOCKED &&
	    sender->block_reason == WAITING_FOR_SENDING &&
	    sender->block_channel == ch) {
		VDEV_DEBUG("Unblock process %d to send to channel %d.\n",
			   sender->pid, channel_getid(ch));
		proc_unblock(sender);
	}
	sched_unlock(sender->cpu);

	return 0;
}

static int
vdev_read_ioport_passthrough(uint16_t port, data_sz_t width, uint32_t *data)
{
	if (width == SZ8)
		*(uint8_t *) data = inb(port);
	else if (width == SZ16)
		*(uint16_t *) data = inw(port);
	else
		*(uint32_t *) data = inl(port);
	return 0;
}

static int
vdev_write_ioport_passthrough(uint16_t port, data_sz_t width, uint32_t data)
{
	if (width == SZ8)
		outb(port, (uint8_t) data);
	else if (width == SZ16)
		outw(port, (uint16_t) data);
	else
		outl(port, (uint32_t) data);
	return 0;
}

int
vdev_read_guest_ioport(struct vm *vm,
		       uint16_t port, data_sz_t width, uint32_t *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(data != NULL);

	if (port == IO_PIC1 || port == IO_PIC1+1 ||
	    port == IO_PIC2 || port == IO_PIC2+1 ||
	    port == IO_ELCR1 || port == IO_ELCR2)
		return vpic_read_ioport(&vm->vdev.vpic, port, (uint8_t *) data);
	else if (
#ifndef TRACE_VIRT
		 (0x3f8 <= port && port <= 0x3ff) || /* COM1 */
#endif
		 (0x3b0 <= port && port <= 0x3bf) || /* MONO */
		 (0x3c0 <= port && port <= 0x3cf) || /* VGA */
		 (0x3d0 <= port && port <= 0x3df)    /* CGA */)
		return vdev_read_ioport_passthrough(port, width, data);

	int rc = 0;

	uint8_t *recv_buf;
	size_t recv_size;

	struct vdev *vdev = &vm->vdev;
	struct channel *ch;
	struct vdev_ioport_info *result;

	vid_t vid;

	if ((vid = vdev->ioport[port].vid) == -1) {
		SET_IOPORT_DATA(data, 0xffffffff, width);
		return 0;
	}

	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	ch = vdev->ch[vid];

	struct vdev_ioport_info req = { .magic = VDEV_READ_GUEST_IOPORT,
				        .port  = port, .width = width };
	VDEV_DEBUG("Send read request (port 0x%x, width %d bytes) to "
		   "virtual device %d.\n", port, 1 << width, vid);
	if ((rc = vdev_send_request(vm, ch, &req, sizeof(req)))) {
		VDEV_DEBUG("Cannot send read request (port 0x%x, "
			   "width %d bytes) to process %d.\n",
			   port, 1 << width, vdev->dev[vid]->pid);
		goto ret;
	}

	recv_buf = vm->proc->sys_buf;

	VDEV_DEBUG("Wait for data from virtual device %d.\n", vid);

	rc = vdev_recv_result(vm, ch, recv_buf, &recv_size);
	result = (struct vdev_ioport_info *) recv_buf;

	if (rc || recv_size != sizeof(struct vdev_ioport_info) ||
	    result->magic != VDEV_GUEST_IOPORT_DATA) {
		VDEV_DEBUG("Cannot receive data (port 0x%x, "
			   "width %d bytes) from process %d.\n",
			   port, 1 << width, vdev->dev[vid]->pid);
		rc = 1;
		goto ret;
	}

	SET_IOPORT_DATA(data, result->val, width);

 ret:
	if (rc)
		SET_IOPORT_DATA(data, 0xffffffff, width);
	VDEV_DEBUG("Read guest I/O port 0x%x, width %d bytes, val 0x%x, "
		   "vid %d.\n", port, 1 << width, *data, vid);
	return rc;
}

int
vdev_write_guest_ioport(struct vm *vm,
			uint16_t port, data_sz_t width, uint32_t data)
{
	KERN_ASSERT(vm != NULL);

	int rc = 0;

	if (port == IO_PIC1 || port == IO_PIC1+1 ||
	    port == IO_PIC2 || port == IO_PIC2+1 ||
	    port == IO_ELCR1 || port == IO_ELCR2)
		return vpic_write_ioport(&vm->vdev.vpic, port, data);
	else if (
#ifndef TRACE_VIRT
		 (0x3f8 <= port && port <= 0x3ff) || /* COM1 */
#endif
		 (0x3b0 <= port && port <= 0x3bf) || /* MONO */
		 (0x3c0 <= port && port <= 0x3cf) || /* VGA */
		 (0x3d0 <= port && port <= 0x3df)    /* CGA */)
		return vdev_write_ioport_passthrough(port, width, data);

	struct vdev *vdev = &vm->vdev;
	vid_t vid;

	if ((vid = vdev->ioport[port].vid) == -1)
		return 0;

	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	struct vdev_ioport_info req = {	.magic = VDEV_WRITE_GUEST_IOPORT,
					.port  = port, .width = width };
	SET_IOPORT_DATA(&req.val, data, width);
	if ((rc = vdev_send_request(vm, vdev->ch[vid], &req, sizeof(req)))) {
		VDEV_DEBUG("Cannot send write request (port 0x%x, "
			   "width %d bytes, data 0x%x) to process %d.\n",
			   port, 1 << width, req.val, vdev->dev[vid]->pid);
		return rc;
	}

	return 0;
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

	spinlock_acquire(&vdev->vpic_lk);
	if (mode == 0) {
		vpic_set_irq(&vdev->vpic, irq, 1);
	} else if (mode == 1) {
		vpic_set_irq(&vdev->vpic, irq, 0);
	} else {
		vpic_set_irq(&vdev->vpic, irq, 0);
		vpic_set_irq(&vdev->vpic, irq, 1);
	}
	spinlock_release(&vdev->vpic_lk);

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

	if (gpa >= vm->memsize) {
		VDEV_DEBUG("Guest physical address 0x%08x is out of range "
			   "(0x00000000 ~ 0x%08x).\n", gpa, vm->memsize);
		return 1;
	}

	if (vm->memsize - gpa < size) {
		VDEV_DEBUG("Size (%d bytes) is out of range (%d bytes).\n",
			   size, vm->memsize - gpa);
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

	if ((ch = vdev->ch[vid]) == NULL) {
		VDEV_DEBUG("No available channel.\n");
		spinlock_release(&vdev->dev_lk);
		return 2;
	}

	VDEV_DEBUG("Synchronize virtual device %d.\n", vid);

	struct vdev_device_sync req = { .magic = VDEV_DEVICE_SYNC };
	vdev_send_request(vm, ch, &req, sizeof(req));

	spinlock_release(&vdev->dev_lk);

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

	uint8_t *recv_buf;
	size_t recv_size;
	struct vdev_device_ready *rdy;

	recv_buf = vm->proc->sys_buf;

	for (vid = 0; vid < MAX_VID; vid++) {
		p = vdev->dev[vid];
		ch = vdev->ch[vid];

		if (p == NULL || ch == NULL)
			continue;

		VDEV_DEBUG("Waiting for DEVICE_RDY from virtual device %d.\n",
			   vid);

		if (vdev_recv_result(vm, ch, recv_buf, &recv_size) ||
		    recv_size != sizeof(struct vdev_device_ready)) {
			VDEV_DEBUG("Cannot receve DEVICE_RDY from process %d.\n",
				   p->pid);
			continue;
		}

		rdy = (struct vdev_device_ready *) recv_buf;

		if (rdy->magic != VDEV_DEVICE_READY)
			continue;

		VDEV_DEBUG("Virtual device %d is ready.\n", vid);
	}

	return 0;
}

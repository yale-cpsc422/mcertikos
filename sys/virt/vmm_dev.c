#include <sys/channel.h>
#include <sys/debug.h>
#include <sys/ipc.h>
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

	struct vdev *vdev = &vm->vdev;
	int i;

	spinlock_init(&vdev->dev_lk);

	/*
	 * Initialize the kernel virtual devices.
	 */

	KERN_DEBUG("Initialize virtual i8259.\n");
	spinlock_init(&vdev->vpic_lk);
	vpic_init(&vdev->vpic, vm);

	/*
	 * Intialize the user virtual device interface.
	 */

	KERN_DEBUG("Initialize virtual device interface.\n");

	memzero(vm->vdev.dev, sizeof(struct proc *) * MAX_VID);
	memzero(vm->vdev.dev_in, sizeof(struct channel *) * MAX_VID);
	memzero(vm->vdev.dev_out, sizeof(struct channel *) * MAX_VID);

	for (i = 0; i < MAX_IOPORT; i++) {
		spinlock_init(&vdev->ioport[i].ioport_lk);
		vdev->ioport[i].vid = -1;
	}

	for (i = 0; i < MAX_IRQ; i++) {
		spinlock_init(&vdev->irq[i].irq_lk);
		vdev->irq[i].vid = -1;
	}
}

vid_t
vdev_register_device(struct vm *vm, struct proc *p,
		     struct channel *in_ch, struct channel *out_ch)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(in_ch != NULL);
	KERN_ASSERT(out_ch != NULL);

	vid_t vid;

	spinlock_acquire(&vm->vdev.dev_lk);

	for (vid = 0; vid < MAX_VID; vid++)
		if (vm->vdev.dev[vid] == NULL) {
			vm->vdev.dev[vid] = p;
			vm->vdev.dev_in[vid] = in_ch;
			vm->vdev.dev_out[vid] = out_ch;

			proc_lock(p);
			p->vid = vid;
			p->master_vm = vm;
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

	struct proc *dev_p;

	spinlock_acquire(&vm->vdev.dev_lk);

	dev_p = vm->vdev.dev[vid];

	proc_lock(dev_p);
	dev_p->vid = -1;
	dev_p->master_vm = NULL;
	proc_unlock(dev_p);

	vm->vdev.dev[vid] = NULL;
	vm->vdev.dev_in[vid] = vm->vdev.dev_out[vid] = NULL;

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
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->proc != NULL);
	KERN_ASSERT(vm->proc == proc_cur());
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(req != NULL);
	KERN_ASSERT(size > 0);

	int rc;
	do {
		rc = ipc_send(ch, (uintptr_t) req, size, TRUE, FALSE);
	} while (rc != E_IPC_SUCC && rc != E_IPC_FAIL);

	return rc;
}

static int
vdev_recv_result(struct vm *vm, struct channel *ch, void *result, size_t size)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->proc != NULL);
	KERN_ASSERT(vm->proc == proc_cur());
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(result != NULL);

	int rc;
	do {
		rc = ipc_recv(ch, (uintptr_t) result, size, TRUE, FALSE);
	} while (rc != E_IPC_SUCC && rc != E_IPC_FAIL);

	return rc;
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

	struct vdev *vdev = &vm->vdev;
	struct channel *ch;
	struct vdev_ioport_info *result;

	vid_t vid;

	if ((vid = vdev->ioport[port].vid) == -1) {
		SET_IOPORT_DATA(data, 0xffffffff, width);
		return 0;
	}

	KERN_ASSERT(0 <= vid && vid < MAX_VID);

	ch = vdev->dev_in[vid];

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
	ch = vdev->dev_out[vid];

	VDEV_DEBUG("Wait for data from virtual device %d.\n", vid);

	rc = vdev_recv_result(vm, ch, recv_buf, sizeof(struct vdev_ioport_info));
	result = (struct vdev_ioport_info *) recv_buf;

	if (rc || result->magic != VDEV_GUEST_IOPORT_DATA) {
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
	if ((rc = vdev_send_request(vm, vdev->dev_in[vid], &req, sizeof(req)))) {
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
	return vmm_rw_guest_memory(vm, gpa, pmap, la, size, write);
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

	if ((ch = vdev->dev_in[vid]) == NULL) {
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
	size_t recv_size = sizeof(struct vdev_device_ready);
	struct vdev_device_ready *rdy;

	recv_buf = vm->proc->sys_buf;

	for (vid = 0; vid < MAX_VID; vid++) {
		p = vdev->dev[vid];
		ch = vdev->dev_out[vid];

		if (p == NULL || ch == NULL)
			continue;

		VDEV_DEBUG("Waiting for DEVICE_RDY from virtual device %d.\n",
			   vid);

		if (vdev_recv_result(vm, ch, recv_buf, recv_size)) {
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

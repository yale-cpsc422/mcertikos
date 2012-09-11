#include <sys/debug.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

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
	memzero(&vm->vdev, sizeof(struct vdev));
}

int
vdev_register_ioport(struct vm *vm, struct proc *p,
		     uint16_t port, data_sz_t width, bool write)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = (write == TRUE) ? &vdev->ioport[port][width].write_lk :
		&vdev->ioport[port][width].read_lk;

	spinlock_acquire(lk);

	if ((write == FALSE && vdev->ioport[port][width].read_proc != NULL) ||
	    (write == TRUE && vdev->ioport[port][width].write_proc != NULL)) {
		VDEV_DEBUG("Process %d is already registered to %s "
			   "I/O port %d (width %d bits).\n",
			   (write == FALSE) ?
			   vdev->ioport[port][width].read_proc->pid :
			   vdev->ioport[port][width].write_proc->pid,
			   (write == FALSE) ? "read" : "write",
			   port,
			   8 * (1 << width));
		spinlock_release(lk);
		return 1;
	}

	if (write == FALSE)
		vdev->ioport[port][width].read_proc = p;
	else
		vdev->ioport[port][width].write_proc = p;

	spinlock_release(lk);

	VDEV_DEBUG("Register process %d to %s I/O port %d "
		   "(width %d bits).\n",
		   p->pid,
		   (write == FALSE) ? "read" : "write",
		   port,
		   8 * (1 << width));

	return 0;
}

int
vdev_unregister_ioport(struct vm *vm, struct proc *p,
		       uint16_t port, data_sz_t width, bool write)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(p != NULL && p->state == PROC_INVAL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = (write == TRUE) ? &vdev->ioport[port][width].write_lk :
		&vdev->ioport[port][width].read_lk;

	spinlock_acquire(lk);

	if ((write == FALSE && vdev->ioport[port][width].read_proc != p) ||
	    (write == TRUE && vdev->ioport[port][width].write_proc != p)) {
		VDEV_DEBUG("Process %d is not registered to %s "
			   "I/O port %d (width %d bits).\n",
			   p->pid,
			   (write == FALSE) ? "read" : "write",
			   port,
			   8 * (1 << width));
		spinlock_release(lk);
		return 1;
	}

	if (write == FALSE)
		vdev->ioport[port][width].read_proc = NULL;
	else
		vdev->ioport[port][width].write_proc = NULL;

	spinlock_release(lk);

	VDEV_DEBUG("Unregister process %d to %s I/O port %d "
		   "(width %d bits).\n",
		   p->pid,
		   (write == FALSE) ? "read" : "write",
		   port,
		   8 * (1 << width));

	return 0;
}

int
vdev_register_irq(struct vm *vm, struct proc *p, uint8_t irq)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->irq[irq].irq_lk;

	spinlock_acquire(lk);

	if (vdev->irq[irq].irq_proc != NULL) {
		VDEV_DEBUG("Process %d is already registered as the source of "
			   "IRQ %d.\n", vdev->irq[irq].irq_proc->pid, irq);
		spinlock_release(lk);
		return 1;
	}

	vdev->irq[irq].irq_proc = p;

	spinlock_release(lk);

	VDEV_DEBUG("Register process %d as the source of IRQ %d.\n",
		   p->pid, irq);

	return 0;
}

int
vdev_unregister_irq(struct vm *vm, struct proc *p, uint8_t irq)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->irq[irq].irq_lk;

	spinlock_acquire(lk);

	if (vdev->irq[irq].irq_proc != p) {
		VDEV_DEBUG("Process %d is not registered as the source of "
			   "IRQ %d.\n", p->pid, irq);
		spinlock_release(lk);
		return 1;
	}

	vdev->irq[irq].irq_proc = NULL;

	spinlock_release(lk);

	VDEV_DEBUG("Unregistered process %d as the source of IRQ %d.\n",
		   p->pid, irq);

	return 0;
}

int
vdev_register_pic(struct vm *vm, struct proc *p)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->pic_lk;

	spinlock_acquire(lk);

	if (vdev->pic_proc != NULL) {
		VDEV_DEBUG("Process %d is already registered as PIC.\n",
			   vdev->pic_proc->pid);
		spinlock_release(lk);
		return 1;
	}

	vdev->pic_proc = p;

	spinlock_release(lk);

	VDEV_DEBUG("Register process %d as PIC.\n", p->pid);

	return 0;
}

int
vdev_unregister_pic(struct vm *vm, struct proc *p)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(p != NULL && p->state != PROC_INVAL);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->pic_lk;

	spinlock_acquire(lk);

	if (vdev->pic_proc != p) {
		VDEV_DEBUG("Process %d is not registered as PIC.\n", p->pid);
		spinlock_release(lk);
		return 1;
	}

	vdev->pic_proc = NULL;

	spinlock_release(lk);

	VDEV_DEBUG("Unregister process %d as PIC.\n", p->pid);

	return 0;
}

int
vdev_register_mmio(struct vm *vm, struct proc *p,
		   uintptr_t gpa, uintptr_t hla, size_t size)
{
	KERN_PANIC("Not implement yet!\n");
	return 0;
}

int
vdev_unregister_mmio(struct vm *vm, struct proc *p, uintptr_t gpa, size_t size)
{
	KERN_PANIC("Not implement yet!\n");
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

static gcc_inline void
physical_ioport_read(uint16_t port, data_sz_t width, uint32_t *data)
{
	uint32_t val;

	switch (width) {
	case SZ8:
		val = inb(port);
		break;
	case SZ16:
		val = inw(port);
		break;
	case SZ32:
		val = inl(port);
		break;
	default:
		val = 0xffffffff;
	}

	SET_IOPORT_DATA(data, val, width);

	VDEV_DEBUG("Read host I/O port %d, width %d bits, val 0x%x.\n",
		   port, 8 * (1 << width), val);
}

static gcc_inline void
physical_ioport_write(uint16_t port, data_sz_t width, uint32_t data)
{
	VDEV_DEBUG("Write host I/O port %d, width %d bits, val 0x%x.\n",
		   port, 8 * (1 << width), data);

	switch (width) {
	case SZ8:
		outb(port, (uint8_t) data);
		break;
	case SZ16:
		outw(port, (uint16_t) data);
		break;
	case SZ32:
		outl(port, data);
		break;
	default:
		/* do nothing */
		return;
	}
}

int
vdev_ioport_read(struct vm *vm, uint16_t port, data_sz_t width, uint32_t *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);
	KERN_ASSERT(data != NULL);

	struct vdev *vdev;
	spinlock_t *lk;
	struct proc *p;

	vdev = &vm->vdev;
	lk = &vdev->ioport[port][width].read_lk;

	spinlock_acquire(lk);

	p = vdev->ioport[port][width].read_proc;

	if (p == NULL) {
		physical_ioport_read(port, width, data);
	} else {
		struct channel *ch;
		struct ioport_rw_req req =
			{ .magic = MAGIC_IOPORT_RW_REQ,
			  .port  = port,
			  .width = width,
			  .write = 0 };

		uint8_t recv_buf[CHANNEL_BUFFER_SIZE];
		size_t recv_size;

		struct ioport_read_ret *read_ret;

		int rc;

		ch = p->parent_ch;

#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Send I/O port read request "
			   "(port %d, width %d bits) to process %d.\n",
			   port, 8 * (1 << width), p->pid);
#endif

		if ((rc = proc_send_msg(ch, p->parent, &req, sizeof(req)))) {
			VDEV_DEBUG("Cannot send I/O port read request "
				   "(port %d, width %d bits) to process %d. "
				   "(errno %d)\n",
				   port, 8 * (1 << width), p->pid, rc);
			SET_IOPORT_DATA(data, 0xffffffff, width);

			spinlock_release(lk);
			return 1;
		}

		if ((rc = proc_recv_msg(ch, p->parent, recv_buf, &recv_size, TRUE))) {
			VDEV_DEBUG("Cannot receive I/O port read result from "
				   "process %d. (errno %d)\n",
				   p->pid, rc);
			SET_IOPORT_DATA(data, 0xffffffff, width);

			spinlock_release(lk);
			return 1;
		}

		if (recv_size != sizeof(struct ioport_read_ret)) {
			VDEV_DEBUG("Unmatched message size. "
				   "(recv %d bytes, expect %d bytes)\n",
				   recv_size, sizeof(struct ioport_read_ret));
			SET_IOPORT_DATA(data, 0xffffffff, width);

			spinlock_release(lk);
			return 1;
		}

		read_ret = (struct ioport_read_ret *) recv_buf;

		if (read_ret->magic != MAGIC_IOPORT_READ_RET) {
			VDEV_DEBUG("Unmatched message magic. "
				   "(recv 0x%08x, expect 0x%08x)\n",
				   read_ret->magic, MAGIC_IOPORT_READ_RET);
			SET_IOPORT_DATA(data, 0xffffffff, width);

			spinlock_release(lk);
			return 1;
		}

#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Receive result 0x%x of I/O port read request "
			   "(port %d, width %d bits) from process %d.\n",
			   read_ret->data, port, 8 * (1 << width), p->pid);
#endif

		SET_IOPORT_DATA(data, read_ret->data, width);
	}

	spinlock_release(lk);
	return 0;
}

int
vdev_ioport_write(struct vm *vm, uint16_t port, data_sz_t width, uint32_t data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);

	struct vdev *vdev;
	spinlock_t *lk;
	struct proc *p;

	vdev = &vm->vdev;
	lk = &vdev->ioport[port][width].write_lk;

	spinlock_acquire(lk);

	p = vdev->ioport[port][width].write_proc;

	if (p == NULL) {
		physical_ioport_write(port, width, data);
	} else {
		struct channel *ch;
		struct ioport_rw_req req =
			{ .magic = MAGIC_IOPORT_RW_REQ,
			  .port  = port,
			  .width = width,
			  .write = 1 };
		int rc;

		ch = p->parent_ch;
		SET_IOPORT_DATA(&req.data, data, width);

#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Send I/O port write request "
			   "(port %d, width %d bits, value 0x%x) to process %d.\n",
			   port, 8 * (1 << width), data, p->pid);
#endif

		if ((rc = proc_send_msg(ch, p->parent, &req, sizeof(req)))) {
			VDEV_DEBUG("Cannot send I/O port write request "
				   "(port %d, data %x, width %d bits) to "
				   "process %d. (errno %d)\n",
				   port, data, 8 * (1 << width), p->pid, rc);

			spinlock_release(lk);
			return 1;
		}
	}

	spinlock_release(lk);
	return 0;
}

int
vdev_host_ioport_read(struct vm *vm,
		      uint16_t port, data_sz_t width, uint32_t *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);
	KERN_ASSERT(data != NULL);

	struct proc *p = proc_cur();
	struct vdev *vdev = &vm->vdev;

	VDEV_DEBUG("Receive host I/O port read request "
		   "(port %d, width %d bits) from process %d.\n",
		   port, 8 * (1 << width), p->pid);

	if (vdev->ioport[port][width].read_proc != p) {
		VDEV_DEBUG("Process %d is not allowed to read I/O port %d.\n",
			   p->pid, port);
		return 1;
	}

	physical_ioport_read(port, width, data);

	return 0;
}

int
vdev_host_ioport_write(struct vm *vm,
		       uint16_t port, data_sz_t width, uint32_t data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= port && port < MAX_IOPORT);

	struct proc *p = proc_cur();
	struct vdev *vdev = &vm->vdev;

	VDEV_DEBUG("Receive host I/O port write request "
		   "(port %d, width %d bits, data 0x%x) from process %d.\n",
		   port, 8 * (1 << width),
		   (width == SZ8) ? (uint8_t) data :
		   (width == SZ16) ? (uint16_t) data : data,
		   p->pid);

	if (vdev->ioport[port][width].write_proc != p) {
		VDEV_DEBUG("Process %d is not allowed to write I/O port %d.\n",
			   p->pid, port);
		return 1;
	}

	physical_ioport_write(port, width, data);

	return 0;
}

static int
vdev_trigger_irq_helper(struct vm *vm, uint8_t irq, int trigger)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);
	KERN_ASSERT(-1 <= trigger && trigger <= 1);

	struct proc *p = proc_cur();
	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk;
	struct channel *ch;
	int rc;

	lk = &vdev->irq[irq].irq_lk;
	KERN_ASSERT(spinlock_holding(lk) == TRUE);

	if (p != vdev->irq[irq].irq_proc) {
		VDEV_DEBUG("Process %d is not the source of IRQ %d.\n",
			   p->pid, irq);
		return 1;
	}

	lk = &vdev->pic_lk;

	spinlock_acquire(lk);

	if (vdev->pic_proc == NULL) {
		spinlock_release(lk);
		VDEV_DEBUG("No process is registered as PIC.\n");
		return 1;
	}

	struct irq_req req =
		{ .magic = MAGIC_IRQ_REQ, .irq = irq, .trigger = trigger };
	ch = vdev->pic_proc->parent_ch;

	VDEV_DEBUG("Process %d: send IRQ_%s request (irq %d) to process %d.\n",
		   proc_cur()->pid,
		   (trigger == -1) ? "LOWER" :
		   (trigger == 0) ? "TRIGGER" : "RAISE",
		   irq, vdev->pic_proc->pid);

	if ((rc = proc_send_msg(ch,
				vdev->pic_proc->parent, &req, sizeof(req)))) {
		VDEV_DEBUG("Cannot send IRQ_%s request (irq %d) to "
			   "process %d. (errno %d)\n",
			   (trigger == -1) ? "LOWER" :
			   (trigger == 0) ? "TRIGGER" : "RAISE",
			   irq, vdev->pic_proc->pid, rc);
		spinlock_release(lk);
		return 1;
	}

	spinlock_release(lk);

	return 0;
}

int
vdev_raise_irq(struct vm *vm, uint8_t irq)
{
	return vdev_trigger_irq_helper(vm, irq, 1);
}

int
vdev_trigger_irq(struct vm *vm, uint8_t irq)
{
	return vdev_trigger_irq_helper(vm, irq, 0);
}

int
vdev_lower_irq(struct vm *vm, uint8_t irq)
{
	return vdev_trigger_irq_helper(vm, irq, -1);
}

int
vdev_notify_sync(struct vm *vm, uint8_t irq)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->irq[irq].irq_lk;
	struct proc *p;
	int rc;
	uint8_t recv_buf[CHANNEL_BUFFER_SIZE];
	size_t size;

	spinlock_acquire(lk);

	p = vdev->irq[irq].irq_proc;

	if (p == NULL) {
		spinlock_release(lk);
		VDEV_DEBUG("No process is registered as the source of IRQ %d.\n",
			   irq);
		return 1;
	}

	struct sync_req req = { .magic = MAGIC_SYNC_IRQ, .irq = irq };

	VDEV_DEBUG("Send SYNC request (irq %d) to process %d.\n", irq, p->pid);

	if ((rc = proc_send_msg(p->parent_ch, p->parent, &req, sizeof(req)))) {
		VDEV_DEBUG("Cannot send SYNC request to process %d. "
			   "(errno %d)\n", p->pid);
		spinlock_release(lk);
		return 1;
	}

	VDEV_DEBUG("Waiting for SYNC_COMPLETE.\n");

	struct sync_complete *compl;

	if ((rc = proc_recv_msg(p->parent_ch,
				p->parent, recv_buf, &size, TRUE))) {
		VDEV_DEBUG("Cannot receive SYNC_COMPLETE from process %d. "
			   "(errno %d)\n", p->pid);
		spinlock_release(lk);
		return 1;
	}

	compl = (struct sync_complete *) recv_buf;

	if (size != sizeof(struct sync_complete)) {
		VDEV_DEBUG("Unmatched message size "
			   "(recv %d bytes, expect %d bytes).\n",
			   size, sizeof(struct sync_complete));
		spinlock_release(lk);
		return 1;
	}

	if (compl->magic != MAGIC_SYNC_COMPLETE) {
		VDEV_DEBUG("Unmached message magic "
			   "(recv 0x%x, expect 0x%x).\n",
			   compl->magic, MAGIC_SYNC_COMPLETE);
		spinlock_release(lk);
		return 1;
	}

	if (compl->irq != irq) {
		VDEV_DEBUG("Unmatched IRQ (recv %d, expect %d).\n",
			   compl->irq, irq);
		spinlock_release(lk);
		return 1;
	}

	VDEV_DEBUG("Receive SYNC_COMPLETE for IRQ %d from process %d.\n",
		   irq, p->pid);

	spinlock_release(lk);

	return 0;
}

int
vdev_notify_irq(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct proc *p = proc_cur();
	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->pic_lk;

	spinlock_acquire(lk);

	if (p != vdev->pic_proc) {
		spinlock_release(lk);
		VDEV_DEBUG("Process %d is not virtual PIC.\n", p->pid);
		return 1;
	}

	if (p->cpu == vm->proc->cpu) {
		spinlock_release(lk);
		VDEV_DEBUG("Virtual machine and virtual PIC on the same "
			   "CPU%d.\n", pcpu_cpu_idx(p->cpu));
		return 1;
	}

	VDEV_DEBUG("Notify IRQ on INTOUT pin of virtual PIC.\n");

	/* send an IPI to notify an interrupt from the virtual PIC */
	lapic_send_ipi(vm->proc->cpu->arch_info.lapicid,
		       T_IRQ0 + IRQ_IPI_VINTR,
		       LAPIC_ICRLO_FIXED, LAPIC_ICRLO_NOBCAST);

	spinlock_release(lk);

	return 0;
}

static int
vdev_send_read_irq_req(struct vm *vm, int *irq, int read)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(irq != NULL);

	struct vdev *vdev = &vm->vdev;
	spinlock_t *lk = &vdev->pic_lk;
	int rc;

	spinlock_acquire(lk);

	struct proc *p = vdev->pic_proc;

	if (p == NULL) {
		spinlock_release(lk);
		VDEV_DEBUG("No process is registered as PIC.\n");
		return 1;
	}

	struct irq_read_req req = { .magic = MAGIC_IRQ_READ_REQ, .read = read };

	VDEV_DEBUG("Send IRQ_%s request to process %d.\n",
		   read ? "READ" : "GET", p->pid);

	if ((rc = proc_send_msg(p->parent_ch, p->parent, &req, sizeof(req)))) {
		VDEV_DEBUG("Cannot send %s request to process %d.\n",
			   read == 1 ? "READ_IRQ" : "GET_IRQ", p->pid);
		spinlock_release(lk);
		return 1;
	}

	VDEV_DEBUG("Waiting for IRQ_READ_RETURN.\n");

	uint8_t recv_buf[CHANNEL_BUFFER_SIZE];
	size_t recv_size;

	if ((rc = proc_recv_msg(p->parent_ch,
				p->parent, recv_buf, &recv_size, TRUE))) {
		VDEV_DEBUG("Cannot receive IRQ from process %d.\n", p->pid);
		spinlock_release(lk);
		return 1;
	}

	if (recv_size != sizeof(struct irq_read_ret)) {
		spinlock_release(lk);
		VDEV_DEBUG("Unmatched message size. "
			   "(recv %d bytes, expect %d bytes)\n",
			   recv_size, sizeof(struct irq_read_ret));
	}

	struct irq_read_ret *ret = (struct irq_read_ret *) recv_buf;

	if (ret->magic != MAGIC_IRQ_READ_RET) {
		spinlock_release(lk);
		VDEV_DEBUG("Dismacthed message magic. "
			   "(recv 0x%08x, expect 0x%08x)\n",
			   ret->magic, MAGIC_IRQ_READ_RET);
		return 1;
	}

	VDEV_DEBUG("Receive result %d of IRQ_%s request from process %d.\n",
		   ret->irq, read ? "READ" : "GET", p->pid);

	*irq = ret->irq;

	spinlock_release(lk);

	return 0;
}

int
vdev_get_irq(struct vm *vm, int *irq)
{
	return vdev_send_read_irq_req(vm, irq, 0);
}

int
vdev_read_irq(struct vm *vm, int *irq)
{
	return vdev_send_read_irq_req(vm, irq, 1);
}

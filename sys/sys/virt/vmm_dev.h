/*
 *
 *                      vdev_notify_irq           +-----------+
 *              +-------------------------------  |  Virtual  |
 *              |   +-------------------------->  |    PIC    |
 *              |   |        vdev_get_irq         +-----------+
 *              V   |        vdev_read_irq              ^
 *    +-------------------+                             | vdev_raise_irq
 *    |                   |                             | vdev_lower_irq
 *    |                   |                             |
 *    |  Virtual Machine  |   vdev_ioport_write   +-----------+
 *    |                   |   vdev_ioport_read    o           |
 *    |                   |  ------------------>  o           |
 *    +-------------------+                       |  Virtual  |
 *              |                                 |    DEV    |
 *              |      vdev_notify_sync           |           |
 *              +------------------------------>  |           |
 *                                                +-----------+
 *                                                      |
 *                                                      | vdev_sync_ioport_read
 *                                                      |
 *                                                      V
 *                                                +-----------+
 *                                                |  Physical |
 *                                                |    DEV    |
 *                                                +-----------+
 */

#ifndef _SYS_VIRT_VMM_DEV_H_
#define _SYS_VIRT_VMM_DEV_H_

#ifdef _KERN_

#include <sys/proc.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>

/*
 * Initialize the virtual device structures of a virtual machine.
 */
void vdev_init(struct vm *);

/*
 * Register/Unregister a process as a virtual device of a virtual machine.
 *
 * @param vm which virtual machine the virtual device belongs to
 * @param p  which process is being registered
 *
 * @return a valid virtual device id (>= 0) if successful; otherwise, return -1.
 */
vid_t vdev_register_device(struct vm *vm, struct proc *p);
void  vdev_unregister_device(struct vm *vm, vid_t vid);

/*
 * Register/Unregister a process to handle accesses to I/O ports.
 *
 * @param vm    the virtual machine to be operating on
 * @param port  the I/O port to be accessed
 * @param width the data width
 * @param vid   the virtual device responsible for the I/O port
 *
 * @return 0 if the registration/unregistration succeeds; otherwise, return a
 *         non-zero value.
 */
int vdev_register_ioport(struct vm *vm,
			 uint16_t port, data_sz_t width, vid_t vid);
int vdev_unregister_ioport(struct vm *vm,
			   uint16_t port, data_sz_t width, vid_t vid);

/*
 * Register/Unregister a process as the source of an interrupt.
 *
 * @param vm  the virtual machine to be operating on
 * @param irq the interrupt number (PIC/APIC vector number, not IDT vector
 *            number)
 * @param vid the virtual device responsible for the interrupt
 *
 * @return 0 if the registration/unregistration succeeds; otherwise, return a
 *           non-zero value.
 */
int vdev_register_irq(struct vm *vm, uint8_t irq, vid_t vid);
int vdev_unregister_irq(struct vm *vm, uint8_t irq, vid_t vid);

/*
 * Read/Write the guest I/O port.
 *
 * @param vm    the virtual machine operating on
 * @param vid   the virtual device responsible for the I/O port
 * @param port  the I/O port accessed
 * @param width the data width
 * @param data  the data read/written
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int vdev_read_guest_ioport(struct vm *vm, vid_t vid,
			   uint16_t port, data_sz_t width, uint32_t *data);
int vdev_write_guest_ioport(struct vm *vm, vid_t vid,
			    uint16_t port, data_sz_t width, uint32_t data);

/*
 * Return the data on the guest I/O port.
 *
 * @param vm    the virtual machine operating on
 * @param vid   the virtual machine responsible for the I/O port
 * @param port  the I/O port accessed
 * @param width the data width
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_return_guest_ioport(struct vm *vm, vid_t vid,
			     uint16_t port, data_sz_t width, uint32_t val);

/*
 * Read/Write the host I/O port.
 *
 * @param vm    the virtual machine operating on
 * @param vid   the virtual device responsible for the I/O port
 * @param port  the I/O port accessed
 * @param width the data width
 * @param data  the data read/written
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int vdev_read_host_ioport(struct vm *vm, vid_t vid,
			  uint16_t port, data_sz_t width, uint32_t *data);
int vdev_write_host_ioport(struct vm *vm, vid_t vid,
			   uint16_t port, data_sz_t width, uint32_t data);

/*
 * Set the interrupt line of the virtual PIC.
 *
 * @param vm   the virtual machine operating on
 * @param vid  the virtual device as the source of interrupt
 * @param irq  the interrupt to be raised
 * @param mode 0: raise the interrupt line;
 *             1: lower the interruot line;
 *             2: trigger the interrupt line
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_set_irq(struct vm *vm, vid_t vid, uint8_t irq, int mode);
#define vdev_raise_irq(vm, vid, irq)	vdev_set_irq((vm), (vid), (irq), 0)
#define vdev_lower_irq(vm, vid, irq)	vdev_set_irq((vm), (vid), (irq), 1)
#define vdev_trigger_irq(vm, vid, irq)	vdev_set_irq((vm), (vid), (irq), 2)

/*
 * Read/Peep the INTOUT line of the virtual PIC. Read operation has the side
 * effect, which will clear the INTOUT line; peep operation has no side effect.
 *
 * @param vm     the virtual machine operating on
 * @param intout return the data on the INTOUT line
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_get_intout(struct vm *vm, int peep);
#define vdev_peep_intout(vm)	vdev_get_intout((vm), 0)
#define vdev_read_intout(vm)	vdev_get_intout((vm), 1)

/*
 * Notify the guest an interrupt comes.
 *
 * @param vm  the virtual machine operating on
 * @param vid the virtual PIC
 * @param irq the interrupt number
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_notify_irq(struct vm *vm, vid_t vid, uint8_t irq);

/*
 * Transfer data between the guest physical address space and the host linear
 * address space.
 *
 * @param vm   the virtual machine operating on
 * @param gpa  the start guest physical address
 * @param pmap
 * @param la   the start host liear address
 * @param size the number of bytes to transfer
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_rw_guest_mem(struct vm *vm, uintptr_t gpa,
		      pmap_t *pmap, uintptr_t la, size_t size, int write);
#define vdev_read_guest_mem(vm, gpa, pmap, la, size)			\
	vdev_rw_guest_mem((vm), (gpa), (pmap), (la), (size), 0)
#define vdev_write_guest_mem(vm, gpa, pmap, la, size)			\
	vdev_rw_guest_mem((vm), (gpa), (pmap), (la), (size), 1)

/*
 * Notify a virtual device to synchronize with the host device.
 *
 * @param vm  the virtual machine operating on
 * @param vid the virtual device
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_sync_dev(struct vm *vm, vid_t vid);

/*
 * Notify the virtual machine a virtual device completes the synchronization.
 *
 * @param vm  the virtual machine operating on
 * @param vid the virtual device
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_sync_complete(struct vm *vm, vid_t vid);

/*
 * Notify the virtual machine a virtual device is ready to work.
 *
 * @param vm  the virtual machine operating on
 * @param vid the virtual device
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_device_ready(struct vm *vm, vid_t vid);

/*
 * Get the process of a virtual device.
 */
struct proc *vdev_get_dev(struct vm *vm, vid_t vid);

#else /* !_KERN_ */

#include <types.h>

typedef int vid_t;

#endif /* _KERN_ */

enum vdev_msg_magic {
	RAISE_IRQ_REQ = 0xabcd0001,
	LOWER_IRQ_REQ,
	TRIGGER_IRQ_REQ,
	READ_IOPORT_REQ,
	WRITE_IOPORT_REQ,
	RETURN_IOPORT,
	DEVIDE_READY,
	DEV_SYNC_REQ,
	DEV_SYNC_COMPLETE
};

struct raise_irq_req {
	uint32_t	magic;
	uint8_t		irq;
};

struct lower_irq_req {
	uint32_t	magic;
	uint8_t		irq;
};

struct trigger_irq_req {
	uint32_t	magic;
	uint8_t		irq;
};

struct read_ioport_req {
	uint32_t	magic;
	uint16_t	port;
	uint8_t		width;
};

struct write_ioport_req {
	uint32_t	magic;
	uint16_t	port;
	uint8_t		width;
	uint32_t	val;
};

struct return_ioport {
	uint32_t	magic;
	uint16_t	port;
	uint8_t		width;
	uint32_t	val;
};

struct device_rdy {
	uint32_t	magic;
	vid_t		vid;
};

struct dev_sync_req {
	uint32_t	magic;
	vid_t		vid;
};

struct dev_sync_complete {
	uint32_t	magic;
	vid_t		vid;
};

typedef union {
	struct raise_irq_req	__req0;
	struct lower_irq_req	__req1;
	struct trigger_irq_req	__req2;
	struct read_ioport_req	__req3;
	struct write_ioport_req	__req4;
	struct dev_sync_req	__req5;
} dev_req_t;

typedef union {
	struct return_ioport	__ack0;
	struct device_rdy	__ack1;
	struct dev_sync_complete __ack2;
} dev_ack_t;

#endif /* !_SYS_VIRT_VMM_IODEV_H_ */

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

/*
 * Initialize the virtual device structures of a virtual machine.
 */
void vdev_init(struct vm *);

/*
 * Registration/Unregistration Interface.
 */

/*
 * Register/Unregister a process to handle I/O port readings/writings.
 *
 * @param vm    the virtual machine to which the process is registered
 * @param p     the process responsible for the readings/writings
 * @param port  the I/O port being read/written
 * @param width the data width
 * @param write is it the reading or the writing operation?
 *
 * @return 0 if the registration/unregistration succeeds; otherwise, return a
 *         non-zero value.
 */
int vdev_register_ioport(struct vm *vm, struct proc *p,
			 uint16_t port, data_sz_t width, bool write);
int vdev_unregister_ioport(struct vm *vm, struct proc *p,
			   uint16_t port, data_sz_t width, bool write);

/*
 * Register/Unregister a process as the source of an interrupt.
 *
 * @param vm  the virtual machine to which the process is registered
 * @param p   the process which is the source of the interrupt
 * @param irq the interrupt number (PIC/APIC vector number, not IDT vector
 *            number)
 *
 * @return 0 if the registration/unregistration succeeds; otherwise, return a
 *           non-zero value.
 */
int vdev_register_irq(struct vm *vm, struct proc *p, uint8_t irq);
int vdev_unregister_irq(struct vm *vm, struct proc *p, uint8_t irq);

/*
 * Register/Unregister a process as the programmable interrupt controller.
 *
 * @param vm the virtual machine to which the process is registered
 * @param p  the process which is the programmable interrupt controller
 *
 * @param 0 if the registration/unregistraion succeeds; otherwise, return a
 *        non-zero value.
 */
int vdev_register_pic(struct vm *vm, struct proc *p);
int vdev_unregister_pic(struct vm *vm, struct proc *p);

/*
 * Register/Unregister a process to handle the memory readings/writings to a
 * range of the guest physical memory.
 *
 * @param vm   the virtual machine to which the process is registered
 * @param p    the process responsible for the readings/writings
 * @param gpa  the start guest physical address of the memory region
 * @param hla  the host linear address where the memory region is mapped to
 * @param size the size in bytes of the memory region
 *
 * @return 0 if the registration/unregistration succeeds; otherwise, return a
 *         non-zero value.
 */
int vdev_register_mmio(struct vm *vm, struct proc *p,
		       uintptr_t gpa, uintptr_t hla, size_t size);
int vdev_unregister_mmio(struct vm *vm, struct proc *p,
			 uintptr_t gpa, size_t size);

/*
 * Virtual Device Interface
 */

/*
 * Read/Write an I/O port of a virtual device.
 *
 * @param vm    the virtual machine to be operated on
 * @param port  the I/O port to be read/written
 * @param width the data width
 * @param data  where the data comes from or goes to
 *
 * @return 0 if the reading/writing succeeds; otherwise, return a non-zero value.
 */
int vdev_ioport_read(struct vm *vm,
		     uint16_t port, data_sz_t width, uint32_t *data);
int vdev_ioport_write(struct vm *vm,
		      uint16_t port, data_sz_t width, uint32_t data);

/*
 * Read/Write the host I/O port.
 *
 * @param vm    the virtual machine to which the virtual device is attached
 * @param port  the port to be read/written
 * @param width the data width
 * @param data  where the data is
 *
 * @return 0 if the operation succeeds; otherwise, return a non-zero value.
 */
int vdev_host_ioport_read(struct vm *vm,
			  uint16_t port, data_sz_t width, uint32_t *data);
int vdev_host_ioport_write(struct vm *vm,
			   uint16_t port, data_sz_t width, uint32_t data);

/*
 * Raise/Lower an IRQ line/vector of the virtual programmable interrupt
 * controller.
 *
 * @param vm  the virtual machine to be operated on
 * @param irq the interrupt number
 *
 * @return 0 if the operation succeeds; otherwise, return a non-zero value.
 */
int vdev_raise_irq(struct vm *vm, uint8_t irq);
int vdev_trigger_irq(struct vm *vm, uint8_t irq);
int vdev_lower_irq(struct vm *vm, uint8_t irq);

/*
 * Notify the virtual machine an interrupt comes from the virtual PIC.
 *
 * vdev_notify_irq() doesn't send the IRQ number to the virtual machine.
 * Instead, the virtual machine uses vdev_get_irq() and vdev_read_irq() to
 * get the IRQ number from the virtual PIC.
 *
 * @param vm  the virtual machine where the interrupt comes to
 *
 * @return 0 if the notification is sent; otherwise, return a non-zero value.
 */
int vdev_notify_irq(struct vm *vm);

/*
 * Get the highest-priority interrupt from the virtual PIC.
 *
 * vdev_get_irq() has no side-effect, i.e. it doesn't change the states of the
 * virtual PIC.
 *
 * @param vm  the virtual machine to which the virtual PIC is attached
 * @param irq where the interrupt number will be returned to; if -1 is returned,
 *            then there is no interrupt.
 *
 * @return 0 if the operation succeeds; otherwise, return a non-zero value.
 */
int vdev_get_irq(struct vm *vm, int *irq);

/*
 * Read the highest-priority interrupt from the virtual PIC.
 *
 * vdev_read_irq may have side-effects, e.g. it may clear the most significant
 * bit of IRR and set the corresponding bit of ISR of the virtual PIC. Which
 * side-effects would happen depends on the implementation of the virtual PIC.
 *
 * @param vm  the virtual machine to which the virtual PIC is attached
 * @param irq where the interrupt number will be returned to; if -1 is returned,
 *            then there is no interrupt.
 *
 * @return 0 if the operation succeeds; otherwise, return a non-zero value.
 */
int vdev_read_irq(struct vm *vm, int *irq);

/*
 * Notify a virtual device to synchronize with the physical device.
 *
 * The notifications are always triggered by physical external interrupts. When
 * an physical external interrupt comes, if a virtual device is registered as
 * the source of the same interrupt, CertiKOS will notify the virtual device to
 * synchronize with the physical devices.
 *
 * @param vm  the virtual machine to which the virtual device is attached
 * @param irq the interrupt number
 *
 * @return 0 if the notification succeeds; otherwise, return a non-zero value
 */
int vdev_notify_sync(struct vm *vm, uint8_t irq);

#else /* !_KERN_ */

#include <types.h>

#endif /* _KERN_ */

#define MAGIC_IOPORT_RW_REQ	0x76540001
#define MAGIC_IOPORT_READ_RET	0x76540002
#define MAGIC_IRQ_REQ		0x76540003
#define MAGIC_SYNC_IRQ		0x76540004
#define MAGIC_IRQ_READ_REQ	0x76540005
#define MAGIC_IRQ_READ_RET	0x76540006
#define MAGIC_DEVICE_READY	0x76540007
#define MAGIC_SYNC_COMPLETE	0x76540008

struct ioport_rw_req {
	uint32_t	magic;
	uint16_t	port;
	uint8_t		width;
	int		write;
	uint32_t	data;
};

struct ioport_read_ret {
	uint32_t	magic;
	uint32_t	data;
};

struct irq_req {
	uint32_t	magic;
	uint8_t		irq;
	int		trigger;/* -1: lower the irq line;
				    0: first lower and then raise the irq line;
				    1: raise the irq line. */
};

struct sync_req {
	uint32_t	magic;
	uint8_t		irq;
};

struct sync_complete {
	uint32_t	magic;
	uint8_t		irq;
};

struct irq_read_req {
	uint32_t	magic;
	int		read;	/* If read = 1, the request is sent by
				   vdev_read_irq(); if read = 0, the request is
				   sent bby vdev_get_irq(). */
};

struct irq_read_ret {
	uint32_t	magic;
	int		irq;
};

struct device_ready {
	uint32_t	magic;
};

#endif /* !_SYS_VIRT_VMM_IODEV_H_ */

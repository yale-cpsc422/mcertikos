#ifndef _SYS_VIRT_VMM_H_
#define _SYS_VIRT_VMM_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/dev/kbd.h>
#include <sys/virt/dev/pci.h>
#include <sys/virt/dev/pic.h>

#define VM_PHY_MEMORY_SIZE	(64 * 1024 * 1024)

#define MAX_IOPORT		0x10000
#define MAX_EXTINTR		0x100

typedef enum {
	SZ8, 	/* 1 byte */
	SZ16, 	/* 2 byte */
	SZ32	/* 4 byte */
} data_sz_t;

typedef void (*iodev_read_func_t)(struct vm *,
				  void *iodev, uint32_t port, void *data);
typedef void (*iodev_write_func_t)(struct vm *,
				   void *iodev, uint32_t port, void *data);
typedef void (*intr_handle_t)(struct vm *);

struct vm {
	void		*cookie;	/* processor-specific data */

	bool		exit_for_intr;	/**/

	struct {
		void			*dev;
		iodev_read_func_t	read_func;
		iodev_write_func_t	write_func;
		data_sz_t		read_size, write_size;
	} iodev[MAX_IOPORT];

	struct {
		void		*dev;
		intr_handle_t	handle;
	} extintr[MAX_EXTINTR];

	struct vpic	vpic;		/* virtual PIC (i8259) */
	struct vpci	vpci;		/* virtual PCI host */
	struct vkbd	vkbd;		/* virtual keyboard */

	bool		used;
};

/*
 * Machine-dependent VMM initialization function.
 *
 * @return 0 if no errors happen
 */
typedef int (*vmm_init_func_t)(void);

/*
 * Machine-dependent VM initialization function.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_init_func_t)(struct vm *);

/*
 * Machine-dependent function that start running a VM.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_run_func_t)(struct vm *);

/*
 * Machine-dependent function that handle the interrupt the execution of VM.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_handle_func_t)(struct vm *);

typedef enum {EVENT_INT, EVENT_NMI, EVENT_EXPT, EVENT_SWINT} event_t;

/*
 * Machine-dependent function that inject an exception/interrupt/software
 * interrupt into the VM.
 *
 * @param vm
 * @param type type of the injected TYPE
 * @param vector the interrupt number for an injected interrupt, or the
 *               exception number for an injected exception
 * @param ev the injected event needs an error code
 * @param errcode the error code for the injected event
 */
typedef int (*vm_inject_func_t)(struct vm *,
				event_t type, uint8_t vector,
				bool ev, uint32_t errcode);

/*
 * Each machine-dependent HVM implementation should define such a structure.
 */
struct vmm_ops {
	vmm_init_func_t		vmm_init;

	vm_init_func_t		vm_init;
	vm_run_func_t		vm_run;
	vm_handle_func_t	vm_handle;
	vm_inject_func_t	vm_inject;
};

/*
 * Top-level VMM initialization function.
 */
int vmm_init(void);

/*
 * Top-level VM initialization function.
 */
struct vm *vmm_init_vm(void);

/*
 * Top-level function launch a VM.
 */
int vmm_run_vm(struct vm *);

/*
 * Get the current VM.
 */
struct vm *vmm_cur_vm(void);

/*
 * Assert/Deassert an IRQ to VM.
 */
void vmm_set_vm_irq(struct vm *, int irq, int level);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_H_ */

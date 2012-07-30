#ifndef _SYS_VIRT_VMM_H_
#define _SYS_VIRT_VMM_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/dev/kbd.h>
#include <sys/virt/dev/nvram.h>
#include <sys/virt/dev/pci.h>
#include <sys/virt/dev/pic.h>
#include <sys/virt/dev/pit.h>
#include <sys/virt/dev/serial.h>
#include <sys/virt/dev/virtio_blk.h>
#include <sys/virt/dev/debug_dev.h>

#include <machine/trap.h>

#define VM_PHY_MEMORY_SIZE	(128 * 1024 * 1024)

#define VM_TIME_SCALE		1
#define VM_TSC_ADJUST		0

#define VM_PIT_FREQ		(1193182)
#define VM_TSC_FREQ		(800 * 1000 * 1000 / VM_TIME_SCALE)

#define MAX_IOPORT		0x10000
#define MAX_EXTINTR		0x100

typedef enum data_sz_t {
	SZ8, 	/* 1 byte */
	SZ16, 	/* 2 byte */
	SZ32	/* 4 byte */
} data_sz_t;

typedef enum {
	HYPERCALL_BITAND,
	HYPERCALL_BITOR,
	HYPERCALL_BITXOR,
	HYPERCALL_BITNOT,
	HYPERCALL_GETC,
	HYPERCALL_NULL,	/* XXX: should be the last one */
} hypercall_t;

typedef void (*iodev_read_func_t)(struct vm *,
				  void *iodev, uint32_t port, void *data);
typedef void (*iodev_write_func_t)(struct vm *,
				   void *iodev, uint32_t port, void *data);
typedef void (*intr_handle_t)(struct vm *);

struct vm {
	void		*cookie;	/* processor-specific data */

	volatile bool	exit_for_intr;	/* VMEXIT for interrupts */

	struct {
		void			*dev;
		iodev_read_func_t	read_func[3]; // read data size: 0-sz8, 1-sz16,2-sz32
		iodev_write_func_t	write_func[3];// write data size:0-sz8, 1-sz16,2-sz32
	} iodev[MAX_IOPORT];

	struct {
		void		*dev;
		intr_handle_t	handle;
	} extintr[MAX_EXTINTR];

	uint64_t	tsc;		/* TSC read by guests */

	struct vpic		vpic;	/* virtual PIC (i8259) */
	struct vpci_host	vpci;	/* virtual PCI host */
	struct vkbd		vkbd;	/* virtual keyboard controller (i8042) */
	struct vserial		vserial;/* virtual serial ports */
	struct vnvram		vnvram;	/* virtual NVRAM */
	struct vpit		vpit;	/* virtual PIT (i8254) */

	struct virtio_blk	blk;	/* virtio block device - hard drive */

	struct guest_debug_dev debug_dev;

	bool		used;
};

typedef enum vmm_sig {
	AMD_SVM,
	INTEL_VMX
} vmm_sig_t;

/*
 * Arch-dependent VMM initialization function.
 *
 * @return 0 if no errors happen
 */
typedef int (*vmm_init_func_t)(void);

/*
 * Arch-dependent VM initialization function.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_init_func_t)(struct vm *);

/*
 * Arch-dependent function that starts running a VM.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_run_func_t)(struct vm *);

/*
 * Arch-dependent function that handles VMEXIT events.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_exit_handle_func_t)(struct vm *);

/*
 * Arch-dependent function that handles interrupts in the guest.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_intr_handle_func_t)(struct vm *, uint8_t irqno);

/*
 * Arch-dependent function that gets last TSC when entering the guest
 *
 * @return TSC
 */
typedef uint64_t (*vm_enter_tsc_func_t)(struct vm *);

/*
 * Arch-dependent function that gets last TSC when exiting the gueste
 *
 * @return TSC
 */
typedef uint64_t (*vm_exit_tsc_func_t)(struct vm *);

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
 * Machine-dependent function that enables/disables the interception on I/O
 * port.
 */
typedef void (*vm_intercept_ioio_func_t)(struct vm *,
					 uint32_t port, data_sz_t, bool enable);

/*
 * Machine-dependent function that translates guest physical address to host
 * physical address.
 */
typedef uintptr_t (*vm_translate_gp2hp_func_t)(struct vm *, uintptr_t);

/*
 * Each machine-dependent HVM implementation should define such a structure.
 */
struct vmm_ops {
	vmm_sig_t			signature;

	vmm_init_func_t			vmm_init;

	vm_init_func_t			vm_init;
	vm_run_func_t			vm_run;
	vm_exit_handle_func_t		vm_exit_handle;
	vm_intr_handle_func_t		vm_intr_handle;
	vm_inject_func_t		vm_inject;
	vm_enter_tsc_func_t		vm_enter_tsc;
	vm_exit_tsc_func_t		vm_exit_tsc;

	vm_intercept_ioio_func_t	vm_intercept_ioio;

	vm_translate_gp2hp_func_t	vm_translate_gp2hp;
};

/*
 * Top-level VMM initialization function.
 */
int vmm_init(void);

/*
*  vm enable on Application Processor
*
*/
int vmm_init_on_ap(void);


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
 * Set the level of an IRQ of the virtualized PIC/APIC.
 * XXX: It's to set the level of an IRQ line, other than to trigger or
 *      untrigger an interrupt, e.g. for an edge-triggered interrupt IRQ1,
 *        vmm_set_vm_irq(vm, IRQ1, 1);
 *      does not trigger IRQ1 if the level of IRQ1 is already 1;
 *        vmm_set_vm_irq(vm, IRQ1, 0);
 *        vmm_set_vm_irq(vm, IRQ1, 1);
 *      emulate the edge trigger and conseqently do trigger the interrupt.
 */
void vmm_set_vm_irq(struct vm *, int irq, int level);

/*
 * VMM stage of interrupt handling.
 */
void vmm_handle_intr(struct vm *, uint8_t irqno);

/*
 * Read TSC of a VM.
 */
uint64_t vmm_rdtsc(struct vm *);

/*
 * Update the interception setup.
 */
void vmm_update(struct vm *);

/*
 * Translate the guest physical address to the host physical address.
 */
uintptr_t vmm_translate_gp2hp(struct vm *, uintptr_t);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_H_ */

#ifndef _SYS_VIRT_VMM_H_
#define _SYS_VIRT_VMM_H_

#ifdef _KERN_

#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <sys/virt/dev/pic.h>

#define MAX_VMID		32

#define VM_PHY_MEMORY_SIZE	(256 * 1024 * 1024)

#define VM_TIME_SCALE		1
#define VM_TSC_ADJUST		0

#define VM_PIT_FREQ		(1193182)
#define VM_TSC_FREQ		(800 * 1000 * 1000 / VM_TIME_SCALE)

#define MAX_IOPORT		0x10000
#define MAX_IRQ			0x100
#define MAX_VID			32

typedef enum {
	HYPERCALL_BITAND,
	HYPERCALL_BITOR,
	HYPERCALL_BITXOR,
	HYPERCALL_BITNOT,
	HYPERCALL_GETC,
	HYPERCALL_NULL,	/* XXX: should be the last one */
} hypercall_t;

typedef enum {
	EXIT_NONE,		/* no VMEXIT */
	EXIT_FOR_EXTINT,	/* exit for the external interrupt */
	EXIT_FOR_HLT,		/* exit for the hlt instruction */
	EXIT_FOR_OTHERS		/* exit for other reasons */
} exit_reason_t;;

typedef enum {
	VM_STOP,
	VM_RUNNING
} vm_stat_t;

struct vm;

typedef void (*iodev_read_func_t)(struct vm *,
				  void *iodev, uint32_t port, void *data);
typedef void (*iodev_write_func_t)(struct vm *,
				   void *iodev, uint32_t port, void *data);
typedef void (*intr_handle_t)(struct vm *);

struct vdev {
	struct vpic	vpic;
	spinlock_t	vpic_lk;

	spinlock_t	dev_lk;

	struct proc	*dev[MAX_VID];
	struct channel	*data_ch[MAX_VID], *sync_ch[MAX_VID];
	struct {
		spinlock_t	ioport_lk;
		vid_t		vid;
	} ioport[MAX_IOPORT];
	struct {
		spinlock_t	irq_lk;
		vid_t		vid;
	} irq[MAX_IRQ];
};

struct vm {
	void		*cookie;	/* processor-specific data */

	vmid_t		vmid;		/* the virtual machine ID */

	struct proc	*proc;		/* the process hosting the VM */

	exit_reason_t	exit_reason;
	volatile bool	handled;	/* Is the exit event handled? */

	struct vdev	vdev;		/* virtual devices attached to the VM */

	uint64_t	tsc;		/* guest TSC */

#ifdef TRACE_TOTAL_TIME
	uint64_t	start_tsc, total_tsc;
#endif

	vm_stat_t	state;
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

struct vmm_ops *vmm_ops;

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
 * Top-level function that launches a VM.
 */
int vmm_run_vm(struct vm *);

/*
 * Get the current VM.
 */
struct vm *vmm_cur_vm(void);

/*
 * VMM stage of interrupt handling.
 */
void vmm_handle_intr(struct vm *, uint8_t irqno);

/*
 * Read TSC of a VM.
 */
uint64_t vmm_rdtsc(struct vm *);

/*
 * Translate the guest physical address to the host physical address.
 */
uintptr_t vmm_translate_gp2hp(struct vm *, uintptr_t);

/*
 * Get the vm structure.
 */
struct vm* vmm_get_vm(vmid_t);

/*
 * Setup the IRQ line of the virtual interrupt handler (PIC/APIC).
 */
void vmm_set_irq(struct vm *, uint8_t irq, int mode);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_H_ */

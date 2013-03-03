#ifndef _SYS_VIRT_VMM_H_
#define _SYS_VIRT_VMM_H_

#ifdef _KERN_

#include <sys/mem.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <sys/virt/dev/pic.h>

#define MAX_VMID		1
#define MAX_IOPORT		0x10000
#define MAX_IRQ			0x100
#define MAX_VID			32

struct vm;

typedef enum {
	EXIT_NONE = 0,		/* no VMEXIT */
	EXIT_FOR_EXTINT,	/* exit for the external interrupt */
	EXIT_FOR_INTWIN,	/* exit for the interrupt window */
	EXIT_FOR_IOPORT,	/* exit for accessing an I/O port */
	EXIT_FOR_PGFLT,		/* exit for the page fault */
	EXIT_FOR_RDMSR,		/* exit for the rdmsr instruction */
	EXIT_FOR_WRMSR,		/* exit for the wrmsr instruction */
	EXIT_FOR_CPUID,		/* exit for the cpuid instruction */
	EXIT_FOR_RDTSC,		/* exit for the rdtsc/rdtscp instruction */
	EXIT_FOR_HYPERCALL,	/* exit for the hypercall */
	EXIT_FOR_INVAL_INSTR,	/* exit for the invalid instruction */
	EXIT_INVAL		/* invalid exit */
} exit_reason_t;

typedef union {
	struct {
		uint16_t  port;	/* I/O port */
		data_sz_t width;/* data width */
		bool      write;/* is write? */
		bool      rep;	/* has the prefix rep? */
		bool      str;	/* is a string operation? */
	} ioport;
	struct {
		uintptr_t addr;	/* the fault address */
	} pgflt;
} exit_info_t;

typedef enum {
	VM_STATE_STOP,		/* the virtual machine is stopped */
	VM_STATE_RUNNING	/* the virtual machine is running */
} vm_stat_t;

#ifdef TRACE_VIRT

#define VM_TRACE_OUTPUT_INTERVAL	5

struct vm_perf_trace {
	uint64_t	entry_time;	/* time of the latest VM entry */
	uint64_t	exit_time;	/* time of the latest VM exit */
	uint64_t	last_output_time;

	uint64_t	in_guest_time;	/* total VM running time */
	uint64_t	in_host_time;	/* total VMM handling time */

	uint64_t	entry_counter;	/* amount of VM entries */
	uint64_t	exit_counter;	/* amount of VM exits */

	uint64_t	total_ioport_time;	/* total time handling IO ports */
	uint64_t	total_ioport_counter;	/* total amount of VM exits
						   caused by I/O ports */
	uint64_t	total_irq_time;		/* total time handling interupts */
	uint64_t	total_irq_counter;	/* total amount of VM exits
						   caused by interrupts */

	uint64_t	total_intwin_time;	/* total time handling interrupt
						   windows */
	uint64_t	total_intwin_counter;	/* total amount of VM exits
						   caused by interrupt windows */

	uint64_t	total_msr_time;		/* total time handling MSRs */
	uint64_t	total_msr_counter;	/* total amount of VM exits
						   caused by MSRs */

	uint64_t	total_cpuid_time;	/* total time handling CPUIDs */
	uint64_t	total_cpuid_counter;	/* total amount of VM exits
						   caused by CPUIDs */

	uint64_t	total_pgflt_time;	/* total time handling pgflts */
	uint64_t	total_pgflt_counter;	/* total amount of VM exits
						   caused by page faults */

	uint64_t	total_tsc_time;		/* total time handling TSC */
	uint64_t	total_tsc_counter;	/* total amount of VM exits
						   caused by TSC */

	uint64_t	total_hcall_time;	/* total time handling hypercalls */
	uint64_t	total_hcall_counter;	/* total amount of VM exits
						   caused by hypercalls */

	uint64_t	other_time;		/* total time handling other
						   VM exits */
	uint64_t	other_counter;		/* total amount of VM exits
						   caused by other reasons */

#ifdef TRACE_GUEST_IOPORT
	/*
	 * ioport_time[i]   : total time handling a single I/O port i
	 * ioport_counter[i]: total amount of VM exits caused by a single I/O
	 *                    port i
	 */
	uint64_t	ioport_time[MAX_IOPORT];
	uint64_t	ioport_counter[MAX_IOPORT];
#endif

#ifdef TRACE_GUEST_INTR
	/*
	 * irq_time[i]   : total time handling a single interrupt i
	 * irq_counter[i]: total amount of VM exits caused by a single
	 *                 interrupt i
	 */
	uint64_t	irq_time[MAX_IRQ];
	uint64_t	irq_counter[MAX_IRQ];
#endif
};

#endif

/*
 * CertiKOS provides two categories of virtual devices: kernel virtual devices
 * and user virtual devices.
 *
 * The kernel virtual devices are parts of CertiKOS kernel.
 *
 * The user virtual devices are executing in the userspace.
 * - A user virtual device can serve at most one virtual machine.
 * - A user virtual device can emulate only the I/O ports it has already
 *   registered.
 * - A user virtual device can access the host I/O ports identical to the I/O
 *   ports it's emulating.
 * - A user virtual device can emulate only the interrupts it has already
 *   registered.
 * - XXX: Currently, there's no restriction of the guest physical memory access
 *        on the user virtual devices.
 */

struct vdev {
	spinlock_t	dev_lk;

	/*
	 * kernel virtual devices
	 */

	/* virtual interrupt controller */
	struct vpic	vpic;
	spinlock_t	vpic_lk;

	/*
	 * user virtual devices
	 */

	struct proc	*dev[MAX_VID];
	struct channel	*dev_in[MAX_VID];
	struct channel	*dev_out[MAX_VID];

	struct {
		spinlock_t ioport_lk;
		vid_t vid;
	} ioport[MAX_IOPORT];	/* I/O ports registry */

	struct {
		spinlock_t irq_lk;
		vid_t vid;
	} irq[MAX_IRQ];		/* interrupts registry */
};

struct vm {
	vmid_t		vmid;	/* the identity of the virtual machine */
	struct proc	*proc;	/* the process hosting the virtual machine */
	vm_stat_t	state;	/* the state of the virtual machine */
	bool		used;	/* is the virtual machine in use? */

	uint64_t	cpufreq;/* the frequency (in Hz) of the virtual CPU */
	size_t		memsize;/* the size (in byte) of the virtual memory */
	uint64_t	tsc;	/* the current value of the virtual TSC */

	exit_reason_t	exit_reason;	/* the reason of the latest VMEXIT */
	exit_info_t	exit_info;	/* the information of the latest VMEXIT */
	volatile bool	exit_handled;	/* is the latest VMEXIT handled? */

	struct vdev	vdev;	/* the virtual devices of the virtual machine */

#ifdef TRACE_VIRT
	struct vm_perf_trace trace;	/* performace trace information */
#endif

	void		*cookie;
};

typedef enum {
	AMD_SVM,
	INTEL_VMX
} vmm_sig_t;

typedef enum {
	GUEST_EAX, GUEST_EBX, GUEST_ECX, GUEST_EDX, GUEST_ESI, GUEST_EDI,
	GUEST_EBP, GUEST_ESP, GUEST_EIP, GUEST_EFLAGS,
	GUEST_CR0, GUEST_CR2, GUEST_CR3, GUEST_CR4,
	GUEST_MAX_REG
} guest_reg_t;

typedef enum {
	GUEST_CS, GUEST_DS, GUEST_ES, GUEST_FS, GUEST_GS, GUEST_SS,
	GUEST_LDTR, GUEST_TR, GUEST_GDTR, GUEST_IDTR,
	GUEST_MAX_SEG_DESC
} guest_seg_t;

#define GUEST_SEG_TYPE_MASK	0xf
#define GUEST_SEG_ATTR_S	(1 << 4)
#define GUEST_SEG_DPL_SHIFT	(1 << 5)
#define GUEST_SEG_DPL_MASK	0x60
#define GUEST_SEG_ATTR_P	(1 << 7)
#define GUEST_SEG_ATTR_AVL	(1 << 12)
#define GUEST_SEG_ATTR_L	(1 << 13)
#define GUEST_SEG_ATTR_D	(1 << 14)
#define GUEST_SEG_ATTR_B	(1 << 14)
#define GUEST_SEG_ATTR_G	(1 << 15)
#define GUEST_SEG_ATTR_UNUSABLE	(1 << 16)

#define GUEST_SEG_ATTR_A	(1 << 0)
#define GUEST_SEG_ATTR_W	(1 << 1)	/* for data segments */
#define GUEST_SEG_ATTR_R	(1 << 1)	/* for code segments */
#define GUEST_SEG_ATTR_E	(1 << 2)	/* for data segments */
#define GUEST_SEG_ATTR_C	(1 << 2)	/* for code segments */

#define GUEST_SEG_TYPE_CODE	0xa
#define GUEST_SEG_TYPE_DATA	0x2
#define GUEST_SEG_TYPE_LDT	0x2
#define GUEST_SEG_TYPE_TSS_BUSY	0x3
#define GUEST_SEG_TYPE_TSS	0xb

struct guest_seg_desc {
	uint16_t	sel;
	uint64_t	base;
	uint32_t	lim;
	uint32_t	ar;
};

typedef enum {
	EVENT_EXTINT,		/* external interrupt */
	EVENT_NMI,		/* non-maskable interrupt */
	EVENT_EXCEPTION,	/* exception */
	EVENT_SWINT		/* software interrupt */
} guest_event_t;

typedef enum {
	INSTR_IN, INSTR_OUT, INSTR_RDMSR, INSTR_WRMSR, INSTR_CPUID, INSTR_RDTSC,
	INSTR_HYPERCALL
} instr_t;

/*
 * Each architecture-dependent HVM implementation must provide such a structure.
 *
 * - signature: indicates which hardware virtualization technology is used
 *
 * - int hw_init(void)
 *   Enable the hardware virtualization on the current CPU core.
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int intercept_ioport(struct vm *vm, uint16_t port, bool enable)
 *   Enable/Disable intercepting an I/O port in a virtual machine.
 *   @param vm     the virtual machine
 *   @param port   the I/O port number
 *   @param enable TRUE - enable the interception;
 *                 FALSE - disable the interception
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int intercept_all_ioports(struct vm *vm, bool enable)
 *   Enable/Disable intercepting all I/O ports in a virtual machine.
 *   @param vm     the virtual machine
 *   @param enable TRUE - enable the interception;
 *                 FALSE - disable the interception
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int intercept_msr(struct vm *vm, uint32_t msr, int rw)
 *   Enable/Disable intercepting a MSR in a virtual machine.
 *   @param vm     the virtual machine
 *   @param msr    the address of the MSR
 *   @param rw     00 - disable intercepting both rdmsr and wrmsr;
 *                 01 - enable intercepting rdmsr, and disable wrmsr;
 *                 10 - enable intercepting wrmsr, and disable rdmsr;
 *                 11 - enable intercepting both rdmsr and wrmsr
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int intercept_all_msrs(struct vm *vm, int rw)
 *   Enable/Disable intercepting all MSRs in a virtual machine.
 *   @param vm     the virtual machine
 *   @param rw     00 - disable intercepting both rdmsr and wrmsr;
 *                 01 - enable intercepting rdmsr, and disable wrmsr;
 *                 10 - enable intercepting wrmsr, and disable rdmsr;
 *                 11 - enable intercepting both rdmsr and wrmsr
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int intercept_intr_window(struct vm *vm, bool enable)
 *   Enable/Disable intercepting the interrupt windows in a virtual machine. The
 *   interrupt window is the period during which CPU is able to respond to the
 *   external interrupts.
 *   @param vm     the virtual machine
 *   @param enable TRUE - enable the interception;
 *                 FALSE - disable the interception
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int vm_init(struct vm *vm)
 *   Architecture-dependent initialization of a virtual machine.
 *   @param vm the virtual machine
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int vm_run(struct vm *vm)
 *   Run a virtual machine. When vm_run() returns, either a exit event is
 *   happening, or there's a fatal error which is not handled properly by
 *   CertiKOS.
 *   @param vm the virtual machine
 *   @return 0 if a exit event is happening; otherwise, return a non-zero value
 *
 * - int get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val)
 *   Get the value of a guest register.
 *   @param vm  the virtual machine
 *   @param reg the guest register
 *   @param val where the value of the guest register is returned
 *   @return 0 if successful and the value of the guest register is put at val;
 *           otherwise, return a non-zero value and the value at val is
 *           undefined
 *
 * - int set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
 *   Set a guest register.
 *   @param vm  the virtual machine
 *   @param reg the guest register
 *   @param val the value of the guest register
 *   @return 0 if successful; otherwise, return a non-zero value and the guest
 *           register is in the state before the setting
 *
 * - int get_msr(struct vm *vm, uint32_t msr, uint64_t *val)
 *   Get the vaule of a guest MSR.
 *   @param vm  the virtual machine
 *   @param msr the encode of the guest MSR
 *   @param val where the value of the guest MSR is returned
 *   @return 0 if successful and the value of the guest MSR is put at val;
 *           otherwise, return a non-zero value and the value at val is
 *           undefined
 *
 * - int set_msr(struct vm *vm, uint32_t msr, uint64_t val)
 *   Set the guest MSR.
 *   @param vm  the virtual machine
 *   @param msr the encode of the guest MSR
 *   @param val the value of the guest MSR
 *   @return 0 if successful ; otherwise, return a non-zero value and the guest
 *           MSR is in the state before the setting
 *
 * - int get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
 *   Get the descriptor of a guest segment.
 *   @param vm   the virtual machine
 *   @param seg  the guest segment
 *   @param desc where the descriptor of the guest segment is returned
 *   @return 0 if successful and the descriptor is put at desc; otherwise,
 *           return a non-zero value and the value at desc is undefined
 *
 * - int set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
 *   Set the descriptor of a guest segment.
 *   @param vm   the virtual machine
 *   @param seg  the guest segment
 *   @param desc the descriptor
 *   @return 0 if successful; otherwise, return a non-zero value and the
 *           descriptor of the guest segment is in the state before the setting
 *
 * - int get_mmap(struct vm *vm, uintptr_t gpa, uintptr_t *hpa)
 *   Get the host physical address to which a guest physical memoey page is
 *   mapped.
 *   @param vm  the virtual machine
 *   @param gpa the guest physical address of the guest physical memory page,
 *              which should be aligned to the lower boundary of the memory page
 *   @param hpa where the host physical address is returned
 *   @return 0 if successful and the host physical address is put at hpa;
 *           otherwise, return a non-zero value and the value at hpa is
 *           undefined
 *
 * - int set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa)
 *   Map a guest physical memory page to a host physical memory page.
 *   @param vm  the virtual machine
 *   @param gpa the guest physical address of the guest physical memory page,
 *              which should be aligned to the lower boundary of the memory page
 *   @param hpa the host physical address of the host physical memory page,
 *              which should be aligned to the lower boundary of the memory page
 *   @return 0 if successful; otherwise, return a non-zero value and the mapping
 *           of the guest physical memory page is in the state before the
 *           mapping
 *
 * - int unset_mmap(struct vn *vm, uintptr_t gpa)
 *   Unmap a guest physical memory page from the host physical memory page.
 *   @param vm  the virtual machine
 *   @param gpa the guest physical address of the guest physical memory page,
 *              which should be aligned to the lower boundary of the memory page
 *   @return 0 if successful; otherwise, return a non-zero value and the mapping
 *             of the guest physical memory page is in the state before the
 *             unmapping
 *
 * - int inject_event(struct vm *vm, guest_event_t type, uint8_t vector,
 *                    uint32_t errcode, bool ev)
 *   Inject a vector event to a virtual machine.
 *   @param vm      the virtual machine
 *   @param type    the type of the vector event
 *   @param vector  the vector of the vector event
 *   @param errcode the error code of the vector event
 *   @param ev      TRUE - the error code of the vector event is valid;
 *                  FALSE - the error code of the vector event is invalid
 *   @return 0 if successful; otherwise, return a non-zero value
 *
 * - int get_next_eip(struct vm *vm, instr_t instr, uint32_t *val)
 *   Get the guest EIP of the next instruction of a virtual machine.
 *   @param vm    the virtual machine
 *   @param instr the current instruction
 *   @param val   where the value of the guest EIP is returned
 *   @return 0 if successful and the guest EIP of the next instruction is put at
 *           val; otherwise, return a non-zero value and the value at val is
 *           undefined
 *
 * - int get_cpuid(struct vm *vm, uint32_t id,
 *                 uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
 *   Get the guest cpuid.
 *   @param vm  the virtual machine
 *   @param in_eax
 *   @param in_ecx the encode of the id
 *   @param eax
 *   @param ebx
 *   @param ecx
 *   @param edx    where the guest %eax, %ebx, %ecx and %edx are returned
 *   @return 0 if successful and the guest %eax, %ebx, %ecx and %edx are put at
 *           eax, ebx, ecx and edx respectively; otherwise, return a non-zero
 *           value and the values at eax, ebx, ecx and edx are all undefined
 *
 * - bool pending_event(struct vm *vm)
 *   Is there any pending events which were injected before the latest exit?
 *   @param vm the virtual machine
 *   @return TRUE if there is at least one pending event; otherwise, return
 *           FALSE
 *
 * - bool intr_shadow(struct vm *vm)
 *   Is the virtual machine in the interrupt shadow at the latest exit?
 *   @param vm the virtual machine
 *   @return TRUE is it's in the interrupt shadow; otherwise, return FALSE
 */
struct vmm_ops {
	vmm_sig_t	signature;

	int (*hw_init)(void);

	int (*intercept_ioport)(struct vm *vm, uint16_t port, bool enable);
	int (*intercept_all_ioports)(struct vm *vm, bool enable);

	int (*intercept_msr)(struct vm *vm, uint32_t msr, int rw);
	int (*intercept_all_msrs)(struct vm *vm, int rw);

	int (*intercept_intr_window)(struct vm *vm, bool enable);

	int (*vm_init)(struct vm *vm);
	int (*vm_run)(struct vm *vm);

	int (*get_reg)(struct vm *vm, guest_reg_t reg, uint32_t *val);
	int (*set_reg)(struct vm *vm, guest_reg_t reg, uint32_t val);

	int (*get_msr)(struct vm *vm, uint32_t msr, uint64_t *val);
	int (*set_msr)(struct vm *vm, uint32_t msr, uint64_t val);

	int (*get_desc)(struct vm *vm,
			guest_seg_t seg, struct guest_seg_desc *desc);
	int (*set_desc)(struct vm *vm,
			guest_seg_t seg, struct guest_seg_desc *desc);

	int (*get_mmap)(struct vm *vm, uintptr_t gpa, uintptr_t *hpa);
	int (*set_mmap)(struct vm *vm, uintptr_t gpa, uintptr_t hpa);
	int (*unset_mmap)(struct vm *vm, uintptr_t gpa);

	int (*inject_event)(struct vm *vm, guest_event_t type,
			    uint8_t vector, uint32_t errcode, bool ev);

	int (*get_next_eip)(struct vm *vm, instr_t instr, uint32_t *val);

	int (*get_cpuid)(struct vm *vm, uint32_t in_eax, uint32_t in_ecx,
			 uint32_t *eax, uint32_t *ebx,
			 uint32_t *ecx, uint32_t *edx);

	bool (*pending_event)(struct vm *vm);
	bool (*intr_shadow)(struct vm *vm);
};

/*
 * Initialize the Virtual Machine Management module.
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vmm_init(void);

/*
 * Create a virtual machine.
 *
 * @param cpufreq the guest CPU frequency in Hz
 * @param memsize the guest memory size in byte
 *
 * @return the pointer to the virtual machine if successful; otherwise, return
 *         NULL
 */
struct vm *vmm_create_vm(uint64_t cpufreq, size_t memsize);

/*
 * Run a virtual machine.
 *
 * @param vm the virtual machine
 *
 * @return 0 if the virtual machine teminates normally; otherwise, return a
 *         non-zero value.
 */
int vmm_run_vm(struct vm *vm);

/*
 * Get the virtual machine which is running on the processor where the caller is.
 *
 * @return a pointer to the virtual machine if there's such a virtual machine;
 *         otherwise, return NULL.
 */
struct vm *vmm_cur_vm(void);

/*
 * Read the time stamp counter (TSC) of a virtual machine.
 *
 * @param vm the virtual machine
 *
 * @return the value of the time stamp counter of the virtual machine
 */
uint64_t vmm_rdtsc(struct vm *vm);

/*
 * Get the host physical address to which a guest physical memory page is mapped.
 *
 * @param vm  the virtual machine
 * @param gpa the guest physical address of the guest physical memory page
 * @param hpa the host physical address to which the guest physical memory page
 *            is mapped
 *
 * @return 0 if the guest physical memory is mapped in the virtual machine;
 *         otherwise, return a non-zero value and hpa is invalid.
 */
int vmm_get_mmap(struct vm *vm, uintptr_t gpa, uintptr_t *hpa);

/*
 * Map a guest memory page to the specified host physical memory page.
 *
 * @param vm  the virtual machine
 * @param gpa the guest physical address of the guest physical memory page
 * @param pi  the host physical memory page
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vmm_set_mmap(struct vm *vm, uintptr_t gpa, pageinfo_t *pi);

/*
 * Unmap a guest memory page from the host physical memory page.
 *
 * @param vm  the virtual machine
 * @param gpa the guest physical address of the guest physical memory page
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vmm_unset_mmap(struct vm *vm, uintptr_t gpa);

/*
 * Enable/Disable intercepting a guest I/O port.
 *
 * @param vm     the virtual machine
 * @param port   the I/O port
 * @param enable TRUE - enable; FALSE - disable
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int vmm_intercept_ioport(struct vm *vm, uint16_t port, bool enable);

/*
 * Handle the external interrupts which are happening in the virtual machine.
 *
 * @param vm  the virtual machine
 * @param irq the interrupt number
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vmm_handle_extint(struct vm *vm, uint8_t irq);

/*
 * Transfer data from the host physical memory to the guest physical memory.
 *
 * @param vm       the virtual machine
 * @param dest_gpa the destination guest physical address
 * @param src_hpa  the source host physical address
 * @param size     how many bytes will be transferred
 *
 * @return 0 if successful; otherwise, return a non-zero value and the guest
 *         physical memory maybe not in the state before the transfer.
 */
int vmm_memcpy_to_guest(struct vm *vm,
			uintptr_t dest_gpa, uintptr_t src_hpa, size_t size);

/*
 * Transfer data from the guest physical memory to the host physical memory.
 *
 * @param vm       the virtual machine
 * @param dest_hpa the destination host physical address
 * @param src_gpa  the source guest physical address
 * @param size     how many bytes will be transferred
 *
 * @return 0 if successful; otherwise, return a non-zero value and the host
 *         physical memory maybe not in the state before the transfer.
 */
int vmm_memcpy_to_host(struct vm *vm,
		       uintptr_t dest_hpa, uintptr_t src_gpa, size_t size);

/*
 * Translate a guest physical address to the host physical address.
 */
uintptr_t vmm_translate_gp2hp(struct vm *vm, uintptr_t gpa);

#ifdef _CCOMP_

int ccomp_vmm_set_msr(struct vmm_ops *ops,
		      struct vm *vm, uint32_t msr, uint64_t *v);

#endif

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_H_ */

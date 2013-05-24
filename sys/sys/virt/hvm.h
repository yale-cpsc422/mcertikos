#ifndef _SYS_VIRT_HVM_H_
#define _SYS_VIRT_HVM_H_

#ifdef _KERN_

#include <sys/proc.h>
#include <sys/types.h>

#else

#include <types.h>

#endif

typedef enum {
	EXIT_REASON_NONE = 0,	/* no VMEXIT */
	EXIT_REASON_EXTINT,	/* exit for the external interrupt */
	EXIT_REASON_INTWIN,	/* exit for the interrupt window */
	EXIT_REASON_IOPORT,	/* exit for accessing an I/O port */
	EXIT_REASON_PGFLT,	/* exit for the page fault */
	EXIT_REASON_RDMSR,	/* exit for the rdmsr instruction */
	EXIT_REASON_WRMSR,	/* exit for the wrmsr instruction */
	EXIT_REASON_CPUID,	/* exit for the cpuid instruction */
	EXIT_REASON_RDTSC,	/* exit for the rdtsc/rdtscp instruction */
	EXIT_REASON_INVAL_INSTR,/* exit for the invalid instruction */
	EXIT_REASON_INVAL	/* invalid exit */
} exit_reason_t;

typedef union {
	/* valid when exiting for I/O ports */
	struct {
		uint16_t  port;	/* I/O port */
		data_sz_t width;/* data width */
		bool      write;/* is write? */
		bool      rep;	/* has the prefix rep? */
		bool      str;	/* is a string operation? */
	} ioport;

	/* valid when exiting for EPT/NPT page faults */
	struct {
		uintptr_t addr;	/* the fault address */
	} pgflt;
} exit_info_t;

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
} guest_instr_t;

#ifdef _KERN_

#define MAX_VMID	1

struct vm {
	int		vmid;		/* identifier of the virtual machine */
	int		inuse;		/* in use? */

	struct proc	*proc;		/* the owner process */

	exit_reason_t	exit_reason;	/* the reason of the latest VMEXIT */
	exit_info_t	exit_info;	/* the information of the latest VMEXIT */

	uint8_t		guest_irq;

	void		*cookie;
};

typedef enum {
	AMD_SVM,
	INTEL_VMX
} hvm_sig_t;

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
 * - int set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type)
 *   Map a guest physical memory page to a host physical memory page.
 *   @param vm  the virtual machine
 *   @param gpa the guest physical address of the guest physical memory page,
 *              which should be aligned to the lower boundary of the memory page
 *   @param hpa the host physical address of the host physical memory page,
 *              which should be aligned to the lower boundary of the memory page
 *   @param type the cache type
 *   @return 0 if successful; otherwise, return a non-zero value and the mapping
 *           of the guest physical memory page is in the state before the
 *           mapping
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
struct hvm_ops {
	hvm_sig_t	signature;

	int (*hw_init)(void);

	int (*intercept_ioport)(struct vm *vm, uint16_t port, bool enable);
	int (*intercept_msr)(struct vm *vm, uint32_t msr, int rw);
	int (*intercept_intr_window)(struct vm *vm, bool enable);

	int (*vm_init)(struct vm *vm);
	int (*vm_run)(struct vm *vm);

	int (*get_reg)(struct vm *vm, guest_reg_t reg, uint32_t *val);
	int (*set_reg)(struct vm *vm, guest_reg_t reg, uint32_t val);

	int (*get_desc)(struct vm *vm,
			guest_seg_t seg, struct guest_seg_desc *desc);
	int (*set_desc)(struct vm *vm,
			guest_seg_t seg, struct guest_seg_desc *desc);

	int (*set_mmap)(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type);

	int (*inject_event)(struct vm *vm, guest_event_t type,
			    uint8_t vector, uint32_t errcode, bool ev);

	int (*get_next_eip)(struct vm *vm, guest_instr_t instr, uint32_t *val);

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
int hvm_init(void);

/*
 * Create a descriptor for a virtual machine.
 *
 * @return the descriptor of the virtual machine if successful; otherwise,
 *         return NULL
 */
struct vm *hvm_create_vm(void);

/*
 * Run the virtual machine. hvm_run_vm() returns when an error or a VMEXIT
 * happens.
 *
 * @param vm the virtual machine
 *
 * @return 0 if a VMEXIT happens; otherwise, return a non-zero value
 */
int hvm_run_vm(struct vm *vm);

/*
 * Map a guest memory page to the specified host physical memory page.
 *
 * @param vm  the virtual machine
 * @param gpa the guest physical address of the guest physical memory page
 * @param hpa the host physical address of the host physical memory page
 * @param type the cache type
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int hvm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type);

int hvm_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val);
int hvm_get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val);

int hvm_set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc);
int hvm_get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc);

int hvm_get_next_eip(struct vm *vm, guest_instr_t instr, uintptr_t *neip);

int hvm_inject_event(struct vm *vm, guest_event_t type,
		     uint8_t vector, uint32_t errcode, bool ev);
int hvm_pending_event(struct vm *vm);
int hvm_intr_shadow(struct vm *vm);

int hvm_intercept_ioport(struct vm *vm, uint16_t port, bool enabled);
int hvm_intercept_msr(struct vm *vm, uint32_t msr, bool enabled);
int hvm_intercept_intr_window(struct vm *vm, bool enabled);

struct vm *hvm_get_vm(int vmid);

bool hvm_available(void);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_HVM_H_ */

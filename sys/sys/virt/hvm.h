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
#ifndef __COMPCERT__
	uint64_t	base;
#else
	uint32_t	base_lo;
	uint32_t	base_hi;
#endif
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

#ifndef _SYS_VIRT_HVM_H_
#define _SYS_VIRT_HVM_H_

#ifdef _KERN_

#include <lib/types.h>

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
	uint32_t	base;
	uint32_t	lim;
	uint32_t	ar;
};

typedef enum {
	EVENT_EXTINT,		/* external interrupt */
	EVENT_EXCEPTION		/* exception */
} guest_event_t;

typedef enum {
	INSTR_IN, INSTR_OUT, INSTR_RDMSR, INSTR_WRMSR, INSTR_CPUID, INSTR_RDTSC,
	INSTR_HYPERCALL
} guest_instr_t;

#ifdef _KERN_

/*
 * Initialize the Virtual Machine Management module.
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int hvm_init(void);

/*
 * Create a descriptor for a virtual machine.
 *
 * @return a non-negative ID of the virtual machine if successful; otherwise,
 *         return a negative value.
 */
int hvm_create_vm(void);

/*
 * Run the virtual machine. hvm_run_vm() returns when an error or a VMEXIT
 * happens.
 *
 * @param vmid the ID of the virtual machine
 *
 * @return 0 if a VMEXIT happens; otherwise, return a non-zero value
 */
int hvm_run_vm(int vmid);

/*
 * Map a guest memory page to the specified host physical memory page.
 *
 * @param vmid the ID of the virtual machine
 * @param gpa  the guest physical address of the guest physical memory page
 * @param hpa  the host physical address of the host physical memory page
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int hvm_set_mmap(int vmid, uintptr_t gpa, uintptr_t hpa);

/*
 * Set the value of a guest register (one of eax, ebx, ecx, edx, esi, edi,
 * ebp, esp, eip, eflags, cr0, cr2, cr3, cr4).
 *
 * @param vmid the ID of the virtual machine
 * @param reg  the guest register
 * @param val  the value of the register
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int hvm_set_reg(int vmid, guest_reg_t reg, uint32_t val);

/*
 * Get the value of a guest register (one of eax, ebx, ecx, edx, esi, edi,
 * ebp, esp, eip, eflags, cr0, cr2, cr3, cr4).
 *
 * @param vmid the ID of the virtual machine
 * @param reg  the guest register
 *
 * @return the 32-bit value of the register if it's one of one of eax, ebx, ecx,
 *         edx, esi, edi, ebp, esp, eip, eflags, cr0, cr2, cr3, cr4; otherwise,
 *         the returned value is undefined.
 */
uint32_t hvm_get_reg(int vmid, guest_reg_t reg);

/*
 * Set the content of a guest segment (one of cs, ds, es, fs, gs ,ss,
 * ldt, tss, gdt, idt).
 *
 * @param vmid the ID of the virtual machine
 * @param seg  the guest segment
 * @param sel  the selector of the segment
 * @param base the lower 32-bit of the base address of the segment
 * @param lim  the limitation of the segment
 * @param ar   the attributes of the segment
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int hvm_set_seg(int vmid, guest_seg_t seg,
		uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar);

/*
 * Get the address of the next instruction in a virtual machine.
 *
 * @param vmid  the ID of the virtual machine
 * @param instr the current instruction
 *
 * @return the address of the next instruction
 */
uint32_t hvm_get_next_eip(int vmid, guest_instr_t instr);

/*
 * Inject a vector event (one of the external interrupt, the exception, the NMI,
 * the software interrupt) to the virtual machine.
 *
 * @param vmid    the ID of the virtual machine
 * @param type    the event type
 * @param vector  the vector number of the event
 * @param errcode the error code of the event; it's ignored if ev == FALSE
 * @param ev      if errcode is valid
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int hvm_inject_event(int vmid, guest_event_t type, uint8_t vector,
		     uint32_t errcode, bool ev);

/*
 * Is there a pending event?
 *
 * @param vmid the ID of the virtual machine
 *
 * @return TRUE if there's a pending event; FALSE if not.
 */
int hvm_pending_event(int vmid);

/*
 * Is the virtual machine in the interrupt shadow?
 *
 * @param vmid the ID of the virtual machine
 *
 * @return TRUE if the virtual machine is in the interrupt shadow; FALSE if not.
 */
int hvm_intr_shadow(int vmid);

/*
 * Enable/Disable intercepting the interrupt window.
 *
 * @param vmid    the ID of the virtual machine
 * @param enabled TRUE - intercept; FALSE - not intercept
 */
void hvm_intercept_intr_window(int vmid, bool enabled);

/*
 * Get the exit reason.
 *
 * @param vmid the ID of the virtual machine
 *
 * @return the exit reason
 */
exit_reason_t hvm_exit_reason(int vmid);

/*
 * Get the I/O port of the last VMEXIT caused by accessing I/O port.
 *
 * @param vmid the ID of the virtual machine
 *
 * @return the I/O port if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
uint16_t hvm_exit_io_port(int vmid);

/*
 * Get the data width of the last VMEXIT caused by accessing I/O port.
 *
 * @param vmid the ID of the virtual machine
 *
 * @return the data width if there was at least one VMEXIT caused by accessing
 *         I/O port; otherwise, the returned value is undefined.
 */
data_sz_t hvm_exit_io_width(int vmid);

/*
 * Is the last VMEXIT caused by accessing I/O port a write operation?
 *
 * @param vmid the ID of the virtual machine
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool hvm_exit_io_write(int vmid);

/*
 * Does the last VMEXIT caused by accessing I/O port have a prefix "rep"?
 *
 * @param vmid the ID of the virtual machine
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool hvm_exit_io_rep(int vmid);

/*
 * Is the last VMEXIT caused by accessing I/O port a string operation?
 *
 * @param vmid the ID of the virtual machine
 *
 * @return TRUE/FALSE if there was at least one VMEXIT caused by accessing I/O
 *         port; otherwise, the returned value is undefined.
 */
bool hvm_exit_io_str(int vmid);

/*
 * Get the fault address of the last VMEXIT caused by NPT faults.
 *
 * @param vmid the ID of the virtual machine
 *
 * @return the fault address if there was at least one VMEXIT caused by NPT
 *         faults; otherwise, the returned value is undefined.
 */
uintptr_t hvm_exit_fault_addr(int vmid);

/*
 * Does the virtual machine ID correspond to a virtual machine?
 *
 * @param vmid the ID of the virtual machine
 *
 * @return TRUE if it's a valid ID; FALSE if not.
 */
bool hvm_valid_vm(int vmid);

/*
 * Is HVM available?
 *
 * @return TRUE if it's available; FALSE if not.
 */
bool hvm_available(void);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_HVM_H_ */

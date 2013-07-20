#ifndef _KERN_VIRT_COMMON_H_
#define _KERN_VIRT_COMMON_H_

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

#endif /* _KERN_VIRT_COMMON_H_ */

#ifndef __VMCB_H__
#define __VMCB_H__

#include <architecture/types.h>

#define MSR_SYSENTER_CS		0x174
#define MSR_SYSENTER_ESP	0x175
#define MSR_SYSENTER_EIP	0x176

/*
 * Attribute for segment selector. This is a copy of bit 40:47 & 52:55 of the
 * segment descriptor. */
union seg_attrs
{
	uint16_t bytes;
	struct {
		uint16_t type:4;    /* 0;  Bit 40-43 */
		uint16_t s:   1;    /* 4;  Bit 44 */
		uint16_t dpl: 2;    /* 5;  Bit 45-46 */
		uint16_t p:   1;    /* 7;  Bit 47 */
		uint16_t avl: 1;    /* 8;  Bit 52 */
		uint16_t l:   1;    /* 9;  Bit 53 */
		uint16_t db:  1;    /* 10; Bit 54 */
		uint16_t g:   1;    /* 11; Bit 55 */
	} fields;
} __attribute__ ((packed));

struct seg_selector {
	uint16_t		sel;
	union seg_attrs	attrs;
	uint32_t		limit;
	uint64_t		base;
} __attribute__ ((packed));

union eventinj {
	uint64_t bytes;
	struct {
		uint64_t vector:    8;
		uint64_t type:      3;
		uint64_t ev:        1;
		uint64_t resvd1:   19;
		uint64_t v:         1;
		uint64_t errorcode:32;
	} fields;
} __attribute__ ((packed));

enum EVENT_TYPES
{
	EVENT_TYPE_INTR = 0,
	EVENT_TYPE_NMI = 2,
	EVENT_TYPE_EXCEPTION = 3,
	EVENT_TYPE_SWINT = 4,
};

union vintr
{
	uint64_t bytes;
	struct {
	uint64_t tpr:          8;
	uint64_t irq:          1;
	uint64_t rsvd0:        7;
	uint64_t prio:         4;
	uint64_t ign_tpr:      1;
	uint64_t rsvd1:        3;
	uint64_t intr_masking: 1;
	uint64_t rsvd2:        7;
	uint64_t vector:       8;
	uint64_t rsvd3:       24;
    } fields;
} __attribute__ ((packed));

struct vmcb
{
	/*** Control Area ***/
	uint32_t cr_intercepts;          /* offset 0x00 */
	uint32_t dr_intercepts;          /* offset 0x04 */
	uint32_t exception_intercepts;   /* offset 0x08 */
	uint32_t general1_intercepts;    /* offset 0x0C */
	uint32_t general2_intercepts;    /* offset 0x10 */
	uint32_t res01;                  /* offset 0x14 */
	uint64_t res02;                  /* offset 0x18 */
	uint64_t res03;                  /* offset 0x20 */
	uint64_t res04;                  /* offset 0x28 */
	uint64_t res05;                  /* offset 0x30 */
	uint64_t res06;                  /* offset 0x38 */
	uint64_t iopm_base_pa;           /* offset 0x40 */
	uint64_t msrpm_base_pa;          /* offset 0x48 */
	uint64_t tsc_offset;             /* offset 0x50 */
	uint32_t guest_asid;             /* offset 0x58 */
	uint8_t  tlb_control;            /* offset 0x5C */
	uint8_t  res07[3];
	union vintr vintr;              /* offset 0x60 */
	uint64_t interrupt_shadow;       /* offset 0x68 */
	uint64_t exitcode;               /* offset 0x70 */
	uint64_t exitinfo1;              /* offset 0x78 */
	uint64_t exitinfo2;              /* offset 0x80 */
	union eventinj exitintinfo;    /* offset 0x88 */
	uint64_t np_enable;              /* offset 0x90 */   /* set 1 to enable nested page table */
	uint64_t res08[2];
	union eventinj eventinj;       /* offset 0xA8 */
	uint64_t n_cr3;                  /* offset 0xB0 */   /* physical memory of the VM --> physical memory of the PM */
	uint64_t res09[105];             /* offset 0xB8 pad to save area */

	/*** State Save Area ****/
	struct seg_selector es, cs, ss, ds, fs, gs, gdtr, ldtr, idtr, tr;      /* offset 1024 */
	uint64_t res10[5];
	uint8_t res11[3];
	uint8_t cpl;
	uint32_t res12;
	uint64_t efer;			/* offset 1024 + 0xD0 */
	uint64_t res13[14];
	uint64_t cr4;			/* loffset 1024 + 0x148 */
	uint64_t cr3;
	uint64_t cr0;
	uint64_t dr7;
	uint64_t dr6;
	uint64_t rflags;
	uint64_t rip;
	uint64_t res14[11]; /* reserved */
	uint64_t rsp;
	uint64_t res15[3]; /* reserved */
	uint64_t rax;
	uint64_t star;
	uint64_t lstar;
	uint64_t cstar;
	uint64_t sfmask;
	uint64_t kerngsbase;
	uint64_t sysenter_cs;
	uint64_t sysenter_esp;
	uint64_t sysenter_eip;
	uint64_t cr2;
	uint64_t pdpe0; /* reserved ? */
	uint64_t pdpe1; /* reserved ? */
	uint64_t pdpe2; /* reserved ? */
	uint64_t pdpe3; /* reserved ? */
	uint64_t g_pat;
	uint64_t res16[50];
	uint64_t res17[128];
	uint64_t res18[128];
} __attribute__ ((packed));

#define MSRPM_OFFSET1	0x0
#define MSRPM_OFFSET2	0x800
#define MSRPM_OFFSET3	0x1000
#define MSRPM_OFFSET4	0x1800

#define MSR_ADDR1_MIN	0x0
#define MSR_ADDR1_MAX	0x1FFF
#define MSR_ADDR2_MIN	0xC0000000
#define MSR_ADDR2_MAX	0xC0001FFF
#define MSR_ADDR3_MIN	0xC0010000
#define MSR_ADDR3_MAX	0xC0011FFF

extern void vmcb_check_consistency ( struct vmcb *vmcb );
extern void vmcb_dump( struct vmcb *vmcb);
extern void print_vmcb_state (struct vmcb *vmcb);
void print_vmcb_vintr_state (struct vmcb *vmcb);
void print_vmcb_intr_state (struct vmcb *vmcb);

#endif /* __VMCB_H__ */

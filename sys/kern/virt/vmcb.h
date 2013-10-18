#ifndef _VIRT_SVM_VMCB_H_
#define _VIRT_SVM_VMCB_H_

#ifdef _KERN_

#include <lib/export.h>

#include "common.h"

enum {
	INTERCEPT_INTR,
	INTERCEPT_NMI,
	INTERCEPT_SMI,
	INTERCEPT_INIT,
	INTERCEPT_VINTR,
	INTERCEPT_SELECTIVE_CR0,
	INTERCEPT_STORE_IDTR,
	INTERCEPT_STORE_GDTR,
	INTERCEPT_STORE_LDTR,
	INTERCEPT_STORE_TR,
	INTERCEPT_LOAD_IDTR,
	INTERCEPT_LOAD_GDTR,
	INTERCEPT_LOAD_LDTR,
	INTERCEPT_LOAD_TR,
	INTERCEPT_RDTSC,
	INTERCEPT_RDPMC,
	INTERCEPT_PUSHF,
	INTERCEPT_POPF,
	INTERCEPT_CPUID,
	INTERCEPT_RSM,
	INTERCEPT_IRET,
	INTERCEPT_INTn,
	INTERCEPT_INVD,
	INTERCEPT_PAUSE,
	INTERCEPT_HLT,
	INTERCEPT_INVLPG,
	INTERCEPT_INVLPGA,
	INTERCEPT_IOIO_PROT,
	INTERCEPT_MSR_PROT,
	INTERCEPT_TASK_SWITCH,
	INTERCEPT_FERR_FREEZE,
	INTERCEPT_SHUTDOWN,
	INTERCEPT_VMRUN,
	INTERCEPT_VMMCALL,
	INTERCEPT_VMLOAD,
	INTERCEPT_VMSAVE,
	INTERCEPT_STGI,
	INTERCEPT_CLGI,
	INTERCEPT_SKINIT,
	INTERCEPT_RDTSCP,
	INTERCEPT_ICEBP,
	INTERCEPT_WBINVD,
	INTERCEPT_MONITOR,
	INTERCEPT_MWAIT,
	INTERCEPT_MWAIT_COND,
	INTERCEPT_XSETBV,
};

#ifdef __COMPCERT__
#pragma pack(1)
#endif
struct vmcb_control_area {
	uint32_t	intercept_cr;
	uint32_t	intercept_dr;
	uint32_t	intercept_exceptions;
	uint32_t	intercept_lo;
	uint32_t	intercept_hi;
	uint8_t 	reserved_1[42];
	uint16_t 	pause_filter_count;
	uint32_t	iopm_base_pa_lo;
	uint32_t	iopm_base_pa_hi;
	uint32_t	msrpm_base_pa_lo;
	uint32_t	msrpm_base_pa_hi;
	uint32_t	tsc_offset_lo;
	uint32_t	tsc_offset_hi;
	uint32_t 	asid;
	uint8_t 	tlb_ctl;
	uint8_t 	reserved_2[3];
	uint32_t 	int_ctl;
	uint32_t 	int_vector;
	uint32_t 	int_state;
	uint8_t 	reserved_3[4];
	uint32_t 	exit_code;
	uint32_t 	exit_code_hi;
	uint32_t	exit_info_1_lo;
	uint32_t	exit_info_1_hi;
	uint32_t	exit_info_2_lo;
	uint32_t	exit_info_2_hi;
	uint32_t 	exit_int_info;
	uint32_t 	exit_int_info_err;
	uint32_t	nested_ctl_lo;
	uint32_t	nested_ctl_hi;
	uint8_t 	reserved_4[16];
	uint32_t 	event_inj;
	uint32_t 	event_inj_err;
	uint32_t	nested_cr3_lo;
	uint32_t	nested_cr3_hi;
	uint32_t	lbr_ctl_lo;
	uint32_t	lbr_ctl_hi;
	uint32_t 	clean;
	uint32_t 	reserved_5;
	uint32_t	next_rip_lo;
	uint32_t	next_rip_hi;
	uint8_t 	insn_len;
	uint8_t 	insn_bytes[15];
	uint8_t 	reserved_6[800];
} gcc_packed;

struct vmcb_seg {
	uint16_t 	selector;
	uint16_t 	attrib;
	uint32_t 	limit;
	uint32_t	base_lo;
	uint32_t	base_hi;
};

#ifdef __COMPCERT__
#pragma pack(1)
#endif
struct vmcb_save_area {
	struct vmcb_seg	es;
	struct vmcb_seg	cs;
	struct vmcb_seg	ss;
	struct vmcb_seg	ds;
	struct vmcb_seg	fs;
	struct vmcb_seg	gs;
	struct vmcb_seg	gdtr;
	struct vmcb_seg ldtr;
	struct vmcb_seg idtr;
	struct vmcb_seg tr;
	uint8_t 	reserved_1[43];
	uint8_t 	cpl;
	uint8_t 	reserved_2[4];
	uint32_t	efer_lo;
	uint32_t	efer_hi;
	uint8_t 	reserved_3[112];
	uint32_t	cr4_lo;
	uint32_t	cr4_hi;
	uint32_t	cr3_lo;
	uint32_t	cr3_hi;
	uint32_t	cr0_lo;
	uint32_t	cr0_hi;
	uint32_t	dr7_lo;
	uint32_t	dr7_hi;
	uint32_t	dr6_lo;
	uint32_t	dr6_hi;
	uint32_t	rflags_lo;
	uint32_t	rflags_hi;
	uint32_t	rip_lo;
	uint32_t	rip_hi;
	uint8_t 	reserved_4[88];
	uint32_t	rsp_lo;
	uint32_t	rsp_hi;
	uint8_t 	reserved_5[24];
	uint32_t 	rax_lo;
	uint32_t	rax_hi;
	uint32_t	star_lo;
	uint32_t 	star_hi;
	uint32_t 	lstar_lo;
	uint32_t 	lstar_hi;
	uint32_t 	cstar_lo;
	uint32_t 	cstar_hi;
	uint32_t 	sfmask_lo;
	uint32_t 	sfmask_hi;
	uint32_t 	kernel_gs_base_lo;
	uint32_t 	kernel_gs_base_hi;
	uint32_t 	sysenter_cs_lo;
	uint32_t 	sysenter_cs_hi;
	uint32_t 	sysenter_esp_lo;
	uint32_t 	sysenter_esp_hi;
	uint32_t 	sysenter_eip_lo;
	uint32_t 	sysenter_eip_hi;
	uint32_t 	cr2_lo;
	uint32_t 	cr2_hi;
	uint8_t 	reserved_6[32];
	uint32_t 	g_pat_lo;
	uint32_t 	g_pat_hi;
	uint32_t 	dbgctl_lo;
	uint32_t 	dbgctl_hi;
	uint32_t 	br_from_lo;
	uint32_t 	br_from_hi;
	uint32_t 	br_to_lo;
	uint32_t 	br_to_hi;
	uint32_t 	last_excp_from_lo;
	uint32_t 	last_excp_from_hi;
	uint32_t 	last_excp_to_lo;
	uint32_t 	last_excp_to_hi;
} gcc_packed;

#ifdef __COMPCERT__
#pragma pack(1)
#endif
struct vmcb {
	struct vmcb_control_area control;
	struct vmcb_save_area	 save;
} gcc_packed;

#define INTERCEPT_CR0_READ	0
#define INTERCEPT_CR3_READ	3
#define INTERCEPT_CR4_READ	4
#define INTERCEPT_CR8_READ	8
#define INTERCEPT_CR0_WRITE	(16 + 0)
#define INTERCEPT_CR3_WRITE	(16 + 3)
#define INTERCEPT_CR4_WRITE	(16 + 4)
#define INTERCEPT_CR8_WRITE	(16 + 8)

#define INTERCEPT_DR0_READ	0
#define INTERCEPT_DR1_READ	1
#define INTERCEPT_DR2_READ	2
#define INTERCEPT_DR3_READ	3
#define INTERCEPT_DR4_READ	4
#define INTERCEPT_DR5_READ	5
#define INTERCEPT_DR6_READ	6
#define INTERCEPT_DR7_READ	7
#define INTERCEPT_DR0_WRITE	(16 + 0)
#define INTERCEPT_DR1_WRITE	(16 + 1)
#define INTERCEPT_DR2_WRITE	(16 + 2)
#define INTERCEPT_DR3_WRITE	(16 + 3)
#define INTERCEPT_DR4_WRITE	(16 + 4)
#define INTERCEPT_DR5_WRITE	(16 + 5)
#define INTERCEPT_DR6_WRITE	(16 + 6)
#define INTERCEPT_DR7_WRITE	(16 + 7)

#define SVM_EVTINJ_VEC_MASK	0xff

#define SVM_EVTINJ_TYPE_SHIFT	8
#define SVM_EVTINJ_TYPE_MASK	(7 << SVM_EVTINJ_TYPE_SHIFT)

#define SVM_EVTINJ_TYPE_INTR	(0 << SVM_EVTINJ_TYPE_SHIFT)
#define SVM_EVTINJ_TYPE_NMI	(2 << SVM_EVTINJ_TYPE_SHIFT)
#define SVM_EVTINJ_TYPE_EXEPT	(3 << SVM_EVTINJ_TYPE_SHIFT)
#define SVM_EVTINJ_TYPE_SOFT	(4 << SVM_EVTINJ_TYPE_SHIFT)

#define SVM_EVTINJ_VALID	(1 << 31)
#define SVM_EVTINJ_VALID_ERR	(1 << 11)

#define SVM_EXITINTINFO_VEC_MASK	SVM_EVTINJ_VEC_MASK
#define SVM_EXITINTINFO_TYPE_MASK	SVM_EVTINJ_TYPE_MASK

#define	SVM_EXITINTINFO_TYPE_INTR	SVM_EVTINJ_TYPE_INTR
#define	SVM_EXITINTINFO_TYPE_NMI	SVM_EVTINJ_TYPE_NMI
#define	SVM_EXITINTINFO_TYPE_EXEPT	SVM_EVTINJ_TYPE_EXEPT
#define	SVM_EXITINTINFO_TYPE_SOFT	SVM_EVTINJ_TYPE_SOFT

#define SVM_EXITINTINFO_VALID		SVM_EVTINJ_VALID
#define SVM_EXITINTINFO_VALID_ERR	SVM_EVTINJ_VALID_ERR

#define SVM_EXITINFOSHIFT_TS_REASON_IRET	36
#define SVM_EXITINFOSHIFT_TS_REASON_JMP		38
#define SVM_EXITINFOSHIFT_TS_HAS_ERROR_CODE	44

#define SVM_EXITINFO_REG_MASK	0x0F

#define	SVM_EXIT_READ_CR0 	0x000
#define	SVM_EXIT_READ_CR3 	0x003
#define	SVM_EXIT_READ_CR4 	0x004
#define	SVM_EXIT_READ_CR8 	0x008
#define	SVM_EXIT_WRITE_CR0 	0x010
#define	SVM_EXIT_WRITE_CR3 	0x013
#define	SVM_EXIT_WRITE_CR4 	0x014
#define	SVM_EXIT_WRITE_CR8 	0x018
#define	SVM_EXIT_READ_DR0 	0x020
#define	SVM_EXIT_READ_DR1 	0x021
#define	SVM_EXIT_READ_DR2 	0x022
#define	SVM_EXIT_READ_DR3 	0x023
#define	SVM_EXIT_READ_DR4 	0x024
#define	SVM_EXIT_READ_DR5 	0x025
#define	SVM_EXIT_READ_DR6 	0x026
#define	SVM_EXIT_READ_DR7 	0x027
#define	SVM_EXIT_WRITE_DR0 	0x030
#define	SVM_EXIT_WRITE_DR1 	0x031
#define	SVM_EXIT_WRITE_DR2 	0x032
#define	SVM_EXIT_WRITE_DR3 	0x033
#define	SVM_EXIT_WRITE_DR4 	0x034
#define	SVM_EXIT_WRITE_DR5 	0x035
#define	SVM_EXIT_WRITE_DR6 	0x036
#define	SVM_EXIT_WRITE_DR7 	0x037
#define SVM_EXIT_EXCP_BASE      0x040
#define SVM_EXIT_INTR		0x060
#define SVM_EXIT_NMI		0x061
#define SVM_EXIT_SMI		0x062
#define SVM_EXIT_INIT		0x063
#define SVM_EXIT_VINTR		0x064
#define SVM_EXIT_CR0_SEL_WRITE	0x065
#define SVM_EXIT_IDTR_READ	0x066
#define SVM_EXIT_GDTR_READ	0x067
#define SVM_EXIT_LDTR_READ	0x068
#define SVM_EXIT_TR_READ	0x069
#define SVM_EXIT_IDTR_WRITE	0x06a
#define SVM_EXIT_GDTR_WRITE	0x06b
#define SVM_EXIT_LDTR_WRITE	0x06c
#define SVM_EXIT_TR_WRITE	0x06d
#define SVM_EXIT_RDTSC		0x06e
#define SVM_EXIT_RDPMC		0x06f
#define SVM_EXIT_PUSHF		0x070
#define SVM_EXIT_POPF		0x071
#define SVM_EXIT_CPUID		0x072
#define SVM_EXIT_RSM		0x073
#define SVM_EXIT_IRET		0x074
#define SVM_EXIT_SWINT		0x075
#define SVM_EXIT_INVD		0x076
#define SVM_EXIT_PAUSE		0x077
#define SVM_EXIT_HLT		0x078
#define SVM_EXIT_INVLPG		0x079
#define SVM_EXIT_INVLPGA	0x07a
#define SVM_EXIT_IOIO		0x07b
#define SVM_EXIT_MSR		0x07c
#define SVM_EXIT_TASK_SWITCH	0x07d
#define SVM_EXIT_FERR_FREEZE	0x07e
#define SVM_EXIT_SHUTDOWN	0x07f
#define SVM_EXIT_VMRUN		0x080
#define SVM_EXIT_VMMCALL	0x081
#define SVM_EXIT_VMLOAD		0x082
#define SVM_EXIT_VMSAVE		0x083
#define SVM_EXIT_STGI		0x084
#define SVM_EXIT_CLGI		0x085
#define SVM_EXIT_SKINIT		0x086
#define SVM_EXIT_RDTSCP		0x087
#define SVM_EXIT_ICEBP		0x088
#define SVM_EXIT_WBINVD		0x089
#define SVM_EXIT_MONITOR	0x08a
#define SVM_EXIT_MWAIT		0x08b
#define SVM_EXIT_MWAIT_COND	0x08c
#define SVM_EXIT_XSETBV		0x08d
#define SVM_EXIT_NPF  		0x400

#define SVM_EXIT_ERR		-1

#define SVM_IOPM_SIZE		(12 << 10)
#define SVM_MSRPM_SIZE		(8 << 10)

#define SEG_ATTR_G		(1 << 11)
#define SEG_ATTR_D		(1 << 10)
#define SEG_ATTR_B		(1 << 10)
#define SEG_ATTR_L		(1 << 9)
#define SEG_ATTR_AVL		(1 << 8)
#define SEG_ATTR_P		(1 << 7)
#define SEG_ATTR_DPL_SHIFT	5
#define SEG_ATTR_S		(1 << 4)

#define SEG_TYPE_CODE		0xa
#define SEG_TYPE_DATA		0x2
#define SEG_TYPE_LDT		0x2
#define SEG_TYPE_TSS_BUSY	0x3
#define SEG_TYPE_TSS		0xb

#define SVM_INTR_CTRL_VTPR		0xff
#define SVM_INTR_CTRL_VIRQ_SHIFT	8
#define SVM_INTR_CTRL_VIRQ		(1 << SVM_INTR_CTRL_VIRQ_SHIFT)
#define SVM_INTR_CTRL_PRIO_SHIFT	16
#define SVM_INTR_CTRL_PRIO		(0xf << SVM_INTR_CTRL_PRIO_SHIFT)
#define SVM_INTR_CTRL_IGN_VTPR_SHIFT	20
#define SVM_INTR_CTRL_IGN_VTPR		(1 << SVM_INTR_CTRL_IGN_VTPR_SHIFT)
#define SVM_INTR_CTRL_VINTR_MASK_SHIFT	24
#define SVM_INTR_CTRL_VINTR_MASK	(1 << SVM_INTR_CTRL_VINTR_MASK_SHIFT)
#define SVM_INTR_CTRL_VINTR_VEC_SHIFT	32
#define SVM_INTR_CTRL_VINTR_VEC		(0xff << SVM_INTR_CTRL_VINTR_VEC_SHIFT)

/* EXITINFO1 for IOIO interception */
#define SVM_EXITINFO1_PORT_SHIFT	16
#define SVM_EXITINFO1_PORT_MASK		(0xffff << SVM_EXITINFO1_PORT_SHIFT)
#define SVM_EXITINFO1_SEG_SHIFT		10
#define SVM_EXITINFO1_SEG_MASK		(0x7 << SVM_EXITINFO1_SEG_SHIFT)
#define SVM_EXITINFO1_A64		(1 << 9)
#define SVM_EXITINFO1_A32		(1 << 8)
#define SVM_EXITINFO1_A16		(1 << 7)
#define SVM_EXITINFO1_SZ32		(1 << 6)
#define SVM_EXITINFO1_SZ16		(1 << 5)
#define SVM_EXITINFO1_SZ8		(1 << 4)
#define SVM_EXITINFO1_REP		(1 << 3)
#define SVM_EXITINFO1_STR		(1 << 2)
#define SVM_EXITINFO1_TYPE_MASK		0x1
#define SVM_EXITINFO1_TYPE_OUT		0
#define SVM_EXITINFO1_TYPE_IN		1

/* EXITINFO1 for nested page fault */
#define SVM_EXITINFO1_NFP_P		(1ULL << 0)
#define SVM_EXITINFO1_NFP_W		(1ULL << 1)
#define SVM_EXITINFO1_NFP_U		(1ULL << 2)
#define SVM_EXITINFO1_NFP_RSV		(1ULL << 3)
#define SVM_EXITINFO1_NFP_ID		(1ULL << 4)
#define SVM_EXITINFO1_NFP_ADDR		(1ULL << 32)
#define SVM_EXITINFO1_NFP_WALK		(1ULL << 33)

/* functions */

/*
 * Initialize VMCB.
 */
void vmcb_init(void);

/*
 * Enable/Disbale intercepting the virtual interrupts.
 */
void vmcb_set_intercept_vint(void);
void vmcb_clear_intercept_vint(void);

/*
 * Clear the pending virtual interrupt in VMCB.
 */
void vmcb_clear_virq(void);

/*
 * Inject a virtual interrupt.
 */
void vmcb_inject_virq(void);

/*
 * Get the exit_code field of VMCB.
 *
 * @return the 32-bit value of the exit_code
 */
uint32_t vmcb_get_exit_code(void);

/*
 * Get the exit_info_1/exit_info_2 field of VMCB.
 *
 * @return the 32-bit value of the exit_info1/exit_info2
 */
uint32_t vmcb_get_exit_info1(void);
uint32_t vmcb_get_exit_info2(void);

/*
 * Get the exit_int_info field of VMCB.
 *
 * @return the 32-bit value of the exit_int_info
 */
uint32_t vmcb_get_exit_intinfo(void);

/*
 * Get the exit_int_info_error field of VMCB.
 *
 * @return the 32-bit value of the exit_int_info_error
 */
uint32_t vmcb_get_exit_interr(void);

/*
 * Is the interrupt shadow bit set in the int_state field of VMCB.
 *
 * @return TRUE if set; FALSE if not
 */
bool vmcb_check_int_shadow(void);

/*
 * Inject a vector event (one of external interrupt, exception, NMI, software
 * interrupt).
 *
 * @param type    the event type
 * @param vector  the vector number of the event
 * @param errcode the error code; it's ignored if ev == FALSE
 * @param ev      if errcode is valid
 */
void vmcb_inject_event(int type, uint8_t vector, uint32_t errcode, bool ev);

/*
 * Is there a pending event in VMCB?
 *
 * @return TRUE if there is a pending event; FALSE if not
 */
bool vmcb_check_pending_event(void);

/*
 * Get the next_rip field of VMCB.
 *
 * @return the next_rip
 */
uint32_t vmcb_get_next_eip(void);

/*
 * Set the guest segment in VMCB.
 *
 * @param seg   the guest segment
 * @param sel   the selector of the segment
 * @param base  the lower 32-bit of the base address of the segment
 * @param lim   the limitation of the segment
 * @param ar    the attributes of the segment
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
void vmcb_set_seg(guest_seg_t seg,
		  uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar);

/*
 * Set the guest registers in VMCB.
 *
 * @param reg the guest register
 * @param val the lower 32-bit of the value of the register
 */
void vmcb_set_reg(guest_reg_t reg, uint32_t val);

/*
 * Get the value of the guest register in VMCB.
 *
 * @param reg the guest register
 *
 * @return the lower 32-bit of the value of the guest register
 */
uint32_t vmcb_get_reg(guest_reg_t reg);

#endif /* _KERN_ */

#endif /* !_VIRT_SVM_VMCB_H_ */

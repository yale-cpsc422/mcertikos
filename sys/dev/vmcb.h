#ifndef _KERN_DEV_VMCB_H_
#define _KERN_DEV_VMCB_H_

#ifdef _KERN_

#include <lib/gcc.h>
#include <lib/types.h>

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

struct vmcb_control_area {
	uint32_t	intercept_cr;
	uint32_t	intercept_dr;
	uint32_t	intercept_exceptions;
	uint64_t	intercept;
	uint8_t 	reserved_1[42];
	uint16_t 	pause_filter_count;
	uint64_t 	iopm_base_pa;
	uint64_t 	msrpm_base_pa;
	uint64_t 	tsc_offset;
	uint32_t 	asid;
	uint8_t 	tlb_ctl;
	uint8_t 	reserved_2[3];
	uint32_t 	int_ctl;
	uint32_t 	int_vector;
	uint32_t 	int_state;
	uint8_t 	reserved_3[4];
	uint32_t 	exit_code;
	uint32_t 	exit_code_hi;
	uint64_t 	exit_info_1;
	uint64_t 	exit_info_2;
	uint32_t 	exit_int_info;
	uint32_t 	exit_int_info_err;
	uint64_t 	nested_ctl;
	uint8_t 	reserved_4[16];
	uint32_t 	event_inj;
	uint32_t 	event_inj_err;
	uint64_t 	nested_cr3;
	uint64_t 	lbr_ctl;
	uint32_t 	clean;
	uint32_t 	reserved_5;
	uint64_t 	next_rip;
	uint8_t 	insn_len;
	uint8_t 	insn_bytes[15];
	uint8_t 	reserved_6[800];
} gcc_packed;

struct vmcb_seg {
	uint16_t 	selector;
	uint16_t 	attrib;
	uint32_t 	limit;
	uint64_t 	base;
};

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
	uint64_t 	efer;
	uint8_t 	reserved_3[112];
	uint64_t	cr4;
	uint64_t 	cr3;
	uint64_t 	cr0;
	uint64_t 	dr7;
	uint64_t 	dr6;
	uint64_t 	rflags;
	uint64_t 	rip;
	uint8_t 	reserved_4[88];
	uint64_t 	rsp;
	uint8_t 	reserved_5[24];
	uint64_t 	rax;
	uint64_t 	star;
	uint64_t 	lstar;
	uint64_t 	cstar;
	uint64_t 	sfmask;
	uint64_t 	kernel_gs_base;
	uint64_t 	sysenter_cs;
	uint64_t 	sysenter_esp;
	uint64_t 	sysenter_eip;
	uint64_t 	cr2;
	uint8_t 	reserved_6[32];
	uint64_t 	g_pat;
	uint64_t 	dbgctl;
	uint64_t 	br_from;
	uint64_t 	br_to;
	uint64_t 	last_excp_from;
	uint64_t 	last_excp_to;
} gcc_packed;

struct vmcb {
	struct vmcb_control_area control;
	struct vmcb_save_area	 save;
} gcc_packed;

#endif /* _KERN_ */

#endif /* !_KERN_DEV_VMCB_H_ */

#ifndef _SYS_LIB_SEG_H_
#define _SYS_LIB_SEG_H_

#ifdef _KERN_

#include <lib/types.h>

typedef
struct taskgate {
	uint32_t	ts_link;
	uint32_t	ts_esp0;
	uint16_t	ts_ss0;
	uint16_t	ts_padding1;
	uint32_t	ts_esp1;
	uint16_t	ts_ss1;
	uint16_t	ts_padding2;
	uint32_t	ts_esp2;
	uint16_t	ts_ss2;
	uint16_t	ts_padding3;
	uint32_t	ts_cr3;
	uint32_t	ts_eip;
	uint32_t	ts_eflags;
	uint32_t	ts_eax;
	uint32_t        ts_ecx;
	uint32_t	ts_edx;
	uint32_t	ts_ebx;
	uint32_t	ts_esp;
	uint32_t	ts_ebp;
	uint32_t	ts_esi;
	uint32_t	ts_edi;
	uint16_t	ts_es;
	uint16_t	ts_padding4;
	uint16_t	ts_cs;
	uint16_t	ts_padding5;
	uint16_t	ts_ss;
	uint16_t	ts_padding6;
	uint16_t	ts_ds;
	uint16_t	ts_padding7;
	uint16_t	ts_fs;
	uint16_t	ts_padding8;
	uint16_t	ts_gs;
	uint16_t	ts_padding9;
	uint16_t	ts_ldt;
	uint16_t	ts_padding10;
	uint16_t	ts_trap;
	uint16_t	ts_iomb;
} tss_t;

#define CPU_GDT_KDATA	0x10	    /* kernel data */

void tss_switch(uint32_t pid);

#endif /* _KERN_ */

#endif /* !_SYS_LIB_SEG_H_ */

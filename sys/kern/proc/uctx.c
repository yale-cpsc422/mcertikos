#include "uctx.h"

#define NUM_PROC	64
#define UCTX_SIZE	17

/* typedef */
/* struct pushregs { */
/* 	uint32_t edi; */
/* 	uint32_t esi; */
/* 	uint32_t ebp; */
/* 	uint32_t oesp;		/\* Useless *\/ */
/* 	uint32_t ebx; */
/* 	uint32_t edx; */
/* 	uint32_t ecx; */
/* 	uint32_t eax; */
/* } pushregs; */

/* typedef */
/* struct tf_t { */
/* 	/\* registers and other info we push manually in trapasm.S *\/ */
/* 	pushregs regs; */
/* 	uint16_t es;		uint16_t padding_es; */
/* 	uint16_t ds;		uint16_t padding_ds; */
/* 	uint32_t trapno; */

/* 	/\* format from here on determined by x86 hardware architecture *\/ */
/* 	uint32_t err; */
/* 	uintptr_t eip; */
/* 	uint16_t cs;		uint16_t padding_cs; */
/* 	uint32_t eflags; */

/* 	/\* rest included only when crossing rings, e.g., user to kernel *\/ */
/* 	uintptr_t esp; */
/* 	uint16_t ss;		uint16_t padding_ss; */
/* } tf_t; */

unsigned int UCTX_LOC[NUM_PROC][UCTX_SIZE];

void
uctx_set(unsigned int pid, unsigned int idx, unsigned int val)
{
	UCTX_LOC[pid][idx] = val;
}

void
uctx_set_eip(unsigned int pid, unsigned int eip)
{
	UCTX_LOC[pid][U_EIP] = eip;
}

unsigned int
uctx_get(unsigned int pid, unsigned int idx)
{
	return UCTX_LOC[pid][idx];
}

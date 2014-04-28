/*
 * Derived from BHyVe (svn 237539).
 * Adapted for CertiKOS by Haozhong Zhang at Yale.
 *
 * XXX: BHyVe is a 64-bit hypervisor, while CertiKOS is 32-bit.
 */

/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VIRT_VMX_X86_H_
#define _VIRT_VMX_X86_H_

#ifdef _KERN_

#include <preinit/lib/debug.h>
#include <preinit/lib/gcc.h>
#include <preinit/lib/types.h>

#include "vmcs.h"

#define SEG_TYPE_MASK		0xf
#define SEG_ATTR_S		(1 << 4)
#define SEG_DPL_SHIFT		(1 << 5)
#define SEG_DPL_MASK		0x60
#define SEG_ATTR_P		(1 << 7)
#define SEG_ATTR_AVL		(1 << 12)
#define SEG_ATTR_L		(1 << 13)
#define SEG_ATTR_D		(1 << 14)
#define SEG_ATTR_B		(1 << 14)
#define SEG_ATTR_G		(1 << 15)
#define SEG_ATTR_UNUSABLE	(1 << 16)

#define SEG_ATTR_A		(1 << 0)
#define SEG_ATTR_W		(1 << 1)	/* for data segments */
#define SEG_ATTR_R		(1 << 1)	/* for code segments */
#define SEG_ATTR_E		(1 << 2)	/* for data segments */
#define SEG_ATTR_C		(1 << 2)	/* for code segments */

#define SEG_TYPE_CODE		0xa
#define SEG_TYPE_DATA		0x2
#define SEG_TYPE_LDT		0x2
#define SEG_TYPE_TSS_BUSY	0x3
#define SEG_TYPE_TSS		0xb

/* CPUID 0x1, ECX */
#define CPUID_FEATURE_VMX	(1<<5)		/* support VMX */

/* CR4.VMXE */
#define CR4_VMXE		(1<<13)		/* enable VMX */

/*
 * Section 5.2 "Conventions" from Intel Architecture Manual 2B.
 *
 *			error
 * VMsucceed		  0
 * VMFailInvalid	  1
 * VMFailValid		  2	see also VMCS VM-Instruction Error Field
 */
#define	VMX_SUCCESS		0
#define	VMX_FAIL_INVALID	1
#define	VMX_FAIL_VALID		2
#define	VMX_SET_ERROR_CODE(varname)					\
	do {								\
	__asm __volatile("	jnc 1f;"				\
			 "	mov $1, %0;"	/* CF: error = 1 */	\
			 "	jmp 3f;"				\
			 "1:	jnz 2f;"				\
			 "	mov $2, %0;"	/* ZF: error = 2 */	\
			 "	jmp 3f;"				\
			 "2:	mov $0, %0;"				\
			 "3:	nop"					\
			 :"=r" (varname));				\
	} while (0)

/* returns 0 on success and non-zero on failure */
static gcc_inline int
vmxon(char *region)
{
	int error;
	uint64_t addr;

	addr = (uintptr_t) region | 0ULL;

	__asm __volatile("vmxon %0" : : "m" (addr) : "cc", "memory");

	VMX_SET_ERROR_CODE(error);
	return (error);
}

static gcc_inline void
vmclear(struct vmcs *vmcs)
{
	int error;
	uint64_t addr;

	addr = (uintptr_t) vmcs | 0ULL;

	__asm __volatile("vmclear %0" : : "m" (addr) : "cc", "memory");

	VMX_SET_ERROR_CODE(error);
	if (error)
		KERN_PANIC("vmclear error %d", error);
}

static gcc_inline void
vmxoff(void)
{
	__asm __volatile("vmxoff");
}

static gcc_inline void
vmptrst(uintptr_t *addr)
{
	__asm __volatile("vmptrst %0" : : "m" (*addr) : "cc", "memory");
}

static gcc_inline void
vmptrld(struct vmcs *vmcs)
{
	int error;
	uint64_t addr;

	addr = (uintptr_t) vmcs | 0ULL;

	__asm __volatile("vmptrld %0" : : "m" (*(uint64_t *) &addr)
			 : "cc", "memory");

	VMX_SET_ERROR_CODE(error);
	if (error)
		KERN_PANIC("vmptrld error %d", error);
}

static gcc_inline uint32_t
vmread(uint32_t encoding)
{
	uint32_t val;
	__asm __volatile("vmread %1, %0" : "=r" (val) : "r" (encoding) : "cc");
	return val;
}

static gcc_inline void
vmwrite(uint32_t encoding, uint32_t val)
{
	uint8_t error;
	__asm __volatile("vmwrite %1, %2; setna %0"
			 : "=q" (error)
			 : "r" (val), "r" (encoding)
			 : "cc");
	if (unlikely(error))
		KERN_PANIC("vmwrite 0x%08x 0x%08x error %d\n",
			   val, encoding, error);
}

#define	INVVPID_TYPE_ADDRESS		0UL
#define	INVVPID_TYPE_SINGLE_CONTEXT	1UL
#define	INVVPID_TYPE_ALL_CONTEXTS	2UL

static gcc_inline void
invvpid(uint32_t type, uint16_t vpid, uint64_t la)
{
	int error;

	struct {
		uint16_t vpid; uint16_t res[3]; uint64_t la;
	} gcc_packed desc = { vpid, {0, 0, 0}, la };

	KERN_ASSERT(sizeof(desc) == 16);

	__asm __volatile("invvpid %0, %1" :: "m" (desc), "r" (type)
			 : "cc", "memory");

	VMX_SET_ERROR_CODE(error);
	if (error)
		KERN_PANIC("invvpid error %d", error);
}

#define	INVEPT_TYPE_SINGLE_CONTEXT	1
#define	INVEPT_TYPE_ALL_CONTEXTS	2

static gcc_inline void
invept(uint32_t type, uint64_t eptp)
{
	int error;

	struct {
		uint64_t eptp; uint64_t res;
	} gcc_packed desc = { eptp, 0 };

	KERN_ASSERT(sizeof(desc) == 16);

	__asm __volatile("invept %0, %1" :: "m" (desc), "r" (type)
			 : "cc", "memory");

	VMX_SET_ERROR_CODE(error);
	if (error)
		KERN_PANIC("invept error %d", error);
}

#endif /* _KERN_ */

#endif /* !_VIRT_VMX_X86_H_ */

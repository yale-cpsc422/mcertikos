/*
 * Derived from BHyVe (svn 237539).
 * Adapted for CertiKOS by Haozhong Zhang at Yale.
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

#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include "ept.h"
#include "vmx_msr.h"
#include "x86.h"

#ifdef DEBUG_EPT

#define EPT_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG(fmt);		\
	}

#else

#define EPT_DEBUG(fmt...)			\
	{					\
	}

#endif

#define	EPT_PWL4(cap)			((cap) & (1ULL << 6))
#define	EPT_MEMORY_TYPE_WB(cap)		((cap) & (1ULL << 14))
#define	EPT_PDE_SUPERPAGE(cap)		((cap) & (1ULL << 16))	/* 2MB pages */
#define	EPT_PDPTE_SUPERPAGE(cap)	((cap) & (1ULL << 17))	/* 1GB pages */
#define	INVVPID_SUPPORTED(cap)		((cap) & (1ULL << 32))
#define	INVEPT_SUPPORTED(cap)		((cap) & (1ULL << 20))

#define	INVVPID_ALL_TYPES_MASK		0xF0000000000ULL
#define	INVVPID_ALL_TYPES_SUPPORTED(cap)				\
	(((cap) & INVVPID_ALL_TYPES_MASK) == INVVPID_ALL_TYPES_MASK)

#define	INVEPT_ALL_TYPES_MASK		0x6000000ULL
#define	INVEPT_ALL_TYPES_SUPPORTED(cap)					\
	(((cap) & INVEPT_ALL_TYPES_MASK) == INVEPT_ALL_TYPES_MASK)

#define	EPT_PG_RD			(1 << 0)
#define	EPT_PG_WR			(1 << 1)
#define	EPT_PG_EX			(1 << 2)
#define	EPT_PG_MEMORY_TYPE(x)		((x) << 3)
#define	EPT_PG_IGNORE_PAT		(1 << 6)
#define	EPT_PG_SUPERPAGE		(1 << 7)

#define	EPT_ADDR_MASK			((uint64_t)-1 << 12)
#define EPT_ADDR_OFFSET_MASK		((1 << 12) - 1)

#define EPT_PML4_INDEX(gpa)		(((uint64_t) (gpa) >> 39) & 0x1ff)
#define EPT_PDPT_INDEX(gpa)		(((uint64_t) (gpa) >> 30) & 0x1ff)
#define EPT_PDIR_INDEX(gpa)		(((uint64_t) (gpa) >> 21) & 0x1ff)
#define EPT_PTAB_INDEX(gpa)		(((uint64_t) (gpa) >> 12) & 0x1ff)
#define EPT_PAGE_OFFSET(gpa)		((gpa) & EPT_ADDR_OFFSET_MASK)

static uint64_t page_sizes_mask;

static void
ept_free_mappings_helper(uint64_t *table, uint8_t dep)
{
	int i;

	KERN_ASSERT(table != NULL);
	KERN_ASSERT(dep < 4);

	for (i = 0; i < 512; i++) {
		if (table[i] & (EPT_PG_EX | EPT_PG_WR | EPT_PG_RD)) {
			uintptr_t addr = (uintptr_t) (table[i] & EPT_ADDR_MASK);
			if (dep < 3)
				ept_free_mappings_helper((uint64_t *) addr,
							 dep+1);
			else
				mem_page_free(mem_phys2pi(addr));
		}
	}
}

static void
ept_free_mappings(uint64_t *pml4ept)
{
	KERN_ASSERT(pml4ept != NULL);
	ept_free_mappings_helper(pml4ept, 0);
}

static int
ept_create_ptable(uint64_t *ptable, uintptr_t start, size_t len)
{
	int i;
	pageinfo_t *pi;
	uintptr_t addr, page;

	EPT_DEBUG("PTABEPT @ 0x%08x\n", ptable);

	KERN_ASSERT(ptable != NULL);

	EPT_DEBUG("Create EPT page table for 0x%08x ~ 0x%08x\n",
		  start, start + len);

	for (i = 0, addr = start; addr < start + len; i++, addr += 4096) {
		if ((pi = mem_page_alloc()) == NULL)
			return 1;

		page = mem_pi2phys(pi);
		ptable[i] = (page & EPT_ADDR_MASK) |
			EPT_PG_IGNORE_PAT | EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;

		/* EPT_DEBUG("\thpa 0x%08x ~ 0x%08x\n", page,  page+PAGESIZE-1); */
	}

	return 0;
}

static int
ept_create_pdt(uint64_t *pdt, uintptr_t start, size_t len)
{
	int i;
	pageinfo_t *pi;
	uint64_t *ptable;
	uintptr_t addr;
	size_t len2;

	EPT_DEBUG("PDTEPT @ 0x%08x\n", pdt);

	KERN_ASSERT(pdt != NULL);

	/* EPT_DEBUG("Create EPT PDT for 0x%08x ~ 0x%08x\n", start, start + len); */

	for (i = 0, addr = start; addr < start + len; i++, addr += 0x200000) {
		if ((pi = mem_page_alloc()) == NULL)
			return 1;

		ptable = (uint64_t *) mem_pi2phys(pi);
		pdt[i] = ((uintptr_t) ptable & EPT_ADDR_MASK) |
			EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;

		len2 = (len - addr > 0x200000) ? 0x200000 : len - addr;

		if (ept_create_ptable(ptable, addr, len2))
			return 1;
	}

	return 0;
}

static int
ept_create_pdpt(uint64_t *pdpt, uintptr_t start, size_t len)
{
	int i;
	pageinfo_t *pi;
	uint64_t *pdt;
	uintptr_t addr;
	size_t len2;

	EPT_DEBUG("PDPTEPT @ 0x%08x\n", pdpt);

	KERN_ASSERT(pdpt != NULL);

	EPT_DEBUG("Create EPT PDPT for 0x%08x ~ 0x%08x\n", start, start+len);

	for (i = 0, addr = start; addr < start + len; i++, addr += 0x40000000) {
		if ((pi = mem_page_alloc()) == NULL)
			return 1;

		pdt = (uint64_t *) mem_pi2phys(pi);
		pdpt[i] = ((uintptr_t) pdt & EPT_ADDR_MASK) |
			EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;

		len2 = (len - addr > 0x40000000) ? 0x40000000 : (len - addr);

		if (ept_create_pdt(pdt, addr, len2))
			return 1;
	}

	return 0;
}

int
ept_init(void)
{
	int page_shift;
	uint64_t cap;

	cap = rdmsr(MSR_VMX_EPT_VPID_CAP);

	/*
	 * Verify that:
	 * - page walk length is 4 steps
	 * - extended page tables can be laid out in write-back memory
	 * - invvpid instruction with all possible types is supported
	 * - invept instruction with all possible types is supported
	 */
	if (!EPT_PWL4(cap) ||
	    !EPT_MEMORY_TYPE_WB(cap) ||
	    !INVVPID_SUPPORTED(cap) ||
	    !INVVPID_ALL_TYPES_SUPPORTED(cap) ||
	    !INVEPT_SUPPORTED(cap) ||
	    !INVEPT_ALL_TYPES_SUPPORTED(cap))
		return 1;

	/* Set bits in 'page_sizes_mask' for each valid page size */
	page_shift = PAGESHIFT;
	page_sizes_mask = 1UL << page_shift;		/* 4KB page */

	page_shift += 9;
	if (EPT_PDE_SUPERPAGE(cap))
		page_sizes_mask |= 1UL << page_shift;	/* 2MB superpage */

	page_shift += 9;
	if (EPT_PDPTE_SUPERPAGE(cap))
		page_sizes_mask |= 1UL << page_shift;	/* 1GB superpage */

	return 0;
}

void
ept_invalidate_mappings(uint64_t pml4ept)
{
	invept(INVEPT_TYPE_SINGLE_CONTEXT, EPTP(pml4ept));
}

int
ept_create_mappings(uint64_t *pml4ept, size_t mem_size)
{
	pageinfo_t *pi;
	uint64_t *pdpt;

	EPT_DEBUG("PML4EPT @ 0x%08x\n", pml4ept);

	KERN_ASSERT(pml4ept != NULL);

	if ((pi = mem_page_alloc()) == NULL)
		return 1;

	pdpt = (uint64_t *) mem_pi2phys(pi);
	pml4ept[0] = ((uintptr_t) pdpt & EPT_ADDR_MASK) |
		EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;

	if (ept_create_pdpt(pdpt, 0, mem_size)) {
		ept_free_mappings(pml4ept);
		return 1;
	}

	return 0;
}

uintptr_t
ept_gpa_to_hpa(uint64_t *pml4ept, uintptr_t gpa)
{
	KERN_ASSERT(pml4ept != NULL);

	uint64_t pml4e, pdpte, pde, pte;
	uint64_t *pdpt, *pdir, *ptab;

	pml4e = pml4ept[EPT_PML4_INDEX(gpa)];
	if (!(pml4e & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return 0;
	pdpt = (uint64_t *)(uintptr_t) (pml4e & EPT_ADDR_MASK);

	pdpte = pdpt[EPT_PDPT_INDEX(gpa)];
	if (!(pdpte & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return 0;
	pdir = (uint64_t *)(uintptr_t) (pdpte & EPT_ADDR_MASK);

	pde = pdir[EPT_PDIR_INDEX(gpa)];
	if (!(pde & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return 0;
	ptab = (uint64_t *)(uintptr_t) (pde & EPT_ADDR_MASK);

	pte = ptab[EPT_PTAB_INDEX(gpa)];
	if (!(pte & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return 0;

	return ((pte & EPT_ADDR_MASK) | EPT_PAGE_OFFSET(gpa));
}

size_t
ept_copy_to_guest(uint64_t *pml4ept, uintptr_t dest, uintptr_t src, size_t sz)
{
	KERN_ASSERT(pml4ept != NULL);

	size_t remaining, copied;
	uintptr_t dest_hpa, dest_gpa, src_hpa, aligned_dest_hpa;

	if (sz == 0)
		return 0;

	remaining = sz;
	dest_gpa = dest;
	src_hpa = src;

	dest_hpa = ept_gpa_to_hpa(pml4ept, dest_gpa);
	if (dest_hpa == 0)
		KERN_PANIC("Copy from hpa 0x%08x to gpa 0x%08x\n",
			   src_hpa, dest_gpa);
	aligned_dest_hpa = ROUNDDOWN(dest_hpa, PAGESIZE);

	copied = (dest_hpa == aligned_dest_hpa) ?
		((remaining >= PAGESIZE) ? PAGESIZE : remaining) :
		((remaining >= PAGESIZE - (dest_hpa - aligned_dest_hpa)) ?
		 (PAGESIZE - (dest_hpa - aligned_dest_hpa)) : remaining);
	memcpy((uint8_t *) dest_hpa, (uint8_t *) src, copied);

	dest_gpa = ROUNDDOWN(dest_gpa, PAGESIZE) + PAGESIZE;
	src_hpa += copied;
	remaining -= copied;

	while (remaining) {
		dest_hpa = ept_gpa_to_hpa(pml4ept, dest_gpa);
		if (dest_hpa == 0)
			KERN_PANIC("Copy from hpa 0x%08x to gpa 0x%08x\n",
				   src_hpa, dest_gpa);
		copied = (remaining >= PAGESIZE) ? PAGESIZE : remaining;
		memcpy((uint8_t *) dest_hpa, (uint8_t *) src_hpa, copied);

		dest_gpa += PAGESIZE;
		src_hpa += copied;
		remaining -= copied;
	}

	return sz;
}

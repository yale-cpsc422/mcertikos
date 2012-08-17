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
#define EPT_2M_ADDR_MASK		((uint64_t)-1 << 21)
#define EPT_2M_ADDR_OFFSET_MASK		((1 << 21) - 1)

#define EPT_PML4_INDEX(gpa)		(((uint64_t) (gpa) >> 39) & 0x1ff)
#define EPT_PDPT_INDEX(gpa)		(((uint64_t) (gpa) >> 30) & 0x1ff)
#define EPT_PDIR_INDEX(gpa)		(((uint64_t) (gpa) >> 21) & 0x1ff)
#define EPT_PTAB_INDEX(gpa)		(((uint64_t) (gpa) >> 12) & 0x1ff)
#define EPT_PAGE_OFFSET(gpa)		((gpa) & EPT_ADDR_OFFSET_MASK)
#define EPT_2M_PAGE_OFFSET(gpa)		((gpa) & EPT_2M_ADDR_OFFSET_MASK)

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

/*
 * Initialzie EPT.
 *
 * - Check whether the processor supports EPT.
 * - Check the EPT features supported by the processor.
 */
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

/*
 * Invalidate the EPT TLB.
 */
void
ept_invalidate_mappings(uint64_t pml4ept)
{
	invept(INVEPT_TYPE_SINGLE_CONTEXT, EPTP(pml4ept));
}

/*
 * Create EPT page structures.
 *
 * - The lowest 2MB guest physical address is mapped by 4KB EPT pages.
 * - Other guest physical address is mapped by 2MB EPT pages.
 */
int
ept_create_mappings(uint64_t *pml4ept, size_t mem_size)
{
	KERN_ASSERT(pml4ept != NULL);
	KERN_ASSERT(mem_size != 0);

	pageinfo_t *pi;
	uintptr_t gpa;

	/*
	 * The lowest 2MB guest physical memory is mapped by 4KB EPT pages.
	 */
	for (gpa = 0; gpa < 0xa0000 && gpa < mem_size; gpa += PAGESIZE) {
		if ((pi = mem_page_alloc()) == NULL)
			return 2;
		if (ept_add_mapping(pml4ept, gpa, mem_pi2phys(pi),
				    PAT_WRITE_BACK, FALSE))
			return 1;
	}
	/*
	 * Guest VGA RAM 0xa0000 ~ 0xbffff is mapped to host VGA RAM 0xa0000
	 * ~ 0xbffff.
	 */
	for (gpa = 0xa0000; gpa < 0xc0000 && gpa < mem_size; gpa += PAGESIZE)
		if (ept_add_mapping(pml4ept, gpa, gpa, PAT_UNCACHEABLE, FALSE))
			return 1;
	for (gpa = 0xc0000; gpa < 0x200000 && gpa < mem_size; gpa += PAGESIZE) {
		if ((pi = mem_page_alloc()) == NULL)
			return 2;
		if (ept_add_mapping(pml4ept, gpa, mem_pi2phys(pi),
				    PAT_WRITE_BACK, FALSE))
			return 1;
	}

	/*
	 * Guest physical memory above 2MB is mapped by 2MB EPT pages.
	 */
	for (gpa = 0x200000; gpa < mem_size && gpa < 0xf0000000;
	     gpa += PAGESIZE * 512) {
		if ((pi = mem_pages_alloc_align(512, 9)) == NULL)
			return 2;
		if (ept_add_mapping(pml4ept, gpa, mem_pi2phys(pi),
				    PAT_WRITE_BACK, TRUE))
			return 1;
	}

	return 0;
}

/*
 * Add page structures which map the guest physical address gpa to the host
 * physical address hpa. If superpage is TRUE, 2MB EPT pages are used;
 * otherwise, 4KB pages are used.
 */
int
ept_add_mapping(uint64_t *pml4ept, uintptr_t gpa, uintptr_t hpa,
		uint8_t mem_type, bool superpage)
{
	KERN_ASSERT(pml4ept != NULL);

	uint64_t *pml4e, *pdpte, *pde, *pte;
	uint64_t *pdpt, *pdt, *ptab;
	pageinfo_t *pi;

	pml4e = &pml4ept[EPT_PML4_INDEX(gpa)];
	if (!(*pml4e & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX))) {
		EPT_DEBUG("Create PDPT for gpa 0x%08x.", gpa);

		pi = mem_page_alloc();
		if (pi == NULL)
			return 1;

		pdpt = (uint64_t *) mem_pi2phys(pi);
		memset(pdpt, 0, PAGESIZE);

		*pml4e = ((uintptr_t) pdpt & EPT_ADDR_MASK) |
			EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;
	} else {
		pdpt = (uint64_t *)(uintptr_t) (*pml4e & EPT_ADDR_MASK);
	}

	pdpte = &pdpt[EPT_PDPT_INDEX(gpa)];
	if (!(*pdpte & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX))) {
		EPT_DEBUG("Create PDT for gpa 0x%08x.\n", gpa);

		pi = mem_page_alloc();
		if (pi == NULL)
			return 1;

		pdt = (uint64_t *) mem_pi2phys(pi);
		memset(pdt, 0, PAGESIZE);

		*pdpte = ((uintptr_t) pdt & EPT_ADDR_MASK) |
			EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;
	} else {
		pdt = (uint64_t *)(uintptr_t) (*pdpte & EPT_ADDR_MASK);
	}

	pde = &pdt[EPT_PDIR_INDEX(gpa)];
	if (!(*pde & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX))) {
		if (superpage == TRUE) {
			/* use 2MB EPT pages */
			*pde = ((uintptr_t) hpa & EPT_2M_ADDR_MASK) |
				EPT_PG_SUPERPAGE | EPT_PG_IGNORE_PAT |
				EPT_PG_EX | EPT_PG_WR | EPT_PG_RD |
				EPT_PG_MEMORY_TYPE(mem_type);

			EPT_DEBUG("Add 2MB mapping: gpa 0x%08x ==> hpa 0x%08x.\n",
				gpa, hpa);
		} else {
			/* use 4KB EPT pages */
			EPT_DEBUG("Create page table for gpa 0x%08x.\n", gpa);

			pi = mem_page_alloc();

			if (pi == NULL)
				return 1;

			ptab = (uint64_t *) mem_pi2phys(pi);
			memset(ptab, 0, PAGESIZE);

			*pde = ((uintptr_t) ptab & EPT_ADDR_MASK) |
				EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;
		}
	} else {
		if (superpage == TRUE) {
			EPT_DEBUG("gpa 0x%08x is already mapped to hpa 0x%08x.\n",
				  gpa, (uintptr_t) pde & EPT_ADDR_MASK);
			return 1;
		} else {
			ptab = (uint64_t *)(uintptr_t) (*pde & EPT_ADDR_MASK);
		}
	}

	if (superpage == TRUE)
		return 0;

	pte = &ptab[EPT_PTAB_INDEX(gpa)];
	if (!(*pte & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX))) {
		*pte = ((uintptr_t) hpa & EPT_ADDR_MASK) |
			EPT_PG_IGNORE_PAT | EPT_PG_EX | EPT_PG_WR | EPT_PG_RD |
			EPT_PG_MEMORY_TYPE(mem_type);

		EPT_DEBUG("Add 4KB mapping: gpa 0x%08x ==> hpa 0x%08x.\n",
			  gpa, hpa);
	} else {
		EPT_DEBUG("gpa 0x%08x is already mapped to hpa 0x%08x.\n",
			  gpa, (uintptr_t) pte & EPT_ADDR_MASK);
		return 1;
	}

	return 0;
}

/*
 * Get the last level's EPT page structure entry for the guest address gpa.
 *
 * If 4KB EPT page is used, return the page table entry; if 2MB EPT page is
 * used, return the page directory entry.
 */
static gcc_inline uint64_t *
ept_get_page_entry(uint64_t *pml4ept, uintptr_t gpa)
{
	uint64_t pml4e, pdpte, pde;
	uint64_t *pdpt, *pdir, *ptab;

	pml4e = pml4ept[EPT_PML4_INDEX(gpa)];
	if (!(pml4e & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return NULL;
	pdpt = (uint64_t *)(uintptr_t) (pml4e & EPT_ADDR_MASK);

	pdpte = pdpt[EPT_PDPT_INDEX(gpa)];
	if (!(pdpte & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return NULL;
	pdir = (uint64_t *)(uintptr_t) (pdpte & EPT_ADDR_MASK);

	pde = pdir[EPT_PDIR_INDEX(gpa)];

	if (pde & EPT_PG_SUPERPAGE)
		/* 2MB EPT pages */
		return &pdir[EPT_PDIR_INDEX(gpa)];

	if (!(pde & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return NULL;
	ptab = (uint64_t *)(uintptr_t) (pde & EPT_ADDR_MASK);

	return &ptab[EPT_PTAB_INDEX(gpa)];
}

/*
 * Convert the guest physical address to the host physical address.
 */
uintptr_t
ept_gpa_to_hpa(uint64_t *pml4ept, uintptr_t gpa)
{
	KERN_ASSERT(pml4ept != NULL);

	uint64_t *entry;

	if ((entry = ept_get_page_entry(pml4ept, gpa)) == NULL)
		return 0;

	if (!(*entry & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
		return 0;

	if (*entry & EPT_PG_SUPERPAGE) {
		KERN_ASSERT(gpa >= 0x200000);
		return ((*entry & EPT_2M_ADDR_MASK) | EPT_2M_PAGE_OFFSET(gpa));
	} else {
		KERN_ASSERT(gpa < 0x200000);
		return ((*entry & EPT_ADDR_MASK) | EPT_PAGE_OFFSET(gpa));
	}
}

/*
 * Copy sz bytes data from the host physical address src to the guest physical
 * address dest.
 */
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
		KERN_PANIC("Cannot copy from hpa 0x%08x to gpa 0x%08x\n",
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
			KERN_PANIC("Cannot copy from hpa 0x%08x to gpa 0x%08x\n",
				   src_hpa, dest_gpa);
		copied = (remaining >= PAGESIZE) ? PAGESIZE : remaining;
		memcpy((uint8_t *) dest_hpa, (uint8_t *) src_hpa, copied);

		dest_gpa += PAGESIZE;
		src_hpa += copied;
		remaining -= copied;
	}

	return sz;
}

/*
 * Set the access permission of the guest physical address gpa.
 */
int
ept_set_permission(uint64_t *pml4ept, uintptr_t gpa, uint8_t perm)
{
	KERN_ASSERT(pml4ept != NULL);

	uint64_t *entry;

	if ((entry = ept_get_page_entry(pml4ept, gpa)) == NULL)
		return 1;

	*entry = (*entry & ~(uint64_t) 0x7) | (perm & 0x7);

	return 0;
}

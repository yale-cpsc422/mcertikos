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

#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>
#include <preinit/lib/x86.h>

#include "ept.h"
#include "vmx.h"
#include "vmx_msr.h"
#include "x86.h"

#ifdef DEBUG_EPT

#define EPT_DEBUG(fmt, ...)				\
	{						\
		KERN_DEBUG("EPT: "fmt, ##__VA_ARGS__);	\
	}

#else

#define EPT_DEBUG(fmt, ...)			\
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

#define PAGESHIFT 12

static uint64_t page_sizes_mask;

/*
 * Initialzie EPT.
 *
 * - Check whether the processor supports EPT.
 * - Check the EPT features supported by the processor.
 */
int
ept_init(void)
{
    unsigned int i, j;

	page_sizes_mask = 1UL << PAGESHIFT;		/* 4KB page */

    //initilize the static ept data structure.
    ept.pml4 = ((uintptr_t) ept.pdpt & EPT_ADDR_MASK) |
                    EPT_PG_EX | EPT_PG_WR | EPT_PG_RD; 
    for(i = 0; i < 4; i++)
    {
        ept.pdpt[i] = ((uintptr_t) ept.pdt[i] & EPT_ADDR_MASK) |
                        EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;
        for(j = 0; j < 512; j++)
        {
            ept.pdt[i][j] = ((uintptr_t) ept.ptab[i][j] & EPT_ADDR_MASK) |
                                EPT_PG_EX | EPT_PG_WR | EPT_PG_RD;
        }
    }

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
 * Add page structures which map the guest physical address gpa to the host
 * physical address hpa. 
 */
int
ept_add_mapping(uintptr_t gpa, uint64_t hpa, uint8_t mem_type)
{
    ept.ptab[EPT_PDPT_INDEX(gpa)][EPT_PDIR_INDEX(gpa)][EPT_PTAB_INDEX(gpa)] = (hpa & EPT_ADDR_MASK) |
			EPT_PG_IGNORE_PAT | EPT_PG_EX | EPT_PG_WR | EPT_PG_RD |
			EPT_PG_MEMORY_TYPE(mem_type);

    EPT_DEBUG("Add 4KB mapping: gpa 0x%08x ==> hpa 0x%llx.\n", gpa, hpa);

	return 0;
}

/*
 * Get the last level's EPT page structure entry for the guest address gpa.
 */
static gcc_inline uint64_t 
ept_get_page_entry(uintptr_t gpa)
{
	return ept.ptab[EPT_PDPT_INDEX(gpa)][EPT_PDIR_INDEX(gpa)][EPT_PTAB_INDEX(gpa)];
}

/*
 * Set the last level's EPT page structure entry for the guest address gpa.
 */
static gcc_inline void 
ept_set_page_entry(uintptr_t gpa, uint64_t val)
{
	ept.ptab[EPT_PDPT_INDEX(gpa)][EPT_PDIR_INDEX(gpa)][EPT_PTAB_INDEX(gpa)] = val;
}

/*
 * Convert the guest physical address to the host physical address.
 */
uint64_t
ept_gpa_to_hpa(uintptr_t gpa)
{
    uint64_t entry;

    entry = ept_get_page_entry(gpa);

    if (!(entry & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
        return 0;

    return ((entry & EPT_ADDR_MASK) | EPT_PAGE_OFFSET(gpa));
}

/*
 * Set the access permission of the guest physical address gpa.
 */
int
ept_set_permission(uintptr_t gpa, uint8_t perm)
{
	uint64_t entry;

	entry = ept_get_page_entry(gpa);

	ept_set_page_entry(gpa, (entry & ~(uint64_t) 0x7) | (perm & 0x7));

	return 0;
}

int
ept_mmap(uintptr_t gpa, uint64_t hpa, uint8_t mem_type)
{
	uint64_t pg_entry;

	/*
	 * XXX: ASSUME 4KB pages are used in both the EPT and the host page
	 *      structures.
	 */
	KERN_ASSERT(gpa == ROUNDDOWN(gpa, PAGESIZE));
	KERN_ASSERT(hpa == ROUNDDOWN(hpa, PAGESIZE));

	pg_entry = ept_get_page_entry(gpa);

	if (((pg_entry) & (EPT_PG_RD | EPT_PG_WR | EPT_PG_EX)))
	{
		KERN_WARN("Guest page 0x%08x is already mapped to 0x%llx.\n",
			  gpa, (pg_entry & EPT_ADDR_MASK));
		return 1;
	}

	return ept_add_mapping(gpa, hpa, mem_type);
}


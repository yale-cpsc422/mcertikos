/* See COPYRIGHT for copyright information. */

#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <inc/gcc.h>
#include <architecture/mem.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/mem/mem.h>

#include <kern/pmap/pmap.h>

// This modules blindly implements a 2-level page table
// data structure. It does not worry about reserving pages or
// maintaining page counts, except for the pages used by the pagetable

#define PTE_NULL ((pte_t)NULL)
typedef uint32_t pde_t;
typedef uint32_t pte_t;

void
pmap_init(void)
{
		  // We currently do nothing.
}

//
// Allocate a new page directory, initialized from the bootstrap pdir.
// Returns the new pdir with a reference count of 1.
//
pmap_t *
pmap_new(void)
{
	pageinfo *pi = mem_alloc();
	if (pi == NULL)
		return NULL;
	mem_incref(pi);
	pde_t *pdir = mem_pi2ptr(pi);
	assert (pdir != NULL);
	memset(pdir,0,PAGESIZE);
	return pdir;
}

//
// Free a page directory, and all page tables and mappings it may contain.
//
void
pmap_free(pmap_t *pmap)
{
		  pmap_remove(pmap,0,~0);
		  mem_free(mem_ptr2pi(pmap));
}



// This is a helper function. It should not be caled outside this module
pte_t *
pmap_walk(pde_t *pdir, uint32_t va, bool writing)
{
	uint32_t la = va;			// linear = virtual address
	pde_t *pde = &pdir[PDX(la)];		// find PDE
	pte_t *ptab;
	if (*pde & PTE_P) {			// ptab already exist?
		ptab = mem_ptr(PGADDR(*pde));
	} else {				// no - create?
		pageinfo *pi;
		if (!writing || (pi = mem_alloc()) == NULL)
			return NULL;
		mem_incref(pi);
		ptab = mem_pi2ptr(pi);
		memset(ptab,0,PAGESIZE);
		*pde = mem_pi2phys(pi) | PTE_A | PTE_P | PTE_W | PTE_U;
	}
	return &ptab[PTX(la)];
}

//
// Map the physical page 'pi' at user virtual address 'va'.
// The permissions (the low 12 bits) of the page table
//  entry should be set to 'perm | PTE_P'.
//
// Requirements
//   - If there is already a page mapped at 'va', it should be pmap_remove()d.
//   - If necessary, allocate a page able on demand and insert into 'pdir'.
//   - pi->refcount should be incremented if the insertion succeeds.
//   - The TLB must be invalidated if a page was formerly present at 'va'.
//
// Corner-case hint: Make sure to consider what happens when the same 
// pi is re-inserted at the same virtual address in the same pdir.
// What if this is the only reference to that page?
//
// RETURNS: 
//   a pointer to the inserted PTE on success (same as pmap_walk)
//   NULL, if page table couldn't be allocated
//
// Hint: The reference solution uses pmap_walk, pmap_remove, and mem_pi2phys.
//
// Returns pmap back, unless a failure occurs, in which case NULL
pmap_t*
pmap_insert(pmap_t *pmap, pageinfo *pi, uint32_t va, int perm)
{
	assert(pmap != NULL);
//    if (va > mem_max &&va < 0xb000000)
 //       cprintf("inserting address %x\n", va);
	pte_t* pte = pmap_walk(pmap, va, 1);
	if (pte == PTE_NULL) {
		return NULL;
	}
    if ((*pte & PTE_P)) {
        cprintf("already allocated page at %x, pte %x\n", va, *pte);
        return NULL; //should be an assert I think
    }
	*pte = mem_pi2phys(pi) | perm | PTE_P;
	if (va > mem_max) {
			 // cprintf("new pte %x\n", *pte);
	}
	return pmap;
}

//
// Unmap the physical pages starting at user virtual address 'va'
// and covering a virtual address region of 'size' bytes.
// The caller must ensure that both 'va' and 'size' are page-aligned.
// If there is no mapping at that address, pmap_remove silently does nothing.
// Clears nominal permissions (SYS_RW flags) as well as mappings themselves.
//
// Details:
//   - The refcount on mapped pages should be decremented atomically.
//   - The physical page should be freed if the refcount reaches 0.
//   - The page table entry corresponding to 'va' should be set to 0.
//     (if such a PTE exists)
//   - The TLB must be invalidated if you remove an entry from
//     the pdir/ptab.
//   - If the region to remove covers a whole 4MB page table region,
//     then unmap and free the page table after unmapping all its contents.
//
//
void
pmap_remove(pde_t *pdir, uint32_t va, size_t size)
{
	assert(PGOFF(size) == 0);	// must be page-aligned

	uint32_t vahi = va + size;
	while (va < vahi) {
		pde_t *pde = &pdir[PDX(va)];		// find PDE
		if (*pde == PTE_NULL) {	// no page table - skip 4MB region
			va = PTADDR(va + PTSIZE);	// start of next ptab
			continue;
		}

		// Are we removing the entire page table?
		bool wholeptab = (PTX(va) == 0 && vahi-va >= PTSIZE);

		pte_t *pte = pmap_walk(pdir, va, 1);	// find PTE
		assert(pte != PTE_NULL);	// PTE must exist since ptable is present

		// Remove page mappings up to end of region or page table
		do {
			*pte++ = PTE_NULL;
			va += PAGESIZE;
		} while (va < vahi && PTX(va) != 0);

		// Free the page table too if appropriate
		if (wholeptab) {
			mem_decref(mem_phys2pi(PGADDR(*pde)));
			*pde = PTE_NULL;
		}
	}
}

//
// Set the nominal permission bits on a range of virtual pages to 'perm'.
// Adding permission to a nonexistent page maps zero-filled memory.
// It's OK to add SYS_READ and/or SYS_WRITE permission to a PTE_ZERO mapping;
// this causes the pmap_zero page to be mapped read-only (PTE_P but not PTE_W).
// If the user gives SYS_WRITE permission to a PTE_ZERO mapping,
// the page fault handler copies the zero page when the first write occurs.
//
int
pmap_setperm(pmap_t *pdir, uint32_t va, uint32_t size, int perm)
{
	assert(PGOFF(va) == 0);
	assert(PGOFF(size) == 0);

	// Determine the nominal and actual bits to set or clear
	uint32_t pteand, pteor;
		pteand = ~0, pteor = perm | PTE_P;

	uint32_t vahi = va + size;
	while (va < vahi) {
		pde_t *pde = &pdir[PDX(va)];		// find PDE
		if (*pde == PTE_NULL && pteor == 0) {
			// clearing perms, but no page table - skip 4MB region
			va = PTADDR(va + PTSIZE);	// start of next ptab
			continue;
		}

		pte_t *pte = pmap_walk(pdir, va, 1);	// find & unshare PTE
		if (pte == NULL)
			return 0;	// page table alloc failed

		// Adjust page mappings up to end of region or page table
		do {
			*pte = (*pte & pteand) | pteor;
			pte++;
			va += PAGESIZE;
		} while (va < vahi && PTX(va) != 0);
	}
	return 1;
}

//
// This function returns the physical address of the page containing 'va',
// defined by the page directory 'pdir'.  The hardware normally performs
// this functionality for us!  We define our own version to help check
// the pmap_check() function; it shouldn't be used elsewhere.
//
//

uint32_t pmap_lookup(pmap_t *pdir, uintptr_t va)
{
	pdir = &pdir[PDX(va)];
	if (!(*pdir & PTE_P))
		return ~0;
	pte_t *ptab = mem_ptr(PGADDR(*pdir));
	if (!(ptab[PTX(va)] & PTE_P))
		return ~0;
	return (ptab[PTX(va)]);
}

void pmap_install(pmap_t *pmap) {
	lcr3(mem_phys(pmap));
}

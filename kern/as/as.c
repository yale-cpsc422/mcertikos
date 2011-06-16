/* See COPYRIGHT for copyright information. */

#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <inc/gcc.h>
#include <architecture/mem.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <architecture/context.h>
#include <kern/mem/mem.h>

#include <kern/pmap/pmap.h>
#include <kern/as/as.h>

#include <architecture/mp.h>

// This modules implements a general page table system.
// It provides functionality to expose x86 style pagetables
// in a standard way to the rest of the kernel

static as_t* as_active[MAX_CPU];

as_t*
as_init(void)
{
	//assert(!as_active[cpu_number()]);
	pmap_init(); // currently nothing, but is it per CPU?
	as_t* as = as_new();
	if (!as) return NULL;

	uint32_t cr4 = rcr4();
	cr4 |= CR4_PSE | CR4_PGE;
	lcr4(cr4);

	pmap_install(as);

	as_active[mp_curcpu()] = as;

	uint32_t cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);
	return as;
}

as_t* as_current() {
	return as_active[mp_curcpu()];
}

void as_activate(as_t* as) {
	as_active[mp_curcpu()] = as;
	pmap_install(as);
}

//
// Allocate a new page directory, initialized from the bootstrap pdir.
// Returns the new pdir with a reference count of 1.
//
as_t* as_new(void)
{
    pmap_t* pmap = pmap_new();
    if (pmap == NULL) return NULL;
    uint32_t i;
    for (i = 0; i < mem_max; i += PAGESIZE) {
     if (!pmap_insert(pmap, mem_phys2pi(i), i, PTE_W | PTE_G)) {
        pmap_free(pmap);
        return NULL;
     }
    }
	// map the 32-bit memory hole directly
    for (i = 0xf0000000; i != 0; i += PAGESIZE) {
     if (!pmap_insert(pmap, mem_phys2pi(i), i, PTE_W | PTE_G)) {
        pmap_free(pmap);
        return NULL;
     }
    }
    return pmap;
}

//
// Free a page directory, and all page tables and mappings it may contain.
//
void
as_free (as_t *as)
{
		  pmap_free(as);
}

as_t* as_remove(as_t *as, uint32_t va, size_t size) {
  assert(PGOFF(size) == 0); // must be page-aligned
  uint32_t vahi = va + size;
  assert (vahi > va); // whole range must be within valid addresses
  // Recursively find all the pages in question and free them
  // but do not do this for physical pages...
  while(va < vahi) {
     if (va >= mem_max) {
        pageinfo* page = mem_phys2pi(pmap_lookup(as, va));
        mem_decref(page);
     }
     va+=PAGESIZE;
  }
  pmap_remove(as,va,size);
  return as;
}

as_t* as_disconnect(as_t *as, uint32_t va, size_t size) {
  assert(PGOFF(size) == 0); // must be page-aligned
  uint32_t vahi = va + size;
  assert (vahi > va); // whole range must be within valid addresses
  
  pmap_remove(as,va,size);
  return as;
}

as_t* as_reserve(as_t* as, uint32_t uva, int perm) {
		  pageinfo* page = mem_alloc();
		  if (page == NULL) {
					 return NULL;
		  }
		  mem_incref(page);
		  return pmap_insert(as, page, uva, perm);
}

as_t* as_assign(as_t* as, uint32_t uva, int perm, pageinfo* pi) {
	return pmap_insert(as, pi, uva, perm);
}

as_t* as_setperm(as_t *as, uint32_t uva, uint32_t size, int perm) {
		  if (pmap_setperm(as, uva, size, perm) == 0)
					 return NULL;
		  return as;
}

int as_getperm(as_t *as, uint32_t uva) {
    uint32_t result = pmap_lookup(as, uva);
    if (!(result & PTE_P)) return 0;
    return result;

}


// Should check for read and write flags
bool as_checkrange(as_t *as, uint32_t va, size_t size) {
	uint32_t sva = ROUNDDOWN(va,PAGESIZE);
	uint32_t eva = ROUNDDOWN(va + size, PAGESIZE);
	// check for overflow (we also do not tolerate copy to the final address either)
	if (eva < sva) {
		return false;
	}
	while (sva <= eva) {
		if ((pmap_lookup(as,sva) & 1) == 0)
			return false;
		sva+=PAGESIZE;
	}
	return true;
}

bool as_copy(as_t *das, uint32_t dva, as_t *sas, uint32_t sva, size_t size) {
	//cprintf("as_copy: from %08x to %08x\n", sva, dva);
	if (!as_checkrange(das, dva, size)) {
		cprintf("Destination at %08x for %x bytes invalid\n", dva, size);
		return false;
	}
	if (!as_checkrange(sas, sva, size)) {
		cprintf("source at %08x for %x bytes invalid\n", sva, size);
		return false;
	}
	//cprintf("as_copy: continuing\n", dva, size);

	uint32_t eva = sva + size;
	uint32_t spa, dpa;

	// FIXME: do this more than one byte at a time.
	while(sva < eva) {
		spa = PGADDR(pmap_lookup(sas, sva)) + PGOFF(sva);
		dpa = PGADDR(pmap_lookup(das, dva)) + PGOFF(dva);
		*(char*)dpa = *(char*)spa;
		sva++;
		dva++;
	}
	//cprintf("as_copy: success\n");
	return true;
}

void as_memset(as_t* as, uint32_t va, char v, size_t size) {
	//cprintf("as_memset: from %08x\n", va);
	if (!as_checkrange(as, va, size)) return;
	uint32_t eva = va+size;
	uint32_t spa;
	while(va < eva) {
		spa = PGADDR(pmap_lookup(as,va)) + PGOFF(va);
		*(char*)spa = v;
		va++;
	}
	//cprintf("as_memset: success\n");
}

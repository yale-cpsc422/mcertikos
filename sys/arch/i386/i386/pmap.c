#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <machine/pmap.h>

#include <dev/ioapic.h>

typedef pmap_t		pde_t;
typedef pmap_t		pte_t;

#define PTE_INV		(~((pte_t) 0))
#define PTE_NULL	((pte_t) NULL)

static bool
is_in_acpi(uintptr_t addr)
{
	pmmap_t *p;

	p = pmmap_acpi;
	while (p != NULL) {
		uintptr_t start, end;

		start = ROUNDDOWN(p->start, PAGESIZE);
		end = ROUNDUP(p->end, PAGESIZE);

		if (start <= addr && addr < end)
			return TRUE;

		p = p->type_next;
	}

	p = pmmap_nvs;
	while (p != NULL) {
		uintptr_t start, end;

		start = ROUNDDOWN(p->start, PAGESIZE);
		end = ROUNDUP(p->end, PAGESIZE);

		if (start <= addr && addr < end)
			return TRUE;

		p = p->type_next;
	}

	return FALSE;
}

static bool
is_in_ioapic(uintptr_t addr)
{
	int i;

	for (i = 0; i < ioapic_number(); i++) {
		uintptr_t ioapic;

		ioapic = (uintptr_t) ioapic_get(i);
		if (ROUNDDOWN(addr, PAGESIZE) == ROUNDDOWN(ioapic, PAGESIZE))
			return TRUE;
	}

	return FALSE;
}

static pte_t
pmap_lookup(pmap_t *pmap, uintptr_t va)
{
	pde_t *pdir = (pde_t *) pmap;

	pde_t pde = pdir[PDX(va)];

	if (!(pde & PTE_P))
		return PTE_INV;

	pte_t *ptab = (pte_t *) PGADDR(pde);
	pte_t pte = ptab[PTX(va)];

	if (!(pte & PTE_P))
		return PTE_INV;

	return pte;
}

/*
 * Walk through the page directory and corresponding page tables to find the
 * page table entry that addresses the linear address la. When the walking
 * fails due to the absence of a page table, if it's about to reading at the
 * linear address la, then just return NULL, otherwise create the absent page
 * structures.
 *
 * @param pdir  the page directory to walk through
 * @param la    the linear address
 * @param write TRUE if it's about to write at la
 *
 * @return the address of the page table entry that addresses la, or NULL when
 *         failing
 */
static pte_t *
pmap_walk(pde_t *pdir, uintptr_t la, bool write)
{
	KERN_ASSERT(pdir != NULL);

	pde_t *pde = &pdir[PDX(la)];
	pte_t *ptab;

	if (*pde & PTE_P) {	/* page table is present */
		ptab = (pte_t *) PGADDR(*pde);
	} else {		/* page table is not present */
		/* fail if try to read la */
		if (write == FALSE)
			return NULL;

		/* otherwise, create a new page table */
		pageinfo_t *pi = (pageinfo_t *) mem_page_alloc();
		if (pi == NULL)
			return NULL;
		mem_incref(pi);
		ptab = (pte_t *) mem_pi2phys(pi);
		memset(ptab, 0x0, PAGESIZE);
		*pde = ((uintptr_t) ptab) | PTE_P | PTE_A | PTE_W | PTE_U;
	}

	return &ptab[PTX(la)];
}

/*
 * Initialize pmap module.
 */
void
pmap_init()
{
	if (pcpu_onboot() == TRUE) {
		/* do nothing currently */
	}
}

/*
 * Create an empty pmap.
 *
 * @return the address of pmap object, or NULL when failing
 */
pmap_t *
pmap_new(void)
{
	pageinfo_t *pi = (pageinfo_t *) mem_page_alloc();

	if (pi == NULL)
		return NULL;

	mem_incref(pi);
	pmap_t *pmap = (pmap_t *) mem_pi2ptr(pi);
	KERN_ASSERT(pmap != NULL);
	memset(pmap, 0, PAGESIZE);

	return pmap;
}

/*
 * Free a pmap and all page tables it contains.
 *
 * @param pmap
 */
void
pmap_free(pmap_t *pmap)
{
	KERN_ASSERT(pmap != NULL);

	pmap_remove(pmap, 0, ~0);
	mem_page_free(mem_ptr2pi(pmap));
}

/*
 * Map virtual address va to physical memory page pi with permission perm
 * in the pmap structure pmap.
 *
 * @param pmap
 * @param pi
 * @param va
 * @param perm
 *
 * @return the pmap structure
 */
pmap_t *
pmap_insert(pmap_t *pmap, pageinfo_t *pi, uintptr_t va, int perm)
{
	KERN_ASSERT(pmap != NULL);

	pte_t *pte = pmap_walk(pmap, va, TRUE);

	if (pte == NULL)
		return NULL;

	if (*pte & PTE_P) {
		KERN_WARN("Virtual address %x is already mapped.\n", va);
		return NULL;
	}

	*pte = mem_pi2phys(pi) | perm | PTE_P;

	return pmap;
}

/*
 * Unmap virtual address from va to va + size.
 */
void
pmap_remove(pmap_t *pmap, uintptr_t va, size_t size)
{
	KERN_ASSERT(pmap != NULL && PGOFF(size) == 0);

	pde_t *pdir = (pde_t *) pmap;

	uintptr_t vahi = va + size;
	while (va < vahi) {
		pde_t *pde = &pdir[PDX(va)];
		if (*pde == PTE_NULL) {
			/* skip unmapped virtual address */
			va = PTADDR(va + PTSIZE);
			continue;
		}

		/* Are we removing the entire page table? */
		bool wholeptab = (PTX(va) == 0 && vahi-va >= PTSIZE);

		pte_t *pte = pmap_walk(pdir, va, TRUE);
		KERN_ASSERT(pte != NULL);

		/* Remove page mappings up to end of region or page table */
		do {
			*pte++ = PTE_NULL;
			va += PAGESIZE;
		} while (va < vahi && PTX(va) != 0);

		/* Free the page table too if appropriate */
		if (wholeptab) {
			mem_decref(mem_phys2pi(PGADDR(*pde)));
			*pde = PTE_NULL;
		}
	}
}

int
pmap_setperm(pmap_t *pmap, uintptr_t va, uint32_t size, int perm)
{
	KERN_ASSERT(pmap != NULL &&
		    va % PAGESIZE == 0 && size % PAGESIZE == 0);

	pde_t *pdir = (pde_t *) pmap;

	// Determine the nominal and actual bits to set or clear
	uint32_t pteand, pteor;
	pteand = ~0, pteor = perm | PTE_P;

	uintptr_t vahi = va + size;
	while (va < vahi) {
		pde_t *pde = &pdir[PDX(va)];
		if (*pde == PTE_NULL && pteor == 0) {
			/* skip unmapped virtual address */
			va = PTADDR(va + PTSIZE);
			continue;
		}

		pte_t *pte = pmap_walk(pdir, va, TRUE);
		if (pte == NULL)
			return 0;	/* page table alloc failed */

		/* Adjust page mappings up to end of region or page table */
		do {
			*pte = (*pte & pteand) | pteor;
			pte++;
			va += PAGESIZE;
		} while (va < vahi && PTX(va) != 0);
	}

	return 1;
}

void
pmap_enable()
{
	uint32_t cr4 = rcr4();
	cr4 |= CR4_PSE | CR4_PGE;
	lcr4(cr4);
}

void
pmap_install(pmap_t *pmap)
{
	lcr3((uintptr_t) pmap);
}

uintptr_t
pmap_checkrange(pmap_t *pmap, uintptr_t va, size_t size)
{
	uintptr_t sva = ROUNDDOWN(va, PAGESIZE);
	uintptr_t eva = ROUNDDOWN(va + size, PAGESIZE);

	if (eva < sva)
		return sva;

	while (sva <= eva) {
		pte_t pte = pmap_lookup(pmap, sva);

		if (pte == PTE_INV || !(pte & PTE_P))
			return sva;

		sva += PAGESIZE;
	}

	return ~((uintptr_t) 0x0);
}

size_t
pmap_copy(pmap_t *d_pmap, uintptr_t d_va,
	  pmap_t *s_pmap, uintptr_t s_va, size_t size)
{
	if (pmap_checkrange(d_pmap, d_va, size) == FALSE ||
	    pmap_checkrange(s_pmap, s_va, size) == FALSE)
		return 0;

	size_t copyed_bytes = 0;
	uintptr_t d_cur_va = d_va, s_cur_va = s_va;

	while (copyed_bytes < size) {
		uintptr_t d_cur_pa =
			PGADDR(pmap_lookup(d_pmap, d_cur_va)) + PGOFF(d_cur_va);
		uintptr_t s_cur_pa =
			PGADDR(pmap_lookup(s_pmap, s_cur_va)) + PGOFF(s_cur_va);

		*(uint8_t *) d_cur_pa = *(uint8_t *) s_cur_pa;

		d_cur_va++;
		s_cur_va++;

		copyed_bytes++;
	}

	return copyed_bytes;
}

size_t
pmap_memset(pmap_t *pmap, uintptr_t va, char c, size_t size)
{
	if (pmap_checkrange(pmap, va, size) == FALSE)
		return 0;

	size_t bytes = 0;
	uintptr_t cur_va = va;

	while (bytes < size) {
		uintptr_t cur_pa =
			PGADDR(pmap_lookup(pmap, va)) + PGOFF(cur_va);

		*(char *) cur_pa = c;

		cur_va++;
		bytes++;
	}

	return bytes;
}

pmap_t *
pmap_kinit(pmap_t *pmap)
{
	int i;
	uint32_t addr;
	pmmap_t *p;
	ioapic_t *ioapic;

	/* identically map memory below 0xf0000000 */
	for (addr = 0; addr < MIN(mem_max_phys(), VM_USERLO);
	     addr += PAGESIZE) {
		if (is_in_acpi(addr) || is_in_ioapic(addr))
			continue;

		if (!pmap_insert(pmap, mem_phys2pi(addr), addr,
				 PTE_W | PTE_G)) {
			pmap_free(pmap);
			return NULL;
		}
	}

#if 0
	for (; addr < MIN(mem_max_phys(), VM_USERHI); addr += PAGESIZE) {
		if (is_in_acpi(addr) || is_in_ioapic(addr))
			continue;

		if (!pmap_insert(pmap, mem_phys2pi(addr), addr,
				 PTE_W | PTE_G | PTE_U)) {
			pmap_free(pmap);
			return NULL;
		}
	}
#endif

	/* identically map memory hole above 0xf0000000 */
	for (addr = 0xf0000000; addr < 0xffffffff; addr += PAGESIZE) {
		if (is_in_acpi(addr) || is_in_ioapic(addr))
			continue;

		if (!pmap_insert(pmap, mem_phys2pi(addr), addr,
				 PTE_W | PTE_G)) {
			pmap_free(pmap);
			return NULL;
		}
		if (addr == 0xfffff000)
			break;
	}

	/* identically map memory for IOAPIC */
	for (i = 0; i < ioapic_number(); i++) {
		ioapic = ioapic_get(i);

		if (ioapic == NULL) {
			pmap_free(pmap);
			return NULL;
		}

		if (!pmap_insert(pmap, mem_phys2pi((uintptr_t) ioapic),
				 (uintptr_t) ioapic, PTE_W | PTE_G)) {
			pmap_free(pmap);
			return NULL;
		}
	}

	/* identically map memory in ACPI reclaimable region */
	for (p = pmmap_acpi; p != NULL; p = p->type_next) {
		uint32_t start_addr = ROUNDDOWN(p->start, PAGESIZE);
		uint32_t end_addr = ROUNDUP(p->end, PAGESIZE);

		for (addr = start_addr; addr < end_addr; addr += PAGESIZE) {
			if (!pmap_insert(pmap, mem_phys2pi(addr), addr,
					 PTE_W | PTE_G)) {
				pmap_free(pmap);
				return NULL;
			}

			if (addr >= 0xfffff000)
				break;
		}
	}

	/* identically map memory in ACPI NVS region */
	for (p = pmmap_nvs; p != NULL; p = p->type_next) {
		uint32_t start_addr = ROUNDDOWN(p->start, PAGESIZE);
		uint32_t end_addr = ROUNDUP(p->end, PAGESIZE);

		for (addr = start_addr; addr < end_addr; addr += PAGESIZE) {
			if (!pmap_insert(pmap, mem_phys2pi(addr), addr,
					 PTE_W | PTE_G)) {
				pmap_free(pmap);
				return NULL;
			}

			if (addr >= 0xfffff000)
				break;
		}
	}

	return pmap;
}

pmap_t *
pmap_uinit(pmap_t *pmap)
{
	uint32_t addr;

	/* identically map memory below 0x40000000 */
	for (addr = 0; addr < MIN(mem_max_phys(), VM_USERLO);
	     addr += PAGESIZE) {
		if (!pmap_insert(pmap,
				 mem_phys2pi(addr), addr, PTE_W | PTE_G)) {
			pmap_free(pmap);
			return NULL;
		}
	}

#if 0
	for (; addr < MIN(mem_max_phys(), VM_USERHI);
	     addr += PAGESIZE) {
		if (!pmap_insert(pmap, mem_phys2pi(addr), addr,
				 PTE_W | PTE_G | PTE_U)) {
			pmap_free(pmap);
			return NULL;
		}
	}
#endif

	/* identically map memory hole above 0xf0000000 */
	for (addr = 0xf0000000; addr < 0xffffffff; addr += PAGESIZE) {
		if (!pmap_insert(pmap,
				 mem_phys2pi(addr), addr, PTE_W | PTE_G)) {
			pmap_free(pmap);
			return NULL;
		}
		if (addr == 0xfffff000)
			break;
	}

	return pmap;
}

uintptr_t
pmap_la2pa(pmap_t *pmap, uintptr_t la)
{
	pte_t pte = pmap_lookup(pmap, la);

	if (pte == PTE_INV)
		KERN_PANIC("Invalid linear address: la=%x.\n", la);

	return PGADDR(pte)+PGOFF(la);
}

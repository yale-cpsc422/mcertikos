#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/pmap.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <dev/ioapic.h>

typedef pmap_t pde_t;
typedef pmap_t pte_t;

/* the bootstrap page map */
static pmap_t pmap_boot[NPDENTRIES] gcc_aligned(PAGESIZE);

/* the kernel page map */
static pmap_t pmap_kern[NPDENTRIES] gcc_aligned(PAGESIZE);

volatile static bool pmap_boot_inited = FALSE;

#define PTE_INV		(~((pte_t) 0))
#define PTE_NULL	((pte_t) NULL)

/*
 * Look up the virtual address in the page tables.
 *
 * @param pmap the root page table
 * @param va the virtual address
 *
 * @return the page table entry, if the virtual address is in the page tables;
 *         otherwise, NULL.
 */
static pte_t
pmap_lookup(pmap_t *pmap, uintptr_t va)
{
	KERN_ASSERT(pmap != NULL);

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
		if (pi == NULL) {
			KERN_PANIC("Cannot allocate memory for page table "
				   "mapping la 0x%08x.\n", la);
			return NULL;
		}
		mem_incref(pi);
		ptab = (pte_t *) mem_pi2phys(pi);
		memset(ptab, 0x0, PAGESIZE);
		/* KERN_DEBUG("Create page table at 0x%08x for " */
		/* 	   "linear address 0x%08x ~ 0x%08x.\n", */
		/* 	   ptab, la, la + PAGESIZE * NPTENTRIES); */
		*pde = ((uintptr_t) ptab) | PTE_P | PTE_A | PTE_W | PTE_U;
	}

	/* KERN_DEBUG("ptab 0x%08x, index %d, val 0x%08x, size %d.\n", */
	/* 	   ptab, PTX(la), ptab[PTX(la)], sizeof(ptab[PTX(la)])); */

	return &ptab[PTX(la)];
}

/*
 * Initialize the bootstap page map. It identically maps the linear address
 * below VM_USERLO and the linear addres above VM_USERHI.
 */
static pmap_t *
pmap_init_boot(void)
{
	uintptr_t addr;
	size_t memsize = mem_max_phys();

	memset(pmap_boot, 0, PAGESIZE);

	KERN_DEBUG("Bootstrap pmap at 0x%08x.\n", pmap_boot);

	for (addr = 0; addr < MIN(VM_USERLO, memsize); addr += PAGESIZE)
		if (pmap_insert(pmap_boot,
				mem_phys2pi(addr), addr, PTE_W | PTE_G) == NULL)
			return NULL;

	for (addr = VM_USERHI; addr < 0xffffffff; addr += PAGESIZE) {
		if (pmap_insert(pmap_boot,
				mem_phys2pi(addr), addr, PTE_W | PTE_G) == NULL)
			return NULL;

		if (addr == 0xfffff000)
			break;
	}

	return pmap_boot;
}

/*
 * Initialize the kernel page map. It's a copy of the bootstrap page map in
 * addition of the identical mappings for liear address which satisfies the
 * condition "the physical address to which it's identically mappaed is in
 * the reserved memory pages".
 */
static pmap_t *
pmap_init_kern(void)
{
	uintptr_t addr;
	pageinfo_t *pi;
	int i;
	ioapic_t *ioapic_addr;

	memcpy(pmap_kern, pmap_boot, PAGESIZE);

	for (addr = VM_USERLO; addr < mem_max_phys(); addr += PAGESIZE) {
		pi = mem_phys2pi(addr);

		if (pi->type != PG_RESERVED)
			goto next;

		pmap_insert(pmap_kern, pi, addr, PTE_W);

	next:
		if (addr == 0xfffff000)
			break;
	}

	for (i = 0; i < ioapic_number(); i++) {
		if ((ioapic_addr = ioapic_get(i)) == NULL ||
		    (uintptr_t) ioapic_addr >= VM_USERHI)
			continue;
		pi = mem_phys2pi((uintptr_t) ioapic_addr);
		pmap_insert(pmap_kern, pi, (uintptr_t) ioapic_addr, PTE_W);
	}

	return pmap_kern;
}

/*
 * Initialize pmap module.
 */
void
pmap_init(void)
{
	if (pmap_boot_inited == FALSE) {
		/* pmap_new() cannot be useable right now. */
		if (pmap_init_boot() == NULL)
			KERN_PANIC("Cannot initialize the bootstrap "
				   "page map.\n");
		if (pmap_init_kern() == NULL)
			KERN_PANIC("Cannot initialize the kernel page map.");
		pmap_boot_inited = TRUE;
	}


	/* enable global pages (Sec 4.10.2.4, Intel ASDM Vol3) */
	uint32_t cr4 = rcr4();
	cr4 |= CR4_PGE | CR4_OSFXSR | CR4_OSXMMEXCPT;
	lcr4(cr4);

	/* load page table */
	pmap_install(pmap_kern);

	/* turn on paging */
	uint32_t cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_MP;
	cr0 &= ~CR0_EM;
#ifdef __COMPCERT__
	cr0 &= ~CR0_TS;
#endif
	lcr0(cr0);
}

/*
 * Create a page map for a process. It's a copy of the bootstrap page map.
 */
pmap_t *
pmap_new(void)
{
	pageinfo_t *pi;
	pmap_t *pmap;

	if ((pi = mem_page_alloc()) == NULL)
		return NULL;
	mem_incref(pi);

	pmap = mem_pi2ptr(pi);
	memcpy(pmap, pmap_boot, PAGESIZE);

	return pmap;
}

/*
 * Free the memory allocated for a page map of a process. All maps for the
 * linear address below VM_USERLO and the linear address above VM_USERHI
 * are shared for all page maps, it must not be freed.
 */
void
pmap_free(pmap_t *pmap)
{
	KERN_ASSERT(pmap != NULL && pmap != pmap_boot && pmap != pmap_kern);
	pmap_remove(pmap, VM_USERLO, VM_USERHI-1);
	mem_decref(mem_ptr2pi(pmap));
	mem_page_free(mem_ptr2pi(pmap));
}

pmap_t *
pmap_insert(pmap_t *pmap, pageinfo_t *pi, uintptr_t la, int perm)
{
	KERN_ASSERT(pmap != NULL);

	/* KERN_DEBUG("Map linear address 0x%08x to 0x%08x.\n", */
	/* 	   la, mem_pi2phys(pi)); */

	pte_t *pte = pmap_walk(pmap, la, TRUE);

	if (pte == NULL) {
		KERN_DEBUG("Cannot create page structures to "
			   "map linear address 0x%08x.\n", la);
		return NULL;
	}

	if (*pte & PTE_P) {
		KERN_DEBUG("Linear address %x is already mapped to 0x%08x.\n",
			   la, pmap_la2pa(pmap, la));
		return NULL;
	}

	*pte = mem_pi2phys(pi) | perm | PTE_P;
	if (mem_pi2phys(pi) < mem_max_phys() && pi->type == PG_NORMAL)
		mem_incref(pi);

	return pmap;
}

pmap_t *
pmap_reserve(pmap_t *pmap, uintptr_t la, int perm)
{
	KERN_ASSERT(pmap != NULL);

	pageinfo_t *pi = (pageinfo_t *) mem_page_alloc();
	if (pi == NULL)
		return NULL;
	memset(mem_pi2ptr(pi), 0, PAGESIZE);

	return pmap_insert(pmap, pi, la, perm);
}

void
pmap_remove(pmap_t *pmap, uintptr_t la, size_t size)
{
	KERN_ASSERT(pmap != NULL && PGOFF(size) == 0);

	pde_t *pdir = (pde_t *) pmap;

	uintptr_t lahi = la + size;
	while (la < lahi) {
		pde_t *pde = &pdir[PDX(la)];
		if (*pde == PTE_NULL) {
			/* skip unmapped virtual address */
			la = PTADDR(la + PTSIZE);
			continue;
		}

		/* Are we removing the entire page table? */
		bool wholeptab = (PTX(la) == 0 && lahi-la >= PTSIZE);

		pte_t *pte = pmap_walk(pdir, la, TRUE);
		KERN_ASSERT(pte != NULL);

		/* Remove page mappings up to end of region or page table */
		do {
			uintptr_t pgaddr = PGADDR(*pte);
			if (pgaddr != 0 && pgaddr < mem_max_phys() &&
			    mem_phys2pi(pgaddr)->type == PG_NORMAL)
				mem_decref(mem_phys2pi(pgaddr));
			*pte++ = PTE_NULL;
			la += PAGESIZE;
		} while (la < lahi && PTX(la) != 0);

		/* Free the page table too if appropriate */
		if (wholeptab) {
			mem_decref(mem_phys2pi(PGADDR(*pde)));
			*pde = PTE_NULL;
		}
	}
}

/*
 * Set the permission of a linear address region.
 */
int
pmap_setperm(pmap_t *pmap, uintptr_t la, uint32_t size, int perm)
{
	KERN_ASSERT(pmap != NULL &&
		    la % PAGESIZE == 0 && size % PAGESIZE == 0);

	pde_t *pdir = (pde_t *) pmap;

	// Determine the nominal and actual bits to set or clear
	uint32_t pteand, pteor;
	pteand = ~0, pteor = perm | PTE_P;

	uintptr_t lahi = la + size;
	while (la < lahi) {
		pde_t *pde = &pdir[PDX(la)];
		if (*pde == PTE_NULL && pteor == 0) {
			/* skip unmapped virtual address */
			la = PTADDR(la + PTSIZE);
			continue;
		}

		pte_t *pte = pmap_walk(pdir, la, TRUE);
		if (pte == NULL)
			return 0;	/* page table alloc failed */

		/* Adjust page mappings up to end of region or page table */
		do {
			*pte = (*pte & pteand) | pteor;
			pte++;
			la += PAGESIZE;
		} while (la < lahi && PTX(la) != 0);
	}

	return 1;
}

/*
 * Activate a page map.
 */
void
pmap_install(pmap_t *pmap)
{
	/* KERN_DEBUG("pmap_install: 0x%08x\n", pmap); */

	lcr3((uintptr_t) pmap);
}

void
pmap_install_kern(void)
{
	pmap_install(pmap_kern);
}

/*
 * Chech whether virtual address from la to (la+size) are present in the
 * virtual address space pmap.
 *
 * @param pmap
 * @param la
 * @param size
 *
 * @return TRUE, if all address are in the pmap; otherwise, FALSE.
 */
bool
pmap_checkrange(pmap_t *pmap, uintptr_t la, size_t size)
{
	pte_t pte;
	uintptr_t addr = ROUNDDOWN(la, PAGESIZE);
	ssize_t remain_size = size;

	pte = pmap_lookup(pmap, addr);
	if (pte == PTE_INV || !(pte & PTE_P)) {
		KERN_DEBUG("%x is out of range of pmap %x.\n", addr, pmap);
		return FALSE;
	}

	addr += PAGESIZE;
	remain_size -= PAGESIZE - (la - addr);

	while (remain_size > 0) {
		pte = pmap_lookup(pmap, addr);
		if (pte == PTE_INV || !(pte & PTE_P)) {
			KERN_DEBUG("%x is out of range of pmap %x.\n", addr, pmap);
			return FALSE;
		}

		addr += PAGESIZE;
		remain_size -= PAGESIZE;
	}

	return TRUE;
}

size_t
pmap_copy(pmap_t *d_pmap, uintptr_t d_la,
	  pmap_t *s_pmap, uintptr_t s_la, size_t size)
{
	/* KERN_DEBUG("Copy %d bytes from 0x%08x:0x%08x to 0x%08x:0x%08x\n", */
	/* 	   size, s_pmap, s_la, d_pmap, d_la); */

	size_t copied_bytes = 0;
	uintptr_t d_cur_la = d_la, s_cur_la = s_la;
	uintptr_t d_cur_pa, s_cur_pa;

	if (size == 0 ||
	    d_cur_la + size <= d_cur_la || s_cur_la + size <= s_cur_la)
		return 0;

	d_cur_pa = pmap_la2pa(d_pmap, d_cur_la);
	s_cur_pa = pmap_la2pa(s_pmap, s_cur_la);

	do {
		/* KERN_DEBUG("  copy from 0x%08x to 0x%08x\n", */
		/* 	   s_cur_pa, d_cur_pa); */

		*(uint8_t *) d_cur_pa = *(uint8_t *) s_cur_pa;

		d_cur_pa++;
		s_cur_pa++;
		d_cur_la++;
		s_cur_la++;
		copied_bytes++;

		if (copied_bytes == size)
			break;

		if (d_cur_la == ROUNDDOWN(d_cur_la, PAGESIZE))
			d_cur_pa = pmap_la2pa(d_pmap, d_cur_la);
		if (s_cur_la == ROUNDDOWN(s_cur_la, PAGESIZE))
			s_cur_pa = pmap_la2pa(s_pmap, s_cur_la);
	} while (copied_bytes < size);

	return copied_bytes;
}

size_t
pmap_memset(pmap_t *pmap, uintptr_t la, char c, size_t size)
{
	KERN_ASSERT(pmap != NULL);

	size_t set_bytes = 0;
	uintptr_t d_la = la, d_pa;

	d_pa = pmap_la2pa(pmap, d_la);

	do {
		*(char *) d_pa = c;

		d_la++;
		d_pa++;
		set_bytes++;

		if (set_bytes == size)
			break;

		if (d_la == ROUNDDOWN(d_la, PAGESIZE))
			d_pa = pmap_la2pa(pmap, d_la);
	} while (set_bytes < size);

	return set_bytes;
}

uintptr_t
pmap_la2pa(pmap_t *pmap, uintptr_t la)
{
	pageinfo_t *pi;
	pte_t pte = pmap_lookup(pmap, la);
	int perm;

	if (pte == PTE_INV) {
		if ((pi = mem_page_alloc()) == NULL)
			KERN_PANIC("Cannot allocate physical memory.\n");

		perm = PTE_W |
			((VM_USERLO <= la && la < VM_USERHI) ? PTE_U : 0);

		if (pmap_insert(pmap, pi, ROUNDDOWN(la,PAGESIZE), perm) == NULL)
			KERN_PANIC("Cannot map linear address 0x%08x to "
				   "physical address 0x%08x.\n",
				   ROUNDDOWN(la,PAGESIZE), mem_pi2phys(pi));

		pmap_install(pmap);

		return ROUNDDOWN(mem_pi2phys(pi), PAGESIZE) + PGOFF(la);
	} else {
		return PGADDR(pte) + PGOFF(la);
	}
}

pmap_t *
pmap_kern_map(void)
{
	return pmap_kern;
}

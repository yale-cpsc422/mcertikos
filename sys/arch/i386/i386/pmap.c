#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <machine/pmap.h>

#include <dev/ioapic.h>

typedef pmap_t pde_t;
typedef pmap_t pte_t;

//static pde_t pmap_bootpdir[NPDENTRIES] gcc_aligned(PAGESIZE);
static pmap_t pmap_bootpdir[NPDENTRIES] gcc_aligned(PAGESIZE);

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
 * Create the kernel page table. It identically maps the address from 0x0 to
 * max(maximum physical memory address, 0x4000_0000). In addition, it also
 * identically maps the ACPI address space, IOAPIC address space, and
 * 0xf000_0000 to 0xffff_ffff.
 *
 * XXX: all these pages are marked as global pages (PTE_G=1) so that "move to
 *      CR3" can not invalidate the TLB entries for them.
 *
 * @param pmap the kernel page table
 *
 * @return the kernel page table, if succeed; otherwise, NULL.
 */
static pmap_t *
pmap_init_bootpdir(pmap_t *pmap)
{
	KERN_ASSERT(pmap != NULL);

	pmmap_t *e820_ent, *e820_last_ent;
	uint32_t addr;
	pmap_t *rc;

	for (e820_ent = pmmap, e820_last_ent = NULL;
	     e820_ent != NULL;
	     e820_last_ent = e820_ent, e820_ent = e820_ent->next) {
		uint32_t start, end, type, addr;
		pmap_t *rc;

		start = e820_ent->start;
		end = e820_ent->end - 1;
		type = e820_ent->type;

		if (ROUNDDOWN(start, PAGESIZE) >= 0xf0000000)
			break;

		if (ROUNDDOWN(end, PAGESIZE) >= 0xf0000000)
			end = 0xefffffff;

		KERN_DEBUG("0x%08x ~ 0x%08x ==> ", start, end);

		if (e820_last_ent == NULL) {
			start = ROUNDDOWN(start, PAGESIZE);

			if (type == MEM_RAM)
				end = ROUNDDOWN(end, PAGESIZE) - 1;
			else
				end = ROUNDDOWN(end, PAGESIZE);
		} else {
			if (type == MEM_RAM) {
				start = ROUNDDOWN(start, PAGESIZE);
				if (e820_last_ent->type != MEM_RAM &&
				    e820_last_ent->end-1 > start)
					start += PAGESIZE;

				end = ROUNDDOWN(end, PAGESIZE) - 1;
			} else {
				start = ROUNDDOWN(start, PAGESIZE);
				if (e820_last_ent->type != MEM_RAM &&
				    e820_last_ent->end-1 > start)
					start += PAGESIZE;

				end = ROUNDDOWN(end, PAGESIZE);
			}
		}

		dprintf("0x%08x ~ 0x%08x", start, end);

		for (addr = start;
		     addr < ((type == MEM_RAM) ? MIN(end, VM_USERLO) : end);
		     addr += PAGESIZE) {
			/* KERN_DEBUG("map 0x%08x\n", addr); */

			rc = pmap_insert(pmap, mem_phys2pi(addr), addr,
					 PTE_W | PTE_G);

			if (rc == NULL) {
				pmap_free(pmap);
				return NULL;
			}
		}

		dprintf(" done.\n");
	}

	/* map the lowest 1MB address */
	KERN_DEBUG("0x00000000 ~ 0x00100000 ==> 0x00000000 ~ 0x00100000 ");
	for (addr = 0x00000000; addr < 0x00100000; addr += PAGESIZE)
		pmap_insert(pmap, mem_phys2pi(addr), addr, PTE_W | PTE_G);
	dprintf("done.\n");

	/* map the memory hole above 0xf000_0000 */
	KERN_DEBUG("0xf0000000 ~ 0xffffffff ==> 0xf0000000 ~ 0xffffffff ");
	for (addr = 0xf0000000; addr < 0xffffffff; addr += PAGESIZE) {
		rc = pmap_insert(pmap, mem_phys2pi(addr), addr,
				 PTE_W | PTE_G);

		if (rc == NULL) {
			pmap_free(pmap);
			return NULL;
		}

		if (addr == 0xfffff000)
			break;
	}
	dprintf("done.\n");

	/* map address used by IOAPIC */
	int ioapic_no;
	ioapic_t *ioapic;
	for (ioapic_no = 0; ioapic_no < ioapic_number(); ioapic_no++) {
		ioapic = ioapic_get(ioapic_no);

		if (ioapic == NULL) {
			KERN_WARN("NULL IOAPIC %x.\n", ioapic_no);
			continue;
		}

		KERN_DEBUG("0x%08x ~ 0x%08x ==> 0x%08x ~ 0x%08x ",
			   (uintptr_t) ioapic, (uintptr_t) ioapic + PAGESIZE,
			   (uintptr_t) ioapic, (uintptr_t) ioapic + PAGESIZE);

		pmap_insert(pmap, mem_phys2pi((uintptr_t) ioapic),
			    (uintptr_t) ioapic, PTE_W | PTE_G);

		dprintf("done.\n");
	}

	KERN_DEBUG("bootstrap page structures initialized.\n");

	return pmap;
}

/*
 * Initialize pmap module.
 */
void
pmap_init(void)
{
	if (pcpu_onboot() == TRUE) {
		/* pmap_new() cannot be useable right now. */
		memset(pmap_bootpdir, 0, PAGESIZE);
		if (pmap_init_bootpdir(pmap_bootpdir) == NULL)
			KERN_PANIC("Failed to initialize bootstrap page structures.\n");
	}

	/* enable global pages (Sec 4.10.2.4, Intel ASDM Vol3) */
	uint32_t cr4 = rcr4();
	cr4 |= CR4_PGE;
	lcr4(cr4);

	/* load page table */
	pmap_install(pmap_bootpdir);

	/* turn on paging */
	uint32_t cr0 = CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_MP;
	cr0 &= ~CR0_EM;
	lcr0(cr0);
}

/*
 * Create a pmap which is a clone of the kernel pmap.
 *
 * @return the address of pmap object, or NULL when failing
 */
pmap_t *
pmap_new(void)
{
	pmap_t *pmap = pmap_new_empty();

	if (pmap == NULL)
		return NULL;

	/* initialize it from the bootstrap page structure */
	memcpy(pmap, pmap_bootpdir, PAGESIZE);

	return pmap;
}

/*
 * Create an empty pmap.
 */
pmap_t *
pmap_new_empty(void)
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
		/* KERN_DEBUG("Virtual address %x is already mapped.\n", va); */
		return NULL;
	}

	*pte = mem_pi2phys(pi) | perm | PTE_P;

	return pmap;
}

/*
 * Reserve a page of physical memory for a virtual page.
 *
 * @return pmap if succeed; otherwise, NULL.
 */
pmap_t *
pmap_reserve(pmap_t *pmap, uintptr_t va, int perm)
{
	KERN_ASSERT(pmap != NULL);

	pageinfo_t *pi = (pageinfo_t *) mem_page_alloc();
	if (pi == NULL)
		return NULL;

	mem_incref(pi);
	memset(mem_pi2ptr(pi), 0, PAGESIZE);

	return pmap_insert(pmap, pi, va, perm);
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
pmap_install(pmap_t *pmap)
{
	/* KERN_DEBUG("pmap_install: 0x%08x\n", pmap); */

	lcr3((uintptr_t) pmap);
}

/*
 * Chech whether virtual address from va to (va+size) are present in the
 * virtual address space pmap.
 *
 * @param pmap
 * @param va
 * @param size
 *
 * @return TRUE, if all address are in the pmap; otherwise, FALSE.
 */
bool
pmap_checkrange(pmap_t *pmap, uintptr_t va, size_t size)
{
	pte_t pte;
	uintptr_t addr = ROUNDDOWN(va, PAGESIZE);
	ssize_t remain_size = size;

	pte = pmap_lookup(pmap, addr);
	if (pte == PTE_INV || !(pte & PTE_P)) {
		KERN_DEBUG("%x is out of range of pmap %x.\n", addr, pmap);
		return FALSE;
	}

	addr += PAGESIZE;
	remain_size -= PAGESIZE - (va - addr);

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

/*
 * Copy data between two virtual address spaces.
 *
 * @param d_pmap the destination pmap
 * @param d_va the destination virtual address
 * @param s_pmap the source pmap
 * @param s_va the source vritual address
 * @size numbers of bytes to copy
 *
 * @return the actual number of bytes copied
 */
size_t
pmap_copy(pmap_t *d_pmap, uintptr_t d_va,
	  pmap_t *s_pmap, uintptr_t s_va, size_t size)
{
	/* KERN_DEBUG("Copy %d bytes from 0x%08x:0x%08x to 0x%08x:0x%08x\n", */
	/* 	   size, s_pmap, s_va, d_pmap, d_va); */

	size_t copied_bytes = 0;
	uintptr_t d_cur_va = d_va, s_cur_va = s_va;
	uintptr_t d_cur_pa, s_cur_pa;

	if (size == 0 ||
	    d_cur_va + size <= d_cur_va || s_cur_va + size <= s_cur_va)
		return 0;

	d_cur_pa = pmap_la2pa(d_pmap, d_cur_va);
	s_cur_pa = pmap_la2pa(s_pmap, s_cur_va);

	do {
		/* KERN_DEBUG("  copy from 0x%08x to 0x%08x\n", */
		/* 	   s_cur_pa, d_cur_pa); */

		*(uint8_t *) d_cur_pa = *(uint8_t *) s_cur_pa;

		d_cur_pa++;
		s_cur_pa++;
		d_cur_va++;
		s_cur_va++;
		copied_bytes++;

		if (copied_bytes == size)
			break;

		if (d_cur_va == ROUNDDOWN(d_cur_va, PAGESIZE))
			d_cur_pa = pmap_la2pa(d_pmap, d_cur_va);
		if (s_cur_va == ROUNDDOWN(s_cur_va, PAGESIZE))
			s_cur_pa = pmap_la2pa(s_pmap, s_cur_va);
	} while (copied_bytes < size);

	return copied_bytes;
}

/*
 * Set the value of address in the virtual address space.
 *
 * @param pmap
 * @param va the virtual address
 * @param c all bytes from va to (va+size) will be set to this character
 * @param size the number of bytes to be set
 *
 * @return the actual number of bytes set
 */
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

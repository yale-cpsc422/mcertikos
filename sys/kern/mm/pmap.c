#include <lib/export.h>
#include <dev/export.h>

#include "at.h"
#include "pmap.h"

typedef uint32_t	pde_t;
typedef uint32_t	pte_t;

#define PAGESIZE	4096		/* bytes mapped by a page */

#define NPDENTRIES	1024	/* PDEs per page directory */
#define NPTENTRIES	1024	/* PTEs per page table */

#define PDX(la)		((((uintptr_t) (la)) >> PDXSHIFT) & 0x3FF)
#define PTX(la)		((((uintptr_t) (la)) >> PTXSHIFT) & 0x3FF)

#define PGADDR(la)	((uintptr_t) (la) & ~0xFFF)	/* address of page */
#define PGOFF(la)	((uintptr_t) (la) & 0xFFF)	/* offset in page */

#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

/* the bootstrap page map */
static pmap_t pmap_boot[NPDENTRIES] gcc_aligned(PAGESIZE);
static pte_t pmap_boot_lv2[NPDENTRIES][NPTENTRIES] gcc_aligned(PAGESIZE);

/* the kernel page map */
static pmap_t pmap_kern[NPDENTRIES] gcc_aligned(PAGESIZE);
static pte_t pmap_kern_lv2[NPDENTRIES][NPTENTRIES] gcc_aligned(PAGESIZE);

/*
 * Initialize the bootstap page map. It identically maps the linear address
 * below VM_USERLO and the linear addres above VM_USERHI.
 */
static void
pmap_init_boot(void)
{
	int pdx;
	uintptr_t addr;
	size_t memsize = at_get_nps() * PAGESIZE;

	memzero(pmap_boot, sizeof(pmap_t) * NPDENTRIES);
	memzero(pmap_boot_lv2, sizeof(pte_t) * NPDENTRIES * NPTENTRIES);

	for (pdx = 0; pdx < PDX(VM_USERLO); pdx++) {
		pmap_boot[pdx] = (uintptr_t) pmap_boot_lv2[pdx] | PTE_G | PTE_W;
	for (pdx = PDX(VM_USERHI); pdx <= 0xfffff000; pdx++) {
		pmap_boot[pdx] = (uintptr_t) pmap_boot_lv2[pdx] | PTE_G | PTE_W;
		/* explicit check to avoid the overflow */
		if (pdx == 0xfffff000)
			break;
	}



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

static void
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

void
pmap_init(void)
{
	pmap_init_boot();
	pmap_init_kern();

	uint32_t cr4 = rcr4();
	cr4 |= CR4_PGE | CR4_OSFXSR | CR4_OSXMMEXCPT;
	lcr4(cr4);

	/* load page table */
	pmap_install(pmap_kern);

	/* turn on paging */
	uint32_t cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_MP;
	cr0 &= ~(CR0_EM | CR0_TS);
	lcr0(cr0);
}

#include "MPTOp.h"

#define PAGESIZE	4096
#define NPDENTRIES	1024	/* PDEs per page directory */
#define NPTENTRIES	1024	/* PTEs per page table */

#define VM_USERLO	0x40000000
#define VM_USERHI	0xF0000000
#define VM_USERLO_PI	(VM_USERLO / PAGESIZE)
#define VM_USERHI_PI	(VM_USERHI / PAGESIZE)

#define MAX_PAGE	0x100000u

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */
#define PTE_G		0x100	/* Global */
#define PTE_KERN	(PTE_P | PTE_W | PTE_G)

void
pt_init_comm(unsigned int mbi_addr)
{
	unsigned int i;
	unsigned int j;
	unsigned int last_vaddr;

	mem_init(mbi_addr);

	i = 0;
	while (i < 64) {
		j = 0;
		while (j < NPDENTRIES) {
			set_PDX(i, j);
			j++;
		}

		j = 0;
		last_vaddr = (MAX_PAGE - 1) * PAGESIZE;

		while (j < last_vaddr) {
			if (j < VM_USERLO || j >= VM_USERHI)
				pt_insert(i, j, j, PTE_KERN);
			else
				pt_rmv(i, j);

			j = j + PAGESIZE;
		}
		pt_insert(i, last_vaddr, last_vaddr, PTE_KERN);

		i++;
	}
}

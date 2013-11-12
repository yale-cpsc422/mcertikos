#include "MPTComm.h"

#define PAGESIZE	4096
#define VM_USERLO	0x40000000
#define VM_USERHI	0xF0000000
#define VM_USERLO_PI	(VM_USERLO / PAGESIZE)
#define VM_USERHI_PI	(VM_USERHI / PAGESIZE)

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_USER	(PTE_P | PTE_W)

void
pt_init_kern(unsigned int mbi_addr)
{
	unsigned int i;
	pt_init_comm(mbi_addr);
	i = VM_USERLO;
	while (i < VM_USERHI) {
		pt_insert(0, i, i, PTE_USER);
		i = i + PAGESIZE;
	}
}

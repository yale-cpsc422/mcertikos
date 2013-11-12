#include <lib/gcc.h>

#include "MALOp.h"

#define PAGESIZE	4096
#define NPDENTRIES	1024	/* PDEs per page directory */
#define NPTENTRIES	1024	/* PTEs per page table */
#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */
#define PDXPERM		(PTE_P | PTE_W | PTE_U)

struct PTStruct {
	unsigned int pdir[NPDENTRIES]		gcc_aligned(PAGESIZE);
	unsigned int pt[NPDENTRIES][NPTENTRIES]	gcc_aligned(PAGESIZE);
};

static struct PTStruct PTPool_LOC[64] gcc_aligned(PAGESIZE);


void
pt_in(void)
{
}

void
pt_out(void)
{
}

void
set_PT(unsigned int index)
{
	set_pt(PTPool_LOC[index].pdir);
}

void
set_PDX(unsigned int pid, unsigned int pdx)
{
	PTPool_LOC[pid].pdir[pdx] = (char *) PTPool_LOC[pid].pt[pdx] + PDXPERM;
}

void
set_PTX(unsigned int pid, unsigned int pdx, unsigned int ptx,
	unsigned int paddr, unsigned int perm)
{
	PTPool_LOC[pid].pt[pdx][ptx] = paddr + perm;
}

void
set_PTX_P(unsigned int pid, unsigned int pdx, unsigned int ptx)
{
	PTPool_LOC[pid].pt[pdx][ptx] = 0;
}

unsigned int
get_PTX(unsigned int pid, unsigned int pdx, unsigned int ptx)
{
	unsigned int paddr;
	paddr = PTPool_LOC[pid].pt[pdx][ptx];
	return paddr;
}

void
rmv_PTX(unsigned int pid, unsigned int pdx, unsigned int ptx)
{
	PTPool_LOC[pid].pt[pdx][ptx] = 0;
}

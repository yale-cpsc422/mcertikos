#include <lib/debug.h>
#include <lib/gcc.h>

#include "MAL.h"

struct PTStruct {
	unsigned int pdir[1024] gcc_aligned(4096);
	unsigned int pt[1024][1024] gcc_aligned(4096);
};

static struct PTStruct PTPool_LOC[64] gcc_aligned(4096);

/*
 * TODO: what is PDXPERM ?
 */

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

#define PDXPERM		(PTE_P | PTE_W | PTE_U)

void
pt_in(void)
{
}

void
pt_out(void)
{
}

void
set_PT(int index)
{
	set_pt(PTPool_LOC[index].pdir);
}

void
set_PDX(int proc_index, int pdx_index)
{
	PTPool_LOC[proc_index].pdir[pdx_index] =
		(unsigned int) PTPool_LOC[proc_index].pt[pdx_index] + PDXPERM;
}

void
set_PTX(int proc_index, int pdx_index, int ptx, unsigned int paddr, int perm)
{
	PTPool_LOC[proc_index].pt[pdx_index][ptx] = paddr + perm;
}

void
set_PTX_P(int proc_index, int pdx_index, int ptx)
{
	PTPool_LOC[proc_index].pt[pdx_index][ptx] = 0;
}

unsigned int
get_PTX(int proc_index, int pdx_index, int ptx)
{
	unsigned int paddr;
	paddr = PTPool_LOC[proc_index].pt[pdx_index][ptx];
	return paddr;
}

void
rmv_PTX(int proc_index, int pdx_index, int ptx)
{
	PTPool_LOC[proc_index].pt[pdx_index][ptx] = 0;
}

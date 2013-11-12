#include "MPTIntro.h"

#define PAGESIZE	4096
#define NPDENTRIES	1024	/* PDEs per page directory */
#define NPTENTRIES	1024	/* PTEs per page table */

void
pt_insert(unsigned int pid,
	  unsigned int vaddr, unsigned int paddr, unsigned int perm)
{
	unsigned int pdx;
	unsigned int ptx;
	pdx = vaddr / (PAGESIZE * NPTENTRIES);
	ptx = (vaddr / PAGESIZE) % NPTENTRIES;
	set_PDX(pid, pdx);
	set_PTX(pid, pdx, ptx, paddr, perm);
}

void
pt_rmv(unsigned int pid, unsigned int vaddr)
{
	unsigned int pdx;
	unsigned int ptx;
	pdx = vaddr / (PAGESIZE * NPTENTRIES);
	ptx = (vaddr / PAGESIZE) % NPTENTRIES;
	rmv_PTX(pid, pdx, ptx);
}

unsigned int
pt_read(unsigned int pid, unsigned int vaddr)
{
	unsigned int pdx;
	unsigned int ptx;
	unsigned int paddr;
	pdx = vaddr / (PAGESIZE * NPTENTRIES);
	ptx = (vaddr / PAGESIZE) % NPTENTRIES;
	paddr = get_PTX(pid, pdx, ptx);
	return paddr;
}

#include "MPTIntro.h"

void
pt_insert(int proc_index, unsigned int vaddr, unsigned int paddr, int perm)
{
	int pdx_index;
	int vaddrl;
	pdx_index = vaddr / (4096 * 1024);
	vaddrl = (vaddr / 4096) % 1024;
	set_PDX(proc_index, pdx_index);
	set_PTX(proc_index, pdx_index, vaddrl, paddr, perm);
}

void
pt_rmv(int proc_index, unsigned int vaddr)
{
	int pdx_index;
	int vaddrl;
	pdx_index = vaddr / (4096 * 1024);
	vaddrl = (vaddr / 4096) % 1024;
	rmv_PTX(proc_index, pdx_index, vaddrl);
}

unsigned int
pt_read(int proc_index, unsigned int vaddr)
{
	int pdx_index;
	int vaddrl;
	unsigned int paddr;
	pdx_index = vaddr / (4096 * 1024);
	vaddrl = (vaddr / 4096) % 1024;
	paddr = get_PTX(proc_index, pdx_index, vaddrl);
	return paddr;
}

void
pt_unpresent(int proc_index, unsigned int vaddr)
{
	int pdx_index;
	int vaddrl;
	pdx_index = vaddr / (4096 * 1024);
	vaddrl = (vaddr / 4096) % 1024;
	set_PTX_P(proc_index, pdx_index, vaddrl);
}

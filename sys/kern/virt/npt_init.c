#include "npt_intro.h"

#define PAGESIZE	4096

void
npt_init(unsigned int mbi_addr)
{
	unsigned int i;
	sched_init(mbi_addr);
	i = 0;
	while (i < 1024) {
		set_NPDE(i);
		i++;
	}
}

void
npt_insert(unsigned int gpa, unsigned hpa)
{
	unsigned int pdx;
	unsigned int ptx;
	pdx = gpa / (PAGESIZE * 1024);
	ptx = (gpa / PAGESIZE) % 1024;
	set_NPTE(pdx, ptx, hpa);
}

#include "npt_intro.h"

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */
#define PTE_G		0x100	/* Global */
#define NPDEPERM	(PTE_P | PTE_W | PTE_U | PTE_G)
#define NPTEPERM	(PTE_P | PTE_W | PTE_U)

struct NPTStruct NPT_LOC gcc_aligned(PAGESIZE);

void
set_NPDE(unsigned int pdx)
{
	NPT_LOC.pdir[pdx] = ((char *) NPT_LOC.pt[pdx]) + NPDEPERM;
}

void
set_NPTE(unsigned int pdx, unsigned int ptx, unsigned int paddr)
{
	NPT_LOC.pt[pdx][ptx] = paddr + NPTEPERM;
}

#ifndef _KERN_VIRT_NPT_INTRO_H_
#define _KERN_VIRT_NPT_INTRO_H_

#ifdef _KERN_

#include <lib/export.h>

extern uint32_t npt_lv1[1024] gcc_aligned(4096);
extern uint32_t npt_lv2[1024][1024] gcc_aligned(4096);

#define PTXSHIFT	12
#define PDXSHIFT	22

#define PDX(la)		((((uintptr_t) (la)) >> PDXSHIFT) & 0x3FF)
#define PTX(la)		((((uintptr_t) (la)) >> PTXSHIFT) & 0x3FF)

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */
#define PTE_G		0x100

void npt_set_pde(int pdx);
void npt_set_pte(int pdx, int ptx, uint32_t hpa);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_NPT_INTRO_H_ */

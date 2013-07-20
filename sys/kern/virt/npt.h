#ifndef _VIRT_SVM_NPT_H_
#define _VIRT_SVM_NPT_H_

#ifdef _KERN_

#include <lib/export.h>

#include "vmcb.h"

typedef uint32_t *npt_t;

/*
 * Create a new and empty NPT,
 *
 * @return a pointer to the NPT if successful; otherwise, return NULL.
 */
npt_t npt_new(void);

/*
 * Free a NPT.
 */
void npt_free(npt_t npt);

/*
 * Map a guest physical address to a host physical address in the given NPT.
 *
 * @param npt the pointer to NPT
 * @param gpa the guest phyiscal address; it must be aligned to 4 KB
 * @param hpa the host physical address; it must be aligned to 4 KB
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int npt_insert(npt_t npt, uintptr_t gpa, uintptr_t hpa);

/*
 * Set ncr3 of a VMCB.
 *
 * @param vmcb the VMCB
 * @param npt  the root of the nested page table
 */
void npt_install(struct vmcb *vmcb, npt_t npt);

#endif /* _KERN_ */

#endif /* _VIRT_SVM_NPT_H_ */

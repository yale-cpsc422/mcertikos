#ifndef _KERN_VIRT_NPT_OPS_H_
#define _KERN_VIRT_NPT_OPS_H_

#ifdef _KERN_

#include <lib/types.h>

void npt_set_pde(int pdx);
void npt_insert(uint32_t gpa, uint32_t hpa);

#endif /* _KERN_ */

#endif /* !_KERN_VIRT_NPT_OPS_H_ */

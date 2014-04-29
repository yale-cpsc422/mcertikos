#ifndef _SYS_PREINIT_DEV_VMX_DRV_H_
#define _SYS_PREINIT_DEV_VMX_DRV_H_

#ifdef _KERN_

#include <preinit/lib/types.h>

#define MSR_VMX_CR0_FIXED0      0x486
#define MSR_VMX_CR0_FIXED1      0x487

#define MSR_VMX_CR4_FIXED0      0x488
#define MSR_VMX_CR4_FIXED1      0x489

int vmx_hw_init(void);

#endif /* _KERN_ */

#endif /* !_SYS_PREINIT_DEV_VMX_DRV_H_ */

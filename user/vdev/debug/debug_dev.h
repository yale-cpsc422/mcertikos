#ifndef _SYS_VIRT_DEV_DEBUG_DEV_H_
#define _SYS_VIRT_DEV_DEBUG_DEV_H_

#ifdef _KERN_

#include <sys/virt/vmm.h>

#define GUEST_DEBUG_IOPORT	0x402

struct guest_debug_dev {
};

void guest_debug_dev_init(struct guest_debug_dev *, struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_DEBUG_DEV_H_ */

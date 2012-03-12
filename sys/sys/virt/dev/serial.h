#ifndef _SYS_VIRT_DEV_SERIAL_H_
#define _SYS_VIRT_DEV_SERIAL_H_

#ifdef _KERN_

#include <sys/virt/vmm.h>

struct vserial {};

void vserial_init(struct vserial *, struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_SERIAL_H_ */

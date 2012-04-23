#ifndef _SYS_VIRT_IDE_H_
#define _SYS_VIRT_IDE_H_

#ifdef _KERN_

#include <sys/virt/vmm.h>

struct vide {
	uint32_t data_port;
	uint32_t stat_port;
};

void vide_init(struct vide *, struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_IDE_H_ */

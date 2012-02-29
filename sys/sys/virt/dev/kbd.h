#ifndef _SYS_VIRT_DEV_KBD_H_
#define _SYS_VIRT_DEV_KBD_H_

#ifdef _KERN_

#include <sys/types.h>

struct vkbd {
};

void vkbd_init(struct vkbd *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_KBD_H_ */

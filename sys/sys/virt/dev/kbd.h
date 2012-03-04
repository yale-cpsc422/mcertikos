#ifndef _SYS_VIRT_DEV_KBD_H_
#define _SYS_VIRT_DEV_KBD_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/dev/ps2.h>

struct vkbd {
	struct vm	*vm;

	uint8_t		write_cmd;

	uint8_t		status;		/* buffer of command/status port */
	uint8_t		outport;	/* buffer of data port */

	uint8_t		mode;

	uint8_t		pending;	/* KBD/AUX event pending? */

	struct PS2_kbd		kbd;
	struct PS2_mouse	mouse;
};

void vkbd_init(struct vkbd *, struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_KBD_H_ */

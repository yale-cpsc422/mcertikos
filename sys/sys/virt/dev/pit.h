#ifndef _SYS_VIRT_DEV_PIT_H_
#define _SYS_VIRT_DEV_PIT_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>

/* virtualized i8254 */

/* structures of i8254 counter channles */
struct vpit_channel {
	int16_t		count;
	uint16_t	latched_count;
	uint8_t		count_latched;
	uint8_t		status_latched;
	uint8_t		status;
	uint8_t		read_state;
	uint8_t		write_state;
	uint8_t		write_latch;
	uint8_t		rw_mode;
	uint8_t		mode;
	uint8_t		bcd; /* XXX: not implemented yet  */
	uint8_t		gate;
	int64_t		count_load_time;
};

/* structure of the virtualized i8254 */
struct vpit {
	vpit_channel	channels[3];
};

void vpit_init(struct vpit *, struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_PIT_H_ */

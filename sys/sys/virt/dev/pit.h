#ifndef _SYS_VIRT_DEV_PIT_H_
#define _SYS_VIRT_DEV_PIT_H_

#ifdef _KERN_

#include <sys/spinlock.h>
#ifdef USE_KERN_TIMER
#include <sys/timer.h>
#endif
#include <sys/types.h>

#include <sys/virt/vmm.h>

/* virtualized i8254 */

struct vpit;

/* structures of i8254 counter channles */
struct vpit_channel {
	spinlock_t	lk;
	struct vpit	*pit;

	int32_t		count; /* XXX: valid value range 0x0 ~ 0x10000 */
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
	uint64_t	count_load_time;

	uint64_t	last_intr_time;
	bool		last_intr_time_valid;

	bool		enabled;
};

/* structure of the virtualized i8254 */
struct vpit {
	struct vm		*vm;
	struct vpit_channel	channels[3];
};

void vpit_init(struct vpit *, struct vm *);
void vpit_update(struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_PIT_H_ */

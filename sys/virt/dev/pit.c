#include <sys/debug.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pit.h>

#define PIT_CHANNEL_MODE_0	0	/* interrupt on terminal count */
#define PIT_CHANNEL_MODE_1	1	/* hardware retriggerable one-shot */
#define PIT_CHANNEL_MODE_2	2	/* rate generator */
#define PIT_CHANNEL_MODE_3	3	/* square wave mode */
#define PIT_CHANNEL_MODE_4	4	/* software triggered strobe */
#define PIT_CHANNEL_MODE_5	5	/* hardware triggered strobe */

#define TICKS_PER_SEC		1000000000LL

#define PIT_RW_STATE_LSB	1
#define PIT_RW_STATE_MSB	2
#define PIT_RW_STATE_WORD0	3
#define PIT_RW_STATE_WORD1	4

#define PIT_CHANNEL0_PORT	0x40
#define PIT_CHANNEL1_PORT	0x41
#define PIT_CHANNEL2_PORT	0x42
#define PIT_CONTROL_PORT	0x43
#define PIT_GATE_PORT		0x61

static void vpit_irq_timer_update(struct vpit_channel *, uint64_t current_tsc);

static gcc_inline uint64_t
muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
	union {
		uint64_t ll;
		struct {
			uint32_t low, high;
		} l;
	} u, res;
	uint64_t rl, rh;

	u.ll = a;
	rl = (uint64_t)u.l.low * (uint64_t)b;
	rh = (uint64_t)u.l.high * (uint64_t)b;
	rh += (rl >> 32);
	res.l.high = rh / c;
	res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
	return res.ll;
}

/*
 * Get the remaining numbers to count before OUT changes.
 */
static int32_t
vpit_get_count(struct vpit_channel *ch)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	uint64_t d, count;
	int32_t counter = 0;
	uint16_t d0, d1;

	d = muldiv64(vmm_rdtsc(ch->pit->vm) - ch->count_load_time,
		     VM_PIT_FREQ, VM_TSC_FREQ);
	d /= VM_TIME_SCALE;
	count = (uint64_t)(uint32_t) ch->count;

	switch(ch->mode) {
	case PIT_CHANNEL_MODE_0:
	case PIT_CHANNEL_MODE_1:
	case PIT_CHANNEL_MODE_4:
	case PIT_CHANNEL_MODE_5:
		/* mode 0, 1, 4 and 5 are not repeated */
		if (count > d)
			counter = (count - d) & 0xffff;
		else
			counter = 0;
		break;
	case PIT_CHANNEL_MODE_3:
		/*
		 * mode 3: high for N/2, low for N/2, when N is even;
		 *         high for (N+1)/2, low for (N-1)/2, when N is odd
		 */
		if (count % 2 == 0)
			counter = count - ((2 * d) % count);
		else {
			d0 = d % count;
			d1 = (count + 1) / 2;
			if (d0 < d1)
				counter = d1 - d0;
			else
				counter = d0 - d1;
		}
		break;
	case PIT_CHANNEL_MODE_2:
		counter = count - (d % count);
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	KERN_ASSERT(counter >= 0 && counter <= count);
	return counter;
}

/*
 * Get PIT output bit.
 */
static int
vpit_get_out(struct vpit_channel *ch, uint64_t current_time)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	uint64_t d, count;
	int out = 0;

	d = muldiv64(current_time - ch->count_load_time,
		     VM_PIT_FREQ, VM_TSC_FREQ);
	d /= VM_TIME_SCALE;
	count = (uint64_t)(uint32_t) ch->count;
	switch(ch->mode) {
	case PIT_CHANNEL_MODE_0:
		out = (d >= count);
		break;
	case PIT_CHANNEL_MODE_1:
		out = (d < count);
		break;
	case PIT_CHANNEL_MODE_2:
		if ((d % count) == 0 && d != 0)
			out = 1;
		else
			out = 0;
		break;
	case PIT_CHANNEL_MODE_3:
		out = (d % count) < ((count + 1) >> 1);
		break;
	case PIT_CHANNEL_MODE_4:
	case PIT_CHANNEL_MODE_5:
		out = (d == count);
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	return out;
}

/*
 * Get the time of next OUT change. If no OUT change will happen, valid will be
 * FALSE.
 */
static uint64_t
vpit_get_next_transition_time(struct vpit_channel *ch, uint64_t current_time,
			      bool *valid)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	uint64_t d, count, next_time = 0, base;
	int period2;

	d = muldiv64(current_time - ch->count_load_time,
		     VM_PIT_FREQ, VM_TSC_FREQ);
	d /= VM_TIME_SCALE;
	count = (uint64_t)(uint32_t) ch->count;
	*valid = TRUE;
	switch(ch->mode) {
	case PIT_CHANNEL_MODE_0:
	case PIT_CHANNEL_MODE_1:
		if (d < count)
			next_time = count;
		else {
			*valid = FALSE;
			return -1;
		}
		break;
	case PIT_CHANNEL_MODE_2:
		base = (d / count) * count;
		if ((d - base) == 0 && d != 0)
			next_time = base + count;
		else
			next_time = base + count + 1;
		break;
	case PIT_CHANNEL_MODE_3:
		base = (d / count) * count;
		period2 = ((count + 1) >> 1);
		if ((d - base) < period2)
			next_time = base + period2;
		else
			next_time = base + count;
		break;
	case PIT_CHANNEL_MODE_4:
	case PIT_CHANNEL_MODE_5:
		if (d < count)
			next_time = count;
		else if (d == count)
			next_time = count + 1;
		else {
			*valid = FALSE;
			return -1;
		}
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	/* convert to timer units */
	next_time = ch->count_load_time
		+ muldiv64(next_time, VM_TSC_FREQ, VM_PIT_FREQ);
	/* fix potential rounding problems */
	/* XXX: better solution: use a clock at VM_PIT_FREQ Hz */
	if (next_time <= current_time)
		next_time = current_time + 1;
	return next_time;
}

static void
vpit_set_gate(struct vpit *pit, int channel, int val)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);
	KERN_ASSERT(val == 0 || val == 1);

	struct vpit_channel *ch = &pit->channels[channel];
	uint64_t load_time = vmm_rdtsc(pit->vm);

	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	switch(ch->mode) {
	case 0:
	case 4:
		/* XXX: just disable/enable counting */
		break;
	case 1:
	case 5:
		if (ch->gate < val) {
			/* restart counting on rising edge */
			ch->count_load_time = load_time;
			vpit_irq_timer_update(ch, ch->count_load_time);
		}
		break;
	case 2:
	case 3:
		if (ch->gate < val) {
			/* restart counting on rising edge */
			ch->count_load_time = load_time;
			vpit_irq_timer_update(ch, ch->count_load_time);
		}
		/* XXX: disable/enable counting */
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	ch->gate = val;
}

static int
vpit_get_gate(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	return ch->gate;
}

static int32_t
vpit_get_initial_count(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	return ch->count;
}

static int
vpit_get_mode(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	return ch->mode;
}

static void
vpit_load_count(struct vpit_channel *ch, uint16_t val)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	int32_t count = val;

	if (count == 0)
		count = 0x10000;
	ch->count_load_time = vmm_rdtsc(ch->pit->vm);
	ch->count = count;
	/* KERN_DEBUG("[%llx] Set counter: %x.\n", */
	/* 	   ch->count_load_time, ch->count); */
	vpit_irq_timer_update(ch, ch->count_load_time);
}

static void
vpit_latch_count(struct vpit_channel *ch)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	if (!ch->count_latched) {
		ch->latched_count = vpit_get_count(ch);
		ch->count_latched = ch->rw_mode;
	}
}

static void
vpit_ioport_write(struct vpit *pit, uint32_t port, uint8_t data)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(port == PIT_CHANNEL0_PORT || port == PIT_CHANNEL1_PORT ||
		    port == PIT_CHANNEL2_PORT || port == PIT_CONTROL_PORT);

	int channel, rw;
	struct vpit_channel *ch;

	if (port == PIT_CONTROL_PORT) { /* i8254 Control Port */
		/*
		 * Control word format:
		 * +-----+-----+-----+-----+----+----+----+-----+
		 * | SC1 | SC0 | RW1 | RW0 | M2 | M1 | M0 | BCD |
		 * +-----+-----+-----+-----+----+----+----+-----+
		 *
		 * SC1,SC0: 00 - select counter 0
		 *          01 - select counter 1
		 *          10 - select counter 2
		 *          11 - read-back command
		 * RW1,RW0: 00 - counter latch command
		 *          01 - read/write least significant byte only
		 *          10 - read/write most significant byte only
		 *          11 - read/write least significant byte first, then
		 *               most significant byte
		 * M2,M1,M0: 000 - mode 0
		 *           001 - mode 1
		 *           _10 - mode 2
		 *           _11 - mode 3
		 *           100 - mode 4
		 *           101 - mode 5
		 * BSD: 0 - binary counter 16-bits
		 *      1 - binary coded decimal (BCD) counter
		 *
		 *
		 * Read-back command format:
		 * +----+----+----+----+----+----+----+----+
		 * | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 |
		 * +----+----+----+----+----+----+----+----+
		 *
		 * D7,D6: always 11
		 * D5: 0 - latch count of the selected counter(s)
		 * D4: 0 - latch status of the selected counter(s)
		 * D3: 1 - select counter 2
		 * D2: 1 - select counter 1
		 * D1: 1 - select counter 0
		 * D0: always 0
		 *
		 *
		 * Status byte format:
		 * +--------+------------+-----+-----+----+----+----+-----+
		 * | Output | NULL Count | RW1 | RW0 | M2 | M1 | M0 | BCD |
		 * +--------+------------+-----+-----+----+----+----+-----+
		 *
		 * Output: 1 - out pin is 1
		 *         0 - out pin is 0
		 * NULL Count: 1 - null count
		 *             0 - count available for reading
		 * RW1,RW0,M2,M1,BCD: see "Control word format" above
		 */

		/* XXX: not suuport BCD yet */
		if (data & 0x1)
			KERN_PANIC("PIT not support BCD yet.\n");

		channel = (data >> 6) & 0x3;

		if (channel == 3) { /* read-back command */
			KERN_ASSERT(!(data & 0x1));

			int i;
			for (i = 0; i < 3; i++) {
				if (!(data & (2 << i))) /* not selected */
					continue;

				ch = &pit->channels[i];

				spinlock_acquire(&ch->lk);

				if (!(data & 0x20)) { /* latch count */
					/* KERN_DEBUG("Latch counter of channel %d.\n", */
					/* 	   i); */
					vpit_latch_count(ch);
				}

				if (!(data & 0x10)) { /* latch status */
					/* KERN_DEBUG("Latch status of channel %d.\n", */
					/* 	   i); */

					/* only latch once */
					if (ch->status_latched) {
						spinlock_release(&ch->lk);
						continue;
					}

					uint64_t tsc = vmm_rdtsc(pit->vm);
					int out = vpit_get_out(ch, tsc);

					ch->status =
						(out << 7) |(ch->rw_mode << 4) |
						(ch->mode << 1) | ch->bcd;
					ch->status_latched = 1;
				}

				spinlock_release(&ch->lk);
			}
		} else {
			ch = &pit->channels[channel];
			rw = (data >> 4) & 3; /* get RW1,RW0 */

			spinlock_acquire(&ch->lk);

			if (rw == 0) { /* counter latch command */
				/* KERN_DEBUG("Latch counter of channel %d.\n", */
				/* 	   channel); */
				vpit_latch_count(ch);
			} else {
				/* KERN_DEBUG("Set channel %d: RW=%x, Mode=%x.\n", */
				/* 	   channel, rw, (data >> 1) & 7); */
				ch->rw_mode = rw;
				ch->read_state = rw;
				ch->write_state = rw;
				ch->mode = (data >> 1) & 7;
				ch->bcd = data & 0x1;
				/* XXX: need to update irq timer? */
			}

			spinlock_release(&ch->lk);
		}
	} else { /* i8254 Channel Ports */
		channel = port - PIT_CHANNEL0_PORT;
		ch = &pit->channels[channel];

		spinlock_acquire(&ch->lk);

		switch (ch->write_state) {
		case PIT_RW_STATE_LSB:
			/* load LSB only */
			/* KERN_DEBUG("Load LSB to channel %d: %x.\n", */
			/* 	   channel, data); */
			vpit_load_count(ch, data);
			break;

		case PIT_RW_STATE_MSB:
			/* load MSB only */
			/* KERN_DEBUG("Load MSB to channel %d: %x.\n", */
			/* 	   channel, data); */
			vpit_load_count(ch, data << 8);
			break;

		case PIT_RW_STATE_WORD0:
			/* load LSB first */
			/* KERN_DEBUG("Load LSB to channel %d: %x.\n", */
			/* 	   channel, data); */
			ch->write_latch = data;
			ch->write_state = PIT_RW_STATE_WORD1;
			break;

		case PIT_RW_STATE_WORD1:
			/* load MSB then */
			/* KERN_DEBUG("Load MSB to channel %d: %x.\n", */
			/* 	   channel, data); */
			vpit_load_count(ch, ch->write_latch | (data << 8));
			ch->write_state = PIT_RW_STATE_WORD0;
			break;

		default:
			KERN_PANIC("Invalid write state %x of counter %x.\n",
				   ch->write_state, channel);
		}

		spinlock_release(&ch->lk);
	}
}

static uint8_t
vpit_ioport_read(struct vpit *pit, uint32_t port)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(port == PIT_CHANNEL0_PORT || port == PIT_CHANNEL1_PORT ||
		    port == PIT_CHANNEL2_PORT);

	int ret = 0, count;
	int channel = port-PIT_CHANNEL0_PORT;
	struct vpit_channel *ch = &pit->channels[channel];

	spinlock_acquire(&ch->lk);

	if (ch->status_latched) { /* read latched status byte */
		/* KERN_DEBUG("Read status of channel %d: %x.\n", */
		/* 	   channel, ch->status); */
		ch->status_latched = 0;
		ret = ch->status;
	} else if (ch->count_latched) { /* read latched count */
		switch(ch->count_latched) {
		case PIT_RW_STATE_LSB:
			/* read LSB only */
			/* KERN_DEBUG("Read LSB from channel %d: %x.\n", */
			/* 	   channel, ch->latched_count & 0xff); */
			ret = ch->latched_count & 0xff;
			ch->count_latched = 0;
			break;

		case PIT_RW_STATE_MSB:
			/* read MSB only */
			/* KERN_DEBUG("Read MSB from channel %d: %x.\n", */
			/* 	   channel, ch->latched_count >> 8); */
			ret = ch->latched_count >> 8;
			ch->count_latched = 0;
			break;

		case PIT_RW_STATE_WORD0:
			/* read LSB first */
			/* KERN_DEBUG("Read LSB from channel %d: %x.\n", */
			/* 	   ch->latched_count & 0xff); */
			ret = ch->latched_count & 0xff;
			ch->count_latched = PIT_RW_STATE_MSB;
			break;

		default:
			KERN_PANIC("Invalid read state %x of counter %x.\n ",
				   ch->count_latched, channel);
		}
	} else { /* read unlatched count */
		switch(ch->read_state) {
		case PIT_RW_STATE_LSB:
			count = vpit_get_count(ch);
			ret = count & 0xff;
			/* KERN_DEBUG("Read LSB from channel %d: %x.\n", */
			/* 	   channel, ret); */
			break;

		case PIT_RW_STATE_MSB:
			count = vpit_get_count(ch);
			ret = (count >> 8) & 0xff;
			/* KERN_DEBUG("Read MSB from channel %d: %x.\n", */
			/* 	   channel, ret); */
			break;

		case PIT_RW_STATE_WORD0:
			count = vpit_get_count(ch);
			ret = count & 0xff;
			/* KERN_DEBUG("Read LSB from channel %d: %x.\n", */
			/* 	   channel, ret); */
			ch->read_state = PIT_RW_STATE_WORD1;
			break;

		case PIT_RW_STATE_WORD1:
			count = vpit_get_count(ch);
			ret = (count >> 8) & 0xff;
			/* KERN_DEBUG("Read MSB from channel %d: %x.\n", */
			/* 	   channel, ret); */
			ch->read_state = PIT_RW_STATE_WORD0;
			break;

		default:
			KERN_PANIC("Invalid read state %x of counter %x.\n",
				   ch->read_state, channel);
		}
	}

	spinlock_release(&ch->lk);

	return ret;
}

static void
_vpit_ioport_read(struct vm *vm, void *pit, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && pit != NULL && data != NULL);
	KERN_ASSERT(port == PIT_CONTROL_PORT || port == PIT_CHANNEL0_PORT ||
		    port == PIT_CHANNEL1_PORT || port == PIT_CHANNEL2_PORT);

	*(uint8_t *) data = vpit_ioport_read(pit, port);

	/* KERN_DEBUG("Read: port=%x, data=%x.\n", port, *(uint8_t *) data); */
}

static void
_vpit_ioport_write(struct vm *vm, void *pit, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && pit != NULL && data != NULL);
	KERN_ASSERT(port == PIT_CONTROL_PORT || port == PIT_CHANNEL0_PORT ||
		    port == PIT_CHANNEL1_PORT || port == PIT_CHANNEL2_PORT);

	/* KERN_DEBUG("Write: port=%x, data=%x.\n", port, *(uint8_t *) data); */

	vpit_ioport_write(pit, port, *(uint8_t *) data);
}

/*
 * Read I/O port 0x61.
 *
 * XXX: I/O port 0x61 has multiple functions, including setting up the speakers
 *      and setting up GATE of i8254 channel 2. CertiKOS only simulates the GATE
 *      setup function, and ignores all speaker setup requests (and does not
 *      pass requests to the physical hardware).
 */
static uint8_t
vpit_gate_ioport_read(struct vpit *pit)
{
	KERN_ASSERT(pit != NULL);

	struct vpit_channel *ch = &pit->channels[2];
	uint64_t current_time = vmm_rdtsc(pit->vm);

	spinlock_acquire(&ch->lk);
	uint8_t ret =
		(vpit_get_out(ch, current_time) << 5) | vpit_get_gate(pit, 2);
	spinlock_release(&ch->lk);

	/* KERN_DEBUG("[%llx] Read GATE of channel 2: %x\n", current_time, ret); */

	return ret;
}

/*
 * Write I/O port 0x61.
 *
 * XXX: I/O port 0x61 has multiple functions, including setting up the speakers
 *      and setting up GATE of i8254 channel 2. CertiKOS only simulates the GATE
 *      setup function, and ignores all speaker setup requests (and does not
 *      pass the requests to the physical hardware).
 */
static void
vpit_gate_ioport_write(struct vpit *pit, uint8_t data)
{
	KERN_ASSERT(pit != NULL);

	/* KERN_DEBUG("[%llx] Set GATE of channel 2: gate=%x.\n", */
	/* 	   vmm_rdtsc(pit->vm), data & 0x1); */

	spinlock_acquire(&pit->channels[2].lk);
	vpit_set_gate(pit, 2, data & 0x1);
	spinlock_release(&pit->channels[2].lk);
}

static void
_vpit_gate_ioport_read(struct vm *vm, void *pit, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && pit != NULL && data != NULL);
	KERN_ASSERT(port == PIT_GATE_PORT);

	*(uint8_t *) data = vpit_gate_ioport_read(pit);
}

static void
_vpit_gate_ioport_write(struct vm *vm, void *pit, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && pit != NULL && data != NULL);
	KERN_ASSERT(port == PIT_GATE_PORT);

	vpit_gate_ioport_write(pit, *(uint8_t *) data);
}

static void
vpit_irq_timer_update(struct vpit_channel *ch, uint64_t current_time)
{
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	/* if OUT of channel0 changes, let virtualized i8259 know */
	bool is_channel0 = (&ch->pit->channels[0] == ch) ? TRUE : FALSE;
	if (ch->enabled == TRUE && is_channel0 == TRUE &&
	    ch->next_transition_time <= current_time) {
		/* KERN_DEBUG("[%llx] Channel 0 timeout: %llx.\n", */
		/* 	   current_time, ch->next_transition_time); */
#if 0
		int irq_level = vpit_get_out(ch, current_time);
		KERN_DEBUG("[%llx] Set TIMER_IRQ to %x.\n",
			   current_time, irq_level);
		vmm_set_vm_irq(ch->pit->vm, IRQ_TIMER, irq_level);
#endif
	}

	/* update next transition time */
	uint64_t expired_time;
	bool expired_time_valid;

	expired_time = vpit_get_next_transition_time(ch, current_time,
						     &expired_time_valid);

	if (expired_time_valid == TRUE) {
		/* KERN_DEBUG("[%llx] Update timeout of channel %x to %llx.\n", */
		/* 	   current_time, */
		/* 	   (ch - ch->pit->channels)/sizeof(struct vpit_channel), */
		/* 	   expired_time); */

		ch->next_transition_time = expired_time;
		ch->enabled = TRUE;
	} else {
		/* KERN_DEBUG("[%llx] Disable channel %x.\n", */
		/* 	   current_time, */
		/* 	   (ch - ch->pit->channels)/sizeof(struct vpit_channel)); */
		ch->enabled = FALSE;
	}
}

static void
vpit_timer_intr_handler(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
}

void
vpit_init(struct vpit *pit, struct vm *vm)
{
	KERN_ASSERT(pit != NULL && vm != NULL);

	memset(pit, 0x0, sizeof(struct vpit));

	pit->vm = vm;

	/* initialize channle 0 */
	spinlock_init(&pit->channels[0].lk);
	pit->channels[0].pit = pit;
	pit->channels[0].enabled = FALSE;

	/* initialize channel 1 */
	spinlock_init(&pit->channels[1].lk);
	pit->channels[1].pit = pit;
	pit->channels[1].enabled = FALSE;

	/* initialize channel 2 */
	spinlock_init(&pit->channels[2].lk);
	pit->channels[2].pit = pit;
	pit->channels[2].enabled = FALSE;

	/* register virtualized device (handlers of I/O ports & IRQ) */
	vmm_iodev_register_read(vm, pit, PIT_CONTROL_PORT, SZ8,
				_vpit_ioport_read);
	vmm_iodev_register_read(vm, pit, PIT_CHANNEL0_PORT, SZ8,
				_vpit_ioport_read);
	vmm_iodev_register_read(vm, pit, PIT_CHANNEL1_PORT, SZ8,
				_vpit_ioport_read);
	vmm_iodev_register_read(vm, pit, PIT_CHANNEL2_PORT, SZ8,
				_vpit_ioport_read);
	vmm_iodev_register_read(vm, pit, PIT_GATE_PORT, SZ8,
				_vpit_gate_ioport_read);
	vmm_iodev_register_write(vm, pit, PIT_CONTROL_PORT, SZ8,
				 _vpit_ioport_write);
	vmm_iodev_register_write(vm, pit, PIT_CHANNEL0_PORT, SZ8,
				 _vpit_ioport_write);
	vmm_iodev_register_write(vm, pit, PIT_CHANNEL1_PORT, SZ8,
				 _vpit_ioport_write);
	vmm_iodev_register_write(vm, pit, PIT_CHANNEL2_PORT, SZ8,
				 _vpit_ioport_write);
	vmm_iodev_register_write(vm, pit, PIT_GATE_PORT, SZ8,
				 _vpit_gate_ioport_write);
	vmm_register_extintr(vm, pit, IRQ_TIMER, vpit_timer_intr_handler);
}

void
vpit_update(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	uint64_t tsc = vmm_rdtsc(vm);

	/* KERN_DEBUG("[%llx] vpit_timer_intr_handler()\n", tsc); */

	int i;
	for (i = 0; i < 3; i++) {
		struct vpit_channel *ch = &vm->vpit.channels[i];
		spinlock_acquire(&ch->lk);
		if (ch->enabled == TRUE) {
			/* KERN_DEBUG("[%llx] Check channel %x.\n", tsc, i); */
			vpit_irq_timer_update(ch, tsc);
		}
		spinlock_release(&ch->lk);
	}
}

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pit.h>

#define PIT_FREQUENCY		1193182

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

/*
 * Get the remaining numbers to count before OUT changes.
 */
static uint16_t
vpit_get_count(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	uint64_t d;
	int counter;

	d = muldiv64(qemu_get_clock_ns(vm_clock) - s->count_load_time, PIT_FREQ,
		     get_ticks_per_sec());

	switch(ch->mode) {
	case PIT_CHANNEL_MODE_0:
	case PIT_CHANNEL_MODE_1:
	case PIT_CHANNEL_MODE_4:
	case PIT_CHANNEL_MODE_5:
		counter = (ch->count - d) & 0xffff;
		break;
	case PIT_CHANNEL_MODE_3:
		/* XXX: may be incorrect for odd counts */
		counter = ch->count - ((2 * d) % ch->count);
		break;
	case PIT_CHANNEL_MODE_2::
		counter = ch->count - (d % ch->count);
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	return counter;
}

/*
 * Get PIT output bit.
 */
static int
vpit_get_out(struct vpit *pit, int channel, int64_t current_time)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	uint64_t d;
	int out;

	d = muldiv64(current_time - ch->count_load_time, PIT_FREQ,
		     get_ticks_per_sec());
	switch(ch->mode) {
	default:
	case PIT_CHANNEL_MODE_0:
		out = (d >= ch->count);
		break;
	case PIT_CHANNEL_MODE_1:
		out = (d < ch->count);
		break;
	case PIT_CHANNEL_MODE_2:
		if ((d % ch->count) == 0 && d != 0)
			out = 1;
		else
			out = 0;
		break;
	case PIT_CHANNEL_MODE_3:
		out = (d % ch->count) < ((ch->count + 1) >> 1);
		break;
	case PIT_CHANNEL_MODE_4:
	case PIT_CHANNEL_MODE_5:
		out = (d == ch->count);
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	return out;
}

/*
 * Get the time of next OUT change; or -1, if no change will happen.
 */
static int64_t
vpit_get_next_transition_time(struct vpit *pit, int channel, int64_t current_time)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	uint64_t d, next_time, base;
	int period2;

	d = muldiv64(current_time - ch->count_load_time, PIT_FREQ,
		     get_ticks_per_sec());
	switch(ch->mode) {
	default:
	case PIT_CHANNEL_MODE_0:
	case PIT_CHANNEL_MODE_1:
		if (d < ch->count)
			next_time = ch->count;
		else
			return -1;
		break;
	case PIT_CHANNEL_MODE_2:
		base = (d / ch->count) * ch->count;
		if ((d - base) == 0 && d != 0)
			next_time = base + ch->count;
		else
			next_time = base + ch->count + 1;
		break;
	case PIT_CHANNEL_MODE_3:
		base = (d / ch->count) * ch->count;
		period2 = ((ch->count + 1) >> 1);
		if ((d - base) < period2)
			next_time = base + period2;
		else
			next_time = base + ch->count;
		break;
	case PIT_CHANNEL_MODE_4:
	case PIT_CHANNEL_MODE_5:
		if (d < ch->count)
			next_time = ch->count;
		else if (d == ch->count)
			next_time = ch->count + 1;
		else
			return -1;
		break;

	default:
		KERN_PANIC("Invalid PIT channel mode %x.\n", ch->mode);
	}
	/* convert to timer units */
	next_time = ch->count_load_time + muldiv64(next_time, get_ticks_per_sec(),
						  PIT_FREQ);
	/* fix potential rounding problems */
	/* XXX: better solution: use a clock at PIT_FREQ Hz */
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

	struct vpit_channel *ch = &vpit->channels[channel];

	switch(ch->mode) {
	case 0:
	case 4:
		/* XXX: just disable/enable counting */
		break;
	case 1:
	case 5:
		if (ch->gate < val) {
			/* restart counting on rising edge */
			ch->count_load_time = qemu_get_clock_ns(vm_clock);
			vpit_irq_timer_update(s, ch->count_load_time);
		}
		break;
	case 2:
	case 3:
		if (ch->gate < val) {
			/* restart counting on rising edge */
			ch->count_load_time = qemu_get_clock_ns(vm_clock);
			vpit_irq_timer_update(s, ch->count_load_time);
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

	return pit->channels[channel].gate;
}

static int
vpit_get_initial_count(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	return vpit->channels[channle].count;
}

static int
vpit_get_mode(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	return vpit->channels[channel].mode;
}

static void
vpit_load_count(struct vpit *pit, int channel, int val)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &vpit->channels[channel];

	if (val == 0)
		val = 0x10000;
	ch->count_load_time = qemu_get_clock_ns(vm_clock);
	ch->count = val;
	vpit_irq_timer_update(s, ch->count_load_time);
}

static void
vpit_latch_count(struct vpit *pit, int channel)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &vpit->channels[channel];

	if (!ch->count_latched) {
		ch->latched_count = vpit_get_count(s);
		ch->count_latched = ch->rw_mode;
	}
}

static void
vpit_ioport_write(struct vpit *pit, uint32_t port, uint32_t data)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(port == PIT_CHANNEL0_PORT || port == PIT_CHANNEL1_PORT ||
		    port == PIT_CHANNEL2_PORT || port == PIT_CONTROL_PORT);

	int channel, rw;
	struct vpit_channel *ch;

	if (port == PIT_CONTROL_PORT) {
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

				if (!(data & 0x20)) /* latch count */
					vpit_latch_count(vpit, i);

				if (!(data & 0x10)) { /* latch status */
					ch = &vpit->channels[i];

					/* only latch once */
					if (ch->status_latched)
						continue;

					ch->status =
						(vpit_get_out(vpit, i, qemu_get_clock_ns(vm_clock)) << 7) |
						(ch->rw_mode << 4) |
						(ch->mode << 1) |
						s->bcd;
					ch->status_latched = 1;
				}
			}
		} else {
			ch = &pit->channels[channel];
			rw = (data >> 4) & 3; /* get RW1,RW0 */

			if (rw == 0) { /* counter latch command */
				vpit_latch_count(ch);
			} else {
				ch->rw_mode = rw;
				ch->read_state = rw;
				ch->write_state = rw;
				ch->mode = (data >> 1) & 7;
				ch->bcd = data & 0x1;
				/* XXX: need to update irq timer? */
			}
		}
	} else {
		ch = &pit->channels[port-PIT_CHANNEL0_PORT];

		switch (ch->write_state) {
		case PIT_RW_STATE_LSB:
			/* load LSB only */
			vpit_load_count(pit, channel, data);
			break;

		case PIT_RW_STATE_MSB:
			/* load MSB only */
			vpit_load_count(pit, channel, data << 8);
			break;

		case PIT_RW_STATE_WORD0:
			/* load LSB first */
			ch->write_latch = data;
			ch->write_state = RW_STATE_WORD1;
			break;

		case PIT_RW_STATE_WORD1:
			/* load MSB then */
			vpit_load_count(pit, channel,
					ch->write_latch | (data << 8));
			ch->write_state = RW_STATE_WORD0;
			break;

		default:
			KERN_PANIC("Invalid write state %x of counter %x.\n",
				   ch->write_state, channel);
		}
	}
}

static uint32_t
vpit_ioport_read(struct vpit *pit, uint32_t port)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(port == PIT_CHANNEL0_PORT || port == PIT_CHANNEL1_PORT ||
		    port == PIT_CHANNEL2_PORT);

	int ret, count;
	int channel = port-PIT_CHANNEL0_PORT;
	struct vpit_channel *ch = &pit->channels[channel];

	if (ch->status_latched) { /* read latched status byte */
		ch->status_latched = 0;
		ret = ch->status;
	} else if (ch->count_latched) { /* read latched count */
		switch(ch->count_latched) {
		case PIT_RW_STATE_LSB:
			/* read LSB only */
			ret = ch->latched_count & 0xff;
			ch->count_latched = 0;
			break;

		case PIT_RW_STATE_MSB:
			/* read MSB only */
			ret = ch->latched_count >> 8;
			ch->count_latched = 0;
			break;

		case PIT_RW_STATE_WORD0:
			/* read MSB first */
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
			count = vpit_get_count(pit, channel);
			ret = count & 0xff;
			break;

		case PIT_RW_STATE_MSB:
			count = vpit_get_count(pit, channel);
			ret = (count >> 8) & 0xff;
			break;

		case PIT_RW_STATE_WORD0:
			count = vpit_get_count(pit, channel);
			ret = count & 0xff;
			ch->read_state = PIT_RW_STATE_WORD1;
			break;

		case PIT_RW_STATE_WORD1:
			count = vpit_get_count(pit, channel);
			ret = (count >> 8) & 0xff;
			ch->read_state = PIT_RW_STATE_WORD0;
			break;

		default:
			KERN_PANIC("Invalid read state %x of counter %x.\n",
				   ch->read_state, channel);
		}
	}

	return ret;
}

static void
vpit_irq_timer_update(struct vpit *pit, int channel, int64_t current_time)
{
	KERN_ASSERT(pit != NULL);
	KERN_ASSERT(0 <= channel && channel < 3);

	struct vpit_channel *ch = &pit->channels[channel];

	int64_t expire_time;
	int irq_level;

	if (!ch->irq_timer)
		return;
	expire_time = vpit_get_next_transition_time(pit, channel, current_time);
	irq_level = vpit_get_out(pit, channel, current_time);
	qemu_set_irq(ch->irq, irq_level);
	ch->next_transition_time = expire_time;
	if (expire_time != -1)
		qemu_mod_timer(ch->irq_timer, expire_time);
	else
		qemu_del_timer(ch->irq_timer);
}

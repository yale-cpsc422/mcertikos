#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

#include <dev/pic.h>

#define i8259_pre_debug(chip)					\
	do {							\
		struct i8259 *_chip = (struct i8259 *) (chip);	\
		if (_chip->master == TRUE)			\
			KERN_DEBUG("Master: ");			\
		else						\
			KERN_DEBUG("Slave: ");			\
	} while (0)

/*
 * Get the priority number of the IR in mask with the highest priority.
 *
 * mask is a 8-bit vector. Bit i represents IR i.
 *
 * XXX: i8259 has three kinds of priority mode:
 * - Fixed Mode: IR0 has the highest priority, while IR7 has the lowest
 *               priority.
 * - Automatic Rotation Mode: In this model a device, after being serviced,
 *               receives the lowest priority, e.g. consider IR4 has the
 *               highest priority. Before being serviced,
 *                    +---+---+---+---+---+---+---+---+
 *                    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *                    +---+---+---+---+---+---+---+---+
 *                      ^                           ^
 *                    lowest                      highest
 *                   priority                     priority
 *               After being serviced,
 *                    +---+---+---+---+---+---+---+---+
 *                    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 *                    +---+---+---+---+---+---+---+---+
 *                                  ^   ^
 *                           lowest |   | highest
 *                         priority       priority
 * - Specific rotation mode: In this model, programmers can specify which IR
 *               has the lowest priority, e.g. consider in this mode we specify
 *               IR6 has the lowest priority, then IR7 will have the highest
 *               priority.
 *
 * It's natural to consider the mask as a circle, and bits from the least
 * significant to the most significant are put in the circle in clockwise order.
 * Once we set the IR with the lowest priority, the priorities of all other
 * IR decrease in the anticlockwise.
 *
 * All priority modes of i8259 can be descibed in this mode as long as the
 * lowest priority is specified (lowest_priority field in struct i8259).
 *
 * @param chip
 * @param mask
 *
 * @return the IR number with the highest priority; or 8 if no IR present in
 *         mask (all 0s).
 */
static uint8_t
i8259_get_priority(struct i8259 *chip, uint8_t mask)
{
	KERN_ASSERT(chip != NULL);

	if (mask == 0) /* no IR present in mask */
		return 8;

	uint8_t priority = 0;
	while (!(mask & (1 << ((priority + chip->lowest_priority) & 7))))
		priority++;

	return priority;
}

/*
 * Check whether there is IRQ in IRR (but not in IMR) that has the higher
 * priority than the IRs in ISR. If so, return the IRQ number of such IRQ;
 * otherwise, return -1.
 *
 * XXX: There are three models that may affect the result of this function:
 * - Fully Nested Mode: In this mode, when an interrupt is acknowledged, the
 *                      highest priority request is determined and its vector
 *                      is put on the bus. Additional, a bit in ISR is set. All
 *                      interrupt with the same or lower priority are inhibited
 *                      until an EOI is received.
 * - Special Fully Nested Mode: In this mode, during an IRQ from a slave is in
 *                      serice, further interrupts from the slave with higher
 *                      priority are not inhibited.
 * - Special Mask Mode: In this mode, only interrupts masked in IMR are
 *                      inhibited.
 */
static int
i8259_get_irq(struct i8259 *chip)
{
	KERN_ASSERT(chip != NULL);

	int mask, cur_priority, priority;

	mask = chip->irr & ~chip->imr;
	priority = i8259_get_priority(chip, mask);
	if (priority == 8) {
		return -1;
	}
	/* compute current priority. If special fully nested mode on the
	   master, the IRQ coming from the slave is not taken into account
	   for the priority computation. */
	mask = chip->isr;
	if (chip->special_mask_mode) {
		mask &= ~chip->imr;
	}
	if (chip->special_fully_nested_mode && chip->master) {
		mask &= ~(1 << 2);
	}
	cur_priority = i8259_get_priority(chip, mask);
	if (priority < cur_priority) {
		/* higher priority found: an irq should be generated */
		return (priority + chip->lowest_priority) & 7;
	} else {
		return -1;
	}
}

/*
 * Update INT output. Check whether there is IR in IRR which has a higher
 * priority than all IRs in ISR. If so, output that IR in IRR to the INT
 * line of the virtualized i8259; otherwise, do nothing.
 */
static void
i8259_update_irq(struct i8259 *chip)
{
	KERN_ASSERT(chip != NULL);

	i8259_pre_debug(chip);

	int irq;

	irq = i8259_get_irq(chip);
	if (irq >= 0) {
		dprintf("Set INT_OUT for IRQ %x.\n", irq);
		chip->int_out = 1;
	} else {
		dprintf("Clear INT_OUT.\n");
		chip->int_out = 0;
	}
}

/*
 * Set irq level. If an edge is detected, then the IRR is set to 1.
 */
static void
i8259_set_irq(struct i8259 *chip, int irq, int level)
{
	KERN_ASSERT(chip != NULL);

	i8259_pre_debug(chip);

	int mask = 1 << irq;

	if (chip->elcr & mask) {
		/* level triggered */
		if (level) {
			dprintf("Set bit %x of IRR.\n", irq);
			chip->irr |= mask;
			chip->last_irr |= mask;
		} else {
			dprintf("Clear bit %x of IRR.\n", irq);
			chip->irr &= ~mask;
			chip->last_irr &= ~mask;
		}
	} else {
		/* edge triggered */
		if (level) {
			if ((chip->last_irr & mask) == 0) {
				dprintf("Set bit %x of IRR.\n", irq);
				chip->irr |= mask;
			} else
				dprintf("\n");
			chip->last_irr |= mask;
		} else {
			dprintf("Clear bit %x of IRR.\n", irq);
			chip->last_irr &= ~mask;
		}
	}

	/*
	 * If there is some interrupt in IRR with higher priority than the
	 * interrupt in ISR,
	 */
	i8259_update_irq(chip);
}

/*
 * Emulate an INTA is sent from CPU.
 *
 * XXX: If automatic rotation mode is set, remember to set the lowest priority
 *      here.
 */
static void
i8259_intack(struct i8259 *chip, int irq)
{
	KERN_ASSERT(chip != NULL);

	i8259_pre_debug(chip);
	dprintf("INTA\n");

	if (chip->auto_eoi_mode) {
		if (chip->rotate_on_auto_eoi == TRUE) {
			chip->lowest_priority = (irq + 1) & 7;
		}
	} else {
		i8259_pre_debug(chip);
		dprintf("Set bit %x of ISR.\n", irq);
		chip->isr |= (1 << irq);
	}

	/* We don't clear a level sensitive interrupt here */
	if (!(chip->elcr & (1 << irq))) {
		i8259_pre_debug(chip);
		dprintf("Clear bit %x of IRR.\n", irq);
		chip->irr &= ~(1 << irq);
	}

	i8259_update_irq(chip);
}

static void
i8259_init_reset(struct i8259 *chip)
{
	KERN_ASSERT(chip != NULL);

	chip->last_irr = 0;
	chip->irr = 0;
	chip->imr = 0;
	chip->isr = 0;
	chip->lowest_priority = 0;
	chip->irq_base = 0;
	chip->select_isr = FALSE;
	chip->poll = FALSE;
	chip->special_mask_mode = FALSE;
	chip->init_state = 0;
	chip->auto_eoi_mode = FALSE;
	chip->rotate_on_auto_eoi = FALSE;
	chip->special_fully_nested_mode = FALSE;
	chip->init4 = FALSE;
	chip->single_mode = FALSE;
	chip->ready = FALSE;
	/* Note: ELCR is not reset */

	i8259_update_irq(chip);
}

static void
i8259_reset(struct i8259 *chip)
{
	KERN_ASSERT(chip != NULL);

	i8259_init_reset(chip);
	chip->elcr = 0;
}

static void
vpic_ioport_write(struct vpic *vpic, uint8_t port, uint8_t data)
{
	KERN_ASSERT(vpic != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC1+1 ||
		    port == IO_PIC2 || port == IO_PIC2+1);

	bool master_slave =
		(port == IO_PIC1 || port == IO_PIC1+1) ? TRUE : FALSE;
	struct i8259 *master = &vpic->master;
	struct i8259 *slave = &vpic->slave;
	struct i8259 *chip = master_slave ? master : slave;

	switch (port) {
	case IO_PIC1:
	case IO_PIC2:
		if (data & 0x10) {
			/*
			 * ICW1
			 *
			 * D7~D5: 0
			 * D4   : 1
			 * D3   : 1 - level triggered mode,
			 *        0 - edge triggered mode
			 * D2   : 1 - interval of 4
			 *        0 - interval of 8
			 * D1   : 1 - single mode
			 *        0 - cascade mode
			 * D0   : 1 - ICW needed
			 *        0 - NO ICW4 needed
			 */
			i8259_pre_debug(chip);
			dprintf("Write ICW1: data=%x, port=%x.\n", data, port);

			i8259_init_reset(chip);
			chip->init_state = 1;
			chip->init4 = (data & 1) ? TRUE : FALSE;
			chip->single_mode = (data & 2) ? TRUE : FALSE;
			if (data & 0x08)
				KERN_PANIC("Level triggered mode not supported.\n");
		} else if ((data & 0x18) == 0x8) {
			/*
			 * OCW3
			 *
			 * D7    : 0
			 * D6,D5 : 00,01 - no action
			 *         10    - reset special mask
			 *         11    - set special mask
			 * D4    : 0
			 * D3    : 1
			 * D2    : 1 - poll command, 0 - no poll command
			 * D1,D0 : 00,01 - no action
			 *         10    - select IRR
			 *         11    - select ISR
			 */
			int special_mask = (data & 0x60) >> 5;
			int poll = (data & 0x04) >> 2;
			int select_isr = (data & 0x03);

			if (poll) { /* set poll mode */
				i8259_pre_debug(chip);
				dprintf("OCW3: Set poll mode, port=%x.\n", port);
				chip->poll = TRUE;
			}

			if (select_isr == 0x2) { /* select IRR */
				i8259_pre_debug(chip);
				dprintf("OCW3: Select IRR, port=%x.\n", port);
				chip->select_isr = FALSE;
			} else if (select_isr == 0x3) { /* select ISR */
				i8259_pre_debug(chip);
				dprintf("OCW3: Select ISR, port=%x.\n", port);
				chip->select_isr = TRUE;
			}

			if (special_mask == 0x2) { /* reset special mask mode */
				i8259_pre_debug(chip);
				dprintf("OCW3: Reset special mask mode, port=%x.\n",
					port);
				chip->special_mask_mode = FALSE;
			} else if (special_mask == 0x3) { /* set specal mask */
				i8259_pre_debug(chip);
				dprintf("OCW3: Set special mask mode, port=%x.\n",
					port);
				chip->special_mask_mode = TRUE;
			}
		} else {
			/*
			 * OCW2
			 *
			 * D7,D6,D5 : 001 - non-specific EOI
			 *            011 - specific EOI
			 *            101 - rotate on non-specific EOI
			 *            100 - rotate on automatic EOI (set)
			 *            000 - rotate on automatic EOI (clear)
			 *            111 - rotate on specific EOI
			 *            110 - set priority
			 *            010 - nop
			 * D4,D3    : 00
			 * D2,D1,D0 : IR level to be acted upon
			 */
			int priority, irq;

			uint8_t cmd = (data >> 5) & 0x7;
			switch (cmd) {
			case 0: /* rotate on automatic EOI (clear) */
				i8259_pre_debug(chip);
				dprintf("OCW2: Clear rotate on automatic EOI, port=%x.\n",
					port);
				chip->rotate_on_auto_eoi = FALSE;
				break;

			case 4: /* rotate on automatic EOI (set) */
				i8259_pre_debug(chip);
				dprintf("OCW2: Set rotate on automatic EOI, port=%x.\n",
					port);
				chip->rotate_on_auto_eoi = TRUE;
				break;

			case 1: /* non-specific EOI */
				i8259_pre_debug(chip);
				dprintf("OCW2: EOI, port=%x.\n", port);
			case 5: /* rotate on non-specific EOI */
				priority = i8259_get_priority(chip, chip->isr);

				if (priority == 8)
					break;

				irq = (priority + chip->lowest_priority) & 7;
				i8259_pre_debug(chip);
				dprintf("Clear bit %x of ISR.\n", irq);
				chip->isr &= ~(1 << irq);
				if (cmd == 5) {
					i8259_pre_debug(chip);
					dprintf("OCW2: Rotate to %x, port=%x.\n",
						(irq + 1) % 7, port);
					chip->lowest_priority = (irq + 1) & 7;
				}
				i8259_update_irq(chip);
				break;

			case 3: /* specific EOI */
				i8259_pre_debug(chip);
				dprintf("OCW2: EOI for IRQ%x, port=%x.\n",
					data & 7, port);
				irq = data & 7;
				i8259_pre_debug(chip);
				dprintf("Clear bit %x of ISR.\n", irq);
				chip->isr &= ~(1 << irq);
				i8259_update_irq(chip);
				break;

			case 6: /* set priority */
				i8259_pre_debug(chip);
				dprintf("OCW2: Set priority to %x, port=%x.\n",
					(data + 1) & 7, port);
				chip->lowest_priority = (data + 1) & 7;
				i8259_update_irq(chip);
				break;

			case 7: /* rotate on specific EOI */
				i8259_pre_debug(chip);
				dprintf("OCW2: Rotate to %x, port=%x.\n",
					(data + 1) & 7, port);
				irq = data & 7;
				i8259_pre_debug(chip);
				dprintf("Clear bit %x of ISR.\n", irq);
				chip->isr &= ~(1 << irq);
				chip->lowest_priority = (irq + 1) & 7;
				i8259_update_irq(chip);
				break;

			default:
				i8259_pre_debug(chip);
				dprintf("OCW2: Nop, port=%x.\n", port);
				break;
			}
		}

		break;

	case IO_PIC1+1:
	case IO_PIC2+1:
		switch (chip->init_state) {
		case 0: /* OCW1 */
			i8259_pre_debug(chip);
			dprintf("OCW1: Set IMR=%x, port=%x.\n", data, port);
			chip->imr = data;
			i8259_update_irq(chip);
			break;

		case 1: /* ICW2 */
			i8259_pre_debug(chip);
			dprintf("ICW2: Set IRQ base=%x, port=%x.\n",
				data & 0xf8, port);
			chip->irq_base = data & 0xf8;
			chip->init_state =
				chip->single_mode ? (chip->init4 ? 3 : 0) : 2;
			if (chip->init_state == 0)
				chip->ready = TRUE;
			break;

		case 2: /* ICW3 */
			i8259_pre_debug(chip);
			dprintf("ICW3: port=%x.\n", port);
			if (chip->init4) {
				chip->init_state = 3;
			} else {
				chip->init_state = 0;
				chip->ready = TRUE;
			}
			break;
		case 3:
			/*
			 * ICW4
			 *
			 * D7~D5 : 0
			 * D4    : 1 - special fully nested mode
			 *         0 - non-special fully nested mode
			 * D3,D2 : 0_ - non buffered mode
			 *         10 - buffered mode/slave
			 *         11 - buffered mode/master
			 * D1    : 1 - automatic EOI
			 *         0 - normal EOI
			 * D0    : 1 - 8086/0888 mode
			 *         0 - MCS-80/85 mode
			 */
			i8259_pre_debug(chip);
			dprintf("ICW4: SFN mode=%x, AEOI=%x, port=%x.\n",
				(data >> 4) & 1, (data >> 1) & 1, port);
			chip->special_fully_nested_mode = (data >> 4) & 1;
			chip->auto_eoi_mode = (data >> 1) & 1;
			chip->init_state = 0;
			chip->ready = TRUE;
			break;
		}

		break;

	default:
		KERN_PANIC("Unknown i8259 port %x.\n", port);
	}
}

static uint32_t
vpic_ioport_read(struct vpic *vpic, uint8_t port)
{
	KERN_ASSERT(vpic != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC1+1 ||
		    port == IO_PIC2 || port == IO_PIC2+1);

	bool master_slave =
		(port == IO_PIC1 || port == IO_PIC1+1) ? TRUE : FALSE;
	struct i8259 *master = &vpic->master;
	struct i8259 *slave = &vpic->slave;
	struct i8259 *chip = master_slave ? master : slave;

	uint8_t ret;

	if (chip->poll == TRUE) {
		ret = i8259_get_irq(chip);
		if (ret >= 0) {
			i8259_intack(chip, ret);
			ret |= 0x80;
		} else {
			ret = 0;
		}
		chip->poll = 0;

		i8259_pre_debug(chip);
		dprintf("Polling read: data=%x, port=%x.\n", ret, port);
	} else {
		if (port == IO_PIC1 || port == IO_PIC2) {
			if (chip->select_isr == TRUE) {
				ret = chip->isr;
				i8259_pre_debug(chip);
				dprintf("Read ISR: data=%x, port=%x.\n",
					ret, port);
			} else {
				ret = chip->irr;
				i8259_pre_debug(chip);
				dprintf("Read IRR: data=%x, port=%x.\n",
					ret, port);
			}
		} else {
			ret = chip->imr;
			i8259_pre_debug(chip);
			dprintf("Read IMR: data=%x, port=%x.\n", ret, port);
		}
	}

	return ret;
}

static void
elcr_ioport_write(struct vpic *vpic, uint8_t port, uint32_t data)
{
	KERN_ASSERT(vpic != NULL);
	KERN_ASSERT(port == IO_ELCR1 || port == IO_ELCR2);

	struct i8259 *chip = (port == IO_ELCR1) ? &vpic->master : &vpic->slave;

	chip->elcr = data & chip->elcr_mask;
	KERN_DEBUG("Set ELCR: data=%x, port=%x.\n", chip->elcr, port);
}

static uint32_t
elcr_ioport_read(struct vpic *vpic, uint8_t port)
{
	KERN_ASSERT(vpic != NULL);
	KERN_ASSERT(port == IO_ELCR1 || port == IO_ELCR2);

	struct i8259 *chip = (port == IO_ELCR1) ? &vpic->master : &vpic->slave;

	KERN_DEBUG("Read ELCR: data=%x, port=%x.\n", chip->elcr, port);
	return chip->elcr;
}

static void
_vpic_ioport_read(struct vm *vm, void *vpic, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vpic != NULL && data != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC2 ||
		    port == IO_PIC1+1 || port == IO_PIC2+1);

	*(uint8_t *) data = vpic_ioport_read(vpic, port);
}

static void
_vpic_ioport_write(struct vm *vm, void *vpic, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vpic != NULL && data != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC2 ||
		    port == IO_PIC1+1 || port == IO_PIC2+1);

	vpic_ioport_write(vpic, port, *(uint8_t *) data);
}

static void
_elcr_ioport_read(struct vm *vm, void *vpic, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vpic != NULL && data != NULL);
	KERN_ASSERT(port == IO_ELCR1 || port == IO_ELCR2);

	*(uint8_t *) data = elcr_ioport_read(vpic, port);
}

static void
_elcr_ioport_write(struct vm *vm, void *vpic, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vpic != NULL && data != NULL);
	KERN_ASSERT(port == IO_ELCR1 || port == IO_ELCR2);

	elcr_ioport_write(vpic, port, *(uint8_t *) data);
}

void
vpic_init(struct vpic *vpic, struct vm *vm)
{
	KERN_ASSERT(vpic != NULL);

	KERN_DEBUG("Master: %x, ISR@%x, IRR@%x, IMR@%x\n", &vpic->master,
		   &vpic->master.isr, &vpic->master.irr, &vpic->master.imr);
	KERN_DEBUG("Slave: %x, ISR@%x, IRR@%x, IMR@%x\n", &vpic->slave,
		   &vpic->slave.isr, &vpic->slave.irr, &vpic->slave.imr);

	i8259_reset(&vpic->master);
	vpic->master.master = TRUE;
	i8259_reset(&vpic->slave);
	vpic->slave.master = FALSE;
	vpic->master.vpic = vpic->slave.vpic = vpic;

	vmm_iodev_register_read(vm, vpic, IO_PIC1, SZ8, _vpic_ioport_read);
	vmm_iodev_register_read(vm, vpic, IO_PIC1+1, SZ8, _vpic_ioport_read);
	vmm_iodev_register_read(vm, vpic, IO_PIC2, SZ8, _vpic_ioport_read);
	vmm_iodev_register_read(vm, vpic, IO_PIC2+1, SZ8, _vpic_ioport_read);
	vmm_iodev_register_write(vm, vpic, IO_PIC1, SZ8, _vpic_ioport_write);
	vmm_iodev_register_write(vm, vpic, IO_PIC1+1, SZ8, _vpic_ioport_write);
	vmm_iodev_register_write(vm, vpic, IO_PIC2, SZ8, _vpic_ioport_write);
	vmm_iodev_register_write(vm, vpic, IO_PIC2+1, SZ8, _vpic_ioport_write);

	vmm_iodev_register_read(vm, vpic, IO_ELCR1, SZ8, _elcr_ioport_read);
	vmm_iodev_register_write(vm, vpic, IO_ELCR2, SZ8, _elcr_ioport_write);
}

/*
 * Read the IRQ with the highest priority and send INTA to virtualized i8259
 * chips (then i8259 will set and clear corresponding bit of ISR and IRR).
 */
int
vpic_read_irq(struct vpic *vpic)
{
	KERN_ASSERT(vpic != NULL);

	int irq, irq2, intno;

	irq = i8259_get_irq(&vpic->master);

	if (irq >= 0) {
		if (irq == 2) {
			/* irq is from slave i8259 */
			irq2 = i8259_get_irq(&vpic->slave);

			if (irq2 >= 0) {
				i8259_intack(&vpic->slave, irq2);
			} else {
				/* spurious IRQ on slave controller */
				irq2 = 7;
			}

			intno = vpic->slave.irq_base + irq2;
		} else {
			intno = vpic->master.irq_base + irq;
		}

		i8259_intack(&vpic->master, irq);
	} else {
		/* spurious IRQ on host controller */
		irq = 7;
		intno = vpic->master.irq_base + irq;
	}

	return intno;
}

/*
 * Check wheter there is IRQ from virtualized PIC.
 */
bool
vpic_has_irq(struct vpic *vpic)
{
	KERN_ASSERT(vpic != NULL);

	if (vpic->master.int_out == 1)
		KERN_DEBUG("master.INT_OUT is set.\n");

	return (vpic->master.int_out != 0) ? TRUE : FALSE;
}


/*
 * Set/Unset an IR line of the virtualized i8259. If the slave i8259 is
 * set/unset, this function will conseqently set/unset the master i8259.
 */
void
vpic_set_irq(struct vpic *vpic, int irq, int level)
{
	KERN_ASSERT(vpic != NULL);
	KERN_ASSERT(0 <= irq && irq < 16);

	if (irq < 8) {
		i8259_set_irq(&vpic->master, irq, level);
	} else {
		i8259_set_irq(&vpic->slave, irq-8, level);
		if (vpic->slave.int_out != 0)
			i8259_set_irq(&vpic->master, 2, 1);
	}
}

bool
vpic_is_ready(struct vpic *vpic)
{
	KERN_ASSERT(vpic != NULL);

	if (vpic->master.ready == TRUE && vpic->slave.ready == TRUE)
		return TRUE;
	else
		return FALSE;
}

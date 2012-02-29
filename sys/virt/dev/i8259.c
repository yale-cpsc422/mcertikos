#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_iodev.h>

#include <dev/pic.h>

static void _i8259_ioport_read(struct vm*, void*, uint32_t, void *);
static void _i8259_ioport_write(struct vm*, void*, uint32_t, void *);

static void
i8259_init(struct i8259 *chip, bool master)
{
	KERN_ASSERT(chip != NULL);

	/* register IO port read/write functions for master/slave PICs */
	uint32_t cmd_port = (master == TRUE) ? IO_PIC1 : IO_PIC2;
	uint32_t data_port = cmd_port + 1;
	vmm_iodev_register_read(chip, cmd_port, SZ8, _i8259_ioport_read);
	vmm_iodev_register_read(chip, data_port, SZ8, _i8259_ioport_read);
	vmm_iodev_register_write(chip, cmd_port, SZ8, _i8259_ioport_write);
	vmm_iodev_register_write(chip, data_port, SZ8, _i8259_ioport_write);

	chip->single = 0;

	if (master == TRUE) {
		chip->irq_offset = 0x08;
		chip->u.slave_mask = 0x04;
	} else {
		chip->irq_offset = 0x70;
		chip->u.slave_id = 0x2;
	}

	chip->special_fully_nested_mode = 0;
	chip->buffered_mode = 0;

	if (master == TRUE)
		chip->master_slave = 1;
	else
		chip->master_slave = 0;

	chip->auto_eoi = 0;

	chip->imr = 0xff;		/* mask all IRQs */
	/* if (master == TRUE) */
	/* 	chip->imr = 0xb8; */
	/* else */
	/* 	chip->imr = 0x0f; */
	chip->isr = 0x00;		/* no IRQs in service */
	chip->irr = 0x00;		/* no IRQ requests */
	chip->read_reg_select = 0;	/* select IRR */
	chip->irq = 0;
	chip->lowest_priority = 7;
	chip->INT = FALSE;
	chip->IRQ_in = 0;

	chip->init.in_init = FALSE;
	chip->init.requires_4 = FALSE;
	chip->init.byte_expected = 0;

	chip->special_mask = FALSE;
	chip->polled = FALSE;
	chip->rotate_on_autoeoi = FALSE;

	chip->edge_level = 0;
}

static void
i8259_clear_highest_interrupt(struct i8259 *chip)
{
	int irq;
	int lowest_priority;
	int highest_priority;

	/* clear highest current in service bit */
	lowest_priority = chip->lowest_priority;
	highest_priority = lowest_priority + 1;
	if(highest_priority > 7)
		highest_priority = 0;

	irq = highest_priority;
	do {
		if (chip->isr & (1 << irq)) {
			chip->isr &= ~(1 << irq);
			break; /* Return mask of bit cleared. */
		}

		irq ++;
		if(irq > 7)
			irq = 0;
	} while (irq != highest_priority);
}

static uint32_t
i8259_poll_read(struct i8259 *chip, uint32_t port)
{
	KERN_PANIC("Not implemented yet.\n");
	return 0;
}

static uint32_t
i8259_ioport_read(struct i8259 *chip, uint32_t port)
{
	KERN_ASSERT(chip != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC1+1 ||
		    port == IO_PIC2 || port == IO_PIC2+1);
	KERN_ASSERT((chip->master_slave == 1 &&
		     (port == IO_PIC1 || port == IO_PIC1+1)) ||
		    (chip->master_slave == 0 &&
		     (port == IO_PIC2 || port == IO_PIC2+1)));

	if (chip->polled == TRUE)
		return i8259_poll_read(chip, port);

	switch (port) {
	case IO_PIC1:
	case IO_PIC2:
		if (chip->read_reg_select) {	/* read ISR */
			KERN_DEBUG("Read ISR %x.\n", chip->isr);
			return chip->isr;
		} else {			/* read IRR */
			KERN_DEBUG("Read IRR %x.\n", chip->irr);
			return chip->irr;
		}
		break;

	case (IO_PIC1+1):			/* read IMR */
	case (IO_PIC2+1):
		KERN_DEBUG("Read IMR %x.\n", chip->imr);
		return chip->imr;
		break;

	default:
		KERN_PANIC("We should not be here.\n");
		return 0;
	}
}

static void
i8259_update(struct i8259 *chip)
{
	KERN_ASSERT(chip != NULL);

	uint8_t unmasked_requests;
	int irq;
	uint8_t isr, max_irq;
	uint8_t highest_priority = chip->lowest_priority + 1;
	if(highest_priority > 7)
		highest_priority = 0;

	if (chip->INT) { /* last interrupt still not acknowleged */
		return;
	}

	isr = chip->isr;
	if (chip->special_mask) {
		/* all priorities may be enabled.  check all IRR bits except ones
		 * which have corresponding ISR bits set
		 */
		max_irq = highest_priority;
	} else { /* normal mode */
		/* Find the highest priority IRQ that is enabled due to current ISR */
		max_irq = highest_priority;
		if (isr) {
			while ((isr & (1 << max_irq)) == 0) {
				max_irq++;
				if(max_irq > 7)
					max_irq = 0;
			}

			/*
			 * Highest priority interrupt in-service, no other
			 * priorities allowed.
			 */
			if (max_irq == highest_priority)
				return;
			if (max_irq > 7)
				KERN_PANIC("Invalid IRQ: %x.\n", max_irq);
		}
	}

	/* now, see if there are any higher priority requests */
	if ((unmasked_requests = (chip->irr & ~chip->imr))) {
		irq = highest_priority;
		do {
			/* for special mode, since we're looking at all IRQ's, skip if
			 * current IRQ is already in-service
			 */
			if (! (chip->special_mask && ((isr >> irq) & 0x01))) {
				if (unmasked_requests & (1 << irq)) {
					KERN_DEBUG("Signal IRQ %x.\n", irq);

					chip->INT = 1;
					chip->irq = irq;

					if (chip->master_slave == 0)
						vpic_raise_irq(chip->vpic, 2);

					return;
				} /* if (unmasked_requests & ... */
			}

			irq ++;
			if(irq > 7)
				irq = 0;
		} while(irq != max_irq); /* do ... */
	} /* if (unmasked_requests = ... */
}

static void
i8259_ioport_write(struct i8259 *chip, uint32_t port, uint32_t data)
{
	KERN_ASSERT(chip != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC1+1 ||
		    port == IO_PIC2 || port == IO_PIC2+1);
	KERN_ASSERT((chip->master_slave == 1 &&
		     (port == IO_PIC1 || port == IO_PIC1+1)) ||
		    (chip->master_slave == 0 &&
		     (port == IO_PIC2 || port == IO_PIC2+1)));

	switch (port) {
	case IO_PIC1:
	case IO_PIC2:
		if (data & 0x10) { /* ICW1 */
			KERN_DEBUG("Write ICW1: data=%x.\n", data);

			chip->init.in_init = 1;
			chip->init.requires_4 = (data & 0x01);
			chip->init.byte_expected = 2; /* expect OCW2 */
			chip->imr = 0x00;
			chip->isr = 0x00;
			chip->irr = 0x00;
			chip->lowest_priority = 7;
			chip->INT = 0;
			chip->auto_eoi = 0;
			chip->rotate_on_autoeoi = 0;

			if (port == IO_PIC2)
				chip->vpic->master.IRQ_in &= ~(1 << 2);

			if (data & 0x02)
				KERN_PANIC("ICW1: Single mode is not supported.\n");

			if (data & 0x08)
				KERN_PANIC("ICW1: Level sensitive mode is not supported.\n");

			return;
		}

		if ((data & 0x18) == 0x08) { /* OCW3 */
			KERN_DEBUG("Write OCW3: data=%x.\n", data);

			uint8_t special_mask, poll, read_op;

			special_mask = (data & 0x60) >> 5;
			poll         = (data & 0x04) >> 2;
			read_op      = (data & 0x03);

			if (poll) {
				chip->polled = TRUE;
				return;
			}

			if (read_op == 0x02) { /* select IRR */
				KERN_DEBUG("Select IRR.\n");
				chip->read_reg_select = 0;
			} else if (read_op == 0x03) { /* select ISR */
				KERN_DEBUG("Select ISR.\n");
				chip->read_reg_select = 1;
			}

			if (special_mask == 0x02) { /* reset special mask */
				KERN_DEBUG("Reset special mask.\n");
				chip->special_mask = FALSE;
			} else if (special_mask == 0x03) { /* set special mask */
				KERN_DEBUG("Set special mask.\n");
				chip->special_mask = TRUE;
				i8259_update(chip);
			}

			return;
		}

		/* OCW2 */
		KERN_DEBUG("Write OCW2: data=%x.\n", data);
		switch (data) {
		case 0x00: /* clear rotate on auto eoi mode */
			KERN_DEBUG("Clear rotate on AEOI mode.\n");
			chip->rotate_on_autoeoi = FALSE;
			break;

		case 0x80: /* set rotate on auto eoi mode */
			KERN_DEBUG("Set rotate on AEOI mode.\n");
			chip->rotate_on_autoeoi = TRUE;
			break;

		case 0xA0: /* rotate on non-specific end of interrupt */
		case 0x20: /* EOI */
			KERN_DEBUG("EOI.\n");
			i8259_clear_highest_interrupt(chip);

			if(data == 0xA0) {
				chip->lowest_priority++;
				if(chip->lowest_priority > 7)
					chip->lowest_priority = 0;
			}

			i8259_update(chip);
			break;

		case 0x40: /* ignore */
			break;

		case 0x60: /* specific EOI 0 */
		case 0x61: /* specific EOI 1 */
		case 0x62: /* specific EOI 2 */
		case 0x63: /* specific EOI 3 */
		case 0x64: /* specific EOI 4 */
		case 0x65: /* specific EOI 5 */
		case 0x66: /* specific EOI 6 */
		case 0x67: /* specific EOI 7 */
			KERN_DEBUG("EOI %x.\n", data-0x60);
			chip->isr &= ~(1 << (data - 0x60));
			i8259_update(chip);
			break;

		/* IRQ long priority commands */
		case 0xC0: /* 0 7 6 5 4 3 2 1 */
		case 0xC1: /* 1 0 7 6 5 4 3 2 */
		case 0xC2: /* 2 1 0 7 6 5 4 3 */
		case 0xC3: /* 3 2 1 0 7 6 5 4 */
		case 0xC4: /* 4 3 2 1 0 7 6 5 */
		case 0xC5: /* 5 4 3 2 1 0 7 6 */
		case 0xC6: /* 6 5 4 3 2 1 0 7 */
		case 0xC7: /* 7 6 5 4 3 2 1 0 */
			KERN_DEBUG("Set lowest priority to %x.\n", data-0xc0);
			chip->lowest_priority = data - 0xC0;
			break;

		case 0xE0: /* specific EOI and rotate 0 */
		case 0xE1: /* specific EOI and rotate 1 */
		case 0xE2: /* specific EOI and rotate 2 */
		case 0xE3: /* specific EOI and rotate 3 */
		case 0xE4: /* specific EOI and rotate 4 */
		case 0xE5: /* specific EOI and rotate 5 */
		case 0xE6: /* specific EOI and rotate 6 */
		case 0xE7: /* specific EOI and rotate 7 */
			KERN_DEBUG("EOI and rotate %x.\n", data-0xe0);
			chip->isr &= ~(1 << (data - 0xE0));
		        chip->lowest_priority = (data - 0xE0);
			i8259_update(chip);
			break;

		case 0x02: /* set single mode, ignore */
			break;

		default:
			KERN_PANIC("Invalid OCW2: data=%x.\n", data);
		}

		return;

	case (IO_PIC1+1):
	case (IO_PIC2+1):
		if (chip->init.in_init == TRUE) {
			switch (chip->init.byte_expected) {
			case 2: /* ICW2 */
				KERN_DEBUG("Write ICW2: data=%x.\n", data);

				chip->irq_offset = data & 0xf8;
				chip->init.byte_expected = 3;
				break;

			case 3: /* ICW3 */
				KERN_DEBUG("Write ICW3: data=%x.\n", data);

				if (chip->init.requires_4)
					chip->init.byte_expected = 4;
				else
					chip->init.in_init = FALSE;
				break;

			case 4: /* ICW4 */
				KERN_DEBUG("Write ICW4: data=%x.\n", data);

				if (data & 0x02)
					chip->auto_eoi = 1;
				else
					chip->auto_eoi = 0;

				if ((data & 0x01) == 0)
					KERN_PANIC("MCS-80/85 mode is not supported.\n");

				chip->init.in_init = 0;

				break;

			default:
				KERN_PANIC("Invalid ICW: data=%x.\n", data);
			}

			return;
		}

		/* OCW1 */
		KERN_DEBUG("Write OCW1: data=%x.\n", data);
		KERN_DEBUG("Set mask to %x.\n", data);
		chip->imr = data;
		i8259_update(chip);
		return;

	default:
		KERN_PANIC("Invalid port: port=%x.\n", port);
	}
}

static void
_i8259_ioport_read(struct vm *vm, void *iodev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && iodev != NULL && data != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC1+1 ||
		    port == IO_PIC2 || port == IO_PIC2+1);

	*(uint32_t *) data = i8259_ioport_read((struct i8259 *) iodev, port);
}

static void
_i8259_ioport_write(struct vm * vm, void *iodev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && iodev != NULL && data != NULL);
	KERN_ASSERT(port == IO_PIC1 || port == IO_PIC1+1 ||
		    port == IO_PIC2 || port == IO_PIC2+1);

	i8259_ioport_write((struct i8259 *) iodev, port, *(uint32_t *) data);
}

void
vpic_init(struct vpic *vpic)
{
	KERN_ASSERT(vpic != NULL);

	vpic->master.vpic = vpic;
	vpic->slave.vpic = vpic;

	i8259_init(&vpic->master, TRUE);
	i8259_init(&vpic->slave, FALSE);
}

void
vpic_raise_irq(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);

	uint8_t mask = (1 << (irq_no & 7));

	if (irq_no <= 7 && !(vpic->master.IRQ_in & mask)) {
		KERN_DEBUG("Raise IRQ%x on master PIC.\n", irq_no);

		vpic->master.IRQ_in |= mask;
		vpic->master.irr |= mask;
		i8259_update(&vpic->master);
	} else if (irq_no > 7 && irq_no <= 15 && !(vpic->slave.IRQ_in & mask)) {
		KERN_DEBUG("Raise IRQ%x on slave PIC.\n", irq_no);

		vpic->slave.IRQ_in |= mask;
		vpic->slave.irr |= mask;
		i8259_update(&vpic->slave);
	}
}

void
vpic_lower_irq(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);

	uint8_t mask = (1 << (irq_no & 7));

	if (irq_no <= 7 && (vpic->master.IRQ_in & mask)) {
		KERN_DEBUG("Lower IRQ%x on master PIC.\n", irq_no);

		vpic->master.IRQ_in &= ~(mask);
		vpic->master.irr &= ~(mask);
	} else if (irq_no > 7 && irq_no <= 15 && (vpic->slave.IRQ_in & mask)) {
		KERN_DEBUG("Lower IRQ%x on slave PIC.\n", irq_no);

		vpic->slave.IRQ_in &= ~(mask);
		vpic->slave.irr &= ~(mask);
	}
}

void
vpic_intack(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);

	uint8_t mask = (1 << (irq_no & 7));

	if (irq_no <= 7 && (vpic->master.IRQ_in & mask)) {
		KERN_DEBUG("INTA IRQ%x on master PIC.\n", irq_no);

		vpic->master.irr &= ~(mask);
		vpic->master.isr |= mask;
	} else if (irq_no > 7 && irq_no <= 15 && (vpic->slave.IRQ_in & mask)) {
		KERN_DEBUG("INTA IRQ%x on slave PIC.\n", irq_no);

		vpic->slave.irr &= ~(mask);
		vpic->slave.isr |= mask;

		vpic->master.irr &= ~(1 << 2);
		vpic->master.isr |= (1 << 2);
	}
}

void
vpic_set_irr(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);
}

void
vpic_clear_irr(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);
}

void
vpic_set_isr(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);
}

void
vpic_clear_isr(struct vpic *vpic, uint8_t irq_no)
{
	KERN_ASSERT(vpic != NULL);
}

void
vpic_eoi(struct vpic *vpic)
{
	KERN_ASSERT(vpic != NULL);
}

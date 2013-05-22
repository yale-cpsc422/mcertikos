/*
 * Serial (COM) port I/O device driver.
 *
 * Copyright (c) 2010 Yale University.
 * Copyright (c) 1993, 1994, 1995 Charles Hannum.
 * Copyright (c) 1990 The Regents of the University of California.
 * See section "BSD License" in the file LICENSES for licensing terms.
 *
 * This code is derived from the NetBSD pcons driver, and in turn derived
 * from software contributed to Berkeley by William Jolitz and Don Ahn.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

#include <sys/console.h>
#include <sys/intr.h>
#include <sys/x86.h>

#include <machine/trap.h>

#include <dev/ioapic.h>
#include <dev/lapic.h>
#include <dev/pic.h>
#include <dev/serial.h>

bool serial_exists;

// Stupid I/O delay routine necessitated by historical PC design flaws
static void
delay(void)
{
	inb(0x84);
	inb(0x84);
	inb(0x84);
	inb(0x84);
}

static int
serial_proc_data(void)
{
	if (!(inb(COM1+COM_LSR) & COM_LSR_DATA))
		return -1;
	return inb(COM1+COM_RX);
}

void
serial_intr(void)
{
	if (serial_exists)
		cons_intr(serial_proc_data);
}

static int
serial_reformatnewline(int c, int p)
{
	int cr = '\r';
	int nl = '\n';
	/* POSIX requires newline on the serial line to
	 * be a CR-LF pair. Without this, you get a malformed output
	 * with clients like minicom or screen
	 */
	if (c == nl) {
		outb(p, cr);
		outb(p, nl);
		return 1;
	}
	else
		return 0;
}

void
serial_putc(char c)
{
	if (!serial_exists)
		return;

	int i;
	for (i = 0;
	     !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800;
	     i++)
		delay();

	if (!serial_reformatnewline(c, COM1 + COM_TX))
		outb(COM1 + COM_TX, c);
}

void
serial_init(void)
{
	/* turn off interrupt */
	outb(COM1 + COM_IER, 0);

	/* set DLAB */
	outb(COM1 + COM_LCR, COM_LCR_DLAB);

	/* set baud rate */
	outb(COM1 + COM_DLL, 0x0001 & 0xff);
	outb(COM1 + COM_DLM, 0x0001 >> 8);

	/* Set the line status.  */
	outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

	/* Enable the FIFO.  */
	outb(COM1 + COM_FCR, 0xc7);

	/* Turn on DTR, RTS, and OUT2.  */
	outb(COM1 + COM_MCR, 0x0b);

	// Clear any preexisting overrun indications and interrupts
	// Serial COM1 doesn't exist if COM_LSR returns 0xFF
	serial_exists = (inb(COM1+COM_LSR) != 0xFF);
	(void) inb(COM1+COM_IIR);
	(void) inb(COM1+COM_RX);
}

void
serial_intenable(void)
{
	if (serial_exists) {
		outb(COM1+COM_IER, 1);
		intr_enable(IRQ_SERIAL13, 0);
		serial_intr();
	}
}

int
serial_intr_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	return 0;
}

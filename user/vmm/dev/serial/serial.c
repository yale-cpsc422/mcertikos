#include <debug.h>
#include <syscall.h>
#include <types.h>
#include <x86.h>

#include "serial.h"
#include "../../vmm_dev.h"

#define COM1		0x3F8
#define COM2		0x2F8
#define COM3		0x3E8
#define COM4		0x2E8

#define COM_RX		0	// In:	Receive buffer (DLAB=0)
#define COM_TX		0	// Out: Transmit buffer (DLAB=0)
#define COM_DLL		0	// Out: Divisor Latch Low (DLAB=1)
#define COM_DLM		1	// Out: Divisor Latch High (DLAB=1)
#define COM_IER		1	// Out: Interrupt Enable Register
#define COM_IER_RDI	0x01	//   Enable receiver data interrupt
#define COM_IIR		2	// In:	Interrupt ID Register
#define COM_FCR		2	// Out: FIFO Control Register
#define COM_LCR		3	// Out: Line Control Register
#define	COM_LCR_DLAB	0x80	//   Divisor latch access bit
#define	COM_LCR_WLEN8	0x03	//   Wordlength: 8 bits
#define COM_MCR		4	// Out: Modem Control Register
#define	COM_MCR_RTS	0x02	// RTS complement
#define	COM_MCR_DTR	0x01	// DTR complement
#define	COM_MCR_OUT2	0x08	// Out2 complement
#define COM_LSR		5	// In:	Line Status Register
#define COM_LSR_DATA	0x01	//   Data available
#define COM_LSR_TXRDY	0x20	//   Transmit buffer avail
#define COM_LSR_TSRE	0x40	//   Transmitter off
#define COM_MSR		6	// In: Modem Status Register
#define COM_SRR		7	// In: Shadow Receive Register

#define IRQ_SERIAL13	4

static int
vserial_read_ioport(void *dev, uint16_t port, data_sz_t width, void *val)
{
	if (!(0x3f8 <= port && port <= 0x3ff))
		return -1;
	return sys_read_ioport(port, width, val);
}

static int
vserial_write_ioport(void *dev, uint16_t port, data_sz_t width, uint32_t val)
{
	if (!(0x3f8 <= port && port <= 0x3ff))
		return -1;
	return sys_write_ioport(port, width, (uint8_t) val);
}

static int
vserial_update(void *dev)
{
	if (dev == NULL)
		return -1;

	uint8_t status;
	struct vserial *com = dev;

	if (sys_read_ioport(COM1+COM_LSR, SZ8, &status))
		return -2;

	if (status & COM_LSR_DATA)
		vdev_set_irq(com->vdev, IRQ_SERIAL13, 2);

	return 0;
}

int
vserial_init(struct vdev *vdev, struct vserial *dev)
{
	uint32_t port;

	dev->vdev = vdev;

	for (port = 0x3f8; port <= 0x3ff; port++)
		vdev_register_ioport(vdev, dev, port,
				     vserial_read_ioport, vserial_write_ioport);

	vdev_register_update(vdev, dev, vserial_update);

	return 0;
}

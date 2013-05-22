#include <debug.h>
#include <syscall.h>
#include <types.h>
#include <x86.h>

#include "serial.h"
#include "../../vmm_dev.h"

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
vserial_sync(void *dev, uint8_t irq)
{
	struct vserial *com = dev;
	if (irq != IRQ_SERIAL13)
		return -1;
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

	vdev_register_irq(vdev, dev, IRQ_SERIAL13, vserial_sync);

	return 0;
}

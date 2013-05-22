#include <debug.h>
#include <string.h>
#include <syscall.h>
#include <types.h>

#include "vmm.h"
#include "vmm_dev.h"

int
vdev_init(struct vdev *vdev, struct vm *vm)
{
	if (vdev == NULL)
		return -1;
	memzero(vdev, sizeof(struct vdev));
	vdev->vm = vm;
	return 0;
}

int
vdev_register_ioport(struct vdev *vdev, void *dev, uint16_t port,
		     in_func_t in_f, out_func_t out_f)
{
	if (vdev == NULL || dev == NULL)
		return -1;

	if (vdev->ioport[port].dev != NULL)
		return -2;

	vdev->ioport[port].dev = dev;
	vdev->ioport[port].in = in_f;
	vdev->ioport[port].out = out_f;

	return 0;
}

int
vdev_unregister_ioport(struct vdev *vdev, void *dev, uint16_t port)
{
	if (vdev == NULL || dev == NULL)
		return -1;

	if (vdev->ioport[port].dev != dev)
		return -2;

	vdev->ioport[port].dev = NULL;
	vdev->ioport[port].in = NULL;
	vdev->ioport[port].out = NULL;

	return 0;
}

int
vdev_register_irq(struct vdev *vdev, void *dev, uint8_t irq, sync_func_t f)
{
	if (vdev == NULL || dev == NULL)
		return -1;

	if (vdev->irq[irq].dev != NULL)
		return -2;

	vdev->irq[irq].dev = dev;
	vdev->irq[irq].sync = f;

	return 0;
}

int
vdev_unregister_irq(struct vdev *vdev, void *dev, uint8_t irq)
{
	if (vdev == NULL || dev == NULL)
		return -1;

	if (vdev->irq[irq].dev != dev)
		return -2;

	vdev->irq[irq].dev = NULL;
	vdev->irq[irq].sync = NULL;

	return 0;
}

int
vdev_register_pic(struct vdev *vdev, void *dev, set_irq_func_t set_irq_f,
		  read_intout_func_t read_f, get_intout_func_t get_f)
{
	if (vdev == NULL || dev == NULL)
		return -1;

	if (vdev->pic.dev != NULL)
		return -2;

	vdev->pic.dev = dev;
	vdev->pic.set_irq = set_irq_f;
	vdev->pic.read_intout = read_f;
	vdev->pic.get_intout = get_f;

	return 0;
}

int
vdev_unregister_pic(struct vdev *vdev, void *dev)
{
	if (vdev == NULL || dev == NULL)
		return -1;

	if (vdev->pic.dev != dev)
		return -2;

	vdev->pic.dev = NULL;
	vdev->pic.set_irq = NULL;
	vdev->pic.read_intout = NULL;
	vdev->pic.get_intout = NULL;

	return 0;
}

#define SET_IOPORT(lval, data, width) do {		\
		switch (width) {			\
		case SZ8:				\
			*lval = (uint8_t) data;		\
			break;				\
		case SZ16:				\
			*lval = (uint16_t) data;	\
			break;				\
		default:				\
			*lval = (uint32_t) data;	\
		}					\
	} while (0)

void
vdev_read_guest_ioport(struct vdev *vdev, uint16_t port, data_sz_t width,
		       uint32_t *val)
{
	uint32_t data = 0xffffffff;

	if (vdev == NULL)
		goto ret;

	if (vdev->ioport[port].dev == NULL || vdev->ioport[port].in == NULL)
		goto ret;

	if (vdev->ioport[port].in(vdev->ioport[port].dev, port, width, &data))
		data = 0xffffffff;

 ret:
	SET_IOPORT(val, data, width);
}

void
vdev_write_guest_ioport(struct vdev *vdev, uint16_t port, data_sz_t width,
			uint32_t val)
{
	if (vdev == NULL ||
	    vdev->ioport[port].dev == NULL || vdev->ioport[port].out == NULL)
		return;
	vdev->ioport[port].out(vdev->ioport[port].dev, port, width, val);
}

int
vdev_peep_intout(struct vdev *vdev)
{
	if (vdev == NULL ||
	    vdev->pic.dev == NULL || vdev->pic.get_intout == NULL)
		return -1;
	return vdev->pic.get_intout(vdev->pic.dev);
}

int
vdev_read_intout(struct vdev *vdev)
{
	if (vdev == NULL ||
	    vdev->pic.dev == NULL || vdev->pic.read_intout == NULL)
		return -1;
	return vdev->pic.read_intout(vdev->pic.dev);
}

void
vdev_set_irq(struct vdev *vdev, uint8_t irq, int mode)
{
	if (vdev == NULL)
		return;

	if (mode == 0) {
		vdev->pic.set_irq(vdev->pic.dev, irq, 1);
	} else if (mode == 1) {
		vdev->pic.set_irq(vdev->pic.dev, irq, 0);
	} else if (mode == 2) {
		vdev->pic.set_irq(vdev->pic.dev, irq, 0);
		vdev->pic.set_irq(vdev->pic.dev, irq, 1);
	}
}

uint64_t
vdev_guest_tsc(struct vdev *vdev)
{
	if (vdev == NULL)
		return 0;
	return vdev->vm->tsc;
}

int
vdev_handle_intr(struct vdev *vdev, uint8_t irq)
{
	if (vdev == NULL)
		return -1;

	/* ignore all registered interrupts */
	if (vdev->irq[irq].dev == NULL || vdev->irq[irq].sync == NULL) {
		/* DEBUG("Ignore unregistered IRQ %d.\n", irq); */
		return 0;
	}

	return vdev->irq[irq].sync(vdev->irq[irq].dev, irq);
}

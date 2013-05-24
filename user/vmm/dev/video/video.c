#include <syscall.h>
#include <types.h>
#include <x86.h>

#include "video.h"
#include "../../vmm_dev.h"

static int
vvideo_read_ioport(void *dev, uint16_t port, data_sz_t width, void *val)
{
	if (dev == NULL || val == NULL)
		return -1;

	if (!((0x3b0 <= port && port <= 0x3bf) || /* MONO */
	      (0x3c0 <= port && port <= 0x3cf) || /* VGA */
	      (0x3d0 <= port && port <= 0x3df)    /* CGA */))
		return -2;

	return sys_read_ioport(port, width, val);
}

static int
vvideo_write_ioport(void *dev, uint16_t port, data_sz_t width, uint32_t val)
{
	if (dev == NULL)
		return -1;

	if (!((0x3b0 <= port && port <= 0x3bf) || /* MONO */
	      (0x3c0 <= port && port <= 0x3cf) || /* VGA */
	      (0x3d0 <= port && port <= 0x3df)    /* CGA */))
		return -2;

	return sys_write_ioport(port, width, val);
}

int
vvideo_init(struct vdev *vdev, struct vvideo *dev)
{
	uint32_t port;

	dev->vdev = vdev;

	for (port = 0x3b0; port < 0x3e0; port++)
		vdev_register_ioport(vdev, dev, port,
				     vvideo_read_ioport, vvideo_write_ioport);

	return 0;
}

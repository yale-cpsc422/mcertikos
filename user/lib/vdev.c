#include <debug.h>
#include <string.h>
#include <syscall.h>
#include <types.h>
#include <vdev.h>

int
vdev_init(struct vdev *vdev, void *opaque_dev, char *desc,
	  int (*init)(void *),
	  int (*read_ioport)(void *, uint16_t, data_sz_t, void *),
	  int (*write_ioport)(void *, uint16_t, data_sz_t, uint32_t),
	  int (*sync)(void *))
{
	if (vdev == NULL || opaque_dev == NULL)
		return 1;

	strncpy(vdev->desc, desc, 47);
	vdev->desc[47] = '\0';

	vdev->opaque_dev = opaque_dev;
	vdev->init = init;
	vdev->read_ioport = read_ioport;
	vdev->write_ioport = write_ioport;
	vdev->sync = sync;

	return 0;
}

int
vdev_start(struct vdev *vdev)
{
	void *opaque_dev;
	int chid;
	vdev_req_t req;
	struct vdev_ioport_info *read_req, *write_req;
	uint32_t data;

	if (vdev == NULL)
		return 1;

	if ((opaque_dev = vdev->opaque_dev) == NULL)
		return 2;

	if ((chid = sys_getchid()) == -1)
		return 3;

	if (vdev->init && vdev->init(opaque_dev))
		return 4;

	vdev_ready(chid);

	while (1) {
		if (!vdev_get_request(chid, &req, TRUE))
			continue;

		switch (((uint32_t *) &req)[0]) {
		case VDEV_READ_GUEST_IOPORT:
			read_req = (struct vdev_ioport_info *) &req;
			if (vdev->read_ioport == NULL ||
			    vdev->read_ioport(opaque_dev, read_req->port,
					      read_req->width, &data))
				data = 0xffffffff;
			vdev_return_guest_ioport(chid, read_req->port,
						 read_req->width, data);
			continue;


		case VDEV_WRITE_GUEST_IOPORT:
			write_req = (struct vdev_ioport_info *) &req;
			if (vdev->write_ioport)
				vdev->write_ioport(opaque_dev, write_req->port,
						   write_req->width,
						   write_req->val);
			continue;

		case VDEV_DEVICE_SYNC:
			if (vdev->sync)
				vdev->sync(opaque_dev);
			continue;

		default:
			continue;
		}
	}

	return 0;
}

vid_t
vdev_attach_proc(pid_t pid)
{
	return sys_attach_vdev(pid);
}

int
vdev_detach_proc(vid_t vid)
{
	return sys_detach_vdev(vid);
}

int
vdev_attach_ioport(uint16_t port, data_sz_t width)
{
	return sys_attach_port(port, width);
}

int
vdev_detach_ioport(uint16_t port, data_sz_t width)
{
	return sys_detach_port(port, width);
}

int
vdev_attach_irq(uint8_t irq)
{
	return sys_attach_irq(irq);
}

int
vdev_detach_irq(uint8_t irq)
{
	return sys_detach_irq(irq);
}

int
vdev_ready(int chid)
{
	struct vdev_device_ready rdy = { .magic = VDEV_DEVICE_READY };
	return sys_send(chid, &rdy, sizeof(rdy));
}

size_t
vdev_get_request(int chid, void *req, int blocking)
{
	size_t size;
	if (sys_recv(chid, req, &size))
		return 0;
	else
		return size;
}

int
vdev_return_guest_ioport(int chid, uint16_t port, data_sz_t width, uint32_t val)
{
	struct vdev_ioport_info info = { .magic = VDEV_GUEST_IOPORT_DATA,
					 .port = port,
					 .width = width,
					 .val = val };
	return sys_send(chid, &info, sizeof(info));
}

int
vdev_copy_from_guest(void *dest, uintptr_t src, size_t size)
{
	return sys_copy_from_guest(dest, src, size);
}

int
vdev_copy_to_guest(uintptr_t dest, void *src, size_t size)
{
	return sys_copy_to_guest(dest, src, size);
}

uint32_t
vdev_read_host_ioport(uint16_t port, data_sz_t width)
{
	return sys_host_in(port, width);
}

void
vdev_write_host_ioport(uint16_t port, data_sz_t width, uint32_t val)
{
	sys_host_out(port, width, val);
}

void
vdev_set_irq(uint8_t irq, int mode)
{
	sys_set_irq(irq, mode);
}

uint64_t
vdev_guest_tsc(void)
{
	return sys_guest_tsc();
}

uint64_t
vdev_guest_cpufreq(void)
{
	return sys_guest_cpufreq();
}

uint64_t
vdev_guest_memsize(void)
{
	return sys_guest_memsize();
}

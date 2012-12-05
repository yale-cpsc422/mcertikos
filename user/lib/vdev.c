#include <debug.h>
#include <proc.h>
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
	pid_t ppid;
	chid_t dev_in, dev_out;

	if (vdev == NULL || opaque_dev == NULL)
		return 1;

	dev_out = sys_getchid();

	if ((dev_in = sys_channel(sizeof(vdev_req_t))) == -1) {
		DEBUG("Cannot create IN channel.\n");
		return 2;
	}

	if ((ppid = getppid()) == -1) {
		DEBUG("Cannot find parent.\n");
		return 3;
	}

	if (sys_grant(dev_in, ppid, CHANNEL_PERM_SEND)) {
		DEBUG("Cannot grant sending permission of channel %d "
		      "to parent %d.\n", dev_in, ppid);
		return 4;
	}

	if (sys_send(dev_out, &dev_in, sizeof(chid_t))) {
		DEBUG("Cannot send IN channel to parent.\n");
		return 5;
	}

	vdev->dev_in = dev_in;
	vdev->dev_out = dev_out;

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
	vdev_req_t req;
	struct vdev_ioport_info *read_req, *write_req;
	uint32_t data;

	if (vdev == NULL)
		return 1;

	if ((opaque_dev = vdev->opaque_dev) == NULL)
		return 2;

	if (vdev_recv_ack(vdev->dev_in))
		return 3;

	if (vdev->init && vdev->init(opaque_dev))
		return 4;

	vdev_ready(vdev->dev_out);

	while (1) {
		if (vdev_get_request(vdev->dev_in, &req, sizeof(vdev_req_t)))
			continue;

		switch (((uint32_t *) &req)[0]) {
		case VDEV_READ_GUEST_IOPORT:
			read_req = (struct vdev_ioport_info *) &req;
			if (vdev->read_ioport == NULL ||
			    vdev->read_ioport(opaque_dev, read_req->port,
					      read_req->width, &data))
				data = 0xffffffff;
			vdev_return_guest_ioport(vdev->dev_out, read_req->port,
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

int
vdev_send_ack(chid_t dev_in)
{
	uint32_t ack = VM_TO_VDEV_ACK;
	return sys_send(dev_in, &ack, sizeof(ack));
}

int
vdev_recv_ack(chid_t dev_in)
{
	uint32_t ack;
	if (sys_recv(dev_in, &ack, sizeof(ack)) || ack != VM_TO_VDEV_ACK)
		return 1;
	return 0;
}

vid_t
vdev_attach_proc(pid_t pid, chid_t dev_in, chid_t dev_out)
{
	return sys_attach_vdev(pid, dev_in, dev_out);
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
vdev_ready(chid_t dev_out)
{
	struct vdev_device_ready rdy = { .magic = VDEV_DEVICE_READY };
	return sys_send(dev_out, &rdy, sizeof(rdy));
}

int
vdev_get_request(chid_t dev_in, void *req, size_t size)
{
	return sys_recv(dev_in, req, size);
}

int
vdev_return_guest_ioport(chid_t dev_out,
			 uint16_t port, data_sz_t width, uint32_t val)
{
	struct vdev_ioport_info info = { .magic = VDEV_GUEST_IOPORT_DATA,
					 .port = port,
					 .width = width,
					 .val = val };
	return sys_send(dev_out, &info, sizeof(info));
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

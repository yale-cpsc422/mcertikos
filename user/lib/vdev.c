#include <debug.h>
#include <syscall.h>
#include <types.h>
#include <vdev.h>

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

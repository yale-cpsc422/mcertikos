#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/serial.h>

#include <dev/serial.h>

static uint8_t
vserial_ioport_read(uint32_t port)
{
	KERN_ASSERT(port == COM1 || port == COM2 ||
		    port == COM3 || port == COM4);

	return inb(port);
}

static void
vserial_ioport_write(uint32_t port, uint8_t data)
{
	KERN_ASSERT(port == COM1 || port == COM2 ||
		    port == COM3 || port == COM4);

	dprintf("%c", port, (char) data);
}

static void
_vserial_ioport_read(struct vm *vm, void *vserial, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vserial != NULL && data != NULL);
	KERN_ASSERT(port == COM1 || port == COM2 ||
		    port == COM3 || port == COM4);

	*(uint8_t *) data = vserial_ioport_read(port);
}

static void
_vserial_ioport_write(struct vm *vm, void *vserial, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vserial != NULL && data != NULL);
	KERN_ASSERT(port == COM1 || port == COM2 ||
		    port == COM3 || port == COM4);

	vserial_ioport_write(port, *(uint8_t *) data);
}

void
vserial_init(struct vserial *vserial, struct vm *vm)
{
	KERN_ASSERT(vserial != NULL && vm != NULL);

	vmm_iodev_register_read(vm, vserial, COM1, SZ8, _vserial_ioport_read);
	vmm_iodev_register_read(vm, vserial, COM2, SZ8, _vserial_ioport_read);
	vmm_iodev_register_read(vm, vserial, COM3, SZ8, _vserial_ioport_read);
	vmm_iodev_register_read(vm, vserial, COM4, SZ8, _vserial_ioport_read);
	vmm_iodev_register_write(vm, vserial, COM1, SZ8, _vserial_ioport_write);
	vmm_iodev_register_write(vm, vserial, COM2, SZ8, _vserial_ioport_write);
	vmm_iodev_register_write(vm, vserial, COM3, SZ8, _vserial_ioport_write);
	vmm_iodev_register_write(vm, vserial, COM4, SZ8, _vserial_ioport_write);
}

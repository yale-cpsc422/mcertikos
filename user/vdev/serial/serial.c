#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/serial.h>

#include <dev/serial.h>
#include <dev/video.h>

static void
_vserial_ioport_read(struct vm *vm, void *vserial, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vserial != NULL && data != NULL);
	KERN_ASSERT(COM1 <= port && port < COM1 + 8);

	if (port == COM1 + COM_RX)
		*(uint8_t *) data = inb(port);
	else
		*(uint8_t *) data = 0xff;
}

static void
_vserial_ioport_write(struct vm *vm, void *vserial, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && vserial != NULL && data != NULL);
	KERN_ASSERT(COM1 <= port && port < COM1 + 8);

	if (port == COM1 + COM_TX)
		outb(port, *(uint8_t *) data);
}

void
vserial_init(struct vserial *vserial, struct vm *vm)
{
	KERN_ASSERT(vserial != NULL && vm != NULL);

	int i;

	for (i = 0; i < 8; i++) {
		vmm_iodev_register_read(vm, vserial, COM1+i, SZ8,
					_vserial_ioport_read);
		vmm_iodev_register_write(vm, vserial, COM1+i, SZ8,
					 _vserial_ioport_write);
	}
}

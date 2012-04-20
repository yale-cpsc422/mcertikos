#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/debug_dev.h>

#include <dev/video.h>

static void
_debug_ioport_read(struct vm *vm, void *debug_dev, uint32_t port, void *data)
{
	KERN_ASSERT(port == GUEST_DEBUG_IOPORT);
	KERN_ASSERT(vm != NULL && debug_dev != NULL && data != NULL);

	*(uint8_t *) data = 0xff;
}

static void
_debug_ioport_write(struct vm *vm, void *debug_dev, uint32_t port, void *data)
{
	KERN_ASSERT(port == GUEST_DEBUG_IOPORT);
	KERN_ASSERT(vm != NULL && debug_dev != NULL && data != NULL);

	char c = (char) (*(uint8_t *) data);
	video_putc(c);
}

void
guest_debug_dev_init(struct guest_debug_dev *dev, struct vm *vm)
{
	KERN_ASSERT(dev != NULL && vm != NULL);

	vmm_iodev_register_write(vm, dev,
				 GUEST_DEBUG_IOPORT, SZ8, _debug_ioport_write);
	vmm_iodev_register_read(vm, dev,
				GUEST_DEBUG_IOPORT, SZ8, _debug_ioport_read);
}

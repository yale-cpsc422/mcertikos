#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pci.h>

static void
_vpci_ioport_read(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	KERN_ASSERT(port >= PCI_CMD_PORT && port < PCI_DATA_PORT+4);
	KERN_ASSERT(data != NULL);

	*(uint32_t *) data = 0xffffffff;
}

static void
_vpci_ioport_write(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	KERN_ASSERT(port >= PCI_CMD_PORT && port < PCI_DATA_PORT+4);
	KERN_ASSERT(data != NULL);
}

void vpci_init(struct vpci *vpci, struct vm *vm)
{
	KERN_ASSERT(vpci != NULL && vm != NULL);

	uint32_t port;

	for (port = PCI_CMD_PORT; port < PCI_DATA_PORT+4; port++) {
		vmm_iodev_register_read(vm, vpci,
					port, SZ8, _vpci_ioport_read);
		vmm_iodev_register_write(vm, vpci,
					 port, SZ8, _vpci_ioport_write);

		if (PCI_DATA_PORT+4-port > 1) {
			vmm_iodev_register_read(vm, vpci,
						port, SZ16, _vpci_ioport_read);
			vmm_iodev_register_write(vm, vpci,
						 port, SZ16, _vpci_ioport_write);
		}

		if (PCI_DATA_PORT+4-port > 2) {
			vmm_iodev_register_read(vm, vpci,
						port, SZ32, _vpci_ioport_read);
			vmm_iodev_register_write(vm, vpci,
						 port, SZ32, _vpci_ioport_write);
		}
	}
}

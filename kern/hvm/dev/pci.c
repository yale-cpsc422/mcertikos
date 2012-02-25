#include <kern/debug/debug.h>
#include <architecture/types.h>

#include <kern/hvm/vmm.h>
#include <kern/hvm/vmm_iodev.h>
#include <kern/hvm/dev/pci.h>

static uint32_t
vpci_ioport_read(uint32_t port)
{
	assert(port == PCI_CMD_PORT || port == PCI_DATA_PORT);
	return 0xffffffff;
}

static void
vpci_ioport_write(uint32_t port, uint32_t data)
{
	assert(port == PCI_CMD_PORT || port == PCI_DATA_PORT);
}

static void
_vpci_ioport_read(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	assert(port == PCI_CMD_PORT || port == PCI_DATA_PORT);
	assert(vm != NULL && vpci != NULL && data != NULL);

	*(uint32_t *) data = vpci_ioport_read(port);
}

static void
_vpci_ioport_write(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	assert(port == PCI_CMD_PORT || port == PCI_DATA_PORT);
	assert(vm != NULL && vpci != NULL && data != NULL);

	vpci_ioport_write(port, *(uint32_t *) data);
}

void
vpci_init(struct vpci *vpci)
{
	assert(vpci != NULL);

	vmm_iodev_register_read(vpci,
				PCI_CMD_PORT, SZ32, _vpci_ioport_read);
	vmm_iodev_register_write(vpci,
				 PCI_CMD_PORT, SZ32, _vpci_ioport_write);
	vmm_iodev_register_read(vpci,
				PCI_DATA_PORT, SZ32, _vpci_ioport_read);
	vmm_iodev_register_write(vpci,
				 PCI_DATA_PORT, SZ32, _vpci_ioport_write);
}

#include <architecture/types.h>

#include <kern/debug/debug.h>
#include <kern/hvm/ioport.h>
#include <kern/hvm/dev/pci.h>

static uint32_t
vpci_ioport_read(void *opaque, uint32_t port)
{
	assert(port == PCI_CMD_PORT || port == PCI_DATA_PORT);
	return 0xffffffff;
}

static void
vpci_ioport_write(void *opaque, uint32_t port, uint32_t data)
{
	assert(port == PCI_CMD_PORT || port == PCI_DATA_PORT);
}

void
vpci_init(void)
{
	register_ioport_read(PCI_CMD_PORT, 1, 4, vpci_ioport_read, NULL);
	register_ioport_read(PCI_DATA_PORT, 1, 4, vpci_ioport_read, NULL);
	register_ioport_write(PCI_CMD_PORT, 1, 4, vpci_ioport_write, NULL);
	register_ioport_write(PCI_DATA_PORT, 1, 4, vpci_ioport_write, NULL);
}

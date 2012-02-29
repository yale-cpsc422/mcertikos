#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_iodev.h>
#include <sys/virt/dev/pci.h>

static void _vpci_read_cmd_port(struct vm *, void *, uint32_t, void *);
static void _vpci_write_cmd_port(struct vm *, void *, uint32_t, void *);
static void _vpci_read_data_port(struct vm *, void *, uint32_t, void *);
static void _vpci_write_data_port(struct vm *, void *, uint32_t, void *);

void vpci_init(struct vpci *vpci)
{
	KERN_ASSERT(vpci != NULL);

	vmm_iodev_register_read(vpci,
				PCI_CMD_PORT, SZ32, _vpci_read_cmd_port);
	vmm_iodev_register_write(vpci,
				 PCI_CMD_PORT, SZ32, _vpci_write_cmd_port);
	vmm_iodev_register_read(vpci,
				PCI_DATA_PORT, SZ32, _vpci_read_data_port);
	vmm_iodev_register_write(vpci,
				 PCI_DATA_PORT, SZ32, _vpci_write_data_port);
}

static uint32_t
vpci_read_cmd_port(void)
{
	return 0xffffffff;
}

static void
vpci_write_cmd_port(uint32_t data)
{
}

static uint32_t
vpci_read_data_port(void)
{
	return 0xffffffff;
}

static void
vpci_write_data_port(uint32_t data)
{
}

static void
_vpci_read_cmd_port(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	KERN_ASSERT(port == PCI_CMD_PORT);
	KERN_ASSERT(data != NULL);

	*(uint32_t *) data = vpci_read_cmd_port();
}

static void
_vpci_write_cmd_port(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	KERN_ASSERT(port == PCI_CMD_PORT);
	KERN_ASSERT(data != NULL);

	vpci_write_cmd_port(*(uint32_t *) data);
}

static void
_vpci_read_data_port(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	KERN_ASSERT(port == PCI_DATA_PORT);
	KERN_ASSERT(data != NULL);

	*(uint32_t *) data = vpci_read_data_port();
}

static void
_vpci_write_data_port(struct vm *vm, void *vpci, uint32_t port, void *data)
{
	KERN_ASSERT(port == PCI_DATA_PORT);
	KERN_ASSERT(data != NULL);

	vpci_write_data_port(*(uint32_t *) data);
}

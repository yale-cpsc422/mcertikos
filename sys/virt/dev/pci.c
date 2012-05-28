#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/stdarg.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pci.h>

#ifdef DEBUG_VPCI

#define VPCI_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG("VPCI: ");		\
		dprintf(fmt);			\
	}

#else

#define VPCI_DEBUG(fmt...)			\
	{					\
	}

#endif

static struct vpci_device *
vpci_find_device(struct vpci_host *vpci_host, uint32_t addr)
{
	KERN_ASSERT(vpci_host != NULL);

	uint8_t bus_id, dev_id;

	if ((bus_id = (addr >> 16) & 0xff) != vpci_host->bus_id)
		return NULL;

	dev_id = (addr >> 11) & 0x1f;

	return vpci_host->dev[dev_id];
}

static gcc_inline uint32_t
vpci_read_config_addr(struct vpci_host *vpci_host)
{
	KERN_ASSERT(vpci_host != NULL);
	return vpci_host->config_addr;
}

static gcc_inline void
vpci_write_config_addr(struct vpci_host *vpci_host, uint32_t val)
{
	KERN_ASSERT(vpci_host != NULL);
	vpci_host->config_addr = val;
}

static uint32_t
vpci_read_config_data(struct vpci_host *vpci_host, data_sz_t size)
{
	KERN_ASSERT(vpci_host != NULL);

	struct vpci_device *dev;

	if ((dev = vpci_find_device(vpci_host, vpci_host->config_addr)) == NULL)
		return 0xffffffff;

	return dev->conf_read(dev->dev, vpci_host->config_addr, size);
}

static void
vpci_write_config_data(struct vpci_host *vpci_host, uint32_t val, data_sz_t size)
{
	KERN_ASSERT(vpci_host != NULL);

	struct vpci_device *dev;

	if ((dev = vpci_find_device(vpci_host, vpci_host->config_addr)) == NULL)
		return;

	dev->conf_write(dev->dev, vpci_host->config_addr, val, size);
}

static void
vpci_ioport_readb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+4);

	uint32_t val;
	struct vpci_host *vpci_host = (struct vpci_host *) dev;

	if (port < PCI_CONFIG_DATA) {
		val = vpci_read_config_addr(vpci_host);
		*(uint8_t *) data = val >> ((port - PCI_CONFIG_ADDR) * 8);
	} else {
		val = vpci_read_config_data(vpci_host, SZ8);
		*(uint8_t *) data = val >> ((port - PCI_CONFIG_DATA) * 8);
	}

	VPCI_DEBUG("readb port %x, val %08x.\n", port, val);
}

static void
vpci_ioport_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+3);

	VPCI_DEBUG("readw port %x.\n", port);

	uint32_t val;
	struct vpci_host *vpci_host = (struct vpci_host *) dev;

	if (port < PCI_CONFIG_DATA - 1) {
		val = vpci_read_config_addr(vpci_host);
		*(uint16_t *) data = val >> ((port - PCI_CONFIG_ADDR) * 8);
	} else if (port >= PCI_CONFIG_DATA) {
		val = vpci_read_config_data(vpci_host, SZ16);
		*(uint16_t *) data = val >> ((port - PCI_CONFIG_DATA) * 8);
	} else {
		uint16_t _addr, _data;
		_addr = vpci_read_config_addr(vpci_host) >> 24;
		_data = vpci_read_config_data(vpci_host, SZ8) << 8;
		val = _data | _addr;
		*(uint16_t *) data = val;
	}

	VPCI_DEBUG("readw port %x, val %08x.\n", port, val);
}

static void
vpci_ioport_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port <= PCI_CONFIG_DATA);

	uint32_t val;
	struct vpci_host *vpci_host = (struct vpci_host *) dev;

	if (port == PCI_CONFIG_ADDR) {
		val = vpci_read_config_addr(vpci_host);
		*(uint32_t *) data = val;
	} else if (port == PCI_CONFIG_DATA) {
		val = vpci_read_config_data(vpci_host, SZ32);
		*(uint32_t *) data = val;
	} else {
		uint32_t _addr, _data;
		int addr_bytes;
		addr_bytes = PCI_CONFIG_DATA - port;
		_addr = vpci_read_config_addr(vpci_host) >> (4 - addr_bytes);
		_data = vpci_read_config_data(vpci_host, SZ32) << addr_bytes;
		val = _data | _addr;
		*(uint32_t *) data = val;
	}

	VPCI_DEBUG("readl port %x, val %08x.\n", port, val);
}

static void
vpci_ioport_writeb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+4);

	uint32_t val;
	struct vpci_host *vpci_host = (struct vpci_host *) dev;

	if (port == PCI_CONFIG_ADDR) {
		val = vpci_read_config_addr(vpci_host) & ~0xff;
		val |= *(uint8_t *) data;
		vpci_write_config_addr(vpci_host, val);
	} else if (port == PCI_CONFIG_DATA) {
		val = *(uint8_t *) data;
		vpci_write_config_data(vpci_host, val, SZ8);
	} else {
		VPCI_DEBUG("writeb, invalid port %x.\n", port);
		return;
	}

	VPCI_DEBUG("writeb port %x, val %08x.\n", port, val);
}

static void
vpci_ioport_writew(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+3);

	uint32_t val;
	struct vpci_host *vpci_host = (struct vpci_host *) dev;

	if (port == PCI_CONFIG_ADDR) {
		val = vpci_read_config_addr(vpci_host) & ~0xffff;
		val |= *(uint16_t *) data;
		vpci_write_config_addr(vpci_host, val);
	} else if (port == PCI_CONFIG_DATA) {
		val = *(uint16_t *) data;
		vpci_write_config_data(vpci_host, val, SZ16);
	} else {
		VPCI_DEBUG("writew, invalid port %x.\n", port);
		return;
	}

	VPCI_DEBUG("writew port %x, val %08x.\n", port, val);
}

static void
vpci_ioport_writel(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port <= PCI_CONFIG_DATA);

	uint32_t val;
	struct vpci_host *vpci_host = (struct vpci_host *) dev;

	if (port == PCI_CONFIG_ADDR) {
		val = *(uint32_t *) data;
		vpci_write_config_addr(vpci_host, val);
	} else if (port == PCI_CONFIG_DATA) {
		val = *(uint32_t *) data;
		vpci_write_config_data(vpci_host, val, SZ32);
	} else {
		VPCI_DEBUG("writel, invalid port %x.\n", port);
		return;
	}

	VPCI_DEBUG("writel port %x, val %08x.\n", port, val);
}

void
vpci_init(struct vpci_host *vpci_host, struct vm *vm)
{
	KERN_ASSERT(vpci_host != NULL && vm != NULL);

	uint16_t port;

	memset(vpci_host, 0x0, sizeof(struct vpci_host));

	for (port = PCI_CONFIG_ADDR; port < PCI_CONFIG_DATA + 4; port++) {
		vmm_iodev_register_read(vm, vpci_host, port, SZ8,
					vpci_ioport_readb);
		vmm_iodev_register_write(vm, vpci_host, port, SZ8,
					 vpci_ioport_writeb);

		if (PCI_CONFIG_DATA + 4 - port > 1) {
			vmm_iodev_register_read(vm, vpci_host, port, SZ16,
						vpci_ioport_readw);
			vmm_iodev_register_write(vm, vpci_host, port, SZ16,
						 vpci_ioport_writew);
		}

		if (PCI_CONFIG_DATA + 4 - port >= 4) {
			vmm_iodev_register_read(vm, vpci_host, port, SZ32,
						vpci_ioport_readl);
			vmm_iodev_register_write(vm, vpci_host, port, SZ32,
						 vpci_ioport_writel);
		}
	}
}

int
vpci_attach_device(struct vpci_host *vpci_host, struct vpci_device *vpci_device)
{
	KERN_ASSERT(vpci_host != NULL && vpci_device != NULL);

	int i;
	struct vpci_device **free_slot;

	for (i = 0; i < 32; i++) {
		free_slot = &vpci_host->dev[i];
		if (*free_slot == NULL)
			break;
	}

	if (i == 32) {
		VPCI_DEBUG("Cannot find a free PCI slot.\n");
		return 1;
	}

	*free_slot = vpci_device;

	VPCI_DEBUG("Attach a device, bus id %d, dev id %d.\n",
		   vpci_host->bus_id, i);

	return 0;
}

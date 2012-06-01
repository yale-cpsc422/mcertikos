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

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (bus_id == 0 && dev_id == 0)
#endif

	VPCI_DEBUG("bus %02x, dev %02x, func %1x, reg %02x.\n",
		   bus_id, dev_id, (addr >> 8) & 0x7, addr & 0xff);

	return vpci_host->dev[dev_id];
}

static void
vpci_config_addr_readb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+4);

	struct vpci_host *host;
	int shift;
	uint32_t mask;

	host = (struct vpci_host *) dev;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xff << shift;

	*(uint8_t *) data = (host->config_addr & mask) >> shift;

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("readb port %x (PCI_CONFIG_ADDR), val %02x.\n",
		   port, *(uint8_t *) data);
}

static void
vpci_config_addr_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+3);

	struct vpci_host *host;
	int shift;
	uint32_t mask;

	host = (struct vpci_host *) dev;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xffff << shift;

	*(uint16_t *) data = (host->config_addr & mask) >> shift;

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("readw port %x (PCI_CONFIG_ADDR), val %04x.\n",
		   port, *(uint16_t *) data);
}

static void
vpci_config_addr_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(port == PCI_CONFIG_ADDR);

	struct vpci_host *host;

	host = (struct vpci_host *) dev;

	*(uint32_t *) data = host->config_addr;

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("readl port %x (PCI_CONFIG_ADDR), val %04x.\n",
		   port, *(uint32_t *) data);
}

static void
vpci_config_addr_writeb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+4);

	struct vpci_host *host;
	int shift;
	uint32_t mask;

	host = (struct vpci_host *) dev;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xff << shift;

	host->config_addr =
		(host->config_addr & ~mask) | (*(uint8_t *) data << shift);

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("writeb port %x (PCI_CONFIG_ADDR), val %08x.\n",
		   port, host->config_addr);
}

static void
vpci_config_addr_writew(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+3);

	struct vpci_host *host;
	int shift;
	uint32_t mask;

	host = (struct vpci_host *) dev;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xffff << shift;

	host->config_addr =
		(host->config_addr & ~mask) | (*(uint16_t *) data << shift);

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("writew port %x (PCI_CONFIG_ADDR), val %08x.\n",
		   port, host->config_addr);
}

static void
vpci_config_addr_writel(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(port == PCI_CONFIG_ADDR);

	struct vpci_host *host;

	host = (struct vpci_host *) dev;
	host->config_addr = *(uint32_t *) data;

#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("writew port %x (PCI_CONFIG_ADDR), val %08x.\n",
		   port, host->config_addr);
}

static void
vpci_config_data_readb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+4);

	struct vpci_host *host;
	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask;

	host = (struct vpci_host *) dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL) {
		*(uint8_t *) data = 0xff;
		goto ret;
	}

	shift = (port - PCI_CONFIG_DATA) * 8;
	mask = (uint32_t) 0xff << shift;

	*(uint8_t *) data =
		(pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ8) &
		 mask) >> shift;

 ret:
#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("readb port %x (PCI_CONFIG_DATA), val %02x.\n",
		   port, *(uint8_t *) data);
}

static void
vpci_config_data_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+3);

	struct vpci_host *host;
	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask;

	host = (struct vpci_host *) dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL) {
		*(uint16_t *) data = 0xffff;
		goto ret;
	}

	shift = (port - PCI_CONFIG_DATA) * 8;
	mask = (uint32_t) 0xffff << shift;

	*(uint16_t *) data =
		(pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ16) &
		 mask) >> shift;

 ret:
#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("readw port %x (PCI_CONFIG_DATA), val %04x.\n",
		   port, *(uint16_t *) data);
}

static void
vpci_config_data_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(port == PCI_CONFIG_DATA);

	struct vpci_host *host;
	struct vpci_device *pci_dev;

	host = (struct vpci_host *) dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL) {
		*(uint32_t *) data = 0xffffffff;
		goto ret;
	}

	*(uint32_t *) data =
		pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32);

 ret:
#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("readl port %x (PCI_CONFIG_DATA), val %08x.\n",
		   port, *(uint32_t *) data);
}

static void
vpci_config_data_writeb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+4);

	struct vpci_host *host;
	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask, val;

	host = (struct vpci_host *) dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL)
		goto ret;

	shift = (port - PCI_CONFIG_DATA) * 8;

	if (shift == 0) {
		pci_dev->conf_write(pci_dev->dev, host->config_addr,
				    *(uint8_t *) data, SZ8);
		goto ret;
	}

	mask = (uint32_t) 0xff << shift;

	val = pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32);
	val = (val & ~mask) | ((uint32_t) (*(uint8_t *) data) << shift);

	pci_dev->conf_write(pci_dev->dev, host->config_addr, val, SZ32);

 ret:
#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("writeb port %x (PCI_CONFIG_DATA), val %02x.\n",
		   port, *(uint8_t *) data);
}

static void
vpci_config_data_writew(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+3);

	struct vpci_host *host;
	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask, val;

	host = (struct vpci_host *) dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL)
		goto ret;

	shift = (port - PCI_CONFIG_DATA) * 8;

	if (shift == 0) {
		pci_dev->conf_write(pci_dev->dev, host->config_addr,
				    *(uint16_t *) data, SZ16);
		goto ret;
	}

	mask = (uint32_t) 0xffff << shift;

	val = pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32);
	val = (val & ~mask) | ((uint32_t) (*(uint16_t *) data) << shift);

	pci_dev->conf_write(pci_dev->dev, host->config_addr, val, SZ32);

 ret:
#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("writew port %x (PCI_CONFIG_DATA), val %04x.\n",
		   port, *(uint16_t *) data);
}

static void
vpci_config_data_writel(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);
	KERN_ASSERT(port == PCI_CONFIG_DATA);

	struct vpci_host *host;
	struct vpci_device *pci_dev;

	host = (struct vpci_host *) dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL)
		goto ret;

	pci_dev->conf_write(pci_dev->dev,
			    host->config_addr, *(uint32_t *) data, SZ32);

 ret:
#if defined (DEBUG_VPCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

	VPCI_DEBUG("writel port %x (PCI_CONFIG_DATA), val %08x.\n",
		   port, *(uint32_t *) data);
}

static void
vpci_default_ioport_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	VPCI_DEBUG("readw port %x, invalid.\n", port);
}

static void
vpci_default_ioport_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	VPCI_DEBUG("readl port %x, invalid.\n", port);
}

static void
vpci_default_ioport_writew(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	VPCI_DEBUG("writew port %x, val %04x, invalid.\n",
		   port, *(uint16_t *) data);
}

static void
vpci_default_ioport_writel(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	VPCI_DEBUG("writel port %x, val %08x, invalid.\n",
		   port, *(uint32_t *) data);
}

void
vpci_init(struct vpci_host *vpci_host, struct vm *vm)
{
	KERN_ASSERT(vpci_host != NULL && vm != NULL);

	uint32_t port;

	memset(vpci_host, 0x0, sizeof(struct vpci_host));

	for (port = PCI_CONFIG_ADDR; port < PCI_CONFIG_ADDR+4; port++) {
		vmm_iodev_register_read(vm, vpci_host, port, SZ8,
					vpci_config_addr_readb);
		vmm_iodev_register_write(vm, vpci_host, port, SZ8,
					 vpci_config_addr_writeb);

		if (PCI_CONFIG_ADDR + 4 - port > 1) {
			vmm_iodev_register_read(vm, vpci_host, port, SZ16,
						vpci_config_addr_readw);
			vmm_iodev_register_write(vm, vpci_host, port, SZ16,
						 vpci_config_addr_writew);
		} else {
			vmm_iodev_register_read(vm, vpci_host, port, SZ16,
						vpci_default_ioport_readw);
			vmm_iodev_register_write(vm, vpci_host, port, SZ16,
						 vpci_default_ioport_writew);
		}

		if (port == PCI_CONFIG_ADDR) {
			vmm_iodev_register_read(vm, vpci_host, port, SZ32,
						vpci_config_addr_readl);
			vmm_iodev_register_write(vm, vpci_host, port, SZ32,
						 vpci_config_addr_writel);
		} else {
			vmm_iodev_register_read(vm, vpci_host, port, SZ32,
						vpci_default_ioport_readl);
			vmm_iodev_register_write(vm, vpci_host, port, SZ32,
						 vpci_default_ioport_writel);
		}
	}

	for (port = PCI_CONFIG_DATA; port < PCI_CONFIG_DATA+4; port++) {
		vmm_iodev_register_read(vm, vpci_host, port, SZ8,
					vpci_config_data_readb);
		vmm_iodev_register_write(vm, vpci_host, port, SZ8,
					 vpci_config_data_writeb);

		if (PCI_CONFIG_DATA + 4 - port > 1) {
			vmm_iodev_register_read(vm, vpci_host, port, SZ16,
						vpci_config_data_readw);
			vmm_iodev_register_write(vm, vpci_host, port, SZ16,
						 vpci_config_data_writew);
		} else {
			vmm_iodev_register_read(vm, vpci_host, port, SZ16,
						vpci_default_ioport_readw);
			vmm_iodev_register_write(vm, vpci_host, port, SZ16,
						 vpci_default_ioport_writew);
		}

		if (port == PCI_CONFIG_DATA) {
			vmm_iodev_register_read(vm, vpci_host, port, SZ32,
						vpci_config_data_readl);
			vmm_iodev_register_write(vm, vpci_host, port, SZ32,
						 vpci_config_data_writel);
		} else {
			vmm_iodev_register_read(vm, vpci_host, port, SZ32,
						vpci_default_ioport_readl);
			vmm_iodev_register_write(vm, vpci_host, port, SZ32,
						 vpci_default_ioport_writel);
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

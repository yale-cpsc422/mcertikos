#include <debug.h>
#include <gcc.h>
#include <string.h>
#include <syscall.h>
#include <types.h>

#include "pci.h"

#ifdef DEBUG_VPCI

#define VPCI_DEBUG(fmt, ...)				\
	{						\
		DEBUG("VPCI: "fmt, ##__VA_ARGS__);	\
	}

#else

#define VPCI_DEBUG(fmt...)			\
	{					\
	}

#endif

static struct vpci_device *
vpci_find_device(struct vpci_host *vpci_host, uint32_t addr)
{
	ASSERT(vpci_host != NULL);

	uint8_t bus_id, dev_id;

	if ((bus_id = (addr >> 16) & 0xff) != vpci_host->bus_id)
		return NULL;

	dev_id = (addr >> 11) & 0x1f;

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (bus_id == 0 && dev_id == 0)
#endif

		VPCI_DEBUG("bus %02x, dev %02x, func %1x, reg %02x.\n",
			   bus_id, dev_id, (addr >> 8) & 0x7, addr & 0xff);

	return vpci_host->dev[dev_id];
}

static void
vpci_config_addr_readb(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+4);

	int shift;
	uint32_t mask;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xff << shift;

	*(uint8_t *) data = (host->config_addr & mask) >> shift;

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("readb port %x (PCI_CONFIG_ADDR), val %02x.\n",
			   port, *(uint8_t *) data);
}

static void
vpci_config_addr_readw(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+3);

	int shift;
	uint32_t mask;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xffff << shift;

	*(uint16_t *) data = (host->config_addr & mask) >> shift;

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("readw port %x (PCI_CONFIG_ADDR), val %04x.\n",
			   port, *(uint16_t *) data);
}

static void
vpci_config_addr_readl(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(port == PCI_CONFIG_ADDR);

	*(uint32_t *) data = host->config_addr;

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("readl port %x (PCI_CONFIG_ADDR), val %04x.\n",
			   port, *(uint32_t *) data);
}

static void
vpci_config_addr_writeb(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+4);

	int shift;
	uint32_t mask;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xff << shift;

	host->config_addr =
		(host->config_addr & ~mask) | (*(uint8_t *) data << shift);

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("writeb port %x (PCI_CONFIG_ADDR), val %08x.\n",
			   port, host->config_addr);
}

static void
vpci_config_addr_writew(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_ADDR+3);

	int shift;
	uint32_t mask;

	shift = (port - PCI_CONFIG_ADDR) * 8;
	mask = (uint32_t) 0xffff << shift;

	host->config_addr =
		(host->config_addr & ~mask) | (*(uint16_t *) data << shift);

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("writew port %x (PCI_CONFIG_ADDR), val %08x.\n",
			   port, host->config_addr);
}

static void
vpci_config_addr_writel(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(port == PCI_CONFIG_ADDR);

	host->config_addr = *(uint32_t *) data;

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("writew port %x (PCI_CONFIG_ADDR), val %08x.\n",
			   port, host->config_addr);
}

static void
vpci_config_data_readb(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+4);

	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL) {
		*(uint8_t *) data = 0xff;
		goto ret;
	}

	shift = (port - PCI_CONFIG_DATA) * 8;
	mask = (uint32_t) 0xff << shift;

	*(uint8_t *) data =
		(pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32) &
		 mask) >> shift;

 ret:
#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("readb port %x (PCI_CONFIG_DATA), val %02x.\n",
			   port, *(uint8_t *) data);
}

static void
vpci_config_data_readw(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+3);

	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL) {
		*(uint16_t *) data = 0xffff;
		goto ret;
	}

	shift = (port - PCI_CONFIG_DATA) * 8;
	mask = (uint32_t) 0xffff << shift;

	*(uint16_t *) data =
		(pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32) &
		 mask) >> shift;

 ret:
#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("readw port %x (PCI_CONFIG_DATA), val %04x.\n",
			   port, *(uint16_t *) data);
}

static void
vpci_config_data_readl(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(port == PCI_CONFIG_DATA);

	struct vpci_device *pci_dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL) {
		*(uint32_t *) data = 0xffffffff;
		goto ret;
	}

	*(uint32_t *) data =
		pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32);

 ret:
#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("readl port %x (PCI_CONFIG_DATA), val %08x.\n",
			   port, *(uint32_t *) data);
}

static void
vpci_config_data_writeb(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+4);

	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask, val;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL)
		return;

	shift = (port - PCI_CONFIG_DATA) * 8;

	if (shift == 0) {
#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
		if (((host->config_addr >> 16) & 0xff) == 0 &&
		    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

			VPCI_DEBUG("writeb port %x (PCI_CONFIG_DATA), val %02x.\n",
				   port, *(uint8_t *) data);
		pci_dev->conf_write(pci_dev->dev, host->config_addr,
				    *(uint8_t *) data, SZ8);
		return;
	}

	mask = (uint32_t) 0xff << shift;

	val = pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32);
	val = (val & ~mask) | ((uint32_t) (*(uint8_t *) data) << shift);

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("writeb port %x (PCI_CONFIG_DATA), val %02x.\n",
			   port, *(uint8_t *) data);

	pci_dev->conf_write(pci_dev->dev, host->config_addr, val, SZ32);
}

static void
vpci_config_data_writew(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+3);

	struct vpci_device *pci_dev;
	int shift;
	uint32_t mask, val;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL)
		return;

	shift = (port - PCI_CONFIG_DATA) * 8;

	if (shift == 0) {
#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
		if (((host->config_addr >> 16) & 0xff) == 0 &&
		    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

			VPCI_DEBUG("writew port %x (PCI_CONFIG_DATA), val %04x.\n",
				   port, *(uint16_t *) data);
		pci_dev->conf_write(pci_dev->dev, host->config_addr,
				    *(uint16_t *) data, SZ16);
		return;
	}

	mask = (uint32_t) 0xffff << shift;

	val = pci_dev->conf_read(pci_dev->dev, host->config_addr, SZ32);
	val = (val & ~mask) | ((uint32_t) (*(uint16_t *) data) << shift);

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("writew port %x (PCI_CONFIG_DATA), val %04x.\n",
			   port, *(uint16_t *) data);

	pci_dev->conf_write(pci_dev->dev, host->config_addr, val, SZ32);
}

static void
vpci_config_data_writel(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);
	ASSERT(port == PCI_CONFIG_DATA);

	struct vpci_device *pci_dev;

	if ((pci_dev = vpci_find_device(host, host->config_addr)) == NULL)
		return;

#if defined (DEBUG_PCI) && defined (DEBUG_VIRTIO_BLK)
	if (((host->config_addr >> 16) & 0xff) == 0 &&
	    ((host->config_addr >> 11) & 0x1f) == 0)
#endif

		VPCI_DEBUG("writel port %x (PCI_CONFIG_DATA), val %08x.\n",
			   port, *(uint32_t *) data);

	pci_dev->conf_write(pci_dev->dev,
			    host->config_addr, *(uint32_t *) data, SZ32);
}

static void
vpci_default_ioport_readw(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);

	VPCI_DEBUG("readw port %x, invalid.\n", port);
}

static void
vpci_default_ioport_readl(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);

	VPCI_DEBUG("readl port %x, invalid.\n", port);
}

static void
vpci_default_ioport_writew(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);

	VPCI_DEBUG("writew port %x, val %04x, invalid.\n",
		   port, *(uint16_t *) data);
}

static void
vpci_default_ioport_writel(struct vpci_host *host, uint16_t port, void *data)
{
	ASSERT(host != NULL && data != NULL);

	VPCI_DEBUG("writel port %x, val %08x, invalid.\n",
		   port, *(uint32_t *) data);
}

static void
_vpci_ioport_read(struct vpci_host *pci,
		  uint16_t port, data_sz_t size, void *data)
{
	ASSERT(pci != NULL);
	ASSERT(data != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+4);

	switch (size) {
	case SZ8:
		if (port < PCI_CONFIG_DATA)
			vpci_config_addr_readb(pci, port, data);
		else
			vpci_config_data_readb(pci, port, data);
		break;
	case SZ16:
		if (port < PCI_CONFIG_DATA-1)
			vpci_config_addr_readw(pci, port, data);
		else if (PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+3)
			vpci_config_data_readw(pci, port, data);
		else
			vpci_default_ioport_readw(pci, port, data);
		break;
	case SZ32:
		if (port == PCI_CONFIG_ADDR)
			vpci_config_addr_readl(pci, port, data);
		else if (port == PCI_CONFIG_DATA)
			vpci_config_data_readl(pci, port, data);
		else
			vpci_default_ioport_readl(pci, port, data);
		break;
	default:
		break;
	}
}

static void
_vpci_ioport_write(struct vpci_host *pci,
		   uint16_t port, data_sz_t size, void *data)
{
	ASSERT(pci != NULL);
	ASSERT(data != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+4);

	switch (size) {
	case SZ8:
		if (port < PCI_CONFIG_DATA)
			vpci_config_addr_writeb(pci, port, data);
		else
			vpci_config_data_writeb(pci, port, data);
		break;
	case SZ16:
		if (port < PCI_CONFIG_DATA-1)
			vpci_config_addr_writew(pci, port, data);
		else if (PCI_CONFIG_DATA <= port && port < PCI_CONFIG_DATA+3)
			vpci_config_data_writew(pci, port, data);
		else
			vpci_default_ioport_writew(pci, port, data);
		break;
	case SZ32:
		if (port == PCI_CONFIG_ADDR)
			vpci_config_addr_writel(pci, port, data);
		else if (port == PCI_CONFIG_DATA)
			vpci_config_data_writel(pci, port, data);
		else
			vpci_default_ioport_writel(pci, port, data);
		break;
	default:
		break;
	}
}

void
vpci_init(struct vpci_host *vpci_host)
{
	ASSERT(vpci_host != NULL);

	uint16_t port;

	memset(vpci_host, 0x0, sizeof(struct vpci_host));

	for (port = PCI_CONFIG_ADDR; port < PCI_CONFIG_ADDR+4; port++) {
		sys_register_ioport(port, SZ8, 0);
		sys_register_ioport(port, SZ8, 1);

		if (PCI_CONFIG_ADDR + 4 - port > 1) {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		} else {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		}

		if (port == PCI_CONFIG_ADDR) {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		} else {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		}
	}

	for (port = PCI_CONFIG_DATA; port < PCI_CONFIG_DATA+4; port++) {
		sys_register_ioport(port, SZ8, 0);
		sys_register_ioport(port, SZ8, 1);

		if (PCI_CONFIG_DATA + 4 - port > 1) {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		} else {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		}

		if (port == PCI_CONFIG_DATA) {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		} else {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		}
	}
}

void
vpci_handle_ioport(struct vpci_host *vpci_host, int channel_id,
		   uint16_t port, data_sz_t width, void *data, int write)
{
	ASSERT(vpci_host != NULL);
	ASSERT(PCI_CONFIG_ADDR <= port && port < PCI_CONFIG_DATA+4);
	ASSERT(data != NULL);

	struct ioport_read_ret ret;

	if (write) {
		_vpci_ioport_write(vpci_host, port, width, data);
	} else {
		_vpci_ioport_read(vpci_host, port, width, &ret.data);
		ret.magic = MAGIC_IOPORT_READ_RET;
		sys_send(channel_id, ret, sizeof(struct ioport_read_ret));
	}
}

int
vpci_attach_device(struct vpci_host *vpci_host, struct vpci_device *vpci_device)
{
	ASSERT(vpci_host != NULL && vpci_device != NULL);

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

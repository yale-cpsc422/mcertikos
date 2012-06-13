#include <sys/debug.h>
#include <sys/stdarg.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pci.h>
#include <sys/virt/dev/virtio.h>
#include <sys/virt/dev/virtio_blk.h>

#include <dev/ahci.h>
#include <dev/pci.h>
#include <dev/tsc.h>

#include <machine/trap.h>

#ifdef DEBUG_VIRTIO_BLK

#define virtio_blk_debug(fmt...)		\
	{					\
		KERN_DEBUG("VIRTIO-BLK: ");	\
		dprintf(fmt);			\
	}

#else

#define virtio_blk_debug(fmt...)		\
	{					\
	}

#endif

static uint16_t unsupported_command =
	0xf880 /* reserved bits */ | PCI_COMMAND_MEM_ENABLE |
	PCI_COMMAND_SPECIAL_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE |
	PCI_COMMAND_PALETTE_ENABLE | PCI_COMMAND_PARITY_ENABLE |
	PCI_COMMAND_SERR_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE;

static void
virtio_blk_perform_pci_command(struct virtio_blk *blk)
{
	KERN_ASSERT(blk != NULL);

	uint16_t command = blk->common_header.pci_conf.header.command;

	if (command == 0) {
		virtio_blk_debug("logically disconnected.\n");
		blk->vring.desc_guest_addr = 0;
		blk->vring.avail_guest_addr = 0;
		blk->vring.used_guest_addr = 0;
		blk->vring.last_avail_idx = 0;
		blk->vring.need_notify = FALSE;
	}

	if (command & unsupported_command) {
		virtio_blk_debug("ignore unsupported command %08x.\n",
				 command & unsupported_command);
		command &= ~unsupported_command;
		blk->common_header.pci_conf.header.command = command;
	}

	smp_wmb();
}

static uint32_t
virtio_blk_pci_conf_read(void *opaque, uint32_t addr, data_sz_t size)
{
	KERN_ASSERT(opaque != NULL);

	struct virtio_device *dev;
	int len;
	uint8_t reg, *pci_conf;

	dev = (struct virtio_device *) opaque;

	reg = addr & 0xfc;
	len = sizeof(dev->pci_conf);

	if (reg >= len)
		goto err;

	pci_conf = (uint8_t *) &dev->pci_conf;

	if (size == SZ8) {
		return pci_conf[reg];
	} else if (size == SZ16) {
		uint8_t data[2];
		data[0] = pci_conf[reg];
		data[1] = (reg >= len - 1) ? 0x00 : pci_conf[reg+1];
		return ((uint16_t) data[1] << 8) | data[0];
	} else if (size == SZ32) {
		uint8_t data[4];
		data[0] = pci_conf[reg];
		data[1] = (reg >= len - 1) ? 0x00 : pci_conf[reg+1];
		data[2] = (reg >= len - 2) ? 0x00 : pci_conf[reg+2];
		data[3] = (reg >= len - 3) ? 0x00 : pci_conf[reg+3];
		return ((uint32_t) data[3] << 24) | ((uint32_t) data[2] << 16) |
			((uint32_t) data[1] << 8) | data[0];
	}

 err:
	return ((size == SZ8) ? 0x000000ff :
		(size == SZ16) ? 0x0000ffff : 0xffffffff);
}

static void
virtio_blk_pci_conf_write(void *opaque,
			  uint32_t addr, uint32_t val, data_sz_t size)
{
	KERN_ASSERT(opaque != NULL);

	struct virtio_device *dev;
	int len;
	uint8_t reg, *pci_conf;

	dev = (struct virtio_device *) opaque;

	reg = addr & 0xfc;
	len = sizeof(dev->pci_conf);

	/* invaild/read-only area */
	if (reg >= len || reg == PCI_ID_REG || reg == PCI_CLASS_REG ||
	    (PCI_MAPREG_START+4 <= reg && reg < PCI_MAPREG_END) ||
	    (0x2c <= reg && reg <= 0x3c))
		return;

	pci_conf = (uint8_t *) &dev->pci_conf;

	pci_conf[reg] = (uint8_t) val;
	if (size <= SZ32 && reg < len - 1)
		pci_conf[reg+1] = (uint8_t) (val >> 8);
	if (size == SZ32) {
		if (reg < len - 2)
			pci_conf[reg+2] = (uint8_t) (val >> 16);
		if (reg < len - 3)
			pci_conf[reg+3] = (uint8_t) (val >> 24);
	}

	if (reg == PCI_COMMAND_STATUS_REG) {
		virtio_blk_perform_pci_command((struct virtio_blk *) dev);
	} else if (reg == PCI_MAPREG_START) {
		if (dev->pci_conf.bar[0] == 0xffffffff) {
			/* calculate BAR0 size */
			uint16_t size;
			int bytes;
			size = sizeof(struct virtio_header) +
				sizeof(struct virtio_blk_config);
			for (bytes = 0; size > 0; bytes++)
				size >>= 1;
			dev->iosize = (1 << bytes);
			dev->pci_conf.bar[0] ^= (1 << bytes) - 1;
			dev->pci_conf.bar[0] |= 0x1;

			virtio_blk_debug("BAR0=%08x.\n", dev->pci_conf.bar[0]);
		} else if (dev->pci_conf.bar[0] != 0) {
			/* set BAR0 address */
			dev->pci_conf.bar[0] =
				(dev->pci_conf.bar[0] & ~(uint32_t) 0x3) | 0x1;
			virtio_device_update_ioport_handlers
				(dev, (uint16_t) dev->pci_conf.bar[0] & 0xfffc);
		}
	} else if (reg == PCI_INTERRUPT_REG) {
		pci_conf[PCI_INTERRUPT_REG] &= 0x000000ff;
	}

	smp_wmb();
}

static void
virtio_blk_conf_readb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t iobase;
	uint8_t *conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->common_header.iobase + sizeof(struct virtio_header);
	KERN_ASSERT(iobase <= port &&
		    port < iobase + sizeof(struct virtio_blk_config));
	conf = (uint8_t *) &blk->blk_header;

	*(uint8_t *) data = conf[port-iobase];

	virtio_blk_debug("readb blk reg %x, val %02x.\n",
			 port - iobase, *(uint8_t *) data);

	smp_wmb();
}

static void
virtio_blk_conf_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t iobase;
	uint8_t *conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->common_header.iobase + sizeof(struct virtio_header);
	KERN_ASSERT(iobase <= port &&
		    port < iobase + sizeof(struct virtio_blk_config) - 1);
	conf = (uint8_t *) &blk->blk_header;

	*(uint16_t *) data =
		conf[port-iobase] | ((uint16_t) conf[port-iobase+1] << 8);

	virtio_blk_debug("readw blk reg %x, val %04x.\n",
			 port - iobase, *(uint16_t *) data);

	smp_wmb();
}

static void
virtio_blk_conf_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t iobase;
	uint8_t *conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->common_header.iobase + sizeof(struct virtio_header);
	KERN_ASSERT(iobase <= port &&
		    port < iobase + sizeof(struct virtio_blk_config) - 3);
	conf = (uint8_t *) &blk->blk_header;

	*(uint32_t *) data =
		conf[port-iobase] | ((uint32_t) conf[port-iobase+1] << 8) |
		((uint32_t) conf[port-iobase+2] << 16) |
		((uint32_t) conf[port-iobase+3] << 24);

	virtio_blk_debug("readw blk reg %x, val %08x.\n",
			 port - iobase, *(uint32_t *) data);

	smp_wmb();
}

static gcc_inline void
unregister_register_read(struct virtio_device *dev,
			 uint32_t port, data_sz_t size, iodev_read_func_t func)
{
	vmm_iodev_unregister_read(dev->vm, port, size);
	vmm_iodev_register_read(dev->vm, dev, port, size, func);
}

static void
virtio_blk_update_ioport_handlers(struct virtio_device *dev)
{
	KERN_ASSERT(dev != NULL);

	uint16_t base, end, port;

	base = dev->iobase + sizeof(struct virtio_header);
	end = base + sizeof(struct virtio_blk_config);

	for (port = base; port < end; port++) {
		unregister_register_read(dev, port, SZ8,
					 virtio_blk_conf_readb);
		if (port < end - 1)
			unregister_register_read
				(dev, port, SZ16, virtio_blk_conf_readw);
		if (port < end - 3)
			unregister_register_read
				(dev, port, SZ32, virtio_blk_conf_readl);
	}
}

static uint8_t
virtio_blk_vrings_amount(struct virtio_device *dev)
{
	return 1;
}

static struct vring *
virtio_blk_get_vring(struct virtio_device *dev, uint8_t idx)
{
	KERN_ASSERT(dev != NULL);

	if (idx != 0)
		return NULL;
	else
		return &((struct virtio_blk *) dev)->vring;
}

static int
virtio_blk_read_disk(uint64_t lba, uint64_t nsectors, void *buf)
{
	KERN_ASSERT(buf != NULL);

	int ret = ahci_disk_read(0, lba, nsectors, buf);

	virtio_blk_debug("read %s.\n", ret ? "failed" : "OK");

	smp_wmb();

	return ret;
}

static int
virtio_blk_write_disk(uint64_t lba, uint64_t nsectors, void *buf)
{
	KERN_ASSERT(buf != NULL);

	int ret = ahci_disk_write(0, lba, nsectors, buf);

	virtio_blk_debug("write %s.\n", ret ? "failed" : "OK");

	smp_wmb();

	return ret;
}

static void
virtio_blk_get_id(void *buf, uint32_t len)
{
	KERN_ASSERT(buf != NULL);

	char *p;
	uint32_t size;

	if (len == 0)
		return;

	p = (char *) buf;
	size = MIN(VIRTIO_BLK_DEVICE_NAME_LEN, len-1);

	strncpy(p, VIRTIO_BLK_DEVICE_NAME, size);
	p[size] = '\0';

	smp_wmb();
}

static void
virtio_blk_dump_req(struct vm *vm, struct virtio_blk *blk, uint16_t desc_id)
{
#ifdef DEBUG_VIRTIO_BLK
	KERN_ASSERT(vm != NULL && blk != NULL);

	uint16_t cur;
	struct vring_desc *desc;
	struct virtio_blk_outhdr *req;

	dprintf("=== Desc %d ~ Desc %d ===\n", desc_id, desc_id+2);

	cur = desc_id;
	desc = (struct vring_desc *)
		vmm_translate_gp2hp(vm, blk->vring.desc_guest_addr +
				    sizeof(struct vring_desc) * cur);
	dprintf("Desc %d, addr %llx, len %d, flags %04x, next %04x.\n",
		cur, desc->addr, desc->len, desc->flags, desc->next);

	req = (struct virtio_blk_outhdr *) vmm_translate_gp2hp(vm, desc->addr);
	if (req->type == VIRTIO_BLK_T_IN)
		dprintf("  read sector 0x%x", req->sector);
	else if (req->type == VIRTIO_BLK_T_OUT)
		dprintf("  write sector 0x%x", req->sector);
	else if (req->type == VIRTIO_BLK_T_FLUSH)
		dprintf("  flush");
	else if (req->type == VIRTIO_BLK_T_GET_ID)
		dprintf("  get id");
	else if (req->type == VIRTIO_BLK_T_BARRIER)
		dprintf("  barrier");
	else
		dprintf("  command %08x", req->type);
	dprintf(".\n");

	cur = desc->next;
	desc = (struct vring_desc *)
		vmm_translate_gp2hp(vm, blk->vring.desc_guest_addr +
				    sizeof(struct vring_desc) * cur);
	dprintf("Desc %d, addr %llx, len %08x, flags %04x, next %04x.\n",
		cur, desc->addr, desc->len, desc->flags, desc->next);

	dprintf("  buf addr %llx, len %d", desc->addr, desc->len);
	if (req->type == VIRTIO_BLK_T_IN || req->type == VIRTIO_BLK_T_OUT)
		dprintf(", %d sectors", desc->len / ATA_SECTOR_SIZE);
	dprintf(".\n");

	cur = desc->next;
	desc = (struct vring_desc *)
		vmm_translate_gp2hp(vm, blk->vring.desc_guest_addr +
				    sizeof(struct vring_desc) * cur);
	dprintf("Desc %d, addr %llx, len %08x, flags %04x, next %04x.\n",
		cur, desc->addr, desc->len, desc->flags, desc->next);
	dprintf("  status addr %llx.\n", desc->addr);

	dprintf("================\n");
#endif
}

static int
virtio_blk_handle_req(struct virtio_device *dev,
		      uint8_t vq_idx, uint16_t desc_idx)
{
	KERN_ASSERT(dev != NULL);

	struct vm *vm;
	struct virtio_blk *blk;
	struct vring_desc *req_desc, *buf_desc, *status_desc;
	struct vring_avail *avail;
	struct vring_used *used;
	uint16_t last_used_idx;
	struct virtio_blk_outhdr *req;
	uint8_t *buf, *status;
	uint32_t nsectors;

	if (vq_idx != 0)
		return 1;

	vm = dev->vm;

	blk = (struct virtio_blk *) dev;

	virtio_blk_dump_req(vm, blk, desc_idx);

	/* 1st ring descriptor: request type */
	req_desc = vring_get_desc(vm, &blk->vring, desc_idx);
	KERN_ASSERT(!(req_desc->flags & VRING_DESC_F_WRITE));
	KERN_ASSERT(req_desc->flags & VRING_DESC_F_NEXT);
	req = (struct virtio_blk_outhdr *)
		vmm_translate_gp2hp(vm, req_desc->addr);

	/* 2nd ring descriptor: buffer address, buffer length */
	buf_desc = vring_get_desc(vm, &blk->vring, req_desc->next);
	KERN_ASSERT(buf_desc->flags & VRING_DESC_F_NEXT);
	buf = (uint8_t *) vmm_translate_gp2hp(vm, buf_desc->addr);
	if (req->type == VIRTIO_BLK_T_IN || req->type == VIRTIO_BLK_T_OUT)
		KERN_ASSERT(buf_desc->len >= ATA_SECTOR_SIZE);
	nsectors = buf_desc->len / ATA_SECTOR_SIZE;

	/* 3rd ring descriptor: status address */
	status_desc = vring_get_desc(vm, &blk->vring, buf_desc->next);
	status = (uint8_t *) vmm_translate_gp2hp(vm, status_desc->addr);

	/* dispatch the request */
	switch (req->type) {
	case VIRTIO_BLK_T_IN:
		if (virtio_blk_read_disk(req->sector, nsectors, buf))
			*status = VIRTIO_BLK_S_IOERR;
		else
			*status = VIRTIO_BLK_S_OK;
		break;
	case VIRTIO_BLK_T_OUT:
		if (virtio_blk_write_disk(req->sector, nsectors, buf))
			*status = VIRTIO_BLK_S_IOERR;
		else
			*status = VIRTIO_BLK_S_OK;
		break;
	case VIRTIO_BLK_T_FLUSH:
		*status = VIRTIO_BLK_S_OK;
		break;
	case VIRTIO_BLK_T_GET_ID:
		virtio_blk_get_id(buf, buf_desc->len);
		*status = VIRTIO_BLK_S_OK;
		break;
	case VIRTIO_BLK_T_BARRIER:
		*status = VIRTIO_BLK_S_OK;
		break;
	default:
		virtio_blk_debug("unsupported command %x.\n", req->type);
		*status = VIRTIO_BLK_S_UNSUPP;
	}

	avail = vring_get_avail(vm, &blk->vring);
	blk->vring.need_notify =
		(avail->flags & VRING_AVAIL_F_NO_INTERRUPT) ? FALSE : TRUE;

	/* update used ring info */
	used = vring_get_used(vm, &blk->vring);
	last_used_idx = used->idx % blk->vring.queue_size;
	used->ring[last_used_idx].id = desc_idx;
	used->ring[last_used_idx].len =
		(req->type == VIRTIO_BLK_T_IN) ? nsectors * ATA_SECTOR_SIZE :
		(req->type == VIRTIO_BLK_T_GET_ID) ? VIRTIO_BLK_DEVICE_NAME_LEN
		: 0;
	used->idx += 1;

	smp_wmb();

	return 0;
}

static struct virtio_device_ops virtio_blk_ops =
	{
		.pci_conf_read		= virtio_blk_pci_conf_read,
		.pci_conf_write		= virtio_blk_pci_conf_write,
		.update_ioport_handlers	= virtio_blk_update_ioport_handlers,
		.get_vrings_amount	= virtio_blk_vrings_amount,
		.get_vring		= virtio_blk_get_vring,
		.handle_req		= virtio_blk_handle_req,
	};

int
virtio_blk_init(struct virtio_blk *blk, struct vm *vm)
{
	KERN_ASSERT(blk != NULL && vm != NULL);

	struct virtio_device *dev = &blk->common_header;

	if (virtio_device_init(dev, vm, &virtio_blk_ops)) {
		virtio_blk_debug("failed to initialize virtio device.\n");
		return 1;
	}

	dev->pci_conf.header.device = VIRTIO_PCI_DEVICE_BLK;
	dev->pci_conf.header.class = PCI_CLASS_MASS_STORAGE;
	dev->pci_conf.header.subclass = PCI_SUBCLASS_MASS_STORAGE_IDE;
	dev->pci_conf.header.header_type = PCI_HDRTYPE_DEVICE;
	dev->pci_conf.sub_id = VIRTIO_PCI_SUBDEV_BLK;
	dev->pci_conf.intr_line = 5 /* IRQ_IDE */;
	dev->pci_conf.intr_pin = PCI_INTERRUPT_PIN_A;

	blk->common_header.virtio_header.device_features =
		VIRTIO_BLK_F_SIZE_MAX |
		VIRTIO_BLK_F_SEG_MAX |
		VIRTIO_BLK_F_BLK_SIZE;

	blk->blk_header.capacity = ahci_disk_capacity(0);
	blk->blk_header.size_max = 4096;
	blk->blk_header.seg_max = 1;
	blk->blk_header.blk_size = 512;

	blk->vring.queue_size = VIRTIO_BLK_QUEUE_SIZE;

	smp_wmb();

	return 0;
}

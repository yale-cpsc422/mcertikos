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

#include <machine/trap.h>

#ifdef DEBUG_VIRTIO_BLK

#define VIO_BLK_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG("VIO_BLK: ");	\
		dprintf(fmt);			\
	}

#else

#define VIO_BLK_DEBUG(fmt...)			\
	{					\
	}

#endif

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
virtio_blk_read_disk(uint64_t lba, uint64_t nsectors, void *buf)
{
	KERN_ASSERT(buf != NULL);

	int ret = ahci_disk_read(0, lba, nsectors, buf);

	VIO_BLK_DEBUG("read %s.\n", ret ? "failed" : "OK");

	return ret;
}

static int
virtio_blk_write_disk(uint64_t lba, uint64_t nsectors, void *buf)
{
	KERN_ASSERT(buf != NULL);

	int ret = ahci_disk_write(0, lba, nsectors, buf);

	VIO_BLK_DEBUG("write %s.\n", ret ? "failed" : "OK");

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
}

static void
virtio_blk_notify(struct vm *vm, struct virtio_blk *blk)
{
	KERN_ASSERT(vm != NULL && blk != NULL);

	VIO_BLK_DEBUG("raise IRQ_IDE (%x).\n", blk->pci_conf.intr_line);

	/* int isr = blk->virtio_header.isr_status; */

	blk->virtio_header.isr_status = 1;

	blk->pci_conf.header.status |= (1 << 3);

	vmm_set_vm_irq(vm, blk->pci_conf.intr_line, 0);
	vmm_set_vm_irq(vm, blk->pci_conf.intr_line, 1);
}

static void
virtio_blk_handle_req(struct vm *vm, struct virtio_blk *blk, uint16_t desc_id)
{
	KERN_ASSERT(vm != NULL && blk != NULL);

	uint16_t cur_desc, used_ring;
	struct vring_desc *desc;
	struct vring_used *used;
	struct virtio_blk_outhdr *req;
	uint8_t *buf, *status;
	uint32_t nsectors;

	cur_desc = desc_id;
	desc = (struct vring_desc *)
		vmm_translate_gp2hp(vm, blk->vring.desc_guest_addr +
				    sizeof(struct vring_desc) * cur_desc);

	KERN_ASSERT(!(desc->flags & VRING_DESC_F_WRITE));
	KERN_ASSERT(desc->flags & VRING_DESC_F_NEXT);

	req = (struct virtio_blk_outhdr *)
		vmm_translate_gp2hp(vm, desc->addr);

	cur_desc = desc->next;
	desc = (struct vring_desc *)
		vmm_translate_gp2hp(vm, blk->vring.desc_guest_addr +
				    sizeof(struct vring_desc) * cur_desc);

	KERN_ASSERT(desc->flags & VRING_DESC_F_NEXT);

	buf = (uint8_t *) vmm_translate_gp2hp(vm, desc->addr);
	if (req->type == VIRTIO_BLK_T_IN || req->type == VIRTIO_BLK_T_OUT)
		KERN_ASSERT(desc->len >= ATA_SECTOR_SIZE);
	nsectors = desc->len / ATA_SECTOR_SIZE;

	cur_desc = desc->next;
	desc = (struct vring_desc *)
		vmm_translate_gp2hp(vm, blk->vring.desc_guest_addr +
				    sizeof(struct vring_desc) * cur_desc);
	status = (uint8_t *) vmm_translate_gp2hp(vm, desc->addr);

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
		virtio_blk_get_id(buf, desc->len);
		*status = VIRTIO_BLK_S_OK;
		break;
	case VIRTIO_BLK_T_BARRIER:
		*status = VIRTIO_BLK_S_OK;
		break;
	default:
		VIO_BLK_DEBUG("unsupported command %x.\n", req->type);
		*status = VIRTIO_BLK_S_UNSUPP;
	}

	used = (struct vring_used *)
		vmm_translate_gp2hp(vm, blk->vring.used_guest_addr);
	used_ring = used->idx % blk->vring.queue_size;
	used->ring[used_ring].id = desc_id;
	used->ring[used_ring].len =
		(req->type == VIRTIO_BLK_T_IN) ? nsectors * ATA_SECTOR_SIZE :
		(req->type == VIRTIO_BLK_T_GET_ID) ? VIRTIO_BLK_DEVICE_NAME_LEN : 0;
	used->idx += 1/* (used_ring + 1) % (blk->vring.queue_size) */;
	smp_wmb();

	VIO_BLK_DEBUG("used.ring[%d]={id %d, nsect %d}, used.idx %d\n",
		      used_ring,
		      used->ring[used_ring].id,
		      used->ring[used_ring].len/ATA_SECTOR_SIZE,
		      used->idx);
}

void
virtio_blk_handle_vrings(struct vm *vm, struct virtio_blk *blk)
{
	KERN_ASSERT(vm != NULL && blk != NULL);
	KERN_ASSERT(blk->virtio_header.queue_notify == 0);

	struct vring_avail *avail;
	uint16_t avail_idx, qsz, i;
	int no_int;

	qsz = blk->vring.queue_size;

	if (qsz == 0)
		return;

	avail = (struct vring_avail *)
		vmm_translate_gp2hp(vm, blk->vring.avail_guest_addr);
	avail_idx = avail->idx;
	no_int = avail->flags & VRING_AVAIL_F_NO_INTERRUPT;

	VIO_BLK_DEBUG("avail.ring[%d]=%d, flags %04x, avail %d, last avail %d.\n",
		      blk->vring.last_avail_idx % qsz,
		      avail->ring[blk->vring.last_avail_idx % qsz],
		      avail->flags, avail->idx, blk->vring.last_avail_idx);

#if 1
	KERN_ASSERT(blk->vring.last_avail_idx <= avail_idx);
#else
	if (blk->vring.last_avail_idx > avail_idx) {
		VIO_BLK_DEBUG("redundant notify?\n");
		blk->pending_req = FALSE;
		smp_wmb();
		return;
	}
#endif

	if (blk->vring.last_avail_idx == avail_idx) {
		VIO_BLK_DEBUG("queue is full or queue is empty?\n");

		/* virtio_blk_dump_req */
		/* 	(vm, blk, avail->ring[blk->vring.last_avail_idx % qsz]); */
		/* virtio_blk_handle_req */
		/* 	(vm, blk, avail->ring[blk->vring.last_avail_idx % qsz]); */

		/* if (!no_int) */
		/* 	virtio_blk_notify(vm, blk); */

		/* blk->vring.last_avail_idx = avail_idx + 1; */

		blk->pending_req = FALSE;
		smp_wmb();
		return;
	}

	for (i = blk->vring.last_avail_idx; i < avail_idx; i++) {
		virtio_blk_dump_req(vm, blk, avail->ring[i % qsz]);
		virtio_blk_handle_req(vm, blk, avail->ring[i % qsz]);
		if (!no_int) {
			virtio_blk_notify(vm, blk);
			break;
		}
	}

	blk->vring.last_avail_idx = (i == avail_idx) ? avail_idx : (i+1);

	if (i < avail_idx)
		blk->pending_req = TRUE;
	else
		blk->pending_req = FALSE;

	smp_wmb();
}

static void
virtio_blk_get_device_features(struct vm *vm,
			       void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase);

	*(uint32_t *) data = blk->virtio_header.device_features;

	VIO_BLK_DEBUG("device features %08x.\n", *(uint32_t *) data);
}

static void
virtio_blk_get_guest_features(struct vm *vm,
			      void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x4);

	*(uint32_t *) data = blk->virtio_header.guest_features;

	VIO_BLK_DEBUG("guest features %08x.\n", *(uint32_t *) data);
}

static void
virtio_blk_set_guest_features(struct vm *vm,
			      void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x4);

	blk->virtio_header.guest_features = *(uint32_t *) data;

	VIO_BLK_DEBUG("set guest features %08x.\n", *(uint32_t *) data);
}

static void
virtio_blk_get_queue_addr(struct vm *vm,
			  void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x8);

	*(uint32_t *) data = blk->virtio_header.queue_addr;

	VIO_BLK_DEBUG("queue address %08x.\n", *(uint32_t *) data * 4096);
}

static void
virtio_blk_set_queue_addr(struct vm *vm,
			  void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t queue_addr;
	uint16_t queue_size;
	uint32_t desc_size, avail_size;

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x8);

	blk->virtio_header.queue_addr = *(uint32_t *) data;

	if (blk->virtio_header.queue_select != 0)
		goto ret;

	queue_addr = *(uint32_t *) data * 4096;
	queue_size = blk->vring.queue_size;
	desc_size = sizeof(struct vring_desc);
	avail_size = sizeof(struct vring_avail) + sizeof(uint16_t) * queue_size;

	blk->vring.desc_guest_addr = queue_addr;
	blk->vring.avail_guest_addr = queue_addr + desc_size * queue_size;
	blk->vring.used_guest_addr = ROUNDUP
		(queue_addr + desc_size * queue_size + avail_size, 4096);

	VIO_BLK_DEBUG("desc %08x, avail %08x, used %08x.\n",
		      blk->vring.desc_guest_addr,
		      blk->vring.avail_guest_addr,
		      blk->vring.used_guest_addr);

 ret:
	VIO_BLK_DEBUG("set queue address %08x.\n", *(uint32_t *) data * 4096);
}

static void
virtio_blk_get_queue_size(struct vm *vm,
			  void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0xc);

	*(uint16_t *) data = blk->virtio_header.queue_size;

	VIO_BLK_DEBUG("queue size %x.\n", *(uint16_t *) data);
}

static void
virtio_blk_get_queue_select(struct vm *vm,
			    void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0xe);

	*(uint16_t *) data = blk->virtio_header.queue_select;

	VIO_BLK_DEBUG("queue %d is selected.\n", *(uint16_t *) data);
}

static void
virtio_blk_set_queue_select(struct vm *vm,
			    void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0xe);

	blk->virtio_header.queue_select = *(uint16_t *) data;

	if (blk->virtio_header.queue_select == 0) {
		blk->virtio_header.queue_addr = blk->vring.desc_guest_addr/4096;
		blk->virtio_header.queue_size = blk->vring.queue_size;
	} else {
		blk->virtio_header.queue_addr = 0;
		blk->virtio_header.queue_size = 0;
	}

	VIO_BLK_DEBUG("select queue %d.\n", *(uint16_t *) data);
}

static void
virtio_blk_get_queue_notify(struct vm *vm,
			    void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x10);

	*(uint16_t *) data = blk->virtio_header.queue_notify;

	VIO_BLK_DEBUG("queue %d is notified.\n", *(uint16_t *) data);
}

static void
virtio_blk_set_queue_notify(struct vm *vm,
			    void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x10);

	blk->virtio_header.queue_notify = *(uint16_t *) data;

	dprintf("\n");
	VIO_BLK_DEBUG("notify queue %d.\n", *(uint16_t *) data);

	if (blk->virtio_header.queue_notify != 0)
		return;

	virtio_blk_handle_vrings(vm, blk);
}

static void
virtio_blk_get_device_status(struct vm *vm,
			     void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x12);

        *(uint8_t *) data = blk->virtio_header.device_status;

	VIO_BLK_DEBUG("device status %02x.\n", *(uint8_t *) data);
}

static void
virtio_blk_set_device_status(struct vm *vm,
			     void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x12);

	blk->virtio_header.device_status = *(uint8_t *) data;

	VIO_BLK_DEBUG("set device status %02x.\n", *(uint8_t *) data);
}

static void
virtio_blk_get_isr_status(struct vm *vm,
			  void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	struct virtio_blk *blk = (struct virtio_blk *) dev;
	KERN_ASSERT(port == blk->iobase+0x13);

	uint8_t isr = blk->virtio_header.isr_status;
	if (isr) {
		blk->virtio_header.isr_status = 0;
		blk->pci_conf.header.status &= ~(uint16_t) (1 << 3);
	}
        *(uint8_t *) data = isr;

	VIO_BLK_DEBUG("ISR %02x.\n", *(uint8_t *) data);
}

static void
virtio_blk_conf_readb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t iobase;
	uint8_t *conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase + sizeof(struct virtio_header);
	KERN_ASSERT(iobase <= port &&
		    port < iobase + sizeof(struct virtio_blk_config));
	conf = (uint8_t *) &blk->virtio_blk_header;

	*(uint8_t *) data = conf[port-iobase];

	VIO_BLK_DEBUG("readb blk reg %x, val %02x.\n",
		      port - iobase, *(uint8_t *) data);
}

static void
virtio_blk_conf_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t iobase;
	uint8_t *conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase + sizeof(struct virtio_header);
	KERN_ASSERT(iobase <= port &&
		    port < iobase + sizeof(struct virtio_blk_config) - 1);
	conf = (uint8_t *) &blk->virtio_blk_header;

	*(uint16_t *) data =
		conf[port-iobase] | ((uint16_t) conf[port-iobase+1] << 8);

	VIO_BLK_DEBUG("readw blk reg %x, val %04x.\n",
		      port - iobase, *(uint16_t *) data);
}

static void
virtio_blk_conf_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint32_t iobase;
	uint8_t *conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase + sizeof(struct virtio_header);
	KERN_ASSERT(iobase <= port &&
		    port < iobase + sizeof(struct virtio_blk_config) - 3);
	conf = (uint8_t *) &blk->virtio_blk_header;

	*(uint32_t *) data =
		conf[port-iobase] | ((uint32_t) conf[port-iobase+1] << 8) |
		((uint32_t) conf[port-iobase+2] << 16) |
		((uint32_t) conf[port-iobase+3] << 24);

	VIO_BLK_DEBUG("readw blk reg %x, val %08x.\n",
		      port - iobase, *(uint32_t *) data);
}

static void
virtio_blk_ioport_readb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint16_t iobase, iosize;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase;
	iosize = blk->iosize;

	KERN_ASSERT(iobase <= port && port < iobase+iosize);

	*(uint8_t *) data = 0;

	VIO_BLK_DEBUG("readb reg %x, nop.\n", port - blk->iobase);
}

static void
virtio_blk_ioport_readw(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint16_t iobase, iosize;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase;
	iosize = blk->iosize;

	KERN_ASSERT(iobase <= port && port < iobase+iosize-1);

	*(uint16_t *) data = 0;

	VIO_BLK_DEBUG("readw reg %x, nop.\n", port - blk->iobase);
}

static void
virtio_blk_ioport_readl(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint16_t iobase, iosize;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase;
	iosize = blk->iosize;

	KERN_ASSERT(iobase <= port && port < iobase+iosize-3);

	*(uint32_t *) data = 0;

	VIO_BLK_DEBUG("readl reg %x, nop.\n", port - blk->iobase);
}

static void
virtio_blk_ioport_writeb(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint16_t iobase, iosize;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase;
	iosize = blk->iosize;

	KERN_ASSERT(iobase <= port && port < iobase+iosize);

	VIO_BLK_DEBUG("writeb reg %x, val %02x, nop.\n",
		      port - blk->iobase, *(uint8_t *) data);
}

static void
virtio_blk_ioport_writew(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint16_t iobase, iosize;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase;
	iosize = blk->iosize;

	KERN_ASSERT(iobase <= port && port < iobase+iosize-1);

	VIO_BLK_DEBUG("writew reg %x, val %04x, nop.\n",
		      port - blk->iobase, *(uint16_t *) data);
}

static void
virtio_blk_ioport_writel(struct vm *vm, void *dev, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && dev != NULL && data != NULL);

	uint16_t iobase, iosize;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	iobase = blk->iobase;
	iosize = blk->iosize;

	KERN_ASSERT(iobase <= port && port < iobase+iosize-3);

	VIO_BLK_DEBUG("writel reg %x, val %08x, nop.\n",
		      port - blk->iobase, *(uint32_t *) data);
}

static void
virtio_blk_perform_pci_command(struct virtio_blk *blk)
{
	KERN_ASSERT(blk != NULL);

	uint16_t command;
	struct pci_general *pci_conf;

	static uint16_t unsupported_command;

	unsupported_command = 0xf880 /* reserved bits */ |
		PCI_COMMAND_MEM_ENABLE |
		PCI_COMMAND_SPECIAL_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE |
		PCI_COMMAND_PALETTE_ENABLE | PCI_COMMAND_PARITY_ENABLE |
		PCI_COMMAND_SERR_ENABLE | PCI_COMMAND_BACKTOBACK_ENABLE;

	pci_conf = &blk->pci_conf;
	command = pci_conf->header.command;

	if (command == 0) {
		VIO_BLK_DEBUG("logically disconnected.\n");
		blk->disconnected = 1;
		blk->vring.desc_guest_addr = 0;
		blk->vring.avail_guest_addr = 0;
		blk->vring.used_guest_addr = 0;
		blk->vring.last_avail_idx = 0;
	}

	if (command & unsupported_command) {
		VIO_BLK_DEBUG("ignore unsupported command %08x.\n",
			      command & unsupported_command);
		command &= ~unsupported_command;
		pci_conf->header.command = command;
	}
}

static gcc_inline void
unregister_register_read(struct vm *vm, struct virtio_blk *blk,
			 uint32_t port, data_sz_t size,
			 iodev_read_func_t func)
{
	vmm_iodev_unregister_read(vm, port, size);
	vmm_iodev_register_read(vm, blk, port, size, func);
}

static gcc_inline void
unregister_register_write(struct vm *vm, struct virtio_blk *blk,
			  uint32_t port, data_sz_t size,
			  iodev_write_func_t func)
{
	vmm_iodev_unregister_write(vm, port, size);
	vmm_iodev_register_write(vm, blk, port, size, func);
}

static void
virtio_blk_register_port_handlers(struct virtio_blk *blk)
{
	KERN_ASSERT(blk != NULL);

	struct vm *vm;
	uint16_t iobase, iosize, conf_iobase, conf_ioend, port;

	vm = vmm_cur_vm();
	KERN_ASSERT(vm != NULL);

	iobase = blk->iobase;
	iosize = blk->iosize;
	KERN_ASSERT(0xffff - iobase + 1 >= iosize);

	VIO_BLK_DEBUG("Register handlers for ports %x ~ %x.\n",
		      iobase, iobase+iosize-1);

	for (port = iobase; port < iobase + iosize; port++) {
		int delta = iobase + iosize - port;

		vmm_iodev_register_read(vm, blk, port, SZ8,
					virtio_blk_ioport_readb);
		vmm_iodev_register_write(vm, blk, port, SZ8,
					 virtio_blk_ioport_writeb);
		if (delta > 1) {
			vmm_iodev_register_read(vm, blk, port, SZ16,
						virtio_blk_ioport_readw);
			vmm_iodev_register_write(vm, blk, port, SZ16,
						 virtio_blk_ioport_writew);
		}
		if (delta > 3) {
			vmm_iodev_register_read(vm, blk, port, SZ32,
						virtio_blk_ioport_readl);
			vmm_iodev_register_write(vm, blk, port, SZ32,
						 virtio_blk_ioport_writel);
		}
	}

	/* device features bits */
	unregister_register_read(vm, blk, iobase, SZ32,
				 virtio_blk_get_device_features);

	/* guest features bits */
	unregister_register_read(vm, blk, iobase+0x4, SZ32,
				 virtio_blk_get_guest_features);
	unregister_register_write(vm, blk, iobase+0x4, SZ32,
				  virtio_blk_set_guest_features);

	/* queue address */
	unregister_register_read(vm, blk, iobase+0x8, SZ32,
				 virtio_blk_get_queue_addr);
	unregister_register_write(vm, blk, iobase+0x8, SZ32,
				  virtio_blk_set_queue_addr);

	/* queue size */
	unregister_register_read(vm, blk, iobase+0xc, SZ16,
				 virtio_blk_get_queue_size);

	/* queue select */
	unregister_register_read(vm, blk, iobase+0xe, SZ16,
				 virtio_blk_get_queue_select);
	unregister_register_write(vm, blk, iobase+0xe, SZ16,
				  virtio_blk_set_queue_select);

	/* queue notify */
	unregister_register_read(vm, blk, iobase+0x10, SZ16,
				 virtio_blk_get_queue_notify);
	unregister_register_write(vm, blk, iobase+0x10, SZ16,
				  virtio_blk_set_queue_notify);

	/* device status */
	unregister_register_read(vm, blk, iobase+0x12, SZ8,
				 virtio_blk_get_device_status);
	unregister_register_write(vm, blk, iobase+0x12, SZ8,
				  virtio_blk_set_device_status);

	/* ISR */
	unregister_register_read(vm, blk, iobase+0x13, SZ8,
				 virtio_blk_get_isr_status);

	/* virtio_blk_header */
	conf_iobase = iobase + sizeof(struct virtio_header);
	conf_ioend = conf_iobase + sizeof(struct virtio_blk_config);
	for (port = conf_iobase; port < conf_ioend; port++) {
		unregister_register_read(vm, blk, port, SZ8,
					 virtio_blk_conf_readb);
		if (port < conf_ioend - 1)
			unregister_register_read(vm, blk, port, SZ16,
						 virtio_blk_conf_readw);
		if (port < conf_ioend - 3)
			unregister_register_read(vm, blk, port, SZ32,
						 virtio_blk_conf_readl);
	}

	vmm_update(vm);
}

static void
virtio_blk_unregister_port_handlers(struct virtio_blk *blk)
{
	KERN_ASSERT(blk != NULL);

	struct vm *vm;
	uint16_t iobase, iosize, port;

	vm = vmm_cur_vm();
	KERN_ASSERT(vm != NULL);

	iobase = blk->iobase;
	iosize = blk->iosize;
	KERN_ASSERT(0xffff - iobase + 1 >= iosize);

	VIO_BLK_DEBUG("Unregister handlers for ports %x ~ %x.\n",
		      iobase, iobase+iosize-1);

	for (port = iobase; port < iobase + iosize; port++) {
		int delta = iobase + iosize - port;

		vmm_iodev_unregister_read(vm, port, SZ8);
		vmm_iodev_unregister_write(vm, port, SZ8);
		if (delta > 1) {
			vmm_iodev_unregister_read(vm, port, SZ16);
			vmm_iodev_unregister_write(vm, port, SZ16);
		}
		if (delta > 3) {
			vmm_iodev_unregister_read(vm, port, SZ32);
			vmm_iodev_unregister_write(vm, port, SZ32);
		}
	}

	vmm_update(vm);
}

static uint32_t
virtio_blk_pci_conf_read(void *dev, uint32_t pci_addr, data_sz_t size)
{
	KERN_ASSERT(dev != NULL);

	int len;
	uint8_t reg, *pci_conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	reg = pci_addr & 0xfc;
	len = sizeof(blk->pci_conf);

	if (reg >= len)
		goto err;

	pci_conf = (uint8_t *) &blk->pci_conf;

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
		(size == SZ16) ? 0x0000ffff :
		0xffffffff);
}

static void
virtio_blk_pci_conf_write(void *dev,
		      uint32_t pci_addr, uint32_t val, data_sz_t size)
{
	KERN_ASSERT(dev != NULL);

	int len;
	uint8_t reg, *pci_conf;
	struct virtio_blk *blk;

	blk = (struct virtio_blk *) dev;
	reg = pci_addr & 0xfc;
	len = sizeof(blk->pci_conf);

	if (reg >= len || reg == PCI_ID_REG || reg == PCI_CLASS_REG ||
	    (PCI_MAPREG_START+4 <= reg && reg < PCI_MAPREG_END) ||
	    (0x2c <= reg && reg <= 0x3c))
		return;

	pci_conf = (uint8_t *) &blk->pci_conf;

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
		virtio_blk_perform_pci_command(blk);
	} else if (reg == PCI_MAPREG_START) {
		if (blk->pci_conf.bar[0] == 0xffffffff) {
			/* calculate BAR0 size */
			uint16_t size;
			int bytes;
			size = sizeof(struct virtio_header) +
				sizeof(struct virtio_blk_config);
			for (bytes = 0; size > 0; bytes++)
				size >>= 1;
			blk->iosize = (1 << bytes);
			blk->pci_conf.bar[0] ^= (1 << bytes) - 1;
			blk->pci_conf.bar[0] |= 0x1;
			VIO_BLK_DEBUG("BAR[0]=%08x.\n", blk->pci_conf.bar[0]);
		} else if (blk->pci_conf.bar[0] != 0) {
			/* set BAR0 address */
			if (blk->iobase != 0xffff)
				virtio_blk_unregister_port_handlers(blk);
			blk->pci_conf.bar[0] =
				(blk->pci_conf.bar[0] & ~(uint32_t) 0x3) | 0x1;
			blk->iobase = blk->pci_conf.bar[0] & 0xfffc;
			virtio_blk_register_port_handlers(blk);
		}
	} else if (reg == PCI_INTERRUPT_REG) {
		pci_conf[PCI_INTERRUPT_REG] &= 0x000000ff;
	}
}

void
virtio_blk_init(struct vm *vm, struct vpci_device *pci_dev,
		struct virtio_blk *virtio_blk)
{
	KERN_ASSERT(vm != NULL && pci_dev != NULL && virtio_blk != NULL);

	memset(virtio_blk, 0, sizeof(struct virtio_blk));

	virtio_blk->pci_conf.header.vendor = VIRTIO_PCI_VENDOR_ID;
	virtio_blk->pci_conf.header.device = VIRTIO_PCI_DEVICE_BLK;
	virtio_blk->pci_conf.header.revision = VIRTIO_PCI_REVISION;
	virtio_blk->pci_conf.header.class = PCI_CLASS_MASS_STORAGE;
	virtio_blk->pci_conf.header.subclass = PCI_SUBCLASS_MASS_STORAGE_IDE;
	virtio_blk->pci_conf.header.header_type = PCI_HDRTYPE_DEVICE;
	virtio_blk->pci_conf.sub_id = VIRTIO_PCI_SUBDEV_BLK;
	virtio_blk->pci_conf.sub_vendor = VIRTIO_PCI_VENDOR_ID;
	virtio_blk->pci_conf.intr_line = 5 /* IRQ_IDE */;
	virtio_blk->pci_conf.intr_pin = PCI_INTERRUPT_PIN_A;

	virtio_blk->virtio_header.device_features = VIRTIO_BLK_F_SIZE_MAX
		| VIRTIO_BLK_F_SEG_MAX | VIRTIO_BLK_F_BLK_SIZE;
	virtio_blk->virtio_blk_header.capacity = ahci_disk_capacity(0);
	virtio_blk->virtio_blk_header.size_max = PAGE_SIZE;
	virtio_blk->virtio_blk_header.seg_max = 1;
	virtio_blk->virtio_blk_header.blk_size = 512;

	virtio_blk->iobase = 0xffff;

	virtio_blk->virtio_header.device_features = VIRTIO_BLK_F_SIZE_MAX
		| VIRTIO_BLK_F_SEG_MAX | VIRTIO_BLK_F_BLK_SIZE;
	virtio_blk->virtio_blk_header.capacity = ahci_disk_capacity(0);
	virtio_blk->virtio_blk_header.size_max = PAGE_SIZE;
	virtio_blk->virtio_blk_header.seg_max = 1;
	virtio_blk->virtio_blk_header.blk_size = 512;

	virtio_blk->vring.queue_size = VIRTIO_BLK_QUEUE_SIZE;

	pci_dev->dev = virtio_blk;
	pci_dev->conf_read = virtio_blk_pci_conf_read;
	pci_dev->conf_write = virtio_blk_pci_conf_write;

	vpci_attach_device(&vm->vpci, pci_dev);
}

bool
virtio_blk_has_pending_req(struct vm * vm, struct virtio_blk *blk)
{
	KERN_ASSERT(vm != NULL && blk != NULL);

	if (blk->virtio_header.device_status !=
	    (VIRTIO_CONFIG_S_ACKNOWLEDGE | VIRTIO_CONFIG_S_DRIVER |
	     VIRTIO_CONFIG_S_DRIVER_OK))
		return FALSE;

	return blk->pending_req;
}

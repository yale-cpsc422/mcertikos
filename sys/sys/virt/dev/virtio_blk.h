#ifndef _SYS_VIRT_DEV_VIRTIO_BLK_H_
#define _SYS_VIRT_DEV_VIRTIO_BLK_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/dev/virtio.h>

#include <dev/pci.h>

/* VirtIO block device features */
#define VIRTIO_BLK_F_BARRIER	(1 << 0)
#define VIRTIO_BLK_F_SIZE_MAX	(1 << 1)
#define VIRTIO_BLK_F_SEG_MAX	(1 << 2)
#define VIRTIO_BLK_F_GEOMETRY	(1 << 4)
#define VIRTIO_BLK_F_RO		(1 << 5)
#define VIRTIO_BLK_F_BLK_SIZE	(1 << 6)
#define VIRTIO_BLK_F_SCSI	(1 << 7)
#define VIRTIO_BLK_F_FLUSH	(1 << 9)

/* VirtIO block device configuration layout */
struct virtio_blk_config {
	uint64_t capacity;	/* in 512 byte sectors */
	uint32_t size_max;
	uint32_t seg_max;

	struct virtio_blk_geometry {
		uint16_t cylinders;
		uint8_t heads;
		uint8_t sectors;
	} geometry;

	uint32_t blk_size;
};

struct virtio_blk_outhdr {
	uint32_t type;
#define VIRTIO_BLK_T_IN			0x00000000
#define VIRTIO_BLK_T_OUT		0x00000001
#define VIRTIO_BLK_T_SCSI_CMD		0x00000002
#define VIRTIO_BLK_T_FLUSH		0x00000004
#define VIRTIO_BLK_T_BARRIER		0x80000000
	uint32_t ioprio;
	uint64_t sector;
};

#define VIRTIO_BLK_S_OK			0x00000000
#define VIRTIO_BLK_S_IOERR		0x00000001
#define VIRTIO_BLK_S_UNSUPP		0x00000002

#define VIRTIO_BLK_QUEUE_SIZE		8

struct virtio_blk {
	struct pci_general pci_conf;
	struct virtio_header virtio_header;
	struct virtio_blk_config virtio_blk_header;

	int disconnected;
	uint16_t iobase, iosize;

	struct vring vring;
};

void virtio_blk_init(struct vm *, struct vpci_device *, struct virtio_blk *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_VIRTIO_BLK_H_ */

#ifndef _SYS_VIRT_DEV_VIRTIO_H_
#define _SYS_VIRT_DEV_VIRTIO_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>

#include <dev/pci.h>

#define VIRTIO_PCI_VENDOR_ID		0x1af4
#define VIRTIO_PCI_DEVICE_BLK		0x1001
#define VIRTIO_PCI_REVISION		0x0

#define VIRTIO_PCI_SUBDEV_NIC		0x1
#define VIRTIO_PCI_SUBDEV_BLK		0x2
#define VIRTIO_PCI_SUBDEV_CONSOLE	0x3
#define VIRTIO_PCI_SUBDEV_ENTROPY	0x4
#define VIRTIO_PCI_SUBDEV_BALLOON	0x5
#define VIRTIO_PCI_SUBDEV_IOMEM		0x6
#define VIRTIO_PCI_SUBDEV_RPMSG		0x7
#define VIRTIO_PCI_SUBDEV_SCSI		0x8
#define VIRTIO_PCI_SUBDEV_9P		0x9

/*
 * PCI BAR0 ----> +---------------+  offset: 0x0
 *                |               |
 *                | VirtIO Header |  struct virtio_header
 *                |               |
 *                +---------------+  offset: 0x14
 *                |               |
 *                |   MSI Header  |  struct virtio_ms_header
 *                |   (optional)  |
 *                |               |
 *                +---------------+  offset: 0x18/0x14
 *                |               |
 *                | Device Header |  strcut virtio_blk_config, ...
 *                |               |
 *                +---------------+
 */

struct virtio_header {
	uint32_t device_features;	/* R */
	uint32_t guest_features;	/* RW */
	uint32_t queue_addr;		/* RW */
	uint16_t queue_size;		/* R */
	uint16_t queue_select;		/* RW */
	uint16_t queue_notify;		/* RW */
	uint8_t device_status;		/* RW */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE	(1 << 0)
#define VIRTIO_CONFIG_S_DRIVER		(1 << 1)
#define VIRTIO_CONFIG_S_DRIVER_OK	(1 << 2)
#define VIRTIO_CONFIG_S_FAILED		(1 << 8)
	uint8_t isr_status;		/* R */
};

struct virtio_msi_header {
	uint16_t config_vector;		/* RW */
	uint16_t queue_vector;		/* RW */
};

/*
 * VirtIO Rings
 */

struct vring_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
#define VRING_DESC_F_NEXT		(1 << 0)
#define VRING_DESC_F_WRITE		(1 << 1)
#define VRING_DESC_F_INDIRECT		(1 << 2)
	uint16_t next;
} gcc_packed;

struct vring_avail {
	uint16_t flags;
#define VRING_AVAIL_F_NO_INTERRUPT	(1 << 0)
	uint16_t idx;
	uint16_t ring[0];
} gcc_packed;

struct vring_used {
	uint16_t flags;
#define VRING_USED_F_NO_NOTIFY		(1 << 0)
	uint16_t idx;
	struct vring_used_elem {
		uint32_t id;
		uint32_t len;
	} ring[0];
} gcc_packed;

struct vring {
	uint16_t queue_size;
	uintptr_t desc_guest_addr;
	uintptr_t avail_guest_addr;
	uintptr_t used_guest_addr;
	uint16_t last_avail_idx;
};

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_VIRTIO_H_ */

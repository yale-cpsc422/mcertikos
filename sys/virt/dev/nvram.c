#include <sys/debug.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/nvram.h>

#define CMOS_INDEX_PORT		0x70
#define CMOS_DATA_PORT		0x71

/* memory size in KB from 1 MB ~ 16 MB */
#define CMOS_EXTMEM_LOW		0x30	/* Bits 7 ~ 0 */
#define CMOS_EXTMEM_HIGH	0x31	/* Bits 15 ~ 8 */
/* memory size in byte from 16 MB ~ 4 GB */
#define CMOS_EXTMEM2_LOW	0x34	/* Bits 23 ~ 16 */
#define CMOS_EXTMEM2_HIGH	0x35	/* Bits 31 ~ 24 */
/* memory size in byte above 4GB */
#define CMOS_HIGHMEM_LOW	0x5b	/* Bits 23 ~ 16 */
#define CMOS_HIGHMEM_MID	0x5c	/* Bits 31 ~ 24 */
#define CMOS_HIGHMEM_HIGH	0x5d	/* Bits 39 ~ 32 */

#define CMOS_NMI_DISABLE_BIT	(1 << 7)

static uint8_t
vnvram_ioport_read(struct vnvram *nvram, uint8_t port)
{
	KERN_ASSERT(nvram != NULL);
	KERN_ASSERT(port == CMOS_DATA_PORT);

	if (nvram->data_valid == TRUE)
		return nvram->data;
	else {
		/* KERN_DEBUG("Passthrough\n"); */
		return inb(port);
	}
}

static void
vnvram_ioport_write(struct vnvram *nvram, uint8_t port, uint8_t data)
{
	KERN_ASSERT(nvram != NULL);
	KERN_ASSERT(port == CMOS_INDEX_PORT);

	uint8_t idx = data & ~(uint8_t) CMOS_NMI_DISABLE_BIT;

	switch (idx) {
	case CMOS_EXTMEM_LOW:
		nvram->data = (nvram->extmem_size >> 10) & 0xFF;
		break;

	case CMOS_EXTMEM_HIGH:
		nvram->data = (nvram->extmem_size >> 18) & 0xFF;
		break;

	case CMOS_EXTMEM2_LOW:
		nvram->data = (nvram->extmem2_size >> 16) & 0xFF;
		break;

	case CMOS_EXTMEM2_HIGH:
		nvram->data = (nvram->extmem2_size >> 24) & 0xFF;
		break;

	case CMOS_HIGHMEM_LOW:
		nvram->data = (nvram->highmem_size >> 16) & 0xFF;
		break;

	case CMOS_HIGHMEM_MID:
		nvram->data = (nvram->highmem_size >> 24) & 0xFF;
		break;

	case CMOS_HIGHMEM_HIGH:
		nvram->data = (nvram->highmem_size >> 32) & 0xFF;
		break;

	default:
		/* KERN_DEBUG("Passthrough\n"); */
		nvram->data_valid = FALSE;
		outb(port, data);
		return;
	}

	nvram->data_valid = TRUE;
}

static void
_vnvram_ioport_read(struct vm *vm, void *nvram, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && nvram != NULL && data != NULL);
	KERN_ASSERT(port == CMOS_DATA_PORT);

	*(uint8_t *) data = vnvram_ioport_read(nvram, port);
}

static void
_vnvram_ioport_write(struct vm *vm, void *nvram, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL && nvram != NULL && data != NULL);
	KERN_ASSERT(port == CMOS_INDEX_PORT);

	vnvram_ioport_write(nvram, port, *(uint8_t *) data);
}

void
vnvram_init(struct vnvram *nvram, struct vm *vm)
{
	KERN_ASSERT(nvram != NULL && vm != NULL);

	memset(nvram, 0, sizeof(struct vnvram));

	nvram->data_valid = FALSE;

	uint64_t phy_mem_size = VM_PHY_MEMORY_SIZE;
	if (phy_mem_size > 0x100000000ULL) { /* above 4 GB */
		nvram->highmem_size = phy_mem_size - 0x100000000ULL;
		phy_mem_size -= nvram->highmem_size;
	}
	if (phy_mem_size > 0x1000000) { /* 16 MB ~ 4G */
		nvram->extmem2_size = phy_mem_size - 0x1000000;
		phy_mem_size -= nvram->extmem2_size;
	}
	if (phy_mem_size > 0x100000) { /* 1 MB ~ 16 MB */
		nvram->extmem_size = phy_mem_size - 0x100000;
		phy_mem_size -= nvram->extmem_size;
	}

	KERN_DEBUG("ExtMem: %x, ExtMem2: %x, HighMem:%llx.\n",
		   nvram->extmem_size, nvram->extmem2_size,
		   nvram->highmem_size);

	vmm_iodev_register_read(vm, nvram,
				CMOS_DATA_PORT, SZ8, _vnvram_ioport_read);
	vmm_iodev_register_write(vm, nvram,
				 CMOS_INDEX_PORT, SZ8, _vnvram_ioport_write);
}

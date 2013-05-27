#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/ide.h>

#ifdef DEBUG_DISK

#define DISK_DEBUG(fmt, ...) do {		\
		KERN_DEBUG(fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define DISK_DEBUG(fmt, ...) do {		\
	} while (0)

#endif

#define IDE_MASTER		0x1f0

#define IDE_PORT_DATA		0
#define IDE_PORT_ERR		1
#define IDE_PORT_NSECT		2
#define IDE_PORT_LBA_LO		3
#define IDE_PORT_LBA_MI		4
#define IDE_PORT_LBA_HI		5
#define IDE_PORT_DRV		6
# define IDE_DRV_MASTER_EXT	0x40
# define IDE_DRV_SLAVE_EXT	0x50
# define IDE_DRV_MASTER_IDENT	0xa0
# define IDE_DRV_SLAVE_IDENT	0xb0
# define IDE_DRV_SECONDARY	(1<<4)	/* select the secondary device */
#define IDE_PORT_CMD		7
# define IDE_CMD_READ_PIO_EXT	0x24	/* LBA48 read */
# define IDE_CMD_WRITE_PIO_EXT	0x34	/* LBA48 write */
# define IDE_CMD_CACHE_FLUSH	0xe7
# define IDE_CMD_IDENTIFY	0xec
#define IDE_PORT_STS		7
# define IDE_STS_BSY		(1<<7)	/* device is busy */
# define IDE_STS_DRDY		(1<<6)	/* device is ready */
# define IDE_STS_DWF		(1<<5)	/* device write fault */
# define IDE_STS_DRQ		(1<<3)
# define IDE_STS_ERR		(1<<0)	/* error bit */

#define DISK_SECT_SIZE		ATA_SECTOR_SIZE

static bool ide_inited = FALSE;
static uint16_t ide_info[256];

static void
delay(void)
{
	inb(0x84);
	inb(0x84);
	inb(0x84);
	inb(0x84);
}

static int
ide_wait_ready(void)
{
	int rc;

	do {
		rc = inb(IDE_MASTER + IDE_PORT_STS);
	} while ((rc & (IDE_STS_BSY | IDE_STS_DRDY)) != IDE_STS_DRDY &&
		 (rc & (IDE_STS_ERR | IDE_STS_DWF)) == 0);

	return (rc & (IDE_STS_ERR | IDE_STS_DWF)) ? -1 : 0;
}

int
ide_init(void)
{
	int rc;

	outb(IDE_MASTER + IDE_PORT_DRV, IDE_DRV_MASTER_IDENT);
	outb(IDE_MASTER + IDE_PORT_NSECT, 0);
	outb(IDE_MASTER + IDE_PORT_LBA_LO, 0);
	outb(IDE_MASTER + IDE_PORT_LBA_MI, 0);
	outb(IDE_MASTER + IDE_PORT_LBA_HI, 0);
	outb(IDE_MASTER + IDE_PORT_CMD, IDE_CMD_IDENTIFY);

	if (inb(IDE_MASTER + IDE_PORT_STS) == 0) { /* device doesn't exist */
		ide_inited = FALSE;
		return -1;
	}

	do {
		rc = inb(IDE_MASTER + IDE_PORT_STS);
	} while (rc & IDE_STS_BSY);

	do {
		rc = inb(IDE_MASTER + IDE_PORT_STS);
	} while ((rc & (IDE_STS_DRQ | IDE_STS_ERR)) == 0);

	if (rc & IDE_STS_ERR) {
		ide_inited = FALSE;
		return -1;
	}

	insl(IDE_MASTER + IDE_PORT_DATA, ide_info, 512 / 4);

	ide_inited = TRUE;

	KERN_DEBUG("Disk size %lld bytes.\n",
		   (((uint64_t) ide_disk_size_hi() << 32) | ide_disk_size_lo())
		   * DISK_SECT_SIZE);

	return 0;
}

int
ide_disk_read(uint32_t lba_lo, uint32_t lba_hi, void *buf, uint16_t nsectors)
{
	if (ide_inited == FALSE)
		return -1;

	uint64_t lba = ((uint64_t) lba_hi << 32) | lba_lo;
	uint32_t offset = 0;
	uint32_t *__buf = buf;
	int rc;

	outb(IDE_MASTER + IDE_PORT_DRV, IDE_DRV_MASTER_EXT);
	outb(IDE_MASTER + IDE_PORT_NSECT, (nsectors >> 8) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_LO, (lba >> 24) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_MI, (lba >> 32) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_HI, (lba >> 40) & 0xff);
	outb(IDE_MASTER + IDE_PORT_NSECT, nsectors & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_LO, lba & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_MI, (lba >> 8) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_HI, (lba >> 16) & 0xff);
	outb(IDE_MASTER + IDE_PORT_CMD, IDE_CMD_READ_PIO_EXT);

	while (offset < nsectors * DISK_SECT_SIZE / 4) {
		if ((rc = ide_wait_ready()))
			break;
		insl(IDE_MASTER + IDE_PORT_DATA, &__buf[offset],
		     DISK_SECT_SIZE / 4);
		offset += DISK_SECT_SIZE / 4;
		delay();
	}

	if (rc)
		DISK_DEBUG("ide_disk_read() failed: LBA %lld, buf 0x%08x, "
			   "%d sectors, failed sector %lld.\n",
			   lba, buf, nsectors, offset * 4 / DISK_SECT_SIZE);

	return rc;
}

int
ide_disk_write(uint32_t lba_lo, uint32_t lba_hi, void *buf, uint16_t nsectors)
{
	if (ide_inited == FALSE)
		return -1;

	uint64_t lba = ((uint64_t) lba_hi << 32) | lba_lo;
	uint32_t offset = 0;
	uint16_t *__buf = buf;
	int rc;

	outb(IDE_MASTER + IDE_PORT_DRV, IDE_DRV_MASTER_EXT);
	outb(IDE_MASTER + IDE_PORT_NSECT, (nsectors >> 8) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_LO, (lba >> 24) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_MI, (lba >> 32) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_HI, (lba >> 40) & 0xff);
	outb(IDE_MASTER + IDE_PORT_NSECT, nsectors & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_LO, lba & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_MI, (lba >> 8) & 0xff);
	outb(IDE_MASTER + IDE_PORT_LBA_HI, (lba >> 16) & 0xff);
	outb(IDE_MASTER + IDE_PORT_CMD, IDE_CMD_WRITE_PIO_EXT);

	while (offset < nsectors * DISK_SECT_SIZE / 2) {
		if ((rc = ide_wait_ready()))
			break;
		outsw(IDE_MASTER + IDE_PORT_DATA, &__buf[offset], 1);
		delay();
		offset++;
	}

	if (rc)
		DISK_DEBUG("ide_disk_write() failed: LBA %lld, buf 0x%08x, "
			   "%d sectors, failed sector %lld.\n",
			   lba, buf, nsectors, offset * 2 / DISK_SECT_SIZE);
	else
		outb(IDE_MASTER + IDE_PORT_CMD, IDE_CMD_CACHE_FLUSH);

	return rc;
}

uint32_t
ide_disk_size_lo(void)
{
	if (ide_inited == FALSE)
		return 0;
	return ((uint32_t) ide_info[101] << 16) | (uint32_t) ide_info[100] ;
}

uint32_t
ide_disk_size_hi(void)
{
	if (ide_inited == FALSE)
		return 0;
	return ((uint32_t) ide_info[103] << 16) | (uint32_t) ide_info[102];
}

#ifndef _SYS_DEV_IDE_H_
#define _SYS_DEV_IDE_H_

#ifdef _KERN_

#include <lib/types.h>

#define ATA_SECTOR_SIZE		512

int ide_init(void);
int ide_disk_read(uint32_t lba_lo, uint32_t lba_hi, void *buf, uint16_t nsects);
int ide_disk_write(uint32_t lba_lo, uint32_t lba_hi, void *buf, uint16_t nsects);
uint32_t ide_disk_size_lo(void);
uint32_t ide_disk_size_hi(void);

#endif /* _KERN_ */

#endif /* !_SYS_DEV_IDE_H_ */

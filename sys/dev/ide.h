#ifndef _SYS_DEV_IDE_H_
#define _SYS_DEV_IDE_H_

#ifdef _KERN_

#include <sys/types.h>

#define ATA_SECTOR_SIZE		512

int ide_init(void);
int ide_disk_read(uint32_t lba_lo, uint32_t lba_hi,
		  void *buf, uint16_t nsectors);
int ide_disk_write(uint32_t lba_lo, uint32_t lba_hi,
		   void *buf, uint16_t nsectors);
uint32_t ide_disk_size_lo(void);
uint32_t ide_disk_size_hi(void);

#endif /* _KERN_ */

#endif /* !_SYS_DEV_IDE_H_ */

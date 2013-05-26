#ifndef _SYS_DEV_IDE_H_
#define _SYS_DEV_IDE_H_

#ifdef _KERN_

#include <sys/types.h>

#define ATA_SECTOR_SIZE		512

int ide_init(void);
int ide_disk_read(uint64_t lba, void *buf, uint16_t nsectors);
int ide_disk_write(uint64_t lba, void *buf, uint16_t nsectors);
uint64_t ide_disk_size(void);

#endif /* _KERN_ */

#endif /* !_SYS_DEV_IDE_H_ */

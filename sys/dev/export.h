#ifndef _KERN_DEV_EXPORT_H_
#define _KERN_DEV_EXPORT_H_

#ifdef _KERN_

/*
 * Console.
 */

void cons_init(void);

/*
 * CPU
 */

void pcpu_init(void);

/*
 * Pmap.
 */

#include "pmap.h"

/*
 * IDE disk driver.
 */

#define ATA_SECTOR_SIZE	512

int ide_init(void);
int ide_disk_read(uint32_t lba_lo, uint32_t lba_hi, void *buf, uint16_t nsects);
int ide_disk_write(uint32_t lba_lo, uint32_t lba_hi, void *buf, uint16_t nsects);
uint32_t ide_disk_size_lo(void);
uint32_t ide_disk_size_hi(void);

/*
 * Interrupt.
 */

void intr_init(void);
void intr_eoi(void);
void intr_enable(uint8_t irq);

/*
 * SVM.
 */

struct vmcb;

int svm_drv_init(void);
void svm_drv_run_vm(struct vmcb *vmcb, uint32_t *ebx, uint32_t *ecx,
		    uint32_t *edx, uint32_t *esi, uint32_t *edi, uint32_t *ebp);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_EXPORT_H_ */

#ifndef _KERN_DEV_AHCI_H_
#define _KERN_DEV_AHCI_H_

#ifdef _KERN_

#include <dev/pci.h>

uint32_t ide_data_port, ide_stat_port;

void ahci_init(struct pci_func *f);
void ahci_ide_init(struct pci_func *f);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_AHCI_H_ */

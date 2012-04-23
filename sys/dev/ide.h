#ifndef _KERN_DEV_IDE_H_
#define _KERN_DEV_IDE_H_

#ifdef _KERN_

#include <dev/pci.h>

uint32_t ide_port;

void ide_init(struct pci_func *);

#endif /* _KERN_ */

#endif /* !_KERN_DEV_IDE_H_ */

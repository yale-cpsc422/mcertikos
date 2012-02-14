#ifndef _KERN_HVM_PCI_H_
#define _KERN_HVM_PCI_H_

#include <kern/hvm/ioport.h>

#define PCI_CMD_PORT	0x0cf8
#define PCI_DATA_PORT	0x0cfc

void vpci_init(void);

#endif /* !_KERN_HVM_PCI_H_ */

#ifndef _SYS_VIRT_DEV_PCI_H_
#define _SYS_VIRT_DEV_PCI_H_

#ifdef _KERN_

#define PCI_CMD_PORT	0x0cf8
#define PCI_DATA_PORT	0x0cfc

#include <sys/virt/vmm.h>

struct vpci {
};

void vpci_init(struct vpci *, struct vm *);

#endif /* _KERN_ */

#endif /* _SYS_VIRT_DEV_PCI_H_ */

#ifndef _SYS_VIRT_DEV_PCI_H_
#define _SYS_VIRT_DEV_PCI_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>

#define PCI_CONFIG_ADDR	0x0cf8
#define PCI_CONFIG_DATA	0x0cfc

enum data_sz_t;

struct vpci_device {
	void *dev;

	uint32_t (*conf_read)(void *dev,
			      uint32_t addr, enum data_sz_t);
	void (*conf_write)(void *dev,
			   uint32_t addr, uint32_t data, enum data_sz_t);
};

struct vpci_host {
	int bus_id;
	uint32_t config_addr;
	struct vpci_device *dev[32];
};

void vpci_init(struct vpci_host *, struct vm *);
int vpci_attach_device(struct vpci_host *, struct vpci_device *);

#endif /* _KERN_ */

#endif /* _SYS_VIRT_DEV_PCI_H_ */

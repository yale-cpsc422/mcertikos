#ifndef _VDEV_PCI_H_
#define _VDEV_PCI_H_

#include <types.h>

#define PCI_CONFIG_ADDR	0x0cf8
#define PCI_CONFIG_DATA	0x0cfc

struct vpci_device {
	void *dev;

	uint32_t (*conf_read)(void *dev, uint32_t addr, data_sz_t);
	void (*conf_write)(void *dev, uint32_t addr, uint32_t data, data_sz_t);
};

struct vpci_host {
	int bus_id;
	uint32_t config_addr;
	struct vpci_device *dev[32];
};

void vpci_init(struct vpci_host *);
void vpci_handle_ioport(struct vpci_host *, uint16_t, data_sz_t, void *);
int vpci_attach_device(struct vpci_host *, struct vpci_device *);

#endif /* !_VDEV_PCI_H_ */

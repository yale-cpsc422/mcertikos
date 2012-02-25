#ifndef _HVM_PCI_H_
#define _HVM_PCI_H_

#define PCI_CMD_PORT	0x0cf8
#define PCI_DATA_PORT	0x0cfc

/* currently vpci is a dummy structure */
struct vpci {
};

void vpci_init(struct vpci *);

#endif /* _HVM_PCI_H_ */

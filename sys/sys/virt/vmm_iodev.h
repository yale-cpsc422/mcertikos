#ifndef _SYS_VIRT_VMM_IODEV_H_
#define _SYS_VIRT_VMM_IODEV_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>

typedef enum {
	SZ8, 	/* 1 byte */
	SZ16, 	/* 2 byte */
	SZ32	/* 4 byte */
} data_sz_t;

typedef void (*vmm_iodev_write_func)(struct vm *,
				     void *iodev, uint32_t port, void *data);
typedef void (*vmm_iodev_read_func)(struct vm *,
				    void *iodev, uint32_t port, void *data);

int vmm_iodev_register_read(void *iodev, uint32_t port, data_sz_t data_sz,
			    vmm_iodev_read_func port_read);
int vmm_iodev_register_write(void *iodev, uint32_t port, data_sz_t data_sz,
			     vmm_iodev_write_func port_write);

int vmm_iodev_read_port(struct vm *, uint32_t port, void *data);
int vmm_iodev_write_port(struct vm *, uint32_t port, void *data);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_IODEV_H_ */

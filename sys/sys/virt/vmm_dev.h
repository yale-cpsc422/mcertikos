#ifndef _SYS_VIRT_VMM_DEV_H_
#define _SYS_VIRT_VMM_DEV_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>

int vmm_iodev_register_read(struct vm *,
			    void *iodev, uint32_t port, data_sz_t data_sz,
			    iodev_read_func_t port_read);
int vmm_iodev_register_write(struct vm *,
			     void *iodev, uint32_t port, data_sz_t data_sz,
			     iodev_write_func_t port_write);
int vmm_register_extintr(struct vm *, void *iodev,
			 uint8_t intr_no, intr_handle_t);

void vmm_iodev_unregister_read(struct vm *, uint16_t port, data_sz_t);
void vmm_iodev_unregister_write(struct vm *, uint16_t port, data_sz_t);

int vmm_iodev_read_port(struct vm *, uint32_t port, void *data, data_sz_t size);
int vmm_iodev_write_port(struct vm *, uint32_t port, void *data, data_sz_t size);
int vmm_handle_extintr(struct vm *, uint8_t intr_no);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_IODEV_H_ */

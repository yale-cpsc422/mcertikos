#ifndef _USER_VMM_DEV_H_
#define _USER_VMM_DEV_H_

#include <types.h>

#define MAX_IOPORT	(1 << 16)
#define MAX_IRQ		(1 << 8)

typedef int (*in_func_t)(void *dev, uint16_t port, data_sz_t width,
			  void *val);
typedef int (*out_func_t)(void *dev, uint16_t port, data_sz_t width,
			   uint32_t val);

typedef int (*sync_func_t)(void *dev, uint8_t irq);

typedef void (*set_irq_func_t)(void *dev, uint8_t irq, int level);
typedef int (*read_intout_func_t)(void *dev);
typedef int (*get_intout_func_t)(void *dev);

struct vm;

struct vdev {
	struct {
		void *dev;
		in_func_t in;
		out_func_t out;
	} ioport[MAX_IOPORT];

	struct {
		void *dev;
		sync_func_t sync;
	} irq[MAX_IRQ];

	struct {
		void *dev;
		set_irq_func_t set_irq;
		read_intout_func_t read_intout;
		get_intout_func_t get_intout;
	} pic;

	struct vm *vm;
};

int vdev_init(struct vdev *vdev, struct vm *vm);

int vdev_register_ioport(struct vdev *vdev, void *dev, uint16_t port,
			 in_func_t in_f, out_func_t out_f);
int vdev_unregister_ioport(struct vdev *vdev, void *dev, uint16_t port);

int vdev_register_irq(struct vdev *vdev, void *dev, uint8_t irq, sync_func_t f);
int vdev_unregister_irq(struct vdev *vdev, void *dev, uint8_t irq);

int vdev_register_pic(struct vdev *vdev, void *dev, set_irq_func_t set_irq_f,
		      read_intout_func_t read_f, get_intout_func_t get_f);
int vdev_unregister_pic(struct vdev *vdev, void *dev);

void vdev_read_guest_ioport(struct vdev *vdev,
			    uint16_t port, data_sz_t width, uint32_t *val);
void vdev_write_guest_ioport(struct vdev *vdev,
			     uint16_t port, data_sz_t width, uint32_t val);

int vdev_peep_intout(struct vdev *vdev);
int vdev_read_intout(struct vdev *vdev);
void vdev_set_irq(struct vdev *vdev, uint8_t irq, int type);

uint64_t vdev_guest_tsc(struct vdev *vdev);

int vdev_handle_intr(struct vdev *vdev, uint8_t irq);

#endif /* !_USER_VMM_DEV_H_ */

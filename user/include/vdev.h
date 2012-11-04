#ifndef _USER_VDEV_H_
#define _USER_VDEV_H_

#include <types.h>

/*
 * Attach a process as a virtual device to the virtual machine in the same
 * session.
 *
 * @param pid the identity of the process
 *
 * @return the identity of the virtual device if successful; otherwise, return
 *         -1
 */
vid_t vdev_attach_proc(pid_t pid);

/*
 * Detach a virtual device from the virtual machine in the same session
 *
 * @param vid the identity of the virtual device
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_detach_proc(vid_t vid);

/*
 * Attach an I/O port to the virtual device which is emulated by the caller
 * process.
 *
 * @param port  the I/O port the caller intends to attach
 * @param width the width of the I/O port
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_attach_ioport(uint16_t port, data_sz_t width);

/*
 * Detach an I/O port from the virtual device which is emulated by the caller
 * process.
 *
 * @param port  the I/O port the caller intends to detach
 * @param width the width of the I/O port
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_detach_ioport(uint16_t port, data_sz_t width);

/*
 * Attach an IRQ to the virtual device which is emulated by the caller process.
 *
 * @param irq the number of the IRQ the caller intends to attach
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_attach_irq(uint8_t irq);

/*
 * Detach an IRQ from the virtual device which is emulated by the caller process.
 *
 * @param irq the number of the IRQ the caller intends to detach
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_detach_irq(uint8_t irq);

/*
 * Notify the virtual machine the virtual device emulated by the caller process
 * is ready to receive and handle requests.
 *
 * @param chid the identity of the channel which is used to communicate with the
 *             virtual machine.
 *
 * @param 0 if successful; otherwise, return a non-zero value.
 */
int vdev_ready(int chid);

/*
 * Get a request from the virtual machine to the virtual device which is
 * emulated by the caller process.
 *
 * @param chid     the identity of the channel which is used to communicate with
 *                 the virtual machine.
 * @param req      where the received request will be stored
 * @param blocking if non-zero, the function will block the caller process
 *                 until getting a request; if zero, the function will
 *                 return immediately.
 *
 * @return the size (in bytes) of the received request; 0 indicates no request
 *         is requested
 */
size_t vdev_get_request(int chid, void *req, int blocking);

/*
 * Return the data on a guest I/O port, which is attached to the virtual device
 * emulated by the caller process, to the virtual machine.
 *
 * @param chid  the identity of the channel which is used to communicate with
 *              the virtual machine
 * @param port  the guest I/O port
 * @param width the width of the data
 * @param val   the data on the guest I/O port
 *
 * @param 0 if successful; otherwise, return a non-zero value.
 */
int vdev_return_guest_ioport(int chid,
			     uint16_t port, data_sz_t width, uint32_t val);

/*
 * Transfer the data from the virtual machine.
 *
 * @param dest where the data is copied to
 * @param src  the guest physical address where the data is copied from
 * @param size how many bytes are copied
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
int vdev_copy_from_guest(void *dest, uintptr_t src, size_t size);

/*
 * Transfer the data to the virtual machine.
 *
 * @param dest the guest physical address where the data is copied to
 * @oaram src  where the data is copied from
 * @param size how many bytes are copied
 *
 * @param 0 if successful; otherwise, return a non-zero value
 */
int vdev_copy_to_guest(uintptr_t dest, void *src, size_t size);

/*
 * Read the host I/O port. The caller process must have attached the I/O port to
 * the virtual device it emulates.
 *
 * @param port  the host I/O port
 * @param width the width of the data
 *
 * @return the date on the host I/O port
 */
uint32_t vdev_read_host_ioport(uint16_t port, data_sz_t width);

/*
 * Write the host I/O port. The caller process must have attached the I/O port
 * to the virtual device it emulates.
 *
 * @param port  the host I/O port
 * @param width the width of the data
 * @param val   the data to be written to the host I/O port
 */
void vdev_write_host_ioport(uint16_t port, data_sz_t width, uint32_t val);

/*
 * Set the IRQ line of the virtual interrupt controller.
 *
 * @param irq  the number of the IRQ line to be set
 * @param mode 0 - assert the IRQ line;
 *             1 - desert the IRQ line
 */
void vdev_set_irq(uint8_t irq, int mode);

/*
 * Get the value of the guest TSC.
 * @return the value of the guest TSC
 */
uint64_t vdev_guest_tsc(void);

/*
 * Get the frequency of CPU of the virtual machine.
 * @return the value of the frequency
 */
uint64_t vdev_guest_cpufreq(void);

/*
 * Get the size (in bytes) of the physical memory of the virtual machine.
 * @return the size of the guest physical memory
 */
uint64_t vdev_guest_memsize(void);



struct vdev {
	char	desc[48];
	void	*opaque_dev;

	int	(*init)(void *opaque_dev);
	int	(*read_ioport)(void *opaque_dev,
			       uint16_t port, data_sz_t width, void *val);
	int	(*write_ioport)(void *opaque_dev,
				uint16_t port, data_sz_t width, uint32_t val);
	int	(*sync)(void *opaque_dev);
};

/*
 * Initialize a virtual device.
 *
 * @param vdev         the virtual device to be initialized
 * @param opaque_dev   the pointer to the concrete virtual device
 * @param desc         a describing string of the virtual device
 * @param read_ioport  the pointer to the function which handles the read
 *                     requests to the guest I/O ports
 * @param write_ioport the pointer to the fucntion which handles the write
 *                     requests to the guest I/O ports
 * @param sync         the pointer to the function which handles the sync
 *                     requests
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_init(struct vdev *vdev, void *opaque_dev, char *desc,
	      int (*init)(void *),
	      int (*read_ioport)(void *, uint16_t, data_sz_t, void *),
	      int (*write_ioport)(void *, uint16_t, data_sz_t, uint32_t),
	      int (*sync)(void *));

/*
 * Start a virtual device.
 *
 * @param vdev the pointer to the virtual device to be started
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int vdev_start(struct vdev *vdev);

#endif /* !_USER_VDEV_H_ */

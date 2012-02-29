#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_iodev.h>

#define MAX_INTERCEPT_IO	0x10000

static void *vmm_iodev[MAX_INTERCEPT_IO];
static vmm_iodev_read_func iodev_read_table[3][MAX_INTERCEPT_IO];
static vmm_iodev_write_func iodev_write_table[3][MAX_INTERCEPT_IO];

/*
 * Register IO read function for the IO device.
 *
 * @param iodev the pointer to the IO device
 * @param port the port the read function reads
 * @param data_sz the data width the read function reads
 * @param port_read the function pointer to the read function
 *
 * @return 0 if registeration succeeds
 */
int
vmm_iodev_register_read(void *iodev, uint32_t port, data_sz_t data_sz,
			vmm_iodev_read_func port_read)
{
	KERN_ASSERT(iodev != NULL);

	/* KERN_DEBUG("Register function %x to read port %x.\n", */
	/* 	   port_read, port); */

	int i;

	if (vmm_iodev[port] != NULL && vmm_iodev[port] != iodev) {
		KERN_DEBUG("Port %x has already been occupied by IO device %x.\n",
			   port, vmm_iodev[port]);
		return 1;
	}

	vmm_iodev[port] = iodev;

	for (i = 0; i < 3; i++)
		if (iodev_read_table[i][port] != NULL) {
			KERN_WARN("IO read function %x has already been registered for port %x.\n",
				  iodev_read_table[i][port], port);
			KERN_WARN("Replace it with IO read function %x.\n",
				  port_read);

			iodev_read_table[i][port] = NULL;
		}

	iodev_read_table[data_sz][port] = port_read;

	return 0;
}

/*
 * Register IO write function for the IO device.
 *
 * @param iodev the pointer to the IO device
 * @param port the port the write function writes
 * @param data_sz the data width the write function writes
 * @param port_write the function pointer to the write function
 *
 * @return 0 if registeration succeeds
 */
int
vmm_iodev_register_write(void *iodev, uint32_t port, data_sz_t data_sz,
			 vmm_iodev_write_func port_write)
{
	KERN_ASSERT(iodev != NULL);

	/* KERN_DEBUG("Register function %x to write port %x.\n", */
	/* 	   port_write, port); */

	int i;

	if (vmm_iodev[port] != NULL && vmm_iodev[port] != iodev) {
		KERN_DEBUG("Port %x has already been occupied by IO device %x.\n",
			   port, vmm_iodev[port]);
		return 1;
	}

	vmm_iodev[port] = iodev;

	for (i = 0; i < 3; i++)
		if (iodev_write_table[i][port] != NULL) {
			KERN_WARN("IO write function %x has alwritey been registered for port %x.\n",
				  iodev_write_table[i][port], port);
			KERN_WARN("Replace it with IO write function %x.\n",
				  port_write);

			iodev_write_table[i][port] = NULL;
		}

	iodev_write_table[data_sz][port] = port_write;

	return 0;

}

/*
 * Call the registered IO read function to read on port. The result is returned
 * via data and the width (in bytes) of the data is returned via sz.
 *
 * @param port the port to be read
 * @param data the address where the read data is stored
 * @param sz the address where the width (in bytese) of the data is stored
 *
 * @return 0 if read succeeeds
 */
int
vmm_iodev_read_port(struct vm *vm, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(data != NULL);

	int i;

	if (vmm_iodev[port] == NULL) {
		KERN_DEBUG("No IO device was registered on port %x.\n", port);
		return 1;
	}

	for (i = 0; i < 3; i++) {
		vmm_iodev_read_func read_func = iodev_read_table[i][port];

		if (read_func != NULL) {
			read_func(vm, vmm_iodev[port], port, data);
			/* KERN_DEBUG("Read from port %x: func = %x, data = %x.\n", */
			/* 	   port, read_func, *(uint32_t *) data); */
			break;
		}
	}

	if (i == 3) {
		KERN_DEBUG("No IO read function was registered on port %x.\n",
			   port);
		return 1;
	}

	return 0;
}

/*
 * Call the registered IO write function to write on port. The result is
 * returned via data and the width (in bytes) of the data is returned via sz.
 *
 * @param port the port to be write
 * @param data the address where the write data is stored
 * @param sz the address where the width (in bytese) of the data is stored
 *
 * @return 0 if write succeeds
 */
int
vmm_iodev_write_port(struct vm *vm, uint32_t port, void *data)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(data != NULL);

	int i;

	if (vmm_iodev[port] == NULL) {
		KERN_DEBUG("No IO device was registered on port %x.\n", port);
		return 1;
	}

	for (i = 0; i < 3; i++) {
		vmm_iodev_write_func write_func = iodev_write_table[i][port];

		if (write_func != NULL) {
			/* KERN_DEBUG("Write to port %x: func = %x, data = %x.\n", */
			/* 	   port, write_func, *(uint32_t *) data); */
			write_func(vm, vmm_iodev[port], port, data);
			break;
		}
	}

	if (i == 3) {
		KERN_DEBUG("No IO write function was registered on port %x.\n",
			   port);
		return 1;
	}

	return 0;
}

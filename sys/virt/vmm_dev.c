#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

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
vmm_iodev_register_read(struct vm *vm,
			void *iodev, uint32_t port, data_sz_t size,
			iodev_read_func_t port_read)
{
	KERN_ASSERT(vm != NULL && iodev != NULL && port_read != NULL);
	KERN_ASSERT(port < MAX_IOPORT);

	if (vm->iodev[port].dev != NULL && vm->iodev[port].dev != iodev) {
		KERN_DEBUG("Port %x is already occupied by device %x.\n",
			   port, vm->iodev[port].dev);
		return 1;
	}

	if (vm->iodev[port].read_func[size] != NULL) {
		KERN_DEBUG("Function %x is already resgitered to read port %x.\n",
			   vm->iodev[port].read_func[size], port);
		return 1;
	}

	vm->iodev[port].dev = iodev;
	vm->iodev[port].read_func[size] = port_read;

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
vmm_iodev_register_write(struct vm *vm,
			 void *iodev, uint32_t port, data_sz_t size,
			 iodev_write_func_t port_write)
{
	KERN_ASSERT(vm != NULL && iodev != NULL && port_write != NULL);
	KERN_ASSERT(port < MAX_IOPORT);

	if (vm->iodev[port].dev != NULL && vm->iodev[port].dev != iodev) {
		KERN_DEBUG("Port %x is already occupied by device %x.\n",
			   port, vm->iodev[port].dev);
		return 1;
	}

	if (vm->iodev[port].write_func[size] != NULL) {
		KERN_DEBUG("Function %x is already resgitered to write port %x.\n",
			   vm->iodev[port].write_func[size], port);
		return 1;
	}

	vm->iodev[port].dev = iodev;
	vm->iodev[port].write_func[size] = port_write;

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
vmm_iodev_read_port(struct vm *vm, uint32_t port, void *data, data_sz_t size)
{
	KERN_ASSERT(vm != NULL && data != NULL);
	KERN_ASSERT(port < MAX_IOPORT);

	if (vm->iodev[port].dev == NULL) {
		KERN_DEBUG("No IO device was registered on port %x, %d bytes.\n",
			   port, 1<<size);
		return 1;
	}

	if (vm->iodev[port].read_func[size] == NULL) {
		KERN_DEBUG("No read function was registered on port %x, %d bytes.\n",
			   port, 1<<size);
		return 1;
	}

	vm->iodev[port].read_func[size](vm, vm->iodev[port].dev, port, data);

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
vmm_iodev_write_port(struct vm *vm, uint32_t port, void *data, data_sz_t size)
{
	KERN_ASSERT(vm != NULL && data != NULL);
	KERN_ASSERT(port < MAX_IOPORT);

	if (vm->iodev[port].dev == NULL) {
		KERN_DEBUG("No IO device was registered on port %x, %d bytes.\n",
			   port, 1<<size);
		return 1;
	}

	if (vm->iodev[port].write_func[size] == NULL) {
		KERN_DEBUG("No write function was registered on port %x, %d bytes.\n",
			   port, 1<<size);
		return 1;
	}

	vm->iodev[port].write_func[size](vm, vm->iodev[port].dev, port, data);

	return 0;
}

int
vmm_register_extintr(struct vm *vm, void *dev,
		     uint8_t intr_no, intr_handle_t handle)
{
	KERN_ASSERT(vm != NULL && dev != NULL && handle != NULL);
	KERN_ASSERT(intr_no < MAX_EXTINTR);

	if (vm->extintr[intr_no].dev != NULL) {
		KERN_DEBUG("ExtINTR %x is already registered for device %x.\n",
			   intr_no, vm->extintr[intr_no].dev);
		return 1;
	}

	if (vm->extintr[intr_no].handle != NULL) {
		KERN_DEBUG("Handle %x is already registered for ExtINTR %x.\n",
			   vm->extintr[intr_no].handle, intr_no);
		return 1;
	}

	vm->extintr[intr_no].dev = dev;
	vm->extintr[intr_no].handle = handle;

	return 1;
}

int
vmm_handle_extintr(struct vm *vm, uint8_t intr_no)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(intr_no < MAX_EXTINTR);

	if (vm->extintr[intr_no].dev == NULL) {
		KERN_DEBUG("No device is registered for ExtINTR %x.\n",
			   intr_no);
		return 1;
	}

	if (vm->extintr[intr_no].handle == NULL) {
		KERN_DEBUG("No handle is registered for ExtINTR %x.\n",
			   intr_no);
		return 1;
	}

	vm->extintr[intr_no].handle(vm);

	return 0;
}

void
vmm_iodev_unregister_read(struct vm *vm, uint16_t port, data_sz_t size)
{
	KERN_ASSERT(vm != NULL);

	vm->iodev[port].dev = NULL;
	vm->iodev[port].read_func[size] = NULL;
}

void
vmm_iodev_unregister_write(struct vm *vm, uint16_t port, data_sz_t size)
{
	KERN_ASSERT(vm != NULL);

	vm->iodev[port].dev = NULL;
	vm->iodev[port].write_func[size] = NULL;
}

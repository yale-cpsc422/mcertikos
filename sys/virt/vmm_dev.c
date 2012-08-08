#include <sys/debug.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

#ifdef TRACE_IOIO

#include <sys/string.h>

#define MAX_TRACE_IOIO	256

static struct {
	uint16_t port;
	uint64_t read_amount, read_time, read_delta;
	uint64_t write_amount, write_time, write_delta;
} time_slot[MAX_TRACE_IOIO];

static int last_slot = 0;

static void
trace_port(struct vm *vm, uint16_t port)
{
	if (last_slot >= MAX_TRACE_IOIO) {
		KERN_DEBUG("time slots are full,\n");
		return;
	}

	KERN_DEBUG("Add tracing port 0x%x.\n", port);

	memset(&time_slot[last_slot], 0, sizeof(time_slot[last_slot]));
	time_slot[last_slot].port = port;

	vm->iodev[port].tracing = 1;
	vm->iodev[port].slot = last_slot;

	last_slot++;
}

void
dump_ioio_trace_info(void)
{
	int slot;

	KERN_DEBUG("IO trace:\n");

	for (slot = 0; slot < last_slot; slot++) {
		if (time_slot[slot].read_delta > 0) {
			time_slot[slot].read_amount += time_slot[slot].read_delta;

			dprintf("Read 0x%04x: +%lld times, "
				"%lld ticks in total, "
				"%lld ticks/time\n",
				time_slot[slot].port,
				time_slot[slot].read_delta,
				time_slot[slot].read_time,
				time_slot[slot].read_time/time_slot[slot].read_amount);

			time_slot[slot].read_delta = 0;
		}

		if (time_slot[slot].write_delta > 0) {
			time_slot[slot].write_amount += time_slot[slot].write_delta;

			dprintf("Write 0x%04x: +%lld times, "
				"%lld ticks in total, "
				"%lld ticks/time\n",
				time_slot[slot].port,
				time_slot[slot].write_delta,
				time_slot[slot].write_time,
				time_slot[slot].write_time/time_slot[slot].write_amount);

			time_slot[slot].write_delta = 0;
		}
	}
}

#endif

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

#ifdef DEBUG_GUEST_IOIO
	KERN_DEBUG("Register function to read %s from I/O port 0x%x.\n",
		   (size == SZ8) ? "bytes" : (size == SZ16) ? "words" : "dwords",
		   port);
#endif

	if (vm->iodev[port].dev != NULL && vm->iodev[port].dev != iodev) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Port %x is already occupied by device %x.\n",
			   port, vm->iodev[port].dev);
#endif
		return 1;
	}

	if (vm->iodev[port].read_func[size] != NULL) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Function %x is already resgitered to read port %x.\n",
			   vm->iodev[port].read_func[size], port);
#endif
		return 1;
	}

	vm->iodev[port].dev = iodev;
	vm->iodev[port].read_func[size] = port_read;

#ifdef TRACE_IOIO
	if (vm->iodev[port].tracing == 0)
		trace_port(vm, port);
#endif

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

#ifdef DEBUG_GUEST_IOIO
	KERN_DEBUG("Register function to write %s to I/O port 0x%0x.\n",
		   (size == SZ8) ? "bytes" : (size == SZ16) ? "wrods" : "dwords",
		   port);
#endif

	if (vm->iodev[port].dev != NULL && vm->iodev[port].dev != iodev) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Port %x is already occupied by device %x.\n",
			   port, vm->iodev[port].dev);
#endif
		return 1;
	}

	if (vm->iodev[port].write_func[size] != NULL) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("Function %x is already resgitered to write port %x.\n",
			   vm->iodev[port].write_func[size], port);
#endif
		return 1;
	}

	vm->iodev[port].dev = iodev;
	vm->iodev[port].write_func[size] = port_write;

#ifdef TRACE_IOIO
	if (vm->iodev[port].tracing == 0)
		trace_port(vm, port);
#endif

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

#ifdef TRACE_IOIO
	uint64_t start_tsc = 0, end_tsc = 0;
#endif

	if (vm->iodev[port].dev == NULL) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("No IO device was registered on port %x, %d bytes.\n",
			   port, 1<<size);
#endif
		return 1;
	}

	if (vm->iodev[port].read_func[size] == NULL) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("No read function was registered on port %x, %d bytes.\n",
			   port, 1<<size);
#endif
		return 1;
	}

#ifdef TRACE_IOIO
	if (vm->iodev[port].tracing)
		start_tsc = rdtscp();
#endif

	vm->iodev[port].read_func[size](vm, vm->iodev[port].dev, port, data);

#ifdef TRACE_IOIO
	if (vm->iodev[port].tracing) {
		end_tsc = rdtscp();
		time_slot[vm->iodev[port].slot].read_delta++;
		time_slot[vm->iodev[port].slot].read_time +=
			(end_tsc - start_tsc);
	}
#endif

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

#ifdef TRACE_IOIO
	uint64_t start_tsc = 0, end_tsc = 0;
#endif

	if (vm->iodev[port].dev == NULL) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("No IO device was registered on port %x, %d bytes.\n",
			   port, 1<<size);
#endif
		return 1;
	}

	if (vm->iodev[port].write_func[size] == NULL) {
#ifdef DEBUG_GUEST_IOIO
		KERN_DEBUG("No write function was registered on port %x, %d bytes.\n",
			   port, 1<<size);
#endif
		return 1;
	}

#ifdef TRACE_IOIO
	if (vm->iodev[port].tracing)
		start_tsc = rdtscp();
#endif

	vm->iodev[port].write_func[size](vm, vm->iodev[port].dev, port, data);

#ifdef TRACE_IOIO
	if (vm->iodev[port].tracing) {
		end_tsc = rdtscp();
		time_slot[vm->iodev[port].slot].write_delta++;
		time_slot[vm->iodev[port].slot].write_time +=
			(end_tsc - start_tsc);
	}
#endif

	return 0;
}

int
vmm_register_extintr(struct vm *vm, void *dev,
		     uint8_t intr_no, intr_handle_t handle)
{
	KERN_ASSERT(vm != NULL && dev != NULL && handle != NULL);
	KERN_ASSERT(intr_no < MAX_EXTINTR);

	if (vm->extintr[intr_no].dev != NULL) {
#ifdef DEBUG_GUEST_INTR
		KERN_DEBUG("ExtINTR %x is already registered for device %x.\n",
			   intr_no, vm->extintr[intr_no].dev);
#endif
		return 1;
	}

	if (vm->extintr[intr_no].handle != NULL) {
#ifdef DEBUG_GUEST_INTR
		KERN_DEBUG("Handle %x is already registered for ExtINTR %x.\n",
			   vm->extintr[intr_no].handle, intr_no);
#endif
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
#ifdef DEBUG_GUEST_INTR
		KERN_DEBUG("No device is registered for ExtINTR %x.\n",
			   intr_no);
#endif
		return 1;
	}

	if (vm->extintr[intr_no].handle == NULL) {
#ifdef DEBUG_GUEST_INTR
		KERN_DEBUG("No handle is registered for ExtINTR %x.\n",
			   intr_no);
#endif
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

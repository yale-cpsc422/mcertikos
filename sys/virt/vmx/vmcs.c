#include <sys/debug.h>
#include <sys/types.h>

#include "vmcs.h"
#include "x86.h"

/*
 * Read a field in VMCS structure.
 *
 * @param encoding the encoding of the VMCS field
 *
 * @return the value of the field; the higher bits will be zeroed, if
 *         the width of the field is less than 64 bits.
 */
uint64_t
vmcs_read(uint32_t encoding)
{
	KERN_ASSERT((encoding & (uint32_t) 1) == 0);

	uint32_t val_hi, val_lo;
	int error;

	val_lo = val_hi = 0;

	switch (encoding) {
		/* 64-bit fields */
	case VMCS_IO_BITMAP_A ... VMCS_EPTP:
	case VMCS_GUEST_PHYSICAL_ADDRESS:
	case VMCS_LINK_POINTER ... VMCS_GUEST_PDPTE3:
	case VMCS_HOST_IA32_PAT ... VMCS_HOST_IA32_PERF_GLOBAL_CTRL:
		error = vmread(encoding, (uintptr_t) &val_lo);
		if (error)
			break;
		encoding += 1;
		error = vmread(encoding, (uintptr_t) &val_hi);
		break;

		/* 16-bit, 32-bit and natural-width fields */
	default:
		error = vmread(encoding, (uintptr_t) &val_lo);
		break;
	}

	if (error)
		KERN_PANIC("vmcs_read(encoding 0x%08x) error %d.\n",
			   encoding, error);

	return (((uint64_t) val_hi << 32) | (uint64_t) val_lo);
}

/*
 * Write to a fileld in VMCS structure.
 *
 * @param encoding the encoding of the VMCS field
 * @param val the value to be written; the higher bits will be masked as 0's. if
 *            width of the field is less than 64 bits.
 */
void
vmcs_write(uint32_t encoding, uint64_t val)
{
	KERN_ASSERT((encoding & (uint32_t) 1) == 0);

	int error;

	switch (encoding) {
		/* 64-bit fields */
	case VMCS_IO_BITMAP_A ... VMCS_EPTP:
	case VMCS_GUEST_PHYSICAL_ADDRESS:
	case VMCS_LINK_POINTER ... VMCS_GUEST_PDPTE3:
	case VMCS_HOST_IA32_PAT ... VMCS_HOST_IA32_PERF_GLOBAL_CTRL:
		error = vmwrite(encoding, (uint32_t) val);
		if (error)
			break;
		encoding += 1;
		error = vmwrite(encoding, (uint32_t) (val >> 32));
		break;

		/* 16-bit, 32-bit and natural-width fields */
	default:
		error = vmwrite(encoding, (uint32_t) val);
		break;
	}

	if (error)
		KERN_PANIC("vmcs_write(encoding 0x%08x, val 0x%llx) error %d.\n",
			   encoding, val, error);
}

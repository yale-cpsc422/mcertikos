#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>
#include <preinit/lib/x86.h>
#include <preinit/lib/seg.h>

#include "ept.h"
#include "vmcs.h"
#include "vmx.h"
#include "x86.h"

gcc_inline uint16_t
vmcs_read16(uint32_t encoding)
{
	return (uint16_t) vmread(encoding);
}

gcc_inline uint32_t
vmcs_read32(uint32_t encoding)
{
	return vmread(encoding);
}

gcc_inline uint64_t
vmcs_read64(uint32_t encoding)
{
	return vmread(encoding) |
		((uint64_t) vmread(encoding+1) << 32);
}

gcc_inline void
vmcs_write16(uint32_t encoding, uint16_t val)
{
	vmwrite(encoding, val);
}

gcc_inline void
vmcs_write32(uint32_t encoding, uint32_t val)
{
	vmwrite(encoding, val);
}

gcc_inline void
vmcs_write64(uint32_t encoding, uint64_t val)
{
	vmwrite(encoding, val);
	vmwrite(encoding+1, val >> 32);
}


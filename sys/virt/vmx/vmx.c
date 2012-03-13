#include <sys/types.h>

#include <sys/virt/vmm.h>

#include "vmx.h"

struct vmm_ops vmm_ops_intel = {
	.vmm_init	= NULL,
	.vm_init	= NULL,
	.vm_run		= NULL,
	.vm_exit_handle	= NULL,
	.vm_intr_handle = NULL
};

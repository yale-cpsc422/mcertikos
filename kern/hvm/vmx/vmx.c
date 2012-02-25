#include <kern/debug/debug.h>
#include <kern/hvm/vmm.h>
#include <kern/hvm/vmx/vmx.h>

struct vmx {
};

struct vmm_ops vmm_ops_intel = {
	.vmm_init = NULL,
	.vm_init = NULL,
	.vm_run = NULL,
	.vm_handle_exit = NULL
};

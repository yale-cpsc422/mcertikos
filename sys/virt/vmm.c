#include <sys/debug.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pcpu.h>

static bool vmm_inited = FALSE;

static struct vm *cur_vm;

static enum {SVM, VMX} virt_type;
static struct vmm_ops *vmm_ops = NULL;

static bool gcc_inline
is_intel(void)
{
	return (strncmp(cpuinfo.vendor, "GenuineIntel", 20) == 0);
}

static bool gcc_inline
is_amd(void)
{
	return (strncmp(cpuinfo.vendor, "AuthenticAMD", 20) == 0);
}

int
vmm_init(void)
{
	extern struct vmm_ops vmm_ops_intel;
	extern struct vmm_ops vmm_ops_amd;

	if (vmm_inited == TRUE)
		return 0;

	if (is_intel() == TRUE) {
		vmm_ops = &vmm_ops_intel;
		virt_type = VMX;
	} else if (is_amd() == TRUE) {
		vmm_ops = &vmm_ops_amd;
		virt_type = SVM;
	} else {
		KERN_DEBUG("neither Intel nor AMD processor.\n");
		return 1;
	}

	if (vmm_ops->vmm_init() != 0) {
		KERN_DEBUG("machine-dependent vmm_init() failed.\n");
		return 1;
	}

	cur_vm = NULL;

	vmm_inited = TRUE;

	return 0;
}

int
vmm_init_vm(struct vm *vm)
{
	KERN_ASSERT(vmm_inited == TRUE);
	KERN_ASSERT(vm != NULL);

	vm->exit_for_intr = FALSE;

	return vmm_ops->vm_init(vm);
}

int
vmm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vmm_inited == TRUE);
	KERN_ASSERT(vm != NULL);

	cur_vm = vm;

	while (1) {
		vmm_ops->vm_run(vm);
		vmm_ops->vm_handle(vm);
	}

	return 0;
}

struct vm *
vmm_cur_vm(void)
{
	return cur_vm;
}

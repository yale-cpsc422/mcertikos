#include <kern/debug/debug.h>
#include <kern/debug/string.h>

#include <kern/hvm/vmm.h>

#include <architecture/cpu.h>
#include <architecture/types.h>
#include <architecture/x86.h>

static bool vmm_inited = false;

static enum {SVM, VMX} hvm_type;
static struct vmm_ops *vmm_ops = NULL;

static struct vm *cur_vm;

static bool
is_intel(void)
{
	char vendor[20];
	uint32_t dummy, ebx, ecx, edx;

	cpuid(0x0, &dummy, &ebx, &ecx, &edx);
	((uint32_t *) vendor)[0] = ebx;
	((uint32_t *) vendor)[1] = edx;
	((uint32_t *) vendor)[2] = ecx;

	return (strncmp(vendor, "GenuineIntel", 20) == 0);
}

static bool
is_amd(void)
{
	char vendor[20];
	uint32_t dummy, ebx, ecx, edx;

	cpuid(0x0, &dummy, &ebx, &ecx, &edx);
	((uint32_t *) vendor)[0] = ebx;
	((uint32_t *) vendor)[1] = edx;
	((uint32_t *) vendor)[2] = ecx;

	return (strncmp(vendor, "AuthenticAMD", 20) == 0);
}
/*
 * Initialize VMM.
 *
 * XXX: it detects whick kind of processors are present, and then invokes
 *      vmm_init function in vmm_ops structure to do the machine-dependent
 *      VMM initialization.
 *
 * @return 0 if no errors happen
 */
int
vmm_init(void)
{
	extern struct vmm_ops vmm_ops_intel;
	extern struct vmm_ops vmm_ops_amd;

	if (vmm_inited == true)
		return 0;

	if (is_intel() == true) {
		vmm_ops = &vmm_ops_intel;
		hvm_type = VMX;
	} else if (is_amd() == true) {
		vmm_ops = &vmm_ops_amd;
		hvm_type = SVM;
	} else {
		debug("Neither Intel nor AMD processors are detected.\n");
		return 1;
	}

	if (vmm_ops->vmm_init() != 0) {
		debug("Machine-dependent vmm_init() failed.\n");
		return 1;
	}

	cur_vm = NULL;

	vmm_inited = true;

	return 0;
}

/*
 * Initialize VM.
 *
 * @param vm the VM
 *
 * @return 0 if no errors happen
 */
int
vmm_init_vm(struct vm *vm)
{
	assert(vmm_inited == true);
	assert(vm != NULL);

	vm->exit_for_intr = false;

	return vmm_ops->vm_init(vm);
}

/*
 * Run a VM.
 *
 * @param vm the VM
 *
 * @return 0 if no errors happen
 */
int
vmm_run_vm(struct vm *vm)
{
	assert(vmm_inited == true);
	assert(vm != NULL);

	cur_vm = vm;

	while (1) {
		vmm_ops->vm_run(vm);

		/*
		 * If VM exits for interrupts, then enable interrupts to let the
		 * kernel interrupt handlers take control.
		 */
		if (vm->exit_for_intr == true)
			sti();

		vmm_ops->vm_handle_exit(vm);
	}

	return 0;
}

/*
 * Get currently running VM.
 *
 * @return currently running VM
 */
struct vm *
vmm_cur_vm(void)
{
	return cur_vm;
}

#include <sys/debug.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pcpu.h>

static bool vmm_inited = FALSE;

static struct vm vm_pool[4];
static spinlock_t vm_pool_lock;

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

	int i;

	if (vmm_inited == TRUE)
		return 0;

	if (is_intel() == TRUE) {
		vmm_ops = &vmm_ops_intel;
		virt_type = VMX;
	} else if (is_amd() == TRUE) {
		vmm_ops = &vmm_ops_amd;
		virt_type = SVM;
	} else {
		KERN_DEBUG("Neither Intel nor AMD processor.\n");
		return 1;
	}

	if (vmm_ops->vmm_init == NULL || vmm_ops->vmm_init() != 0) {
		KERN_DEBUG("Machine-dependent vmm_init() failed.\n");
		return 1;
	}

	for (i = 0; i < 4; i++) {
		memset(&vm_pool[i], 0x0, sizeof(struct vm));
		vm_pool[i].used = FALSE;
	}

	spinlock_init(&vm_pool_lock);

	cur_vm = NULL;

	vmm_inited = TRUE;

	return 0;
}

struct vm *
vmm_init_vm(void)
{
	KERN_ASSERT(vmm_inited == TRUE);

	int i;
	struct vm *vm = NULL;

	spinlock_acquire(&vm_pool_lock);
	for (i = 0; i < 4; i++)
		if (vm_pool[i].used == FALSE) {
			vm = &vm_pool[i];
			vm->used = TRUE;
			break;
		}
	spinlock_release(&vm_pool_lock);

	if (i == 4)
		return NULL;

	memset(vm, 0x0, sizeof(struct vm));

	vm->exit_for_intr = FALSE;

	/* initializa virtualized devices */
	vpic_init(&vm->vpic, vm);
	vkbd_init(&vm->vkbd, vm);
	vpci_init(&vm->vpci, vm);

	/* machine-dependent VM initialization */
	if (vmm_ops->vm_init(vm) != 0) {
		KERN_DEBUG("Machine-dependent VM initialization failed.\n");
		return NULL;
	}

	return vm;
}

int
vmm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vmm_inited == TRUE);
	KERN_ASSERT(vm != NULL);

	cur_vm = vm;

	while (1) {
		vmm_ops->vm_run(vm);
		/*
		 * If VM exits for interrupts, then enable interrupts in the
		 * host in order to let host interrupt handlers take in.
		 */
		if (vm->exit_for_intr == TRUE)
			sti();
		vmm_ops->vm_handle(vm);
	}

	return 0;
}

struct vm *
vmm_cur_vm(void)
{
	return cur_vm;
}

void
vmm_set_vm_irq(struct vm *vm, int irq, int level)
{
	KERN_ASSERT(vm != NULL);

	vpic_set_irq(&vm->vpic, irq, level);
}

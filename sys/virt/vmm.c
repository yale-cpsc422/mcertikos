#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/dev/virtio_blk.h>

#include <machine/pcpu.h>

#include <dev/tsc.h>

static bool vmm_inited = FALSE;

static struct vm vm_pool[4];
static spinlock_t vm_pool_lock;

static struct vm *cur_vm = NULL;

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

static void
vmm_pre_time_update(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	/* empty currently */
}

static void
vmm_post_time_update(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	/* update guest TSC */
	uint64_t enter_host_tsc = vmm_ops->vm_enter_tsc(vm);
	uint64_t exit_host_tsc = vmm_ops->vm_exit_tsc(vm);
	KERN_ASSERT(enter_host_tsc < exit_host_tsc);
	uint64_t delta = exit_host_tsc - enter_host_tsc;
	uint64_t incr = (delta * VM_TSC_FREQ) / (tsc_per_ms * 1000);
	vm->tsc += (incr - VM_TSC_ADJUST);

	/* update guest PIT */
	vpit_update(vm);
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

int
vmm_init_on_ap(void)
{
        if (vmm_ops->vmm_init == NULL || vmm_ops->vmm_init() != 0) {
                KERN_DEBUG("Machine-dependent vmm_init() failed.\n");
                return 1;
        } else
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

	vm->tsc = 0;

	/* initializa virtualized devices */
	/* XXX: there is no order requirement right now. */
	vpic_init(&vm->vpic, vm);
	vkbd_init(&vm->vkbd, vm);
	vpci_init(&vm->vpci, vm);
	vnvram_init(&vm->vnvram, vm);
	vpit_init(&vm->vpit, vm);
	virtio_blk_init(&vm->blk, vm);

#ifdef REDIRECT_GUEST_SERIAL
	vserial_init(&vm->vserial, vm);
#endif

#ifdef GUEST_DEBUG_DEV
	guest_debug_dev_init(&vm->debug_dev, vm);
#endif

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
		vmm_pre_time_update(vm);

		vmm_ops->vm_run(vm);

		vmm_post_time_update(vm);

		/*
		 * If VM exits for interrupts, then enable interrupts in the
		 * host in order to let host interrupt handlers come in.
		 */
		if (vm->exit_for_intr == TRUE)
			intr_local_enable();

		/*
		 * Wait until the interrupt is handled.
		 * XXX: According to Intel's instruction set reference, STI
		 *      takes effect after the instruction following STI.
		 *      However, it maybe not enough in QEMU+KVM, so following
		 *      loop is added to explictly wait for the effect.
		 * XXX: pause() in the loop body can NOT be removed. pause() is
		 *      actually "__asm __volatile("pause":::"memory")". Note
		 *      the third parameter of the inline assembly tells the
		 *      compilers it may clobber some memory, so that the
		 *      compiler may not treat this loop as an empty loop.
		 *      Otherwise, compilers may merge this loop and above if
		 *      statement as
		 *         if (vm->exit_for_intr == TRUE) {
		 *                 intr_local_enable();
		 *                 while (1)
		 *                         ;
		 *         }
		 *      , which will halt the whole kernel when the if condition
		 *      is satisfied.
		 * XXX: Another solution is to declare exit_for_intr as volatile.
		 *      CertiKOS uses both to prevent compilers doing
		 *      unnecessary optimizations.
		 */
		while (vm->exit_for_intr == TRUE)
			pause();
		/* assertion that makes sure that interrupts are disabled */
		KERN_ASSERT((read_eflags() & FL_IF) == 0x0);
		vmm_ops->vm_exit_handle(vm);
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

void
vmm_handle_intr(struct vm *vm, uint8_t irqno)
{
	KERN_ASSERT(vm != NULL);
	vmm_ops->vm_intr_handle(vm, irqno);
}

uint64_t
vmm_rdtsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	return vm->tsc;
}

void
vmm_update(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	uint32_t port;

	for (port = 0; port < MAX_IOPORT; port++) {
		if (vm->iodev[port].dev == NULL)
			continue;
		int i;
		for (i = 0; i < 3; i++) {
			if (vm->iodev[port].read_func[i] == NULL &&
			    vm->iodev[port].write_func[i] == NULL)
				continue;
			vmm_ops->vm_intercept_ioio(vm, port, i, TRUE);
		}
	}
}

uintptr_t
vmm_translate_gp2hp(struct vm *vm, uintptr_t gp)
{
	KERN_ASSERT(vm != NULL);

	return vmm_ops->vm_translate_gp2hp(vm, gp);
}

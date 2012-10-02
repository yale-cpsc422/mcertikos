#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pic.h>

#include <dev/tsc.h>

static bool vmm_inited = FALSE;

static struct vm vm_pool[4];
static spinlock_t vm_pool_lock;

static enum {SVM, VMX} virt_type;

static bool gcc_inline
is_intel(void)
{
	return (strncmp(pcpu_cur()->arch_info.vendor, "GenuineIntel", 20) == 0);
}

static bool gcc_inline
is_amd(void)
{
	return (strncmp(pcpu_cur()->arch_info.vendor, "AuthenticAMD", 20) == 0);
}

static void
vmm_update_guest_tsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	uint64_t entry_host_tsc, exit_host_tsc, delta, incr;

	entry_host_tsc = vmm_ops->vm_enter_tsc(vm);
	exit_host_tsc = vmm_ops->vm_exit_tsc(vm);

	KERN_ASSERT(exit_host_tsc > entry_host_tsc);

	delta = exit_host_tsc - entry_host_tsc;
	incr = (delta * VM_TSC_FREQ) / (tsc_per_ms * 1000);
	vm->tsc += (incr - VM_TSC_ADJUST);
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

	for (i = 0; i < MAX_VMID; i++) {
		memset(&vm_pool[i], 0x0, sizeof(struct vm));
		vm_pool[i].used = FALSE;
		vm_pool[i].vmid = i;
	}

	spinlock_init(&vm_pool_lock);

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

	vm->exit_reason = EXIT_NONE;
	vm->handled = TRUE;

	vm->tsc = 0;

	/* initialize the virtual PIC */
	vpic_init(&vm->vdev.vpic, vm);

	/* initialize the virtual device interface  */
	vdev_init(vm);

#ifdef TRACE_TOTAL_TIME
	vm->start_tsc = vm->total_tsc = 0;
#endif

	/* machine-dependent VM initialization */
	if (vmm_ops->vm_init(vm) != 0) {
		KERN_DEBUG("Machine-dependent VM initialization failed.\n");
		return NULL;
	}

	pcpu[0].vm = vm;

	return vm;
}

int
vmm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vmm_inited == TRUE);
	KERN_ASSERT(vm != NULL);

#if defined (TRACE_IOIO) || defined (TRACE_TOTAL_TIME)
	static uint64_t last_dump = 0;
#endif

	while (1) {
		KERN_ASSERT(vm->handled == TRUE);

		vmm_ops->vm_run(vm);

		vmm_update_guest_tsc(vm);

		/*
		 * If VM exits for interrupts, then enable interrupts in the
		 * host in order to let host interrupt handlers come in.
		 */
		if (vm->exit_reason == EXIT_FOR_EXTINT && vm->handled == FALSE)
			intr_local_enable();

		/* wait until the external interrupt is handled */
		while (vm->exit_reason == EXIT_FOR_EXTINT &&
		       vm->handled == FALSE)
			pause();
		/* assertion makes sure that interrupts are disabled */
		KERN_ASSERT((read_eflags() & FL_IF) == 0x0);
		vmm_ops->vm_exit_handle(vm);

#if defined (TRACE_IOIO) || defined (TRACE_TOTAL_TIME)
		if ((rdtscp() - last_dump) / tsc_per_ms >= 10 * 1000) {
			last_dump = rdtscp();
#ifdef TRACE_IOIO
			dump_ioio_trace_info();
#endif

#ifdef TRACE_TOTAL_TIME
			dprintf("%lld ms of 10,000 ms are out of the guest.\n",
				vm->total_tsc / tsc_per_ms);
			vm->total_tsc = 0;
#endif
		}
#endif

	}

	return 0;
}

struct vm *
vmm_cur_vm(void)
{
	struct vm *vm;

	if (vmm_inited == FALSE)
		return NULL;

#if 0
	struct proc *p;

	if ((p = proc_cur()) == NULL)
		return NULL;

	proc_lock(p);
	vm = p->vm;
	proc_unlock(p);
#else
	vm = pcpu[0].vm;
#endif

	return vm;
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

uintptr_t
vmm_translate_gp2hp(struct vm *vm, uintptr_t gp)
{
	KERN_ASSERT(vm != NULL);

	return vmm_ops->vm_translate_gp2hp(vm, gp);
}

struct vm *
vmm_get_vm(vmid_t vmid)
{
	if (!(0 <= vmid && vmid < MAX_VMID))
		return NULL;
	return &vm_pool[vmid];
}

void
vmm_set_irq(struct vm *vm, uint8_t irq, int mode)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(0 <= irq && irq < MAX_IRQ);
	KERN_ASSERT(0 <= mode && mode < 3);

	struct vdev *vdev = &vm->vdev;

	spinlock_acquire(&vdev->pic_lk);
	if (mode == 0) {
		vpic_set_irq(&vdev->vpic, irq, 1);
	} else if (mode == 1) {
		vpic_set_irq(&vdev->vpic, irq, 0);
	} else {
		vpic_set_irq(&vdev->vpic, irq, 0);
		vpic_set_irq(&vdev->vpic, irq, 1);
	}
	spinlock_release(&vdev->pic_lk);
}

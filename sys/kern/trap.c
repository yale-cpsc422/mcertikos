#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>
#include <machine/trap.h>
#include <machine/vm.h>

#include <dev/lapic.h>
#include <dev/kbd.h>

void gcc_noreturn
trap(tf_t *tf)
{
	struct proc *p;
	struct context *ctx;
	int vmexit;	/* is a virtual machine running? */
	struct vm *vm;
	trap_cb_t f;

	asm volatile("cld" ::: "cc");

	KERN_ASSERT(tf != NULL);
	if (tf->trapno < T_IRQ0) {
		trap_dump(tf);
		if (tf->trapno == T_PGFLT)
			KERN_DEBUG("Page fault for address 0x%08x.\n", rcr2());
	}

	KERN_ASSERT(VM_USERLO <= tf->eip && tf->eip < VM_USERHI);

	p = proc_cur();

	proc_lock(p);

	ctx = &p->ctx;
	vmexit = (p->vm != NULL && p->vm->exit_for_intr == TRUE);

	KERN_DEBUG("Trap %d from process %d on CPU %d, vmexit=%d.\n",
		   tf->trapno, p->pid, pcpu_cur_idx(), vmexit);

	vm = vmexit ? p->vm : NULL;
	proc_save(p, tf);

	proc_unlock(p);

	f = pcpu_cur()->trap_cb[tf->trapno];

	if (f) {
		f(ctx);
	} else if (vmexit) {
		int irq = tf->trapno - T_IRQ0;

		KERN_ASSERT(vm != NULL);
		KERN_ASSERT(irq >= 0);

		vmm_handle_intr(vm, irq);
	} else {
		KERN_WARN("NO handler registered for trap %d.\n", tf->trapno);
	}

	if (vmexit) {
		KERN_DEBUG("Re-enter VM\n");
		KERN_ASSERT(vm != NULL);
		vmm_run_vm(vm);
	}

	ctx->tf.eflags &= ~FL_IF;
	proc_run();
}

void
trap_handler_register(int trapno, trap_cb_t cb)
{
	KERN_ASSERT(0 <= trapno && trapno < 256);
	KERN_ASSERT(cb != NULL);

	pcpu_cur()->trap_cb[trapno] = cb;
}

int
default_exception_handler(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("Exception %d caused by process %d on CPU %d.\n",
		   ctx->tf.trapno, proc_cur()->pid, pcpu_cur_idx());
	ctx_dump(ctx);

	KERN_PANIC("Stop here.\n");

	return 0;
}

int
gpf_handler(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("General protection fault caused by process %d on CPU %d.\n",
		   proc_cur()->pid, pcpu_cur_idx());
	ctx_dump(ctx);

	/*
	 * TODO: kill the process which caused the fault instead of kernel panic
	 */
	KERN_PANIC("Stop here.\n");

	return 0;
}

int
pgf_handler(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

	struct proc *p = proc_cur();
	KERN_ASSERT(p != NULL);

	uint32_t errno = ctx_errno(ctx);
	uintptr_t fault_va = rcr2();

	KERN_DEBUG("Page fault at 0x%08x caused by process %d on CPU %d.\n",
		   fault_va, p->pid, pcpu_cur_idx());
	/* trap_dump(&ctx->tf); */

	if (errno & PFE_PR) {
		/*
		 * TODO: kill the process instead of kernel panic
		 */
		KERN_PANIC("Page fault caused for permission violation.\n");
		return 1;
	}

	proc_lock(p);

	KERN_ASSERT(p->pmap == (pmap_t *) rcr3());

	if (!pmap_reserve(p->pmap, (uintptr_t) PGADDR(fault_va),
			  PTE_W | PTE_U | PTE_P)) {
		/*
		 * TODO: kill the process instead of kernel panic
		 */
		KERN_PANIC("Cannot allocate physical memory for 0x%x\n",
			   fault_va);
		return 1;
	}

	proc_unlock(p);

	KERN_DEBUG("Page fault at 0x%08x, process %d on CPU %d is handled.\n",
		   fault_va, p->pid, pcpu_cur_idx());

	return 0;
}

int
spurious_intr_handler(struct context *ctx)
{
	KERN_DEBUG("Ignore spurious interrupt.\n");
	/* XXX: do not send EOI for spurious interrupts */
	return 0;
}

int
timer_intr_handler(struct context *ctx)
{
	/* timer_handle_timeout(); */

	struct vm *vm = vmm_cur_vm();
	bool from_guest =
		(vm != NULL && vm->exit_for_intr == TRUE) ? TRUE : FALSE;

	if (from_guest == TRUE) {
		vmm_handle_intr(vm, IRQ_TIMER);
	} else {
		/*
		 * XXX: If the current process is running a virtual machine,
		 *      don't do the process schedule.
		 */
		proc_sched(pcpu_cur());
	}

	intr_eoi();

	return 0;
}

int
kbd_intr_handler(struct context *ctx)
{
	KERN_DEBUG("master_kbd_handler\n");

	struct vm *vm = vmm_cur_vm();
	bool from_guest =
		(vm != NULL && vm->exit_for_intr == TRUE) ? TRUE : FALSE;

	if (from_guest != TRUE) { /* for a normal application */
		kbd_intr();
	} else /* for a guest */
		vmm_handle_intr(vm, IRQ_KBD);

	intr_eoi();

	return 0;
}

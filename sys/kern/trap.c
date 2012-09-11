#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/string.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

#include <machine/pmap.h>
#include <machine/vm.h>

#include <dev/kbd.h>
#include <dev/lapic.h>
#include <dev/tsc.h>

static gcc_inline bool
trap_from_guest(tf_t *tf)
{
	return (tf->eip < VM_USERLO &&
		proc_cur()->vm != NULL &&
		proc_cur()->vm->exit_reason == EXIT_FOR_EXTINT &&
		proc_cur()->vm->handled == FALSE) ? TRUE : FALSE;
}

/*
 * Trap handler entry for traps from userspace.
 */
static gcc_inline gcc_noreturn void
trap_user(tf_t *tf)
{
	KERN_ASSERT(tf->eip >= VM_USERLO && tf->eip < VM_USERHI);

	struct proc *cur_proc;
	trap_cb_t f;

	cur_proc = proc_cur();

	/* save the user context */
	proc_save(cur_proc, tf);

	/* call the specific trap handler */
	f = (*pcpu_cur()->trap_handler)[tf->trapno];
	if (f)
		f(&cur_proc->ctx);
	else
		KERN_WARN("No handler for user trap %d.\n", tf->trapno);

	/*
	 * XXX: f may call the scheduler, so the current process maybe changed.
	 */
	cur_proc = proc_cur();

	/* return to the userspace */
	ctx_start(&cur_proc->ctx);
}

/*
 * Trap handler entry for traps from kernel.
 *
 * XXX: In CertiKOS, traps from the kernel only consist of the external
 *      interrupts from a running guest. All exceptions from the kernel
 *      are invalid.
 */
static gcc_inline gcc_noreturn void
trap_kern(tf_t *tf)
{
	if (tf->trapno < T_IRQ0) {
		trap_dump(tf);
		KERN_PANIC("Exeception %d from kernel.\n", tf->trapno);
	}

	KERN_ASSERT(vmm_cur_vm() != NULL);
	KERN_ASSERT(trap_from_guest(tf) == TRUE);

	struct vm *vm;
	trap_cb_t f;
	int irq;

#ifdef DEBUG_GUEST_INTR
	KERN_DEBUG("ExtINTR %d from guest.\n", tf->trapno - T_IRQ0);
#endif

	f = (*pcpu_cur()->trap_handler)[tf->trapno];

	if (f) {
		f(NULL);
	} else {
		/*
		 * If no handler is registered for the interrupt, call
		 * the default handler in the virtualization module.
		 */
		vm = vmm_cur_vm();
		KERN_ASSERT(vm != NULL);
		KERN_ASSERT(vm->exit_reason == EXIT_FOR_EXTINT &&
			    vm->handled == FALSE);

		irq = tf->trapno - T_IRQ0;
		KERN_ASSERT(irq >= 0);

		vmm_handle_intr(vm, irq);
	}

	/*
	 * Make sure the interrupts are disabled when returning from the
	 * interrupt.
	 */
	tf->eflags &= ~(uint32_t) FL_IF;
	trap_return(tf);
}

/*
 * Top-level trap handler entry.
 */
gcc_noreturn void
trap(tf_t *tf)
{
	asm volatile("cld" ::: "cc");

	KERN_ASSERT(tf != NULL);

	if (tf->trapno < T_IRQ0) {
		if (tf->trapno == T_PGFLT)
			KERN_DEBUG("Page fault @ 0x%08x on CPU%d.\n",
				   rcr2(), pcpu_cpu_idx(pcpu_cur()));
		/* trap_dump(tf); */
	} else if (tf->trapno == T_IRQ0+IRQ_IPI_RESCHED) {
		/* KERN_DEBUG("IRQ_IPI_RESCHED on CPU%d, EIP 0x%08x.\n", */
		/* 	   pcpu_cpu_idx(pcpu_cur()), tf->eip); */
		/* trap_dump(tf); */
	}

	if (tf->eip >= VM_USERLO)
		trap_user(tf);
	else
		trap_kern(tf);
}

void
trap_init_array(struct pcpu *c)
{
	KERN_ASSERT(c != NULL);
	KERN_ASSERT(c->inited == TRUE);

	int npages;
	pageinfo_t *pi;

	npages = ROUNDUP(sizeof(trap_cb_t) * T_MAX, PAGESIZE) / PAGESIZE;
	if ((pi = mem_pages_alloc(npages)) == NULL)
		KERN_PANIC("Cannot allocate memory for trap handlers.\n");

	spinlock_acquire(&c->lk);
	c->trap_handler = (trap_cb_t **) mem_pi2phys(pi);
	memzero(c->trap_handler, npages * PAGESIZE);
	spinlock_release(&c->lk);
}

void
trap_handler_register(int trapno, trap_cb_t cb)
{
	KERN_ASSERT(0 <= trapno && trapno < T_MAX);
	KERN_ASSERT(cb != NULL);

	(*pcpu_cur()->trap_handler)[trapno] = cb;
}

int
default_exception_handler(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("Exception %d caused by process %d on CPU %d.\n",
		   ctx->tf.trapno, proc_cur()->pid, pcpu_cpu_idx(pcpu_cur()));
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
		   proc_cur()->pid, pcpu_cpu_idx(pcpu_cur()));
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
		   fault_va, p->pid, pcpu_cpu_idx(pcpu_cur()));
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
		   fault_va, p->pid, pcpu_cpu_idx(pcpu_cur()));

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

	intr_eoi();

	struct vm *vm = vmm_cur_vm();
	bool from_guest = trap_from_guest(&ctx->tf);

	if (from_guest == TRUE) {
		vmm_handle_intr(vm, IRQ_TIMER);
	} else {
		proc_sched_update();

		struct pcpu *c = pcpu_cur();
		sched_lock(c);
		if (c->sched.run_ticks > SCHED_SLICE) {
			/* KERN_DEBUG("Resched on CPU%d (run ticks %lld).\n", */
			/* 	   pcpu_cpu_idx(pcpu_cur()), c->sched.run_ticks); */
			proc_sched(FALSE);
		}
		sched_unlock(c);
	}

	return 0;
}

int
kbd_intr_handler(struct context *ctx)
{
	intr_eoi();

	struct vm *vm = vmm_cur_vm();
	bool from_guest = trap_from_guest(&ctx->tf);

	if (from_guest != TRUE)
		kbd_intr();
	else
		vmm_handle_intr(vm, IRQ_KBD);

	return 0;
}

int
ipi_resched_handler(struct context *ctx)
{
	intr_eoi();

	if (trap_from_guest(&ctx->tf)) {
		vmm_cur_vm()->handled = TRUE;
	} else {
		sched_lock(pcpu_cur());
		proc_sched(TRUE);
		sched_unlock(pcpu_cur());
	}

	return 0;
}

int
ipi_vintr_handler(struct context *ctx)
{
	intr_eoi();

	if (trap_from_guest(&ctx->tf))
		vmm_cur_vm()->handled = TRUE;

	return 0;
}

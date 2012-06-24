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
	KERN_ASSERT(tf != NULL);

	/* KERN_DEBUG("Trap %d on CPU %d.\n", tf->trapno, pcpu_cur_idx()); */

	static struct context kern_ctx = { .p = NULL };
	struct context *ctx = NULL;

	asm volatile("cld" ::: "cc");

	/* if (tf->trapno < T_IRQ0) /\* exceptions *\/ */
	/* 	trap_dump(tf); */

	if (tf->eip >= VM_USERLO) {
		/* save the context of the interrupted process */
		KERN_ASSERT(tf->eip < VM_USERHI);

		struct proc *p = proc_cur();

		KERN_ASSERT(p != NULL);

		proc_lock(p);
		ctx = &p->ctx;
		proc_unlock(p);

		proc_save(p, tf);
	} else {
		/*
		 * The trap is from the kernel space. If a trap happens while a
		 * virtual machine is running on this processor and CertiKOS
		 * successfully intercepts the interrupt (and enters the kernel
		 * space), CertiKOS will temporally enable interrupts on this
		 * processor (by setting RFLAGS.IF of host) and finally reach
		 * here. In this way, CertiKOS can know which interrupt it
		 * intercepted (via the trapframe).
		 *
		 * This maybe the only way that CertiKOS can get the detailed
		 * information of the intercepted interrupts, especially when
		 * the hardware (e.g. AMD-V, or Intel VMX with acknowledge_
		 * interrupt_on_exit clear does not provide.
		 */
		ctx = &kern_ctx;
		ctx->tf = *tf;
	}

	trap_cb_t f = pcpu_cur()->trap_cb[tf->trapno];

	if (f) { /* a handler is registered for this trap */
		f(ctx);
	} else {
		if (tf->eip >= VM_USERLO) {
			KERN_WARN("No handler registered for trap %d.\n",
				  tf->trapno);
		} else {
			/*
			 * If the interrupt is from guest and the host has no
			 * handler for it, just let VMM handle the interrupt.
			 */
			struct vm *vm = vmm_cur_vm();
			int irq = tf->trapno - T_IRQ0;
			KERN_ASSERT(vm != NULL && irq >= 0);
			vmm_handle_intr(vm, irq);
		}
	}

	if (tf->eip >= VM_USERLO) {
		proc_run();
	} else {
		/* clear RFLAGS.IF to avoid nesting traps */
		ctx->tf.eflags &= ~(uint32_t) FL_IF;
		ctx_start(ctx);
	}
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

	if (from_guest == TRUE)
		vmm_handle_intr(vm, IRQ_TIMER);

	proc_sched(pcpu_cur());

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

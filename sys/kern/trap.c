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

static gcc_inline int
trap_from_guest(tf_t *tf)
{
	struct vm *vm = vmm_cur_vm();
	return tf && tf->eip < VM_USERLO &&
		vm && vm->state == VM_RUNNING &&
		vm->exit_reason == EXIT_FOR_EXTINT && vm->handled == FALSE;
}

static gcc_inline void
pre_handle_guest(tf_t *tf)
{
	KERN_ASSERT(tf->trapno >= T_IRQ0);
#ifdef DEBUG_GUEST_INTR
	KERN_DEBUG("External interrupt 0x%x from guest on CPU%d.\n",
		   tf->trapno - T_IRQ0, pcpu_cpu_idx(pcpu_cur()));
#endif
}

static gcc_inline  void
pre_handle_user(tf_t *tf)
{
	if (!(tf->eip >= VM_USERLO && tf->eip < VM_USERHI))
		KERN_PANIC("trapno %d, eip 0x%08x, 0x%08x.\n",
			   tf->trapno, tf->eip, rcr2());
	KERN_ASSERT(tf->eip >= VM_USERLO && tf->eip < VM_USERHI);
	struct proc *cur_p = proc_cur();
	KERN_ASSERT(cur_p);
	proc_save(cur_p, tf);		/* save the user context */
	pmap_install(pmap_kern);	/* switch to kernel pmap */
}

static gcc_inline void
default_handler_guest(tf_t *tf)
{
	vmm_handle_intr(vmm_cur_vm(), tf->trapno - T_IRQ0);
}

static gcc_inline void
default_handler_user(tf_t *tf)
{
	KERN_WARN("No handler for user trap 0x%x.\n", tf->trapno);
}

static gcc_inline gcc_noreturn void
post_handle_guest(tf_t *tf)
{
	tf->eflags &= ~(uint32_t) FL_IF;
	trap_return(tf);
}

static gcc_inline gcc_noreturn void
post_handle_user(tf_t *tf)
{
	struct proc *cur_p = proc_cur();
	KERN_ASSERT(cur_p);
	pmap_install(cur_p->pmap);
	ctx_start(&cur_p->ctx);
}

gcc_noreturn void
trap(tf_t *tf)
{
	asm volatile("cld" ::: "cc");

	KERN_ASSERT(tf != NULL);

	trap_cb_t f;
	int guest = trap_from_guest(tf);

	if (guest)
		pre_handle_guest(tf);
	else
		pre_handle_user(tf);

	f = (*pcpu_cur()->trap_handler)[tf->trapno];

	if (f) {
		f(&proc_cur()->ctx, guest);
	} else {
		if (guest)
			default_handler_guest(tf);
		else
			default_handler_user(tf);
	}

	if (guest)
		post_handle_guest(tf);
	else
		post_handle_user(tf);
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
default_exception_handler(struct context *ctx, int guest)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);
	KERN_ASSERT(!guest);

	KERN_DEBUG("Exception %d caused by process %d on CPU %d.\n",
		   ctx->tf.trapno, proc_cur()->pid, pcpu_cpu_idx(pcpu_cur()));
	ctx_dump(ctx);

	KERN_PANIC("Stop here.\n");

	return 0;
}

int
gpf_handler(struct context *ctx, int guest)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);
	KERN_ASSERT(!guest);

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
pgf_handler(struct context *ctx, int guest)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(!guest);

	struct proc *p = proc_cur();
	KERN_ASSERT(p != NULL);

	uint32_t errno = ctx_errno(ctx);
	uintptr_t fault_va = rcr2();

	/* KERN_DEBUG("Page fault at 0x%08x caused by process %d on CPU %d.\n", */
	/* 	   fault_va, p->pid, pcpu_cpu_idx(pcpu_cur())); */
	/* trap_dump(&ctx->tf); */

	if (errno & PFE_PR) {
		/*
		 * TODO: kill the process instead of kernel panic
		 */
		KERN_PANIC("Page fault caused for permission violation.\n");
		return 1;
	}

	proc_lock(p);

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
spurious_intr_handler(struct context *ctx, int guest)
{
	KERN_DEBUG("Ignore spurious interrupt.\n");
	/* XXX: do not send EOI for spurious interrupts */
	return 0;
}

/*
 * CertiKOS uses LAPIC timers as the sources of the timer interrupts. This
 * function only handles the timer interrupts from the processor on which
 * it's running.
 *
 * If there is a virtual machine running on the processor when timer interrupts
 * come, this function will use the interrupt handler provided by VMM to handle
 * them (which may result in injecting the timer interrupts to the virtual
 * machine).
 *
 * Otherwise, handle the interrupts as normal.
 */
int
timer_intr_handler(struct context *ctx, int guest)
{
	intr_eoi();

	if (guest) {
		vmm_handle_intr(vmm_cur_vm(), IRQ_TIMER);
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

/*
 * CertiKOS redirects the keyboard interrupts to BSP.
 *
 * If there's a virtual machine running when the keyboard interrupts come, this
 * function will use the interrupt handler provided by VMM to handle them (which
 * may result in injecting the keyboard interrupts to the virtual machine).
 *
 * Otherwise, handle the interrupts as normal.
 */
int
kbd_intr_handler(struct context *ctx, int guest)
{
	intr_eoi();

	if (guest)
		vmm_handle_intr(vmm_cur_vm(), IRQ_KBD);
	else
		kbd_intr();

	return 0;
}

/*
 * CertiKOS uses IRQ_IPI_RESCHED to force the scheduler on another processor to
 * reschedule.
 */
int
ipi_resched_handler(struct context *ctx, int guest)
{
	intr_eoi();

	if (guest) {
		vmm_handle_intr(vmm_cur_vm(), IRQ_IPI_RESCHED);
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
	return 0;
}

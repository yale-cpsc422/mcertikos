#include <sys/context.h>
#include <sys/debug.h>
#include <sys/pmap.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/string.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include <dev/intr.h>
#include <dev/kbd.h>
#include <dev/lapic.h>
#include <dev/pcpu.h>
#include <dev/serial.h>
#include <dev/tsc.h>

static gcc_inline void
default_intr_handler(int trapno, struct context *ctx)
{
	KERN_ASSERT(trapno >= T_IRQ0);
	KERN_WARN("No handler for IRQ 0x%x, process %d, eip 0x%08x.\n",
		  trapno-T_IRQ0, proc_cur()->pid, ctx->tf.eip);
	intr_eoi();
}

static gcc_inline void
default_exception_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("Exception %d caused by process %d.\n",
		   ctx->tf.trapno, proc_cur()->pid);
	ctx_dump(ctx);

	/*
	 * TODO: kill the process which caused the fault instead of kernel panic
	 */
	KERN_PANIC("Stop here.\n");
}

static gcc_inline int
gpf_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	KERN_ASSERT(proc_cur() != NULL);

	KERN_DEBUG("General protection fault caused by process %d.\n",
		   proc_cur()->pid);
	ctx_dump(ctx);

	/*
	 * TODO: kill the process which caused the fault instead of kernel panic
	 */
	KERN_PANIC("Stop here.\n");
}

static gcc_inline void
pgf_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

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
		trap_dump(&ctx->tf);
		KERN_PANIC("Page fault caused for permission violation "
			   "(va 0x%08x).\n", fault_va);
	}

	proc_lock(p);

	if (!pmap_reserve(p->pmap, (uintptr_t) PGADDR(fault_va),
			  PTE_W | PTE_U | PTE_P)) {
		/*
		 * TODO: kill the process instead of kernel panic
		 */
		KERN_PANIC("Cannot allocate physical memory for 0x%x\n",
			   fault_va);
	}

	proc_unlock(p);

	KERN_DEBUG("Page fault at 0x%08x, process %d is handled.\n",
		   fault_va, p->pid);
}

static gcc_inline void
spurious_intr_handler(uint8_t trapno, struct context *ctx)
{
	KERN_DEBUG("Ignore spurious interrupt.\n");
	/* XXX: do not send EOI for spurious interrupts */
}

/*
 * CertiKOS uses LAPIC timers as the sources of the timer interrupts. This
 * function only handles the timer interrupts from the processor on which
 * it's running.
 */
static gcc_inline void
timer_intr_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	sched_update();
}

static gcc_inline void
kbd_intr_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	/* kbd_intr(); */
}

static gcc_inline void
serial_intr_handler(uint8_t trapno, struct context *ctx)
{
	intr_eoi();
	/* serial_intr(); */
}

void
trap(tf_t *tf)
{
	cld();

	KERN_ASSERT(tf != NULL);
	KERN_ASSERT(VM_USERLO <= tf->eip && tf->eip < VM_USERHI);

	trap_cb_t f;
	struct proc *cur_p = proc_cur();
	struct pcpu *cur_c = pcpu_cur();

	KERN_ASSERT(cur_p);

	proc_save(cur_p, tf);
	pmap_install(pmap_kern_map());

	switch (tf->trapno) {
	case T_GPFLT:
		gpf_handler(tf->trapno, &cur_p->uctx);
		break;
	case T_PGFLT:
		pgf_handler(tf->trapno, &cur_p->uctx);
		break;
	case T_SYSCALL:
		syscall_handler(tf->trapno, &cur_p->uctx);
		break;
	case T_IRQ0+IRQ_SPURIOUS:
		spurious_intr_handler(tf->trapno, &cur_p->uctx);
		break;
	case T_IRQ0+IRQ_TIMER:
		timer_intr_handler(tf->trapno, &cur_p->uctx);
		break;
	case T_IRQ0+IRQ_KBD:
		kbd_intr_handler(tf->trapno, &cur_p->uctx);
		break;
	case T_IRQ0+IRQ_SERIAL13:
		serial_intr_handler(tf->trapno, &cur_p->uctx);
		break;
	default:
		if (tf->trapno < T_IRQ0)
			default_exception_handler(tf->trapno, &cur_p->uctx);
		else
			default_intr_handler(tf->trapno, &cur_p->uctx);
	}

	ctx_start(&cur_p->uctx);
}

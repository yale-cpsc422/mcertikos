#include <sys/context.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <machine/pmap.h>
#include <machine/trap.h>

/*
 * Initialize the context of a process.
 */
void
ctx_init(struct proc *p, void (*entry)(void), uintptr_t stack)
{
	KERN_ASSERT(p != NULL && entry != NULL);

	struct context *ctx = &p->uctx;
	tf_t *tf = &ctx->tf;

	ctx->p = p;

	/* setup segment registers */
	tf->es = CPU_GDT_UDATA | 3;
	tf->ds = CPU_GDT_UDATA | 3;
	tf->cs = CPU_GDT_UCODE | 3;
	tf->ss = CPU_GDT_UDATA | 3;

	/* setup the entry point */
	tf->eip = (uintptr_t) entry;

	/* setup the one-page stack  */
	tf->esp = (uintptr_t) stack + PAGE_SIZE;

	/* enable interrupt */
	tf->eflags = FL_IF;
}

/*
 * Resume/Start executing the context.
 */
void
ctx_start(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

	struct proc *cur_p = proc_cur();
	KERN_ASSERT(cur_p != NULL);
	pmap_install(cur_p->pmap);

	tf_t *tf = &ctx->tf;

	if (ctx->p != NULL) {
		KERN_ASSERT(spinlock_holding(&ctx->p->proc_lk) == FALSE);
	} else {
		KERN_ASSERT(tf->eip < VM_USERLO);
		KERN_ASSERT(tf->eflags & FL_IF);
	}

	trap_return(tf);
}

uint32_t
ctx_errno(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.err;
}

uint32_t
ctx_arg1(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.regs.eax;
}

uint32_t
ctx_arg2(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.regs.ebx;
}

uint32_t
ctx_arg3(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.regs.ecx;
}

uint32_t
ctx_arg4(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.regs.edx;
}

void
ctx_set_retval(struct context *ctx, uint32_t rc)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.eax = rc;
}

void
ctx_dump(struct context *ctx)
{
	if (ctx == NULL)
		return;

	trap_dump(&ctx->tf);
}

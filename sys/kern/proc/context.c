#include <lib/export.h>

#include "context.h"

/*
 * Initialize the context of a process.
 */
void
ctx_init(struct context *ctx, uintptr_t entry, uintptr_t stack_top)
{
	KERN_ASSERT(ctx != NULL);

	tf_t *tf = &ctx->tf;

	/* setup segment registers */
	tf->es = 0x20 | 3;	/* CPU_GDT_UDATA | 3 */
	tf->ds = 0x20 | 3;	/* CPU_GDT_UDATA | 3 */
	tf->cs = 0x18 | 3;	/* CPU_GDT_UCODE | 3 */
	tf->ss = 0x20 | 3;	/* CPU_GDT_UDATA | 3 */

	/* setup the entry point */
	tf->eip = entry;

	/* setup the one-page stack  */
	tf->esp = stack_top;

	/* enable interrupt */
	tf->eflags = 0x00000200;/* FL_IF */
}

/*
 * Resume/Start executing the context.
 */
void
ctx_start(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	trap_return(&ctx->tf);
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

uint32_t
ctx_arg5(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.regs.esi;
}

uint32_t
ctx_arg6(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);
	return ctx->tf.regs.edi;
}

void
ctx_set_errno(struct context *ctx, uint32_t errno)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.eax = errno;
}

void
ctx_set_retval1(struct context *ctx, uint32_t ret)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.ebx = ret;
}

void
ctx_set_retval2(struct context *ctx, uint32_t ret)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.ecx = ret;
}

void
ctx_set_retval3(struct context *ctx, uint32_t ret)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.edx = ret;
}

void
ctx_set_retval4(struct context *ctx, uint32_t ret)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.esi = ret;
}

void
ctx_set_retval5(struct context *ctx, uint32_t ret)
{
	KERN_ASSERT(ctx != NULL);
	ctx->tf.regs.edi = ret;
}

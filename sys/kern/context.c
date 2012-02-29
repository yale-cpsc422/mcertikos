#include <sys/types.h>
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/trap.h>
#include <sys/x86.h>

/* Entries of interrupt handlers, defined in trapasm.S */
extern char Xdivide,Xdebug,Xnmi,Xbrkpt,Xoflow,Xbound,
	Xillop,Xdevice,Xdblflt,Xtss,Xsegnp,Xstack,
	Xgpflt,Xpgflt,Xfperr,Xalign,Xmchk,Xdefault,Xsyscall;
extern char Xirq0,Xirq1,Xirq2,Xirq3,Xirq4,Xirq5,
	Xirq6,Xirq7,Xirq8,Xirq9,Xirq10,Xirq11,
	Xirq12,Xirq13,Xirq14,Xirq15,Xirq19;

volatile bool context_inited = FALSE;

static context_t *__context_cur[MAX_CPU];

/* Interrupt Descriptors Table */
gatedesc_t idt[256];
pseudodesc_t idt_pd =
	{
		.pd_lim = sizeof(idt) - 1,
		.pd_base = (uint32_t) idt
	};

static void
context_init_idt()
{
	int i;

	/* check that T_IRQ0 is a multiple of 8 */
	KERN_ASSERT((T_IRQ0 & 7) == 0);

	/* install a default handler */
	for (i = 0; i < sizeof(idt)/sizeof(idt[0]); i++)
		SETGATE(idt[i], 0, CPU_GDT_KCODE, &Xdefault, 0);

	SETGATE(idt[T_DIVIDE], 0, CPU_GDT_KCODE, &Xdivide, 0);
	SETGATE(idt[T_DEBUG],  0, CPU_GDT_KCODE, &Xdebug,  0);
	SETGATE(idt[T_NMI],    0, CPU_GDT_KCODE, &Xnmi,    0);
	SETGATE(idt[T_BRKPT],  0, CPU_GDT_KCODE, &Xbrkpt,  3);
	SETGATE(idt[T_OFLOW],  0, CPU_GDT_KCODE, &Xoflow,  3);
	SETGATE(idt[T_BOUND],  0, CPU_GDT_KCODE, &Xbound,  0);
	SETGATE(idt[T_ILLOP],  0, CPU_GDT_KCODE, &Xillop,  0);
	SETGATE(idt[T_DEVICE], 0, CPU_GDT_KCODE, &Xdevice, 0);
	SETGATE(idt[T_DBLFLT], 0, CPU_GDT_KCODE, &Xdblflt, 0);
	SETGATE(idt[T_TSS],    0, CPU_GDT_KCODE, &Xtss,    0);
	SETGATE(idt[T_SEGNP],  0, CPU_GDT_KCODE, &Xsegnp,  0);
	SETGATE(idt[T_STACK],  0, CPU_GDT_KCODE, &Xstack,  0);
	SETGATE(idt[T_GPFLT],  0, CPU_GDT_KCODE, &Xgpflt,  0);
	SETGATE(idt[T_PGFLT],  0, CPU_GDT_KCODE, &Xpgflt,  0);
	SETGATE(idt[T_FPERR],  0, CPU_GDT_KCODE, &Xfperr,  0);
	SETGATE(idt[T_ALIGN],  0, CPU_GDT_KCODE, &Xalign,  0);
	SETGATE(idt[T_MCHK],   0, CPU_GDT_KCODE, &Xmchk,   0);

	// Use DPL=3 here because system calls are explicitly invoked
	// by the user process (with "int $T_SYSCALL").
	SETGATE(idt[48], 0, CPU_GDT_KCODE, &Xsyscall, 3);

	SETGATE(idt[T_IRQ0 + 0], 0, CPU_GDT_KCODE, &Xirq0, 0);
	SETGATE(idt[T_IRQ0 + 1], 0, CPU_GDT_KCODE, &Xirq1, 0);
	SETGATE(idt[T_IRQ0 + 2], 0, CPU_GDT_KCODE, &Xirq2, 0);
	SETGATE(idt[T_IRQ0 + 3], 0, CPU_GDT_KCODE, &Xirq3, 0);
	SETGATE(idt[T_IRQ0 + 4], 0, CPU_GDT_KCODE, &Xirq4, 0);
	SETGATE(idt[T_IRQ0 + 5], 0, CPU_GDT_KCODE, &Xirq5, 0);
	SETGATE(idt[T_IRQ0 + 6], 0, CPU_GDT_KCODE, &Xirq6, 0);
	SETGATE(idt[T_IRQ0 + 7], 0, CPU_GDT_KCODE, &Xirq7, 0);
	SETGATE(idt[T_IRQ0 + 8], 0, CPU_GDT_KCODE, &Xirq8, 0);
	SETGATE(idt[T_IRQ0 + 9], 0, CPU_GDT_KCODE, &Xirq9, 0);
	SETGATE(idt[T_IRQ0 + 10], 0, CPU_GDT_KCODE, &Xirq10, 0);
	SETGATE(idt[T_IRQ0 + 11], 0, CPU_GDT_KCODE, &Xirq11, 0);
	SETGATE(idt[T_IRQ0 + 12], 0, CPU_GDT_KCODE, &Xirq12, 0);
	SETGATE(idt[T_IRQ0 + 13], 0, CPU_GDT_KCODE, &Xirq13, 0);
	SETGATE(idt[T_IRQ0 + 14], 0, CPU_GDT_KCODE, &Xirq14, 0);
	SETGATE(idt[T_IRQ0 + 15], 0, CPU_GDT_KCODE, &Xirq15, 0);

	/* interrupt gate for IRQ_ERROR */
	SETGATE(idt[T_IRQ0 + 19], 0, CPU_GDT_KCODE, &Xirq19, 0);

	asm volatile("lidt %0" : : "m" (idt_pd));
}

void
context_init()
{
	if (context_inited == TRUE)
		return;

	context_init_idt();

	context_inited = TRUE;
}

/*
 * creates a user stack page with a context header on the top.
 * The context header is created with a EIP set to f
 * The expected va is the virtual address location where this
 * stack is expected to be placed.
 */
context_t *
context_new(void (*f)(void), uint32_t expected_va)
{
	pageinfo_t *pi = mem_page_alloc();

	if (pi == NULL)
		return NULL;

	mem_incref(pi);

	tf_t *tf = (tf_t *) (mem_pi2ptr(pi) + PAGE_SIZE - sizeof(tf_t));

	/* set all segments to ring 3. */
	tf->es = CPU_GDT_UDATA | 3;
	tf->ds = CPU_GDT_UDATA | 3;
	tf->cs = CPU_GDT_UCODE | 3;
	tf->ss = CPU_GDT_UDATA | 3;

	/* EIP to the entrypoint of f (most likely a virtual address) */
	tf->eip = (uintptr_t) f;

	/* we expect the stack to be used with the virtual address */
	tf->esp = (uintptr_t) expected_va + PAGE_SIZE - sizeof(tf_t);

	/* make sure that the stack chain is interrupted */
	tf->regs.ebp = 0;

	/* must handle timer before enabling interrupts */
	tf->eflags = FL_IF;

	return (context_t *) mem_pi2ptr(pi);
}

void
context_destroy(context_t *ctx)
{
	mem_decref(mem_ptr2pi(ctx));
}

/*
 * Register an interrupt handler on the current CPU
 */
void
context_register_handler(int trapno, callback_t func)
{
	KERN_ASSERT(0 <= trapno && trapno < 256);

	pcpu_cur()->registered_callbacks[trapno] = func;
}

/*
 */
void gcc_noreturn
context_start(context_t *ctx)
{
	KERN_ASSERT(ctx->tf.eflags & FL_IF);

	__context_cur[pcpu_cur_idx()] = ctx;

	/* trap_dump(&ctx->tf); */

	KERN_ASSERT(ctx->tf.eip < 0x50000000);
	trap_return(&ctx->tf);
}

uint32_t
context_errno(context_t *ctx)
{
	return ctx->tf.err;
}

uint32_t
context_arg1(context_t *ctx)
{
	return ctx->tf.regs.eax;
}

uint32_t
context_arg2(context_t *ctx)
{
	return ctx->tf.regs.ebx;
}

uint32_t
context_arg3(context_t *ctx)
{
	return ctx->tf.regs.ecx;
}

uint32_t
context_arg4(context_t *ctx)
{
	return ctx->tf.regs.edx;
}

context_t *
context_cur(void)
{
	return __context_cur[pcpu_cur_idx()];
}

void
context_set_cur(context_t *ctx)
{
	__context_cur[pcpu_cur_idx()] = ctx;
}

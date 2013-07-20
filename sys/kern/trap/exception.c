#include <lib/export.h>
#include <mm/export.h>
#include <proc/export.h>

void
default_exception_handler(tf_t *tf)
{
	proc_exit();
}

void
pgflt_handler(tf_t *tf)
{
	struct proc *p = proc_curr();
	struct context *ctx = &p->uctx;
	uint32_t errno = ctx_errno(ctx);
	uint32_t fault_va = rcr2();

	if (errno & PFE_PR) {
		proc_exit();
		return;
	}

	if (pmap_reserve(p->pmap, rounddown(fault_va, PAGESIZE), PTE_W | PTE_U))
		proc_exit();
}

void exception_handler(tf_t *tf)
{
	switch (tf->trapno) {
	case T_PGFLT:
		pgflt_handler(tf);
		break;
	default:
		default_exception_handler(tf);
	}
}

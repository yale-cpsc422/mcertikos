#include <lib/debug.h>
#include <lib/trap.h>
#include <lib/types.h>
#include <lib/x86.h>

#include <mm/export.h>
#include <proc/export.h>

#define PFE_PR		0x1	/* Page fault caused by protection violation */

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

void
default_exception_handler(tf_t *tf)
{
	KERN_DEBUG("Trap %d @ 0x%08x.\n", tf->trapno, tf->eip);
	proc_exit();
}

void
pgflt_handler(tf_t *tf)
{
	struct proc *p = proc_cur();
	struct context *ctx = &p->uctx;
	uint32_t errno = ctx_errno(ctx);
	uint32_t fault_va = rcr2();

	/* KERN_DEBUG("Page fault: VA 0x%08x, errno 0x%08x, process %d, EIP 0x%08x.\n", */
	/* 	   fault_va, errno, p->pid, tf->eip); */

	if (errno & PFE_PR) {
		proc_exit();
		return;
	}

	pt_resv(p->pmap_id, rounddown(fault_va, PAGESIZE), PTE_W | PTE_U | PTE_P);
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

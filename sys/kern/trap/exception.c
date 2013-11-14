#include <preinit/lib/debug.h>
#include <lib/trap.h>
#include <lib/x86.h>
#include "syscall_dispatch.h"

#define PAGESIZE	4096

#define PFE_PR		0x1	/* Page fault caused by protection violation */

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

void
default_exception_handler(void)
{
	unsigned int cur_pid;

	cur_pid = get_curid();

	KERN_PANIC("Trap %d @ 0x%08x.\n",
		   uctx_get(cur_pid, U_TRAPNO), uctx_get(cur_pid, U_EIP));
}

void
pgflt_handler(void)
{
	unsigned int cur_pid;
	unsigned int errno;
	unsigned int fault_va;

	cur_pid = get_curid();
	errno = uctx_get(cur_pid, U_ERRNO);
	fault_va = rcr2();

	/* KERN_DEBUG("Page fault: VA 0x%08x, errno 0x%08x, process %d, EIP 0x%08x.\n", */
	/* 	   fault_va, errno, p->pid, tf->eip); */

	if (errno & PFE_PR) {
		KERN_PANIC("Permission denied: va = 0x%08x, errno = 0x%08x.\n",
			   fault_va, errno);
		return;
	}

	pt_resv(cur_pid, rounddown(fault_va, PAGESIZE), PTE_W | PTE_U | PTE_P);
}

void
exception_handler(void)
{
	unsigned int cur_pid;
	unsigned int trapno;

	cur_pid = get_curid();
	trapno = uctx_get(cur_pid, U_TRAPNO);

	if (trapno == T_PGFLT)
		pgflt_handler();
	else
		default_exception_handler();
}

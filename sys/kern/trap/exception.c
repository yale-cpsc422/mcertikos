#include <preinit/lib/debug.h>
#include <lib/trap.h>
#include <lib/x86.h>
#include <preinit/lib/timing.h>
#include "syscall_dispatch.h"

#define PAGESIZE	4096

#define PFE_PR		0x1	/* Page fault caused by protection violation */

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */

#define NUM_PROC	64
#define UCTX_SIZE	17

static void
trap_dump(tf_t *tf)
{
	if (tf == NULL)
		return;

	uintptr_t base = (uintptr_t) tf;

	KERN_DEBUG("trapframe at %x\n", base);
	KERN_DEBUG("\t%08x:\tedi:   \t\t%08x\n", &tf->regs.edi, tf->regs.edi);
	KERN_DEBUG("\t%08x:\tesi:   \t\t%08x\n", &tf->regs.esi, tf->regs.esi);
	KERN_DEBUG("\t%08x:\tebp:   \t\t%08x\n", &tf->regs.ebp, tf->regs.ebp);
	KERN_DEBUG("\t%08x:\tesp:   \t\t%08x\n", &tf->regs.oesp, tf->regs.oesp);
	KERN_DEBUG("\t%08x:\tebx:   \t\t%08x\n", &tf->regs.ebx, tf->regs.ebx);
	KERN_DEBUG("\t%08x:\tedx:   \t\t%08x\n", &tf->regs.edx, tf->regs.edx);
	KERN_DEBUG("\t%08x:\tecx:   \t\t%08x\n", &tf->regs.ecx, tf->regs.ecx);
	KERN_DEBUG("\t%08x:\teax:   \t\t%08x\n", &tf->regs.eax, tf->regs.eax);
	KERN_DEBUG("\t%08x:\tes:    \t\t%08x\n", &tf->es, tf->es);
	KERN_DEBUG("\t%08x:\tds:    \t\t%08x\n", &tf->ds, tf->ds);
	KERN_DEBUG("\t%08x:\ttrapno:\t\t%08x\n", &tf->trapno, tf->trapno);
	KERN_DEBUG("\t%08x:\terr:   \t\t%08x\n", &tf->err, tf->err);
	KERN_DEBUG("\t%08x:\teip:   \t\t%08x\n", &tf->eip, tf->eip);
	KERN_DEBUG("\t%08x:\tcs:    \t\t%08x\n", &tf->cs, tf->cs);
	KERN_DEBUG("\t%08x:\teflags:\t\t%08x\n", &tf->eflags, tf->eflags);
	KERN_DEBUG("\t%08x:\tesp:   \t\t%08x\n", &tf->esp, tf->esp);
	KERN_DEBUG("\t%08x:\tss:    \t\t%08x\n", &tf->ss, tf->ss);
}

extern unsigned int UCTX_LOC[NUM_PROC][UCTX_SIZE];

void
default_exception_handler(void)
{
	unsigned int cur_pid;

	cur_pid = get_curid();
	trap_dump((tf_t *) UCTX_LOC[cur_pid]);

	KERN_PANIC("Trap %d @ 0x%08x.\n",
		   uctx_get(cur_pid, U_TRAPNO), uctx_get(cur_pid, U_EIP));
}

void
pgflt_handler(void)
{
	unsigned int cur_pid;
	unsigned int errno;
	unsigned int fault_va;

    tri(TR_PGFLT, "enter pgflt_handler");

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
    tri(TR_PGFLT, "leave pgflt_handler");
}

void
exception_handler(void)
{
	unsigned int cur_pid;
	unsigned int trapno;
    tri(TR_PGFLT, "enter exception_handler");

	cur_pid = get_curid();
	trapno = uctx_get(cur_pid, U_TRAPNO);

	if (trapno == T_PGFLT)
		pgflt_handler();
	else
		default_exception_handler();

	tri(TR_PGFLT, "leave exception_handler");
}

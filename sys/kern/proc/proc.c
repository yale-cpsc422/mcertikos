#include <lib/debug.h>
#include <lib/string.h>

#include <mm/export.h>

#include "context.h"
#include "elf.h"
#include "kstack.h"
#include "proc.h"
#include "thread.h"

#define VM_USERHI	0xf0000000

static bool		proc_inited = FALSE;
static struct proc	all_processes[MAX_PROC];

void
proc_init(void)
{
	if (proc_inited == TRUE)
		return;

	thread_init();

	memzero(all_processes, sizeof(struct proc) * MAX_PROC);
	proc_inited = TRUE;
}

static struct proc *
proc_alloc(void)
{
	struct proc *p;
	int i;

	for (i = 0; i < MAX_PROC; i++)
		if (all_processes[i].inuse == FALSE)
			break;

	if (i == MAX_PROC)
		return NULL;

	p = &all_processes[i];
	p->pid = i;
	p->inuse = TRUE;

	return p;
}

static void
proc_free(struct proc *p)
{
	if (p == NULL)
		return;
	p->inuse = FALSE;
}

void
proc_start_user(void)
{
	kstack_switch(proc_cur()->td->td_kstack);
	set_PT(proc_cur()->pmap_id);
	ctx_start(&proc_cur()->uctx);
}

struct proc *
proc_create(uintptr_t elf_addr)
{
	struct proc *p;

	if ((p = proc_alloc()) == NULL)
		return NULL;

	if ((p->td = thread_spawn(proc_start_user)) == NULL) {
		proc_free(p);
		return NULL;
	}
	p->td->td_proc = p;

	p->pmap_id = pt_new();
	elf_load(elf_addr, p->pmap_id);

	ctx_init(&p->uctx, elf_entry(elf_addr), VM_USERHI - PAGESIZE);

	return p;
}

int
proc_exit(void)
{
	KERN_PANIC("proc_exit() not implemented yet.\n");
	return -1;
}

int
proc_terminate(struct proc *p)
{
	KERN_PANIC("proc_terminate() not implemented yet.\n");
	return -1;
}

void
proc_yield(void)
{
	thread_yield();
}

void
proc_sleep(struct threadq *slpq)
{
	thread_sleep(slpq);
}

void
proc_wakeup(struct threadq *slpq)
{
	thread_wakeup(slpq);
}

struct proc *
proc_cur(void)
{
	return current_thread()->td_proc;
}

void
proc_save_uctx(struct proc *p, tf_t *tf)
{
	p->uctx.tf = *tf;
}

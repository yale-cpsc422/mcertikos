#include <preinit/lib/elf.h>
#include <preinit/lib/debug.h>
#include <lib/gcc.h>
#include <lib/seg.h>
#include <lib/trap.h>

#include "uctx.h"
#include "kctx.h"
#include "thread.h"

#define PAGESIZE	4096
#define NUM_PROC	64
#define UCTX_SIZE	17

#define CPU_GDT_UCODE	0x18	    /* user text */
#define CPU_GDT_UDATA	0x20	    /* user data */

#define VM_USERHI	0xF0000000

#define FL_IF		0x00000200	/* Interrupt Flag */

void
foo_main(void)
{
    while (1)
    {
        dprintf("In foo_main...\n");
        thread_yield();
    }
}

void
start_foo(void)
{
    extern char STACK_LOC[NUM_PROC][PAGESIZE] gcc_aligned(PAGESIZE);
    unsigned int cur_tid = get_curid();
    asm volatile("movl %0, %%esp\n"
                 "pushl $0\n" // push a dummy return address
                 "jmp foo_main"
                 :
                 : "m" (STACK_LOC[cur_tid+1][0])
                );
}

unsigned int
ring0proc_create(void)
{
    unsigned int pid;
    unsigned int cur_pid;
    pid = thread_spawn((void *) start_foo);
	cur_pid = get_curid();
    kctx_switch(cur_pid, pid);

    return pid;
}

void
proc_start_user(void)
{
	extern unsigned int UCTX_LOC[NUM_PROC][UCTX_SIZE];

	unsigned int cur_pid = get_curid();

	tss_switch(cur_pid);
	set_PT(cur_pid);
	trap_return((void *) UCTX_LOC[cur_pid]);
}

unsigned int
proc_create(void *elf_addr)
{
	unsigned int pid;

	pid = thread_spawn((void *) proc_start_user);

	elf_load(elf_addr, pid);

	uctx_set(pid, U_ES, CPU_GDT_UDATA | 3);
	uctx_set(pid, U_DS, CPU_GDT_UDATA | 3);
	uctx_set(pid, U_CS, CPU_GDT_UCODE | 3);
	uctx_set(pid, U_SS, CPU_GDT_UDATA | 3);
	uctx_set(pid, U_ESP, VM_USERHI);
	uctx_set(pid, U_EFLAGS, FL_IF);
	uctx_set(pid, U_EIP, elf_entry(elf_addr));

	return pid;
}

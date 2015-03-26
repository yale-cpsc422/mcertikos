#include <preinit/lib/elf.h>
#include <preinit/lib/debug.h>
#include <lib/gcc.h>
#include <lib/seg.h>
#include <lib/trap.h>
#include <lib/x86.h>
#include <preinit/lib/x86.h>
#include <kern/ring0proc/ring0proc.h>
#include <preinit/lib/timing.h>

#include "uctx.h"

#define PAGESIZE	4096
#define NUM_PROC	64
#define UCTX_SIZE	17

#define CPU_GDT_UCODE	0x18	    /* user text */
#define CPU_GDT_UDATA	0x20	    /* user data */

#define VM_USERHI	0xF0000000

#define FL_IF		0x00000200	/* Interrupt Flag */

typedef void (*ring0_proc_entry_func_t)(void);

ring0_proc_entry_func_t ring0_proc_entries[NUM_PROC];

void
proc_start_ring0(void)
{
    extern char STACK_LOC[NUM_PROC][PAGESIZE] gcc_aligned(PAGESIZE);
    unsigned int cur_tid = get_curid();
    unsigned int stack_top = (unsigned int) STACK_LOC[cur_tid + 1];
    asm volatile("movl %0, %%esp\n"
                 "pushl $0\n" // push a dummy return address
                 "jmp *%1"
                 :
		 : "m" (stack_top), "r" (ring0_proc_entries[cur_tid])
                );
}

unsigned int
ring0proc_create(unsigned int id)
{
    unsigned int pid;


    if (id != 1 && id != 2)
        KERN_PANIC("Wrong ring0 process id!\n");

    pid = thread_spawn((void *) proc_start_ring0);

    if (id == 1)
        ring0_proc_entries[pid] = ring0_proc1;
    else if (id == 2)
        ring0_proc_entries[pid] = ring0_proc2;

    return pid;
}

void
proc_start_user(void)
{
	extern unsigned int UCTX_LOC[NUM_PROC][UCTX_SIZE];
	extern char STACK_LOC[NUM_PROC][PAGESIZE];

	unsigned int cur_pid = get_curid();

	if (get_pt() != cur_pid) {
	    tri(TR_PGFLT, "before tss_switch");

	    tss_switch(cur_pid);

        tri(TR_PGFLT, "before set_pt pid");
	    set_pt(cur_pid);

	    wrmsr(SYSENTER_ESP_MSR, (uint32_t) (&STACK_LOC[cur_pid][PAGESIZE-4]));
	}

	tri(TR_PGFLT, "before trap return");
	trap_return((void *) UCTX_LOC[cur_pid]);
}

unsigned int
proc_create(void *elf_addr)
{
	unsigned int pid;

	tri(TR_SPAWN, "enter proc_create");

	pid = thread_spawn((void *) proc_start_user);

    tri(TR_SPAWN, "begin elf_load");

	elf_load(elf_addr, pid);

    tri(TR_SPAWN, "complete elf_load");

	uctx_set(pid, U_ES, CPU_GDT_UDATA | 3);
	uctx_set(pid, U_DS, CPU_GDT_UDATA | 3);
	uctx_set(pid, U_CS, CPU_GDT_UCODE | 3);
	uctx_set(pid, U_SS, CPU_GDT_UDATA | 3);
	uctx_set(pid, U_ESP, VM_USERHI);
	uctx_set(pid, U_EFLAGS, FL_IF);
	uctx_set(pid, U_EIP, elf_entry(elf_addr));

    tri(TR_SPAWN, "leave proc_create");

	return pid;
}

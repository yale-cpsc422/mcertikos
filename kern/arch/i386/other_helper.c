#include <inc/arch/gcc.h>
#include <inc/arch/types.h>
#include <inc/arch/x86.h>
#include <inc/arch/mmu.h>
#include <inc/arch/mp.h>
#include <inc/arch/pic.h>
#include <inc/arch/mem.h>

#include <inc/arch/context.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/mem/mem.h>

// this function initializes the system on secondary processors.

void other_init(void(*f)(void)) {
	mp_donebooting();
	kstack_init();
	interrupts_init();
	f();
}

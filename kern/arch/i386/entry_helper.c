#include <inc/gcc.h>
#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <architecture/mp.h>
#include <architecture/mem.h>
#include <architecture/pic.h>

#include <architecture/context.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/mem/mem.h>
#include <kern/mem/pmem_layout.h>
#include <inc/multiboot.h>
#include <kern/hvm/svm/vm.h>

// We can not rely on the bootloader stack to remain there
// So we define a page of memory per CPU in use for stack.
// Since we do not know how many CPUs we have, we may be able to free
// these pages later

char stacks[MAX_CPU*PAGESIZE] gcc_aligned(PAGESIZE);

void init(void);

void entry_init(const struct multiboot_info *mbi) {
	extern char start[], edata[], end[];
	extern char stacks[];
	// Before anything else, complete the ELF loading process.
	// Clear all uninitialized global data (BSS) in our program,
	// ensuring that all static/global variables start out zero.

	//memset(edata, 0, end - edata);
	//
	// Can not use the line above because it will clear the current stack

	// clear all bss kernel data, except the stack we are currently on.
	// I would like to move this out of BSS - not sure how.
	memset(edata, 0, stacks - edata);
	memset(stacks+PAGESIZE, 0, end - stacks-PAGESIZE);

	// Console I/O
	// We will ignore all prints and key queries in our reasoning
	debug_init(); // initialize debug features
	cprintf("Console I/O was initialized\n");
	cprintf("Stacks are located at 0x%x\n", stacks);

	// initialize multi_processor interface
	// Required for interrupt subsystem
	mp_init();
	cprintf("MP initialized: %d cpus detected\n", mp_ncpu());


	// Initialize simple memory allocator
	// Depends on NVRAM / primitive memory instructions (to get the size of memory)
	// Parse GRUB info to init mem
	mem_init(mbi);

	pic_init();
	// enable the system of contexts (needs to be done only once)
	// depends on the memory subsystem.
	context_init();
	// Enable the more advanced kernel stack (GDT)
	kstack_init();
	// Now we can do context switches, and TSS is active, pointing to the current kernel stack

	// We can now initialize the hardware interrupts.
	// This system (mp.c) configures the PIC devices created by the mp system
	interrupts_init();

	// At this point we begin our verified kernel.
	init();
}

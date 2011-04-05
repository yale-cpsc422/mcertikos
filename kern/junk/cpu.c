// Segment management required for privilege level changes:
// global descriptor table (GDT) and task state segment (TSS)
// See COPYRIGHT for copyright information.

#include <inc/arch/gcc.h>
#include <inc/arch/x86.h>
#include <inc/arch/mp.h>
#include <inc/arch/mem.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/arch/cpu_internal.h>
#include <inc/arch/cpu.h>

#include <kern/mem/mem.h>

extern cpu stacks[]; // from entry.S
	extern char
		Xdivide,Xdebug,Xnmi,Xbrkpt,Xoflow,Xbound,
		Xillop,Xdevice,Xdblflt,Xtss,Xsegnp,Xstack,
		Xgpflt,Xpgflt,Xfperr,Xalign,Xmchk,Xdefault,Xsyscall;
	extern char
		Xirq0,Xirq1,Xirq2,Xirq3,Xirq4,Xirq5,
		Xirq6,Xirq7,Xirq8,Xirq9,Xirq10,Xirq11,
		Xirq12,Xirq13,Xirq14,Xirq15;


	// Interrupt descriptor table.  Must be built at run time because
	// shifted function addresses can't be represented in relocation records.
	struct gatedesc idt[256];
	struct pseudodesc idt_pd = {
		sizeof(idt) - 1, (uint32_t) idt
	};

static void cpu_init_gdt(cpu* c)
{
	c->gdt[0] = SEGDESC_NULL;
	// 0x08 - kernel code segment
	c->gdt[CPU_GDT_KCODE >> 3] = SEGDESC32(STA_X | STA_R, 0x0,
					0xffffffff, 0);
		// 0x10 - kernel data segment
	c->gdt[CPU_GDT_KDATA >> 3] = SEGDESC32(STA_W, 0x0,
					0xffffffff, 0);
		// 0x18 - user code segment
	c->gdt[CPU_GDT_UCODE >> 3] = SEGDESC32(STA_X | STA_R,
					0x00000000, 0xffffffff, 3);
		// 0x20 - user code segment
	c->gdt[CPU_GDT_UDATA >> 3] = SEGDESC32(STA_W,
					0x00000000, 0xffffffff, 3);
		// 0x28 - tss
	c->gdt[CPU_GDT_TSS >> 3] = SEGDESC16(STS_T32A, (uint32_t) (&c->tss),
					sizeof(taskstate)-1, 0);
	c->gdt[CPU_GDT_TSS >> 3].sd_s = 0;
	
	// we do not know the ID yet
	c->id = 0;

	// mark as not booted
	c->booted = false;

	// Magic numbers
	c->magic = CPU_MAGIC;
	// points to the top of the page
	c->tss.ts_esp0 = (uint32_t) c->kstackhi;
	// upon activation TSS switches stack to kernel mode
	c->tss.ts_ss0 = CPU_GDT_KDATA;
}

static void
cpu_init_idt()
{
	int i;

	// check that T_IRQ0 is a multiple of 8
	static_assert((T_IRQ0 & 7) == 0);

	// install a default handler
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
}


// Switch operation from "boot" mode of the cpu
// to regular mode for the current cpu.
// This involves hijacking the bottom of the kernel stack
// for GDT and other data structures
// First processor that boots also creates the IDT
// The rest simply load it up.
// (for now I assume that all processors want to share interrupts)
void cpu_init()
{
	int i;
	for (i=0;i<MAX_CPU;i++) {
		cpu_init_gdt(&stacks[i]);
	}
	
	// prepare a basic IDT table 
	cpu_init_idt();

	for(i=0;i<MAX_CPU;i++) {
		assert(stacks[i].magic == CPU_MAGIC);
//		if (i!=0)
//			assert(!mp_booted(i));
	}
}

void cpu_activate()
{

	cpu *c = cpu_cur();
	cprintf("cpu_activate on processor %d\n", 
			(((uint32_t)c-(uint32_t)stacks) / PAGESIZE));
	

	// Load the GDT
	struct pseudodesc gdt_pd = {
		sizeof(c->gdt) - 1, (uint32_t) c->gdt };
	asm volatile("lgdt %0" : : "m" (gdt_pd));

	// Reload all segment registers.
	asm volatile("movw %%ax,%%gs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%fs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%es" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ds" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ss" :: "a" (CPU_GDT_KDATA));
	asm volatile("ljmp %0,$1f\n 1:\n" :: "i" (CPU_GDT_KCODE)); // reload CS

	// We don't need an LDT.
	asm volatile("lldt %%ax" :: "a" (0));
	// Load the TSS (from the GDT)
	ltr(CPU_GDT_TSS);
	
	// This "pseudo-descriptor" is needed only by the LIDT instruction,
	// to specify both the size and address of th IDT at once.
	// Load the IDT into this processor's IDT register.
	asm volatile("lidt %0" : : "m" (idt_pd));
	// Mark CPU as activated
	
	// Create a new zeroed page which is used to keep the callback table per processor
	pageinfo* pi = mem_alloc();
	assert(pi);
	mem_incref(pi);
	c->registered_callbacks = (callback*)mem_pi2ptr(pi);


	c->booted = true;
	int i;
	for(i=0;i<10000;i++);
	assert(c->booted);
//	cprintf("Cpu %d (id %d) booted\n", cpu_number(c), c->id);
}

int cpu_number() {
	return (((uint32_t)(cpu_cur())-(uint32_t)stacks)/PAGESIZE);
}

// Find the CPU struct representing the current CPU.
// It always resides at the bottom of the page containing the CPU's stack.
static cpu *
cpu_cur() {
	cpu *c = (cpu*)ROUNDDOWN(read_esp(), PAGESIZE);
	//assert(c->magic == CPU_MAGIC);
	return c;
}

int cpu_id(cpu* c) {
	return c->id;
}

// Returns true if we're running on the bootstrap CPU.
int
cpu_onboot() {
	return cpu_cur() == stacks;
}


void trap_new (context* ctx, void (*f)(void), uint32_t stack, int usermode)
{
    trapframe* tf = (trapframe*)(ctx);
    tf->tf_es = CPU_GDT_UDATA | 3;
    tf->tf_ds = CPU_GDT_UDATA | 3;
    tf->trap.tf_cs = CPU_GDT_UCODE | 3;
    tf->trap.tf_ss = CPU_GDT_UDATA | 3;
    tf->trap.tf_eip = (uint32_t)f;
    tf->trap.tf_esp = (uint32_t)stack+PAGESIZE;
    tf->tf_regs.reg_ebp = 0;
	// must handle timer before enabling interrupts
    tf->trap.tf_eflags |= FL_IF;
//    return (context*) tf;
}

void
trap_clone (context* dest, context* src, void (*f)(void))
{
	memmove (dest, src, sizeof(trapframe));
	if (f != NULL) {
		((trapframe*)dest)->trap.tf_eip = (uint32_t)f;
		((trapframe*)dest)->trap.tf_eflags |= FL_IF; //not needed?
	}
	((trapframe*)dest)->trap.tf_eflags |= FL_IF; //not needed?
	return;
}


void
trap_register(int cpu, int trapno, callback func)
{
	assert(0 <= cpu && cpu < MAX_CPU);
	assert(0 <= trapno && trapno < 256);
	cprintf("Registered trap %d for callback %x\n", trapno, (uint32_t)func);
	stacks[cpu].registered_callbacks[trapno] = func;
}

void sl_syscall();
void stimer();

void gcc_noreturn
trap(trapframe *tf)
{
	// The user-level environment may have set the DF flag,
	// and some versions of GCC rely on DF being clear.
	asm volatile("cld" ::: "cc");
	uint32_t result=0;
    callback f = cpu_cur()->registered_callbacks[tf->trap.tf_trapno];
//	cprintf("Status of NT flag %x\n", read_eflags() & FL_NT);
//	cprintf("trapframe NT flag %x\n", tf->trap.tf_eflags & FL_NT);
//	if (cpu_cur() == &stacks[1]) {
//		cprintf("In trap number %d, cb %x, sl_syscall %x\n", tf->trap.tf_trapno, (uint32_t)f, (uint32_t)&stimer);
//	}
//    cprintf("In trap number %d, cpu %d, cb %x\n", tf->trap.tf_trapno, cpu_number(), (uint32_t)f);
	if (f) {
        //cprintf("Running callback %x\n", f);
		result = f(tf->trap.tf_err,(context*)tf);
    } else
		cprintf("Unregistered interrupt fired (maybe %d)\n", tf->trap.tf_trapno);

    
    // The callback is unlikely to return, but if it does
    // simply return to the interrupted state
    trap_return(tf);
}

void gcc_noreturn
trap_start (context *ctx)
{
	assert (((trapframe*)ctx)->trap.tf_eflags & FL_IF);
//    cprintf("About to jump to %x\n", ((trapframe*)ctx)->trap.tf_eip);
    trap_return((trapframe*) ctx);
}

uint32_t trap_arg1 (context *ctx) {
    return ((trapframe*)ctx)->tf_regs.reg_eax;
}

uint32_t trap_arg2 (context *ctx) {
    return ((trapframe*)ctx)->tf_regs.reg_ebx;
}

uint32_t trap_arg3 (context *ctx) {
    return ((trapframe*)ctx)->tf_regs.reg_ecx;
}

uint32_t trap_arg4 (context *ctx) {
    return ((trapframe*)ctx)->tf_regs.reg_edx;
}

void
trap_print(trapframe *tf)
{
        cprintf("TRAP frame at %p\n", tf);
//        trap_print_regs(&tf->tf_regs);
        cprintf("  es   0x----%04x\n", tf->tf_es);
        cprintf("  ds   0x----%04x\n", tf->tf_ds);
        cprintf("  trap 0x%08x\n", tf->trap.tf_trapno); // trap_name(tf->tf_trapno));
        cprintf("  err  0x%08x\n", tf->trap.tf_err);
        cprintf("  eip  0x%08x\n", tf->trap.tf_eip);
        cprintf("  cs   0x----%04x\n", tf->trap.tf_cs);
        cprintf("  flag 0x%08x\n", tf->trap.tf_eflags);
        cprintf("  esp  0x%08x\n", tf->trap.tf_esp);
        cprintf("  ss   0x----%04x\n", tf->trap.tf_ss);
}



void trap_debug(context *ctx)
{
    trapframe* tf = (trapframe*)ctx;
    trap_print(tf);
//    cprintf("trap no %d at (%x)\n", tf->trap.tf_trapno, tf->trap.tf_eip);
//    cprintf("cs\t%x\n", tf->trap.tf_cs);
}

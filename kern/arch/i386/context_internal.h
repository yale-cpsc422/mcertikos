// Per-CPU kernel state.
// See COPYRIGHT for copyright information.
#ifndef PIOS_KERN_SEG_INTERNAL_H
#define PIOS_KERN_SEG_INTERNAL_H

// Global segment descriptor numbers used by the kernel
#define CPU_GDT_NULL	0x00	// null descriptor (required by x86 processor)
#define CPU_GDT_KCODE	0x08	// kernel text
#define CPU_GDT_KDATA	0x10	// kernel data
#define CPU_GDT_UCODE	0x18	// user text
#define CPU_GDT_UDATA	0x20	// user data
#define CPU_GDT_TSS	0x28	// task state segment
#define CPU_GDT_NDESC	6	// number of GDT entries used, including null

#ifndef __ASSEMBLER__

#include <inc/gcc.h>
#include <architecture/types.h>
#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <architecture/trap.h>
#include <architecture/mp.h>

typedef struct context {
	char stack[PAGESIZE-sizeof(trapframe)];
	trapframe tf;
} context;

typedef uint32_t(*callback)(context *ctx);

// Per-CPU kernel state structure.
// Exactly one page (4096 bytes) in size.
typedef struct kstack {
	// Since the x86 processor finds the TSS from a descriptor in the GDT,
	// each processor needs its own TSS segment descriptor in some GDT.
	// We could have a single, "global" GDT with multiple TSS descriptors,
	// but it's easier just to have a separate fixed-size GDT per CPU.
	segdesc		gdt[CPU_GDT_NDESC];
	// Each CPU needs its own TSS,
	// because when the processor switches from lower to higher privilege,
	// it loads a new stack pointer (ESP) and stack segment (SS)
	// for the higher privilege level from this task state structure.
	taskstate	tss;


	// SHOULD THESE EVEN BE HERE
	//
	// used by this code to keep a registration of the functions to call.
	// Basically it is a C-style interrupt table
	callback* registered_callbacks;

	uint8_t id;
	volatile bool booted;

	// Magic verification tag (CPU_MAGIC) to help detect corruption,
	// e.g., if the CPU's ring 0 stack overflows down onto the cpu struct.
	uint32_t	magic;

	// Low end (growth limit) of the kernel stack.
	char		kstacklo[1];

	// High end (starting point) of the kernel stack.
	char gcc_aligned(PAGESIZE) kstackhi[0];
} kstack;

// The context subsystem tracks which context is currently running on each CPU
context* cur[MAX_CPU];

#define CPU_MAGIC	0x98765432	// cpu.magic should always = this

kstack* kstack_cur();

static void context_init_idt(void);

// context is an abstract type
//typedef void context;

// activates the context subsystem
// fills out certain data structures
// This should be done once per memory
void context_init();

// kstack_init:
// updates a simple active kernel stack into
// an advanced kernel stack
// adds support for context activation, etc
void kstack_init();


context* context_new(void (*f)(void), uint32_t expected_va);
void context_handler(int trapno, callback func);
void context_start (context *ctx) gcc_noreturn;

uint32_t context_errno (context *ctx);
uint32_t context_arg1(context* ctx);
uint32_t context_arg2(context* ctx);
uint32_t context_arg3(context* ctx);
uint32_t context_arg4(context* ctx);

// DEBUG
void context_debug(context* ctx);
#endif // ASSEMBLER
#endif // PIOS_KERN_CPU_H

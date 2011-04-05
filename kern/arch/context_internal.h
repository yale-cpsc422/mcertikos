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

#include <inc/arch/gcc.h>
#include <inc/arch/types.h>
#include <inc/arch/x86.h>
#include <inc/arch/mmu.h>


// This struct represents the format of the trap frames
// that get pushed on the kernel stack by the processor
// in conjunction with the interrupt/trap entry code in trapasm.S.
// All interrupts and traps use this same format,
// although not all fields are always used:
// e.g., the error code (tf_err) applies only to some traps,
// and the processor pushes tf_esp and tf_ss
// only when taking a trap from user mode (privilege level >0).
typedef struct trapframe {
	// registers and other info we push manually in trapasm.S
	pushregs tf_regs;
	uint16_t tf_es;
	uint16_t tf_padding1;
	uint16_t tf_ds;
	uint16_t tf_padding2;
	// TRAPHANDLERs macros will set this value to the interrupt number
    // that fired
    uint32_t tf_trapno;
	
    // Some interrupts will push an error code onto the stack
    // For others, the trap handler will place a 0 onto the stack
    uint32_t tf_err;  

    // EIP, CS, EFLAGS - frame of the CALL instruction
	uintptr_t tf_eip;
	uint16_t tf_cs;
	uint16_t tf_cspad; // pads CS to 4 bytes
	uint32_t tf_eflags;

	// rest included only when crossing rings, e.g., user to kernel
    // Ends up on the stack whenever TSS is involved in interrupt
    // This information may not be present in all cases, and may be
    // dangerous to access (pointers may point outside of stack)
	uintptr_t tf_esp;
	uint16_t tf_ss;
	uint16_t tf_sspad;
} trapframe;

// size of trapframe pushed when called from user and kernel mode, respectively
#define trapframe_usize sizeof(trapframe)	// full trapframe struct
#define trapframe_ksize (sizeof(trapframe) - 8)	// no esp, ss, padding4



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

#define CPU_MAGIC	0x98765432	// cpu.magic should always = this

static kstack* kstack_cur();


void trap(trapframe *tf) gcc_noreturn;
void trap_return(trapframe *tf) gcc_noreturn;
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

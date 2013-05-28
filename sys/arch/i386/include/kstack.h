#ifndef _MACHINE_KSTACK_H_
#define _MACHINE_KSTACK_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>

#include <machine/mmu.h>

#define KSTACK_SIZE	4096
#define KSTACK_MAGIC	0x98765432

struct kern_ctx {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t ebp;
	uint32_t eip;
};

struct kstack {
	/*
	 * Since the x86 processor finds the TSS from a descriptor in the GDT,
	 * each processor needs its own TSS segment descriptor in some GDT.
	 * We could have a single, "global" GDT with multiple TSS descriptors,
	 * but it's easier just to have a separate fixed-size GDT per CPU.
	 */
	segdesc_t	gdt[CPU_GDT_NDESC];

	/*
	 * Each CPU needs its own TSS,
	 * because when the processor switches from lower to higher privilege,
	 * it loads a new stack pointer (ESP) and stack segment (SS)
	 * for the higher privilege level from this task state structure.
	 */
	tss_t		tss;

	uint32_t	magic;

	uint8_t		kstack_lo[1];
	uint8_t		kstack_hi[0] gcc_aligned(KSTACK_SIZE);
};

/*
 * Initialize a bootstrap kernel stack.
 */
void kstack_init(struct kstack *ks);

/*
 * Initialize the kernel stack of a process.
 */
void kstack_init_proc(struct proc *p, void (*kern_entry)(void));

/*
 * Allocate a new kstack structure.
 */
struct kstack *kstack_alloc(void);

/*
 * Free a kstack structure.
 */
void kstack_free(struct kstack *ks);

/*
 * Switch to another kernel stack.
 */
void kstack_switch(struct kstack *to);

/*
 * Get the kernel stack where the caller is.
 */
#ifdef __COMPCERT__

extern gcc_inline struct kstack *ccomp_kstack_get_stack(void);

#define kstack_get_stack()			\
	ccomp_kstack_get_stack()

#else

#define kstack_get_stack()						\
	((struct kstack *) ROUNDDOWN(get_stack_pointer(), KSTACK_SIZE))

#endif

#endif /* _KERN_ */

#endif /* !_MACHINE_KSTACK_H_ */

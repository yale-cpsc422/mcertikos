#ifndef _MACHINE_PCPU_H_
#define _MACHINE_PCPU_H_

#ifndef _KERN_
#error "This is a kernel header; do not include it in userspace programs."
#endif

#include <sys/gcc.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#include <machine/mmu.h>
#include <machine/trap.h>

#include <dev/lapic.h>

#define MAX_CPU		64

typedef
struct __pcpu_t {
	spinlock_t	lk;

	bool		inited;

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

	/* When non-NULL, all traps get diverted to this handler. */
	void		(*recover)(tf_t *, void *recoverdata) gcc_noreturn;
	void		*recoverdata;

	bool		mp_inited;

	/* Local APIC id of thie cpu */
	lapicid_t	lapic_id;

	/* Is this cpu the bootstrap processor? */
	bool		is_bsp;
} __pcpu_t;

struct {
	/* cpuid 0x0 */
	uint32_t	max_input;	/* Maximum input value for basic CPUID
					   information (%eax)*/
	char		vendor[20];	/* CPU vendor (%ebx, %ecx, %edx) */

	/* cpuid 0x1 */
	uint8_t		family;		/* CPU family (%eax[11:8]) */
	uint8_t		model;		/* CPU model (%eax[7:4]) */
	uint8_t		step;		/* CPU step (%eax[3:0]) */
	uint8_t		ext_family;	/* extended CPU family (%eax[27:20]) */
	uint8_t		ext_model;	/* extended CPU model (%eax[19:16]) */
	uint8_t		brand_idx;	/* CPU brand index (%ebx[7:0]) */
	uint8_t		clflush_size;	/* CLFLUSH line size in bits (%ebx[15:8]) */
	uint8_t		max_cpu_id;	/* maximum number of addressable IDs for
					   logical processors (%ebx[23:16]) */
	uint8_t		apic_id;	/* initial APIC id (%ebx[31:24]) */
	uint32_t	feature1;	/* CPU features (%ecx) */
	uint32_t	feature2;	/* CPU features (%edx) */
} cpuinfo;

void __pcpu_init(void);
void __pcpu_init_cpu(uint32_t, __pcpu_t **, uintptr_t kstack_hi);
uint32_t __pcpu_ncpu(void);

bool __pcpu_mp_init(void);
void __pcpu_mp_init_cpu(uint32_t idx, uint8_t, bool);

bool __pcpu_is_smp(void);

lapicid_t __pcpu_cpu_lapicid(uint32_t idx);

#endif /* !_MACHINE_PCPU_H_ */

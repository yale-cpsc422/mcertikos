#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/kstack.h>
#include <sys/pmap.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <dev/lapic.h>

typedef enum { UNKNOWN, INTEL, AMD } cpu_vendor;

struct pcpuinfo {
	uint32_t	lapicid;
	bool		bsp;

	/* cpuid 0x0 */
	uint32_t	cpuid_high;	/* Maximum input value for basic CPUID
					   information (%eax)*/
	char		vendor[20];	/* CPU vendor string (%ebx, %ecx, %edx) */
	cpu_vendor	cpu_vendor;	/* CPU vendor */

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

	/* L1 cache information */
	size_t		l1_cache_size;		/* L1 cache size in KBs */
	size_t		l1_cache_line_size;	/* L1 cache line size in bytes */

	/* cpuid 0x80000000 */
	uint32_t	cpuid_exthigh;	/* Maximum Input Value for Extended
					   Function CPUID Information */
};

struct pcpu {
	spinlock_t	lk;
	bool		inited;		/* is pcpu structure initialized? */
	volatile bool	booted;		/* does this processor boot up ? */
	struct kstack	*kstack;	/* bootstrap kernel stack */
	struct pcpuinfo	arch_info;	/* arch-dependent information */
	trap_cb_t	**trap_handler;	/* arrays of trap handlers */

	bool		hvm_inited;	/* is HVM already initialized? */
};

/*
 * Initialize PCPU module.
 */
void pcpu_init(void);

/*
 * Get the current processor core, i.e. the processor core on which pcpu_cur()
 * is called.
 */
struct pcpu *pcpu_cur(void);

/*
 * Get the local APIC id of a processor core.
 *
 * @return the local APIC id of the processor core
 */
lapicid_t pcpu_cpu_lapicid(void);

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */

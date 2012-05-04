#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/types.h>

#include <machine/pcpu.h>
#include <machine/pmap.h>

#define PCPU_AP_START_ADDR	0x8000

/* typedef uint32_t (*callback_t) (struct context_t *); */

/* typedef struct context_t context_t; */

typedef
enum {
	PCPU_STOP,	/* this CPU is stopped; there maybe or maybe not some
			   process running on it */
	PCPU_RUNNING,	/* some process is running on this CPU */
} pcpu_stat_t;

typedef
struct pcpu_t {
	/* pcpu_t strcuture should be accessed mutually. */
	spinlock_t	lk;

	/*
	 * After calling pcpu_init_cpu() on a pcpu_t structure, this field
	 * should be TRUE.
	 */
	bool		inited;

	/* machine-dependent fields */
	__pcpu_t	*_pcpu;

	/* page table used on this processor */
	pmap_t		*pmap;

	/* interrupt handlers */
	callback_t	*registered_callbacks;

	/* Is this cpu booted?  */
	volatile bool	booted;

	/* Which process is running on this cpu? */
	proc_t		*proc;

	/* cpu status */
	pcpu_stat_t	stat;

	/*
	 * Magic verification tag (CPU_MAGIC) to help detect corruption,
	 * e.g., if the CPU's ring 0 stack overflows down onto the cpu struct.
	 */
	uint32_t	magic;

	/* Low end (growth limit) of the kernel stack. */
	uint8_t		kstacklo[1];

	/* High end (starting point) of the kernel stack. */
	uint8_t		kstackhi[0] gcc_aligned(PAGE_SIZE);
} pcpu_t;

#define PCPU_MAGIC	0x98765432

pcpu_t *pcpu;

void pcpu_init(void);

void pcpu_mp_init(void);
void pcpu_init_cpu(void);

void pcpu_boot_ap(uint32_t, void (*f)(void), uintptr_t);

pcpu_t * pcpu_cur(void);
int pcpu_cur_idx(void);

uint32_t pcpu_ncpu(void);

bool pcpu_onboot();

bool pcpu_is_smp(void);

lapicid_t pcpu_cpu_lapicid(uint32_t idx);

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */

#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <machine/pcpu.h>
#include <machine/pmap.h>

/*
 * The address of the first instruction which is executed by an application
 * processor core.
 *
 * XXX: It's actually start_ap() in sys/arch/i386/i386/boot_ap.S.
 */
#define PCPU_AP_START_ADDR	0x8000

typedef
volatile enum {
	PCPU_SHUTDOWN,	/* processor core is uninitialized or shutdown */
	PCPU_INITED	/* processor core is initialized */
} pcpu_stat_t;

struct sched {
	/*
	 * Schedule information
	 *
	 * - cur_proc: the currently running process on this processor.
	 *             There is at most one runnig process per processor. If no
	 *             process is running on this processor, cur_proc == NULL.
	 *
	 * - ready_queue: queue of processes that can be scheduled to run on
	 *                this processor.
	 *
	 * - blocked_queue: queue of processes that are blocked.
	 *
	 * - dead_queue: queue of processes that are waiting for being recycled.
	 */
	struct proc			*cur_proc;
	TAILQ_HEAD(readyq, proc)	ready_queue;
	TAILQ_HEAD(blockedq, proc)      blocked_queue;
	TAILQ_HEAD(deadq, proc)		dead_queue;

	spinlock_t			lk;
};

struct pcpu {
	/* pcpu_t strcuture should be accessed mutually. */
	spinlock_t	lk;

	/* machine-dependent fields */
	__pcpu_t	*_pcpu;

	/* state */
	pcpu_stat_t	state;

	/* page table used on this processor */
	pmap_t		*pmap;

	/* trap handlers */
	trap_cb_t	*trap_cb;

	/* schedule info */
	struct sched	sched;

	/* buffer for handleing system calls */
	void		*sys_buf;

	/*
	 * Magic verification tag (CPU_MAGIC) to help detect corruption,
	 * e.g., if the CPU's ring 0 stack overflows down onto the cpu struct.
	 */
	uint32_t	magic;

	/* Low end (growth limit) of the kernel stack. */
	uint8_t		kstacklo[1];

	/* High end (starting point) of the kernel stack. */
	uint8_t		kstackhi[0] gcc_aligned(PAGE_SIZE);
};

#define PCPU_MAGIC	0x98765432

struct pcpu *pcpu;

void pcpu_init(void);

void pcpu_mp_init(void);
void pcpu_init_cpu(void);

int pcpu_boot_ap(uint32_t, void (*f)(void), uintptr_t);

struct pcpu * pcpu_cur(void);
int pcpu_cur_idx(void);

uint32_t pcpu_ncpu(void);

bool pcpu_onboot();

bool pcpu_is_smp(void);

lapicid_t pcpu_cpu_lapicid(uint32_t idx);

void pcpu_lock(struct pcpu *);
void pcpu_unlock(struct pcpu *);

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */

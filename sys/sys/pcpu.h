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

#define PCPU_AP_START_ADDR	0x8000

/*
 * States and state transitions of a physical processor core.
 *
 * PCPU_SHUTDOWN: the processor core is not initialized or shut down
 * PCPU_BOOTUP  : the processor core boots up but is not initialized
 * PCPU_READY   : the processor core is initialized and ready for the workload
 * PCPU_RUNNING : the processor core is running some workload
 *
 *
 *     after        +---------------+   encounter errors / shutdown
 *   power on /     | PCPU_SHUTDOWN | <-------------------+
 *     reset        +---------------+                     |
 *                     ^    |                             |
 *                     |    |                             |
 *               +-----+    V                             |
 *               |   +-------------+  initialize   +------------+
 *               |   | PCPU_BOOTUP | ------------> | PCPU_READY | <--------+
 *               |   +-------------+               +------------+          |
 *               |                                         |       remove  |
 *               |                         start workload  |      workload |
 *               |                                         V               |
 *               |                                  +--------------+       |
 *               +--------------------------------- | PCPU_RUNNING | ------+
 *                  encounter errors / shutdown     +--------------+
 */

typedef
volatile enum {
	PCPU_SHUTDOWN,
	PCPU_BOOTUP,
	PCPU_READY,
	PCPU_RUNNING
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
	 * - sleeping_queue: queue of processes that are waiting for resources.
	 *
	 * - dead_queue: queue of processes that are waiting for being recycled.
	 */
	struct proc			*cur_proc;
	TAILQ_HEAD(readyq, proc)	ready_queue;
	TAILQ_HEAD(sleepingq, proc)	sleeping_queue;
	TAILQ_HEAD(deadq, proc)		dead_queue;

	spinlock_t cur_lk, ready_lk, sleeping_lk, dead_lk;
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

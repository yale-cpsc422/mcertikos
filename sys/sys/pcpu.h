#ifndef _KERN_PCPU_H_
#define _KERN_PCPU_H_

#ifdef _KERN_

#include <sys/context.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <machine/pcpu_mp.h>
#include <machine/pmap.h>
#include <machine/kstack.h>

#include <dev/lapic.h>

#define MAX_CPU			64

/*
 * The address of the first instruction which is executed by an application
 * processor core.
 *
 * XXX: It's actually start_ap() in sys/arch/i386/i386/boot_ap.S.
 */
#define PCPU_AP_START_ADDR	0x8000

/*
 * There's a scheduler per processor core. The scheduler is responsible to
 * schedule to all processes on that core.
 */
struct sched {
	struct proc	*cur_proc;	/* current running process */

	uint64_t	run_ticks;	/* time current process is running */

	/*
	 * ready_queue   all process which are ready to run
	 * blocked_queue all process which are blocked
	 * dead_queue    all process which are being killed
	 */
	TAILQ_HEAD(readyq, proc)   ready_queue;
	TAILQ_HEAD(blockedq, proc) blocked_queue;
	TAILQ_HEAD(deadq, proc)    dead_queue;

	spinlock_t	lk;		/* scheduler lock */
};

struct pcpu {
	spinlock_t	lk;

	bool		inited;		/* is pcpu structure initialized? */
	volatile bool	booted;		/* does this processor boot up ? */

	struct kstack	*kstack;	/* bootstrap kernel stack */

	struct pcpuinfo	arch_info;	/* arch-dependent information */

	struct sched	sched;		/* process scheduler */

	uint8_t		*sys_buf;	/* buffer for passing parameters and
					   return values of system calls */

	trap_cb_t	**trap_handler;	/* arrays of trap handlers */
};

struct pcpu pcpu[MAX_CPU];

/*
 * Initialize PCPU module.
 */
void pcpu_init(void);

/*
 * Processor-specific initialization.
 */
void pcpu_init_cpu(void);

/*
 * Boot a application processor core.
 *
 * @param cpu_idx    the index of the application processor core (> 0)
 * @param f          the initilization function which will execute by the
 *                   application processor core
 * @param stack_addr the base address of the kernel stack used on the
 *                   application core
 *
 * @return 0 if the application processor core is boot up, and the state of
 *         the application processor core is changed to PCPU_INITED; otherwise,
 *         return 1.
 */
int pcpu_boot_ap(uint32_t cpu_idx, void (*f)(void), uintptr_t stack_addr);

/*
 * Get the current processor core, i.e. the processor core on which pcpu_cur()
 * is called.
 */
struct pcpu *pcpu_cur(void);

/*
 * Get the index of the processor core.
 *
 * XXX: The index of the processor core is not always the same as the local APIC
 *      id of that processor core.
 *
 * @param c the processor core
 *
 * @return the index of the processor core if the processor core is valid;
 *         otherwise, return -1.
 */
int pcpu_cpu_idx(struct pcpu *c);

/*
 * Is the current processor the bootstrap processor core (BSP)?
 *
 * @return TRUE, if the current processor core is BSP; otherwise, return FALSE.
 */
bool pcpu_onboot(void);

/*
 * How many processor cores are there?
 *
 * @return the number of all processor cores
 */
uint32_t pcpu_ncpu(void);

/*
 * Is it a SMP system?
 *
 * @return TRUE if it's a SMP system; otherwise, return FALSE.
 */
bool pcpu_is_smp(void);

/*
 * Get the local APIC id of a processor core.
 *
 * @param cpu_idx the index of the processor core
 *
 * @return the local APIC id of the processor core
 */
lapicid_t pcpu_cpu_lapicid(int cpu_idx);

#define SCHED_SLICE			20	/* schedule every 20ms */

/*
 * Lock the scheduler on the processor c.
 */
#define sched_lock(c)				\
	do {					\
		spinlock_acquire(&c->sched.lk);	\
	} while (0)

/*
 * Unlock the scheduler on the processor c.
 */
#define sched_unlock(c)				\
	do {					\
		spinlock_release(&c->sched.lk);	\
	} while (0)

#endif /* _KERN_ */

#endif /* !_SYS_PCPU_H_ */

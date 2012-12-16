/*
 * Process scheduler.
 *
 * - Each process is pinned to a processor. No migration between processors is
 *   allowed.
 *
 * - A round-robin and per-precessor scheduler is responsible to schedule the
 *   processes on that processor.
 */

#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/trap.h>
#include <sys/types.h>

#include <dev/lapic.h>

#ifdef DEBUG_SCHED

#define SCHED_DEBUG(fmt, ...)					\
	do {							\
		KERN_DEBUG("SCHED: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define SCHED_DEBUG(fmt, ...)			\
	do {					\
	} while (0)

#endif

struct sched {
	spinlock_t	sched_lk;	/* scheduler lock */

	struct proc	*cur_proc;	/* current process */
	uint64_t	run_ticks;	/* how long has the current process
					   run? */

	TAILQ_HEAD(readyq, proc) rdyq;
	TAILQ_HEAD(slpq, proc)   slpq;
	TAILQ_HEAD(deadq, proc)  deadq;
};

static volatile bool sched_inited = FALSE;
static struct sched scheduler[MAX_CPU];

#define SCHED_SLICE		20
#define SCHED_LOCKED(sched)	spinlock_holding(&(sched)->sched_lk)
#define CURRENT_SCHEDULER()	(&scheduler[pcpu_cpu_idx(pcpu_cur())])
#define PROCESS_SCHEDULER(p)	(&scheduler[pcpu_cpu_idx((p)->cpu)])
#define CPU_SCHEDULER(c)	(&scheduler[pcpu_cpu_idx((c))])

static gcc_inline struct proc *sched_choose(struct sched *, bool choose_ready);
static gcc_inline void sched_ready(struct sched *, struct proc *, bool head);
static gcc_inline void sched_switch(struct sched *, struct proc *, struct proc *);

/*
 * Defined in sys/$ARCH/$ARCH/switch.S. (ARCH = i386)
 */
extern void swtch(struct kern_ctx **from, struct kern_ctx *to);

static gcc_inline struct proc *
sched_choose(struct sched *sched, bool choose_new)
{
	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);

	struct proc *chosen_p = sched->cur_proc;
	bool choose_ready = FALSE;

	if (unlikely((sched->cur_proc == NULL ||
		      sched->cur_proc->state == PROC_READY) &&
		     TAILQ_EMPTY(&sched->rdyq)))
		KERN_PANIC("No schedulable process.\n");

	if (sched->cur_proc == NULL) {
		/*
		 * If there's no current process, choose a process from the
		 * ready queue.
		 */
		choose_ready = TRUE;
	} else if (sched->cur_proc->state != PROC_RUNNING) {
		/*
		 * If the current process is already moved to the ready queue or
		 * the sleeping queue, choose a process from the ready queue.
		 */
		choose_ready = TRUE;
	} else if (!TAILQ_EMPTY(&sched->rdyq) &&
		   (choose_new || sched->run_ticks > SCHED_SLICE)) {
		/*
		 * If the ready queue is not empty, and it's forced to select a
		 * new process or the current process has ran out of its time
		 * slice, choose a new process from the ready queue.
		 */
		sched_ready(sched, sched->cur_proc, FALSE);
		choose_ready = TRUE;
	}

	if (choose_ready == FALSE)
		return chosen_p;

	chosen_p = TAILQ_FIRST(&sched->rdyq);
	TAILQ_REMOVE(&sched->rdyq, chosen_p, entry);

	SCHED_DEBUG("A new process %d is chosen.\n", chosen_p->pid);
	return chosen_p;
}

static gcc_inline void
sched_ready(struct sched *sched, struct proc *p, bool head)
{
	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_INITED ||
		    (p->state == PROC_RUNNING && p->cpu == pcpu_cur()) ||
		    p->state == PROC_SLEEPING);

	p->state = PROC_READY;

	if (head == TRUE)
		TAILQ_INSERT_HEAD(&sched->rdyq, p, entry);
	else
		TAILQ_INSERT_TAIL(&sched->rdyq, p, entry);

	SCHED_DEBUG("Process %d is moved to the ready queue on CPU%d.\n",
		    p->pid, sched - scheduler);
}

/*
 * XXX: sched_switch() is doing the kernel context switch. The user contexts
 *      are switched when returning to the userspace (see trap() and
 *      ctx_start()).
 */
static gcc_inline void
sched_switch(struct sched *sched, struct proc *from, struct proc *to)
{
	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);
	KERN_ASSERT(from == NULL || from->cpu == pcpu_cur());
	KERN_ASSERT(to != NULL);
	KERN_ASSERT(to->cpu == pcpu_cur());
	KERN_ASSERT((read_eflags() & FL_IF) == 0);

	struct kern_ctx *kctx; /* used when switching to the first process */

	sched->cur_proc = to;
	to->state = PROC_RUNNING;

	kstack_switch(to->kstack); /* XXX: is this necessary? */

	if (from != NULL)
		swtch(&from->kctx, to->kctx);
	else
		swtch(&kctx, to->kctx);

	/* return from another process */

	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);
	KERN_ASSERT(from != NULL);
	KERN_ASSERT(sched->cur_proc == from);
}

int
sched_init(void)
{
	int cpu_idx;

	if (pcpu_onboot() == FALSE || sched_inited == TRUE)
		return 0;

	for (cpu_idx = 0; cpu_idx < MAX_CPU; cpu_idx++) {
		spinlock_init(&scheduler[cpu_idx].sched_lk);
		scheduler[cpu_idx].cur_proc = NULL;
		scheduler[cpu_idx].run_ticks = 0;
		TAILQ_INIT(&scheduler[cpu_idx].rdyq);
		TAILQ_INIT(&scheduler[cpu_idx].slpq);
		TAILQ_INIT(&scheduler[cpu_idx].deadq);
	}

	sched_inited = TRUE;

	return 0;
}

/*
 * XXX: The scheduler on the current processor must be locked before entering
 *      sched_resched().
 */
void
sched_resched(bool force_choose)
{
	KERN_ASSERT(sched_inited == TRUE);

	struct sched *sched = CURRENT_SCHEDULER();

	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);

	struct proc *chosen_p = sched_choose(sched, force_choose);

	if (chosen_p != sched->cur_proc) {
		sched->run_ticks = 0;
		if (sched->cur_proc)
			SCHED_DEBUG("Switch from process %d to process %d.\n",
				    sched->cur_proc->pid, chosen_p->pid);
		else
			SCHED_DEBUG("Switch to process %d.\n", chosen_p->pid);
		sched_switch(sched, sched->cur_proc, chosen_p);
	}
}

/*
 * XXX: The scheduler on the processor where the process will be running must be
 *      locked before entering sched_add().
 */
void
sched_add(struct proc *p, struct pcpu *c)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_INITED);

	struct sched *sched = CPU_SCHEDULER(c);

	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);

	p->cpu = c;
	sched_ready(sched, p, FALSE);
}

/*
 * XXX: The scheduler on the processor where the sleeping process is must be
 *      locked before entering sched_wake().
 */
void
sched_wake(struct proc *p)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_SLEEPING);

	struct sched *sched = PROCESS_SCHEDULER(p);

	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);

	TAILQ_REMOVE(&sched->slpq, p, entry);
	sched_ready(sched, p, TRUE);
	SCHED_DEBUG("Process %d is waken.\n", p->pid);

	/*
	 * If the process is on another processor, send an IPI to trigger the
	 * scheduler on the processor.
	 *
	 * XXX: DON'T reschedule if the waken process is on the current
	 *      processor.
	 */
	if (p->cpu != pcpu_cur())
		lapic_send_ipi(p->cpu->arch_info.lapicid,
			       T_IRQ0+IRQ_IPI_RESCHED,
			       LAPIC_ICRLO_FIXED, LAPIC_ICRLO_NOBCAST);
}

/*
 * XXX: The scheduler on the current processor must be locked before entering
 *      sched_sleep().
 */
void
sched_sleep(struct proc *p)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_RUNNING);
	KERN_ASSERT(p->cpu == pcpu_cur());

	struct sched *sched = CURRENT_SCHEDULER();

	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);

	p->state = PROC_SLEEPING;
	TAILQ_INSERT_TAIL(&sched->slpq, p, entry);

	SCHED_DEBUG("Process %d is sleeping.\n", p->pid);

	sched_resched(TRUE);
}

void
sched_yield(void)
{
	KERN_ASSERT(sched_inited == TRUE);

	struct sched *sched = CPU_SCHEDULER(pcpu_cur());

	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);

	if (!TAILQ_EMPTY(&sched->rdyq)) {
		sched_ready(sched, sched->cur_proc, FALSE);
		sched_resched(TRUE);
	}
}

void
sched_lock(struct pcpu *c)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(c != NULL);
	spinlock_acquire(&CPU_SCHEDULER(c)->sched_lk);
}

void
sched_unlock(struct pcpu *c)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(c != NULL);
	spinlock_release(&CPU_SCHEDULER(c)->sched_lk);
}

void
sched_update(void)
{
	KERN_ASSERT(sched_inited == TRUE);
	struct sched *sched = CURRENT_SCHEDULER();
	sched_lock(pcpu_cur());
	sched->run_ticks += (1000 / LAPIC_TIMER_INTR_FREQ);
	if (sched->run_ticks > SCHED_SLICE)
		sched_resched(TRUE);
	sched_unlock(pcpu_cur());
}

struct proc *
sched_cur_proc(struct pcpu *c)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(c != NULL);
	return scheduler[pcpu_cpu_idx(c)].cur_proc;
}

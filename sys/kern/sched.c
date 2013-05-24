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
#include <sys/slab.h>
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

#define SC_TABLESIZE		128			/* must be power of 2 */
#define SC_MASK			(SC_TABLESIZE - 1)
#define SC_SHIFT		8
#define SC_HASH(wc)					\
	(((uintptr_t) (wc) >> SC_SHIFT) & SC_MASK)
#define SC_LOOKUP(wc)		(&slpq_chain[SC_HASH(wc)])

struct sleepqueue {
	TAILQ_HEAD(, proc)	blocked;
	int			blocked_nr;
	LIST_ENTRY(sleepqueue)	hash_entry;
	spinlock_t		lk;
	void			*wchan;
};

static struct kmem_cache	*slpq_cache;

struct sleepqueue_chain {
	LIST_HEAD(, sleepqueue)	slpqs;
	spinlock_t		lk;
};

/*
 * Global sleep queues.
 */
struct sleepqueue_chain		slpq_chain[SC_TABLESIZE];

/*
 * Per-CPU scheduler.
 */
struct sched {
	spinlock_t		sched_lk;	/* scheduler lock */

	struct proc		*cur_proc;	/* current process */
#ifndef __COMPCERT__
	uint64_t		run_ticks;	/* how long has the current
						   process run? */
#else
	uint32_t		run_ticks;	/* XXX: no 64-bit integer in
						        CompCert */
#endif

	TAILQ_HEAD(, proc)	rdyq;
	TAILQ_HEAD(, proc)	deadq;
};

static volatile bool sched_inited = FALSE;
static struct sched scheduler[MAX_CPU];

#define SCHED_SLICE		20
#define SCHED_LOCKED(sched)	spinlock_holding(&(sched)->sched_lk)
#define CURRENT_SCHEDULER()	(&scheduler[pcpu_cpu_idx(pcpu_cur())])
#define PROCESS_SCHEDULER(p)	(&scheduler[pcpu_cpu_idx((p)->cpu)])
#define CPU_SCHEDULER(c)	(&scheduler[pcpu_cpu_idx((c))])

static void slpq_init(void);
static void slpq_init_obj(void *slpq, struct kmem_cache *cache);

static gcc_inline struct proc *sched_choose(struct sched *, bool choose_ready);
static gcc_inline void sched_ready(struct sched *, struct proc *, bool head);
static gcc_inline void sched_switch(struct sched *, struct proc *, struct proc *);

static void
slpq_init(void)
{
	int sc_idx;
	struct sleepqueue_chain *sc;

	slpq_cache = kmem_cache_create("SLPQ", sizeof(struct sleepqueue),
				       pcpu_cur()->arch_info.l1_cache_line_size,
				       SLAB_F_NO_REAP,
				       slpq_init_obj, slpq_init_obj);
	KERN_ASSERT(slpq_cache != NULL);

	/*
	 * Initialize the global sleep queue chain.
	 */
	for (sc_idx = 0; sc_idx < SC_TABLESIZE; sc_idx++) {
		sc = &slpq_chain[sc_idx];
		LIST_INIT(&sc->slpqs);
		spinlock_init(&sc->lk);
	}
}

static void
slpq_init_obj(void *opaque, struct kmem_cache *cache)
{
	KERN_ASSERT(opaque != NULL);
	KERN_ASSERT(cache != NULL);
	KERN_ASSERT(cache == slpq_cache);

	struct sleepqueue *slpq = opaque;
	TAILQ_INIT(&slpq->blocked);
	slpq->blocked_nr = 0;
	slpq->wchan = NULL;
	spinlock_init(&slpq->lk);
}

static gcc_inline struct sleepqueue *
slpq_sc_lookup(struct sleepqueue_chain *sc, void *wchan)
{
	KERN_ASSERT(sc != NULL);
	KERN_ASSERT(spinlock_holding(&sc->lk) == TRUE);

	struct sleepqueue *slpq;

	LIST_FOREACH(slpq, &sc->slpqs, hash_entry) {
		if (slpq->wchan == wchan)
			break;
	}

	return (slpq->wchan == wchan) ? slpq : NULL;
}

int
sched_add_slpq(void *wchan)
{
	struct sleepqueue_chain *sc;
	struct sleepqueue *slpq;

	if (wchan == NULL) {
		SCHED_DEBUG("Cannot add a sleep queue for NULL.\n");
		return -1;
	}

	sc = SC_LOOKUP(wchan);

	spinlock_acquire(&sc->lk);

#ifdef SCHED_DEBUG
	slpq = slpq_sc_lookup(sc, wchan);
	KERN_ASSERT(slpq == NULL);
#endif

	slpq = kmem_cache_alloc(slpq_cache, 0);

	if (slpq == NULL) {
		SCHED_DEBUG("Cannot allocate a sleep queue.\n");
		spinlock_release(&sc->lk);
		return -2;
	}

	slpq->wchan = wchan;

	LIST_INSERT_HEAD(&sc->slpqs, slpq, hash_entry);

	spinlock_release(&sc->lk);

	return 0;
}

int
sched_remove_slpq(void *wchan)
{
	struct sleepqueue_chain *sc;
	struct sleepqueue *slpq;

	if (wchan == NULL) {
		SCHED_DEBUG("Cannot add a sleep queue for NULL.\n");
		return -1;
	}

	sc = SC_LOOKUP(wchan);

	spinlock_acquire(&sc->lk);

	slpq = slpq_sc_lookup(sc, wchan);

	if (slpq == NULL) {
		SCHED_DEBUG("No sleep queue for 0x%08x.\n", wchan);
		spinlock_release(&sc->lk);
		return -2;
	}

	spinlock_acquire(&slpq->lk);

	if (slpq->blocked_nr) {
		SCHED_DEBUG("Sleep queue for 0x%08x is not empty.\n", wchan);
		spinlock_release(&slpq->lk);
		spinlock_release(&sc->lk);
		return -3;
	}

	LIST_REMOVE(slpq, hash_entry);

	spinlock_release(&slpq->lk);

	kmem_cache_free(slpq_cache, slpq);

	spinlock_release(&sc->lk);

	return 0;
}

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
#ifndef __COMPCERT__
	KERN_ASSERT((read_eflags() & FL_IF) == 0);
#endif

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

	/*
	 * Initialize all per-CPU schedulers.
	 */
	for (cpu_idx = 0; cpu_idx < MAX_CPU; cpu_idx++) {
		spinlock_init(&scheduler[cpu_idx].sched_lk);
		scheduler[cpu_idx].cur_proc = NULL;
		scheduler[cpu_idx].run_ticks = 0;
		TAILQ_INIT(&scheduler[cpu_idx].rdyq);
		TAILQ_INIT(&scheduler[cpu_idx].deadq);
	}

	/*
	 * Initialize the global sleep queue chaine.
	 */
	slpq_init();

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

	if (chosen_p->inv) {
		spinlock_acquire(chosen_p->inv);
		chosen_p->inv = NULL;
	}

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

void
sched_wake(void *wchan)
{
	KERN_ASSERT(sched_inited == TRUE);

	struct sched *sched;
	struct pcpu *cur_cpu = pcpu_cur();
	struct proc *p;
	struct sleepqueue_chain *sc;
	struct sleepqueue *slpq;

	int blocked_nr;

	sc = SC_LOOKUP(wchan);
	spinlock_acquire(&sc->lk);
	slpq = slpq_sc_lookup(sc, wchan);
	KERN_ASSERT(slpq != NULL);
	spinlock_acquire(&slpq->lk);
	spinlock_release(&sc->lk);

	blocked_nr = slpq->blocked_nr;

	while (blocked_nr) {
		KERN_ASSERT(spinlock_holding(&slpq->lk) == TRUE);
		KERN_ASSERT(blocked_nr >= 0);

		p = TAILQ_FIRST(&slpq->blocked);
		TAILQ_REMOVE(&slpq->blocked, p, entry);
		slpq->blocked_nr--;
		blocked_nr--;

		spinlock_release(&slpq->lk);

		KERN_ASSERT(p != NULL);
		KERN_ASSERT(p->state == PROC_SLEEPING);

		sched = PROCESS_SCHEDULER(p);
		KERN_ASSERT(sched != NULL);

		spinlock_acquire(&sched->sched_lk);

		sched_ready(sched, p, TRUE);
		SCHED_DEBUG("Process %d is waken for 0x%08x.\n", p->pid, wchan);

		spinlock_release(&sched->sched_lk);

		/*
		 * If the process is on another processor, send an IPI to
		 * trigger the scheduler on the processor.
		 *
		 * XXX: DON'T reschedule if the waken process is on the current
		 *      processor.
		 */
		if (p->cpu != cur_cpu)
			lapic_send_ipi(p->cpu->arch_info.lapicid,
				       T_IPI0 + IPI_RESCHED,
				       LAPIC_ICRLO_FIXED, LAPIC_ICRLO_NOBCAST);

		spinlock_acquire(&slpq->lk);
	}

	spinlock_release(&slpq->lk);
}

/*
 * XXX: The scheduler on the current processor must be locked before entering
 *      sched_sleep().
 */
void
sched_sleep(struct proc *p, void *wchan, spinlock_t *inv)
{
	KERN_ASSERT(sched_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_RUNNING);
	KERN_ASSERT(p->cpu == pcpu_cur());
	KERN_ASSERT(wchan != NULL);
	KERN_ASSERT(inv == NULL || spinlock_holding(inv) == TRUE);

#ifdef DEBUG_SCHED
	struct sched *sched = CURRENT_SCHEDULER();
#endif
	struct sleepqueue_chain *sc;
	struct sleepqueue *slpq;

#ifdef DEBUG_SCHED
	KERN_ASSERT(sched != NULL);
	KERN_ASSERT(SCHED_LOCKED(sched) == TRUE);
#endif

	sc = SC_LOOKUP(wchan);
	spinlock_acquire(&sc->lk);
	slpq = slpq_sc_lookup(sc, wchan);
	KERN_ASSERT(slpq != NULL);
	spinlock_acquire(&slpq->lk);
	spinlock_release(&sc->lk);

	p->state = PROC_SLEEPING;
	TAILQ_INSERT_TAIL(&slpq->blocked, p, entry);
	slpq->blocked_nr++;

	if (inv) {
		p->inv = inv;
		spinlock_release(inv);
	}

	spinlock_release(&slpq->lk);

	SCHED_DEBUG("Process %d is sleeping on 0x%08x.\n", p->pid, wchan);

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

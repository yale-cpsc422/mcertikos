#include <sys/context.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/mqueue.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>

#include <machine/pmap.h>

#include <dev/tsc.h>

#ifdef DEBUG_PROC

#define PROC_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG(fmt);		\
	}
#else

#define PROC_DEBUG(fmt...)			\
	{					\
	}

#endif

#define SCHED_PERIOD	200

static bool proc_inited = FALSE;

static TAILQ_HEAD(free_proc_list, proc)	free_procs;
static struct proc process[MAX_PID];

static spinlock_t free_procs_lk;

/*
 * Allocate a process structure from the free processes list.
 *
 * XXX: no state transition (PROC_UNINITED)
 *
 * @return pointer to the process structure; NULL if there is no free process
 */
static struct proc *
proc_alloc(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct proc *new_proc = NULL;

	spinlock_acquire(&free_procs_lk);

	if (TAILQ_EMPTY(&free_procs))
		goto ret;

	new_proc = TAILQ_FIRST(&free_procs);
	KERN_ASSERT(new_proc->state == PROC_UNINITED);
	TAILQ_REMOVE(&free_procs, new_proc, entry);

 ret:
	spinlock_release(&free_procs_lk);
	return new_proc;
}

/*
 * Free a process structure; put it in the free processes queue again.
 *
 * XXX: any state --> PROC_UNINITED
 *
 * XXX: proc->lk should be held by the caller.
 */
static void
proc_free(struct proc *proc)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(proc != NULL);
	KERN_ASSERT(0 <= proc - process && proc - process < MAX_PID);

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);
	KERN_ASSERT(proc->waiting_for == NULL);

	if (proc->state == PROC_UNINITED) /* already in the free queue */
		return;

	spinlock_acquire(&free_procs_lk);
	TAILQ_INSERT_TAIL(&free_procs, proc, entry);
	spinlock_release(&free_procs_lk);

	proc->state = PROC_UNINITED;
}

static int
proc_migrate(struct proc *p, struct pcpu *c)
{
	PROC_DEBUG("Not support process migration yet.\n");
	return 1;
}

int
proc_init(void)
{
	if (proc_inited == TRUE)
		return 0;

	pid_t pid;

	/* clear all process structures */
	memset(process, 0x0, sizeof(struct proc) * MAX_PID);

	/* initialize the free processs list */
	spinlock_init(&free_procs_lk);
	spinlock_acquire(&free_procs_lk);
	TAILQ_INIT(&free_procs);
	for (pid = 0; pid < MAX_PID; pid++) {
		process[pid].pid = pid;
		TAILQ_INSERT_TAIL(&free_procs, &process[pid], entry);
	}
	spinlock_release(&free_procs_lk);

	proc_inited = TRUE;

	return 0;
}

int
proc_sched_init(struct sched *sched)
{
	KERN_ASSERT(sched != NULL);

	sched->cur_proc = NULL;

	TAILQ_INIT(&sched->ready_queue);
	TAILQ_INIT(&sched->sleeping_queue);
	TAILQ_INIT(&sched->dead_queue);

	spinlock_init(&sched->cur_lk);
	spinlock_init(&sched->ready_lk);
	spinlock_init(&sched->sleeping_lk);
	spinlock_init(&sched->dead_lk);

	return 0;
}

gcc_inline void
proc_lock(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);

	spinlock_acquire(&p->lk);
}

gcc_inline void
proc_unlock(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);

	if (spinlock_holding(&p->lk) == FALSE) {
		KERN_PANIC("lock of process %d is not acquired.\n", p->pid);
		return;
	}

	spinlock_release(&p->lk);
}

/*
 * XXX: PROC_UNINITED --> PROC_INITED
 */
struct proc *
proc_create(uintptr_t binary)
{
	KERN_ASSERT(proc_inited == TRUE);

	pageinfo_t *pi;
	struct proc *new_proc = proc_alloc();

	if (new_proc == NULL) {
		PROC_DEBUG("No free process.\n");
		goto ret;
	}

	spinlock_init(&new_proc->lk);
	proc_lock(new_proc);

	KERN_ASSERT(new_proc->state == PROC_UNINITED);

	new_proc->state = PROC_INITED;
	new_proc->cpu = NULL;
	new_proc->waiting_for = NULL;
	new_proc->vm = NULL;

	if ((new_proc->pmap = pmap_new()) == NULL) {
		PROC_DEBUG("Failed to allocate address space for process %d.\n",
			   new_proc->pid);
		proc_free(new_proc);
		goto lock_ret;
	}

	PROC_DEBUG("pmap of process %d: 0x%08x\n",
		   new_proc->pid, new_proc->pmap);

	/*
	 * TODO: move this to the userspace
	 */
	elf_load(binary, new_proc->pmap);
	ctx_init(new_proc,
		 (void (*)(void)) elf_entry(binary), VM_STACKHI - PAGESIZE);

	/* allocate memory for the stack */
	if ((pi = mem_page_alloc()) == NULL) {
		PROC_DEBUG("Cannot allocate memory for stack of process %d.\n",
			   new_proc->pid);
		proc_free(new_proc);
		goto lock_ret;
	}
	pmap_insert(new_proc->pmap, pi, VM_STACKHI - PAGESIZE,
		    PTE_P | PTE_U | PTE_W);

	mqueue_init(&new_proc->mqueue);

	PROC_DEBUG("Process %d is created.\n", new_proc->pid);

 lock_ret:
	proc_unlock(new_proc);
 ret:
	return new_proc;
}



/*
 * Put process p in the ready queue of processor c.
 *
 * XXX: PROC_INITED | PROC_RUNNING | PROC_SLEEPING --> PROC_READY
 */
int
proc_ready(struct proc *p, struct pcpu *c)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL && c != NULL);

	int rc = 0;
	struct sched *sched;

	proc_lock(p);

	KERN_ASSERT(p->state == PROC_READY || p->state == PROC_INITED ||
		    p->state == PROC_RUNNING || p->state == PROC_SLEEPING);

	if (p->cpu != NULL && p->cpu != c) {
		/* p is on another processor; process migration is required */
		PROC_DEBUG("Process %d is on another processor.\n", p->pid);
		if ((rc = proc_migrate(p, c)))
			goto ret;
	}

	if (p->state == PROC_READY) {
		/* p is already on the ready queue; early return */
		PROC_DEBUG("Process %d is already in the ready queue.\n",
			   p->pid);
		goto ret;
	}

	sched = &c->sched;

	if (p->state == PROC_RUNNING) {
		/* clear cur_proc */
		spinlock_acquire(&sched->cur_lk);
		KERN_ASSERT(p == sched->cur_proc);
		sched->cur_proc = NULL;
		spinlock_release(&sched->cur_lk);
	} else if (p->state == PROC_SLEEPING) {
		/* remove from sleeping queue */
		spinlock_acquire(&sched->sleeping_lk);
		TAILQ_REMOVE(&sched->sleeping_queue, p, entry);
		spinlock_release(&sched->sleeping_lk);
	}

	p->state = PROC_READY;
	p->cpu = c;

	spinlock_acquire(&sched->ready_lk);
	TAILQ_INSERT_TAIL(&sched->ready_queue, p, entry);
	spinlock_release(&sched->ready_lk);

 ret:
	proc_unlock(p);
	return rc;
}

/*
 * Let process p sleep for spinlock lk; put process p in the sleeping queue.
 *
 * XXX: PROC_RUNNING | PROC_READY --> PROC_SLEEPING
 */
int
proc_sleep(struct proc *p, spinlock_t *lk, wake_cb_t cb)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL && lk != NULL);

	struct sched *sched;
	int rc = 0;
	int need_sched = 0;

	proc_lock(p);

	KERN_ASSERT(p->state == PROC_SLEEPING ||
		    p->state == PROC_RUNNING || p->state == PROC_READY);

	if (p->state == PROC_SLEEPING) {
		/* p is already in the sleeping queue; early return */
		PROC_DEBUG("Process %d is already in the sleeping queue.\n",
			   p->pid);
		goto ret;
	}

	sched = &p->cpu->sched;

	if (p->state == PROC_RUNNING) {
		/* clear cur_proc */
		spinlock_acquire(&sched->cur_lk);
		KERN_ASSERT(p == sched->cur_proc);
		sched->cur_proc = NULL;
		spinlock_release(&sched->cur_lk);
		need_sched = 1;
	} else if (p->state == PROC_READY) {
		/* remove from the ready queue */
		spinlock_acquire(&sched->ready_lk);
		TAILQ_REMOVE(&sched->ready_queue, p, entry);
		spinlock_release(&sched->ready_lk);
	}

	p->state = PROC_SLEEPING;
	p->waiting_for = lk;
	p->wake_cb = cb;

	spinlock_acquire(&sched->sleeping_lk);
	TAILQ_INSERT_TAIL(&sched->sleeping_queue, p, entry);
	spinlock_release(&sched->sleeping_lk);

 ret:
	proc_unlock(p);

	if (need_sched)
		proc_sched(pcpu_cur());

	return rc;
}

/*
 * Wake a sleeping process.
 *
 * XXX: PROC_SLEEPING -> PROC_READY
 */
int
proc_wake(struct proc *p)
{
	KERN_ASSERT(p != NULL);

	struct pcpu *c;
	int rc = 0;

	proc_lock(p);
	KERN_ASSERT(p->state == PROC_SLEEPING);
	proc_unlock(p);

	if (rc == 0) {
		c = pcpu_cur();
		KERN_ASSERT(c != NULL);
		rc = proc_ready(p, c);
	}

	return rc;
}

/*
 * Yield from process p to another process.
 *
 * XXX: PROC_RUNNING --> PROC_READY
 */
int
proc_yield(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);

	struct pcpu *c;
	struct sched *sched;
	int rc = 0;

	proc_lock(p);

	if (p->state != PROC_RUNNING) {
		/* only running process can yield */
		PROC_DEBUG("Process %d is not runnning.\n", p->pid);
		proc_unlock(p);
		return 1;
	}

	c = p->cpu;
	sched = &c->sched;

	proc_unlock(p);

	spinlock_acquire(&sched->ready_lk);
	if (TAILQ_EMPTY(&sched->ready_queue)) {
		/* p is only runnable process; early return */
		spinlock_release(&sched->ready_lk);
		return 1;
	}
	spinlock_release(&sched->ready_lk);

	/* move myself to the ready queue */
	if ((rc = proc_ready(p, c))) {
		PROC_DEBUG("Cannot move process %d to the ready queue.\n",
			   p->pid);
		return rc;
	}

	/* schedule another process */
	return proc_sched(c);
}

/*
 * Schedule a process to run on the processor c.
 *
 * XXX: proc_sched() removes a process from the ready queue and marks it as the
 *      current process on the processor c. It does not executing the process.
 *      The execution is delayed when returing from a trap in trap().
 *
 * XXX: proc_sched() can be called only after all traps except those resulting
 *      in calling proc_sched() have been handled.
 *
 * XXX: PROC_READY --> PROC_RUNNING
 */
int
proc_sched(struct pcpu *c)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(c != NULL);

	int rc = 0;
	struct sched *sched;
	struct proc *p = NULL;

	KERN_ASSERT(c->state == PCPU_RUNNING);

	sched = &c->sched;

	spinlock_acquire(&sched->cur_lk);
	spinlock_acquire(&sched->ready_lk);

	if (unlikely(sched->cur_proc == NULL &&
		     TAILQ_EMPTY(&sched->ready_queue))) {
		/* there is no process in both cur_proc and the ready queue */
		PROC_DEBUG("Cannot find a runnable process.\n");
		rc = 1;
		goto ret;
	}

	if (sched->cur_proc != NULL) {
		/* move the current process to the ready queue */
		p = sched->cur_proc;

		proc_lock(p);

		if (time_ms() - p->start_time < SCHED_PERIOD) {
			proc_unlock(p);
			goto ret;
		}

		PROC_DEBUG("Process %d timeout.\n", p->pid);

		p->state = PROC_READY;
		TAILQ_INSERT_TAIL(&sched->ready_queue, p, entry);

		proc_unlock(p);
		sched->cur_proc = NULL;
	}

	/* select the first process on the ready queue as the current process */
	p = TAILQ_FIRST(&sched->ready_queue);
	KERN_ASSERT(p != NULL);
	proc_lock(p);
	TAILQ_REMOVE(&sched->ready_queue, p, entry);
	p->state = PROC_RUNNING;
	p->start_time = time_ms();
	sched->cur_proc = p;
	proc_unlock(p);

	PROC_DEBUG("Select process %d.\n", p->pid);

 ret:
	spinlock_release(&sched->ready_lk);
	spinlock_release(&sched->cur_lk);
	return rc;
}

/*
 * Start running a process.
 */
void gcc_noreturn
proc_run(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct pcpu *c;
	struct proc *p;
	struct sched *sched;
	struct context *ctx;

	c = pcpu_cur();
	KERN_ASSERT(c != NULL);

	KERN_ASSERT(c->state == PCPU_RUNNING);
	sched = &c->sched;

	spinlock_acquire(&sched->cur_lk);

	p = sched->cur_proc;
	KERN_ASSERT(p != NULL);

	spinlock_acquire(&p->lk);

	KERN_ASSERT(p->state == PROC_RUNNING && p->pmap != NULL);

	/* update the pcpu_t structure of the processor */
	c->pmap = p->pmap;
	c->state = PCPU_RUNNING;

	ctx = &p->ctx;

	pmap_install(c->pmap);

	if (p->wake_cb) {
		p->wake_cb(p);
		p->wake_cb = NULL;
	}

	/* it's safe to release all locks here (?) */
	proc_unlock(p);
	spinlock_release(&sched->cur_lk);


	PROC_DEBUG("Start runnig process %d.\n", p->pid);
	/* ctx_dump(ctx); */

	ctx_start(ctx);
}

struct proc *
proc_cur(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct pcpu *c;

	c = pcpu_cur();
	KERN_ASSERT(c != NULL);

	return c->sched.cur_proc;
}

void
proc_save(struct proc *p, tf_t *tf)
{
	KERN_ASSERT(p != NULL && tf != NULL);

	proc_lock(p);
	p->ctx.tf = *tf;
	proc_unlock(p);
}

struct proc *
proc_pid2proc(pid_t pid)
{
	return &process[pid];
}

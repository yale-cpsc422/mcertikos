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

#include <sys/virt/vmm.h>

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

#define SCHED_TIME_SLICE	200

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
	KERN_ASSERT(new_proc->state == PROC_INVAL);
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

	if (proc->state == PROC_INVAL) /* already in the free queue */
		return;

	spinlock_acquire(&free_procs_lk);
	TAILQ_INSERT_TAIL(&free_procs, proc, entry);
	spinlock_release(&free_procs_lk);

	proc->state = PROC_INVAL;
}

/*
 * Put process p on the ready queue of the current processor.
 *
 * XXX: proc_ready() assumes the process p is locked.
 * XXX: proc_ready() assumes the scheduler of the current processor is locked.
 *
 * @return 0 if no errors, and the state of the process is set to PROC_READY.
 */
static int
proc_ready(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(spinlock_holding(&p->lk) == TRUE);
	KERN_ASSERT(p->state == PROC_INITED ||
		    p->state == PROC_BLOCKED ||
		    p->state == PROC_RUNNING);

	struct pcpu *c;
	struct sched *sched;

	c = pcpu_cur();

	KERN_ASSERT(p->cpu == NULL || p->cpu == c);

	sched = &c->sched;

	KERN_ASSERT(spinlock_holding(&sched->lk) == TRUE);

	if (p == proc_cur())
		sched->cur_proc = NULL;

	if (p->state == PROC_BLOCKED)
		TAILQ_REMOVE(&sched->blocked_queue, p, entry);

	TAILQ_INSERT_TAIL(&sched->ready_queue, p, entry);

	p->cpu = c;
	p->state = PROC_READY;

	return 0;
}

/*
 * Run the current process on the current processor.
 *
 * XXX: proc_run() assumes the current process is locked.
 * XXX: proc_run() assumes the scheduler of the current processor is locked.
 */
static gcc_noreturn void
proc_run(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct pcpu *c;
	struct proc *p;
	struct sched *sched;
	struct context *ctx;

	c = pcpu_cur();
	sched = &c->sched;

	KERN_ASSERT(spinlock_holding(&sched->lk) == TRUE);

	p = sched->cur_proc;

	KERN_ASSERT(p != NULL);
	KERN_ASSERT(spinlock_holding(&p->lk) == TRUE);
	KERN_ASSERT(p->state == PROC_READY || p->state == PROC_RUNNING);
	KERN_ASSERT(p->pmap != NULL);

	/* update the pcpu_t structure of the processor */
	spinlock_acquire(&c->lk);
	c->pmap = p->pmap;
	spinlock_release(&c->lk);

	pmap_install(p->pmap);
	ctx = &p->ctx;
	p->state = PROC_RUNNING;
	p->last_running_time = time_ms();

	/* it's safe to release all locks here (?) */
	proc_unlock(p);
	spinlock_release(&sched->lk);

	PROC_DEBUG("Start runnig process %d.\n", p->pid);
	/* ctx_dump(ctx); */

	ctx->tf.eflags &= ~FL_IF;
	ctx_start(ctx);
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
	TAILQ_INIT(&sched->blocked_queue);
	TAILQ_INIT(&sched->dead_queue);

	spinlock_init(&sched->lk);

	return 0;
}

/*
 * Create a new process.
 *
 * @return the process control block of the new process, and the state of the
 *         new process is set to PROC_INITED; otherwise, return NULL.
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

	KERN_ASSERT(new_proc->state == PROC_INVAL);

	new_proc->cpu = NULL;
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

	new_proc->state = PROC_INITED;

	PROC_DEBUG("Process %d is created.\n", new_proc->pid);

 lock_ret:
	proc_unlock(new_proc);
 ret:
	return new_proc;
}

/*
 * Put a process p on the blocked queue of the current processor.
 *
 * @return 0 if no errors, and the state of the process is set to PCPU_BLOCKED;
 *         otherwise, return 1.
 */
int
proc_block(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);

	struct pcpu *c;
	struct sched *sched;

	c = pcpu_cur();
	sched = &c->sched;

	proc_lock(p);
	spinlock_acquire(&sched->lk);

	KERN_ASSERT(p == proc_cur());
	KERN_ASSERT(p->state == PROC_RUNNING);
	KERN_ASSERT(p->cpu == c);

	sched->cur_proc = NULL;
	p->state = PROC_BLOCKED;
	TAILQ_INSERT_TAIL(&sched->blocked_queue, p, entry);

	spinlock_release(&sched->lk);
	proc_unlock(p);

	return 0;
}

/*
 * Move a process from the blocked queue of the current processor to the ready
 * ready queue of the same processor.
 *
 * @return 0 if no errors, and the state of the process is set to PCPU_READY;
 *         otherwise, return 1.
 */
int
proc_unblock(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);

	struct pcpu *c;
	struct sched *sched;

	c = pcpu_cur();
	sched = &c->sched;

	proc_lock(p);
	spinlock_acquire(&sched->lk);

	KERN_ASSERT(p->state == PROC_BLOCKED);
	KERN_ASSERT(p->cpu == c);

	proc_ready(p);

	spinlock_release(&sched->lk);
	proc_unlock(p);

	return 0;
}

/*
 * Per-processor scheduler. It selects a process from the ready queue, sets it
 * as the current process, and runs it.
 */
gcc_noreturn void
proc_sched(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct pcpu *c;
	struct sched *sched;
	struct proc *p;

	c = pcpu_cur();
	sched = &c->sched;

	spinlock_acquire(&sched->lk);

	if (unlikely(sched->cur_proc == NULL &&
		     TAILQ_EMPTY(&sched->ready_queue)))
		KERN_PANIC("No schedulable process!");

	if (sched->cur_proc != NULL) {
		p = sched->cur_proc;
		proc_lock(p);

		if (p->total_running_time > SCHED_TIME_SLICE &&
		    !TAILQ_EMPTY(&sched->ready_queue)) {
			p->total_running_time = 0;
			proc_ready(p);
			proc_unlock(p);

			PROC_DEBUG("Process %d is time-out.\n", p->pid);
		}
	}

	if (sched->cur_proc == NULL) {
		p = TAILQ_FIRST(&sched->ready_queue);
		proc_lock(p);
		TAILQ_REMOVE(&sched->ready_queue, p, entry);
		sched->cur_proc = p;
	}

	proc_run();
}

int
proc_add2sched(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct pcpu *c;
	struct sched *sched;

	c = pcpu_cur();
	sched = &c->sched;

	proc_lock(p);
	spinlock_acquire(&sched->lk);

	KERN_ASSERT(p->state == PROC_INITED);
	proc_ready(p);

	spinlock_release(&sched->lk);
	proc_unlock(p);

	return 0;
}

int
proc_yield(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct proc *p;
	struct pcpu *c;
	struct sched *sched;

	c = pcpu_cur();
	sched = &c->sched;

	spinlock_acquire(&sched->lk);

	p = proc_cur();
	proc_lock(p);
	proc_ready(p);
	proc_unlock(p);

	spinlock_release(&sched->lk);

	return 0;
}

struct proc *
proc_cur(void)
{
	struct pcpu *c;

	if (proc_inited == FALSE)
		return NULL;

	c = pcpu_cur();
	KERN_ASSERT(c != NULL);

	return c->sched.cur_proc;
}

void
proc_save(struct proc *p, tf_t *tf)
{
	KERN_ASSERT(p != NULL && tf != NULL);
	KERN_ASSERT(spinlock_holding(&p->lk) == TRUE);

	p->ctx.tf = *tf;
}

struct proc *
proc_pid2proc(pid_t pid)
{
	return &process[pid];
}

/*
 * Process Management & Process Scheduler.
 *
 * - CertiKOS runs a round-robin process scheduler on each processor core.
 *
 * - The scheduler only schedules processes on that processor core.
 *
 * - A process is pined to one processor core and can't be migrated to another
 *   processor.
 */

#include <sys/channel.h>
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>

#include <machine/pmap.h>

#include <dev/tsc.h>

#ifdef DEBUG_PROC

#define PROC_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG(fmt, ##__VA_ARGS__);		\
	} while (0)
#else

#define PROC_DEBUG(fmt...)			\
	do {					\
	} while (0)

#endif

static bool 				proc_inited = FALSE;

static spinlock_t			proc_pool_lk;
static TAILQ_HEAD(proc_pool, proc)	proc_pool;
static struct proc 			process[MAX_PID];

static struct proc *
proc_alloc(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct proc *p;

	spinlock_acquire(&proc_pool_lk);

	if (TAILQ_EMPTY(&proc_pool)) {
		spinlock_release(&proc_pool_lk);
		PROC_DEBUG("Process pool is empty.\n");
		return NULL;
	}

	p = TAILQ_FIRST(&proc_pool);
	TAILQ_REMOVE(&proc_pool, p, entry);

	spinlock_release(&proc_pool_lk);

	return p;
}

static void
proc_free(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(0 <= p - process && p - process < MAX_PID);

	spinlock_acquire(&proc_pool_lk);
	TAILQ_INSERT_TAIL(&proc_pool, p, entry);
	spinlock_release(&proc_pool_lk);
}

/*
 * The first-created process uses proc_spawn_return() to return to userspace.
 *
 * XXX: The lock the scheduler of the current processor must be acquired
 *      before entering proc_spawn_return().
 */
static void
proc_spawn_return(void)
{
	sched_unlock(pcpu_cur());
	/* return to ctx_start() (see kstack_init_proc()) */
}

int
proc_init(void)
{
	pid_t pid;

	if (pcpu_onboot() == FALSE || proc_inited == TRUE)
		return 0;

	spinlock_init(&proc_pool_lk);

	memzero(process, sizeof(struct proc) * MAX_PID);

	TAILQ_INIT(&proc_pool);
	for (pid = 0; pid < MAX_PID; pid++) {
		process[pid].pid = pid;
		TAILQ_INSERT_TAIL(&proc_pool, &process[pid], entry);
	}

	if (pcpu_onboot() == TRUE)
		sched_init();

	proc_inited = TRUE;

	return 0;
}

struct proc *
proc_new(struct proc *parent, struct channel *ch)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct proc *p;
	pageinfo_t *user_pi, *buf_pi;

	/* allocate a PCB */
	if ((p = proc_alloc()) == NULL) {
		PROC_DEBUG("Cannot create a PCB.\n");
		return NULL;
	}

	/* maintain the process tree */
	p->parent = (parent == NULL) ? p : parent;
	if (parent != NULL)
		TAILQ_INSERT_TAIL(&parent->children, p, child);
	TAILQ_INIT(&p->children);

	/* create the channel to parent */
	if (parent != NULL && ch != NULL) {
		channel_lock(ch);
		p->parent_ch = ch;
		channel_unlock(ch);
	}

	/* initialize the page structures */
	if ((p->pmap = pmap_new()) == NULL) {
		PROC_DEBUG("Cannot initialize page structures.\n");
		proc_free(p);
		return NULL;
	}

	/* initialize the kernel stack for the process */
	if ((p->kstack = kstack_alloc()) == NULL) {
		PROC_DEBUG("Cannot allocate memory for kernek stack.\n");
		pmap_free(p->pmap);
		proc_free(p);
		return NULL;
	}

	/* allocate memory for the syscall buffer */
	if ((buf_pi = mem_page_alloc()) == NULL) {
		PROC_DEBUG("Cannot allocate memory for syscall buffer.\n");
		pmap_free(p->pmap);
		kstack_free(p->kstack);
		proc_free(p);
		return NULL;
	}
	p->sys_buf = (uint8_t *) mem_pi2ptr(buf_pi);

	/* allocate memory for the userspace stack */
	if ((user_pi = mem_page_alloc()) == NULL) {
		PROC_DEBUG("Cannot allocate memory for userspace stack.\n");
		mem_page_free(buf_pi);
		pmap_free(p->pmap);
		kstack_free(p->kstack);
		proc_free(p);
		return NULL;
	}
	pmap_insert(p->pmap, user_pi,
		    VM_STACKHI - PAGESIZE, PTE_P | PTE_U | PTE_W);

	/* other fields */
	p->inv = NULL;
	spinlock_init(&p->proc_lk);
	p->state = PROC_INITED;

	return p;
}

int
proc_exec(struct proc *p, struct pcpu *c, uintptr_t u_elf)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(c != NULL);
	KERN_ASSERT(p != NULL && p->state == PROC_INITED);

	proc_lock(p);

	/* load the execution file */
	elf_load(u_elf, p->pmap);

	/* initialize user context */
	ctx_init(p, (void (*)(void)) elf_entry(u_elf), VM_STACKHI - PAGESIZE);

	kstack_init_proc(p, c, proc_spawn_return);

	sched_lock(c);
	sched_add(p, c);
	sched_unlock(c);

	proc_unlock(p);

	return 0;
}

void
proc_sleep(struct proc *p, void *wchan, spinlock_t *inv)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(wchan != NULL);
	KERN_ASSERT(inv == NULL || spinlock_holding(inv) == TRUE);

	sched_lock(p->cpu);
	sched_sleep(p, wchan, inv);
	sched_unlock(p->cpu);

	KERN_ASSERT(inv == NULL || spinlock_holding(inv) == TRUE);
}

void
proc_wake(void *wchan)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(wchan != NULL);
	sched_wake(wchan);
}

void
proc_yield(void)
{
	KERN_ASSERT(proc_inited == TRUE);
	sched_lock(pcpu_cur());
	sched_yield();
	sched_unlock(pcpu_cur());
}

struct proc *
proc_cur(void)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(pcpu_cur() != NULL);
	return sched_cur_proc(pcpu_cur());
}

struct proc *
proc_pid2proc(pid_t pid)
{
	KERN_ASSERT(proc_inited == TRUE);
	if (!(0 <= pid && pid < MAX_PID))
		return NULL;
	return &process[pid];
}

void
proc_save(struct proc *p, tf_t *tf)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_RUNNING);
	KERN_ASSERT(tf != NULL);

	proc_lock(p);
	p->uctx.tf = *tf;
	proc_unlock(p);
}

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
#include <sys/session.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>

#include <dev/tsc.h>

#ifdef DEBUG_PROC

#define PROC_DEBUG(fmt...)			\
	do {					\
		KERN_DEBUG(fmt);		\
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
 * Put a process on the ready queue of a specified processor.
 *
 * XXX: The lock of the scheduler of the specified processor must be acquired
 *      before entering proc_ready().
 *
 * XXX: Only the new created process, the running process and the blocked
 *      process can be put on the ready queue.
 *
 * XXX: If the process is a running process, proc_ready() must be running on
 *      the same processor as that on which the process is running.
 *
 * XXX: proc_ready() doesn't modify sched->cur_proc.
 *
 * @param p    the process
 * @param c    the processor
 * @param head if TRUE, put the process at the beginning of the ready queue;
 *             if FALSE, put at the end of the ready queue.
 */
static void
proc_ready(struct proc *p, struct pcpu *c, bool head)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(c != NULL);
	KERN_ASSERT(p->state == PROC_INITED ||
		    (p->state == PROC_RUNNING && pcpu_cur() == c) ||
		    p->state == PROC_BLOCKED);
	KERN_ASSERT(p->cpu == NULL || p->cpu == c);
	KERN_ASSERT(spinlock_holding(&c->sched.lk) == TRUE);

	struct sched *sched = &c->sched;

	if (p->state == PROC_BLOCKED)
		TAILQ_REMOVE(&sched->blocked_queue, p, entry);

	if (head == TRUE)
		TAILQ_INSERT_HEAD(&sched->ready_queue, p, entry);
	else
		TAILQ_INSERT_TAIL(&sched->ready_queue, p, entry);

	p->cpu = c;
	p->state = PROC_READY;

	PROC_DEBUG("Process %d is put on the ready queue of CPU%d.\n",
		   p->pid, c-pcpu);
}

/*
 * Defined in sys/$ARCH/$ARCH/switch.S. (ARCH = i386)
 */
extern void swtch(struct kern_ctx **from, struct kern_ctx *to);

/*
 * Switch from the current process to another process.
 *
 * XXX: The lock of the scheduler of the current processor must be acquired
 *      before entering proc_switch().
 *
 * XXX: The process to which proc_switch() will switch must be on the current
 *      processor.
 */
static void
proc_switch(struct proc *to)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(to != NULL && to->cpu == pcpu_cur());
	KERN_ASSERT(spinlock_holding(&pcpu_cur()->sched.lk) == TRUE);
	KERN_ASSERT((read_eflags() & FL_IF) == 0);

	struct sched *sched = &pcpu_cur()->sched;
	struct proc *from;
	struct kern_ctx *kctx; /* used when switching to the first process */

	/* switch the kernel stack (i.e. TSS) */
	kstack_switch(to->kstack);

	/* switch the page structures  */
	pmap_install(to->pmap);

	/* switch to process to */
	from = sched->cur_proc;
	if (from != NULL)
		PROC_DEBUG("Switch from process %d to process %d.\n",
			   from->pid, to->pid);
	else
		PROC_DEBUG("Switch to the first process %d.\n", to->pid);
	sched->cur_proc = to;
	to->state = PROC_RUNNING;
	if (from != NULL)
		swtch(&from->kctx, to->kctx);
	else
		swtch(&kctx, to->kctx);

	/*
	 * XXX: Return from another process.
	 */

	if (from == NULL)
		KERN_PANIC("Switch from the last process on CPU%d.\n",
			   pcpu_cpu_idx(pcpu_cur()));

	KERN_ASSERT(sched->cur_proc != NULL);
	KERN_ASSERT(sched->cur_proc == from);
	KERN_ASSERT(spinlock_holding(&sched->lk) == TRUE);
	KERN_ASSERT(rcr3() == (uintptr_t) from->pmap);
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
	KERN_ASSERT(spinlock_holding(&pcpu_cur()->sched.lk) == TRUE);
	sched_unlock(pcpu_cur());
	/* return to ctx_start() (see kstack_init_proc()) */
}

int
proc_init(void)
{
	pid_t pid;

	if (proc_inited == TRUE)
		return 0;

	if (pcpu_onboot() == FALSE)
		return 1;

	spinlock_init(&proc_pool_lk);

	memzero(process, sizeof(struct proc) * MAX_PID);

	TAILQ_INIT(&proc_pool);
	for (pid = 0; pid < MAX_PID; pid++) {
		process[pid].pid = pid;
		TAILQ_INSERT_TAIL(&proc_pool, &process[pid], entry);
	}

	channel_init();
	session_init();

	proc_inited = TRUE;

	return 0;
}

int
proc_sched_init(struct sched *sched)
{
	KERN_ASSERT(sched != NULL);

	sched->cur_proc = NULL;
	sched->run_ticks = 0;

	TAILQ_INIT(&sched->ready_queue);
	TAILQ_INIT(&sched->blocked_queue);
	TAILQ_INIT(&sched->dead_queue);

	spinlock_init(&sched->lk);

	return 0;
}

struct proc *
proc_new(uintptr_t binary, struct session *s)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(s != NULL);

	struct proc *p;
	pageinfo_t *user_pi, *buf_pi;

	/* allocate a PCB */
	if ((p = proc_alloc()) == NULL) {
		PROC_DEBUG("Cannot create a PCB.\n");
		return NULL;
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

	/* load the execution file */
	elf_load(binary, p->pmap);

	/* initialize user context */
	ctx_init(p, (void (*)(void)) elf_entry(binary), VM_STACKHI - PAGESIZE);

	/* other fields */
	p->vm = NULL;
	spinlock_init(&p->lk);
	p->state = PROC_INITED;

	/* maintain the process tree */
	p->parent = pcpu_cur()->sched.cur_proc;
	if (p->parent != NULL)
		TAILQ_INSERT_TAIL(&p->parent->child_list, p, child_entry);
	else
		p->parent = p;
	TAILQ_INIT(&p->child_list);

	/* set session */
	session_add_proc(s, p);

	PROC_DEBUG("Process %d is created in session %d.\n",
		   p->pid, p->session->sid);

	return p;
}

void
proc_run(struct pcpu *c, struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(c != NULL);
	KERN_ASSERT(p != NULL && p->state == PROC_INITED);

	proc_lock(p);
	kstack_init_proc(p, c, proc_spawn_return);
	p->cpu = c;
	proc_unlock(p);

	sched_lock(c);
	proc_ready(p, c, TRUE);
	sched_unlock(c);

	PROC_DEBUG("Process %d is put on the ready queue of CPU %d.\n",
		   p->pid, pcpu_cpu_idx(c));
}

struct proc *
proc_spawn(struct pcpu *c, uintptr_t binary, struct session *s)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(c != NULL && s != NULL);

	struct proc *p;

	if ((p = proc_new(binary, s)) == NULL) {
		PROC_DEBUG("Cannot create a new process in session %d.\n",
			   s->sid);
		return NULL;
	}

	proc_run(c, p);

	return p;
}

/*
 * XXX: The lock of the scheduler of the current processor must be acquired
 *      before entering proc_block().
 *
 * XXX: The blocked process must be the running process on the current
 *      processor.
 */
void
proc_block(struct proc *p, block_reason_t reason, struct channel *ch)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_RUNNING);
	KERN_ASSERT(p->cpu == pcpu_cur());
	KERN_ASSERT(spinlock_holding(&pcpu_cur()->sched.lk) == TRUE);
	KERN_ASSERT(spinlock_holding(&ch->lk) == TRUE);

	struct sched *sched = &pcpu_cur()->sched;

	p->block_reason = reason;
	p->block_channel = ch;
	p->state = PROC_BLOCKED;

	TAILQ_INSERT_TAIL(&sched->blocked_queue, p, entry);

	spinlock_release(&ch->lk);

	PROC_DEBUG("Process %d is blocked for %s channel %d.\n",
		   p->pid, (reason == WAITING_FOR_SENDING) ? "sending to" :
		   "receiving from", channel_getid(ch));

	proc_sched(TRUE);
}

/*
 * XXX: The lock of the scheduler of the processor on which is the blocked
 *      process must be acquired before entering proc_unblock().
 *
 * XXX: Only blocked processes can be unblocked.
 *
 * XXX: proc_unblock() can unblock processes on another processor.
 */
void
proc_unblock(struct proc *p)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_BLOCKED);
	KERN_ASSERT(spinlock_holding(&p->cpu->sched.lk) == TRUE);

	proc_ready(p, p->cpu, TRUE);

	PROC_DEBUG("Process %d is unblocked.\n", p->pid);

	/*
	 * If the process is on another processor, send an IPI to trigger the
	 * scheduler on the processor.
	 */
	if (p->cpu != pcpu_cur())
		lapic_send_ipi(p->cpu->arch_info.lapicid,
			       T_IRQ0+IRQ_IPI_RESCHED,
			       LAPIC_ICRLO_FIXED, LAPIC_ICRLO_NOBCAST);
	else
		proc_sched(TRUE);
}

/*
 * XXX: The lock of the scheduler of the current processor must be acquired
 *      before entering proc_sched().
 *
 * XXX: proc_sched() only schedules processes on the current processor.
 *
 * XXX: If need_sched is TRUE and the ready queue is not empty, proc_sched()
 *      will select one process from the ready queue as the new current
 *      process.
 */
void
proc_sched(bool need_sched)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(spinlock_holding(&pcpu_cur()->sched.lk) == TRUE);

	struct proc *new_curp;
	struct pcpu *c = pcpu_cur();
	struct sched *sched = &c->sched;
	bool select_new = FALSE;

	new_curp = sched->cur_proc;

	if (unlikely((sched->cur_proc == NULL ||
		      sched->cur_proc->state == PROC_READY) &&
		     TAILQ_EMPTY(&sched->ready_queue)))
		KERN_PANIC("No schedulable process.\n");

	if (sched->cur_proc != NULL && sched->cur_proc->state != PROC_RUNNING) {
		/*
		 * If the current process is already moved to the ready queue or
		 * the blocked queue, force to select a new process.
		 */
		select_new = TRUE;
	} else if (sched->cur_proc != NULL &&
		   (need_sched || sched->run_ticks > SCHED_SLICE) &&
		   !TAILQ_EMPTY(&sched->ready_queue)) {
		/*
		 * Otherwise, if
		 * - the current process is not empty,
		 * - need_sched is TRUE, or the current process runs out of its
		 *   time slice, and
		 * - the ready queue is not empty,
		 * then move the current process to the ready queue and force to
		 * select a new process.
		 */
		proc_ready(sched->cur_proc, c, FALSE);
		select_new = TRUE;
	}

	if (sched->cur_proc == NULL || select_new == TRUE) {
		new_curp = TAILQ_FIRST(&sched->ready_queue);
		TAILQ_REMOVE(&sched->ready_queue, new_curp, entry);

		PROC_DEBUG("Process %d is selected.\n", new_curp->pid);
	}

	if (sched->cur_proc != new_curp) {
		sched->run_ticks = 0;
		proc_switch(new_curp);
	}

	PROC_DEBUG("proc_sched(): process %d returns to 0x%08x.\n",
		   sched->cur_proc->pid, *(uint32_t *) (read_ebp() + 4));
}

void
proc_sched_update(void)
{
	sched_lock(pcpu_cur());
	pcpu_cur()->sched.run_ticks += (1000 / LAPIC_TIMER_INTR_FREQ);
	PROC_DEBUG("+%d to run_ticks on CPU%d.\n",
		   (1000 / LAPIC_TIMER_INTR_FREQ), pcpu_cpu_idx(pcpu_cur()));
	sched_unlock(pcpu_cur());
}

void
proc_yield(void)
{
	KERN_ASSERT(proc_inited == TRUE);

	struct sched *sched = &pcpu_cur()->sched;

	sched_lock(pcpu_cur());

	if (!TAILQ_EMPTY(&sched->ready_queue)) {
		proc_ready(sched->cur_proc, pcpu_cur(), FALSE);
		proc_sched(TRUE);
	}

	sched_unlock(pcpu_cur());
}

struct proc *
proc_cur(void)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(pcpu_cur() != NULL);
	return pcpu_cur()->sched.cur_proc;
}

struct proc *
proc_pid2proc(pid_t pid)
{
	KERN_ASSERT(proc_inited == TRUE);
	if (!(0 <= pid && pid < MAX_PID))
		return NULL;
	return &process[pid];
}

struct channel *
proc_create_channel(struct proc *p1, struct proc *p2, channel_type type)
{
	KERN_ASSERT(p1 != NULL && p2 != NULL);
	if (p1->session != p2->session)
		return NULL;
	return channel_alloc(p1, p2, type);
}

/*
 * XXX: If the receiver is waiting for the message, proc_send_msg() will wake up
 *      it by moving it from the blocked queue to the head of the ready queue.
 */
int
proc_send_msg(struct channel *ch, struct proc *sender, void *msg, size_t size)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(sender != NULL);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(msg != NULL);
	KERN_ASSERT((uintptr_t) msg + size < VM_USERLO);
	KERN_ASSERT(size > 0);

	int rc;

	spinlock_acquire(&ch->lk);

	while ((rc = channel_send(ch, sender, msg, size)) == E_CHANNEL_BUSY) {
		PROC_DEBUG("Process %d: channel %d is busy, wait for a while.\n",
			   sender->pid, channel_getid(ch));

		sched_lock(pcpu_cur());
		proc_block(sender, WAITING_FOR_SENDING, ch);
		spinlock_acquire(&ch->lk);
		sched_unlock(pcpu_cur());
	}

	spinlock_release(&ch->lk);

	if (rc == 0) {
		PROC_DEBUG("Process %d: %d bytes are sent to channel %d.\n",
			   sender->pid, size, channel_getid(ch));

		struct proc *receiver =
			(ch->p1 == sender) ? ch->p2 : ch->p1;
		sched_lock(receiver->cpu);
		if (receiver->state == PROC_BLOCKED &&
		    receiver->block_reason == WAITING_FOR_RECEIVING &&
		    receiver->block_channel == ch) {
			PROC_DEBUG("Unblock process %d to receive from "
				   "channel %d.\n",
				   receiver->pid, channel_getid(ch));
			proc_unblock(receiver);
		}
		sched_unlock(receiver->cpu);
	} else {
		PROC_DEBUG("Process %d: cannot send to channel %d, errno %d.\n",
			   sender->pid, channel_getid(ch), rc);
	}

	return rc;
}

/*
 * XXX: If the sender is waiting for the completion of sending the message,
 *      proc_recv_msg() will wake up it by moving it from the blocked queue
 *      to the head of the ready queue.
 */
int
proc_recv_msg(struct channel *ch,
	      struct proc *receiver, void *msg, size_t *size, bool block)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(receiver != NULL);
	KERN_ASSERT(ch != NULL);
	KERN_ASSERT(msg != NULL);
	KERN_ASSERT((uintptr_t) msg + CHANNEL_BUFFER_SIZE <= VM_USERLO);
	KERN_ASSERT(size != NULL);

	int rc;

	spinlock_acquire(&ch->lk);

	while ((rc = channel_receive(ch, receiver, msg, size))
	       == E_CHANNEL_IDLE) {
		PROC_DEBUG("Process %d: channel %d is empty, %s.\n",
			   receiver->pid, channel_getid(ch),
			   (block == TRUE) ?
			   "wait for a while" : "return immediately");

		if (block == TRUE) {
			sched_lock(pcpu_cur());
			proc_block(receiver, WAITING_FOR_RECEIVING, ch);
			spinlock_acquire(&ch->lk);
			sched_unlock(pcpu_cur());
		} else {
			spinlock_release(&ch->lk);
			return rc;
		}
	}

	spinlock_release(&ch->lk);

	if (rc == 0) {
		PROC_DEBUG("Process %d: %d bytes are received from channel %d.\n",
			   receiver->pid, *size, channel_getid(ch));

		struct proc *sender =
			(ch->p2 == receiver) ? ch->p1 : ch->p2;
		sched_lock(sender->cpu);
		if (sender->state == PROC_BLOCKED &&
		    sender->block_reason == WAITING_FOR_SENDING &&
		    sender->block_channel == ch) {
			PROC_DEBUG("Unblock process %d to send to channel %d.\n",
				   sender->pid, channel_getid(ch));
			proc_unblock(sender);
		}
		sched_unlock(sender->cpu);
	} else {
		PROC_DEBUG("Process %d: cannot receive from channel %d, "
			   "errno %d.\n",
			   receiver->pid, channel_getid(ch), rc);
	}

	return rc;
}

void
proc_save(struct proc *p, tf_t *tf)
{
	KERN_ASSERT(proc_inited == TRUE);
	KERN_ASSERT(p != NULL);
	KERN_ASSERT(p->state == PROC_RUNNING);
	KERN_ASSERT(tf != NULL);

	proc_lock(p);
	p->ctx.tf = *tf;
	proc_unlock(p);
}

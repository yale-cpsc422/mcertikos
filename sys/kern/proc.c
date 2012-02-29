#include <sys/as.h>
#include <sys/context.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/msg.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>

#define NPROC		64

struct {
	spinlock_t	lk;

	proc_t		proc[NPROC];
	bool		used[NPROC];
} ptable;

static bool proc_inited;

/*
 * Check whether a proc structure is in ptable.proc[].
 *
 * @param proc the proc structure to be checked
 *
 * @return TRUE if it's in ptable.proc[]; otherwise, FALSE.
 */
static bool
proc_is_valid(proc_t *proc)
{
	KERN_ASSERT(spinlock_holding(&ptable.lk) == TRUE);

	if (proc == NULL) {
		KERN_DEBUG("proc is NULL.\n");
		return FALSE;
	}

	int i;

	for (i = 0; i < NPROC; ++i)
		if (&ptable.proc[i] == proc)
			break;

	if (i == NPROC) {
		KERN_DEBUG("no such proc.\n");
		return FALSE;
	}

	return ptable.used[i];
}

/*
 * Search proc in ptable.proc[]. If it exists in ptable.proc[], return its
 * process id; otherwise, return 0, which stands for an invalid process id.
 *
 * @param proc the proc structure to be searched
 *
 * @return the process id of proc if it's in ptable.proc[]; otherwise return
 *         0.
 */
static pid_t
proc2pid(proc_t *proc)
{
	KERN_ASSERT(spinlock_holding(&ptable.lk) == TRUE);

	int i;
	for (i = 0; i < NPROC; i++)
		if (&ptable.proc[i] == proc)
			break;

	if (i == NPROC)
		return ((pid_t) 0);
	else
		return ((pid_t) (i + 1));
}

/*
 * Search process whose process id is pid in ptable.proc[]. If one exists,
 * return the pointer to the proc structure; otherwise, return NULL.
 *
 * @param pid the process to be searched
 *
 * @return the pointer to the proc structure if the process exists;
 *         otherwise NULL.
 */
static proc_t *
pid2proc(pid_t pid)
{
	KERN_ASSERT(spinlock_holding(&ptable.lk) == TRUE);

	if (pid <= 0 || pid > NPROC)
		return NULL;

	return &ptable.proc[pid-1];
}

/*
 * Allocated an unused proc structure from ptable.proc[], and mark that
 * structure used if existing.
 *
 * @return the pointer to a proc structure, or NULL if there is no useable
 *         proc structure.
 */
static proc_t *
proc_alloc(void)
{
	spinlock_acquire(&ptable.lk);

	int i;
	for (i = 0; i < NPROC; i++)
		if (ptable.used[i] == FALSE)
			break;

	if (i == NPROC) {
		spinlock_release(&ptable.lk);
		return NULL;
	}

	ptable.proc[i].pid = (pid_t) (i + 1);
	ptable.used[i] = TRUE;

	spinlock_release(&ptable.lk);

	return &ptable.proc[i];
}

/*
 * Free a proc structure. Zero the structure and mark it unused in ptable.
 *
 * @param proc the proc structure to be freed
 */
static void
proc_free(proc_t *proc)
{
	if (proc == NULL) {
		KERN_WARN("proc is a NULL pointer.\n");
		return;
	}

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);
	KERN_ASSERT(proc->state == PROC_DEAD);

	spinlock_acquire(&ptable.lk);
	pid_t pid = proc2pid(proc);
	spinlock_release(&ptable.lk);

	if (proc == 0) {
		KERN_WARN("proc is an invalid process.\n");
		return;
	}

	memset(proc, 0x0, sizeof(proc_t));

	spinlock_acquire(&ptable.lk);
	ptable.used[pid - 1] = FALSE;
	spinlock_release(&ptable.lk);
}

/*
 * Initialize process related structures (i.e. ptable).
 */
void
proc_init(void)
{
	if (proc_inited == TRUE)
		return;

	spinlock_init(&ptable.lk);

	int i;
	for (i = 0; i < NPROC; i++) {
		memset(&ptable.proc[i], 0x0, sizeof(proc_t));
		ptable.used[i] = FALSE;
	}

	proc_inited = TRUE;
}

/*
 * Create a new process and mark the process as READY.
 *
 * @param binary the binary code the process will execute
 *
 * @return the pointer to the proc structure.
 */
pid_t
proc_new(uintptr_t binary)
{
	proc_t *proc  = proc_alloc();

	if (proc == NULL) {
		KERN_DEBUG("Cannot allocate a proc structure.\n");
		return NULL;
	}

	spinlock_init(&proc->lk);

	proc->as = as_new(TRUE);

	if (proc->as == NULL) {
		KERN_DEBUG("Cannot allocate address space for process %d\n",
			   proc->pid);

		proc->state = PROC_DEAD;
		spinlock_acquire(&proc->lk);
		proc_free(proc);
		spinlock_release(&proc->lk);

		return NULL;
	}

	elf_load(proc->as, binary);

	proc->normal_ctx =
		context_new((void (*)(void)) elf_entry(binary),
			    VM_STACKHI - PAGESIZE);
	if (proc->normal_ctx == NULL) {
		KERN_DEBUG("Cannot create normal execution context for process %d\n",
			   proc->pid);

		proc->state = PROC_DEAD;
		spinlock_acquire(&proc->lk);
		proc_free(proc);
		spinlock_release(&proc->lk);

		return NULL;
	}
	as_assign(proc->as, VM_STACKHI - PAGESIZE,
		  PTE_P | PTE_U | PTE_W, mem_ptr2pi(proc->normal_ctx));
	proc->signal_ctx = NULL;

	mqueue_init(&proc->mqueue);

	proc->state = PROC_READY;

	return proc->pid;
}

/*
 * Start executing process proc.
 *
 * @param proc the process to be executed
 */
void gcc_noreturn
proc_start(pid_t pid)
{
	spinlock_acquire(&ptable.lk);
	proc_t *proc = pid2proc(pid);
	KERN_ASSERT(proc_is_valid(proc) == TRUE);
	spinlock_release(&ptable.lk);

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);

	KERN_ASSERT(proc->state == PROC_READY);
	KERN_ASSERT(proc->as != NULL);
	KERN_ASSERT(proc->normal_ctx != NULL);

	pcpu_t *c = pcpu_cur();
	spinlock_acquire(&c->lk);
	c->proc = proc;
	spinlock_release(&c->lk);

	proc->state = PROC_RUN;

	as_activate(proc->as);

	spinlock_release(&proc->lk);

	/* XXX: release proc->lk in context_start()? */
	context_start(proc->normal_ctx);
}

/*
 * Send a message to process proc.
 *
 * @param proc the receiver process
 * @param type the type of the message
 * @param data the data of the message
 * @param size the size in bytes of the message
 *
 * @return 0 if sending succeed; otherwise, -1
 */
int
proc_send_msg(pid_t pid, msg_type_t type, void *data, size_t size)
{
	int succ;

	spinlock_acquire(&ptable.lk);
	proc_t *proc = pid2proc(pid);
	KERN_ASSERT(proc_is_valid(proc) == TRUE);
	spinlock_release(&ptable.lk);

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);

	spinlock_acquire(&proc->mqueue.lk);
	succ = mqueue_enqueue(&proc->mqueue, type, data, size);
	spinlock_release(&proc->mqueue.lk);

	return succ;
}

/*
 * Let process proc receive a message.
 *
 * @param proc the receiver message
 *
 * @return a pointer to the message if succeed; otherwise, NULL.
 */
msg_t *
proc_recv_msg(pid_t pid)
{
	msg_t *msg;

	spinlock_acquire(&ptable.lk);
	proc_t *proc = pid2proc(pid);
	KERN_ASSERT(proc_is_valid(proc) == TRUE);
	spinlock_release(&ptable.lk);

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);

	spinlock_acquire(&proc->mqueue.lk);
	msg = mqueue_dequeue(&proc->mqueue);
	spinlock_release(&proc->mqueue.lk);

	return msg;
}

as_t *
proc_as(pid_t pid)
{
	spinlock_acquire(&ptable.lk);
	proc_t *proc = pid2proc(pid);
	KERN_ASSERT(proc_is_valid(proc) == TRUE);
	spinlock_release(&ptable.lk);

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);

	return proc->as;
}

void
proc_lock(pid_t pid)
{
	spinlock_acquire(&ptable.lk);
	proc_t *proc = pid2proc(pid);
	KERN_ASSERT(proc_is_valid(proc) == TRUE);
	spinlock_release(&ptable.lk);

	KERN_ASSERT(spinlock_holding(&proc->lk) == FALSE);

	spinlock_acquire(&proc->lk);
}

void
proc_unlock(pid_t pid)
{
	spinlock_acquire(&ptable.lk);
	proc_t *proc = pid2proc(pid);
	KERN_ASSERT(proc_is_valid(proc) == TRUE);
	spinlock_release(&ptable.lk);

	KERN_ASSERT(spinlock_holding(&proc->lk) == TRUE);

	spinlock_release(&proc->lk);
}

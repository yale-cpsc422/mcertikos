#include <sys/context.h>
#include <sys/debug.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include <machine/pmap.h>

#ifdef DEBUG_SYSCALL

#define SYSCALL_DEBUG(fmt...)			\
	{					\
		KERN_DEBUG(fmt);		\
	}

static char *syscall_name[256] =
	{
		[SYS_puts]	= "sys_puts",
		[SYS_getc]	= "sys_getc",
		[SYS_yield]	= "sys_yield",
		[SYS_getpid]	= "sys_getpid",
		[SYS_send]	= "sys_send",
		[SYS_recv]	= "sys_recv",
		[SYS_allocvm]	= "sys_allocvm",
		[SYS_execvm]	= "sys_execvm",
		[SYS_test]	= "sys_test",
	};

#else

#define SYSCALL_DEBUG(fmt...)			\
	{					\
	}

#endif

/*
 * Copy data from user's virtual address space to kernel's virtual address
 * space.
 *
 * @param dest kernel's virtual address
 * @param src user's virtual address
 * @param size the size of the data
 *
 * @return the kernel's virtual address if copy succeed; otherwise, NULL.
 */
static void *
copy_from_user(void *dest, void *src, size_t size)
{
	if (dest == NULL || src == NULL || size == 0 ||
	    (uintptr_t) dest + size > VM_USERLO || (uintptr_t) src < VM_USERLO)
		return NULL;

	struct proc *user_proc;
	pmap_t *user_pmap;

	user_proc = proc_cur();
	KERN_ASSERT(spinlock_holding(&user_proc->lk) == TRUE);

	user_pmap = user_proc->pmap;
	KERN_ASSERT(user_pmap == (pmap_t *) rcr3());

	if (pmap_checkrange(user_pmap, (uintptr_t) dest, size) == FALSE) {
		SYSCALL_DEBUG("%x ~ %x do not fit in the kernel address space.\n",
			   dest, dest+size);
		return NULL;
	}

	if (pmap_checkrange(user_pmap, (uintptr_t) src, size) == FALSE) {
		SYSCALL_DEBUG("%x ~ %x do not fit in the user address space.\n",
			   src, src+size);
		return NULL;
	}

	memcpy(dest, src, size);

	return dest;
}

/*
 * Copy data from kernel's virtual address space to user's virtual address
 * space. If there is not enough virtual space in user's virtual address space,
 * try to allocate more space for user.
 *
 * @param dest user's virtual address
 * @param src kernel's virtual address
 * @param size the size in bytes of the data
 *
 * @return the user's virtual address if copy succeed; otherwise, NULL.
 */
static void *
copy_to_user(void *dest, void *src, size_t size)
{
	if (dest == NULL || src == NULL || size == 0 ||
	    (uintptr_t) dest < VM_USERLO || (uintptr_t) src + size > VM_USERLO)
		return NULL;

	struct proc *user_proc;
	pmap_t *user_pmap;

	user_proc = proc_cur();
	KERN_ASSERT(spinlock_holding(&user_proc->lk) == TRUE);

	user_pmap = user_proc->pmap;
	KERN_ASSERT(user_pmap == (pmap_t *) rcr3());

	if (pmap_checkrange(user_pmap, (uintptr_t) src, size) == FALSE) {
		SYSCALL_DEBUG("%x ~ %x do not fit in the kernel address space.\n",
			   src, src+size);
		return NULL;
	}

	if (pmap_checkrange(user_pmap, (uintptr_t) dest, size) == FALSE) {
		SYSCALL_DEBUG("%x ~ %x do not fit in the user address space.\n",
			   dest, dest+size);
		return NULL;
	}

	memcpy(dest, src, size);

	return dest;
}

static int
sys_test(uint32_t arg)
{
	SYSCALL_DEBUG(" test: arg0 0x%08x\n", arg);
	return 0;
}

static int
sys_getc(struct pcpu *c, struct proc *p, char *buf)
{
	int rc = 0;

	*(char *) c->sys_buf = getchar();
	proc_lock(p);
	if (copy_to_user(buf, c->sys_buf, sizeof(char)) == NULL)
		rc = 1;
	proc_unlock(p);
	memset(c->sys_buf, 0x0, sizeof(char));

	return rc;
}

static int
sys_puts(struct pcpu *c, struct proc *p, char *buf)
{
	int rc = 0;

	proc_lock(p);
	if (copy_from_user(c->sys_buf, buf, PAGE_SIZE) == NULL) {
		rc = 1;
		proc_unlock(p);
		goto ret;
	}
	proc_unlock(p);

	((char *) (c->sys_buf))[PAGE_SIZE - 1] = '\0';
	cprintf("%s", (char *) c->sys_buf);

 ret:
	memset(c->sys_buf, 0x0, PAGE_SIZE);
	return rc;
}

static int
sys_spawn(struct pcpu *c, struct proc *p, uintptr_t exe, pid_t *pid)
{
	int rc = 0;

	struct proc *q;

	if ((q = proc_create(exe)) == NULL) {
		*(pid_t *) c->sys_buf = PID_INV;
		proc_lock(p);
		copy_to_user(pid, c->sys_buf, sizeof(pid_t));
		proc_unlock(p);
		memset(c->sys_buf, 0, sizeof(pid_t));
		rc = 1;
		goto ret;
	}

	proc_add2sched(q);

	*(pid_t *) c->sys_buf = q->pid;
	proc_lock(p);
	copy_to_user(pid, c->sys_buf, sizeof(pid_t));
	proc_unlock(p);
	memset(c->sys_buf, 0, sizeof(pid_t));

 ret:
	return rc;
}

static int
sys_yield(struct proc *p)
{
	KERN_ASSERT(p != NULL);

	if (proc_cur() != p)
		return 1;

	return proc_yield();
}

static int
sys_getpid(struct pcpu *c, struct proc *p, pid_t *pid)
{
	int rc = 0;

	KERN_ASSERT(p != NULL);

	proc_lock(p);

	*(pid_t *) c->sys_buf = p->pid;

	if (copy_to_user(pid, c->sys_buf, sizeof(pid_t)) == NULL)
		rc = 1;

	proc_unlock(p);

	memset(c->sys_buf, 0x0, sizeof(pid_t));

	return rc;
}

static int
sys_send(struct pcpu *c, struct proc *sender,
	 pid_t receiver_pid, void *data, size_t size)
{
	int rc  = 0;
	struct proc *receiver;

	SYSCALL_DEBUG("Send message to process %d, size %d.\n",
		      receiver_pid, size);

	if (c == NULL || sender == NULL || data == NULL) {
		SYSCALL_DEBUG("Illegal parameters.\n");
		rc = 2;
		goto ret;
	}

	if ((receiver = proc_pid2proc(receiver_pid)) == NULL) {
		SYSCALL_DEBUG("Cannnot find proecss %d.\n", receiver_pid);
		rc = 1;
		goto ret;
	}

	proc_lock(sender);
	if (copy_from_user(c->sys_buf, data, size) == NULL) {
		SYSCALL_DEBUG("Cannot copy data from userspace.\n");
		proc_unlock(sender);
		rc = 3;
		goto ret;
	}
	proc_unlock(sender);

	rc = proc_send_msg(receiver, c->sys_buf, size);

 ret:
	memzero(c->sys_buf, size);
	return rc;
}

static int
sys_recv_unblock(struct proc *p)
{
	int rc = 0;
	struct message *msg;
	uint8_t *data;
	size_t *size;

	if (p == NULL) {
		SYSCALL_DEBUG("Illegal parameters.\n");
		rc = 2;
		goto ret;
	}

	KERN_ASSERT(spinlock_holding(&p->lk) == TRUE);
	KERN_ASSERT(p->blocked_for == WAIT_FOR_MSG);

	data = (uint8_t *) ctx_arg2(&p->ctx);
	size = (size_t *) ctx_arg3(&p->ctx);

	spinlock_acquire(&p->mqueue.lk);
	msg = mqueue_dequeue(&p->mqueue);
	spinlock_release(&p->mqueue.lk);

	if (msg == NULL) {
		KERN_WARN("sys_recv_unblock() receives an empty message.\n");
		*size = 0;
	} else {
		if (copy_to_user(data, msg->data, msg->size) == NULL) {
			SYSCALL_DEBUG("Cannot copy data to userspace.\n");
			rc = 3;
			goto ret;
		}
		*size = msg->size;
	}

 ret:
	return rc;
}

static int
sys_recv(struct pcpu *c, struct proc *receiver, void *buf, size_t *size)
{
	int rc = 0;
	struct message *msg;

	if (c == NULL || receiver == NULL || buf == NULL || size == NULL) {
		SYSCALL_DEBUG("Illegal parameters.\n");
		rc = 2;
		goto ret;
	}

	msg = proc_recv_msg();

	if (msg == NULL) {
		proc_lock(receiver);
		receiver->unblock_callback = sys_recv_unblock;
		proc_unlock(receiver);
	} else {
		proc_lock(receiver);
		if (copy_to_user(buf, msg->data, msg->size) == NULL) {
			SYSCALL_DEBUG("Cannot copy data to userspace.\n");
			rc = 1;
			proc_unlock(receiver);
			goto ret;
		}
		*size = msg->size;
		proc_unlock(receiver);
	}

 ret:
	return rc;
}

static int
sys_allocvm(struct pcpu *c, struct proc *p)
{
	int rc = 0;

	if (pcpu_onboot() != TRUE) {
		SYSCALL_DEBUG("Only support running the virtual machine on the "
			      "bootstrap processor.\n");
		rc = 1;
		goto ret;
	}

	proc_lock(p);

	if (p->vm != NULL) {
		SYSCALL_DEBUG("Cannot create the virtual machine in process %d."
			      " There is already a virtual machine "
			      "in this process.\n", p->pid);
		rc = 2;
		goto lock_ret;
	}

	if ((p->vm = vmm_init_vm()) == NULL) {
		SYSCALL_DEBUG("Cannot create a virtual machine "
			      "in process %d.\n", p->pid);
		rc = 3;
		goto lock_ret;
	}

 lock_ret:
	proc_unlock(p);
 ret:
	return rc;
}

static int
sys_execvm(struct pcpu *c, struct proc *p)
{
	int rc = 0;

	if (pcpu_onboot() != TRUE) {
		SYSCALL_DEBUG("Only support running the virtual machine on the "
			      "bootstrap processor.\n");
		rc = 1;
		goto ret;
	}

	proc_lock(p);

	if (p->vm == NULL) {
		SYSCALL_DEBUG("No virtual machine in process %d.\n", p->pid);
		rc = 2;
		goto lock_ret;
	}

	proc_unlock(p);
	vmm_run_vm(p->vm);
	goto ret;

 lock_ret:
	proc_unlock(p);
 ret:
	return rc;
}

int
syscall_handler(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

	uint32_t nr = ctx_arg1(ctx);
	uint32_t a[3] =
		{ctx_arg2(ctx), ctx_arg3(ctx), ctx_arg4(ctx)};
	uint32_t rc = 0;
	struct pcpu *c = pcpu_cur();
	struct proc *p = proc_cur();

	KERN_ASSERT(p != NULL);

	KERN_ASSERT(c != NULL);
	spinlock_acquire(&c->lk);

	SYSCALL_DEBUG("syscall %s (nr %d) from process %d on CPU %d, RIP 0x%08x.\n",
		   (0 <= nr && nr < 256) ? syscall_name[nr] : "unkown",
		   nr, p->pid, pcpu_cur_idx(), ctx->tf.eip);
	/* ctx_dump(ctx); */

	switch (nr) {
	case SYS_test:
		rc = sys_test(a[0]);
		break;

	case SYS_getc:
		/*
		 * Read a character from the console.
		 * a[0]: the address where the character will be stored.
		 */
		rc = sys_getc(c, p, (char *) a[0]);
		break;

	case SYS_puts:
		/*
		 * Output a string to the console.
		 * a[0]: the address where the string is stored.
		 */
		rc = sys_puts(c, p, (char *) a[0]);
		break;

	case SYS_spawn:
		/*
		 * Spawn a process and put it on the ready queue.
		 * a[0]: the base address of the execution image
		 * a[1]: the address where the process id will be returned.
		 */
		rc = sys_spawn(c, p, a[0], (pid_t *) a[1]);
		break;

	case SYS_yield:
		/*
		 * Yield currently running process to other processes.
		 */
		rc = sys_yield(p);
		break;

	case SYS_getpid:
		/*
		 * Get the process id of currently running process.
		 */
		rc = sys_getpid(c, p, (pid_t *) a[0]);
		break;

	case SYS_send:
		/*
		 * Send a message to another process.
		 * a[0]: process id the receiver process
		 * a[1]: address of the data
		 * a[2]: size in bytes of the data of message
		 */
		rc = sys_send(c, p, (pid_t) a[0],
			      (struct message *) a[1], (size_t) a[2]);
		break;

	case SYS_recv:
		/*
		 * Receive a message.
		 * a[0]: address where the message data will be stored.
		 * a[1]: address where the data size will be stored
		 */
		rc = sys_recv(c, p, (char *) a[0], (size_t *) a[1]);
		break;

	case SYS_allocvm:
		/*
		 * Create a virtual machine in the current process.
		 */
		rc = sys_allocvm(c, p);
		break;

	case SYS_execvm:
		/*
		 * Execute the virtual machine in the current process.
		 */
		rc = sys_execvm(c, p);
		break;

	default:
		rc = 1;
		break;
	}

	ctx_set_retval(ctx, rc);

	if (rc) {
		SYSCALL_DEBUG("Syscall %s (nr %d) from "
			      "process %d on CPU %d failed, error %d.\n",
			      (0 <= nr && nr < 256) ? syscall_name[nr] : "unkown",
			      nr, p->pid, pcpu_cur_idx(), rc);
		memset(c->sys_buf, 0, PAGE_SIZE);
	}

	spinlock_release(&c->lk);
	return rc;
}

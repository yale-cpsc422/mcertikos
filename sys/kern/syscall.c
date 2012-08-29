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

static char *syscall_name[256] =
	{
		[SYS_puts]	= "sys_puts",
		[SYS_getc]	= "sys_getc",
		[SYS_spawn]	= "sys_spawn",
		[SYS_yield]	= "sys_yield",
		[SYS_getpid]	= "sys_getpid",
		[SYS_send]	= "sys_send",
		[SYS_recv]	= "sys_recv",
		[SYS_ncpus]	= "sys_ncpus",
		[SYS_getpchid]	= "sys_getpchid",
		[SYS_allocvm]	= "sys_allocvm",
		[SYS_execvm]	= "sys_execvm",
		[SYS_test]	= "sys_test",
	};

#define SYSCALL_DEBUG(fmt, args...)			\
	do {						\
		KERN_DEBUG("CPU%d:PID%d: "fmt,		\
			   pcpu_cpu_idx(pcpu_cur()),	\
			   proc_cur()->pid,		\
			   args);			\
	} while (0)

#define NR_DEBUG(nr, fmt, args...)					\
	do {								\
		KERN_DEBUG("CPU%d:PID%d:%s(nr %d): "fmt,		\
			   pcpu_cpu_idx(pcpu_cur()),			\
			   proc_cur()->pid,				\
			   (0 <= nr && nr < 256) ? syscall_name[nr] : "null", \
			   nr,						\
			   args);					\
	} while (0)

#else

#define SYSCALL_DEBUG(fmt...)			\
	do {					\
	} while (0)

#define NR_DEBUG(fmt...)			\
	do {					\
	} while (0)

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
	NR_DEBUG(SYS_test, "arg0 0x%08x\n", arg);
	return E_SUCC;
}

static int
sys_getc(char *buf)
{
	struct pcpu *c = pcpu_cur();

	*(char *) c->sys_buf = getchar();
	if (copy_to_user(buf, c->sys_buf, sizeof(char)) == NULL)
		return E_MEM_FAIL;
	else
		return E_SUCC;
}

static int
sys_puts(char *buf)
{
	struct pcpu *c = pcpu_cur();

	if (copy_from_user(c->sys_buf, buf, PAGE_SIZE) == NULL)
		return E_MEM_FAIL;

	((char *) (c->sys_buf))[PAGE_SIZE - 1] = '\0';
	cprintf("%s", (char *) c->sys_buf);

	return E_SUCC;
}

static int
sys_spawn(uint32_t cpu_idx, uintptr_t exe, pid_t *pid)
{
	int rc = E_SUCC;
	struct pcpu *cur_cpu = pcpu_cur();
	struct pcpu *c = &pcpu[cpu_idx];
	struct proc *child;

	if ((child = proc_spawn(c, exe)) == NULL) {
		*(pid_t *) cur_cpu->sys_buf = PID_INV;
		rc = E_SPAWN_FAIL;
	} else {
		*(pid_t *) cur_cpu->sys_buf = child->pid;
	}

	if (copy_to_user(pid, cur_cpu->sys_buf, sizeof(pid_t)) == NULL)
		rc = E_MEM_FAIL;

	return rc;
}

static int
sys_yield(void)
{
	proc_yield();
	return E_SUCC;
}

static int
sys_getpid(pid_t *pid)
{
	struct pcpu *c = pcpu_cur();
	struct proc *p = proc_cur();

	*(pid_t *) c->sys_buf = p->pid;

	if (copy_to_user(pid, c->sys_buf, sizeof(pid_t)) == NULL)
		return E_MEM_FAIL;
	else
		return E_SUCC;
}

static int
sys_allocvm(void)
{
	struct proc *p = proc_cur();

	if (pcpu_onboot() != TRUE) {
		NR_DEBUG(SYS_allocvm,
			 "Process %d is not on the bootstrap processor.\n",
			 p->pid);
		return E_VM_ON_AP;
	}

	proc_lock(p);

	if (p->vm != NULL) {
		NR_DEBUG(SYS_allocvm,
			 "A virtual machine is already running in process %d.\n",
			 p->pid);
		proc_unlock(p);
		return E_VM_EXIST;
	}

	if ((p->vm = vmm_init_vm()) == NULL) {
		NR_DEBUG(SYS_allocvm,
			 "Cannot initialize a virtual machine in process %d.\n",
			 p->pid);
		proc_unlock(p);
		return E_VM_INIT_FAIL;
	}

	proc_unlock(p);
	return E_SUCC;
}

static int
sys_execvm(void)
{
	struct proc *p = proc_cur();

	if (pcpu_onboot() != TRUE) {
		NR_DEBUG(SYS_execvm,
			 "Process %d is not on the bootstrap processor.\n",
			 p->pid);
		return E_VM_ON_AP;
	}

	proc_lock(p);

	if (p->vm == NULL) {
		NR_DEBUG(SYS_execvm,
			 "No virtual machine in process %d.\n", p->pid);
		proc_unlock(p);
		return E_NO_VM;
	}

	proc_unlock(p);
	vmm_run_vm(p->vm);

	return E_SUCC;
}

static int
sys_ncpus(uint32_t *n)
{
	struct pcpu *c = pcpu_cur();
	*(uint32_t *) c->sys_buf = pcpu_ncpu();
	if (copy_to_user(n, c->sys_buf, sizeof(uint32_t)) == NULL)
		return E_MEM_FAIL;
	else
		return E_SUCC;
}

static int
sys_getpchid(int *chid)
{
	struct pcpu *c = pcpu_cur();
	struct proc *cur_proc = proc_cur();;
	int errno;

	if ((cur_proc = proc_cur()) == NULL) {
		*(int *) c->sys_buf = -1;
		errno = E_INVAL_PROC;
	} else if (cur_proc->parent == NULL || cur_proc->parent_ch == NULL) {
		*(int *) c->sys_buf = -1;
		errno = E_NO_CHANNEL;
	} else {
		*(int *) c->sys_buf = channel_getid(cur_proc->parent_ch);
		errno = E_SUCC;
	}

	if (copy_to_user(chid, c->sys_buf, sizeof(int)) == NULL)
		errno = E_MEM_FAIL;

	return errno;
}

static int
sys_send(int channel_id, void *msg, size_t size)
{
	int errno;
	struct channel *ch;

	ch = channel_getch(channel_id);

	if (ch == NULL || (ch->state & CHANNEL_STAT_INITED) == 0)
		return E_NO_PERM;

	if (msg == NULL || size == 0)
		return E_EMPTY_MSG;

	errno = proc_send_msg(ch, msg, size);

	switch (errno) {
	case E_CHANNEL_MSG_TOO_LARGE:
		return E_LARGE_MSG;
	case E_CHANNEL_ILL_SENDER:
		return E_NO_PERM;
	case E_CHANNEL_BUSY:
		return E_SEND_BUSY;
	default:
		return E_SUCC;
	}
}

static int
sys_recv(int channel_id, void *msg, size_t *size)
{
	int errno;
	struct channel *ch;

	ch = channel_getch(channel_id);

	if (ch == NULL || (ch->state & CHANNEL_STAT_INITED) == 0)
		return E_NO_PERM;

	if (msg == NULL || size == NULL)
		return E_EMPTY_MSG;

	errno = proc_recv_msg(ch, msg, size);

	switch (errno) {
	case E_CHANNEL_ILL_RECEIVER:
		return E_NO_PERM;
	case E_CHANNEL_IDLE:
		return E_RECV_IDLE;
	default:
		return E_SUCC;
	}
}

/*
 * Syetem calls in CertiKOS follow the convention below.
 *
 * - The system call number is passed through the first argument of the user
 *   context, i.e. %eax in the i386 architecture.
 *
 * - Each system call can have at most three parameters, which are passed
 *   through the second argument to the fourth argument of the user context
 *   respectively, i.e. %ebx, %ecx, and %edx in the i386 architecture.
 *
 * - The error number of the syscall is returned through the first argument of
 *   the user context, i.e. %eax in the i386 architecture. If the error number
 *   is zero, it means no error.
 *
 * - Other return value can be passed through the second argument to the fourth
 *   argument. Which one/ones is/are used by a system call is decided by the
 *   system call.
 */
int
syscall_handler(struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

	uint32_t nr = ctx_arg1(ctx);
	uint32_t a[3] =
		{ctx_arg2(ctx), ctx_arg3(ctx), ctx_arg4(ctx)};
	uint32_t errno = 0;

	NR_DEBUG(nr, "called from 0x%08x.\n", ctx->tf.eip);
	/* ctx_dump(ctx); */

	switch (nr) {
	case SYS_test:
		errno = sys_test(a[0]);
		break;

	case SYS_getc:
		/*
		 * Read a character from the console.
		 * a[0]: the address where the character will be stored.
		 */
		errno = sys_getc((char *) a[0]);
		break;

	case SYS_puts:
		/*
		 * Output a string to the console.
		 * a[0]: the address where the string is stored.
		 */
		errno = sys_puts((char *) a[0]);
		break;

	case SYS_spawn:
		/*
		 * Spawn a process and put it on the ready queue.
		 * a[0]: the index of the processor where the process will run
		 * a[1]: the base address of the execution image
		 * a[2]: the address where the process id will be returned.
		 */
		errno = sys_spawn(a[0], a[1], (pid_t *) a[2]);
		break;

	case SYS_yield:
		/*
		 * Yield currently running process to other processes.
		 */
		errno = sys_yield();
		break;

	case SYS_getpid:
		/*
		 * Get the process id of currently running process.
		 * a[0]: the address where the process id will be returned
		 */
		errno = sys_getpid((pid_t *) a[0]);
		break;

	case SYS_send:
		/*
		 * Send a meesage to a channel.
		 * a[0]: the channel id
		 * a[1]: the start address of the message
		 * a[2]: the size of the message
		 */
		errno = sys_send((int) a[0], (void *) a[1], (size_t) a[2]);
		break;

	case SYS_recv:
		/*
		 * Receive a meesage from a channel.
		 * a[0]: the channel id
		 * a[1]: the address where the received message will be stored
		 * a[2]: the address where the size of the message will be
		 *       stored
		 */
		errno = sys_recv((int) a[0], (void *) a[1], (size_t *) a[2]);
		break;

	case SYS_allocvm:
		/*
		 * Create a virtual machine in the current process.
		 */
		errno = sys_allocvm();
		break;

	case SYS_execvm:
		/*
		 * Execute the virtual machine in the current process.
		 */
		errno = sys_execvm();
		break;

	case SYS_ncpus:
		/*
		 * Get the number of processors.
		 * a[0]: the address where the number will be returned to
		 */
		errno = sys_ncpus((uint32_t *) a[0]);
		break;

	case SYS_getpchid:
		/*
		 * Get the id of the channel between the current process and its
		 * parent.
		 * a[0]: the address where the channel id will be returned
		 */
		errno = sys_getpchid((int *) a[0]);
		break;

	default:
		errno = E_INVAL_NR;
		break;
	}

	ctx_set_retval(ctx, errno);

	if (errno)
		NR_DEBUG(nr, "failed (errno=%d)\n", errno);

	memset(pcpu_cur()->sys_buf, 0, PAGE_SIZE);

	return errno;
}

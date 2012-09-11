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
#include <sys/virt/vmm_dev.h>

#include <machine/pmap.h>

#ifdef DEBUG_SYSCALL

static char *syscall_name[256] =
	{
		[SYS_puts]		= "sys_puts",
		[SYS_getc]		= "sys_getc",
		[SYS_spawn]		= "sys_spawn",
		[SYS_yield]		= "sys_yield",
		[SYS_getpid]		= "sys_getpid",
		[SYS_send]		= "sys_send",
		[SYS_recv]		= "sys_recv",
		[SYS_recv_nonblock]	= "sys_recv_nonblock",
		[SYS_ncpus]		= "sys_ncpus",
		[SYS_getpchid]		= "sys_getpchid",
		[SYS_allocvm]		= "sys_allocvm",
		[SYS_execvm]		= "sys_execvm",
		[SYS_test]		= "sys_test",
		[SYS_register_ioport]	= "sys_register_ioport",
		[SYS_unregister_ioport]	= "sys_unregister_ioport",
		[SYS_register_irq]	= "sys_register_irq",
		[SYS_unregister_irq]	= "sys_unregister_irq",
		[SYS_register_pic]	= "sys_register_pic",
		[SYS_unregister_pic]	= "sys_unregister_pic",
		[SYS_register_mmio]	= "sys_register_mmio",
		[SYS_unregister_mmio]	= "sys_unregister_mmio",
		[SYS_read_ioport]	= "sys_read_ioport",
		[SYS_write_ioport]	= "sys_write_ioport",
		[SYS_raise_irq]		= "sys_raise_irq",
		[SYS_trigger_irq]	= "sys_trigger_irq",
		[SYS_lower_irq]		= "sys_lower_irq",
		[SYS_notify_irq]	= "sys_notify_irq",
		[SYS_read_guest_tsc]	= "sys_read_guest_tsc",
		[SYS_guest_tsc_freq]	= "sys_guest_tsc_freq",
		[SYS_guest_mem_size]	= "sys_guest_mem_size",
		[SYS_getchid]		= "sys_getchid",
	};

#define SYSCALL_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("CPU%d:PID%d: "fmt,		\
			   pcpu_cpu_idx(pcpu_cur()),	\
			   proc_cur()->pid,		\
			   ##__VA_ARGS__);		\
	} while (0)

#define NR_DEBUG(nr, fmt, ...)						\
	do {								\
		KERN_DEBUG("CPU%d:PID%d:%s(nr %d): "fmt,		\
			   pcpu_cpu_idx(pcpu_cur()),			\
			   proc_cur()->pid,				\
			   (0 <= nr && nr < 256) ? syscall_name[nr] : "null", \
			   nr,						\
			   ##__VA_ARGS__);				\
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
	KERN_INFO("%s", (char *) c->sys_buf);

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

	errno = proc_send_msg(ch, proc_cur(), msg, size);

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

static gcc_inline int
sys_recv_helper(int channel_id, void *msg, size_t *size, bool blocking)
{
	int errno;
	struct channel *ch;

	ch = channel_getch(channel_id);

	if (ch == NULL || (ch->state & CHANNEL_STAT_INITED) == 0)
		return E_NO_PERM;

	if (msg == NULL || size == NULL)
		return E_EMPTY_MSG;

	errno = proc_recv_msg(ch, proc_cur(), msg, size, blocking);

	switch (errno) {
	case E_CHANNEL_ILL_RECEIVER:
		return E_NO_PERM;
	case E_CHANNEL_IDLE:
		return E_RECV_IDLE;
	default:
		return E_SUCC;
	}
}

static int
sys_recv(int channel_id, void *msg, size_t *size)
{
	return sys_recv_helper(channel_id, msg, size, TRUE);
}

static int
sys_recv_nonblock(int channel_id, void *msg, size_t *size)
{
	return sys_recv_helper(channel_id, msg, size, FALSE);
}

int
sys_register_ioport(uint16_t port, data_sz_t width, int write)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (port >= MAX_IOPORT)
		return E_INVAL_IOPORT;

	if (width != SZ8 && width != SZ16 && width != SZ32)
		return E_INVAL_WIDTH;

	if (vdev_register_ioport(vm, proc_cur(),
				 port, width, (write != 0) ? TRUE : FALSE))
		return E_REG_FAIL;

	return E_SUCC;
}

int
sys_unregister_ioport(uint16_t port, data_sz_t width, int write)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (port >= MAX_IOPORT)
		return E_INVAL_IOPORT;

	if (width != SZ8 && width != SZ16 && width != SZ32)
		return E_INVAL_WIDTH;

	if (vdev_unregister_ioport(vm, proc_cur(),
				   port, width, (write != 0) ? TRUE : FALSE))
		return E_UNREG_FAIL;

	return E_SUCC;
}

int
sys_register_irq(uint8_t irq)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (irq >= MAX_IRQ)
		return E_INVAL_IRQ;

	if (vdev_register_irq(vm, proc_cur(), irq))
		return E_REG_FAIL;

	return E_SUCC;
}

int
sys_unregister_irq(uint8_t irq)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (irq >= MAX_IRQ)
		return E_INVAL_IRQ;

	if (vdev_unregister_irq(vm, proc_cur(), irq))
		return E_UNREG_FAIL;

	return E_SUCC;
}

int
sys_register_pic(void)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (vdev_register_pic(vm, proc_cur()))
		return E_REG_FAIL;

	return E_SUCC;
}

int
sys_unregister_pic(void)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (vdev_unregister_pic(vm, proc_cur()))
		return E_UNREG_FAIL;

	return E_SUCC;
}

int
sys_register_mmio(uintptr_t gpa, uintptr_t hla, size_t size)
{
	KERN_PANIC("Not implemented yet.\n");
	return 0;
}

int
sys_unregister_mmio(uintptr_t gpa, size_t size)
{
	KERN_PANIC("Not implemented yet.\n");
	return 0;
}

int
sys_read_ioport(uint16_t port, data_sz_t width, void *buf)
{
	uint32_t data;
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (port >= MAX_IOPORT)
		return E_INVAL_IOPORT;

	if (width != SZ8 && width != SZ16 && width != SZ32)
		return E_INVAL_WIDTH;

	vdev_host_ioport_read(vm, port, width, &data);

	if (copy_to_user(buf, &data, 1 << width) == NULL)
		return E_MEM_FAIL;

	return E_SUCC;
}

int
sys_write_ioport(uint16_t port, data_sz_t width, void *buf)
{
	uint32_t data;
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (port >= MAX_IOPORT)
		return E_INVAL_IOPORT;

	if (width != SZ8 && width != SZ16 && width != SZ32)
		return E_INVAL_WIDTH;

	if (copy_from_user(&data, buf, 1 << width) == NULL)
		return E_MEM_FAIL;

	vdev_host_ioport_write(vm, port, width, data);

	return E_SUCC;
}

int
sys_raise_irq(uint8_t irq)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (irq >= MAX_IRQ)
		return E_INVAL_IRQ;

	if (vdev_raise_irq(vm, irq))
		return E_IRQ_FAIL;

	return E_SUCC;
}

int
sys_trigger_irq(uint8_t irq)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (irq >= MAX_IRQ)
		return E_INVAL_IRQ;

	if (vdev_trigger_irq(vm, irq))
		return E_IRQ_FAIL;

	return E_SUCC;
}

int
sys_lower_irq(uint8_t irq)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (irq >= MAX_IRQ)
		return E_INVAL_IRQ;

	if (vdev_lower_irq(vm, irq))
		return E_IRQ_FAIL;

	return E_SUCC;
}

int
sys_notify_irq(void)
{
	struct vm *vm = vmm_cur_vm();

	if (vm == NULL)
		return E_NO_VM;

	if (vdev_notify_irq(vm))
		return E_IRQ_FAIL;

	return E_SUCC;
}

static int
sys_read_guest_tsc(uint64_t *tsc)
{
	struct vm *vm = vmm_cur_vm();
	uint64_t guest_tsc;

	if (vm == NULL)
		return E_NO_VM;

	guest_tsc = vmm_rdtsc(vm);

	if (copy_to_user(tsc, &guest_tsc, sizeof(uint64_t)) == NULL)
		return E_MEM_FAIL;

	return E_SUCC;
}

static int
sys_guest_tsc_freq(uint64_t *freq)
{
	struct vm *vm = vmm_cur_vm();
	uint64_t guest_freq;

	if (vm == NULL)
		return E_NO_VM;

	guest_freq = VM_TSC_FREQ;

	if (copy_to_user(freq, &guest_freq, sizeof(uint64_t)) == NULL)
		return E_MEM_FAIL;

	return E_SUCC;
}

static int
sys_guest_mem_size(uint64_t *memsize)
{
	struct vm *vm = vmm_cur_vm();
	uint64_t size;

	if (vm == NULL)
		return E_NO_VM;

	size = VM_PHY_MEMORY_SIZE;

	if (copy_to_user(memsize, &size, sizeof(uint64_t)) == NULL)
		return E_MEM_FAIL;

	return E_SUCC;
}

static int
sys_getchid(pid_t pid, int *chid)
{
	int child_chid;
	struct proc *child_proc = proc_pid2proc(pid);

	if (child_proc == NULL || child_proc->parent != proc_cur())
		child_chid = -1;
	else
		child_chid = channel_getid(child_proc->parent_ch);

	if (copy_to_user(chid, &child_chid, sizeof(int)) == NULL)
		return E_MEM_FAIL;

	return E_SUCC;
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

	case SYS_recv_nonblock:
		/*
		 * Receive a meesage from a channel.
		 * a[0]: the channel id
		 * a[1]: the address where the received message will be stored
		 * a[2]: the address where the size of the message will be
		 *       stored
		 */
		errno = sys_recv_nonblock
			((int) a[0], (void *) a[1], (size_t *) a[2]);
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

	case SYS_register_ioport:
		/*
		 * Register the caller to handle the I/O port readings/writings
		 * from the virtual machine.
		 * a[0]: the I/O port number
		 * a[1]: the data width,
		 *       0 for 1 bytes, 1 for 2 bytes, 2 for 4 bytes
		 * a[2]: 1 for writings, 0 for readings
		 */
		errno = sys_register_ioport
			((uint16_t) a[0], (data_sz_t) a[1], (int) a[2]);
		break;

	case SYS_unregister_ioport:
		/*
		 * Unregister the caller from handling the I/O port readings/
		 * writings from the virtual machine.
		 * a[0]: the I/O port number
		 * a[1]: the data width,
		 *       0 for 1 bytes, 1 for 2 bytes, 2 for 4 bytes
		 * a[2]: 1 for writings, 0 for readings
		 */
		errno = sys_unregister_ioport
			((uint16_t) a[0], (data_sz_t) a[1], (int) a[2]);
		break;

	case SYS_register_irq:
		/*
		 * Register the caller as the source of an interrupt.
		 * a[0]: the IRQ number
		 */
		errno = sys_register_irq((uint8_t) a[0]);
		break;

	case SYS_unregister_irq:
		/*
		 * Unregister the caller as the source of an interrupt.
		 * a[0]: the IRQ number
		 */
		errno = sys_unregister_irq((uint8_t) a[0]);
		break;

	case SYS_register_pic:
		/*
		 * Register the caller as the virtual PIC of the virtual machine
		 */
		errno = sys_register_pic();
		break;

	case SYS_unregister_pic:
		/*
		 * Unregister the caller as the virtual PIC of the virtual
		 * machine.
		 */
		errno = sys_unregister_pic();
		break;

	case SYS_register_mmio:
		/*
		 * a[0]: the start guest physical address of the memory region
		 * a[1]: the host linear address where the memory region is
		 *       mapped to
		 * a[2]: the size in bytes of the memory region
		 */
		errno = sys_register_mmio
			((uintptr_t) a[0], (uintptr_t) a[1], (size_t) a[2]);
		break;

	case SYS_unregister_mmio:
		/*
		 * a[0]: the start guest physical address of the memory region
		 * a[1]: the size in bytes of the memory region
		 */
		errno = sys_unregister_mmio((uintptr_t) a[0], (size_t) a[1]);
		break;

	case SYS_read_ioport:
		/*
		 * Read the physical I/O port.
		 * a[0]: the I/O port number
		 * a[1]: the data width
		 * a[2]: the buffer where the data will be stored
		 */
		errno = sys_read_ioport
			((uint16_t) a[0], (data_sz_t) a[1], (void *) a[2]);
		break;

	case SYS_write_ioport:
		/*
		 * Write to the host I/O port.
		 * a[0]: the I/O port number
		 * a[1]: the data width
		 * a[2]: where the data is
		 */
		errno = sys_write_ioport
			((uint16_t) a[0], (data_sz_t) a[1], (void *) a[2]);
		break;

	case SYS_raise_irq:
		/*
		 * Raise the IRQ line of the virtual PIC.
		 * a[0]: the IRQ number
		 */
		errno = sys_raise_irq((uint8_t) a[0]);
		break;

	case SYS_trigger_irq:
		/*
		 * Trigger the IRQ line of the virtual PIC.
		 * a[0]: the IRQ number
		 */
		errno = sys_trigger_irq((uint8_t) a[0]);
		break;

	case SYS_lower_irq:
		/*
		 * Lower the IRQ line of the virtual PIC.
		 * a[0]: the IRQ number
		 */
		errno = sys_lower_irq((uint8_t) a[0]);
		break;

	case SYS_notify_irq:
		/*
		 * Notify the virtual machine an interrupt comes.
		 */
		errno = sys_notify_irq();
		break;

	case SYS_read_guest_tsc:
		/*
		 * Read the guest TSC.
		 * a[0]: where the guest TSC is returned
		 */
		errno = sys_read_guest_tsc((uint64_t *) a[0]);
		break;

	case SYS_guest_tsc_freq:
		/*
		 * Get the frequency of the guest TSC.
		 * a[0]: where the frequency is returned
		 */
		errno = sys_guest_tsc_freq((uint64_t *) a[0]);
		break;

	case SYS_guest_mem_size:
		/*
		 * Get the physical memory size of the guest.
		 * a[0]: where the memory size is returned
		 */
		errno = sys_guest_mem_size((uint64_t *) a[0]);
		break;

	case SYS_getchid:
		/*
		 * Get ID of the channel to a child process.
		 * a[0]: PID of the child process
		 * a[1]: where the channel ID is returned
		 */
		errno = sys_getchid((pid_t) a[0], (int *) a[1]);
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

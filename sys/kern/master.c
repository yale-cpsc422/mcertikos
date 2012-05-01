#include <sys/context.h>
#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/mgmt.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/string.h>
#include <sys/syscall.h>
#include <sys/timer.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>
#include <sys/master.h>

#include <sys/elf.h>
#include <sys/virt/vmm.h>

#include <machine/pmap.h>

#include <dev/kbd.h>

extern uint8_t _binary___obj_user_mgmt_mgmt_start[];
extern uint8_t _binary___obj_user_mgmt_mgmt_size[];

#define MGMT_START	_binary___obj_user_mgmt_mgmt_start
#define MGMT_SIZE	_binary___obj_user_mgmt_mgmt_size

static pid_t mgmt_pid;

static uint8_t master_buf[PAGE_SIZE];
uint32_t  time=0;

//volatile kstack stacks[MAX_CPU];
volatile cpu_use cpus[MAX_CPU];


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
	/* KERN_DEBUG("copy_from_user(): %x <- %x, size=%x\n", dest, src, size); */

	if (dest == NULL || src == NULL || size == 0 ||
	    (uintptr_t) dest + size > VM_USERLO || (uintptr_t) src < VM_USERLO)
		return NULL;

	pmap_t *user_pmap = pcpu_cur()->proc->pmap;

	if (pmap_checkrange(user_pmap, (uintptr_t) dest, size) == FALSE) {
		KERN_DEBUG("%x ~ %x do not fit in the kernel address space.\n",
			   dest, dest+size);
		return NULL;
	}

	if (pmap_checkrange(user_pmap, (uintptr_t) src, size) == FALSE) {
		KERN_DEBUG("%x ~ %x do not fit in the user address space.\n",
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

	pmap_t *user_pmap = pcpu_cur()->proc->pmap;

	if (pmap_checkrange(user_pmap, (uintptr_t) src, size) == FALSE) {
		KERN_DEBUG("%x ~ %x do not fit in the kernel address space.\n",
			   src, src+size);
		return NULL;
	}
	


	if (pmap_checkrange(user_pmap, (uintptr_t) dest, size) == FALSE) {
		KERN_DEBUG("%x ~ %x do not fit in the user address space.\n",
			   dest, dest+size);
		return NULL;
	}

	memcpy(dest, src, size);

	return dest;
}

static uint32_t
master_default_exception_handler(context_t *ctx)
{
	KERN_ASSERT(ctx != NULL);

	KERN_DEBUG("Exception %x, rip = %x.\n", ctx->tf.trapno, ctx->tf.eip);

	return 0;
}

static uint32_t
master_gpf_handler(context_t *ctx)
{
	KERN_ASSERT(ctx != NULL);

	uint32_t errno = context_errno(ctx);

	KERN_PANIC("General Protection Fault on CPU%d, error code = 0x%x\n",
		   pcpu_cur_idx(), errno);

	/*
	 * TODO: handle GPF according to its causes; if it's caused by a user
	 *       process, kill that process; if it's caused by the kernel,
	 *       panic.
	 */

	return 0;
}

static uint32_t
master_pgf_handler(context_t *ctx)
{
	KERN_DEBUG("CR2:%x\n",rcr2());
	KERN_DEBUG("CR3:%x\n",rcr3());
	KERN_ASSERT(ctx != NULL);
	

	uint32_t errno = context_errno(ctx);
	uintptr_t fault_va = rcr2();

	if ((errno & PFE_U) == 0)
		KERN_PANIC("Page fault at 0x%x in kernel space.\n", fault_va);

	if (errno & PFE_PR) {
		KERN_DEBUG("Page fault at 0x%x is caused by a protection violation.\n",
			   fault_va);
		KERN_PANIC("Stop here.\n");
		return 1;
	}

	KERN_DEBUG("Page fault at 0x%x in userspace.\n", fault_va);

	pmap_t *user_pmap = pcpu_cur()->proc->pmap;

	if (!pmap_reserve(user_pmap, (uintptr_t) PGADDR(fault_va),
			  PTE_W | PTE_U | PTE_P)) {
		KERN_DEBUG("Cannot allocate physical memory for 0x%x\n",
			   fault_va);
		KERN_PANIC("Stop here.\n");
		return 1;
	}

	return 0;
}

static void
master_syscall_fail(context_t *ctx)
{
	KERN_DEBUG("System call 0x%x failed.\n", context_arg1(ctx));
}

static uint32_t
mgmt_start(context_t *ctx, mgmt_start_t *param)
{
	if (param->cpu >= pcpu_ncpu()) {
		KERN_INFO("MGMT_START: CPU%d is out of range.\n", param->cpu);
		return 1;
	}

	if (pcpu[param->cpu].proc != NULL) {
		KERN_INFO("MGMT_START: process %d is running on CPU%d.\n",
			  pcpu[param->cpu].proc->pid, param->cpu);
		return 1;
	}

	/* TODO: use IPI to start AP */
//	KERN_PANIC("MGMT_START: Not implemented yet.\n");
	cprintf("start: %d on CPU: %d\n", param->pid, param->cpu);
	cpus[param->cpu].start = param->pid;
	pcpu[param->cpu].stat = PCPU_RUNNING;
	
//	proc_lock(param->pid);

	return 0;
}

static uint32_t
mgmt_stop(context_t *ctx, mgmt_stop_t *param)
{
	if (param->cpu == 0) {
		KERN_INFO("MGMT_STOP: CPU0 is running management shell; cannot stop it.\n");
		return 1;
	}

	if (param->cpu >= pcpu_ncpu()) {
		KERN_INFO("MGMT_STOP: CPU%d is out of range.\n", param->cpu);
		return 1;
	}

	if (pcpu[param->cpu].stat == PCPU_STOP) {
		KERN_INFO("MGMT_STOP: CPU%d is already stopped.\n",
			  param->cpu);
		return 1;
	}

	/* TODO: use IPI ot stop AP */
	//KERN_PANIC("MGMT_STOP: Not implemented yet.\n");

	pcpu[param->cpu].proc->state=PROC_READY;	
	pcpu[param->cpu].proc=NULL;
        cpus[param->cpu].stop = TRUE;


	return 0;
}

static uint32_t
mgmt_allocpage(context_t *ctx, mgmt_allocpage_t *param)
{
	pid_t pid = param->pid;
	uintptr_t fault_va = param->va;

	if (!(pid < pcpu_ncpu())) {
		KERN_DEBUG("MGMT_ALLOCPAGE: Process %d is invalid.\n", pid);
		return 1;
	}

	proc_lock(pid);

	if (!pmap_reserve(proc_pmap(pid), fault_va, PTE_P | PTE_U | PTE_W)) {
		KERN_DEBUG("MGMT_ALLOCPAGE: Cannot allocate physical memory for 0x%x.\n",
			   fault_va);
		proc_unlock(pid);
		return 1;
	}

	proc_unlock(pid);

	return 0;
}

static uint32_t
master_mgmt_handler(context_t *ctx, mgmt_data_t *data)
{
	switch (data->cmd) {
	case SYSCALL_MGMT_START:
		mgmt_start(ctx, (mgmt_start_t *) (&data->params));

		break;

	case SYSCALL_MGMT_STOP:
		mgmt_stop(ctx, (mgmt_stop_t *) (&data->params));

		break;

	case SYSCALL_MGMT_ALLOCA_PAGE:
		mgmt_allocpage(ctx, (mgmt_allocpage_t *) (&data->params));

		break;

	default:
		KERN_INFO("Error: unrecognized managment command.\n");
		return 1;
	}

	return 0;
}

static uint32_t
master_syscall_handler(context_t *ctx)
{
	KERN_ASSERT(ctx != NULL);

	uint32_t cmd = context_arg1(ctx);
	uint32_t args[3] =
		{context_arg2(ctx), context_arg3(ctx), context_arg4(ctx)};

	switch (cmd) {
	case SYSCALL_PUTS:
		if (copy_from_user(master_buf,
				   (char *) args[0], PAGE_SIZE)== NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0, sizeof(char));
			return 1;
		}
	
		master_buf[PAGE_SIZE-1]=0;
		//cprintf("%c", *(char *) master_buf);
		cprintf("%s", master_buf);

		memset(master_buf, 0, sizeof(char));

		break;

	case SYSCALL_GETC:
		/*
		 * Get a character.
		 *
		 * args[0]: none;
		 *         the address where the character would be stored.
		 */
		*(char *) master_buf = getchar(); 
		if (copy_to_user((char *) args[0],
				 master_buf, sizeof(char)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(char));
			return 1;
		}

		memset(master_buf, 0x0, sizeof(char));

		break;

	case SYSCALL_NCPU:
		/*
		 * Get the amount of CPUs.
		 *
		 * args[0]: none;
		 *         the address where the amount would be stored.
		 */
		*(uint32_t *) master_buf = pcpu_ncpu();

		if (copy_to_user((uint32_t *) args[0],
				 master_buf, sizeof(uint32_t)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(uint32_t));
			return 1;
		}

		memset(master_buf, 0x0, sizeof(uint32_t));

		break;

	case SYSCALL_CPUSTAT:
		/*
		 * Get the status of a CPU.
		 *
		 * args[0]: the index of the CPU;
		 *         the address where the status of CPU would be stored.
		 */
		if (copy_from_user(master_buf, (uint32_t *) args[0],
				   sizeof(uint32_t)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(uint32_t));
			return 1;
		}

		uint32_t cpu_idx = *(uint32_t *) master_buf;

		if (!(cpu_idx < pcpu_ncpu())) {
			master_syscall_fail(ctx);
			return 2;
		}

		*(pcpu_stat_t *) master_buf = pcpu[cpu_idx].stat;

		if (copy_to_user((pcpu_stat_t *) args[0],
				 master_buf, sizeof(pcpu_stat_t)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(pcpu_stat_t));
			return 1;
		}

		memset(master_buf, 0x0, sizeof(pcpu_stat_t));

		break;

	case SYSCALL_LOAD:
		/*
		 * Load a binary as the code of a process.
		 *
		 * args[0]: the address of the binary;
		 *         none.
		 * args[1]: none;
		 *         the address where the pid of the process would
		 *         be stored.
		 */
		if (copy_from_user(master_buf, (uintptr_t *) args[0],
				   sizeof(uintptr_t)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(uintptr_t));
			return 1;
		}

		uintptr_t binary = *(uintptr_t *) master_buf;
		memset(master_buf, 0x0, sizeof(uintptr_t));

		KERN_DEBUG("SYSCALL_LOAD: binary:%x.\n", binary);
		//elfhdr *eh = (elfhdr *) binary;
		//KERN_DEBUG("code:%x.\n", eh->e_magic);
		pid_t pid = proc_new(binary);
		KERN_DEBUG("proc created\n");

		if (pid == 0) {
			KERN_DEBUG("SYSCALL_LOAD: Cannot create a new process.\n");
			master_syscall_fail(ctx);
			return 1;
		}

		*(pid_t *) master_buf = pid;

		if (copy_to_user((pid_t *) args[1],
				 master_buf, sizeof(pid_t)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(pid_t));
			return 1;
		}

		memset(master_buf, 0x0, sizeof(pid_t));

		break;

	case SYSCALL_STARTUPVM:
		/*
		 * Setup and start a VM.
		 */
		;
		struct vm *vm = vmm_init_vm();
		if (vm == NULL) {
			KERN_DEBUG("SYSCALL_SETUPVM: Cannot initialize a VM.\n");
			return 1;
		}
		vmm_run_vm(vm);

		break;

	case SYSCALL_MGMT:
		/*
		 * Management call.
		 *
		 * args[0]: the address where the management data is stored;
		 *         none;
		 */
		if (copy_from_user(master_buf, (mgmt_data_t *) args[0],
				   sizeof(mgmt_data_t)) == NULL) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(mgmt_data_t));
			return 1;
		}

		if (master_mgmt_handler(ctx, (mgmt_data_t *) master_buf)) {
			master_syscall_fail(ctx);
			memset(master_buf, 0x0, sizeof(mgmt_data_t));
			return 1;
		}

		memset(master_buf, 0x0, sizeof(mgmt_data_t));

		break;

	default:
		KERN_INFO("Error: unrecognized command.\n");
		return 1;
	}

	return 0;
}

static uint32_t
master_spurious_handler(context_t *ctx)
{
	KERN_DEBUG("Ignore spurious interrupt.\n");
	/* XXX: do not send EOI for spurious interrupts */
	return 0;
}

static uint32_t
master_timer_handler(context_t *ctx)
{
	//KERN_DEBUG("master_timer_handler\n"); 
	//cprintf("master_timer_handler\n"); 

	time++;

	/* timer_handle_timeout(); */

	struct vm *vm = vmm_cur_vm();
	bool from_guest =
		(vm != NULL && vm->exit_for_intr == TRUE) ? TRUE : FALSE;

	if (from_guest == TRUE)
		vmm_handle_intr(vm, IRQ_TIMER);

	intr_eoi();

	return 0;
}

/*
 * Keyboard interrupt handler for the master kernel. It checks whether the
 * interrupt is for a normal application or for a guest. Then it dispatch the
 * interrupt to the handler provided by the keyboard driver or to the handler
 * provided by the VMM module.
 */
static uint32_t
master_kbd_handler(context_t *ctx)
{
	KERN_DEBUG("master_kbd_handler\n");

	struct vm *vm = vmm_cur_vm();
	bool from_guest =
		(vm != NULL && vm->exit_for_intr == TRUE) ? TRUE : FALSE;

	if (from_guest != TRUE) { /* for a normal application */
		kbd_intr();
	} else /* for a guest */
		vmm_handle_intr(vm, IRQ_KBD);

	intr_eoi();

	return 0;
}

void
master_kernel(void)
{
	KERN_INFO("[MASTER] Master kernel starts ... \n");

	memset(master_buf, 0x0, PAGE_SIZE);

	KERN_INFO("[MASTER] Register exception handlers ... ");
	context_register_handler(T_GPFLT, master_gpf_handler);
	context_register_handler(T_PGFLT, master_pgf_handler);
	context_register_handler(T_SYSCALL, master_syscall_handler);
	/* use default handler to handle other exceptions */
	context_register_handler(T_DIVIDE, master_default_exception_handler);
	context_register_handler(T_DEBUG, master_default_exception_handler);
	context_register_handler(T_NMI, master_default_exception_handler);
	context_register_handler(T_BRKPT, master_default_exception_handler);
	context_register_handler(T_OFLOW, master_default_exception_handler);
	context_register_handler(T_BOUND, master_default_exception_handler);
	context_register_handler(T_ILLOP, master_default_exception_handler);
	context_register_handler(T_DEVICE, master_default_exception_handler);
	context_register_handler(T_DBLFLT, master_default_exception_handler);
	context_register_handler(T_COPROC, master_default_exception_handler);
	context_register_handler(T_TSS, master_default_exception_handler);
	context_register_handler(T_SEGNP, master_default_exception_handler);
	context_register_handler(T_STACK, master_default_exception_handler);
	context_register_handler(T_RES, master_default_exception_handler);
	context_register_handler(T_FPERR, master_default_exception_handler);
	context_register_handler(T_ALIGN, master_default_exception_handler);
	context_register_handler(T_MCHK, master_default_exception_handler);
	context_register_handler(T_SIMD, master_default_exception_handler);
	context_register_handler(T_SECEV, master_default_exception_handler);
	KERN_INFO("done.\n");

	KERN_INFO("[MASTER] Register interrupt handlers ... ");
	context_register_handler(T_IRQ0+IRQ_SPURIOUS, master_spurious_handler);
	context_register_handler(T_IRQ0+IRQ_TIMER, master_timer_handler);
	context_register_handler(T_IRQ0+IRQ_KBD, master_kbd_handler);
	KERN_INFO("done.\n");

	KERN_INFO("[MASTER] Enable TIMER interrupt.\n");
	intr_enable(IRQ_TIMER, 0);

	KERN_INFO("[MASTER] Enable KBD interrupt.\n");
	kbd_intenable();

	KERN_INFO("[MASTER] Enable IDE interrupt.\n");
	intr_enable(IRQ_IDE, 0);

	/* intr_global_enable(); */

	mgmt_pid = proc_new((uintptr_t) MGMT_START);
	if (mgmt_pid == 0)
		KERN_PANIC("[MASTER] Failed to start mgmt.\n ");
	proc_lock(mgmt_pid);
	proc_start(mgmt_pid);

	KERN_PANIC("Master should not be here.\n");
}

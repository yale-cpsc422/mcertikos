#include <sys/channel.h>
#include <sys/debug.h>
#include <sys/ipc.h>
#include <sys/mmu.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/string.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include <dev/ahci.h>
#include <dev/cons.h>
#include <dev/disk.h>
#include <dev/tsc.h>

#include <machine/pmap.h>

#ifdef DEBUG_SYSCALL

static char *syscall_name[MAX_SYSCALL_NR] =
	{
		[SYS_puts]		= "sys_puts",
		[SYS_getc]		= "sys_getc",
		[SYS_create_proc]	= "sys_create_proc",
		[SYS_run_proc]		= "sys_run_proc",
		[SYS_yield]		= "sys_yield",
		[SYS_getpid]		= "sys_getpid",
		[SYS_getppid]		= "sys_getppid",
		[SYS_getchid]		= "sys_getchid",
		[SYS_channel]		= "sys_channel",
		[SYS_send]		= "sys_send",
		[SYS_recv]		= "sys_recv",
		[SYS_disk_op]		= "sys_disk_op",
		[SYS_disk_cap]		= "sys_disk_cap",
		[SYS_sysinfo_lookup]	= "sys_sysinfo_lookup",
		[SYS_hvm_create_vm]	= "sys_hvm_create_vm",
		[SYS_hvm_run_vm]	= "sys_hvm_run_vm",
		[SYS_hvm_set_mmap]	= "sys_hvm_set_mmap",
		[SYS_hvm_set_reg]	= "sys_hvm_set_reg",
		[SYS_hvm_get_reg]	= "sys_hvm_get_reg",
		[SYS_hvm_set_desc]	= "sys_hvm_set_desc",
		[SYS_hvm_get_desc]	= "sys_hvm_get_desc",
		[SYS_hvm_get_next_eip]	= "sys_hvm_get_next_eip",
		[SYS_hvm_inject_event]	= "sys_hvm_inject_event",
		[SYS_hvm_pending_event]	= "sys_hvm_pending_event",
		[SYS_hvm_intr_shadow]	= "sys_hvm_intr_shadow",
		[SYS_hvm_intercept_ioport] = "sys_hvm_intercept_ioport",
		[SYS_hvm_intercept_msr]	= "sys_hvm_intercept_msr",
		[SYS_hvm_intercept_intr_window] = "sys_hvm_intercept_intr_window",
		[SYS_hvm_mmap_bios]	= "sys_hvm_mmap_bios",
		[SYS_read_ioport]	= "sys_read_ioport",
		[SYS_write_ioport]	= "sys_write_ioport",
	};

static char *errno_name[MAX_ERROR_NR] =
	{
		[E_SUCC]		= "E_SUCC",
		[E_MEM]			= "E_MEM",
		[E_INVAL_CALLNR]	= "E_INVAL_CALLNR",
		[E_INVAL_CPU]		= "E_INVAL_CPU",
		[E_INVAL_ADDR]		= "E_INVAL_ADDR",
		[E_INVAL_PID]		= "E_INVAL_PID",
		[E_INVAL_CHID]		= "E_INVAL_CHID",
		[E_INVAL_VMID]		= "E_INVAL_VMID",
		[E_INVAL_CACHE_TYPE]	= "E_INVAL_CACHE_TYPE",
		[E_INVAL_REG]		= "E_INVAL_REG",
		[E_INVAL_SEG]		= "E_INVAL_SEG",
		[E_INVAL_EVENT]		= "E_INVAL_EVENT",
		[E_INVAL_SYSINFO_NAME]	= "E_INVAL_SYSINFO_NAME",
		[E_INVAL_PORT]		= "E_INVAL_PORT",
		[E_SEND]		= "E_SEND",
		[E_RECV]		= "E_RECV",
		[E_CHANNEL]		= "E_CHANNEL",
		[E_PERM]		= "E_PERM",
		[E_DISK_OP]		= "E_DISK_OP",
		[E_DISK_NODRV]		= "E_DISK_NODRV",
		[E_HVM_VMRUN]		= "E_HMV_VMRUN",
		[E_HVM_MMAP]		= "E_HVM_MMAP",
		[E_HVM_REG]		= "E_HVM_REG",
		[E_HVM_SEG]		= "E_HVM_SEG",
		[E_HVM_NEIP]		= "E_HVM_NEIP",
		[E_HVM_INJECT]		= "E_HVM_INJECT",
		[E_HVM_IOPORT]		= "E_HVM_IOPORT",
		[E_HVM_MSR]		= "E_HVM_MSR",
		[E_HVM_INTRWIN]		= "E_HVM_INTRWIN",
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
		KERN_DEBUG("%s(nr %d, CPU%d, PID%d) "fmt,		\
			   (0 <= nr && nr < MAX_SYSCALL_NR) ?		\
			   syscall_name[nr] : "unknown",		\
			   nr,						\
			   pcpu_cpu_idx(pcpu_cur()),			\
			   proc_cur()->pid,				\
			   ##__VA_ARGS__);				\
	} while (0)

#define ERRNO_STR(errno)						\
	((errno) >= MAX_ERROR_NR ? "unknown" : errno_name[(errno)])

#else

#define SYSCALL_DEBUG(fmt...)			\
	do {					\
	} while (0)

#define NR_DEBUG(fmt...)			\
	do {					\
	} while (0)

#endif

extern uint8_t _binary___obj_user_idle_idle_start[],
	_binary___obj_user_vmm_vmm_start[];

static uintptr_t elf_addr[2] = {
	(uintptr_t) _binary___obj_user_idle_idle_start,
	(uintptr_t) _binary___obj_user_vmm_vmm_start
};

/*
 * Transfer data between the kernel linear address space and the user linear
 * address space.
 *
 * @param ka   the start linear address of the kernel linear address space
 * @param (pmap, la)
 *             the start linear address of the user linear address space
 * @param size how many bytes is transfered
 *
 * @return the number of bytes that are actually transfered
 */
static size_t
rw_user(uintptr_t ka, pmap_t *pmap, uintptr_t la, size_t size, int write)
{
	KERN_ASSERT(ka + size <= VM_USERLO);
	KERN_ASSERT(pmap != NULL);

	if (la < VM_USERLO || VM_USERLO - size > la)
		return 0;

	if (write)
		return pmap_copy(pmap, la, pmap_kern_map(), ka, size);
	else
		return pmap_copy(pmap_kern_map(), ka, pmap, la, size);
}

#define copy_from_user(ka, pmap, la, size)	\
	rw_user((ka), (pmap), (la), (size), 0)

#define copy_to_user(pmap, la, ka, size)	\
	rw_user((ka), (pmap), (la), (size), 1)

static int
sys_puts(uintptr_t str_la)
{
	struct proc *p = proc_cur();

	if (copy_from_user((uintptr_t) p->sys_buf,
			   p->pmap, str_la, PAGESIZE) == 0)
		return E_MEM;

	((char *) (p->sys_buf))[PAGESIZE - 1] = '\0';
	KERN_INFO("%s", (char *) p->sys_buf);

	return E_SUCC;
}

static int
sys_getc(uintptr_t buf_la)
{
	struct proc *p = proc_cur();
	int c;

	if (cons_getchar(0, &c))
		c = -1;

	if (copy_to_user(p->pmap, buf_la, (uintptr_t) &c, sizeof(int)) == 0)
		return E_MEM;
	else
		return E_SUCC;
}

static int
sys_create_proc(uintptr_t pid_la, chid_t chid)
{
	struct proc *new_p, *cur_p;
	struct channel *ch;

	if (!(VM_USERLO <= pid_la && pid_la + sizeof(pid_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (unlikely((cur_p = proc_cur()) == NULL))
		return E_INVAL_PID;

	if (chid != -1) {
		if ((ch = channel_getch(chid)) == NULL)
			return E_INVAL_CHID;
	} else {
		ch = NULL;
	}

	if ((new_p = proc_new(cur_p, ch)) == NULL)
		return E_INVAL_PID;

	if (copy_to_user(cur_p->pmap, pid_la,
			 (uintptr_t) &new_p->pid, sizeof(pid_t)) == 0)
		return E_MEM;

	return E_SUCC;
}

static int
sys_run_proc(pid_t pid, uint32_t cpu_idx, uintptr_t binary_la)
{
	struct pcpu *c;
	struct proc *p;

	if (cpu_idx >= pcpu_ncpu())
		return E_INVAL_CPU;
	c = pcpu_get_cpu(cpu_idx);

	if ((p = proc_pid2proc(pid)) == NULL || p->state != PROC_INITED)
		return E_INVAL_PID;

	proc_exec(p, c, binary_la);
	return E_SUCC;
}

static int
sys_yield(void)
{
	proc_yield();
	return E_SUCC;
}

static int
sys_getpid(uintptr_t pid_la)
{
	if (pid_la < VM_USERLO || pid_la + sizeof(pid_t) > VM_USERHI)
		return E_INVAL_ADDR;

	struct proc *p = proc_cur();
	KERN_ASSERT(p != NULL);

	if (copy_to_user(p->pmap, pid_la,
			 (uintptr_t) &p->pid, sizeof(pid_t)) == 0)
		return E_MEM;
	else
		return E_SUCC;
}

static int
sys_getppid(uintptr_t ppid_la)
{
	if (ppid_la < VM_USERLO || ppid_la + sizeof(pid_t) > VM_USERHI)
		return E_INVAL_ADDR;

	struct proc *p = proc_cur();

	if (copy_to_user(p->pmap, ppid_la,
			 (uintptr_t) &p->parent->pid, sizeof(pid_t)) == 0)
		return E_MEM;
	else
		return E_SUCC;
}

static int
sys_getchid(uintptr_t chid_la)
{
	struct proc *cur_p;
	struct channel *ch;
	chid_t chid;

	if (!(VM_USERLO <= chid_la && chid_la + sizeof(int) <= VM_USERHI))
		return E_INVAL_ADDR;

	if ((cur_p = proc_cur()) == NULL)
		return E_INVAL_PID;

	if ((ch = cur_p->parent_ch) == NULL)
		return E_INVAL_CHID;

	chid = channel_getid(ch);

	if (copy_to_user(cur_p->pmap, chid_la,
			 (uintptr_t) &chid, sizeof(int)) == 0)
		return E_MEM;

	return E_SUCC;
}

static int
sys_send(int chid, uintptr_t msg_la, size_t size)
{
	struct channel *ch;

	if (!(VM_USERLO <= msg_la && msg_la + size <= VM_USERHI))
		return E_INVAL_ADDR;

	if ((ch = channel_getch(chid)) == NULL)
		return E_INVAL_CHID;

	if (ipc_send(ch, (uintptr_t) msg_la, size, FALSE, TRUE))
		return E_SEND;

	return E_SUCC;
}

static int
sys_recv(int chid, uintptr_t msg_la, size_t size)
{
	struct channel *ch;

	if (!(VM_USERLO <= msg_la && msg_la + size <= VM_USERHI))
		return E_INVAL_ADDR;

	if ((ch = channel_getch(chid)) == NULL)
		return E_INVAL_CHID;

	if (ipc_recv(ch, (uintptr_t) msg_la, size, FALSE, TRUE))
		return E_RECV;

	return E_SUCC;
}

static int
__sys_disk_read(struct disk_dev *drv,
		uint64_t lba, uint64_t nsectors, uintptr_t buf)
{
	uint64_t cur_lba, remaining;
	uintptr_t cur_la;

	struct proc *p = proc_cur();

	cur_lba = lba;
	remaining = nsectors;
	cur_la = buf;

	while (remaining > 0) {
		uint64_t n = MIN(remaining, PAGESIZE / ATA_SECTOR_SIZE);
		if (disk_xfer(drv, cur_lba, (uintptr_t) p->sys_buf, n, FALSE))
			return E_DISK_OP;
		if (copy_to_user(p->pmap, cur_la, (uintptr_t) p->sys_buf,
				 n * ATA_SECTOR_SIZE) == 0)
			return E_MEM;
		cur_lba += n;
		remaining -= n;
		cur_la += n * ATA_SECTOR_SIZE;
	}

	return E_SUCC;
}

static int
__sys_disk_write(struct disk_dev *drv,
		 uint64_t lba, uint64_t nsectors, uintptr_t buf)
{
	uint64_t cur_lba, remaining;
	uintptr_t cur_la;

	struct proc *p = proc_cur();

	cur_lba = lba;
	remaining = nsectors;
	cur_la = buf;

	while (remaining > 0) {
		uint64_t n = MIN(remaining, PAGESIZE / ATA_SECTOR_SIZE);
		if (copy_from_user((uintptr_t) p->sys_buf, p->pmap, cur_la,
				   n * ATA_SECTOR_SIZE) == 0)
			return E_MEM;
		if (disk_xfer(drv, cur_lba, (uintptr_t) p->sys_buf, n, TRUE))
			return E_DISK_OP;
		cur_lba += n;
		remaining -= n;
		cur_la += n * ATA_SECTOR_SIZE;
	}

	return E_SUCC;
}

static int
sys_disk_op(uintptr_t dop_la)
{
	struct proc *p;
	pmap_t *pmap;

	struct user_disk_op dop;
	struct disk_dev *drv;
	int rc = 0;

	if (dop_la < VM_USERLO || dop_la + sizeof(dop) > VM_USERHI)
		return E_INVAL_ADDR;

	p = proc_cur();
	pmap = p->pmap;

	if (copy_from_user((uintptr_t) &dop, pmap, dop_la, sizeof(dop)) == 0)
		return E_MEM;

	if ((drv = disk_get_dev(dop.dev_nr)) == NULL)
		return E_DISK_NODRV;

	if (!(VM_USERLO <= dop.buf &&
	      dop.buf + dop.n * ATA_SECTOR_SIZE <= VM_USERHI))
		return E_INVAL_ADDR;

	switch (dop.type) {
	case DISK_READ:
		rc = __sys_disk_read(drv, dop.lba, dop.n, dop.buf);
		break;
	case DISK_WRITE:
		rc = __sys_disk_write(drv, dop.lba, dop.n, dop.buf);
		break;
	default:
		rc = 1;
	}

	if (rc)
		return E_DISK_OP;

	return E_SUCC;
}

static int
sys_disk_cap(uint32_t dev_nr, uintptr_t lo_la, uintptr_t hi_la)
{
	uint64_t cap;
	uint32_t cap_lo, cap_hi;
	struct disk_dev *drv;

	if (!(VM_USERLO <= lo_la && lo_la + sizeof(uint32_t) <= VM_USERHI))
		return E_INVAL_ADDR;
	if (!(VM_USERLO <= hi_la && hi_la + sizeof(uint32_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	if ((drv = disk_get_dev(dev_nr)) == NULL)
		return E_DISK_NODRV;

	cap = disk_capacity(drv);
	cap_lo = cap & 0xffffffff;
	cap_hi = (cap >> 32) & 0xffffffff;

	if ((copy_to_user(proc_cur()->pmap, lo_la,
			  (uintptr_t) &cap_lo, sizeof(uint32_t))) == 0)
		return E_MEM;
	if ((copy_to_user(proc_cur()->pmap, hi_la,
			  (uintptr_t) &cap_hi, sizeof(uint32_t))) == 0)
		return E_MEM;

	return E_SUCC;
}

static int
sys_channel(uintptr_t chid_la, size_t msg_size)
{
	struct channel *ch;
	chid_t chid;

	if (!(VM_USERLO <= chid_la && chid_la + sizeof(chid_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	if ((ch = channel_alloc(msg_size)) == NULL)
		return E_CHANNEL;

	chid = channel_getid(ch);

	if (copy_to_user(proc_cur()->pmap, chid_la,
			 (uintptr_t) &chid, sizeof(chid_t)) == 0)
		return E_MEM;

	return E_SUCC;
}

static int
sys_sysinfo_lookup(sysinfo_name_t name, uintptr_t info_la)
{
	sysinfo_info_t info;

	if (!(VM_USERLO <= info_la &&
	      info_la + sizeof(sysinfo_info_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	switch (name) {
	case SYSINFO_CPU_VENDOR:
		info.info32 = pcpu_cur()->arch_info.cpu_vendor;
		break;
	case SYSINFO_CPU_FREQ:
		info.info64 = tsc_per_ms * 1000;
		break;
	default:
		return E_INVAL_SYSINFO_NAME;
	}

	if (copy_to_user(proc_cur()->pmap, info_la, (uintptr_t) &info,
			 sizeof(info)) != sizeof(info))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_create_vm(uintptr_t vmid_la)
{
	if (!(VM_USERLO <= vmid_la && vmid_la + sizeof(int) <= VM_USERHI))
		return E_INVAL_ADDR;

	struct vm *vm = hvm_create_vm();

	if (vm == NULL)
		return E_INVAL_VMID;

	if (copy_to_user(proc_cur()->pmap, vmid_la,
			 (uintptr_t) &vm->vmid, sizeof(int)) != sizeof(int))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_run_vm(int vmid, uintptr_t exit_reason_la, uintptr_t exit_info_la)
{
	struct vm *vm = hvm_get_vm(vmid);

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= exit_reason_la &&
	      exit_reason_la + sizeof(exit_reason_t) <= VM_USERHI) ||
	    !(VM_USERLO <= exit_info_la &&
	      exit_info_la + sizeof(exit_info_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (hvm_run_vm(vm))
		return E_HVM_VMRUN;

	if (copy_to_user(proc_cur()->pmap,
			 exit_reason_la, (uintptr_t) &vm->exit_reason,
			 sizeof(exit_reason_t)) != sizeof(exit_reason_t))
		return E_MEM;

	if (copy_to_user(proc_cur()->pmap,
			 exit_info_la, (uintptr_t) &vm->exit_info,
			 sizeof(exit_info_t)) != sizeof(exit_info_t))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_set_mmap(int vmid, uintptr_t hvm_mmap_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	struct user_hvm_mmap hvm_mmap;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= hvm_mmap_la &&
	      hvm_mmap_la + sizeof(struct user_hvm_mmap) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (copy_from_user((uintptr_t) &hvm_mmap, proc_cur()->pmap, hvm_mmap_la,
			   sizeof(struct user_hvm_mmap))
	    != sizeof(struct user_hvm_mmap))
		return E_MEM;

	if (hvm_mmap.gpa % PAGESIZE != 0 || hvm_mmap.hva % PAGESIZE != 0 ||
	    !(VM_USERLO <= hvm_mmap.hva && hvm_mmap.hva + PAGESIZE <= VM_USERHI))
		return E_INVAL_ADDR;

	if (!(PAT_UNCACHEABLE <= hvm_mmap.type &&
	      hvm_mmap.type <= PAT_UNCACHED))
		return E_INVAL_CACHE_TYPE;

	if (hvm_set_mmap(vm, hvm_mmap.gpa,
			 pmap_la2pa(proc_cur()->pmap, hvm_mmap.hva),
			 hvm_mmap.type))
		return E_HVM_MMAP;
	else
		return E_SUCC;
}

static int
sys_hvm_set_reg(int vmid, guest_reg_t reg, uint32_t val)
{
	struct vm *vm = hvm_get_vm(vmid);

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(GUEST_EAX <= reg && reg < GUEST_MAX_REG))
		return E_INVAL_REG;

	if (hvm_set_reg(vm, reg, val))
		return E_HVM_REG;
	else
		return E_SUCC;
}

static int
sys_hvm_get_reg(int vmid, guest_reg_t reg, uintptr_t val_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	uint32_t val;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(GUEST_EAX <= reg && reg < GUEST_MAX_REG))
		return E_INVAL_REG;

	if (!(VM_USERLO <= val_la && val_la + sizeof(uint32_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (hvm_get_reg(vm, reg, &val))
		return E_HVM_REG;

	if (copy_to_user(proc_cur()->pmap, val_la, (uintptr_t) &val,
			 sizeof(uint32_t)) != sizeof(uint32_t))
		return E_INVAL_ADDR;

	return E_SUCC;
}

static int
sys_hvm_set_desc(int vmid, guest_seg_t seg, uintptr_t desc_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	struct guest_seg_desc desc;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(GUEST_CS <= seg && seg < GUEST_MAX_SEG_DESC))
		return E_INVAL_SEG;

	if (!(VM_USERLO <= desc_la && desc_la + sizeof(desc) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (copy_from_user((uintptr_t) &desc, proc_cur()->pmap, desc_la,
			   sizeof(desc)) != sizeof(desc))
		return E_MEM;

	if (hvm_set_desc(vm, seg, &desc))
		return E_HVM_SEG;
	else
		return E_SUCC;
}

static int
sys_hvm_get_desc(int vmid, guest_seg_t seg, uintptr_t desc_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	struct guest_seg_desc desc;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(GUEST_CS <= seg && seg < GUEST_MAX_SEG_DESC))
		return E_INVAL_SEG;

	if (!(VM_USERLO <= desc_la && desc_la + sizeof(desc) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (hvm_get_desc(vm, seg, &desc))
		return E_HVM_SEG;

	if (copy_to_user(proc_cur()->pmap, desc_la, (uintptr_t) &desc,
			 sizeof(desc)) != sizeof(desc))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_get_next_eip(int vmid, guest_instr_t instr, uintptr_t neip_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	uint32_t neip;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= neip_la && neip_la + sizeof(uintptr_t) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (hvm_get_next_eip(vm, instr, &neip))
		return E_HVM_NEIP;

	if (copy_to_user(proc_cur()->pmap, neip_la, (uintptr_t) &neip,
			 sizeof(uint32_t)) != sizeof(uint32_t))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_inject_event(int vmid, uintptr_t event_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	struct user_hvm_event event;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= event_la && event_la + sizeof(event) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (copy_from_user((uintptr_t) &event, proc_cur()->pmap, event_la,
			   sizeof(event)) != sizeof(event))
		return E_MEM;

	if (!(EVENT_EXTINT <= event.type && event.type <= EVENT_SWINT))
		return E_INVAL_EVENT;

	if (hvm_inject_event(vm,
			     event.type, event.vector, event.errcode, event.ev))
		return E_HVM_INJECT;
	else
		return E_SUCC;
}

static int
sys_hvm_pending_event(int vmid, uintptr_t result_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	int result;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= result_la && result_la + sizeof(result) <= VM_USERHI))
		return E_INVAL_ADDR;

	result = hvm_pending_event(vm);

	if (copy_to_user(proc_cur()->pmap, result_la, (uintptr_t) &result,
			 sizeof(result)) != sizeof(result))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_intr_shadow(int vmid, uintptr_t result_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	int result;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= result_la && result_la + sizeof(result) <= VM_USERHI))
		return E_INVAL_ADDR;

	result = hvm_intr_shadow(vm);

	if (copy_to_user(proc_cur()->pmap, result_la, (uintptr_t) &result,
			 sizeof(result)) != sizeof(result))
		return E_MEM;

	return E_SUCC;
}

static int
sys_hvm_intercept_ioport(int vmid, uint16_t ioport, bool enable)
{
	struct vm *vm = hvm_get_vm(vmid);

	if (vm == NULL)
		return E_INVAL_VMID;

	if (hvm_intercept_ioport(vm, ioport, enable))
		return E_HVM_IOPORT;
	else
		return E_SUCC;
}

static int
sys_hvm_intercept_msr(int vmid, uint32_t msr, bool enable)
{
	struct vm *vm = hvm_get_vm(vmid);

	if (vm == NULL)
		return E_INVAL_VMID;

	if (hvm_intercept_msr(vm, msr, enable))
		return E_HVM_MSR;
	else
		return E_SUCC;
}

static int
sys_hvm_intercept_intr_window(int vmid, bool enable)
{
	struct vm *vm = hvm_get_vm(vmid);

	if (vm == NULL)
		return E_INVAL_VMID;

	if (hvm_intercept_intr_window(vm, enable))
		return E_HVM_INTRWIN;
	else
		return E_SUCC;
}

static int
sys_hvm_mmap_bios(int vmid, uintptr_t bios_la)
{
	struct vm *vm = hvm_get_vm(vmid);
	struct proc *p = proc_cur();
	uintptr_t addr;

	if (vm == NULL)
		return E_INVAL_VMID;

	if (!(VM_USERLO <= bios_la && bios_la + 0x100000 <= VM_USERHI))
		return E_INVAL_ADDR;

	pmap_remove(p->pmap, bios_la, 0xc0000 - 0xa0000);

	for (addr = 0xa0000; addr < 0xc0000; addr += PAGESIZE) {
		if (pmap_insert(p->pmap, mem_phys2pi(addr), bios_la+addr-0xa0000,
				PTE_U | PTE_W) == NULL) {
			KERN_DEBUG("Cannot map VA 0x%08x to PA 0x%08x.\n",
				   bios_la+addr-0xa0000, addr);
			return E_MEM;
		}
	}

	return 0;
}

static int
sys_read_ioport(uint16_t port, data_sz_t width, uintptr_t data_la)
{
	uint32_t data;

	if (!(VM_USERLO <= data_la && data_la + (1 << width) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (width == SZ8)
		data = inb(port);
	else if (width == SZ16)
		data = inw(port);
	else if (width == SZ32)
		data = inl(port);
	else
		return E_INVAL_PORT;

	if (copy_to_user(proc_cur()->pmap, data_la, (uintptr_t) &data,
			 (1 << width)) != (1 << width))
		return E_MEM;

	return E_SUCC;
}

static int
sys_write_ioport(uint16_t port, data_sz_t width, uint32_t data)
{
	if (width == SZ8)
		outb(port, (uint8_t) data);
	else if (width == SZ16)
		outw(port, (uint16_t) data);
	else if (width == SZ32)
		outl(port, data);
	else
		return E_INVAL_PORT;
	return E_SUCC;
}

/*
 * Syetem calls in CertiKOS follow the convention below.
 *
 * - The system call number is passed through the first argument of the user
 *   context, e.g. %eax on i386 arch.
 *
 * - Each system call can have at most three parameters, which are passed
 *   through the second argument to the fourth argument of the user context
 *   respectively, e.g. %ebx, %ecx, and %edx on i386 arch.
 *
 * - The error code of the syscall is returned through the first argument of
 *   the user context, i.e. %eax on i386 arch. Error code 0 means no errors.
 *
 * - Changes to other arguments maybe not returned to the caller.
 */
int
syscall_handler(uint8_t trapno, struct context *ctx)
{
	KERN_ASSERT(ctx != NULL);

	uint32_t nr = ctx_arg1(ctx);
	uint32_t a[3] =
		{ctx_arg2(ctx), ctx_arg3(ctx), ctx_arg4(ctx)};
	uint32_t errno = E_SUCC;

	NR_DEBUG(nr, "from 0x%08x.\n", ctx->tf.eip);
	/* ctx_dump(ctx); */

	memzero(proc_cur()->sys_buf, PAGESIZE);

	switch (nr) {
	case SYS_puts:
		/*
		 * Output a string to the screen.
		 * a[0]: the linear address where the string is
		 */
		errno = sys_puts((uintptr_t) a[0]);
		break;
	case SYS_getc:
		/*
		 * Input a character from the keyboard.
		 * a[0]: the linear address where the input will be returned to
		 */
		errno = sys_getc((uintptr_t) a[0]);
		break;
	case SYS_create_proc:
		/*
		 * Create a new process.
		 * a[0]: the linear address where the process ID is returned to
		 * a[1]: the id of the channel to the parent process
		 */
		errno = sys_create_proc((uintptr_t) a[0], (chid_t) a[1]);
		break;
	case SYS_run_proc:
		/*
		 * Run a process.
		 * a[0]: the process ID
		 * a[1]: the CPU index on which the process is going to run
		 * a[2]: the linear address where the binary code is
		 */
		errno = sys_run_proc((uint32_t) a[0], (pid_t) a[1],
				     elf_addr[a[2]]);
		break;
	case SYS_yield:
		/*
		 * Called by a process to abandon its CPU slice.
		 */
		errno = sys_yield();
		break;
	case SYS_getpid:
		/*
		 * Get the process id of the caller.
		 * a[0]: the linear address where the process id will be
		 *       returned to
		 */
		errno = sys_getpid((uintptr_t) a[0]);
		break;
	case SYS_getppid:
		/*
		 * Get the process id of the parent process of the caller.
		 * a[0]: the linear address where the process id will be
		 *       returned to
		 */
		errno = sys_getppid((uintptr_t) a[0]);
		break;
	case SYS_getchid:
		/*
		 * Get the channel ID to the parent process.
		 * a[0]: the linear address where the channel ID is returned to
		 */
		errno = sys_getchid((uintptr_t) a[0]);
		break;
	case SYS_send:
		/*
		 * Send a message to a channel.
		 * a[0]: the identity of the channel
		 * a[1]: the linear address where the message is stored
		 * a[2]: the size of the message
		 */
		errno = sys_send((int) a[0], (uintptr_t) a[1], (size_t) a[2]);
		break;
	case SYS_recv:
		/*
		 * Receive a message from a channel.
		 * a[0]: the identity of the channel
		 * a[1]: the linear address where the message will be stored
		 * a[2]: the size of the message
		 */
		errno = sys_recv((int) a[0], (uintptr_t) a[1], (size_t) a[2]);
		break;
	case SYS_disk_op:
		/*
		 * Disk operation. The operation information must be provided in
		 * an object of type struct user_disk_op by the caller.
		 * a[0]: the linear address where the user_disk_op is
		 */
		errno = sys_disk_op((uintptr_t) a[0]);
#ifdef DEBUG_DISK
		if (errno)
			KERN_DEBUG("sys_disk_op() failed. (errno %d)\n", errno);
#endif
		break;
	case SYS_disk_cap:
		/*
		 * Get the capability of the disk for the virtual machine.
		 * a[0]: the disk device number
		 * a[1]: the linear address where the lowest 32 bits of the
		 *       capability are returned to
		 * a[2]: the linear address where the highest 32 bits of the
		 *       capability are returned to
		 */
		errno = sys_disk_cap((uint32_t) a[0],
				     (uintptr_t) a[1], (uintptr_t) a[2]);
#ifdef DEBUG_DISK
		if (errno)
			KERN_DEBUG("sys_disk_cap() failed. (errno %d)\n", errno);
#endif
		break;
	case SYS_sysinfo_lookup:
		/*
		 * Lookup the system information by name.
		 * a[0]: the name of the system information
		 * a[1]: the linear address where the information is returned to
		 */
		errno = sys_sysinfo_lookup((int) a[0], (uintptr_t) a[1]);
		break;
	case SYS_hvm_create_vm:
		/*
		 * Create a new virtual machine descriptor.
		 * a[0]: the linear address where the descriptor is returned to
		 */
		errno = sys_hvm_create_vm((uintptr_t) a[0]);
		break;
	case SYS_hvm_run_vm:
		/*
		 * Run a virtual machine and returns when a VMEXIT or an error
		 * happens.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the linear address where the exit reason is returned to
		 * a[2]: the linear address where the exit info is returned to
		 */
		errno = sys_hvm_run_vm((int) a[0],
				       (uintptr_t) a[1], (uintptr_t) a[2]);
		break;
	case SYS_hvm_set_mmap:
		/*
		 * Map a guest physical page to a host virtual page.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the linear address of the mapping information
		 */
		errno = sys_hvm_set_mmap((int) a[0], (uintptr_t) a[1]);
		if (errno)
			KERN_DEBUG("sys_hvm_set_mmap() failed, errno %d.\n",
				   errno);
		break;
	case SYS_hvm_set_reg:
		/*
		 * Set the general-purpose register of a virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the guest register
		 * a[2]: the value of the register
		 */
		errno = sys_hvm_set_reg((int) a[0],
					(guest_reg_t) a[1], (uint32_t) a[2]);
		break;
	case SYS_hvm_get_reg:
		/*
		 * Get the value of the general-purpose register of a virtual
		 * machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the guest register
		 * a[2]: the linear address where the value is returned to
		 */
		errno = sys_hvm_get_reg((int) a[0],
					(guest_reg_t) a[1], (uintptr_t) a[2]);
		break;
	case SYS_hvm_set_desc:
		/*
		 * Set the segment descriptor of a virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the guest segment
		 * a[2]: the linear address of the descriptor information
		 */
		errno = sys_hvm_set_desc((int) a[0],
					 (guest_seg_t) a[1], (uintptr_t) a[2]);
		break;
	case SYS_hvm_get_desc:
		/*
		 * Get the content of the segment descriptor of a virtual
		 * machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the guest segment
		 * a[2]: the linear address where the descriptor is returned to
		 */
		errno = sys_hvm_get_desc((int) a[0],
					 (guest_seg_t) a[1], (uintptr_t) a[2]);
		break;
	case SYS_hvm_get_next_eip:
		/*
		 * Get guest EIP of the next instruction in the virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the guest instruction
		 * a[2]: the linear address where the next EIP is returned to
		 */
		errno = sys_hvm_get_next_eip((int) a[0], (guest_instr_t) a[1],
					     (uintptr_t) a[2]);
		break;
	case SYS_hvm_inject_event:
		/*
		 * Inject an event to the virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the linear address of the event information
		 */
		errno = sys_hvm_inject_event((int) a[0], (uintptr_t) a[1]);
		break;
	case SYS_hvm_pending_event:
		/*
		 * Check whether there's pending event in a virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the linear address where the result is returned to
		 */
		errno = sys_hvm_pending_event((int) a[0], (uintptr_t) a[1]);
		break;
	case SYS_hvm_intr_shadow:
		/*
		 * Check whether the virtual machine is in the interrupt shadow.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the linear address where the result is returned to
		 */
		errno = sys_hvm_intr_shadow((int) a[0], (uintptr_t) a[1]);
		break;
	case SYS_hvm_intercept_ioport:
		/*
		 * Enable/Disable intercepting an I/O port of the virtual
		 * machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: the I/O port
		 * a[2]: enable/disable
		 */
		errno = sys_hvm_intercept_ioport((int) a[0],
						 (uint16_t) a[1], (bool) a[2]);
		break;
	case SYS_hvm_intercept_msr:
		/*
		 * Enable/Disable intercepting a MSR of the virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: MSR
		 * a[2]: enable/disable
		 */
		errno = sys_hvm_intercept_msr((int) a[0],
					      (uint32_t) a[1], (bool) a[2]);
		break;
	case SYS_hvm_intercept_intr_window:
		/*
		 * Enable/Disable intercepting the interrupt windows of the
		 * virtual machine.
		 * a[0]: the virtual machine descriptor
		 * a[1]: enable/disable
		 */
		errno = sys_hvm_intercept_intr_window((int) a[0], (bool) a[1]);
		break;
	case SYS_hvm_mmap_bios:
		errno = sys_hvm_mmap_bios((int) a[0], (uintptr_t) a[1]);
		break;
	case SYS_read_ioport:
		/*
		 * Read an I/O port.
		 * a[0]: the port number
		 * a[1]: the data width
		 * a[2]: the linear address where the data is returned to
		 */
		errno = sys_read_ioport((uint16_t) a[0], (data_sz_t) a[1],
					(uintptr_t) a[2]);
		break;
	case SYS_write_ioport:
		/*
		 * Write an I/O port.
		 * a[0]: the port number
		 * a[1]: the data width
		 * a[2]: the data
		 */
		errno = sys_write_ioport((uint16_t) a[0], (data_sz_t) a[1],
					 (uint32_t) a[2]);
		break;
	default:
		errno = E_INVAL_CALLNR;
		break;
	}

	ctx_set_retval(ctx, errno);

	if (errno)
		NR_DEBUG(nr, "failed (error %s).\n", ERRNO_STR(errno));
	else
		NR_DEBUG(nr, "done.\n");
	return errno;
}

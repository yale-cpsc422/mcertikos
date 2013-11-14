#include <preinit/lib/debug.h>
#include <dev/ide.h>
#include <virt/svm.h>
#include "syscall_args.h"
#include "syscall.h"

#define PAGESIZE			4096

#define NUM_PROC			64

#define VM_USERHI			0xf0000000
#define VM_USERLO			0x40000000

#define PTE_P				0x001	/* Present */
#define PTE_W				0x002	/* Writeable */
#define PTE_U				0x004	/* User-accessible */

#define VMEXIT_IOIO			0x07b
#define VMEXIT_NPF			0x400

#define VMCB_EVENTINJ_TYPE_INTR		0
#define VMCB_EVENTINJ_TYPE_EXCPT	2

#define ATA_SECTOR_SIZE			512

/*
 * Simulate adding an unsigned 64-bit integer (a) to an unsigned 32-bit integer
 * (b) and return an unsigned 64-bit integer (c). CompCert doesn't support
 * 64-bit integers.
 *
 * @param a_lo the lower 32-bit of the operand a
 * @param a_hi the higher 32-bit of the operand a
 * @param b    the operand b
 *
 * @return c_lo the lower 32-bit of the result c
 * @return c_hi the higher 32-bit of the result c
 */
#define u64_add_u32(a_lo, a_hi, b, c_lo, c_hi) do {	\
		unsigned int __a_lo = (a_lo);		\
		unsigned int __a_hi = (a_hi);		\
		unsigned int __b = (b);			\
		unsigned int __delta;			\
		if (0xffffffff - __a_lo < __b) {	\
			__delta = 0xffffffff - __a_lo;	\
			(c_lo) = __b - __delta -1;	\
			(c_hi) = __a_hi + 1;		\
		} else {				\
			(c_hi) = __a_hi;		\
			(c_lo) = __a_lo + __b;		\
		}					\
	} while (0)

static char sys_buf[NUM_PROC][PAGESIZE];
static void *elf_list[1];

void
sys_puts(void)
{
	unsigned int cur_pid;
	unsigned int str_uva, str_len;
	unsigned int remain, cur_pos, nbytes;

	cur_pid = get_curid();
	str_uva = syscall_get_arg2();
	str_len = syscall_get_arg3();

	if (!(VM_USERLO <= str_uva && str_uva + str_len <= VM_USERHI)) {
		syscall_set_errno(E_INVAL_ADDR);
		return;
	}

	remain = str_len;
	cur_pos = str_uva;

	while (remain) {
		if (remain < PAGESIZE - 1)
			nbytes = remain;
		else
			nbytes = PAGESIZE - 1;

		if (pt_copyin(cur_pid,
			      cur_pos, sys_buf[cur_pid], nbytes) != nbytes) {
			syscall_set_errno(E_MEM);
			return;
		}

		sys_buf[cur_pid][nbytes] = '\0';
		KERN_INFO("%s", sys_buf[cur_pid]);

		remain -= nbytes;
		cur_pos += nbytes;
	}

	syscall_set_errno(E_SUCC);
}

void
sys_spawn(void)
{
	unsigned int cur_pid;
	unsigned int new_pid;
	unsigned int elf_id;

	cur_pid = get_curid();
	elf_id = syscall_get_arg2();
	new_pid = proc_create(elf_list[elf_id]);

	if (new_pid == NUM_PROC) {
		syscall_set_errno(E_INVAL_PID);
		syscall_set_retval1(NUM_PROC);
	} else {
		syscall_set_errno(E_SUCC);
		syscall_set_retval1(new_pid);
	}
}

void
sys_yield(void)
{
	thread_yield();
	syscall_set_errno(E_SUCC);
}

static int
__sys_disk_read(unsigned int lba_lo, unsigned int lba_hi, unsigned int nsectors,
		unsigned int buf)
{
	unsigned int cur_lba_lo, cur_lba_hi;
	unsigned int remaining;
	unsigned int cur_la;
	unsigned int cur_pid;
	unsigned int n;

	cur_pid = get_curid();

	cur_lba_lo = lba_lo;
	cur_lba_hi = lba_hi;
	remaining = nsectors;
	cur_la = buf;

	while (remaining > 0) {
		if (remaining < PAGESIZE / ATA_SECTOR_SIZE)
			n = remaining;
		else
			n = PAGESIZE / ATA_SECTOR_SIZE;

		if (ide_disk_read(cur_lba_lo, cur_lba_hi, sys_buf[cur_pid], n))
			return E_DISK_OP;
		if (pt_copyout(sys_buf[cur_pid], cur_pid, cur_la,
			       n * ATA_SECTOR_SIZE) != n * ATA_SECTOR_SIZE)
			return E_MEM;

		u64_add_u32(cur_lba_lo, cur_lba_hi, n, cur_lba_lo, cur_lba_hi);
		remaining -= n;
		cur_la += n * ATA_SECTOR_SIZE;
	}

	return E_SUCC;
}

static int
__sys_disk_write(unsigned int lba_lo, unsigned int lba_hi, unsigned int nsectors,
		 unsigned int buf)
{
	unsigned int cur_lba_lo, cur_lba_hi;
	unsigned int remaining;
	unsigned int cur_la;
	unsigned int cur_pid;
	unsigned int n;

	cur_pid = get_curid();

	cur_lba_lo = lba_lo;
	cur_lba_hi = lba_hi;
	remaining = nsectors;
	cur_la = buf;

	while (remaining > 0) {
		if (remaining < PAGESIZE / ATA_SECTOR_SIZE)
			n = remaining;
		else
			n = PAGESIZE / ATA_SECTOR_SIZE;

		if (pt_copyin(cur_pid, cur_la, sys_buf[cur_pid],
			      n * ATA_SECTOR_SIZE) != n * ATA_SECTOR_SIZE)
			return E_MEM;
		if (ide_disk_write(cur_lba_lo, cur_lba_hi, sys_buf[cur_pid], n))
			return E_DISK_OP;

		u64_add_u32(cur_lba_lo, cur_lba_hi, n, cur_lba_lo, cur_lba_hi);
		remaining -= n;
		cur_la += n * ATA_SECTOR_SIZE;
	}

	return E_SUCC;
}

void
sys_disk_op(void)
{
	unsigned int op;
	unsigned int lba_lo;
	unsigned int lba_hi;
	unsigned int nsects;
	unsigned int buf_uva;
	unsigned int rc;

	op = syscall_get_arg2();
	lba_lo = syscall_get_arg3();
	lba_hi = syscall_get_arg4();
	nsects = syscall_get_arg5();
	buf_uva = syscall_get_arg6();

	if (!(VM_USERLO <= buf_uva &&
	      buf_uva + nsects * ATA_SECTOR_SIZE <= VM_USERHI)) {
		syscall_set_errno(E_INVAL_ADDR);
		return;
	}

	if (op == DISK_READ)
		rc = __sys_disk_read(lba_lo, lba_hi, nsects, buf_uva);
	else if (op == DISK_WRITE)
		rc = __sys_disk_write(lba_lo, lba_hi, nsects, buf_uva);
	else
		rc = 1;

	if (rc)
		syscall_set_errno(E_DISK_OP);
	else
		syscall_set_errno(E_SUCC);
}

void
sys_disk_cap(void)
{
	unsigned int cap_lo, cap_hi;

	cap_lo = ide_disk_size_lo();
	cap_hi = ide_disk_size_hi();

	syscall_set_retval1(cap_lo);
	syscall_set_retval2(cap_hi);
	syscall_set_errno(E_SUCC);
}

void
sys_hvm_run_vm(void)
{
	svm_run_vm();
	svm_sync();

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_get_exitinfo(void)
{
	unsigned int reason;
	unsigned int flags;

	reason = svm_get_exit_reason();
	flags = 0;

	syscall_set_retval1(reason);

	if (reason == VMEXIT_IOIO) {
		syscall_set_retval2(svm_get_exit_io_port());
		syscall_set_retval3(svm_get_exit_io_width());
		if (svm_get_exit_io_write())
			flags |= (1 << 0);
		if (svm_get_exit_io_rep())
			flags |= (1 << 1);
		if (svm_get_exit_io_str())
			flags |= (1 << 2);
		syscall_set_retval4(flags);
	} else if (reason == VMEXIT_NPF) {
		syscall_set_retval2(svm_get_exit_fault_addr());
	}

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_mmap(void)
{
	unsigned int cur_pid;
	unsigned int gpa;
	unsigned int hva;
	unsigned int hpa;

	cur_pid = get_curid();
	gpa = syscall_get_arg2();
	hva = syscall_get_arg3();

	if (hva % PAGESIZE != 0 || gpa % PAGESIZE != 0 ||
	    !(VM_USERLO <= hva && hva + PAGESIZE <= VM_USERHI)) {
		syscall_set_errno(E_INVAL_ADDR);
		return;
	}

	hpa = pt_read(cur_pid, hva);

	if ((hpa & PTE_P) == 0) {
		pt_resv(cur_pid, hva, PTE_P | PTE_U | PTE_W);
		hpa = pt_read(cur_pid, hva);
	}

	hpa = (hpa & 0xfffff000) + (hva % PAGESIZE);

	npt_insert(gpa, hpa);

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_set_reg(void)
{
	unsigned int reg;
	unsigned int val;

	reg = syscall_get_arg2();
	val = syscall_get_arg3();

	if (!(GUEST_EAX <= reg && reg < GUEST_MAX_REG)) {
		syscall_set_errno(E_INVAL_REG);
		return;
	}

	svm_set_reg(reg, val);

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_get_reg(void)
{
	unsigned int reg;

	reg = syscall_get_arg2();

	if (!(GUEST_EAX <= reg && reg < GUEST_MAX_REG)) {
		syscall_set_errno(E_INVAL_REG);
		return;
	}

	syscall_set_errno(E_SUCC);
	syscall_set_retval1(svm_get_reg(reg));
}

void
sys_hvm_set_seg(void)
{
	unsigned int seg;
	unsigned int sel;
	unsigned int base;
	unsigned int lim;
	unsigned int ar;

	seg = syscall_get_arg2();
	sel = syscall_get_arg3();
	base = syscall_get_arg4();
	lim = syscall_get_arg5();
	ar = syscall_get_arg6();

	if (!(GUEST_CS <= seg && seg < GUEST_MAX_SEG_DESC)) {
		syscall_set_errno(E_INVAL_SEG);
		return;
	}

	vmcb_set_seg(seg, sel, base, lim, ar);

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_get_next_eip(void)
{
	unsigned int reason;

	reason = svm_get_exit_reason();

	if (reason == VMEXIT_IOIO)
		syscall_set_retval1(svm_get_exit_io_neip());
	else
		syscall_set_retval1(vmcb_get_next_eip());

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_inject_event(void)
{
	unsigned int ev_type;
	unsigned int vector;
	unsigned int errcode;
	unsigned int ev;

	ev_type = syscall_get_arg2();
	vector = syscall_get_arg3();
	errcode = syscall_get_arg4();
	ev = syscall_get_arg5();

	if (!(0 <= vector && vector < 256)) {
		KERN_DEBUG("Invalid vector number %d.\n", vector);
		syscall_set_errno(E_INVAL_EVENT);
		return;
	}

	if (ev_type != VMCB_EVENTINJ_TYPE_INTR &&
	    ev_type != VMCB_EVENTINJ_TYPE_EXCPT) {
		KERN_DEBUG("Invalid event type %d.\n", ev_type);
		syscall_set_errno(E_INVAL_EVENT);
		return;
	}

	vmcb_inject_event(ev_type, vector, errcode, ev);

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_check_pending_event(void)
{
	syscall_set_errno(E_SUCC);
	syscall_set_retval1(vmcb_check_pending_event());
}

void
sys_hvm_check_int_shadow(void)
{
	syscall_set_errno(E_SUCC);
	syscall_set_retval1(vmcb_check_int_shadow());
}

void
sys_hvm_intercept_int_window(void)
{
	unsigned int enable;
	enable = syscall_get_arg2();
	svm_set_intercept_intwin(enable);
	syscall_set_errno(E_SUCC);
}

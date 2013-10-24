#include <lib/export.h>
#include <dev/export.h>
#include <mm/export.h>
#include <proc/export.h>
#include <virt/export.h>

#include "syscall.h"

#define VM_USERHI	0xf0000000
#define VM_USERLO	0x40000000

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */


#define u64_add_u32(a_lo, a_hi, b, c_lo, c_hi) do {	\
		uint32_t __a_lo = (a_lo);		\
		uint32_t __a_hi = (a_hi);		\
		uint32_t __b = (b);			\
		uint32_t __delta;			\
		if (0xffffffff - __a_lo < __b) {	\
			__delta = 0xffffffff - __a_lo;	\
			(c_lo) = __b - __delta -1;	\
			(c_hi) = __a_hi + 1;		\
		} else {				\
			(c_hi) = __a_hi;		\
			(c_lo) = __a_lo + __b;		\
		}					\
	} while (0)

static int
sys_puts(uintptr_t str_la, size_t len)
{
	if (!(VM_USERLO <= str_la && str_la + len <= VM_USERHI))
		return E_INVAL_ADDR;

	struct proc *p = proc_cur();
	size_t remain = len;
	uintptr_t cur_pos = str_la;

	while (remain) {
		size_t nbytes = min(remain, PAGESIZE-1);

		if (pt_copyin(p->pmap_id, cur_pos, p->sysbuf, nbytes) != nbytes)
			return E_MEM;

		p->sysbuf[nbytes] = 0;
		KERN_INFO("%s", (char *) p->sysbuf);

		remain -= nbytes;
		cur_pos += nbytes;
	}

	return E_SUCC;
}

static int
sys_spawn(struct context *ctx, uintptr_t uelf_addr)
{
	if (!(VM_USERLO <= uelf_addr && uelf_addr < VM_USERHI))
		return E_INVAL_ADDR;

	struct proc *p;

	if ((p = proc_create(uelf_addr)) == NULL)
		return E_INVAL_PID;

	ctx_set_retval1(ctx, p->pid);

	return E_SUCC;
}

static int
sys_yield(void)
{
	proc_yield();
	return E_SUCC;
}

static int
__sys_disk_read(uint32_t lba_lo, uint32_t lba_hi, uint32_t nsectors,
		uintptr_t buf)
{
	uint32_t cur_lba_lo, cur_lba_hi;
	uint32_t remaining;
	uintptr_t cur_la;

	struct proc *p = proc_cur();

	cur_lba_lo = lba_lo;
	cur_lba_hi = lba_hi;
	remaining = nsectors;
	cur_la = buf;

	while (remaining > 0) {
		uint32_t n = min(remaining, PAGESIZE / ATA_SECTOR_SIZE);

		if (ide_disk_read(cur_lba_lo, cur_lba_hi, p->sysbuf, n))
			return E_DISK_OP;
		if (pt_copyout(p->sysbuf, p->pmap_id, cur_la,
			       n * ATA_SECTOR_SIZE) != n * ATA_SECTOR_SIZE)
			return E_MEM;

		u64_add_u32(cur_lba_lo, cur_lba_hi, n, cur_lba_lo, cur_lba_hi);
		remaining -= n;
		cur_la += n * ATA_SECTOR_SIZE;
	}

	return E_SUCC;
}

static int
__sys_disk_write(uint32_t lba_lo, uint32_t lba_hi, uint32_t nsectors,
		 uintptr_t buf)
{
	uint32_t cur_lba_lo, cur_lba_hi;
	uint32_t remaining;
	uintptr_t cur_la;

	struct proc *p = proc_cur();

	cur_lba_lo = lba_lo;
	cur_lba_hi = lba_hi;
	remaining = nsectors;
	cur_la = buf;

	while (remaining > 0) {
		uint32_t n = min(remaining, PAGESIZE / ATA_SECTOR_SIZE);

		if (pt_copyin(p->pmap_id, cur_la, p->sysbuf,
			      n * ATA_SECTOR_SIZE) != n * ATA_SECTOR_SIZE)
			return E_MEM;
		if (ide_disk_write(cur_lba_lo, cur_lba_hi, p->sysbuf, n))
			return E_DISK_OP;

		u64_add_u32(cur_lba_lo, cur_lba_hi, n, cur_lba_lo, cur_lba_hi);
		remaining -= n;
		cur_la += n * ATA_SECTOR_SIZE;
	}

	return E_SUCC;
}

static int
sys_disk_op(int op, uint32_t lba_lo, uint32_t lba_hi, uint32_t nsects,
	    uintptr_t buf_la)
{
	int rc = 0;

	if (!(VM_USERLO <= buf_la &&
	      buf_la + nsects * ATA_SECTOR_SIZE <= VM_USERHI))
		return E_INVAL_ADDR;

	switch (op) {
	case DISK_READ:
		rc = __sys_disk_read(lba_lo, lba_hi, nsects, buf_la);
		break;
	case DISK_WRITE:
		rc = __sys_disk_write(lba_lo, lba_hi, nsects, buf_la);
		break;
	default:
		rc = 1;
	}

	if (rc)
		return E_DISK_OP;

	return E_SUCC;
}

static int
sys_disk_cap(struct context *ctx)
{
	uint32_t cap_lo, cap_hi;

	cap_lo = ide_disk_size_lo();
	cap_hi = ide_disk_size_hi();

	ctx_set_retval1(ctx, cap_lo);
	ctx_set_retval2(ctx, cap_hi);

	return E_SUCC;
}

static int
sys_hvm_create_vm(struct context *ctx)
{
	svm_init_vm();

	ctx_set_retval1(ctx, 0);

	return E_SUCC;
}

static int
sys_hvm_run_vm(struct context *ctx, int vmid)
{
	ctx_set_errno(ctx, (vmid == 0) ? E_SUCC : E_INVAL_VMID);
	svm_run_vm();
	return E_SUCC;
}

static int
sys_hvm_get_exitinfo(struct context *ctx, int vmid)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	svm_sync();

	exit_reason_t reason = svm_get_exit_reason();
	uint32_t flags = 0;

	ctx_set_retval1(ctx, reason);

	if (reason == EXIT_REASON_IOPORT) {
		ctx_set_retval2(ctx, svm_get_exit_io_port());
		ctx_set_retval3(ctx, svm_get_exit_io_width());
		if (svm_get_exit_io_write())
			flags |= (1 << 0);
		if (svm_get_exit_io_rep())
			flags |= (1 << 1);
		if (svm_get_exit_io_str())
			flags |= (1 << 2);
		ctx_set_retval4(ctx, flags);
	} else if (reason == EXIT_REASON_PGFLT) {
		ctx_set_retval2(ctx, svm_get_exit_fault_addr());
	}

	return E_SUCC;
}

static int
sys_hvm_set_mmap(int vmid, uint32_t gpa, uint32_t hva)
{
	uint32_t hpa;

	if (vmid != 0)
		return E_INVAL_VMID;

	if (gpa % PAGESIZE != 0 || hva % PAGESIZE != 0 ||
	    !(VM_USERLO <= hva && hva + PAGESIZE <= VM_USERHI))
		return E_INVAL_ADDR;

	hpa = pt_read(proc_cur()->pmap_id, hva);

	if ((hpa & PTE_P) == 0) {
		pt_resv(proc_cur()->pmap_id, hva, PTE_P | PTE_U | PTE_W);
		hpa = pt_read(proc_cur()->pmap_id, hva);
	}

	hpa = (hpa & 0xfffff000) + (hva % PAGESIZE);

	svm_sync();
	svm_set_mmap(gpa, hpa);

	return E_SUCC;
}

static int
sys_hvm_set_reg(int vmid, guest_reg_t reg, uint32_t val)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	if (!(GUEST_EAX <= reg && reg < GUEST_MAX_REG))
		return E_INVAL_REG;

	svm_sync();
	svm_set_reg(reg, val);

	return E_SUCC;
}

static int
sys_hvm_get_reg(struct context *ctx, int vmid, guest_reg_t reg)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	if (!(GUEST_EAX <= reg && reg < GUEST_MAX_REG))
		return E_INVAL_REG;

	svm_sync();

	ctx_set_retval1(ctx, svm_get_reg(reg));

	return E_SUCC;
}

static int
sys_hvm_set_seg(int vmid, guest_seg_t seg, uintptr_t desc_la)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	struct guest_seg_desc desc;

	if (!(GUEST_CS <= seg && seg < GUEST_MAX_SEG_DESC))
		return E_INVAL_SEG;

	if (!(VM_USERLO <= desc_la && desc_la + sizeof(desc) <= VM_USERHI))
		return E_INVAL_ADDR;

	if (pt_copyin(proc_cur()->pmap_id, desc_la, (char *) &desc,
		      sizeof(desc)) != sizeof(desc))
		return E_MEM;

	svm_sync();
	svm_set_seg(seg, desc.sel, desc.base, desc.lim, desc.ar);

	return E_SUCC;
}

static int
sys_hvm_get_next_eip(struct context *ctx, int vmid, guest_instr_t instr)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	svm_sync();

	exit_reason_t reason = svm_get_exit_reason();

	if (reason == EXIT_REASON_IOPORT)
		ctx_set_retval1(ctx, svm_get_exit_io_neip());
	else
		ctx_set_retval1(ctx, svm_get_next_eip());

	return E_SUCC;
}

static int
sys_hvm_inject_event(int vmid, guest_event_t ev_type, uint8_t vector,
		     uint32_t errcode, bool ev)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	if (ev_type != EVENT_EXTINT && ev_type != EVENT_EXCEPTION)
		return E_INVAL_EVENT;

	svm_sync();
	svm_inject_event(ev_type, vector, errcode, ev);

	return E_SUCC;
}

static int
sys_hvm_pending_event(struct context *ctx, int vmid)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	svm_sync();
	ctx_set_retval1(ctx, svm_check_pending_event());

	return E_SUCC;
}

static int
sys_hvm_intr_shadow(struct context *ctx, int vmid)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	svm_sync();
	ctx_set_retval1(ctx, svm_check_int_shadow());

	return E_SUCC;
}

static int
sys_hvm_intercept_intr_window(int vmid, bool enable)
{
	if (vmid != 0)
		return E_INVAL_VMID;

	svm_sync();

	if (enable == TRUE)
		svm_set_intercept_vint();
	else
		svm_clear_intercept_vint();

	return E_SUCC;
}

static int
sys_read_ioport(struct context *ctx, uint16_t port, data_sz_t width)
{
	uint32_t data;

	if (width == SZ8)
		data = inb(port);
	else if (width == SZ16)
		data = inw(port);
	else if (width == SZ32)
		data = inl(port);
	else
		return E_INVAL_PORT;

	ctx_set_retval1(ctx, data);

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

void
syscall_handler(void)
{
	struct context *ctx = &proc_cur()->uctx;
	uint32_t nr = ctx_arg1(ctx);

	uint32_t a[5];
	a[0] = ctx_arg2(ctx);
	a[1] = ctx_arg3(ctx);
	a[2] = ctx_arg4(ctx);
	a[3] = ctx_arg5(ctx);
	a[4] = ctx_arg6(ctx);

	uint32_t errno;

	switch (nr) {
	case SYS_puts:
		/*
		 * Output a string to the screen.
		 *
		 * Parameters:
		 *   a[0]: the linear address where the string is
		 *   a[1]: the length of the string
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_MEM
		 */
		errno = sys_puts((uintptr_t) a[0], (size_t) a[1]);
		break;
	case SYS_spawn:
		/*
		 * Create a new process.
		 *
		 * Parameters:
		 *   a[0]: the identifier of the ELF image
		 *
		 * Return:
		 *   the process ID of the process
		 *
		 * Error:
		 *   E_INVAL_ADDR, E_INVAL_PID
		 */
		errno = sys_spawn(ctx, (int) a[0]);
		break;
	case SYS_yield:
		/*
		 * Called by a process to abandon its CPU slice.
		 *
		 * Parameters:
		 *   None.
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   None.
		 */
		errno = sys_yield();
		break;
	case SYS_disk_op:
		/*
		 * Disk operation. The operation information must be provided in
		 * an object of type struct user_disk_op by the caller.
		 *
		 * Parameters:
		 *   a[0]: the type of the disk operation: 0 read, 1 write
		 *   a[1]: the lower 32-bit of LBA
		 *   a[2]: the higher 32-bit of LBA
		 *   a[3]: the number of sectors
		 *   a[4]: the user linear address of the buffer
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_ADDR, E_DISK_NODRV, E_MEM, E_DISK_OP
		 */
		errno = sys_disk_op((int) a[0], (uint32_t) a[1], (uint32_t) a[2],
				    (uint32_t) a[3], (uintptr_t) a[4]);
#ifdef DEBUG_DISK
		if (errno)
			KERN_DEBUG("sys_disk_op() failed. (errno %d)\n", errno);
#endif
		break;
	case SYS_disk_cap:
		/*
		 * Get the capability of the disk for the virtual machine.
		 *
		 * Parameters:
		 *   None.
		 *
		 * Return:
		 *   1st: the lower 32-bit of the capability
		 *   2nd: the higher 32-bit of the capability
		 *
		 * Error:
		 *   E_DISK_NONDRV
		 */
		errno = sys_disk_cap(ctx);
#ifdef DEBUG_DISK
		if (errno)
			KERN_DEBUG("sys_disk_cap() failed. (errno %d)\n", errno);
#endif
		break;
	case SYS_hvm_create_vm:
		/*
		 * Create a new virtual machine descriptor.
		 *
		 * Parameters:
		 *
		 * Return:
		 *   the virtual machine ID of the new virtual machine
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		errno = sys_hvm_create_vm(ctx);
		break;
	case SYS_hvm_run_vm:
		/*
		 * Run a virtual machine and returns when a VMEXIT or an error
		 * happens.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID
		 *
		 */
		errno = sys_hvm_run_vm(ctx, (int) a[0]);
		break;
	case SYS_hvm_get_exitinfo:
		/*
		 * Get the information of the latest VMEXIT.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *
		 * Error:
		 *
		 */
		errno = sys_hvm_get_exitinfo(ctx, (int) a[0]);
		break;
	case SYS_hvm_set_mmap:
		/*
		 * Map guest physical pages to a host virtual pages.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest physical address
		 *   a[2]: the host virtual address
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_ADDR, E_INVAL_CACHE_TYPE, E_HVM_MMAP
		 */
		errno = sys_hvm_set_mmap((int) a[0],
					 (uintptr_t) a[1], (uintptr_t) a[2]);
		break;
	case SYS_hvm_set_reg:
		/*
		 * Set the register of a virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest register
		 *   a[2]: the value of the register
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_REG, E_HVM_REG
		 */
		errno = sys_hvm_set_reg((int) a[0],
					(guest_reg_t) a[1], (uint32_t) a[2]);
		break;
	case SYS_hvm_get_reg:
		/*
		 * Get the value of the register of a virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest register
		 *
		 * Return:
		 *   the value of the registers
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_REG, E_HVM_REG
		 */
		errno = sys_hvm_get_reg(ctx, (int) a[0], (guest_reg_t) a[1]);
		break;
	case SYS_hvm_set_seg:
		/*
		 * Set the segment descriptor of a virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest segment
		 *   a[2]: the linear address of the descriptor information
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_SEG, E_INVAL_ADDR, E_MEM, E_HVM_SEG
		 */
		errno = sys_hvm_set_seg((int) a[0],
					(guest_seg_t) a[1], (uintptr_t) a[2]);
		break;
	case SYS_hvm_get_next_eip:
		/*
		 * Get guest EIP of the next instruction in the virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the guest instruction
		 *
		 * Return:
		 *   the guest physical address of the next instruction
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		errno = sys_hvm_get_next_eip(ctx,
					     (int) a[0], (guest_instr_t) a[1]);
		break;
	case SYS_hvm_inject_event:
		/*
		 * Inject an event to the virtual machine.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: the event type
		 *   a[2]: the vector number
		 *   a[3]: the error code
		 *   a[4]: is the error code valid
		 *
		 * Return:
		 *   None.
		 *
		 * Error:
		 *   E_INVAL_VMID, E_INVAL_EVENT, E_HVM_INJECT
		 */
		errno = sys_hvm_inject_event((int) a[0], (guest_event_t) a[1],
					     (uint8_t) a[2], (uint32_t) a[3],
					     (bool) a[4]);
		break;
	case SYS_hvm_pending_event:
		/*
		 * Check whether the virtual machine have pending events.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *   1, if existing pending events; 0, if not
		 *
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		errno = sys_hvm_pending_event(ctx, (int) a[0]);
		break;
	case SYS_hvm_intr_shadow:
		/*
		 * Check whether the virtual machine is in the interrupt shadow.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *
		 * Return:
		 *   1, if in the interrupt shadow; 0, if not
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		errno = sys_hvm_intr_shadow(ctx, (int) a[0]);
		break;
	case SYS_hvm_intercept_intr_window:
		/*
		 * Enable/Disable intercepting the interrupt windows.
		 *
		 * Parameters:
		 *   a[0]: the virtual machine descriptor
		 *   a[1]: TRUE - enable; FALSE - disable
		 *
		 * Return:
		 *   None
		 *
		 * Error:
		 *   E_INVAL_VMID
		 */
		errno = sys_hvm_intercept_intr_window((int) a[0], (bool) a[1]);
		break;
	case SYS_read_ioport:
		/*
		 * Read the host I/O port.
		 *
		 * Parameters:
		 *   a[0]: the I/O port
		 *   a[1]: the data width
		 *
		 * Return:
		 *   the read data
		 *
		 * Error:
		 */
		errno = sys_read_ioport(ctx, (uint16_t) a[0], (data_sz_t) a[1]);
		break;
	case SYS_write_ioport:
		/*
		 * Write the host I/O port.
		 *
		 * Parameters:
		 *   a[0]: the I/O port
		 *   a[1]: the data width
		 *   a[2]: the data
		 *
		 * Return:
		 *   None
		 *
		 * Error:
		 */
		errno = sys_write_ioport((uint16_t) a[0], (data_sz_t) a[1],
					 (uint32_t) a[2]);
		break;
	default:
		errno = E_INVAL_CALLNR;
	}

	ctx_set_errno(ctx, errno);
}

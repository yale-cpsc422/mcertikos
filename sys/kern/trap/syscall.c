#include <kern/proc/proc.h>
#include <preinit/lib/debug.h>
#include <preinit/lib/types.h>
#include <preinit/lib/x86.h>
#include <preinit/dev/ide.h>
#include <virt/svm/svm.h>
#include <virt/vmx/vmx.h>
#include <lib/trap.h>
#include "syscall_args.h"
#include "syscall.h"

#define PAGESIZE			4096

#define NUM_PROC			64
#define NUM_CHAN			64

#define VM_USERHI			0xf0000000
#define VM_USERLO			0x40000000

#define PTE_P				0x001	/* Present */
#define PTE_W				0x002	/* Writeable */
#define PTE_U				0x004	/* User-accessible */

#define VMEXIT_IOIO			0x07b
#define VMEXIT_NPF			0x400

#define VMCB_EVENTINJ_TYPE_INTR		0
#define VMCB_EVENTINJ_TYPE_EXCPT	3

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

extern cpu_vendor cpuvendor;

static char sys_buf[NUM_PROC][PAGESIZE];

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
sys_ring0_spawn(void)
{
    unsigned int id;
    unsigned int new_pid;

    id = syscall_get_arg2();
    new_pid = ring0proc_create(id);
	
    if (new_pid == NUM_PROC) {
		syscall_set_errno(E_INVAL_PID);
		syscall_set_retval1(NUM_PROC);
	} else {
		syscall_set_errno(E_SUCC);
		syscall_set_retval1(new_pid);
	}
}

extern uint8_t _binary___obj_user_vmm_vmm_start[];
extern uint8_t _binary___obj_user_pingpong_ping_start[];
extern uint8_t _binary___obj_user_pingpong_pong_start[];

void
sys_spawn(void)
{
	unsigned int new_pid;
	unsigned int elf_id;
	void *elf_addr;

	elf_id = syscall_get_arg2();

	if (elf_id == 0) {
		elf_addr = _binary___obj_user_vmm_vmm_start;
	} else if (elf_id == 1) {
		elf_addr = _binary___obj_user_pingpong_ping_start;
	} else if (elf_id == 2) {
		elf_addr = _binary___obj_user_pingpong_pong_start;
	} else {
		syscall_set_errno(E_INVAL_PID);
		syscall_set_retval1(NUM_PROC);
		return;
	}

	new_pid = proc_create(elf_addr);

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
    if (cpuvendor == AMD) {
	    svm_run_vm();
	    svm_sync();
    }
    else if (cpuvendor == INTEL) {
        vmx_run_vm();
    }

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_get_exitinfo(void)
{
	unsigned int reason;
	unsigned int port;
	unsigned int width;
	unsigned int write;
	unsigned int rep;
	unsigned int str;
	unsigned int fault_addr;
	unsigned int flags;
    unsigned int reason_io;
    unsigned int reason_fault;
	flags = 0;

    if (cpuvendor == AMD) {
	    reason = svm_get_exit_reason();
        port = svm_get_exit_io_port();
        width = svm_get_exit_io_width();
        write = svm_get_exit_io_write();
        rep = svm_get_exit_io_rep();
        str = svm_get_exit_io_str();
        fault_addr = svm_get_exit_fault_addr();
        reason_io = VMEXIT_IOIO;
        reason_fault = VMEXIT_NPF;
    }
    else if (cpuvendor == INTEL) {
	    reason = vmx_get_exit_reason();
        port = vmx_get_exit_io_port();
        width = vmx_get_exit_io_width();
        write = vmx_get_exit_io_write();
        rep = vmx_get_exit_io_rep();
        str = vmx_get_exit_io_str();
        fault_addr = vmx_get_exit_fault_addr();
        reason_io = EXIT_REASON_INOUT;
        reason_fault = EXIT_REASON_EPT_FAULT;
    }

	syscall_set_retval1(reason);

	if (reason == reason_io) {
		syscall_set_retval2(port);
		syscall_set_retval3(width);
		if (write)
			flags |= (1 << 0);
		if (rep)
			flags |= (1 << 1);
		if (str)
			flags |= (1 << 2);
		syscall_set_retval4(flags);
	} else if (reason == reason_fault) {
		syscall_set_retval2(fault_addr);
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
    unsigned int mem_type;

	cur_pid = get_curid();
	gpa = syscall_get_arg2();
	hva = syscall_get_arg3();
    mem_type = syscall_get_arg4();

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

    if (cpuvendor == AMD) {
        npt_insert(gpa, hpa);
    }
    else if (cpuvendor == INTEL) {
        vmx_set_mmap(gpa, hpa, mem_type);
    }

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

    if (cpuvendor == AMD) {
	    svm_set_reg(reg, val);
    }
    else if (cpuvendor == INTEL) {
        vmx_set_reg(reg, val);
    }

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

    if (cpuvendor == AMD) {
        reg = svm_get_reg(reg);
    }
    else if (cpuvendor == INTEL) {
        reg = vmx_get_reg(reg);
    }

	syscall_set_retval1(reg);
	syscall_set_errno(E_SUCC);
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

    if (cpuvendor == AMD) {
	    vmcb_set_seg(seg, sel, base, lim, ar);
    }
    else if (cpuvendor == INTEL) {
        vmx_set_desc(seg, sel, base, lim, ar);
    }

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_get_next_eip(void)
{
	unsigned int reason;
    unsigned int neip;

    if (cpuvendor == AMD) {
	    reason = svm_get_exit_reason();

    	if (reason == VMEXIT_IOIO)
	    	neip = svm_get_exit_io_neip();
	    else
		    neip = vmcb_get_next_eip();
    }
    else if (cpuvendor == INTEL) {
        neip = vmx_get_next_eip();
    }
    
    syscall_set_retval1(neip);

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

    if (cpuvendor == AMD) {
	    vmcb_inject_event(ev_type, vector, errcode, ev);
    }
    else if (cpuvendor == INTEL) {
        vmx_inject_event(ev_type, vector, errcode, ev);
    }

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_check_pending_event(void)
{
    unsigned int event;

    if (cpuvendor == AMD) {
        event = vmcb_check_pending_event();
    }
    else if (cpuvendor == INTEL) {
        event = vmx_check_pending_event();
    }

	syscall_set_retval1(event);

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_check_int_shadow(void)
{
    unsigned int shadow;

    if (cpuvendor == AMD) {
        shadow = vmcb_check_int_shadow();
    }
    else if (cpuvendor == INTEL) {
        shadow = vmx_check_int_shadow();
    }

	syscall_set_retval1(shadow);

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_intercept_int_window(void)
{
	unsigned int enable;
	enable = syscall_get_arg2();

    if (cpuvendor == AMD) {
	    svm_set_intercept_intwin(enable);
    }
    else if (cpuvendor == INTEL) {
        vmx_set_intercept_intwin(enable);
    }

	syscall_set_errno(E_SUCC);
}

/*
 * Arch-independent inject event type
 */

typedef enum {
    EVENT_EXTINT,       /* external interrupt */
    EVENT_EXCEPTION     /* exception */
} guest_event_t;

/*
 * Instruction type
 */

typedef enum {
    INSTR_IN, INSTR_OUT, INSTR_RDMSR, INSTR_WRMSR, INSTR_CPUID, INSTR_RDTSC,
    INSTR_HYPERCALL
} guest_instr_t;


static int
vmx_get_msr(uint32_t msr, uint64_t *val)
{
    KERN_ASSERT(val != NULL);

    /*
    if (!(msr <= 0x00001fff || (0xc0000000 <= msr && msr <= 0xc0001fff)))
        return 1;
    */

    *val = rdmsr(msr);

#ifdef DEBUG_GUEST_MSR
    KERN_DEBUG("Guest rdmsr 0x%08x = 0x%llx.\n", msr, *val);
#endif

    return 0;
}


static int
vmx_set_msr(uint32_t msr, uint64_t val)
{
    /*
    if (!(msr <= 0x00001fff || (0xc0000000 <= msr && msr <= 0xc0001fff)))
        return 1;
    */

    wrmsr(msr, val);

#ifdef DEBUG_GUEST_MSR
    KERN_DEBUG("Guest wrmsr 0x%08x, 0x%llx.\n", msr, val);
#endif

    return 0;
}

void
sys_hvm_handle_rdmsr(void)
{
    uint32_t msr, next_eip;
    uint64_t val;

    msr = vmx_get_reg(GUEST_EAX);

    /*
     * XXX: I/O permission check is not necessary when using HVM.
     */
    if (vmx_get_msr(msr, &val)) {
#ifdef DEBUG_GUEST_MSR
        KERN_DEBUG("Guest rdmsr failed: invalid MSR 0x%llx.\n", msr);
#endif
        vmx_inject_event(EVENT_EXCEPTION, T_GPFLT, 0, TRUE);
    }
    else {
#ifdef DEBUG_GUEST_MSR
        KERN_DEBUG("Guest rdmsr 0x%08x = 0x%llx.\n", msr, val);
#endif

        vmx_set_reg(GUEST_EAX, val & 0xffffffff);
        vmx_set_reg(GUEST_EDX, (val >> 32) & 0xffffffff);

        next_eip = vmx_get_next_eip();
        vmx_set_reg(GUEST_EIP, next_eip);
    }

	syscall_set_errno(E_SUCC);
}

void
sys_hvm_handle_wrmsr(void)
{
	uint32_t msr, next_eip, eax, edx;
	uint64_t val;

	msr = vmx_get_reg(GUEST_ECX);
	eax = vmx_get_reg(GUEST_EAX);
	edx = vmx_get_reg(GUEST_EDX);
	val = ((uint64_t) edx << 32) | (uint64_t) eax;

	/*
	 * XXX: I/O permission check is not necessary when using HVM.
	 */
	if (vmx_set_msr(msr, val)) {
#ifdef DEBUG_GUEST_MSR
		KERN_DEBUG("Guest wrmsr failed: invalid MSR 0x%llx.\n", msr);
#endif
		vmx_inject_event(EVENT_EXCEPTION, T_GPFLT, 0, TRUE);
	}
    else {
#ifdef DEBUG_GUEST_MSR
	    KERN_DEBUG("Guest wrmsr 0x%08x, 0x%llx.\n", msr, val);
#endif
    	next_eip = vmx_get_next_eip();
	    vmx_set_reg(GUEST_EIP, next_eip);
    }

	syscall_set_errno(E_SUCC);
}



extern unsigned int is_chan_ready(void);
extern unsigned int send(unsigned int chid, unsigned int content);
extern unsigned int recv(void);
extern void thread_sleep(unsigned int chid);

void
sys_is_chan_ready(void)
{
	syscall_set_retval1(is_chan_ready());
	syscall_set_errno(E_SUCC);
}

void
sys_send(void)
{
	unsigned int chid;
	unsigned int content;
	unsigned int val;

	chid = syscall_get_arg2();

	if (!(0 <= chid && chid < NUM_CHAN)) {
		syscall_set_errno(E_INVAL_CHID);
		return;
	}

	content = syscall_get_arg3();

	if (content == 0) {
		syscall_set_errno(E_INVAL_CHID);
		return;
	}

	val = send(chid, content);

	if (val == 1)
		syscall_set_errno(E_SUCC);
	else
		syscall_set_errno(E_IPC);
}

void
sys_recv(void)
{
	unsigned int val;
	val = recv();
	syscall_set_retval1(val);
	syscall_set_errno(E_SUCC);
}

void
sys_sleep(void)
{
	unsigned int chid;
	chid = syscall_get_arg2();
	thread_sleep(chid);
	syscall_set_errno(E_SUCC);
}

#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_

#include <kern/trap/syscall.h>

#include <debug.h>
#include <gcc.h>
#include <proc.h>
#include <types.h>

#include <kern/virt/common.h>

static gcc_inline void
sys_puts(const char *s, size_t len)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_puts),
		       "b" (s),
		       "c" (len)
		     : "cc", "memory");
}

static gcc_inline pid_t
sys_spawn(uintptr_t exec)
{
	int errno;
	pid_t pid;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (pid)
		     : "i" (T_SYSCALL),
		       "a" (SYS_spawn),
		       "b" (exec)
		     : "cc", "memory");

	return errno ? -1 : pid;
}

static gcc_inline void
sys_yield(void)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_yield)
		     : "cc", "memory");
}

static gcc_inline int
sys_disk_read(uint64_t lba, uint32_t nsectors, void *buf)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_disk_op),
		       "b" (DISK_READ),
		       "c" ((uint32_t) (lba & 0xffffffff)),
		       "d" ((uint32_t) ((lba >> 32) & 0xffffffff)),
		       "S" (nsectors),
		       "D" ((uintptr_t) buf)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_disk_write(uint64_t lba, uint64_t nsectors, void *buf)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_disk_op),
		       "b" (DISK_WRITE),
		       "c" ((uint32_t) (lba & 0xffffffff)),
		       "d" ((uint32_t) ((lba >> 32) & 0xffffffff)),
		       "S" ((uint32_t) nsectors),
		       "D" ((uintptr_t) buf)
		     : "cc", "memory");

	return errno;
}

static gcc_inline uint64_t
sys_disk_capacity(void)
{
	int errno;
	uint32_t size_lo, size_hi;

	asm volatile("int %3"
		     : "=a" (errno),
		       "=b" (size_lo),
		       "=c" (size_hi)
		     : "i" (T_SYSCALL),
		       "a" (SYS_disk_cap)
		     : "cc", "memory");

	return errno ? 0 : ((uint64_t) size_hi << 32 | size_lo);
}

static gcc_inline int
sys_hvm_create_vm(void)
{
	int errno, vmid;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (vmid)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_create_vm)
		     : "cc", "memory");

	return errno ? -1 : vmid;
}

static gcc_inline int
sys_hvm_run_vm(int vmid)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_run_vm),
		       "b" (vmid)
		     : "cc", "memory");

	return errno ? errno : 0;
}

static gcc_inline int
sys_hvm_get_exitinfo(int vmid, exit_reason_t *reason, exit_info_t *info)
{
	int errno;
	exit_reason_t exit_reason;
	uint32_t exit_info[3];

	asm volatile("int %5"
		     : "=a" (errno),
		       "=b" (exit_reason),
		       "=c" (exit_info[0]),
		       "=d" (exit_info[1]),
		       "=S" (exit_info[2])
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_get_exitinfo),
		       "b" (vmid)
		     : "cc", "memory");

	if (errno)
		return errno;

	*reason = exit_reason;

	switch (exit_reason) {
	case EXIT_REASON_IOPORT:
		info->ioport.port = (uint16_t) exit_info[0];
		info->ioport.dw = (data_sz_t) exit_info[1];
		info->ioport.write = (exit_info[2] & 0x1) ? 1 : 0;
		info->ioport.rep = (exit_info[2] & 0x2) ? 1 : 0;
		info->ioport.str = (exit_info[2] & 0x4) ? 1 : 0;
		break;
	case EXIT_REASON_PGFLT:
		info->pgflt.addr = exit_info[0];
		break;
	default:
		break;
	}

	return 0;
}

static gcc_inline int
sys_hvm_set_mmap(int vmid, uintptr_t gpa, uintptr_t hva, int type)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_set_mmap),
		       "b" (vmid),
		       "c" (gpa),
		       "d" (hva)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_set_reg(int vmid, guest_reg_t reg, uint32_t val)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_set_reg),
		       "b" (vmid),
		       "c" (reg),
		       "d" (val)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_get_reg(int vmid, guest_reg_t reg, uint32_t *val)
{
	int errno;
	uint32_t reg_val;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (reg_val)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_get_reg),
		       "b" (vmid),
		       "c" (reg)
		     : "cc", "memory");

	*val = reg_val;

	return errno;
}

static gcc_inline int
sys_hvm_set_desc(int vmid, guest_seg_t seg, struct guest_seg_desc *desc)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_set_seg),
		       "b" (vmid),
		       "c" (seg),
		       "d" (desc)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_get_next_eip(int vmid, guest_instr_t instr, uint32_t *eip)
{
	int errno;
	uint32_t neip;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (neip)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_get_next_eip),
		       "b" (vmid),
		       "c" (instr)
		     : "cc", "memory");

	*eip = neip;

	return errno;
}

static gcc_inline int
sys_hvm_inject_event(int vmid, guest_event_t type,
		     uint8_t vector, uint32_t errcode, bool ev)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_inject_event),
		       "b" (vmid),
		       "c" (type),
		       "d" (vector),
		       "S" (errcode),
		       "D" (ev)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_pending_event(int vmid)
{
	int errno, result;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (result)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_pending_event),
		       "b" (vmid)
		     : "cc", "memory");

	return errno ? -1 : result;
}

static gcc_inline int
sys_hvm_intr_shadow(int vmid)
{
	int errno, result;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (result)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_intr_shadow),
		       "b" (vmid)
		     : "cc", "memory");

	return errno ? -1 : result;
}

static gcc_inline int
sys_hvm_intercept_intr_window(int vmid, bool enable)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_intercept_intr_window),
		       "b" (vmid),
		       "c" (enable)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_read_ioport(uint16_t port, data_sz_t width, void *val)
{
	int errno;
	uint32_t data;

	asm volatile("int %2"
		     : "=a" (errno),
		       "=b" (data)
		     : "i" (T_SYSCALL),
		       "a" (SYS_read_ioport),
		       "b" (port),
		       "c" (width)
		     : "cc", "memory");

	if (errno) {
		*(uint8_t *) val = 0;
	} else {
		switch (width) {
		case SZ8:
			*(uint8_t *) val = (uint8_t) data;
			break;
		case SZ16:
			*(uint16_t *) val = (uint16_t) data;
			break;
		case SZ32:
			*(uint32_t *) val = (uint32_t) data;
			break;
		default:
			*(uint8_t *) val = 0;
		}
	}

	return errno;
}

static gcc_inline int
sys_write_ioport(uint16_t port, data_sz_t width, uint32_t val)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_write_ioport),
		       "b" (port),
		       "c" (width),
		       "d" (val)
		     : "cc", "memory");

	return errno;
}

#endif /* !_USER_SYSCALL_H_ */

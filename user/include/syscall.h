#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_

#include <sys/syscall.h>

#include <debug.h>
#include <gcc.h>
#include <proc.h>
#include <types.h>

static gcc_inline void
sys_puts(const char *s)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_puts),
		       "b" (s)
		     : "cc", "memory");
}

static gcc_inline pid_t
sys_create_proc(chid_t chid)
{
	int errno;
	pid_t pid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_create_proc),
		       "b" (&pid),
		       "c" (chid)
		     : "cc", "memory");

	return errno ? -1 : pid;
}

static gcc_inline int
sys_run_proc(pid_t pid, uintptr_t exec)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_run_proc),
		       "b" (pid),
		       "c" (exec)
		     : "cc", "memory");

	return errno;
}

static gcc_inline void
sys_yield(void)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_yield)
		     : "cc", "memory");
}

static gcc_inline pid_t
sys_getpid(void)
{
	int errno;
	pid_t pid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_getpid),
		       "b" (&pid)
		     : "cc", "memory");

	return errno ? -1 : pid;
}

static gcc_inline pid_t
sys_getppid(void)
{
	int errno;
	pid_t pid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_getppid),
		       "b" (&pid)
		     : "cc", "memory");

	return errno ? -1 : pid;
}

static gcc_inline int
sys_getchid(void)
{
	int errno, chid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_getchid),
		       "b" (&chid)
		     : "cc", "memory");

	return errno ? -1 : chid;
}

static gcc_inline int
sys_send(int chid, void *buf, size_t size)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_send),
		       "b" (chid),
		       "c" (buf),
		       "d" (size)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_recv(int chid, void *buf, size_t size)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_recv),
		       "b" (chid),
		       "c" (buf),
		       "d" (size)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_disk_read(uint64_t lba, uint64_t nsectors, void *buf)
{
	int errno;
	struct user_disk_op dop = { .type = DISK_READ,
				    .lba_lo = lba & 0xffffffff,
				    .lba_hi = (lba >> 32) & 0xffffffff,
				    .n = nsectors, .buf = (uintptr_t) buf };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_disk_op),
		       "b" (&dop)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_disk_write(uint64_t lba, uint64_t nsectors, void *buf)
{
	int errno;
	struct user_disk_op dop = { .type = DISK_WRITE,
				    .lba_lo = lba & 0xffffffff,
				    .lba_hi = (lba >> 32) & 0xffffffff,
				    .n = nsectors, .buf = (uintptr_t) buf };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_disk_op),
		       "b" (&dop)
		     : "cc", "memory");

	return errno;
}

static gcc_inline uint64_t
sys_disk_capacity(void)
{
	int errno;
	uint32_t size_lo, size_hi;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_disk_cap),
		       "b" (&size_lo),
		       "c" (&size_hi)
		     : "cc", "memory");

	return errno ? 0 : ((uint64_t) size_hi << 32 | size_lo);
}

static gcc_inline chid_t
sys_channel(size_t msg_size)
{
	int errno;
	chid_t chid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_channel),
		       "b" (&chid),
		       "c" (msg_size)
		     : "cc", "memory");

	return errno ? -1 : chid;
}

static gcc_inline int
sys_sysinfo_lookup(sysinfo_name_t name, sysinfo_info_t *info)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_sysinfo_lookup),
		       "b" (name),
		       "c" (info)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_create_vm(void)
{
	int errno, vmid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_create_vm),
		       "b" (&vmid)
		     : "cc", "memory");

	return errno ? -1 : vmid;
}

static gcc_inline int
sys_hvm_run_vm(int vmid, exit_reason_t *reason, exit_info_t *info)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_run_vm),
		       "b" (vmid),
		       "c" (reason),
		       "d" (info)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_set_mmap(int vmid, uintptr_t gpa, uintptr_t hva, int type)
{
	int errno;
	struct user_hvm_mmap hvm_mmap = { .gpa = gpa, .hva = hva, .type = type};

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_set_mmap),
		       "b" (vmid),
		       "c" (&hvm_mmap)
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

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_get_reg),
		       "b" (vmid),
		       "c" (reg),
		       "d" (val)
		     : "cc", "memory");

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

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_get_next_eip),
		       "b" (vmid),
		       "c" (instr),
		       "d" (eip)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_inject_event(int vmid, guest_event_t type,
		     uint8_t vector, uint32_t errcode, bool ev)
{
	int errno;
	struct user_hvm_event event = { .type = type, .vector = vector,
					.errcode = errcode, .ev = ev };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_inject_event),
		       "b" (vmid),
		       "c" (&event)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_hvm_pending_event(int vmid)
{
	int errno, result;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_pending_event),
		       "b" (vmid),
		       "c" (&result)
		     : "cc", "memory");

	return errno ? -1 : result;
}

static gcc_inline int
sys_hvm_intr_shadow(int vmid)
{
	int errno, result;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_hvm_intr_shadow),
		       "b" (vmid),
		       "c" (&result)
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

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_read_ioport),
		       "b" (port),
		       "c" (width),
		       "d" (val)
		     : "cc", "memory");

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

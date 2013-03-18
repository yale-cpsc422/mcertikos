#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_

#include <sys/syscall.h>

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

static gcc_inline int
sys_getc(void)
{
	int c;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_getc),
		       "b" (&c)
		     : "cc", "memory");

	return c;
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
sys_run_proc(pid_t pid, uint32_t cpu_idx, uintptr_t exec)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_run_proc),
		       "b" (pid),
		       "c" (cpu_idx),
		       "d" (exec)
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
sys_attach_port(uint16_t port, data_sz_t width)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_attach_port),
		       "b" (port),
		       "c" (width)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_detach_port(uint16_t port, data_sz_t width)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_detach_port),
		       "b" (port),
		       "c" (width)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_attach_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_attach_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_detach_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_detach_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline uint32_t
sys_host_in(uint16_t port, data_sz_t width)
{
	int errno;
	uint32_t val;
	struct user_ioport portinfo = { .port = port, .width = width };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_host_in),
		       "b" (&portinfo),
		       "c" (&val)
		     : "cc", "memory");

	return errno ? 0xffffffff : val;
}

static gcc_inline int
sys_host_out(uint16_t port, data_sz_t width, uint32_t val)
{
	int errno;
	struct user_ioport portinfo = { .port = port, .width = width };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_host_out),
		       "b" (&portinfo),
		       "c" (val)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_set_irq(uint8_t irq, int mode)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_set_irq),
		       "b" (irq),
		       "c" (mode)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_copy_from_guest(void *dest, uintptr_t src_gpa, size_t sz)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_read),
		       "b" (src_gpa),
		       "c" ((uintptr_t) dest),
		       "d" (sz)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_copy_to_guest(uintptr_t dest_gpa, void *src, size_t sz)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_write),
		       "b" (dest_gpa),
		       "c" ((uintptr_t) src),
		       "d" (sz)
		     : "cc", "memory");

	return errno;
}

static gcc_inline uint64_t
sys_guest_tsc(void)
{
	int errno;
	uint64_t tsc;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_tsc),
		       "b" (&tsc)
		     : "cc", "memory");

	return errno ? 0 : tsc;
}

static gcc_inline uint64_t
sys_guest_cpufreq(void)
{
	int errno;
	uint64_t freq;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_cpufreq),
		       "b" (&freq)
		     : "cc", "memory");

	return errno ? 0 : freq;
}

static gcc_inline size_t
sys_guest_memsize(void)
{
	int errno;
	size_t size;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_memsize),
		       "b" (&size)
		     : "cc", "memory");

	return errno ? 0 : size;
}

static gcc_inline int
sys_disk_read(uint64_t lba, uint64_t nsectors, void *buf)
{
	int errno;
	struct user_disk_op dop = { .type = DISK_READ, .lba = lba,
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
	struct user_disk_op dop = { .type = DISK_WRITE, .lba = lba,
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

static gcc_inline chid_t
sys_get_inchan(void)
{
	int errno;
	chid_t chid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_get_inchan),
		       "b" (&chid)
		     : "cc", "memory");

	return errno ? -1 : chid;
}

static gcc_inline chid_t
sys_get_outchan(void)
{
	int errno;
	chid_t chid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_get_outchan),
		       "b" (&chid)
		     : "cc", "memory");

	return errno ? -1 : chid;
}

#endif /* !_USER_SYSCALL_H_ */

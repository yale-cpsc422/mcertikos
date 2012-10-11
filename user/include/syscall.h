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
sys_spawn(uint32_t cpu_idx, uintptr_t exe)
{
	int errno;
	pid_t pid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_spawn),
		       "b" (cpu_idx),
		       "c" (exe),
		       "d" (&pid)
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

static gcc_inline sid_t
sys_session(int type)
{
	int errno;
	sid_t sid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_session),
		       "b" (type),
		       "c" (&sid)
		     : "cc", "memory");

	return errno ? -1 : sid;
}

static gcc_inline sid_t
sys_getsid(void)
{
	int errno;
	sid_t sid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_getsid),
		       "b" (&sid)
		     : "cc", "memory");

	return errno ? -1 : sid;
}

static gcc_inline vmid_t
sys_newvm(void)
{
	int errno;
	vmid_t vmid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_new_vm),
		       "b" (&vmid)
		     : "cc", "memory");

	return errno ? -1 : vmid;
}

static gcc_inline int
sys_runvm(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_run_vm)
		     : "cc", "memory");

	return errno;
}

static gcc_inline vid_t
sys_attach_vdev(uint32_t cpu_idx, int exe)
{
	int errno;
	vid_t vid;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_attach_vdev),
		       "b" (cpu_idx),
		       "c" (exe),
		       "d" (&vid)
		     : "cc", "memory");

	return errno ? -1 : vid;
}

static gcc_inline int
sys_detach_vdev(vid_t vid)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_detach_vdev)
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
sys_ret_in(uint16_t port, data_sz_t width, uint32_t val)
{
	int errno;
	struct user_ioport portinfo = { .port = port, .width = width };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_ret_in),
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

static gcc_inline int
sys_send_ready(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_send_ready)
		     : "cc", "memory");

	return errno;
}

static gcc_inline uint64_t
sys_guest_rdtsc(void)
{
	int errno;
	uint64_t tsc;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_rdtsc),
		       "b" (&tsc)
		     : "cc", "memory");

	return errno ? 0 : tsc;
}

static gcc_inline uint64_t
sys_guest_tsc_freq(void)
{
	int errno;
	uint64_t freq;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_tsc_freq),
		       "b" (&freq)
		     : "cc", "memory");

	return errno ? 0 : freq;
}

static gcc_inline size_t
sys_guest_mem_size(void)
{
	int errno;
	size_t size;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_mem_size),
		       "b" (&size)
		     : "cc", "memory");

	return errno ? 0 : size;
}

static gcc_inline int
sys_recv_req(dev_req_t *req, bool blocking)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_recv_req),
		       "b" (req),
		       "c" (blocking)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_guest_disk_read(uintptr_t gpa, uint64_t lba, size_t nsectors)
{
	int errno;
	struct user_disk_op dop =
		{ .type = DISK_READ, .lba = lba, .n = nsectors, .gpa = gpa };

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_disk_op),
		       "b" (&dop)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_guest_disk_write(uint64_t lba, uintptr_t gpa, size_t nsectors)
{
	int errno;
	struct user_disk_op dop =
		{ .type = DISK_WRITE, .lba = lba, .n = nsectors, .gpa = gpa};

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_disk_op),
		       "b" (&dop)
		     : "cc", "memory");

	return errno;
}

static gcc_inline uint64_t
sys_guest_disk_capacity(void)
{
	int errno;
	uint32_t size_lo, size_hi;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_disk_cap),
		       "b" (&size_lo),
		       "c" (&size_hi)
		     : "cc", "memory");

	return errno ? 0 : ((uint64_t) size_hi << 32 | size_lo);
}

#endif /* !_USER_SYSCALL_H_ */

#ifndef _USER_SYSCALL_H_
#define _USER_SYSCALL_H_

#include <sys/syscall.h>

#include <gcc.h>
#include <proc.h>
#include <types.h>

static gcc_inline void
sys_test(uint32_t a)
{
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_test),
		       "b" (a)
		     : "cc", "memory");
}

static gcc_inline void
sys_puts(const char *s)
{
#if 1
	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_puts),
		       "b" (s)
		     : "cc", "memory");
#endif
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
	pid_t pid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_spawn),
		       "b" (cpu_idx),
		       "c" (exe),
		       "d" (&pid)
		     : "cc", "memory");

	return pid;
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
	pid_t pid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_getpid),
		       "b" (&pid)
		     : "cc", "memory");

	return pid;
}

static gcc_inline int
sys_allocvm(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_allocvm)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_execvm(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_execvm)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_ncpus(void)
{
	int n;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_ncpus),
		       "b" (&n)
		     : "cc", "memory");

	return n;
}

static gcc_inline int
sys_getpchid(void)
{
	int chid;

	asm volatile("int %0" :
		     : "i" (T_SYSCALL),
		       "a" (SYS_getpchid),
		       "b" (&chid)
		     : "cc", "memory");

	return chid;
}

static gcc_inline int
sys_send(int channel_id, void *msg, size_t size)
{
	int errno;

	if (channel_id < 0 || msg == NULL || size == 0)
		return -1;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_send),
		       "b" (channel_id),
		       "c" (msg),
		       "d" (size)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_recv(int channel_id, void *msg, size_t *size, bool blocking)
{
	int errno;

	if (channel_id < 0 || msg == NULL || size == NULL)
		return -1;

	if (blocking == TRUE)
		asm volatile("int %1"
			     : "=a" (errno)
			     : "i" (T_SYSCALL),
			       "a" (SYS_recv),
			       "b" (channel_id),
			       "c" (msg),
			       "d" (size)
			     : "cc", "memory");
	else
		asm volatile("int %1"
			     : "=a" (errno)
			     : "i" (T_SYSCALL),
			       "a" (SYS_recv_nonblock),
			       "b" (channel_id),
			       "c" (msg),
			       "d" (size)
			     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_register_ioport(uint16_t port, data_sz_t width, int rw)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_register_ioport),
		       "b" (port),
		       "c" (width),
		       "d" (rw)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_unregister_ioport(uint16_t port, data_sz_t width, int rw)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_unregister_ioport),
		       "b" (port),
		       "c" (width),
		       "d" (rw)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_register_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_register_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_unregister_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_unregister_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_register_pic(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_register_pic)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_unregister_pic(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_unregister_pic)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_register_mmio(uintptr_t gpa, uintptr_t hla, size_t size)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_register_mmio),
		       "b" (gpa),
		       "c" (hla),
		       "d" (size)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_unregister_mmio(uintptr_t gpa, size_t size)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_unregister_mmio),
		       "b" (gpa),
		       "c" (size)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_read_ioport(uint16_t port, data_sz_t width, void *data)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_read_ioport),
		       "b" (port),
		       "c" (width),
		       "d" (data)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_write_ioport(uint16_t port, data_sz_t width, uint32_t data)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_write_ioport),
		       "b" (port),
		       "c" (width),
		       "d" (&data)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_raise_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_raise_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_trigger_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_trigger_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_lower_irq(uint8_t irq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_lower_irq),
		       "b" (irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_notify_irq(void)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_notify_irq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_read_guest_tsc(uint64_t *tsc)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_read_guest_tsc),
		       "b" (tsc)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_guest_tsc_freq(uint64_t *freq)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_tsc_freq),
		       "b" (freq)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_guest_mem_size(uint64_t *memsize)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_guest_mem_size),
		       "b" (memsize)
		     : "cc", "memory");

	return errno;
}

static gcc_inline int
sys_getchid(pid_t pid, int *chid)
{
	int errno;

	asm volatile("int %1"
		     : "=a" (errno)
		     : "i" (T_SYSCALL),
		       "a" (SYS_getchid),
		       "b" (pid),
		       "c" (chid)
		     : "cc", "memory");

	return errno;
}

#endif /* !_USER_SYSCALL_H_ */

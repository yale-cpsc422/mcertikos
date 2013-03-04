#ifndef _KERN_X86_H_
#define _KERN_X86_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>

#include <machine/x86.h>

#ifndef __COMPCERT__

static gcc_inline uint8_t
inb(int port)
{
	uint8_t data;
	__asm __volatile("inb %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static gcc_inline void
insb(int port, void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\tinsb"                 :
			 "=D" (addr), "=c" (cnt)                :
			 "d" (port), "0" (addr), "1" (cnt)      :
			 "memory", "cc");
}

static gcc_inline uint16_t
inw(int port)
{
	uint16_t data;
	__asm __volatile("inw %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static gcc_inline void
insw(int port, void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\tinsw"                 :
			 "=D" (addr), "=c" (cnt)                :
			 "d" (port), "0" (addr), "1" (cnt)      :
			 "memory", "cc");
}

static gcc_inline uint32_t
inl(int port)
{
	uint32_t data;
	__asm __volatile("inl %w1,%0" : "=a" (data) : "d" (port));
	return data;
}

static gcc_inline void
insl(int port, void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\tinsl"                 :
			 "=D" (addr), "=c" (cnt)                :
			 "d" (port), "0" (addr), "1" (cnt)      :
			 "memory", "cc");
}

static gcc_inline void
outb(int port, uint8_t data)
{
	__asm __volatile("outb %0,%w1" : : "a" (data), "d" (port));
}

static gcc_inline void
outsb(int port, const void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\toutsb"                :
			 "=S" (addr), "=c" (cnt)                :
			 "d" (port), "0" (addr), "1" (cnt)      :
			 "cc");
}

static gcc_inline void
outw(int port, uint16_t data)
{
	__asm __volatile("outw %0,%w1" : : "a" (data), "d" (port));
}

static gcc_inline void
outsw(int port, const void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\toutsw"                :
			 "=S" (addr), "=c" (cnt)                :
			 "d" (port), "0" (addr), "1" (cnt)      :
			 "cc");
}

static gcc_inline void
outsl(int port, const void *addr, int cnt)
{
	__asm __volatile("cld\n\trepne\n\toutsl"                :
			 "=S" (addr), "=c" (cnt)                :
			 "d" (port), "0" (addr), "1" (cnt)      :
			 "cc");
}

static gcc_inline void
outl(int port, uint32_t data)
{
	__asm __volatile("outl %0,%w1" : : "a" (data), "d" (port));
}

static gcc_inline void
halt(void)
{
	__asm __volatile("hlt");
}

static gcc_inline void
pause(void)
{
	__asm __volatile("pause":::"memory");
}

static gcc_inline uint32_t
xchg(volatile uint32_t *addr, uint32_t newval)
{
	uint32_t result;

	__asm __volatile("lock; xchgl %0, %1" :
			 "+m" (*addr), "=a" (result) :
			 "1" (newval) :
			 "cc");

	return result;
}

static gcc_inline uint64_t
rdtsc(void)
{
	uint64_t rv;

	__asm __volatile("rdtsc" : "=A" (rv));
	return (rv);
}

static gcc_inline uint64_t
rdtscp(void)
{
	uint64_t rv;

	/* TODO: decide to use which one through checking cpuid */
#if 0
	/* rdtscp maybe not provided by VirtualBox */
	__asm __volatile("rdtscp" : "=A" (rv));
#else
	__asm __volatile("lfence;rdtsc" : "=A" (rv));
#endif
	return (rv);
}

static gcc_inline void
smp_wmb(void)
{
	__asm __volatile("":::"memory");
}

static gcc_inline void
smp_rmb(void)
{
	__asm __volatile("":::"memory");
}

#else /* !__COMPCERT__ */

void ccomp_rdtscp(uint64_t *tsc);

#endif /* __COMPCERT__ */

#endif /* _KERN_ */

#endif /* !_KERN_X86_H_ */

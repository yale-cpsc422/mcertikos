//minimal mmap/sbrk

#include "mman.h"
#include "mem.h"
#include <kern/debug/stdio.h>

//#include <inc/stdio.h>
//#include <inc/mmu.h>
//#include <inc/syscall.h>

static char * mem_brk;

void * sbrk(intptr_t increment) {
	extern char end[];
	char * m, * e;
	e = end;
	e = ROUNDUP(e, PAGE_SIZE);
	if (mem_brk < e)
		mem_brk = e;
	m = mem_brk;
	increment = ROUNDUP(increment, PAGE_SIZE);
	mem_brk += increment;
	 cprintf("%p %p %d\n", m, mem_brk, increment);
	//sys_get(SYS_PERM | SYS_RW, 0, NULL, NULL, m, increment);
	return m;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	return sbrk(length);
}

int munmap(void *addr, size_t length)
{
	cprintf("unmap addr $x, len %x\n", addr, length);
	return 0;
}

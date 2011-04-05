#ifndef PIOS_ARCH_MEM_H
#define PIOS_ARCH_MEM_H

#include <inc/arch/types.h>


// returns the sizes of physical memory
size_t mem_base(void);
size_t mem_ext(void);

// Memory chunk operations
void *	memset(void *dst, int c, size_t len);
void *	memcpy(void *dst, const void *src, size_t len);
void *	memmove(void *dst, const void *src, size_t len);
int	memcmp(const void *s1, const void *s2, size_t len);
void *	memfind(const void *s, int c, size_t len);


// Given a physical address,
// return a C pointer the kernel can use to access it.
// This macro does nothing in PIOS because physical memory
// is mapped into the kernel's virtual address space at address 0,
// but this is not the case for many other systems such as JOS or Linux,
// which must do some translation here (usually just adding an offset).
#define mem_ptr(physaddr)	((void*)(physaddr))

// The converse to the above: given a C pointer, return a physical address.
#define mem_phys(ptr)		((uint32_t)(ptr))

#endif /* not PIOS_ARCH_MEM_H */

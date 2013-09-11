#ifndef _LIB_EXPORT_H_
#define _LIB_EXPORT_H_

#ifdef _KERN_

#include "mboot.h"

/*
 * Types.
 */

#include "types.h"

/*
 * GCC.
 */

#include "gcc.h"

/*
 * Segments.
 */

#include "seg.h"

/*
 * String functions.
 */

int  strncmp(const char *p, const char *q, size_t n);
void *memset(void *dst, int c, size_t len);
void *memzero(void *dst, size_t len);
void *memcpy(void *dst, const void *src, size_t n);

/*
 * Wrapers of x86 instructions.
 */

void lcr3(uint32_t val);
uint32_t rcr2(void);
uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);
void outb(uint16_t port, uint8_t v);
void outw(uint16_t port, uint16_t v);
void outl(uint16_t port, uint32_t v);
void enable_sse(void);

/*
 * Trap numbers and trap_return().
 */

#include "trap.h"

/*
 * mboot
 */

void pmmap_init(uintptr_t mbi_addr);
int pmmap_entries_nr(void);
uint32_t pmmap_entry_start(int idx);
uint32_t pmmap_entry_length(int idx);
int pmmap_entry_usable(int idx);

#endif /* _KERN_ */

#endif /* !_LIB_EXPORT_H_ */

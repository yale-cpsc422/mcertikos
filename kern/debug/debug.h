// Kernel debugging code.
// See COPYRIGHT for copyright information.

#ifndef PIOS_KERN_DEBUG_H_
#define PIOS_KERN_DEBUG_H_

#include <architecture/types.h>
#include <inc/gcc.h>


#define DEBUG_TRACEFRAMES	10

void debug_init();
void debug_info(const char *, ...);
void debug_normal(const char *, int, const char *, ...);
void debug_warn(const char*, int, const char*, ...);
void debug_panic(const char*, int, const char*, ...) gcc_noreturn;

#define info(...)	debug_info(__VA_ARGS__)
#define debug(...)	debug_normal(__FILE__, __LINE__, __VA_ARGS__)
#define warn(...)	debug_warn(__FILE__, __LINE__, __VA_ARGS__)
#define panic(...)	debug_panic(__FILE__, __LINE__, __VA_ARGS__)

#define assert(x)		\
	do { if (!(x)) panic("assertion failed: %s", #x); } while (0)

// static_assert(x) will generate a compile-time error if 'x' is false.
#define static_assert(x)	switch (x) case 0: case (x):


void debug_trace(uint32_t ebp, uint32_t eips[DEBUG_TRACEFRAMES]);
void debug_check(void);

#endif /* PIOS_KERN_DEBUG_H_ */

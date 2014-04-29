#ifndef _VIRT_VMX_H_
#define _VIRT_VMX_H_

#ifdef _KERN_

#include <preinit/lib/gcc.h>
#include <preinit/lib/types.h>
#include <preinit/lib/x86.h>
#include <preinit/lib/string.h>

#include "vmcs.h"

struct vmx {
	/*
	 * VMCS does not store following registers for guest, so we have
	 * to do that by ourself.
	 */
	uint64_t	g_rax, g_rbx, g_rcx, g_rdx, g_rsi, g_rdi, g_rbp, g_rip;
	uint32_t	g_cr2;
	uint32_t	g_dr0, g_dr1, g_dr2, g_dr3, g_dr6;
	uint32_t	enter_tsc[2], exit_tsc[2];

	struct vmcs	*vmcs;

	uint16_t	vpid;
	uint64_t	*pml4ept;
	char		*msr_bitmap, *io_bitmap;

	uint32_t	exit_reason;
	uint64_t	exit_qualification;
	int32_t		pending_intr;

	int		launched;
	int		failed;
};

enum {
        EXIT_NONE = 0,      /* no VMEXIT */
        EXIT_FOR_EXCEPTION,     /* exit for the exception*/
        EXIT_FOR_EXTINT,    /* exit for the external interrupt */
        EXIT_FOR_INTWIN,    /* exit for the interrupt window */
        EXIT_FOR_IOPORT,    /* exit for accessing an I/O port */
        EXIT_FOR_PGFLT,     /* exit for the page fault */
        EXIT_FOR_RDMSR,     /* exit for the rdmsr instruction */
        EXIT_FOR_WRMSR,     /* exit for the wrmsr instruction */
        EXIT_FOR_CPUID,     /* exit for the cpuid instruction */
        EXIT_FOR_RDTSC,     /* exit for the rdtsc/rdtscp instruction */
        EXIT_FOR_HYPERCALL, /* exit for the hypercall */
        EXIT_FOR_INVAL_INSTR,   /* exit for the invalid instruction */
        EXIT_INVAL      /* invalid exit */
};

#define offsetof(type, member)  __builtin_offsetof(type, member)

#ifdef DEBUG_VMX

#define VMX_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("VMX: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define VMX_DEBUG(fmt, ...)			\
	do {					\
	} while (0)

#endif

#endif /* _KERN_ */

#endif /* !_VIRT_VMX_H_ */

#ifndef _VIRT_VMX_H_
#define _VIRT_VMX_H_

#ifdef _KERN_

#include <sys/gcc.h>
#include <sys/types.h>

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

#ifdef DEBUG_SWITCH_COST
        int64_t         exit_counter;
        uint64_t        host_tsc_total;
        uint64_t        guest_tsc_total;
        uint64_t        last_exit_tsc;
#endif
};

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

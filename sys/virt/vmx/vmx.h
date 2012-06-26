#ifndef _VIRT_VMX_H_
#define _VIRT_VMX_H_

#ifdef _KERN_

#include <sys/types.h>

#include "vmcs.h"

struct vmx {
	/*
	 * VMCS does not store following registers for guest, so we have
	 * to do that by ourself.
	 */
	uint64_t	g_rbx, g_rcx, g_rdx, g_rsi, g_rdi, g_rbp;
	uint64_t	enter_tsc, exit_tsc;

	struct vmcs	*vmcs;
};

#endif /* _KERN_ */

#endif /* !_VIRT_VMX_H_ */

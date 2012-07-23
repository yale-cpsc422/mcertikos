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
	uint64_t	g_rax, g_rbx, g_rcx, g_rdx, g_rsi, g_rdi, g_rbp;
	uint32_t	g_cr2;
	uint32_t	g_dr0, g_dr1, g_dr2, g_dr3, g_dr6;
	uint32_t	enter_tsc[2], exit_tsc[2];

	struct vmcs	*vmcs;

	uint16_t	vpid;
	uint64_t	*pml4ept;
	char		*msr_bitmap, *ioio_bitmap;

	int		launched;
	int		failed;
};

#endif /* _KERN_ */

#endif /* !_VIRT_VMX_H_ */

#ifndef _KERN_HVM_INTERNAL_H_
#define _KERN_HVM_INTERNAL_H_

#ifdef _KERN_

#define MAX_VMID	1

#include <virt/hvm.h>

struct vm {
	int		vmid;		/* identifier of the virtual machine */
	int		inuse;		/* in use? */

	exit_reason_t	exit_reason;	/* the reason of the latest VMEXIT */
	exit_info_t	exit_info;	/* the information of the latest VMEXIT */

	void		*cookie;
};

static struct vm	vm0;

#endif /* _KERN_ */

#endif /* !_KERN_HVM_INTERNAL_H_ */

#ifndef _HVM_VMM_H_
#define _HVM_VMM_H_

#include <architecture/types.h>

#include <kern/hvm/dev/kbd.h>
#include <kern/hvm/dev/pci.h>
#include <kern/hvm/dev/pic.h>

#define VM_PHY_MEMORY_SIZE	(64 * 1024 * 1024)

struct vm {
	void		*cookie;	/* arch-specific data (VMCB or VMCS) */

	bool		exit_for_intr;	/* Did an interrupt happened in VM? */

	struct vpic	vpic;		/* virtual PIC (i8259) */
	struct vpci	vpci;		/* virtual PCI host */
	struct vkbd	vkbd;		/* virtual keyboard */
};

/* functions that initialize VMM */
typedef int	(*vmm_init_func_t)(void);
/* functions that initialize a VM */
typedef int	(*vm_init_func_t)(struct vm *);
/* functions that run a VM */
typedef int	(*vm_run_func_t)(struct vm *);
/* functions that handle the exits of VM */
typedef int	(*vm_handle_func_t)(struct vm *);

/*
 * Each implementation of a specific hardware virtualization should provide such
 * a structure.
 */
struct vmm_ops {
	vmm_init_func_t		vmm_init;

	vm_init_func_t		vm_init;
	vm_run_func_t		vm_run;
	vm_handle_func_t	vm_handle_exit;
};

int vmm_init(void);
int vmm_init_vm(struct vm *);
int vmm_run_vm(struct vm *);

struct vm *vmm_cur_vm(void);

#endif /* !_HVM_VMM_H_ */

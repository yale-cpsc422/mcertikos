#ifndef _SYS_VIRT_VMM_H_
#define _SYS_VIRT_VMM_H_

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/dev/i8259.h>
#include <sys/virt/dev/kbd.h>
#include <sys/virt/dev/pci.h>

#define VM_PHY_MEMORY_SIZE	(64 * 1024 * 1024)

struct vm {
	void		*cookie;	/* processor-specific data */

	bool		exit_for_intr;	/**/

	struct vpic	vpic;		/* virtual PIC (i8259) */
	struct vpci	vpci;		/* virtual PCI host */
	struct vkbd	vkbd;		/* virtual keyboard */
};

/*
 * Machine-dependent VMM initialization function.
 *
 * @return 0 if no errors happen
 */
typedef int (*vmm_init_func_t)(void);

/*
 * Machine-dependent VM initialization function.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_init_func_t)(struct vm *);

/*
 * Machine-dependent function that start running a VM.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_run_func_t)(struct vm *);

/*
 * Machine-dependent function that handle the interrupt the execution of VM.
 *
 * @return 0 if no errors happen
 */
typedef int (*vm_handle_func_t)(struct vm *);

/*
 * Each machine-dependent HVM implementation should define such a structure.
 */
struct vmm_ops {
	vmm_init_func_t		vmm_init;

	vm_init_func_t		vm_init;
	vm_run_func_t		vm_run;
	vm_handle_func_t	vm_handle;
};

/*
 * Top-level VMM initialization function.
 */
int vmm_init(void);

/*
 * Top-level VM initialization function.
 */
int vmm_init_vm(struct vm *);

/*
 * Top-level function launch a VM.
 */
int vmm_run_vm(struct vm *);

/*
 * Get the current VM.
 */
struct vm *vmm_cur_vm(void);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_VMM_H_ */

#ifndef __VM_H__
#define __VM_H__

#include <inc/multiboot.h>
#include "vmcb.h"
#include <kern/mem/e820.h>

#define	GUEST_PADDR_MBI	0x2d0e0UL

#define VM_MAX_CUR_OPENFILE		(PAGE_SIZE_4KB / sizeof (fid2name_map))
#define VM_MAX_FILENAME 			20

#define VM_MAX_PNAME_LEN			20
#define VM_MAX_PROCESS_TRACKED		(PAGE_SIZE_4KB / MAX_PNAME_LEN)

struct vm_info
{
	struct vmcb *vmcb;

	unsigned long n_cr3;  /* [Note] When #VMEXIT occurs with
						   * nested paging enabled, hCR3 is not
						   * saved back into the VMCB (vol2 p. 409)???*/

	char waitingRetSysCall;

	uint64_t org_sysenter_cs;	//original sysenter msrs, used when syscall interception is enabled

	int itc_flag; 			//flags specifying which interceptions were
							//registered for this vm (see user.h)
	int itc_skip_flag;

	//mapping from id of files opened by this VM to their names
	struct fid2name_map * fmap;
	int nOpenFile;

	struct tid2syscall_map * syscallmap;
	int nWaitingThreads;

	//list of process names to be tracked
	int nTrackedProcess;
	char * ptracked;
	uint8_t btrackcurrent;	//whether the current process is being tracked or not
};

extern void vm_enable_intercept(struct vm_info * vm, int flags);
extern void vm_disable_intercept(struct vm_info *vm, int flags);

extern void vm_create ( struct vm_info *vm, unsigned long vmm_pmem_start,
		unsigned long vmm_pmem_size, struct e820_map *e820);
extern void vm_create_simple(struct vm_info *vm);
extern void vm_create_simple_with_interceptionxtern (struct vm_info *vm);

extern void vm_create_guest_pios(struct vm_info *vm);
extern void vm_boot (struct vm_info *vm);
extern void print_page_errorcode(uint64_t errcode);

extern void start_vm_1( struct vmcb *vmcb1 );
extern void start_vm();
extern void start_vm_with_interception();
extern void  run_vm_once(struct vm_info *vm );
extern uint32_t create_vm();
#endif /* __VM_H__ */

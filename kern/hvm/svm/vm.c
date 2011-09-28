/********************************************************************************************
*
* Derived from  XEN and MAVMM
* Adapted for CertiKOS by Liang Gu and Yale University
*
* This  module provides opreations for Hardware-based Virtual Machine
*/

#include "svm.h"
#include "vm.h"
#include <architecture/cpufeature.h>
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/mem/mem.h>
#include <kern/as/as.h>
#include <architecture/types.h>
#include <architecture/pic.h>
#include <architecture/mp.h>
#include <architecture/mmu.h>
#include <architecture/x86.h>
#include <architecture/mem.h>
#include "vmexit.h"
#include "intercept.h"


static struct vmcb * alloc_vmcb ( void )
{
	struct vmcb *vmcb;

//	const unsigned long pfn = alloc_host_pages (1, 1);
	const unsigned long pfn  = mem_alloc_one_page(); 
	//cprintf("Free page for vmcb: %x\n", pfn);
	vmcb = (struct vmcb *) (pfn << PAGE_SHIFT);
	memset (( char *) vmcb, 0, sizeof (struct vmcb));

	return vmcb;
}

static unsigned long create_intercept_table ( unsigned long size )
{
	const unsigned long pfn = alloc_host_pages ( size >> PAGE_SHIFT, 1 );
	//const unsigned long pfn = find_contiguous_pages(size >> PAGE_SHIFT);
	void *p = ( void * ) ( pfn << PAGE_SHIFT );
	cprintf("allocated page number: %x\n",pfn);
	/* vol. 2, p. 445 */
	//memset ( p, 0xff, size );
	memset ( p, 0x00, size );

	return pfn << PAGE_SHIFT;
}

static void set_control_params (struct vmcb *vmcb)
{
	//Note: anything not set will be 0 (since vmcb was filled with 0)

	/****************************** SVM CONFIGURATION *****************************/
	/******************************************************************************/
	/* Enable/disable nested paging (See AMD64 manual Vol. 2, p. 409) */
	vmcb->np_enable = 1;
	cprintf ("Nested paging enabled.\n");

	// set this to 1 will make VMRUN to flush all TBL entries, regardless of ASID
	// and global / non global property of pages
    vmcb->tlb_control = 0;

    //time stamp counter offset, to be added to guest RDTSC and RDTSCP instructions
	/* To be added in RDTSC and RDTSCP */
	vmcb->tsc_offset = 0;

	//Guest address space identifier (ASID), must <> 0 - vol2 373
	vmcb->guest_asid = 1;
	cprintf("vmcb->guset_asid@%x =%x\n",&vmcb->guest_asid,vmcb->guest_asid);

	/* Intercept the VMRUN and VMMCALL instructions */
	//must intercept VMRUN at least vol2 373
	vmcb->general2_intercepts = (INTRCPT_VMRUN | INTRCPT_VMMCALL);

	//allocating a region for IOPM (permission map)
	//and fill it with 0x00 (not intercepting anything)
	vmcb->iopm_base_pa  = create_intercept_table ( 12 << 10 ); /* 12 Kbytes */

	//allocating a region for msr intercept table, and fill it with 0x00
	vmcb->msrpm_base_pa = create_intercept_table ( 8 << 10 );  /* 8 Kbytes */

	/********** WHAT TO INTERCEPT *********/
	//Note: start without any interception. Specific interception will be enabled
	// by user program when appropriate.

//	vmcb->general1_intercepts |= INTRCPT_INTN;
}

/********************************************************************************************/

static unsigned long create_4kb_nested_pagetable ( )
{
	//const unsigned long cr3  = pg_table_alloc();
//	const unsigned long cr3  = as_new();

	as_t *  pmap=as_new_vm();

	//const unsigned long vm_pmem_pfn = PFN_DOWN_2MB ( PHYS ( vm_pmem_start ) );
	int i;
	cprintf("as_new:%x\n",pmap);


	for(i=0x100000;i<(GUEST_FIXED_PMEM_BYTES+0x100000);i=i+PAGE_SIZE){
		
		as_reserve(pmap,i,PTE_W|PTE_U|PTE_G);
	}	

	cprintf( "Nested page table created.\n" );

//	print_4MB_pg_table(cr3);

	return (unsigned long) pmap;
}

/********************************************************************************************/
/******************************* NESTED PAGING PROTECTION ***********************************/
/********************************************************************************************/


void __vm_protect_all_nonPAE_page(uint32_t cr3)
{
	int index;
	unsigned long all4gb = 1 << 10;
	for ( index = 0; index < all4gb; index++ )
	{
		int * entry = (int *) (cr3 + index * 4);
		*entry &= 0xFFFFFFFF - PTTEF_RW;
	}
}
void vm_protect_all_nonPAE_nestedpage(struct vm_info * vm)
{
	cprintf("protect all pages\n");
	__vm_protect_all_nonPAE_page(vm->n_cr3);
}

void vm_protect_all_nonPAE_guestpage(struct vm_info * vm)
{
	cprintf("protect all GUEST pages\n");
	__vm_protect_all_nonPAE_page(vm->vmcb->cr3);
}

void __vm_unprotect_nonPAE_page(uint32_t cr3)
{
	int index;

	unsigned long all4gb = 1 << 10;
	for ( index = 0; index < all4gb; index++ )
	{
		int * entry = (int *) (cr3 + index * 4);
		*entry |= PTTEF_RW;
	}
}

void vm_unprotect_all_nonPAE_nestedpage(struct vm_info * vm)
{
	cprintf("unprotect all pages\n");
	__vm_unprotect_nonPAE_page(vm->n_cr3);
}

void vm_unprotect_all_nonPAE_guestpage(struct vm_info * vm)
{
	cprintf("unprotect all GUEST pages\n");
	__vm_unprotect_nonPAE_page(vm->vmcb->cr3);
}

/*****************************************************************************************/
/************************ HANDLING VM INTERCEPTS ***************************************/
/*****************************************************************************************/

void vm_disable_intercept(struct vm_info *vm, int flags)
{
//	cprintf("vm_disable_intercept - %x\n", flags);

	if (flags & USER_UNPACK) {
//		vm->vmcb->rflags &= ~X86_RFLAGS_TF;
//		vm->vmcb->exception_intercepts &= ~INTRCPT_DB;
		vm->vmcb->exception_intercepts &= ~INTRCPT_PF;
		vm_unprotect_all_nonPAE_guestpage(vm);
	}

	//disable taskswitch interception
	if (flags & USER_ITC_TASKSWITCH) {
//		cprintf("Disable taskswitch interception\n");
		vm->vmcb->cr_intercepts &= ~INTRCPT_WRITE_CR3;
	}

	if (flags & USER_ITC_SWINT) {
//		cprintf("Disable software interrupt interception\n");
		vm->vmcb->general1_intercepts &= ~INTRCPT_INTN;
	}

	if (flags & USER_ITC_IRET) {
//		cprintf("Enable software interrupt interception\n");
		vm->vmcb->general1_intercepts &= ~INTRCPT_IRET;
	}

	if (flags & USER_ITC_SYSCALL) {
//		cprintf("Disable syscall interception\n");
		// **************** int 80h **************************
		vm->vmcb->general1_intercepts &= ~INTRCPT_INTN;

		// **************** sysenter **************************
//		vm->vmcb->general1_intercepts &= ~INTRCPT_MSR;
//
//		// Disable R/W interception for sysenter_cs, sysenter_esp and sysenter_eip
//		// each register <=> 2 bits
//		// flags for 3 consecutive registers = 1111 1100 b = 0xFC
//		uint8_t * sysenter_msrs = vm->vmcb->msrpm_base_pa + MSR_SYSENTER_CS / 4;
//		*sysenter_msrs &= ~0x3;
//
//		vm->vmcb->sysenter_cs = vm->org_sysenter_cs;
////		vm->vmcb->sysenter_esp = vm->org_sysenter_esp;
////		vm->vmcb->sysenter_eip = vm->org_sysenter_eip;
//
//		//Disable interception of that fault
//		vm->vmcb->exception_intercepts &= ~INTRCPT_GP;
	}

	// disable single stepping
	if (flags & USER_SINGLE_STEPPING) {
//		cprintf("Disable single stepping\n");
		vm->vmcb->rflags &= ~X86_RFLAGS_TF;
		vm->vmcb->exception_intercepts &= ~INTRCPT_DB;
	}
}

void vm_enable_intercept(struct vm_info * vm, int flags)
{
//	cprintf("vm_enable_intercept - %x\n", flags);

	if (flags & USER_UNPACK) {
//		vm->vmcb->rflags |= X86_RFLAGS_TF;
//		vm->vmcb->exception_intercepts |= INTRCPT_DB;
		vm->vmcb->exception_intercepts |= INTRCPT_PF;
		vm_protect_all_nonPAE_guestpage(vm);
	}

	//enable taskswitch interception
	if (flags & USER_ITC_TASKSWITCH) {
//		cprintf("Enable taskswitch interception\n");
		vm->vmcb->cr_intercepts |= INTRCPT_WRITE_CR3;

		//vm->vmcb->general1_intercepts |= INTRCPT_TASKSWITCH; <== does not work
		//vm->vmcb->general1_intercepts |= INTRCPT_READTR; <== does not work
	}

	//enable software interrupt interception
	if (flags & USER_ITC_SWINT) {
//		cprintf("Enable software interrupt interception\n");
		vm->vmcb->general1_intercepts |= INTRCPT_INTN;
	}

	if (flags & USER_ITC_IRET) {
//		cprintf("Enable software interrupt interception\n");
		vm->vmcb->general1_intercepts |= INTRCPT_IRET;
	}

	//enable syscall interception - both int80 and sys_enter
	if (flags & USER_ITC_SYSCALL) {
//		cprintf("Enable syscall interception\n");

		// **************** int 80h ***************************
		vm->vmcb->general1_intercepts |= INTRCPT_INTN;

		// **************** sysenter ***************************
//		vm->vmcb->general1_intercepts |= INTRCPT_MSR;
//
//		// Intercept R/W to sysenter_cs, sysenter_esp and sysenter_eip
//		// each register <=> 2 bits
//		// flags for 3 consecutive registers = 1111 1100 b = 0xFC
//		uint8_t * sysenter_msrs = vm->vmcb->msrpm_base_pa + MSR_SYSENTER_CS / 4;
//		*sysenter_msrs |= 0x3;
//
//		vm->org_sysenter_cs = vm->vmcb->sysenter_cs;
////		vm->org_sysenter_esp = vm->vmcb->sysenter_esp;
////		vm->org_sysenter_eip = vm->vmcb->sysenter_eip;
//
//		// Set vmcb's msr values so that syscall will create fault
//		vm->vmcb->sysenter_cs = SYSENTER_CS_FAULT;
////		vm->vmcb->sysenter_esp = SYSENTER_ESP_FAULT;
////		vm->vmcb->sysenter_eip = SYSENTER_EIP_FAULT;
//
//		//Enable interception of that fault
//		vm->vmcb->exception_intercepts |= INTRCPT_GP;	//general protection
//
//		//HeeDong - Set the bit in MSR Permission Map to intercept R/W to STAR MSR
////		u64 * star_msr_set_addr = vmcb->msrpm_base_pa + 0x820;
////		*star_msr_set_addr = *star_msr_set_addr | 0x04;
	}

	// enable single stepping
	if (flags & USER_SINGLE_STEPPING) {
//		cprintf("Enable single stepping\n");
		vm->vmcb->rflags |= X86_RFLAGS_TF;
		vm->vmcb->exception_intercepts |= INTRCPT_DB;
	}
}


/********************************************************************************************/
/********************************* INITIALIZE VM STATE *************************************/
/********************************************************************************************/

//- Test, to set initial state of VM to state of machine right after power up
void set_vm_to_powerup_state(struct vmcb * vmcb)
{
	// vol 2 p350
	memset(vmcb, 0, sizeof(vmcb));

	vmcb->cr0 = 0x0000000060000010;
	vmcb->cr2 = 0;
	vmcb->cr3 = 0;
	vmcb->cr4 = 0;
	vmcb->rflags = 0x2;
	vmcb->efer = EFER_SVME; // exception

	vmcb->rip = 0xFFF0;
	vmcb->cs.sel = 0xF000;
	vmcb->cs.base = 0xFFFF0000;
	vmcb->cs.limit = 0xFFFF;

	vmcb->ds.sel = 0;
	vmcb->ds.limit = 0xFFFF;
	vmcb->es.sel = 0;
	vmcb->es.limit = 0xFFFF;
	vmcb->fs.sel = 0;
	vmcb->fs.limit = 0xFFFF;
	vmcb->gs.sel = 0;
	vmcb->gs.limit = 0xFFFF;
	vmcb->ss.sel = 0;
	vmcb->ss.limit = 0xFFFF;

	vmcb->gdtr.base = 0;
	vmcb->gdtr.limit = 0xFFFF;
	vmcb->idtr.base = 0;
	vmcb->idtr.limit = 0xFFFF;

	vmcb->ldtr.sel = 0;
	vmcb->ldtr.base = 0;
	vmcb->ldtr.limit = 0xFFFF;
	vmcb->tr.sel = 0;
	vmcb->tr.base = 0;
	vmcb->tr.limit = 0xFFFF;

//	vmcb->rdx = model info;
//	vmcb->cr8 = 0;
}

void set_vm_to_pios_state(struct vmcb * vmcb)
{
	// vol 2 p350
	memset(vmcb, 0, sizeof(vmcb));

	//vmcb->cr0 = 0x0000000060000011;
	vmcb->cr0 = X86_CR0_PE | X86_CR0_ET;
	vmcb->cr2 = 0;
	vmcb->cr3 = 0;
	vmcb->cr4 = 0;
	vmcb->rflags = 0x2046;
	vmcb->efer = EFER_SVME; // exception
	vmcb->rsp= 0x67e3c;

	vmcb->rip = 0x10000c;
	vmcb->cs.sel = 0x8;
	vmcb->cs.base = 0x0;
	vmcb->cs.limit = 0xFFFFFFFF;

	vmcb->ds.sel = 0x10;
	vmcb->ds.limit = 0xFFFFFFFF;
	vmcb->es.sel = 0x10;
	vmcb->es.limit = 0xFFFFFFFF;
	vmcb->fs.sel = 0x10;
	vmcb->fs.limit = 0xFFFFFFFF;
	vmcb->gs.sel = 0x10;
	vmcb->gs.limit = 0xFFFFFFFF;
	vmcb->ss.sel = 0x10;
	vmcb->ss.limit = 0xFFFFFFFF;

	vmcb->gdtr.base = 0;
	vmcb->gdtr.limit = 0xFFFF;
	vmcb->idtr.base = 0;
	vmcb->idtr.limit = 0xFFFF;

	vmcb->ldtr.sel = 0;
	vmcb->ldtr.base = 0;
	vmcb->ldtr.limit = 0xFFFF;
	vmcb->tr.sel = 0;
	vmcb->tr.base = 0;
	vmcb->tr.limit = 0xFFFF;

//This is also the default PAT */
        vmcb->g_pat = 0x7040600070406UL;
	vmcb->cpl = 0;
}



void set_vm_to_mbr_start_state(struct vmcb* vmcb)
{
	// Prepare to load GRUB for the second time
	// Basically copy the state when GRUB is first started
	// Note: some other states will be set in svm_asm.S, at load_guest_states:
	// ebx, ecx, edx, esi, edi, ebp

	memset(vmcb, 0, sizeof(vmcb));
	cprintf("TEST:vmcb add:%x, vmcb points to: %x, vmcb size : %x\n",&vmcb,vmcb, sizeof(vmcb));

	vmcb->rax = 0;

	vmcb->rip = 0x7c00;
	cprintf("TEST:vmcb->rip : %x\n",vmcb->rip);

	vmcb->cs.attrs.bytes = 0x019B;
	vmcb->cs.limit = 0xFFFF;
	vmcb->cs.base = 0;
	vmcb->cs.sel = 0;

	vmcb->ds.sel=0x0040;
	vmcb->fs.sel=0xE717;
	vmcb->gs.sel=0xF000;

	int i;
	struct seg_selector *segregs [] = {&vmcb->ss, &vmcb->ds, &vmcb->es, &vmcb->fs, &vmcb->gs, NULL};
	for (i = 0; segregs [i] != NULL; i++)
	{
			struct seg_selector * x = segregs [i];
			x->attrs.bytes = 0x93;
			x->base = 0;
			x->limit = 0xFFFF;
	}

	vmcb->rsp=0x000003E2;

	vmcb->ss.attrs.bytes = 0x193;
	vmcb->ds.base = 0x000400;
	vmcb->fs.base = 0xE7170;
	vmcb->gs.base = 0xF0000;

	vmcb->efer = EFER_SVME;	// must be set - vol2 p373
	// EFLAGS=odItszaPc;

	vmcb->cr0 = 0x0000000000000010;

	vmcb->idtr.limit = 0x3FF;
	vmcb->idtr.base = 0;
	//setup idt?

	vmcb->gdtr.limit = 0x20;
	vmcb->gdtr.base = 0x06E127;
	//setup gdt

	vmcb->rflags = 0x2206;

	vmcb->cpl = 0; /* must be equal to SS.DPL - TODO */

	/*Anh - Setup PAT, vol2 p193
	 * Each page table entry use 3 flags: PAT PCD PWT to specify index of the
	 * corresponding PAT entry, which then specify the type of memory access for that page
		PA0 = 110	- Writeback
		PA1 = 100	- Writethrough
		PA2 = 111	- UC-
		PA3 = 000	- Unchachable
		PA4 = 110	- Writeback
		PA5 = 100	- Writethrough
		PA6 = 111	- UC-
		PA7 = 000	- Unchachable
	 This is also the default PAT */
	vmcb->g_pat = 0x7040600070406UL;
//	cprintf("TEST:vmcb->rip : %x\n",vmcb->rip);

	/******* GUEST INITIAL OPERATING MODE  ***************/
	/******* pick one ******/

	/* Legacy real mode*/
//	vmcb->cr0 = X86_CR0_ET;
//	vmcb->cr4 = 0;
/*	*/

/*	 Legacy protected mode, paging disabled 
	vmcb->cr0 = X86_CR0_PE | X86_CR0_ET;
	vmcb->cr3 = 0;
	vmcb->cr4 = 0;
	*/
	

	/* Legacy protected mode, paging enabled (4MB pages)*/
//	vmcb->cr0 = X86_CR0_PE | X86_CR0_ET | X86_CR0_PG;
//	vmcb->cr3 = 0x07000000; //Anh -vmcb->cr3 must be aligned by page size (4MB)???
//	vmcb->cr4 = X86_CR4_PSE; //Anh - enable 4MB pages
/*	*/

	/*//Anh - Long mode
	vmcb->cr0 = X86_CR0_PE | X86_CR0_MP | X86_CR0_ET | X86_CR0_NE | X86_CR0_PG;
	vmcb->cr4 = X86_CR4_PAE;
	vmcb->cr3 = 0x07000000; // dummy
	vmcb->efer |= EFER_LME | EFER_LMA;
	*/
}



/******************************************************************************************/

//void vm_create ( struct vm_info *vm, unsigned long vmm_pmem_start,
//		unsigned long vmm_pmem_size, struct e820_map *e820)
void vm_create_simple (struct vm_info *vm )
{
	cprintf("\n++++++ Creating guest VM....\n");


	struct pmem_layout * pml= get_pmem_layout();
	// Allocate a new page inside host memory for storing VMCB.
	struct vmcb *vmcb;
	vmcb = alloc_vmcb();
	vm->vmcb = vmcb;
	cprintf("VMCB location: %x\n", vmcb);

	/* Allocate new pages for physical memory of the guest OS.  */
	//const unsigned long vm_pmem_start = alloc_vm_pmem ( vm_pmem_size );
	//const unsigned long vm_pmem_start = 0x0; // guest is preallocated

	/* Set Host-level CR3 to use for nested paging.  */
	//vm->n_cr3 = create_4mb_nested_pagetable_simple(pml);
	vm->n_cr3 = create_4kb_nested_pagetable();
	vmcb->n_cr3 = vm->n_cr3;

	//Set control params for this VM
	//such as whether to enable nested paging, what to intercept...
	set_control_params(vmcb);

	// Set VM initial state
	// Guest VM start at MBR code, which is GRUB stage 1
	// vmcb->rip = 0x7c00; address of loaded MBR
	set_vm_to_mbr_start_state(vmcb);
//	set_vm_to_powerup_state(vmcb);
//	cprintf("vmcdafter:%x\n",vmcb);
//	vmcb_dump(vmcb);

//	initialize_fid2name_map(vm);
//	initialize_syscallmap(vm);
//	initialize_ptracked_list(vm);

//	vm->waitingRetSysCall = 0;
//	vm_enable_intercept(vm, 0x4);
//	vmcb->general1_intercepts |= INTRCPT_SHUTDOWN;
//vmcb->general1_intercepts |= INTRCPT_HLT;
//vmcb->general1_intercepts |= INTRCPT_INTR	;	
//	vmcb->general1_intercepts |= IUSER_ITC_SWINT		;
//	NTRCPT_INTR	;	
}

void vm_create_simple_with_interception (struct vm_info *vm )
{
	cprintf("\n++++++ Creating guest VM....\n");


	struct pmem_layout * pml= get_pmem_layout();
	// Allocate a new page inside host memory for storing VMCB.
	struct vmcb *vmcb;
	vmcb = alloc_vmcb();
	vm->vmcb = vmcb;
	cprintf("VMCB location: %x\n", vmcb);

	/* Allocate new pages for physical memory of the guest OS.  */
	//const unsigned long vm_pmem_start = alloc_vm_pmem ( vm_pmem_size );
	//const unsigned long vm_pmem_start = 0x0; // guest is preallocated

	/* Set Host-level CR3 to use for nested paging.  */
	//vm->n_cr3 = create_4mb_nested_pagetable_simple(pml);
	vm->n_cr3 = create_4kb_nested_pagetable();
	vmcb->n_cr3 = vm->n_cr3;

	//Set control params for this VM
	//such as whether to enable nested paging, what to intercept...
	set_control_params(vmcb);

	// Set VM initial state
	// Guest VM start at MBR code, which is GRUB stage 1
	// vmcb->rip = 0x7c00; address of loaded MBR
	set_vm_to_mbr_start_state(vmcb);
//	set_vm_to_powerup_state(vmcb);
//	cprintf("vmcdafter:%x\n",vmcb);
//	vmcb_dump(vmcb);

//	initialize_fid2name_map(vm);
//	initialize_syscallmap(vm);
//	vm_enable_intercept(vm, 0x4);
//	vmcb->general1_intercepts |= INTRCPT_SHUTDOWN;
//vmcb->general1_intercepts |= INTRCPT_HLT;
//vmcb->general1_intercepts |= INTRCPT_INTR	;	
//	vmcb->general1_intercepts |= IUSER_ITC_SWINT		;

}




static void switch_to_guest_os ( struct vm_info *vm )
{
//	cprintf(">>>>>>>>>>> GOING INTO THE MATRIX!!!!!\n");
	//u64 p_vmcb = vm->vmcb;

	/* Set the pointer to VMCB to %rax (vol. 2, p. 440) */
	__asm__("push %%eax; mov %0, %%eax" :: "r" (vm->vmcb));

	svm_launch ();

	__asm__("pop %eax");
}
 void start_vm_1 ( struct vmcb *vmcb1 )
{
//	cprintf(">>>>>>>>>>> GOING INTO THE MATRIX!!!!!\n");
	//u64 p_vmcb = vm->vmcb;

	/* Set the pointer to VMCB to %rax (vol. 2, p. 440) */
	__asm__("push %%eax; mov %0, %%eax" :: "r" (vmcb1));

	svm_launch ();
	
	__asm__("pop %eax");
}


/*****************************************************/

void vm_boot (struct vm_info *vm)
{
	//print_pg_table(vm->vmcb->n_cr3);
	int i=0;

	for(;;)
	{
//		vmcb_check_consistency ( vm->vmcb );
//		cprintf("\n=============================================\n");

		/* setup registers for boot  */
//		vm->vmcb->exitcode = 0;
//		cprintf("\n<<< %x >>> Guest state in VMCB:\n", i);
//		show_vmcb_state (vm); // Anh - print guest state in VMCB

//	pic_reset();
		switch_to_guest_os (vm);

	//	cprintf("\n<<< #%x >>> Guest state at VMEXIT:\n", i);

//	pic_init();
		handle_vmexit (vm);

//		cprintf("\nTesting address translation function...\n");
//		unsigned long gphysic = glogic_2_gphysic(vm);
//		cprintf("Glogical %x:%x <=> Gphysical ", vm->vmcb->cs.sel, vm->vmcb->rip);
//		cprintf("%x\n", gphysic);
//		cprintf("=============================================\n");
		i++;

//		breakpoint("End of round...\n\n");
	}
}
void clear_screen(){
	cprintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
}

void 
start_vm(){
	cprintf("Setup a vm!;\n");
        enable_amd_svm();
        struct vm_info vm;
        vm_create_simple(&vm);
	clear_screen();
        cprintf("\n++++++ New virtual machine created. Going to GRUB for the 2nd time\n");
	pic_reset();
        vm_boot (&vm);
//	run_vm_once(&vm);
}

void 
start_vm_with_interception(){
	cprintf("Setup a vm!;\n");
        enable_amd_svm();
        struct vm_info vm;
        vm_create_simple_with_interception(&vm);
	clear_screen();
        cprintf("\n++++++ New virtual machine created. Going to GRUB for the 2nd time\n");
	pic_reset();
        vm_boot (&vm);
//	run_vm_once(&vm);
}
/*
uint32_t   
create_vm(){
	cprintf("Setup the vmcb for VM!;\n");
        struct vm_info vm;
        vm_create_simple(&vm);
        cprintf("\n++++++ New virtual machine created\n");
	//pic_reset();
return &vm;
}

uint32_t   
create_vm_vmcb(){
	cprintf("Setup the vmcb for VM!;\n");
        struct vm_info vm;
        vm_create_simple(&vm);
        cprintf("\n++++++ New virtual machine created\n");
	//pic_reset();
return (uint32_t) &(vm.vmcb);
}

*/
void  run_vm_once(struct vm_info *vm){

	//pic_reset();
	cprintf(" VM@%x is going to run...\n",vm);
		switch_to_guest_os (vm);
		cprintf("\n<<< #%x >>> Guest state at VMEXIT:\n");

	pic_init();
		handle_vmexit(vm);

        interrupts_eoi();
//		cprintf("\nTesting address translation function...\n");
}

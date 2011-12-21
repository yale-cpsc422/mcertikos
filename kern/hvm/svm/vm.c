/********************************************************************************************
*
* Derived from  XEN and MAVMM
* Adapted for CertiKOS
*
* This  module provides opreations for Hardware-based Virtual Machine
*/

#include <architecture/cpufeature.h>
#include <kern/debug/debug.h>
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
#include <kern/hvm/dev/irq.h>
#include <kern/hvm/dev/i8259.h>
#include <kern/hvm/ioport.h>
#include "svm.h"
#include "vm.h"

extern void
set_iopm_intercept(uint64_t * iopmtable, uint16_t ioport, bool enable);

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

static void set_msrpm(uint32_t msrpm, uint32_t msr, bool write)
{
	assert(msrpm != 0);
	assert((msr >= MSR_ADDR1_MIN && msr <= MSR_ADDR1_MAX) ||
	       (msr >= MSR_ADDR2_MIN && msr <= MSR_ADDR2_MAX) ||
	       (msr >= MSR_ADDR3_MIN && msr <= MSR_ADDR3_MAX));

	uint32_t base = msrpm, offset, nr;

	if (msr >= MSR_ADDR1_MIN && msr <= MSR_ADDR1_MAX) {
		base += MSRPM_OFFSET1;
		offset = msr - MSR_ADDR1_MIN;
	} else if (msr >= MSR_ADDR2_MIN && msr <= MSR_ADDR2_MAX) {
		base += MSRPM_OFFSET2;
		offset = msr - MSR_ADDR2_MIN;
	} else if (msr >= MSR_ADDR3_MIN && msr <= MSR_ADDR3_MAX) {
		base += MSRPM_OFFSET3;
		offset = msr - MSR_ADDR3_MIN;
	} else {
		warn("Unrecognized MSR.\n");
		return;
	}

	nr = ((offset % (sizeof(uint8_t) << 3)) << 1) + write;
	offset = (offset / (sizeof(uint8_t) << 3)) << 1;

	set_bit(nr, (uint32_t *)base+offset);
	/* debug("set_bit nr=%x base=%x offset=%x\n", nr, base-msrpm, offset); */
	/* asm volatile("hlt"); */
}

void certikos_handle_io(uint16_t port, void *data, int direction, int size,uint32_t count)
{
    int i;
    uint8_t *ptr = data;

    for (i = 0; i < count; i++) {
	if (direction == SVM_IOIO_IN) {
	    switch (size) {
	    case 1:
		*(uint8_t *) ptr=cpu_inb(port);
		//stb_p(ptr, cpu_inb(port));
		break;
	    case 2:
		*(uint16_t *) ptr=cpu_inw(port);
	       // stw_p(ptr, cpu_inw(port));
		break;
	    case 4:
		*(uint32_t *) ptr=cpu_inl(port);
		//stl_p(ptr, cpu_inl(port));
		break;
	    }
	} else {
	    switch (size) {
	    case 1:
		//cpu_outb(port, ldub_p(ptr));
		cpu_outb(port, *(uint8_t * )ptr);
		break;
	    case 2:
		//cpu_outw(port, lduw_p(ptr));
		cpu_outw(port, *(uint16_t * )ptr);
		break;
	    case 4:
		//cpu_outl(port, ldl_p(ptr));
		cpu_outl(port, *(uint32_t * )ptr);
		break;
	    }
	}

	ptr += size;
    }
}


void
enable_intercept_pic(struct vmcb *vmcb)
{
	set_iopm_intercept(&vmcb->iopm_base_pa,0x20,1);
	set_iopm_intercept(&vmcb->iopm_base_pa,0xA0,1);
}

void
disable_intercept_pic(struct vmcb *vmcb)
{
	set_iopm_intercept(&vmcb->iopm_base_pa,0x20,0);
	set_iopm_intercept(&vmcb->iopm_base_pa,0xA0,0);
}

void enable_intercept_all_ioport(struct vmcb *vmcb){
	int port;
	//for (port=0;port<(0x4000+0x3);port++)
	for (port=0;port<(0x4003);port++)
	set_iopm_intercept(&vmcb->iopm_base_pa,port,1);
}
void disable_intercept_all_ioport(struct vmcb *vmcb){
	int port;
	for (port=0;port<(0x400*64+0x3);port++)
	set_iopm_intercept(&vmcb->iopm_base_pa,port,0);

}

void enable_vpic(struct vmcb *vmcb){

	vmcb->general1_intercepts |= INTRCPT_IO;
	//vmcb->general1_intercepts |= INTRCPT_INTR;
	//vmcb->general1_intercepts |= INTRCPT_CPUID;
	set_iopm_intercept(&vmcb->iopm_base_pa,0x20,1);
	set_iopm_intercept(&vmcb->iopm_base_pa,0x21,1);
	set_iopm_intercept(&vmcb->iopm_base_pa,0xa0,1);
	set_iopm_intercept(&vmcb->iopm_base_pa,0xa1,1);
	set_iopm_intercept(&vmcb->iopm_base_pa,0x4d0,1);
	set_iopm_intercept(&vmcb->iopm_base_pa,0x4d1,1);

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
	/* vmcb->general1_intercepts = (INTRCPT_CPUID | INTRCPT_MSR); */
	vmcb->general2_intercepts = (INTRCPT_VMRUN | INTRCPT_VMMCALL);

	//allocating a region for IOPM (permission map)
	//and fill it with 0x00 (not intercepting anything)
	vmcb->iopm_base_pa  = create_intercept_table ( 12 << 10 ); /* 12 Kbytes */
	//memset ( vmcb->iopm_base_pa  , 0xff, 0x3*PAGE_SIZE );

	//allocating a region for msr intercept table, and fill it with 0x00
	vmcb->msrpm_base_pa = create_intercept_table ( 8 << 10 );  /* 8 Kbytes */
	set_msrpm(vmcb->msrpm_base_pa, MSR_INTR_PENDING, MSR_READ);

//	vmcb->general1_intercepts |= INTRCPT_INTR;
//		enable_vpic(vmcb);

	// set_iopm_intercept(&vmcb->iopm_base_pa,0x3f8,1);
	cprintf("iopmbase:%x\n",&vmcb->iopm_base_pa);
	//enable_intercept_all_ioport(vmcb);

//	enable_intercept_pic(vmcb);

//	vmcb->general1_intercepts |= INTRCPT_INTN;
}

/********************************************************************************************/

static unsigned long create_4kb_nested_pagetable ( )
{
	as_t *  pmap=as_new_vm();

	int i;
	cprintf("as_new:%x\n",pmap);


	for(i=0x100000;i<(GUEST_FIXED_PMEM_BYTES+0x100000);i=i+PAGE_SIZE){

		as_reserve(pmap,i,PTE_W|PTE_U|PTE_G);
	}

	cprintf( "Nested page table created.\n" );

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
	/*if (flags & USER_ITC_TASKSWITCH) {
//		cprintf("Disable taskswitch interception\n");
		vm->vmcb->cr_intercepts &= ~INTRCPT_WRITE_CR3;
	}*/
/*
	if (flags & USER_ITC_SWINT) {
//		cprintf("Disable software interrupt interception\n");
		vm->vmcb->general1_intercepts &= ~INTRCPT_INTN;
	}
*/
	if (flags & USER_ITC_IRET) {
//		cprintf("Enable software interrupt interception\n");
		vm->vmcb->general1_intercepts &= ~INTRCPT_IRET;
	}

	if (flags & USER_ITC_SYSCALL) {
//		cprintf("Disable syscall interception\n");
		// **************** int 80h **************************
		vm->vmcb->general1_intercepts &= ~INTRCPT_INTN;

	}

	// disable single stepping
	if (flags & USER_SINGLE_STEPPING) {
//		cprintf("Disable single stepping\n");
		vm->vmcb->rflags &= ~X86_RFLAGS_TF;
		vm->vmcb->exception_intercepts &= ~INTRCPT_DB;
	}

	if (flags & INTRCPT_INTR) {
		vm->vmcb->general1_intercepts &= ~INTRCPT_INTR;
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
/*	if (flags & USER_ITC_TASKSWITCH) {
//		cprintf("Enable taskswitch interception\n");
		vm->vmcb->cr_intercepts |= INTRCPT_WRITE_CR3;

	}
*/
	//enable software interrupt interception
	/*if (flags & USER_ITC_SWINT) {
//		cprintf("Enable software interrupt interception\n");
		vm->vmcb->general1_intercepts |= INTRCPT_INTN;
	}
*/
	if (flags & USER_ITC_IRET) {
//		cprintf("Enable software interrupt interception\n");
		vm->vmcb->general1_intercepts |= INTRCPT_IRET;
	}

	//enable syscall interception - both int80 and sys_enter
	if (flags & USER_ITC_SYSCALL) {
//		cprintf("Enable syscall interception\n");

		// **************** int 80h ***************************
		vm->vmcb->general1_intercepts |= INTRCPT_INTN;

	}

	// enable single stepping
	if (flags & USER_SINGLE_STEPPING) {
//		cprintf("Enable single stepping\n");
		vm->vmcb->rflags |= X86_RFLAGS_TF;
		vm->vmcb->exception_intercepts |= INTRCPT_DB;
	}

	if (flags & INTRCPT_INTR) {
		vm->vmcb->general1_intercepts |= INTRCPT_INTR;
	}
}


/********************************************************************************************/
/********************************* INITIALIZE VM STATE *************************************/
/********************************************************************************************/

//- Test, to set initial state of VM to state of machine right after power up
void set_vm_to_powerup_state(struct vmcb * vmcb)
{
	// vol 2 p350
	//memset(vmcb, 0, sizeof(vmcb));

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
	vmcb->g_pat = 0x7040600070406ULL;
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

	vmcb->g_pat = 0x7040600070406ULL;


}



/******************************************************************************************/

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

	//inital setting for single step interception
	vm->itc_skip_flag=0;

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
	//set_vm_to_powerup_state(vmcb);
//	vm->cpu_irq=qemu_alloc_irqs(pic_irq_request, NULL, 1);

	//vm->i8259=i8259_init(vm->cpu_irq);
	vm->i8259=i8259_init(NULL);
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


	/* Set Host-level CR3 to use for nested paging.  */
	vm->n_cr3 = create_4kb_nested_pagetable();
	vmcb->n_cr3 = vm->n_cr3;

	//Set control params for this VM
	//such as whether to enable nested paging, what to intercept...
	set_control_params(vmcb);

	// Set VM initial state
	set_vm_to_mbr_start_state(vmcb);
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

void
vm_boot (struct vm_info *vm)
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

		switch_to_guest_os (vm);

	//	cprintf("\n<<< #%x >>> Guest state at VMEXIT:\n", i);

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
void
clear_screen(){
	cprintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
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
	//pic_enable(IRQ_KBD);
	vm_boot (&vm);
}

void
start_vm_with_interception(){
	cprintf("Setup a vm!;\n");
	enable_amd_svm();
	struct vm_info vm;
	vm_create_simple_with_interception(&vm);
	clear_screen();
	cprintf("\n++++++ New virtual machine created. Going to GRUB for the 2nd time\n");
//	pic_reset();
	vm_boot (&vm);
}

void
run_vm_once(struct vm_info *vm){

	cprintf(" VM@%x is going to run...\n",vm);
	switch_to_guest_os (vm);
	cprintf("\n<<< #%x >>> Guest state at VMEXIT:\n");
	handle_vmexit(vm);
	interrupts_eoi();
}

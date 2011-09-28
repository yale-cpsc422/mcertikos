
//#include "user.h"
#include "intercept.h"
#include "vmexit.h"
#include "svm.h"
#include "vm.h"
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/as/as.h>
#include <kern/mem/mem.h>
#include <architecture/mmu.h>


/*****************************************************************************************/
/************************ MISCELANEOUS FUNCTIONS ***************************************/




/*****************************************************************************************/
/************************ HANDLE NESTED PAGE FAULT ********************************/

void __handle_vm_npf (struct vm_info *vm)
{
	// Note for NPF: p410 - 15.24.6 Nested versus Guest Page Faults, Fault Ordering
	uint64_t errcode = vm->vmcb->exitinfo1;
	print_page_errorcode(errcode);

	//TODO: execute requested access on flash memory (usb drive)
//	cprintf("Nested page fault!");

//	mmap_4mb(vm->n_cr3, vm->vmcb->exitinfo2, vm->vmcb->exitinfo2, 1);
//	cprintf("mapping %lx on demand %lx \n",  vm->vmcb->exitinfo2,vm->vmcb->exitinfo2);

	// bit 1 of rflags must be set
	vm->vmcb->rflags |= 2;

	//uint32_t  va= PGADDR(vm->vmcb->exitinfo2); 
//	uint32_t va=PGADDR((uint32_t)* (&(vm->vmcb->exitinfo2))); 
	//pageinfo * pi=mem_alloc();
//	cprintf("n_cr3 is : %x, va is :  %x\n", vm->vmcb->n_cr3, va);// PGADDR((unsigned long)vm->vmcb->exitinfo2));
	as_reserve((as_t *)vm->vmcb->n_cr3,(uint32_t) vm->vmcb->exitinfo2, PTE_W|PTE_U|PTE_G); 
}

/*****************************************************************************************/
/************************ HANDLE SOFTWARE INTERRUPT & IRET ********************************/

void __handle_vm_swint (struct vm_info *vm)
{

}

/******************************************************/

void __handle_vm_iret (struct vm_info *vm)
{

}

/*****************************************************************************************/
/************************************ HANDLE EXCEPTIONS **********************************/

void __handle_vm_exception (struct vm_info *vm)
{
	//Special handling for GP (syscall)
	switch (vm->vmcb->exitcode)
	{
	case VMEXIT_EXCEPTION_TS:
		print_vmcb_state(vm->vmcb);
		cprintf("TSS invalid!");
		break;

	case VMEXIT_EXCEPTION_GP:
		print_vmcb_state(vm->vmcb);

		//switch to system call handling code
		vm->vmcb->cs.sel = vm->org_sysenter_cs;
		vm->vmcb->ss.sel = vm->org_sysenter_cs + 8;
		vm->vmcb->rsp = vm->vmcb->sysenter_esp;
		vm->vmcb->rip = vm->vmcb->sysenter_eip;
		return;

	case VMEXIT_EXCEPTION_DB:
//		cprintf(">> got DB exception\n");

		//check if #DB is caused by single stepping and, if VM enabled single stepping
		// if yes => handle #DB transparently
		if ((vm->vmcb->dr6 & X86_DR6_BS) && (vm->itc_flag & USER_SINGLE_STEPPING)) {
			//TODO: check if program is unpacked?
//			print_vmcb_state(vm->vmcb);
		}

		if (vm->itc_skip_flag) {
//			cprintf("re-enable skipped interception: %x\n", vm->itc_skip_flag);
			//re-enable skipped interception
			vm_enable_intercept(vm, vm->itc_skip_flag);
			vm->itc_skip_flag = 0;

			// if current process is being tracked and single stepping is requested
			// => should not disable single stepping for the next instruction
			if (!vm->btrackcurrent || !(vm->itc_flag & USER_SINGLE_STEPPING))
				vm_disable_intercept(vm, USER_SINGLE_STEPPING);
		}

		return;

	case VMEXIT_EXCEPTION_PF: //VECTOR_PF
		// check if error was a write protection violation
		// and that UNPACK mode is enabled
		if ((vm->vmcb->exitinfo1 & 1) && (vm->vmcb->exitinfo1 & 2) && (vm->itc_flag & USER_UNPACK))
		{
			cprintf("USER_UNPACK: guest wrote to %x\n", vm->vmcb->exitinfo2);

			return;
		}
		else print_page_errorcode(vm->vmcb->exitinfo1);

		//else => page fault caused by normal guest activity => inject exception
		vm->vmcb->cr2 = vm->vmcb->exitinfo2;
		break;
	}

	// See AMD manual vol2, page 392 & 385
	int vector =  vm->vmcb->exitcode - VMEXIT_EXCEPTION_DE;

	//General handling
	vm->vmcb->eventinj.fields.vector = vector;
	vm->vmcb->eventinj.fields.type = EVENT_TYPE_EXCEPTION;
	vm->vmcb->eventinj.fields.ev = 1;
	vm->vmcb->eventinj.fields.v = 1;
	vm->vmcb->eventinj.fields.errorcode = vm->vmcb->exitinfo1;

	cprintf( "\n#Exception vector=%x injected into guest, code: %x\n", vector, vm->vmcb->eventinj.bytes);

//	cprintf( "\nValue of EXITINTINFO: %x\n", vm->vmcb->exitintinfo);
}

/*****************************************************************************************/
/********************************* HANDLE TASKSWITCH **************************************/

void __handle_cr3_write (struct vm_info *vm) {
	cprintf(">> write cr3\n");

}

/***************************************************/


/***************************************************/
void __handle_task_switch (struct vm_info *vm) {
	cprintf("Task switch\n");
}

/**********************************************************/

void __handle_vm_interupt (struct vm_info *vm)
{
	// See AMD manual vol2, page 392
	cprintf("vm_interrupt\n");


}
/*****************************************************************************************/
/********************************** MAIN FUNCTION ****************************************/
/*****************************************************************************************/

void handle_vmexit (struct vm_info *vm)
{
/*	cprintf("**** ");
	cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
	print_vmcb_state(vm);
	print_vmexit_exitcode (vm->vmcb);
*/

//	if (vm->vmcb->exitintinfo.fields.type == EVENT_TYPE_EXCEPTION)
//		cprintf("Pending guest exception!\n");

	switch (vm->vmcb->exitcode)
	{
		case VMEXIT_MSR:
			if (vm->vmcb->exitinfo1 == 1) ;//__handle_vm_wrmsr (vm);
			break;
		case VMEXIT_EXCEPTION_DE ... VMEXIT_EXCEPTION_XF:
			__handle_vm_exception(vm); break;

		//software interrupt (int n)
		case VMEXIT_SWINT: __handle_vm_swint(vm); break;
		//nested page fault
		case VMEXIT_NPF: __handle_vm_npf (vm); break;
		//vmmcall
		case VMEXIT_VMMCALL: ; break;//
		//iret
		case VMEXIT_IRET: __handle_vm_iret(vm); break;
		//write to cr3
		case VMEXIT_CR3_WRITE: __handle_cr3_write(vm);break;
    		case VMEXIT_INTR: /*cprintf("N");*/break;
//		case VMEXIT_TASK_SWITCH: __handle_task_switch(vm); break;
	}
//	cprintf("**** ");
//	cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
}

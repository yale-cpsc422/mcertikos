/*************************************************************************
*
* This file was adapted from XEN and MAVMM
*
* VMCB module provides the Virtual Machine Control Block and relted operations
*
*/

#include "intercept.h"
#include "vmexit.h"
#include "svm.h"
#include "vm.h"
#include <kern/debug/debug.h>
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/as/as.h>
#include <kern/mem/mem.h>
#include <architecture/mmu.h>
#include <architecture/cpuid.h>
#include <architecture/x86.h>

/*****************************************************************************************/
/************************ HANDLE NESTED PAGE FAULT ********************************/

void __handle_vm_npf (struct vm_info *vm)
{
	// Note for NPF: p410 - 15.24.6 Nested versus Guest Page Faults, Fault Ordering
	uint64_t errcode = vm->vmcb->exitinfo1;
	/* print_page_errorcode(errcode); */

	// bit 1 of rflags must be set
	vm->vmcb->rflags |= 2;

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
	debug("__handle_vm_exception\n");
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
		else /* print_page_errorcode(vm->vmcb->exitinfo1) */;

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


void __handle_task_switch (struct vm_info *vm) {
	cprintf("Task switch\n");
}

/**********************************************************/

void __handle_vm_interrupt (struct vm_info *vm)
{
	// See AMD manual vol2, page 392
	debug("vm_interrupt\n");
	debug(">> error code = %x, error valid = %d\n",
	      vm->vmcb->exitintinfo.fields.errorcode,
	      vm->vmcb->exitintinfo.fields.ev);
	debug(">> vector = %x, valid = %d\n",
	      vm->vmcb->exitintinfo.fields.vector,
	      vm->vmcb->exitintinfo.fields.v);
	debug(">> type = %x\n", vm->vmcb->exitintinfo.fields.type);

	vm->vmcb->vintr.fields.irq = 0;

}

void __handle_vm_cpuid (struct vm_info *vm)
{
	debug("__handle_vm_cpuid\n");
	debug("cpuid %x\n", vm->vmcb->rax);

	uint32_t eax, ebx, ecx, edx;
	switch (vm->vmcb->rax) {
	/* case CPUID_CPU_VID: */
	/*	cpuid(CPUID_CPU_VENDOR, &eax, &ebx, &ecx, &edx); */
	/*	vm->vmcb->rip += 2; */
	/*	g_ebx = INTEL_VENDOR_EBX; */
	/*	g_ecx = INTEL_VENDOR_ECX; */
	/*	g_edx = INTEL_VENDOR_EDX; */
	/*	break; */
	case CPUID_CPU_INFO:
		cpuid(CPUID_CPU_VID, &eax, &ebx, &ecx, &edx);;
		if (ebx == CPUID_VID_AMD_EBX &&
		    ecx == CPUID_VID_AMD_ECX &&
		    edx == CPUID_VID_AMD_EDX) {
			debug("CPU Vendor: AuthenticAMD\n");
			cpuid(CPUID_CPU_INFO, &eax, &ebx, &ecx, &edx);
			vm->vmcb->rax =
				(CPUID_TYPE_OEM << 12) ||
				(/* CPUID_FAMILY_EXTENDED */CPUID_FAMILY_686 << 8) ||
				(CPUID_MODEL_ATHLON64 <<4) ||
				(0x8);
			vm->vmcb->rip += 2;
			g_ebx = ebx;
			g_ecx = ecx;
			g_edx = edx;
		} else if (ebx == CPUID_VID_INTEL_EBX &&
			   ecx == CPUID_VID_INTEL_ECX &&
			   edx == CPUID_VID_INTEL_EDX){
			debug("CPU Vendor: GenuineIntel\n");
			cpuid(CPUID_CPU_INFO, &eax, &ebx, &ecx, &edx);
			vm->vmcb->rax = eax;
			vm->vmcb->rip += 2;
			g_ebx = ebx;
			g_ecx = ecx;
			g_edx = edx;
		} else {
			debug("CPU Vendor: %x %x %x\n", ebx, ecx, edx);
			cpuid(CPUID_CPU_INFO, &eax, &ebx, &ecx, &edx);
			vm->vmcb->rax = eax;
			vm->vmcb->rip += 2;
			g_ebx = ebx;
			g_ecx = ecx;
			g_edx = edx;
		}
		break;

	default:
		cpuid(vm->vmcb->rax, &eax, &ebx, &ecx, &edx);
		debug(">> eax=%x, ebx=%x, ecx=%x, edx=%x\n", eax, ebx, ecx, edx);
		vm->vmcb->rax = eax;
		vm->vmcb->rip += 2;
		g_ebx = ebx;
		g_ecx = ecx;
		g_edx = edx;
		break;
	}
}

void __handle_vm_rdmsr (struct vm_info *vm)
{
	debug("__handle_vm_rdmsr]n");

	uint32_t edx, eax;

	switch (g_ecx) {
	case MSR_INTR_PENDING:
		debug("rdmsr MSR_INTR_PENDING, ip=%x\n", vm->vmcb->rip);

		/* uint32_t hi, lo; */
		/* rdmsr(MSR_INTR_PENDING, hi, lo); */
		/* debug("MSR_INTR_PEND_DIS=%d\n", lo & MSR_INTR_PEND_DIS); */
		/* g_edx = hi; */
		/* vm->vmcb->rax = lo; */
		/* vm->vmcb->rip += 2; */

		g_edx = 0;
		vm->vmcb->rax = 1<<MSR_INTR_PND_MSG_DIS;
		vm->vmcb->rip += 2;
		break;

	default:
		/* TODO: Manipulate GP#, may inject GP to guest? */
		debug("rdmsr %x, ip=%x\n", g_ecx, vm->vmcb->rip);
		rdmsr(g_ecx, edx, eax);
		g_edx = edx;
		vm->vmcb->rax = eax;
		vm->vmcb->rip += 2;
		break;
	}
}

void __handle_vm_hlt (struct vm_info *vm)
{
	debug("hlt ip=%x\n", vm->vmcb->rip);

	vm->vmcb->rip += 1;

	halt();
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
	case VMEXIT_CPUID:
		__handle_vm_cpuid(vm);
		break;

	case VMEXIT_MSR:
		if (vm->vmcb->exitinfo1 == 1)
			;//__handle_vm_wrmsr (vm);
		else
			__handle_vm_rdmsr(vm);
		break;

	case VMEXIT_HLT:
		__handle_vm_hlt(vm);
		break;

	case VMEXIT_EXCEPTION_DE ... VMEXIT_EXCEPTION_XF:
		__handle_vm_exception(vm);
		break;

	//software interrupt (int n)
	case VMEXIT_SWINT:
		__handle_vm_swint(vm);
		break;

	//nested page fault
	case VMEXIT_NPF:
		__handle_vm_npf (vm);
		break;

	//vmmcall
	case VMEXIT_VMMCALL:
		;
		break;//

	//iret
	case VMEXIT_IRET:
		__handle_vm_iret(vm);
		break;

	//write to cr3
	case VMEXIT_CR3_WRITE:
		__handle_cr3_write(vm);
		break;

	case VMEXIT_INTR:
		__handle_vm_interrupt(vm);
		break;

//		case VMEXIT_TASK_SWITCH: __handle_task_switch(vm); break;
	}
//	cprintf("**** ");
//	cprintf("**** #VMEXIT - exit code: %x\n", (uint32_t) vm->vmcb->exitcode);
	/* debug("handle_vmexit done\n"); */
}

/********************************************************************************
* Derived from  XEN and MAVMM
* Adapted for CertiKOS
*
* This module handles the SVM VMExit related operations
*
*********************************************************************************/
 
#include "vmexit.h"
#include <kern/debug/stdio.h>

void print_vmexit_exitcode (struct vmcb * vmcb)
{
	cprintf ( "#VMEXIT: ");

	switch ( vmcb->exitcode ) {
	case VMEXIT_EXCEPTION_PF:
		cprintf ( "EXCP (page fault)" ); break;
	case VMEXIT_NPF:
		cprintf ( "NPF (nested-paging: host-level page fault)" ); break;
	case VMEXIT_INVALID:
		cprintf ( "INVALID" ); break;
	default:
		cprintf ( "%x", ( unsigned long ) vmcb->exitcode ); break;
	}

	cprintf ( "\n" );
	cprintf ( "exitinfo1 (error_code) = %x, ", vmcb->exitinfo1);
	cprintf ( "exitinfo2 = %x, ", vmcb->exitinfo2);
	cprintf ( "exitINTinfo = %x\n", vmcb->exitintinfo );
}

/*****************************************************/
//manual vol 2 - 8.4.2 Page-Fault Error Code
// Note for NPF: p410 - 15.24.6 Nested versus Guest Page Faults, Fault Ordering
void print_page_errorcode(uint64_t errcode)
{
	if (errcode & 1) cprintf ( "page fault was caused by a page-protection violation\n" );
	else cprintf ( "page fault was caused by a not-present page\n" );

	if (errcode & 2) cprintf ( "memory access was write\n" );
	else cprintf ( "memory access was read\n" );

	if (errcode & 4 ) cprintf ( "an access in user mode caused the page fault\n" );
	else cprintf ( "an access in supervisor mode caused the page fault\n" );

	if (errcode & 8 ) cprintf ( "error caused by reading a '1' from reserved field, \
			when CR4.PSE=1 or CR4.PAE=1\n" );

	if (errcode & 16 ) cprintf ( "error caused by instruction fetch, when \
			EFER.NXE=1 && CR4.PAE=1");
}

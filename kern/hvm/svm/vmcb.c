/*************************************************************************
*
* This file was adapted from XEN and MAVMM
*
* VMCB module provides the Virtual Machine Control Block and relted operations
*
*/

#include "intercept.h"
#include <architecture/cpu.h>
#include "vmcb.h"
#include "svm.h"
#include <architecture/cpufeature.h>
#include <kern/debug/string.h>
#include <kern/debug/stdio.h>
#include <kern/mem/mem.h>
#include <architecture/types.h>
#include <architecture/x86.h>

void print_vmcb_state (struct vmcb *vmcb)
{
	cprintf ( "*******  VMCB STATE  *********\n" );
	cprintf ( "cs:ip = %x:%x,", vmcb->cs.sel, vmcb->rip );
	cprintf ( "ss:sp = %x:%x, ", vmcb->ss.sel, vmcb->rsp );
	cprintf ( "ds:bp = %x:%x\n", vmcb->ds.sel, g_ebp);

	cprintf ( "eax = %x, ebx = %x, ecx = %x, edx = %x", vmcb->rax, g_ebx, g_ecx, g_edx);
	cprintf ( "esi = %x, edi = %x\n", g_esi, g_edi);

	cprintf ( "cpl=%x,", vmcb->cpl );
	cprintf ( "cr0=%x, cr3=%x, cr4=%x,", vmcb->cr0, vmcb->cr3, vmcb->cr4 );
	cprintf ( "rflags=%x, efer=%x\n", vmcb->rflags, vmcb->efer );
	cprintf("RSP = %x, ",(unsigned long long) vmcb->rsp);
	cprintf("RIP = %x\n",(unsigned long long) vmcb->rip);

//	cprintf ( "cs.attrs=%x, ds.attrs=%x\n", vmcb->cs.attrs.bytes, vmcb->ds.attrs.bytes );
//	cprintf ( "cs.base=%x, ds.base=%x\n", vmcb->cs.base, vmcb->ds.base );
//	cprintf ( "cs.limit=%x, ds.limit=%x\n", vmcb->cs.limit, vmcb->ds.limit);
}

void print_vmcb_intr_state(struct vmcb *vmcb) {


	cprintf("Vector: %x||", vmcb->exitintinfo.fields.vector);
	cprintf("Type: %x||", vmcb->exitintinfo.fields.type);
	cprintf("Ev: %x||", vmcb->exitintinfo.fields.ev);
	cprintf("Rsvd1: %x||", vmcb->exitintinfo.fields.resvd1);
	cprintf("V: %x||", vmcb->exitintinfo.fields.v);
	cprintf("Errorcode: %x||", vmcb->exitintinfo.fields.errorcode);
//	int vector =vmcb->exitintinfo.fields.vector;
//	cprintf("vector: %x",vector);
}

void print_vmcb_vintr_state (struct vmcb *vmcb)
{
	cprintf ( "*******  VMCB vintr  *********\n" );
//	cprintf ( "vintr = %x:", (uint64_t) vmcb->vintr.fields  );
	cprintf ( "tpr = %x:", vmcb->vintr.fields.tpr );
	cprintf ( "riq = %x:", vmcb->vintr.fields.irq );
	cprintf ( "rsvd0 = %x:", vmcb->vintr.fields.rsvd0 );
	cprintf ( "prio = %x:", vmcb->vintr.fields.prio );
	cprintf ( "vector = %x:", vmcb->vintr.fields.vector );
	cprintf ( "intr_masking = %x:", vmcb->vintr.fields.intr_masking );
	cprintf ( "rsvd2 = %x:", vmcb->vintr.fields.rsvd2 );
	cprintf ( "rsvd3 = %x:", vmcb->vintr.fields.rsvd3 );
}

#define BIT_MASK(n)  ( ~ ( ~0ULL << (n) ) )	/* 64 bits mask */
#define SUB_BIT(x, start, len) ( ( ( ( x ) >> ( start ) ) & BIT_MASK ( len ) ) )


/* [REF] AMD64 manual vol 2, pp. 373 */
static int check_efer_svme ( const struct vmcb *vmcb )
{
	return ( ! ( vmcb->efer & EFER_SVME ) );
}

static int check_cr0cd_cr0nw ( const struct vmcb *vmcb )
{
	return ( ( ! ( vmcb->cr0 & X86_CR0_CD ) ) && ( vmcb->cr0 & X86_CR0_NW ) );
}

static int check_cr0_32_63 ( const struct vmcb *vmcb )
{
	return ( SUB_BIT ( vmcb->cr0, 32,  32 ) );
}

static int check_cr4_11_63 ( const struct vmcb *vmcb )
{
	return ( SUB_BIT ( vmcb->cr4, 11, (uint64_t) 53 ) );
}

static int check_dr6_32_63 ( const struct vmcb *vmcb )
{
	return ( SUB_BIT ( vmcb->dr6, 32, 32 ) );
}

static int check_dr7_32_63 ( const struct vmcb *vmcb )
{
	return ( SUB_BIT ( vmcb->dr7, 32, 32 ) );
}

static int check_efer_15_63 ( const struct vmcb *vmcb )
{
	return ( SUB_BIT ( vmcb->efer, 15, 49 ) );
}

static int check_eferlme_cr0pg_cr4pae ( const struct vmcb *vmcb )
{
	return ( ( vmcb->efer & EFER_LME ) && ( vmcb->cr0 & X86_CR0_PG ) &&
		 ( ! ( vmcb->cr4 & X86_CR4_PAE ) ) );
}

static int check_eferlme_cr0pg_cr0pe ( const struct vmcb *vmcb )
{
	return ( ( vmcb->efer & EFER_LME ) && ( vmcb->cr0 & X86_CR0_PG ) &&
		 ( ! ( vmcb->cr0 & X86_CR0_PE ) ) );
}

/* [REF] Code-Segment Register - Long mode */
static int check_eferlme_cr0pg_cr4pae_csl_csd ( const struct vmcb *vmcb )
{
	return ( ( vmcb->efer & EFER_LME ) && ( vmcb->cr0 & X86_CR0_PG ) &&
		 ( vmcb->cr4 & X86_CR4_PAE ) && ( vmcb->cs.attrs.fields.l ) && ( vmcb->cs.attrs.fields.db ) );
}

static int check_vmrun_intercept ( const struct vmcb *vmcb )
{
	return ( ! ( vmcb->general2_intercepts & INTRCPT_VMRUN ) );
}

static int check_msr_ioio_intercept_tables ( const struct vmcb *vmcb )
{
	/* The MSR or IOIO intercept tables extend to a physical address >= the maximum supported physical address */
//	return ( ( vmcb->iopm_base_pa >= ) || ( vmcb->msrpm_base_pa >= ) ); /* [DEBUG] */
	return 0;
}


struct consistencty_check {
	int ( *func ) ( const struct vmcb *vmcb );
	char *error_msg;
};

void vmcb_check_consistency ( struct vmcb *vmcb )
{
	const struct consistencty_check tbl[]
		= { { &check_efer_svme,   "EFER.SVME is not set.\n" },
		    { &check_cr0cd_cr0nw, "CR0.CD is not set, and CR0.NW is set.\n" },
		    { &check_cr0_32_63,   "CR0[32:63] are not zero.\n" },
		    { &check_cr4_11_63,   "CR4[11:63] are not zero.\n" },
		    { &check_dr6_32_63,   "DR6[32:63] are not zero.\n" },
		    { &check_dr7_32_63,   "DR7[32:63] are not zero.\n" },
		    { &check_efer_15_63,  "EFER[15:63] are not zero.\n" },
		    { &check_eferlme_cr0pg_cr4pae, "EFER.LME is set, CR0.PG is set, and CR4.PAE is not set.\n" },
		    { &check_eferlme_cr0pg_cr0pe,  "EFER.LME is set, CR0.PG is set, and CR4.PE is not set.\n" },
		    { &check_eferlme_cr0pg_cr4pae_csl_csd, "EFER.LME, CR0.PG, CR4.PAE, CS.L, and CS.D are set.\n" },
		    { &check_vmrun_intercept, "The VMRUN intercept bit is clear.\n" },
		    { &check_msr_ioio_intercept_tables, "Wrong The MSR or IOIO intercept tables address.\n" } };
	const size_t nelm = sizeof ( tbl ) / sizeof ( struct consistencty_check );

	int i;
	for (i = 0; i < nelm; i++)
	{
		if ( (* tbl[i].func) (vmcb) )
		{
			cprintf (tbl[i].error_msg);
			cprintf ("Consistency check failed.\n");
		}
	}
}

/********************************************************************************************/

static void seg_selector_dump ( char *name, const struct seg_selector *s )
{
	cprintf ( "%s: sel=%x, attr=%x, limit=%x, base=%x\n",
		 name, s->sel, s->attrs.bytes, s->limit,
		 (unsigned long long)s->base );
}

void vmcb_dump( struct vmcb *vmcb )
{
	cprintf("Dumping guest's current state\n");
	cprintf("Size of VMCB = %x, address = %x\n",
	       (int) sizeof(struct vmcb), vmcb);

	cprintf("cr_intercepts = %x dr_intercepts = %x exception_intercepts = %x\n",
	       vmcb->cr_intercepts, vmcb->dr_intercepts, vmcb->exception_intercepts);
	cprintf("general1_intercepts = %x general2_intercepts = %x\n",
	       vmcb->general1_intercepts, vmcb->general2_intercepts);
	cprintf("iopm_base_pa = %x msrpm_base_pa = %x tsc_offset = %x\n",
	       (unsigned long long) vmcb->iopm_base_pa,
	       (unsigned long long) vmcb->msrpm_base_pa,
	       (unsigned long long) vmcb->tsc_offset);
	cprintf("tlb_control = %x vintr = %x interrupt_shadow = %x\n", vmcb->tlb_control,
	       (unsigned long long) vmcb->vintr.bytes,
	       (unsigned long long) vmcb->interrupt_shadow);
	cprintf("exitcode = %x exitintinfo = %x",
	       (unsigned long long) vmcb->exitcode,
	       (unsigned long long) vmcb->exitintinfo.bytes);
	cprintf("exitinfo1 = %x exitinfo2 = %x \n",
	       (unsigned long long) vmcb->exitinfo1,
	       (unsigned long long) vmcb->exitinfo2);
	//cprintf("np_enable = %x guest_asid@ = %x, with value %x\n",(unsigned long long )vmcb->np_enable, &vmcb->guest_asid,&(vmcb->guest_asid),(unsigned long long )vmcb->guest_asid);
	       //(unsigned long long) vmcb->np_enable, &(vmcb->guest_asid), (unsigned long long )vmcb->guest_asid);
	cprintf("np_enable = %x guest_asid= %x\n",(unsigned long long )vmcb->np_enable, &(vmcb->guest_asid));
	      // vmcb->np_enable, &(vmcb->guest_asid), vmcb->guest_asid);
	cprintf("cpl = %x efer = %x star = %x lstar = %x\n",
	       vmcb->cpl, (unsigned long long) vmcb->efer,
	       (unsigned long long) vmcb->star, (unsigned long long) vmcb->lstar);
	cprintf("CR0 = %x CR2 = %x ",
	       (unsigned long long) vmcb->cr0, (unsigned long long) vmcb->cr2);
	cprintf("CR3 = %x CR4 = %x  ",
	       (unsigned long long) vmcb->cr3, (unsigned long long) vmcb->cr4);
	cprintf("RSP = %x  RIP = %x\n",
	       (unsigned long long) vmcb->rsp, (unsigned long long) vmcb->rip);
	cprintf("RAX = %x  RFLAGS=%x ",
	       (unsigned long long) vmcb->rax, (unsigned long long) vmcb->rflags);
	cprintf("DR6 = %x, DR7 = %x\n",
	       (unsigned long long) vmcb->dr6, (unsigned long long) vmcb->dr7);
	cprintf("CSTAR = %x SFMask = %x ",
	       (unsigned long long) vmcb->cstar, (unsigned long long) vmcb->sfmask);
	cprintf("KernGSBase = %x PAT = %x \n",
	       (unsigned long long) vmcb->kerngsbase,
	       (unsigned long long) vmcb->g_pat);

	/* print out all the selectors */
	seg_selector_dump ( "CS", &vmcb->cs );
	seg_selector_dump ( "DS", &vmcb->ds );
	seg_selector_dump ( "SS", &vmcb->ss );
	seg_selector_dump ( "ES", &vmcb->es );
	seg_selector_dump ( "FS", &vmcb->fs );
	seg_selector_dump ( "GS", &vmcb->gs );
	seg_selector_dump ( "GDTR", &vmcb->gdtr );
	seg_selector_dump ( "LDTR", &vmcb->ldtr );
	seg_selector_dump ( "IDTR", &vmcb->idtr );
	seg_selector_dump ( "TR", &vmcb->tr );
}

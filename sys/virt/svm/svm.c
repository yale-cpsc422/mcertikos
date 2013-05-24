#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include "svm.h"

#ifdef DEBUG_SVM

#define SVM_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("SVM: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define SVM_DEBUG(fmt, ...)			\
	do {					\
	} while (0)

#endif

static struct svm	svm_pool[MAX_VMID];
static spinlock_t	svm_pool_lk;
volatile static bool	svm_pool_inited = FALSE;

struct svm *
alloc_svm(void)
{
	struct svm *svm;
	int i;

	spinlock_acquire(&svm_pool_lk);

	for (i = 0; i < MAX_VMID; i++)
		if (svm_pool[i].inuse == 0)
			break;

	if (i == MAX_VMID) {
		SVM_DEBUG("No unused SVM descriptor.\n");
		spinlock_release(&svm_pool_lk);
		return NULL;
	}

	svm = &svm_pool[i];
	svm->inuse = 1;

	spinlock_release(&svm_pool_lk);

	return svm;
}

void
free_svm(struct svm *svm)
{
	if (svm == NULL)
		return;

	spinlock_acquire(&svm_pool_lk);
	svm->inuse = 0;
	spinlock_release(&svm_pool_lk);
}

/*
 * Check whether the machine supports SVM. (Sec 15.4, APM Vol2 r3.19)
 *
 * @return TRUE if SVM is supported; otherwise FALSE
 */
static bool
svm_check(void)
{
	/* check CPUID 0x80000001 */
	uint32_t feature, dummy;
	cpuid(CPUID_FEATURE_FUNC, &dummy, &dummy, &feature, &dummy);
	if ((feature & CPUID_X_FEATURE_SVM) == 0) {
		SVM_DEBUG("The processor does not support SVM.\n");
		return FALSE;
	}

	/* check MSR VM_CR */
	if ((rdmsr(MSR_VM_CR) & MSR_VM_CR_SVMDIS) == 0) {
		return TRUE;
	}

	/* check CPUID 0x8000000a */
	cpuid(CPUID_SVM_FEATURE_FUNC, &dummy, &dummy, &dummy, &feature);
	if ((feature & CPUID_SVM_LOCKED) == 0) {
		SVM_DEBUG("SVM maybe disabled by BIOS.\n");
		return FALSE;
	} else {
		SVM_DEBUG("SVM maybe disabled with key.\n");
		return FALSE;
	}

	SVM_DEBUG("SVM is available.\n");

	return TRUE;
}

/*
 * Enable SVM. (Sec 15.4, APM Vol2 r3.19)
 */
static void
svm_enable(void)
{
	/* set MSR_EFER.SVME */
	uint64_t efer;

	efer = rdmsr(MSR_EFER);
	efer |= MSR_EFER_SVME;
	wrmsr(MSR_EFER, efer);

	SVM_DEBUG("SVM is enabled.\n");
}

/*
 * Allocate one memory page for the host state-save area.
 */
static uintptr_t
alloc_hsave_area(void)
{
	pageinfo_t *pi = mem_page_alloc();

	if (pi == NULL) {
		SVM_DEBUG("Failed to allocate memory for "
			  "the host state-save area failed.\n");
		return 0x0;
	}

	return mem_pi2phys(pi);
}

/*
 * Set an interception bit in VMCB.
 *
 * @param vmcb the pointer to a VMCB
 * @param bit the interception bit to be set
 */
static void
set_intercept(struct vmcb *vmcb, int bit, bool enable)
{
	KERN_ASSERT(vmcb != NULL);
	KERN_ASSERT(INTERCEPT_INTR <= bit && bit <= INTERCEPT_XSETBV);

	if (enable == TRUE)
		vmcb->control.intercept |= (1ULL << bit);
	else
		vmcb->control.intercept &= ~(1ULL << bit);
}

static int
svm_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	uint32_t exitinfo1 = ctrl->exit_info_1;

#ifdef DEBUG_VMEXIT
	SVM_DEBUG("VMEXIT exit_code 0x%x, guest EIP 0x%08x.\n",
		  ctrl->exit_code, (uintptr_t) vmcb->save.rip);
#endif

	switch (ctrl->exit_code) {
	case SVM_EXIT_INTR:
		vm->exit_reason = EXIT_REASON_EXTINT;
		break;

	case SVM_EXIT_VINTR:
		vm->exit_reason = EXIT_REASON_INTWIN;
		ctrl->int_ctl &= ~SVM_INTR_CTRL_VIRQ;
		break;

	case SVM_EXIT_IOIO:
		vm->exit_reason = EXIT_REASON_IOPORT;
		vm->exit_info.ioport.port =
			(exitinfo1 & SVM_EXITINFO1_PORT_MASK) >>
			SVM_EXITINFO1_PORT_SHIFT;
		vm->exit_info.ioport.width =
			(exitinfo1 & SVM_EXITINFO1_SZ8) ? SZ8 :
			(exitinfo1 & SVM_EXITINFO1_SZ16) ? SZ16 : SZ32;
		vm->exit_info.ioport.write =
			(exitinfo1 & SVM_EXITINFO1_TYPE_IN) ? FALSE : TRUE;
		vm->exit_info.ioport.rep =
			(exitinfo1 & SVM_EXITINFO1_REP) ? TRUE : FALSE;
		vm->exit_info.ioport.str =
			(exitinfo1 & SVM_EXITINFO1_STR) ? TRUE : FALSE;
		break;

	case SVM_EXIT_NPF:
		vm->exit_reason = EXIT_REASON_PGFLT;
		vm->exit_info.pgflt.addr = (uintptr_t) PGADDR(ctrl->exit_info_2);
		KERN_DEBUG("NPT fault @ 0x%08x.\n", vm->exit_info.pgflt.addr);
		break;

	case SVM_EXIT_CPUID:
		vm->exit_reason = EXIT_REASON_CPUID;
		break;

	case SVM_EXIT_RDTSC:
		vm->exit_reason = EXIT_REASON_RDTSC;
		break;

	case SVM_EXIT_VMMCALL:
	case SVM_EXIT_HLT:
	case SVM_EXIT_MSR:
	case SVM_EXIT_VMRUN:
	case SVM_EXIT_VMLOAD:
	case SVM_EXIT_VMSAVE:
	case SVM_EXIT_STGI:
	case SVM_EXIT_CLGI:
	case SVM_EXIT_SKINIT:
	case SVM_EXIT_RDTSCP:
	case SVM_EXIT_MONITOR:
	case SVM_EXIT_MWAIT:
	case SVM_EXIT_MWAIT_COND:
		vm->exit_reason = EXIT_REASON_INVAL_INSTR;
		break;

	default:
		vm->exit_reason = EXIT_REASON_INVAL;
	}

	return 0;
}

/*
 * Initialize SVM module.
 * - check whether SVM is supported
 * - enable SVM
 * - allocate host save area
 *
 * @return 0 if succeed
 */
static int
svm_init(void)
{
	if (svm_pool_inited == FALSE) {
		memzero(svm_pool, sizeof(struct svm) * MAX_VMID);
		spinlock_init(&svm_pool_lk);
		svm_pool_inited = TRUE;
	}

	uintptr_t hsave_addr;

	/* check whether the processor supports SVM */
	if (svm_check() == FALSE)
		return 1;

	/* enable SVM */
	svm_enable();

	/* setup host state-save area */
	if ((hsave_addr = alloc_hsave_area()) == 0x0)
		return 1;
	else
		wrmsr(MSR_VM_HSAVE_PA, hsave_addr);

	SVM_DEBUG("Host state-save area is at %x.\n", hsave_addr);

	return 0;
}

static int
svm_init_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	int rc;
	struct svm *svm;
	pageinfo_t *vmcb_pi, *iopm_pi, *msrpm_pi, *ncr3_pi;

	/*
	 * Allocate memory.
	 */

	if ((svm = alloc_svm()) == NULL) {
		rc = -1;
		goto err0;
	}
	vm->cookie = svm;

	if ((vmcb_pi = mem_page_alloc()) == NULL) {
		rc = -2;
		goto err1;
	}
	svm->vmcb = mem_pi2ptr(vmcb_pi);
	memzero(svm->vmcb, PAGESIZE);

	if ((iopm_pi = mem_pages_alloc(SVM_IOPM_SIZE)) == NULL) {
		rc = -3;
		goto err2;
	}
	svm->vmcb->control.iopm_base_pa = mem_pi2phys(iopm_pi);

	if ((msrpm_pi = mem_pages_alloc(SVM_MSRPM_SIZE)) == NULL) {
		rc = -4;
		goto err3;
	}
	svm->vmcb->control.msrpm_base_pa = mem_pi2phys(msrpm_pi);

	if ((ncr3_pi = mem_page_alloc()) == NULL) {
		rc = -5;
		goto err4;
	}
	svm->vmcb->control.nested_cr3 = mem_pi2phys(ncr3_pi);
	memzero((void *)(uint32_t) svm->vmcb->control.nested_cr3, PAGESIZE);

	/*
	 * Setup default interception.
	 */

	/* intercept all I/O ports */
	memset((void *)(uint32_t) svm->vmcb->control.iopm_base_pa,
	       0xf, SVM_IOPM_SIZE);
	/* do not intercept any MSR */
	memzero((void *)(uint32_t) svm->vmcb->control.msrpm_base_pa,
		SVM_MSRPM_SIZE);

	/* intercept instructions */
	set_intercept(svm->vmcb, INTERCEPT_VMRUN, TRUE);	/* vmrun */
	set_intercept(svm->vmcb, INTERCEPT_VMMCALL, TRUE);	/* vmmcall */
	set_intercept(svm->vmcb, INTERCEPT_STGI, TRUE);		/* stgi */
	set_intercept(svm->vmcb, INTERCEPT_CLGI, TRUE);		/* clgi */
	set_intercept(svm->vmcb, INTERCEPT_IOIO_PROT, TRUE);	/* in/out */
	set_intercept(svm->vmcb, INTERCEPT_CPUID, TRUE);	/* cpuid */
	set_intercept(svm->vmcb, INTERCEPT_RDTSC, TRUE);	/* rdtsc */
	set_intercept(svm->vmcb, INTERCEPT_RDTSCP, TRUE);	/* rdtscp */

	/* intercept interrupts */
	set_intercept(svm->vmcb, INTERCEPT_INTR, TRUE);

	/*
	 * Setup initial state.
	 */

	svm->vmcb->save.cr0 = 0x60000010;
	svm->vmcb->save.cr2 = svm->vmcb->save.cr3 = svm->vmcb->save.cr4 = 0;
	svm->vmcb->save.dr6 = 0xffff0ff0;
	svm->vmcb->save.dr7 = 0x00000400;
	svm->vmcb->save.efer = MSR_EFER_SVME;
	svm->vmcb->save.g_pat = 0x7040600070406ULL;

	svm->vmcb->control.asid = 1;
	svm->vmcb->control.nested_ctl = 1;
	svm->vmcb->control.tlb_ctl = 0;
	svm->vmcb->control.tsc_offset = 0;
	/* Sec 15.21.1, APM Vol2 r3.19 */
	svm->vmcb->control.int_ctl =
		(SVM_INTR_CTRL_VINTR_MASK | (0x0 & SVM_INTR_CTRL_VTPR));

	return 0;

 err4:
	mem_pages_free(msrpm_pi);
 err3:
	mem_pages_free(iopm_pi);
 err2:
	mem_pages_free(vmcb_pi);
 err1:
	free_svm(svm);
 err0:
	return rc;
}

/* defined in svm_asm.S */
extern void svm_run(struct svm *);

#define save_host_segment(seg, val)					\
	__asm __volatile("mov %%" #seg ", %0" : "=r" (val) :: "memory")

#define load_host_segment(seg, val)				\
	do {							\
		uint16_t _val = (uint16_t) val;			\
		__asm __volatile("movl %k0, %%" #seg		\
				 : "+r" (_val) :: "memory");	\
	} while (0)

static int
svm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	volatile uint16_t h_fs, h_gs, h_ldt;

	SVM_CLGI();

	save_host_segment(fs, h_fs);
	save_host_segment(gs, h_gs);
	h_ldt = rldt();

	intr_local_enable();
	svm_run(svm);
	intr_local_disable();

	load_host_segment(fs, h_fs);
	load_host_segment(gs, h_gs);
	lldt(h_ldt);

	SVM_STGI();

	if (ctrl->exit_int_info & SVM_EXITINTINFO_VALID) {
		uint32_t exit_int_info = ctrl->exit_int_info;
		uint32_t errcode = ctrl->exit_int_info_err;
		uint32_t int_type = exit_int_info & SVM_EXITINTINFO_TYPE_MASK;

		switch (int_type) {
		case SVM_EXITINTINFO_TYPE_INTR:
#ifdef DEBUG_GUEST_INTR
			SVM_DEBUG("Pending INTR: vec=%x.\n",
				  exit_int_info & SVM_EXITINTINFO_VEC_MASK);
#endif
			ctrl->event_inj = exit_int_info;
			ctrl->event_inj_err = errcode;
			break;

		case SVM_EXITINTINFO_TYPE_NMI:
#ifdef DEBUG_GUEST_INTR
			SVM_DEBUG("Pending NMI.\n");
#endif
			ctrl->event_inj = exit_int_info;
			ctrl->event_inj_err = errcode;
			break;

		default:
			break;
		}
	}

	return svm_handle_exit(vm);
}

static int
svm_intercept_ioport(struct vm *vm, uint16_t port, bool enable)
{
	KERN_ASSERT(vm != NULL);

#ifdef DEBUG_GUEST_IOPORT
	SVM_DEBUG("%s intercepting guest I/O port 0x%x.\n",
		  (enable == TRUE) ? "Enable" : "Disable", port);
#endif

	struct svm *svm = (struct svm *) vm->cookie;
	uint32_t *iopm = (uint32_t *)(uintptr_t) svm->vmcb->control.iopm_base_pa;

	int entry = port / 32;
	int bit = port - entry * 32;

	if (enable == TRUE)
		iopm[entry] |= (1 << bit);
	else
		iopm[entry] &= ~(uint32_t) (1 << bit);

	return 0;
}

static int
svm_intercept_msr(struct vm *vm, uint32_t msr, int rw)
{
	KERN_PANIC("svm_intercept_msr() not implemented yet.\n");
	return 1;
}

static void
svm_inject_vintr(struct vmcb *vmcb, uint8_t vector, uint8_t priority)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_control_area *ctrl  = &vmcb->control;

	ctrl->int_ctl |= SVM_INTR_CTRL_VIRQ |
		((priority << SVM_INTR_CTRL_PRIO_SHIFT) & SVM_INTR_CTRL_PRIO) |
		SVM_INTR_CTRL_IGN_VTPR;
	ctrl->int_vector = vector;
}

static int
svm_intercept_vintr(struct vm *vm, bool enable)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	if (enable == TRUE) {
		svm_inject_vintr(vmcb, 0, 0);
		set_intercept(vmcb, INTERCEPT_VINTR, TRUE);
	} else {
		set_intercept(vmcb, INTERCEPT_VINTR, FALSE);
	}

	return 0;
}

static int
svm_get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (reg) {
	case GUEST_EAX:
		*val = (uint32_t) save->rax;
		break;
	case GUEST_EBX:
		*val = (uint32_t) svm->g_rbx;
		break;
	case GUEST_ECX:
		*val = (uint32_t) svm->g_rcx;
		break;
	case GUEST_EDX:
		*val = (uint32_t) svm->g_rdx;
		break;
	case GUEST_ESI:
		*val = (uint32_t) svm->g_rsi;
		break;
	case GUEST_EDI:
		*val = (uint32_t) svm->g_rdi;
		break;
	case GUEST_EBP:
		*val = (uint32_t) svm->g_rbp;
		break;
	case GUEST_ESP:
		*val = (uint32_t) save->rsp;
		break;
	case GUEST_EIP:
		*val = (uint32_t) save->rip;
		break;
	case GUEST_EFLAGS:
		*val = (uint32_t) save->rflags;
		break;
	case GUEST_CR0:
		*val = (uint32_t) save->cr0;
		break;
	case GUEST_CR2:
		*val = (uint32_t) save->cr2;
		break;
	case GUEST_CR3:
		*val = (uint32_t) save->cr3;
		break;
	case GUEST_CR4:
		*val = (uint32_t) save->cr4;
		break;
	default:
		return 1;
	}

	return 0;
}

static int
svm_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (reg) {
	case GUEST_EAX:
		save->rax = val;
		break;
	case GUEST_EBX:
		svm->g_rbx = val;
		break;
	case GUEST_ECX:
		svm->g_rcx = val;
		break;
	case GUEST_EDX:
		svm->g_rdx = val;
		break;
	case GUEST_ESI:
		svm->g_rsi = val;
		break;
	case GUEST_EDI:
		svm->g_rdi = val;
		break;
	case GUEST_EBP:
		svm->g_rbp = val;
		break;
	case GUEST_ESP:
		save->rsp = val;
		break;
	case GUEST_EIP:
		save->rip = val;
		break;
	case GUEST_EFLAGS:
		save->rflags = val;
		break;
	case GUEST_CR0:
		save->cr0 = val;
		break;
	case GUEST_CR2:
		save->cr2 = val;
		break;
	case GUEST_CR3:
		save->cr3 = val;
		break;
	case GUEST_CR4:
		save->cr4 = val;
		break;
	default:
		return 1;
	}

	return 0;
}

static uintptr_t seg_offset[10] = {
	[GUEST_CS]	= offsetof(struct vmcb_save_area, cs),
	[GUEST_DS]	= offsetof(struct vmcb_save_area, ds),
	[GUEST_ES]	= offsetof(struct vmcb_save_area, es),
	[GUEST_FS]	= offsetof(struct vmcb_save_area, fs),
	[GUEST_GS]	= offsetof(struct vmcb_save_area, gs),
	[GUEST_SS]	= offsetof(struct vmcb_save_area, ss),
	[GUEST_LDTR]	= offsetof(struct vmcb_save_area, ldtr),
	[GUEST_TR]	= offsetof(struct vmcb_save_area, tr),
	[GUEST_GDTR]	= offsetof(struct vmcb_save_area, gdtr),
	[GUEST_IDTR]	= offsetof(struct vmcb_save_area, idtr),
};

static int
svm_get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(desc != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;
	struct vmcb_seg *vmcb_seg =
		(struct vmcb_seg *) ((uintptr_t) save + seg_offset[seg]);

	desc->sel = vmcb_seg->selector;
	desc->base = vmcb_seg->base;
	desc->lim = vmcb_seg->limit;
	desc->ar =
		(vmcb_seg->attrib & 0xff) | ((vmcb_seg->attrib & 0xf00) << 4);

	return 0;
}

static int
svm_set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(desc != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;
	struct vmcb_seg *vmcb_seg =
		(struct vmcb_seg *) ((uintptr_t) save + seg_offset[seg]);

	vmcb_seg->selector = desc->sel;
	vmcb_seg->base = desc->base;
	vmcb_seg->limit = desc->lim;
	vmcb_seg->attrib = (desc->ar & 0xff) | ((desc->ar & 0xf000) >> 4);

	return 0;
}

static int
svm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type)
{
	KERN_ASSERT(vm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa || ROUNDDOWN(hpa, PAGESIZE) != hpa)
		return 1;

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;
	pmap_t *npt = (pmap_t *)(uintptr_t) ctrl->nested_cr3;

	npt = pmap_insert(npt, mem_phys2pi(hpa), gpa, PTE_W | PTE_G | PTE_U);

	return (npt == NULL) ? 2 : 0;
}

static int svm_event_type[4] = {
	[EVENT_EXTINT]		= SVM_EVTINJ_TYPE_INTR,
	[EVENT_NMI]		= SVM_EVTINJ_TYPE_NMI,
	[EVENT_EXCEPTION]	= SVM_EVTINJ_TYPE_EXEPT,
	[EVENT_SWINT]		= SVM_EVTINJ_TYPE_SOFT
};

static int
svm_inject_event(struct vm *vm,
		 guest_event_t type, uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		return 1;

	ctrl->event_inj = SVM_EVTINJ_VALID | (vector & SVM_EVTINJ_VEC_MASK) |
		((svm_event_type[type] << SVM_EVTINJ_TYPE_SHIFT) &
		 SVM_EVTINJ_TYPE_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == TRUE)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;

	return 0;
}

static int
svm_get_next_eip(struct vm *vm, guest_instr_t instr, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;
	struct vmcb_save_area *save = &svm->vmcb->save;

	switch (instr) {
	case INSTR_IN:
	case INSTR_OUT:
		*val = (uint32_t) ctrl->exit_info_2;
		break;
	case INSTR_RDMSR:
	case INSTR_WRMSR:
	case INSTR_CPUID:
	case INSTR_RDTSC:
		*val = (uint32_t) save->rip + 2;
		break;
	case INSTR_HYPERCALL:
		*val = (uint32_t) save->rip + 3;
		break;
	default:
		return 1;
	}

	return 0;
}

static bool
svm_pending_event(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	return (ctrl->event_inj & SVM_EVTINJ_VALID) ? TRUE : FALSE;
}

static bool
svm_intr_shadow(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	return (ctrl->int_state & 0x1) ? TRUE : FALSE;
}

struct hvm_ops hvm_ops_amd = {
	.signature		= AMD_SVM,
	.hw_init		= svm_init,
	.intercept_ioport	= svm_intercept_ioport,
	.intercept_msr		= svm_intercept_msr,
	.intercept_intr_window	= svm_intercept_vintr,
	.vm_init		= svm_init_vm,
	.vm_run			= svm_run_vm,
	.get_reg		= svm_get_reg,
	.set_reg		= svm_set_reg,
	.get_desc		= svm_get_desc,
	.set_desc		= svm_set_desc,
	.set_mmap		= svm_set_mmap,
	.inject_event		= svm_inject_event,
	.get_next_eip		= svm_get_next_eip,
	.pending_event		= svm_pending_event,
	.intr_shadow		= svm_intr_shadow,
};

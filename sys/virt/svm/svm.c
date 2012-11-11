#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>

#include <machine/pmap.h>

#include <dev/kbd.h>
#include <dev/lapic.h>
#include <dev/pic.h>

#include "svm.h"
#include "svm_utils.h"

static struct {
	struct svm 	svm[MAX_VMID];
	bool		used[MAX_VMID];
} svm_pool;

static spinlock_t svm_pool_lk;

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
		KERN_DEBUG("The processor does not support SVM.\n");
		return FALSE;
	}

	/* check MSR VM_CR */
	if ((rdmsr(MSR_VM_CR) & MSR_VM_CR_SVMDIS) == 0) {
		return TRUE;
	}

	/* check CPUID 0x8000000a */
	cpuid(CPUID_SVM_FEATURE_FUNC, &dummy, &dummy, &dummy, &feature);
	if ((feature & CPUID_SVM_LOCKED) == 0) {
		KERN_DEBUG("SVM maybe disabled by BIOS.\n");
		return FALSE;
	} else {
		KERN_DEBUG("SVM maybe disabled with key.\n");
		return FALSE;
	}

	KERN_DEBUG("SVM is available.\n");

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

	KERN_DEBUG("SVM is enabled.\n");
}

/*
 * Allocate one memory page for the host state-save area.
 */
static uintptr_t
alloc_hsave_area(void)
{
	pageinfo_t *pi = mem_page_alloc();

	if (pi == NULL) {
		KERN_DEBUG("Failed to allocate memory for "
			   "the host state-save area failed.\n");
		return 0x0;
	}

	return mem_pi2phys(pi);
}

static struct svm *
alloc_svm(void)
{
	int i;
	struct svm *svm = NULL;

	spinlock_acquire(&svm_pool_lk);

	for (i = 0; i < MAX_VMID; i++)
		if (svm_pool.used[i] == FALSE)
			break;

	if (i < MAX_VMID) {
		svm_pool.used[i] = TRUE;
		svm = &svm_pool.svm[i];
		memzero(svm, sizeof(struct svm));
	}

	spinlock_release(&svm_pool_lk);

	return svm;
}

static void
free_svm(struct svm *svm)
{
	uintptr_t offset;

	if (svm < svm_pool.svm)
		return;

	offset = (uintptr_t) svm - (uintptr_t) svm_pool.svm;
	if (offset % sizeof(struct svm) ||
	    offset / sizeof(struct svm) >= MAX_VMID)
	    return;

	spinlock_acquire(&svm_pool_lk);
	svm_pool.used[svm - svm_pool.svm] = FALSE;
	spinlock_release(&svm_pool_lk);
}

/*
 * Allocate one memory page for VMCB.
 *
 * @return the pointer to VMCB if succeed; otherwise NULL.
 */
static struct vmcb *
alloc_vmcb(void)
{
	pageinfo_t *pi = mem_page_alloc();

	if (pi == NULL) {
		KERN_DEBUG("Failed to allocate memory for VMCB.\n");
		return NULL;
	}

	memset((struct vmcb *) mem_pi2phys(pi), 0x0, sizeof(struct vmcb));

	return (struct vmcb *) mem_pi2phys(pi);
}

static void
free_vmcb(struct vmcb *vmcb)
{
	KERN_ASSERT(vmcb != NULL);
	mem_page_free(mem_ptr2pi(vmcb));
}

/*
 * Create the nestd page table.
 * - identically maps the lowest 1MB guest physical memory
 *
 * @return the pointer to the page table if succeed; otherwise NULL.
 */
static pmap_t *
alloc_nested_ptable(size_t memsize)
{
	uintptr_t addr;
	pageinfo_t *pmap_pi;
	pmap_t *pmap;

	if ((pmap_pi = mem_page_alloc()) == NULL) {
		KERN_DEBUG("Cannot allocate memory for nested page table.\n");
		return NULL;
	}
	pmap = mem_pi2ptr(pmap_pi);

	for (addr = 0x0; addr < memsize; addr += PAGESIZE) {
		if (addr >= 0xa0000 && addr <= 0xbffff) {
			/* identically map VGA display memory to the host */
			if (pmap_insert(pmap, mem_phys2pi(addr),
					addr, PTE_G | PTE_W | PTE_U) == NULL)
				KERN_PANIC("Cannot map guest physical address "
					   "0x%08x.\n", addr);
		} else if (pmap_reserve(pmap, addr,
					PTE_G | PTE_W | PTE_U) == NULL) {
			KERN_DEBUG("Failed to map guest memory page at %x.\n",
				   addr);
			pmap_free(pmap);
			return NULL;
		}

		if (addr == 0xfffff000)
			break;
	}

	return pmap;
}

static void
free_nested_ptable(pmap_t *pmap)
{
	KERN_PANIC("Not implemented yet.\n");
}

/*
 * Create an permission map. The permission map is aligned to the
 * boundary of the physical memory page and occupies continuous
 * ROUNDUP(size, PAGESIZE) bytes of physical memory.
 *
 * @param size the least size of the permission map in bytes
 *
 * @return the 64-bit address of the permission map if succeed;
 *         otherwise, 0x0.
 */
static uint64_t
alloc_perm_map(size_t size)
{
	pageinfo_t *pi = mem_pages_alloc(size);

	if (pi == NULL)
		return 0;

	uintptr_t addr = mem_pi2phys(pi);
	memzero((uint8_t *) addr, size);

	return (uint64_t) addr;
}

static void
free_perm_map(uint64_t map_addr)
{
	mem_pages_free(mem_phys2pi((uintptr_t) map_addr));
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
svm_handle_err(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;
	struct vmcb_control_area *ctrl = &vmcb->control;

	/* Sec 3.1.7, APM Vol2 r3.19 */
	uint64_t efer = save->efer;
	if ((efer & MSR_EFER_SVME) == 0)
		KERN_WARN("EFER.SVME != 0\n");
	if ((efer >> 32) != 0ULL)
		KERN_WARN("EFER[63..32] != 0\n");
	if ((efer >> 15) != 0ULL)
		KERN_WARN("EFER[31..15] != 0\n");
	if ((efer & 0xfe) != 0ULL)
		KERN_WARN("EFER[7..1] != 0\n");

	uint64_t cr0 = save->cr0;
	if ((cr0 & CR0_CD) == 0 && (cr0 & CR0_NW))
		KERN_WARN("CR0.CD = 0 && CR0.NW = 1\n");
	if ((cr0 >> 32) != 0ULL)
		KERN_WARN("CR0[63..32] != 0\n");

	/*
	 * TODO: check the MBZ bits of CR3 according to the actual mode
	 *       CertiKOS and/or VM is running in. Current version CertiKOS
	 *       and VM are using the legacy-mode non-PAE paging, so there
	 *       is no MBZ bits in CR3. (Sec 3.1.2, APM Vol2 r3.19)
	 */

	/* Sec 3.1.3, APM Vol2 r3.19 */
	uint64_t cr4 = save->cr4;
	if ((cr4 >> 32) != 0ULL)
		KERN_WARN("CR4[63..32] != 0\n");
	if ((cr4 >> 19) != 0ULL)
		KERN_WARN("CR4[31..19] != 0\n");
	if (((cr4 >> 11) & 0x7fULL) != 0ULL)
		KERN_WARN("CR4[17..11] != 0\n");

	if ((save->dr6 >> 32) != 0ULL)
		KERN_WARN("DR6[63..32] != 0\n");

	if ((save->dr7 >> 32) != 0ULL)
		KERN_WARN("DR7[63..32] != 0\n");

	if (((efer & MSR_EFER_LMA) || (efer & MSR_EFER_LME)) &&
	    (pcpu_cur()->arch_info.feature2 & CPUID_X_FEATURE_LM) == 0)
		KERN_WARN("EFER.LMA =1 or EFER.LME = 1, "
			  "while long mode is not supported.\n");
	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) == 0)
		KERN_WARN("EFER.LME = 1 and CR0.PG =1, while CR4.PAE = 0.\n");

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr0 & CR0_PE) == 0)
		KERN_WARN("EFER.LME = 1 and CR0.PG =1, while CR0.PE = 0.\n");

	uint16_t cs_attrib = save->cs.attrib;

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) &&
	    (cs_attrib & SEG_ATTR_L) && (cs_attrib & SEG_ATTR_D))
		KERN_WARN("EFER.LME, CR0.PG, CR4.PAE, CS.L and CS.D "
			  "are non-zero.\n");

	if ((ctrl->intercept & (1ULL << INTERCEPT_VMRUN)) == 0)
		KERN_WARN("VMRUN is not intercepted.\n");

	if (ctrl->iopm_base_pa > mem_max_phys())
		KERN_WARN("The address of IOPM (%x) is out of range of "
			  "the physical memory.\n",
			  ctrl->iopm_base_pa);

	if (ctrl->msrpm_base_pa > mem_max_phys())
		KERN_WARN("The address of MSRPM (%x) is out of range of "
			  "the physical memory.\n",
			  ctrl->msrpm_base_pa);

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		KERN_WARN("Illegal event injection.\n");

	if (ctrl->asid == 0)
		KERN_WARN("ASID = 0\n");

	return 0;
}

static int
svm_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	uint32_t exitinfo1 = ctrl->exit_info_1;

	vm->exit_handled = FALSE;

	switch (ctrl->exit_code) {
	case SVM_EXIT_INTR:
		vm->exit_reason = EXIT_FOR_EXTINT;
		break;

	case SVM_EXIT_VINTR:
		vm->exit_reason = EXIT_FOR_INTWIN;
		ctrl->int_ctl &= ~SVM_INTR_CTRL_VIRQ;
		break;

	case SVM_EXIT_IOIO:
		vm->exit_reason = EXIT_FOR_IOPORT;
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
		vm->exit_reason = EXIT_FOR_PGFLT;
		vm->exit_info.pgflt.addr = (uintptr_t) PGADDR(ctrl->exit_info_2);
		break;

	case SVM_EXIT_CPUID:
		vm->exit_reason = EXIT_FOR_CPUID;
		break;

	case SVM_EXIT_RDTSC:
		vm->exit_reason = EXIT_FOR_RDTSC;
		break;

	case SVM_EXIT_VMMCALL:
		vm->exit_reason = EXIT_FOR_HYPERCALL;
		break;

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
		vm->exit_reason = EXIT_FOR_INVAL_INSTR;
		break;

	default:
		vm->exit_reason = EXIT_INVAL;
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

	KERN_DEBUG("Host state-save area is at %x.\n", hsave_addr);

	return 0;
}

static int
svm_init_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	int rc;
	struct svm *svm;
	struct vmcb *vmcb;
	struct vmcb_save_area *save;
	struct vmcb_control_area *control;
	pmap_t *ncr3;

	if ((svm = alloc_svm()) == NULL) {
		rc = 1;
		goto err_ret;
	}

	vm->cookie = svm;

	/* allocate memory for VMCB */
	if ((svm->vmcb = alloc_vmcb()) == NULL) {
		rc = 2;
		goto vmcb_err;
	}
	KERN_DEBUG("VMCB is at 0x%08x.\n", svm->vmcb);

	vmcb = svm->vmcb;
	save = &vmcb->save;
	control = &vmcb->control;

	/* setup CRs, DRs, and MSRs */
	save->cr0 = 0x60000010;
	save->cr2 = save->cr3 = save->cr4 = 0;
	save->dr6 = 0xffff0ff0;
	save->dr7 = 0x00000400;
	save->efer = MSR_EFER_SVME;
	save->g_pat = 0x7040600070406ULL;

	/* setup the nested page table */
	if ((ncr3 = alloc_nested_ptable(vm->memsize)) == NULL) {
		rc = 3;
		goto npt_err;
	}
	control->nested_cr3 = (uintptr_t) ncr3;
	KERN_DEBUG("Nested page table is at 0x%08x.\n", ncr3);

	/* create IOPM */
	if (!(control->iopm_base_pa = alloc_perm_map(SVM_IOPM_SIZE))) {
		rc = 4;
		goto iopm_err;
	}

	/* create MSRPM */
	if (!(control->msrpm_base_pa = alloc_perm_map(SVM_MSRPM_SIZE))) {
		rc = 5;
		goto msrpm_err;
	}

	/* enable intercepting a selected set of instructions */
	set_intercept(vmcb, INTERCEPT_VMRUN, TRUE);
	set_intercept(vmcb, INTERCEPT_VMMCALL, TRUE);
	set_intercept(vmcb, INTERCEPT_STGI, TRUE);
	set_intercept(vmcb, INTERCEPT_CLGI, TRUE);
	set_intercept(vmcb, INTERCEPT_IOIO_PROT, TRUE);
	set_intercept(vmcb, INTERCEPT_CPUID, TRUE);
	set_intercept(vmcb, INTERCEPT_RDTSC, TRUE);
	set_intercept(vmcb, INTERCEPT_RDTSCP, TRUE);

	/* enable intercepting interrupts */
	set_intercept(vmcb, INTERCEPT_INTR, TRUE);

	/* miscellenea initialization */
	control->asid = 1;
	control->nested_ctl = 1;
	control->tlb_ctl = 0;
	control->tsc_offset = 0;
	/* Sec 15.21.1, APM Vol2 r3.19 */
	control->int_ctl =
		(SVM_INTR_CTRL_VINTR_MASK | (0x0 & SVM_INTR_CTRL_VTPR));

	return 0;

 /* err: */
 /* 	free_perm_map(control->msrpm_base_pa); */
 msrpm_err:
	free_perm_map(control->iopm_base_pa);
 iopm_err:
	free_nested_ptable((pmap_t *)(uintptr_t) control->nested_cr3);
 npt_err:
	free_vmcb(svm->vmcb);
 vmcb_err:
	free_svm(svm);
 err_ret:
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
	int rc = 0;

	SVM_CLGI();

	save_host_segment(fs, svm->h_fs);
	save_host_segment(gs, svm->h_gs);
	svm->h_ldt = rldt();

	intr_local_enable();
	svm_run(svm);
	intr_local_disable();

	load_host_segment(fs, svm->h_fs);
	load_host_segment(gs, svm->h_gs);
	lldt(svm->h_ldt);

	if (ctrl->exit_int_info & SVM_EXITINTINFO_VALID) {
		uint32_t exit_int_info = ctrl->exit_int_info;
		uint32_t errcode = ctrl->exit_int_info_err;
		uint32_t int_type = exit_int_info & SVM_EXITINTINFO_TYPE_MASK;

		switch (int_type) {
		case SVM_EXITINTINFO_TYPE_INTR:
#ifdef DEBUG_GUEST_INTR
			KERN_DEBUG("Pending INTR: vec=%x.\n",
				   exit_int_info & SVM_EXITINTINFO_VEC_MASK);
#endif
			ctrl->event_inj = exit_int_info;
			ctrl->event_inj_err = errcode;
			break;

		case SVM_EXITINTINFO_TYPE_NMI:
#ifdef DEBUG_GUEST_INTR
			KERN_DEBUG("Pending NMI.\n");
#endif
			ctrl->event_inj = exit_int_info;
			ctrl->event_inj_err = errcode;
			break;

		case SVM_EXITINTINFO_TYPE_EXEPT:
#ifdef DEBUG_GUEST_INTR
			KERN_DEBUG("Pending exception: vec=%x, errcode=%x.\n",
				   exit_int_info & SVM_EXITINTINFO_VEC_MASK,
				   errcode);
#endif
			break;

		case SVM_EXITINTINFO_TYPE_SOFT:
#ifdef DEBUG_GUEST_INTR
			KERN_DEBUG("Pending soft INTR.\n");
#endif
			break;

		default:
			KERN_PANIC("Invalid event type: %x.\n", int_type);
		}
	}

	if (ctrl->exit_code == SVM_EXIT_ERR) {
		vm->exit_reason = EXIT_INVAL;
		svm_handle_err(vm);
		rc = 1;
		goto ret;
	}

	rc = svm_handle_exit(vm);

 ret:
	SVM_STGI();
	return rc;
}

static int
svm_intercept_ioport(struct vm *vm, uint16_t port, bool enable)
{
	KERN_ASSERT(vm != NULL);

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
svm_intercept_all_ioports(struct vm *vm, bool enable)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	uint32_t *iopm = (uint32_t *)(uintptr_t) svm->vmcb->control.iopm_base_pa;

	if (enable == TRUE)
		memset(iopm, 0xf, SVM_IOPM_SIZE);
	else
		memset(iopm, 0x0, SVM_IOPM_SIZE);

	return 0;
}

static int
svm_intercept_msr(struct vm *vm, uint32_t msr, int rw)
{
	KERN_PANIC("svm_intercept_msr() not implemented yet.\n");
	return 1;
}

static int
svm_intercept_all_msrs(struct vm *vm, int rw)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(rw == 0 || rw == 1 || rw == 2 || rw == 3);

	struct svm *svm = (struct svm *) vm->cookie;
	uint8_t *msrpm = (uint8_t *)(uintptr_t) svm->vmcb->control.msrpm_base_pa;
	uint8_t val = rw;
	val = (rw << 6) | (rw << 4) | (rw << 2) | rw;

	memset(msrpm, val, SVM_MSRPM_SIZE);

	return 0;
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

static int
svm_get_msr(struct vm *vm, uint32_t msr, uint64_t *val)
{
	KERN_PANIC("svm_get_msr() not implemented yet.\n");
	return 1;
}

static int
svm_set_msr(struct vm *vm, uint32_t msr, uint64_t val)
{
	KERN_PANIC("svm_set_msr() not implemented yet.\n");
	return 1;
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
svm_get_mmap(struct vm *vm, uintptr_t gpa, uintptr_t *hpa)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hpa != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa)
		return 1;

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	*hpa = glinear_2_gphysical(vmcb, gpa);

	return 0;
}

static int
svm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa)
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

static int
svm_unset_mmap(struct vm *vm, uintptr_t gpa)
{
	KERN_PANIC("svm_unset_mmap() not implemented yet.\n");
	return 1;
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
svm_get_next_eip(struct vm *vm, instr_t instr, uint32_t *val)
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

static int
svm_get_cpuid(struct vm *vm, uint32_t in_eax, uint32_t in_ecx,
	      uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(eax != NULL && ebx != NULL && ecx != NULL && edx != NULL);

	switch (in_eax) {
	case 0x40000000:
		/* 0x40000000 ~ 0x400000ff are reserved for the hypervisor. */
		*ebx = 0x74726543;	/* "treC" */
		*ecx = 0x534f4b69;	/* "SOKi" */
		break;

	case 0x00000001:
		cpuid(in_eax, eax, ebx, ecx, edx);
		*eax = (6 << 8) | (2 << 4) | (3);
		*ebx = /* only 1 processor/core */
			(*ebx & ~(uint32_t) (0xf << 16)) | (uint32_t) (1 << 16);
		*ecx = *ecx & ~(uint32_t)
			(CPUID_FEATURE_AVX | CPUID_FEATURE_AES |
			 CPUID_FEATURE_MONITOR);
		*edx = *edx & ~(uint32_t)
			(CPUID_FEATURE_HTT | CPUID_FEATURE_MCA |
			 CPUID_FEATURE_MTRR | CPUID_FEATURE_APIC |
			 CPUID_FEATURE_MCE | CPUID_FEATURE_MSR |
			 CPUID_FEATURE_DE);
		break;

	case 0x80000001:
		cpuid(in_eax, eax, ebx, ecx, edx);
		*eax = (6 << 8) | (2 << 4) | (3);
		*ecx = *ecx & ~(uint32_t)
			(CPUID_X_FEATURE_WDT | CPUID_X_FEATURE_SKINIT |
			 CPUID_X_FEATURE_XAPIC | CPUID_X_FEATURE_SVM);
		*edx = *edx & ~(uint32_t)
			(CPUID_X_FEATURE_RDTSCP | CPUID_X_FEATURE_NX |
			 CPUID_X_FEATURE_MCA | CPUID_X_FEATURE_MTRR |
			 CPUID_X_FEATURE_APIC | CPUID_X_FEATURE_MCE |
			 CPUID_X_FEATURE_MSR | CPUID_X_FEATURE_DE);
		break;

	default:
		cpuid(in_eax, eax, ebx, ecx, edx);
		break;
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

struct vmm_ops vmm_ops_amd = {
	.signature		= AMD_SVM,
	.hw_init		= svm_init,
	.intercept_ioport	= svm_intercept_ioport,
	.intercept_all_ioports	= svm_intercept_all_ioports,
	.intercept_msr		= svm_intercept_msr,
	.intercept_all_msrs	= svm_intercept_all_msrs,
	.intercept_intr_window	= svm_intercept_vintr,
	.vm_init		= svm_init_vm,
	.vm_run			= svm_run_vm,
	.get_reg		= svm_get_reg,
	.set_reg		= svm_set_reg,
	.get_msr		= svm_get_msr,
	.set_msr		= svm_set_msr,
	.get_desc		= svm_get_desc,
	.set_desc		= svm_set_desc,
	.get_mmap		= svm_get_mmap,
	.set_mmap		= svm_set_mmap,
	.unset_mmap		= svm_unset_mmap,
	.inject_event		= svm_inject_event,
	.get_next_eip		= svm_get_next_eip,
	.get_cpuid		= svm_get_cpuid,
	.pending_event		= svm_pending_event,
	.intr_shadow		= svm_intr_shadow,
};

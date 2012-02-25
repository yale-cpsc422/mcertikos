#include <kern/as/as.h>
#include <kern/debug/debug.h>
#include <kern/debug/kbd.h>
#include <kern/mem/mem.h>
#include <kern/pmap/pmap.h>

#include <architecture/apic.h>
#include <architecture/cpuid.h>
#include <architecture/intr.h>
#include <architecture/mem.h>
#include <architecture/pic.h>
#include <architecture/pic_internal.h>
#include <architecture/types.h>
#include <architecture/x86.h>

#include <kern/hvm/vmm.h>
#include <kern/hvm/vmm_iodev.h>
#include <kern/hvm/dev/kbd.h>
#include <kern/hvm/dev/pci.h>
#include <kern/hvm/dev/pic.h>

#include <kern/hvm/svm/svm.h>
#include <kern/hvm/svm/svm_handle.h>
#include <kern/hvm/svm/svm_utils.h>

struct {
	struct svm 	svms[4];
	bool		used[4];
} svm_info;

/*
 * data allocator.
 */
static struct svm *
alloc_svm(void)
{
	int i;

	for (i = 0; i < 4; i++)
		if (svm_info.used[i] == false)
			break;

	if (i == 4)
		return NULL;
	else {
		memset(&svm_info.svms[i], 0x0, sizeof(struct svm));
		svm_info.used[i] = true;
		return &svm_info.svms[i];
	}
}

/*
 * Check whether the machine supports SVM. (Sec 15.4, APM Vol2 r3.19)
 *
 * @return true if SVM is supported; otherwise false
 */
static bool
svm_check(void)
{
	/* check CPUID 0x80000001 */
	uint32_t feature, dummy;
	cpuid(CPUID_FEATURE_FUNC, &dummy, &dummy, &feature, &dummy);
	if ((feature & CPUID_X_FEATURE_SVM) == 0) {
		/* debug("The processor does not support SVM.\n"); */
		return false;
	}

	/* check MSR VM_CR */
	if ((rdmsr(MSR_VM_CR) & MSR_VM_CR_SVMDIS) == 1) {
		/* debug("SVM is disabled.\n"); */
		return false;
	}

	/* check CPUID 0x8000000a */
	cpuid(CPUID_SVM_FEATURE_FUNC, &dummy, &dummy, &dummy, &feature);
	if ((feature & CPUID_SVM_LOCKED) == 0) {
		/* debug("SVM maybe disabled by BIOS.\n"); */
		return false;
	}

	/* debug("SVM is available.\n"); */

	return true;
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

	/* debug("SVM is enabled.\n"); */
}

/*
 * Allocate one memory page for the host state-save area.
 */
static uintptr_t
alloc_hsave_area(void)
{
	pageinfo *pi = mem_alloc();

	if (pi == NULL) {
		debug("Failed to allocate memory for the host state-save area failed.\n");
		return 0x0;
	}

	return mem_pi2phys(pi);
}


/*
 * Allocate one memory page for VMCB.
 *
 * @return the pointer to VMCB if succeed; otherwise NULL.
 */
static struct vmcb *
alloc_vmcb(void)
{
	pageinfo *pi = mem_alloc();

	if (pi == NULL) {
		debug("Failed to allocate memory for VMCB.\n");
		return NULL;
	}

	memset((struct vmcb *) mem_pi2phys(pi), 0x0, sizeof(struct vmcb));

	return (struct vmcb *) mem_pi2phys(pi);
}

/*
 * Create the nestd page table.
 * - identically maps the lowest 1MB guest physical memory
 * - identically maps the memory hole above 0xf0000000
 *
 * TODO: This function breaks the boundary of as module and pmap module. Try to
 *       alter all pmap APIs with as APIs.
 *
 * @return the pointer to the page table if succeed; otherwise NULL.
 */
static pmap_t *
alloc_nested_ptable(void)
{
	uintptr_t addr;
	pmap_t *pmap = pmap_new();

	if (pmap == NULL) {
		debug("Failed to allocate memory for nested page table.\n");
		return NULL;
	}

	for (addr = 0x0; addr < VM_PHY_MEMORY_SIZE; addr += PAGESIZE) {
		if (addr >= 0xa0000 && addr <= 0xbffff) {
			/* identically map VGA display memory to the host */
			as_assign((as_t *) pmap, addr, PTE_G | PTE_W | PTE_U,
				  mem_phys2pi(addr));
		} else if (as_reserve((as_t *) pmap, addr,
				      PTE_G | PTE_W | PTE_U) == NULL) {
			debug("Failed to map guest memory page at %x.\n",
			      addr);
			pmap_free(pmap);
			return NULL;
		}
	}

	return pmap;
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
alloc_permission_map(size_t size)
{
	uintptr_t addr = alloc_host_pages(size/PAGESIZE, 1);

	if (addr == 0x0)
		return 0;
	else
		addr = addr << PAGE_SHIFT;

	memset((uint8_t *) addr, 0, size);

	return (uint64_t) addr;
}

/*
 * Enable/Disable intercept an I/O port.
 *
 * @param vmcb
 * @param port the I/O port to be intercepted
 * @param enable true to enable intercept, false to disable intercept
 */
void
set_intercept_ioio(struct vmcb *vmcb, uint32_t port, bool enable)
{
	assert(vmcb != NULL);

	uint32_t *iopm = (uint32_t *)(uintptr_t) vmcb->control.iopm_base_pa;

	int entry = port / 32;
	int bit = port - entry * 32;

	if (enable)
		iopm[entry] |= (1 << bit);
	else
		iopm[entry] &= ~(1 << bit);
}

/*
 * Enable/Disable intercept reading a MSR.
 *
 * @param vmcb
 * @param msr the MSR to be intercepted
 * @param enable true to enable intercept, false to disable intercept
 */
void
set_intercept_rdmsr(struct vmcb *vmcb, uint64_t msr, bool enable)
{
	panic("Not implemented yet.\n");
}

/*
 * Enable/Disable intercept writing a MSR.
 *
 * @param vmcb
 * @param msr the MSR to be intercepted
 * @param enable true to enable intercept, false to disable intercept
 */
void
set_intercept_wrmsr(struct vmcb *vmcb, uint64_t msr, bool enable)
{
	panic("Not implemented yet.\n");
}

/*
 * Set/Unset an intercept bit in VMCB.
 *
 * @param vmcb the pointer to a VMCB
 * @param bit the intercept bit to be set
 * @param enable true to enable intercept, false to disable intercept
 */
void
set_intercept(struct vmcb *vmcb, int bit, bool enable)
{
	assert(vmcb != NULL);
	assert(INTERCEPT_INTR <= bit && bit <= INTERCEPT_XSETBV);

	if (enable == true)
		vmcb->control.intercept |= (1ULL << bit);
	else
		vmcb->control.intercept &= ~(1ULL << bit);
}

/*
 * Enable/Disable intercept an exception.
 *
 * @param vmcb
 * @param bit the vector number of the exception
 * @param enable true to enable intercept, false to disable intercept
 */
void
set_intercept_exception(struct vmcb *vmcb, int bit, bool enable)
{
	assert(vmcb != NULL);
	assert(0 <= bit && bit < 32);

	if (enable == true)
		vmcb->control.intercept_exceptions |= (1UL << bit);
	else
		vmcb->control.intercept_exceptions &= ~(1UL << bit);
}

/*
 * Setup the intercept.
 *
 * @param vmcb the pointer to a VMCB
 *
 * @return 0 if succeed; otherwise, 1.
 */
static int
setup_intercept(struct vmcb *vmcb)
{
	assert(vmcb != NULL);

	/* create IOPM */
	vmcb->control.iopm_base_pa = alloc_permission_map(SVM_IOPM_SIZE);

	if (vmcb->control.iopm_base_pa == 0x0) {
		debug("Failed to create IOPM.\n");
		return 1;
	}

	debug("IOPM is at %x.\n", vmcb->control.iopm_base_pa);

	/* create MSRPM */
	vmcb->control.msrpm_base_pa = alloc_permission_map(SVM_MSRPM_SIZE);

	if (vmcb->control.msrpm_base_pa == 0x0) {
		debug("Failed to create MSRPM.\n");
		return 1;
	}

	debug("MSRPM is at %x.\n", vmcb->control.msrpm_base_pa);

	/* setup IOIO intercept */
#if 1
	/* intercept i8259 */
	set_intercept_ioio(vmcb, IO_PIC1, true);
	set_intercept_ioio(vmcb, IO_PIC1+1, true);
	set_intercept_ioio(vmcb, IO_PIC2, true);
	set_intercept_ioio(vmcb, IO_PIC2+1, true);
#endif

#if 1
	/* intercept PCI */
	set_intercept_ioio(vmcb, PCI_CMD_PORT, true);
	set_intercept_ioio(vmcb, PCI_DATA_PORT, true);
#endif

#if 1
	/* intercept keyboard */
	set_intercept_ioio(vmcb, KBSTATP, true);
	set_intercept_ioio(vmcb, KBDATAP, true);
#endif

	/* intercept other things */
	set_intercept(vmcb, INTERCEPT_VMRUN, true);
	set_intercept(vmcb, INTERCEPT_VMMCALL, true);
	set_intercept(vmcb, INTERCEPT_STGI, true);
	set_intercept(vmcb, INTERCEPT_CLGI, true);
	set_intercept(vmcb, INTERCEPT_INTR, true);
	set_intercept(vmcb, INTERCEPT_VINTR, true);
	set_intercept(vmcb, INTERCEPT_IOIO_PROT, true);
	set_intercept(vmcb, INTERCEPT_CPUID, true);

	/* intercept exceptions */
	set_intercept_exception(vmcb, T_DEBUG, true);

	return 0;
}

/*
 * Setup the segment registers in VMCB.
 *
 * @param seg
 * @param sel segment selector
 * @param base segment base
 * @param lim segment limitation
 * @param attr segment attribution
 */
static void
set_segment(struct vmcb_seg *seg,
	    uint16_t sel, uint64_t base, uint32_t lim, uint16_t attr)
{
	assert(seg != NULL);

	seg->selector = sel;
	seg->base = base;
	seg->limit = lim;
	seg->attrib = attr;
}

/*
 * Setup VM to the powerup state. (Sec 14.1.3, APM Vol2 r3.19)
 *
 * XXX: Because CS.sel=0xf000, CS.base=0xffff_0000, and rip=0xfff0, the first
 *      instruction of VM is at 0xffff_fff0. Remember to redirect it to 0xfff0
 *      where the actual first instruction sits. CertiKOS treats it as a
 *      special case when handling nested page faults.
 */
static void
setup_powerup_state(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_save_area *save = &svm->vmcb->save;

	save->cr0 = 0x60000010;
	save->cr2 = save->cr3 = save->cr4 = 0;
	save->rflags = 0x00000002;
	save->efer = MSR_EFER_SVME;
	save->rip = 0x0000fff0;
	set_segment(&save->cs, 0xf000, 0xffff0000, 0xffff,
		    SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_CODE);
	set_segment(&save->ds, 0x0000, 0x0, 0xffff,
		    SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_DATA);
	set_segment(&save->es, 0x0000, 0x0, 0xffff,
		    SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_DATA);
	set_segment(&save->fs, 0x0000, 0x0, 0xffff,
		    SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_DATA);
	set_segment(&save->gs, 0x0000, 0x0, 0xffff,
		    SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_DATA);
	set_segment(&save->ss, 0x0000, 0x0, 0xffff,
		    SEG_ATTR_P | SEG_ATTR_S | SEG_TYPE_DATA);
	set_segment(&save->gdtr, 0, 0x0, 0xffff, 0);
	set_segment(&save->idtr, 0, 0x0, 0xffff, 0);
	set_segment(&save->ldtr, 0, 0x0, 0xffff, SEG_ATTR_P | SEG_TYPE_LDT);
	set_segment(&save->tr, 0, 0x0, 0xffff, SEG_ATTR_P | SEG_TYPE_TSS_BUSY);
	save->rax = 0x0;
	save->dr6 = 0xffff0ff0;
	save->dr7 = 0x00000400;
	save->g_pat = 0x7040600070406UL;

	svm->g_rbx = svm->g_rcx = svm->g_rsi = svm->g_rdi = svm->g_rbp = 0x0;
	svm->g_rdx = 0x80;
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
	if (svm_check() == false)
		return 1;

	/* enable SVM */
	svm_enable();

	/* setup host state-save area */
	if ((hsave_addr = alloc_hsave_area()) == 0x0)
		return 1;
	else
		wrmsr(MSR_VM_HSAVE_PA, hsave_addr);

	/* debug("Host state-save area is at %x.\n", hsave_addr); */

	/* register guest interrupt handler */
	trap_register_default_kern_handler(svm_guest_intr_handler);
	trap_register_kern_handler(T_GPFLT, svm_guest_handle_gpf);

	return 0;
}

/*
 * Initialize VM.
 */
static int
vm_init(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = alloc_svm();

	if (svm == NULL)
		return 1;

	vm->cookie = svm;

	/* allocate memory for VMCB */
	if ((svm->vmcb = alloc_vmcb()) == NULL)
		return 1;

	debug("VMCB is at %0x.\n", svm->vmcb);

	/* setup VM to the powerup state */
	setup_powerup_state(vm);

	/* create the nested page table */
	pmap_t *ncr3 = alloc_nested_ptable();

	if (ncr3 == NULL)
		return 1;

	svm->vmcb->control.nested_cr3 = (uintptr_t) ncr3;

	debug("Nested page table is at %x.\n", ncr3);

	/* load seabios */
	load_bios((uintptr_t) ncr3);

	/* setup interception */
	if (setup_intercept(svm->vmcb) != 0)
		return 1;

	/* miscellenea initialization */
	svm->vmcb->control.asid = 1;
	svm->vmcb->control.nested_ctl = 1;
	svm->vmcb->control.tlb_ctl = 0;
	svm->vmcb->control.tsc_offset = 0;
	/* Sec 15.21.1, APM Vol2 r3.19 */
	svm->vmcb->control.int_ctl = (SVM_INTR_CTRL_VINTR_MASK |
				      (0x0 & SVM_INTR_CTRL_VTPR));

	/* intialize virtual devices */
	vkbd_init(&vm->vkbd);
	vpci_init(&vm->vpci);
	vpic_init(&vm->vpic);

	return 0;
}

/*
 * Run VM.
 */
static int
vm_run(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;

	SVM_CLGI();

	sti();
	svm_run(svm);
	cli();

	if(svm->vmcb->control.exit_code == SVM_EXIT_INTR)
		vm->exit_for_intr = true;

	SVM_STGI();

	return 0;
}

/*
 * Handle VMEXIT.
 */
static int
svm_handle_exit(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb_control_area *ctrl = &svm->vmcb->control;

	bool handled = false;

	debug("[%x:%llx] ",
	      svm->vmcb->save.cs.selector, svm->vmcb->save.rip);

	switch (ctrl->exit_code) {
	case SVM_EXIT_EXCP_BASE ... (SVM_EXIT_INTR-1):
		info("VMEXIT for EXCP ");
		handled = svm_handle_exception(vm);
		break;

	case SVM_EXIT_INTR:
		info("VMEXIT for INTR.\n");
		/* rely on kernel interrupt handlers to ack interupts */
		handled = svm_handle_intr(vm);
		break;

	case SVM_EXIT_VINTR:
		info("VMEXIT for VINTR.\n");
		handled = svm_handle_vintr(vm);
		break;

	case SVM_EXIT_IOIO:
		info("VMEXIT for IO");
		handled = svm_handle_ioio(vm);
		break;

	case SVM_EXIT_NPF:
		info("VMEXIT for NPF");
		handled = svm_handle_npf(vm);
		break;

	case SVM_EXIT_CPUID:
		info("VMEXIT for cpuid");
		handled = svm_handle_cpuid(vm);
		break;

	case SVM_EXIT_SWINT:
		info("VMEXIT for INTn.\n");
		handled = svm_handle_swint(vm);
		break;

	case SVM_EXIT_RDTSC:
		info("VMEXIT for RDTSC.\n");
		handled = svm_handle_rdtsc(vm);
		break;

	case SVM_EXIT_ERR:
		info("VMEXIT for invalid guest state in VMCB.\n");
		handled = svm_handle_err(vm);
		break;

	default:
		panic("Unhandled VMEXIT: exit_code = %x.\n",
		      ctrl->exit_code);
	}

	if (ctrl->exit_code == SVM_EXIT_ERR)
		panic("Halt for SVM_EXIT_ERR.\n");

	if (handled == false) {
		panic("Unhandled VMEXIT: exit_code = %x.\n",
		      ctrl->exit_code);
	}

	return 0;
}

struct vmm_ops vmm_ops_amd = {
	.vmm_init = svm_init,
	.vm_init = vm_init,
	.vm_run = vm_run,
	.vm_handle_exit = svm_handle_exit
};

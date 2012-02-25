#include <kern/as/as.h>
#include <kern/debug/debug.h>

#include <architecture/cpuid.h>
#include <architecture/intr.h>
#include <architecture/trap.h>
#include <architecture/types.h>
#include <architecture/x86.h>

#include <kern/hvm/vmm.h>
#include <kern/hvm/vmm_iodev.h>
#include <kern/hvm/dev/pic.h>

#include <kern/hvm/svm/svm.h>
#include <kern/hvm/svm/svm_handle.h>
#include <kern/hvm/svm/svm_utils.h>

/*
 * Inject an event to the guest. (Sec 15.20, APM Vol2 r3.19)
 *
 * @param vmcb VMCB of the guest
 * @param type the type of the injected event
 * @param vector the 8-bit IDT vector of the interrupt or exception; if the
 *               injected event is NMI, it's ignored
 * @param ev whether the injected event requires an error code on the stack
 * @param errcode the error code
 */
static void
svm_inject_event(struct vmcb *vmcb,
		 uint8_t type, uint8_t vector, bool ev, uint32_t errcode)
{
	assert(vmcb != NULL);
	assert(type == SVM_EVTINJ_TYPE_INTR ||
	       type == SVM_EVTINJ_TYPE_NMI ||
	       type == SVM_EVTINJ_TYPE_EXEPT ||
	       type == SVM_EVTINJ_TYPE_SOFT);

	struct vmcb_control_area *ctrl = &vmcb->control;

	ctrl->event_inj =
		SVM_EVTINJ_VALID |
		((type << SVM_EVTINJ_TYPE_SHIFT) & SVM_EVTINJ_TYPE_MASK) |
		(vector & SVM_EVTINJ_VEC_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == true)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;

	debug("Inject event: type=%x, vec=%x, errcode=%x\n",
	      type, vector, errcode);
}

static void
svm_inject_vintr(struct vmcb *vmcb, uint8_t vector, uint8_t priority)
{
	assert(vmcb != NULL);

	struct vmcb_control_area *ctrl  = &vmcb->control;

	ctrl->int_ctl =
		SVM_INTR_CTRL_VIRQ | ((priority << 16) & SVM_INTR_CTRL_PRIO);
	ctrl->int_vector = vector;

	debug("Inject VINTR: vec=%x, prio=%x.\n", vector, priority);
}

/*
 * Temporarily disable bit interception on the current guest instruction. After
 * this instruction, the interception will be resumed.
 *
 * @param vm
 * @param bit which kind of interception will be disabled
 */
static void
skip_intercept_cur_instr(struct vm *vm, int bit)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;

	assert(svm->single_step == false);

	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	svm->single_step = true;
	svm->skip_intercept = bit;

	set_intercept(vmcb, bit, false);

	save->rflags |= FL_TF;
	set_intercept_exception(vmcb, T_DEBUG, true);
}

void
svm_guest_handle_gpf(struct vm *vm, struct trapframe *tf)
{
	assert(tf != NULL);
	assert(tf->tf_trapno == T_GPFLT);


	if (vm == NULL || vm->exit_for_intr == false)
		panic("General protection fault in kernel.\n");
	else
		panic("Trap %x from guest.\n", tf->tf_trapno);
}

/*
 * Handle interrupts from guest. The default behavior is to inject the interrupt
 * back to the guest.
 */
void
svm_guest_intr_handler(struct vm *vm, struct trapframe *tf)
{
	assert(tf != NULL);

	if (vm == NULL || vm->exit_for_intr == false)
		goto out;

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	int intr_no = tf->tf_trapno - T_IRQ0;

	assert(intr_no >= 0);
	debug("INTR%x happened in the guest.\n", intr_no);
	debug("RFLAGS=%llx.\n", save->rflags);

	uint32_t eflags = (uint32_t) save->rflags;

	if (eflags & FL_IF) {
		/*
		 * If interrupts is enabled in the guest, then
		 * - set corresponding ISR bit of VPIC,
		 * - clear correspoding IRR bit of VPIC,
		 * - inject an interrupt, and
		 * - send EOI to the physical PIC/APIC.
		 */

		/* vpic_set_isr(&vm->vpic, intr_no); */
		/* vpic_clear_irr(&vm->vpic, intr_no); */
		svm_inject_event(vmcb, SVM_EVTINJ_TYPE_INTR, intr_no, false, 0);
	} else {
		/*
		 * If interrupts is disabled in the guest, then
		 * - set corresponding IRR bit of VPIC,
		 * - inject a virtual interrupt, and
		 * - send EOI to the physical PIC/APIC.
		 *
		 * XXX: (a little more about this case) when the guest enables
		 *      interrupts sometime later, the injected virtual
		 *      interrupt will trigger a VMEXIT (if VMM intercepts
		 *      virtual interrupts). Then, VMM should send an EOI to the
		 *      virtual PIC.
		 */

		/* vpic_set_irr(&vm->vpic, intr_no); */
		svm_inject_vintr(vmcb, intr_no, 0);
	}

	/* vpic_raise_irq(&vm->vpic, intr_no); */
	/* vpic_intack(&vm->vpic, intr_no); */

 out:
	intr_eoi();
	vm->exit_for_intr = false;
}

bool
svm_handle_exception(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;
	struct vmcb_control_area *ctrl = &vmcb->control;
	uint32_t exit_code = ctrl->exit_code;

	switch (exit_code) {
	case SVM_EXIT_EXCP_BASE+T_DEBUG:
		info("DEBUG.\n");

		if (svm->single_step == true) {
			svm->single_step = false;
			set_intercept(vmcb, svm->skip_intercept, true);
			save->rflags &= ~(uint64_t) FL_TF;
			set_intercept_exception(vmcb, T_DEBUG, false);
		}

		break;

	default:
		debug("Unhandled expection %x.\n",
		      exit_code - SVM_EXIT_EXCP_BASE);
		return false;
	}

	return true;
}

bool
svm_handle_intr(struct vm *vm)
{
	assert(vm != NULL);

	/* rely on the kernel interrupt handlers to complete the work */

	return true;
}

bool
svm_handle_vintr(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;

	uint8_t v_tpr = ctrl->int_ctl & 0xff;
	int pending = (ctrl->int_ctl >> 8) & 0x1;
	uint8_t v_prio = (ctrl->int_ctl >> 16) & 0xf;
	int ign_tpr = (ctrl->int_ctl >> 20) & 0x1;
	int mask_vintr = (ctrl->int_ctl >> 24) & 0x1;
	uint8_t vector = ctrl->int_vector;

	debug("VINTR: pending=%d, vector=%x, TPR=%x, ignore TPR=%x, prio=%x, mask=%x.\n",
	      pending, vector, v_tpr, ign_tpr, v_prio, mask_vintr);

	/* vpic_eoi(&vm->vpic); */

	return true;
}

bool
svm_handle_ioio(struct vm *vm)
{
	assert(vm != NULL);

	int ret;

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	struct vmcb_save_area *save = &vmcb->save;

	uint32_t exitinfo1 = ctrl->exit_info_1;

	uint32_t port = (exitinfo1 & SVM_EXITINFO1_PORT_MASK) >>
		SVM_EXITINFO1_PORT_SHIFT;

	int sz32 = exitinfo1 & SVM_EXITINFO1_SZ32;
	int sz16 = exitinfo1 & SVM_EXITINFO1_SZ16;
	int sz8 = exitinfo1 & SVM_EXITINFO1_SZ8;

	bool type = exitinfo1 & SVM_EXITINFO1_TYPE_MASK;

	uint32_t data = (uint32_t) save->rax;

	if (sz32)
		data = (uint32_t) data;
	else if (sz16)
		data = (uint16_t) data;
	else if (sz8)
		data = (uint8_t) data;

	info("(port=%x, ", port);
	if (type & SVM_EXITINFO1_TYPE_IN) {
		info("in).\n");
		ret = vmm_iodev_read_port(vm, port, &data);
	} else {
		info("out).\n");
		ret = vmm_iodev_write_port(vm, port, &data);
	}

	if (ret) {
#if 0
		return false;
#else
		if (type & SVM_EXITINFO1_TYPE_IN) {
			data = inb(port);
			debug("(Passthrough) read %x from port %x.\n",
			      data, port);
		} else {
			outb(port, data);
			debug("(Passthrough) write %x to port %x.\n",
			      data, port);
		}
#endif
	}

	if (type & SVM_EXITINFO1_TYPE_IN) {
		if (sz32)
			*(uint32_t *) &save->rax = (uint32_t) data;
		else if (sz16)
			*(uint16_t *) &save->rax = (uint16_t) data;
		else if (sz8)
			*(uint8_t *) &save->rax = (uint8_t) data;
		else
			panic("Invalid data length.\n");
	}

	save->rip = ctrl->exit_info_2;

	return true;
}

/*
 * Handle nested page fault. (Sec 15.25.6, APM Vol2 r3.19)
 */
bool
svm_handle_npf(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = (struct vmcb *) svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	struct vmcb_save_area *save = &vmcb->save;

	uint64_t errcode = ctrl->exit_info_1;
	uintptr_t fault_addr = (uintptr_t) PGADDR(ctrl->exit_info_2);

	info("(va=%x, err=%llx).\n", fault_addr, errcode);

	assert(errcode & SVM_EXITINFO1_NFP_U);

	/*
	 * When BIOS starts, CS.base = 0xffff_0000 and IP = 0xfff0, so it will
	 * cause a nested page fault at 0xffff_fff0. CertiKOS will redirect this
	 * it to the instruction at 0x000f_fff0 (the top of 1MB BIOS memory).
	 */
	if (fault_addr == 0xfffff000 &&
	    save->rip == 0xfff0 && save->cs.base == 0xffff0000) {
		save->cs.base = 0xf0000;
		return true;
	}

#if 0
	if (fault_addr >= 0xf0000000) {
		as_assign((as_t *)(uintptr_t) ctrl->nested_cr3, fault_addr,
			  PTE_G | PTE_W | PTE_U, mem_phys2pi(fault_addr));
		return true;
	}
#endif

	if (as_reserve((as_t *)(uintptr_t) ctrl->nested_cr3, fault_addr,
		       PTE_W | PTE_U | PTE_P) == NULL) {
		debug("Failed to reserve memory for guest address %x.\n",
		      fault_addr);
		return false;
	}

	if (fault_addr >= VM_PHY_MEMORY_SIZE && fault_addr < 0xf0000000) {
		warn("Guest memory address (%x) is out of range (%x).\n",
		     fault_addr, VM_PHY_MEMORY_SIZE);
	}

#if 0
	if (fault_addr >= 0xf0000000) {
		as_copy((as_t *)(uintptr_t) ctrl->nested_cr3, fault_addr,
			kern_as, fault_addr, PAGESIZE);
	}
#endif

	return true;
}

/*
 * Handle CPUIDs. (Sec 2, AMD CPUID Specification)
 */
bool
svm_handle_cpuid(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	uint32_t rax, rbx, rcx, rdx;

	info(" %x.\n", save->rax);

	switch (save->rax) {
	case 0x40000000:
		/* 0x40000000 ~ 0x400000ff are reserved for the hypervisor. */
		svm->g_rbx = 0x74726543;	/* "treC" */
		svm->g_rcx = 0x534f4b69;	/* "SOKi" */

		break;

	case 0x00000001:
		/* pretend AVX, AEX, MONITOR, HTT and APIC are not present */
		cpuid(save->rax, &rax, &rbx, &rcx, &rdx);
		save->rax = rax;
		svm->g_rbx = rbx;
		svm->g_rcx = rcx & ~(uint64_t) (CPUID_FEATURE_AVX |
						CPUID_FEATURE_AES |
						CPUID_FEATURE_MONITOR);
		svm->g_rdx = rdx & ~(uint64_t) (CPUID_FEATURE_HTT |
						CPUID_FEATURE_APIC);
		break;

	case 0x80000001:
		/* pretend SKINIT, SVM and APIC are not present */
		cpuid(save->rax, &rax, &rbx, &rcx, &rdx);
		save->rax = rax;
		svm->g_rbx = rbx;
		svm->g_rcx = rcx & ~(uint64_t) (CPUID_X_FEATURE_SKINIT |
						CPUID_X_FEATURE_SVM);
		svm->g_rdx = rdx & ~(uint64_t) CPUID_FEATURE_APIC;

		break;

	default:
		/* passthrough all other CPUIDs */
		cpuid(save->rax, &rax, &rbx, &rcx, &rdx);
		save->rax = rax;
		svm->g_rbx = rbx;
		svm->g_rcx = rcx;
		svm->g_rdx = rdx;

		break;
	}

	save->rip += 2;		/* cpuid is a two-byte instruction. */

	return true;
}

/*
 * Handle software interrupts.
 *
 * XXX: DO NOT REFER TO Sec 15.8.2, APM Vol2 r3.19. On some machines, EXITINFO1
 *      does NOT provide the correct software interrupt number. Decode the
 *      intercepted instruction manually to retrieve the correct software
 *      interrupt number.
 */
bool
svm_handle_swint(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	uint8_t *instr = get_guest_instruction(vmcb);
	assert(*instr == 0xcd); /* intercepted instruction is INTn */
	uint8_t int_no = *(instr+1);

	switch (int_no) {
	default:
		skip_intercept_cur_instr(vm, INTERCEPT_INTn);
		break;
	}

	return true;
}

/*
 * Handle RDTSC instructions.
 */
bool
svm_handle_rdtsc(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	uint64_t tsc = rdtsc();
	svm->g_rdx = (tsc >> 32);
	save->rax = (tsc & 0xffffffff);
	save->rip += 2;

	return true;
}

/*
 * Check the errors of VMCB. (Sec 15.5.1, APM Vol2 r3.19)
 */
bool
svm_handle_err(struct vm *vm)
{
	assert(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;
	struct vmcb_control_area *ctrl = &vmcb->control;

	/* Sec 3.1.7, APM Vol2 r3.19 */
	uint64_t efer = save->efer;
	if ((efer & MSR_EFER_SVME) == 0)
		warn("EFER.SVME != 0\n");
	if ((efer >> 32) != 0ULL)
		warn("EFER[63..32] != 0\n");
	if ((efer >> 15) != 0ULL)
		warn("EFER[31..15] != 0\n");
	if ((efer & 0xfe) != 0ULL)
		warn("EFER[7..1] != 0\n");

	uint64_t cr0 = save->cr0;
	if ((cr0 & CR0_CD) == 0 && (cr0 & CR0_NW))
		warn("CR0.CD = 0 && CR0.NW = 1\n");
	if ((cr0 >> 32) != 0ULL)
		warn("CR0[63..32] != 0\n");

	/*
	 * TODO: check the MBZ bits of CR3 according to the actual mode
	 *       CertiKOS and/or VM is running in. Current version CertiKOS
	 *       and VM are using the legacy-mode non-PAE paging, so there
	 *       is no MBZ bits in CR3. (Sec 3.1.2, APM Vol2 r3.19)
	 */

	/* Sec 3.1.3, APM Vol2 r3.19 */
	uint64_t cr4 = save->cr4;
	if ((cr4 >> 32) != 0ULL)
		warn("CR4[63..32] != 0\n");
	if ((cr4 >> 19) != 0ULL)
		warn("CR4[31..19] != 0\n");
	if (((cr4 >> 11) & 0x7fULL) != 0ULL)
		warn("CR4[17..11] != 0\n");

	if ((save->dr6 >> 32) != 0ULL)
		warn("DR6[63..32] != 0\n");

	if ((save->dr7 >> 32) != 0ULL)
		warn("DR7[63..32] != 0\n");

	uint32_t dummy, edx;
	cpuid(0x80000001, &dummy, &dummy, &dummy, &edx);

	if (((efer & MSR_EFER_LMA) || (efer & MSR_EFER_LME)) &&
	    (edx & CPUID_X_FEATURE_LM) == 0)
		warn("EFER.LMA =1 or EFER.LME = 1, while long mode is not supported.\n");
	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) == 0)
		warn("EFER.LME = 1 and CR0.PG =1, while CR4.PAE = 0.\n ");

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr0 & CR0_PE) == 0)
		warn("EFER.LME = 1 and CR0.PG =1, while CR0.PE = 0.\n ");

	uint16_t cs_attrib = save->cs.attrib;

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) &&
	    (cs_attrib & SEG_ATTR_L) && (cs_attrib & SEG_ATTR_D))
		warn("EFER.LME, CR0.PG, CR4.PAE, CS.L and CS.D are non-zero.\n");

	if ((ctrl->intercept & (1ULL << INTERCEPT_VMRUN)) == 0)
		warn("VMRUN is not intercepted.\n");

	if (ctrl->iopm_base_pa > mem_max)
		warn("The address of IOPM (%x) is out of range of the physical memory.\n",
		     ctrl->iopm_base_pa);

	if (ctrl->msrpm_base_pa > mem_max)
		warn("The address of MSRPM (%x) is out of range of the physical memory.\n",
		     ctrl->msrpm_base_pa);

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		warn("Illegal event injection.\n");

	if (ctrl->asid == 0)
		warn("ASID = 0\n");

	return true;
}

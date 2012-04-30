#include <sys/debug.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/trap.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pic.h>

#include <machine/pmap.h>

#include "svm.h"
#include "svm_handle.h"
#include "svm_utils.h"
#include "svm_intr.h"

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
void
svm_inject_event(struct vmcb *vmcb,
		 uint32_t type, uint8_t vector, bool ev, uint32_t errcode)
{
	KERN_ASSERT(vmcb != NULL);
	KERN_ASSERT(type == SVM_EVTINJ_TYPE_INTR ||
		    type == SVM_EVTINJ_TYPE_NMI ||
		    type == SVM_EVTINJ_TYPE_EXEPT ||
		    type == SVM_EVTINJ_TYPE_SOFT);

	struct vmcb_control_area *ctrl = &vmcb->control;

	KERN_ASSERT((ctrl->event_inj & SVM_EVTINJ_VALID) == 0);

	ctrl->event_inj =
		SVM_EVTINJ_VALID |
		((type << SVM_EVTINJ_TYPE_SHIFT) & SVM_EVTINJ_TYPE_MASK) |
		(vector & SVM_EVTINJ_VEC_MASK);
	ctrl->event_inj_err = errcode;

	if (ev == TRUE)
		ctrl->event_inj |= SVM_EVTINJ_VALID_ERR;

	switch (type) {
	case SVM_EVTINJ_TYPE_INTR:
#ifdef DEBUG_EVT_INJECT
		KERN_DEBUG("Inject ExtINTR: vec=%x.\n", vector);
#endif
		break;
	case SVM_EVTINJ_TYPE_NMI:
#ifdef DEBUG_EVT_INJECT
		KERN_DEBUG("Inject NMI.\n");
#endif
		break;
	case SVM_EVTINJ_TYPE_EXEPT:
#ifdef DEBUG_EVT_INJECT
		KERN_DEBUG("Inject exception: vec=%x, ev=%x, errcode=%x.\n",
			   vector, ev, errcode);
#endif
		break;
	case SVM_EVTINJ_TYPE_SOFT:
#ifdef DEBUG_EVT_INJECT
		KERN_DEBUG("Inject INT %x.\n", vector);
#endif
		break;
	}
}

void
svm_inject_vintr(struct vmcb *vmcb, uint8_t vector, uint8_t priority)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_control_area *ctrl  = &vmcb->control;

	ctrl->int_ctl |= SVM_INTR_CTRL_VIRQ |
		((priority << SVM_INTR_CTRL_PRIO_SHIFT) & SVM_INTR_CTRL_PRIO) |
		SVM_INTR_CTRL_IGN_VTPR;
	ctrl->int_vector = vector;

#ifdef DEBUG_EVT_INJECT
	KERN_DEBUG("Inject VINTR: vec=%x, prio=%x.\n", vector, priority);
#endif
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
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;

	KERN_ASSERT(svm->single_step == FALSE);

	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	svm->single_step = TRUE;
	svm->skip_intercept = bit;

	set_intercept(vmcb, bit, FALSE);

	save->rflags |= FL_TF;
	set_intercept_exception(vmcb, T_DEBUG, TRUE);
}

static void
physical_ioport_read(uint32_t port, void *data, data_sz_t size)
{
	KERN_ASSERT(data != NULL);

	switch (size) {
	case SZ8:
		*(uint8_t *) data = inb(port);
		break;

	case SZ16:
		*(uint16_t *) data = inw(port);
		break;

	case SZ32:
		*(uint32_t *) data = inl(port);
		break;

	default:
		KERN_PANIC("Invalid data size.\n");
	}
}

static void
physical_ioport_write(uint32_t port, void *data, data_sz_t size)
{
	KERN_ASSERT(data != NULL);

	switch (size) {
	case SZ8:
		outb(port, *(uint8_t *) data);
		break;

	case SZ16:
		outw(port, *(uint16_t *) data);
		break;

	case SZ32:
		outl(port, *(uint32_t *) data);
		break;

	default:
		KERN_PANIC("Invalid data size.\n");
	}
}

/*
 * Handle interrupts from guest. The default behavior is to inject the interrupt
 * back to the guest.
 */
int
svm_guest_intr_handler(struct vm *vm, uint8_t irq)
{
	KERN_ASSERT(vm != NULL && vm->exit_for_intr == TRUE);

	KERN_ASSERT(irq >= 0);
#ifdef DEBUG_GUEST_INTR
	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;
	KERN_DEBUG("INTR %x happened in the guest (gIF=%x, hIF=%x).\n",
		   irq, (save->rflags & FL_IF), (read_eflags() & FL_IF));
#endif

	if (vmm_handle_extintr(vm, irq)) {
		/*
		 * If there is no handler for this interrupt, then inject the
		 * interrupt to the guest.
		 */
		if (vpic_is_ready(&vm->vpic) == TRUE) {
			vpic_set_irq(&vm->vpic, irq, 0);
			vpic_set_irq(&vm->vpic, irq, 1);
		}
	}

	vm->exit_for_intr = FALSE;

	return 0;
}

bool
svm_handle_exception(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;
	struct vmcb_control_area *ctrl = &vmcb->control;
	uint32_t exit_code = ctrl->exit_code;

	switch (exit_code) {
	case SVM_EXIT_EXCP_BASE+T_DEBUG:
#ifdef DEBUG_GUEST_EXCEPT
		dprintf("DEBUG.\n");
#endif

		if (svm->single_step == TRUE) {
			svm->single_step = FALSE;
			set_intercept(vmcb, svm->skip_intercept, TRUE);
			save->rflags &= ~(uint64_t) FL_TF;
			set_intercept_exception(vmcb, T_DEBUG, FALSE);
		}

		break;

	default:
#ifdef DEBUG_GUEST_EXCEPT
		KERN_DEBUG("Unhandled expection %x.\n",
			   exit_code - SVM_EXIT_EXCP_BASE);
#endif
		return FALSE;
	}

	return TRUE;
}

bool
svm_handle_intr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	/* Interrupts should be already handled before. */
	return TRUE;
}

bool
svm_handle_vintr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	struct vmcb_save_area *save = &vmcb->save;

#ifdef DEBUG_GUEST_VINTR
	uint8_t v_tpr = ctrl->int_ctl & SVM_INTR_CTRL_VTPR;
	int pending = ctrl->int_ctl & SVM_INTR_CTRL_VIRQ;
	uint8_t v_prio = ctrl->int_ctl & SVM_INTR_CTRL_PRIO;
	int ign_tpr = ctrl->int_ctl & SVM_INTR_CTRL_IGN_VTPR;
	int mask_vintr = ctrl->int_ctl & SVM_INTR_CTRL_VINTR_MASK;
	uint8_t vector = ctrl->int_vector;

	KERN_DEBUG("VINTR: pending=%d, vector=%x, TPR=%x, ignore TPR=%x, prio=%x, mask=%x.\n",
		   pending >> SVM_INTR_CTRL_VIRQ_SHIFT,
		   vector, v_tpr,
		   ign_tpr >> SVM_INTR_CTRL_IGN_VTPR_SHIFT,
		   v_prio >> SVM_INTR_CTRL_PRIO_SHIFT,
		   mask_vintr >> SVM_INTR_CTRL_VINTR_MASK_SHIFT);
#endif

	if (save->rflags) {
		if (!(save->rflags & FL_IF))
			/*
			 * XXX: Is IF bit of EFLAGS always set whenever a
			 *      virtual interrupt is taken?
			 * TODO: If so, change this branch to an assertion
			 *         KERN_ASSERT(save->rflags & FL_IF);
			 */
			KERN_WARN("IF FLAG is not set ! \n");
		else {
#ifdef DEUBG_GUEST_VINTR
			KERN_DEBUG("IF FLAG is set ! \n");
#endif
			ctrl->int_ctl &= ~SVM_INTR_CTRL_VIRQ;
		}
	}

	return TRUE;
}

bool
svm_handle_ioio(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

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
	data_sz_t size = SZ8; /* set the default data size to 1 byte */

#ifdef DEBUG_GUEST_IOIO
	dprintf("(port=%x, ", port);
#endif

	if (sz32) {
#ifdef DEBUG_GUEST_IOIO
		dprintf("4 bytes, ");
#endif
		data = (uint32_t) data;
		size = SZ32;
	} else if (sz16) {
#ifdef DEBUG_GUEST_IOIO
		dprintf("2 bytes, ");
#endif
		data = (uint16_t) data;
		size = SZ16;
	} else if (sz8) {
#ifdef DEBUG_GUEST_IOIO
		dprintf("1 byte, ");
#endif
		data = (uint8_t) data;
		size = SZ8;
	} else
		KERN_PANIC("Invalid data length.\n");

	if (type & SVM_EXITINFO1_TYPE_IN) {
#ifdef DEBUG_GUEST_IOIO
		dprintf("in).\n");
#endif
		ret = vmm_iodev_read_port(vm, port, &data, size);

		if (ret) {
#ifdef DEBUG_GUEST_IOIO
			KERN_DEBUG("Passthrough\n");
#endif
			physical_ioport_read(port, &data, size);
		}
	} else {
#ifdef DEBUG_GUEST_IOIO
		dprintf("out).\n");
#endif
		ret = vmm_iodev_write_port(vm, port, &data, size);

		if (ret) {
#ifdef DEBUG_GUEST_IOIO
			KERN_DEBUG("Passthrough\n");
#endif
			physical_ioport_write(port, &data, size);
		}
	}

	if (type & SVM_EXITINFO1_TYPE_IN) {
		if (sz32)
			*(uint32_t *) &save->rax = (uint32_t) data;
		else if (sz16)
			*(uint16_t *) &save->rax = (uint16_t) data;
		else if (sz8)
			*(uint8_t *) &save->rax = (uint8_t) data;
		else
			KERN_PANIC("Invalid data length.\n");
	}

	save->rip = ctrl->exit_info_2;

	return TRUE;
}

/*
 * Handle nested page fault. (Sec 15.25.6, APM Vol2 r3.19)
 */
bool
svm_handle_npf(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = (struct vmcb *) svm->vmcb;
	struct vmcb_control_area *ctrl = &vmcb->control;
	struct vmcb_save_area *save = &vmcb->save;

	uint64_t errcode = ctrl->exit_info_1;
	uintptr_t fault_addr = (uintptr_t) PGADDR(ctrl->exit_info_2);

#ifdef DEBUG_GUEST_NPF
	dprintf("(va=%x, err=%llx).\n", fault_addr, errcode);
#endif

	KERN_ASSERT(errcode & SVM_EXITINFO1_NFP_U);

	/*
	 * When BIOS starts, CS.base = 0xffff_0000 and IP = 0xfff0, so it will
	 * cause a nested page fault at 0xffff_fff0. CertiKOS will redirect this
	 * it to the instruction at 0x000f_fff0 (the top of 1MB BIOS memory).
	 */
	if (fault_addr == 0xfffff000 &&
	    save->rip == 0xfff0 && save->cs.base == 0xffff0000) {
		save->cs.base = 0xf0000;
		return TRUE;
	}

#if 0
	if (fault_addr >= 0xf0000000) {
		as_assign((as_t *)(uintptr_t) ctrl->nested_cr3, fault_addr,
			  PTE_G | PTE_W | PTE_U, mem_phys2pi(fault_addr));
		return TRUE;
	}
#endif

	if (pmap_reserve((pmap_t *)(uintptr_t) ctrl->nested_cr3,
			 fault_addr, PTE_W | PTE_G | PTE_U) == NULL) {
#ifdef DEBUG_GUEST_NPF
		KERN_DEBUG("Failed to reserve memory for guest address %x.\n",
			   fault_addr);
#endif
		return FALSE;
	}

	if (fault_addr >= VM_PHY_MEMORY_SIZE && fault_addr < 0xf0000000) {
		KERN_WARN("Guest memory address (%x) is out of range (%x).\n",
			  fault_addr, VM_PHY_MEMORY_SIZE);
	}

#if 0
	if (fault_addr >= 0xf0000000) {
		as_copy((as_t *)(uintptr_t) ctrl->nested_cr3, fault_addr,
			kern_as, fault_addr, PAGESIZE);
	}
#endif

	return TRUE;
}

bool
svm_handle_cpuid(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	uint32_t rax, rbx, rcx, rdx;

#ifdef DEBUG_GUEST_CPUID
	dprintf(" %x.\n", save->rax);
#endif

	switch (save->rax) {
	case 0x40000000:
		/* 0x40000000 ~ 0x400000ff are reserved for the hypervisor. */
		svm->g_rbx = 0x74726543;	/* "treC" */
		svm->g_rcx = 0x534f4b69;	/* "SOKi" */

		break;

	case 0x00000001:
		cpuid(save->rax, &rax, &rbx, &rcx, &rdx);
		save->rax = (6 << 8) | (2 << 4) | (3);
		svm->g_rbx = /* only 1 processor/core */
			(rbx & ~(uint64_t) (0xf << 16)) | (uint64_t) (1 << 16);
		svm->g_rcx = rcx & ~(uint64_t) (CPUID_FEATURE_AVX |
						CPUID_FEATURE_AES |
						CPUID_FEATURE_MONITOR);
		svm->g_rdx = rdx & ~(uint64_t) (CPUID_FEATURE_HTT |
						CPUID_FEATURE_MCA |
						CPUID_FEATURE_MTRR |
						CPUID_FEATURE_APIC |
						CPUID_FEATURE_MCE |
						CPUID_FEATURE_MSR |
						CPUID_FEATURE_DE);
		break;

	case 0x80000001:
		cpuid(save->rax, &rax, &rbx, &rcx, &rdx);
		save->rax = (6 << 8) | (2 << 4) | (3);
		svm->g_rbx = rbx;
		svm->g_rcx = rcx & ~(uint64_t) (CPUID_X_FEATURE_WDT |
						CPUID_X_FEATURE_SKINIT |
						CPUID_X_FEATURE_XAPIC |
						CPUID_X_FEATURE_SVM);
		svm->g_rdx = rdx & ~(uint64_t) (CPUID_X_FEATURE_RDTSCP |
						CPUID_X_FEATURE_NX |
						CPUID_X_FEATURE_MCA |
						CPUID_X_FEATURE_MTRR |
						CPUID_X_FEATURE_APIC |
						CPUID_X_FEATURE_MCE |
						CPUID_X_FEATURE_MSR |
						CPUID_X_FEATURE_DE);

		break;

	default:
		cpuid(save->rax, &rax, &rbx, &rcx, &rdx);
		save->rax = rax;
		svm->g_rbx = rbx;
		svm->g_rcx = rcx;
		svm->g_rdx = rdx;

		break;
	}

#ifdef DEBUG_GUEST_CPUID
	KERN_DEBUG("eax=%x, ebx=%x, ecx=%x, edx=%x\n",
		   (uint32_t) save->rax, (uint32_t) svm->g_rbx,
		   (uint32_t) svm->g_rcx, (uint32_t) svm->g_rdx);
#endif

	save->rip += 2;

	return TRUE;
}

bool
svm_handle_swint(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	uint8_t *instr = get_guest_instruction(vmcb);
	KERN_ASSERT(*instr == 0xcd); /* intercepted instruction is INTn */
	uint8_t int_no = *(instr+1);

	switch (int_no) {
	default:
		skip_intercept_cur_instr(vm, INTERCEPT_INTn);
		break;
	}

	return TRUE;
}

bool
svm_handle_rdtsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;
	struct vmcb_save_area *save = &vmcb->save;

	svm->g_rdx = vm->tsc >> 32;
	save->rax = (vm->tsc & 0xffffffff);

	save->rip += 2;

	return TRUE;
}

/*
 * CertiKOS does not support RDTSCP instruction in guest, so when it detects the
 * guest tries to execute RDTSCP instructions, it injects an invalid opcode
 * exception to the guest.
 */
bool
svm_handle_rdtscp(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	struct svm *svm = (struct svm *) vm->cookie;
	struct vmcb *vmcb = svm->vmcb;

	svm_inject_event(vmcb, SVM_EVTINJ_TYPE_EXEPT, T_ILLOP, FALSE, 0);

	/* XXX: Do NOT increase rip since we inject an exception. */

	return 0;
}

/*
 * Check the errors of VMCB. (Sec 15.5.1, APM Vol2 r3.19)
 */
bool
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
	    (cpuinfo.feature2 & CPUID_X_FEATURE_LM) == 0)
		KERN_WARN("EFER.LMA =1 or EFER.LME = 1, while long mode is not supported.\n");
	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) == 0)
		KERN_WARN("EFER.LME = 1 and CR0.PG =1, while CR4.PAE = 0.\n ");

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr0 & CR0_PE) == 0)
		KERN_WARN("EFER.LME = 1 and CR0.PG =1, while CR0.PE = 0.\n ");

	uint16_t cs_attrib = save->cs.attrib;

	if ((efer & MSR_EFER_LME) && (cr0 & CR0_PG) && (cr4 & CR4_PAE) &&
	    (cs_attrib & SEG_ATTR_L) && (cs_attrib & SEG_ATTR_D))
		KERN_WARN("EFER.LME, CR0.PG, CR4.PAE, CS.L and CS.D are non-zero.\n");

	if ((ctrl->intercept & (1ULL << INTERCEPT_VMRUN)) == 0)
		KERN_WARN("VMRUN is not intercepted.\n");

	if (ctrl->iopm_base_pa > mem_max_phys())
		KERN_WARN("The address of IOPM (%x) is out of range of the physical memory.\n",
			  ctrl->iopm_base_pa);

	if (ctrl->msrpm_base_pa > mem_max_phys())
		KERN_WARN("The address of MSRPM (%x) is out of range of the physical memory.\n",
			  ctrl->msrpm_base_pa);

	if (ctrl->event_inj & SVM_EVTINJ_VALID)
		KERN_WARN("Illegal event injection.\n");

	if (ctrl->asid == 0)
		KERN_WARN("ASID = 0\n");

	return TRUE;
}

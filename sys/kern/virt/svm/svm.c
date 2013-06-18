#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/hvm.h>

#include <dev/intr.h>
#include <dev/pcpu.h>

#include "npt.h"
#include "svm.h"
#include "svm_drv.h"
#include "vmcb.h"

static struct svm svm0;

static exit_reason_t
svm_handle_exit(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;
	uint32_t exitcode = vmcb_get_exit_code(vmcb);
	uint32_t exitinfo1 = vmcb_get_exit_info1(vmcb);
	uint32_t exitinfo2 = vmcb_get_exit_info2(vmcb);

#ifdef DEBUG_VMEXIT
	SVM_DEBUG("VMEXIT exit_code 0x%x, guest EIP 0x%08x.\n",
		  exitcode, vmcb_get_eip(vmcb));
#endif

	switch (exitcode) {
	case SVM_EXIT_INTR:
		return EXIT_REASON_EXTINT;
	case SVM_EXIT_VINTR:
		vmcb_clear_virq(vmcb);
		return EXIT_REASON_INTWIN;
	case SVM_EXIT_IOIO:
		svm->port = (exitinfo1 & SVM_EXITINFO1_PORT_MASK) >>
			SVM_EXITINFO1_PORT_SHIFT;
		svm->width = (exitinfo1 & SVM_EXITINFO1_SZ8) ? SZ8 :
			(exitinfo1 & SVM_EXITINFO1_SZ16) ? SZ16 : SZ32;
		svm->write = (exitinfo1 & SVM_EXITINFO1_TYPE_IN) ? FALSE : TRUE;
		svm->rep = (exitinfo1 & SVM_EXITINFO1_REP) ? TRUE : FALSE;
		svm->str = (exitinfo1 & SVM_EXITINFO1_STR) ? TRUE : FALSE;
		return EXIT_REASON_IOPORT;
	case SVM_EXIT_NPF:
		svm->fault_addr = (uintptr_t) PGADDR(exitinfo2);
		return EXIT_REASON_PGFLT;
	case SVM_EXIT_CPUID:
		return EXIT_REASON_CPUID;
	case SVM_EXIT_RDTSC:
		return EXIT_REASON_RDTSC;
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
		return EXIT_REASON_INVAL_INSTR;
	default:
		return EXIT_REASON_INVAL;
	}
}

/*
 * Initialize SVM module.
 * - check whether SVM is supported
 * - enable SVM
 * - allocate host save area
 *
 * @return 0 if succeed
 */
int
svm_init(void)
{
	KERN_ASSERT(offsetof(struct svm, vmcb) == 24);

	memzero(&svm0, sizeof(svm0));
	svm0.inuse = 0;

	return svm_drv_init();
}

struct svm *
svm_init_vm(void)
{
	struct svm *svm = &svm0;
	struct vmcb *vmcb;
	npt_t npt;

	if (svm->inuse == 1)
		return NULL;
	memzero(svm, sizeof(struct svm));
	svm->inuse = 1;

	if ((vmcb = vmcb_new()) == NULL) {
		svm->inuse = 0;
		return NULL;
	}
	svm->vmcb = vmcb;

	if ((npt = npt_new()) == NULL) {
		svm->inuse = 0;
		vmcb_free(vmcb);
		return NULL;
	}
	svm->npt = npt;
	npt_install(vmcb, npt);

	/*
	 * Setup default interception.
	 */

	/* intercept instructions */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_VMRUN);		/* vmrun */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_VMMCALL);	/* vmmcall */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_STGI);		/* stgi */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_CLGI);		/* clgi */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_CPUID);		/* cpuid */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_RDTSC);		/* rdtsc */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_RDTSCP);	/* rdtscp */
	vmcb_set_intercept(vmcb, INTERCEPT_IOIO_PROT);		/* in/out */

	/* intercept interrupts */
	vmcb_set_intercept(svm->vmcb, INTERCEPT_INTR);

	return svm;
}

exit_reason_t
svm_run_vm(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;

	svm_drv_run_vm(vmcb, &svm->g_rbx, &svm->g_rcx, &svm->g_rdx, &svm->g_rsi,
		       &svm->g_rdi, &svm->g_rbp);

	uint32_t exit_int_info = vmcb_get_exit_int_info(vmcb);

	if (exit_int_info & SVM_EXITINTINFO_VALID) {
		uint32_t errcode = vmcb_get_exit_int_errcode(vmcb);;
		uint32_t int_type = exit_int_info & SVM_EXITINTINFO_TYPE_MASK;

		struct vmcb_control_area *ctrl = &vmcb->control;

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

	uint32_t exit_code = vmcb_get_exit_code(vmcb);

	if (exit_code == SVM_EXIT_ERR) {
#ifdef DEBUG_MSG
		svm_handle_err(vmcb);
#endif
		return EXIT_REASON_INVAL;
	}

	return svm_handle_exit(svm);
}

void
svm_intercept_vintr(struct svm *svm, bool enable)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;

	if (enable == TRUE) {
		vmcb_inject_virq(vmcb, 0, 0);
		vmcb_set_intercept(vmcb, INTERCEPT_VINTR);
	} else {
		vmcb_clear_intercept(vmcb, INTERCEPT_VINTR);
	}
}

uint32_t
svm_get_reg(struct svm *svm, guest_reg_t reg)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;

	switch (reg) {
	case GUEST_EAX:
		return vmcb_get_eax(vmcb);
	case GUEST_EBX:
		return (uint32_t) svm->g_rbx;
	case GUEST_ECX:
		return (uint32_t) svm->g_rcx;
	case GUEST_EDX:
		return (uint32_t) svm->g_rdx;
	case GUEST_ESI:
		return (uint32_t) svm->g_rsi;
	case GUEST_EDI:
		return (uint32_t) svm->g_rdi;
	case GUEST_EBP:
		return (uint32_t) svm->g_rbp;
	case GUEST_ESP:
		return vmcb_get_esp(vmcb);
	case GUEST_EIP:
		return vmcb_get_eip(vmcb);
	case GUEST_EFLAGS:
		return vmcb_get_eflags(vmcb);
	case GUEST_CR0:
		return vmcb_get_cr0(vmcb);
	case GUEST_CR2:
		return vmcb_get_cr2(vmcb);
	case GUEST_CR3:
		return vmcb_get_cr3(vmcb);
	case GUEST_CR4:
		return vmcb_get_cr4(vmcb);
	default:
		return 0xffffffff;
	}
}

int
svm_set_reg(struct svm *svm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;

	switch (reg) {
	case GUEST_EAX:
		vmcb_set_eax(vmcb, val);
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
		vmcb_set_esp(vmcb, val);
		break;
	case GUEST_EIP:
		vmcb_set_eip(vmcb, val);
		break;
	case GUEST_EFLAGS:
		vmcb_set_eflags(vmcb, val);
		break;
	case GUEST_CR0:
		vmcb_set_cr0(vmcb, val);
		break;
	case GUEST_CR2:
		vmcb_set_cr2(vmcb, val);
		break;
	case GUEST_CR3:
		vmcb_set_cr3(vmcb, val);
		break;
	case GUEST_CR4:
		vmcb_set_cr4(vmcb, val);
		break;
	default:
		return -1;
	}

	return 0;
}

int
svm_set_seg(struct svm *svm, guest_seg_t seg,
	    uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar)
{
	KERN_ASSERT(svm != NULL);
	return vmcb_set_seg(svm->vmcb, seg, sel, base, lim, ar);
}

int
svm_set_mmap(struct svm *svm, uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT(svm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa || ROUNDDOWN(hpa, PAGESIZE) != hpa)
		return 1;

	npt_t npt = svm->npt;

	return npt_insert(npt, gpa, hpa);
}

int
svm_inject_event(struct svm *svm,
		 guest_event_t type, uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(svm != NULL);

	int evt_type;

	if (type == EVENT_EXTINT)
		evt_type = SVM_EVTINJ_TYPE_INTR;
	else if (type == EVENT_EXCEPTION)
		evt_type = SVM_EVTINJ_TYPE_EXEPT;
	else
		return 1;

	return vmcb_inject_event(svm->vmcb, evt_type, vector, errcode, ev);
}

uint32_t
svm_get_next_eip(struct svm *svm, guest_instr_t instr)
{
	KERN_ASSERT(svm != NULL);

	struct vmcb *vmcb = svm->vmcb;

	switch (instr) {
	case INSTR_IN:
	case INSTR_OUT:
		return vmcb_get_exit_info2(vmcb);
	default:
		return vmcb_get_neip(vmcb);
	}
}

bool
svm_pending_event(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return vmcb_pending_event(svm->vmcb);
}

bool
svm_intr_shadow(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return vmcb_get_intr_shadow(svm->vmcb);
}

uint16_t
svm_exit_io_port(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->port;
}

data_sz_t
svm_exit_io_width(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->width;
}

bool
svm_exit_io_write(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->write;
}

bool
svm_exit_io_rep(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->rep;
}

bool
svm_exit_io_str(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->str;
}

uintptr_t
svm_exit_fault_addr(struct svm *svm)
{
	KERN_ASSERT(svm != NULL);
	return svm->fault_addr;
}

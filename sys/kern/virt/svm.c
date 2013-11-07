#include <preinit/lib/debug.h>
#include <preinit/dev/svm_drv.h>

#include <lib/string.h>
#include <lib/types.h>

#include "npt_init.h"
#include "svm.h"
#include "vmcb.h"

#include "npt_intro.h"

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

#define PGADDR(la)	((uintptr_t) (la) & ~0xFFF)	/* address of page */

#define PAGESIZE	4096

static bool svm_inited = FALSE;
static struct svm svm0;
static bool svm0_inited = FALSE;

static exit_reason_t
svm_handle_exit(void)
{
	uint32_t exitcode = vmcb_get_exit_code();
	uint32_t exitinfo1 = vmcb_get_exit_info1();
	uint32_t exitinfo2 = vmcb_get_exit_info2();

#ifdef DEBUG_VMEXIT
	SVM_DEBUG("VMEXIT exit_code 0x%x, guest EIP 0x%08x.\n",
		  exitcode, vmcb_get_reg(GUEST_EIP));
#endif

	switch (exitcode) {
	case SVM_EXIT_INTR:
		return EXIT_REASON_EXTINT;
	case SVM_EXIT_VINTR:
		vmcb_clear_virq();
		return EXIT_REASON_INTWIN;
	case SVM_EXIT_IOIO:
		svm0.exit_info.ioport.port =
			(exitinfo1 & SVM_EXITINFO1_PORT_MASK) >>
			SVM_EXITINFO1_PORT_SHIFT;
		svm0.exit_info.ioport.seg =
			(exitinfo1 & SVM_EXITINFO1_SEG_MASK) >>
			SVM_EXITINFO1_SEG_SHIFT;
		svm0.exit_info.ioport.aw =
			(exitinfo1 & SVM_EXITINFO1_A16) ? A16 :
			(exitinfo1 & SVM_EXITINFO1_A32) ? A32 : A64;
		svm0.exit_info.ioport.dw =
			(exitinfo1 & SVM_EXITINFO1_SZ8) ? SZ8 :
			(exitinfo1 & SVM_EXITINFO1_SZ16) ? SZ16 : SZ32;
		svm0.exit_info.ioport.write =
			(exitinfo1 & SVM_EXITINFO1_TYPE_IN) ? FALSE : TRUE;
		svm0.exit_info.ioport.rep =
			(exitinfo1 & SVM_EXITINFO1_REP) ? TRUE : FALSE;
		svm0.exit_info.ioport.str =
			(exitinfo1 & SVM_EXITINFO1_STR) ? TRUE : FALSE;
		svm0.exit_info.ioport.neip = exitinfo2;
		return EXIT_REASON_IOPORT;
	case SVM_EXIT_NPF:
		svm0.exit_info.pgflt.addr = (uintptr_t) PGADDR(exitinfo2);
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

void
svm_init(void)
{
	if (svm_inited == TRUE)
		return;

	svm_drv_init();
	svm_inited = TRUE;
}

void
svm_init_vm(void)
{
	if (svm0_inited == TRUE)
		return;

	memzero(&svm0, sizeof(struct svm));
	vmcb_init();
	npt_init();

	svm0.synced = FALSE;

	svm0_inited = TRUE;
}

void
svm_run_vm(void)
{
	svm0.synced = FALSE;

	vmcb_run_vm(&svm0.g_rbx, &svm0.g_rcx, &svm0.g_rdx, &svm0.g_rsi,
		    &svm0.g_rdi, &svm0.g_rbp);
}

void
svm_sync(void)
{
	if (svm0.synced == TRUE)
		return;

	uint32_t exit_int_info = vmcb_get_exit_intinfo();

	if (exit_int_info & SVM_EXITINTINFO_VALID) {
		uint32_t errcode = vmcb_get_exit_interr();
		bool ev = (exit_int_info & SVM_EXITINTINFO_VALID_ERR) ?
			TRUE : FALSE;
		uint32_t int_type = exit_int_info & SVM_EXITINTINFO_TYPE_MASK;
		uint8_t vector = exit_int_info & SVM_EXITINTINFO_VEC_MASK;

		switch (int_type) {
		case SVM_EXITINTINFO_TYPE_INTR:
#ifdef DEBUG_GUEST_INTR
			SVM_DEBUG("Pending INTR: vec=%x.\n",
				  exit_int_info & SVM_EXITINTINFO_VEC_MASK);
#endif
			vmcb_inject_event(int_type >> 7, vector, errcode, ev);
			break;

		case SVM_EXITINTINFO_TYPE_NMI:
#ifdef DEBUG_GUEST_INTR
			SVM_DEBUG("Pending NMI.\n");
#endif
			vmcb_inject_event(int_type >> 7, vector, errcode, ev);
			break;

		default:
			break;
		}
	}

	svm0.exit_reason = svm_handle_exit();
	svm0.synced = TRUE;
}

void
svm_set_intercept_vint(void)
{
	vmcb_inject_virq();
	vmcb_set_intercept_vint();
}

void
svm_clear_intercept_vint(void)
{
	vmcb_clear_intercept_vint();
}

uint32_t
svm_get_reg(guest_reg_t reg)
{
	switch (reg) {
	case GUEST_EAX:
		return vmcb_get_reg(GUEST_EAX);
	case GUEST_EBX:
		return svm0.g_rbx;
	case GUEST_ECX:
		return svm0.g_rcx;
	case GUEST_EDX:
		return svm0.g_rdx;
	case GUEST_ESI:
		return svm0.g_rsi;
	case GUEST_EDI:
		return svm0.g_rdi;
	case GUEST_EBP:
		return svm0.g_rbp;
	case GUEST_ESP:
		return vmcb_get_reg(GUEST_ESP);
	case GUEST_EIP:
		return vmcb_get_reg(GUEST_EIP);
	case GUEST_EFLAGS:
		return vmcb_get_reg(GUEST_EFLAGS);
	case GUEST_CR0:
		return vmcb_get_reg(GUEST_CR0);
	case GUEST_CR2:
		return vmcb_get_reg(GUEST_CR2);
	case GUEST_CR3:
		return vmcb_get_reg(GUEST_CR3);
	case GUEST_CR4:
		return vmcb_get_reg(GUEST_CR4);
	default:
		return 0xffffffff;
	}
}

void
svm_set_reg(guest_reg_t reg, uint32_t val)
{
	switch (reg) {
	case GUEST_EAX:
		vmcb_set_reg(GUEST_EAX, val);
		break;
	case GUEST_EBX:
		svm0.g_rbx = val;
		break;
	case GUEST_ECX:
		svm0.g_rcx = val;
		break;
	case GUEST_EDX:
		svm0.g_rdx = val;
		break;
	case GUEST_ESI:
		svm0.g_rsi = val;
		break;
	case GUEST_EDI:
		svm0.g_rdi = val;
		break;
	case GUEST_EBP:
		svm0.g_rbp = val;
		break;
	case GUEST_ESP:
		vmcb_set_reg(GUEST_ESP, val);
		break;
	case GUEST_EIP:
		vmcb_set_reg(GUEST_EIP, val);
		break;
	case GUEST_EFLAGS:
		vmcb_set_reg(GUEST_EFLAGS, val);
		break;
	case GUEST_CR0:
		vmcb_set_reg(GUEST_CR0, val);
		break;
	case GUEST_CR2:
		vmcb_set_reg(GUEST_CR2, val);
		break;
	case GUEST_CR3:
		vmcb_set_reg(GUEST_CR3, val);
		break;
	case GUEST_CR4:
		vmcb_set_reg(GUEST_CR4, val);
		break;
	default:
	}
}

void
svm_set_seg(guest_seg_t seg,
	    uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar)
{
	vmcb_set_seg(seg, sel, base, lim, ar);
}

void
svm_set_mmap(uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT((gpa % PAGESIZE) == 0 || (hpa % PAGESIZE) == 0);
	npt_insert(gpa, hpa);
}

void
svm_inject_event(guest_event_t type, uint8_t vector, uint32_t errcode, bool ev)
{
	int evt_type;

	if (type == EVENT_EXTINT)
		evt_type = SVM_EVTINJ_TYPE_INTR;
	else if (type == EVENT_EXCEPTION)
		evt_type = SVM_EVTINJ_TYPE_EXEPT;
	else
		return;

	vmcb_inject_event(evt_type, vector, errcode, ev);
}

uint32_t
svm_get_next_eip(void)
{
	return vmcb_get_next_eip();
}

bool
svm_check_pending_event(void)
{
	return vmcb_check_pending_event();
}

bool
svm_check_int_shadow(void)
{
	return vmcb_check_int_shadow();
}

exit_reason_t
svm_get_exit_reason(void)
{
	return svm0.exit_reason;
}

uint16_t
svm_get_exit_io_port(void)
{
	return svm0.exit_info.ioport.port;
}

data_sz_t
svm_get_exit_io_width(void)
{
	return svm0.exit_info.ioport.dw;
}

bool
svm_get_exit_io_write(void)
{
	return svm0.exit_info.ioport.write;
}

bool
svm_get_exit_io_rep(void)
{
	return svm0.exit_info.ioport.rep;
}

bool
svm_get_exit_io_str(void)
{
	return svm0.exit_info.ioport.str;
}

uint32_t
svm_get_exit_io_neip(void)
{
	return svm0.exit_info.ioport.neip;
}

uintptr_t
svm_get_exit_fault_addr(void)
{
	return svm0.exit_info.pgflt.addr;
}

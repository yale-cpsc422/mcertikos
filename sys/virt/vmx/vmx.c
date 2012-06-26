#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <sys/virt/vmm.h>

#include "ept.h"
#include "vmcs.h"
#include "vmx.h"
#include "vmx_controls.h"
#include "vmx_msr.h"
#include "x86.h"

#define PINBASED_CTLS_ONE_SETTING		\
	PINBASED_EXTINT_EXITING		|	\
	PINBASED_NMI_EXITING		|	\
	PINBASED_VIRTUAL_NMI
#define PINBASED_CTLS_ZERO_SETTING		\
	PINBASED_PREMPTION_TIMER

#define PROCBASED_CTLS_WINDOW_SETTING		\
	PROCBASED_INT_WINDOW_EXITING	|	\
	PROCBASED_NMI_WINDOW_EXITING

#define PROCBASED_CTLS_ONE_SETTING		\
	(PROCBASED_TSC_OFFSET		|	\
	 PROCBASED_IO_BITMAPS		|	\
	 PROCBASED_MSR_BITMAPS		|	\
	 PROCBASED_CTLS_WINDOW_SETTING	|	\
	 PROCBASED_SECONDARY_CONTROLS	|	\
	 /* unsupported instructions */		\
	 PROCBASED_HLT_EXITING		|	\
	 PROCBASED_INVLPG_EXITING	|	\
	 PROCBASED_MWAIT_EXITING	|	\
	 PROCBASED_RDPMC_EXITING	|	\
	 PROCBASED_MONITOR_EXITING)
#define PROCBASED_CTLS_ZERO_SETTING		\
	(PROCBASED_CR8_LOAD_EXITING	|	\
	 PROCBASED_CR8_STORE_EXITING	|	\
	 PROCBASED_USE_TPR_SHADOW	|	\
	 PROCBASED_MOV_DR_EXITING	|	\
	 PROCBASED_MTF)

#define PROCBASED_CTLS2_ONE_SETTING		\
	(PROCBASED2_ENABLE_EPT		|	\
	 PROCBASED2_ENABLE_VPID		|	\
	 PROCBASED2_UNRESTRICTED_GUEST	|	\
	 /* unsupported instructions */		\
	 PROCBASED2_WBINVD_EXITING)
#define PROCBASED_CTLS2_ZERO_SETTING		\
	(PROCBASED2_VIRTUALIZE_APIC	|	\
	 PROCBASED2_DESC_TABLE_EXITING	|	\
	 PROCBASED2_ENABLE_RDTSCP	|	\
	 PROCBASED2_VIRTUALIZE_X2APIC	|	\
	 PROCBASED2_PAUSE_LOOP_EXITING	|	\
	 PROCBASED2_RDRAND_EXITING	|	\
	 PROCBASED2_ENABLE_INVPCID	|	\
	 PROCBASED2_ENABLE_VMFUNC)

#define VM_EXIT_CTLS_ONE_SETTING		\
	(VM_EXIT_SAVE_PAT		|	\
	 VM_EXIT_LOAD_PAT		|	\
	 VM_EXIT_SAVE_EFER		|	\
	 VM_EXIT_LOAD_EFER)
#define VM_EXIT_CTLS_ZERO_SETTING		\
	(VM_EXIT_SAVE_DEBUG_CONTROLS	|	\
	 VM_EXIT_HOST_LMA		|	\
	 VM_EXIT_LOAD_PERF_GLOBAL_CTRL	|	\
	 VM_EXIT_ACKNOWLEDGE_INTERRUPT	|	\
	 VM_EXIT_SAVE_PREEMPTION_TIMER)

#define VM_ENTRY_CTLS_ONE_SETTING		\
	(VM_ENTRY_LOAD_PAT		|	\
	 VM_ENTRY_LOAD_EFER)
#define VM_ENTRY_CTLS_ZERO_SETTING		\
	(VM_ENTRY_LOAD_DEBUG_CONTROLS	|	\
	 VM_ENTRY_GUEST_LMA		|	\
	 VM_ENTRY_INTO_SMM		|	\
	 VM_ENTRY_DEACTIVATE_DUAL_MONITOR |	\
	 VM_ENTRY_LOAD_PERF_GLOBAL_CTRL)

static uint32_t procbased_ctls, procbased_ctls2, pinbased_ctls;
static uint32_t exit_ctls, entry_ctls;

static int
vmx_init(void)
{
	int error;
	uint32_t dummy, val;

	/* CPUID.1:ECX[bit 5] must be 1 for processor to support VMX */
	cpuid(0x00000001, &dummy, &dummy, &val, &dummy);
	if (!(val & CPUID_FEATURE_VMX)) {
		KERN_DEBUG("No VMX (cpuid 0x1 : ecx=0x%08x).\n", val);
		return 1;
	}

	KERN_DEBUG("MSR_VMX_BASIC = 0x%llx\n", rdmsr(MSR_VMX_BASIC));

	/* setup pin-based control registers */
	KERN_DEBUG("MSR_VMX_PINBASED_CTLS = 0x%llx\n",
		   rdmsr(MSR_VMX_PINBASED_CTLS));
	if ((rdmsr(MSR_VMX_BASIC) & (1ULL << 55)))
		KERN_DEBUG("MSR_VMX_TRUE_PINBASED_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_PINBASED_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_PINBASED_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_PINBASED_CTLS, MSR_VMX_TRUE_EXIT_CTLS,
		 PINBASED_CTLS_ONE_SETTING, PINBASED_CTLS_ZERO_SETTING,
		 &pinbased_ctls);
	if (error) {
		KERN_DEBUG("Not support desired pin-based controls. "
			   "(error=%d)\n", error);
		return (error);
	}

	/* setup primary processor-based control registrers */
	KERN_DEBUG("MSR_VMX_PROCBASED_CTLS = 0x%llx\n",
		   rdmsr(MSR_VMX_PROCBASED_CTLS));
	if ((rdmsr(MSR_VMX_BASIC) & (1ULL << 55)))
		KERN_DEBUG("MSR_VMX_TRUE_PROCBASED_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_PROCBASED_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_PROCBASED_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_PROCBASED_CTLS,     MSR_VMX_TRUE_PROCBASED_CTLS,
		 PROCBASED_CTLS_ONE_SETTING, PROCBASED_CTLS_ZERO_SETTING,
		 &procbased_ctls);
	if (error) {
		KERN_DEBUG("Not support desired primary processor-based "
			   "controls. (error=%d)\n", error);
		return (error);
	}
	procbased_ctls &= ~PROCBASED_CTLS_WINDOW_SETTING;

	/* setup secondary processor-based control registers */
	KERN_DEBUG("MSR_VMX_PROCBASED_CTLS2 = 0x%llx\n",
		   rdmsr(MSR_VMX_PROCBASED_CTLS2));
	error = vmx_set_ctlreg
		(MSR_VMX_PROCBASED_CTLS,      MSR_VMX_PROCBASED_CTLS2,
		 PROCBASED_CTLS2_ONE_SETTING, PROCBASED_CTLS2_ZERO_SETTING,
		 &procbased_ctls2);
	if (error) {
		KERN_DEBUG("Not support desired secondary processor-based "
			   "controls. (error=%d)\n", error);
		return (error);
	}

	/* setup VM exit control registers */
	KERN_DEBUG("MSR_VMX_EXIT_CTLS = 0x%llx\n", rdmsr(MSR_VMX_EXIT_CTLS));
	if (rdmsr(MSR_VMX_BASIC) & (1ULL << 51))
		KERN_DEBUG("MSR_VMX_TRUE_EXIT_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_EXIT_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_EXIT_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_EXIT_CTLS, MSR_VMX_TRUE_EXIT_CTLS,
		 VM_EXIT_CTLS_ONE_SETTING, VM_EXIT_CTLS_ZERO_SETTING,
		 &exit_ctls);
	if (error) {
		KERN_DEBUG("Not support desired VM-exit controls. (error=%d)\n",
			   error);
		return (error);
	}

	/* setup VM entry control registers */
	KERN_DEBUG("MSR_VMX_ENTRY_CTLS = 0x%llx\n", rdmsr(MSR_VMX_ENTRY_CTLS));
	if (rdmsr(MSR_VMX_BASIC) & (1ULL << 51))
		KERN_DEBUG("MSR_VNX_TRUE_ENTRY_CTLS = 0x%llx\n",
			   rdmsr(MSR_VMX_TRUE_ENTRY_CTLS));
	else
		KERN_DEBUG("NO MSR_VMX_TRUE_ENTRY_CTLS\n");
	error = vmx_set_ctlreg
		(MSR_VMX_ENTRY_CTLS, MSR_VMX_TRUE_ENTRY_CTLS,
		 VM_ENTRY_CTLS_ONE_SETTING, VM_ENTRY_CTLS_ZERO_SETTING,
		 &entry_ctls);
	if (error) {
		KERN_DEBUG("Not support desired VM-entry controls. "
			   "(error=%d)\n", error);
		return (error);
	}

	return 0;
}

struct vmm_ops vmm_ops_intel = {
	.vmm_init		= vmx_init,
	.vm_init		= NULL,
	.vm_run			= NULL,
	.vm_exit_handle		= NULL,
	.vm_intr_handle		= NULL,
	.vm_enter_tsc		= NULL,
	.vm_exit_tsc		= NULL,
	.vm_intercept_ioio	= NULL,
	.vm_translate_gp2hp	= NULL,
};

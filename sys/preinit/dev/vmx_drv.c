#include <lib/gcc.h>
#include <lib/string.h>

#include <preinit/lib/debug.h>
#include <preinit/lib/string.h>
#include <preinit/lib/types.h>
#include <preinit/lib/x86.h>

#include <preinit/dev/vmx_drv.h>

#include <kern/virt/vmx/x86.h>
#include <kern/virt/vmx/vmx.h>
#include <kern/virt/vmx/ept.h>

#define MSR_VMX_BASIC           0x480
#define PAGESIZE 4096

extern struct vmx_info vmx_info;

/*
 * Enable VMX. 
 */
static int 
vmx_enable(void)
{
    int error;

    lcr4(rcr4() | CR4_VMXE);
    memzero(vmx_info.vmx_region, PAGESIZE);

    vmx_info.vmx_region[0] = rdmsr(MSR_VMX_BASIC) & 0xffffffff;

    VMX_DEBUG("VMX region initialized.\n");

    error = vmxon((char *)vmx_info.vmx_region);

	VMX_DEBUG("VMX is enabled.\n");

    return error;
}

extern struct vmx vmx;

int
vmx_hw_init(void)
{
    int error;
    uint32_t dummy;
    uint32_t val;
    uint64_t fixed0;
    uint64_t fixed1;

    VMX_DEBUG("In vmx_hw_init.\n");

    memset(&vmx, 0, sizeof(vmx));

    /* CPUID.1:ECX[bit 5] must be 1 for processor to support VMX */
    cpuid(0x00000001, &dummy, &dummy, &val, &dummy);
    if (!(val & CPUID_FEATURE_VMX)) {
        VMX_DEBUG("No VMX (cpuid 0x1 : ecx=0x%08x).\n", val);
        return 1;
    }

    VMX_DEBUG("MSR_VMX_BASIC = 0x%llx\n", rdmsr(MSR_VMX_BASIC));

    /* setup pin-based control registers */
    VMX_DEBUG("MSR_VMX_PINBASED_CTLS = 0x%llx\n",
          rdmsr(MSR_VMX_PINBASED_CTLS));
    if ((rdmsr(MSR_VMX_BASIC) & (1ULL << 55)))
        VMX_DEBUG("MSR_VMX_TRUE_PINBASED_CTLS = 0x%llx\n",
              rdmsr(MSR_VMX_TRUE_PINBASED_CTLS));
    else
        VMX_DEBUG("NO MSR_VMX_TRUE_PINBASED_CTLS\n");
    error = vmx_set_ctlreg
        (MSR_VMX_PINBASED_CTLS,     MSR_VMX_TRUE_PINBASED_CTLS,
         PINBASED_CTLS_ONE_SETTING, PINBASED_CTLS_ZERO_SETTING,
         &vmx_info.pinbased_ctls);
    if (error) {
        VMX_DEBUG("Not support desired pin-based controls. "
              "(error=%d)\n", error);
        return (error);
    }
    VMX_DEBUG("pin-based ctls 0x%08x\n", vmx_info.pinbased_ctls);

    /* setup primary processor-based control registrers */
    VMX_DEBUG("MSR_VMX_PROCBASED_CTLS = 0x%llx\n",
          rdmsr(MSR_VMX_PROCBASED_CTLS));
    if ((rdmsr(MSR_VMX_BASIC) & (1ULL << 55)))
        VMX_DEBUG("MSR_VMX_TRUE_PROCBASED_CTLS = 0x%llx\n",
              rdmsr(MSR_VMX_TRUE_PROCBASED_CTLS));
    else
        VMX_DEBUG("NO MSR_VMX_TRUE_PROCBASED_CTLS\n");
    error = vmx_set_ctlreg
        (MSR_VMX_PROCBASED_CTLS,     MSR_VMX_TRUE_PROCBASED_CTLS,
         PROCBASED_CTLS_ONE_SETTING, PROCBASED_CTLS_ZERO_SETTING,
         &vmx_info.procbased_ctls);
    if (error) {
        VMX_DEBUG("Not support desired primary processor-based "
              "controls. (error=%d)\n", error);
        return (error);
    }
    vmx_info.procbased_ctls &= ~PROCBASED_CTLS_WINDOW_SETTING;
    VMX_DEBUG("primary processor-based ctls 0x%08x\n",
          vmx_info.procbased_ctls);

    /* setup secondary processor-based control registers */
    VMX_DEBUG("MSR_VMX_PROCBASED_CTLS2 = 0x%llx\n",
          rdmsr(MSR_VMX_PROCBASED_CTLS2));
    error = vmx_set_ctlreg
        (MSR_VMX_PROCBASED_CTLS,      MSR_VMX_PROCBASED_CTLS2,
         PROCBASED_CTLS2_ONE_SETTING, PROCBASED_CTLS2_ZERO_SETTING,
         &vmx_info.procbased_ctls2);
    if (error) {
        VMX_DEBUG("Not support desired secondary processor-based "
              "controls. (error=%d)\n", error);
        return (error);
    }
    VMX_DEBUG("secondary processor-based ctls 0x%08x\n",
          vmx_info.procbased_ctls2);

    /* setup VM exit control registers */
    VMX_DEBUG("MSR_VMX_EXIT_CTLS = 0x%llx\n", rdmsr(MSR_VMX_EXIT_CTLS));
    if (rdmsr(MSR_VMX_BASIC) & (1ULL << 51))
        VMX_DEBUG("MSR_VMX_TRUE_EXIT_CTLS = 0x%llx\n",
              rdmsr(MSR_VMX_TRUE_EXIT_CTLS));
    else
        VMX_DEBUG("NO MSR_VMX_TRUE_EXIT_CTLS\n");
    error = vmx_set_ctlreg
        (MSR_VMX_EXIT_CTLS,        MSR_VMX_TRUE_EXIT_CTLS,
         VM_EXIT_CTLS_ONE_SETTING, VM_EXIT_CTLS_ZERO_SETTING,
         &vmx_info.exit_ctls);
    if (error) {
        VMX_DEBUG("Not support desired VM-exit controls. (error=%d)\n",
              error);
        return (error);
    }
    VMX_DEBUG("exit ctls 0x%08x\n", vmx_info.exit_ctls);

    /* setup VM entry control registers */
    VMX_DEBUG("MSR_VMX_ENTRY_CTLS = 0x%llx\n", rdmsr(MSR_VMX_ENTRY_CTLS));
    if (rdmsr(MSR_VMX_BASIC) & (1ULL << 51))
        VMX_DEBUG("MSR_VNX_TRUE_ENTRY_CTLS = 0x%llx\n",
              rdmsr(MSR_VMX_TRUE_ENTRY_CTLS));
    else
        VMX_DEBUG("NO MSR_VMX_TRUE_ENTRY_CTLS\n");
    error = vmx_set_ctlreg
        (MSR_VMX_ENTRY_CTLS,        MSR_VMX_TRUE_ENTRY_CTLS,
         VM_ENTRY_CTLS_ONE_SETTING, VM_ENTRY_CTLS_ZERO_SETTING,
         &vmx_info.entry_ctls);
    if (error) {
        VMX_DEBUG("Not support desired VM-entry controls. "
              "(error=%d)\n", error);
        return (error);
    }
    VMX_DEBUG("entry ctls 0x%08x\n", vmx_info.entry_ctls);

    /* initialize EPT */
    error = ept_init();
    if (error) {
        VMX_DEBUG("Cannot initalize EPT. (error=%d)\n", error);
        return error;
    }

    /* check fixed bits of CR0 */
    fixed0 = rdmsr(MSR_VMX_CR0_FIXED0);
    fixed1 = rdmsr(MSR_VMX_CR0_FIXED1);
    VMX_DEBUG("CR0 1s mask 0x%llx, 0s mask 0x%llx.\n",
          (fixed0 & fixed1) & ~(CR0_PG | CR0_PE), (CR0_NW | CR0_CD) | (~fixed0 & ~fixed1));

    /* check fixed bits of CR4 */
    fixed0 = rdmsr(MSR_VMX_CR4_FIXED0);
    fixed1 = rdmsr(MSR_VMX_CR4_FIXED1);
    VMX_DEBUG("CR4 1s mask 0x%llx, 0s mask 0x%llx.\n",
          fixed0 & fixed1, ~fixed0 & ~fixed1);

    /* enable VMX */
    error = vmx_enable();
    if (error) {
        VMX_DEBUG("Cannot enable VMX.\n");
        return error;
    }

    return 0;
}


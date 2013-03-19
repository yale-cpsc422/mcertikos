#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/intr.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/vmm.h>
#include <sys/virt/vmm_dev.h>
#include <sys/virt/dev/pic.h>

#include <dev/pic.h>
#include <dev/tsc.h>

#ifdef DEBUG_VIRT

#define VIRT_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("VMM: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define VIRT_DEBUG(fmt...)			\
	do {					\
	} while (0)

#endif

static bool vmm_inited = FALSE;

static struct vm vm_pool[MAX_VMID];
static spinlock_t vm_pool_lock;

static struct vmm_ops *vmm_ops = NULL;

#define CODE_SEG_AR (GUEST_SEG_ATTR_P | GUEST_SEG_ATTR_S |	\
		     GUEST_SEG_TYPE_CODE | GUEST_SEG_ATTR_A)
#define DATA_SEG_AR (GUEST_SEG_ATTR_P | GUEST_SEG_ATTR_S |	\
		     GUEST_SEG_TYPE_DATA | GUEST_SEG_ATTR_A)
#define LDT_AR      (GUEST_SEG_ATTR_P | GUEST_SEG_TYPE_LDT)
#define TSS_AR      (GUEST_SEG_ATTR_P | GUEST_SEG_TYPE_TSS)

static struct guest_seg_desc guest_seg_desc[GUEST_MAX_SEG_DESC] = {
	/* 0: code segment */
	{ .sel = 0xf000, .base = 0x000f0000, .lim = 0xffff, .ar = CODE_SEG_AR },
	/* 1: data segment */
	{ .sel = 0x0000, .base = 0x00000000, .lim = 0xffff, .ar = DATA_SEG_AR },
	/* 2: LDT */
	{ .sel = 0, .base = 0, .lim = 0xffff, .ar = LDT_AR },
	/* 3: TSS */
	{ .sel = 0, .base = 0, .lim = 0xffff, .ar = TSS_AR },
	/* 4: GDT/IDT */
	{ .sel = 0, .base = 0, .lim = 0xffff, .ar = 0 }
};

#undef CODE_SEG_AR
#undef DATA_SEG_AR
#undef LDT_AR
#undef TSS_AR

#define is_intel()							\
	(strncmp(pcpu_cur()->arch_info.vendor, "GenuineIntel", 20) == 0)
#define is_amd()							\
	(strncmp(pcpu_cur()->arch_info.vendor, "AuthenticAMD", 20) == 0)

static struct vm *
vmm_alloc_vm(void)
{
	struct vm *new_vm = NULL;
	int i;

	spinlock_acquire(&vm_pool_lock);

	for (i = 0; i < MAX_VMID; i++)
		if (vm_pool[i].used == FALSE)
			break;
	if (likely(i < MAX_VMID)) {
		new_vm = &vm_pool[i];
		new_vm->used = TRUE;
	}

	spinlock_release(&vm_pool_lock);
	return new_vm;
}

static void
vmm_update_guest_tsc(struct vm *vm, uint64_t last_h_tsc, uint64_t cur_h_tsc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(cur_h_tsc >= last_h_tsc);
	uint64_t delta = cur_h_tsc - last_h_tsc;
	vm->tsc += (delta * vm->cpufreq) / (tsc_per_ms * 1000);
}

static int
vmm_handle_ioport(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_IOPORT);

	exit_info_t *exit_info = &vm->exit_info;
	uint16_t port;
	data_sz_t width;
	uint32_t eax, next_eip;

	if (exit_info->ioport.rep == TRUE) {
		KERN_PANIC("REP I/O instructions is not implemented yet.\n");
		return 1;
	}

	if (exit_info->ioport.str == TRUE) {
		KERN_PANIC("String operation is not implemented yet.\n");
		return 2;
	}

	port = exit_info->ioport.port;
	width = exit_info->ioport.width;

	/*
	 * XXX: I/O permission check is not necessary when using HVM. If the
	 *      check fails, the corresponding exception, instead of an I/O
	 *      related VMEXIT, will happen in the guest.
	 */

	if (exit_info->ioport.write == TRUE) {
		vmm_ops->get_reg(vm, GUEST_EAX, &eax);
#ifdef DEBUG_GUEST_IOPORT
		KERN_DEBUG("Write guest I/O port 0x%x, width %d bytes, "
			   "val 0x%08x.\n", port, 1 << width, eax);
#endif
		vdev_write_guest_ioport(vm, port, width, eax);
		vmm_ops->get_next_eip(vm, INSTR_OUT, &next_eip);
	} else {
		vdev_read_guest_ioport(vm, port, width, &eax);
		vmm_ops->set_reg(vm, GUEST_EAX, eax);
		vmm_ops->get_next_eip(vm, INSTR_IN, &next_eip);
#ifdef DEBUG_GUEST_IOPORT
		KERN_DEBUG("Read guest I/O port 0x%x, width %d bytes, "
			   "val 0x%08x.\n", port, 1 << width, eax);
#endif
	}

	vmm_ops->set_reg(vm, GUEST_EIP, next_eip);

	return 0;
}

static int
vmm_handle_rdmsr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_RDMSR);

	uint32_t msr, next_eip;
	uint64_t val;

	vmm_ops->get_reg(vm, GUEST_ECX, &msr);

	/*
	 * XXX: I/O permission check is not necessary when using HVM.
	 */
	if (vmm_ops->get_msr(vm, msr, &val)) {
#ifdef DEBUG_GUEST_MSR
		KERN_DEBUG("Guest rdmsr failed: invalid MSR 0x%llx.\n", msr);
#endif
		vmm_ops->inject_event(vm, EVENT_EXCEPTION, T_GPFLT, 0, TRUE);
		return 0;
	}

#ifdef DEBUG_GUEST_MSR
	KERN_DEBUG("Guest rdmsr 0x%08x = 0x%llx.\n", msr, val);
#endif

	vmm_ops->set_reg(vm, GUEST_EAX, val & 0xffffffff);
	vmm_ops->set_reg(vm, GUEST_EDX, (val >> 32) & 0xffffffff);

	vmm_ops->get_next_eip(vm, INSTR_RDMSR, &next_eip);
	vmm_ops->set_reg(vm, GUEST_EIP, next_eip);

	return 0;
}

static int
vmm_handle_wrmsr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_WRMSR);

	uint32_t msr, next_eip, eax, edx;
	uint64_t val;

	vmm_ops->get_reg(vm, GUEST_ECX, &msr);

	vmm_ops->get_reg(vm, GUEST_EAX, &eax);
	vmm_ops->get_reg(vm, GUEST_EDX, &edx);
	val = ((uint64_t) edx << 32) | (uint64_t) eax;

	/*
	 * XXX: I/O permission check is not necessary when using HVM.
	 */
	if (vmm_ops->set_msr(vm, msr, val)) {
#ifdef DEBUG_GUEST_MSR
		KERN_DEBUG("Guest wrmsr failed: invalid MSR 0x%llx.\n", msr);
#endif
		vmm_ops->inject_event(vm, EVENT_EXCEPTION, T_GPFLT, 0, TRUE);
		return 0;
	}

#ifdef DEBUG_GUEST_MSR
	KERN_DEBUG("Guest wrmsr 0x%08x, 0x%llx.\n", msr, val);
#endif

	vmm_ops->get_next_eip(vm, INSTR_WRMSR, &next_eip);
	vmm_ops->set_reg(vm, GUEST_EIP, next_eip);

	return 0;
}

static int
vmm_handle_pgflt(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_PGFLT);

	exit_info_t *exit_info = &vm->exit_info;
	uint32_t fault_pa = exit_info->pgflt.addr;
	pageinfo_t *pi;
	uintptr_t host_pa;

	if (vm->memsize <= fault_pa && fault_pa < 0xf0000000) {
		KERN_PANIC("EPT/NPT fault @ 0x%08x: out of range.\n", fault_pa);
		return 1;
	}

	if ((pi = mem_page_alloc()) == NULL) {
		KERN_PANIC("EPT/NPT fault @ 0x%08x: no host memory.\n",
			   fault_pa);
		return 2;
	}

	host_pa = mem_pi2phys(pi);
	memzero((void *) host_pa, PAGESIZE);

	if ((vmm_ops->set_mmap(vm, fault_pa, host_pa))) {
		KERN_PANIC("EPT/NPT fault @ 0x%08x: cannot be mapped to "
			   "HPA 0x%08x.\n", fault_pa, host_pa);
		return 3;
	}

#ifdef DEBUG_GUEST_PGFLT
	KERN_DEBUG("EPT/NPT fault @ 0x%08x: mapped to HPA 0x%08x.\n",
		   fault_pa, host_pa);
#endif
	return 0;
}

static int
vmm_handle_cpuid(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_CPUID);

	uint32_t func1, func2, eax, ebx, ecx, edx, next_eip;

	vmm_ops->get_reg(vm, GUEST_EAX, &func1);
	vmm_ops->get_reg(vm, GUEST_ECX, &func2);

	vmm_ops->get_cpuid(vm, func1, func2, &eax, &ebx, &ecx, &edx);
	vmm_ops->set_reg(vm, GUEST_EAX, eax);
	vmm_ops->set_reg(vm, GUEST_EBX, ebx);
	vmm_ops->set_reg(vm, GUEST_ECX, ecx);
	vmm_ops->set_reg(vm, GUEST_EDX, edx);

#ifdef DEBUG_GUEST_CPUID
	KERN_DEBUG("Guest cpuid eax=0x%08x ecx=0x%08x: "
		   "eax=0x%08x, ebx=0x%08x, ecx=0x%08x, edx=0x%08x.\n",
		   func1, func2, eax, ebx, ecx, edx);
#endif

	vmm_ops->get_next_eip(vm, INSTR_CPUID, &next_eip);
	vmm_ops->set_reg(vm, GUEST_EIP, next_eip);

	return 0;
}

static int
vmm_handle_rdtsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_RDTSC);

	uint32_t next_eip;

	vmm_ops->set_reg(vm, GUEST_EDX, (vm->tsc >> 32) & 0xffffffff);
	vmm_ops->set_reg(vm, GUEST_EAX, vm->tsc & 0xffffffff);

#ifdef DEBUG_GUEST_TSC
	KERN_DEBUG("Guest rdtsc = 0x%llx.\n", vm->tsc);
#endif

	vmm_ops->get_next_eip(vm, INSTR_RDTSC, &next_eip);
	vmm_ops->set_reg(vm, GUEST_EIP, next_eip);

	return 0;
}

static int
vmm_handle_hypercall(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_HYPERCALL);
	KERN_WARN("vmm_handle_hypercall() not implemented yet.\n");

	uint32_t next_eip;

	vmm_ops->get_next_eip(vm, INSTR_HYPERCALL, &next_eip);
	vmm_ops->set_reg(vm, GUEST_EIP, next_eip);

	return 0;
}

static int
vmm_handle_invalid_instr(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_INVAL_INSTR);

	VIRT_DEBUG("Invalid instruction.\n");

	vmm_ops->inject_event(vm, EVENT_EXCEPTION, T_ILLOP, 0, FALSE);
	return 0;
}

static int
vmm_handle_exit(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason != EXIT_NONE);

	int rc = 0;

	switch (vm->exit_reason) {
	case EXIT_FOR_EXTINT:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_INTR)
		KERN_DEBUG("VMEXIT for external interrupt.\n");
#endif
		KERN_ASSERT(vm->exit_handled == TRUE);
		break;

	case EXIT_FOR_INTWIN:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_INTR)
		KERN_DEBUG("VMEXIT for interrupt window.\n");
#endif
		rc = vmm_ops->intercept_intr_window(vm, FALSE);
		break;

	case EXIT_FOR_IOPORT:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_IOPORT)
		KERN_DEBUG("VMEXIT for I/O port.\n");
#endif
		rc = vmm_handle_ioport(vm);
		break;

	case EXIT_FOR_RDMSR:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_MSR)
		KERN_DEBUG("VMEXIT for rdmsr.\n");
#endif
		rc = vmm_handle_rdmsr(vm);
		break;

	case EXIT_FOR_WRMSR:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_MSR)
		KERN_DEBUG("VMEXIT for wrmsr.\n");
#endif
		rc = vmm_handle_wrmsr(vm);
		break;

	case EXIT_FOR_PGFLT:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_PGFLT)
		KERN_DEBUG("VMEXIT for page fault.\n");
#endif
		rc = vmm_handle_pgflt(vm);
		break;

	case EXIT_FOR_CPUID:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_CPUID)
		KERN_DEBUG("VMEXIT for cpuid.\n");
#endif
		rc = vmm_handle_cpuid(vm);
		break;

	case EXIT_FOR_RDTSC:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_GUEST_TSC)
		KERN_DEBUG("VMEXIT for rdtsc.\n");
#endif
		rc = vmm_handle_rdtsc(vm);
		break;

	case EXIT_FOR_HYPERCALL:
#if defined (DEBUG_VMEXIT) || defined (DEBUG_HYPERCALL)
		KERN_DEBUG("VMEXIT for hypercall.\n");
#endif
		rc = vmm_handle_hypercall(vm);
		break;

	case EXIT_FOR_INVAL_INSTR:
#ifdef DEBUG_VMEXIT
		KERN_DEBUG("VMEXIT for invalid instruction.\n");
#endif
		rc = vmm_handle_invalid_instr(vm);
		break;

	default:
#ifdef DEBUG_VMEXIT
		KERN_DEBUG("VMEXIT for unknown reason %d.\n", vm->exit_reason);
#endif
		rc = 1;
	}

	return rc;
}

/*
 * @return 0 if no interrupt is injected; otherwise, return the number of
 *         injected interrupts.
 */
static int
vmm_intr_assist(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	int vector;
	uint32_t eflags;
	int blocked = 0;

	/* no interrupt needs to be injected */
	if ((vector = vdev_peep_intout(vm)) == -1) {
#if defined (DEBUG_GUEST_INTR) || defined (DEBUG_GUEST_INJECT)
		KERN_DEBUG("Found no interrupt.\n");
#endif
		return 0;
	}

	if (vmm_ops->pending_event(vm) == TRUE) {
#if defined (DEBUG_GUEST_INTR) || defined (DEBUG_GUEST_INJECT)
		KERN_DEBUG("Found pending event.\n");
#endif
		blocked = 1;
		goto after_check;
	}

	/* check if the virtual CPU is able to accept the interrupt */
	vmm_ops->get_reg(vm, GUEST_EFLAGS, &eflags);
	if (!(eflags & FL_IF)) {
#if defined (DEBUG_GUEST_INTR) || defined (DEBUG_GUEST_INJECT)
		KERN_DEBUG("Guest EFLAGS.IF = 0.\n");
#endif
		blocked = 1;
		goto after_check;
	}
	if (vmm_ops->intr_shadow(vm) == TRUE) {
#if defined (DEBUG_GUEST_INTR) || defined (DEBUG_GUEST_INJECT)
		KERN_DEBUG("Guest in interrupt shadow.\n");
#endif
		blocked = 1;
	}

 after_check:
	/*
	 * If not, enable intercepting the interrupt window so that CertiKOS
	 * will be acknowledged once the virtual CPU is able to accept the
	 * interrupt.
	 */
	if (blocked) {
		vmm_ops->intercept_intr_window(vm, TRUE);
		return 0;
	}

	if ((vector = vdev_read_intout(vm)) == -1)
		return 0;

	/* inject the interrupt and disable intercepting the interrupt window */
#if defined (DEBUG_GUEST_INTR) || defined (DEBUG_GUEST_INJECT)
	KERN_DEBUG("Inject ExtINTR vec=0x%x.\n", vector);
#endif
	vmm_ops->inject_event(vm, EVENT_EXTINT, vector, 0, FALSE);
	vmm_ops->intercept_intr_window(vm, FALSE);

	return 1;
}

static int
vmm_load_bios(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	extern uint8_t _binary___misc_bios_bin_start[],
		_binary___misc_bios_bin_size[],
		_binary___misc_vgabios_bin_start[],
		_binary___misc_vgabios_bin_size[];

	/* load BIOS ROM */
	KERN_ASSERT((size_t) _binary___misc_bios_bin_size % 0x10000 == 0);
	vmm_memcpy_to_guest(vm,
			    0x100000 - (size_t) _binary___misc_bios_bin_size,
			    (uintptr_t) _binary___misc_bios_bin_start,
			    (size_t) _binary___misc_bios_bin_size);

	/* load VGA BIOS ROM */
	vmm_memcpy_to_guest(vm, 0xc0000,
			    (uintptr_t) _binary___misc_vgabios_bin_start,
			    (size_t) _binary___misc_vgabios_bin_size);

	return 0;
}

#ifdef TRACE_VIRT

static void
vmm_trace_output(struct vm_perf_trace *trace)
{
	/* dprintf("\nPerformance trace information in the past %d s:\n" */
	/* 	"  In-guest time       %lld ms\n" */
	/* 	"  In-host time        %lld ms\n" */
	/* 	"    I/O port          %lld ms \t(%3d \%)\n" */
	/* 	"    IRQ               %lld ms \t(%3d \%)\n" */
	/* 	"    Interrupt window  %lld ms \t(%3d \%)\n" */
	/* 	"    MSR               %lld ms \t(%3d \%)\n" */
	/* 	"    CPUID             %lld ms \t(%3d \%)\n" */
	/* 	"    Page fault        %lld ms \t(%3d \%)\n" */
	/* 	"    TSC               %lld ms \t(%3d \%)\n" */
	/* 	"    Hypercall         %lld ms \t(%3d \%)\n" */
	/* 	"    Others            %lld ms \t(%3d \%)\n" */
	/* 	"  Total VM entries    %lld\n" */
	/* 	"  Total VM exits      %lld\n" */
	/* 	"    I/O port          %lld \t(%3d \%)\n" */
	/* 	"    IRQ               %lld \t(%3d \%)\n" */
	/* 	"    Interrupt window  %lld \t(%3d \%)\n" */
	/* 	"    MSR               %lld \t(%3d \%)\n" */
	/* 	"    CPUID             %lld \t(%3d \%)\n" */
	/* 	"    Page fault        %lld \t(%3d \%)\n" */
	/* 	"    TSC               %lld \t(%3d \%)\n" */
	/* 	"    Hypercall         %lld \t(%3d \%)\n" */
	/* 	"    Others            %lld \t(%3d \%)\n", */
	dprintf("  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld"
		"  %lld",
		trace->in_guest_time / tsc_per_ms,
		trace->in_host_time / tsc_per_ms,
		trace->total_ioport_time / tsc_per_ms,
		trace->total_irq_time / tsc_per_ms,
		trace->total_intwin_time / tsc_per_ms,
		trace->total_msr_time / tsc_per_ms,
		trace->total_cpuid_time / tsc_per_ms,
		trace->total_pgflt_time / tsc_per_ms,
		trace->total_tsc_time / tsc_per_ms,
		trace->total_hcall_time / tsc_per_ms,
		trace->other_time / tsc_per_ms,
		trace->exit_counter,
		trace->total_ioport_counter,
		trace->total_irq_counter,
		trace->total_intwin_counter,
		trace->total_msr_counter,
		trace->total_cpuid_counter,
		trace->total_pgflt_counter,
		trace->total_tsc_counter,
		trace->total_hcall_counter,
		trace->other_counter);
}

#ifdef TRACE_GUEST_IOPORT
static void
vmm_trace_ioport_output(struct vm_perf_trace *trace)
{
	uint32_t port;
	for (port = 0; port < MAX_IOPORT; port++)
		if (trace->ioport_counter[port]) {
			dprintf("  0x%x  %lld  %lld",
				(uint16_t) port,
				trace->ioport_time[port] / tsc_per_ms,
				trace->ioport_counter[port]);
			trace->ioport_time[port] =
				trace->ioport_counter[port] = 0;
		}
}
#endif

static void
vmm_trace_vmentry(struct vm *vm, uint64_t entry_tsc)
{
	struct vm_perf_trace *trace = &vm->trace;
	uint64_t handling_time;

	trace->entry_counter++;

	if (unlikely(trace->entry_time == 0)) {
		trace->entry_time = trace->last_output_time = entry_tsc;
		return;
	}

	handling_time = entry_tsc - trace->exit_time;

	trace->in_host_time += handling_time;
	trace->entry_time = entry_tsc;

	switch (vm->exit_reason) {
	case EXIT_FOR_EXTINT:
		trace->total_irq_time += handling_time;
		break;
	case EXIT_FOR_INTWIN:
		trace->total_intwin_time += handling_time;
		break;
	case EXIT_FOR_IOPORT:
		trace->total_ioport_time += handling_time;
#ifdef TRACE_GUEST_IOPORT
		trace->ioport_time[vm->exit_info.ioport.port] += handling_time;
#endif
		break;
	case EXIT_FOR_RDMSR:
	case EXIT_FOR_WRMSR:
		trace->total_msr_time += handling_time;
		break;
	case EXIT_FOR_PGFLT:
		trace->total_pgflt_time += handling_time;
		break;
	case EXIT_FOR_CPUID:
		trace->total_cpuid_time += handling_time;
		break;
	case EXIT_FOR_RDTSC:
		trace->total_tsc_time += handling_time;
		break;
	case EXIT_FOR_HYPERCALL:
		trace->total_hcall_time += handling_time;
		break;
	default:
		trace->other_time += handling_time;
	}

	if (entry_tsc - trace->last_output_time >=
	    VM_TRACE_OUTPUT_INTERVAL * tsc_per_ms * 1000) {
		vmm_trace_output(trace);
#ifdef TRACE_GUEST_IOPORT
		vmm_trace_ioport_output(trace);
#endif
		dprintf("\n");
		trace->last_output_time = entry_tsc;
		trace->in_guest_time = trace->in_host_time = 0;
		trace->entry_counter = trace->exit_counter = 0;
		trace->total_ioport_counter =
			trace->total_irq_counter =
			trace->total_intwin_counter =
			trace->total_msr_counter =
			trace->total_cpuid_counter =
			trace->total_pgflt_counter =
			trace->total_tsc_counter =
			trace->total_hcall_counter =
			trace->other_counter = 0;
		trace->total_ioport_time =
			trace->total_irq_time =
			trace->total_intwin_time =
			trace->total_msr_time =
			trace->total_cpuid_time =
			trace->total_pgflt_time =
			trace->total_tsc_time =
			trace->total_hcall_time =
			trace->other_time = 0;
	}
}

static void
vmm_trace_vmexit(struct vm *vm, uint64_t exit_tsc)
{
	struct vm_perf_trace *trace = &vm->trace;

	uint64_t running_time = exit_tsc - trace->entry_time;

	trace->in_guest_time += running_time;
	trace->exit_counter++;
	trace->exit_time = exit_tsc;

	switch (vm->exit_reason) {
	case EXIT_FOR_EXTINT:
		trace->total_irq_counter++;
		break;
	case EXIT_FOR_INTWIN:
		trace->total_intwin_counter++;
		break;
	case EXIT_FOR_IOPORT:
		trace->total_ioport_counter++;
#ifdef TRACE_GUEST_IOPORT
		trace->ioport_counter[vm->exit_info.ioport.port]++;
#endif
		break;
	case EXIT_FOR_RDMSR:
	case EXIT_FOR_WRMSR:
		trace->total_msr_counter++;
		break;
	case EXIT_FOR_PGFLT:
		trace->total_pgflt_counter++;
		break;
	case EXIT_FOR_CPUID:
		trace->total_cpuid_counter++;
		break;
	case EXIT_FOR_RDTSC:
		trace->total_tsc_counter++;
		break;
	case EXIT_FOR_HYPERCALL:
		trace->total_hcall_counter++;
		break;
	default:
		trace->other_counter++;
	}
}

#endif

int
vmm_init(void)
{
	extern struct vmm_ops vmm_ops_intel;
	extern struct vmm_ops vmm_ops_amd;

	int i;
	struct pcpu *c;

	c = pcpu_cur();
	KERN_ASSERT(c);

	if (c->vm_inited == TRUE)
		return 0;

	if (vmm_inited == FALSE && pcpu_onboot() == TRUE) {
		for (i = 0; i < MAX_VMID; i++) {
			memzero(&vm_pool[i], sizeof(struct vm));
			vm_pool[i].used = FALSE;
			vm_pool[i].vmid = i;
		}
		spinlock_init(&vm_pool_lock);

		if (is_intel() == TRUE) {
			vmm_ops = &vmm_ops_intel;
		} else if (is_amd() == TRUE) {
			vmm_ops = &vmm_ops_amd;
		} else {
			VIRT_DEBUG("Neither Intel nor AMD processor.\n");
			return 1;
		}

		vmm_inited = TRUE;
	}

	if (vmm_inited == FALSE)
		return 2;

	if (vmm_ops->hw_init == NULL || vmm_ops->hw_init() != 0) {
		VIRT_DEBUG("Cannot initialize the virtualization hardware.\n");
		return 3;
	}

	c->vm_inited = TRUE;

	return 0;
}

struct vm *
vmm_create_vm(uint64_t cpufreq, size_t memsize)
{
	KERN_ASSERT(vmm_inited == TRUE);

	struct vm *vm ;

	if (cpufreq >= tsc_per_ms * 1000) {
		VIRT_DEBUG("Guest CPU frequency cannot be higher than the host "
			   "CPU frequency.\n");
		return NULL;
	}

	if ((vm = vmm_alloc_vm()) == NULL) {
		VIRT_DEBUG("Cannot allocate a vm structure.\n");
		return NULL;
	}

	vm->proc = NULL;
	vm->state = VM_STATE_STOP;

	vm->cpufreq = cpufreq;
	vm->memsize = memsize;
	vm->tsc = 0;

	vm->exit_reason = EXIT_NONE;
	vm->exit_handled = TRUE;

#ifdef TRACE_VIRT
	memzero(&vm->trace, sizeof(struct vm_perf_trace));
#endif

	if (vmm_ops->vm_init == NULL || vmm_ops->vm_init(vm)) {
		VIRT_DEBUG("Machine-dependent VM initialization failed.\n");
		return NULL;
	}

	vmm_ops->intercept_all_ioports(vm, TRUE);
	vmm_ops->intercept_all_msrs(vm, 0);

	/* setup the segment registers */
	vmm_ops->set_desc(vm, GUEST_CS, &guest_seg_desc[0]);
	vmm_ops->set_desc(vm, GUEST_DS, &guest_seg_desc[1]);
	vmm_ops->set_desc(vm, GUEST_ES, &guest_seg_desc[1]);
	vmm_ops->set_desc(vm, GUEST_FS, &guest_seg_desc[1]);
	vmm_ops->set_desc(vm, GUEST_GS, &guest_seg_desc[1]);
	vmm_ops->set_desc(vm, GUEST_SS, &guest_seg_desc[1]);
	vmm_ops->set_desc(vm, GUEST_LDTR, &guest_seg_desc[2]);
	vmm_ops->set_desc(vm, GUEST_TR, &guest_seg_desc[3]);
	vmm_ops->set_desc(vm, GUEST_GDTR, &guest_seg_desc[4]);
	vmm_ops->set_desc(vm, GUEST_IDTR, &guest_seg_desc[4]);

	/* setup the general registers */
	vmm_ops->set_reg(vm, GUEST_EAX, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_EBX, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_ECX, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_EDX,
			 (pcpu_cur()->arch_info.family << 8) |
			 (pcpu_cur()->arch_info.model << 4) |
			 (pcpu_cur()->arch_info.step));
	vmm_ops->set_reg(vm, GUEST_ESI, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_EDI, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_EBP, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_ESP, 0x00000000);
	vmm_ops->set_reg(vm, GUEST_EIP, 0x0000fff0);
	vmm_ops->set_reg(vm, GUEST_EFLAGS, 0x00000002);

	/* load BIOS */
	vmm_load_bios(vm);

	/* setup the virtual device interface */
	vdev_init(vm);

	VIRT_DEBUG("VM (CPU %lld Hz, MEM %d bytes) is created.\n",
		   cpufreq, memsize);

	return vm;
}

int
vmm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vmm_inited == TRUE);
	KERN_ASSERT(vm != NULL);

	uint64_t start_tsc, exit_tsc;
	int rc, injected;

	pcpu_cur()->vm = vm;

	if (vdev_wait_all_devices_ready(vm)) {
		VIRT_DEBUG("Cannot start all virtual devices.\n");
		return 1;
	}
	vm->state = VM_STATE_RUNNING;

	VIRT_DEBUG("Start running VM ... \n");

	rc = 0;
	while (1) {
		KERN_ASSERT(vm->exit_handled == TRUE);

		injected = 0;
		start_tsc = rdtscp();

#ifdef TRACE_VIRT
		vmm_trace_vmentry(vm, start_tsc);
#endif

		if ((rc = vmm_ops->vm_run(vm)))
			break;

		exit_tsc = rdtscp();
		vmm_update_guest_tsc(vm, start_tsc, exit_tsc);

#ifdef TRACE_VIRT
		vmm_trace_vmexit(vm, exit_tsc);
#endif

		injected = vmm_intr_assist(vm);

		/*
		 * If the exit is caused by the interrupt, set the IF bit on the
		 * current processor so that the interrupt handler in CertiKOS
		 * kernel will be invoked. Therefore, we can know which
		 * interrupt is happening in the virtual machine.
		 */
		if (vm->exit_reason == EXIT_FOR_EXTINT)
			intr_local_enable();

		KERN_ASSERT(vm->exit_reason != EXIT_FOR_EXTINT ||
			    (vm->exit_handled == TRUE &&
			     (read_eflags() & FL_IF) == 0x0));

		/* handle other types of the VM exits */
		if (vmm_handle_exit(vm)) {
			VIRT_DEBUG("Cannot handle a VM exit (exit_reason %d).\n",
				   vm->exit_reason);
			rc = 1;
			break;
		}

		vm->exit_handled = TRUE;

		/* post-handling of the interrupts from the virtual machine */
		if (injected == 0)
			vmm_intr_assist(vm);
	}

	if (rc)
		VIRT_DEBUG("A virtual machine terminates abnormally.\n");
	return rc;
}

struct vm *
vmm_cur_vm(void)
{
	return pcpu_cur()->vm;
}

uint64_t
vmm_rdtsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	return vm->tsc;
}

int
vmm_get_mmap(struct vm *vm, uintptr_t gpa, uintptr_t *hpa)
{
	KERN_ASSERT(vm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa ||
	    (gpa >= vm->memsize && gpa < 0xf0000000))
		return 1;

	if (hpa == NULL)
		return 2;

	return vmm_ops->get_mmap(vm, gpa, hpa);
}

int
vmm_set_mmap(struct vm *vm, uintptr_t gpa, pageinfo_t *pi)
{
	KERN_ASSERT(vm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa ||
	    (gpa >= vm->memsize && gpa < 0xf0000000))
		return 1;

	if (pi == NULL)
		return 2;

	return vmm_ops->set_mmap(vm, gpa, mem_pi2phys(pi));
}

int
vmm_unset_mmap(struct vm *vm, uintptr_t gpa)
{
	KERN_ASSERT(vm != NULL);

	if (ROUNDDOWN(gpa, PAGESIZE) != gpa ||
	    (gpa >= vm->memsize && gpa < 0xf0000000))
		return 1;

	return vmm_ops->unset_mmap(vm, gpa);
}

int
vmm_intercept_ioport(struct vm *vm, uint16_t port, bool enable)
{
	KERN_ASSERT(vm != NULL);
	return vmm_ops->intercept_ioport(vm, port, enable);
}

int
vmm_handle_extint(struct vm *vm, uint8_t irq)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(vm->exit_reason == EXIT_FOR_EXTINT &&
		    vm->exit_handled == FALSE);

	vid_t vid = vm->vdev.irq[irq].vid;

	/*
	 * Try to notify the virtual device which occupies the interrupt. If no
	 * virtual device is registered as the source of the interrupt, raise
	 * corresponding interrupt line of the virtual PIC.
	 */
	if (vdev_sync_dev(vm, vid) && vpic_is_ready(&vm->vdev.vpic) == TRUE) {
		vdev_raise_irq(vm, vid, irq);
		vdev_lower_irq(vm, vid, irq);
	}

	vm->exit_handled = TRUE;

	return 0;
}

int
vmm_memcpy_to_guest(struct vm *vm,
		    uintptr_t dest_gpa, uintptr_t src_hpa, size_t size)
{
	KERN_ASSERT(vm != NULL);

	uintptr_t dest_hpa, dest, src;
	size_t remaining, copied;

	if (dest_gpa > vm->memsize || size > vm->memsize ||
	    vm->memsize - size < dest_gpa)
		return 1;

	vmm_ops->get_mmap(vm, dest_gpa, &dest_hpa);
	dest_hpa += dest_gpa - ROUNDDOWN(dest_gpa, PAGESIZE);
	dest = dest_gpa;
	src = src_hpa;
	remaining = size;

	copied = PAGESIZE - (dest_gpa - ROUNDDOWN(dest_gpa, PAGESIZE));
	copied = MIN(copied, remaining);

	do {
		memcpy((void *) dest_hpa, (void *) src, copied);
		remaining -= copied;
		if (remaining == 0)
			break;
		dest += copied;
		vmm_ops->get_mmap(vm, dest, &dest_hpa);
		src += copied;
		copied = MIN(PAGESIZE, remaining);
	} while (remaining);

	return 0;
}

int
vmm_memcpy_to_host(struct vm *vm,
		   uintptr_t dest_hpa, uintptr_t src_gpa, size_t size)
{
	KERN_ASSERT(vm != NULL);

	uintptr_t src_hpa, dest, src;
	size_t remaining, copied;

	if (src_gpa > vm->memsize || size > vm->memsize ||
	    vm->memsize - size < src_gpa)
		return 1;

	vmm_ops->get_mmap(vm, src_gpa, &src_hpa);
	src_hpa += src_gpa - ROUNDDOWN(src_gpa, PAGESIZE);
	src = src_gpa;
	dest = dest_hpa;
	remaining = size;

	copied = PAGESIZE - (src_gpa - ROUNDDOWN(src_gpa, PAGE_SIZE));
	copied = MIN(copied, remaining);

	do {
		memcpy((void *) dest, (void *) src_hpa, copied);
		remaining -= copied;
		if (remaining == 0)
			break;
		dest += copied;
		src += copied;
		vmm_ops->get_mmap(vm, src, &src_hpa);
		copied = MIN(PAGESIZE, remaining);
	} while (remaining);


	return 0;
}

uintptr_t
vmm_translate_gp2hp(struct vm *vm, uintptr_t gpa)
{
	KERN_ASSERT(vm != NULL);
	uintptr_t hpa;
	vmm_ops->get_mmap(vm, ROUNDDOWN(gpa, PAGESIZE), &hpa);
	return hpa + (gpa - ROUNDDOWN(gpa, PAGESIZE));
}

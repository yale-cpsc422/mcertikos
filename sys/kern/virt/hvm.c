#include <sys/debug.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/hvm.h>

#include <dev/pcpu.h>

#include "svm/svm.h"

#ifdef DEBUG_HVM

#define HVM_DEBUG(fmt, ...)				\
	do {						\
		KERN_DEBUG("VMM: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define HVM_DEBUG(fmt...)			\
	do {					\
	} while (0)

#endif

static bool		hvm_inited = FALSE;

static struct vm	vm0;

int
hvm_init(void)
{
	if (hvm_inited == TRUE)
		return 0;

	struct pcpu *c = pcpu_cur();

	if (c->arch_info.cpu_vendor != AMD) {
		HVM_DEBUG("Not support non-AMD platform.\n");
		return -1;
	}

	if (svm_init()) {
		HVM_DEBUG("Cannot intialize HVM on CPU%d.\n", pcpu_cpu_idx(c));
		return -2;
	}

	memzero(&vm0, sizeof(vm0));

	hvm_inited = TRUE;

	return 0;
}

struct vm *
hvm_create_vm(void)
{
	struct vm *vm = &vm0;

	if (vm->inuse)
		return NULL;

	vm->vmid = 0;
	vm->proc = proc_cur();
	vm->exit_reason = EXIT_REASON_NONE;

	if ((vm->cookie = svm_init_vm()) == NULL) {
		HVM_DEBUG("svm_init_vm() failed.\n");
		return NULL;
	}

	vm->inuse = 1;

	return vm;
}

int
hvm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	exit_reason_t reason;

	if ((reason = svm_run_vm(vm->cookie)) == EXIT_REASON_INVAL) {
		HVM_DEBUG("svm_run_vm() failed.\n");
		return -1;
	}

	/* restore TSS */
	kstack_switch(proc_cur()->kstack);

	vm->exit_reason = reason;

	if (reason == EXIT_REASON_IOPORT) {
		vm->exit_info.ioport.port = svm_exit_io_port(vm->cookie);
		vm->exit_info.ioport.width = svm_exit_io_width(vm->cookie);
		vm->exit_info.ioport.write = svm_exit_io_write(vm->cookie);
		vm->exit_info.ioport.rep = svm_exit_io_rep(vm->cookie);
		vm->exit_info.ioport.str = svm_exit_io_str(vm->cookie);
	} else if (reason == EXIT_REASON_PGFLT) {
		vm->exit_info.pgflt.addr = svm_exit_fault_addr(vm->cookie);
	}

	return 0;
}

int
hvm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT(vm != NULL);

	if (gpa % PAGESIZE || hpa % PAGESIZE)
		return -1;

	return svm_set_mmap(vm->cookie, gpa, hpa);
}

int
hvm_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(vm != NULL);

	return svm_set_reg(vm->cookie, reg, val);
}

uint32_t
hvm_get_reg(struct vm *vm, guest_reg_t reg)
{
	KERN_ASSERT(vm != NULL);

	return svm_get_reg(vm->cookie, reg);
}

int
hvm_set_seg(struct vm *vm, guest_seg_t seg, uint16_t sel, uint32_t base_lo,
	    uint32_t base_hi, uint32_t lim, uint32_t ar)
{
	KERN_ASSERT(vm != NULL);

	return svm_set_seg(vm->cookie, seg, sel, base_lo, base_hi, lim, ar);
}

uint32_t
hvm_get_next_eip(struct vm *vm, guest_instr_t instr)
{
	KERN_ASSERT(vm != NULL);

	return svm_get_next_eip(vm->cookie, instr);
}

int
hvm_inject_event(struct vm *vm, guest_event_t type,
		 uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(vm != NULL);

	return svm_inject_event(vm->cookie, type, vector, errcode, ev);
}

int
hvm_pending_event(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	return svm_pending_event(vm->cookie);
}

int
hvm_intr_shadow(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	return svm_intr_shadow(vm->cookie);
}

void
hvm_intercept_intr_window(struct vm *vm, bool enabled)
{
	KERN_ASSERT(vm != NULL);

	svm_intercept_vintr(vm->cookie, enabled);
}

struct vm *
hvm_get_vm(int vmid)
{
	if (!(0 <= vmid && vmid < MAX_VMID))
		return NULL;
	return &vm0;
}

bool
hvm_available(void)
{
	return hvm_inited;
}

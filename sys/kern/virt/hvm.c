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
	KERN_ASSERT(hvm_ops != NULL);

	struct vm *vm = &vm0;

	if (vm->inuse)
		return NULL;

	vm->vmid = 0;
	vm->proc = proc_cur();
	vm->exit_reason = EXIT_REASON_NONE;

	if (svm_init_vm(vm)) {
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

	int rc;

	if ((rc = svm_run_vm(vm))) {
		HVM_DEBUG("svm_run_vm() failed.\n");
		return rc;
	}

	/* restore TSS */
	kstack_switch(proc_cur()->kstack);

	return 0;
}

int
hvm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type)
{
	KERN_ASSERT(vm != NULL);

	if (gpa % PAGESIZE || hpa % PAGESIZE)
		return -1;

	return svm_set_mmap(vm, gpa, hpa, type);
}

int
hvm_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(vm != NULL);

	return svm_set_reg(vm, reg, val);
}

int
hvm_get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(val != NULL);

	return svm_get_reg(vm, reg, val);
}

int
hvm_set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(desc != NULL);

	return svm_set_desc(vm, seg, desc);
}

int
hvm_get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(desc != NULL);

	return svm_get_desc(vm, seg, desc);
}

int
hvm_get_next_eip(struct vm *vm, guest_instr_t instr, uintptr_t *neip)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(neip != NULL);

	return svm_get_next_eip(vm, instr, neip);
}

int
hvm_inject_event(struct vm *vm, guest_event_t type,
		 uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(vm != NULL);

	return svm_inject_event(vm, type, vector, errcode, ev);
}

int
hvm_pending_event(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	return svm_pending_event(vm);
}

int
hvm_intr_shadow(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);

	return svm_intr_shadow(vm);
}

int
hvm_intercept_intr_window(struct vm *vm, bool enabled)
{
	KERN_ASSERT(vm != NULL);

	return svm_intercept_vintr(vm, enabled);
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

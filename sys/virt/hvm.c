#include <sys/debug.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#include <sys/virt/hvm.h>

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

static struct vm	vm_pool[MAX_VMID];
static spinlock_t	vm_pool_lk;
volatile static bool	vm_pool_inited = FALSE;

static struct hvm_ops	*hvm_ops = NULL;
extern struct hvm_ops	hvm_ops_amd;

static struct vm *
hvm_alloc_vm(void)
{
	struct vm *vm;
	int i;

	spinlock_acquire(&vm_pool_lk);

	for (i = 0; i < MAX_VMID; i++)
		if (vm_pool[i].inuse == 0)
			break;

	if (i == MAX_VMID) {
		HVM_DEBUG("No unused VM descriptor.\n");
		spinlock_release(&vm_pool_lk);
		return NULL;
	}

	vm = &vm_pool[i];
	vm->inuse = 1;
	vm->vmid = i;

	spinlock_release(&vm_pool_lk);

	return vm;
}

static void
hvm_free_vm(struct vm *vm)
{
	if (vm == NULL)
		return;

	spinlock_acquire(&vm_pool_lk);
	vm->inuse = 0;
	spinlock_release(&vm_pool_lk);
}

int
hvm_init(void)
{
	if (vm_pool_inited == FALSE) {
		memzero(vm_pool, sizeof(struct vm) * MAX_VMID);
		spinlock_init(&vm_pool_lk);
		vm_pool_inited = TRUE;
	}

	struct pcpu *c = pcpu_cur();

	if (hvm_ops == NULL)
		hvm_ops =
			(c->arch_info.cpu_vendor == AMD) ? &hvm_ops_amd : NULL;

	if (hvm_ops == NULL) {
		HVM_DEBUG("Cannot detect HVM support.\n");
		return -1;
	}

	if (c->hvm_inited == FALSE && hvm_ops->hw_init()) {
		HVM_DEBUG("Cannot intialize HVM on CPU%d.\n", pcpu_cpu_idx(c));
		return -2;
	}

	c->hvm_inited = TRUE;

	return 0;
}

struct vm *
hvm_create_vm(void)
{
	KERN_ASSERT(hvm_ops != NULL);

	struct vm *vm = hvm_alloc_vm();

	if (vm == NULL)
		return NULL;

	vm->proc = proc_cur();
	vm->exit_reason = EXIT_REASON_NONE;

	if (hvm_ops->vm_init(vm)) {
		HVM_DEBUG("hvm_ops->vm_init() failed.\n");
		hvm_free_vm(vm);
		return NULL;
	}

	return vm;
}

int
hvm_run_vm(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	int rc;

	if ((rc = hvm_ops->vm_run(vm))) {
		HVM_DEBUG("hvm_ops->vm_run() failed.\n");
		return rc;
	}

	/* restore TSS */
	kstack_switch(proc_cur()->kstack);

	if (vm->exit_reason == EXIT_REASON_EXTINT) {
		KERN_ASSERT(pcpu_cur()->guest_irq == NULL);
		pcpu_cur()->guest_irq = &vm->guest_irq;
	}

	return 0;
}

int
hvm_set_mmap(struct vm *vm, uintptr_t gpa, uintptr_t hpa, int type)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	if (gpa % PAGESIZE || hpa % PAGESIZE)
		return -1;

	return hvm_ops->set_mmap(vm, gpa, hpa, type);
}

int
hvm_set_reg(struct vm *vm, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->set_reg(vm, reg, val);
}

int
hvm_get_reg(struct vm *vm, guest_reg_t reg, uint32_t *val)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);
	KERN_ASSERT(val != NULL);

	return hvm_ops->get_reg(vm, reg, val);
}

int
hvm_set_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);
	KERN_ASSERT(desc != NULL);

	return hvm_ops->set_desc(vm, seg, desc);
}

int
hvm_get_desc(struct vm *vm, guest_seg_t seg, struct guest_seg_desc *desc)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);
	KERN_ASSERT(desc != NULL);

	return hvm_ops->get_desc(vm, seg, desc);
}

int
hvm_get_next_eip(struct vm *vm, guest_instr_t instr, uintptr_t *neip)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);
	KERN_ASSERT(neip != NULL);

	return hvm_ops->get_next_eip(vm, instr, neip);
}

int
hvm_inject_event(struct vm *vm, guest_event_t type,
		 uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->inject_event(vm, type, vector, errcode, ev);
}

int
hvm_pending_event(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->pending_event(vm);
}

int
hvm_intr_shadow(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->intr_shadow(vm);
}

int
hvm_intercept_ioport(struct vm *vm, uint16_t port, bool enabled)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->intercept_ioport(vm, port, enabled);
}

int
hvm_intercept_msr(struct vm *vm, uint32_t msr, bool enabled)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->intercept_msr(vm, msr, enabled);
}

int
hvm_intercept_intr_window(struct vm *vm, bool enabled)
{
	KERN_ASSERT(vm != NULL);
	KERN_ASSERT(hvm_ops != NULL);

	return hvm_ops->intercept_intr_window(vm, enabled);
}

struct vm *
hvm_get_vm(int vmid)
{
	if (!(0 <= vmid && vmid < MAX_VMID))
		return NULL;
	return &vm_pool[vmid];
}

bool
hvm_available(void)
{
	return (pcpu_cur()->hvm_inited == TRUE && vm_pool_inited == TRUE &&
		hvm_ops != NULL) ? TRUE : FALSE;
}

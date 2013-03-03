#include <sys/types.h>
#include <sys/virt/vmm.h>

int
ccomp_vmm_set_msr(struct vmm_ops *ops, struct vm *vm, uint32_t msr, uint64_t *v)
{
	return ops->set_msr(vm, msr, *v);
}

uint64_t
vmm_rdtsc(struct vm *vm)
{
	KERN_ASSERT(vm != NULL);
	return vm->tsc;
}

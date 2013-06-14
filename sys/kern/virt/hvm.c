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

static struct vm *
hvm_get_vm(int vmid)
{
	if (!(0 <= vmid && vmid < MAX_VMID))
		return NULL;
	return &vm0;
}

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
		HVM_DEBUG("Cannot intialize HVM.\n");
		return -2;
	}

	memzero(&vm0, sizeof(vm0));

	hvm_inited = TRUE;

	return 0;
}

int
hvm_create_vm(void)
{
	struct vm *vm = &vm0;

	if (vm->inuse)
		return -1;

	vm->exit_reason = EXIT_REASON_NONE;

	if ((vm->cookie = svm_init_vm()) == NULL) {
		HVM_DEBUG("svm_init_vm() failed.\n");
		return -1;
	}

	vm->inuse = 1;

	return vm->vmid;
}

int
hvm_run_vm(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));

	struct vm *vm = hvm_get_vm(vmid);
	exit_reason_t reason;

	if ((reason = svm_run_vm(vm->cookie)) == EXIT_REASON_INVAL) {
		HVM_DEBUG("svm_run_vm() failed.\n");
		return -1;
	}

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
hvm_set_mmap(int vmid, uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT(hvm_valid_vm(vmid));

	struct vm *vm = hvm_get_vm(vmid);

	if (gpa % PAGESIZE || hpa % PAGESIZE)
		return -1;

	return svm_set_mmap(vm->cookie, gpa, hpa);
}

int
hvm_set_reg(int vmid, guest_reg_t reg, uint32_t val)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_set_reg(vm->cookie, reg, val);
}

uint32_t
hvm_get_reg(int vmid, guest_reg_t reg)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_get_reg(vm->cookie, reg);
}

int
hvm_set_seg(int vmid, guest_seg_t seg,
	    uint16_t sel, uint32_t base, uint32_t lim, uint32_t ar)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_set_seg(vm->cookie, seg, sel, base, lim, ar);
}

uint32_t
hvm_get_next_eip(int vmid, guest_instr_t instr)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_get_next_eip(vm->cookie, instr);
}

int
hvm_inject_event(int vmid, guest_event_t type,
		 uint8_t vector, uint32_t errcode, bool ev)
{
	KERN_ASSERT(hvm_get_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_inject_event(vm->cookie, type, vector, errcode, ev);
}

int
hvm_pending_event(int vmid)
{
	KERN_ASSERT(hvm_get_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_pending_event(vm->cookie);
}

int
hvm_intr_shadow(int vmid)
{
	KERN_ASSERT(hvm_get_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return svm_intr_shadow(vm->cookie);
}

void
hvm_intercept_intr_window(int vmid, bool enabled)
{
	KERN_ASSERT(hvm_get_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	svm_intercept_vintr(vm->cookie, enabled);
}

bool
hvm_available(void)
{
	return hvm_inited;
}

bool
hvm_valid_vm(int vmid)
{
	return hvm_available() && (hvm_get_vm(vmid) != NULL);
}

exit_reason_t
hvm_exit_reason(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_reason;
}

uint16_t
hvm_exit_io_port(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_info.ioport.port;
}

data_sz_t
hvm_exit_io_width(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_info.ioport.width;
}

bool
hvm_exit_io_write(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_info.ioport.write;
}

bool
hvm_exit_io_rep(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_info.ioport.rep;
}

bool
hvm_exit_io_str(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_info.ioport.str;
}

uintptr_t
hvm_exit_fault_addr(int vmid)
{
	KERN_ASSERT(hvm_valid_vm(vmid));
	struct vm *vm = hvm_get_vm(vmid);
	return vm->exit_info.pgflt.addr;
}

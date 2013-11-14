#include "vmcb_op.h"

#define SVM_EXITINFO1_PORT_SHIFT	16
#define SVM_EXITINFO1_PORT_MASK		(0xffffUL << SVM_EXITINFO1_PORT_SHIFT)
#define SVM_EXITINFO1_SZ32		(1UL << 6)
#define SVM_EXITINFO1_SZ16		(1UL << 5)
#define SVM_EXITINFO1_SZ8		(1UL << 4)
#define SVM_EXITINFO1_REP		(1UL << 3)
#define SVM_EXITINFO1_STR		(1UL << 2)
#define SVM_EXITINFO1_TYPE_MASK		0x1
#define SVM_EXITINFO1_TYPE_OUT		0
#define SVM_EXITINFO1_TYPE_IN		1

#define EXIT_INTINFO_VALID_SHIFT	31
#define EXIT_INTINFO_TYPE_SHIFT		8
#define EXIT_INTINFO_TYPE_MASK		(0x7UL << EXIT_INTINFO_TYPE_SHIFT)
#define EXIT_INTINFO_TYPE_INTR		(0 << EXIT_INTINFO_TYPE_SHIFT)
#define EXIT_INTINFO_TYPE_NMI		(2 << EXIT_INTINFO_TYPE_SHIFT)
#define EXIT_INTINFO_VECTOR_MASK	255

#define VMEXIT_VINTR			100

enum {
	GUEST_EAX, GUEST_EBX, GUEST_ECX, GUEST_EDX, GUEST_ESI, GUEST_EDI,
	GUEST_EBP, GUEST_ESP, GUEST_EIP, GUEST_EFLAGS,
	GUEST_CR0, GUEST_CR2, GUEST_CR3, GUEST_CR4,
	GUEST_MAX_REG
};

void
svm_set_intercept_intwin(unsigned int enable)
{
	if(enable != 0)
		vmcb_inject_virq();
	vmcb_set_intercept_vint(enable);
}

void
svm_set_reg(unsigned int reg, unsigned int val)
{
	if (reg == GUEST_EAX || reg == GUEST_ESP || reg == GUEST_EIP ||
	    reg == GUEST_EFLAGS || reg == GUEST_CR0 || reg == GUEST_CR2 ||
	    reg == GUEST_CR3 || reg == GUEST_CR4)
		vmcb_set_reg(reg, val);
	else
		xvmst_write(reg - 1, val);
}

unsigned int
svm_get_reg(unsigned int reg)
{
	if (reg == GUEST_EAX || reg == GUEST_ESP || reg == GUEST_EIP ||
	    reg == GUEST_EFLAGS || reg == GUEST_CR0 || reg == GUEST_CR2 ||
	    reg == GUEST_CR3 || reg == GUEST_CR4)
		return vmcb_get_reg(reg);
	else
		return xvmst_read(reg - 1);
}

unsigned int
svm_get_exit_reason(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(0);
	return val;
}

unsigned int
svm_get_exit_io_port(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(1);
	val = val >> SVM_EXITINFO1_PORT_SHIFT;
	return val;
}

unsigned int
svm_get_exit_io_width(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(1);

	if (0 < (val & SVM_EXITINFO1_SZ8))
		return 0;
	else if (0 < (val & SVM_EXITINFO1_SZ16))
		return 1;
	else
		return 2;
}

unsigned int
svm_get_exit_io_write(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(1);
	if (0 < (val & SVM_EXITINFO1_TYPE_IN))
		return 0;
	else
		return 1;
}

unsigned int
svm_get_exit_io_rep(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(1);
	if(0 < (val & SVM_EXITINFO1_REP))
		return 1;
	else
		return 0;
}

unsigned int
svm_get_exit_io_str(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(1);
	if(0 < (val & SVM_EXITINFO1_STR))
		return 1;
	else
		return 0;
}

unsigned int
svm_get_exit_io_neip(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(2);
	return val;
}

unsigned int
svm_get_exit_fault_addr(void)
{
	unsigned int val;
	val = vmcb_get_exit_info(2);
	return val;
}

void
svm_sync(void)
{
	unsigned int val;
	unsigned int valid;
	unsigned int type;
	unsigned int vector;

	val = vmcb_get_exit_info(3);
	valid = val & (1UL << EXIT_INTINFO_VALID_SHIFT);

	if (valid) {
		type = val & EXIT_INTINFO_TYPE_MASK;
		vector = val & EXIT_INTINFO_VECTOR_MASK;

		if (type == EXIT_INTINFO_TYPE_INTR)
			vmcb_inject_event(type >> EXIT_INTINFO_TYPE_SHIFT,
					  vector, 0, 0);
	}

	val = svm_get_exit_reason();
	if (val == VMEXIT_VINTR)
		vmcb_clear_virq();
}

void
svm_run_vm(void)
{
	switch_to_guest();
}

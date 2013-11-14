#include "svm_intro.h"

#define VMCB_Z_INTCEPT_LO_OFFSET	3
#define VMCB_Z_INT_CTL_OFFSET		24
#define VMCB_Z_VINT_VECTOR_OFFSET	25
#define VMCB_Z_INT_STATE_OFFSET		26
#define VMCB_Z_EXITCODE_LO_OFFSET	28
#define VMCB_Z_EXITINFO1_LO_OFFSET	30
#define VMCB_Z_EXITINFO2_LO_OFFSET	32
#define VMCB_Z_EXITINTINFO_LO_OFFSET	34
#define VMCB_Z_EXITINTINFO_HI_OFFSET	35

#define VMCB_Z_EVENTINJ_LO_OFFSET	42
#define VMCB_Z_EVENTINJ_HI_OFFSET	43
#define VMCB_Z_NEXT_RIP_LO_OFFSET	50
#define VMCB_Z_ES_OFFSET		256
#define VMCB_Z_CS_OFFSET		260
#define VMCB_Z_SS_OFFSET		264
#define VMCB_Z_DS_OFFSET		268
#define VMCB_Z_FS_OFFSET		272
#define VMCB_Z_GS_OFFSET		276
#define VMCB_Z_GDTR_OFFSET		280
#define VMCB_Z_LDTR_OFFSET		284
#define VMCB_Z_IDTR_OFFSET		288
#define VMCB_Z_TR_OFFSET		292

#define VMCB_V_CR4_LO_OFFSET		338
#define VMCB_V_CR3_LO_OFFSET		340
#define VMCB_V_CR0_LO_OFFSET		342
#define VMCB_V_RFLAGS_LO_OFFSET		348
#define VMCB_V_RIP_LO_OFFSET		350
#define VMCB_V_RSP_LO_OFFSET		374
#define VMCB_V_RAX_LO_OFFSET		382
#define VMCB_V_CR2_LO_OFFSET		400

#define VMCB_INTCEPT_VINT_SHIFT		4

#define VMCB_INT_CTL_VIRQ_SHIFT		8
#define VMCB_INT_CTL_IGN_TPR_SHIFT	20

#define VMCB_EVENTINJ_VALID_SHIFT	31
#define VMCB_EVENTINJ_EV_SHIFT		11
#define VMCB_EVENTINJ_TYPE_SHIFT	8
#define VMCB_EVENTINJ_TYPE_INTR		0
#define VMCB_EVENTINJ_TYPE_NMI		2
#define VMCB_EVENTINJ_TYPE_EXCPT	3

#define VMCB_SEG_ATTR_SHIFT		2
#define VMCB_SEG_LIM_SHIFT		4
#define VMCB_SEG_BASE_SHIFT		8

#define EXIT_INTINFO_VALID_SHIFT	31
#define EXIT_INTINFO_TYPE_SHIFT		8
#define EXIT_INTINFO_TYPE_MASK		(0x7UL << EXIT_INTINFO_TYPE_SHIFT)
#define EXIT_INTINFO_TYPE_INTR		0
#define EXIT_INTINFO_VECTOR_MASK	255

enum {
	GUEST_EAX, GUEST_EBX, GUEST_ECX, GUEST_EDX, GUEST_ESI, GUEST_EDI,
	GUEST_EBP, GUEST_ESP, GUEST_EIP, GUEST_EFLAGS,
	GUEST_CR0, GUEST_CR2, GUEST_CR3, GUEST_CR4,
	GUEST_MAX_REG
};

void
vmcb_set_intercept_vint(unsigned int enable)
{
	unsigned int rval;
	rval = vmcb_read_z(VMCB_Z_INTCEPT_LO_OFFSET);
	if (enable)
		vmcb_write_z(VMCB_Z_INTCEPT_LO_OFFSET,
			     rval | (1UL << VMCB_INTCEPT_VINT_SHIFT));
	else
		vmcb_write_z(VMCB_Z_INTCEPT_LO_OFFSET,
			     rval & (~ (1UL << VMCB_INTCEPT_VINT_SHIFT)));
}

void
vmcb_clear_virq(void)
{
	unsigned int rval;
	rval = vmcb_read_z(VMCB_Z_INT_CTL_OFFSET);
	vmcb_write_z(VMCB_Z_INT_CTL_OFFSET,
		     rval & (~ (1UL << VMCB_INT_CTL_VIRQ_SHIFT)));
}

void
vmcb_inject_virq(void)
{
	unsigned int rval;
	rval = vmcb_read_z(VMCB_Z_INT_CTL_OFFSET);
	vmcb_write_z(VMCB_Z_INT_CTL_OFFSET,
		     rval | (1UL << VMCB_INT_CTL_VIRQ_SHIFT) |
		     (1UL << VMCB_INT_CTL_IGN_TPR_SHIFT));
	vmcb_write_z(VMCB_Z_VINT_VECTOR_OFFSET, 0);
}

unsigned int
vmcb_get_exit_info(unsigned int idx)
{
	unsigned int rval;
	if (idx == 0)
		rval = vmcb_read_z(VMCB_Z_EXITCODE_LO_OFFSET);
	else if (idx == 1)
		rval = vmcb_read_z(VMCB_Z_EXITINFO1_LO_OFFSET);
	else if (idx == 2)
		rval = vmcb_read_z(VMCB_Z_EXITINFO2_LO_OFFSET);
	else if (idx == 3)
		rval = vmcb_read_z(VMCB_Z_EXITINTINFO_LO_OFFSET);
	else
		rval = vmcb_read_z(VMCB_Z_EXITINTINFO_HI_OFFSET);
	return rval;
}

unsigned int
vmcb_check_int_shadow(void)
{
	unsigned int rval;
	rval = vmcb_read_z(VMCB_Z_INT_STATE_OFFSET);
	if ((rval & 1) == 1)
		return 1;
	else
		return 0;
}

unsigned int
vmcb_check_pending_event(void)
{
	unsigned int rval;
	rval = vmcb_read_z(VMCB_Z_EVENTINJ_LO_OFFSET);
	if ((rval & (1UL << VMCB_EVENTINJ_VALID_SHIFT)))
		return 1;
	else
		return 0;
}

void
vmcb_inject_event(unsigned int type, unsigned int vector,
		  unsigned int errcode, unsigned int ev)
{
	if (type == VMCB_EVENTINJ_TYPE_INTR) {
		vmcb_write_z(VMCB_Z_EVENTINJ_LO_OFFSET,
			     (1UL << VMCB_EVENTINJ_VALID_SHIFT) | vector |
			     (type << VMCB_EVENTINJ_TYPE_SHIFT));
	} else if (type == VMCB_EVENTINJ_TYPE_EXCPT) {
		if (ev == 1) {
			vmcb_write_z(VMCB_Z_EVENTINJ_LO_OFFSET,
				     (1UL << VMCB_EVENTINJ_VALID_SHIFT) | vector |
				     (type << VMCB_EVENTINJ_TYPE_SHIFT) |
				     (1UL << VMCB_EVENTINJ_EV_SHIFT));
			vmcb_write_z(VMCB_Z_EVENTINJ_HI_OFFSET, errcode);
		} else {
			vmcb_write_z(VMCB_Z_EVENTINJ_LO_OFFSET,
				     (1UL << VMCB_EVENTINJ_VALID_SHIFT) | vector |
				     (type << VMCB_EVENTINJ_TYPE_SHIFT));
		}
	}
}

unsigned int
vmcb_get_next_eip(void)
{
	unsigned int rval;
	rval = vmcb_read_z(VMCB_Z_NEXT_RIP_LO_OFFSET);
	return rval;
}

void
vmcb_set_seg(unsigned int seg, unsigned int sel,
	     unsigned int base, unsigned int lim, unsigned int ar)
{
	unsigned int offset;
	if (seg == 0)
		offset = VMCB_Z_CS_OFFSET;
	else if (seg == 1)
		offset = VMCB_Z_DS_OFFSET;
	else if (seg == 2)
		offset = VMCB_Z_ES_OFFSET;
	else if (seg == 3)
		offset = VMCB_Z_FS_OFFSET;
	else if (seg == 4)
		offset = VMCB_Z_GS_OFFSET;
	else if (seg == 5)
		offset = VMCB_Z_SS_OFFSET;
	else if (seg == 6)
		offset = VMCB_Z_LDTR_OFFSET;
	else if (seg == 7)
		offset = VMCB_Z_TR_OFFSET;
	else if (seg == 8)
		offset = VMCB_Z_GDTR_OFFSET;
	else
		offset = VMCB_Z_IDTR_OFFSET;
	vmcb_write_z(offset, sel | (ar << 16));
	vmcb_write_z(offset + 1, lim);
	vmcb_write_z(offset + 2, base);
	vmcb_write_z(offset + 3, 0);
}

void
vmcb_set_reg(unsigned int reg, unsigned int v)
{
	unsigned int offset;

	if (reg == GUEST_EAX)
		offset = VMCB_V_RAX_LO_OFFSET;
	else if (reg == GUEST_ESP)
		offset = VMCB_V_RSP_LO_OFFSET;
	else if (reg == GUEST_EIP)
		offset = VMCB_V_RIP_LO_OFFSET;
	else if (reg == GUEST_EFLAGS)
		offset = VMCB_V_RFLAGS_LO_OFFSET;
	else if (reg == GUEST_CR0)
		offset = VMCB_V_CR0_LO_OFFSET;
	else if (reg == GUEST_CR2)
		offset = VMCB_V_CR2_LO_OFFSET;
	else if (reg == GUEST_CR3)
		offset = VMCB_V_CR3_LO_OFFSET;
	else
		offset = VMCB_V_CR4_LO_OFFSET;

	vmcb_write_v(offset, v);
}

unsigned int
vmcb_get_reg(unsigned int reg)
{
	unsigned int offset;

	if (reg == GUEST_EAX)
		offset = VMCB_V_RAX_LO_OFFSET;
	else if (reg == GUEST_ESP)
		offset = VMCB_V_RSP_LO_OFFSET;
	else if (reg == GUEST_EIP)
		offset = VMCB_V_RIP_LO_OFFSET;
	else if (reg == GUEST_EFLAGS)
		offset = VMCB_V_RFLAGS_LO_OFFSET;
	else if (reg == GUEST_CR0)
		offset = VMCB_V_CR0_LO_OFFSET;
	else if (reg == GUEST_CR2)
		offset = VMCB_V_CR2_LO_OFFSET;
	else if (reg == GUEST_CR3)
		offset = VMCB_V_CR3_LO_OFFSET;
	else
		offset = VMCB_V_CR4_LO_OFFSET;

	return vmcb_read_v(offset);
}

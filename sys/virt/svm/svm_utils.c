#include <sys/debug.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/pmap.h>

#include "svm.h"
#include "svm_utils.h"

static uintptr_t
la2pa(uintptr_t cr3, uintptr_t la)
{
	return pmap_la2pa((pmap_t *) cr3, la);
}

/*
 * (Sec 3.4, Intel Arch Dev Man Vol3)
 */
uintptr_t
glogical_2_glinear(struct vmcb *vmcb, uint16_t seg_sel, uint32_t offset)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_save_area *save = &vmcb->save;

	if (seg_sel == 0)
		return offset;

	/* Step 1: get the segment descriptor in GDT/LDT */

	uint16_t seg_idx = seg_sel >> 3;
	bool is_gdt = (seg_sel & 0x4) ? FALSE : TRUE;

	struct vmcb_seg *seg = (is_gdt == TRUE) ? &save->gdtr : &save->ldtr;
	uint64_t seg_base = seg->base;

	uint64_t seg_desc = *(uint64_t *)(uintptr_t) (seg_base + seg_idx * 8);

	/* Step 2: check the validation of the segment descriptor */
	uint32_t base = (uint32_t)
		((seg_desc >> 32) & 0xff000000) |
		((seg_desc >> 16) & 0x00ff0000) |
		((seg_desc >> 16) & 0x0000ffff);
	uint32_t limit = (uint32_t)
		((seg_desc >> 32) & 0x000f0000) |
		((seg_desc) & 0x0000ffff);

	if (offset > limit)
		KERN_PANIC("Offset %x is out of limitation %x.\n",
			   offset, limit);

	/* Step 3: add base address  to the offset of the logical address */
	return base+offset;
}

uintptr_t
glinear_2_gphysical(struct vmcb *vmcb, uintptr_t la)
{
	KERN_ASSERT(vmcb != NULL);

	return la2pa((uintptr_t) vmcb->control.nested_cr3, la);
}

uint8_t *
get_guest_instruction(struct vmcb *vmcb)
{
	KERN_ASSERT(vmcb != NULL);

	struct vmcb_save_area *save = &vmcb->save;
	uint16_t cs_sel = save->cs.selector;
	uint64_t rip = save->rip;

	uintptr_t _instr = glogical_2_glinear(vmcb, cs_sel, rip);
	uint8_t *instr = (uint8_t *) glinear_2_gphysical(vmcb, _instr);

	return instr;
}

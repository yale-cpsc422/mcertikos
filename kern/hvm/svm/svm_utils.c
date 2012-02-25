#include <kern/as/as.h>
#include <kern/debug/debug.h>
#include <kern/pmap/pmap.h>

#include <architecture/mmu.h>
#include <architecture/types.h>
#include <architecture/x86.h>

#include <kern/hvm/svm/svm.h>
#include <kern/hvm/svm/svm_utils.h>

static uintptr_t
la2pa(uintptr_t cr3, uintptr_t la)
{
	return PGADDR(pmap_lookup((pmap_t *) cr3, la)) + PGOFF(la);
}

/*
 * (Sec 3.4, Intel Arch Dev Man Vol3)
 */
uintptr_t
glogical_2_glinear(struct vmcb *vmcb, uint16_t seg_sel, uint32_t offset)
{
	assert(vmcb != NULL);

	struct vmcb_save_area *save = &vmcb->save;

	if (seg_sel == 0)
		return offset;

	/* Step 1: get the segment descriptor in GDT/LDT */

	uint16_t seg_idx = seg_sel >> 3;
	bool is_gdt = (seg_sel & 0x4) ? false : true;

	struct vmcb_seg *seg = (is_gdt == true) ? &save->gdtr : &save->ldtr;
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
		panic("Offset %x is out of limitation %x.\n",
		      offset, limit);

	/* Step 3: add base address  to the offset of the logical address */
	return base+offset;
}

uintptr_t
glinear_2_gphysical(struct vmcb *vmcb, uintptr_t la)
{
	assert(vmcb != NULL);

	return la2pa((uintptr_t) vmcb->control.nested_cr3, la);
}

uint8_t *
get_guest_instruction(struct vmcb *vmcb)
{
	assert(vmcb != NULL);

	struct vmcb_save_area *save = &vmcb->save;
	uint16_t cs_sel = save->cs.selector;
	uint64_t rip = save->rip;

	uintptr_t _instr = glogical_2_glinear(vmcb, cs_sel, rip);
	uint8_t *instr = (uint8_t *) glinear_2_gphysical(vmcb, _instr);

	return instr;
}

void
load_bios(uintptr_t ncr3)
{
	/* load BIOS */
	extern uint8_t _binary_misc_bios_bin_start[],
		_binary_misc_bios_bin_size[];

	assert((size_t) _binary_misc_bios_bin_size % 0x10000 == 0);

	uintptr_t bios_addr = 0x100000 - (size_t) _binary_misc_bios_bin_size;

	as_copy((as_t *) ncr3, bios_addr,
		kern_as, (uintptr_t) _binary_misc_bios_bin_start,
		(size_t) _binary_misc_bios_bin_size);

#if 1
	/* load VGA BIOS */
	extern uint8_t _binary_misc_vgabios_bin_start[],
		_binary_misc_vgabios_bin_size[];

	/* assert((size_t) _binary_misc_vgabios_bin_size <= 0x8000); */

	as_copy((as_t *) ncr3, 0xc0000,
		kern_as, (uintptr_t) _binary_misc_vgabios_bin_start,
		(size_t) _binary_misc_vgabios_bin_size);
#endif
}

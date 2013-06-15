#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/string.h>
#include <sys/types.h>

#include "npt.h"
#include "vmcb.h"

static uint32_t npt_lv1[NPDENTRIES] gcc_aligned(PAGESIZE);
static uint32_t npt_lv2[NPDENTRIES][NPTENTRIES] gcc_aligned(PAGESIZE);
static int npt_inuse = 0;

npt_t
npt_new(void)
{
	int i;

	if (npt_inuse == 1)
		return NULL;

	for (i = 0; i < NPDENTRIES; i++)
		npt_lv1[i] = PGADDR((uintptr_t) npt_lv2[i]) |
			PTE_P | PTE_A | PTE_W | PTE_U;

	memzero(npt_lv2, PAGESIZE * NPDENTRIES * NPTENTRIES);

	npt_inuse = 1;

	return npt_lv1;
}

void
npt_free(npt_t npt)
{
	if (npt != npt_lv1)
		return;
	npt_inuse = 0;
}

int
npt_insert(npt_t npt, uintptr_t gpa, uintptr_t hpa)
{
	KERN_ASSERT(npt != NULL);
	KERN_ASSERT(gpa % PAGESIZE == 0);
	KERN_ASSERT(hpa % PAGESIZE == 0);

	uint32_t *lv2 = (uint32_t *) PGADDR(npt[PDX(gpa)]);
	uint32_t *lv2_entry = &lv2[PTX(gpa)];

	if (*lv2_entry & PTE_P) {
		KERN_DEBUG("GPA 0x%08x is already mapped to HPA 0x%08x.\n",
			   gpa, PGADDR(*lv2_entry));
		return -1;
	}

	*lv2_entry = PGADDR(hpa) | PTE_P | PTE_W | PTE_U | PTE_G;

	return 0;
}

void
npt_install(struct vmcb *vmcb, npt_t npt)
{
	vmcb_set_ncr3(vmcb, (uintptr_t) npt);
}

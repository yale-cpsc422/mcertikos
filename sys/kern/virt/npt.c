#include <lib/debug.h>
#include <lib/export.h>

#include "npt.h"
#include "vmcb.h"

static uint32_t npt_lv1[1024] gcc_aligned(4096);
static uint32_t npt_lv2[1024][1024] gcc_aligned(4096);
static int npt_inuse = 0;

#define PTXSHIFT	12
#define PDXSHIFT	22

#define PDX(la)		((((uintptr_t) (la)) >> PDXSHIFT) & 0x3FF)
#define PTX(la)		((((uintptr_t) (la)) >> PTXSHIFT) & 0x3FF)

#define PGADDR(la)	((uintptr_t) (la) & ~0xFFF)

#define PTE_P		0x001	/* Present */
#define PTE_W		0x002	/* Writeable */
#define PTE_U		0x004	/* User-accessible */
#define PTE_A		0x020	/* Accessed */
#define PTE_G		0x100	/* Global */

npt_t
npt_new(void)
{
	int i;

	if (npt_inuse == 1)
		return NULL;

	for (i = 0; i < 1024; i++)
		npt_lv1[i] = PGADDR((uintptr_t) npt_lv2[i]) |
			PTE_P | PTE_A | PTE_W | PTE_U;

	memzero(npt_lv2, sizeof(uint32_t) * 1024 * 1024);

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
	KERN_ASSERT(gpa % 4096 == 0);
	KERN_ASSERT(hpa % 4096 == 0);

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

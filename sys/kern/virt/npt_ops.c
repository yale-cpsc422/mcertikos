#include <lib/types.h>

#include "npt_intro.h"

void
npt_insert(uint32_t gpa, uint32_t hpa)
{
	if (gpa % 4096)
		return;

	if (hpa % 4096)
		return;

	int pdx = PDX(gpa);
	int ptx = PTX(gpa);

	npt_set_pte(pdx, ptx, hpa);
}

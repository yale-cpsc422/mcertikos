#include <lib/export.h>

#include "npt_intro.h"

uint32_t npt_lv1[1024] gcc_aligned(4096);
uint32_t npt_lv2[1024][1024] gcc_aligned(4096);

void
npt_set_pde(int pdx)
{
	if (!(0 <= pdx && pdx < 1024))
		return;

	npt_lv1[pdx] = ((uint32_t) npt_lv2[pdx]) | PTE_P | PTE_W | PTE_U;
}

void
npt_set_pte(int pdx, int ptx, uint32_t hpa)
{
	if (!(0 <= pdx && pdx < 1024))
		return;

	if (!(0 <= ptx && ptx < 1024))
		return;

	if (hpa % 4096)
		return;

	npt_lv2[pdx][ptx] = hpa | PTE_P | PTE_W | PTE_U | PTE_G;
}

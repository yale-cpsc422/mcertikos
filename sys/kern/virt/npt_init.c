#include "npt_intro.h"
#include "npt_ops.h"

void
npt_init(void)
{
	int pdx;

	memzero(npt_lv2, sizeof(uint32_t) * 1024 * 1024);

	for (pdx = 0; pdx < 1024; pdx++)
		npt_set_pde(pdx);
}

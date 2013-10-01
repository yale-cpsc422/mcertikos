#include <lib/export.h>

#include "kctx.h"
#include "kstack.h"

static bool		kstack_inited = FALSE;
static struct kstack	all_kstacks[MAX_KSTACK];
static uint8_t		all_stacks[MAX_KSTACK][KSTACK_SIZE] gcc_aligned(KSTACK_SIZE);

void
kstack_init(void)
{
	if (kstack_inited == TRUE)
		return;

	kctx_init();

	memzero(all_kstacks, sizeof(struct kstack) * MAX_KSTACK);
	kstack_inited = TRUE;
}

struct kstack *
kstack_new(void)
{
	struct kstack *ks;
	int i;

	for (i = 0; i < MAX_KSTACK; i++)
		if (all_kstacks[i].inuse == FALSE)
			break;

	if (i == MAX_KSTACK)
		return NULL;

	ks = &all_kstacks[i];
	memzero(all_stacks[i], KSTACK_SIZE);

	ks->tss.ts_esp0 = (uint32_t) all_stacks[i] + KSTACK_SIZE;
	ks->tss.ts_ss0 = CPU_GDT_KDATA;

	ks->inuse = TRUE;

	return ks;
}

int
kstack_free(struct kstack *ks)
{
	if (ks == NULL)
		return -1;
	ks->inuse = FALSE;
	return 0;
}

void
kstack_switch(struct kstack *ks)
{
	tss_switch(&ks->tss);
}

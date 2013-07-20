#include <lib/export.h>

#include "kctx.h"

static bool		kctx_inited = FALSE;
static struct kctx	all_kctx[MAX_KCTX];

void
kctx_init(void)
{
	if (kctx_inited == TRUE)
		return;
	memzero(all_kctx, sizeof(struct kctx) * MAX_KCTX);
	kctx_inited = TRUE;
}

struct kctx *
kctx_new(void (*f)(void), uintptr_t stack_top)
{
	struct kctx *kctx;
	int i;

	for (i = 0; i < MAX_KCTX; i++)
		if (all_kctx[i].inuse == FALSE)
			break;

	if (i == MAX_KCTX)
		return NULL;

	kctx = &all_kctx[i];

	kctx->esp = (uint32_t) stack_top - sizeof(uint32_t);
	*(uint32_t *) kctx->esp = 0;
	kctx->eip = (uintptr_t) f;
	kctx->inuse = TRUE;

	return kctx;
}

int
kctx_free(struct kctx *kctx)
{
	if (kctx == NULL)
		return -1;

	kctx->inuse = FALSE;

	return 0;
}

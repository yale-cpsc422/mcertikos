#include <lib/export.h>

struct host_ctx {
	uint32_t fs, gs, ldt;
	uint32_t ebx, ecx, edx, esi, edi, ebp;
};

static struct host_ctx h_ctx0;

struct vmcb;

void enter_guest(struct vmcb *vmcb,
		 uint32_t *g_ebx, uint32_t *g_ecx, uint32_t *g_edx,
		 uint32_t *g_esi, uint32_t *g_edi, uint32_t *g_ebp,
		 uint32_t *h_fs, uint32_t *h_gs, uint32_t *h_ldt,
		 uint32_t *h_ebx, uint32_t *h_ecx, uint32_t *h_edx,
		 uint32_t *h_esi, uint32_t *h_edi, uint32_t *h_ebp);

void
svm_switch(struct vmcb *vmcb,
	   uint32_t *g_ebx, uint32_t *g_ecx, uint32_t *g_edx,
	   uint32_t *g_esi, uint32_t *g_edi, uint32_t *g_ebp)
{
	struct host_ctx *hctx = &h_ctx0;

	enter_guest(vmcb, g_ebx, g_ecx, g_edx, g_esi, g_edi, g_ebp,
		    &hctx->fs, &hctx->gs, &hctx->ldt,
		    &hctx->ebx, &hctx->ecx, &hctx->edx, &hctx->esi, &hctx->edi,
		    &hctx->ebp);
}

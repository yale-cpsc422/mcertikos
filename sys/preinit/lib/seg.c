#include <lib/gcc.h>
#include <lib/string.h>
#include <lib/types.h>

#include <preinit/lib/x86.h>

#include "seg.h"

static tss_t		tss0;
uint8_t			bsp_kstack[4096] gcc_aligned(4096);
extern char		STACK_LOC[64][4096] gcc_aligned(4096);

#define offsetof(type, member)	__builtin_offsetof(type, member)

void
seg_init(void)
{
	/* clear BSS */
	extern uint8_t end[], edata[];
	memzero(edata, bsp_kstack - edata);
	memzero(bsp_kstack + 4096, end - bsp_kstack - 4096);

	/* setup GDT */
	gdt[0] = SEGDESC_NULL;
	/* 0x08: kernel code */
	gdt[CPU_GDT_KCODE >> 3] =
		SEGDESC32(STA_X | STA_R, 0x0, 0xffffffff, 0);
	/* 0x10: kernel data */
	gdt[CPU_GDT_KDATA >> 3] =
		SEGDESC32(STA_W, 0x0, 0xffffffff, 0);
	/* 0x18: user code */
	gdt[CPU_GDT_UCODE >> 3] =
		SEGDESC32(STA_X | STA_R, 0x00000000, 0xffffffff, 3);
	/* 0x20: user data */
	gdt[CPU_GDT_UDATA >> 3] =
		SEGDESC32(STA_W, 0x00000000, 0xffffffff, 3);

	/* setup TSS */
	tss0.ts_esp0 = (uint32_t) bsp_kstack + 4096;
	tss0.ts_ss0 = CPU_GDT_KDATA;
	gdt[CPU_GDT_TSS >> 3] =
		SEGDESC16(STS_T32A,
			  (uint32_t) (&tss0), sizeof(tss_t) - 1, 0);
	gdt[CPU_GDT_TSS >> 3].sd_s = 0;

	pseudodesc_t gdt_desc = {
		.pd_lim   = sizeof(gdt) - 1,
		.pd_base  = (uint32_t) gdt
	};
	asm volatile("lgdt %0" :: "m" (gdt_desc));
	asm volatile("movw %%ax,%%gs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%fs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%es" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ds" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ss" :: "a" (CPU_GDT_KDATA));
	/* reload %cs */
	asm volatile("ljmp %0,$1f\n 1:\n" :: "i" (CPU_GDT_KCODE));

	/*
	 * Load a null LDT.
	 */
	lldt(0);

	/*
	 * Load the bootstrap TSS.
	 */
	ltr(CPU_GDT_TSS);

	/*
	 * Load IDT.
	 */
	extern pseudodesc_t idt_pd;
        asm volatile("lidt %0" : : "m" (idt_pd));

	/*
	 * Initialize all TSS structures for processes.
	 */
	unsigned int pid;
	memzero(tss, sizeof(tss_t) * 64);
	memzero(STACK_LOC, sizeof(char) * 64 * 4096);
	for (pid = 0; pid < 64; pid++) {
		tss[pid].ts_esp0 = (uint32_t) STACK_LOC[pid] + 4096;
		tss[pid].ts_ss0 = CPU_GDT_KDATA;
		tss[pid].ts_iomb = offsetof(tss_t, ts_iopm);
		memzero(tss[pid].ts_iopm, sizeof(uint8_t) * 128);
		tss[pid].ts_iopm[128] = 0xff;
	}
}

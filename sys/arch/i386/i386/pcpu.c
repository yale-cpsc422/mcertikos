#include <sys/debug.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>
#include <sys/context.h>

#include <machine/mmu.h>
#include <machine/pcpu.h>

#include <dev/lapic.h>

static bool __pcpu_inited = FALSE;

static __pcpu_t _pcpu[MAX_CPU];

extern pseudodesc_t  idt_pd;

static void
__pcpu_print_cpuinfo(void)
{
	KERN_INFO("\n");
	KERN_INFO("\tCPU: %s, FAMILY: %x(%x), MODEL: %x(%x), STEP: %x\n",
		  cpuinfo.vendor,
		  cpuinfo.family, cpuinfo.ext_family,
		  cpuinfo.model, cpuinfo.ext_model,
		  cpuinfo.step);

	if (strncmp(cpuinfo.vendor, "AuthenticAMD", 20) == 0) {

	} else
		KERN_INFO("FEATURES: %x %x\n",
			  cpuinfo.feature1, cpuinfo.feature2);
}

static void
__pcpu_identify(void)
{
	uint32_t eax, ebx, ecx, edx;

	memset(&cpuinfo, 0x0, sizeof(cpuinfo));

	cpuid(0x0, &eax, &ebx, &ecx, &edx);
	cpuinfo.max_input = eax;
	((uint32_t *) cpuinfo.vendor)[0] = ebx;
	((uint32_t *) cpuinfo.vendor)[1] = edx;
	((uint32_t *) cpuinfo.vendor)[2] = ecx;
	cpuinfo.vendor[12] = '\0';

	cpuid(0x1, &eax, &ebx, &ecx, &edx);
	cpuinfo.family = (eax >> 8) & 0xf;
	cpuinfo.model = (eax >> 4) & 0xf;
	cpuinfo.step = eax & 0xf;
	cpuinfo.ext_family = (eax >> 20) & 0xff;
	cpuinfo.ext_model = (eax >> 16) & 0xff;
	cpuinfo.brand_idx = ebx & 0xff;
	cpuinfo.clflush_size = (ebx >> 8) & 0xff;
	cpuinfo.max_cpu_id = (ebx >> 16) &0xff;
	cpuinfo.apic_id = (ebx >> 24) & 0xff;
	cpuinfo.feature1 = ecx;
	cpuinfo.feature2 = edx;

	__pcpu_print_cpuinfo();
}

void
__pcpu_init(void)
{
	int i;

	if (__pcpu_inited == TRUE)
		return;

	__pcpu_identify();

	for (i = 0; i < MAX_CPU; i++) {
		spinlock_init(&_pcpu[i].lk);
		_pcpu[i].inited = FALSE;
		_pcpu[i].mp_inited = FALSE;
	}

	__pcpu_inited = TRUE;
}

void
__pcpu_init_cpu(uint32_t idx, __pcpu_t **_c, uintptr_t kstack_hi)
{
	if (idx >= MAX_CPU) {
		KERN_DEBUG("CPU%d is out of range.\n", idx);
		return;
	}

	spinlock_acquire(&_pcpu[idx].lk);

	if (_pcpu[idx].inited == TRUE) {
		spinlock_release(&_pcpu[idx].lk);
		return;
	}

	/* set up GDT */
	_pcpu[idx].gdt[0] = SEGDESC_NULL;
	/* 0x08: kernel code */
	_pcpu[idx].gdt[CPU_GDT_KCODE >> 3] =
		SEGDESC32(STA_X | STA_R, 0x0, 0xffffffff, 0);
	/* 0x10: kernel data */
	_pcpu[idx].gdt[CPU_GDT_KDATA >> 3] =
		SEGDESC32(STA_W, 0x0, 0xffffffff, 0);
	/* 0x18: user code */
	_pcpu[idx].gdt[CPU_GDT_UCODE >> 3] =
		SEGDESC32(STA_X | STA_R, 0x00000000, 0xffffffff, 3);
	/* 0x20: user data */
	_pcpu[idx].gdt[CPU_GDT_UDATA >> 3] =
		SEGDESC32(STA_W, 0x00000000, 0xffffffff, 3);

	/* set up TSS */
	_pcpu[idx].tss.ts_esp0 = (uint32_t) kstack_hi;
	_pcpu[idx].tss.ts_ss0 = CPU_GDT_KDATA;
	_pcpu[idx].gdt[CPU_GDT_TSS >> 3] =
		SEGDESC16(STS_T32A,
			  (uint32_t) (&_pcpu[idx].tss), sizeof(tss_t) - 1, 0);
	_pcpu[idx].gdt[CPU_GDT_TSS >> 3].sd_s = 0;

	/* load GDT */
	pseudodesc_t gdt_desc = {
		.pd_lim = sizeof(_pcpu[idx].gdt) - 1,
		.pd_base  = (uint32_t) _pcpu[idx].gdt
	};
	asm volatile("lgdt %0" :: "m" (gdt_desc));
	asm volatile("movw %%ax,%%gs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%fs" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%es" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ds" :: "a" (CPU_GDT_KDATA));
	asm volatile("movw %%ax,%%ss" :: "a" (CPU_GDT_KDATA));
	/* reload %cs */
	asm volatile("ljmp %0,$1f\n 1:\n" :: "i" (CPU_GDT_KCODE));

	/* load LDT */
	lldt(0);

	/* load TSS */
	ltr(CPU_GDT_TSS);

	// This "pseudo-descriptor" is needed only by the LIDT instruction,
        // to specify both the size and address of th IDT at once.
        // Load the IDT into this processor's IDT register.
	KERN_DEBUG("idt_pd@%x\n", &idt_pd);
        asm volatile("lidt %0" : : "m" (idt_pd));


	_pcpu[idx].inited = TRUE;

	*_c = &_pcpu[idx];

	spinlock_release(&_pcpu[idx].lk);
}

void
__pcpu_mp_init_cpu(uint32_t idx, uint8_t lapic_id, bool is_bsp)
{
	KERN_ASSERT((is_bsp == TRUE && idx == 0) || (is_bsp == FALSE));

	if (idx >= MAX_CPU)
		return;

	spinlock_acquire(&_pcpu[idx].lk);

	if (_pcpu[idx].mp_inited == TRUE) {
		spinlock_release(&_pcpu[idx].lk);
		return;
	}

	_pcpu[idx].lapic_id = lapic_id;
	_pcpu[idx].is_bsp = is_bsp;

	_pcpu[idx].mp_inited = TRUE;

	spinlock_release(&_pcpu[idx].lk);
}

lapicid_t
__pcpu_cpu_lapicid(uint32_t idx)
{
	return _pcpu[idx].lapic_id;
}

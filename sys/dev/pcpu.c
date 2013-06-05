#include <sys/types.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/string.h>

#include <dev/acpi.h>
#include <dev/ioapic.h>
#include <dev/lapic.h>
#include <dev/pcpu.h>

static bool pcpu_inited = FALSE;
static struct pcpu cpu0;

static gcc_inline void
pcpu_print_cpuinfo(struct pcpuinfo *cpuinfo)
{
	KERN_INFO("CPU: %s, FAMILY %d(%d), MODEL %d(%d), STEP %d, "
		  "FEATURE 0x%x 0x%x,%s%s%s%s%s%s%s "
		  "L1 Cache %d KB (%d bytes) \n",
		  cpuinfo->vendor, cpuinfo->family, cpuinfo->ext_family,
		  cpuinfo->model, cpuinfo->ext_model, cpuinfo->step,
		  cpuinfo->feature1, cpuinfo->feature2,
		  cpuinfo->feature2 & CPUID_FEATURE_SSE ? " SSE," : "",
		  cpuinfo->feature2 & CPUID_FEATURE_SSE2 ? " SSE2," : "",
		  cpuinfo->feature1 & CPUID_FEATURE_SSE3 ? " SSE3," : "",
		  cpuinfo->feature1 & CPUID_FEATURE_SSSE3 ? " SSSE3," : "",
		  cpuinfo->feature1 & CPUID_FEATURE_SSE41 ? " SSE41," : "",
		  cpuinfo->feature1 & CPUID_FEATURE_SSE42 ? " SSE42," : "",
		  cpuinfo->feature1 & CPUID_FEATURE_POPCNT ? " POPCNT," : "",
		  cpuinfo->l1_cache_size, cpuinfo->l1_cache_line_size);
}

static void
pcpu_identify(struct pcpuinfo *cpuinfo)
{
	uint32_t eax, ebx, ecx, edx;

	int i, j;
	uint8_t *desc;
	uint32_t *regs[4] = { &eax, &ebx, &ecx, &edx };

	cpuid(0x0, &eax, &ebx, &ecx, &edx);
	cpuinfo->cpuid_high = eax;
	((uint32_t *) cpuinfo->vendor)[0] = ebx;
	((uint32_t *) cpuinfo->vendor)[1] = edx;
	((uint32_t *) cpuinfo->vendor)[2] = ecx;
	cpuinfo->vendor[12] = '\0';

	if (strncmp(cpuinfo->vendor, "GenuineIntel", 20) == 0)
		cpuinfo->cpu_vendor = INTEL;
	else if (strncmp(cpuinfo->vendor, "AuthenticAMD", 20) == 0)
		cpuinfo->cpu_vendor = AMD;
	else
		cpuinfo->cpu_vendor = UNKNOWN;

	cpuid(0x1, &eax, &ebx, &ecx, &edx);
	cpuinfo->family = (eax >> 8) & 0xf;
	cpuinfo->model = (eax >> 4) & 0xf;
	cpuinfo->step = eax & 0xf;
	cpuinfo->ext_family = (eax >> 20) & 0xff;
	cpuinfo->ext_model = (eax >> 16) & 0xff;
	cpuinfo->brand_idx = ebx & 0xff;
	cpuinfo->clflush_size = (ebx >> 8) & 0xff;
	cpuinfo->max_cpu_id = (ebx >> 16) &0xff;
	cpuinfo->apic_id = (ebx >> 24) & 0xff;
	cpuinfo->feature1 = ecx;
	cpuinfo->feature2 = edx;

	switch (cpuinfo->cpu_vendor) {
	case INTEL:
		KERN_PANIC("Not support yet!\n");
		break;
	case AMD:
		cpuid(0x80000005, &eax, &ebx, &ecx, &edx);
		cpuinfo->l1_cache_size = (ecx & 0xff000000) >> 24;
		cpuinfo->l1_cache_line_size = (ecx & 0x000000ff);
		break;
	default:
		cpuinfo->l1_cache_size = 0;
		cpuinfo->l1_cache_line_size = 0;
		break;
	}

	cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
	cpuinfo->cpuid_exthigh = eax;

	pcpu_print_cpuinfo(cpuinfo);
}

/*
 * multiple processors initialization method using ACPI
 */

static bool
pcpu_arch_init(struct pcpu *c)
{
	uint8_t *p, *e;
	acpi_rsdp_t *rsdp;
	acpi_rsdt_t *rsdt;
	acpi_xsdt_t *xsdt;
	acpi_madt_t *madt;
	bool found_bsp=FALSE;

	KERN_INFO("\n");

	if ((rsdp = acpi_probe_rsdp()) == NULL) {
		KERN_DEBUG("Not found RSDP.\n");
		goto fallback;
	}

	xsdt = NULL;
	if ((xsdt = acpi_probe_xsdt(rsdp)) == NULL &&
	    (rsdt = acpi_probe_rsdt(rsdp)) == NULL) {
		KERN_DEBUG("Not found either RSDT or XSDT.\n");
		goto fallback;
	}

	if ((madt =
	     (xsdt != NULL) ?
	     (acpi_madt_t *) acpi_probe_xsdt_ent(xsdt, ACPI_MADT_SIG) :
	     (acpi_madt_t *) acpi_probe_rsdt_ent(rsdt, ACPI_MADT_SIG)) == NULL) {
		KERN_DEBUG("Not found MADT.\n");
		goto fallback;
	}

	lapic_register(madt->lapic_addr);

	p = (uint8_t *)madt->ent;
	e = (uint8_t *)madt + madt->length;

	while (p < e) {
		acpi_madt_apic_hdr_t * hdr = (acpi_madt_apic_hdr_t *) p;

		switch (hdr->type) {
		case ACPI_MADT_APIC_LAPIC:
			;
			if (found_bsp)
				break;

			acpi_madt_lapic_t *
				lapic_ent = (acpi_madt_lapic_t *) hdr;

			if (!(lapic_ent->flags & ACPI_APIC_ENABLED)) {
				/* KERN_DEBUG("CPU is disabled.\n"); */
				break;
			}

			KERN_INFO("\tCPU: APIC id = %x, ",
				  lapic_ent->lapic_id);

			//according to acpi p.138, section 5.2.12.1,
			//"platform firmware should list the boot processor as the first processor entry in the MADT"
			if (!found_bsp) {
				found_bsp=TRUE;
				KERN_INFO("BSP\n");
				c->arch_info.lapicid = lapic_ent->lapic_id;
				c->arch_info.bsp = TRUE;
				pcpu_identify(&c->arch_info);
			} else {
				KERN_INFO("AP\n");
			}

			break;

		case ACPI_MADT_APIC_IOAPIC:
			;
			acpi_madt_ioapic_t *
				ioapic_ent = (acpi_madt_ioapic_t *)hdr;

			KERN_INFO("\tIOAPIC: APIC id = %x, base = %x\n",
				  ioapic_ent->ioapic_id, ioapic_ent->ioapic_addr);

			ioapic_register(ioapic_ent->ioapic_addr,
					ioapic_ent->ioapic_id,
					ioapic_ent->gsi);

			break;

		default:
			;
			KERN_INFO("\tUnhandled ACPI entry (type=%x)\n",
				  hdr->type);
			break;
		}

		p += hdr->length;
	}

	/*
	 * Force NMI and 8259 signals to APIC when PIC mode
	 * is not implemented.
	 *
	 * TODO: check whether the code is correct
	 */
	if ((madt->flags & APIC_MADT_PCAT_COMPAT) == 0) {
		outb(0x22, 0x70);
		outb(0x23, inb(0x23) | 1);
	}

	return TRUE;

 fallback:
	return FALSE;
 }

void
pcpu_init(void)
{
	int i;

	if (pcpu_inited == TRUE)
		return;

	memzero(&cpu0, sizeof(cpu0));

	pcpu_arch_init(&cpu0);

	spinlock_init(&cpu0.lk);
	cpu0.inited = TRUE;

	pcpu_inited = TRUE;
}

struct pcpu *
pcpu_cur(void)
{
	return &cpu0;
}

lapicid_t
pcpu_cpu_lapicid(void)
{
	return cpu0.arch_info.lapicid;
}

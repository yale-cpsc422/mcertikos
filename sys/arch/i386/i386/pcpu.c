#include <sys/debug.h>
#include <sys/mptable.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/acpi.h>
#include <dev/ioapic.h>
#include <dev/lapic.h>

#include <machine/pcpu.h>

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

	static const int intel_cache_info[0xff][2] = {
		[0x0a] = {  8, 32 },
		[0x0c] = { 16, 32 },
		[0x0d] = { 16, 64 },
		[0x0e] = { 24, 64 },
		[0x2c] = { 32, 64 },
		[0x60] = { 16, 64 },
		[0x66] = {  8, 64 },
		[0x67] = { 16, 64 },
		[0x68] = { 32, 64 }
	};

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
		/* try cpuid 2 first */
		cpuid(0x00000002, &eax, &ebx, &ecx, &edx);
		i = eax & 0x000000ff;
		while (i--)
			cpuid(0x00000002, &eax, &ebx, &ecx, &edx);

		for (i = 0; i < 4; i++) {
			desc = (uint8_t *) regs[i];
			for (j = 0; j < 4; j++) {
				cpuinfo->l1_cache_size =
					intel_cache_info[desc[j]][0];
				cpuinfo->l1_cache_line_size =
					intel_cache_info[desc[j]][1];
			}
		}

		/* try cpuid 4 if no cache info are got by cpuid 2 */
		if (cpuinfo->l1_cache_size && cpuinfo->l1_cache_line_size)
			break;

		for (i = 0; i < 3; i++) {
			cpuid_subleaf(0x00000004, i, &eax, &ebx, &ecx, &edx);
			if ((eax & 0xf) == 1 && ((eax & 0xe0) >> 5) == 1)
				break;
		}

		if (i == 3) {
			KERN_WARN("Cannot determine L1 cache size.\n");
			break;
		}

		cpuinfo->l1_cache_size =
			(((ebx & 0xffc00000) >> 22) + 1) *	/* ways */
			(((ebx & 0x003ff000) >> 12) + 1) *	/* partitions */
			(((ebx & 0x00000fff)) + 1) *		/* line size */
			(ecx + 1) /				/* sets */
			1024;
		cpuinfo->l1_cache_line_size = ((ebx & 0x00000fff)) + 1;

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

bool
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

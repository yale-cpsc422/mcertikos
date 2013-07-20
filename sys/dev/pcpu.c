#include <lib/debug.h>
#include <lib/string.h>
#include <lib/types.h>
#include <lib/x86.h>

#include "acpi.h"
#include "ioapic.h"
#include "lapic.h"
#include "pcpu.h"

static bool pcpu_inited = FALSE;
static struct pcpu cpu0;

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
				c->lapicid = lapic_ent->lapic_id;
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
	if (pcpu_inited == TRUE)
		return;

	memzero(&cpu0, sizeof(cpu0));

	pcpu_arch_init(&cpu0);

	pcpu_inited = TRUE;
}

lapicid_t
pcpu_cpu_lapicid(void)
{
	return cpu0.lapicid;
}

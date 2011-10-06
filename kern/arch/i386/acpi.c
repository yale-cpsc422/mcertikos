#include <architecture/types.h>
#include <architecture/mem.h>
#include <architecture/x86.h>
#include <architecture/acpi.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>


static uint8_t
sum(uint8_t * addr, int len)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < len; i++) {

		sum += addr[i];
	}
	return sum;
}

static acpi_rsdp *
acpi_probe_rsdp_aux(uint8_t *addr, int length)
{
	uint8_t *e, *p;

	cprintf("Search %08x ~ %08x for RSDP", addr, addr+length);
	e = addr + length;
	for (p = addr; p < e; p += 16) {
		if (*(uint32_t *)p == ACPI_RSDP_SIG1 &&
		    *(uint32_t *)(p + 4) == ACPI_RSDP_SIG2 &&
		    sum(p, sizeof(acpi_rsdp)) == 0) {
			cprintf("RSDP is at %08x", p);
			return (acpi_rsdp *)p;
		}
	}

	return NULL;
}

acpi_rsdp *
acpi_probe_rsdp(void)
{
	uint8_t *bda;
	uint32_t p;
	acpi_rsdp *rsdp;

	bda = (uint8_t *) 0x400;
	if ((p = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
		cprintf("Search RSDP from %08x", p);
		if ((rsdp = acpi_probe_rsdp_aux((uint8_t *) p, 1024)))
			return rsdp;
	}

	cprintf("Search RSDP from 0xE0000");
	return acpi_probe_rsdp_aux((uint8_t *) 0xE0000, 0x1FFFF);
}

acpi_rsdt *
acpi_probe_rsdt(acpi_rsdp *rsdp)
{
	assert(rsdp != NULL);

	acpi_rsdt *rsdt = (acpi_rsdt *)(rsdp->rsdt_addr);
	cprintf("rsdp->rsdt_addr = %08x", rsdt);
	if (rsdt == NULL)
		return NULL;
	if (rsdt->sig == ACPI_RSDT_SIG &&
	    sum((uint8_t *)rsdt, rsdt->length) == 0) {
		cprintf("RSDT is at %08x", rsdt);
		return rsdt;
	}

	return NULL;
}

acpi_sdt_hdr *
acpi_probe_rsdt_ent(acpi_rsdt *rsdt, const uint32_t sig)
{
	assert(rsdt != NULL);

	uint8_t * p = (uint8_t *)(&rsdt->ent[0]),
		* e = (uint8_t *)rsdt + rsdt->length;

	cprintf("RSDT->entry is at %08x", rsdt->ent);

	int i;
	for (i = 0; p < e; i++) {
		acpi_sdt_hdr *hdr = (acpi_sdt_hdr *)(rsdt->ent[i]);
		cprintf("RSDT entry (%08x): addr = %08x, sig = %08x, length = %x",
		      &rsdt->ent[i], hdr, hdr->sig, hdr->length);
		if (hdr->sig == sig &&
		    sum((uint8_t *)hdr, hdr->length) == 0) {
			return hdr;
		}
		p = (uint8_t *)(&rsdt->ent[i+1]);
	}

	return NULL;
}

acpi_xsdt *
acpi_probe_xsdt(acpi_rsdp *rsdp)
{
	assert(rsdp != NULL);

	acpi_xsdt *xsdt  = (acpi_xsdt *)rsdp->xsdt_addr;
	cprintf("rsdp->xsdt_addr = %08x", xsdt);
	if (xsdt == NULL)
		return NULL;
	if (xsdt->sig == ACPI_XSDT_SIG &&
	    sum((uint8_t *)xsdt, xsdt->length) == 0) {
		cprintf("XSDT is at %08x", xsdt);
		return xsdt;
	}

	return NULL;
}

acpi_sdt_hdr *
acpi_probe_xsdt_ent(acpi_xsdt *xsdt, const uint32_t sig)
{
	assert(xsdt != NULL);

	uint8_t * p = (uint8_t *)(&xsdt->ent[0]),
		* e = (uint8_t *)xsdt + xsdt->length;

	int i;
	for (i = 0; p < e; i++) {
		acpi_sdt_hdr *hdr = (acpi_sdt_hdr *)(xsdt->ent[i]);
		cprintf("probe XSDT entry %d at %08x", i, hdr);
		if (hdr->sig == sig &&
		    sum((uint8_t *)hdr, hdr->length) == 0) {
			return hdr;
		}
		p = (uint8_t *)(&xsdt->ent[i+1]);
	}

	return NULL;
}

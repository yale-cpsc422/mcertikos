#include <sys/debug.h>
#include <sys/mptable.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <dev/acpi.h>
#include <dev/ioapic.h>
#include <dev/lapic.h>

#include <machine/pcpu.h>

uint32_t ncpu = 0;

bool mp_inited = FALSE;

bool ismp = FALSE;

extern uint8_t _binary___obj_sys_arch_i386_i386_boot_ap_start[];
extern uint8_t _binary___obj_sys_arch_i386_i386_boot_ap_size[];

/*
 * fallback multiple processors initialization method using MP table.
 */

static uint8_t
sum(uint8_t * addr, int len)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < len; i++)
		sum += addr[i];
	return sum;
}

/* Look for an MP structure in the len bytes at addr. */
static struct mp *
mpsearch1(uint8_t * addr, int len)
{
	uint8_t *e, *p;

	e = addr + len;
	for (p = addr; p < e; p += sizeof(struct mp))
		if (memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
			return (struct mp *) p;
	return 0;
}

/*
 * Search for the MP Floating Pointer Structure, which according to the
 * spec is in one of the following three locations:
 * 1) in the first KB of the EBDA;
 * 2) in the last KB of system base memory;
 * 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
 */
static struct mp *
mpsearch(void)
{
	uint8_t          *bda;
	uint32_t            p;
	struct mp      *mp;

	bda = (uint8_t *) 0x400;
	if ((p = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
		if ((mp = mpsearch1((uint8_t *) p, 1024)))
			return mp;
	} else {
		p = ((bda[0x14] << 8) | bda[0x13]) * 1024;
		if ((mp = mpsearch1((uint8_t *) p - 1024, 1024)))
			return mp;
	}
	return mpsearch1((uint8_t *) 0xF0000, 0x10000);
}

/*
 * Search for an MP configuration table.  For now,
 * don 't accept the default configurations (physaddr == 0).
 * Check for correct signature, calculate the checksum and,
 * if correct, check the version.
 *
 * TODO: check extended table checksum.
 */
static struct mpconf *
mpconfig(struct mp **pmp) {
	struct mpconf  *conf;
	struct mp      *mp;

	if ((mp = mpsearch()) == 0 || mp->physaddr == 0)
		return 0;
	conf = (struct mpconf *) mp->physaddr;
	if (memcmp(conf, "PCMP", 4) != 0)
		return 0;
	if (conf->version != 1 && conf->version != 4)
		return 0;
	if (sum((uint8_t *) conf, conf->length) != 0)
		return 0;
	*pmp = mp;
	return conf;
}

static bool
mp_init_fallback(void)
{
	uint8_t *p, *e;
	struct mp      *mp;
	struct mpconf  *conf;
	struct mpproc  *proc;
	struct mpioapic *mpio;
	uint32_t ap_idx = 1;

	if (mp_inited == TRUE)
		return TRUE;

	if ((conf = mpconfig(&mp)) == 0) /* not SMP */
		return FALSE;

	ismp = 1;

	ncpu = 0;

	lapic_register((uintptr_t) conf->lapicaddr);

	for (p = (uint8_t *) (conf + 1), e = (uint8_t *) conf + conf->length;
	     p < e;) {
		switch (*p) {
		case MPPROC:
			proc = (struct mpproc *) p;
			p += sizeof(struct mpproc);
			if (!(proc->flags & MPENAB))
				continue;

			KERN_INFO("\tCPU%d: APIC id = %x, ",
				  ncpu, proc->apicid);

			if (proc->flags & MPBOOT) {
				KERN_INFO("BSP.\n");
				__pcpu_mp_init_cpu(0, proc->apicid, TRUE);
			} else {
				KERN_INFO("AP.\n");
				__pcpu_mp_init_cpu(ap_idx, proc->apicid, FALSE);
				ap_idx++;
			}
			ncpu++;
			continue;
		case MPIOAPIC:
			mpio = (struct mpioapic *) p;
			p += sizeof(struct mpioapic);

			KERN_INFO("\tIOAPIC: APIC id = %x, base = %x\n",
				  mpio->apicno, mpio->addr);

			ioapic_register((uintptr_t) mpio->addr,
					mpio->apicno, 0);
			continue;
		case MPBUS:
		case MPIOINTR:
		case MPLINTR:
			p += 8;
			continue;
		default:
			KERN_WARN("mpinit: unknown config type %x\n", *p);
		}
	}

	if (mp->imcrp) {
		outb(0x22, 0x70);
		outb(0x23, inb(0x23) | 1);
	}

	/*
	 * Copy AP boot code to 0x8000.
	 */
	memcpy((uint8_t *)0x8000,
	       _binary___obj_sys_arch_i386_i386_boot_ap_start,
	       (size_t)_binary___obj_sys_arch_i386_i386_boot_ap_size);

	mp_inited = TRUE;

	return TRUE;
}

/*
 * multiple processors initialization method using ACPI
 */

bool
__pcpu_mp_init(void)
{
	KERN_INFO("\n");

	uint8_t *p, *e;
	acpi_rsdp_t *rsdp;
	acpi_rsdt_t *rsdt;
	acpi_xsdt_t *xsdt;
	acpi_madt_t *madt;
	uint8_t bsp_apic_id;
	uint32_t ap_idx = 1;
	uint32_t cpuinfo[4];

	if (mp_inited == TRUE)
		return TRUE;

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

	ismp = TRUE;

	lapic_register(madt->lapic_addr);

	ncpu = 0;

	cpuid(0x1, &cpuinfo[0], &cpuinfo[1], &cpuinfo[2], &cpuinfo[3]);
	bsp_apic_id = cpuinfo[1] >> 24;

	p = (uint8_t *)madt->ent;
	e = (uint8_t *)madt + madt->length;
	while (p < e) {
		acpi_madt_apic_hdr_t * hdr = (acpi_madt_apic_hdr_t *) p;

		switch (hdr->type) {
		case ACPI_MADT_APIC_LAPIC:
			;
			acpi_madt_lapic_t *
				lapic_ent = (acpi_madt_lapic_t *) hdr;

			if (!(lapic_ent->flags & ACPI_APIC_ENABLED)) {
				/* KERN_DEBUG("CPU is disabled.\n"); */
				break;
			}

			KERN_INFO("\tCPU%d: APIC id = %x, ",
				  ncpu, lapic_ent->lapic_id);

			if (lapic_ent->lapic_id == bsp_apic_id) {
				KERN_INFO("BSP\n");
				__pcpu_mp_init_cpu
					(0, lapic_ent->lapic_id, TRUE);
			} else {
				KERN_INFO("AP\n");
				__pcpu_mp_init_cpu
					(ap_idx, lapic_ent->lapic_id, FALSE);
				ap_idx++;
			}

			ncpu++;

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

	/*
	 * Copy AP boot code to 0x8000.
	 */
	memmove((uint8_t *)0x8000,
	       _binary___obj_sys_arch_i386_i386_boot_ap_start,
		(size_t)_binary___obj_sys_arch_i386_i386_boot_ap_size);

	mp_inited = TRUE;

	return TRUE;

 fallback:
	KERN_DEBUG("Use the fallback multiprocessor initialization.\n");
	if (mp_init_fallback() == FALSE) {
		ismp = 0;
		ncpu = 1;
		return FALSE;
	} else
		return TRUE;
}

uint32_t
__pcpu_ncpu()
{
	return ncpu;
}

bool
__pcpu_is_smp()
{
	return ismp;
}

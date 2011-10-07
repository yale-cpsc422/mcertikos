// Multiprocessor bootstrap.
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf
// This source file adapted from xv6.

#include <architecture/types.h>
#include <architecture/mem.h>
#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <architecture/mp.h>
#include <architecture/mp_internal.h>
#include <architecture/acpi.h>
#include <architecture/pic.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

// is set t true if the MP system has been initialized
static bool mp_inited;

// a value which detects if we are running on an SMP system
bool ismp;

// a value which keeps the actual number of CPUs in the system
int ncpu;

// a table of CPU apicids
uint8_t cpu_ids[MAX_CPU];

// table of stacks of current processors
uint32_t cpu_stacks[MAX_CPU];

// ID of an APIC controller
uint8_t ioapicid;

// Pointers to MMIO area of PICs
volatile struct ioapic *ioapic;
volatile uint32_t* lapic;

static uint8_t
sum(uint8_t * addr, int len)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < len; i++)
		sum += addr[i];
	return sum;
}

//Look for an MP structure in the len bytes at addr.
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

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
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

// Search for an MP configuration table.  For now,
// don 't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
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

	extern uint8_t _binary_obj_boot_bootother_start[],
		   _binary_obj_boot_bootother_size[];


static bool
mp_init_fallback(void)
{
	uint8_t          *p, *e;
	struct mp      *mp;
	struct mpconf  *conf;
	struct mpproc  *proc;
	struct mpioapic *mpio;
	uint32_t nextproc=0;

	// mark MP subsystem as inited
	assert(!mp_inited);
	mp_inited=true;

	// set the primary stack marker to the current stack
	cpu_stacks[0] = ROUNDUP(read_esp(), PAGESIZE);

	// Find and process hardware flags
	if ((conf = mpconfig(&mp)) == 0) {
		// Not a multiprocessor machine - just use boot CPU.
		// configure the MP system for single PIC
		return false;
	}
	ismp = 1;
	lapic = (uint32_t *) conf->lapicaddr;
//	cprintf ("LAPIC set to %x\n", lapic);
//	Search for devices connected to LAPIC
	for (p = (uint8_t *) (conf + 1), e = (uint8_t *) conf + conf->length;
			p < e;) {
		switch (*p) {
		case MPPROC:
			proc = (struct mpproc *) p;
			p += sizeof(struct mpproc);
			if (!(proc->flags & MPENAB))
				continue;	// processor disabled

			// Register a new cpu
			if (proc->flags & MPBOOT)
				cpu_ids[0] = proc->apicid;
			else
				cpu_ids[++nextproc] = proc->apicid;
			ncpu++;
			continue;
		case MPIOAPIC:
			mpio = (struct mpioapic *) p;
			p += sizeof(struct mpioapic);
			ioapicid = mpio->apicno;
			ioapic = (struct ioapic *) mpio->addr;
			continue;
		case MPBUS:
		case MPIOINTR:
		case MPLINTR:
			p += 8;
			continue;
		default:
			panic("mpinit: unknown config type %x\n", *p);
		}
	}
	if (mp->imcrp) {
		// Bochs doesn 't support IMCR, so this doesn' t run on Bochs.
		// But it would on real hardware.
		outb(0x22, 0x70);		// Select IMCR
		outb(0x23, inb(0x23) | 1);	// Mask external interrupts.
	}

	// Set up page 1 with secondary bootstrap code
	// It would be nice to do this with linker voodoo
	uint8_t *boot2 = (uint8_t*)0x1000;
	memmove(boot2, _binary_obj_boot_bootother_start,
			(uint32_t)_binary_obj_boot_bootother_size);
	return true;
}

bool detect_cpuid(void);

static uint8_t
get_bsp_apic_id(void)
{
	if (detect_cpuid() == false) {
		// debug("CPUID is not supported. Assume local APIC ID of BSD is 0x0.\n");
		return 0x0;
	}

	uint32_t cpuinfo[4];
	cpuid(0x1, &cpuinfo[0], &cpuinfo[1], &cpuinfo[2], &cpuinfo[3]);
	// debug("lapic id of current processor is %08x.\n", cpuinfo[1] >> 24);
	return (cpuinfo[1] >> 24);
}

// mp_init needs to execute only once.
bool
mp_init(void)
{
	uint8_t *p, *e;
	acpi_rsdp * rsdp;
	acpi_rsdt * rsdt;
	acpi_xsdt * xsdt;
	acpi_madt * madt;
	uint8_t bsp_apic_id;

	// mark MP subsystem as inited
	assert(!mp_inited);
	mp_inited = true;

	// set the primary stack marker to the current stack
	cpu_stacks[0] = ROUNDUP(read_esp(), PAGESIZE);

	ismp = 1;

	if ((rsdp = acpi_probe_rsdp()) == NULL) {
		// debug("Not found RSDP.\n");
		goto fallback;
	}

	xsdt = NULL;
	if ((rsdt = acpi_probe_rsdt(rsdp)) == NULL &&
	    (xsdt = acpi_probe_xsdt(rsdp)) == NULL) {
		// debug("Not found either RSDT or XSDT.\n");
		goto fallback;
	}

	if ((madt = (xsdt == NULL) ?
	     (acpi_madt *)acpi_probe_rsdt_ent(rsdt, ACPI_MADT_SIG) :
	     (acpi_madt *)acpi_probe_xsdt_ent(xsdt, ACPI_MADT_SIG)) == NULL) {
		// debug("Not found MADT.\n");
		goto fallback;
	}

	lapic = (uint32_t *) (madt->lapic_addr);
	// debug("Local APIC: addr = %08x.\n", lapic);

	ncpu = 0;
	bsp_apic_id = get_bsp_apic_id();
	p = (uint8_t *)madt->ent;
	e = (uint8_t *)madt + madt->length;
	while (p < e) {
		acpi_madt_apic_hdr * hdr = (acpi_madt_apic_hdr *)p;
		switch (hdr->type) {
		case ACPI_MADT_APIC_LAPIC:
			;
			acpi_madt_lapic *lapic_ent = (acpi_madt_lapic *)hdr;

			// debug("Local APIC: acip id = %08x, lapic id = %08x, flags = ", lapic_ent->acip_proc_id, lapic_ent->lapic_id);

			if (!(lapic_ent->flags & ACPI_APIC_ENABLED)) {
				// debug("disabled.\n");
				continue;
			} else
				// debug("enabled.\n");
				;

			if (lapic_ent->lapic_id == bsp_apic_id)
				cpu_ids[0] = lapic_ent->lapic_id;
			else
				cpu_ids[++ncpu] = lapic_ent->lapic_id;

			break;

		case ACPI_MADT_APIC_IOAPIC:
			;
			acpi_madt_ioapic *ioapic_ent = (acpi_madt_ioapic *)hdr;

			// debug("IO APIC: ioapic id = %08x, ioapic addr = %08x, gsi = %08x\n", ioapic_ent->ioapic_id, ioapic_ent->ioapic_addr, ioapic_ent->gsi);

			ioapicid = ioapic_ent->ioapic_id;
			ioapic = (struct ioapic *) (ioapic_ent->ioapic_addr);

			break;

		default:
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
		// debug("PIC mode is not implemented. Force NMI and 8259 signals to APIC.\n");
		outb(0x22, 0x70);		// Select IMCR
		outb(0x23, inb(0x23) | 1);	// Mask external interrupts.
	}

	// Set up page 1 with secondary bootstrap code
	// It would be nice to do this with linker voodoo
	uint8_t *boot2 = (uint8_t*)0x1000;
	memmove(boot2, _binary_obj_boot_bootother_start,
			(uint32_t)_binary_obj_boot_bootother_size);
	return true;

 fallback:
	if (mp_init_fallback() == false) {
		// debug("Fallback mp_init failed. Assume it's a non-smp system.\n");
		ismp = 0;
		return false;
	} else
		return true;
}


// Returns the number of processors in the system
int mp_ncpu()
{
	assert(mp_inited);
	return ncpu;
}

uint8_t mp_curcpu()
{
	int i;
	for(i=0;i<ncpu;i++) {

//	cprintf("cpustack:%d @%x, esp:@%x\n",i,&cpu_stacks[i],read_esp());
		if (cpu_stacks[i] == ROUNDUP(read_esp(),PAGESIZE))
			return i;
	}
	assert (1 == 0);
}

// pointer to the C-part of the secondary bootloader
// e.g. other_helper.c
// used by the secondary ASM bootloader
void other_init(void(*f)(void));


// boots the CPU, telling it to run f once initialized
// does not return until CPU has activated advanced mode
// i.e. released page1 lock
static volatile bool booting;

void mp_boot(int cpu, void(*f)(void), uint32_t kstack_loc)
{
	debug("Boot CPu%d: apic id = %08x, stack addr = %x\n",
	      cpu, cpu_ids[cpu], kstack_loc);

	//assert(!mp_booted(cpu));
	assert(!booting);
	booting=true;
	cpu_stacks[cpu] = kstack_loc+PAGESIZE;
	uint8_t *boot = (uint8_t*)0x1000;
	*(uint32_t*)(boot-4) = kstack_loc+PAGESIZE;
	*(uint32_t*)(boot-8) = (uint32_t)f;
	*(uint32_t*)(boot-12) = (uint32_t)other_init;
	lapic_startcpu(cpu_ids[cpu], (uint32_t)boot);

	while(booting);
//	cprintf("Waiting for CPU %d to boot\n", cpu);
//
//	We should wait using locks
//	while(!mp_booted(cpu));
}

void mp_boot_vm(int cpu, void(*f)(uint32_t), uint32_t kstack_loc)
{
	//assert(!mp_booted(cpu));
	assert(!booting);
	booting=true;
	cpu_stacks[cpu] = kstack_loc+PAGESIZE;
	uint8_t *boot = (uint8_t*)0x1000;
	*(uint32_t*)(boot-4) = kstack_loc+PAGESIZE;
	*(uint32_t*)(boot-8) = (uint32_t)f;
	*(uint32_t*)(boot-12) = (uint32_t)other_init;
	lapic_startcpu(cpu_ids[cpu], (uint32_t)boot);

	while(booting);
//	cprintf("Waiting for CPU %d to boot\n", cpu);
//
//	We should wait using locks
//	while(!mp_booted(cpu));
}

void mp_donebooting() {
	assert(booting);
	booting=false;
}

#define IO_RTC  0x70

// Start additional processor running bootstrap code at addr.
// See Appendix B of MultiProcessor Specification.
void
lapic_startcpu(uint8_t apicid, uint32_t addr)
{
	int i;
	uint16_t *wrv;

	// "The BSP must initialize CMOS shutdown code to 0AH
	// and the warm reset vector (DWORD based at 40:67) to point at
	// the AP startup code prior to the [universal startup algorithm]."
	outb(IO_RTC, 0xF);  // offset 0xF is shutdown code
	outb(IO_RTC+1, 0x0A);
	wrv = (uint16_t*)(0x40<<4 | 0x67);  // Warm reset vector
	wrv[0] = 0;
	wrv[1] = addr >> 4;

	// "Universal startup algorithm."
	// Send INIT (level-triggered) interrupt to reset other CPU.
	lapicw(ICRHI, apicid<<24);
	lapicw(ICRLO, INIT | LEVEL | ASSERT);
	microdelay(200);
	lapicw(ICRLO, INIT | LEVEL);
	microdelay(100);    // should be 10ms, but too slow in Bochs!

	// Send startup IPI (twice!) to enter bootstrap code.
	// Regular hardware is supposed to only accept a STARTUP
	// when it is in the halted state due to an INIT.  So the second
	// should be ignored, but it is part of the official Intel algorithm.
	// Bochs complains about the second one.  Too bad for Bochs.
	for(i = 0; i < 2; i++){
		lapicw(ICRHI, apicid<<24);
		lapicw(ICRLO, STARTUP | (addr>>12));
		microdelay(200);
	}
}




// Interrupt Related code here


// This function sets up an interrupt subsystem
// By default activates the timer interrupt
// This initialization needs to happen on each processor
// ????
void interrupts_init() {
	assert(mp_inited);
	if (ismp) {
		ioapic_init();
		lapic_init();
	}
	else {
		pic_init();
	}
}


void interrupts_enable(int irq, int cpunum) {
	assert (0 <= irq && irq < 16);
	assert (0 <= cpunum && cpunum < ncpu);
	pic_enable(irq);
	if (ismp) {
		ioapic_enable(irq, cpu_ids[cpunum]);
	}
}

void interrupts_eoi() {
	ismp ? lapic_eoi() : pic_eoi();
}



// PIC control code here


static uint32_t
ioapic_read(int reg)
{
	ioapic->reg = reg;
	return ioapic->data;
}

static void
ioapic_write(int reg, uint32_t data)
{
	ioapic->reg = reg;
	ioapic->data = data;
}

void
ioapic_init(void)
{
	int i, id, maxintr;

	assert(ioapic != NULL);

	maxintr = (ioapic_read(REG_VER) >> 16) & 0xFF;
	id = ioapic_read(REG_ID) >> 24;
	if (id == 0) {
		// I/O APIC ID not initialized yet - have to do it ourselves.
		ioapic_write(REG_ID, ioapicid << 24);
		id = ioapicid;
	}
	if (id != ioapicid)
		warn("ioapicinit: id %d != ioapicid %d\n", id, ioapicid);

	// Mark all interrupts edge-triggered, active high, disabled,
	// and not routed to any CPUs.
	for (i = 0; i <= maxintr; i++){
		ioapic_write(REG_TABLE+2*i, INT_DISABLED | (T_IRQ0 + i));
		ioapic_write(REG_TABLE+2*i+1, 0);
	}
}

void
ioapic_enable(int irq, int apicid)
{
	assert(ismp);
	// Mark interrupt edge-triggered, active high,
	// enabled, and routed to the given APIC ID,
	ioapic_write(REG_TABLE+2*irq, T_IRQ0 + irq);
	ioapic_write(REG_TABLE+2*irq+1, apicid << 24);
}


static void
lapicw(int index, int value)
{
	lapic[index] = value;
	lapic[ID];  // wait for write to finish, by reading
}

void
lapic_init()
{
	if (!lapic) {
	   panic("NO LAPIC");
		return;
	}
//	cprintf("Using LAPIC at %x\n", lapic);

	// Enable local APIC; set spurious interrupt vector.
	lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

	// The timer repeatedly counts down at bus frequency
	// from lapic[TICR] and then issues an interrupt.
	// If we cared more about precise timekeeping,
	// TICR would be calibrated using an external time source.
	lapicw(TDCR, X1);
	lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
	lapicw(TICR, 10000000);

	// Disable logical interrupt lines.
	lapicw(LINT0, MASKED);
	lapicw(LINT1, MASKED);

	// Disable performance counter overflow interrupts
	// on machines that provide that interrupt entry.
	if (((lapic[VER]>>16) & 0xFF) >= 4)
		lapicw(PCINT, MASKED);

	// Map error interrupt to IRQ_ERROR.
	lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

	// Clear error status register (requires back-to-back writes).
	lapicw(ESR, 0);
	lapicw(ESR, 0);

	// Ack any outstanding interrupts.
	lapicw(EOI, 0);

	// Send an Init Level De-Assert to synchronise arbitration ID's.
	lapicw(ICRHI, 0);
	lapicw(ICRLO, BCAST | INIT | LEVEL);
	while(lapic[ICRLO] & DELIVS)
		;

	// Enable interrupts on the APIC (but not on the processor).
	lapicw(TPR, 0);
}


// Acknowledge interrupt.
void
lapic_eoi(void)
{
	if (lapic)
		lapicw(EOI, 0);
}

// Spin for a given number of microseconds.
// On real hardware would want to tune this dynamically.
void
microdelay(int us)
{
}

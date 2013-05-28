#include <sys/types.h>
#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/pcpu.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/string.h>

#include <machine/pcpu.h>

static bool pcpu_inited = FALSE;
static struct pcpu cpu0;

void
pcpu_init(void)
{
	int i;

	if (pcpu_inited == TRUE)
		return;

	memzero(&cpu0, sizeof(cpu0));

	pcpu_arch_init(&cpu0);

	spinlock_init(&cpu0.lk);
	cpu0.hvm_inited = FALSE;
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

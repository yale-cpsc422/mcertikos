#ifndef __CPU_H__
#define __CPU_H__

//#include "types.h"
#include <architecture/types.h>
#include "cpufeature.h"


enum {
	X86_VENDOR_AMD     = 2,
	X86_VENDOR_UNKNOWN = 0xff
};

struct cpuinfo_x86 {
	uint8_t	x86;		/* CPU family */
	uint8_t	x86_vendor;	/* CPU vendor */
	uint8_t	x86_model;
	uint8_t	x86_mask;
	int	cpuid_max_stdfunc;	/* Maximum supported CPUID standard function, -1 = no CPUID */
	uint32_t	x86_capability[NCAPINTS];
	char	x86_vendor_id[16];
	char	x86_model_id[64];
	int 	x86_cache_size;  /* in KB */
	int	x86_clflush_size;
	int	x86_cache_alignment;
	int	x86_tlbsize;	/* number of 4K pages in DTLB/ITLB combined(in pages)*/
        uint8_t    x86_virt_bits, x86_phys_bits;
	uint8_t	x86_max_cores;	/* cpuid returned max cores value */
        uint32_t   x86_power;
	uint32_t   cpuid_max_exfunc;	/* Max extended CPUID function supported */
	unsigned long loops_per_jiffy;
	uint8_t	apicid;
	uint8_t	booted_cores;	/* number of cores as seen by OS */
};



#endif /* __CPU_H__ */

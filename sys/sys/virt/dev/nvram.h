#ifndef _SYS_VIRT_DEV_NVRAM_H_
#define _SYS_VIRT_DEV_NVRAM_H_

/*
 * XXX: Virtualized NVRAM does NOT intend to provide the full simulation of
 *      NVRAM. It does only handle the requests of getting memory size. All
 *      other requests are passed to the physical NVRAM.
 *
 *      The functions handled by virtualized NVRAM include:
 *      - getting physical memory size between 1 MB ~ 16 MB,
 *      - getting physical memory size between 16 MB ~ 4 GB, and
 *      - getting physical memory size above 4 GB.
 */

#ifdef _KERN_

#include <sys/types.h>

#include <sys/virt/vmm.h>

struct vnvram {
	bool		data_valid;
	size_t		extmem_size;
	size_t		extmem2_size;
	uint64_t	highmem_size;

	uint8_t	data;
};

void vnvram_init(struct vnvram *, struct vm *);

#endif /* _KERN_ */

#endif /* !_SYS_VIRT_DEV_NVRAM_H_ */

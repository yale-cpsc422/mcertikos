#include <gcc.h>
#include <types.h>
#include <debug.h>
#include <bios.h>
#include <x86.h>

typedef
struct disk_address_packet {
	uint8_t  size;
	uint8_t  _reserved;
	uint16_t count;
	uint32_t buf_addr;
	uint64_t start_sect;
} gcc_packed dap_t;

int read_sector(int drive, uint64_t sector, void *buf)
{
	/* debug("read_sector: drive = %x, sector = %x, buf = %x\n", */
	/*       drive, (uint32_t)sector, (uint32_t)buf); */

	assert(sector >= 0 && buf != 0);

	dap_t dap = {
		.size = 0x10,
		._reserved = 0x0,
		.count = 1,
		.buf_addr = (uint32_t)buf,
		.start_sect = sector
	};

	return int13x(0x42, drive, &dap);
}

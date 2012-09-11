#include <debug.h>
#include <gcc.h>
#include <string.h>
#include <syscall.h>
#include <types.h>

#include "pci.h"

#ifdef DEBUG_VPCI

#define VPCI_DEBUG(fmt, ...)				\
	{						\
		DEBUG("VPCI: "fmt, ##__VA_ARGS__);	\
	}

#else

#define VPCI_DEBUG(fmt...)			\
	{					\
	}

#endif

int
main(int argc, char **argv)
{
	int parent_chid;

	uint8_t buf[1024];
	size_t size;

	uint16_t port;

	struct ioport_rw_req *rw_req;
	struct ioport_read_ret *ret;
	struct device_ready *dev_rdy;

	for (port = PCI_CONFIG_ADDR; port < PCI_CONFIG_ADDR+4; port++) {
		sys_register_ioport(port, SZ8, 0);
		sys_register_ioport(port, SZ8, 1);

		if (PCI_CONFIG_ADDR + 4 - port > 1) {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		} else {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		}

		if (port == PCI_CONFIG_ADDR) {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		} else {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		}
	}

	for (port = PCI_CONFIG_DATA; port < PCI_CONFIG_DATA+4; port++) {
		sys_register_ioport(port, SZ8, 0);
		sys_register_ioport(port, SZ8, 1);

		if (PCI_CONFIG_DATA + 4 - port > 1) {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		} else {
			sys_register_ioport(port, SZ16, 0);
			sys_register_ioport(port, SZ16, 1);
		}

		if (port == PCI_CONFIG_DATA) {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		} else {
			sys_register_ioport(port, SZ32, 0);
			sys_register_ioport(port, SZ32, 1);
		}
	}

	parent_chid = sys_getpchid();

	dev_rdy = (struct device_ready *) buf;
	dev_rdy->magic = MAGIC_DEVICE_READY;
	sys_send(parent_chid, dev_rdy, sizeof(struct device_ready));

	while (1) {
		if (sys_recv(parent_chid, buf, &size, TRUE))
			continue;

		switch (((uint32_t *) buf)[0]) {
		case MAGIC_IOPORT_RW_REQ:
			rw_req = (struct ioport_rw_req *) buf;

			if (rw_req->port < PCI_CONFIG_ADDR ||
			    rw_req->port >= PCI_CONFIG_DATA+4)
				continue;

			ret = (struct ioport_read_ret *)buf;

			if (rw_req->write == 0) {
				ret->magic = MAGIC_IOPORT_READ_RET;
				ret->data = 0xffffffff;
				sys_send(parent_chid,
					 ret, sizeof(struct ioport_read_ret));
			}

			continue;

		default:
			continue;
		}
	}

	return 0;
}

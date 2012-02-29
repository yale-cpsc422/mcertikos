#ifndef _SYS_MGMT_H_
#define _SYS_MGMT_H_

#ifdef _KERN_

#include <sys/types.h>

typedef
struct mgmt_start_t {
	uint32_t cpu;
	pid_t pid;
} mgmt_start_t;

typedef
struct mgmt_stop_t {
	uint32_t cpu;
} mgmt_stop_t;

typedef
struct mgmt_allocpage_t {
	pid_t pid;
	uintptr_t va;
} mgmt_allocpage_t;

typedef
struct mgmt_data_t {
	uint32_t cmd;
	uint8_t params[100];
} mgmt_data_t;

#endif /* _KERN_ */

#endif /* !_SYS_MGMT_H_ */

#ifndef _KERN_USER_H_
#define _KERN_USER_H_

#ifndef _KERN_
#error "This is a kernel header; do not include it in userspace programs"
#endif

#include <sys/types.h>

typedef
struct {
	uint32_t cpu;
	pid_t pid;
} mgmt_start_t;

typedef
struct {
	uint32_t cpu;
} mgmt_stop_t;

typedef
struct {
	pid_t pid;
	uintptr_t va;
} mgmt_allocpage_t;

typedef
struct {
	uint32_t cmd;
	uint8_t params[100];
} mgmt_data_t;

#endif /* !_KERN_USER_H_ */

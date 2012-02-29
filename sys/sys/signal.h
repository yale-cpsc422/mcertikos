#ifndef _KERN_SIGNAL_H_
#define _KERN_SIGNAL_H_

#ifndef _KERN_
#error "This is a kernel header; do not include it in userspace programs"
#endif

#include <sys/types.h>

typedef
enum signal_type {
	SIGNAL_TIMER,
	SIGNAL_PGFLT,
} signal_type;

typedef
struct {
	uint32_t cpu;
	pid_t procid;
	uint32_t fault_addr;
} signal_pgflt_t;

typedef
struct signal {
	signal_type type;
	uint8_t data[];
} sig_t;

typedef
struct signaldesc {
	void(*f)(void);
	sig_t *s;
} sigdesc_t;

#endif /* !_KERN_SIGNAL_H_ */

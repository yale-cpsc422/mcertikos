#ifndef _KERN_MASTER_H_
#define _KERN_MASTER_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif

#include <sys/kernel.h>

void master_kernel(void);

#endif /* !_KERN_MASTER_H_ */

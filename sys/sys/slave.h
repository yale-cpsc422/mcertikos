#ifndef _KERN_SLAVE_H_
#define _KERN_SLAVE_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif

#include <sys/gcc.h>
#include <sys/kernel.h>

void slave_kernel(void);

#endif /* !_KERN_SLAVE_H_ */

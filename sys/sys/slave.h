#ifndef _KERN_SLAVE_H_
#define _KERN_SLAVE_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace programs."
#endif

void slave_kernel(void);

#endif /* !_KERN_SLAVE_H_ */

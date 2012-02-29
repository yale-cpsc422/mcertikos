#ifndef _KERN_AS_H_
#define _KERN_AS_H_

#ifndef _KERN_
#error "This is a kernel header file; do not include it in userspace program."
#endif /* !_KERN_ */

#include <sys/types.h>

#include <machine/pmap.h>

typedef pmap_t as_t;

as_t *kern_as;

as_t *as_init(void);

as_t *as_new(bool user);
void as_free(as_t *);

void as_activate(as_t *);

as_t *as_assign(as_t *, uintptr_t, int, pageinfo_t *);
as_t *as_reserve(as_t *, uintptr_t, int);
as_t *as_unassign(as_t *, uintptr_t, size_t);

as_t *as_cur(void);
as_t *as_setperm(as_t *, uintptr_t, int, size_t);
uintptr_t as_checkrange(as_t *, void *, size_t);

size_t as_copy(as_t *, uintptr_t, as_t *, uintptr_t, size_t);
size_t as_memset(as_t *, uintptr_t, char, size_t);

#endif /* !_KERN_AS_H_ */

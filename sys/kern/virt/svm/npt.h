#include <sys/types.h>

typedef uint32_t *npt_t;

/*
 * Create a new and empty NPT,
 *
 * @return a pointer to the NPT if successful; otherwise, return NULL.
 */
npt_t npt_new(void);

/*
 * Free a NPT.
 */
void npt_free(npt_t npt);

/*
 * Map a guest physical address to a host physical address in the given NPT.
 *
 * @param npt the pointer to NPT
 * @param gpa the guest phyiscal address; it must be aligned to 4 KB
 * @param hpa the host physical address; it must be aligned to 4 KB
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int npt_insert(npt_t npt, uintptr_t gpa, uintptr_t hpa);

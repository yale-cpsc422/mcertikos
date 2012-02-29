#include <sys/types.h>
#include <sys/as.h>
#include <sys/debug.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/string.h>
#include <sys/x86.h>

#include <machine/pmap.h>

as_t *as_active[MAX_CPU];

as_t *
as_init()
{
	if (pcpu_onboot() == TRUE)
		pmap_init();

	as_t *as = as_new(FALSE);
	if (as == NULL)
		return NULL;

	pmap_enable();

	as_activate(as);

	/* TODO: move these cr0 codes to machine-dependt part */
	uint32_t cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_TS|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	return as;
}

as_t *
as_cur()
{
	return as_active[pcpu_cur_idx()];
}

void
as_activate(as_t *as)
{
	/* KERN_DEBUG("as_activate: %x\n", as); */
	as_active[pcpu_cur_idx()] = as;
	pmap_install(as);
}

/*
 * TODO: as_new() is actually machine-dependent (32-bit).
 */
as_t *
as_new(bool user)
{
	pmap_t* pmap = pmap_new();

	if (pmap == NULL)
		return NULL;

	pmap = (user == TRUE) ? pmap_uinit(pmap) : pmap_kinit(pmap);

	if (pmap == NULL)
		return NULL;

	return ((as_t *) pmap);
}

void
as_free(as_t *as)
{
	pmap_free((pmap_t *) as);
}

as_t *
as_unassign(as_t *as, uintptr_t va, size_t size)
{
	KERN_ASSERT(PGOFF(size) == 0);

	uint32_t vahi = va + size;
	KERN_ASSERT(vahi > va);

	pmap_remove(as, va, size);

	return as;
}

as_t *
as_reserve(as_t *as, uintptr_t va, int perm)
{
	pageinfo_t *pi = (pageinfo_t *) mem_page_alloc();

	if (pi == NULL)
		return NULL;

	mem_incref(pi);

	memset((void *) mem_pi2phys(pi), 0, PAGESIZE);

	return pmap_insert(as, pi, va, perm);
}

size_t
as_copy(as_t *das, uintptr_t dva, as_t *sas, uintptr_t sva, size_t size)
{
	return pmap_copy((pmap_t *) das, dva, (pmap_t *) sas, sva, size);
}

size_t
as_memset(as_t *as, uintptr_t va, char v, size_t size)
{
	return pmap_memset((pmap_t *) as, va, v, size);
}

as_t *
as_setperm(as_t *as, uintptr_t va, int perm, size_t size)
{
	if (pmap_setperm(as, va, size, perm) == NULL)
		return NULL;
	return as;
}

uintptr_t
as_checkrange(as_t *as, void *va, size_t size)
{
	return pmap_checkrange((pmap_t *) as, (uintptr_t) va, size);
}

as_t *
as_assign(as_t *as, uintptr_t va, int perm, pageinfo_t *pi)
{
	return pmap_insert(as, pi, va, perm);
}

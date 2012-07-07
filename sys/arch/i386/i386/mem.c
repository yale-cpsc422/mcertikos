#include <sys/debug.h>
#include <sys/mboot.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/mem.h>

/* Is mem module initialized? */
static bool mem_inited = FALSE;

/*
 * After mem_init(), all operations on mem_pageinfo should be done after
 * getting this lock.
 */
static spinlock_t mem_lock;

/* linked list of page informations of all physical memory pages */
static pageinfo_t *mem_pageinfo;

/* linked list of page informations of all free physical memory pages */
static pageinfo_t *mem_freepage;

/* max physical address in bytes */
static size_t mem_max;

/* amount of physical page */
static size_t mem_npages;

static int __free_pmmap_ent_idx = 0;


/*
 * Insert an physical memory map entry in pmmap[].
 *
 * XXX: The start fields of all entries of the physical memory map are in
 *      incremental order.
 * XXX: The memory regions of some entries maybe overlapped.
 *
 * @param start
 * @param end
 * @param type
 */
static void
pmmap_insert(uintptr_t start, uintptr_t end, uint32_t type)
{
	pmmap_t *pre, *p;

	pre = NULL;
	if (__free_pmmap_ent_idx == 0)
		p = NULL;
	else
		p = &pmmap[0];

	while (p != NULL) {
		if (start < p->start)
			break;
		pre = p;
		p = p->next;
	}

	pmmap[__free_pmmap_ent_idx].start = start;
	pmmap[__free_pmmap_ent_idx].end = end;
	pmmap[__free_pmmap_ent_idx].type = type;
	pmmap[__free_pmmap_ent_idx].next = p;
	if (pre != NULL)
		pre->next = &pmmap[__free_pmmap_ent_idx];
	__free_pmmap_ent_idx++;
}

static void
pmmap_merge(void)
{
	pmmap_t *p = &pmmap[0];

	while (p != NULL) {
		if (p->next == NULL) /* we reach the last entry */
			break;

		pmmap_t *q = p->next;

		/*
		 * Merge p and q if they are overlapped; otherwise turn to next
		 * pair of pmmap entries.
		 */
		if (p->start <= q->start && p->end >= q->start &&
		    p->type == q->type) {
			p->end = MAX(p->end, q->end);
			p->next = q->next;
		} else
			p = q;
	}

	/* link the entries of the same type together */
	pmmap_t *last_usable, *last_resv, *last_acpi_reclaim, *last_acpi_nvs;

	pmmap_usable = pmmap_resv = pmmap_acpi = pmmap_nvs = NULL;
	last_usable = last_resv = last_acpi_reclaim = last_acpi_nvs = NULL;
	p = &pmmap[0];

	while (p != NULL) {
		switch (p->type) {
		case MEM_RAM:
			if (pmmap_usable == NULL)
				pmmap_usable = p;
			if (last_usable != NULL)
				last_usable->type_next = p;
			p->type_next = NULL;
			last_usable = p;

			break;

		case MEM_RESERVED:
			if (pmmap_resv == NULL)
				pmmap_resv = p;
			if (last_resv != NULL)
				last_resv->type_next = p;
			p->type_next = NULL;
			last_resv = p;

			break;

		case MEM_ACPI:
			if (pmmap_acpi == NULL)
				pmmap_acpi = p;
			if (last_acpi_reclaim != NULL)
				last_acpi_reclaim->type_next = p;
			p->type_next = NULL;
			last_acpi_reclaim = p;

			break;

		case MEM_NVS:
			if (pmmap_nvs == NULL)
				pmmap_nvs = p;
			if (last_acpi_nvs != NULL)
				last_acpi_nvs->type_next = p;
			p->type_next = NULL;
			last_acpi_nvs = p;

			break;

		default:
			break;
		}

		p = p->next;
	}
}

static void
pmmap_dump(void)
{
	pmmap_t *p = pmmap;
	while (p != NULL) {
		uintptr_t start = p->start;
		uintptr_t end = p->end;
		uint32_t type = p->type;

		KERN_INFO("\tBIOS-e820: %08x - %08x",
			  start, (start == end) ? end : end-1);
		if (type == MEM_RAM)
			KERN_INFO(" (usable)\n");
		else if (type == MEM_RESERVED)
			KERN_INFO(" (reserved)\n");
		else if (type == MEM_ACPI)
			KERN_INFO(" (ACPI data)\n");
		else if (type == MEM_NVS)
			KERN_INFO(" (ACPI NVS)\n");
		else
			KERN_INFO(" (unknown)\n");

		p = p->next;
	}
}

static void
pmmap_init(mboot_info_t *mbi)
{
	KERN_INFO("\n");

	mboot_mmap_t *p = (mboot_mmap_t *) mbi->mmap_addr;

	/*
	 * Copy memory map information from multiboot information mbi to pmmap.
	 */
	while ((uintptr_t) p - (uintptr_t) mbi->mmap_addr < mbi->mmap_length) {
		uintptr_t start,end;
		uint32_t type;

		if (p->base_addr_high != 0)	/* ignore address above 4G */
			goto next;
		else
			start = p->base_addr_low;

		if (p->length_high != 0 ||
		    p->length_low >= 0xffffffff - start)
			end = 0xffffffff;
		else
			end = start + p->length_low;

		type = p->type;

		pmmap_insert(start, end, type);

	next:
		p = (mboot_mmap_t *) (((uint32_t) p) + sizeof(mboot_mmap_t)/* p->size */);
	}

	/* merge overlapped memory regions */
	pmmap_merge();
	pmmap_dump();
}

static uintptr_t
pmmap_max()
{
	pmmap_t *p = pmmap;
	pmmap_t *last_resv_ent = NULL;

	while (p->next != NULL) {
		if (p->type == MEM_RAM)
			last_resv_ent = p;
		p = p->next;
	}

	if (last_resv_ent == NULL)
		return 0;
	else
		return last_resv_ent->end;
}

void
mem_init(mboot_info_t *mbi)
{
	if (mem_inited == TRUE)
		return;

	extern uint8_t end[];

	/* get a usable physical memory map */
	pmmap_init(mbi);

	/*
	 * get the maximum address and the amounts of memory pages, including
	 * all available pages and reserved pages
	 */
	mem_max = pmmap_max();
	mem_npages = pmmap_max() / PAGE_SIZE;

	/* reserve memory for mem_npage pageinfo_t structures */
	mem_pageinfo =
		(pageinfo_t *) ROUNDUP((uintptr_t)end, sizeof(pageinfo_t));
	memset(mem_pageinfo, 0x0, sizeof(pageinfo_t) * mem_npages);

	/*
	 * initialize free memory page list
	 *
	 * 1) all pages below 1MB are reserved,
	 * 2) all pages below end are reserved,
	 * 3) all pages for array mem_pageinfo are reserved, and
	 * 4) all pages above 0xf0000000 are reserved.
	 * 5) all pages not in MEM_RAM memory regions are reserved.
	 */
	pmmap_t *p = pmmap;
	pageinfo_t *last_freepage = NULL;
	uint32_t nfreepags = 0;
	while (p != NULL) {
		if (p->type != MEM_RAM)
			goto next;

		uintptr_t lo = (uintptr_t) ROUNDUP(p->start, PAGE_SIZE);
		uintptr_t hi = (uintptr_t) ROUNDDOWN(p->end, PAGE_SIZE);

		if (lo >= hi)
			goto next;

		uintptr_t i;
		for (i = lo; i < hi; i += PAGE_SIZE) {
			if (i < (uintptr_t)
			    ROUNDUP((uintptr_t) (mem_pageinfo + mem_npages),
				    PAGE_SIZE) ||
			    i >= 0xf0000000)
				continue;

			KERN_ASSERT(p->type == MEM_RAM);
			pageinfo_t *freepage = mem_phys2pi(i);
			freepage->free_next = NULL;
			if (last_freepage == NULL)
				mem_freepage = freepage;
			else
				last_freepage->free_next = freepage;
			last_freepage = freepage;
			freepage->free_next = NULL;

			nfreepags++;
		}

	next:
		p = p->next;
	}

	spinlock_init(&mem_lock);
}

pageinfo_t *
mem_page_alloc()
{
	spinlock_acquire(&mem_lock);

	pageinfo_t *pi = mem_freepage;
	mem_freepage = pi->free_next;
	if (pi != NULL)
		pi->free_next = NULL;

	spinlock_release(&mem_lock);

	if (pi == NULL) {
		KERN_WARN("No physical memory available.\n");
	}

	return pi;
}

static int
mem_find_continuous_pages(pageinfo_t *freepage, int npage)
{
	pageinfo_t *p = freepage, *q;
	int counter = 0;

	while (p != NULL && counter < npage) {
		counter++;

		q = p->free_next;

		if (q == NULL)
			return counter;

		if (q - p != 1)
			return counter;

		p = q;
	}

	return counter;
}

/*
 * Allocate multiple continous pages of physical memory, which is at least size
 * bytes.
 */
pageinfo_t *
mem_pages_alloc(size_t size)
{
	spinlock_acquire(&mem_lock);

	int i;
	int npages = ROUNDUP(size, PAGESIZE) / PAGESIZE;
	pageinfo_t *pi = mem_freepage;
	pageinfo_t *pre = NULL;

	while (pi != NULL) {
		int nfree = mem_find_continuous_pages(pi, npages);

		KERN_ASSERT(nfree >= 1);

		if (nfree == npages)
			break;

		pre = &pi[nfree-1];
		pi = pi[nfree-1].free_next;
	}

	if (pi == NULL)
		goto ret;

	if (pre == NULL)
		mem_freepage = pi[npages-1].free_next;
	else
		pre->free_next = pi[npages-1].free_next;

	for (i = 0; i < npages; i++) {
		pi[i].free_next = NULL;
		mem_incref(&pi[i]);
	}

 ret:
	spinlock_release(&mem_lock);

	return pi;
}

void
mem_page_free(pageinfo_t *pi)
{
	spinlock_acquire(&mem_lock);

	KERN_ASSERT(pi != NULL);
	KERN_ASSERT((uintptr_t) pi >= (uintptr_t) &mem_pageinfo[0] &&
		    (uintptr_t) pi < (uintptr_t) &mem_pageinfo[mem_npages]);
	KERN_ASSERT(pi->refcount == 0 && pi->free_next == NULL);

	pi->free_next = mem_freepage;
	mem_freepage = pi;

	spinlock_release(&mem_lock);
}

void
mem_incref(pageinfo_t *pi)
{
	KERN_ASSERT((uintptr_t) pi >= (uintptr_t) &mem_pageinfo[0] &&
		    (uintptr_t) pi < (uintptr_t) &mem_pageinfo[mem_npages]);
	lockadd(&pi->refcount, 1);
}

void
mem_decref(pageinfo_t *pi)
{
	KERN_ASSERT((uintptr_t) pi >= (uintptr_t) &mem_pageinfo[0] &&
		    (uintptr_t) pi < (uintptr_t) &mem_pageinfo[mem_npages]);
	KERN_ASSERT(pi->refcount > 0);
	lockadd(&pi->refcount, -1);
}

uintptr_t
mem_pi2phys(pageinfo_t *pi)
{
	return ((pi - mem_pageinfo) * PAGE_SIZE);
}

pageinfo_t *
mem_phys2pi(uintptr_t phys)
{
	return &mem_pageinfo[phys / PAGE_SIZE];
}

void *
mem_pi2ptr(pageinfo_t *pi)
{
	KERN_ASSERT(pi != NULL);

	return ((void *) mem_pi2phys(pi));
}

pageinfo_t *
mem_ptr2pi(void *ptr)
{
	KERN_ASSERT(ptr != NULL);

	return mem_phys2pi((uintptr_t) ptr);
}

size_t
mem_max_phys(void)
{
	return mem_max;
}

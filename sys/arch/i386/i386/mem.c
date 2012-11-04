#include <sys/debug.h>
#include <sys/mboot.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/mem.h>

/*
 * Data structures and variables for BIOS E820 table.
 */

typedef
struct pmmap_t {
	uintptr_t	start;
	uintptr_t	end;
	uint32_t	type;
	struct pmmap_t	*next;
	struct pmmap_t	*type_next;
} pmmap_t;

static pmmap_t pmmap[128];
static pmmap_t *pmmap_usable, *pmmap_resv, *pmmap_acpi, *pmmap_nvs;

static int __free_pmmap_ent_idx = 0;

/*
 * Data structures and variables for the physical memory allocator.
 */

static bool mem_inited = FALSE;	/* is the memory allocator initialized? */
static spinlock_t mem_lock;	/* lock of the memory allocator */
static struct page_info mem_all_pages[1<<20];/* all memory pages (all types) */
static size_t mem_npages;	/* the amount of all memory pages */
static size_t mem_nfreepages;
static struct page_info *mem_free_pages;

/*
 * Converters between page indices and physical address/pointers.
 */

/* page index --> physical address */
uintptr_t
mem_pi2phys(struct page_info *pi)
{
	return (pi - mem_all_pages) * PAGESIZE;
}

/* physical address --> page index */
struct page_info *
mem_phys2pi(uintptr_t pa)
{
	return &mem_all_pages[pa / PAGESIZE];
}

/* page index --> pointer */
void *
mem_pi2ptr(struct page_info *pi)
{
	return (void *) mem_pi2phys(pi);
}

/* pointer --> page index */
struct page_info *
mem_ptr2pi(void *ptr)
{
	return mem_phys2pi((uintptr_t) ptr);
}

#define mem_page_index(pi)				\
	((struct page_info *) (pi) - mem_all_pages)

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
	pmmap_t *ent, *last_mem_ent = NULL;

	for (ent = pmmap; ent != NULL && ent->next != NULL; ent = ent->next)
		if (ent->type == MEM_RAM)
			last_mem_ent = ent;

	if (last_mem_ent == NULL)
		return 0;
	else
		return last_mem_ent->end;
}

static gcc_inline int
mem_in_reserved_page(uintptr_t addr)
{
	return (addr < (uintptr_t)
		ROUNDUP((uintptr_t) (mem_all_pages + mem_npages), PAGESIZE)) ||
		addr >= 0xf0000000;
}

static void
mem_test(void)
{
	if (mem_inited == FALSE || pcpu_onboot() == FALSE)
		return;

	struct page_info *pi0, *pi1, *pi2;

	pi0 = mem_page_alloc();
	KERN_ASSERT(pi0 != NULL);
	mem_pages_free(pi0);

	pi1 = mem_page_alloc();
	KERN_DEBUG("pi0 0x%08x, pi1 0x%08x.\n", pi0, pi1);
	KERN_ASSERT(pi1 != NULL && pi0 == pi1);
	mem_pages_free(pi1);

	pi0 = mem_pages_alloc(11 * PAGESIZE);
	KERN_ASSERT(pi0 != NULL);
	mem_pages_free(pi0);

	pi1 = mem_pages_alloc(12 * PAGESIZE);
	KERN_DEBUG("pi0 0x%08x, pi1 0x%08x.\n", pi0, pi1);
	KERN_ASSERT(pi1 != NULL && pi0 == pi1);
	mem_pages_free(pi1);

	pi0 = mem_page_alloc();
	pi2 = mem_page_alloc();
	mem_page_free(pi0);
	pi1 = mem_page_alloc();
	KERN_DEBUG("pi0 0x%08x, pi1 0x%08x, pi2 0x%08x.\n", pi0, pi1, pi2);
	KERN_ASSERT(pi0 == pi1);
	mem_page_free(pi1);
	mem_page_free(pi2);

	KERN_DEBUG("mem_test() succeeds.\n");
}

/*
 * Initialize the physical memory allocator.
 */
void
mem_init(mboot_info_t *mbi)
{
	extern uint8_t end[];

	pmmap_t *e820_entry;
	struct page_info *last_free;

	if (mem_inited == TRUE || pcpu_onboot() == FALSE)
		return;

	/* get a usable physical memory map */
	pmmap_init(mbi);

	/* reserve memory for mem_npage pageinfo_t structures */
	mem_npages = ROUNDDOWN(pmmap_max(), PAGESIZE) / PAGESIZE;
	memzero(mem_all_pages, sizeof(struct page_info) * mem_npages);

	mem_free_pages = NULL;
	last_free = NULL;

	mem_nfreepages = 0;

	/*
	 * Initialize page info structures.
	 */
	for (e820_entry = pmmap;
	     e820_entry != NULL && e820_entry->end <= pmmap_max();
	     e820_entry = e820_entry->next) {
		uintptr_t lo, hi, i;
		pg_type type;
		struct page_info *pi;

		lo = (uintptr_t) ROUNDUP(e820_entry->start, PAGESIZE);
		hi = (uintptr_t) ROUNDDOWN(e820_entry->end, PAGESIZE);

		if (lo >= hi)
			continue;

		type = e820_entry->type;

		for (i = lo, pi = mem_phys2pi(lo); i < hi; i+= PAGESIZE, pi++) {
			pi->type = (type != MEM_RAM) ? PG_RESERVED :
				(i < (uintptr_t) end) ? PG_KERNEL : PG_NORMAL;

			if (pi->type == PG_NORMAL) {
				if (unlikely(mem_free_pages == NULL))
					mem_free_pages = pi;
				if (last_free != NULL) {
					last_free->next = pi;
					pi->prev = last_free;
				}
				last_free = pi;
				mem_nfreepages++;
			}
		}
	}

	spinlock_init(&mem_lock);
	mem_inited = TRUE;

	KERN_INFO("Total physical memory %u bytes, %u/%u pages.\n",
		  mem_npages * PAGESIZE, mem_nfreepages, mem_npages);

	mem_test();
}

static gcc_inline int
mem_next_npages_free(struct page_info *page, int npages)
{
	KERN_ASSERT(page != NULL && page->used == 0);
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);
	KERN_ASSERT(npages > 0);

	int n = 1;
	struct page_info *pi, *prev_pi;

	prev_pi = page;
	pi = page->next;

	while (pi && n < npages) {
		KERN_ASSERT(pi->used == 0);

		if (pi - prev_pi == 1)
			n++;
		else
			break;

		prev_pi = pi;
		pi = pi->next;
	}

	return n;
}

/*
 * Allocate n pages of which the lowest address is aligned to 2^p pages.
 */
struct page_info *
mem_pages_alloc_align(size_t n, int p)
{
	KERN_ASSERT(0 < n && n < (1 << 20));
	KERN_ASSERT(0 <= p && p < 20);

	struct page_info *page;
	int aligned_to, offset, i;

	aligned_to = (1 << p);

	spinlock_acquire(&mem_lock);

	page = mem_free_pages;

	while (page) {
		if (mem_page_index(page) % aligned_to != 0) {
			page = page->next;
			continue;
		}

		offset = mem_next_npages_free(page, n);
		if (offset == n)
			break;
		else
			page = page[offset-1].next;
	}

	if (page == NULL) {
		KERN_DEBUG("Cannot find free memory page, %u free pages.\n",
			   mem_nfreepages);
		goto ret;
	}

	if (mem_free_pages->prev)
		mem_free_pages->prev->next = page[n-1].next;
	if (page[n-1].next)
		page[n-1].next->prev = page->prev;
	if (mem_free_pages == page) {
		/* KERN_DEBUG("Move mem_free_pages from 0x%08x to 0x%08x.\n", */
		/* 	   page, page[n-1].next); */
		mem_free_pages = page[n-1].next;
	}
	page->prev = page[n-1].next = NULL;

	for (i = 0; i < n; i++) {
		page[i].used = 1;
		page[i].refcount = 0;
	}

 ret:
	if (page != NULL) {
		KERN_ASSERT(mem_nfreepages >= n);
		mem_nfreepages -= n;
		/* KERN_DEBUG("%d pages allocated.\n", n); */
	}

	spinlock_release(&mem_lock);

	return page;
}

/*
 * Free memory pages.
 */
struct page_info *
mem_pages_free(struct page_info *page)
{
	KERN_ASSERT(page != NULL);
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);

	struct page_info *pi, *prev_free_pi, *next_free_pi;
	int i, n;

	spinlock_acquire(&mem_lock);

	prev_free_pi = NULL;
	next_free_pi = mem_free_pages;

	/* search the previous and next free pages */
	for (i = mem_page_index(page); i > 0; i--)
		if (mem_all_pages[i-1].type == PG_NORMAL &&
		    mem_all_pages[i-1].used == 0) {
			prev_free_pi = &mem_all_pages[i-1];
			break;
		}
	if (prev_free_pi) {
		/* KERN_DEBUG("Found previous free page 0x%08x.\n", prev_free_pi); */
		next_free_pi = prev_free_pi->next;
	}

	/* KERN_DEBUG("Next free page 0x%08x.\n", next_free_pi); */

	/* link the pages together */

	if (prev_free_pi)
		prev_free_pi->next = page;
	page->prev = prev_free_pi;

	n = 0;
	for (pi = page; pi != NULL; pi = pi->next) {
		KERN_ASSERT(pi->used != 0);
		KERN_ASSERT(pi->refcount == 0);
		pi->used = 0;
		n++;
		if (pi->next == NULL)
			break;
	}

	if (next_free_pi)
		next_free_pi->prev = pi;
	pi->next = next_free_pi;

	if (mem_free_pages == next_free_pi) {
		/* KERN_DEBUG("Move mem_free_pages from 0x%08x to 0x%08x.\n", */
		/* 	   mem_free_pages, page); */
		mem_free_pages = page;
	}

	mem_nfreepages += n;
	/* KERN_DEBUG("%d pages freed.\n", n); */

	spinlock_release(&mem_lock);

	return page;
}

void
mem_incref(struct page_info *page)
{
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);
	spinlock_acquire(&mem_lock);
	page->refcount++;
	spinlock_release(&mem_lock);
}

void
mem_decref(struct page_info *page)
{
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);
	spinlock_acquire(&mem_lock);
	if (page->refcount)
		page->refcount--;
	if (page->refcount == 0)
		mem_page_free(page);
	spinlock_release(&mem_lock);
}

size_t
mem_max_phys(void)
{
	return mem_npages * PAGESIZE;
}

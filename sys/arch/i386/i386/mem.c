#include <sys/debug.h>
#include <sys/mboot.h>
#include <sys/pcpu.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

#include <machine/mem.h>

struct page_info {
	int			used;
	int			npages_alloc;
	int			refcount;
	struct page_info	*last_free_page, *next_free_page;
};

/* Is the memory module initialized? */
static bool mem_inited = FALSE;

/* Should be acquired before any memory operations */
static spinlock_t mem_lock;

/* information for all memory pages */
struct page_info *mem_all_pages;

/* information for all free memory pages */
struct page_info *mem_all_free_pages;

/* maximum physical address in byte */
static size_t mem_max;

/* amount of physical memory pages */
static size_t mem_npages;

static int __free_pmmap_ent_idx = 0;

uintptr_t
mem_pi2phys(struct page_info *pi)
{
	return (pi - mem_all_pages) * PAGESIZE;
}

struct page_info *
mem_phys2pi(uintptr_t pa)
{
	return &mem_all_pages[pa / PAGESIZE];
}

void *
mem_pi2ptr(struct page_info *pi)
{
	return (void *) mem_pi2phys(pi);
}

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

static gcc_inline int
mem_in_reserved_page(uintptr_t addr)
{
	return (addr < (uintptr_t)
		ROUNDUP((uintptr_t) (mem_all_pages + mem_npages), PAGESIZE)) ||
		addr >= 0xf0000000;
}

/*
 * Initialize the physical memory allocator.
 */
void
mem_init(mboot_info_t *mbi)
{
	extern uint8_t end[];

	pmmap_t *e820_entry;
	struct page_info *last_free_page;
	size_t nfreepages;

	if (mem_inited == TRUE || pcpu_onboot() == FALSE)
		return;

	/* get a usable physical memory map */
	pmmap_init(mbi);

	/*
	 * get the maximum address and the amounts of memory pages, including
	 * all available pages and reserved pages
	 */
	mem_max = pmmap_max();
	mem_npages = pmmap_max() / PAGE_SIZE;

	KERN_INFO("\n\tUsable memory %u bytes, %u pages.\n",
		   mem_max, mem_npages);

	/* reserve memory for mem_npage pageinfo_t structures */
	mem_all_pages = (struct page_info *) ROUNDUP((uintptr_t)end,
						     sizeof(struct page_info));
	memzero(mem_all_pages, sizeof(struct page_info) * mem_npages);

	/*
	 * Initialize the free pages list.
	 *
	 * - all pages below 1MB (used by BIOS) are reserved,
	 * - all pages below end (used by the kernel code) are reserved,
	 * - all pages for mem_all_pages are reserved,
	 * - all pages above 0xf0000000 (used by devices) are reserved, and
	 * - all pages not in MEM_RAM e820 entries are reserved.
	 */
	for (e820_entry = pmmap, last_free_page = NULL, nfreepages = 0;
	     e820_entry != NULL;
	     e820_entry = e820_entry->next) {
		uintptr_t lo, hi, i;
		struct page_info *free_page;

		if (e820_entry->type != MEM_RAM)
			continue;

		lo = (uintptr_t) ROUNDUP(e820_entry->start, PAGESIZE);
		hi = (uintptr_t) ROUNDDOWN(e820_entry->end, PAGESIZE);

		if (lo >= hi)
			continue;

		for (i = lo; i < hi; i+= PAGESIZE) {
			if (mem_in_reserved_page(i))
				continue;

			free_page = mem_phys2pi(i);
			free_page->used = 0;
			free_page->refcount = 0;
			free_page->npages_alloc = 0;
			free_page->last_free_page = last_free_page;
			free_page->next_free_page = NULL;

			if (likely(last_free_page != NULL))
				last_free_page->next_free_page = free_page;
			else
				mem_all_free_pages = free_page;

			last_free_page = free_page;
			nfreepages++;
		}
	}

	spinlock_init(&mem_lock);
}

static gcc_inline int
mem_next_npages_free(size_t page, size_t n)
{
	while (n)
		if (mem_all_pages[page].used != 0) {
			return 0;
		} else {
			page++;
			n--;
		}
	return 1;
}

/*
 * Allocate n pages of which the lowest address is aligned to 2^p pages.
 */
struct page_info *
mem_pages_alloc_align(size_t n, int p)
{
	struct page_info *page;
	size_t aligned_to, i;

	if (n > (1 <<20) || p >= 20)
		return NULL;

	aligned_to = (1 << p);

	spinlock_acquire(&mem_lock);

	for (page = mem_all_free_pages;
	     page != NULL; page = page->next_free_page) {
		size_t page_idx = mem_page_index(page);

		if (page_idx % aligned_to != 0)
			continue;

		if (mem_next_npages_free(page_idx, n))
			break;
	}

	if (page == NULL) {
		spinlock_release(&mem_lock);
		return NULL;
	}

	if (page == mem_all_free_pages)
		mem_all_free_pages = page[n-1].next_free_page;

	if (page->last_free_page != NULL)
		page->last_free_page->next_free_page = page[n-1].next_free_page;

	if (page[n-1].next_free_page != NULL)
		page[n-1].next_free_page->last_free_page = page->last_free_page;

	for (i = 0; i < n; i++) {
		page[i].used = 1;
		page[i].npages_alloc = (i == 0) ? n : 0;
		page[i].last_free_page = NULL;
		page[i].next_free_page = NULL;
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
	struct page_info *last_free_page;
	size_t page_idx, npages, i;

	spinlock_acquire(&mem_lock);

	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);
	npages = page->npages_alloc;
	KERN_ASSERT(npages > 0);
	page_idx = mem_page_index(page);

	i = page_idx;
	while (i)
		if (mem_all_pages[i].used == 0)
			break;
		else
			i--;
	if (i > 0)
		last_free_page = &mem_all_pages[i];
	else if (i == 0 && mem_all_pages[i].used == 0)
		last_free_page = &mem_all_pages[0];
	else
		last_free_page = NULL;

	if (last_free_page != NULL) {
		page[npages-1].next_free_page = last_free_page->next_free_page;
		last_free_page->next_free_page->last_free_page = &page[npages-1];
	} else {
		page[npages-1].next_free_page = mem_all_free_pages;
		if (mem_all_free_pages != NULL)
			mem_all_free_pages->last_free_page = &page[npages-1];
		mem_all_free_pages = page;
	}

	for (i = 0; i < npages; i++) {
		page[i].used = 0;
		page[i].last_free_page = last_free_page;
		if (last_free_page != NULL)
			last_free_page->next_free_page = &page[i];
		last_free_page = &page[i];
	}

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
	spinlock_release(&mem_lock);
}

size_t
mem_max_phys(void)
{
	return mem_max;
}

#include <sys/debug.h>
#include <sys/mboot.h>
#include <sys/mem.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/x86.h>

/*
 * Data structures and variables for BIOS E820 table.
 */

struct pmmap {
	uintptr_t		start;
	uintptr_t		end;
	uint32_t		type;
	SLIST_ENTRY(pmmap)	next;
	SLIST_ENTRY(pmmap)	type_next;
};

static struct pmmap pmmap_slots[128];
static int pmmap_slots_next_free = 0;

static SLIST_HEAD(, pmmap) pmmap_list;	/* all memory regions */
static SLIST_HEAD(, pmmap) pmmap_sublist[4];

enum __pmmap_type { PMMAP_USABLE, PMMAP_RESV, PMMAP_ACPI, PMMAP_NVS };

#define PMMAP_SUBLIST_NR(type)				\
	(((type) == MEM_RAM) ? PMMAP_USABLE :		\
	 ((type) == MEM_RESERVED) ? PMMAP_RESV :	\
	 ((type) == MEM_ACPI) ? PMMAP_ACPI :		\
	 ((type) == MEM_NVS) ? PMMAP_NVS : -1)

static uintptr_t max_usable_memory = 0;

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

struct pmmap *
pmmap_alloc_slot(void)
{
	if (unlikely(pmmap_slots_next_free == 128))
		return NULL;
	return &pmmap_slots[pmmap_slots_next_free++];
}

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
	struct pmmap *free_slot, *slot, *last_slot;

	if ((free_slot = pmmap_alloc_slot()) == NULL)
		KERN_PANIC("More than 128 E820 entries.\n");

	free_slot->start = start;
	free_slot->end = end;
	free_slot->type = type;

	last_slot = NULL;

	SLIST_FOREACH(slot, &pmmap_list, next) {
		if (start < slot->start)
			break;
		last_slot = slot;
	}

	if (last_slot == NULL)
		SLIST_INSERT_HEAD(&pmmap_list, free_slot, next);
	else
		SLIST_INSERT_AFTER(last_slot, free_slot, next);
}

static void
pmmap_merge(void)
{
	struct pmmap *slot, *next_slot;
	struct pmmap *last_slot[4] = { NULL, NULL, NULL, NULL };
	int sublist_nr;

	/*
	 * Step 1: Merge overlaped entries in pmmap_list.
	 */
	SLIST_FOREACH(slot, &pmmap_list, next) {
		if ((next_slot = SLIST_NEXT(slot, next)) == NULL)
			break;
		if (slot->start <= next_slot->start &&
		    slot->end >= next_slot->start &&
		    slot->type == next_slot->type) {
			slot->end = MAX(slot->end, next_slot->end);
			SLIST_REMOVE_AFTER(slot, next);
		}
	}

	/*
	 * Step 2: Create the specfic lists: pmmap_usable, pmmap_resv,
	 *         pmmap_acpi, pmmap_nvs.
	 */
	SLIST_FOREACH(slot, &pmmap_list, next) {
		sublist_nr = PMMAP_SUBLIST_NR(slot->type);
		KERN_ASSERT(sublist_nr != -1);
		if (last_slot[sublist_nr] != NULL)
			SLIST_INSERT_AFTER(last_slot[sublist_nr], slot,
					   type_next);
		else
			SLIST_INSERT_HEAD(&pmmap_sublist[sublist_nr], slot,
					  type_next);
		last_slot[sublist_nr] = slot;
	}

	if (last_slot[PMMAP_USABLE] != NULL)
		max_usable_memory = last_slot[PMMAP_USABLE]->end;
}

static void
pmmap_dump(void)
{
	struct pmmap *slot;
	SLIST_FOREACH(slot, &pmmap_list, next) {
		KERN_INFO("BIOS-e820: 0x%08x - 0x%08x (%s)\n",
			  slot->start,
			  (slot->start == slot->end) ? slot->end : slot->end-1,
			  (slot->type == MEM_RAM) ? "usable" :
			  (slot->type == MEM_RESERVED) ? "reserved" :
			  (slot->type == MEM_ACPI) ? "ACPI data" :
			  (slot->type == MEM_NVS) ? "ACPI NVS" :
			  "unknown");
	}
}

static void
pmmap_init(mboot_info_t *mbi)
{
	KERN_INFO("\n");

	mboot_mmap_t *p = (mboot_mmap_t *) mbi->mmap_addr;

	SLIST_INIT(&pmmap_list);
	SLIST_INIT(&pmmap_sublist[PMMAP_USABLE]);
	SLIST_INIT(&pmmap_sublist[PMMAP_RESV]);
	SLIST_INIT(&pmmap_sublist[PMMAP_ACPI]);
	SLIST_INIT(&pmmap_sublist[PMMAP_NVS]);

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
	if (mem_inited == FALSE)
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

	struct pmmap *slot;
	struct page_info *last_free = NULL;

	if (mem_inited == TRUE)
		return;

	/* get a usable physical memory map */
	pmmap_init(mbi);

	/* reserve memory for mem_npage pageinfo_t structures */
	mem_npages = ROUNDDOWN(max_usable_memory, PAGESIZE) / PAGESIZE;
	memzero(mem_all_pages, sizeof(struct page_info) * mem_npages);

	mem_free_pages = NULL;
	last_free = NULL;

	mem_nfreepages = 0;

	/*
	 * Initialize page info structures.
	 */
	memzero(mem_all_pages, sizeof(struct page_info) * (1 << 20));
	SLIST_FOREACH(slot, &pmmap_sublist[PMMAP_USABLE], type_next) {
		uintptr_t lo, hi, addr;
		pg_type type;
		struct page_info *pi;

		lo = (uintptr_t) ROUNDUP(slot->start, PAGESIZE);
		hi = (uintptr_t) ROUNDDOWN(slot->end, PAGESIZE);

		if (lo >= hi)
			continue;

		type = slot->type;

		for (addr = lo, pi = mem_phys2pi(lo); addr < hi;
		     addr += PAGESIZE, pi++) {
			pi->type = (type != MEM_RAM) ? PG_RESERVED :
				(addr < (uintptr_t) end) ? PG_KERNEL : PG_NORMAL;

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

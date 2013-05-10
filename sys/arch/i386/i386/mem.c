#include <sys/debug.h>
#include <sys/mboot.h>
#include <sys/queue.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>
#include <sys/vm.h>
#include <sys/x86.h>

#include <machine/mem.h>

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
static struct page_info mem_all_pages[1<<20];/* all memory pages (all types) */
static size_t mem_npages;	/* the amount of all memory pages */

/*
 * The physical memory are separated to the normal memory region and the high
 * memory region. The normal memory region is below VM_HIMEM and is
 * identically mapped in the kernel page tables. The high memory region is above
 * VM_HIMEM and is mapped to the virtual address space between VM_DYNAMICLO and
 * VM_DYNMAICHI on demand.
 */

static struct page_info *mem_free_normal_pages;
static spinlock_t mem_normal_pages_lk;
static size_t mem_nr_free_normal_pages;

static struct page_info *mem_free_himem_pages;
static spinlock_t mem_himem_pages_lk;
static size_t mem_nr_free_himem_pages;

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
	struct page_info *last_free_normal = NULL, *last_free_himem = NULL;

	if (mem_inited == TRUE)
		return;

	/* get a usable physical memory map */
	pmmap_init(mbi);

	mem_npages = ROUNDDOWN(max_usable_memory, PAGESIZE) / PAGESIZE;
	mem_free_normal_pages = mem_free_himem_pages = NULL;
	mem_nr_free_normal_pages = mem_nr_free_himem_pages = 0;

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
				(addr < (uintptr_t) end) ? PG_KERNEL :
				(addr < VM_HIGHMEMLO) ? PG_NORMAL : PG_HIGH;

			if (pi->type == PG_NORMAL) {
				if (unlikely(mem_free_normal_pages == NULL))
					mem_free_normal_pages = pi;
				if (last_free_normal != NULL) {
					last_free_normal->next = pi;
					pi->prev = last_free_normal;
				}
				last_free_normal = pi;
				mem_nr_free_normal_pages++;
			} else if (pi->type == PG_HIGH) {
				if (unlikely(mem_free_himem_pages == NULL))
					mem_free_himem_pages = pi;
				if (last_free_himem != NULL) {
					last_free_himem->next = pi;
					pi->prev = last_free_himem;
				}
				last_free_himem = pi;
				mem_nr_free_himem_pages++;
			}
		}
	}

	spinlock_init(&mem_normal_pages_lk);
	spinlock_init(&mem_himem_pages_lk);

	mem_inited = TRUE;

	KERN_INFO("Total physical memory %u bytes (%u pages of normal memory, "
		  "%u pages of high memory, %u pages in total).\n",
		  max_usable_memory, mem_nr_free_normal_pages,
		  mem_nr_free_himem_pages, mem_npages);

	mem_test();
}

/*
 * Check whether there are a number consecutively free physcal memory pages
 * starting from a specific page in the same memory region (normal or high).
 *
 * @param page   the start page
 * @param npages the number or the consecutively free pages
 * @param high   TRUE - search in the high memory region,
 *               FALSE - search in the normal memory region
 *
 * @return npages if there are npages pages of consecutively free pages from
 *         page; otherwise, return the number of consecutively free pages
 *         starting from page.
 */
static gcc_inline int
mem_next_npages_free(struct page_info *page, size_t npages)
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

static struct page_info *
mem_pages_alloc_align_region(size_t n, int p, bool high)
{
	struct page_info **free_pages_ptr;
	spinlock_t *lock;
	size_t *nr_free_pages_ptr;

	struct page_info *page;
	int aligned_to, offset, i;

	if (high == TRUE) {
		free_pages_ptr = &mem_free_himem_pages;
		lock = &mem_himem_pages_lk;
		nr_free_pages_ptr = &mem_nr_free_himem_pages;
	} else {
		free_pages_ptr = &mem_free_normal_pages;
		lock = &mem_normal_pages_lk;
		nr_free_pages_ptr = &mem_nr_free_normal_pages;
	}

	aligned_to = (1 << p);

	spinlock_acquire(lock);

	page = *free_pages_ptr;

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
		if (high == FALSE)
			KERN_DEBUG("Cannot find free %smemory page, "
				   "%u free pages.\n",
				   (high == TRUE) ? "high" : "normal",
				   *nr_free_pages_ptr);
		goto ret;
	}

	if ((*free_pages_ptr)->prev)
		(*free_pages_ptr)->prev->next = page[n-1].next;
	if (page[n-1].next)
		page[n-1].next->prev = page->prev;
	if (*free_pages_ptr == page) {
		/* KERN_DEBUG("Move mem_free_pages from 0x%08x to 0x%08x.\n", */
		/* 	   page, page[n-1].next); */
		*free_pages_ptr = page[n-1].next;
	}
	page->prev = page[n-1].next = NULL;

	for (i = 0; i < n; i++) {
		page[i].used = 1;
		page[i].refcount = 0;
	}

 ret:
	if (page != NULL) {
		KERN_ASSERT(*nr_free_pages_ptr >= n);
		*nr_free_pages_ptr -= n;
		/* KERN_DEBUG("%d pages allocated.\n", n); */
	}

	spinlock_release(lock);

	return page;
}

/*
 * Allocate n pages of which the lowest address is aligned to 2^p pages.
 */
struct page_info *
mem_pages_alloc_align(size_t n, int p)
{
	return mem_pages_alloc_align_region(n, p, FALSE);
}

struct page_info *
himem_pages_alloc_align(size_t n, int p)
{
	struct page_info *page = mem_pages_alloc_align_region(n, p, TRUE);
	return (page == NULL) ?
		mem_pages_alloc_align_region(n, p, FALSE) : page;
}

/*
 * Free memory pages.
 */
void
mem_pages_free(struct page_info *page)
{
	KERN_ASSERT(page != NULL);
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);

	struct page_info **free_pages_ptr;
	pg_type type;
	size_t *nr_free_pages_ptr;
	spinlock_t *lock;

	struct page_info *pi, *prev_free_pi, *next_free_pi;
	int i, n;

	if (page->type == PG_NORMAL) {
		free_pages_ptr = &mem_free_normal_pages;
		type = PG_NORMAL;
		nr_free_pages_ptr = &mem_nr_free_normal_pages;
		lock = &mem_normal_pages_lk;
	} else if (page->type == PG_HIGH) {
		free_pages_ptr = &mem_free_himem_pages;
		type = PG_HIGH;
		nr_free_pages_ptr = &mem_nr_free_himem_pages;
		lock = &mem_himem_pages_lk;
	} else {
		KERN_PANIC("Page %u is neither a normal memory page nor "
			   "a high memory page.\n", mem_page_index(page));
		return;
	}

	spinlock_acquire(lock);

	prev_free_pi = NULL;
	next_free_pi = *free_pages_ptr;

	/* search the previous and next free pages */
	for (i = mem_page_index(page); i > 0; i--)
		if (mem_all_pages[i-1].type == type &&
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

	if (*free_pages_ptr == next_free_pi) {
		/* KERN_DEBUG("Move mem_free_pages from 0x%08x to 0x%08x.\n", */
		/* 	   mem_free_pages, page); */
		*free_pages_ptr = page;
	}

	*nr_free_pages_ptr += n;
	/* KERN_DEBUG("%d pages freed.\n", n); */

	spinlock_release(lock);
}

void
mem_incref(struct page_info *page)
{
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);
	KERN_ASSERT(page->type == PG_NORMAL || page->type == PG_HIGH);
	spinlock_t *lock = (page->type == PG_NORMAL) ?
		&mem_normal_pages_lk : &mem_himem_pages_lk;
	spinlock_acquire(lock);
	page->refcount++;
	spinlock_release(lock);
}

void
mem_decref(struct page_info *page)
{
	KERN_ASSERT(mem_all_pages <= page && page < mem_all_pages + mem_npages);
	KERN_ASSERT(page->type == PG_NORMAL || page->type == PG_HIGH);
	spinlock_t *lock = (page->type == PG_NORMAL) ?
		&mem_normal_pages_lk : &mem_himem_pages_lk;
	spinlock_acquire(lock);
	if (page->refcount)
		page->refcount--;
	if (page->refcount == 0)
		mem_page_free(page);
	spinlock_release(lock);
}

size_t
mem_max_phys(void)
{
	return mem_npages * PAGESIZE;
}

// Physical memory management.
// See COPYRIGHT for copyright information.

#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <architecture/mem.h>
#include <architecture/spinlock.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>
#include <kern/mem/mem.h>
#include <kern/mem/e820.h>
#include <inc/multiboot.h>



// These special symbols mark the start and end of
// the program's entire linker-arranged memory region,
// including the program's code, data, and bss sections.
// Use these to avoid treating kernel code/data pages as free memory!
extern char start[], end[];


size_t mem_max;			// Maximum physical address
size_t mem_npage;		// Total number of physical memory pages

pageinfo *mem_pageinfo;		// Metadata array indexed by page number
pageinfo *mem_freelist;		// Start of free page list

static spinlock memlock;
static bool mem_inited;

static struct pmem_layout pml;

void mem_check(void);

extern struct pmem_layout * get_pmem_layout();
struct pmem_layout * get_pmem_layout(){
	return &pml;
};



void
mem_init(const struct multiboot_info *mbi)
{
	// Do mem_init only once
	assert(!mem_inited);
	mem_inited = true;

	spinlock_init(&memlock);

	//Parse the command line that user pass to GRUB
	if ( ( mbi->flags & MBI_CMDLINE ) && ( mbi->cmdline != 0 ) )
	{
		char *cmdline = (char*)(unsigned long)mbi->cmdline;
		cprintf ("Command line passed to CertiKOS: %s\n", cmdline );
		cprintf ("Memory map size is %x\n", mbi->mmap_length );
	}

	//Set up memory layout and store the layout info in pml
	setup_memory_region (&(pml.e820), mbi);
	pml.total_pages  = get_nr_pages ( &(pml.e820) );
	pml.max_page     = get_max_pfn ( &(pml.e820) );
	pml.vmm_pmem_end= (unsigned long )end;
	pml.vmm_pmem_start = DEFAULT_VMM_PMEM_START;

	// Memory detection in PIOS
	// Determine how much base (<640K) and extended (>1MB) memory
	// is available in the system (in bytes),
	// by reading the PC's BIOS-managed nonvolatile RAM (NVRAM).
	// The NVRAM tells us how many kilobytes there are.
	// Since the count is 16 bits, this gives us up to 64MB of RAM;
	// additional RAM beyond that would have to be detected another way.

	/*size_t basemem = mem_base();
	size_t extmem = mem_ext();


	// The maximum physical address is the top of extended memory.
	//mem_max = MEM_EXT + extmem;
	*/

	cprintf("pml.vmm_pmem_start: %x\n",pml.vmm_pmem_start);
	cprintf("pml.max_page: %x\n",pml.max_page);

	// Compute the total number of physical pages (including I/O holes)
	mem_npage = pml.max_page > (0x10000000>>PAGE_SHIFT) ?
		(0x10000000>>PAGE_SHIFT) : pml.max_page;
	mem_max = mem_npage*PAGESIZE;

	// Now that we know the size of physical memory,
	// reserve enough space for the pageinfo array
	// just past our statically-assigned program code/data/bss,
	// which the linker placed at the start of extended memory.
	// Make sure the pageinfo entries are naturally aligned.
	//cprintf("end is %x\n",end);
	mem_pageinfo = (pageinfo *) ROUNDUP((size_t) end, sizeof(pageinfo));

	// Initialize the entire pageinfo array to zero for good measure.
	memset(mem_pageinfo, 0, sizeof(pageinfo) * mem_npage);

	// Free extended memory starts just past the pageinfo array.
	void *freemem = &mem_pageinfo[mem_npage];

	// Align freemem to page boundary.
	freemem = ROUNDUP(freemem, PAGESIZE);
	// Update the physical memory layout record
	//allocate the CertiKOS heap space: from pml->vmm_heap_start to lower 128+64M
	pml.vmm_heap_start=mem_phys(freemem)/PAGESIZE;

	cprintf("\n++++++ CertiKOS physical memory map\n");
	cprintf("CertiKOS start at: %x\n", pml.vmm_pmem_start);
	cprintf("CertiKOS heap start at: %x\n", pml.vmm_heap_start*PAGESIZE);
	cprintf("CertiKOS Image end at: %x\n", pml.vmm_pmem_end);

	pageinfo **freetail = &mem_freelist;
	int i;
	int count=0;
	for (i = 0; i < mem_npage; i++) {
		// Off-limits until proven otherwise.
		int inuse = 1;

		// The bottom basemem bytes are free except page 0 and 1.
	//do not use the lower 640k space
	//	if (i > 1 && i < basemem / PAGESIZE)
	//		inuse = 0;

		// The IO hole and the kernel abut.

		// The memory past the kernel is free.
		if (i >= mem_phys(freemem) / PAGESIZE)
			inuse = 0;

		mem_pageinfo[i].refcount = inuse;
		if (!inuse) {
			// Add the page to the end of the free list.
			*freetail = &mem_pageinfo[i];
			freetail = &mem_pageinfo[i].free_next;
			count ++;
		}
	}
	*freetail = NULL;	// null-terminate the freelist
	cprintf("finish memory\n");
	// Check to make sure the page allocator seems to work correctly.
	mem_check();
}



//
// Allocates a physical page from the page free list.
// Does NOT set the contents of the physical page to zero -
// the caller must do that if necessary.
//
// RETURNS
//   - a pointer to the page's pageinfo struct if successful
//   - NULL if no available physical pages.
//
// Hint: pi->refs should not be incremented
// Hint: be sure to use proper mutual exclusion for multiprocessor operation.
pageinfo *
mem_alloc(void)
{
	assert(mem_inited);

	spinlock_acquire(&memlock);

	pageinfo *pi = mem_freelist;
	if (pi != NULL) {
		mem_freelist = pi->free_next;	// Remove page from free list
		pi->free_next = NULL;		// Mark it not on the free list
	}

	spinlock_release(&memlock);

	//update the alloc bitmap for vmm of certikos
//	map_alloc_record(pi);
	return pi;	// Return pageinfo pointer or NULL

}

//get the n th	page after the pi in the freelist
pageinfo *
next_n_page_in_freelist(pageinfo *pi, unsigned long n){
	unsigned long i;
	pageinfo *t_pi=pi;
	for (i=0;i<=n;i++){
		if (t_pi!=NULL){
		t_pi=t_pi->free_next;
		}else{
		return NULL;
		}
	}
	return t_pi;
}

// return: &mem_freelist,
//	   no prior, pi is the head of the list
//	   NULL, pi is not in the free list
//	   others, pageinfo index
pageinfo * find_prior_pi_in_freelist(pageinfo * pi){
	pageinfo *tpi=mem_freelist;
	cprintf("pi:%x, tpi:%x\n",pi,tpi);
	if (pi==mem_freelist) return mem_freelist; //pi is the first iterm in the list, has no prior pi;
	else{
	while (tpi->free_next!=NULL){
		cprintf("pi:%x, tpi:%x,tpi->next:%x\n",pi,tpi,tpi->free_next);
		if (tpi->free_next==pi) return tpi;
		tpi=tpi->free_next;
		}
	return NULL;
	}
}

bool is_free_contiguous_pages(pageinfo * pi, unsigned long n_pages){
	pageinfo *tpi=pi;
	unsigned long sizeof_pi=sizeof(pageinfo);
	while((tpi-pi)<(n_pages)){
		if (tpi->refcount!=0) {
			return false;
			}
		if ((tpi->free_next-tpi)!=0x1) {
			return false;
			}
		tpi=tpi+0x1;
	}
	return true;
}

pageinfo * find_contiguous_pages(unsigned long n_pages){
	unsigned long begin=pml.vmm_heap_start;
	unsigned long end=pml.max_page;
	unsigned long i,j;
	for (i=0;i<(end-begin);i++){
		pageinfo *tpi= &(mem_pageinfo[i+begin]);
		if (is_free_contiguous_pages(tpi,n_pages)){
			cprintf("Find contiguous region:%x, of %d pages, TPIis:%x\n",&mem_pageinfo[i+begin],n_pages,tpi);
			pageinfo * prior=find_prior_pi_in_freelist(tpi);
			pageinfo * next=next_n_page_in_freelist(tpi,n_pages-1);
			if (prior == mem_freelist)  mem_freelist=next;
			else  prior->free_next=next;

			for (j=0;j<n_pages;j++){
			mem_incref(&mem_pageinfo[j+i+begin]);
			mem_pageinfo[j+i+begin].refcount=1;
			}
			cprintf("prior:0x%x, next:0x%x\n",prior,next);
			//TODO:update the reference count of all theste allocate pages
			return &mem_pageinfo[i+begin];
		}
}
	return NULL;	//error, find no contigous pages
}

unsigned long alloc_host_pages ( unsigned long nr_pfns, unsigned long pfn_align )
{
//      pageinfo * returnedpage=mem_alloc_contiguous(nr_pfns);
	pageinfo * returnedpage=find_contiguous_pages(nr_pfns);
	if(returnedpage==NULL) return 0;
	return (returnedpage-mem_pageinfo);
}


unsigned long mem_alloc_one_page(){
	pageinfo *pi=mem_alloc();
	if (pi==NULL) {
	cprintf("ERROR:Allocation a page \n");
	return 0;
	}
	cprintf("alloc page: %x\n",(pi-mem_pageinfo));
	return  (pi-mem_pageinfo);
}

//
// Return a page to the free list, given its pageinfo pointer.
// (This function should only be called when pp->pp_ref reaches 0.)
//
void
mem_free(pageinfo *pi)
{
	assert(mem_inited);

	if (pi->refcount != 0)
		panic("mem_free: attempt to free in-use page");
	if (pi->free_next != NULL)
		panic("mem_free: attempt to free already free page!");

	spinlock_acquire(&memlock);
	// Insert the page at the head of the free list.
	pi->free_next = mem_freelist;
	mem_freelist = pi;
	spinlock_release(&memlock);
}

// Atomically increment the reference count on a page.
void
mem_incref(pageinfo *pi)
{
	assert(mem_inited);
	assert(pi > &mem_pageinfo[1] && pi < &mem_pageinfo[mem_npage]);
	assert(pi < mem_ptr2pi(start) || pi > mem_ptr2pi(pml.vmm_heap_start-1));

	lockadd(&pi->refcount, 1);
}

// Atomically decrement the reference count on a page,
// freeing the page if there are no more refs.
void
mem_decref(pageinfo* pi)
{
	assert(mem_inited);
	assert(pi > &mem_pageinfo[1] && pi < &mem_pageinfo[mem_npage]);
	assert(pi < mem_ptr2pi(start) || pi > mem_ptr2pi(pml.vmm_heap_start-1));
	assert(pi->refcount > 0);

	if (lockaddz(&pi->refcount, -1))
			mem_free(pi);
	assert(pi->refcount >= 0);
}

//
// Check the physical page allocator (mem_alloc(), mem_free())
// for correct operation after initialization via mem_init().
//
void
mem_check()
{
	pageinfo *pp, *pp0, *pp1, *pp2;
	pageinfo *fl;
	int i;

	// if there's a page that shouldn't be on
	// the free list, try to make sure it
	// eventually causes trouble.
	int freepages = 0;
	for (pp = mem_freelist; pp != 0; pp = pp->free_next) {
		memset(mem_pi2ptr(pp), 0x97, 128);
		freepages++;
	}
	cprintf("mem_check: %d free pages\n", freepages);
	assert(freepages < mem_npage);	// can't have more free than total!
//	assert(freepages > 16000);	// make sure it's in the right ballpark

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	pp0 = mem_alloc(); assert(pp0 != 0);
	pp1 = mem_alloc(); assert(pp1 != 0);
	pp2 = mem_alloc(); assert(pp2 != 0);

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(mem_pi2phys(pp0) < mem_npage*PAGESIZE);
	assert(mem_pi2phys(pp1) < mem_npage*PAGESIZE);
	assert(mem_pi2phys(pp2) < mem_npage*PAGESIZE);

	// temporarily steal the rest of the free pages
	fl = mem_freelist;
	mem_freelist = 0;

	// should be no free memory
	assert(mem_alloc() == 0);

	// free and re-allocate?
	mem_free(pp0);
	mem_free(pp1);
	mem_free(pp2);
	pp0 = pp1 = pp2 = 0;
	pp0 = mem_alloc(); assert(pp0 != 0);
	pp1 = mem_alloc(); assert(pp1 != 0);
	pp2 = mem_alloc(); assert(pp2 != 0);
	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(mem_alloc() == 0);

	// give free list back
	mem_freelist = fl;

	// free the pages we took
	mem_free(pp0);
	mem_free(pp1);
	mem_free(pp2);

//	cprintf("mem_check() succeeded!\n");
}

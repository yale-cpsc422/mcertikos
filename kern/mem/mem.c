// Physical memory management.
// See COPYRIGHT for copyright information.

#include <architecture/x86.h>
#include <architecture/mmu.h>
#include <architecture/mem.h>

#include <architecture/spinlock.h>

#include <kern/debug/debug.h>
#include <kern/debug/stdio.h>

#include <kern/mem/mem.h>
#include <inc/pmem_layout.h>
#include <inc/multiboot.h>
#include <inc/vmm.h>

#include <kern/hvm/svm/system.h>


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

struct cmdline_option
{       
        unsigned long vmm_pmem_size;
};    

static struct cmdline_option __init parse_cmdline ( const struct multiboot_info *mbi )
{       
        /*TODO: get VMM pmem size and VM pmem size from cmd line
                for now just return the default value, regardless of the content of cmd line
        */
        
        struct cmdline_option opt = { DEFAULT_VMM_PMEM_SIZE };
        
        // Print the command line
        if ( ( mbi->flags & MBI_CMDLINE ) && ( mbi->cmdline != 0 ) )
        {
                char *cmdline = (char*)(unsigned long)mbi->cmdline;
                cprintf ("Command line passed to CertiKOS: %s\n", cmdline );
                cprintf ("Memory map size is %x\n", mbi->mmap_length );
        }

        return opt;
}

static void __init setup_memory(const struct multiboot_info *mbi,
                const struct cmdline_option *opt, struct pmem_layout *pml )
{
        //get the map of machine memory regions from mbi (provided by GRUB)
        //each mem region will have a start address x, size s, and a type t (usable, reserved, ...)
        //store the map to pml->e820
        setup_memory_region (&(pml->e820), mbi);

        // get the number of memory pages, and number of the LAST page usable as RAM (has E820_RAM type)
        pml->total_pages  = get_nr_pages ( &(pml->e820) );
        pml->max_page     = get_max_pfn ( &(pml->e820) );

        // VMM starts at 0x800000 = 128MB
        pml->vmm_pmem_start = DEFAULT_VMM_PMEM_START;
        // VMM heap at the end of VMM memory region
       // pml->vmm_pmem_end = pml->vmm_pmem_start + opt->vmm_pmem_size - 1;

        // Allocate and initialize VMM memory allocation bitmap
        naive_memmap_init ( &(pml->e820), pml );
}

void
mem_init(const struct multiboot_info *mbi)
{
        //Parse the command line that user pass to GRUB
        struct cmdline_option opt = parse_cmdline ( mbi );

        //Set up memory layout and store the layout in pml
        cprintf("pml is at:%x\n",&pml);
        pml.vmm_pmem_end=end+START_PMEM_VMM;
        cprintf("vmm_pmem_end are located at %x\n", pml.vmm_pmem_end);
        setup_memory(mbi, &opt, &pml);
	


	// Do mem_init only once
	assert(!mem_inited);
	mem_inited = true;

	spinlock_init(&memlock);

	// Determine how much base (<640K) and extended (>1MB) memory
	// is available in the system (in bytes),
	// by reading the PC's BIOS-managed nonvolatile RAM (NVRAM).
	// The NVRAM tells us how many kilobytes there are.
	// Since the count is 16 bits, this gives us up to 64MB of RAM;
	// additional RAM beyond that would have to be detected another way.
	size_t basemem = mem_base();
	size_t extmem = mem_ext();

	// The maximum physical address is the top of extended memory.
	mem_max = MEM_EXT + extmem;

	// Compute the total number of physical pages (including I/O holes)
	mem_npage = mem_max / PAGESIZE;
	
	// Update the physical memory layout record
	//allocate the CertiKOS heap space: from pml->vmm_heap_start to lower 64M
/*
	pml->host_heap_pmem_space.addr=pml->vmm_heap_start;
	pml->host_heap_pmem_space.size=mem_max-pml->vmm_heap_start+1;
	pml->host_heap_pmem_space.type=E820_RESERVED;
	pml->rest_pmem4guest_start_page=mem_npage+1;
*/	


	//pml.total_pages=mem_npage;
	//pml.

	// Now that we know the size of physical memory,
	// reserve enough space for the pageinfo array
	// just past our statically-assigned program code/data/bss,
	// which the linker placed at the start of extended memory.
	// Make sure the pageinfo entries are naturally aligned.
	mem_pageinfo = (pageinfo *) ROUNDUP((size_t) end, sizeof(pageinfo));

	// Initialize the entire pageinfo array to zero for good measure.
	memset(mem_pageinfo, 0, sizeof(pageinfo) * mem_npage);

	// Free extended memory starts just past the pageinfo array.
	void *freemem = &mem_pageinfo[mem_npage];

	// Align freemem to page boundary.
	freemem = ROUNDUP(freemem, PAGESIZE);
	// Update the physical memory layout record
	//allocate the CertiKOS heap space: from pml->vmm_heap_start to lower 64M
	pml.vmm_heap_start=mem_phys(freemem)/PAGESIZE;
	pml.host_heap_pmem_space.addr=pml.vmm_heap_start;
	pml.host_heap_pmem_space.size=mem_npage-pml.vmm_heap_start+1;
	pml.host_heap_pmem_space.type=E820_RESERVED;
	pml.rest_pmem4guest_start_page=mem_npage+1;

	int j;
	for(j=0;j<pml.vmm_heap_start;j++){
		map_alloc_record(&mem_pageinfo[j]);
	}


	cprintf("Physical memory for CertiKOS: %dK available, ", (int)(mem_max/1024));
	cprintf("base = %dK, extended = %dK, HEAPpage:0x%x to 0x%x,=%d*4K=%dK\n",
		(int)(basemem/1024), (int)(extmem/1024),pml.vmm_heap_start,mem_npage,pml.host_heap_pmem_space.size, pml.host_heap_pmem_space.size*4);
	// Chain all the available physical pages onto the free page list.
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
	map_alloc_record(pi);
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

// the n_pages of pages start from head are contiguous or not
bool is_free_contiguous_region(pageinfo * head, unsigned long n_pages){
	
	pageinfo * endpage=next_n_page_in_freelist(head,n_pages-1);		
	if (endpage==NULL) return false;
	if (( (endpage-head)/sizeof(pageinfo))==(n_pages-1)) return true;	
	else return false;
}
//
// Allocates a continuous region of physical page from the page free list.
// Does NOT set the contents of the physical page to zero -
// the caller must do that if necessary.
//
// RETURNS 
//   - a pointer to the first page's pageinfo struct if successful
//   - NULL if no available physical pages.
//
// Hint: pi->refs should not be incremented 
// Hint: be sure to use proper mutual exclusion for multiprocessor operation.
pageinfo * mem_alloc_contiguous_pages(pageinfo *head,unsigned long n_pages)
{

	 if (is_free_contiguous_region(head,n_pages)){
		pageinfo *pi=head;	
		unsigned long i;
		for (i=0;i<n_pages;i++){
			pageinfo *t_pi=pi;
			pi=pi->free_next;
			t_pi->free_next=NULL;
			map_alloc_record(t_pi);
		}
		head->free_next=pi;
		return head;
	}else{
	return NULL;
	}					
}

pageinfo *
mem_alloc_contiguous(unsigned long n_pages)
{
	assert(mem_inited);

	spinlock_acquire(&memlock);
	pageinfo *head=mem_freelist;
	pageinfo *returned_pi, *prior_head;	

	while (head!=NULL&&returned_pi==NULL){
		returned_pi=mem_alloc_contiguous_pages(head,n_pages);
		prior_head=head;
		head=head->free_next;
	}
	if (mem_freelist==returned_pi) {
		mem_freelist=returned_pi->free_next;
		}
	else{
		prior_head->free_next=returned_pi->free_next;
	}
	returned_pi->free_next=NULL;
	spinlock_release(&memlock);
	return returned_pi;
}
// return: &mem_freelist, no prior, pi is the head of the list
//	   NULL, pi is not in the free list	
//	   others, pageinfo index
pageinfo * find_prior_pi_in_freelist(pageinfo * pi){
	pageinfo *tpi=mem_freelist;
	if (pi==tpi) return &mem_freelist; //pi is the first iterm in the list, has no prior pi;
	else{
	while (tpi->free_next!=NULL){
		if (tpi->free_next==pi) return tpi;
		tpi=tpi->free_next;
		}	
	return NULL;
	}
}

bool is_free_contiguous_pages(pageinfo * pi, unsigned long n_pages){
	pageinfo *tpi=pi;
	//cprintf("is_freeCP:pi:%x \n",pi);
	unsigned long sizeof_pi=sizeof(pageinfo);
	//cprintf("sizeof pageinfo: %x\n",sizeof_pi);	
	while((tpi-pi)<(n_pages)){
		//tpi=tpi+0x1;	
		//cprintf("sizeof pageinfo: 0x%x\n",sizeof_pi);	
		//cprintf("tpi:0x%x, tpi->refcount:0x%x,tpi->free_next:0x%x\n",tpi,tpi->refcount,tpi->free_next);
		if (tpi->refcount!=0) {
		//	cprintf("tpi->refcount is 0\n");
			return false;
			}
	//	cprintf("tpi->free_next-tpi:%x, address distance:%x\n",tpi->free_next-tpi, (void *) (tpi->free_next)-(void *) tpi);
		if ((tpi->free_next-tpi)!=0x1) {
			//cprintf("tpi->free_next is not next to tpi \n");
			return false;	
			}
		tpi=tpi+0x1;	
	}
	return true;
}

pageinfo * find_contiguous_pages(unsigned long n_pages){
	unsigned long begin=pml.vmm_heap_start;
	unsigned long end=pml.rest_pmem4guest_start_page-1;
	//cprintf("from page:0x%x to page: 0x%x, totoal page numbers: 0x%x\n", begin, end, end-begin);
	//cprintf("mem_pageinfo:%x and it points to %x\n", &mem_pageinfo, mem_pageinfo);
	//cprintf("mem_freelist:%x and it points to %x\n", &mem_freelist, mem_freelist);
	unsigned long i=0;
	for (i=0;i<(end-begin);i++){
		pageinfo *tpi= &(mem_pageinfo[i+begin]);
		if (is_free_contiguous_pages(tpi,n_pages)){
		//	cprintf("Find contiguous region:%x, of %d pages, TPIis:%x\n",&mem_pageinfo[i+begin],n_pages,tpi);
			pageinfo * prior=find_prior_pi_in_freelist(tpi);
			pageinfo * next=next_n_page_in_freelist(tpi,n_pages);
			prior->free_next=next;
			unsigned long j;
			for (j=0;j<n_pages;j++){
			mem_incref(&mem_pageinfo[j+i+begin]);
			mem_pageinfo[j+i+begin].refcount=1;
			} 
		//	cprintf("prior:0x%x, next:0x%x\n",prior,next);
			//TODO:update the reference count of all theste allocate pages
			return &mem_pageinfo[i+begin];	
		}	
}	
	
}
		
unsigned long mem_alloc_one_page(){
	pageinfo *pi=mem_alloc();
	if (pi==NULL) {
	cprintf("ERROR:Allocation a page \n");
	return 0;
	}
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

//added funtions for svm
unsigned long pg_table_alloc (void)
{
	//const unsigned long pfn   = alloc_host_pages (1, 1);
	const unsigned long pfn   = mem_alloc_one_page();
	const unsigned long paddr = pfn << PAGE_SHIFT;

	memset ((char *) paddr, 0, PAGE_SIZE );

	return paddr;
}

unsigned long pml4_table_alloc (void)
{
	return pg_table_alloc();
}

static unsigned long get_index_pae2mb ( unsigned long vaddr, enum pg_table_level level )
{
	unsigned long shift = 0;
	const unsigned long MASK = ( ( 1 << 9 ) - 1 );

	switch ( level ) {
	case PGT_LEVEL_PML4: shift = 39; break;
	case PGT_LEVEL_PDP:  shift = 30; break;
	case PGT_LEVEL_PD:   shift = 21; break;
	default:             cprintf ( "get_index_pae2mb get_index_pae2mb wrong level\n" ); break;
	}

	return ( vaddr >> shift ) & MASK;
}


static unsigned long get_index_4kb ( unsigned long vaddr, enum pg_table_level level )
{
	unsigned long shift = 0;
	const unsigned long MASK = ( ( 1 << 10 ) - 1 );

	switch ( level ) {
	case PGT_LEVEL_PD:   shift = 22; break;
	case PGT_LEVEL_PT:   shift = 12; break;
	default:             cprintf ( "get_index_4kb wrong level\n" ); break;
	}

	return (vaddr >> shift ) & MASK;
}

static union pgt_entry_2mb * get_entry (unsigned long pg_table_base_vaddr, unsigned long vaddr, enum pg_table_level level)
{
	const unsigned long index = get_index_pae2mb ( vaddr, level );
	return ( union pgt_entry_2mb *) ( pg_table_base_vaddr + index * sizeof ( union pgt_entry_2mb ) );
}

static int entry2mb_is_present (const union pgt_entry_2mb *x)
{
	return x->term.flags & PTTEF_PRESENT;
}

static int entry4mb_is_present (const struct pd4M_entry *x)
{
	return x->flags & PTTEF_PRESENT;
}

//haind
static int entry4kb_is_present ( const union pgt_entry_4kb *x )
{
	return x->pte.flags & PTTEF_PRESENT;
}


void mmap_4mb ( unsigned long pg_table_base_vaddr, unsigned long vaddr, unsigned long paddr,
		int flags )
{
	//cprintf ( "__mmap: vaddr=%x, paddr=%x.\n", vaddr, paddr );

	int index = vaddr >> PAGE_SHIFT_4MB;
	struct pd4M_entry * entry = (struct pd4M_entry *) (pg_table_base_vaddr + index * sizeof(struct pd4M_entry));

	/* For page directory entry */
	unsigned int base = paddr >> PAGE_SHIFT_4MB;
	//TODO: handle base > 32 bits
	entry->baselow = base;
	entry->basehigh = 0;

	entry->rsvr = 0;

	if (flags == -1) entry->flags = PTTEF_PRESENT | PTTEF_RW | PTTEF_PAGE_SIZE | PTTEF_US;
	else entry->flags = flags;

//	cprintf ( "__mmap: table=%x, vaddr=%x, paddr=%x, flag=%x.\n", pg_table_base_vaddr, vaddr, paddr, entry->flags);
}


//haind: get a pointer to an entry of page directory or page table
static union pgt_entry_4kb * get_entry4kb (unsigned long pg_table_base_vaddr, unsigned long vaddr, enum pg_table_level level)
{
	const unsigned long index = get_index_4kb (vaddr, level);
	return (union pgt_entry_4kb *) (pg_table_base_vaddr + index * sizeof (union pgt_entry_4kb));
}

static struct pd4M_entry * get_pt_entry4mb (unsigned long pg_table_base_hpaddr, unsigned long linearaddr)
{
	const unsigned long index = linearaddr >> 22;
//	cprintf("pd4M entry index = %x\n",index);

	return (struct pd4M_entry *) (pg_table_base_hpaddr + index * sizeof (struct pd4M_entry));
}


/* [Note] paging with compatibility-mode (long mode with 2-Mbyte page tranlation) is only supported.  */
void __mmap_2mb ( unsigned long pg_table_base_vaddr, unsigned long vaddr, unsigned long paddr,
		enum pg_table_level level, int is_user )
{
	cprintf ( "__mmap: level=%x, vaddr=%x, paddr=%x.\n", level, vaddr, paddr );

	union pgt_entry_2mb *entry = get_entry (pg_table_base_vaddr, vaddr, level);

	if (level == PGT_LEVEL_PD)
	{
		/* For page directory entry */
		entry->term.base = paddr >> PAGE_SHIFT_2MB;
		entry->term.flags = PTTEF_PRESENT | PTTEF_RW | PTTEF_PAGE_SIZE;
		if ( is_user ) { entry->term.flags |= PTTEF_US; }

		//printf ( "__mmap: level=%x, vaddr=%x, paddr=%x.\n", level, vaddr, paddr );

		return;
	}

	//Anh - Note: non terminating tables point to 4KB pages, not 2MB pages
	/* For page-map level-4 entry and page-directory-pointer entry */

	if ( ! entry2mb_is_present ( entry ) )
	{
		const unsigned long paddr = pg_table_alloc();
		entry->non_term.base  = paddr >> PAGE_SHIFT;
		entry->non_term.flags = PTTEF_PRESENT | PTTEF_RW | PTTEF_US;
	}

	// pg_table_base
	const unsigned long next_table_base_vaddr = ( unsigned long ) (entry->non_term.base << PAGE_SHIFT);

	__mmap_2mb ( next_table_base_vaddr, vaddr, paddr, level - 1, is_user );
}

void mmap_pml4 ( unsigned long pml4_table_base_vaddr, unsigned long vaddr, unsigned long paddr, int is_user )
{
	__mmap_2mb( pml4_table_base_vaddr, vaddr, paddr, PGT_LEVEL_PML4, is_user );
}

/******************************************************/

static unsigned long
__linear2physical_legacy2mb ( unsigned long pg_table_base_vaddr, unsigned long vaddr, enum pg_table_level level )
{
	union pgt_entry_2mb *e = get_entry ( pg_table_base_vaddr, vaddr, level );

	if ( ! entry2mb_is_present ( e ) ) {
		cprintf ( "Page table entry is not present.\n" );
	}

	if ( level == PGT_LEVEL_PD ) {

		/* For page directory entry */

		if ( ! ( e->term.flags & PTTEF_PAGE_SIZE ) ) {
			cprintf ( "Not 2 Mbyte page size.\n" );
		}

		return ( ( e->term.base << PAGE_SHIFT_2MB ) + ( vaddr & ( ( 1 << PAGE_SHIFT_2MB ) - 1 ) ) );
	}

	const unsigned long next_table_base_vaddr = ( unsigned long )  (e->non_term.base << PAGE_SHIFT);
	return __linear2physical_legacy2mb ( next_table_base_vaddr, vaddr, level - 1 );
}

//Anh - return host physical addr from guest physical addr, and a pointer to nested pml4 table
// if pml4_table_base_vaddr = 0 => nested paging was disabled
unsigned long linear2physical_legacy2mb (unsigned long pml4_table_base_vaddr, unsigned long vaddr )
{
	if (pml4_table_base_vaddr == 0) return vaddr;
	else return __linear2physical_legacy2mb ( pml4_table_base_vaddr, vaddr, PGT_LEVEL_PML4 );
}

/******************************************************/
void print_4MB_pg_table ( unsigned long pg_table_base_vaddr)
{
	int i;

	for (i = 0; i < 1024; i++ )
	{
		struct pd4M_entry *e = (struct pd4M_entry *) (pg_table_base_vaddr + i * sizeof (struct pd4M_entry));

		if (!entry4mb_is_present(e)) continue;

		cprintf ("index=%x, vaddr=%x, paddr = %x", i, i << PAGE_SHIFT_4MB,
				((u64) e->baselow << PAGE_SHIFT_4MB) + ((u64) e->basehigh << 32));
		cprintf (", flags=%x\n", e->flags );
	}
}

static void __print_pml4_2MB_pg_table ( unsigned long pg_table_base_vaddr, enum pg_table_level level )
{
	int i;

	for ( i = 0; i < 512; i++ ) {
		union pgt_entry_2mb *e = ( union pgt_entry_2mb *) ( pg_table_base_vaddr + i * sizeof ( union pgt_entry_2mb ) );

		if ( ! entry2mb_is_present ( e ) ) {
			continue;
		}

		if ( level == PGT_LEVEL_PD ) {
			cprintf ( "level=%x, index=%x, voffset=%x, paddr = %x, flags=%x PD\n" ,
				 level, i, i << PAGE_SHIFT_2MB, e->term.base << PAGE_SHIFT_2MB, e->term.flags );
		} else {
			char shift;
			switch (level)
			{
			case PGT_LEVEL_PML4:
				shift = 39;
				break;
			case PGT_LEVEL_PDP:
				shift = 30;
				break;
			case PGT_LEVEL_PD:
				//nothing? nabil
				break;
			default: cprintf("__print_pml4_2MB_pg_table wrong paging level");
			}

			//Anh - Note: non terminating tables point to 4KB pages, not 2MB pages
			cprintf ( "level=%x, index=%x, vbase=%x, paddr %x, flags=%x\n" ,
				 level, i, i << shift, e->non_term.base << PAGE_SHIFT, e->non_term.flags );

			const unsigned long next_table_base_vaddr = ( unsigned long ) (e->non_term.base << PAGE_SHIFT);
			__print_pml4_2MB_pg_table( next_table_base_vaddr, level - 1 );
		}
	}
}

void print_pml4_2MB_pg_table ( unsigned long pml4_table_base_vaddr )
{
	cprintf ( "-----------------------------------------\n" );
	__print_pml4_2MB_pg_table ( pml4_table_base_vaddr, PGT_LEVEL_PML4 );
	cprintf ( "-----------------------------------------\n" );
}

//haind the following functions help convert a linear address
//to a physical address & in particular convert a guest physical
//address to a host physical address

static long long __linear2physical_legacy4kb
	(unsigned long pml2_table_base_physicaladdr, unsigned long linearaddr, enum pg_table_level level );

static unsigned long ___linear2physical_legacy4kb
	(union pgt_entry_4kb *e, unsigned long linearaddr, enum pg_table_level level)
{
	unsigned long base = e->pte.base << PAGE_SHIFT_4KB;
//	cprintf("base = %x\n", base);

	/* For page table entry */
	if ( level == PGT_LEVEL_PT ) {
		unsigned long offset = linearaddr & ((1 << PAGE_SHIFT_4KB) - 1);
//		cprintf("offset = %x\n", offset);

		return (base + offset);
	}

	return __linear2physical_legacy4kb (base, linearaddr, level - 1);
}

//haind, Anh: convert linear addr to  physical addr, 4kb in use
static long long __linear2physical_legacy4kb
	(unsigned long pml2_table_base_physicaladdr, unsigned long linearaddr, enum pg_table_level level )
{
	union pgt_entry_4kb *e = get_entry4kb ( pml2_table_base_physicaladdr, linearaddr, level);

	if (!entry4kb_is_present(e)) {
		//TODO: check why page not present in some case (after enter username with tty)
		// cprintf ("Page table entry is not present.\n");
		return -1;
	}

	return ___linear2physical_legacy4kb(e, linearaddr, level);
}

//haind: convert linear addr to  physical addr, 4kb in use
long long linear2physical_legacy4kb (unsigned long pml2_table_base_physicaladdr, unsigned long linearaddr )
{
	return __linear2physical_legacy4kb ( pml2_table_base_physicaladdr, linearaddr, PGT_LEVEL_PD );
}

//haind, Anh: convert linear addr to  physical addr, 4mb in use
static long long __linear2physical_legacy4mb ( unsigned long pml2_table_base_physicaladdr, unsigned long linearaddr)
{
	struct pd4M_entry *e = get_pt_entry4mb (pml2_table_base_physicaladdr, linearaddr);

//	cprintf("entry ptr = %x\n", e);

//	u32 ue = *((u32 *) e);
//	cprintf("entry = %x\n", ue);

	if (e->flags & PTTEF_PAGE_SIZE) {
		// if PS flag is set in PDE => this page is 4MB
		// physical addr = base + offset
		if (!entry4mb_is_present (e))	{
			//TODO: check why page not present in some case (after enter username with tty)
//			cprintf ("Page directory entry is not present.\n");
			return -1;
		}

//		cprintf("entry's flag: %x\n", e->flags);
//		cprintf("basehigh = %x\n", e->basehigh);
//		cprintf("baselow = %x\n", e->baselow);

		//TODO: handle basehigh and base > 4GB
		unsigned long base = e->baselow << 22;
//		cprintf("base = %x\n", base);

		unsigned long offset = linearaddr & ((1 << PAGE_SHIFT_4MB) - 1);
//		cprintf("offset = %x\n", offset);
		return base + offset;
	}
	else { //Anh - PS not set does not mean an error, AMD man vol2, p48
		// if PS flag is not set in PDE => this page is 4KB
//		cprintf("This is a 4KB page\n");
		return ___linear2physical_legacy4kb((union pgt_entry_4kb *) e, linearaddr, PGT_LEVEL_PD);
	}
}

//haind: convert linear addr to  physical addr, 4mb in use
long long linear2physical_legacy4mb (unsigned long pml2_table_base_physicaladdr, unsigned long linearaddr)
{
//	breakpoint("start calling convert");
	return __linear2physical_legacy4mb ( pml2_table_base_physicaladdr, linearaddr);
}

u64 linear_2_physical(u64 cr0, u64 cr3, u64 cr4, u64 guest_linear)
{
	u8 pe = (cr0 & X86_CR0_PE) > 0;

	//get segment selector and offset in case segmentation is in use
	u64 guest_physical;

	if (pe == 0)//real mode
		guest_physical = guest_linear;
	else //protected mode
	{
		u8 pg = (cr0 & X86_CR0_PG) > 0;
		u8 pse = (cr4 & X86_CR4_PSE) > 0;

		if (cr4 & X86_CR4_PAE) {
			cprintf("Converting address in 2mb PAE paging mode not supported yet!");
		}

		if (pg == 0) // no guest paging => GP = GLinear
			guest_physical = guest_linear;
		else
		{
			//the following instructions work on a hypothesis that long mode
			//isn't in use and page address extension is disabled
			//first determining whether 4 kb pages or 4 mb pages are being used
			//reading page 123 of AMD vol 2 for more details
			//extracting the bit PSE of cr4

			// cr3 contains guest physical
			// since GP = HP => cr3 contains HP of the guest paging structure
			if (pse==0)//4kb
				guest_physical = linear2physical_legacy4kb(cr3, guest_linear);
			else //4mb
				guest_physical = linear2physical_legacy4mb(cr3, guest_linear);
		}

//		cprintf("==> guest physical: %x\n", guest_physical);
	}

	return guest_physical;
}


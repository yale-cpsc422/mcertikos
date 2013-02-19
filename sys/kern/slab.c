#include <sys/debug.h>
#include <sys/gcc.h>
#include <sys/mem.h>
#include <sys/pcpu.h>
#include <sys/queue.h>
#include <sys/slab.h>
#include <sys/spinlock.h>
#include <sys/string.h>
#include <sys/types.h>

#ifdef DEBUG_SLAB

#define SLAB_DEBUG(fmt, ...) do {			\
		KERN_DEBUG("SLAB: "fmt, ##__VA_ARGS__);	\
	} while (0)

#else

#define SLAB_DEBUG(...) do {			\
	} while (0)

#endif

#define SLAB_MAX_NPGS_ORDER	11

#define SLAB_FLAGS_MASK		(SLAB_F_OFF_SLAB | SLAB_F_NO_REAP)
#define ALLOC_FLAGS_MASK	(KMEM_ALLOC_F_ZERO)

#define SLAB_BUFCTL(slab)					\
	((kmem_bufctl_t *) ((struct kmem_slab *) (slab) + 1))

#define SLAB_OBJECT(slab, size, nr)				\
	((void *) ((uintptr_t) (slab)->objs + (size) * (nr)))

static volatile int kmem_cache_inited = FALSE;

/*
 * All caches are linked to cache_chain.
 */
static LIST_HEAD(CACHE_CHAIN, kmem_cache) cache_chain;
static spinlock_t cache_chain_lock;

/*
 * All objects of type struct kmem_cache except cache_cache are allocated from
 * cache_cache.
 */
static struct kmem_cache cache_cache = {
	.slabs_full = LIST_HEAD_INITIALIZER(0),
	.slabs_partial = LIST_HEAD_INITIALIZER(0),
	.slabs_free = LIST_HEAD_INITIALIZER(0),
	.objsize = sizeof(struct kmem_cache),
	.flags = SLAB_F_NO_REAP,
	.cache_lock = 0,
	.npgs_order = 0,
	.color_next = 0,
	.growing = FALSE,
	.ctor = NULL,
	.dtor = NULL,
	.name = "cache_cache",
};

/*
 * All available sizes of the size caches.
 */
static struct size_cache_info {
	const char		name[KMEM_CACHE_NAME_MAX_LEN];
	size_t			size;
	struct kmem_cache	*cache;
} size_cache_infos[] = {
	{ .name = "kmalloc-32",		.size = 32 },
	{ .name = "kmalloc-64",		.size = 64 },
	{ .name = "kmalloc-128",	.size = 128 },
	{ .name = "kmalloc-256",	.size = 256 },
	{ .name = "kmalloc-512",	.size = 512 },
	{ .name = "kmalloc-1024",	.size = 1024 },
	{ .name = "kmalloc-2048",	.size = 2048 },
	{ .name = "kmalloc-4096",	.size = 4096 },
	{ .name = "kmalloc-8192",	.size = 8192 },
	{ .name = "kmalloc-16384",	.size = 16384 },
	{ .name = "kmalloc-32768",	.size = 32768 },
	{ .name = "kmalloc-65536",	.size = 65536 },
	{ .name = "kmalloc-131072",	.size = 131072 },
	{ .size = 0 }
};

/*
 * Calculate which size cache is best fit for the given size.
 *
 * @param size the size in bytes
 *
 * @return the index in the array cache_size[] if a fit one is found; otherwise,
 *         return -1.
 */
static gcc_inline int
index_of(size_t size)
{
	if (size <= 32)
		return 0;
	else if (size <= 64)
		return 1;
	else if (size <= 128)
		return 2;
	else if (size <= 256)
		return 3;
	else if (size <= 512)
		return 4;
	else if (size <= 1024)
		return 5;
	else if (size <= 2048)
		return 6;
	else if (size <= 4096)
		return 7;
	else if (size <= 8192)
		return 8;
	else if (size <= 16384)
		return 9;
	else if (size <= 32768)
		return 10;
	else if (size <= 65536)
		return 11;
	else if (size <= 131072)
		return 12;
	else
		return -1;
}

/*
 * Calculate the number of objects on a slab.
 *
 * @param npgs_order the number of pages (2^npgs_order) used by the slab
 * @param buf_size   the size in bytes of an object on the slab
 * @param align      the alignment in bytes of an object on the slab
 * @param flags      the slab flags
 * @param left_over  return how many bytes are left on the slab when the slab
 *                   is full of objects
 * @param num        return the number of the objects on the slab
 */
static gcc_inline void
kmem_estimate_objs_num(size_t npgs_order, size_t buf_size, uint32_t align,
		       uint32_t flags, size_t *left_over, size_t *num)
{
	size_t slab_size, nr_objs;

	slab_size = PAGE_SIZE * (1 << npgs_order);

	/*
	 * Off slab.
	 */
	if (flags & SLAB_F_OFF_SLAB) {
		*num = slab_size / buf_size;
		*left_over = slab_size - *num * buf_size;
		return;
	}

	/*
	 * On slab.
	 */
	nr_objs = (slab_size - sizeof(struct kmem_slab)) /
		(buf_size + sizeof(kmem_bufctl_t));
	if (ROUNDUP(sizeof(struct kmem_slab) + sizeof(kmem_bufctl_t) * nr_objs,
		    align) + buf_size * nr_objs > slab_size)
		nr_objs--;
	*num = nr_objs;
	*left_over = slab_size - buf_size * nr_objs -
		ROUNDUP(sizeof(struct kmem_slab) +
			sizeof(kmem_bufctl_t) * nr_objs, align);
}

/*
 * Initialize cache_cache.
 */
static gcc_inline void
kmem_init_cache_cache(void)
{
	size_t left_over;

	spinlock_acquire(&cache_cache.cache_lock);

	cache_cache.color_off = pcpu_cur()->arch_info.l1_cache_line_size;
	kmem_estimate_objs_num(cache_cache.npgs_order, cache_cache.objsize,
			       cache_cache.color_off, cache_cache.flags,
			       &left_over, &cache_cache.num);
	KERN_ASSERT(cache_cache.num);
	cache_cache.color = left_over / cache_cache.color_off;

	cache_cache.slab_size = ROUNDUP(sizeof(struct kmem_slab) +
					sizeof(kmem_bufctl_t) * cache_cache.num,
					cache_cache.color_off);

	spinlock_acquire(&cache_chain_lock);
	LIST_INSERT_HEAD(&cache_chain, &cache_cache, cache_entry);
	spinlock_release(&cache_chain_lock);

	SLAB_DEBUG("Created cache %s, objsize %d, num %d, page order %d, "
		   "slab_size %d, color offset %d, color %d.\n",
		   cache_cache.name, cache_cache.objsize, cache_cache.num,
		   cache_cache.npgs_order, cache_cache.slab_size,
		   cache_cache.color_off, cache_cache.color);

	spinlock_release(&cache_cache.cache_lock);
}

/*
 * Grow the cache by a slab.
 *
 * @param cache the cache to grow
 *
 * @return 0 if successful; otherwise, return a non-zero value
 */
static int
kmem_cache_grow(struct kmem_cache *cache)
{
	KERN_ASSERT(cache != NULL);
	KERN_ASSERT(spinlock_holding(&cache->cache_lock) == TRUE);
	KERN_ASSERT(cache->growing == FALSE);

	SLAB_DEBUG("Growing cache %s ... \n", cache->name);

	struct kmem_slab *slab;
	pageinfo_t *slab_pi;
	int i;
	size_t color_off;

	cache->growing = TRUE;

	/*
	 * Allocate memory pages for the slab.
	 */

	slab_pi = mem_pages_alloc(PAGE_SIZE * (1 << cache->npgs_order));

	if (slab_pi == NULL) {
		SLAB_DEBUG("Cannot allocate memory to grow cache %s.\n",
			   cache->name);
		return -1;
	}

	/*
	 * Allocate memory for the slab structures.
	 */

	if (cache->flags & SLAB_F_OFF_SLAB)
		slab = kmalloc(cache->slab_size, KMEM_ALLOC_F_ZERO);
	else
		slab = (struct kmem_slab *) mem_pi2ptr(slab_pi);

	if (slab == NULL) {
		SLAB_DEBUG("Cannot allocate slab structures to "
			   "grow cache %s.\n", cache->name);
		mem_pages_free(slab_pi);
		return -2;
	}

	/*
	 * Calculate the color offset.
	 */

	color_off = cache->color_off * cache->color_next;
	cache->color_next++;
	if (cache->color_next >= cache->color)
		cache->color_next = 0;

	/*
	 * Initialize bufctl[].
	 */

	slab->free = 0;

	for (i = 0; i < cache->num - 1; i++)
		SLAB_BUFCTL(slab)[i] = i+1;
	SLAB_BUFCTL(slab)[i] = BUFCTL_END;

	/*
	 * Initialize objects on the slab.
	 */

	slab->inuse = 0;

	if (cache->flags & SLAB_F_OFF_SLAB)
		slab->objs = (void *) (mem_pi2phys(slab_pi) + color_off);
	else
		slab->objs = (void *)
			((uintptr_t) slab + cache->slab_size + color_off);

	if (cache->ctor)
		for (i = 0; i < cache->num; i++)
			cache->ctor(SLAB_OBJECT(slab, cache->objsize, i),
				    cache);

	/*
	 * Link the pages used by the slab.
	 */

	for (i = 0; i < (1 << cache->npgs_order); i++) {
		slab_pi[i].slab = slab;
		slab_pi[i].cache = cache;
	}

	/*
	 * Add the slab to slabs_free.
	 */

	LIST_INSERT_HEAD(&cache->slabs_free, slab, slab_entry);

	cache->growing = FALSE;
	SLAB_DEBUG("Grew cache %s by 1 slab (0x%08x, %d objects 0x%08x).\n",
		   cache->name, slab, cache->num, slab->objs);
	return 0;
}

#ifdef DEBUG_SLAB
#ifdef DEBUG_SLAB_TEST

static void *__slab_test_obj[100];

static int
kmem_cache_test(void)
{
	size_t size;
	int i;

	for (size = 0; size < 131072; size++) {
		for (i = 0; i < 100; i++) {
			__slab_test_obj[i] = kmalloc(size, 0);
			if (__slab_test_obj[i] == NULL) {
				SLAB_DEBUG("Cannot allocate for size %d (%d).\n",
					   size, i);
				return 1;
			}
		}

		for (i = 0; i < 100; i++)
			kfree(__slab_test_obj[i]);

		SLAB_DEBUG("Pass size %d.\n", size);
	}

	return 0;
}

#endif
#endif

int
kmem_cache_init(void)
{
	struct size_cache_info *sc_info;
	uint32_t slab_flags;

	SLAB_DEBUG("sizeof(struct kmem_cache) = %d, "
		   "sizeof(struct kmem_slab) = %d.\n",
		   sizeof(struct kmem_cache), sizeof(struct kmem_slab));

	if (kmem_cache_inited == TRUE || pcpu_onboot() == FALSE)
		return -1;

	/*
	 * Initialize cache_chain.
	 */
	spinlock_init(&cache_chain_lock);
	LIST_INIT(&cache_chain);

	/*
	 * Initialize cache_cache.
	 */
	kmem_init_cache_cache();

	/*
	 * Initialize size caches.
	 */
	for (sc_info = size_cache_infos; sc_info->size != 0; sc_info++) {
		SLAB_DEBUG("Creating cache %s ...\n", sc_info->name);

		slab_flags = (sc_info->size >= PAGE_SIZE / 8) ?
			(SLAB_F_OFF_SLAB | SLAB_F_NO_REAP) : SLAB_F_NO_REAP;
		sc_info->cache =
			kmem_cache_create(sc_info->name, sc_info->size, 8,
					  slab_flags, NULL, NULL);

		if (sc_info->cache == NULL)
			KERN_PANIC("Cannot create cache %s.\n", sc_info->name);
	}

	kmem_cache_inited = TRUE;

#ifdef DEBUG_SLAB
#ifdef DEBUG_SLAB_TEST
	if (kmem_cache_test())
		KERN_PANIC("kmem_cache_test() failed.\n");
#endif
#endif

	return 0;
}

struct kmem_cache *
kmem_cache_create(const char *name, size_t size, size_t align, uint32_t flags,
		  void (*ctor)(void *, struct kmem_cache *),
		  void (*dtor)(void *, struct kmem_cache *))
{
	struct kmem_cache *cache;
	int i;
	size_t left_over;

	cache = kmem_cache_alloc(&cache_cache, KMEM_ALLOC_F_ZERO);

	if (cache == NULL) {
		SLAB_DEBUG("Cannot allocate from cache_cache.\n");
		return NULL;
	}

	LIST_INIT(&cache->slabs_full);
	LIST_INIT(&cache->slabs_partial);
	LIST_INIT(&cache->slabs_free);

	cache->objsize = size;
	cache->flags = flags & SLAB_FLAGS_MASK;

	align = (align == 0) ? pcpu_cur()->arch_info.l1_cache_line_size : align;

	for (i = 0; i < SLAB_MAX_NPGS_ORDER; i++) {
		kmem_estimate_objs_num(i, size, align, cache->flags,
				       &left_over, &cache->num);
		if (cache->num)
			break;
	}

	if (i == SLAB_MAX_NPGS_ORDER) {
		SLAB_DEBUG("Object size %d is too large.\n", size);
		return NULL;
	}

	cache->npgs_order = i;
	cache->slab_size = ROUNDUP(sizeof(struct kmem_slab) +
				   cache->num * sizeof(kmem_bufctl_t), align);

	cache->color_off = align;
	cache->color = left_over / align;
	cache->color_next = 0;

	cache->growing = FALSE;

	cache->ctor = ctor;
	cache->dtor = dtor;

	strncpy(cache->name, name ? name : "NONAME", KMEM_CACHE_NAME_MAX_LEN);

	spinlock_init(&cache->cache_lock);

	spinlock_acquire(&cache_chain_lock);
	LIST_INSERT_HEAD(&cache_chain, cache, cache_entry);
	spinlock_release(&cache_chain_lock);

	SLAB_DEBUG("Created cache %s, objsize %d, num %d, page order %d, "
		   "slab_size %d, color offset %d, color %d.\n",
		   cache->name, cache->objsize, cache->num, cache->npgs_order,
		   cache->slab_size, cache->color_off, cache->color);

	return cache;
}

void
kmem_cache_destroy(struct kmem_cache *cache)
{
	KERN_ASSERT(cache != NULL);
	KERN_PANIC("kmem_cache_destroy() not implemented yet!\n");
}

void *
kmem_cache_alloc(struct kmem_cache *cache, uint32_t flags)
{
	KERN_ASSERT(cache != NULL);

	SLAB_DEBUG("Allocating from cache %s ...\n", cache->name);

	struct kmem_slab *slab;
	void *obj;

	obj = NULL;
	flags &= ALLOC_FLAGS_MASK;

 retry:
	spinlock_acquire(&cache->cache_lock);

	/*
	 * Grow the cache if it's full.
	 */
	if (LIST_EMPTY(&cache->slabs_partial) &&
	    LIST_EMPTY(&cache->slabs_free)) {
		SLAB_DEBUG("Slab is empty. Grow it.\n");

		if (kmem_cache_grow(cache)) {
			SLAB_DEBUG("Cannot grown cache %s.\n", cache->name);
			goto ret;
		}
		spinlock_release(&cache->cache_lock);
		goto retry;
	}

	/*
	 * Move a slab from slabs_free to slabs_partial if there are no partial
	 * slabs right now.
	 */
	if (LIST_EMPTY(&cache->slabs_partial)) {
		slab = LIST_FIRST(&cache->slabs_free);
		KERN_ASSERT(slab != NULL);
		LIST_REMOVE(slab, slab_entry);
		LIST_INSERT_HEAD(&cache->slabs_partial, slab, slab_entry);

		SLAB_DEBUG("Moved slab 0x%08x from slabs_free to "
			   "slabs_partial.\n", slab);

		spinlock_release(&cache->cache_lock);
		goto retry;
	}

	/*
	 * Allocate from the partial slab.
	 */

	slab = LIST_FIRST(&cache->slabs_partial);
	KERN_ASSERT(slab != NULL);

	obj = (void *) ((uintptr_t) slab->objs + slab->free * cache->objsize);
	KERN_ASSERT(obj != NULL);
	slab->free = SLAB_BUFCTL(slab)[slab->free];
	slab->inuse++;

	/*
	 * Move the partial slab to slabs_full if it's full.
	 */
	if (slab->inuse == cache->num) {
		LIST_REMOVE(slab, slab_entry);
		LIST_INSERT_HEAD(&cache->slabs_full, slab, slab_entry);

		SLAB_DEBUG("Moved slab 0x%08x from slabs_partial to "
			   "slabs_full.\n", slab);
	}

	SLAB_DEBUG("Allocated obj 0x%08x from slab 0x%08x (free %d) "
		   "in cache %s.\n", obj, slab, slab->free, cache->name);

 ret:
	spinlock_release(&cache->cache_lock);
	return obj;
}

void
kmem_cache_free(struct kmem_cache *cache, void *obj)
{
	KERN_ASSERT(cache != NULL);
	KERN_ASSERT(obj != NULL);

	pageinfo_t *pi;
	struct kmem_slab *slab;
	size_t nr;

	pi = mem_ptr2pi(obj);

	spinlock_acquire(&cache->cache_lock);

	KERN_ASSERT(pi->cache == cache);

	slab = pi->slab;

	/*
	 * Link the corresponding bufctl.
	 */
	nr = ((uintptr_t) obj - (uintptr_t) slab->objs) / cache->objsize;
	SLAB_BUFCTL(slab)[nr] = slab->free;
	slab->free = nr;

	/*
	 * Adjust the linkage of the slab if necessary.
	 *
	 * - If the slab is empty after the deallocation, move it to slabs_free.
	 *
	 * - If the slab is full before the deallocation and not empty after the
	 *   deallocation, move it to slabs_partial.
	 */

	if (slab->inuse == cache->num || slab->inuse == 1)
		LIST_REMOVE(slab, slab_entry);

	if (slab->inuse == 1)
		LIST_INSERT_HEAD(&cache->slabs_free, slab, slab_entry);
	else if (slab->inuse == cache->num)
		LIST_INSERT_HEAD(&cache->slabs_partial, slab, slab_entry);

	slab->inuse--;

	/*
	 * Call the destructor if necessary.
	 */

	if (cache->dtor)
		cache->dtor(obj, cache);

	spinlock_release(&cache->cache_lock);
}

void *
kmalloc(size_t size, uint32_t flags)
{
	struct kmem_cache *cache;
	int index;

	if ((index = index_of(size)) == -1)
		return NULL;

	if ((cache = size_cache_infos[index].cache) == NULL)
		return NULL;

	return kmem_cache_alloc(cache, flags & ALLOC_FLAGS_MASK);
}

void
kfree(void *obj)
{
	if (obj == NULL)
		return;

	pageinfo_t *pi;
	struct kmem_cache *cache;

	if ((pi = mem_ptr2pi(obj)) == NULL)
		return;

	if ((cache = pi->cache) == NULL)
		return;

	kmem_cache_free(cache, obj);
}

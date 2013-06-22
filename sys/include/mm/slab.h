#ifndef _SYS_SLAB_H_
#define _SYS_SLAB_H_

#ifdef _KERN_

#include <lib/gcc.h>
#include <lib/queue.h>
#include <lib/spinlock.h>
#include <lib/types.h>

#define KMEM_CACHE_NAME_MAX_LEN	64

/*
 * Slab flags.
 */
#define SLAB_F_OFF_SLAB		0x00000001	/* slab control structures are
						   off the slab */
#define SLAB_F_NO_REAP		0x00000002	/* the slab should never be
						   reaped */

/*
 * Allocation flags.
 */
#define KMEM_ALLOC_F_ZERO	0x00000001	/* zero the object after the
						   the successful allocation */

typedef unsigned int		kmem_bufctl_t;

#define BUFCTL_END		(~(kmem_bufctl_t) 0)

struct kmem_slab {
	void		*objs;		/* list of objects */
	size_t		inuse;		/* number of objects in use */
	kmem_bufctl_t	free;		/* index of the first free object */
	LIST_ENTRY(kmem_slab)	slab_entry;
};

struct kmem_cache {
	LIST_HEAD(SLABS_FULL, kmem_slab)	slabs_full;
	LIST_HEAD(SLABS_PARTIAL, kmem_slab)	slabs_partial;
	LIST_HEAD(SLABS_FREE, kmem_slab)	slabs_free;

	size_t		objsize;	/* object size in bytes */
	uint32_t	flags;		/* flags of slabs */
	size_t		num;		/* number of objects on a slab */

	spinlock_t	cache_lock;	/* spinlock of the cache */

	size_t		npgs_order;	/* 2^(npgs_order) pages per slab */
	size_t		slab_size;	/* size in bytes for slab structures */

	uint32_t	color_off;	/* offset of the color */
	int		color;		/* number of colors */
	int		color_next;	/* next color index to use */

	bool		growing;	/* set when the cache is growing */

	void (*ctor)(void *, struct kmem_cache *);	/* object constructor */
	void (*dtor)(void *, struct kmem_cache *);	/* object destructor */

	char		name[KMEM_CACHE_NAME_MAX_LEN];

	LIST_ENTRY(kmem_cache)	cache_entry;	/* entry in cache_cache */
};

/*
 * Initialize the slab allocator.
 *
 * @return 0 if successful; otherwise, return a non-zero value.
 */
int kmem_cache_init(void);

/*
 * Create a cache.
 *
 * @param name  the name of the cache
 * @param size  the size in bytes of the object in the cache
 * @param align the alignment in bytes of the object in the cache
 * @param flags the slab flags
 * @param ctor  the object constructor
 * @param dtor  the object destructor
 *
 * @return a pointer to the newly created cache if successful; otherwise, return
 *         NULL
 */
struct kmem_cache *kmem_cache_create(const char *name, size_t size,
				     size_t align, uint32_t flags,
				     void (*ctor)(void *, struct kmem_cache *),
				     void (*dtor)(void *, struct kmem_cache *));

/*
 * Destroy a cache and free all its memory.
 *
 * XXX: The caller must make sure there's no allocation during
 *      kmem_cache_destroy(). Otherwise, either the allocation or
 *      kmem_cache_destroy(), or both may crash.
 *
 * @param cache the cache to destroy
 */
void kmem_cache_destroy(struct kmem_cache *cache);

/*
 * Release memory used by the free slabs in a cache.
 *
 * @param cache the cache to shrink
 */
void kmem_cache_shrink(struct kmem_cache *cache);

/*
 * Allocate an object from a cache.
 *
 * @param cache the cache where the object is allocated from
 * @param flags the allocation flags
 *
 * @return a pointer to the newly allocated object if successful; otherwise,
 *         return NULL
 */
void *kmem_cache_alloc(struct kmem_cache *cache, uint32_t flags);

/*
 * Free an object.
 *
 * @param cache the cache where the object was allocated
 * @param obj   the object to free
 */
void kmem_cache_free(struct kmem_cache *cache, void *obj);

/*
 * Allocate memory for an object.
 *
 * @param size  the size in bytes of the object
 * @param flags the allocation flags
 *
 * @return a pointer to the newly allocated object if successful; otherwise,
 *         return NULL
 */
void *kmalloc(size_t size, uint32_t flags);

/*
 * Free an object.
 *
 * @param obj the object to free
 */
void kfree(void *obj);

#endif /* _KERN_ */

#endif /* !_SYS_SLAB_H_ */

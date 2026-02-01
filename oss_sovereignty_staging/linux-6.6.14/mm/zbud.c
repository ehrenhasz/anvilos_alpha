
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/atomic.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/preempt.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/zpool.h>

 
 
#define NCHUNKS_ORDER	6

#define CHUNK_SHIFT	(PAGE_SHIFT - NCHUNKS_ORDER)
#define CHUNK_SIZE	(1 << CHUNK_SHIFT)
#define ZHDR_SIZE_ALIGNED CHUNK_SIZE
#define NCHUNKS		((PAGE_SIZE - ZHDR_SIZE_ALIGNED) >> CHUNK_SHIFT)

struct zbud_pool;

 
struct zbud_pool {
	spinlock_t lock;
	union {
		 
		struct list_head buddied;
		struct list_head unbuddied[NCHUNKS];
	};
	u64 pages_nr;
};

 
struct zbud_header {
	struct list_head buddy;
	unsigned int first_chunks;
	unsigned int last_chunks;
};

 
 
enum buddy {
	FIRST,
	LAST
};

 
static int size_to_chunks(size_t size)
{
	return (size + CHUNK_SIZE - 1) >> CHUNK_SHIFT;
}

#define for_each_unbuddied_list(_iter, _begin) \
	for ((_iter) = (_begin); (_iter) < NCHUNKS; (_iter)++)

 
static struct zbud_header *init_zbud_page(struct page *page)
{
	struct zbud_header *zhdr = page_address(page);
	zhdr->first_chunks = 0;
	zhdr->last_chunks = 0;
	INIT_LIST_HEAD(&zhdr->buddy);
	return zhdr;
}

 
static void free_zbud_page(struct zbud_header *zhdr)
{
	__free_page(virt_to_page(zhdr));
}

 
static unsigned long encode_handle(struct zbud_header *zhdr, enum buddy bud)
{
	unsigned long handle;

	 
	handle = (unsigned long)zhdr;
	if (bud == FIRST)
		 
		handle += ZHDR_SIZE_ALIGNED;
	else  
		handle += PAGE_SIZE - (zhdr->last_chunks  << CHUNK_SHIFT);
	return handle;
}

 
static struct zbud_header *handle_to_zbud_header(unsigned long handle)
{
	return (struct zbud_header *)(handle & PAGE_MASK);
}

 
static int num_free_chunks(struct zbud_header *zhdr)
{
	 
	return NCHUNKS - zhdr->first_chunks - zhdr->last_chunks;
}

 
 
static struct zbud_pool *zbud_create_pool(gfp_t gfp)
{
	struct zbud_pool *pool;
	int i;

	pool = kzalloc(sizeof(struct zbud_pool), gfp);
	if (!pool)
		return NULL;
	spin_lock_init(&pool->lock);
	for_each_unbuddied_list(i, 0)
		INIT_LIST_HEAD(&pool->unbuddied[i]);
	INIT_LIST_HEAD(&pool->buddied);
	pool->pages_nr = 0;
	return pool;
}

 
static void zbud_destroy_pool(struct zbud_pool *pool)
{
	kfree(pool);
}

 
static int zbud_alloc(struct zbud_pool *pool, size_t size, gfp_t gfp,
			unsigned long *handle)
{
	int chunks, i, freechunks;
	struct zbud_header *zhdr = NULL;
	enum buddy bud;
	struct page *page;

	if (!size || (gfp & __GFP_HIGHMEM))
		return -EINVAL;
	if (size > PAGE_SIZE - ZHDR_SIZE_ALIGNED - CHUNK_SIZE)
		return -ENOSPC;
	chunks = size_to_chunks(size);
	spin_lock(&pool->lock);

	 
	for_each_unbuddied_list(i, chunks) {
		if (!list_empty(&pool->unbuddied[i])) {
			zhdr = list_first_entry(&pool->unbuddied[i],
					struct zbud_header, buddy);
			list_del(&zhdr->buddy);
			if (zhdr->first_chunks == 0)
				bud = FIRST;
			else
				bud = LAST;
			goto found;
		}
	}

	 
	spin_unlock(&pool->lock);
	page = alloc_page(gfp);
	if (!page)
		return -ENOMEM;
	spin_lock(&pool->lock);
	pool->pages_nr++;
	zhdr = init_zbud_page(page);
	bud = FIRST;

found:
	if (bud == FIRST)
		zhdr->first_chunks = chunks;
	else
		zhdr->last_chunks = chunks;

	if (zhdr->first_chunks == 0 || zhdr->last_chunks == 0) {
		 
		freechunks = num_free_chunks(zhdr);
		list_add(&zhdr->buddy, &pool->unbuddied[freechunks]);
	} else {
		 
		list_add(&zhdr->buddy, &pool->buddied);
	}

	*handle = encode_handle(zhdr, bud);
	spin_unlock(&pool->lock);

	return 0;
}

 
static void zbud_free(struct zbud_pool *pool, unsigned long handle)
{
	struct zbud_header *zhdr;
	int freechunks;

	spin_lock(&pool->lock);
	zhdr = handle_to_zbud_header(handle);

	 
	if ((handle - ZHDR_SIZE_ALIGNED) & ~PAGE_MASK)
		zhdr->last_chunks = 0;
	else
		zhdr->first_chunks = 0;

	 
	list_del(&zhdr->buddy);

	if (zhdr->first_chunks == 0 && zhdr->last_chunks == 0) {
		 
		free_zbud_page(zhdr);
		pool->pages_nr--;
	} else {
		 
		freechunks = num_free_chunks(zhdr);
		list_add(&zhdr->buddy, &pool->unbuddied[freechunks]);
	}

	spin_unlock(&pool->lock);
}

 
static void *zbud_map(struct zbud_pool *pool, unsigned long handle)
{
	return (void *)(handle);
}

 
static void zbud_unmap(struct zbud_pool *pool, unsigned long handle)
{
}

 
static u64 zbud_get_pool_size(struct zbud_pool *pool)
{
	return pool->pages_nr;
}

 

static void *zbud_zpool_create(const char *name, gfp_t gfp)
{
	return zbud_create_pool(gfp);
}

static void zbud_zpool_destroy(void *pool)
{
	zbud_destroy_pool(pool);
}

static int zbud_zpool_malloc(void *pool, size_t size, gfp_t gfp,
			unsigned long *handle)
{
	return zbud_alloc(pool, size, gfp, handle);
}
static void zbud_zpool_free(void *pool, unsigned long handle)
{
	zbud_free(pool, handle);
}

static void *zbud_zpool_map(void *pool, unsigned long handle,
			enum zpool_mapmode mm)
{
	return zbud_map(pool, handle);
}
static void zbud_zpool_unmap(void *pool, unsigned long handle)
{
	zbud_unmap(pool, handle);
}

static u64 zbud_zpool_total_size(void *pool)
{
	return zbud_get_pool_size(pool) * PAGE_SIZE;
}

static struct zpool_driver zbud_zpool_driver = {
	.type =		"zbud",
	.sleep_mapped = true,
	.owner =	THIS_MODULE,
	.create =	zbud_zpool_create,
	.destroy =	zbud_zpool_destroy,
	.malloc =	zbud_zpool_malloc,
	.free =		zbud_zpool_free,
	.map =		zbud_zpool_map,
	.unmap =	zbud_zpool_unmap,
	.total_size =	zbud_zpool_total_size,
};

MODULE_ALIAS("zpool-zbud");

static int __init init_zbud(void)
{
	 
	BUILD_BUG_ON(sizeof(struct zbud_header) > ZHDR_SIZE_ALIGNED);
	pr_info("loaded\n");

	zpool_register_driver(&zbud_zpool_driver);

	return 0;
}

static void __exit exit_zbud(void)
{
	zpool_unregister_driver(&zbud_zpool_driver);
	pr_info("unloaded\n");
}

module_init(init_zbud);
module_exit(exit_zbud);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seth Jennings <sjennings@variantweb.net>");
MODULE_DESCRIPTION("Buddy Allocator for Compressed Pages");

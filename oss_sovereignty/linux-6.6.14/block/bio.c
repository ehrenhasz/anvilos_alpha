
 
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/uio.h>
#include <linux/iocontext.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mempool.h>
#include <linux/workqueue.h>
#include <linux/cgroup.h>
#include <linux/highmem.h>
#include <linux/sched/sysctl.h>
#include <linux/blk-crypto.h>
#include <linux/xarray.h>

#include <trace/events/block.h>
#include "blk.h"
#include "blk-rq-qos.h"
#include "blk-cgroup.h"

#define ALLOC_CACHE_THRESHOLD	16
#define ALLOC_CACHE_MAX		256

struct bio_alloc_cache {
	struct bio		*free_list;
	struct bio		*free_list_irq;
	unsigned int		nr;
	unsigned int		nr_irq;
};

static struct biovec_slab {
	int nr_vecs;
	char *name;
	struct kmem_cache *slab;
} bvec_slabs[] __read_mostly = {
	{ .nr_vecs = 16, .name = "biovec-16" },
	{ .nr_vecs = 64, .name = "biovec-64" },
	{ .nr_vecs = 128, .name = "biovec-128" },
	{ .nr_vecs = BIO_MAX_VECS, .name = "biovec-max" },
};

static struct biovec_slab *biovec_slab(unsigned short nr_vecs)
{
	switch (nr_vecs) {
	 
	case 5 ... 16:
		return &bvec_slabs[0];
	case 17 ... 64:
		return &bvec_slabs[1];
	case 65 ... 128:
		return &bvec_slabs[2];
	case 129 ... BIO_MAX_VECS:
		return &bvec_slabs[3];
	default:
		BUG();
		return NULL;
	}
}

 
struct bio_set fs_bio_set;
EXPORT_SYMBOL(fs_bio_set);

 
struct bio_slab {
	struct kmem_cache *slab;
	unsigned int slab_ref;
	unsigned int slab_size;
	char name[8];
};
static DEFINE_MUTEX(bio_slab_lock);
static DEFINE_XARRAY(bio_slabs);

static struct bio_slab *create_bio_slab(unsigned int size)
{
	struct bio_slab *bslab = kzalloc(sizeof(*bslab), GFP_KERNEL);

	if (!bslab)
		return NULL;

	snprintf(bslab->name, sizeof(bslab->name), "bio-%d", size);
	bslab->slab = kmem_cache_create(bslab->name, size,
			ARCH_KMALLOC_MINALIGN,
			SLAB_HWCACHE_ALIGN | SLAB_TYPESAFE_BY_RCU, NULL);
	if (!bslab->slab)
		goto fail_alloc_slab;

	bslab->slab_ref = 1;
	bslab->slab_size = size;

	if (!xa_err(xa_store(&bio_slabs, size, bslab, GFP_KERNEL)))
		return bslab;

	kmem_cache_destroy(bslab->slab);

fail_alloc_slab:
	kfree(bslab);
	return NULL;
}

static inline unsigned int bs_bio_slab_size(struct bio_set *bs)
{
	return bs->front_pad + sizeof(struct bio) + bs->back_pad;
}

static struct kmem_cache *bio_find_or_create_slab(struct bio_set *bs)
{
	unsigned int size = bs_bio_slab_size(bs);
	struct bio_slab *bslab;

	mutex_lock(&bio_slab_lock);
	bslab = xa_load(&bio_slabs, size);
	if (bslab)
		bslab->slab_ref++;
	else
		bslab = create_bio_slab(size);
	mutex_unlock(&bio_slab_lock);

	if (bslab)
		return bslab->slab;
	return NULL;
}

static void bio_put_slab(struct bio_set *bs)
{
	struct bio_slab *bslab = NULL;
	unsigned int slab_size = bs_bio_slab_size(bs);

	mutex_lock(&bio_slab_lock);

	bslab = xa_load(&bio_slabs, slab_size);
	if (WARN(!bslab, KERN_ERR "bio: unable to find slab!\n"))
		goto out;

	WARN_ON_ONCE(bslab->slab != bs->bio_slab);

	WARN_ON(!bslab->slab_ref);

	if (--bslab->slab_ref)
		goto out;

	xa_erase(&bio_slabs, slab_size);

	kmem_cache_destroy(bslab->slab);
	kfree(bslab);

out:
	mutex_unlock(&bio_slab_lock);
}

void bvec_free(mempool_t *pool, struct bio_vec *bv, unsigned short nr_vecs)
{
	BUG_ON(nr_vecs > BIO_MAX_VECS);

	if (nr_vecs == BIO_MAX_VECS)
		mempool_free(bv, pool);
	else if (nr_vecs > BIO_INLINE_VECS)
		kmem_cache_free(biovec_slab(nr_vecs)->slab, bv);
}

 
static inline gfp_t bvec_alloc_gfp(gfp_t gfp)
{
	return (gfp & ~(__GFP_DIRECT_RECLAIM | __GFP_IO)) |
		__GFP_NOMEMALLOC | __GFP_NORETRY | __GFP_NOWARN;
}

struct bio_vec *bvec_alloc(mempool_t *pool, unsigned short *nr_vecs,
		gfp_t gfp_mask)
{
	struct biovec_slab *bvs = biovec_slab(*nr_vecs);

	if (WARN_ON_ONCE(!bvs))
		return NULL;

	 
	*nr_vecs = bvs->nr_vecs;

	 
	if (*nr_vecs < BIO_MAX_VECS) {
		struct bio_vec *bvl;

		bvl = kmem_cache_alloc(bvs->slab, bvec_alloc_gfp(gfp_mask));
		if (likely(bvl) || !(gfp_mask & __GFP_DIRECT_RECLAIM))
			return bvl;
		*nr_vecs = BIO_MAX_VECS;
	}

	return mempool_alloc(pool, gfp_mask);
}

void bio_uninit(struct bio *bio)
{
#ifdef CONFIG_BLK_CGROUP
	if (bio->bi_blkg) {
		blkg_put(bio->bi_blkg);
		bio->bi_blkg = NULL;
	}
#endif
	if (bio_integrity(bio))
		bio_integrity_free(bio);

	bio_crypt_free_ctx(bio);
}
EXPORT_SYMBOL(bio_uninit);

static void bio_free(struct bio *bio)
{
	struct bio_set *bs = bio->bi_pool;
	void *p = bio;

	WARN_ON_ONCE(!bs);

	bio_uninit(bio);
	bvec_free(&bs->bvec_pool, bio->bi_io_vec, bio->bi_max_vecs);
	mempool_free(p - bs->front_pad, &bs->bio_pool);
}

 
void bio_init(struct bio *bio, struct block_device *bdev, struct bio_vec *table,
	      unsigned short max_vecs, blk_opf_t opf)
{
	bio->bi_next = NULL;
	bio->bi_bdev = bdev;
	bio->bi_opf = opf;
	bio->bi_flags = 0;
	bio->bi_ioprio = 0;
	bio->bi_status = 0;
	bio->bi_iter.bi_sector = 0;
	bio->bi_iter.bi_size = 0;
	bio->bi_iter.bi_idx = 0;
	bio->bi_iter.bi_bvec_done = 0;
	bio->bi_end_io = NULL;
	bio->bi_private = NULL;
#ifdef CONFIG_BLK_CGROUP
	bio->bi_blkg = NULL;
	bio->bi_issue.value = 0;
	if (bdev)
		bio_associate_blkg(bio);
#ifdef CONFIG_BLK_CGROUP_IOCOST
	bio->bi_iocost_cost = 0;
#endif
#endif
#ifdef CONFIG_BLK_INLINE_ENCRYPTION
	bio->bi_crypt_context = NULL;
#endif
#ifdef CONFIG_BLK_DEV_INTEGRITY
	bio->bi_integrity = NULL;
#endif
	bio->bi_vcnt = 0;

	atomic_set(&bio->__bi_remaining, 1);
	atomic_set(&bio->__bi_cnt, 1);
	bio->bi_cookie = BLK_QC_T_NONE;

	bio->bi_max_vecs = max_vecs;
	bio->bi_io_vec = table;
	bio->bi_pool = NULL;
}
EXPORT_SYMBOL(bio_init);

 
void bio_reset(struct bio *bio, struct block_device *bdev, blk_opf_t opf)
{
	bio_uninit(bio);
	memset(bio, 0, BIO_RESET_BYTES);
	atomic_set(&bio->__bi_remaining, 1);
	bio->bi_bdev = bdev;
	if (bio->bi_bdev)
		bio_associate_blkg(bio);
	bio->bi_opf = opf;
}
EXPORT_SYMBOL(bio_reset);

static struct bio *__bio_chain_endio(struct bio *bio)
{
	struct bio *parent = bio->bi_private;

	if (bio->bi_status && !parent->bi_status)
		parent->bi_status = bio->bi_status;
	bio_put(bio);
	return parent;
}

static void bio_chain_endio(struct bio *bio)
{
	bio_endio(__bio_chain_endio(bio));
}

 
void bio_chain(struct bio *bio, struct bio *parent)
{
	BUG_ON(bio->bi_private || bio->bi_end_io);

	bio->bi_private = parent;
	bio->bi_end_io	= bio_chain_endio;
	bio_inc_remaining(parent);
}
EXPORT_SYMBOL(bio_chain);

struct bio *blk_next_bio(struct bio *bio, struct block_device *bdev,
		unsigned int nr_pages, blk_opf_t opf, gfp_t gfp)
{
	struct bio *new = bio_alloc(bdev, nr_pages, opf, gfp);

	if (bio) {
		bio_chain(bio, new);
		submit_bio(bio);
	}

	return new;
}
EXPORT_SYMBOL_GPL(blk_next_bio);

static void bio_alloc_rescue(struct work_struct *work)
{
	struct bio_set *bs = container_of(work, struct bio_set, rescue_work);
	struct bio *bio;

	while (1) {
		spin_lock(&bs->rescue_lock);
		bio = bio_list_pop(&bs->rescue_list);
		spin_unlock(&bs->rescue_lock);

		if (!bio)
			break;

		submit_bio_noacct(bio);
	}
}

static void punt_bios_to_rescuer(struct bio_set *bs)
{
	struct bio_list punt, nopunt;
	struct bio *bio;

	if (WARN_ON_ONCE(!bs->rescue_workqueue))
		return;
	 

	bio_list_init(&punt);
	bio_list_init(&nopunt);

	while ((bio = bio_list_pop(&current->bio_list[0])))
		bio_list_add(bio->bi_pool == bs ? &punt : &nopunt, bio);
	current->bio_list[0] = nopunt;

	bio_list_init(&nopunt);
	while ((bio = bio_list_pop(&current->bio_list[1])))
		bio_list_add(bio->bi_pool == bs ? &punt : &nopunt, bio);
	current->bio_list[1] = nopunt;

	spin_lock(&bs->rescue_lock);
	bio_list_merge(&bs->rescue_list, &punt);
	spin_unlock(&bs->rescue_lock);

	queue_work(bs->rescue_workqueue, &bs->rescue_work);
}

static void bio_alloc_irq_cache_splice(struct bio_alloc_cache *cache)
{
	unsigned long flags;

	 
	if (WARN_ON_ONCE(cache->free_list))
		return;

	local_irq_save(flags);
	cache->free_list = cache->free_list_irq;
	cache->free_list_irq = NULL;
	cache->nr += cache->nr_irq;
	cache->nr_irq = 0;
	local_irq_restore(flags);
}

static struct bio *bio_alloc_percpu_cache(struct block_device *bdev,
		unsigned short nr_vecs, blk_opf_t opf, gfp_t gfp,
		struct bio_set *bs)
{
	struct bio_alloc_cache *cache;
	struct bio *bio;

	cache = per_cpu_ptr(bs->cache, get_cpu());
	if (!cache->free_list) {
		if (READ_ONCE(cache->nr_irq) >= ALLOC_CACHE_THRESHOLD)
			bio_alloc_irq_cache_splice(cache);
		if (!cache->free_list) {
			put_cpu();
			return NULL;
		}
	}
	bio = cache->free_list;
	cache->free_list = bio->bi_next;
	cache->nr--;
	put_cpu();

	bio_init(bio, bdev, nr_vecs ? bio->bi_inline_vecs : NULL, nr_vecs, opf);
	bio->bi_pool = bs;
	return bio;
}

 
struct bio *bio_alloc_bioset(struct block_device *bdev, unsigned short nr_vecs,
			     blk_opf_t opf, gfp_t gfp_mask,
			     struct bio_set *bs)
{
	gfp_t saved_gfp = gfp_mask;
	struct bio *bio;
	void *p;

	 
	if (WARN_ON_ONCE(!mempool_initialized(&bs->bvec_pool) && nr_vecs > 0))
		return NULL;

	if (opf & REQ_ALLOC_CACHE) {
		if (bs->cache && nr_vecs <= BIO_INLINE_VECS) {
			bio = bio_alloc_percpu_cache(bdev, nr_vecs, opf,
						     gfp_mask, bs);
			if (bio)
				return bio;
			 
		} else {
			opf &= ~REQ_ALLOC_CACHE;
		}
	}

	 
	if (current->bio_list &&
	    (!bio_list_empty(&current->bio_list[0]) ||
	     !bio_list_empty(&current->bio_list[1])) &&
	    bs->rescue_workqueue)
		gfp_mask &= ~__GFP_DIRECT_RECLAIM;

	p = mempool_alloc(&bs->bio_pool, gfp_mask);
	if (!p && gfp_mask != saved_gfp) {
		punt_bios_to_rescuer(bs);
		gfp_mask = saved_gfp;
		p = mempool_alloc(&bs->bio_pool, gfp_mask);
	}
	if (unlikely(!p))
		return NULL;
	if (!mempool_is_saturated(&bs->bio_pool))
		opf &= ~REQ_ALLOC_CACHE;

	bio = p + bs->front_pad;
	if (nr_vecs > BIO_INLINE_VECS) {
		struct bio_vec *bvl = NULL;

		bvl = bvec_alloc(&bs->bvec_pool, &nr_vecs, gfp_mask);
		if (!bvl && gfp_mask != saved_gfp) {
			punt_bios_to_rescuer(bs);
			gfp_mask = saved_gfp;
			bvl = bvec_alloc(&bs->bvec_pool, &nr_vecs, gfp_mask);
		}
		if (unlikely(!bvl))
			goto err_free;

		bio_init(bio, bdev, bvl, nr_vecs, opf);
	} else if (nr_vecs) {
		bio_init(bio, bdev, bio->bi_inline_vecs, BIO_INLINE_VECS, opf);
	} else {
		bio_init(bio, bdev, NULL, 0, opf);
	}

	bio->bi_pool = bs;
	return bio;

err_free:
	mempool_free(p, &bs->bio_pool);
	return NULL;
}
EXPORT_SYMBOL(bio_alloc_bioset);

 
struct bio *bio_kmalloc(unsigned short nr_vecs, gfp_t gfp_mask)
{
	struct bio *bio;

	if (nr_vecs > UIO_MAXIOV)
		return NULL;
	return kmalloc(struct_size(bio, bi_inline_vecs, nr_vecs), gfp_mask);
}
EXPORT_SYMBOL(bio_kmalloc);

void zero_fill_bio_iter(struct bio *bio, struct bvec_iter start)
{
	struct bio_vec bv;
	struct bvec_iter iter;

	__bio_for_each_segment(bv, bio, iter, start)
		memzero_bvec(&bv);
}
EXPORT_SYMBOL(zero_fill_bio_iter);

 
static void bio_truncate(struct bio *bio, unsigned new_size)
{
	struct bio_vec bv;
	struct bvec_iter iter;
	unsigned int done = 0;
	bool truncated = false;

	if (new_size >= bio->bi_iter.bi_size)
		return;

	if (bio_op(bio) != REQ_OP_READ)
		goto exit;

	bio_for_each_segment(bv, bio, iter) {
		if (done + bv.bv_len > new_size) {
			unsigned offset;

			if (!truncated)
				offset = new_size - done;
			else
				offset = 0;
			zero_user(bv.bv_page, bv.bv_offset + offset,
				  bv.bv_len - offset);
			truncated = true;
		}
		done += bv.bv_len;
	}

 exit:
	 
	bio->bi_iter.bi_size = new_size;
}

 
void guard_bio_eod(struct bio *bio)
{
	sector_t maxsector = bdev_nr_sectors(bio->bi_bdev);

	if (!maxsector)
		return;

	 
	if (unlikely(bio->bi_iter.bi_sector >= maxsector))
		return;

	maxsector -= bio->bi_iter.bi_sector;
	if (likely((bio->bi_iter.bi_size >> 9) <= maxsector))
		return;

	bio_truncate(bio, maxsector << 9);
}

static int __bio_alloc_cache_prune(struct bio_alloc_cache *cache,
				   unsigned int nr)
{
	unsigned int i = 0;
	struct bio *bio;

	while ((bio = cache->free_list) != NULL) {
		cache->free_list = bio->bi_next;
		cache->nr--;
		bio_free(bio);
		if (++i == nr)
			break;
	}
	return i;
}

static void bio_alloc_cache_prune(struct bio_alloc_cache *cache,
				  unsigned int nr)
{
	nr -= __bio_alloc_cache_prune(cache, nr);
	if (!READ_ONCE(cache->free_list)) {
		bio_alloc_irq_cache_splice(cache);
		__bio_alloc_cache_prune(cache, nr);
	}
}

static int bio_cpu_dead(unsigned int cpu, struct hlist_node *node)
{
	struct bio_set *bs;

	bs = hlist_entry_safe(node, struct bio_set, cpuhp_dead);
	if (bs->cache) {
		struct bio_alloc_cache *cache = per_cpu_ptr(bs->cache, cpu);

		bio_alloc_cache_prune(cache, -1U);
	}
	return 0;
}

static void bio_alloc_cache_destroy(struct bio_set *bs)
{
	int cpu;

	if (!bs->cache)
		return;

	cpuhp_state_remove_instance_nocalls(CPUHP_BIO_DEAD, &bs->cpuhp_dead);
	for_each_possible_cpu(cpu) {
		struct bio_alloc_cache *cache;

		cache = per_cpu_ptr(bs->cache, cpu);
		bio_alloc_cache_prune(cache, -1U);
	}
	free_percpu(bs->cache);
	bs->cache = NULL;
}

static inline void bio_put_percpu_cache(struct bio *bio)
{
	struct bio_alloc_cache *cache;

	cache = per_cpu_ptr(bio->bi_pool->cache, get_cpu());
	if (READ_ONCE(cache->nr_irq) + cache->nr > ALLOC_CACHE_MAX) {
		put_cpu();
		bio_free(bio);
		return;
	}

	bio_uninit(bio);

	if ((bio->bi_opf & REQ_POLLED) && !WARN_ON_ONCE(in_interrupt())) {
		bio->bi_next = cache->free_list;
		bio->bi_bdev = NULL;
		cache->free_list = bio;
		cache->nr++;
	} else {
		unsigned long flags;

		local_irq_save(flags);
		bio->bi_next = cache->free_list_irq;
		cache->free_list_irq = bio;
		cache->nr_irq++;
		local_irq_restore(flags);
	}
	put_cpu();
}

 
void bio_put(struct bio *bio)
{
	if (unlikely(bio_flagged(bio, BIO_REFFED))) {
		BUG_ON(!atomic_read(&bio->__bi_cnt));
		if (!atomic_dec_and_test(&bio->__bi_cnt))
			return;
	}
	if (bio->bi_opf & REQ_ALLOC_CACHE)
		bio_put_percpu_cache(bio);
	else
		bio_free(bio);
}
EXPORT_SYMBOL(bio_put);

static int __bio_clone(struct bio *bio, struct bio *bio_src, gfp_t gfp)
{
	bio_set_flag(bio, BIO_CLONED);
	bio->bi_ioprio = bio_src->bi_ioprio;
	bio->bi_iter = bio_src->bi_iter;

	if (bio->bi_bdev) {
		if (bio->bi_bdev == bio_src->bi_bdev &&
		    bio_flagged(bio_src, BIO_REMAPPED))
			bio_set_flag(bio, BIO_REMAPPED);
		bio_clone_blkg_association(bio, bio_src);
	}

	if (bio_crypt_clone(bio, bio_src, gfp) < 0)
		return -ENOMEM;
	if (bio_integrity(bio_src) &&
	    bio_integrity_clone(bio, bio_src, gfp) < 0)
		return -ENOMEM;
	return 0;
}

 
struct bio *bio_alloc_clone(struct block_device *bdev, struct bio *bio_src,
		gfp_t gfp, struct bio_set *bs)
{
	struct bio *bio;

	bio = bio_alloc_bioset(bdev, 0, bio_src->bi_opf, gfp, bs);
	if (!bio)
		return NULL;

	if (__bio_clone(bio, bio_src, gfp) < 0) {
		bio_put(bio);
		return NULL;
	}
	bio->bi_io_vec = bio_src->bi_io_vec;

	return bio;
}
EXPORT_SYMBOL(bio_alloc_clone);

 
int bio_init_clone(struct block_device *bdev, struct bio *bio,
		struct bio *bio_src, gfp_t gfp)
{
	int ret;

	bio_init(bio, bdev, bio_src->bi_io_vec, 0, bio_src->bi_opf);
	ret = __bio_clone(bio, bio_src, gfp);
	if (ret)
		bio_uninit(bio);
	return ret;
}
EXPORT_SYMBOL(bio_init_clone);

 
static inline bool bio_full(struct bio *bio, unsigned len)
{
	if (bio->bi_vcnt >= bio->bi_max_vecs)
		return true;
	if (bio->bi_iter.bi_size > UINT_MAX - len)
		return true;
	return false;
}

static bool bvec_try_merge_page(struct bio_vec *bv, struct page *page,
		unsigned int len, unsigned int off, bool *same_page)
{
	size_t bv_end = bv->bv_offset + bv->bv_len;
	phys_addr_t vec_end_addr = page_to_phys(bv->bv_page) + bv_end - 1;
	phys_addr_t page_addr = page_to_phys(page);

	if (vec_end_addr + 1 != page_addr + off)
		return false;
	if (xen_domain() && !xen_biovec_phys_mergeable(bv, page))
		return false;
	if (!zone_device_pages_have_same_pgmap(bv->bv_page, page))
		return false;

	*same_page = ((vec_end_addr & PAGE_MASK) == page_addr);
	if (!*same_page) {
		if (IS_ENABLED(CONFIG_KMSAN))
			return false;
		if (bv->bv_page + bv_end / PAGE_SIZE != page + off / PAGE_SIZE)
			return false;
	}

	bv->bv_len += len;
	return true;
}

 
bool bvec_try_merge_hw_page(struct request_queue *q, struct bio_vec *bv,
		struct page *page, unsigned len, unsigned offset,
		bool *same_page)
{
	unsigned long mask = queue_segment_boundary(q);
	phys_addr_t addr1 = page_to_phys(bv->bv_page) + bv->bv_offset;
	phys_addr_t addr2 = page_to_phys(page) + offset + len - 1;

	if ((addr1 | mask) != (addr2 | mask))
		return false;
	if (bv->bv_len + len > queue_max_segment_size(q))
		return false;
	return bvec_try_merge_page(bv, page, len, offset, same_page);
}

 
int bio_add_hw_page(struct request_queue *q, struct bio *bio,
		struct page *page, unsigned int len, unsigned int offset,
		unsigned int max_sectors, bool *same_page)
{
	if (WARN_ON_ONCE(bio_flagged(bio, BIO_CLONED)))
		return 0;

	if (((bio->bi_iter.bi_size + len) >> SECTOR_SHIFT) > max_sectors)
		return 0;

	if (bio->bi_vcnt > 0) {
		struct bio_vec *bv = &bio->bi_io_vec[bio->bi_vcnt - 1];

		if (bvec_try_merge_hw_page(q, bv, page, len, offset,
				same_page)) {
			bio->bi_iter.bi_size += len;
			return len;
		}

		if (bio->bi_vcnt >=
		    min(bio->bi_max_vecs, queue_max_segments(q)))
			return 0;

		 
		if (bvec_gap_to_prev(&q->limits, bv, offset))
			return 0;
	}

	bvec_set_page(&bio->bi_io_vec[bio->bi_vcnt], page, len, offset);
	bio->bi_vcnt++;
	bio->bi_iter.bi_size += len;
	return len;
}

 
int bio_add_pc_page(struct request_queue *q, struct bio *bio,
		struct page *page, unsigned int len, unsigned int offset)
{
	bool same_page = false;
	return bio_add_hw_page(q, bio, page, len, offset,
			queue_max_hw_sectors(q), &same_page);
}
EXPORT_SYMBOL(bio_add_pc_page);

 
int bio_add_zone_append_page(struct bio *bio, struct page *page,
			     unsigned int len, unsigned int offset)
{
	struct request_queue *q = bdev_get_queue(bio->bi_bdev);
	bool same_page = false;

	if (WARN_ON_ONCE(bio_op(bio) != REQ_OP_ZONE_APPEND))
		return 0;

	if (WARN_ON_ONCE(!bdev_is_zoned(bio->bi_bdev)))
		return 0;

	return bio_add_hw_page(q, bio, page, len, offset,
			       queue_max_zone_append_sectors(q), &same_page);
}
EXPORT_SYMBOL_GPL(bio_add_zone_append_page);

 
void __bio_add_page(struct bio *bio, struct page *page,
		unsigned int len, unsigned int off)
{
	WARN_ON_ONCE(bio_flagged(bio, BIO_CLONED));
	WARN_ON_ONCE(bio_full(bio, len));

	bvec_set_page(&bio->bi_io_vec[bio->bi_vcnt], page, len, off);
	bio->bi_iter.bi_size += len;
	bio->bi_vcnt++;
}
EXPORT_SYMBOL_GPL(__bio_add_page);

 
int bio_add_page(struct bio *bio, struct page *page,
		 unsigned int len, unsigned int offset)
{
	bool same_page = false;

	if (WARN_ON_ONCE(bio_flagged(bio, BIO_CLONED)))
		return 0;
	if (bio->bi_iter.bi_size > UINT_MAX - len)
		return 0;

	if (bio->bi_vcnt > 0 &&
	    bvec_try_merge_page(&bio->bi_io_vec[bio->bi_vcnt - 1],
				page, len, offset, &same_page)) {
		bio->bi_iter.bi_size += len;
		return len;
	}

	if (bio->bi_vcnt >= bio->bi_max_vecs)
		return 0;
	__bio_add_page(bio, page, len, offset);
	return len;
}
EXPORT_SYMBOL(bio_add_page);

void bio_add_folio_nofail(struct bio *bio, struct folio *folio, size_t len,
			  size_t off)
{
	WARN_ON_ONCE(len > UINT_MAX);
	WARN_ON_ONCE(off > UINT_MAX);
	__bio_add_page(bio, &folio->page, len, off);
}

 
bool bio_add_folio(struct bio *bio, struct folio *folio, size_t len,
		   size_t off)
{
	if (len > UINT_MAX || off > UINT_MAX)
		return false;
	return bio_add_page(bio, &folio->page, len, off) > 0;
}
EXPORT_SYMBOL(bio_add_folio);

void __bio_release_pages(struct bio *bio, bool mark_dirty)
{
	struct folio_iter fi;

	bio_for_each_folio_all(fi, bio) {
		struct page *page;
		size_t done = 0;

		if (mark_dirty) {
			folio_lock(fi.folio);
			folio_mark_dirty(fi.folio);
			folio_unlock(fi.folio);
		}
		page = folio_page(fi.folio, fi.offset / PAGE_SIZE);
		do {
			bio_release_page(bio, page++);
			done += PAGE_SIZE;
		} while (done < fi.length);
	}
}
EXPORT_SYMBOL_GPL(__bio_release_pages);

void bio_iov_bvec_set(struct bio *bio, struct iov_iter *iter)
{
	size_t size = iov_iter_count(iter);

	WARN_ON_ONCE(bio->bi_max_vecs);

	if (bio_op(bio) == REQ_OP_ZONE_APPEND) {
		struct request_queue *q = bdev_get_queue(bio->bi_bdev);
		size_t max_sectors = queue_max_zone_append_sectors(q);

		size = min(size, max_sectors << SECTOR_SHIFT);
	}

	bio->bi_vcnt = iter->nr_segs;
	bio->bi_io_vec = (struct bio_vec *)iter->bvec;
	bio->bi_iter.bi_bvec_done = iter->iov_offset;
	bio->bi_iter.bi_size = size;
	bio_set_flag(bio, BIO_CLONED);
}

static int bio_iov_add_page(struct bio *bio, struct page *page,
		unsigned int len, unsigned int offset)
{
	bool same_page = false;

	if (WARN_ON_ONCE(bio->bi_iter.bi_size > UINT_MAX - len))
		return -EIO;

	if (bio->bi_vcnt > 0 &&
	    bvec_try_merge_page(&bio->bi_io_vec[bio->bi_vcnt - 1],
				page, len, offset, &same_page)) {
		bio->bi_iter.bi_size += len;
		if (same_page)
			bio_release_page(bio, page);
		return 0;
	}
	__bio_add_page(bio, page, len, offset);
	return 0;
}

static int bio_iov_add_zone_append_page(struct bio *bio, struct page *page,
		unsigned int len, unsigned int offset)
{
	struct request_queue *q = bdev_get_queue(bio->bi_bdev);
	bool same_page = false;

	if (bio_add_hw_page(q, bio, page, len, offset,
			queue_max_zone_append_sectors(q), &same_page) != len)
		return -EINVAL;
	if (same_page)
		bio_release_page(bio, page);
	return 0;
}

#define PAGE_PTRS_PER_BVEC     (sizeof(struct bio_vec) / sizeof(struct page *))

 
static int __bio_iov_iter_get_pages(struct bio *bio, struct iov_iter *iter)
{
	iov_iter_extraction_t extraction_flags = 0;
	unsigned short nr_pages = bio->bi_max_vecs - bio->bi_vcnt;
	unsigned short entries_left = bio->bi_max_vecs - bio->bi_vcnt;
	struct bio_vec *bv = bio->bi_io_vec + bio->bi_vcnt;
	struct page **pages = (struct page **)bv;
	ssize_t size, left;
	unsigned len, i = 0;
	size_t offset;
	int ret = 0;

	 
	BUILD_BUG_ON(PAGE_PTRS_PER_BVEC < 2);
	pages += entries_left * (PAGE_PTRS_PER_BVEC - 1);

	if (bio->bi_bdev && blk_queue_pci_p2pdma(bio->bi_bdev->bd_disk->queue))
		extraction_flags |= ITER_ALLOW_P2PDMA;

	 
	size = iov_iter_extract_pages(iter, &pages,
				      UINT_MAX - bio->bi_iter.bi_size,
				      nr_pages, extraction_flags, &offset);
	if (unlikely(size <= 0))
		return size ? size : -EFAULT;

	nr_pages = DIV_ROUND_UP(offset + size, PAGE_SIZE);

	if (bio->bi_bdev) {
		size_t trim = size & (bdev_logical_block_size(bio->bi_bdev) - 1);
		iov_iter_revert(iter, trim);
		size -= trim;
	}

	if (unlikely(!size)) {
		ret = -EFAULT;
		goto out;
	}

	for (left = size, i = 0; left > 0; left -= len, i++) {
		struct page *page = pages[i];

		len = min_t(size_t, PAGE_SIZE - offset, left);
		if (bio_op(bio) == REQ_OP_ZONE_APPEND) {
			ret = bio_iov_add_zone_append_page(bio, page, len,
					offset);
			if (ret)
				break;
		} else
			bio_iov_add_page(bio, page, len, offset);

		offset = 0;
	}

	iov_iter_revert(iter, left);
out:
	while (i < nr_pages)
		bio_release_page(bio, pages[i++]);

	return ret;
}

 
int bio_iov_iter_get_pages(struct bio *bio, struct iov_iter *iter)
{
	int ret = 0;

	if (WARN_ON_ONCE(bio_flagged(bio, BIO_CLONED)))
		return -EIO;

	if (iov_iter_is_bvec(iter)) {
		bio_iov_bvec_set(bio, iter);
		iov_iter_advance(iter, bio->bi_iter.bi_size);
		return 0;
	}

	if (iov_iter_extract_will_pin(iter))
		bio_set_flag(bio, BIO_PAGE_PINNED);
	do {
		ret = __bio_iov_iter_get_pages(bio, iter);
	} while (!ret && iov_iter_count(iter) && !bio_full(bio, 0));

	return bio->bi_vcnt ? 0 : ret;
}
EXPORT_SYMBOL_GPL(bio_iov_iter_get_pages);

static void submit_bio_wait_endio(struct bio *bio)
{
	complete(bio->bi_private);
}

 
int submit_bio_wait(struct bio *bio)
{
	DECLARE_COMPLETION_ONSTACK_MAP(done,
			bio->bi_bdev->bd_disk->lockdep_map);
	unsigned long hang_check;

	bio->bi_private = &done;
	bio->bi_end_io = submit_bio_wait_endio;
	bio->bi_opf |= REQ_SYNC;
	submit_bio(bio);

	 
	hang_check = sysctl_hung_task_timeout_secs;
	if (hang_check)
		while (!wait_for_completion_io_timeout(&done,
					hang_check * (HZ/2)))
			;
	else
		wait_for_completion_io(&done);

	return blk_status_to_errno(bio->bi_status);
}
EXPORT_SYMBOL(submit_bio_wait);

void __bio_advance(struct bio *bio, unsigned bytes)
{
	if (bio_integrity(bio))
		bio_integrity_advance(bio, bytes);

	bio_crypt_advance(bio, bytes);
	bio_advance_iter(bio, &bio->bi_iter, bytes);
}
EXPORT_SYMBOL(__bio_advance);

void bio_copy_data_iter(struct bio *dst, struct bvec_iter *dst_iter,
			struct bio *src, struct bvec_iter *src_iter)
{
	while (src_iter->bi_size && dst_iter->bi_size) {
		struct bio_vec src_bv = bio_iter_iovec(src, *src_iter);
		struct bio_vec dst_bv = bio_iter_iovec(dst, *dst_iter);
		unsigned int bytes = min(src_bv.bv_len, dst_bv.bv_len);
		void *src_buf = bvec_kmap_local(&src_bv);
		void *dst_buf = bvec_kmap_local(&dst_bv);

		memcpy(dst_buf, src_buf, bytes);

		kunmap_local(dst_buf);
		kunmap_local(src_buf);

		bio_advance_iter_single(src, src_iter, bytes);
		bio_advance_iter_single(dst, dst_iter, bytes);
	}
}
EXPORT_SYMBOL(bio_copy_data_iter);

 
void bio_copy_data(struct bio *dst, struct bio *src)
{
	struct bvec_iter src_iter = src->bi_iter;
	struct bvec_iter dst_iter = dst->bi_iter;

	bio_copy_data_iter(dst, &dst_iter, src, &src_iter);
}
EXPORT_SYMBOL(bio_copy_data);

void bio_free_pages(struct bio *bio)
{
	struct bio_vec *bvec;
	struct bvec_iter_all iter_all;

	bio_for_each_segment_all(bvec, bio, iter_all)
		__free_page(bvec->bv_page);
}
EXPORT_SYMBOL(bio_free_pages);

 

 
void bio_set_pages_dirty(struct bio *bio)
{
	struct folio_iter fi;

	bio_for_each_folio_all(fi, bio) {
		folio_lock(fi.folio);
		folio_mark_dirty(fi.folio);
		folio_unlock(fi.folio);
	}
}
EXPORT_SYMBOL_GPL(bio_set_pages_dirty);

 

static void bio_dirty_fn(struct work_struct *work);

static DECLARE_WORK(bio_dirty_work, bio_dirty_fn);
static DEFINE_SPINLOCK(bio_dirty_lock);
static struct bio *bio_dirty_list;

 
static void bio_dirty_fn(struct work_struct *work)
{
	struct bio *bio, *next;

	spin_lock_irq(&bio_dirty_lock);
	next = bio_dirty_list;
	bio_dirty_list = NULL;
	spin_unlock_irq(&bio_dirty_lock);

	while ((bio = next) != NULL) {
		next = bio->bi_private;

		bio_release_pages(bio, true);
		bio_put(bio);
	}
}

void bio_check_pages_dirty(struct bio *bio)
{
	struct folio_iter fi;
	unsigned long flags;

	bio_for_each_folio_all(fi, bio) {
		if (!folio_test_dirty(fi.folio))
			goto defer;
	}

	bio_release_pages(bio, false);
	bio_put(bio);
	return;
defer:
	spin_lock_irqsave(&bio_dirty_lock, flags);
	bio->bi_private = bio_dirty_list;
	bio_dirty_list = bio;
	spin_unlock_irqrestore(&bio_dirty_lock, flags);
	schedule_work(&bio_dirty_work);
}
EXPORT_SYMBOL_GPL(bio_check_pages_dirty);

static inline bool bio_remaining_done(struct bio *bio)
{
	 
	if (!bio_flagged(bio, BIO_CHAIN))
		return true;

	BUG_ON(atomic_read(&bio->__bi_remaining) <= 0);

	if (atomic_dec_and_test(&bio->__bi_remaining)) {
		bio_clear_flag(bio, BIO_CHAIN);
		return true;
	}

	return false;
}

 
void bio_endio(struct bio *bio)
{
again:
	if (!bio_remaining_done(bio))
		return;
	if (!bio_integrity_endio(bio))
		return;

	rq_qos_done_bio(bio);

	if (bio->bi_bdev && bio_flagged(bio, BIO_TRACE_COMPLETION)) {
		trace_block_bio_complete(bdev_get_queue(bio->bi_bdev), bio);
		bio_clear_flag(bio, BIO_TRACE_COMPLETION);
	}

	 
	if (bio->bi_end_io == bio_chain_endio) {
		bio = __bio_chain_endio(bio);
		goto again;
	}

	blk_throtl_bio_endio(bio);
	 
	bio_uninit(bio);
	if (bio->bi_end_io)
		bio->bi_end_io(bio);
}
EXPORT_SYMBOL(bio_endio);

 
struct bio *bio_split(struct bio *bio, int sectors,
		      gfp_t gfp, struct bio_set *bs)
{
	struct bio *split;

	BUG_ON(sectors <= 0);
	BUG_ON(sectors >= bio_sectors(bio));

	 
	if (WARN_ON_ONCE(bio_op(bio) == REQ_OP_ZONE_APPEND))
		return NULL;

	split = bio_alloc_clone(bio->bi_bdev, bio, gfp, bs);
	if (!split)
		return NULL;

	split->bi_iter.bi_size = sectors << 9;

	if (bio_integrity(split))
		bio_integrity_trim(split);

	bio_advance(bio, split->bi_iter.bi_size);

	if (bio_flagged(bio, BIO_TRACE_COMPLETION))
		bio_set_flag(split, BIO_TRACE_COMPLETION);

	return split;
}
EXPORT_SYMBOL(bio_split);

 
void bio_trim(struct bio *bio, sector_t offset, sector_t size)
{
	if (WARN_ON_ONCE(offset > BIO_MAX_SECTORS || size > BIO_MAX_SECTORS ||
			 offset + size > bio_sectors(bio)))
		return;

	size <<= 9;
	if (offset == 0 && size == bio->bi_iter.bi_size)
		return;

	bio_advance(bio, offset << 9);
	bio->bi_iter.bi_size = size;

	if (bio_integrity(bio))
		bio_integrity_trim(bio);
}
EXPORT_SYMBOL_GPL(bio_trim);

 
int biovec_init_pool(mempool_t *pool, int pool_entries)
{
	struct biovec_slab *bp = bvec_slabs + ARRAY_SIZE(bvec_slabs) - 1;

	return mempool_init_slab_pool(pool, pool_entries, bp->slab);
}

 
void bioset_exit(struct bio_set *bs)
{
	bio_alloc_cache_destroy(bs);
	if (bs->rescue_workqueue)
		destroy_workqueue(bs->rescue_workqueue);
	bs->rescue_workqueue = NULL;

	mempool_exit(&bs->bio_pool);
	mempool_exit(&bs->bvec_pool);

	bioset_integrity_free(bs);
	if (bs->bio_slab)
		bio_put_slab(bs);
	bs->bio_slab = NULL;
}
EXPORT_SYMBOL(bioset_exit);

 
int bioset_init(struct bio_set *bs,
		unsigned int pool_size,
		unsigned int front_pad,
		int flags)
{
	bs->front_pad = front_pad;
	if (flags & BIOSET_NEED_BVECS)
		bs->back_pad = BIO_INLINE_VECS * sizeof(struct bio_vec);
	else
		bs->back_pad = 0;

	spin_lock_init(&bs->rescue_lock);
	bio_list_init(&bs->rescue_list);
	INIT_WORK(&bs->rescue_work, bio_alloc_rescue);

	bs->bio_slab = bio_find_or_create_slab(bs);
	if (!bs->bio_slab)
		return -ENOMEM;

	if (mempool_init_slab_pool(&bs->bio_pool, pool_size, bs->bio_slab))
		goto bad;

	if ((flags & BIOSET_NEED_BVECS) &&
	    biovec_init_pool(&bs->bvec_pool, pool_size))
		goto bad;

	if (flags & BIOSET_NEED_RESCUER) {
		bs->rescue_workqueue = alloc_workqueue("bioset",
							WQ_MEM_RECLAIM, 0);
		if (!bs->rescue_workqueue)
			goto bad;
	}
	if (flags & BIOSET_PERCPU_CACHE) {
		bs->cache = alloc_percpu(struct bio_alloc_cache);
		if (!bs->cache)
			goto bad;
		cpuhp_state_add_instance_nocalls(CPUHP_BIO_DEAD, &bs->cpuhp_dead);
	}

	return 0;
bad:
	bioset_exit(bs);
	return -ENOMEM;
}
EXPORT_SYMBOL(bioset_init);

static int __init init_bio(void)
{
	int i;

	BUILD_BUG_ON(BIO_FLAG_LAST > 8 * sizeof_field(struct bio, bi_flags));

	bio_integrity_init();

	for (i = 0; i < ARRAY_SIZE(bvec_slabs); i++) {
		struct biovec_slab *bvs = bvec_slabs + i;

		bvs->slab = kmem_cache_create(bvs->name,
				bvs->nr_vecs * sizeof(struct bio_vec), 0,
				SLAB_HWCACHE_ALIGN | SLAB_PANIC, NULL);
	}

	cpuhp_setup_state_multi(CPUHP_BIO_DEAD, "block/bio:dead", NULL,
					bio_cpu_dead);

	if (bioset_init(&fs_bio_set, BIO_POOL_SIZE, 0,
			BIOSET_NEED_BVECS | BIOSET_PERCPU_CACHE))
		panic("bio: can't allocate bios\n");

	if (bioset_integrity_create(&fs_bio_set, BIO_POOL_SIZE))
		panic("bio: can't create integrity pool\n");

	return 0;
}
subsys_initcall(init_bio);

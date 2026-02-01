
 

#include <linux/sched.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/raid/pq.h>
#include <linux/hash.h>
#include <linux/list_sort.h>
#include <linux/raid/xor.h>
#include <linux/mm.h>
#include "messages.h"
#include "misc.h"
#include "ctree.h"
#include "disk-io.h"
#include "volumes.h"
#include "raid56.h"
#include "async-thread.h"
#include "file-item.h"
#include "btrfs_inode.h"

 
#define RBIO_RMW_LOCKED_BIT	1

 
#define RBIO_CACHE_BIT		2

 
#define RBIO_CACHE_READY_BIT	3

#define RBIO_CACHE_SIZE 1024

#define BTRFS_STRIPE_HASH_TABLE_BITS				11

 
struct btrfs_stripe_hash {
	struct list_head hash_list;
	spinlock_t lock;
};

 
struct btrfs_stripe_hash_table {
	struct list_head stripe_cache;
	spinlock_t cache_lock;
	int cache_size;
	struct btrfs_stripe_hash table[];
};

 
struct sector_ptr {
	struct page *page;
	unsigned int pgoff:24;
	unsigned int uptodate:8;
};

static void rmw_rbio_work(struct work_struct *work);
static void rmw_rbio_work_locked(struct work_struct *work);
static void index_rbio_pages(struct btrfs_raid_bio *rbio);
static int alloc_rbio_pages(struct btrfs_raid_bio *rbio);

static int finish_parity_scrub(struct btrfs_raid_bio *rbio);
static void scrub_rbio_work_locked(struct work_struct *work);

static void free_raid_bio_pointers(struct btrfs_raid_bio *rbio)
{
	bitmap_free(rbio->error_bitmap);
	kfree(rbio->stripe_pages);
	kfree(rbio->bio_sectors);
	kfree(rbio->stripe_sectors);
	kfree(rbio->finish_pointers);
}

static void free_raid_bio(struct btrfs_raid_bio *rbio)
{
	int i;

	if (!refcount_dec_and_test(&rbio->refs))
		return;

	WARN_ON(!list_empty(&rbio->stripe_cache));
	WARN_ON(!list_empty(&rbio->hash_list));
	WARN_ON(!bio_list_empty(&rbio->bio_list));

	for (i = 0; i < rbio->nr_pages; i++) {
		if (rbio->stripe_pages[i]) {
			__free_page(rbio->stripe_pages[i]);
			rbio->stripe_pages[i] = NULL;
		}
	}

	btrfs_put_bioc(rbio->bioc);
	free_raid_bio_pointers(rbio);
	kfree(rbio);
}

static void start_async_work(struct btrfs_raid_bio *rbio, work_func_t work_func)
{
	INIT_WORK(&rbio->work, work_func);
	queue_work(rbio->bioc->fs_info->rmw_workers, &rbio->work);
}

 
int btrfs_alloc_stripe_hash_table(struct btrfs_fs_info *info)
{
	struct btrfs_stripe_hash_table *table;
	struct btrfs_stripe_hash_table *x;
	struct btrfs_stripe_hash *cur;
	struct btrfs_stripe_hash *h;
	int num_entries = 1 << BTRFS_STRIPE_HASH_TABLE_BITS;
	int i;

	if (info->stripe_hash_table)
		return 0;

	 
	table = kvzalloc(struct_size(table, table, num_entries), GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	spin_lock_init(&table->cache_lock);
	INIT_LIST_HEAD(&table->stripe_cache);

	h = table->table;

	for (i = 0; i < num_entries; i++) {
		cur = h + i;
		INIT_LIST_HEAD(&cur->hash_list);
		spin_lock_init(&cur->lock);
	}

	x = cmpxchg(&info->stripe_hash_table, NULL, table);
	kvfree(x);
	return 0;
}

 
static void cache_rbio_pages(struct btrfs_raid_bio *rbio)
{
	int i;
	int ret;

	ret = alloc_rbio_pages(rbio);
	if (ret)
		return;

	for (i = 0; i < rbio->nr_sectors; i++) {
		 
		if (!rbio->bio_sectors[i].page) {
			 
			if (i < rbio->nr_data * rbio->stripe_nsectors)
				ASSERT(rbio->stripe_sectors[i].uptodate);
			continue;
		}

		ASSERT(rbio->stripe_sectors[i].page);
		memcpy_page(rbio->stripe_sectors[i].page,
			    rbio->stripe_sectors[i].pgoff,
			    rbio->bio_sectors[i].page,
			    rbio->bio_sectors[i].pgoff,
			    rbio->bioc->fs_info->sectorsize);
		rbio->stripe_sectors[i].uptodate = 1;
	}
	set_bit(RBIO_CACHE_READY_BIT, &rbio->flags);
}

 
static int rbio_bucket(struct btrfs_raid_bio *rbio)
{
	u64 num = rbio->bioc->full_stripe_logical;

	 
	return hash_64(num >> 16, BTRFS_STRIPE_HASH_TABLE_BITS);
}

static bool full_page_sectors_uptodate(struct btrfs_raid_bio *rbio,
				       unsigned int page_nr)
{
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	const u32 sectors_per_page = PAGE_SIZE / sectorsize;
	int i;

	ASSERT(page_nr < rbio->nr_pages);

	for (i = sectors_per_page * page_nr;
	     i < sectors_per_page * page_nr + sectors_per_page;
	     i++) {
		if (!rbio->stripe_sectors[i].uptodate)
			return false;
	}
	return true;
}

 
static void index_stripe_sectors(struct btrfs_raid_bio *rbio)
{
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	u32 offset;
	int i;

	for (i = 0, offset = 0; i < rbio->nr_sectors; i++, offset += sectorsize) {
		int page_index = offset >> PAGE_SHIFT;

		ASSERT(page_index < rbio->nr_pages);
		rbio->stripe_sectors[i].page = rbio->stripe_pages[page_index];
		rbio->stripe_sectors[i].pgoff = offset_in_page(offset);
	}
}

static void steal_rbio_page(struct btrfs_raid_bio *src,
			    struct btrfs_raid_bio *dest, int page_nr)
{
	const u32 sectorsize = src->bioc->fs_info->sectorsize;
	const u32 sectors_per_page = PAGE_SIZE / sectorsize;
	int i;

	if (dest->stripe_pages[page_nr])
		__free_page(dest->stripe_pages[page_nr]);
	dest->stripe_pages[page_nr] = src->stripe_pages[page_nr];
	src->stripe_pages[page_nr] = NULL;

	 
	for (i = sectors_per_page * page_nr;
	     i < sectors_per_page * page_nr + sectors_per_page; i++)
		dest->stripe_sectors[i].uptodate = true;
}

static bool is_data_stripe_page(struct btrfs_raid_bio *rbio, int page_nr)
{
	const int sector_nr = (page_nr << PAGE_SHIFT) >>
			      rbio->bioc->fs_info->sectorsize_bits;

	 
	return (sector_nr < rbio->nr_data * rbio->stripe_nsectors);
}

 
static void steal_rbio(struct btrfs_raid_bio *src, struct btrfs_raid_bio *dest)
{
	int i;

	if (!test_bit(RBIO_CACHE_READY_BIT, &src->flags))
		return;

	for (i = 0; i < dest->nr_pages; i++) {
		struct page *p = src->stripe_pages[i];

		 
		if (!is_data_stripe_page(src, i))
			continue;

		 
		ASSERT(p);
		ASSERT(full_page_sectors_uptodate(src, i));
		steal_rbio_page(src, dest, i);
	}
	index_stripe_sectors(dest);
	index_stripe_sectors(src);
}

 
static void merge_rbio(struct btrfs_raid_bio *dest,
		       struct btrfs_raid_bio *victim)
{
	bio_list_merge(&dest->bio_list, &victim->bio_list);
	dest->bio_list_bytes += victim->bio_list_bytes;
	 
	bitmap_or(&dest->dbitmap, &victim->dbitmap, &dest->dbitmap,
		  dest->stripe_nsectors);
	bio_list_init(&victim->bio_list);
}

 
static void __remove_rbio_from_cache(struct btrfs_raid_bio *rbio)
{
	int bucket = rbio_bucket(rbio);
	struct btrfs_stripe_hash_table *table;
	struct btrfs_stripe_hash *h;
	int freeit = 0;

	 
	if (!test_bit(RBIO_CACHE_BIT, &rbio->flags))
		return;

	table = rbio->bioc->fs_info->stripe_hash_table;
	h = table->table + bucket;

	 
	spin_lock(&h->lock);

	 
	spin_lock(&rbio->bio_list_lock);

	if (test_and_clear_bit(RBIO_CACHE_BIT, &rbio->flags)) {
		list_del_init(&rbio->stripe_cache);
		table->cache_size -= 1;
		freeit = 1;

		 
		if (bio_list_empty(&rbio->bio_list)) {
			if (!list_empty(&rbio->hash_list)) {
				list_del_init(&rbio->hash_list);
				refcount_dec(&rbio->refs);
				BUG_ON(!list_empty(&rbio->plug_list));
			}
		}
	}

	spin_unlock(&rbio->bio_list_lock);
	spin_unlock(&h->lock);

	if (freeit)
		free_raid_bio(rbio);
}

 
static void remove_rbio_from_cache(struct btrfs_raid_bio *rbio)
{
	struct btrfs_stripe_hash_table *table;

	if (!test_bit(RBIO_CACHE_BIT, &rbio->flags))
		return;

	table = rbio->bioc->fs_info->stripe_hash_table;

	spin_lock(&table->cache_lock);
	__remove_rbio_from_cache(rbio);
	spin_unlock(&table->cache_lock);
}

 
static void btrfs_clear_rbio_cache(struct btrfs_fs_info *info)
{
	struct btrfs_stripe_hash_table *table;
	struct btrfs_raid_bio *rbio;

	table = info->stripe_hash_table;

	spin_lock(&table->cache_lock);
	while (!list_empty(&table->stripe_cache)) {
		rbio = list_entry(table->stripe_cache.next,
				  struct btrfs_raid_bio,
				  stripe_cache);
		__remove_rbio_from_cache(rbio);
	}
	spin_unlock(&table->cache_lock);
}

 
void btrfs_free_stripe_hash_table(struct btrfs_fs_info *info)
{
	if (!info->stripe_hash_table)
		return;
	btrfs_clear_rbio_cache(info);
	kvfree(info->stripe_hash_table);
	info->stripe_hash_table = NULL;
}

 
static void cache_rbio(struct btrfs_raid_bio *rbio)
{
	struct btrfs_stripe_hash_table *table;

	if (!test_bit(RBIO_CACHE_READY_BIT, &rbio->flags))
		return;

	table = rbio->bioc->fs_info->stripe_hash_table;

	spin_lock(&table->cache_lock);
	spin_lock(&rbio->bio_list_lock);

	 
	if (!test_and_set_bit(RBIO_CACHE_BIT, &rbio->flags))
		refcount_inc(&rbio->refs);

	if (!list_empty(&rbio->stripe_cache)){
		list_move(&rbio->stripe_cache, &table->stripe_cache);
	} else {
		list_add(&rbio->stripe_cache, &table->stripe_cache);
		table->cache_size += 1;
	}

	spin_unlock(&rbio->bio_list_lock);

	if (table->cache_size > RBIO_CACHE_SIZE) {
		struct btrfs_raid_bio *found;

		found = list_entry(table->stripe_cache.prev,
				  struct btrfs_raid_bio,
				  stripe_cache);

		if (found != rbio)
			__remove_rbio_from_cache(found);
	}

	spin_unlock(&table->cache_lock);
}

 
static void run_xor(void **pages, int src_cnt, ssize_t len)
{
	int src_off = 0;
	int xor_src_cnt = 0;
	void *dest = pages[src_cnt];

	while(src_cnt > 0) {
		xor_src_cnt = min(src_cnt, MAX_XOR_BLOCKS);
		xor_blocks(xor_src_cnt, len, dest, pages + src_off);

		src_cnt -= xor_src_cnt;
		src_off += xor_src_cnt;
	}
}

 
static int rbio_is_full(struct btrfs_raid_bio *rbio)
{
	unsigned long size = rbio->bio_list_bytes;
	int ret = 1;

	spin_lock(&rbio->bio_list_lock);
	if (size != rbio->nr_data * BTRFS_STRIPE_LEN)
		ret = 0;
	BUG_ON(size > rbio->nr_data * BTRFS_STRIPE_LEN);
	spin_unlock(&rbio->bio_list_lock);

	return ret;
}

 
static int rbio_can_merge(struct btrfs_raid_bio *last,
			  struct btrfs_raid_bio *cur)
{
	if (test_bit(RBIO_RMW_LOCKED_BIT, &last->flags) ||
	    test_bit(RBIO_RMW_LOCKED_BIT, &cur->flags))
		return 0;

	 
	if (test_bit(RBIO_CACHE_BIT, &last->flags) ||
	    test_bit(RBIO_CACHE_BIT, &cur->flags))
		return 0;

	if (last->bioc->full_stripe_logical != cur->bioc->full_stripe_logical)
		return 0;

	 
	if (last->operation != cur->operation)
		return 0;
	 
	if (last->operation == BTRFS_RBIO_PARITY_SCRUB)
		return 0;

	if (last->operation == BTRFS_RBIO_READ_REBUILD)
		return 0;

	return 1;
}

static unsigned int rbio_stripe_sector_index(const struct btrfs_raid_bio *rbio,
					     unsigned int stripe_nr,
					     unsigned int sector_nr)
{
	ASSERT(stripe_nr < rbio->real_stripes);
	ASSERT(sector_nr < rbio->stripe_nsectors);

	return stripe_nr * rbio->stripe_nsectors + sector_nr;
}

 
static struct sector_ptr *rbio_stripe_sector(const struct btrfs_raid_bio *rbio,
					     unsigned int stripe_nr,
					     unsigned int sector_nr)
{
	return &rbio->stripe_sectors[rbio_stripe_sector_index(rbio, stripe_nr,
							      sector_nr)];
}

 
static struct sector_ptr *rbio_pstripe_sector(const struct btrfs_raid_bio *rbio,
					      unsigned int sector_nr)
{
	return rbio_stripe_sector(rbio, rbio->nr_data, sector_nr);
}

 
static struct sector_ptr *rbio_qstripe_sector(const struct btrfs_raid_bio *rbio,
					      unsigned int sector_nr)
{
	if (rbio->nr_data + 1 == rbio->real_stripes)
		return NULL;
	return rbio_stripe_sector(rbio, rbio->nr_data + 1, sector_nr);
}

 
static noinline int lock_stripe_add(struct btrfs_raid_bio *rbio)
{
	struct btrfs_stripe_hash *h;
	struct btrfs_raid_bio *cur;
	struct btrfs_raid_bio *pending;
	struct btrfs_raid_bio *freeit = NULL;
	struct btrfs_raid_bio *cache_drop = NULL;
	int ret = 0;

	h = rbio->bioc->fs_info->stripe_hash_table->table + rbio_bucket(rbio);

	spin_lock(&h->lock);
	list_for_each_entry(cur, &h->hash_list, hash_list) {
		if (cur->bioc->full_stripe_logical != rbio->bioc->full_stripe_logical)
			continue;

		spin_lock(&cur->bio_list_lock);

		 
		if (bio_list_empty(&cur->bio_list) &&
		    list_empty(&cur->plug_list) &&
		    test_bit(RBIO_CACHE_BIT, &cur->flags) &&
		    !test_bit(RBIO_RMW_LOCKED_BIT, &cur->flags)) {
			list_del_init(&cur->hash_list);
			refcount_dec(&cur->refs);

			steal_rbio(cur, rbio);
			cache_drop = cur;
			spin_unlock(&cur->bio_list_lock);

			goto lockit;
		}

		 
		if (rbio_can_merge(cur, rbio)) {
			merge_rbio(cur, rbio);
			spin_unlock(&cur->bio_list_lock);
			freeit = rbio;
			ret = 1;
			goto out;
		}


		 
		list_for_each_entry(pending, &cur->plug_list, plug_list) {
			if (rbio_can_merge(pending, rbio)) {
				merge_rbio(pending, rbio);
				spin_unlock(&cur->bio_list_lock);
				freeit = rbio;
				ret = 1;
				goto out;
			}
		}

		 
		list_add_tail(&rbio->plug_list, &cur->plug_list);
		spin_unlock(&cur->bio_list_lock);
		ret = 1;
		goto out;
	}
lockit:
	refcount_inc(&rbio->refs);
	list_add(&rbio->hash_list, &h->hash_list);
out:
	spin_unlock(&h->lock);
	if (cache_drop)
		remove_rbio_from_cache(cache_drop);
	if (freeit)
		free_raid_bio(freeit);
	return ret;
}

static void recover_rbio_work_locked(struct work_struct *work);

 
static noinline void unlock_stripe(struct btrfs_raid_bio *rbio)
{
	int bucket;
	struct btrfs_stripe_hash *h;
	int keep_cache = 0;

	bucket = rbio_bucket(rbio);
	h = rbio->bioc->fs_info->stripe_hash_table->table + bucket;

	if (list_empty(&rbio->plug_list))
		cache_rbio(rbio);

	spin_lock(&h->lock);
	spin_lock(&rbio->bio_list_lock);

	if (!list_empty(&rbio->hash_list)) {
		 
		if (list_empty(&rbio->plug_list) &&
		    test_bit(RBIO_CACHE_BIT, &rbio->flags)) {
			keep_cache = 1;
			clear_bit(RBIO_RMW_LOCKED_BIT, &rbio->flags);
			BUG_ON(!bio_list_empty(&rbio->bio_list));
			goto done;
		}

		list_del_init(&rbio->hash_list);
		refcount_dec(&rbio->refs);

		 
		if (!list_empty(&rbio->plug_list)) {
			struct btrfs_raid_bio *next;
			struct list_head *head = rbio->plug_list.next;

			next = list_entry(head, struct btrfs_raid_bio,
					  plug_list);

			list_del_init(&rbio->plug_list);

			list_add(&next->hash_list, &h->hash_list);
			refcount_inc(&next->refs);
			spin_unlock(&rbio->bio_list_lock);
			spin_unlock(&h->lock);

			if (next->operation == BTRFS_RBIO_READ_REBUILD) {
				start_async_work(next, recover_rbio_work_locked);
			} else if (next->operation == BTRFS_RBIO_WRITE) {
				steal_rbio(rbio, next);
				start_async_work(next, rmw_rbio_work_locked);
			} else if (next->operation == BTRFS_RBIO_PARITY_SCRUB) {
				steal_rbio(rbio, next);
				start_async_work(next, scrub_rbio_work_locked);
			}

			goto done_nolock;
		}
	}
done:
	spin_unlock(&rbio->bio_list_lock);
	spin_unlock(&h->lock);

done_nolock:
	if (!keep_cache)
		remove_rbio_from_cache(rbio);
}

static void rbio_endio_bio_list(struct bio *cur, blk_status_t err)
{
	struct bio *next;

	while (cur) {
		next = cur->bi_next;
		cur->bi_next = NULL;
		cur->bi_status = err;
		bio_endio(cur);
		cur = next;
	}
}

 
static void rbio_orig_end_io(struct btrfs_raid_bio *rbio, blk_status_t err)
{
	struct bio *cur = bio_list_get(&rbio->bio_list);
	struct bio *extra;

	kfree(rbio->csum_buf);
	bitmap_free(rbio->csum_bitmap);
	rbio->csum_buf = NULL;
	rbio->csum_bitmap = NULL;

	 
	bitmap_clear(&rbio->dbitmap, 0, rbio->stripe_nsectors);

	 
	unlock_stripe(rbio);
	extra = bio_list_get(&rbio->bio_list);
	free_raid_bio(rbio);

	rbio_endio_bio_list(cur, err);
	if (extra)
		rbio_endio_bio_list(extra, err);
}

 
static struct sector_ptr *sector_in_rbio(struct btrfs_raid_bio *rbio,
					 int stripe_nr, int sector_nr,
					 bool bio_list_only)
{
	struct sector_ptr *sector;
	int index;

	ASSERT(stripe_nr >= 0 && stripe_nr < rbio->real_stripes);
	ASSERT(sector_nr >= 0 && sector_nr < rbio->stripe_nsectors);

	index = stripe_nr * rbio->stripe_nsectors + sector_nr;
	ASSERT(index >= 0 && index < rbio->nr_sectors);

	spin_lock(&rbio->bio_list_lock);
	sector = &rbio->bio_sectors[index];
	if (sector->page || bio_list_only) {
		 
		if (!sector->page)
			sector = NULL;
		spin_unlock(&rbio->bio_list_lock);
		return sector;
	}
	spin_unlock(&rbio->bio_list_lock);

	return &rbio->stripe_sectors[index];
}

 
static struct btrfs_raid_bio *alloc_rbio(struct btrfs_fs_info *fs_info,
					 struct btrfs_io_context *bioc)
{
	const unsigned int real_stripes = bioc->num_stripes - bioc->replace_nr_stripes;
	const unsigned int stripe_npages = BTRFS_STRIPE_LEN >> PAGE_SHIFT;
	const unsigned int num_pages = stripe_npages * real_stripes;
	const unsigned int stripe_nsectors =
		BTRFS_STRIPE_LEN >> fs_info->sectorsize_bits;
	const unsigned int num_sectors = stripe_nsectors * real_stripes;
	struct btrfs_raid_bio *rbio;

	 
	ASSERT(IS_ALIGNED(PAGE_SIZE, fs_info->sectorsize));
	 
	ASSERT(stripe_nsectors <= BITS_PER_LONG);

	rbio = kzalloc(sizeof(*rbio), GFP_NOFS);
	if (!rbio)
		return ERR_PTR(-ENOMEM);
	rbio->stripe_pages = kcalloc(num_pages, sizeof(struct page *),
				     GFP_NOFS);
	rbio->bio_sectors = kcalloc(num_sectors, sizeof(struct sector_ptr),
				    GFP_NOFS);
	rbio->stripe_sectors = kcalloc(num_sectors, sizeof(struct sector_ptr),
				       GFP_NOFS);
	rbio->finish_pointers = kcalloc(real_stripes, sizeof(void *), GFP_NOFS);
	rbio->error_bitmap = bitmap_zalloc(num_sectors, GFP_NOFS);

	if (!rbio->stripe_pages || !rbio->bio_sectors || !rbio->stripe_sectors ||
	    !rbio->finish_pointers || !rbio->error_bitmap) {
		free_raid_bio_pointers(rbio);
		kfree(rbio);
		return ERR_PTR(-ENOMEM);
	}

	bio_list_init(&rbio->bio_list);
	init_waitqueue_head(&rbio->io_wait);
	INIT_LIST_HEAD(&rbio->plug_list);
	spin_lock_init(&rbio->bio_list_lock);
	INIT_LIST_HEAD(&rbio->stripe_cache);
	INIT_LIST_HEAD(&rbio->hash_list);
	btrfs_get_bioc(bioc);
	rbio->bioc = bioc;
	rbio->nr_pages = num_pages;
	rbio->nr_sectors = num_sectors;
	rbio->real_stripes = real_stripes;
	rbio->stripe_npages = stripe_npages;
	rbio->stripe_nsectors = stripe_nsectors;
	refcount_set(&rbio->refs, 1);
	atomic_set(&rbio->stripes_pending, 0);

	ASSERT(btrfs_nr_parity_stripes(bioc->map_type));
	rbio->nr_data = real_stripes - btrfs_nr_parity_stripes(bioc->map_type);

	return rbio;
}

 
static int alloc_rbio_pages(struct btrfs_raid_bio *rbio)
{
	int ret;

	ret = btrfs_alloc_page_array(rbio->nr_pages, rbio->stripe_pages);
	if (ret < 0)
		return ret;
	 
	index_stripe_sectors(rbio);
	return 0;
}

 
static int alloc_rbio_parity_pages(struct btrfs_raid_bio *rbio)
{
	const int data_pages = rbio->nr_data * rbio->stripe_npages;
	int ret;

	ret = btrfs_alloc_page_array(rbio->nr_pages - data_pages,
				     rbio->stripe_pages + data_pages);
	if (ret < 0)
		return ret;

	index_stripe_sectors(rbio);
	return 0;
}

 
static int get_rbio_veritical_errors(struct btrfs_raid_bio *rbio, int sector_nr,
				     int *faila, int *failb)
{
	int stripe_nr;
	int found_errors = 0;

	if (faila || failb) {
		 
		ASSERT(faila && failb);
		*faila = -1;
		*failb = -1;
	}

	for (stripe_nr = 0; stripe_nr < rbio->real_stripes; stripe_nr++) {
		int total_sector_nr = stripe_nr * rbio->stripe_nsectors + sector_nr;

		if (test_bit(total_sector_nr, rbio->error_bitmap)) {
			found_errors++;
			if (faila) {
				 
				if (*faila < 0)
					*faila = stripe_nr;
				else if (*failb < 0)
					*failb = stripe_nr;
			}
		}
	}
	return found_errors;
}

 
static int rbio_add_io_sector(struct btrfs_raid_bio *rbio,
			      struct bio_list *bio_list,
			      struct sector_ptr *sector,
			      unsigned int stripe_nr,
			      unsigned int sector_nr,
			      enum req_op op)
{
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	struct bio *last = bio_list->tail;
	int ret;
	struct bio *bio;
	struct btrfs_io_stripe *stripe;
	u64 disk_start;

	 
	ASSERT(stripe_nr >= 0 && stripe_nr < rbio->bioc->num_stripes);
	ASSERT(sector_nr >= 0 && sector_nr < rbio->stripe_nsectors);
	ASSERT(sector->page);

	stripe = &rbio->bioc->stripes[stripe_nr];
	disk_start = stripe->physical + sector_nr * sectorsize;

	 
	if (!stripe->dev->bdev) {
		int found_errors;

		set_bit(stripe_nr * rbio->stripe_nsectors + sector_nr,
			rbio->error_bitmap);

		 
		found_errors = get_rbio_veritical_errors(rbio, sector_nr,
							 NULL, NULL);
		if (found_errors > rbio->bioc->max_errors)
			return -EIO;
		return 0;
	}

	 
	if (last) {
		u64 last_end = last->bi_iter.bi_sector << SECTOR_SHIFT;
		last_end += last->bi_iter.bi_size;

		 
		if (last_end == disk_start && !last->bi_status &&
		    last->bi_bdev == stripe->dev->bdev) {
			ret = bio_add_page(last, sector->page, sectorsize,
					   sector->pgoff);
			if (ret == sectorsize)
				return 0;
		}
	}

	 
	bio = bio_alloc(stripe->dev->bdev,
			max(BTRFS_STRIPE_LEN >> PAGE_SHIFT, 1),
			op, GFP_NOFS);
	bio->bi_iter.bi_sector = disk_start >> SECTOR_SHIFT;
	bio->bi_private = rbio;

	__bio_add_page(bio, sector->page, sectorsize, sector->pgoff);
	bio_list_add(bio_list, bio);
	return 0;
}

static void index_one_bio(struct btrfs_raid_bio *rbio, struct bio *bio)
{
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	struct bio_vec bvec;
	struct bvec_iter iter;
	u32 offset = (bio->bi_iter.bi_sector << SECTOR_SHIFT) -
		     rbio->bioc->full_stripe_logical;

	bio_for_each_segment(bvec, bio, iter) {
		u32 bvec_offset;

		for (bvec_offset = 0; bvec_offset < bvec.bv_len;
		     bvec_offset += sectorsize, offset += sectorsize) {
			int index = offset / sectorsize;
			struct sector_ptr *sector = &rbio->bio_sectors[index];

			sector->page = bvec.bv_page;
			sector->pgoff = bvec.bv_offset + bvec_offset;
			ASSERT(sector->pgoff < PAGE_SIZE);
		}
	}
}

 
static void index_rbio_pages(struct btrfs_raid_bio *rbio)
{
	struct bio *bio;

	spin_lock(&rbio->bio_list_lock);
	bio_list_for_each(bio, &rbio->bio_list)
		index_one_bio(rbio, bio);

	spin_unlock(&rbio->bio_list_lock);
}

static void bio_get_trace_info(struct btrfs_raid_bio *rbio, struct bio *bio,
			       struct raid56_bio_trace_info *trace_info)
{
	const struct btrfs_io_context *bioc = rbio->bioc;
	int i;

	ASSERT(bioc);

	 
	if (!bio->bi_bdev)
		goto not_found;

	for (i = 0; i < bioc->num_stripes; i++) {
		if (bio->bi_bdev != bioc->stripes[i].dev->bdev)
			continue;
		trace_info->stripe_nr = i;
		trace_info->devid = bioc->stripes[i].dev->devid;
		trace_info->offset = (bio->bi_iter.bi_sector << SECTOR_SHIFT) -
				     bioc->stripes[i].physical;
		return;
	}

not_found:
	trace_info->devid = -1;
	trace_info->offset = -1;
	trace_info->stripe_nr = -1;
}

static inline void bio_list_put(struct bio_list *bio_list)
{
	struct bio *bio;

	while ((bio = bio_list_pop(bio_list)))
		bio_put(bio);
}

 
static void generate_pq_vertical(struct btrfs_raid_bio *rbio, int sectornr)
{
	void **pointers = rbio->finish_pointers;
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	struct sector_ptr *sector;
	int stripe;
	const bool has_qstripe = rbio->bioc->map_type & BTRFS_BLOCK_GROUP_RAID6;

	 
	for (stripe = 0; stripe < rbio->nr_data; stripe++) {
		sector = sector_in_rbio(rbio, stripe, sectornr, 0);
		pointers[stripe] = kmap_local_page(sector->page) +
				   sector->pgoff;
	}

	 
	sector = rbio_pstripe_sector(rbio, sectornr);
	sector->uptodate = 1;
	pointers[stripe++] = kmap_local_page(sector->page) + sector->pgoff;

	if (has_qstripe) {
		 
		sector = rbio_qstripe_sector(rbio, sectornr);
		sector->uptodate = 1;
		pointers[stripe++] = kmap_local_page(sector->page) +
				     sector->pgoff;

		raid6_call.gen_syndrome(rbio->real_stripes, sectorsize,
					pointers);
	} else {
		 
		memcpy(pointers[rbio->nr_data], pointers[0], sectorsize);
		run_xor(pointers + 1, rbio->nr_data - 1, sectorsize);
	}
	for (stripe = stripe - 1; stripe >= 0; stripe--)
		kunmap_local(pointers[stripe]);
}

static int rmw_assemble_write_bios(struct btrfs_raid_bio *rbio,
				   struct bio_list *bio_list)
{
	 
	int total_sector_nr;
	int sectornr;
	int stripe;
	int ret;

	ASSERT(bio_list_size(bio_list) == 0);

	 
	ASSERT(bitmap_weight(&rbio->dbitmap, rbio->stripe_nsectors));

	 
	bitmap_clear(rbio->error_bitmap, 0, rbio->nr_sectors);

	 
	for (total_sector_nr = 0; total_sector_nr < rbio->nr_sectors;
	     total_sector_nr++) {
		struct sector_ptr *sector;

		stripe = total_sector_nr / rbio->stripe_nsectors;
		sectornr = total_sector_nr % rbio->stripe_nsectors;

		 
		if (!test_bit(sectornr, &rbio->dbitmap))
			continue;

		if (stripe < rbio->nr_data) {
			sector = sector_in_rbio(rbio, stripe, sectornr, 1);
			if (!sector)
				continue;
		} else {
			sector = rbio_stripe_sector(rbio, stripe, sectornr);
		}

		ret = rbio_add_io_sector(rbio, bio_list, sector, stripe,
					 sectornr, REQ_OP_WRITE);
		if (ret)
			goto error;
	}

	if (likely(!rbio->bioc->replace_nr_stripes))
		return 0;

	 
	ASSERT(rbio->bioc->replace_stripe_src >= 0);

	for (total_sector_nr = 0; total_sector_nr < rbio->nr_sectors;
	     total_sector_nr++) {
		struct sector_ptr *sector;

		stripe = total_sector_nr / rbio->stripe_nsectors;
		sectornr = total_sector_nr % rbio->stripe_nsectors;

		 
		if (stripe != rbio->bioc->replace_stripe_src) {
			 
			ASSERT(sectornr == 0);
			total_sector_nr += rbio->stripe_nsectors - 1;
			continue;
		}

		 
		if (!test_bit(sectornr, &rbio->dbitmap))
			continue;

		if (stripe < rbio->nr_data) {
			sector = sector_in_rbio(rbio, stripe, sectornr, 1);
			if (!sector)
				continue;
		} else {
			sector = rbio_stripe_sector(rbio, stripe, sectornr);
		}

		ret = rbio_add_io_sector(rbio, bio_list, sector,
					 rbio->real_stripes,
					 sectornr, REQ_OP_WRITE);
		if (ret)
			goto error;
	}

	return 0;
error:
	bio_list_put(bio_list);
	return -EIO;
}

static void set_rbio_range_error(struct btrfs_raid_bio *rbio, struct bio *bio)
{
	struct btrfs_fs_info *fs_info = rbio->bioc->fs_info;
	u32 offset = (bio->bi_iter.bi_sector << SECTOR_SHIFT) -
		     rbio->bioc->full_stripe_logical;
	int total_nr_sector = offset >> fs_info->sectorsize_bits;

	ASSERT(total_nr_sector < rbio->nr_data * rbio->stripe_nsectors);

	bitmap_set(rbio->error_bitmap, total_nr_sector,
		   bio->bi_iter.bi_size >> fs_info->sectorsize_bits);

	 
	if (bio->bi_iter.bi_size == 0) {
		bool found_missing = false;
		int stripe_nr;

		for (stripe_nr = 0; stripe_nr < rbio->real_stripes; stripe_nr++) {
			if (!rbio->bioc->stripes[stripe_nr].dev->bdev) {
				found_missing = true;
				bitmap_set(rbio->error_bitmap,
					   stripe_nr * rbio->stripe_nsectors,
					   rbio->stripe_nsectors);
			}
		}
		ASSERT(found_missing);
	}
}

 
static struct sector_ptr *find_stripe_sector(struct btrfs_raid_bio *rbio,
					     struct page *page,
					     unsigned int pgoff)
{
	int i;

	for (i = 0; i < rbio->nr_sectors; i++) {
		struct sector_ptr *sector = &rbio->stripe_sectors[i];

		if (sector->page == page && sector->pgoff == pgoff)
			return sector;
	}
	return NULL;
}

 
static void set_bio_pages_uptodate(struct btrfs_raid_bio *rbio, struct bio *bio)
{
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	struct bio_vec *bvec;
	struct bvec_iter_all iter_all;

	ASSERT(!bio_flagged(bio, BIO_CLONED));

	bio_for_each_segment_all(bvec, bio, iter_all) {
		struct sector_ptr *sector;
		int pgoff;

		for (pgoff = bvec->bv_offset; pgoff - bvec->bv_offset < bvec->bv_len;
		     pgoff += sectorsize) {
			sector = find_stripe_sector(rbio, bvec->bv_page, pgoff);
			ASSERT(sector);
			if (sector)
				sector->uptodate = 1;
		}
	}
}

static int get_bio_sector_nr(struct btrfs_raid_bio *rbio, struct bio *bio)
{
	struct bio_vec *bv = bio_first_bvec_all(bio);
	int i;

	for (i = 0; i < rbio->nr_sectors; i++) {
		struct sector_ptr *sector;

		sector = &rbio->stripe_sectors[i];
		if (sector->page == bv->bv_page && sector->pgoff == bv->bv_offset)
			break;
		sector = &rbio->bio_sectors[i];
		if (sector->page == bv->bv_page && sector->pgoff == bv->bv_offset)
			break;
	}
	ASSERT(i < rbio->nr_sectors);
	return i;
}

static void rbio_update_error_bitmap(struct btrfs_raid_bio *rbio, struct bio *bio)
{
	int total_sector_nr = get_bio_sector_nr(rbio, bio);
	u32 bio_size = 0;
	struct bio_vec *bvec;
	int i;

	bio_for_each_bvec_all(bvec, bio, i)
		bio_size += bvec->bv_len;

	 
	for (i = total_sector_nr; i < total_sector_nr +
	     (bio_size >> rbio->bioc->fs_info->sectorsize_bits); i++)
		set_bit(i, rbio->error_bitmap);
}

 
static void verify_bio_data_sectors(struct btrfs_raid_bio *rbio,
				    struct bio *bio)
{
	struct btrfs_fs_info *fs_info = rbio->bioc->fs_info;
	int total_sector_nr = get_bio_sector_nr(rbio, bio);
	struct bio_vec *bvec;
	struct bvec_iter_all iter_all;

	 
	if (!rbio->csum_bitmap || !rbio->csum_buf)
		return;

	 
	if (total_sector_nr >= rbio->nr_data * rbio->stripe_nsectors)
		return;

	bio_for_each_segment_all(bvec, bio, iter_all) {
		int bv_offset;

		for (bv_offset = bvec->bv_offset;
		     bv_offset < bvec->bv_offset + bvec->bv_len;
		     bv_offset += fs_info->sectorsize, total_sector_nr++) {
			u8 csum_buf[BTRFS_CSUM_SIZE];
			u8 *expected_csum = rbio->csum_buf +
					    total_sector_nr * fs_info->csum_size;
			int ret;

			 
			if (!test_bit(total_sector_nr, rbio->csum_bitmap))
				continue;

			ret = btrfs_check_sector_csum(fs_info, bvec->bv_page,
				bv_offset, csum_buf, expected_csum);
			if (ret < 0)
				set_bit(total_sector_nr, rbio->error_bitmap);
		}
	}
}

static void raid_wait_read_end_io(struct bio *bio)
{
	struct btrfs_raid_bio *rbio = bio->bi_private;

	if (bio->bi_status) {
		rbio_update_error_bitmap(rbio, bio);
	} else {
		set_bio_pages_uptodate(rbio, bio);
		verify_bio_data_sectors(rbio, bio);
	}

	bio_put(bio);
	if (atomic_dec_and_test(&rbio->stripes_pending))
		wake_up(&rbio->io_wait);
}

static void submit_read_wait_bio_list(struct btrfs_raid_bio *rbio,
			     struct bio_list *bio_list)
{
	struct bio *bio;

	atomic_set(&rbio->stripes_pending, bio_list_size(bio_list));
	while ((bio = bio_list_pop(bio_list))) {
		bio->bi_end_io = raid_wait_read_end_io;

		if (trace_raid56_read_enabled()) {
			struct raid56_bio_trace_info trace_info = { 0 };

			bio_get_trace_info(rbio, bio, &trace_info);
			trace_raid56_read(rbio, bio, &trace_info);
		}
		submit_bio(bio);
	}

	wait_event(rbio->io_wait, atomic_read(&rbio->stripes_pending) == 0);
}

static int alloc_rbio_data_pages(struct btrfs_raid_bio *rbio)
{
	const int data_pages = rbio->nr_data * rbio->stripe_npages;
	int ret;

	ret = btrfs_alloc_page_array(data_pages, rbio->stripe_pages);
	if (ret < 0)
		return ret;

	index_stripe_sectors(rbio);
	return 0;
}

 
struct btrfs_plug_cb {
	struct blk_plug_cb cb;
	struct btrfs_fs_info *info;
	struct list_head rbio_list;
	struct work_struct work;
};

 
static int plug_cmp(void *priv, const struct list_head *a,
		    const struct list_head *b)
{
	const struct btrfs_raid_bio *ra = container_of(a, struct btrfs_raid_bio,
						       plug_list);
	const struct btrfs_raid_bio *rb = container_of(b, struct btrfs_raid_bio,
						       plug_list);
	u64 a_sector = ra->bio_list.head->bi_iter.bi_sector;
	u64 b_sector = rb->bio_list.head->bi_iter.bi_sector;

	if (a_sector < b_sector)
		return -1;
	if (a_sector > b_sector)
		return 1;
	return 0;
}

static void raid_unplug(struct blk_plug_cb *cb, bool from_schedule)
{
	struct btrfs_plug_cb *plug = container_of(cb, struct btrfs_plug_cb, cb);
	struct btrfs_raid_bio *cur;
	struct btrfs_raid_bio *last = NULL;

	list_sort(NULL, &plug->rbio_list, plug_cmp);

	while (!list_empty(&plug->rbio_list)) {
		cur = list_entry(plug->rbio_list.next,
				 struct btrfs_raid_bio, plug_list);
		list_del_init(&cur->plug_list);

		if (rbio_is_full(cur)) {
			 
			start_async_work(cur, rmw_rbio_work);
			continue;
		}
		if (last) {
			if (rbio_can_merge(last, cur)) {
				merge_rbio(last, cur);
				free_raid_bio(cur);
				continue;
			}
			start_async_work(last, rmw_rbio_work);
		}
		last = cur;
	}
	if (last)
		start_async_work(last, rmw_rbio_work);
	kfree(plug);
}

 
static void rbio_add_bio(struct btrfs_raid_bio *rbio, struct bio *orig_bio)
{
	const struct btrfs_fs_info *fs_info = rbio->bioc->fs_info;
	const u64 orig_logical = orig_bio->bi_iter.bi_sector << SECTOR_SHIFT;
	const u64 full_stripe_start = rbio->bioc->full_stripe_logical;
	const u32 orig_len = orig_bio->bi_iter.bi_size;
	const u32 sectorsize = fs_info->sectorsize;
	u64 cur_logical;

	ASSERT(orig_logical >= full_stripe_start &&
	       orig_logical + orig_len <= full_stripe_start +
	       rbio->nr_data * BTRFS_STRIPE_LEN);

	bio_list_add(&rbio->bio_list, orig_bio);
	rbio->bio_list_bytes += orig_bio->bi_iter.bi_size;

	 
	for (cur_logical = orig_logical; cur_logical < orig_logical + orig_len;
	     cur_logical += sectorsize) {
		int bit = ((u32)(cur_logical - full_stripe_start) >>
			   fs_info->sectorsize_bits) % rbio->stripe_nsectors;

		set_bit(bit, &rbio->dbitmap);
	}
}

 
void raid56_parity_write(struct bio *bio, struct btrfs_io_context *bioc)
{
	struct btrfs_fs_info *fs_info = bioc->fs_info;
	struct btrfs_raid_bio *rbio;
	struct btrfs_plug_cb *plug = NULL;
	struct blk_plug_cb *cb;

	rbio = alloc_rbio(fs_info, bioc);
	if (IS_ERR(rbio)) {
		bio->bi_status = errno_to_blk_status(PTR_ERR(rbio));
		bio_endio(bio);
		return;
	}
	rbio->operation = BTRFS_RBIO_WRITE;
	rbio_add_bio(rbio, bio);

	 
	if (!rbio_is_full(rbio)) {
		cb = blk_check_plugged(raid_unplug, fs_info, sizeof(*plug));
		if (cb) {
			plug = container_of(cb, struct btrfs_plug_cb, cb);
			if (!plug->info) {
				plug->info = fs_info;
				INIT_LIST_HEAD(&plug->rbio_list);
			}
			list_add_tail(&rbio->plug_list, &plug->rbio_list);
			return;
		}
	}

	 
	start_async_work(rbio, rmw_rbio_work);
}

static int verify_one_sector(struct btrfs_raid_bio *rbio,
			     int stripe_nr, int sector_nr)
{
	struct btrfs_fs_info *fs_info = rbio->bioc->fs_info;
	struct sector_ptr *sector;
	u8 csum_buf[BTRFS_CSUM_SIZE];
	u8 *csum_expected;
	int ret;

	if (!rbio->csum_bitmap || !rbio->csum_buf)
		return 0;

	 
	if (stripe_nr >= rbio->nr_data)
		return 0;
	 
	if (rbio->operation == BTRFS_RBIO_READ_REBUILD) {
		sector = sector_in_rbio(rbio, stripe_nr, sector_nr, 0);
	} else {
		sector = rbio_stripe_sector(rbio, stripe_nr, sector_nr);
	}

	ASSERT(sector->page);

	csum_expected = rbio->csum_buf +
			(stripe_nr * rbio->stripe_nsectors + sector_nr) *
			fs_info->csum_size;
	ret = btrfs_check_sector_csum(fs_info, sector->page, sector->pgoff,
				      csum_buf, csum_expected);
	return ret;
}

 
static int recover_vertical(struct btrfs_raid_bio *rbio, int sector_nr,
			    void **pointers, void **unmap_array)
{
	struct btrfs_fs_info *fs_info = rbio->bioc->fs_info;
	struct sector_ptr *sector;
	const u32 sectorsize = fs_info->sectorsize;
	int found_errors;
	int faila;
	int failb;
	int stripe_nr;
	int ret = 0;

	 
	if (rbio->operation == BTRFS_RBIO_PARITY_SCRUB &&
	    !test_bit(sector_nr, &rbio->dbitmap))
		return 0;

	found_errors = get_rbio_veritical_errors(rbio, sector_nr, &faila,
						 &failb);
	 
	if (!found_errors)
		return 0;

	if (found_errors > rbio->bioc->max_errors)
		return -EIO;

	 
	for (stripe_nr = 0; stripe_nr < rbio->real_stripes; stripe_nr++) {
		 
		if (rbio->operation == BTRFS_RBIO_READ_REBUILD) {
			sector = sector_in_rbio(rbio, stripe_nr, sector_nr, 0);
		} else {
			sector = rbio_stripe_sector(rbio, stripe_nr, sector_nr);
		}
		ASSERT(sector->page);
		pointers[stripe_nr] = kmap_local_page(sector->page) +
				   sector->pgoff;
		unmap_array[stripe_nr] = pointers[stripe_nr];
	}

	 
	if (rbio->bioc->map_type & BTRFS_BLOCK_GROUP_RAID6) {
		 
		if (failb < 0) {
			if (faila == rbio->nr_data)
				 
				goto cleanup;
			 
			goto pstripe;
		}

		 
		if (failb == rbio->real_stripes - 1) {
			if (faila == rbio->real_stripes - 2)
				 
				goto cleanup;
			 
			goto pstripe;
		}

		if (failb == rbio->real_stripes - 2) {
			raid6_datap_recov(rbio->real_stripes, sectorsize,
					  faila, pointers);
		} else {
			raid6_2data_recov(rbio->real_stripes, sectorsize,
					  faila, failb, pointers);
		}
	} else {
		void *p;

		 
		ASSERT(failb == -1);
pstripe:
		 
		memcpy(pointers[faila], pointers[rbio->nr_data], sectorsize);

		 
		p = pointers[faila];
		for (stripe_nr = faila; stripe_nr < rbio->nr_data - 1;
		     stripe_nr++)
			pointers[stripe_nr] = pointers[stripe_nr + 1];
		pointers[rbio->nr_data - 1] = p;

		 
		run_xor(pointers, rbio->nr_data - 1, sectorsize);

	}

	 
	if (faila >= 0) {
		ret = verify_one_sector(rbio, faila, sector_nr);
		if (ret < 0)
			goto cleanup;

		sector = rbio_stripe_sector(rbio, faila, sector_nr);
		sector->uptodate = 1;
	}
	if (failb >= 0) {
		ret = verify_one_sector(rbio, failb, sector_nr);
		if (ret < 0)
			goto cleanup;

		sector = rbio_stripe_sector(rbio, failb, sector_nr);
		sector->uptodate = 1;
	}

cleanup:
	for (stripe_nr = rbio->real_stripes - 1; stripe_nr >= 0; stripe_nr--)
		kunmap_local(unmap_array[stripe_nr]);
	return ret;
}

static int recover_sectors(struct btrfs_raid_bio *rbio)
{
	void **pointers = NULL;
	void **unmap_array = NULL;
	int sectornr;
	int ret = 0;

	 
	pointers = kcalloc(rbio->real_stripes, sizeof(void *), GFP_NOFS);
	unmap_array = kcalloc(rbio->real_stripes, sizeof(void *), GFP_NOFS);
	if (!pointers || !unmap_array) {
		ret = -ENOMEM;
		goto out;
	}

	if (rbio->operation == BTRFS_RBIO_READ_REBUILD) {
		spin_lock(&rbio->bio_list_lock);
		set_bit(RBIO_RMW_LOCKED_BIT, &rbio->flags);
		spin_unlock(&rbio->bio_list_lock);
	}

	index_rbio_pages(rbio);

	for (sectornr = 0; sectornr < rbio->stripe_nsectors; sectornr++) {
		ret = recover_vertical(rbio, sectornr, pointers, unmap_array);
		if (ret < 0)
			break;
	}

out:
	kfree(pointers);
	kfree(unmap_array);
	return ret;
}

static void recover_rbio(struct btrfs_raid_bio *rbio)
{
	struct bio_list bio_list = BIO_EMPTY_LIST;
	int total_sector_nr;
	int ret = 0;

	 
	ASSERT(bitmap_weight(rbio->error_bitmap, rbio->nr_sectors));

	 
	ret = alloc_rbio_pages(rbio);
	if (ret < 0)
		goto out;

	index_rbio_pages(rbio);

	 
	for (total_sector_nr = 0; total_sector_nr < rbio->nr_sectors;
	     total_sector_nr++) {
		int stripe = total_sector_nr / rbio->stripe_nsectors;
		int sectornr = total_sector_nr % rbio->stripe_nsectors;
		struct sector_ptr *sector;

		 
		if (!rbio->bioc->stripes[stripe].dev->bdev ||
		    test_bit(total_sector_nr, rbio->error_bitmap)) {
			 
			set_bit(total_sector_nr, rbio->error_bitmap);
			continue;
		}

		sector = rbio_stripe_sector(rbio, stripe, sectornr);
		ret = rbio_add_io_sector(rbio, &bio_list, sector, stripe,
					 sectornr, REQ_OP_READ);
		if (ret < 0) {
			bio_list_put(&bio_list);
			goto out;
		}
	}

	submit_read_wait_bio_list(rbio, &bio_list);
	ret = recover_sectors(rbio);
out:
	rbio_orig_end_io(rbio, errno_to_blk_status(ret));
}

static void recover_rbio_work(struct work_struct *work)
{
	struct btrfs_raid_bio *rbio;

	rbio = container_of(work, struct btrfs_raid_bio, work);
	if (!lock_stripe_add(rbio))
		recover_rbio(rbio);
}

static void recover_rbio_work_locked(struct work_struct *work)
{
	recover_rbio(container_of(work, struct btrfs_raid_bio, work));
}

static void set_rbio_raid6_extra_error(struct btrfs_raid_bio *rbio, int mirror_num)
{
	bool found = false;
	int sector_nr;

	 
	ASSERT(mirror_num > 2);
	for (sector_nr = 0; sector_nr < rbio->stripe_nsectors; sector_nr++) {
		int found_errors;
		int faila;
		int failb;

		found_errors = get_rbio_veritical_errors(rbio, sector_nr,
							 &faila, &failb);
		 
		if (!found_errors)
			continue;

		 
		ASSERT(found_errors == 1);
		found = true;

		 
		failb = rbio->real_stripes - (mirror_num - 1);
		if (failb <= faila)
			failb--;

		 
		if (failb >= 0)
			set_bit(failb * rbio->stripe_nsectors + sector_nr,
				rbio->error_bitmap);
	}

	 
	ASSERT(found);
}

 
void raid56_parity_recover(struct bio *bio, struct btrfs_io_context *bioc,
			   int mirror_num)
{
	struct btrfs_fs_info *fs_info = bioc->fs_info;
	struct btrfs_raid_bio *rbio;

	rbio = alloc_rbio(fs_info, bioc);
	if (IS_ERR(rbio)) {
		bio->bi_status = errno_to_blk_status(PTR_ERR(rbio));
		bio_endio(bio);
		return;
	}

	rbio->operation = BTRFS_RBIO_READ_REBUILD;
	rbio_add_bio(rbio, bio);

	set_rbio_range_error(rbio, bio);

	 
	if (mirror_num > 2)
		set_rbio_raid6_extra_error(rbio, mirror_num);

	start_async_work(rbio, recover_rbio_work);
}

static void fill_data_csums(struct btrfs_raid_bio *rbio)
{
	struct btrfs_fs_info *fs_info = rbio->bioc->fs_info;
	struct btrfs_root *csum_root = btrfs_csum_root(fs_info,
						       rbio->bioc->full_stripe_logical);
	const u64 start = rbio->bioc->full_stripe_logical;
	const u32 len = (rbio->nr_data * rbio->stripe_nsectors) <<
			fs_info->sectorsize_bits;
	int ret;

	 
	ASSERT(!rbio->csum_buf && !rbio->csum_bitmap);

	 
	if (!(rbio->bioc->map_type & BTRFS_BLOCK_GROUP_DATA) ||
	    rbio->bioc->map_type & BTRFS_BLOCK_GROUP_METADATA)
		return;

	rbio->csum_buf = kzalloc(rbio->nr_data * rbio->stripe_nsectors *
				 fs_info->csum_size, GFP_NOFS);
	rbio->csum_bitmap = bitmap_zalloc(rbio->nr_data * rbio->stripe_nsectors,
					  GFP_NOFS);
	if (!rbio->csum_buf || !rbio->csum_bitmap) {
		ret = -ENOMEM;
		goto error;
	}

	ret = btrfs_lookup_csums_bitmap(csum_root, NULL, start, start + len - 1,
					rbio->csum_buf, rbio->csum_bitmap);
	if (ret < 0)
		goto error;
	if (bitmap_empty(rbio->csum_bitmap, len >> fs_info->sectorsize_bits))
		goto no_csum;
	return;

error:
	 
	btrfs_warn_rl(fs_info,
"sub-stripe write for full stripe %llu is not safe, failed to get csum: %d",
			rbio->bioc->full_stripe_logical, ret);
no_csum:
	kfree(rbio->csum_buf);
	bitmap_free(rbio->csum_bitmap);
	rbio->csum_buf = NULL;
	rbio->csum_bitmap = NULL;
}

static int rmw_read_wait_recover(struct btrfs_raid_bio *rbio)
{
	struct bio_list bio_list = BIO_EMPTY_LIST;
	int total_sector_nr;
	int ret = 0;

	 
	fill_data_csums(rbio);

	 
	for (total_sector_nr = 0; total_sector_nr < rbio->nr_sectors;
	     total_sector_nr++) {
		struct sector_ptr *sector;
		int stripe = total_sector_nr / rbio->stripe_nsectors;
		int sectornr = total_sector_nr % rbio->stripe_nsectors;

		sector = rbio_stripe_sector(rbio, stripe, sectornr);
		ret = rbio_add_io_sector(rbio, &bio_list, sector,
			       stripe, sectornr, REQ_OP_READ);
		if (ret) {
			bio_list_put(&bio_list);
			return ret;
		}
	}

	 
	submit_read_wait_bio_list(rbio, &bio_list);
	return recover_sectors(rbio);
}

static void raid_wait_write_end_io(struct bio *bio)
{
	struct btrfs_raid_bio *rbio = bio->bi_private;
	blk_status_t err = bio->bi_status;

	if (err)
		rbio_update_error_bitmap(rbio, bio);
	bio_put(bio);
	if (atomic_dec_and_test(&rbio->stripes_pending))
		wake_up(&rbio->io_wait);
}

static void submit_write_bios(struct btrfs_raid_bio *rbio,
			      struct bio_list *bio_list)
{
	struct bio *bio;

	atomic_set(&rbio->stripes_pending, bio_list_size(bio_list));
	while ((bio = bio_list_pop(bio_list))) {
		bio->bi_end_io = raid_wait_write_end_io;

		if (trace_raid56_write_enabled()) {
			struct raid56_bio_trace_info trace_info = { 0 };

			bio_get_trace_info(rbio, bio, &trace_info);
			trace_raid56_write(rbio, bio, &trace_info);
		}
		submit_bio(bio);
	}
}

 
static bool need_read_stripe_sectors(struct btrfs_raid_bio *rbio)
{
	int i;

	for (i = 0; i < rbio->nr_data * rbio->stripe_nsectors; i++) {
		struct sector_ptr *sector = &rbio->stripe_sectors[i];

		 
		if (!sector->page || !sector->uptodate)
			return true;
	}
	return false;
}

static void rmw_rbio(struct btrfs_raid_bio *rbio)
{
	struct bio_list bio_list;
	int sectornr;
	int ret = 0;

	 
	ret = alloc_rbio_parity_pages(rbio);
	if (ret < 0)
		goto out;

	 
	if (!rbio_is_full(rbio) && need_read_stripe_sectors(rbio)) {
		 
		ret = alloc_rbio_data_pages(rbio);
		if (ret < 0)
			goto out;

		index_rbio_pages(rbio);

		ret = rmw_read_wait_recover(rbio);
		if (ret < 0)
			goto out;
	}

	 
	spin_lock(&rbio->bio_list_lock);
	set_bit(RBIO_RMW_LOCKED_BIT, &rbio->flags);
	spin_unlock(&rbio->bio_list_lock);

	bitmap_clear(rbio->error_bitmap, 0, rbio->nr_sectors);

	index_rbio_pages(rbio);

	 
	if (!rbio_is_full(rbio))
		cache_rbio_pages(rbio);
	else
		clear_bit(RBIO_CACHE_READY_BIT, &rbio->flags);

	for (sectornr = 0; sectornr < rbio->stripe_nsectors; sectornr++)
		generate_pq_vertical(rbio, sectornr);

	bio_list_init(&bio_list);
	ret = rmw_assemble_write_bios(rbio, &bio_list);
	if (ret < 0)
		goto out;

	 
	ASSERT(bio_list_size(&bio_list));
	submit_write_bios(rbio, &bio_list);
	wait_event(rbio->io_wait, atomic_read(&rbio->stripes_pending) == 0);

	 
	for (sectornr = 0; sectornr < rbio->stripe_nsectors; sectornr++) {
		int found_errors;

		found_errors = get_rbio_veritical_errors(rbio, sectornr, NULL, NULL);
		if (found_errors > rbio->bioc->max_errors) {
			ret = -EIO;
			break;
		}
	}
out:
	rbio_orig_end_io(rbio, errno_to_blk_status(ret));
}

static void rmw_rbio_work(struct work_struct *work)
{
	struct btrfs_raid_bio *rbio;

	rbio = container_of(work, struct btrfs_raid_bio, work);
	if (lock_stripe_add(rbio) == 0)
		rmw_rbio(rbio);
}

static void rmw_rbio_work_locked(struct work_struct *work)
{
	rmw_rbio(container_of(work, struct btrfs_raid_bio, work));
}

 

struct btrfs_raid_bio *raid56_parity_alloc_scrub_rbio(struct bio *bio,
				struct btrfs_io_context *bioc,
				struct btrfs_device *scrub_dev,
				unsigned long *dbitmap, int stripe_nsectors)
{
	struct btrfs_fs_info *fs_info = bioc->fs_info;
	struct btrfs_raid_bio *rbio;
	int i;

	rbio = alloc_rbio(fs_info, bioc);
	if (IS_ERR(rbio))
		return NULL;
	bio_list_add(&rbio->bio_list, bio);
	 
	ASSERT(!bio->bi_iter.bi_size);
	rbio->operation = BTRFS_RBIO_PARITY_SCRUB;

	 
	for (i = rbio->nr_data; i < rbio->real_stripes; i++) {
		if (bioc->stripes[i].dev == scrub_dev) {
			rbio->scrubp = i;
			break;
		}
	}
	ASSERT(i < rbio->real_stripes);

	bitmap_copy(&rbio->dbitmap, dbitmap, stripe_nsectors);
	return rbio;
}

 
static int alloc_rbio_essential_pages(struct btrfs_raid_bio *rbio)
{
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	int total_sector_nr;

	for (total_sector_nr = 0; total_sector_nr < rbio->nr_sectors;
	     total_sector_nr++) {
		struct page *page;
		int sectornr = total_sector_nr % rbio->stripe_nsectors;
		int index = (total_sector_nr * sectorsize) >> PAGE_SHIFT;

		if (!test_bit(sectornr, &rbio->dbitmap))
			continue;
		if (rbio->stripe_pages[index])
			continue;
		page = alloc_page(GFP_NOFS);
		if (!page)
			return -ENOMEM;
		rbio->stripe_pages[index] = page;
	}
	index_stripe_sectors(rbio);
	return 0;
}

static int finish_parity_scrub(struct btrfs_raid_bio *rbio)
{
	struct btrfs_io_context *bioc = rbio->bioc;
	const u32 sectorsize = bioc->fs_info->sectorsize;
	void **pointers = rbio->finish_pointers;
	unsigned long *pbitmap = &rbio->finish_pbitmap;
	int nr_data = rbio->nr_data;
	int stripe;
	int sectornr;
	bool has_qstripe;
	struct sector_ptr p_sector = { 0 };
	struct sector_ptr q_sector = { 0 };
	struct bio_list bio_list;
	int is_replace = 0;
	int ret;

	bio_list_init(&bio_list);

	if (rbio->real_stripes - rbio->nr_data == 1)
		has_qstripe = false;
	else if (rbio->real_stripes - rbio->nr_data == 2)
		has_qstripe = true;
	else
		BUG();

	 
	if (bioc->replace_nr_stripes && bioc->replace_stripe_src == rbio->scrubp) {
		is_replace = 1;
		bitmap_copy(pbitmap, &rbio->dbitmap, rbio->stripe_nsectors);
	}

	 
	clear_bit(RBIO_CACHE_READY_BIT, &rbio->flags);

	p_sector.page = alloc_page(GFP_NOFS);
	if (!p_sector.page)
		return -ENOMEM;
	p_sector.pgoff = 0;
	p_sector.uptodate = 1;

	if (has_qstripe) {
		 
		q_sector.page = alloc_page(GFP_NOFS);
		if (!q_sector.page) {
			__free_page(p_sector.page);
			p_sector.page = NULL;
			return -ENOMEM;
		}
		q_sector.pgoff = 0;
		q_sector.uptodate = 1;
		pointers[rbio->real_stripes - 1] = kmap_local_page(q_sector.page);
	}

	bitmap_clear(rbio->error_bitmap, 0, rbio->nr_sectors);

	 
	pointers[nr_data] = kmap_local_page(p_sector.page);

	for_each_set_bit(sectornr, &rbio->dbitmap, rbio->stripe_nsectors) {
		struct sector_ptr *sector;
		void *parity;

		 
		for (stripe = 0; stripe < nr_data; stripe++) {
			sector = sector_in_rbio(rbio, stripe, sectornr, 0);
			pointers[stripe] = kmap_local_page(sector->page) +
					   sector->pgoff;
		}

		if (has_qstripe) {
			 
			raid6_call.gen_syndrome(rbio->real_stripes, sectorsize,
						pointers);
		} else {
			 
			memcpy(pointers[nr_data], pointers[0], sectorsize);
			run_xor(pointers + 1, nr_data - 1, sectorsize);
		}

		 
		sector = rbio_stripe_sector(rbio, rbio->scrubp, sectornr);
		parity = kmap_local_page(sector->page) + sector->pgoff;
		if (memcmp(parity, pointers[rbio->scrubp], sectorsize) != 0)
			memcpy(parity, pointers[rbio->scrubp], sectorsize);
		else
			 
			bitmap_clear(&rbio->dbitmap, sectornr, 1);
		kunmap_local(parity);

		for (stripe = nr_data - 1; stripe >= 0; stripe--)
			kunmap_local(pointers[stripe]);
	}

	kunmap_local(pointers[nr_data]);
	__free_page(p_sector.page);
	p_sector.page = NULL;
	if (q_sector.page) {
		kunmap_local(pointers[rbio->real_stripes - 1]);
		__free_page(q_sector.page);
		q_sector.page = NULL;
	}

	 
	for_each_set_bit(sectornr, &rbio->dbitmap, rbio->stripe_nsectors) {
		struct sector_ptr *sector;

		sector = rbio_stripe_sector(rbio, rbio->scrubp, sectornr);
		ret = rbio_add_io_sector(rbio, &bio_list, sector, rbio->scrubp,
					 sectornr, REQ_OP_WRITE);
		if (ret)
			goto cleanup;
	}

	if (!is_replace)
		goto submit_write;

	 
	ASSERT(rbio->bioc->replace_stripe_src >= 0);
	for_each_set_bit(sectornr, pbitmap, rbio->stripe_nsectors) {
		struct sector_ptr *sector;

		sector = rbio_stripe_sector(rbio, rbio->scrubp, sectornr);
		ret = rbio_add_io_sector(rbio, &bio_list, sector,
					 rbio->real_stripes,
					 sectornr, REQ_OP_WRITE);
		if (ret)
			goto cleanup;
	}

submit_write:
	submit_write_bios(rbio, &bio_list);
	return 0;

cleanup:
	bio_list_put(&bio_list);
	return ret;
}

static inline int is_data_stripe(struct btrfs_raid_bio *rbio, int stripe)
{
	if (stripe >= 0 && stripe < rbio->nr_data)
		return 1;
	return 0;
}

static int recover_scrub_rbio(struct btrfs_raid_bio *rbio)
{
	void **pointers = NULL;
	void **unmap_array = NULL;
	int sector_nr;
	int ret = 0;

	 
	pointers = kcalloc(rbio->real_stripes, sizeof(void *), GFP_NOFS);
	unmap_array = kcalloc(rbio->real_stripes, sizeof(void *), GFP_NOFS);
	if (!pointers || !unmap_array) {
		ret = -ENOMEM;
		goto out;
	}

	for (sector_nr = 0; sector_nr < rbio->stripe_nsectors; sector_nr++) {
		int dfail = 0, failp = -1;
		int faila;
		int failb;
		int found_errors;

		found_errors = get_rbio_veritical_errors(rbio, sector_nr,
							 &faila, &failb);
		if (found_errors > rbio->bioc->max_errors) {
			ret = -EIO;
			goto out;
		}
		if (found_errors == 0)
			continue;

		 
		ASSERT(faila >= 0 || failb >= 0);

		if (is_data_stripe(rbio, faila))
			dfail++;
		else if (is_parity_stripe(faila))
			failp = faila;

		if (is_data_stripe(rbio, failb))
			dfail++;
		else if (is_parity_stripe(failb))
			failp = failb;
		 
		if (dfail > rbio->bioc->max_errors - 1) {
			ret = -EIO;
			goto out;
		}
		 
		if (dfail == 0)
			continue;

		 
		if (failp != rbio->scrubp) {
			ret = -EIO;
			goto out;
		}

		ret = recover_vertical(rbio, sector_nr, pointers, unmap_array);
		if (ret < 0)
			goto out;
	}
out:
	kfree(pointers);
	kfree(unmap_array);
	return ret;
}

static int scrub_assemble_read_bios(struct btrfs_raid_bio *rbio)
{
	struct bio_list bio_list = BIO_EMPTY_LIST;
	int total_sector_nr;
	int ret = 0;

	 
	for (total_sector_nr = 0; total_sector_nr < rbio->nr_sectors;
	     total_sector_nr++) {
		int sectornr = total_sector_nr % rbio->stripe_nsectors;
		int stripe = total_sector_nr / rbio->stripe_nsectors;
		struct sector_ptr *sector;

		 
		if (!test_bit(sectornr, &rbio->dbitmap))
			continue;

		 
		sector = sector_in_rbio(rbio, stripe, sectornr, 1);
		if (sector)
			continue;

		sector = rbio_stripe_sector(rbio, stripe, sectornr);
		 
		if (sector->uptodate)
			continue;

		ret = rbio_add_io_sector(rbio, &bio_list, sector, stripe,
					 sectornr, REQ_OP_READ);
		if (ret) {
			bio_list_put(&bio_list);
			return ret;
		}
	}

	submit_read_wait_bio_list(rbio, &bio_list);
	return 0;
}

static void scrub_rbio(struct btrfs_raid_bio *rbio)
{
	int sector_nr;
	int ret;

	ret = alloc_rbio_essential_pages(rbio);
	if (ret)
		goto out;

	bitmap_clear(rbio->error_bitmap, 0, rbio->nr_sectors);

	ret = scrub_assemble_read_bios(rbio);
	if (ret < 0)
		goto out;

	 
	ret = recover_scrub_rbio(rbio);
	if (ret < 0)
		goto out;

	 
	ret = finish_parity_scrub(rbio);
	wait_event(rbio->io_wait, atomic_read(&rbio->stripes_pending) == 0);
	for (sector_nr = 0; sector_nr < rbio->stripe_nsectors; sector_nr++) {
		int found_errors;

		found_errors = get_rbio_veritical_errors(rbio, sector_nr, NULL, NULL);
		if (found_errors > rbio->bioc->max_errors) {
			ret = -EIO;
			break;
		}
	}
out:
	rbio_orig_end_io(rbio, errno_to_blk_status(ret));
}

static void scrub_rbio_work_locked(struct work_struct *work)
{
	scrub_rbio(container_of(work, struct btrfs_raid_bio, work));
}

void raid56_parity_submit_scrub_rbio(struct btrfs_raid_bio *rbio)
{
	if (!lock_stripe_add(rbio))
		start_async_work(rbio, scrub_rbio_work_locked);
}

 
void raid56_parity_cache_data_pages(struct btrfs_raid_bio *rbio,
				    struct page **data_pages, u64 data_logical)
{
	const u64 offset_in_full_stripe = data_logical -
					  rbio->bioc->full_stripe_logical;
	const int page_index = offset_in_full_stripe >> PAGE_SHIFT;
	const u32 sectorsize = rbio->bioc->fs_info->sectorsize;
	const u32 sectors_per_page = PAGE_SIZE / sectorsize;
	int ret;

	 
	ret = alloc_rbio_data_pages(rbio);
	if (ret < 0)
		return;

	 
	ASSERT(IS_ALIGNED(offset_in_full_stripe, BTRFS_STRIPE_LEN));
	ASSERT(offset_in_full_stripe < (rbio->nr_data << BTRFS_STRIPE_LEN_SHIFT));

	for (int page_nr = 0; page_nr < (BTRFS_STRIPE_LEN >> PAGE_SHIFT); page_nr++) {
		struct page *dst = rbio->stripe_pages[page_nr + page_index];
		struct page *src = data_pages[page_nr];

		memcpy_page(dst, 0, src, 0, PAGE_SIZE);
		for (int sector_nr = sectors_per_page * page_index;
		     sector_nr < sectors_per_page * (page_index + 1);
		     sector_nr++)
			rbio->stripe_sectors[sector_nr].uptodate = true;
	}
}


#ifndef _BCACHE_H
#define _BCACHE_H



#define pr_fmt(fmt) "bcache: %s() " fmt, __func__

#include <linux/bio.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/rwsem.h>
#include <linux/refcount.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>

#include "bcache_ondisk.h"
#include "bset.h"
#include "util.h"
#include "closure.h"

struct bucket {
	atomic_t	pin;
	uint16_t	prio;
	uint8_t		gen;
	uint8_t		last_gc; 
	uint16_t	gc_mark; 
};



BITMASK(GC_MARK,	 struct bucket, gc_mark, 0, 2);
#define GC_MARK_RECLAIMABLE	1
#define GC_MARK_DIRTY		2
#define GC_MARK_METADATA	3
#define GC_SECTORS_USED_SIZE	13
#define MAX_GC_SECTORS_USED	(~(~0ULL << GC_SECTORS_USED_SIZE))
BITMASK(GC_SECTORS_USED, struct bucket, gc_mark, 2, GC_SECTORS_USED_SIZE);
BITMASK(GC_MOVE, struct bucket, gc_mark, 15, 1);

#include "journal.h"
#include "stats.h"
struct search;
struct btree;
struct keybuf;

struct keybuf_key {
	struct rb_node		node;
	BKEY_PADDED(key);
	void			*private;
};

struct keybuf {
	struct bkey		last_scanned;
	spinlock_t		lock;

	
	struct bkey		start;
	struct bkey		end;

	struct rb_root		keys;

#define KEYBUF_NR		500
	DECLARE_ARRAY_ALLOCATOR(struct keybuf_key, freelist, KEYBUF_NR);
};

struct bcache_device {
	struct closure		cl;

	struct kobject		kobj;

	struct cache_set	*c;
	unsigned int		id;
#define BCACHEDEVNAME_SIZE	12
	char			name[BCACHEDEVNAME_SIZE];

	struct gendisk		*disk;

	unsigned long		flags;
#define BCACHE_DEV_CLOSING		0
#define BCACHE_DEV_DETACHING		1
#define BCACHE_DEV_UNLINK_DONE		2
#define BCACHE_DEV_WB_RUNNING		3
#define BCACHE_DEV_RATE_DW_RUNNING	4
	int			nr_stripes;
#define BCH_MIN_STRIPE_SZ		((4 << 20) >> SECTOR_SHIFT)
	unsigned int		stripe_size;
	atomic_t		*stripe_sectors_dirty;
	unsigned long		*full_dirty_stripes;

	struct bio_set		bio_split;

	unsigned int		data_csum:1;

	int (*cache_miss)(struct btree *b, struct search *s,
			  struct bio *bio, unsigned int sectors);
	int (*ioctl)(struct bcache_device *d, blk_mode_t mode,
		     unsigned int cmd, unsigned long arg);
};

struct io {
	
	struct hlist_node	hash;
	struct list_head	lru;

	unsigned long		jiffies;
	unsigned int		sequential;
	sector_t		last;
};

enum stop_on_failure {
	BCH_CACHED_DEV_STOP_AUTO = 0,
	BCH_CACHED_DEV_STOP_ALWAYS,
	BCH_CACHED_DEV_STOP_MODE_MAX,
};

struct cached_dev {
	struct list_head	list;
	struct bcache_device	disk;
	struct block_device	*bdev;

	struct cache_sb		sb;
	struct cache_sb_disk	*sb_disk;
	struct bio		sb_bio;
	struct bio_vec		sb_bv[1];
	struct closure		sb_write;
	struct semaphore	sb_write_mutex;

	
	refcount_t		count;
	struct work_struct	detach;

	
	atomic_t		running;

	
	struct rw_semaphore	writeback_lock;

	
	atomic_t		has_dirty;

#define BCH_CACHE_READA_ALL		0
#define BCH_CACHE_READA_META_ONLY	1
	unsigned int		cache_readahead_policy;
	struct bch_ratelimit	writeback_rate;
	struct delayed_work	writeback_rate_update;

	
	struct semaphore	in_flight;
	struct task_struct	*writeback_thread;
	struct workqueue_struct	*writeback_write_wq;

	struct keybuf		writeback_keys;

	struct task_struct	*status_update_thread;
	
	struct closure_waitlist writeback_ordering_wait;
	atomic_t		writeback_sequence_next;

	
#define RECENT_IO_BITS	7
#define RECENT_IO	(1 << RECENT_IO_BITS)
	struct io		io[RECENT_IO];
	struct hlist_head	io_hash[RECENT_IO + 1];
	struct list_head	io_lru;
	spinlock_t		io_lock;

	struct cache_accounting	accounting;

	
	unsigned int		sequential_cutoff;

	unsigned int		io_disable:1;
	unsigned int		verify:1;
	unsigned int		bypass_torture_test:1;

	unsigned int		partial_stripes_expensive:1;
	unsigned int		writeback_metadata:1;
	unsigned int		writeback_running:1;
	unsigned int		writeback_consider_fragment:1;
	unsigned char		writeback_percent;
	unsigned int		writeback_delay;

	uint64_t		writeback_rate_target;
	int64_t			writeback_rate_proportional;
	int64_t			writeback_rate_integral;
	int64_t			writeback_rate_integral_scaled;
	int32_t			writeback_rate_change;

	unsigned int		writeback_rate_update_seconds;
	unsigned int		writeback_rate_i_term_inverse;
	unsigned int		writeback_rate_p_term_inverse;
	unsigned int		writeback_rate_fp_term_low;
	unsigned int		writeback_rate_fp_term_mid;
	unsigned int		writeback_rate_fp_term_high;
	unsigned int		writeback_rate_minimum;

	enum stop_on_failure	stop_when_cache_set_failed;
#define DEFAULT_CACHED_DEV_ERROR_LIMIT	64
	atomic_t		io_errors;
	unsigned int		error_limit;
	unsigned int		offline_seconds;

	
#define BCH_WBRATE_UPDATE_MAX_SKIPS	15
	unsigned int		rate_update_retry;
};

enum alloc_reserve {
	RESERVE_BTREE,
	RESERVE_PRIO,
	RESERVE_MOVINGGC,
	RESERVE_NONE,
	RESERVE_NR,
};

struct cache {
	struct cache_set	*set;
	struct cache_sb		sb;
	struct cache_sb_disk	*sb_disk;
	struct bio		sb_bio;
	struct bio_vec		sb_bv[1];

	struct kobject		kobj;
	struct block_device	*bdev;

	struct task_struct	*alloc_thread;

	struct closure		prio;
	struct prio_set		*disk_buckets;

	
	uint64_t		*prio_buckets;
	uint64_t		*prio_last_buckets;

	
	DECLARE_FIFO(long, free)[RESERVE_NR];
	DECLARE_FIFO(long, free_inc);

	size_t			fifo_last_bucket;

	
	struct bucket		*buckets;

	DECLARE_HEAP(struct bucket *, heap);

	
	unsigned int		invalidate_needs_gc;

	bool			discard; 

	struct journal_device	journal;

	
#define IO_ERROR_SHIFT		20
	atomic_t		io_errors;
	atomic_t		io_count;

	atomic_long_t		meta_sectors_written;
	atomic_long_t		btree_sectors_written;
	atomic_long_t		sectors_written;
};

struct gc_stat {
	size_t			nodes;
	size_t			nodes_pre;
	size_t			key_bytes;

	size_t			nkeys;
	uint64_t		data;	
	unsigned int		in_use; 
};


#define CACHE_SET_UNREGISTERING		0
#define	CACHE_SET_STOPPING		1
#define	CACHE_SET_RUNNING		2
#define CACHE_SET_IO_DISABLE		3

struct cache_set {
	struct closure		cl;

	struct list_head	list;
	struct kobject		kobj;
	struct kobject		internal;
	struct dentry		*debug;
	struct cache_accounting accounting;

	unsigned long		flags;
	atomic_t		idle_counter;
	atomic_t		at_max_writeback_rate;

	struct cache		*cache;

	struct bcache_device	**devices;
	unsigned int		devices_max_used;
	atomic_t		attached_dev_nr;
	struct list_head	cached_devs;
	uint64_t		cached_dev_sectors;
	atomic_long_t		flash_dev_dirty_sectors;
	struct closure		caching;

	struct closure		sb_write;
	struct semaphore	sb_write_mutex;

	mempool_t		search;
	mempool_t		bio_meta;
	struct bio_set		bio_split;

	
	struct shrinker		shrink;

	
	struct mutex		bucket_lock;

	
	unsigned short		bucket_bits;

	
	unsigned short		block_bits;

	
	unsigned int		btree_pages;

	
	struct list_head	btree_cache;
	struct list_head	btree_cache_freeable;
	struct list_head	btree_cache_freed;

	
	unsigned int		btree_cache_used;

	
	wait_queue_head_t	btree_cache_wait;
	struct task_struct	*btree_cache_alloc_lock;
	spinlock_t		btree_cannibalize_lock;

	
	atomic_t		prio_blocked;
	wait_queue_head_t	bucket_wait;

	
	atomic_t		rescale;
	
	atomic_t		search_inflight;
	
	uint16_t		min_prio;

	
	uint8_t			need_gc;
	struct gc_stat		gc_stats;
	size_t			nbuckets;
	size_t			avail_nbuckets;

	struct task_struct	*gc_thread;
	
	struct bkey		gc_done;

	
#define BCH_ENABLE_AUTO_GC	1
#define BCH_DO_AUTO_GC		2
	uint8_t			gc_after_writeback;

	
	int			gc_mark_valid;

	
	atomic_t		sectors_to_gc;
	wait_queue_head_t	gc_wait;

	struct keybuf		moving_gc_keys;
	
	struct semaphore	moving_in_flight;

	struct workqueue_struct	*moving_gc_wq;

	struct btree		*root;

#ifdef CONFIG_BCACHE_DEBUG
	struct btree		*verify_data;
	struct bset		*verify_ondisk;
	struct mutex		verify_lock;
#endif

	uint8_t			set_uuid[16];
	unsigned int		nr_uuids;
	struct uuid_entry	*uuids;
	BKEY_PADDED(uuid_bucket);
	struct closure		uuid_write;
	struct semaphore	uuid_write_mutex;

	
	mempool_t		fill_iter;

	struct bset_sort_state	sort;

	
	struct list_head	data_buckets;
	spinlock_t		data_bucket_lock;

	struct journal		journal;

#define CONGESTED_MAX		1024
	unsigned int		congested_last_us;
	atomic_t		congested;

	
	unsigned int		congested_read_threshold_us;
	unsigned int		congested_write_threshold_us;

	struct time_stats	btree_gc_time;
	struct time_stats	btree_split_time;
	struct time_stats	btree_read_time;

	atomic_long_t		cache_read_races;
	atomic_long_t		writeback_keys_done;
	atomic_long_t		writeback_keys_failed;

	atomic_long_t		reclaim;
	atomic_long_t		reclaimed_journal_buckets;
	atomic_long_t		flush_write;

	enum			{
		ON_ERROR_UNREGISTER,
		ON_ERROR_PANIC,
	}			on_error;
#define DEFAULT_IO_ERROR_LIMIT 8
	unsigned int		error_limit;
	unsigned int		error_decay;

	unsigned short		journal_delay_ms;
	bool			expensive_debug_checks;
	unsigned int		verify:1;
	unsigned int		key_merging_disabled:1;
	unsigned int		gc_always_rewrite:1;
	unsigned int		shrinker_disabled:1;
	unsigned int		copy_gc_enabled:1;
	unsigned int		idle_max_writeback_rate_enabled:1;

#define BUCKET_HASH_BITS	12
	struct hlist_head	bucket_hash[1 << BUCKET_HASH_BITS];
};

struct bbio {
	unsigned int		submit_time_us;
	union {
		struct bkey	key;
		uint64_t	_pad[3];
		
	};
	struct bio		bio;
};

#define BTREE_PRIO		USHRT_MAX
#define INITIAL_PRIO		32768U

#define btree_bytes(c)		((c)->btree_pages * PAGE_SIZE)
#define btree_blocks(b)							\
	((unsigned int) (KEY_SIZE(&b->key) >> (b)->c->block_bits))

#define btree_default_blocks(c)						\
	((unsigned int) ((PAGE_SECTORS * (c)->btree_pages) >> (c)->block_bits))

#define bucket_bytes(ca)	((ca)->sb.bucket_size << 9)
#define block_bytes(ca)		((ca)->sb.block_size << 9)

static inline unsigned int meta_bucket_pages(struct cache_sb *sb)
{
	unsigned int n, max_pages;

	max_pages = min_t(unsigned int,
			  __rounddown_pow_of_two(USHRT_MAX) / PAGE_SECTORS,
			  MAX_ORDER_NR_PAGES);

	n = sb->bucket_size / PAGE_SECTORS;
	if (n > max_pages)
		n = max_pages;

	return n;
}

static inline unsigned int meta_bucket_bytes(struct cache_sb *sb)
{
	return meta_bucket_pages(sb) << PAGE_SHIFT;
}

#define prios_per_bucket(ca)						\
	((meta_bucket_bytes(&(ca)->sb) - sizeof(struct prio_set)) /	\
	 sizeof(struct bucket_disk))

#define prio_buckets(ca)						\
	DIV_ROUND_UP((size_t) (ca)->sb.nbuckets, prios_per_bucket(ca))

static inline size_t sector_to_bucket(struct cache_set *c, sector_t s)
{
	return s >> c->bucket_bits;
}

static inline sector_t bucket_to_sector(struct cache_set *c, size_t b)
{
	return ((sector_t) b) << c->bucket_bits;
}

static inline sector_t bucket_remainder(struct cache_set *c, sector_t s)
{
	return s & (c->cache->sb.bucket_size - 1);
}

static inline size_t PTR_BUCKET_NR(struct cache_set *c,
				   const struct bkey *k,
				   unsigned int ptr)
{
	return sector_to_bucket(c, PTR_OFFSET(k, ptr));
}

static inline struct bucket *PTR_BUCKET(struct cache_set *c,
					const struct bkey *k,
					unsigned int ptr)
{
	return c->cache->buckets + PTR_BUCKET_NR(c, k, ptr);
}

static inline uint8_t gen_after(uint8_t a, uint8_t b)
{
	uint8_t r = a - b;

	return r > 128U ? 0 : r;
}

static inline uint8_t ptr_stale(struct cache_set *c, const struct bkey *k,
				unsigned int i)
{
	return gen_after(PTR_BUCKET(c, k, i)->gen, PTR_GEN(k, i));
}

static inline bool ptr_available(struct cache_set *c, const struct bkey *k,
				 unsigned int i)
{
	return (PTR_DEV(k, i) < MAX_CACHES_PER_SET) && c->cache;
}




#define csum_set(i)							\
	bch_crc64(((void *) (i)) + sizeof(uint64_t),			\
		  ((void *) bset_bkey_last(i)) -			\
		  (((void *) (i)) + sizeof(uint64_t)))



#define btree_bug(b, ...)						\
do {									\
	if (bch_cache_set_error((b)->c, __VA_ARGS__))			\
		dump_stack();						\
} while (0)

#define cache_bug(c, ...)						\
do {									\
	if (bch_cache_set_error(c, __VA_ARGS__))			\
		dump_stack();						\
} while (0)

#define btree_bug_on(cond, b, ...)					\
do {									\
	if (cond)							\
		btree_bug(b, __VA_ARGS__);				\
} while (0)

#define cache_bug_on(cond, c, ...)					\
do {									\
	if (cond)							\
		cache_bug(c, __VA_ARGS__);				\
} while (0)

#define cache_set_err_on(cond, c, ...)					\
do {									\
	if (cond)							\
		bch_cache_set_error(c, __VA_ARGS__);			\
} while (0)



#define for_each_bucket(b, ca)						\
	for (b = (ca)->buckets + (ca)->sb.first_bucket;			\
	     b < (ca)->buckets + (ca)->sb.nbuckets; b++)

static inline void cached_dev_put(struct cached_dev *dc)
{
	if (refcount_dec_and_test(&dc->count))
		schedule_work(&dc->detach);
}

static inline bool cached_dev_get(struct cached_dev *dc)
{
	if (!refcount_inc_not_zero(&dc->count))
		return false;

	
	smp_mb__after_atomic();
	return true;
}



static inline uint8_t bucket_gc_gen(struct bucket *b)
{
	return b->gen - b->last_gc;
}

#define BUCKET_GC_GEN_MAX	96U

#define kobj_attribute_write(n, fn)					\
	static struct kobj_attribute ksysfs_##n = __ATTR(n, 0200, NULL, fn)

#define kobj_attribute_rw(n, show, store)				\
	static struct kobj_attribute ksysfs_##n =			\
		__ATTR(n, 0600, show, store)

static inline void wake_up_allocators(struct cache_set *c)
{
	struct cache *ca = c->cache;

	wake_up_process(ca->alloc_thread);
}

static inline void closure_bio_submit(struct cache_set *c,
				      struct bio *bio,
				      struct closure *cl)
{
	closure_get(cl);
	if (unlikely(test_bit(CACHE_SET_IO_DISABLE, &c->flags))) {
		bio->bi_status = BLK_STS_IOERR;
		bio_endio(bio);
		return;
	}
	submit_bio_noacct(bio);
}


static inline void wait_for_kthread_stop(void)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
}



void bch_count_backing_io_errors(struct cached_dev *dc, struct bio *bio);
void bch_count_io_errors(struct cache *ca, blk_status_t error,
			 int is_read, const char *m);
void bch_bbio_count_io_errors(struct cache_set *c, struct bio *bio,
			      blk_status_t error, const char *m);
void bch_bbio_endio(struct cache_set *c, struct bio *bio,
		    blk_status_t error, const char *m);
void bch_bbio_free(struct bio *bio, struct cache_set *c);
struct bio *bch_bbio_alloc(struct cache_set *c);

void __bch_submit_bbio(struct bio *bio, struct cache_set *c);
void bch_submit_bbio(struct bio *bio, struct cache_set *c,
		     struct bkey *k, unsigned int ptr);

uint8_t bch_inc_gen(struct cache *ca, struct bucket *b);
void bch_rescale_priorities(struct cache_set *c, int sectors);

bool bch_can_invalidate_bucket(struct cache *ca, struct bucket *b);
void __bch_invalidate_one_bucket(struct cache *ca, struct bucket *b);

void __bch_bucket_free(struct cache *ca, struct bucket *b);
void bch_bucket_free(struct cache_set *c, struct bkey *k);

long bch_bucket_alloc(struct cache *ca, unsigned int reserve, bool wait);
int __bch_bucket_alloc_set(struct cache_set *c, unsigned int reserve,
			   struct bkey *k, bool wait);
int bch_bucket_alloc_set(struct cache_set *c, unsigned int reserve,
			 struct bkey *k, bool wait);
bool bch_alloc_sectors(struct cache_set *c, struct bkey *k,
		       unsigned int sectors, unsigned int write_point,
		       unsigned int write_prio, bool wait);
bool bch_cached_dev_error(struct cached_dev *dc);

__printf(2, 3)
bool bch_cache_set_error(struct cache_set *c, const char *fmt, ...);

int bch_prio_write(struct cache *ca, bool wait);
void bch_write_bdev_super(struct cached_dev *dc, struct closure *parent);

extern struct workqueue_struct *bcache_wq;
extern struct workqueue_struct *bch_journal_wq;
extern struct workqueue_struct *bch_flush_wq;
extern struct mutex bch_register_lock;
extern struct list_head bch_cache_sets;

extern const struct kobj_type bch_cached_dev_ktype;
extern const struct kobj_type bch_flash_dev_ktype;
extern const struct kobj_type bch_cache_set_ktype;
extern const struct kobj_type bch_cache_set_internal_ktype;
extern const struct kobj_type bch_cache_ktype;

void bch_cached_dev_release(struct kobject *kobj);
void bch_flash_dev_release(struct kobject *kobj);
void bch_cache_set_release(struct kobject *kobj);
void bch_cache_release(struct kobject *kobj);

int bch_uuid_write(struct cache_set *c);
void bcache_write_super(struct cache_set *c);

int bch_flash_dev_create(struct cache_set *c, uint64_t size);

int bch_cached_dev_attach(struct cached_dev *dc, struct cache_set *c,
			  uint8_t *set_uuid);
void bch_cached_dev_detach(struct cached_dev *dc);
int bch_cached_dev_run(struct cached_dev *dc);
void bcache_device_stop(struct bcache_device *d);

void bch_cache_set_unregister(struct cache_set *c);
void bch_cache_set_stop(struct cache_set *c);

struct cache_set *bch_cache_set_alloc(struct cache_sb *sb);
void bch_btree_cache_free(struct cache_set *c);
int bch_btree_cache_alloc(struct cache_set *c);
void bch_moving_init_cache_set(struct cache_set *c);
int bch_open_buckets_alloc(struct cache_set *c);
void bch_open_buckets_free(struct cache_set *c);

int bch_cache_allocator_start(struct cache *ca);

void bch_debug_exit(void);
void bch_debug_init(void);
void bch_request_exit(void);
int bch_request_init(void);
void bch_btree_exit(void);
int bch_btree_init(void);

#endif 

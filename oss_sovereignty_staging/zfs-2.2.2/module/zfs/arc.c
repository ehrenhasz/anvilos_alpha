 
 

 

 

 

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/spa_impl.h>
#include <sys/zio_compress.h>
#include <sys/zio_checksum.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/zfs_refcount.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/dsl_pool.h>
#include <sys/multilist.h>
#include <sys/abd.h>
#include <sys/zil.h>
#include <sys/fm/fs/zfs.h>
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/zthr.h>
#include <zfs_fletcher.h>
#include <sys/arc_impl.h>
#include <sys/trace_zfs.h>
#include <sys/aggsum.h>
#include <sys/wmsum.h>
#include <cityhash.h>
#include <sys/vdev_trim.h>
#include <sys/zfs_racct.h>
#include <sys/zstd/zstd.h>

#ifndef _KERNEL
 
boolean_t arc_watch = B_FALSE;
#endif

 
static zthr_t *arc_reap_zthr;

 
static zthr_t *arc_evict_zthr;
static arc_buf_hdr_t **arc_state_evict_markers;
static int arc_state_evict_marker_count;

static kmutex_t arc_evict_lock;
static boolean_t arc_evict_needed = B_FALSE;
static clock_t arc_last_uncached_flush;

 
static uint64_t arc_evict_count;

 
static list_t arc_evict_waiters;

 
static uint_t zfs_arc_eviction_pct = 200;

 
static uint_t zfs_arc_evict_batch_limit = 10;

 
uint_t arc_grow_retry = 5;

 
static const int arc_kmem_cache_reap_retry_ms = 1000;

 
static int zfs_arc_overflow_shift = 8;

 
uint_t arc_shrink_shift = 7;

 
#ifdef _KERNEL
uint_t zfs_arc_pc_percent = 0;
#endif

 
uint_t		arc_no_grow_shift = 5;


 
static uint_t		arc_min_prefetch_ms;
static uint_t		arc_min_prescient_prefetch_ms;

 
uint_t arc_lotsfree_percent = 10;

 
boolean_t arc_warm;

 
uint64_t zfs_arc_max = 0;
uint64_t zfs_arc_min = 0;
static uint64_t zfs_arc_dnode_limit = 0;
static uint_t zfs_arc_dnode_reduce_percent = 10;
static uint_t zfs_arc_grow_retry = 0;
static uint_t zfs_arc_shrink_shift = 0;
uint_t zfs_arc_average_blocksize = 8 * 1024;  

 
static const unsigned long zfs_arc_dirty_limit_percent = 50;
static const unsigned long zfs_arc_anon_limit_percent = 25;
static const unsigned long zfs_arc_pool_dirty_percent = 20;

 
int zfs_compressed_arc_enabled = B_TRUE;

 
static uint_t zfs_arc_meta_balance = 500;

 
static uint_t zfs_arc_dnode_limit_percent = 10;

 
static uint64_t zfs_arc_sys_free = 0;
static uint_t zfs_arc_min_prefetch_ms = 0;
static uint_t zfs_arc_min_prescient_prefetch_ms = 0;
static uint_t zfs_arc_lotsfree_percent = 10;

 
static int zfs_arc_prune_task_threads = 1;

 
arc_state_t ARC_anon;
arc_state_t ARC_mru;
arc_state_t ARC_mru_ghost;
arc_state_t ARC_mfu;
arc_state_t ARC_mfu_ghost;
arc_state_t ARC_l2c_only;
arc_state_t ARC_uncached;

arc_stats_t arc_stats = {
	{ "hits",			KSTAT_DATA_UINT64 },
	{ "iohits",			KSTAT_DATA_UINT64 },
	{ "misses",			KSTAT_DATA_UINT64 },
	{ "demand_data_hits",		KSTAT_DATA_UINT64 },
	{ "demand_data_iohits",		KSTAT_DATA_UINT64 },
	{ "demand_data_misses",		KSTAT_DATA_UINT64 },
	{ "demand_metadata_hits",	KSTAT_DATA_UINT64 },
	{ "demand_metadata_iohits",	KSTAT_DATA_UINT64 },
	{ "demand_metadata_misses",	KSTAT_DATA_UINT64 },
	{ "prefetch_data_hits",		KSTAT_DATA_UINT64 },
	{ "prefetch_data_iohits",	KSTAT_DATA_UINT64 },
	{ "prefetch_data_misses",	KSTAT_DATA_UINT64 },
	{ "prefetch_metadata_hits",	KSTAT_DATA_UINT64 },
	{ "prefetch_metadata_iohits",	KSTAT_DATA_UINT64 },
	{ "prefetch_metadata_misses",	KSTAT_DATA_UINT64 },
	{ "mru_hits",			KSTAT_DATA_UINT64 },
	{ "mru_ghost_hits",		KSTAT_DATA_UINT64 },
	{ "mfu_hits",			KSTAT_DATA_UINT64 },
	{ "mfu_ghost_hits",		KSTAT_DATA_UINT64 },
	{ "uncached_hits",		KSTAT_DATA_UINT64 },
	{ "deleted",			KSTAT_DATA_UINT64 },
	{ "mutex_miss",			KSTAT_DATA_UINT64 },
	{ "access_skip",		KSTAT_DATA_UINT64 },
	{ "evict_skip",			KSTAT_DATA_UINT64 },
	{ "evict_not_enough",		KSTAT_DATA_UINT64 },
	{ "evict_l2_cached",		KSTAT_DATA_UINT64 },
	{ "evict_l2_eligible",		KSTAT_DATA_UINT64 },
	{ "evict_l2_eligible_mfu",	KSTAT_DATA_UINT64 },
	{ "evict_l2_eligible_mru",	KSTAT_DATA_UINT64 },
	{ "evict_l2_ineligible",	KSTAT_DATA_UINT64 },
	{ "evict_l2_skip",		KSTAT_DATA_UINT64 },
	{ "hash_elements",		KSTAT_DATA_UINT64 },
	{ "hash_elements_max",		KSTAT_DATA_UINT64 },
	{ "hash_collisions",		KSTAT_DATA_UINT64 },
	{ "hash_chains",		KSTAT_DATA_UINT64 },
	{ "hash_chain_max",		KSTAT_DATA_UINT64 },
	{ "meta",			KSTAT_DATA_UINT64 },
	{ "pd",				KSTAT_DATA_UINT64 },
	{ "pm",				KSTAT_DATA_UINT64 },
	{ "c",				KSTAT_DATA_UINT64 },
	{ "c_min",			KSTAT_DATA_UINT64 },
	{ "c_max",			KSTAT_DATA_UINT64 },
	{ "size",			KSTAT_DATA_UINT64 },
	{ "compressed_size",		KSTAT_DATA_UINT64 },
	{ "uncompressed_size",		KSTAT_DATA_UINT64 },
	{ "overhead_size",		KSTAT_DATA_UINT64 },
	{ "hdr_size",			KSTAT_DATA_UINT64 },
	{ "data_size",			KSTAT_DATA_UINT64 },
	{ "metadata_size",		KSTAT_DATA_UINT64 },
	{ "dbuf_size",			KSTAT_DATA_UINT64 },
	{ "dnode_size",			KSTAT_DATA_UINT64 },
	{ "bonus_size",			KSTAT_DATA_UINT64 },
#if defined(COMPAT_FREEBSD11)
	{ "other_size",			KSTAT_DATA_UINT64 },
#endif
	{ "anon_size",			KSTAT_DATA_UINT64 },
	{ "anon_data",			KSTAT_DATA_UINT64 },
	{ "anon_metadata",		KSTAT_DATA_UINT64 },
	{ "anon_evictable_data",	KSTAT_DATA_UINT64 },
	{ "anon_evictable_metadata",	KSTAT_DATA_UINT64 },
	{ "mru_size",			KSTAT_DATA_UINT64 },
	{ "mru_data",			KSTAT_DATA_UINT64 },
	{ "mru_metadata",		KSTAT_DATA_UINT64 },
	{ "mru_evictable_data",		KSTAT_DATA_UINT64 },
	{ "mru_evictable_metadata",	KSTAT_DATA_UINT64 },
	{ "mru_ghost_size",		KSTAT_DATA_UINT64 },
	{ "mru_ghost_data",		KSTAT_DATA_UINT64 },
	{ "mru_ghost_metadata",		KSTAT_DATA_UINT64 },
	{ "mru_ghost_evictable_data",	KSTAT_DATA_UINT64 },
	{ "mru_ghost_evictable_metadata", KSTAT_DATA_UINT64 },
	{ "mfu_size",			KSTAT_DATA_UINT64 },
	{ "mfu_data",			KSTAT_DATA_UINT64 },
	{ "mfu_metadata",		KSTAT_DATA_UINT64 },
	{ "mfu_evictable_data",		KSTAT_DATA_UINT64 },
	{ "mfu_evictable_metadata",	KSTAT_DATA_UINT64 },
	{ "mfu_ghost_size",		KSTAT_DATA_UINT64 },
	{ "mfu_ghost_data",		KSTAT_DATA_UINT64 },
	{ "mfu_ghost_metadata",		KSTAT_DATA_UINT64 },
	{ "mfu_ghost_evictable_data",	KSTAT_DATA_UINT64 },
	{ "mfu_ghost_evictable_metadata", KSTAT_DATA_UINT64 },
	{ "uncached_size",		KSTAT_DATA_UINT64 },
	{ "uncached_data",		KSTAT_DATA_UINT64 },
	{ "uncached_metadata",		KSTAT_DATA_UINT64 },
	{ "uncached_evictable_data",	KSTAT_DATA_UINT64 },
	{ "uncached_evictable_metadata", KSTAT_DATA_UINT64 },
	{ "l2_hits",			KSTAT_DATA_UINT64 },
	{ "l2_misses",			KSTAT_DATA_UINT64 },
	{ "l2_prefetch_asize",		KSTAT_DATA_UINT64 },
	{ "l2_mru_asize",		KSTAT_DATA_UINT64 },
	{ "l2_mfu_asize",		KSTAT_DATA_UINT64 },
	{ "l2_bufc_data_asize",		KSTAT_DATA_UINT64 },
	{ "l2_bufc_metadata_asize",	KSTAT_DATA_UINT64 },
	{ "l2_feeds",			KSTAT_DATA_UINT64 },
	{ "l2_rw_clash",		KSTAT_DATA_UINT64 },
	{ "l2_read_bytes",		KSTAT_DATA_UINT64 },
	{ "l2_write_bytes",		KSTAT_DATA_UINT64 },
	{ "l2_writes_sent",		KSTAT_DATA_UINT64 },
	{ "l2_writes_done",		KSTAT_DATA_UINT64 },
	{ "l2_writes_error",		KSTAT_DATA_UINT64 },
	{ "l2_writes_lock_retry",	KSTAT_DATA_UINT64 },
	{ "l2_evict_lock_retry",	KSTAT_DATA_UINT64 },
	{ "l2_evict_reading",		KSTAT_DATA_UINT64 },
	{ "l2_evict_l1cached",		KSTAT_DATA_UINT64 },
	{ "l2_free_on_write",		KSTAT_DATA_UINT64 },
	{ "l2_abort_lowmem",		KSTAT_DATA_UINT64 },
	{ "l2_cksum_bad",		KSTAT_DATA_UINT64 },
	{ "l2_io_error",		KSTAT_DATA_UINT64 },
	{ "l2_size",			KSTAT_DATA_UINT64 },
	{ "l2_asize",			KSTAT_DATA_UINT64 },
	{ "l2_hdr_size",		KSTAT_DATA_UINT64 },
	{ "l2_log_blk_writes",		KSTAT_DATA_UINT64 },
	{ "l2_log_blk_avg_asize",	KSTAT_DATA_UINT64 },
	{ "l2_log_blk_asize",		KSTAT_DATA_UINT64 },
	{ "l2_log_blk_count",		KSTAT_DATA_UINT64 },
	{ "l2_data_to_meta_ratio",	KSTAT_DATA_UINT64 },
	{ "l2_rebuild_success",		KSTAT_DATA_UINT64 },
	{ "l2_rebuild_unsupported",	KSTAT_DATA_UINT64 },
	{ "l2_rebuild_io_errors",	KSTAT_DATA_UINT64 },
	{ "l2_rebuild_dh_errors",	KSTAT_DATA_UINT64 },
	{ "l2_rebuild_cksum_lb_errors",	KSTAT_DATA_UINT64 },
	{ "l2_rebuild_lowmem",		KSTAT_DATA_UINT64 },
	{ "l2_rebuild_size",		KSTAT_DATA_UINT64 },
	{ "l2_rebuild_asize",		KSTAT_DATA_UINT64 },
	{ "l2_rebuild_bufs",		KSTAT_DATA_UINT64 },
	{ "l2_rebuild_bufs_precached",	KSTAT_DATA_UINT64 },
	{ "l2_rebuild_log_blks",	KSTAT_DATA_UINT64 },
	{ "memory_throttle_count",	KSTAT_DATA_UINT64 },
	{ "memory_direct_count",	KSTAT_DATA_UINT64 },
	{ "memory_indirect_count",	KSTAT_DATA_UINT64 },
	{ "memory_all_bytes",		KSTAT_DATA_UINT64 },
	{ "memory_free_bytes",		KSTAT_DATA_UINT64 },
	{ "memory_available_bytes",	KSTAT_DATA_INT64 },
	{ "arc_no_grow",		KSTAT_DATA_UINT64 },
	{ "arc_tempreserve",		KSTAT_DATA_UINT64 },
	{ "arc_loaned_bytes",		KSTAT_DATA_UINT64 },
	{ "arc_prune",			KSTAT_DATA_UINT64 },
	{ "arc_meta_used",		KSTAT_DATA_UINT64 },
	{ "arc_dnode_limit",		KSTAT_DATA_UINT64 },
	{ "async_upgrade_sync",		KSTAT_DATA_UINT64 },
	{ "predictive_prefetch", KSTAT_DATA_UINT64 },
	{ "demand_hit_predictive_prefetch", KSTAT_DATA_UINT64 },
	{ "demand_iohit_predictive_prefetch", KSTAT_DATA_UINT64 },
	{ "prescient_prefetch", KSTAT_DATA_UINT64 },
	{ "demand_hit_prescient_prefetch", KSTAT_DATA_UINT64 },
	{ "demand_iohit_prescient_prefetch", KSTAT_DATA_UINT64 },
	{ "arc_need_free",		KSTAT_DATA_UINT64 },
	{ "arc_sys_free",		KSTAT_DATA_UINT64 },
	{ "arc_raw_size",		KSTAT_DATA_UINT64 },
	{ "cached_only_in_progress",	KSTAT_DATA_UINT64 },
	{ "abd_chunk_waste_size",	KSTAT_DATA_UINT64 },
};

arc_sums_t arc_sums;

#define	ARCSTAT_MAX(stat, val) {					\
	uint64_t m;							\
	while ((val) > (m = arc_stats.stat.value.ui64) &&		\
	    (m != atomic_cas_64(&arc_stats.stat.value.ui64, m, (val))))	\
		continue;						\
}

 
#define	ARCSTAT_CONDSTAT(cond1, stat1, notstat1, cond2, stat2, notstat2, stat) \
	if (cond1) {							\
		if (cond2) {						\
			ARCSTAT_BUMP(arcstat_##stat1##_##stat2##_##stat); \
		} else {						\
			ARCSTAT_BUMP(arcstat_##stat1##_##notstat2##_##stat); \
		}							\
	} else {							\
		if (cond2) {						\
			ARCSTAT_BUMP(arcstat_##notstat1##_##stat2##_##stat); \
		} else {						\
			ARCSTAT_BUMP(arcstat_##notstat1##_##notstat2##_##stat);\
		}							\
	}

 
#define	ARCSTAT_F_AVG_FACTOR	3
#define	ARCSTAT_F_AVG(stat, value) \
	do { \
		uint64_t x = ARCSTAT(stat); \
		x = x - x / ARCSTAT_F_AVG_FACTOR + \
		    (value) / ARCSTAT_F_AVG_FACTOR; \
		ARCSTAT(stat) = x; \
	} while (0)

static kstat_t			*arc_ksp;

 
#define	arc_tempreserve	ARCSTAT(arcstat_tempreserve)
#define	arc_loaned_bytes	ARCSTAT(arcstat_loaned_bytes)
#define	arc_dnode_limit	ARCSTAT(arcstat_dnode_limit)  
#define	arc_need_free	ARCSTAT(arcstat_need_free)  

hrtime_t arc_growtime;
list_t arc_prune_list;
kmutex_t arc_prune_mtx;
taskq_t *arc_prune_taskq;

#define	GHOST_STATE(state)	\
	((state) == arc_mru_ghost || (state) == arc_mfu_ghost ||	\
	(state) == arc_l2c_only)

#define	HDR_IN_HASH_TABLE(hdr)	((hdr)->b_flags & ARC_FLAG_IN_HASH_TABLE)
#define	HDR_IO_IN_PROGRESS(hdr)	((hdr)->b_flags & ARC_FLAG_IO_IN_PROGRESS)
#define	HDR_IO_ERROR(hdr)	((hdr)->b_flags & ARC_FLAG_IO_ERROR)
#define	HDR_PREFETCH(hdr)	((hdr)->b_flags & ARC_FLAG_PREFETCH)
#define	HDR_PRESCIENT_PREFETCH(hdr)	\
	((hdr)->b_flags & ARC_FLAG_PRESCIENT_PREFETCH)
#define	HDR_COMPRESSION_ENABLED(hdr)	\
	((hdr)->b_flags & ARC_FLAG_COMPRESSED_ARC)

#define	HDR_L2CACHE(hdr)	((hdr)->b_flags & ARC_FLAG_L2CACHE)
#define	HDR_UNCACHED(hdr)	((hdr)->b_flags & ARC_FLAG_UNCACHED)
#define	HDR_L2_READING(hdr)	\
	(((hdr)->b_flags & ARC_FLAG_IO_IN_PROGRESS) &&	\
	((hdr)->b_flags & ARC_FLAG_HAS_L2HDR))
#define	HDR_L2_WRITING(hdr)	((hdr)->b_flags & ARC_FLAG_L2_WRITING)
#define	HDR_L2_EVICTED(hdr)	((hdr)->b_flags & ARC_FLAG_L2_EVICTED)
#define	HDR_L2_WRITE_HEAD(hdr)	((hdr)->b_flags & ARC_FLAG_L2_WRITE_HEAD)
#define	HDR_PROTECTED(hdr)	((hdr)->b_flags & ARC_FLAG_PROTECTED)
#define	HDR_NOAUTH(hdr)		((hdr)->b_flags & ARC_FLAG_NOAUTH)
#define	HDR_SHARED_DATA(hdr)	((hdr)->b_flags & ARC_FLAG_SHARED_DATA)

#define	HDR_ISTYPE_METADATA(hdr)	\
	((hdr)->b_flags & ARC_FLAG_BUFC_METADATA)
#define	HDR_ISTYPE_DATA(hdr)	(!HDR_ISTYPE_METADATA(hdr))

#define	HDR_HAS_L1HDR(hdr)	((hdr)->b_flags & ARC_FLAG_HAS_L1HDR)
#define	HDR_HAS_L2HDR(hdr)	((hdr)->b_flags & ARC_FLAG_HAS_L2HDR)
#define	HDR_HAS_RABD(hdr)	\
	(HDR_HAS_L1HDR(hdr) && HDR_PROTECTED(hdr) &&	\
	(hdr)->b_crypt_hdr.b_rabd != NULL)
#define	HDR_ENCRYPTED(hdr)	\
	(HDR_PROTECTED(hdr) && DMU_OT_IS_ENCRYPTED((hdr)->b_crypt_hdr.b_ot))
#define	HDR_AUTHENTICATED(hdr)	\
	(HDR_PROTECTED(hdr) && !DMU_OT_IS_ENCRYPTED((hdr)->b_crypt_hdr.b_ot))

 
#define	HDR_COMPRESS_OFFSET	(highbit64(ARC_FLAG_COMPRESS_0) - 1)

#define	HDR_GET_COMPRESS(hdr)	((enum zio_compress)BF32_GET((hdr)->b_flags, \
	HDR_COMPRESS_OFFSET, SPA_COMPRESSBITS))
#define	HDR_SET_COMPRESS(hdr, cmp) BF32_SET((hdr)->b_flags, \
	HDR_COMPRESS_OFFSET, SPA_COMPRESSBITS, (cmp));

#define	ARC_BUF_LAST(buf)	((buf)->b_next == NULL)
#define	ARC_BUF_SHARED(buf)	((buf)->b_flags & ARC_BUF_FLAG_SHARED)
#define	ARC_BUF_COMPRESSED(buf)	((buf)->b_flags & ARC_BUF_FLAG_COMPRESSED)
#define	ARC_BUF_ENCRYPTED(buf)	((buf)->b_flags & ARC_BUF_FLAG_ENCRYPTED)

 

#define	HDR_FULL_SIZE ((int64_t)sizeof (arc_buf_hdr_t))
#define	HDR_L2ONLY_SIZE ((int64_t)offsetof(arc_buf_hdr_t, b_l1hdr))

 

#define	BUF_LOCKS 2048
typedef struct buf_hash_table {
	uint64_t ht_mask;
	arc_buf_hdr_t **ht_table;
	kmutex_t ht_locks[BUF_LOCKS] ____cacheline_aligned;
} buf_hash_table_t;

static buf_hash_table_t buf_hash_table;

#define	BUF_HASH_INDEX(spa, dva, birth) \
	(buf_hash(spa, dva, birth) & buf_hash_table.ht_mask)
#define	BUF_HASH_LOCK(idx)	(&buf_hash_table.ht_locks[idx & (BUF_LOCKS-1)])
#define	HDR_LOCK(hdr) \
	(BUF_HASH_LOCK(BUF_HASH_INDEX(hdr->b_spa, &hdr->b_dva, hdr->b_birth)))

uint64_t zfs_crc64_table[256];

 

#define	L2ARC_WRITE_SIZE	(8 * 1024 * 1024)	 
#define	L2ARC_HEADROOM		2			 

 
#define	L2ARC_HEADROOM_BOOST	200
#define	L2ARC_FEED_SECS		1		 
#define	L2ARC_FEED_MIN_MS	200		 

 
#define	L2ARC_FEED_TYPES	4

 
uint64_t l2arc_write_max = L2ARC_WRITE_SIZE;	 
uint64_t l2arc_write_boost = L2ARC_WRITE_SIZE;	 
uint64_t l2arc_headroom = L2ARC_HEADROOM;	 
uint64_t l2arc_headroom_boost = L2ARC_HEADROOM_BOOST;
uint64_t l2arc_feed_secs = L2ARC_FEED_SECS;	 
uint64_t l2arc_feed_min_ms = L2ARC_FEED_MIN_MS;	 
int l2arc_noprefetch = B_TRUE;			 
int l2arc_feed_again = B_TRUE;			 
int l2arc_norw = B_FALSE;			 
static uint_t l2arc_meta_percent = 33;	 

 
static list_t L2ARC_dev_list;			 
static list_t *l2arc_dev_list;			 
static kmutex_t l2arc_dev_mtx;			 
static l2arc_dev_t *l2arc_dev_last;		 
static list_t L2ARC_free_on_write;		 
static list_t *l2arc_free_on_write;		 
static kmutex_t l2arc_free_on_write_mtx;	 
static uint64_t l2arc_ndev;			 

typedef struct l2arc_read_callback {
	arc_buf_hdr_t		*l2rcb_hdr;		 
	blkptr_t		l2rcb_bp;		 
	zbookmark_phys_t	l2rcb_zb;		 
	int			l2rcb_flags;		 
	abd_t			*l2rcb_abd;		 
} l2arc_read_callback_t;

typedef struct l2arc_data_free {
	 
	abd_t		*l2df_abd;
	size_t		l2df_size;
	arc_buf_contents_t l2df_type;
	list_node_t	l2df_list_node;
} l2arc_data_free_t;

typedef enum arc_fill_flags {
	ARC_FILL_LOCKED		= 1 << 0,  
	ARC_FILL_COMPRESSED	= 1 << 1,  
	ARC_FILL_ENCRYPTED	= 1 << 2,  
	ARC_FILL_NOAUTH		= 1 << 3,  
	ARC_FILL_IN_PLACE	= 1 << 4   
} arc_fill_flags_t;

typedef enum arc_ovf_level {
	ARC_OVF_NONE,			 
	ARC_OVF_SOME,			 
	ARC_OVF_SEVERE			 
} arc_ovf_level_t;

static kmutex_t l2arc_feed_thr_lock;
static kcondvar_t l2arc_feed_thr_cv;
static uint8_t l2arc_thread_exit;

static kmutex_t l2arc_rebuild_thr_lock;
static kcondvar_t l2arc_rebuild_thr_cv;

enum arc_hdr_alloc_flags {
	ARC_HDR_ALLOC_RDATA = 0x1,
	ARC_HDR_USE_RESERVE = 0x4,
	ARC_HDR_ALLOC_LINEAR = 0x8,
};


static abd_t *arc_get_data_abd(arc_buf_hdr_t *, uint64_t, const void *, int);
static void *arc_get_data_buf(arc_buf_hdr_t *, uint64_t, const void *);
static void arc_get_data_impl(arc_buf_hdr_t *, uint64_t, const void *, int);
static void arc_free_data_abd(arc_buf_hdr_t *, abd_t *, uint64_t, const void *);
static void arc_free_data_buf(arc_buf_hdr_t *, void *, uint64_t, const void *);
static void arc_free_data_impl(arc_buf_hdr_t *hdr, uint64_t size,
    const void *tag);
static void arc_hdr_free_abd(arc_buf_hdr_t *, boolean_t);
static void arc_hdr_alloc_abd(arc_buf_hdr_t *, int);
static void arc_hdr_destroy(arc_buf_hdr_t *);
static void arc_access(arc_buf_hdr_t *, arc_flags_t, boolean_t);
static void arc_buf_watch(arc_buf_t *);
static void arc_change_state(arc_state_t *, arc_buf_hdr_t *);

static arc_buf_contents_t arc_buf_type(arc_buf_hdr_t *);
static uint32_t arc_bufc_to_flags(arc_buf_contents_t);
static inline void arc_hdr_set_flags(arc_buf_hdr_t *hdr, arc_flags_t flags);
static inline void arc_hdr_clear_flags(arc_buf_hdr_t *hdr, arc_flags_t flags);

static boolean_t l2arc_write_eligible(uint64_t, arc_buf_hdr_t *);
static void l2arc_read_done(zio_t *);
static void l2arc_do_free_on_write(void);
static void l2arc_hdr_arcstats_update(arc_buf_hdr_t *hdr, boolean_t incr,
    boolean_t state_only);

static void arc_prune_async(uint64_t adjust);

#define	l2arc_hdr_arcstats_increment(hdr) \
	l2arc_hdr_arcstats_update((hdr), B_TRUE, B_FALSE)
#define	l2arc_hdr_arcstats_decrement(hdr) \
	l2arc_hdr_arcstats_update((hdr), B_FALSE, B_FALSE)
#define	l2arc_hdr_arcstats_increment_state(hdr) \
	l2arc_hdr_arcstats_update((hdr), B_TRUE, B_TRUE)
#define	l2arc_hdr_arcstats_decrement_state(hdr) \
	l2arc_hdr_arcstats_update((hdr), B_FALSE, B_TRUE)

 
int l2arc_exclude_special = 0;

 
static int l2arc_mfuonly = 0;

 
static uint64_t l2arc_trim_ahead = 0;

 
static int l2arc_rebuild_enabled = B_TRUE;
static uint64_t l2arc_rebuild_blocks_min_l2size = 1024 * 1024 * 1024;

 
void l2arc_rebuild_vdev(vdev_t *vd, boolean_t reopen);
static __attribute__((noreturn)) void l2arc_dev_rebuild_thread(void *arg);
static int l2arc_rebuild(l2arc_dev_t *dev);

 
static int l2arc_dev_hdr_read(l2arc_dev_t *dev);
static int l2arc_log_blk_read(l2arc_dev_t *dev,
    const l2arc_log_blkptr_t *this_lp, const l2arc_log_blkptr_t *next_lp,
    l2arc_log_blk_phys_t *this_lb, l2arc_log_blk_phys_t *next_lb,
    zio_t *this_io, zio_t **next_io);
static zio_t *l2arc_log_blk_fetch(vdev_t *vd,
    const l2arc_log_blkptr_t *lp, l2arc_log_blk_phys_t *lb);
static void l2arc_log_blk_fetch_abort(zio_t *zio);

 
static void l2arc_log_blk_restore(l2arc_dev_t *dev,
    const l2arc_log_blk_phys_t *lb, uint64_t lb_asize);
static void l2arc_hdr_restore(const l2arc_log_ent_phys_t *le,
    l2arc_dev_t *dev);

 
static uint64_t l2arc_log_blk_commit(l2arc_dev_t *dev, zio_t *pio,
    l2arc_write_callback_t *cb);

 
boolean_t l2arc_log_blkptr_valid(l2arc_dev_t *dev,
    const l2arc_log_blkptr_t *lbp);
static boolean_t l2arc_log_blk_insert(l2arc_dev_t *dev,
    const arc_buf_hdr_t *ab);
boolean_t l2arc_range_check_overlap(uint64_t bottom,
    uint64_t top, uint64_t check);
static void l2arc_blk_fetch_done(zio_t *zio);
static inline uint64_t
    l2arc_log_blk_overhead(uint64_t write_sz, l2arc_dev_t *dev);

 
static uint64_t
buf_hash(uint64_t spa, const dva_t *dva, uint64_t birth)
{
	return (cityhash4(spa, dva->dva_word[0], dva->dva_word[1], birth));
}

#define	HDR_EMPTY(hdr)						\
	((hdr)->b_dva.dva_word[0] == 0 &&			\
	(hdr)->b_dva.dva_word[1] == 0)

#define	HDR_EMPTY_OR_LOCKED(hdr)				\
	(HDR_EMPTY(hdr) || MUTEX_HELD(HDR_LOCK(hdr)))

#define	HDR_EQUAL(spa, dva, birth, hdr)				\
	((hdr)->b_dva.dva_word[0] == (dva)->dva_word[0]) &&	\
	((hdr)->b_dva.dva_word[1] == (dva)->dva_word[1]) &&	\
	((hdr)->b_birth == birth) && ((hdr)->b_spa == spa)

static void
buf_discard_identity(arc_buf_hdr_t *hdr)
{
	hdr->b_dva.dva_word[0] = 0;
	hdr->b_dva.dva_word[1] = 0;
	hdr->b_birth = 0;
}

static arc_buf_hdr_t *
buf_hash_find(uint64_t spa, const blkptr_t *bp, kmutex_t **lockp)
{
	const dva_t *dva = BP_IDENTITY(bp);
	uint64_t birth = BP_PHYSICAL_BIRTH(bp);
	uint64_t idx = BUF_HASH_INDEX(spa, dva, birth);
	kmutex_t *hash_lock = BUF_HASH_LOCK(idx);
	arc_buf_hdr_t *hdr;

	mutex_enter(hash_lock);
	for (hdr = buf_hash_table.ht_table[idx]; hdr != NULL;
	    hdr = hdr->b_hash_next) {
		if (HDR_EQUAL(spa, dva, birth, hdr)) {
			*lockp = hash_lock;
			return (hdr);
		}
	}
	mutex_exit(hash_lock);
	*lockp = NULL;
	return (NULL);
}

 
static arc_buf_hdr_t *
buf_hash_insert(arc_buf_hdr_t *hdr, kmutex_t **lockp)
{
	uint64_t idx = BUF_HASH_INDEX(hdr->b_spa, &hdr->b_dva, hdr->b_birth);
	kmutex_t *hash_lock = BUF_HASH_LOCK(idx);
	arc_buf_hdr_t *fhdr;
	uint32_t i;

	ASSERT(!DVA_IS_EMPTY(&hdr->b_dva));
	ASSERT(hdr->b_birth != 0);
	ASSERT(!HDR_IN_HASH_TABLE(hdr));

	if (lockp != NULL) {
		*lockp = hash_lock;
		mutex_enter(hash_lock);
	} else {
		ASSERT(MUTEX_HELD(hash_lock));
	}

	for (fhdr = buf_hash_table.ht_table[idx], i = 0; fhdr != NULL;
	    fhdr = fhdr->b_hash_next, i++) {
		if (HDR_EQUAL(hdr->b_spa, &hdr->b_dva, hdr->b_birth, fhdr))
			return (fhdr);
	}

	hdr->b_hash_next = buf_hash_table.ht_table[idx];
	buf_hash_table.ht_table[idx] = hdr;
	arc_hdr_set_flags(hdr, ARC_FLAG_IN_HASH_TABLE);

	 
	if (i > 0) {
		ARCSTAT_BUMP(arcstat_hash_collisions);
		if (i == 1)
			ARCSTAT_BUMP(arcstat_hash_chains);

		ARCSTAT_MAX(arcstat_hash_chain_max, i);
	}
	uint64_t he = atomic_inc_64_nv(
	    &arc_stats.arcstat_hash_elements.value.ui64);
	ARCSTAT_MAX(arcstat_hash_elements_max, he);

	return (NULL);
}

static void
buf_hash_remove(arc_buf_hdr_t *hdr)
{
	arc_buf_hdr_t *fhdr, **hdrp;
	uint64_t idx = BUF_HASH_INDEX(hdr->b_spa, &hdr->b_dva, hdr->b_birth);

	ASSERT(MUTEX_HELD(BUF_HASH_LOCK(idx)));
	ASSERT(HDR_IN_HASH_TABLE(hdr));

	hdrp = &buf_hash_table.ht_table[idx];
	while ((fhdr = *hdrp) != hdr) {
		ASSERT3P(fhdr, !=, NULL);
		hdrp = &fhdr->b_hash_next;
	}
	*hdrp = hdr->b_hash_next;
	hdr->b_hash_next = NULL;
	arc_hdr_clear_flags(hdr, ARC_FLAG_IN_HASH_TABLE);

	 
	atomic_dec_64(&arc_stats.arcstat_hash_elements.value.ui64);

	if (buf_hash_table.ht_table[idx] &&
	    buf_hash_table.ht_table[idx]->b_hash_next == NULL)
		ARCSTAT_BUMPDOWN(arcstat_hash_chains);
}

 

static kmem_cache_t *hdr_full_cache;
static kmem_cache_t *hdr_l2only_cache;
static kmem_cache_t *buf_cache;

static void
buf_fini(void)
{
#if defined(_KERNEL)
	 
	vmem_free(buf_hash_table.ht_table,
	    (buf_hash_table.ht_mask + 1) * sizeof (void *));
#else
	kmem_free(buf_hash_table.ht_table,
	    (buf_hash_table.ht_mask + 1) * sizeof (void *));
#endif
	for (int i = 0; i < BUF_LOCKS; i++)
		mutex_destroy(BUF_HASH_LOCK(i));
	kmem_cache_destroy(hdr_full_cache);
	kmem_cache_destroy(hdr_l2only_cache);
	kmem_cache_destroy(buf_cache);
}

 
static int
hdr_full_cons(void *vbuf, void *unused, int kmflag)
{
	(void) unused, (void) kmflag;
	arc_buf_hdr_t *hdr = vbuf;

	memset(hdr, 0, HDR_FULL_SIZE);
	hdr->b_l1hdr.b_byteswap = DMU_BSWAP_NUMFUNCS;
	zfs_refcount_create(&hdr->b_l1hdr.b_refcnt);
#ifdef ZFS_DEBUG
	mutex_init(&hdr->b_l1hdr.b_freeze_lock, NULL, MUTEX_DEFAULT, NULL);
#endif
	multilist_link_init(&hdr->b_l1hdr.b_arc_node);
	list_link_init(&hdr->b_l2hdr.b_l2node);
	arc_space_consume(HDR_FULL_SIZE, ARC_SPACE_HDRS);

	return (0);
}

static int
hdr_l2only_cons(void *vbuf, void *unused, int kmflag)
{
	(void) unused, (void) kmflag;
	arc_buf_hdr_t *hdr = vbuf;

	memset(hdr, 0, HDR_L2ONLY_SIZE);
	arc_space_consume(HDR_L2ONLY_SIZE, ARC_SPACE_L2HDRS);

	return (0);
}

static int
buf_cons(void *vbuf, void *unused, int kmflag)
{
	(void) unused, (void) kmflag;
	arc_buf_t *buf = vbuf;

	memset(buf, 0, sizeof (arc_buf_t));
	arc_space_consume(sizeof (arc_buf_t), ARC_SPACE_HDRS);

	return (0);
}

 
static void
hdr_full_dest(void *vbuf, void *unused)
{
	(void) unused;
	arc_buf_hdr_t *hdr = vbuf;

	ASSERT(HDR_EMPTY(hdr));
	zfs_refcount_destroy(&hdr->b_l1hdr.b_refcnt);
#ifdef ZFS_DEBUG
	mutex_destroy(&hdr->b_l1hdr.b_freeze_lock);
#endif
	ASSERT(!multilist_link_active(&hdr->b_l1hdr.b_arc_node));
	arc_space_return(HDR_FULL_SIZE, ARC_SPACE_HDRS);
}

static void
hdr_l2only_dest(void *vbuf, void *unused)
{
	(void) unused;
	arc_buf_hdr_t *hdr = vbuf;

	ASSERT(HDR_EMPTY(hdr));
	arc_space_return(HDR_L2ONLY_SIZE, ARC_SPACE_L2HDRS);
}

static void
buf_dest(void *vbuf, void *unused)
{
	(void) unused;
	(void) vbuf;

	arc_space_return(sizeof (arc_buf_t), ARC_SPACE_HDRS);
}

static void
buf_init(void)
{
	uint64_t *ct = NULL;
	uint64_t hsize = 1ULL << 12;
	int i, j;

	 
	while (hsize * zfs_arc_average_blocksize < arc_all_memory())
		hsize <<= 1;
retry:
	buf_hash_table.ht_mask = hsize - 1;
#if defined(_KERNEL)
	 
	buf_hash_table.ht_table =
	    vmem_zalloc(hsize * sizeof (void*), KM_SLEEP);
#else
	buf_hash_table.ht_table =
	    kmem_zalloc(hsize * sizeof (void*), KM_NOSLEEP);
#endif
	if (buf_hash_table.ht_table == NULL) {
		ASSERT(hsize > (1ULL << 8));
		hsize >>= 1;
		goto retry;
	}

	hdr_full_cache = kmem_cache_create("arc_buf_hdr_t_full", HDR_FULL_SIZE,
	    0, hdr_full_cons, hdr_full_dest, NULL, NULL, NULL, 0);
	hdr_l2only_cache = kmem_cache_create("arc_buf_hdr_t_l2only",
	    HDR_L2ONLY_SIZE, 0, hdr_l2only_cons, hdr_l2only_dest, NULL,
	    NULL, NULL, 0);
	buf_cache = kmem_cache_create("arc_buf_t", sizeof (arc_buf_t),
	    0, buf_cons, buf_dest, NULL, NULL, NULL, 0);

	for (i = 0; i < 256; i++)
		for (ct = zfs_crc64_table + i, *ct = i, j = 8; j > 0; j--)
			*ct = (*ct >> 1) ^ (-(*ct & 1) & ZFS_CRC64_POLY);

	for (i = 0; i < BUF_LOCKS; i++)
		mutex_init(BUF_HASH_LOCK(i), NULL, MUTEX_DEFAULT, NULL);
}

#define	ARC_MINTIME	(hz>>4)  

 
uint64_t
arc_buf_size(arc_buf_t *buf)
{
	return (ARC_BUF_COMPRESSED(buf) ?
	    HDR_GET_PSIZE(buf->b_hdr) : HDR_GET_LSIZE(buf->b_hdr));
}

uint64_t
arc_buf_lsize(arc_buf_t *buf)
{
	return (HDR_GET_LSIZE(buf->b_hdr));
}

 
boolean_t
arc_is_encrypted(arc_buf_t *buf)
{
	return (ARC_BUF_ENCRYPTED(buf) != 0);
}

 
boolean_t
arc_is_unauthenticated(arc_buf_t *buf)
{
	return (HDR_NOAUTH(buf->b_hdr) != 0);
}

void
arc_get_raw_params(arc_buf_t *buf, boolean_t *byteorder, uint8_t *salt,
    uint8_t *iv, uint8_t *mac)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT(HDR_PROTECTED(hdr));

	memcpy(salt, hdr->b_crypt_hdr.b_salt, ZIO_DATA_SALT_LEN);
	memcpy(iv, hdr->b_crypt_hdr.b_iv, ZIO_DATA_IV_LEN);
	memcpy(mac, hdr->b_crypt_hdr.b_mac, ZIO_DATA_MAC_LEN);
	*byteorder = (hdr->b_l1hdr.b_byteswap == DMU_BSWAP_NUMFUNCS) ?
	    ZFS_HOST_BYTEORDER : !ZFS_HOST_BYTEORDER;
}

 
enum zio_compress
arc_get_compression(arc_buf_t *buf)
{
	return (ARC_BUF_COMPRESSED(buf) ?
	    HDR_GET_COMPRESS(buf->b_hdr) : ZIO_COMPRESS_OFF);
}

 
static inline enum zio_compress
arc_hdr_get_compress(arc_buf_hdr_t *hdr)
{
	return (HDR_COMPRESSION_ENABLED(hdr) ?
	    HDR_GET_COMPRESS(hdr) : ZIO_COMPRESS_OFF);
}

uint8_t
arc_get_complevel(arc_buf_t *buf)
{
	return (buf->b_hdr->b_complevel);
}

static inline boolean_t
arc_buf_is_shared(arc_buf_t *buf)
{
	boolean_t shared = (buf->b_data != NULL &&
	    buf->b_hdr->b_l1hdr.b_pabd != NULL &&
	    abd_is_linear(buf->b_hdr->b_l1hdr.b_pabd) &&
	    buf->b_data == abd_to_buf(buf->b_hdr->b_l1hdr.b_pabd));
	IMPLY(shared, HDR_SHARED_DATA(buf->b_hdr));
	EQUIV(shared, ARC_BUF_SHARED(buf));
	IMPLY(shared, ARC_BUF_COMPRESSED(buf) || ARC_BUF_LAST(buf));

	 

	return (shared);
}

 
static inline void
arc_cksum_free(arc_buf_hdr_t *hdr)
{
#ifdef ZFS_DEBUG
	ASSERT(HDR_HAS_L1HDR(hdr));

	mutex_enter(&hdr->b_l1hdr.b_freeze_lock);
	if (hdr->b_l1hdr.b_freeze_cksum != NULL) {
		kmem_free(hdr->b_l1hdr.b_freeze_cksum, sizeof (zio_cksum_t));
		hdr->b_l1hdr.b_freeze_cksum = NULL;
	}
	mutex_exit(&hdr->b_l1hdr.b_freeze_lock);
#endif
}

 
static boolean_t
arc_hdr_has_uncompressed_buf(arc_buf_hdr_t *hdr)
{
	ASSERT(hdr->b_l1hdr.b_state == arc_anon || HDR_EMPTY_OR_LOCKED(hdr));

	for (arc_buf_t *b = hdr->b_l1hdr.b_buf; b != NULL; b = b->b_next) {
		if (!ARC_BUF_COMPRESSED(b)) {
			return (B_TRUE);
		}
	}
	return (B_FALSE);
}


 
static void
arc_cksum_verify(arc_buf_t *buf)
{
#ifdef ZFS_DEBUG
	arc_buf_hdr_t *hdr = buf->b_hdr;
	zio_cksum_t zc;

	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

	if (ARC_BUF_COMPRESSED(buf))
		return;

	ASSERT(HDR_HAS_L1HDR(hdr));

	mutex_enter(&hdr->b_l1hdr.b_freeze_lock);

	if (hdr->b_l1hdr.b_freeze_cksum == NULL || HDR_IO_ERROR(hdr)) {
		mutex_exit(&hdr->b_l1hdr.b_freeze_lock);
		return;
	}

	fletcher_2_native(buf->b_data, arc_buf_size(buf), NULL, &zc);
	if (!ZIO_CHECKSUM_EQUAL(*hdr->b_l1hdr.b_freeze_cksum, zc))
		panic("buffer modified while frozen!");
	mutex_exit(&hdr->b_l1hdr.b_freeze_lock);
#endif
}

 
static boolean_t
arc_cksum_is_equal(arc_buf_hdr_t *hdr, zio_t *zio)
{
	ASSERT(!BP_IS_EMBEDDED(zio->io_bp));
	VERIFY3U(BP_GET_PSIZE(zio->io_bp), ==, HDR_GET_PSIZE(hdr));

	 
	return (zio_checksum_error_impl(zio->io_spa, zio->io_bp,
	    BP_GET_CHECKSUM(zio->io_bp), zio->io_abd, zio->io_size,
	    zio->io_offset, NULL) == 0);
}

 
static void
arc_cksum_compute(arc_buf_t *buf)
{
	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

#ifdef ZFS_DEBUG
	arc_buf_hdr_t *hdr = buf->b_hdr;
	ASSERT(HDR_HAS_L1HDR(hdr));
	mutex_enter(&hdr->b_l1hdr.b_freeze_lock);
	if (hdr->b_l1hdr.b_freeze_cksum != NULL || ARC_BUF_COMPRESSED(buf)) {
		mutex_exit(&hdr->b_l1hdr.b_freeze_lock);
		return;
	}

	ASSERT(!ARC_BUF_ENCRYPTED(buf));
	ASSERT(!ARC_BUF_COMPRESSED(buf));
	hdr->b_l1hdr.b_freeze_cksum = kmem_alloc(sizeof (zio_cksum_t),
	    KM_SLEEP);
	fletcher_2_native(buf->b_data, arc_buf_size(buf), NULL,
	    hdr->b_l1hdr.b_freeze_cksum);
	mutex_exit(&hdr->b_l1hdr.b_freeze_lock);
#endif
	arc_buf_watch(buf);
}

#ifndef _KERNEL
void
arc_buf_sigsegv(int sig, siginfo_t *si, void *unused)
{
	(void) sig, (void) unused;
	panic("Got SIGSEGV at address: 0x%lx\n", (long)si->si_addr);
}
#endif

static void
arc_buf_unwatch(arc_buf_t *buf)
{
#ifndef _KERNEL
	if (arc_watch) {
		ASSERT0(mprotect(buf->b_data, arc_buf_size(buf),
		    PROT_READ | PROT_WRITE));
	}
#else
	(void) buf;
#endif
}

static void
arc_buf_watch(arc_buf_t *buf)
{
#ifndef _KERNEL
	if (arc_watch)
		ASSERT0(mprotect(buf->b_data, arc_buf_size(buf),
		    PROT_READ));
#else
	(void) buf;
#endif
}

static arc_buf_contents_t
arc_buf_type(arc_buf_hdr_t *hdr)
{
	arc_buf_contents_t type;
	if (HDR_ISTYPE_METADATA(hdr)) {
		type = ARC_BUFC_METADATA;
	} else {
		type = ARC_BUFC_DATA;
	}
	VERIFY3U(hdr->b_type, ==, type);
	return (type);
}

boolean_t
arc_is_metadata(arc_buf_t *buf)
{
	return (HDR_ISTYPE_METADATA(buf->b_hdr) != 0);
}

static uint32_t
arc_bufc_to_flags(arc_buf_contents_t type)
{
	switch (type) {
	case ARC_BUFC_DATA:
		 
		return (0);
	case ARC_BUFC_METADATA:
		return (ARC_FLAG_BUFC_METADATA);
	default:
		break;
	}
	panic("undefined ARC buffer type!");
	return ((uint32_t)-1);
}

void
arc_buf_thaw(arc_buf_t *buf)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT3P(hdr->b_l1hdr.b_state, ==, arc_anon);
	ASSERT(!HDR_IO_IN_PROGRESS(hdr));

	arc_cksum_verify(buf);

	 
	if (ARC_BUF_COMPRESSED(buf))
		return;

	ASSERT(HDR_HAS_L1HDR(hdr));
	arc_cksum_free(hdr);
	arc_buf_unwatch(buf);
}

void
arc_buf_freeze(arc_buf_t *buf)
{
	if (!(zfs_flags & ZFS_DEBUG_MODIFY))
		return;

	if (ARC_BUF_COMPRESSED(buf))
		return;

	ASSERT(HDR_HAS_L1HDR(buf->b_hdr));
	arc_cksum_compute(buf);
}

 
static inline void
arc_hdr_set_flags(arc_buf_hdr_t *hdr, arc_flags_t flags)
{
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));
	hdr->b_flags |= flags;
}

static inline void
arc_hdr_clear_flags(arc_buf_hdr_t *hdr, arc_flags_t flags)
{
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));
	hdr->b_flags &= ~flags;
}

 
static void
arc_hdr_set_compress(arc_buf_hdr_t *hdr, enum zio_compress cmp)
{
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));

	 
	if (!zfs_compressed_arc_enabled || HDR_GET_PSIZE(hdr) == 0) {
		arc_hdr_clear_flags(hdr, ARC_FLAG_COMPRESSED_ARC);
		ASSERT(!HDR_COMPRESSION_ENABLED(hdr));
	} else {
		arc_hdr_set_flags(hdr, ARC_FLAG_COMPRESSED_ARC);
		ASSERT(HDR_COMPRESSION_ENABLED(hdr));
	}

	HDR_SET_COMPRESS(hdr, cmp);
	ASSERT3U(HDR_GET_COMPRESS(hdr), ==, cmp);
}

 
static boolean_t
arc_buf_try_copy_decompressed_data(arc_buf_t *buf)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;
	boolean_t copied = B_FALSE;

	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT3P(buf->b_data, !=, NULL);
	ASSERT(!ARC_BUF_COMPRESSED(buf));

	for (arc_buf_t *from = hdr->b_l1hdr.b_buf; from != NULL;
	    from = from->b_next) {
		 
		if (from == buf) {
			continue;
		}

		if (!ARC_BUF_COMPRESSED(from)) {
			memcpy(buf->b_data, from->b_data, arc_buf_size(buf));
			copied = B_TRUE;
			break;
		}
	}

#ifdef ZFS_DEBUG
	 
	if (zfs_flags & ZFS_DEBUG_MODIFY)
		EQUIV(!copied, hdr->b_l1hdr.b_freeze_cksum == NULL);
#endif

	return (copied);
}

 
static arc_buf_hdr_t *
arc_buf_alloc_l2only(size_t size, arc_buf_contents_t type, l2arc_dev_t *dev,
    dva_t dva, uint64_t daddr, int32_t psize, uint64_t birth,
    enum zio_compress compress, uint8_t complevel, boolean_t protected,
    boolean_t prefetch, arc_state_type_t arcs_state)
{
	arc_buf_hdr_t	*hdr;

	ASSERT(size != 0);
	hdr = kmem_cache_alloc(hdr_l2only_cache, KM_SLEEP);
	hdr->b_birth = birth;
	hdr->b_type = type;
	hdr->b_flags = 0;
	arc_hdr_set_flags(hdr, arc_bufc_to_flags(type) | ARC_FLAG_HAS_L2HDR);
	HDR_SET_LSIZE(hdr, size);
	HDR_SET_PSIZE(hdr, psize);
	arc_hdr_set_compress(hdr, compress);
	hdr->b_complevel = complevel;
	if (protected)
		arc_hdr_set_flags(hdr, ARC_FLAG_PROTECTED);
	if (prefetch)
		arc_hdr_set_flags(hdr, ARC_FLAG_PREFETCH);
	hdr->b_spa = spa_load_guid(dev->l2ad_vdev->vdev_spa);

	hdr->b_dva = dva;

	hdr->b_l2hdr.b_dev = dev;
	hdr->b_l2hdr.b_daddr = daddr;
	hdr->b_l2hdr.b_arcs_state = arcs_state;

	return (hdr);
}

 
static uint64_t
arc_hdr_size(arc_buf_hdr_t *hdr)
{
	uint64_t size;

	if (arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF &&
	    HDR_GET_PSIZE(hdr) > 0) {
		size = HDR_GET_PSIZE(hdr);
	} else {
		ASSERT3U(HDR_GET_LSIZE(hdr), !=, 0);
		size = HDR_GET_LSIZE(hdr);
	}
	return (size);
}

static int
arc_hdr_authenticate(arc_buf_hdr_t *hdr, spa_t *spa, uint64_t dsobj)
{
	int ret;
	uint64_t csize;
	uint64_t lsize = HDR_GET_LSIZE(hdr);
	uint64_t psize = HDR_GET_PSIZE(hdr);
	void *tmpbuf = NULL;
	abd_t *abd = hdr->b_l1hdr.b_pabd;

	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));
	ASSERT(HDR_AUTHENTICATED(hdr));
	ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);

	 
	if (HDR_GET_COMPRESS(hdr) != ZIO_COMPRESS_OFF &&
	    !HDR_COMPRESSION_ENABLED(hdr)) {

		csize = zio_compress_data(HDR_GET_COMPRESS(hdr),
		    hdr->b_l1hdr.b_pabd, &tmpbuf, lsize, hdr->b_complevel);
		ASSERT3P(tmpbuf, !=, NULL);
		ASSERT3U(csize, <=, psize);
		abd = abd_get_from_buf(tmpbuf, lsize);
		abd_take_ownership_of_buf(abd, B_TRUE);
		abd_zero_off(abd, csize, psize - csize);
	}

	 
	if (hdr->b_crypt_hdr.b_ot == DMU_OT_OBJSET) {
		ASSERT3U(HDR_GET_COMPRESS(hdr), ==, ZIO_COMPRESS_OFF);
		ASSERT3U(lsize, ==, psize);
		ret = spa_do_crypt_objset_mac_abd(B_FALSE, spa, dsobj, abd,
		    psize, hdr->b_l1hdr.b_byteswap != DMU_BSWAP_NUMFUNCS);
	} else {
		ret = spa_do_crypt_mac_abd(B_FALSE, spa, dsobj, abd, psize,
		    hdr->b_crypt_hdr.b_mac);
	}

	if (ret == 0)
		arc_hdr_clear_flags(hdr, ARC_FLAG_NOAUTH);
	else if (ret != ENOENT)
		goto error;

	if (tmpbuf != NULL)
		abd_free(abd);

	return (0);

error:
	if (tmpbuf != NULL)
		abd_free(abd);

	return (ret);
}

 
static int
arc_hdr_decrypt(arc_buf_hdr_t *hdr, spa_t *spa, const zbookmark_phys_t *zb)
{
	int ret;
	abd_t *cabd = NULL;
	void *tmp = NULL;
	boolean_t no_crypt = B_FALSE;
	boolean_t bswap = (hdr->b_l1hdr.b_byteswap != DMU_BSWAP_NUMFUNCS);

	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));
	ASSERT(HDR_ENCRYPTED(hdr));

	arc_hdr_alloc_abd(hdr, 0);

	ret = spa_do_crypt_abd(B_FALSE, spa, zb, hdr->b_crypt_hdr.b_ot,
	    B_FALSE, bswap, hdr->b_crypt_hdr.b_salt, hdr->b_crypt_hdr.b_iv,
	    hdr->b_crypt_hdr.b_mac, HDR_GET_PSIZE(hdr), hdr->b_l1hdr.b_pabd,
	    hdr->b_crypt_hdr.b_rabd, &no_crypt);
	if (ret != 0)
		goto error;

	if (no_crypt) {
		abd_copy(hdr->b_l1hdr.b_pabd, hdr->b_crypt_hdr.b_rabd,
		    HDR_GET_PSIZE(hdr));
	}

	 
	if (HDR_GET_COMPRESS(hdr) != ZIO_COMPRESS_OFF &&
	    !HDR_COMPRESSION_ENABLED(hdr)) {
		 
		cabd = arc_get_data_abd(hdr, arc_hdr_size(hdr), hdr, 0);
		tmp = abd_borrow_buf(cabd, arc_hdr_size(hdr));

		ret = zio_decompress_data(HDR_GET_COMPRESS(hdr),
		    hdr->b_l1hdr.b_pabd, tmp, HDR_GET_PSIZE(hdr),
		    HDR_GET_LSIZE(hdr), &hdr->b_complevel);
		if (ret != 0) {
			abd_return_buf(cabd, tmp, arc_hdr_size(hdr));
			goto error;
		}

		abd_return_buf_copy(cabd, tmp, arc_hdr_size(hdr));
		arc_free_data_abd(hdr, hdr->b_l1hdr.b_pabd,
		    arc_hdr_size(hdr), hdr);
		hdr->b_l1hdr.b_pabd = cabd;
	}

	return (0);

error:
	arc_hdr_free_abd(hdr, B_FALSE);
	if (cabd != NULL)
		arc_free_data_buf(hdr, cabd, arc_hdr_size(hdr), hdr);

	return (ret);
}

 
static int
arc_fill_hdr_crypt(arc_buf_hdr_t *hdr, kmutex_t *hash_lock, spa_t *spa,
    const zbookmark_phys_t *zb, boolean_t noauth)
{
	int ret;

	ASSERT(HDR_PROTECTED(hdr));

	if (hash_lock != NULL)
		mutex_enter(hash_lock);

	if (HDR_NOAUTH(hdr) && !noauth) {
		 
		ret = arc_hdr_authenticate(hdr, spa, zb->zb_objset);
		if (ret != 0)
			goto error;
	} else if (HDR_HAS_RABD(hdr) && hdr->b_l1hdr.b_pabd == NULL) {
		 
		ret = arc_hdr_decrypt(hdr, spa, zb);
		if (ret != 0)
			goto error;
	}

	ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);

	if (hash_lock != NULL)
		mutex_exit(hash_lock);

	return (0);

error:
	if (hash_lock != NULL)
		mutex_exit(hash_lock);

	return (ret);
}

 
static void
arc_buf_untransform_in_place(arc_buf_t *buf)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT(HDR_ENCRYPTED(hdr));
	ASSERT3U(hdr->b_crypt_hdr.b_ot, ==, DMU_OT_DNODE);
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));
	ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);

	zio_crypt_copy_dnode_bonus(hdr->b_l1hdr.b_pabd, buf->b_data,
	    arc_buf_size(buf));
	buf->b_flags &= ~ARC_BUF_FLAG_ENCRYPTED;
	buf->b_flags &= ~ARC_BUF_FLAG_COMPRESSED;
}

 
static int
arc_buf_fill(arc_buf_t *buf, spa_t *spa, const zbookmark_phys_t *zb,
    arc_fill_flags_t flags)
{
	int error = 0;
	arc_buf_hdr_t *hdr = buf->b_hdr;
	boolean_t hdr_compressed =
	    (arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF);
	boolean_t compressed = (flags & ARC_FILL_COMPRESSED) != 0;
	boolean_t encrypted = (flags & ARC_FILL_ENCRYPTED) != 0;
	dmu_object_byteswap_t bswap = hdr->b_l1hdr.b_byteswap;
	kmutex_t *hash_lock = (flags & ARC_FILL_LOCKED) ? NULL : HDR_LOCK(hdr);

	ASSERT3P(buf->b_data, !=, NULL);
	IMPLY(compressed, hdr_compressed || ARC_BUF_ENCRYPTED(buf));
	IMPLY(compressed, ARC_BUF_COMPRESSED(buf));
	IMPLY(encrypted, HDR_ENCRYPTED(hdr));
	IMPLY(encrypted, ARC_BUF_ENCRYPTED(buf));
	IMPLY(encrypted, ARC_BUF_COMPRESSED(buf));
	IMPLY(encrypted, !arc_buf_is_shared(buf));

	 
	if (encrypted) {
		ASSERT(HDR_HAS_RABD(hdr));
		abd_copy_to_buf(buf->b_data, hdr->b_crypt_hdr.b_rabd,
		    HDR_GET_PSIZE(hdr));
		goto byteswap;
	}

	 
	if (HDR_PROTECTED(hdr)) {
		error = arc_fill_hdr_crypt(hdr, hash_lock, spa,
		    zb, !!(flags & ARC_FILL_NOAUTH));
		if (error == EACCES && (flags & ARC_FILL_IN_PLACE) != 0) {
			return (error);
		} else if (error != 0) {
			if (hash_lock != NULL)
				mutex_enter(hash_lock);
			arc_hdr_set_flags(hdr, ARC_FLAG_IO_ERROR);
			if (hash_lock != NULL)
				mutex_exit(hash_lock);
			return (error);
		}
	}

	 
	if ((flags & ARC_FILL_IN_PLACE) != 0) {
		ASSERT(!hdr_compressed);
		ASSERT(!compressed);
		ASSERT(!encrypted);

		if (HDR_ENCRYPTED(hdr) && ARC_BUF_ENCRYPTED(buf)) {
			ASSERT3U(hdr->b_crypt_hdr.b_ot, ==, DMU_OT_DNODE);

			if (hash_lock != NULL)
				mutex_enter(hash_lock);
			arc_buf_untransform_in_place(buf);
			if (hash_lock != NULL)
				mutex_exit(hash_lock);

			 
			arc_cksum_compute(buf);
		}

		return (0);
	}

	if (hdr_compressed == compressed) {
		if (ARC_BUF_SHARED(buf)) {
			ASSERT(arc_buf_is_shared(buf));
		} else {
			abd_copy_to_buf(buf->b_data, hdr->b_l1hdr.b_pabd,
			    arc_buf_size(buf));
		}
	} else {
		ASSERT(hdr_compressed);
		ASSERT(!compressed);

		 
		if (ARC_BUF_SHARED(buf)) {
			ASSERT(ARC_BUF_COMPRESSED(buf));

			 
			buf->b_flags &= ~ARC_BUF_FLAG_SHARED;
			buf->b_data =
			    arc_get_data_buf(hdr, HDR_GET_LSIZE(hdr), buf);
			arc_hdr_clear_flags(hdr, ARC_FLAG_SHARED_DATA);

			 
			ARCSTAT_INCR(arcstat_overhead_size, HDR_GET_LSIZE(hdr));
		} else if (ARC_BUF_COMPRESSED(buf)) {
			ASSERT(!arc_buf_is_shared(buf));

			 
			arc_free_data_buf(hdr, buf->b_data, HDR_GET_PSIZE(hdr),
			    buf);
			buf->b_data =
			    arc_get_data_buf(hdr, HDR_GET_LSIZE(hdr), buf);

			 
			ARCSTAT_INCR(arcstat_overhead_size,
			    HDR_GET_LSIZE(hdr) - HDR_GET_PSIZE(hdr));
		}

		 
		buf->b_flags &= ~ARC_BUF_FLAG_COMPRESSED;

		 
		if (arc_buf_try_copy_decompressed_data(buf)) {
			 
			return (0);
		} else {
			error = zio_decompress_data(HDR_GET_COMPRESS(hdr),
			    hdr->b_l1hdr.b_pabd, buf->b_data,
			    HDR_GET_PSIZE(hdr), HDR_GET_LSIZE(hdr),
			    &hdr->b_complevel);

			 
			if (error != 0) {
				zfs_dbgmsg(
				    "hdr %px, compress %d, psize %d, lsize %d",
				    hdr, arc_hdr_get_compress(hdr),
				    HDR_GET_PSIZE(hdr), HDR_GET_LSIZE(hdr));
				if (hash_lock != NULL)
					mutex_enter(hash_lock);
				arc_hdr_set_flags(hdr, ARC_FLAG_IO_ERROR);
				if (hash_lock != NULL)
					mutex_exit(hash_lock);
				return (SET_ERROR(EIO));
			}
		}
	}

byteswap:
	 
	if (bswap != DMU_BSWAP_NUMFUNCS) {
		ASSERT(!HDR_SHARED_DATA(hdr));
		ASSERT3U(bswap, <, DMU_BSWAP_NUMFUNCS);
		dmu_ot_byteswap[bswap].ob_func(buf->b_data, HDR_GET_LSIZE(hdr));
	}

	 
	arc_cksum_compute(buf);

	return (0);
}

 
int
arc_untransform(arc_buf_t *buf, spa_t *spa, const zbookmark_phys_t *zb,
    boolean_t in_place)
{
	int ret;
	arc_fill_flags_t flags = 0;

	if (in_place)
		flags |= ARC_FILL_IN_PLACE;

	ret = arc_buf_fill(buf, spa, zb, flags);
	if (ret == ECKSUM) {
		 
		ret = SET_ERROR(EIO);
		spa_log_error(spa, zb, &buf->b_hdr->b_birth);
		(void) zfs_ereport_post(FM_EREPORT_ZFS_AUTHENTICATION,
		    spa, NULL, zb, NULL, 0);
	}

	return (ret);
}

 
static void
arc_evictable_space_increment(arc_buf_hdr_t *hdr, arc_state_t *state)
{
	arc_buf_contents_t type = arc_buf_type(hdr);

	ASSERT(HDR_HAS_L1HDR(hdr));

	if (GHOST_STATE(state)) {
		ASSERT3P(hdr->b_l1hdr.b_buf, ==, NULL);
		ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
		ASSERT(!HDR_HAS_RABD(hdr));
		(void) zfs_refcount_add_many(&state->arcs_esize[type],
		    HDR_GET_LSIZE(hdr), hdr);
		return;
	}

	if (hdr->b_l1hdr.b_pabd != NULL) {
		(void) zfs_refcount_add_many(&state->arcs_esize[type],
		    arc_hdr_size(hdr), hdr);
	}
	if (HDR_HAS_RABD(hdr)) {
		(void) zfs_refcount_add_many(&state->arcs_esize[type],
		    HDR_GET_PSIZE(hdr), hdr);
	}

	for (arc_buf_t *buf = hdr->b_l1hdr.b_buf; buf != NULL;
	    buf = buf->b_next) {
		if (ARC_BUF_SHARED(buf))
			continue;
		(void) zfs_refcount_add_many(&state->arcs_esize[type],
		    arc_buf_size(buf), buf);
	}
}

 
static void
arc_evictable_space_decrement(arc_buf_hdr_t *hdr, arc_state_t *state)
{
	arc_buf_contents_t type = arc_buf_type(hdr);

	ASSERT(HDR_HAS_L1HDR(hdr));

	if (GHOST_STATE(state)) {
		ASSERT3P(hdr->b_l1hdr.b_buf, ==, NULL);
		ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
		ASSERT(!HDR_HAS_RABD(hdr));
		(void) zfs_refcount_remove_many(&state->arcs_esize[type],
		    HDR_GET_LSIZE(hdr), hdr);
		return;
	}

	if (hdr->b_l1hdr.b_pabd != NULL) {
		(void) zfs_refcount_remove_many(&state->arcs_esize[type],
		    arc_hdr_size(hdr), hdr);
	}
	if (HDR_HAS_RABD(hdr)) {
		(void) zfs_refcount_remove_many(&state->arcs_esize[type],
		    HDR_GET_PSIZE(hdr), hdr);
	}

	for (arc_buf_t *buf = hdr->b_l1hdr.b_buf; buf != NULL;
	    buf = buf->b_next) {
		if (ARC_BUF_SHARED(buf))
			continue;
		(void) zfs_refcount_remove_many(&state->arcs_esize[type],
		    arc_buf_size(buf), buf);
	}
}

 
static void
add_reference(arc_buf_hdr_t *hdr, const void *tag)
{
	arc_state_t *state = hdr->b_l1hdr.b_state;

	ASSERT(HDR_HAS_L1HDR(hdr));
	if (!HDR_EMPTY(hdr) && !MUTEX_HELD(HDR_LOCK(hdr))) {
		ASSERT(state == arc_anon);
		ASSERT(zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt));
		ASSERT3P(hdr->b_l1hdr.b_buf, ==, NULL);
	}

	if ((zfs_refcount_add(&hdr->b_l1hdr.b_refcnt, tag) == 1) &&
	    state != arc_anon && state != arc_l2c_only) {
		 
		multilist_remove(&state->arcs_list[arc_buf_type(hdr)], hdr);
		arc_evictable_space_decrement(hdr, state);
	}
}

 
static int
remove_reference(arc_buf_hdr_t *hdr, const void *tag)
{
	int cnt;
	arc_state_t *state = hdr->b_l1hdr.b_state;

	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT(state == arc_anon || MUTEX_HELD(HDR_LOCK(hdr)));
	ASSERT(!GHOST_STATE(state));	 

	if ((cnt = zfs_refcount_remove(&hdr->b_l1hdr.b_refcnt, tag)) != 0)
		return (cnt);

	if (state == arc_anon) {
		arc_hdr_destroy(hdr);
		return (0);
	}
	if (state == arc_uncached && !HDR_PREFETCH(hdr)) {
		arc_change_state(arc_anon, hdr);
		arc_hdr_destroy(hdr);
		return (0);
	}
	multilist_insert(&state->arcs_list[arc_buf_type(hdr)], hdr);
	arc_evictable_space_increment(hdr, state);
	return (0);
}

 
void
arc_buf_info(arc_buf_t *ab, arc_buf_info_t *abi, int state_index)
{
	(void) state_index;
	arc_buf_hdr_t *hdr = ab->b_hdr;
	l1arc_buf_hdr_t *l1hdr = NULL;
	l2arc_buf_hdr_t *l2hdr = NULL;
	arc_state_t *state = NULL;

	memset(abi, 0, sizeof (arc_buf_info_t));

	if (hdr == NULL)
		return;

	abi->abi_flags = hdr->b_flags;

	if (HDR_HAS_L1HDR(hdr)) {
		l1hdr = &hdr->b_l1hdr;
		state = l1hdr->b_state;
	}
	if (HDR_HAS_L2HDR(hdr))
		l2hdr = &hdr->b_l2hdr;

	if (l1hdr) {
		abi->abi_bufcnt = 0;
		for (arc_buf_t *buf = l1hdr->b_buf; buf; buf = buf->b_next)
			abi->abi_bufcnt++;
		abi->abi_access = l1hdr->b_arc_access;
		abi->abi_mru_hits = l1hdr->b_mru_hits;
		abi->abi_mru_ghost_hits = l1hdr->b_mru_ghost_hits;
		abi->abi_mfu_hits = l1hdr->b_mfu_hits;
		abi->abi_mfu_ghost_hits = l1hdr->b_mfu_ghost_hits;
		abi->abi_holds = zfs_refcount_count(&l1hdr->b_refcnt);
	}

	if (l2hdr) {
		abi->abi_l2arc_dattr = l2hdr->b_daddr;
		abi->abi_l2arc_hits = l2hdr->b_hits;
	}

	abi->abi_state_type = state ? state->arcs_state : ARC_STATE_ANON;
	abi->abi_state_contents = arc_buf_type(hdr);
	abi->abi_size = arc_hdr_size(hdr);
}

 
static void
arc_change_state(arc_state_t *new_state, arc_buf_hdr_t *hdr)
{
	arc_state_t *old_state;
	int64_t refcnt;
	boolean_t update_old, update_new;
	arc_buf_contents_t type = arc_buf_type(hdr);

	 
	if (HDR_HAS_L1HDR(hdr)) {
		old_state = hdr->b_l1hdr.b_state;
		refcnt = zfs_refcount_count(&hdr->b_l1hdr.b_refcnt);
		update_old = (hdr->b_l1hdr.b_buf != NULL ||
		    hdr->b_l1hdr.b_pabd != NULL || HDR_HAS_RABD(hdr));

		IMPLY(GHOST_STATE(old_state), hdr->b_l1hdr.b_buf == NULL);
		IMPLY(GHOST_STATE(new_state), hdr->b_l1hdr.b_buf == NULL);
		IMPLY(old_state == arc_anon, hdr->b_l1hdr.b_buf == NULL ||
		    ARC_BUF_LAST(hdr->b_l1hdr.b_buf));
	} else {
		old_state = arc_l2c_only;
		refcnt = 0;
		update_old = B_FALSE;
	}
	update_new = update_old;
	if (GHOST_STATE(old_state))
		update_old = B_TRUE;
	if (GHOST_STATE(new_state))
		update_new = B_TRUE;

	ASSERT(MUTEX_HELD(HDR_LOCK(hdr)));
	ASSERT3P(new_state, !=, old_state);

	 
	if (refcnt == 0) {
		if (old_state != arc_anon && old_state != arc_l2c_only) {
			ASSERT(HDR_HAS_L1HDR(hdr));
			 
			if (multilist_link_active(&hdr->b_l1hdr.b_arc_node)) {
				multilist_remove(&old_state->arcs_list[type],
				    hdr);
				arc_evictable_space_decrement(hdr, old_state);
			}
		}
		if (new_state != arc_anon && new_state != arc_l2c_only) {
			 
			ASSERT(HDR_HAS_L1HDR(hdr));
			multilist_insert(&new_state->arcs_list[type], hdr);
			arc_evictable_space_increment(hdr, new_state);
		}
	}

	ASSERT(!HDR_EMPTY(hdr));
	if (new_state == arc_anon && HDR_IN_HASH_TABLE(hdr))
		buf_hash_remove(hdr);

	 

	if (update_new && new_state != arc_l2c_only) {
		ASSERT(HDR_HAS_L1HDR(hdr));
		if (GHOST_STATE(new_state)) {

			 
			(void) zfs_refcount_add_many(
			    &new_state->arcs_size[type],
			    HDR_GET_LSIZE(hdr), hdr);
			ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
			ASSERT(!HDR_HAS_RABD(hdr));
		} else {

			 
			for (arc_buf_t *buf = hdr->b_l1hdr.b_buf; buf != NULL;
			    buf = buf->b_next) {

				 
				if (ARC_BUF_SHARED(buf))
					continue;

				(void) zfs_refcount_add_many(
				    &new_state->arcs_size[type],
				    arc_buf_size(buf), buf);
			}

			if (hdr->b_l1hdr.b_pabd != NULL) {
				(void) zfs_refcount_add_many(
				    &new_state->arcs_size[type],
				    arc_hdr_size(hdr), hdr);
			}

			if (HDR_HAS_RABD(hdr)) {
				(void) zfs_refcount_add_many(
				    &new_state->arcs_size[type],
				    HDR_GET_PSIZE(hdr), hdr);
			}
		}
	}

	if (update_old && old_state != arc_l2c_only) {
		ASSERT(HDR_HAS_L1HDR(hdr));
		if (GHOST_STATE(old_state)) {
			ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
			ASSERT(!HDR_HAS_RABD(hdr));

			 

			(void) zfs_refcount_remove_many(
			    &old_state->arcs_size[type],
			    HDR_GET_LSIZE(hdr), hdr);
		} else {

			 
			for (arc_buf_t *buf = hdr->b_l1hdr.b_buf; buf != NULL;
			    buf = buf->b_next) {

				 
				if (ARC_BUF_SHARED(buf))
					continue;

				(void) zfs_refcount_remove_many(
				    &old_state->arcs_size[type],
				    arc_buf_size(buf), buf);
			}
			ASSERT(hdr->b_l1hdr.b_pabd != NULL ||
			    HDR_HAS_RABD(hdr));

			if (hdr->b_l1hdr.b_pabd != NULL) {
				(void) zfs_refcount_remove_many(
				    &old_state->arcs_size[type],
				    arc_hdr_size(hdr), hdr);
			}

			if (HDR_HAS_RABD(hdr)) {
				(void) zfs_refcount_remove_many(
				    &old_state->arcs_size[type],
				    HDR_GET_PSIZE(hdr), hdr);
			}
		}
	}

	if (HDR_HAS_L1HDR(hdr)) {
		hdr->b_l1hdr.b_state = new_state;

		if (HDR_HAS_L2HDR(hdr) && new_state != arc_l2c_only) {
			l2arc_hdr_arcstats_decrement_state(hdr);
			hdr->b_l2hdr.b_arcs_state = new_state->arcs_state;
			l2arc_hdr_arcstats_increment_state(hdr);
		}
	}
}

void
arc_space_consume(uint64_t space, arc_space_type_t type)
{
	ASSERT(type >= 0 && type < ARC_SPACE_NUMTYPES);

	switch (type) {
	default:
		break;
	case ARC_SPACE_DATA:
		ARCSTAT_INCR(arcstat_data_size, space);
		break;
	case ARC_SPACE_META:
		ARCSTAT_INCR(arcstat_metadata_size, space);
		break;
	case ARC_SPACE_BONUS:
		ARCSTAT_INCR(arcstat_bonus_size, space);
		break;
	case ARC_SPACE_DNODE:
		ARCSTAT_INCR(arcstat_dnode_size, space);
		break;
	case ARC_SPACE_DBUF:
		ARCSTAT_INCR(arcstat_dbuf_size, space);
		break;
	case ARC_SPACE_HDRS:
		ARCSTAT_INCR(arcstat_hdr_size, space);
		break;
	case ARC_SPACE_L2HDRS:
		aggsum_add(&arc_sums.arcstat_l2_hdr_size, space);
		break;
	case ARC_SPACE_ABD_CHUNK_WASTE:
		 
		ARCSTAT_INCR(arcstat_abd_chunk_waste_size, space);
		break;
	}

	if (type != ARC_SPACE_DATA && type != ARC_SPACE_ABD_CHUNK_WASTE)
		ARCSTAT_INCR(arcstat_meta_used, space);

	aggsum_add(&arc_sums.arcstat_size, space);
}

void
arc_space_return(uint64_t space, arc_space_type_t type)
{
	ASSERT(type >= 0 && type < ARC_SPACE_NUMTYPES);

	switch (type) {
	default:
		break;
	case ARC_SPACE_DATA:
		ARCSTAT_INCR(arcstat_data_size, -space);
		break;
	case ARC_SPACE_META:
		ARCSTAT_INCR(arcstat_metadata_size, -space);
		break;
	case ARC_SPACE_BONUS:
		ARCSTAT_INCR(arcstat_bonus_size, -space);
		break;
	case ARC_SPACE_DNODE:
		ARCSTAT_INCR(arcstat_dnode_size, -space);
		break;
	case ARC_SPACE_DBUF:
		ARCSTAT_INCR(arcstat_dbuf_size, -space);
		break;
	case ARC_SPACE_HDRS:
		ARCSTAT_INCR(arcstat_hdr_size, -space);
		break;
	case ARC_SPACE_L2HDRS:
		aggsum_add(&arc_sums.arcstat_l2_hdr_size, -space);
		break;
	case ARC_SPACE_ABD_CHUNK_WASTE:
		ARCSTAT_INCR(arcstat_abd_chunk_waste_size, -space);
		break;
	}

	if (type != ARC_SPACE_DATA && type != ARC_SPACE_ABD_CHUNK_WASTE)
		ARCSTAT_INCR(arcstat_meta_used, -space);

	ASSERT(aggsum_compare(&arc_sums.arcstat_size, space) >= 0);
	aggsum_add(&arc_sums.arcstat_size, -space);
}

 
static boolean_t
arc_can_share(arc_buf_hdr_t *hdr, arc_buf_t *buf)
{
	 
	ASSERT3P(buf->b_hdr, ==, hdr);
	boolean_t hdr_compressed =
	    arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF;
	boolean_t buf_compressed = ARC_BUF_COMPRESSED(buf) != 0;
	return (!ARC_BUF_ENCRYPTED(buf) &&
	    buf_compressed == hdr_compressed &&
	    hdr->b_l1hdr.b_byteswap == DMU_BSWAP_NUMFUNCS &&
	    !HDR_SHARED_DATA(hdr) &&
	    (ARC_BUF_LAST(buf) || ARC_BUF_COMPRESSED(buf)));
}

 
static int
arc_buf_alloc_impl(arc_buf_hdr_t *hdr, spa_t *spa, const zbookmark_phys_t *zb,
    const void *tag, boolean_t encrypted, boolean_t compressed,
    boolean_t noauth, boolean_t fill, arc_buf_t **ret)
{
	arc_buf_t *buf;
	arc_fill_flags_t flags = ARC_FILL_LOCKED;

	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT3U(HDR_GET_LSIZE(hdr), >, 0);
	VERIFY(hdr->b_type == ARC_BUFC_DATA ||
	    hdr->b_type == ARC_BUFC_METADATA);
	ASSERT3P(ret, !=, NULL);
	ASSERT3P(*ret, ==, NULL);
	IMPLY(encrypted, compressed);

	buf = *ret = kmem_cache_alloc(buf_cache, KM_PUSHPAGE);
	buf->b_hdr = hdr;
	buf->b_data = NULL;
	buf->b_next = hdr->b_l1hdr.b_buf;
	buf->b_flags = 0;

	add_reference(hdr, tag);

	 
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));

	 
	if (encrypted) {
		buf->b_flags |= ARC_BUF_FLAG_COMPRESSED;
		buf->b_flags |= ARC_BUF_FLAG_ENCRYPTED;
		flags |= ARC_FILL_COMPRESSED | ARC_FILL_ENCRYPTED;
	} else if (compressed &&
	    arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF) {
		buf->b_flags |= ARC_BUF_FLAG_COMPRESSED;
		flags |= ARC_FILL_COMPRESSED;
	}

	if (noauth) {
		ASSERT0(encrypted);
		flags |= ARC_FILL_NOAUTH;
	}

	 
	boolean_t can_share = arc_can_share(hdr, buf) &&
	    !HDR_L2_WRITING(hdr) &&
	    hdr->b_l1hdr.b_pabd != NULL &&
	    abd_is_linear(hdr->b_l1hdr.b_pabd) &&
	    !abd_is_linear_page(hdr->b_l1hdr.b_pabd);

	 
	if (can_share) {
		buf->b_data = abd_to_buf(hdr->b_l1hdr.b_pabd);
		buf->b_flags |= ARC_BUF_FLAG_SHARED;
		arc_hdr_set_flags(hdr, ARC_FLAG_SHARED_DATA);
	} else {
		buf->b_data =
		    arc_get_data_buf(hdr, arc_buf_size(buf), buf);
		ARCSTAT_INCR(arcstat_overhead_size, arc_buf_size(buf));
	}
	VERIFY3P(buf->b_data, !=, NULL);

	hdr->b_l1hdr.b_buf = buf;

	 
	if (fill) {
		ASSERT3P(zb, !=, NULL);
		return (arc_buf_fill(buf, spa, zb, flags));
	}

	return (0);
}

static const char *arc_onloan_tag = "onloan";

static inline void
arc_loaned_bytes_update(int64_t delta)
{
	atomic_add_64(&arc_loaned_bytes, delta);

	 
	ASSERT3S(atomic_add_64_nv(&arc_loaned_bytes, 0), >=, 0);
}

 
arc_buf_t *
arc_loan_buf(spa_t *spa, boolean_t is_metadata, int size)
{
	arc_buf_t *buf = arc_alloc_buf(spa, arc_onloan_tag,
	    is_metadata ? ARC_BUFC_METADATA : ARC_BUFC_DATA, size);

	arc_loaned_bytes_update(arc_buf_size(buf));

	return (buf);
}

arc_buf_t *
arc_loan_compressed_buf(spa_t *spa, uint64_t psize, uint64_t lsize,
    enum zio_compress compression_type, uint8_t complevel)
{
	arc_buf_t *buf = arc_alloc_compressed_buf(spa, arc_onloan_tag,
	    psize, lsize, compression_type, complevel);

	arc_loaned_bytes_update(arc_buf_size(buf));

	return (buf);
}

arc_buf_t *
arc_loan_raw_buf(spa_t *spa, uint64_t dsobj, boolean_t byteorder,
    const uint8_t *salt, const uint8_t *iv, const uint8_t *mac,
    dmu_object_type_t ot, uint64_t psize, uint64_t lsize,
    enum zio_compress compression_type, uint8_t complevel)
{
	arc_buf_t *buf = arc_alloc_raw_buf(spa, arc_onloan_tag, dsobj,
	    byteorder, salt, iv, mac, ot, psize, lsize, compression_type,
	    complevel);

	atomic_add_64(&arc_loaned_bytes, psize);
	return (buf);
}


 
void
arc_return_buf(arc_buf_t *buf, const void *tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT3P(buf->b_data, !=, NULL);
	ASSERT(HDR_HAS_L1HDR(hdr));
	(void) zfs_refcount_add(&hdr->b_l1hdr.b_refcnt, tag);
	(void) zfs_refcount_remove(&hdr->b_l1hdr.b_refcnt, arc_onloan_tag);

	arc_loaned_bytes_update(-arc_buf_size(buf));
}

 
void
arc_loan_inuse_buf(arc_buf_t *buf, const void *tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT3P(buf->b_data, !=, NULL);
	ASSERT(HDR_HAS_L1HDR(hdr));
	(void) zfs_refcount_add(&hdr->b_l1hdr.b_refcnt, arc_onloan_tag);
	(void) zfs_refcount_remove(&hdr->b_l1hdr.b_refcnt, tag);

	arc_loaned_bytes_update(arc_buf_size(buf));
}

static void
l2arc_free_abd_on_write(abd_t *abd, size_t size, arc_buf_contents_t type)
{
	l2arc_data_free_t *df = kmem_alloc(sizeof (*df), KM_SLEEP);

	df->l2df_abd = abd;
	df->l2df_size = size;
	df->l2df_type = type;
	mutex_enter(&l2arc_free_on_write_mtx);
	list_insert_head(l2arc_free_on_write, df);
	mutex_exit(&l2arc_free_on_write_mtx);
}

static void
arc_hdr_free_on_write(arc_buf_hdr_t *hdr, boolean_t free_rdata)
{
	arc_state_t *state = hdr->b_l1hdr.b_state;
	arc_buf_contents_t type = arc_buf_type(hdr);
	uint64_t size = (free_rdata) ? HDR_GET_PSIZE(hdr) : arc_hdr_size(hdr);

	 
	if (multilist_link_active(&hdr->b_l1hdr.b_arc_node)) {
		ASSERT(zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt));
		ASSERT(state != arc_anon && state != arc_l2c_only);

		(void) zfs_refcount_remove_many(&state->arcs_esize[type],
		    size, hdr);
	}
	(void) zfs_refcount_remove_many(&state->arcs_size[type], size, hdr);
	if (type == ARC_BUFC_METADATA) {
		arc_space_return(size, ARC_SPACE_META);
	} else {
		ASSERT(type == ARC_BUFC_DATA);
		arc_space_return(size, ARC_SPACE_DATA);
	}

	if (free_rdata) {
		l2arc_free_abd_on_write(hdr->b_crypt_hdr.b_rabd, size, type);
	} else {
		l2arc_free_abd_on_write(hdr->b_l1hdr.b_pabd, size, type);
	}
}

 
static void
arc_share_buf(arc_buf_hdr_t *hdr, arc_buf_t *buf)
{
	ASSERT(arc_can_share(hdr, buf));
	ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
	ASSERT(!ARC_BUF_ENCRYPTED(buf));
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));

	 
	zfs_refcount_transfer_ownership_many(
	    &hdr->b_l1hdr.b_state->arcs_size[arc_buf_type(hdr)],
	    arc_hdr_size(hdr), buf, hdr);
	hdr->b_l1hdr.b_pabd = abd_get_from_buf(buf->b_data, arc_buf_size(buf));
	abd_take_ownership_of_buf(hdr->b_l1hdr.b_pabd,
	    HDR_ISTYPE_METADATA(hdr));
	arc_hdr_set_flags(hdr, ARC_FLAG_SHARED_DATA);
	buf->b_flags |= ARC_BUF_FLAG_SHARED;

	 
	ARCSTAT_INCR(arcstat_compressed_size, arc_hdr_size(hdr));
	ARCSTAT_INCR(arcstat_uncompressed_size, HDR_GET_LSIZE(hdr));
	ARCSTAT_INCR(arcstat_overhead_size, -arc_buf_size(buf));
}

static void
arc_unshare_buf(arc_buf_hdr_t *hdr, arc_buf_t *buf)
{
	ASSERT(arc_buf_is_shared(buf));
	ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));

	 
	zfs_refcount_transfer_ownership_many(
	    &hdr->b_l1hdr.b_state->arcs_size[arc_buf_type(hdr)],
	    arc_hdr_size(hdr), hdr, buf);
	arc_hdr_clear_flags(hdr, ARC_FLAG_SHARED_DATA);
	abd_release_ownership_of_buf(hdr->b_l1hdr.b_pabd);
	abd_free(hdr->b_l1hdr.b_pabd);
	hdr->b_l1hdr.b_pabd = NULL;
	buf->b_flags &= ~ARC_BUF_FLAG_SHARED;

	 
	ARCSTAT_INCR(arcstat_compressed_size, -arc_hdr_size(hdr));
	ARCSTAT_INCR(arcstat_uncompressed_size, -HDR_GET_LSIZE(hdr));
	ARCSTAT_INCR(arcstat_overhead_size, arc_buf_size(buf));
}

 
static arc_buf_t *
arc_buf_remove(arc_buf_hdr_t *hdr, arc_buf_t *buf)
{
	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT(HDR_EMPTY_OR_LOCKED(hdr));

	arc_buf_t **bufp = &hdr->b_l1hdr.b_buf;
	arc_buf_t *lastbuf = NULL;

	 
	while (*bufp != NULL) {
		if (*bufp == buf)
			*bufp = buf->b_next;

		 
		if (*bufp != NULL) {
			lastbuf = *bufp;
			bufp = &(*bufp)->b_next;
		}
	}
	buf->b_next = NULL;
	ASSERT3P(lastbuf, !=, buf);
	IMPLY(lastbuf != NULL, ARC_BUF_LAST(lastbuf));

	return (lastbuf);
}

 
static void
arc_buf_destroy_impl(arc_buf_t *buf)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	 
	if (buf->b_data != NULL) {
		 
		ASSERT(HDR_EMPTY_OR_LOCKED(hdr));

		arc_cksum_verify(buf);
		arc_buf_unwatch(buf);

		if (ARC_BUF_SHARED(buf)) {
			arc_hdr_clear_flags(hdr, ARC_FLAG_SHARED_DATA);
		} else {
			ASSERT(!arc_buf_is_shared(buf));
			uint64_t size = arc_buf_size(buf);
			arc_free_data_buf(hdr, buf->b_data, size, buf);
			ARCSTAT_INCR(arcstat_overhead_size, -size);
		}
		buf->b_data = NULL;

		 
		if (ARC_BUF_ENCRYPTED(buf) && HDR_HAS_RABD(hdr) &&
		    hdr->b_l1hdr.b_pabd != NULL && !HDR_IO_IN_PROGRESS(hdr)) {
			arc_buf_t *b;
			for (b = hdr->b_l1hdr.b_buf; b; b = b->b_next) {
				if (b != buf && ARC_BUF_ENCRYPTED(b))
					break;
			}
			if (b == NULL)
				arc_hdr_free_abd(hdr, B_TRUE);
		}
	}

	arc_buf_t *lastbuf = arc_buf_remove(hdr, buf);

	if (ARC_BUF_SHARED(buf) && !ARC_BUF_COMPRESSED(buf)) {
		 
		if (lastbuf != NULL && !ARC_BUF_ENCRYPTED(lastbuf)) {
			 
			ASSERT(!arc_buf_is_shared(lastbuf));
			 
			ASSERT(!ARC_BUF_COMPRESSED(lastbuf));

			ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);
			arc_hdr_free_abd(hdr, B_FALSE);

			 
			arc_share_buf(hdr, lastbuf);
		}
	} else if (HDR_SHARED_DATA(hdr)) {
		 
		ASSERT3P(lastbuf, !=, NULL);
		ASSERT(arc_buf_is_shared(lastbuf) ||
		    arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF);
	}

	 
	if (!arc_hdr_has_uncompressed_buf(hdr)) {
		arc_cksum_free(hdr);
	}

	 
	buf->b_hdr = NULL;
	kmem_cache_free(buf_cache, buf);
}

static void
arc_hdr_alloc_abd(arc_buf_hdr_t *hdr, int alloc_flags)
{
	uint64_t size;
	boolean_t alloc_rdata = ((alloc_flags & ARC_HDR_ALLOC_RDATA) != 0);

	ASSERT3U(HDR_GET_LSIZE(hdr), >, 0);
	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT(!HDR_SHARED_DATA(hdr) || alloc_rdata);
	IMPLY(alloc_rdata, HDR_PROTECTED(hdr));

	if (alloc_rdata) {
		size = HDR_GET_PSIZE(hdr);
		ASSERT3P(hdr->b_crypt_hdr.b_rabd, ==, NULL);
		hdr->b_crypt_hdr.b_rabd = arc_get_data_abd(hdr, size, hdr,
		    alloc_flags);
		ASSERT3P(hdr->b_crypt_hdr.b_rabd, !=, NULL);
		ARCSTAT_INCR(arcstat_raw_size, size);
	} else {
		size = arc_hdr_size(hdr);
		ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
		hdr->b_l1hdr.b_pabd = arc_get_data_abd(hdr, size, hdr,
		    alloc_flags);
		ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);
	}

	ARCSTAT_INCR(arcstat_compressed_size, size);
	ARCSTAT_INCR(arcstat_uncompressed_size, HDR_GET_LSIZE(hdr));
}

static void
arc_hdr_free_abd(arc_buf_hdr_t *hdr, boolean_t free_rdata)
{
	uint64_t size = (free_rdata) ? HDR_GET_PSIZE(hdr) : arc_hdr_size(hdr);

	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT(hdr->b_l1hdr.b_pabd != NULL || HDR_HAS_RABD(hdr));
	IMPLY(free_rdata, HDR_HAS_RABD(hdr));

	 
	if (HDR_L2_WRITING(hdr)) {
		arc_hdr_free_on_write(hdr, free_rdata);
		ARCSTAT_BUMP(arcstat_l2_free_on_write);
	} else if (free_rdata) {
		arc_free_data_abd(hdr, hdr->b_crypt_hdr.b_rabd, size, hdr);
	} else {
		arc_free_data_abd(hdr, hdr->b_l1hdr.b_pabd, size, hdr);
	}

	if (free_rdata) {
		hdr->b_crypt_hdr.b_rabd = NULL;
		ARCSTAT_INCR(arcstat_raw_size, -size);
	} else {
		hdr->b_l1hdr.b_pabd = NULL;
	}

	if (hdr->b_l1hdr.b_pabd == NULL && !HDR_HAS_RABD(hdr))
		hdr->b_l1hdr.b_byteswap = DMU_BSWAP_NUMFUNCS;

	ARCSTAT_INCR(arcstat_compressed_size, -size);
	ARCSTAT_INCR(arcstat_uncompressed_size, -HDR_GET_LSIZE(hdr));
}

 
static arc_buf_hdr_t *
arc_hdr_alloc(uint64_t spa, int32_t psize, int32_t lsize,
    boolean_t protected, enum zio_compress compression_type, uint8_t complevel,
    arc_buf_contents_t type)
{
	arc_buf_hdr_t *hdr;

	VERIFY(type == ARC_BUFC_DATA || type == ARC_BUFC_METADATA);
	hdr = kmem_cache_alloc(hdr_full_cache, KM_PUSHPAGE);

	ASSERT(HDR_EMPTY(hdr));
#ifdef ZFS_DEBUG
	ASSERT3P(hdr->b_l1hdr.b_freeze_cksum, ==, NULL);
#endif
	HDR_SET_PSIZE(hdr, psize);
	HDR_SET_LSIZE(hdr, lsize);
	hdr->b_spa = spa;
	hdr->b_type = type;
	hdr->b_flags = 0;
	arc_hdr_set_flags(hdr, arc_bufc_to_flags(type) | ARC_FLAG_HAS_L1HDR);
	arc_hdr_set_compress(hdr, compression_type);
	hdr->b_complevel = complevel;
	if (protected)
		arc_hdr_set_flags(hdr, ARC_FLAG_PROTECTED);

	hdr->b_l1hdr.b_state = arc_anon;
	hdr->b_l1hdr.b_arc_access = 0;
	hdr->b_l1hdr.b_mru_hits = 0;
	hdr->b_l1hdr.b_mru_ghost_hits = 0;
	hdr->b_l1hdr.b_mfu_hits = 0;
	hdr->b_l1hdr.b_mfu_ghost_hits = 0;
	hdr->b_l1hdr.b_buf = NULL;

	ASSERT(zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt));

	return (hdr);
}

 
static arc_buf_hdr_t *
arc_hdr_realloc(arc_buf_hdr_t *hdr, kmem_cache_t *old, kmem_cache_t *new)
{
	ASSERT(HDR_HAS_L2HDR(hdr));

	arc_buf_hdr_t *nhdr;
	l2arc_dev_t *dev = hdr->b_l2hdr.b_dev;

	ASSERT((old == hdr_full_cache && new == hdr_l2only_cache) ||
	    (old == hdr_l2only_cache && new == hdr_full_cache));

	nhdr = kmem_cache_alloc(new, KM_PUSHPAGE);

	ASSERT(MUTEX_HELD(HDR_LOCK(hdr)));
	buf_hash_remove(hdr);

	memcpy(nhdr, hdr, HDR_L2ONLY_SIZE);

	if (new == hdr_full_cache) {
		arc_hdr_set_flags(nhdr, ARC_FLAG_HAS_L1HDR);
		 
		nhdr->b_l1hdr.b_state = arc_l2c_only;

		 
		ASSERT3P(nhdr->b_l1hdr.b_pabd, ==, NULL);
		ASSERT(!HDR_HAS_RABD(hdr));
	} else {
		ASSERT3P(hdr->b_l1hdr.b_buf, ==, NULL);
#ifdef ZFS_DEBUG
		ASSERT3P(hdr->b_l1hdr.b_freeze_cksum, ==, NULL);
#endif

		 
		ASSERT(!multilist_link_active(&hdr->b_l1hdr.b_arc_node));

		 
		VERIFY(!HDR_L2_WRITING(hdr));
		VERIFY3P(hdr->b_l1hdr.b_pabd, ==, NULL);
		ASSERT(!HDR_HAS_RABD(hdr));

		arc_hdr_clear_flags(nhdr, ARC_FLAG_HAS_L1HDR);
	}
	 
	(void) buf_hash_insert(nhdr, NULL);

	ASSERT(list_link_active(&hdr->b_l2hdr.b_l2node));

	mutex_enter(&dev->l2ad_mtx);

	 
	list_insert_after(&dev->l2ad_buflist, hdr, nhdr);
	list_remove(&dev->l2ad_buflist, hdr);

	mutex_exit(&dev->l2ad_mtx);

	 

	(void) zfs_refcount_remove_many(&dev->l2ad_alloc,
	    arc_hdr_size(hdr), hdr);
	(void) zfs_refcount_add_many(&dev->l2ad_alloc,
	    arc_hdr_size(nhdr), nhdr);

	buf_discard_identity(hdr);
	kmem_cache_free(old, hdr);

	return (nhdr);
}

 
void
arc_convert_to_raw(arc_buf_t *buf, uint64_t dsobj, boolean_t byteorder,
    dmu_object_type_t ot, const uint8_t *salt, const uint8_t *iv,
    const uint8_t *mac)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT(ot == DMU_OT_DNODE || ot == DMU_OT_OBJSET);
	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT3P(hdr->b_l1hdr.b_state, ==, arc_anon);

	buf->b_flags |= (ARC_BUF_FLAG_COMPRESSED | ARC_BUF_FLAG_ENCRYPTED);
	arc_hdr_set_flags(hdr, ARC_FLAG_PROTECTED);
	hdr->b_crypt_hdr.b_dsobj = dsobj;
	hdr->b_crypt_hdr.b_ot = ot;
	hdr->b_l1hdr.b_byteswap = (byteorder == ZFS_HOST_BYTEORDER) ?
	    DMU_BSWAP_NUMFUNCS : DMU_OT_BYTESWAP(ot);
	if (!arc_hdr_has_uncompressed_buf(hdr))
		arc_cksum_free(hdr);

	if (salt != NULL)
		memcpy(hdr->b_crypt_hdr.b_salt, salt, ZIO_DATA_SALT_LEN);
	if (iv != NULL)
		memcpy(hdr->b_crypt_hdr.b_iv, iv, ZIO_DATA_IV_LEN);
	if (mac != NULL)
		memcpy(hdr->b_crypt_hdr.b_mac, mac, ZIO_DATA_MAC_LEN);
}

 
arc_buf_t *
arc_alloc_buf(spa_t *spa, const void *tag, arc_buf_contents_t type,
    int32_t size)
{
	arc_buf_hdr_t *hdr = arc_hdr_alloc(spa_load_guid(spa), size, size,
	    B_FALSE, ZIO_COMPRESS_OFF, 0, type);

	arc_buf_t *buf = NULL;
	VERIFY0(arc_buf_alloc_impl(hdr, spa, NULL, tag, B_FALSE, B_FALSE,
	    B_FALSE, B_FALSE, &buf));
	arc_buf_thaw(buf);

	return (buf);
}

 
arc_buf_t *
arc_alloc_compressed_buf(spa_t *spa, const void *tag, uint64_t psize,
    uint64_t lsize, enum zio_compress compression_type, uint8_t complevel)
{
	ASSERT3U(lsize, >, 0);
	ASSERT3U(lsize, >=, psize);
	ASSERT3U(compression_type, >, ZIO_COMPRESS_OFF);
	ASSERT3U(compression_type, <, ZIO_COMPRESS_FUNCTIONS);

	arc_buf_hdr_t *hdr = arc_hdr_alloc(spa_load_guid(spa), psize, lsize,
	    B_FALSE, compression_type, complevel, ARC_BUFC_DATA);

	arc_buf_t *buf = NULL;
	VERIFY0(arc_buf_alloc_impl(hdr, spa, NULL, tag, B_FALSE,
	    B_TRUE, B_FALSE, B_FALSE, &buf));
	arc_buf_thaw(buf);

	 
	arc_share_buf(hdr, buf);

	return (buf);
}

arc_buf_t *
arc_alloc_raw_buf(spa_t *spa, const void *tag, uint64_t dsobj,
    boolean_t byteorder, const uint8_t *salt, const uint8_t *iv,
    const uint8_t *mac, dmu_object_type_t ot, uint64_t psize, uint64_t lsize,
    enum zio_compress compression_type, uint8_t complevel)
{
	arc_buf_hdr_t *hdr;
	arc_buf_t *buf;
	arc_buf_contents_t type = DMU_OT_IS_METADATA(ot) ?
	    ARC_BUFC_METADATA : ARC_BUFC_DATA;

	ASSERT3U(lsize, >, 0);
	ASSERT3U(lsize, >=, psize);
	ASSERT3U(compression_type, >=, ZIO_COMPRESS_OFF);
	ASSERT3U(compression_type, <, ZIO_COMPRESS_FUNCTIONS);

	hdr = arc_hdr_alloc(spa_load_guid(spa), psize, lsize, B_TRUE,
	    compression_type, complevel, type);

	hdr->b_crypt_hdr.b_dsobj = dsobj;
	hdr->b_crypt_hdr.b_ot = ot;
	hdr->b_l1hdr.b_byteswap = (byteorder == ZFS_HOST_BYTEORDER) ?
	    DMU_BSWAP_NUMFUNCS : DMU_OT_BYTESWAP(ot);
	memcpy(hdr->b_crypt_hdr.b_salt, salt, ZIO_DATA_SALT_LEN);
	memcpy(hdr->b_crypt_hdr.b_iv, iv, ZIO_DATA_IV_LEN);
	memcpy(hdr->b_crypt_hdr.b_mac, mac, ZIO_DATA_MAC_LEN);

	 
	buf = NULL;
	VERIFY0(arc_buf_alloc_impl(hdr, spa, NULL, tag, B_TRUE, B_TRUE,
	    B_FALSE, B_FALSE, &buf));
	arc_buf_thaw(buf);

	return (buf);
}

static void
l2arc_hdr_arcstats_update(arc_buf_hdr_t *hdr, boolean_t incr,
    boolean_t state_only)
{
	l2arc_buf_hdr_t *l2hdr = &hdr->b_l2hdr;
	l2arc_dev_t *dev = l2hdr->b_dev;
	uint64_t lsize = HDR_GET_LSIZE(hdr);
	uint64_t psize = HDR_GET_PSIZE(hdr);
	uint64_t asize = vdev_psize_to_asize(dev->l2ad_vdev, psize);
	arc_buf_contents_t type = hdr->b_type;
	int64_t lsize_s;
	int64_t psize_s;
	int64_t asize_s;

	if (incr) {
		lsize_s = lsize;
		psize_s = psize;
		asize_s = asize;
	} else {
		lsize_s = -lsize;
		psize_s = -psize;
		asize_s = -asize;
	}

	 
	if (HDR_PREFETCH(hdr)) {
		ARCSTAT_INCR(arcstat_l2_prefetch_asize, asize_s);
	} else {
		 
		switch (hdr->b_l2hdr.b_arcs_state) {
			case ARC_STATE_MRU_GHOST:
			case ARC_STATE_MRU:
				ARCSTAT_INCR(arcstat_l2_mru_asize, asize_s);
				break;
			case ARC_STATE_MFU_GHOST:
			case ARC_STATE_MFU:
				ARCSTAT_INCR(arcstat_l2_mfu_asize, asize_s);
				break;
			default:
				break;
		}
	}

	if (state_only)
		return;

	ARCSTAT_INCR(arcstat_l2_psize, psize_s);
	ARCSTAT_INCR(arcstat_l2_lsize, lsize_s);

	switch (type) {
		case ARC_BUFC_DATA:
			ARCSTAT_INCR(arcstat_l2_bufc_data_asize, asize_s);
			break;
		case ARC_BUFC_METADATA:
			ARCSTAT_INCR(arcstat_l2_bufc_metadata_asize, asize_s);
			break;
		default:
			break;
	}
}


static void
arc_hdr_l2hdr_destroy(arc_buf_hdr_t *hdr)
{
	l2arc_buf_hdr_t *l2hdr = &hdr->b_l2hdr;
	l2arc_dev_t *dev = l2hdr->b_dev;
	uint64_t psize = HDR_GET_PSIZE(hdr);
	uint64_t asize = vdev_psize_to_asize(dev->l2ad_vdev, psize);

	ASSERT(MUTEX_HELD(&dev->l2ad_mtx));
	ASSERT(HDR_HAS_L2HDR(hdr));

	list_remove(&dev->l2ad_buflist, hdr);

	l2arc_hdr_arcstats_decrement(hdr);
	vdev_space_update(dev->l2ad_vdev, -asize, 0, 0);

	(void) zfs_refcount_remove_many(&dev->l2ad_alloc, arc_hdr_size(hdr),
	    hdr);
	arc_hdr_clear_flags(hdr, ARC_FLAG_HAS_L2HDR);
}

static void
arc_hdr_destroy(arc_buf_hdr_t *hdr)
{
	if (HDR_HAS_L1HDR(hdr)) {
		ASSERT(zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt));
		ASSERT3P(hdr->b_l1hdr.b_state, ==, arc_anon);
	}
	ASSERT(!HDR_IO_IN_PROGRESS(hdr));
	ASSERT(!HDR_IN_HASH_TABLE(hdr));

	if (HDR_HAS_L2HDR(hdr)) {
		l2arc_dev_t *dev = hdr->b_l2hdr.b_dev;
		boolean_t buflist_held = MUTEX_HELD(&dev->l2ad_mtx);

		if (!buflist_held)
			mutex_enter(&dev->l2ad_mtx);

		 
		if (HDR_HAS_L2HDR(hdr)) {

			if (!HDR_EMPTY(hdr))
				buf_discard_identity(hdr);

			arc_hdr_l2hdr_destroy(hdr);
		}

		if (!buflist_held)
			mutex_exit(&dev->l2ad_mtx);
	}

	 
	if (!HDR_EMPTY(hdr))
		buf_discard_identity(hdr);

	if (HDR_HAS_L1HDR(hdr)) {
		arc_cksum_free(hdr);

		while (hdr->b_l1hdr.b_buf != NULL)
			arc_buf_destroy_impl(hdr->b_l1hdr.b_buf);

		if (hdr->b_l1hdr.b_pabd != NULL)
			arc_hdr_free_abd(hdr, B_FALSE);

		if (HDR_HAS_RABD(hdr))
			arc_hdr_free_abd(hdr, B_TRUE);
	}

	ASSERT3P(hdr->b_hash_next, ==, NULL);
	if (HDR_HAS_L1HDR(hdr)) {
		ASSERT(!multilist_link_active(&hdr->b_l1hdr.b_arc_node));
		ASSERT3P(hdr->b_l1hdr.b_acb, ==, NULL);
#ifdef ZFS_DEBUG
		ASSERT3P(hdr->b_l1hdr.b_freeze_cksum, ==, NULL);
#endif
		kmem_cache_free(hdr_full_cache, hdr);
	} else {
		kmem_cache_free(hdr_l2only_cache, hdr);
	}
}

void
arc_buf_destroy(arc_buf_t *buf, const void *tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	if (hdr->b_l1hdr.b_state == arc_anon) {
		ASSERT3P(hdr->b_l1hdr.b_buf, ==, buf);
		ASSERT(ARC_BUF_LAST(buf));
		ASSERT(!HDR_IO_IN_PROGRESS(hdr));
		VERIFY0(remove_reference(hdr, tag));
		return;
	}

	kmutex_t *hash_lock = HDR_LOCK(hdr);
	mutex_enter(hash_lock);

	ASSERT3P(hdr, ==, buf->b_hdr);
	ASSERT3P(hdr->b_l1hdr.b_buf, !=, NULL);
	ASSERT3P(hash_lock, ==, HDR_LOCK(hdr));
	ASSERT3P(hdr->b_l1hdr.b_state, !=, arc_anon);
	ASSERT3P(buf->b_data, !=, NULL);

	arc_buf_destroy_impl(buf);
	(void) remove_reference(hdr, tag);
	mutex_exit(hash_lock);
}

 
static int64_t
arc_evict_hdr(arc_buf_hdr_t *hdr, uint64_t *real_evicted)
{
	arc_state_t *evicted_state, *state;
	int64_t bytes_evicted = 0;
	uint_t min_lifetime = HDR_PRESCIENT_PREFETCH(hdr) ?
	    arc_min_prescient_prefetch_ms : arc_min_prefetch_ms;

	ASSERT(MUTEX_HELD(HDR_LOCK(hdr)));
	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT(!HDR_IO_IN_PROGRESS(hdr));
	ASSERT3P(hdr->b_l1hdr.b_buf, ==, NULL);
	ASSERT0(zfs_refcount_count(&hdr->b_l1hdr.b_refcnt));

	*real_evicted = 0;
	state = hdr->b_l1hdr.b_state;
	if (GHOST_STATE(state)) {

		 
		if (HDR_HAS_L2HDR(hdr) && HDR_L2_WRITING(hdr)) {
			ARCSTAT_BUMP(arcstat_evict_l2_skip);
			return (bytes_evicted);
		}

		ARCSTAT_BUMP(arcstat_deleted);
		bytes_evicted += HDR_GET_LSIZE(hdr);

		DTRACE_PROBE1(arc__delete, arc_buf_hdr_t *, hdr);

		if (HDR_HAS_L2HDR(hdr)) {
			ASSERT(hdr->b_l1hdr.b_pabd == NULL);
			ASSERT(!HDR_HAS_RABD(hdr));
			 
			arc_change_state(arc_l2c_only, hdr);
			 
			(void) arc_hdr_realloc(hdr, hdr_full_cache,
			    hdr_l2only_cache);
			*real_evicted += HDR_FULL_SIZE - HDR_L2ONLY_SIZE;
		} else {
			arc_change_state(arc_anon, hdr);
			arc_hdr_destroy(hdr);
			*real_evicted += HDR_FULL_SIZE;
		}
		return (bytes_evicted);
	}

	ASSERT(state == arc_mru || state == arc_mfu || state == arc_uncached);
	evicted_state = (state == arc_uncached) ? arc_anon :
	    ((state == arc_mru) ? arc_mru_ghost : arc_mfu_ghost);

	 
	if ((hdr->b_flags & (ARC_FLAG_PREFETCH | ARC_FLAG_INDIRECT)) &&
	    ddi_get_lbolt() - hdr->b_l1hdr.b_arc_access <
	    MSEC_TO_TICK(min_lifetime)) {
		ARCSTAT_BUMP(arcstat_evict_skip);
		return (bytes_evicted);
	}

	if (HDR_HAS_L2HDR(hdr)) {
		ARCSTAT_INCR(arcstat_evict_l2_cached, HDR_GET_LSIZE(hdr));
	} else {
		if (l2arc_write_eligible(hdr->b_spa, hdr)) {
			ARCSTAT_INCR(arcstat_evict_l2_eligible,
			    HDR_GET_LSIZE(hdr));

			switch (state->arcs_state) {
				case ARC_STATE_MRU:
					ARCSTAT_INCR(
					    arcstat_evict_l2_eligible_mru,
					    HDR_GET_LSIZE(hdr));
					break;
				case ARC_STATE_MFU:
					ARCSTAT_INCR(
					    arcstat_evict_l2_eligible_mfu,
					    HDR_GET_LSIZE(hdr));
					break;
				default:
					break;
			}
		} else {
			ARCSTAT_INCR(arcstat_evict_l2_ineligible,
			    HDR_GET_LSIZE(hdr));
		}
	}

	bytes_evicted += arc_hdr_size(hdr);
	*real_evicted += arc_hdr_size(hdr);

	 
	if (hdr->b_l1hdr.b_pabd != NULL)
		arc_hdr_free_abd(hdr, B_FALSE);

	if (HDR_HAS_RABD(hdr))
		arc_hdr_free_abd(hdr, B_TRUE);

	arc_change_state(evicted_state, hdr);
	DTRACE_PROBE1(arc__evict, arc_buf_hdr_t *, hdr);
	if (evicted_state == arc_anon) {
		arc_hdr_destroy(hdr);
		*real_evicted += HDR_FULL_SIZE;
	} else {
		ASSERT(HDR_IN_HASH_TABLE(hdr));
	}

	return (bytes_evicted);
}

static void
arc_set_need_free(void)
{
	ASSERT(MUTEX_HELD(&arc_evict_lock));
	int64_t remaining = arc_free_memory() - arc_sys_free / 2;
	arc_evict_waiter_t *aw = list_tail(&arc_evict_waiters);
	if (aw == NULL) {
		arc_need_free = MAX(-remaining, 0);
	} else {
		arc_need_free =
		    MAX(-remaining, (int64_t)(aw->aew_count - arc_evict_count));
	}
}

static uint64_t
arc_evict_state_impl(multilist_t *ml, int idx, arc_buf_hdr_t *marker,
    uint64_t spa, uint64_t bytes)
{
	multilist_sublist_t *mls;
	uint64_t bytes_evicted = 0, real_evicted = 0;
	arc_buf_hdr_t *hdr;
	kmutex_t *hash_lock;
	uint_t evict_count = zfs_arc_evict_batch_limit;

	ASSERT3P(marker, !=, NULL);

	mls = multilist_sublist_lock(ml, idx);

	for (hdr = multilist_sublist_prev(mls, marker); likely(hdr != NULL);
	    hdr = multilist_sublist_prev(mls, marker)) {
		if ((evict_count == 0) || (bytes_evicted >= bytes))
			break;

		 
		multilist_sublist_move_forward(mls, marker);

		 
		if (hdr->b_spa == 0)
			continue;

		 
		if (spa != 0 && hdr->b_spa != spa) {
			ARCSTAT_BUMP(arcstat_evict_skip);
			continue;
		}

		hash_lock = HDR_LOCK(hdr);

		 
		ASSERT(!MUTEX_HELD(hash_lock));

		if (mutex_tryenter(hash_lock)) {
			uint64_t revicted;
			uint64_t evicted = arc_evict_hdr(hdr, &revicted);
			mutex_exit(hash_lock);

			bytes_evicted += evicted;
			real_evicted += revicted;

			 
			if (evicted != 0)
				evict_count--;

		} else {
			ARCSTAT_BUMP(arcstat_mutex_miss);
		}
	}

	multilist_sublist_unlock(mls);

	 
	mutex_enter(&arc_evict_lock);
	arc_evict_count += real_evicted;

	if (arc_free_memory() > arc_sys_free / 2) {
		arc_evict_waiter_t *aw;
		while ((aw = list_head(&arc_evict_waiters)) != NULL &&
		    aw->aew_count <= arc_evict_count) {
			list_remove(&arc_evict_waiters, aw);
			cv_broadcast(&aw->aew_cv);
		}
	}
	arc_set_need_free();
	mutex_exit(&arc_evict_lock);

	 
	kpreempt(KPREEMPT_SYNC);

	return (bytes_evicted);
}

 
static arc_buf_hdr_t **
arc_state_alloc_markers(int count)
{
	arc_buf_hdr_t **markers;

	markers = kmem_zalloc(sizeof (*markers) * count, KM_SLEEP);
	for (int i = 0; i < count; i++) {
		markers[i] = kmem_cache_alloc(hdr_full_cache, KM_SLEEP);

		 
		markers[i]->b_spa = 0;

	}
	return (markers);
}

static void
arc_state_free_markers(arc_buf_hdr_t **markers, int count)
{
	for (int i = 0; i < count; i++)
		kmem_cache_free(hdr_full_cache, markers[i]);
	kmem_free(markers, sizeof (*markers) * count);
}

 
static uint64_t
arc_evict_state(arc_state_t *state, arc_buf_contents_t type, uint64_t spa,
    uint64_t bytes)
{
	uint64_t total_evicted = 0;
	multilist_t *ml = &state->arcs_list[type];
	int num_sublists;
	arc_buf_hdr_t **markers;

	num_sublists = multilist_get_num_sublists(ml);

	 
	if (zthr_iscurthread(arc_evict_zthr)) {
		markers = arc_state_evict_markers;
		ASSERT3S(num_sublists, <=, arc_state_evict_marker_count);
	} else {
		markers = arc_state_alloc_markers(num_sublists);
	}
	for (int i = 0; i < num_sublists; i++) {
		multilist_sublist_t *mls;

		mls = multilist_sublist_lock(ml, i);
		multilist_sublist_insert_tail(mls, markers[i]);
		multilist_sublist_unlock(mls);
	}

	 
	while (total_evicted < bytes) {
		int sublist_idx = multilist_get_random_index(ml);
		uint64_t scan_evicted = 0;

		 
		for (int i = 0; i < num_sublists; i++) {
			uint64_t bytes_remaining;
			uint64_t bytes_evicted;

			if (total_evicted < bytes)
				bytes_remaining = bytes - total_evicted;
			else
				break;

			bytes_evicted = arc_evict_state_impl(ml, sublist_idx,
			    markers[sublist_idx], spa, bytes_remaining);

			scan_evicted += bytes_evicted;
			total_evicted += bytes_evicted;

			 
			if (++sublist_idx >= num_sublists)
				sublist_idx = 0;
		}

		 
		if (scan_evicted == 0) {
			 
			ASSERT3S(bytes, !=, 0);

			 
			if (bytes != ARC_EVICT_ALL) {
				ASSERT3S(total_evicted, <, bytes);
				ARCSTAT_BUMP(arcstat_evict_not_enough);
			}

			break;
		}
	}

	for (int i = 0; i < num_sublists; i++) {
		multilist_sublist_t *mls = multilist_sublist_lock(ml, i);
		multilist_sublist_remove(mls, markers[i]);
		multilist_sublist_unlock(mls);
	}
	if (markers != arc_state_evict_markers)
		arc_state_free_markers(markers, num_sublists);

	return (total_evicted);
}

 
static uint64_t
arc_flush_state(arc_state_t *state, uint64_t spa, arc_buf_contents_t type,
    boolean_t retry)
{
	uint64_t evicted = 0;

	while (zfs_refcount_count(&state->arcs_esize[type]) != 0) {
		evicted += arc_evict_state(state, type, spa, ARC_EVICT_ALL);

		if (!retry)
			break;
	}

	return (evicted);
}

 
static uint64_t
arc_evict_impl(arc_state_t *state, arc_buf_contents_t type, int64_t bytes)
{
	uint64_t delta;

	if (bytes > 0 && zfs_refcount_count(&state->arcs_esize[type]) > 0) {
		delta = MIN(zfs_refcount_count(&state->arcs_esize[type]),
		    bytes);
		return (arc_evict_state(state, type, 0, delta));
	}

	return (0);
}

 
static uint64_t
arc_evict_adj(uint64_t frac, uint64_t total, uint64_t up, uint64_t down,
    uint_t balance)
{
	if (total < 8 || up + down == 0)
		return (frac);

	 
	if (up + down >= total / 4) {
		uint64_t scale = (up + down) / (total / 8);
		up /= scale;
		down /= scale;
	}

	 
	int s = highbit64(total);
	s = MIN(64 - s, 32);

	uint64_t ofrac = (1ULL << 32) - frac;

	if (frac >= 4 * ofrac)
		up /= frac / (2 * ofrac + 1);
	up = (up << s) / (total >> (32 - s));
	if (ofrac >= 4 * frac)
		down /= ofrac / (2 * frac + 1);
	down = (down << s) / (total >> (32 - s));
	down = down * 100 / balance;

	return (frac + up - down);
}

 
static uint64_t
arc_evict(void)
{
	uint64_t asize, bytes, total_evicted = 0;
	int64_t e, mrud, mrum, mfud, mfum, w;
	static uint64_t ogrd, ogrm, ogfd, ogfm;
	static uint64_t gsrd, gsrm, gsfd, gsfm;
	uint64_t ngrd, ngrm, ngfd, ngfm;

	 
	mrud = zfs_refcount_count(&arc_mru->arcs_size[ARC_BUFC_DATA]) +
	    zfs_refcount_count(&arc_anon->arcs_size[ARC_BUFC_DATA]);
	mrum = zfs_refcount_count(&arc_mru->arcs_size[ARC_BUFC_METADATA]) +
	    zfs_refcount_count(&arc_anon->arcs_size[ARC_BUFC_METADATA]);
	mfud = zfs_refcount_count(&arc_mfu->arcs_size[ARC_BUFC_DATA]);
	mfum = zfs_refcount_count(&arc_mfu->arcs_size[ARC_BUFC_METADATA]);
	uint64_t d = mrud + mfud;
	uint64_t m = mrum + mfum;
	uint64_t t = d + m;

	 
	ngrd = wmsum_value(&arc_mru_ghost->arcs_hits[ARC_BUFC_DATA]);
	uint64_t grd = ngrd - ogrd;
	ogrd = ngrd;
	ngrm = wmsum_value(&arc_mru_ghost->arcs_hits[ARC_BUFC_METADATA]);
	uint64_t grm = ngrm - ogrm;
	ogrm = ngrm;
	ngfd = wmsum_value(&arc_mfu_ghost->arcs_hits[ARC_BUFC_DATA]);
	uint64_t gfd = ngfd - ogfd;
	ogfd = ngfd;
	ngfm = wmsum_value(&arc_mfu_ghost->arcs_hits[ARC_BUFC_METADATA]);
	uint64_t gfm = ngfm - ogfm;
	ogfm = ngfm;

	 
	arc_meta = arc_evict_adj(arc_meta, gsrd + gsrm + gsfd + gsfm,
	    grm + gfm, grd + gfd, zfs_arc_meta_balance);
	arc_pd = arc_evict_adj(arc_pd, gsrd + gsfd, grd, gfd, 100);
	arc_pm = arc_evict_adj(arc_pm, gsrm + gsfm, grm, gfm, 100);

	asize = aggsum_value(&arc_sums.arcstat_size);
	int64_t wt = t - (asize - arc_c);

	 
	int64_t prune = 0;
	int64_t dn = wmsum_value(&arc_sums.arcstat_dnode_size);
	w = wt * (int64_t)(arc_meta >> 16) >> 16;
	if (zfs_refcount_count(&arc_mru->arcs_size[ARC_BUFC_METADATA]) +
	    zfs_refcount_count(&arc_mfu->arcs_size[ARC_BUFC_METADATA]) -
	    zfs_refcount_count(&arc_mru->arcs_esize[ARC_BUFC_METADATA]) -
	    zfs_refcount_count(&arc_mfu->arcs_esize[ARC_BUFC_METADATA]) >
	    w * 3 / 4) {
		prune = dn / sizeof (dnode_t) *
		    zfs_arc_dnode_reduce_percent / 100;
	} else if (dn > arc_dnode_limit) {
		prune = (dn - arc_dnode_limit) / sizeof (dnode_t) *
		    zfs_arc_dnode_reduce_percent / 100;
	}
	if (prune > 0)
		arc_prune_async(prune);

	 
	w = wt * (int64_t)(arc_meta * arc_pm >> 48) >> 16;
	e = MIN((int64_t)(asize - arc_c), (int64_t)(mrum - w));
	bytes = arc_evict_impl(arc_mru, ARC_BUFC_METADATA, e);
	total_evicted += bytes;
	mrum -= bytes;
	asize -= bytes;

	 
	w = wt * (int64_t)(arc_meta >> 16) >> 16;
	e = MIN((int64_t)(asize - arc_c), (int64_t)(m - w));
	bytes = arc_evict_impl(arc_mfu, ARC_BUFC_METADATA, e);
	total_evicted += bytes;
	mfum -= bytes;
	asize -= bytes;

	 
	wt -= m - total_evicted;
	w = wt * (int64_t)(arc_pd >> 16) >> 16;
	e = MIN((int64_t)(asize - arc_c), (int64_t)(mrud - w));
	bytes = arc_evict_impl(arc_mru, ARC_BUFC_DATA, e);
	total_evicted += bytes;
	mrud -= bytes;
	asize -= bytes;

	 
	e = asize - arc_c;
	bytes = arc_evict_impl(arc_mfu, ARC_BUFC_DATA, e);
	mfud -= bytes;
	total_evicted += bytes;

	 
	gsrd = (mrum + mfud + mfum) / 2;
	e = zfs_refcount_count(&arc_mru_ghost->arcs_size[ARC_BUFC_DATA]) -
	    gsrd;
	(void) arc_evict_impl(arc_mru_ghost, ARC_BUFC_DATA, e);

	gsrm = (mrud + mfud + mfum) / 2;
	e = zfs_refcount_count(&arc_mru_ghost->arcs_size[ARC_BUFC_METADATA]) -
	    gsrm;
	(void) arc_evict_impl(arc_mru_ghost, ARC_BUFC_METADATA, e);

	gsfd = (mrud + mrum + mfum) / 2;
	e = zfs_refcount_count(&arc_mfu_ghost->arcs_size[ARC_BUFC_DATA]) -
	    gsfd;
	(void) arc_evict_impl(arc_mfu_ghost, ARC_BUFC_DATA, e);

	gsfm = (mrud + mrum + mfud) / 2;
	e = zfs_refcount_count(&arc_mfu_ghost->arcs_size[ARC_BUFC_METADATA]) -
	    gsfm;
	(void) arc_evict_impl(arc_mfu_ghost, ARC_BUFC_METADATA, e);

	return (total_evicted);
}

void
arc_flush(spa_t *spa, boolean_t retry)
{
	uint64_t guid = 0;

	 
	ASSERT(!retry || spa == NULL);

	if (spa != NULL)
		guid = spa_load_guid(spa);

	(void) arc_flush_state(arc_mru, guid, ARC_BUFC_DATA, retry);
	(void) arc_flush_state(arc_mru, guid, ARC_BUFC_METADATA, retry);

	(void) arc_flush_state(arc_mfu, guid, ARC_BUFC_DATA, retry);
	(void) arc_flush_state(arc_mfu, guid, ARC_BUFC_METADATA, retry);

	(void) arc_flush_state(arc_mru_ghost, guid, ARC_BUFC_DATA, retry);
	(void) arc_flush_state(arc_mru_ghost, guid, ARC_BUFC_METADATA, retry);

	(void) arc_flush_state(arc_mfu_ghost, guid, ARC_BUFC_DATA, retry);
	(void) arc_flush_state(arc_mfu_ghost, guid, ARC_BUFC_METADATA, retry);

	(void) arc_flush_state(arc_uncached, guid, ARC_BUFC_DATA, retry);
	(void) arc_flush_state(arc_uncached, guid, ARC_BUFC_METADATA, retry);
}

void
arc_reduce_target_size(int64_t to_free)
{
	uint64_t c = arc_c;

	if (c <= arc_c_min)
		return;

	 
	uint64_t asize = aggsum_value(&arc_sums.arcstat_size);
	if (asize < c)
		to_free += c - asize;
	arc_c = MAX((int64_t)c - to_free, (int64_t)arc_c_min);

	 
	mutex_enter(&arc_evict_lock);
	arc_evict_needed = B_TRUE;
	mutex_exit(&arc_evict_lock);
	zthr_wakeup(arc_evict_zthr);
}

 
boolean_t
arc_reclaim_needed(void)
{
	return (arc_available_memory() < 0);
}

void
arc_kmem_reap_soon(void)
{
	size_t			i;
	kmem_cache_t		*prev_cache = NULL;
	kmem_cache_t		*prev_data_cache = NULL;

#ifdef _KERNEL
#if defined(_ILP32)
	 
	kmem_reap();
#endif
#endif

	for (i = 0; i < SPA_MAXBLOCKSIZE >> SPA_MINBLOCKSHIFT; i++) {
#if defined(_ILP32)
		 
		if (zio_buf_cache[i] == NULL)
			break;
#endif
		if (zio_buf_cache[i] != prev_cache) {
			prev_cache = zio_buf_cache[i];
			kmem_cache_reap_now(zio_buf_cache[i]);
		}
		if (zio_data_buf_cache[i] != prev_data_cache) {
			prev_data_cache = zio_data_buf_cache[i];
			kmem_cache_reap_now(zio_data_buf_cache[i]);
		}
	}
	kmem_cache_reap_now(buf_cache);
	kmem_cache_reap_now(hdr_full_cache);
	kmem_cache_reap_now(hdr_l2only_cache);
	kmem_cache_reap_now(zfs_btree_leaf_cache);
	abd_cache_reap_now();
}

static boolean_t
arc_evict_cb_check(void *arg, zthr_t *zthr)
{
	(void) arg, (void) zthr;

#ifdef ZFS_DEBUG
	 
	if (arc_ksp != NULL)
		arc_ksp->ks_update(arc_ksp, KSTAT_READ);
#endif

	 
	if (arc_evict_needed)
		return (B_TRUE);

	 
	return ((zfs_refcount_count(&arc_uncached->arcs_esize[ARC_BUFC_DATA]) +
	    zfs_refcount_count(&arc_uncached->arcs_esize[ARC_BUFC_METADATA]) &&
	    ddi_get_lbolt() - arc_last_uncached_flush >
	    MSEC_TO_TICK(arc_min_prefetch_ms / 2)));
}

 
static void
arc_evict_cb(void *arg, zthr_t *zthr)
{
	(void) arg, (void) zthr;

	uint64_t evicted = 0;
	fstrans_cookie_t cookie = spl_fstrans_mark();

	 
	arc_last_uncached_flush = ddi_get_lbolt();
	evicted += arc_flush_state(arc_uncached, 0, ARC_BUFC_DATA, B_FALSE);
	evicted += arc_flush_state(arc_uncached, 0, ARC_BUFC_METADATA, B_FALSE);

	 
	if (arc_evict_needed)
		evicted += arc_evict();

	 
	mutex_enter(&arc_evict_lock);
	arc_evict_needed = !zthr_iscancelled(arc_evict_zthr) &&
	    evicted > 0 && aggsum_compare(&arc_sums.arcstat_size, arc_c) > 0;
	if (!arc_evict_needed) {
		 
		arc_evict_waiter_t *aw;
		while ((aw = list_remove_head(&arc_evict_waiters)) != NULL) {
			cv_broadcast(&aw->aew_cv);
		}
		arc_set_need_free();
	}
	mutex_exit(&arc_evict_lock);
	spl_fstrans_unmark(cookie);
}

static boolean_t
arc_reap_cb_check(void *arg, zthr_t *zthr)
{
	(void) arg, (void) zthr;

	int64_t free_memory = arc_available_memory();
	static int reap_cb_check_counter = 0;

	 
	if (!kmem_cache_reap_active() && free_memory < 0) {

		arc_no_grow = B_TRUE;
		arc_warm = B_TRUE;
		 
		arc_growtime = gethrtime() + SEC2NSEC(arc_grow_retry);
		return (B_TRUE);
	} else if (free_memory < arc_c >> arc_no_grow_shift) {
		arc_no_grow = B_TRUE;
	} else if (gethrtime() >= arc_growtime) {
		arc_no_grow = B_FALSE;
	}

	 
	if (!((reap_cb_check_counter++) % 60))
		zfs_zstd_cache_reap_now();

	return (B_FALSE);
}

 
static void
arc_reap_cb(void *arg, zthr_t *zthr)
{
	(void) arg, (void) zthr;

	int64_t free_memory;
	fstrans_cookie_t cookie = spl_fstrans_mark();

	 
	arc_kmem_reap_soon();

	 
	delay((hz * arc_kmem_cache_reap_retry_ms + 999) / 1000);

	 
	free_memory = arc_available_memory();

	int64_t can_free = arc_c - arc_c_min;
	if (can_free > 0) {
		int64_t to_free = (can_free >> arc_shrink_shift) - free_memory;
		if (to_free > 0)
			arc_reduce_target_size(to_free);
	}
	spl_fstrans_unmark(cookie);
}

#ifdef _KERNEL
 

#endif  

 
static void
arc_adapt(uint64_t bytes)
{
	 
	if (arc_reclaim_needed()) {
		zthr_wakeup(arc_reap_zthr);
		return;
	}

	if (arc_no_grow)
		return;

	if (arc_c >= arc_c_max)
		return;

	 
	if (aggsum_upper_bound(&arc_sums.arcstat_size) +
	    2 * SPA_MAXBLOCKSIZE >= arc_c) {
		uint64_t dc = MAX(bytes, SPA_OLD_MAXBLOCKSIZE);
		if (atomic_add_64_nv(&arc_c, dc) > arc_c_max)
			arc_c = arc_c_max;
	}
}

 
static arc_ovf_level_t
arc_is_overflowing(boolean_t use_reserve)
{
	 
	int64_t overflow = MAX(SPA_MAXBLOCKSIZE,
	    arc_c >> zfs_arc_overflow_shift);

	 
	int64_t over = aggsum_lower_bound(&arc_sums.arcstat_size) -
	    arc_c - overflow / 2;
	if (!use_reserve)
		overflow /= 2;
	return (over < 0 ? ARC_OVF_NONE :
	    over < overflow ? ARC_OVF_SOME : ARC_OVF_SEVERE);
}

static abd_t *
arc_get_data_abd(arc_buf_hdr_t *hdr, uint64_t size, const void *tag,
    int alloc_flags)
{
	arc_buf_contents_t type = arc_buf_type(hdr);

	arc_get_data_impl(hdr, size, tag, alloc_flags);
	if (alloc_flags & ARC_HDR_ALLOC_LINEAR)
		return (abd_alloc_linear(size, type == ARC_BUFC_METADATA));
	else
		return (abd_alloc(size, type == ARC_BUFC_METADATA));
}

static void *
arc_get_data_buf(arc_buf_hdr_t *hdr, uint64_t size, const void *tag)
{
	arc_buf_contents_t type = arc_buf_type(hdr);

	arc_get_data_impl(hdr, size, tag, 0);
	if (type == ARC_BUFC_METADATA) {
		return (zio_buf_alloc(size));
	} else {
		ASSERT(type == ARC_BUFC_DATA);
		return (zio_data_buf_alloc(size));
	}
}

 
void
arc_wait_for_eviction(uint64_t amount, boolean_t use_reserve)
{
	switch (arc_is_overflowing(use_reserve)) {
	case ARC_OVF_NONE:
		return;
	case ARC_OVF_SOME:
		 
		if (!arc_evict_needed) {
			arc_evict_needed = B_TRUE;
			zthr_wakeup(arc_evict_zthr);
		}
		return;
	case ARC_OVF_SEVERE:
	default:
	{
		arc_evict_waiter_t aw;
		list_link_init(&aw.aew_node);
		cv_init(&aw.aew_cv, NULL, CV_DEFAULT, NULL);

		uint64_t last_count = 0;
		mutex_enter(&arc_evict_lock);
		if (!list_is_empty(&arc_evict_waiters)) {
			arc_evict_waiter_t *last =
			    list_tail(&arc_evict_waiters);
			last_count = last->aew_count;
		} else if (!arc_evict_needed) {
			arc_evict_needed = B_TRUE;
			zthr_wakeup(arc_evict_zthr);
		}
		 
		aw.aew_count = MAX(last_count, arc_evict_count) + amount;

		list_insert_tail(&arc_evict_waiters, &aw);

		arc_set_need_free();

		DTRACE_PROBE3(arc__wait__for__eviction,
		    uint64_t, amount,
		    uint64_t, arc_evict_count,
		    uint64_t, aw.aew_count);

		 
		do {
			cv_wait(&aw.aew_cv, &arc_evict_lock);
		} while (list_link_active(&aw.aew_node));
		mutex_exit(&arc_evict_lock);

		cv_destroy(&aw.aew_cv);
	}
	}
}

 
static void
arc_get_data_impl(arc_buf_hdr_t *hdr, uint64_t size, const void *tag,
    int alloc_flags)
{
	arc_adapt(size);

	 
	arc_wait_for_eviction(size * zfs_arc_eviction_pct / 100,
	    alloc_flags & ARC_HDR_USE_RESERVE);

	arc_buf_contents_t type = arc_buf_type(hdr);
	if (type == ARC_BUFC_METADATA) {
		arc_space_consume(size, ARC_SPACE_META);
	} else {
		arc_space_consume(size, ARC_SPACE_DATA);
	}

	 
	arc_state_t *state = hdr->b_l1hdr.b_state;
	if (!GHOST_STATE(state)) {

		(void) zfs_refcount_add_many(&state->arcs_size[type], size,
		    tag);

		 
		if (multilist_link_active(&hdr->b_l1hdr.b_arc_node)) {
			ASSERT(zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt));
			(void) zfs_refcount_add_many(&state->arcs_esize[type],
			    size, tag);
		}
	}
}

static void
arc_free_data_abd(arc_buf_hdr_t *hdr, abd_t *abd, uint64_t size,
    const void *tag)
{
	arc_free_data_impl(hdr, size, tag);
	abd_free(abd);
}

static void
arc_free_data_buf(arc_buf_hdr_t *hdr, void *buf, uint64_t size, const void *tag)
{
	arc_buf_contents_t type = arc_buf_type(hdr);

	arc_free_data_impl(hdr, size, tag);
	if (type == ARC_BUFC_METADATA) {
		zio_buf_free(buf, size);
	} else {
		ASSERT(type == ARC_BUFC_DATA);
		zio_data_buf_free(buf, size);
	}
}

 
static void
arc_free_data_impl(arc_buf_hdr_t *hdr, uint64_t size, const void *tag)
{
	arc_state_t *state = hdr->b_l1hdr.b_state;
	arc_buf_contents_t type = arc_buf_type(hdr);

	 
	if (multilist_link_active(&hdr->b_l1hdr.b_arc_node)) {
		ASSERT(zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt));
		ASSERT(state != arc_anon && state != arc_l2c_only);

		(void) zfs_refcount_remove_many(&state->arcs_esize[type],
		    size, tag);
	}
	(void) zfs_refcount_remove_many(&state->arcs_size[type], size, tag);

	VERIFY3U(hdr->b_type, ==, type);
	if (type == ARC_BUFC_METADATA) {
		arc_space_return(size, ARC_SPACE_META);
	} else {
		ASSERT(type == ARC_BUFC_DATA);
		arc_space_return(size, ARC_SPACE_DATA);
	}
}

 
static void
arc_access(arc_buf_hdr_t *hdr, arc_flags_t arc_flags, boolean_t hit)
{
	ASSERT(MUTEX_HELD(HDR_LOCK(hdr)));
	ASSERT(HDR_HAS_L1HDR(hdr));

	 
	boolean_t was_prefetch = HDR_PREFETCH(hdr);
	boolean_t now_prefetch = arc_flags & ARC_FLAG_PREFETCH;
	if (was_prefetch != now_prefetch) {
		if (was_prefetch) {
			ARCSTAT_CONDSTAT(hit, demand_hit, demand_iohit,
			    HDR_PRESCIENT_PREFETCH(hdr), prescient, predictive,
			    prefetch);
		}
		if (HDR_HAS_L2HDR(hdr))
			l2arc_hdr_arcstats_decrement_state(hdr);
		if (was_prefetch) {
			arc_hdr_clear_flags(hdr,
			    ARC_FLAG_PREFETCH | ARC_FLAG_PRESCIENT_PREFETCH);
		} else {
			arc_hdr_set_flags(hdr, ARC_FLAG_PREFETCH);
		}
		if (HDR_HAS_L2HDR(hdr))
			l2arc_hdr_arcstats_increment_state(hdr);
	}
	if (now_prefetch) {
		if (arc_flags & ARC_FLAG_PRESCIENT_PREFETCH) {
			arc_hdr_set_flags(hdr, ARC_FLAG_PRESCIENT_PREFETCH);
			ARCSTAT_BUMP(arcstat_prescient_prefetch);
		} else {
			ARCSTAT_BUMP(arcstat_predictive_prefetch);
		}
	}
	if (arc_flags & ARC_FLAG_L2CACHE)
		arc_hdr_set_flags(hdr, ARC_FLAG_L2CACHE);

	clock_t now = ddi_get_lbolt();
	if (hdr->b_l1hdr.b_state == arc_anon) {
		arc_state_t	*new_state;
		 
		ASSERT0(hdr->b_l1hdr.b_arc_access);
		hdr->b_l1hdr.b_arc_access = now;
		if (HDR_UNCACHED(hdr)) {
			new_state = arc_uncached;
			DTRACE_PROBE1(new_state__uncached, arc_buf_hdr_t *,
			    hdr);
		} else {
			new_state = arc_mru;
			DTRACE_PROBE1(new_state__mru, arc_buf_hdr_t *, hdr);
		}
		arc_change_state(new_state, hdr);
	} else if (hdr->b_l1hdr.b_state == arc_mru) {
		 
		if (HDR_IO_IN_PROGRESS(hdr)) {
			hdr->b_l1hdr.b_arc_access = now;
			return;
		}
		hdr->b_l1hdr.b_mru_hits++;
		ARCSTAT_BUMP(arcstat_mru_hits);

		 
		if (was_prefetch) {
			hdr->b_l1hdr.b_arc_access = now;
			return;
		}

		 
		if (ddi_time_after(now, hdr->b_l1hdr.b_arc_access +
		    ARC_MINTIME)) {
			hdr->b_l1hdr.b_arc_access = now;
			DTRACE_PROBE1(new_state__mfu, arc_buf_hdr_t *, hdr);
			arc_change_state(arc_mfu, hdr);
		}
	} else if (hdr->b_l1hdr.b_state == arc_mru_ghost) {
		arc_state_t	*new_state;
		 
		hdr->b_l1hdr.b_mru_ghost_hits++;
		ARCSTAT_BUMP(arcstat_mru_ghost_hits);
		hdr->b_l1hdr.b_arc_access = now;
		wmsum_add(&arc_mru_ghost->arcs_hits[arc_buf_type(hdr)],
		    arc_hdr_size(hdr));
		if (was_prefetch) {
			new_state = arc_mru;
			DTRACE_PROBE1(new_state__mru, arc_buf_hdr_t *, hdr);
		} else {
			new_state = arc_mfu;
			DTRACE_PROBE1(new_state__mfu, arc_buf_hdr_t *, hdr);
		}
		arc_change_state(new_state, hdr);
	} else if (hdr->b_l1hdr.b_state == arc_mfu) {
		 
		if (!HDR_IO_IN_PROGRESS(hdr)) {
			hdr->b_l1hdr.b_mfu_hits++;
			ARCSTAT_BUMP(arcstat_mfu_hits);
		}
		hdr->b_l1hdr.b_arc_access = now;
	} else if (hdr->b_l1hdr.b_state == arc_mfu_ghost) {
		 
		hdr->b_l1hdr.b_mfu_ghost_hits++;
		ARCSTAT_BUMP(arcstat_mfu_ghost_hits);
		hdr->b_l1hdr.b_arc_access = now;
		wmsum_add(&arc_mfu_ghost->arcs_hits[arc_buf_type(hdr)],
		    arc_hdr_size(hdr));
		DTRACE_PROBE1(new_state__mfu, arc_buf_hdr_t *, hdr);
		arc_change_state(arc_mfu, hdr);
	} else if (hdr->b_l1hdr.b_state == arc_uncached) {
		 
		if (!HDR_IO_IN_PROGRESS(hdr))
			ARCSTAT_BUMP(arcstat_uncached_hits);
		hdr->b_l1hdr.b_arc_access = now;
	} else if (hdr->b_l1hdr.b_state == arc_l2c_only) {
		 
		hdr->b_l1hdr.b_arc_access = now;
		DTRACE_PROBE1(new_state__mru, arc_buf_hdr_t *, hdr);
		arc_change_state(arc_mru, hdr);
	} else {
		cmn_err(CE_PANIC, "invalid arc state 0x%p",
		    hdr->b_l1hdr.b_state);
	}
}

 
void
arc_buf_access(arc_buf_t *buf)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	 
	if (hdr->b_l1hdr.b_state == arc_anon || HDR_EMPTY(hdr))
		return;

	kmutex_t *hash_lock = HDR_LOCK(hdr);
	mutex_enter(hash_lock);

	if (hdr->b_l1hdr.b_state == arc_anon || HDR_EMPTY(hdr)) {
		mutex_exit(hash_lock);
		ARCSTAT_BUMP(arcstat_access_skip);
		return;
	}

	ASSERT(hdr->b_l1hdr.b_state == arc_mru ||
	    hdr->b_l1hdr.b_state == arc_mfu ||
	    hdr->b_l1hdr.b_state == arc_uncached);

	DTRACE_PROBE1(arc__hit, arc_buf_hdr_t *, hdr);
	arc_access(hdr, 0, B_TRUE);
	mutex_exit(hash_lock);

	ARCSTAT_BUMP(arcstat_hits);
	ARCSTAT_CONDSTAT(B_TRUE  , demand, prefetch,
	    !HDR_ISTYPE_METADATA(hdr), data, metadata, hits);
}

 
void
arc_bcopy_func(zio_t *zio, const zbookmark_phys_t *zb, const blkptr_t *bp,
    arc_buf_t *buf, void *arg)
{
	(void) zio, (void) zb, (void) bp;

	if (buf == NULL)
		return;

	memcpy(arg, buf->b_data, arc_buf_size(buf));
	arc_buf_destroy(buf, arg);
}

 
void
arc_getbuf_func(zio_t *zio, const zbookmark_phys_t *zb, const blkptr_t *bp,
    arc_buf_t *buf, void *arg)
{
	(void) zb, (void) bp;
	arc_buf_t **bufp = arg;

	if (buf == NULL) {
		ASSERT(zio == NULL || zio->io_error != 0);
		*bufp = NULL;
	} else {
		ASSERT(zio == NULL || zio->io_error == 0);
		*bufp = buf;
		ASSERT(buf->b_data != NULL);
	}
}

static void
arc_hdr_verify(arc_buf_hdr_t *hdr, blkptr_t *bp)
{
	if (BP_IS_HOLE(bp) || BP_IS_EMBEDDED(bp)) {
		ASSERT3U(HDR_GET_PSIZE(hdr), ==, 0);
		ASSERT3U(arc_hdr_get_compress(hdr), ==, ZIO_COMPRESS_OFF);
	} else {
		if (HDR_COMPRESSION_ENABLED(hdr)) {
			ASSERT3U(arc_hdr_get_compress(hdr), ==,
			    BP_GET_COMPRESS(bp));
		}
		ASSERT3U(HDR_GET_LSIZE(hdr), ==, BP_GET_LSIZE(bp));
		ASSERT3U(HDR_GET_PSIZE(hdr), ==, BP_GET_PSIZE(bp));
		ASSERT3U(!!HDR_PROTECTED(hdr), ==, BP_IS_PROTECTED(bp));
	}
}

static void
arc_read_done(zio_t *zio)
{
	blkptr_t 	*bp = zio->io_bp;
	arc_buf_hdr_t	*hdr = zio->io_private;
	kmutex_t	*hash_lock = NULL;
	arc_callback_t	*callback_list;
	arc_callback_t	*acb;

	 
	if (HDR_IN_HASH_TABLE(hdr)) {
		arc_buf_hdr_t *found;

		ASSERT3U(hdr->b_birth, ==, BP_PHYSICAL_BIRTH(zio->io_bp));
		ASSERT3U(hdr->b_dva.dva_word[0], ==,
		    BP_IDENTITY(zio->io_bp)->dva_word[0]);
		ASSERT3U(hdr->b_dva.dva_word[1], ==,
		    BP_IDENTITY(zio->io_bp)->dva_word[1]);

		found = buf_hash_find(hdr->b_spa, zio->io_bp, &hash_lock);

		ASSERT((found == hdr &&
		    DVA_EQUAL(&hdr->b_dva, BP_IDENTITY(zio->io_bp))) ||
		    (found == hdr && HDR_L2_READING(hdr)));
		ASSERT3P(hash_lock, !=, NULL);
	}

	if (BP_IS_PROTECTED(bp)) {
		hdr->b_crypt_hdr.b_ot = BP_GET_TYPE(bp);
		hdr->b_crypt_hdr.b_dsobj = zio->io_bookmark.zb_objset;
		zio_crypt_decode_params_bp(bp, hdr->b_crypt_hdr.b_salt,
		    hdr->b_crypt_hdr.b_iv);

		if (zio->io_error == 0) {
			if (BP_GET_TYPE(bp) == DMU_OT_INTENT_LOG) {
				void *tmpbuf;

				tmpbuf = abd_borrow_buf_copy(zio->io_abd,
				    sizeof (zil_chain_t));
				zio_crypt_decode_mac_zil(tmpbuf,
				    hdr->b_crypt_hdr.b_mac);
				abd_return_buf(zio->io_abd, tmpbuf,
				    sizeof (zil_chain_t));
			} else {
				zio_crypt_decode_mac_bp(bp,
				    hdr->b_crypt_hdr.b_mac);
			}
		}
	}

	if (zio->io_error == 0) {
		 
		if (BP_SHOULD_BYTESWAP(zio->io_bp)) {
			if (BP_GET_LEVEL(zio->io_bp) > 0) {
				hdr->b_l1hdr.b_byteswap = DMU_BSWAP_UINT64;
			} else {
				hdr->b_l1hdr.b_byteswap =
				    DMU_OT_BYTESWAP(BP_GET_TYPE(zio->io_bp));
			}
		} else {
			hdr->b_l1hdr.b_byteswap = DMU_BSWAP_NUMFUNCS;
		}
		if (!HDR_L2_READING(hdr)) {
			hdr->b_complevel = zio->io_prop.zp_complevel;
		}
	}

	arc_hdr_clear_flags(hdr, ARC_FLAG_L2_EVICTED);
	if (l2arc_noprefetch && HDR_PREFETCH(hdr))
		arc_hdr_clear_flags(hdr, ARC_FLAG_L2CACHE);

	callback_list = hdr->b_l1hdr.b_acb;
	ASSERT3P(callback_list, !=, NULL);
	hdr->b_l1hdr.b_acb = NULL;

	 
	int callback_cnt = 0;
	for (acb = callback_list; acb != NULL; acb = acb->acb_next) {

		 
		callback_list = acb;

		if (!acb->acb_done || acb->acb_nobuf)
			continue;

		callback_cnt++;

		if (zio->io_error != 0)
			continue;

		int error = arc_buf_alloc_impl(hdr, zio->io_spa,
		    &acb->acb_zb, acb->acb_private, acb->acb_encrypted,
		    acb->acb_compressed, acb->acb_noauth, B_TRUE,
		    &acb->acb_buf);

		 
		ASSERT((zio->io_flags & ZIO_FLAG_SPECULATIVE) ||
		    error != EACCES);

		 
		if (error == ECKSUM) {
			ASSERT(BP_IS_PROTECTED(bp));
			error = SET_ERROR(EIO);
			if ((zio->io_flags & ZIO_FLAG_SPECULATIVE) == 0) {
				spa_log_error(zio->io_spa, &acb->acb_zb,
				    &zio->io_bp->blk_birth);
				(void) zfs_ereport_post(
				    FM_EREPORT_ZFS_AUTHENTICATION,
				    zio->io_spa, NULL, &acb->acb_zb, zio, 0);
			}
		}

		if (error != 0) {
			 
			zio->io_error = error;
		}
	}

	 
	ASSERT(callback_cnt < 2 || hash_lock != NULL);

	if (zio->io_error == 0) {
		arc_hdr_verify(hdr, zio->io_bp);
	} else {
		arc_hdr_set_flags(hdr, ARC_FLAG_IO_ERROR);
		if (hdr->b_l1hdr.b_state != arc_anon)
			arc_change_state(arc_anon, hdr);
		if (HDR_IN_HASH_TABLE(hdr))
			buf_hash_remove(hdr);
	}

	arc_hdr_clear_flags(hdr, ARC_FLAG_IO_IN_PROGRESS);
	(void) remove_reference(hdr, hdr);

	if (hash_lock != NULL)
		mutex_exit(hash_lock);

	 
	while ((acb = callback_list) != NULL) {
		if (acb->acb_done != NULL) {
			if (zio->io_error != 0 && acb->acb_buf != NULL) {
				 
				arc_buf_destroy(acb->acb_buf,
				    acb->acb_private);
				acb->acb_buf = NULL;
			}
			acb->acb_done(zio, &zio->io_bookmark, zio->io_bp,
			    acb->acb_buf, acb->acb_private);
		}

		if (acb->acb_zio_dummy != NULL) {
			acb->acb_zio_dummy->io_error = zio->io_error;
			zio_nowait(acb->acb_zio_dummy);
		}

		callback_list = acb->acb_prev;
		if (acb->acb_wait) {
			mutex_enter(&acb->acb_wait_lock);
			acb->acb_wait_error = zio->io_error;
			acb->acb_wait = B_FALSE;
			cv_signal(&acb->acb_wait_cv);
			mutex_exit(&acb->acb_wait_lock);
			 
		} else {
			kmem_free(acb, sizeof (arc_callback_t));
		}
	}
}

 
int
arc_read(zio_t *pio, spa_t *spa, const blkptr_t *bp,
    arc_read_done_func_t *done, void *private, zio_priority_t priority,
    int zio_flags, arc_flags_t *arc_flags, const zbookmark_phys_t *zb)
{
	arc_buf_hdr_t *hdr = NULL;
	kmutex_t *hash_lock = NULL;
	zio_t *rzio;
	uint64_t guid = spa_load_guid(spa);
	boolean_t compressed_read = (zio_flags & ZIO_FLAG_RAW_COMPRESS) != 0;
	boolean_t encrypted_read = BP_IS_ENCRYPTED(bp) &&
	    (zio_flags & ZIO_FLAG_RAW_ENCRYPT) != 0;
	boolean_t noauth_read = BP_IS_AUTHENTICATED(bp) &&
	    (zio_flags & ZIO_FLAG_RAW_ENCRYPT) != 0;
	boolean_t embedded_bp = !!BP_IS_EMBEDDED(bp);
	boolean_t no_buf = *arc_flags & ARC_FLAG_NO_BUF;
	arc_buf_t *buf = NULL;
	int rc = 0;

	ASSERT(!embedded_bp ||
	    BPE_GET_ETYPE(bp) == BP_EMBEDDED_TYPE_DATA);
	ASSERT(!BP_IS_HOLE(bp));
	ASSERT(!BP_IS_REDACTED(bp));

	 
	fstrans_cookie_t cookie = spl_fstrans_mark();
top:
	 
	if (!zfs_blkptr_verify(spa, bp, (zio_flags & ZIO_FLAG_CONFIG_WRITER) ?
	    BLK_CONFIG_HELD : BLK_CONFIG_NEEDED, BLK_VERIFY_LOG)) {
		rc = SET_ERROR(ECKSUM);
		goto done;
	}

	if (!embedded_bp) {
		 
		hdr = buf_hash_find(guid, bp, &hash_lock);
	}

	 
	if (hdr != NULL && HDR_HAS_L1HDR(hdr) && (HDR_HAS_RABD(hdr) ||
	    (hdr->b_l1hdr.b_pabd != NULL && !encrypted_read))) {
		boolean_t is_data = !HDR_ISTYPE_METADATA(hdr);

		if (HDR_IO_IN_PROGRESS(hdr)) {
			if (*arc_flags & ARC_FLAG_CACHED_ONLY) {
				mutex_exit(hash_lock);
				ARCSTAT_BUMP(arcstat_cached_only_in_progress);
				rc = SET_ERROR(ENOENT);
				goto done;
			}

			zio_t *head_zio = hdr->b_l1hdr.b_acb->acb_zio_head;
			ASSERT3P(head_zio, !=, NULL);
			if ((hdr->b_flags & ARC_FLAG_PRIO_ASYNC_READ) &&
			    priority == ZIO_PRIORITY_SYNC_READ) {
				 
				zio_change_priority(head_zio, priority);
				DTRACE_PROBE1(arc__async__upgrade__sync,
				    arc_buf_hdr_t *, hdr);
				ARCSTAT_BUMP(arcstat_async_upgrade_sync);
			}

			DTRACE_PROBE1(arc__iohit, arc_buf_hdr_t *, hdr);
			arc_access(hdr, *arc_flags, B_FALSE);

			 
			arc_callback_t *acb = NULL;
			if (done || pio || *arc_flags & ARC_FLAG_WAIT) {
				acb = kmem_zalloc(sizeof (arc_callback_t),
				    KM_SLEEP);
				acb->acb_done = done;
				acb->acb_private = private;
				acb->acb_compressed = compressed_read;
				acb->acb_encrypted = encrypted_read;
				acb->acb_noauth = noauth_read;
				acb->acb_nobuf = no_buf;
				if (*arc_flags & ARC_FLAG_WAIT) {
					acb->acb_wait = B_TRUE;
					mutex_init(&acb->acb_wait_lock, NULL,
					    MUTEX_DEFAULT, NULL);
					cv_init(&acb->acb_wait_cv, NULL,
					    CV_DEFAULT, NULL);
				}
				acb->acb_zb = *zb;
				if (pio != NULL) {
					acb->acb_zio_dummy = zio_null(pio,
					    spa, NULL, NULL, NULL, zio_flags);
				}
				acb->acb_zio_head = head_zio;
				acb->acb_next = hdr->b_l1hdr.b_acb;
				hdr->b_l1hdr.b_acb->acb_prev = acb;
				hdr->b_l1hdr.b_acb = acb;
			}
			mutex_exit(hash_lock);

			ARCSTAT_BUMP(arcstat_iohits);
			ARCSTAT_CONDSTAT(!(*arc_flags & ARC_FLAG_PREFETCH),
			    demand, prefetch, is_data, data, metadata, iohits);

			if (*arc_flags & ARC_FLAG_WAIT) {
				mutex_enter(&acb->acb_wait_lock);
				while (acb->acb_wait) {
					cv_wait(&acb->acb_wait_cv,
					    &acb->acb_wait_lock);
				}
				rc = acb->acb_wait_error;
				mutex_exit(&acb->acb_wait_lock);
				mutex_destroy(&acb->acb_wait_lock);
				cv_destroy(&acb->acb_wait_cv);
				kmem_free(acb, sizeof (arc_callback_t));
			}
			goto out;
		}

		ASSERT(hdr->b_l1hdr.b_state == arc_mru ||
		    hdr->b_l1hdr.b_state == arc_mfu ||
		    hdr->b_l1hdr.b_state == arc_uncached);

		DTRACE_PROBE1(arc__hit, arc_buf_hdr_t *, hdr);
		arc_access(hdr, *arc_flags, B_TRUE);

		if (done && !no_buf) {
			ASSERT(!embedded_bp || !BP_IS_HOLE(bp));

			 
			rc = arc_buf_alloc_impl(hdr, spa, zb, private,
			    encrypted_read, compressed_read, noauth_read,
			    B_TRUE, &buf);
			if (rc == ECKSUM) {
				 
				rc = SET_ERROR(EIO);
				if ((zio_flags & ZIO_FLAG_SPECULATIVE) == 0) {
					spa_log_error(spa, zb, &hdr->b_birth);
					(void) zfs_ereport_post(
					    FM_EREPORT_ZFS_AUTHENTICATION,
					    spa, NULL, zb, NULL, 0);
				}
			}
			if (rc != 0) {
				arc_buf_destroy_impl(buf);
				buf = NULL;
				(void) remove_reference(hdr, private);
			}

			 
			ASSERT((zio_flags & ZIO_FLAG_SPECULATIVE) ||
			    rc != EACCES);
		}
		mutex_exit(hash_lock);
		ARCSTAT_BUMP(arcstat_hits);
		ARCSTAT_CONDSTAT(!(*arc_flags & ARC_FLAG_PREFETCH),
		    demand, prefetch, is_data, data, metadata, hits);
		*arc_flags |= ARC_FLAG_CACHED;
		goto done;
	} else {
		uint64_t lsize = BP_GET_LSIZE(bp);
		uint64_t psize = BP_GET_PSIZE(bp);
		arc_callback_t *acb;
		vdev_t *vd = NULL;
		uint64_t addr = 0;
		boolean_t devw = B_FALSE;
		uint64_t size;
		abd_t *hdr_abd;
		int alloc_flags = encrypted_read ? ARC_HDR_ALLOC_RDATA : 0;
		arc_buf_contents_t type = BP_GET_BUFC_TYPE(bp);

		if (*arc_flags & ARC_FLAG_CACHED_ONLY) {
			if (hash_lock != NULL)
				mutex_exit(hash_lock);
			rc = SET_ERROR(ENOENT);
			goto done;
		}

		if (hdr == NULL) {
			 
			arc_buf_hdr_t *exists = NULL;
			hdr = arc_hdr_alloc(spa_load_guid(spa), psize, lsize,
			    BP_IS_PROTECTED(bp), BP_GET_COMPRESS(bp), 0, type);

			if (!embedded_bp) {
				hdr->b_dva = *BP_IDENTITY(bp);
				hdr->b_birth = BP_PHYSICAL_BIRTH(bp);
				exists = buf_hash_insert(hdr, &hash_lock);
			}
			if (exists != NULL) {
				 
				mutex_exit(hash_lock);
				buf_discard_identity(hdr);
				arc_hdr_destroy(hdr);
				goto top;  
			}
		} else {
			 
			if (!HDR_HAS_L1HDR(hdr)) {
				hdr = arc_hdr_realloc(hdr, hdr_l2only_cache,
				    hdr_full_cache);
			}

			if (GHOST_STATE(hdr->b_l1hdr.b_state)) {
				ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
				ASSERT(!HDR_HAS_RABD(hdr));
				ASSERT(!HDR_IO_IN_PROGRESS(hdr));
				ASSERT0(zfs_refcount_count(
				    &hdr->b_l1hdr.b_refcnt));
				ASSERT3P(hdr->b_l1hdr.b_buf, ==, NULL);
#ifdef ZFS_DEBUG
				ASSERT3P(hdr->b_l1hdr.b_freeze_cksum, ==, NULL);
#endif
			} else if (HDR_IO_IN_PROGRESS(hdr)) {
				 
				arc_callback_t *acb = kmem_zalloc(
				    sizeof (arc_callback_t), KM_SLEEP);
				acb->acb_wait = B_TRUE;
				mutex_init(&acb->acb_wait_lock, NULL,
				    MUTEX_DEFAULT, NULL);
				cv_init(&acb->acb_wait_cv, NULL, CV_DEFAULT,
				    NULL);
				acb->acb_zio_head =
				    hdr->b_l1hdr.b_acb->acb_zio_head;
				acb->acb_next = hdr->b_l1hdr.b_acb;
				hdr->b_l1hdr.b_acb->acb_prev = acb;
				hdr->b_l1hdr.b_acb = acb;
				mutex_exit(hash_lock);
				mutex_enter(&acb->acb_wait_lock);
				while (acb->acb_wait) {
					cv_wait(&acb->acb_wait_cv,
					    &acb->acb_wait_lock);
				}
				mutex_exit(&acb->acb_wait_lock);
				mutex_destroy(&acb->acb_wait_lock);
				cv_destroy(&acb->acb_wait_cv);
				kmem_free(acb, sizeof (arc_callback_t));
				goto top;
			}
		}
		if (*arc_flags & ARC_FLAG_UNCACHED) {
			arc_hdr_set_flags(hdr, ARC_FLAG_UNCACHED);
			if (!encrypted_read)
				alloc_flags |= ARC_HDR_ALLOC_LINEAR;
		}

		 
		add_reference(hdr, hdr);
		if (!embedded_bp)
			arc_access(hdr, *arc_flags, B_FALSE);
		arc_hdr_set_flags(hdr, ARC_FLAG_IO_IN_PROGRESS);
		arc_hdr_alloc_abd(hdr, alloc_flags);
		if (encrypted_read) {
			ASSERT(HDR_HAS_RABD(hdr));
			size = HDR_GET_PSIZE(hdr);
			hdr_abd = hdr->b_crypt_hdr.b_rabd;
			zio_flags |= ZIO_FLAG_RAW;
		} else {
			ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);
			size = arc_hdr_size(hdr);
			hdr_abd = hdr->b_l1hdr.b_pabd;

			if (arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF) {
				zio_flags |= ZIO_FLAG_RAW_COMPRESS;
			}

			 
			if (BP_IS_AUTHENTICATED(bp))
				zio_flags |= ZIO_FLAG_RAW_ENCRYPT;
		}

		if (BP_IS_AUTHENTICATED(bp))
			arc_hdr_set_flags(hdr, ARC_FLAG_NOAUTH);
		if (BP_GET_LEVEL(bp) > 0)
			arc_hdr_set_flags(hdr, ARC_FLAG_INDIRECT);
		ASSERT(!GHOST_STATE(hdr->b_l1hdr.b_state));

		acb = kmem_zalloc(sizeof (arc_callback_t), KM_SLEEP);
		acb->acb_done = done;
		acb->acb_private = private;
		acb->acb_compressed = compressed_read;
		acb->acb_encrypted = encrypted_read;
		acb->acb_noauth = noauth_read;
		acb->acb_zb = *zb;

		ASSERT3P(hdr->b_l1hdr.b_acb, ==, NULL);
		hdr->b_l1hdr.b_acb = acb;

		if (HDR_HAS_L2HDR(hdr) &&
		    (vd = hdr->b_l2hdr.b_dev->l2ad_vdev) != NULL) {
			devw = hdr->b_l2hdr.b_dev->l2ad_writing;
			addr = hdr->b_l2hdr.b_daddr;
			 
			if (vdev_is_dead(vd) ||
			    !spa_config_tryenter(spa, SCL_L2ARC, vd, RW_READER))
				vd = NULL;
		}

		 
		if (priority == ZIO_PRIORITY_ASYNC_READ ||
		    priority == ZIO_PRIORITY_SCRUB)
			arc_hdr_set_flags(hdr, ARC_FLAG_PRIO_ASYNC_READ);
		else
			arc_hdr_clear_flags(hdr, ARC_FLAG_PRIO_ASYNC_READ);

		 
		ASSERT3U(HDR_GET_LSIZE(hdr), ==, lsize);

		 
		if (!embedded_bp) {
			DTRACE_PROBE4(arc__miss, arc_buf_hdr_t *, hdr,
			    blkptr_t *, bp, uint64_t, lsize,
			    zbookmark_phys_t *, zb);
			ARCSTAT_BUMP(arcstat_misses);
			ARCSTAT_CONDSTAT(!(*arc_flags & ARC_FLAG_PREFETCH),
			    demand, prefetch, !HDR_ISTYPE_METADATA(hdr), data,
			    metadata, misses);
			zfs_racct_read(size, 1);
		}

		 
		const boolean_t spa_has_l2 = l2arc_ndev != 0 &&
		    spa->spa_l2cache.sav_count > 0;

		if (vd != NULL && spa_has_l2 && !(l2arc_norw && devw)) {
			 
			if (HDR_HAS_L2HDR(hdr) &&
			    !HDR_L2_WRITING(hdr) && !HDR_L2_EVICTED(hdr)) {
				l2arc_read_callback_t *cb;
				abd_t *abd;
				uint64_t asize;

				DTRACE_PROBE1(l2arc__hit, arc_buf_hdr_t *, hdr);
				ARCSTAT_BUMP(arcstat_l2_hits);
				hdr->b_l2hdr.b_hits++;

				cb = kmem_zalloc(sizeof (l2arc_read_callback_t),
				    KM_SLEEP);
				cb->l2rcb_hdr = hdr;
				cb->l2rcb_bp = *bp;
				cb->l2rcb_zb = *zb;
				cb->l2rcb_flags = zio_flags;

				 
				if (HDR_GET_COMPRESS(hdr) != ZIO_COMPRESS_OFF &&
				    !HDR_COMPRESSION_ENABLED(hdr) &&
				    HDR_GET_PSIZE(hdr) != 0) {
					size = HDR_GET_PSIZE(hdr);
				}

				asize = vdev_psize_to_asize(vd, size);
				if (asize != size) {
					abd = abd_alloc_for_io(asize,
					    HDR_ISTYPE_METADATA(hdr));
					cb->l2rcb_abd = abd;
				} else {
					abd = hdr_abd;
				}

				ASSERT(addr >= VDEV_LABEL_START_SIZE &&
				    addr + asize <= vd->vdev_psize -
				    VDEV_LABEL_END_SIZE);

				 
				ASSERT3U(arc_hdr_get_compress(hdr), !=,
				    ZIO_COMPRESS_EMPTY);
				rzio = zio_read_phys(pio, vd, addr,
				    asize, abd,
				    ZIO_CHECKSUM_OFF,
				    l2arc_read_done, cb, priority,
				    zio_flags | ZIO_FLAG_CANFAIL |
				    ZIO_FLAG_DONT_PROPAGATE |
				    ZIO_FLAG_DONT_RETRY, B_FALSE);
				acb->acb_zio_head = rzio;

				if (hash_lock != NULL)
					mutex_exit(hash_lock);

				DTRACE_PROBE2(l2arc__read, vdev_t *, vd,
				    zio_t *, rzio);
				ARCSTAT_INCR(arcstat_l2_read_bytes,
				    HDR_GET_PSIZE(hdr));

				if (*arc_flags & ARC_FLAG_NOWAIT) {
					zio_nowait(rzio);
					goto out;
				}

				ASSERT(*arc_flags & ARC_FLAG_WAIT);
				if (zio_wait(rzio) == 0)
					goto out;

				 
				if (hash_lock != NULL)
					mutex_enter(hash_lock);
			} else {
				DTRACE_PROBE1(l2arc__miss,
				    arc_buf_hdr_t *, hdr);
				ARCSTAT_BUMP(arcstat_l2_misses);
				if (HDR_L2_WRITING(hdr))
					ARCSTAT_BUMP(arcstat_l2_rw_clash);
				spa_config_exit(spa, SCL_L2ARC, vd);
			}
		} else {
			if (vd != NULL)
				spa_config_exit(spa, SCL_L2ARC, vd);

			 
			if (spa_has_l2) {
				 
				if (!embedded_bp) {
					DTRACE_PROBE1(l2arc__miss,
					    arc_buf_hdr_t *, hdr);
					ARCSTAT_BUMP(arcstat_l2_misses);
				}
			}
		}

		rzio = zio_read(pio, spa, bp, hdr_abd, size,
		    arc_read_done, hdr, priority, zio_flags, zb);
		acb->acb_zio_head = rzio;

		if (hash_lock != NULL)
			mutex_exit(hash_lock);

		if (*arc_flags & ARC_FLAG_WAIT) {
			rc = zio_wait(rzio);
			goto out;
		}

		ASSERT(*arc_flags & ARC_FLAG_NOWAIT);
		zio_nowait(rzio);
	}

out:
	 
	if (!embedded_bp)
		spa_read_history_add(spa, zb, *arc_flags);
	spl_fstrans_unmark(cookie);
	return (rc);

done:
	if (done)
		done(NULL, zb, bp, buf, private);
	if (pio && rc != 0) {
		zio_t *zio = zio_null(pio, spa, NULL, NULL, NULL, zio_flags);
		zio->io_error = rc;
		zio_nowait(zio);
	}
	goto out;
}

arc_prune_t *
arc_add_prune_callback(arc_prune_func_t *func, void *private)
{
	arc_prune_t *p;

	p = kmem_alloc(sizeof (*p), KM_SLEEP);
	p->p_pfunc = func;
	p->p_private = private;
	list_link_init(&p->p_node);
	zfs_refcount_create(&p->p_refcnt);

	mutex_enter(&arc_prune_mtx);
	zfs_refcount_add(&p->p_refcnt, &arc_prune_list);
	list_insert_head(&arc_prune_list, p);
	mutex_exit(&arc_prune_mtx);

	return (p);
}

void
arc_remove_prune_callback(arc_prune_t *p)
{
	boolean_t wait = B_FALSE;
	mutex_enter(&arc_prune_mtx);
	list_remove(&arc_prune_list, p);
	if (zfs_refcount_remove(&p->p_refcnt, &arc_prune_list) > 0)
		wait = B_TRUE;
	mutex_exit(&arc_prune_mtx);

	 
	if (wait)
		taskq_wait_outstanding(arc_prune_taskq, 0);
	ASSERT0(zfs_refcount_count(&p->p_refcnt));
	zfs_refcount_destroy(&p->p_refcnt);
	kmem_free(p, sizeof (*p));
}

 
static void
arc_prune_task(void *ptr)
{
	arc_prune_t *ap = (arc_prune_t *)ptr;
	arc_prune_func_t *func = ap->p_pfunc;

	if (func != NULL)
		func(ap->p_adjust, ap->p_private);

	zfs_refcount_remove(&ap->p_refcnt, func);
}

 
static void
arc_prune_async(uint64_t adjust)
{
	arc_prune_t *ap;

	mutex_enter(&arc_prune_mtx);
	for (ap = list_head(&arc_prune_list); ap != NULL;
	    ap = list_next(&arc_prune_list, ap)) {

		if (zfs_refcount_count(&ap->p_refcnt) >= 2)
			continue;

		zfs_refcount_add(&ap->p_refcnt, ap->p_pfunc);
		ap->p_adjust = adjust;
		if (taskq_dispatch(arc_prune_taskq, arc_prune_task,
		    ap, TQ_SLEEP) == TASKQID_INVALID) {
			zfs_refcount_remove(&ap->p_refcnt, ap->p_pfunc);
			continue;
		}
		ARCSTAT_BUMP(arcstat_prune);
	}
	mutex_exit(&arc_prune_mtx);
}

 
void
arc_freed(spa_t *spa, const blkptr_t *bp)
{
	arc_buf_hdr_t *hdr;
	kmutex_t *hash_lock;
	uint64_t guid = spa_load_guid(spa);

	ASSERT(!BP_IS_EMBEDDED(bp));

	hdr = buf_hash_find(guid, bp, &hash_lock);
	if (hdr == NULL)
		return;

	 
	if (!HDR_HAS_L1HDR(hdr) ||
	    zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt)) {
		arc_change_state(arc_anon, hdr);
		arc_hdr_destroy(hdr);
		mutex_exit(hash_lock);
	} else {
		mutex_exit(hash_lock);
	}

}

 
void
arc_release(arc_buf_t *buf, const void *tag)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;

	 

	ASSERT(HDR_HAS_L1HDR(hdr));

	 
	if (hdr->b_l1hdr.b_state == arc_anon) {
		ASSERT(!HDR_IO_IN_PROGRESS(hdr));
		ASSERT(!HDR_IN_HASH_TABLE(hdr));
		ASSERT(!HDR_HAS_L2HDR(hdr));

		ASSERT3P(hdr->b_l1hdr.b_buf, ==, buf);
		ASSERT(ARC_BUF_LAST(buf));
		ASSERT3S(zfs_refcount_count(&hdr->b_l1hdr.b_refcnt), ==, 1);
		ASSERT(!multilist_link_active(&hdr->b_l1hdr.b_arc_node));

		hdr->b_l1hdr.b_arc_access = 0;

		 
		buf_discard_identity(hdr);
		arc_buf_thaw(buf);

		return;
	}

	kmutex_t *hash_lock = HDR_LOCK(hdr);
	mutex_enter(hash_lock);

	 
	arc_state_t *state = hdr->b_l1hdr.b_state;
	ASSERT3P(hash_lock, ==, HDR_LOCK(hdr));
	ASSERT3P(state, !=, arc_anon);

	 
	ASSERT3S(zfs_refcount_count(&hdr->b_l1hdr.b_refcnt), >, 0);

	if (HDR_HAS_L2HDR(hdr)) {
		mutex_enter(&hdr->b_l2hdr.b_dev->l2ad_mtx);

		 
		if (HDR_HAS_L2HDR(hdr))
			arc_hdr_l2hdr_destroy(hdr);

		mutex_exit(&hdr->b_l2hdr.b_dev->l2ad_mtx);
	}

	 
	if (hdr->b_l1hdr.b_buf != buf || !ARC_BUF_LAST(buf)) {
		arc_buf_hdr_t *nhdr;
		uint64_t spa = hdr->b_spa;
		uint64_t psize = HDR_GET_PSIZE(hdr);
		uint64_t lsize = HDR_GET_LSIZE(hdr);
		boolean_t protected = HDR_PROTECTED(hdr);
		enum zio_compress compress = arc_hdr_get_compress(hdr);
		arc_buf_contents_t type = arc_buf_type(hdr);
		VERIFY3U(hdr->b_type, ==, type);

		ASSERT(hdr->b_l1hdr.b_buf != buf || buf->b_next != NULL);
		VERIFY3S(remove_reference(hdr, tag), >, 0);

		if (ARC_BUF_SHARED(buf) && !ARC_BUF_COMPRESSED(buf)) {
			ASSERT3P(hdr->b_l1hdr.b_buf, !=, buf);
			ASSERT(ARC_BUF_LAST(buf));
		}

		 
		arc_buf_t *lastbuf = arc_buf_remove(hdr, buf);
		ASSERT3P(lastbuf, !=, NULL);

		 
		if (ARC_BUF_SHARED(buf)) {
			ASSERT3P(hdr->b_l1hdr.b_buf, !=, buf);
			ASSERT(!arc_buf_is_shared(lastbuf));

			 
			arc_unshare_buf(hdr, buf);

			 
			if (arc_can_share(hdr, lastbuf)) {
				arc_share_buf(hdr, lastbuf);
			} else {
				arc_hdr_alloc_abd(hdr, 0);
				abd_copy_from_buf(hdr->b_l1hdr.b_pabd,
				    buf->b_data, psize);
			}
			VERIFY3P(lastbuf->b_data, !=, NULL);
		} else if (HDR_SHARED_DATA(hdr)) {
			 
			ASSERT(arc_buf_is_shared(lastbuf) ||
			    arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF);
			ASSERT(!arc_buf_is_shared(buf));
		}

		ASSERT(hdr->b_l1hdr.b_pabd != NULL || HDR_HAS_RABD(hdr));
		ASSERT3P(state, !=, arc_l2c_only);

		(void) zfs_refcount_remove_many(&state->arcs_size[type],
		    arc_buf_size(buf), buf);

		if (zfs_refcount_is_zero(&hdr->b_l1hdr.b_refcnt)) {
			ASSERT3P(state, !=, arc_l2c_only);
			(void) zfs_refcount_remove_many(
			    &state->arcs_esize[type],
			    arc_buf_size(buf), buf);
		}

		arc_cksum_verify(buf);
		arc_buf_unwatch(buf);

		 
		if (!arc_hdr_has_uncompressed_buf(hdr))
			arc_cksum_free(hdr);

		mutex_exit(hash_lock);

		nhdr = arc_hdr_alloc(spa, psize, lsize, protected,
		    compress, hdr->b_complevel, type);
		ASSERT3P(nhdr->b_l1hdr.b_buf, ==, NULL);
		ASSERT0(zfs_refcount_count(&nhdr->b_l1hdr.b_refcnt));
		VERIFY3U(nhdr->b_type, ==, type);
		ASSERT(!HDR_SHARED_DATA(nhdr));

		nhdr->b_l1hdr.b_buf = buf;
		(void) zfs_refcount_add(&nhdr->b_l1hdr.b_refcnt, tag);
		buf->b_hdr = nhdr;

		(void) zfs_refcount_add_many(&arc_anon->arcs_size[type],
		    arc_buf_size(buf), buf);
	} else {
		ASSERT(zfs_refcount_count(&hdr->b_l1hdr.b_refcnt) == 1);
		 
		ASSERT(!multilist_link_active(&hdr->b_l1hdr.b_arc_node));
		ASSERT(!HDR_IO_IN_PROGRESS(hdr));
		hdr->b_l1hdr.b_mru_hits = 0;
		hdr->b_l1hdr.b_mru_ghost_hits = 0;
		hdr->b_l1hdr.b_mfu_hits = 0;
		hdr->b_l1hdr.b_mfu_ghost_hits = 0;
		arc_change_state(arc_anon, hdr);
		hdr->b_l1hdr.b_arc_access = 0;

		mutex_exit(hash_lock);
		buf_discard_identity(hdr);
		arc_buf_thaw(buf);
	}
}

int
arc_released(arc_buf_t *buf)
{
	return (buf->b_data != NULL &&
	    buf->b_hdr->b_l1hdr.b_state == arc_anon);
}

#ifdef ZFS_DEBUG
int
arc_referenced(arc_buf_t *buf)
{
	return (zfs_refcount_count(&buf->b_hdr->b_l1hdr.b_refcnt));
}
#endif

static void
arc_write_ready(zio_t *zio)
{
	arc_write_callback_t *callback = zio->io_private;
	arc_buf_t *buf = callback->awcb_buf;
	arc_buf_hdr_t *hdr = buf->b_hdr;
	blkptr_t *bp = zio->io_bp;
	uint64_t psize = BP_IS_HOLE(bp) ? 0 : BP_GET_PSIZE(bp);
	fstrans_cookie_t cookie = spl_fstrans_mark();

	ASSERT(HDR_HAS_L1HDR(hdr));
	ASSERT(!zfs_refcount_is_zero(&buf->b_hdr->b_l1hdr.b_refcnt));
	ASSERT3P(hdr->b_l1hdr.b_buf, !=, NULL);

	 
	if (zio->io_flags & ZIO_FLAG_REEXECUTED) {
		arc_cksum_free(hdr);
		arc_buf_unwatch(buf);
		if (hdr->b_l1hdr.b_pabd != NULL) {
			if (ARC_BUF_SHARED(buf)) {
				arc_unshare_buf(hdr, buf);
			} else {
				ASSERT(!arc_buf_is_shared(buf));
				arc_hdr_free_abd(hdr, B_FALSE);
			}
		}

		if (HDR_HAS_RABD(hdr))
			arc_hdr_free_abd(hdr, B_TRUE);
	}
	ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);
	ASSERT(!HDR_HAS_RABD(hdr));
	ASSERT(!HDR_SHARED_DATA(hdr));
	ASSERT(!arc_buf_is_shared(buf));

	callback->awcb_ready(zio, buf, callback->awcb_private);

	if (HDR_IO_IN_PROGRESS(hdr)) {
		ASSERT(zio->io_flags & ZIO_FLAG_REEXECUTED);
	} else {
		arc_hdr_set_flags(hdr, ARC_FLAG_IO_IN_PROGRESS);
		add_reference(hdr, hdr);  
	}

	if (BP_IS_PROTECTED(bp)) {
		 
		ASSERT3U(BP_GET_TYPE(bp), !=, DMU_OT_INTENT_LOG);

		if (BP_SHOULD_BYTESWAP(bp)) {
			if (BP_GET_LEVEL(bp) > 0) {
				hdr->b_l1hdr.b_byteswap = DMU_BSWAP_UINT64;
			} else {
				hdr->b_l1hdr.b_byteswap =
				    DMU_OT_BYTESWAP(BP_GET_TYPE(bp));
			}
		} else {
			hdr->b_l1hdr.b_byteswap = DMU_BSWAP_NUMFUNCS;
		}

		arc_hdr_set_flags(hdr, ARC_FLAG_PROTECTED);
		hdr->b_crypt_hdr.b_ot = BP_GET_TYPE(bp);
		hdr->b_crypt_hdr.b_dsobj = zio->io_bookmark.zb_objset;
		zio_crypt_decode_params_bp(bp, hdr->b_crypt_hdr.b_salt,
		    hdr->b_crypt_hdr.b_iv);
		zio_crypt_decode_mac_bp(bp, hdr->b_crypt_hdr.b_mac);
	} else {
		arc_hdr_clear_flags(hdr, ARC_FLAG_PROTECTED);
	}

	 
	if (BP_IS_AUTHENTICATED(bp) && ARC_BUF_ENCRYPTED(buf)) {
		arc_hdr_set_flags(hdr, ARC_FLAG_NOAUTH);
		buf->b_flags &= ~ARC_BUF_FLAG_ENCRYPTED;
		if (BP_GET_COMPRESS(bp) == ZIO_COMPRESS_OFF)
			buf->b_flags &= ~ARC_BUF_FLAG_COMPRESSED;
	} else if (BP_IS_HOLE(bp) && ARC_BUF_ENCRYPTED(buf)) {
		buf->b_flags &= ~ARC_BUF_FLAG_ENCRYPTED;
		buf->b_flags &= ~ARC_BUF_FLAG_COMPRESSED;
	}

	 
	arc_cksum_compute(buf);

	enum zio_compress compress;
	if (BP_IS_HOLE(bp) || BP_IS_EMBEDDED(bp)) {
		compress = ZIO_COMPRESS_OFF;
	} else {
		ASSERT3U(HDR_GET_LSIZE(hdr), ==, BP_GET_LSIZE(bp));
		compress = BP_GET_COMPRESS(bp);
	}
	HDR_SET_PSIZE(hdr, psize);
	arc_hdr_set_compress(hdr, compress);
	hdr->b_complevel = zio->io_prop.zp_complevel;

	if (zio->io_error != 0 || psize == 0)
		goto out;

	 
	if (ARC_BUF_ENCRYPTED(buf)) {
		ASSERT3U(psize, >, 0);
		ASSERT(ARC_BUF_COMPRESSED(buf));
		arc_hdr_alloc_abd(hdr, ARC_HDR_ALLOC_RDATA |
		    ARC_HDR_USE_RESERVE);
		abd_copy(hdr->b_crypt_hdr.b_rabd, zio->io_abd, psize);
	} else if (!(HDR_UNCACHED(hdr) ||
	    abd_size_alloc_linear(arc_buf_size(buf))) ||
	    !arc_can_share(hdr, buf)) {
		 
		if (BP_IS_ENCRYPTED(bp)) {
			ASSERT3U(psize, >, 0);
			arc_hdr_alloc_abd(hdr, ARC_HDR_ALLOC_RDATA |
			    ARC_HDR_USE_RESERVE);
			abd_copy(hdr->b_crypt_hdr.b_rabd, zio->io_abd, psize);
		} else if (arc_hdr_get_compress(hdr) != ZIO_COMPRESS_OFF &&
		    !ARC_BUF_COMPRESSED(buf)) {
			ASSERT3U(psize, >, 0);
			arc_hdr_alloc_abd(hdr, ARC_HDR_USE_RESERVE);
			abd_copy(hdr->b_l1hdr.b_pabd, zio->io_abd, psize);
		} else {
			ASSERT3U(zio->io_orig_size, ==, arc_hdr_size(hdr));
			arc_hdr_alloc_abd(hdr, ARC_HDR_USE_RESERVE);
			abd_copy_from_buf(hdr->b_l1hdr.b_pabd, buf->b_data,
			    arc_buf_size(buf));
		}
	} else {
		ASSERT3P(buf->b_data, ==, abd_to_buf(zio->io_orig_abd));
		ASSERT3U(zio->io_orig_size, ==, arc_buf_size(buf));
		ASSERT3P(hdr->b_l1hdr.b_buf, ==, buf);
		ASSERT(ARC_BUF_LAST(buf));

		arc_share_buf(hdr, buf);
	}

out:
	arc_hdr_verify(hdr, bp);
	spl_fstrans_unmark(cookie);
}

static void
arc_write_children_ready(zio_t *zio)
{
	arc_write_callback_t *callback = zio->io_private;
	arc_buf_t *buf = callback->awcb_buf;

	callback->awcb_children_ready(zio, buf, callback->awcb_private);
}

static void
arc_write_done(zio_t *zio)
{
	arc_write_callback_t *callback = zio->io_private;
	arc_buf_t *buf = callback->awcb_buf;
	arc_buf_hdr_t *hdr = buf->b_hdr;

	ASSERT3P(hdr->b_l1hdr.b_acb, ==, NULL);

	if (zio->io_error == 0) {
		arc_hdr_verify(hdr, zio->io_bp);

		if (BP_IS_HOLE(zio->io_bp) || BP_IS_EMBEDDED(zio->io_bp)) {
			buf_discard_identity(hdr);
		} else {
			hdr->b_dva = *BP_IDENTITY(zio->io_bp);
			hdr->b_birth = BP_PHYSICAL_BIRTH(zio->io_bp);
		}
	} else {
		ASSERT(HDR_EMPTY(hdr));
	}

	 
	if (!HDR_EMPTY(hdr)) {
		arc_buf_hdr_t *exists;
		kmutex_t *hash_lock;

		ASSERT3U(zio->io_error, ==, 0);

		arc_cksum_verify(buf);

		exists = buf_hash_insert(hdr, &hash_lock);
		if (exists != NULL) {
			 
			if (zio->io_flags & ZIO_FLAG_IO_REWRITE) {
				if (!BP_EQUAL(&zio->io_bp_orig, zio->io_bp))
					panic("bad overwrite, hdr=%p exists=%p",
					    (void *)hdr, (void *)exists);
				ASSERT(zfs_refcount_is_zero(
				    &exists->b_l1hdr.b_refcnt));
				arc_change_state(arc_anon, exists);
				arc_hdr_destroy(exists);
				mutex_exit(hash_lock);
				exists = buf_hash_insert(hdr, &hash_lock);
				ASSERT3P(exists, ==, NULL);
			} else if (zio->io_flags & ZIO_FLAG_NOPWRITE) {
				 
				ASSERT(zio->io_prop.zp_nopwrite);
				if (!BP_EQUAL(&zio->io_bp_orig, zio->io_bp))
					panic("bad nopwrite, hdr=%p exists=%p",
					    (void *)hdr, (void *)exists);
			} else {
				 
				ASSERT3P(hdr->b_l1hdr.b_buf, !=, NULL);
				ASSERT(ARC_BUF_LAST(hdr->b_l1hdr.b_buf));
				ASSERT(hdr->b_l1hdr.b_state == arc_anon);
				ASSERT(BP_GET_DEDUP(zio->io_bp));
				ASSERT(BP_GET_LEVEL(zio->io_bp) == 0);
			}
		}
		arc_hdr_clear_flags(hdr, ARC_FLAG_IO_IN_PROGRESS);
		VERIFY3S(remove_reference(hdr, hdr), >, 0);
		 
		if (exists == NULL && hdr->b_l1hdr.b_state == arc_anon)
			arc_access(hdr, 0, B_FALSE);
		mutex_exit(hash_lock);
	} else {
		arc_hdr_clear_flags(hdr, ARC_FLAG_IO_IN_PROGRESS);
		VERIFY3S(remove_reference(hdr, hdr), >, 0);
	}

	callback->awcb_done(zio, buf, callback->awcb_private);

	abd_free(zio->io_abd);
	kmem_free(callback, sizeof (arc_write_callback_t));
}

zio_t *
arc_write(zio_t *pio, spa_t *spa, uint64_t txg,
    blkptr_t *bp, arc_buf_t *buf, boolean_t uncached, boolean_t l2arc,
    const zio_prop_t *zp, arc_write_done_func_t *ready,
    arc_write_done_func_t *children_ready, arc_write_done_func_t *done,
    void *private, zio_priority_t priority, int zio_flags,
    const zbookmark_phys_t *zb)
{
	arc_buf_hdr_t *hdr = buf->b_hdr;
	arc_write_callback_t *callback;
	zio_t *zio;
	zio_prop_t localprop = *zp;

	ASSERT3P(ready, !=, NULL);
	ASSERT3P(done, !=, NULL);
	ASSERT(!HDR_IO_ERROR(hdr));
	ASSERT(!HDR_IO_IN_PROGRESS(hdr));
	ASSERT3P(hdr->b_l1hdr.b_acb, ==, NULL);
	ASSERT3P(hdr->b_l1hdr.b_buf, !=, NULL);
	if (uncached)
		arc_hdr_set_flags(hdr, ARC_FLAG_UNCACHED);
	else if (l2arc)
		arc_hdr_set_flags(hdr, ARC_FLAG_L2CACHE);

	if (ARC_BUF_ENCRYPTED(buf)) {
		ASSERT(ARC_BUF_COMPRESSED(buf));
		localprop.zp_encrypt = B_TRUE;
		localprop.zp_compress = HDR_GET_COMPRESS(hdr);
		localprop.zp_complevel = hdr->b_complevel;
		localprop.zp_byteorder =
		    (hdr->b_l1hdr.b_byteswap == DMU_BSWAP_NUMFUNCS) ?
		    ZFS_HOST_BYTEORDER : !ZFS_HOST_BYTEORDER;
		memcpy(localprop.zp_salt, hdr->b_crypt_hdr.b_salt,
		    ZIO_DATA_SALT_LEN);
		memcpy(localprop.zp_iv, hdr->b_crypt_hdr.b_iv,
		    ZIO_DATA_IV_LEN);
		memcpy(localprop.zp_mac, hdr->b_crypt_hdr.b_mac,
		    ZIO_DATA_MAC_LEN);
		if (DMU_OT_IS_ENCRYPTED(localprop.zp_type)) {
			localprop.zp_nopwrite = B_FALSE;
			localprop.zp_copies =
			    MIN(localprop.zp_copies, SPA_DVAS_PER_BP - 1);
		}
		zio_flags |= ZIO_FLAG_RAW;
	} else if (ARC_BUF_COMPRESSED(buf)) {
		ASSERT3U(HDR_GET_LSIZE(hdr), !=, arc_buf_size(buf));
		localprop.zp_compress = HDR_GET_COMPRESS(hdr);
		localprop.zp_complevel = hdr->b_complevel;
		zio_flags |= ZIO_FLAG_RAW_COMPRESS;
	}
	callback = kmem_zalloc(sizeof (arc_write_callback_t), KM_SLEEP);
	callback->awcb_ready = ready;
	callback->awcb_children_ready = children_ready;
	callback->awcb_done = done;
	callback->awcb_private = private;
	callback->awcb_buf = buf;

	 
	if (hdr->b_l1hdr.b_pabd != NULL) {
		 
		if (ARC_BUF_SHARED(buf)) {
			arc_unshare_buf(hdr, buf);
		} else {
			ASSERT(!arc_buf_is_shared(buf));
			arc_hdr_free_abd(hdr, B_FALSE);
		}
		VERIFY3P(buf->b_data, !=, NULL);
	}

	if (HDR_HAS_RABD(hdr))
		arc_hdr_free_abd(hdr, B_TRUE);

	if (!(zio_flags & ZIO_FLAG_RAW))
		arc_hdr_set_compress(hdr, ZIO_COMPRESS_OFF);

	ASSERT(!arc_buf_is_shared(buf));
	ASSERT3P(hdr->b_l1hdr.b_pabd, ==, NULL);

	zio = zio_write(pio, spa, txg, bp,
	    abd_get_from_buf(buf->b_data, HDR_GET_LSIZE(hdr)),
	    HDR_GET_LSIZE(hdr), arc_buf_size(buf), &localprop, arc_write_ready,
	    (children_ready != NULL) ? arc_write_children_ready : NULL,
	    arc_write_done, callback, priority, zio_flags, zb);

	return (zio);
}

void
arc_tempreserve_clear(uint64_t reserve)
{
	atomic_add_64(&arc_tempreserve, -reserve);
	ASSERT((int64_t)arc_tempreserve >= 0);
}

int
arc_tempreserve_space(spa_t *spa, uint64_t reserve, uint64_t txg)
{
	int error;
	uint64_t anon_size;

	if (!arc_no_grow &&
	    reserve > arc_c/4 &&
	    reserve * 4 > (2ULL << SPA_MAXBLOCKSHIFT))
		arc_c = MIN(arc_c_max, reserve * 4);

	 
	if (reserve > arc_c) {
		DMU_TX_STAT_BUMP(dmu_tx_memory_reserve);
		return (SET_ERROR(ERESTART));
	}

	 

	 
	ASSERT3S(atomic_add_64_nv(&arc_loaned_bytes, 0), >=, 0);

	anon_size = MAX((int64_t)
	    (zfs_refcount_count(&arc_anon->arcs_size[ARC_BUFC_DATA]) +
	    zfs_refcount_count(&arc_anon->arcs_size[ARC_BUFC_METADATA]) -
	    arc_loaned_bytes), 0);

	 
	error = arc_memory_throttle(spa, reserve, txg);
	if (error != 0)
		return (error);

	 
	uint64_t total_dirty = reserve + arc_tempreserve + anon_size;
	uint64_t spa_dirty_anon = spa_dirty_data(spa);
	uint64_t rarc_c = arc_warm ? arc_c : arc_c_max;
	if (total_dirty > rarc_c * zfs_arc_dirty_limit_percent / 100 &&
	    anon_size > rarc_c * zfs_arc_anon_limit_percent / 100 &&
	    spa_dirty_anon > anon_size * zfs_arc_pool_dirty_percent / 100) {
#ifdef ZFS_DEBUG
		uint64_t meta_esize = zfs_refcount_count(
		    &arc_anon->arcs_esize[ARC_BUFC_METADATA]);
		uint64_t data_esize =
		    zfs_refcount_count(&arc_anon->arcs_esize[ARC_BUFC_DATA]);
		dprintf("failing, arc_tempreserve=%lluK anon_meta=%lluK "
		    "anon_data=%lluK tempreserve=%lluK rarc_c=%lluK\n",
		    (u_longlong_t)arc_tempreserve >> 10,
		    (u_longlong_t)meta_esize >> 10,
		    (u_longlong_t)data_esize >> 10,
		    (u_longlong_t)reserve >> 10,
		    (u_longlong_t)rarc_c >> 10);
#endif
		DMU_TX_STAT_BUMP(dmu_tx_dirty_throttle);
		return (SET_ERROR(ERESTART));
	}
	atomic_add_64(&arc_tempreserve, reserve);
	return (0);
}

static void
arc_kstat_update_state(arc_state_t *state, kstat_named_t *size,
    kstat_named_t *data, kstat_named_t *metadata,
    kstat_named_t *evict_data, kstat_named_t *evict_metadata)
{
	data->value.ui64 =
	    zfs_refcount_count(&state->arcs_size[ARC_BUFC_DATA]);
	metadata->value.ui64 =
	    zfs_refcount_count(&state->arcs_size[ARC_BUFC_METADATA]);
	size->value.ui64 = data->value.ui64 + metadata->value.ui64;
	evict_data->value.ui64 =
	    zfs_refcount_count(&state->arcs_esize[ARC_BUFC_DATA]);
	evict_metadata->value.ui64 =
	    zfs_refcount_count(&state->arcs_esize[ARC_BUFC_METADATA]);
}

static int
arc_kstat_update(kstat_t *ksp, int rw)
{
	arc_stats_t *as = ksp->ks_data;

	if (rw == KSTAT_WRITE)
		return (SET_ERROR(EACCES));

	as->arcstat_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_hits);
	as->arcstat_iohits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_iohits);
	as->arcstat_misses.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_misses);
	as->arcstat_demand_data_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_data_hits);
	as->arcstat_demand_data_iohits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_data_iohits);
	as->arcstat_demand_data_misses.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_data_misses);
	as->arcstat_demand_metadata_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_metadata_hits);
	as->arcstat_demand_metadata_iohits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_metadata_iohits);
	as->arcstat_demand_metadata_misses.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_metadata_misses);
	as->arcstat_prefetch_data_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prefetch_data_hits);
	as->arcstat_prefetch_data_iohits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prefetch_data_iohits);
	as->arcstat_prefetch_data_misses.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prefetch_data_misses);
	as->arcstat_prefetch_metadata_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prefetch_metadata_hits);
	as->arcstat_prefetch_metadata_iohits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prefetch_metadata_iohits);
	as->arcstat_prefetch_metadata_misses.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prefetch_metadata_misses);
	as->arcstat_mru_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_mru_hits);
	as->arcstat_mru_ghost_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_mru_ghost_hits);
	as->arcstat_mfu_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_mfu_hits);
	as->arcstat_mfu_ghost_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_mfu_ghost_hits);
	as->arcstat_uncached_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_uncached_hits);
	as->arcstat_deleted.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_deleted);
	as->arcstat_mutex_miss.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_mutex_miss);
	as->arcstat_access_skip.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_access_skip);
	as->arcstat_evict_skip.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_skip);
	as->arcstat_evict_not_enough.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_not_enough);
	as->arcstat_evict_l2_cached.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_l2_cached);
	as->arcstat_evict_l2_eligible.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_l2_eligible);
	as->arcstat_evict_l2_eligible_mfu.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_l2_eligible_mfu);
	as->arcstat_evict_l2_eligible_mru.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_l2_eligible_mru);
	as->arcstat_evict_l2_ineligible.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_l2_ineligible);
	as->arcstat_evict_l2_skip.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_evict_l2_skip);
	as->arcstat_hash_collisions.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_hash_collisions);
	as->arcstat_hash_chains.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_hash_chains);
	as->arcstat_size.value.ui64 =
	    aggsum_value(&arc_sums.arcstat_size);
	as->arcstat_compressed_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_compressed_size);
	as->arcstat_uncompressed_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_uncompressed_size);
	as->arcstat_overhead_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_overhead_size);
	as->arcstat_hdr_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_hdr_size);
	as->arcstat_data_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_data_size);
	as->arcstat_metadata_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_metadata_size);
	as->arcstat_dbuf_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_dbuf_size);
#if defined(COMPAT_FREEBSD11)
	as->arcstat_other_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_bonus_size) +
	    wmsum_value(&arc_sums.arcstat_dnode_size) +
	    wmsum_value(&arc_sums.arcstat_dbuf_size);
#endif

	arc_kstat_update_state(arc_anon,
	    &as->arcstat_anon_size,
	    &as->arcstat_anon_data,
	    &as->arcstat_anon_metadata,
	    &as->arcstat_anon_evictable_data,
	    &as->arcstat_anon_evictable_metadata);
	arc_kstat_update_state(arc_mru,
	    &as->arcstat_mru_size,
	    &as->arcstat_mru_data,
	    &as->arcstat_mru_metadata,
	    &as->arcstat_mru_evictable_data,
	    &as->arcstat_mru_evictable_metadata);
	arc_kstat_update_state(arc_mru_ghost,
	    &as->arcstat_mru_ghost_size,
	    &as->arcstat_mru_ghost_data,
	    &as->arcstat_mru_ghost_metadata,
	    &as->arcstat_mru_ghost_evictable_data,
	    &as->arcstat_mru_ghost_evictable_metadata);
	arc_kstat_update_state(arc_mfu,
	    &as->arcstat_mfu_size,
	    &as->arcstat_mfu_data,
	    &as->arcstat_mfu_metadata,
	    &as->arcstat_mfu_evictable_data,
	    &as->arcstat_mfu_evictable_metadata);
	arc_kstat_update_state(arc_mfu_ghost,
	    &as->arcstat_mfu_ghost_size,
	    &as->arcstat_mfu_ghost_data,
	    &as->arcstat_mfu_ghost_metadata,
	    &as->arcstat_mfu_ghost_evictable_data,
	    &as->arcstat_mfu_ghost_evictable_metadata);
	arc_kstat_update_state(arc_uncached,
	    &as->arcstat_uncached_size,
	    &as->arcstat_uncached_data,
	    &as->arcstat_uncached_metadata,
	    &as->arcstat_uncached_evictable_data,
	    &as->arcstat_uncached_evictable_metadata);

	as->arcstat_dnode_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_dnode_size);
	as->arcstat_bonus_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_bonus_size);
	as->arcstat_l2_hits.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_hits);
	as->arcstat_l2_misses.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_misses);
	as->arcstat_l2_prefetch_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_prefetch_asize);
	as->arcstat_l2_mru_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_mru_asize);
	as->arcstat_l2_mfu_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_mfu_asize);
	as->arcstat_l2_bufc_data_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_bufc_data_asize);
	as->arcstat_l2_bufc_metadata_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_bufc_metadata_asize);
	as->arcstat_l2_feeds.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_feeds);
	as->arcstat_l2_rw_clash.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rw_clash);
	as->arcstat_l2_read_bytes.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_read_bytes);
	as->arcstat_l2_write_bytes.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_write_bytes);
	as->arcstat_l2_writes_sent.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_writes_sent);
	as->arcstat_l2_writes_done.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_writes_done);
	as->arcstat_l2_writes_error.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_writes_error);
	as->arcstat_l2_writes_lock_retry.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_writes_lock_retry);
	as->arcstat_l2_evict_lock_retry.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_evict_lock_retry);
	as->arcstat_l2_evict_reading.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_evict_reading);
	as->arcstat_l2_evict_l1cached.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_evict_l1cached);
	as->arcstat_l2_free_on_write.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_free_on_write);
	as->arcstat_l2_abort_lowmem.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_abort_lowmem);
	as->arcstat_l2_cksum_bad.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_cksum_bad);
	as->arcstat_l2_io_error.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_io_error);
	as->arcstat_l2_lsize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_lsize);
	as->arcstat_l2_psize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_psize);
	as->arcstat_l2_hdr_size.value.ui64 =
	    aggsum_value(&arc_sums.arcstat_l2_hdr_size);
	as->arcstat_l2_log_blk_writes.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_log_blk_writes);
	as->arcstat_l2_log_blk_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_log_blk_asize);
	as->arcstat_l2_log_blk_count.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_log_blk_count);
	as->arcstat_l2_rebuild_success.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_success);
	as->arcstat_l2_rebuild_abort_unsupported.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_abort_unsupported);
	as->arcstat_l2_rebuild_abort_io_errors.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_abort_io_errors);
	as->arcstat_l2_rebuild_abort_dh_errors.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_abort_dh_errors);
	as->arcstat_l2_rebuild_abort_cksum_lb_errors.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_abort_cksum_lb_errors);
	as->arcstat_l2_rebuild_abort_lowmem.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_abort_lowmem);
	as->arcstat_l2_rebuild_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_size);
	as->arcstat_l2_rebuild_asize.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_asize);
	as->arcstat_l2_rebuild_bufs.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_bufs);
	as->arcstat_l2_rebuild_bufs_precached.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_bufs_precached);
	as->arcstat_l2_rebuild_log_blks.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_l2_rebuild_log_blks);
	as->arcstat_memory_throttle_count.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_memory_throttle_count);
	as->arcstat_memory_direct_count.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_memory_direct_count);
	as->arcstat_memory_indirect_count.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_memory_indirect_count);

	as->arcstat_memory_all_bytes.value.ui64 =
	    arc_all_memory();
	as->arcstat_memory_free_bytes.value.ui64 =
	    arc_free_memory();
	as->arcstat_memory_available_bytes.value.i64 =
	    arc_available_memory();

	as->arcstat_prune.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prune);
	as->arcstat_meta_used.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_meta_used);
	as->arcstat_async_upgrade_sync.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_async_upgrade_sync);
	as->arcstat_predictive_prefetch.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_predictive_prefetch);
	as->arcstat_demand_hit_predictive_prefetch.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_hit_predictive_prefetch);
	as->arcstat_demand_iohit_predictive_prefetch.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_iohit_predictive_prefetch);
	as->arcstat_prescient_prefetch.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_prescient_prefetch);
	as->arcstat_demand_hit_prescient_prefetch.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_hit_prescient_prefetch);
	as->arcstat_demand_iohit_prescient_prefetch.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_demand_iohit_prescient_prefetch);
	as->arcstat_raw_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_raw_size);
	as->arcstat_cached_only_in_progress.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_cached_only_in_progress);
	as->arcstat_abd_chunk_waste_size.value.ui64 =
	    wmsum_value(&arc_sums.arcstat_abd_chunk_waste_size);

	return (0);
}

 
static unsigned int
arc_state_multilist_index_func(multilist_t *ml, void *obj)
{
	arc_buf_hdr_t *hdr = obj;

	 
	ASSERT(!HDR_EMPTY(hdr));

	 
	return ((unsigned int)buf_hash(hdr->b_spa, &hdr->b_dva, hdr->b_birth) %
	    multilist_get_num_sublists(ml));
}

static unsigned int
arc_state_l2c_multilist_index_func(multilist_t *ml, void *obj)
{
	panic("Header %p insert into arc_l2c_only %p", obj, ml);
}

#define	WARN_IF_TUNING_IGNORED(tuning, value, do_warn) do {	\
	if ((do_warn) && (tuning) && ((tuning) != (value))) {	\
		cmn_err(CE_WARN,				\
		    "ignoring tunable %s (using %llu instead)",	\
		    (#tuning), (u_longlong_t)(value));	\
	}							\
} while (0)

 
void
arc_tuning_update(boolean_t verbose)
{
	uint64_t allmem = arc_all_memory();

	 
	if ((zfs_arc_min) && (zfs_arc_min != arc_c_min) &&
	    (zfs_arc_min >= 2ULL << SPA_MAXBLOCKSHIFT) &&
	    (zfs_arc_min <= arc_c_max)) {
		arc_c_min = zfs_arc_min;
		arc_c = MAX(arc_c, arc_c_min);
	}
	WARN_IF_TUNING_IGNORED(zfs_arc_min, arc_c_min, verbose);

	 
	if ((zfs_arc_max) && (zfs_arc_max != arc_c_max) &&
	    (zfs_arc_max >= MIN_ARC_MAX) && (zfs_arc_max < allmem) &&
	    (zfs_arc_max > arc_c_min)) {
		arc_c_max = zfs_arc_max;
		arc_c = MIN(arc_c, arc_c_max);
		if (arc_dnode_limit > arc_c_max)
			arc_dnode_limit = arc_c_max;
	}
	WARN_IF_TUNING_IGNORED(zfs_arc_max, arc_c_max, verbose);

	 
	arc_dnode_limit = zfs_arc_dnode_limit ? zfs_arc_dnode_limit :
	    MIN(zfs_arc_dnode_limit_percent, 100) * arc_c_max / 100;
	WARN_IF_TUNING_IGNORED(zfs_arc_dnode_limit, arc_dnode_limit, verbose);

	 
	if (zfs_arc_grow_retry)
		arc_grow_retry = zfs_arc_grow_retry;

	 
	if (zfs_arc_shrink_shift) {
		arc_shrink_shift = zfs_arc_shrink_shift;
		arc_no_grow_shift = MIN(arc_no_grow_shift, arc_shrink_shift -1);
	}

	 
	if (zfs_arc_min_prefetch_ms)
		arc_min_prefetch_ms = zfs_arc_min_prefetch_ms;

	 
	if (zfs_arc_min_prescient_prefetch_ms) {
		arc_min_prescient_prefetch_ms =
		    zfs_arc_min_prescient_prefetch_ms;
	}

	 
	if (zfs_arc_lotsfree_percent <= 100)
		arc_lotsfree_percent = zfs_arc_lotsfree_percent;
	WARN_IF_TUNING_IGNORED(zfs_arc_lotsfree_percent, arc_lotsfree_percent,
	    verbose);

	 
	if ((zfs_arc_sys_free) && (zfs_arc_sys_free != arc_sys_free))
		arc_sys_free = MIN(zfs_arc_sys_free, allmem);
	WARN_IF_TUNING_IGNORED(zfs_arc_sys_free, arc_sys_free, verbose);
}

static void
arc_state_multilist_init(multilist_t *ml,
    multilist_sublist_index_func_t *index_func, int *maxcountp)
{
	multilist_create(ml, sizeof (arc_buf_hdr_t),
	    offsetof(arc_buf_hdr_t, b_l1hdr.b_arc_node), index_func);
	*maxcountp = MAX(*maxcountp, multilist_get_num_sublists(ml));
}

static void
arc_state_init(void)
{
	int num_sublists = 0;

	arc_state_multilist_init(&arc_mru->arcs_list[ARC_BUFC_METADATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mru->arcs_list[ARC_BUFC_DATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mru_ghost->arcs_list[ARC_BUFC_METADATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mru_ghost->arcs_list[ARC_BUFC_DATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mfu->arcs_list[ARC_BUFC_METADATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mfu->arcs_list[ARC_BUFC_DATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mfu_ghost->arcs_list[ARC_BUFC_METADATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_mfu_ghost->arcs_list[ARC_BUFC_DATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_uncached->arcs_list[ARC_BUFC_METADATA],
	    arc_state_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_uncached->arcs_list[ARC_BUFC_DATA],
	    arc_state_multilist_index_func, &num_sublists);

	 
	arc_state_multilist_init(&arc_l2c_only->arcs_list[ARC_BUFC_METADATA],
	    arc_state_l2c_multilist_index_func, &num_sublists);
	arc_state_multilist_init(&arc_l2c_only->arcs_list[ARC_BUFC_DATA],
	    arc_state_l2c_multilist_index_func, &num_sublists);

	 
	arc_state_evict_marker_count = num_sublists;

	zfs_refcount_create(&arc_anon->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_anon->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mru->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mru->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mru_ghost->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mru_ghost->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mfu->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mfu->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mfu_ghost->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mfu_ghost->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_l2c_only->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_l2c_only->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_uncached->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_uncached->arcs_esize[ARC_BUFC_DATA]);

	zfs_refcount_create(&arc_anon->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_anon->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mru->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mru->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mru_ghost->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mru_ghost->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mfu->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mfu->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_mfu_ghost->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_mfu_ghost->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_l2c_only->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_l2c_only->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_create(&arc_uncached->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_create(&arc_uncached->arcs_size[ARC_BUFC_METADATA]);

	wmsum_init(&arc_mru_ghost->arcs_hits[ARC_BUFC_DATA], 0);
	wmsum_init(&arc_mru_ghost->arcs_hits[ARC_BUFC_METADATA], 0);
	wmsum_init(&arc_mfu_ghost->arcs_hits[ARC_BUFC_DATA], 0);
	wmsum_init(&arc_mfu_ghost->arcs_hits[ARC_BUFC_METADATA], 0);

	wmsum_init(&arc_sums.arcstat_hits, 0);
	wmsum_init(&arc_sums.arcstat_iohits, 0);
	wmsum_init(&arc_sums.arcstat_misses, 0);
	wmsum_init(&arc_sums.arcstat_demand_data_hits, 0);
	wmsum_init(&arc_sums.arcstat_demand_data_iohits, 0);
	wmsum_init(&arc_sums.arcstat_demand_data_misses, 0);
	wmsum_init(&arc_sums.arcstat_demand_metadata_hits, 0);
	wmsum_init(&arc_sums.arcstat_demand_metadata_iohits, 0);
	wmsum_init(&arc_sums.arcstat_demand_metadata_misses, 0);
	wmsum_init(&arc_sums.arcstat_prefetch_data_hits, 0);
	wmsum_init(&arc_sums.arcstat_prefetch_data_iohits, 0);
	wmsum_init(&arc_sums.arcstat_prefetch_data_misses, 0);
	wmsum_init(&arc_sums.arcstat_prefetch_metadata_hits, 0);
	wmsum_init(&arc_sums.arcstat_prefetch_metadata_iohits, 0);
	wmsum_init(&arc_sums.arcstat_prefetch_metadata_misses, 0);
	wmsum_init(&arc_sums.arcstat_mru_hits, 0);
	wmsum_init(&arc_sums.arcstat_mru_ghost_hits, 0);
	wmsum_init(&arc_sums.arcstat_mfu_hits, 0);
	wmsum_init(&arc_sums.arcstat_mfu_ghost_hits, 0);
	wmsum_init(&arc_sums.arcstat_uncached_hits, 0);
	wmsum_init(&arc_sums.arcstat_deleted, 0);
	wmsum_init(&arc_sums.arcstat_mutex_miss, 0);
	wmsum_init(&arc_sums.arcstat_access_skip, 0);
	wmsum_init(&arc_sums.arcstat_evict_skip, 0);
	wmsum_init(&arc_sums.arcstat_evict_not_enough, 0);
	wmsum_init(&arc_sums.arcstat_evict_l2_cached, 0);
	wmsum_init(&arc_sums.arcstat_evict_l2_eligible, 0);
	wmsum_init(&arc_sums.arcstat_evict_l2_eligible_mfu, 0);
	wmsum_init(&arc_sums.arcstat_evict_l2_eligible_mru, 0);
	wmsum_init(&arc_sums.arcstat_evict_l2_ineligible, 0);
	wmsum_init(&arc_sums.arcstat_evict_l2_skip, 0);
	wmsum_init(&arc_sums.arcstat_hash_collisions, 0);
	wmsum_init(&arc_sums.arcstat_hash_chains, 0);
	aggsum_init(&arc_sums.arcstat_size, 0);
	wmsum_init(&arc_sums.arcstat_compressed_size, 0);
	wmsum_init(&arc_sums.arcstat_uncompressed_size, 0);
	wmsum_init(&arc_sums.arcstat_overhead_size, 0);
	wmsum_init(&arc_sums.arcstat_hdr_size, 0);
	wmsum_init(&arc_sums.arcstat_data_size, 0);
	wmsum_init(&arc_sums.arcstat_metadata_size, 0);
	wmsum_init(&arc_sums.arcstat_dbuf_size, 0);
	wmsum_init(&arc_sums.arcstat_dnode_size, 0);
	wmsum_init(&arc_sums.arcstat_bonus_size, 0);
	wmsum_init(&arc_sums.arcstat_l2_hits, 0);
	wmsum_init(&arc_sums.arcstat_l2_misses, 0);
	wmsum_init(&arc_sums.arcstat_l2_prefetch_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_mru_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_mfu_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_bufc_data_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_bufc_metadata_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_feeds, 0);
	wmsum_init(&arc_sums.arcstat_l2_rw_clash, 0);
	wmsum_init(&arc_sums.arcstat_l2_read_bytes, 0);
	wmsum_init(&arc_sums.arcstat_l2_write_bytes, 0);
	wmsum_init(&arc_sums.arcstat_l2_writes_sent, 0);
	wmsum_init(&arc_sums.arcstat_l2_writes_done, 0);
	wmsum_init(&arc_sums.arcstat_l2_writes_error, 0);
	wmsum_init(&arc_sums.arcstat_l2_writes_lock_retry, 0);
	wmsum_init(&arc_sums.arcstat_l2_evict_lock_retry, 0);
	wmsum_init(&arc_sums.arcstat_l2_evict_reading, 0);
	wmsum_init(&arc_sums.arcstat_l2_evict_l1cached, 0);
	wmsum_init(&arc_sums.arcstat_l2_free_on_write, 0);
	wmsum_init(&arc_sums.arcstat_l2_abort_lowmem, 0);
	wmsum_init(&arc_sums.arcstat_l2_cksum_bad, 0);
	wmsum_init(&arc_sums.arcstat_l2_io_error, 0);
	wmsum_init(&arc_sums.arcstat_l2_lsize, 0);
	wmsum_init(&arc_sums.arcstat_l2_psize, 0);
	aggsum_init(&arc_sums.arcstat_l2_hdr_size, 0);
	wmsum_init(&arc_sums.arcstat_l2_log_blk_writes, 0);
	wmsum_init(&arc_sums.arcstat_l2_log_blk_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_log_blk_count, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_success, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_abort_unsupported, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_abort_io_errors, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_abort_dh_errors, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_abort_cksum_lb_errors, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_abort_lowmem, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_size, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_asize, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_bufs, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_bufs_precached, 0);
	wmsum_init(&arc_sums.arcstat_l2_rebuild_log_blks, 0);
	wmsum_init(&arc_sums.arcstat_memory_throttle_count, 0);
	wmsum_init(&arc_sums.arcstat_memory_direct_count, 0);
	wmsum_init(&arc_sums.arcstat_memory_indirect_count, 0);
	wmsum_init(&arc_sums.arcstat_prune, 0);
	wmsum_init(&arc_sums.arcstat_meta_used, 0);
	wmsum_init(&arc_sums.arcstat_async_upgrade_sync, 0);
	wmsum_init(&arc_sums.arcstat_predictive_prefetch, 0);
	wmsum_init(&arc_sums.arcstat_demand_hit_predictive_prefetch, 0);
	wmsum_init(&arc_sums.arcstat_demand_iohit_predictive_prefetch, 0);
	wmsum_init(&arc_sums.arcstat_prescient_prefetch, 0);
	wmsum_init(&arc_sums.arcstat_demand_hit_prescient_prefetch, 0);
	wmsum_init(&arc_sums.arcstat_demand_iohit_prescient_prefetch, 0);
	wmsum_init(&arc_sums.arcstat_raw_size, 0);
	wmsum_init(&arc_sums.arcstat_cached_only_in_progress, 0);
	wmsum_init(&arc_sums.arcstat_abd_chunk_waste_size, 0);

	arc_anon->arcs_state = ARC_STATE_ANON;
	arc_mru->arcs_state = ARC_STATE_MRU;
	arc_mru_ghost->arcs_state = ARC_STATE_MRU_GHOST;
	arc_mfu->arcs_state = ARC_STATE_MFU;
	arc_mfu_ghost->arcs_state = ARC_STATE_MFU_GHOST;
	arc_l2c_only->arcs_state = ARC_STATE_L2C_ONLY;
	arc_uncached->arcs_state = ARC_STATE_UNCACHED;
}

static void
arc_state_fini(void)
{
	zfs_refcount_destroy(&arc_anon->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_anon->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mru->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mru->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mru_ghost->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mru_ghost->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mfu->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mfu->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mfu_ghost->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mfu_ghost->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_l2c_only->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_l2c_only->arcs_esize[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_uncached->arcs_esize[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_uncached->arcs_esize[ARC_BUFC_DATA]);

	zfs_refcount_destroy(&arc_anon->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_anon->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mru->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mru->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mru_ghost->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mru_ghost->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mfu->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mfu->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_mfu_ghost->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_mfu_ghost->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_l2c_only->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_l2c_only->arcs_size[ARC_BUFC_METADATA]);
	zfs_refcount_destroy(&arc_uncached->arcs_size[ARC_BUFC_DATA]);
	zfs_refcount_destroy(&arc_uncached->arcs_size[ARC_BUFC_METADATA]);

	multilist_destroy(&arc_mru->arcs_list[ARC_BUFC_METADATA]);
	multilist_destroy(&arc_mru_ghost->arcs_list[ARC_BUFC_METADATA]);
	multilist_destroy(&arc_mfu->arcs_list[ARC_BUFC_METADATA]);
	multilist_destroy(&arc_mfu_ghost->arcs_list[ARC_BUFC_METADATA]);
	multilist_destroy(&arc_mru->arcs_list[ARC_BUFC_DATA]);
	multilist_destroy(&arc_mru_ghost->arcs_list[ARC_BUFC_DATA]);
	multilist_destroy(&arc_mfu->arcs_list[ARC_BUFC_DATA]);
	multilist_destroy(&arc_mfu_ghost->arcs_list[ARC_BUFC_DATA]);
	multilist_destroy(&arc_l2c_only->arcs_list[ARC_BUFC_METADATA]);
	multilist_destroy(&arc_l2c_only->arcs_list[ARC_BUFC_DATA]);
	multilist_destroy(&arc_uncached->arcs_list[ARC_BUFC_METADATA]);
	multilist_destroy(&arc_uncached->arcs_list[ARC_BUFC_DATA]);

	wmsum_fini(&arc_mru_ghost->arcs_hits[ARC_BUFC_DATA]);
	wmsum_fini(&arc_mru_ghost->arcs_hits[ARC_BUFC_METADATA]);
	wmsum_fini(&arc_mfu_ghost->arcs_hits[ARC_BUFC_DATA]);
	wmsum_fini(&arc_mfu_ghost->arcs_hits[ARC_BUFC_METADATA]);

	wmsum_fini(&arc_sums.arcstat_hits);
	wmsum_fini(&arc_sums.arcstat_iohits);
	wmsum_fini(&arc_sums.arcstat_misses);
	wmsum_fini(&arc_sums.arcstat_demand_data_hits);
	wmsum_fini(&arc_sums.arcstat_demand_data_iohits);
	wmsum_fini(&arc_sums.arcstat_demand_data_misses);
	wmsum_fini(&arc_sums.arcstat_demand_metadata_hits);
	wmsum_fini(&arc_sums.arcstat_demand_metadata_iohits);
	wmsum_fini(&arc_sums.arcstat_demand_metadata_misses);
	wmsum_fini(&arc_sums.arcstat_prefetch_data_hits);
	wmsum_fini(&arc_sums.arcstat_prefetch_data_iohits);
	wmsum_fini(&arc_sums.arcstat_prefetch_data_misses);
	wmsum_fini(&arc_sums.arcstat_prefetch_metadata_hits);
	wmsum_fini(&arc_sums.arcstat_prefetch_metadata_iohits);
	wmsum_fini(&arc_sums.arcstat_prefetch_metadata_misses);
	wmsum_fini(&arc_sums.arcstat_mru_hits);
	wmsum_fini(&arc_sums.arcstat_mru_ghost_hits);
	wmsum_fini(&arc_sums.arcstat_mfu_hits);
	wmsum_fini(&arc_sums.arcstat_mfu_ghost_hits);
	wmsum_fini(&arc_sums.arcstat_uncached_hits);
	wmsum_fini(&arc_sums.arcstat_deleted);
	wmsum_fini(&arc_sums.arcstat_mutex_miss);
	wmsum_fini(&arc_sums.arcstat_access_skip);
	wmsum_fini(&arc_sums.arcstat_evict_skip);
	wmsum_fini(&arc_sums.arcstat_evict_not_enough);
	wmsum_fini(&arc_sums.arcstat_evict_l2_cached);
	wmsum_fini(&arc_sums.arcstat_evict_l2_eligible);
	wmsum_fini(&arc_sums.arcstat_evict_l2_eligible_mfu);
	wmsum_fini(&arc_sums.arcstat_evict_l2_eligible_mru);
	wmsum_fini(&arc_sums.arcstat_evict_l2_ineligible);
	wmsum_fini(&arc_sums.arcstat_evict_l2_skip);
	wmsum_fini(&arc_sums.arcstat_hash_collisions);
	wmsum_fini(&arc_sums.arcstat_hash_chains);
	aggsum_fini(&arc_sums.arcstat_size);
	wmsum_fini(&arc_sums.arcstat_compressed_size);
	wmsum_fini(&arc_sums.arcstat_uncompressed_size);
	wmsum_fini(&arc_sums.arcstat_overhead_size);
	wmsum_fini(&arc_sums.arcstat_hdr_size);
	wmsum_fini(&arc_sums.arcstat_data_size);
	wmsum_fini(&arc_sums.arcstat_metadata_size);
	wmsum_fini(&arc_sums.arcstat_dbuf_size);
	wmsum_fini(&arc_sums.arcstat_dnode_size);
	wmsum_fini(&arc_sums.arcstat_bonus_size);
	wmsum_fini(&arc_sums.arcstat_l2_hits);
	wmsum_fini(&arc_sums.arcstat_l2_misses);
	wmsum_fini(&arc_sums.arcstat_l2_prefetch_asize);
	wmsum_fini(&arc_sums.arcstat_l2_mru_asize);
	wmsum_fini(&arc_sums.arcstat_l2_mfu_asize);
	wmsum_fini(&arc_sums.arcstat_l2_bufc_data_asize);
	wmsum_fini(&arc_sums.arcstat_l2_bufc_metadata_asize);
	wmsum_fini(&arc_sums.arcstat_l2_feeds);
	wmsum_fini(&arc_sums.arcstat_l2_rw_clash);
	wmsum_fini(&arc_sums.arcstat_l2_read_bytes);
	wmsum_fini(&arc_sums.arcstat_l2_write_bytes);
	wmsum_fini(&arc_sums.arcstat_l2_writes_sent);
	wmsum_fini(&arc_sums.arcstat_l2_writes_done);
	wmsum_fini(&arc_sums.arcstat_l2_writes_error);
	wmsum_fini(&arc_sums.arcstat_l2_writes_lock_retry);
	wmsum_fini(&arc_sums.arcstat_l2_evict_lock_retry);
	wmsum_fini(&arc_sums.arcstat_l2_evict_reading);
	wmsum_fini(&arc_sums.arcstat_l2_evict_l1cached);
	wmsum_fini(&arc_sums.arcstat_l2_free_on_write);
	wmsum_fini(&arc_sums.arcstat_l2_abort_lowmem);
	wmsum_fini(&arc_sums.arcstat_l2_cksum_bad);
	wmsum_fini(&arc_sums.arcstat_l2_io_error);
	wmsum_fini(&arc_sums.arcstat_l2_lsize);
	wmsum_fini(&arc_sums.arcstat_l2_psize);
	aggsum_fini(&arc_sums.arcstat_l2_hdr_size);
	wmsum_fini(&arc_sums.arcstat_l2_log_blk_writes);
	wmsum_fini(&arc_sums.arcstat_l2_log_blk_asize);
	wmsum_fini(&arc_sums.arcstat_l2_log_blk_count);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_success);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_abort_unsupported);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_abort_io_errors);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_abort_dh_errors);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_abort_cksum_lb_errors);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_abort_lowmem);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_size);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_asize);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_bufs);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_bufs_precached);
	wmsum_fini(&arc_sums.arcstat_l2_rebuild_log_blks);
	wmsum_fini(&arc_sums.arcstat_memory_throttle_count);
	wmsum_fini(&arc_sums.arcstat_memory_direct_count);
	wmsum_fini(&arc_sums.arcstat_memory_indirect_count);
	wmsum_fini(&arc_sums.arcstat_prune);
	wmsum_fini(&arc_sums.arcstat_meta_used);
	wmsum_fini(&arc_sums.arcstat_async_upgrade_sync);
	wmsum_fini(&arc_sums.arcstat_predictive_prefetch);
	wmsum_fini(&arc_sums.arcstat_demand_hit_predictive_prefetch);
	wmsum_fini(&arc_sums.arcstat_demand_iohit_predictive_prefetch);
	wmsum_fini(&arc_sums.arcstat_prescient_prefetch);
	wmsum_fini(&arc_sums.arcstat_demand_hit_prescient_prefetch);
	wmsum_fini(&arc_sums.arcstat_demand_iohit_prescient_prefetch);
	wmsum_fini(&arc_sums.arcstat_raw_size);
	wmsum_fini(&arc_sums.arcstat_cached_only_in_progress);
	wmsum_fini(&arc_sums.arcstat_abd_chunk_waste_size);
}

uint64_t
arc_target_bytes(void)
{
	return (arc_c);
}

void
arc_set_limits(uint64_t allmem)
{
	 
	arc_c_min = MAX(allmem / 32, 2ULL << SPA_MAXBLOCKSHIFT);

	 
	arc_c_max = arc_default_max(arc_c_min, allmem);
}
void
arc_init(void)
{
	uint64_t percent, allmem = arc_all_memory();
	mutex_init(&arc_evict_lock, NULL, MUTEX_DEFAULT, NULL);
	list_create(&arc_evict_waiters, sizeof (arc_evict_waiter_t),
	    offsetof(arc_evict_waiter_t, aew_node));

	arc_min_prefetch_ms = 1000;
	arc_min_prescient_prefetch_ms = 6000;

#if defined(_KERNEL)
	arc_lowmem_init();
#endif

	arc_set_limits(allmem);

#ifdef _KERNEL
	 
	if (zfs_arc_max != 0 && zfs_arc_max >= MIN_ARC_MAX &&
	    zfs_arc_max < allmem) {
		arc_c_max = zfs_arc_max;
		if (arc_c_min >= arc_c_max) {
			arc_c_min = MAX(zfs_arc_max / 2,
			    2ULL << SPA_MAXBLOCKSHIFT);
		}
	}
#else
	 
	arc_c_min = MAX(arc_c_max / 2, 2ULL << SPA_MAXBLOCKSHIFT);
#endif

	arc_c = arc_c_min;
	 
	arc_meta = (1ULL << 32) / 4;	 
	arc_pd = (1ULL << 32) / 2;	 
	arc_pm = (1ULL << 32) / 2;	 

	percent = MIN(zfs_arc_dnode_limit_percent, 100);
	arc_dnode_limit = arc_c_max * percent / 100;

	 
	arc_tuning_update(B_TRUE);

	 
	if (kmem_debugging())
		arc_c = arc_c / 2;
	if (arc_c < arc_c_min)
		arc_c = arc_c_min;

	arc_register_hotplug();

	arc_state_init();

	buf_init();

	list_create(&arc_prune_list, sizeof (arc_prune_t),
	    offsetof(arc_prune_t, p_node));
	mutex_init(&arc_prune_mtx, NULL, MUTEX_DEFAULT, NULL);

	arc_prune_taskq = taskq_create("arc_prune", zfs_arc_prune_task_threads,
	    defclsyspri, 100, INT_MAX, TASKQ_PREPOPULATE | TASKQ_DYNAMIC);

	arc_ksp = kstat_create("zfs", 0, "arcstats", "misc", KSTAT_TYPE_NAMED,
	    sizeof (arc_stats) / sizeof (kstat_named_t), KSTAT_FLAG_VIRTUAL);

	if (arc_ksp != NULL) {
		arc_ksp->ks_data = &arc_stats;
		arc_ksp->ks_update = arc_kstat_update;
		kstat_install(arc_ksp);
	}

	arc_state_evict_markers =
	    arc_state_alloc_markers(arc_state_evict_marker_count);
	arc_evict_zthr = zthr_create_timer("arc_evict",
	    arc_evict_cb_check, arc_evict_cb, NULL, SEC2NSEC(1), defclsyspri);
	arc_reap_zthr = zthr_create_timer("arc_reap",
	    arc_reap_cb_check, arc_reap_cb, NULL, SEC2NSEC(1), minclsyspri);

	arc_warm = B_FALSE;

	 
#ifdef __LP64__
	if (zfs_dirty_data_max_max == 0)
		zfs_dirty_data_max_max = MIN(4ULL * 1024 * 1024 * 1024,
		    allmem * zfs_dirty_data_max_max_percent / 100);
#else
	if (zfs_dirty_data_max_max == 0)
		zfs_dirty_data_max_max = MIN(1ULL * 1024 * 1024 * 1024,
		    allmem * zfs_dirty_data_max_max_percent / 100);
#endif

	if (zfs_dirty_data_max == 0) {
		zfs_dirty_data_max = allmem *
		    zfs_dirty_data_max_percent / 100;
		zfs_dirty_data_max = MIN(zfs_dirty_data_max,
		    zfs_dirty_data_max_max);
	}

	if (zfs_wrlog_data_max == 0) {

		 
		zfs_wrlog_data_max = zfs_dirty_data_max * 2;
	}
}

void
arc_fini(void)
{
	arc_prune_t *p;

#ifdef _KERNEL
	arc_lowmem_fini();
#endif  

	 
	arc_flush(NULL, B_TRUE);

	if (arc_ksp != NULL) {
		kstat_delete(arc_ksp);
		arc_ksp = NULL;
	}

	taskq_wait(arc_prune_taskq);
	taskq_destroy(arc_prune_taskq);

	mutex_enter(&arc_prune_mtx);
	while ((p = list_remove_head(&arc_prune_list)) != NULL) {
		zfs_refcount_remove(&p->p_refcnt, &arc_prune_list);
		zfs_refcount_destroy(&p->p_refcnt);
		kmem_free(p, sizeof (*p));
	}
	mutex_exit(&arc_prune_mtx);

	list_destroy(&arc_prune_list);
	mutex_destroy(&arc_prune_mtx);

	(void) zthr_cancel(arc_evict_zthr);
	(void) zthr_cancel(arc_reap_zthr);
	arc_state_free_markers(arc_state_evict_markers,
	    arc_state_evict_marker_count);

	mutex_destroy(&arc_evict_lock);
	list_destroy(&arc_evict_waiters);

	 
	l2arc_do_free_on_write();

	 
	buf_fini();
	arc_state_fini();

	arc_unregister_hotplug();

	 
	zthr_destroy(arc_evict_zthr);
	zthr_destroy(arc_reap_zthr);

	ASSERT0(arc_loaned_bytes);
}

 

static boolean_t
l2arc_write_eligible(uint64_t spa_guid, arc_buf_hdr_t *hdr)
{
	 
	if (hdr->b_spa != spa_guid || HDR_HAS_L2HDR(hdr) ||
	    HDR_IO_IN_PROGRESS(hdr) || !HDR_L2CACHE(hdr))
		return (B_FALSE);

	return (B_TRUE);
}

static uint64_t
l2arc_write_size(l2arc_dev_t *dev)
{
	uint64_t size;

	 
	size = l2arc_write_max;
	if (size == 0) {
		cmn_err(CE_NOTE, "Bad value for l2arc_write_max, value must "
		    "be greater than zero, resetting it to the default (%d)",
		    L2ARC_WRITE_SIZE);
		size = l2arc_write_max = L2ARC_WRITE_SIZE;
	}

	if (arc_warm == B_FALSE)
		size += l2arc_write_boost;

	 
	size += l2arc_log_blk_overhead(size, dev);
	if (dev->l2ad_vdev->vdev_has_trim && l2arc_trim_ahead > 0) {
		 
		size += MAX(64 * 1024 * 1024,
		    (size * l2arc_trim_ahead) / 100);
	}

	 
	if (size > dev->l2ad_end - dev->l2ad_start) {
		cmn_err(CE_NOTE, "l2arc_write_max or l2arc_write_boost "
		    "plus the overhead of log blocks (persistent L2ARC, "
		    "%llu bytes) exceeds the size of the cache device "
		    "(guid %llu), resetting them to the default (%d)",
		    (u_longlong_t)l2arc_log_blk_overhead(size, dev),
		    (u_longlong_t)dev->l2ad_vdev->vdev_guid, L2ARC_WRITE_SIZE);

		size = l2arc_write_max = l2arc_write_boost = L2ARC_WRITE_SIZE;

		if (l2arc_trim_ahead > 1) {
			cmn_err(CE_NOTE, "l2arc_trim_ahead set to 1");
			l2arc_trim_ahead = 1;
		}

		if (arc_warm == B_FALSE)
			size += l2arc_write_boost;

		size += l2arc_log_blk_overhead(size, dev);
		if (dev->l2ad_vdev->vdev_has_trim && l2arc_trim_ahead > 0) {
			size += MAX(64 * 1024 * 1024,
			    (size * l2arc_trim_ahead) / 100);
		}
	}

	return (size);

}

static clock_t
l2arc_write_interval(clock_t began, uint64_t wanted, uint64_t wrote)
{
	clock_t interval, next, now;

	 
	if (l2arc_feed_again && wrote > (wanted / 2))
		interval = (hz * l2arc_feed_min_ms) / 1000;
	else
		interval = hz * l2arc_feed_secs;

	now = ddi_get_lbolt();
	next = MAX(now, MIN(now + interval, began + interval));

	return (next);
}

 
static l2arc_dev_t *
l2arc_dev_get_next(void)
{
	l2arc_dev_t *first, *next = NULL;

	 
	mutex_enter(&spa_namespace_lock);
	mutex_enter(&l2arc_dev_mtx);

	 
	if (l2arc_ndev == 0)
		goto out;

	first = NULL;
	next = l2arc_dev_last;
	do {
		 
		if (next == NULL) {
			next = list_head(l2arc_dev_list);
		} else {
			next = list_next(l2arc_dev_list, next);
			if (next == NULL)
				next = list_head(l2arc_dev_list);
		}

		 
		if (first == NULL)
			first = next;
		else if (next == first)
			break;

		ASSERT3P(next, !=, NULL);
	} while (vdev_is_dead(next->l2ad_vdev) || next->l2ad_rebuild ||
	    next->l2ad_trim_all);

	 
	if (vdev_is_dead(next->l2ad_vdev) || next->l2ad_rebuild ||
	    next->l2ad_trim_all)
		next = NULL;

	l2arc_dev_last = next;

out:
	mutex_exit(&l2arc_dev_mtx);

	 
	if (next != NULL)
		spa_config_enter(next->l2ad_spa, SCL_L2ARC, next, RW_READER);
	mutex_exit(&spa_namespace_lock);

	return (next);
}

 
static void
l2arc_do_free_on_write(void)
{
	l2arc_data_free_t *df;

	mutex_enter(&l2arc_free_on_write_mtx);
	while ((df = list_remove_head(l2arc_free_on_write)) != NULL) {
		ASSERT3P(df->l2df_abd, !=, NULL);
		abd_free(df->l2df_abd);
		kmem_free(df, sizeof (l2arc_data_free_t));
	}
	mutex_exit(&l2arc_free_on_write_mtx);
}

 
static void
l2arc_write_done(zio_t *zio)
{
	l2arc_write_callback_t	*cb;
	l2arc_lb_abd_buf_t	*abd_buf;
	l2arc_lb_ptr_buf_t	*lb_ptr_buf;
	l2arc_dev_t		*dev;
	l2arc_dev_hdr_phys_t	*l2dhdr;
	list_t			*buflist;
	arc_buf_hdr_t		*head, *hdr, *hdr_prev;
	kmutex_t		*hash_lock;
	int64_t			bytes_dropped = 0;

	cb = zio->io_private;
	ASSERT3P(cb, !=, NULL);
	dev = cb->l2wcb_dev;
	l2dhdr = dev->l2ad_dev_hdr;
	ASSERT3P(dev, !=, NULL);
	head = cb->l2wcb_head;
	ASSERT3P(head, !=, NULL);
	buflist = &dev->l2ad_buflist;
	ASSERT3P(buflist, !=, NULL);
	DTRACE_PROBE2(l2arc__iodone, zio_t *, zio,
	    l2arc_write_callback_t *, cb);

	 
top:
	mutex_enter(&dev->l2ad_mtx);
	for (hdr = list_prev(buflist, head); hdr; hdr = hdr_prev) {
		hdr_prev = list_prev(buflist, hdr);

		hash_lock = HDR_LOCK(hdr);

		 
		if (!mutex_tryenter(hash_lock)) {
			 
			ARCSTAT_BUMP(arcstat_l2_writes_lock_retry);

			 
			list_remove(buflist, head);
			list_insert_after(buflist, hdr, head);

			mutex_exit(&dev->l2ad_mtx);

			 
			mutex_enter(hash_lock);
			mutex_exit(hash_lock);
			goto top;
		}

		 
		ASSERT(HDR_HAS_L1HDR(hdr));

		 
		if (zio->io_error != 0) {
			 
			list_remove(buflist, hdr);
			arc_hdr_clear_flags(hdr, ARC_FLAG_HAS_L2HDR);

			uint64_t psize = HDR_GET_PSIZE(hdr);
			l2arc_hdr_arcstats_decrement(hdr);

			bytes_dropped +=
			    vdev_psize_to_asize(dev->l2ad_vdev, psize);
			(void) zfs_refcount_remove_many(&dev->l2ad_alloc,
			    arc_hdr_size(hdr), hdr);
		}

		 
		arc_hdr_clear_flags(hdr, ARC_FLAG_L2_WRITING);

		mutex_exit(hash_lock);
	}

	 
	while ((abd_buf = list_remove_tail(&cb->l2wcb_abd_list)) != NULL) {
		abd_free(abd_buf->abd);
		zio_buf_free(abd_buf, sizeof (*abd_buf));
		if (zio->io_error != 0) {
			lb_ptr_buf = list_remove_head(&dev->l2ad_lbptr_list);
			 
			uint64_t asize =
			    L2BLK_GET_PSIZE((lb_ptr_buf->lb_ptr)->lbp_prop);
			bytes_dropped += asize;
			ARCSTAT_INCR(arcstat_l2_log_blk_asize, -asize);
			ARCSTAT_BUMPDOWN(arcstat_l2_log_blk_count);
			zfs_refcount_remove_many(&dev->l2ad_lb_asize, asize,
			    lb_ptr_buf);
			zfs_refcount_remove(&dev->l2ad_lb_count, lb_ptr_buf);
			kmem_free(lb_ptr_buf->lb_ptr,
			    sizeof (l2arc_log_blkptr_t));
			kmem_free(lb_ptr_buf, sizeof (l2arc_lb_ptr_buf_t));
		}
	}
	list_destroy(&cb->l2wcb_abd_list);

	if (zio->io_error != 0) {
		ARCSTAT_BUMP(arcstat_l2_writes_error);

		 
		lb_ptr_buf = list_head(&dev->l2ad_lbptr_list);
		for (int i = 0; i < 2; i++) {
			if (lb_ptr_buf == NULL) {
				 
				if (i == 0) {
					memset(l2dhdr, 0,
					    dev->l2ad_dev_hdr_asize);
				} else {
					memset(&l2dhdr->dh_start_lbps[i], 0,
					    sizeof (l2arc_log_blkptr_t));
				}
				break;
			}
			memcpy(&l2dhdr->dh_start_lbps[i], lb_ptr_buf->lb_ptr,
			    sizeof (l2arc_log_blkptr_t));
			lb_ptr_buf = list_next(&dev->l2ad_lbptr_list,
			    lb_ptr_buf);
		}
	}

	ARCSTAT_BUMP(arcstat_l2_writes_done);
	list_remove(buflist, head);
	ASSERT(!HDR_HAS_L1HDR(head));
	kmem_cache_free(hdr_l2only_cache, head);
	mutex_exit(&dev->l2ad_mtx);

	ASSERT(dev->l2ad_vdev != NULL);
	vdev_space_update(dev->l2ad_vdev, -bytes_dropped, 0, 0);

	l2arc_do_free_on_write();

	kmem_free(cb, sizeof (l2arc_write_callback_t));
}

static int
l2arc_untransform(zio_t *zio, l2arc_read_callback_t *cb)
{
	int ret;
	spa_t *spa = zio->io_spa;
	arc_buf_hdr_t *hdr = cb->l2rcb_hdr;
	blkptr_t *bp = zio->io_bp;
	uint8_t salt[ZIO_DATA_SALT_LEN];
	uint8_t iv[ZIO_DATA_IV_LEN];
	uint8_t mac[ZIO_DATA_MAC_LEN];
	boolean_t no_crypt = B_FALSE;

	 
	ASSERT3U(BP_GET_TYPE(bp), !=, DMU_OT_INTENT_LOG);
	ASSERT(MUTEX_HELD(HDR_LOCK(hdr)));
	ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);

	 
	if (BP_IS_ENCRYPTED(bp)) {
		abd_t *eabd = arc_get_data_abd(hdr, arc_hdr_size(hdr), hdr,
		    ARC_HDR_USE_RESERVE);

		zio_crypt_decode_params_bp(bp, salt, iv);
		zio_crypt_decode_mac_bp(bp, mac);

		ret = spa_do_crypt_abd(B_FALSE, spa, &cb->l2rcb_zb,
		    BP_GET_TYPE(bp), BP_GET_DEDUP(bp), BP_SHOULD_BYTESWAP(bp),
		    salt, iv, mac, HDR_GET_PSIZE(hdr), eabd,
		    hdr->b_l1hdr.b_pabd, &no_crypt);
		if (ret != 0) {
			arc_free_data_abd(hdr, eabd, arc_hdr_size(hdr), hdr);
			goto error;
		}

		 
		if (!no_crypt) {
			arc_free_data_abd(hdr, hdr->b_l1hdr.b_pabd,
			    arc_hdr_size(hdr), hdr);
			hdr->b_l1hdr.b_pabd = eabd;
			zio->io_abd = eabd;
		} else {
			arc_free_data_abd(hdr, eabd, arc_hdr_size(hdr), hdr);
		}
	}

	 
	if (HDR_GET_COMPRESS(hdr) != ZIO_COMPRESS_OFF &&
	    !HDR_COMPRESSION_ENABLED(hdr)) {
		abd_t *cabd = arc_get_data_abd(hdr, arc_hdr_size(hdr), hdr,
		    ARC_HDR_USE_RESERVE);
		void *tmp = abd_borrow_buf(cabd, arc_hdr_size(hdr));

		ret = zio_decompress_data(HDR_GET_COMPRESS(hdr),
		    hdr->b_l1hdr.b_pabd, tmp, HDR_GET_PSIZE(hdr),
		    HDR_GET_LSIZE(hdr), &hdr->b_complevel);
		if (ret != 0) {
			abd_return_buf_copy(cabd, tmp, arc_hdr_size(hdr));
			arc_free_data_abd(hdr, cabd, arc_hdr_size(hdr), hdr);
			goto error;
		}

		abd_return_buf_copy(cabd, tmp, arc_hdr_size(hdr));
		arc_free_data_abd(hdr, hdr->b_l1hdr.b_pabd,
		    arc_hdr_size(hdr), hdr);
		hdr->b_l1hdr.b_pabd = cabd;
		zio->io_abd = cabd;
		zio->io_size = HDR_GET_LSIZE(hdr);
	}

	return (0);

error:
	return (ret);
}


 
static void
l2arc_read_done(zio_t *zio)
{
	int tfm_error = 0;
	l2arc_read_callback_t *cb = zio->io_private;
	arc_buf_hdr_t *hdr;
	kmutex_t *hash_lock;
	boolean_t valid_cksum;
	boolean_t using_rdata = (BP_IS_ENCRYPTED(&cb->l2rcb_bp) &&
	    (cb->l2rcb_flags & ZIO_FLAG_RAW_ENCRYPT));

	ASSERT3P(zio->io_vd, !=, NULL);
	ASSERT(zio->io_flags & ZIO_FLAG_DONT_PROPAGATE);

	spa_config_exit(zio->io_spa, SCL_L2ARC, zio->io_vd);

	ASSERT3P(cb, !=, NULL);
	hdr = cb->l2rcb_hdr;
	ASSERT3P(hdr, !=, NULL);

	hash_lock = HDR_LOCK(hdr);
	mutex_enter(hash_lock);
	ASSERT3P(hash_lock, ==, HDR_LOCK(hdr));

	 
	if (cb->l2rcb_abd != NULL) {
		ASSERT3U(arc_hdr_size(hdr), <, zio->io_size);
		if (zio->io_error == 0) {
			if (using_rdata) {
				abd_copy(hdr->b_crypt_hdr.b_rabd,
				    cb->l2rcb_abd, arc_hdr_size(hdr));
			} else {
				abd_copy(hdr->b_l1hdr.b_pabd,
				    cb->l2rcb_abd, arc_hdr_size(hdr));
			}
		}

		 
		abd_free(cb->l2rcb_abd);
		zio->io_size = zio->io_orig_size = arc_hdr_size(hdr);

		if (using_rdata) {
			ASSERT(HDR_HAS_RABD(hdr));
			zio->io_abd = zio->io_orig_abd =
			    hdr->b_crypt_hdr.b_rabd;
		} else {
			ASSERT3P(hdr->b_l1hdr.b_pabd, !=, NULL);
			zio->io_abd = zio->io_orig_abd = hdr->b_l1hdr.b_pabd;
		}
	}

	ASSERT3P(zio->io_abd, !=, NULL);

	 
	ASSERT(zio->io_abd == hdr->b_l1hdr.b_pabd ||
	    (HDR_HAS_RABD(hdr) && zio->io_abd == hdr->b_crypt_hdr.b_rabd));
	zio->io_bp_copy = cb->l2rcb_bp;	 
	zio->io_bp = &zio->io_bp_copy;	 
	zio->io_prop.zp_complevel = hdr->b_complevel;

	valid_cksum = arc_cksum_is_equal(hdr, zio);

	 
	if (valid_cksum && !using_rdata)
		tfm_error = l2arc_untransform(zio, cb);

	if (valid_cksum && tfm_error == 0 && zio->io_error == 0 &&
	    !HDR_L2_EVICTED(hdr)) {
		mutex_exit(hash_lock);
		zio->io_private = hdr;
		arc_read_done(zio);
	} else {
		 
		if (zio->io_error != 0) {
			ARCSTAT_BUMP(arcstat_l2_io_error);
		} else {
			zio->io_error = SET_ERROR(EIO);
		}
		if (!valid_cksum || tfm_error != 0)
			ARCSTAT_BUMP(arcstat_l2_cksum_bad);

		 
		if (zio->io_waiter == NULL) {
			zio_t *pio = zio_unique_parent(zio);
			void *abd = (using_rdata) ?
			    hdr->b_crypt_hdr.b_rabd : hdr->b_l1hdr.b_pabd;

			ASSERT(!pio || pio->io_child_type == ZIO_CHILD_LOGICAL);

			zio = zio_read(pio, zio->io_spa, zio->io_bp,
			    abd, zio->io_size, arc_read_done,
			    hdr, zio->io_priority, cb->l2rcb_flags,
			    &cb->l2rcb_zb);

			 
			for (struct arc_callback *acb = hdr->b_l1hdr.b_acb;
			    acb != NULL; acb = acb->acb_next)
				acb->acb_zio_head = zio;

			mutex_exit(hash_lock);
			zio_nowait(zio);
		} else {
			mutex_exit(hash_lock);
		}
	}

	kmem_free(cb, sizeof (l2arc_read_callback_t));
}

 
static multilist_sublist_t *
l2arc_sublist_lock(int list_num)
{
	multilist_t *ml = NULL;
	unsigned int idx;

	ASSERT(list_num >= 0 && list_num < L2ARC_FEED_TYPES);

	switch (list_num) {
	case 0:
		ml = &arc_mfu->arcs_list[ARC_BUFC_METADATA];
		break;
	case 1:
		ml = &arc_mru->arcs_list[ARC_BUFC_METADATA];
		break;
	case 2:
		ml = &arc_mfu->arcs_list[ARC_BUFC_DATA];
		break;
	case 3:
		ml = &arc_mru->arcs_list[ARC_BUFC_DATA];
		break;
	default:
		return (NULL);
	}

	 
	idx = multilist_get_random_index(ml);
	return (multilist_sublist_lock(ml, idx));
}

 
static inline uint64_t
l2arc_log_blk_overhead(uint64_t write_sz, l2arc_dev_t *dev)
{
	if (dev->l2ad_log_entries == 0) {
		return (0);
	} else {
		uint64_t log_entries = write_sz >> SPA_MINBLOCKSHIFT;

		uint64_t log_blocks = (log_entries +
		    dev->l2ad_log_entries - 1) /
		    dev->l2ad_log_entries;

		return (vdev_psize_to_asize(dev->l2ad_vdev,
		    sizeof (l2arc_log_blk_phys_t)) * log_blocks);
	}
}

 
static void
l2arc_evict(l2arc_dev_t *dev, uint64_t distance, boolean_t all)
{
	list_t *buflist;
	arc_buf_hdr_t *hdr, *hdr_prev;
	kmutex_t *hash_lock;
	uint64_t taddr;
	l2arc_lb_ptr_buf_t *lb_ptr_buf, *lb_ptr_buf_prev;
	vdev_t *vd = dev->l2ad_vdev;
	boolean_t rerun;

	buflist = &dev->l2ad_buflist;

top:
	rerun = B_FALSE;
	if (dev->l2ad_hand + distance > dev->l2ad_end) {
		 
		rerun = B_TRUE;
		taddr = dev->l2ad_end;
	} else {
		taddr = dev->l2ad_hand + distance;
	}
	DTRACE_PROBE4(l2arc__evict, l2arc_dev_t *, dev, list_t *, buflist,
	    uint64_t, taddr, boolean_t, all);

	if (!all) {
		 
		if (dev->l2ad_first) {
			 
			goto out;
		} else {
			 
			if (vd->vdev_has_trim && dev->l2ad_evict < taddr &&
			    l2arc_trim_ahead > 0) {
				 
				spa_config_exit(dev->l2ad_spa, SCL_L2ARC, dev);
				vdev_trim_simple(vd,
				    dev->l2ad_evict - VDEV_LABEL_START_SIZE,
				    taddr - dev->l2ad_evict);
				spa_config_enter(dev->l2ad_spa, SCL_L2ARC, dev,
				    RW_READER);
			}

			 
			dev->l2ad_evict = MAX(dev->l2ad_evict, taddr);
		}
	}

retry:
	mutex_enter(&dev->l2ad_mtx);
	 
	for (lb_ptr_buf = list_tail(&dev->l2ad_lbptr_list); lb_ptr_buf;
	    lb_ptr_buf = lb_ptr_buf_prev) {

		lb_ptr_buf_prev = list_prev(&dev->l2ad_lbptr_list, lb_ptr_buf);

		 
		uint64_t asize = L2BLK_GET_PSIZE(
		    (lb_ptr_buf->lb_ptr)->lbp_prop);

		 
		if (!all && l2arc_log_blkptr_valid(dev, lb_ptr_buf->lb_ptr)) {
			break;
		} else {
			vdev_space_update(vd, -asize, 0, 0);
			ARCSTAT_INCR(arcstat_l2_log_blk_asize, -asize);
			ARCSTAT_BUMPDOWN(arcstat_l2_log_blk_count);
			zfs_refcount_remove_many(&dev->l2ad_lb_asize, asize,
			    lb_ptr_buf);
			zfs_refcount_remove(&dev->l2ad_lb_count, lb_ptr_buf);
			list_remove(&dev->l2ad_lbptr_list, lb_ptr_buf);
			kmem_free(lb_ptr_buf->lb_ptr,
			    sizeof (l2arc_log_blkptr_t));
			kmem_free(lb_ptr_buf, sizeof (l2arc_lb_ptr_buf_t));
		}
	}

	for (hdr = list_tail(buflist); hdr; hdr = hdr_prev) {
		hdr_prev = list_prev(buflist, hdr);

		ASSERT(!HDR_EMPTY(hdr));
		hash_lock = HDR_LOCK(hdr);

		 
		if (!mutex_tryenter(hash_lock)) {
			 
			ARCSTAT_BUMP(arcstat_l2_evict_lock_retry);
			mutex_exit(&dev->l2ad_mtx);
			mutex_enter(hash_lock);
			mutex_exit(hash_lock);
			goto retry;
		}

		 
		ASSERT(HDR_HAS_L2HDR(hdr));

		 
		ASSERT(!HDR_L2_WRITING(hdr));
		ASSERT(!HDR_L2_WRITE_HEAD(hdr));

		if (!all && (hdr->b_l2hdr.b_daddr >= dev->l2ad_evict ||
		    hdr->b_l2hdr.b_daddr < dev->l2ad_hand)) {
			 
			mutex_exit(hash_lock);
			break;
		}

		if (!HDR_HAS_L1HDR(hdr)) {
			ASSERT(!HDR_L2_READING(hdr));
			 
			arc_change_state(arc_anon, hdr);
			arc_hdr_destroy(hdr);
		} else {
			ASSERT(hdr->b_l1hdr.b_state != arc_l2c_only);
			ARCSTAT_BUMP(arcstat_l2_evict_l1cached);
			 
			if (HDR_L2_READING(hdr)) {
				ARCSTAT_BUMP(arcstat_l2_evict_reading);
				arc_hdr_set_flags(hdr, ARC_FLAG_L2_EVICTED);
			}

			arc_hdr_l2hdr_destroy(hdr);
		}
		mutex_exit(hash_lock);
	}
	mutex_exit(&dev->l2ad_mtx);

out:
	 
	if (!all && rerun) {
		 
		dev->l2ad_hand = dev->l2ad_start;
		dev->l2ad_evict = dev->l2ad_start;
		dev->l2ad_first = B_FALSE;
		goto top;
	}

	if (!all) {
		 
		ASSERT3U(dev->l2ad_hand + distance, <, dev->l2ad_end);
		if (!dev->l2ad_first)
			ASSERT3U(dev->l2ad_hand, <=, dev->l2ad_evict);
	}
}

 
static int
l2arc_apply_transforms(spa_t *spa, arc_buf_hdr_t *hdr, uint64_t asize,
    abd_t **abd_out)
{
	int ret;
	void *tmp = NULL;
	abd_t *cabd = NULL, *eabd = NULL, *to_write = hdr->b_l1hdr.b_pabd;
	enum zio_compress compress = HDR_GET_COMPRESS(hdr);
	uint64_t psize = HDR_GET_PSIZE(hdr);
	uint64_t size = arc_hdr_size(hdr);
	boolean_t ismd = HDR_ISTYPE_METADATA(hdr);
	boolean_t bswap = (hdr->b_l1hdr.b_byteswap != DMU_BSWAP_NUMFUNCS);
	dsl_crypto_key_t *dck = NULL;
	uint8_t mac[ZIO_DATA_MAC_LEN] = { 0 };
	boolean_t no_crypt = B_FALSE;

	ASSERT((HDR_GET_COMPRESS(hdr) != ZIO_COMPRESS_OFF &&
	    !HDR_COMPRESSION_ENABLED(hdr)) ||
	    HDR_ENCRYPTED(hdr) || HDR_SHARED_DATA(hdr) || psize != asize);
	ASSERT3U(psize, <=, asize);

	 
	if (HDR_HAS_RABD(hdr) && asize != psize) {
		ASSERT3U(asize, >=, psize);
		to_write = abd_alloc_for_io(asize, ismd);
		abd_copy(to_write, hdr->b_crypt_hdr.b_rabd, psize);
		if (psize != asize)
			abd_zero_off(to_write, psize, asize - psize);
		goto out;
	}

	if ((compress == ZIO_COMPRESS_OFF || HDR_COMPRESSION_ENABLED(hdr)) &&
	    !HDR_ENCRYPTED(hdr)) {
		ASSERT3U(size, ==, psize);
		to_write = abd_alloc_for_io(asize, ismd);
		abd_copy(to_write, hdr->b_l1hdr.b_pabd, size);
		if (size != asize)
			abd_zero_off(to_write, size, asize - size);
		goto out;
	}

	if (compress != ZIO_COMPRESS_OFF && !HDR_COMPRESSION_ENABLED(hdr)) {
		 
		uint64_t bufsize = MAX(size, asize);
		cabd = abd_alloc_for_io(bufsize, ismd);
		tmp = abd_borrow_buf(cabd, bufsize);

		psize = zio_compress_data(compress, to_write, &tmp, size,
		    hdr->b_complevel);

		if (psize >= asize) {
			psize = HDR_GET_PSIZE(hdr);
			abd_return_buf_copy(cabd, tmp, bufsize);
			HDR_SET_COMPRESS(hdr, ZIO_COMPRESS_OFF);
			to_write = cabd;
			abd_copy(to_write, hdr->b_l1hdr.b_pabd, psize);
			if (psize != asize)
				abd_zero_off(to_write, psize, asize - psize);
			goto encrypt;
		}
		ASSERT3U(psize, <=, HDR_GET_PSIZE(hdr));
		if (psize < asize)
			memset((char *)tmp + psize, 0, bufsize - psize);
		psize = HDR_GET_PSIZE(hdr);
		abd_return_buf_copy(cabd, tmp, bufsize);
		to_write = cabd;
	}

encrypt:
	if (HDR_ENCRYPTED(hdr)) {
		eabd = abd_alloc_for_io(asize, ismd);

		 
		ret = spa_keystore_lookup_key(spa, hdr->b_crypt_hdr.b_dsobj,
		    FTAG, &dck);
		if (ret != 0)
			goto error;

		ret = zio_do_crypt_abd(B_TRUE, &dck->dck_key,
		    hdr->b_crypt_hdr.b_ot, bswap, hdr->b_crypt_hdr.b_salt,
		    hdr->b_crypt_hdr.b_iv, mac, psize, to_write, eabd,
		    &no_crypt);
		if (ret != 0)
			goto error;

		if (no_crypt)
			abd_copy(eabd, to_write, psize);

		if (psize != asize)
			abd_zero_off(eabd, psize, asize - psize);

		 
		ASSERT0(memcmp(mac, hdr->b_crypt_hdr.b_mac, ZIO_DATA_MAC_LEN));
		spa_keystore_dsl_key_rele(spa, dck, FTAG);

		if (to_write == cabd)
			abd_free(cabd);

		to_write = eabd;
	}

out:
	ASSERT3P(to_write, !=, hdr->b_l1hdr.b_pabd);
	*abd_out = to_write;
	return (0);

error:
	if (dck != NULL)
		spa_keystore_dsl_key_rele(spa, dck, FTAG);
	if (cabd != NULL)
		abd_free(cabd);
	if (eabd != NULL)
		abd_free(eabd);

	*abd_out = NULL;
	return (ret);
}

static void
l2arc_blk_fetch_done(zio_t *zio)
{
	l2arc_read_callback_t *cb;

	cb = zio->io_private;
	if (cb->l2rcb_abd != NULL)
		abd_free(cb->l2rcb_abd);
	kmem_free(cb, sizeof (l2arc_read_callback_t));
}

 
static uint64_t
l2arc_write_buffers(spa_t *spa, l2arc_dev_t *dev, uint64_t target_sz)
{
	arc_buf_hdr_t 		*hdr, *hdr_prev, *head;
	uint64_t 		write_asize, write_psize, write_lsize, headroom;
	boolean_t		full;
	l2arc_write_callback_t	*cb = NULL;
	zio_t 			*pio, *wzio;
	uint64_t 		guid = spa_load_guid(spa);
	l2arc_dev_hdr_phys_t	*l2dhdr = dev->l2ad_dev_hdr;

	ASSERT3P(dev->l2ad_vdev, !=, NULL);

	pio = NULL;
	write_lsize = write_asize = write_psize = 0;
	full = B_FALSE;
	head = kmem_cache_alloc(hdr_l2only_cache, KM_PUSHPAGE);
	arc_hdr_set_flags(head, ARC_FLAG_L2_WRITE_HEAD | ARC_FLAG_HAS_L2HDR);

	 
	for (int pass = 0; pass < L2ARC_FEED_TYPES; pass++) {
		 
		if (l2arc_mfuonly) {
			if (pass == 1 || pass == 3)
				continue;
		}

		multilist_sublist_t *mls = l2arc_sublist_lock(pass);
		uint64_t passed_sz = 0;

		VERIFY3P(mls, !=, NULL);

		 
		if (arc_warm == B_FALSE)
			hdr = multilist_sublist_head(mls);
		else
			hdr = multilist_sublist_tail(mls);

		headroom = target_sz * l2arc_headroom;
		if (zfs_compressed_arc_enabled)
			headroom = (headroom * l2arc_headroom_boost) / 100;

		for (; hdr; hdr = hdr_prev) {
			kmutex_t *hash_lock;
			abd_t *to_write = NULL;

			if (arc_warm == B_FALSE)
				hdr_prev = multilist_sublist_next(mls, hdr);
			else
				hdr_prev = multilist_sublist_prev(mls, hdr);

			hash_lock = HDR_LOCK(hdr);
			if (!mutex_tryenter(hash_lock)) {
				 
				continue;
			}

			passed_sz += HDR_GET_LSIZE(hdr);
			if (l2arc_headroom != 0 && passed_sz > headroom) {
				 
				mutex_exit(hash_lock);
				break;
			}

			if (!l2arc_write_eligible(guid, hdr)) {
				mutex_exit(hash_lock);
				continue;
			}

			ASSERT(HDR_HAS_L1HDR(hdr));

			ASSERT3U(HDR_GET_PSIZE(hdr), >, 0);
			ASSERT3U(arc_hdr_size(hdr), >, 0);
			ASSERT(hdr->b_l1hdr.b_pabd != NULL ||
			    HDR_HAS_RABD(hdr));
			uint64_t psize = HDR_GET_PSIZE(hdr);
			uint64_t asize = vdev_psize_to_asize(dev->l2ad_vdev,
			    psize);

			 
			if (write_asize + asize +
			    sizeof (l2arc_log_blk_phys_t) > target_sz) {
				full = B_TRUE;
				mutex_exit(hash_lock);
				break;
			}

			 
			arc_hdr_set_flags(hdr, ARC_FLAG_L2_WRITING);

			 
			if (HDR_HAS_RABD(hdr) && psize == asize) {
				to_write = hdr->b_crypt_hdr.b_rabd;
			} else if ((HDR_COMPRESSION_ENABLED(hdr) ||
			    HDR_GET_COMPRESS(hdr) == ZIO_COMPRESS_OFF) &&
			    !HDR_ENCRYPTED(hdr) && !HDR_SHARED_DATA(hdr) &&
			    psize == asize) {
				to_write = hdr->b_l1hdr.b_pabd;
			} else {
				int ret;
				arc_buf_contents_t type = arc_buf_type(hdr);

				ret = l2arc_apply_transforms(spa, hdr, asize,
				    &to_write);
				if (ret != 0) {
					arc_hdr_clear_flags(hdr,
					    ARC_FLAG_L2_WRITING);
					mutex_exit(hash_lock);
					continue;
				}

				l2arc_free_abd_on_write(to_write, asize, type);
			}

			if (pio == NULL) {
				 
				mutex_enter(&dev->l2ad_mtx);
				list_insert_head(&dev->l2ad_buflist, head);
				mutex_exit(&dev->l2ad_mtx);

				cb = kmem_alloc(
				    sizeof (l2arc_write_callback_t), KM_SLEEP);
				cb->l2wcb_dev = dev;
				cb->l2wcb_head = head;
				 
				list_create(&cb->l2wcb_abd_list,
				    sizeof (l2arc_lb_abd_buf_t),
				    offsetof(l2arc_lb_abd_buf_t, node));
				pio = zio_root(spa, l2arc_write_done, cb,
				    ZIO_FLAG_CANFAIL);
			}

			hdr->b_l2hdr.b_dev = dev;
			hdr->b_l2hdr.b_hits = 0;

			hdr->b_l2hdr.b_daddr = dev->l2ad_hand;
			hdr->b_l2hdr.b_arcs_state =
			    hdr->b_l1hdr.b_state->arcs_state;
			arc_hdr_set_flags(hdr, ARC_FLAG_HAS_L2HDR);

			mutex_enter(&dev->l2ad_mtx);
			list_insert_head(&dev->l2ad_buflist, hdr);
			mutex_exit(&dev->l2ad_mtx);

			(void) zfs_refcount_add_many(&dev->l2ad_alloc,
			    arc_hdr_size(hdr), hdr);

			wzio = zio_write_phys(pio, dev->l2ad_vdev,
			    hdr->b_l2hdr.b_daddr, asize, to_write,
			    ZIO_CHECKSUM_OFF, NULL, hdr,
			    ZIO_PRIORITY_ASYNC_WRITE,
			    ZIO_FLAG_CANFAIL, B_FALSE);

			write_lsize += HDR_GET_LSIZE(hdr);
			DTRACE_PROBE2(l2arc__write, vdev_t *, dev->l2ad_vdev,
			    zio_t *, wzio);

			write_psize += psize;
			write_asize += asize;
			dev->l2ad_hand += asize;
			l2arc_hdr_arcstats_increment(hdr);
			vdev_space_update(dev->l2ad_vdev, asize, 0, 0);

			mutex_exit(hash_lock);

			 
			if (l2arc_log_blk_insert(dev, hdr)) {
				 
				write_asize +=
				    l2arc_log_blk_commit(dev, pio, cb);
			}

			zio_nowait(wzio);
		}

		multilist_sublist_unlock(mls);

		if (full == B_TRUE)
			break;
	}

	 
	if (pio == NULL) {
		ASSERT0(write_lsize);
		ASSERT(!HDR_HAS_L1HDR(head));
		kmem_cache_free(hdr_l2only_cache, head);

		 
		if (dev->l2ad_evict != l2dhdr->dh_evict)
			l2arc_dev_hdr_update(dev);

		return (0);
	}

	if (!dev->l2ad_first)
		ASSERT3U(dev->l2ad_hand, <=, dev->l2ad_evict);

	ASSERT3U(write_asize, <=, target_sz);
	ARCSTAT_BUMP(arcstat_l2_writes_sent);
	ARCSTAT_INCR(arcstat_l2_write_bytes, write_psize);

	dev->l2ad_writing = B_TRUE;
	(void) zio_wait(pio);
	dev->l2ad_writing = B_FALSE;

	 
	l2arc_dev_hdr_update(dev);

	return (write_asize);
}

static boolean_t
l2arc_hdr_limit_reached(void)
{
	int64_t s = aggsum_upper_bound(&arc_sums.arcstat_l2_hdr_size);

	return (arc_reclaim_needed() ||
	    (s > (arc_warm ? arc_c : arc_c_max) * l2arc_meta_percent / 100));
}

 
static  __attribute__((noreturn)) void
l2arc_feed_thread(void *unused)
{
	(void) unused;
	callb_cpr_t cpr;
	l2arc_dev_t *dev;
	spa_t *spa;
	uint64_t size, wrote;
	clock_t begin, next = ddi_get_lbolt();
	fstrans_cookie_t cookie;

	CALLB_CPR_INIT(&cpr, &l2arc_feed_thr_lock, callb_generic_cpr, FTAG);

	mutex_enter(&l2arc_feed_thr_lock);

	cookie = spl_fstrans_mark();
	while (l2arc_thread_exit == 0) {
		CALLB_CPR_SAFE_BEGIN(&cpr);
		(void) cv_timedwait_idle(&l2arc_feed_thr_cv,
		    &l2arc_feed_thr_lock, next);
		CALLB_CPR_SAFE_END(&cpr, &l2arc_feed_thr_lock);
		next = ddi_get_lbolt() + hz;

		 
		mutex_enter(&l2arc_dev_mtx);
		if (l2arc_ndev == 0) {
			mutex_exit(&l2arc_dev_mtx);
			continue;
		}
		mutex_exit(&l2arc_dev_mtx);
		begin = ddi_get_lbolt();

		 
		if ((dev = l2arc_dev_get_next()) == NULL)
			continue;

		spa = dev->l2ad_spa;
		ASSERT3P(spa, !=, NULL);

		 
		if (!spa_writeable(spa)) {
			next = ddi_get_lbolt() + 5 * l2arc_feed_secs * hz;
			spa_config_exit(spa, SCL_L2ARC, dev);
			continue;
		}

		 
		if (l2arc_hdr_limit_reached()) {
			ARCSTAT_BUMP(arcstat_l2_abort_lowmem);
			spa_config_exit(spa, SCL_L2ARC, dev);
			continue;
		}

		ARCSTAT_BUMP(arcstat_l2_feeds);

		size = l2arc_write_size(dev);

		 
		l2arc_evict(dev, size, B_FALSE);

		 
		wrote = l2arc_write_buffers(spa, dev, size);

		 
		next = l2arc_write_interval(begin, size, wrote);
		spa_config_exit(spa, SCL_L2ARC, dev);
	}
	spl_fstrans_unmark(cookie);

	l2arc_thread_exit = 0;
	cv_broadcast(&l2arc_feed_thr_cv);
	CALLB_CPR_EXIT(&cpr);		 
	thread_exit();
}

boolean_t
l2arc_vdev_present(vdev_t *vd)
{
	return (l2arc_vdev_get(vd) != NULL);
}

 
l2arc_dev_t *
l2arc_vdev_get(vdev_t *vd)
{
	l2arc_dev_t	*dev;

	mutex_enter(&l2arc_dev_mtx);
	for (dev = list_head(l2arc_dev_list); dev != NULL;
	    dev = list_next(l2arc_dev_list, dev)) {
		if (dev->l2ad_vdev == vd)
			break;
	}
	mutex_exit(&l2arc_dev_mtx);

	return (dev);
}

static void
l2arc_rebuild_dev(l2arc_dev_t *dev, boolean_t reopen)
{
	l2arc_dev_hdr_phys_t *l2dhdr = dev->l2ad_dev_hdr;
	uint64_t l2dhdr_asize = dev->l2ad_dev_hdr_asize;
	spa_t *spa = dev->l2ad_spa;

	 
	if (dev->l2ad_end < l2arc_rebuild_blocks_min_l2size) {
		dev->l2ad_log_entries = 0;
	} else {
		dev->l2ad_log_entries = MIN((dev->l2ad_end -
		    dev->l2ad_start) >> SPA_MAXBLOCKSHIFT,
		    L2ARC_LOG_BLK_MAX_ENTRIES);
	}

	 
	if (l2arc_dev_hdr_read(dev) == 0 && dev->l2ad_log_entries > 0) {
		 
		if (reopen) {
			if (!l2arc_rebuild_enabled) {
				return;
			} else {
				l2arc_evict(dev, 0, B_TRUE);
				 
				dev->l2ad_log_ent_idx = 0;
				dev->l2ad_log_blk_payload_asize = 0;
				dev->l2ad_log_blk_payload_start = 0;
			}
		}
		 
		dev->l2ad_rebuild = B_TRUE;
	} else if (spa_writeable(spa)) {
		 
		if (l2arc_trim_ahead > 0) {
			dev->l2ad_trim_all = B_TRUE;
		} else {
			memset(l2dhdr, 0, l2dhdr_asize);
			l2arc_dev_hdr_update(dev);
		}
	}
}

 
void
l2arc_add_vdev(spa_t *spa, vdev_t *vd)
{
	l2arc_dev_t		*adddev;
	uint64_t		l2dhdr_asize;

	ASSERT(!l2arc_vdev_present(vd));

	 
	adddev = vmem_zalloc(sizeof (l2arc_dev_t), KM_SLEEP);
	adddev->l2ad_spa = spa;
	adddev->l2ad_vdev = vd;
	 
	l2dhdr_asize = adddev->l2ad_dev_hdr_asize =
	    MAX(sizeof (*adddev->l2ad_dev_hdr), 1 << vd->vdev_ashift);
	adddev->l2ad_start = VDEV_LABEL_START_SIZE + l2dhdr_asize;
	adddev->l2ad_end = VDEV_LABEL_START_SIZE + vdev_get_min_asize(vd);
	ASSERT3U(adddev->l2ad_start, <, adddev->l2ad_end);
	adddev->l2ad_hand = adddev->l2ad_start;
	adddev->l2ad_evict = adddev->l2ad_start;
	adddev->l2ad_first = B_TRUE;
	adddev->l2ad_writing = B_FALSE;
	adddev->l2ad_trim_all = B_FALSE;
	list_link_init(&adddev->l2ad_node);
	adddev->l2ad_dev_hdr = kmem_zalloc(l2dhdr_asize, KM_SLEEP);

	mutex_init(&adddev->l2ad_mtx, NULL, MUTEX_DEFAULT, NULL);
	 
	list_create(&adddev->l2ad_buflist, sizeof (arc_buf_hdr_t),
	    offsetof(arc_buf_hdr_t, b_l2hdr.b_l2node));

	 
	list_create(&adddev->l2ad_lbptr_list, sizeof (l2arc_lb_ptr_buf_t),
	    offsetof(l2arc_lb_ptr_buf_t, node));

	vdev_space_update(vd, 0, 0, adddev->l2ad_end - adddev->l2ad_hand);
	zfs_refcount_create(&adddev->l2ad_alloc);
	zfs_refcount_create(&adddev->l2ad_lb_asize);
	zfs_refcount_create(&adddev->l2ad_lb_count);

	 
	l2arc_rebuild_dev(adddev, B_FALSE);

	 
	mutex_enter(&l2arc_dev_mtx);
	list_insert_head(l2arc_dev_list, adddev);
	atomic_inc_64(&l2arc_ndev);
	mutex_exit(&l2arc_dev_mtx);
}

 
void
l2arc_rebuild_vdev(vdev_t *vd, boolean_t reopen)
{
	l2arc_dev_t		*dev = NULL;

	dev = l2arc_vdev_get(vd);
	ASSERT3P(dev, !=, NULL);

	 
	l2arc_rebuild_dev(dev, reopen);
}

 
void
l2arc_remove_vdev(vdev_t *vd)
{
	l2arc_dev_t *remdev = NULL;

	 
	remdev = l2arc_vdev_get(vd);
	ASSERT3P(remdev, !=, NULL);

	 
	mutex_enter(&l2arc_rebuild_thr_lock);
	if (remdev->l2ad_rebuild_began == B_TRUE) {
		remdev->l2ad_rebuild_cancel = B_TRUE;
		while (remdev->l2ad_rebuild == B_TRUE)
			cv_wait(&l2arc_rebuild_thr_cv, &l2arc_rebuild_thr_lock);
	}
	mutex_exit(&l2arc_rebuild_thr_lock);

	 
	mutex_enter(&l2arc_dev_mtx);
	list_remove(l2arc_dev_list, remdev);
	l2arc_dev_last = NULL;		 
	atomic_dec_64(&l2arc_ndev);
	mutex_exit(&l2arc_dev_mtx);

	 
	l2arc_evict(remdev, 0, B_TRUE);
	list_destroy(&remdev->l2ad_buflist);
	ASSERT(list_is_empty(&remdev->l2ad_lbptr_list));
	list_destroy(&remdev->l2ad_lbptr_list);
	mutex_destroy(&remdev->l2ad_mtx);
	zfs_refcount_destroy(&remdev->l2ad_alloc);
	zfs_refcount_destroy(&remdev->l2ad_lb_asize);
	zfs_refcount_destroy(&remdev->l2ad_lb_count);
	kmem_free(remdev->l2ad_dev_hdr, remdev->l2ad_dev_hdr_asize);
	vmem_free(remdev, sizeof (l2arc_dev_t));
}

void
l2arc_init(void)
{
	l2arc_thread_exit = 0;
	l2arc_ndev = 0;

	mutex_init(&l2arc_feed_thr_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&l2arc_feed_thr_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&l2arc_rebuild_thr_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&l2arc_rebuild_thr_cv, NULL, CV_DEFAULT, NULL);
	mutex_init(&l2arc_dev_mtx, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&l2arc_free_on_write_mtx, NULL, MUTEX_DEFAULT, NULL);

	l2arc_dev_list = &L2ARC_dev_list;
	l2arc_free_on_write = &L2ARC_free_on_write;
	list_create(l2arc_dev_list, sizeof (l2arc_dev_t),
	    offsetof(l2arc_dev_t, l2ad_node));
	list_create(l2arc_free_on_write, sizeof (l2arc_data_free_t),
	    offsetof(l2arc_data_free_t, l2df_list_node));
}

void
l2arc_fini(void)
{
	mutex_destroy(&l2arc_feed_thr_lock);
	cv_destroy(&l2arc_feed_thr_cv);
	mutex_destroy(&l2arc_rebuild_thr_lock);
	cv_destroy(&l2arc_rebuild_thr_cv);
	mutex_destroy(&l2arc_dev_mtx);
	mutex_destroy(&l2arc_free_on_write_mtx);

	list_destroy(l2arc_dev_list);
	list_destroy(l2arc_free_on_write);
}

void
l2arc_start(void)
{
	if (!(spa_mode_global & SPA_MODE_WRITE))
		return;

	(void) thread_create(NULL, 0, l2arc_feed_thread, NULL, 0, &p0,
	    TS_RUN, defclsyspri);
}

void
l2arc_stop(void)
{
	if (!(spa_mode_global & SPA_MODE_WRITE))
		return;

	mutex_enter(&l2arc_feed_thr_lock);
	cv_signal(&l2arc_feed_thr_cv);	 
	l2arc_thread_exit = 1;
	while (l2arc_thread_exit != 0)
		cv_wait(&l2arc_feed_thr_cv, &l2arc_feed_thr_lock);
	mutex_exit(&l2arc_feed_thr_lock);
}

 
void
l2arc_spa_rebuild_start(spa_t *spa)
{
	ASSERT(MUTEX_HELD(&spa_namespace_lock));

	 
	for (int i = 0; i < spa->spa_l2cache.sav_count; i++) {
		l2arc_dev_t *dev =
		    l2arc_vdev_get(spa->spa_l2cache.sav_vdevs[i]);
		if (dev == NULL) {
			 
			continue;
		}
		mutex_enter(&l2arc_rebuild_thr_lock);
		if (dev->l2ad_rebuild && !dev->l2ad_rebuild_cancel) {
			dev->l2ad_rebuild_began = B_TRUE;
			(void) thread_create(NULL, 0, l2arc_dev_rebuild_thread,
			    dev, 0, &p0, TS_RUN, minclsyspri);
		}
		mutex_exit(&l2arc_rebuild_thr_lock);
	}
}

 
static __attribute__((noreturn)) void
l2arc_dev_rebuild_thread(void *arg)
{
	l2arc_dev_t *dev = arg;

	VERIFY(!dev->l2ad_rebuild_cancel);
	VERIFY(dev->l2ad_rebuild);
	(void) l2arc_rebuild(dev);
	mutex_enter(&l2arc_rebuild_thr_lock);
	dev->l2ad_rebuild_began = B_FALSE;
	dev->l2ad_rebuild = B_FALSE;
	mutex_exit(&l2arc_rebuild_thr_lock);

	thread_exit();
}

 
static int
l2arc_rebuild(l2arc_dev_t *dev)
{
	vdev_t			*vd = dev->l2ad_vdev;
	spa_t			*spa = vd->vdev_spa;
	int			err = 0;
	l2arc_dev_hdr_phys_t	*l2dhdr = dev->l2ad_dev_hdr;
	l2arc_log_blk_phys_t	*this_lb, *next_lb;
	zio_t			*this_io = NULL, *next_io = NULL;
	l2arc_log_blkptr_t	lbps[2];
	l2arc_lb_ptr_buf_t	*lb_ptr_buf;
	boolean_t		lock_held;

	this_lb = vmem_zalloc(sizeof (*this_lb), KM_SLEEP);
	next_lb = vmem_zalloc(sizeof (*next_lb), KM_SLEEP);

	 
	spa_config_enter(spa, SCL_L2ARC, vd, RW_READER);
	lock_held = B_TRUE;

	 
	dev->l2ad_evict = MAX(l2dhdr->dh_evict, dev->l2ad_start);
	dev->l2ad_hand = MAX(l2dhdr->dh_start_lbps[0].lbp_daddr +
	    L2BLK_GET_PSIZE((&l2dhdr->dh_start_lbps[0])->lbp_prop),
	    dev->l2ad_start);
	dev->l2ad_first = !!(l2dhdr->dh_flags & L2ARC_DEV_HDR_EVICT_FIRST);

	vd->vdev_trim_action_time = l2dhdr->dh_trim_action_time;
	vd->vdev_trim_state = l2dhdr->dh_trim_state;

	 
	if (!l2arc_rebuild_enabled)
		goto out;

	 
	memcpy(lbps, l2dhdr->dh_start_lbps, sizeof (lbps));

	 
	for (;;) {
		if (!l2arc_log_blkptr_valid(dev, &lbps[0]))
			break;

		if ((err = l2arc_log_blk_read(dev, &lbps[0], &lbps[1],
		    this_lb, next_lb, this_io, &next_io)) != 0)
			goto out;

		 
		if (l2arc_hdr_limit_reached()) {
			ARCSTAT_BUMP(arcstat_l2_rebuild_abort_lowmem);
			cmn_err(CE_NOTE, "System running low on memory, "
			    "aborting L2ARC rebuild.");
			err = SET_ERROR(ENOMEM);
			goto out;
		}

		spa_config_exit(spa, SCL_L2ARC, vd);
		lock_held = B_FALSE;

		 
		uint64_t asize = L2BLK_GET_PSIZE((&lbps[0])->lbp_prop);
		l2arc_log_blk_restore(dev, this_lb, asize);

		 
		lb_ptr_buf = kmem_zalloc(sizeof (l2arc_lb_ptr_buf_t), KM_SLEEP);
		lb_ptr_buf->lb_ptr = kmem_zalloc(sizeof (l2arc_log_blkptr_t),
		    KM_SLEEP);
		memcpy(lb_ptr_buf->lb_ptr, &lbps[0],
		    sizeof (l2arc_log_blkptr_t));
		mutex_enter(&dev->l2ad_mtx);
		list_insert_tail(&dev->l2ad_lbptr_list, lb_ptr_buf);
		ARCSTAT_INCR(arcstat_l2_log_blk_asize, asize);
		ARCSTAT_BUMP(arcstat_l2_log_blk_count);
		zfs_refcount_add_many(&dev->l2ad_lb_asize, asize, lb_ptr_buf);
		zfs_refcount_add(&dev->l2ad_lb_count, lb_ptr_buf);
		mutex_exit(&dev->l2ad_mtx);
		vdev_space_update(vd, asize, 0, 0);

		 
		if (l2arc_range_check_overlap(lbps[1].lbp_payload_start,
		    lbps[0].lbp_payload_start, dev->l2ad_evict) &&
		    !dev->l2ad_first)
			goto out;

		kpreempt(KPREEMPT_SYNC);
		for (;;) {
			mutex_enter(&l2arc_rebuild_thr_lock);
			if (dev->l2ad_rebuild_cancel) {
				dev->l2ad_rebuild = B_FALSE;
				cv_signal(&l2arc_rebuild_thr_cv);
				mutex_exit(&l2arc_rebuild_thr_lock);
				err = SET_ERROR(ECANCELED);
				goto out;
			}
			mutex_exit(&l2arc_rebuild_thr_lock);
			if (spa_config_tryenter(spa, SCL_L2ARC, vd,
			    RW_READER)) {
				lock_held = B_TRUE;
				break;
			}
			 
			delay(1);
		}

		 
		lbps[0] = lbps[1];
		lbps[1] = this_lb->lb_prev_lbp;
		PTR_SWAP(this_lb, next_lb);
		this_io = next_io;
		next_io = NULL;
	}

	if (this_io != NULL)
		l2arc_log_blk_fetch_abort(this_io);
out:
	if (next_io != NULL)
		l2arc_log_blk_fetch_abort(next_io);
	vmem_free(this_lb, sizeof (*this_lb));
	vmem_free(next_lb, sizeof (*next_lb));

	if (!l2arc_rebuild_enabled) {
		spa_history_log_internal(spa, "L2ARC rebuild", NULL,
		    "disabled");
	} else if (err == 0 && zfs_refcount_count(&dev->l2ad_lb_count) > 0) {
		ARCSTAT_BUMP(arcstat_l2_rebuild_success);
		spa_history_log_internal(spa, "L2ARC rebuild", NULL,
		    "successful, restored %llu blocks",
		    (u_longlong_t)zfs_refcount_count(&dev->l2ad_lb_count));
	} else if (err == 0 && zfs_refcount_count(&dev->l2ad_lb_count) == 0) {
		 
		spa_history_log_internal(spa, "L2ARC rebuild", NULL,
		    "no valid log blocks");
		memset(l2dhdr, 0, dev->l2ad_dev_hdr_asize);
		l2arc_dev_hdr_update(dev);
	} else if (err == ECANCELED) {
		 
		zfs_dbgmsg("L2ARC rebuild aborted, restored %llu blocks",
		    (u_longlong_t)zfs_refcount_count(&dev->l2ad_lb_count));
	} else if (err != 0) {
		spa_history_log_internal(spa, "L2ARC rebuild", NULL,
		    "aborted, restored %llu blocks",
		    (u_longlong_t)zfs_refcount_count(&dev->l2ad_lb_count));
	}

	if (lock_held)
		spa_config_exit(spa, SCL_L2ARC, vd);

	return (err);
}

 
static int
l2arc_dev_hdr_read(l2arc_dev_t *dev)
{
	int			err;
	uint64_t		guid;
	l2arc_dev_hdr_phys_t	*l2dhdr = dev->l2ad_dev_hdr;
	const uint64_t		l2dhdr_asize = dev->l2ad_dev_hdr_asize;
	abd_t 			*abd;

	guid = spa_guid(dev->l2ad_vdev->vdev_spa);

	abd = abd_get_from_buf(l2dhdr, l2dhdr_asize);

	err = zio_wait(zio_read_phys(NULL, dev->l2ad_vdev,
	    VDEV_LABEL_START_SIZE, l2dhdr_asize, abd,
	    ZIO_CHECKSUM_LABEL, NULL, NULL, ZIO_PRIORITY_SYNC_READ,
	    ZIO_FLAG_CANFAIL | ZIO_FLAG_DONT_PROPAGATE | ZIO_FLAG_DONT_RETRY |
	    ZIO_FLAG_SPECULATIVE, B_FALSE));

	abd_free(abd);

	if (err != 0) {
		ARCSTAT_BUMP(arcstat_l2_rebuild_abort_dh_errors);
		zfs_dbgmsg("L2ARC IO error (%d) while reading device header, "
		    "vdev guid: %llu", err,
		    (u_longlong_t)dev->l2ad_vdev->vdev_guid);
		return (err);
	}

	if (l2dhdr->dh_magic == BSWAP_64(L2ARC_DEV_HDR_MAGIC))
		byteswap_uint64_array(l2dhdr, sizeof (*l2dhdr));

	if (l2dhdr->dh_magic != L2ARC_DEV_HDR_MAGIC ||
	    l2dhdr->dh_spa_guid != guid ||
	    l2dhdr->dh_vdev_guid != dev->l2ad_vdev->vdev_guid ||
	    l2dhdr->dh_version != L2ARC_PERSISTENT_VERSION ||
	    l2dhdr->dh_log_entries != dev->l2ad_log_entries ||
	    l2dhdr->dh_end != dev->l2ad_end ||
	    !l2arc_range_check_overlap(dev->l2ad_start, dev->l2ad_end,
	    l2dhdr->dh_evict) ||
	    (l2dhdr->dh_trim_state != VDEV_TRIM_COMPLETE &&
	    l2arc_trim_ahead > 0)) {
		 
		ARCSTAT_BUMP(arcstat_l2_rebuild_abort_unsupported);
		return (SET_ERROR(ENOTSUP));
	}

	return (0);
}

 
static int
l2arc_log_blk_read(l2arc_dev_t *dev,
    const l2arc_log_blkptr_t *this_lbp, const l2arc_log_blkptr_t *next_lbp,
    l2arc_log_blk_phys_t *this_lb, l2arc_log_blk_phys_t *next_lb,
    zio_t *this_io, zio_t **next_io)
{
	int		err = 0;
	zio_cksum_t	cksum;
	abd_t		*abd = NULL;
	uint64_t	asize;

	ASSERT(this_lbp != NULL && next_lbp != NULL);
	ASSERT(this_lb != NULL && next_lb != NULL);
	ASSERT(next_io != NULL && *next_io == NULL);
	ASSERT(l2arc_log_blkptr_valid(dev, this_lbp));

	 
	if (this_io == NULL) {
		this_io = l2arc_log_blk_fetch(dev->l2ad_vdev, this_lbp,
		    this_lb);
	}

	 
	if (l2arc_log_blkptr_valid(dev, next_lbp)) {
		 
		*next_io = l2arc_log_blk_fetch(dev->l2ad_vdev, next_lbp,
		    next_lb);
	}

	 
	if ((err = zio_wait(this_io)) != 0) {
		ARCSTAT_BUMP(arcstat_l2_rebuild_abort_io_errors);
		zfs_dbgmsg("L2ARC IO error (%d) while reading log block, "
		    "offset: %llu, vdev guid: %llu", err,
		    (u_longlong_t)this_lbp->lbp_daddr,
		    (u_longlong_t)dev->l2ad_vdev->vdev_guid);
		goto cleanup;
	}

	 
	asize = L2BLK_GET_PSIZE((this_lbp)->lbp_prop);
	fletcher_4_native(this_lb, asize, NULL, &cksum);
	if (!ZIO_CHECKSUM_EQUAL(cksum, this_lbp->lbp_cksum)) {
		ARCSTAT_BUMP(arcstat_l2_rebuild_abort_cksum_lb_errors);
		zfs_dbgmsg("L2ARC log block cksum failed, offset: %llu, "
		    "vdev guid: %llu, l2ad_hand: %llu, l2ad_evict: %llu",
		    (u_longlong_t)this_lbp->lbp_daddr,
		    (u_longlong_t)dev->l2ad_vdev->vdev_guid,
		    (u_longlong_t)dev->l2ad_hand,
		    (u_longlong_t)dev->l2ad_evict);
		err = SET_ERROR(ECKSUM);
		goto cleanup;
	}

	 
	switch (L2BLK_GET_COMPRESS((this_lbp)->lbp_prop)) {
	case ZIO_COMPRESS_OFF:
		break;
	case ZIO_COMPRESS_LZ4:
		abd = abd_alloc_for_io(asize, B_TRUE);
		abd_copy_from_buf_off(abd, this_lb, 0, asize);
		if ((err = zio_decompress_data(
		    L2BLK_GET_COMPRESS((this_lbp)->lbp_prop),
		    abd, this_lb, asize, sizeof (*this_lb), NULL)) != 0) {
			err = SET_ERROR(EINVAL);
			goto cleanup;
		}
		break;
	default:
		err = SET_ERROR(EINVAL);
		goto cleanup;
	}
	if (this_lb->lb_magic == BSWAP_64(L2ARC_LOG_BLK_MAGIC))
		byteswap_uint64_array(this_lb, sizeof (*this_lb));
	if (this_lb->lb_magic != L2ARC_LOG_BLK_MAGIC) {
		err = SET_ERROR(EINVAL);
		goto cleanup;
	}
cleanup:
	 
	if (err != 0 && *next_io != NULL) {
		l2arc_log_blk_fetch_abort(*next_io);
		*next_io = NULL;
	}
	if (abd != NULL)
		abd_free(abd);
	return (err);
}

 
static void
l2arc_log_blk_restore(l2arc_dev_t *dev, const l2arc_log_blk_phys_t *lb,
    uint64_t lb_asize)
{
	uint64_t	size = 0, asize = 0;
	uint64_t	log_entries = dev->l2ad_log_entries;

	 
	arc_adapt(log_entries * HDR_L2ONLY_SIZE);

	for (int i = log_entries - 1; i >= 0; i--) {
		 
		size += L2BLK_GET_LSIZE((&lb->lb_entries[i])->le_prop);
		asize += vdev_psize_to_asize(dev->l2ad_vdev,
		    L2BLK_GET_PSIZE((&lb->lb_entries[i])->le_prop));
		l2arc_hdr_restore(&lb->lb_entries[i], dev);
	}

	 
	ARCSTAT_INCR(arcstat_l2_rebuild_size, size);
	ARCSTAT_INCR(arcstat_l2_rebuild_asize, asize);
	ARCSTAT_INCR(arcstat_l2_rebuild_bufs, log_entries);
	ARCSTAT_F_AVG(arcstat_l2_log_blk_avg_asize, lb_asize);
	ARCSTAT_F_AVG(arcstat_l2_data_to_meta_ratio, asize / lb_asize);
	ARCSTAT_BUMP(arcstat_l2_rebuild_log_blks);
}

 
static void
l2arc_hdr_restore(const l2arc_log_ent_phys_t *le, l2arc_dev_t *dev)
{
	arc_buf_hdr_t		*hdr, *exists;
	kmutex_t		*hash_lock;
	arc_buf_contents_t	type = L2BLK_GET_TYPE((le)->le_prop);
	uint64_t		asize;

	 
	hdr = arc_buf_alloc_l2only(L2BLK_GET_LSIZE((le)->le_prop), type,
	    dev, le->le_dva, le->le_daddr,
	    L2BLK_GET_PSIZE((le)->le_prop), le->le_birth,
	    L2BLK_GET_COMPRESS((le)->le_prop), le->le_complevel,
	    L2BLK_GET_PROTECTED((le)->le_prop),
	    L2BLK_GET_PREFETCH((le)->le_prop),
	    L2BLK_GET_STATE((le)->le_prop));
	asize = vdev_psize_to_asize(dev->l2ad_vdev,
	    L2BLK_GET_PSIZE((le)->le_prop));

	 
	l2arc_hdr_arcstats_increment(hdr);
	vdev_space_update(dev->l2ad_vdev, asize, 0, 0);

	mutex_enter(&dev->l2ad_mtx);
	list_insert_tail(&dev->l2ad_buflist, hdr);
	(void) zfs_refcount_add_many(&dev->l2ad_alloc, arc_hdr_size(hdr), hdr);
	mutex_exit(&dev->l2ad_mtx);

	exists = buf_hash_insert(hdr, &hash_lock);
	if (exists) {
		 
		arc_hdr_destroy(hdr);
		 
		if (!HDR_HAS_L2HDR(exists)) {
			arc_hdr_set_flags(exists, ARC_FLAG_HAS_L2HDR);
			exists->b_l2hdr.b_dev = dev;
			exists->b_l2hdr.b_daddr = le->le_daddr;
			exists->b_l2hdr.b_arcs_state =
			    L2BLK_GET_STATE((le)->le_prop);
			mutex_enter(&dev->l2ad_mtx);
			list_insert_tail(&dev->l2ad_buflist, exists);
			(void) zfs_refcount_add_many(&dev->l2ad_alloc,
			    arc_hdr_size(exists), exists);
			mutex_exit(&dev->l2ad_mtx);
			l2arc_hdr_arcstats_increment(exists);
			vdev_space_update(dev->l2ad_vdev, asize, 0, 0);
		}
		ARCSTAT_BUMP(arcstat_l2_rebuild_bufs_precached);
	}

	mutex_exit(hash_lock);
}

 
static zio_t *
l2arc_log_blk_fetch(vdev_t *vd, const l2arc_log_blkptr_t *lbp,
    l2arc_log_blk_phys_t *lb)
{
	uint32_t		asize;
	zio_t			*pio;
	l2arc_read_callback_t	*cb;

	 
	asize = L2BLK_GET_PSIZE((lbp)->lbp_prop);
	ASSERT(asize <= sizeof (l2arc_log_blk_phys_t));

	cb = kmem_zalloc(sizeof (l2arc_read_callback_t), KM_SLEEP);
	cb->l2rcb_abd = abd_get_from_buf(lb, asize);
	pio = zio_root(vd->vdev_spa, l2arc_blk_fetch_done, cb,
	    ZIO_FLAG_CANFAIL | ZIO_FLAG_DONT_PROPAGATE | ZIO_FLAG_DONT_RETRY);
	(void) zio_nowait(zio_read_phys(pio, vd, lbp->lbp_daddr, asize,
	    cb->l2rcb_abd, ZIO_CHECKSUM_OFF, NULL, NULL,
	    ZIO_PRIORITY_ASYNC_READ, ZIO_FLAG_CANFAIL |
	    ZIO_FLAG_DONT_PROPAGATE | ZIO_FLAG_DONT_RETRY, B_FALSE));

	return (pio);
}

 
static void
l2arc_log_blk_fetch_abort(zio_t *zio)
{
	(void) zio_wait(zio);
}

 
void
l2arc_dev_hdr_update(l2arc_dev_t *dev)
{
	l2arc_dev_hdr_phys_t	*l2dhdr = dev->l2ad_dev_hdr;
	const uint64_t		l2dhdr_asize = dev->l2ad_dev_hdr_asize;
	abd_t			*abd;
	int			err;

	VERIFY(spa_config_held(dev->l2ad_spa, SCL_STATE_ALL, RW_READER));

	l2dhdr->dh_magic = L2ARC_DEV_HDR_MAGIC;
	l2dhdr->dh_version = L2ARC_PERSISTENT_VERSION;
	l2dhdr->dh_spa_guid = spa_guid(dev->l2ad_vdev->vdev_spa);
	l2dhdr->dh_vdev_guid = dev->l2ad_vdev->vdev_guid;
	l2dhdr->dh_log_entries = dev->l2ad_log_entries;
	l2dhdr->dh_evict = dev->l2ad_evict;
	l2dhdr->dh_start = dev->l2ad_start;
	l2dhdr->dh_end = dev->l2ad_end;
	l2dhdr->dh_lb_asize = zfs_refcount_count(&dev->l2ad_lb_asize);
	l2dhdr->dh_lb_count = zfs_refcount_count(&dev->l2ad_lb_count);
	l2dhdr->dh_flags = 0;
	l2dhdr->dh_trim_action_time = dev->l2ad_vdev->vdev_trim_action_time;
	l2dhdr->dh_trim_state = dev->l2ad_vdev->vdev_trim_state;
	if (dev->l2ad_first)
		l2dhdr->dh_flags |= L2ARC_DEV_HDR_EVICT_FIRST;

	abd = abd_get_from_buf(l2dhdr, l2dhdr_asize);

	err = zio_wait(zio_write_phys(NULL, dev->l2ad_vdev,
	    VDEV_LABEL_START_SIZE, l2dhdr_asize, abd, ZIO_CHECKSUM_LABEL, NULL,
	    NULL, ZIO_PRIORITY_ASYNC_WRITE, ZIO_FLAG_CANFAIL, B_FALSE));

	abd_free(abd);

	if (err != 0) {
		zfs_dbgmsg("L2ARC IO error (%d) while writing device header, "
		    "vdev guid: %llu", err,
		    (u_longlong_t)dev->l2ad_vdev->vdev_guid);
	}
}

 
static uint64_t
l2arc_log_blk_commit(l2arc_dev_t *dev, zio_t *pio, l2arc_write_callback_t *cb)
{
	l2arc_log_blk_phys_t	*lb = &dev->l2ad_log_blk;
	l2arc_dev_hdr_phys_t	*l2dhdr = dev->l2ad_dev_hdr;
	uint64_t		psize, asize;
	zio_t			*wzio;
	l2arc_lb_abd_buf_t	*abd_buf;
	uint8_t			*tmpbuf = NULL;
	l2arc_lb_ptr_buf_t	*lb_ptr_buf;

	VERIFY3S(dev->l2ad_log_ent_idx, ==, dev->l2ad_log_entries);

	abd_buf = zio_buf_alloc(sizeof (*abd_buf));
	abd_buf->abd = abd_get_from_buf(lb, sizeof (*lb));
	lb_ptr_buf = kmem_zalloc(sizeof (l2arc_lb_ptr_buf_t), KM_SLEEP);
	lb_ptr_buf->lb_ptr = kmem_zalloc(sizeof (l2arc_log_blkptr_t), KM_SLEEP);

	 
	lb->lb_prev_lbp = l2dhdr->dh_start_lbps[1];
	lb->lb_magic = L2ARC_LOG_BLK_MAGIC;

	 
	list_insert_tail(&cb->l2wcb_abd_list, abd_buf);

	 
	psize = zio_compress_data(ZIO_COMPRESS_LZ4,
	    abd_buf->abd, (void **) &tmpbuf, sizeof (*lb), 0);

	 
	ASSERT(psize != 0);
	asize = vdev_psize_to_asize(dev->l2ad_vdev, psize);
	ASSERT(asize <= sizeof (*lb));

	 
	l2dhdr->dh_start_lbps[1] = l2dhdr->dh_start_lbps[0];
	l2dhdr->dh_start_lbps[0].lbp_daddr = dev->l2ad_hand;
	l2dhdr->dh_start_lbps[0].lbp_payload_asize =
	    dev->l2ad_log_blk_payload_asize;
	l2dhdr->dh_start_lbps[0].lbp_payload_start =
	    dev->l2ad_log_blk_payload_start;
	L2BLK_SET_LSIZE(
	    (&l2dhdr->dh_start_lbps[0])->lbp_prop, sizeof (*lb));
	L2BLK_SET_PSIZE(
	    (&l2dhdr->dh_start_lbps[0])->lbp_prop, asize);
	L2BLK_SET_CHECKSUM(
	    (&l2dhdr->dh_start_lbps[0])->lbp_prop,
	    ZIO_CHECKSUM_FLETCHER_4);
	if (asize < sizeof (*lb)) {
		 
		memset(tmpbuf + psize, 0, asize - psize);
		L2BLK_SET_COMPRESS(
		    (&l2dhdr->dh_start_lbps[0])->lbp_prop,
		    ZIO_COMPRESS_LZ4);
	} else {
		 
		memcpy(tmpbuf, lb, sizeof (*lb));
		L2BLK_SET_COMPRESS(
		    (&l2dhdr->dh_start_lbps[0])->lbp_prop,
		    ZIO_COMPRESS_OFF);
	}

	 
	fletcher_4_native(tmpbuf, asize, NULL,
	    &l2dhdr->dh_start_lbps[0].lbp_cksum);

	abd_free(abd_buf->abd);

	 
	abd_buf->abd = abd_get_from_buf(tmpbuf, sizeof (*lb));
	abd_take_ownership_of_buf(abd_buf->abd, B_TRUE);
	wzio = zio_write_phys(pio, dev->l2ad_vdev, dev->l2ad_hand,
	    asize, abd_buf->abd, ZIO_CHECKSUM_OFF, NULL, NULL,
	    ZIO_PRIORITY_ASYNC_WRITE, ZIO_FLAG_CANFAIL, B_FALSE);
	DTRACE_PROBE2(l2arc__write, vdev_t *, dev->l2ad_vdev, zio_t *, wzio);
	(void) zio_nowait(wzio);

	dev->l2ad_hand += asize;
	 
	memcpy(lb_ptr_buf->lb_ptr, &l2dhdr->dh_start_lbps[0],
	    sizeof (l2arc_log_blkptr_t));
	mutex_enter(&dev->l2ad_mtx);
	list_insert_head(&dev->l2ad_lbptr_list, lb_ptr_buf);
	ARCSTAT_INCR(arcstat_l2_log_blk_asize, asize);
	ARCSTAT_BUMP(arcstat_l2_log_blk_count);
	zfs_refcount_add_many(&dev->l2ad_lb_asize, asize, lb_ptr_buf);
	zfs_refcount_add(&dev->l2ad_lb_count, lb_ptr_buf);
	mutex_exit(&dev->l2ad_mtx);
	vdev_space_update(dev->l2ad_vdev, asize, 0, 0);

	 
	ARCSTAT_INCR(arcstat_l2_write_bytes, asize);
	ARCSTAT_BUMP(arcstat_l2_log_blk_writes);
	ARCSTAT_F_AVG(arcstat_l2_log_blk_avg_asize, asize);
	ARCSTAT_F_AVG(arcstat_l2_data_to_meta_ratio,
	    dev->l2ad_log_blk_payload_asize / asize);

	 
	dev->l2ad_log_ent_idx = 0;
	dev->l2ad_log_blk_payload_asize = 0;
	dev->l2ad_log_blk_payload_start = 0;

	return (asize);
}

 
boolean_t
l2arc_log_blkptr_valid(l2arc_dev_t *dev, const l2arc_log_blkptr_t *lbp)
{
	 
	uint64_t asize = L2BLK_GET_PSIZE((lbp)->lbp_prop);
	uint64_t end = lbp->lbp_daddr + asize - 1;
	uint64_t start = lbp->lbp_payload_start;
	boolean_t evicted = B_FALSE;

	 

	evicted =
	    l2arc_range_check_overlap(start, end, dev->l2ad_hand) ||
	    l2arc_range_check_overlap(start, end, dev->l2ad_evict) ||
	    l2arc_range_check_overlap(dev->l2ad_hand, dev->l2ad_evict, start) ||
	    l2arc_range_check_overlap(dev->l2ad_hand, dev->l2ad_evict, end);

	return (start >= dev->l2ad_start && end <= dev->l2ad_end &&
	    asize > 0 && asize <= sizeof (l2arc_log_blk_phys_t) &&
	    (!evicted || dev->l2ad_first));
}

 
static boolean_t
l2arc_log_blk_insert(l2arc_dev_t *dev, const arc_buf_hdr_t *hdr)
{
	l2arc_log_blk_phys_t	*lb = &dev->l2ad_log_blk;
	l2arc_log_ent_phys_t	*le;

	if (dev->l2ad_log_entries == 0)
		return (B_FALSE);

	int index = dev->l2ad_log_ent_idx++;

	ASSERT3S(index, <, dev->l2ad_log_entries);
	ASSERT(HDR_HAS_L2HDR(hdr));

	le = &lb->lb_entries[index];
	memset(le, 0, sizeof (*le));
	le->le_dva = hdr->b_dva;
	le->le_birth = hdr->b_birth;
	le->le_daddr = hdr->b_l2hdr.b_daddr;
	if (index == 0)
		dev->l2ad_log_blk_payload_start = le->le_daddr;
	L2BLK_SET_LSIZE((le)->le_prop, HDR_GET_LSIZE(hdr));
	L2BLK_SET_PSIZE((le)->le_prop, HDR_GET_PSIZE(hdr));
	L2BLK_SET_COMPRESS((le)->le_prop, HDR_GET_COMPRESS(hdr));
	le->le_complevel = hdr->b_complevel;
	L2BLK_SET_TYPE((le)->le_prop, hdr->b_type);
	L2BLK_SET_PROTECTED((le)->le_prop, !!(HDR_PROTECTED(hdr)));
	L2BLK_SET_PREFETCH((le)->le_prop, !!(HDR_PREFETCH(hdr)));
	L2BLK_SET_STATE((le)->le_prop, hdr->b_l1hdr.b_state->arcs_state);

	dev->l2ad_log_blk_payload_asize += vdev_psize_to_asize(dev->l2ad_vdev,
	    HDR_GET_PSIZE(hdr));

	return (dev->l2ad_log_ent_idx == dev->l2ad_log_entries);
}

 
boolean_t
l2arc_range_check_overlap(uint64_t bottom, uint64_t top, uint64_t check)
{
	if (bottom < top)
		return (bottom <= check && check <= top);
	else if (bottom > top)
		return (check <= top || bottom <= check);
	else
		return (check == top);
}

EXPORT_SYMBOL(arc_buf_size);
EXPORT_SYMBOL(arc_write);
EXPORT_SYMBOL(arc_read);
EXPORT_SYMBOL(arc_buf_info);
EXPORT_SYMBOL(arc_getbuf_func);
EXPORT_SYMBOL(arc_add_prune_callback);
EXPORT_SYMBOL(arc_remove_prune_callback);

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, min, param_set_arc_min,
	spl_param_get_u64, ZMOD_RW, "Minimum ARC size in bytes");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, max, param_set_arc_max,
	spl_param_get_u64, ZMOD_RW, "Maximum ARC size in bytes");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, meta_balance, UINT, ZMOD_RW,
	"Balance between metadata and data on ghost hits.");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, grow_retry, param_set_arc_int,
	param_get_uint, ZMOD_RW, "Seconds before growing ARC size");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, shrink_shift, param_set_arc_int,
	param_get_uint, ZMOD_RW, "log2(fraction of ARC to reclaim)");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, pc_percent, UINT, ZMOD_RW,
	"Percent of pagecache to reclaim ARC to");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, average_blocksize, UINT, ZMOD_RD,
	"Target average block size");

ZFS_MODULE_PARAM(zfs, zfs_, compressed_arc_enabled, INT, ZMOD_RW,
	"Disable compressed ARC buffers");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, min_prefetch_ms, param_set_arc_int,
	param_get_uint, ZMOD_RW, "Min life of prefetch block in ms");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, min_prescient_prefetch_ms,
    param_set_arc_int, param_get_uint, ZMOD_RW,
	"Min life of prescient prefetched block in ms");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, write_max, U64, ZMOD_RW,
	"Max write bytes per interval");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, write_boost, U64, ZMOD_RW,
	"Extra write bytes during device warmup");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, headroom, U64, ZMOD_RW,
	"Number of max device writes to precache");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, headroom_boost, U64, ZMOD_RW,
	"Compressed l2arc_headroom multiplier");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, trim_ahead, U64, ZMOD_RW,
	"TRIM ahead L2ARC write size multiplier");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, feed_secs, U64, ZMOD_RW,
	"Seconds between L2ARC writing");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, feed_min_ms, U64, ZMOD_RW,
	"Min feed interval in milliseconds");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, noprefetch, INT, ZMOD_RW,
	"Skip caching prefetched buffers");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, feed_again, INT, ZMOD_RW,
	"Turbo L2ARC warmup");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, norw, INT, ZMOD_RW,
	"No reads during writes");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, meta_percent, UINT, ZMOD_RW,
	"Percent of ARC size allowed for L2ARC-only headers");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, rebuild_enabled, INT, ZMOD_RW,
	"Rebuild the L2ARC when importing a pool");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, rebuild_blocks_min_l2size, U64, ZMOD_RW,
	"Min size in bytes to write rebuild log blocks in L2ARC");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, mfuonly, INT, ZMOD_RW,
	"Cache only MFU data from ARC into L2ARC");

ZFS_MODULE_PARAM(zfs_l2arc, l2arc_, exclude_special, INT, ZMOD_RW,
	"Exclude dbufs on special vdevs from being cached to L2ARC if set.");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, lotsfree_percent, param_set_arc_int,
	param_get_uint, ZMOD_RW, "System free memory I/O throttle in bytes");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, sys_free, param_set_arc_u64,
	spl_param_get_u64, ZMOD_RW, "System free memory target size in bytes");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, dnode_limit, param_set_arc_u64,
	spl_param_get_u64, ZMOD_RW, "Minimum bytes of dnodes in ARC");

ZFS_MODULE_PARAM_CALL(zfs_arc, zfs_arc_, dnode_limit_percent,
    param_set_arc_int, param_get_uint, ZMOD_RW,
	"Percent of ARC meta buffers for dnodes");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, dnode_reduce_percent, UINT, ZMOD_RW,
	"Percentage of excess dnodes to try to unpin");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, eviction_pct, UINT, ZMOD_RW,
	"When full, ARC allocation waits for eviction of this % of alloc size");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, evict_batch_limit, UINT, ZMOD_RW,
	"The number of headers to evict per sublist before moving to the next");

ZFS_MODULE_PARAM(zfs_arc, zfs_arc_, prune_task_threads, INT, ZMOD_RW,
	"Number of arc_prune threads");

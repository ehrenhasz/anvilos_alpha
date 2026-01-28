


#ifndef _SYS_ARC_IMPL_H
#define	_SYS_ARC_IMPL_H

#include <sys/arc.h>
#include <sys/multilist.h>
#include <sys/zio_crypt.h>
#include <sys/zthr.h>
#include <sys/aggsum.h>
#include <sys/wmsum.h>

#ifdef __cplusplus
extern "C" {
#endif



typedef struct arc_state {
	
	multilist_t arcs_list[ARC_BUFC_NUMTYPES];
	
	arc_state_type_t arcs_state;
	
	zfs_refcount_t arcs_size[ARC_BUFC_NUMTYPES] ____cacheline_aligned;
	
	zfs_refcount_t arcs_esize[ARC_BUFC_NUMTYPES];
	
	wmsum_t arcs_hits[ARC_BUFC_NUMTYPES];
} arc_state_t;

typedef struct arc_callback arc_callback_t;

struct arc_callback {
	void			*acb_private;
	arc_read_done_func_t	*acb_done;
	arc_buf_t		*acb_buf;
	boolean_t		acb_encrypted;
	boolean_t		acb_compressed;
	boolean_t		acb_noauth;
	boolean_t		acb_nobuf;
	boolean_t		acb_wait;
	int			acb_wait_error;
	kmutex_t		acb_wait_lock;
	kcondvar_t		acb_wait_cv;
	zbookmark_phys_t	acb_zb;
	zio_t			*acb_zio_dummy;
	zio_t			*acb_zio_head;
	arc_callback_t		*acb_prev;
	arc_callback_t		*acb_next;
};

typedef struct arc_write_callback arc_write_callback_t;

struct arc_write_callback {
	void			*awcb_private;
	arc_write_done_func_t	*awcb_ready;
	arc_write_done_func_t	*awcb_children_ready;
	arc_write_done_func_t	*awcb_done;
	arc_buf_t		*awcb_buf;
};


typedef struct l1arc_buf_hdr {
	
	arc_state_t		*b_state;
	multilist_node_t	b_arc_node;

	
	clock_t			b_arc_access;
	uint32_t		b_mru_hits;
	uint32_t		b_mru_ghost_hits;
	uint32_t		b_mfu_hits;
	uint32_t		b_mfu_ghost_hits;
	uint8_t			b_byteswap;
	arc_buf_t		*b_buf;

	
	zfs_refcount_t		b_refcnt;

	arc_callback_t		*b_acb;
	abd_t			*b_pabd;

#ifdef ZFS_DEBUG
	zio_cksum_t		*b_freeze_cksum;
	kmutex_t		b_freeze_lock;
#endif
} l1arc_buf_hdr_t;

typedef enum l2arc_dev_hdr_flags_t {
	L2ARC_DEV_HDR_EVICT_FIRST = (1 << 0)	
} l2arc_dev_hdr_flags_t;


typedef struct l2arc_log_blkptr {
	
	uint64_t	lbp_daddr;
	
	uint64_t	lbp_payload_asize;
	
	uint64_t	lbp_payload_start;
	
	uint64_t	lbp_prop;
	zio_cksum_t	lbp_cksum;	
} l2arc_log_blkptr_t;


typedef struct l2arc_dev_hdr_phys {
	uint64_t	dh_magic;	
	uint64_t	dh_version;	

	
	uint64_t	dh_spa_guid;
	uint64_t	dh_vdev_guid;
	uint64_t	dh_log_entries;		
	uint64_t	dh_evict;		
	uint64_t	dh_flags;		
	
	uint64_t	dh_start;		
	uint64_t	dh_end;			
	
	l2arc_log_blkptr_t	dh_start_lbps[2];
	
	uint64_t	dh_lb_asize;		
	uint64_t	dh_lb_count;		
	
	uint64_t		dh_trim_action_time;
	uint64_t		dh_trim_state;
	const uint64_t		dh_pad[30];	
	zio_eck_t		dh_tail;
} l2arc_dev_hdr_phys_t;
_Static_assert(sizeof (l2arc_dev_hdr_phys_t) == SPA_MINBLOCKSIZE,
	"l2arc_dev_hdr_phys_t wrong size");


typedef struct l2arc_log_ent_phys {
	dva_t			le_dva;		
	uint64_t		le_birth;	
	
	uint64_t		le_prop;
	uint64_t		le_daddr;	
	uint64_t		le_complevel;
	
	const uint64_t		le_pad[2];	
} l2arc_log_ent_phys_t;

#define	L2ARC_LOG_BLK_MAX_ENTRIES	(1022)


typedef struct l2arc_log_blk_phys {
	uint64_t		lb_magic;	
	
	l2arc_log_blkptr_t	lb_prev_lbp;	
	
	uint64_t		lb_pad[7];
	
	l2arc_log_ent_phys_t	lb_entries[L2ARC_LOG_BLK_MAX_ENTRIES];
} l2arc_log_blk_phys_t;				


_Static_assert(IS_P2ALIGNED(sizeof (l2arc_log_blk_phys_t),
    1ULL << SPA_MINBLOCKSHIFT), "l2arc_log_blk_phys_t misaligned");
_Static_assert(sizeof (l2arc_log_blk_phys_t) >= SPA_MINBLOCKSIZE,
	"l2arc_log_blk_phys_t too small");
_Static_assert(sizeof (l2arc_log_blk_phys_t) <= SPA_MAXBLOCKSIZE,
	"l2arc_log_blk_phys_t too big");


typedef struct l2arc_lb_abd_buf {
	abd_t		*abd;
	list_node_t	node;
} l2arc_lb_abd_buf_t;


typedef struct l2arc_lb_ptr_buf {
	l2arc_log_blkptr_t	*lb_ptr;
	list_node_t		node;
} l2arc_lb_ptr_buf_t;


#define	L2BLK_GET_LSIZE(field)	\
	BF64_GET_SB((field), 0, SPA_LSIZEBITS, SPA_MINBLOCKSHIFT, 1)
#define	L2BLK_SET_LSIZE(field, x)	\
	BF64_SET_SB((field), 0, SPA_LSIZEBITS, SPA_MINBLOCKSHIFT, 1, x)
#define	L2BLK_GET_PSIZE(field)	\
	BF64_GET_SB((field), 16, SPA_PSIZEBITS, SPA_MINBLOCKSHIFT, 1)
#define	L2BLK_SET_PSIZE(field, x)	\
	BF64_SET_SB((field), 16, SPA_PSIZEBITS, SPA_MINBLOCKSHIFT, 1, x)
#define	L2BLK_GET_COMPRESS(field)	\
	BF64_GET((field), 32, SPA_COMPRESSBITS)
#define	L2BLK_SET_COMPRESS(field, x)	\
	BF64_SET((field), 32, SPA_COMPRESSBITS, x)
#define	L2BLK_GET_PREFETCH(field)	BF64_GET((field), 39, 1)
#define	L2BLK_SET_PREFETCH(field, x)	BF64_SET((field), 39, 1, x)
#define	L2BLK_GET_CHECKSUM(field)	BF64_GET((field), 40, 8)
#define	L2BLK_SET_CHECKSUM(field, x)	BF64_SET((field), 40, 8, x)

#define	L2BLK_GET_TYPE(field)		(BF64_GET((field), 48, 8) - 1)
#define	L2BLK_SET_TYPE(field, x)	BF64_SET((field), 48, 8, (x) + 1)
#define	L2BLK_GET_PROTECTED(field)	BF64_GET((field), 56, 1)
#define	L2BLK_SET_PROTECTED(field, x)	BF64_SET((field), 56, 1, x)
#define	L2BLK_GET_STATE(field)		BF64_GET((field), 57, 4)
#define	L2BLK_SET_STATE(field, x)	BF64_SET((field), 57, 4, x)

#define	PTR_SWAP(x, y)		\
	do {			\
		void *tmp = (x);\
		x = y;		\
		y = tmp;	\
	} while (0)

#define	L2ARC_DEV_HDR_MAGIC	0x5a46534341434845LLU	
#define	L2ARC_LOG_BLK_MAGIC	0x4c4f47424c4b4844LLU	


typedef struct l2arc_dev {
	vdev_t			*l2ad_vdev;	
	spa_t			*l2ad_spa;	
	uint64_t		l2ad_hand;	
	uint64_t		l2ad_start;	
	uint64_t		l2ad_end;	
	boolean_t		l2ad_first;	
	boolean_t		l2ad_writing;	
	kmutex_t		l2ad_mtx;	
	list_t			l2ad_buflist;	
	list_node_t		l2ad_node;	
	zfs_refcount_t		l2ad_alloc;	
	
	l2arc_dev_hdr_phys_t	*l2ad_dev_hdr;	
	uint64_t		l2ad_dev_hdr_asize; 
	l2arc_log_blk_phys_t	l2ad_log_blk;	
	int			l2ad_log_ent_idx; 
	
	uint64_t		l2ad_log_blk_payload_asize;
	
	uint64_t		l2ad_log_blk_payload_start;
	
	boolean_t		l2ad_rebuild;
	boolean_t		l2ad_rebuild_cancel;
	boolean_t		l2ad_rebuild_began;
	uint64_t		l2ad_log_entries;   
	uint64_t		l2ad_evict;	 
	
	list_t			l2ad_lbptr_list;
	
	zfs_refcount_t		l2ad_lb_asize;
	
	zfs_refcount_t		l2ad_lb_count;
	boolean_t		l2ad_trim_all; 
} l2arc_dev_t;


typedef struct arc_buf_hdr_crypt {
	abd_t			*b_rabd;	

	
	uint64_t		b_dsobj;

	dmu_object_type_t	b_ot;		

	
	uint8_t			b_salt[ZIO_DATA_SALT_LEN];
	uint8_t			b_iv[ZIO_DATA_IV_LEN];

	
	uint8_t			b_mac[ZIO_DATA_MAC_LEN];
} arc_buf_hdr_crypt_t;

typedef struct l2arc_buf_hdr {
	
	l2arc_dev_t		*b_dev;		
	uint64_t		b_daddr;	
	uint32_t		b_hits;
	arc_state_type_t	b_arcs_state;
	list_node_t		b_l2node;
} l2arc_buf_hdr_t;

typedef struct l2arc_write_callback {
	l2arc_dev_t	*l2wcb_dev;		
	arc_buf_hdr_t	*l2wcb_head;		
	
	list_t		l2wcb_abd_list;
} l2arc_write_callback_t;

struct arc_buf_hdr {
	
	dva_t			b_dva;
	uint64_t		b_birth;

	arc_buf_contents_t	b_type;
	uint8_t			b_complevel;
	uint8_t			b_reserved1; 
	uint16_t		b_reserved2; 
	arc_buf_hdr_t		*b_hash_next;
	arc_flags_t		b_flags;

	
	uint16_t		b_psize;

	
	uint16_t		b_lsize;	
	uint64_t		b_spa;		

	
	l2arc_buf_hdr_t		b_l2hdr;
	
	l1arc_buf_hdr_t		b_l1hdr;
	
	arc_buf_hdr_crypt_t b_crypt_hdr;
};

typedef struct arc_stats {
	
	kstat_named_t arcstat_hits;
	
	kstat_named_t arcstat_iohits;
	
	kstat_named_t arcstat_misses;
	
	kstat_named_t arcstat_demand_data_hits;
	kstat_named_t arcstat_demand_data_iohits;
	kstat_named_t arcstat_demand_data_misses;
	
	kstat_named_t arcstat_demand_metadata_hits;
	kstat_named_t arcstat_demand_metadata_iohits;
	kstat_named_t arcstat_demand_metadata_misses;
	
	kstat_named_t arcstat_prefetch_data_hits;
	kstat_named_t arcstat_prefetch_data_iohits;
	kstat_named_t arcstat_prefetch_data_misses;
	
	kstat_named_t arcstat_prefetch_metadata_hits;
	kstat_named_t arcstat_prefetch_metadata_iohits;
	kstat_named_t arcstat_prefetch_metadata_misses;
	kstat_named_t arcstat_mru_hits;
	kstat_named_t arcstat_mru_ghost_hits;
	kstat_named_t arcstat_mfu_hits;
	kstat_named_t arcstat_mfu_ghost_hits;
	kstat_named_t arcstat_uncached_hits;
	kstat_named_t arcstat_deleted;
	
	kstat_named_t arcstat_mutex_miss;
	
	kstat_named_t arcstat_access_skip;
	
	kstat_named_t arcstat_evict_skip;
	
	kstat_named_t arcstat_evict_not_enough;
	kstat_named_t arcstat_evict_l2_cached;
	kstat_named_t arcstat_evict_l2_eligible;
	kstat_named_t arcstat_evict_l2_eligible_mfu;
	kstat_named_t arcstat_evict_l2_eligible_mru;
	kstat_named_t arcstat_evict_l2_ineligible;
	kstat_named_t arcstat_evict_l2_skip;
	kstat_named_t arcstat_hash_elements;
	kstat_named_t arcstat_hash_elements_max;
	kstat_named_t arcstat_hash_collisions;
	kstat_named_t arcstat_hash_chains;
	kstat_named_t arcstat_hash_chain_max;
	kstat_named_t arcstat_meta;
	kstat_named_t arcstat_pd;
	kstat_named_t arcstat_pm;
	kstat_named_t arcstat_c;
	kstat_named_t arcstat_c_min;
	kstat_named_t arcstat_c_max;
	kstat_named_t arcstat_size;
	
	kstat_named_t arcstat_compressed_size;
	
	kstat_named_t arcstat_uncompressed_size;
	
	kstat_named_t arcstat_overhead_size;
	
	kstat_named_t arcstat_hdr_size;
	
	kstat_named_t arcstat_data_size;
	
	kstat_named_t arcstat_metadata_size;
	
	kstat_named_t arcstat_dbuf_size;
	
	kstat_named_t arcstat_dnode_size;
	
	kstat_named_t arcstat_bonus_size;
#if defined(COMPAT_FREEBSD11)
	
	kstat_named_t arcstat_other_size;
#endif

	
	kstat_named_t arcstat_anon_size;
	kstat_named_t arcstat_anon_data;
	kstat_named_t arcstat_anon_metadata;
	
	kstat_named_t arcstat_anon_evictable_data;
	
	kstat_named_t arcstat_anon_evictable_metadata;
	
	kstat_named_t arcstat_mru_size;
	kstat_named_t arcstat_mru_data;
	kstat_named_t arcstat_mru_metadata;
	
	kstat_named_t arcstat_mru_evictable_data;
	
	kstat_named_t arcstat_mru_evictable_metadata;
	
	kstat_named_t arcstat_mru_ghost_size;
	kstat_named_t arcstat_mru_ghost_data;
	kstat_named_t arcstat_mru_ghost_metadata;
	
	kstat_named_t arcstat_mru_ghost_evictable_data;
	
	kstat_named_t arcstat_mru_ghost_evictable_metadata;
	
	kstat_named_t arcstat_mfu_size;
	kstat_named_t arcstat_mfu_data;
	kstat_named_t arcstat_mfu_metadata;
	
	kstat_named_t arcstat_mfu_evictable_data;
	
	kstat_named_t arcstat_mfu_evictable_metadata;
	
	kstat_named_t arcstat_mfu_ghost_size;
	kstat_named_t arcstat_mfu_ghost_data;
	kstat_named_t arcstat_mfu_ghost_metadata;
	
	kstat_named_t arcstat_mfu_ghost_evictable_data;
	
	kstat_named_t arcstat_mfu_ghost_evictable_metadata;
	
	kstat_named_t arcstat_uncached_size;
	kstat_named_t arcstat_uncached_data;
	kstat_named_t arcstat_uncached_metadata;
	
	kstat_named_t arcstat_uncached_evictable_data;
	
	kstat_named_t arcstat_uncached_evictable_metadata;
	kstat_named_t arcstat_l2_hits;
	kstat_named_t arcstat_l2_misses;
	
	kstat_named_t arcstat_l2_prefetch_asize;
	kstat_named_t arcstat_l2_mru_asize;
	kstat_named_t arcstat_l2_mfu_asize;
	
	kstat_named_t arcstat_l2_bufc_data_asize;
	kstat_named_t arcstat_l2_bufc_metadata_asize;
	kstat_named_t arcstat_l2_feeds;
	kstat_named_t arcstat_l2_rw_clash;
	kstat_named_t arcstat_l2_read_bytes;
	kstat_named_t arcstat_l2_write_bytes;
	kstat_named_t arcstat_l2_writes_sent;
	kstat_named_t arcstat_l2_writes_done;
	kstat_named_t arcstat_l2_writes_error;
	kstat_named_t arcstat_l2_writes_lock_retry;
	kstat_named_t arcstat_l2_evict_lock_retry;
	kstat_named_t arcstat_l2_evict_reading;
	kstat_named_t arcstat_l2_evict_l1cached;
	kstat_named_t arcstat_l2_free_on_write;
	kstat_named_t arcstat_l2_abort_lowmem;
	kstat_named_t arcstat_l2_cksum_bad;
	kstat_named_t arcstat_l2_io_error;
	kstat_named_t arcstat_l2_lsize;
	kstat_named_t arcstat_l2_psize;
	kstat_named_t arcstat_l2_hdr_size;
	
	kstat_named_t arcstat_l2_log_blk_writes;
	
	kstat_named_t arcstat_l2_log_blk_avg_asize;
	
	kstat_named_t arcstat_l2_log_blk_asize;
	
	kstat_named_t arcstat_l2_log_blk_count;
	
	kstat_named_t arcstat_l2_data_to_meta_ratio;
	
	kstat_named_t arcstat_l2_rebuild_success;
	
	kstat_named_t arcstat_l2_rebuild_abort_unsupported;
	
	kstat_named_t arcstat_l2_rebuild_abort_io_errors;
	
	kstat_named_t arcstat_l2_rebuild_abort_dh_errors;
	
	kstat_named_t arcstat_l2_rebuild_abort_cksum_lb_errors;
	
	kstat_named_t arcstat_l2_rebuild_abort_lowmem;
	
	kstat_named_t arcstat_l2_rebuild_size;
	
	kstat_named_t arcstat_l2_rebuild_asize;
	
	kstat_named_t arcstat_l2_rebuild_bufs;
	
	kstat_named_t arcstat_l2_rebuild_bufs_precached;
	
	kstat_named_t arcstat_l2_rebuild_log_blks;
	kstat_named_t arcstat_memory_throttle_count;
	kstat_named_t arcstat_memory_direct_count;
	kstat_named_t arcstat_memory_indirect_count;
	kstat_named_t arcstat_memory_all_bytes;
	kstat_named_t arcstat_memory_free_bytes;
	kstat_named_t arcstat_memory_available_bytes;
	kstat_named_t arcstat_no_grow;
	kstat_named_t arcstat_tempreserve;
	kstat_named_t arcstat_loaned_bytes;
	kstat_named_t arcstat_prune;
	kstat_named_t arcstat_meta_used;
	kstat_named_t arcstat_dnode_limit;
	kstat_named_t arcstat_async_upgrade_sync;
	
	kstat_named_t arcstat_predictive_prefetch;
	
	kstat_named_t arcstat_demand_hit_predictive_prefetch;
	
	kstat_named_t arcstat_demand_iohit_predictive_prefetch;
	
	kstat_named_t arcstat_prescient_prefetch;
	
	kstat_named_t arcstat_demand_hit_prescient_prefetch;
	
	kstat_named_t arcstat_demand_iohit_prescient_prefetch;
	kstat_named_t arcstat_need_free;
	kstat_named_t arcstat_sys_free;
	kstat_named_t arcstat_raw_size;
	kstat_named_t arcstat_cached_only_in_progress;
	kstat_named_t arcstat_abd_chunk_waste_size;
} arc_stats_t;

typedef struct arc_sums {
	wmsum_t arcstat_hits;
	wmsum_t arcstat_iohits;
	wmsum_t arcstat_misses;
	wmsum_t arcstat_demand_data_hits;
	wmsum_t arcstat_demand_data_iohits;
	wmsum_t arcstat_demand_data_misses;
	wmsum_t arcstat_demand_metadata_hits;
	wmsum_t arcstat_demand_metadata_iohits;
	wmsum_t arcstat_demand_metadata_misses;
	wmsum_t arcstat_prefetch_data_hits;
	wmsum_t arcstat_prefetch_data_iohits;
	wmsum_t arcstat_prefetch_data_misses;
	wmsum_t arcstat_prefetch_metadata_hits;
	wmsum_t arcstat_prefetch_metadata_iohits;
	wmsum_t arcstat_prefetch_metadata_misses;
	wmsum_t arcstat_mru_hits;
	wmsum_t arcstat_mru_ghost_hits;
	wmsum_t arcstat_mfu_hits;
	wmsum_t arcstat_mfu_ghost_hits;
	wmsum_t arcstat_uncached_hits;
	wmsum_t arcstat_deleted;
	wmsum_t arcstat_mutex_miss;
	wmsum_t arcstat_access_skip;
	wmsum_t arcstat_evict_skip;
	wmsum_t arcstat_evict_not_enough;
	wmsum_t arcstat_evict_l2_cached;
	wmsum_t arcstat_evict_l2_eligible;
	wmsum_t arcstat_evict_l2_eligible_mfu;
	wmsum_t arcstat_evict_l2_eligible_mru;
	wmsum_t arcstat_evict_l2_ineligible;
	wmsum_t arcstat_evict_l2_skip;
	wmsum_t arcstat_hash_collisions;
	wmsum_t arcstat_hash_chains;
	aggsum_t arcstat_size;
	wmsum_t arcstat_compressed_size;
	wmsum_t arcstat_uncompressed_size;
	wmsum_t arcstat_overhead_size;
	wmsum_t arcstat_hdr_size;
	wmsum_t arcstat_data_size;
	wmsum_t arcstat_metadata_size;
	wmsum_t arcstat_dbuf_size;
	wmsum_t arcstat_dnode_size;
	wmsum_t arcstat_bonus_size;
	wmsum_t arcstat_l2_hits;
	wmsum_t arcstat_l2_misses;
	wmsum_t arcstat_l2_prefetch_asize;
	wmsum_t arcstat_l2_mru_asize;
	wmsum_t arcstat_l2_mfu_asize;
	wmsum_t arcstat_l2_bufc_data_asize;
	wmsum_t arcstat_l2_bufc_metadata_asize;
	wmsum_t arcstat_l2_feeds;
	wmsum_t arcstat_l2_rw_clash;
	wmsum_t arcstat_l2_read_bytes;
	wmsum_t arcstat_l2_write_bytes;
	wmsum_t arcstat_l2_writes_sent;
	wmsum_t arcstat_l2_writes_done;
	wmsum_t arcstat_l2_writes_error;
	wmsum_t arcstat_l2_writes_lock_retry;
	wmsum_t arcstat_l2_evict_lock_retry;
	wmsum_t arcstat_l2_evict_reading;
	wmsum_t arcstat_l2_evict_l1cached;
	wmsum_t arcstat_l2_free_on_write;
	wmsum_t arcstat_l2_abort_lowmem;
	wmsum_t arcstat_l2_cksum_bad;
	wmsum_t arcstat_l2_io_error;
	wmsum_t arcstat_l2_lsize;
	wmsum_t arcstat_l2_psize;
	aggsum_t arcstat_l2_hdr_size;
	wmsum_t arcstat_l2_log_blk_writes;
	wmsum_t arcstat_l2_log_blk_asize;
	wmsum_t arcstat_l2_log_blk_count;
	wmsum_t arcstat_l2_rebuild_success;
	wmsum_t arcstat_l2_rebuild_abort_unsupported;
	wmsum_t arcstat_l2_rebuild_abort_io_errors;
	wmsum_t arcstat_l2_rebuild_abort_dh_errors;
	wmsum_t arcstat_l2_rebuild_abort_cksum_lb_errors;
	wmsum_t arcstat_l2_rebuild_abort_lowmem;
	wmsum_t arcstat_l2_rebuild_size;
	wmsum_t arcstat_l2_rebuild_asize;
	wmsum_t arcstat_l2_rebuild_bufs;
	wmsum_t arcstat_l2_rebuild_bufs_precached;
	wmsum_t arcstat_l2_rebuild_log_blks;
	wmsum_t arcstat_memory_throttle_count;
	wmsum_t arcstat_memory_direct_count;
	wmsum_t arcstat_memory_indirect_count;
	wmsum_t arcstat_prune;
	wmsum_t arcstat_meta_used;
	wmsum_t arcstat_async_upgrade_sync;
	wmsum_t arcstat_predictive_prefetch;
	wmsum_t arcstat_demand_hit_predictive_prefetch;
	wmsum_t arcstat_demand_iohit_predictive_prefetch;
	wmsum_t arcstat_prescient_prefetch;
	wmsum_t arcstat_demand_hit_prescient_prefetch;
	wmsum_t arcstat_demand_iohit_prescient_prefetch;
	wmsum_t arcstat_raw_size;
	wmsum_t arcstat_cached_only_in_progress;
	wmsum_t arcstat_abd_chunk_waste_size;
} arc_sums_t;

typedef struct arc_evict_waiter {
	list_node_t aew_node;
	kcondvar_t aew_cv;
	uint64_t aew_count;
} arc_evict_waiter_t;

#define	ARCSTAT(stat)	(arc_stats.stat.value.ui64)

#define	ARCSTAT_INCR(stat, val) \
	wmsum_add(&arc_sums.stat, (val))

#define	ARCSTAT_BUMP(stat)	ARCSTAT_INCR(stat, 1)
#define	ARCSTAT_BUMPDOWN(stat)	ARCSTAT_INCR(stat, -1)

#define	arc_no_grow	ARCSTAT(arcstat_no_grow) 
#define	arc_meta	ARCSTAT(arcstat_meta)	
#define	arc_pd		ARCSTAT(arcstat_pd)	
#define	arc_pm		ARCSTAT(arcstat_pm)	
#define	arc_c		ARCSTAT(arcstat_c)	
#define	arc_c_min	ARCSTAT(arcstat_c_min)	
#define	arc_c_max	ARCSTAT(arcstat_c_max)	
#define	arc_sys_free	ARCSTAT(arcstat_sys_free) 

#define	arc_anon	(&ARC_anon)
#define	arc_mru		(&ARC_mru)
#define	arc_mru_ghost	(&ARC_mru_ghost)
#define	arc_mfu		(&ARC_mfu)
#define	arc_mfu_ghost	(&ARC_mfu_ghost)
#define	arc_l2c_only	(&ARC_l2c_only)
#define	arc_uncached	(&ARC_uncached)

extern taskq_t *arc_prune_taskq;
extern arc_stats_t arc_stats;
extern arc_sums_t arc_sums;
extern hrtime_t arc_growtime;
extern boolean_t arc_warm;
extern uint_t arc_grow_retry;
extern uint_t arc_no_grow_shift;
extern uint_t arc_shrink_shift;
extern kmutex_t arc_prune_mtx;
extern list_t arc_prune_list;
extern arc_state_t	ARC_mfu;
extern arc_state_t	ARC_mru;
extern uint_t zfs_arc_pc_percent;
extern uint_t arc_lotsfree_percent;
extern uint64_t zfs_arc_min;
extern uint64_t zfs_arc_max;

extern void arc_reduce_target_size(int64_t to_free);
extern boolean_t arc_reclaim_needed(void);
extern void arc_kmem_reap_soon(void);
extern void arc_wait_for_eviction(uint64_t, boolean_t);

extern void arc_lowmem_init(void);
extern void arc_lowmem_fini(void);
extern int arc_memory_throttle(spa_t *spa, uint64_t reserve, uint64_t txg);
extern uint64_t arc_free_memory(void);
extern int64_t arc_available_memory(void);
extern void arc_tuning_update(boolean_t);
extern void arc_register_hotplug(void);
extern void arc_unregister_hotplug(void);

extern int param_set_arc_u64(ZFS_MODULE_PARAM_ARGS);
extern int param_set_arc_int(ZFS_MODULE_PARAM_ARGS);
extern int param_set_arc_min(ZFS_MODULE_PARAM_ARGS);
extern int param_set_arc_max(ZFS_MODULE_PARAM_ARGS);


boolean_t l2arc_log_blkptr_valid(l2arc_dev_t *dev,
    const l2arc_log_blkptr_t *lbp);


void l2arc_dev_hdr_update(l2arc_dev_t *dev);
l2arc_dev_t *l2arc_vdev_get(vdev_t *vd);

#ifdef __cplusplus
}
#endif

#endif 

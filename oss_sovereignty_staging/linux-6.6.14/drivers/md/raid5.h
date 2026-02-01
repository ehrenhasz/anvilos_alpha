 
#ifndef _RAID5_H
#define _RAID5_H

#include <linux/raid/xor.h>
#include <linux/dmaengine.h>
#include <linux/local_lock.h>

 

 
 
enum check_states {
	check_state_idle = 0,
	check_state_run,  
	check_state_run_q,  
	check_state_run_pq,  
	check_state_check_result,
	check_state_compute_run,  
	check_state_compute_result,
};

 
enum reconstruct_states {
	reconstruct_state_idle = 0,
	reconstruct_state_prexor_drain_run,	 
	reconstruct_state_drain_run,		 
	reconstruct_state_run,			 
	reconstruct_state_prexor_drain_result,
	reconstruct_state_drain_result,
	reconstruct_state_result,
};

#define DEFAULT_STRIPE_SIZE	4096
struct stripe_head {
	struct hlist_node	hash;
	struct list_head	lru;	       
	struct llist_node	release_list;
	struct r5conf		*raid_conf;
	short			generation;	 
	sector_t		sector;		 
	short			pd_idx;		 
	short			qd_idx;		 
	short			ddf_layout; 
	short			hash_lock_index;
	unsigned long		state;		 
	atomic_t		count;	       
	int			bm_seq;	 
	int			disks;		 
	int			overwrite_disks;  
	enum check_states	check_state;
	enum reconstruct_states reconstruct_state;
	spinlock_t		stripe_lock;
	int			cpu;
	struct r5worker_group	*group;

	struct stripe_head	*batch_head;  
	spinlock_t		batch_lock;  
	struct list_head	batch_list;  

	union {
		struct r5l_io_unit	*log_io;
		struct ppl_io_unit	*ppl_io;
	};

	struct list_head	log_list;
	sector_t		log_start;  
	struct list_head	r5c;  

	struct page		*ppl_page;  
	 
	struct stripe_operations {
		int 		     target, target2;
		enum sum_check_flags zero_sum_result;
	} ops;

#if PAGE_SIZE != DEFAULT_STRIPE_SIZE
	 
	struct page	**pages;
	int	nr_pages;	 
	int	stripes_per_page;
#endif
	struct r5dev {
		 
		struct bio	req, rreq;
		struct bio_vec	vec, rvec;
		struct page	*page, *orig_page;
		unsigned int    offset;      
		struct bio	*toread, *read, *towrite, *written;
		sector_t	sector;			 
		unsigned long	flags;
		u32		log_checksum;
		unsigned short	write_hint;
	} dev[];  
};

 
struct stripe_head_state {
	 
	int syncing, expanding, expanded, replacing;
	int locked, uptodate, to_read, to_write, failed, written;
	int to_fill, compute, req_compute, non_overwrite;
	int injournal, just_cached;
	int failed_num[2];
	int p_failed, q_failed;
	int dec_preread_active;
	unsigned long ops_request;

	struct md_rdev *blocked_rdev;
	int handle_bad_blocks;
	int log_failed;
	int waiting_extra_page;
};

 
enum r5dev_flags {
	R5_UPTODATE,	 
	R5_LOCKED,	 
	R5_DOUBLE_LOCKED, 
	R5_OVERWRITE,	 
 
	R5_Insync,	 
	R5_Wantread,	 
	R5_Wantwrite,
	R5_Overlap,	 
	R5_ReadNoMerge,  
	R5_ReadError,	 
	R5_ReWrite,	 

	R5_Expanded,	 
	R5_Wantcompute,	 
	R5_Wantfill,	 
	R5_Wantdrain,	 
	R5_WantFUA,	 
	R5_SyncIO,	 
	R5_WriteError,	 
	R5_MadeGood,	 
	R5_ReadRepl,	 
	R5_MadeGoodRepl, 
	R5_NeedReplace,	 
	R5_WantReplace,  
	R5_Discard,	 
	R5_SkipCopy,	 
	R5_InJournal,	 
	R5_OrigPageUPTDODATE,	 
};

 
enum {
	STRIPE_ACTIVE,
	STRIPE_HANDLE,
	STRIPE_SYNC_REQUESTED,
	STRIPE_SYNCING,
	STRIPE_INSYNC,
	STRIPE_REPLACED,
	STRIPE_PREREAD_ACTIVE,
	STRIPE_DELAYED,
	STRIPE_DEGRADED,
	STRIPE_BIT_DELAY,
	STRIPE_EXPANDING,
	STRIPE_EXPAND_SOURCE,
	STRIPE_EXPAND_READY,
	STRIPE_IO_STARTED,	 
	STRIPE_FULL_WRITE,	 
	STRIPE_BIOFILL_RUN,
	STRIPE_COMPUTE_RUN,
	STRIPE_ON_UNPLUG_LIST,
	STRIPE_DISCARD,
	STRIPE_ON_RELEASE_LIST,
	STRIPE_BATCH_READY,
	STRIPE_BATCH_ERR,
	STRIPE_BITMAP_PENDING,	 
	STRIPE_LOG_TRAPPED,	 
	STRIPE_R5C_CACHING,	 
	STRIPE_R5C_PARTIAL_STRIPE,	 
	STRIPE_R5C_FULL_STRIPE,	 
	STRIPE_R5C_PREFLUSH,	 
};

#define STRIPE_EXPAND_SYNC_FLAGS \
	((1 << STRIPE_EXPAND_SOURCE) |\
	(1 << STRIPE_EXPAND_READY) |\
	(1 << STRIPE_EXPANDING) |\
	(1 << STRIPE_SYNC_REQUESTED))
 
enum {
	STRIPE_OP_BIOFILL,
	STRIPE_OP_COMPUTE_BLK,
	STRIPE_OP_PREXOR,
	STRIPE_OP_BIODRAIN,
	STRIPE_OP_RECONSTRUCT,
	STRIPE_OP_CHECK,
	STRIPE_OP_PARTIAL_PARITY,
};

 
enum {
	PARITY_DISABLE_RMW = 0,
	PARITY_ENABLE_RMW,
	PARITY_PREFER_RMW,
};

 
enum {
	SYNDROME_SRC_ALL,
	SYNDROME_SRC_WANT_DRAIN,
	SYNDROME_SRC_WRITTEN,
};
 

 

struct disk_info {
	struct md_rdev	__rcu *rdev;
	struct md_rdev  __rcu *replacement;
	struct page	*extra_page;  
};

 

#define NR_STRIPES		256

#if PAGE_SIZE == DEFAULT_STRIPE_SIZE
#define STRIPE_SIZE		PAGE_SIZE
#define STRIPE_SHIFT		(PAGE_SHIFT - 9)
#define STRIPE_SECTORS		(STRIPE_SIZE>>9)
#endif

#define	IO_THRESHOLD		1
#define BYPASS_THRESHOLD	1
#define NR_HASH			(PAGE_SIZE / sizeof(struct hlist_head))
#define HASH_MASK		(NR_HASH - 1)
#define MAX_STRIPE_BATCH	8

 
#define NR_STRIPE_HASH_LOCKS 8
#define STRIPE_HASH_LOCKS_MASK (NR_STRIPE_HASH_LOCKS - 1)

struct r5worker {
	struct work_struct work;
	struct r5worker_group *group;
	struct list_head temp_inactive_list[NR_STRIPE_HASH_LOCKS];
	bool working;
};

struct r5worker_group {
	struct list_head handle_list;
	struct list_head loprio_list;
	struct r5conf *conf;
	struct r5worker *workers;
	int stripes_cnt;
};

 
enum r5c_journal_mode {
	R5C_JOURNAL_MODE_WRITE_THROUGH = 0,
	R5C_JOURNAL_MODE_WRITE_BACK = 1,
};

enum r5_cache_state {
	R5_INACTIVE_BLOCKED,	 
	R5_ALLOC_MORE,		 
	R5_DID_ALLOC,		 
	R5C_LOG_TIGHT,		 
	R5C_LOG_CRITICAL,	 
	R5C_EXTRA_PAGE_IN_USE,	 
};

#define PENDING_IO_MAX 512
#define PENDING_IO_ONE_FLUSH 128
struct r5pending_data {
	struct list_head sibling;
	sector_t sector;  
	struct bio_list bios;
};

struct raid5_percpu {
	struct page	*spare_page;  
	void		*scribble;   
	int             scribble_obj_size;
	local_lock_t    lock;
};

struct r5conf {
	struct hlist_head	*stripe_hashtbl;
	 
	spinlock_t		hash_locks[NR_STRIPE_HASH_LOCKS];
	struct mddev		*mddev;
	int			chunk_sectors;
	int			level, algorithm, rmw_level;
	int			max_degraded;
	int			raid_disks;
	int			max_nr_stripes;
	int			min_nr_stripes;
#if PAGE_SIZE != DEFAULT_STRIPE_SIZE
	unsigned long	stripe_size;
	unsigned int	stripe_shift;
	unsigned long	stripe_sectors;
#endif

	 
	sector_t		reshape_progress;
	 
	sector_t		reshape_safe;
	int			previous_raid_disks;
	int			prev_chunk_sectors;
	int			prev_algo;
	short			generation;  
	seqcount_spinlock_t	gen_lock;	 
	unsigned long		reshape_checkpoint;  
	long long		min_offset_diff;  

	struct list_head	handle_list;  
	struct list_head	loprio_list;  
	struct list_head	hold_list;  
	struct list_head	delayed_list;  
	struct list_head	bitmap_list;  
	struct bio		*retry_read_aligned;  
	unsigned int		retry_read_offset;  
	struct bio		*retry_read_aligned_list;  
	atomic_t		preread_active_stripes;  
	atomic_t		active_aligned_reads;
	atomic_t		pending_full_writes;  
	int			bypass_count;  
	int			bypass_threshold;  
	int			skip_copy;  
	struct list_head	*last_hold;  

	atomic_t		reshape_stripes;  
	 
	int			active_name;
	char			cache_name[2][32];
	struct kmem_cache	*slab_cache;  
	struct mutex		cache_size_mutex;  

	int			seq_flush, seq_write;
	int			quiesce;

	int			fullsync;   
	int			recovery_disabled;
	 
	struct raid5_percpu __percpu *percpu;
	int scribble_disks;
	int scribble_sectors;
	struct hlist_node node;

	 
	atomic_t		active_stripes;
	struct list_head	inactive_list[NR_STRIPE_HASH_LOCKS];

	atomic_t		r5c_cached_full_stripes;
	struct list_head	r5c_full_stripe_list;
	atomic_t		r5c_cached_partial_stripes;
	struct list_head	r5c_partial_stripe_list;
	atomic_t		r5c_flushing_full_stripes;
	atomic_t		r5c_flushing_partial_stripes;

	atomic_t		empty_inactive_list_nr;
	struct llist_head	released_stripes;
	wait_queue_head_t	wait_for_quiescent;
	wait_queue_head_t	wait_for_stripe;
	wait_queue_head_t	wait_for_overlap;
	unsigned long		cache_state;
	struct shrinker		shrinker;
	int			pool_size;  
	spinlock_t		device_lock;
	struct disk_info	*disks;
	struct bio_set		bio_split;

	 
	struct md_thread __rcu	*thread;
	struct list_head	temp_inactive_list[NR_STRIPE_HASH_LOCKS];
	struct r5worker_group	*worker_groups;
	int			group_cnt;
	int			worker_cnt_per_group;
	struct r5l_log		*log;
	void			*log_private;

	spinlock_t		pending_bios_lock;
	bool			batch_bio_dispatch;
	struct r5pending_data	*pending_data;
	struct list_head	free_list;
	struct list_head	pending_list;
	int			pending_data_cnt;
	struct r5pending_data	*next_pending_data;
};

#if PAGE_SIZE == DEFAULT_STRIPE_SIZE
#define RAID5_STRIPE_SIZE(conf)	STRIPE_SIZE
#define RAID5_STRIPE_SHIFT(conf)	STRIPE_SHIFT
#define RAID5_STRIPE_SECTORS(conf)	STRIPE_SECTORS
#else
#define RAID5_STRIPE_SIZE(conf)	((conf)->stripe_size)
#define RAID5_STRIPE_SHIFT(conf)	((conf)->stripe_shift)
#define RAID5_STRIPE_SECTORS(conf)	((conf)->stripe_sectors)
#endif

 
static inline struct bio *r5_next_bio(struct r5conf *conf, struct bio *bio, sector_t sector)
{
	if (bio_end_sector(bio) < sector + RAID5_STRIPE_SECTORS(conf))
		return bio->bi_next;
	else
		return NULL;
}

 
#define ALGORITHM_LEFT_ASYMMETRIC	0  
#define ALGORITHM_RIGHT_ASYMMETRIC	1  
#define ALGORITHM_LEFT_SYMMETRIC	2  
#define ALGORITHM_RIGHT_SYMMETRIC	3  

 
#define ALGORITHM_PARITY_0		4  
#define ALGORITHM_PARITY_N		5  

 

#define ALGORITHM_ROTATING_ZERO_RESTART	8  
#define ALGORITHM_ROTATING_N_RESTART	9  
#define ALGORITHM_ROTATING_N_CONTINUE	10  

 
#define ALGORITHM_LEFT_ASYMMETRIC_6	16
#define ALGORITHM_RIGHT_ASYMMETRIC_6	17
#define ALGORITHM_LEFT_SYMMETRIC_6	18
#define ALGORITHM_RIGHT_SYMMETRIC_6	19
#define ALGORITHM_PARITY_0_6		20
#define ALGORITHM_PARITY_N_6		ALGORITHM_PARITY_N

static inline int algorithm_valid_raid5(int layout)
{
	return (layout >= 0) &&
		(layout <= 5);
}
static inline int algorithm_valid_raid6(int layout)
{
	return (layout >= 0 && layout <= 5)
		||
		(layout >= 8 && layout <= 10)
		||
		(layout >= 16 && layout <= 20);
}

static inline int algorithm_is_DDF(int layout)
{
	return layout >= 8 && layout <= 10;
}

#if PAGE_SIZE != DEFAULT_STRIPE_SIZE
 
static inline int raid5_get_page_offset(struct stripe_head *sh, int disk_idx)
{
	return (disk_idx % sh->stripes_per_page) * RAID5_STRIPE_SIZE(sh->raid_conf);
}

 
static inline struct page *
raid5_get_dev_page(struct stripe_head *sh, int disk_idx)
{
	return sh->pages[disk_idx / sh->stripes_per_page];
}
#endif

void md_raid5_kick_device(struct r5conf *conf);
int raid5_set_cache_size(struct mddev *mddev, int size);
sector_t raid5_compute_blocknr(struct stripe_head *sh, int i, int previous);
void raid5_release_stripe(struct stripe_head *sh);
sector_t raid5_compute_sector(struct r5conf *conf, sector_t r_sector,
		int previous, int *dd_idx, struct stripe_head *sh);

struct stripe_request_ctx;
 
#define R5_GAS_PREVIOUS		(1 << 0)
 
#define R5_GAS_NOBLOCK		(1 << 1)
 
#define R5_GAS_NOQUIESCE	(1 << 2)
struct stripe_head *raid5_get_active_stripe(struct r5conf *conf,
		struct stripe_request_ctx *ctx, sector_t sector,
		unsigned int flags);

int raid5_calc_degraded(struct r5conf *conf);
int r5c_journal_mode_set(struct mddev *mddev, int journal_mode);
#endif

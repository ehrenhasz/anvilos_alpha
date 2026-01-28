#ifndef BTRFS_FS_H
#define BTRFS_FS_H
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/btrfs_tree.h>
#include <linux/sizes.h>
#include "extent-io-tree.h"
#include "extent_map.h"
#include "async-thread.h"
#include "block-rsv.h"
#define BTRFS_MAX_EXTENT_SIZE SZ_128M
#define BTRFS_OLDEST_GENERATION	0ULL
#define BTRFS_EMPTY_DIR_SIZE 0
#define BTRFS_DIRTY_METADATA_THRESH		SZ_32M
#define BTRFS_SUPER_INFO_OFFSET			SZ_64K
#define BTRFS_SUPER_INFO_SIZE			4096
static_assert(sizeof(struct btrfs_super_block) == BTRFS_SUPER_INFO_SIZE);
#define BTRFS_UNLINK_METADATA_UNITS		6
#define BTRFS_DEVICE_RANGE_RESERVED			(SZ_1M)
enum {
	BTRFS_FS_STATE_REMOUNTING,
	BTRFS_FS_STATE_RO,
	BTRFS_FS_STATE_TRANS_ABORTED,
	BTRFS_FS_STATE_DEV_REPLACING,
	BTRFS_FS_STATE_DUMMY_FS_INFO,
	BTRFS_FS_STATE_NO_CSUMS,
	BTRFS_FS_STATE_LOG_CLEANUP_ERROR,
	BTRFS_FS_STATE_COUNT
};
enum {
	BTRFS_FS_CLOSING_START,
	BTRFS_FS_CLOSING_DONE,
	BTRFS_FS_LOG_RECOVERING,
	BTRFS_FS_OPEN,
	BTRFS_FS_QUOTA_ENABLED,
	BTRFS_FS_UPDATE_UUID_TREE_GEN,
	BTRFS_FS_CREATING_FREE_SPACE_TREE,
	BTRFS_FS_BTREE_ERR,
	BTRFS_FS_LOG1_ERR,
	BTRFS_FS_LOG2_ERR,
	BTRFS_FS_QUOTA_OVERRIDE,
	BTRFS_FS_FROZEN,
	BTRFS_FS_BALANCE_RUNNING,
	BTRFS_FS_RELOC_RUNNING,
	BTRFS_FS_CLEANER_RUNNING,
	BTRFS_FS_CSUM_IMPL_FAST,
	BTRFS_FS_DISCARD_RUNNING,
	BTRFS_FS_CLEANUP_SPACE_CACHE_V1,
	BTRFS_FS_FREE_SPACE_TREE_UNTRUSTED,
	BTRFS_FS_TREE_MOD_LOG_USERS,
	BTRFS_FS_COMMIT_TRANS,
	BTRFS_FS_UNFINISHED_DROPS,
	BTRFS_FS_NEED_ZONE_FINISH,
	BTRFS_FS_NEED_TRANS_COMMIT,
	BTRFS_FS_ACTIVE_ZONE_TRACKING,
	BTRFS_FS_FEATURE_CHANGED,
#if BITS_PER_LONG == 32
	BTRFS_FS_32BIT_ERROR,
	BTRFS_FS_32BIT_WARN,
#endif
};
enum {
	BTRFS_MOUNT_NODATASUM			= (1UL << 0),
	BTRFS_MOUNT_NODATACOW			= (1UL << 1),
	BTRFS_MOUNT_NOBARRIER			= (1UL << 2),
	BTRFS_MOUNT_SSD				= (1UL << 3),
	BTRFS_MOUNT_DEGRADED			= (1UL << 4),
	BTRFS_MOUNT_COMPRESS			= (1UL << 5),
	BTRFS_MOUNT_NOTREELOG   		= (1UL << 6),
	BTRFS_MOUNT_FLUSHONCOMMIT		= (1UL << 7),
	BTRFS_MOUNT_SSD_SPREAD			= (1UL << 8),
	BTRFS_MOUNT_NOSSD			= (1UL << 9),
	BTRFS_MOUNT_DISCARD_SYNC		= (1UL << 10),
	BTRFS_MOUNT_FORCE_COMPRESS      	= (1UL << 11),
	BTRFS_MOUNT_SPACE_CACHE			= (1UL << 12),
	BTRFS_MOUNT_CLEAR_CACHE			= (1UL << 13),
	BTRFS_MOUNT_USER_SUBVOL_RM_ALLOWED	= (1UL << 14),
	BTRFS_MOUNT_ENOSPC_DEBUG		= (1UL << 15),
	BTRFS_MOUNT_AUTO_DEFRAG			= (1UL << 16),
	BTRFS_MOUNT_USEBACKUPROOT		= (1UL << 17),
	BTRFS_MOUNT_SKIP_BALANCE		= (1UL << 18),
	BTRFS_MOUNT_CHECK_INTEGRITY		= (1UL << 19),
	BTRFS_MOUNT_CHECK_INTEGRITY_DATA	= (1UL << 20),
	BTRFS_MOUNT_PANIC_ON_FATAL_ERROR	= (1UL << 21),
	BTRFS_MOUNT_RESCAN_UUID_TREE		= (1UL << 22),
	BTRFS_MOUNT_FRAGMENT_DATA		= (1UL << 23),
	BTRFS_MOUNT_FRAGMENT_METADATA		= (1UL << 24),
	BTRFS_MOUNT_FREE_SPACE_TREE		= (1UL << 25),
	BTRFS_MOUNT_NOLOGREPLAY			= (1UL << 26),
	BTRFS_MOUNT_REF_VERIFY			= (1UL << 27),
	BTRFS_MOUNT_DISCARD_ASYNC		= (1UL << 28),
	BTRFS_MOUNT_IGNOREBADROOTS		= (1UL << 29),
	BTRFS_MOUNT_IGNOREDATACSUMS		= (1UL << 30),
	BTRFS_MOUNT_NODISCARD			= (1UL << 31),
};
#define BTRFS_FEATURE_COMPAT_SUPP		0ULL
#define BTRFS_FEATURE_COMPAT_SAFE_SET		0ULL
#define BTRFS_FEATURE_COMPAT_SAFE_CLEAR		0ULL
#define BTRFS_FEATURE_COMPAT_RO_SUPP			\
	(BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE |	\
	 BTRFS_FEATURE_COMPAT_RO_FREE_SPACE_TREE_VALID | \
	 BTRFS_FEATURE_COMPAT_RO_VERITY |		\
	 BTRFS_FEATURE_COMPAT_RO_BLOCK_GROUP_TREE)
#define BTRFS_FEATURE_COMPAT_RO_SAFE_SET	0ULL
#define BTRFS_FEATURE_COMPAT_RO_SAFE_CLEAR	0ULL
#define BTRFS_FEATURE_INCOMPAT_SUPP_STABLE		\
	(BTRFS_FEATURE_INCOMPAT_MIXED_BACKREF |		\
	 BTRFS_FEATURE_INCOMPAT_DEFAULT_SUBVOL |	\
	 BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS |		\
	 BTRFS_FEATURE_INCOMPAT_BIG_METADATA |		\
	 BTRFS_FEATURE_INCOMPAT_COMPRESS_LZO |		\
	 BTRFS_FEATURE_INCOMPAT_COMPRESS_ZSTD |		\
	 BTRFS_FEATURE_INCOMPAT_RAID56 |		\
	 BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF |		\
	 BTRFS_FEATURE_INCOMPAT_SKINNY_METADATA |	\
	 BTRFS_FEATURE_INCOMPAT_NO_HOLES	|	\
	 BTRFS_FEATURE_INCOMPAT_METADATA_UUID	|	\
	 BTRFS_FEATURE_INCOMPAT_RAID1C34	|	\
	 BTRFS_FEATURE_INCOMPAT_ZONED)
#ifdef CONFIG_BTRFS_DEBUG
#define BTRFS_FEATURE_INCOMPAT_SUPP		\
	(BTRFS_FEATURE_INCOMPAT_SUPP_STABLE |	\
	 BTRFS_FEATURE_INCOMPAT_EXTENT_TREE_V2)
#else
#define BTRFS_FEATURE_INCOMPAT_SUPP		\
	(BTRFS_FEATURE_INCOMPAT_SUPP_STABLE)
#endif
#define BTRFS_FEATURE_INCOMPAT_SAFE_SET			\
	(BTRFS_FEATURE_INCOMPAT_EXTENDED_IREF)
#define BTRFS_FEATURE_INCOMPAT_SAFE_CLEAR		0ULL
#define BTRFS_DEFAULT_COMMIT_INTERVAL	(30)
#define BTRFS_DEFAULT_MAX_INLINE	(2048)
struct btrfs_dev_replace {
	u64 replace_state;
	time64_t time_started;
	time64_t time_stopped;
	atomic64_t num_write_errors;
	atomic64_t num_uncorrectable_read_errors;
	u64 cursor_left;
	u64 committed_cursor_left;
	u64 cursor_left_last_write_of_item;
	u64 cursor_right;
	u64 cont_reading_from_srcdev_mode;
	int is_valid;
	int item_needs_writeback;
	struct btrfs_device *srcdev;
	struct btrfs_device *tgtdev;
	struct mutex lock_finishing_cancel_unmount;
	struct rw_semaphore rwsem;
	struct btrfs_scrub_progress scrub_progress;
	struct percpu_counter bio_counter;
	wait_queue_head_t replace_wait;
};
struct btrfs_free_cluster {
	spinlock_t lock;
	spinlock_t refill_lock;
	struct rb_root root;
	u64 max_size;
	u64 window_start;
	bool fragmented;
	struct btrfs_block_group *block_group;
	struct list_head block_group_list;
};
#define BTRFS_NR_DISCARD_LISTS		3
#define BTRFS_DISCARD_INDEX_UNUSED	0
#define BTRFS_DISCARD_INDEX_START	1
struct btrfs_discard_ctl {
	struct workqueue_struct *discard_workers;
	struct delayed_work work;
	spinlock_t lock;
	struct btrfs_block_group *block_group;
	struct list_head discard_list[BTRFS_NR_DISCARD_LISTS];
	u64 prev_discard;
	u64 prev_discard_time;
	atomic_t discardable_extents;
	atomic64_t discardable_bytes;
	u64 max_discard_size;
	u64 delay_ms;
	u32 iops_limit;
	u32 kbps_limit;
	u64 discard_extent_bytes;
	u64 discard_bitmap_bytes;
	atomic64_t discard_bytes_saved;
};
enum btrfs_exclusive_operation {
	BTRFS_EXCLOP_NONE,
	BTRFS_EXCLOP_BALANCE_PAUSED,
	BTRFS_EXCLOP_BALANCE,
	BTRFS_EXCLOP_DEV_ADD,
	BTRFS_EXCLOP_DEV_REMOVE,
	BTRFS_EXCLOP_DEV_REPLACE,
	BTRFS_EXCLOP_RESIZE,
	BTRFS_EXCLOP_SWAP_ACTIVATE,
};
struct btrfs_commit_stats {
	u64 commit_count;
	u64 max_commit_dur;
	u64 last_commit_dur;
	u64 total_commit_dur;
};
struct btrfs_fs_info {
	u8 chunk_tree_uuid[BTRFS_UUID_SIZE];
	unsigned long flags;
	struct btrfs_root *tree_root;
	struct btrfs_root *chunk_root;
	struct btrfs_root *dev_root;
	struct btrfs_root *fs_root;
	struct btrfs_root *quota_root;
	struct btrfs_root *uuid_root;
	struct btrfs_root *data_reloc_root;
	struct btrfs_root *block_group_root;
	struct btrfs_root *log_root_tree;
	rwlock_t global_root_lock;
	struct rb_root global_root_tree;
	spinlock_t fs_roots_radix_lock;
	struct radix_tree_root fs_roots_radix;
	rwlock_t block_group_cache_lock;
	struct rb_root_cached block_group_cache_tree;
	atomic64_t free_chunk_space;
	struct extent_io_tree excluded_extents;
	struct extent_map_tree mapping_tree;
	struct btrfs_block_rsv global_block_rsv;
	struct btrfs_block_rsv trans_block_rsv;
	struct btrfs_block_rsv chunk_block_rsv;
	struct btrfs_block_rsv delayed_block_rsv;
	struct btrfs_block_rsv delayed_refs_rsv;
	struct btrfs_block_rsv empty_block_rsv;
	u64 generation;
	u64 last_trans_committed;
	u64 last_reloc_trans;
	u64 last_trans_log_full_commit;
	unsigned long mount_opt;
	unsigned long compress_type:4;
	unsigned int compress_level;
	u32 commit_interval;
	u64 max_inline;
	struct btrfs_transaction *running_transaction;
	wait_queue_head_t transaction_throttle;
	wait_queue_head_t transaction_wait;
	wait_queue_head_t transaction_blocked_wait;
	wait_queue_head_t async_submit_wait;
	spinlock_t super_lock;
	struct btrfs_super_block *super_copy;
	struct btrfs_super_block *super_for_commit;
	struct super_block *sb;
	struct inode *btree_inode;
	struct mutex tree_log_mutex;
	struct mutex transaction_kthread_mutex;
	struct mutex cleaner_mutex;
	struct mutex chunk_mutex;
	struct mutex ro_block_group_mutex;
	struct btrfs_stripe_hash_table *stripe_hash_table;
	struct mutex ordered_operations_mutex;
	struct rw_semaphore commit_root_sem;
	struct rw_semaphore cleanup_work_sem;
	struct rw_semaphore subvol_sem;
	spinlock_t trans_lock;
	struct mutex reloc_mutex;
	struct list_head trans_list;
	struct list_head dead_roots;
	struct list_head caching_block_groups;
	spinlock_t delayed_iput_lock;
	struct list_head delayed_iputs;
	atomic_t nr_delayed_iputs;
	wait_queue_head_t delayed_iputs_wait;
	atomic64_t tree_mod_seq;
	rwlock_t tree_mod_log_lock;
	struct rb_root tree_mod_log;
	struct list_head tree_mod_seq_list;
	atomic_t async_delalloc_pages;
	spinlock_t ordered_root_lock;
	struct list_head ordered_roots;
	struct mutex delalloc_root_mutex;
	spinlock_t delalloc_root_lock;
	struct list_head delalloc_roots;
	struct btrfs_workqueue *workers;
	struct btrfs_workqueue *delalloc_workers;
	struct btrfs_workqueue *flush_workers;
	struct workqueue_struct *endio_workers;
	struct workqueue_struct *endio_meta_workers;
	struct workqueue_struct *rmw_workers;
	struct workqueue_struct *compressed_write_workers;
	struct btrfs_workqueue *endio_write_workers;
	struct btrfs_workqueue *endio_freespace_worker;
	struct btrfs_workqueue *caching_workers;
	struct btrfs_workqueue *fixup_workers;
	struct btrfs_workqueue *delayed_workers;
	struct task_struct *transaction_kthread;
	struct task_struct *cleaner_kthread;
	u32 thread_pool_size;
	struct kobject *space_info_kobj;
	struct kobject *qgroups_kobj;
	struct kobject *discard_kobj;
	struct percpu_counter dirty_metadata_bytes;
	struct percpu_counter delalloc_bytes;
	struct percpu_counter ordered_bytes;
	s32 dirty_metadata_batch;
	s32 delalloc_batch;
	struct list_head dirty_cowonly_roots;
	struct btrfs_fs_devices *fs_devices;
	struct list_head space_info;
	struct btrfs_space_info *data_sinfo;
	struct reloc_control *reloc_ctl;
	struct btrfs_free_cluster data_alloc_cluster;
	struct btrfs_free_cluster meta_alloc_cluster;
	spinlock_t defrag_inodes_lock;
	struct rb_root defrag_inodes;
	atomic_t defrag_running;
	seqlock_t profiles_lock;
	u64 avail_data_alloc_bits;
	u64 avail_metadata_alloc_bits;
	u64 avail_system_alloc_bits;
	spinlock_t balance_lock;
	struct mutex balance_mutex;
	atomic_t balance_pause_req;
	atomic_t balance_cancel_req;
	struct btrfs_balance_control *balance_ctl;
	wait_queue_head_t balance_wait_q;
	atomic_t reloc_cancel_req;
	u32 data_chunk_allocations;
	u32 metadata_ratio;
	void *bdev_holder;
	struct mutex scrub_lock;
	atomic_t scrubs_running;
	atomic_t scrub_pause_req;
	atomic_t scrubs_paused;
	atomic_t scrub_cancel_req;
	wait_queue_head_t scrub_pause_wait;
	refcount_t scrub_workers_refcnt;
	struct workqueue_struct *scrub_workers;
	struct btrfs_subpage_info *subpage_info;
	struct btrfs_discard_ctl discard_ctl;
#ifdef CONFIG_BTRFS_FS_CHECK_INTEGRITY
	u32 check_integrity_print_mask;
#endif
	u64 qgroup_flags;
	struct rb_root qgroup_tree;
	spinlock_t qgroup_lock;
	struct ulist *qgroup_ulist;
	struct mutex qgroup_ioctl_lock;
	struct list_head dirty_qgroups;
	u64 qgroup_seq;
	struct mutex qgroup_rescan_lock;
	struct btrfs_key qgroup_rescan_progress;
	struct btrfs_workqueue *qgroup_rescan_workers;
	struct completion qgroup_rescan_completion;
	struct btrfs_work qgroup_rescan_work;
	bool qgroup_rescan_running;
	u8 qgroup_drop_subtree_thres;
	int fs_error;
	unsigned long fs_state;
	struct btrfs_delayed_root *delayed_root;
	spinlock_t buffer_lock;
	struct radix_tree_root buffer_radix;
	int backup_root_index;
	struct btrfs_dev_replace dev_replace;
	struct semaphore uuid_tree_rescan_sem;
	struct work_struct async_reclaim_work;
	struct work_struct async_data_reclaim_work;
	struct work_struct preempt_reclaim_work;
	struct work_struct reclaim_bgs_work;
	struct list_head reclaim_bgs;
	int bg_reclaim_threshold;
	spinlock_t unused_bgs_lock;
	struct list_head unused_bgs;
	struct mutex unused_bg_unpin_mutex;
	struct mutex reclaim_bgs_lock;
	u32 nodesize;
	u32 sectorsize;
	u32 sectorsize_bits;
	u32 csum_size;
	u32 csums_per_leaf;
	u32 stripesize;
	u64 max_extent_size;
	spinlock_t swapfile_pins_lock;
	struct rb_root swapfile_pins;
	struct crypto_shash *csum_shash;
	enum btrfs_exclusive_operation exclusive_operation;
	u64 zone_size;
	struct queue_limits limits;
	u64 max_zone_append_size;
	struct mutex zoned_meta_io_lock;
	spinlock_t treelog_bg_lock;
	u64 treelog_bg;
	spinlock_t relocation_bg_lock;
	u64 data_reloc_bg;
	struct mutex zoned_data_reloc_io_lock;
	struct btrfs_block_group *active_meta_bg;
	struct btrfs_block_group *active_system_bg;
	u64 nr_global_roots;
	spinlock_t zone_active_bgs_lock;
	struct list_head zone_active_bgs;
	struct btrfs_commit_stats commit_stats;
	u64 last_root_drop_gen;
	struct lockdep_map btrfs_trans_num_writers_map;
	struct lockdep_map btrfs_trans_num_extwriters_map;
	struct lockdep_map btrfs_state_change_map[4];
	struct lockdep_map btrfs_trans_pending_ordered_map;
	struct lockdep_map btrfs_ordered_extent_map;
#ifdef CONFIG_BTRFS_FS_REF_VERIFY
	spinlock_t ref_verify_lock;
	struct rb_root block_tree;
#endif
#ifdef CONFIG_BTRFS_DEBUG
	struct kobject *debug_kobj;
	struct list_head allocated_roots;
	spinlock_t eb_leak_lock;
	struct list_head allocated_ebs;
#endif
};
static inline void btrfs_set_last_root_drop_gen(struct btrfs_fs_info *fs_info,
						u64 gen)
{
	WRITE_ONCE(fs_info->last_root_drop_gen, gen);
}
static inline u64 btrfs_get_last_root_drop_gen(const struct btrfs_fs_info *fs_info)
{
	return READ_ONCE(fs_info->last_root_drop_gen);
}
static inline u64 btrfs_csum_bytes_to_leaves(
			const struct btrfs_fs_info *fs_info, u64 csum_bytes)
{
	const u64 num_csums = csum_bytes >> fs_info->sectorsize_bits;
	return DIV_ROUND_UP_ULL(num_csums, fs_info->csums_per_leaf);
}
static inline u64 btrfs_calc_insert_metadata_size(const struct btrfs_fs_info *fs_info,
						  unsigned num_items)
{
	return (u64)fs_info->nodesize * BTRFS_MAX_LEVEL * 2 * num_items;
}
static inline u64 btrfs_calc_metadata_size(const struct btrfs_fs_info *fs_info,
						 unsigned num_items)
{
	return (u64)fs_info->nodesize * BTRFS_MAX_LEVEL * num_items;
}
#define BTRFS_MAX_EXTENT_ITEM_SIZE(r) ((BTRFS_LEAF_DATA_SIZE(r->fs_info) >> 4) - \
					sizeof(struct btrfs_item))
static inline bool btrfs_is_zoned(const struct btrfs_fs_info *fs_info)
{
	return IS_ENABLED(CONFIG_BLK_DEV_ZONED) && fs_info->zone_size > 0;
}
static inline u32 count_max_extents(struct btrfs_fs_info *fs_info, u64 size)
{
#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
	if (!fs_info)
		return div_u64(size + BTRFS_MAX_EXTENT_SIZE - 1, BTRFS_MAX_EXTENT_SIZE);
#endif
	return div_u64(size + fs_info->max_extent_size - 1, fs_info->max_extent_size);
}
bool btrfs_exclop_start(struct btrfs_fs_info *fs_info,
			enum btrfs_exclusive_operation type);
bool btrfs_exclop_start_try_lock(struct btrfs_fs_info *fs_info,
				 enum btrfs_exclusive_operation type);
void btrfs_exclop_start_unlock(struct btrfs_fs_info *fs_info);
void btrfs_exclop_finish(struct btrfs_fs_info *fs_info);
void btrfs_exclop_balance(struct btrfs_fs_info *fs_info,
			  enum btrfs_exclusive_operation op);
void __btrfs_set_fs_incompat(struct btrfs_fs_info *fs_info, u64 flag,
			     const char *name);
void __btrfs_clear_fs_incompat(struct btrfs_fs_info *fs_info, u64 flag,
			       const char *name);
void __btrfs_set_fs_compat_ro(struct btrfs_fs_info *fs_info, u64 flag,
			      const char *name);
void __btrfs_clear_fs_compat_ro(struct btrfs_fs_info *fs_info, u64 flag,
				const char *name);
#define __btrfs_fs_incompat(fs_info, flags)				\
	(!!(btrfs_super_incompat_flags((fs_info)->super_copy) & (flags)))
#define __btrfs_fs_compat_ro(fs_info, flags)				\
	(!!(btrfs_super_compat_ro_flags((fs_info)->super_copy) & (flags)))
#define btrfs_set_fs_incompat(__fs_info, opt)				\
	__btrfs_set_fs_incompat((__fs_info), BTRFS_FEATURE_INCOMPAT_##opt, #opt)
#define btrfs_clear_fs_incompat(__fs_info, opt)				\
	__btrfs_clear_fs_incompat((__fs_info), BTRFS_FEATURE_INCOMPAT_##opt, #opt)
#define btrfs_fs_incompat(fs_info, opt)					\
	__btrfs_fs_incompat((fs_info), BTRFS_FEATURE_INCOMPAT_##opt)
#define btrfs_set_fs_compat_ro(__fs_info, opt)				\
	__btrfs_set_fs_compat_ro((__fs_info), BTRFS_FEATURE_COMPAT_RO_##opt, #opt)
#define btrfs_clear_fs_compat_ro(__fs_info, opt)			\
	__btrfs_clear_fs_compat_ro((__fs_info), BTRFS_FEATURE_COMPAT_RO_##opt, #opt)
#define btrfs_fs_compat_ro(fs_info, opt)				\
	__btrfs_fs_compat_ro((fs_info), BTRFS_FEATURE_COMPAT_RO_##opt)
#define btrfs_clear_opt(o, opt)		((o) &= ~BTRFS_MOUNT_##opt)
#define btrfs_set_opt(o, opt)		((o) |= BTRFS_MOUNT_##opt)
#define btrfs_raw_test_opt(o, opt)	((o) & BTRFS_MOUNT_##opt)
#define btrfs_test_opt(fs_info, opt)	((fs_info)->mount_opt & \
					 BTRFS_MOUNT_##opt)
#define btrfs_set_and_info(fs_info, opt, fmt, args...)			\
do {									\
	if (!btrfs_test_opt(fs_info, opt))				\
		btrfs_info(fs_info, fmt, ##args);			\
	btrfs_set_opt(fs_info->mount_opt, opt);				\
} while (0)
#define btrfs_clear_and_info(fs_info, opt, fmt, args...)		\
do {									\
	if (btrfs_test_opt(fs_info, opt))				\
		btrfs_info(fs_info, fmt, ##args);			\
	btrfs_clear_opt(fs_info->mount_opt, opt);			\
} while (0)
static inline int btrfs_fs_closing(struct btrfs_fs_info *fs_info)
{
	if (test_bit(BTRFS_FS_CLOSING_START, &fs_info->flags)) {
		if (test_bit(BTRFS_FS_CLOSING_DONE, &fs_info->flags))
			return 2;
		return 1;
	}
	return 0;
}
static inline int btrfs_need_cleaner_sleep(struct btrfs_fs_info *fs_info)
{
	return test_bit(BTRFS_FS_STATE_RO, &fs_info->fs_state) ||
		btrfs_fs_closing(fs_info);
}
static inline void btrfs_wake_unfinished_drop(struct btrfs_fs_info *fs_info)
{
	clear_and_wake_up_bit(BTRFS_FS_UNFINISHED_DROPS, &fs_info->flags);
}
#define BTRFS_FS_ERROR(fs_info)	(READ_ONCE((fs_info)->fs_error))
#define BTRFS_FS_LOG_CLEANUP_ERROR(fs_info)				\
	(unlikely(test_bit(BTRFS_FS_STATE_LOG_CLEANUP_ERROR,		\
			   &(fs_info)->fs_state)))
#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
#define EXPORT_FOR_TESTS
static inline int btrfs_is_testing(struct btrfs_fs_info *fs_info)
{
	return test_bit(BTRFS_FS_STATE_DUMMY_FS_INFO, &fs_info->fs_state);
}
void btrfs_test_destroy_inode(struct inode *inode);
#else
#define EXPORT_FOR_TESTS static
static inline int btrfs_is_testing(struct btrfs_fs_info *fs_info)
{
	return 0;
}
#endif
#endif

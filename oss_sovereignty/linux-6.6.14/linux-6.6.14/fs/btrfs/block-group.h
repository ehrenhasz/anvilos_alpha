#ifndef BTRFS_BLOCK_GROUP_H
#define BTRFS_BLOCK_GROUP_H
#include "free-space-cache.h"
enum btrfs_disk_cache_state {
	BTRFS_DC_WRITTEN,
	BTRFS_DC_ERROR,
	BTRFS_DC_CLEAR,
	BTRFS_DC_SETUP,
};
enum btrfs_block_group_size_class {
	BTRFS_BG_SZ_NONE,
	BTRFS_BG_SZ_SMALL,
	BTRFS_BG_SZ_MEDIUM,
	BTRFS_BG_SZ_LARGE,
};
enum btrfs_discard_state {
	BTRFS_DISCARD_EXTENTS,
	BTRFS_DISCARD_BITMAPS,
	BTRFS_DISCARD_RESET_CURSOR,
};
enum btrfs_chunk_alloc_enum {
	CHUNK_ALLOC_NO_FORCE,
	CHUNK_ALLOC_LIMITED,
	CHUNK_ALLOC_FORCE,
	CHUNK_ALLOC_FORCE_FOR_EXTENT,
};
enum btrfs_block_group_flags {
	BLOCK_GROUP_FLAG_IREF,
	BLOCK_GROUP_FLAG_REMOVED,
	BLOCK_GROUP_FLAG_TO_COPY,
	BLOCK_GROUP_FLAG_RELOCATING_REPAIR,
	BLOCK_GROUP_FLAG_CHUNK_ITEM_INSERTED,
	BLOCK_GROUP_FLAG_ZONE_IS_ACTIVE,
	BLOCK_GROUP_FLAG_ZONED_DATA_RELOC,
	BLOCK_GROUP_FLAG_NEEDS_FREE_SPACE,
	BLOCK_GROUP_FLAG_SEQUENTIAL_ZONE,
	BLOCK_GROUP_FLAG_NEW,
};
enum btrfs_caching_type {
	BTRFS_CACHE_NO,
	BTRFS_CACHE_STARTED,
	BTRFS_CACHE_FINISHED,
	BTRFS_CACHE_ERROR,
};
struct btrfs_caching_control {
	struct list_head list;
	struct mutex mutex;
	wait_queue_head_t wait;
	struct btrfs_work work;
	struct btrfs_block_group *block_group;
	atomic_t progress;
	refcount_t count;
};
#define CACHING_CTL_WAKE_UP SZ_2M
struct btrfs_block_group {
	struct btrfs_fs_info *fs_info;
	struct inode *inode;
	spinlock_t lock;
	u64 start;
	u64 length;
	u64 pinned;
	u64 reserved;
	u64 used;
	u64 delalloc_bytes;
	u64 bytes_super;
	u64 flags;
	u64 cache_generation;
	u64 global_root_id;
	u64 commit_used;
	u32 bitmap_high_thresh;
	u32 bitmap_low_thresh;
	struct rw_semaphore data_rwsem;
	unsigned long full_stripe_len;
	unsigned long runtime_flags;
	unsigned int ro;
	int disk_cache_state;
	int cached;
	struct btrfs_caching_control *caching_ctl;
	struct btrfs_space_info *space_info;
	struct btrfs_free_space_ctl *free_space_ctl;
	struct rb_node cache_node;
	struct list_head list;
	refcount_t refs;
	struct list_head cluster_list;
	struct list_head bg_list;
	struct list_head ro_list;
	atomic_t frozen;
	struct list_head discard_list;
	int discard_index;
	u64 discard_eligible_time;
	u64 discard_cursor;
	enum btrfs_discard_state discard_state;
	struct list_head dirty_list;
	struct list_head io_list;
	struct btrfs_io_ctl io_ctl;
	atomic_t reservations;
	atomic_t nocow_writers;
	struct mutex free_space_lock;
	int swap_extents;
	u64 alloc_offset;
	u64 zone_unusable;
	u64 zone_capacity;
	u64 meta_write_pointer;
	struct map_lookup *physical_map;
	struct list_head active_bg_list;
	struct work_struct zone_finish_work;
	struct extent_buffer *last_eb;
	enum btrfs_block_group_size_class size_class;
};
static inline u64 btrfs_block_group_end(struct btrfs_block_group *block_group)
{
	return (block_group->start + block_group->length);
}
static inline bool btrfs_is_block_group_data_only(
					struct btrfs_block_group *block_group)
{
	return (block_group->flags & BTRFS_BLOCK_GROUP_DATA) &&
	       !(block_group->flags & BTRFS_BLOCK_GROUP_METADATA);
}
#ifdef CONFIG_BTRFS_DEBUG
int btrfs_should_fragment_free_space(struct btrfs_block_group *block_group);
#endif
struct btrfs_block_group *btrfs_lookup_first_block_group(
		struct btrfs_fs_info *info, u64 bytenr);
struct btrfs_block_group *btrfs_lookup_block_group(
		struct btrfs_fs_info *info, u64 bytenr);
struct btrfs_block_group *btrfs_next_block_group(
		struct btrfs_block_group *cache);
void btrfs_get_block_group(struct btrfs_block_group *cache);
void btrfs_put_block_group(struct btrfs_block_group *cache);
void btrfs_dec_block_group_reservations(struct btrfs_fs_info *fs_info,
					const u64 start);
void btrfs_wait_block_group_reservations(struct btrfs_block_group *bg);
struct btrfs_block_group *btrfs_inc_nocow_writers(struct btrfs_fs_info *fs_info,
						  u64 bytenr);
void btrfs_dec_nocow_writers(struct btrfs_block_group *bg);
void btrfs_wait_nocow_writers(struct btrfs_block_group *bg);
void btrfs_wait_block_group_cache_progress(struct btrfs_block_group *cache,
				           u64 num_bytes);
int btrfs_cache_block_group(struct btrfs_block_group *cache, bool wait);
void btrfs_put_caching_control(struct btrfs_caching_control *ctl);
struct btrfs_caching_control *btrfs_get_caching_control(
		struct btrfs_block_group *cache);
int btrfs_add_new_free_space(struct btrfs_block_group *block_group,
			     u64 start, u64 end, u64 *total_added_ret);
struct btrfs_trans_handle *btrfs_start_trans_remove_block_group(
				struct btrfs_fs_info *fs_info,
				const u64 chunk_offset);
int btrfs_remove_block_group(struct btrfs_trans_handle *trans,
			     u64 group_start, struct extent_map *em);
void btrfs_delete_unused_bgs(struct btrfs_fs_info *fs_info);
void btrfs_mark_bg_unused(struct btrfs_block_group *bg);
void btrfs_reclaim_bgs_work(struct work_struct *work);
void btrfs_reclaim_bgs(struct btrfs_fs_info *fs_info);
void btrfs_mark_bg_to_reclaim(struct btrfs_block_group *bg);
int btrfs_read_block_groups(struct btrfs_fs_info *info);
struct btrfs_block_group *btrfs_make_block_group(struct btrfs_trans_handle *trans,
						 u64 type,
						 u64 chunk_offset, u64 size);
void btrfs_create_pending_block_groups(struct btrfs_trans_handle *trans);
int btrfs_inc_block_group_ro(struct btrfs_block_group *cache,
			     bool do_chunk_alloc);
void btrfs_dec_block_group_ro(struct btrfs_block_group *cache);
int btrfs_start_dirty_block_groups(struct btrfs_trans_handle *trans);
int btrfs_write_dirty_block_groups(struct btrfs_trans_handle *trans);
int btrfs_setup_space_cache(struct btrfs_trans_handle *trans);
int btrfs_update_block_group(struct btrfs_trans_handle *trans,
			     u64 bytenr, u64 num_bytes, bool alloc);
int btrfs_add_reserved_bytes(struct btrfs_block_group *cache,
			     u64 ram_bytes, u64 num_bytes, int delalloc,
			     bool force_wrong_size_class);
void btrfs_free_reserved_bytes(struct btrfs_block_group *cache,
			       u64 num_bytes, int delalloc);
int btrfs_chunk_alloc(struct btrfs_trans_handle *trans, u64 flags,
		      enum btrfs_chunk_alloc_enum force);
int btrfs_force_chunk_alloc(struct btrfs_trans_handle *trans, u64 type);
void check_system_chunk(struct btrfs_trans_handle *trans, const u64 type);
void btrfs_reserve_chunk_metadata(struct btrfs_trans_handle *trans,
				  bool is_item_insertion);
u64 btrfs_get_alloc_profile(struct btrfs_fs_info *fs_info, u64 orig_flags);
void btrfs_put_block_group_cache(struct btrfs_fs_info *info);
int btrfs_free_block_groups(struct btrfs_fs_info *info);
int btrfs_rmap_block(struct btrfs_fs_info *fs_info, u64 chunk_start,
		     u64 physical, u64 **logical, int *naddrs, int *stripe_len);
static inline u64 btrfs_data_alloc_profile(struct btrfs_fs_info *fs_info)
{
	return btrfs_get_alloc_profile(fs_info, BTRFS_BLOCK_GROUP_DATA);
}
static inline u64 btrfs_metadata_alloc_profile(struct btrfs_fs_info *fs_info)
{
	return btrfs_get_alloc_profile(fs_info, BTRFS_BLOCK_GROUP_METADATA);
}
static inline u64 btrfs_system_alloc_profile(struct btrfs_fs_info *fs_info)
{
	return btrfs_get_alloc_profile(fs_info, BTRFS_BLOCK_GROUP_SYSTEM);
}
static inline int btrfs_block_group_done(struct btrfs_block_group *cache)
{
	smp_mb();
	return cache->cached == BTRFS_CACHE_FINISHED ||
		cache->cached == BTRFS_CACHE_ERROR;
}
void btrfs_freeze_block_group(struct btrfs_block_group *cache);
void btrfs_unfreeze_block_group(struct btrfs_block_group *cache);
bool btrfs_inc_block_group_swap_extents(struct btrfs_block_group *bg);
void btrfs_dec_block_group_swap_extents(struct btrfs_block_group *bg, int amount);
enum btrfs_block_group_size_class btrfs_calc_block_group_size_class(u64 size);
int btrfs_use_block_group_size_class(struct btrfs_block_group *bg,
				     enum btrfs_block_group_size_class size_class,
				     bool force_wrong_size_class);
bool btrfs_block_group_should_use_size_class(struct btrfs_block_group *bg);
#endif  



#include "misc.h"
#include "ctree.h"
#include "space-info.h"
#include "sysfs.h"
#include "volumes.h"
#include "free-space-cache.h"
#include "ordered-data.h"
#include "transaction.h"
#include "block-group.h"
#include "zoned.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"

 

u64 __pure btrfs_space_info_used(struct btrfs_space_info *s_info,
			  bool may_use_included)
{
	ASSERT(s_info);
	return s_info->bytes_used + s_info->bytes_reserved +
		s_info->bytes_pinned + s_info->bytes_readonly +
		s_info->bytes_zone_unusable +
		(may_use_included ? s_info->bytes_may_use : 0);
}

 
void btrfs_clear_space_info_full(struct btrfs_fs_info *info)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	list_for_each_entry(found, head, list)
		found->full = 0;
}

 
#define BTRFS_DEFAULT_ZONED_RECLAIM_THRESH			(75)

 
static u64 calc_chunk_size(const struct btrfs_fs_info *fs_info, u64 flags)
{
	if (btrfs_is_zoned(fs_info))
		return fs_info->zone_size;

	ASSERT(flags & BTRFS_BLOCK_GROUP_TYPE_MASK);

	if (flags & BTRFS_BLOCK_GROUP_DATA)
		return BTRFS_MAX_DATA_CHUNK_SIZE;
	else if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		return SZ_32M;

	 
	if (fs_info->fs_devices->total_rw_bytes > 50ULL * SZ_1G)
		return SZ_1G;

	return SZ_256M;
}

 
void btrfs_update_space_info_chunk_size(struct btrfs_space_info *space_info,
					u64 chunk_size)
{
	WRITE_ONCE(space_info->chunk_size, chunk_size);
}

static int create_space_info(struct btrfs_fs_info *info, u64 flags)
{

	struct btrfs_space_info *space_info;
	int i;
	int ret;

	space_info = kzalloc(sizeof(*space_info), GFP_NOFS);
	if (!space_info)
		return -ENOMEM;

	for (i = 0; i < BTRFS_NR_RAID_TYPES; i++)
		INIT_LIST_HEAD(&space_info->block_groups[i]);
	init_rwsem(&space_info->groups_sem);
	spin_lock_init(&space_info->lock);
	space_info->flags = flags & BTRFS_BLOCK_GROUP_TYPE_MASK;
	space_info->force_alloc = CHUNK_ALLOC_NO_FORCE;
	INIT_LIST_HEAD(&space_info->ro_bgs);
	INIT_LIST_HEAD(&space_info->tickets);
	INIT_LIST_HEAD(&space_info->priority_tickets);
	space_info->clamp = 1;
	btrfs_update_space_info_chunk_size(space_info, calc_chunk_size(info, flags));

	if (btrfs_is_zoned(info))
		space_info->bg_reclaim_threshold = BTRFS_DEFAULT_ZONED_RECLAIM_THRESH;

	ret = btrfs_sysfs_add_space_info_type(info, space_info);
	if (ret)
		return ret;

	list_add(&space_info->list, &info->space_info);
	if (flags & BTRFS_BLOCK_GROUP_DATA)
		info->data_sinfo = space_info;

	return ret;
}

int btrfs_init_space_info(struct btrfs_fs_info *fs_info)
{
	struct btrfs_super_block *disk_super;
	u64 features;
	u64 flags;
	int mixed = 0;
	int ret;

	disk_super = fs_info->super_copy;
	if (!btrfs_super_root(disk_super))
		return -EINVAL;

	features = btrfs_super_incompat_flags(disk_super);
	if (features & BTRFS_FEATURE_INCOMPAT_MIXED_GROUPS)
		mixed = 1;

	flags = BTRFS_BLOCK_GROUP_SYSTEM;
	ret = create_space_info(fs_info, flags);
	if (ret)
		goto out;

	if (mixed) {
		flags = BTRFS_BLOCK_GROUP_METADATA | BTRFS_BLOCK_GROUP_DATA;
		ret = create_space_info(fs_info, flags);
	} else {
		flags = BTRFS_BLOCK_GROUP_METADATA;
		ret = create_space_info(fs_info, flags);
		if (ret)
			goto out;

		flags = BTRFS_BLOCK_GROUP_DATA;
		ret = create_space_info(fs_info, flags);
	}
out:
	return ret;
}

void btrfs_add_bg_to_space_info(struct btrfs_fs_info *info,
				struct btrfs_block_group *block_group)
{
	struct btrfs_space_info *found;
	int factor, index;

	factor = btrfs_bg_type_to_factor(block_group->flags);

	found = btrfs_find_space_info(info, block_group->flags);
	ASSERT(found);
	spin_lock(&found->lock);
	found->total_bytes += block_group->length;
	found->disk_total += block_group->length * factor;
	found->bytes_used += block_group->used;
	found->disk_used += block_group->used * factor;
	found->bytes_readonly += block_group->bytes_super;
	found->bytes_zone_unusable += block_group->zone_unusable;
	if (block_group->length > 0)
		found->full = 0;
	btrfs_try_granting_tickets(info, found);
	spin_unlock(&found->lock);

	block_group->space_info = found;

	index = btrfs_bg_flags_to_raid_index(block_group->flags);
	down_write(&found->groups_sem);
	list_add_tail(&block_group->list, &found->block_groups[index]);
	up_write(&found->groups_sem);
}

struct btrfs_space_info *btrfs_find_space_info(struct btrfs_fs_info *info,
					       u64 flags)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	flags &= BTRFS_BLOCK_GROUP_TYPE_MASK;

	list_for_each_entry(found, head, list) {
		if (found->flags & flags)
			return found;
	}
	return NULL;
}

static u64 calc_available_free_space(struct btrfs_fs_info *fs_info,
			  struct btrfs_space_info *space_info,
			  enum btrfs_reserve_flush_enum flush)
{
	u64 profile;
	u64 avail;
	int factor;

	if (space_info->flags & BTRFS_BLOCK_GROUP_SYSTEM)
		profile = btrfs_system_alloc_profile(fs_info);
	else
		profile = btrfs_metadata_alloc_profile(fs_info);

	avail = atomic64_read(&fs_info->free_chunk_space);

	 
	factor = btrfs_bg_type_to_factor(profile);
	avail = div_u64(avail, factor);

	 
	if (flush == BTRFS_RESERVE_FLUSH_ALL)
		avail >>= 3;
	else
		avail >>= 1;
	return avail;
}

int btrfs_can_overcommit(struct btrfs_fs_info *fs_info,
			 struct btrfs_space_info *space_info, u64 bytes,
			 enum btrfs_reserve_flush_enum flush)
{
	u64 avail;
	u64 used;

	 
	if (space_info->flags & BTRFS_BLOCK_GROUP_DATA)
		return 0;

	used = btrfs_space_info_used(space_info, true);
	avail = calc_available_free_space(fs_info, space_info, flush);

	if (used + bytes < space_info->total_bytes + avail)
		return 1;
	return 0;
}

static void remove_ticket(struct btrfs_space_info *space_info,
			  struct reserve_ticket *ticket)
{
	if (!list_empty(&ticket->list)) {
		list_del_init(&ticket->list);
		ASSERT(space_info->reclaim_size >= ticket->bytes);
		space_info->reclaim_size -= ticket->bytes;
	}
}

 
void btrfs_try_granting_tickets(struct btrfs_fs_info *fs_info,
				struct btrfs_space_info *space_info)
{
	struct list_head *head;
	enum btrfs_reserve_flush_enum flush = BTRFS_RESERVE_NO_FLUSH;

	lockdep_assert_held(&space_info->lock);

	head = &space_info->priority_tickets;
again:
	while (!list_empty(head)) {
		struct reserve_ticket *ticket;
		u64 used = btrfs_space_info_used(space_info, true);

		ticket = list_first_entry(head, struct reserve_ticket, list);

		 
		if ((used + ticket->bytes <= space_info->total_bytes) ||
		    btrfs_can_overcommit(fs_info, space_info, ticket->bytes,
					 flush)) {
			btrfs_space_info_update_bytes_may_use(fs_info,
							      space_info,
							      ticket->bytes);
			remove_ticket(space_info, ticket);
			ticket->bytes = 0;
			space_info->tickets_id++;
			wake_up(&ticket->wait);
		} else {
			break;
		}
	}

	if (head == &space_info->priority_tickets) {
		head = &space_info->tickets;
		flush = BTRFS_RESERVE_FLUSH_ALL;
		goto again;
	}
}

#define DUMP_BLOCK_RSV(fs_info, rsv_name)				\
do {									\
	struct btrfs_block_rsv *__rsv = &(fs_info)->rsv_name;		\
	spin_lock(&__rsv->lock);					\
	btrfs_info(fs_info, #rsv_name ": size %llu reserved %llu",	\
		   __rsv->size, __rsv->reserved);			\
	spin_unlock(&__rsv->lock);					\
} while (0)

static const char *space_info_flag_to_str(const struct btrfs_space_info *space_info)
{
	switch (space_info->flags) {
	case BTRFS_BLOCK_GROUP_SYSTEM:
		return "SYSTEM";
	case BTRFS_BLOCK_GROUP_METADATA | BTRFS_BLOCK_GROUP_DATA:
		return "DATA+METADATA";
	case BTRFS_BLOCK_GROUP_DATA:
		return "DATA";
	case BTRFS_BLOCK_GROUP_METADATA:
		return "METADATA";
	default:
		return "UNKNOWN";
	}
}

static void dump_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	DUMP_BLOCK_RSV(fs_info, global_block_rsv);
	DUMP_BLOCK_RSV(fs_info, trans_block_rsv);
	DUMP_BLOCK_RSV(fs_info, chunk_block_rsv);
	DUMP_BLOCK_RSV(fs_info, delayed_block_rsv);
	DUMP_BLOCK_RSV(fs_info, delayed_refs_rsv);
}

static void __btrfs_dump_space_info(struct btrfs_fs_info *fs_info,
				    struct btrfs_space_info *info)
{
	const char *flag_str = space_info_flag_to_str(info);
	lockdep_assert_held(&info->lock);

	 
	btrfs_info(fs_info, "space_info %s has %lld free, is %sfull",
		   flag_str,
		   (s64)(info->total_bytes - btrfs_space_info_used(info, true)),
		   info->full ? "" : "not ");
	btrfs_info(fs_info,
"space_info total=%llu, used=%llu, pinned=%llu, reserved=%llu, may_use=%llu, readonly=%llu zone_unusable=%llu",
		info->total_bytes, info->bytes_used, info->bytes_pinned,
		info->bytes_reserved, info->bytes_may_use,
		info->bytes_readonly, info->bytes_zone_unusable);
}

void btrfs_dump_space_info(struct btrfs_fs_info *fs_info,
			   struct btrfs_space_info *info, u64 bytes,
			   int dump_block_groups)
{
	struct btrfs_block_group *cache;
	u64 total_avail = 0;
	int index = 0;

	spin_lock(&info->lock);
	__btrfs_dump_space_info(fs_info, info);
	dump_global_block_rsv(fs_info);
	spin_unlock(&info->lock);

	if (!dump_block_groups)
		return;

	down_read(&info->groups_sem);
again:
	list_for_each_entry(cache, &info->block_groups[index], list) {
		u64 avail;

		spin_lock(&cache->lock);
		avail = cache->length - cache->used - cache->pinned -
			cache->reserved - cache->delalloc_bytes -
			cache->bytes_super - cache->zone_unusable;
		btrfs_info(fs_info,
"block group %llu has %llu bytes, %llu used %llu pinned %llu reserved %llu delalloc %llu super %llu zone_unusable (%llu bytes available) %s",
			   cache->start, cache->length, cache->used, cache->pinned,
			   cache->reserved, cache->delalloc_bytes,
			   cache->bytes_super, cache->zone_unusable,
			   avail, cache->ro ? "[readonly]" : "");
		spin_unlock(&cache->lock);
		btrfs_dump_free_space(cache, bytes);
		total_avail += avail;
	}
	if (++index < BTRFS_NR_RAID_TYPES)
		goto again;
	up_read(&info->groups_sem);

	btrfs_info(fs_info, "%llu bytes available across all block groups", total_avail);
}

static inline u64 calc_reclaim_items_nr(const struct btrfs_fs_info *fs_info,
					u64 to_reclaim)
{
	u64 bytes;
	u64 nr;

	bytes = btrfs_calc_insert_metadata_size(fs_info, 1);
	nr = div64_u64(to_reclaim, bytes);
	if (!nr)
		nr = 1;
	return nr;
}

static inline u64 calc_delayed_refs_nr(const struct btrfs_fs_info *fs_info,
				       u64 to_reclaim)
{
	const u64 bytes = btrfs_calc_delayed_ref_bytes(fs_info, 1);
	u64 nr;

	nr = div64_u64(to_reclaim, bytes);
	if (!nr)
		nr = 1;
	return nr;
}

#define EXTENT_SIZE_PER_ITEM	SZ_256K

 
static void shrink_delalloc(struct btrfs_fs_info *fs_info,
			    struct btrfs_space_info *space_info,
			    u64 to_reclaim, bool wait_ordered,
			    bool for_preempt)
{
	struct btrfs_trans_handle *trans;
	u64 delalloc_bytes;
	u64 ordered_bytes;
	u64 items;
	long time_left;
	int loops;

	delalloc_bytes = percpu_counter_sum_positive(&fs_info->delalloc_bytes);
	ordered_bytes = percpu_counter_sum_positive(&fs_info->ordered_bytes);
	if (delalloc_bytes == 0 && ordered_bytes == 0)
		return;

	 
	if (to_reclaim == U64_MAX) {
		items = U64_MAX;
	} else {
		 
		to_reclaim = max(to_reclaim, delalloc_bytes >> 3);
		items = calc_reclaim_items_nr(fs_info, to_reclaim) * 2;
	}

	trans = current->journal_info;

	 
	if (ordered_bytes > delalloc_bytes && !for_preempt)
		wait_ordered = true;

	loops = 0;
	while ((delalloc_bytes || ordered_bytes) && loops < 3) {
		u64 temp = min(delalloc_bytes, to_reclaim) >> PAGE_SHIFT;
		long nr_pages = min_t(u64, temp, LONG_MAX);
		int async_pages;

		btrfs_start_delalloc_roots(fs_info, nr_pages, true);

		 
		async_pages = atomic_read(&fs_info->async_delalloc_pages);
		if (!async_pages)
			goto skip_async;

		 
		if (async_pages > nr_pages)
			async_pages -= nr_pages;
		else
			async_pages = 0;
		wait_event(fs_info->async_submit_wait,
			   atomic_read(&fs_info->async_delalloc_pages) <=
			   async_pages);
skip_async:
		loops++;
		if (wait_ordered && !trans) {
			btrfs_wait_ordered_roots(fs_info, items, 0, (u64)-1);
		} else {
			time_left = schedule_timeout_killable(1);
			if (time_left)
				break;
		}

		 
		if (for_preempt)
			break;

		spin_lock(&space_info->lock);
		if (list_empty(&space_info->tickets) &&
		    list_empty(&space_info->priority_tickets)) {
			spin_unlock(&space_info->lock);
			break;
		}
		spin_unlock(&space_info->lock);

		delalloc_bytes = percpu_counter_sum_positive(
						&fs_info->delalloc_bytes);
		ordered_bytes = percpu_counter_sum_positive(
						&fs_info->ordered_bytes);
	}
}

 
static void flush_space(struct btrfs_fs_info *fs_info,
		       struct btrfs_space_info *space_info, u64 num_bytes,
		       enum btrfs_flush_state state, bool for_preempt)
{
	struct btrfs_root *root = fs_info->tree_root;
	struct btrfs_trans_handle *trans;
	int nr;
	int ret = 0;

	switch (state) {
	case FLUSH_DELAYED_ITEMS_NR:
	case FLUSH_DELAYED_ITEMS:
		if (state == FLUSH_DELAYED_ITEMS_NR)
			nr = calc_reclaim_items_nr(fs_info, num_bytes) * 2;
		else
			nr = -1;

		trans = btrfs_join_transaction_nostart(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			if (ret == -ENOENT)
				ret = 0;
			break;
		}
		ret = btrfs_run_delayed_items_nr(trans, nr);
		btrfs_end_transaction(trans);
		break;
	case FLUSH_DELALLOC:
	case FLUSH_DELALLOC_WAIT:
	case FLUSH_DELALLOC_FULL:
		if (state == FLUSH_DELALLOC_FULL)
			num_bytes = U64_MAX;
		shrink_delalloc(fs_info, space_info, num_bytes,
				state != FLUSH_DELALLOC, for_preempt);
		break;
	case FLUSH_DELAYED_REFS_NR:
	case FLUSH_DELAYED_REFS:
		trans = btrfs_join_transaction_nostart(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			if (ret == -ENOENT)
				ret = 0;
			break;
		}
		if (state == FLUSH_DELAYED_REFS_NR)
			nr = calc_delayed_refs_nr(fs_info, num_bytes);
		else
			nr = 0;
		btrfs_run_delayed_refs(trans, nr);
		btrfs_end_transaction(trans);
		break;
	case ALLOC_CHUNK:
	case ALLOC_CHUNK_FORCE:
		trans = btrfs_join_transaction(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			break;
		}
		ret = btrfs_chunk_alloc(trans,
				btrfs_get_alloc_profile(fs_info, space_info->flags),
				(state == ALLOC_CHUNK) ? CHUNK_ALLOC_NO_FORCE :
					CHUNK_ALLOC_FORCE);
		btrfs_end_transaction(trans);

		if (ret > 0 || ret == -ENOSPC)
			ret = 0;
		break;
	case RUN_DELAYED_IPUTS:
		 
		btrfs_run_delayed_iputs(fs_info);
		btrfs_wait_on_delayed_iputs(fs_info);
		break;
	case COMMIT_TRANS:
		ASSERT(current->journal_info == NULL);
		 
		trans = btrfs_attach_transaction_barrier(root);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			if (ret == -ENOENT)
				ret = 0;
			break;
		}
		ret = btrfs_commit_transaction(trans);
		break;
	default:
		ret = -ENOSPC;
		break;
	}

	trace_btrfs_flush_space(fs_info, space_info->flags, num_bytes, state,
				ret, for_preempt);
	return;
}

static inline u64
btrfs_calc_reclaim_metadata_size(struct btrfs_fs_info *fs_info,
				 struct btrfs_space_info *space_info)
{
	u64 used;
	u64 avail;
	u64 to_reclaim = space_info->reclaim_size;

	lockdep_assert_held(&space_info->lock);

	avail = calc_available_free_space(fs_info, space_info,
					  BTRFS_RESERVE_FLUSH_ALL);
	used = btrfs_space_info_used(space_info, true);

	 
	if (space_info->total_bytes + avail < used)
		to_reclaim += used - (space_info->total_bytes + avail);

	return to_reclaim;
}

static bool need_preemptive_reclaim(struct btrfs_fs_info *fs_info,
				    struct btrfs_space_info *space_info)
{
	u64 global_rsv_size = fs_info->global_block_rsv.reserved;
	u64 ordered, delalloc;
	u64 thresh;
	u64 used;

	thresh = mult_perc(space_info->total_bytes, 90);

	lockdep_assert_held(&space_info->lock);

	 
	if ((space_info->bytes_used + space_info->bytes_reserved +
	     global_rsv_size) >= thresh)
		return false;

	used = space_info->bytes_may_use + space_info->bytes_pinned;

	 
	if (global_rsv_size >= used)
		return false;

	 
	if (used - global_rsv_size <= SZ_128M)
		return false;

	 
	if (space_info->reclaim_size)
		return false;

	 

	thresh = calc_available_free_space(fs_info, space_info,
					   BTRFS_RESERVE_FLUSH_ALL);
	used = space_info->bytes_used + space_info->bytes_reserved +
	       space_info->bytes_readonly + global_rsv_size;
	if (used < space_info->total_bytes)
		thresh += space_info->total_bytes - used;
	thresh >>= space_info->clamp;

	used = space_info->bytes_pinned;

	 
	ordered = percpu_counter_read_positive(&fs_info->ordered_bytes) >> 1;
	delalloc = percpu_counter_read_positive(&fs_info->delalloc_bytes);
	if (ordered >= delalloc)
		used += fs_info->delayed_refs_rsv.reserved +
			fs_info->delayed_block_rsv.reserved;
	else
		used += space_info->bytes_may_use - global_rsv_size;

	return (used >= thresh && !btrfs_fs_closing(fs_info) &&
		!test_bit(BTRFS_FS_STATE_REMOUNTING, &fs_info->fs_state));
}

static bool steal_from_global_rsv(struct btrfs_fs_info *fs_info,
				  struct btrfs_space_info *space_info,
				  struct reserve_ticket *ticket)
{
	struct btrfs_block_rsv *global_rsv = &fs_info->global_block_rsv;
	u64 min_bytes;

	if (!ticket->steal)
		return false;

	if (global_rsv->space_info != space_info)
		return false;

	spin_lock(&global_rsv->lock);
	min_bytes = mult_perc(global_rsv->size, 10);
	if (global_rsv->reserved < min_bytes + ticket->bytes) {
		spin_unlock(&global_rsv->lock);
		return false;
	}
	global_rsv->reserved -= ticket->bytes;
	remove_ticket(space_info, ticket);
	ticket->bytes = 0;
	wake_up(&ticket->wait);
	space_info->tickets_id++;
	if (global_rsv->reserved < global_rsv->size)
		global_rsv->full = 0;
	spin_unlock(&global_rsv->lock);

	return true;
}

 
static bool maybe_fail_all_tickets(struct btrfs_fs_info *fs_info,
				   struct btrfs_space_info *space_info)
{
	struct reserve_ticket *ticket;
	u64 tickets_id = space_info->tickets_id;
	const bool aborted = BTRFS_FS_ERROR(fs_info);

	trace_btrfs_fail_all_tickets(fs_info, space_info);

	if (btrfs_test_opt(fs_info, ENOSPC_DEBUG)) {
		btrfs_info(fs_info, "cannot satisfy tickets, dumping space info");
		__btrfs_dump_space_info(fs_info, space_info);
	}

	while (!list_empty(&space_info->tickets) &&
	       tickets_id == space_info->tickets_id) {
		ticket = list_first_entry(&space_info->tickets,
					  struct reserve_ticket, list);

		if (!aborted && steal_from_global_rsv(fs_info, space_info, ticket))
			return true;

		if (!aborted && btrfs_test_opt(fs_info, ENOSPC_DEBUG))
			btrfs_info(fs_info, "failing ticket with %llu bytes",
				   ticket->bytes);

		remove_ticket(space_info, ticket);
		if (aborted)
			ticket->error = -EIO;
		else
			ticket->error = -ENOSPC;
		wake_up(&ticket->wait);

		 
		if (!aborted)
			btrfs_try_granting_tickets(fs_info, space_info);
	}
	return (tickets_id != space_info->tickets_id);
}

 
static void btrfs_async_reclaim_metadata_space(struct work_struct *work)
{
	struct btrfs_fs_info *fs_info;
	struct btrfs_space_info *space_info;
	u64 to_reclaim;
	enum btrfs_flush_state flush_state;
	int commit_cycles = 0;
	u64 last_tickets_id;

	fs_info = container_of(work, struct btrfs_fs_info, async_reclaim_work);
	space_info = btrfs_find_space_info(fs_info, BTRFS_BLOCK_GROUP_METADATA);

	spin_lock(&space_info->lock);
	to_reclaim = btrfs_calc_reclaim_metadata_size(fs_info, space_info);
	if (!to_reclaim) {
		space_info->flush = 0;
		spin_unlock(&space_info->lock);
		return;
	}
	last_tickets_id = space_info->tickets_id;
	spin_unlock(&space_info->lock);

	flush_state = FLUSH_DELAYED_ITEMS_NR;
	do {
		flush_space(fs_info, space_info, to_reclaim, flush_state, false);
		spin_lock(&space_info->lock);
		if (list_empty(&space_info->tickets)) {
			space_info->flush = 0;
			spin_unlock(&space_info->lock);
			return;
		}
		to_reclaim = btrfs_calc_reclaim_metadata_size(fs_info,
							      space_info);
		if (last_tickets_id == space_info->tickets_id) {
			flush_state++;
		} else {
			last_tickets_id = space_info->tickets_id;
			flush_state = FLUSH_DELAYED_ITEMS_NR;
			if (commit_cycles)
				commit_cycles--;
		}

		 
		if (flush_state == FLUSH_DELALLOC_FULL && !commit_cycles)
			flush_state++;

		 
		if (flush_state == ALLOC_CHUNK_FORCE && !commit_cycles)
			flush_state++;

		if (flush_state > COMMIT_TRANS) {
			commit_cycles++;
			if (commit_cycles > 2) {
				if (maybe_fail_all_tickets(fs_info, space_info)) {
					flush_state = FLUSH_DELAYED_ITEMS_NR;
					commit_cycles--;
				} else {
					space_info->flush = 0;
				}
			} else {
				flush_state = FLUSH_DELAYED_ITEMS_NR;
			}
		}
		spin_unlock(&space_info->lock);
	} while (flush_state <= COMMIT_TRANS);
}

 
static void btrfs_preempt_reclaim_metadata_space(struct work_struct *work)
{
	struct btrfs_fs_info *fs_info;
	struct btrfs_space_info *space_info;
	struct btrfs_block_rsv *delayed_block_rsv;
	struct btrfs_block_rsv *delayed_refs_rsv;
	struct btrfs_block_rsv *global_rsv;
	struct btrfs_block_rsv *trans_rsv;
	int loops = 0;

	fs_info = container_of(work, struct btrfs_fs_info,
			       preempt_reclaim_work);
	space_info = btrfs_find_space_info(fs_info, BTRFS_BLOCK_GROUP_METADATA);
	delayed_block_rsv = &fs_info->delayed_block_rsv;
	delayed_refs_rsv = &fs_info->delayed_refs_rsv;
	global_rsv = &fs_info->global_block_rsv;
	trans_rsv = &fs_info->trans_block_rsv;

	spin_lock(&space_info->lock);
	while (need_preemptive_reclaim(fs_info, space_info)) {
		enum btrfs_flush_state flush;
		u64 delalloc_size = 0;
		u64 to_reclaim, block_rsv_size;
		u64 global_rsv_size = global_rsv->reserved;

		loops++;

		 
		block_rsv_size = global_rsv_size +
			delayed_block_rsv->reserved +
			delayed_refs_rsv->reserved +
			trans_rsv->reserved;
		if (block_rsv_size < space_info->bytes_may_use)
			delalloc_size = space_info->bytes_may_use - block_rsv_size;

		 
		block_rsv_size -= global_rsv_size;

		 
		if (delalloc_size > block_rsv_size) {
			to_reclaim = delalloc_size;
			flush = FLUSH_DELALLOC;
		} else if (space_info->bytes_pinned >
			   (delayed_block_rsv->reserved +
			    delayed_refs_rsv->reserved)) {
			to_reclaim = space_info->bytes_pinned;
			flush = COMMIT_TRANS;
		} else if (delayed_block_rsv->reserved >
			   delayed_refs_rsv->reserved) {
			to_reclaim = delayed_block_rsv->reserved;
			flush = FLUSH_DELAYED_ITEMS_NR;
		} else {
			to_reclaim = delayed_refs_rsv->reserved;
			flush = FLUSH_DELAYED_REFS_NR;
		}

		spin_unlock(&space_info->lock);

		 
		to_reclaim >>= 2;
		if (!to_reclaim)
			to_reclaim = btrfs_calc_insert_metadata_size(fs_info, 1);
		flush_space(fs_info, space_info, to_reclaim, flush, true);
		cond_resched();
		spin_lock(&space_info->lock);
	}

	 
	if (loops == 1 && !space_info->reclaim_size)
		space_info->clamp = max(1, space_info->clamp - 1);
	trace_btrfs_done_preemptive_reclaim(fs_info, space_info);
	spin_unlock(&space_info->lock);
}

 
static const enum btrfs_flush_state data_flush_states[] = {
	FLUSH_DELALLOC_FULL,
	RUN_DELAYED_IPUTS,
	COMMIT_TRANS,
	ALLOC_CHUNK_FORCE,
};

static void btrfs_async_reclaim_data_space(struct work_struct *work)
{
	struct btrfs_fs_info *fs_info;
	struct btrfs_space_info *space_info;
	u64 last_tickets_id;
	enum btrfs_flush_state flush_state = 0;

	fs_info = container_of(work, struct btrfs_fs_info, async_data_reclaim_work);
	space_info = fs_info->data_sinfo;

	spin_lock(&space_info->lock);
	if (list_empty(&space_info->tickets)) {
		space_info->flush = 0;
		spin_unlock(&space_info->lock);
		return;
	}
	last_tickets_id = space_info->tickets_id;
	spin_unlock(&space_info->lock);

	while (!space_info->full) {
		flush_space(fs_info, space_info, U64_MAX, ALLOC_CHUNK_FORCE, false);
		spin_lock(&space_info->lock);
		if (list_empty(&space_info->tickets)) {
			space_info->flush = 0;
			spin_unlock(&space_info->lock);
			return;
		}

		 
		if (BTRFS_FS_ERROR(fs_info))
			goto aborted_fs;
		last_tickets_id = space_info->tickets_id;
		spin_unlock(&space_info->lock);
	}

	while (flush_state < ARRAY_SIZE(data_flush_states)) {
		flush_space(fs_info, space_info, U64_MAX,
			    data_flush_states[flush_state], false);
		spin_lock(&space_info->lock);
		if (list_empty(&space_info->tickets)) {
			space_info->flush = 0;
			spin_unlock(&space_info->lock);
			return;
		}

		if (last_tickets_id == space_info->tickets_id) {
			flush_state++;
		} else {
			last_tickets_id = space_info->tickets_id;
			flush_state = 0;
		}

		if (flush_state >= ARRAY_SIZE(data_flush_states)) {
			if (space_info->full) {
				if (maybe_fail_all_tickets(fs_info, space_info))
					flush_state = 0;
				else
					space_info->flush = 0;
			} else {
				flush_state = 0;
			}

			 
			if (BTRFS_FS_ERROR(fs_info))
				goto aborted_fs;

		}
		spin_unlock(&space_info->lock);
	}
	return;

aborted_fs:
	maybe_fail_all_tickets(fs_info, space_info);
	space_info->flush = 0;
	spin_unlock(&space_info->lock);
}

void btrfs_init_async_reclaim_work(struct btrfs_fs_info *fs_info)
{
	INIT_WORK(&fs_info->async_reclaim_work, btrfs_async_reclaim_metadata_space);
	INIT_WORK(&fs_info->async_data_reclaim_work, btrfs_async_reclaim_data_space);
	INIT_WORK(&fs_info->preempt_reclaim_work,
		  btrfs_preempt_reclaim_metadata_space);
}

static const enum btrfs_flush_state priority_flush_states[] = {
	FLUSH_DELAYED_ITEMS_NR,
	FLUSH_DELAYED_ITEMS,
	ALLOC_CHUNK,
};

static const enum btrfs_flush_state evict_flush_states[] = {
	FLUSH_DELAYED_ITEMS_NR,
	FLUSH_DELAYED_ITEMS,
	FLUSH_DELAYED_REFS_NR,
	FLUSH_DELAYED_REFS,
	FLUSH_DELALLOC,
	FLUSH_DELALLOC_WAIT,
	FLUSH_DELALLOC_FULL,
	ALLOC_CHUNK,
	COMMIT_TRANS,
};

static void priority_reclaim_metadata_space(struct btrfs_fs_info *fs_info,
				struct btrfs_space_info *space_info,
				struct reserve_ticket *ticket,
				const enum btrfs_flush_state *states,
				int states_nr)
{
	u64 to_reclaim;
	int flush_state = 0;

	spin_lock(&space_info->lock);
	to_reclaim = btrfs_calc_reclaim_metadata_size(fs_info, space_info);
	 
	if (ticket->bytes == 0) {
		spin_unlock(&space_info->lock);
		return;
	}

	while (flush_state < states_nr) {
		spin_unlock(&space_info->lock);
		flush_space(fs_info, space_info, to_reclaim, states[flush_state],
			    false);
		flush_state++;
		spin_lock(&space_info->lock);
		if (ticket->bytes == 0) {
			spin_unlock(&space_info->lock);
			return;
		}
	}

	 
	if (BTRFS_FS_ERROR(fs_info)) {
		ticket->error = BTRFS_FS_ERROR(fs_info);
		remove_ticket(space_info, ticket);
	} else if (!steal_from_global_rsv(fs_info, space_info, ticket)) {
		ticket->error = -ENOSPC;
		remove_ticket(space_info, ticket);
	}

	 
	btrfs_try_granting_tickets(fs_info, space_info);
	spin_unlock(&space_info->lock);
}

static void priority_reclaim_data_space(struct btrfs_fs_info *fs_info,
					struct btrfs_space_info *space_info,
					struct reserve_ticket *ticket)
{
	spin_lock(&space_info->lock);

	 
	if (ticket->bytes == 0) {
		spin_unlock(&space_info->lock);
		return;
	}

	while (!space_info->full) {
		spin_unlock(&space_info->lock);
		flush_space(fs_info, space_info, U64_MAX, ALLOC_CHUNK_FORCE, false);
		spin_lock(&space_info->lock);
		if (ticket->bytes == 0) {
			spin_unlock(&space_info->lock);
			return;
		}
	}

	ticket->error = -ENOSPC;
	remove_ticket(space_info, ticket);
	btrfs_try_granting_tickets(fs_info, space_info);
	spin_unlock(&space_info->lock);
}

static void wait_reserve_ticket(struct btrfs_fs_info *fs_info,
				struct btrfs_space_info *space_info,
				struct reserve_ticket *ticket)

{
	DEFINE_WAIT(wait);
	int ret = 0;

	spin_lock(&space_info->lock);
	while (ticket->bytes > 0 && ticket->error == 0) {
		ret = prepare_to_wait_event(&ticket->wait, &wait, TASK_KILLABLE);
		if (ret) {
			 
			remove_ticket(space_info, ticket);
			ticket->error = -EINTR;
			break;
		}
		spin_unlock(&space_info->lock);

		schedule();

		finish_wait(&ticket->wait, &wait);
		spin_lock(&space_info->lock);
	}
	spin_unlock(&space_info->lock);
}

 
static int handle_reserve_ticket(struct btrfs_fs_info *fs_info,
				 struct btrfs_space_info *space_info,
				 struct reserve_ticket *ticket,
				 u64 start_ns, u64 orig_bytes,
				 enum btrfs_reserve_flush_enum flush)
{
	int ret;

	switch (flush) {
	case BTRFS_RESERVE_FLUSH_DATA:
	case BTRFS_RESERVE_FLUSH_ALL:
	case BTRFS_RESERVE_FLUSH_ALL_STEAL:
		wait_reserve_ticket(fs_info, space_info, ticket);
		break;
	case BTRFS_RESERVE_FLUSH_LIMIT:
		priority_reclaim_metadata_space(fs_info, space_info, ticket,
						priority_flush_states,
						ARRAY_SIZE(priority_flush_states));
		break;
	case BTRFS_RESERVE_FLUSH_EVICT:
		priority_reclaim_metadata_space(fs_info, space_info, ticket,
						evict_flush_states,
						ARRAY_SIZE(evict_flush_states));
		break;
	case BTRFS_RESERVE_FLUSH_FREE_SPACE_INODE:
		priority_reclaim_data_space(fs_info, space_info, ticket);
		break;
	default:
		ASSERT(0);
		break;
	}

	ret = ticket->error;
	ASSERT(list_empty(&ticket->list));
	 
	ASSERT(!(ticket->bytes == 0 && ticket->error));
	trace_btrfs_reserve_ticket(fs_info, space_info->flags, orig_bytes,
				   start_ns, flush, ticket->error);
	return ret;
}

 
static inline bool is_normal_flushing(enum btrfs_reserve_flush_enum flush)
{
	return	(flush == BTRFS_RESERVE_FLUSH_ALL) ||
		(flush == BTRFS_RESERVE_FLUSH_ALL_STEAL);
}

static inline void maybe_clamp_preempt(struct btrfs_fs_info *fs_info,
				       struct btrfs_space_info *space_info)
{
	u64 ordered = percpu_counter_sum_positive(&fs_info->ordered_bytes);
	u64 delalloc = percpu_counter_sum_positive(&fs_info->delalloc_bytes);

	 
	if (ordered < delalloc)
		space_info->clamp = min(space_info->clamp + 1, 8);
}

static inline bool can_steal(enum btrfs_reserve_flush_enum flush)
{
	return (flush == BTRFS_RESERVE_FLUSH_ALL_STEAL ||
		flush == BTRFS_RESERVE_FLUSH_EVICT);
}

 
static inline bool can_ticket(enum btrfs_reserve_flush_enum flush)
{
	return (flush != BTRFS_RESERVE_NO_FLUSH &&
		flush != BTRFS_RESERVE_FLUSH_EMERGENCY);
}

 
static int __reserve_bytes(struct btrfs_fs_info *fs_info,
			   struct btrfs_space_info *space_info, u64 orig_bytes,
			   enum btrfs_reserve_flush_enum flush)
{
	struct work_struct *async_work;
	struct reserve_ticket ticket;
	u64 start_ns = 0;
	u64 used;
	int ret = -ENOSPC;
	bool pending_tickets;

	ASSERT(orig_bytes);
	 
	if (current->journal_info) {
		 
		ASSERT(flush != BTRFS_RESERVE_FLUSH_ALL);
		ASSERT(flush != BTRFS_RESERVE_FLUSH_ALL_STEAL);
		ASSERT(flush != BTRFS_RESERVE_FLUSH_EVICT);
	}

	if (flush == BTRFS_RESERVE_FLUSH_DATA)
		async_work = &fs_info->async_data_reclaim_work;
	else
		async_work = &fs_info->async_reclaim_work;

	spin_lock(&space_info->lock);
	used = btrfs_space_info_used(space_info, true);

	 
	if (is_normal_flushing(flush) || (flush == BTRFS_RESERVE_NO_FLUSH))
		pending_tickets = !list_empty(&space_info->tickets) ||
			!list_empty(&space_info->priority_tickets);
	else
		pending_tickets = !list_empty(&space_info->priority_tickets);

	 
	if (!pending_tickets &&
	    ((used + orig_bytes <= space_info->total_bytes) ||
	     btrfs_can_overcommit(fs_info, space_info, orig_bytes, flush))) {
		btrfs_space_info_update_bytes_may_use(fs_info, space_info,
						      orig_bytes);
		ret = 0;
	}

	 
	if (ret && unlikely(flush == BTRFS_RESERVE_FLUSH_EMERGENCY)) {
		used = btrfs_space_info_used(space_info, false);
		if (used + orig_bytes <= space_info->total_bytes) {
			btrfs_space_info_update_bytes_may_use(fs_info, space_info,
							      orig_bytes);
			ret = 0;
		}
	}

	 
	if (ret && can_ticket(flush)) {
		ticket.bytes = orig_bytes;
		ticket.error = 0;
		space_info->reclaim_size += ticket.bytes;
		init_waitqueue_head(&ticket.wait);
		ticket.steal = can_steal(flush);
		if (trace_btrfs_reserve_ticket_enabled())
			start_ns = ktime_get_ns();

		if (flush == BTRFS_RESERVE_FLUSH_ALL ||
		    flush == BTRFS_RESERVE_FLUSH_ALL_STEAL ||
		    flush == BTRFS_RESERVE_FLUSH_DATA) {
			list_add_tail(&ticket.list, &space_info->tickets);
			if (!space_info->flush) {
				 
				maybe_clamp_preempt(fs_info, space_info);

				space_info->flush = 1;
				trace_btrfs_trigger_flush(fs_info,
							  space_info->flags,
							  orig_bytes, flush,
							  "enospc");
				queue_work(system_unbound_wq, async_work);
			}
		} else {
			list_add_tail(&ticket.list,
				      &space_info->priority_tickets);
		}
	} else if (!ret && space_info->flags & BTRFS_BLOCK_GROUP_METADATA) {
		 
		if (!test_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags) &&
		    !work_busy(&fs_info->preempt_reclaim_work) &&
		    need_preemptive_reclaim(fs_info, space_info)) {
			trace_btrfs_trigger_flush(fs_info, space_info->flags,
						  orig_bytes, flush, "preempt");
			queue_work(system_unbound_wq,
				   &fs_info->preempt_reclaim_work);
		}
	}
	spin_unlock(&space_info->lock);
	if (!ret || !can_ticket(flush))
		return ret;

	return handle_reserve_ticket(fs_info, space_info, &ticket, start_ns,
				     orig_bytes, flush);
}

 
int btrfs_reserve_metadata_bytes(struct btrfs_fs_info *fs_info,
				 struct btrfs_block_rsv *block_rsv,
				 u64 orig_bytes,
				 enum btrfs_reserve_flush_enum flush)
{
	int ret;

	ret = __reserve_bytes(fs_info, block_rsv->space_info, orig_bytes, flush);
	if (ret == -ENOSPC) {
		trace_btrfs_space_reservation(fs_info, "space_info:enospc",
					      block_rsv->space_info->flags,
					      orig_bytes, 1);

		if (btrfs_test_opt(fs_info, ENOSPC_DEBUG))
			btrfs_dump_space_info(fs_info, block_rsv->space_info,
					      orig_bytes, 0);
	}
	return ret;
}

 
int btrfs_reserve_data_bytes(struct btrfs_fs_info *fs_info, u64 bytes,
			     enum btrfs_reserve_flush_enum flush)
{
	struct btrfs_space_info *data_sinfo = fs_info->data_sinfo;
	int ret;

	ASSERT(flush == BTRFS_RESERVE_FLUSH_DATA ||
	       flush == BTRFS_RESERVE_FLUSH_FREE_SPACE_INODE ||
	       flush == BTRFS_RESERVE_NO_FLUSH);
	ASSERT(!current->journal_info || flush != BTRFS_RESERVE_FLUSH_DATA);

	ret = __reserve_bytes(fs_info, data_sinfo, bytes, flush);
	if (ret == -ENOSPC) {
		trace_btrfs_space_reservation(fs_info, "space_info:enospc",
					      data_sinfo->flags, bytes, 1);
		if (btrfs_test_opt(fs_info, ENOSPC_DEBUG))
			btrfs_dump_space_info(fs_info, data_sinfo, bytes, 0);
	}
	return ret;
}

 
__cold void btrfs_dump_space_info_for_trans_abort(struct btrfs_fs_info *fs_info)
{
	struct btrfs_space_info *space_info;

	btrfs_info(fs_info, "dumping space info:");
	list_for_each_entry(space_info, &fs_info->space_info, list) {
		spin_lock(&space_info->lock);
		__btrfs_dump_space_info(fs_info, space_info);
		spin_unlock(&space_info->lock);
	}
	dump_global_block_rsv(fs_info);
}

 
u64 btrfs_account_ro_block_groups_free_space(struct btrfs_space_info *sinfo)
{
	struct btrfs_block_group *block_group;
	u64 free_bytes = 0;
	int factor;

	 
	if (list_empty(&sinfo->ro_bgs))
		return 0;

	spin_lock(&sinfo->lock);
	list_for_each_entry(block_group, &sinfo->ro_bgs, ro_list) {
		spin_lock(&block_group->lock);

		if (!block_group->ro) {
			spin_unlock(&block_group->lock);
			continue;
		}

		factor = btrfs_bg_type_to_factor(block_group->flags);
		free_bytes += (block_group->length -
			       block_group->used) * factor;

		spin_unlock(&block_group->lock);
	}
	spin_unlock(&sinfo->lock);

	return free_bytes;
}

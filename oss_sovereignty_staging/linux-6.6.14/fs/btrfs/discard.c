

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/math64.h>
#include <linux/sizes.h>
#include <linux/workqueue.h>
#include "ctree.h"
#include "block-group.h"
#include "discard.h"
#include "free-space-cache.h"
#include "fs.h"

 

 
#define BTRFS_DISCARD_DELAY		(120ULL * NSEC_PER_SEC)
#define BTRFS_DISCARD_UNUSED_DELAY	(10ULL * NSEC_PER_SEC)

#define BTRFS_DISCARD_MIN_DELAY_MSEC	(1UL)
#define BTRFS_DISCARD_MAX_DELAY_MSEC	(1000UL)
#define BTRFS_DISCARD_MAX_IOPS		(1000U)

 
static int discard_minlen[BTRFS_NR_DISCARD_LISTS] = {
	0,
	BTRFS_ASYNC_DISCARD_MAX_FILTER,
	BTRFS_ASYNC_DISCARD_MIN_FILTER
};

static struct list_head *get_discard_list(struct btrfs_discard_ctl *discard_ctl,
					  struct btrfs_block_group *block_group)
{
	return &discard_ctl->discard_list[block_group->discard_index];
}

 
static bool btrfs_run_discard_work(struct btrfs_discard_ctl *discard_ctl)
{
	struct btrfs_fs_info *fs_info = container_of(discard_ctl,
						     struct btrfs_fs_info,
						     discard_ctl);

	return (!(fs_info->sb->s_flags & SB_RDONLY) &&
		test_bit(BTRFS_FS_DISCARD_RUNNING, &fs_info->flags));
}

static void __add_to_discard_list(struct btrfs_discard_ctl *discard_ctl,
				  struct btrfs_block_group *block_group)
{
	lockdep_assert_held(&discard_ctl->lock);
	if (!btrfs_run_discard_work(discard_ctl))
		return;

	if (list_empty(&block_group->discard_list) ||
	    block_group->discard_index == BTRFS_DISCARD_INDEX_UNUSED) {
		if (block_group->discard_index == BTRFS_DISCARD_INDEX_UNUSED)
			block_group->discard_index = BTRFS_DISCARD_INDEX_START;
		block_group->discard_eligible_time = (ktime_get_ns() +
						      BTRFS_DISCARD_DELAY);
		block_group->discard_state = BTRFS_DISCARD_RESET_CURSOR;
	}
	if (list_empty(&block_group->discard_list))
		btrfs_get_block_group(block_group);

	list_move_tail(&block_group->discard_list,
		       get_discard_list(discard_ctl, block_group));
}

static void add_to_discard_list(struct btrfs_discard_ctl *discard_ctl,
				struct btrfs_block_group *block_group)
{
	if (!btrfs_is_block_group_data_only(block_group))
		return;

	spin_lock(&discard_ctl->lock);
	__add_to_discard_list(discard_ctl, block_group);
	spin_unlock(&discard_ctl->lock);
}

static void add_to_discard_unused_list(struct btrfs_discard_ctl *discard_ctl,
				       struct btrfs_block_group *block_group)
{
	bool queued;

	spin_lock(&discard_ctl->lock);

	queued = !list_empty(&block_group->discard_list);

	if (!btrfs_run_discard_work(discard_ctl)) {
		spin_unlock(&discard_ctl->lock);
		return;
	}

	list_del_init(&block_group->discard_list);

	block_group->discard_index = BTRFS_DISCARD_INDEX_UNUSED;
	block_group->discard_eligible_time = (ktime_get_ns() +
					      BTRFS_DISCARD_UNUSED_DELAY);
	block_group->discard_state = BTRFS_DISCARD_RESET_CURSOR;
	if (!queued)
		btrfs_get_block_group(block_group);
	list_add_tail(&block_group->discard_list,
		      &discard_ctl->discard_list[BTRFS_DISCARD_INDEX_UNUSED]);

	spin_unlock(&discard_ctl->lock);
}

static bool remove_from_discard_list(struct btrfs_discard_ctl *discard_ctl,
				     struct btrfs_block_group *block_group)
{
	bool running = false;
	bool queued = false;

	spin_lock(&discard_ctl->lock);

	if (block_group == discard_ctl->block_group) {
		running = true;
		discard_ctl->block_group = NULL;
	}

	block_group->discard_eligible_time = 0;
	queued = !list_empty(&block_group->discard_list);
	list_del_init(&block_group->discard_list);
	 
	if (queued && !running)
		btrfs_put_block_group(block_group);

	spin_unlock(&discard_ctl->lock);

	return running;
}

 
static struct btrfs_block_group *find_next_block_group(
					struct btrfs_discard_ctl *discard_ctl,
					u64 now)
{
	struct btrfs_block_group *ret_block_group = NULL, *block_group;
	int i;

	for (i = 0; i < BTRFS_NR_DISCARD_LISTS; i++) {
		struct list_head *discard_list = &discard_ctl->discard_list[i];

		if (!list_empty(discard_list)) {
			block_group = list_first_entry(discard_list,
						       struct btrfs_block_group,
						       discard_list);

			if (!ret_block_group)
				ret_block_group = block_group;

			if (ret_block_group->discard_eligible_time < now)
				break;

			if (ret_block_group->discard_eligible_time >
			    block_group->discard_eligible_time)
				ret_block_group = block_group;
		}
	}

	return ret_block_group;
}

 
static struct btrfs_block_group *peek_discard_list(
					struct btrfs_discard_ctl *discard_ctl,
					enum btrfs_discard_state *discard_state,
					int *discard_index, u64 now)
{
	struct btrfs_block_group *block_group;

	spin_lock(&discard_ctl->lock);
again:
	block_group = find_next_block_group(discard_ctl, now);

	if (block_group && now >= block_group->discard_eligible_time) {
		if (block_group->discard_index == BTRFS_DISCARD_INDEX_UNUSED &&
		    block_group->used != 0) {
			if (btrfs_is_block_group_data_only(block_group)) {
				__add_to_discard_list(discard_ctl, block_group);
			} else {
				list_del_init(&block_group->discard_list);
				btrfs_put_block_group(block_group);
			}
			goto again;
		}
		if (block_group->discard_state == BTRFS_DISCARD_RESET_CURSOR) {
			block_group->discard_cursor = block_group->start;
			block_group->discard_state = BTRFS_DISCARD_EXTENTS;
		}
		discard_ctl->block_group = block_group;
	}
	if (block_group) {
		*discard_state = block_group->discard_state;
		*discard_index = block_group->discard_index;
	}
	spin_unlock(&discard_ctl->lock);

	return block_group;
}

 
void btrfs_discard_check_filter(struct btrfs_block_group *block_group,
				u64 bytes)
{
	struct btrfs_discard_ctl *discard_ctl;

	if (!block_group ||
	    !btrfs_test_opt(block_group->fs_info, DISCARD_ASYNC))
		return;

	discard_ctl = &block_group->fs_info->discard_ctl;

	if (block_group->discard_index > BTRFS_DISCARD_INDEX_START &&
	    bytes >= discard_minlen[block_group->discard_index - 1]) {
		int i;

		remove_from_discard_list(discard_ctl, block_group);

		for (i = BTRFS_DISCARD_INDEX_START; i < BTRFS_NR_DISCARD_LISTS;
		     i++) {
			if (bytes >= discard_minlen[i]) {
				block_group->discard_index = i;
				add_to_discard_list(discard_ctl, block_group);
				break;
			}
		}
	}
}

 
static void btrfs_update_discard_index(struct btrfs_discard_ctl *discard_ctl,
				       struct btrfs_block_group *block_group)
{
	block_group->discard_index++;
	if (block_group->discard_index == BTRFS_NR_DISCARD_LISTS) {
		block_group->discard_index = 1;
		return;
	}

	add_to_discard_list(discard_ctl, block_group);
}

 
void btrfs_discard_cancel_work(struct btrfs_discard_ctl *discard_ctl,
			       struct btrfs_block_group *block_group)
{
	if (remove_from_discard_list(discard_ctl, block_group)) {
		cancel_delayed_work_sync(&discard_ctl->work);
		btrfs_discard_schedule_work(discard_ctl, true);
	}
}

 
void btrfs_discard_queue_work(struct btrfs_discard_ctl *discard_ctl,
			      struct btrfs_block_group *block_group)
{
	if (!block_group || !btrfs_test_opt(block_group->fs_info, DISCARD_ASYNC))
		return;

	if (block_group->used == 0)
		add_to_discard_unused_list(discard_ctl, block_group);
	else
		add_to_discard_list(discard_ctl, block_group);

	if (!delayed_work_pending(&discard_ctl->work))
		btrfs_discard_schedule_work(discard_ctl, false);
}

static void __btrfs_discard_schedule_work(struct btrfs_discard_ctl *discard_ctl,
					  u64 now, bool override)
{
	struct btrfs_block_group *block_group;

	if (!btrfs_run_discard_work(discard_ctl))
		return;
	if (!override && delayed_work_pending(&discard_ctl->work))
		return;

	block_group = find_next_block_group(discard_ctl, now);
	if (block_group) {
		u64 delay = discard_ctl->delay_ms * NSEC_PER_MSEC;
		u32 kbps_limit = READ_ONCE(discard_ctl->kbps_limit);

		 
		if (kbps_limit && discard_ctl->prev_discard) {
			u64 bps_limit = ((u64)kbps_limit) * SZ_1K;
			u64 bps_delay = div64_u64(discard_ctl->prev_discard *
						  NSEC_PER_SEC, bps_limit);

			delay = max(delay, bps_delay);
		}

		 
		if (now < block_group->discard_eligible_time) {
			u64 bg_timeout = block_group->discard_eligible_time - now;

			delay = max(delay, bg_timeout);
		}

		if (override && discard_ctl->prev_discard) {
			u64 elapsed = now - discard_ctl->prev_discard_time;

			if (delay > elapsed)
				delay -= elapsed;
			else
				delay = 0;
		}

		mod_delayed_work(discard_ctl->discard_workers,
				 &discard_ctl->work, nsecs_to_jiffies(delay));
	}
}

 
void btrfs_discard_schedule_work(struct btrfs_discard_ctl *discard_ctl,
				 bool override)
{
	const u64 now = ktime_get_ns();

	spin_lock(&discard_ctl->lock);
	__btrfs_discard_schedule_work(discard_ctl, now, override);
	spin_unlock(&discard_ctl->lock);
}

 
static void btrfs_finish_discard_pass(struct btrfs_discard_ctl *discard_ctl,
				      struct btrfs_block_group *block_group)
{
	remove_from_discard_list(discard_ctl, block_group);

	if (block_group->used == 0) {
		if (btrfs_is_free_space_trimmed(block_group))
			btrfs_mark_bg_unused(block_group);
		else
			add_to_discard_unused_list(discard_ctl, block_group);
	} else {
		btrfs_update_discard_index(discard_ctl, block_group);
	}
}

 
static void btrfs_discard_workfn(struct work_struct *work)
{
	struct btrfs_discard_ctl *discard_ctl;
	struct btrfs_block_group *block_group;
	enum btrfs_discard_state discard_state;
	int discard_index = 0;
	u64 trimmed = 0;
	u64 minlen = 0;
	u64 now = ktime_get_ns();

	discard_ctl = container_of(work, struct btrfs_discard_ctl, work.work);

	block_group = peek_discard_list(discard_ctl, &discard_state,
					&discard_index, now);
	if (!block_group || !btrfs_run_discard_work(discard_ctl))
		return;
	if (now < block_group->discard_eligible_time) {
		btrfs_discard_schedule_work(discard_ctl, false);
		return;
	}

	 
	minlen = discard_minlen[discard_index];

	if (discard_state == BTRFS_DISCARD_BITMAPS) {
		u64 maxlen = 0;

		 
		if (discard_index != BTRFS_DISCARD_INDEX_UNUSED)
			maxlen = discard_minlen[discard_index - 1];

		btrfs_trim_block_group_bitmaps(block_group, &trimmed,
				       block_group->discard_cursor,
				       btrfs_block_group_end(block_group),
				       minlen, maxlen, true);
		discard_ctl->discard_bitmap_bytes += trimmed;
	} else {
		btrfs_trim_block_group_extents(block_group, &trimmed,
				       block_group->discard_cursor,
				       btrfs_block_group_end(block_group),
				       minlen, true);
		discard_ctl->discard_extent_bytes += trimmed;
	}

	 
	if (block_group->discard_cursor >= btrfs_block_group_end(block_group)) {
		if (discard_state == BTRFS_DISCARD_BITMAPS) {
			btrfs_finish_discard_pass(discard_ctl, block_group);
		} else {
			block_group->discard_cursor = block_group->start;
			spin_lock(&discard_ctl->lock);
			if (block_group->discard_state !=
			    BTRFS_DISCARD_RESET_CURSOR)
				block_group->discard_state =
							BTRFS_DISCARD_BITMAPS;
			spin_unlock(&discard_ctl->lock);
		}
	}

	now = ktime_get_ns();
	spin_lock(&discard_ctl->lock);
	discard_ctl->prev_discard = trimmed;
	discard_ctl->prev_discard_time = now;
	 
	if (discard_ctl->block_group == NULL)
		btrfs_put_block_group(block_group);
	discard_ctl->block_group = NULL;
	__btrfs_discard_schedule_work(discard_ctl, now, false);
	spin_unlock(&discard_ctl->lock);
}

 
void btrfs_discard_calc_delay(struct btrfs_discard_ctl *discard_ctl)
{
	s32 discardable_extents;
	s64 discardable_bytes;
	u32 iops_limit;
	unsigned long min_delay = BTRFS_DISCARD_MIN_DELAY_MSEC;
	unsigned long delay;

	discardable_extents = atomic_read(&discard_ctl->discardable_extents);
	if (!discardable_extents)
		return;

	spin_lock(&discard_ctl->lock);

	 
	if (discardable_extents < 0)
		atomic_add(-discardable_extents,
			   &discard_ctl->discardable_extents);

	discardable_bytes = atomic64_read(&discard_ctl->discardable_bytes);
	if (discardable_bytes < 0)
		atomic64_add(-discardable_bytes,
			     &discard_ctl->discardable_bytes);

	if (discardable_extents <= 0) {
		spin_unlock(&discard_ctl->lock);
		return;
	}

	iops_limit = READ_ONCE(discard_ctl->iops_limit);

	if (iops_limit) {
		delay = MSEC_PER_SEC / iops_limit;
	} else {
		 
		delay = 0;
		min_delay = 0;
	}

	delay = clamp(delay, min_delay, BTRFS_DISCARD_MAX_DELAY_MSEC);
	discard_ctl->delay_ms = delay;

	spin_unlock(&discard_ctl->lock);
}

 
void btrfs_discard_update_discardable(struct btrfs_block_group *block_group)
{
	struct btrfs_free_space_ctl *ctl;
	struct btrfs_discard_ctl *discard_ctl;
	s32 extents_delta;
	s64 bytes_delta;

	if (!block_group ||
	    !btrfs_test_opt(block_group->fs_info, DISCARD_ASYNC) ||
	    !btrfs_is_block_group_data_only(block_group))
		return;

	ctl = block_group->free_space_ctl;
	discard_ctl = &block_group->fs_info->discard_ctl;

	lockdep_assert_held(&ctl->tree_lock);
	extents_delta = ctl->discardable_extents[BTRFS_STAT_CURR] -
			ctl->discardable_extents[BTRFS_STAT_PREV];
	if (extents_delta) {
		atomic_add(extents_delta, &discard_ctl->discardable_extents);
		ctl->discardable_extents[BTRFS_STAT_PREV] =
			ctl->discardable_extents[BTRFS_STAT_CURR];
	}

	bytes_delta = ctl->discardable_bytes[BTRFS_STAT_CURR] -
		      ctl->discardable_bytes[BTRFS_STAT_PREV];
	if (bytes_delta) {
		atomic64_add(bytes_delta, &discard_ctl->discardable_bytes);
		ctl->discardable_bytes[BTRFS_STAT_PREV] =
			ctl->discardable_bytes[BTRFS_STAT_CURR];
	}
}

 
void btrfs_discard_punt_unused_bgs_list(struct btrfs_fs_info *fs_info)
{
	struct btrfs_block_group *block_group, *next;

	spin_lock(&fs_info->unused_bgs_lock);
	 
	list_for_each_entry_safe(block_group, next, &fs_info->unused_bgs,
				 bg_list) {
		list_del_init(&block_group->bg_list);
		btrfs_discard_queue_work(&fs_info->discard_ctl, block_group);
		 
		btrfs_put_block_group(block_group);
	}
	spin_unlock(&fs_info->unused_bgs_lock);
}

 
static void btrfs_discard_purge_list(struct btrfs_discard_ctl *discard_ctl)
{
	struct btrfs_block_group *block_group, *next;
	int i;

	spin_lock(&discard_ctl->lock);
	for (i = 0; i < BTRFS_NR_DISCARD_LISTS; i++) {
		list_for_each_entry_safe(block_group, next,
					 &discard_ctl->discard_list[i],
					 discard_list) {
			list_del_init(&block_group->discard_list);
			spin_unlock(&discard_ctl->lock);
			if (block_group->used == 0)
				btrfs_mark_bg_unused(block_group);
			spin_lock(&discard_ctl->lock);
			btrfs_put_block_group(block_group);
		}
	}
	spin_unlock(&discard_ctl->lock);
}

void btrfs_discard_resume(struct btrfs_fs_info *fs_info)
{
	if (!btrfs_test_opt(fs_info, DISCARD_ASYNC)) {
		btrfs_discard_cleanup(fs_info);
		return;
	}

	btrfs_discard_punt_unused_bgs_list(fs_info);

	set_bit(BTRFS_FS_DISCARD_RUNNING, &fs_info->flags);
}

void btrfs_discard_stop(struct btrfs_fs_info *fs_info)
{
	clear_bit(BTRFS_FS_DISCARD_RUNNING, &fs_info->flags);
}

void btrfs_discard_init(struct btrfs_fs_info *fs_info)
{
	struct btrfs_discard_ctl *discard_ctl = &fs_info->discard_ctl;
	int i;

	spin_lock_init(&discard_ctl->lock);
	INIT_DELAYED_WORK(&discard_ctl->work, btrfs_discard_workfn);

	for (i = 0; i < BTRFS_NR_DISCARD_LISTS; i++)
		INIT_LIST_HEAD(&discard_ctl->discard_list[i]);

	discard_ctl->prev_discard = 0;
	discard_ctl->prev_discard_time = 0;
	atomic_set(&discard_ctl->discardable_extents, 0);
	atomic64_set(&discard_ctl->discardable_bytes, 0);
	discard_ctl->max_discard_size = BTRFS_ASYNC_DISCARD_DEFAULT_MAX_SIZE;
	discard_ctl->delay_ms = BTRFS_DISCARD_MAX_DELAY_MSEC;
	discard_ctl->iops_limit = BTRFS_DISCARD_MAX_IOPS;
	discard_ctl->kbps_limit = 0;
	discard_ctl->discard_extent_bytes = 0;
	discard_ctl->discard_bitmap_bytes = 0;
	atomic64_set(&discard_ctl->discard_bytes_saved, 0);
}

void btrfs_discard_cleanup(struct btrfs_fs_info *fs_info)
{
	btrfs_discard_stop(fs_info);
	cancel_delayed_work_sync(&fs_info->discard_ctl.work);
	btrfs_discard_purge_list(&fs_info->discard_ctl);
}

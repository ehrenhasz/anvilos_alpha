

#include <linux/sizes.h>
#include <linux/list_sort.h>
#include "misc.h"
#include "ctree.h"
#include "block-group.h"
#include "space-info.h"
#include "disk-io.h"
#include "free-space-cache.h"
#include "free-space-tree.h"
#include "volumes.h"
#include "transaction.h"
#include "ref-verify.h"
#include "sysfs.h"
#include "tree-log.h"
#include "delalloc-space.h"
#include "discard.h"
#include "raid56.h"
#include "zoned.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"

#ifdef CONFIG_BTRFS_DEBUG
int btrfs_should_fragment_free_space(struct btrfs_block_group *block_group)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;

	return (btrfs_test_opt(fs_info, FRAGMENT_METADATA) &&
		block_group->flags & BTRFS_BLOCK_GROUP_METADATA) ||
	       (btrfs_test_opt(fs_info, FRAGMENT_DATA) &&
		block_group->flags &  BTRFS_BLOCK_GROUP_DATA);
}
#endif

 
static u64 get_restripe_target(struct btrfs_fs_info *fs_info, u64 flags)
{
	struct btrfs_balance_control *bctl = fs_info->balance_ctl;
	u64 target = 0;

	if (!bctl)
		return 0;

	if (flags & BTRFS_BLOCK_GROUP_DATA &&
	    bctl->data.flags & BTRFS_BALANCE_ARGS_CONVERT) {
		target = BTRFS_BLOCK_GROUP_DATA | bctl->data.target;
	} else if (flags & BTRFS_BLOCK_GROUP_SYSTEM &&
		   bctl->sys.flags & BTRFS_BALANCE_ARGS_CONVERT) {
		target = BTRFS_BLOCK_GROUP_SYSTEM | bctl->sys.target;
	} else if (flags & BTRFS_BLOCK_GROUP_METADATA &&
		   bctl->meta.flags & BTRFS_BALANCE_ARGS_CONVERT) {
		target = BTRFS_BLOCK_GROUP_METADATA | bctl->meta.target;
	}

	return target;
}

 
static u64 btrfs_reduce_alloc_profile(struct btrfs_fs_info *fs_info, u64 flags)
{
	u64 num_devices = fs_info->fs_devices->rw_devices;
	u64 target;
	u64 raid_type;
	u64 allowed = 0;

	 
	spin_lock(&fs_info->balance_lock);
	target = get_restripe_target(fs_info, flags);
	if (target) {
		spin_unlock(&fs_info->balance_lock);
		return extended_to_chunk(target);
	}
	spin_unlock(&fs_info->balance_lock);

	 
	for (raid_type = 0; raid_type < BTRFS_NR_RAID_TYPES; raid_type++) {
		if (num_devices >= btrfs_raid_array[raid_type].devs_min)
			allowed |= btrfs_raid_array[raid_type].bg_flag;
	}
	allowed &= flags;

	 
	if (allowed & BTRFS_BLOCK_GROUP_RAID1C4)
		allowed = BTRFS_BLOCK_GROUP_RAID1C4;
	else if (allowed & BTRFS_BLOCK_GROUP_RAID6)
		allowed = BTRFS_BLOCK_GROUP_RAID6;
	else if (allowed & BTRFS_BLOCK_GROUP_RAID1C3)
		allowed = BTRFS_BLOCK_GROUP_RAID1C3;
	else if (allowed & BTRFS_BLOCK_GROUP_RAID5)
		allowed = BTRFS_BLOCK_GROUP_RAID5;
	else if (allowed & BTRFS_BLOCK_GROUP_RAID10)
		allowed = BTRFS_BLOCK_GROUP_RAID10;
	else if (allowed & BTRFS_BLOCK_GROUP_RAID1)
		allowed = BTRFS_BLOCK_GROUP_RAID1;
	else if (allowed & BTRFS_BLOCK_GROUP_DUP)
		allowed = BTRFS_BLOCK_GROUP_DUP;
	else if (allowed & BTRFS_BLOCK_GROUP_RAID0)
		allowed = BTRFS_BLOCK_GROUP_RAID0;

	flags &= ~BTRFS_BLOCK_GROUP_PROFILE_MASK;

	return extended_to_chunk(flags | allowed);
}

u64 btrfs_get_alloc_profile(struct btrfs_fs_info *fs_info, u64 orig_flags)
{
	unsigned seq;
	u64 flags;

	do {
		flags = orig_flags;
		seq = read_seqbegin(&fs_info->profiles_lock);

		if (flags & BTRFS_BLOCK_GROUP_DATA)
			flags |= fs_info->avail_data_alloc_bits;
		else if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
			flags |= fs_info->avail_system_alloc_bits;
		else if (flags & BTRFS_BLOCK_GROUP_METADATA)
			flags |= fs_info->avail_metadata_alloc_bits;
	} while (read_seqretry(&fs_info->profiles_lock, seq));

	return btrfs_reduce_alloc_profile(fs_info, flags);
}

void btrfs_get_block_group(struct btrfs_block_group *cache)
{
	refcount_inc(&cache->refs);
}

void btrfs_put_block_group(struct btrfs_block_group *cache)
{
	if (refcount_dec_and_test(&cache->refs)) {
		WARN_ON(cache->pinned > 0);
		 
		if (!(cache->flags & BTRFS_BLOCK_GROUP_METADATA) ||
		    !BTRFS_FS_LOG_CLEANUP_ERROR(cache->fs_info))
			WARN_ON(cache->reserved > 0);

		 
		if (WARN_ON(!list_empty(&cache->discard_list)))
			btrfs_discard_cancel_work(&cache->fs_info->discard_ctl,
						  cache);

		kfree(cache->free_space_ctl);
		kfree(cache->physical_map);
		kfree(cache);
	}
}

 
static int btrfs_add_block_group_cache(struct btrfs_fs_info *info,
				       struct btrfs_block_group *block_group)
{
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct btrfs_block_group *cache;
	bool leftmost = true;

	ASSERT(block_group->length != 0);

	write_lock(&info->block_group_cache_lock);
	p = &info->block_group_cache_tree.rb_root.rb_node;

	while (*p) {
		parent = *p;
		cache = rb_entry(parent, struct btrfs_block_group, cache_node);
		if (block_group->start < cache->start) {
			p = &(*p)->rb_left;
		} else if (block_group->start > cache->start) {
			p = &(*p)->rb_right;
			leftmost = false;
		} else {
			write_unlock(&info->block_group_cache_lock);
			return -EEXIST;
		}
	}

	rb_link_node(&block_group->cache_node, parent, p);
	rb_insert_color_cached(&block_group->cache_node,
			       &info->block_group_cache_tree, leftmost);

	write_unlock(&info->block_group_cache_lock);

	return 0;
}

 
static struct btrfs_block_group *block_group_cache_tree_search(
		struct btrfs_fs_info *info, u64 bytenr, int contains)
{
	struct btrfs_block_group *cache, *ret = NULL;
	struct rb_node *n;
	u64 end, start;

	read_lock(&info->block_group_cache_lock);
	n = info->block_group_cache_tree.rb_root.rb_node;

	while (n) {
		cache = rb_entry(n, struct btrfs_block_group, cache_node);
		end = cache->start + cache->length - 1;
		start = cache->start;

		if (bytenr < start) {
			if (!contains && (!ret || start < ret->start))
				ret = cache;
			n = n->rb_left;
		} else if (bytenr > start) {
			if (contains && bytenr <= end) {
				ret = cache;
				break;
			}
			n = n->rb_right;
		} else {
			ret = cache;
			break;
		}
	}
	if (ret)
		btrfs_get_block_group(ret);
	read_unlock(&info->block_group_cache_lock);

	return ret;
}

 
struct btrfs_block_group *btrfs_lookup_first_block_group(
		struct btrfs_fs_info *info, u64 bytenr)
{
	return block_group_cache_tree_search(info, bytenr, 0);
}

 
struct btrfs_block_group *btrfs_lookup_block_group(
		struct btrfs_fs_info *info, u64 bytenr)
{
	return block_group_cache_tree_search(info, bytenr, 1);
}

struct btrfs_block_group *btrfs_next_block_group(
		struct btrfs_block_group *cache)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct rb_node *node;

	read_lock(&fs_info->block_group_cache_lock);

	 
	if (RB_EMPTY_NODE(&cache->cache_node)) {
		const u64 next_bytenr = cache->start + cache->length;

		read_unlock(&fs_info->block_group_cache_lock);
		btrfs_put_block_group(cache);
		return btrfs_lookup_first_block_group(fs_info, next_bytenr);
	}
	node = rb_next(&cache->cache_node);
	btrfs_put_block_group(cache);
	if (node) {
		cache = rb_entry(node, struct btrfs_block_group, cache_node);
		btrfs_get_block_group(cache);
	} else
		cache = NULL;
	read_unlock(&fs_info->block_group_cache_lock);
	return cache;
}

 
struct btrfs_block_group *btrfs_inc_nocow_writers(struct btrfs_fs_info *fs_info,
						  u64 bytenr)
{
	struct btrfs_block_group *bg;
	bool can_nocow = true;

	bg = btrfs_lookup_block_group(fs_info, bytenr);
	if (!bg)
		return NULL;

	spin_lock(&bg->lock);
	if (bg->ro)
		can_nocow = false;
	else
		atomic_inc(&bg->nocow_writers);
	spin_unlock(&bg->lock);

	if (!can_nocow) {
		btrfs_put_block_group(bg);
		return NULL;
	}

	 
	return bg;
}

 
void btrfs_dec_nocow_writers(struct btrfs_block_group *bg)
{
	if (atomic_dec_and_test(&bg->nocow_writers))
		wake_up_var(&bg->nocow_writers);

	 
	btrfs_put_block_group(bg);
}

void btrfs_wait_nocow_writers(struct btrfs_block_group *bg)
{
	wait_var_event(&bg->nocow_writers, !atomic_read(&bg->nocow_writers));
}

void btrfs_dec_block_group_reservations(struct btrfs_fs_info *fs_info,
					const u64 start)
{
	struct btrfs_block_group *bg;

	bg = btrfs_lookup_block_group(fs_info, start);
	ASSERT(bg);
	if (atomic_dec_and_test(&bg->reservations))
		wake_up_var(&bg->reservations);
	btrfs_put_block_group(bg);
}

void btrfs_wait_block_group_reservations(struct btrfs_block_group *bg)
{
	struct btrfs_space_info *space_info = bg->space_info;

	ASSERT(bg->ro);

	if (!(bg->flags & BTRFS_BLOCK_GROUP_DATA))
		return;

	 
	down_write(&space_info->groups_sem);
	up_write(&space_info->groups_sem);

	wait_var_event(&bg->reservations, !atomic_read(&bg->reservations));
}

struct btrfs_caching_control *btrfs_get_caching_control(
		struct btrfs_block_group *cache)
{
	struct btrfs_caching_control *ctl;

	spin_lock(&cache->lock);
	if (!cache->caching_ctl) {
		spin_unlock(&cache->lock);
		return NULL;
	}

	ctl = cache->caching_ctl;
	refcount_inc(&ctl->count);
	spin_unlock(&cache->lock);
	return ctl;
}

void btrfs_put_caching_control(struct btrfs_caching_control *ctl)
{
	if (refcount_dec_and_test(&ctl->count))
		kfree(ctl);
}

 
void btrfs_wait_block_group_cache_progress(struct btrfs_block_group *cache,
					   u64 num_bytes)
{
	struct btrfs_caching_control *caching_ctl;
	int progress;

	caching_ctl = btrfs_get_caching_control(cache);
	if (!caching_ctl)
		return;

	 
	progress = atomic_read(&caching_ctl->progress);

	wait_event(caching_ctl->wait, btrfs_block_group_done(cache) ||
		   (progress != atomic_read(&caching_ctl->progress) &&
		    (cache->free_space_ctl->free_space >= num_bytes)));

	btrfs_put_caching_control(caching_ctl);
}

static int btrfs_caching_ctl_wait_done(struct btrfs_block_group *cache,
				       struct btrfs_caching_control *caching_ctl)
{
	wait_event(caching_ctl->wait, btrfs_block_group_done(cache));
	return cache->cached == BTRFS_CACHE_ERROR ? -EIO : 0;
}

static int btrfs_wait_block_group_cache_done(struct btrfs_block_group *cache)
{
	struct btrfs_caching_control *caching_ctl;
	int ret;

	caching_ctl = btrfs_get_caching_control(cache);
	if (!caching_ctl)
		return (cache->cached == BTRFS_CACHE_ERROR) ? -EIO : 0;
	ret = btrfs_caching_ctl_wait_done(cache, caching_ctl);
	btrfs_put_caching_control(caching_ctl);
	return ret;
}

#ifdef CONFIG_BTRFS_DEBUG
static void fragment_free_space(struct btrfs_block_group *block_group)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	u64 start = block_group->start;
	u64 len = block_group->length;
	u64 chunk = block_group->flags & BTRFS_BLOCK_GROUP_METADATA ?
		fs_info->nodesize : fs_info->sectorsize;
	u64 step = chunk << 1;

	while (len > chunk) {
		btrfs_remove_free_space(block_group, start, chunk);
		start += step;
		if (len < step)
			len = 0;
		else
			len -= step;
	}
}
#endif

 
int btrfs_add_new_free_space(struct btrfs_block_group *block_group, u64 start,
			     u64 end, u64 *total_added_ret)
{
	struct btrfs_fs_info *info = block_group->fs_info;
	u64 extent_start, extent_end, size;
	int ret;

	if (total_added_ret)
		*total_added_ret = 0;

	while (start < end) {
		if (!find_first_extent_bit(&info->excluded_extents, start,
					   &extent_start, &extent_end,
					   EXTENT_DIRTY | EXTENT_UPTODATE,
					   NULL))
			break;

		if (extent_start <= start) {
			start = extent_end + 1;
		} else if (extent_start > start && extent_start < end) {
			size = extent_start - start;
			ret = btrfs_add_free_space_async_trimmed(block_group,
								 start, size);
			if (ret)
				return ret;
			if (total_added_ret)
				*total_added_ret += size;
			start = extent_end + 1;
		} else {
			break;
		}
	}

	if (start < end) {
		size = end - start;
		ret = btrfs_add_free_space_async_trimmed(block_group, start,
							 size);
		if (ret)
			return ret;
		if (total_added_ret)
			*total_added_ret += size;
	}

	return 0;
}

 
static int sample_block_group_extent_item(struct btrfs_caching_control *caching_ctl,
					  struct btrfs_block_group *block_group,
					  int index, int max_index,
					  struct btrfs_key *found_key)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct btrfs_root *extent_root;
	u64 search_offset;
	u64 search_end = block_group->start + block_group->length;
	struct btrfs_path *path;
	struct btrfs_key search_key;
	int ret = 0;

	ASSERT(index >= 0);
	ASSERT(index <= max_index);
	ASSERT(max_index > 0);
	lockdep_assert_held(&caching_ctl->mutex);
	lockdep_assert_held_read(&fs_info->commit_root_sem);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	extent_root = btrfs_extent_root(fs_info, max_t(u64, block_group->start,
						       BTRFS_SUPER_INFO_OFFSET));

	path->skip_locking = 1;
	path->search_commit_root = 1;
	path->reada = READA_FORWARD;

	search_offset = index * div_u64(block_group->length, max_index);
	search_key.objectid = block_group->start + search_offset;
	search_key.type = BTRFS_EXTENT_ITEM_KEY;
	search_key.offset = 0;

	btrfs_for_each_slot(extent_root, &search_key, found_key, path, ret) {
		 
		if (found_key->type == BTRFS_EXTENT_ITEM_KEY &&
		    found_key->objectid >= block_group->start &&
		    found_key->objectid + found_key->offset <= search_end)
			break;

		 
		if (found_key->objectid >= search_end) {
			ret = 1;
			break;
		}
	}

	lockdep_assert_held(&caching_ctl->mutex);
	lockdep_assert_held_read(&fs_info->commit_root_sem);
	btrfs_free_path(path);
	return ret;
}

 
static int load_block_group_size_class(struct btrfs_caching_control *caching_ctl,
				       struct btrfs_block_group *block_group)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct btrfs_key key;
	int i;
	u64 min_size = block_group->length;
	enum btrfs_block_group_size_class size_class = BTRFS_BG_SZ_NONE;
	int ret;

	if (!btrfs_block_group_should_use_size_class(block_group))
		return 0;

	lockdep_assert_held(&caching_ctl->mutex);
	lockdep_assert_held_read(&fs_info->commit_root_sem);
	for (i = 0; i < 5; ++i) {
		ret = sample_block_group_extent_item(caching_ctl, block_group, i, 5, &key);
		if (ret < 0)
			goto out;
		if (ret > 0)
			continue;
		min_size = min_t(u64, min_size, key.offset);
		size_class = btrfs_calc_block_group_size_class(min_size);
	}
	if (size_class != BTRFS_BG_SZ_NONE) {
		spin_lock(&block_group->lock);
		block_group->size_class = size_class;
		spin_unlock(&block_group->lock);
	}
out:
	return ret;
}

static int load_extent_tree_free(struct btrfs_caching_control *caching_ctl)
{
	struct btrfs_block_group *block_group = caching_ctl->block_group;
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct btrfs_root *extent_root;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u64 total_found = 0;
	u64 last = 0;
	u32 nritems;
	int ret;
	bool wakeup = true;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	last = max_t(u64, block_group->start, BTRFS_SUPER_INFO_OFFSET);
	extent_root = btrfs_extent_root(fs_info, last);

#ifdef CONFIG_BTRFS_DEBUG
	 
	if (btrfs_should_fragment_free_space(block_group))
		wakeup = false;
#endif
	 
	path->skip_locking = 1;
	path->search_commit_root = 1;
	path->reada = READA_FORWARD;

	key.objectid = last;
	key.offset = 0;
	key.type = BTRFS_EXTENT_ITEM_KEY;

next:
	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);

	while (1) {
		if (btrfs_fs_closing(fs_info) > 1) {
			last = (u64)-1;
			break;
		}

		if (path->slots[0] < nritems) {
			btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		} else {
			ret = btrfs_find_next_key(extent_root, path, &key, 0, 0);
			if (ret)
				break;

			if (need_resched() ||
			    rwsem_is_contended(&fs_info->commit_root_sem)) {
				btrfs_release_path(path);
				up_read(&fs_info->commit_root_sem);
				mutex_unlock(&caching_ctl->mutex);
				cond_resched();
				mutex_lock(&caching_ctl->mutex);
				down_read(&fs_info->commit_root_sem);
				goto next;
			}

			ret = btrfs_next_leaf(extent_root, path);
			if (ret < 0)
				goto out;
			if (ret)
				break;
			leaf = path->nodes[0];
			nritems = btrfs_header_nritems(leaf);
			continue;
		}

		if (key.objectid < last) {
			key.objectid = last;
			key.offset = 0;
			key.type = BTRFS_EXTENT_ITEM_KEY;
			btrfs_release_path(path);
			goto next;
		}

		if (key.objectid < block_group->start) {
			path->slots[0]++;
			continue;
		}

		if (key.objectid >= block_group->start + block_group->length)
			break;

		if (key.type == BTRFS_EXTENT_ITEM_KEY ||
		    key.type == BTRFS_METADATA_ITEM_KEY) {
			u64 space_added;

			ret = btrfs_add_new_free_space(block_group, last,
						       key.objectid, &space_added);
			if (ret)
				goto out;
			total_found += space_added;
			if (key.type == BTRFS_METADATA_ITEM_KEY)
				last = key.objectid +
					fs_info->nodesize;
			else
				last = key.objectid + key.offset;

			if (total_found > CACHING_CTL_WAKE_UP) {
				total_found = 0;
				if (wakeup) {
					atomic_inc(&caching_ctl->progress);
					wake_up(&caching_ctl->wait);
				}
			}
		}
		path->slots[0]++;
	}

	ret = btrfs_add_new_free_space(block_group, last,
				       block_group->start + block_group->length,
				       NULL);
out:
	btrfs_free_path(path);
	return ret;
}

static inline void btrfs_free_excluded_extents(const struct btrfs_block_group *bg)
{
	clear_extent_bits(&bg->fs_info->excluded_extents, bg->start,
			  bg->start + bg->length - 1, EXTENT_UPTODATE);
}

static noinline void caching_thread(struct btrfs_work *work)
{
	struct btrfs_block_group *block_group;
	struct btrfs_fs_info *fs_info;
	struct btrfs_caching_control *caching_ctl;
	int ret;

	caching_ctl = container_of(work, struct btrfs_caching_control, work);
	block_group = caching_ctl->block_group;
	fs_info = block_group->fs_info;

	mutex_lock(&caching_ctl->mutex);
	down_read(&fs_info->commit_root_sem);

	load_block_group_size_class(caching_ctl, block_group);
	if (btrfs_test_opt(fs_info, SPACE_CACHE)) {
		ret = load_free_space_cache(block_group);
		if (ret == 1) {
			ret = 0;
			goto done;
		}

		 
		spin_lock(&block_group->lock);
		block_group->cached = BTRFS_CACHE_STARTED;
		spin_unlock(&block_group->lock);
		wake_up(&caching_ctl->wait);
	}

	 
	if (btrfs_fs_compat_ro(fs_info, FREE_SPACE_TREE) &&
	    !(test_bit(BTRFS_FS_FREE_SPACE_TREE_UNTRUSTED, &fs_info->flags)))
		ret = load_free_space_tree(caching_ctl);
	else
		ret = load_extent_tree_free(caching_ctl);
done:
	spin_lock(&block_group->lock);
	block_group->caching_ctl = NULL;
	block_group->cached = ret ? BTRFS_CACHE_ERROR : BTRFS_CACHE_FINISHED;
	spin_unlock(&block_group->lock);

#ifdef CONFIG_BTRFS_DEBUG
	if (btrfs_should_fragment_free_space(block_group)) {
		u64 bytes_used;

		spin_lock(&block_group->space_info->lock);
		spin_lock(&block_group->lock);
		bytes_used = block_group->length - block_group->used;
		block_group->space_info->bytes_used += bytes_used >> 1;
		spin_unlock(&block_group->lock);
		spin_unlock(&block_group->space_info->lock);
		fragment_free_space(block_group);
	}
#endif

	up_read(&fs_info->commit_root_sem);
	btrfs_free_excluded_extents(block_group);
	mutex_unlock(&caching_ctl->mutex);

	wake_up(&caching_ctl->wait);

	btrfs_put_caching_control(caching_ctl);
	btrfs_put_block_group(block_group);
}

int btrfs_cache_block_group(struct btrfs_block_group *cache, bool wait)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct btrfs_caching_control *caching_ctl = NULL;
	int ret = 0;

	 
	if (btrfs_is_zoned(fs_info))
		return 0;

	caching_ctl = kzalloc(sizeof(*caching_ctl), GFP_NOFS);
	if (!caching_ctl)
		return -ENOMEM;

	INIT_LIST_HEAD(&caching_ctl->list);
	mutex_init(&caching_ctl->mutex);
	init_waitqueue_head(&caching_ctl->wait);
	caching_ctl->block_group = cache;
	refcount_set(&caching_ctl->count, 2);
	atomic_set(&caching_ctl->progress, 0);
	btrfs_init_work(&caching_ctl->work, caching_thread, NULL, NULL);

	spin_lock(&cache->lock);
	if (cache->cached != BTRFS_CACHE_NO) {
		kfree(caching_ctl);

		caching_ctl = cache->caching_ctl;
		if (caching_ctl)
			refcount_inc(&caching_ctl->count);
		spin_unlock(&cache->lock);
		goto out;
	}
	WARN_ON(cache->caching_ctl);
	cache->caching_ctl = caching_ctl;
	cache->cached = BTRFS_CACHE_STARTED;
	spin_unlock(&cache->lock);

	write_lock(&fs_info->block_group_cache_lock);
	refcount_inc(&caching_ctl->count);
	list_add_tail(&caching_ctl->list, &fs_info->caching_block_groups);
	write_unlock(&fs_info->block_group_cache_lock);

	btrfs_get_block_group(cache);

	btrfs_queue_work(fs_info->caching_workers, &caching_ctl->work);
out:
	if (wait && caching_ctl)
		ret = btrfs_caching_ctl_wait_done(cache, caching_ctl);
	if (caching_ctl)
		btrfs_put_caching_control(caching_ctl);

	return ret;
}

static void clear_avail_alloc_bits(struct btrfs_fs_info *fs_info, u64 flags)
{
	u64 extra_flags = chunk_to_extended(flags) &
				BTRFS_EXTENDED_PROFILE_MASK;

	write_seqlock(&fs_info->profiles_lock);
	if (flags & BTRFS_BLOCK_GROUP_DATA)
		fs_info->avail_data_alloc_bits &= ~extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_METADATA)
		fs_info->avail_metadata_alloc_bits &= ~extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		fs_info->avail_system_alloc_bits &= ~extra_flags;
	write_sequnlock(&fs_info->profiles_lock);
}

 
static void clear_incompat_bg_bits(struct btrfs_fs_info *fs_info, u64 flags)
{
	bool found_raid56 = false;
	bool found_raid1c34 = false;

	if ((flags & BTRFS_BLOCK_GROUP_RAID56_MASK) ||
	    (flags & BTRFS_BLOCK_GROUP_RAID1C3) ||
	    (flags & BTRFS_BLOCK_GROUP_RAID1C4)) {
		struct list_head *head = &fs_info->space_info;
		struct btrfs_space_info *sinfo;

		list_for_each_entry_rcu(sinfo, head, list) {
			down_read(&sinfo->groups_sem);
			if (!list_empty(&sinfo->block_groups[BTRFS_RAID_RAID5]))
				found_raid56 = true;
			if (!list_empty(&sinfo->block_groups[BTRFS_RAID_RAID6]))
				found_raid56 = true;
			if (!list_empty(&sinfo->block_groups[BTRFS_RAID_RAID1C3]))
				found_raid1c34 = true;
			if (!list_empty(&sinfo->block_groups[BTRFS_RAID_RAID1C4]))
				found_raid1c34 = true;
			up_read(&sinfo->groups_sem);
		}
		if (!found_raid56)
			btrfs_clear_fs_incompat(fs_info, RAID56);
		if (!found_raid1c34)
			btrfs_clear_fs_incompat(fs_info, RAID1C34);
	}
}

static int remove_block_group_item(struct btrfs_trans_handle *trans,
				   struct btrfs_path *path,
				   struct btrfs_block_group *block_group)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *root;
	struct btrfs_key key;
	int ret;

	root = btrfs_block_group_root(fs_info);
	key.objectid = block_group->start;
	key.type = BTRFS_BLOCK_GROUP_ITEM_KEY;
	key.offset = block_group->length;

	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret > 0)
		ret = -ENOENT;
	if (ret < 0)
		return ret;

	ret = btrfs_del_item(trans, root, path);
	return ret;
}

int btrfs_remove_block_group(struct btrfs_trans_handle *trans,
			     u64 group_start, struct extent_map *em)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_path *path;
	struct btrfs_block_group *block_group;
	struct btrfs_free_cluster *cluster;
	struct inode *inode;
	struct kobject *kobj = NULL;
	int ret;
	int index;
	int factor;
	struct btrfs_caching_control *caching_ctl = NULL;
	bool remove_em;
	bool remove_rsv = false;

	block_group = btrfs_lookup_block_group(fs_info, group_start);
	BUG_ON(!block_group);
	BUG_ON(!block_group->ro);

	trace_btrfs_remove_block_group(block_group);
	 
	btrfs_free_excluded_extents(block_group);
	btrfs_free_ref_tree_range(fs_info, block_group->start,
				  block_group->length);

	index = btrfs_bg_flags_to_raid_index(block_group->flags);
	factor = btrfs_bg_type_to_factor(block_group->flags);

	 
	cluster = &fs_info->data_alloc_cluster;
	spin_lock(&cluster->refill_lock);
	btrfs_return_cluster_to_free_space(block_group, cluster);
	spin_unlock(&cluster->refill_lock);

	 
	cluster = &fs_info->meta_alloc_cluster;
	spin_lock(&cluster->refill_lock);
	btrfs_return_cluster_to_free_space(block_group, cluster);
	spin_unlock(&cluster->refill_lock);

	btrfs_clear_treelog_bg(block_group);
	btrfs_clear_data_reloc_bg(block_group);

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	 
	inode = lookup_free_space_inode(block_group, path);

	mutex_lock(&trans->transaction->cache_write_mutex);
	 
	spin_lock(&trans->transaction->dirty_bgs_lock);
	if (!list_empty(&block_group->io_list)) {
		list_del_init(&block_group->io_list);

		WARN_ON(!IS_ERR(inode) && inode != block_group->io_ctl.inode);

		spin_unlock(&trans->transaction->dirty_bgs_lock);
		btrfs_wait_cache_io(trans, block_group, path);
		btrfs_put_block_group(block_group);
		spin_lock(&trans->transaction->dirty_bgs_lock);
	}

	if (!list_empty(&block_group->dirty_list)) {
		list_del_init(&block_group->dirty_list);
		remove_rsv = true;
		btrfs_put_block_group(block_group);
	}
	spin_unlock(&trans->transaction->dirty_bgs_lock);
	mutex_unlock(&trans->transaction->cache_write_mutex);

	ret = btrfs_remove_free_space_inode(trans, inode, block_group);
	if (ret)
		goto out;

	write_lock(&fs_info->block_group_cache_lock);
	rb_erase_cached(&block_group->cache_node,
			&fs_info->block_group_cache_tree);
	RB_CLEAR_NODE(&block_group->cache_node);

	 
	btrfs_put_block_group(block_group);

	write_unlock(&fs_info->block_group_cache_lock);

	down_write(&block_group->space_info->groups_sem);
	 
	list_del_init(&block_group->list);
	if (list_empty(&block_group->space_info->block_groups[index])) {
		kobj = block_group->space_info->block_group_kobjs[index];
		block_group->space_info->block_group_kobjs[index] = NULL;
		clear_avail_alloc_bits(fs_info, block_group->flags);
	}
	up_write(&block_group->space_info->groups_sem);
	clear_incompat_bg_bits(fs_info, block_group->flags);
	if (kobj) {
		kobject_del(kobj);
		kobject_put(kobj);
	}

	if (block_group->cached == BTRFS_CACHE_STARTED)
		btrfs_wait_block_group_cache_done(block_group);

	write_lock(&fs_info->block_group_cache_lock);
	caching_ctl = btrfs_get_caching_control(block_group);
	if (!caching_ctl) {
		struct btrfs_caching_control *ctl;

		list_for_each_entry(ctl, &fs_info->caching_block_groups, list) {
			if (ctl->block_group == block_group) {
				caching_ctl = ctl;
				refcount_inc(&caching_ctl->count);
				break;
			}
		}
	}
	if (caching_ctl)
		list_del_init(&caching_ctl->list);
	write_unlock(&fs_info->block_group_cache_lock);

	if (caching_ctl) {
		 
		btrfs_put_caching_control(caching_ctl);
		btrfs_put_caching_control(caching_ctl);
	}

	spin_lock(&trans->transaction->dirty_bgs_lock);
	WARN_ON(!list_empty(&block_group->dirty_list));
	WARN_ON(!list_empty(&block_group->io_list));
	spin_unlock(&trans->transaction->dirty_bgs_lock);

	btrfs_remove_free_space_cache(block_group);

	spin_lock(&block_group->space_info->lock);
	list_del_init(&block_group->ro_list);

	if (btrfs_test_opt(fs_info, ENOSPC_DEBUG)) {
		WARN_ON(block_group->space_info->total_bytes
			< block_group->length);
		WARN_ON(block_group->space_info->bytes_readonly
			< block_group->length - block_group->zone_unusable);
		WARN_ON(block_group->space_info->bytes_zone_unusable
			< block_group->zone_unusable);
		WARN_ON(block_group->space_info->disk_total
			< block_group->length * factor);
	}
	block_group->space_info->total_bytes -= block_group->length;
	block_group->space_info->bytes_readonly -=
		(block_group->length - block_group->zone_unusable);
	block_group->space_info->bytes_zone_unusable -=
		block_group->zone_unusable;
	block_group->space_info->disk_total -= block_group->length * factor;

	spin_unlock(&block_group->space_info->lock);

	 
	ret = remove_block_group_free_space(trans, block_group);
	if (ret)
		goto out;

	ret = remove_block_group_item(trans, path, block_group);
	if (ret < 0)
		goto out;

	spin_lock(&block_group->lock);
	set_bit(BLOCK_GROUP_FLAG_REMOVED, &block_group->runtime_flags);

	 
	remove_em = (atomic_read(&block_group->frozen) == 0);
	spin_unlock(&block_group->lock);

	if (remove_em) {
		struct extent_map_tree *em_tree;

		em_tree = &fs_info->mapping_tree;
		write_lock(&em_tree->lock);
		remove_extent_mapping(em_tree, em);
		write_unlock(&em_tree->lock);
		 
		free_extent_map(em);
	}

out:
	 
	btrfs_put_block_group(block_group);
	if (remove_rsv)
		btrfs_delayed_refs_rsv_release(fs_info, 1);
	btrfs_free_path(path);
	return ret;
}

struct btrfs_trans_handle *btrfs_start_trans_remove_block_group(
		struct btrfs_fs_info *fs_info, const u64 chunk_offset)
{
	struct btrfs_root *root = btrfs_block_group_root(fs_info);
	struct extent_map_tree *em_tree = &fs_info->mapping_tree;
	struct extent_map *em;
	struct map_lookup *map;
	unsigned int num_items;

	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree, chunk_offset, 1);
	read_unlock(&em_tree->lock);
	ASSERT(em && em->start == chunk_offset);

	 
	map = em->map_lookup;
	num_items = 3 + map->num_stripes;
	free_extent_map(em);

	return btrfs_start_transaction_fallback_global_rsv(root, num_items);
}

 
static int inc_block_group_ro(struct btrfs_block_group *cache, int force)
{
	struct btrfs_space_info *sinfo = cache->space_info;
	u64 num_bytes;
	int ret = -ENOSPC;

	spin_lock(&sinfo->lock);
	spin_lock(&cache->lock);

	if (cache->swap_extents) {
		ret = -ETXTBSY;
		goto out;
	}

	if (cache->ro) {
		cache->ro++;
		ret = 0;
		goto out;
	}

	num_bytes = cache->length - cache->reserved - cache->pinned -
		    cache->bytes_super - cache->zone_unusable - cache->used;

	 
	if (force) {
		ret = 0;
	} else if (sinfo->flags & BTRFS_BLOCK_GROUP_DATA) {
		u64 sinfo_used = btrfs_space_info_used(sinfo, true);

		 
		if (sinfo_used + num_bytes <= sinfo->total_bytes)
			ret = 0;
	} else {
		 
		if (btrfs_can_overcommit(cache->fs_info, sinfo, num_bytes,
					 BTRFS_RESERVE_NO_FLUSH))
			ret = 0;
	}

	if (!ret) {
		sinfo->bytes_readonly += num_bytes;
		if (btrfs_is_zoned(cache->fs_info)) {
			 
			sinfo->bytes_readonly += cache->zone_unusable;
			sinfo->bytes_zone_unusable -= cache->zone_unusable;
			cache->zone_unusable = 0;
		}
		cache->ro++;
		list_add_tail(&cache->ro_list, &sinfo->ro_bgs);
	}
out:
	spin_unlock(&cache->lock);
	spin_unlock(&sinfo->lock);
	if (ret == -ENOSPC && btrfs_test_opt(cache->fs_info, ENOSPC_DEBUG)) {
		btrfs_info(cache->fs_info,
			"unable to make block group %llu ro", cache->start);
		btrfs_dump_space_info(cache->fs_info, cache->space_info, 0, 0);
	}
	return ret;
}

static bool clean_pinned_extents(struct btrfs_trans_handle *trans,
				 struct btrfs_block_group *bg)
{
	struct btrfs_fs_info *fs_info = bg->fs_info;
	struct btrfs_transaction *prev_trans = NULL;
	const u64 start = bg->start;
	const u64 end = start + bg->length - 1;
	int ret;

	spin_lock(&fs_info->trans_lock);
	if (trans->transaction->list.prev != &fs_info->trans_list) {
		prev_trans = list_last_entry(&trans->transaction->list,
					     struct btrfs_transaction, list);
		refcount_inc(&prev_trans->use_count);
	}
	spin_unlock(&fs_info->trans_lock);

	 
	mutex_lock(&fs_info->unused_bg_unpin_mutex);
	if (prev_trans) {
		ret = clear_extent_bits(&prev_trans->pinned_extents, start, end,
					EXTENT_DIRTY);
		if (ret)
			goto out;
	}

	ret = clear_extent_bits(&trans->transaction->pinned_extents, start, end,
				EXTENT_DIRTY);
out:
	mutex_unlock(&fs_info->unused_bg_unpin_mutex);
	if (prev_trans)
		btrfs_put_transaction(prev_trans);

	return ret == 0;
}

 
void btrfs_delete_unused_bgs(struct btrfs_fs_info *fs_info)
{
	struct btrfs_block_group *block_group;
	struct btrfs_space_info *space_info;
	struct btrfs_trans_handle *trans;
	const bool async_trim_enabled = btrfs_test_opt(fs_info, DISCARD_ASYNC);
	int ret = 0;

	if (!test_bit(BTRFS_FS_OPEN, &fs_info->flags))
		return;

	if (btrfs_fs_closing(fs_info))
		return;

	 
	if (!mutex_trylock(&fs_info->reclaim_bgs_lock))
		return;

	spin_lock(&fs_info->unused_bgs_lock);
	while (!list_empty(&fs_info->unused_bgs)) {
		int trimming;

		block_group = list_first_entry(&fs_info->unused_bgs,
					       struct btrfs_block_group,
					       bg_list);
		list_del_init(&block_group->bg_list);

		space_info = block_group->space_info;

		if (ret || btrfs_mixed_space_info(space_info)) {
			btrfs_put_block_group(block_group);
			continue;
		}
		spin_unlock(&fs_info->unused_bgs_lock);

		btrfs_discard_cancel_work(&fs_info->discard_ctl, block_group);

		 
		down_write(&space_info->groups_sem);

		 
		if (btrfs_test_opt(fs_info, DISCARD_ASYNC) &&
		    !btrfs_is_free_space_trimmed(block_group)) {
			trace_btrfs_skip_unused_block_group(block_group);
			up_write(&space_info->groups_sem);
			 
			btrfs_discard_queue_work(&fs_info->discard_ctl,
						 block_group);
			goto next;
		}

		spin_lock(&block_group->lock);
		if (block_group->reserved || block_group->pinned ||
		    block_group->used || block_group->ro ||
		    list_is_singular(&block_group->list)) {
			 
			trace_btrfs_skip_unused_block_group(block_group);
			spin_unlock(&block_group->lock);
			up_write(&space_info->groups_sem);
			goto next;
		}
		spin_unlock(&block_group->lock);

		 
		ret = inc_block_group_ro(block_group, 0);
		up_write(&space_info->groups_sem);
		if (ret < 0) {
			ret = 0;
			goto next;
		}

		ret = btrfs_zone_finish(block_group);
		if (ret < 0) {
			btrfs_dec_block_group_ro(block_group);
			if (ret == -EAGAIN)
				ret = 0;
			goto next;
		}

		 
		trans = btrfs_start_trans_remove_block_group(fs_info,
						     block_group->start);
		if (IS_ERR(trans)) {
			btrfs_dec_block_group_ro(block_group);
			ret = PTR_ERR(trans);
			goto next;
		}

		 
		if (!clean_pinned_extents(trans, block_group)) {
			btrfs_dec_block_group_ro(block_group);
			goto end_trans;
		}

		 
		spin_lock(&fs_info->discard_ctl.lock);
		if (!list_empty(&block_group->discard_list)) {
			spin_unlock(&fs_info->discard_ctl.lock);
			btrfs_dec_block_group_ro(block_group);
			btrfs_discard_queue_work(&fs_info->discard_ctl,
						 block_group);
			goto end_trans;
		}
		spin_unlock(&fs_info->discard_ctl.lock);

		 
		spin_lock(&space_info->lock);
		spin_lock(&block_group->lock);

		btrfs_space_info_update_bytes_pinned(fs_info, space_info,
						     -block_group->pinned);
		space_info->bytes_readonly += block_group->pinned;
		block_group->pinned = 0;

		spin_unlock(&block_group->lock);
		spin_unlock(&space_info->lock);

		 
		if (!async_trim_enabled && btrfs_test_opt(fs_info, DISCARD_ASYNC))
			goto flip_async;

		 
		trimming = btrfs_test_opt(fs_info, DISCARD_SYNC) ||
				btrfs_is_zoned(fs_info);

		 
		if (trimming)
			btrfs_freeze_block_group(block_group);

		 
		ret = btrfs_remove_chunk(trans, block_group->start);

		if (ret) {
			if (trimming)
				btrfs_unfreeze_block_group(block_group);
			goto end_trans;
		}

		 
		if (trimming) {
			spin_lock(&fs_info->unused_bgs_lock);
			 
			list_move(&block_group->bg_list,
				  &trans->transaction->deleted_bgs);
			spin_unlock(&fs_info->unused_bgs_lock);
			btrfs_get_block_group(block_group);
		}
end_trans:
		btrfs_end_transaction(trans);
next:
		btrfs_put_block_group(block_group);
		spin_lock(&fs_info->unused_bgs_lock);
	}
	spin_unlock(&fs_info->unused_bgs_lock);
	mutex_unlock(&fs_info->reclaim_bgs_lock);
	return;

flip_async:
	btrfs_end_transaction(trans);
	mutex_unlock(&fs_info->reclaim_bgs_lock);
	btrfs_put_block_group(block_group);
	btrfs_discard_punt_unused_bgs_list(fs_info);
}

void btrfs_mark_bg_unused(struct btrfs_block_group *bg)
{
	struct btrfs_fs_info *fs_info = bg->fs_info;

	spin_lock(&fs_info->unused_bgs_lock);
	if (list_empty(&bg->bg_list)) {
		btrfs_get_block_group(bg);
		trace_btrfs_add_unused_block_group(bg);
		list_add_tail(&bg->bg_list, &fs_info->unused_bgs);
	} else if (!test_bit(BLOCK_GROUP_FLAG_NEW, &bg->runtime_flags)) {
		 
		trace_btrfs_add_unused_block_group(bg);
		list_move_tail(&bg->bg_list, &fs_info->unused_bgs);
	}
	spin_unlock(&fs_info->unused_bgs_lock);
}

 
static int reclaim_bgs_cmp(void *unused, const struct list_head *a,
			   const struct list_head *b)
{
	const struct btrfs_block_group *bg1, *bg2;

	bg1 = list_entry(a, struct btrfs_block_group, bg_list);
	bg2 = list_entry(b, struct btrfs_block_group, bg_list);

	return bg1->used > bg2->used;
}

static inline bool btrfs_should_reclaim(struct btrfs_fs_info *fs_info)
{
	if (btrfs_is_zoned(fs_info))
		return btrfs_zoned_should_reclaim(fs_info);
	return true;
}

static bool should_reclaim_block_group(struct btrfs_block_group *bg, u64 bytes_freed)
{
	const struct btrfs_space_info *space_info = bg->space_info;
	const int reclaim_thresh = READ_ONCE(space_info->bg_reclaim_threshold);
	const u64 new_val = bg->used;
	const u64 old_val = new_val + bytes_freed;
	u64 thresh;

	if (reclaim_thresh == 0)
		return false;

	thresh = mult_perc(bg->length, reclaim_thresh);

	 
	if (old_val < thresh)
		return false;
	if (new_val >= thresh)
		return false;
	return true;
}

void btrfs_reclaim_bgs_work(struct work_struct *work)
{
	struct btrfs_fs_info *fs_info =
		container_of(work, struct btrfs_fs_info, reclaim_bgs_work);
	struct btrfs_block_group *bg;
	struct btrfs_space_info *space_info;

	if (!test_bit(BTRFS_FS_OPEN, &fs_info->flags))
		return;

	if (btrfs_fs_closing(fs_info))
		return;

	if (!btrfs_should_reclaim(fs_info))
		return;

	sb_start_write(fs_info->sb);

	if (!btrfs_exclop_start(fs_info, BTRFS_EXCLOP_BALANCE)) {
		sb_end_write(fs_info->sb);
		return;
	}

	 
	if (!mutex_trylock(&fs_info->reclaim_bgs_lock)) {
		btrfs_exclop_finish(fs_info);
		sb_end_write(fs_info->sb);
		return;
	}

	spin_lock(&fs_info->unused_bgs_lock);
	 
	list_sort(NULL, &fs_info->reclaim_bgs, reclaim_bgs_cmp);
	while (!list_empty(&fs_info->reclaim_bgs)) {
		u64 zone_unusable;
		int ret = 0;

		bg = list_first_entry(&fs_info->reclaim_bgs,
				      struct btrfs_block_group,
				      bg_list);
		list_del_init(&bg->bg_list);

		space_info = bg->space_info;
		spin_unlock(&fs_info->unused_bgs_lock);

		 
		down_write(&space_info->groups_sem);

		spin_lock(&bg->lock);
		if (bg->reserved || bg->pinned || bg->ro) {
			 
			spin_unlock(&bg->lock);
			up_write(&space_info->groups_sem);
			goto next;
		}
		if (bg->used == 0) {
			 
			if (!btrfs_test_opt(fs_info, DISCARD_ASYNC))
				btrfs_mark_bg_unused(bg);
			spin_unlock(&bg->lock);
			up_write(&space_info->groups_sem);
			goto next;

		}
		 
		if (!should_reclaim_block_group(bg, bg->length)) {
			spin_unlock(&bg->lock);
			up_write(&space_info->groups_sem);
			goto next;
		}
		spin_unlock(&bg->lock);

		 
		if (btrfs_need_cleaner_sleep(fs_info)) {
			up_write(&space_info->groups_sem);
			goto next;
		}

		 
		zone_unusable = bg->zone_unusable;
		ret = inc_block_group_ro(bg, 0);
		up_write(&space_info->groups_sem);
		if (ret < 0)
			goto next;

		btrfs_info(fs_info,
			"reclaiming chunk %llu with %llu%% used %llu%% unusable",
				bg->start,
				div64_u64(bg->used * 100, bg->length),
				div64_u64(zone_unusable * 100, bg->length));
		trace_btrfs_reclaim_block_group(bg);
		ret = btrfs_relocate_chunk(fs_info, bg->start);
		if (ret) {
			btrfs_dec_block_group_ro(bg);
			btrfs_err(fs_info, "error relocating chunk %llu",
				  bg->start);
		}

next:
		if (ret)
			btrfs_mark_bg_to_reclaim(bg);
		btrfs_put_block_group(bg);

		mutex_unlock(&fs_info->reclaim_bgs_lock);
		 
		btrfs_delete_unused_bgs(fs_info);
		 
		if (!mutex_trylock(&fs_info->reclaim_bgs_lock))
			goto end;
		spin_lock(&fs_info->unused_bgs_lock);
	}
	spin_unlock(&fs_info->unused_bgs_lock);
	mutex_unlock(&fs_info->reclaim_bgs_lock);
end:
	btrfs_exclop_finish(fs_info);
	sb_end_write(fs_info->sb);
}

void btrfs_reclaim_bgs(struct btrfs_fs_info *fs_info)
{
	spin_lock(&fs_info->unused_bgs_lock);
	if (!list_empty(&fs_info->reclaim_bgs))
		queue_work(system_unbound_wq, &fs_info->reclaim_bgs_work);
	spin_unlock(&fs_info->unused_bgs_lock);
}

void btrfs_mark_bg_to_reclaim(struct btrfs_block_group *bg)
{
	struct btrfs_fs_info *fs_info = bg->fs_info;

	spin_lock(&fs_info->unused_bgs_lock);
	if (list_empty(&bg->bg_list)) {
		btrfs_get_block_group(bg);
		trace_btrfs_add_reclaim_block_group(bg);
		list_add_tail(&bg->bg_list, &fs_info->reclaim_bgs);
	}
	spin_unlock(&fs_info->unused_bgs_lock);
}

static int read_bg_from_eb(struct btrfs_fs_info *fs_info, struct btrfs_key *key,
			   struct btrfs_path *path)
{
	struct extent_map_tree *em_tree;
	struct extent_map *em;
	struct btrfs_block_group_item bg;
	struct extent_buffer *leaf;
	int slot;
	u64 flags;
	int ret = 0;

	slot = path->slots[0];
	leaf = path->nodes[0];

	em_tree = &fs_info->mapping_tree;
	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree, key->objectid, key->offset);
	read_unlock(&em_tree->lock);
	if (!em) {
		btrfs_err(fs_info,
			  "logical %llu len %llu found bg but no related chunk",
			  key->objectid, key->offset);
		return -ENOENT;
	}

	if (em->start != key->objectid || em->len != key->offset) {
		btrfs_err(fs_info,
			"block group %llu len %llu mismatch with chunk %llu len %llu",
			key->objectid, key->offset, em->start, em->len);
		ret = -EUCLEAN;
		goto out_free_em;
	}

	read_extent_buffer(leaf, &bg, btrfs_item_ptr_offset(leaf, slot),
			   sizeof(bg));
	flags = btrfs_stack_block_group_flags(&bg) &
		BTRFS_BLOCK_GROUP_TYPE_MASK;

	if (flags != (em->map_lookup->type & BTRFS_BLOCK_GROUP_TYPE_MASK)) {
		btrfs_err(fs_info,
"block group %llu len %llu type flags 0x%llx mismatch with chunk type flags 0x%llx",
			  key->objectid, key->offset, flags,
			  (BTRFS_BLOCK_GROUP_TYPE_MASK & em->map_lookup->type));
		ret = -EUCLEAN;
	}

out_free_em:
	free_extent_map(em);
	return ret;
}

static int find_first_block_group(struct btrfs_fs_info *fs_info,
				  struct btrfs_path *path,
				  struct btrfs_key *key)
{
	struct btrfs_root *root = btrfs_block_group_root(fs_info);
	int ret;
	struct btrfs_key found_key;

	btrfs_for_each_slot(root, key, &found_key, path, ret) {
		if (found_key.objectid >= key->objectid &&
		    found_key.type == BTRFS_BLOCK_GROUP_ITEM_KEY) {
			return read_bg_from_eb(fs_info, &found_key, path);
		}
	}
	return ret;
}

static void set_avail_alloc_bits(struct btrfs_fs_info *fs_info, u64 flags)
{
	u64 extra_flags = chunk_to_extended(flags) &
				BTRFS_EXTENDED_PROFILE_MASK;

	write_seqlock(&fs_info->profiles_lock);
	if (flags & BTRFS_BLOCK_GROUP_DATA)
		fs_info->avail_data_alloc_bits |= extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_METADATA)
		fs_info->avail_metadata_alloc_bits |= extra_flags;
	if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		fs_info->avail_system_alloc_bits |= extra_flags;
	write_sequnlock(&fs_info->profiles_lock);
}

 
int btrfs_rmap_block(struct btrfs_fs_info *fs_info, u64 chunk_start,
		     u64 physical, u64 **logical, int *naddrs, int *stripe_len)
{
	struct extent_map *em;
	struct map_lookup *map;
	u64 *buf;
	u64 bytenr;
	u64 data_stripe_length;
	u64 io_stripe_size;
	int i, nr = 0;
	int ret = 0;

	em = btrfs_get_chunk_map(fs_info, chunk_start, 1);
	if (IS_ERR(em))
		return -EIO;

	map = em->map_lookup;
	data_stripe_length = em->orig_block_len;
	io_stripe_size = BTRFS_STRIPE_LEN;
	chunk_start = em->start;

	 
	if (map->type & BTRFS_BLOCK_GROUP_RAID56_MASK)
		io_stripe_size = btrfs_stripe_nr_to_offset(nr_data_stripes(map));

	buf = kcalloc(map->num_stripes, sizeof(u64), GFP_NOFS);
	if (!buf) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < map->num_stripes; i++) {
		bool already_inserted = false;
		u32 stripe_nr;
		u32 offset;
		int j;

		if (!in_range(physical, map->stripes[i].physical,
			      data_stripe_length))
			continue;

		stripe_nr = (physical - map->stripes[i].physical) >>
			    BTRFS_STRIPE_LEN_SHIFT;
		offset = (physical - map->stripes[i].physical) &
			 BTRFS_STRIPE_LEN_MASK;

		if (map->type & (BTRFS_BLOCK_GROUP_RAID0 |
				 BTRFS_BLOCK_GROUP_RAID10))
			stripe_nr = div_u64(stripe_nr * map->num_stripes + i,
					    map->sub_stripes);
		 
		bytenr = chunk_start + stripe_nr * io_stripe_size + offset;

		 
		for (j = 0; j < nr; j++) {
			if (buf[j] == bytenr) {
				already_inserted = true;
				break;
			}
		}

		if (!already_inserted)
			buf[nr++] = bytenr;
	}

	*logical = buf;
	*naddrs = nr;
	*stripe_len = io_stripe_size;
out:
	free_extent_map(em);
	return ret;
}

static int exclude_super_stripes(struct btrfs_block_group *cache)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	const bool zoned = btrfs_is_zoned(fs_info);
	u64 bytenr;
	u64 *logical;
	int stripe_len;
	int i, nr, ret;

	if (cache->start < BTRFS_SUPER_INFO_OFFSET) {
		stripe_len = BTRFS_SUPER_INFO_OFFSET - cache->start;
		cache->bytes_super += stripe_len;
		ret = set_extent_bit(&fs_info->excluded_extents, cache->start,
				     cache->start + stripe_len - 1,
				     EXTENT_UPTODATE, NULL);
		if (ret)
			return ret;
	}

	for (i = 0; i < BTRFS_SUPER_MIRROR_MAX; i++) {
		bytenr = btrfs_sb_offset(i);
		ret = btrfs_rmap_block(fs_info, cache->start,
				       bytenr, &logical, &nr, &stripe_len);
		if (ret)
			return ret;

		 
		if (zoned && nr) {
			kfree(logical);
			btrfs_err(fs_info,
			"zoned: block group %llu must not contain super block",
				  cache->start);
			return -EUCLEAN;
		}

		while (nr--) {
			u64 len = min_t(u64, stripe_len,
				cache->start + cache->length - logical[nr]);

			cache->bytes_super += len;
			ret = set_extent_bit(&fs_info->excluded_extents, logical[nr],
					     logical[nr] + len - 1,
					     EXTENT_UPTODATE, NULL);
			if (ret) {
				kfree(logical);
				return ret;
			}
		}

		kfree(logical);
	}
	return 0;
}

static struct btrfs_block_group *btrfs_create_block_group_cache(
		struct btrfs_fs_info *fs_info, u64 start)
{
	struct btrfs_block_group *cache;

	cache = kzalloc(sizeof(*cache), GFP_NOFS);
	if (!cache)
		return NULL;

	cache->free_space_ctl = kzalloc(sizeof(*cache->free_space_ctl),
					GFP_NOFS);
	if (!cache->free_space_ctl) {
		kfree(cache);
		return NULL;
	}

	cache->start = start;

	cache->fs_info = fs_info;
	cache->full_stripe_len = btrfs_full_stripe_len(fs_info, start);

	cache->discard_index = BTRFS_DISCARD_INDEX_UNUSED;

	refcount_set(&cache->refs, 1);
	spin_lock_init(&cache->lock);
	init_rwsem(&cache->data_rwsem);
	INIT_LIST_HEAD(&cache->list);
	INIT_LIST_HEAD(&cache->cluster_list);
	INIT_LIST_HEAD(&cache->bg_list);
	INIT_LIST_HEAD(&cache->ro_list);
	INIT_LIST_HEAD(&cache->discard_list);
	INIT_LIST_HEAD(&cache->dirty_list);
	INIT_LIST_HEAD(&cache->io_list);
	INIT_LIST_HEAD(&cache->active_bg_list);
	btrfs_init_free_space_ctl(cache, cache->free_space_ctl);
	atomic_set(&cache->frozen, 0);
	mutex_init(&cache->free_space_lock);

	return cache;
}

 
static int check_chunk_block_group_mappings(struct btrfs_fs_info *fs_info)
{
	struct extent_map_tree *map_tree = &fs_info->mapping_tree;
	struct extent_map *em;
	struct btrfs_block_group *bg;
	u64 start = 0;
	int ret = 0;

	while (1) {
		read_lock(&map_tree->lock);
		 
		em = lookup_extent_mapping(map_tree, start, 1);
		read_unlock(&map_tree->lock);
		if (!em)
			break;

		bg = btrfs_lookup_block_group(fs_info, em->start);
		if (!bg) {
			btrfs_err(fs_info,
	"chunk start=%llu len=%llu doesn't have corresponding block group",
				     em->start, em->len);
			ret = -EUCLEAN;
			free_extent_map(em);
			break;
		}
		if (bg->start != em->start || bg->length != em->len ||
		    (bg->flags & BTRFS_BLOCK_GROUP_TYPE_MASK) !=
		    (em->map_lookup->type & BTRFS_BLOCK_GROUP_TYPE_MASK)) {
			btrfs_err(fs_info,
"chunk start=%llu len=%llu flags=0x%llx doesn't match block group start=%llu len=%llu flags=0x%llx",
				em->start, em->len,
				em->map_lookup->type & BTRFS_BLOCK_GROUP_TYPE_MASK,
				bg->start, bg->length,
				bg->flags & BTRFS_BLOCK_GROUP_TYPE_MASK);
			ret = -EUCLEAN;
			free_extent_map(em);
			btrfs_put_block_group(bg);
			break;
		}
		start = em->start + em->len;
		free_extent_map(em);
		btrfs_put_block_group(bg);
	}
	return ret;
}

static int read_one_block_group(struct btrfs_fs_info *info,
				struct btrfs_block_group_item *bgi,
				const struct btrfs_key *key,
				int need_clear)
{
	struct btrfs_block_group *cache;
	const bool mixed = btrfs_fs_incompat(info, MIXED_GROUPS);
	int ret;

	ASSERT(key->type == BTRFS_BLOCK_GROUP_ITEM_KEY);

	cache = btrfs_create_block_group_cache(info, key->objectid);
	if (!cache)
		return -ENOMEM;

	cache->length = key->offset;
	cache->used = btrfs_stack_block_group_used(bgi);
	cache->commit_used = cache->used;
	cache->flags = btrfs_stack_block_group_flags(bgi);
	cache->global_root_id = btrfs_stack_block_group_chunk_objectid(bgi);

	set_free_space_tree_thresholds(cache);

	if (need_clear) {
		 
		if (btrfs_test_opt(info, SPACE_CACHE))
			cache->disk_cache_state = BTRFS_DC_CLEAR;
	}
	if (!mixed && ((cache->flags & BTRFS_BLOCK_GROUP_METADATA) &&
	    (cache->flags & BTRFS_BLOCK_GROUP_DATA))) {
			btrfs_err(info,
"bg %llu is a mixed block group but filesystem hasn't enabled mixed block groups",
				  cache->start);
			ret = -EINVAL;
			goto error;
	}

	ret = btrfs_load_block_group_zone_info(cache, false);
	if (ret) {
		btrfs_err(info, "zoned: failed to load zone info of bg %llu",
			  cache->start);
		goto error;
	}

	 
	ret = exclude_super_stripes(cache);
	if (ret) {
		 
		btrfs_free_excluded_extents(cache);
		goto error;
	}

	 
	if (btrfs_is_zoned(info)) {
		btrfs_calc_zone_unusable(cache);
		 
		btrfs_free_excluded_extents(cache);
	} else if (cache->length == cache->used) {
		cache->cached = BTRFS_CACHE_FINISHED;
		btrfs_free_excluded_extents(cache);
	} else if (cache->used == 0) {
		cache->cached = BTRFS_CACHE_FINISHED;
		ret = btrfs_add_new_free_space(cache, cache->start,
					       cache->start + cache->length, NULL);
		btrfs_free_excluded_extents(cache);
		if (ret)
			goto error;
	}

	ret = btrfs_add_block_group_cache(info, cache);
	if (ret) {
		btrfs_remove_free_space_cache(cache);
		goto error;
	}
	trace_btrfs_add_block_group(info, cache, 0);
	btrfs_add_bg_to_space_info(info, cache);

	set_avail_alloc_bits(info, cache->flags);
	if (btrfs_chunk_writeable(info, cache->start)) {
		if (cache->used == 0) {
			ASSERT(list_empty(&cache->bg_list));
			if (btrfs_test_opt(info, DISCARD_ASYNC))
				btrfs_discard_queue_work(&info->discard_ctl, cache);
			else
				btrfs_mark_bg_unused(cache);
		}
	} else {
		inc_block_group_ro(cache, 1);
	}

	return 0;
error:
	btrfs_put_block_group(cache);
	return ret;
}

static int fill_dummy_bgs(struct btrfs_fs_info *fs_info)
{
	struct extent_map_tree *em_tree = &fs_info->mapping_tree;
	struct rb_node *node;
	int ret = 0;

	for (node = rb_first_cached(&em_tree->map); node; node = rb_next(node)) {
		struct extent_map *em;
		struct map_lookup *map;
		struct btrfs_block_group *bg;

		em = rb_entry(node, struct extent_map, rb_node);
		map = em->map_lookup;
		bg = btrfs_create_block_group_cache(fs_info, em->start);
		if (!bg) {
			ret = -ENOMEM;
			break;
		}

		 
		bg->length = em->len;
		bg->flags = map->type;
		bg->cached = BTRFS_CACHE_FINISHED;
		bg->used = em->len;
		bg->flags = map->type;
		ret = btrfs_add_block_group_cache(fs_info, bg);
		 
		if (ret == -EEXIST) {
			ret = 0;
			btrfs_put_block_group(bg);
			continue;
		}

		if (ret) {
			btrfs_remove_free_space_cache(bg);
			btrfs_put_block_group(bg);
			break;
		}

		btrfs_add_bg_to_space_info(fs_info, bg);

		set_avail_alloc_bits(fs_info, bg->flags);
	}
	if (!ret)
		btrfs_init_global_block_rsv(fs_info);
	return ret;
}

int btrfs_read_block_groups(struct btrfs_fs_info *info)
{
	struct btrfs_root *root = btrfs_block_group_root(info);
	struct btrfs_path *path;
	int ret;
	struct btrfs_block_group *cache;
	struct btrfs_space_info *space_info;
	struct btrfs_key key;
	int need_clear = 0;
	u64 cache_gen;

	 
	if (!root || (btrfs_super_compat_ro_flags(info->super_copy) &
		      ~BTRFS_FEATURE_COMPAT_RO_SUPP))
		return fill_dummy_bgs(info);

	key.objectid = 0;
	key.offset = 0;
	key.type = BTRFS_BLOCK_GROUP_ITEM_KEY;
	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	cache_gen = btrfs_super_cache_generation(info->super_copy);
	if (btrfs_test_opt(info, SPACE_CACHE) &&
	    btrfs_super_generation(info->super_copy) != cache_gen)
		need_clear = 1;
	if (btrfs_test_opt(info, CLEAR_CACHE))
		need_clear = 1;

	while (1) {
		struct btrfs_block_group_item bgi;
		struct extent_buffer *leaf;
		int slot;

		ret = find_first_block_group(info, path, &key);
		if (ret > 0)
			break;
		if (ret != 0)
			goto error;

		leaf = path->nodes[0];
		slot = path->slots[0];

		read_extent_buffer(leaf, &bgi, btrfs_item_ptr_offset(leaf, slot),
				   sizeof(bgi));

		btrfs_item_key_to_cpu(leaf, &key, slot);
		btrfs_release_path(path);
		ret = read_one_block_group(info, &bgi, &key, need_clear);
		if (ret < 0)
			goto error;
		key.objectid += key.offset;
		key.offset = 0;
	}
	btrfs_release_path(path);

	list_for_each_entry(space_info, &info->space_info, list) {
		int i;

		for (i = 0; i < BTRFS_NR_RAID_TYPES; i++) {
			if (list_empty(&space_info->block_groups[i]))
				continue;
			cache = list_first_entry(&space_info->block_groups[i],
						 struct btrfs_block_group,
						 list);
			btrfs_sysfs_add_block_group_type(cache);
		}

		if (!(btrfs_get_alloc_profile(info, space_info->flags) &
		      (BTRFS_BLOCK_GROUP_RAID10 |
		       BTRFS_BLOCK_GROUP_RAID1_MASK |
		       BTRFS_BLOCK_GROUP_RAID56_MASK |
		       BTRFS_BLOCK_GROUP_DUP)))
			continue;
		 
		list_for_each_entry(cache,
				&space_info->block_groups[BTRFS_RAID_RAID0],
				list)
			inc_block_group_ro(cache, 1);
		list_for_each_entry(cache,
				&space_info->block_groups[BTRFS_RAID_SINGLE],
				list)
			inc_block_group_ro(cache, 1);
	}

	btrfs_init_global_block_rsv(info);
	ret = check_chunk_block_group_mappings(info);
error:
	btrfs_free_path(path);
	 
	if (ret && btrfs_test_opt(info, IGNOREBADROOTS))
		ret = fill_dummy_bgs(info);
	return ret;
}

 
static int insert_block_group_item(struct btrfs_trans_handle *trans,
				   struct btrfs_block_group *block_group)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group_item bgi;
	struct btrfs_root *root = btrfs_block_group_root(fs_info);
	struct btrfs_key key;
	u64 old_commit_used;
	int ret;

	spin_lock(&block_group->lock);
	btrfs_set_stack_block_group_used(&bgi, block_group->used);
	btrfs_set_stack_block_group_chunk_objectid(&bgi,
						   block_group->global_root_id);
	btrfs_set_stack_block_group_flags(&bgi, block_group->flags);
	old_commit_used = block_group->commit_used;
	block_group->commit_used = block_group->used;
	key.objectid = block_group->start;
	key.type = BTRFS_BLOCK_GROUP_ITEM_KEY;
	key.offset = block_group->length;
	spin_unlock(&block_group->lock);

	ret = btrfs_insert_item(trans, root, &key, &bgi, sizeof(bgi));
	if (ret < 0) {
		spin_lock(&block_group->lock);
		block_group->commit_used = old_commit_used;
		spin_unlock(&block_group->lock);
	}

	return ret;
}

static int insert_dev_extent(struct btrfs_trans_handle *trans,
			    struct btrfs_device *device, u64 chunk_offset,
			    u64 start, u64 num_bytes)
{
	struct btrfs_fs_info *fs_info = device->fs_info;
	struct btrfs_root *root = fs_info->dev_root;
	struct btrfs_path *path;
	struct btrfs_dev_extent *extent;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	int ret;

	WARN_ON(!test_bit(BTRFS_DEV_STATE_IN_FS_METADATA, &device->dev_state));
	WARN_ON(test_bit(BTRFS_DEV_STATE_REPLACE_TGT, &device->dev_state));
	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = device->devid;
	key.type = BTRFS_DEV_EXTENT_KEY;
	key.offset = start;
	ret = btrfs_insert_empty_item(trans, root, path, &key, sizeof(*extent));
	if (ret)
		goto out;

	leaf = path->nodes[0];
	extent = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_dev_extent);
	btrfs_set_dev_extent_chunk_tree(leaf, extent, BTRFS_CHUNK_TREE_OBJECTID);
	btrfs_set_dev_extent_chunk_objectid(leaf, extent,
					    BTRFS_FIRST_CHUNK_TREE_OBJECTID);
	btrfs_set_dev_extent_chunk_offset(leaf, extent, chunk_offset);

	btrfs_set_dev_extent_length(leaf, extent, num_bytes);
	btrfs_mark_buffer_dirty(trans, leaf);
out:
	btrfs_free_path(path);
	return ret;
}

 
static int insert_dev_extents(struct btrfs_trans_handle *trans,
				   u64 chunk_offset, u64 chunk_size)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_device *device;
	struct extent_map *em;
	struct map_lookup *map;
	u64 dev_offset;
	u64 stripe_size;
	int i;
	int ret = 0;

	em = btrfs_get_chunk_map(fs_info, chunk_offset, chunk_size);
	if (IS_ERR(em))
		return PTR_ERR(em);

	map = em->map_lookup;
	stripe_size = em->orig_block_len;

	 
	mutex_lock(&fs_info->fs_devices->device_list_mutex);
	for (i = 0; i < map->num_stripes; i++) {
		device = map->stripes[i].dev;
		dev_offset = map->stripes[i].physical;

		ret = insert_dev_extent(trans, device, chunk_offset, dev_offset,
				       stripe_size);
		if (ret)
			break;
	}
	mutex_unlock(&fs_info->fs_devices->device_list_mutex);

	free_extent_map(em);
	return ret;
}

 
void btrfs_create_pending_block_groups(struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group *block_group;
	int ret = 0;

	while (!list_empty(&trans->new_bgs)) {
		int index;

		block_group = list_first_entry(&trans->new_bgs,
					       struct btrfs_block_group,
					       bg_list);
		if (ret)
			goto next;

		index = btrfs_bg_flags_to_raid_index(block_group->flags);

		ret = insert_block_group_item(trans, block_group);
		if (ret)
			btrfs_abort_transaction(trans, ret);
		if (!test_bit(BLOCK_GROUP_FLAG_CHUNK_ITEM_INSERTED,
			      &block_group->runtime_flags)) {
			mutex_lock(&fs_info->chunk_mutex);
			ret = btrfs_chunk_alloc_add_chunk_item(trans, block_group);
			mutex_unlock(&fs_info->chunk_mutex);
			if (ret)
				btrfs_abort_transaction(trans, ret);
		}
		ret = insert_dev_extents(trans, block_group->start,
					 block_group->length);
		if (ret)
			btrfs_abort_transaction(trans, ret);
		add_block_group_free_space(trans, block_group);

		 
		if (block_group->space_info->block_group_kobjs[index] == NULL)
			btrfs_sysfs_add_block_group_type(block_group);

		 
next:
		btrfs_delayed_refs_rsv_release(fs_info, 1);
		list_del_init(&block_group->bg_list);
		clear_bit(BLOCK_GROUP_FLAG_NEW, &block_group->runtime_flags);
	}
	btrfs_trans_release_chunk_metadata(trans);
}

 
static u64 calculate_global_root_id(struct btrfs_fs_info *fs_info, u64 offset)
{
	u64 div = SZ_1G;
	u64 index;

	if (!btrfs_fs_incompat(fs_info, EXTENT_TREE_V2))
		return BTRFS_FIRST_CHUNK_TREE_OBJECTID;

	 
	if (btrfs_super_total_bytes(fs_info->super_copy) <= (SZ_1G * 10ULL))
		div = SZ_128M;

	offset = div64_u64(offset, div);
	div64_u64_rem(offset, fs_info->nr_global_roots, &index);
	return index;
}

struct btrfs_block_group *btrfs_make_block_group(struct btrfs_trans_handle *trans,
						 u64 type,
						 u64 chunk_offset, u64 size)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group *cache;
	int ret;

	btrfs_set_log_full_commit(trans);

	cache = btrfs_create_block_group_cache(fs_info, chunk_offset);
	if (!cache)
		return ERR_PTR(-ENOMEM);

	 
	set_bit(BLOCK_GROUP_FLAG_NEW, &cache->runtime_flags);

	cache->length = size;
	set_free_space_tree_thresholds(cache);
	cache->flags = type;
	cache->cached = BTRFS_CACHE_FINISHED;
	cache->global_root_id = calculate_global_root_id(fs_info, cache->start);

	if (btrfs_fs_compat_ro(fs_info, FREE_SPACE_TREE))
		set_bit(BLOCK_GROUP_FLAG_NEEDS_FREE_SPACE, &cache->runtime_flags);

	ret = btrfs_load_block_group_zone_info(cache, true);
	if (ret) {
		btrfs_put_block_group(cache);
		return ERR_PTR(ret);
	}

	ret = exclude_super_stripes(cache);
	if (ret) {
		 
		btrfs_free_excluded_extents(cache);
		btrfs_put_block_group(cache);
		return ERR_PTR(ret);
	}

	ret = btrfs_add_new_free_space(cache, chunk_offset, chunk_offset + size, NULL);
	btrfs_free_excluded_extents(cache);
	if (ret) {
		btrfs_put_block_group(cache);
		return ERR_PTR(ret);
	}

	 
	cache->space_info = btrfs_find_space_info(fs_info, cache->flags);
	ASSERT(cache->space_info);

	ret = btrfs_add_block_group_cache(fs_info, cache);
	if (ret) {
		btrfs_remove_free_space_cache(cache);
		btrfs_put_block_group(cache);
		return ERR_PTR(ret);
	}

	 
	trace_btrfs_add_block_group(fs_info, cache, 1);
	btrfs_add_bg_to_space_info(fs_info, cache);
	btrfs_update_global_block_rsv(fs_info);

#ifdef CONFIG_BTRFS_DEBUG
	if (btrfs_should_fragment_free_space(cache)) {
		cache->space_info->bytes_used += size >> 1;
		fragment_free_space(cache);
	}
#endif

	list_add_tail(&cache->bg_list, &trans->new_bgs);
	trans->delayed_ref_updates++;
	btrfs_update_delayed_refs_rsv(trans);

	set_avail_alloc_bits(fs_info, type);
	return cache;
}

 
int btrfs_inc_block_group_ro(struct btrfs_block_group *cache,
			     bool do_chunk_alloc)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = btrfs_block_group_root(fs_info);
	u64 alloc_flags;
	int ret;
	bool dirty_bg_running;

	 
	if (sb_rdonly(fs_info->sb)) {
		mutex_lock(&fs_info->ro_block_group_mutex);
		ret = inc_block_group_ro(cache, 0);
		mutex_unlock(&fs_info->ro_block_group_mutex);
		return ret;
	}

	do {
		trans = btrfs_join_transaction(root);
		if (IS_ERR(trans))
			return PTR_ERR(trans);

		dirty_bg_running = false;

		 
		mutex_lock(&fs_info->ro_block_group_mutex);
		if (test_bit(BTRFS_TRANS_DIRTY_BG_RUN, &trans->transaction->flags)) {
			u64 transid = trans->transid;

			mutex_unlock(&fs_info->ro_block_group_mutex);
			btrfs_end_transaction(trans);

			ret = btrfs_wait_for_commit(fs_info, transid);
			if (ret)
				return ret;
			dirty_bg_running = true;
		}
	} while (dirty_bg_running);

	if (do_chunk_alloc) {
		 
		alloc_flags = btrfs_get_alloc_profile(fs_info, cache->flags);
		if (alloc_flags != cache->flags) {
			ret = btrfs_chunk_alloc(trans, alloc_flags,
						CHUNK_ALLOC_FORCE);
			 
			if (ret == -ENOSPC)
				ret = 0;
			if (ret < 0)
				goto out;
		}
	}

	ret = inc_block_group_ro(cache, 0);
	if (!ret)
		goto out;
	if (ret == -ETXTBSY)
		goto unlock_out;

	 
	if (!do_chunk_alloc && ret == -ENOSPC &&
	    (cache->flags & BTRFS_BLOCK_GROUP_SYSTEM))
		goto unlock_out;

	alloc_flags = btrfs_get_alloc_profile(fs_info, cache->space_info->flags);
	ret = btrfs_chunk_alloc(trans, alloc_flags, CHUNK_ALLOC_FORCE);
	if (ret < 0)
		goto out;
	 
	ret = btrfs_zoned_activate_one_bg(fs_info, cache->space_info, true);
	if (ret < 0)
		goto out;

	ret = inc_block_group_ro(cache, 0);
	if (ret == -ETXTBSY)
		goto unlock_out;
out:
	if (cache->flags & BTRFS_BLOCK_GROUP_SYSTEM) {
		alloc_flags = btrfs_get_alloc_profile(fs_info, cache->flags);
		mutex_lock(&fs_info->chunk_mutex);
		check_system_chunk(trans, alloc_flags);
		mutex_unlock(&fs_info->chunk_mutex);
	}
unlock_out:
	mutex_unlock(&fs_info->ro_block_group_mutex);

	btrfs_end_transaction(trans);
	return ret;
}

void btrfs_dec_block_group_ro(struct btrfs_block_group *cache)
{
	struct btrfs_space_info *sinfo = cache->space_info;
	u64 num_bytes;

	BUG_ON(!cache->ro);

	spin_lock(&sinfo->lock);
	spin_lock(&cache->lock);
	if (!--cache->ro) {
		if (btrfs_is_zoned(cache->fs_info)) {
			 
			cache->zone_unusable =
				(cache->alloc_offset - cache->used) +
				(cache->length - cache->zone_capacity);
			sinfo->bytes_zone_unusable += cache->zone_unusable;
			sinfo->bytes_readonly -= cache->zone_unusable;
		}
		num_bytes = cache->length - cache->reserved -
			    cache->pinned - cache->bytes_super -
			    cache->zone_unusable - cache->used;
		sinfo->bytes_readonly -= num_bytes;
		list_del_init(&cache->ro_list);
	}
	spin_unlock(&cache->lock);
	spin_unlock(&sinfo->lock);
}

static int update_block_group_item(struct btrfs_trans_handle *trans,
				   struct btrfs_path *path,
				   struct btrfs_block_group *cache)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int ret;
	struct btrfs_root *root = btrfs_block_group_root(fs_info);
	unsigned long bi;
	struct extent_buffer *leaf;
	struct btrfs_block_group_item bgi;
	struct btrfs_key key;
	u64 old_commit_used;
	u64 used;

	 
	spin_lock(&cache->lock);
	old_commit_used = cache->commit_used;
	used = cache->used;
	 
	if (cache->commit_used == used) {
		spin_unlock(&cache->lock);
		return 0;
	}
	cache->commit_used = used;
	spin_unlock(&cache->lock);

	key.objectid = cache->start;
	key.type = BTRFS_BLOCK_GROUP_ITEM_KEY;
	key.offset = cache->length;

	ret = btrfs_search_slot(trans, root, &key, path, 0, 1);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto fail;
	}

	leaf = path->nodes[0];
	bi = btrfs_item_ptr_offset(leaf, path->slots[0]);
	btrfs_set_stack_block_group_used(&bgi, used);
	btrfs_set_stack_block_group_chunk_objectid(&bgi,
						   cache->global_root_id);
	btrfs_set_stack_block_group_flags(&bgi, cache->flags);
	write_extent_buffer(leaf, &bgi, bi, sizeof(bgi));
	btrfs_mark_buffer_dirty(trans, leaf);
fail:
	btrfs_release_path(path);
	 
	if (ret < 0 && ret != -ENOENT) {
		spin_lock(&cache->lock);
		cache->commit_used = old_commit_used;
		spin_unlock(&cache->lock);
	}
	return ret;

}

static int cache_save_setup(struct btrfs_block_group *block_group,
			    struct btrfs_trans_handle *trans,
			    struct btrfs_path *path)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct btrfs_root *root = fs_info->tree_root;
	struct inode *inode = NULL;
	struct extent_changeset *data_reserved = NULL;
	u64 alloc_hint = 0;
	int dcs = BTRFS_DC_ERROR;
	u64 cache_size = 0;
	int retries = 0;
	int ret = 0;

	if (!btrfs_test_opt(fs_info, SPACE_CACHE))
		return 0;

	 
	if (block_group->length < (100 * SZ_1M)) {
		spin_lock(&block_group->lock);
		block_group->disk_cache_state = BTRFS_DC_WRITTEN;
		spin_unlock(&block_group->lock);
		return 0;
	}

	if (TRANS_ABORTED(trans))
		return 0;
again:
	inode = lookup_free_space_inode(block_group, path);
	if (IS_ERR(inode) && PTR_ERR(inode) != -ENOENT) {
		ret = PTR_ERR(inode);
		btrfs_release_path(path);
		goto out;
	}

	if (IS_ERR(inode)) {
		BUG_ON(retries);
		retries++;

		if (block_group->ro)
			goto out_free;

		ret = create_free_space_inode(trans, block_group, path);
		if (ret)
			goto out_free;
		goto again;
	}

	 
	BTRFS_I(inode)->generation = 0;
	ret = btrfs_update_inode(trans, root, BTRFS_I(inode));
	if (ret) {
		 
		btrfs_abort_transaction(trans, ret);
		goto out_put;
	}
	WARN_ON(ret);

	 
	if (block_group->cache_generation == trans->transid &&
	    i_size_read(inode)) {
		dcs = BTRFS_DC_SETUP;
		goto out_put;
	}

	if (i_size_read(inode) > 0) {
		ret = btrfs_check_trunc_cache_free_space(fs_info,
					&fs_info->global_block_rsv);
		if (ret)
			goto out_put;

		ret = btrfs_truncate_free_space_cache(trans, NULL, inode);
		if (ret)
			goto out_put;
	}

	spin_lock(&block_group->lock);
	if (block_group->cached != BTRFS_CACHE_FINISHED ||
	    !btrfs_test_opt(fs_info, SPACE_CACHE)) {
		 
		dcs = BTRFS_DC_WRITTEN;
		spin_unlock(&block_group->lock);
		goto out_put;
	}
	spin_unlock(&block_group->lock);

	 
	if (test_bit(BTRFS_TRANS_CACHE_ENOSPC, &trans->transaction->flags)) {
		ret = -ENOSPC;
		goto out_put;
	}

	 
	cache_size = div_u64(block_group->length, SZ_256M);
	if (!cache_size)
		cache_size = 1;

	cache_size *= 16;
	cache_size *= fs_info->sectorsize;

	ret = btrfs_check_data_free_space(BTRFS_I(inode), &data_reserved, 0,
					  cache_size, false);
	if (ret)
		goto out_put;

	ret = btrfs_prealloc_file_range_trans(inode, trans, 0, 0, cache_size,
					      cache_size, cache_size,
					      &alloc_hint);
	 
	if (!ret)
		dcs = BTRFS_DC_SETUP;
	else if (ret == -ENOSPC)
		set_bit(BTRFS_TRANS_CACHE_ENOSPC, &trans->transaction->flags);

out_put:
	iput(inode);
out_free:
	btrfs_release_path(path);
out:
	spin_lock(&block_group->lock);
	if (!ret && dcs == BTRFS_DC_SETUP)
		block_group->cache_generation = trans->transid;
	block_group->disk_cache_state = dcs;
	spin_unlock(&block_group->lock);

	extent_changeset_free(data_reserved);
	return ret;
}

int btrfs_setup_space_cache(struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group *cache, *tmp;
	struct btrfs_transaction *cur_trans = trans->transaction;
	struct btrfs_path *path;

	if (list_empty(&cur_trans->dirty_bgs) ||
	    !btrfs_test_opt(fs_info, SPACE_CACHE))
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	 
	list_for_each_entry_safe(cache, tmp, &cur_trans->dirty_bgs,
				 dirty_list) {
		if (cache->disk_cache_state == BTRFS_DC_CLEAR)
			cache_save_setup(cache, trans, path);
	}

	btrfs_free_path(path);
	return 0;
}

 
int btrfs_start_dirty_block_groups(struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group *cache;
	struct btrfs_transaction *cur_trans = trans->transaction;
	int ret = 0;
	int should_put;
	struct btrfs_path *path = NULL;
	LIST_HEAD(dirty);
	struct list_head *io = &cur_trans->io_bgs;
	int loops = 0;

	spin_lock(&cur_trans->dirty_bgs_lock);
	if (list_empty(&cur_trans->dirty_bgs)) {
		spin_unlock(&cur_trans->dirty_bgs_lock);
		return 0;
	}
	list_splice_init(&cur_trans->dirty_bgs, &dirty);
	spin_unlock(&cur_trans->dirty_bgs_lock);

again:
	 
	btrfs_create_pending_block_groups(trans);

	if (!path) {
		path = btrfs_alloc_path();
		if (!path) {
			ret = -ENOMEM;
			goto out;
		}
	}

	 
	mutex_lock(&trans->transaction->cache_write_mutex);
	while (!list_empty(&dirty)) {
		bool drop_reserve = true;

		cache = list_first_entry(&dirty, struct btrfs_block_group,
					 dirty_list);
		 
		if (!list_empty(&cache->io_list)) {
			list_del_init(&cache->io_list);
			btrfs_wait_cache_io(trans, cache, path);
			btrfs_put_block_group(cache);
		}


		 
		spin_lock(&cur_trans->dirty_bgs_lock);
		list_del_init(&cache->dirty_list);
		spin_unlock(&cur_trans->dirty_bgs_lock);

		should_put = 1;

		cache_save_setup(cache, trans, path);

		if (cache->disk_cache_state == BTRFS_DC_SETUP) {
			cache->io_ctl.inode = NULL;
			ret = btrfs_write_out_cache(trans, cache, path);
			if (ret == 0 && cache->io_ctl.inode) {
				should_put = 0;

				 
				list_add_tail(&cache->io_list, io);
			} else {
				 
				ret = 0;
			}
		}
		if (!ret) {
			ret = update_block_group_item(trans, path, cache);
			 
			if (ret == -ENOENT) {
				ret = 0;
				spin_lock(&cur_trans->dirty_bgs_lock);
				if (list_empty(&cache->dirty_list)) {
					list_add_tail(&cache->dirty_list,
						      &cur_trans->dirty_bgs);
					btrfs_get_block_group(cache);
					drop_reserve = false;
				}
				spin_unlock(&cur_trans->dirty_bgs_lock);
			} else if (ret) {
				btrfs_abort_transaction(trans, ret);
			}
		}

		 
		if (should_put)
			btrfs_put_block_group(cache);
		if (drop_reserve)
			btrfs_delayed_refs_rsv_release(fs_info, 1);
		 
		mutex_unlock(&trans->transaction->cache_write_mutex);
		if (ret)
			goto out;
		mutex_lock(&trans->transaction->cache_write_mutex);
	}
	mutex_unlock(&trans->transaction->cache_write_mutex);

	 
	if (!ret)
		ret = btrfs_run_delayed_refs(trans, 0);
	if (!ret && loops == 0) {
		loops++;
		spin_lock(&cur_trans->dirty_bgs_lock);
		list_splice_init(&cur_trans->dirty_bgs, &dirty);
		 
		if (!list_empty(&dirty)) {
			spin_unlock(&cur_trans->dirty_bgs_lock);
			goto again;
		}
		spin_unlock(&cur_trans->dirty_bgs_lock);
	}
out:
	if (ret < 0) {
		spin_lock(&cur_trans->dirty_bgs_lock);
		list_splice_init(&dirty, &cur_trans->dirty_bgs);
		spin_unlock(&cur_trans->dirty_bgs_lock);
		btrfs_cleanup_dirty_bgs(cur_trans, fs_info);
	}

	btrfs_free_path(path);
	return ret;
}

int btrfs_write_dirty_block_groups(struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group *cache;
	struct btrfs_transaction *cur_trans = trans->transaction;
	int ret = 0;
	int should_put;
	struct btrfs_path *path;
	struct list_head *io = &cur_trans->io_bgs;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	 
	spin_lock(&cur_trans->dirty_bgs_lock);
	while (!list_empty(&cur_trans->dirty_bgs)) {
		cache = list_first_entry(&cur_trans->dirty_bgs,
					 struct btrfs_block_group,
					 dirty_list);

		 
		if (!list_empty(&cache->io_list)) {
			spin_unlock(&cur_trans->dirty_bgs_lock);
			list_del_init(&cache->io_list);
			btrfs_wait_cache_io(trans, cache, path);
			btrfs_put_block_group(cache);
			spin_lock(&cur_trans->dirty_bgs_lock);
		}

		 
		list_del_init(&cache->dirty_list);
		spin_unlock(&cur_trans->dirty_bgs_lock);
		should_put = 1;

		cache_save_setup(cache, trans, path);

		if (!ret)
			ret = btrfs_run_delayed_refs(trans,
						     (unsigned long) -1);

		if (!ret && cache->disk_cache_state == BTRFS_DC_SETUP) {
			cache->io_ctl.inode = NULL;
			ret = btrfs_write_out_cache(trans, cache, path);
			if (ret == 0 && cache->io_ctl.inode) {
				should_put = 0;
				list_add_tail(&cache->io_list, io);
			} else {
				 
				ret = 0;
			}
		}
		if (!ret) {
			ret = update_block_group_item(trans, path, cache);
			 
			if (ret == -ENOENT) {
				wait_event(cur_trans->writer_wait,
				   atomic_read(&cur_trans->num_writers) == 1);
				ret = update_block_group_item(trans, path, cache);
			}
			if (ret)
				btrfs_abort_transaction(trans, ret);
		}

		 
		if (should_put)
			btrfs_put_block_group(cache);
		btrfs_delayed_refs_rsv_release(fs_info, 1);
		spin_lock(&cur_trans->dirty_bgs_lock);
	}
	spin_unlock(&cur_trans->dirty_bgs_lock);

	 
	while (!list_empty(io)) {
		cache = list_first_entry(io, struct btrfs_block_group,
					 io_list);
		list_del_init(&cache->io_list);
		btrfs_wait_cache_io(trans, cache, path);
		btrfs_put_block_group(cache);
	}

	btrfs_free_path(path);
	return ret;
}

int btrfs_update_block_group(struct btrfs_trans_handle *trans,
			     u64 bytenr, u64 num_bytes, bool alloc)
{
	struct btrfs_fs_info *info = trans->fs_info;
	struct btrfs_block_group *cache = NULL;
	u64 total = num_bytes;
	u64 old_val;
	u64 byte_in_group;
	int factor;
	int ret = 0;

	 
	spin_lock(&info->delalloc_root_lock);
	old_val = btrfs_super_bytes_used(info->super_copy);
	if (alloc)
		old_val += num_bytes;
	else
		old_val -= num_bytes;
	btrfs_set_super_bytes_used(info->super_copy, old_val);
	spin_unlock(&info->delalloc_root_lock);

	while (total) {
		struct btrfs_space_info *space_info;
		bool reclaim = false;

		cache = btrfs_lookup_block_group(info, bytenr);
		if (!cache) {
			ret = -ENOENT;
			break;
		}
		space_info = cache->space_info;
		factor = btrfs_bg_type_to_factor(cache->flags);

		 
		if (!alloc && !btrfs_block_group_done(cache))
			btrfs_cache_block_group(cache, true);

		byte_in_group = bytenr - cache->start;
		WARN_ON(byte_in_group > cache->length);

		spin_lock(&space_info->lock);
		spin_lock(&cache->lock);

		if (btrfs_test_opt(info, SPACE_CACHE) &&
		    cache->disk_cache_state < BTRFS_DC_CLEAR)
			cache->disk_cache_state = BTRFS_DC_CLEAR;

		old_val = cache->used;
		num_bytes = min(total, cache->length - byte_in_group);
		if (alloc) {
			old_val += num_bytes;
			cache->used = old_val;
			cache->reserved -= num_bytes;
			space_info->bytes_reserved -= num_bytes;
			space_info->bytes_used += num_bytes;
			space_info->disk_used += num_bytes * factor;
			spin_unlock(&cache->lock);
			spin_unlock(&space_info->lock);
		} else {
			old_val -= num_bytes;
			cache->used = old_val;
			cache->pinned += num_bytes;
			btrfs_space_info_update_bytes_pinned(info, space_info,
							     num_bytes);
			space_info->bytes_used -= num_bytes;
			space_info->disk_used -= num_bytes * factor;

			reclaim = should_reclaim_block_group(cache, num_bytes);

			spin_unlock(&cache->lock);
			spin_unlock(&space_info->lock);

			set_extent_bit(&trans->transaction->pinned_extents,
				       bytenr, bytenr + num_bytes - 1,
				       EXTENT_DIRTY, NULL);
		}

		spin_lock(&trans->transaction->dirty_bgs_lock);
		if (list_empty(&cache->dirty_list)) {
			list_add_tail(&cache->dirty_list,
				      &trans->transaction->dirty_bgs);
			trans->delayed_ref_updates++;
			btrfs_get_block_group(cache);
		}
		spin_unlock(&trans->transaction->dirty_bgs_lock);

		 
		if (!alloc && old_val == 0) {
			if (!btrfs_test_opt(info, DISCARD_ASYNC))
				btrfs_mark_bg_unused(cache);
		} else if (!alloc && reclaim) {
			btrfs_mark_bg_to_reclaim(cache);
		}

		btrfs_put_block_group(cache);
		total -= num_bytes;
		bytenr += num_bytes;
	}

	 
	btrfs_update_delayed_refs_rsv(trans);
	return ret;
}

 
int btrfs_add_reserved_bytes(struct btrfs_block_group *cache,
			     u64 ram_bytes, u64 num_bytes, int delalloc,
			     bool force_wrong_size_class)
{
	struct btrfs_space_info *space_info = cache->space_info;
	enum btrfs_block_group_size_class size_class;
	int ret = 0;

	spin_lock(&space_info->lock);
	spin_lock(&cache->lock);
	if (cache->ro) {
		ret = -EAGAIN;
		goto out;
	}

	if (btrfs_block_group_should_use_size_class(cache)) {
		size_class = btrfs_calc_block_group_size_class(num_bytes);
		ret = btrfs_use_block_group_size_class(cache, size_class, force_wrong_size_class);
		if (ret)
			goto out;
	}
	cache->reserved += num_bytes;
	space_info->bytes_reserved += num_bytes;
	trace_btrfs_space_reservation(cache->fs_info, "space_info",
				      space_info->flags, num_bytes, 1);
	btrfs_space_info_update_bytes_may_use(cache->fs_info,
					      space_info, -ram_bytes);
	if (delalloc)
		cache->delalloc_bytes += num_bytes;

	 
	if (num_bytes < ram_bytes)
		btrfs_try_granting_tickets(cache->fs_info, space_info);
out:
	spin_unlock(&cache->lock);
	spin_unlock(&space_info->lock);
	return ret;
}

 
void btrfs_free_reserved_bytes(struct btrfs_block_group *cache,
			       u64 num_bytes, int delalloc)
{
	struct btrfs_space_info *space_info = cache->space_info;

	spin_lock(&space_info->lock);
	spin_lock(&cache->lock);
	if (cache->ro)
		space_info->bytes_readonly += num_bytes;
	cache->reserved -= num_bytes;
	space_info->bytes_reserved -= num_bytes;
	space_info->max_extent_size = 0;

	if (delalloc)
		cache->delalloc_bytes -= num_bytes;
	spin_unlock(&cache->lock);

	btrfs_try_granting_tickets(cache->fs_info, space_info);
	spin_unlock(&space_info->lock);
}

static void force_metadata_allocation(struct btrfs_fs_info *info)
{
	struct list_head *head = &info->space_info;
	struct btrfs_space_info *found;

	list_for_each_entry(found, head, list) {
		if (found->flags & BTRFS_BLOCK_GROUP_METADATA)
			found->force_alloc = CHUNK_ALLOC_FORCE;
	}
}

static int should_alloc_chunk(struct btrfs_fs_info *fs_info,
			      struct btrfs_space_info *sinfo, int force)
{
	u64 bytes_used = btrfs_space_info_used(sinfo, false);
	u64 thresh;

	if (force == CHUNK_ALLOC_FORCE)
		return 1;

	 
	if (force == CHUNK_ALLOC_LIMITED) {
		thresh = btrfs_super_total_bytes(fs_info->super_copy);
		thresh = max_t(u64, SZ_64M, mult_perc(thresh, 1));

		if (sinfo->total_bytes - bytes_used < thresh)
			return 1;
	}

	if (bytes_used + SZ_2M < mult_perc(sinfo->total_bytes, 80))
		return 0;
	return 1;
}

int btrfs_force_chunk_alloc(struct btrfs_trans_handle *trans, u64 type)
{
	u64 alloc_flags = btrfs_get_alloc_profile(trans->fs_info, type);

	return btrfs_chunk_alloc(trans, alloc_flags, CHUNK_ALLOC_FORCE);
}

static struct btrfs_block_group *do_chunk_alloc(struct btrfs_trans_handle *trans, u64 flags)
{
	struct btrfs_block_group *bg;
	int ret;

	 
	check_system_chunk(trans, flags);

	bg = btrfs_create_chunk(trans, flags);
	if (IS_ERR(bg)) {
		ret = PTR_ERR(bg);
		goto out;
	}

	ret = btrfs_chunk_alloc_add_chunk_item(trans, bg);
	 
	if (ret == -ENOSPC) {
		const u64 sys_flags = btrfs_system_alloc_profile(trans->fs_info);
		struct btrfs_block_group *sys_bg;

		sys_bg = btrfs_create_chunk(trans, sys_flags);
		if (IS_ERR(sys_bg)) {
			ret = PTR_ERR(sys_bg);
			btrfs_abort_transaction(trans, ret);
			goto out;
		}

		ret = btrfs_chunk_alloc_add_chunk_item(trans, sys_bg);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out;
		}

		ret = btrfs_chunk_alloc_add_chunk_item(trans, bg);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out;
		}
	} else if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
out:
	btrfs_trans_release_chunk_metadata(trans);

	if (ret)
		return ERR_PTR(ret);

	btrfs_get_block_group(bg);
	return bg;
}

 
int btrfs_chunk_alloc(struct btrfs_trans_handle *trans, u64 flags,
		      enum btrfs_chunk_alloc_enum force)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_space_info *space_info;
	struct btrfs_block_group *ret_bg;
	bool wait_for_alloc = false;
	bool should_alloc = false;
	bool from_extent_allocation = false;
	int ret = 0;

	if (force == CHUNK_ALLOC_FORCE_FOR_EXTENT) {
		from_extent_allocation = true;
		force = CHUNK_ALLOC_FORCE;
	}

	 
	if (trans->allocating_chunk)
		return -ENOSPC;
	 
	if (flags & BTRFS_BLOCK_GROUP_SYSTEM)
		return -ENOSPC;

	space_info = btrfs_find_space_info(fs_info, flags);
	ASSERT(space_info);

	do {
		spin_lock(&space_info->lock);
		if (force < space_info->force_alloc)
			force = space_info->force_alloc;
		should_alloc = should_alloc_chunk(fs_info, space_info, force);
		if (space_info->full) {
			 
			if (should_alloc)
				ret = -ENOSPC;
			else
				ret = 0;
			spin_unlock(&space_info->lock);
			return ret;
		} else if (!should_alloc) {
			spin_unlock(&space_info->lock);
			return 0;
		} else if (space_info->chunk_alloc) {
			 
			wait_for_alloc = true;
			force = CHUNK_ALLOC_NO_FORCE;
			spin_unlock(&space_info->lock);
			mutex_lock(&fs_info->chunk_mutex);
			mutex_unlock(&fs_info->chunk_mutex);
		} else {
			 
			space_info->chunk_alloc = 1;
			wait_for_alloc = false;
			spin_unlock(&space_info->lock);
		}

		cond_resched();
	} while (wait_for_alloc);

	mutex_lock(&fs_info->chunk_mutex);
	trans->allocating_chunk = true;

	 
	if (btrfs_mixed_space_info(space_info))
		flags |= (BTRFS_BLOCK_GROUP_DATA | BTRFS_BLOCK_GROUP_METADATA);

	 
	if (flags & BTRFS_BLOCK_GROUP_DATA && fs_info->metadata_ratio) {
		fs_info->data_chunk_allocations++;
		if (!(fs_info->data_chunk_allocations %
		      fs_info->metadata_ratio))
			force_metadata_allocation(fs_info);
	}

	ret_bg = do_chunk_alloc(trans, flags);
	trans->allocating_chunk = false;

	if (IS_ERR(ret_bg)) {
		ret = PTR_ERR(ret_bg);
	} else if (from_extent_allocation && (flags & BTRFS_BLOCK_GROUP_DATA)) {
		 
		btrfs_zone_activate(ret_bg);
	}

	if (!ret)
		btrfs_put_block_group(ret_bg);

	spin_lock(&space_info->lock);
	if (ret < 0) {
		if (ret == -ENOSPC)
			space_info->full = 1;
		else
			goto out;
	} else {
		ret = 1;
		space_info->max_extent_size = 0;
	}

	space_info->force_alloc = CHUNK_ALLOC_NO_FORCE;
out:
	space_info->chunk_alloc = 0;
	spin_unlock(&space_info->lock);
	mutex_unlock(&fs_info->chunk_mutex);

	return ret;
}

static u64 get_profile_num_devs(struct btrfs_fs_info *fs_info, u64 type)
{
	u64 num_dev;

	num_dev = btrfs_raid_array[btrfs_bg_flags_to_raid_index(type)].devs_max;
	if (!num_dev)
		num_dev = fs_info->fs_devices->rw_devices;

	return num_dev;
}

static void reserve_chunk_space(struct btrfs_trans_handle *trans,
				u64 bytes,
				u64 type)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_space_info *info;
	u64 left;
	int ret = 0;

	 
	lockdep_assert_held(&fs_info->chunk_mutex);

	info = btrfs_find_space_info(fs_info, BTRFS_BLOCK_GROUP_SYSTEM);
	spin_lock(&info->lock);
	left = info->total_bytes - btrfs_space_info_used(info, true);
	spin_unlock(&info->lock);

	if (left < bytes && btrfs_test_opt(fs_info, ENOSPC_DEBUG)) {
		btrfs_info(fs_info, "left=%llu, need=%llu, flags=%llu",
			   left, bytes, type);
		btrfs_dump_space_info(fs_info, info, 0, 0);
	}

	if (left < bytes) {
		u64 flags = btrfs_system_alloc_profile(fs_info);
		struct btrfs_block_group *bg;

		 
		bg = btrfs_create_chunk(trans, flags);
		if (IS_ERR(bg)) {
			ret = PTR_ERR(bg);
		} else {
			 
			ret = btrfs_zoned_activate_one_bg(fs_info, info, true);
			if (ret < 0)
				return;

			 
			btrfs_chunk_alloc_add_chunk_item(trans, bg);
		}
	}

	if (!ret) {
		ret = btrfs_block_rsv_add(fs_info,
					  &fs_info->chunk_block_rsv,
					  bytes, BTRFS_RESERVE_NO_FLUSH);
		if (!ret)
			trans->chunk_bytes_reserved += bytes;
	}
}

 
void check_system_chunk(struct btrfs_trans_handle *trans, u64 type)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	const u64 num_devs = get_profile_num_devs(fs_info, type);
	u64 bytes;

	 
	bytes = btrfs_calc_metadata_size(fs_info, num_devs) +
		btrfs_calc_insert_metadata_size(fs_info, 1);

	reserve_chunk_space(trans, bytes, type);
}

 
void btrfs_reserve_chunk_metadata(struct btrfs_trans_handle *trans,
				  bool is_item_insertion)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	u64 bytes;

	if (is_item_insertion)
		bytes = btrfs_calc_insert_metadata_size(fs_info, 1);
	else
		bytes = btrfs_calc_metadata_size(fs_info, 1);

	mutex_lock(&fs_info->chunk_mutex);
	reserve_chunk_space(trans, bytes, BTRFS_BLOCK_GROUP_SYSTEM);
	mutex_unlock(&fs_info->chunk_mutex);
}

void btrfs_put_block_group_cache(struct btrfs_fs_info *info)
{
	struct btrfs_block_group *block_group;

	block_group = btrfs_lookup_first_block_group(info, 0);
	while (block_group) {
		btrfs_wait_block_group_cache_done(block_group);
		spin_lock(&block_group->lock);
		if (test_and_clear_bit(BLOCK_GROUP_FLAG_IREF,
				       &block_group->runtime_flags)) {
			struct inode *inode = block_group->inode;

			block_group->inode = NULL;
			spin_unlock(&block_group->lock);

			ASSERT(block_group->io_ctl.inode == NULL);
			iput(inode);
		} else {
			spin_unlock(&block_group->lock);
		}
		block_group = btrfs_next_block_group(block_group);
	}
}

 
int btrfs_free_block_groups(struct btrfs_fs_info *info)
{
	struct btrfs_block_group *block_group;
	struct btrfs_space_info *space_info;
	struct btrfs_caching_control *caching_ctl;
	struct rb_node *n;

	if (btrfs_is_zoned(info)) {
		if (info->active_meta_bg) {
			btrfs_put_block_group(info->active_meta_bg);
			info->active_meta_bg = NULL;
		}
		if (info->active_system_bg) {
			btrfs_put_block_group(info->active_system_bg);
			info->active_system_bg = NULL;
		}
	}

	write_lock(&info->block_group_cache_lock);
	while (!list_empty(&info->caching_block_groups)) {
		caching_ctl = list_entry(info->caching_block_groups.next,
					 struct btrfs_caching_control, list);
		list_del(&caching_ctl->list);
		btrfs_put_caching_control(caching_ctl);
	}
	write_unlock(&info->block_group_cache_lock);

	spin_lock(&info->unused_bgs_lock);
	while (!list_empty(&info->unused_bgs)) {
		block_group = list_first_entry(&info->unused_bgs,
					       struct btrfs_block_group,
					       bg_list);
		list_del_init(&block_group->bg_list);
		btrfs_put_block_group(block_group);
	}

	while (!list_empty(&info->reclaim_bgs)) {
		block_group = list_first_entry(&info->reclaim_bgs,
					       struct btrfs_block_group,
					       bg_list);
		list_del_init(&block_group->bg_list);
		btrfs_put_block_group(block_group);
	}
	spin_unlock(&info->unused_bgs_lock);

	spin_lock(&info->zone_active_bgs_lock);
	while (!list_empty(&info->zone_active_bgs)) {
		block_group = list_first_entry(&info->zone_active_bgs,
					       struct btrfs_block_group,
					       active_bg_list);
		list_del_init(&block_group->active_bg_list);
		btrfs_put_block_group(block_group);
	}
	spin_unlock(&info->zone_active_bgs_lock);

	write_lock(&info->block_group_cache_lock);
	while ((n = rb_last(&info->block_group_cache_tree.rb_root)) != NULL) {
		block_group = rb_entry(n, struct btrfs_block_group,
				       cache_node);
		rb_erase_cached(&block_group->cache_node,
				&info->block_group_cache_tree);
		RB_CLEAR_NODE(&block_group->cache_node);
		write_unlock(&info->block_group_cache_lock);

		down_write(&block_group->space_info->groups_sem);
		list_del(&block_group->list);
		up_write(&block_group->space_info->groups_sem);

		 
		if (block_group->cached == BTRFS_CACHE_NO ||
		    block_group->cached == BTRFS_CACHE_ERROR)
			btrfs_free_excluded_extents(block_group);

		btrfs_remove_free_space_cache(block_group);
		ASSERT(block_group->cached != BTRFS_CACHE_STARTED);
		ASSERT(list_empty(&block_group->dirty_list));
		ASSERT(list_empty(&block_group->io_list));
		ASSERT(list_empty(&block_group->bg_list));
		ASSERT(refcount_read(&block_group->refs) == 1);
		ASSERT(block_group->swap_extents == 0);
		btrfs_put_block_group(block_group);

		write_lock(&info->block_group_cache_lock);
	}
	write_unlock(&info->block_group_cache_lock);

	btrfs_release_global_block_rsv(info);

	while (!list_empty(&info->space_info)) {
		space_info = list_entry(info->space_info.next,
					struct btrfs_space_info,
					list);

		 
		if (WARN_ON(space_info->bytes_pinned > 0 ||
			    space_info->bytes_may_use > 0))
			btrfs_dump_space_info(info, space_info, 0, 0);

		 
		if (!(space_info->flags & BTRFS_BLOCK_GROUP_METADATA) ||
		    !BTRFS_FS_LOG_CLEANUP_ERROR(info)) {
			if (WARN_ON(space_info->bytes_reserved > 0))
				btrfs_dump_space_info(info, space_info, 0, 0);
		}

		WARN_ON(space_info->reclaim_size > 0);
		list_del(&space_info->list);
		btrfs_sysfs_remove_space_info(space_info);
	}
	return 0;
}

void btrfs_freeze_block_group(struct btrfs_block_group *cache)
{
	atomic_inc(&cache->frozen);
}

void btrfs_unfreeze_block_group(struct btrfs_block_group *block_group)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct extent_map_tree *em_tree;
	struct extent_map *em;
	bool cleanup;

	spin_lock(&block_group->lock);
	cleanup = (atomic_dec_and_test(&block_group->frozen) &&
		   test_bit(BLOCK_GROUP_FLAG_REMOVED, &block_group->runtime_flags));
	spin_unlock(&block_group->lock);

	if (cleanup) {
		em_tree = &fs_info->mapping_tree;
		write_lock(&em_tree->lock);
		em = lookup_extent_mapping(em_tree, block_group->start,
					   1);
		BUG_ON(!em);  
		remove_extent_mapping(em_tree, em);
		write_unlock(&em_tree->lock);

		 
		free_extent_map(em);
		free_extent_map(em);

		 
		btrfs_remove_free_space_cache(block_group);
	}
}

bool btrfs_inc_block_group_swap_extents(struct btrfs_block_group *bg)
{
	bool ret = true;

	spin_lock(&bg->lock);
	if (bg->ro)
		ret = false;
	else
		bg->swap_extents++;
	spin_unlock(&bg->lock);

	return ret;
}

void btrfs_dec_block_group_swap_extents(struct btrfs_block_group *bg, int amount)
{
	spin_lock(&bg->lock);
	ASSERT(!bg->ro);
	ASSERT(bg->swap_extents >= amount);
	bg->swap_extents -= amount;
	spin_unlock(&bg->lock);
}

enum btrfs_block_group_size_class btrfs_calc_block_group_size_class(u64 size)
{
	if (size <= SZ_128K)
		return BTRFS_BG_SZ_SMALL;
	if (size <= SZ_8M)
		return BTRFS_BG_SZ_MEDIUM;
	return BTRFS_BG_SZ_LARGE;
}

 
int btrfs_use_block_group_size_class(struct btrfs_block_group *bg,
				     enum btrfs_block_group_size_class size_class,
				     bool force_wrong_size_class)
{
	ASSERT(size_class != BTRFS_BG_SZ_NONE);

	 
	if (bg->size_class == size_class)
		return 0;
	 
	if (bg->size_class != BTRFS_BG_SZ_NONE) {
		if (force_wrong_size_class)
			return 0;
		return -EAGAIN;
	}
	 
	bg->size_class = size_class;

	return 0;
}

bool btrfs_block_group_should_use_size_class(struct btrfs_block_group *bg)
{
	if (btrfs_is_zoned(bg->fs_info))
		return false;
	if (!btrfs_is_block_group_data_only(bg))
		return false;
	return true;
}



#include "misc.h"
#include "ctree.h"
#include "block-rsv.h"
#include "space-info.h"
#include "transaction.h"
#include "block-group.h"
#include "disk-io.h"
#include "fs.h"
#include "accessors.h"

 

static u64 block_rsv_release_bytes(struct btrfs_fs_info *fs_info,
				    struct btrfs_block_rsv *block_rsv,
				    struct btrfs_block_rsv *dest, u64 num_bytes,
				    u64 *qgroup_to_release_ret)
{
	struct btrfs_space_info *space_info = block_rsv->space_info;
	u64 qgroup_to_release = 0;
	u64 ret;

	spin_lock(&block_rsv->lock);
	if (num_bytes == (u64)-1) {
		num_bytes = block_rsv->size;
		qgroup_to_release = block_rsv->qgroup_rsv_size;
	}
	block_rsv->size -= num_bytes;
	if (block_rsv->reserved >= block_rsv->size) {
		num_bytes = block_rsv->reserved - block_rsv->size;
		block_rsv->reserved = block_rsv->size;
		block_rsv->full = true;
	} else {
		num_bytes = 0;
	}
	if (qgroup_to_release_ret &&
	    block_rsv->qgroup_rsv_reserved >= block_rsv->qgroup_rsv_size) {
		qgroup_to_release = block_rsv->qgroup_rsv_reserved -
				    block_rsv->qgroup_rsv_size;
		block_rsv->qgroup_rsv_reserved = block_rsv->qgroup_rsv_size;
	} else {
		qgroup_to_release = 0;
	}
	spin_unlock(&block_rsv->lock);

	ret = num_bytes;
	if (num_bytes > 0) {
		if (dest) {
			spin_lock(&dest->lock);
			if (!dest->full) {
				u64 bytes_to_add;

				bytes_to_add = dest->size - dest->reserved;
				bytes_to_add = min(num_bytes, bytes_to_add);
				dest->reserved += bytes_to_add;
				if (dest->reserved >= dest->size)
					dest->full = true;
				num_bytes -= bytes_to_add;
			}
			spin_unlock(&dest->lock);
		}
		if (num_bytes)
			btrfs_space_info_free_bytes_may_use(fs_info,
							    space_info,
							    num_bytes);
	}
	if (qgroup_to_release_ret)
		*qgroup_to_release_ret = qgroup_to_release;
	return ret;
}

int btrfs_block_rsv_migrate(struct btrfs_block_rsv *src,
			    struct btrfs_block_rsv *dst, u64 num_bytes,
			    bool update_size)
{
	int ret;

	ret = btrfs_block_rsv_use_bytes(src, num_bytes);
	if (ret)
		return ret;

	btrfs_block_rsv_add_bytes(dst, num_bytes, update_size);
	return 0;
}

void btrfs_init_block_rsv(struct btrfs_block_rsv *rsv, enum btrfs_rsv_type type)
{
	memset(rsv, 0, sizeof(*rsv));
	spin_lock_init(&rsv->lock);
	rsv->type = type;
}

void btrfs_init_metadata_block_rsv(struct btrfs_fs_info *fs_info,
				   struct btrfs_block_rsv *rsv,
				   enum btrfs_rsv_type type)
{
	btrfs_init_block_rsv(rsv, type);
	rsv->space_info = btrfs_find_space_info(fs_info,
					    BTRFS_BLOCK_GROUP_METADATA);
}

struct btrfs_block_rsv *btrfs_alloc_block_rsv(struct btrfs_fs_info *fs_info,
					      enum btrfs_rsv_type type)
{
	struct btrfs_block_rsv *block_rsv;

	block_rsv = kmalloc(sizeof(*block_rsv), GFP_NOFS);
	if (!block_rsv)
		return NULL;

	btrfs_init_metadata_block_rsv(fs_info, block_rsv, type);
	return block_rsv;
}

void btrfs_free_block_rsv(struct btrfs_fs_info *fs_info,
			  struct btrfs_block_rsv *rsv)
{
	if (!rsv)
		return;
	btrfs_block_rsv_release(fs_info, rsv, (u64)-1, NULL);
	kfree(rsv);
}

int btrfs_block_rsv_add(struct btrfs_fs_info *fs_info,
			struct btrfs_block_rsv *block_rsv, u64 num_bytes,
			enum btrfs_reserve_flush_enum flush)
{
	int ret;

	if (num_bytes == 0)
		return 0;

	ret = btrfs_reserve_metadata_bytes(fs_info, block_rsv, num_bytes, flush);
	if (!ret)
		btrfs_block_rsv_add_bytes(block_rsv, num_bytes, true);

	return ret;
}

int btrfs_block_rsv_check(struct btrfs_block_rsv *block_rsv, int min_percent)
{
	u64 num_bytes = 0;
	int ret = -ENOSPC;

	spin_lock(&block_rsv->lock);
	num_bytes = mult_perc(block_rsv->size, min_percent);
	if (block_rsv->reserved >= num_bytes)
		ret = 0;
	spin_unlock(&block_rsv->lock);

	return ret;
}

int btrfs_block_rsv_refill(struct btrfs_fs_info *fs_info,
			   struct btrfs_block_rsv *block_rsv, u64 num_bytes,
			   enum btrfs_reserve_flush_enum flush)
{
	int ret = -ENOSPC;

	if (!block_rsv)
		return 0;

	spin_lock(&block_rsv->lock);
	if (block_rsv->reserved >= num_bytes)
		ret = 0;
	else
		num_bytes -= block_rsv->reserved;
	spin_unlock(&block_rsv->lock);

	if (!ret)
		return 0;

	ret = btrfs_reserve_metadata_bytes(fs_info, block_rsv, num_bytes, flush);
	if (!ret) {
		btrfs_block_rsv_add_bytes(block_rsv, num_bytes, false);
		return 0;
	}

	return ret;
}

u64 btrfs_block_rsv_release(struct btrfs_fs_info *fs_info,
			    struct btrfs_block_rsv *block_rsv, u64 num_bytes,
			    u64 *qgroup_to_release)
{
	struct btrfs_block_rsv *global_rsv = &fs_info->global_block_rsv;
	struct btrfs_block_rsv *delayed_rsv = &fs_info->delayed_refs_rsv;
	struct btrfs_block_rsv *target = NULL;

	 
	if (block_rsv == delayed_rsv)
		target = global_rsv;
	else if (block_rsv != global_rsv && !btrfs_block_rsv_full(delayed_rsv))
		target = delayed_rsv;

	if (target && block_rsv->space_info != target->space_info)
		target = NULL;

	return block_rsv_release_bytes(fs_info, block_rsv, target, num_bytes,
				       qgroup_to_release);
}

int btrfs_block_rsv_use_bytes(struct btrfs_block_rsv *block_rsv, u64 num_bytes)
{
	int ret = -ENOSPC;

	spin_lock(&block_rsv->lock);
	if (block_rsv->reserved >= num_bytes) {
		block_rsv->reserved -= num_bytes;
		if (block_rsv->reserved < block_rsv->size)
			block_rsv->full = false;
		ret = 0;
	}
	spin_unlock(&block_rsv->lock);
	return ret;
}

void btrfs_block_rsv_add_bytes(struct btrfs_block_rsv *block_rsv,
			       u64 num_bytes, bool update_size)
{
	spin_lock(&block_rsv->lock);
	block_rsv->reserved += num_bytes;
	if (update_size)
		block_rsv->size += num_bytes;
	else if (block_rsv->reserved >= block_rsv->size)
		block_rsv->full = true;
	spin_unlock(&block_rsv->lock);
}

void btrfs_update_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	struct btrfs_block_rsv *block_rsv = &fs_info->global_block_rsv;
	struct btrfs_space_info *sinfo = block_rsv->space_info;
	struct btrfs_root *root, *tmp;
	u64 num_bytes = btrfs_root_used(&fs_info->tree_root->root_item);
	unsigned int min_items = 1;

	 
	read_lock(&fs_info->global_root_lock);
	rbtree_postorder_for_each_entry_safe(root, tmp, &fs_info->global_root_tree,
					     rb_node) {
		if (root->root_key.objectid == BTRFS_EXTENT_TREE_OBJECTID ||
		    root->root_key.objectid == BTRFS_CSUM_TREE_OBJECTID ||
		    root->root_key.objectid == BTRFS_FREE_SPACE_TREE_OBJECTID) {
			num_bytes += btrfs_root_used(&root->root_item);
			min_items++;
		}
	}
	read_unlock(&fs_info->global_root_lock);

	if (btrfs_fs_compat_ro(fs_info, BLOCK_GROUP_TREE)) {
		num_bytes += btrfs_root_used(&fs_info->block_group_root->root_item);
		min_items++;
	}

	 
	min_items += BTRFS_UNLINK_METADATA_UNITS;

	num_bytes = max_t(u64, num_bytes,
			  btrfs_calc_insert_metadata_size(fs_info, min_items) +
			  btrfs_calc_delayed_ref_bytes(fs_info,
					       BTRFS_UNLINK_METADATA_UNITS));

	spin_lock(&sinfo->lock);
	spin_lock(&block_rsv->lock);

	block_rsv->size = min_t(u64, num_bytes, SZ_512M);

	if (block_rsv->reserved < block_rsv->size) {
		num_bytes = block_rsv->size - block_rsv->reserved;
		btrfs_space_info_update_bytes_may_use(fs_info, sinfo,
						      num_bytes);
		block_rsv->reserved = block_rsv->size;
	} else if (block_rsv->reserved > block_rsv->size) {
		num_bytes = block_rsv->reserved - block_rsv->size;
		btrfs_space_info_update_bytes_may_use(fs_info, sinfo,
						      -num_bytes);
		block_rsv->reserved = block_rsv->size;
		btrfs_try_granting_tickets(fs_info, sinfo);
	}

	block_rsv->full = (block_rsv->reserved == block_rsv->size);

	if (block_rsv->size >= sinfo->total_bytes)
		sinfo->force_alloc = CHUNK_ALLOC_FORCE;
	spin_unlock(&block_rsv->lock);
	spin_unlock(&sinfo->lock);
}

void btrfs_init_root_block_rsv(struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;

	switch (root->root_key.objectid) {
	case BTRFS_CSUM_TREE_OBJECTID:
	case BTRFS_EXTENT_TREE_OBJECTID:
	case BTRFS_FREE_SPACE_TREE_OBJECTID:
	case BTRFS_BLOCK_GROUP_TREE_OBJECTID:
		root->block_rsv = &fs_info->delayed_refs_rsv;
		break;
	case BTRFS_ROOT_TREE_OBJECTID:
	case BTRFS_DEV_TREE_OBJECTID:
	case BTRFS_QUOTA_TREE_OBJECTID:
		root->block_rsv = &fs_info->global_block_rsv;
		break;
	case BTRFS_CHUNK_TREE_OBJECTID:
		root->block_rsv = &fs_info->chunk_block_rsv;
		break;
	default:
		root->block_rsv = NULL;
		break;
	}
}

void btrfs_init_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	struct btrfs_space_info *space_info;

	space_info = btrfs_find_space_info(fs_info, BTRFS_BLOCK_GROUP_SYSTEM);
	fs_info->chunk_block_rsv.space_info = space_info;

	space_info = btrfs_find_space_info(fs_info, BTRFS_BLOCK_GROUP_METADATA);
	fs_info->global_block_rsv.space_info = space_info;
	fs_info->trans_block_rsv.space_info = space_info;
	fs_info->empty_block_rsv.space_info = space_info;
	fs_info->delayed_block_rsv.space_info = space_info;
	fs_info->delayed_refs_rsv.space_info = space_info;

	btrfs_update_global_block_rsv(fs_info);
}

void btrfs_release_global_block_rsv(struct btrfs_fs_info *fs_info)
{
	btrfs_block_rsv_release(fs_info, &fs_info->global_block_rsv, (u64)-1,
				NULL);
	WARN_ON(fs_info->trans_block_rsv.size > 0);
	WARN_ON(fs_info->trans_block_rsv.reserved > 0);
	WARN_ON(fs_info->chunk_block_rsv.size > 0);
	WARN_ON(fs_info->chunk_block_rsv.reserved > 0);
	WARN_ON(fs_info->delayed_block_rsv.size > 0);
	WARN_ON(fs_info->delayed_block_rsv.reserved > 0);
	WARN_ON(fs_info->delayed_refs_rsv.reserved > 0);
	WARN_ON(fs_info->delayed_refs_rsv.size > 0);
}

static struct btrfs_block_rsv *get_block_rsv(
					const struct btrfs_trans_handle *trans,
					const struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_rsv *block_rsv = NULL;

	if (test_bit(BTRFS_ROOT_SHAREABLE, &root->state) ||
	    (root == fs_info->uuid_root) ||
	    (trans->adding_csums &&
	     root->root_key.objectid == BTRFS_CSUM_TREE_OBJECTID))
		block_rsv = trans->block_rsv;

	if (!block_rsv)
		block_rsv = root->block_rsv;

	if (!block_rsv)
		block_rsv = &fs_info->empty_block_rsv;

	return block_rsv;
}

struct btrfs_block_rsv *btrfs_use_block_rsv(struct btrfs_trans_handle *trans,
					    struct btrfs_root *root,
					    u32 blocksize)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_rsv *block_rsv;
	struct btrfs_block_rsv *global_rsv = &fs_info->global_block_rsv;
	int ret;
	bool global_updated = false;

	block_rsv = get_block_rsv(trans, root);

	if (unlikely(block_rsv->size == 0))
		goto try_reserve;
again:
	ret = btrfs_block_rsv_use_bytes(block_rsv, blocksize);
	if (!ret)
		return block_rsv;

	if (block_rsv->failfast)
		return ERR_PTR(ret);

	if (block_rsv->type == BTRFS_BLOCK_RSV_GLOBAL && !global_updated) {
		global_updated = true;
		btrfs_update_global_block_rsv(fs_info);
		goto again;
	}

	 
	if (block_rsv->type != BTRFS_BLOCK_RSV_DELREFS &&
	    btrfs_test_opt(fs_info, ENOSPC_DEBUG)) {
		static DEFINE_RATELIMIT_STATE(_rs,
				DEFAULT_RATELIMIT_INTERVAL * 10,
				  1);
		if (__ratelimit(&_rs))
			WARN(1, KERN_DEBUG
				"BTRFS: block rsv %d returned %d\n",
				block_rsv->type, ret);
	}
try_reserve:
	ret = btrfs_reserve_metadata_bytes(fs_info, block_rsv, blocksize,
					   BTRFS_RESERVE_NO_FLUSH);
	if (!ret)
		return block_rsv;
	 
	if (block_rsv->type != BTRFS_BLOCK_RSV_GLOBAL &&
	    block_rsv->space_info == global_rsv->space_info) {
		ret = btrfs_block_rsv_use_bytes(global_rsv, blocksize);
		if (!ret)
			return global_rsv;
	}

	 
	ret = btrfs_reserve_metadata_bytes(fs_info, block_rsv, blocksize,
					   BTRFS_RESERVE_FLUSH_EMERGENCY);
	if (!ret)
		return block_rsv;

	return ERR_PTR(ret);
}

int btrfs_check_trunc_cache_free_space(struct btrfs_fs_info *fs_info,
				       struct btrfs_block_rsv *rsv)
{
	u64 needed_bytes;
	int ret;

	 
	needed_bytes = btrfs_calc_insert_metadata_size(fs_info, 1) +
		btrfs_calc_metadata_size(fs_info, 1);

	spin_lock(&rsv->lock);
	if (rsv->reserved < needed_bytes)
		ret = -ENOSPC;
	else
		ret = 0;
	spin_unlock(&rsv->lock);
	return ret;
}

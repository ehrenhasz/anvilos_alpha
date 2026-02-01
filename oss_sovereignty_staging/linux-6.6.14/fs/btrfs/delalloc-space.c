

#include "messages.h"
#include "ctree.h"
#include "delalloc-space.h"
#include "block-rsv.h"
#include "btrfs_inode.h"
#include "space-info.h"
#include "transaction.h"
#include "qgroup.h"
#include "block-group.h"
#include "fs.h"

 

int btrfs_alloc_data_chunk_ondemand(struct btrfs_inode *inode, u64 bytes)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	enum btrfs_reserve_flush_enum flush = BTRFS_RESERVE_FLUSH_DATA;

	 
	bytes = ALIGN(bytes, fs_info->sectorsize);

	if (btrfs_is_free_space_inode(inode))
		flush = BTRFS_RESERVE_FLUSH_FREE_SPACE_INODE;

	return btrfs_reserve_data_bytes(fs_info, bytes, flush);
}

int btrfs_check_data_free_space(struct btrfs_inode *inode,
				struct extent_changeset **reserved, u64 start,
				u64 len, bool noflush)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	enum btrfs_reserve_flush_enum flush = BTRFS_RESERVE_FLUSH_DATA;
	int ret;

	 
	len = round_up(start + len, fs_info->sectorsize) -
	      round_down(start, fs_info->sectorsize);
	start = round_down(start, fs_info->sectorsize);

	if (noflush)
		flush = BTRFS_RESERVE_NO_FLUSH;
	else if (btrfs_is_free_space_inode(inode))
		flush = BTRFS_RESERVE_FLUSH_FREE_SPACE_INODE;

	ret = btrfs_reserve_data_bytes(fs_info, len, flush);
	if (ret < 0)
		return ret;

	 
	ret = btrfs_qgroup_reserve_data(inode, reserved, start, len);
	if (ret < 0) {
		btrfs_free_reserved_data_space_noquota(fs_info, len);
		extent_changeset_free(*reserved);
		*reserved = NULL;
	} else {
		ret = 0;
	}
	return ret;
}

 
void btrfs_free_reserved_data_space_noquota(struct btrfs_fs_info *fs_info,
					    u64 len)
{
	struct btrfs_space_info *data_sinfo;

	ASSERT(IS_ALIGNED(len, fs_info->sectorsize));

	data_sinfo = fs_info->data_sinfo;
	btrfs_space_info_free_bytes_may_use(fs_info, data_sinfo, len);
}

 
void btrfs_free_reserved_data_space(struct btrfs_inode *inode,
			struct extent_changeset *reserved, u64 start, u64 len)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;

	 
	len = round_up(start + len, fs_info->sectorsize) -
	      round_down(start, fs_info->sectorsize);
	start = round_down(start, fs_info->sectorsize);

	btrfs_free_reserved_data_space_noquota(fs_info, len);
	btrfs_qgroup_free_data(inode, reserved, start, len, NULL);
}

 
static void btrfs_inode_rsv_release(struct btrfs_inode *inode, bool qgroup_free)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct btrfs_block_rsv *block_rsv = &inode->block_rsv;
	u64 released = 0;
	u64 qgroup_to_release = 0;

	 
	released = btrfs_block_rsv_release(fs_info, block_rsv, 0,
					   &qgroup_to_release);
	if (released > 0)
		trace_btrfs_space_reservation(fs_info, "delalloc",
					      btrfs_ino(inode), released, 0);
	if (qgroup_free)
		btrfs_qgroup_free_meta_prealloc(inode->root, qgroup_to_release);
	else
		btrfs_qgroup_convert_reserved_meta(inode->root,
						   qgroup_to_release);
}

static void btrfs_calculate_inode_block_rsv_size(struct btrfs_fs_info *fs_info,
						 struct btrfs_inode *inode)
{
	struct btrfs_block_rsv *block_rsv = &inode->block_rsv;
	u64 reserve_size = 0;
	u64 qgroup_rsv_size = 0;
	u64 csum_leaves;
	unsigned outstanding_extents;

	lockdep_assert_held(&inode->lock);
	outstanding_extents = inode->outstanding_extents;

	 
	if (outstanding_extents) {
		reserve_size = btrfs_calc_insert_metadata_size(fs_info,
						outstanding_extents);
		reserve_size += btrfs_calc_metadata_size(fs_info, 1);
	}
	csum_leaves = btrfs_csum_bytes_to_leaves(fs_info,
						 inode->csum_bytes);
	reserve_size += btrfs_calc_insert_metadata_size(fs_info,
							csum_leaves);
	 
	qgroup_rsv_size = (u64)outstanding_extents * fs_info->nodesize;

	spin_lock(&block_rsv->lock);
	block_rsv->size = reserve_size;
	block_rsv->qgroup_rsv_size = qgroup_rsv_size;
	spin_unlock(&block_rsv->lock);
}

static void calc_inode_reservations(struct btrfs_fs_info *fs_info,
				    u64 num_bytes, u64 disk_num_bytes,
				    u64 *meta_reserve, u64 *qgroup_reserve)
{
	u64 nr_extents = count_max_extents(fs_info, num_bytes);
	u64 csum_leaves = btrfs_csum_bytes_to_leaves(fs_info, disk_num_bytes);
	u64 inode_update = btrfs_calc_metadata_size(fs_info, 1);

	*meta_reserve = btrfs_calc_insert_metadata_size(fs_info,
						nr_extents + csum_leaves);

	 
	*meta_reserve += inode_update;
	*qgroup_reserve = nr_extents * fs_info->nodesize;
}

int btrfs_delalloc_reserve_metadata(struct btrfs_inode *inode, u64 num_bytes,
				    u64 disk_num_bytes, bool noflush)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_block_rsv *block_rsv = &inode->block_rsv;
	u64 meta_reserve, qgroup_reserve;
	unsigned nr_extents;
	enum btrfs_reserve_flush_enum flush = BTRFS_RESERVE_FLUSH_ALL;
	int ret = 0;

	 
	if (noflush || btrfs_is_free_space_inode(inode)) {
		flush = BTRFS_RESERVE_NO_FLUSH;
	} else {
		if (current->journal_info)
			flush = BTRFS_RESERVE_FLUSH_LIMIT;
	}

	num_bytes = ALIGN(num_bytes, fs_info->sectorsize);
	disk_num_bytes = ALIGN(disk_num_bytes, fs_info->sectorsize);

	 
	calc_inode_reservations(fs_info, num_bytes, disk_num_bytes,
				&meta_reserve, &qgroup_reserve);
	ret = btrfs_qgroup_reserve_meta_prealloc(root, qgroup_reserve, true,
						 noflush);
	if (ret)
		return ret;
	ret = btrfs_reserve_metadata_bytes(fs_info, block_rsv, meta_reserve, flush);
	if (ret) {
		btrfs_qgroup_free_meta_prealloc(root, qgroup_reserve);
		return ret;
	}

	 
	nr_extents = count_max_extents(fs_info, num_bytes);
	spin_lock(&inode->lock);
	btrfs_mod_outstanding_extents(inode, nr_extents);
	inode->csum_bytes += disk_num_bytes;
	btrfs_calculate_inode_block_rsv_size(fs_info, inode);
	spin_unlock(&inode->lock);

	 
	btrfs_block_rsv_add_bytes(block_rsv, meta_reserve, false);
	trace_btrfs_space_reservation(root->fs_info, "delalloc",
				      btrfs_ino(inode), meta_reserve, 1);

	spin_lock(&block_rsv->lock);
	block_rsv->qgroup_rsv_reserved += qgroup_reserve;
	spin_unlock(&block_rsv->lock);

	return 0;
}

 
void btrfs_delalloc_release_metadata(struct btrfs_inode *inode, u64 num_bytes,
				     bool qgroup_free)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;

	num_bytes = ALIGN(num_bytes, fs_info->sectorsize);
	spin_lock(&inode->lock);
	inode->csum_bytes -= num_bytes;
	btrfs_calculate_inode_block_rsv_size(fs_info, inode);
	spin_unlock(&inode->lock);

	if (btrfs_is_testing(fs_info))
		return;

	btrfs_inode_rsv_release(inode, qgroup_free);
}

 
void btrfs_delalloc_release_extents(struct btrfs_inode *inode, u64 num_bytes)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	unsigned num_extents;

	spin_lock(&inode->lock);
	num_extents = count_max_extents(fs_info, num_bytes);
	btrfs_mod_outstanding_extents(inode, -num_extents);
	btrfs_calculate_inode_block_rsv_size(fs_info, inode);
	spin_unlock(&inode->lock);

	if (btrfs_is_testing(fs_info))
		return;

	btrfs_inode_rsv_release(inode, true);
}

 
int btrfs_delalloc_reserve_space(struct btrfs_inode *inode,
			struct extent_changeset **reserved, u64 start, u64 len)
{
	int ret;

	ret = btrfs_check_data_free_space(inode, reserved, start, len, false);
	if (ret < 0)
		return ret;
	ret = btrfs_delalloc_reserve_metadata(inode, len, len, false);
	if (ret < 0) {
		btrfs_free_reserved_data_space(inode, *reserved, start, len);
		extent_changeset_free(*reserved);
		*reserved = NULL;
	}
	return ret;
}

 
void btrfs_delalloc_release_space(struct btrfs_inode *inode,
				  struct extent_changeset *reserved,
				  u64 start, u64 len, bool qgroup_free)
{
	btrfs_delalloc_release_metadata(inode, len, qgroup_free);
	btrfs_free_reserved_data_space(inode, reserved, start, len);
}

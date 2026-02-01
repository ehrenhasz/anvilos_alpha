
 

#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/sched/mm.h>
#include <crypto/hash.h>
#include "messages.h"
#include "misc.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "bio.h"
#include "print-tree.h"
#include "compression.h"
#include "fs.h"
#include "accessors.h"
#include "file-item.h"
#include "super.h"

#define __MAX_CSUM_ITEMS(r, size) ((unsigned long)(((BTRFS_LEAF_DATA_SIZE(r) - \
				   sizeof(struct btrfs_item) * 2) / \
				  size) - 1))

#define MAX_CSUM_ITEMS(r, size) (min_t(u32, __MAX_CSUM_ITEMS(r, size), \
				       PAGE_SIZE))

 
void btrfs_inode_safe_disk_i_size_write(struct btrfs_inode *inode, u64 new_i_size)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	u64 start, end, i_size;
	int ret;

	spin_lock(&inode->lock);
	i_size = new_i_size ?: i_size_read(&inode->vfs_inode);
	if (btrfs_fs_incompat(fs_info, NO_HOLES)) {
		inode->disk_i_size = i_size;
		goto out_unlock;
	}

	ret = find_contiguous_extent_bit(&inode->file_extent_tree, 0, &start,
					 &end, EXTENT_DIRTY);
	if (!ret && start == 0)
		i_size = min(i_size, end + 1);
	else
		i_size = 0;
	inode->disk_i_size = i_size;
out_unlock:
	spin_unlock(&inode->lock);
}

 
int btrfs_inode_set_file_extent_range(struct btrfs_inode *inode, u64 start,
				      u64 len)
{
	if (len == 0)
		return 0;

	ASSERT(IS_ALIGNED(start + len, inode->root->fs_info->sectorsize));

	if (btrfs_fs_incompat(inode->root->fs_info, NO_HOLES))
		return 0;
	return set_extent_bit(&inode->file_extent_tree, start, start + len - 1,
			      EXTENT_DIRTY, NULL);
}

 
int btrfs_inode_clear_file_extent_range(struct btrfs_inode *inode, u64 start,
					u64 len)
{
	if (len == 0)
		return 0;

	ASSERT(IS_ALIGNED(start + len, inode->root->fs_info->sectorsize) ||
	       len == (u64)-1);

	if (btrfs_fs_incompat(inode->root->fs_info, NO_HOLES))
		return 0;
	return clear_extent_bit(&inode->file_extent_tree, start,
				start + len - 1, EXTENT_DIRTY, NULL);
}

static size_t bytes_to_csum_size(const struct btrfs_fs_info *fs_info, u32 bytes)
{
	ASSERT(IS_ALIGNED(bytes, fs_info->sectorsize));

	return (bytes >> fs_info->sectorsize_bits) * fs_info->csum_size;
}

static size_t csum_size_to_bytes(const struct btrfs_fs_info *fs_info, u32 csum_size)
{
	ASSERT(IS_ALIGNED(csum_size, fs_info->csum_size));

	return (csum_size / fs_info->csum_size) << fs_info->sectorsize_bits;
}

static inline u32 max_ordered_sum_bytes(const struct btrfs_fs_info *fs_info)
{
	u32 max_csum_size = round_down(PAGE_SIZE - sizeof(struct btrfs_ordered_sum),
				       fs_info->csum_size);

	return csum_size_to_bytes(fs_info, max_csum_size);
}

 
static int btrfs_ordered_sum_size(struct btrfs_fs_info *fs_info, unsigned long bytes)
{
	return sizeof(struct btrfs_ordered_sum) + bytes_to_csum_size(fs_info, bytes);
}

int btrfs_insert_hole_extent(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     u64 objectid, u64 pos, u64 num_bytes)
{
	int ret = 0;
	struct btrfs_file_extent_item *item;
	struct btrfs_key file_key;
	struct btrfs_path *path;
	struct extent_buffer *leaf;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	file_key.objectid = objectid;
	file_key.offset = pos;
	file_key.type = BTRFS_EXTENT_DATA_KEY;

	ret = btrfs_insert_empty_item(trans, root, path, &file_key,
				      sizeof(*item));
	if (ret < 0)
		goto out;
	BUG_ON(ret);  
	leaf = path->nodes[0];
	item = btrfs_item_ptr(leaf, path->slots[0],
			      struct btrfs_file_extent_item);
	btrfs_set_file_extent_disk_bytenr(leaf, item, 0);
	btrfs_set_file_extent_disk_num_bytes(leaf, item, 0);
	btrfs_set_file_extent_offset(leaf, item, 0);
	btrfs_set_file_extent_num_bytes(leaf, item, num_bytes);
	btrfs_set_file_extent_ram_bytes(leaf, item, num_bytes);
	btrfs_set_file_extent_generation(leaf, item, trans->transid);
	btrfs_set_file_extent_type(leaf, item, BTRFS_FILE_EXTENT_REG);
	btrfs_set_file_extent_compression(leaf, item, 0);
	btrfs_set_file_extent_encryption(leaf, item, 0);
	btrfs_set_file_extent_other_encoding(leaf, item, 0);

	btrfs_mark_buffer_dirty(trans, leaf);
out:
	btrfs_free_path(path);
	return ret;
}

static struct btrfs_csum_item *
btrfs_lookup_csum(struct btrfs_trans_handle *trans,
		  struct btrfs_root *root,
		  struct btrfs_path *path,
		  u64 bytenr, int cow)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret;
	struct btrfs_key file_key;
	struct btrfs_key found_key;
	struct btrfs_csum_item *item;
	struct extent_buffer *leaf;
	u64 csum_offset = 0;
	const u32 csum_size = fs_info->csum_size;
	int csums_in_item;

	file_key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
	file_key.offset = bytenr;
	file_key.type = BTRFS_EXTENT_CSUM_KEY;
	ret = btrfs_search_slot(trans, root, &file_key, path, 0, cow);
	if (ret < 0)
		goto fail;
	leaf = path->nodes[0];
	if (ret > 0) {
		ret = 1;
		if (path->slots[0] == 0)
			goto fail;
		path->slots[0]--;
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		if (found_key.type != BTRFS_EXTENT_CSUM_KEY)
			goto fail;

		csum_offset = (bytenr - found_key.offset) >>
				fs_info->sectorsize_bits;
		csums_in_item = btrfs_item_size(leaf, path->slots[0]);
		csums_in_item /= csum_size;

		if (csum_offset == csums_in_item) {
			ret = -EFBIG;
			goto fail;
		} else if (csum_offset > csums_in_item) {
			goto fail;
		}
	}
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_csum_item);
	item = (struct btrfs_csum_item *)((unsigned char *)item +
					  csum_offset * csum_size);
	return item;
fail:
	if (ret > 0)
		ret = -ENOENT;
	return ERR_PTR(ret);
}

int btrfs_lookup_file_extent(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid,
			     u64 offset, int mod)
{
	struct btrfs_key file_key;
	int ins_len = mod < 0 ? -1 : 0;
	int cow = mod != 0;

	file_key.objectid = objectid;
	file_key.offset = offset;
	file_key.type = BTRFS_EXTENT_DATA_KEY;

	return btrfs_search_slot(trans, root, &file_key, path, ins_len, cow);
}

 
static int search_csum_tree(struct btrfs_fs_info *fs_info,
			    struct btrfs_path *path, u64 disk_bytenr,
			    u64 len, u8 *dst)
{
	struct btrfs_root *csum_root;
	struct btrfs_csum_item *item = NULL;
	struct btrfs_key key;
	const u32 sectorsize = fs_info->sectorsize;
	const u32 csum_size = fs_info->csum_size;
	u32 itemsize;
	int ret;
	u64 csum_start;
	u64 csum_len;

	ASSERT(IS_ALIGNED(disk_bytenr, sectorsize) &&
	       IS_ALIGNED(len, sectorsize));

	 
	if (path->nodes[0]) {
		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_csum_item);
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		itemsize = btrfs_item_size(path->nodes[0], path->slots[0]);

		csum_start = key.offset;
		csum_len = (itemsize / csum_size) * sectorsize;

		if (in_range(disk_bytenr, csum_start, csum_len))
			goto found;
	}

	 
	btrfs_release_path(path);
	csum_root = btrfs_csum_root(fs_info, disk_bytenr);
	item = btrfs_lookup_csum(NULL, csum_root, path, disk_bytenr, 0);
	if (IS_ERR(item)) {
		ret = PTR_ERR(item);
		goto out;
	}
	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
	itemsize = btrfs_item_size(path->nodes[0], path->slots[0]);

	csum_start = key.offset;
	csum_len = (itemsize / csum_size) * sectorsize;
	ASSERT(in_range(disk_bytenr, csum_start, csum_len));

found:
	ret = (min(csum_start + csum_len, disk_bytenr + len) -
		   disk_bytenr) >> fs_info->sectorsize_bits;
	read_extent_buffer(path->nodes[0], dst, (unsigned long)item,
			ret * csum_size);
out:
	if (ret == -ENOENT || ret == -EFBIG)
		ret = 0;
	return ret;
}

 
blk_status_t btrfs_lookup_bio_sums(struct btrfs_bio *bbio)
{
	struct btrfs_inode *inode = bbio->inode;
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct bio *bio = &bbio->bio;
	struct btrfs_path *path;
	const u32 sectorsize = fs_info->sectorsize;
	const u32 csum_size = fs_info->csum_size;
	u32 orig_len = bio->bi_iter.bi_size;
	u64 orig_disk_bytenr = bio->bi_iter.bi_sector << SECTOR_SHIFT;
	const unsigned int nblocks = orig_len >> fs_info->sectorsize_bits;
	blk_status_t ret = BLK_STS_OK;
	u32 bio_offset = 0;

	if ((inode->flags & BTRFS_INODE_NODATASUM) ||
	    test_bit(BTRFS_FS_STATE_NO_CSUMS, &fs_info->fs_state))
		return BLK_STS_OK;

	 
	ASSERT(bio_op(bio) == REQ_OP_READ);
	path = btrfs_alloc_path();
	if (!path)
		return BLK_STS_RESOURCE;

	if (nblocks * csum_size > BTRFS_BIO_INLINE_CSUM_SIZE) {
		bbio->csum = kmalloc_array(nblocks, csum_size, GFP_NOFS);
		if (!bbio->csum) {
			btrfs_free_path(path);
			return BLK_STS_RESOURCE;
		}
	} else {
		bbio->csum = bbio->csum_inline;
	}

	 
	if (nblocks > fs_info->csums_per_leaf)
		path->reada = READA_FORWARD;

	 
	if (btrfs_is_free_space_inode(inode)) {
		path->search_commit_root = 1;
		path->skip_locking = 1;
	}

	while (bio_offset < orig_len) {
		int count;
		u64 cur_disk_bytenr = orig_disk_bytenr + bio_offset;
		u8 *csum_dst = bbio->csum +
			(bio_offset >> fs_info->sectorsize_bits) * csum_size;

		count = search_csum_tree(fs_info, path, cur_disk_bytenr,
					 orig_len - bio_offset, csum_dst);
		if (count < 0) {
			ret = errno_to_blk_status(count);
			if (bbio->csum != bbio->csum_inline)
				kfree(bbio->csum);
			bbio->csum = NULL;
			break;
		}

		 
		if (count == 0) {
			memset(csum_dst, 0, csum_size);
			count = 1;

			if (inode->root->root_key.objectid ==
			    BTRFS_DATA_RELOC_TREE_OBJECTID) {
				u64 file_offset = bbio->file_offset + bio_offset;

				set_extent_bit(&inode->io_tree, file_offset,
					       file_offset + sectorsize - 1,
					       EXTENT_NODATASUM, NULL);
			} else {
				btrfs_warn_rl(fs_info,
			"csum hole found for disk bytenr range [%llu, %llu)",
				cur_disk_bytenr, cur_disk_bytenr + sectorsize);
			}
		}
		bio_offset += count * sectorsize;
	}

	btrfs_free_path(path);
	return ret;
}

int btrfs_lookup_csums_list(struct btrfs_root *root, u64 start, u64 end,
			    struct list_head *list, int search_commit,
			    bool nowait)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key key;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_ordered_sum *sums;
	struct btrfs_csum_item *item;
	LIST_HEAD(tmplist);
	int ret;

	ASSERT(IS_ALIGNED(start, fs_info->sectorsize) &&
	       IS_ALIGNED(end + 1, fs_info->sectorsize));

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	path->nowait = nowait;
	if (search_commit) {
		path->skip_locking = 1;
		path->reada = READA_FORWARD;
		path->search_commit_root = 1;
	}

	key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
	key.offset = start;
	key.type = BTRFS_EXTENT_CSUM_KEY;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto fail;
	if (ret > 0 && path->slots[0] > 0) {
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0] - 1);

		 
		if (key.objectid == BTRFS_EXTENT_CSUM_OBJECTID &&
		    key.type == BTRFS_EXTENT_CSUM_KEY) {
			if (bytes_to_csum_size(fs_info, start - key.offset) <
			    btrfs_item_size(leaf, path->slots[0] - 1))
				path->slots[0]--;
		}
	}

	while (start <= end) {
		u64 csum_end;

		leaf = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto fail;
			if (ret > 0)
				break;
			leaf = path->nodes[0];
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != BTRFS_EXTENT_CSUM_OBJECTID ||
		    key.type != BTRFS_EXTENT_CSUM_KEY ||
		    key.offset > end)
			break;

		if (key.offset > start)
			start = key.offset;

		csum_end = key.offset + csum_size_to_bytes(fs_info,
					btrfs_item_size(leaf, path->slots[0]));
		if (csum_end <= start) {
			path->slots[0]++;
			continue;
		}

		csum_end = min(csum_end, end + 1);
		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_csum_item);
		while (start < csum_end) {
			unsigned long offset;
			size_t size;

			size = min_t(size_t, csum_end - start,
				     max_ordered_sum_bytes(fs_info));
			sums = kzalloc(btrfs_ordered_sum_size(fs_info, size),
				       GFP_NOFS);
			if (!sums) {
				ret = -ENOMEM;
				goto fail;
			}

			sums->logical = start;
			sums->len = size;

			offset = bytes_to_csum_size(fs_info, start - key.offset);

			read_extent_buffer(path->nodes[0],
					   sums->sums,
					   ((unsigned long)item) + offset,
					   bytes_to_csum_size(fs_info, size));

			start += size;
			list_add_tail(&sums->list, &tmplist);
		}
		path->slots[0]++;
	}
	ret = 0;
fail:
	while (ret < 0 && !list_empty(&tmplist)) {
		sums = list_entry(tmplist.next, struct btrfs_ordered_sum, list);
		list_del(&sums->list);
		kfree(sums);
	}
	list_splice_tail(&tmplist, list);

	btrfs_free_path(path);
	return ret;
}

 
int btrfs_lookup_csums_bitmap(struct btrfs_root *root, struct btrfs_path *path,
			      u64 start, u64 end, u8 *csum_buf,
			      unsigned long *csum_bitmap)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_csum_item *item;
	const u64 orig_start = start;
	bool free_path = false;
	int ret;

	ASSERT(IS_ALIGNED(start, fs_info->sectorsize) &&
	       IS_ALIGNED(end + 1, fs_info->sectorsize));

	if (!path) {
		path = btrfs_alloc_path();
		if (!path)
			return -ENOMEM;
		free_path = true;
	}

	 
	if (path->nodes[0]) {
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

		if (key.objectid == BTRFS_EXTENT_CSUM_OBJECTID &&
		    key.type == BTRFS_EXTENT_CSUM_KEY &&
		    key.offset <= start)
			goto search_forward;
		btrfs_release_path(path);
	}

	key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
	key.type = BTRFS_EXTENT_CSUM_KEY;
	key.offset = start;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto fail;
	if (ret > 0 && path->slots[0] > 0) {
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0] - 1);

		 
		if (key.objectid == BTRFS_EXTENT_CSUM_OBJECTID &&
		    key.type == BTRFS_EXTENT_CSUM_KEY) {
			if (bytes_to_csum_size(fs_info, start - key.offset) <
			    btrfs_item_size(leaf, path->slots[0] - 1))
				path->slots[0]--;
		}
	}

search_forward:
	while (start <= end) {
		u64 csum_end;

		leaf = path->nodes[0];
		if (path->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto fail;
			if (ret > 0)
				break;
			leaf = path->nodes[0];
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != BTRFS_EXTENT_CSUM_OBJECTID ||
		    key.type != BTRFS_EXTENT_CSUM_KEY ||
		    key.offset > end)
			break;

		if (key.offset > start)
			start = key.offset;

		csum_end = key.offset + csum_size_to_bytes(fs_info,
					btrfs_item_size(leaf, path->slots[0]));
		if (csum_end <= start) {
			path->slots[0]++;
			continue;
		}

		csum_end = min(csum_end, end + 1);
		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_csum_item);
		while (start < csum_end) {
			unsigned long offset;
			size_t size;
			u8 *csum_dest = csum_buf + bytes_to_csum_size(fs_info,
						start - orig_start);

			size = min_t(size_t, csum_end - start, end + 1 - start);

			offset = bytes_to_csum_size(fs_info, start - key.offset);

			read_extent_buffer(path->nodes[0], csum_dest,
					   ((unsigned long)item) + offset,
					   bytes_to_csum_size(fs_info, size));

			bitmap_set(csum_bitmap,
				(start - orig_start) >> fs_info->sectorsize_bits,
				size >> fs_info->sectorsize_bits);

			start += size;
		}
		path->slots[0]++;
	}
	ret = 0;
fail:
	if (free_path)
		btrfs_free_path(path);
	return ret;
}

 
blk_status_t btrfs_csum_one_bio(struct btrfs_bio *bbio)
{
	struct btrfs_ordered_extent *ordered = bbio->ordered;
	struct btrfs_inode *inode = bbio->inode;
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	SHASH_DESC_ON_STACK(shash, fs_info->csum_shash);
	struct bio *bio = &bbio->bio;
	struct btrfs_ordered_sum *sums;
	char *data;
	struct bvec_iter iter;
	struct bio_vec bvec;
	int index;
	unsigned int blockcount;
	int i;
	unsigned nofs_flag;

	nofs_flag = memalloc_nofs_save();
	sums = kvzalloc(btrfs_ordered_sum_size(fs_info, bio->bi_iter.bi_size),
		       GFP_KERNEL);
	memalloc_nofs_restore(nofs_flag);

	if (!sums)
		return BLK_STS_RESOURCE;

	sums->len = bio->bi_iter.bi_size;
	INIT_LIST_HEAD(&sums->list);

	sums->logical = bio->bi_iter.bi_sector << SECTOR_SHIFT;
	index = 0;

	shash->tfm = fs_info->csum_shash;

	bio_for_each_segment(bvec, bio, iter) {
		blockcount = BTRFS_BYTES_TO_BLKS(fs_info,
						 bvec.bv_len + fs_info->sectorsize
						 - 1);

		for (i = 0; i < blockcount; i++) {
			data = bvec_kmap_local(&bvec);
			crypto_shash_digest(shash,
					    data + (i * fs_info->sectorsize),
					    fs_info->sectorsize,
					    sums->sums + index);
			kunmap_local(data);
			index += fs_info->csum_size;
		}

	}

	bbio->sums = sums;
	btrfs_add_ordered_sum(ordered, sums);
	return 0;
}

 
blk_status_t btrfs_alloc_dummy_sum(struct btrfs_bio *bbio)
{
	bbio->sums = kmalloc(sizeof(*bbio->sums), GFP_NOFS);
	if (!bbio->sums)
		return BLK_STS_RESOURCE;
	bbio->sums->len = bbio->bio.bi_iter.bi_size;
	bbio->sums->logical = bbio->bio.bi_iter.bi_sector << SECTOR_SHIFT;
	btrfs_add_ordered_sum(bbio->ordered, bbio->sums);
	return 0;
}

 
static noinline void truncate_one_csum(struct btrfs_trans_handle *trans,
				       struct btrfs_path *path,
				       struct btrfs_key *key,
				       u64 bytenr, u64 len)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct extent_buffer *leaf;
	const u32 csum_size = fs_info->csum_size;
	u64 csum_end;
	u64 end_byte = bytenr + len;
	u32 blocksize_bits = fs_info->sectorsize_bits;

	leaf = path->nodes[0];
	csum_end = btrfs_item_size(leaf, path->slots[0]) / csum_size;
	csum_end <<= blocksize_bits;
	csum_end += key->offset;

	if (key->offset < bytenr && csum_end <= end_byte) {
		 
		u32 new_size = (bytenr - key->offset) >> blocksize_bits;
		new_size *= csum_size;
		btrfs_truncate_item(trans, path, new_size, 1);
	} else if (key->offset >= bytenr && csum_end > end_byte &&
		   end_byte > key->offset) {
		 
		u32 new_size = (csum_end - end_byte) >> blocksize_bits;
		new_size *= csum_size;

		btrfs_truncate_item(trans, path, new_size, 0);

		key->offset = end_byte;
		btrfs_set_item_key_safe(trans, path, key);
	} else {
		BUG();
	}
}

 
int btrfs_del_csums(struct btrfs_trans_handle *trans,
		    struct btrfs_root *root, u64 bytenr, u64 len)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_path *path;
	struct btrfs_key key;
	u64 end_byte = bytenr + len;
	u64 csum_end;
	struct extent_buffer *leaf;
	int ret = 0;
	const u32 csum_size = fs_info->csum_size;
	u32 blocksize_bits = fs_info->sectorsize_bits;

	ASSERT(root->root_key.objectid == BTRFS_CSUM_TREE_OBJECTID ||
	       root->root_key.objectid == BTRFS_TREE_LOG_OBJECTID);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	while (1) {
		key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
		key.offset = end_byte - 1;
		key.type = BTRFS_EXTENT_CSUM_KEY;

		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret > 0) {
			ret = 0;
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		} else if (ret < 0) {
			break;
		}

		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

		if (key.objectid != BTRFS_EXTENT_CSUM_OBJECTID ||
		    key.type != BTRFS_EXTENT_CSUM_KEY) {
			break;
		}

		if (key.offset >= end_byte)
			break;

		csum_end = btrfs_item_size(leaf, path->slots[0]) / csum_size;
		csum_end <<= blocksize_bits;
		csum_end += key.offset;

		 
		if (csum_end <= bytenr)
			break;

		 
		if (key.offset >= bytenr && csum_end <= end_byte) {
			int del_nr = 1;

			 
			if (key.offset > bytenr && path->slots[0] > 0) {
				int slot = path->slots[0] - 1;

				while (slot >= 0) {
					struct btrfs_key pk;

					btrfs_item_key_to_cpu(leaf, &pk, slot);
					if (pk.offset < bytenr ||
					    pk.type != BTRFS_EXTENT_CSUM_KEY ||
					    pk.objectid !=
					    BTRFS_EXTENT_CSUM_OBJECTID)
						break;
					path->slots[0] = slot;
					del_nr++;
					key.offset = pk.offset;
					slot--;
				}
			}
			ret = btrfs_del_items(trans, root, path,
					      path->slots[0], del_nr);
			if (ret)
				break;
			if (key.offset == bytenr)
				break;
		} else if (key.offset < bytenr && csum_end > end_byte) {
			unsigned long offset;
			unsigned long shift_len;
			unsigned long item_offset;
			 
			offset = (bytenr - key.offset) >> blocksize_bits;
			offset *= csum_size;

			shift_len = (len >> blocksize_bits) * csum_size;

			item_offset = btrfs_item_ptr_offset(leaf,
							    path->slots[0]);

			memzero_extent_buffer(leaf, item_offset + offset,
					     shift_len);
			key.offset = bytenr;

			 
			ret = btrfs_split_item(trans, root, path, &key, offset);
			if (ret && ret != -EAGAIN) {
				btrfs_abort_transaction(trans, ret);
				break;
			}
			ret = 0;

			key.offset = end_byte - 1;
		} else {
			truncate_one_csum(trans, path, &key, bytenr, len);
			if (key.offset < bytenr)
				break;
		}
		btrfs_release_path(path);
	}
	btrfs_free_path(path);
	return ret;
}

static int find_next_csum_offset(struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 *next_offset)
{
	const u32 nritems = btrfs_header_nritems(path->nodes[0]);
	struct btrfs_key found_key;
	int slot = path->slots[0] + 1;
	int ret;

	if (nritems == 0 || slot >= nritems) {
		ret = btrfs_next_leaf(root, path);
		if (ret < 0) {
			return ret;
		} else if (ret > 0) {
			*next_offset = (u64)-1;
			return 0;
		}
		slot = path->slots[0];
	}

	btrfs_item_key_to_cpu(path->nodes[0], &found_key, slot);

	if (found_key.objectid != BTRFS_EXTENT_CSUM_OBJECTID ||
	    found_key.type != BTRFS_EXTENT_CSUM_KEY)
		*next_offset = (u64)-1;
	else
		*next_offset = found_key.offset;

	return 0;
}

int btrfs_csum_file_blocks(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct btrfs_ordered_sum *sums)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key file_key;
	struct btrfs_key found_key;
	struct btrfs_path *path;
	struct btrfs_csum_item *item;
	struct btrfs_csum_item *item_end;
	struct extent_buffer *leaf = NULL;
	u64 next_offset;
	u64 total_bytes = 0;
	u64 csum_offset;
	u64 bytenr;
	u32 ins_size;
	int index = 0;
	int found_next;
	int ret;
	const u32 csum_size = fs_info->csum_size;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
again:
	next_offset = (u64)-1;
	found_next = 0;
	bytenr = sums->logical + total_bytes;
	file_key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
	file_key.offset = bytenr;
	file_key.type = BTRFS_EXTENT_CSUM_KEY;

	item = btrfs_lookup_csum(trans, root, path, bytenr, 1);
	if (!IS_ERR(item)) {
		ret = 0;
		leaf = path->nodes[0];
		item_end = btrfs_item_ptr(leaf, path->slots[0],
					  struct btrfs_csum_item);
		item_end = (struct btrfs_csum_item *)((char *)item_end +
			   btrfs_item_size(leaf, path->slots[0]));
		goto found;
	}
	ret = PTR_ERR(item);
	if (ret != -EFBIG && ret != -ENOENT)
		goto out;

	if (ret == -EFBIG) {
		u32 item_size;
		 
		leaf = path->nodes[0];
		item_size = btrfs_item_size(leaf, path->slots[0]);
		if ((item_size / csum_size) >=
		    MAX_CSUM_ITEMS(fs_info, csum_size)) {
			 
			goto insert;
		}
	} else {
		 
		ret = find_next_csum_offset(root, path, &next_offset);
		if (ret < 0)
			goto out;
		found_next = 1;
		goto insert;
	}

	 
	if (btrfs_leaf_free_space(leaf) >= csum_size) {
		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		csum_offset = (bytenr - found_key.offset) >>
			fs_info->sectorsize_bits;
		goto extend_csum;
	}

	btrfs_release_path(path);
	path->search_for_extension = 1;
	ret = btrfs_search_slot(trans, root, &file_key, path,
				csum_size, 1);
	path->search_for_extension = 0;
	if (ret < 0)
		goto out;

	if (ret > 0) {
		if (path->slots[0] == 0)
			goto insert;
		path->slots[0]--;
	}

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
	csum_offset = (bytenr - found_key.offset) >> fs_info->sectorsize_bits;

	if (found_key.type != BTRFS_EXTENT_CSUM_KEY ||
	    found_key.objectid != BTRFS_EXTENT_CSUM_OBJECTID ||
	    csum_offset >= MAX_CSUM_ITEMS(fs_info, csum_size)) {
		goto insert;
	}

extend_csum:
	if (csum_offset == btrfs_item_size(leaf, path->slots[0]) /
	    csum_size) {
		int extend_nr;
		u64 tmp;
		u32 diff;

		tmp = sums->len - total_bytes;
		tmp >>= fs_info->sectorsize_bits;
		WARN_ON(tmp < 1);
		extend_nr = max_t(int, 1, tmp);

		 
		if (root->root_key.objectid == BTRFS_TREE_LOG_OBJECTID) {
			if (path->slots[0] + 1 >=
			    btrfs_header_nritems(path->nodes[0])) {
				ret = find_next_csum_offset(root, path, &next_offset);
				if (ret < 0)
					goto out;
				found_next = 1;
				goto insert;
			}

			ret = find_next_csum_offset(root, path, &next_offset);
			if (ret < 0)
				goto out;

			tmp = (next_offset - bytenr) >> fs_info->sectorsize_bits;
			if (tmp <= INT_MAX)
				extend_nr = min_t(int, extend_nr, tmp);
		}

		diff = (csum_offset + extend_nr) * csum_size;
		diff = min(diff,
			   MAX_CSUM_ITEMS(fs_info, csum_size) * csum_size);

		diff = diff - btrfs_item_size(leaf, path->slots[0]);
		diff = min_t(u32, btrfs_leaf_free_space(leaf), diff);
		diff /= csum_size;
		diff *= csum_size;

		btrfs_extend_item(trans, path, diff);
		ret = 0;
		goto csum;
	}

insert:
	btrfs_release_path(path);
	csum_offset = 0;
	if (found_next) {
		u64 tmp;

		tmp = sums->len - total_bytes;
		tmp >>= fs_info->sectorsize_bits;
		tmp = min(tmp, (next_offset - file_key.offset) >>
					 fs_info->sectorsize_bits);

		tmp = max_t(u64, 1, tmp);
		tmp = min_t(u64, tmp, MAX_CSUM_ITEMS(fs_info, csum_size));
		ins_size = csum_size * tmp;
	} else {
		ins_size = csum_size;
	}
	ret = btrfs_insert_empty_item(trans, root, path, &file_key,
				      ins_size);
	if (ret < 0)
		goto out;
	if (WARN_ON(ret != 0))
		goto out;
	leaf = path->nodes[0];
csum:
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_csum_item);
	item_end = (struct btrfs_csum_item *)((unsigned char *)item +
				      btrfs_item_size(leaf, path->slots[0]));
	item = (struct btrfs_csum_item *)((unsigned char *)item +
					  csum_offset * csum_size);
found:
	ins_size = (u32)(sums->len - total_bytes) >> fs_info->sectorsize_bits;
	ins_size *= csum_size;
	ins_size = min_t(u32, (unsigned long)item_end - (unsigned long)item,
			      ins_size);
	write_extent_buffer(leaf, sums->sums + index, (unsigned long)item,
			    ins_size);

	index += ins_size;
	ins_size /= csum_size;
	total_bytes += ins_size * fs_info->sectorsize;

	btrfs_mark_buffer_dirty(trans, path->nodes[0]);
	if (total_bytes < sums->len) {
		btrfs_release_path(path);
		cond_resched();
		goto again;
	}
out:
	btrfs_free_path(path);
	return ret;
}

void btrfs_extent_item_to_extent_map(struct btrfs_inode *inode,
				     const struct btrfs_path *path,
				     struct btrfs_file_extent_item *fi,
				     struct extent_map *em)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct btrfs_root *root = inode->root;
	struct extent_buffer *leaf = path->nodes[0];
	const int slot = path->slots[0];
	struct btrfs_key key;
	u64 extent_start, extent_end;
	u64 bytenr;
	u8 type = btrfs_file_extent_type(leaf, fi);
	int compress_type = btrfs_file_extent_compression(leaf, fi);

	btrfs_item_key_to_cpu(leaf, &key, slot);
	extent_start = key.offset;
	extent_end = btrfs_file_extent_end(path);
	em->ram_bytes = btrfs_file_extent_ram_bytes(leaf, fi);
	em->generation = btrfs_file_extent_generation(leaf, fi);
	if (type == BTRFS_FILE_EXTENT_REG ||
	    type == BTRFS_FILE_EXTENT_PREALLOC) {
		em->start = extent_start;
		em->len = extent_end - extent_start;
		em->orig_start = extent_start -
			btrfs_file_extent_offset(leaf, fi);
		em->orig_block_len = btrfs_file_extent_disk_num_bytes(leaf, fi);
		bytenr = btrfs_file_extent_disk_bytenr(leaf, fi);
		if (bytenr == 0) {
			em->block_start = EXTENT_MAP_HOLE;
			return;
		}
		if (compress_type != BTRFS_COMPRESS_NONE) {
			set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);
			em->compress_type = compress_type;
			em->block_start = bytenr;
			em->block_len = em->orig_block_len;
		} else {
			bytenr += btrfs_file_extent_offset(leaf, fi);
			em->block_start = bytenr;
			em->block_len = em->len;
			if (type == BTRFS_FILE_EXTENT_PREALLOC)
				set_bit(EXTENT_FLAG_PREALLOC, &em->flags);
		}
	} else if (type == BTRFS_FILE_EXTENT_INLINE) {
		em->block_start = EXTENT_MAP_INLINE;
		em->start = extent_start;
		em->len = extent_end - extent_start;
		 
		em->orig_start = EXTENT_MAP_HOLE;
		em->block_len = (u64)-1;
		em->compress_type = compress_type;
		if (compress_type != BTRFS_COMPRESS_NONE)
			set_bit(EXTENT_FLAG_COMPRESSED, &em->flags);
	} else {
		btrfs_err(fs_info,
			  "unknown file extent item type %d, inode %llu, offset %llu, "
			  "root %llu", type, btrfs_ino(inode), extent_start,
			  root->root_key.objectid);
	}
}

 
u64 btrfs_file_extent_end(const struct btrfs_path *path)
{
	const struct extent_buffer *leaf = path->nodes[0];
	const int slot = path->slots[0];
	struct btrfs_file_extent_item *fi;
	struct btrfs_key key;
	u64 end;

	btrfs_item_key_to_cpu(leaf, &key, slot);
	ASSERT(key.type == BTRFS_EXTENT_DATA_KEY);
	fi = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);

	if (btrfs_file_extent_type(leaf, fi) == BTRFS_FILE_EXTENT_INLINE) {
		end = btrfs_file_extent_ram_bytes(leaf, fi);
		end = ALIGN(key.offset + end, leaf->fs_info->sectorsize);
	} else {
		end = key.offset + btrfs_file_extent_num_bytes(leaf, fi);
	}

	return end;
}

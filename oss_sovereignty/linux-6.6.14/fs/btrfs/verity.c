

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/rwsem.h>
#include <linux/xattr.h>
#include <linux/security.h>
#include <linux/posix_acl_xattr.h>
#include <linux/iversion.h>
#include <linux/fsverity.h>
#include <linux/sched/mm.h>
#include "messages.h"
#include "ctree.h"
#include "btrfs_inode.h"
#include "transaction.h"
#include "disk-io.h"
#include "locking.h"
#include "fs.h"
#include "accessors.h"
#include "ioctl.h"
#include "verity.h"
#include "orphan.h"

 

#define MERKLE_START_ALIGN			65536

 
static loff_t merkle_file_pos(const struct inode *inode)
{
	u64 sz = inode->i_size;
	u64 rounded = round_up(sz, MERKLE_START_ALIGN);

	if (rounded > inode->i_sb->s_maxbytes)
		return -EFBIG;

	return rounded;
}

 
static int drop_verity_items(struct btrfs_inode *inode, u8 key_type)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = inode->root;
	struct btrfs_path *path;
	struct btrfs_key key;
	int count = 0;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	while (1) {
		 
		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			goto out;
		}

		 
		key.objectid = btrfs_ino(inode);
		key.type = key_type;
		key.offset = (u64)-1;

		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret > 0) {
			ret = 0;
			 
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		} else if (ret < 0) {
			btrfs_end_transaction(trans);
			goto out;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

		 
		if (key.objectid != btrfs_ino(inode) || key.type != key_type)
			break;

		 
		ret = btrfs_del_items(trans, root, path, path->slots[0], 1);
		if (ret) {
			btrfs_end_transaction(trans);
			goto out;
		}
		count++;
		btrfs_release_path(path);
		btrfs_end_transaction(trans);
	}
	ret = count;
	btrfs_end_transaction(trans);
out:
	btrfs_free_path(path);
	return ret;
}

 
int btrfs_drop_verity_items(struct btrfs_inode *inode)
{
	int ret;

	ret = drop_verity_items(inode, BTRFS_VERITY_DESC_ITEM_KEY);
	if (ret < 0)
		return ret;
	ret = drop_verity_items(inode, BTRFS_VERITY_MERKLE_ITEM_KEY);
	if (ret < 0)
		return ret;

	return 0;
}

 
static int write_key_bytes(struct btrfs_inode *inode, u8 key_type, u64 offset,
			   const char *src, u64 len)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_path *path;
	struct btrfs_root *root = inode->root;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	unsigned long copy_bytes;
	unsigned long src_offset = 0;
	void *data;
	int ret = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	while (len > 0) {
		 
		trans = btrfs_start_transaction(root, 1);
		if (IS_ERR(trans)) {
			ret = PTR_ERR(trans);
			break;
		}

		key.objectid = btrfs_ino(inode);
		key.type = key_type;
		key.offset = offset;

		 
		copy_bytes = min_t(u64, len, 2048);

		ret = btrfs_insert_empty_item(trans, root, path, &key, copy_bytes);
		if (ret) {
			btrfs_end_transaction(trans);
			break;
		}

		leaf = path->nodes[0];

		data = btrfs_item_ptr(leaf, path->slots[0], void);
		write_extent_buffer(leaf, src + src_offset,
				    (unsigned long)data, copy_bytes);
		offset += copy_bytes;
		src_offset += copy_bytes;
		len -= copy_bytes;

		btrfs_release_path(path);
		btrfs_end_transaction(trans);
	}

	btrfs_free_path(path);
	return ret;
}

 
static int read_key_bytes(struct btrfs_inode *inode, u8 key_type, u64 offset,
			  char *dest, u64 len, struct page *dest_page)
{
	struct btrfs_path *path;
	struct btrfs_root *root = inode->root;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u64 item_end;
	u64 copy_end;
	int copied = 0;
	u32 copy_offset;
	unsigned long copy_bytes;
	unsigned long dest_offset = 0;
	void *data;
	char *kaddr = dest;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	if (dest_page)
		path->reada = READA_FORWARD;

	key.objectid = btrfs_ino(inode);
	key.type = key_type;
	key.offset = offset;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0) {
		goto out;
	} else if (ret > 0) {
		ret = 0;
		if (path->slots[0] == 0)
			goto out;
		path->slots[0]--;
	}

	while (len > 0) {
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

		if (key.objectid != btrfs_ino(inode) || key.type != key_type)
			break;

		item_end = btrfs_item_size(leaf, path->slots[0]) + key.offset;

		if (copied > 0) {
			 
			if (key.offset != offset)
				break;
		} else {
			 
			if (key.offset > offset)
				break;
			if (item_end <= offset)
				break;
		}

		 
		if (!dest)
			copy_end = item_end;
		else
			copy_end = min(offset + len, item_end);

		 
		copy_bytes = copy_end - offset;

		 
		copy_offset = offset - key.offset;

		if (dest) {
			if (dest_page)
				kaddr = kmap_local_page(dest_page);

			data = btrfs_item_ptr(leaf, path->slots[0], void);
			read_extent_buffer(leaf, kaddr + dest_offset,
					   (unsigned long)data + copy_offset,
					   copy_bytes);

			if (dest_page)
				kunmap_local(kaddr);
		}

		offset += copy_bytes;
		dest_offset += copy_bytes;
		len -= copy_bytes;
		copied += copy_bytes;

		path->slots[0]++;
		if (path->slots[0] >= btrfs_header_nritems(path->nodes[0])) {
			 
			ret = btrfs_next_leaf(root, path);
			if (ret < 0) {
				break;
			} else if (ret > 0) {
				ret = 0;
				break;
			}
		}
	}
out:
	btrfs_free_path(path);
	if (!ret)
		ret = copied;
	return ret;
}

 
static int del_orphan(struct btrfs_trans_handle *trans, struct btrfs_inode *inode)
{
	struct btrfs_root *root = inode->root;
	int ret;

	 
	if (!inode->vfs_inode.i_nlink)
		return 0;

	ret = btrfs_del_orphan_item(trans, root, btrfs_ino(inode));
	if (ret == -ENOENT)
		ret = 0;
	return ret;
}

 
static int rollback_verity(struct btrfs_inode *inode)
{
	struct btrfs_trans_handle *trans = NULL;
	struct btrfs_root *root = inode->root;
	int ret;

	ASSERT(inode_is_locked(&inode->vfs_inode));
	truncate_inode_pages(inode->vfs_inode.i_mapping, inode->vfs_inode.i_size);
	clear_bit(BTRFS_INODE_VERITY_IN_PROGRESS, &inode->runtime_flags);
	ret = btrfs_drop_verity_items(inode);
	if (ret) {
		btrfs_handle_fs_error(root->fs_info, ret,
				"failed to drop verity items in rollback %llu",
				(u64)inode->vfs_inode.i_ino);
		goto out;
	}

	 
	trans = btrfs_start_transaction(root, 2);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		trans = NULL;
		btrfs_handle_fs_error(root->fs_info, ret,
			"failed to start transaction in verity rollback %llu",
			(u64)inode->vfs_inode.i_ino);
		goto out;
	}
	inode->ro_flags &= ~BTRFS_INODE_RO_VERITY;
	btrfs_sync_inode_flags_to_i_flags(&inode->vfs_inode);
	ret = btrfs_update_inode(trans, root, inode);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
	ret = del_orphan(trans, inode);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
out:
	if (trans)
		btrfs_end_transaction(trans);
	return ret;
}

 
static int finish_verity(struct btrfs_inode *inode, const void *desc,
			 size_t desc_size)
{
	struct btrfs_trans_handle *trans = NULL;
	struct btrfs_root *root = inode->root;
	struct btrfs_verity_descriptor_item item;
	int ret;

	 
	memset(&item, 0, sizeof(item));
	btrfs_set_stack_verity_descriptor_size(&item, desc_size);
	ret = write_key_bytes(inode, BTRFS_VERITY_DESC_ITEM_KEY, 0,
			      (const char *)&item, sizeof(item));
	if (ret)
		goto out;

	 
	ret = write_key_bytes(inode, BTRFS_VERITY_DESC_ITEM_KEY, 1,
			      desc, desc_size);
	if (ret)
		goto out;

	 
	trans = btrfs_start_transaction(root, 2);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto out;
	}
	inode->ro_flags |= BTRFS_INODE_RO_VERITY;
	btrfs_sync_inode_flags_to_i_flags(&inode->vfs_inode);
	ret = btrfs_update_inode(trans, root, inode);
	if (ret)
		goto end_trans;
	ret = del_orphan(trans, inode);
	if (ret)
		goto end_trans;
	clear_bit(BTRFS_INODE_VERITY_IN_PROGRESS, &inode->runtime_flags);
	btrfs_set_fs_compat_ro(root->fs_info, VERITY);
end_trans:
	btrfs_end_transaction(trans);
out:
	return ret;

}

 
static int btrfs_begin_enable_verity(struct file *filp)
{
	struct btrfs_inode *inode = BTRFS_I(file_inode(filp));
	struct btrfs_root *root = inode->root;
	struct btrfs_trans_handle *trans;
	int ret;

	ASSERT(inode_is_locked(file_inode(filp)));

	if (test_bit(BTRFS_INODE_VERITY_IN_PROGRESS, &inode->runtime_flags))
		return -EBUSY;

	 
	ret = btrfs_drop_verity_items(inode);
	if (ret)
		return ret;

	 
	trans = btrfs_start_transaction(root, 1);
	if (IS_ERR(trans))
		return PTR_ERR(trans);

	ret = btrfs_orphan_add(trans, inode);
	if (!ret)
		set_bit(BTRFS_INODE_VERITY_IN_PROGRESS, &inode->runtime_flags);
	btrfs_end_transaction(trans);

	return 0;
}

 
static int btrfs_end_enable_verity(struct file *filp, const void *desc,
				   size_t desc_size, u64 merkle_tree_size)
{
	struct btrfs_inode *inode = BTRFS_I(file_inode(filp));
	int ret = 0;
	int rollback_ret;

	ASSERT(inode_is_locked(file_inode(filp)));

	if (desc == NULL)
		goto rollback;

	ret = finish_verity(inode, desc, desc_size);
	if (ret)
		goto rollback;
	return ret;

rollback:
	rollback_ret = rollback_verity(inode);
	if (rollback_ret)
		btrfs_err(inode->root->fs_info,
			  "failed to rollback verity items: %d", rollback_ret);
	return ret;
}

 
int btrfs_get_verity_descriptor(struct inode *inode, void *buf, size_t buf_size)
{
	u64 true_size;
	int ret = 0;
	struct btrfs_verity_descriptor_item item;

	memset(&item, 0, sizeof(item));
	ret = read_key_bytes(BTRFS_I(inode), BTRFS_VERITY_DESC_ITEM_KEY, 0,
			     (char *)&item, sizeof(item), NULL);
	if (ret < 0)
		return ret;

	if (item.reserved[0] != 0 || item.reserved[1] != 0)
		return -EUCLEAN;

	true_size = btrfs_stack_verity_descriptor_size(&item);
	if (true_size > INT_MAX)
		return -EUCLEAN;

	if (buf_size == 0)
		return true_size;
	if (buf_size < true_size)
		return -ERANGE;

	ret = read_key_bytes(BTRFS_I(inode), BTRFS_VERITY_DESC_ITEM_KEY, 1,
			     buf, buf_size, NULL);
	if (ret < 0)
		return ret;
	if (ret != true_size)
		return -EIO;

	return true_size;
}

 
static struct page *btrfs_read_merkle_tree_page(struct inode *inode,
						pgoff_t index,
						unsigned long num_ra_pages)
{
	struct folio *folio;
	u64 off = (u64)index << PAGE_SHIFT;
	loff_t merkle_pos = merkle_file_pos(inode);
	int ret;

	if (merkle_pos < 0)
		return ERR_PTR(merkle_pos);
	if (merkle_pos > inode->i_sb->s_maxbytes - off - PAGE_SIZE)
		return ERR_PTR(-EFBIG);
	index += merkle_pos >> PAGE_SHIFT;
again:
	folio = __filemap_get_folio(inode->i_mapping, index, FGP_ACCESSED, 0);
	if (!IS_ERR(folio)) {
		if (folio_test_uptodate(folio))
			goto out;

		folio_lock(folio);
		 
		if (!folio_test_uptodate(folio)) {
			folio_unlock(folio);
			folio_put(folio);
			return ERR_PTR(-EIO);
		}
		folio_unlock(folio);
		goto out;
	}

	folio = filemap_alloc_folio(mapping_gfp_constraint(inode->i_mapping, ~__GFP_FS),
				    0);
	if (!folio)
		return ERR_PTR(-ENOMEM);

	ret = filemap_add_folio(inode->i_mapping, folio, index, GFP_NOFS);
	if (ret) {
		folio_put(folio);
		 
		if (ret == -EEXIST)
			goto again;
		return ERR_PTR(ret);
	}

	 
	ret = read_key_bytes(BTRFS_I(inode), BTRFS_VERITY_MERKLE_ITEM_KEY, off,
			     folio_address(folio), PAGE_SIZE, &folio->page);
	if (ret < 0) {
		folio_put(folio);
		return ERR_PTR(ret);
	}
	if (ret < PAGE_SIZE)
		folio_zero_segment(folio, ret, PAGE_SIZE);

	folio_mark_uptodate(folio);
	folio_unlock(folio);

out:
	return folio_file_page(folio, index);
}

 
static int btrfs_write_merkle_tree_block(struct inode *inode, const void *buf,
					 u64 pos, unsigned int size)
{
	loff_t merkle_pos = merkle_file_pos(inode);

	if (merkle_pos < 0)
		return merkle_pos;
	if (merkle_pos > inode->i_sb->s_maxbytes - pos - size)
		return -EFBIG;

	return write_key_bytes(BTRFS_I(inode), BTRFS_VERITY_MERKLE_ITEM_KEY,
			       pos, buf, size);
}

const struct fsverity_operations btrfs_verityops = {
	.begin_enable_verity     = btrfs_begin_enable_verity,
	.end_enable_verity       = btrfs_end_enable_verity,
	.get_verity_descriptor   = btrfs_get_verity_descriptor,
	.read_merkle_tree_page   = btrfs_read_merkle_tree_page,
	.write_merkle_tree_block = btrfs_write_merkle_tree_block,
};

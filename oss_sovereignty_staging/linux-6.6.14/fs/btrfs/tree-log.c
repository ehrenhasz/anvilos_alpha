
 

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/list_sort.h>
#include <linux/iversion.h>
#include "misc.h"
#include "ctree.h"
#include "tree-log.h"
#include "disk-io.h"
#include "locking.h"
#include "print-tree.h"
#include "backref.h"
#include "compression.h"
#include "qgroup.h"
#include "block-group.h"
#include "space-info.h"
#include "zoned.h"
#include "inode-item.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"
#include "root-tree.h"
#include "dir-item.h"
#include "file-item.h"
#include "file.h"
#include "orphan.h"
#include "tree-checker.h"

#define MAX_CONFLICT_INODES 10

 
enum {
	LOG_INODE_ALL,
	LOG_INODE_EXISTS,
};

 

 
enum {
	LOG_WALK_PIN_ONLY,
	LOG_WALK_REPLAY_INODES,
	LOG_WALK_REPLAY_DIR_INDEX,
	LOG_WALK_REPLAY_ALL,
};

static int btrfs_log_inode(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode,
			   int inode_only,
			   struct btrfs_log_ctx *ctx);
static int link_to_fixup_dir(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path, u64 objectid);
static noinline int replay_dir_deletes(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid, int del_all);
static void wait_log_commit(struct btrfs_root *root, int transid);

 

 
static int start_log_trans(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct btrfs_log_ctx *ctx)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_root *tree_root = fs_info->tree_root;
	const bool zoned = btrfs_is_zoned(fs_info);
	int ret = 0;
	bool created = false;

	 
	if (!test_bit(BTRFS_ROOT_HAS_LOG_TREE, &tree_root->state)) {
		mutex_lock(&tree_root->log_mutex);
		if (!fs_info->log_root_tree) {
			ret = btrfs_init_log_root_tree(trans, fs_info);
			if (!ret) {
				set_bit(BTRFS_ROOT_HAS_LOG_TREE, &tree_root->state);
				created = true;
			}
		}
		mutex_unlock(&tree_root->log_mutex);
		if (ret)
			return ret;
	}

	mutex_lock(&root->log_mutex);

again:
	if (root->log_root) {
		int index = (root->log_transid + 1) % 2;

		if (btrfs_need_log_full_commit(trans)) {
			ret = BTRFS_LOG_FORCE_COMMIT;
			goto out;
		}

		if (zoned && atomic_read(&root->log_commit[index])) {
			wait_log_commit(root, root->log_transid - 1);
			goto again;
		}

		if (!root->log_start_pid) {
			clear_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state);
			root->log_start_pid = current->pid;
		} else if (root->log_start_pid != current->pid) {
			set_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state);
		}
	} else {
		 
		if (zoned && !created) {
			ret = BTRFS_LOG_FORCE_COMMIT;
			goto out;
		}

		ret = btrfs_add_log_tree(trans, root);
		if (ret)
			goto out;

		set_bit(BTRFS_ROOT_HAS_LOG_TREE, &root->state);
		clear_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state);
		root->log_start_pid = current->pid;
	}

	atomic_inc(&root->log_writers);
	if (!ctx->logging_new_name) {
		int index = root->log_transid % 2;
		list_add_tail(&ctx->list, &root->log_ctxs[index]);
		ctx->log_transid = root->log_transid;
	}

out:
	mutex_unlock(&root->log_mutex);
	return ret;
}

 
static int join_running_log_trans(struct btrfs_root *root)
{
	const bool zoned = btrfs_is_zoned(root->fs_info);
	int ret = -ENOENT;

	if (!test_bit(BTRFS_ROOT_HAS_LOG_TREE, &root->state))
		return ret;

	mutex_lock(&root->log_mutex);
again:
	if (root->log_root) {
		int index = (root->log_transid + 1) % 2;

		ret = 0;
		if (zoned && atomic_read(&root->log_commit[index])) {
			wait_log_commit(root, root->log_transid - 1);
			goto again;
		}
		atomic_inc(&root->log_writers);
	}
	mutex_unlock(&root->log_mutex);
	return ret;
}

 
void btrfs_pin_log_trans(struct btrfs_root *root)
{
	atomic_inc(&root->log_writers);
}

 
void btrfs_end_log_trans(struct btrfs_root *root)
{
	if (atomic_dec_and_test(&root->log_writers)) {
		 
		cond_wake_up_nomb(&root->log_writer_wait);
	}
}

 
struct walk_control {
	 
	int free;

	 
	int pin;

	 
	int stage;

	 
	bool ignore_cur_inode;

	 
	struct btrfs_root *replay_dest;

	 
	struct btrfs_trans_handle *trans;

	 
	int (*process_func)(struct btrfs_root *log, struct extent_buffer *eb,
			    struct walk_control *wc, u64 gen, int level);
};

 
static int process_one_buffer(struct btrfs_root *log,
			      struct extent_buffer *eb,
			      struct walk_control *wc, u64 gen, int level)
{
	struct btrfs_fs_info *fs_info = log->fs_info;
	int ret = 0;

	 
	if (btrfs_fs_incompat(fs_info, MIXED_GROUPS)) {
		struct btrfs_tree_parent_check check = {
			.level = level,
			.transid = gen
		};

		ret = btrfs_read_extent_buffer(eb, &check);
		if (ret)
			return ret;
	}

	if (wc->pin) {
		ret = btrfs_pin_extent_for_log_replay(wc->trans, eb->start,
						      eb->len);
		if (ret)
			return ret;

		if (btrfs_buffer_uptodate(eb, gen, 0) &&
		    btrfs_header_level(eb) == 0)
			ret = btrfs_exclude_logged_extents(eb);
	}
	return ret;
}

 
static int overwrite_item(struct btrfs_trans_handle *trans,
			  struct btrfs_root *root,
			  struct btrfs_path *path,
			  struct extent_buffer *eb, int slot,
			  struct btrfs_key *key)
{
	int ret;
	u32 item_size;
	u64 saved_i_size = 0;
	int save_old_i_size = 0;
	unsigned long src_ptr;
	unsigned long dst_ptr;
	bool inode_item = key->type == BTRFS_INODE_ITEM_KEY;

	 
	ASSERT(root->root_key.objectid != BTRFS_TREE_LOG_OBJECTID);

	item_size = btrfs_item_size(eb, slot);
	src_ptr = btrfs_item_ptr_offset(eb, slot);

	 
	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret < 0)
		return ret;

	if (ret == 0) {
		char *src_copy;
		char *dst_copy;
		u32 dst_size = btrfs_item_size(path->nodes[0],
						  path->slots[0]);
		if (dst_size != item_size)
			goto insert;

		if (item_size == 0) {
			btrfs_release_path(path);
			return 0;
		}
		dst_copy = kmalloc(item_size, GFP_NOFS);
		src_copy = kmalloc(item_size, GFP_NOFS);
		if (!dst_copy || !src_copy) {
			btrfs_release_path(path);
			kfree(dst_copy);
			kfree(src_copy);
			return -ENOMEM;
		}

		read_extent_buffer(eb, src_copy, src_ptr, item_size);

		dst_ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
		read_extent_buffer(path->nodes[0], dst_copy, dst_ptr,
				   item_size);
		ret = memcmp(dst_copy, src_copy, item_size);

		kfree(dst_copy);
		kfree(src_copy);
		 
		if (ret == 0) {
			btrfs_release_path(path);
			return 0;
		}

		 
		if (inode_item) {
			struct btrfs_inode_item *item;
			u64 nbytes;
			u32 mode;

			item = btrfs_item_ptr(path->nodes[0], path->slots[0],
					      struct btrfs_inode_item);
			nbytes = btrfs_inode_nbytes(path->nodes[0], item);
			item = btrfs_item_ptr(eb, slot,
					      struct btrfs_inode_item);
			btrfs_set_inode_nbytes(eb, item, nbytes);

			 
			mode = btrfs_inode_mode(eb, item);
			if (S_ISDIR(mode))
				btrfs_set_inode_size(eb, item, 0);
		}
	} else if (inode_item) {
		struct btrfs_inode_item *item;
		u32 mode;

		 
		item = btrfs_item_ptr(eb, slot, struct btrfs_inode_item);
		btrfs_set_inode_nbytes(eb, item, 0);

		 
		mode = btrfs_inode_mode(eb, item);
		if (S_ISDIR(mode))
			btrfs_set_inode_size(eb, item, 0);
	}
insert:
	btrfs_release_path(path);
	 
	path->skip_release_on_error = 1;
	ret = btrfs_insert_empty_item(trans, root, path,
				      key, item_size);
	path->skip_release_on_error = 0;

	 
	if (ret == -EEXIST || ret == -EOVERFLOW) {
		u32 found_size;
		found_size = btrfs_item_size(path->nodes[0],
						path->slots[0]);
		if (found_size > item_size)
			btrfs_truncate_item(trans, path, item_size, 1);
		else if (found_size < item_size)
			btrfs_extend_item(trans, path, item_size - found_size);
	} else if (ret) {
		return ret;
	}
	dst_ptr = btrfs_item_ptr_offset(path->nodes[0],
					path->slots[0]);

	 
	if (key->type == BTRFS_INODE_ITEM_KEY && ret == -EEXIST) {
		struct btrfs_inode_item *src_item;
		struct btrfs_inode_item *dst_item;

		src_item = (struct btrfs_inode_item *)src_ptr;
		dst_item = (struct btrfs_inode_item *)dst_ptr;

		if (btrfs_inode_generation(eb, src_item) == 0) {
			struct extent_buffer *dst_eb = path->nodes[0];
			const u64 ino_size = btrfs_inode_size(eb, src_item);

			 
			if (S_ISREG(btrfs_inode_mode(eb, src_item)) &&
			    S_ISREG(btrfs_inode_mode(dst_eb, dst_item)) &&
			    ino_size != 0)
				btrfs_set_inode_size(dst_eb, dst_item, ino_size);
			goto no_copy;
		}

		if (S_ISDIR(btrfs_inode_mode(eb, src_item)) &&
		    S_ISDIR(btrfs_inode_mode(path->nodes[0], dst_item))) {
			save_old_i_size = 1;
			saved_i_size = btrfs_inode_size(path->nodes[0],
							dst_item);
		}
	}

	copy_extent_buffer(path->nodes[0], eb, dst_ptr,
			   src_ptr, item_size);

	if (save_old_i_size) {
		struct btrfs_inode_item *dst_item;
		dst_item = (struct btrfs_inode_item *)dst_ptr;
		btrfs_set_inode_size(path->nodes[0], dst_item, saved_i_size);
	}

	 
	if (key->type == BTRFS_INODE_ITEM_KEY) {
		struct btrfs_inode_item *dst_item;
		dst_item = (struct btrfs_inode_item *)dst_ptr;
		if (btrfs_inode_generation(path->nodes[0], dst_item) == 0) {
			btrfs_set_inode_generation(path->nodes[0], dst_item,
						   trans->transid);
		}
	}
no_copy:
	btrfs_mark_buffer_dirty(trans, path->nodes[0]);
	btrfs_release_path(path);
	return 0;
}

static int read_alloc_one_name(struct extent_buffer *eb, void *start, int len,
			       struct fscrypt_str *name)
{
	char *buf;

	buf = kmalloc(len, GFP_NOFS);
	if (!buf)
		return -ENOMEM;

	read_extent_buffer(eb, buf, (unsigned long)start, len);
	name->name = buf;
	name->len = len;
	return 0;
}

 
static noinline struct inode *read_one_inode(struct btrfs_root *root,
					     u64 objectid)
{
	struct inode *inode;

	inode = btrfs_iget(root->fs_info->sb, objectid, root);
	if (IS_ERR(inode))
		inode = NULL;
	return inode;
}

 
static noinline int replay_one_extent(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      struct extent_buffer *eb, int slot,
				      struct btrfs_key *key)
{
	struct btrfs_drop_extents_args drop_args = { 0 };
	struct btrfs_fs_info *fs_info = root->fs_info;
	int found_type;
	u64 extent_end;
	u64 start = key->offset;
	u64 nbytes = 0;
	struct btrfs_file_extent_item *item;
	struct inode *inode = NULL;
	unsigned long size;
	int ret = 0;

	item = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
	found_type = btrfs_file_extent_type(eb, item);

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		nbytes = btrfs_file_extent_num_bytes(eb, item);
		extent_end = start + nbytes;

		 
		if (btrfs_file_extent_disk_bytenr(eb, item) == 0)
			nbytes = 0;
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		size = btrfs_file_extent_ram_bytes(eb, item);
		nbytes = btrfs_file_extent_ram_bytes(eb, item);
		extent_end = ALIGN(start + size,
				   fs_info->sectorsize);
	} else {
		ret = 0;
		goto out;
	}

	inode = read_one_inode(root, key->objectid);
	if (!inode) {
		ret = -EIO;
		goto out;
	}

	 
	ret = btrfs_lookup_file_extent(trans, root, path,
			btrfs_ino(BTRFS_I(inode)), start, 0);

	if (ret == 0 &&
	    (found_type == BTRFS_FILE_EXTENT_REG ||
	     found_type == BTRFS_FILE_EXTENT_PREALLOC)) {
		struct btrfs_file_extent_item cmp1;
		struct btrfs_file_extent_item cmp2;
		struct btrfs_file_extent_item *existing;
		struct extent_buffer *leaf;

		leaf = path->nodes[0];
		existing = btrfs_item_ptr(leaf, path->slots[0],
					  struct btrfs_file_extent_item);

		read_extent_buffer(eb, &cmp1, (unsigned long)item,
				   sizeof(cmp1));
		read_extent_buffer(leaf, &cmp2, (unsigned long)existing,
				   sizeof(cmp2));

		 
		if (memcmp(&cmp1, &cmp2, sizeof(cmp1)) == 0) {
			btrfs_release_path(path);
			goto out;
		}
	}
	btrfs_release_path(path);

	 
	drop_args.start = start;
	drop_args.end = extent_end;
	drop_args.drop_cache = true;
	ret = btrfs_drop_extents(trans, root, BTRFS_I(inode), &drop_args);
	if (ret)
		goto out;

	if (found_type == BTRFS_FILE_EXTENT_REG ||
	    found_type == BTRFS_FILE_EXTENT_PREALLOC) {
		u64 offset;
		unsigned long dest_offset;
		struct btrfs_key ins;

		if (btrfs_file_extent_disk_bytenr(eb, item) == 0 &&
		    btrfs_fs_incompat(fs_info, NO_HOLES))
			goto update_inode;

		ret = btrfs_insert_empty_item(trans, root, path, key,
					      sizeof(*item));
		if (ret)
			goto out;
		dest_offset = btrfs_item_ptr_offset(path->nodes[0],
						    path->slots[0]);
		copy_extent_buffer(path->nodes[0], eb, dest_offset,
				(unsigned long)item,  sizeof(*item));

		ins.objectid = btrfs_file_extent_disk_bytenr(eb, item);
		ins.offset = btrfs_file_extent_disk_num_bytes(eb, item);
		ins.type = BTRFS_EXTENT_ITEM_KEY;
		offset = key->offset - btrfs_file_extent_offset(eb, item);

		 
		ret = btrfs_qgroup_trace_extent(trans,
				btrfs_file_extent_disk_bytenr(eb, item),
				btrfs_file_extent_disk_num_bytes(eb, item));
		if (ret < 0)
			goto out;

		if (ins.objectid > 0) {
			struct btrfs_ref ref = { 0 };
			u64 csum_start;
			u64 csum_end;
			LIST_HEAD(ordered_sums);

			 
			ret = btrfs_lookup_data_extent(fs_info, ins.objectid,
						ins.offset);
			if (ret < 0) {
				goto out;
			} else if (ret == 0) {
				btrfs_init_generic_ref(&ref,
						BTRFS_ADD_DELAYED_REF,
						ins.objectid, ins.offset, 0);
				btrfs_init_data_ref(&ref,
						root->root_key.objectid,
						key->objectid, offset, 0, false);
				ret = btrfs_inc_extent_ref(trans, &ref);
				if (ret)
					goto out;
			} else {
				 
				ret = btrfs_alloc_logged_file_extent(trans,
						root->root_key.objectid,
						key->objectid, offset, &ins);
				if (ret)
					goto out;
			}
			btrfs_release_path(path);

			if (btrfs_file_extent_compression(eb, item)) {
				csum_start = ins.objectid;
				csum_end = csum_start + ins.offset;
			} else {
				csum_start = ins.objectid +
					btrfs_file_extent_offset(eb, item);
				csum_end = csum_start +
					btrfs_file_extent_num_bytes(eb, item);
			}

			ret = btrfs_lookup_csums_list(root->log_root,
						csum_start, csum_end - 1,
						&ordered_sums, 0, false);
			if (ret)
				goto out;
			 
			while (!list_empty(&ordered_sums)) {
				struct btrfs_ordered_sum *sums;
				struct btrfs_root *csum_root;

				sums = list_entry(ordered_sums.next,
						struct btrfs_ordered_sum,
						list);
				csum_root = btrfs_csum_root(fs_info,
							    sums->logical);
				if (!ret)
					ret = btrfs_del_csums(trans, csum_root,
							      sums->logical,
							      sums->len);
				if (!ret)
					ret = btrfs_csum_file_blocks(trans,
								     csum_root,
								     sums);
				list_del(&sums->list);
				kfree(sums);
			}
			if (ret)
				goto out;
		} else {
			btrfs_release_path(path);
		}
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		 
		ret = overwrite_item(trans, root, path, eb, slot, key);
		if (ret)
			goto out;
	}

	ret = btrfs_inode_set_file_extent_range(BTRFS_I(inode), start,
						extent_end - start);
	if (ret)
		goto out;

update_inode:
	btrfs_update_inode_bytes(BTRFS_I(inode), nbytes, drop_args.bytes_found);
	ret = btrfs_update_inode(trans, root, BTRFS_I(inode));
out:
	iput(inode);
	return ret;
}

static int unlink_inode_for_log_replay(struct btrfs_trans_handle *trans,
				       struct btrfs_inode *dir,
				       struct btrfs_inode *inode,
				       const struct fscrypt_str *name)
{
	int ret;

	ret = btrfs_unlink_inode(trans, dir, inode, name);
	if (ret)
		return ret;
	 
	return btrfs_run_delayed_items(trans);
}

 
static noinline int drop_one_dir_item(struct btrfs_trans_handle *trans,
				      struct btrfs_path *path,
				      struct btrfs_inode *dir,
				      struct btrfs_dir_item *di)
{
	struct btrfs_root *root = dir->root;
	struct inode *inode;
	struct fscrypt_str name;
	struct extent_buffer *leaf;
	struct btrfs_key location;
	int ret;

	leaf = path->nodes[0];

	btrfs_dir_item_key_to_cpu(leaf, di, &location);
	ret = read_alloc_one_name(leaf, di + 1, btrfs_dir_name_len(leaf, di), &name);
	if (ret)
		return -ENOMEM;

	btrfs_release_path(path);

	inode = read_one_inode(root, location.objectid);
	if (!inode) {
		ret = -EIO;
		goto out;
	}

	ret = link_to_fixup_dir(trans, root, path, location.objectid);
	if (ret)
		goto out;

	ret = unlink_inode_for_log_replay(trans, dir, BTRFS_I(inode), &name);
out:
	kfree(name.name);
	iput(inode);
	return ret;
}

 
static noinline int inode_in_dir(struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 dirid, u64 objectid, u64 index,
				 struct fscrypt_str *name)
{
	struct btrfs_dir_item *di;
	struct btrfs_key location;
	int ret = 0;

	di = btrfs_lookup_dir_index_item(NULL, root, path, dirid,
					 index, name, 0);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto out;
	} else if (di) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &location);
		if (location.objectid != objectid)
			goto out;
	} else {
		goto out;
	}

	btrfs_release_path(path);
	di = btrfs_lookup_dir_item(NULL, root, path, dirid, name, 0);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto out;
	} else if (di) {
		btrfs_dir_item_key_to_cpu(path->nodes[0], di, &location);
		if (location.objectid == objectid)
			ret = 1;
	}
out:
	btrfs_release_path(path);
	return ret;
}

 
static noinline int backref_in_log(struct btrfs_root *log,
				   struct btrfs_key *key,
				   u64 ref_objectid,
				   const struct fscrypt_str *name)
{
	struct btrfs_path *path;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = btrfs_search_slot(NULL, log, key, path, 0, 0);
	if (ret < 0) {
		goto out;
	} else if (ret == 1) {
		ret = 0;
		goto out;
	}

	if (key->type == BTRFS_INODE_EXTREF_KEY)
		ret = !!btrfs_find_name_in_ext_backref(path->nodes[0],
						       path->slots[0],
						       ref_objectid, name);
	else
		ret = !!btrfs_find_name_in_backref(path->nodes[0],
						   path->slots[0], name);
out:
	btrfs_free_path(path);
	return ret;
}

static inline int __add_inode_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_path *path,
				  struct btrfs_root *log_root,
				  struct btrfs_inode *dir,
				  struct btrfs_inode *inode,
				  u64 inode_objectid, u64 parent_objectid,
				  u64 ref_index, struct fscrypt_str *name)
{
	int ret;
	struct extent_buffer *leaf;
	struct btrfs_dir_item *di;
	struct btrfs_key search_key;
	struct btrfs_inode_extref *extref;

again:
	 
	search_key.objectid = inode_objectid;
	search_key.type = BTRFS_INODE_REF_KEY;
	search_key.offset = parent_objectid;
	ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	if (ret == 0) {
		struct btrfs_inode_ref *victim_ref;
		unsigned long ptr;
		unsigned long ptr_end;

		leaf = path->nodes[0];

		 
		if (search_key.objectid == search_key.offset)
			return 1;

		 
		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		ptr_end = ptr + btrfs_item_size(leaf, path->slots[0]);
		while (ptr < ptr_end) {
			struct fscrypt_str victim_name;

			victim_ref = (struct btrfs_inode_ref *)ptr;
			ret = read_alloc_one_name(leaf, (victim_ref + 1),
				 btrfs_inode_ref_name_len(leaf, victim_ref),
				 &victim_name);
			if (ret)
				return ret;

			ret = backref_in_log(log_root, &search_key,
					     parent_objectid, &victim_name);
			if (ret < 0) {
				kfree(victim_name.name);
				return ret;
			} else if (!ret) {
				inc_nlink(&inode->vfs_inode);
				btrfs_release_path(path);

				ret = unlink_inode_for_log_replay(trans, dir, inode,
						&victim_name);
				kfree(victim_name.name);
				if (ret)
					return ret;
				goto again;
			}
			kfree(victim_name.name);

			ptr = (unsigned long)(victim_ref + 1) + victim_name.len;
		}
	}
	btrfs_release_path(path);

	 
	extref = btrfs_lookup_inode_extref(NULL, root, path, name,
					   inode_objectid, parent_objectid, 0,
					   0);
	if (IS_ERR(extref)) {
		return PTR_ERR(extref);
	} else if (extref) {
		u32 item_size;
		u32 cur_offset = 0;
		unsigned long base;
		struct inode *victim_parent;

		leaf = path->nodes[0];

		item_size = btrfs_item_size(leaf, path->slots[0]);
		base = btrfs_item_ptr_offset(leaf, path->slots[0]);

		while (cur_offset < item_size) {
			struct fscrypt_str victim_name;

			extref = (struct btrfs_inode_extref *)(base + cur_offset);

			if (btrfs_inode_extref_parent(leaf, extref) != parent_objectid)
				goto next;

			ret = read_alloc_one_name(leaf, &extref->name,
				 btrfs_inode_extref_name_len(leaf, extref),
				 &victim_name);
			if (ret)
				return ret;

			search_key.objectid = inode_objectid;
			search_key.type = BTRFS_INODE_EXTREF_KEY;
			search_key.offset = btrfs_extref_hash(parent_objectid,
							      victim_name.name,
							      victim_name.len);
			ret = backref_in_log(log_root, &search_key,
					     parent_objectid, &victim_name);
			if (ret < 0) {
				kfree(victim_name.name);
				return ret;
			} else if (!ret) {
				ret = -ENOENT;
				victim_parent = read_one_inode(root,
						parent_objectid);
				if (victim_parent) {
					inc_nlink(&inode->vfs_inode);
					btrfs_release_path(path);

					ret = unlink_inode_for_log_replay(trans,
							BTRFS_I(victim_parent),
							inode, &victim_name);
				}
				iput(victim_parent);
				kfree(victim_name.name);
				if (ret)
					return ret;
				goto again;
			}
			kfree(victim_name.name);
next:
			cur_offset += victim_name.len + sizeof(*extref);
		}
	}
	btrfs_release_path(path);

	 
	di = btrfs_lookup_dir_index_item(trans, root, path, btrfs_ino(dir),
					 ref_index, name, 0);
	if (IS_ERR(di)) {
		return PTR_ERR(di);
	} else if (di) {
		ret = drop_one_dir_item(trans, path, dir, di);
		if (ret)
			return ret;
	}
	btrfs_release_path(path);

	 
	di = btrfs_lookup_dir_item(trans, root, path, btrfs_ino(dir), name, 0);
	if (IS_ERR(di)) {
		return PTR_ERR(di);
	} else if (di) {
		ret = drop_one_dir_item(trans, path, dir, di);
		if (ret)
			return ret;
	}
	btrfs_release_path(path);

	return 0;
}

static int extref_get_fields(struct extent_buffer *eb, unsigned long ref_ptr,
			     struct fscrypt_str *name, u64 *index,
			     u64 *parent_objectid)
{
	struct btrfs_inode_extref *extref;
	int ret;

	extref = (struct btrfs_inode_extref *)ref_ptr;

	ret = read_alloc_one_name(eb, &extref->name,
				  btrfs_inode_extref_name_len(eb, extref), name);
	if (ret)
		return ret;

	if (index)
		*index = btrfs_inode_extref_index(eb, extref);
	if (parent_objectid)
		*parent_objectid = btrfs_inode_extref_parent(eb, extref);

	return 0;
}

static int ref_get_fields(struct extent_buffer *eb, unsigned long ref_ptr,
			  struct fscrypt_str *name, u64 *index)
{
	struct btrfs_inode_ref *ref;
	int ret;

	ref = (struct btrfs_inode_ref *)ref_ptr;

	ret = read_alloc_one_name(eb, ref + 1, btrfs_inode_ref_name_len(eb, ref),
				  name);
	if (ret)
		return ret;

	if (index)
		*index = btrfs_inode_ref_index(eb, ref);

	return 0;
}

 
static int unlink_old_inode_refs(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_inode *inode,
				 struct extent_buffer *log_eb,
				 int log_slot,
				 struct btrfs_key *key)
{
	int ret;
	unsigned long ref_ptr;
	unsigned long ref_end;
	struct extent_buffer *eb;

again:
	btrfs_release_path(path);
	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret > 0) {
		ret = 0;
		goto out;
	}
	if (ret < 0)
		goto out;

	eb = path->nodes[0];
	ref_ptr = btrfs_item_ptr_offset(eb, path->slots[0]);
	ref_end = ref_ptr + btrfs_item_size(eb, path->slots[0]);
	while (ref_ptr < ref_end) {
		struct fscrypt_str name;
		u64 parent_id;

		if (key->type == BTRFS_INODE_EXTREF_KEY) {
			ret = extref_get_fields(eb, ref_ptr, &name,
						NULL, &parent_id);
		} else {
			parent_id = key->offset;
			ret = ref_get_fields(eb, ref_ptr, &name, NULL);
		}
		if (ret)
			goto out;

		if (key->type == BTRFS_INODE_EXTREF_KEY)
			ret = !!btrfs_find_name_in_ext_backref(log_eb, log_slot,
							       parent_id, &name);
		else
			ret = !!btrfs_find_name_in_backref(log_eb, log_slot, &name);

		if (!ret) {
			struct inode *dir;

			btrfs_release_path(path);
			dir = read_one_inode(root, parent_id);
			if (!dir) {
				ret = -ENOENT;
				kfree(name.name);
				goto out;
			}
			ret = unlink_inode_for_log_replay(trans, BTRFS_I(dir),
						 inode, &name);
			kfree(name.name);
			iput(dir);
			if (ret)
				goto out;
			goto again;
		}

		kfree(name.name);
		ref_ptr += name.len;
		if (key->type == BTRFS_INODE_EXTREF_KEY)
			ref_ptr += sizeof(struct btrfs_inode_extref);
		else
			ref_ptr += sizeof(struct btrfs_inode_ref);
	}
	ret = 0;
 out:
	btrfs_release_path(path);
	return ret;
}

 
static noinline int add_inode_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_root *log,
				  struct btrfs_path *path,
				  struct extent_buffer *eb, int slot,
				  struct btrfs_key *key)
{
	struct inode *dir = NULL;
	struct inode *inode = NULL;
	unsigned long ref_ptr;
	unsigned long ref_end;
	struct fscrypt_str name;
	int ret;
	int log_ref_ver = 0;
	u64 parent_objectid;
	u64 inode_objectid;
	u64 ref_index = 0;
	int ref_struct_size;

	ref_ptr = btrfs_item_ptr_offset(eb, slot);
	ref_end = ref_ptr + btrfs_item_size(eb, slot);

	if (key->type == BTRFS_INODE_EXTREF_KEY) {
		struct btrfs_inode_extref *r;

		ref_struct_size = sizeof(struct btrfs_inode_extref);
		log_ref_ver = 1;
		r = (struct btrfs_inode_extref *)ref_ptr;
		parent_objectid = btrfs_inode_extref_parent(eb, r);
	} else {
		ref_struct_size = sizeof(struct btrfs_inode_ref);
		parent_objectid = key->offset;
	}
	inode_objectid = key->objectid;

	 
	dir = read_one_inode(root, parent_objectid);
	if (!dir) {
		ret = -ENOENT;
		goto out;
	}

	inode = read_one_inode(root, inode_objectid);
	if (!inode) {
		ret = -EIO;
		goto out;
	}

	while (ref_ptr < ref_end) {
		if (log_ref_ver) {
			ret = extref_get_fields(eb, ref_ptr, &name,
						&ref_index, &parent_objectid);
			 
			if (!dir)
				dir = read_one_inode(root, parent_objectid);
			if (!dir) {
				ret = -ENOENT;
				goto out;
			}
		} else {
			ret = ref_get_fields(eb, ref_ptr, &name, &ref_index);
		}
		if (ret)
			goto out;

		ret = inode_in_dir(root, path, btrfs_ino(BTRFS_I(dir)),
				   btrfs_ino(BTRFS_I(inode)), ref_index, &name);
		if (ret < 0) {
			goto out;
		} else if (ret == 0) {
			 
			ret = __add_inode_ref(trans, root, path, log,
					      BTRFS_I(dir), BTRFS_I(inode),
					      inode_objectid, parent_objectid,
					      ref_index, &name);
			if (ret) {
				if (ret == 1)
					ret = 0;
				goto out;
			}

			 
			ret = btrfs_add_link(trans, BTRFS_I(dir), BTRFS_I(inode),
					     &name, 0, ref_index);
			if (ret)
				goto out;

			ret = btrfs_update_inode(trans, root, BTRFS_I(inode));
			if (ret)
				goto out;
		}
		 

		ref_ptr = (unsigned long)(ref_ptr + ref_struct_size) + name.len;
		kfree(name.name);
		name.name = NULL;
		if (log_ref_ver) {
			iput(dir);
			dir = NULL;
		}
	}

	 
	ret = unlink_old_inode_refs(trans, root, path, BTRFS_I(inode), eb, slot,
				    key);
	if (ret)
		goto out;

	 
	ret = overwrite_item(trans, root, path, eb, slot, key);
out:
	btrfs_release_path(path);
	kfree(name.name);
	iput(dir);
	iput(inode);
	return ret;
}

static int count_inode_extrefs(struct btrfs_root *root,
		struct btrfs_inode *inode, struct btrfs_path *path)
{
	int ret = 0;
	int name_len;
	unsigned int nlink = 0;
	u32 item_size;
	u32 cur_offset = 0;
	u64 inode_objectid = btrfs_ino(inode);
	u64 offset = 0;
	unsigned long ptr;
	struct btrfs_inode_extref *extref;
	struct extent_buffer *leaf;

	while (1) {
		ret = btrfs_find_one_extref(root, inode_objectid, offset, path,
					    &extref, &offset);
		if (ret)
			break;

		leaf = path->nodes[0];
		item_size = btrfs_item_size(leaf, path->slots[0]);
		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		cur_offset = 0;

		while (cur_offset < item_size) {
			extref = (struct btrfs_inode_extref *) (ptr + cur_offset);
			name_len = btrfs_inode_extref_name_len(leaf, extref);

			nlink++;

			cur_offset += name_len + sizeof(*extref);
		}

		offset++;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);

	if (ret < 0 && ret != -ENOENT)
		return ret;
	return nlink;
}

static int count_inode_refs(struct btrfs_root *root,
			struct btrfs_inode *inode, struct btrfs_path *path)
{
	int ret;
	struct btrfs_key key;
	unsigned int nlink = 0;
	unsigned long ptr;
	unsigned long ptr_end;
	int name_len;
	u64 ino = btrfs_ino(inode);

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		if (ret < 0)
			break;
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
process_slot:
		btrfs_item_key_to_cpu(path->nodes[0], &key,
				      path->slots[0]);
		if (key.objectid != ino ||
		    key.type != BTRFS_INODE_REF_KEY)
			break;
		ptr = btrfs_item_ptr_offset(path->nodes[0], path->slots[0]);
		ptr_end = ptr + btrfs_item_size(path->nodes[0],
						   path->slots[0]);
		while (ptr < ptr_end) {
			struct btrfs_inode_ref *ref;

			ref = (struct btrfs_inode_ref *)ptr;
			name_len = btrfs_inode_ref_name_len(path->nodes[0],
							    ref);
			ptr = (unsigned long)(ref + 1) + name_len;
			nlink++;
		}

		if (key.offset == 0)
			break;
		if (path->slots[0] > 0) {
			path->slots[0]--;
			goto process_slot;
		}
		key.offset--;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);

	return nlink;
}

 
static noinline int fixup_inode_link_count(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct inode *inode)
{
	struct btrfs_path *path;
	int ret;
	u64 nlink = 0;
	u64 ino = btrfs_ino(BTRFS_I(inode));

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = count_inode_refs(root, BTRFS_I(inode), path);
	if (ret < 0)
		goto out;

	nlink = ret;

	ret = count_inode_extrefs(root, BTRFS_I(inode), path);
	if (ret < 0)
		goto out;

	nlink += ret;

	ret = 0;

	if (nlink != inode->i_nlink) {
		set_nlink(inode, nlink);
		ret = btrfs_update_inode(trans, root, BTRFS_I(inode));
		if (ret)
			goto out;
	}
	BTRFS_I(inode)->index_cnt = (u64)-1;

	if (inode->i_nlink == 0) {
		if (S_ISDIR(inode->i_mode)) {
			ret = replay_dir_deletes(trans, root, NULL, path,
						 ino, 1);
			if (ret)
				goto out;
		}
		ret = btrfs_insert_orphan_item(trans, root, ino);
		if (ret == -EEXIST)
			ret = 0;
	}

out:
	btrfs_free_path(path);
	return ret;
}

static noinline int fixup_inode_link_counts(struct btrfs_trans_handle *trans,
					    struct btrfs_root *root,
					    struct btrfs_path *path)
{
	int ret;
	struct btrfs_key key;
	struct inode *inode;

	key.objectid = BTRFS_TREE_LOG_FIXUP_OBJECTID;
	key.type = BTRFS_ORPHAN_ITEM_KEY;
	key.offset = (u64)-1;
	while (1) {
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0)
			break;

		if (ret == 1) {
			ret = 0;
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
		if (key.objectid != BTRFS_TREE_LOG_FIXUP_OBJECTID ||
		    key.type != BTRFS_ORPHAN_ITEM_KEY)
			break;

		ret = btrfs_del_item(trans, root, path);
		if (ret)
			break;

		btrfs_release_path(path);
		inode = read_one_inode(root, key.offset);
		if (!inode) {
			ret = -EIO;
			break;
		}

		ret = fixup_inode_link_count(trans, root, inode);
		iput(inode);
		if (ret)
			break;

		 
		key.offset = (u64)-1;
	}
	btrfs_release_path(path);
	return ret;
}


 
static noinline int link_to_fixup_dir(struct btrfs_trans_handle *trans,
				      struct btrfs_root *root,
				      struct btrfs_path *path,
				      u64 objectid)
{
	struct btrfs_key key;
	int ret = 0;
	struct inode *inode;

	inode = read_one_inode(root, objectid);
	if (!inode)
		return -EIO;

	key.objectid = BTRFS_TREE_LOG_FIXUP_OBJECTID;
	key.type = BTRFS_ORPHAN_ITEM_KEY;
	key.offset = objectid;

	ret = btrfs_insert_empty_item(trans, root, path, &key, 0);

	btrfs_release_path(path);
	if (ret == 0) {
		if (!inode->i_nlink)
			set_nlink(inode, 1);
		else
			inc_nlink(inode);
		ret = btrfs_update_inode(trans, root, BTRFS_I(inode));
	} else if (ret == -EEXIST) {
		ret = 0;
	}
	iput(inode);

	return ret;
}

 
static noinline int insert_one_name(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    u64 dirid, u64 index,
				    const struct fscrypt_str *name,
				    struct btrfs_key *location)
{
	struct inode *inode;
	struct inode *dir;
	int ret;

	inode = read_one_inode(root, location->objectid);
	if (!inode)
		return -ENOENT;

	dir = read_one_inode(root, dirid);
	if (!dir) {
		iput(inode);
		return -EIO;
	}

	ret = btrfs_add_link(trans, BTRFS_I(dir), BTRFS_I(inode), name,
			     1, index);

	 

	iput(inode);
	iput(dir);
	return ret;
}

static int delete_conflicting_dir_entry(struct btrfs_trans_handle *trans,
					struct btrfs_inode *dir,
					struct btrfs_path *path,
					struct btrfs_dir_item *dst_di,
					const struct btrfs_key *log_key,
					u8 log_flags,
					bool exists)
{
	struct btrfs_key found_key;

	btrfs_dir_item_key_to_cpu(path->nodes[0], dst_di, &found_key);
	 
	if (found_key.objectid == log_key->objectid &&
	    found_key.type == log_key->type &&
	    found_key.offset == log_key->offset &&
	    btrfs_dir_flags(path->nodes[0], dst_di) == log_flags)
		return 1;

	 
	if (!exists)
		return 0;

	return drop_one_dir_item(trans, path, dir, dst_di);
}

 
static noinline int replay_one_name(struct btrfs_trans_handle *trans,
				    struct btrfs_root *root,
				    struct btrfs_path *path,
				    struct extent_buffer *eb,
				    struct btrfs_dir_item *di,
				    struct btrfs_key *key)
{
	struct fscrypt_str name;
	struct btrfs_dir_item *dir_dst_di;
	struct btrfs_dir_item *index_dst_di;
	bool dir_dst_matches = false;
	bool index_dst_matches = false;
	struct btrfs_key log_key;
	struct btrfs_key search_key;
	struct inode *dir;
	u8 log_flags;
	bool exists;
	int ret;
	bool update_size = true;
	bool name_added = false;

	dir = read_one_inode(root, key->objectid);
	if (!dir)
		return -EIO;

	ret = read_alloc_one_name(eb, di + 1, btrfs_dir_name_len(eb, di), &name);
	if (ret)
		goto out;

	log_flags = btrfs_dir_flags(eb, di);
	btrfs_dir_item_key_to_cpu(eb, di, &log_key);
	ret = btrfs_lookup_inode(trans, root, path, &log_key, 0);
	btrfs_release_path(path);
	if (ret < 0)
		goto out;
	exists = (ret == 0);
	ret = 0;

	dir_dst_di = btrfs_lookup_dir_item(trans, root, path, key->objectid,
					   &name, 1);
	if (IS_ERR(dir_dst_di)) {
		ret = PTR_ERR(dir_dst_di);
		goto out;
	} else if (dir_dst_di) {
		ret = delete_conflicting_dir_entry(trans, BTRFS_I(dir), path,
						   dir_dst_di, &log_key,
						   log_flags, exists);
		if (ret < 0)
			goto out;
		dir_dst_matches = (ret == 1);
	}

	btrfs_release_path(path);

	index_dst_di = btrfs_lookup_dir_index_item(trans, root, path,
						   key->objectid, key->offset,
						   &name, 1);
	if (IS_ERR(index_dst_di)) {
		ret = PTR_ERR(index_dst_di);
		goto out;
	} else if (index_dst_di) {
		ret = delete_conflicting_dir_entry(trans, BTRFS_I(dir), path,
						   index_dst_di, &log_key,
						   log_flags, exists);
		if (ret < 0)
			goto out;
		index_dst_matches = (ret == 1);
	}

	btrfs_release_path(path);

	if (dir_dst_matches && index_dst_matches) {
		ret = 0;
		update_size = false;
		goto out;
	}

	 
	search_key.objectid = log_key.objectid;
	search_key.type = BTRFS_INODE_REF_KEY;
	search_key.offset = key->objectid;
	ret = backref_in_log(root->log_root, &search_key, 0, &name);
	if (ret < 0) {
	        goto out;
	} else if (ret) {
	         
	        ret = 0;
	        update_size = false;
	        goto out;
	}

	search_key.objectid = log_key.objectid;
	search_key.type = BTRFS_INODE_EXTREF_KEY;
	search_key.offset = key->objectid;
	ret = backref_in_log(root->log_root, &search_key, key->objectid, &name);
	if (ret < 0) {
		goto out;
	} else if (ret) {
		 
		ret = 0;
		update_size = false;
		goto out;
	}
	btrfs_release_path(path);
	ret = insert_one_name(trans, root, key->objectid, key->offset,
			      &name, &log_key);
	if (ret && ret != -ENOENT && ret != -EEXIST)
		goto out;
	if (!ret)
		name_added = true;
	update_size = false;
	ret = 0;

out:
	if (!ret && update_size) {
		btrfs_i_size_write(BTRFS_I(dir), dir->i_size + name.len * 2);
		ret = btrfs_update_inode(trans, root, BTRFS_I(dir));
	}
	kfree(name.name);
	iput(dir);
	if (!ret && name_added)
		ret = 1;
	return ret;
}

 
static noinline int replay_one_dir_item(struct btrfs_trans_handle *trans,
					struct btrfs_root *root,
					struct btrfs_path *path,
					struct extent_buffer *eb, int slot,
					struct btrfs_key *key)
{
	int ret;
	struct btrfs_dir_item *di;

	 
	ASSERT(key->type == BTRFS_DIR_INDEX_KEY);

	di = btrfs_item_ptr(eb, slot, struct btrfs_dir_item);
	ret = replay_one_name(trans, root, path, eb, di, key);
	if (ret < 0)
		return ret;

	 
	if (ret == 1 && btrfs_dir_ftype(eb, di) != BTRFS_FT_DIR) {
		struct btrfs_path *fixup_path;
		struct btrfs_key di_key;

		fixup_path = btrfs_alloc_path();
		if (!fixup_path)
			return -ENOMEM;

		btrfs_dir_item_key_to_cpu(eb, di, &di_key);
		ret = link_to_fixup_dir(trans, root, fixup_path, di_key.objectid);
		btrfs_free_path(fixup_path);
	}

	return ret;
}

 
static noinline int find_dir_range(struct btrfs_root *root,
				   struct btrfs_path *path,
				   u64 dirid,
				   u64 *start_ret, u64 *end_ret)
{
	struct btrfs_key key;
	u64 found_end;
	struct btrfs_dir_log_item *item;
	int ret;
	int nritems;

	if (*start_ret == (u64)-1)
		return 1;

	key.objectid = dirid;
	key.type = BTRFS_DIR_LOG_INDEX_KEY;
	key.offset = *start_ret;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret > 0) {
		if (path->slots[0] == 0)
			goto out;
		path->slots[0]--;
	}
	if (ret != 0)
		btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

	if (key.type != BTRFS_DIR_LOG_INDEX_KEY || key.objectid != dirid) {
		ret = 1;
		goto next;
	}
	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	found_end = btrfs_dir_log_end(path->nodes[0], item);

	if (*start_ret >= key.offset && *start_ret <= found_end) {
		ret = 0;
		*start_ret = key.offset;
		*end_ret = found_end;
		goto out;
	}
	ret = 1;
next:
	 
	nritems = btrfs_header_nritems(path->nodes[0]);
	path->slots[0]++;
	if (path->slots[0] >= nritems) {
		ret = btrfs_next_leaf(root, path);
		if (ret)
			goto out;
	}

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);

	if (key.type != BTRFS_DIR_LOG_INDEX_KEY || key.objectid != dirid) {
		ret = 1;
		goto out;
	}
	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	found_end = btrfs_dir_log_end(path->nodes[0], item);
	*start_ret = key.offset;
	*end_ret = found_end;
	ret = 0;
out:
	btrfs_release_path(path);
	return ret;
}

 
static noinline int check_item_in_log(struct btrfs_trans_handle *trans,
				      struct btrfs_root *log,
				      struct btrfs_path *path,
				      struct btrfs_path *log_path,
				      struct inode *dir,
				      struct btrfs_key *dir_key)
{
	struct btrfs_root *root = BTRFS_I(dir)->root;
	int ret;
	struct extent_buffer *eb;
	int slot;
	struct btrfs_dir_item *di;
	struct fscrypt_str name;
	struct inode *inode = NULL;
	struct btrfs_key location;

	 
	ASSERT(dir_key->type == BTRFS_DIR_INDEX_KEY);

	eb = path->nodes[0];
	slot = path->slots[0];
	di = btrfs_item_ptr(eb, slot, struct btrfs_dir_item);
	ret = read_alloc_one_name(eb, di + 1, btrfs_dir_name_len(eb, di), &name);
	if (ret)
		goto out;

	if (log) {
		struct btrfs_dir_item *log_di;

		log_di = btrfs_lookup_dir_index_item(trans, log, log_path,
						     dir_key->objectid,
						     dir_key->offset, &name, 0);
		if (IS_ERR(log_di)) {
			ret = PTR_ERR(log_di);
			goto out;
		} else if (log_di) {
			 
			ret = 0;
			goto out;
		}
	}

	btrfs_dir_item_key_to_cpu(eb, di, &location);
	btrfs_release_path(path);
	btrfs_release_path(log_path);
	inode = read_one_inode(root, location.objectid);
	if (!inode) {
		ret = -EIO;
		goto out;
	}

	ret = link_to_fixup_dir(trans, root, path, location.objectid);
	if (ret)
		goto out;

	inc_nlink(inode);
	ret = unlink_inode_for_log_replay(trans, BTRFS_I(dir), BTRFS_I(inode),
					  &name);
	 
out:
	btrfs_release_path(path);
	btrfs_release_path(log_path);
	kfree(name.name);
	iput(inode);
	return ret;
}

static int replay_xattr_deletes(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct btrfs_root *log,
			      struct btrfs_path *path,
			      const u64 ino)
{
	struct btrfs_key search_key;
	struct btrfs_path *log_path;
	int i;
	int nritems;
	int ret;

	log_path = btrfs_alloc_path();
	if (!log_path)
		return -ENOMEM;

	search_key.objectid = ino;
	search_key.type = BTRFS_XATTR_ITEM_KEY;
	search_key.offset = 0;
again:
	ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	if (ret < 0)
		goto out;
process_leaf:
	nritems = btrfs_header_nritems(path->nodes[0]);
	for (i = path->slots[0]; i < nritems; i++) {
		struct btrfs_key key;
		struct btrfs_dir_item *di;
		struct btrfs_dir_item *log_di;
		u32 total_size;
		u32 cur;

		btrfs_item_key_to_cpu(path->nodes[0], &key, i);
		if (key.objectid != ino || key.type != BTRFS_XATTR_ITEM_KEY) {
			ret = 0;
			goto out;
		}

		di = btrfs_item_ptr(path->nodes[0], i, struct btrfs_dir_item);
		total_size = btrfs_item_size(path->nodes[0], i);
		cur = 0;
		while (cur < total_size) {
			u16 name_len = btrfs_dir_name_len(path->nodes[0], di);
			u16 data_len = btrfs_dir_data_len(path->nodes[0], di);
			u32 this_len = sizeof(*di) + name_len + data_len;
			char *name;

			name = kmalloc(name_len, GFP_NOFS);
			if (!name) {
				ret = -ENOMEM;
				goto out;
			}
			read_extent_buffer(path->nodes[0], name,
					   (unsigned long)(di + 1), name_len);

			log_di = btrfs_lookup_xattr(NULL, log, log_path, ino,
						    name, name_len, 0);
			btrfs_release_path(log_path);
			if (!log_di) {
				 
				btrfs_release_path(path);
				di = btrfs_lookup_xattr(trans, root, path, ino,
							name, name_len, -1);
				kfree(name);
				if (IS_ERR(di)) {
					ret = PTR_ERR(di);
					goto out;
				}
				ASSERT(di);
				ret = btrfs_delete_one_dir_name(trans, root,
								path, di);
				if (ret)
					goto out;
				btrfs_release_path(path);
				search_key = key;
				goto again;
			}
			kfree(name);
			if (IS_ERR(log_di)) {
				ret = PTR_ERR(log_di);
				goto out;
			}
			cur += this_len;
			di = (struct btrfs_dir_item *)((char *)di + this_len);
		}
	}
	ret = btrfs_next_leaf(root, path);
	if (ret > 0)
		ret = 0;
	else if (ret == 0)
		goto process_leaf;
out:
	btrfs_free_path(log_path);
	btrfs_release_path(path);
	return ret;
}


 
static noinline int replay_dir_deletes(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid, int del_all)
{
	u64 range_start;
	u64 range_end;
	int ret = 0;
	struct btrfs_key dir_key;
	struct btrfs_key found_key;
	struct btrfs_path *log_path;
	struct inode *dir;

	dir_key.objectid = dirid;
	dir_key.type = BTRFS_DIR_INDEX_KEY;
	log_path = btrfs_alloc_path();
	if (!log_path)
		return -ENOMEM;

	dir = read_one_inode(root, dirid);
	 
	if (!dir) {
		btrfs_free_path(log_path);
		return 0;
	}

	range_start = 0;
	range_end = 0;
	while (1) {
		if (del_all)
			range_end = (u64)-1;
		else {
			ret = find_dir_range(log, path, dirid,
					     &range_start, &range_end);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
		}

		dir_key.offset = range_start;
		while (1) {
			int nritems;
			ret = btrfs_search_slot(NULL, root, &dir_key, path,
						0, 0);
			if (ret < 0)
				goto out;

			nritems = btrfs_header_nritems(path->nodes[0]);
			if (path->slots[0] >= nritems) {
				ret = btrfs_next_leaf(root, path);
				if (ret == 1)
					break;
				else if (ret < 0)
					goto out;
			}
			btrfs_item_key_to_cpu(path->nodes[0], &found_key,
					      path->slots[0]);
			if (found_key.objectid != dirid ||
			    found_key.type != dir_key.type) {
				ret = 0;
				goto out;
			}

			if (found_key.offset > range_end)
				break;

			ret = check_item_in_log(trans, log, path,
						log_path, dir,
						&found_key);
			if (ret)
				goto out;
			if (found_key.offset == (u64)-1)
				break;
			dir_key.offset = found_key.offset + 1;
		}
		btrfs_release_path(path);
		if (range_end == (u64)-1)
			break;
		range_start = range_end + 1;
	}
	ret = 0;
out:
	btrfs_release_path(path);
	btrfs_free_path(log_path);
	iput(dir);
	return ret;
}

 
static int replay_one_buffer(struct btrfs_root *log, struct extent_buffer *eb,
			     struct walk_control *wc, u64 gen, int level)
{
	int nritems;
	struct btrfs_tree_parent_check check = {
		.transid = gen,
		.level = level
	};
	struct btrfs_path *path;
	struct btrfs_root *root = wc->replay_dest;
	struct btrfs_key key;
	int i;
	int ret;

	ret = btrfs_read_extent_buffer(eb, &check);
	if (ret)
		return ret;

	level = btrfs_header_level(eb);

	if (level != 0)
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	nritems = btrfs_header_nritems(eb);
	for (i = 0; i < nritems; i++) {
		btrfs_item_key_to_cpu(eb, &key, i);

		 
		if (key.type == BTRFS_INODE_ITEM_KEY &&
		    wc->stage == LOG_WALK_REPLAY_INODES) {
			struct btrfs_inode_item *inode_item;
			u32 mode;

			inode_item = btrfs_item_ptr(eb, i,
					    struct btrfs_inode_item);
			 
			if (btrfs_inode_nlink(eb, inode_item) == 0) {
				wc->ignore_cur_inode = true;
				continue;
			} else {
				wc->ignore_cur_inode = false;
			}
			ret = replay_xattr_deletes(wc->trans, root, log,
						   path, key.objectid);
			if (ret)
				break;
			mode = btrfs_inode_mode(eb, inode_item);
			if (S_ISDIR(mode)) {
				ret = replay_dir_deletes(wc->trans,
					 root, log, path, key.objectid, 0);
				if (ret)
					break;
			}
			ret = overwrite_item(wc->trans, root, path,
					     eb, i, &key);
			if (ret)
				break;

			 
			if (S_ISREG(mode)) {
				struct btrfs_drop_extents_args drop_args = { 0 };
				struct inode *inode;
				u64 from;

				inode = read_one_inode(root, key.objectid);
				if (!inode) {
					ret = -EIO;
					break;
				}
				from = ALIGN(i_size_read(inode),
					     root->fs_info->sectorsize);
				drop_args.start = from;
				drop_args.end = (u64)-1;
				drop_args.drop_cache = true;
				ret = btrfs_drop_extents(wc->trans, root,
							 BTRFS_I(inode),
							 &drop_args);
				if (!ret) {
					inode_sub_bytes(inode,
							drop_args.bytes_found);
					 
					ret = btrfs_update_inode(wc->trans,
							root, BTRFS_I(inode));
				}
				iput(inode);
				if (ret)
					break;
			}

			ret = link_to_fixup_dir(wc->trans, root,
						path, key.objectid);
			if (ret)
				break;
		}

		if (wc->ignore_cur_inode)
			continue;

		if (key.type == BTRFS_DIR_INDEX_KEY &&
		    wc->stage == LOG_WALK_REPLAY_DIR_INDEX) {
			ret = replay_one_dir_item(wc->trans, root, path,
						  eb, i, &key);
			if (ret)
				break;
		}

		if (wc->stage < LOG_WALK_REPLAY_ALL)
			continue;

		 
		if (key.type == BTRFS_XATTR_ITEM_KEY) {
			ret = overwrite_item(wc->trans, root, path,
					     eb, i, &key);
			if (ret)
				break;
		} else if (key.type == BTRFS_INODE_REF_KEY ||
			   key.type == BTRFS_INODE_EXTREF_KEY) {
			ret = add_inode_ref(wc->trans, root, log, path,
					    eb, i, &key);
			if (ret && ret != -ENOENT)
				break;
			ret = 0;
		} else if (key.type == BTRFS_EXTENT_DATA_KEY) {
			ret = replay_one_extent(wc->trans, root, path,
						eb, i, &key);
			if (ret)
				break;
		}
		 
	}
	btrfs_free_path(path);
	return ret;
}

 
static void unaccount_log_buffer(struct btrfs_fs_info *fs_info, u64 start)
{
	struct btrfs_block_group *cache;

	cache = btrfs_lookup_block_group(fs_info, start);
	if (!cache) {
		btrfs_err(fs_info, "unable to find block group for %llu", start);
		return;
	}

	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	cache->reserved -= fs_info->nodesize;
	cache->space_info->bytes_reserved -= fs_info->nodesize;
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);

	btrfs_put_block_group(cache);
}

static int clean_log_buffer(struct btrfs_trans_handle *trans,
			    struct extent_buffer *eb)
{
	int ret;

	btrfs_tree_lock(eb);
	btrfs_clear_buffer_dirty(trans, eb);
	wait_on_extent_buffer_writeback(eb);
	btrfs_tree_unlock(eb);

	if (trans) {
		ret = btrfs_pin_reserved_extent(trans, eb->start, eb->len);
		if (ret)
			return ret;
		btrfs_redirty_list_add(trans->transaction, eb);
	} else {
		unaccount_log_buffer(eb->fs_info, eb->start);
	}

	return 0;
}

static noinline int walk_down_log_tree(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path, int *level,
				   struct walk_control *wc)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 bytenr;
	u64 ptr_gen;
	struct extent_buffer *next;
	struct extent_buffer *cur;
	int ret = 0;

	while (*level > 0) {
		struct btrfs_tree_parent_check check = { 0 };

		cur = path->nodes[*level];

		WARN_ON(btrfs_header_level(cur) != *level);

		if (path->slots[*level] >=
		    btrfs_header_nritems(cur))
			break;

		bytenr = btrfs_node_blockptr(cur, path->slots[*level]);
		ptr_gen = btrfs_node_ptr_generation(cur, path->slots[*level]);
		check.transid = ptr_gen;
		check.level = *level - 1;
		check.has_first_key = true;
		btrfs_node_key_to_cpu(cur, &check.first_key, path->slots[*level]);

		next = btrfs_find_create_tree_block(fs_info, bytenr,
						    btrfs_header_owner(cur),
						    *level - 1);
		if (IS_ERR(next))
			return PTR_ERR(next);

		if (*level == 1) {
			ret = wc->process_func(root, next, wc, ptr_gen,
					       *level - 1);
			if (ret) {
				free_extent_buffer(next);
				return ret;
			}

			path->slots[*level]++;
			if (wc->free) {
				ret = btrfs_read_extent_buffer(next, &check);
				if (ret) {
					free_extent_buffer(next);
					return ret;
				}

				ret = clean_log_buffer(trans, next);
				if (ret) {
					free_extent_buffer(next);
					return ret;
				}
			}
			free_extent_buffer(next);
			continue;
		}
		ret = btrfs_read_extent_buffer(next, &check);
		if (ret) {
			free_extent_buffer(next);
			return ret;
		}

		if (path->nodes[*level-1])
			free_extent_buffer(path->nodes[*level-1]);
		path->nodes[*level-1] = next;
		*level = btrfs_header_level(next);
		path->slots[*level] = 0;
		cond_resched();
	}
	path->slots[*level] = btrfs_header_nritems(path->nodes[*level]);

	cond_resched();
	return 0;
}

static noinline int walk_up_log_tree(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path, int *level,
				 struct walk_control *wc)
{
	int i;
	int slot;
	int ret;

	for (i = *level; i < BTRFS_MAX_LEVEL - 1 && path->nodes[i]; i++) {
		slot = path->slots[i];
		if (slot + 1 < btrfs_header_nritems(path->nodes[i])) {
			path->slots[i]++;
			*level = i;
			WARN_ON(*level == 0);
			return 0;
		} else {
			ret = wc->process_func(root, path->nodes[*level], wc,
				 btrfs_header_generation(path->nodes[*level]),
				 *level);
			if (ret)
				return ret;

			if (wc->free) {
				ret = clean_log_buffer(trans, path->nodes[*level]);
				if (ret)
					return ret;
			}
			free_extent_buffer(path->nodes[*level]);
			path->nodes[*level] = NULL;
			*level = i + 1;
		}
	}
	return 1;
}

 
static int walk_log_tree(struct btrfs_trans_handle *trans,
			 struct btrfs_root *log, struct walk_control *wc)
{
	int ret = 0;
	int wret;
	int level;
	struct btrfs_path *path;
	int orig_level;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	level = btrfs_header_level(log->node);
	orig_level = level;
	path->nodes[level] = log->node;
	atomic_inc(&log->node->refs);
	path->slots[level] = 0;

	while (1) {
		wret = walk_down_log_tree(trans, log, path, &level, wc);
		if (wret > 0)
			break;
		if (wret < 0) {
			ret = wret;
			goto out;
		}

		wret = walk_up_log_tree(trans, log, path, &level, wc);
		if (wret > 0)
			break;
		if (wret < 0) {
			ret = wret;
			goto out;
		}
	}

	 
	if (path->nodes[orig_level]) {
		ret = wc->process_func(log, path->nodes[orig_level], wc,
			 btrfs_header_generation(path->nodes[orig_level]),
			 orig_level);
		if (ret)
			goto out;
		if (wc->free)
			ret = clean_log_buffer(trans, path->nodes[orig_level]);
	}

out:
	btrfs_free_path(path);
	return ret;
}

 
static int update_log_root(struct btrfs_trans_handle *trans,
			   struct btrfs_root *log,
			   struct btrfs_root_item *root_item)
{
	struct btrfs_fs_info *fs_info = log->fs_info;
	int ret;

	if (log->log_transid == 1) {
		 
		ret = btrfs_insert_root(trans, fs_info->log_root_tree,
				&log->root_key, root_item);
	} else {
		ret = btrfs_update_root(trans, fs_info->log_root_tree,
				&log->root_key, root_item);
	}
	return ret;
}

static void wait_log_commit(struct btrfs_root *root, int transid)
{
	DEFINE_WAIT(wait);
	int index = transid % 2;

	 
	for (;;) {
		prepare_to_wait(&root->log_commit_wait[index],
				&wait, TASK_UNINTERRUPTIBLE);

		if (!(root->log_transid_committed < transid &&
		      atomic_read(&root->log_commit[index])))
			break;

		mutex_unlock(&root->log_mutex);
		schedule();
		mutex_lock(&root->log_mutex);
	}
	finish_wait(&root->log_commit_wait[index], &wait);
}

static void wait_for_writer(struct btrfs_root *root)
{
	DEFINE_WAIT(wait);

	for (;;) {
		prepare_to_wait(&root->log_writer_wait, &wait,
				TASK_UNINTERRUPTIBLE);
		if (!atomic_read(&root->log_writers))
			break;

		mutex_unlock(&root->log_mutex);
		schedule();
		mutex_lock(&root->log_mutex);
	}
	finish_wait(&root->log_writer_wait, &wait);
}

static inline void btrfs_remove_log_ctx(struct btrfs_root *root,
					struct btrfs_log_ctx *ctx)
{
	mutex_lock(&root->log_mutex);
	list_del_init(&ctx->list);
	mutex_unlock(&root->log_mutex);
}

 
static inline void btrfs_remove_all_log_ctxs(struct btrfs_root *root,
					     int index, int error)
{
	struct btrfs_log_ctx *ctx;
	struct btrfs_log_ctx *safe;

	list_for_each_entry_safe(ctx, safe, &root->log_ctxs[index], list) {
		list_del_init(&ctx->list);
		ctx->log_ret = error;
	}
}

 
int btrfs_sync_log(struct btrfs_trans_handle *trans,
		   struct btrfs_root *root, struct btrfs_log_ctx *ctx)
{
	int index1;
	int index2;
	int mark;
	int ret;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_root *log = root->log_root;
	struct btrfs_root *log_root_tree = fs_info->log_root_tree;
	struct btrfs_root_item new_root_item;
	int log_transid = 0;
	struct btrfs_log_ctx root_log_ctx;
	struct blk_plug plug;
	u64 log_root_start;
	u64 log_root_level;

	mutex_lock(&root->log_mutex);
	log_transid = ctx->log_transid;
	if (root->log_transid_committed >= log_transid) {
		mutex_unlock(&root->log_mutex);
		return ctx->log_ret;
	}

	index1 = log_transid % 2;
	if (atomic_read(&root->log_commit[index1])) {
		wait_log_commit(root, log_transid);
		mutex_unlock(&root->log_mutex);
		return ctx->log_ret;
	}
	ASSERT(log_transid == root->log_transid);
	atomic_set(&root->log_commit[index1], 1);

	 
	if (atomic_read(&root->log_commit[(index1 + 1) % 2]))
		wait_log_commit(root, log_transid - 1);

	while (1) {
		int batch = atomic_read(&root->log_batch);
		 
		if (!btrfs_test_opt(fs_info, SSD) &&
		    test_bit(BTRFS_ROOT_MULTI_LOG_TASKS, &root->state)) {
			mutex_unlock(&root->log_mutex);
			schedule_timeout_uninterruptible(1);
			mutex_lock(&root->log_mutex);
		}
		wait_for_writer(root);
		if (batch == atomic_read(&root->log_batch))
			break;
	}

	 
	if (btrfs_need_log_full_commit(trans)) {
		ret = BTRFS_LOG_FORCE_COMMIT;
		mutex_unlock(&root->log_mutex);
		goto out;
	}

	if (log_transid % 2 == 0)
		mark = EXTENT_DIRTY;
	else
		mark = EXTENT_NEW;

	 
	blk_start_plug(&plug);
	ret = btrfs_write_marked_extents(fs_info, &log->dirty_log_pages, mark);
	 
	if (ret == -EAGAIN && btrfs_is_zoned(fs_info))
		ret = 0;
	if (ret) {
		blk_finish_plug(&plug);
		btrfs_set_log_full_commit(trans);
		mutex_unlock(&root->log_mutex);
		goto out;
	}

	 
	btrfs_set_root_node(&log->root_item, log->node);
	memcpy(&new_root_item, &log->root_item, sizeof(new_root_item));

	root->log_transid++;
	log->log_transid = root->log_transid;
	root->log_start_pid = 0;
	 
	mutex_unlock(&root->log_mutex);

	if (btrfs_is_zoned(fs_info)) {
		mutex_lock(&fs_info->tree_root->log_mutex);
		if (!log_root_tree->node) {
			ret = btrfs_alloc_log_tree_node(trans, log_root_tree);
			if (ret) {
				mutex_unlock(&fs_info->tree_root->log_mutex);
				blk_finish_plug(&plug);
				goto out;
			}
		}
		mutex_unlock(&fs_info->tree_root->log_mutex);
	}

	btrfs_init_log_ctx(&root_log_ctx, NULL);

	mutex_lock(&log_root_tree->log_mutex);

	index2 = log_root_tree->log_transid % 2;
	list_add_tail(&root_log_ctx.list, &log_root_tree->log_ctxs[index2]);
	root_log_ctx.log_transid = log_root_tree->log_transid;

	 
	ret = update_log_root(trans, log, &new_root_item);
	if (ret) {
		if (!list_empty(&root_log_ctx.list))
			list_del_init(&root_log_ctx.list);

		blk_finish_plug(&plug);
		btrfs_set_log_full_commit(trans);
		if (ret != -ENOSPC)
			btrfs_err(fs_info,
				  "failed to update log for root %llu ret %d",
				  root->root_key.objectid, ret);
		btrfs_wait_tree_log_extents(log, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out;
	}

	if (log_root_tree->log_transid_committed >= root_log_ctx.log_transid) {
		blk_finish_plug(&plug);
		list_del_init(&root_log_ctx.list);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = root_log_ctx.log_ret;
		goto out;
	}

	index2 = root_log_ctx.log_transid % 2;
	if (atomic_read(&log_root_tree->log_commit[index2])) {
		blk_finish_plug(&plug);
		ret = btrfs_wait_tree_log_extents(log, mark);
		wait_log_commit(log_root_tree,
				root_log_ctx.log_transid);
		mutex_unlock(&log_root_tree->log_mutex);
		if (!ret)
			ret = root_log_ctx.log_ret;
		goto out;
	}
	ASSERT(root_log_ctx.log_transid == log_root_tree->log_transid);
	atomic_set(&log_root_tree->log_commit[index2], 1);

	if (atomic_read(&log_root_tree->log_commit[(index2 + 1) % 2])) {
		wait_log_commit(log_root_tree,
				root_log_ctx.log_transid - 1);
	}

	 
	if (btrfs_need_log_full_commit(trans)) {
		blk_finish_plug(&plug);
		btrfs_wait_tree_log_extents(log, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		ret = BTRFS_LOG_FORCE_COMMIT;
		goto out_wake_log_root;
	}

	ret = btrfs_write_marked_extents(fs_info,
					 &log_root_tree->dirty_log_pages,
					 EXTENT_DIRTY | EXTENT_NEW);
	blk_finish_plug(&plug);
	 
	if (ret == -EAGAIN && btrfs_is_zoned(fs_info)) {
		btrfs_set_log_full_commit(trans);
		btrfs_wait_tree_log_extents(log, mark);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	} else if (ret) {
		btrfs_set_log_full_commit(trans);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	}
	ret = btrfs_wait_tree_log_extents(log, mark);
	if (!ret)
		ret = btrfs_wait_tree_log_extents(log_root_tree,
						  EXTENT_NEW | EXTENT_DIRTY);
	if (ret) {
		btrfs_set_log_full_commit(trans);
		mutex_unlock(&log_root_tree->log_mutex);
		goto out_wake_log_root;
	}

	log_root_start = log_root_tree->node->start;
	log_root_level = btrfs_header_level(log_root_tree->node);
	log_root_tree->log_transid++;
	mutex_unlock(&log_root_tree->log_mutex);

	 
	mutex_lock(&fs_info->tree_log_mutex);

	 
	if (BTRFS_FS_ERROR(fs_info)) {
		ret = -EIO;
		btrfs_set_log_full_commit(trans);
		btrfs_abort_transaction(trans, ret);
		mutex_unlock(&fs_info->tree_log_mutex);
		goto out_wake_log_root;
	}

	btrfs_set_super_log_root(fs_info->super_for_commit, log_root_start);
	btrfs_set_super_log_root_level(fs_info->super_for_commit, log_root_level);
	ret = write_all_supers(fs_info, 1);
	mutex_unlock(&fs_info->tree_log_mutex);
	if (ret) {
		btrfs_set_log_full_commit(trans);
		btrfs_abort_transaction(trans, ret);
		goto out_wake_log_root;
	}

	 
	ASSERT(root->last_log_commit <= log_transid);
	root->last_log_commit = log_transid;

out_wake_log_root:
	mutex_lock(&log_root_tree->log_mutex);
	btrfs_remove_all_log_ctxs(log_root_tree, index2, ret);

	log_root_tree->log_transid_committed++;
	atomic_set(&log_root_tree->log_commit[index2], 0);
	mutex_unlock(&log_root_tree->log_mutex);

	 
	cond_wake_up(&log_root_tree->log_commit_wait[index2]);
out:
	mutex_lock(&root->log_mutex);
	btrfs_remove_all_log_ctxs(root, index1, ret);
	root->log_transid_committed++;
	atomic_set(&root->log_commit[index1], 0);
	mutex_unlock(&root->log_mutex);

	 
	cond_wake_up(&root->log_commit_wait[index1]);
	return ret;
}

static void free_log_tree(struct btrfs_trans_handle *trans,
			  struct btrfs_root *log)
{
	int ret;
	struct walk_control wc = {
		.free = 1,
		.process_func = process_one_buffer
	};

	if (log->node) {
		ret = walk_log_tree(trans, log, &wc);
		if (ret) {
			 
			set_bit(BTRFS_FS_STATE_LOG_CLEANUP_ERROR,
				&log->fs_info->fs_state);

			 
			btrfs_write_marked_extents(log->fs_info,
						   &log->dirty_log_pages,
						   EXTENT_DIRTY | EXTENT_NEW);
			btrfs_wait_tree_log_extents(log,
						    EXTENT_DIRTY | EXTENT_NEW);

			if (trans)
				btrfs_abort_transaction(trans, ret);
			else
				btrfs_handle_fs_error(log->fs_info, ret, NULL);
		}
	}

	clear_extent_bits(&log->dirty_log_pages, 0, (u64)-1,
			  EXTENT_DIRTY | EXTENT_NEW | EXTENT_NEED_WAIT);
	extent_io_tree_release(&log->log_csum_range);

	btrfs_put_root(log);
}

 
int btrfs_free_log(struct btrfs_trans_handle *trans, struct btrfs_root *root)
{
	if (root->log_root) {
		free_log_tree(trans, root->log_root);
		root->log_root = NULL;
		clear_bit(BTRFS_ROOT_HAS_LOG_TREE, &root->state);
	}
	return 0;
}

int btrfs_free_log_root_tree(struct btrfs_trans_handle *trans,
			     struct btrfs_fs_info *fs_info)
{
	if (fs_info->log_root_tree) {
		free_log_tree(trans, fs_info->log_root_tree);
		fs_info->log_root_tree = NULL;
		clear_bit(BTRFS_ROOT_HAS_LOG_TREE, &fs_info->tree_root->state);
	}
	return 0;
}

 
static int inode_logged(const struct btrfs_trans_handle *trans,
			struct btrfs_inode *inode,
			struct btrfs_path *path_in)
{
	struct btrfs_path *path = path_in;
	struct btrfs_key key;
	int ret;

	if (inode->logged_trans == trans->transid)
		return 1;

	 
	if (inode->logged_trans > 0)
		return 0;

	 
	if (!test_bit(BTRFS_ROOT_HAS_LOG_TREE, &inode->root->state)) {
		inode->logged_trans = trans->transid - 1;
		return 0;
	}

	 
	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	if (!path) {
		path = btrfs_alloc_path();
		if (!path)
			return -ENOMEM;
	}

	ret = btrfs_search_slot(NULL, inode->root->log_root, &key, path, 0, 0);

	if (path_in)
		btrfs_release_path(path);
	else
		btrfs_free_path(path);

	 
	if (ret < 0) {
		return ret;
	} else if (ret > 0) {
		 
		inode->logged_trans = trans->transid - 1;
		return 0;
	}

	 
	inode->logged_trans = trans->transid;

	 
	if (S_ISDIR(inode->vfs_inode.i_mode))
		inode->last_dir_index_offset = (u64)-1;

	return 1;
}

 
static int del_logged_dentry(struct btrfs_trans_handle *trans,
			     struct btrfs_root *log,
			     struct btrfs_path *path,
			     u64 dir_ino,
			     const struct fscrypt_str *name,
			     u64 index)
{
	struct btrfs_dir_item *di;

	 
	di = btrfs_lookup_dir_index_item(trans, log, path, dir_ino,
					 index, name, -1);
	if (IS_ERR(di))
		return PTR_ERR(di);
	else if (!di)
		return 1;

	 
	return btrfs_delete_one_dir_name(trans, log, path, di);
}

 
void btrfs_del_dir_entries_in_log(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  const struct fscrypt_str *name,
				  struct btrfs_inode *dir, u64 index)
{
	struct btrfs_path *path;
	int ret;

	ret = inode_logged(trans, dir, NULL);
	if (ret == 0)
		return;
	else if (ret < 0) {
		btrfs_set_log_full_commit(trans);
		return;
	}

	ret = join_running_log_trans(root);
	if (ret)
		return;

	mutex_lock(&dir->log_mutex);

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	ret = del_logged_dentry(trans, root->log_root, path, btrfs_ino(dir),
				name, index);
	btrfs_free_path(path);
out_unlock:
	mutex_unlock(&dir->log_mutex);
	if (ret < 0)
		btrfs_set_log_full_commit(trans);
	btrfs_end_log_trans(root);
}

 
void btrfs_del_inode_ref_in_log(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				const struct fscrypt_str *name,
				struct btrfs_inode *inode, u64 dirid)
{
	struct btrfs_root *log;
	u64 index;
	int ret;

	ret = inode_logged(trans, inode, NULL);
	if (ret == 0)
		return;
	else if (ret < 0) {
		btrfs_set_log_full_commit(trans);
		return;
	}

	ret = join_running_log_trans(root);
	if (ret)
		return;
	log = root->log_root;
	mutex_lock(&inode->log_mutex);

	ret = btrfs_del_inode_ref(trans, log, name, btrfs_ino(inode),
				  dirid, &index);
	mutex_unlock(&inode->log_mutex);
	if (ret < 0 && ret != -ENOENT)
		btrfs_set_log_full_commit(trans);
	btrfs_end_log_trans(root);
}

 
static noinline int insert_dir_log_key(struct btrfs_trans_handle *trans,
				       struct btrfs_root *log,
				       struct btrfs_path *path,
				       u64 dirid,
				       u64 first_offset, u64 last_offset)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_dir_log_item *item;

	key.objectid = dirid;
	key.offset = first_offset;
	key.type = BTRFS_DIR_LOG_INDEX_KEY;
	ret = btrfs_insert_empty_item(trans, log, path, &key, sizeof(*item));
	 
	if (ret && ret != -EEXIST)
		return ret;

	item = btrfs_item_ptr(path->nodes[0], path->slots[0],
			      struct btrfs_dir_log_item);
	if (ret == -EEXIST) {
		const u64 curr_end = btrfs_dir_log_end(path->nodes[0], item);

		 
		last_offset = max(last_offset, curr_end);
	}
	btrfs_set_dir_log_end(path->nodes[0], item, last_offset);
	btrfs_mark_buffer_dirty(trans, path->nodes[0]);
	btrfs_release_path(path);
	return 0;
}

static int flush_dir_items_batch(struct btrfs_trans_handle *trans,
				 struct btrfs_inode *inode,
				 struct extent_buffer *src,
				 struct btrfs_path *dst_path,
				 int start_slot,
				 int count)
{
	struct btrfs_root *log = inode->root->log_root;
	char *ins_data = NULL;
	struct btrfs_item_batch batch;
	struct extent_buffer *dst;
	unsigned long src_offset;
	unsigned long dst_offset;
	u64 last_index;
	struct btrfs_key key;
	u32 item_size;
	int ret;
	int i;

	ASSERT(count > 0);
	batch.nr = count;

	if (count == 1) {
		btrfs_item_key_to_cpu(src, &key, start_slot);
		item_size = btrfs_item_size(src, start_slot);
		batch.keys = &key;
		batch.data_sizes = &item_size;
		batch.total_data_size = item_size;
	} else {
		struct btrfs_key *ins_keys;
		u32 *ins_sizes;

		ins_data = kmalloc(count * sizeof(u32) +
				   count * sizeof(struct btrfs_key), GFP_NOFS);
		if (!ins_data)
			return -ENOMEM;

		ins_sizes = (u32 *)ins_data;
		ins_keys = (struct btrfs_key *)(ins_data + count * sizeof(u32));
		batch.keys = ins_keys;
		batch.data_sizes = ins_sizes;
		batch.total_data_size = 0;

		for (i = 0; i < count; i++) {
			const int slot = start_slot + i;

			btrfs_item_key_to_cpu(src, &ins_keys[i], slot);
			ins_sizes[i] = btrfs_item_size(src, slot);
			batch.total_data_size += ins_sizes[i];
		}
	}

	ret = btrfs_insert_empty_items(trans, log, dst_path, &batch);
	if (ret)
		goto out;

	dst = dst_path->nodes[0];
	 
	dst_offset = btrfs_item_ptr_offset(dst, dst_path->slots[0] + count - 1);
	src_offset = btrfs_item_ptr_offset(src, start_slot + count - 1);
	copy_extent_buffer(dst, src, dst_offset, src_offset, batch.total_data_size);
	btrfs_release_path(dst_path);

	last_index = batch.keys[count - 1].offset;
	ASSERT(last_index > inode->last_dir_index_offset);

	 
	if (WARN_ON(last_index <= inode->last_dir_index_offset))
		ret = BTRFS_LOG_FORCE_COMMIT;
	else
		inode->last_dir_index_offset = last_index;

	if (btrfs_get_first_dir_index_to_log(inode) == 0)
		btrfs_set_first_dir_index_to_log(inode, batch.keys[0].offset);
out:
	kfree(ins_data);

	return ret;
}

static int process_dir_items_leaf(struct btrfs_trans_handle *trans,
				  struct btrfs_inode *inode,
				  struct btrfs_path *path,
				  struct btrfs_path *dst_path,
				  struct btrfs_log_ctx *ctx,
				  u64 *last_old_dentry_offset)
{
	struct btrfs_root *log = inode->root->log_root;
	struct extent_buffer *src;
	const int nritems = btrfs_header_nritems(path->nodes[0]);
	const u64 ino = btrfs_ino(inode);
	bool last_found = false;
	int batch_start = 0;
	int batch_size = 0;
	int i;

	 
	src = btrfs_clone_extent_buffer(path->nodes[0]);
	if (!src)
		return -ENOMEM;

	i = path->slots[0];
	btrfs_release_path(path);
	path->nodes[0] = src;
	path->slots[0] = i;

	for (; i < nritems; i++) {
		struct btrfs_dir_item *di;
		struct btrfs_key key;
		int ret;

		btrfs_item_key_to_cpu(src, &key, i);

		if (key.objectid != ino || key.type != BTRFS_DIR_INDEX_KEY) {
			last_found = true;
			break;
		}

		di = btrfs_item_ptr(src, i, struct btrfs_dir_item);

		 
		if (btrfs_dir_transid(src, di) < trans->transid) {
			if (key.offset > *last_old_dentry_offset + 1) {
				ret = insert_dir_log_key(trans, log, dst_path,
						 ino, *last_old_dentry_offset + 1,
						 key.offset - 1);
				if (ret < 0)
					return ret;
			}

			*last_old_dentry_offset = key.offset;
			continue;
		}

		 
		if (key.offset <= inode->last_dir_index_offset)
			continue;

		 
		if (!ctx->log_new_dentries) {
			struct btrfs_key di_key;

			btrfs_dir_item_key_to_cpu(src, di, &di_key);
			if (di_key.type != BTRFS_ROOT_ITEM_KEY)
				ctx->log_new_dentries = true;
		}

		if (batch_size == 0)
			batch_start = i;
		batch_size++;
	}

	if (batch_size > 0) {
		int ret;

		ret = flush_dir_items_batch(trans, inode, src, dst_path,
					    batch_start, batch_size);
		if (ret < 0)
			return ret;
	}

	return last_found ? 1 : 0;
}

 
static noinline int log_dir_items(struct btrfs_trans_handle *trans,
			  struct btrfs_inode *inode,
			  struct btrfs_path *path,
			  struct btrfs_path *dst_path,
			  struct btrfs_log_ctx *ctx,
			  u64 min_offset, u64 *last_offset_ret)
{
	struct btrfs_key min_key;
	struct btrfs_root *root = inode->root;
	struct btrfs_root *log = root->log_root;
	int ret;
	u64 last_old_dentry_offset = min_offset - 1;
	u64 last_offset = (u64)-1;
	u64 ino = btrfs_ino(inode);

	min_key.objectid = ino;
	min_key.type = BTRFS_DIR_INDEX_KEY;
	min_key.offset = min_offset;

	ret = btrfs_search_forward(root, &min_key, path, trans->transid);

	 
	if (ret != 0 || min_key.objectid != ino ||
	    min_key.type != BTRFS_DIR_INDEX_KEY) {
		min_key.objectid = ino;
		min_key.type = BTRFS_DIR_INDEX_KEY;
		min_key.offset = (u64)-1;
		btrfs_release_path(path);
		ret = btrfs_search_slot(NULL, root, &min_key, path, 0, 0);
		if (ret < 0) {
			btrfs_release_path(path);
			return ret;
		}
		ret = btrfs_previous_item(root, path, ino, BTRFS_DIR_INDEX_KEY);

		 
		if (ret == 0) {
			struct btrfs_key tmp;

			btrfs_item_key_to_cpu(path->nodes[0], &tmp,
					      path->slots[0]);
			if (tmp.type == BTRFS_DIR_INDEX_KEY)
				last_old_dentry_offset = tmp.offset;
		} else if (ret > 0) {
			ret = 0;
		}

		goto done;
	}

	 
	ret = btrfs_previous_item(root, path, ino, BTRFS_DIR_INDEX_KEY);
	if (ret == 0) {
		struct btrfs_key tmp;

		btrfs_item_key_to_cpu(path->nodes[0], &tmp, path->slots[0]);
		 
		if (tmp.type == BTRFS_DIR_INDEX_KEY)
			last_old_dentry_offset = tmp.offset;
	} else if (ret < 0) {
		goto done;
	}

	btrfs_release_path(path);

	 
search:
	ret = btrfs_search_slot(NULL, root, &min_key, path, 0, 0);
	if (ret > 0) {
		ret = btrfs_next_item(root, path);
		if (ret > 0) {
			 
			ret = 0;
			goto done;
		}
	}
	if (ret < 0)
		goto done;

	 
	while (1) {
		ret = process_dir_items_leaf(trans, inode, path, dst_path, ctx,
					     &last_old_dentry_offset);
		if (ret != 0) {
			if (ret > 0)
				ret = 0;
			goto done;
		}
		path->slots[0] = btrfs_header_nritems(path->nodes[0]);

		 
		ret = btrfs_next_leaf(root, path);
		if (ret) {
			if (ret == 1) {
				last_offset = (u64)-1;
				ret = 0;
			}
			goto done;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &min_key, path->slots[0]);
		if (min_key.objectid != ino || min_key.type != BTRFS_DIR_INDEX_KEY) {
			last_offset = (u64)-1;
			goto done;
		}
		if (btrfs_header_generation(path->nodes[0]) != trans->transid) {
			 
			last_offset = min_key.offset - 1;
			goto done;
		}
		if (need_resched()) {
			btrfs_release_path(path);
			cond_resched();
			goto search;
		}
	}
done:
	btrfs_release_path(path);
	btrfs_release_path(dst_path);

	if (ret == 0) {
		*last_offset_ret = last_offset;
		 
		ASSERT(last_old_dentry_offset <= last_offset);
		if (last_old_dentry_offset < last_offset)
			ret = insert_dir_log_key(trans, log, path, ino,
						 last_old_dentry_offset + 1,
						 last_offset);
	}

	return ret;
}

 
static int update_last_dir_index_offset(struct btrfs_inode *inode,
					struct btrfs_path *path,
					const struct btrfs_log_ctx *ctx)
{
	const u64 ino = btrfs_ino(inode);
	struct btrfs_key key;
	int ret;

	lockdep_assert_held(&inode->log_mutex);

	if (inode->last_dir_index_offset != (u64)-1)
		return 0;

	if (!ctx->logged_before) {
		inode->last_dir_index_offset = BTRFS_DIR_START_INDEX - 1;
		return 0;
	}

	key.objectid = ino;
	key.type = BTRFS_DIR_INDEX_KEY;
	key.offset = (u64)-1;

	ret = btrfs_search_slot(NULL, inode->root->log_root, &key, path, 0, 0);
	 
	if (ret <= 0)
		goto out;

	ret = 0;
	inode->last_dir_index_offset = BTRFS_DIR_START_INDEX - 1;

	 
	if (path->slots[0] == 0)
		goto out;

	 
	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0] - 1);
	if (key.objectid == ino && key.type == BTRFS_DIR_INDEX_KEY)
		inode->last_dir_index_offset = key.offset;

out:
	btrfs_release_path(path);

	return ret;
}

 
static noinline int log_directory_changes(struct btrfs_trans_handle *trans,
			  struct btrfs_inode *inode,
			  struct btrfs_path *path,
			  struct btrfs_path *dst_path,
			  struct btrfs_log_ctx *ctx)
{
	u64 min_key;
	u64 max_key;
	int ret;

	ret = update_last_dir_index_offset(inode, path, ctx);
	if (ret)
		return ret;

	min_key = BTRFS_DIR_START_INDEX;
	max_key = 0;

	while (1) {
		ret = log_dir_items(trans, inode, path, dst_path,
				ctx, min_key, &max_key);
		if (ret)
			return ret;
		if (max_key == (u64)-1)
			break;
		min_key = max_key + 1;
	}

	return 0;
}

 
static int drop_inode_items(struct btrfs_trans_handle *trans,
				  struct btrfs_root *log,
				  struct btrfs_path *path,
				  struct btrfs_inode *inode,
				  int max_key_type)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_key found_key;
	int start_slot;

	key.objectid = btrfs_ino(inode);
	key.type = max_key_type;
	key.offset = (u64)-1;

	while (1) {
		ret = btrfs_search_slot(trans, log, &key, path, -1, 1);
		if (ret < 0) {
			break;
		} else if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}

		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				      path->slots[0]);

		if (found_key.objectid != key.objectid)
			break;

		found_key.offset = 0;
		found_key.type = 0;
		ret = btrfs_bin_search(path->nodes[0], 0, &found_key, &start_slot);
		if (ret < 0)
			break;

		ret = btrfs_del_items(trans, log, path, start_slot,
				      path->slots[0] - start_slot + 1);
		 
		if (ret || start_slot != 0)
			break;
		btrfs_release_path(path);
	}
	btrfs_release_path(path);
	if (ret > 0)
		ret = 0;
	return ret;
}

static int truncate_inode_items(struct btrfs_trans_handle *trans,
				struct btrfs_root *log_root,
				struct btrfs_inode *inode,
				u64 new_size, u32 min_type)
{
	struct btrfs_truncate_control control = {
		.new_size = new_size,
		.ino = btrfs_ino(inode),
		.min_type = min_type,
		.skip_ref_updates = true,
	};

	return btrfs_truncate_inode_items(trans, log_root, &control);
}

static void fill_inode_item(struct btrfs_trans_handle *trans,
			    struct extent_buffer *leaf,
			    struct btrfs_inode_item *item,
			    struct inode *inode, int log_inode_only,
			    u64 logged_isize)
{
	struct btrfs_map_token token;
	u64 flags;

	btrfs_init_map_token(&token, leaf);

	if (log_inode_only) {
		 
		btrfs_set_token_inode_generation(&token, item, 0);
		btrfs_set_token_inode_size(&token, item, logged_isize);
	} else {
		btrfs_set_token_inode_generation(&token, item,
						 BTRFS_I(inode)->generation);
		btrfs_set_token_inode_size(&token, item, inode->i_size);
	}

	btrfs_set_token_inode_uid(&token, item, i_uid_read(inode));
	btrfs_set_token_inode_gid(&token, item, i_gid_read(inode));
	btrfs_set_token_inode_mode(&token, item, inode->i_mode);
	btrfs_set_token_inode_nlink(&token, item, inode->i_nlink);

	btrfs_set_token_timespec_sec(&token, &item->atime,
				     inode->i_atime.tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->atime,
				      inode->i_atime.tv_nsec);

	btrfs_set_token_timespec_sec(&token, &item->mtime,
				     inode->i_mtime.tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->mtime,
				      inode->i_mtime.tv_nsec);

	btrfs_set_token_timespec_sec(&token, &item->ctime,
				     inode_get_ctime(inode).tv_sec);
	btrfs_set_token_timespec_nsec(&token, &item->ctime,
				      inode_get_ctime(inode).tv_nsec);

	 

	btrfs_set_token_inode_sequence(&token, item, inode_peek_iversion(inode));
	btrfs_set_token_inode_transid(&token, item, trans->transid);
	btrfs_set_token_inode_rdev(&token, item, inode->i_rdev);
	flags = btrfs_inode_combine_flags(BTRFS_I(inode)->flags,
					  BTRFS_I(inode)->ro_flags);
	btrfs_set_token_inode_flags(&token, item, flags);
	btrfs_set_token_inode_block_group(&token, item, 0);
}

static int log_inode_item(struct btrfs_trans_handle *trans,
			  struct btrfs_root *log, struct btrfs_path *path,
			  struct btrfs_inode *inode, bool inode_item_dropped)
{
	struct btrfs_inode_item *inode_item;
	int ret;

	 
	if (!inode_item_dropped && inode->logged_trans == trans->transid) {
		ret = btrfs_search_slot(trans, log, &inode->location, path, 0, 1);
		ASSERT(ret <= 0);
		if (ret > 0)
			ret = -ENOENT;
	} else {
		 
		ret = btrfs_insert_empty_item(trans, log, path, &inode->location,
					      sizeof(*inode_item));
		ASSERT(ret != -EEXIST);
	}
	if (ret)
		return ret;
	inode_item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				    struct btrfs_inode_item);
	fill_inode_item(trans, path->nodes[0], inode_item, &inode->vfs_inode,
			0, 0);
	btrfs_release_path(path);
	return 0;
}

static int log_csums(struct btrfs_trans_handle *trans,
		     struct btrfs_inode *inode,
		     struct btrfs_root *log_root,
		     struct btrfs_ordered_sum *sums)
{
	const u64 lock_end = sums->logical + sums->len - 1;
	struct extent_state *cached_state = NULL;
	int ret;

	 
	if (inode->last_reflink_trans < trans->transid)
		return btrfs_csum_file_blocks(trans, log_root, sums);

	 
	ret = lock_extent(&log_root->log_csum_range, sums->logical, lock_end,
			  &cached_state);
	if (ret)
		return ret;
	 
	ret = btrfs_del_csums(trans, log_root, sums->logical, sums->len);
	if (!ret)
		ret = btrfs_csum_file_blocks(trans, log_root, sums);

	unlock_extent(&log_root->log_csum_range, sums->logical, lock_end,
		      &cached_state);

	return ret;
}

static noinline int copy_items(struct btrfs_trans_handle *trans,
			       struct btrfs_inode *inode,
			       struct btrfs_path *dst_path,
			       struct btrfs_path *src_path,
			       int start_slot, int nr, int inode_only,
			       u64 logged_isize)
{
	struct btrfs_root *log = inode->root->log_root;
	struct btrfs_file_extent_item *extent;
	struct extent_buffer *src;
	int ret = 0;
	struct btrfs_key *ins_keys;
	u32 *ins_sizes;
	struct btrfs_item_batch batch;
	char *ins_data;
	int i;
	int dst_index;
	const bool skip_csum = (inode->flags & BTRFS_INODE_NODATASUM);
	const u64 i_size = i_size_read(&inode->vfs_inode);

	 
	src = btrfs_clone_extent_buffer(src_path->nodes[0]);
	if (!src)
		return -ENOMEM;

	i = src_path->slots[0];
	btrfs_release_path(src_path);
	src_path->nodes[0] = src;
	src_path->slots[0] = i;

	ins_data = kmalloc(nr * sizeof(struct btrfs_key) +
			   nr * sizeof(u32), GFP_NOFS);
	if (!ins_data)
		return -ENOMEM;

	ins_sizes = (u32 *)ins_data;
	ins_keys = (struct btrfs_key *)(ins_data + nr * sizeof(u32));
	batch.keys = ins_keys;
	batch.data_sizes = ins_sizes;
	batch.total_data_size = 0;
	batch.nr = 0;

	dst_index = 0;
	for (i = 0; i < nr; i++) {
		const int src_slot = start_slot + i;
		struct btrfs_root *csum_root;
		struct btrfs_ordered_sum *sums;
		struct btrfs_ordered_sum *sums_next;
		LIST_HEAD(ordered_sums);
		u64 disk_bytenr;
		u64 disk_num_bytes;
		u64 extent_offset;
		u64 extent_num_bytes;
		bool is_old_extent;

		btrfs_item_key_to_cpu(src, &ins_keys[dst_index], src_slot);

		if (ins_keys[dst_index].type != BTRFS_EXTENT_DATA_KEY)
			goto add_to_batch;

		extent = btrfs_item_ptr(src, src_slot,
					struct btrfs_file_extent_item);

		is_old_extent = (btrfs_file_extent_generation(src, extent) <
				 trans->transid);

		 
		if (is_old_extent &&
		    ins_keys[dst_index].offset < i_size &&
		    inode->last_reflink_trans < trans->transid)
			continue;

		if (skip_csum)
			goto add_to_batch;

		 
		if (btrfs_file_extent_type(src, extent) != BTRFS_FILE_EXTENT_REG)
			goto add_to_batch;

		 
		if (is_old_extent)
			goto add_to_batch;

		disk_bytenr = btrfs_file_extent_disk_bytenr(src, extent);
		 
		if (disk_bytenr == 0)
			goto add_to_batch;

		disk_num_bytes = btrfs_file_extent_disk_num_bytes(src, extent);

		if (btrfs_file_extent_compression(src, extent)) {
			extent_offset = 0;
			extent_num_bytes = disk_num_bytes;
		} else {
			extent_offset = btrfs_file_extent_offset(src, extent);
			extent_num_bytes = btrfs_file_extent_num_bytes(src, extent);
		}

		csum_root = btrfs_csum_root(trans->fs_info, disk_bytenr);
		disk_bytenr += extent_offset;
		ret = btrfs_lookup_csums_list(csum_root, disk_bytenr,
					      disk_bytenr + extent_num_bytes - 1,
					      &ordered_sums, 0, false);
		if (ret)
			goto out;

		list_for_each_entry_safe(sums, sums_next, &ordered_sums, list) {
			if (!ret)
				ret = log_csums(trans, inode, log, sums);
			list_del(&sums->list);
			kfree(sums);
		}
		if (ret)
			goto out;

add_to_batch:
		ins_sizes[dst_index] = btrfs_item_size(src, src_slot);
		batch.total_data_size += ins_sizes[dst_index];
		batch.nr++;
		dst_index++;
	}

	 
	if (batch.nr == 0)
		goto out;

	ret = btrfs_insert_empty_items(trans, log, dst_path, &batch);
	if (ret)
		goto out;

	dst_index = 0;
	for (i = 0; i < nr; i++) {
		const int src_slot = start_slot + i;
		const int dst_slot = dst_path->slots[0] + dst_index;
		struct btrfs_key key;
		unsigned long src_offset;
		unsigned long dst_offset;

		 
		if (dst_index >= batch.nr)
			break;

		btrfs_item_key_to_cpu(src, &key, src_slot);

		if (key.type != BTRFS_EXTENT_DATA_KEY)
			goto copy_item;

		extent = btrfs_item_ptr(src, src_slot,
					struct btrfs_file_extent_item);

		 
		if (btrfs_file_extent_generation(src, extent) < trans->transid &&
		    key.offset < i_size &&
		    inode->last_reflink_trans < trans->transid)
			continue;

copy_item:
		dst_offset = btrfs_item_ptr_offset(dst_path->nodes[0], dst_slot);
		src_offset = btrfs_item_ptr_offset(src, src_slot);

		if (key.type == BTRFS_INODE_ITEM_KEY) {
			struct btrfs_inode_item *inode_item;

			inode_item = btrfs_item_ptr(dst_path->nodes[0], dst_slot,
						    struct btrfs_inode_item);
			fill_inode_item(trans, dst_path->nodes[0], inode_item,
					&inode->vfs_inode,
					inode_only == LOG_INODE_EXISTS,
					logged_isize);
		} else {
			copy_extent_buffer(dst_path->nodes[0], src, dst_offset,
					   src_offset, ins_sizes[dst_index]);
		}

		dst_index++;
	}

	btrfs_mark_buffer_dirty(trans, dst_path->nodes[0]);
	btrfs_release_path(dst_path);
out:
	kfree(ins_data);

	return ret;
}

static int extent_cmp(void *priv, const struct list_head *a,
		      const struct list_head *b)
{
	const struct extent_map *em1, *em2;

	em1 = list_entry(a, struct extent_map, list);
	em2 = list_entry(b, struct extent_map, list);

	if (em1->start < em2->start)
		return -1;
	else if (em1->start > em2->start)
		return 1;
	return 0;
}

static int log_extent_csums(struct btrfs_trans_handle *trans,
			    struct btrfs_inode *inode,
			    struct btrfs_root *log_root,
			    const struct extent_map *em,
			    struct btrfs_log_ctx *ctx)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_root *csum_root;
	u64 csum_offset;
	u64 csum_len;
	u64 mod_start = em->mod_start;
	u64 mod_len = em->mod_len;
	LIST_HEAD(ordered_sums);
	int ret = 0;

	if (inode->flags & BTRFS_INODE_NODATASUM ||
	    test_bit(EXTENT_FLAG_PREALLOC, &em->flags) ||
	    em->block_start == EXTENT_MAP_HOLE)
		return 0;

	list_for_each_entry(ordered, &ctx->ordered_extents, log_list) {
		const u64 ordered_end = ordered->file_offset + ordered->num_bytes;
		const u64 mod_end = mod_start + mod_len;
		struct btrfs_ordered_sum *sums;

		if (mod_len == 0)
			break;

		if (ordered_end <= mod_start)
			continue;
		if (mod_end <= ordered->file_offset)
			break;

		 
		if (ordered->file_offset > mod_start) {
			if (ordered_end >= mod_end)
				mod_len = ordered->file_offset - mod_start;
			 
		} else {
			if (ordered_end < mod_end) {
				mod_len = mod_end - ordered_end;
				mod_start = ordered_end;
			} else {
				mod_len = 0;
			}
		}

		 
		if (test_and_set_bit(BTRFS_ORDERED_LOGGED_CSUM, &ordered->flags))
			continue;

		list_for_each_entry(sums, &ordered->list, list) {
			ret = log_csums(trans, inode, log_root, sums);
			if (ret)
				return ret;
		}
	}

	 
	if (mod_len == 0)
		return 0;

	 
	if (em->compress_type) {
		csum_offset = 0;
		csum_len = max(em->block_len, em->orig_block_len);
	} else {
		csum_offset = mod_start - em->start;
		csum_len = mod_len;
	}

	 
	csum_root = btrfs_csum_root(trans->fs_info, em->block_start);
	ret = btrfs_lookup_csums_list(csum_root, em->block_start + csum_offset,
				      em->block_start + csum_offset +
				      csum_len - 1, &ordered_sums, 0, false);
	if (ret)
		return ret;

	while (!list_empty(&ordered_sums)) {
		struct btrfs_ordered_sum *sums = list_entry(ordered_sums.next,
						   struct btrfs_ordered_sum,
						   list);
		if (!ret)
			ret = log_csums(trans, inode, log_root, sums);
		list_del(&sums->list);
		kfree(sums);
	}

	return ret;
}

static int log_one_extent(struct btrfs_trans_handle *trans,
			  struct btrfs_inode *inode,
			  const struct extent_map *em,
			  struct btrfs_path *path,
			  struct btrfs_log_ctx *ctx)
{
	struct btrfs_drop_extents_args drop_args = { 0 };
	struct btrfs_root *log = inode->root->log_root;
	struct btrfs_file_extent_item fi = { 0 };
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u64 extent_offset = em->start - em->orig_start;
	u64 block_len;
	int ret;

	btrfs_set_stack_file_extent_generation(&fi, trans->transid);
	if (test_bit(EXTENT_FLAG_PREALLOC, &em->flags))
		btrfs_set_stack_file_extent_type(&fi, BTRFS_FILE_EXTENT_PREALLOC);
	else
		btrfs_set_stack_file_extent_type(&fi, BTRFS_FILE_EXTENT_REG);

	block_len = max(em->block_len, em->orig_block_len);
	if (em->compress_type != BTRFS_COMPRESS_NONE) {
		btrfs_set_stack_file_extent_disk_bytenr(&fi, em->block_start);
		btrfs_set_stack_file_extent_disk_num_bytes(&fi, block_len);
	} else if (em->block_start < EXTENT_MAP_LAST_BYTE) {
		btrfs_set_stack_file_extent_disk_bytenr(&fi, em->block_start -
							extent_offset);
		btrfs_set_stack_file_extent_disk_num_bytes(&fi, block_len);
	}

	btrfs_set_stack_file_extent_offset(&fi, extent_offset);
	btrfs_set_stack_file_extent_num_bytes(&fi, em->len);
	btrfs_set_stack_file_extent_ram_bytes(&fi, em->ram_bytes);
	btrfs_set_stack_file_extent_compression(&fi, em->compress_type);

	ret = log_extent_csums(trans, inode, log, em, ctx);
	if (ret)
		return ret;

	 
	if (ctx->logged_before) {
		drop_args.path = path;
		drop_args.start = em->start;
		drop_args.end = em->start + em->len;
		drop_args.replace_extent = true;
		drop_args.extent_item_size = sizeof(fi);
		ret = btrfs_drop_extents(trans, log, inode, &drop_args);
		if (ret)
			return ret;
	}

	if (!drop_args.extent_inserted) {
		key.objectid = btrfs_ino(inode);
		key.type = BTRFS_EXTENT_DATA_KEY;
		key.offset = em->start;

		ret = btrfs_insert_empty_item(trans, log, path, &key,
					      sizeof(fi));
		if (ret)
			return ret;
	}
	leaf = path->nodes[0];
	write_extent_buffer(leaf, &fi,
			    btrfs_item_ptr_offset(leaf, path->slots[0]),
			    sizeof(fi));
	btrfs_mark_buffer_dirty(trans, leaf);

	btrfs_release_path(path);

	return ret;
}

 
static int btrfs_log_prealloc_extents(struct btrfs_trans_handle *trans,
				      struct btrfs_inode *inode,
				      struct btrfs_path *path)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_key key;
	const u64 i_size = i_size_read(&inode->vfs_inode);
	const u64 ino = btrfs_ino(inode);
	struct btrfs_path *dst_path = NULL;
	bool dropped_extents = false;
	u64 truncate_offset = i_size;
	struct extent_buffer *leaf;
	int slot;
	int ins_nr = 0;
	int start_slot = 0;
	int ret;

	if (!(inode->flags & BTRFS_INODE_PREALLOC))
		return 0;

	key.objectid = ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = i_size;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;

	 
	ret = btrfs_previous_item(root, path, ino, BTRFS_EXTENT_DATA_KEY);
	if (ret < 0)
		goto out;

	if (ret == 0) {
		struct btrfs_file_extent_item *ei;

		leaf = path->nodes[0];
		slot = path->slots[0];
		ei = btrfs_item_ptr(leaf, slot, struct btrfs_file_extent_item);

		if (btrfs_file_extent_type(leaf, ei) ==
		    BTRFS_FILE_EXTENT_PREALLOC) {
			u64 extent_end;

			btrfs_item_key_to_cpu(leaf, &key, slot);
			extent_end = key.offset +
				btrfs_file_extent_num_bytes(leaf, ei);

			if (extent_end > i_size)
				truncate_offset = extent_end;
		}
	} else {
		ret = 0;
	}

	while (true) {
		leaf = path->nodes[0];
		slot = path->slots[0];

		if (slot >= btrfs_header_nritems(leaf)) {
			if (ins_nr > 0) {
				ret = copy_items(trans, inode, dst_path, path,
						 start_slot, ins_nr, 1, 0);
				if (ret < 0)
					goto out;
				ins_nr = 0;
			}
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			if (ret > 0) {
				ret = 0;
				break;
			}
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid > ino)
			break;
		if (WARN_ON_ONCE(key.objectid < ino) ||
		    key.type < BTRFS_EXTENT_DATA_KEY ||
		    key.offset < i_size) {
			path->slots[0]++;
			continue;
		}
		if (!dropped_extents) {
			 
			ret = truncate_inode_items(trans, root->log_root, inode,
						   truncate_offset,
						   BTRFS_EXTENT_DATA_KEY);
			if (ret)
				goto out;
			dropped_extents = true;
		}
		if (ins_nr == 0)
			start_slot = slot;
		ins_nr++;
		path->slots[0]++;
		if (!dst_path) {
			dst_path = btrfs_alloc_path();
			if (!dst_path) {
				ret = -ENOMEM;
				goto out;
			}
		}
	}
	if (ins_nr > 0)
		ret = copy_items(trans, inode, dst_path, path,
				 start_slot, ins_nr, 1, 0);
out:
	btrfs_release_path(path);
	btrfs_free_path(dst_path);
	return ret;
}

static int btrfs_log_changed_extents(struct btrfs_trans_handle *trans,
				     struct btrfs_inode *inode,
				     struct btrfs_path *path,
				     struct btrfs_log_ctx *ctx)
{
	struct btrfs_ordered_extent *ordered;
	struct btrfs_ordered_extent *tmp;
	struct extent_map *em, *n;
	LIST_HEAD(extents);
	struct extent_map_tree *tree = &inode->extent_tree;
	int ret = 0;
	int num = 0;

	write_lock(&tree->lock);

	list_for_each_entry_safe(em, n, &tree->modified_extents, list) {
		list_del_init(&em->list);
		 
		if (++num > 32768) {
			list_del_init(&tree->modified_extents);
			ret = -EFBIG;
			goto process;
		}

		if (em->generation < trans->transid)
			continue;

		 
		if (test_bit(EXTENT_FLAG_PREALLOC, &em->flags) &&
		    em->start >= i_size_read(&inode->vfs_inode))
			continue;

		 
		refcount_inc(&em->refs);
		set_bit(EXTENT_FLAG_LOGGING, &em->flags);
		list_add_tail(&em->list, &extents);
		num++;
	}

	list_sort(NULL, &extents, extent_cmp);
process:
	while (!list_empty(&extents)) {
		em = list_entry(extents.next, struct extent_map, list);

		list_del_init(&em->list);

		 
		if (ret) {
			clear_em_logging(tree, em);
			free_extent_map(em);
			continue;
		}

		write_unlock(&tree->lock);

		ret = log_one_extent(trans, inode, em, path, ctx);
		write_lock(&tree->lock);
		clear_em_logging(tree, em);
		free_extent_map(em);
	}
	WARN_ON(!list_empty(&extents));
	write_unlock(&tree->lock);

	if (!ret)
		ret = btrfs_log_prealloc_extents(trans, inode, path);
	if (ret)
		return ret;

	 
	list_for_each_entry_safe(ordered, tmp, &ctx->ordered_extents, log_list) {
		list_del_init(&ordered->log_list);
		set_bit(BTRFS_ORDERED_LOGGED, &ordered->flags);

		if (!test_bit(BTRFS_ORDERED_COMPLETE, &ordered->flags)) {
			spin_lock_irq(&inode->ordered_tree.lock);
			if (!test_bit(BTRFS_ORDERED_COMPLETE, &ordered->flags)) {
				set_bit(BTRFS_ORDERED_PENDING, &ordered->flags);
				atomic_inc(&trans->transaction->pending_ordered);
			}
			spin_unlock_irq(&inode->ordered_tree.lock);
		}
		btrfs_put_ordered_extent(ordered);
	}

	return 0;
}

static int logged_inode_size(struct btrfs_root *log, struct btrfs_inode *inode,
			     struct btrfs_path *path, u64 *size_ret)
{
	struct btrfs_key key;
	int ret;

	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, log, &key, path, 0, 0);
	if (ret < 0) {
		return ret;
	} else if (ret > 0) {
		*size_ret = 0;
	} else {
		struct btrfs_inode_item *item;

		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_inode_item);
		*size_ret = btrfs_inode_size(path->nodes[0], item);
		 
		if (*size_ret > inode->vfs_inode.i_size)
			*size_ret = inode->vfs_inode.i_size;
	}

	btrfs_release_path(path);
	return 0;
}

 
static int btrfs_log_all_xattrs(struct btrfs_trans_handle *trans,
				struct btrfs_inode *inode,
				struct btrfs_path *path,
				struct btrfs_path *dst_path)
{
	struct btrfs_root *root = inode->root;
	int ret;
	struct btrfs_key key;
	const u64 ino = btrfs_ino(inode);
	int ins_nr = 0;
	int start_slot = 0;
	bool found_xattrs = false;

	if (test_bit(BTRFS_INODE_NO_XATTRS, &inode->runtime_flags))
		return 0;

	key.objectid = ino;
	key.type = BTRFS_XATTR_ITEM_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	while (true) {
		int slot = path->slots[0];
		struct extent_buffer *leaf = path->nodes[0];
		int nritems = btrfs_header_nritems(leaf);

		if (slot >= nritems) {
			if (ins_nr > 0) {
				ret = copy_items(trans, inode, dst_path, path,
						 start_slot, ins_nr, 1, 0);
				if (ret < 0)
					return ret;
				ins_nr = 0;
			}
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				return ret;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid != ino || key.type != BTRFS_XATTR_ITEM_KEY)
			break;

		if (ins_nr == 0)
			start_slot = slot;
		ins_nr++;
		path->slots[0]++;
		found_xattrs = true;
		cond_resched();
	}
	if (ins_nr > 0) {
		ret = copy_items(trans, inode, dst_path, path,
				 start_slot, ins_nr, 1, 0);
		if (ret < 0)
			return ret;
	}

	if (!found_xattrs)
		set_bit(BTRFS_INODE_NO_XATTRS, &inode->runtime_flags);

	return 0;
}

 
static int btrfs_log_holes(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode,
			   struct btrfs_path *path)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key key;
	const u64 ino = btrfs_ino(inode);
	const u64 i_size = i_size_read(&inode->vfs_inode);
	u64 prev_extent_end = 0;
	int ret;

	if (!btrfs_fs_incompat(fs_info, NO_HOLES) || i_size == 0)
		return 0;

	key.objectid = ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = 0;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];

		if (path->slots[0] >= btrfs_header_nritems(path->nodes[0])) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				return ret;
			if (ret > 0) {
				ret = 0;
				break;
			}
			leaf = path->nodes[0];
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != ino || key.type != BTRFS_EXTENT_DATA_KEY)
			break;

		 
		if (prev_extent_end < key.offset) {
			const u64 hole_len = key.offset - prev_extent_end;

			 
			btrfs_release_path(path);
			ret = btrfs_insert_hole_extent(trans, root->log_root,
						       ino, prev_extent_end,
						       hole_len);
			if (ret < 0)
				return ret;

			 
			ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
			if (ret < 0)
				return ret;
			if (WARN_ON(ret > 0))
				return -ENOENT;
			leaf = path->nodes[0];
		}

		prev_extent_end = btrfs_file_extent_end(path);
		path->slots[0]++;
		cond_resched();
	}

	if (prev_extent_end < i_size) {
		u64 hole_len;

		btrfs_release_path(path);
		hole_len = ALIGN(i_size - prev_extent_end, fs_info->sectorsize);
		ret = btrfs_insert_hole_extent(trans, root->log_root, ino,
					       prev_extent_end, hole_len);
		if (ret < 0)
			return ret;
	}

	return 0;
}

 
static int btrfs_check_ref_name_override(struct extent_buffer *eb,
					 const int slot,
					 const struct btrfs_key *key,
					 struct btrfs_inode *inode,
					 u64 *other_ino, u64 *other_parent)
{
	int ret;
	struct btrfs_path *search_path;
	char *name = NULL;
	u32 name_len = 0;
	u32 item_size = btrfs_item_size(eb, slot);
	u32 cur_offset = 0;
	unsigned long ptr = btrfs_item_ptr_offset(eb, slot);

	search_path = btrfs_alloc_path();
	if (!search_path)
		return -ENOMEM;
	search_path->search_commit_root = 1;
	search_path->skip_locking = 1;

	while (cur_offset < item_size) {
		u64 parent;
		u32 this_name_len;
		u32 this_len;
		unsigned long name_ptr;
		struct btrfs_dir_item *di;
		struct fscrypt_str name_str;

		if (key->type == BTRFS_INODE_REF_KEY) {
			struct btrfs_inode_ref *iref;

			iref = (struct btrfs_inode_ref *)(ptr + cur_offset);
			parent = key->offset;
			this_name_len = btrfs_inode_ref_name_len(eb, iref);
			name_ptr = (unsigned long)(iref + 1);
			this_len = sizeof(*iref) + this_name_len;
		} else {
			struct btrfs_inode_extref *extref;

			extref = (struct btrfs_inode_extref *)(ptr +
							       cur_offset);
			parent = btrfs_inode_extref_parent(eb, extref);
			this_name_len = btrfs_inode_extref_name_len(eb, extref);
			name_ptr = (unsigned long)&extref->name;
			this_len = sizeof(*extref) + this_name_len;
		}

		if (this_name_len > name_len) {
			char *new_name;

			new_name = krealloc(name, this_name_len, GFP_NOFS);
			if (!new_name) {
				ret = -ENOMEM;
				goto out;
			}
			name_len = this_name_len;
			name = new_name;
		}

		read_extent_buffer(eb, name, name_ptr, this_name_len);

		name_str.name = name;
		name_str.len = this_name_len;
		di = btrfs_lookup_dir_item(NULL, inode->root, search_path,
				parent, &name_str, 0);
		if (di && !IS_ERR(di)) {
			struct btrfs_key di_key;

			btrfs_dir_item_key_to_cpu(search_path->nodes[0],
						  di, &di_key);
			if (di_key.type == BTRFS_INODE_ITEM_KEY) {
				if (di_key.objectid != key->objectid) {
					ret = 1;
					*other_ino = di_key.objectid;
					*other_parent = parent;
				} else {
					ret = 0;
				}
			} else {
				ret = -EAGAIN;
			}
			goto out;
		} else if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out;
		}
		btrfs_release_path(search_path);

		cur_offset += this_len;
	}
	ret = 0;
out:
	btrfs_free_path(search_path);
	kfree(name);
	return ret;
}

 
static bool need_log_inode(const struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode)
{
	 
	if (S_ISDIR(inode->vfs_inode.i_mode) && inode->last_trans < trans->transid)
		return false;

	 
	if (inode_logged(trans, inode, NULL) == 1 &&
	    !test_bit(BTRFS_INODE_COPY_EVERYTHING, &inode->runtime_flags))
		return false;

	return true;
}

struct btrfs_dir_list {
	u64 ino;
	struct list_head list;
};

 
static int log_new_dir_dentries(struct btrfs_trans_handle *trans,
				struct btrfs_inode *start_inode,
				struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = start_inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path;
	LIST_HEAD(dir_list);
	struct btrfs_dir_list *dir_elem;
	u64 ino = btrfs_ino(start_inode);
	struct btrfs_inode *curr_inode = start_inode;
	int ret = 0;

	 
	if (ctx->logging_new_name)
		return 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	 
	ihold(&curr_inode->vfs_inode);

	while (true) {
		struct inode *vfs_inode;
		struct btrfs_key key;
		struct btrfs_key found_key;
		u64 next_index;
		bool continue_curr_inode = true;
		int iter_ret;

		key.objectid = ino;
		key.type = BTRFS_DIR_INDEX_KEY;
		key.offset = btrfs_get_first_dir_index_to_log(curr_inode);
		next_index = key.offset;
again:
		btrfs_for_each_slot(root->log_root, &key, &found_key, path, iter_ret) {
			struct extent_buffer *leaf = path->nodes[0];
			struct btrfs_dir_item *di;
			struct btrfs_key di_key;
			struct inode *di_inode;
			int log_mode = LOG_INODE_EXISTS;
			int type;

			if (found_key.objectid != ino ||
			    found_key.type != BTRFS_DIR_INDEX_KEY) {
				continue_curr_inode = false;
				break;
			}

			next_index = found_key.offset + 1;

			di = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_dir_item);
			type = btrfs_dir_ftype(leaf, di);
			if (btrfs_dir_transid(leaf, di) < trans->transid)
				continue;
			btrfs_dir_item_key_to_cpu(leaf, di, &di_key);
			if (di_key.type == BTRFS_ROOT_ITEM_KEY)
				continue;

			btrfs_release_path(path);
			di_inode = btrfs_iget(fs_info->sb, di_key.objectid, root);
			if (IS_ERR(di_inode)) {
				ret = PTR_ERR(di_inode);
				goto out;
			}

			if (!need_log_inode(trans, BTRFS_I(di_inode))) {
				btrfs_add_delayed_iput(BTRFS_I(di_inode));
				break;
			}

			ctx->log_new_dentries = false;
			if (type == BTRFS_FT_DIR)
				log_mode = LOG_INODE_ALL;
			ret = btrfs_log_inode(trans, BTRFS_I(di_inode),
					      log_mode, ctx);
			btrfs_add_delayed_iput(BTRFS_I(di_inode));
			if (ret)
				goto out;
			if (ctx->log_new_dentries) {
				dir_elem = kmalloc(sizeof(*dir_elem), GFP_NOFS);
				if (!dir_elem) {
					ret = -ENOMEM;
					goto out;
				}
				dir_elem->ino = di_key.objectid;
				list_add_tail(&dir_elem->list, &dir_list);
			}
			break;
		}

		btrfs_release_path(path);

		if (iter_ret < 0) {
			ret = iter_ret;
			goto out;
		} else if (iter_ret > 0) {
			continue_curr_inode = false;
		} else {
			key = found_key;
		}

		if (continue_curr_inode && key.offset < (u64)-1) {
			key.offset++;
			goto again;
		}

		btrfs_set_first_dir_index_to_log(curr_inode, next_index);

		if (list_empty(&dir_list))
			break;

		dir_elem = list_first_entry(&dir_list, struct btrfs_dir_list, list);
		ino = dir_elem->ino;
		list_del(&dir_elem->list);
		kfree(dir_elem);

		btrfs_add_delayed_iput(curr_inode);
		curr_inode = NULL;

		vfs_inode = btrfs_iget(fs_info->sb, ino, root);
		if (IS_ERR(vfs_inode)) {
			ret = PTR_ERR(vfs_inode);
			break;
		}
		curr_inode = BTRFS_I(vfs_inode);
	}
out:
	btrfs_free_path(path);
	if (curr_inode)
		btrfs_add_delayed_iput(curr_inode);

	if (ret) {
		struct btrfs_dir_list *next;

		list_for_each_entry_safe(dir_elem, next, &dir_list, list)
			kfree(dir_elem);
	}

	return ret;
}

struct btrfs_ino_list {
	u64 ino;
	u64 parent;
	struct list_head list;
};

static void free_conflicting_inodes(struct btrfs_log_ctx *ctx)
{
	struct btrfs_ino_list *curr;
	struct btrfs_ino_list *next;

	list_for_each_entry_safe(curr, next, &ctx->conflict_inodes, list) {
		list_del(&curr->list);
		kfree(curr);
	}
}

static int conflicting_inode_is_dir(struct btrfs_root *root, u64 ino,
				    struct btrfs_path *path)
{
	struct btrfs_key key;
	int ret;

	key.objectid = ino;
	key.type = BTRFS_INODE_ITEM_KEY;
	key.offset = 0;

	path->search_commit_root = 1;
	path->skip_locking = 1;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (WARN_ON_ONCE(ret > 0)) {
		 
		ret = -ENOENT;
	} else if (ret == 0) {
		struct btrfs_inode_item *item;

		item = btrfs_item_ptr(path->nodes[0], path->slots[0],
				      struct btrfs_inode_item);
		if (S_ISDIR(btrfs_inode_mode(path->nodes[0], item)))
			ret = 1;
	}

	btrfs_release_path(path);
	path->search_commit_root = 0;
	path->skip_locking = 0;

	return ret;
}

static int add_conflicting_inode(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 u64 ino, u64 parent,
				 struct btrfs_log_ctx *ctx)
{
	struct btrfs_ino_list *ino_elem;
	struct inode *inode;

	 
	if (ctx->num_conflict_inodes >= MAX_CONFLICT_INODES)
		return BTRFS_LOG_FORCE_COMMIT;

	inode = btrfs_iget(root->fs_info->sb, ino, root);
	 
	if (IS_ERR(inode)) {
		int ret = PTR_ERR(inode);

		if (ret != -ENOENT)
			return ret;

		ret = conflicting_inode_is_dir(root, ino, path);
		 
		if (ret <= 0)
			return ret;

		 
		ino_elem = kmalloc(sizeof(*ino_elem), GFP_NOFS);
		if (!ino_elem)
			return -ENOMEM;
		ino_elem->ino = ino;
		ino_elem->parent = parent;
		list_add_tail(&ino_elem->list, &ctx->conflict_inodes);
		ctx->num_conflict_inodes++;

		return 0;
	}

	 
	if (!need_log_inode(trans, BTRFS_I(inode))) {
		btrfs_add_delayed_iput(BTRFS_I(inode));
		return 0;
	}

	btrfs_add_delayed_iput(BTRFS_I(inode));

	ino_elem = kmalloc(sizeof(*ino_elem), GFP_NOFS);
	if (!ino_elem)
		return -ENOMEM;
	ino_elem->ino = ino;
	ino_elem->parent = parent;
	list_add_tail(&ino_elem->list, &ctx->conflict_inodes);
	ctx->num_conflict_inodes++;

	return 0;
}

static int log_conflicting_inodes(struct btrfs_trans_handle *trans,
				  struct btrfs_root *root,
				  struct btrfs_log_ctx *ctx)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret = 0;

	 
	if (ctx->logging_conflict_inodes)
		return 0;

	ctx->logging_conflict_inodes = true;

	 
	while (!list_empty(&ctx->conflict_inodes)) {
		struct btrfs_ino_list *curr;
		struct inode *inode;
		u64 ino;
		u64 parent;

		curr = list_first_entry(&ctx->conflict_inodes,
					struct btrfs_ino_list, list);
		ino = curr->ino;
		parent = curr->parent;
		list_del(&curr->list);
		kfree(curr);

		inode = btrfs_iget(fs_info->sb, ino, root);
		 
		if (IS_ERR(inode)) {
			ret = PTR_ERR(inode);
			if (ret != -ENOENT)
				break;

			inode = btrfs_iget(fs_info->sb, parent, root);
			if (IS_ERR(inode)) {
				ret = PTR_ERR(inode);
				break;
			}

			 
			ret = btrfs_log_inode(trans, BTRFS_I(inode),
					      LOG_INODE_ALL, ctx);
			btrfs_add_delayed_iput(BTRFS_I(inode));
			if (ret)
				break;
			continue;
		}

		 
		if (!need_log_inode(trans, BTRFS_I(inode))) {
			btrfs_add_delayed_iput(BTRFS_I(inode));
			continue;
		}

		 
		ret = btrfs_log_inode(trans, BTRFS_I(inode), LOG_INODE_EXISTS, ctx);
		btrfs_add_delayed_iput(BTRFS_I(inode));
		if (ret)
			break;
	}

	ctx->logging_conflict_inodes = false;
	if (ret)
		free_conflicting_inodes(ctx);

	return ret;
}

static int copy_inode_items_to_log(struct btrfs_trans_handle *trans,
				   struct btrfs_inode *inode,
				   struct btrfs_key *min_key,
				   const struct btrfs_key *max_key,
				   struct btrfs_path *path,
				   struct btrfs_path *dst_path,
				   const u64 logged_isize,
				   const int inode_only,
				   struct btrfs_log_ctx *ctx,
				   bool *need_log_inode_item)
{
	const u64 i_size = i_size_read(&inode->vfs_inode);
	struct btrfs_root *root = inode->root;
	int ins_start_slot = 0;
	int ins_nr = 0;
	int ret;

	while (1) {
		ret = btrfs_search_forward(root, min_key, path, trans->transid);
		if (ret < 0)
			return ret;
		if (ret > 0) {
			ret = 0;
			break;
		}
again:
		 
		if (min_key->objectid != max_key->objectid)
			break;
		if (min_key->type > max_key->type)
			break;

		if (min_key->type == BTRFS_INODE_ITEM_KEY) {
			*need_log_inode_item = false;
		} else if (min_key->type == BTRFS_EXTENT_DATA_KEY &&
			   min_key->offset >= i_size) {
			 
			break;
		} else if ((min_key->type == BTRFS_INODE_REF_KEY ||
			    min_key->type == BTRFS_INODE_EXTREF_KEY) &&
			   (inode->generation == trans->transid ||
			    ctx->logging_conflict_inodes)) {
			u64 other_ino = 0;
			u64 other_parent = 0;

			ret = btrfs_check_ref_name_override(path->nodes[0],
					path->slots[0], min_key, inode,
					&other_ino, &other_parent);
			if (ret < 0) {
				return ret;
			} else if (ret > 0 &&
				   other_ino != btrfs_ino(BTRFS_I(ctx->inode))) {
				if (ins_nr > 0) {
					ins_nr++;
				} else {
					ins_nr = 1;
					ins_start_slot = path->slots[0];
				}
				ret = copy_items(trans, inode, dst_path, path,
						 ins_start_slot, ins_nr,
						 inode_only, logged_isize);
				if (ret < 0)
					return ret;
				ins_nr = 0;

				btrfs_release_path(path);
				ret = add_conflicting_inode(trans, root, path,
							    other_ino,
							    other_parent, ctx);
				if (ret)
					return ret;
				goto next_key;
			}
		} else if (min_key->type == BTRFS_XATTR_ITEM_KEY) {
			 
			if (ins_nr == 0)
				goto next_slot;
			ret = copy_items(trans, inode, dst_path, path,
					 ins_start_slot,
					 ins_nr, inode_only, logged_isize);
			if (ret < 0)
				return ret;
			ins_nr = 0;
			goto next_slot;
		}

		if (ins_nr && ins_start_slot + ins_nr == path->slots[0]) {
			ins_nr++;
			goto next_slot;
		} else if (!ins_nr) {
			ins_start_slot = path->slots[0];
			ins_nr = 1;
			goto next_slot;
		}

		ret = copy_items(trans, inode, dst_path, path, ins_start_slot,
				 ins_nr, inode_only, logged_isize);
		if (ret < 0)
			return ret;
		ins_nr = 1;
		ins_start_slot = path->slots[0];
next_slot:
		path->slots[0]++;
		if (path->slots[0] < btrfs_header_nritems(path->nodes[0])) {
			btrfs_item_key_to_cpu(path->nodes[0], min_key,
					      path->slots[0]);
			goto again;
		}
		if (ins_nr) {
			ret = copy_items(trans, inode, dst_path, path,
					 ins_start_slot, ins_nr, inode_only,
					 logged_isize);
			if (ret < 0)
				return ret;
			ins_nr = 0;
		}
		btrfs_release_path(path);
next_key:
		if (min_key->offset < (u64)-1) {
			min_key->offset++;
		} else if (min_key->type < max_key->type) {
			min_key->type++;
			min_key->offset = 0;
		} else {
			break;
		}

		 
		cond_resched();
	}
	if (ins_nr) {
		ret = copy_items(trans, inode, dst_path, path, ins_start_slot,
				 ins_nr, inode_only, logged_isize);
		if (ret)
			return ret;
	}

	if (inode_only == LOG_INODE_ALL && S_ISREG(inode->vfs_inode.i_mode)) {
		 
		btrfs_release_path(path);
		ret = btrfs_log_prealloc_extents(trans, inode, dst_path);
	}

	return ret;
}

static int insert_delayed_items_batch(struct btrfs_trans_handle *trans,
				      struct btrfs_root *log,
				      struct btrfs_path *path,
				      const struct btrfs_item_batch *batch,
				      const struct btrfs_delayed_item *first_item)
{
	const struct btrfs_delayed_item *curr = first_item;
	int ret;

	ret = btrfs_insert_empty_items(trans, log, path, batch);
	if (ret)
		return ret;

	for (int i = 0; i < batch->nr; i++) {
		char *data_ptr;

		data_ptr = btrfs_item_ptr(path->nodes[0], path->slots[0], char);
		write_extent_buffer(path->nodes[0], &curr->data,
				    (unsigned long)data_ptr, curr->data_len);
		curr = list_next_entry(curr, log_list);
		path->slots[0]++;
	}

	btrfs_release_path(path);

	return 0;
}

static int log_delayed_insertion_items(struct btrfs_trans_handle *trans,
				       struct btrfs_inode *inode,
				       struct btrfs_path *path,
				       const struct list_head *delayed_ins_list,
				       struct btrfs_log_ctx *ctx)
{
	 
	const int max_batch_size = 195;
	const int leaf_data_size = BTRFS_LEAF_DATA_SIZE(trans->fs_info);
	const u64 ino = btrfs_ino(inode);
	struct btrfs_root *log = inode->root->log_root;
	struct btrfs_item_batch batch = {
		.nr = 0,
		.total_data_size = 0,
	};
	const struct btrfs_delayed_item *first = NULL;
	const struct btrfs_delayed_item *curr;
	char *ins_data;
	struct btrfs_key *ins_keys;
	u32 *ins_sizes;
	u64 curr_batch_size = 0;
	int batch_idx = 0;
	int ret;

	 
	lockdep_assert_held(&inode->log_mutex);

	 
	list_for_each_entry(curr, delayed_ins_list, log_list) {
		if (curr->index > inode->last_dir_index_offset) {
			first = curr;
			break;
		}
	}

	 
	if (!first)
		return 0;

	ins_data = kmalloc(max_batch_size * sizeof(u32) +
			   max_batch_size * sizeof(struct btrfs_key), GFP_NOFS);
	if (!ins_data)
		return -ENOMEM;
	ins_sizes = (u32 *)ins_data;
	batch.data_sizes = ins_sizes;
	ins_keys = (struct btrfs_key *)(ins_data + max_batch_size * sizeof(u32));
	batch.keys = ins_keys;

	curr = first;
	while (!list_entry_is_head(curr, delayed_ins_list, log_list)) {
		const u32 curr_size = curr->data_len + sizeof(struct btrfs_item);

		if (curr_batch_size + curr_size > leaf_data_size ||
		    batch.nr == max_batch_size) {
			ret = insert_delayed_items_batch(trans, log, path,
							 &batch, first);
			if (ret)
				goto out;
			batch_idx = 0;
			batch.nr = 0;
			batch.total_data_size = 0;
			curr_batch_size = 0;
			first = curr;
		}

		ins_sizes[batch_idx] = curr->data_len;
		ins_keys[batch_idx].objectid = ino;
		ins_keys[batch_idx].type = BTRFS_DIR_INDEX_KEY;
		ins_keys[batch_idx].offset = curr->index;
		curr_batch_size += curr_size;
		batch.total_data_size += curr->data_len;
		batch.nr++;
		batch_idx++;
		curr = list_next_entry(curr, log_list);
	}

	ASSERT(batch.nr >= 1);
	ret = insert_delayed_items_batch(trans, log, path, &batch, first);

	curr = list_last_entry(delayed_ins_list, struct btrfs_delayed_item,
			       log_list);
	inode->last_dir_index_offset = curr->index;
out:
	kfree(ins_data);

	return ret;
}

static int log_delayed_deletions_full(struct btrfs_trans_handle *trans,
				      struct btrfs_inode *inode,
				      struct btrfs_path *path,
				      const struct list_head *delayed_del_list,
				      struct btrfs_log_ctx *ctx)
{
	const u64 ino = btrfs_ino(inode);
	const struct btrfs_delayed_item *curr;

	curr = list_first_entry(delayed_del_list, struct btrfs_delayed_item,
				log_list);

	while (!list_entry_is_head(curr, delayed_del_list, log_list)) {
		u64 first_dir_index = curr->index;
		u64 last_dir_index;
		const struct btrfs_delayed_item *next;
		int ret;

		 
		next = list_next_entry(curr, log_list);
		while (!list_entry_is_head(next, delayed_del_list, log_list)) {
			if (next->index != curr->index + 1)
				break;
			curr = next;
			next = list_next_entry(next, log_list);
		}

		last_dir_index = curr->index;
		ASSERT(last_dir_index >= first_dir_index);

		ret = insert_dir_log_key(trans, inode->root->log_root, path,
					 ino, first_dir_index, last_dir_index);
		if (ret)
			return ret;
		curr = list_next_entry(curr, log_list);
	}

	return 0;
}

static int batch_delete_dir_index_items(struct btrfs_trans_handle *trans,
					struct btrfs_inode *inode,
					struct btrfs_path *path,
					struct btrfs_log_ctx *ctx,
					const struct list_head *delayed_del_list,
					const struct btrfs_delayed_item *first,
					const struct btrfs_delayed_item **last_ret)
{
	const struct btrfs_delayed_item *next;
	struct extent_buffer *leaf = path->nodes[0];
	const int last_slot = btrfs_header_nritems(leaf) - 1;
	int slot = path->slots[0] + 1;
	const u64 ino = btrfs_ino(inode);

	next = list_next_entry(first, log_list);

	while (slot < last_slot &&
	       !list_entry_is_head(next, delayed_del_list, log_list)) {
		struct btrfs_key key;

		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid != ino ||
		    key.type != BTRFS_DIR_INDEX_KEY ||
		    key.offset != next->index)
			break;

		slot++;
		*last_ret = next;
		next = list_next_entry(next, log_list);
	}

	return btrfs_del_items(trans, inode->root->log_root, path,
			       path->slots[0], slot - path->slots[0]);
}

static int log_delayed_deletions_incremental(struct btrfs_trans_handle *trans,
					     struct btrfs_inode *inode,
					     struct btrfs_path *path,
					     const struct list_head *delayed_del_list,
					     struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *log = inode->root->log_root;
	const struct btrfs_delayed_item *curr;
	u64 last_range_start = 0;
	u64 last_range_end = 0;
	struct btrfs_key key;

	key.objectid = btrfs_ino(inode);
	key.type = BTRFS_DIR_INDEX_KEY;
	curr = list_first_entry(delayed_del_list, struct btrfs_delayed_item,
				log_list);

	while (!list_entry_is_head(curr, delayed_del_list, log_list)) {
		const struct btrfs_delayed_item *last = curr;
		u64 first_dir_index = curr->index;
		u64 last_dir_index;
		bool deleted_items = false;
		int ret;

		key.offset = curr->index;
		ret = btrfs_search_slot(trans, log, &key, path, -1, 1);
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			ret = batch_delete_dir_index_items(trans, inode, path, ctx,
							   delayed_del_list, curr,
							   &last);
			if (ret)
				return ret;
			deleted_items = true;
		}

		btrfs_release_path(path);

		 
		if (deleted_items)
			goto next_batch;

		last_dir_index = last->index;
		ASSERT(last_dir_index >= first_dir_index);
		 
		if (last_range_end != 0 && first_dir_index == last_range_end + 1)
			first_dir_index = last_range_start;

		ret = insert_dir_log_key(trans, log, path, key.objectid,
					 first_dir_index, last_dir_index);
		if (ret)
			return ret;

		last_range_start = first_dir_index;
		last_range_end = last_dir_index;
next_batch:
		curr = list_next_entry(last, log_list);
	}

	return 0;
}

static int log_delayed_deletion_items(struct btrfs_trans_handle *trans,
				      struct btrfs_inode *inode,
				      struct btrfs_path *path,
				      const struct list_head *delayed_del_list,
				      struct btrfs_log_ctx *ctx)
{
	 
	lockdep_assert_held(&inode->log_mutex);

	if (list_empty(delayed_del_list))
		return 0;

	if (ctx->logged_before)
		return log_delayed_deletions_incremental(trans, inode, path,
							 delayed_del_list, ctx);

	return log_delayed_deletions_full(trans, inode, path, delayed_del_list,
					  ctx);
}

 
static int log_new_delayed_dentries(struct btrfs_trans_handle *trans,
				    struct btrfs_inode *inode,
				    const struct list_head *delayed_ins_list,
				    struct btrfs_log_ctx *ctx)
{
	const bool orig_log_new_dentries = ctx->log_new_dentries;
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_delayed_item *item;
	int ret = 0;

	 
	lockdep_assert_not_held(&inode->log_mutex);

	ASSERT(!ctx->logging_new_delayed_dentries);
	ctx->logging_new_delayed_dentries = true;

	list_for_each_entry(item, delayed_ins_list, log_list) {
		struct btrfs_dir_item *dir_item;
		struct inode *di_inode;
		struct btrfs_key key;
		int log_mode = LOG_INODE_EXISTS;

		dir_item = (struct btrfs_dir_item *)item->data;
		btrfs_disk_key_to_cpu(&key, &dir_item->location);

		if (key.type == BTRFS_ROOT_ITEM_KEY)
			continue;

		di_inode = btrfs_iget(fs_info->sb, key.objectid, inode->root);
		if (IS_ERR(di_inode)) {
			ret = PTR_ERR(di_inode);
			break;
		}

		if (!need_log_inode(trans, BTRFS_I(di_inode))) {
			btrfs_add_delayed_iput(BTRFS_I(di_inode));
			continue;
		}

		if (btrfs_stack_dir_ftype(dir_item) == BTRFS_FT_DIR)
			log_mode = LOG_INODE_ALL;

		ctx->log_new_dentries = false;
		ret = btrfs_log_inode(trans, BTRFS_I(di_inode), log_mode, ctx);

		if (!ret && ctx->log_new_dentries)
			ret = log_new_dir_dentries(trans, BTRFS_I(di_inode), ctx);

		btrfs_add_delayed_iput(BTRFS_I(di_inode));

		if (ret)
			break;
	}

	ctx->log_new_dentries = orig_log_new_dentries;
	ctx->logging_new_delayed_dentries = false;

	return ret;
}

 
static int btrfs_log_inode(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode,
			   int inode_only,
			   struct btrfs_log_ctx *ctx)
{
	struct btrfs_path *path;
	struct btrfs_path *dst_path;
	struct btrfs_key min_key;
	struct btrfs_key max_key;
	struct btrfs_root *log = inode->root->log_root;
	int ret;
	bool fast_search = false;
	u64 ino = btrfs_ino(inode);
	struct extent_map_tree *em_tree = &inode->extent_tree;
	u64 logged_isize = 0;
	bool need_log_inode_item = true;
	bool xattrs_logged = false;
	bool inode_item_dropped = true;
	bool full_dir_logging = false;
	LIST_HEAD(delayed_ins_list);
	LIST_HEAD(delayed_del_list);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	dst_path = btrfs_alloc_path();
	if (!dst_path) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	min_key.objectid = ino;
	min_key.type = BTRFS_INODE_ITEM_KEY;
	min_key.offset = 0;

	max_key.objectid = ino;


	 
	if (S_ISDIR(inode->vfs_inode.i_mode) ||
	    (!test_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
		       &inode->runtime_flags) &&
	     inode_only >= LOG_INODE_EXISTS))
		max_key.type = BTRFS_XATTR_ITEM_KEY;
	else
		max_key.type = (u8)-1;
	max_key.offset = (u64)-1;

	if (S_ISDIR(inode->vfs_inode.i_mode) && inode_only == LOG_INODE_ALL)
		full_dir_logging = true;

	 
	if (full_dir_logging && ctx->logging_new_delayed_dentries) {
		ret = btrfs_commit_inode_delayed_items(trans, inode);
		if (ret)
			goto out;
	}

	mutex_lock(&inode->log_mutex);

	 
	if (S_ISLNK(inode->vfs_inode.i_mode))
		inode_only = LOG_INODE_ALL;

	 
	ret = inode_logged(trans, inode, path);
	if (ret < 0)
		goto out_unlock;
	ctx->logged_before = (ret == 1);
	ret = 0;

	 
	if (full_dir_logging && inode->last_unlink_trans >= trans->transid) {
		ret = BTRFS_LOG_FORCE_COMMIT;
		goto out_unlock;
	}

	 
	if (S_ISDIR(inode->vfs_inode.i_mode)) {
		clear_bit(BTRFS_INODE_COPY_EVERYTHING, &inode->runtime_flags);
		if (ctx->logged_before)
			ret = drop_inode_items(trans, log, path, inode,
					       BTRFS_XATTR_ITEM_KEY);
	} else {
		if (inode_only == LOG_INODE_EXISTS && ctx->logged_before) {
			 
			ret = logged_inode_size(log, inode, path, &logged_isize);
			if (ret)
				goto out_unlock;
		}
		if (test_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
			     &inode->runtime_flags)) {
			if (inode_only == LOG_INODE_EXISTS) {
				max_key.type = BTRFS_XATTR_ITEM_KEY;
				if (ctx->logged_before)
					ret = drop_inode_items(trans, log, path,
							       inode, max_key.type);
			} else {
				clear_bit(BTRFS_INODE_NEEDS_FULL_SYNC,
					  &inode->runtime_flags);
				clear_bit(BTRFS_INODE_COPY_EVERYTHING,
					  &inode->runtime_flags);
				if (ctx->logged_before)
					ret = truncate_inode_items(trans, log,
								   inode, 0, 0);
			}
		} else if (test_and_clear_bit(BTRFS_INODE_COPY_EVERYTHING,
					      &inode->runtime_flags) ||
			   inode_only == LOG_INODE_EXISTS) {
			if (inode_only == LOG_INODE_ALL)
				fast_search = true;
			max_key.type = BTRFS_XATTR_ITEM_KEY;
			if (ctx->logged_before)
				ret = drop_inode_items(trans, log, path, inode,
						       max_key.type);
		} else {
			if (inode_only == LOG_INODE_ALL)
				fast_search = true;
			inode_item_dropped = false;
			goto log_extents;
		}

	}
	if (ret)
		goto out_unlock;

	 
	if (full_dir_logging && !ctx->logging_new_delayed_dentries)
		btrfs_log_get_delayed_items(inode, &delayed_ins_list,
					    &delayed_del_list);

	ret = copy_inode_items_to_log(trans, inode, &min_key, &max_key,
				      path, dst_path, logged_isize,
				      inode_only, ctx,
				      &need_log_inode_item);
	if (ret)
		goto out_unlock;

	btrfs_release_path(path);
	btrfs_release_path(dst_path);
	ret = btrfs_log_all_xattrs(trans, inode, path, dst_path);
	if (ret)
		goto out_unlock;
	xattrs_logged = true;
	if (max_key.type >= BTRFS_EXTENT_DATA_KEY && !fast_search) {
		btrfs_release_path(path);
		btrfs_release_path(dst_path);
		ret = btrfs_log_holes(trans, inode, path);
		if (ret)
			goto out_unlock;
	}
log_extents:
	btrfs_release_path(path);
	btrfs_release_path(dst_path);
	if (need_log_inode_item) {
		ret = log_inode_item(trans, log, dst_path, inode, inode_item_dropped);
		if (ret)
			goto out_unlock;
		 
		if (!xattrs_logged && inode->logged_trans < trans->transid) {
			ret = btrfs_log_all_xattrs(trans, inode, path, dst_path);
			if (ret)
				goto out_unlock;
			btrfs_release_path(path);
		}
	}
	if (fast_search) {
		ret = btrfs_log_changed_extents(trans, inode, dst_path, ctx);
		if (ret)
			goto out_unlock;
	} else if (inode_only == LOG_INODE_ALL) {
		struct extent_map *em, *n;

		write_lock(&em_tree->lock);
		list_for_each_entry_safe(em, n, &em_tree->modified_extents, list)
			list_del_init(&em->list);
		write_unlock(&em_tree->lock);
	}

	if (full_dir_logging) {
		ret = log_directory_changes(trans, inode, path, dst_path, ctx);
		if (ret)
			goto out_unlock;
		ret = log_delayed_insertion_items(trans, inode, path,
						  &delayed_ins_list, ctx);
		if (ret)
			goto out_unlock;
		ret = log_delayed_deletion_items(trans, inode, path,
						 &delayed_del_list, ctx);
		if (ret)
			goto out_unlock;
	}

	spin_lock(&inode->lock);
	inode->logged_trans = trans->transid;
	 
	if (inode_only != LOG_INODE_EXISTS)
		inode->last_log_commit = inode->last_sub_trans;
	spin_unlock(&inode->lock);

	 
	if (inode_only == LOG_INODE_ALL)
		inode->last_reflink_trans = 0;

out_unlock:
	mutex_unlock(&inode->log_mutex);
out:
	btrfs_free_path(path);
	btrfs_free_path(dst_path);

	if (ret)
		free_conflicting_inodes(ctx);
	else
		ret = log_conflicting_inodes(trans, inode->root, ctx);

	if (full_dir_logging && !ctx->logging_new_delayed_dentries) {
		if (!ret)
			ret = log_new_delayed_dentries(trans, inode,
						       &delayed_ins_list, ctx);

		btrfs_log_put_delayed_items(inode, &delayed_ins_list,
					    &delayed_del_list);
	}

	return ret;
}

static int btrfs_log_all_parents(struct btrfs_trans_handle *trans,
				 struct btrfs_inode *inode,
				 struct btrfs_log_ctx *ctx)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int ret;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_root *root = inode->root;
	const u64 ino = btrfs_ino(inode);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	path->skip_locking = 1;
	path->search_commit_root = 1;

	key.objectid = ino;
	key.type = BTRFS_INODE_REF_KEY;
	key.offset = 0;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		u32 cur_offset = 0;
		u32 item_size;
		unsigned long ptr;

		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &key, slot);
		 
		if (key.objectid != ino || key.type > BTRFS_INODE_EXTREF_KEY)
			break;

		item_size = btrfs_item_size(leaf, slot);
		ptr = btrfs_item_ptr_offset(leaf, slot);
		while (cur_offset < item_size) {
			struct btrfs_key inode_key;
			struct inode *dir_inode;

			inode_key.type = BTRFS_INODE_ITEM_KEY;
			inode_key.offset = 0;

			if (key.type == BTRFS_INODE_EXTREF_KEY) {
				struct btrfs_inode_extref *extref;

				extref = (struct btrfs_inode_extref *)
					(ptr + cur_offset);
				inode_key.objectid = btrfs_inode_extref_parent(
					leaf, extref);
				cur_offset += sizeof(*extref);
				cur_offset += btrfs_inode_extref_name_len(leaf,
					extref);
			} else {
				inode_key.objectid = key.offset;
				cur_offset = item_size;
			}

			dir_inode = btrfs_iget(fs_info->sb, inode_key.objectid,
					       root);
			 
			if (IS_ERR(dir_inode)) {
				ret = PTR_ERR(dir_inode);
				goto out;
			}

			if (!need_log_inode(trans, BTRFS_I(dir_inode))) {
				btrfs_add_delayed_iput(BTRFS_I(dir_inode));
				continue;
			}

			ctx->log_new_dentries = false;
			ret = btrfs_log_inode(trans, BTRFS_I(dir_inode),
					      LOG_INODE_ALL, ctx);
			if (!ret && ctx->log_new_dentries)
				ret = log_new_dir_dentries(trans,
						   BTRFS_I(dir_inode), ctx);
			btrfs_add_delayed_iput(BTRFS_I(dir_inode));
			if (ret)
				goto out;
		}
		path->slots[0]++;
	}
	ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

static int log_new_ancestors(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct btrfs_path *path,
			     struct btrfs_log_ctx *ctx)
{
	struct btrfs_key found_key;

	btrfs_item_key_to_cpu(path->nodes[0], &found_key, path->slots[0]);

	while (true) {
		struct btrfs_fs_info *fs_info = root->fs_info;
		struct extent_buffer *leaf;
		int slot;
		struct btrfs_key search_key;
		struct inode *inode;
		u64 ino;
		int ret = 0;

		btrfs_release_path(path);

		ino = found_key.offset;

		search_key.objectid = found_key.offset;
		search_key.type = BTRFS_INODE_ITEM_KEY;
		search_key.offset = 0;
		inode = btrfs_iget(fs_info->sb, ino, root);
		if (IS_ERR(inode))
			return PTR_ERR(inode);

		if (BTRFS_I(inode)->generation >= trans->transid &&
		    need_log_inode(trans, BTRFS_I(inode)))
			ret = btrfs_log_inode(trans, BTRFS_I(inode),
					      LOG_INODE_EXISTS, ctx);
		btrfs_add_delayed_iput(BTRFS_I(inode));
		if (ret)
			return ret;

		if (search_key.objectid == BTRFS_FIRST_FREE_OBJECTID)
			break;

		search_key.type = BTRFS_INODE_REF_KEY;
		ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
		if (ret < 0)
			return ret;

		leaf = path->nodes[0];
		slot = path->slots[0];
		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				return ret;
			else if (ret > 0)
				return -ENOENT;
			leaf = path->nodes[0];
			slot = path->slots[0];
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);
		if (found_key.objectid != search_key.objectid ||
		    found_key.type != BTRFS_INODE_REF_KEY)
			return -ENOENT;
	}
	return 0;
}

static int log_new_ancestors_fast(struct btrfs_trans_handle *trans,
				  struct btrfs_inode *inode,
				  struct dentry *parent,
				  struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	struct dentry *old_parent = NULL;
	struct super_block *sb = inode->vfs_inode.i_sb;
	int ret = 0;

	while (true) {
		if (!parent || d_really_is_negative(parent) ||
		    sb != parent->d_sb)
			break;

		inode = BTRFS_I(d_inode(parent));
		if (root != inode->root)
			break;

		if (inode->generation >= trans->transid &&
		    need_log_inode(trans, inode)) {
			ret = btrfs_log_inode(trans, inode,
					      LOG_INODE_EXISTS, ctx);
			if (ret)
				break;
		}
		if (IS_ROOT(parent))
			break;

		parent = dget_parent(parent);
		dput(old_parent);
		old_parent = parent;
	}
	dput(old_parent);

	return ret;
}

static int log_all_new_ancestors(struct btrfs_trans_handle *trans,
				 struct btrfs_inode *inode,
				 struct dentry *parent,
				 struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	const u64 ino = btrfs_ino(inode);
	struct btrfs_path *path;
	struct btrfs_key search_key;
	int ret;

	 
	if (inode->vfs_inode.i_nlink < 2)
		return log_new_ancestors_fast(trans, inode, parent, ctx);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	search_key.objectid = ino;
	search_key.type = BTRFS_INODE_REF_KEY;
	search_key.offset = 0;
again:
	ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret == 0)
		path->slots[0]++;

	while (true) {
		struct extent_buffer *leaf = path->nodes[0];
		int slot = path->slots[0];
		struct btrfs_key found_key;

		if (slot >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				goto out;
			else if (ret > 0)
				break;
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);
		if (found_key.objectid != ino ||
		    found_key.type > BTRFS_INODE_EXTREF_KEY)
			break;

		 
		if (found_key.type == BTRFS_INODE_EXTREF_KEY) {
			ret = -EMLINK;
			goto out;
		}

		 
		memcpy(&search_key, &found_key, sizeof(search_key));

		ret = log_new_ancestors(trans, root, path, ctx);
		if (ret)
			goto out;
		btrfs_release_path(path);
		goto again;
	}
	ret = 0;
out:
	btrfs_free_path(path);
	return ret;
}

 
static int btrfs_log_inode_parent(struct btrfs_trans_handle *trans,
				  struct btrfs_inode *inode,
				  struct dentry *parent,
				  int inode_only,
				  struct btrfs_log_ctx *ctx)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret = 0;
	bool log_dentries = false;

	if (btrfs_test_opt(fs_info, NOTREELOG)) {
		ret = BTRFS_LOG_FORCE_COMMIT;
		goto end_no_trans;
	}

	if (btrfs_root_refs(&root->root_item) == 0) {
		ret = BTRFS_LOG_FORCE_COMMIT;
		goto end_no_trans;
	}

	 
	if ((btrfs_inode_in_log(inode, trans->transid) &&
	     list_empty(&ctx->ordered_extents)) ||
	    inode->vfs_inode.i_nlink == 0) {
		ret = BTRFS_NO_LOG_SYNC;
		goto end_no_trans;
	}

	ret = start_log_trans(trans, root, ctx);
	if (ret)
		goto end_no_trans;

	ret = btrfs_log_inode(trans, inode, inode_only, ctx);
	if (ret)
		goto end_trans;

	 
	if (S_ISREG(inode->vfs_inode.i_mode) &&
	    inode->generation < trans->transid &&
	    inode->last_unlink_trans < trans->transid) {
		ret = 0;
		goto end_trans;
	}

	if (S_ISDIR(inode->vfs_inode.i_mode) && ctx->log_new_dentries)
		log_dentries = true;

	 
	if (inode->last_unlink_trans >= trans->transid) {
		ret = btrfs_log_all_parents(trans, inode, ctx);
		if (ret)
			goto end_trans;
	}

	ret = log_all_new_ancestors(trans, inode, parent, ctx);
	if (ret)
		goto end_trans;

	if (log_dentries)
		ret = log_new_dir_dentries(trans, inode, ctx);
	else
		ret = 0;
end_trans:
	if (ret < 0) {
		btrfs_set_log_full_commit(trans);
		ret = BTRFS_LOG_FORCE_COMMIT;
	}

	if (ret)
		btrfs_remove_log_ctx(root, ctx);
	btrfs_end_log_trans(root);
end_no_trans:
	return ret;
}

 
int btrfs_log_dentry_safe(struct btrfs_trans_handle *trans,
			  struct dentry *dentry,
			  struct btrfs_log_ctx *ctx)
{
	struct dentry *parent = dget_parent(dentry);
	int ret;

	ret = btrfs_log_inode_parent(trans, BTRFS_I(d_inode(dentry)), parent,
				     LOG_INODE_ALL, ctx);
	dput(parent);

	return ret;
}

 
int btrfs_recover_log_trees(struct btrfs_root *log_root_tree)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_trans_handle *trans;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_root *log;
	struct btrfs_fs_info *fs_info = log_root_tree->fs_info;
	struct walk_control wc = {
		.process_func = process_one_buffer,
		.stage = LOG_WALK_PIN_ONLY,
	};

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	set_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags);

	trans = btrfs_start_transaction(fs_info->tree_root, 0);
	if (IS_ERR(trans)) {
		ret = PTR_ERR(trans);
		goto error;
	}

	wc.trans = trans;
	wc.pin = 1;

	ret = walk_log_tree(trans, log_root_tree, &wc);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		goto error;
	}

again:
	key.objectid = BTRFS_TREE_LOG_OBJECTID;
	key.offset = (u64)-1;
	key.type = BTRFS_ROOT_ITEM_KEY;

	while (1) {
		ret = btrfs_search_slot(NULL, log_root_tree, &key, path, 0, 0);

		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			goto error;
		}
		if (ret > 0) {
			if (path->slots[0] == 0)
				break;
			path->slots[0]--;
		}
		btrfs_item_key_to_cpu(path->nodes[0], &found_key,
				      path->slots[0]);
		btrfs_release_path(path);
		if (found_key.objectid != BTRFS_TREE_LOG_OBJECTID)
			break;

		log = btrfs_read_tree_root(log_root_tree, &found_key);
		if (IS_ERR(log)) {
			ret = PTR_ERR(log);
			btrfs_abort_transaction(trans, ret);
			goto error;
		}

		wc.replay_dest = btrfs_get_fs_root(fs_info, found_key.offset,
						   true);
		if (IS_ERR(wc.replay_dest)) {
			ret = PTR_ERR(wc.replay_dest);

			 
			if (ret == -ENOENT)
				ret = btrfs_pin_extent_for_log_replay(trans,
							log->node->start,
							log->node->len);
			btrfs_put_root(log);

			if (!ret)
				goto next;
			btrfs_abort_transaction(trans, ret);
			goto error;
		}

		wc.replay_dest->log_root = log;
		ret = btrfs_record_root_in_trans(trans, wc.replay_dest);
		if (ret)
			 
			btrfs_abort_transaction(trans, ret);
		else
			ret = walk_log_tree(trans, log, &wc);

		if (!ret && wc.stage == LOG_WALK_REPLAY_ALL) {
			ret = fixup_inode_link_counts(trans, wc.replay_dest,
						      path);
			if (ret)
				btrfs_abort_transaction(trans, ret);
		}

		if (!ret && wc.stage == LOG_WALK_REPLAY_ALL) {
			struct btrfs_root *root = wc.replay_dest;

			btrfs_release_path(path);

			 
			ret = btrfs_init_root_free_objectid(root);
			if (ret)
				btrfs_abort_transaction(trans, ret);
		}

		wc.replay_dest->log_root = NULL;
		btrfs_put_root(wc.replay_dest);
		btrfs_put_root(log);

		if (ret)
			goto error;
next:
		if (found_key.offset == 0)
			break;
		key.offset = found_key.offset - 1;
	}
	btrfs_release_path(path);

	 
	if (wc.pin) {
		wc.pin = 0;
		wc.process_func = replay_one_buffer;
		wc.stage = LOG_WALK_REPLAY_INODES;
		goto again;
	}
	 
	if (wc.stage < LOG_WALK_REPLAY_ALL) {
		wc.stage++;
		goto again;
	}

	btrfs_free_path(path);

	 
	ret = btrfs_commit_transaction(trans);
	if (ret)
		return ret;

	log_root_tree->log_root = NULL;
	clear_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags);
	btrfs_put_root(log_root_tree);

	return 0;
error:
	if (wc.trans)
		btrfs_end_transaction(wc.trans);
	clear_bit(BTRFS_FS_LOG_RECOVERING, &fs_info->flags);
	btrfs_free_path(path);
	return ret;
}

 
void btrfs_record_unlink_dir(struct btrfs_trans_handle *trans,
			     struct btrfs_inode *dir, struct btrfs_inode *inode,
			     bool for_rename)
{
	 
	mutex_lock(&inode->log_mutex);
	inode->last_unlink_trans = trans->transid;
	mutex_unlock(&inode->log_mutex);

	if (!for_rename)
		return;

	 
	if (inode_logged(trans, dir, NULL) == 1)
		return;

	 
	if (inode_logged(trans, inode, NULL) == 1)
		return;

	 
	mutex_lock(&dir->log_mutex);
	dir->last_unlink_trans = trans->transid;
	mutex_unlock(&dir->log_mutex);
}

 
void btrfs_record_snapshot_destroy(struct btrfs_trans_handle *trans,
				   struct btrfs_inode *dir)
{
	mutex_lock(&dir->log_mutex);
	dir->last_unlink_trans = trans->transid;
	mutex_unlock(&dir->log_mutex);
}

 
void btrfs_log_new_name(struct btrfs_trans_handle *trans,
			struct dentry *old_dentry, struct btrfs_inode *old_dir,
			u64 old_dir_index, struct dentry *parent)
{
	struct btrfs_inode *inode = BTRFS_I(d_inode(old_dentry));
	struct btrfs_root *root = inode->root;
	struct btrfs_log_ctx ctx;
	bool log_pinned = false;
	int ret;

	 
	if (!S_ISDIR(inode->vfs_inode.i_mode))
		inode->last_unlink_trans = trans->transid;

	 
	ret = inode_logged(trans, inode, NULL);
	if (ret < 0) {
		goto out;
	} else if (ret == 0) {
		if (!old_dir)
			return;
		 
		ret = inode_logged(trans, old_dir, NULL);
		if (ret < 0)
			goto out;
		else if (ret == 0)
			return;
	}
	ret = 0;

	 
	if (old_dir && old_dir->logged_trans == trans->transid) {
		struct btrfs_root *log = old_dir->root->log_root;
		struct btrfs_path *path;
		struct fscrypt_name fname;

		ASSERT(old_dir_index >= BTRFS_DIR_START_INDEX);

		ret = fscrypt_setup_filename(&old_dir->vfs_inode,
					     &old_dentry->d_name, 0, &fname);
		if (ret)
			goto out;
		 
		ret = join_running_log_trans(root);
		 
		if (WARN_ON_ONCE(ret < 0)) {
			fscrypt_free_filename(&fname);
			goto out;
		}

		log_pinned = true;

		path = btrfs_alloc_path();
		if (!path) {
			ret = -ENOMEM;
			fscrypt_free_filename(&fname);
			goto out;
		}

		 
		mutex_lock(&old_dir->log_mutex);
		ret = del_logged_dentry(trans, log, path, btrfs_ino(old_dir),
					&fname.disk_name, old_dir_index);
		if (ret > 0) {
			 
			btrfs_release_path(path);
			ret = insert_dir_log_key(trans, log, path,
						 btrfs_ino(old_dir),
						 old_dir_index, old_dir_index);
		}
		mutex_unlock(&old_dir->log_mutex);

		btrfs_free_path(path);
		fscrypt_free_filename(&fname);
		if (ret < 0)
			goto out;
	}

	btrfs_init_log_ctx(&ctx, &inode->vfs_inode);
	ctx.logging_new_name = true;
	 
	btrfs_log_inode_parent(trans, inode, parent, LOG_INODE_EXISTS, &ctx);
	ASSERT(list_empty(&ctx.conflict_inodes));
out:
	 
	if (ret < 0)
		btrfs_set_log_full_commit(trans);
	if (log_pinned)
		btrfs_end_log_trans(root);
}


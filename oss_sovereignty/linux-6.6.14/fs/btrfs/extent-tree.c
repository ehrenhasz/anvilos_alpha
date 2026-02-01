
 

#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pagemap.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/sort.h>
#include <linux/rcupdate.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/percpu_counter.h>
#include <linux/lockdep.h>
#include <linux/crc32c.h>
#include "ctree.h"
#include "extent-tree.h"
#include "tree-log.h"
#include "disk-io.h"
#include "print-tree.h"
#include "volumes.h"
#include "raid56.h"
#include "locking.h"
#include "free-space-cache.h"
#include "free-space-tree.h"
#include "sysfs.h"
#include "qgroup.h"
#include "ref-verify.h"
#include "space-info.h"
#include "block-rsv.h"
#include "delalloc-space.h"
#include "discard.h"
#include "rcu-string.h"
#include "zoned.h"
#include "dev-replace.h"
#include "fs.h"
#include "accessors.h"
#include "root-tree.h"
#include "file-item.h"
#include "orphan.h"
#include "tree-checker.h"

#undef SCRAMBLE_DELAYED_REFS


static int __btrfs_free_extent(struct btrfs_trans_handle *trans,
			       struct btrfs_delayed_ref_node *node, u64 parent,
			       u64 root_objectid, u64 owner_objectid,
			       u64 owner_offset, int refs_to_drop,
			       struct btrfs_delayed_extent_op *extra_op);
static void __run_delayed_extent_op(struct btrfs_delayed_extent_op *extent_op,
				    struct extent_buffer *leaf,
				    struct btrfs_extent_item *ei);
static int alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				      u64 parent, u64 root_objectid,
				      u64 flags, u64 owner, u64 offset,
				      struct btrfs_key *ins, int ref_mod);
static int alloc_reserved_tree_block(struct btrfs_trans_handle *trans,
				     struct btrfs_delayed_ref_node *node,
				     struct btrfs_delayed_extent_op *extent_op);
static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key);

static int block_group_bits(struct btrfs_block_group *cache, u64 bits)
{
	return (cache->flags & bits) == bits;
}

 
int btrfs_lookup_data_extent(struct btrfs_fs_info *fs_info, u64 start, u64 len)
{
	struct btrfs_root *root = btrfs_extent_root(fs_info, start);
	int ret;
	struct btrfs_key key;
	struct btrfs_path *path;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = start;
	key.offset = len;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	btrfs_free_path(path);
	return ret;
}

 
int btrfs_lookup_extent_info(struct btrfs_trans_handle *trans,
			     struct btrfs_fs_info *fs_info, u64 bytenr,
			     u64 offset, int metadata, u64 *refs, u64 *flags)
{
	struct btrfs_root *extent_root;
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_path *path;
	struct btrfs_extent_item *ei;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	u32 item_size;
	u64 num_refs;
	u64 extent_flags;
	int ret;

	 
	if (metadata && !btrfs_fs_incompat(fs_info, SKINNY_METADATA)) {
		offset = fs_info->nodesize;
		metadata = 0;
	}

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	if (!trans) {
		path->skip_locking = 1;
		path->search_commit_root = 1;
	}

search_again:
	key.objectid = bytenr;
	key.offset = offset;
	if (metadata)
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;

	extent_root = btrfs_extent_root(fs_info, bytenr);
	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out_free;

	if (ret > 0 && metadata && key.type == BTRFS_METADATA_ITEM_KEY) {
		if (path->slots[0]) {
			path->slots[0]--;
			btrfs_item_key_to_cpu(path->nodes[0], &key,
					      path->slots[0]);
			if (key.objectid == bytenr &&
			    key.type == BTRFS_EXTENT_ITEM_KEY &&
			    key.offset == fs_info->nodesize)
				ret = 0;
		}
	}

	if (ret == 0) {
		leaf = path->nodes[0];
		item_size = btrfs_item_size(leaf, path->slots[0]);
		if (item_size >= sizeof(*ei)) {
			ei = btrfs_item_ptr(leaf, path->slots[0],
					    struct btrfs_extent_item);
			num_refs = btrfs_extent_refs(leaf, ei);
			extent_flags = btrfs_extent_flags(leaf, ei);
		} else {
			ret = -EUCLEAN;
			btrfs_err(fs_info,
			"unexpected extent item size, has %u expect >= %zu",
				  item_size, sizeof(*ei));
			if (trans)
				btrfs_abort_transaction(trans, ret);
			else
				btrfs_handle_fs_error(fs_info, ret, NULL);

			goto out_free;
		}

		BUG_ON(num_refs == 0);
	} else {
		num_refs = 0;
		extent_flags = 0;
		ret = 0;
	}

	if (!trans)
		goto out;

	delayed_refs = &trans->transaction->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(delayed_refs, bytenr);
	if (head) {
		if (!mutex_trylock(&head->mutex)) {
			refcount_inc(&head->refs);
			spin_unlock(&delayed_refs->lock);

			btrfs_release_path(path);

			 
			mutex_lock(&head->mutex);
			mutex_unlock(&head->mutex);
			btrfs_put_delayed_ref_head(head);
			goto search_again;
		}
		spin_lock(&head->lock);
		if (head->extent_op && head->extent_op->update_flags)
			extent_flags |= head->extent_op->flags_to_set;
		else
			BUG_ON(num_refs == 0);

		num_refs += head->ref_mod;
		spin_unlock(&head->lock);
		mutex_unlock(&head->mutex);
	}
	spin_unlock(&delayed_refs->lock);
out:
	WARN_ON(num_refs == 0);
	if (refs)
		*refs = num_refs;
	if (flags)
		*flags = extent_flags;
out_free:
	btrfs_free_path(path);
	return ret;
}

 

 
int btrfs_get_extent_inline_ref_type(const struct extent_buffer *eb,
				     struct btrfs_extent_inline_ref *iref,
				     enum btrfs_inline_ref_type is_data)
{
	int type = btrfs_extent_inline_ref_type(eb, iref);
	u64 offset = btrfs_extent_inline_ref_offset(eb, iref);

	if (type == BTRFS_TREE_BLOCK_REF_KEY ||
	    type == BTRFS_SHARED_BLOCK_REF_KEY ||
	    type == BTRFS_SHARED_DATA_REF_KEY ||
	    type == BTRFS_EXTENT_DATA_REF_KEY) {
		if (is_data == BTRFS_REF_TYPE_BLOCK) {
			if (type == BTRFS_TREE_BLOCK_REF_KEY)
				return type;
			if (type == BTRFS_SHARED_BLOCK_REF_KEY) {
				ASSERT(eb->fs_info);
				 
				if (offset &&
				    IS_ALIGNED(offset, eb->fs_info->sectorsize))
					return type;
			}
		} else if (is_data == BTRFS_REF_TYPE_DATA) {
			if (type == BTRFS_EXTENT_DATA_REF_KEY)
				return type;
			if (type == BTRFS_SHARED_DATA_REF_KEY) {
				ASSERT(eb->fs_info);
				 
				if (offset &&
				    IS_ALIGNED(offset, eb->fs_info->sectorsize))
					return type;
			}
		} else {
			ASSERT(is_data == BTRFS_REF_TYPE_ANY);
			return type;
		}
	}

	WARN_ON(1);
	btrfs_print_leaf(eb);
	btrfs_err(eb->fs_info,
		  "eb %llu iref 0x%lx invalid extent inline ref type %d",
		  eb->start, (unsigned long)iref, type);

	return BTRFS_REF_TYPE_INVALID;
}

u64 hash_extent_data_ref(u64 root_objectid, u64 owner, u64 offset)
{
	u32 high_crc = ~(u32)0;
	u32 low_crc = ~(u32)0;
	__le64 lenum;

	lenum = cpu_to_le64(root_objectid);
	high_crc = btrfs_crc32c(high_crc, &lenum, sizeof(lenum));
	lenum = cpu_to_le64(owner);
	low_crc = btrfs_crc32c(low_crc, &lenum, sizeof(lenum));
	lenum = cpu_to_le64(offset);
	low_crc = btrfs_crc32c(low_crc, &lenum, sizeof(lenum));

	return ((u64)high_crc << 31) ^ (u64)low_crc;
}

static u64 hash_extent_data_ref_item(struct extent_buffer *leaf,
				     struct btrfs_extent_data_ref *ref)
{
	return hash_extent_data_ref(btrfs_extent_data_ref_root(leaf, ref),
				    btrfs_extent_data_ref_objectid(leaf, ref),
				    btrfs_extent_data_ref_offset(leaf, ref));
}

static int match_extent_data_ref(struct extent_buffer *leaf,
				 struct btrfs_extent_data_ref *ref,
				 u64 root_objectid, u64 owner, u64 offset)
{
	if (btrfs_extent_data_ref_root(leaf, ref) != root_objectid ||
	    btrfs_extent_data_ref_objectid(leaf, ref) != owner ||
	    btrfs_extent_data_ref_offset(leaf, ref) != offset)
		return 0;
	return 1;
}

static noinline int lookup_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid,
					   u64 owner, u64 offset)
{
	struct btrfs_root *root = btrfs_extent_root(trans->fs_info, bytenr);
	struct btrfs_key key;
	struct btrfs_extent_data_ref *ref;
	struct extent_buffer *leaf;
	u32 nritems;
	int ret;
	int recow;
	int err = -ENOENT;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
	}
again:
	recow = 0;
	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret < 0) {
		err = ret;
		goto fail;
	}

	if (parent) {
		if (!ret)
			return 0;
		goto fail;
	}

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);
	while (1) {
		if (path->slots[0] >= nritems) {
			ret = btrfs_next_leaf(root, path);
			if (ret < 0)
				err = ret;
			if (ret)
				goto fail;

			leaf = path->nodes[0];
			nritems = btrfs_header_nritems(leaf);
			recow = 1;
		}

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.objectid != bytenr ||
		    key.type != BTRFS_EXTENT_DATA_REF_KEY)
			goto fail;

		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_data_ref);

		if (match_extent_data_ref(leaf, ref, root_objectid,
					  owner, offset)) {
			if (recow) {
				btrfs_release_path(path);
				goto again;
			}
			err = 0;
			break;
		}
		path->slots[0]++;
	}
fail:
	return err;
}

static noinline int insert_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_path *path,
					   u64 bytenr, u64 parent,
					   u64 root_objectid, u64 owner,
					   u64 offset, int refs_to_add)
{
	struct btrfs_root *root = btrfs_extent_root(trans->fs_info, bytenr);
	struct btrfs_key key;
	struct extent_buffer *leaf;
	u32 size;
	u32 num_refs;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_DATA_REF_KEY;
		key.offset = parent;
		size = sizeof(struct btrfs_shared_data_ref);
	} else {
		key.type = BTRFS_EXTENT_DATA_REF_KEY;
		key.offset = hash_extent_data_ref(root_objectid,
						  owner, offset);
		size = sizeof(struct btrfs_extent_data_ref);
	}

	ret = btrfs_insert_empty_item(trans, root, path, &key, size);
	if (ret && ret != -EEXIST)
		goto fail;

	leaf = path->nodes[0];
	if (parent) {
		struct btrfs_shared_data_ref *ref;
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_shared_data_ref);
		if (ret == 0) {
			btrfs_set_shared_data_ref_count(leaf, ref, refs_to_add);
		} else {
			num_refs = btrfs_shared_data_ref_count(leaf, ref);
			num_refs += refs_to_add;
			btrfs_set_shared_data_ref_count(leaf, ref, num_refs);
		}
	} else {
		struct btrfs_extent_data_ref *ref;
		while (ret == -EEXIST) {
			ref = btrfs_item_ptr(leaf, path->slots[0],
					     struct btrfs_extent_data_ref);
			if (match_extent_data_ref(leaf, ref, root_objectid,
						  owner, offset))
				break;
			btrfs_release_path(path);
			key.offset++;
			ret = btrfs_insert_empty_item(trans, root, path, &key,
						      size);
			if (ret && ret != -EEXIST)
				goto fail;

			leaf = path->nodes[0];
		}
		ref = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_data_ref);
		if (ret == 0) {
			btrfs_set_extent_data_ref_root(leaf, ref,
						       root_objectid);
			btrfs_set_extent_data_ref_objectid(leaf, ref, owner);
			btrfs_set_extent_data_ref_offset(leaf, ref, offset);
			btrfs_set_extent_data_ref_count(leaf, ref, refs_to_add);
		} else {
			num_refs = btrfs_extent_data_ref_count(leaf, ref);
			num_refs += refs_to_add;
			btrfs_set_extent_data_ref_count(leaf, ref, num_refs);
		}
	}
	btrfs_mark_buffer_dirty(trans, leaf);
	ret = 0;
fail:
	btrfs_release_path(path);
	return ret;
}

static noinline int remove_extent_data_ref(struct btrfs_trans_handle *trans,
					   struct btrfs_root *root,
					   struct btrfs_path *path,
					   int refs_to_drop)
{
	struct btrfs_key key;
	struct btrfs_extent_data_ref *ref1 = NULL;
	struct btrfs_shared_data_ref *ref2 = NULL;
	struct extent_buffer *leaf;
	u32 num_refs = 0;
	int ret = 0;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
		ref1 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		num_refs = btrfs_extent_data_ref_count(leaf, ref1);
	} else if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
		ref2 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_shared_data_ref);
		num_refs = btrfs_shared_data_ref_count(leaf, ref2);
	} else {
		btrfs_err(trans->fs_info,
			  "unrecognized backref key (%llu %u %llu)",
			  key.objectid, key.type, key.offset);
		btrfs_abort_transaction(trans, -EUCLEAN);
		return -EUCLEAN;
	}

	BUG_ON(num_refs < refs_to_drop);
	num_refs -= refs_to_drop;

	if (num_refs == 0) {
		ret = btrfs_del_item(trans, root, path);
	} else {
		if (key.type == BTRFS_EXTENT_DATA_REF_KEY)
			btrfs_set_extent_data_ref_count(leaf, ref1, num_refs);
		else if (key.type == BTRFS_SHARED_DATA_REF_KEY)
			btrfs_set_shared_data_ref_count(leaf, ref2, num_refs);
		btrfs_mark_buffer_dirty(trans, leaf);
	}
	return ret;
}

static noinline u32 extent_data_ref_count(struct btrfs_path *path,
					  struct btrfs_extent_inline_ref *iref)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_data_ref *ref1;
	struct btrfs_shared_data_ref *ref2;
	u32 num_refs = 0;
	int type;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (iref) {
		 
		type = btrfs_get_extent_inline_ref_type(leaf, iref, BTRFS_REF_TYPE_DATA);
		ASSERT(type != BTRFS_REF_TYPE_INVALID);
		if (type == BTRFS_EXTENT_DATA_REF_KEY) {
			ref1 = (struct btrfs_extent_data_ref *)(&iref->offset);
			num_refs = btrfs_extent_data_ref_count(leaf, ref1);
		} else {
			ref2 = (struct btrfs_shared_data_ref *)(iref + 1);
			num_refs = btrfs_shared_data_ref_count(leaf, ref2);
		}
	} else if (key.type == BTRFS_EXTENT_DATA_REF_KEY) {
		ref1 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_extent_data_ref);
		num_refs = btrfs_extent_data_ref_count(leaf, ref1);
	} else if (key.type == BTRFS_SHARED_DATA_REF_KEY) {
		ref2 = btrfs_item_ptr(leaf, path->slots[0],
				      struct btrfs_shared_data_ref);
		num_refs = btrfs_shared_data_ref_count(leaf, ref2);
	} else {
		WARN_ON(1);
	}
	return num_refs;
}

static noinline int lookup_tree_block_ref(struct btrfs_trans_handle *trans,
					  struct btrfs_path *path,
					  u64 bytenr, u64 parent,
					  u64 root_objectid)
{
	struct btrfs_root *root = btrfs_extent_root(trans->fs_info, bytenr);
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_TREE_BLOCK_REF_KEY;
		key.offset = root_objectid;
	}

	ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
	if (ret > 0)
		ret = -ENOENT;
	return ret;
}

static noinline int insert_tree_block_ref(struct btrfs_trans_handle *trans,
					  struct btrfs_path *path,
					  u64 bytenr, u64 parent,
					  u64 root_objectid)
{
	struct btrfs_root *root = btrfs_extent_root(trans->fs_info, bytenr);
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	if (parent) {
		key.type = BTRFS_SHARED_BLOCK_REF_KEY;
		key.offset = parent;
	} else {
		key.type = BTRFS_TREE_BLOCK_REF_KEY;
		key.offset = root_objectid;
	}

	ret = btrfs_insert_empty_item(trans, root, path, &key, 0);
	btrfs_release_path(path);
	return ret;
}

static inline int extent_ref_type(u64 parent, u64 owner)
{
	int type;
	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		if (parent > 0)
			type = BTRFS_SHARED_BLOCK_REF_KEY;
		else
			type = BTRFS_TREE_BLOCK_REF_KEY;
	} else {
		if (parent > 0)
			type = BTRFS_SHARED_DATA_REF_KEY;
		else
			type = BTRFS_EXTENT_DATA_REF_KEY;
	}
	return type;
}

static int find_next_key(struct btrfs_path *path, int level,
			 struct btrfs_key *key)

{
	for (; level < BTRFS_MAX_LEVEL; level++) {
		if (!path->nodes[level])
			break;
		if (path->slots[level] + 1 >=
		    btrfs_header_nritems(path->nodes[level]))
			continue;
		if (level == 0)
			btrfs_item_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
		else
			btrfs_node_key_to_cpu(path->nodes[level], key,
					      path->slots[level] + 1);
		return 0;
	}
	return 1;
}

 
static noinline_for_stack
int lookup_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref **ref_ret,
				 u64 bytenr, u64 num_bytes,
				 u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int insert)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *root = btrfs_extent_root(fs_info, bytenr);
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	u64 flags;
	u64 item_size;
	unsigned long ptr;
	unsigned long end;
	int extra_size;
	int type;
	int want;
	int ret;
	int err = 0;
	bool skinny_metadata = btrfs_fs_incompat(fs_info, SKINNY_METADATA);
	int needed;

	key.objectid = bytenr;
	key.type = BTRFS_EXTENT_ITEM_KEY;
	key.offset = num_bytes;

	want = extent_ref_type(parent, owner);
	if (insert) {
		extra_size = btrfs_extent_inline_ref_size(want);
		path->search_for_extension = 1;
		path->keep_locks = 1;
	} else
		extra_size = -1;

	 
	if (skinny_metadata && owner < BTRFS_FIRST_FREE_OBJECTID) {
		key.type = BTRFS_METADATA_ITEM_KEY;
		key.offset = owner;
	}

again:
	ret = btrfs_search_slot(trans, root, &key, path, extra_size, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}

	 
	if (ret > 0 && skinny_metadata) {
		skinny_metadata = false;
		if (path->slots[0]) {
			path->slots[0]--;
			btrfs_item_key_to_cpu(path->nodes[0], &key,
					      path->slots[0]);
			if (key.objectid == bytenr &&
			    key.type == BTRFS_EXTENT_ITEM_KEY &&
			    key.offset == num_bytes)
				ret = 0;
		}
		if (ret) {
			key.objectid = bytenr;
			key.type = BTRFS_EXTENT_ITEM_KEY;
			key.offset = num_bytes;
			btrfs_release_path(path);
			goto again;
		}
	}

	if (ret && !insert) {
		err = -ENOENT;
		goto out;
	} else if (WARN_ON(ret)) {
		btrfs_print_leaf(path->nodes[0]);
		btrfs_err(fs_info,
"extent item not found for insert, bytenr %llu num_bytes %llu parent %llu root_objectid %llu owner %llu offset %llu",
			  bytenr, num_bytes, parent, root_objectid, owner,
			  offset);
		err = -EIO;
		goto out;
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size(leaf, path->slots[0]);
	if (unlikely(item_size < sizeof(*ei))) {
		err = -EUCLEAN;
		btrfs_err(fs_info,
			  "unexpected extent item size, has %llu expect >= %zu",
			  item_size, sizeof(*ei));
		btrfs_abort_transaction(trans, err);
		goto out;
	}

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	flags = btrfs_extent_flags(leaf, ei);

	ptr = (unsigned long)(ei + 1);
	end = (unsigned long)ei + item_size;

	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK && !skinny_metadata) {
		ptr += sizeof(struct btrfs_tree_block_info);
		BUG_ON(ptr > end);
	}

	if (owner >= BTRFS_FIRST_FREE_OBJECTID)
		needed = BTRFS_REF_TYPE_DATA;
	else
		needed = BTRFS_REF_TYPE_BLOCK;

	err = -ENOENT;
	while (1) {
		if (ptr >= end) {
			if (ptr > end) {
				err = -EUCLEAN;
				btrfs_print_leaf(path->nodes[0]);
				btrfs_crit(fs_info,
"overrun extent record at slot %d while looking for inline extent for root %llu owner %llu offset %llu parent %llu",
					path->slots[0], root_objectid, owner, offset, parent);
			}
			break;
		}
		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_get_extent_inline_ref_type(leaf, iref, needed);
		if (type == BTRFS_REF_TYPE_INVALID) {
			err = -EUCLEAN;
			goto out;
		}

		if (want < type)
			break;
		if (want > type) {
			ptr += btrfs_extent_inline_ref_size(type);
			continue;
		}

		if (type == BTRFS_EXTENT_DATA_REF_KEY) {
			struct btrfs_extent_data_ref *dref;
			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			if (match_extent_data_ref(leaf, dref, root_objectid,
						  owner, offset)) {
				err = 0;
				break;
			}
			if (hash_extent_data_ref_item(leaf, dref) <
			    hash_extent_data_ref(root_objectid, owner, offset))
				break;
		} else {
			u64 ref_offset;
			ref_offset = btrfs_extent_inline_ref_offset(leaf, iref);
			if (parent > 0) {
				if (parent == ref_offset) {
					err = 0;
					break;
				}
				if (ref_offset < parent)
					break;
			} else {
				if (root_objectid == ref_offset) {
					err = 0;
					break;
				}
				if (ref_offset < root_objectid)
					break;
			}
		}
		ptr += btrfs_extent_inline_ref_size(type);
	}
	if (err == -ENOENT && insert) {
		if (item_size + extra_size >=
		    BTRFS_MAX_EXTENT_ITEM_SIZE(root)) {
			err = -EAGAIN;
			goto out;
		}
		 
		if (find_next_key(path, 0, &key) == 0 &&
		    key.objectid == bytenr &&
		    key.type < BTRFS_BLOCK_GROUP_ITEM_KEY) {
			err = -EAGAIN;
			goto out;
		}
	}
	*ref_ret = (struct btrfs_extent_inline_ref *)ptr;
out:
	if (insert) {
		path->keep_locks = 0;
		path->search_for_extension = 0;
		btrfs_unlock_up_safe(path, 1);
	}
	return err;
}

 
static noinline_for_stack
void setup_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref *iref,
				 u64 parent, u64 root_objectid,
				 u64 owner, u64 offset, int refs_to_add,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	unsigned long ptr;
	unsigned long end;
	unsigned long item_offset;
	u64 refs;
	int size;
	int type;

	leaf = path->nodes[0];
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	item_offset = (unsigned long)iref - (unsigned long)ei;

	type = extent_ref_type(parent, owner);
	size = btrfs_extent_inline_ref_size(type);

	btrfs_extend_item(trans, path, size);

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, ei);
	refs += refs_to_add;
	btrfs_set_extent_refs(leaf, ei, refs);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, ei);

	ptr = (unsigned long)ei + item_offset;
	end = (unsigned long)ei + btrfs_item_size(leaf, path->slots[0]);
	if (ptr < end - size)
		memmove_extent_buffer(leaf, ptr + size, ptr,
				      end - size - ptr);

	iref = (struct btrfs_extent_inline_ref *)ptr;
	btrfs_set_extent_inline_ref_type(leaf, iref, type);
	if (type == BTRFS_EXTENT_DATA_REF_KEY) {
		struct btrfs_extent_data_ref *dref;
		dref = (struct btrfs_extent_data_ref *)(&iref->offset);
		btrfs_set_extent_data_ref_root(leaf, dref, root_objectid);
		btrfs_set_extent_data_ref_objectid(leaf, dref, owner);
		btrfs_set_extent_data_ref_offset(leaf, dref, offset);
		btrfs_set_extent_data_ref_count(leaf, dref, refs_to_add);
	} else if (type == BTRFS_SHARED_DATA_REF_KEY) {
		struct btrfs_shared_data_ref *sref;
		sref = (struct btrfs_shared_data_ref *)(iref + 1);
		btrfs_set_shared_data_ref_count(leaf, sref, refs_to_add);
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else if (type == BTRFS_SHARED_BLOCK_REF_KEY) {
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
	} else {
		btrfs_set_extent_inline_ref_offset(leaf, iref, root_objectid);
	}
	btrfs_mark_buffer_dirty(trans, leaf);
}

static int lookup_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref **ref_ret,
				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64 root_objectid, u64 owner, u64 offset)
{
	int ret;

	ret = lookup_inline_extent_backref(trans, path, ref_ret, bytenr,
					   num_bytes, parent, root_objectid,
					   owner, offset, 0);
	if (ret != -ENOENT)
		return ret;

	btrfs_release_path(path);
	*ref_ret = NULL;

	if (owner < BTRFS_FIRST_FREE_OBJECTID) {
		ret = lookup_tree_block_ref(trans, path, bytenr, parent,
					    root_objectid);
	} else {
		ret = lookup_extent_data_ref(trans, path, bytenr, parent,
					     root_objectid, owner, offset);
	}
	return ret;
}

 
static noinline_for_stack int update_inline_extent_backref(
				  struct btrfs_trans_handle *trans,
				  struct btrfs_path *path,
				  struct btrfs_extent_inline_ref *iref,
				  int refs_to_mod,
				  struct btrfs_delayed_extent_op *extent_op)
{
	struct extent_buffer *leaf = path->nodes[0];
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_data_ref *dref = NULL;
	struct btrfs_shared_data_ref *sref = NULL;
	unsigned long ptr;
	unsigned long end;
	u32 item_size;
	int size;
	int type;
	u64 refs;

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, ei);
	if (unlikely(refs_to_mod < 0 && refs + refs_to_mod <= 0)) {
		struct btrfs_key key;
		u32 extent_size;

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.type == BTRFS_METADATA_ITEM_KEY)
			extent_size = fs_info->nodesize;
		else
			extent_size = key.offset;
		btrfs_print_leaf(leaf);
		btrfs_err(fs_info,
	"invalid refs_to_mod for extent %llu num_bytes %u, has %d expect >= -%llu",
			  key.objectid, extent_size, refs_to_mod, refs);
		return -EUCLEAN;
	}
	refs += refs_to_mod;
	btrfs_set_extent_refs(leaf, ei, refs);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, ei);

	type = btrfs_get_extent_inline_ref_type(leaf, iref, BTRFS_REF_TYPE_ANY);
	 
	if (unlikely(type == BTRFS_REF_TYPE_INVALID))
		return -EUCLEAN;

	if (type == BTRFS_EXTENT_DATA_REF_KEY) {
		dref = (struct btrfs_extent_data_ref *)(&iref->offset);
		refs = btrfs_extent_data_ref_count(leaf, dref);
	} else if (type == BTRFS_SHARED_DATA_REF_KEY) {
		sref = (struct btrfs_shared_data_ref *)(iref + 1);
		refs = btrfs_shared_data_ref_count(leaf, sref);
	} else {
		refs = 1;
		 
		if (unlikely(refs_to_mod != -1)) {
			struct btrfs_key key;

			btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

			btrfs_print_leaf(leaf);
			btrfs_err(fs_info,
			"invalid refs_to_mod for tree block %llu, has %d expect -1",
				  key.objectid, refs_to_mod);
			return -EUCLEAN;
		}
	}

	if (unlikely(refs_to_mod < 0 && refs < -refs_to_mod)) {
		struct btrfs_key key;
		u32 extent_size;

		btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
		if (key.type == BTRFS_METADATA_ITEM_KEY)
			extent_size = fs_info->nodesize;
		else
			extent_size = key.offset;
		btrfs_print_leaf(leaf);
		btrfs_err(fs_info,
"invalid refs_to_mod for backref entry, iref %lu extent %llu num_bytes %u, has %d expect >= -%llu",
			  (unsigned long)iref, key.objectid, extent_size,
			  refs_to_mod, refs);
		return -EUCLEAN;
	}
	refs += refs_to_mod;

	if (refs > 0) {
		if (type == BTRFS_EXTENT_DATA_REF_KEY)
			btrfs_set_extent_data_ref_count(leaf, dref, refs);
		else
			btrfs_set_shared_data_ref_count(leaf, sref, refs);
	} else {
		size =  btrfs_extent_inline_ref_size(type);
		item_size = btrfs_item_size(leaf, path->slots[0]);
		ptr = (unsigned long)iref;
		end = (unsigned long)ei + item_size;
		if (ptr + size < end)
			memmove_extent_buffer(leaf, ptr, ptr + size,
					      end - ptr - size);
		item_size -= size;
		btrfs_truncate_item(trans, path, item_size, 1);
	}
	btrfs_mark_buffer_dirty(trans, leaf);
	return 0;
}

static noinline_for_stack
int insert_inline_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_path *path,
				 u64 bytenr, u64 num_bytes, u64 parent,
				 u64 root_objectid, u64 owner,
				 u64 offset, int refs_to_add,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_extent_inline_ref *iref;
	int ret;

	ret = lookup_inline_extent_backref(trans, path, &iref, bytenr,
					   num_bytes, parent, root_objectid,
					   owner, offset, 1);
	if (ret == 0) {
		 
		if (owner < BTRFS_FIRST_FREE_OBJECTID) {
			btrfs_print_leaf(path->nodes[0]);
			btrfs_crit(trans->fs_info,
"adding refs to an existing tree ref, bytenr %llu num_bytes %llu root_objectid %llu slot %u",
				   bytenr, num_bytes, root_objectid, path->slots[0]);
			return -EUCLEAN;
		}
		ret = update_inline_extent_backref(trans, path, iref,
						   refs_to_add, extent_op);
	} else if (ret == -ENOENT) {
		setup_inline_extent_backref(trans, path, iref, parent,
					    root_objectid, owner, offset,
					    refs_to_add, extent_op);
		ret = 0;
	}
	return ret;
}

static int remove_extent_backref(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct btrfs_extent_inline_ref *iref,
				 int refs_to_drop, int is_data)
{
	int ret = 0;

	BUG_ON(!is_data && refs_to_drop != 1);
	if (iref)
		ret = update_inline_extent_backref(trans, path, iref,
						   -refs_to_drop, NULL);
	else if (is_data)
		ret = remove_extent_data_ref(trans, root, path, refs_to_drop);
	else
		ret = btrfs_del_item(trans, root, path);
	return ret;
}

static int btrfs_issue_discard(struct block_device *bdev, u64 start, u64 len,
			       u64 *discarded_bytes)
{
	int j, ret = 0;
	u64 bytes_left, end;
	u64 aligned_start = ALIGN(start, 1 << SECTOR_SHIFT);

	if (WARN_ON(start != aligned_start)) {
		len -= aligned_start - start;
		len = round_down(len, 1 << SECTOR_SHIFT);
		start = aligned_start;
	}

	*discarded_bytes = 0;

	if (!len)
		return 0;

	end = start + len;
	bytes_left = len;

	 
	for (j = 0; j < BTRFS_SUPER_MIRROR_MAX; j++) {
		u64 sb_start = btrfs_sb_offset(j);
		u64 sb_end = sb_start + BTRFS_SUPER_INFO_SIZE;
		u64 size = sb_start - start;

		if (!in_range(sb_start, start, bytes_left) &&
		    !in_range(sb_end, start, bytes_left) &&
		    !in_range(start, sb_start, BTRFS_SUPER_INFO_SIZE))
			continue;

		 
		if (sb_start <= start) {
			start += sb_end - start;
			if (start > end) {
				bytes_left = 0;
				break;
			}
			bytes_left = end - start;
			continue;
		}

		if (size) {
			ret = blkdev_issue_discard(bdev, start >> SECTOR_SHIFT,
						   size >> SECTOR_SHIFT,
						   GFP_NOFS);
			if (!ret)
				*discarded_bytes += size;
			else if (ret != -EOPNOTSUPP)
				return ret;
		}

		start = sb_end;
		if (start > end) {
			bytes_left = 0;
			break;
		}
		bytes_left = end - start;
	}

	if (bytes_left) {
		ret = blkdev_issue_discard(bdev, start >> SECTOR_SHIFT,
					   bytes_left >> SECTOR_SHIFT,
					   GFP_NOFS);
		if (!ret)
			*discarded_bytes += bytes_left;
	}
	return ret;
}

static int do_discard_extent(struct btrfs_discard_stripe *stripe, u64 *bytes)
{
	struct btrfs_device *dev = stripe->dev;
	struct btrfs_fs_info *fs_info = dev->fs_info;
	struct btrfs_dev_replace *dev_replace = &fs_info->dev_replace;
	u64 phys = stripe->physical;
	u64 len = stripe->length;
	u64 discarded = 0;
	int ret = 0;

	 
	if (btrfs_can_zone_reset(dev, phys, len)) {
		u64 src_disc;

		ret = btrfs_reset_device_zone(dev, phys, len, &discarded);
		if (ret)
			goto out;

		if (!btrfs_dev_replace_is_ongoing(dev_replace) ||
		    dev != dev_replace->srcdev)
			goto out;

		src_disc = discarded;

		 
		ret = btrfs_reset_device_zone(dev_replace->tgtdev, phys, len,
					      &discarded);
		discarded += src_disc;
	} else if (bdev_max_discard_sectors(stripe->dev->bdev)) {
		ret = btrfs_issue_discard(dev->bdev, phys, len, &discarded);
	} else {
		ret = 0;
		*bytes = 0;
	}

out:
	*bytes = discarded;
	return ret;
}

int btrfs_discard_extent(struct btrfs_fs_info *fs_info, u64 bytenr,
			 u64 num_bytes, u64 *actual_bytes)
{
	int ret = 0;
	u64 discarded_bytes = 0;
	u64 end = bytenr + num_bytes;
	u64 cur = bytenr;

	 
	btrfs_bio_counter_inc_blocked(fs_info);
	while (cur < end) {
		struct btrfs_discard_stripe *stripes;
		unsigned int num_stripes;
		int i;

		num_bytes = end - cur;
		stripes = btrfs_map_discard(fs_info, cur, &num_bytes, &num_stripes);
		if (IS_ERR(stripes)) {
			ret = PTR_ERR(stripes);
			if (ret == -EOPNOTSUPP)
				ret = 0;
			break;
		}

		for (i = 0; i < num_stripes; i++) {
			struct btrfs_discard_stripe *stripe = stripes + i;
			u64 bytes;

			if (!stripe->dev->bdev) {
				ASSERT(btrfs_test_opt(fs_info, DEGRADED));
				continue;
			}

			if (!test_bit(BTRFS_DEV_STATE_WRITEABLE,
					&stripe->dev->dev_state))
				continue;

			ret = do_discard_extent(stripe, &bytes);
			if (ret) {
				 
				if (ret != -EOPNOTSUPP)
					break;
				ret = 0;
			} else {
				discarded_bytes += bytes;
			}
		}
		kfree(stripes);
		if (ret)
			break;
		cur += num_bytes;
	}
	btrfs_bio_counter_dec(fs_info);
	if (actual_bytes)
		*actual_bytes = discarded_bytes;
	return ret;
}

 
int btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
			 struct btrfs_ref *generic_ref)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int ret;

	ASSERT(generic_ref->type != BTRFS_REF_NOT_SET &&
	       generic_ref->action);
	BUG_ON(generic_ref->type == BTRFS_REF_METADATA &&
	       generic_ref->tree_ref.owning_root == BTRFS_TREE_LOG_OBJECTID);

	if (generic_ref->type == BTRFS_REF_METADATA)
		ret = btrfs_add_delayed_tree_ref(trans, generic_ref, NULL);
	else
		ret = btrfs_add_delayed_data_ref(trans, generic_ref, 0);

	btrfs_ref_tree_mod(fs_info, generic_ref);

	return ret;
}

 
static int __btrfs_inc_extent_ref(struct btrfs_trans_handle *trans,
				  struct btrfs_delayed_ref_node *node,
				  u64 parent, u64 root_objectid,
				  u64 owner, u64 offset, int refs_to_add,
				  struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *item;
	struct btrfs_key key;
	u64 bytenr = node->bytenr;
	u64 num_bytes = node->num_bytes;
	u64 refs;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	 
	ret = insert_inline_extent_backref(trans, path, bytenr, num_bytes,
					   parent, root_objectid, owner,
					   offset, refs_to_add, extent_op);
	if ((ret < 0 && ret != -EAGAIN) || !ret)
		goto out;

	 
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	item = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	refs = btrfs_extent_refs(leaf, item);
	btrfs_set_extent_refs(leaf, item, refs + refs_to_add);
	if (extent_op)
		__run_delayed_extent_op(extent_op, leaf, item);

	btrfs_mark_buffer_dirty(trans, leaf);
	btrfs_release_path(path);

	 
	if (owner < BTRFS_FIRST_FREE_OBJECTID)
		ret = insert_tree_block_ref(trans, path, bytenr, parent,
					    root_objectid);
	else
		ret = insert_extent_data_ref(trans, path, bytenr, parent,
					     root_objectid, owner, offset,
					     refs_to_add);

	if (ret)
		btrfs_abort_transaction(trans, ret);
out:
	btrfs_free_path(path);
	return ret;
}

static int run_delayed_data_ref(struct btrfs_trans_handle *trans,
				struct btrfs_delayed_ref_node *node,
				struct btrfs_delayed_extent_op *extent_op,
				bool insert_reserved)
{
	int ret = 0;
	struct btrfs_delayed_data_ref *ref;
	struct btrfs_key ins;
	u64 parent = 0;
	u64 ref_root = 0;
	u64 flags = 0;

	ins.objectid = node->bytenr;
	ins.offset = node->num_bytes;
	ins.type = BTRFS_EXTENT_ITEM_KEY;

	ref = btrfs_delayed_node_to_data_ref(node);
	trace_run_delayed_data_ref(trans->fs_info, node, ref, node->action);

	if (node->type == BTRFS_SHARED_DATA_REF_KEY)
		parent = ref->parent;
	ref_root = ref->root;

	if (node->action == BTRFS_ADD_DELAYED_REF && insert_reserved) {
		if (extent_op)
			flags |= extent_op->flags_to_set;
		ret = alloc_reserved_file_extent(trans, parent, ref_root,
						 flags, ref->objectid,
						 ref->offset, &ins,
						 node->ref_mod);
	} else if (node->action == BTRFS_ADD_DELAYED_REF) {
		ret = __btrfs_inc_extent_ref(trans, node, parent, ref_root,
					     ref->objectid, ref->offset,
					     node->ref_mod, extent_op);
	} else if (node->action == BTRFS_DROP_DELAYED_REF) {
		ret = __btrfs_free_extent(trans, node, parent,
					  ref_root, ref->objectid,
					  ref->offset, node->ref_mod,
					  extent_op);
	} else {
		BUG();
	}
	return ret;
}

static void __run_delayed_extent_op(struct btrfs_delayed_extent_op *extent_op,
				    struct extent_buffer *leaf,
				    struct btrfs_extent_item *ei)
{
	u64 flags = btrfs_extent_flags(leaf, ei);
	if (extent_op->update_flags) {
		flags |= extent_op->flags_to_set;
		btrfs_set_extent_flags(leaf, ei, flags);
	}

	if (extent_op->update_key) {
		struct btrfs_tree_block_info *bi;
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_TREE_BLOCK));
		bi = (struct btrfs_tree_block_info *)(ei + 1);
		btrfs_set_tree_block_key(leaf, bi, &extent_op->key);
	}
}

static int run_delayed_extent_op(struct btrfs_trans_handle *trans,
				 struct btrfs_delayed_ref_head *head,
				 struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *root;
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_extent_item *ei;
	struct extent_buffer *leaf;
	u32 item_size;
	int ret;
	int err = 0;
	int metadata = 1;

	if (TRANS_ABORTED(trans))
		return 0;

	if (!btrfs_fs_incompat(fs_info, SKINNY_METADATA))
		metadata = 0;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	key.objectid = head->bytenr;

	if (metadata) {
		key.type = BTRFS_METADATA_ITEM_KEY;
		key.offset = extent_op->level;
	} else {
		key.type = BTRFS_EXTENT_ITEM_KEY;
		key.offset = head->num_bytes;
	}

	root = btrfs_extent_root(fs_info, key.objectid);
again:
	ret = btrfs_search_slot(trans, root, &key, path, 0, 1);
	if (ret < 0) {
		err = ret;
		goto out;
	}
	if (ret > 0) {
		if (metadata) {
			if (path->slots[0] > 0) {
				path->slots[0]--;
				btrfs_item_key_to_cpu(path->nodes[0], &key,
						      path->slots[0]);
				if (key.objectid == head->bytenr &&
				    key.type == BTRFS_EXTENT_ITEM_KEY &&
				    key.offset == head->num_bytes)
					ret = 0;
			}
			if (ret > 0) {
				btrfs_release_path(path);
				metadata = 0;

				key.objectid = head->bytenr;
				key.offset = head->num_bytes;
				key.type = BTRFS_EXTENT_ITEM_KEY;
				goto again;
			}
		} else {
			err = -EUCLEAN;
			btrfs_err(fs_info,
		  "missing extent item for extent %llu num_bytes %llu level %d",
				  head->bytenr, head->num_bytes, extent_op->level);
			goto out;
		}
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size(leaf, path->slots[0]);

	if (unlikely(item_size < sizeof(*ei))) {
		err = -EUCLEAN;
		btrfs_err(fs_info,
			  "unexpected extent item size, has %u expect >= %zu",
			  item_size, sizeof(*ei));
		btrfs_abort_transaction(trans, err);
		goto out;
	}

	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);
	__run_delayed_extent_op(extent_op, leaf, ei);

	btrfs_mark_buffer_dirty(trans, leaf);
out:
	btrfs_free_path(path);
	return err;
}

static int run_delayed_tree_ref(struct btrfs_trans_handle *trans,
				struct btrfs_delayed_ref_node *node,
				struct btrfs_delayed_extent_op *extent_op,
				bool insert_reserved)
{
	int ret = 0;
	struct btrfs_delayed_tree_ref *ref;
	u64 parent = 0;
	u64 ref_root = 0;

	ref = btrfs_delayed_node_to_tree_ref(node);
	trace_run_delayed_tree_ref(trans->fs_info, node, ref, node->action);

	if (node->type == BTRFS_SHARED_BLOCK_REF_KEY)
		parent = ref->parent;
	ref_root = ref->root;

	if (unlikely(node->ref_mod != 1)) {
		btrfs_err(trans->fs_info,
	"btree block %llu has %d references rather than 1: action %d ref_root %llu parent %llu",
			  node->bytenr, node->ref_mod, node->action, ref_root,
			  parent);
		return -EUCLEAN;
	}
	if (node->action == BTRFS_ADD_DELAYED_REF && insert_reserved) {
		BUG_ON(!extent_op || !extent_op->update_flags);
		ret = alloc_reserved_tree_block(trans, node, extent_op);
	} else if (node->action == BTRFS_ADD_DELAYED_REF) {
		ret = __btrfs_inc_extent_ref(trans, node, parent, ref_root,
					     ref->level, 0, 1, extent_op);
	} else if (node->action == BTRFS_DROP_DELAYED_REF) {
		ret = __btrfs_free_extent(trans, node, parent, ref_root,
					  ref->level, 0, 1, extent_op);
	} else {
		BUG();
	}
	return ret;
}

 
static int run_one_delayed_ref(struct btrfs_trans_handle *trans,
			       struct btrfs_delayed_ref_node *node,
			       struct btrfs_delayed_extent_op *extent_op,
			       bool insert_reserved)
{
	int ret = 0;

	if (TRANS_ABORTED(trans)) {
		if (insert_reserved)
			btrfs_pin_extent(trans, node->bytenr, node->num_bytes, 1);
		return 0;
	}

	if (node->type == BTRFS_TREE_BLOCK_REF_KEY ||
	    node->type == BTRFS_SHARED_BLOCK_REF_KEY)
		ret = run_delayed_tree_ref(trans, node, extent_op,
					   insert_reserved);
	else if (node->type == BTRFS_EXTENT_DATA_REF_KEY ||
		 node->type == BTRFS_SHARED_DATA_REF_KEY)
		ret = run_delayed_data_ref(trans, node, extent_op,
					   insert_reserved);
	else
		BUG();
	if (ret && insert_reserved)
		btrfs_pin_extent(trans, node->bytenr, node->num_bytes, 1);
	if (ret < 0)
		btrfs_err(trans->fs_info,
"failed to run delayed ref for logical %llu num_bytes %llu type %u action %u ref_mod %d: %d",
			  node->bytenr, node->num_bytes, node->type,
			  node->action, node->ref_mod, ret);
	return ret;
}

static inline struct btrfs_delayed_ref_node *
select_delayed_ref(struct btrfs_delayed_ref_head *head)
{
	struct btrfs_delayed_ref_node *ref;

	if (RB_EMPTY_ROOT(&head->ref_tree.rb_root))
		return NULL;

	 
	if (!list_empty(&head->ref_add_list))
		return list_first_entry(&head->ref_add_list,
				struct btrfs_delayed_ref_node, add_list);

	ref = rb_entry(rb_first_cached(&head->ref_tree),
		       struct btrfs_delayed_ref_node, ref_node);
	ASSERT(list_empty(&ref->add_list));
	return ref;
}

static void unselect_delayed_ref_head(struct btrfs_delayed_ref_root *delayed_refs,
				      struct btrfs_delayed_ref_head *head)
{
	spin_lock(&delayed_refs->lock);
	head->processing = false;
	delayed_refs->num_heads_ready++;
	spin_unlock(&delayed_refs->lock);
	btrfs_delayed_ref_unlock(head);
}

static struct btrfs_delayed_extent_op *cleanup_extent_op(
				struct btrfs_delayed_ref_head *head)
{
	struct btrfs_delayed_extent_op *extent_op = head->extent_op;

	if (!extent_op)
		return NULL;

	if (head->must_insert_reserved) {
		head->extent_op = NULL;
		btrfs_free_delayed_extent_op(extent_op);
		return NULL;
	}
	return extent_op;
}

static int run_and_cleanup_extent_op(struct btrfs_trans_handle *trans,
				     struct btrfs_delayed_ref_head *head)
{
	struct btrfs_delayed_extent_op *extent_op;
	int ret;

	extent_op = cleanup_extent_op(head);
	if (!extent_op)
		return 0;
	head->extent_op = NULL;
	spin_unlock(&head->lock);
	ret = run_delayed_extent_op(trans, head, extent_op);
	btrfs_free_delayed_extent_op(extent_op);
	return ret ? ret : 1;
}

void btrfs_cleanup_ref_head_accounting(struct btrfs_fs_info *fs_info,
				  struct btrfs_delayed_ref_root *delayed_refs,
				  struct btrfs_delayed_ref_head *head)
{
	int nr_items = 1;	 

	 
	if (head->total_ref_mod < 0 && head->is_data) {
		spin_lock(&delayed_refs->lock);
		delayed_refs->pending_csums -= head->num_bytes;
		spin_unlock(&delayed_refs->lock);
		nr_items += btrfs_csum_bytes_to_leaves(fs_info, head->num_bytes);
	}

	btrfs_delayed_refs_rsv_release(fs_info, nr_items);
}

static int cleanup_ref_head(struct btrfs_trans_handle *trans,
			    struct btrfs_delayed_ref_head *head)
{

	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_delayed_ref_root *delayed_refs;
	int ret;

	delayed_refs = &trans->transaction->delayed_refs;

	ret = run_and_cleanup_extent_op(trans, head);
	if (ret < 0) {
		unselect_delayed_ref_head(delayed_refs, head);
		btrfs_debug(fs_info, "run_delayed_extent_op returned %d", ret);
		return ret;
	} else if (ret) {
		return ret;
	}

	 
	spin_unlock(&head->lock);
	spin_lock(&delayed_refs->lock);
	spin_lock(&head->lock);
	if (!RB_EMPTY_ROOT(&head->ref_tree.rb_root) || head->extent_op) {
		spin_unlock(&head->lock);
		spin_unlock(&delayed_refs->lock);
		return 1;
	}
	btrfs_delete_ref_head(delayed_refs, head);
	spin_unlock(&head->lock);
	spin_unlock(&delayed_refs->lock);

	if (head->must_insert_reserved) {
		btrfs_pin_extent(trans, head->bytenr, head->num_bytes, 1);
		if (head->is_data) {
			struct btrfs_root *csum_root;

			csum_root = btrfs_csum_root(fs_info, head->bytenr);
			ret = btrfs_del_csums(trans, csum_root, head->bytenr,
					      head->num_bytes);
		}
	}

	btrfs_cleanup_ref_head_accounting(fs_info, delayed_refs, head);

	trace_run_delayed_ref_head(fs_info, head, 0);
	btrfs_delayed_ref_unlock(head);
	btrfs_put_delayed_ref_head(head);
	return ret;
}

static struct btrfs_delayed_ref_head *btrfs_obtain_ref_head(
					struct btrfs_trans_handle *trans)
{
	struct btrfs_delayed_ref_root *delayed_refs =
		&trans->transaction->delayed_refs;
	struct btrfs_delayed_ref_head *head = NULL;
	int ret;

	spin_lock(&delayed_refs->lock);
	head = btrfs_select_ref_head(delayed_refs);
	if (!head) {
		spin_unlock(&delayed_refs->lock);
		return head;
	}

	 
	ret = btrfs_delayed_ref_lock(delayed_refs, head);
	spin_unlock(&delayed_refs->lock);

	 
	if (ret == -EAGAIN)
		head = ERR_PTR(-EAGAIN);

	return head;
}

static int btrfs_run_delayed_refs_for_head(struct btrfs_trans_handle *trans,
					   struct btrfs_delayed_ref_head *locked_ref)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_extent_op *extent_op;
	struct btrfs_delayed_ref_node *ref;
	bool must_insert_reserved;
	int ret;

	delayed_refs = &trans->transaction->delayed_refs;

	lockdep_assert_held(&locked_ref->mutex);
	lockdep_assert_held(&locked_ref->lock);

	while ((ref = select_delayed_ref(locked_ref))) {
		if (ref->seq &&
		    btrfs_check_delayed_seq(fs_info, ref->seq)) {
			spin_unlock(&locked_ref->lock);
			unselect_delayed_ref_head(delayed_refs, locked_ref);
			return -EAGAIN;
		}

		rb_erase_cached(&ref->ref_node, &locked_ref->ref_tree);
		RB_CLEAR_NODE(&ref->ref_node);
		if (!list_empty(&ref->add_list))
			list_del(&ref->add_list);
		 
		switch (ref->action) {
		case BTRFS_ADD_DELAYED_REF:
		case BTRFS_ADD_DELAYED_EXTENT:
			locked_ref->ref_mod -= ref->ref_mod;
			break;
		case BTRFS_DROP_DELAYED_REF:
			locked_ref->ref_mod += ref->ref_mod;
			break;
		default:
			WARN_ON(1);
		}
		atomic_dec(&delayed_refs->num_entries);

		 
		must_insert_reserved = locked_ref->must_insert_reserved;
		locked_ref->must_insert_reserved = false;

		extent_op = locked_ref->extent_op;
		locked_ref->extent_op = NULL;
		spin_unlock(&locked_ref->lock);

		ret = run_one_delayed_ref(trans, ref, extent_op,
					  must_insert_reserved);

		btrfs_free_delayed_extent_op(extent_op);
		if (ret) {
			unselect_delayed_ref_head(delayed_refs, locked_ref);
			btrfs_put_delayed_ref(ref);
			return ret;
		}

		btrfs_put_delayed_ref(ref);
		cond_resched();

		spin_lock(&locked_ref->lock);
		btrfs_merge_delayed_refs(fs_info, delayed_refs, locked_ref);
	}

	return 0;
}

 
static noinline int __btrfs_run_delayed_refs(struct btrfs_trans_handle *trans,
					     unsigned long nr)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_head *locked_ref = NULL;
	int ret;
	unsigned long count = 0;

	delayed_refs = &trans->transaction->delayed_refs;
	do {
		if (!locked_ref) {
			locked_ref = btrfs_obtain_ref_head(trans);
			if (IS_ERR_OR_NULL(locked_ref)) {
				if (PTR_ERR(locked_ref) == -EAGAIN) {
					continue;
				} else {
					break;
				}
			}
			count++;
		}
		 
		spin_lock(&locked_ref->lock);
		btrfs_merge_delayed_refs(fs_info, delayed_refs, locked_ref);

		ret = btrfs_run_delayed_refs_for_head(trans, locked_ref);
		if (ret < 0 && ret != -EAGAIN) {
			 
			return ret;
		} else if (!ret) {
			 
			ret = cleanup_ref_head(trans, locked_ref);
			if (ret > 0 ) {
				 
				ret = 0;
				continue;
			} else if (ret) {
				return ret;
			}
		}

		 

		locked_ref = NULL;
		cond_resched();
	} while ((nr != -1 && count < nr) || locked_ref);

	return 0;
}

#ifdef SCRAMBLE_DELAYED_REFS
 
static u64 find_middle(struct rb_root *root)
{
	struct rb_node *n = root->rb_node;
	struct btrfs_delayed_ref_node *entry;
	int alt = 1;
	u64 middle;
	u64 first = 0, last = 0;

	n = rb_first(root);
	if (n) {
		entry = rb_entry(n, struct btrfs_delayed_ref_node, rb_node);
		first = entry->bytenr;
	}
	n = rb_last(root);
	if (n) {
		entry = rb_entry(n, struct btrfs_delayed_ref_node, rb_node);
		last = entry->bytenr;
	}
	n = root->rb_node;

	while (n) {
		entry = rb_entry(n, struct btrfs_delayed_ref_node, rb_node);
		WARN_ON(!entry->in_tree);

		middle = entry->bytenr;

		if (alt)
			n = n->rb_left;
		else
			n = n->rb_right;

		alt = 1 - alt;
	}
	return middle;
}
#endif

 
int btrfs_run_delayed_refs(struct btrfs_trans_handle *trans,
			   unsigned long count)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct rb_node *node;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_delayed_ref_head *head;
	int ret;
	int run_all = count == (unsigned long)-1;

	 
	if (TRANS_ABORTED(trans))
		return 0;

	if (test_bit(BTRFS_FS_CREATING_FREE_SPACE_TREE, &fs_info->flags))
		return 0;

	delayed_refs = &trans->transaction->delayed_refs;
	if (count == 0)
		count = delayed_refs->num_heads_ready;

again:
#ifdef SCRAMBLE_DELAYED_REFS
	delayed_refs->run_delayed_start = find_middle(&delayed_refs->root);
#endif
	ret = __btrfs_run_delayed_refs(trans, count);
	if (ret < 0) {
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	if (run_all) {
		btrfs_create_pending_block_groups(trans);

		spin_lock(&delayed_refs->lock);
		node = rb_first_cached(&delayed_refs->href_root);
		if (!node) {
			spin_unlock(&delayed_refs->lock);
			goto out;
		}
		head = rb_entry(node, struct btrfs_delayed_ref_head,
				href_node);
		refcount_inc(&head->refs);
		spin_unlock(&delayed_refs->lock);

		 
		mutex_lock(&head->mutex);
		mutex_unlock(&head->mutex);

		btrfs_put_delayed_ref_head(head);
		cond_resched();
		goto again;
	}
out:
	return 0;
}

int btrfs_set_disk_extent_flags(struct btrfs_trans_handle *trans,
				struct extent_buffer *eb, u64 flags)
{
	struct btrfs_delayed_extent_op *extent_op;
	int level = btrfs_header_level(eb);
	int ret;

	extent_op = btrfs_alloc_delayed_extent_op();
	if (!extent_op)
		return -ENOMEM;

	extent_op->flags_to_set = flags;
	extent_op->update_flags = true;
	extent_op->update_key = false;
	extent_op->level = level;

	ret = btrfs_add_delayed_extent_op(trans, eb->start, eb->len, extent_op);
	if (ret)
		btrfs_free_delayed_extent_op(extent_op);
	return ret;
}

static noinline int check_delayed_ref(struct btrfs_root *root,
				      struct btrfs_path *path,
				      u64 objectid, u64 offset, u64 bytenr)
{
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_node *ref;
	struct btrfs_delayed_data_ref *data_ref;
	struct btrfs_delayed_ref_root *delayed_refs;
	struct btrfs_transaction *cur_trans;
	struct rb_node *node;
	int ret = 0;

	spin_lock(&root->fs_info->trans_lock);
	cur_trans = root->fs_info->running_transaction;
	if (cur_trans)
		refcount_inc(&cur_trans->use_count);
	spin_unlock(&root->fs_info->trans_lock);
	if (!cur_trans)
		return 0;

	delayed_refs = &cur_trans->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(delayed_refs, bytenr);
	if (!head) {
		spin_unlock(&delayed_refs->lock);
		btrfs_put_transaction(cur_trans);
		return 0;
	}

	if (!mutex_trylock(&head->mutex)) {
		if (path->nowait) {
			spin_unlock(&delayed_refs->lock);
			btrfs_put_transaction(cur_trans);
			return -EAGAIN;
		}

		refcount_inc(&head->refs);
		spin_unlock(&delayed_refs->lock);

		btrfs_release_path(path);

		 
		mutex_lock(&head->mutex);
		mutex_unlock(&head->mutex);
		btrfs_put_delayed_ref_head(head);
		btrfs_put_transaction(cur_trans);
		return -EAGAIN;
	}
	spin_unlock(&delayed_refs->lock);

	spin_lock(&head->lock);
	 
	for (node = rb_first_cached(&head->ref_tree); node;
	     node = rb_next(node)) {
		ref = rb_entry(node, struct btrfs_delayed_ref_node, ref_node);
		 
		if (ref->type != BTRFS_EXTENT_DATA_REF_KEY) {
			ret = 1;
			break;
		}

		data_ref = btrfs_delayed_node_to_data_ref(ref);

		 
		if (data_ref->root != root->root_key.objectid ||
		    data_ref->objectid != objectid ||
		    data_ref->offset != offset) {
			ret = 1;
			break;
		}
	}
	spin_unlock(&head->lock);
	mutex_unlock(&head->mutex);
	btrfs_put_transaction(cur_trans);
	return ret;
}

static noinline int check_committed_ref(struct btrfs_root *root,
					struct btrfs_path *path,
					u64 objectid, u64 offset, u64 bytenr,
					bool strict)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_root *extent_root = btrfs_extent_root(fs_info, bytenr);
	struct extent_buffer *leaf;
	struct btrfs_extent_data_ref *ref;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_extent_item *ei;
	struct btrfs_key key;
	u32 item_size;
	int type;
	int ret;

	key.objectid = bytenr;
	key.offset = (u64)-1;
	key.type = BTRFS_EXTENT_ITEM_KEY;

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	BUG_ON(ret == 0);  

	ret = -ENOENT;
	if (path->slots[0] == 0)
		goto out;

	path->slots[0]--;
	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	if (key.objectid != bytenr || key.type != BTRFS_EXTENT_ITEM_KEY)
		goto out;

	ret = 1;
	item_size = btrfs_item_size(leaf, path->slots[0]);
	ei = btrfs_item_ptr(leaf, path->slots[0], struct btrfs_extent_item);

	 
	if (item_size != sizeof(*ei) +
	    btrfs_extent_inline_ref_size(BTRFS_EXTENT_DATA_REF_KEY))
		goto out;

	 
	if (!strict &&
	    (btrfs_extent_generation(leaf, ei) <=
	     btrfs_root_last_snapshot(&root->root_item)))
		goto out;

	iref = (struct btrfs_extent_inline_ref *)(ei + 1);

	 
	type = btrfs_get_extent_inline_ref_type(leaf, iref, BTRFS_REF_TYPE_DATA);
	if (type != BTRFS_EXTENT_DATA_REF_KEY)
		goto out;

	ref = (struct btrfs_extent_data_ref *)(&iref->offset);
	if (btrfs_extent_refs(leaf, ei) !=
	    btrfs_extent_data_ref_count(leaf, ref) ||
	    btrfs_extent_data_ref_root(leaf, ref) !=
	    root->root_key.objectid ||
	    btrfs_extent_data_ref_objectid(leaf, ref) != objectid ||
	    btrfs_extent_data_ref_offset(leaf, ref) != offset)
		goto out;

	ret = 0;
out:
	return ret;
}

int btrfs_cross_ref_exist(struct btrfs_root *root, u64 objectid, u64 offset,
			  u64 bytenr, bool strict, struct btrfs_path *path)
{
	int ret;

	do {
		ret = check_committed_ref(root, path, objectid,
					  offset, bytenr, strict);
		if (ret && ret != -ENOENT)
			goto out;

		ret = check_delayed_ref(root, path, objectid, offset, bytenr);
	} while (ret == -EAGAIN);

out:
	btrfs_release_path(path);
	if (btrfs_is_data_reloc_root(root))
		WARN_ON(ret > 0);
	return ret;
}

static int __btrfs_mod_ref(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct extent_buffer *buf,
			   int full_backref, int inc)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 bytenr;
	u64 num_bytes;
	u64 parent;
	u64 ref_root;
	u32 nritems;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	struct btrfs_ref generic_ref = { 0 };
	bool for_reloc = btrfs_header_flag(buf, BTRFS_HEADER_FLAG_RELOC);
	int i;
	int action;
	int level;
	int ret = 0;

	if (btrfs_is_testing(fs_info))
		return 0;

	ref_root = btrfs_header_owner(buf);
	nritems = btrfs_header_nritems(buf);
	level = btrfs_header_level(buf);

	if (!test_bit(BTRFS_ROOT_SHAREABLE, &root->state) && level == 0)
		return 0;

	if (full_backref)
		parent = buf->start;
	else
		parent = 0;
	if (inc)
		action = BTRFS_ADD_DELAYED_REF;
	else
		action = BTRFS_DROP_DELAYED_REF;

	for (i = 0; i < nritems; i++) {
		if (level == 0) {
			btrfs_item_key_to_cpu(buf, &key, i);
			if (key.type != BTRFS_EXTENT_DATA_KEY)
				continue;
			fi = btrfs_item_ptr(buf, i,
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(buf, fi) ==
			    BTRFS_FILE_EXTENT_INLINE)
				continue;
			bytenr = btrfs_file_extent_disk_bytenr(buf, fi);
			if (bytenr == 0)
				continue;

			num_bytes = btrfs_file_extent_disk_num_bytes(buf, fi);
			key.offset -= btrfs_file_extent_offset(buf, fi);
			btrfs_init_generic_ref(&generic_ref, action, bytenr,
					       num_bytes, parent);
			btrfs_init_data_ref(&generic_ref, ref_root, key.objectid,
					    key.offset, root->root_key.objectid,
					    for_reloc);
			if (inc)
				ret = btrfs_inc_extent_ref(trans, &generic_ref);
			else
				ret = btrfs_free_extent(trans, &generic_ref);
			if (ret)
				goto fail;
		} else {
			bytenr = btrfs_node_blockptr(buf, i);
			num_bytes = fs_info->nodesize;
			btrfs_init_generic_ref(&generic_ref, action, bytenr,
					       num_bytes, parent);
			btrfs_init_tree_ref(&generic_ref, level - 1, ref_root,
					    root->root_key.objectid, for_reloc);
			if (inc)
				ret = btrfs_inc_extent_ref(trans, &generic_ref);
			else
				ret = btrfs_free_extent(trans, &generic_ref);
			if (ret)
				goto fail;
		}
	}
	return 0;
fail:
	return ret;
}

int btrfs_inc_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref)
{
	return __btrfs_mod_ref(trans, root, buf, full_backref, 1);
}

int btrfs_dec_ref(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct extent_buffer *buf, int full_backref)
{
	return __btrfs_mod_ref(trans, root, buf, full_backref, 0);
}

static u64 get_alloc_profile_by_root(struct btrfs_root *root, int data)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 flags;
	u64 ret;

	if (data)
		flags = BTRFS_BLOCK_GROUP_DATA;
	else if (root == fs_info->chunk_root)
		flags = BTRFS_BLOCK_GROUP_SYSTEM;
	else
		flags = BTRFS_BLOCK_GROUP_METADATA;

	ret = btrfs_get_alloc_profile(fs_info, flags);
	return ret;
}

static u64 first_logical_byte(struct btrfs_fs_info *fs_info)
{
	struct rb_node *leftmost;
	u64 bytenr = 0;

	read_lock(&fs_info->block_group_cache_lock);
	 
	leftmost = rb_first_cached(&fs_info->block_group_cache_tree);
	if (leftmost) {
		struct btrfs_block_group *bg;

		bg = rb_entry(leftmost, struct btrfs_block_group, cache_node);
		bytenr = bg->start;
	}
	read_unlock(&fs_info->block_group_cache_lock);

	return bytenr;
}

static int pin_down_extent(struct btrfs_trans_handle *trans,
			   struct btrfs_block_group *cache,
			   u64 bytenr, u64 num_bytes, int reserved)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;

	spin_lock(&cache->space_info->lock);
	spin_lock(&cache->lock);
	cache->pinned += num_bytes;
	btrfs_space_info_update_bytes_pinned(fs_info, cache->space_info,
					     num_bytes);
	if (reserved) {
		cache->reserved -= num_bytes;
		cache->space_info->bytes_reserved -= num_bytes;
	}
	spin_unlock(&cache->lock);
	spin_unlock(&cache->space_info->lock);

	set_extent_bit(&trans->transaction->pinned_extents, bytenr,
		       bytenr + num_bytes - 1, EXTENT_DIRTY, NULL);
	return 0;
}

int btrfs_pin_extent(struct btrfs_trans_handle *trans,
		     u64 bytenr, u64 num_bytes, int reserved)
{
	struct btrfs_block_group *cache;

	cache = btrfs_lookup_block_group(trans->fs_info, bytenr);
	BUG_ON(!cache);  

	pin_down_extent(trans, cache, bytenr, num_bytes, reserved);

	btrfs_put_block_group(cache);
	return 0;
}

 
int btrfs_pin_extent_for_log_replay(struct btrfs_trans_handle *trans,
				    u64 bytenr, u64 num_bytes)
{
	struct btrfs_block_group *cache;
	int ret;

	cache = btrfs_lookup_block_group(trans->fs_info, bytenr);
	if (!cache)
		return -EINVAL;

	 
	ret = btrfs_cache_block_group(cache, true);
	if (ret)
		goto out;

	pin_down_extent(trans, cache, bytenr, num_bytes, 0);

	 
	ret = btrfs_remove_free_space(cache, bytenr, num_bytes);
out:
	btrfs_put_block_group(cache);
	return ret;
}

static int __exclude_logged_extent(struct btrfs_fs_info *fs_info,
				   u64 start, u64 num_bytes)
{
	int ret;
	struct btrfs_block_group *block_group;

	block_group = btrfs_lookup_block_group(fs_info, start);
	if (!block_group)
		return -EINVAL;

	ret = btrfs_cache_block_group(block_group, true);
	if (ret)
		goto out;

	ret = btrfs_remove_free_space(block_group, start, num_bytes);
out:
	btrfs_put_block_group(block_group);
	return ret;
}

int btrfs_exclude_logged_extents(struct extent_buffer *eb)
{
	struct btrfs_fs_info *fs_info = eb->fs_info;
	struct btrfs_file_extent_item *item;
	struct btrfs_key key;
	int found_type;
	int i;
	int ret = 0;

	if (!btrfs_fs_incompat(fs_info, MIXED_GROUPS))
		return 0;

	for (i = 0; i < btrfs_header_nritems(eb); i++) {
		btrfs_item_key_to_cpu(eb, &key, i);
		if (key.type != BTRFS_EXTENT_DATA_KEY)
			continue;
		item = btrfs_item_ptr(eb, i, struct btrfs_file_extent_item);
		found_type = btrfs_file_extent_type(eb, item);
		if (found_type == BTRFS_FILE_EXTENT_INLINE)
			continue;
		if (btrfs_file_extent_disk_bytenr(eb, item) == 0)
			continue;
		key.objectid = btrfs_file_extent_disk_bytenr(eb, item);
		key.offset = btrfs_file_extent_disk_num_bytes(eb, item);
		ret = __exclude_logged_extent(fs_info, key.objectid, key.offset);
		if (ret)
			break;
	}

	return ret;
}

static void
btrfs_inc_block_group_reservations(struct btrfs_block_group *bg)
{
	atomic_inc(&bg->reservations);
}

 
static struct btrfs_free_cluster *
fetch_cluster_info(struct btrfs_fs_info *fs_info,
		   struct btrfs_space_info *space_info, u64 *empty_cluster)
{
	struct btrfs_free_cluster *ret = NULL;

	*empty_cluster = 0;
	if (btrfs_mixed_space_info(space_info))
		return ret;

	if (space_info->flags & BTRFS_BLOCK_GROUP_METADATA) {
		ret = &fs_info->meta_alloc_cluster;
		if (btrfs_test_opt(fs_info, SSD))
			*empty_cluster = SZ_2M;
		else
			*empty_cluster = SZ_64K;
	} else if ((space_info->flags & BTRFS_BLOCK_GROUP_DATA) &&
		   btrfs_test_opt(fs_info, SSD_SPREAD)) {
		*empty_cluster = SZ_2M;
		ret = &fs_info->data_alloc_cluster;
	}

	return ret;
}

static int unpin_extent_range(struct btrfs_fs_info *fs_info,
			      u64 start, u64 end,
			      const bool return_free_space)
{
	struct btrfs_block_group *cache = NULL;
	struct btrfs_space_info *space_info;
	struct btrfs_block_rsv *global_rsv = &fs_info->global_block_rsv;
	struct btrfs_free_cluster *cluster = NULL;
	u64 len;
	u64 total_unpinned = 0;
	u64 empty_cluster = 0;
	bool readonly;

	while (start <= end) {
		readonly = false;
		if (!cache ||
		    start >= cache->start + cache->length) {
			if (cache)
				btrfs_put_block_group(cache);
			total_unpinned = 0;
			cache = btrfs_lookup_block_group(fs_info, start);
			BUG_ON(!cache);  

			cluster = fetch_cluster_info(fs_info,
						     cache->space_info,
						     &empty_cluster);
			empty_cluster <<= 1;
		}

		len = cache->start + cache->length - start;
		len = min(len, end + 1 - start);

		if (return_free_space)
			btrfs_add_free_space(cache, start, len);

		start += len;
		total_unpinned += len;
		space_info = cache->space_info;

		 
		if (cluster && cluster->fragmented &&
		    total_unpinned > empty_cluster) {
			spin_lock(&cluster->lock);
			cluster->fragmented = 0;
			spin_unlock(&cluster->lock);
		}

		spin_lock(&space_info->lock);
		spin_lock(&cache->lock);
		cache->pinned -= len;
		btrfs_space_info_update_bytes_pinned(fs_info, space_info, -len);
		space_info->max_extent_size = 0;
		if (cache->ro) {
			space_info->bytes_readonly += len;
			readonly = true;
		} else if (btrfs_is_zoned(fs_info)) {
			 
			space_info->bytes_zone_unusable += len;
			readonly = true;
		}
		spin_unlock(&cache->lock);
		if (!readonly && return_free_space &&
		    global_rsv->space_info == space_info) {
			spin_lock(&global_rsv->lock);
			if (!global_rsv->full) {
				u64 to_add = min(len, global_rsv->size -
						      global_rsv->reserved);

				global_rsv->reserved += to_add;
				btrfs_space_info_update_bytes_may_use(fs_info,
						space_info, to_add);
				if (global_rsv->reserved >= global_rsv->size)
					global_rsv->full = 1;
				len -= to_add;
			}
			spin_unlock(&global_rsv->lock);
		}
		 
		if (!readonly && return_free_space && len)
			btrfs_try_granting_tickets(fs_info, space_info);
		spin_unlock(&space_info->lock);
	}

	if (cache)
		btrfs_put_block_group(cache);
	return 0;
}

int btrfs_finish_extent_commit(struct btrfs_trans_handle *trans)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_block_group *block_group, *tmp;
	struct list_head *deleted_bgs;
	struct extent_io_tree *unpin;
	u64 start;
	u64 end;
	int ret;

	unpin = &trans->transaction->pinned_extents;

	while (!TRANS_ABORTED(trans)) {
		struct extent_state *cached_state = NULL;

		mutex_lock(&fs_info->unused_bg_unpin_mutex);
		if (!find_first_extent_bit(unpin, 0, &start, &end,
					   EXTENT_DIRTY, &cached_state)) {
			mutex_unlock(&fs_info->unused_bg_unpin_mutex);
			break;
		}

		if (btrfs_test_opt(fs_info, DISCARD_SYNC))
			ret = btrfs_discard_extent(fs_info, start,
						   end + 1 - start, NULL);

		clear_extent_dirty(unpin, start, end, &cached_state);
		unpin_extent_range(fs_info, start, end, true);
		mutex_unlock(&fs_info->unused_bg_unpin_mutex);
		free_extent_state(cached_state);
		cond_resched();
	}

	if (btrfs_test_opt(fs_info, DISCARD_ASYNC)) {
		btrfs_discard_calc_delay(&fs_info->discard_ctl);
		btrfs_discard_schedule_work(&fs_info->discard_ctl, true);
	}

	 
	deleted_bgs = &trans->transaction->deleted_bgs;
	list_for_each_entry_safe(block_group, tmp, deleted_bgs, bg_list) {
		u64 trimmed = 0;

		ret = -EROFS;
		if (!TRANS_ABORTED(trans))
			ret = btrfs_discard_extent(fs_info,
						   block_group->start,
						   block_group->length,
						   &trimmed);

		list_del_init(&block_group->bg_list);
		btrfs_unfreeze_block_group(block_group);
		btrfs_put_block_group(block_group);

		if (ret) {
			const char *errstr = btrfs_decode_error(ret);
			btrfs_warn(fs_info,
			   "discard failed while removing blockgroup: errno=%d %s",
				   ret, errstr);
		}
	}

	return 0;
}

static int do_free_extent_accounting(struct btrfs_trans_handle *trans,
				     u64 bytenr, u64 num_bytes, bool is_data)
{
	int ret;

	if (is_data) {
		struct btrfs_root *csum_root;

		csum_root = btrfs_csum_root(trans->fs_info, bytenr);
		ret = btrfs_del_csums(trans, csum_root, bytenr, num_bytes);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
	}

	ret = add_to_free_space_tree(trans, bytenr, num_bytes);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	ret = btrfs_update_block_group(trans, bytenr, num_bytes, false);
	if (ret)
		btrfs_abort_transaction(trans, ret);

	return ret;
}

#define abort_and_dump(trans, path, fmt, args...)	\
({							\
	btrfs_abort_transaction(trans, -EUCLEAN);	\
	btrfs_print_leaf(path->nodes[0]);		\
	btrfs_crit(trans->fs_info, fmt, ##args);	\
})

 
static int __btrfs_free_extent(struct btrfs_trans_handle *trans,
			       struct btrfs_delayed_ref_node *node, u64 parent,
			       u64 root_objectid, u64 owner_objectid,
			       u64 owner_offset, int refs_to_drop,
			       struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_fs_info *info = trans->fs_info;
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_root *extent_root;
	struct extent_buffer *leaf;
	struct btrfs_extent_item *ei;
	struct btrfs_extent_inline_ref *iref;
	int ret;
	int is_data;
	int extent_slot = 0;
	int found_extent = 0;
	int num_to_del = 1;
	u32 item_size;
	u64 refs;
	u64 bytenr = node->bytenr;
	u64 num_bytes = node->num_bytes;
	bool skinny_metadata = btrfs_fs_incompat(info, SKINNY_METADATA);

	extent_root = btrfs_extent_root(info, bytenr);
	ASSERT(extent_root);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	is_data = owner_objectid >= BTRFS_FIRST_FREE_OBJECTID;

	if (!is_data && refs_to_drop != 1) {
		btrfs_crit(info,
"invalid refs_to_drop, dropping more than 1 refs for tree block %llu refs_to_drop %u",
			   node->bytenr, refs_to_drop);
		ret = -EINVAL;
		btrfs_abort_transaction(trans, ret);
		goto out;
	}

	if (is_data)
		skinny_metadata = false;

	ret = lookup_extent_backref(trans, path, &iref, bytenr, num_bytes,
				    parent, root_objectid, owner_objectid,
				    owner_offset);
	if (ret == 0) {
		 
		extent_slot = path->slots[0];
		while (extent_slot >= 0) {
			btrfs_item_key_to_cpu(path->nodes[0], &key,
					      extent_slot);
			if (key.objectid != bytenr)
				break;
			if (key.type == BTRFS_EXTENT_ITEM_KEY &&
			    key.offset == num_bytes) {
				found_extent = 1;
				break;
			}
			if (key.type == BTRFS_METADATA_ITEM_KEY &&
			    key.offset == owner_objectid) {
				found_extent = 1;
				break;
			}

			 
			if (path->slots[0] - extent_slot > 5)
				break;
			extent_slot--;
		}

		if (!found_extent) {
			if (iref) {
				abort_and_dump(trans, path,
"invalid iref slot %u, no EXTENT/METADATA_ITEM found but has inline extent ref",
					   path->slots[0]);
				ret = -EUCLEAN;
				goto out;
			}
			 
			ret = remove_extent_backref(trans, extent_root, path,
						    NULL, refs_to_drop, is_data);
			if (ret) {
				btrfs_abort_transaction(trans, ret);
				goto out;
			}
			btrfs_release_path(path);

			 
			key.objectid = bytenr;
			key.type = BTRFS_EXTENT_ITEM_KEY;
			key.offset = num_bytes;

			if (!is_data && skinny_metadata) {
				key.type = BTRFS_METADATA_ITEM_KEY;
				key.offset = owner_objectid;
			}

			ret = btrfs_search_slot(trans, extent_root,
						&key, path, -1, 1);
			if (ret > 0 && skinny_metadata && path->slots[0]) {
				 
				path->slots[0]--;
				btrfs_item_key_to_cpu(path->nodes[0], &key,
						      path->slots[0]);
				if (key.objectid == bytenr &&
				    key.type == BTRFS_EXTENT_ITEM_KEY &&
				    key.offset == num_bytes)
					ret = 0;
			}

			if (ret > 0 && skinny_metadata) {
				skinny_metadata = false;
				key.objectid = bytenr;
				key.type = BTRFS_EXTENT_ITEM_KEY;
				key.offset = num_bytes;
				btrfs_release_path(path);
				ret = btrfs_search_slot(trans, extent_root,
							&key, path, -1, 1);
			}

			if (ret) {
				if (ret > 0)
					btrfs_print_leaf(path->nodes[0]);
				btrfs_err(info,
			"umm, got %d back from search, was looking for %llu, slot %d",
					  ret, bytenr, path->slots[0]);
			}
			if (ret < 0) {
				btrfs_abort_transaction(trans, ret);
				goto out;
			}
			extent_slot = path->slots[0];
		}
	} else if (WARN_ON(ret == -ENOENT)) {
		abort_and_dump(trans, path,
"unable to find ref byte nr %llu parent %llu root %llu owner %llu offset %llu slot %d",
			       bytenr, parent, root_objectid, owner_objectid,
			       owner_offset, path->slots[0]);
		goto out;
	} else {
		btrfs_abort_transaction(trans, ret);
		goto out;
	}

	leaf = path->nodes[0];
	item_size = btrfs_item_size(leaf, extent_slot);
	if (unlikely(item_size < sizeof(*ei))) {
		ret = -EUCLEAN;
		btrfs_err(trans->fs_info,
			  "unexpected extent item size, has %u expect >= %zu",
			  item_size, sizeof(*ei));
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
	ei = btrfs_item_ptr(leaf, extent_slot,
			    struct btrfs_extent_item);
	if (owner_objectid < BTRFS_FIRST_FREE_OBJECTID &&
	    key.type == BTRFS_EXTENT_ITEM_KEY) {
		struct btrfs_tree_block_info *bi;

		if (item_size < sizeof(*ei) + sizeof(*bi)) {
			abort_and_dump(trans, path,
"invalid extent item size for key (%llu, %u, %llu) slot %u owner %llu, has %u expect >= %zu",
				       key.objectid, key.type, key.offset,
				       path->slots[0], owner_objectid, item_size,
				       sizeof(*ei) + sizeof(*bi));
			ret = -EUCLEAN;
			goto out;
		}
		bi = (struct btrfs_tree_block_info *)(ei + 1);
		WARN_ON(owner_objectid != btrfs_tree_block_level(leaf, bi));
	}

	refs = btrfs_extent_refs(leaf, ei);
	if (refs < refs_to_drop) {
		abort_and_dump(trans, path,
		"trying to drop %d refs but we only have %llu for bytenr %llu slot %u",
			       refs_to_drop, refs, bytenr, path->slots[0]);
		ret = -EUCLEAN;
		goto out;
	}
	refs -= refs_to_drop;

	if (refs > 0) {
		if (extent_op)
			__run_delayed_extent_op(extent_op, leaf, ei);
		 
		if (iref) {
			if (!found_extent) {
				abort_and_dump(trans, path,
"invalid iref, got inlined extent ref but no EXTENT/METADATA_ITEM found, slot %u",
					       path->slots[0]);
				ret = -EUCLEAN;
				goto out;
			}
		} else {
			btrfs_set_extent_refs(leaf, ei, refs);
			btrfs_mark_buffer_dirty(trans, leaf);
		}
		if (found_extent) {
			ret = remove_extent_backref(trans, extent_root, path,
						    iref, refs_to_drop, is_data);
			if (ret) {
				btrfs_abort_transaction(trans, ret);
				goto out;
			}
		}
	} else {
		 
		if (found_extent) {
			if (is_data && refs_to_drop !=
			    extent_data_ref_count(path, iref)) {
				abort_and_dump(trans, path,
		"invalid refs_to_drop, current refs %u refs_to_drop %u slot %u",
					       extent_data_ref_count(path, iref),
					       refs_to_drop, path->slots[0]);
				ret = -EUCLEAN;
				goto out;
			}
			if (iref) {
				if (path->slots[0] != extent_slot) {
					abort_and_dump(trans, path,
"invalid iref, extent item key (%llu %u %llu) slot %u doesn't have wanted iref",
						       key.objectid, key.type,
						       key.offset, path->slots[0]);
					ret = -EUCLEAN;
					goto out;
				}
			} else {
				 
				if (path->slots[0] != extent_slot + 1) {
					abort_and_dump(trans, path,
	"invalid SHARED_* item slot %u, previous item is not EXTENT/METADATA_ITEM",
						       path->slots[0]);
					ret = -EUCLEAN;
					goto out;
				}
				path->slots[0] = extent_slot;
				num_to_del = 2;
			}
		}

		ret = btrfs_del_items(trans, extent_root, path, path->slots[0],
				      num_to_del);
		if (ret) {
			btrfs_abort_transaction(trans, ret);
			goto out;
		}
		btrfs_release_path(path);

		ret = do_free_extent_accounting(trans, bytenr, num_bytes, is_data);
	}
	btrfs_release_path(path);

out:
	btrfs_free_path(path);
	return ret;
}

 
static noinline int check_ref_cleanup(struct btrfs_trans_handle *trans,
				      u64 bytenr)
{
	struct btrfs_delayed_ref_head *head;
	struct btrfs_delayed_ref_root *delayed_refs;
	int ret = 0;

	delayed_refs = &trans->transaction->delayed_refs;
	spin_lock(&delayed_refs->lock);
	head = btrfs_find_delayed_ref_head(delayed_refs, bytenr);
	if (!head)
		goto out_delayed_unlock;

	spin_lock(&head->lock);
	if (!RB_EMPTY_ROOT(&head->ref_tree.rb_root))
		goto out;

	if (cleanup_extent_op(head) != NULL)
		goto out;

	 
	if (!mutex_trylock(&head->mutex))
		goto out;

	btrfs_delete_ref_head(delayed_refs, head);
	head->processing = false;

	spin_unlock(&head->lock);
	spin_unlock(&delayed_refs->lock);

	BUG_ON(head->extent_op);
	if (head->must_insert_reserved)
		ret = 1;

	btrfs_cleanup_ref_head_accounting(trans->fs_info, delayed_refs, head);
	mutex_unlock(&head->mutex);
	btrfs_put_delayed_ref_head(head);
	return ret;
out:
	spin_unlock(&head->lock);

out_delayed_unlock:
	spin_unlock(&delayed_refs->lock);
	return 0;
}

void btrfs_free_tree_block(struct btrfs_trans_handle *trans,
			   u64 root_id,
			   struct extent_buffer *buf,
			   u64 parent, int last_ref)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_ref generic_ref = { 0 };
	int ret;

	btrfs_init_generic_ref(&generic_ref, BTRFS_DROP_DELAYED_REF,
			       buf->start, buf->len, parent);
	btrfs_init_tree_ref(&generic_ref, btrfs_header_level(buf),
			    root_id, 0, false);

	if (root_id != BTRFS_TREE_LOG_OBJECTID) {
		btrfs_ref_tree_mod(fs_info, &generic_ref);
		ret = btrfs_add_delayed_tree_ref(trans, &generic_ref, NULL);
		BUG_ON(ret);  
	}

	if (last_ref && btrfs_header_generation(buf) == trans->transid) {
		struct btrfs_block_group *cache;
		bool must_pin = false;

		if (root_id != BTRFS_TREE_LOG_OBJECTID) {
			ret = check_ref_cleanup(trans, buf->start);
			if (!ret) {
				btrfs_redirty_list_add(trans->transaction, buf);
				goto out;
			}
		}

		cache = btrfs_lookup_block_group(fs_info, buf->start);

		if (btrfs_header_flag(buf, BTRFS_HEADER_FLAG_WRITTEN)) {
			pin_down_extent(trans, cache, buf->start, buf->len, 1);
			btrfs_put_block_group(cache);
			goto out;
		}

		 
		if (test_bit(BTRFS_FS_TREE_MOD_LOG_USERS, &fs_info->flags))
			must_pin = true;

		if (must_pin || btrfs_is_zoned(fs_info)) {
			btrfs_redirty_list_add(trans->transaction, buf);
			pin_down_extent(trans, cache, buf->start, buf->len, 1);
			btrfs_put_block_group(cache);
			goto out;
		}

		WARN_ON(test_bit(EXTENT_BUFFER_DIRTY, &buf->bflags));

		btrfs_add_free_space(cache, buf->start, buf->len);
		btrfs_free_reserved_bytes(cache, buf->len, 0);
		btrfs_put_block_group(cache);
		trace_btrfs_reserved_extent_free(fs_info, buf->start, buf->len);
	}
out:
	if (last_ref) {
		 
		clear_bit(EXTENT_BUFFER_CORRUPT, &buf->bflags);
	}
}

 
int btrfs_free_extent(struct btrfs_trans_handle *trans, struct btrfs_ref *ref)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int ret;

	if (btrfs_is_testing(fs_info))
		return 0;

	 
	if ((ref->type == BTRFS_REF_METADATA &&
	     ref->tree_ref.owning_root == BTRFS_TREE_LOG_OBJECTID) ||
	    (ref->type == BTRFS_REF_DATA &&
	     ref->data_ref.owning_root == BTRFS_TREE_LOG_OBJECTID)) {
		 
		btrfs_pin_extent(trans, ref->bytenr, ref->len, 1);
		ret = 0;
	} else if (ref->type == BTRFS_REF_METADATA) {
		ret = btrfs_add_delayed_tree_ref(trans, ref, NULL);
	} else {
		ret = btrfs_add_delayed_data_ref(trans, ref, 0);
	}

	if (!((ref->type == BTRFS_REF_METADATA &&
	       ref->tree_ref.owning_root == BTRFS_TREE_LOG_OBJECTID) ||
	      (ref->type == BTRFS_REF_DATA &&
	       ref->data_ref.owning_root == BTRFS_TREE_LOG_OBJECTID)))
		btrfs_ref_tree_mod(fs_info, ref);

	return ret;
}

enum btrfs_loop_type {
	 
	LOOP_CACHING_NOWAIT,

	 
	LOOP_CACHING_WAIT,

	 
	LOOP_UNSET_SIZE_CLASS,

	 
	LOOP_ALLOC_CHUNK,

	 
	LOOP_WRONG_SIZE_CLASS,

	 
	LOOP_NO_EMPTY_SIZE,
};

static inline void
btrfs_lock_block_group(struct btrfs_block_group *cache,
		       int delalloc)
{
	if (delalloc)
		down_read(&cache->data_rwsem);
}

static inline void btrfs_grab_block_group(struct btrfs_block_group *cache,
		       int delalloc)
{
	btrfs_get_block_group(cache);
	if (delalloc)
		down_read(&cache->data_rwsem);
}

static struct btrfs_block_group *btrfs_lock_cluster(
		   struct btrfs_block_group *block_group,
		   struct btrfs_free_cluster *cluster,
		   int delalloc)
	__acquires(&cluster->refill_lock)
{
	struct btrfs_block_group *used_bg = NULL;

	spin_lock(&cluster->refill_lock);
	while (1) {
		used_bg = cluster->block_group;
		if (!used_bg)
			return NULL;

		if (used_bg == block_group)
			return used_bg;

		btrfs_get_block_group(used_bg);

		if (!delalloc)
			return used_bg;

		if (down_read_trylock(&used_bg->data_rwsem))
			return used_bg;

		spin_unlock(&cluster->refill_lock);

		 
		down_read_nested(&used_bg->data_rwsem, SINGLE_DEPTH_NESTING);

		spin_lock(&cluster->refill_lock);
		if (used_bg == cluster->block_group)
			return used_bg;

		up_read(&used_bg->data_rwsem);
		btrfs_put_block_group(used_bg);
	}
}

static inline void
btrfs_release_block_group(struct btrfs_block_group *cache,
			 int delalloc)
{
	if (delalloc)
		up_read(&cache->data_rwsem);
	btrfs_put_block_group(cache);
}

 
static int find_free_extent_clustered(struct btrfs_block_group *bg,
				      struct find_free_extent_ctl *ffe_ctl,
				      struct btrfs_block_group **cluster_bg_ret)
{
	struct btrfs_block_group *cluster_bg;
	struct btrfs_free_cluster *last_ptr = ffe_ctl->last_ptr;
	u64 aligned_cluster;
	u64 offset;
	int ret;

	cluster_bg = btrfs_lock_cluster(bg, last_ptr, ffe_ctl->delalloc);
	if (!cluster_bg)
		goto refill_cluster;
	if (cluster_bg != bg && (cluster_bg->ro ||
	    !block_group_bits(cluster_bg, ffe_ctl->flags)))
		goto release_cluster;

	offset = btrfs_alloc_from_cluster(cluster_bg, last_ptr,
			ffe_ctl->num_bytes, cluster_bg->start,
			&ffe_ctl->max_extent_size);
	if (offset) {
		 
		spin_unlock(&last_ptr->refill_lock);
		trace_btrfs_reserve_extent_cluster(cluster_bg, ffe_ctl);
		*cluster_bg_ret = cluster_bg;
		ffe_ctl->found_offset = offset;
		return 0;
	}
	WARN_ON(last_ptr->block_group != cluster_bg);

release_cluster:
	 
	if (ffe_ctl->loop >= LOOP_NO_EMPTY_SIZE && cluster_bg != bg) {
		spin_unlock(&last_ptr->refill_lock);
		btrfs_release_block_group(cluster_bg, ffe_ctl->delalloc);
		return -ENOENT;
	}

	 
	btrfs_return_cluster_to_free_space(NULL, last_ptr);

	if (cluster_bg != bg)
		btrfs_release_block_group(cluster_bg, ffe_ctl->delalloc);

refill_cluster:
	if (ffe_ctl->loop >= LOOP_NO_EMPTY_SIZE) {
		spin_unlock(&last_ptr->refill_lock);
		return -ENOENT;
	}

	aligned_cluster = max_t(u64,
			ffe_ctl->empty_cluster + ffe_ctl->empty_size,
			bg->full_stripe_len);
	ret = btrfs_find_space_cluster(bg, last_ptr, ffe_ctl->search_start,
			ffe_ctl->num_bytes, aligned_cluster);
	if (ret == 0) {
		 
		offset = btrfs_alloc_from_cluster(bg, last_ptr,
				ffe_ctl->num_bytes, ffe_ctl->search_start,
				&ffe_ctl->max_extent_size);
		if (offset) {
			 
			spin_unlock(&last_ptr->refill_lock);
			ffe_ctl->found_offset = offset;
			trace_btrfs_reserve_extent_cluster(bg, ffe_ctl);
			return 0;
		}
	}
	 
	btrfs_return_cluster_to_free_space(NULL, last_ptr);
	spin_unlock(&last_ptr->refill_lock);
	return 1;
}

 
static int find_free_extent_unclustered(struct btrfs_block_group *bg,
					struct find_free_extent_ctl *ffe_ctl)
{
	struct btrfs_free_cluster *last_ptr = ffe_ctl->last_ptr;
	u64 offset;

	 
	if (unlikely(last_ptr)) {
		spin_lock(&last_ptr->lock);
		last_ptr->fragmented = 1;
		spin_unlock(&last_ptr->lock);
	}
	if (ffe_ctl->cached) {
		struct btrfs_free_space_ctl *free_space_ctl;

		free_space_ctl = bg->free_space_ctl;
		spin_lock(&free_space_ctl->tree_lock);
		if (free_space_ctl->free_space <
		    ffe_ctl->num_bytes + ffe_ctl->empty_cluster +
		    ffe_ctl->empty_size) {
			ffe_ctl->total_free_space = max_t(u64,
					ffe_ctl->total_free_space,
					free_space_ctl->free_space);
			spin_unlock(&free_space_ctl->tree_lock);
			return 1;
		}
		spin_unlock(&free_space_ctl->tree_lock);
	}

	offset = btrfs_find_space_for_alloc(bg, ffe_ctl->search_start,
			ffe_ctl->num_bytes, ffe_ctl->empty_size,
			&ffe_ctl->max_extent_size);
	if (!offset)
		return 1;
	ffe_ctl->found_offset = offset;
	return 0;
}

static int do_allocation_clustered(struct btrfs_block_group *block_group,
				   struct find_free_extent_ctl *ffe_ctl,
				   struct btrfs_block_group **bg_ret)
{
	int ret;

	 
	if (ffe_ctl->last_ptr && ffe_ctl->use_cluster) {
		ret = find_free_extent_clustered(block_group, ffe_ctl, bg_ret);
		if (ret >= 0)
			return ret;
		 
	}

	return find_free_extent_unclustered(block_group, ffe_ctl);
}

 

 
static int do_allocation_zoned(struct btrfs_block_group *block_group,
			       struct find_free_extent_ctl *ffe_ctl,
			       struct btrfs_block_group **bg_ret)
{
	struct btrfs_fs_info *fs_info = block_group->fs_info;
	struct btrfs_space_info *space_info = block_group->space_info;
	struct btrfs_free_space_ctl *ctl = block_group->free_space_ctl;
	u64 start = block_group->start;
	u64 num_bytes = ffe_ctl->num_bytes;
	u64 avail;
	u64 bytenr = block_group->start;
	u64 log_bytenr;
	u64 data_reloc_bytenr;
	int ret = 0;
	bool skip = false;

	ASSERT(btrfs_is_zoned(block_group->fs_info));

	 
	spin_lock(&fs_info->treelog_bg_lock);
	log_bytenr = fs_info->treelog_bg;
	if (log_bytenr && ((ffe_ctl->for_treelog && bytenr != log_bytenr) ||
			   (!ffe_ctl->for_treelog && bytenr == log_bytenr)))
		skip = true;
	spin_unlock(&fs_info->treelog_bg_lock);
	if (skip)
		return 1;

	 
	spin_lock(&fs_info->relocation_bg_lock);
	data_reloc_bytenr = fs_info->data_reloc_bg;
	if (data_reloc_bytenr &&
	    ((ffe_ctl->for_data_reloc && bytenr != data_reloc_bytenr) ||
	     (!ffe_ctl->for_data_reloc && bytenr == data_reloc_bytenr)))
		skip = true;
	spin_unlock(&fs_info->relocation_bg_lock);
	if (skip)
		return 1;

	 
	spin_lock(&block_group->lock);
	if (block_group->ro || btrfs_zoned_bg_is_full(block_group)) {
		ret = 1;
		 
	}
	spin_unlock(&block_group->lock);

	 
	if (!ret && (block_group->flags & BTRFS_BLOCK_GROUP_DATA) &&
	    !btrfs_zone_activate(block_group)) {
		ret = 1;
		 
	}

	spin_lock(&space_info->lock);
	spin_lock(&block_group->lock);
	spin_lock(&fs_info->treelog_bg_lock);
	spin_lock(&fs_info->relocation_bg_lock);

	if (ret)
		goto out;

	ASSERT(!ffe_ctl->for_treelog ||
	       block_group->start == fs_info->treelog_bg ||
	       fs_info->treelog_bg == 0);
	ASSERT(!ffe_ctl->for_data_reloc ||
	       block_group->start == fs_info->data_reloc_bg ||
	       fs_info->data_reloc_bg == 0);

	if (block_group->ro ||
	    (!ffe_ctl->for_data_reloc &&
	     test_bit(BLOCK_GROUP_FLAG_ZONED_DATA_RELOC, &block_group->runtime_flags))) {
		ret = 1;
		goto out;
	}

	 
	if (ffe_ctl->for_treelog && !fs_info->treelog_bg &&
	    (block_group->used || block_group->reserved)) {
		ret = 1;
		goto out;
	}

	 
	if (ffe_ctl->for_data_reloc && !fs_info->data_reloc_bg &&
	    (block_group->used || block_group->reserved)) {
		ret = 1;
		goto out;
	}

	WARN_ON_ONCE(block_group->alloc_offset > block_group->zone_capacity);
	avail = block_group->zone_capacity - block_group->alloc_offset;
	if (avail < num_bytes) {
		if (ffe_ctl->max_extent_size < avail) {
			 
			ffe_ctl->max_extent_size = avail;
			ffe_ctl->total_free_space = avail;
		}
		ret = 1;
		goto out;
	}

	if (ffe_ctl->for_treelog && !fs_info->treelog_bg)
		fs_info->treelog_bg = block_group->start;

	if (ffe_ctl->for_data_reloc) {
		if (!fs_info->data_reloc_bg)
			fs_info->data_reloc_bg = block_group->start;
		 
		set_bit(BLOCK_GROUP_FLAG_ZONED_DATA_RELOC, &block_group->runtime_flags);
	}

	ffe_ctl->found_offset = start + block_group->alloc_offset;
	block_group->alloc_offset += num_bytes;
	spin_lock(&ctl->tree_lock);
	ctl->free_space -= num_bytes;
	spin_unlock(&ctl->tree_lock);

	 

	ffe_ctl->search_start = ffe_ctl->found_offset;

out:
	if (ret && ffe_ctl->for_treelog)
		fs_info->treelog_bg = 0;
	if (ret && ffe_ctl->for_data_reloc)
		fs_info->data_reloc_bg = 0;
	spin_unlock(&fs_info->relocation_bg_lock);
	spin_unlock(&fs_info->treelog_bg_lock);
	spin_unlock(&block_group->lock);
	spin_unlock(&space_info->lock);
	return ret;
}

static int do_allocation(struct btrfs_block_group *block_group,
			 struct find_free_extent_ctl *ffe_ctl,
			 struct btrfs_block_group **bg_ret)
{
	switch (ffe_ctl->policy) {
	case BTRFS_EXTENT_ALLOC_CLUSTERED:
		return do_allocation_clustered(block_group, ffe_ctl, bg_ret);
	case BTRFS_EXTENT_ALLOC_ZONED:
		return do_allocation_zoned(block_group, ffe_ctl, bg_ret);
	default:
		BUG();
	}
}

static void release_block_group(struct btrfs_block_group *block_group,
				struct find_free_extent_ctl *ffe_ctl,
				int delalloc)
{
	switch (ffe_ctl->policy) {
	case BTRFS_EXTENT_ALLOC_CLUSTERED:
		ffe_ctl->retry_uncached = false;
		break;
	case BTRFS_EXTENT_ALLOC_ZONED:
		 
		break;
	default:
		BUG();
	}

	BUG_ON(btrfs_bg_flags_to_raid_index(block_group->flags) !=
	       ffe_ctl->index);
	btrfs_release_block_group(block_group, delalloc);
}

static void found_extent_clustered(struct find_free_extent_ctl *ffe_ctl,
				   struct btrfs_key *ins)
{
	struct btrfs_free_cluster *last_ptr = ffe_ctl->last_ptr;

	if (!ffe_ctl->use_cluster && last_ptr) {
		spin_lock(&last_ptr->lock);
		last_ptr->window_start = ins->objectid;
		spin_unlock(&last_ptr->lock);
	}
}

static void found_extent(struct find_free_extent_ctl *ffe_ctl,
			 struct btrfs_key *ins)
{
	switch (ffe_ctl->policy) {
	case BTRFS_EXTENT_ALLOC_CLUSTERED:
		found_extent_clustered(ffe_ctl, ins);
		break;
	case BTRFS_EXTENT_ALLOC_ZONED:
		 
		break;
	default:
		BUG();
	}
}

static int can_allocate_chunk_zoned(struct btrfs_fs_info *fs_info,
				    struct find_free_extent_ctl *ffe_ctl)
{
	 
	if (!(ffe_ctl->flags & BTRFS_BLOCK_GROUP_DATA))
		return 0;

	 
	if (btrfs_can_activate_zone(fs_info->fs_devices, ffe_ctl->flags))
		return 0;

	 
	if (ffe_ctl->flags & BTRFS_BLOCK_GROUP_DATA) {
		int ret = btrfs_zone_finish_one_bg(fs_info);

		if (ret == 1)
			return 0;
		else if (ret < 0)
			return ret;
	}

	 
	if (ffe_ctl->max_extent_size >= ffe_ctl->min_alloc_size)
		return -ENOSPC;

	 
	if (ffe_ctl->flags & BTRFS_BLOCK_GROUP_DATA)
		return -EAGAIN;

	 
	return 0;
}

static int can_allocate_chunk(struct btrfs_fs_info *fs_info,
			      struct find_free_extent_ctl *ffe_ctl)
{
	switch (ffe_ctl->policy) {
	case BTRFS_EXTENT_ALLOC_CLUSTERED:
		return 0;
	case BTRFS_EXTENT_ALLOC_ZONED:
		return can_allocate_chunk_zoned(fs_info, ffe_ctl);
	default:
		BUG();
	}
}

 
static int find_free_extent_update_loop(struct btrfs_fs_info *fs_info,
					struct btrfs_key *ins,
					struct find_free_extent_ctl *ffe_ctl,
					bool full_search)
{
	struct btrfs_root *root = fs_info->chunk_root;
	int ret;

	if ((ffe_ctl->loop == LOOP_CACHING_NOWAIT) &&
	    ffe_ctl->have_caching_bg && !ffe_ctl->orig_have_caching_bg)
		ffe_ctl->orig_have_caching_bg = true;

	if (ins->objectid) {
		found_extent(ffe_ctl, ins);
		return 0;
	}

	if (ffe_ctl->loop >= LOOP_CACHING_WAIT && ffe_ctl->have_caching_bg)
		return 1;

	ffe_ctl->index++;
	if (ffe_ctl->index < BTRFS_NR_RAID_TYPES)
		return 1;

	 
	if (ffe_ctl->loop < LOOP_NO_EMPTY_SIZE) {
		ffe_ctl->index = 0;
		 
		if (ffe_ctl->loop == LOOP_CACHING_NOWAIT &&
		    (!ffe_ctl->orig_have_caching_bg && full_search))
			ffe_ctl->loop++;
		ffe_ctl->loop++;

		if (ffe_ctl->loop == LOOP_ALLOC_CHUNK) {
			struct btrfs_trans_handle *trans;
			int exist = 0;

			 
			ret = can_allocate_chunk(fs_info, ffe_ctl);
			if (ret)
				return ret;

			trans = current->journal_info;
			if (trans)
				exist = 1;
			else
				trans = btrfs_join_transaction(root);

			if (IS_ERR(trans)) {
				ret = PTR_ERR(trans);
				return ret;
			}

			ret = btrfs_chunk_alloc(trans, ffe_ctl->flags,
						CHUNK_ALLOC_FORCE_FOR_EXTENT);

			 
			if (ret == -ENOSPC) {
				ret = 0;
				ffe_ctl->loop++;
			}
			else if (ret < 0)
				btrfs_abort_transaction(trans, ret);
			else
				ret = 0;
			if (!exist)
				btrfs_end_transaction(trans);
			if (ret)
				return ret;
		}

		if (ffe_ctl->loop == LOOP_NO_EMPTY_SIZE) {
			if (ffe_ctl->policy != BTRFS_EXTENT_ALLOC_CLUSTERED)
				return -ENOSPC;

			 
			if (ffe_ctl->empty_size == 0 &&
			    ffe_ctl->empty_cluster == 0)
				return -ENOSPC;
			ffe_ctl->empty_size = 0;
			ffe_ctl->empty_cluster = 0;
		}
		return 1;
	}
	return -ENOSPC;
}

static bool find_free_extent_check_size_class(struct find_free_extent_ctl *ffe_ctl,
					      struct btrfs_block_group *bg)
{
	if (ffe_ctl->policy == BTRFS_EXTENT_ALLOC_ZONED)
		return true;
	if (!btrfs_block_group_should_use_size_class(bg))
		return true;
	if (ffe_ctl->loop >= LOOP_WRONG_SIZE_CLASS)
		return true;
	if (ffe_ctl->loop >= LOOP_UNSET_SIZE_CLASS &&
	    bg->size_class == BTRFS_BG_SZ_NONE)
		return true;
	return ffe_ctl->size_class == bg->size_class;
}

static int prepare_allocation_clustered(struct btrfs_fs_info *fs_info,
					struct find_free_extent_ctl *ffe_ctl,
					struct btrfs_space_info *space_info,
					struct btrfs_key *ins)
{
	 
	if (space_info->max_extent_size) {
		spin_lock(&space_info->lock);
		if (space_info->max_extent_size &&
		    ffe_ctl->num_bytes > space_info->max_extent_size) {
			ins->offset = space_info->max_extent_size;
			spin_unlock(&space_info->lock);
			return -ENOSPC;
		} else if (space_info->max_extent_size) {
			ffe_ctl->use_cluster = false;
		}
		spin_unlock(&space_info->lock);
	}

	ffe_ctl->last_ptr = fetch_cluster_info(fs_info, space_info,
					       &ffe_ctl->empty_cluster);
	if (ffe_ctl->last_ptr) {
		struct btrfs_free_cluster *last_ptr = ffe_ctl->last_ptr;

		spin_lock(&last_ptr->lock);
		if (last_ptr->block_group)
			ffe_ctl->hint_byte = last_ptr->window_start;
		if (last_ptr->fragmented) {
			 
			ffe_ctl->hint_byte = last_ptr->window_start;
			ffe_ctl->use_cluster = false;
		}
		spin_unlock(&last_ptr->lock);
	}

	return 0;
}

static int prepare_allocation(struct btrfs_fs_info *fs_info,
			      struct find_free_extent_ctl *ffe_ctl,
			      struct btrfs_space_info *space_info,
			      struct btrfs_key *ins)
{
	switch (ffe_ctl->policy) {
	case BTRFS_EXTENT_ALLOC_CLUSTERED:
		return prepare_allocation_clustered(fs_info, ffe_ctl,
						    space_info, ins);
	case BTRFS_EXTENT_ALLOC_ZONED:
		if (ffe_ctl->for_treelog) {
			spin_lock(&fs_info->treelog_bg_lock);
			if (fs_info->treelog_bg)
				ffe_ctl->hint_byte = fs_info->treelog_bg;
			spin_unlock(&fs_info->treelog_bg_lock);
		}
		if (ffe_ctl->for_data_reloc) {
			spin_lock(&fs_info->relocation_bg_lock);
			if (fs_info->data_reloc_bg)
				ffe_ctl->hint_byte = fs_info->data_reloc_bg;
			spin_unlock(&fs_info->relocation_bg_lock);
		}
		return 0;
	default:
		BUG();
	}
}

 
static noinline int find_free_extent(struct btrfs_root *root,
				     struct btrfs_key *ins,
				     struct find_free_extent_ctl *ffe_ctl)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret = 0;
	int cache_block_group_error = 0;
	struct btrfs_block_group *block_group = NULL;
	struct btrfs_space_info *space_info;
	bool full_search = false;

	WARN_ON(ffe_ctl->num_bytes < fs_info->sectorsize);

	ffe_ctl->search_start = 0;
	 
	ffe_ctl->empty_cluster = 0;
	ffe_ctl->last_ptr = NULL;
	ffe_ctl->use_cluster = true;
	ffe_ctl->have_caching_bg = false;
	ffe_ctl->orig_have_caching_bg = false;
	ffe_ctl->index = btrfs_bg_flags_to_raid_index(ffe_ctl->flags);
	ffe_ctl->loop = 0;
	ffe_ctl->retry_uncached = false;
	ffe_ctl->cached = 0;
	ffe_ctl->max_extent_size = 0;
	ffe_ctl->total_free_space = 0;
	ffe_ctl->found_offset = 0;
	ffe_ctl->policy = BTRFS_EXTENT_ALLOC_CLUSTERED;
	ffe_ctl->size_class = btrfs_calc_block_group_size_class(ffe_ctl->num_bytes);

	if (btrfs_is_zoned(fs_info))
		ffe_ctl->policy = BTRFS_EXTENT_ALLOC_ZONED;

	ins->type = BTRFS_EXTENT_ITEM_KEY;
	ins->objectid = 0;
	ins->offset = 0;

	trace_find_free_extent(root, ffe_ctl);

	space_info = btrfs_find_space_info(fs_info, ffe_ctl->flags);
	if (!space_info) {
		btrfs_err(fs_info, "No space info for %llu", ffe_ctl->flags);
		return -ENOSPC;
	}

	ret = prepare_allocation(fs_info, ffe_ctl, space_info, ins);
	if (ret < 0)
		return ret;

	ffe_ctl->search_start = max(ffe_ctl->search_start,
				    first_logical_byte(fs_info));
	ffe_ctl->search_start = max(ffe_ctl->search_start, ffe_ctl->hint_byte);
	if (ffe_ctl->search_start == ffe_ctl->hint_byte) {
		block_group = btrfs_lookup_block_group(fs_info,
						       ffe_ctl->search_start);
		 
		if (block_group && block_group_bits(block_group, ffe_ctl->flags) &&
		    block_group->cached != BTRFS_CACHE_NO) {
			down_read(&space_info->groups_sem);
			if (list_empty(&block_group->list) ||
			    block_group->ro) {
				 
				btrfs_put_block_group(block_group);
				up_read(&space_info->groups_sem);
			} else {
				ffe_ctl->index = btrfs_bg_flags_to_raid_index(
							block_group->flags);
				btrfs_lock_block_group(block_group,
						       ffe_ctl->delalloc);
				ffe_ctl->hinted = true;
				goto have_block_group;
			}
		} else if (block_group) {
			btrfs_put_block_group(block_group);
		}
	}
search:
	trace_find_free_extent_search_loop(root, ffe_ctl);
	ffe_ctl->have_caching_bg = false;
	if (ffe_ctl->index == btrfs_bg_flags_to_raid_index(ffe_ctl->flags) ||
	    ffe_ctl->index == 0)
		full_search = true;
	down_read(&space_info->groups_sem);
	list_for_each_entry(block_group,
			    &space_info->block_groups[ffe_ctl->index], list) {
		struct btrfs_block_group *bg_ret;

		ffe_ctl->hinted = false;
		 
		if (unlikely(block_group->ro)) {
			if (ffe_ctl->for_treelog)
				btrfs_clear_treelog_bg(block_group);
			if (ffe_ctl->for_data_reloc)
				btrfs_clear_data_reloc_bg(block_group);
			continue;
		}

		btrfs_grab_block_group(block_group, ffe_ctl->delalloc);
		ffe_ctl->search_start = block_group->start;

		 
		if (!block_group_bits(block_group, ffe_ctl->flags)) {
			u64 extra = BTRFS_BLOCK_GROUP_DUP |
				BTRFS_BLOCK_GROUP_RAID1_MASK |
				BTRFS_BLOCK_GROUP_RAID56_MASK |
				BTRFS_BLOCK_GROUP_RAID10;

			 
			if ((ffe_ctl->flags & extra) && !(block_group->flags & extra))
				goto loop;

			 
			btrfs_release_block_group(block_group, ffe_ctl->delalloc);
			continue;
		}

have_block_group:
		trace_find_free_extent_have_block_group(root, ffe_ctl, block_group);
		ffe_ctl->cached = btrfs_block_group_done(block_group);
		if (unlikely(!ffe_ctl->cached)) {
			ffe_ctl->have_caching_bg = true;
			ret = btrfs_cache_block_group(block_group, false);

			 
			if (ret < 0) {
				if (!cache_block_group_error)
					cache_block_group_error = ret;
				ret = 0;
				goto loop;
			}
			ret = 0;
		}

		if (unlikely(block_group->cached == BTRFS_CACHE_ERROR)) {
			if (!cache_block_group_error)
				cache_block_group_error = -EIO;
			goto loop;
		}

		if (!find_free_extent_check_size_class(ffe_ctl, block_group))
			goto loop;

		bg_ret = NULL;
		ret = do_allocation(block_group, ffe_ctl, &bg_ret);
		if (ret > 0)
			goto loop;

		if (bg_ret && bg_ret != block_group) {
			btrfs_release_block_group(block_group, ffe_ctl->delalloc);
			block_group = bg_ret;
		}

		 
		ffe_ctl->search_start = round_up(ffe_ctl->found_offset,
						 fs_info->stripesize);

		 
		if (ffe_ctl->search_start + ffe_ctl->num_bytes >
		    block_group->start + block_group->length) {
			btrfs_add_free_space_unused(block_group,
					    ffe_ctl->found_offset,
					    ffe_ctl->num_bytes);
			goto loop;
		}

		if (ffe_ctl->found_offset < ffe_ctl->search_start)
			btrfs_add_free_space_unused(block_group,
					ffe_ctl->found_offset,
					ffe_ctl->search_start - ffe_ctl->found_offset);

		ret = btrfs_add_reserved_bytes(block_group, ffe_ctl->ram_bytes,
					       ffe_ctl->num_bytes,
					       ffe_ctl->delalloc,
					       ffe_ctl->loop >= LOOP_WRONG_SIZE_CLASS);
		if (ret == -EAGAIN) {
			btrfs_add_free_space_unused(block_group,
					ffe_ctl->found_offset,
					ffe_ctl->num_bytes);
			goto loop;
		}
		btrfs_inc_block_group_reservations(block_group);

		 
		ins->objectid = ffe_ctl->search_start;
		ins->offset = ffe_ctl->num_bytes;

		trace_btrfs_reserve_extent(block_group, ffe_ctl);
		btrfs_release_block_group(block_group, ffe_ctl->delalloc);
		break;
loop:
		if (!ffe_ctl->cached && ffe_ctl->loop > LOOP_CACHING_NOWAIT &&
		    !ffe_ctl->retry_uncached) {
			ffe_ctl->retry_uncached = true;
			btrfs_wait_block_group_cache_progress(block_group,
						ffe_ctl->num_bytes +
						ffe_ctl->empty_cluster +
						ffe_ctl->empty_size);
			goto have_block_group;
		}
		release_block_group(block_group, ffe_ctl, ffe_ctl->delalloc);
		cond_resched();
	}
	up_read(&space_info->groups_sem);

	ret = find_free_extent_update_loop(fs_info, ins, ffe_ctl, full_search);
	if (ret > 0)
		goto search;

	if (ret == -ENOSPC && !cache_block_group_error) {
		 
		if (!ffe_ctl->max_extent_size)
			ffe_ctl->max_extent_size = ffe_ctl->total_free_space;
		spin_lock(&space_info->lock);
		space_info->max_extent_size = ffe_ctl->max_extent_size;
		spin_unlock(&space_info->lock);
		ins->offset = ffe_ctl->max_extent_size;
	} else if (ret == -ENOSPC) {
		ret = cache_block_group_error;
	}
	return ret;
}

 
int btrfs_reserve_extent(struct btrfs_root *root, u64 ram_bytes,
			 u64 num_bytes, u64 min_alloc_size,
			 u64 empty_size, u64 hint_byte,
			 struct btrfs_key *ins, int is_data, int delalloc)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct find_free_extent_ctl ffe_ctl = {};
	bool final_tried = num_bytes == min_alloc_size;
	u64 flags;
	int ret;
	bool for_treelog = (root->root_key.objectid == BTRFS_TREE_LOG_OBJECTID);
	bool for_data_reloc = (btrfs_is_data_reloc_root(root) && is_data);

	flags = get_alloc_profile_by_root(root, is_data);
again:
	WARN_ON(num_bytes < fs_info->sectorsize);

	ffe_ctl.ram_bytes = ram_bytes;
	ffe_ctl.num_bytes = num_bytes;
	ffe_ctl.min_alloc_size = min_alloc_size;
	ffe_ctl.empty_size = empty_size;
	ffe_ctl.flags = flags;
	ffe_ctl.delalloc = delalloc;
	ffe_ctl.hint_byte = hint_byte;
	ffe_ctl.for_treelog = for_treelog;
	ffe_ctl.for_data_reloc = for_data_reloc;

	ret = find_free_extent(root, ins, &ffe_ctl);
	if (!ret && !is_data) {
		btrfs_dec_block_group_reservations(fs_info, ins->objectid);
	} else if (ret == -ENOSPC) {
		if (!final_tried && ins->offset) {
			num_bytes = min(num_bytes >> 1, ins->offset);
			num_bytes = round_down(num_bytes,
					       fs_info->sectorsize);
			num_bytes = max(num_bytes, min_alloc_size);
			ram_bytes = num_bytes;
			if (num_bytes == min_alloc_size)
				final_tried = true;
			goto again;
		} else if (btrfs_test_opt(fs_info, ENOSPC_DEBUG)) {
			struct btrfs_space_info *sinfo;

			sinfo = btrfs_find_space_info(fs_info, flags);
			btrfs_err(fs_info,
	"allocation failed flags %llu, wanted %llu tree-log %d, relocation: %d",
				  flags, num_bytes, for_treelog, for_data_reloc);
			if (sinfo)
				btrfs_dump_space_info(fs_info, sinfo,
						      num_bytes, 1);
		}
	}

	return ret;
}

int btrfs_free_reserved_extent(struct btrfs_fs_info *fs_info,
			       u64 start, u64 len, int delalloc)
{
	struct btrfs_block_group *cache;

	cache = btrfs_lookup_block_group(fs_info, start);
	if (!cache) {
		btrfs_err(fs_info, "Unable to find block group for %llu",
			  start);
		return -ENOSPC;
	}

	btrfs_add_free_space(cache, start, len);
	btrfs_free_reserved_bytes(cache, len, delalloc);
	trace_btrfs_reserved_extent_free(fs_info, start, len);

	btrfs_put_block_group(cache);
	return 0;
}

int btrfs_pin_reserved_extent(struct btrfs_trans_handle *trans, u64 start,
			      u64 len)
{
	struct btrfs_block_group *cache;
	int ret = 0;

	cache = btrfs_lookup_block_group(trans->fs_info, start);
	if (!cache) {
		btrfs_err(trans->fs_info, "unable to find block group for %llu",
			  start);
		return -ENOSPC;
	}

	ret = pin_down_extent(trans, cache, start, len, 1);
	btrfs_put_block_group(cache);
	return ret;
}

static int alloc_reserved_extent(struct btrfs_trans_handle *trans, u64 bytenr,
				 u64 num_bytes)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int ret;

	ret = remove_from_free_space_tree(trans, bytenr, num_bytes);
	if (ret)
		return ret;

	ret = btrfs_update_block_group(trans, bytenr, num_bytes, true);
	if (ret) {
		ASSERT(!ret);
		btrfs_err(fs_info, "update block group failed for %llu %llu",
			  bytenr, num_bytes);
		return ret;
	}

	trace_btrfs_reserved_extent_alloc(fs_info, bytenr, num_bytes);
	return 0;
}

static int alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				      u64 parent, u64 root_objectid,
				      u64 flags, u64 owner, u64 offset,
				      struct btrfs_key *ins, int ref_mod)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *extent_root;
	int ret;
	struct btrfs_extent_item *extent_item;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	int type;
	u32 size;

	if (parent > 0)
		type = BTRFS_SHARED_DATA_REF_KEY;
	else
		type = BTRFS_EXTENT_DATA_REF_KEY;

	size = sizeof(*extent_item) + btrfs_extent_inline_ref_size(type);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	extent_root = btrfs_extent_root(fs_info, ins->objectid);
	ret = btrfs_insert_empty_item(trans, extent_root, path, ins, size);
	if (ret) {
		btrfs_free_path(path);
		return ret;
	}

	leaf = path->nodes[0];
	extent_item = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_item);
	btrfs_set_extent_refs(leaf, extent_item, ref_mod);
	btrfs_set_extent_generation(leaf, extent_item, trans->transid);
	btrfs_set_extent_flags(leaf, extent_item,
			       flags | BTRFS_EXTENT_FLAG_DATA);

	iref = (struct btrfs_extent_inline_ref *)(extent_item + 1);
	btrfs_set_extent_inline_ref_type(leaf, iref, type);
	if (parent > 0) {
		struct btrfs_shared_data_ref *ref;
		ref = (struct btrfs_shared_data_ref *)(iref + 1);
		btrfs_set_extent_inline_ref_offset(leaf, iref, parent);
		btrfs_set_shared_data_ref_count(leaf, ref, ref_mod);
	} else {
		struct btrfs_extent_data_ref *ref;
		ref = (struct btrfs_extent_data_ref *)(&iref->offset);
		btrfs_set_extent_data_ref_root(leaf, ref, root_objectid);
		btrfs_set_extent_data_ref_objectid(leaf, ref, owner);
		btrfs_set_extent_data_ref_offset(leaf, ref, offset);
		btrfs_set_extent_data_ref_count(leaf, ref, ref_mod);
	}

	btrfs_mark_buffer_dirty(trans, path->nodes[0]);
	btrfs_free_path(path);

	return alloc_reserved_extent(trans, ins->objectid, ins->offset);
}

static int alloc_reserved_tree_block(struct btrfs_trans_handle *trans,
				     struct btrfs_delayed_ref_node *node,
				     struct btrfs_delayed_extent_op *extent_op)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_root *extent_root;
	int ret;
	struct btrfs_extent_item *extent_item;
	struct btrfs_key extent_key;
	struct btrfs_tree_block_info *block_info;
	struct btrfs_extent_inline_ref *iref;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	struct btrfs_delayed_tree_ref *ref;
	u32 size = sizeof(*extent_item) + sizeof(*iref);
	u64 flags = extent_op->flags_to_set;
	bool skinny_metadata = btrfs_fs_incompat(fs_info, SKINNY_METADATA);

	ref = btrfs_delayed_node_to_tree_ref(node);

	extent_key.objectid = node->bytenr;
	if (skinny_metadata) {
		extent_key.offset = ref->level;
		extent_key.type = BTRFS_METADATA_ITEM_KEY;
	} else {
		extent_key.offset = node->num_bytes;
		extent_key.type = BTRFS_EXTENT_ITEM_KEY;
		size += sizeof(*block_info);
	}

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	extent_root = btrfs_extent_root(fs_info, extent_key.objectid);
	ret = btrfs_insert_empty_item(trans, extent_root, path, &extent_key,
				      size);
	if (ret) {
		btrfs_free_path(path);
		return ret;
	}

	leaf = path->nodes[0];
	extent_item = btrfs_item_ptr(leaf, path->slots[0],
				     struct btrfs_extent_item);
	btrfs_set_extent_refs(leaf, extent_item, 1);
	btrfs_set_extent_generation(leaf, extent_item, trans->transid);
	btrfs_set_extent_flags(leaf, extent_item,
			       flags | BTRFS_EXTENT_FLAG_TREE_BLOCK);

	if (skinny_metadata) {
		iref = (struct btrfs_extent_inline_ref *)(extent_item + 1);
	} else {
		block_info = (struct btrfs_tree_block_info *)(extent_item + 1);
		btrfs_set_tree_block_key(leaf, block_info, &extent_op->key);
		btrfs_set_tree_block_level(leaf, block_info, ref->level);
		iref = (struct btrfs_extent_inline_ref *)(block_info + 1);
	}

	if (node->type == BTRFS_SHARED_BLOCK_REF_KEY) {
		btrfs_set_extent_inline_ref_type(leaf, iref,
						 BTRFS_SHARED_BLOCK_REF_KEY);
		btrfs_set_extent_inline_ref_offset(leaf, iref, ref->parent);
	} else {
		btrfs_set_extent_inline_ref_type(leaf, iref,
						 BTRFS_TREE_BLOCK_REF_KEY);
		btrfs_set_extent_inline_ref_offset(leaf, iref, ref->root);
	}

	btrfs_mark_buffer_dirty(trans, leaf);
	btrfs_free_path(path);

	return alloc_reserved_extent(trans, node->bytenr, fs_info->nodesize);
}

int btrfs_alloc_reserved_file_extent(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root, u64 owner,
				     u64 offset, u64 ram_bytes,
				     struct btrfs_key *ins)
{
	struct btrfs_ref generic_ref = { 0 };

	BUG_ON(root->root_key.objectid == BTRFS_TREE_LOG_OBJECTID);

	btrfs_init_generic_ref(&generic_ref, BTRFS_ADD_DELAYED_EXTENT,
			       ins->objectid, ins->offset, 0);
	btrfs_init_data_ref(&generic_ref, root->root_key.objectid, owner,
			    offset, 0, false);
	btrfs_ref_tree_mod(root->fs_info, &generic_ref);

	return btrfs_add_delayed_data_ref(trans, &generic_ref, ram_bytes);
}

 
int btrfs_alloc_logged_file_extent(struct btrfs_trans_handle *trans,
				   u64 root_objectid, u64 owner, u64 offset,
				   struct btrfs_key *ins)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int ret;
	struct btrfs_block_group *block_group;
	struct btrfs_space_info *space_info;

	 
	if (!btrfs_fs_incompat(fs_info, MIXED_GROUPS)) {
		ret = __exclude_logged_extent(fs_info, ins->objectid,
					      ins->offset);
		if (ret)
			return ret;
	}

	block_group = btrfs_lookup_block_group(fs_info, ins->objectid);
	if (!block_group)
		return -EINVAL;

	space_info = block_group->space_info;
	spin_lock(&space_info->lock);
	spin_lock(&block_group->lock);
	space_info->bytes_reserved += ins->offset;
	block_group->reserved += ins->offset;
	spin_unlock(&block_group->lock);
	spin_unlock(&space_info->lock);

	ret = alloc_reserved_file_extent(trans, 0, root_objectid, 0, owner,
					 offset, ins, 1);
	if (ret)
		btrfs_pin_extent(trans, ins->objectid, ins->offset, 1);
	btrfs_put_block_group(block_group);
	return ret;
}

static struct extent_buffer *
btrfs_init_new_buffer(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      u64 bytenr, int level, u64 owner,
		      enum btrfs_lock_nesting nest)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *buf;
	u64 lockdep_owner = owner;

	buf = btrfs_find_create_tree_block(fs_info, bytenr, owner, level);
	if (IS_ERR(buf))
		return buf;

	 
	if (buf->lock_owner == current->pid) {
		btrfs_err_rl(fs_info,
"tree block %llu owner %llu already locked by pid=%d, extent tree corruption detected",
			buf->start, btrfs_header_owner(buf), current->pid);
		free_extent_buffer(buf);
		return ERR_PTR(-EUCLEAN);
	}

	 
	if (lockdep_owner == BTRFS_TREE_RELOC_OBJECTID &&
	    !test_bit(BTRFS_ROOT_RESET_LOCKDEP_CLASS, &root->state))
		lockdep_owner = BTRFS_FS_TREE_OBJECTID;

	 
	btrfs_set_header_generation(buf, trans->transid);

	 
	btrfs_set_buffer_lockdep_class(lockdep_owner, buf, level);

	__btrfs_tree_lock(buf, nest);
	btrfs_clear_buffer_dirty(trans, buf);
	clear_bit(EXTENT_BUFFER_STALE, &buf->bflags);
	clear_bit(EXTENT_BUFFER_NO_CHECK, &buf->bflags);

	set_extent_buffer_uptodate(buf);

	memzero_extent_buffer(buf, 0, sizeof(struct btrfs_header));
	btrfs_set_header_level(buf, level);
	btrfs_set_header_bytenr(buf, buf->start);
	btrfs_set_header_generation(buf, trans->transid);
	btrfs_set_header_backref_rev(buf, BTRFS_MIXED_BACKREF_REV);
	btrfs_set_header_owner(buf, owner);
	write_extent_buffer_fsid(buf, fs_info->fs_devices->metadata_uuid);
	write_extent_buffer_chunk_tree_uuid(buf, fs_info->chunk_tree_uuid);
	if (root->root_key.objectid == BTRFS_TREE_LOG_OBJECTID) {
		buf->log_index = root->log_transid % 2;
		 
		if (buf->log_index == 0)
			set_extent_bit(&root->dirty_log_pages, buf->start,
				       buf->start + buf->len - 1,
				       EXTENT_DIRTY, NULL);
		else
			set_extent_bit(&root->dirty_log_pages, buf->start,
				       buf->start + buf->len - 1,
				       EXTENT_NEW, NULL);
	} else {
		buf->log_index = -1;
		set_extent_bit(&trans->transaction->dirty_pages, buf->start,
			       buf->start + buf->len - 1, EXTENT_DIRTY, NULL);
	}
	 
	return buf;
}

 
struct extent_buffer *btrfs_alloc_tree_block(struct btrfs_trans_handle *trans,
					     struct btrfs_root *root,
					     u64 parent, u64 root_objectid,
					     const struct btrfs_disk_key *key,
					     int level, u64 hint,
					     u64 empty_size,
					     enum btrfs_lock_nesting nest)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key ins;
	struct btrfs_block_rsv *block_rsv;
	struct extent_buffer *buf;
	struct btrfs_delayed_extent_op *extent_op;
	struct btrfs_ref generic_ref = { 0 };
	u64 flags = 0;
	int ret;
	u32 blocksize = fs_info->nodesize;
	bool skinny_metadata = btrfs_fs_incompat(fs_info, SKINNY_METADATA);

#ifdef CONFIG_BTRFS_FS_RUN_SANITY_TESTS
	if (btrfs_is_testing(fs_info)) {
		buf = btrfs_init_new_buffer(trans, root, root->alloc_bytenr,
					    level, root_objectid, nest);
		if (!IS_ERR(buf))
			root->alloc_bytenr += blocksize;
		return buf;
	}
#endif

	block_rsv = btrfs_use_block_rsv(trans, root, blocksize);
	if (IS_ERR(block_rsv))
		return ERR_CAST(block_rsv);

	ret = btrfs_reserve_extent(root, blocksize, blocksize, blocksize,
				   empty_size, hint, &ins, 0, 0);
	if (ret)
		goto out_unuse;

	buf = btrfs_init_new_buffer(trans, root, ins.objectid, level,
				    root_objectid, nest);
	if (IS_ERR(buf)) {
		ret = PTR_ERR(buf);
		goto out_free_reserved;
	}

	if (root_objectid == BTRFS_TREE_RELOC_OBJECTID) {
		if (parent == 0)
			parent = ins.objectid;
		flags |= BTRFS_BLOCK_FLAG_FULL_BACKREF;
	} else
		BUG_ON(parent > 0);

	if (root_objectid != BTRFS_TREE_LOG_OBJECTID) {
		extent_op = btrfs_alloc_delayed_extent_op();
		if (!extent_op) {
			ret = -ENOMEM;
			goto out_free_buf;
		}
		if (key)
			memcpy(&extent_op->key, key, sizeof(extent_op->key));
		else
			memset(&extent_op->key, 0, sizeof(extent_op->key));
		extent_op->flags_to_set = flags;
		extent_op->update_key = skinny_metadata ? false : true;
		extent_op->update_flags = true;
		extent_op->level = level;

		btrfs_init_generic_ref(&generic_ref, BTRFS_ADD_DELAYED_EXTENT,
				       ins.objectid, ins.offset, parent);
		btrfs_init_tree_ref(&generic_ref, level, root_objectid,
				    root->root_key.objectid, false);
		btrfs_ref_tree_mod(fs_info, &generic_ref);
		ret = btrfs_add_delayed_tree_ref(trans, &generic_ref, extent_op);
		if (ret)
			goto out_free_delayed;
	}
	return buf;

out_free_delayed:
	btrfs_free_delayed_extent_op(extent_op);
out_free_buf:
	btrfs_tree_unlock(buf);
	free_extent_buffer(buf);
out_free_reserved:
	btrfs_free_reserved_extent(fs_info, ins.objectid, ins.offset, 0);
out_unuse:
	btrfs_unuse_block_rsv(fs_info, block_rsv, blocksize);
	return ERR_PTR(ret);
}

struct walk_control {
	u64 refs[BTRFS_MAX_LEVEL];
	u64 flags[BTRFS_MAX_LEVEL];
	struct btrfs_key update_progress;
	struct btrfs_key drop_progress;
	int drop_level;
	int stage;
	int level;
	int shared_level;
	int update_ref;
	int keep_locks;
	int reada_slot;
	int reada_count;
	int restarted;
};

#define DROP_REFERENCE	1
#define UPDATE_BACKREF	2

static noinline void reada_walk_down(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     struct walk_control *wc,
				     struct btrfs_path *path)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 bytenr;
	u64 generation;
	u64 refs;
	u64 flags;
	u32 nritems;
	struct btrfs_key key;
	struct extent_buffer *eb;
	int ret;
	int slot;
	int nread = 0;

	if (path->slots[wc->level] < wc->reada_slot) {
		wc->reada_count = wc->reada_count * 2 / 3;
		wc->reada_count = max(wc->reada_count, 2);
	} else {
		wc->reada_count = wc->reada_count * 3 / 2;
		wc->reada_count = min_t(int, wc->reada_count,
					BTRFS_NODEPTRS_PER_BLOCK(fs_info));
	}

	eb = path->nodes[wc->level];
	nritems = btrfs_header_nritems(eb);

	for (slot = path->slots[wc->level]; slot < nritems; slot++) {
		if (nread >= wc->reada_count)
			break;

		cond_resched();
		bytenr = btrfs_node_blockptr(eb, slot);
		generation = btrfs_node_ptr_generation(eb, slot);

		if (slot == path->slots[wc->level])
			goto reada;

		if (wc->stage == UPDATE_BACKREF &&
		    generation <= root->root_key.offset)
			continue;

		 
		ret = btrfs_lookup_extent_info(trans, fs_info, bytenr,
					       wc->level - 1, 1, &refs,
					       &flags);
		 
		if (ret < 0)
			continue;
		BUG_ON(refs == 0);

		if (wc->stage == DROP_REFERENCE) {
			if (refs == 1)
				goto reada;

			if (wc->level == 1 &&
			    (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF))
				continue;
			if (!wc->update_ref ||
			    generation <= root->root_key.offset)
				continue;
			btrfs_node_key_to_cpu(eb, &key, slot);
			ret = btrfs_comp_cpu_keys(&key,
						  &wc->update_progress);
			if (ret < 0)
				continue;
		} else {
			if (wc->level == 1 &&
			    (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF))
				continue;
		}
reada:
		btrfs_readahead_node_child(eb, slot);
		nread++;
	}
	wc->reada_slot = slot;
}

 
static noinline int walk_down_proc(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path,
				   struct walk_control *wc, int lookup_info)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int level = wc->level;
	struct extent_buffer *eb = path->nodes[level];
	u64 flag = BTRFS_BLOCK_FLAG_FULL_BACKREF;
	int ret;

	if (wc->stage == UPDATE_BACKREF &&
	    btrfs_header_owner(eb) != root->root_key.objectid)
		return 1;

	 
	if (lookup_info &&
	    ((wc->stage == DROP_REFERENCE && wc->refs[level] != 1) ||
	     (wc->stage == UPDATE_BACKREF && !(wc->flags[level] & flag)))) {
		BUG_ON(!path->locks[level]);
		ret = btrfs_lookup_extent_info(trans, fs_info,
					       eb->start, level, 1,
					       &wc->refs[level],
					       &wc->flags[level]);
		BUG_ON(ret == -ENOMEM);
		if (ret)
			return ret;
		BUG_ON(wc->refs[level] == 0);
	}

	if (wc->stage == DROP_REFERENCE) {
		if (wc->refs[level] > 1)
			return 1;

		if (path->locks[level] && !wc->keep_locks) {
			btrfs_tree_unlock_rw(eb, path->locks[level]);
			path->locks[level] = 0;
		}
		return 0;
	}

	 
	if (!(wc->flags[level] & flag)) {
		BUG_ON(!path->locks[level]);
		ret = btrfs_inc_ref(trans, root, eb, 1);
		BUG_ON(ret);  
		ret = btrfs_dec_ref(trans, root, eb, 0);
		BUG_ON(ret);  
		ret = btrfs_set_disk_extent_flags(trans, eb, flag);
		BUG_ON(ret);  
		wc->flags[level] |= flag;
	}

	 
	if (path->locks[level] && level > 0) {
		btrfs_tree_unlock_rw(eb, path->locks[level]);
		path->locks[level] = 0;
	}
	return 0;
}

 
static int check_ref_exists(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root, u64 bytenr, u64 parent,
			    int level)
{
	struct btrfs_path *path;
	struct btrfs_extent_inline_ref *iref;
	int ret;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	ret = lookup_extent_backref(trans, path, &iref, bytenr,
				    root->fs_info->nodesize, parent,
				    root->root_key.objectid, level, 0);
	btrfs_free_path(path);
	if (ret == -ENOENT)
		return 0;
	if (ret < 0)
		return ret;
	return 1;
}

 
static noinline int do_walk_down(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct walk_control *wc, int *lookup_info)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 bytenr;
	u64 generation;
	u64 parent;
	struct btrfs_tree_parent_check check = { 0 };
	struct btrfs_key key;
	struct btrfs_ref ref = { 0 };
	struct extent_buffer *next;
	int level = wc->level;
	int reada = 0;
	int ret = 0;
	bool need_account = false;

	generation = btrfs_node_ptr_generation(path->nodes[level],
					       path->slots[level]);
	 
	if (wc->stage == UPDATE_BACKREF &&
	    generation <= root->root_key.offset) {
		*lookup_info = 1;
		return 1;
	}

	bytenr = btrfs_node_blockptr(path->nodes[level], path->slots[level]);

	check.level = level - 1;
	check.transid = generation;
	check.owner_root = root->root_key.objectid;
	check.has_first_key = true;
	btrfs_node_key_to_cpu(path->nodes[level], &check.first_key,
			      path->slots[level]);

	next = find_extent_buffer(fs_info, bytenr);
	if (!next) {
		next = btrfs_find_create_tree_block(fs_info, bytenr,
				root->root_key.objectid, level - 1);
		if (IS_ERR(next))
			return PTR_ERR(next);
		reada = 1;
	}
	btrfs_tree_lock(next);

	ret = btrfs_lookup_extent_info(trans, fs_info, bytenr, level - 1, 1,
				       &wc->refs[level - 1],
				       &wc->flags[level - 1]);
	if (ret < 0)
		goto out_unlock;

	if (unlikely(wc->refs[level - 1] == 0)) {
		btrfs_err(fs_info, "Missing references.");
		ret = -EIO;
		goto out_unlock;
	}
	*lookup_info = 0;

	if (wc->stage == DROP_REFERENCE) {
		if (wc->refs[level - 1] > 1) {
			need_account = true;
			if (level == 1 &&
			    (wc->flags[0] & BTRFS_BLOCK_FLAG_FULL_BACKREF))
				goto skip;

			if (!wc->update_ref ||
			    generation <= root->root_key.offset)
				goto skip;

			btrfs_node_key_to_cpu(path->nodes[level], &key,
					      path->slots[level]);
			ret = btrfs_comp_cpu_keys(&key, &wc->update_progress);
			if (ret < 0)
				goto skip;

			wc->stage = UPDATE_BACKREF;
			wc->shared_level = level - 1;
		}
	} else {
		if (level == 1 &&
		    (wc->flags[0] & BTRFS_BLOCK_FLAG_FULL_BACKREF))
			goto skip;
	}

	if (!btrfs_buffer_uptodate(next, generation, 0)) {
		btrfs_tree_unlock(next);
		free_extent_buffer(next);
		next = NULL;
		*lookup_info = 1;
	}

	if (!next) {
		if (reada && level == 1)
			reada_walk_down(trans, root, wc, path);
		next = read_tree_block(fs_info, bytenr, &check);
		if (IS_ERR(next)) {
			return PTR_ERR(next);
		} else if (!extent_buffer_uptodate(next)) {
			free_extent_buffer(next);
			return -EIO;
		}
		btrfs_tree_lock(next);
	}

	level--;
	ASSERT(level == btrfs_header_level(next));
	if (level != btrfs_header_level(next)) {
		btrfs_err(root->fs_info, "mismatched level");
		ret = -EIO;
		goto out_unlock;
	}
	path->nodes[level] = next;
	path->slots[level] = 0;
	path->locks[level] = BTRFS_WRITE_LOCK;
	wc->level = level;
	if (wc->level == 1)
		wc->reada_slot = 0;
	return 0;
skip:
	wc->refs[level - 1] = 0;
	wc->flags[level - 1] = 0;
	if (wc->stage == DROP_REFERENCE) {
		if (wc->flags[level] & BTRFS_BLOCK_FLAG_FULL_BACKREF) {
			parent = path->nodes[level]->start;
		} else {
			ASSERT(root->root_key.objectid ==
			       btrfs_header_owner(path->nodes[level]));
			if (root->root_key.objectid !=
			    btrfs_header_owner(path->nodes[level])) {
				btrfs_err(root->fs_info,
						"mismatched block owner");
				ret = -EIO;
				goto out_unlock;
			}
			parent = 0;
		}

		 
		if (wc->restarted) {
			ret = check_ref_exists(trans, root, bytenr, parent,
					       level - 1);
			if (ret < 0)
				goto out_unlock;
			if (ret == 0)
				goto no_delete;
			ret = 0;
			wc->restarted = 0;
		}

		 
		if (root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID &&
		    need_account) {
			ret = btrfs_qgroup_trace_subtree(trans, next,
							 generation, level - 1);
			if (ret) {
				btrfs_err_rl(fs_info,
					     "Error %d accounting shared subtree. Quota is out of sync, rescan required.",
					     ret);
			}
		}

		 
		wc->drop_level = level;
		find_next_key(path, level, &wc->drop_progress);

		btrfs_init_generic_ref(&ref, BTRFS_DROP_DELAYED_REF, bytenr,
				       fs_info->nodesize, parent);
		btrfs_init_tree_ref(&ref, level - 1, root->root_key.objectid,
				    0, false);
		ret = btrfs_free_extent(trans, &ref);
		if (ret)
			goto out_unlock;
	}
no_delete:
	*lookup_info = 1;
	ret = 1;

out_unlock:
	btrfs_tree_unlock(next);
	free_extent_buffer(next);

	return ret;
}

 
static noinline int walk_up_proc(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct walk_control *wc)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret;
	int level = wc->level;
	struct extent_buffer *eb = path->nodes[level];
	u64 parent = 0;

	if (wc->stage == UPDATE_BACKREF) {
		BUG_ON(wc->shared_level < level);
		if (level < wc->shared_level)
			goto out;

		ret = find_next_key(path, level + 1, &wc->update_progress);
		if (ret > 0)
			wc->update_ref = 0;

		wc->stage = DROP_REFERENCE;
		wc->shared_level = -1;
		path->slots[level] = 0;

		 
		if (!path->locks[level]) {
			BUG_ON(level == 0);
			btrfs_tree_lock(eb);
			path->locks[level] = BTRFS_WRITE_LOCK;

			ret = btrfs_lookup_extent_info(trans, fs_info,
						       eb->start, level, 1,
						       &wc->refs[level],
						       &wc->flags[level]);
			if (ret < 0) {
				btrfs_tree_unlock_rw(eb, path->locks[level]);
				path->locks[level] = 0;
				return ret;
			}
			BUG_ON(wc->refs[level] == 0);
			if (wc->refs[level] == 1) {
				btrfs_tree_unlock_rw(eb, path->locks[level]);
				path->locks[level] = 0;
				return 1;
			}
		}
	}

	 
	BUG_ON(wc->refs[level] > 1 && !path->locks[level]);

	if (wc->refs[level] == 1) {
		if (level == 0) {
			if (wc->flags[level] & BTRFS_BLOCK_FLAG_FULL_BACKREF)
				ret = btrfs_dec_ref(trans, root, eb, 1);
			else
				ret = btrfs_dec_ref(trans, root, eb, 0);
			BUG_ON(ret);  
			if (is_fstree(root->root_key.objectid)) {
				ret = btrfs_qgroup_trace_leaf_items(trans, eb);
				if (ret) {
					btrfs_err_rl(fs_info,
	"error %d accounting leaf items, quota is out of sync, rescan required",
					     ret);
				}
			}
		}
		 
		if (!path->locks[level]) {
			btrfs_tree_lock(eb);
			path->locks[level] = BTRFS_WRITE_LOCK;
		}
		btrfs_clear_buffer_dirty(trans, eb);
	}

	if (eb == root->node) {
		if (wc->flags[level] & BTRFS_BLOCK_FLAG_FULL_BACKREF)
			parent = eb->start;
		else if (root->root_key.objectid != btrfs_header_owner(eb))
			goto owner_mismatch;
	} else {
		if (wc->flags[level + 1] & BTRFS_BLOCK_FLAG_FULL_BACKREF)
			parent = path->nodes[level + 1]->start;
		else if (root->root_key.objectid !=
			 btrfs_header_owner(path->nodes[level + 1]))
			goto owner_mismatch;
	}

	btrfs_free_tree_block(trans, btrfs_root_id(root), eb, parent,
			      wc->refs[level] == 1);
out:
	wc->refs[level] = 0;
	wc->flags[level] = 0;
	return 0;

owner_mismatch:
	btrfs_err_rl(fs_info, "unexpected tree owner, have %llu expect %llu",
		     btrfs_header_owner(eb), root->root_key.objectid);
	return -EUCLEAN;
}

static noinline int walk_down_tree(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path,
				   struct walk_control *wc)
{
	int level = wc->level;
	int lookup_info = 1;
	int ret = 0;

	while (level >= 0) {
		ret = walk_down_proc(trans, root, path, wc, lookup_info);
		if (ret)
			break;

		if (level == 0)
			break;

		if (path->slots[level] >=
		    btrfs_header_nritems(path->nodes[level]))
			break;

		ret = do_walk_down(trans, root, path, wc, &lookup_info);
		if (ret > 0) {
			path->slots[level]++;
			continue;
		} else if (ret < 0)
			break;
		level = wc->level;
	}
	return (ret == 1) ? 0 : ret;
}

static noinline int walk_up_tree(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 struct walk_control *wc, int max_level)
{
	int level = wc->level;
	int ret;

	path->slots[level] = btrfs_header_nritems(path->nodes[level]);
	while (level < max_level && path->nodes[level]) {
		wc->level = level;
		if (path->slots[level] + 1 <
		    btrfs_header_nritems(path->nodes[level])) {
			path->slots[level]++;
			return 0;
		} else {
			ret = walk_up_proc(trans, root, path, wc);
			if (ret > 0)
				return 0;
			if (ret < 0)
				return ret;

			if (path->locks[level]) {
				btrfs_tree_unlock_rw(path->nodes[level],
						     path->locks[level]);
				path->locks[level] = 0;
			}
			free_extent_buffer(path->nodes[level]);
			path->nodes[level] = NULL;
			level++;
		}
	}
	return 1;
}

 
int btrfs_drop_snapshot(struct btrfs_root *root, int update_ref, int for_reloc)
{
	const bool is_reloc_root = (root->root_key.objectid ==
				    BTRFS_TREE_RELOC_OBJECTID);
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *tree_root = fs_info->tree_root;
	struct btrfs_root_item *root_item = &root->root_item;
	struct walk_control *wc;
	struct btrfs_key key;
	int err = 0;
	int ret;
	int level;
	bool root_dropped = false;
	bool unfinished_drop = false;

	btrfs_debug(fs_info, "Drop subvolume %llu", root->root_key.objectid);

	path = btrfs_alloc_path();
	if (!path) {
		err = -ENOMEM;
		goto out;
	}

	wc = kzalloc(sizeof(*wc), GFP_NOFS);
	if (!wc) {
		btrfs_free_path(path);
		err = -ENOMEM;
		goto out;
	}

	 
	if (for_reloc)
		trans = btrfs_join_transaction(tree_root);
	else
		trans = btrfs_start_transaction(tree_root, 0);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out_free;
	}

	err = btrfs_run_delayed_items(trans);
	if (err)
		goto out_end_trans;

	 
	set_bit(BTRFS_ROOT_DELETING, &root->state);
	unfinished_drop = test_bit(BTRFS_ROOT_UNFINISHED_DROP, &root->state);

	if (btrfs_disk_key_objectid(&root_item->drop_progress) == 0) {
		level = btrfs_header_level(root->node);
		path->nodes[level] = btrfs_lock_root_node(root);
		path->slots[level] = 0;
		path->locks[level] = BTRFS_WRITE_LOCK;
		memset(&wc->update_progress, 0,
		       sizeof(wc->update_progress));
	} else {
		btrfs_disk_key_to_cpu(&key, &root_item->drop_progress);
		memcpy(&wc->update_progress, &key,
		       sizeof(wc->update_progress));

		level = btrfs_root_drop_level(root_item);
		BUG_ON(level == 0);
		path->lowest_level = level;
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
		path->lowest_level = 0;
		if (ret < 0) {
			err = ret;
			goto out_end_trans;
		}
		WARN_ON(ret > 0);

		 
		btrfs_unlock_up_safe(path, 0);

		level = btrfs_header_level(root->node);
		while (1) {
			btrfs_tree_lock(path->nodes[level]);
			path->locks[level] = BTRFS_WRITE_LOCK;

			ret = btrfs_lookup_extent_info(trans, fs_info,
						path->nodes[level]->start,
						level, 1, &wc->refs[level],
						&wc->flags[level]);
			if (ret < 0) {
				err = ret;
				goto out_end_trans;
			}
			BUG_ON(wc->refs[level] == 0);

			if (level == btrfs_root_drop_level(root_item))
				break;

			btrfs_tree_unlock(path->nodes[level]);
			path->locks[level] = 0;
			WARN_ON(wc->refs[level] != 1);
			level--;
		}
	}

	wc->restarted = test_bit(BTRFS_ROOT_DEAD_TREE, &root->state);
	wc->level = level;
	wc->shared_level = -1;
	wc->stage = DROP_REFERENCE;
	wc->update_ref = update_ref;
	wc->keep_locks = 0;
	wc->reada_count = BTRFS_NODEPTRS_PER_BLOCK(fs_info);

	while (1) {

		ret = walk_down_tree(trans, root, path, wc);
		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			err = ret;
			break;
		}

		ret = walk_up_tree(trans, root, path, wc, BTRFS_MAX_LEVEL);
		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			err = ret;
			break;
		}

		if (ret > 0) {
			BUG_ON(wc->stage != DROP_REFERENCE);
			break;
		}

		if (wc->stage == DROP_REFERENCE) {
			wc->drop_level = wc->level;
			btrfs_node_key_to_cpu(path->nodes[wc->drop_level],
					      &wc->drop_progress,
					      path->slots[wc->drop_level]);
		}
		btrfs_cpu_key_to_disk(&root_item->drop_progress,
				      &wc->drop_progress);
		btrfs_set_root_drop_level(root_item, wc->drop_level);

		BUG_ON(wc->level == 0);
		if (btrfs_should_end_transaction(trans) ||
		    (!for_reloc && btrfs_need_cleaner_sleep(fs_info))) {
			ret = btrfs_update_root(trans, tree_root,
						&root->root_key,
						root_item);
			if (ret) {
				btrfs_abort_transaction(trans, ret);
				err = ret;
				goto out_end_trans;
			}

			if (!is_reloc_root)
				btrfs_set_last_root_drop_gen(fs_info, trans->transid);

			btrfs_end_transaction_throttle(trans);
			if (!for_reloc && btrfs_need_cleaner_sleep(fs_info)) {
				btrfs_debug(fs_info,
					    "drop snapshot early exit");
				err = -EAGAIN;
				goto out_free;
			}

		        
			if (for_reloc)
				trans = btrfs_join_transaction(tree_root);
			else
				trans = btrfs_start_transaction(tree_root, 0);
			if (IS_ERR(trans)) {
				err = PTR_ERR(trans);
				goto out_free;
			}
		}
	}
	btrfs_release_path(path);
	if (err)
		goto out_end_trans;

	ret = btrfs_del_root(trans, &root->root_key);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		err = ret;
		goto out_end_trans;
	}

	if (!is_reloc_root) {
		ret = btrfs_find_root(tree_root, &root->root_key, path,
				      NULL, NULL);
		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			err = ret;
			goto out_end_trans;
		} else if (ret > 0) {
			 
			btrfs_del_orphan_item(trans, tree_root,
					      root->root_key.objectid);
		}
	}

	 
	btrfs_qgroup_convert_reserved_meta(root, INT_MAX);
	btrfs_qgroup_free_meta_all_pertrans(root);

	if (test_bit(BTRFS_ROOT_IN_RADIX, &root->state))
		btrfs_add_dropped_root(trans, root);
	else
		btrfs_put_root(root);
	root_dropped = true;
out_end_trans:
	if (!is_reloc_root)
		btrfs_set_last_root_drop_gen(fs_info, trans->transid);

	btrfs_end_transaction_throttle(trans);
out_free:
	kfree(wc);
	btrfs_free_path(path);
out:
	 
	if (!err && unfinished_drop)
		btrfs_maybe_wake_unfinished_drop(fs_info);

	 
	if (!for_reloc && !root_dropped)
		btrfs_add_dead_root(root);
	return err;
}

 
int btrfs_drop_subtree(struct btrfs_trans_handle *trans,
			struct btrfs_root *root,
			struct extent_buffer *node,
			struct extent_buffer *parent)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_path *path;
	struct walk_control *wc;
	int level;
	int parent_level;
	int ret = 0;
	int wret;

	BUG_ON(root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID);

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;

	wc = kzalloc(sizeof(*wc), GFP_NOFS);
	if (!wc) {
		btrfs_free_path(path);
		return -ENOMEM;
	}

	btrfs_assert_tree_write_locked(parent);
	parent_level = btrfs_header_level(parent);
	atomic_inc(&parent->refs);
	path->nodes[parent_level] = parent;
	path->slots[parent_level] = btrfs_header_nritems(parent);

	btrfs_assert_tree_write_locked(node);
	level = btrfs_header_level(node);
	path->nodes[level] = node;
	path->slots[level] = 0;
	path->locks[level] = BTRFS_WRITE_LOCK;

	wc->refs[parent_level] = 1;
	wc->flags[parent_level] = BTRFS_BLOCK_FLAG_FULL_BACKREF;
	wc->level = level;
	wc->shared_level = -1;
	wc->stage = DROP_REFERENCE;
	wc->update_ref = 0;
	wc->keep_locks = 1;
	wc->reada_count = BTRFS_NODEPTRS_PER_BLOCK(fs_info);

	while (1) {
		wret = walk_down_tree(trans, root, path, wc);
		if (wret < 0) {
			ret = wret;
			break;
		}

		wret = walk_up_tree(trans, root, path, wc, parent_level);
		if (wret < 0)
			ret = wret;
		if (wret != 0)
			break;
	}

	kfree(wc);
	btrfs_free_path(path);
	return ret;
}

int btrfs_error_unpin_extent_range(struct btrfs_fs_info *fs_info,
				   u64 start, u64 end)
{
	return unpin_extent_range(fs_info, start, end, false);
}

 
static int btrfs_trim_free_extents(struct btrfs_device *device, u64 *trimmed)
{
	u64 start = BTRFS_DEVICE_RANGE_RESERVED, len = 0, end = 0;
	int ret;

	*trimmed = 0;

	 
	if (!bdev_max_discard_sectors(device->bdev))
		return 0;

	 
	if (!test_bit(BTRFS_DEV_STATE_WRITEABLE, &device->dev_state))
		return 0;

	 
	if (device->total_bytes <= device->bytes_used)
		return 0;

	ret = 0;

	while (1) {
		struct btrfs_fs_info *fs_info = device->fs_info;
		u64 bytes;

		ret = mutex_lock_interruptible(&fs_info->chunk_mutex);
		if (ret)
			break;

		find_first_clear_extent_bit(&device->alloc_state, start,
					    &start, &end,
					    CHUNK_TRIMMED | CHUNK_ALLOCATED);

		 
		if (start > device->total_bytes) {
			WARN_ON(IS_ENABLED(CONFIG_BTRFS_DEBUG));
			btrfs_warn_in_rcu(fs_info,
"ignoring attempt to trim beyond device size: offset %llu length %llu device %s device size %llu",
					  start, end - start + 1,
					  btrfs_dev_name(device),
					  device->total_bytes);
			mutex_unlock(&fs_info->chunk_mutex);
			ret = 0;
			break;
		}

		 
		start = max_t(u64, start, BTRFS_DEVICE_RANGE_RESERVED);

		 
		end = min(end, device->total_bytes - 1);

		len = end - start + 1;

		 
		if (!len) {
			mutex_unlock(&fs_info->chunk_mutex);
			ret = 0;
			break;
		}

		ret = btrfs_issue_discard(device->bdev, start, len,
					  &bytes);
		if (!ret)
			set_extent_bit(&device->alloc_state, start,
				       start + bytes - 1, CHUNK_TRIMMED, NULL);
		mutex_unlock(&fs_info->chunk_mutex);

		if (ret)
			break;

		start += len;
		*trimmed += bytes;

		if (fatal_signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		cond_resched();
	}

	return ret;
}

 
int btrfs_trim_fs(struct btrfs_fs_info *fs_info, struct fstrim_range *range)
{
	struct btrfs_fs_devices *fs_devices = fs_info->fs_devices;
	struct btrfs_block_group *cache = NULL;
	struct btrfs_device *device;
	u64 group_trimmed;
	u64 range_end = U64_MAX;
	u64 start;
	u64 end;
	u64 trimmed = 0;
	u64 bg_failed = 0;
	u64 dev_failed = 0;
	int bg_ret = 0;
	int dev_ret = 0;
	int ret = 0;

	if (range->start == U64_MAX)
		return -EINVAL;

	 
	if (range->len != U64_MAX &&
	    check_add_overflow(range->start, range->len, &range_end))
		return -EINVAL;

	cache = btrfs_lookup_first_block_group(fs_info, range->start);
	for (; cache; cache = btrfs_next_block_group(cache)) {
		if (cache->start >= range_end) {
			btrfs_put_block_group(cache);
			break;
		}

		start = max(range->start, cache->start);
		end = min(range_end, cache->start + cache->length);

		if (end - start >= range->minlen) {
			if (!btrfs_block_group_done(cache)) {
				ret = btrfs_cache_block_group(cache, true);
				if (ret) {
					bg_failed++;
					bg_ret = ret;
					continue;
				}
			}
			ret = btrfs_trim_block_group(cache,
						     &group_trimmed,
						     start,
						     end,
						     range->minlen);

			trimmed += group_trimmed;
			if (ret) {
				bg_failed++;
				bg_ret = ret;
				continue;
			}
		}
	}

	if (bg_failed)
		btrfs_warn(fs_info,
			"failed to trim %llu block group(s), last error %d",
			bg_failed, bg_ret);

	mutex_lock(&fs_devices->device_list_mutex);
	list_for_each_entry(device, &fs_devices->devices, dev_list) {
		if (test_bit(BTRFS_DEV_STATE_MISSING, &device->dev_state))
			continue;

		ret = btrfs_trim_free_extents(device, &group_trimmed);
		if (ret) {
			dev_failed++;
			dev_ret = ret;
			break;
		}

		trimmed += group_trimmed;
	}
	mutex_unlock(&fs_devices->device_list_mutex);

	if (dev_failed)
		btrfs_warn(fs_info,
			"failed to trim %llu device(s), last error %d",
			dev_failed, dev_ret);
	range->len = trimmed;
	if (bg_ret)
		return bg_ret;
	return dev_ret;
}

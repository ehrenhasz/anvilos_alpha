
 

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/mm.h>
#include <linux/error-injection.h>
#include "messages.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "print-tree.h"
#include "locking.h"
#include "volumes.h"
#include "qgroup.h"
#include "tree-mod-log.h"
#include "tree-checker.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"
#include "relocation.h"
#include "file-item.h"

static struct kmem_cache *btrfs_path_cachep;

static int split_node(struct btrfs_trans_handle *trans, struct btrfs_root
		      *root, struct btrfs_path *path, int level);
static int split_leaf(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      const struct btrfs_key *ins_key, struct btrfs_path *path,
		      int data_size, int extend);
static int push_node_left(struct btrfs_trans_handle *trans,
			  struct extent_buffer *dst,
			  struct extent_buffer *src, int empty);
static int balance_node_right(struct btrfs_trans_handle *trans,
			      struct extent_buffer *dst_buf,
			      struct extent_buffer *src_buf);

static const struct btrfs_csums {
	u16		size;
	const char	name[10];
	const char	driver[12];
} btrfs_csums[] = {
	[BTRFS_CSUM_TYPE_CRC32] = { .size = 4, .name = "crc32c" },
	[BTRFS_CSUM_TYPE_XXHASH] = { .size = 8, .name = "xxhash64" },
	[BTRFS_CSUM_TYPE_SHA256] = { .size = 32, .name = "sha256" },
	[BTRFS_CSUM_TYPE_BLAKE2] = { .size = 32, .name = "blake2b",
				     .driver = "blake2b-256" },
};

 
static unsigned int leaf_data_end(const struct extent_buffer *leaf)
{
	u32 nr = btrfs_header_nritems(leaf);

	if (nr == 0)
		return BTRFS_LEAF_DATA_SIZE(leaf->fs_info);
	return btrfs_item_offset(leaf, nr - 1);
}

 
static inline void memmove_leaf_data(const struct extent_buffer *leaf,
				     unsigned long dst_offset,
				     unsigned long src_offset,
				     unsigned long len)
{
	memmove_extent_buffer(leaf, btrfs_item_nr_offset(leaf, 0) + dst_offset,
			      btrfs_item_nr_offset(leaf, 0) + src_offset, len);
}

 
static inline void copy_leaf_data(const struct extent_buffer *dst,
				  const struct extent_buffer *src,
				  unsigned long dst_offset,
				  unsigned long src_offset, unsigned long len)
{
	copy_extent_buffer(dst, src, btrfs_item_nr_offset(dst, 0) + dst_offset,
			   btrfs_item_nr_offset(src, 0) + src_offset, len);
}

 
static inline void memmove_leaf_items(const struct extent_buffer *leaf,
				      int dst_item, int src_item, int nr_items)
{
	memmove_extent_buffer(leaf, btrfs_item_nr_offset(leaf, dst_item),
			      btrfs_item_nr_offset(leaf, src_item),
			      nr_items * sizeof(struct btrfs_item));
}

 
static inline void copy_leaf_items(const struct extent_buffer *dst,
				   const struct extent_buffer *src,
				   int dst_item, int src_item, int nr_items)
{
	copy_extent_buffer(dst, src, btrfs_item_nr_offset(dst, dst_item),
			      btrfs_item_nr_offset(src, src_item),
			      nr_items * sizeof(struct btrfs_item));
}

 
u16 btrfs_csum_type_size(u16 type)
{
	return btrfs_csums[type].size;
}

int btrfs_super_csum_size(const struct btrfs_super_block *s)
{
	u16 t = btrfs_super_csum_type(s);
	 
	return btrfs_csum_type_size(t);
}

const char *btrfs_super_csum_name(u16 csum_type)
{
	 
	return btrfs_csums[csum_type].name;
}

 
const char *btrfs_super_csum_driver(u16 csum_type)
{
	 
	return btrfs_csums[csum_type].driver[0] ?
		btrfs_csums[csum_type].driver :
		btrfs_csums[csum_type].name;
}

size_t __attribute_const__ btrfs_get_num_csums(void)
{
	return ARRAY_SIZE(btrfs_csums);
}

struct btrfs_path *btrfs_alloc_path(void)
{
	might_sleep();

	return kmem_cache_zalloc(btrfs_path_cachep, GFP_NOFS);
}

 
void btrfs_free_path(struct btrfs_path *p)
{
	if (!p)
		return;
	btrfs_release_path(p);
	kmem_cache_free(btrfs_path_cachep, p);
}

 
noinline void btrfs_release_path(struct btrfs_path *p)
{
	int i;

	for (i = 0; i < BTRFS_MAX_LEVEL; i++) {
		p->slots[i] = 0;
		if (!p->nodes[i])
			continue;
		if (p->locks[i]) {
			btrfs_tree_unlock_rw(p->nodes[i], p->locks[i]);
			p->locks[i] = 0;
		}
		free_extent_buffer(p->nodes[i]);
		p->nodes[i] = NULL;
	}
}

 
bool __cold abort_should_print_stack(int errno)
{
	switch (errno) {
	case -EIO:
	case -EROFS:
	case -ENOMEM:
		return false;
	}
	return true;
}

 
struct extent_buffer *btrfs_root_node(struct btrfs_root *root)
{
	struct extent_buffer *eb;

	while (1) {
		rcu_read_lock();
		eb = rcu_dereference(root->node);

		 
		if (atomic_inc_not_zero(&eb->refs)) {
			rcu_read_unlock();
			break;
		}
		rcu_read_unlock();
		synchronize_rcu();
	}
	return eb;
}

 
static void add_root_to_dirty_list(struct btrfs_root *root)
{
	struct btrfs_fs_info *fs_info = root->fs_info;

	if (test_bit(BTRFS_ROOT_DIRTY, &root->state) ||
	    !test_bit(BTRFS_ROOT_TRACK_DIRTY, &root->state))
		return;

	spin_lock(&fs_info->trans_lock);
	if (!test_and_set_bit(BTRFS_ROOT_DIRTY, &root->state)) {
		 
		if (root->root_key.objectid == BTRFS_EXTENT_TREE_OBJECTID)
			list_move_tail(&root->dirty_list,
				       &fs_info->dirty_cowonly_roots);
		else
			list_move(&root->dirty_list,
				  &fs_info->dirty_cowonly_roots);
	}
	spin_unlock(&fs_info->trans_lock);
}

 
int btrfs_copy_root(struct btrfs_trans_handle *trans,
		      struct btrfs_root *root,
		      struct extent_buffer *buf,
		      struct extent_buffer **cow_ret, u64 new_root_objectid)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *cow;
	int ret = 0;
	int level;
	struct btrfs_disk_key disk_key;

	WARN_ON(test_bit(BTRFS_ROOT_SHAREABLE, &root->state) &&
		trans->transid != fs_info->running_transaction->transid);
	WARN_ON(test_bit(BTRFS_ROOT_SHAREABLE, &root->state) &&
		trans->transid != root->last_trans);

	level = btrfs_header_level(buf);
	if (level == 0)
		btrfs_item_key(buf, &disk_key, 0);
	else
		btrfs_node_key(buf, &disk_key, 0);

	cow = btrfs_alloc_tree_block(trans, root, 0, new_root_objectid,
				     &disk_key, level, buf->start, 0,
				     BTRFS_NESTING_NEW_ROOT);
	if (IS_ERR(cow))
		return PTR_ERR(cow);

	copy_extent_buffer_full(cow, buf);
	btrfs_set_header_bytenr(cow, cow->start);
	btrfs_set_header_generation(cow, trans->transid);
	btrfs_set_header_backref_rev(cow, BTRFS_MIXED_BACKREF_REV);
	btrfs_clear_header_flag(cow, BTRFS_HEADER_FLAG_WRITTEN |
				     BTRFS_HEADER_FLAG_RELOC);
	if (new_root_objectid == BTRFS_TREE_RELOC_OBJECTID)
		btrfs_set_header_flag(cow, BTRFS_HEADER_FLAG_RELOC);
	else
		btrfs_set_header_owner(cow, new_root_objectid);

	write_extent_buffer_fsid(cow, fs_info->fs_devices->metadata_uuid);

	WARN_ON(btrfs_header_generation(buf) > trans->transid);
	if (new_root_objectid == BTRFS_TREE_RELOC_OBJECTID)
		ret = btrfs_inc_ref(trans, root, cow, 1);
	else
		ret = btrfs_inc_ref(trans, root, cow, 0);
	if (ret) {
		btrfs_tree_unlock(cow);
		free_extent_buffer(cow);
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	btrfs_mark_buffer_dirty(trans, cow);
	*cow_ret = cow;
	return 0;
}

 
int btrfs_block_can_be_shared(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct extent_buffer *buf)
{
	 
	if (test_bit(BTRFS_ROOT_SHAREABLE, &root->state) &&
	    buf != root->node &&
	    (btrfs_header_generation(buf) <=
	     btrfs_root_last_snapshot(&root->root_item) ||
	     btrfs_header_flag(buf, BTRFS_HEADER_FLAG_RELOC))) {
		if (buf != root->commit_root)
			return 1;
		 
		if (btrfs_header_generation(buf) == trans->transid)
			return 1;
	}

	return 0;
}

static noinline int update_ref_for_cow(struct btrfs_trans_handle *trans,
				       struct btrfs_root *root,
				       struct extent_buffer *buf,
				       struct extent_buffer *cow,
				       int *last_ref)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 refs;
	u64 owner;
	u64 flags;
	u64 new_flags = 0;
	int ret;

	 

	if (btrfs_block_can_be_shared(trans, root, buf)) {
		ret = btrfs_lookup_extent_info(trans, fs_info, buf->start,
					       btrfs_header_level(buf), 1,
					       &refs, &flags);
		if (ret)
			return ret;
		if (unlikely(refs == 0)) {
			btrfs_crit(fs_info,
		"found 0 references for tree block at bytenr %llu level %d root %llu",
				   buf->start, btrfs_header_level(buf),
				   btrfs_root_id(root));
			ret = -EUCLEAN;
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
	} else {
		refs = 1;
		if (root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID ||
		    btrfs_header_backref_rev(buf) < BTRFS_MIXED_BACKREF_REV)
			flags = BTRFS_BLOCK_FLAG_FULL_BACKREF;
		else
			flags = 0;
	}

	owner = btrfs_header_owner(buf);
	BUG_ON(owner == BTRFS_TREE_RELOC_OBJECTID &&
	       !(flags & BTRFS_BLOCK_FLAG_FULL_BACKREF));

	if (refs > 1) {
		if ((owner == root->root_key.objectid ||
		     root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID) &&
		    !(flags & BTRFS_BLOCK_FLAG_FULL_BACKREF)) {
			ret = btrfs_inc_ref(trans, root, buf, 1);
			if (ret)
				return ret;

			if (root->root_key.objectid ==
			    BTRFS_TREE_RELOC_OBJECTID) {
				ret = btrfs_dec_ref(trans, root, buf, 0);
				if (ret)
					return ret;
				ret = btrfs_inc_ref(trans, root, cow, 1);
				if (ret)
					return ret;
			}
			new_flags |= BTRFS_BLOCK_FLAG_FULL_BACKREF;
		} else {

			if (root->root_key.objectid ==
			    BTRFS_TREE_RELOC_OBJECTID)
				ret = btrfs_inc_ref(trans, root, cow, 1);
			else
				ret = btrfs_inc_ref(trans, root, cow, 0);
			if (ret)
				return ret;
		}
		if (new_flags != 0) {
			ret = btrfs_set_disk_extent_flags(trans, buf, new_flags);
			if (ret)
				return ret;
		}
	} else {
		if (flags & BTRFS_BLOCK_FLAG_FULL_BACKREF) {
			if (root->root_key.objectid ==
			    BTRFS_TREE_RELOC_OBJECTID)
				ret = btrfs_inc_ref(trans, root, cow, 1);
			else
				ret = btrfs_inc_ref(trans, root, cow, 0);
			if (ret)
				return ret;
			ret = btrfs_dec_ref(trans, root, buf, 1);
			if (ret)
				return ret;
		}
		btrfs_clear_buffer_dirty(trans, buf);
		*last_ref = 1;
	}
	return 0;
}

 
static noinline int __btrfs_cow_block(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     struct extent_buffer *buf,
			     struct extent_buffer *parent, int parent_slot,
			     struct extent_buffer **cow_ret,
			     u64 search_start, u64 empty_size,
			     enum btrfs_lock_nesting nest)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_disk_key disk_key;
	struct extent_buffer *cow;
	int level, ret;
	int last_ref = 0;
	int unlock_orig = 0;
	u64 parent_start = 0;

	if (*cow_ret == buf)
		unlock_orig = 1;

	btrfs_assert_tree_write_locked(buf);

	WARN_ON(test_bit(BTRFS_ROOT_SHAREABLE, &root->state) &&
		trans->transid != fs_info->running_transaction->transid);
	WARN_ON(test_bit(BTRFS_ROOT_SHAREABLE, &root->state) &&
		trans->transid != root->last_trans);

	level = btrfs_header_level(buf);

	if (level == 0)
		btrfs_item_key(buf, &disk_key, 0);
	else
		btrfs_node_key(buf, &disk_key, 0);

	if ((root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID) && parent)
		parent_start = parent->start;

	cow = btrfs_alloc_tree_block(trans, root, parent_start,
				     root->root_key.objectid, &disk_key, level,
				     search_start, empty_size, nest);
	if (IS_ERR(cow))
		return PTR_ERR(cow);

	 

	copy_extent_buffer_full(cow, buf);
	btrfs_set_header_bytenr(cow, cow->start);
	btrfs_set_header_generation(cow, trans->transid);
	btrfs_set_header_backref_rev(cow, BTRFS_MIXED_BACKREF_REV);
	btrfs_clear_header_flag(cow, BTRFS_HEADER_FLAG_WRITTEN |
				     BTRFS_HEADER_FLAG_RELOC);
	if (root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID)
		btrfs_set_header_flag(cow, BTRFS_HEADER_FLAG_RELOC);
	else
		btrfs_set_header_owner(cow, root->root_key.objectid);

	write_extent_buffer_fsid(cow, fs_info->fs_devices->metadata_uuid);

	ret = update_ref_for_cow(trans, root, buf, cow, &last_ref);
	if (ret) {
		btrfs_tree_unlock(cow);
		free_extent_buffer(cow);
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	if (test_bit(BTRFS_ROOT_SHAREABLE, &root->state)) {
		ret = btrfs_reloc_cow_block(trans, root, buf, cow);
		if (ret) {
			btrfs_tree_unlock(cow);
			free_extent_buffer(cow);
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
	}

	if (buf == root->node) {
		WARN_ON(parent && parent != buf);
		if (root->root_key.objectid == BTRFS_TREE_RELOC_OBJECTID ||
		    btrfs_header_backref_rev(buf) < BTRFS_MIXED_BACKREF_REV)
			parent_start = buf->start;

		ret = btrfs_tree_mod_log_insert_root(root->node, cow, true);
		if (ret < 0) {
			btrfs_tree_unlock(cow);
			free_extent_buffer(cow);
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
		atomic_inc(&cow->refs);
		rcu_assign_pointer(root->node, cow);

		btrfs_free_tree_block(trans, btrfs_root_id(root), buf,
				      parent_start, last_ref);
		free_extent_buffer(buf);
		add_root_to_dirty_list(root);
	} else {
		WARN_ON(trans->transid != btrfs_header_generation(parent));
		ret = btrfs_tree_mod_log_insert_key(parent, parent_slot,
						    BTRFS_MOD_LOG_KEY_REPLACE);
		if (ret) {
			btrfs_tree_unlock(cow);
			free_extent_buffer(cow);
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
		btrfs_set_node_blockptr(parent, parent_slot,
					cow->start);
		btrfs_set_node_ptr_generation(parent, parent_slot,
					      trans->transid);
		btrfs_mark_buffer_dirty(trans, parent);
		if (last_ref) {
			ret = btrfs_tree_mod_log_free_eb(buf);
			if (ret) {
				btrfs_tree_unlock(cow);
				free_extent_buffer(cow);
				btrfs_abort_transaction(trans, ret);
				return ret;
			}
		}
		btrfs_free_tree_block(trans, btrfs_root_id(root), buf,
				      parent_start, last_ref);
	}
	if (unlock_orig)
		btrfs_tree_unlock(buf);
	free_extent_buffer_stale(buf);
	btrfs_mark_buffer_dirty(trans, cow);
	*cow_ret = cow;
	return 0;
}

static inline int should_cow_block(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct extent_buffer *buf)
{
	if (btrfs_is_testing(root->fs_info))
		return 0;

	 
	smp_mb__before_atomic();

	 
	if (btrfs_header_generation(buf) == trans->transid &&
	    !btrfs_header_flag(buf, BTRFS_HEADER_FLAG_WRITTEN) &&
	    !(root->root_key.objectid != BTRFS_TREE_RELOC_OBJECTID &&
	      btrfs_header_flag(buf, BTRFS_HEADER_FLAG_RELOC)) &&
	    !test_bit(BTRFS_ROOT_FORCE_COW, &root->state))
		return 0;
	return 1;
}

 
noinline int btrfs_cow_block(struct btrfs_trans_handle *trans,
		    struct btrfs_root *root, struct extent_buffer *buf,
		    struct extent_buffer *parent, int parent_slot,
		    struct extent_buffer **cow_ret,
		    enum btrfs_lock_nesting nest)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 search_start;
	int ret;

	if (unlikely(test_bit(BTRFS_ROOT_DELETING, &root->state))) {
		btrfs_abort_transaction(trans, -EUCLEAN);
		btrfs_crit(fs_info,
		   "attempt to COW block %llu on root %llu that is being deleted",
			   buf->start, btrfs_root_id(root));
		return -EUCLEAN;
	}

	 
	if (unlikely(trans->transaction != fs_info->running_transaction ||
		     trans->transid != fs_info->generation)) {
		btrfs_abort_transaction(trans, -EUCLEAN);
		btrfs_crit(fs_info,
"unexpected transaction when attempting to COW block %llu on root %llu, transaction %llu running transaction %llu fs generation %llu",
			   buf->start, btrfs_root_id(root), trans->transid,
			   fs_info->running_transaction->transid,
			   fs_info->generation);
		return -EUCLEAN;
	}

	if (!should_cow_block(trans, root, buf)) {
		*cow_ret = buf;
		return 0;
	}

	search_start = buf->start & ~((u64)SZ_1G - 1);

	 
	btrfs_qgroup_trace_subtree_after_cow(trans, root, buf);
	ret = __btrfs_cow_block(trans, root, buf, parent,
				 parent_slot, cow_ret, search_start, 0, nest);

	trace_btrfs_cow_block(root, buf, *cow_ret);

	return ret;
}
ALLOW_ERROR_INJECTION(btrfs_cow_block, ERRNO);

 
static int close_blocks(u64 blocknr, u64 other, u32 blocksize)
{
	if (blocknr < other && other - (blocknr + blocksize) < 32768)
		return 1;
	if (blocknr > other && blocknr - (other + blocksize) < 32768)
		return 1;
	return 0;
}

#ifdef __LITTLE_ENDIAN

 
static int comp_keys(const struct btrfs_disk_key *disk_key,
		     const struct btrfs_key *k2)
{
	const struct btrfs_key *k1 = (const struct btrfs_key *)disk_key;

	return btrfs_comp_cpu_keys(k1, k2);
}

#else

 
static int comp_keys(const struct btrfs_disk_key *disk,
		     const struct btrfs_key *k2)
{
	struct btrfs_key k1;

	btrfs_disk_key_to_cpu(&k1, disk);

	return btrfs_comp_cpu_keys(&k1, k2);
}
#endif

 
int __pure btrfs_comp_cpu_keys(const struct btrfs_key *k1, const struct btrfs_key *k2)
{
	if (k1->objectid > k2->objectid)
		return 1;
	if (k1->objectid < k2->objectid)
		return -1;
	if (k1->type > k2->type)
		return 1;
	if (k1->type < k2->type)
		return -1;
	if (k1->offset > k2->offset)
		return 1;
	if (k1->offset < k2->offset)
		return -1;
	return 0;
}

 
int btrfs_realloc_node(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct extent_buffer *parent,
		       int start_slot, u64 *last_ret,
		       struct btrfs_key *progress)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *cur;
	u64 blocknr;
	u64 search_start = *last_ret;
	u64 last_block = 0;
	u64 other;
	u32 parent_nritems;
	int end_slot;
	int i;
	int err = 0;
	u32 blocksize;
	int progress_passed = 0;
	struct btrfs_disk_key disk_key;

	 
	if (unlikely(trans->transaction != fs_info->running_transaction ||
		     trans->transid != fs_info->generation)) {
		btrfs_abort_transaction(trans, -EUCLEAN);
		btrfs_crit(fs_info,
"unexpected transaction when attempting to reallocate parent %llu for root %llu, transaction %llu running transaction %llu fs generation %llu",
			   parent->start, btrfs_root_id(root), trans->transid,
			   fs_info->running_transaction->transid,
			   fs_info->generation);
		return -EUCLEAN;
	}

	parent_nritems = btrfs_header_nritems(parent);
	blocksize = fs_info->nodesize;
	end_slot = parent_nritems - 1;

	if (parent_nritems <= 1)
		return 0;

	for (i = start_slot; i <= end_slot; i++) {
		int close = 1;

		btrfs_node_key(parent, &disk_key, i);
		if (!progress_passed && comp_keys(&disk_key, progress) < 0)
			continue;

		progress_passed = 1;
		blocknr = btrfs_node_blockptr(parent, i);
		if (last_block == 0)
			last_block = blocknr;

		if (i > 0) {
			other = btrfs_node_blockptr(parent, i - 1);
			close = close_blocks(blocknr, other, blocksize);
		}
		if (!close && i < end_slot) {
			other = btrfs_node_blockptr(parent, i + 1);
			close = close_blocks(blocknr, other, blocksize);
		}
		if (close) {
			last_block = blocknr;
			continue;
		}

		cur = btrfs_read_node_slot(parent, i);
		if (IS_ERR(cur))
			return PTR_ERR(cur);
		if (search_start == 0)
			search_start = last_block;

		btrfs_tree_lock(cur);
		err = __btrfs_cow_block(trans, root, cur, parent, i,
					&cur, search_start,
					min(16 * blocksize,
					    (end_slot - i) * blocksize),
					BTRFS_NESTING_COW);
		if (err) {
			btrfs_tree_unlock(cur);
			free_extent_buffer(cur);
			break;
		}
		search_start = cur->start;
		last_block = cur->start;
		*last_ret = search_start;
		btrfs_tree_unlock(cur);
		free_extent_buffer(cur);
	}
	return err;
}

 
int btrfs_bin_search(struct extent_buffer *eb, int first_slot,
		     const struct btrfs_key *key, int *slot)
{
	unsigned long p;
	int item_size;
	 
	u32 low = first_slot;
	u32 high = btrfs_header_nritems(eb);
	int ret;
	const int key_size = sizeof(struct btrfs_disk_key);

	if (unlikely(low > high)) {
		btrfs_err(eb->fs_info,
		 "%s: low (%u) > high (%u) eb %llu owner %llu level %d",
			  __func__, low, high, eb->start,
			  btrfs_header_owner(eb), btrfs_header_level(eb));
		return -EINVAL;
	}

	if (btrfs_header_level(eb) == 0) {
		p = offsetof(struct btrfs_leaf, items);
		item_size = sizeof(struct btrfs_item);
	} else {
		p = offsetof(struct btrfs_node, ptrs);
		item_size = sizeof(struct btrfs_key_ptr);
	}

	while (low < high) {
		unsigned long oip;
		unsigned long offset;
		struct btrfs_disk_key *tmp;
		struct btrfs_disk_key unaligned;
		int mid;

		mid = (low + high) / 2;
		offset = p + mid * item_size;
		oip = offset_in_page(offset);

		if (oip + key_size <= PAGE_SIZE) {
			const unsigned long idx = get_eb_page_index(offset);
			char *kaddr = page_address(eb->pages[idx]);

			oip = get_eb_offset_in_page(eb, offset);
			tmp = (struct btrfs_disk_key *)(kaddr + oip);
		} else {
			read_extent_buffer(eb, &unaligned, offset, key_size);
			tmp = &unaligned;
		}

		ret = comp_keys(tmp, key);

		if (ret < 0)
			low = mid + 1;
		else if (ret > 0)
			high = mid;
		else {
			*slot = mid;
			return 0;
		}
	}
	*slot = low;
	return 1;
}

static void root_add_used(struct btrfs_root *root, u32 size)
{
	spin_lock(&root->accounting_lock);
	btrfs_set_root_used(&root->root_item,
			    btrfs_root_used(&root->root_item) + size);
	spin_unlock(&root->accounting_lock);
}

static void root_sub_used(struct btrfs_root *root, u32 size)
{
	spin_lock(&root->accounting_lock);
	btrfs_set_root_used(&root->root_item,
			    btrfs_root_used(&root->root_item) - size);
	spin_unlock(&root->accounting_lock);
}

 
struct extent_buffer *btrfs_read_node_slot(struct extent_buffer *parent,
					   int slot)
{
	int level = btrfs_header_level(parent);
	struct btrfs_tree_parent_check check = { 0 };
	struct extent_buffer *eb;

	if (slot < 0 || slot >= btrfs_header_nritems(parent))
		return ERR_PTR(-ENOENT);

	ASSERT(level);

	check.level = level - 1;
	check.transid = btrfs_node_ptr_generation(parent, slot);
	check.owner_root = btrfs_header_owner(parent);
	check.has_first_key = true;
	btrfs_node_key_to_cpu(parent, &check.first_key, slot);

	eb = read_tree_block(parent->fs_info, btrfs_node_blockptr(parent, slot),
			     &check);
	if (IS_ERR(eb))
		return eb;
	if (!extent_buffer_uptodate(eb)) {
		free_extent_buffer(eb);
		return ERR_PTR(-EIO);
	}

	return eb;
}

 
static noinline int balance_level(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 struct btrfs_path *path, int level)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *right = NULL;
	struct extent_buffer *mid;
	struct extent_buffer *left = NULL;
	struct extent_buffer *parent = NULL;
	int ret = 0;
	int wret;
	int pslot;
	int orig_slot = path->slots[level];
	u64 orig_ptr;

	ASSERT(level > 0);

	mid = path->nodes[level];

	WARN_ON(path->locks[level] != BTRFS_WRITE_LOCK);
	WARN_ON(btrfs_header_generation(mid) != trans->transid);

	orig_ptr = btrfs_node_blockptr(mid, orig_slot);

	if (level < BTRFS_MAX_LEVEL - 1) {
		parent = path->nodes[level + 1];
		pslot = path->slots[level + 1];
	}

	 
	if (!parent) {
		struct extent_buffer *child;

		if (btrfs_header_nritems(mid) != 1)
			return 0;

		 
		child = btrfs_read_node_slot(mid, 0);
		if (IS_ERR(child)) {
			ret = PTR_ERR(child);
			goto out;
		}

		btrfs_tree_lock(child);
		ret = btrfs_cow_block(trans, root, child, mid, 0, &child,
				      BTRFS_NESTING_COW);
		if (ret) {
			btrfs_tree_unlock(child);
			free_extent_buffer(child);
			goto out;
		}

		ret = btrfs_tree_mod_log_insert_root(root->node, child, true);
		if (ret < 0) {
			btrfs_tree_unlock(child);
			free_extent_buffer(child);
			btrfs_abort_transaction(trans, ret);
			goto out;
		}
		rcu_assign_pointer(root->node, child);

		add_root_to_dirty_list(root);
		btrfs_tree_unlock(child);

		path->locks[level] = 0;
		path->nodes[level] = NULL;
		btrfs_clear_buffer_dirty(trans, mid);
		btrfs_tree_unlock(mid);
		 
		free_extent_buffer(mid);

		root_sub_used(root, mid->len);
		btrfs_free_tree_block(trans, btrfs_root_id(root), mid, 0, 1);
		 
		free_extent_buffer_stale(mid);
		return 0;
	}
	if (btrfs_header_nritems(mid) >
	    BTRFS_NODEPTRS_PER_BLOCK(fs_info) / 4)
		return 0;

	if (pslot) {
		left = btrfs_read_node_slot(parent, pslot - 1);
		if (IS_ERR(left)) {
			ret = PTR_ERR(left);
			left = NULL;
			goto out;
		}

		__btrfs_tree_lock(left, BTRFS_NESTING_LEFT);
		wret = btrfs_cow_block(trans, root, left,
				       parent, pslot - 1, &left,
				       BTRFS_NESTING_LEFT_COW);
		if (wret) {
			ret = wret;
			goto out;
		}
	}

	if (pslot + 1 < btrfs_header_nritems(parent)) {
		right = btrfs_read_node_slot(parent, pslot + 1);
		if (IS_ERR(right)) {
			ret = PTR_ERR(right);
			right = NULL;
			goto out;
		}

		__btrfs_tree_lock(right, BTRFS_NESTING_RIGHT);
		wret = btrfs_cow_block(trans, root, right,
				       parent, pslot + 1, &right,
				       BTRFS_NESTING_RIGHT_COW);
		if (wret) {
			ret = wret;
			goto out;
		}
	}

	 
	if (left) {
		orig_slot += btrfs_header_nritems(left);
		wret = push_node_left(trans, left, mid, 1);
		if (wret < 0)
			ret = wret;
	}

	 
	if (right) {
		wret = push_node_left(trans, mid, right, 1);
		if (wret < 0 && wret != -ENOSPC)
			ret = wret;
		if (btrfs_header_nritems(right) == 0) {
			btrfs_clear_buffer_dirty(trans, right);
			btrfs_tree_unlock(right);
			ret = btrfs_del_ptr(trans, root, path, level + 1, pslot + 1);
			if (ret < 0) {
				free_extent_buffer_stale(right);
				right = NULL;
				goto out;
			}
			root_sub_used(root, right->len);
			btrfs_free_tree_block(trans, btrfs_root_id(root), right,
					      0, 1);
			free_extent_buffer_stale(right);
			right = NULL;
		} else {
			struct btrfs_disk_key right_key;
			btrfs_node_key(right, &right_key, 0);
			ret = btrfs_tree_mod_log_insert_key(parent, pslot + 1,
					BTRFS_MOD_LOG_KEY_REPLACE);
			if (ret < 0) {
				btrfs_abort_transaction(trans, ret);
				goto out;
			}
			btrfs_set_node_key(parent, &right_key, pslot + 1);
			btrfs_mark_buffer_dirty(trans, parent);
		}
	}
	if (btrfs_header_nritems(mid) == 1) {
		 
		if (unlikely(!left)) {
			btrfs_crit(fs_info,
"missing left child when middle child only has 1 item, parent bytenr %llu level %d mid bytenr %llu root %llu",
				   parent->start, btrfs_header_level(parent),
				   mid->start, btrfs_root_id(root));
			ret = -EUCLEAN;
			btrfs_abort_transaction(trans, ret);
			goto out;
		}
		wret = balance_node_right(trans, mid, left);
		if (wret < 0) {
			ret = wret;
			goto out;
		}
		if (wret == 1) {
			wret = push_node_left(trans, left, mid, 1);
			if (wret < 0)
				ret = wret;
		}
		BUG_ON(wret == 1);
	}
	if (btrfs_header_nritems(mid) == 0) {
		btrfs_clear_buffer_dirty(trans, mid);
		btrfs_tree_unlock(mid);
		ret = btrfs_del_ptr(trans, root, path, level + 1, pslot);
		if (ret < 0) {
			free_extent_buffer_stale(mid);
			mid = NULL;
			goto out;
		}
		root_sub_used(root, mid->len);
		btrfs_free_tree_block(trans, btrfs_root_id(root), mid, 0, 1);
		free_extent_buffer_stale(mid);
		mid = NULL;
	} else {
		 
		struct btrfs_disk_key mid_key;
		btrfs_node_key(mid, &mid_key, 0);
		ret = btrfs_tree_mod_log_insert_key(parent, pslot,
						    BTRFS_MOD_LOG_KEY_REPLACE);
		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			goto out;
		}
		btrfs_set_node_key(parent, &mid_key, pslot);
		btrfs_mark_buffer_dirty(trans, parent);
	}

	 
	if (left) {
		if (btrfs_header_nritems(left) > orig_slot) {
			atomic_inc(&left->refs);
			 
			path->nodes[level] = left;
			path->slots[level + 1] -= 1;
			path->slots[level] = orig_slot;
			if (mid) {
				btrfs_tree_unlock(mid);
				free_extent_buffer(mid);
			}
		} else {
			orig_slot -= btrfs_header_nritems(left);
			path->slots[level] = orig_slot;
		}
	}
	 
	if (orig_ptr !=
	    btrfs_node_blockptr(path->nodes[level], path->slots[level]))
		BUG();
out:
	if (right) {
		btrfs_tree_unlock(right);
		free_extent_buffer(right);
	}
	if (left) {
		if (path->nodes[level] != left)
			btrfs_tree_unlock(left);
		free_extent_buffer(left);
	}
	return ret;
}

 
static noinline int push_nodes_for_insert(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path, int level)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *right = NULL;
	struct extent_buffer *mid;
	struct extent_buffer *left = NULL;
	struct extent_buffer *parent = NULL;
	int ret = 0;
	int wret;
	int pslot;
	int orig_slot = path->slots[level];

	if (level == 0)
		return 1;

	mid = path->nodes[level];
	WARN_ON(btrfs_header_generation(mid) != trans->transid);

	if (level < BTRFS_MAX_LEVEL - 1) {
		parent = path->nodes[level + 1];
		pslot = path->slots[level + 1];
	}

	if (!parent)
		return 1;

	 
	if (pslot) {
		u32 left_nr;

		left = btrfs_read_node_slot(parent, pslot - 1);
		if (IS_ERR(left))
			return PTR_ERR(left);

		__btrfs_tree_lock(left, BTRFS_NESTING_LEFT);

		left_nr = btrfs_header_nritems(left);
		if (left_nr >= BTRFS_NODEPTRS_PER_BLOCK(fs_info) - 1) {
			wret = 1;
		} else {
			ret = btrfs_cow_block(trans, root, left, parent,
					      pslot - 1, &left,
					      BTRFS_NESTING_LEFT_COW);
			if (ret)
				wret = 1;
			else {
				wret = push_node_left(trans, left, mid, 0);
			}
		}
		if (wret < 0)
			ret = wret;
		if (wret == 0) {
			struct btrfs_disk_key disk_key;
			orig_slot += left_nr;
			btrfs_node_key(mid, &disk_key, 0);
			ret = btrfs_tree_mod_log_insert_key(parent, pslot,
					BTRFS_MOD_LOG_KEY_REPLACE);
			if (ret < 0) {
				btrfs_tree_unlock(left);
				free_extent_buffer(left);
				btrfs_abort_transaction(trans, ret);
				return ret;
			}
			btrfs_set_node_key(parent, &disk_key, pslot);
			btrfs_mark_buffer_dirty(trans, parent);
			if (btrfs_header_nritems(left) > orig_slot) {
				path->nodes[level] = left;
				path->slots[level + 1] -= 1;
				path->slots[level] = orig_slot;
				btrfs_tree_unlock(mid);
				free_extent_buffer(mid);
			} else {
				orig_slot -=
					btrfs_header_nritems(left);
				path->slots[level] = orig_slot;
				btrfs_tree_unlock(left);
				free_extent_buffer(left);
			}
			return 0;
		}
		btrfs_tree_unlock(left);
		free_extent_buffer(left);
	}

	 
	if (pslot + 1 < btrfs_header_nritems(parent)) {
		u32 right_nr;

		right = btrfs_read_node_slot(parent, pslot + 1);
		if (IS_ERR(right))
			return PTR_ERR(right);

		__btrfs_tree_lock(right, BTRFS_NESTING_RIGHT);

		right_nr = btrfs_header_nritems(right);
		if (right_nr >= BTRFS_NODEPTRS_PER_BLOCK(fs_info) - 1) {
			wret = 1;
		} else {
			ret = btrfs_cow_block(trans, root, right,
					      parent, pslot + 1,
					      &right, BTRFS_NESTING_RIGHT_COW);
			if (ret)
				wret = 1;
			else {
				wret = balance_node_right(trans, right, mid);
			}
		}
		if (wret < 0)
			ret = wret;
		if (wret == 0) {
			struct btrfs_disk_key disk_key;

			btrfs_node_key(right, &disk_key, 0);
			ret = btrfs_tree_mod_log_insert_key(parent, pslot + 1,
					BTRFS_MOD_LOG_KEY_REPLACE);
			if (ret < 0) {
				btrfs_tree_unlock(right);
				free_extent_buffer(right);
				btrfs_abort_transaction(trans, ret);
				return ret;
			}
			btrfs_set_node_key(parent, &disk_key, pslot + 1);
			btrfs_mark_buffer_dirty(trans, parent);

			if (btrfs_header_nritems(mid) <= orig_slot) {
				path->nodes[level] = right;
				path->slots[level + 1] += 1;
				path->slots[level] = orig_slot -
					btrfs_header_nritems(mid);
				btrfs_tree_unlock(mid);
				free_extent_buffer(mid);
			} else {
				btrfs_tree_unlock(right);
				free_extent_buffer(right);
			}
			return 0;
		}
		btrfs_tree_unlock(right);
		free_extent_buffer(right);
	}
	return 1;
}

 
static void reada_for_search(struct btrfs_fs_info *fs_info,
			     struct btrfs_path *path,
			     int level, int slot, u64 objectid)
{
	struct extent_buffer *node;
	struct btrfs_disk_key disk_key;
	u32 nritems;
	u64 search;
	u64 target;
	u64 nread = 0;
	u64 nread_max;
	u32 nr;
	u32 blocksize;
	u32 nscan = 0;

	if (level != 1 && path->reada != READA_FORWARD_ALWAYS)
		return;

	if (!path->nodes[level])
		return;

	node = path->nodes[level];

	 
	if (path->reada == READA_FORWARD_ALWAYS) {
		if (level > 1)
			nread_max = node->fs_info->nodesize;
		else
			nread_max = SZ_128K;
	} else {
		nread_max = SZ_64K;
	}

	search = btrfs_node_blockptr(node, slot);
	blocksize = fs_info->nodesize;
	if (path->reada != READA_FORWARD_ALWAYS) {
		struct extent_buffer *eb;

		eb = find_extent_buffer(fs_info, search);
		if (eb) {
			free_extent_buffer(eb);
			return;
		}
	}

	target = search;

	nritems = btrfs_header_nritems(node);
	nr = slot;

	while (1) {
		if (path->reada == READA_BACK) {
			if (nr == 0)
				break;
			nr--;
		} else if (path->reada == READA_FORWARD ||
			   path->reada == READA_FORWARD_ALWAYS) {
			nr++;
			if (nr >= nritems)
				break;
		}
		if (path->reada == READA_BACK && objectid) {
			btrfs_node_key(node, &disk_key, nr);
			if (btrfs_disk_key_objectid(&disk_key) != objectid)
				break;
		}
		search = btrfs_node_blockptr(node, nr);
		if (path->reada == READA_FORWARD_ALWAYS ||
		    (search <= target && target - search <= 65536) ||
		    (search > target && search - target <= 65536)) {
			btrfs_readahead_node_child(node, nr);
			nread += blocksize;
		}
		nscan++;
		if (nread > nread_max || nscan > 32)
			break;
	}
}

static noinline void reada_for_balance(struct btrfs_path *path, int level)
{
	struct extent_buffer *parent;
	int slot;
	int nritems;

	parent = path->nodes[level + 1];
	if (!parent)
		return;

	nritems = btrfs_header_nritems(parent);
	slot = path->slots[level + 1];

	if (slot > 0)
		btrfs_readahead_node_child(parent, slot - 1);
	if (slot + 1 < nritems)
		btrfs_readahead_node_child(parent, slot + 1);
}


 
static noinline void unlock_up(struct btrfs_path *path, int level,
			       int lowest_unlock, int min_write_lock_level,
			       int *write_lock_level)
{
	int i;
	int skip_level = level;
	bool check_skip = true;

	for (i = level; i < BTRFS_MAX_LEVEL; i++) {
		if (!path->nodes[i])
			break;
		if (!path->locks[i])
			break;

		if (check_skip) {
			if (path->slots[i] == 0) {
				skip_level = i + 1;
				continue;
			}

			if (path->keep_locks) {
				u32 nritems;

				nritems = btrfs_header_nritems(path->nodes[i]);
				if (nritems < 1 || path->slots[i] >= nritems - 1) {
					skip_level = i + 1;
					continue;
				}
			}
		}

		if (i >= lowest_unlock && i > skip_level) {
			check_skip = false;
			btrfs_tree_unlock_rw(path->nodes[i], path->locks[i]);
			path->locks[i] = 0;
			if (write_lock_level &&
			    i > min_write_lock_level &&
			    i <= *write_lock_level) {
				*write_lock_level = i - 1;
			}
		}
	}
}

 
static int
read_block_for_search(struct btrfs_root *root, struct btrfs_path *p,
		      struct extent_buffer **eb_ret, int level, int slot,
		      const struct btrfs_key *key)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_tree_parent_check check = { 0 };
	u64 blocknr;
	u64 gen;
	struct extent_buffer *tmp;
	int ret;
	int parent_level;
	bool unlock_up;

	unlock_up = ((level + 1 < BTRFS_MAX_LEVEL) && p->locks[level + 1]);
	blocknr = btrfs_node_blockptr(*eb_ret, slot);
	gen = btrfs_node_ptr_generation(*eb_ret, slot);
	parent_level = btrfs_header_level(*eb_ret);
	btrfs_node_key_to_cpu(*eb_ret, &check.first_key, slot);
	check.has_first_key = true;
	check.level = parent_level - 1;
	check.transid = gen;
	check.owner_root = root->root_key.objectid;

	 
	tmp = find_extent_buffer(fs_info, blocknr);
	if (tmp) {
		if (p->reada == READA_FORWARD_ALWAYS)
			reada_for_search(fs_info, p, level, slot, key->objectid);

		 
		if (btrfs_buffer_uptodate(tmp, gen, 1) > 0) {
			 
			if (btrfs_verify_level_key(tmp,
					parent_level - 1, &check.first_key, gen)) {
				free_extent_buffer(tmp);
				return -EUCLEAN;
			}
			*eb_ret = tmp;
			return 0;
		}

		if (p->nowait) {
			free_extent_buffer(tmp);
			return -EAGAIN;
		}

		if (unlock_up)
			btrfs_unlock_up_safe(p, level + 1);

		 
		ret = btrfs_read_extent_buffer(tmp, &check);
		if (ret) {
			free_extent_buffer(tmp);
			btrfs_release_path(p);
			return -EIO;
		}
		if (btrfs_check_eb_owner(tmp, root->root_key.objectid)) {
			free_extent_buffer(tmp);
			btrfs_release_path(p);
			return -EUCLEAN;
		}

		if (unlock_up)
			ret = -EAGAIN;

		goto out;
	} else if (p->nowait) {
		return -EAGAIN;
	}

	if (unlock_up) {
		btrfs_unlock_up_safe(p, level + 1);
		ret = -EAGAIN;
	} else {
		ret = 0;
	}

	if (p->reada != READA_NONE)
		reada_for_search(fs_info, p, level, slot, key->objectid);

	tmp = read_tree_block(fs_info, blocknr, &check);
	if (IS_ERR(tmp)) {
		btrfs_release_path(p);
		return PTR_ERR(tmp);
	}
	 
	if (!extent_buffer_uptodate(tmp))
		ret = -EIO;

out:
	if (ret == 0) {
		*eb_ret = tmp;
	} else {
		free_extent_buffer(tmp);
		btrfs_release_path(p);
	}

	return ret;
}

 
static int
setup_nodes_for_search(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root, struct btrfs_path *p,
		       struct extent_buffer *b, int level, int ins_len,
		       int *write_lock_level)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret = 0;

	if ((p->search_for_split || ins_len > 0) && btrfs_header_nritems(b) >=
	    BTRFS_NODEPTRS_PER_BLOCK(fs_info) - 3) {

		if (*write_lock_level < level + 1) {
			*write_lock_level = level + 1;
			btrfs_release_path(p);
			return -EAGAIN;
		}

		reada_for_balance(p, level);
		ret = split_node(trans, root, p, level);

		b = p->nodes[level];
	} else if (ins_len < 0 && btrfs_header_nritems(b) <
		   BTRFS_NODEPTRS_PER_BLOCK(fs_info) / 2) {

		if (*write_lock_level < level + 1) {
			*write_lock_level = level + 1;
			btrfs_release_path(p);
			return -EAGAIN;
		}

		reada_for_balance(p, level);
		ret = balance_level(trans, root, p, level);
		if (ret)
			return ret;

		b = p->nodes[level];
		if (!b) {
			btrfs_release_path(p);
			return -EAGAIN;
		}
		BUG_ON(btrfs_header_nritems(b) == 1);
	}
	return ret;
}

int btrfs_find_item(struct btrfs_root *fs_root, struct btrfs_path *path,
		u64 iobjectid, u64 ioff, u8 key_type,
		struct btrfs_key *found_key)
{
	int ret;
	struct btrfs_key key;
	struct extent_buffer *eb;

	ASSERT(path);
	ASSERT(found_key);

	key.type = key_type;
	key.objectid = iobjectid;
	key.offset = ioff;

	ret = btrfs_search_slot(NULL, fs_root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	eb = path->nodes[0];
	if (ret && path->slots[0] >= btrfs_header_nritems(eb)) {
		ret = btrfs_next_leaf(fs_root, path);
		if (ret)
			return ret;
		eb = path->nodes[0];
	}

	btrfs_item_key_to_cpu(eb, found_key, path->slots[0]);
	if (found_key->type != key.type ||
			found_key->objectid != key.objectid)
		return 1;

	return 0;
}

static struct extent_buffer *btrfs_search_slot_get_root(struct btrfs_root *root,
							struct btrfs_path *p,
							int write_lock_level)
{
	struct extent_buffer *b;
	int root_lock = 0;
	int level = 0;

	if (p->search_commit_root) {
		b = root->commit_root;
		atomic_inc(&b->refs);
		level = btrfs_header_level(b);
		 
		ASSERT(p->skip_locking == 1);

		goto out;
	}

	if (p->skip_locking) {
		b = btrfs_root_node(root);
		level = btrfs_header_level(b);
		goto out;
	}

	 
	root_lock = BTRFS_READ_LOCK;

	 
	if (write_lock_level < BTRFS_MAX_LEVEL) {
		 
		if (p->nowait) {
			b = btrfs_try_read_lock_root_node(root);
			if (IS_ERR(b))
				return b;
		} else {
			b = btrfs_read_lock_root_node(root);
		}
		level = btrfs_header_level(b);
		if (level > write_lock_level)
			goto out;

		 
		btrfs_tree_read_unlock(b);
		free_extent_buffer(b);
	}

	b = btrfs_lock_root_node(root);
	root_lock = BTRFS_WRITE_LOCK;

	 
	level = btrfs_header_level(b);

out:
	 
	if (!extent_buffer_uptodate(b)) {
		if (root_lock)
			btrfs_tree_unlock_rw(b, root_lock);
		free_extent_buffer(b);
		return ERR_PTR(-EIO);
	}

	p->nodes[level] = b;
	if (!p->skip_locking)
		p->locks[level] = root_lock;
	 
	return b;
}

 
static int finish_need_commit_sem_search(struct btrfs_path *path)
{
	const int i = path->lowest_level;
	const int slot = path->slots[i];
	struct extent_buffer *lowest = path->nodes[i];
	struct extent_buffer *clone;

	ASSERT(path->need_commit_sem);

	if (!lowest)
		return 0;

	lockdep_assert_held_read(&lowest->fs_info->commit_root_sem);

	clone = btrfs_clone_extent_buffer(lowest);
	if (!clone)
		return -ENOMEM;

	btrfs_release_path(path);
	path->nodes[i] = clone;
	path->slots[i] = slot;

	return 0;
}

static inline int search_for_key_slot(struct extent_buffer *eb,
				      int search_low_slot,
				      const struct btrfs_key *key,
				      int prev_cmp,
				      int *slot)
{
	 
	if (prev_cmp == 0) {
		*slot = 0;
		return 0;
	}

	return btrfs_bin_search(eb, search_low_slot, key, slot);
}

static int search_leaf(struct btrfs_trans_handle *trans,
		       struct btrfs_root *root,
		       const struct btrfs_key *key,
		       struct btrfs_path *path,
		       int ins_len,
		       int prev_cmp)
{
	struct extent_buffer *leaf = path->nodes[0];
	int leaf_free_space = -1;
	int search_low_slot = 0;
	int ret;
	bool do_bin_search = true;

	 
	if (ins_len > 0) {
		 
		leaf_free_space = btrfs_leaf_free_space(leaf);

		 
		if (path->locks[1] && leaf_free_space >= ins_len) {
			struct btrfs_disk_key first_key;

			ASSERT(btrfs_header_nritems(leaf) > 0);
			btrfs_item_key(leaf, &first_key, 0);

			 
			ret = comp_keys(&first_key, key);
			if (ret < 0) {
				 
				btrfs_unlock_up_safe(path, 1);
				search_low_slot = 1;
			} else {
				 
				if (ret == 0)
					btrfs_unlock_up_safe(path, 1);
				 
				do_bin_search = false;
				path->slots[0] = 0;
			}
		}
	}

	if (do_bin_search) {
		ret = search_for_key_slot(leaf, search_low_slot, key,
					  prev_cmp, &path->slots[0]);
		if (ret < 0)
			return ret;
	}

	if (ins_len > 0) {
		 
		if (ret == 0 && !path->search_for_extension) {
			ASSERT(ins_len >= sizeof(struct btrfs_item));
			ins_len -= sizeof(struct btrfs_item);
		}

		ASSERT(leaf_free_space >= 0);

		if (leaf_free_space < ins_len) {
			int err;

			err = split_leaf(trans, root, key, path, ins_len,
					 (ret == 0));
			ASSERT(err <= 0);
			if (WARN_ON(err > 0))
				err = -EUCLEAN;
			if (err)
				ret = err;
		}
	}

	return ret;
}

 
int btrfs_search_slot(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      const struct btrfs_key *key, struct btrfs_path *p,
		      int ins_len, int cow)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *b;
	int slot;
	int ret;
	int err;
	int level;
	int lowest_unlock = 1;
	 
	int write_lock_level = 0;
	u8 lowest_level = 0;
	int min_write_lock_level;
	int prev_cmp;

	might_sleep();

	lowest_level = p->lowest_level;
	WARN_ON(lowest_level && ins_len > 0);
	WARN_ON(p->nodes[0] != NULL);
	BUG_ON(!cow && ins_len);

	 
	ASSERT(!p->nowait || !cow);

	if (ins_len < 0) {
		lowest_unlock = 2;

		 
		write_lock_level = 2;
	} else if (ins_len > 0) {
		 
		write_lock_level = 1;
	}

	if (!cow)
		write_lock_level = -1;

	if (cow && (p->keep_locks || p->lowest_level))
		write_lock_level = BTRFS_MAX_LEVEL;

	min_write_lock_level = write_lock_level;

	if (p->need_commit_sem) {
		ASSERT(p->search_commit_root);
		if (p->nowait) {
			if (!down_read_trylock(&fs_info->commit_root_sem))
				return -EAGAIN;
		} else {
			down_read(&fs_info->commit_root_sem);
		}
	}

again:
	prev_cmp = -1;
	b = btrfs_search_slot_get_root(root, p, write_lock_level);
	if (IS_ERR(b)) {
		ret = PTR_ERR(b);
		goto done;
	}

	while (b) {
		int dec = 0;

		level = btrfs_header_level(b);

		if (cow) {
			bool last_level = (level == (BTRFS_MAX_LEVEL - 1));

			 
			if (!should_cow_block(trans, root, b))
				goto cow_done;

			 
			if (level > write_lock_level ||
			    (level + 1 > write_lock_level &&
			    level + 1 < BTRFS_MAX_LEVEL &&
			    p->nodes[level + 1])) {
				write_lock_level = level + 1;
				btrfs_release_path(p);
				goto again;
			}

			if (last_level)
				err = btrfs_cow_block(trans, root, b, NULL, 0,
						      &b,
						      BTRFS_NESTING_COW);
			else
				err = btrfs_cow_block(trans, root, b,
						      p->nodes[level + 1],
						      p->slots[level + 1], &b,
						      BTRFS_NESTING_COW);
			if (err) {
				ret = err;
				goto done;
			}
		}
cow_done:
		p->nodes[level] = b;

		 
		if (!ins_len && !p->keep_locks) {
			int u = level + 1;

			if (u < BTRFS_MAX_LEVEL && p->locks[u]) {
				btrfs_tree_unlock_rw(p->nodes[u], p->locks[u]);
				p->locks[u] = 0;
			}
		}

		if (level == 0) {
			if (ins_len > 0)
				ASSERT(write_lock_level >= 1);

			ret = search_leaf(trans, root, key, p, ins_len, prev_cmp);
			if (!p->search_for_split)
				unlock_up(p, level, lowest_unlock,
					  min_write_lock_level, NULL);
			goto done;
		}

		ret = search_for_key_slot(b, 0, key, prev_cmp, &slot);
		if (ret < 0)
			goto done;
		prev_cmp = ret;

		if (ret && slot > 0) {
			dec = 1;
			slot--;
		}
		p->slots[level] = slot;
		err = setup_nodes_for_search(trans, root, p, b, level, ins_len,
					     &write_lock_level);
		if (err == -EAGAIN)
			goto again;
		if (err) {
			ret = err;
			goto done;
		}
		b = p->nodes[level];
		slot = p->slots[level];

		 
		if (slot == 0 && ins_len && write_lock_level < level + 1) {
			write_lock_level = level + 1;
			btrfs_release_path(p);
			goto again;
		}

		unlock_up(p, level, lowest_unlock, min_write_lock_level,
			  &write_lock_level);

		if (level == lowest_level) {
			if (dec)
				p->slots[level]++;
			goto done;
		}

		err = read_block_for_search(root, p, &b, level, slot, key);
		if (err == -EAGAIN)
			goto again;
		if (err) {
			ret = err;
			goto done;
		}

		if (!p->skip_locking) {
			level = btrfs_header_level(b);

			btrfs_maybe_reset_lockdep_class(root, b);

			if (level <= write_lock_level) {
				btrfs_tree_lock(b);
				p->locks[level] = BTRFS_WRITE_LOCK;
			} else {
				if (p->nowait) {
					if (!btrfs_try_tree_read_lock(b)) {
						free_extent_buffer(b);
						ret = -EAGAIN;
						goto done;
					}
				} else {
					btrfs_tree_read_lock(b);
				}
				p->locks[level] = BTRFS_READ_LOCK;
			}
			p->nodes[level] = b;
		}
	}
	ret = 1;
done:
	if (ret < 0 && !p->skip_release_on_error)
		btrfs_release_path(p);

	if (p->need_commit_sem) {
		int ret2;

		ret2 = finish_need_commit_sem_search(p);
		up_read(&fs_info->commit_root_sem);
		if (ret2)
			ret = ret2;
	}

	return ret;
}
ALLOW_ERROR_INJECTION(btrfs_search_slot, ERRNO);

 
int btrfs_search_old_slot(struct btrfs_root *root, const struct btrfs_key *key,
			  struct btrfs_path *p, u64 time_seq)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *b;
	int slot;
	int ret;
	int err;
	int level;
	int lowest_unlock = 1;
	u8 lowest_level = 0;

	lowest_level = p->lowest_level;
	WARN_ON(p->nodes[0] != NULL);
	ASSERT(!p->nowait);

	if (p->search_commit_root) {
		BUG_ON(time_seq);
		return btrfs_search_slot(NULL, root, key, p, 0, 0);
	}

again:
	b = btrfs_get_old_root(root, time_seq);
	if (!b) {
		ret = -EIO;
		goto done;
	}
	level = btrfs_header_level(b);
	p->locks[level] = BTRFS_READ_LOCK;

	while (b) {
		int dec = 0;

		level = btrfs_header_level(b);
		p->nodes[level] = b;

		 
		btrfs_unlock_up_safe(p, level + 1);

		ret = btrfs_bin_search(b, 0, key, &slot);
		if (ret < 0)
			goto done;

		if (level == 0) {
			p->slots[level] = slot;
			unlock_up(p, level, lowest_unlock, 0, NULL);
			goto done;
		}

		if (ret && slot > 0) {
			dec = 1;
			slot--;
		}
		p->slots[level] = slot;
		unlock_up(p, level, lowest_unlock, 0, NULL);

		if (level == lowest_level) {
			if (dec)
				p->slots[level]++;
			goto done;
		}

		err = read_block_for_search(root, p, &b, level, slot, key);
		if (err == -EAGAIN)
			goto again;
		if (err) {
			ret = err;
			goto done;
		}

		level = btrfs_header_level(b);
		btrfs_tree_read_lock(b);
		b = btrfs_tree_mod_log_rewind(fs_info, p, b, time_seq);
		if (!b) {
			ret = -ENOMEM;
			goto done;
		}
		p->locks[level] = BTRFS_READ_LOCK;
		p->nodes[level] = b;
	}
	ret = 1;
done:
	if (ret < 0)
		btrfs_release_path(p);

	return ret;
}

 
static int btrfs_prev_leaf(struct btrfs_root *root, struct btrfs_path *path)
{
	struct btrfs_key key;
	struct btrfs_key orig_key;
	struct btrfs_disk_key found_key;
	int ret;

	btrfs_item_key_to_cpu(path->nodes[0], &key, 0);
	orig_key = key;

	if (key.offset > 0) {
		key.offset--;
	} else if (key.type > 0) {
		key.type--;
		key.offset = (u64)-1;
	} else if (key.objectid > 0) {
		key.objectid--;
		key.type = (u8)-1;
		key.offset = (u64)-1;
	} else {
		return 1;
	}

	btrfs_release_path(path);
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret <= 0)
		return ret;

	 
	if (path->slots[0] < btrfs_header_nritems(path->nodes[0])) {
		btrfs_item_key(path->nodes[0], &found_key, path->slots[0]);
		ret = comp_keys(&found_key, &orig_key);
		if (ret == 0) {
			if (path->slots[0] > 0) {
				path->slots[0]--;
				return 0;
			}
			 
			return 1;
		}
	}

	btrfs_item_key(path->nodes[0], &found_key, 0);
	ret = comp_keys(&found_key, &key);
	 
	if (ret <= 0)
		return 0;
	return 1;
}

 
int btrfs_search_slot_for_read(struct btrfs_root *root,
			       const struct btrfs_key *key,
			       struct btrfs_path *p, int find_higher,
			       int return_any)
{
	int ret;
	struct extent_buffer *leaf;

again:
	ret = btrfs_search_slot(NULL, root, key, p, 0, 0);
	if (ret <= 0)
		return ret;
	 
	leaf = p->nodes[0];

	if (find_higher) {
		if (p->slots[0] >= btrfs_header_nritems(leaf)) {
			ret = btrfs_next_leaf(root, p);
			if (ret <= 0)
				return ret;
			if (!return_any)
				return 1;
			 
			return_any = 0;
			find_higher = 0;
			btrfs_release_path(p);
			goto again;
		}
	} else {
		if (p->slots[0] == 0) {
			ret = btrfs_prev_leaf(root, p);
			if (ret < 0)
				return ret;
			if (!ret) {
				leaf = p->nodes[0];
				if (p->slots[0] == btrfs_header_nritems(leaf))
					p->slots[0]--;
				return 0;
			}
			if (!return_any)
				return 1;
			 
			return_any = 0;
			find_higher = 1;
			btrfs_release_path(p);
			goto again;
		} else {
			--p->slots[0];
		}
	}
	return 0;
}

 
int btrfs_search_backwards(struct btrfs_root *root, struct btrfs_key *key,
			   struct btrfs_path *path)
{
	int ret;

	ret = btrfs_search_slot(NULL, root, key, path, 0, 0);
	if (ret > 0)
		ret = btrfs_previous_item(root, path, key->objectid, key->type);

	if (ret == 0)
		btrfs_item_key_to_cpu(path->nodes[0], key, path->slots[0]);

	return ret;
}

 
int btrfs_get_next_valid_item(struct btrfs_root *root, struct btrfs_key *key,
			      struct btrfs_path *path)
{
	if (path->slots[0] >= btrfs_header_nritems(path->nodes[0])) {
		int ret;

		ret = btrfs_next_leaf(root, path);
		if (ret)
			return ret;
	}

	btrfs_item_key_to_cpu(path->nodes[0], key, path->slots[0]);
	return 0;
}

 
static void fixup_low_keys(struct btrfs_trans_handle *trans,
			   struct btrfs_path *path,
			   struct btrfs_disk_key *key, int level)
{
	int i;
	struct extent_buffer *t;
	int ret;

	for (i = level; i < BTRFS_MAX_LEVEL; i++) {
		int tslot = path->slots[i];

		if (!path->nodes[i])
			break;
		t = path->nodes[i];
		ret = btrfs_tree_mod_log_insert_key(t, tslot,
						    BTRFS_MOD_LOG_KEY_REPLACE);
		BUG_ON(ret < 0);
		btrfs_set_node_key(t, key, tslot);
		btrfs_mark_buffer_dirty(trans, path->nodes[i]);
		if (tslot != 0)
			break;
	}
}

 
void btrfs_set_item_key_safe(struct btrfs_trans_handle *trans,
			     struct btrfs_path *path,
			     const struct btrfs_key *new_key)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	struct btrfs_disk_key disk_key;
	struct extent_buffer *eb;
	int slot;

	eb = path->nodes[0];
	slot = path->slots[0];
	if (slot > 0) {
		btrfs_item_key(eb, &disk_key, slot - 1);
		if (unlikely(comp_keys(&disk_key, new_key) >= 0)) {
			btrfs_print_leaf(eb);
			btrfs_crit(fs_info,
		"slot %u key (%llu %u %llu) new key (%llu %u %llu)",
				   slot, btrfs_disk_key_objectid(&disk_key),
				   btrfs_disk_key_type(&disk_key),
				   btrfs_disk_key_offset(&disk_key),
				   new_key->objectid, new_key->type,
				   new_key->offset);
			BUG();
		}
	}
	if (slot < btrfs_header_nritems(eb) - 1) {
		btrfs_item_key(eb, &disk_key, slot + 1);
		if (unlikely(comp_keys(&disk_key, new_key) <= 0)) {
			btrfs_print_leaf(eb);
			btrfs_crit(fs_info,
		"slot %u key (%llu %u %llu) new key (%llu %u %llu)",
				   slot, btrfs_disk_key_objectid(&disk_key),
				   btrfs_disk_key_type(&disk_key),
				   btrfs_disk_key_offset(&disk_key),
				   new_key->objectid, new_key->type,
				   new_key->offset);
			BUG();
		}
	}

	btrfs_cpu_key_to_disk(&disk_key, new_key);
	btrfs_set_item_key(eb, &disk_key, slot);
	btrfs_mark_buffer_dirty(trans, eb);
	if (slot == 0)
		fixup_low_keys(trans, path, &disk_key, 1);
}

 
static bool check_sibling_keys(struct extent_buffer *left,
			       struct extent_buffer *right)
{
	struct btrfs_key left_last;
	struct btrfs_key right_first;
	int level = btrfs_header_level(left);
	int nr_left = btrfs_header_nritems(left);
	int nr_right = btrfs_header_nritems(right);

	 
	if (!nr_left || !nr_right)
		return false;

	if (level) {
		btrfs_node_key_to_cpu(left, &left_last, nr_left - 1);
		btrfs_node_key_to_cpu(right, &right_first, 0);
	} else {
		btrfs_item_key_to_cpu(left, &left_last, nr_left - 1);
		btrfs_item_key_to_cpu(right, &right_first, 0);
	}

	if (unlikely(btrfs_comp_cpu_keys(&left_last, &right_first) >= 0)) {
		btrfs_crit(left->fs_info, "left extent buffer:");
		btrfs_print_tree(left, false);
		btrfs_crit(left->fs_info, "right extent buffer:");
		btrfs_print_tree(right, false);
		btrfs_crit(left->fs_info,
"bad key order, sibling blocks, left last (%llu %u %llu) right first (%llu %u %llu)",
			   left_last.objectid, left_last.type,
			   left_last.offset, right_first.objectid,
			   right_first.type, right_first.offset);
		return true;
	}
	return false;
}

 
static int push_node_left(struct btrfs_trans_handle *trans,
			  struct extent_buffer *dst,
			  struct extent_buffer *src, int empty)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int push_items = 0;
	int src_nritems;
	int dst_nritems;
	int ret = 0;

	src_nritems = btrfs_header_nritems(src);
	dst_nritems = btrfs_header_nritems(dst);
	push_items = BTRFS_NODEPTRS_PER_BLOCK(fs_info) - dst_nritems;
	WARN_ON(btrfs_header_generation(src) != trans->transid);
	WARN_ON(btrfs_header_generation(dst) != trans->transid);

	if (!empty && src_nritems <= 8)
		return 1;

	if (push_items <= 0)
		return 1;

	if (empty) {
		push_items = min(src_nritems, push_items);
		if (push_items < src_nritems) {
			 
			if (src_nritems - push_items < 8) {
				if (push_items <= 8)
					return 1;
				push_items -= 8;
			}
		}
	} else
		push_items = min(src_nritems - 8, push_items);

	 
	if (check_sibling_keys(dst, src)) {
		ret = -EUCLEAN;
		btrfs_abort_transaction(trans, ret);
		return ret;
	}
	ret = btrfs_tree_mod_log_eb_copy(dst, src, dst_nritems, 0, push_items);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		return ret;
	}
	copy_extent_buffer(dst, src,
			   btrfs_node_key_ptr_offset(dst, dst_nritems),
			   btrfs_node_key_ptr_offset(src, 0),
			   push_items * sizeof(struct btrfs_key_ptr));

	if (push_items < src_nritems) {
		 
		memmove_extent_buffer(src, btrfs_node_key_ptr_offset(src, 0),
				      btrfs_node_key_ptr_offset(src, push_items),
				      (src_nritems - push_items) *
				      sizeof(struct btrfs_key_ptr));
	}
	btrfs_set_header_nritems(src, src_nritems - push_items);
	btrfs_set_header_nritems(dst, dst_nritems + push_items);
	btrfs_mark_buffer_dirty(trans, src);
	btrfs_mark_buffer_dirty(trans, dst);

	return ret;
}

 
static int balance_node_right(struct btrfs_trans_handle *trans,
			      struct extent_buffer *dst,
			      struct extent_buffer *src)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int push_items = 0;
	int max_push;
	int src_nritems;
	int dst_nritems;
	int ret = 0;

	WARN_ON(btrfs_header_generation(src) != trans->transid);
	WARN_ON(btrfs_header_generation(dst) != trans->transid);

	src_nritems = btrfs_header_nritems(src);
	dst_nritems = btrfs_header_nritems(dst);
	push_items = BTRFS_NODEPTRS_PER_BLOCK(fs_info) - dst_nritems;
	if (push_items <= 0)
		return 1;

	if (src_nritems < 4)
		return 1;

	max_push = src_nritems / 2 + 1;
	 
	if (max_push >= src_nritems)
		return 1;

	if (max_push < push_items)
		push_items = max_push;

	 
	if (check_sibling_keys(src, dst)) {
		ret = -EUCLEAN;
		btrfs_abort_transaction(trans, ret);
		return ret;
	}

	 
	memmove_extent_buffer(dst, btrfs_node_key_ptr_offset(dst, push_items),
				      btrfs_node_key_ptr_offset(dst, 0),
				      (dst_nritems) *
				      sizeof(struct btrfs_key_ptr));

	ret = btrfs_tree_mod_log_eb_copy(dst, src, 0, src_nritems - push_items,
					 push_items);
	if (ret) {
		btrfs_abort_transaction(trans, ret);
		return ret;
	}
	copy_extent_buffer(dst, src,
			   btrfs_node_key_ptr_offset(dst, 0),
			   btrfs_node_key_ptr_offset(src, src_nritems - push_items),
			   push_items * sizeof(struct btrfs_key_ptr));

	btrfs_set_header_nritems(src, src_nritems - push_items);
	btrfs_set_header_nritems(dst, dst_nritems + push_items);

	btrfs_mark_buffer_dirty(trans, src);
	btrfs_mark_buffer_dirty(trans, dst);

	return ret;
}

 
static noinline int insert_new_root(struct btrfs_trans_handle *trans,
			   struct btrfs_root *root,
			   struct btrfs_path *path, int level)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 lower_gen;
	struct extent_buffer *lower;
	struct extent_buffer *c;
	struct extent_buffer *old;
	struct btrfs_disk_key lower_key;
	int ret;

	BUG_ON(path->nodes[level]);
	BUG_ON(path->nodes[level-1] != root->node);

	lower = path->nodes[level-1];
	if (level == 1)
		btrfs_item_key(lower, &lower_key, 0);
	else
		btrfs_node_key(lower, &lower_key, 0);

	c = btrfs_alloc_tree_block(trans, root, 0, root->root_key.objectid,
				   &lower_key, level, root->node->start, 0,
				   BTRFS_NESTING_NEW_ROOT);
	if (IS_ERR(c))
		return PTR_ERR(c);

	root_add_used(root, fs_info->nodesize);

	btrfs_set_header_nritems(c, 1);
	btrfs_set_node_key(c, &lower_key, 0);
	btrfs_set_node_blockptr(c, 0, lower->start);
	lower_gen = btrfs_header_generation(lower);
	WARN_ON(lower_gen != trans->transid);

	btrfs_set_node_ptr_generation(c, 0, lower_gen);

	btrfs_mark_buffer_dirty(trans, c);

	old = root->node;
	ret = btrfs_tree_mod_log_insert_root(root->node, c, false);
	if (ret < 0) {
		btrfs_free_tree_block(trans, btrfs_root_id(root), c, 0, 1);
		btrfs_tree_unlock(c);
		free_extent_buffer(c);
		return ret;
	}
	rcu_assign_pointer(root->node, c);

	 
	free_extent_buffer(old);

	add_root_to_dirty_list(root);
	atomic_inc(&c->refs);
	path->nodes[level] = c;
	path->locks[level] = BTRFS_WRITE_LOCK;
	path->slots[level] = 0;
	return 0;
}

 
static int insert_ptr(struct btrfs_trans_handle *trans,
		      struct btrfs_path *path,
		      struct btrfs_disk_key *key, u64 bytenr,
		      int slot, int level)
{
	struct extent_buffer *lower;
	int nritems;
	int ret;

	BUG_ON(!path->nodes[level]);
	btrfs_assert_tree_write_locked(path->nodes[level]);
	lower = path->nodes[level];
	nritems = btrfs_header_nritems(lower);
	BUG_ON(slot > nritems);
	BUG_ON(nritems == BTRFS_NODEPTRS_PER_BLOCK(trans->fs_info));
	if (slot != nritems) {
		if (level) {
			ret = btrfs_tree_mod_log_insert_move(lower, slot + 1,
					slot, nritems - slot);
			if (ret < 0) {
				btrfs_abort_transaction(trans, ret);
				return ret;
			}
		}
		memmove_extent_buffer(lower,
			      btrfs_node_key_ptr_offset(lower, slot + 1),
			      btrfs_node_key_ptr_offset(lower, slot),
			      (nritems - slot) * sizeof(struct btrfs_key_ptr));
	}
	if (level) {
		ret = btrfs_tree_mod_log_insert_key(lower, slot,
						    BTRFS_MOD_LOG_KEY_ADD);
		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
	}
	btrfs_set_node_key(lower, key, slot);
	btrfs_set_node_blockptr(lower, slot, bytenr);
	WARN_ON(trans->transid == 0);
	btrfs_set_node_ptr_generation(lower, slot, trans->transid);
	btrfs_set_header_nritems(lower, nritems + 1);
	btrfs_mark_buffer_dirty(trans, lower);

	return 0;
}

 
static noinline int split_node(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       struct btrfs_path *path, int level)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *c;
	struct extent_buffer *split;
	struct btrfs_disk_key disk_key;
	int mid;
	int ret;
	u32 c_nritems;

	c = path->nodes[level];
	WARN_ON(btrfs_header_generation(c) != trans->transid);
	if (c == root->node) {
		 
		ret = insert_new_root(trans, root, path, level + 1);
		if (ret)
			return ret;
	} else {
		ret = push_nodes_for_insert(trans, root, path, level);
		c = path->nodes[level];
		if (!ret && btrfs_header_nritems(c) <
		    BTRFS_NODEPTRS_PER_BLOCK(fs_info) - 3)
			return 0;
		if (ret < 0)
			return ret;
	}

	c_nritems = btrfs_header_nritems(c);
	mid = (c_nritems + 1) / 2;
	btrfs_node_key(c, &disk_key, mid);

	split = btrfs_alloc_tree_block(trans, root, 0, root->root_key.objectid,
				       &disk_key, level, c->start, 0,
				       BTRFS_NESTING_SPLIT);
	if (IS_ERR(split))
		return PTR_ERR(split);

	root_add_used(root, fs_info->nodesize);
	ASSERT(btrfs_header_level(c) == level);

	ret = btrfs_tree_mod_log_eb_copy(split, c, 0, mid, c_nritems - mid);
	if (ret) {
		btrfs_tree_unlock(split);
		free_extent_buffer(split);
		btrfs_abort_transaction(trans, ret);
		return ret;
	}
	copy_extent_buffer(split, c,
			   btrfs_node_key_ptr_offset(split, 0),
			   btrfs_node_key_ptr_offset(c, mid),
			   (c_nritems - mid) * sizeof(struct btrfs_key_ptr));
	btrfs_set_header_nritems(split, c_nritems - mid);
	btrfs_set_header_nritems(c, mid);

	btrfs_mark_buffer_dirty(trans, c);
	btrfs_mark_buffer_dirty(trans, split);

	ret = insert_ptr(trans, path, &disk_key, split->start,
			 path->slots[level + 1] + 1, level + 1);
	if (ret < 0) {
		btrfs_tree_unlock(split);
		free_extent_buffer(split);
		return ret;
	}

	if (path->slots[level] >= mid) {
		path->slots[level] -= mid;
		btrfs_tree_unlock(c);
		free_extent_buffer(c);
		path->nodes[level] = split;
		path->slots[level + 1] += 1;
	} else {
		btrfs_tree_unlock(split);
		free_extent_buffer(split);
	}
	return 0;
}

 
static int leaf_space_used(const struct extent_buffer *l, int start, int nr)
{
	int data_len;
	int nritems = btrfs_header_nritems(l);
	int end = min(nritems, start + nr) - 1;

	if (!nr)
		return 0;
	data_len = btrfs_item_offset(l, start) + btrfs_item_size(l, start);
	data_len = data_len - btrfs_item_offset(l, end);
	data_len += sizeof(struct btrfs_item) * nr;
	WARN_ON(data_len < 0);
	return data_len;
}

 
int btrfs_leaf_free_space(const struct extent_buffer *leaf)
{
	struct btrfs_fs_info *fs_info = leaf->fs_info;
	int nritems = btrfs_header_nritems(leaf);
	int ret;

	ret = BTRFS_LEAF_DATA_SIZE(fs_info) - leaf_space_used(leaf, 0, nritems);
	if (ret < 0) {
		btrfs_crit(fs_info,
			   "leaf free space ret %d, leaf data size %lu, used %d nritems %d",
			   ret,
			   (unsigned long) BTRFS_LEAF_DATA_SIZE(fs_info),
			   leaf_space_used(leaf, 0, nritems), nritems);
	}
	return ret;
}

 
static noinline int __push_leaf_right(struct btrfs_trans_handle *trans,
				      struct btrfs_path *path,
				      int data_size, int empty,
				      struct extent_buffer *right,
				      int free_space, u32 left_nritems,
				      u32 min_slot)
{
	struct btrfs_fs_info *fs_info = right->fs_info;
	struct extent_buffer *left = path->nodes[0];
	struct extent_buffer *upper = path->nodes[1];
	struct btrfs_map_token token;
	struct btrfs_disk_key disk_key;
	int slot;
	u32 i;
	int push_space = 0;
	int push_items = 0;
	u32 nr;
	u32 right_nritems;
	u32 data_end;
	u32 this_item_size;

	if (empty)
		nr = 0;
	else
		nr = max_t(u32, 1, min_slot);

	if (path->slots[0] >= left_nritems)
		push_space += data_size;

	slot = path->slots[1];
	i = left_nritems - 1;
	while (i >= nr) {
		if (!empty && push_items > 0) {
			if (path->slots[0] > i)
				break;
			if (path->slots[0] == i) {
				int space = btrfs_leaf_free_space(left);

				if (space + push_space * 2 > free_space)
					break;
			}
		}

		if (path->slots[0] == i)
			push_space += data_size;

		this_item_size = btrfs_item_size(left, i);
		if (this_item_size + sizeof(struct btrfs_item) +
		    push_space > free_space)
			break;

		push_items++;
		push_space += this_item_size + sizeof(struct btrfs_item);
		if (i == 0)
			break;
		i--;
	}

	if (push_items == 0)
		goto out_unlock;

	WARN_ON(!empty && push_items == left_nritems);

	 
	right_nritems = btrfs_header_nritems(right);

	push_space = btrfs_item_data_end(left, left_nritems - push_items);
	push_space -= leaf_data_end(left);

	 
	data_end = leaf_data_end(right);
	memmove_leaf_data(right, data_end - push_space, data_end,
			  BTRFS_LEAF_DATA_SIZE(fs_info) - data_end);

	 
	copy_leaf_data(right, left, BTRFS_LEAF_DATA_SIZE(fs_info) - push_space,
		       leaf_data_end(left), push_space);

	memmove_leaf_items(right, push_items, 0, right_nritems);

	 
	copy_leaf_items(right, left, 0, left_nritems - push_items, push_items);

	 
	btrfs_init_map_token(&token, right);
	right_nritems += push_items;
	btrfs_set_header_nritems(right, right_nritems);
	push_space = BTRFS_LEAF_DATA_SIZE(fs_info);
	for (i = 0; i < right_nritems; i++) {
		push_space -= btrfs_token_item_size(&token, i);
		btrfs_set_token_item_offset(&token, i, push_space);
	}

	left_nritems -= push_items;
	btrfs_set_header_nritems(left, left_nritems);

	if (left_nritems)
		btrfs_mark_buffer_dirty(trans, left);
	else
		btrfs_clear_buffer_dirty(trans, left);

	btrfs_mark_buffer_dirty(trans, right);

	btrfs_item_key(right, &disk_key, 0);
	btrfs_set_node_key(upper, &disk_key, slot + 1);
	btrfs_mark_buffer_dirty(trans, upper);

	 
	if (path->slots[0] >= left_nritems) {
		path->slots[0] -= left_nritems;
		if (btrfs_header_nritems(path->nodes[0]) == 0)
			btrfs_clear_buffer_dirty(trans, path->nodes[0]);
		btrfs_tree_unlock(path->nodes[0]);
		free_extent_buffer(path->nodes[0]);
		path->nodes[0] = right;
		path->slots[1] += 1;
	} else {
		btrfs_tree_unlock(right);
		free_extent_buffer(right);
	}
	return 0;

out_unlock:
	btrfs_tree_unlock(right);
	free_extent_buffer(right);
	return 1;
}

 
static int push_leaf_right(struct btrfs_trans_handle *trans, struct btrfs_root
			   *root, struct btrfs_path *path,
			   int min_data_size, int data_size,
			   int empty, u32 min_slot)
{
	struct extent_buffer *left = path->nodes[0];
	struct extent_buffer *right;
	struct extent_buffer *upper;
	int slot;
	int free_space;
	u32 left_nritems;
	int ret;

	if (!path->nodes[1])
		return 1;

	slot = path->slots[1];
	upper = path->nodes[1];
	if (slot >= btrfs_header_nritems(upper) - 1)
		return 1;

	btrfs_assert_tree_write_locked(path->nodes[1]);

	right = btrfs_read_node_slot(upper, slot + 1);
	if (IS_ERR(right))
		return PTR_ERR(right);

	__btrfs_tree_lock(right, BTRFS_NESTING_RIGHT);

	free_space = btrfs_leaf_free_space(right);
	if (free_space < data_size)
		goto out_unlock;

	ret = btrfs_cow_block(trans, root, right, upper,
			      slot + 1, &right, BTRFS_NESTING_RIGHT_COW);
	if (ret)
		goto out_unlock;

	left_nritems = btrfs_header_nritems(left);
	if (left_nritems == 0)
		goto out_unlock;

	if (check_sibling_keys(left, right)) {
		ret = -EUCLEAN;
		btrfs_abort_transaction(trans, ret);
		btrfs_tree_unlock(right);
		free_extent_buffer(right);
		return ret;
	}
	if (path->slots[0] == left_nritems && !empty) {
		 
		btrfs_tree_unlock(left);
		free_extent_buffer(left);
		path->nodes[0] = right;
		path->slots[0] = 0;
		path->slots[1]++;
		return 0;
	}

	return __push_leaf_right(trans, path, min_data_size, empty, right,
				 free_space, left_nritems, min_slot);
out_unlock:
	btrfs_tree_unlock(right);
	free_extent_buffer(right);
	return 1;
}

 
static noinline int __push_leaf_left(struct btrfs_trans_handle *trans,
				     struct btrfs_path *path, int data_size,
				     int empty, struct extent_buffer *left,
				     int free_space, u32 right_nritems,
				     u32 max_slot)
{
	struct btrfs_fs_info *fs_info = left->fs_info;
	struct btrfs_disk_key disk_key;
	struct extent_buffer *right = path->nodes[0];
	int i;
	int push_space = 0;
	int push_items = 0;
	u32 old_left_nritems;
	u32 nr;
	int ret = 0;
	u32 this_item_size;
	u32 old_left_item_size;
	struct btrfs_map_token token;

	if (empty)
		nr = min(right_nritems, max_slot);
	else
		nr = min(right_nritems - 1, max_slot);

	for (i = 0; i < nr; i++) {
		if (!empty && push_items > 0) {
			if (path->slots[0] < i)
				break;
			if (path->slots[0] == i) {
				int space = btrfs_leaf_free_space(right);

				if (space + push_space * 2 > free_space)
					break;
			}
		}

		if (path->slots[0] == i)
			push_space += data_size;

		this_item_size = btrfs_item_size(right, i);
		if (this_item_size + sizeof(struct btrfs_item) + push_space >
		    free_space)
			break;

		push_items++;
		push_space += this_item_size + sizeof(struct btrfs_item);
	}

	if (push_items == 0) {
		ret = 1;
		goto out;
	}
	WARN_ON(!empty && push_items == btrfs_header_nritems(right));

	 
	copy_leaf_items(left, right, btrfs_header_nritems(left), 0, push_items);

	push_space = BTRFS_LEAF_DATA_SIZE(fs_info) -
		     btrfs_item_offset(right, push_items - 1);

	copy_leaf_data(left, right, leaf_data_end(left) - push_space,
		       btrfs_item_offset(right, push_items - 1), push_space);
	old_left_nritems = btrfs_header_nritems(left);
	BUG_ON(old_left_nritems <= 0);

	btrfs_init_map_token(&token, left);
	old_left_item_size = btrfs_item_offset(left, old_left_nritems - 1);
	for (i = old_left_nritems; i < old_left_nritems + push_items; i++) {
		u32 ioff;

		ioff = btrfs_token_item_offset(&token, i);
		btrfs_set_token_item_offset(&token, i,
		      ioff - (BTRFS_LEAF_DATA_SIZE(fs_info) - old_left_item_size));
	}
	btrfs_set_header_nritems(left, old_left_nritems + push_items);

	 
	if (push_items > right_nritems)
		WARN(1, KERN_CRIT "push items %d nr %u\n", push_items,
		       right_nritems);

	if (push_items < right_nritems) {
		push_space = btrfs_item_offset(right, push_items - 1) -
						  leaf_data_end(right);
		memmove_leaf_data(right,
				  BTRFS_LEAF_DATA_SIZE(fs_info) - push_space,
				  leaf_data_end(right), push_space);

		memmove_leaf_items(right, 0, push_items,
				   btrfs_header_nritems(right) - push_items);
	}

	btrfs_init_map_token(&token, right);
	right_nritems -= push_items;
	btrfs_set_header_nritems(right, right_nritems);
	push_space = BTRFS_LEAF_DATA_SIZE(fs_info);
	for (i = 0; i < right_nritems; i++) {
		push_space = push_space - btrfs_token_item_size(&token, i);
		btrfs_set_token_item_offset(&token, i, push_space);
	}

	btrfs_mark_buffer_dirty(trans, left);
	if (right_nritems)
		btrfs_mark_buffer_dirty(trans, right);
	else
		btrfs_clear_buffer_dirty(trans, right);

	btrfs_item_key(right, &disk_key, 0);
	fixup_low_keys(trans, path, &disk_key, 1);

	 
	if (path->slots[0] < push_items) {
		path->slots[0] += old_left_nritems;
		btrfs_tree_unlock(path->nodes[0]);
		free_extent_buffer(path->nodes[0]);
		path->nodes[0] = left;
		path->slots[1] -= 1;
	} else {
		btrfs_tree_unlock(left);
		free_extent_buffer(left);
		path->slots[0] -= push_items;
	}
	BUG_ON(path->slots[0] < 0);
	return ret;
out:
	btrfs_tree_unlock(left);
	free_extent_buffer(left);
	return ret;
}

 
static int push_leaf_left(struct btrfs_trans_handle *trans, struct btrfs_root
			  *root, struct btrfs_path *path, int min_data_size,
			  int data_size, int empty, u32 max_slot)
{
	struct extent_buffer *right = path->nodes[0];
	struct extent_buffer *left;
	int slot;
	int free_space;
	u32 right_nritems;
	int ret = 0;

	slot = path->slots[1];
	if (slot == 0)
		return 1;
	if (!path->nodes[1])
		return 1;

	right_nritems = btrfs_header_nritems(right);
	if (right_nritems == 0)
		return 1;

	btrfs_assert_tree_write_locked(path->nodes[1]);

	left = btrfs_read_node_slot(path->nodes[1], slot - 1);
	if (IS_ERR(left))
		return PTR_ERR(left);

	__btrfs_tree_lock(left, BTRFS_NESTING_LEFT);

	free_space = btrfs_leaf_free_space(left);
	if (free_space < data_size) {
		ret = 1;
		goto out;
	}

	ret = btrfs_cow_block(trans, root, left,
			      path->nodes[1], slot - 1, &left,
			      BTRFS_NESTING_LEFT_COW);
	if (ret) {
		 
		if (ret == -ENOSPC)
			ret = 1;
		goto out;
	}

	if (check_sibling_keys(left, right)) {
		ret = -EUCLEAN;
		btrfs_abort_transaction(trans, ret);
		goto out;
	}
	return __push_leaf_left(trans, path, min_data_size, empty, left,
				free_space, right_nritems, max_slot);
out:
	btrfs_tree_unlock(left);
	free_extent_buffer(left);
	return ret;
}

 
static noinline int copy_for_split(struct btrfs_trans_handle *trans,
				   struct btrfs_path *path,
				   struct extent_buffer *l,
				   struct extent_buffer *right,
				   int slot, int mid, int nritems)
{
	struct btrfs_fs_info *fs_info = trans->fs_info;
	int data_copy_size;
	int rt_data_off;
	int i;
	int ret;
	struct btrfs_disk_key disk_key;
	struct btrfs_map_token token;

	nritems = nritems - mid;
	btrfs_set_header_nritems(right, nritems);
	data_copy_size = btrfs_item_data_end(l, mid) - leaf_data_end(l);

	copy_leaf_items(right, l, 0, mid, nritems);

	copy_leaf_data(right, l, BTRFS_LEAF_DATA_SIZE(fs_info) - data_copy_size,
		       leaf_data_end(l), data_copy_size);

	rt_data_off = BTRFS_LEAF_DATA_SIZE(fs_info) - btrfs_item_data_end(l, mid);

	btrfs_init_map_token(&token, right);
	for (i = 0; i < nritems; i++) {
		u32 ioff;

		ioff = btrfs_token_item_offset(&token, i);
		btrfs_set_token_item_offset(&token, i, ioff + rt_data_off);
	}

	btrfs_set_header_nritems(l, mid);
	btrfs_item_key(right, &disk_key, 0);
	ret = insert_ptr(trans, path, &disk_key, right->start, path->slots[1] + 1, 1);
	if (ret < 0)
		return ret;

	btrfs_mark_buffer_dirty(trans, right);
	btrfs_mark_buffer_dirty(trans, l);
	BUG_ON(path->slots[0] != slot);

	if (mid <= slot) {
		btrfs_tree_unlock(path->nodes[0]);
		free_extent_buffer(path->nodes[0]);
		path->nodes[0] = right;
		path->slots[0] -= mid;
		path->slots[1] += 1;
	} else {
		btrfs_tree_unlock(right);
		free_extent_buffer(right);
	}

	BUG_ON(path->slots[0] < 0);

	return 0;
}

 
static noinline int push_for_double_split(struct btrfs_trans_handle *trans,
					  struct btrfs_root *root,
					  struct btrfs_path *path,
					  int data_size)
{
	int ret;
	int progress = 0;
	int slot;
	u32 nritems;
	int space_needed = data_size;

	slot = path->slots[0];
	if (slot < btrfs_header_nritems(path->nodes[0]))
		space_needed -= btrfs_leaf_free_space(path->nodes[0]);

	 
	ret = push_leaf_right(trans, root, path, 1, space_needed, 0, slot);
	if (ret < 0)
		return ret;

	if (ret == 0)
		progress++;

	nritems = btrfs_header_nritems(path->nodes[0]);
	 
	if (path->slots[0] == 0 || path->slots[0] == nritems)
		return 0;

	if (btrfs_leaf_free_space(path->nodes[0]) >= data_size)
		return 0;

	 
	slot = path->slots[0];
	space_needed = data_size;
	if (slot > 0)
		space_needed -= btrfs_leaf_free_space(path->nodes[0]);
	ret = push_leaf_left(trans, root, path, 1, space_needed, 0, slot);
	if (ret < 0)
		return ret;

	if (ret == 0)
		progress++;

	if (progress)
		return 0;
	return 1;
}

 
static noinline int split_leaf(struct btrfs_trans_handle *trans,
			       struct btrfs_root *root,
			       const struct btrfs_key *ins_key,
			       struct btrfs_path *path, int data_size,
			       int extend)
{
	struct btrfs_disk_key disk_key;
	struct extent_buffer *l;
	u32 nritems;
	int mid;
	int slot;
	struct extent_buffer *right;
	struct btrfs_fs_info *fs_info = root->fs_info;
	int ret = 0;
	int wret;
	int split;
	int num_doubles = 0;
	int tried_avoid_double = 0;

	l = path->nodes[0];
	slot = path->slots[0];
	if (extend && data_size + btrfs_item_size(l, slot) +
	    sizeof(struct btrfs_item) > BTRFS_LEAF_DATA_SIZE(fs_info))
		return -EOVERFLOW;

	 
	if (data_size && path->nodes[1]) {
		int space_needed = data_size;

		if (slot < btrfs_header_nritems(l))
			space_needed -= btrfs_leaf_free_space(l);

		wret = push_leaf_right(trans, root, path, space_needed,
				       space_needed, 0, 0);
		if (wret < 0)
			return wret;
		if (wret) {
			space_needed = data_size;
			if (slot > 0)
				space_needed -= btrfs_leaf_free_space(l);
			wret = push_leaf_left(trans, root, path, space_needed,
					      space_needed, 0, (u32)-1);
			if (wret < 0)
				return wret;
		}
		l = path->nodes[0];

		 
		if (btrfs_leaf_free_space(l) >= data_size)
			return 0;
	}

	if (!path->nodes[1]) {
		ret = insert_new_root(trans, root, path, 1);
		if (ret)
			return ret;
	}
again:
	split = 1;
	l = path->nodes[0];
	slot = path->slots[0];
	nritems = btrfs_header_nritems(l);
	mid = (nritems + 1) / 2;

	if (mid <= slot) {
		if (nritems == 1 ||
		    leaf_space_used(l, mid, nritems - mid) + data_size >
			BTRFS_LEAF_DATA_SIZE(fs_info)) {
			if (slot >= nritems) {
				split = 0;
			} else {
				mid = slot;
				if (mid != nritems &&
				    leaf_space_used(l, mid, nritems - mid) +
				    data_size > BTRFS_LEAF_DATA_SIZE(fs_info)) {
					if (data_size && !tried_avoid_double)
						goto push_for_double;
					split = 2;
				}
			}
		}
	} else {
		if (leaf_space_used(l, 0, mid) + data_size >
			BTRFS_LEAF_DATA_SIZE(fs_info)) {
			if (!extend && data_size && slot == 0) {
				split = 0;
			} else if ((extend || !data_size) && slot == 0) {
				mid = 1;
			} else {
				mid = slot;
				if (mid != nritems &&
				    leaf_space_used(l, mid, nritems - mid) +
				    data_size > BTRFS_LEAF_DATA_SIZE(fs_info)) {
					if (data_size && !tried_avoid_double)
						goto push_for_double;
					split = 2;
				}
			}
		}
	}

	if (split == 0)
		btrfs_cpu_key_to_disk(&disk_key, ins_key);
	else
		btrfs_item_key(l, &disk_key, mid);

	 
	right = btrfs_alloc_tree_block(trans, root, 0, root->root_key.objectid,
				       &disk_key, 0, l->start, 0,
				       num_doubles ? BTRFS_NESTING_NEW_ROOT :
				       BTRFS_NESTING_SPLIT);
	if (IS_ERR(right))
		return PTR_ERR(right);

	root_add_used(root, fs_info->nodesize);

	if (split == 0) {
		if (mid <= slot) {
			btrfs_set_header_nritems(right, 0);
			ret = insert_ptr(trans, path, &disk_key,
					 right->start, path->slots[1] + 1, 1);
			if (ret < 0) {
				btrfs_tree_unlock(right);
				free_extent_buffer(right);
				return ret;
			}
			btrfs_tree_unlock(path->nodes[0]);
			free_extent_buffer(path->nodes[0]);
			path->nodes[0] = right;
			path->slots[0] = 0;
			path->slots[1] += 1;
		} else {
			btrfs_set_header_nritems(right, 0);
			ret = insert_ptr(trans, path, &disk_key,
					 right->start, path->slots[1], 1);
			if (ret < 0) {
				btrfs_tree_unlock(right);
				free_extent_buffer(right);
				return ret;
			}
			btrfs_tree_unlock(path->nodes[0]);
			free_extent_buffer(path->nodes[0]);
			path->nodes[0] = right;
			path->slots[0] = 0;
			if (path->slots[1] == 0)
				fixup_low_keys(trans, path, &disk_key, 1);
		}
		 
		return ret;
	}

	ret = copy_for_split(trans, path, l, right, slot, mid, nritems);
	if (ret < 0) {
		btrfs_tree_unlock(right);
		free_extent_buffer(right);
		return ret;
	}

	if (split == 2) {
		BUG_ON(num_doubles != 0);
		num_doubles++;
		goto again;
	}

	return 0;

push_for_double:
	push_for_double_split(trans, root, path, data_size);
	tried_avoid_double = 1;
	if (btrfs_leaf_free_space(path->nodes[0]) >= data_size)
		return 0;
	goto again;
}

static noinline int setup_leaf_for_split(struct btrfs_trans_handle *trans,
					 struct btrfs_root *root,
					 struct btrfs_path *path, int ins_len)
{
	struct btrfs_key key;
	struct extent_buffer *leaf;
	struct btrfs_file_extent_item *fi;
	u64 extent_len = 0;
	u32 item_size;
	int ret;

	leaf = path->nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);

	BUG_ON(key.type != BTRFS_EXTENT_DATA_KEY &&
	       key.type != BTRFS_EXTENT_CSUM_KEY);

	if (btrfs_leaf_free_space(leaf) >= ins_len)
		return 0;

	item_size = btrfs_item_size(leaf, path->slots[0]);
	if (key.type == BTRFS_EXTENT_DATA_KEY) {
		fi = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_file_extent_item);
		extent_len = btrfs_file_extent_num_bytes(leaf, fi);
	}
	btrfs_release_path(path);

	path->keep_locks = 1;
	path->search_for_split = 1;
	ret = btrfs_search_slot(trans, root, &key, path, 0, 1);
	path->search_for_split = 0;
	if (ret > 0)
		ret = -EAGAIN;
	if (ret < 0)
		goto err;

	ret = -EAGAIN;
	leaf = path->nodes[0];
	 
	if (item_size != btrfs_item_size(leaf, path->slots[0]))
		goto err;

	 
	if (btrfs_leaf_free_space(path->nodes[0]) >= ins_len)
		goto err;

	if (key.type == BTRFS_EXTENT_DATA_KEY) {
		fi = btrfs_item_ptr(leaf, path->slots[0],
				    struct btrfs_file_extent_item);
		if (extent_len != btrfs_file_extent_num_bytes(leaf, fi))
			goto err;
	}

	ret = split_leaf(trans, root, &key, path, ins_len, 1);
	if (ret)
		goto err;

	path->keep_locks = 0;
	btrfs_unlock_up_safe(path, 1);
	return 0;
err:
	path->keep_locks = 0;
	return ret;
}

static noinline int split_item(struct btrfs_trans_handle *trans,
			       struct btrfs_path *path,
			       const struct btrfs_key *new_key,
			       unsigned long split_offset)
{
	struct extent_buffer *leaf;
	int orig_slot, slot;
	char *buf;
	u32 nritems;
	u32 item_size;
	u32 orig_offset;
	struct btrfs_disk_key disk_key;

	leaf = path->nodes[0];
	 
	if (WARN_ON(btrfs_leaf_free_space(leaf) < sizeof(struct btrfs_item)))
		return -ENOSPC;

	orig_slot = path->slots[0];
	orig_offset = btrfs_item_offset(leaf, path->slots[0]);
	item_size = btrfs_item_size(leaf, path->slots[0]);

	buf = kmalloc(item_size, GFP_NOFS);
	if (!buf)
		return -ENOMEM;

	read_extent_buffer(leaf, buf, btrfs_item_ptr_offset(leaf,
			    path->slots[0]), item_size);

	slot = path->slots[0] + 1;
	nritems = btrfs_header_nritems(leaf);
	if (slot != nritems) {
		 
		memmove_leaf_items(leaf, slot + 1, slot, nritems - slot);
	}

	btrfs_cpu_key_to_disk(&disk_key, new_key);
	btrfs_set_item_key(leaf, &disk_key, slot);

	btrfs_set_item_offset(leaf, slot, orig_offset);
	btrfs_set_item_size(leaf, slot, item_size - split_offset);

	btrfs_set_item_offset(leaf, orig_slot,
				 orig_offset + item_size - split_offset);
	btrfs_set_item_size(leaf, orig_slot, split_offset);

	btrfs_set_header_nritems(leaf, nritems + 1);

	 
	write_extent_buffer(leaf, buf,
			    btrfs_item_ptr_offset(leaf, path->slots[0]),
			    split_offset);

	 
	write_extent_buffer(leaf, buf + split_offset,
			    btrfs_item_ptr_offset(leaf, slot),
			    item_size - split_offset);
	btrfs_mark_buffer_dirty(trans, leaf);

	BUG_ON(btrfs_leaf_free_space(leaf) < 0);
	kfree(buf);
	return 0;
}

 
int btrfs_split_item(struct btrfs_trans_handle *trans,
		     struct btrfs_root *root,
		     struct btrfs_path *path,
		     const struct btrfs_key *new_key,
		     unsigned long split_offset)
{
	int ret;
	ret = setup_leaf_for_split(trans, root, path,
				   sizeof(struct btrfs_item));
	if (ret)
		return ret;

	ret = split_item(trans, path, new_key, split_offset);
	return ret;
}

 
void btrfs_truncate_item(struct btrfs_trans_handle *trans,
			 struct btrfs_path *path, u32 new_size, int from_end)
{
	int slot;
	struct extent_buffer *leaf;
	u32 nritems;
	unsigned int data_end;
	unsigned int old_data_start;
	unsigned int old_size;
	unsigned int size_diff;
	int i;
	struct btrfs_map_token token;

	leaf = path->nodes[0];
	slot = path->slots[0];

	old_size = btrfs_item_size(leaf, slot);
	if (old_size == new_size)
		return;

	nritems = btrfs_header_nritems(leaf);
	data_end = leaf_data_end(leaf);

	old_data_start = btrfs_item_offset(leaf, slot);

	size_diff = old_size - new_size;

	BUG_ON(slot < 0);
	BUG_ON(slot >= nritems);

	 
	 
	btrfs_init_map_token(&token, leaf);
	for (i = slot; i < nritems; i++) {
		u32 ioff;

		ioff = btrfs_token_item_offset(&token, i);
		btrfs_set_token_item_offset(&token, i, ioff + size_diff);
	}

	 
	if (from_end) {
		memmove_leaf_data(leaf, data_end + size_diff, data_end,
				  old_data_start + new_size - data_end);
	} else {
		struct btrfs_disk_key disk_key;
		u64 offset;

		btrfs_item_key(leaf, &disk_key, slot);

		if (btrfs_disk_key_type(&disk_key) == BTRFS_EXTENT_DATA_KEY) {
			unsigned long ptr;
			struct btrfs_file_extent_item *fi;

			fi = btrfs_item_ptr(leaf, slot,
					    struct btrfs_file_extent_item);
			fi = (struct btrfs_file_extent_item *)(
			     (unsigned long)fi - size_diff);

			if (btrfs_file_extent_type(leaf, fi) ==
			    BTRFS_FILE_EXTENT_INLINE) {
				ptr = btrfs_item_ptr_offset(leaf, slot);
				memmove_extent_buffer(leaf, ptr,
				      (unsigned long)fi,
				      BTRFS_FILE_EXTENT_INLINE_DATA_START);
			}
		}

		memmove_leaf_data(leaf, data_end + size_diff, data_end,
				  old_data_start - data_end);

		offset = btrfs_disk_key_offset(&disk_key);
		btrfs_set_disk_key_offset(&disk_key, offset + size_diff);
		btrfs_set_item_key(leaf, &disk_key, slot);
		if (slot == 0)
			fixup_low_keys(trans, path, &disk_key, 1);
	}

	btrfs_set_item_size(leaf, slot, new_size);
	btrfs_mark_buffer_dirty(trans, leaf);

	if (btrfs_leaf_free_space(leaf) < 0) {
		btrfs_print_leaf(leaf);
		BUG();
	}
}

 
void btrfs_extend_item(struct btrfs_trans_handle *trans,
		       struct btrfs_path *path, u32 data_size)
{
	int slot;
	struct extent_buffer *leaf;
	u32 nritems;
	unsigned int data_end;
	unsigned int old_data;
	unsigned int old_size;
	int i;
	struct btrfs_map_token token;

	leaf = path->nodes[0];

	nritems = btrfs_header_nritems(leaf);
	data_end = leaf_data_end(leaf);

	if (btrfs_leaf_free_space(leaf) < data_size) {
		btrfs_print_leaf(leaf);
		BUG();
	}
	slot = path->slots[0];
	old_data = btrfs_item_data_end(leaf, slot);

	BUG_ON(slot < 0);
	if (slot >= nritems) {
		btrfs_print_leaf(leaf);
		btrfs_crit(leaf->fs_info, "slot %d too large, nritems %d",
			   slot, nritems);
		BUG();
	}

	 
	 
	btrfs_init_map_token(&token, leaf);
	for (i = slot; i < nritems; i++) {
		u32 ioff;

		ioff = btrfs_token_item_offset(&token, i);
		btrfs_set_token_item_offset(&token, i, ioff - data_size);
	}

	 
	memmove_leaf_data(leaf, data_end - data_size, data_end,
			  old_data - data_end);

	data_end = old_data;
	old_size = btrfs_item_size(leaf, slot);
	btrfs_set_item_size(leaf, slot, old_size + data_size);
	btrfs_mark_buffer_dirty(trans, leaf);

	if (btrfs_leaf_free_space(leaf) < 0) {
		btrfs_print_leaf(leaf);
		BUG();
	}
}

 
static void setup_items_for_insert(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root, struct btrfs_path *path,
				   const struct btrfs_item_batch *batch)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	int i;
	u32 nritems;
	unsigned int data_end;
	struct btrfs_disk_key disk_key;
	struct extent_buffer *leaf;
	int slot;
	struct btrfs_map_token token;
	u32 total_size;

	 
	if (path->slots[0] == 0) {
		btrfs_cpu_key_to_disk(&disk_key, &batch->keys[0]);
		fixup_low_keys(trans, path, &disk_key, 1);
	}
	btrfs_unlock_up_safe(path, 1);

	leaf = path->nodes[0];
	slot = path->slots[0];

	nritems = btrfs_header_nritems(leaf);
	data_end = leaf_data_end(leaf);
	total_size = batch->total_data_size + (batch->nr * sizeof(struct btrfs_item));

	if (btrfs_leaf_free_space(leaf) < total_size) {
		btrfs_print_leaf(leaf);
		btrfs_crit(fs_info, "not enough freespace need %u have %d",
			   total_size, btrfs_leaf_free_space(leaf));
		BUG();
	}

	btrfs_init_map_token(&token, leaf);
	if (slot != nritems) {
		unsigned int old_data = btrfs_item_data_end(leaf, slot);

		if (old_data < data_end) {
			btrfs_print_leaf(leaf);
			btrfs_crit(fs_info,
		"item at slot %d with data offset %u beyond data end of leaf %u",
				   slot, old_data, data_end);
			BUG();
		}
		 
		 
		for (i = slot; i < nritems; i++) {
			u32 ioff;

			ioff = btrfs_token_item_offset(&token, i);
			btrfs_set_token_item_offset(&token, i,
						       ioff - batch->total_data_size);
		}
		 
		memmove_leaf_items(leaf, slot + batch->nr, slot, nritems - slot);

		 
		memmove_leaf_data(leaf, data_end - batch->total_data_size,
				  data_end, old_data - data_end);
		data_end = old_data;
	}

	 
	for (i = 0; i < batch->nr; i++) {
		btrfs_cpu_key_to_disk(&disk_key, &batch->keys[i]);
		btrfs_set_item_key(leaf, &disk_key, slot + i);
		data_end -= batch->data_sizes[i];
		btrfs_set_token_item_offset(&token, slot + i, data_end);
		btrfs_set_token_item_size(&token, slot + i, batch->data_sizes[i]);
	}

	btrfs_set_header_nritems(leaf, nritems + batch->nr);
	btrfs_mark_buffer_dirty(trans, leaf);

	if (btrfs_leaf_free_space(leaf) < 0) {
		btrfs_print_leaf(leaf);
		BUG();
	}
}

 
void btrfs_setup_item_for_insert(struct btrfs_trans_handle *trans,
				 struct btrfs_root *root,
				 struct btrfs_path *path,
				 const struct btrfs_key *key,
				 u32 data_size)
{
	struct btrfs_item_batch batch;

	batch.keys = key;
	batch.data_sizes = &data_size;
	batch.total_data_size = data_size;
	batch.nr = 1;

	setup_items_for_insert(trans, root, path, &batch);
}

 
int btrfs_insert_empty_items(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root,
			    struct btrfs_path *path,
			    const struct btrfs_item_batch *batch)
{
	int ret = 0;
	int slot;
	u32 total_size;

	total_size = batch->total_data_size + (batch->nr * sizeof(struct btrfs_item));
	ret = btrfs_search_slot(trans, root, &batch->keys[0], path, total_size, 1);
	if (ret == 0)
		return -EEXIST;
	if (ret < 0)
		return ret;

	slot = path->slots[0];
	BUG_ON(slot < 0);

	setup_items_for_insert(trans, root, path, batch);
	return 0;
}

 
int btrfs_insert_item(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		      const struct btrfs_key *cpu_key, void *data,
		      u32 data_size)
{
	int ret = 0;
	struct btrfs_path *path;
	struct extent_buffer *leaf;
	unsigned long ptr;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	ret = btrfs_insert_empty_item(trans, root, path, cpu_key, data_size);
	if (!ret) {
		leaf = path->nodes[0];
		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		write_extent_buffer(leaf, data, ptr, data_size);
		btrfs_mark_buffer_dirty(trans, leaf);
	}
	btrfs_free_path(path);
	return ret;
}

 
int btrfs_duplicate_item(struct btrfs_trans_handle *trans,
			 struct btrfs_root *root,
			 struct btrfs_path *path,
			 const struct btrfs_key *new_key)
{
	struct extent_buffer *leaf;
	int ret;
	u32 item_size;

	leaf = path->nodes[0];
	item_size = btrfs_item_size(leaf, path->slots[0]);
	ret = setup_leaf_for_split(trans, root, path,
				   item_size + sizeof(struct btrfs_item));
	if (ret)
		return ret;

	path->slots[0]++;
	btrfs_setup_item_for_insert(trans, root, path, new_key, item_size);
	leaf = path->nodes[0];
	memcpy_extent_buffer(leaf,
			     btrfs_item_ptr_offset(leaf, path->slots[0]),
			     btrfs_item_ptr_offset(leaf, path->slots[0] - 1),
			     item_size);
	return 0;
}

 
int btrfs_del_ptr(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		  struct btrfs_path *path, int level, int slot)
{
	struct extent_buffer *parent = path->nodes[level];
	u32 nritems;
	int ret;

	nritems = btrfs_header_nritems(parent);
	if (slot != nritems - 1) {
		if (level) {
			ret = btrfs_tree_mod_log_insert_move(parent, slot,
					slot + 1, nritems - slot - 1);
			if (ret < 0) {
				btrfs_abort_transaction(trans, ret);
				return ret;
			}
		}
		memmove_extent_buffer(parent,
			      btrfs_node_key_ptr_offset(parent, slot),
			      btrfs_node_key_ptr_offset(parent, slot + 1),
			      sizeof(struct btrfs_key_ptr) *
			      (nritems - slot - 1));
	} else if (level) {
		ret = btrfs_tree_mod_log_insert_key(parent, slot,
						    BTRFS_MOD_LOG_KEY_REMOVE);
		if (ret < 0) {
			btrfs_abort_transaction(trans, ret);
			return ret;
		}
	}

	nritems--;
	btrfs_set_header_nritems(parent, nritems);
	if (nritems == 0 && parent == root->node) {
		BUG_ON(btrfs_header_level(root->node) != 1);
		 
		btrfs_set_header_level(root->node, 0);
	} else if (slot == 0) {
		struct btrfs_disk_key disk_key;

		btrfs_node_key(parent, &disk_key, 0);
		fixup_low_keys(trans, path, &disk_key, level + 1);
	}
	btrfs_mark_buffer_dirty(trans, parent);
	return 0;
}

 
static noinline int btrfs_del_leaf(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct btrfs_path *path,
				   struct extent_buffer *leaf)
{
	int ret;

	WARN_ON(btrfs_header_generation(leaf) != trans->transid);
	ret = btrfs_del_ptr(trans, root, path, 1, path->slots[1]);
	if (ret < 0)
		return ret;

	 
	btrfs_unlock_up_safe(path, 0);

	root_sub_used(root, leaf->len);

	atomic_inc(&leaf->refs);
	btrfs_free_tree_block(trans, btrfs_root_id(root), leaf, 0, 1);
	free_extent_buffer_stale(leaf);
	return 0;
}
 
int btrfs_del_items(struct btrfs_trans_handle *trans, struct btrfs_root *root,
		    struct btrfs_path *path, int slot, int nr)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct extent_buffer *leaf;
	int ret = 0;
	int wret;
	u32 nritems;

	leaf = path->nodes[0];
	nritems = btrfs_header_nritems(leaf);

	if (slot + nr != nritems) {
		const u32 last_off = btrfs_item_offset(leaf, slot + nr - 1);
		const int data_end = leaf_data_end(leaf);
		struct btrfs_map_token token;
		u32 dsize = 0;
		int i;

		for (i = 0; i < nr; i++)
			dsize += btrfs_item_size(leaf, slot + i);

		memmove_leaf_data(leaf, data_end + dsize, data_end,
				  last_off - data_end);

		btrfs_init_map_token(&token, leaf);
		for (i = slot + nr; i < nritems; i++) {
			u32 ioff;

			ioff = btrfs_token_item_offset(&token, i);
			btrfs_set_token_item_offset(&token, i, ioff + dsize);
		}

		memmove_leaf_items(leaf, slot, slot + nr, nritems - slot - nr);
	}
	btrfs_set_header_nritems(leaf, nritems - nr);
	nritems -= nr;

	 
	if (nritems == 0) {
		if (leaf == root->node) {
			btrfs_set_header_level(leaf, 0);
		} else {
			btrfs_clear_buffer_dirty(trans, leaf);
			ret = btrfs_del_leaf(trans, root, path, leaf);
			if (ret < 0)
				return ret;
		}
	} else {
		int used = leaf_space_used(leaf, 0, nritems);
		if (slot == 0) {
			struct btrfs_disk_key disk_key;

			btrfs_item_key(leaf, &disk_key, 0);
			fixup_low_keys(trans, path, &disk_key, 1);
		}

		 
		if (used < BTRFS_LEAF_DATA_SIZE(fs_info) / 3) {
			u32 min_push_space;

			 
			slot = path->slots[1];
			atomic_inc(&leaf->refs);
			 
			min_push_space = sizeof(struct btrfs_item) +
				btrfs_item_size(leaf, 0);
			wret = push_leaf_left(trans, root, path, 0,
					      min_push_space, 1, (u32)-1);
			if (wret < 0 && wret != -ENOSPC)
				ret = wret;

			if (path->nodes[0] == leaf &&
			    btrfs_header_nritems(leaf)) {
				 
				nritems = btrfs_header_nritems(leaf);
				min_push_space = leaf_space_used(leaf, 0, nritems);
				wret = push_leaf_right(trans, root, path, 0,
						       min_push_space, 1, 0);
				if (wret < 0 && wret != -ENOSPC)
					ret = wret;
			}

			if (btrfs_header_nritems(leaf) == 0) {
				path->slots[1] = slot;
				ret = btrfs_del_leaf(trans, root, path, leaf);
				if (ret < 0)
					return ret;
				free_extent_buffer(leaf);
				ret = 0;
			} else {
				 
				if (path->nodes[0] == leaf)
					btrfs_mark_buffer_dirty(trans, leaf);
				free_extent_buffer(leaf);
			}
		} else {
			btrfs_mark_buffer_dirty(trans, leaf);
		}
	}
	return ret;
}

 
int btrfs_search_forward(struct btrfs_root *root, struct btrfs_key *min_key,
			 struct btrfs_path *path,
			 u64 min_trans)
{
	struct extent_buffer *cur;
	struct btrfs_key found_key;
	int slot;
	int sret;
	u32 nritems;
	int level;
	int ret = 1;
	int keep_locks = path->keep_locks;

	ASSERT(!path->nowait);
	path->keep_locks = 1;
again:
	cur = btrfs_read_lock_root_node(root);
	level = btrfs_header_level(cur);
	WARN_ON(path->nodes[level]);
	path->nodes[level] = cur;
	path->locks[level] = BTRFS_READ_LOCK;

	if (btrfs_header_generation(cur) < min_trans) {
		ret = 1;
		goto out;
	}
	while (1) {
		nritems = btrfs_header_nritems(cur);
		level = btrfs_header_level(cur);
		sret = btrfs_bin_search(cur, 0, min_key, &slot);
		if (sret < 0) {
			ret = sret;
			goto out;
		}

		 
		if (level == path->lowest_level) {
			if (slot >= nritems)
				goto find_next_key;
			ret = 0;
			path->slots[level] = slot;
			btrfs_item_key_to_cpu(cur, &found_key, slot);
			goto out;
		}
		if (sret && slot > 0)
			slot--;
		 
		while (slot < nritems) {
			u64 gen;

			gen = btrfs_node_ptr_generation(cur, slot);
			if (gen < min_trans) {
				slot++;
				continue;
			}
			break;
		}
find_next_key:
		 
		if (slot >= nritems) {
			path->slots[level] = slot;
			sret = btrfs_find_next_key(root, path, min_key, level,
						  min_trans);
			if (sret == 0) {
				btrfs_release_path(path);
				goto again;
			} else {
				goto out;
			}
		}
		 
		btrfs_node_key_to_cpu(cur, &found_key, slot);
		path->slots[level] = slot;
		if (level == path->lowest_level) {
			ret = 0;
			goto out;
		}
		cur = btrfs_read_node_slot(cur, slot);
		if (IS_ERR(cur)) {
			ret = PTR_ERR(cur);
			goto out;
		}

		btrfs_tree_read_lock(cur);

		path->locks[level - 1] = BTRFS_READ_LOCK;
		path->nodes[level - 1] = cur;
		unlock_up(path, level, 1, 0, NULL);
	}
out:
	path->keep_locks = keep_locks;
	if (ret == 0) {
		btrfs_unlock_up_safe(path, path->lowest_level + 1);
		memcpy(min_key, &found_key, sizeof(found_key));
	}
	return ret;
}

 
int btrfs_find_next_key(struct btrfs_root *root, struct btrfs_path *path,
			struct btrfs_key *key, int level, u64 min_trans)
{
	int slot;
	struct extent_buffer *c;

	WARN_ON(!path->keep_locks && !path->skip_locking);
	while (level < BTRFS_MAX_LEVEL) {
		if (!path->nodes[level])
			return 1;

		slot = path->slots[level] + 1;
		c = path->nodes[level];
next:
		if (slot >= btrfs_header_nritems(c)) {
			int ret;
			int orig_lowest;
			struct btrfs_key cur_key;
			if (level + 1 >= BTRFS_MAX_LEVEL ||
			    !path->nodes[level + 1])
				return 1;

			if (path->locks[level + 1] || path->skip_locking) {
				level++;
				continue;
			}

			slot = btrfs_header_nritems(c) - 1;
			if (level == 0)
				btrfs_item_key_to_cpu(c, &cur_key, slot);
			else
				btrfs_node_key_to_cpu(c, &cur_key, slot);

			orig_lowest = path->lowest_level;
			btrfs_release_path(path);
			path->lowest_level = level;
			ret = btrfs_search_slot(NULL, root, &cur_key, path,
						0, 0);
			path->lowest_level = orig_lowest;
			if (ret < 0)
				return ret;

			c = path->nodes[level];
			slot = path->slots[level];
			if (ret == 0)
				slot++;
			goto next;
		}

		if (level == 0)
			btrfs_item_key_to_cpu(c, key, slot);
		else {
			u64 gen = btrfs_node_ptr_generation(c, slot);

			if (gen < min_trans) {
				slot++;
				goto next;
			}
			btrfs_node_key_to_cpu(c, key, slot);
		}
		return 0;
	}
	return 1;
}

int btrfs_next_old_leaf(struct btrfs_root *root, struct btrfs_path *path,
			u64 time_seq)
{
	int slot;
	int level;
	struct extent_buffer *c;
	struct extent_buffer *next;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_key key;
	bool need_commit_sem = false;
	u32 nritems;
	int ret;
	int i;

	 
	if (time_seq)
		ASSERT(!path->nowait);

	nritems = btrfs_header_nritems(path->nodes[0]);
	if (nritems == 0)
		return 1;

	btrfs_item_key_to_cpu(path->nodes[0], &key, nritems - 1);
again:
	level = 1;
	next = NULL;
	btrfs_release_path(path);

	path->keep_locks = 1;

	if (time_seq) {
		ret = btrfs_search_old_slot(root, &key, path, time_seq);
	} else {
		if (path->need_commit_sem) {
			path->need_commit_sem = 0;
			need_commit_sem = true;
			if (path->nowait) {
				if (!down_read_trylock(&fs_info->commit_root_sem)) {
					ret = -EAGAIN;
					goto done;
				}
			} else {
				down_read(&fs_info->commit_root_sem);
			}
		}
		ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	}
	path->keep_locks = 0;

	if (ret < 0)
		goto done;

	nritems = btrfs_header_nritems(path->nodes[0]);
	 
	if (nritems > 0 && path->slots[0] < nritems - 1) {
		if (ret == 0)
			path->slots[0]++;
		ret = 0;
		goto done;
	}
	 
	if (nritems > 0 && ret > 0 && path->slots[0] == nritems - 1) {
		ret = 0;
		goto done;
	}

	while (level < BTRFS_MAX_LEVEL) {
		if (!path->nodes[level]) {
			ret = 1;
			goto done;
		}

		slot = path->slots[level] + 1;
		c = path->nodes[level];
		if (slot >= btrfs_header_nritems(c)) {
			level++;
			if (level == BTRFS_MAX_LEVEL) {
				ret = 1;
				goto done;
			}
			continue;
		}


		 
		for (i = 0; i < level; i++) {
			if (path->locks[level]) {
				btrfs_tree_read_unlock(path->nodes[i]);
				path->locks[i] = 0;
			}
			free_extent_buffer(path->nodes[i]);
			path->nodes[i] = NULL;
		}

		next = c;
		ret = read_block_for_search(root, path, &next, level,
					    slot, &key);
		if (ret == -EAGAIN && !path->nowait)
			goto again;

		if (ret < 0) {
			btrfs_release_path(path);
			goto done;
		}

		if (!path->skip_locking) {
			ret = btrfs_try_tree_read_lock(next);
			if (!ret && path->nowait) {
				ret = -EAGAIN;
				goto done;
			}
			if (!ret && time_seq) {
				 
				free_extent_buffer(next);
				btrfs_release_path(path);
				cond_resched();
				goto again;
			}
			if (!ret)
				btrfs_tree_read_lock(next);
		}
		break;
	}
	path->slots[level] = slot;
	while (1) {
		level--;
		path->nodes[level] = next;
		path->slots[level] = 0;
		if (!path->skip_locking)
			path->locks[level] = BTRFS_READ_LOCK;
		if (!level)
			break;

		ret = read_block_for_search(root, path, &next, level,
					    0, &key);
		if (ret == -EAGAIN && !path->nowait)
			goto again;

		if (ret < 0) {
			btrfs_release_path(path);
			goto done;
		}

		if (!path->skip_locking) {
			if (path->nowait) {
				if (!btrfs_try_tree_read_lock(next)) {
					ret = -EAGAIN;
					goto done;
				}
			} else {
				btrfs_tree_read_lock(next);
			}
		}
	}
	ret = 0;
done:
	unlock_up(path, 0, 1, 0, NULL);
	if (need_commit_sem) {
		int ret2;

		path->need_commit_sem = 1;
		ret2 = finish_need_commit_sem_search(path);
		up_read(&fs_info->commit_root_sem);
		if (ret2)
			ret = ret2;
	}

	return ret;
}

int btrfs_next_old_item(struct btrfs_root *root, struct btrfs_path *path, u64 time_seq)
{
	path->slots[0]++;
	if (path->slots[0] >= btrfs_header_nritems(path->nodes[0]))
		return btrfs_next_old_leaf(root, path, time_seq);
	return 0;
}

 
int btrfs_previous_item(struct btrfs_root *root,
			struct btrfs_path *path, u64 min_objectid,
			int type)
{
	struct btrfs_key found_key;
	struct extent_buffer *leaf;
	u32 nritems;
	int ret;

	while (1) {
		if (path->slots[0] == 0) {
			ret = btrfs_prev_leaf(root, path);
			if (ret != 0)
				return ret;
		} else {
			path->slots[0]--;
		}
		leaf = path->nodes[0];
		nritems = btrfs_header_nritems(leaf);
		if (nritems == 0)
			return 1;
		if (path->slots[0] == nritems)
			path->slots[0]--;

		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		if (found_key.objectid < min_objectid)
			break;
		if (found_key.type == type)
			return 0;
		if (found_key.objectid == min_objectid &&
		    found_key.type < type)
			break;
	}
	return 1;
}

 
int btrfs_previous_extent_item(struct btrfs_root *root,
			struct btrfs_path *path, u64 min_objectid)
{
	struct btrfs_key found_key;
	struct extent_buffer *leaf;
	u32 nritems;
	int ret;

	while (1) {
		if (path->slots[0] == 0) {
			ret = btrfs_prev_leaf(root, path);
			if (ret != 0)
				return ret;
		} else {
			path->slots[0]--;
		}
		leaf = path->nodes[0];
		nritems = btrfs_header_nritems(leaf);
		if (nritems == 0)
			return 1;
		if (path->slots[0] == nritems)
			path->slots[0]--;

		btrfs_item_key_to_cpu(leaf, &found_key, path->slots[0]);
		if (found_key.objectid < min_objectid)
			break;
		if (found_key.type == BTRFS_EXTENT_ITEM_KEY ||
		    found_key.type == BTRFS_METADATA_ITEM_KEY)
			return 0;
		if (found_key.objectid == min_objectid &&
		    found_key.type < BTRFS_EXTENT_ITEM_KEY)
			break;
	}
	return 1;
}

int __init btrfs_ctree_init(void)
{
	btrfs_path_cachep = kmem_cache_create("btrfs_path",
			sizeof(struct btrfs_path), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!btrfs_path_cachep)
		return -ENOMEM;
	return 0;
}

void __cold btrfs_ctree_exit(void)
{
	kmem_cache_destroy(btrfs_path_cachep);
}


 

#include <linux/mm.h>
#include <linux/rbtree.h>
#include <trace/events/btrfs.h>
#include "ctree.h"
#include "disk-io.h"
#include "backref.h"
#include "ulist.h"
#include "transaction.h"
#include "delayed-ref.h"
#include "locking.h"
#include "misc.h"
#include "tree-mod-log.h"
#include "fs.h"
#include "accessors.h"
#include "extent-tree.h"
#include "relocation.h"
#include "tree-checker.h"

 
#define BACKREF_FOUND_SHARED     6
#define BACKREF_FOUND_NOT_SHARED 7

struct extent_inode_elem {
	u64 inum;
	u64 offset;
	u64 num_bytes;
	struct extent_inode_elem *next;
};

static int check_extent_in_eb(struct btrfs_backref_walk_ctx *ctx,
			      const struct btrfs_key *key,
			      const struct extent_buffer *eb,
			      const struct btrfs_file_extent_item *fi,
			      struct extent_inode_elem **eie)
{
	const u64 data_len = btrfs_file_extent_num_bytes(eb, fi);
	u64 offset = key->offset;
	struct extent_inode_elem *e;
	const u64 *root_ids;
	int root_count;
	bool cached;

	if (!ctx->ignore_extent_item_pos &&
	    !btrfs_file_extent_compression(eb, fi) &&
	    !btrfs_file_extent_encryption(eb, fi) &&
	    !btrfs_file_extent_other_encoding(eb, fi)) {
		u64 data_offset;

		data_offset = btrfs_file_extent_offset(eb, fi);

		if (ctx->extent_item_pos < data_offset ||
		    ctx->extent_item_pos >= data_offset + data_len)
			return 1;
		offset += ctx->extent_item_pos - data_offset;
	}

	if (!ctx->indirect_ref_iterator || !ctx->cache_lookup)
		goto add_inode_elem;

	cached = ctx->cache_lookup(eb->start, ctx->user_ctx, &root_ids,
				   &root_count);
	if (!cached)
		goto add_inode_elem;

	for (int i = 0; i < root_count; i++) {
		int ret;

		ret = ctx->indirect_ref_iterator(key->objectid, offset,
						 data_len, root_ids[i],
						 ctx->user_ctx);
		if (ret)
			return ret;
	}

add_inode_elem:
	e = kmalloc(sizeof(*e), GFP_NOFS);
	if (!e)
		return -ENOMEM;

	e->next = *eie;
	e->inum = key->objectid;
	e->offset = offset;
	e->num_bytes = data_len;
	*eie = e;

	return 0;
}

static void free_inode_elem_list(struct extent_inode_elem *eie)
{
	struct extent_inode_elem *eie_next;

	for (; eie; eie = eie_next) {
		eie_next = eie->next;
		kfree(eie);
	}
}

static int find_extent_in_eb(struct btrfs_backref_walk_ctx *ctx,
			     const struct extent_buffer *eb,
			     struct extent_inode_elem **eie)
{
	u64 disk_byte;
	struct btrfs_key key;
	struct btrfs_file_extent_item *fi;
	int slot;
	int nritems;
	int extent_type;
	int ret;

	 
	nritems = btrfs_header_nritems(eb);
	for (slot = 0; slot < nritems; ++slot) {
		btrfs_item_key_to_cpu(eb, &key, slot);
		if (key.type != BTRFS_EXTENT_DATA_KEY)
			continue;
		fi = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		extent_type = btrfs_file_extent_type(eb, fi);
		if (extent_type == BTRFS_FILE_EXTENT_INLINE)
			continue;
		 
		disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);
		if (disk_byte != ctx->bytenr)
			continue;

		ret = check_extent_in_eb(ctx, &key, eb, fi, eie);
		if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP || ret < 0)
			return ret;
	}

	return 0;
}

struct preftree {
	struct rb_root_cached root;
	unsigned int count;
};

#define PREFTREE_INIT	{ .root = RB_ROOT_CACHED, .count = 0 }

struct preftrees {
	struct preftree direct;     
	struct preftree indirect;   
	struct preftree indirect_missing_keys;
};

 
struct share_check {
	struct btrfs_backref_share_check_ctx *ctx;
	struct btrfs_root *root;
	u64 inum;
	u64 data_bytenr;
	u64 data_extent_gen;
	 
	int share_count;
	 
	int self_ref_count;
	bool have_delayed_delete_refs;
};

static inline int extent_is_shared(struct share_check *sc)
{
	return (sc && sc->share_count > 1) ? BACKREF_FOUND_SHARED : 0;
}

static struct kmem_cache *btrfs_prelim_ref_cache;

int __init btrfs_prelim_ref_init(void)
{
	btrfs_prelim_ref_cache = kmem_cache_create("btrfs_prelim_ref",
					sizeof(struct prelim_ref),
					0,
					SLAB_MEM_SPREAD,
					NULL);
	if (!btrfs_prelim_ref_cache)
		return -ENOMEM;
	return 0;
}

void __cold btrfs_prelim_ref_exit(void)
{
	kmem_cache_destroy(btrfs_prelim_ref_cache);
}

static void free_pref(struct prelim_ref *ref)
{
	kmem_cache_free(btrfs_prelim_ref_cache, ref);
}

 
static int prelim_ref_compare(struct prelim_ref *ref1,
			      struct prelim_ref *ref2)
{
	if (ref1->level < ref2->level)
		return -1;
	if (ref1->level > ref2->level)
		return 1;
	if (ref1->root_id < ref2->root_id)
		return -1;
	if (ref1->root_id > ref2->root_id)
		return 1;
	if (ref1->key_for_search.type < ref2->key_for_search.type)
		return -1;
	if (ref1->key_for_search.type > ref2->key_for_search.type)
		return 1;
	if (ref1->key_for_search.objectid < ref2->key_for_search.objectid)
		return -1;
	if (ref1->key_for_search.objectid > ref2->key_for_search.objectid)
		return 1;
	if (ref1->key_for_search.offset < ref2->key_for_search.offset)
		return -1;
	if (ref1->key_for_search.offset > ref2->key_for_search.offset)
		return 1;
	if (ref1->parent < ref2->parent)
		return -1;
	if (ref1->parent > ref2->parent)
		return 1;

	return 0;
}

static void update_share_count(struct share_check *sc, int oldcount,
			       int newcount, struct prelim_ref *newref)
{
	if ((!sc) || (oldcount == 0 && newcount < 1))
		return;

	if (oldcount > 0 && newcount < 1)
		sc->share_count--;
	else if (oldcount < 1 && newcount > 0)
		sc->share_count++;

	if (newref->root_id == sc->root->root_key.objectid &&
	    newref->wanted_disk_byte == sc->data_bytenr &&
	    newref->key_for_search.objectid == sc->inum)
		sc->self_ref_count += newref->count;
}

 
static void prelim_ref_insert(const struct btrfs_fs_info *fs_info,
			      struct preftree *preftree,
			      struct prelim_ref *newref,
			      struct share_check *sc)
{
	struct rb_root_cached *root;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	struct prelim_ref *ref;
	int result;
	bool leftmost = true;

	root = &preftree->root;
	p = &root->rb_root.rb_node;

	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct prelim_ref, rbnode);
		result = prelim_ref_compare(ref, newref);
		if (result < 0) {
			p = &(*p)->rb_left;
		} else if (result > 0) {
			p = &(*p)->rb_right;
			leftmost = false;
		} else {
			 
			struct extent_inode_elem *eie = ref->inode_list;

			while (eie && eie->next)
				eie = eie->next;

			if (!eie)
				ref->inode_list = newref->inode_list;
			else
				eie->next = newref->inode_list;
			trace_btrfs_prelim_ref_merge(fs_info, ref, newref,
						     preftree->count);
			 
			update_share_count(sc, ref->count,
					   ref->count + newref->count, newref);
			ref->count += newref->count;
			free_pref(newref);
			return;
		}
	}

	update_share_count(sc, 0, newref->count, newref);
	preftree->count++;
	trace_btrfs_prelim_ref_insert(fs_info, newref, NULL, preftree->count);
	rb_link_node(&newref->rbnode, parent, p);
	rb_insert_color_cached(&newref->rbnode, root, leftmost);
}

 
static void prelim_release(struct preftree *preftree)
{
	struct prelim_ref *ref, *next_ref;

	rbtree_postorder_for_each_entry_safe(ref, next_ref,
					     &preftree->root.rb_root, rbnode) {
		free_inode_elem_list(ref->inode_list);
		free_pref(ref);
	}

	preftree->root = RB_ROOT_CACHED;
	preftree->count = 0;
}

 
static int add_prelim_ref(const struct btrfs_fs_info *fs_info,
			  struct preftree *preftree, u64 root_id,
			  const struct btrfs_key *key, int level, u64 parent,
			  u64 wanted_disk_byte, int count,
			  struct share_check *sc, gfp_t gfp_mask)
{
	struct prelim_ref *ref;

	if (root_id == BTRFS_DATA_RELOC_TREE_OBJECTID)
		return 0;

	ref = kmem_cache_alloc(btrfs_prelim_ref_cache, gfp_mask);
	if (!ref)
		return -ENOMEM;

	ref->root_id = root_id;
	if (key)
		ref->key_for_search = *key;
	else
		memset(&ref->key_for_search, 0, sizeof(ref->key_for_search));

	ref->inode_list = NULL;
	ref->level = level;
	ref->count = count;
	ref->parent = parent;
	ref->wanted_disk_byte = wanted_disk_byte;
	prelim_ref_insert(fs_info, preftree, ref, sc);
	return extent_is_shared(sc);
}

 
static int add_direct_ref(const struct btrfs_fs_info *fs_info,
			  struct preftrees *preftrees, int level, u64 parent,
			  u64 wanted_disk_byte, int count,
			  struct share_check *sc, gfp_t gfp_mask)
{
	return add_prelim_ref(fs_info, &preftrees->direct, 0, NULL, level,
			      parent, wanted_disk_byte, count, sc, gfp_mask);
}

 
static int add_indirect_ref(const struct btrfs_fs_info *fs_info,
			    struct preftrees *preftrees, u64 root_id,
			    const struct btrfs_key *key, int level,
			    u64 wanted_disk_byte, int count,
			    struct share_check *sc, gfp_t gfp_mask)
{
	struct preftree *tree = &preftrees->indirect;

	if (!key)
		tree = &preftrees->indirect_missing_keys;
	return add_prelim_ref(fs_info, tree, root_id, key, level, 0,
			      wanted_disk_byte, count, sc, gfp_mask);
}

static int is_shared_data_backref(struct preftrees *preftrees, u64 bytenr)
{
	struct rb_node **p = &preftrees->direct.root.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct prelim_ref *ref = NULL;
	struct prelim_ref target = {};
	int result;

	target.parent = bytenr;

	while (*p) {
		parent = *p;
		ref = rb_entry(parent, struct prelim_ref, rbnode);
		result = prelim_ref_compare(ref, &target);

		if (result < 0)
			p = &(*p)->rb_left;
		else if (result > 0)
			p = &(*p)->rb_right;
		else
			return 1;
	}
	return 0;
}

static int add_all_parents(struct btrfs_backref_walk_ctx *ctx,
			   struct btrfs_root *root, struct btrfs_path *path,
			   struct ulist *parents,
			   struct preftrees *preftrees, struct prelim_ref *ref,
			   int level)
{
	int ret = 0;
	int slot;
	struct extent_buffer *eb;
	struct btrfs_key key;
	struct btrfs_key *key_for_search = &ref->key_for_search;
	struct btrfs_file_extent_item *fi;
	struct extent_inode_elem *eie = NULL, *old = NULL;
	u64 disk_byte;
	u64 wanted_disk_byte = ref->wanted_disk_byte;
	u64 count = 0;
	u64 data_offset;
	u8 type;

	if (level != 0) {
		eb = path->nodes[level];
		ret = ulist_add(parents, eb->start, 0, GFP_NOFS);
		if (ret < 0)
			return ret;
		return 0;
	}

	 
	eb = path->nodes[0];
	if (path->slots[0] >= btrfs_header_nritems(eb) ||
	    is_shared_data_backref(preftrees, eb->start) ||
	    ref->root_id != btrfs_header_owner(eb)) {
		if (ctx->time_seq == BTRFS_SEQ_LAST)
			ret = btrfs_next_leaf(root, path);
		else
			ret = btrfs_next_old_leaf(root, path, ctx->time_seq);
	}

	while (!ret && count < ref->count) {
		eb = path->nodes[0];
		slot = path->slots[0];

		btrfs_item_key_to_cpu(eb, &key, slot);

		if (key.objectid != key_for_search->objectid ||
		    key.type != BTRFS_EXTENT_DATA_KEY)
			break;

		 
		if (slot == 0 &&
		    (is_shared_data_backref(preftrees, eb->start) ||
		     ref->root_id != btrfs_header_owner(eb))) {
			if (ctx->time_seq == BTRFS_SEQ_LAST)
				ret = btrfs_next_leaf(root, path);
			else
				ret = btrfs_next_old_leaf(root, path, ctx->time_seq);
			continue;
		}
		fi = btrfs_item_ptr(eb, slot, struct btrfs_file_extent_item);
		type = btrfs_file_extent_type(eb, fi);
		if (type == BTRFS_FILE_EXTENT_INLINE)
			goto next;
		disk_byte = btrfs_file_extent_disk_bytenr(eb, fi);
		data_offset = btrfs_file_extent_offset(eb, fi);

		if (disk_byte == wanted_disk_byte) {
			eie = NULL;
			old = NULL;
			if (ref->key_for_search.offset == key.offset - data_offset)
				count++;
			else
				goto next;
			if (!ctx->skip_inode_ref_list) {
				ret = check_extent_in_eb(ctx, &key, eb, fi, &eie);
				if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP ||
				    ret < 0)
					break;
			}
			if (ret > 0)
				goto next;
			ret = ulist_add_merge_ptr(parents, eb->start,
						  eie, (void **)&old, GFP_NOFS);
			if (ret < 0)
				break;
			if (!ret && !ctx->skip_inode_ref_list) {
				while (old->next)
					old = old->next;
				old->next = eie;
			}
			eie = NULL;
		}
next:
		if (ctx->time_seq == BTRFS_SEQ_LAST)
			ret = btrfs_next_item(root, path);
		else
			ret = btrfs_next_old_item(root, path, ctx->time_seq);
	}

	if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP || ret < 0)
		free_inode_elem_list(eie);
	else if (ret > 0)
		ret = 0;

	return ret;
}

 
static int resolve_indirect_ref(struct btrfs_backref_walk_ctx *ctx,
				struct btrfs_path *path,
				struct preftrees *preftrees,
				struct prelim_ref *ref, struct ulist *parents)
{
	struct btrfs_root *root;
	struct extent_buffer *eb;
	int ret = 0;
	int root_level;
	int level = ref->level;
	struct btrfs_key search_key = ref->key_for_search;

	 
	if (path->search_commit_root)
		root = btrfs_get_fs_root_commit_root(ctx->fs_info, path, ref->root_id);
	else
		root = btrfs_get_fs_root(ctx->fs_info, ref->root_id, false);
	if (IS_ERR(root)) {
		ret = PTR_ERR(root);
		goto out_free;
	}

	if (!path->search_commit_root &&
	    test_bit(BTRFS_ROOT_DELETING, &root->state)) {
		ret = -ENOENT;
		goto out;
	}

	if (btrfs_is_testing(ctx->fs_info)) {
		ret = -ENOENT;
		goto out;
	}

	if (path->search_commit_root)
		root_level = btrfs_header_level(root->commit_root);
	else if (ctx->time_seq == BTRFS_SEQ_LAST)
		root_level = btrfs_header_level(root->node);
	else
		root_level = btrfs_old_root_level(root, ctx->time_seq);

	if (root_level + 1 == level)
		goto out;

	 
	if (search_key.type == BTRFS_EXTENT_DATA_KEY &&
	    search_key.offset >= LLONG_MAX)
		search_key.offset = 0;
	path->lowest_level = level;
	if (ctx->time_seq == BTRFS_SEQ_LAST)
		ret = btrfs_search_slot(NULL, root, &search_key, path, 0, 0);
	else
		ret = btrfs_search_old_slot(root, &search_key, path, ctx->time_seq);

	btrfs_debug(ctx->fs_info,
		"search slot in root %llu (level %d, ref count %d) returned %d for key (%llu %u %llu)",
		 ref->root_id, level, ref->count, ret,
		 ref->key_for_search.objectid, ref->key_for_search.type,
		 ref->key_for_search.offset);
	if (ret < 0)
		goto out;

	eb = path->nodes[level];
	while (!eb) {
		if (WARN_ON(!level)) {
			ret = 1;
			goto out;
		}
		level--;
		eb = path->nodes[level];
	}

	ret = add_all_parents(ctx, root, path, parents, preftrees, ref, level);
out:
	btrfs_put_root(root);
out_free:
	path->lowest_level = 0;
	btrfs_release_path(path);
	return ret;
}

static struct extent_inode_elem *
unode_aux_to_inode_list(struct ulist_node *node)
{
	if (!node)
		return NULL;
	return (struct extent_inode_elem *)(uintptr_t)node->aux;
}

static void free_leaf_list(struct ulist *ulist)
{
	struct ulist_node *node;
	struct ulist_iterator uiter;

	ULIST_ITER_INIT(&uiter);
	while ((node = ulist_next(ulist, &uiter)))
		free_inode_elem_list(unode_aux_to_inode_list(node));

	ulist_free(ulist);
}

 
static int resolve_indirect_refs(struct btrfs_backref_walk_ctx *ctx,
				 struct btrfs_path *path,
				 struct preftrees *preftrees,
				 struct share_check *sc)
{
	int err;
	int ret = 0;
	struct ulist *parents;
	struct ulist_node *node;
	struct ulist_iterator uiter;
	struct rb_node *rnode;

	parents = ulist_alloc(GFP_NOFS);
	if (!parents)
		return -ENOMEM;

	 
	while ((rnode = rb_first_cached(&preftrees->indirect.root))) {
		struct prelim_ref *ref;

		ref = rb_entry(rnode, struct prelim_ref, rbnode);
		if (WARN(ref->parent,
			 "BUG: direct ref found in indirect tree")) {
			ret = -EINVAL;
			goto out;
		}

		rb_erase_cached(&ref->rbnode, &preftrees->indirect.root);
		preftrees->indirect.count--;

		if (ref->count == 0) {
			free_pref(ref);
			continue;
		}

		if (sc && ref->root_id != sc->root->root_key.objectid) {
			free_pref(ref);
			ret = BACKREF_FOUND_SHARED;
			goto out;
		}
		err = resolve_indirect_ref(ctx, path, preftrees, ref, parents);
		 
		if (err == -ENOENT) {
			prelim_ref_insert(ctx->fs_info, &preftrees->direct, ref,
					  NULL);
			continue;
		} else if (err) {
			free_pref(ref);
			ret = err;
			goto out;
		}

		 
		ULIST_ITER_INIT(&uiter);
		node = ulist_next(parents, &uiter);
		ref->parent = node ? node->val : 0;
		ref->inode_list = unode_aux_to_inode_list(node);

		 
		while ((node = ulist_next(parents, &uiter))) {
			struct prelim_ref *new_ref;

			new_ref = kmem_cache_alloc(btrfs_prelim_ref_cache,
						   GFP_NOFS);
			if (!new_ref) {
				free_pref(ref);
				ret = -ENOMEM;
				goto out;
			}
			memcpy(new_ref, ref, sizeof(*ref));
			new_ref->parent = node->val;
			new_ref->inode_list = unode_aux_to_inode_list(node);
			prelim_ref_insert(ctx->fs_info, &preftrees->direct,
					  new_ref, NULL);
		}

		 
		prelim_ref_insert(ctx->fs_info, &preftrees->direct, ref, NULL);

		ulist_reinit(parents);
		cond_resched();
	}
out:
	 
	free_leaf_list(parents);
	return ret;
}

 
static int add_missing_keys(struct btrfs_fs_info *fs_info,
			    struct preftrees *preftrees, bool lock)
{
	struct prelim_ref *ref;
	struct extent_buffer *eb;
	struct preftree *tree = &preftrees->indirect_missing_keys;
	struct rb_node *node;

	while ((node = rb_first_cached(&tree->root))) {
		struct btrfs_tree_parent_check check = { 0 };

		ref = rb_entry(node, struct prelim_ref, rbnode);
		rb_erase_cached(node, &tree->root);

		BUG_ON(ref->parent);	 
		BUG_ON(ref->key_for_search.type);
		BUG_ON(!ref->wanted_disk_byte);

		check.level = ref->level - 1;
		check.owner_root = ref->root_id;

		eb = read_tree_block(fs_info, ref->wanted_disk_byte, &check);
		if (IS_ERR(eb)) {
			free_pref(ref);
			return PTR_ERR(eb);
		}
		if (!extent_buffer_uptodate(eb)) {
			free_pref(ref);
			free_extent_buffer(eb);
			return -EIO;
		}

		if (lock)
			btrfs_tree_read_lock(eb);
		if (btrfs_header_level(eb) == 0)
			btrfs_item_key_to_cpu(eb, &ref->key_for_search, 0);
		else
			btrfs_node_key_to_cpu(eb, &ref->key_for_search, 0);
		if (lock)
			btrfs_tree_read_unlock(eb);
		free_extent_buffer(eb);
		prelim_ref_insert(fs_info, &preftrees->indirect, ref, NULL);
		cond_resched();
	}
	return 0;
}

 
static int add_delayed_refs(const struct btrfs_fs_info *fs_info,
			    struct btrfs_delayed_ref_head *head, u64 seq,
			    struct preftrees *preftrees, struct share_check *sc)
{
	struct btrfs_delayed_ref_node *node;
	struct btrfs_key key;
	struct rb_node *n;
	int count;
	int ret = 0;

	spin_lock(&head->lock);
	for (n = rb_first_cached(&head->ref_tree); n; n = rb_next(n)) {
		node = rb_entry(n, struct btrfs_delayed_ref_node,
				ref_node);
		if (node->seq > seq)
			continue;

		switch (node->action) {
		case BTRFS_ADD_DELAYED_EXTENT:
		case BTRFS_UPDATE_DELAYED_HEAD:
			WARN_ON(1);
			continue;
		case BTRFS_ADD_DELAYED_REF:
			count = node->ref_mod;
			break;
		case BTRFS_DROP_DELAYED_REF:
			count = node->ref_mod * -1;
			break;
		default:
			BUG();
		}
		switch (node->type) {
		case BTRFS_TREE_BLOCK_REF_KEY: {
			 
			struct btrfs_delayed_tree_ref *ref;
			struct btrfs_key *key_ptr = NULL;

			if (head->extent_op && head->extent_op->update_key) {
				btrfs_disk_key_to_cpu(&key, &head->extent_op->key);
				key_ptr = &key;
			}

			ref = btrfs_delayed_node_to_tree_ref(node);
			ret = add_indirect_ref(fs_info, preftrees, ref->root,
					       key_ptr, ref->level + 1,
					       node->bytenr, count, sc,
					       GFP_ATOMIC);
			break;
		}
		case BTRFS_SHARED_BLOCK_REF_KEY: {
			 
			struct btrfs_delayed_tree_ref *ref;

			ref = btrfs_delayed_node_to_tree_ref(node);

			ret = add_direct_ref(fs_info, preftrees, ref->level + 1,
					     ref->parent, node->bytenr, count,
					     sc, GFP_ATOMIC);
			break;
		}
		case BTRFS_EXTENT_DATA_REF_KEY: {
			 
			struct btrfs_delayed_data_ref *ref;
			ref = btrfs_delayed_node_to_data_ref(node);

			key.objectid = ref->objectid;
			key.type = BTRFS_EXTENT_DATA_KEY;
			key.offset = ref->offset;

			 
			if (sc && count < 0)
				sc->have_delayed_delete_refs = true;

			ret = add_indirect_ref(fs_info, preftrees, ref->root,
					       &key, 0, node->bytenr, count, sc,
					       GFP_ATOMIC);
			break;
		}
		case BTRFS_SHARED_DATA_REF_KEY: {
			 
			struct btrfs_delayed_data_ref *ref;

			ref = btrfs_delayed_node_to_data_ref(node);

			ret = add_direct_ref(fs_info, preftrees, 0, ref->parent,
					     node->bytenr, count, sc,
					     GFP_ATOMIC);
			break;
		}
		default:
			WARN_ON(1);
		}
		 
		if (ret && (ret != BACKREF_FOUND_SHARED))
			break;
	}
	if (!ret)
		ret = extent_is_shared(sc);

	spin_unlock(&head->lock);
	return ret;
}

 
static int add_inline_refs(struct btrfs_backref_walk_ctx *ctx,
			   struct btrfs_path *path,
			   int *info_level, struct preftrees *preftrees,
			   struct share_check *sc)
{
	int ret = 0;
	int slot;
	struct extent_buffer *leaf;
	struct btrfs_key key;
	struct btrfs_key found_key;
	unsigned long ptr;
	unsigned long end;
	struct btrfs_extent_item *ei;
	u64 flags;
	u64 item_size;

	 
	leaf = path->nodes[0];
	slot = path->slots[0];

	item_size = btrfs_item_size(leaf, slot);
	BUG_ON(item_size < sizeof(*ei));

	ei = btrfs_item_ptr(leaf, slot, struct btrfs_extent_item);

	if (ctx->check_extent_item) {
		ret = ctx->check_extent_item(ctx->bytenr, ei, leaf, ctx->user_ctx);
		if (ret)
			return ret;
	}

	flags = btrfs_extent_flags(leaf, ei);
	btrfs_item_key_to_cpu(leaf, &found_key, slot);

	ptr = (unsigned long)(ei + 1);
	end = (unsigned long)ei + item_size;

	if (found_key.type == BTRFS_EXTENT_ITEM_KEY &&
	    flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
		struct btrfs_tree_block_info *info;

		info = (struct btrfs_tree_block_info *)ptr;
		*info_level = btrfs_tree_block_level(leaf, info);
		ptr += sizeof(struct btrfs_tree_block_info);
		BUG_ON(ptr > end);
	} else if (found_key.type == BTRFS_METADATA_ITEM_KEY) {
		*info_level = found_key.offset;
	} else {
		BUG_ON(!(flags & BTRFS_EXTENT_FLAG_DATA));
	}

	while (ptr < end) {
		struct btrfs_extent_inline_ref *iref;
		u64 offset;
		int type;

		iref = (struct btrfs_extent_inline_ref *)ptr;
		type = btrfs_get_extent_inline_ref_type(leaf, iref,
							BTRFS_REF_TYPE_ANY);
		if (type == BTRFS_REF_TYPE_INVALID)
			return -EUCLEAN;

		offset = btrfs_extent_inline_ref_offset(leaf, iref);

		switch (type) {
		case BTRFS_SHARED_BLOCK_REF_KEY:
			ret = add_direct_ref(ctx->fs_info, preftrees,
					     *info_level + 1, offset,
					     ctx->bytenr, 1, NULL, GFP_NOFS);
			break;
		case BTRFS_SHARED_DATA_REF_KEY: {
			struct btrfs_shared_data_ref *sdref;
			int count;

			sdref = (struct btrfs_shared_data_ref *)(iref + 1);
			count = btrfs_shared_data_ref_count(leaf, sdref);

			ret = add_direct_ref(ctx->fs_info, preftrees, 0, offset,
					     ctx->bytenr, count, sc, GFP_NOFS);
			break;
		}
		case BTRFS_TREE_BLOCK_REF_KEY:
			ret = add_indirect_ref(ctx->fs_info, preftrees, offset,
					       NULL, *info_level + 1,
					       ctx->bytenr, 1, NULL, GFP_NOFS);
			break;
		case BTRFS_EXTENT_DATA_REF_KEY: {
			struct btrfs_extent_data_ref *dref;
			int count;
			u64 root;

			dref = (struct btrfs_extent_data_ref *)(&iref->offset);
			count = btrfs_extent_data_ref_count(leaf, dref);
			key.objectid = btrfs_extent_data_ref_objectid(leaf,
								      dref);
			key.type = BTRFS_EXTENT_DATA_KEY;
			key.offset = btrfs_extent_data_ref_offset(leaf, dref);

			if (sc && key.objectid != sc->inum &&
			    !sc->have_delayed_delete_refs) {
				ret = BACKREF_FOUND_SHARED;
				break;
			}

			root = btrfs_extent_data_ref_root(leaf, dref);

			if (!ctx->skip_data_ref ||
			    !ctx->skip_data_ref(root, key.objectid, key.offset,
						ctx->user_ctx))
				ret = add_indirect_ref(ctx->fs_info, preftrees,
						       root, &key, 0, ctx->bytenr,
						       count, sc, GFP_NOFS);
			break;
		}
		default:
			WARN_ON(1);
		}
		if (ret)
			return ret;
		ptr += btrfs_extent_inline_ref_size(type);
	}

	return 0;
}

 
static int add_keyed_refs(struct btrfs_backref_walk_ctx *ctx,
			  struct btrfs_root *extent_root,
			  struct btrfs_path *path,
			  int info_level, struct preftrees *preftrees,
			  struct share_check *sc)
{
	struct btrfs_fs_info *fs_info = extent_root->fs_info;
	int ret;
	int slot;
	struct extent_buffer *leaf;
	struct btrfs_key key;

	while (1) {
		ret = btrfs_next_item(extent_root, path);
		if (ret < 0)
			break;
		if (ret) {
			ret = 0;
			break;
		}

		slot = path->slots[0];
		leaf = path->nodes[0];
		btrfs_item_key_to_cpu(leaf, &key, slot);

		if (key.objectid != ctx->bytenr)
			break;
		if (key.type < BTRFS_TREE_BLOCK_REF_KEY)
			continue;
		if (key.type > BTRFS_SHARED_DATA_REF_KEY)
			break;

		switch (key.type) {
		case BTRFS_SHARED_BLOCK_REF_KEY:
			 
			ret = add_direct_ref(fs_info, preftrees,
					     info_level + 1, key.offset,
					     ctx->bytenr, 1, NULL, GFP_NOFS);
			break;
		case BTRFS_SHARED_DATA_REF_KEY: {
			 
			struct btrfs_shared_data_ref *sdref;
			int count;

			sdref = btrfs_item_ptr(leaf, slot,
					      struct btrfs_shared_data_ref);
			count = btrfs_shared_data_ref_count(leaf, sdref);
			ret = add_direct_ref(fs_info, preftrees, 0,
					     key.offset, ctx->bytenr, count,
					     sc, GFP_NOFS);
			break;
		}
		case BTRFS_TREE_BLOCK_REF_KEY:
			 
			ret = add_indirect_ref(fs_info, preftrees, key.offset,
					       NULL, info_level + 1, ctx->bytenr,
					       1, NULL, GFP_NOFS);
			break;
		case BTRFS_EXTENT_DATA_REF_KEY: {
			 
			struct btrfs_extent_data_ref *dref;
			int count;
			u64 root;

			dref = btrfs_item_ptr(leaf, slot,
					      struct btrfs_extent_data_ref);
			count = btrfs_extent_data_ref_count(leaf, dref);
			key.objectid = btrfs_extent_data_ref_objectid(leaf,
								      dref);
			key.type = BTRFS_EXTENT_DATA_KEY;
			key.offset = btrfs_extent_data_ref_offset(leaf, dref);

			if (sc && key.objectid != sc->inum &&
			    !sc->have_delayed_delete_refs) {
				ret = BACKREF_FOUND_SHARED;
				break;
			}

			root = btrfs_extent_data_ref_root(leaf, dref);

			if (!ctx->skip_data_ref ||
			    !ctx->skip_data_ref(root, key.objectid, key.offset,
						ctx->user_ctx))
				ret = add_indirect_ref(fs_info, preftrees, root,
						       &key, 0, ctx->bytenr,
						       count, sc, GFP_NOFS);
			break;
		}
		default:
			WARN_ON(1);
		}
		if (ret)
			return ret;

	}

	return ret;
}

 
static bool lookup_backref_shared_cache(struct btrfs_backref_share_check_ctx *ctx,
					struct btrfs_root *root,
					u64 bytenr, int level, bool *is_shared)
{
	const struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_backref_shared_cache_entry *entry;

	if (!current->journal_info)
		lockdep_assert_held(&fs_info->commit_root_sem);

	if (!ctx->use_path_cache)
		return false;

	if (WARN_ON_ONCE(level >= BTRFS_MAX_LEVEL))
		return false;

	 
	ASSERT(level >= 0);

	entry = &ctx->path_cache_entries[level];

	 
	if (entry->bytenr != bytenr)
		return false;

	 
	if (!entry->is_shared &&
	    entry->gen != btrfs_root_last_snapshot(&root->root_item))
		return false;

	 
	if (entry->is_shared &&
	    entry->gen != btrfs_get_last_root_drop_gen(fs_info))
		return false;

	*is_shared = entry->is_shared;
	 
	if (*is_shared) {
		for (int i = 0; i < level; i++) {
			ctx->path_cache_entries[i].is_shared = true;
			ctx->path_cache_entries[i].gen = entry->gen;
		}
	}

	return true;
}

 
static void store_backref_shared_cache(struct btrfs_backref_share_check_ctx *ctx,
				       struct btrfs_root *root,
				       u64 bytenr, int level, bool is_shared)
{
	const struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_backref_shared_cache_entry *entry;
	u64 gen;

	if (!current->journal_info)
		lockdep_assert_held(&fs_info->commit_root_sem);

	if (!ctx->use_path_cache)
		return;

	if (WARN_ON_ONCE(level >= BTRFS_MAX_LEVEL))
		return;

	 
	ASSERT(level >= 0);

	if (is_shared)
		gen = btrfs_get_last_root_drop_gen(fs_info);
	else
		gen = btrfs_root_last_snapshot(&root->root_item);

	entry = &ctx->path_cache_entries[level];
	entry->bytenr = bytenr;
	entry->is_shared = is_shared;
	entry->gen = gen;

	 
	if (is_shared) {
		for (int i = 0; i < level; i++) {
			entry = &ctx->path_cache_entries[i];
			entry->is_shared = is_shared;
			entry->gen = gen;
		}
	}
}

 
static int find_parent_nodes(struct btrfs_backref_walk_ctx *ctx,
			     struct share_check *sc)
{
	struct btrfs_root *root = btrfs_extent_root(ctx->fs_info, ctx->bytenr);
	struct btrfs_key key;
	struct btrfs_path *path;
	struct btrfs_delayed_ref_root *delayed_refs = NULL;
	struct btrfs_delayed_ref_head *head;
	int info_level = 0;
	int ret;
	struct prelim_ref *ref;
	struct rb_node *node;
	struct extent_inode_elem *eie = NULL;
	struct preftrees preftrees = {
		.direct = PREFTREE_INIT,
		.indirect = PREFTREE_INIT,
		.indirect_missing_keys = PREFTREE_INIT
	};

	 
	if (sc)
		ASSERT(ctx->roots == NULL);

	key.objectid = ctx->bytenr;
	key.offset = (u64)-1;
	if (btrfs_fs_incompat(ctx->fs_info, SKINNY_METADATA))
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;

	path = btrfs_alloc_path();
	if (!path)
		return -ENOMEM;
	if (!ctx->trans) {
		path->search_commit_root = 1;
		path->skip_locking = 1;
	}

	if (ctx->time_seq == BTRFS_SEQ_LAST)
		path->skip_locking = 1;

again:
	head = NULL;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto out;
	if (ret == 0) {
		 
		ASSERT(ret != 0);
		ret = -EUCLEAN;
		goto out;
	}

	if (ctx->trans && likely(ctx->trans->type != __TRANS_DUMMY) &&
	    ctx->time_seq != BTRFS_SEQ_LAST) {
		 
		delayed_refs = &ctx->trans->transaction->delayed_refs;
		spin_lock(&delayed_refs->lock);
		head = btrfs_find_delayed_ref_head(delayed_refs, ctx->bytenr);
		if (head) {
			if (!mutex_trylock(&head->mutex)) {
				refcount_inc(&head->refs);
				spin_unlock(&delayed_refs->lock);

				btrfs_release_path(path);

				 
				mutex_lock(&head->mutex);
				mutex_unlock(&head->mutex);
				btrfs_put_delayed_ref_head(head);
				goto again;
			}
			spin_unlock(&delayed_refs->lock);
			ret = add_delayed_refs(ctx->fs_info, head, ctx->time_seq,
					       &preftrees, sc);
			mutex_unlock(&head->mutex);
			if (ret)
				goto out;
		} else {
			spin_unlock(&delayed_refs->lock);
		}
	}

	if (path->slots[0]) {
		struct extent_buffer *leaf;
		int slot;

		path->slots[0]--;
		leaf = path->nodes[0];
		slot = path->slots[0];
		btrfs_item_key_to_cpu(leaf, &key, slot);
		if (key.objectid == ctx->bytenr &&
		    (key.type == BTRFS_EXTENT_ITEM_KEY ||
		     key.type == BTRFS_METADATA_ITEM_KEY)) {
			ret = add_inline_refs(ctx, path, &info_level,
					      &preftrees, sc);
			if (ret)
				goto out;
			ret = add_keyed_refs(ctx, root, path, info_level,
					     &preftrees, sc);
			if (ret)
				goto out;
		}
	}

	 
	ASSERT(extent_is_shared(sc) == 0);

	 
	if (sc && ctx->bytenr == sc->data_bytenr) {
		 
		if (sc->data_extent_gen >
		    btrfs_root_last_snapshot(&sc->root->root_item)) {
			ret = BACKREF_FOUND_NOT_SHARED;
			goto out;
		}

		 
		if (sc->ctx->curr_leaf_bytenr == sc->ctx->prev_leaf_bytenr &&
		    sc->self_ref_count == 1) {
			bool cached;
			bool is_shared;

			cached = lookup_backref_shared_cache(sc->ctx, sc->root,
						     sc->ctx->curr_leaf_bytenr,
						     0, &is_shared);
			if (cached) {
				if (is_shared)
					ret = BACKREF_FOUND_SHARED;
				else
					ret = BACKREF_FOUND_NOT_SHARED;
				goto out;
			}
		}
	}

	btrfs_release_path(path);

	ret = add_missing_keys(ctx->fs_info, &preftrees, path->skip_locking == 0);
	if (ret)
		goto out;

	WARN_ON(!RB_EMPTY_ROOT(&preftrees.indirect_missing_keys.root.rb_root));

	ret = resolve_indirect_refs(ctx, path, &preftrees, sc);
	if (ret)
		goto out;

	WARN_ON(!RB_EMPTY_ROOT(&preftrees.indirect.root.rb_root));

	 
	node = rb_first_cached(&preftrees.direct.root);
	while (node) {
		ref = rb_entry(node, struct prelim_ref, rbnode);
		node = rb_next(&ref->rbnode);
		 
		if (ctx->roots && ref->count && ref->root_id && ref->parent == 0) {
			 
			ret = ulist_add(ctx->roots, ref->root_id, 0, GFP_NOFS);
			if (ret < 0)
				goto out;
		}
		if (ref->count && ref->parent) {
			if (!ctx->skip_inode_ref_list && !ref->inode_list &&
			    ref->level == 0) {
				struct btrfs_tree_parent_check check = { 0 };
				struct extent_buffer *eb;

				check.level = ref->level;

				eb = read_tree_block(ctx->fs_info, ref->parent,
						     &check);
				if (IS_ERR(eb)) {
					ret = PTR_ERR(eb);
					goto out;
				}
				if (!extent_buffer_uptodate(eb)) {
					free_extent_buffer(eb);
					ret = -EIO;
					goto out;
				}

				if (!path->skip_locking)
					btrfs_tree_read_lock(eb);
				ret = find_extent_in_eb(ctx, eb, &eie);
				if (!path->skip_locking)
					btrfs_tree_read_unlock(eb);
				free_extent_buffer(eb);
				if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP ||
				    ret < 0)
					goto out;
				ref->inode_list = eie;
				 
				eie = NULL;
			}
			ret = ulist_add_merge_ptr(ctx->refs, ref->parent,
						  ref->inode_list,
						  (void **)&eie, GFP_NOFS);
			if (ret < 0)
				goto out;
			if (!ret && !ctx->skip_inode_ref_list) {
				 
				ASSERT(eie);
				if (!eie) {
					ret = -EUCLEAN;
					goto out;
				}
				while (eie->next)
					eie = eie->next;
				eie->next = ref->inode_list;
			}
			eie = NULL;
			 
			ref->inode_list = NULL;
		}
		cond_resched();
	}

out:
	btrfs_free_path(path);

	prelim_release(&preftrees.direct);
	prelim_release(&preftrees.indirect);
	prelim_release(&preftrees.indirect_missing_keys);

	if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP || ret < 0)
		free_inode_elem_list(eie);
	return ret;
}

 
int btrfs_find_all_leafs(struct btrfs_backref_walk_ctx *ctx)
{
	int ret;

	ASSERT(ctx->refs == NULL);

	ctx->refs = ulist_alloc(GFP_NOFS);
	if (!ctx->refs)
		return -ENOMEM;

	ret = find_parent_nodes(ctx, NULL);
	if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP ||
	    (ret < 0 && ret != -ENOENT)) {
		free_leaf_list(ctx->refs);
		ctx->refs = NULL;
		return ret;
	}

	return 0;
}

 
static int btrfs_find_all_roots_safe(struct btrfs_backref_walk_ctx *ctx)
{
	const u64 orig_bytenr = ctx->bytenr;
	const bool orig_skip_inode_ref_list = ctx->skip_inode_ref_list;
	bool roots_ulist_allocated = false;
	struct ulist_iterator uiter;
	int ret = 0;

	ASSERT(ctx->refs == NULL);

	ctx->refs = ulist_alloc(GFP_NOFS);
	if (!ctx->refs)
		return -ENOMEM;

	if (!ctx->roots) {
		ctx->roots = ulist_alloc(GFP_NOFS);
		if (!ctx->roots) {
			ulist_free(ctx->refs);
			ctx->refs = NULL;
			return -ENOMEM;
		}
		roots_ulist_allocated = true;
	}

	ctx->skip_inode_ref_list = true;

	ULIST_ITER_INIT(&uiter);
	while (1) {
		struct ulist_node *node;

		ret = find_parent_nodes(ctx, NULL);
		if (ret < 0 && ret != -ENOENT) {
			if (roots_ulist_allocated) {
				ulist_free(ctx->roots);
				ctx->roots = NULL;
			}
			break;
		}
		ret = 0;
		node = ulist_next(ctx->refs, &uiter);
		if (!node)
			break;
		ctx->bytenr = node->val;
		cond_resched();
	}

	ulist_free(ctx->refs);
	ctx->refs = NULL;
	ctx->bytenr = orig_bytenr;
	ctx->skip_inode_ref_list = orig_skip_inode_ref_list;

	return ret;
}

int btrfs_find_all_roots(struct btrfs_backref_walk_ctx *ctx,
			 bool skip_commit_root_sem)
{
	int ret;

	if (!ctx->trans && !skip_commit_root_sem)
		down_read(&ctx->fs_info->commit_root_sem);
	ret = btrfs_find_all_roots_safe(ctx);
	if (!ctx->trans && !skip_commit_root_sem)
		up_read(&ctx->fs_info->commit_root_sem);
	return ret;
}

struct btrfs_backref_share_check_ctx *btrfs_alloc_backref_share_check_ctx(void)
{
	struct btrfs_backref_share_check_ctx *ctx;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return NULL;

	ulist_init(&ctx->refs);

	return ctx;
}

void btrfs_free_backref_share_ctx(struct btrfs_backref_share_check_ctx *ctx)
{
	if (!ctx)
		return;

	ulist_release(&ctx->refs);
	kfree(ctx);
}

 
int btrfs_is_data_extent_shared(struct btrfs_inode *inode, u64 bytenr,
				u64 extent_gen,
				struct btrfs_backref_share_check_ctx *ctx)
{
	struct btrfs_backref_walk_ctx walk_ctx = { 0 };
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct btrfs_trans_handle *trans;
	struct ulist_iterator uiter;
	struct ulist_node *node;
	struct btrfs_seq_list elem = BTRFS_SEQ_LIST_INIT(elem);
	int ret = 0;
	struct share_check shared = {
		.ctx = ctx,
		.root = root,
		.inum = btrfs_ino(inode),
		.data_bytenr = bytenr,
		.data_extent_gen = extent_gen,
		.share_count = 0,
		.self_ref_count = 0,
		.have_delayed_delete_refs = false,
	};
	int level;
	bool leaf_cached;
	bool leaf_is_shared;

	for (int i = 0; i < BTRFS_BACKREF_CTX_PREV_EXTENTS_SIZE; i++) {
		if (ctx->prev_extents_cache[i].bytenr == bytenr)
			return ctx->prev_extents_cache[i].is_shared;
	}

	ulist_init(&ctx->refs);

	trans = btrfs_join_transaction_nostart(root);
	if (IS_ERR(trans)) {
		if (PTR_ERR(trans) != -ENOENT && PTR_ERR(trans) != -EROFS) {
			ret = PTR_ERR(trans);
			goto out;
		}
		trans = NULL;
		down_read(&fs_info->commit_root_sem);
	} else {
		btrfs_get_tree_mod_seq(fs_info, &elem);
		walk_ctx.time_seq = elem.seq;
	}

	ctx->use_path_cache = true;

	 
	leaf_cached = lookup_backref_shared_cache(ctx, root,
						  ctx->curr_leaf_bytenr, 0,
						  &leaf_is_shared);
	if (leaf_cached && leaf_is_shared) {
		ret = 1;
		goto out_trans;
	}

	walk_ctx.skip_inode_ref_list = true;
	walk_ctx.trans = trans;
	walk_ctx.fs_info = fs_info;
	walk_ctx.refs = &ctx->refs;

	 
	level = -1;
	ULIST_ITER_INIT(&uiter);
	while (1) {
		const unsigned long prev_ref_count = ctx->refs.nnodes;

		walk_ctx.bytenr = bytenr;
		ret = find_parent_nodes(&walk_ctx, &shared);
		if (ret == BACKREF_FOUND_SHARED ||
		    ret == BACKREF_FOUND_NOT_SHARED) {
			 
			ret = (ret == BACKREF_FOUND_SHARED) ? 1 : 0;
			if (level >= 0)
				store_backref_shared_cache(ctx, root, bytenr,
							   level, ret == 1);
			break;
		}
		if (ret < 0 && ret != -ENOENT)
			break;
		ret = 0;

		 
		if ((ctx->refs.nnodes - prev_ref_count) > 1)
			ctx->use_path_cache = false;

		if (level >= 0)
			store_backref_shared_cache(ctx, root, bytenr,
						   level, false);
		node = ulist_next(&ctx->refs, &uiter);
		if (!node)
			break;
		bytenr = node->val;
		if (ctx->use_path_cache) {
			bool is_shared;
			bool cached;

			level++;
			cached = lookup_backref_shared_cache(ctx, root, bytenr,
							     level, &is_shared);
			if (cached) {
				ret = (is_shared ? 1 : 0);
				break;
			}
		}
		shared.share_count = 0;
		shared.have_delayed_delete_refs = false;
		cond_resched();
	}

	 
	if (!ctx->use_path_cache) {
		int i = 0;

		level--;
		if (ret >= 0 && level >= 0) {
			bytenr = ctx->path_cache_entries[level].bytenr;
			ctx->use_path_cache = true;
			store_backref_shared_cache(ctx, root, bytenr, level, ret);
			i = level + 1;
		}

		for ( ; i < BTRFS_MAX_LEVEL; i++)
			ctx->path_cache_entries[i].bytenr = 0;
	}

	 
	if (ret >= 0 && shared.self_ref_count > 1) {
		int slot = ctx->prev_extents_cache_slot;

		ctx->prev_extents_cache[slot].bytenr = shared.data_bytenr;
		ctx->prev_extents_cache[slot].is_shared = (ret == 1);

		slot = (slot + 1) % BTRFS_BACKREF_CTX_PREV_EXTENTS_SIZE;
		ctx->prev_extents_cache_slot = slot;
	}

out_trans:
	if (trans) {
		btrfs_put_tree_mod_seq(fs_info, &elem);
		btrfs_end_transaction(trans);
	} else {
		up_read(&fs_info->commit_root_sem);
	}
out:
	ulist_release(&ctx->refs);
	ctx->prev_leaf_bytenr = ctx->curr_leaf_bytenr;

	return ret;
}

int btrfs_find_one_extref(struct btrfs_root *root, u64 inode_objectid,
			  u64 start_off, struct btrfs_path *path,
			  struct btrfs_inode_extref **ret_extref,
			  u64 *found_off)
{
	int ret, slot;
	struct btrfs_key key;
	struct btrfs_key found_key;
	struct btrfs_inode_extref *extref;
	const struct extent_buffer *leaf;
	unsigned long ptr;

	key.objectid = inode_objectid;
	key.type = BTRFS_INODE_EXTREF_KEY;
	key.offset = start_off;

	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	while (1) {
		leaf = path->nodes[0];
		slot = path->slots[0];
		if (slot >= btrfs_header_nritems(leaf)) {
			 
			ret = btrfs_next_leaf(root, path);
			if (ret) {
				if (ret >= 1)
					ret = -ENOENT;
				break;
			}
			continue;
		}

		btrfs_item_key_to_cpu(leaf, &found_key, slot);

		 
		ret = -ENOENT;
		if (found_key.objectid != inode_objectid)
			break;
		if (found_key.type != BTRFS_INODE_EXTREF_KEY)
			break;

		ret = 0;
		ptr = btrfs_item_ptr_offset(leaf, path->slots[0]);
		extref = (struct btrfs_inode_extref *)ptr;
		*ret_extref = extref;
		if (found_off)
			*found_off = found_key.offset;
		break;
	}

	return ret;
}

 
char *btrfs_ref_to_path(struct btrfs_root *fs_root, struct btrfs_path *path,
			u32 name_len, unsigned long name_off,
			struct extent_buffer *eb_in, u64 parent,
			char *dest, u32 size)
{
	int slot;
	u64 next_inum;
	int ret;
	s64 bytes_left = ((s64)size) - 1;
	struct extent_buffer *eb = eb_in;
	struct btrfs_key found_key;
	struct btrfs_inode_ref *iref;

	if (bytes_left >= 0)
		dest[bytes_left] = '\0';

	while (1) {
		bytes_left -= name_len;
		if (bytes_left >= 0)
			read_extent_buffer(eb, dest + bytes_left,
					   name_off, name_len);
		if (eb != eb_in) {
			if (!path->skip_locking)
				btrfs_tree_read_unlock(eb);
			free_extent_buffer(eb);
		}
		ret = btrfs_find_item(fs_root, path, parent, 0,
				BTRFS_INODE_REF_KEY, &found_key);
		if (ret > 0)
			ret = -ENOENT;
		if (ret)
			break;

		next_inum = found_key.offset;

		 
		if (parent == next_inum)
			break;

		slot = path->slots[0];
		eb = path->nodes[0];
		 
		if (eb != eb_in) {
			path->nodes[0] = NULL;
			path->locks[0] = 0;
		}
		btrfs_release_path(path);
		iref = btrfs_item_ptr(eb, slot, struct btrfs_inode_ref);

		name_len = btrfs_inode_ref_name_len(eb, iref);
		name_off = (unsigned long)(iref + 1);

		parent = next_inum;
		--bytes_left;
		if (bytes_left >= 0)
			dest[bytes_left] = '/';
	}

	btrfs_release_path(path);

	if (ret)
		return ERR_PTR(ret);

	return dest + bytes_left;
}

 
int extent_from_logical(struct btrfs_fs_info *fs_info, u64 logical,
			struct btrfs_path *path, struct btrfs_key *found_key,
			u64 *flags_ret)
{
	struct btrfs_root *extent_root = btrfs_extent_root(fs_info, logical);
	int ret;
	u64 flags;
	u64 size = 0;
	u32 item_size;
	const struct extent_buffer *eb;
	struct btrfs_extent_item *ei;
	struct btrfs_key key;

	if (btrfs_fs_incompat(fs_info, SKINNY_METADATA))
		key.type = BTRFS_METADATA_ITEM_KEY;
	else
		key.type = BTRFS_EXTENT_ITEM_KEY;
	key.objectid = logical;
	key.offset = (u64)-1;

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		return ret;

	ret = btrfs_previous_extent_item(extent_root, path, 0);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		return ret;
	}
	btrfs_item_key_to_cpu(path->nodes[0], found_key, path->slots[0]);
	if (found_key->type == BTRFS_METADATA_ITEM_KEY)
		size = fs_info->nodesize;
	else if (found_key->type == BTRFS_EXTENT_ITEM_KEY)
		size = found_key->offset;

	if (found_key->objectid > logical ||
	    found_key->objectid + size <= logical) {
		btrfs_debug(fs_info,
			"logical %llu is not within any extent", logical);
		return -ENOENT;
	}

	eb = path->nodes[0];
	item_size = btrfs_item_size(eb, path->slots[0]);
	BUG_ON(item_size < sizeof(*ei));

	ei = btrfs_item_ptr(eb, path->slots[0], struct btrfs_extent_item);
	flags = btrfs_extent_flags(eb, ei);

	btrfs_debug(fs_info,
		"logical %llu is at position %llu within the extent (%llu EXTENT_ITEM %llu) flags %#llx size %u",
		 logical, logical - found_key->objectid, found_key->objectid,
		 found_key->offset, flags, item_size);

	WARN_ON(!flags_ret);
	if (flags_ret) {
		if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK)
			*flags_ret = BTRFS_EXTENT_FLAG_TREE_BLOCK;
		else if (flags & BTRFS_EXTENT_FLAG_DATA)
			*flags_ret = BTRFS_EXTENT_FLAG_DATA;
		else
			BUG();
		return 0;
	}

	return -EIO;
}

 
static int get_extent_inline_ref(unsigned long *ptr,
				 const struct extent_buffer *eb,
				 const struct btrfs_key *key,
				 const struct btrfs_extent_item *ei,
				 u32 item_size,
				 struct btrfs_extent_inline_ref **out_eiref,
				 int *out_type)
{
	unsigned long end;
	u64 flags;
	struct btrfs_tree_block_info *info;

	if (!*ptr) {
		 
		flags = btrfs_extent_flags(eb, ei);
		if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK) {
			if (key->type == BTRFS_METADATA_ITEM_KEY) {
				 
				*out_eiref =
				     (struct btrfs_extent_inline_ref *)(ei + 1);
			} else {
				WARN_ON(key->type != BTRFS_EXTENT_ITEM_KEY);
				info = (struct btrfs_tree_block_info *)(ei + 1);
				*out_eiref =
				   (struct btrfs_extent_inline_ref *)(info + 1);
			}
		} else {
			*out_eiref = (struct btrfs_extent_inline_ref *)(ei + 1);
		}
		*ptr = (unsigned long)*out_eiref;
		if ((unsigned long)(*ptr) >= (unsigned long)ei + item_size)
			return -ENOENT;
	}

	end = (unsigned long)ei + item_size;
	*out_eiref = (struct btrfs_extent_inline_ref *)(*ptr);
	*out_type = btrfs_get_extent_inline_ref_type(eb, *out_eiref,
						     BTRFS_REF_TYPE_ANY);
	if (*out_type == BTRFS_REF_TYPE_INVALID)
		return -EUCLEAN;

	*ptr += btrfs_extent_inline_ref_size(*out_type);
	WARN_ON(*ptr > end);
	if (*ptr == end)
		return 1;  

	return 0;
}

 
int tree_backref_for_extent(unsigned long *ptr, struct extent_buffer *eb,
			    struct btrfs_key *key, struct btrfs_extent_item *ei,
			    u32 item_size, u64 *out_root, u8 *out_level)
{
	int ret;
	int type;
	struct btrfs_extent_inline_ref *eiref;

	if (*ptr == (unsigned long)-1)
		return 1;

	while (1) {
		ret = get_extent_inline_ref(ptr, eb, key, ei, item_size,
					      &eiref, &type);
		if (ret < 0)
			return ret;

		if (type == BTRFS_TREE_BLOCK_REF_KEY ||
		    type == BTRFS_SHARED_BLOCK_REF_KEY)
			break;

		if (ret == 1)
			return 1;
	}

	 
	*out_root = btrfs_extent_inline_ref_offset(eb, eiref);

	if (key->type == BTRFS_EXTENT_ITEM_KEY) {
		struct btrfs_tree_block_info *info;

		info = (struct btrfs_tree_block_info *)(ei + 1);
		*out_level = btrfs_tree_block_level(eb, info);
	} else {
		ASSERT(key->type == BTRFS_METADATA_ITEM_KEY);
		*out_level = (u8)key->offset;
	}

	if (ret == 1)
		*ptr = (unsigned long)-1;

	return 0;
}

static int iterate_leaf_refs(struct btrfs_fs_info *fs_info,
			     struct extent_inode_elem *inode_list,
			     u64 root, u64 extent_item_objectid,
			     iterate_extent_inodes_t *iterate, void *ctx)
{
	struct extent_inode_elem *eie;
	int ret = 0;

	for (eie = inode_list; eie; eie = eie->next) {
		btrfs_debug(fs_info,
			    "ref for %llu resolved, key (%llu EXTEND_DATA %llu), root %llu",
			    extent_item_objectid, eie->inum,
			    eie->offset, root);
		ret = iterate(eie->inum, eie->offset, eie->num_bytes, root, ctx);
		if (ret) {
			btrfs_debug(fs_info,
				    "stopping iteration for %llu due to ret=%d",
				    extent_item_objectid, ret);
			break;
		}
	}

	return ret;
}

 
int iterate_extent_inodes(struct btrfs_backref_walk_ctx *ctx,
			  bool search_commit_root,
			  iterate_extent_inodes_t *iterate, void *user_ctx)
{
	int ret;
	struct ulist *refs;
	struct ulist_node *ref_node;
	struct btrfs_seq_list seq_elem = BTRFS_SEQ_LIST_INIT(seq_elem);
	struct ulist_iterator ref_uiter;

	btrfs_debug(ctx->fs_info, "resolving all inodes for extent %llu",
		    ctx->bytenr);

	ASSERT(ctx->trans == NULL);
	ASSERT(ctx->roots == NULL);

	if (!search_commit_root) {
		struct btrfs_trans_handle *trans;

		trans = btrfs_attach_transaction(ctx->fs_info->tree_root);
		if (IS_ERR(trans)) {
			if (PTR_ERR(trans) != -ENOENT &&
			    PTR_ERR(trans) != -EROFS)
				return PTR_ERR(trans);
			trans = NULL;
		}
		ctx->trans = trans;
	}

	if (ctx->trans) {
		btrfs_get_tree_mod_seq(ctx->fs_info, &seq_elem);
		ctx->time_seq = seq_elem.seq;
	} else {
		down_read(&ctx->fs_info->commit_root_sem);
	}

	ret = btrfs_find_all_leafs(ctx);
	if (ret)
		goto out;
	refs = ctx->refs;
	ctx->refs = NULL;

	ULIST_ITER_INIT(&ref_uiter);
	while (!ret && (ref_node = ulist_next(refs, &ref_uiter))) {
		const u64 leaf_bytenr = ref_node->val;
		struct ulist_node *root_node;
		struct ulist_iterator root_uiter;
		struct extent_inode_elem *inode_list;

		inode_list = (struct extent_inode_elem *)(uintptr_t)ref_node->aux;

		if (ctx->cache_lookup) {
			const u64 *root_ids;
			int root_count;
			bool cached;

			cached = ctx->cache_lookup(leaf_bytenr, ctx->user_ctx,
						   &root_ids, &root_count);
			if (cached) {
				for (int i = 0; i < root_count; i++) {
					ret = iterate_leaf_refs(ctx->fs_info,
								inode_list,
								root_ids[i],
								leaf_bytenr,
								iterate,
								user_ctx);
					if (ret)
						break;
				}
				continue;
			}
		}

		if (!ctx->roots) {
			ctx->roots = ulist_alloc(GFP_NOFS);
			if (!ctx->roots) {
				ret = -ENOMEM;
				break;
			}
		}

		ctx->bytenr = leaf_bytenr;
		ret = btrfs_find_all_roots_safe(ctx);
		if (ret)
			break;

		if (ctx->cache_store)
			ctx->cache_store(leaf_bytenr, ctx->roots, ctx->user_ctx);

		ULIST_ITER_INIT(&root_uiter);
		while (!ret && (root_node = ulist_next(ctx->roots, &root_uiter))) {
			btrfs_debug(ctx->fs_info,
				    "root %llu references leaf %llu, data list %#llx",
				    root_node->val, ref_node->val,
				    ref_node->aux);
			ret = iterate_leaf_refs(ctx->fs_info, inode_list,
						root_node->val, ctx->bytenr,
						iterate, user_ctx);
		}
		ulist_reinit(ctx->roots);
	}

	free_leaf_list(refs);
out:
	if (ctx->trans) {
		btrfs_put_tree_mod_seq(ctx->fs_info, &seq_elem);
		btrfs_end_transaction(ctx->trans);
		ctx->trans = NULL;
	} else {
		up_read(&ctx->fs_info->commit_root_sem);
	}

	ulist_free(ctx->roots);
	ctx->roots = NULL;

	if (ret == BTRFS_ITERATE_EXTENT_INODES_STOP)
		ret = 0;

	return ret;
}

static int build_ino_list(u64 inum, u64 offset, u64 num_bytes, u64 root, void *ctx)
{
	struct btrfs_data_container *inodes = ctx;
	const size_t c = 3 * sizeof(u64);

	if (inodes->bytes_left >= c) {
		inodes->bytes_left -= c;
		inodes->val[inodes->elem_cnt] = inum;
		inodes->val[inodes->elem_cnt + 1] = offset;
		inodes->val[inodes->elem_cnt + 2] = root;
		inodes->elem_cnt += 3;
	} else {
		inodes->bytes_missing += c - inodes->bytes_left;
		inodes->bytes_left = 0;
		inodes->elem_missed += 3;
	}

	return 0;
}

int iterate_inodes_from_logical(u64 logical, struct btrfs_fs_info *fs_info,
				struct btrfs_path *path,
				void *ctx, bool ignore_offset)
{
	struct btrfs_backref_walk_ctx walk_ctx = { 0 };
	int ret;
	u64 flags = 0;
	struct btrfs_key found_key;
	int search_commit_root = path->search_commit_root;

	ret = extent_from_logical(fs_info, logical, path, &found_key, &flags);
	btrfs_release_path(path);
	if (ret < 0)
		return ret;
	if (flags & BTRFS_EXTENT_FLAG_TREE_BLOCK)
		return -EINVAL;

	walk_ctx.bytenr = found_key.objectid;
	if (ignore_offset)
		walk_ctx.ignore_extent_item_pos = true;
	else
		walk_ctx.extent_item_pos = logical - found_key.objectid;
	walk_ctx.fs_info = fs_info;

	return iterate_extent_inodes(&walk_ctx, search_commit_root,
				     build_ino_list, ctx);
}

static int inode_to_path(u64 inum, u32 name_len, unsigned long name_off,
			 struct extent_buffer *eb, struct inode_fs_paths *ipath);

static int iterate_inode_refs(u64 inum, struct inode_fs_paths *ipath)
{
	int ret = 0;
	int slot;
	u32 cur;
	u32 len;
	u32 name_len;
	u64 parent = 0;
	int found = 0;
	struct btrfs_root *fs_root = ipath->fs_root;
	struct btrfs_path *path = ipath->btrfs_path;
	struct extent_buffer *eb;
	struct btrfs_inode_ref *iref;
	struct btrfs_key found_key;

	while (!ret) {
		ret = btrfs_find_item(fs_root, path, inum,
				parent ? parent + 1 : 0, BTRFS_INODE_REF_KEY,
				&found_key);

		if (ret < 0)
			break;
		if (ret) {
			ret = found ? 0 : -ENOENT;
			break;
		}
		++found;

		parent = found_key.offset;
		slot = path->slots[0];
		eb = btrfs_clone_extent_buffer(path->nodes[0]);
		if (!eb) {
			ret = -ENOMEM;
			break;
		}
		btrfs_release_path(path);

		iref = btrfs_item_ptr(eb, slot, struct btrfs_inode_ref);

		for (cur = 0; cur < btrfs_item_size(eb, slot); cur += len) {
			name_len = btrfs_inode_ref_name_len(eb, iref);
			 
			btrfs_debug(fs_root->fs_info,
				"following ref at offset %u for inode %llu in tree %llu",
				cur, found_key.objectid,
				fs_root->root_key.objectid);
			ret = inode_to_path(parent, name_len,
				      (unsigned long)(iref + 1), eb, ipath);
			if (ret)
				break;
			len = sizeof(*iref) + name_len;
			iref = (struct btrfs_inode_ref *)((char *)iref + len);
		}
		free_extent_buffer(eb);
	}

	btrfs_release_path(path);

	return ret;
}

static int iterate_inode_extrefs(u64 inum, struct inode_fs_paths *ipath)
{
	int ret;
	int slot;
	u64 offset = 0;
	u64 parent;
	int found = 0;
	struct btrfs_root *fs_root = ipath->fs_root;
	struct btrfs_path *path = ipath->btrfs_path;
	struct extent_buffer *eb;
	struct btrfs_inode_extref *extref;
	u32 item_size;
	u32 cur_offset;
	unsigned long ptr;

	while (1) {
		ret = btrfs_find_one_extref(fs_root, inum, offset, path, &extref,
					    &offset);
		if (ret < 0)
			break;
		if (ret) {
			ret = found ? 0 : -ENOENT;
			break;
		}
		++found;

		slot = path->slots[0];
		eb = btrfs_clone_extent_buffer(path->nodes[0]);
		if (!eb) {
			ret = -ENOMEM;
			break;
		}
		btrfs_release_path(path);

		item_size = btrfs_item_size(eb, slot);
		ptr = btrfs_item_ptr_offset(eb, slot);
		cur_offset = 0;

		while (cur_offset < item_size) {
			u32 name_len;

			extref = (struct btrfs_inode_extref *)(ptr + cur_offset);
			parent = btrfs_inode_extref_parent(eb, extref);
			name_len = btrfs_inode_extref_name_len(eb, extref);
			ret = inode_to_path(parent, name_len,
				      (unsigned long)&extref->name, eb, ipath);
			if (ret)
				break;

			cur_offset += btrfs_inode_extref_name_len(eb, extref);
			cur_offset += sizeof(*extref);
		}
		free_extent_buffer(eb);

		offset++;
	}

	btrfs_release_path(path);

	return ret;
}

 
static int inode_to_path(u64 inum, u32 name_len, unsigned long name_off,
			 struct extent_buffer *eb, struct inode_fs_paths *ipath)
{
	char *fspath;
	char *fspath_min;
	int i = ipath->fspath->elem_cnt;
	const int s_ptr = sizeof(char *);
	u32 bytes_left;

	bytes_left = ipath->fspath->bytes_left > s_ptr ?
					ipath->fspath->bytes_left - s_ptr : 0;

	fspath_min = (char *)ipath->fspath->val + (i + 1) * s_ptr;
	fspath = btrfs_ref_to_path(ipath->fs_root, ipath->btrfs_path, name_len,
				   name_off, eb, inum, fspath_min, bytes_left);
	if (IS_ERR(fspath))
		return PTR_ERR(fspath);

	if (fspath > fspath_min) {
		ipath->fspath->val[i] = (u64)(unsigned long)fspath;
		++ipath->fspath->elem_cnt;
		ipath->fspath->bytes_left = fspath - fspath_min;
	} else {
		++ipath->fspath->elem_missed;
		ipath->fspath->bytes_missing += fspath_min - fspath;
		ipath->fspath->bytes_left = 0;
	}

	return 0;
}

 
int paths_from_inode(u64 inum, struct inode_fs_paths *ipath)
{
	int ret;
	int found_refs = 0;

	ret = iterate_inode_refs(inum, ipath);
	if (!ret)
		++found_refs;
	else if (ret != -ENOENT)
		return ret;

	ret = iterate_inode_extrefs(inum, ipath);
	if (ret == -ENOENT && found_refs)
		return 0;

	return ret;
}

struct btrfs_data_container *init_data_container(u32 total_bytes)
{
	struct btrfs_data_container *data;
	size_t alloc_bytes;

	alloc_bytes = max_t(size_t, total_bytes, sizeof(*data));
	data = kvmalloc(alloc_bytes, GFP_KERNEL);
	if (!data)
		return ERR_PTR(-ENOMEM);

	if (total_bytes >= sizeof(*data)) {
		data->bytes_left = total_bytes - sizeof(*data);
		data->bytes_missing = 0;
	} else {
		data->bytes_missing = sizeof(*data) - total_bytes;
		data->bytes_left = 0;
	}

	data->elem_cnt = 0;
	data->elem_missed = 0;

	return data;
}

 
struct inode_fs_paths *init_ipath(s32 total_bytes, struct btrfs_root *fs_root,
					struct btrfs_path *path)
{
	struct inode_fs_paths *ifp;
	struct btrfs_data_container *fspath;

	fspath = init_data_container(total_bytes);
	if (IS_ERR(fspath))
		return ERR_CAST(fspath);

	ifp = kmalloc(sizeof(*ifp), GFP_KERNEL);
	if (!ifp) {
		kvfree(fspath);
		return ERR_PTR(-ENOMEM);
	}

	ifp->btrfs_path = path;
	ifp->fspath = fspath;
	ifp->fs_root = fs_root;

	return ifp;
}

void free_ipath(struct inode_fs_paths *ipath)
{
	if (!ipath)
		return;
	kvfree(ipath->fspath);
	kfree(ipath);
}

struct btrfs_backref_iter *btrfs_backref_iter_alloc(struct btrfs_fs_info *fs_info)
{
	struct btrfs_backref_iter *ret;

	ret = kzalloc(sizeof(*ret), GFP_NOFS);
	if (!ret)
		return NULL;

	ret->path = btrfs_alloc_path();
	if (!ret->path) {
		kfree(ret);
		return NULL;
	}

	 
	ret->path->search_commit_root = 1;
	ret->path->skip_locking = 1;
	ret->fs_info = fs_info;

	return ret;
}

int btrfs_backref_iter_start(struct btrfs_backref_iter *iter, u64 bytenr)
{
	struct btrfs_fs_info *fs_info = iter->fs_info;
	struct btrfs_root *extent_root = btrfs_extent_root(fs_info, bytenr);
	struct btrfs_path *path = iter->path;
	struct btrfs_extent_item *ei;
	struct btrfs_key key;
	int ret;

	key.objectid = bytenr;
	key.type = BTRFS_METADATA_ITEM_KEY;
	key.offset = (u64)-1;
	iter->bytenr = bytenr;

	ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
	if (ret < 0)
		return ret;
	if (ret == 0) {
		ret = -EUCLEAN;
		goto release;
	}
	if (path->slots[0] == 0) {
		WARN_ON(IS_ENABLED(CONFIG_BTRFS_DEBUG));
		ret = -EUCLEAN;
		goto release;
	}
	path->slots[0]--;

	btrfs_item_key_to_cpu(path->nodes[0], &key, path->slots[0]);
	if ((key.type != BTRFS_EXTENT_ITEM_KEY &&
	     key.type != BTRFS_METADATA_ITEM_KEY) || key.objectid != bytenr) {
		ret = -ENOENT;
		goto release;
	}
	memcpy(&iter->cur_key, &key, sizeof(key));
	iter->item_ptr = (u32)btrfs_item_ptr_offset(path->nodes[0],
						    path->slots[0]);
	iter->end_ptr = (u32)(iter->item_ptr +
			btrfs_item_size(path->nodes[0], path->slots[0]));
	ei = btrfs_item_ptr(path->nodes[0], path->slots[0],
			    struct btrfs_extent_item);

	 
	if (btrfs_extent_flags(path->nodes[0], ei) & BTRFS_EXTENT_FLAG_DATA) {
		ret = -ENOTSUPP;
		goto release;
	}
	iter->cur_ptr = (u32)(iter->item_ptr + sizeof(*ei));

	 
	if (iter->cur_ptr >= iter->end_ptr) {
		ret = btrfs_next_item(extent_root, path);

		 
		if (ret > 0) {
			ret = -ENOENT;
			goto release;
		}
		if (ret < 0)
			goto release;

		btrfs_item_key_to_cpu(path->nodes[0], &iter->cur_key,
				path->slots[0]);
		if (iter->cur_key.objectid != bytenr ||
		    (iter->cur_key.type != BTRFS_SHARED_BLOCK_REF_KEY &&
		     iter->cur_key.type != BTRFS_TREE_BLOCK_REF_KEY)) {
			ret = -ENOENT;
			goto release;
		}
		iter->cur_ptr = (u32)btrfs_item_ptr_offset(path->nodes[0],
							   path->slots[0]);
		iter->item_ptr = iter->cur_ptr;
		iter->end_ptr = (u32)(iter->item_ptr + btrfs_item_size(
				      path->nodes[0], path->slots[0]));
	}

	return 0;
release:
	btrfs_backref_iter_release(iter);
	return ret;
}

 
int btrfs_backref_iter_next(struct btrfs_backref_iter *iter)
{
	struct extent_buffer *eb = btrfs_backref_get_eb(iter);
	struct btrfs_root *extent_root;
	struct btrfs_path *path = iter->path;
	struct btrfs_extent_inline_ref *iref;
	int ret;
	u32 size;

	if (btrfs_backref_iter_is_inline_ref(iter)) {
		 
		ASSERT(iter->cur_ptr < iter->end_ptr);

		if (btrfs_backref_has_tree_block_info(iter)) {
			 
			size = sizeof(struct btrfs_tree_block_info);
		} else {
			 
			int type;

			iref = (struct btrfs_extent_inline_ref *)
				((unsigned long)iter->cur_ptr);
			type = btrfs_extent_inline_ref_type(eb, iref);

			size = btrfs_extent_inline_ref_size(type);
		}
		iter->cur_ptr += size;
		if (iter->cur_ptr < iter->end_ptr)
			return 0;

		 
	}

	 
	extent_root = btrfs_extent_root(iter->fs_info, iter->bytenr);
	ret = btrfs_next_item(extent_root, iter->path);
	if (ret)
		return ret;

	btrfs_item_key_to_cpu(path->nodes[0], &iter->cur_key, path->slots[0]);
	if (iter->cur_key.objectid != iter->bytenr ||
	    (iter->cur_key.type != BTRFS_TREE_BLOCK_REF_KEY &&
	     iter->cur_key.type != BTRFS_SHARED_BLOCK_REF_KEY))
		return 1;
	iter->item_ptr = (u32)btrfs_item_ptr_offset(path->nodes[0],
					path->slots[0]);
	iter->cur_ptr = iter->item_ptr;
	iter->end_ptr = iter->item_ptr + (u32)btrfs_item_size(path->nodes[0],
						path->slots[0]);
	return 0;
}

void btrfs_backref_init_cache(struct btrfs_fs_info *fs_info,
			      struct btrfs_backref_cache *cache, int is_reloc)
{
	int i;

	cache->rb_root = RB_ROOT;
	for (i = 0; i < BTRFS_MAX_LEVEL; i++)
		INIT_LIST_HEAD(&cache->pending[i]);
	INIT_LIST_HEAD(&cache->changed);
	INIT_LIST_HEAD(&cache->detached);
	INIT_LIST_HEAD(&cache->leaves);
	INIT_LIST_HEAD(&cache->pending_edge);
	INIT_LIST_HEAD(&cache->useless_node);
	cache->fs_info = fs_info;
	cache->is_reloc = is_reloc;
}

struct btrfs_backref_node *btrfs_backref_alloc_node(
		struct btrfs_backref_cache *cache, u64 bytenr, int level)
{
	struct btrfs_backref_node *node;

	ASSERT(level >= 0 && level < BTRFS_MAX_LEVEL);
	node = kzalloc(sizeof(*node), GFP_NOFS);
	if (!node)
		return node;

	INIT_LIST_HEAD(&node->list);
	INIT_LIST_HEAD(&node->upper);
	INIT_LIST_HEAD(&node->lower);
	RB_CLEAR_NODE(&node->rb_node);
	cache->nr_nodes++;
	node->level = level;
	node->bytenr = bytenr;

	return node;
}

struct btrfs_backref_edge *btrfs_backref_alloc_edge(
		struct btrfs_backref_cache *cache)
{
	struct btrfs_backref_edge *edge;

	edge = kzalloc(sizeof(*edge), GFP_NOFS);
	if (edge)
		cache->nr_edges++;
	return edge;
}

 
void btrfs_backref_cleanup_node(struct btrfs_backref_cache *cache,
				struct btrfs_backref_node *node)
{
	struct btrfs_backref_node *upper;
	struct btrfs_backref_edge *edge;

	if (!node)
		return;

	BUG_ON(!node->lowest && !node->detached);
	while (!list_empty(&node->upper)) {
		edge = list_entry(node->upper.next, struct btrfs_backref_edge,
				  list[LOWER]);
		upper = edge->node[UPPER];
		list_del(&edge->list[LOWER]);
		list_del(&edge->list[UPPER]);
		btrfs_backref_free_edge(cache, edge);

		 
		if (list_empty(&upper->lower)) {
			list_add_tail(&upper->lower, &cache->leaves);
			upper->lowest = 1;
		}
	}

	btrfs_backref_drop_node(cache, node);
}

 
void btrfs_backref_release_cache(struct btrfs_backref_cache *cache)
{
	struct btrfs_backref_node *node;
	int i;

	while (!list_empty(&cache->detached)) {
		node = list_entry(cache->detached.next,
				  struct btrfs_backref_node, list);
		btrfs_backref_cleanup_node(cache, node);
	}

	while (!list_empty(&cache->leaves)) {
		node = list_entry(cache->leaves.next,
				  struct btrfs_backref_node, lower);
		btrfs_backref_cleanup_node(cache, node);
	}

	cache->last_trans = 0;

	for (i = 0; i < BTRFS_MAX_LEVEL; i++)
		ASSERT(list_empty(&cache->pending[i]));
	ASSERT(list_empty(&cache->pending_edge));
	ASSERT(list_empty(&cache->useless_node));
	ASSERT(list_empty(&cache->changed));
	ASSERT(list_empty(&cache->detached));
	ASSERT(RB_EMPTY_ROOT(&cache->rb_root));
	ASSERT(!cache->nr_nodes);
	ASSERT(!cache->nr_edges);
}

 
static int handle_direct_tree_backref(struct btrfs_backref_cache *cache,
				      struct btrfs_key *ref_key,
				      struct btrfs_backref_node *cur)
{
	struct btrfs_backref_edge *edge;
	struct btrfs_backref_node *upper;
	struct rb_node *rb_node;

	ASSERT(ref_key->type == BTRFS_SHARED_BLOCK_REF_KEY);

	 
	if (ref_key->objectid == ref_key->offset) {
		struct btrfs_root *root;

		cur->is_reloc_root = 1;
		 
		if (cache->is_reloc) {
			root = find_reloc_root(cache->fs_info, cur->bytenr);
			if (!root)
				return -ENOENT;
			cur->root = root;
		} else {
			 
			list_add(&cur->list, &cache->useless_node);
		}
		return 0;
	}

	edge = btrfs_backref_alloc_edge(cache);
	if (!edge)
		return -ENOMEM;

	rb_node = rb_simple_search(&cache->rb_root, ref_key->offset);
	if (!rb_node) {
		 
		upper = btrfs_backref_alloc_node(cache, ref_key->offset,
					   cur->level + 1);
		if (!upper) {
			btrfs_backref_free_edge(cache, edge);
			return -ENOMEM;
		}

		 
		list_add_tail(&edge->list[UPPER], &cache->pending_edge);
	} else {
		 
		upper = rb_entry(rb_node, struct btrfs_backref_node, rb_node);
		ASSERT(upper->checked);
		INIT_LIST_HEAD(&edge->list[UPPER]);
	}
	btrfs_backref_link_edge(edge, cur, upper, LINK_LOWER);
	return 0;
}

 
static int handle_indirect_tree_backref(struct btrfs_trans_handle *trans,
					struct btrfs_backref_cache *cache,
					struct btrfs_path *path,
					struct btrfs_key *ref_key,
					struct btrfs_key *tree_key,
					struct btrfs_backref_node *cur)
{
	struct btrfs_fs_info *fs_info = cache->fs_info;
	struct btrfs_backref_node *upper;
	struct btrfs_backref_node *lower;
	struct btrfs_backref_edge *edge;
	struct extent_buffer *eb;
	struct btrfs_root *root;
	struct rb_node *rb_node;
	int level;
	bool need_check = true;
	int ret;

	root = btrfs_get_fs_root(fs_info, ref_key->offset, false);
	if (IS_ERR(root))
		return PTR_ERR(root);
	if (!test_bit(BTRFS_ROOT_SHAREABLE, &root->state))
		cur->cowonly = 1;

	if (btrfs_root_level(&root->root_item) == cur->level) {
		 
		ASSERT(btrfs_root_bytenr(&root->root_item) == cur->bytenr);
		 
		if (btrfs_should_ignore_reloc_root(root) && cache->is_reloc) {
			btrfs_put_root(root);
			list_add(&cur->list, &cache->useless_node);
		} else {
			cur->root = root;
		}
		return 0;
	}

	level = cur->level + 1;

	 
	path->search_commit_root = 1;
	path->skip_locking = 1;
	path->lowest_level = level;
	ret = btrfs_search_slot(NULL, root, tree_key, path, 0, 0);
	path->lowest_level = 0;
	if (ret < 0) {
		btrfs_put_root(root);
		return ret;
	}
	if (ret > 0 && path->slots[level] > 0)
		path->slots[level]--;

	eb = path->nodes[level];
	if (btrfs_node_blockptr(eb, path->slots[level]) != cur->bytenr) {
		btrfs_err(fs_info,
"couldn't find block (%llu) (level %d) in tree (%llu) with key (%llu %u %llu)",
			  cur->bytenr, level - 1, root->root_key.objectid,
			  tree_key->objectid, tree_key->type, tree_key->offset);
		btrfs_put_root(root);
		ret = -ENOENT;
		goto out;
	}
	lower = cur;

	 
	for (; level < BTRFS_MAX_LEVEL; level++) {
		if (!path->nodes[level]) {
			ASSERT(btrfs_root_bytenr(&root->root_item) ==
			       lower->bytenr);
			 
			if (btrfs_should_ignore_reloc_root(root) &&
			    cache->is_reloc) {
				btrfs_put_root(root);
				list_add(&lower->list, &cache->useless_node);
			} else {
				lower->root = root;
			}
			break;
		}

		edge = btrfs_backref_alloc_edge(cache);
		if (!edge) {
			btrfs_put_root(root);
			ret = -ENOMEM;
			goto out;
		}

		eb = path->nodes[level];
		rb_node = rb_simple_search(&cache->rb_root, eb->start);
		if (!rb_node) {
			upper = btrfs_backref_alloc_node(cache, eb->start,
							 lower->level + 1);
			if (!upper) {
				btrfs_put_root(root);
				btrfs_backref_free_edge(cache, edge);
				ret = -ENOMEM;
				goto out;
			}
			upper->owner = btrfs_header_owner(eb);
			if (!test_bit(BTRFS_ROOT_SHAREABLE, &root->state))
				upper->cowonly = 1;

			 
			if (btrfs_block_can_be_shared(trans, root, eb))
				upper->checked = 0;
			else
				upper->checked = 1;

			 
			if (!upper->checked && need_check) {
				need_check = false;
				list_add_tail(&edge->list[UPPER],
					      &cache->pending_edge);
			} else {
				if (upper->checked)
					need_check = true;
				INIT_LIST_HEAD(&edge->list[UPPER]);
			}
		} else {
			upper = rb_entry(rb_node, struct btrfs_backref_node,
					 rb_node);
			ASSERT(upper->checked);
			INIT_LIST_HEAD(&edge->list[UPPER]);
			if (!upper->owner)
				upper->owner = btrfs_header_owner(eb);
		}
		btrfs_backref_link_edge(edge, lower, upper, LINK_LOWER);

		if (rb_node) {
			btrfs_put_root(root);
			break;
		}
		lower = upper;
		upper = NULL;
	}
out:
	btrfs_release_path(path);
	return ret;
}

 
int btrfs_backref_add_tree_node(struct btrfs_trans_handle *trans,
				struct btrfs_backref_cache *cache,
				struct btrfs_path *path,
				struct btrfs_backref_iter *iter,
				struct btrfs_key *node_key,
				struct btrfs_backref_node *cur)
{
	struct btrfs_backref_edge *edge;
	struct btrfs_backref_node *exist;
	int ret;

	ret = btrfs_backref_iter_start(iter, cur->bytenr);
	if (ret < 0)
		return ret;
	 
	if (btrfs_backref_has_tree_block_info(iter)) {
		ret = btrfs_backref_iter_next(iter);
		if (ret < 0)
			goto out;
		 
		if (ret > 0) {
			ret = -EUCLEAN;
			goto out;
		}
	}
	WARN_ON(cur->checked);
	if (!list_empty(&cur->upper)) {
		 
		ASSERT(list_is_singular(&cur->upper));
		edge = list_entry(cur->upper.next, struct btrfs_backref_edge,
				  list[LOWER]);
		ASSERT(list_empty(&edge->list[UPPER]));
		exist = edge->node[UPPER];
		 
		if (!exist->checked)
			list_add_tail(&edge->list[UPPER], &cache->pending_edge);
	} else {
		exist = NULL;
	}

	for (; ret == 0; ret = btrfs_backref_iter_next(iter)) {
		struct extent_buffer *eb;
		struct btrfs_key key;
		int type;

		cond_resched();
		eb = btrfs_backref_get_eb(iter);

		key.objectid = iter->bytenr;
		if (btrfs_backref_iter_is_inline_ref(iter)) {
			struct btrfs_extent_inline_ref *iref;

			 
			iref = (struct btrfs_extent_inline_ref *)
				((unsigned long)iter->cur_ptr);
			type = btrfs_get_extent_inline_ref_type(eb, iref,
							BTRFS_REF_TYPE_BLOCK);
			if (type == BTRFS_REF_TYPE_INVALID) {
				ret = -EUCLEAN;
				goto out;
			}
			key.type = type;
			key.offset = btrfs_extent_inline_ref_offset(eb, iref);
		} else {
			key.type = iter->cur_key.type;
			key.offset = iter->cur_key.offset;
		}

		 
		if (exist &&
		    ((key.type == BTRFS_TREE_BLOCK_REF_KEY &&
		      exist->owner == key.offset) ||
		     (key.type == BTRFS_SHARED_BLOCK_REF_KEY &&
		      exist->bytenr == key.offset))) {
			exist = NULL;
			continue;
		}

		 
		if (key.type == BTRFS_SHARED_BLOCK_REF_KEY) {
			ret = handle_direct_tree_backref(cache, &key, cur);
			if (ret < 0)
				goto out;
		} else if (key.type == BTRFS_TREE_BLOCK_REF_KEY) {
			 
			ret = handle_indirect_tree_backref(trans, cache, path,
							   &key, node_key, cur);
			if (ret < 0)
				goto out;
		}
		 
	}
	ret = 0;
	cur->checked = 1;
	WARN_ON(exist);
out:
	btrfs_backref_iter_release(iter);
	return ret;
}

 
int btrfs_backref_finish_upper_links(struct btrfs_backref_cache *cache,
				     struct btrfs_backref_node *start)
{
	struct list_head *useless_node = &cache->useless_node;
	struct btrfs_backref_edge *edge;
	struct rb_node *rb_node;
	LIST_HEAD(pending_edge);

	ASSERT(start->checked);

	 
	if (!start->cowonly) {
		rb_node = rb_simple_insert(&cache->rb_root, start->bytenr,
					   &start->rb_node);
		if (rb_node)
			btrfs_backref_panic(cache->fs_info, start->bytenr,
					    -EEXIST);
		list_add_tail(&start->lower, &cache->leaves);
	}

	 
	list_for_each_entry(edge, &start->upper, list[LOWER])
		list_add_tail(&edge->list[UPPER], &pending_edge);

	while (!list_empty(&pending_edge)) {
		struct btrfs_backref_node *upper;
		struct btrfs_backref_node *lower;

		edge = list_first_entry(&pending_edge,
				struct btrfs_backref_edge, list[UPPER]);
		list_del_init(&edge->list[UPPER]);
		upper = edge->node[UPPER];
		lower = edge->node[LOWER];

		 
		if (upper->detached) {
			list_del(&edge->list[LOWER]);
			btrfs_backref_free_edge(cache, edge);

			 
			if (list_empty(&lower->upper))
				list_add(&lower->list, useless_node);
			continue;
		}

		 
		if (!RB_EMPTY_NODE(&upper->rb_node)) {
			if (upper->lowest) {
				list_del_init(&upper->lower);
				upper->lowest = 0;
			}

			list_add_tail(&edge->list[UPPER], &upper->lower);
			continue;
		}

		 
		if (!upper->checked) {
			ASSERT(0);
			return -EUCLEAN;
		}

		 
		if (start->cowonly != upper->cowonly) {
			ASSERT(0);
			return -EUCLEAN;
		}

		 
		if (!upper->cowonly) {
			rb_node = rb_simple_insert(&cache->rb_root, upper->bytenr,
						   &upper->rb_node);
			if (rb_node) {
				btrfs_backref_panic(cache->fs_info,
						upper->bytenr, -EEXIST);
				return -EUCLEAN;
			}
		}

		list_add_tail(&edge->list[UPPER], &upper->lower);

		 
		list_for_each_entry(edge, &upper->upper, list[LOWER])
			list_add_tail(&edge->list[UPPER], &pending_edge);
	}
	return 0;
}

void btrfs_backref_error_cleanup(struct btrfs_backref_cache *cache,
				 struct btrfs_backref_node *node)
{
	struct btrfs_backref_node *lower;
	struct btrfs_backref_node *upper;
	struct btrfs_backref_edge *edge;

	while (!list_empty(&cache->useless_node)) {
		lower = list_first_entry(&cache->useless_node,
				   struct btrfs_backref_node, list);
		list_del_init(&lower->list);
	}
	while (!list_empty(&cache->pending_edge)) {
		edge = list_first_entry(&cache->pending_edge,
				struct btrfs_backref_edge, list[UPPER]);
		list_del(&edge->list[UPPER]);
		list_del(&edge->list[LOWER]);
		lower = edge->node[LOWER];
		upper = edge->node[UPPER];
		btrfs_backref_free_edge(cache, edge);

		 
		if (list_empty(&lower->upper) &&
		    RB_EMPTY_NODE(&lower->rb_node))
			list_add(&lower->list, &cache->useless_node);

		if (!RB_EMPTY_NODE(&upper->rb_node))
			continue;

		 
		list_for_each_entry(edge, &upper->upper, list[LOWER])
			list_add_tail(&edge->list[UPPER],
				      &cache->pending_edge);
		if (list_empty(&upper->upper))
			list_add(&upper->list, &cache->useless_node);
	}

	while (!list_empty(&cache->useless_node)) {
		lower = list_first_entry(&cache->useless_node,
				   struct btrfs_backref_node, list);
		list_del_init(&lower->list);
		if (lower == node)
			node = NULL;
		btrfs_backref_drop_node(cache, lower);
	}

	btrfs_backref_cleanup_node(cache, node);
	ASSERT(list_empty(&cache->useless_node) &&
	       list_empty(&cache->pending_edge));
}

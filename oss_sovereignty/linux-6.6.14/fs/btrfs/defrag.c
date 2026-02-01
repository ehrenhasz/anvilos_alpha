
 

#include <linux/sched.h>
#include "ctree.h"
#include "disk-io.h"
#include "print-tree.h"
#include "transaction.h"
#include "locking.h"
#include "accessors.h"
#include "messages.h"
#include "delalloc-space.h"
#include "subpage.h"
#include "defrag.h"
#include "file-item.h"
#include "super.h"

static struct kmem_cache *btrfs_inode_defrag_cachep;

 
struct inode_defrag {
	struct rb_node rb_node;
	 
	u64 ino;
	 
	u64 transid;

	 
	u64 root;

	 
	u32 extent_thresh;
};

static int __compare_inode_defrag(struct inode_defrag *defrag1,
				  struct inode_defrag *defrag2)
{
	if (defrag1->root > defrag2->root)
		return 1;
	else if (defrag1->root < defrag2->root)
		return -1;
	else if (defrag1->ino > defrag2->ino)
		return 1;
	else if (defrag1->ino < defrag2->ino)
		return -1;
	else
		return 0;
}

 
static int __btrfs_add_inode_defrag(struct btrfs_inode *inode,
				    struct inode_defrag *defrag)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct inode_defrag *entry;
	struct rb_node **p;
	struct rb_node *parent = NULL;
	int ret;

	p = &fs_info->defrag_inodes.rb_node;
	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct inode_defrag, rb_node);

		ret = __compare_inode_defrag(defrag, entry);
		if (ret < 0)
			p = &parent->rb_left;
		else if (ret > 0)
			p = &parent->rb_right;
		else {
			 
			if (defrag->transid < entry->transid)
				entry->transid = defrag->transid;
			entry->extent_thresh = min(defrag->extent_thresh,
						   entry->extent_thresh);
			return -EEXIST;
		}
	}
	set_bit(BTRFS_INODE_IN_DEFRAG, &inode->runtime_flags);
	rb_link_node(&defrag->rb_node, parent, p);
	rb_insert_color(&defrag->rb_node, &fs_info->defrag_inodes);
	return 0;
}

static inline int __need_auto_defrag(struct btrfs_fs_info *fs_info)
{
	if (!btrfs_test_opt(fs_info, AUTO_DEFRAG))
		return 0;

	if (btrfs_fs_closing(fs_info))
		return 0;

	return 1;
}

 
int btrfs_add_inode_defrag(struct btrfs_trans_handle *trans,
			   struct btrfs_inode *inode, u32 extent_thresh)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct inode_defrag *defrag;
	u64 transid;
	int ret;

	if (!__need_auto_defrag(fs_info))
		return 0;

	if (test_bit(BTRFS_INODE_IN_DEFRAG, &inode->runtime_flags))
		return 0;

	if (trans)
		transid = trans->transid;
	else
		transid = inode->root->last_trans;

	defrag = kmem_cache_zalloc(btrfs_inode_defrag_cachep, GFP_NOFS);
	if (!defrag)
		return -ENOMEM;

	defrag->ino = btrfs_ino(inode);
	defrag->transid = transid;
	defrag->root = root->root_key.objectid;
	defrag->extent_thresh = extent_thresh;

	spin_lock(&fs_info->defrag_inodes_lock);
	if (!test_bit(BTRFS_INODE_IN_DEFRAG, &inode->runtime_flags)) {
		 
		ret = __btrfs_add_inode_defrag(inode, defrag);
		if (ret)
			kmem_cache_free(btrfs_inode_defrag_cachep, defrag);
	} else {
		kmem_cache_free(btrfs_inode_defrag_cachep, defrag);
	}
	spin_unlock(&fs_info->defrag_inodes_lock);
	return 0;
}

 
static struct inode_defrag *btrfs_pick_defrag_inode(
			struct btrfs_fs_info *fs_info, u64 root, u64 ino)
{
	struct inode_defrag *entry = NULL;
	struct inode_defrag tmp;
	struct rb_node *p;
	struct rb_node *parent = NULL;
	int ret;

	tmp.ino = ino;
	tmp.root = root;

	spin_lock(&fs_info->defrag_inodes_lock);
	p = fs_info->defrag_inodes.rb_node;
	while (p) {
		parent = p;
		entry = rb_entry(parent, struct inode_defrag, rb_node);

		ret = __compare_inode_defrag(&tmp, entry);
		if (ret < 0)
			p = parent->rb_left;
		else if (ret > 0)
			p = parent->rb_right;
		else
			goto out;
	}

	if (parent && __compare_inode_defrag(&tmp, entry) > 0) {
		parent = rb_next(parent);
		if (parent)
			entry = rb_entry(parent, struct inode_defrag, rb_node);
		else
			entry = NULL;
	}
out:
	if (entry)
		rb_erase(parent, &fs_info->defrag_inodes);
	spin_unlock(&fs_info->defrag_inodes_lock);
	return entry;
}

void btrfs_cleanup_defrag_inodes(struct btrfs_fs_info *fs_info)
{
	struct inode_defrag *defrag;
	struct rb_node *node;

	spin_lock(&fs_info->defrag_inodes_lock);
	node = rb_first(&fs_info->defrag_inodes);
	while (node) {
		rb_erase(node, &fs_info->defrag_inodes);
		defrag = rb_entry(node, struct inode_defrag, rb_node);
		kmem_cache_free(btrfs_inode_defrag_cachep, defrag);

		cond_resched_lock(&fs_info->defrag_inodes_lock);

		node = rb_first(&fs_info->defrag_inodes);
	}
	spin_unlock(&fs_info->defrag_inodes_lock);
}

#define BTRFS_DEFRAG_BATCH	1024

static int __btrfs_run_defrag_inode(struct btrfs_fs_info *fs_info,
				    struct inode_defrag *defrag)
{
	struct btrfs_root *inode_root;
	struct inode *inode;
	struct btrfs_ioctl_defrag_range_args range;
	int ret = 0;
	u64 cur = 0;

again:
	if (test_bit(BTRFS_FS_STATE_REMOUNTING, &fs_info->fs_state))
		goto cleanup;
	if (!__need_auto_defrag(fs_info))
		goto cleanup;

	 
	inode_root = btrfs_get_fs_root(fs_info, defrag->root, true);
	if (IS_ERR(inode_root)) {
		ret = PTR_ERR(inode_root);
		goto cleanup;
	}

	inode = btrfs_iget(fs_info->sb, defrag->ino, inode_root);
	btrfs_put_root(inode_root);
	if (IS_ERR(inode)) {
		ret = PTR_ERR(inode);
		goto cleanup;
	}

	if (cur >= i_size_read(inode)) {
		iput(inode);
		goto cleanup;
	}

	 
	clear_bit(BTRFS_INODE_IN_DEFRAG, &BTRFS_I(inode)->runtime_flags);
	memset(&range, 0, sizeof(range));
	range.len = (u64)-1;
	range.start = cur;
	range.extent_thresh = defrag->extent_thresh;

	sb_start_write(fs_info->sb);
	ret = btrfs_defrag_file(inode, NULL, &range, defrag->transid,
				       BTRFS_DEFRAG_BATCH);
	sb_end_write(fs_info->sb);
	iput(inode);

	if (ret < 0)
		goto cleanup;

	cur = max(cur + fs_info->sectorsize, range.start);
	goto again;

cleanup:
	kmem_cache_free(btrfs_inode_defrag_cachep, defrag);
	return ret;
}

 
int btrfs_run_defrag_inodes(struct btrfs_fs_info *fs_info)
{
	struct inode_defrag *defrag;
	u64 first_ino = 0;
	u64 root_objectid = 0;

	atomic_inc(&fs_info->defrag_running);
	while (1) {
		 
		if (test_bit(BTRFS_FS_STATE_REMOUNTING, &fs_info->fs_state))
			break;

		if (!__need_auto_defrag(fs_info))
			break;

		 
		defrag = btrfs_pick_defrag_inode(fs_info, root_objectid, first_ino);
		if (!defrag) {
			if (root_objectid || first_ino) {
				root_objectid = 0;
				first_ino = 0;
				continue;
			} else {
				break;
			}
		}

		first_ino = defrag->ino + 1;
		root_objectid = defrag->root;

		__btrfs_run_defrag_inode(fs_info, defrag);
	}
	atomic_dec(&fs_info->defrag_running);

	 
	wake_up(&fs_info->transaction_wait);
	return 0;
}

 

int btrfs_defrag_leaves(struct btrfs_trans_handle *trans,
			struct btrfs_root *root)
{
	struct btrfs_path *path = NULL;
	struct btrfs_key key;
	int ret = 0;
	int wret;
	int level;
	int next_key_ret = 0;
	u64 last_ret = 0;

	if (!test_bit(BTRFS_ROOT_SHAREABLE, &root->state))
		goto out;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out;
	}

	level = btrfs_header_level(root->node);

	if (level == 0)
		goto out;

	if (root->defrag_progress.objectid == 0) {
		struct extent_buffer *root_node;
		u32 nritems;

		root_node = btrfs_lock_root_node(root);
		nritems = btrfs_header_nritems(root_node);
		root->defrag_max.objectid = 0;
		 
		btrfs_node_key_to_cpu(root_node, &root->defrag_max,
				      nritems - 1);
		btrfs_tree_unlock(root_node);
		free_extent_buffer(root_node);
		memset(&key, 0, sizeof(key));
	} else {
		memcpy(&key, &root->defrag_progress, sizeof(key));
	}

	path->keep_locks = 1;

	ret = btrfs_search_forward(root, &key, path, BTRFS_OLDEST_GENERATION);
	if (ret < 0)
		goto out;
	if (ret > 0) {
		ret = 0;
		goto out;
	}
	btrfs_release_path(path);
	 
	path->lowest_level = 1;
	wret = btrfs_search_slot(trans, root, &key, path, 0, 1);

	if (wret < 0) {
		ret = wret;
		goto out;
	}
	if (!path->nodes[1]) {
		ret = 0;
		goto out;
	}
	 
	BUG_ON(path->locks[1] == 0);
	ret = btrfs_realloc_node(trans, root,
				 path->nodes[1], 0,
				 &last_ret,
				 &root->defrag_progress);
	if (ret) {
		WARN_ON(ret == -EAGAIN);
		goto out;
	}
	 
	path->slots[1] = btrfs_header_nritems(path->nodes[1]);
	next_key_ret = btrfs_find_next_key(root, path, &key, 1,
					   BTRFS_OLDEST_GENERATION);
	if (next_key_ret == 0) {
		memcpy(&root->defrag_progress, &key, sizeof(key));
		ret = -EAGAIN;
	}
out:
	btrfs_free_path(path);
	if (ret == -EAGAIN) {
		if (root->defrag_max.objectid > root->defrag_progress.objectid)
			goto done;
		if (root->defrag_max.type > root->defrag_progress.type)
			goto done;
		if (root->defrag_max.offset > root->defrag_progress.offset)
			goto done;
		ret = 0;
	}
done:
	if (ret != -EAGAIN)
		memset(&root->defrag_progress, 0,
		       sizeof(root->defrag_progress));

	return ret;
}

 
static struct extent_map *defrag_get_extent(struct btrfs_inode *inode,
					    u64 start, u64 newer_than)
{
	struct btrfs_root *root = inode->root;
	struct btrfs_file_extent_item *fi;
	struct btrfs_path path = { 0 };
	struct extent_map *em;
	struct btrfs_key key;
	u64 ino = btrfs_ino(inode);
	int ret;

	em = alloc_extent_map();
	if (!em) {
		ret = -ENOMEM;
		goto err;
	}

	key.objectid = ino;
	key.type = BTRFS_EXTENT_DATA_KEY;
	key.offset = start;

	if (newer_than) {
		ret = btrfs_search_forward(root, &key, &path, newer_than);
		if (ret < 0)
			goto err;
		 
		if (ret > 0)
			goto not_found;
	} else {
		ret = btrfs_search_slot(NULL, root, &key, &path, 0, 0);
		if (ret < 0)
			goto err;
	}
	if (path.slots[0] >= btrfs_header_nritems(path.nodes[0])) {
		 
		ASSERT(btrfs_header_nritems(path.nodes[0]));
		path.slots[0] = btrfs_header_nritems(path.nodes[0]) - 1;
	}
	btrfs_item_key_to_cpu(path.nodes[0], &key, path.slots[0]);
	 
	if (key.objectid == ino && key.type == BTRFS_EXTENT_DATA_KEY &&
	    key.offset == start)
		goto iterate;

	 
	if (path.slots[0] > 0) {
		btrfs_item_key_to_cpu(path.nodes[0], &key, path.slots[0]);
		if (key.objectid == ino && key.type == BTRFS_EXTENT_DATA_KEY)
			path.slots[0]--;
	}

iterate:
	 
	while (true) {
		u64 extent_end;

		if (path.slots[0] >= btrfs_header_nritems(path.nodes[0]))
			goto next;

		btrfs_item_key_to_cpu(path.nodes[0], &key, path.slots[0]);

		 
		if (WARN_ON(key.objectid < ino) || key.type < BTRFS_EXTENT_DATA_KEY)
			goto next;

		 
		if (key.objectid > ino || key.type > BTRFS_EXTENT_DATA_KEY)
			goto not_found;

		 
		if (key.offset > start) {
			em->start = start;
			em->orig_start = start;
			em->block_start = EXTENT_MAP_HOLE;
			em->len = key.offset - start;
			break;
		}

		fi = btrfs_item_ptr(path.nodes[0], path.slots[0],
				    struct btrfs_file_extent_item);
		extent_end = btrfs_file_extent_end(&path);

		 
		if (extent_end <= start)
			goto next;

		 
		btrfs_extent_item_to_extent_map(inode, &path, fi, em);
		break;
next:
		ret = btrfs_next_item(root, &path);
		if (ret < 0)
			goto err;
		if (ret > 0)
			goto not_found;
	}
	btrfs_release_path(&path);
	return em;

not_found:
	btrfs_release_path(&path);
	free_extent_map(em);
	return NULL;

err:
	btrfs_release_path(&path);
	free_extent_map(em);
	return ERR_PTR(ret);
}

static struct extent_map *defrag_lookup_extent(struct inode *inode, u64 start,
					       u64 newer_than, bool locked)
{
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct extent_io_tree *io_tree = &BTRFS_I(inode)->io_tree;
	struct extent_map *em;
	const u32 sectorsize = BTRFS_I(inode)->root->fs_info->sectorsize;

	 
	read_lock(&em_tree->lock);
	em = lookup_extent_mapping(em_tree, start, sectorsize);
	read_unlock(&em_tree->lock);

	 
	if (em && test_bit(EXTENT_FLAG_MERGED, &em->flags) &&
	    newer_than && em->generation >= newer_than) {
		free_extent_map(em);
		em = NULL;
	}

	if (!em) {
		struct extent_state *cached = NULL;
		u64 end = start + sectorsize - 1;

		 
		if (!locked)
			lock_extent(io_tree, start, end, &cached);
		em = defrag_get_extent(BTRFS_I(inode), start, newer_than);
		if (!locked)
			unlock_extent(io_tree, start, end, &cached);

		if (IS_ERR(em))
			return NULL;
	}

	return em;
}

static u32 get_extent_max_capacity(const struct btrfs_fs_info *fs_info,
				   const struct extent_map *em)
{
	if (test_bit(EXTENT_FLAG_COMPRESSED, &em->flags))
		return BTRFS_MAX_COMPRESSED;
	return fs_info->max_extent_size;
}

static bool defrag_check_next_extent(struct inode *inode, struct extent_map *em,
				     u32 extent_thresh, u64 newer_than, bool locked)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	struct extent_map *next;
	bool ret = false;

	 
	if (em->start + em->len >= i_size_read(inode))
		return false;

	 
	next = defrag_lookup_extent(inode, em->start + em->len, newer_than, locked);
	 
	if (!next || next->block_start >= EXTENT_MAP_LAST_BYTE)
		goto out;
	if (test_bit(EXTENT_FLAG_PREALLOC, &next->flags))
		goto out;
	 
	if (next->len >= get_extent_max_capacity(fs_info, em))
		goto out;
	 
	if (next->generation < newer_than)
		goto out;
	 
	if (next->len >= extent_thresh)
		goto out;

	ret = true;
out:
	free_extent_map(next);
	return ret;
}

 
static struct page *defrag_prepare_one_page(struct btrfs_inode *inode, pgoff_t index)
{
	struct address_space *mapping = inode->vfs_inode.i_mapping;
	gfp_t mask = btrfs_alloc_write_mask(mapping);
	u64 page_start = (u64)index << PAGE_SHIFT;
	u64 page_end = page_start + PAGE_SIZE - 1;
	struct extent_state *cached_state = NULL;
	struct page *page;
	int ret;

again:
	page = find_or_create_page(mapping, index, mask);
	if (!page)
		return ERR_PTR(-ENOMEM);

	 
	if (PageCompound(page)) {
		unlock_page(page);
		put_page(page);
		return ERR_PTR(-ETXTBSY);
	}

	ret = set_page_extent_mapped(page);
	if (ret < 0) {
		unlock_page(page);
		put_page(page);
		return ERR_PTR(ret);
	}

	 
	while (1) {
		struct btrfs_ordered_extent *ordered;

		lock_extent(&inode->io_tree, page_start, page_end, &cached_state);
		ordered = btrfs_lookup_ordered_range(inode, page_start, PAGE_SIZE);
		unlock_extent(&inode->io_tree, page_start, page_end,
			      &cached_state);
		if (!ordered)
			break;

		unlock_page(page);
		btrfs_start_ordered_extent(ordered);
		btrfs_put_ordered_extent(ordered);
		lock_page(page);
		 
		if (page->mapping != mapping || !PagePrivate(page)) {
			unlock_page(page);
			put_page(page);
			goto again;
		}
	}

	 
	if (!PageUptodate(page)) {
		btrfs_read_folio(NULL, page_folio(page));
		lock_page(page);
		if (page->mapping != mapping || !PagePrivate(page)) {
			unlock_page(page);
			put_page(page);
			goto again;
		}
		if (!PageUptodate(page)) {
			unlock_page(page);
			put_page(page);
			return ERR_PTR(-EIO);
		}
	}
	return page;
}

struct defrag_target_range {
	struct list_head list;
	u64 start;
	u64 len;
};

 
static int defrag_collect_targets(struct btrfs_inode *inode,
				  u64 start, u64 len, u32 extent_thresh,
				  u64 newer_than, bool do_compress,
				  bool locked, struct list_head *target_list,
				  u64 *last_scanned_ret)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	bool last_is_target = false;
	u64 cur = start;
	int ret = 0;

	while (cur < start + len) {
		struct extent_map *em;
		struct defrag_target_range *new;
		bool next_mergeable = true;
		u64 range_len;

		last_is_target = false;
		em = defrag_lookup_extent(&inode->vfs_inode, cur, newer_than, locked);
		if (!em)
			break;

		 
		if (em->block_start == EXTENT_MAP_INLINE &&
		    em->len <= inode->root->fs_info->max_inline)
			goto next;

		 
		if (em->block_start == EXTENT_MAP_HOLE ||
		    em->block_start == EXTENT_MAP_DELALLOC ||
		    test_bit(EXTENT_FLAG_PREALLOC, &em->flags))
			goto next;

		 
		if (em->generation < newer_than)
			goto next;

		 
		if (em->generation == (u64)-1)
			goto next;

		 
		range_len = em->len - (cur - em->start);
		 
		if (test_range_bit(&inode->io_tree, cur, cur + range_len - 1,
				   EXTENT_DELALLOC, 0, NULL))
			goto next;

		 
		if (do_compress)
			goto add;

		 
		if (range_len >= extent_thresh)
			goto next;

		 
		if (em->len >= get_extent_max_capacity(fs_info, em))
			goto next;

		 
		if (em->block_start == EXTENT_MAP_INLINE)
			goto add;

		next_mergeable = defrag_check_next_extent(&inode->vfs_inode, em,
						extent_thresh, newer_than, locked);
		if (!next_mergeable) {
			struct defrag_target_range *last;

			 
			if (list_empty(target_list))
				goto next;
			last = list_entry(target_list->prev,
					  struct defrag_target_range, list);
			 
			if (last->start + last->len != cur)
				goto next;

			 
		}

add:
		last_is_target = true;
		range_len = min(extent_map_end(em), start + len) - cur;
		 
		if (!list_empty(target_list)) {
			struct defrag_target_range *last;

			last = list_entry(target_list->prev,
					  struct defrag_target_range, list);
			ASSERT(last->start + last->len <= cur);
			if (last->start + last->len == cur) {
				 
				last->len += range_len;
				goto next;
			}
			 
		}

		 
		new = kmalloc(sizeof(*new), GFP_NOFS);
		if (!new) {
			free_extent_map(em);
			ret = -ENOMEM;
			break;
		}
		new->start = cur;
		new->len = range_len;
		list_add_tail(&new->list, target_list);

next:
		cur = extent_map_end(em);
		free_extent_map(em);
	}
	if (ret < 0) {
		struct defrag_target_range *entry;
		struct defrag_target_range *tmp;

		list_for_each_entry_safe(entry, tmp, target_list, list) {
			list_del_init(&entry->list);
			kfree(entry);
		}
	}
	if (!ret && last_scanned_ret) {
		 
		if (!last_is_target)
			*last_scanned_ret = max(cur, *last_scanned_ret);
		else
			*last_scanned_ret = max(start + len, *last_scanned_ret);
	}
	return ret;
}

#define CLUSTER_SIZE	(SZ_256K)
static_assert(PAGE_ALIGNED(CLUSTER_SIZE));

 
static int defrag_one_locked_target(struct btrfs_inode *inode,
				    struct defrag_target_range *target,
				    struct page **pages, int nr_pages,
				    struct extent_state **cached_state)
{
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct extent_changeset *data_reserved = NULL;
	const u64 start = target->start;
	const u64 len = target->len;
	unsigned long last_index = (start + len - 1) >> PAGE_SHIFT;
	unsigned long start_index = start >> PAGE_SHIFT;
	unsigned long first_index = page_index(pages[0]);
	int ret = 0;
	int i;

	ASSERT(last_index - first_index + 1 <= nr_pages);

	ret = btrfs_delalloc_reserve_space(inode, &data_reserved, start, len);
	if (ret < 0)
		return ret;
	clear_extent_bit(&inode->io_tree, start, start + len - 1,
			 EXTENT_DELALLOC | EXTENT_DO_ACCOUNTING |
			 EXTENT_DEFRAG, cached_state);
	set_extent_bit(&inode->io_tree, start, start + len - 1,
		       EXTENT_DELALLOC | EXTENT_DEFRAG, cached_state);

	 
	for (i = start_index - first_index; i <= last_index - first_index; i++) {
		ClearPageChecked(pages[i]);
		btrfs_page_clamp_set_dirty(fs_info, pages[i], start, len);
	}
	btrfs_delalloc_release_extents(inode, len);
	extent_changeset_free(data_reserved);

	return ret;
}

static int defrag_one_range(struct btrfs_inode *inode, u64 start, u32 len,
			    u32 extent_thresh, u64 newer_than, bool do_compress,
			    u64 *last_scanned_ret)
{
	struct extent_state *cached_state = NULL;
	struct defrag_target_range *entry;
	struct defrag_target_range *tmp;
	LIST_HEAD(target_list);
	struct page **pages;
	const u32 sectorsize = inode->root->fs_info->sectorsize;
	u64 last_index = (start + len - 1) >> PAGE_SHIFT;
	u64 start_index = start >> PAGE_SHIFT;
	unsigned int nr_pages = last_index - start_index + 1;
	int ret = 0;
	int i;

	ASSERT(nr_pages <= CLUSTER_SIZE / PAGE_SIZE);
	ASSERT(IS_ALIGNED(start, sectorsize) && IS_ALIGNED(len, sectorsize));

	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_NOFS);
	if (!pages)
		return -ENOMEM;

	 
	for (i = 0; i < nr_pages; i++) {
		pages[i] = defrag_prepare_one_page(inode, start_index + i);
		if (IS_ERR(pages[i])) {
			ret = PTR_ERR(pages[i]);
			pages[i] = NULL;
			goto free_pages;
		}
	}
	for (i = 0; i < nr_pages; i++)
		wait_on_page_writeback(pages[i]);

	 
	lock_extent(&inode->io_tree, start_index << PAGE_SHIFT,
		    (last_index << PAGE_SHIFT) + PAGE_SIZE - 1,
		    &cached_state);
	 
	ret = defrag_collect_targets(inode, start, len, extent_thresh,
				     newer_than, do_compress, true,
				     &target_list, last_scanned_ret);
	if (ret < 0)
		goto unlock_extent;

	list_for_each_entry(entry, &target_list, list) {
		ret = defrag_one_locked_target(inode, entry, pages, nr_pages,
					       &cached_state);
		if (ret < 0)
			break;
	}

	list_for_each_entry_safe(entry, tmp, &target_list, list) {
		list_del_init(&entry->list);
		kfree(entry);
	}
unlock_extent:
	unlock_extent(&inode->io_tree, start_index << PAGE_SHIFT,
		      (last_index << PAGE_SHIFT) + PAGE_SIZE - 1,
		      &cached_state);
free_pages:
	for (i = 0; i < nr_pages; i++) {
		if (pages[i]) {
			unlock_page(pages[i]);
			put_page(pages[i]);
		}
	}
	kfree(pages);
	return ret;
}

static int defrag_one_cluster(struct btrfs_inode *inode,
			      struct file_ra_state *ra,
			      u64 start, u32 len, u32 extent_thresh,
			      u64 newer_than, bool do_compress,
			      unsigned long *sectors_defragged,
			      unsigned long max_sectors,
			      u64 *last_scanned_ret)
{
	const u32 sectorsize = inode->root->fs_info->sectorsize;
	struct defrag_target_range *entry;
	struct defrag_target_range *tmp;
	LIST_HEAD(target_list);
	int ret;

	ret = defrag_collect_targets(inode, start, len, extent_thresh,
				     newer_than, do_compress, false,
				     &target_list, NULL);
	if (ret < 0)
		goto out;

	list_for_each_entry(entry, &target_list, list) {
		u32 range_len = entry->len;

		 
		if (max_sectors && *sectors_defragged >= max_sectors) {
			ret = 1;
			break;
		}

		if (max_sectors)
			range_len = min_t(u32, range_len,
				(max_sectors - *sectors_defragged) * sectorsize);

		 
		if (entry->start + range_len <= *last_scanned_ret)
			continue;

		if (ra)
			page_cache_sync_readahead(inode->vfs_inode.i_mapping,
				ra, NULL, entry->start >> PAGE_SHIFT,
				((entry->start + range_len - 1) >> PAGE_SHIFT) -
				(entry->start >> PAGE_SHIFT) + 1);
		 
		ret = defrag_one_range(inode, entry->start, range_len,
				       extent_thresh, newer_than, do_compress,
				       last_scanned_ret);
		if (ret < 0)
			break;
		*sectors_defragged += range_len >>
				      inode->root->fs_info->sectorsize_bits;
	}
out:
	list_for_each_entry_safe(entry, tmp, &target_list, list) {
		list_del_init(&entry->list);
		kfree(entry);
	}
	if (ret >= 0)
		*last_scanned_ret = max(*last_scanned_ret, start + len);
	return ret;
}

 
int btrfs_defrag_file(struct inode *inode, struct file_ra_state *ra,
		      struct btrfs_ioctl_defrag_range_args *range,
		      u64 newer_than, unsigned long max_to_defrag)
{
	struct btrfs_fs_info *fs_info = btrfs_sb(inode->i_sb);
	unsigned long sectors_defragged = 0;
	u64 isize = i_size_read(inode);
	u64 cur;
	u64 last_byte;
	bool do_compress = (range->flags & BTRFS_DEFRAG_RANGE_COMPRESS);
	bool ra_allocated = false;
	int compress_type = BTRFS_COMPRESS_ZLIB;
	int ret = 0;
	u32 extent_thresh = range->extent_thresh;
	pgoff_t start_index;

	if (isize == 0)
		return 0;

	if (range->start >= isize)
		return -EINVAL;

	if (do_compress) {
		if (range->compress_type >= BTRFS_NR_COMPRESS_TYPES)
			return -EINVAL;
		if (range->compress_type)
			compress_type = range->compress_type;
	}

	if (extent_thresh == 0)
		extent_thresh = SZ_256K;

	if (range->start + range->len > range->start) {
		 
		last_byte = min(isize, range->start + range->len);
	} else {
		 
		last_byte = isize;
	}

	 
	cur = round_down(range->start, fs_info->sectorsize);
	last_byte = round_up(last_byte, fs_info->sectorsize) - 1;

	 
	if (!ra) {
		ra_allocated = true;
		ra = kzalloc(sizeof(*ra), GFP_KERNEL);
		if (ra)
			file_ra_state_init(ra, inode->i_mapping);
	}

	 
	start_index = cur >> PAGE_SHIFT;
	if (start_index < inode->i_mapping->writeback_index)
		inode->i_mapping->writeback_index = start_index;

	while (cur < last_byte) {
		const unsigned long prev_sectors_defragged = sectors_defragged;
		u64 last_scanned = cur;
		u64 cluster_end;

		if (btrfs_defrag_cancelled(fs_info)) {
			ret = -EAGAIN;
			break;
		}

		 
		cluster_end = (((cur >> PAGE_SHIFT) +
			       (SZ_256K >> PAGE_SHIFT)) << PAGE_SHIFT) - 1;
		cluster_end = min(cluster_end, last_byte);

		btrfs_inode_lock(BTRFS_I(inode), 0);
		if (IS_SWAPFILE(inode)) {
			ret = -ETXTBSY;
			btrfs_inode_unlock(BTRFS_I(inode), 0);
			break;
		}
		if (!(inode->i_sb->s_flags & SB_ACTIVE)) {
			btrfs_inode_unlock(BTRFS_I(inode), 0);
			break;
		}
		if (do_compress)
			BTRFS_I(inode)->defrag_compress = compress_type;
		ret = defrag_one_cluster(BTRFS_I(inode), ra, cur,
				cluster_end + 1 - cur, extent_thresh,
				newer_than, do_compress, &sectors_defragged,
				max_to_defrag, &last_scanned);

		if (sectors_defragged > prev_sectors_defragged)
			balance_dirty_pages_ratelimited(inode->i_mapping);

		btrfs_inode_unlock(BTRFS_I(inode), 0);
		if (ret < 0)
			break;
		cur = max(cluster_end + 1, last_scanned);
		if (ret > 0) {
			ret = 0;
			break;
		}
		cond_resched();
	}

	if (ra_allocated)
		kfree(ra);
	 
	range->start = cur;
	if (sectors_defragged) {
		 
		if (range->flags & BTRFS_DEFRAG_RANGE_START_IO) {
			filemap_flush(inode->i_mapping);
			if (test_bit(BTRFS_INODE_HAS_ASYNC_EXTENT,
				     &BTRFS_I(inode)->runtime_flags))
				filemap_flush(inode->i_mapping);
		}
		if (range->compress_type == BTRFS_COMPRESS_LZO)
			btrfs_set_fs_incompat(fs_info, COMPRESS_LZO);
		else if (range->compress_type == BTRFS_COMPRESS_ZSTD)
			btrfs_set_fs_incompat(fs_info, COMPRESS_ZSTD);
		ret = sectors_defragged;
	}
	if (do_compress) {
		btrfs_inode_lock(BTRFS_I(inode), 0);
		BTRFS_I(inode)->defrag_compress = BTRFS_COMPRESS_NONE;
		btrfs_inode_unlock(BTRFS_I(inode), 0);
	}
	return ret;
}

void __cold btrfs_auto_defrag_exit(void)
{
	kmem_cache_destroy(btrfs_inode_defrag_cachep);
}

int __init btrfs_auto_defrag_init(void)
{
	btrfs_inode_defrag_cachep = kmem_cache_create("btrfs_inode_defrag",
					sizeof(struct inode_defrag), 0,
					SLAB_MEM_SPREAD,
					NULL);
	if (!btrfs_inode_defrag_cachep)
		return -ENOMEM;

	return 0;
}

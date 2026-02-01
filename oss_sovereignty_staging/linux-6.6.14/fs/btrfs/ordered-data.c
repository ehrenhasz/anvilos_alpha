
 

#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/writeback.h>
#include <linux/sched/mm.h>
#include "messages.h"
#include "misc.h"
#include "ctree.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "extent_io.h"
#include "disk-io.h"
#include "compression.h"
#include "delalloc-space.h"
#include "qgroup.h"
#include "subpage.h"
#include "file.h"
#include "super.h"

static struct kmem_cache *btrfs_ordered_extent_cache;

static u64 entry_end(struct btrfs_ordered_extent *entry)
{
	if (entry->file_offset + entry->num_bytes < entry->file_offset)
		return (u64)-1;
	return entry->file_offset + entry->num_bytes;
}

 
static struct rb_node *tree_insert(struct rb_root *root, u64 file_offset,
				   struct rb_node *node)
{
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct btrfs_ordered_extent *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct btrfs_ordered_extent, rb_node);

		if (file_offset < entry->file_offset)
			p = &(*p)->rb_left;
		else if (file_offset >= entry_end(entry))
			p = &(*p)->rb_right;
		else
			return parent;
	}

	rb_link_node(node, parent, p);
	rb_insert_color(node, root);
	return NULL;
}

 
static struct rb_node *__tree_search(struct rb_root *root, u64 file_offset,
				     struct rb_node **prev_ret)
{
	struct rb_node *n = root->rb_node;
	struct rb_node *prev = NULL;
	struct rb_node *test;
	struct btrfs_ordered_extent *entry;
	struct btrfs_ordered_extent *prev_entry = NULL;

	while (n) {
		entry = rb_entry(n, struct btrfs_ordered_extent, rb_node);
		prev = n;
		prev_entry = entry;

		if (file_offset < entry->file_offset)
			n = n->rb_left;
		else if (file_offset >= entry_end(entry))
			n = n->rb_right;
		else
			return n;
	}
	if (!prev_ret)
		return NULL;

	while (prev && file_offset >= entry_end(prev_entry)) {
		test = rb_next(prev);
		if (!test)
			break;
		prev_entry = rb_entry(test, struct btrfs_ordered_extent,
				      rb_node);
		if (file_offset < entry_end(prev_entry))
			break;

		prev = test;
	}
	if (prev)
		prev_entry = rb_entry(prev, struct btrfs_ordered_extent,
				      rb_node);
	while (prev && file_offset < entry_end(prev_entry)) {
		test = rb_prev(prev);
		if (!test)
			break;
		prev_entry = rb_entry(test, struct btrfs_ordered_extent,
				      rb_node);
		prev = test;
	}
	*prev_ret = prev;
	return NULL;
}

static int range_overlaps(struct btrfs_ordered_extent *entry, u64 file_offset,
			  u64 len)
{
	if (file_offset + len <= entry->file_offset ||
	    entry->file_offset + entry->num_bytes <= file_offset)
		return 0;
	return 1;
}

 
static inline struct rb_node *tree_search(struct btrfs_ordered_inode_tree *tree,
					  u64 file_offset)
{
	struct rb_root *root = &tree->tree;
	struct rb_node *prev = NULL;
	struct rb_node *ret;
	struct btrfs_ordered_extent *entry;

	if (tree->last) {
		entry = rb_entry(tree->last, struct btrfs_ordered_extent,
				 rb_node);
		if (in_range(file_offset, entry->file_offset, entry->num_bytes))
			return tree->last;
	}
	ret = __tree_search(root, file_offset, &prev);
	if (!ret)
		ret = prev;
	if (ret)
		tree->last = ret;
	return ret;
}

static struct btrfs_ordered_extent *alloc_ordered_extent(
			struct btrfs_inode *inode, u64 file_offset, u64 num_bytes,
			u64 ram_bytes, u64 disk_bytenr, u64 disk_num_bytes,
			u64 offset, unsigned long flags, int compress_type)
{
	struct btrfs_ordered_extent *entry;
	int ret;
	u64 qgroup_rsv = 0;

	if (flags &
	    ((1 << BTRFS_ORDERED_NOCOW) | (1 << BTRFS_ORDERED_PREALLOC))) {
		 
		ret = btrfs_qgroup_free_data(inode, NULL, file_offset, num_bytes, &qgroup_rsv);
		if (ret < 0)
			return ERR_PTR(ret);
	} else {
		 
		ret = btrfs_qgroup_release_data(inode, file_offset, num_bytes, &qgroup_rsv);
		if (ret < 0)
			return ERR_PTR(ret);
	}
	entry = kmem_cache_zalloc(btrfs_ordered_extent_cache, GFP_NOFS);
	if (!entry)
		return ERR_PTR(-ENOMEM);

	entry->file_offset = file_offset;
	entry->num_bytes = num_bytes;
	entry->ram_bytes = ram_bytes;
	entry->disk_bytenr = disk_bytenr;
	entry->disk_num_bytes = disk_num_bytes;
	entry->offset = offset;
	entry->bytes_left = num_bytes;
	entry->inode = igrab(&inode->vfs_inode);
	entry->compress_type = compress_type;
	entry->truncated_len = (u64)-1;
	entry->qgroup_rsv = qgroup_rsv;
	entry->flags = flags;
	refcount_set(&entry->refs, 1);
	init_waitqueue_head(&entry->wait);
	INIT_LIST_HEAD(&entry->list);
	INIT_LIST_HEAD(&entry->log_list);
	INIT_LIST_HEAD(&entry->root_extent_list);
	INIT_LIST_HEAD(&entry->work_list);
	init_completion(&entry->completion);

	 
	spin_lock(&inode->lock);
	btrfs_mod_outstanding_extents(inode, 1);
	spin_unlock(&inode->lock);

	return entry;
}

static void insert_ordered_extent(struct btrfs_ordered_extent *entry)
{
	struct btrfs_inode *inode = BTRFS_I(entry->inode);
	struct btrfs_ordered_inode_tree *tree = &inode->ordered_tree;
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct rb_node *node;

	trace_btrfs_ordered_extent_add(inode, entry);

	percpu_counter_add_batch(&fs_info->ordered_bytes, entry->num_bytes,
				 fs_info->delalloc_batch);

	 
	refcount_inc(&entry->refs);

	spin_lock_irq(&tree->lock);
	node = tree_insert(&tree->tree, entry->file_offset, &entry->rb_node);
	if (node)
		btrfs_panic(fs_info, -EEXIST,
				"inconsistency in ordered tree at offset %llu",
				entry->file_offset);
	spin_unlock_irq(&tree->lock);

	spin_lock(&root->ordered_extent_lock);
	list_add_tail(&entry->root_extent_list,
		      &root->ordered_extents);
	root->nr_ordered_extents++;
	if (root->nr_ordered_extents == 1) {
		spin_lock(&fs_info->ordered_root_lock);
		BUG_ON(!list_empty(&root->ordered_root));
		list_add_tail(&root->ordered_root, &fs_info->ordered_roots);
		spin_unlock(&fs_info->ordered_root_lock);
	}
	spin_unlock(&root->ordered_extent_lock);
}

 
struct btrfs_ordered_extent *btrfs_alloc_ordered_extent(
			struct btrfs_inode *inode, u64 file_offset,
			u64 num_bytes, u64 ram_bytes, u64 disk_bytenr,
			u64 disk_num_bytes, u64 offset, unsigned long flags,
			int compress_type)
{
	struct btrfs_ordered_extent *entry;

	ASSERT((flags & ~BTRFS_ORDERED_TYPE_FLAGS) == 0);

	entry = alloc_ordered_extent(inode, file_offset, num_bytes, ram_bytes,
				     disk_bytenr, disk_num_bytes, offset, flags,
				     compress_type);
	if (!IS_ERR(entry))
		insert_ordered_extent(entry);
	return entry;
}

 
void btrfs_add_ordered_sum(struct btrfs_ordered_extent *entry,
			   struct btrfs_ordered_sum *sum)
{
	struct btrfs_ordered_inode_tree *tree;

	tree = &BTRFS_I(entry->inode)->ordered_tree;
	spin_lock_irq(&tree->lock);
	list_add_tail(&sum->list, &entry->list);
	spin_unlock_irq(&tree->lock);
}

static void finish_ordered_fn(struct btrfs_work *work)
{
	struct btrfs_ordered_extent *ordered_extent;

	ordered_extent = container_of(work, struct btrfs_ordered_extent, work);
	btrfs_finish_ordered_io(ordered_extent);
}

static bool can_finish_ordered_extent(struct btrfs_ordered_extent *ordered,
				      struct page *page, u64 file_offset,
				      u64 len, bool uptodate)
{
	struct btrfs_inode *inode = BTRFS_I(ordered->inode);
	struct btrfs_fs_info *fs_info = inode->root->fs_info;

	lockdep_assert_held(&inode->ordered_tree.lock);

	if (page) {
		ASSERT(page->mapping);
		ASSERT(page_offset(page) <= file_offset);
		ASSERT(file_offset + len <= page_offset(page) + PAGE_SIZE);

		 
		if (!btrfs_page_test_ordered(fs_info, page, file_offset, len))
			return false;
		btrfs_page_clear_ordered(fs_info, page, file_offset, len);
	}

	 
	if (WARN_ON_ONCE(len > ordered->bytes_left)) {
		btrfs_crit(fs_info,
"bad ordered extent accounting, root=%llu ino=%llu OE offset=%llu OE len=%llu to_dec=%llu left=%llu",
			   inode->root->root_key.objectid, btrfs_ino(inode),
			   ordered->file_offset, ordered->num_bytes,
			   len, ordered->bytes_left);
		ordered->bytes_left = 0;
	} else {
		ordered->bytes_left -= len;
	}

	if (!uptodate)
		set_bit(BTRFS_ORDERED_IOERR, &ordered->flags);

	if (ordered->bytes_left)
		return false;

	 
	set_bit(BTRFS_ORDERED_IO_DONE, &ordered->flags);
	cond_wake_up(&ordered->wait);
	refcount_inc(&ordered->refs);
	trace_btrfs_ordered_extent_mark_finished(inode, ordered);
	return true;
}

static void btrfs_queue_ordered_fn(struct btrfs_ordered_extent *ordered)
{
	struct btrfs_inode *inode = BTRFS_I(ordered->inode);
	struct btrfs_fs_info *fs_info = inode->root->fs_info;
	struct btrfs_workqueue *wq = btrfs_is_free_space_inode(inode) ?
		fs_info->endio_freespace_worker : fs_info->endio_write_workers;

	btrfs_init_work(&ordered->work, finish_ordered_fn, NULL, NULL);
	btrfs_queue_work(wq, &ordered->work);
}

bool btrfs_finish_ordered_extent(struct btrfs_ordered_extent *ordered,
				 struct page *page, u64 file_offset, u64 len,
				 bool uptodate)
{
	struct btrfs_inode *inode = BTRFS_I(ordered->inode);
	unsigned long flags;
	bool ret;

	trace_btrfs_finish_ordered_extent(inode, file_offset, len, uptodate);

	spin_lock_irqsave(&inode->ordered_tree.lock, flags);
	ret = can_finish_ordered_extent(ordered, page, file_offset, len, uptodate);
	spin_unlock_irqrestore(&inode->ordered_tree.lock, flags);

	if (ret)
		btrfs_queue_ordered_fn(ordered);
	return ret;
}

 
void btrfs_mark_ordered_io_finished(struct btrfs_inode *inode,
				    struct page *page, u64 file_offset,
				    u64 num_bytes, bool uptodate)
{
	struct btrfs_ordered_inode_tree *tree = &inode->ordered_tree;
	struct rb_node *node;
	struct btrfs_ordered_extent *entry = NULL;
	unsigned long flags;
	u64 cur = file_offset;

	trace_btrfs_writepage_end_io_hook(inode, file_offset,
					  file_offset + num_bytes - 1,
					  uptodate);

	spin_lock_irqsave(&tree->lock, flags);
	while (cur < file_offset + num_bytes) {
		u64 entry_end;
		u64 end;
		u32 len;

		node = tree_search(tree, cur);
		 
		if (!node)
			break;

		entry = rb_entry(node, struct btrfs_ordered_extent, rb_node);
		entry_end = entry->file_offset + entry->num_bytes;
		 
		if (cur >= entry_end) {
			node = rb_next(node);
			 
			if (!node)
				break;
			entry = rb_entry(node, struct btrfs_ordered_extent,
					 rb_node);

			 
			cur = entry->file_offset;
			continue;
		}
		 
		if (cur < entry->file_offset) {
			cur = entry->file_offset;
			continue;
		}

		 
		end = min(entry->file_offset + entry->num_bytes,
			  file_offset + num_bytes) - 1;
		ASSERT(end + 1 - cur < U32_MAX);
		len = end + 1 - cur;

		if (can_finish_ordered_extent(entry, page, cur, len, uptodate)) {
			spin_unlock_irqrestore(&tree->lock, flags);
			btrfs_queue_ordered_fn(entry);
			spin_lock_irqsave(&tree->lock, flags);
		}
		cur += len;
	}
	spin_unlock_irqrestore(&tree->lock, flags);
}

 
bool btrfs_dec_test_ordered_pending(struct btrfs_inode *inode,
				    struct btrfs_ordered_extent **cached,
				    u64 file_offset, u64 io_size)
{
	struct btrfs_ordered_inode_tree *tree = &inode->ordered_tree;
	struct rb_node *node;
	struct btrfs_ordered_extent *entry = NULL;
	unsigned long flags;
	bool finished = false;

	spin_lock_irqsave(&tree->lock, flags);
	if (cached && *cached) {
		entry = *cached;
		goto have_entry;
	}

	node = tree_search(tree, file_offset);
	if (!node)
		goto out;

	entry = rb_entry(node, struct btrfs_ordered_extent, rb_node);
have_entry:
	if (!in_range(file_offset, entry->file_offset, entry->num_bytes))
		goto out;

	if (io_size > entry->bytes_left)
		btrfs_crit(inode->root->fs_info,
			   "bad ordered accounting left %llu size %llu",
		       entry->bytes_left, io_size);

	entry->bytes_left -= io_size;

	if (entry->bytes_left == 0) {
		 
		finished = !test_and_set_bit(BTRFS_ORDERED_IO_DONE, &entry->flags);
		 
		cond_wake_up_nomb(&entry->wait);
	}
out:
	if (finished && cached && entry) {
		*cached = entry;
		refcount_inc(&entry->refs);
		trace_btrfs_ordered_extent_dec_test_pending(inode, entry);
	}
	spin_unlock_irqrestore(&tree->lock, flags);
	return finished;
}

 
void btrfs_put_ordered_extent(struct btrfs_ordered_extent *entry)
{
	struct list_head *cur;
	struct btrfs_ordered_sum *sum;

	trace_btrfs_ordered_extent_put(BTRFS_I(entry->inode), entry);

	if (refcount_dec_and_test(&entry->refs)) {
		ASSERT(list_empty(&entry->root_extent_list));
		ASSERT(list_empty(&entry->log_list));
		ASSERT(RB_EMPTY_NODE(&entry->rb_node));
		if (entry->inode)
			btrfs_add_delayed_iput(BTRFS_I(entry->inode));
		while (!list_empty(&entry->list)) {
			cur = entry->list.next;
			sum = list_entry(cur, struct btrfs_ordered_sum, list);
			list_del(&sum->list);
			kvfree(sum);
		}
		kmem_cache_free(btrfs_ordered_extent_cache, entry);
	}
}

 
void btrfs_remove_ordered_extent(struct btrfs_inode *btrfs_inode,
				 struct btrfs_ordered_extent *entry)
{
	struct btrfs_ordered_inode_tree *tree;
	struct btrfs_root *root = btrfs_inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	struct rb_node *node;
	bool pending;
	bool freespace_inode;

	 
	freespace_inode = btrfs_is_free_space_inode(btrfs_inode);

	btrfs_lockdep_acquire(fs_info, btrfs_trans_pending_ordered);
	 
	spin_lock(&btrfs_inode->lock);
	btrfs_mod_outstanding_extents(btrfs_inode, -1);
	spin_unlock(&btrfs_inode->lock);
	if (root != fs_info->tree_root) {
		u64 release;

		if (test_bit(BTRFS_ORDERED_ENCODED, &entry->flags))
			release = entry->disk_num_bytes;
		else
			release = entry->num_bytes;
		btrfs_delalloc_release_metadata(btrfs_inode, release,
						test_bit(BTRFS_ORDERED_IOERR,
							 &entry->flags));
	}

	percpu_counter_add_batch(&fs_info->ordered_bytes, -entry->num_bytes,
				 fs_info->delalloc_batch);

	tree = &btrfs_inode->ordered_tree;
	spin_lock_irq(&tree->lock);
	node = &entry->rb_node;
	rb_erase(node, &tree->tree);
	RB_CLEAR_NODE(node);
	if (tree->last == node)
		tree->last = NULL;
	set_bit(BTRFS_ORDERED_COMPLETE, &entry->flags);
	pending = test_and_clear_bit(BTRFS_ORDERED_PENDING, &entry->flags);
	spin_unlock_irq(&tree->lock);

	 
	if (pending) {
		struct btrfs_transaction *trans;

		 
		spin_lock(&fs_info->trans_lock);
		trans = fs_info->running_transaction;
		if (trans)
			refcount_inc(&trans->use_count);
		spin_unlock(&fs_info->trans_lock);

		ASSERT(trans || BTRFS_FS_ERROR(fs_info));
		if (trans) {
			if (atomic_dec_and_test(&trans->pending_ordered))
				wake_up(&trans->pending_wait);
			btrfs_put_transaction(trans);
		}
	}

	btrfs_lockdep_release(fs_info, btrfs_trans_pending_ordered);

	spin_lock(&root->ordered_extent_lock);
	list_del_init(&entry->root_extent_list);
	root->nr_ordered_extents--;

	trace_btrfs_ordered_extent_remove(btrfs_inode, entry);

	if (!root->nr_ordered_extents) {
		spin_lock(&fs_info->ordered_root_lock);
		BUG_ON(list_empty(&root->ordered_root));
		list_del_init(&root->ordered_root);
		spin_unlock(&fs_info->ordered_root_lock);
	}
	spin_unlock(&root->ordered_extent_lock);
	wake_up(&entry->wait);
	if (!freespace_inode)
		btrfs_lockdep_release(fs_info, btrfs_ordered_extent);
}

static void btrfs_run_ordered_extent_work(struct btrfs_work *work)
{
	struct btrfs_ordered_extent *ordered;

	ordered = container_of(work, struct btrfs_ordered_extent, flush_work);
	btrfs_start_ordered_extent(ordered);
	complete(&ordered->completion);
}

 
u64 btrfs_wait_ordered_extents(struct btrfs_root *root, u64 nr,
			       const u64 range_start, const u64 range_len)
{
	struct btrfs_fs_info *fs_info = root->fs_info;
	LIST_HEAD(splice);
	LIST_HEAD(skipped);
	LIST_HEAD(works);
	struct btrfs_ordered_extent *ordered, *next;
	u64 count = 0;
	const u64 range_end = range_start + range_len;

	mutex_lock(&root->ordered_extent_mutex);
	spin_lock(&root->ordered_extent_lock);
	list_splice_init(&root->ordered_extents, &splice);
	while (!list_empty(&splice) && nr) {
		ordered = list_first_entry(&splice, struct btrfs_ordered_extent,
					   root_extent_list);

		if (range_end <= ordered->disk_bytenr ||
		    ordered->disk_bytenr + ordered->disk_num_bytes <= range_start) {
			list_move_tail(&ordered->root_extent_list, &skipped);
			cond_resched_lock(&root->ordered_extent_lock);
			continue;
		}

		list_move_tail(&ordered->root_extent_list,
			       &root->ordered_extents);
		refcount_inc(&ordered->refs);
		spin_unlock(&root->ordered_extent_lock);

		btrfs_init_work(&ordered->flush_work,
				btrfs_run_ordered_extent_work, NULL, NULL);
		list_add_tail(&ordered->work_list, &works);
		btrfs_queue_work(fs_info->flush_workers, &ordered->flush_work);

		cond_resched();
		spin_lock(&root->ordered_extent_lock);
		if (nr != U64_MAX)
			nr--;
		count++;
	}
	list_splice_tail(&skipped, &root->ordered_extents);
	list_splice_tail(&splice, &root->ordered_extents);
	spin_unlock(&root->ordered_extent_lock);

	list_for_each_entry_safe(ordered, next, &works, work_list) {
		list_del_init(&ordered->work_list);
		wait_for_completion(&ordered->completion);
		btrfs_put_ordered_extent(ordered);
		cond_resched();
	}
	mutex_unlock(&root->ordered_extent_mutex);

	return count;
}

void btrfs_wait_ordered_roots(struct btrfs_fs_info *fs_info, u64 nr,
			     const u64 range_start, const u64 range_len)
{
	struct btrfs_root *root;
	LIST_HEAD(splice);
	u64 done;

	mutex_lock(&fs_info->ordered_operations_mutex);
	spin_lock(&fs_info->ordered_root_lock);
	list_splice_init(&fs_info->ordered_roots, &splice);
	while (!list_empty(&splice) && nr) {
		root = list_first_entry(&splice, struct btrfs_root,
					ordered_root);
		root = btrfs_grab_root(root);
		BUG_ON(!root);
		list_move_tail(&root->ordered_root,
			       &fs_info->ordered_roots);
		spin_unlock(&fs_info->ordered_root_lock);

		done = btrfs_wait_ordered_extents(root, nr,
						  range_start, range_len);
		btrfs_put_root(root);

		spin_lock(&fs_info->ordered_root_lock);
		if (nr != U64_MAX) {
			nr -= done;
		}
	}
	list_splice_tail(&splice, &fs_info->ordered_roots);
	spin_unlock(&fs_info->ordered_root_lock);
	mutex_unlock(&fs_info->ordered_operations_mutex);
}

 
void btrfs_start_ordered_extent(struct btrfs_ordered_extent *entry)
{
	u64 start = entry->file_offset;
	u64 end = start + entry->num_bytes - 1;
	struct btrfs_inode *inode = BTRFS_I(entry->inode);
	bool freespace_inode;

	trace_btrfs_ordered_extent_start(inode, entry);

	 
	freespace_inode = btrfs_is_free_space_inode(inode);

	 
	if (!test_bit(BTRFS_ORDERED_DIRECT, &entry->flags))
		filemap_fdatawrite_range(inode->vfs_inode.i_mapping, start, end);

	if (!freespace_inode)
		btrfs_might_wait_for_event(inode->root->fs_info, btrfs_ordered_extent);
	wait_event(entry->wait, test_bit(BTRFS_ORDERED_COMPLETE, &entry->flags));
}

 
int btrfs_wait_ordered_range(struct inode *inode, u64 start, u64 len)
{
	int ret = 0;
	int ret_wb = 0;
	u64 end;
	u64 orig_end;
	struct btrfs_ordered_extent *ordered;

	if (start + len < start) {
		orig_end = OFFSET_MAX;
	} else {
		orig_end = start + len - 1;
		if (orig_end > OFFSET_MAX)
			orig_end = OFFSET_MAX;
	}

	 
	ret = btrfs_fdatawrite_range(inode, start, orig_end);
	if (ret)
		return ret;

	 
	ret_wb = filemap_fdatawait_range(inode->i_mapping, start, orig_end);

	end = orig_end;
	while (1) {
		ordered = btrfs_lookup_first_ordered_extent(BTRFS_I(inode), end);
		if (!ordered)
			break;
		if (ordered->file_offset > orig_end) {
			btrfs_put_ordered_extent(ordered);
			break;
		}
		if (ordered->file_offset + ordered->num_bytes <= start) {
			btrfs_put_ordered_extent(ordered);
			break;
		}
		btrfs_start_ordered_extent(ordered);
		end = ordered->file_offset;
		 
		if (test_bit(BTRFS_ORDERED_IOERR, &ordered->flags))
			ret = -EIO;
		btrfs_put_ordered_extent(ordered);
		if (end == 0 || end == start)
			break;
		end--;
	}
	return ret_wb ? ret_wb : ret;
}

 
struct btrfs_ordered_extent *btrfs_lookup_ordered_extent(struct btrfs_inode *inode,
							 u64 file_offset)
{
	struct btrfs_ordered_inode_tree *tree;
	struct rb_node *node;
	struct btrfs_ordered_extent *entry = NULL;
	unsigned long flags;

	tree = &inode->ordered_tree;
	spin_lock_irqsave(&tree->lock, flags);
	node = tree_search(tree, file_offset);
	if (!node)
		goto out;

	entry = rb_entry(node, struct btrfs_ordered_extent, rb_node);
	if (!in_range(file_offset, entry->file_offset, entry->num_bytes))
		entry = NULL;
	if (entry) {
		refcount_inc(&entry->refs);
		trace_btrfs_ordered_extent_lookup(inode, entry);
	}
out:
	spin_unlock_irqrestore(&tree->lock, flags);
	return entry;
}

 
struct btrfs_ordered_extent *btrfs_lookup_ordered_range(
		struct btrfs_inode *inode, u64 file_offset, u64 len)
{
	struct btrfs_ordered_inode_tree *tree;
	struct rb_node *node;
	struct btrfs_ordered_extent *entry = NULL;

	tree = &inode->ordered_tree;
	spin_lock_irq(&tree->lock);
	node = tree_search(tree, file_offset);
	if (!node) {
		node = tree_search(tree, file_offset + len);
		if (!node)
			goto out;
	}

	while (1) {
		entry = rb_entry(node, struct btrfs_ordered_extent, rb_node);
		if (range_overlaps(entry, file_offset, len))
			break;

		if (entry->file_offset >= file_offset + len) {
			entry = NULL;
			break;
		}
		entry = NULL;
		node = rb_next(node);
		if (!node)
			break;
	}
out:
	if (entry) {
		refcount_inc(&entry->refs);
		trace_btrfs_ordered_extent_lookup_range(inode, entry);
	}
	spin_unlock_irq(&tree->lock);
	return entry;
}

 
void btrfs_get_ordered_extents_for_logging(struct btrfs_inode *inode,
					   struct list_head *list)
{
	struct btrfs_ordered_inode_tree *tree = &inode->ordered_tree;
	struct rb_node *n;

	ASSERT(inode_is_locked(&inode->vfs_inode));

	spin_lock_irq(&tree->lock);
	for (n = rb_first(&tree->tree); n; n = rb_next(n)) {
		struct btrfs_ordered_extent *ordered;

		ordered = rb_entry(n, struct btrfs_ordered_extent, rb_node);

		if (test_bit(BTRFS_ORDERED_LOGGED, &ordered->flags))
			continue;

		ASSERT(list_empty(&ordered->log_list));
		list_add_tail(&ordered->log_list, list);
		refcount_inc(&ordered->refs);
		trace_btrfs_ordered_extent_lookup_for_logging(inode, ordered);
	}
	spin_unlock_irq(&tree->lock);
}

 
struct btrfs_ordered_extent *
btrfs_lookup_first_ordered_extent(struct btrfs_inode *inode, u64 file_offset)
{
	struct btrfs_ordered_inode_tree *tree;
	struct rb_node *node;
	struct btrfs_ordered_extent *entry = NULL;

	tree = &inode->ordered_tree;
	spin_lock_irq(&tree->lock);
	node = tree_search(tree, file_offset);
	if (!node)
		goto out;

	entry = rb_entry(node, struct btrfs_ordered_extent, rb_node);
	refcount_inc(&entry->refs);
	trace_btrfs_ordered_extent_lookup_first(inode, entry);
out:
	spin_unlock_irq(&tree->lock);
	return entry;
}

 
struct btrfs_ordered_extent *btrfs_lookup_first_ordered_range(
			struct btrfs_inode *inode, u64 file_offset, u64 len)
{
	struct btrfs_ordered_inode_tree *tree = &inode->ordered_tree;
	struct rb_node *node;
	struct rb_node *cur;
	struct rb_node *prev;
	struct rb_node *next;
	struct btrfs_ordered_extent *entry = NULL;

	spin_lock_irq(&tree->lock);
	node = tree->tree.rb_node;
	 
	while (node) {
		entry = rb_entry(node, struct btrfs_ordered_extent, rb_node);

		if (file_offset < entry->file_offset) {
			node = node->rb_left;
		} else if (file_offset >= entry_end(entry)) {
			node = node->rb_right;
		} else {
			 
			goto out;
		}
	}
	if (!entry) {
		 
		goto out;
	}

	cur = &entry->rb_node;
	 
	if (entry->file_offset < file_offset) {
		prev = cur;
		next = rb_next(cur);
	} else {
		prev = rb_prev(cur);
		next = cur;
	}
	if (prev) {
		entry = rb_entry(prev, struct btrfs_ordered_extent, rb_node);
		if (range_overlaps(entry, file_offset, len))
			goto out;
	}
	if (next) {
		entry = rb_entry(next, struct btrfs_ordered_extent, rb_node);
		if (range_overlaps(entry, file_offset, len))
			goto out;
	}
	 
	entry = NULL;
out:
	if (entry) {
		refcount_inc(&entry->refs);
		trace_btrfs_ordered_extent_lookup_first_range(inode, entry);
	}

	spin_unlock_irq(&tree->lock);
	return entry;
}

 
void btrfs_lock_and_flush_ordered_range(struct btrfs_inode *inode, u64 start,
					u64 end,
					struct extent_state **cached_state)
{
	struct btrfs_ordered_extent *ordered;
	struct extent_state *cache = NULL;
	struct extent_state **cachedp = &cache;

	if (cached_state)
		cachedp = cached_state;

	while (1) {
		lock_extent(&inode->io_tree, start, end, cachedp);
		ordered = btrfs_lookup_ordered_range(inode, start,
						     end - start + 1);
		if (!ordered) {
			 
			if (!cached_state)
				refcount_dec(&cache->refs);
			break;
		}
		unlock_extent(&inode->io_tree, start, end, cachedp);
		btrfs_start_ordered_extent(ordered);
		btrfs_put_ordered_extent(ordered);
	}
}

 
bool btrfs_try_lock_ordered_range(struct btrfs_inode *inode, u64 start, u64 end,
				  struct extent_state **cached_state)
{
	struct btrfs_ordered_extent *ordered;

	if (!try_lock_extent(&inode->io_tree, start, end, cached_state))
		return false;

	ordered = btrfs_lookup_ordered_range(inode, start, end - start + 1);
	if (!ordered)
		return true;

	btrfs_put_ordered_extent(ordered);
	unlock_extent(&inode->io_tree, start, end, cached_state);

	return false;
}

 
struct btrfs_ordered_extent *btrfs_split_ordered_extent(
			struct btrfs_ordered_extent *ordered, u64 len)
{
	struct btrfs_inode *inode = BTRFS_I(ordered->inode);
	struct btrfs_ordered_inode_tree *tree = &inode->ordered_tree;
	struct btrfs_root *root = inode->root;
	struct btrfs_fs_info *fs_info = root->fs_info;
	u64 file_offset = ordered->file_offset;
	u64 disk_bytenr = ordered->disk_bytenr;
	unsigned long flags = ordered->flags;
	struct btrfs_ordered_sum *sum, *tmpsum;
	struct btrfs_ordered_extent *new;
	struct rb_node *node;
	u64 offset = 0;

	trace_btrfs_ordered_extent_split(inode, ordered);

	ASSERT(!(flags & (1U << BTRFS_ORDERED_COMPRESSED)));

	 
	if (WARN_ON_ONCE(len >= ordered->num_bytes))
		return ERR_PTR(-EINVAL);
	 
	if (ordered->bytes_left) {
		ASSERT(!(flags & ~BTRFS_ORDERED_TYPE_FLAGS));
		if (WARN_ON_ONCE(ordered->bytes_left != ordered->disk_num_bytes))
			return ERR_PTR(-EINVAL);
	}
	 
	if (WARN_ON_ONCE(ordered->disk_num_bytes != ordered->num_bytes))
		return ERR_PTR(-EINVAL);

	new = alloc_ordered_extent(inode, file_offset, len, len, disk_bytenr,
				   len, 0, flags, ordered->compress_type);
	if (IS_ERR(new))
		return new;

	 
	refcount_inc(&new->refs);

	spin_lock_irq(&root->ordered_extent_lock);
	spin_lock(&tree->lock);
	 
	node = &ordered->rb_node;
	rb_erase(node, &tree->tree);
	RB_CLEAR_NODE(node);
	if (tree->last == node)
		tree->last = NULL;

	ordered->file_offset += len;
	ordered->disk_bytenr += len;
	ordered->num_bytes -= len;
	ordered->disk_num_bytes -= len;

	if (test_bit(BTRFS_ORDERED_IO_DONE, &ordered->flags)) {
		ASSERT(ordered->bytes_left == 0);
		new->bytes_left = 0;
	} else {
		ordered->bytes_left -= len;
	}

	if (test_bit(BTRFS_ORDERED_TRUNCATED, &ordered->flags)) {
		if (ordered->truncated_len > len) {
			ordered->truncated_len -= len;
		} else {
			new->truncated_len = ordered->truncated_len;
			ordered->truncated_len = 0;
		}
	}

	list_for_each_entry_safe(sum, tmpsum, &ordered->list, list) {
		if (offset == len)
			break;
		list_move_tail(&sum->list, &new->list);
		offset += sum->len;
	}

	 
	node = tree_insert(&tree->tree, ordered->file_offset, &ordered->rb_node);
	if (node)
		btrfs_panic(fs_info, -EEXIST,
			"zoned: inconsistency in ordered tree at offset %llu",
			ordered->file_offset);

	node = tree_insert(&tree->tree, new->file_offset, &new->rb_node);
	if (node)
		btrfs_panic(fs_info, -EEXIST,
			"zoned: inconsistency in ordered tree at offset %llu",
			new->file_offset);
	spin_unlock(&tree->lock);

	list_add_tail(&new->root_extent_list, &root->ordered_extents);
	root->nr_ordered_extents++;
	spin_unlock_irq(&root->ordered_extent_lock);
	return new;
}

int __init ordered_data_init(void)
{
	btrfs_ordered_extent_cache = kmem_cache_create("btrfs_ordered_extent",
				     sizeof(struct btrfs_ordered_extent), 0,
				     SLAB_MEM_SPREAD,
				     NULL);
	if (!btrfs_ordered_extent_cache)
		return -ENOMEM;

	return 0;
}

void __cold ordered_data_exit(void)
{
	kmem_cache_destroy(btrfs_ordered_extent_cache);
}

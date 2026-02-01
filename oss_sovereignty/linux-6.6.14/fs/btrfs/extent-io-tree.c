

#include <linux/slab.h>
#include <trace/events/btrfs.h>
#include "messages.h"
#include "ctree.h"
#include "extent-io-tree.h"
#include "btrfs_inode.h"
#include "misc.h"

static struct kmem_cache *extent_state_cache;

static inline bool extent_state_in_tree(const struct extent_state *state)
{
	return !RB_EMPTY_NODE(&state->rb_node);
}

#ifdef CONFIG_BTRFS_DEBUG
static LIST_HEAD(states);
static DEFINE_SPINLOCK(leak_lock);

static inline void btrfs_leak_debug_add_state(struct extent_state *state)
{
	unsigned long flags;

	spin_lock_irqsave(&leak_lock, flags);
	list_add(&state->leak_list, &states);
	spin_unlock_irqrestore(&leak_lock, flags);
}

static inline void btrfs_leak_debug_del_state(struct extent_state *state)
{
	unsigned long flags;

	spin_lock_irqsave(&leak_lock, flags);
	list_del(&state->leak_list);
	spin_unlock_irqrestore(&leak_lock, flags);
}

static inline void btrfs_extent_state_leak_debug_check(void)
{
	struct extent_state *state;

	while (!list_empty(&states)) {
		state = list_entry(states.next, struct extent_state, leak_list);
		pr_err("BTRFS: state leak: start %llu end %llu state %u in tree %d refs %d\n",
		       state->start, state->end, state->state,
		       extent_state_in_tree(state),
		       refcount_read(&state->refs));
		list_del(&state->leak_list);
		kmem_cache_free(extent_state_cache, state);
	}
}

#define btrfs_debug_check_extent_io_range(tree, start, end)		\
	__btrfs_debug_check_extent_io_range(__func__, (tree), (start), (end))
static inline void __btrfs_debug_check_extent_io_range(const char *caller,
						       struct extent_io_tree *tree,
						       u64 start, u64 end)
{
	struct btrfs_inode *inode = tree->inode;
	u64 isize;

	if (!inode)
		return;

	isize = i_size_read(&inode->vfs_inode);
	if (end >= PAGE_SIZE && (end % 2) == 0 && end != isize - 1) {
		btrfs_debug_rl(inode->root->fs_info,
		    "%s: ino %llu isize %llu odd range [%llu,%llu]",
			caller, btrfs_ino(inode), isize, start, end);
	}
}
#else
#define btrfs_leak_debug_add_state(state)		do {} while (0)
#define btrfs_leak_debug_del_state(state)		do {} while (0)
#define btrfs_extent_state_leak_debug_check()		do {} while (0)
#define btrfs_debug_check_extent_io_range(c, s, e)	do {} while (0)
#endif

 
static struct lock_class_key file_extent_tree_class;

struct tree_entry {
	u64 start;
	u64 end;
	struct rb_node rb_node;
};

void extent_io_tree_init(struct btrfs_fs_info *fs_info,
			 struct extent_io_tree *tree, unsigned int owner)
{
	tree->fs_info = fs_info;
	tree->state = RB_ROOT;
	spin_lock_init(&tree->lock);
	tree->inode = NULL;
	tree->owner = owner;
	if (owner == IO_TREE_INODE_FILE_EXTENT)
		lockdep_set_class(&tree->lock, &file_extent_tree_class);
}

void extent_io_tree_release(struct extent_io_tree *tree)
{
	spin_lock(&tree->lock);
	 
	smp_mb();
	while (!RB_EMPTY_ROOT(&tree->state)) {
		struct rb_node *node;
		struct extent_state *state;

		node = rb_first(&tree->state);
		state = rb_entry(node, struct extent_state, rb_node);
		rb_erase(&state->rb_node, &tree->state);
		RB_CLEAR_NODE(&state->rb_node);
		 
		ASSERT(!waitqueue_active(&state->wq));
		free_extent_state(state);

		cond_resched_lock(&tree->lock);
	}
	spin_unlock(&tree->lock);
}

static struct extent_state *alloc_extent_state(gfp_t mask)
{
	struct extent_state *state;

	 
	mask &= ~(__GFP_DMA32|__GFP_HIGHMEM);
	state = kmem_cache_alloc(extent_state_cache, mask);
	if (!state)
		return state;
	state->state = 0;
	RB_CLEAR_NODE(&state->rb_node);
	btrfs_leak_debug_add_state(state);
	refcount_set(&state->refs, 1);
	init_waitqueue_head(&state->wq);
	trace_alloc_extent_state(state, mask, _RET_IP_);
	return state;
}

static struct extent_state *alloc_extent_state_atomic(struct extent_state *prealloc)
{
	if (!prealloc)
		prealloc = alloc_extent_state(GFP_ATOMIC);

	return prealloc;
}

void free_extent_state(struct extent_state *state)
{
	if (!state)
		return;
	if (refcount_dec_and_test(&state->refs)) {
		WARN_ON(extent_state_in_tree(state));
		btrfs_leak_debug_del_state(state);
		trace_free_extent_state(state, _RET_IP_);
		kmem_cache_free(extent_state_cache, state);
	}
}

static int add_extent_changeset(struct extent_state *state, u32 bits,
				 struct extent_changeset *changeset,
				 int set)
{
	int ret;

	if (!changeset)
		return 0;
	if (set && (state->state & bits) == bits)
		return 0;
	if (!set && (state->state & bits) == 0)
		return 0;
	changeset->bytes_changed += state->end - state->start + 1;
	ret = ulist_add(&changeset->range_changed, state->start, state->end,
			GFP_ATOMIC);
	return ret;
}

static inline struct extent_state *next_state(struct extent_state *state)
{
	struct rb_node *next = rb_next(&state->rb_node);

	if (next)
		return rb_entry(next, struct extent_state, rb_node);
	else
		return NULL;
}

static inline struct extent_state *prev_state(struct extent_state *state)
{
	struct rb_node *next = rb_prev(&state->rb_node);

	if (next)
		return rb_entry(next, struct extent_state, rb_node);
	else
		return NULL;
}

 
static inline struct extent_state *tree_search_for_insert(struct extent_io_tree *tree,
							  u64 offset,
							  struct rb_node ***node_ret,
							  struct rb_node **parent_ret)
{
	struct rb_root *root = &tree->state;
	struct rb_node **node = &root->rb_node;
	struct rb_node *prev = NULL;
	struct extent_state *entry = NULL;

	while (*node) {
		prev = *node;
		entry = rb_entry(prev, struct extent_state, rb_node);

		if (offset < entry->start)
			node = &(*node)->rb_left;
		else if (offset > entry->end)
			node = &(*node)->rb_right;
		else
			return entry;
	}

	if (node_ret)
		*node_ret = node;
	if (parent_ret)
		*parent_ret = prev;

	 
	while (entry && offset > entry->end)
		entry = next_state(entry);

	return entry;
}

 
static struct extent_state *tree_search_prev_next(struct extent_io_tree *tree,
						  u64 offset,
						  struct extent_state **prev_ret,
						  struct extent_state **next_ret)
{
	struct rb_root *root = &tree->state;
	struct rb_node **node = &root->rb_node;
	struct extent_state *orig_prev;
	struct extent_state *entry = NULL;

	ASSERT(prev_ret);
	ASSERT(next_ret);

	while (*node) {
		entry = rb_entry(*node, struct extent_state, rb_node);

		if (offset < entry->start)
			node = &(*node)->rb_left;
		else if (offset > entry->end)
			node = &(*node)->rb_right;
		else
			return entry;
	}

	orig_prev = entry;
	while (entry && offset > entry->end)
		entry = next_state(entry);
	*next_ret = entry;
	entry = orig_prev;

	while (entry && offset < entry->start)
		entry = prev_state(entry);
	*prev_ret = entry;

	return NULL;
}

 
static inline struct extent_state *tree_search(struct extent_io_tree *tree, u64 offset)
{
	return tree_search_for_insert(tree, offset, NULL, NULL);
}

static void extent_io_tree_panic(struct extent_io_tree *tree, int err)
{
	btrfs_panic(tree->fs_info, err,
	"locking error: extent tree was modified by another thread while locked");
}

 
static void merge_state(struct extent_io_tree *tree, struct extent_state *state)
{
	struct extent_state *other;

	if (state->state & (EXTENT_LOCKED | EXTENT_BOUNDARY))
		return;

	other = prev_state(state);
	if (other && other->end == state->start - 1 &&
	    other->state == state->state) {
		if (tree->inode)
			btrfs_merge_delalloc_extent(tree->inode, state, other);
		state->start = other->start;
		rb_erase(&other->rb_node, &tree->state);
		RB_CLEAR_NODE(&other->rb_node);
		free_extent_state(other);
	}
	other = next_state(state);
	if (other && other->start == state->end + 1 &&
	    other->state == state->state) {
		if (tree->inode)
			btrfs_merge_delalloc_extent(tree->inode, state, other);
		state->end = other->end;
		rb_erase(&other->rb_node, &tree->state);
		RB_CLEAR_NODE(&other->rb_node);
		free_extent_state(other);
	}
}

static void set_state_bits(struct extent_io_tree *tree,
			   struct extent_state *state,
			   u32 bits, struct extent_changeset *changeset)
{
	u32 bits_to_set = bits & ~EXTENT_CTLBITS;
	int ret;

	if (tree->inode)
		btrfs_set_delalloc_extent(tree->inode, state, bits);

	ret = add_extent_changeset(state, bits_to_set, changeset, 1);
	BUG_ON(ret < 0);
	state->state |= bits_to_set;
}

 
static int insert_state(struct extent_io_tree *tree,
			struct extent_state *state,
			u32 bits, struct extent_changeset *changeset)
{
	struct rb_node **node;
	struct rb_node *parent = NULL;
	const u64 end = state->end;

	set_state_bits(tree, state, bits, changeset);

	node = &tree->state.rb_node;
	while (*node) {
		struct extent_state *entry;

		parent = *node;
		entry = rb_entry(parent, struct extent_state, rb_node);

		if (end < entry->start) {
			node = &(*node)->rb_left;
		} else if (end > entry->end) {
			node = &(*node)->rb_right;
		} else {
			btrfs_err(tree->fs_info,
			       "found node %llu %llu on insert of %llu %llu",
			       entry->start, entry->end, state->start, end);
			return -EEXIST;
		}
	}

	rb_link_node(&state->rb_node, parent, node);
	rb_insert_color(&state->rb_node, &tree->state);

	merge_state(tree, state);
	return 0;
}

 
static void insert_state_fast(struct extent_io_tree *tree,
			      struct extent_state *state, struct rb_node **node,
			      struct rb_node *parent, unsigned bits,
			      struct extent_changeset *changeset)
{
	set_state_bits(tree, state, bits, changeset);
	rb_link_node(&state->rb_node, parent, node);
	rb_insert_color(&state->rb_node, &tree->state);
	merge_state(tree, state);
}

 
static int split_state(struct extent_io_tree *tree, struct extent_state *orig,
		       struct extent_state *prealloc, u64 split)
{
	struct rb_node *parent = NULL;
	struct rb_node **node;

	if (tree->inode)
		btrfs_split_delalloc_extent(tree->inode, orig, split);

	prealloc->start = orig->start;
	prealloc->end = split - 1;
	prealloc->state = orig->state;
	orig->start = split;

	parent = &orig->rb_node;
	node = &parent;
	while (*node) {
		struct extent_state *entry;

		parent = *node;
		entry = rb_entry(parent, struct extent_state, rb_node);

		if (prealloc->end < entry->start) {
			node = &(*node)->rb_left;
		} else if (prealloc->end > entry->end) {
			node = &(*node)->rb_right;
		} else {
			free_extent_state(prealloc);
			return -EEXIST;
		}
	}

	rb_link_node(&prealloc->rb_node, parent, node);
	rb_insert_color(&prealloc->rb_node, &tree->state);

	return 0;
}

 
static struct extent_state *clear_state_bit(struct extent_io_tree *tree,
					    struct extent_state *state,
					    u32 bits, int wake,
					    struct extent_changeset *changeset)
{
	struct extent_state *next;
	u32 bits_to_clear = bits & ~EXTENT_CTLBITS;
	int ret;

	if (tree->inode)
		btrfs_clear_delalloc_extent(tree->inode, state, bits);

	ret = add_extent_changeset(state, bits_to_clear, changeset, 0);
	BUG_ON(ret < 0);
	state->state &= ~bits_to_clear;
	if (wake)
		wake_up(&state->wq);
	if (state->state == 0) {
		next = next_state(state);
		if (extent_state_in_tree(state)) {
			rb_erase(&state->rb_node, &tree->state);
			RB_CLEAR_NODE(&state->rb_node);
			free_extent_state(state);
		} else {
			WARN_ON(1);
		}
	} else {
		merge_state(tree, state);
		next = next_state(state);
	}
	return next;
}

 
static void set_gfp_mask_from_bits(u32 *bits, gfp_t *mask)
{
	*mask = (*bits & EXTENT_NOWAIT ? GFP_NOWAIT : GFP_NOFS);
	*bits &= EXTENT_NOWAIT - 1;
}

 
int __clear_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
		       u32 bits, struct extent_state **cached_state,
		       struct extent_changeset *changeset)
{
	struct extent_state *state;
	struct extent_state *cached;
	struct extent_state *prealloc = NULL;
	u64 last_end;
	int err;
	int clear = 0;
	int wake;
	int delete = (bits & EXTENT_CLEAR_ALL_BITS);
	gfp_t mask;

	set_gfp_mask_from_bits(&bits, &mask);
	btrfs_debug_check_extent_io_range(tree, start, end);
	trace_btrfs_clear_extent_bit(tree, start, end - start + 1, bits);

	if (delete)
		bits |= ~EXTENT_CTLBITS;

	if (bits & EXTENT_DELALLOC)
		bits |= EXTENT_NORESERVE;

	wake = (bits & EXTENT_LOCKED) ? 1 : 0;
	if (bits & (EXTENT_LOCKED | EXTENT_BOUNDARY))
		clear = 1;
again:
	if (!prealloc) {
		 
		prealloc = alloc_extent_state(mask);
	}

	spin_lock(&tree->lock);
	if (cached_state) {
		cached = *cached_state;

		if (clear) {
			*cached_state = NULL;
			cached_state = NULL;
		}

		if (cached && extent_state_in_tree(cached) &&
		    cached->start <= start && cached->end > start) {
			if (clear)
				refcount_dec(&cached->refs);
			state = cached;
			goto hit_next;
		}
		if (clear)
			free_extent_state(cached);
	}

	 
	state = tree_search(tree, start);
	if (!state)
		goto out;
hit_next:
	if (state->start > end)
		goto out;
	WARN_ON(state->end < start);
	last_end = state->end;

	 
	if (!(state->state & bits)) {
		state = next_state(state);
		goto next;
	}

	 

	if (state->start < start) {
		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc)
			goto search_again;
		err = split_state(tree, state, prealloc, start);
		if (err)
			extent_io_tree_panic(tree, err);

		prealloc = NULL;
		if (err)
			goto out;
		if (state->end <= end) {
			state = clear_state_bit(tree, state, bits, wake, changeset);
			goto next;
		}
		goto search_again;
	}
	 
	if (state->start <= end && state->end > end) {
		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc)
			goto search_again;
		err = split_state(tree, state, prealloc, end + 1);
		if (err)
			extent_io_tree_panic(tree, err);

		if (wake)
			wake_up(&state->wq);

		clear_state_bit(tree, prealloc, bits, wake, changeset);

		prealloc = NULL;
		goto out;
	}

	state = clear_state_bit(tree, state, bits, wake, changeset);
next:
	if (last_end == (u64)-1)
		goto out;
	start = last_end + 1;
	if (start <= end && state && !need_resched())
		goto hit_next;

search_again:
	if (start > end)
		goto out;
	spin_unlock(&tree->lock);
	if (gfpflags_allow_blocking(mask))
		cond_resched();
	goto again;

out:
	spin_unlock(&tree->lock);
	if (prealloc)
		free_extent_state(prealloc);

	return 0;

}

static void wait_on_state(struct extent_io_tree *tree,
			  struct extent_state *state)
		__releases(tree->lock)
		__acquires(tree->lock)
{
	DEFINE_WAIT(wait);
	prepare_to_wait(&state->wq, &wait, TASK_UNINTERRUPTIBLE);
	spin_unlock(&tree->lock);
	schedule();
	spin_lock(&tree->lock);
	finish_wait(&state->wq, &wait);
}

 
void wait_extent_bit(struct extent_io_tree *tree, u64 start, u64 end, u32 bits,
		     struct extent_state **cached_state)
{
	struct extent_state *state;

	btrfs_debug_check_extent_io_range(tree, start, end);

	spin_lock(&tree->lock);
again:
	 
	if (cached_state && *cached_state) {
		state = *cached_state;
		if (extent_state_in_tree(state) &&
		    state->start <= start && start < state->end)
			goto process_node;
	}
	while (1) {
		 
		state = tree_search(tree, start);
process_node:
		if (!state)
			break;
		if (state->start > end)
			goto out;

		if (state->state & bits) {
			start = state->start;
			refcount_inc(&state->refs);
			wait_on_state(tree, state);
			free_extent_state(state);
			goto again;
		}
		start = state->end + 1;

		if (start > end)
			break;

		if (!cond_resched_lock(&tree->lock)) {
			state = next_state(state);
			goto process_node;
		}
	}
out:
	 
	if (cached_state && *cached_state) {
		state = *cached_state;
		*cached_state = NULL;
		free_extent_state(state);
	}
	spin_unlock(&tree->lock);
}

static void cache_state_if_flags(struct extent_state *state,
				 struct extent_state **cached_ptr,
				 unsigned flags)
{
	if (cached_ptr && !(*cached_ptr)) {
		if (!flags || (state->state & flags)) {
			*cached_ptr = state;
			refcount_inc(&state->refs);
		}
	}
}

static void cache_state(struct extent_state *state,
			struct extent_state **cached_ptr)
{
	return cache_state_if_flags(state, cached_ptr,
				    EXTENT_LOCKED | EXTENT_BOUNDARY);
}

 
static struct extent_state *find_first_extent_bit_state(struct extent_io_tree *tree,
							u64 start, u32 bits)
{
	struct extent_state *state;

	 
	state = tree_search(tree, start);
	while (state) {
		if (state->end >= start && (state->state & bits))
			return state;
		state = next_state(state);
	}
	return NULL;
}

 
bool find_first_extent_bit(struct extent_io_tree *tree, u64 start,
			   u64 *start_ret, u64 *end_ret, u32 bits,
			   struct extent_state **cached_state)
{
	struct extent_state *state;
	bool ret = false;

	spin_lock(&tree->lock);
	if (cached_state && *cached_state) {
		state = *cached_state;
		if (state->end == start - 1 && extent_state_in_tree(state)) {
			while ((state = next_state(state)) != NULL) {
				if (state->state & bits)
					goto got_it;
			}
			free_extent_state(*cached_state);
			*cached_state = NULL;
			goto out;
		}
		free_extent_state(*cached_state);
		*cached_state = NULL;
	}

	state = find_first_extent_bit_state(tree, start, bits);
got_it:
	if (state) {
		cache_state_if_flags(state, cached_state, 0);
		*start_ret = state->start;
		*end_ret = state->end;
		ret = true;
	}
out:
	spin_unlock(&tree->lock);
	return ret;
}

 
int find_contiguous_extent_bit(struct extent_io_tree *tree, u64 start,
			       u64 *start_ret, u64 *end_ret, u32 bits)
{
	struct extent_state *state;
	int ret = 1;

	spin_lock(&tree->lock);
	state = find_first_extent_bit_state(tree, start, bits);
	if (state) {
		*start_ret = state->start;
		*end_ret = state->end;
		while ((state = next_state(state)) != NULL) {
			if (state->start > (*end_ret + 1))
				break;
			*end_ret = state->end;
		}
		ret = 0;
	}
	spin_unlock(&tree->lock);
	return ret;
}

 
bool btrfs_find_delalloc_range(struct extent_io_tree *tree, u64 *start,
			       u64 *end, u64 max_bytes,
			       struct extent_state **cached_state)
{
	struct extent_state *state;
	u64 cur_start = *start;
	bool found = false;
	u64 total_bytes = 0;

	spin_lock(&tree->lock);

	 
	state = tree_search(tree, cur_start);
	if (!state) {
		*end = (u64)-1;
		goto out;
	}

	while (state) {
		if (found && (state->start != cur_start ||
			      (state->state & EXTENT_BOUNDARY))) {
			goto out;
		}
		if (!(state->state & EXTENT_DELALLOC)) {
			if (!found)
				*end = state->end;
			goto out;
		}
		if (!found) {
			*start = state->start;
			*cached_state = state;
			refcount_inc(&state->refs);
		}
		found = true;
		*end = state->end;
		cur_start = state->end + 1;
		total_bytes += state->end - state->start + 1;
		if (total_bytes >= max_bytes)
			break;
		state = next_state(state);
	}
out:
	spin_unlock(&tree->lock);
	return found;
}

 
static int __set_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
			    u32 bits, u64 *failed_start,
			    struct extent_state **failed_state,
			    struct extent_state **cached_state,
			    struct extent_changeset *changeset)
{
	struct extent_state *state;
	struct extent_state *prealloc = NULL;
	struct rb_node **p = NULL;
	struct rb_node *parent = NULL;
	int err = 0;
	u64 last_start;
	u64 last_end;
	u32 exclusive_bits = (bits & EXTENT_LOCKED);
	gfp_t mask;

	set_gfp_mask_from_bits(&bits, &mask);
	btrfs_debug_check_extent_io_range(tree, start, end);
	trace_btrfs_set_extent_bit(tree, start, end - start + 1, bits);

	if (exclusive_bits)
		ASSERT(failed_start);
	else
		ASSERT(failed_start == NULL && failed_state == NULL);
again:
	if (!prealloc) {
		 
		prealloc = alloc_extent_state(mask);
	}

	spin_lock(&tree->lock);
	if (cached_state && *cached_state) {
		state = *cached_state;
		if (state->start <= start && state->end > start &&
		    extent_state_in_tree(state))
			goto hit_next;
	}
	 
	state = tree_search_for_insert(tree, start, &p, &parent);
	if (!state) {
		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc)
			goto search_again;
		prealloc->start = start;
		prealloc->end = end;
		insert_state_fast(tree, prealloc, p, parent, bits, changeset);
		cache_state(prealloc, cached_state);
		prealloc = NULL;
		goto out;
	}
hit_next:
	last_start = state->start;
	last_end = state->end;

	 
	if (state->start == start && state->end <= end) {
		if (state->state & exclusive_bits) {
			*failed_start = state->start;
			cache_state(state, failed_state);
			err = -EEXIST;
			goto out;
		}

		set_state_bits(tree, state, bits, changeset);
		cache_state(state, cached_state);
		merge_state(tree, state);
		if (last_end == (u64)-1)
			goto out;
		start = last_end + 1;
		state = next_state(state);
		if (start < end && state && state->start == start &&
		    !need_resched())
			goto hit_next;
		goto search_again;
	}

	 
	if (state->start < start) {
		if (state->state & exclusive_bits) {
			*failed_start = start;
			cache_state(state, failed_state);
			err = -EEXIST;
			goto out;
		}

		 
		if ((state->state & bits) == bits) {
			start = state->end + 1;
			cache_state(state, cached_state);
			goto search_again;
		}

		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc)
			goto search_again;
		err = split_state(tree, state, prealloc, start);
		if (err)
			extent_io_tree_panic(tree, err);

		prealloc = NULL;
		if (err)
			goto out;
		if (state->end <= end) {
			set_state_bits(tree, state, bits, changeset);
			cache_state(state, cached_state);
			merge_state(tree, state);
			if (last_end == (u64)-1)
				goto out;
			start = last_end + 1;
			state = next_state(state);
			if (start < end && state && state->start == start &&
			    !need_resched())
				goto hit_next;
		}
		goto search_again;
	}
	 
	if (state->start > start) {
		u64 this_end;
		if (end < last_start)
			this_end = end;
		else
			this_end = last_start - 1;

		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc)
			goto search_again;

		 
		prealloc->start = start;
		prealloc->end = this_end;
		err = insert_state(tree, prealloc, bits, changeset);
		if (err)
			extent_io_tree_panic(tree, err);

		cache_state(prealloc, cached_state);
		prealloc = NULL;
		start = this_end + 1;
		goto search_again;
	}
	 
	if (state->start <= end && state->end > end) {
		if (state->state & exclusive_bits) {
			*failed_start = start;
			cache_state(state, failed_state);
			err = -EEXIST;
			goto out;
		}

		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc)
			goto search_again;
		err = split_state(tree, state, prealloc, end + 1);
		if (err)
			extent_io_tree_panic(tree, err);

		set_state_bits(tree, prealloc, bits, changeset);
		cache_state(prealloc, cached_state);
		merge_state(tree, prealloc);
		prealloc = NULL;
		goto out;
	}

search_again:
	if (start > end)
		goto out;
	spin_unlock(&tree->lock);
	if (gfpflags_allow_blocking(mask))
		cond_resched();
	goto again;

out:
	spin_unlock(&tree->lock);
	if (prealloc)
		free_extent_state(prealloc);

	return err;

}

int set_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
		   u32 bits, struct extent_state **cached_state)
{
	return __set_extent_bit(tree, start, end, bits, NULL, NULL,
				cached_state, NULL);
}

 
int convert_extent_bit(struct extent_io_tree *tree, u64 start, u64 end,
		       u32 bits, u32 clear_bits,
		       struct extent_state **cached_state)
{
	struct extent_state *state;
	struct extent_state *prealloc = NULL;
	struct rb_node **p = NULL;
	struct rb_node *parent = NULL;
	int err = 0;
	u64 last_start;
	u64 last_end;
	bool first_iteration = true;

	btrfs_debug_check_extent_io_range(tree, start, end);
	trace_btrfs_convert_extent_bit(tree, start, end - start + 1, bits,
				       clear_bits);

again:
	if (!prealloc) {
		 
		prealloc = alloc_extent_state(GFP_NOFS);
		if (!prealloc && !first_iteration)
			return -ENOMEM;
	}

	spin_lock(&tree->lock);
	if (cached_state && *cached_state) {
		state = *cached_state;
		if (state->start <= start && state->end > start &&
		    extent_state_in_tree(state))
			goto hit_next;
	}

	 
	state = tree_search_for_insert(tree, start, &p, &parent);
	if (!state) {
		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc) {
			err = -ENOMEM;
			goto out;
		}
		prealloc->start = start;
		prealloc->end = end;
		insert_state_fast(tree, prealloc, p, parent, bits, NULL);
		cache_state(prealloc, cached_state);
		prealloc = NULL;
		goto out;
	}
hit_next:
	last_start = state->start;
	last_end = state->end;

	 
	if (state->start == start && state->end <= end) {
		set_state_bits(tree, state, bits, NULL);
		cache_state(state, cached_state);
		state = clear_state_bit(tree, state, clear_bits, 0, NULL);
		if (last_end == (u64)-1)
			goto out;
		start = last_end + 1;
		if (start < end && state && state->start == start &&
		    !need_resched())
			goto hit_next;
		goto search_again;
	}

	 
	if (state->start < start) {
		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc) {
			err = -ENOMEM;
			goto out;
		}
		err = split_state(tree, state, prealloc, start);
		if (err)
			extent_io_tree_panic(tree, err);
		prealloc = NULL;
		if (err)
			goto out;
		if (state->end <= end) {
			set_state_bits(tree, state, bits, NULL);
			cache_state(state, cached_state);
			state = clear_state_bit(tree, state, clear_bits, 0, NULL);
			if (last_end == (u64)-1)
				goto out;
			start = last_end + 1;
			if (start < end && state && state->start == start &&
			    !need_resched())
				goto hit_next;
		}
		goto search_again;
	}
	 
	if (state->start > start) {
		u64 this_end;
		if (end < last_start)
			this_end = end;
		else
			this_end = last_start - 1;

		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc) {
			err = -ENOMEM;
			goto out;
		}

		 
		prealloc->start = start;
		prealloc->end = this_end;
		err = insert_state(tree, prealloc, bits, NULL);
		if (err)
			extent_io_tree_panic(tree, err);
		cache_state(prealloc, cached_state);
		prealloc = NULL;
		start = this_end + 1;
		goto search_again;
	}
	 
	if (state->start <= end && state->end > end) {
		prealloc = alloc_extent_state_atomic(prealloc);
		if (!prealloc) {
			err = -ENOMEM;
			goto out;
		}

		err = split_state(tree, state, prealloc, end + 1);
		if (err)
			extent_io_tree_panic(tree, err);

		set_state_bits(tree, prealloc, bits, NULL);
		cache_state(prealloc, cached_state);
		clear_state_bit(tree, prealloc, clear_bits, 0, NULL);
		prealloc = NULL;
		goto out;
	}

search_again:
	if (start > end)
		goto out;
	spin_unlock(&tree->lock);
	cond_resched();
	first_iteration = false;
	goto again;

out:
	spin_unlock(&tree->lock);
	if (prealloc)
		free_extent_state(prealloc);

	return err;
}

 
void find_first_clear_extent_bit(struct extent_io_tree *tree, u64 start,
				 u64 *start_ret, u64 *end_ret, u32 bits)
{
	struct extent_state *state;
	struct extent_state *prev = NULL, *next = NULL;

	spin_lock(&tree->lock);

	 
	while (1) {
		state = tree_search_prev_next(tree, start, &prev, &next);
		if (!state && !next && !prev) {
			 
			*start_ret = 0;
			*end_ret = -1;
			goto out;
		} else if (!state && !next) {
			 
			*start_ret = prev->end + 1;
			*end_ret = -1;
			goto out;
		} else if (!state) {
			state = next;
		}

		 
		if (in_range(start, state->start, state->end - state->start + 1)) {
			if (state->state & bits) {
				 
				start = state->end + 1;
			} else {
				 
				*start_ret = state->start;
				break;
			}
		} else {
			 
			if (prev)
				*start_ret = prev->end + 1;
			else
				*start_ret = 0;
			break;
		}
	}

	 
	while (state) {
		if (state->end >= start && !(state->state & bits)) {
			*end_ret = state->end;
		} else {
			*end_ret = state->start - 1;
			break;
		}
		state = next_state(state);
	}
out:
	spin_unlock(&tree->lock);
}

 
u64 count_range_bits(struct extent_io_tree *tree,
		     u64 *start, u64 search_end, u64 max_bytes,
		     u32 bits, int contig,
		     struct extent_state **cached_state)
{
	struct extent_state *state = NULL;
	struct extent_state *cached;
	u64 cur_start = *start;
	u64 total_bytes = 0;
	u64 last = 0;
	int found = 0;

	if (WARN_ON(search_end < cur_start))
		return 0;

	spin_lock(&tree->lock);

	if (!cached_state || !*cached_state)
		goto search;

	cached = *cached_state;

	if (!extent_state_in_tree(cached))
		goto search;

	if (cached->start <= cur_start && cur_start <= cached->end) {
		state = cached;
	} else if (cached->start > cur_start) {
		struct extent_state *prev;

		 
		prev = prev_state(cached);
		if (!prev)
			state = cached;
		else if (prev->start <= cur_start && cur_start <= prev->end)
			state = prev;
	}

	 
search:
	if (!state)
		state = tree_search(tree, cur_start);

	while (state) {
		if (state->start > search_end)
			break;
		if (contig && found && state->start > last + 1)
			break;
		if (state->end >= cur_start && (state->state & bits) == bits) {
			total_bytes += min(search_end, state->end) + 1 -
				       max(cur_start, state->start);
			if (total_bytes >= max_bytes)
				break;
			if (!found) {
				*start = max(cur_start, state->start);
				found = 1;
			}
			last = state->end;
		} else if (contig && found) {
			break;
		}
		state = next_state(state);
	}

	if (cached_state) {
		free_extent_state(*cached_state);
		*cached_state = state;
		if (state)
			refcount_inc(&state->refs);
	}

	spin_unlock(&tree->lock);

	return total_bytes;
}

 
int test_range_bit(struct extent_io_tree *tree, u64 start, u64 end,
		   u32 bits, int filled, struct extent_state *cached)
{
	struct extent_state *state = NULL;
	int bitset = 0;

	spin_lock(&tree->lock);
	if (cached && extent_state_in_tree(cached) && cached->start <= start &&
	    cached->end > start)
		state = cached;
	else
		state = tree_search(tree, start);
	while (state && start <= end) {
		if (filled && state->start > start) {
			bitset = 0;
			break;
		}

		if (state->start > end)
			break;

		if (state->state & bits) {
			bitset = 1;
			if (!filled)
				break;
		} else if (filled) {
			bitset = 0;
			break;
		}

		if (state->end == (u64)-1)
			break;

		start = state->end + 1;
		if (start > end)
			break;
		state = next_state(state);
	}

	 
	if (filled && !state)
		bitset = 0;
	spin_unlock(&tree->lock);
	return bitset;
}

 
int set_record_extent_bits(struct extent_io_tree *tree, u64 start, u64 end,
			   u32 bits, struct extent_changeset *changeset)
{
	 
	ASSERT(!(bits & EXTENT_LOCKED));

	return __set_extent_bit(tree, start, end, bits, NULL, NULL, NULL, changeset);
}

int clear_record_extent_bits(struct extent_io_tree *tree, u64 start, u64 end,
			     u32 bits, struct extent_changeset *changeset)
{
	 
	ASSERT(!(bits & EXTENT_LOCKED));

	return __clear_extent_bit(tree, start, end, bits, NULL, changeset);
}

int try_lock_extent(struct extent_io_tree *tree, u64 start, u64 end,
		    struct extent_state **cached)
{
	int err;
	u64 failed_start;

	err = __set_extent_bit(tree, start, end, EXTENT_LOCKED, &failed_start,
			       NULL, cached, NULL);
	if (err == -EEXIST) {
		if (failed_start > start)
			clear_extent_bit(tree, start, failed_start - 1,
					 EXTENT_LOCKED, cached);
		return 0;
	}
	return 1;
}

 
int lock_extent(struct extent_io_tree *tree, u64 start, u64 end,
		struct extent_state **cached_state)
{
	struct extent_state *failed_state = NULL;
	int err;
	u64 failed_start;

	err = __set_extent_bit(tree, start, end, EXTENT_LOCKED, &failed_start,
			       &failed_state, cached_state, NULL);
	while (err == -EEXIST) {
		if (failed_start != start)
			clear_extent_bit(tree, start, failed_start - 1,
					 EXTENT_LOCKED, cached_state);

		wait_extent_bit(tree, failed_start, end, EXTENT_LOCKED,
				&failed_state);
		err = __set_extent_bit(tree, start, end, EXTENT_LOCKED,
				       &failed_start, &failed_state,
				       cached_state, NULL);
	}
	return err;
}

void __cold extent_state_free_cachep(void)
{
	btrfs_extent_state_leak_debug_check();
	kmem_cache_destroy(extent_state_cache);
}

int __init extent_state_init_cachep(void)
{
	extent_state_cache = kmem_cache_create("btrfs_extent_state",
			sizeof(struct extent_state), 0,
			SLAB_MEM_SPREAD, NULL);
	if (!extent_state_cache)
		return -ENOMEM;

	return 0;
}

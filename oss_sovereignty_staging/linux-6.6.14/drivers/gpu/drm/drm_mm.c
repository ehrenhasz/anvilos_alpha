 

 

#include <linux/export.h>
#include <linux/interval_tree_generic.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>

#include <drm/drm_mm.h>

 

#ifdef CONFIG_DRM_DEBUG_MM
#include <linux/stackdepot.h>

#define STACKDEPTH 32
#define BUFSZ 4096

static noinline void save_stack(struct drm_mm_node *node)
{
	unsigned long entries[STACKDEPTH];
	unsigned int n;

	n = stack_trace_save(entries, ARRAY_SIZE(entries), 1);

	 
	node->stack = stack_depot_save(entries, n, GFP_NOWAIT);
}

static void show_leaks(struct drm_mm *mm)
{
	struct drm_mm_node *node;
	char *buf;

	buf = kmalloc(BUFSZ, GFP_KERNEL);
	if (!buf)
		return;

	list_for_each_entry(node, drm_mm_nodes(mm), node_list) {
		if (!node->stack) {
			DRM_ERROR("node [%08llx + %08llx]: unknown owner\n",
				  node->start, node->size);
			continue;
		}

		stack_depot_snprint(node->stack, buf, BUFSZ, 0);
		DRM_ERROR("node [%08llx + %08llx]: inserted at\n%s",
			  node->start, node->size, buf);
	}

	kfree(buf);
}

#undef STACKDEPTH
#undef BUFSZ
#else
static void save_stack(struct drm_mm_node *node) { }
static void show_leaks(struct drm_mm *mm) { }
#endif

#define START(node) ((node)->start)
#define LAST(node)  ((node)->start + (node)->size - 1)

INTERVAL_TREE_DEFINE(struct drm_mm_node, rb,
		     u64, __subtree_last,
		     START, LAST, static inline, drm_mm_interval_tree)

struct drm_mm_node *
__drm_mm_interval_first(const struct drm_mm *mm, u64 start, u64 last)
{
	return drm_mm_interval_tree_iter_first((struct rb_root_cached *)&mm->interval_tree,
					       start, last) ?: (struct drm_mm_node *)&mm->head_node;
}
EXPORT_SYMBOL(__drm_mm_interval_first);

static void drm_mm_interval_tree_add_node(struct drm_mm_node *hole_node,
					  struct drm_mm_node *node)
{
	struct drm_mm *mm = hole_node->mm;
	struct rb_node **link, *rb;
	struct drm_mm_node *parent;
	bool leftmost;

	node->__subtree_last = LAST(node);

	if (drm_mm_node_allocated(hole_node)) {
		rb = &hole_node->rb;
		while (rb) {
			parent = rb_entry(rb, struct drm_mm_node, rb);
			if (parent->__subtree_last >= node->__subtree_last)
				break;

			parent->__subtree_last = node->__subtree_last;
			rb = rb_parent(rb);
		}

		rb = &hole_node->rb;
		link = &hole_node->rb.rb_right;
		leftmost = false;
	} else {
		rb = NULL;
		link = &mm->interval_tree.rb_root.rb_node;
		leftmost = true;
	}

	while (*link) {
		rb = *link;
		parent = rb_entry(rb, struct drm_mm_node, rb);
		if (parent->__subtree_last < node->__subtree_last)
			parent->__subtree_last = node->__subtree_last;
		if (node->start < parent->start) {
			link = &parent->rb.rb_left;
		} else {
			link = &parent->rb.rb_right;
			leftmost = false;
		}
	}

	rb_link_node(&node->rb, rb, link);
	rb_insert_augmented_cached(&node->rb, &mm->interval_tree, leftmost,
				   &drm_mm_interval_tree_augment);
}

#define HOLE_SIZE(NODE) ((NODE)->hole_size)
#define HOLE_ADDR(NODE) (__drm_mm_hole_node_start(NODE))

static u64 rb_to_hole_size(struct rb_node *rb)
{
	return rb_entry(rb, struct drm_mm_node, rb_hole_size)->hole_size;
}

static void insert_hole_size(struct rb_root_cached *root,
			     struct drm_mm_node *node)
{
	struct rb_node **link = &root->rb_root.rb_node, *rb = NULL;
	u64 x = node->hole_size;
	bool first = true;

	while (*link) {
		rb = *link;
		if (x > rb_to_hole_size(rb)) {
			link = &rb->rb_left;
		} else {
			link = &rb->rb_right;
			first = false;
		}
	}

	rb_link_node(&node->rb_hole_size, rb, link);
	rb_insert_color_cached(&node->rb_hole_size, root, first);
}

RB_DECLARE_CALLBACKS_MAX(static, augment_callbacks,
			 struct drm_mm_node, rb_hole_addr,
			 u64, subtree_max_hole, HOLE_SIZE)

static void insert_hole_addr(struct rb_root *root, struct drm_mm_node *node)
{
	struct rb_node **link = &root->rb_node, *rb_parent = NULL;
	u64 start = HOLE_ADDR(node), subtree_max_hole = node->subtree_max_hole;
	struct drm_mm_node *parent;

	while (*link) {
		rb_parent = *link;
		parent = rb_entry(rb_parent, struct drm_mm_node, rb_hole_addr);
		if (parent->subtree_max_hole < subtree_max_hole)
			parent->subtree_max_hole = subtree_max_hole;
		if (start < HOLE_ADDR(parent))
			link = &parent->rb_hole_addr.rb_left;
		else
			link = &parent->rb_hole_addr.rb_right;
	}

	rb_link_node(&node->rb_hole_addr, rb_parent, link);
	rb_insert_augmented(&node->rb_hole_addr, root, &augment_callbacks);
}

static void add_hole(struct drm_mm_node *node)
{
	struct drm_mm *mm = node->mm;

	node->hole_size =
		__drm_mm_hole_node_end(node) - __drm_mm_hole_node_start(node);
	node->subtree_max_hole = node->hole_size;
	DRM_MM_BUG_ON(!drm_mm_hole_follows(node));

	insert_hole_size(&mm->holes_size, node);
	insert_hole_addr(&mm->holes_addr, node);

	list_add(&node->hole_stack, &mm->hole_stack);
}

static void rm_hole(struct drm_mm_node *node)
{
	DRM_MM_BUG_ON(!drm_mm_hole_follows(node));

	list_del(&node->hole_stack);
	rb_erase_cached(&node->rb_hole_size, &node->mm->holes_size);
	rb_erase_augmented(&node->rb_hole_addr, &node->mm->holes_addr,
			   &augment_callbacks);
	node->hole_size = 0;
	node->subtree_max_hole = 0;

	DRM_MM_BUG_ON(drm_mm_hole_follows(node));
}

static inline struct drm_mm_node *rb_hole_size_to_node(struct rb_node *rb)
{
	return rb_entry_safe(rb, struct drm_mm_node, rb_hole_size);
}

static inline struct drm_mm_node *rb_hole_addr_to_node(struct rb_node *rb)
{
	return rb_entry_safe(rb, struct drm_mm_node, rb_hole_addr);
}

static struct drm_mm_node *best_hole(struct drm_mm *mm, u64 size)
{
	struct rb_node *rb = mm->holes_size.rb_root.rb_node;
	struct drm_mm_node *best = NULL;

	do {
		struct drm_mm_node *node =
			rb_entry(rb, struct drm_mm_node, rb_hole_size);

		if (size <= node->hole_size) {
			best = node;
			rb = rb->rb_right;
		} else {
			rb = rb->rb_left;
		}
	} while (rb);

	return best;
}

static bool usable_hole_addr(struct rb_node *rb, u64 size)
{
	return rb && rb_hole_addr_to_node(rb)->subtree_max_hole >= size;
}

static struct drm_mm_node *find_hole_addr(struct drm_mm *mm, u64 addr, u64 size)
{
	struct rb_node *rb = mm->holes_addr.rb_node;
	struct drm_mm_node *node = NULL;

	while (rb) {
		u64 hole_start;

		if (!usable_hole_addr(rb, size))
			break;

		node = rb_hole_addr_to_node(rb);
		hole_start = __drm_mm_hole_node_start(node);

		if (addr < hole_start)
			rb = node->rb_hole_addr.rb_left;
		else if (addr > hole_start + node->hole_size)
			rb = node->rb_hole_addr.rb_right;
		else
			break;
	}

	return node;
}

static struct drm_mm_node *
first_hole(struct drm_mm *mm,
	   u64 start, u64 end, u64 size,
	   enum drm_mm_insert_mode mode)
{
	switch (mode) {
	default:
	case DRM_MM_INSERT_BEST:
		return best_hole(mm, size);

	case DRM_MM_INSERT_LOW:
		return find_hole_addr(mm, start, size);

	case DRM_MM_INSERT_HIGH:
		return find_hole_addr(mm, end, size);

	case DRM_MM_INSERT_EVICT:
		return list_first_entry_or_null(&mm->hole_stack,
						struct drm_mm_node,
						hole_stack);
	}
}

 

#define DECLARE_NEXT_HOLE_ADDR(name, first, last)			\
static struct drm_mm_node *name(struct drm_mm_node *entry, u64 size)	\
{									\
	struct rb_node *parent, *node = &entry->rb_hole_addr;		\
									\
	if (!entry || RB_EMPTY_NODE(node))				\
		return NULL;						\
									\
	if (usable_hole_addr(node->first, size)) {			\
		node = node->first;					\
		while (usable_hole_addr(node->last, size))		\
			node = node->last;				\
		return rb_hole_addr_to_node(node);			\
	}								\
									\
	while ((parent = rb_parent(node)) && node == parent->first)	\
		node = parent;						\
									\
	return rb_hole_addr_to_node(parent);				\
}

DECLARE_NEXT_HOLE_ADDR(next_hole_high_addr, rb_left, rb_right)
DECLARE_NEXT_HOLE_ADDR(next_hole_low_addr, rb_right, rb_left)

static struct drm_mm_node *
next_hole(struct drm_mm *mm,
	  struct drm_mm_node *node,
	  u64 size,
	  enum drm_mm_insert_mode mode)
{
	switch (mode) {
	default:
	case DRM_MM_INSERT_BEST:
		return rb_hole_size_to_node(rb_prev(&node->rb_hole_size));

	case DRM_MM_INSERT_LOW:
		return next_hole_low_addr(node, size);

	case DRM_MM_INSERT_HIGH:
		return next_hole_high_addr(node, size);

	case DRM_MM_INSERT_EVICT:
		node = list_next_entry(node, hole_stack);
		return &node->hole_stack == &mm->hole_stack ? NULL : node;
	}
}

 
int drm_mm_reserve_node(struct drm_mm *mm, struct drm_mm_node *node)
{
	struct drm_mm_node *hole;
	u64 hole_start, hole_end;
	u64 adj_start, adj_end;
	u64 end;

	end = node->start + node->size;
	if (unlikely(end <= node->start))
		return -ENOSPC;

	 
	hole = find_hole_addr(mm, node->start, 0);
	if (!hole)
		return -ENOSPC;

	adj_start = hole_start = __drm_mm_hole_node_start(hole);
	adj_end = hole_end = hole_start + hole->hole_size;

	if (mm->color_adjust)
		mm->color_adjust(hole, node->color, &adj_start, &adj_end);

	if (adj_start > node->start || adj_end < end)
		return -ENOSPC;

	node->mm = mm;

	__set_bit(DRM_MM_NODE_ALLOCATED_BIT, &node->flags);
	list_add(&node->node_list, &hole->node_list);
	drm_mm_interval_tree_add_node(hole, node);
	node->hole_size = 0;

	rm_hole(hole);
	if (node->start > hole_start)
		add_hole(hole);
	if (end < hole_end)
		add_hole(node);

	save_stack(node);
	return 0;
}
EXPORT_SYMBOL(drm_mm_reserve_node);

static u64 rb_to_hole_size_or_zero(struct rb_node *rb)
{
	return rb ? rb_to_hole_size(rb) : 0;
}

 
int drm_mm_insert_node_in_range(struct drm_mm * const mm,
				struct drm_mm_node * const node,
				u64 size, u64 alignment,
				unsigned long color,
				u64 range_start, u64 range_end,
				enum drm_mm_insert_mode mode)
{
	struct drm_mm_node *hole;
	u64 remainder_mask;
	bool once;

	DRM_MM_BUG_ON(range_start > range_end);

	if (unlikely(size == 0 || range_end - range_start < size))
		return -ENOSPC;

	if (rb_to_hole_size_or_zero(rb_first_cached(&mm->holes_size)) < size)
		return -ENOSPC;

	if (alignment <= 1)
		alignment = 0;

	once = mode & DRM_MM_INSERT_ONCE;
	mode &= ~DRM_MM_INSERT_ONCE;

	remainder_mask = is_power_of_2(alignment) ? alignment - 1 : 0;
	for (hole = first_hole(mm, range_start, range_end, size, mode);
	     hole;
	     hole = once ? NULL : next_hole(mm, hole, size, mode)) {
		u64 hole_start = __drm_mm_hole_node_start(hole);
		u64 hole_end = hole_start + hole->hole_size;
		u64 adj_start, adj_end;
		u64 col_start, col_end;

		if (mode == DRM_MM_INSERT_LOW && hole_start >= range_end)
			break;

		if (mode == DRM_MM_INSERT_HIGH && hole_end <= range_start)
			break;

		col_start = hole_start;
		col_end = hole_end;
		if (mm->color_adjust)
			mm->color_adjust(hole, color, &col_start, &col_end);

		adj_start = max(col_start, range_start);
		adj_end = min(col_end, range_end);

		if (adj_end <= adj_start || adj_end - adj_start < size)
			continue;

		if (mode == DRM_MM_INSERT_HIGH)
			adj_start = adj_end - size;

		if (alignment) {
			u64 rem;

			if (likely(remainder_mask))
				rem = adj_start & remainder_mask;
			else
				div64_u64_rem(adj_start, alignment, &rem);
			if (rem) {
				adj_start -= rem;
				if (mode != DRM_MM_INSERT_HIGH)
					adj_start += alignment;

				if (adj_start < max(col_start, range_start) ||
				    min(col_end, range_end) - adj_start < size)
					continue;

				if (adj_end <= adj_start ||
				    adj_end - adj_start < size)
					continue;
			}
		}

		node->mm = mm;
		node->size = size;
		node->start = adj_start;
		node->color = color;
		node->hole_size = 0;

		__set_bit(DRM_MM_NODE_ALLOCATED_BIT, &node->flags);
		list_add(&node->node_list, &hole->node_list);
		drm_mm_interval_tree_add_node(hole, node);

		rm_hole(hole);
		if (adj_start > hole_start)
			add_hole(hole);
		if (adj_start + size < hole_end)
			add_hole(node);

		save_stack(node);
		return 0;
	}

	return -ENOSPC;
}
EXPORT_SYMBOL(drm_mm_insert_node_in_range);

static inline bool drm_mm_node_scanned_block(const struct drm_mm_node *node)
{
	return test_bit(DRM_MM_NODE_SCANNED_BIT, &node->flags);
}

 
void drm_mm_remove_node(struct drm_mm_node *node)
{
	struct drm_mm *mm = node->mm;
	struct drm_mm_node *prev_node;

	DRM_MM_BUG_ON(!drm_mm_node_allocated(node));
	DRM_MM_BUG_ON(drm_mm_node_scanned_block(node));

	prev_node = list_prev_entry(node, node_list);

	if (drm_mm_hole_follows(node))
		rm_hole(node);

	drm_mm_interval_tree_remove(node, &mm->interval_tree);
	list_del(&node->node_list);

	if (drm_mm_hole_follows(prev_node))
		rm_hole(prev_node);
	add_hole(prev_node);

	clear_bit_unlock(DRM_MM_NODE_ALLOCATED_BIT, &node->flags);
}
EXPORT_SYMBOL(drm_mm_remove_node);

 
void drm_mm_replace_node(struct drm_mm_node *old, struct drm_mm_node *new)
{
	struct drm_mm *mm = old->mm;

	DRM_MM_BUG_ON(!drm_mm_node_allocated(old));

	*new = *old;

	__set_bit(DRM_MM_NODE_ALLOCATED_BIT, &new->flags);
	list_replace(&old->node_list, &new->node_list);
	rb_replace_node_cached(&old->rb, &new->rb, &mm->interval_tree);

	if (drm_mm_hole_follows(old)) {
		list_replace(&old->hole_stack, &new->hole_stack);
		rb_replace_node_cached(&old->rb_hole_size,
				       &new->rb_hole_size,
				       &mm->holes_size);
		rb_replace_node(&old->rb_hole_addr,
				&new->rb_hole_addr,
				&mm->holes_addr);
	}

	clear_bit_unlock(DRM_MM_NODE_ALLOCATED_BIT, &old->flags);
}
EXPORT_SYMBOL(drm_mm_replace_node);

 

 
void drm_mm_scan_init_with_range(struct drm_mm_scan *scan,
				 struct drm_mm *mm,
				 u64 size,
				 u64 alignment,
				 unsigned long color,
				 u64 start,
				 u64 end,
				 enum drm_mm_insert_mode mode)
{
	DRM_MM_BUG_ON(start >= end);
	DRM_MM_BUG_ON(!size || size > end - start);
	DRM_MM_BUG_ON(mm->scan_active);

	scan->mm = mm;

	if (alignment <= 1)
		alignment = 0;

	scan->color = color;
	scan->alignment = alignment;
	scan->remainder_mask = is_power_of_2(alignment) ? alignment - 1 : 0;
	scan->size = size;
	scan->mode = mode;

	DRM_MM_BUG_ON(end <= start);
	scan->range_start = start;
	scan->range_end = end;

	scan->hit_start = U64_MAX;
	scan->hit_end = 0;
}
EXPORT_SYMBOL(drm_mm_scan_init_with_range);

 
bool drm_mm_scan_add_block(struct drm_mm_scan *scan,
			   struct drm_mm_node *node)
{
	struct drm_mm *mm = scan->mm;
	struct drm_mm_node *hole;
	u64 hole_start, hole_end;
	u64 col_start, col_end;
	u64 adj_start, adj_end;

	DRM_MM_BUG_ON(node->mm != mm);
	DRM_MM_BUG_ON(!drm_mm_node_allocated(node));
	DRM_MM_BUG_ON(drm_mm_node_scanned_block(node));
	__set_bit(DRM_MM_NODE_SCANNED_BIT, &node->flags);
	mm->scan_active++;

	 
	hole = list_prev_entry(node, node_list);
	DRM_MM_BUG_ON(list_next_entry(hole, node_list) != node);
	__list_del_entry(&node->node_list);

	hole_start = __drm_mm_hole_node_start(hole);
	hole_end = __drm_mm_hole_node_end(hole);

	col_start = hole_start;
	col_end = hole_end;
	if (mm->color_adjust)
		mm->color_adjust(hole, scan->color, &col_start, &col_end);

	adj_start = max(col_start, scan->range_start);
	adj_end = min(col_end, scan->range_end);
	if (adj_end <= adj_start || adj_end - adj_start < scan->size)
		return false;

	if (scan->mode == DRM_MM_INSERT_HIGH)
		adj_start = adj_end - scan->size;

	if (scan->alignment) {
		u64 rem;

		if (likely(scan->remainder_mask))
			rem = adj_start & scan->remainder_mask;
		else
			div64_u64_rem(adj_start, scan->alignment, &rem);
		if (rem) {
			adj_start -= rem;
			if (scan->mode != DRM_MM_INSERT_HIGH)
				adj_start += scan->alignment;
			if (adj_start < max(col_start, scan->range_start) ||
			    min(col_end, scan->range_end) - adj_start < scan->size)
				return false;

			if (adj_end <= adj_start ||
			    adj_end - adj_start < scan->size)
				return false;
		}
	}

	scan->hit_start = adj_start;
	scan->hit_end = adj_start + scan->size;

	DRM_MM_BUG_ON(scan->hit_start >= scan->hit_end);
	DRM_MM_BUG_ON(scan->hit_start < hole_start);
	DRM_MM_BUG_ON(scan->hit_end > hole_end);

	return true;
}
EXPORT_SYMBOL(drm_mm_scan_add_block);

 
bool drm_mm_scan_remove_block(struct drm_mm_scan *scan,
			      struct drm_mm_node *node)
{
	struct drm_mm_node *prev_node;

	DRM_MM_BUG_ON(node->mm != scan->mm);
	DRM_MM_BUG_ON(!drm_mm_node_scanned_block(node));
	__clear_bit(DRM_MM_NODE_SCANNED_BIT, &node->flags);

	DRM_MM_BUG_ON(!node->mm->scan_active);
	node->mm->scan_active--;

	 
	prev_node = list_prev_entry(node, node_list);
	DRM_MM_BUG_ON(list_next_entry(prev_node, node_list) !=
		      list_next_entry(node, node_list));
	list_add(&node->node_list, &prev_node->node_list);

	return (node->start + node->size > scan->hit_start &&
		node->start < scan->hit_end);
}
EXPORT_SYMBOL(drm_mm_scan_remove_block);

 
struct drm_mm_node *drm_mm_scan_color_evict(struct drm_mm_scan *scan)
{
	struct drm_mm *mm = scan->mm;
	struct drm_mm_node *hole;
	u64 hole_start, hole_end;

	DRM_MM_BUG_ON(list_empty(&mm->hole_stack));

	if (!mm->color_adjust)
		return NULL;

	 
	list_for_each_entry(hole, &mm->hole_stack, hole_stack) {
		hole_start = __drm_mm_hole_node_start(hole);
		hole_end = hole_start + hole->hole_size;

		if (hole_start <= scan->hit_start &&
		    hole_end >= scan->hit_end)
			break;
	}

	 
	DRM_MM_BUG_ON(&hole->hole_stack == &mm->hole_stack);
	if (unlikely(&hole->hole_stack == &mm->hole_stack))
		return NULL;

	DRM_MM_BUG_ON(hole_start > scan->hit_start);
	DRM_MM_BUG_ON(hole_end < scan->hit_end);

	mm->color_adjust(hole, scan->color, &hole_start, &hole_end);
	if (hole_start > scan->hit_start)
		return hole;
	if (hole_end < scan->hit_end)
		return list_next_entry(hole, node_list);

	return NULL;
}
EXPORT_SYMBOL(drm_mm_scan_color_evict);

 
void drm_mm_init(struct drm_mm *mm, u64 start, u64 size)
{
	DRM_MM_BUG_ON(start + size <= start);

	mm->color_adjust = NULL;

	INIT_LIST_HEAD(&mm->hole_stack);
	mm->interval_tree = RB_ROOT_CACHED;
	mm->holes_size = RB_ROOT_CACHED;
	mm->holes_addr = RB_ROOT;

	 
	INIT_LIST_HEAD(&mm->head_node.node_list);
	mm->head_node.flags = 0;
	mm->head_node.mm = mm;
	mm->head_node.start = start + size;
	mm->head_node.size = -size;
	add_hole(&mm->head_node);

	mm->scan_active = 0;

#ifdef CONFIG_DRM_DEBUG_MM
	stack_depot_init();
#endif
}
EXPORT_SYMBOL(drm_mm_init);

 
void drm_mm_takedown(struct drm_mm *mm)
{
	if (WARN(!drm_mm_clean(mm),
		 "Memory manager not clean during takedown.\n"))
		show_leaks(mm);
}
EXPORT_SYMBOL(drm_mm_takedown);

static u64 drm_mm_dump_hole(struct drm_printer *p, const struct drm_mm_node *entry)
{
	u64 start, size;

	size = entry->hole_size;
	if (size) {
		start = drm_mm_hole_node_start(entry);
		drm_printf(p, "%#018llx-%#018llx: %llu: free\n",
			   start, start + size, size);
	}

	return size;
}
 
void drm_mm_print(const struct drm_mm *mm, struct drm_printer *p)
{
	const struct drm_mm_node *entry;
	u64 total_used = 0, total_free = 0, total = 0;

	total_free += drm_mm_dump_hole(p, &mm->head_node);

	drm_mm_for_each_node(entry, mm) {
		drm_printf(p, "%#018llx-%#018llx: %llu: used\n", entry->start,
			   entry->start + entry->size, entry->size);
		total_used += entry->size;
		total_free += drm_mm_dump_hole(p, entry);
	}
	total = total_free + total_used;

	drm_printf(p, "total: %llu, used %llu free %llu\n", total,
		   total_used, total_free);
}
EXPORT_SYMBOL(drm_mm_print);

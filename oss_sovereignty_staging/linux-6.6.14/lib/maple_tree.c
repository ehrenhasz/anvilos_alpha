
 

 


#include <linux/maple_tree.h>
#include <linux/xarray.h>
#include <linux/types.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/limits.h>
#include <asm/barrier.h>

#define CREATE_TRACE_POINTS
#include <trace/events/maple_tree.h>

#define MA_ROOT_PARENT 1

 
#define MA_STATE_BULK		1
#define MA_STATE_REBALANCE	2
#define MA_STATE_PREALLOC	4

#define ma_parent_ptr(x) ((struct maple_pnode *)(x))
#define mas_tree_parent(x) ((unsigned long)(x->tree) | MA_ROOT_PARENT)
#define ma_mnode_ptr(x) ((struct maple_node *)(x))
#define ma_enode_ptr(x) ((struct maple_enode *)(x))
static struct kmem_cache *maple_node_cache;

#ifdef CONFIG_DEBUG_MAPLE_TREE
static const unsigned long mt_max[] = {
	[maple_dense]		= MAPLE_NODE_SLOTS,
	[maple_leaf_64]		= ULONG_MAX,
	[maple_range_64]	= ULONG_MAX,
	[maple_arange_64]	= ULONG_MAX,
};
#define mt_node_max(x) mt_max[mte_node_type(x)]
#endif

static const unsigned char mt_slots[] = {
	[maple_dense]		= MAPLE_NODE_SLOTS,
	[maple_leaf_64]		= MAPLE_RANGE64_SLOTS,
	[maple_range_64]	= MAPLE_RANGE64_SLOTS,
	[maple_arange_64]	= MAPLE_ARANGE64_SLOTS,
};
#define mt_slot_count(x) mt_slots[mte_node_type(x)]

static const unsigned char mt_pivots[] = {
	[maple_dense]		= 0,
	[maple_leaf_64]		= MAPLE_RANGE64_SLOTS - 1,
	[maple_range_64]	= MAPLE_RANGE64_SLOTS - 1,
	[maple_arange_64]	= MAPLE_ARANGE64_SLOTS - 1,
};
#define mt_pivot_count(x) mt_pivots[mte_node_type(x)]

static const unsigned char mt_min_slots[] = {
	[maple_dense]		= MAPLE_NODE_SLOTS / 2,
	[maple_leaf_64]		= (MAPLE_RANGE64_SLOTS / 2) - 2,
	[maple_range_64]	= (MAPLE_RANGE64_SLOTS / 2) - 2,
	[maple_arange_64]	= (MAPLE_ARANGE64_SLOTS / 2) - 1,
};
#define mt_min_slot_count(x) mt_min_slots[mte_node_type(x)]

#define MAPLE_BIG_NODE_SLOTS	(MAPLE_RANGE64_SLOTS * 2 + 2)
#define MAPLE_BIG_NODE_GAPS	(MAPLE_ARANGE64_SLOTS * 2 + 1)

struct maple_big_node {
	struct maple_pnode *parent;
	unsigned long pivot[MAPLE_BIG_NODE_SLOTS - 1];
	union {
		struct maple_enode *slot[MAPLE_BIG_NODE_SLOTS];
		struct {
			unsigned long padding[MAPLE_BIG_NODE_GAPS];
			unsigned long gap[MAPLE_BIG_NODE_GAPS];
		};
	};
	unsigned char b_end;
	enum maple_type type;
};

 
struct maple_subtree_state {
	struct ma_state *orig_l;	 
	struct ma_state *orig_r;	 
	struct ma_state *l;		 
	struct ma_state *m;		 
	struct ma_state *r;		 
	struct ma_topiary *free;	 
	struct ma_topiary *destroy;	 
	struct maple_big_node *bn;
};

#ifdef CONFIG_KASAN_STACK
 
#define noinline_for_kasan noinline_for_stack
#else
#define noinline_for_kasan inline
#endif

 
static inline struct maple_node *mt_alloc_one(gfp_t gfp)
{
	return kmem_cache_alloc(maple_node_cache, gfp);
}

static inline int mt_alloc_bulk(gfp_t gfp, size_t size, void **nodes)
{
	return kmem_cache_alloc_bulk(maple_node_cache, gfp, size, nodes);
}

static inline void mt_free_bulk(size_t size, void __rcu **nodes)
{
	kmem_cache_free_bulk(maple_node_cache, size, (void **)nodes);
}

static void mt_free_rcu(struct rcu_head *head)
{
	struct maple_node *node = container_of(head, struct maple_node, rcu);

	kmem_cache_free(maple_node_cache, node);
}

 
static void ma_free_rcu(struct maple_node *node)
{
	WARN_ON(node->parent != ma_parent_ptr(node));
	call_rcu(&node->rcu, mt_free_rcu);
}

static void mas_set_height(struct ma_state *mas)
{
	unsigned int new_flags = mas->tree->ma_flags;

	new_flags &= ~MT_FLAGS_HEIGHT_MASK;
	MAS_BUG_ON(mas, mas->depth > MAPLE_HEIGHT_MAX);
	new_flags |= mas->depth << MT_FLAGS_HEIGHT_OFFSET;
	mas->tree->ma_flags = new_flags;
}

static unsigned int mas_mt_height(struct ma_state *mas)
{
	return mt_height(mas->tree);
}

static inline enum maple_type mte_node_type(const struct maple_enode *entry)
{
	return ((unsigned long)entry >> MAPLE_NODE_TYPE_SHIFT) &
		MAPLE_NODE_TYPE_MASK;
}

static inline bool ma_is_dense(const enum maple_type type)
{
	return type < maple_leaf_64;
}

static inline bool ma_is_leaf(const enum maple_type type)
{
	return type < maple_range_64;
}

static inline bool mte_is_leaf(const struct maple_enode *entry)
{
	return ma_is_leaf(mte_node_type(entry));
}

 
static inline bool mt_is_reserved(const void *entry)
{
	return ((unsigned long)entry < MAPLE_RESERVED_RANGE) &&
		xa_is_internal(entry);
}

static inline void mas_set_err(struct ma_state *mas, long err)
{
	mas->node = MA_ERROR(err);
}

static inline bool mas_is_ptr(const struct ma_state *mas)
{
	return mas->node == MAS_ROOT;
}

static inline bool mas_is_start(const struct ma_state *mas)
{
	return mas->node == MAS_START;
}

bool mas_is_err(struct ma_state *mas)
{
	return xa_is_err(mas->node);
}

static __always_inline bool mas_is_overflow(struct ma_state *mas)
{
	if (unlikely(mas->node == MAS_OVERFLOW))
		return true;

	return false;
}

static __always_inline bool mas_is_underflow(struct ma_state *mas)
{
	if (unlikely(mas->node == MAS_UNDERFLOW))
		return true;

	return false;
}

static inline bool mas_searchable(struct ma_state *mas)
{
	if (mas_is_none(mas))
		return false;

	if (mas_is_ptr(mas))
		return false;

	return true;
}

static inline struct maple_node *mte_to_node(const struct maple_enode *entry)
{
	return (struct maple_node *)((unsigned long)entry & ~MAPLE_NODE_MASK);
}

 
static inline struct maple_topiary *mte_to_mat(const struct maple_enode *entry)
{
	return (struct maple_topiary *)
		((unsigned long)entry & ~MAPLE_NODE_MASK);
}

 
static inline struct maple_node *mas_mn(const struct ma_state *mas)
{
	return mte_to_node(mas->node);
}

 
static inline void mte_set_node_dead(struct maple_enode *mn)
{
	mte_to_node(mn)->parent = ma_parent_ptr(mte_to_node(mn));
	smp_wmb();  
}

 
#define MAPLE_ROOT_NODE			0x02
 
#define MAPLE_ENODE_TYPE_SHIFT		0x03
 
#define MAPLE_ENODE_NULL		0x04

static inline struct maple_enode *mt_mk_node(const struct maple_node *node,
					     enum maple_type type)
{
	return (void *)((unsigned long)node |
			(type << MAPLE_ENODE_TYPE_SHIFT) | MAPLE_ENODE_NULL);
}

static inline void *mte_mk_root(const struct maple_enode *node)
{
	return (void *)((unsigned long)node | MAPLE_ROOT_NODE);
}

static inline void *mte_safe_root(const struct maple_enode *node)
{
	return (void *)((unsigned long)node & ~MAPLE_ROOT_NODE);
}

static inline void *mte_set_full(const struct maple_enode *node)
{
	return (void *)((unsigned long)node & ~MAPLE_ENODE_NULL);
}

static inline void *mte_clear_full(const struct maple_enode *node)
{
	return (void *)((unsigned long)node | MAPLE_ENODE_NULL);
}

static inline bool mte_has_null(const struct maple_enode *node)
{
	return (unsigned long)node & MAPLE_ENODE_NULL;
}

static inline bool ma_is_root(struct maple_node *node)
{
	return ((unsigned long)node->parent & MA_ROOT_PARENT);
}

static inline bool mte_is_root(const struct maple_enode *node)
{
	return ma_is_root(mte_to_node(node));
}

static inline bool mas_is_root_limits(const struct ma_state *mas)
{
	return !mas->min && mas->max == ULONG_MAX;
}

static inline bool mt_is_alloc(struct maple_tree *mt)
{
	return (mt->ma_flags & MT_FLAGS_ALLOC_RANGE);
}

 

#define MAPLE_PARENT_ROOT		0x01

#define MAPLE_PARENT_SLOT_SHIFT		0x03
#define MAPLE_PARENT_SLOT_MASK		0xF8

#define MAPLE_PARENT_16B_SLOT_SHIFT	0x02
#define MAPLE_PARENT_16B_SLOT_MASK	0xFC

#define MAPLE_PARENT_RANGE64		0x06
#define MAPLE_PARENT_RANGE32		0x04
#define MAPLE_PARENT_NOT_RANGE16	0x02

 
static inline unsigned long mte_parent_shift(unsigned long parent)
{
	 
	if (likely(parent & MAPLE_PARENT_NOT_RANGE16))
		return MAPLE_PARENT_SLOT_SHIFT;

	return MAPLE_PARENT_16B_SLOT_SHIFT;
}

 
static inline unsigned long mte_parent_slot_mask(unsigned long parent)
{
	 
	if (likely(parent & MAPLE_PARENT_NOT_RANGE16))
		return MAPLE_PARENT_SLOT_MASK;

	return MAPLE_PARENT_16B_SLOT_MASK;
}

 
static inline
enum maple_type mas_parent_type(struct ma_state *mas, struct maple_enode *enode)
{
	unsigned long p_type;

	p_type = (unsigned long)mte_to_node(enode)->parent;
	if (WARN_ON(p_type & MAPLE_PARENT_ROOT))
		return 0;

	p_type &= MAPLE_NODE_MASK;
	p_type &= ~mte_parent_slot_mask(p_type);
	switch (p_type) {
	case MAPLE_PARENT_RANGE64:  
		if (mt_is_alloc(mas->tree))
			return maple_arange_64;
		return maple_range_64;
	}

	return 0;
}

 
static inline
void mas_set_parent(struct ma_state *mas, struct maple_enode *enode,
		    const struct maple_enode *parent, unsigned char slot)
{
	unsigned long val = (unsigned long)parent;
	unsigned long shift;
	unsigned long type;
	enum maple_type p_type = mte_node_type(parent);

	MAS_BUG_ON(mas, p_type == maple_dense);
	MAS_BUG_ON(mas, p_type == maple_leaf_64);

	switch (p_type) {
	case maple_range_64:
	case maple_arange_64:
		shift = MAPLE_PARENT_SLOT_SHIFT;
		type = MAPLE_PARENT_RANGE64;
		break;
	default:
	case maple_dense:
	case maple_leaf_64:
		shift = type = 0;
		break;
	}

	val &= ~MAPLE_NODE_MASK;  
	val |= (slot << shift) | type;
	mte_to_node(enode)->parent = ma_parent_ptr(val);
}

 
static inline unsigned int mte_parent_slot(const struct maple_enode *enode)
{
	unsigned long val = (unsigned long)mte_to_node(enode)->parent;

	if (val & MA_ROOT_PARENT)
		return 0;

	 
	return (val & MAPLE_PARENT_16B_SLOT_MASK) >> mte_parent_shift(val);
}

 
static inline struct maple_node *mte_parent(const struct maple_enode *enode)
{
	return (void *)((unsigned long)
			(mte_to_node(enode)->parent) & ~MAPLE_NODE_MASK);
}

 
static inline bool ma_dead_node(const struct maple_node *node)
{
	struct maple_node *parent;

	 
	smp_rmb();
	parent = (void *)((unsigned long) node->parent & ~MAPLE_NODE_MASK);
	return (parent == node);
}

 
static inline bool mte_dead_node(const struct maple_enode *enode)
{
	struct maple_node *parent, *node;

	node = mte_to_node(enode);
	 
	smp_rmb();
	parent = mte_parent(enode);
	return (parent == node);
}

 
static inline unsigned long mas_allocated(const struct ma_state *mas)
{
	if (!mas->alloc || ((unsigned long)mas->alloc & 0x1))
		return 0;

	return mas->alloc->total;
}

 
static inline void mas_set_alloc_req(struct ma_state *mas, unsigned long count)
{
	if (!mas->alloc || ((unsigned long)mas->alloc & 0x1)) {
		if (!count)
			mas->alloc = NULL;
		else
			mas->alloc = (struct maple_alloc *)(((count) << 1U) | 1U);
		return;
	}

	mas->alloc->request_count = count;
}

 
static inline unsigned int mas_alloc_req(const struct ma_state *mas)
{
	if ((unsigned long)mas->alloc & 0x1)
		return (unsigned long)(mas->alloc) >> 1;
	else if (mas->alloc)
		return mas->alloc->request_count;
	return 0;
}

 
static inline unsigned long *ma_pivots(struct maple_node *node,
					   enum maple_type type)
{
	switch (type) {
	case maple_arange_64:
		return node->ma64.pivot;
	case maple_range_64:
	case maple_leaf_64:
		return node->mr64.pivot;
	case maple_dense:
		return NULL;
	}
	return NULL;
}

 
static inline unsigned long *ma_gaps(struct maple_node *node,
				     enum maple_type type)
{
	switch (type) {
	case maple_arange_64:
		return node->ma64.gap;
	case maple_range_64:
	case maple_leaf_64:
	case maple_dense:
		return NULL;
	}
	return NULL;
}

 
static inline unsigned long mas_pivot(struct ma_state *mas, unsigned char piv)
{
	struct maple_node *node = mas_mn(mas);
	enum maple_type type = mte_node_type(mas->node);

	if (MAS_WARN_ON(mas, piv >= mt_pivots[type])) {
		mas_set_err(mas, -EIO);
		return 0;
	}

	switch (type) {
	case maple_arange_64:
		return node->ma64.pivot[piv];
	case maple_range_64:
	case maple_leaf_64:
		return node->mr64.pivot[piv];
	case maple_dense:
		return 0;
	}
	return 0;
}

 
static inline unsigned long
mas_safe_pivot(const struct ma_state *mas, unsigned long *pivots,
	       unsigned char piv, enum maple_type type)
{
	if (piv >= mt_pivots[type])
		return mas->max;

	return pivots[piv];
}

 
static inline unsigned long
mas_safe_min(struct ma_state *mas, unsigned long *pivots, unsigned char offset)
{
	if (likely(offset))
		return pivots[offset - 1] + 1;

	return mas->min;
}

 
static inline void mte_set_pivot(struct maple_enode *mn, unsigned char piv,
				unsigned long val)
{
	struct maple_node *node = mte_to_node(mn);
	enum maple_type type = mte_node_type(mn);

	BUG_ON(piv >= mt_pivots[type]);
	switch (type) {
	default:
	case maple_range_64:
	case maple_leaf_64:
		node->mr64.pivot[piv] = val;
		break;
	case maple_arange_64:
		node->ma64.pivot[piv] = val;
		break;
	case maple_dense:
		break;
	}

}

 
static inline void __rcu **ma_slots(struct maple_node *mn, enum maple_type mt)
{
	switch (mt) {
	default:
	case maple_arange_64:
		return mn->ma64.slot;
	case maple_range_64:
	case maple_leaf_64:
		return mn->mr64.slot;
	case maple_dense:
		return mn->slot;
	}
}

static inline bool mt_write_locked(const struct maple_tree *mt)
{
	return mt_external_lock(mt) ? mt_write_lock_is_held(mt) :
		lockdep_is_held(&mt->ma_lock);
}

static inline bool mt_locked(const struct maple_tree *mt)
{
	return mt_external_lock(mt) ? mt_lock_is_held(mt) :
		lockdep_is_held(&mt->ma_lock);
}

static inline void *mt_slot(const struct maple_tree *mt,
		void __rcu **slots, unsigned char offset)
{
	return rcu_dereference_check(slots[offset], mt_locked(mt));
}

static inline void *mt_slot_locked(struct maple_tree *mt, void __rcu **slots,
				   unsigned char offset)
{
	return rcu_dereference_protected(slots[offset], mt_write_locked(mt));
}
 
static inline void *mas_slot_locked(struct ma_state *mas, void __rcu **slots,
				       unsigned char offset)
{
	return mt_slot_locked(mas->tree, slots, offset);
}

 
static inline void *mas_slot(struct ma_state *mas, void __rcu **slots,
			     unsigned char offset)
{
	return mt_slot(mas->tree, slots, offset);
}

 
static inline void *mas_root(struct ma_state *mas)
{
	return rcu_dereference_check(mas->tree->ma_root, mt_locked(mas->tree));
}

static inline void *mt_root_locked(struct maple_tree *mt)
{
	return rcu_dereference_protected(mt->ma_root, mt_write_locked(mt));
}

 
static inline void *mas_root_locked(struct ma_state *mas)
{
	return mt_root_locked(mas->tree);
}

static inline struct maple_metadata *ma_meta(struct maple_node *mn,
					     enum maple_type mt)
{
	switch (mt) {
	case maple_arange_64:
		return &mn->ma64.meta;
	default:
		return &mn->mr64.meta;
	}
}

 
static inline void ma_set_meta(struct maple_node *mn, enum maple_type mt,
			       unsigned char offset, unsigned char end)
{
	struct maple_metadata *meta = ma_meta(mn, mt);

	meta->gap = offset;
	meta->end = end;
}

 
static inline void mt_clear_meta(struct maple_tree *mt, struct maple_node *mn,
				  enum maple_type type)
{
	struct maple_metadata *meta;
	unsigned long *pivots;
	void __rcu **slots;
	void *next;

	switch (type) {
	case maple_range_64:
		pivots = mn->mr64.pivot;
		if (unlikely(pivots[MAPLE_RANGE64_SLOTS - 2])) {
			slots = mn->mr64.slot;
			next = mt_slot_locked(mt, slots,
					      MAPLE_RANGE64_SLOTS - 1);
			if (unlikely((mte_to_node(next) &&
				      mte_node_type(next))))
				return;  
		}
		fallthrough;
	case maple_arange_64:
		meta = ma_meta(mn, type);
		break;
	default:
		return;
	}

	meta->gap = 0;
	meta->end = 0;
}

 
static inline unsigned char ma_meta_end(struct maple_node *mn,
					enum maple_type mt)
{
	struct maple_metadata *meta = ma_meta(mn, mt);

	return meta->end;
}

 
static inline unsigned char ma_meta_gap(struct maple_node *mn,
					enum maple_type mt)
{
	return mn->ma64.meta.gap;
}

 
static inline void ma_set_meta_gap(struct maple_node *mn, enum maple_type mt,
				   unsigned char offset)
{

	struct maple_metadata *meta = ma_meta(mn, mt);

	meta->gap = offset;
}

 
static inline void mat_add(struct ma_topiary *mat,
			   struct maple_enode *dead_enode)
{
	mte_set_node_dead(dead_enode);
	mte_to_mat(dead_enode)->next = NULL;
	if (!mat->tail) {
		mat->tail = mat->head = dead_enode;
		return;
	}

	mte_to_mat(mat->tail)->next = dead_enode;
	mat->tail = dead_enode;
}

static void mt_free_walk(struct rcu_head *head);
static void mt_destroy_walk(struct maple_enode *enode, struct maple_tree *mt,
			    bool free);
 
static void mas_mat_destroy(struct ma_state *mas, struct ma_topiary *mat)
{
	struct maple_enode *next;
	struct maple_node *node;
	bool in_rcu = mt_in_rcu(mas->tree);

	while (mat->head) {
		next = mte_to_mat(mat->head)->next;
		node = mte_to_node(mat->head);
		mt_destroy_walk(mat->head, mas->tree, !in_rcu);
		if (in_rcu)
			call_rcu(&node->rcu, mt_free_walk);
		mat->head = next;
	}
}
 
static inline void mas_descend(struct ma_state *mas)
{
	enum maple_type type;
	unsigned long *pivots;
	struct maple_node *node;
	void __rcu **slots;

	node = mas_mn(mas);
	type = mte_node_type(mas->node);
	pivots = ma_pivots(node, type);
	slots = ma_slots(node, type);

	if (mas->offset)
		mas->min = pivots[mas->offset - 1] + 1;
	mas->max = mas_safe_pivot(mas, pivots, mas->offset, type);
	mas->node = mas_slot(mas, slots, mas->offset);
}

 
static inline void mte_set_gap(const struct maple_enode *mn,
				 unsigned char gap, unsigned long val)
{
	switch (mte_node_type(mn)) {
	default:
		break;
	case maple_arange_64:
		mte_to_node(mn)->ma64.gap[gap] = val;
		break;
	}
}

 
static int mas_ascend(struct ma_state *mas)
{
	struct maple_enode *p_enode;  
	struct maple_enode *a_enode;  
	struct maple_node *a_node;  
	struct maple_node *p_node;  
	unsigned char a_slot;
	enum maple_type a_type;
	unsigned long min, max;
	unsigned long *pivots;
	bool set_max = false, set_min = false;

	a_node = mas_mn(mas);
	if (ma_is_root(a_node)) {
		mas->offset = 0;
		return 0;
	}

	p_node = mte_parent(mas->node);
	if (unlikely(a_node == p_node))
		return 1;

	a_type = mas_parent_type(mas, mas->node);
	mas->offset = mte_parent_slot(mas->node);
	a_enode = mt_mk_node(p_node, a_type);

	 
	if (p_node != mte_parent(mas->node))
		return 1;

	mas->node = a_enode;

	if (mte_is_root(a_enode)) {
		mas->max = ULONG_MAX;
		mas->min = 0;
		return 0;
	}

	if (!mas->min)
		set_min = true;

	if (mas->max == ULONG_MAX)
		set_max = true;

	min = 0;
	max = ULONG_MAX;
	do {
		p_enode = a_enode;
		a_type = mas_parent_type(mas, p_enode);
		a_node = mte_parent(p_enode);
		a_slot = mte_parent_slot(p_enode);
		a_enode = mt_mk_node(a_node, a_type);
		pivots = ma_pivots(a_node, a_type);

		if (unlikely(ma_dead_node(a_node)))
			return 1;

		if (!set_min && a_slot) {
			set_min = true;
			min = pivots[a_slot - 1] + 1;
		}

		if (!set_max && a_slot < mt_pivots[a_type]) {
			set_max = true;
			max = pivots[a_slot];
		}

		if (unlikely(ma_dead_node(a_node)))
			return 1;

		if (unlikely(ma_is_root(a_node)))
			break;

	} while (!set_min || !set_max);

	mas->max = max;
	mas->min = min;
	return 0;
}

 
static inline struct maple_node *mas_pop_node(struct ma_state *mas)
{
	struct maple_alloc *ret, *node = mas->alloc;
	unsigned long total = mas_allocated(mas);
	unsigned int req = mas_alloc_req(mas);

	 
	if (WARN_ON(!total))
		return NULL;

	if (total == 1) {
		 
		mas->alloc = NULL;
		ret = node;
		goto single_node;
	}

	if (node->node_count == 1) {
		 
		mas->alloc = node->slot[0];
		mas->alloc->total = node->total - 1;
		ret = node;
		goto new_head;
	}
	node->total--;
	ret = node->slot[--node->node_count];
	node->slot[node->node_count] = NULL;

single_node:
new_head:
	if (req) {
		req++;
		mas_set_alloc_req(mas, req);
	}

	memset(ret, 0, sizeof(*ret));
	return (struct maple_node *)ret;
}

 
static inline void mas_push_node(struct ma_state *mas, struct maple_node *used)
{
	struct maple_alloc *reuse = (struct maple_alloc *)used;
	struct maple_alloc *head = mas->alloc;
	unsigned long count;
	unsigned int requested = mas_alloc_req(mas);

	count = mas_allocated(mas);

	reuse->request_count = 0;
	reuse->node_count = 0;
	if (count && (head->node_count < MAPLE_ALLOC_SLOTS)) {
		head->slot[head->node_count++] = reuse;
		head->total++;
		goto done;
	}

	reuse->total = 1;
	if ((head) && !((unsigned long)head & 0x1)) {
		reuse->slot[0] = head;
		reuse->node_count = 1;
		reuse->total += head->total;
	}

	mas->alloc = reuse;
done:
	if (requested > 1)
		mas_set_alloc_req(mas, requested - 1);
}

 
static inline void mas_alloc_nodes(struct ma_state *mas, gfp_t gfp)
{
	struct maple_alloc *node;
	unsigned long allocated = mas_allocated(mas);
	unsigned int requested = mas_alloc_req(mas);
	unsigned int count;
	void **slots = NULL;
	unsigned int max_req = 0;

	if (!requested)
		return;

	mas_set_alloc_req(mas, 0);
	if (mas->mas_flags & MA_STATE_PREALLOC) {
		if (allocated)
			return;
		WARN_ON(!allocated);
	}

	if (!allocated || mas->alloc->node_count == MAPLE_ALLOC_SLOTS) {
		node = (struct maple_alloc *)mt_alloc_one(gfp);
		if (!node)
			goto nomem_one;

		if (allocated) {
			node->slot[0] = mas->alloc;
			node->node_count = 1;
		} else {
			node->node_count = 0;
		}

		mas->alloc = node;
		node->total = ++allocated;
		requested--;
	}

	node = mas->alloc;
	node->request_count = 0;
	while (requested) {
		max_req = MAPLE_ALLOC_SLOTS - node->node_count;
		slots = (void **)&node->slot[node->node_count];
		max_req = min(requested, max_req);
		count = mt_alloc_bulk(gfp, max_req, slots);
		if (!count)
			goto nomem_bulk;

		if (node->node_count == 0) {
			node->slot[0]->node_count = 0;
			node->slot[0]->request_count = 0;
		}

		node->node_count += count;
		allocated += count;
		node = node->slot[0];
		requested -= count;
	}
	mas->alloc->total = allocated;
	return;

nomem_bulk:
	 
	memset(slots, 0, max_req * sizeof(unsigned long));
nomem_one:
	mas_set_alloc_req(mas, requested);
	if (mas->alloc && !(((unsigned long)mas->alloc & 0x1)))
		mas->alloc->total = allocated;
	mas_set_err(mas, -ENOMEM);
}

 
static inline void mas_free(struct ma_state *mas, struct maple_enode *used)
{
	struct maple_node *tmp = mte_to_node(used);

	if (mt_in_rcu(mas->tree))
		ma_free_rcu(tmp);
	else
		mas_push_node(mas, tmp);
}

 
static void mas_node_count_gfp(struct ma_state *mas, int count, gfp_t gfp)
{
	unsigned long allocated = mas_allocated(mas);

	if (allocated < count) {
		mas_set_alloc_req(mas, count - allocated);
		mas_alloc_nodes(mas, gfp);
	}
}

 
static void mas_node_count(struct ma_state *mas, int count)
{
	return mas_node_count_gfp(mas, count, GFP_NOWAIT | __GFP_NOWARN);
}

 
static inline struct maple_enode *mas_start(struct ma_state *mas)
{
	if (likely(mas_is_start(mas))) {
		struct maple_enode *root;

		mas->min = 0;
		mas->max = ULONG_MAX;

retry:
		mas->depth = 0;
		root = mas_root(mas);
		 
		if (likely(xa_is_node(root))) {
			mas->depth = 1;
			mas->node = mte_safe_root(root);
			mas->offset = 0;
			if (mte_dead_node(mas->node))
				goto retry;

			return NULL;
		}

		 
		if (unlikely(!root)) {
			mas->node = MAS_NONE;
			mas->offset = MAPLE_NODE_SLOTS;
			return NULL;
		}

		 
		mas->node = MAS_ROOT;
		mas->offset = MAPLE_NODE_SLOTS;

		 
		if (mas->index > 0)
			return NULL;

		return root;
	}

	return NULL;
}

 
static inline unsigned char ma_data_end(struct maple_node *node,
					enum maple_type type,
					unsigned long *pivots,
					unsigned long max)
{
	unsigned char offset;

	if (!pivots)
		return 0;

	if (type == maple_arange_64)
		return ma_meta_end(node, type);

	offset = mt_pivots[type] - 1;
	if (likely(!pivots[offset]))
		return ma_meta_end(node, type);

	if (likely(pivots[offset] == max))
		return offset;

	return mt_pivots[type];
}

 
static inline unsigned char mas_data_end(struct ma_state *mas)
{
	enum maple_type type;
	struct maple_node *node;
	unsigned char offset;
	unsigned long *pivots;

	type = mte_node_type(mas->node);
	node = mas_mn(mas);
	if (type == maple_arange_64)
		return ma_meta_end(node, type);

	pivots = ma_pivots(node, type);
	if (unlikely(ma_dead_node(node)))
		return 0;

	offset = mt_pivots[type] - 1;
	if (likely(!pivots[offset]))
		return ma_meta_end(node, type);

	if (likely(pivots[offset] == mas->max))
		return offset;

	return mt_pivots[type];
}

 
static unsigned long mas_leaf_max_gap(struct ma_state *mas)
{
	enum maple_type mt;
	unsigned long pstart, gap, max_gap;
	struct maple_node *mn;
	unsigned long *pivots;
	void __rcu **slots;
	unsigned char i;
	unsigned char max_piv;

	mt = mte_node_type(mas->node);
	mn = mas_mn(mas);
	slots = ma_slots(mn, mt);
	max_gap = 0;
	if (unlikely(ma_is_dense(mt))) {
		gap = 0;
		for (i = 0; i < mt_slots[mt]; i++) {
			if (slots[i]) {
				if (gap > max_gap)
					max_gap = gap;
				gap = 0;
			} else {
				gap++;
			}
		}
		if (gap > max_gap)
			max_gap = gap;
		return max_gap;
	}

	 
	pivots = ma_pivots(mn, mt);
	if (likely(!slots[0])) {
		max_gap = pivots[0] - mas->min + 1;
		i = 2;
	} else {
		i = 1;
	}

	 
	max_piv = ma_data_end(mn, mt, pivots, mas->max) - 1;
	 
	if (unlikely(mas->max == ULONG_MAX) && !slots[max_piv + 1]) {
		gap = ULONG_MAX - pivots[max_piv];
		if (gap > max_gap)
			max_gap = gap;
	}

	for (; i <= max_piv; i++) {
		 
		if (likely(slots[i]))
			continue;

		pstart = pivots[i - 1];
		gap = pivots[i] - pstart;
		if (gap > max_gap)
			max_gap = gap;

		 
		i++;
	}
	return max_gap;
}

 
static inline unsigned long
ma_max_gap(struct maple_node *node, unsigned long *gaps, enum maple_type mt,
	    unsigned char *off)
{
	unsigned char offset, i;
	unsigned long max_gap = 0;

	i = offset = ma_meta_end(node, mt);
	do {
		if (gaps[i] > max_gap) {
			max_gap = gaps[i];
			offset = i;
		}
	} while (i--);

	*off = offset;
	return max_gap;
}

 
static inline unsigned long mas_max_gap(struct ma_state *mas)
{
	unsigned long *gaps;
	unsigned char offset;
	enum maple_type mt;
	struct maple_node *node;

	mt = mte_node_type(mas->node);
	if (ma_is_leaf(mt))
		return mas_leaf_max_gap(mas);

	node = mas_mn(mas);
	MAS_BUG_ON(mas, mt != maple_arange_64);
	offset = ma_meta_gap(node, mt);
	gaps = ma_gaps(node, mt);
	return gaps[offset];
}

 
static inline void mas_parent_gap(struct ma_state *mas, unsigned char offset,
		unsigned long new)
{
	unsigned long meta_gap = 0;
	struct maple_node *pnode;
	struct maple_enode *penode;
	unsigned long *pgaps;
	unsigned char meta_offset;
	enum maple_type pmt;

	pnode = mte_parent(mas->node);
	pmt = mas_parent_type(mas, mas->node);
	penode = mt_mk_node(pnode, pmt);
	pgaps = ma_gaps(pnode, pmt);

ascend:
	MAS_BUG_ON(mas, pmt != maple_arange_64);
	meta_offset = ma_meta_gap(pnode, pmt);
	meta_gap = pgaps[meta_offset];

	pgaps[offset] = new;

	if (meta_gap == new)
		return;

	if (offset != meta_offset) {
		if (meta_gap > new)
			return;

		ma_set_meta_gap(pnode, pmt, offset);
	} else if (new < meta_gap) {
		new = ma_max_gap(pnode, pgaps, pmt, &meta_offset);
		ma_set_meta_gap(pnode, pmt, meta_offset);
	}

	if (ma_is_root(pnode))
		return;

	 
	pnode = mte_parent(penode);
	pmt = mas_parent_type(mas, penode);
	pgaps = ma_gaps(pnode, pmt);
	offset = mte_parent_slot(penode);
	penode = mt_mk_node(pnode, pmt);
	goto ascend;
}

 
static inline void mas_update_gap(struct ma_state *mas)
{
	unsigned char pslot;
	unsigned long p_gap;
	unsigned long max_gap;

	if (!mt_is_alloc(mas->tree))
		return;

	if (mte_is_root(mas->node))
		return;

	max_gap = mas_max_gap(mas);

	pslot = mte_parent_slot(mas->node);
	p_gap = ma_gaps(mte_parent(mas->node),
			mas_parent_type(mas, mas->node))[pslot];

	if (p_gap != max_gap)
		mas_parent_gap(mas, pslot, max_gap);
}

 
static inline void mas_adopt_children(struct ma_state *mas,
		struct maple_enode *parent)
{
	enum maple_type type = mte_node_type(parent);
	struct maple_node *node = mte_to_node(parent);
	void __rcu **slots = ma_slots(node, type);
	unsigned long *pivots = ma_pivots(node, type);
	struct maple_enode *child;
	unsigned char offset;

	offset = ma_data_end(node, type, pivots, mas->max);
	do {
		child = mas_slot_locked(mas, slots, offset);
		mas_set_parent(mas, child, parent, offset);
	} while (offset--);
}

 
static inline void mas_put_in_tree(struct ma_state *mas,
		struct maple_enode *old_enode)
	__must_hold(mas->tree->ma_lock)
{
	unsigned char offset;
	void __rcu **slots;

	if (mte_is_root(mas->node)) {
		mas_mn(mas)->parent = ma_parent_ptr(mas_tree_parent(mas));
		rcu_assign_pointer(mas->tree->ma_root, mte_mk_root(mas->node));
		mas_set_height(mas);
	} else {

		offset = mte_parent_slot(mas->node);
		slots = ma_slots(mte_parent(mas->node),
				 mas_parent_type(mas, mas->node));
		rcu_assign_pointer(slots[offset], mas->node);
	}

	mte_set_node_dead(old_enode);
}

 
static inline void mas_replace_node(struct ma_state *mas,
		struct maple_enode *old_enode)
	__must_hold(mas->tree->ma_lock)
{
	mas_put_in_tree(mas, old_enode);
	mas_free(mas, old_enode);
}

 
static inline bool mas_find_child(struct ma_state *mas, struct ma_state *child)
	__must_hold(mas->tree->ma_lock)
{
	enum maple_type mt;
	unsigned char offset;
	unsigned char end;
	unsigned long *pivots;
	struct maple_enode *entry;
	struct maple_node *node;
	void __rcu **slots;

	mt = mte_node_type(mas->node);
	node = mas_mn(mas);
	slots = ma_slots(node, mt);
	pivots = ma_pivots(node, mt);
	end = ma_data_end(node, mt, pivots, mas->max);
	for (offset = mas->offset; offset <= end; offset++) {
		entry = mas_slot_locked(mas, slots, offset);
		if (mte_parent(entry) == node) {
			*child = *mas;
			mas->offset = offset + 1;
			child->offset = offset;
			mas_descend(child);
			child->offset = 0;
			return true;
		}
	}
	return false;
}

 
static inline void mab_shift_right(struct maple_big_node *b_node,
				 unsigned char shift)
{
	unsigned long size = b_node->b_end * sizeof(unsigned long);

	memmove(b_node->pivot + shift, b_node->pivot, size);
	memmove(b_node->slot + shift, b_node->slot, size);
	if (b_node->type == maple_arange_64)
		memmove(b_node->gap + shift, b_node->gap, size);
}

 
static inline bool mab_middle_node(struct maple_big_node *b_node, int split,
				   unsigned char slot_count)
{
	unsigned char size = b_node->b_end;

	if (size >= 2 * slot_count)
		return true;

	if (!b_node->slot[split] && (size >= 2 * slot_count - 1))
		return true;

	return false;
}

 
static inline int mab_no_null_split(struct maple_big_node *b_node,
				    unsigned char split, unsigned char slot_count)
{
	if (!b_node->slot[split]) {
		 
		if ((split < slot_count - 1) &&
		    (b_node->b_end - split) > (mt_min_slots[b_node->type]))
			split++;
		else
			split--;
	}
	return split;
}

 
static inline int mab_calc_split(struct ma_state *mas,
	 struct maple_big_node *bn, unsigned char *mid_split, unsigned long min)
{
	unsigned char b_end = bn->b_end;
	int split = b_end / 2;  
	unsigned char slot_min, slot_count = mt_slots[bn->type];

	 
	if (unlikely((mas->mas_flags & MA_STATE_BULK))) {
		*mid_split = 0;
		split = b_end - mt_min_slots[bn->type];

		if (!ma_is_leaf(bn->type))
			return split;

		mas->mas_flags |= MA_STATE_REBALANCE;
		if (!bn->slot[split])
			split--;
		return split;
	}

	 
	if (unlikely(mab_middle_node(bn, split, slot_count))) {
		split = b_end / 3;
		*mid_split = split * 2;
	} else {
		slot_min = mt_min_slots[bn->type];

		*mid_split = 0;
		 
		while ((split < slot_count - 1) &&
		       ((bn->pivot[split] - min) < slot_count - 1) &&
		       (b_end - split > slot_min))
			split++;
	}

	 
	split = mab_no_null_split(bn, split, slot_count);

	if (unlikely(*mid_split))
		*mid_split = mab_no_null_split(bn, *mid_split, slot_count);

	return split;
}

 
static inline void mas_mab_cp(struct ma_state *mas, unsigned char mas_start,
			unsigned char mas_end, struct maple_big_node *b_node,
			unsigned char mab_start)
{
	enum maple_type mt;
	struct maple_node *node;
	void __rcu **slots;
	unsigned long *pivots, *gaps;
	int i = mas_start, j = mab_start;
	unsigned char piv_end;

	node = mas_mn(mas);
	mt = mte_node_type(mas->node);
	pivots = ma_pivots(node, mt);
	if (!i) {
		b_node->pivot[j] = pivots[i++];
		if (unlikely(i > mas_end))
			goto complete;
		j++;
	}

	piv_end = min(mas_end, mt_pivots[mt]);
	for (; i < piv_end; i++, j++) {
		b_node->pivot[j] = pivots[i];
		if (unlikely(!b_node->pivot[j]))
			break;

		if (unlikely(mas->max == b_node->pivot[j]))
			goto complete;
	}

	if (likely(i <= mas_end))
		b_node->pivot[j] = mas_safe_pivot(mas, pivots, i, mt);

complete:
	b_node->b_end = ++j;
	j -= mab_start;
	slots = ma_slots(node, mt);
	memcpy(b_node->slot + mab_start, slots + mas_start, sizeof(void *) * j);
	if (!ma_is_leaf(mt) && mt_is_alloc(mas->tree)) {
		gaps = ma_gaps(node, mt);
		memcpy(b_node->gap + mab_start, gaps + mas_start,
		       sizeof(unsigned long) * j);
	}
}

 
static inline void mas_leaf_set_meta(struct ma_state *mas,
		struct maple_node *node, unsigned long *pivots,
		enum maple_type mt, unsigned char end)
{
	 
	if (mt_pivots[mt] <= end)
		return;

	if (pivots[end] && pivots[end] < mas->max)
		end++;

	if (end < mt_slots[mt] - 1)
		ma_set_meta(node, mt, 0, end);
}

 
static inline void mab_mas_cp(struct maple_big_node *b_node,
			      unsigned char mab_start, unsigned char mab_end,
			      struct ma_state *mas, bool new_max)
{
	int i, j = 0;
	enum maple_type mt = mte_node_type(mas->node);
	struct maple_node *node = mte_to_node(mas->node);
	void __rcu **slots = ma_slots(node, mt);
	unsigned long *pivots = ma_pivots(node, mt);
	unsigned long *gaps = NULL;
	unsigned char end;

	if (mab_end - mab_start > mt_pivots[mt])
		mab_end--;

	if (!pivots[mt_pivots[mt] - 1])
		slots[mt_pivots[mt]] = NULL;

	i = mab_start;
	do {
		pivots[j++] = b_node->pivot[i++];
	} while (i <= mab_end && likely(b_node->pivot[i]));

	memcpy(slots, b_node->slot + mab_start,
	       sizeof(void *) * (i - mab_start));

	if (new_max)
		mas->max = b_node->pivot[i - 1];

	end = j - 1;
	if (likely(!ma_is_leaf(mt) && mt_is_alloc(mas->tree))) {
		unsigned long max_gap = 0;
		unsigned char offset = 0;

		gaps = ma_gaps(node, mt);
		do {
			gaps[--j] = b_node->gap[--i];
			if (gaps[j] > max_gap) {
				offset = j;
				max_gap = gaps[j];
			}
		} while (j);

		ma_set_meta(node, mt, offset, end);
	} else {
		mas_leaf_set_meta(mas, node, pivots, mt, end);
	}
}

 
static inline void mas_bulk_rebalance(struct ma_state *mas, unsigned char end,
				      enum maple_type mt)
{
	if (!(mas->mas_flags & MA_STATE_BULK))
		return;

	if (mte_is_root(mas->node))
		return;

	if (end > mt_min_slots[mt]) {
		mas->mas_flags &= ~MA_STATE_REBALANCE;
		return;
	}
}

 
static noinline_for_kasan void mas_store_b_node(struct ma_wr_state *wr_mas,
		struct maple_big_node *b_node, unsigned char offset_end)
{
	unsigned char slot;
	unsigned char b_end;
	 
	unsigned long piv;
	struct ma_state *mas = wr_mas->mas;

	b_node->type = wr_mas->type;
	b_end = 0;
	slot = mas->offset;
	if (slot) {
		 
		mas_mab_cp(mas, 0, slot - 1, b_node, 0);
		b_end = b_node->b_end;
		piv = b_node->pivot[b_end - 1];
	} else
		piv = mas->min - 1;

	if (piv + 1 < mas->index) {
		 
		b_node->slot[b_end] = wr_mas->content;
		if (!wr_mas->content)
			b_node->gap[b_end] = mas->index - 1 - piv;
		b_node->pivot[b_end++] = mas->index - 1;
	}

	 
	mas->offset = b_end;
	b_node->slot[b_end] = wr_mas->entry;
	b_node->pivot[b_end] = mas->last;

	 
	if (mas->last >= mas->max)
		goto b_end;

	 
	piv = mas_safe_pivot(mas, wr_mas->pivots, offset_end, wr_mas->type);
	if (piv > mas->last) {
		if (piv == ULONG_MAX)
			mas_bulk_rebalance(mas, b_node->b_end, wr_mas->type);

		if (offset_end != slot)
			wr_mas->content = mas_slot_locked(mas, wr_mas->slots,
							  offset_end);

		b_node->slot[++b_end] = wr_mas->content;
		if (!wr_mas->content)
			b_node->gap[b_end] = piv - mas->last + 1;
		b_node->pivot[b_end] = piv;
	}

	slot = offset_end + 1;
	if (slot > wr_mas->node_end)
		goto b_end;

	 
	mas_mab_cp(mas, slot, wr_mas->node_end + 1, b_node, ++b_end);
	b_node->b_end--;
	return;

b_end:
	b_node->b_end = b_end;
}

 
static inline bool mas_prev_sibling(struct ma_state *mas)
{
	unsigned int p_slot = mte_parent_slot(mas->node);

	if (mte_is_root(mas->node))
		return false;

	if (!p_slot)
		return false;

	mas_ascend(mas);
	mas->offset = p_slot - 1;
	mas_descend(mas);
	return true;
}

 
static inline bool mas_next_sibling(struct ma_state *mas)
{
	MA_STATE(parent, mas->tree, mas->index, mas->last);

	if (mte_is_root(mas->node))
		return false;

	parent = *mas;
	mas_ascend(&parent);
	parent.offset = mte_parent_slot(mas->node) + 1;
	if (parent.offset > mas_data_end(&parent))
		return false;

	*mas = parent;
	mas_descend(mas);
	return true;
}

 
static inline struct maple_enode *mte_node_or_none(struct maple_enode *enode)
{
	if (enode)
		return enode;

	return ma_enode_ptr(MAS_NONE);
}

 
static inline void mas_wr_node_walk(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;
	unsigned char count, offset;

	if (unlikely(ma_is_dense(wr_mas->type))) {
		wr_mas->r_max = wr_mas->r_min = mas->index;
		mas->offset = mas->index = mas->min;
		return;
	}

	wr_mas->node = mas_mn(wr_mas->mas);
	wr_mas->pivots = ma_pivots(wr_mas->node, wr_mas->type);
	count = wr_mas->node_end = ma_data_end(wr_mas->node, wr_mas->type,
					       wr_mas->pivots, mas->max);
	offset = mas->offset;

	while (offset < count && mas->index > wr_mas->pivots[offset])
		offset++;

	wr_mas->r_max = offset < count ? wr_mas->pivots[offset] : mas->max;
	wr_mas->r_min = mas_safe_min(mas, wr_mas->pivots, offset);
	wr_mas->offset_end = mas->offset = offset;
}

 
static inline void mast_rebalance_next(struct maple_subtree_state *mast)
{
	unsigned char b_end = mast->bn->b_end;

	mas_mab_cp(mast->orig_r, 0, mt_slot_count(mast->orig_r->node),
		   mast->bn, b_end);
	mast->orig_r->last = mast->orig_r->max;
}

 
static inline void mast_rebalance_prev(struct maple_subtree_state *mast)
{
	unsigned char end = mas_data_end(mast->orig_l) + 1;
	unsigned char b_end = mast->bn->b_end;

	mab_shift_right(mast->bn, end);
	mas_mab_cp(mast->orig_l, 0, end - 1, mast->bn, 0);
	mast->l->min = mast->orig_l->min;
	mast->orig_l->index = mast->orig_l->min;
	mast->bn->b_end = end + b_end;
	mast->l->offset += end;
}

 
static inline
bool mast_spanning_rebalance(struct maple_subtree_state *mast)
{
	struct ma_state r_tmp = *mast->orig_r;
	struct ma_state l_tmp = *mast->orig_l;
	unsigned char depth = 0;

	r_tmp = *mast->orig_r;
	l_tmp = *mast->orig_l;
	do {
		mas_ascend(mast->orig_r);
		mas_ascend(mast->orig_l);
		depth++;
		if (mast->orig_r->offset < mas_data_end(mast->orig_r)) {
			mast->orig_r->offset++;
			do {
				mas_descend(mast->orig_r);
				mast->orig_r->offset = 0;
			} while (--depth);

			mast_rebalance_next(mast);
			*mast->orig_l = l_tmp;
			return true;
		} else if (mast->orig_l->offset != 0) {
			mast->orig_l->offset--;
			do {
				mas_descend(mast->orig_l);
				mast->orig_l->offset =
					mas_data_end(mast->orig_l);
			} while (--depth);

			mast_rebalance_prev(mast);
			*mast->orig_r = r_tmp;
			return true;
		}
	} while (!mte_is_root(mast->orig_r->node));

	*mast->orig_r = r_tmp;
	*mast->orig_l = l_tmp;
	return false;
}

 
static inline void mast_ascend(struct maple_subtree_state *mast)
{
	MA_WR_STATE(wr_mas, mast->orig_r,  NULL);
	mas_ascend(mast->orig_l);
	mas_ascend(mast->orig_r);

	mast->orig_r->offset = 0;
	mast->orig_r->index = mast->r->max;
	 
	if (mast->orig_r->last < mast->orig_r->index)
		mast->orig_r->last = mast->orig_r->index;

	wr_mas.type = mte_node_type(mast->orig_r->node);
	mas_wr_node_walk(&wr_mas);
	 
	mast->orig_l->offset = 0;
	mast->orig_l->index = mast->l->min;
	wr_mas.mas = mast->orig_l;
	wr_mas.type = mte_node_type(mast->orig_l->node);
	mas_wr_node_walk(&wr_mas);

	mast->bn->type = wr_mas.type;
}

 
static inline struct maple_enode
*mas_new_ma_node(struct ma_state *mas, struct maple_big_node *b_node)
{
	return mt_mk_node(ma_mnode_ptr(mas_pop_node(mas)), b_node->type);
}

 
static inline unsigned char mas_mab_to_node(struct ma_state *mas,
	struct maple_big_node *b_node, struct maple_enode **left,
	struct maple_enode **right, struct maple_enode **middle,
	unsigned char *mid_split, unsigned long min)
{
	unsigned char split = 0;
	unsigned char slot_count = mt_slots[b_node->type];

	*left = mas_new_ma_node(mas, b_node);
	*right = NULL;
	*middle = NULL;
	*mid_split = 0;

	if (b_node->b_end < slot_count) {
		split = b_node->b_end;
	} else {
		split = mab_calc_split(mas, b_node, mid_split, min);
		*right = mas_new_ma_node(mas, b_node);
	}

	if (*mid_split)
		*middle = mas_new_ma_node(mas, b_node);

	return split;

}

 
static inline void mab_set_b_end(struct maple_big_node *b_node,
				 struct ma_state *mas,
				 void *entry)
{
	if (!entry)
		return;

	b_node->slot[b_node->b_end] = entry;
	if (mt_is_alloc(mas->tree))
		b_node->gap[b_node->b_end] = mas_max_gap(mas);
	b_node->pivot[b_node->b_end++] = mas->max;
}

 
static inline void mas_set_split_parent(struct ma_state *mas,
					struct maple_enode *left,
					struct maple_enode *right,
					unsigned char *slot, unsigned char split)
{
	if (mas_is_none(mas))
		return;

	if ((*slot) <= split)
		mas_set_parent(mas, mas->node, left, *slot);
	else if (right)
		mas_set_parent(mas, mas->node, right, (*slot) - split - 1);

	(*slot)++;
}

 
static inline void mte_mid_split_check(struct maple_enode **l,
				       struct maple_enode **r,
				       struct maple_enode *right,
				       unsigned char slot,
				       unsigned char *split,
				       unsigned char mid_split)
{
	if (*r == right)
		return;

	if (slot < mid_split)
		return;

	*l = *r;
	*r = right;
	*split = mid_split;
}

 
static inline void mast_set_split_parents(struct maple_subtree_state *mast,
					  struct maple_enode *left,
					  struct maple_enode *middle,
					  struct maple_enode *right,
					  unsigned char split,
					  unsigned char mid_split)
{
	unsigned char slot;
	struct maple_enode *l = left;
	struct maple_enode *r = right;

	if (mas_is_none(mast->l))
		return;

	if (middle)
		r = middle;

	slot = mast->l->offset;

	mte_mid_split_check(&l, &r, right, slot, &split, mid_split);
	mas_set_split_parent(mast->l, l, r, &slot, split);

	mte_mid_split_check(&l, &r, right, slot, &split, mid_split);
	mas_set_split_parent(mast->m, l, r, &slot, split);

	mte_mid_split_check(&l, &r, right, slot, &split, mid_split);
	mas_set_split_parent(mast->r, l, r, &slot, split);
}

 
static inline void mas_topiary_node(struct ma_state *mas,
		struct maple_enode *enode, bool in_rcu)
{
	struct maple_node *tmp;

	if (enode == MAS_NONE)
		return;

	tmp = mte_to_node(enode);
	mte_set_node_dead(enode);
	if (in_rcu)
		ma_free_rcu(tmp);
	else
		mas_push_node(mas, tmp);
}

 
static inline void mas_topiary_replace(struct ma_state *mas,
		struct maple_enode *old_enode)
{
	struct ma_state tmp[3], tmp_next[3];
	MA_TOPIARY(subtrees, mas->tree);
	bool in_rcu;
	int i, n;

	 
	mas_put_in_tree(mas, old_enode);

	 
	tmp[0] = *mas;
	tmp[0].offset = 0;
	tmp[1].node = MAS_NONE;
	tmp[2].node = MAS_NONE;
	while (!mte_is_leaf(tmp[0].node)) {
		n = 0;
		for (i = 0; i < 3; i++) {
			if (mas_is_none(&tmp[i]))
				continue;

			while (n < 3) {
				if (!mas_find_child(&tmp[i], &tmp_next[n]))
					break;
				n++;
			}

			mas_adopt_children(&tmp[i], tmp[i].node);
		}

		if (MAS_WARN_ON(mas, n == 0))
			break;

		while (n < 3)
			tmp_next[n++].node = MAS_NONE;

		for (i = 0; i < 3; i++)
			tmp[i] = tmp_next[i];
	}

	 
	if (mte_is_leaf(old_enode))
		return mas_free(mas, old_enode);

	tmp[0] = *mas;
	tmp[0].offset = 0;
	tmp[0].node = old_enode;
	tmp[1].node = MAS_NONE;
	tmp[2].node = MAS_NONE;
	in_rcu = mt_in_rcu(mas->tree);
	do {
		n = 0;
		for (i = 0; i < 3; i++) {
			if (mas_is_none(&tmp[i]))
				continue;

			while (n < 3) {
				if (!mas_find_child(&tmp[i], &tmp_next[n]))
					break;

				if ((tmp_next[n].min >= tmp_next->index) &&
				    (tmp_next[n].max <= tmp_next->last)) {
					mat_add(&subtrees, tmp_next[n].node);
					tmp_next[n].node = MAS_NONE;
				} else {
					n++;
				}
			}
		}

		if (MAS_WARN_ON(mas, n == 0))
			break;

		while (n < 3)
			tmp_next[n++].node = MAS_NONE;

		for (i = 0; i < 3; i++) {
			mas_topiary_node(mas, tmp[i].node, in_rcu);
			tmp[i] = tmp_next[i];
		}
	} while (!mte_is_leaf(tmp[0].node));

	for (i = 0; i < 3; i++)
		mas_topiary_node(mas, tmp[i].node, in_rcu);

	mas_mat_destroy(mas, &subtrees);
}

 
static inline void mas_wmb_replace(struct ma_state *mas,
		struct maple_enode *old_enode)
{
	 
	mas_topiary_replace(mas, old_enode);

	if (mte_is_leaf(mas->node))
		return;

	mas_update_gap(mas);
}

 
static inline void mast_cp_to_nodes(struct maple_subtree_state *mast,
	struct maple_enode *left, struct maple_enode *middle,
	struct maple_enode *right, unsigned char split, unsigned char mid_split)
{
	bool new_lmax = true;

	mast->l->node = mte_node_or_none(left);
	mast->m->node = mte_node_or_none(middle);
	mast->r->node = mte_node_or_none(right);

	mast->l->min = mast->orig_l->min;
	if (split == mast->bn->b_end) {
		mast->l->max = mast->orig_r->max;
		new_lmax = false;
	}

	mab_mas_cp(mast->bn, 0, split, mast->l, new_lmax);

	if (middle) {
		mab_mas_cp(mast->bn, 1 + split, mid_split, mast->m, true);
		mast->m->min = mast->bn->pivot[split] + 1;
		split = mid_split;
	}

	mast->r->max = mast->orig_r->max;
	if (right) {
		mab_mas_cp(mast->bn, 1 + split, mast->bn->b_end, mast->r, false);
		mast->r->min = mast->bn->pivot[split] + 1;
	}
}

 
static inline void mast_combine_cp_left(struct maple_subtree_state *mast)
{
	unsigned char l_slot = mast->orig_l->offset;

	if (!l_slot)
		return;

	mas_mab_cp(mast->orig_l, 0, l_slot - 1, mast->bn, 0);
}

 
static inline void mast_combine_cp_right(struct maple_subtree_state *mast)
{
	if (mast->bn->pivot[mast->bn->b_end - 1] >= mast->orig_r->max)
		return;

	mas_mab_cp(mast->orig_r, mast->orig_r->offset + 1,
		   mt_slot_count(mast->orig_r->node), mast->bn,
		   mast->bn->b_end);
	mast->orig_r->last = mast->orig_r->max;
}

 
static inline bool mast_sufficient(struct maple_subtree_state *mast)
{
	if (mast->bn->b_end > mt_min_slot_count(mast->orig_l->node))
		return true;

	return false;
}

 
static inline bool mast_overflow(struct maple_subtree_state *mast)
{
	if (mast->bn->b_end >= mt_slot_count(mast->orig_l->node))
		return true;

	return false;
}

static inline void *mtree_range_walk(struct ma_state *mas)
{
	unsigned long *pivots;
	unsigned char offset;
	struct maple_node *node;
	struct maple_enode *next, *last;
	enum maple_type type;
	void __rcu **slots;
	unsigned char end;
	unsigned long max, min;
	unsigned long prev_max, prev_min;

	next = mas->node;
	min = mas->min;
	max = mas->max;
	do {
		offset = 0;
		last = next;
		node = mte_to_node(next);
		type = mte_node_type(next);
		pivots = ma_pivots(node, type);
		end = ma_data_end(node, type, pivots, max);
		if (unlikely(ma_dead_node(node)))
			goto dead_node;

		if (pivots[offset] >= mas->index) {
			prev_max = max;
			prev_min = min;
			max = pivots[offset];
			goto next;
		}

		do {
			offset++;
		} while ((offset < end) && (pivots[offset] < mas->index));

		prev_min = min;
		min = pivots[offset - 1] + 1;
		prev_max = max;
		if (likely(offset < end && pivots[offset]))
			max = pivots[offset];

next:
		slots = ma_slots(node, type);
		next = mt_slot(mas->tree, slots, offset);
		if (unlikely(ma_dead_node(node)))
			goto dead_node;
	} while (!ma_is_leaf(type));

	mas->offset = offset;
	mas->index = min;
	mas->last = max;
	mas->min = prev_min;
	mas->max = prev_max;
	mas->node = last;
	return (void *)next;

dead_node:
	mas_reset(mas);
	return NULL;
}

 
static int mas_spanning_rebalance(struct ma_state *mas,
		struct maple_subtree_state *mast, unsigned char count)
{
	unsigned char split, mid_split;
	unsigned char slot = 0;
	struct maple_enode *left = NULL, *middle = NULL, *right = NULL;
	struct maple_enode *old_enode;

	MA_STATE(l_mas, mas->tree, mas->index, mas->index);
	MA_STATE(r_mas, mas->tree, mas->index, mas->last);
	MA_STATE(m_mas, mas->tree, mas->index, mas->index);

	 
	mast->l = &l_mas;
	mast->m = &m_mas;
	mast->r = &r_mas;
	l_mas.node = r_mas.node = m_mas.node = MAS_NONE;

	 
	if (((mast->orig_l->min != 0) || (mast->orig_r->max != ULONG_MAX)) &&
	    unlikely(mast->bn->b_end <= mt_min_slots[mast->bn->type]))
		mast_spanning_rebalance(mast);

	l_mas.depth = 0;

	 
	while (count--) {
		mast->bn->b_end--;
		mast->bn->type = mte_node_type(mast->orig_l->node);
		split = mas_mab_to_node(mas, mast->bn, &left, &right, &middle,
					&mid_split, mast->orig_l->min);
		mast_set_split_parents(mast, left, middle, right, split,
				       mid_split);
		mast_cp_to_nodes(mast, left, middle, right, split, mid_split);

		 
		memset(mast->bn, 0, sizeof(struct maple_big_node));
		mast->bn->type = mte_node_type(left);
		l_mas.depth++;

		 
		if (mas_is_root_limits(mast->l))
			goto new_root;

		mast_ascend(mast);
		mast_combine_cp_left(mast);
		l_mas.offset = mast->bn->b_end;
		mab_set_b_end(mast->bn, &l_mas, left);
		mab_set_b_end(mast->bn, &m_mas, middle);
		mab_set_b_end(mast->bn, &r_mas, right);

		 
		mast_combine_cp_right(mast);
		mast->orig_l->last = mast->orig_l->max;

		if (mast_sufficient(mast))
			continue;

		if (mast_overflow(mast))
			continue;

		 
		if (mas_is_root_limits(mast->orig_l))
			break;

		mast_spanning_rebalance(mast);

		 
		if (!count)
			count++;
	}

	l_mas.node = mt_mk_node(ma_mnode_ptr(mas_pop_node(mas)),
				mte_node_type(mast->orig_l->node));
	l_mas.depth++;
	mab_mas_cp(mast->bn, 0, mt_slots[mast->bn->type] - 1, &l_mas, true);
	mas_set_parent(mas, left, l_mas.node, slot);
	if (middle)
		mas_set_parent(mas, middle, l_mas.node, ++slot);

	if (right)
		mas_set_parent(mas, right, l_mas.node, ++slot);

	if (mas_is_root_limits(mast->l)) {
new_root:
		mas_mn(mast->l)->parent = ma_parent_ptr(mas_tree_parent(mas));
		while (!mte_is_root(mast->orig_l->node))
			mast_ascend(mast);
	} else {
		mas_mn(&l_mas)->parent = mas_mn(mast->orig_l)->parent;
	}

	old_enode = mast->orig_l->node;
	mas->depth = l_mas.depth;
	mas->node = l_mas.node;
	mas->min = l_mas.min;
	mas->max = l_mas.max;
	mas->offset = l_mas.offset;
	mas_wmb_replace(mas, old_enode);
	mtree_range_walk(mas);
	return mast->bn->b_end;
}

 
static inline int mas_rebalance(struct ma_state *mas,
				struct maple_big_node *b_node)
{
	char empty_count = mas_mt_height(mas);
	struct maple_subtree_state mast;
	unsigned char shift, b_end = ++b_node->b_end;

	MA_STATE(l_mas, mas->tree, mas->index, mas->last);
	MA_STATE(r_mas, mas->tree, mas->index, mas->last);

	trace_ma_op(__func__, mas);

	 
	mas_node_count(mas, empty_count * 2 - 1);
	if (mas_is_err(mas))
		return 0;

	mast.orig_l = &l_mas;
	mast.orig_r = &r_mas;
	mast.bn = b_node;
	mast.bn->type = mte_node_type(mas->node);

	l_mas = r_mas = *mas;

	if (mas_next_sibling(&r_mas)) {
		mas_mab_cp(&r_mas, 0, mt_slot_count(r_mas.node), b_node, b_end);
		r_mas.last = r_mas.index = r_mas.max;
	} else {
		mas_prev_sibling(&l_mas);
		shift = mas_data_end(&l_mas) + 1;
		mab_shift_right(b_node, shift);
		mas->offset += shift;
		mas_mab_cp(&l_mas, 0, shift - 1, b_node, 0);
		b_node->b_end = shift + b_end;
		l_mas.index = l_mas.last = l_mas.min;
	}

	return mas_spanning_rebalance(mas, &mast, empty_count);
}

 
static inline void mas_destroy_rebalance(struct ma_state *mas, unsigned char end)
{
	enum maple_type mt = mte_node_type(mas->node);
	struct maple_node reuse, *newnode, *parent, *new_left, *left, *node;
	struct maple_enode *eparent, *old_eparent;
	unsigned char offset, tmp, split = mt_slots[mt] / 2;
	void __rcu **l_slots, **slots;
	unsigned long *l_pivs, *pivs, gap;
	bool in_rcu = mt_in_rcu(mas->tree);

	MA_STATE(l_mas, mas->tree, mas->index, mas->last);

	l_mas = *mas;
	mas_prev_sibling(&l_mas);

	 
	if (in_rcu) {
		 
		mas_node_count(mas, 3);
		if (mas_is_err(mas))
			return;

		newnode = mas_pop_node(mas);
	} else {
		newnode = &reuse;
	}

	node = mas_mn(mas);
	newnode->parent = node->parent;
	slots = ma_slots(newnode, mt);
	pivs = ma_pivots(newnode, mt);
	left = mas_mn(&l_mas);
	l_slots = ma_slots(left, mt);
	l_pivs = ma_pivots(left, mt);
	if (!l_slots[split])
		split++;
	tmp = mas_data_end(&l_mas) - split;

	memcpy(slots, l_slots + split + 1, sizeof(void *) * tmp);
	memcpy(pivs, l_pivs + split + 1, sizeof(unsigned long) * tmp);
	pivs[tmp] = l_mas.max;
	memcpy(slots + tmp, ma_slots(node, mt), sizeof(void *) * end);
	memcpy(pivs + tmp, ma_pivots(node, mt), sizeof(unsigned long) * end);

	l_mas.max = l_pivs[split];
	mas->min = l_mas.max + 1;
	old_eparent = mt_mk_node(mte_parent(l_mas.node),
			     mas_parent_type(&l_mas, l_mas.node));
	tmp += end;
	if (!in_rcu) {
		unsigned char max_p = mt_pivots[mt];
		unsigned char max_s = mt_slots[mt];

		if (tmp < max_p)
			memset(pivs + tmp, 0,
			       sizeof(unsigned long) * (max_p - tmp));

		if (tmp < mt_slots[mt])
			memset(slots + tmp, 0, sizeof(void *) * (max_s - tmp));

		memcpy(node, newnode, sizeof(struct maple_node));
		ma_set_meta(node, mt, 0, tmp - 1);
		mte_set_pivot(old_eparent, mte_parent_slot(l_mas.node),
			      l_pivs[split]);

		 
		tmp = split + 1;
		memset(l_pivs + tmp, 0, sizeof(unsigned long) * (max_p - tmp));
		memset(l_slots + tmp, 0, sizeof(void *) * (max_s - tmp));
		ma_set_meta(left, mt, 0, split);
		eparent = old_eparent;

		goto done;
	}

	 
	mas->node = mt_mk_node(newnode, mt);
	ma_set_meta(newnode, mt, 0, tmp);

	new_left = mas_pop_node(mas);
	new_left->parent = left->parent;
	mt = mte_node_type(l_mas.node);
	slots = ma_slots(new_left, mt);
	pivs = ma_pivots(new_left, mt);
	memcpy(slots, l_slots, sizeof(void *) * split);
	memcpy(pivs, l_pivs, sizeof(unsigned long) * split);
	ma_set_meta(new_left, mt, 0, split);
	l_mas.node = mt_mk_node(new_left, mt);

	 
	offset = mte_parent_slot(mas->node);
	mt = mas_parent_type(&l_mas, l_mas.node);
	parent = mas_pop_node(mas);
	slots = ma_slots(parent, mt);
	pivs = ma_pivots(parent, mt);
	memcpy(parent, mte_to_node(old_eparent), sizeof(struct maple_node));
	rcu_assign_pointer(slots[offset], mas->node);
	rcu_assign_pointer(slots[offset - 1], l_mas.node);
	pivs[offset - 1] = l_mas.max;
	eparent = mt_mk_node(parent, mt);
done:
	gap = mas_leaf_max_gap(mas);
	mte_set_gap(eparent, mte_parent_slot(mas->node), gap);
	gap = mas_leaf_max_gap(&l_mas);
	mte_set_gap(eparent, mte_parent_slot(l_mas.node), gap);
	mas_ascend(mas);

	if (in_rcu) {
		mas_replace_node(mas, old_eparent);
		mas_adopt_children(mas, mas->node);
	}

	mas_update_gap(mas);
}

 
static inline bool mas_split_final_node(struct maple_subtree_state *mast,
					struct ma_state *mas, int height)
{
	struct maple_enode *ancestor;

	if (mte_is_root(mas->node)) {
		if (mt_is_alloc(mas->tree))
			mast->bn->type = maple_arange_64;
		else
			mast->bn->type = maple_range_64;
		mas->depth = height;
	}
	 
	ancestor = mas_new_ma_node(mas, mast->bn);
	mas_set_parent(mas, mast->l->node, ancestor, mast->l->offset);
	mas_set_parent(mas, mast->r->node, ancestor, mast->r->offset);
	mte_to_node(ancestor)->parent = mas_mn(mas)->parent;

	mast->l->node = ancestor;
	mab_mas_cp(mast->bn, 0, mt_slots[mast->bn->type] - 1, mast->l, true);
	mas->offset = mast->bn->b_end - 1;
	return true;
}

 
static inline void mast_fill_bnode(struct maple_subtree_state *mast,
					 struct ma_state *mas,
					 unsigned char skip)
{
	bool cp = true;
	unsigned char split;

	memset(mast->bn->gap, 0, sizeof(unsigned long) * ARRAY_SIZE(mast->bn->gap));
	memset(mast->bn->slot, 0, sizeof(unsigned long) * ARRAY_SIZE(mast->bn->slot));
	memset(mast->bn->pivot, 0, sizeof(unsigned long) * ARRAY_SIZE(mast->bn->pivot));
	mast->bn->b_end = 0;

	if (mte_is_root(mas->node)) {
		cp = false;
	} else {
		mas_ascend(mas);
		mas->offset = mte_parent_slot(mas->node);
	}

	if (cp && mast->l->offset)
		mas_mab_cp(mas, 0, mast->l->offset - 1, mast->bn, 0);

	split = mast->bn->b_end;
	mab_set_b_end(mast->bn, mast->l, mast->l->node);
	mast->r->offset = mast->bn->b_end;
	mab_set_b_end(mast->bn, mast->r, mast->r->node);
	if (mast->bn->pivot[mast->bn->b_end - 1] == mas->max)
		cp = false;

	if (cp)
		mas_mab_cp(mas, split + skip, mt_slot_count(mas->node) - 1,
			   mast->bn, mast->bn->b_end);

	mast->bn->b_end--;
	mast->bn->type = mte_node_type(mas->node);
}

 
static inline void mast_split_data(struct maple_subtree_state *mast,
	   struct ma_state *mas, unsigned char split)
{
	unsigned char p_slot;

	mab_mas_cp(mast->bn, 0, split, mast->l, true);
	mte_set_pivot(mast->r->node, 0, mast->r->max);
	mab_mas_cp(mast->bn, split + 1, mast->bn->b_end, mast->r, false);
	mast->l->offset = mte_parent_slot(mas->node);
	mast->l->max = mast->bn->pivot[split];
	mast->r->min = mast->l->max + 1;
	if (mte_is_leaf(mas->node))
		return;

	p_slot = mast->orig_l->offset;
	mas_set_split_parent(mast->orig_l, mast->l->node, mast->r->node,
			     &p_slot, split);
	mas_set_split_parent(mast->orig_r, mast->l->node, mast->r->node,
			     &p_slot, split);
}

 
static inline bool mas_push_data(struct ma_state *mas, int height,
				 struct maple_subtree_state *mast, bool left)
{
	unsigned char slot_total = mast->bn->b_end;
	unsigned char end, space, split;

	MA_STATE(tmp_mas, mas->tree, mas->index, mas->last);
	tmp_mas = *mas;
	tmp_mas.depth = mast->l->depth;

	if (left && !mas_prev_sibling(&tmp_mas))
		return false;
	else if (!left && !mas_next_sibling(&tmp_mas))
		return false;

	end = mas_data_end(&tmp_mas);
	slot_total += end;
	space = 2 * mt_slot_count(mas->node) - 2;
	 
	if (ma_is_leaf(mast->bn->type))
		space--;

	if (mas->max == ULONG_MAX)
		space--;

	if (slot_total >= space)
		return false;

	 
	mast->bn->b_end++;
	if (left) {
		mab_shift_right(mast->bn, end + 1);
		mas_mab_cp(&tmp_mas, 0, end, mast->bn, 0);
		mast->bn->b_end = slot_total + 1;
	} else {
		mas_mab_cp(&tmp_mas, 0, end, mast->bn, mast->bn->b_end);
	}

	 
	split = mt_slots[mast->bn->type] - 2;
	if (left) {
		 
		*mas = tmp_mas;
		 
		tmp_mas.node = mast->l->node;
		*mast->l = tmp_mas;
	} else {
		tmp_mas.node = mast->r->node;
		*mast->r = tmp_mas;
		split = slot_total - split;
	}
	split = mab_no_null_split(mast->bn, split, mt_slots[mast->bn->type]);
	 
	if (left)
		mast->orig_l->offset += end + 1;

	mast_split_data(mast, mas, split);
	mast_fill_bnode(mast, mas, 2);
	mas_split_final_node(mast, mas, height + 1);
	return true;
}

 
static int mas_split(struct ma_state *mas, struct maple_big_node *b_node)
{
	struct maple_subtree_state mast;
	int height = 0;
	unsigned char mid_split, split = 0;
	struct maple_enode *old;

	 
	MA_STATE(l_mas, mas->tree, mas->index, mas->last);
	MA_STATE(r_mas, mas->tree, mas->index, mas->last);
	MA_STATE(prev_l_mas, mas->tree, mas->index, mas->last);
	MA_STATE(prev_r_mas, mas->tree, mas->index, mas->last);

	trace_ma_op(__func__, mas);
	mas->depth = mas_mt_height(mas);
	 
	mas_node_count(mas, 1 + mas->depth * 2);
	if (mas_is_err(mas))
		return 0;

	mast.l = &l_mas;
	mast.r = &r_mas;
	mast.orig_l = &prev_l_mas;
	mast.orig_r = &prev_r_mas;
	mast.bn = b_node;

	while (height++ <= mas->depth) {
		if (mt_slots[b_node->type] > b_node->b_end) {
			mas_split_final_node(&mast, mas, height);
			break;
		}

		l_mas = r_mas = *mas;
		l_mas.node = mas_new_ma_node(mas, b_node);
		r_mas.node = mas_new_ma_node(mas, b_node);
		 
		 
		if (mas_push_data(mas, height, &mast, true))
			break;

		 
		if (mas_push_data(mas, height, &mast, false))
			break;

		split = mab_calc_split(mas, b_node, &mid_split, prev_l_mas.min);
		mast_split_data(&mast, mas, split);
		 
		mast.r->max = mas->max;
		mast_fill_bnode(&mast, mas, 1);
		prev_l_mas = *mast.l;
		prev_r_mas = *mast.r;
	}

	 
	old = mas->node;
	mas->node = l_mas.node;
	mas_wmb_replace(mas, old);
	mtree_range_walk(mas);
	return 1;
}

 
static inline bool mas_reuse_node(struct ma_wr_state *wr_mas,
			  struct maple_big_node *bn, unsigned char end)
{
	 
	if (mt_in_rcu(wr_mas->mas->tree))
		return false;

	if (end > bn->b_end) {
		int clear = mt_slots[wr_mas->type] - bn->b_end;

		memset(wr_mas->slots + bn->b_end, 0, sizeof(void *) * clear--);
		memset(wr_mas->pivots + bn->b_end, 0, sizeof(void *) * clear);
	}
	mab_mas_cp(bn, 0, bn->b_end, wr_mas->mas, false);
	return true;
}

 
static noinline_for_kasan int mas_commit_b_node(struct ma_wr_state *wr_mas,
			    struct maple_big_node *b_node, unsigned char end)
{
	struct maple_node *node;
	struct maple_enode *old_enode;
	unsigned char b_end = b_node->b_end;
	enum maple_type b_type = b_node->type;

	old_enode = wr_mas->mas->node;
	if ((b_end < mt_min_slots[b_type]) &&
	    (!mte_is_root(old_enode)) &&
	    (mas_mt_height(wr_mas->mas) > 1))
		return mas_rebalance(wr_mas->mas, b_node);

	if (b_end >= mt_slots[b_type])
		return mas_split(wr_mas->mas, b_node);

	if (mas_reuse_node(wr_mas, b_node, end))
		goto reuse_node;

	mas_node_count(wr_mas->mas, 1);
	if (mas_is_err(wr_mas->mas))
		return 0;

	node = mas_pop_node(wr_mas->mas);
	node->parent = mas_mn(wr_mas->mas)->parent;
	wr_mas->mas->node = mt_mk_node(node, b_type);
	mab_mas_cp(b_node, 0, b_end, wr_mas->mas, false);
	mas_replace_node(wr_mas->mas, old_enode);
reuse_node:
	mas_update_gap(wr_mas->mas);
	return 1;
}

 
static inline int mas_root_expand(struct ma_state *mas, void *entry)
{
	void *contents = mas_root_locked(mas);
	enum maple_type type = maple_leaf_64;
	struct maple_node *node;
	void __rcu **slots;
	unsigned long *pivots;
	int slot = 0;

	mas_node_count(mas, 1);
	if (unlikely(mas_is_err(mas)))
		return 0;

	node = mas_pop_node(mas);
	pivots = ma_pivots(node, type);
	slots = ma_slots(node, type);
	node->parent = ma_parent_ptr(mas_tree_parent(mas));
	mas->node = mt_mk_node(node, type);

	if (mas->index) {
		if (contents) {
			rcu_assign_pointer(slots[slot], contents);
			if (likely(mas->index > 1))
				slot++;
		}
		pivots[slot++] = mas->index - 1;
	}

	rcu_assign_pointer(slots[slot], entry);
	mas->offset = slot;
	pivots[slot] = mas->last;
	if (mas->last != ULONG_MAX)
		pivots[++slot] = ULONG_MAX;

	mas->depth = 1;
	mas_set_height(mas);
	ma_set_meta(node, maple_leaf_64, 0, slot);
	 
	rcu_assign_pointer(mas->tree->ma_root, mte_mk_root(mas->node));
	return slot;
}

static inline void mas_store_root(struct ma_state *mas, void *entry)
{
	if (likely((mas->last != 0) || (mas->index != 0)))
		mas_root_expand(mas, entry);
	else if (((unsigned long) (entry) & 3) == 2)
		mas_root_expand(mas, entry);
	else {
		rcu_assign_pointer(mas->tree->ma_root, entry);
		mas->node = MAS_START;
	}
}

 
static bool mas_is_span_wr(struct ma_wr_state *wr_mas)
{
	unsigned long max = wr_mas->r_max;
	unsigned long last = wr_mas->mas->last;
	enum maple_type type = wr_mas->type;
	void *entry = wr_mas->entry;

	 
	if (last < max)
		return false;

	if (ma_is_leaf(type)) {
		max = wr_mas->mas->max;
		if (last < max)
			return false;
	}

	if (last == max) {
		 
		if (entry || last == ULONG_MAX)
			return false;
	}

	trace_ma_write(__func__, wr_mas->mas, wr_mas->r_max, entry);
	return true;
}

static inline void mas_wr_walk_descend(struct ma_wr_state *wr_mas)
{
	wr_mas->type = mte_node_type(wr_mas->mas->node);
	mas_wr_node_walk(wr_mas);
	wr_mas->slots = ma_slots(wr_mas->node, wr_mas->type);
}

static inline void mas_wr_walk_traverse(struct ma_wr_state *wr_mas)
{
	wr_mas->mas->max = wr_mas->r_max;
	wr_mas->mas->min = wr_mas->r_min;
	wr_mas->mas->node = wr_mas->content;
	wr_mas->mas->offset = 0;
	wr_mas->mas->depth++;
}
 
static bool mas_wr_walk(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;

	while (true) {
		mas_wr_walk_descend(wr_mas);
		if (unlikely(mas_is_span_wr(wr_mas)))
			return false;

		wr_mas->content = mas_slot_locked(mas, wr_mas->slots,
						  mas->offset);
		if (ma_is_leaf(wr_mas->type))
			return true;

		mas_wr_walk_traverse(wr_mas);
	}

	return true;
}

static bool mas_wr_walk_index(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;

	while (true) {
		mas_wr_walk_descend(wr_mas);
		wr_mas->content = mas_slot_locked(mas, wr_mas->slots,
						  mas->offset);
		if (ma_is_leaf(wr_mas->type))
			return true;
		mas_wr_walk_traverse(wr_mas);

	}
	return true;
}
 
static inline void mas_extend_spanning_null(struct ma_wr_state *l_wr_mas,
					    struct ma_wr_state *r_wr_mas)
{
	struct ma_state *r_mas = r_wr_mas->mas;
	struct ma_state *l_mas = l_wr_mas->mas;
	unsigned char l_slot;

	l_slot = l_mas->offset;
	if (!l_wr_mas->content)
		l_mas->index = l_wr_mas->r_min;

	if ((l_mas->index == l_wr_mas->r_min) &&
		 (l_slot &&
		  !mas_slot_locked(l_mas, l_wr_mas->slots, l_slot - 1))) {
		if (l_slot > 1)
			l_mas->index = l_wr_mas->pivots[l_slot - 2] + 1;
		else
			l_mas->index = l_mas->min;

		l_mas->offset = l_slot - 1;
	}

	if (!r_wr_mas->content) {
		if (r_mas->last < r_wr_mas->r_max)
			r_mas->last = r_wr_mas->r_max;
		r_mas->offset++;
	} else if ((r_mas->last == r_wr_mas->r_max) &&
	    (r_mas->last < r_mas->max) &&
	    !mas_slot_locked(r_mas, r_wr_mas->slots, r_mas->offset + 1)) {
		r_mas->last = mas_safe_pivot(r_mas, r_wr_mas->pivots,
					     r_wr_mas->type, r_mas->offset + 1);
		r_mas->offset++;
	}
}

static inline void *mas_state_walk(struct ma_state *mas)
{
	void *entry;

	entry = mas_start(mas);
	if (mas_is_none(mas))
		return NULL;

	if (mas_is_ptr(mas))
		return entry;

	return mtree_range_walk(mas);
}

 
static inline void *mtree_lookup_walk(struct ma_state *mas)
{
	unsigned long *pivots;
	unsigned char offset;
	struct maple_node *node;
	struct maple_enode *next;
	enum maple_type type;
	void __rcu **slots;
	unsigned char end;
	unsigned long max;

	next = mas->node;
	max = ULONG_MAX;
	do {
		offset = 0;
		node = mte_to_node(next);
		type = mte_node_type(next);
		pivots = ma_pivots(node, type);
		end = ma_data_end(node, type, pivots, max);
		if (unlikely(ma_dead_node(node)))
			goto dead_node;
		do {
			if (pivots[offset] >= mas->index) {
				max = pivots[offset];
				break;
			}
		} while (++offset < end);

		slots = ma_slots(node, type);
		next = mt_slot(mas->tree, slots, offset);
		if (unlikely(ma_dead_node(node)))
			goto dead_node;
	} while (!ma_is_leaf(type));

	return (void *)next;

dead_node:
	mas_reset(mas);
	return NULL;
}

static void mte_destroy_walk(struct maple_enode *, struct maple_tree *);
 
static inline int mas_new_root(struct ma_state *mas, void *entry)
{
	struct maple_enode *root = mas_root_locked(mas);
	enum maple_type type = maple_leaf_64;
	struct maple_node *node;
	void __rcu **slots;
	unsigned long *pivots;

	if (!entry && !mas->index && mas->last == ULONG_MAX) {
		mas->depth = 0;
		mas_set_height(mas);
		rcu_assign_pointer(mas->tree->ma_root, entry);
		mas->node = MAS_START;
		goto done;
	}

	mas_node_count(mas, 1);
	if (mas_is_err(mas))
		return 0;

	node = mas_pop_node(mas);
	pivots = ma_pivots(node, type);
	slots = ma_slots(node, type);
	node->parent = ma_parent_ptr(mas_tree_parent(mas));
	mas->node = mt_mk_node(node, type);
	rcu_assign_pointer(slots[0], entry);
	pivots[0] = mas->last;
	mas->depth = 1;
	mas_set_height(mas);
	rcu_assign_pointer(mas->tree->ma_root, mte_mk_root(mas->node));

done:
	if (xa_is_node(root))
		mte_destroy_walk(root, mas->tree);

	return 1;
}
 
static inline int mas_wr_spanning_store(struct ma_wr_state *wr_mas)
{
	struct maple_subtree_state mast;
	struct maple_big_node b_node;
	struct ma_state *mas;
	unsigned char height;

	 
	MA_STATE(l_mas, NULL, 0, 0);
	MA_STATE(r_mas, NULL, 0, 0);
	MA_WR_STATE(r_wr_mas, &r_mas, wr_mas->entry);
	MA_WR_STATE(l_wr_mas, &l_mas, wr_mas->entry);

	 
	mas = wr_mas->mas;
	trace_ma_op(__func__, mas);

	if (unlikely(!mas->index && mas->last == ULONG_MAX))
		return mas_new_root(mas, wr_mas->entry);
	 
	height = mas_mt_height(mas);
	mas_node_count(mas, 1 + height * 3);
	if (mas_is_err(mas))
		return 0;

	 
	r_mas = *mas;
	 
	if (r_mas.last + 1)
		r_mas.last++;

	r_mas.index = r_mas.last;
	mas_wr_walk_index(&r_wr_mas);
	r_mas.last = r_mas.index = mas->last;

	 
	l_mas = *mas;
	mas_wr_walk_index(&l_wr_mas);

	if (!wr_mas->entry) {
		mas_extend_spanning_null(&l_wr_mas, &r_wr_mas);
		mas->offset = l_mas.offset;
		mas->index = l_mas.index;
		mas->last = l_mas.last = r_mas.last;
	}

	 
	if (!l_mas.index && r_mas.last == ULONG_MAX) {
		mas_set_range(mas, 0, ULONG_MAX);
		return mas_new_root(mas, wr_mas->entry);
	}

	memset(&b_node, 0, sizeof(struct maple_big_node));
	 
	mas_store_b_node(&l_wr_mas, &b_node, l_wr_mas.node_end);
	 
	if (r_mas.offset <= r_wr_mas.node_end)
		mas_mab_cp(&r_mas, r_mas.offset, r_wr_mas.node_end,
			   &b_node, b_node.b_end + 1);
	else
		b_node.b_end++;

	 
	l_mas.index = l_mas.last = mas->index;

	mast.bn = &b_node;
	mast.orig_l = &l_mas;
	mast.orig_r = &r_mas;
	 
	return mas_spanning_rebalance(mas, &mast, height + 1);
}

 
static inline bool mas_wr_node_store(struct ma_wr_state *wr_mas,
				     unsigned char new_end)
{
	struct ma_state *mas = wr_mas->mas;
	void __rcu **dst_slots;
	unsigned long *dst_pivots;
	unsigned char dst_offset, offset_end = wr_mas->offset_end;
	struct maple_node reuse, *newnode;
	unsigned char copy_size, node_pivots = mt_pivots[wr_mas->type];
	bool in_rcu = mt_in_rcu(mas->tree);

	 
	if (!mte_is_root(mas->node) && (new_end <= mt_min_slots[wr_mas->type]) &&
	    !(mas->mas_flags & MA_STATE_BULK))
		return false;

	if (mas->last == wr_mas->end_piv)
		offset_end++;  
	else if (unlikely(wr_mas->r_max == ULONG_MAX))
		mas_bulk_rebalance(mas, wr_mas->node_end, wr_mas->type);

	 
	if (in_rcu) {
		mas_node_count(mas, 1);
		if (mas_is_err(mas))
			return false;

		newnode = mas_pop_node(mas);
	} else {
		memset(&reuse, 0, sizeof(struct maple_node));
		newnode = &reuse;
	}

	newnode->parent = mas_mn(mas)->parent;
	dst_pivots = ma_pivots(newnode, wr_mas->type);
	dst_slots = ma_slots(newnode, wr_mas->type);
	 
	memcpy(dst_pivots, wr_mas->pivots, sizeof(unsigned long) * mas->offset);
	memcpy(dst_slots, wr_mas->slots, sizeof(void *) * mas->offset);

	 
	if (wr_mas->r_min < mas->index) {
		rcu_assign_pointer(dst_slots[mas->offset], wr_mas->content);
		dst_pivots[mas->offset++] = mas->index - 1;
	}

	 
	if (mas->offset < node_pivots)
		dst_pivots[mas->offset] = mas->last;
	rcu_assign_pointer(dst_slots[mas->offset], wr_mas->entry);

	 
	if (offset_end > wr_mas->node_end)
		goto done;

	dst_offset = mas->offset + 1;
	 
	copy_size = wr_mas->node_end - offset_end + 1;
	memcpy(dst_slots + dst_offset, wr_mas->slots + offset_end,
	       sizeof(void *) * copy_size);
	memcpy(dst_pivots + dst_offset, wr_mas->pivots + offset_end,
	       sizeof(unsigned long) * (copy_size - 1));

	if (new_end < node_pivots)
		dst_pivots[new_end] = mas->max;

done:
	mas_leaf_set_meta(mas, newnode, dst_pivots, maple_leaf_64, new_end);
	if (in_rcu) {
		struct maple_enode *old_enode = mas->node;

		mas->node = mt_mk_node(newnode, wr_mas->type);
		mas_replace_node(mas, old_enode);
	} else {
		memcpy(wr_mas->node, newnode, sizeof(struct maple_node));
	}
	trace_ma_write(__func__, mas, 0, wr_mas->entry);
	mas_update_gap(mas);
	return true;
}

 
static inline bool mas_wr_slot_store(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;
	unsigned char offset = mas->offset;
	void __rcu **slots = wr_mas->slots;
	bool gap = false;

	gap |= !mt_slot_locked(mas->tree, slots, offset);
	gap |= !mt_slot_locked(mas->tree, slots, offset + 1);

	if (wr_mas->offset_end - offset == 1) {
		if (mas->index == wr_mas->r_min) {
			 
			rcu_assign_pointer(slots[offset], wr_mas->entry);
			wr_mas->pivots[offset] = mas->last;
		} else {
			 
			rcu_assign_pointer(slots[offset + 1], wr_mas->entry);
			wr_mas->pivots[offset] = mas->index - 1;
			mas->offset++;  
		}
	} else if (!mt_in_rcu(mas->tree)) {
		 
		gap |= !mt_slot_locked(mas->tree, slots, offset + 2);
		rcu_assign_pointer(slots[offset + 1], wr_mas->entry);
		wr_mas->pivots[offset] = mas->index - 1;
		wr_mas->pivots[offset + 1] = mas->last;
		mas->offset++;  
	} else {
		return false;
	}

	trace_ma_write(__func__, mas, 0, wr_mas->entry);
	 
	if (!wr_mas->entry || gap)
		mas_update_gap(mas);

	return true;
}

static inline void mas_wr_extend_null(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;

	if (!wr_mas->slots[wr_mas->offset_end]) {
		 
		mas->last = wr_mas->end_piv;
	} else {
		 
		if ((mas->last == wr_mas->end_piv) &&
		    (wr_mas->node_end != wr_mas->offset_end) &&
		    !wr_mas->slots[wr_mas->offset_end + 1]) {
			wr_mas->offset_end++;
			if (wr_mas->offset_end == wr_mas->node_end)
				mas->last = mas->max;
			else
				mas->last = wr_mas->pivots[wr_mas->offset_end];
			wr_mas->end_piv = mas->last;
		}
	}

	if (!wr_mas->content) {
		 
		mas->index = wr_mas->r_min;
	} else {
		 
		if (mas->index == wr_mas->r_min && mas->offset &&
		    !wr_mas->slots[mas->offset - 1]) {
			mas->offset--;
			wr_mas->r_min = mas->index =
				mas_safe_min(mas, wr_mas->pivots, mas->offset);
			wr_mas->r_max = wr_mas->pivots[mas->offset];
		}
	}
}

static inline void mas_wr_end_piv(struct ma_wr_state *wr_mas)
{
	while ((wr_mas->offset_end < wr_mas->node_end) &&
	       (wr_mas->mas->last > wr_mas->pivots[wr_mas->offset_end]))
		wr_mas->offset_end++;

	if (wr_mas->offset_end < wr_mas->node_end)
		wr_mas->end_piv = wr_mas->pivots[wr_mas->offset_end];
	else
		wr_mas->end_piv = wr_mas->mas->max;

	if (!wr_mas->entry)
		mas_wr_extend_null(wr_mas);
}

static inline unsigned char mas_wr_new_end(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;
	unsigned char new_end = wr_mas->node_end + 2;

	new_end -= wr_mas->offset_end - mas->offset;
	if (wr_mas->r_min == mas->index)
		new_end--;

	if (wr_mas->end_piv == mas->last)
		new_end--;

	return new_end;
}

 
static inline bool mas_wr_append(struct ma_wr_state *wr_mas,
		unsigned char new_end)
{
	struct ma_state *mas;
	void __rcu **slots;
	unsigned char end;

	mas = wr_mas->mas;
	if (mt_in_rcu(mas->tree))
		return false;

	if (mas->offset != wr_mas->node_end)
		return false;

	end = wr_mas->node_end;
	if (mas->offset != end)
		return false;

	if (new_end < mt_pivots[wr_mas->type]) {
		wr_mas->pivots[new_end] = wr_mas->pivots[end];
		ma_set_meta(wr_mas->node, wr_mas->type, 0, new_end);
	}

	slots = wr_mas->slots;
	if (new_end == end + 1) {
		if (mas->last == wr_mas->r_max) {
			 
			rcu_assign_pointer(slots[new_end], wr_mas->entry);
			wr_mas->pivots[end] = mas->index - 1;
			mas->offset = new_end;
		} else {
			 
			rcu_assign_pointer(slots[new_end], wr_mas->content);
			wr_mas->pivots[end] = mas->last;
			rcu_assign_pointer(slots[end], wr_mas->entry);
		}
	} else {
		 
		rcu_assign_pointer(slots[new_end], wr_mas->content);
		wr_mas->pivots[end + 1] = mas->last;
		rcu_assign_pointer(slots[end + 1], wr_mas->entry);
		wr_mas->pivots[end] = mas->index - 1;
		mas->offset = end + 1;
	}

	if (!wr_mas->content || !wr_mas->entry)
		mas_update_gap(mas);

	trace_ma_write(__func__, mas, new_end, wr_mas->entry);
	return  true;
}

 
static void mas_wr_bnode(struct ma_wr_state *wr_mas)
{
	struct maple_big_node b_node;

	trace_ma_write(__func__, wr_mas->mas, 0, wr_mas->entry);
	memset(&b_node, 0, sizeof(struct maple_big_node));
	mas_store_b_node(wr_mas, &b_node, wr_mas->offset_end);
	mas_commit_b_node(wr_mas, &b_node, wr_mas->node_end);
}

static inline void mas_wr_modify(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;
	unsigned char new_end;

	 
	if (wr_mas->r_min == mas->index && wr_mas->r_max == mas->last) {
		rcu_assign_pointer(wr_mas->slots[mas->offset], wr_mas->entry);
		if (!!wr_mas->entry ^ !!wr_mas->content)
			mas_update_gap(mas);
		return;
	}

	 
	new_end = mas_wr_new_end(wr_mas);
	if (new_end >= mt_slots[wr_mas->type])
		goto slow_path;

	 
	if (mas_wr_append(wr_mas, new_end))
		return;

	if (new_end == wr_mas->node_end && mas_wr_slot_store(wr_mas))
		return;

	if (mas_wr_node_store(wr_mas, new_end))
		return;

	if (mas_is_err(mas))
		return;

slow_path:
	mas_wr_bnode(wr_mas);
}

 
static inline void *mas_wr_store_entry(struct ma_wr_state *wr_mas)
{
	struct ma_state *mas = wr_mas->mas;

	wr_mas->content = mas_start(mas);
	if (mas_is_none(mas) || mas_is_ptr(mas)) {
		mas_store_root(mas, wr_mas->entry);
		return wr_mas->content;
	}

	if (unlikely(!mas_wr_walk(wr_mas))) {
		mas_wr_spanning_store(wr_mas);
		return wr_mas->content;
	}

	 
	mas_wr_end_piv(wr_mas);
	 
	if (unlikely(!mas->index && mas->last == ULONG_MAX)) {
		mas_new_root(mas, wr_mas->entry);
		return wr_mas->content;
	}

	mas_wr_modify(wr_mas);
	return wr_mas->content;
}

 
static inline void *mas_insert(struct ma_state *mas, void *entry)
{
	MA_WR_STATE(wr_mas, mas, entry);

	 
	wr_mas.content = mas_start(mas);
	if (wr_mas.content)
		goto exists;

	if (mas_is_none(mas) || mas_is_ptr(mas)) {
		mas_store_root(mas, entry);
		return NULL;
	}

	 
	if (!mas_wr_walk(&wr_mas))
		goto exists;

	 
	wr_mas.offset_end = mas->offset;
	wr_mas.end_piv = wr_mas.r_max;

	if (wr_mas.content || (mas->last > wr_mas.r_max))
		goto exists;

	if (!entry)
		return NULL;

	mas_wr_modify(&wr_mas);
	return wr_mas.content;

exists:
	mas_set_err(mas, -EEXIST);
	return wr_mas.content;

}

static inline void mas_rewalk(struct ma_state *mas, unsigned long index)
{
retry:
	mas_set(mas, index);
	mas_state_walk(mas);
	if (mas_is_start(mas))
		goto retry;
}

static inline bool mas_rewalk_if_dead(struct ma_state *mas,
		struct maple_node *node, const unsigned long index)
{
	if (unlikely(ma_dead_node(node))) {
		mas_rewalk(mas, index);
		return true;
	}
	return false;
}

 
static inline int mas_prev_node(struct ma_state *mas, unsigned long min)
{
	enum maple_type mt;
	int offset, level;
	void __rcu **slots;
	struct maple_node *node;
	unsigned long *pivots;
	unsigned long max;

	node = mas_mn(mas);
	if (!mas->min)
		goto no_entry;

	max = mas->min - 1;
	if (max < min)
		goto no_entry;

	level = 0;
	do {
		if (ma_is_root(node))
			goto no_entry;

		 
		if (unlikely(mas_ascend(mas)))
			return 1;
		offset = mas->offset;
		level++;
		node = mas_mn(mas);
	} while (!offset);

	offset--;
	mt = mte_node_type(mas->node);
	while (level > 1) {
		level--;
		slots = ma_slots(node, mt);
		mas->node = mas_slot(mas, slots, offset);
		if (unlikely(ma_dead_node(node)))
			return 1;

		mt = mte_node_type(mas->node);
		node = mas_mn(mas);
		pivots = ma_pivots(node, mt);
		offset = ma_data_end(node, mt, pivots, max);
		if (unlikely(ma_dead_node(node)))
			return 1;
	}

	slots = ma_slots(node, mt);
	mas->node = mas_slot(mas, slots, offset);
	pivots = ma_pivots(node, mt);
	if (unlikely(ma_dead_node(node)))
		return 1;

	if (likely(offset))
		mas->min = pivots[offset - 1] + 1;
	mas->max = max;
	mas->offset = mas_data_end(mas);
	if (unlikely(mte_dead_node(mas->node)))
		return 1;

	return 0;

no_entry:
	if (unlikely(ma_dead_node(node)))
		return 1;

	mas->node = MAS_NONE;
	return 0;
}

 
static void *mas_prev_slot(struct ma_state *mas, unsigned long min, bool empty,
			   bool set_underflow)
{
	void *entry;
	void __rcu **slots;
	unsigned long pivot;
	enum maple_type type;
	unsigned long *pivots;
	struct maple_node *node;
	unsigned long save_point = mas->index;

retry:
	node = mas_mn(mas);
	type = mte_node_type(mas->node);
	pivots = ma_pivots(node, type);
	if (unlikely(mas_rewalk_if_dead(mas, node, save_point)))
		goto retry;

	if (mas->min <= min) {
		pivot = mas_safe_min(mas, pivots, mas->offset);

		if (unlikely(mas_rewalk_if_dead(mas, node, save_point)))
			goto retry;

		if (pivot <= min)
			goto underflow;
	}

again:
	if (likely(mas->offset)) {
		mas->offset--;
		mas->last = mas->index - 1;
		mas->index = mas_safe_min(mas, pivots, mas->offset);
	} else  {
		if (mas_prev_node(mas, min)) {
			mas_rewalk(mas, save_point);
			goto retry;
		}

		if (mas_is_none(mas))
			goto underflow;

		mas->last = mas->max;
		node = mas_mn(mas);
		type = mte_node_type(mas->node);
		pivots = ma_pivots(node, type);
		mas->index = pivots[mas->offset - 1] + 1;
	}

	slots = ma_slots(node, type);
	entry = mas_slot(mas, slots, mas->offset);
	if (unlikely(mas_rewalk_if_dead(mas, node, save_point)))
		goto retry;

	if (likely(entry))
		return entry;

	if (!empty) {
		if (mas->index <= min)
			goto underflow;

		goto again;
	}

	return entry;

underflow:
	if (set_underflow)
		mas->node = MAS_UNDERFLOW;
	return NULL;
}

 
static inline int mas_next_node(struct ma_state *mas, struct maple_node *node,
				unsigned long max)
{
	unsigned long min;
	unsigned long *pivots;
	struct maple_enode *enode;
	int level = 0;
	unsigned char node_end;
	enum maple_type mt;
	void __rcu **slots;

	if (mas->max >= max)
		goto no_entry;

	min = mas->max + 1;
	level = 0;
	do {
		if (ma_is_root(node))
			goto no_entry;

		 
		if (unlikely(mas_ascend(mas)))
			return 1;

		level++;
		node = mas_mn(mas);
		mt = mte_node_type(mas->node);
		pivots = ma_pivots(node, mt);
		node_end = ma_data_end(node, mt, pivots, mas->max);
		if (unlikely(ma_dead_node(node)))
			return 1;

	} while (unlikely(mas->offset == node_end));

	slots = ma_slots(node, mt);
	mas->offset++;
	enode = mas_slot(mas, slots, mas->offset);
	if (unlikely(ma_dead_node(node)))
		return 1;

	if (level > 1)
		mas->offset = 0;

	while (unlikely(level > 1)) {
		level--;
		mas->node = enode;
		node = mas_mn(mas);
		mt = mte_node_type(mas->node);
		slots = ma_slots(node, mt);
		enode = mas_slot(mas, slots, 0);
		if (unlikely(ma_dead_node(node)))
			return 1;
	}

	if (!mas->offset)
		pivots = ma_pivots(node, mt);

	mas->max = mas_safe_pivot(mas, pivots, mas->offset, mt);
	if (unlikely(ma_dead_node(node)))
		return 1;

	mas->node = enode;
	mas->min = min;
	return 0;

no_entry:
	if (unlikely(ma_dead_node(node)))
		return 1;

	mas->node = MAS_NONE;
	return 0;
}

 
static void *mas_next_slot(struct ma_state *mas, unsigned long max, bool empty,
			   bool set_overflow)
{
	void __rcu **slots;
	unsigned long *pivots;
	unsigned long pivot;
	enum maple_type type;
	struct maple_node *node;
	unsigned char data_end;
	unsigned long save_point = mas->last;
	void *entry;

retry:
	node = mas_mn(mas);
	type = mte_node_type(mas->node);
	pivots = ma_pivots(node, type);
	data_end = ma_data_end(node, type, pivots, mas->max);
	if (unlikely(mas_rewalk_if_dead(mas, node, save_point)))
		goto retry;

	if (mas->max >= max) {
		if (likely(mas->offset < data_end))
			pivot = pivots[mas->offset];
		else
			goto overflow;

		if (unlikely(mas_rewalk_if_dead(mas, node, save_point)))
			goto retry;

		if (pivot >= max)
			goto overflow;
	}

	if (likely(mas->offset < data_end)) {
		mas->index = pivots[mas->offset] + 1;
again:
		mas->offset++;
		if (likely(mas->offset < data_end))
			mas->last = pivots[mas->offset];
		else
			mas->last = mas->max;
	} else  {
		if (mas_next_node(mas, node, max)) {
			mas_rewalk(mas, save_point);
			goto retry;
		}

		if (WARN_ON_ONCE(mas_is_none(mas))) {
			mas->node = MAS_OVERFLOW;
			return NULL;
			goto overflow;
		}

		mas->offset = 0;
		mas->index = mas->min;
		node = mas_mn(mas);
		type = mte_node_type(mas->node);
		pivots = ma_pivots(node, type);
		mas->last = pivots[0];
	}

	slots = ma_slots(node, type);
	entry = mt_slot(mas->tree, slots, mas->offset);
	if (unlikely(mas_rewalk_if_dead(mas, node, save_point)))
		goto retry;

	if (entry)
		return entry;

	if (!empty) {
		if (mas->last >= max)
			goto overflow;

		mas->index = mas->last + 1;
		 
		goto again;
	}

	return entry;

overflow:
	if (set_overflow)
		mas->node = MAS_OVERFLOW;
	return NULL;
}

 
static inline void *mas_next_entry(struct ma_state *mas, unsigned long limit)
{
	if (mas->last >= limit) {
		mas->node = MAS_OVERFLOW;
		return NULL;
	}

	return mas_next_slot(mas, limit, false, true);
}

 
static bool mas_rev_awalk(struct ma_state *mas, unsigned long size,
		unsigned long *gap_min, unsigned long *gap_max)
{
	enum maple_type type = mte_node_type(mas->node);
	struct maple_node *node = mas_mn(mas);
	unsigned long *pivots, *gaps;
	void __rcu **slots;
	unsigned long gap = 0;
	unsigned long max, min;
	unsigned char offset;

	if (unlikely(mas_is_err(mas)))
		return true;

	if (ma_is_dense(type)) {
		 
		mas->offset = (unsigned char)(mas->index - mas->min);
		return true;
	}

	pivots = ma_pivots(node, type);
	slots = ma_slots(node, type);
	gaps = ma_gaps(node, type);
	offset = mas->offset;
	min = mas_safe_min(mas, pivots, offset);
	 
	while (mas->last < min)
		min = mas_safe_min(mas, pivots, --offset);

	max = mas_safe_pivot(mas, pivots, offset, type);
	while (mas->index <= max) {
		gap = 0;
		if (gaps)
			gap = gaps[offset];
		else if (!mas_slot(mas, slots, offset))
			gap = max - min + 1;

		if (gap) {
			if ((size <= gap) && (size <= mas->last - min + 1))
				break;

			if (!gaps) {
				 
				if (offset < 2)
					goto ascend;

				offset -= 2;
				max = pivots[offset];
				min = mas_safe_min(mas, pivots, offset);
				continue;
			}
		}

		if (!offset)
			goto ascend;

		offset--;
		max = min - 1;
		min = mas_safe_min(mas, pivots, offset);
	}

	if (unlikely((mas->index > max) || (size - 1 > max - mas->index)))
		goto no_space;

	if (unlikely(ma_is_leaf(type))) {
		mas->offset = offset;
		*gap_min = min;
		*gap_max = min + gap - 1;
		return true;
	}

	 
	mas->node = mas_slot(mas, slots, offset);
	mas->min = min;
	mas->max = max;
	mas->offset = mas_data_end(mas);
	return false;

ascend:
	if (!mte_is_root(mas->node))
		return false;

no_space:
	mas_set_err(mas, -EBUSY);
	return false;
}

static inline bool mas_anode_descend(struct ma_state *mas, unsigned long size)
{
	enum maple_type type = mte_node_type(mas->node);
	unsigned long pivot, min, gap = 0;
	unsigned char offset, data_end;
	unsigned long *gaps, *pivots;
	void __rcu **slots;
	struct maple_node *node;
	bool found = false;

	if (ma_is_dense(type)) {
		mas->offset = (unsigned char)(mas->index - mas->min);
		return true;
	}

	node = mas_mn(mas);
	pivots = ma_pivots(node, type);
	slots = ma_slots(node, type);
	gaps = ma_gaps(node, type);
	offset = mas->offset;
	min = mas_safe_min(mas, pivots, offset);
	data_end = ma_data_end(node, type, pivots, mas->max);
	for (; offset <= data_end; offset++) {
		pivot = mas_safe_pivot(mas, pivots, offset, type);

		 
		if (mas->index > pivot)
			goto next_slot;

		if (gaps)
			gap = gaps[offset];
		else if (!mas_slot(mas, slots, offset))
			gap = min(pivot, mas->last) - max(mas->index, min) + 1;
		else
			goto next_slot;

		if (gap >= size) {
			if (ma_is_leaf(type)) {
				found = true;
				goto done;
			}
			if (mas->index <= pivot) {
				mas->node = mas_slot(mas, slots, offset);
				mas->min = min;
				mas->max = pivot;
				offset = 0;
				break;
			}
		}
next_slot:
		min = pivot + 1;
		if (mas->last <= pivot) {
			mas_set_err(mas, -EBUSY);
			return true;
		}
	}

	if (mte_is_root(mas->node))
		found = true;
done:
	mas->offset = offset;
	return found;
}

 
void *mas_walk(struct ma_state *mas)
{
	void *entry;

	if (!mas_is_active(mas) || !mas_is_start(mas))
		mas->node = MAS_START;
retry:
	entry = mas_state_walk(mas);
	if (mas_is_start(mas)) {
		goto retry;
	} else if (mas_is_none(mas)) {
		mas->index = 0;
		mas->last = ULONG_MAX;
	} else if (mas_is_ptr(mas)) {
		if (!mas->index) {
			mas->last = 0;
			return entry;
		}

		mas->index = 1;
		mas->last = ULONG_MAX;
		mas->node = MAS_NONE;
		return NULL;
	}

	return entry;
}
EXPORT_SYMBOL_GPL(mas_walk);

static inline bool mas_rewind_node(struct ma_state *mas)
{
	unsigned char slot;

	do {
		if (mte_is_root(mas->node)) {
			slot = mas->offset;
			if (!slot)
				return false;
		} else {
			mas_ascend(mas);
			slot = mas->offset;
		}
	} while (!slot);

	mas->offset = --slot;
	return true;
}

 
static inline bool mas_skip_node(struct ma_state *mas)
{
	if (mas_is_err(mas))
		return false;

	do {
		if (mte_is_root(mas->node)) {
			if (mas->offset >= mas_data_end(mas)) {
				mas_set_err(mas, -EBUSY);
				return false;
			}
		} else {
			mas_ascend(mas);
		}
	} while (mas->offset >= mas_data_end(mas));

	mas->offset++;
	return true;
}

 
static inline void mas_awalk(struct ma_state *mas, unsigned long size)
{
	struct maple_enode *last = NULL;

	 
	while (!mas_is_err(mas) && !mas_anode_descend(mas, size)) {
		if (last == mas->node)
			mas_skip_node(mas);
		else
			last = mas->node;
	}
}

 
static inline int mas_sparse_area(struct ma_state *mas, unsigned long min,
				unsigned long max, unsigned long size, bool fwd)
{
	if (!unlikely(mas_is_none(mas)) && min == 0) {
		min++;
		 
		if (min > max || max - min + 1 < size)
			return -EBUSY;
	}
	 

	if (fwd) {
		mas->index = min;
		mas->last = min + size - 1;
	} else {
		mas->last = max;
		mas->index = max - size + 1;
	}
	return 0;
}

 
int mas_empty_area(struct ma_state *mas, unsigned long min,
		unsigned long max, unsigned long size)
{
	unsigned char offset;
	unsigned long *pivots;
	enum maple_type mt;

	if (min > max)
		return -EINVAL;

	if (size == 0 || max - min < size - 1)
		return -EINVAL;

	if (mas_is_start(mas))
		mas_start(mas);
	else if (mas->offset >= 2)
		mas->offset -= 2;
	else if (!mas_skip_node(mas))
		return -EBUSY;

	 
	if (mas_is_none(mas) || mas_is_ptr(mas))
		return mas_sparse_area(mas, min, max, size, true);

	 
	mas->index = min;
	mas->last = max;
	mas_awalk(mas, size);

	if (unlikely(mas_is_err(mas)))
		return xa_err(mas->node);

	offset = mas->offset;
	if (unlikely(offset == MAPLE_NODE_SLOTS))
		return -EBUSY;

	mt = mte_node_type(mas->node);
	pivots = ma_pivots(mas_mn(mas), mt);
	min = mas_safe_min(mas, pivots, offset);
	if (mas->index < min)
		mas->index = min;
	mas->last = mas->index + size - 1;
	return 0;
}
EXPORT_SYMBOL_GPL(mas_empty_area);

 
int mas_empty_area_rev(struct ma_state *mas, unsigned long min,
		unsigned long max, unsigned long size)
{
	struct maple_enode *last = mas->node;

	if (min > max)
		return -EINVAL;

	if (size == 0 || max - min < size - 1)
		return -EINVAL;

	if (mas_is_start(mas)) {
		mas_start(mas);
		mas->offset = mas_data_end(mas);
	} else if (mas->offset >= 2) {
		mas->offset -= 2;
	} else if (!mas_rewind_node(mas)) {
		return -EBUSY;
	}

	 
	if (mas_is_none(mas) || mas_is_ptr(mas))
		return mas_sparse_area(mas, min, max, size, false);

	 
	mas->index = min;
	mas->last = max;

	while (!mas_rev_awalk(mas, size, &min, &max)) {
		if (last == mas->node) {
			if (!mas_rewind_node(mas))
				return -EBUSY;
		} else {
			last = mas->node;
		}
	}

	if (mas_is_err(mas))
		return xa_err(mas->node);

	if (unlikely(mas->offset == MAPLE_NODE_SLOTS))
		return -EBUSY;

	 
	if (max < mas->last)
		mas->last = max;

	mas->index = mas->last - size + 1;
	return 0;
}
EXPORT_SYMBOL_GPL(mas_empty_area_rev);

 
static inline
unsigned char mte_dead_leaves(struct maple_enode *enode, struct maple_tree *mt,
			      void __rcu **slots)
{
	struct maple_node *node;
	enum maple_type type;
	void *entry;
	int offset;

	for (offset = 0; offset < mt_slot_count(enode); offset++) {
		entry = mt_slot(mt, slots, offset);
		type = mte_node_type(entry);
		node = mte_to_node(entry);
		 
		if (!node || !type)
			break;

		mte_set_node_dead(entry);
		node->type = type;
		rcu_assign_pointer(slots[offset], node);
	}

	return offset;
}

 
static void __rcu **mte_dead_walk(struct maple_enode **enode, unsigned char offset)
{
	struct maple_node *node, *next;
	void __rcu **slots = NULL;

	next = mte_to_node(*enode);
	do {
		*enode = ma_enode_ptr(next);
		node = mte_to_node(*enode);
		slots = ma_slots(node, node->type);
		next = rcu_dereference_protected(slots[offset],
					lock_is_held(&rcu_callback_map));
		offset = 0;
	} while (!ma_is_leaf(next->type));

	return slots;
}

 
static void mt_free_walk(struct rcu_head *head)
{
	void __rcu **slots;
	struct maple_node *node, *start;
	struct maple_enode *enode;
	unsigned char offset;
	enum maple_type type;

	node = container_of(head, struct maple_node, rcu);

	if (ma_is_leaf(node->type))
		goto free_leaf;

	start = node;
	enode = mt_mk_node(node, node->type);
	slots = mte_dead_walk(&enode, 0);
	node = mte_to_node(enode);
	do {
		mt_free_bulk(node->slot_len, slots);
		offset = node->parent_slot + 1;
		enode = node->piv_parent;
		if (mte_to_node(enode) == node)
			goto free_leaf;

		type = mte_node_type(enode);
		slots = ma_slots(mte_to_node(enode), type);
		if ((offset < mt_slots[type]) &&
		    rcu_dereference_protected(slots[offset],
					      lock_is_held(&rcu_callback_map)))
			slots = mte_dead_walk(&enode, offset);
		node = mte_to_node(enode);
	} while ((node != start) || (node->slot_len < offset));

	slots = ma_slots(node, node->type);
	mt_free_bulk(node->slot_len, slots);

free_leaf:
	mt_free_rcu(&node->rcu);
}

static inline void __rcu **mte_destroy_descend(struct maple_enode **enode,
	struct maple_tree *mt, struct maple_enode *prev, unsigned char offset)
{
	struct maple_node *node;
	struct maple_enode *next = *enode;
	void __rcu **slots = NULL;
	enum maple_type type;
	unsigned char next_offset = 0;

	do {
		*enode = next;
		node = mte_to_node(*enode);
		type = mte_node_type(*enode);
		slots = ma_slots(node, type);
		next = mt_slot_locked(mt, slots, next_offset);
		if ((mte_dead_node(next)))
			next = mt_slot_locked(mt, slots, ++next_offset);

		mte_set_node_dead(*enode);
		node->type = type;
		node->piv_parent = prev;
		node->parent_slot = offset;
		offset = next_offset;
		next_offset = 0;
		prev = *enode;
	} while (!mte_is_leaf(next));

	return slots;
}

static void mt_destroy_walk(struct maple_enode *enode, struct maple_tree *mt,
			    bool free)
{
	void __rcu **slots;
	struct maple_node *node = mte_to_node(enode);
	struct maple_enode *start;

	if (mte_is_leaf(enode)) {
		node->type = mte_node_type(enode);
		goto free_leaf;
	}

	start = enode;
	slots = mte_destroy_descend(&enode, mt, start, 0);
	node = mte_to_node(enode);  
	do {
		enum maple_type type;
		unsigned char offset;
		struct maple_enode *parent, *tmp;

		node->slot_len = mte_dead_leaves(enode, mt, slots);
		if (free)
			mt_free_bulk(node->slot_len, slots);
		offset = node->parent_slot + 1;
		enode = node->piv_parent;
		if (mte_to_node(enode) == node)
			goto free_leaf;

		type = mte_node_type(enode);
		slots = ma_slots(mte_to_node(enode), type);
		if (offset >= mt_slots[type])
			goto next;

		tmp = mt_slot_locked(mt, slots, offset);
		if (mte_node_type(tmp) && mte_to_node(tmp)) {
			parent = enode;
			enode = tmp;
			slots = mte_destroy_descend(&enode, mt, parent, offset);
		}
next:
		node = mte_to_node(enode);
	} while (start != enode);

	node = mte_to_node(enode);
	node->slot_len = mte_dead_leaves(enode, mt, slots);
	if (free)
		mt_free_bulk(node->slot_len, slots);

free_leaf:
	if (free)
		mt_free_rcu(&node->rcu);
	else
		mt_clear_meta(mt, node, node->type);
}

 
static inline void mte_destroy_walk(struct maple_enode *enode,
				    struct maple_tree *mt)
{
	struct maple_node *node = mte_to_node(enode);

	if (mt_in_rcu(mt)) {
		mt_destroy_walk(enode, mt, false);
		call_rcu(&node->rcu, mt_free_walk);
	} else {
		mt_destroy_walk(enode, mt, true);
	}
}

static void mas_wr_store_setup(struct ma_wr_state *wr_mas)
{
	if (!mas_is_active(wr_mas->mas)) {
		if (mas_is_start(wr_mas->mas))
			return;

		if (unlikely(mas_is_paused(wr_mas->mas)))
			goto reset;

		if (unlikely(mas_is_none(wr_mas->mas)))
			goto reset;

		if (unlikely(mas_is_overflow(wr_mas->mas)))
			goto reset;

		if (unlikely(mas_is_underflow(wr_mas->mas)))
			goto reset;
	}

	 
	if (wr_mas->mas->last > wr_mas->mas->max)
		goto reset;

	if (wr_mas->entry)
		return;

	if (mte_is_leaf(wr_mas->mas->node) &&
	    wr_mas->mas->last == wr_mas->mas->max)
		goto reset;

	return;

reset:
	mas_reset(wr_mas->mas);
}

 

 
void *mas_store(struct ma_state *mas, void *entry)
{
	MA_WR_STATE(wr_mas, mas, entry);

	trace_ma_write(__func__, mas, 0, entry);
#ifdef CONFIG_DEBUG_MAPLE_TREE
	if (MAS_WARN_ON(mas, mas->index > mas->last))
		pr_err("Error %lX > %lX %p\n", mas->index, mas->last, entry);

	if (mas->index > mas->last) {
		mas_set_err(mas, -EINVAL);
		return NULL;
	}

#endif

	 
	mas_wr_store_setup(&wr_mas);
	mas_wr_store_entry(&wr_mas);
	return wr_mas.content;
}
EXPORT_SYMBOL_GPL(mas_store);

 
int mas_store_gfp(struct ma_state *mas, void *entry, gfp_t gfp)
{
	MA_WR_STATE(wr_mas, mas, entry);

	mas_wr_store_setup(&wr_mas);
	trace_ma_write(__func__, mas, 0, entry);
retry:
	mas_wr_store_entry(&wr_mas);
	if (unlikely(mas_nomem(mas, gfp)))
		goto retry;

	if (unlikely(mas_is_err(mas)))
		return xa_err(mas->node);

	return 0;
}
EXPORT_SYMBOL_GPL(mas_store_gfp);

 
void mas_store_prealloc(struct ma_state *mas, void *entry)
{
	MA_WR_STATE(wr_mas, mas, entry);

	mas_wr_store_setup(&wr_mas);
	trace_ma_write(__func__, mas, 0, entry);
	mas_wr_store_entry(&wr_mas);
	MAS_WR_BUG_ON(&wr_mas, mas_is_err(mas));
	mas_destroy(mas);
}
EXPORT_SYMBOL_GPL(mas_store_prealloc);

 
int mas_preallocate(struct ma_state *mas, void *entry, gfp_t gfp)
{
	MA_WR_STATE(wr_mas, mas, entry);
	unsigned char node_size;
	int request = 1;
	int ret;


	if (unlikely(!mas->index && mas->last == ULONG_MAX))
		goto ask_now;

	mas_wr_store_setup(&wr_mas);
	wr_mas.content = mas_start(mas);
	 
	if (unlikely(mas_is_none(mas) || mas_is_ptr(mas)))
		goto ask_now;

	if (unlikely(!mas_wr_walk(&wr_mas))) {
		 
		request = 1 + mas_mt_height(mas) * 3;
		goto ask_now;
	}

	 
	 
	if (wr_mas.r_min == mas->index && wr_mas.r_max == mas->last)
		return 0;

	mas_wr_end_piv(&wr_mas);
	node_size = mas_wr_new_end(&wr_mas);

	 
	if (node_size == wr_mas.node_end) {
		 
		if (!mt_in_rcu(mas->tree))
			return 0;
		 
		if (wr_mas.offset_end - mas->offset == 1)
			return 0;
	}

	if (node_size >= mt_slots[wr_mas.type]) {
		 
		request = 1 + mas_mt_height(mas) * 2;
		goto ask_now;
	}

	 
	if (unlikely(mte_is_root(mas->node)))
		goto ask_now;

	 
	if (node_size  - 1 <= mt_min_slots[wr_mas.type])
		request = mas_mt_height(mas) * 2 - 1;

	 
ask_now:
	mas_node_count_gfp(mas, request, gfp);
	mas->mas_flags |= MA_STATE_PREALLOC;
	if (likely(!mas_is_err(mas)))
		return 0;

	mas_set_alloc_req(mas, 0);
	ret = xa_err(mas->node);
	mas_reset(mas);
	mas_destroy(mas);
	mas_reset(mas);
	return ret;
}
EXPORT_SYMBOL_GPL(mas_preallocate);

 
void mas_destroy(struct ma_state *mas)
{
	struct maple_alloc *node;
	unsigned long total;

	 
	if (mas->mas_flags & MA_STATE_REBALANCE) {
		unsigned char end;

		mas_start(mas);
		mtree_range_walk(mas);
		end = mas_data_end(mas) + 1;
		if (end < mt_min_slot_count(mas->node) - 1)
			mas_destroy_rebalance(mas, end);

		mas->mas_flags &= ~MA_STATE_REBALANCE;
	}
	mas->mas_flags &= ~(MA_STATE_BULK|MA_STATE_PREALLOC);

	total = mas_allocated(mas);
	while (total) {
		node = mas->alloc;
		mas->alloc = node->slot[0];
		if (node->node_count > 1) {
			size_t count = node->node_count - 1;

			mt_free_bulk(count, (void __rcu **)&node->slot[1]);
			total -= count;
		}
		kmem_cache_free(maple_node_cache, node);
		total--;
	}

	mas->alloc = NULL;
}
EXPORT_SYMBOL_GPL(mas_destroy);

 
int mas_expected_entries(struct ma_state *mas, unsigned long nr_entries)
{
	int nonleaf_cap = MAPLE_ARANGE64_SLOTS - 2;
	struct maple_enode *enode = mas->node;
	int nr_nodes;
	int ret;

	 

	 
	mas->mas_flags |= MA_STATE_BULK;

	 
	nr_nodes = max(nr_entries, nr_entries * 2 + 1);
	if (!mt_is_alloc(mas->tree))
		nonleaf_cap = MAPLE_RANGE64_SLOTS - 2;

	 
	nr_nodes = DIV_ROUND_UP(nr_nodes, MAPLE_RANGE64_SLOTS - 2);
	 
	nr_nodes += DIV_ROUND_UP(nr_nodes, nonleaf_cap);
	 
	mas_node_count_gfp(mas, nr_nodes + 3, GFP_KERNEL);

	 
	mas->mas_flags |= MA_STATE_PREALLOC;

	if (!mas_is_err(mas))
		return 0;

	ret = xa_err(mas->node);
	mas->node = enode;
	mas_destroy(mas);
	return ret;

}
EXPORT_SYMBOL_GPL(mas_expected_entries);

static inline bool mas_next_setup(struct ma_state *mas, unsigned long max,
		void **entry)
{
	bool was_none = mas_is_none(mas);

	if (unlikely(mas->last >= max)) {
		mas->node = MAS_OVERFLOW;
		return true;
	}

	if (mas_is_active(mas))
		return false;

	if (mas_is_none(mas) || mas_is_paused(mas)) {
		mas->node = MAS_START;
	} else if (mas_is_overflow(mas)) {
		 
		mas->node = MAS_START;
	} else if (mas_is_underflow(mas)) {
		mas->node = MAS_START;
		*entry = mas_walk(mas);
		if (*entry)
			return true;
	}

	if (mas_is_start(mas))
		*entry = mas_walk(mas);  

	if (mas_is_ptr(mas)) {
		*entry = NULL;
		if (was_none && mas->index == 0) {
			mas->index = mas->last = 0;
			return true;
		}
		mas->index = 1;
		mas->last = ULONG_MAX;
		mas->node = MAS_NONE;
		return true;
	}

	if (mas_is_none(mas))
		return true;

	return false;
}

 
void *mas_next(struct ma_state *mas, unsigned long max)
{
	void *entry = NULL;

	if (mas_next_setup(mas, max, &entry))
		return entry;

	 
	return mas_next_slot(mas, max, false, true);
}
EXPORT_SYMBOL_GPL(mas_next);

 
void *mas_next_range(struct ma_state *mas, unsigned long max)
{
	void *entry = NULL;

	if (mas_next_setup(mas, max, &entry))
		return entry;

	 
	return mas_next_slot(mas, max, true, true);
}
EXPORT_SYMBOL_GPL(mas_next_range);

 
void *mt_next(struct maple_tree *mt, unsigned long index, unsigned long max)
{
	void *entry = NULL;
	MA_STATE(mas, mt, index, index);

	rcu_read_lock();
	entry = mas_next(&mas, max);
	rcu_read_unlock();
	return entry;
}
EXPORT_SYMBOL_GPL(mt_next);

static inline bool mas_prev_setup(struct ma_state *mas, unsigned long min,
		void **entry)
{
	if (unlikely(mas->index <= min)) {
		mas->node = MAS_UNDERFLOW;
		return true;
	}

	if (mas_is_active(mas))
		return false;

	if (mas_is_overflow(mas)) {
		mas->node = MAS_START;
		*entry = mas_walk(mas);
		if (*entry)
			return true;
	}

	if (mas_is_none(mas) || mas_is_paused(mas)) {
		mas->node = MAS_START;
	} else if (mas_is_underflow(mas)) {
		 
		mas->node = MAS_START;
	}

	if (mas_is_start(mas))
		mas_walk(mas);

	if (unlikely(mas_is_ptr(mas))) {
		if (!mas->index)
			goto none;
		mas->index = mas->last = 0;
		*entry = mas_root(mas);
		return true;
	}

	if (mas_is_none(mas)) {
		if (mas->index) {
			 
			mas->index = mas->last = 0;
			mas->node = MAS_ROOT;
			*entry = mas_root(mas);
			return true;
		}
		return true;
	}

	return false;

none:
	mas->node = MAS_NONE;
	return true;
}

 
void *mas_prev(struct ma_state *mas, unsigned long min)
{
	void *entry = NULL;

	if (mas_prev_setup(mas, min, &entry))
		return entry;

	return mas_prev_slot(mas, min, false, true);
}
EXPORT_SYMBOL_GPL(mas_prev);

 
void *mas_prev_range(struct ma_state *mas, unsigned long min)
{
	void *entry = NULL;

	if (mas_prev_setup(mas, min, &entry))
		return entry;

	return mas_prev_slot(mas, min, true, true);
}
EXPORT_SYMBOL_GPL(mas_prev_range);

 
void *mt_prev(struct maple_tree *mt, unsigned long index, unsigned long min)
{
	void *entry = NULL;
	MA_STATE(mas, mt, index, index);

	rcu_read_lock();
	entry = mas_prev(&mas, min);
	rcu_read_unlock();
	return entry;
}
EXPORT_SYMBOL_GPL(mt_prev);

 
void mas_pause(struct ma_state *mas)
{
	mas->node = MAS_PAUSE;
}
EXPORT_SYMBOL_GPL(mas_pause);

 
static inline bool mas_find_setup(struct ma_state *mas, unsigned long max,
		void **entry)
{
	if (mas_is_active(mas)) {
		if (mas->last < max)
			return false;

		return true;
	}

	if (mas_is_paused(mas)) {
		if (unlikely(mas->last >= max))
			return true;

		mas->index = ++mas->last;
		mas->node = MAS_START;
	} else if (mas_is_none(mas)) {
		if (unlikely(mas->last >= max))
			return true;

		mas->index = mas->last;
		mas->node = MAS_START;
	} else if (mas_is_overflow(mas) || mas_is_underflow(mas)) {
		if (mas->index > max) {
			mas->node = MAS_OVERFLOW;
			return true;
		}

		mas->node = MAS_START;
	}

	if (mas_is_start(mas)) {
		 
		if (mas->index > max)
			return true;

		*entry = mas_walk(mas);
		if (*entry)
			return true;

	}

	if (unlikely(!mas_searchable(mas))) {
		if (unlikely(mas_is_ptr(mas)))
			goto ptr_out_of_range;

		return true;
	}

	if (mas->index == max)
		return true;

	return false;

ptr_out_of_range:
	mas->node = MAS_NONE;
	mas->index = 1;
	mas->last = ULONG_MAX;
	return true;
}

 
void *mas_find(struct ma_state *mas, unsigned long max)
{
	void *entry = NULL;

	if (mas_find_setup(mas, max, &entry))
		return entry;

	 
	return mas_next_slot(mas, max, false, false);
}
EXPORT_SYMBOL_GPL(mas_find);

 
void *mas_find_range(struct ma_state *mas, unsigned long max)
{
	void *entry = NULL;

	if (mas_find_setup(mas, max, &entry))
		return entry;

	 
	return mas_next_slot(mas, max, true, false);
}
EXPORT_SYMBOL_GPL(mas_find_range);

 
static inline bool mas_find_rev_setup(struct ma_state *mas, unsigned long min,
		void **entry)
{
	if (mas_is_active(mas)) {
		if (mas->index > min)
			return false;

		return true;
	}

	if (mas_is_paused(mas)) {
		if (unlikely(mas->index <= min)) {
			mas->node = MAS_NONE;
			return true;
		}
		mas->node = MAS_START;
		mas->last = --mas->index;
	} else if (mas_is_none(mas)) {
		if (mas->index <= min)
			goto none;

		mas->last = mas->index;
		mas->node = MAS_START;
	} else if (mas_is_underflow(mas) || mas_is_overflow(mas)) {
		if (mas->last <= min) {
			mas->node = MAS_UNDERFLOW;
			return true;
		}

		mas->node = MAS_START;
	}

	if (mas_is_start(mas)) {
		 
		if (mas->index < min)
			return true;

		*entry = mas_walk(mas);
		if (*entry)
			return true;
	}

	if (unlikely(!mas_searchable(mas))) {
		if (mas_is_ptr(mas))
			goto none;

		if (mas_is_none(mas)) {
			 
			mas->last = mas->index = 0;
			mas->node = MAS_ROOT;
			*entry = mas_root(mas);
			return true;
		}
	}

	if (mas->index < min)
		return true;

	return false;

none:
	mas->node = MAS_NONE;
	return true;
}

 
void *mas_find_rev(struct ma_state *mas, unsigned long min)
{
	void *entry = NULL;

	if (mas_find_rev_setup(mas, min, &entry))
		return entry;

	 
	return mas_prev_slot(mas, min, false, false);

}
EXPORT_SYMBOL_GPL(mas_find_rev);

 
void *mas_find_range_rev(struct ma_state *mas, unsigned long min)
{
	void *entry = NULL;

	if (mas_find_rev_setup(mas, min, &entry))
		return entry;

	 
	return mas_prev_slot(mas, min, true, false);
}
EXPORT_SYMBOL_GPL(mas_find_range_rev);

 
void *mas_erase(struct ma_state *mas)
{
	void *entry;
	MA_WR_STATE(wr_mas, mas, NULL);

	if (mas_is_none(mas) || mas_is_paused(mas))
		mas->node = MAS_START;

	 
	entry = mas_state_walk(mas);
	if (!entry)
		return NULL;

write_retry:
	 
	mas_reset(mas);
	mas_wr_store_setup(&wr_mas);
	mas_wr_store_entry(&wr_mas);
	if (mas_nomem(mas, GFP_KERNEL))
		goto write_retry;

	return entry;
}
EXPORT_SYMBOL_GPL(mas_erase);

 
bool mas_nomem(struct ma_state *mas, gfp_t gfp)
	__must_hold(mas->tree->ma_lock)
{
	if (likely(mas->node != MA_ERROR(-ENOMEM))) {
		mas_destroy(mas);
		return false;
	}

	if (gfpflags_allow_blocking(gfp) && !mt_external_lock(mas->tree)) {
		mtree_unlock(mas->tree);
		mas_alloc_nodes(mas, gfp);
		mtree_lock(mas->tree);
	} else {
		mas_alloc_nodes(mas, gfp);
	}

	if (!mas_allocated(mas))
		return false;

	mas->node = MAS_START;
	return true;
}

void __init maple_tree_init(void)
{
	maple_node_cache = kmem_cache_create("maple_node",
			sizeof(struct maple_node), sizeof(struct maple_node),
			SLAB_PANIC, NULL);
}

 
void *mtree_load(struct maple_tree *mt, unsigned long index)
{
	MA_STATE(mas, mt, index, index);
	void *entry;

	trace_ma_read(__func__, &mas);
	rcu_read_lock();
retry:
	entry = mas_start(&mas);
	if (unlikely(mas_is_none(&mas)))
		goto unlock;

	if (unlikely(mas_is_ptr(&mas))) {
		if (index)
			entry = NULL;

		goto unlock;
	}

	entry = mtree_lookup_walk(&mas);
	if (!entry && unlikely(mas_is_start(&mas)))
		goto retry;
unlock:
	rcu_read_unlock();
	if (xa_is_zero(entry))
		return NULL;

	return entry;
}
EXPORT_SYMBOL(mtree_load);

 
int mtree_store_range(struct maple_tree *mt, unsigned long index,
		unsigned long last, void *entry, gfp_t gfp)
{
	MA_STATE(mas, mt, index, last);
	MA_WR_STATE(wr_mas, &mas, entry);

	trace_ma_write(__func__, &mas, 0, entry);
	if (WARN_ON_ONCE(xa_is_advanced(entry)))
		return -EINVAL;

	if (index > last)
		return -EINVAL;

	mtree_lock(mt);
retry:
	mas_wr_store_entry(&wr_mas);
	if (mas_nomem(&mas, gfp))
		goto retry;

	mtree_unlock(mt);
	if (mas_is_err(&mas))
		return xa_err(mas.node);

	return 0;
}
EXPORT_SYMBOL(mtree_store_range);

 
int mtree_store(struct maple_tree *mt, unsigned long index, void *entry,
		 gfp_t gfp)
{
	return mtree_store_range(mt, index, index, entry, gfp);
}
EXPORT_SYMBOL(mtree_store);

 
int mtree_insert_range(struct maple_tree *mt, unsigned long first,
		unsigned long last, void *entry, gfp_t gfp)
{
	MA_STATE(ms, mt, first, last);

	if (WARN_ON_ONCE(xa_is_advanced(entry)))
		return -EINVAL;

	if (first > last)
		return -EINVAL;

	mtree_lock(mt);
retry:
	mas_insert(&ms, entry);
	if (mas_nomem(&ms, gfp))
		goto retry;

	mtree_unlock(mt);
	if (mas_is_err(&ms))
		return xa_err(ms.node);

	return 0;
}
EXPORT_SYMBOL(mtree_insert_range);

 
int mtree_insert(struct maple_tree *mt, unsigned long index, void *entry,
		 gfp_t gfp)
{
	return mtree_insert_range(mt, index, index, entry, gfp);
}
EXPORT_SYMBOL(mtree_insert);

int mtree_alloc_range(struct maple_tree *mt, unsigned long *startp,
		void *entry, unsigned long size, unsigned long min,
		unsigned long max, gfp_t gfp)
{
	int ret = 0;

	MA_STATE(mas, mt, 0, 0);
	if (!mt_is_alloc(mt))
		return -EINVAL;

	if (WARN_ON_ONCE(mt_is_reserved(entry)))
		return -EINVAL;

	mtree_lock(mt);
retry:
	ret = mas_empty_area(&mas, min, max, size);
	if (ret)
		goto unlock;

	mas_insert(&mas, entry);
	 
	if (mas_nomem(&mas, gfp))
		goto retry;

	if (mas_is_err(&mas))
		ret = xa_err(mas.node);
	else
		*startp = mas.index;

unlock:
	mtree_unlock(mt);
	return ret;
}
EXPORT_SYMBOL(mtree_alloc_range);

int mtree_alloc_rrange(struct maple_tree *mt, unsigned long *startp,
		void *entry, unsigned long size, unsigned long min,
		unsigned long max, gfp_t gfp)
{
	int ret = 0;

	MA_STATE(mas, mt, 0, 0);
	if (!mt_is_alloc(mt))
		return -EINVAL;

	if (WARN_ON_ONCE(mt_is_reserved(entry)))
		return -EINVAL;

	mtree_lock(mt);
retry:
	ret = mas_empty_area_rev(&mas, min, max, size);
	if (ret)
		goto unlock;

	mas_insert(&mas, entry);
	 
	if (mas_nomem(&mas, gfp))
		goto retry;

	if (mas_is_err(&mas))
		ret = xa_err(mas.node);
	else
		*startp = mas.index;

unlock:
	mtree_unlock(mt);
	return ret;
}
EXPORT_SYMBOL(mtree_alloc_rrange);

 
void *mtree_erase(struct maple_tree *mt, unsigned long index)
{
	void *entry = NULL;

	MA_STATE(mas, mt, index, index);
	trace_ma_op(__func__, &mas);

	mtree_lock(mt);
	entry = mas_erase(&mas);
	mtree_unlock(mt);

	return entry;
}
EXPORT_SYMBOL(mtree_erase);

 
void __mt_destroy(struct maple_tree *mt)
{
	void *root = mt_root_locked(mt);

	rcu_assign_pointer(mt->ma_root, NULL);
	if (xa_is_node(root))
		mte_destroy_walk(root, mt);

	mt->ma_flags = 0;
}
EXPORT_SYMBOL_GPL(__mt_destroy);

 
void mtree_destroy(struct maple_tree *mt)
{
	mtree_lock(mt);
	__mt_destroy(mt);
	mtree_unlock(mt);
}
EXPORT_SYMBOL(mtree_destroy);

 
void *mt_find(struct maple_tree *mt, unsigned long *index, unsigned long max)
{
	MA_STATE(mas, mt, *index, *index);
	void *entry;
#ifdef CONFIG_DEBUG_MAPLE_TREE
	unsigned long copy = *index;
#endif

	trace_ma_read(__func__, &mas);

	if ((*index) > max)
		return NULL;

	rcu_read_lock();
retry:
	entry = mas_state_walk(&mas);
	if (mas_is_start(&mas))
		goto retry;

	if (unlikely(xa_is_zero(entry)))
		entry = NULL;

	if (entry)
		goto unlock;

	while (mas_searchable(&mas) && (mas.last < max)) {
		entry = mas_next_entry(&mas, max);
		if (likely(entry && !xa_is_zero(entry)))
			break;
	}

	if (unlikely(xa_is_zero(entry)))
		entry = NULL;
unlock:
	rcu_read_unlock();
	if (likely(entry)) {
		*index = mas.last + 1;
#ifdef CONFIG_DEBUG_MAPLE_TREE
		if (MT_WARN_ON(mt, (*index) && ((*index) <= copy)))
			pr_err("index not increased! %lx <= %lx\n",
			       *index, copy);
#endif
	}

	return entry;
}
EXPORT_SYMBOL(mt_find);

 
void *mt_find_after(struct maple_tree *mt, unsigned long *index,
		    unsigned long max)
{
	if (!(*index))
		return NULL;

	return mt_find(mt, index, max);
}
EXPORT_SYMBOL(mt_find_after);

#ifdef CONFIG_DEBUG_MAPLE_TREE
atomic_t maple_tree_tests_run;
EXPORT_SYMBOL_GPL(maple_tree_tests_run);
atomic_t maple_tree_tests_passed;
EXPORT_SYMBOL_GPL(maple_tree_tests_passed);

#ifndef __KERNEL__
extern void kmem_cache_set_non_kernel(struct kmem_cache *, unsigned int);
void mt_set_non_kernel(unsigned int val)
{
	kmem_cache_set_non_kernel(maple_node_cache, val);
}

extern unsigned long kmem_cache_get_alloc(struct kmem_cache *);
unsigned long mt_get_alloc_size(void)
{
	return kmem_cache_get_alloc(maple_node_cache);
}

extern void kmem_cache_zero_nr_tallocated(struct kmem_cache *);
void mt_zero_nr_tallocated(void)
{
	kmem_cache_zero_nr_tallocated(maple_node_cache);
}

extern unsigned int kmem_cache_nr_tallocated(struct kmem_cache *);
unsigned int mt_nr_tallocated(void)
{
	return kmem_cache_nr_tallocated(maple_node_cache);
}

extern unsigned int kmem_cache_nr_allocated(struct kmem_cache *);
unsigned int mt_nr_allocated(void)
{
	return kmem_cache_nr_allocated(maple_node_cache);
}

 
static inline int mas_dead_node(struct ma_state *mas, unsigned long index)
{
	if (unlikely(!mas_searchable(mas) || mas_is_start(mas)))
		return 0;

	if (likely(!mte_dead_node(mas->node)))
		return 0;

	mas_rewalk(mas, index);
	return 1;
}

void mt_cache_shrink(void)
{
}
#else
 
void mt_cache_shrink(void)
{
	kmem_cache_shrink(maple_node_cache);

}
EXPORT_SYMBOL_GPL(mt_cache_shrink);

#endif  
 
static inline struct maple_enode *mas_get_slot(struct ma_state *mas,
		unsigned char offset)
{
	return mas_slot(mas, ma_slots(mas_mn(mas), mte_node_type(mas->node)),
			offset);
}

 
static void mas_dfs_postorder(struct ma_state *mas, unsigned long max)
{

	struct maple_enode *p = MAS_NONE, *mn = mas->node;
	unsigned long p_min, p_max;

	mas_next_node(mas, mas_mn(mas), max);
	if (!mas_is_none(mas))
		return;

	if (mte_is_root(mn))
		return;

	mas->node = mn;
	mas_ascend(mas);
	do {
		p = mas->node;
		p_min = mas->min;
		p_max = mas->max;
		mas_prev_node(mas, 0);
	} while (!mas_is_none(mas));

	mas->node = p;
	mas->max = p_max;
	mas->min = p_min;
}

 
static void mt_dump_node(const struct maple_tree *mt, void *entry,
		unsigned long min, unsigned long max, unsigned int depth,
		enum mt_dump_format format);
static void mt_dump_range(unsigned long min, unsigned long max,
			  unsigned int depth, enum mt_dump_format format)
{
	static const char spaces[] = "                                ";

	switch(format) {
	case mt_dump_hex:
		if (min == max)
			pr_info("%.*s%lx: ", depth * 2, spaces, min);
		else
			pr_info("%.*s%lx-%lx: ", depth * 2, spaces, min, max);
		break;
	default:
	case mt_dump_dec:
		if (min == max)
			pr_info("%.*s%lu: ", depth * 2, spaces, min);
		else
			pr_info("%.*s%lu-%lu: ", depth * 2, spaces, min, max);
	}
}

static void mt_dump_entry(void *entry, unsigned long min, unsigned long max,
			  unsigned int depth, enum mt_dump_format format)
{
	mt_dump_range(min, max, depth, format);

	if (xa_is_value(entry))
		pr_cont("value %ld (0x%lx) [%p]\n", xa_to_value(entry),
				xa_to_value(entry), entry);
	else if (xa_is_zero(entry))
		pr_cont("zero (%ld)\n", xa_to_internal(entry));
	else if (mt_is_reserved(entry))
		pr_cont("UNKNOWN ENTRY (%p)\n", entry);
	else
		pr_cont("%p\n", entry);
}

static void mt_dump_range64(const struct maple_tree *mt, void *entry,
		unsigned long min, unsigned long max, unsigned int depth,
		enum mt_dump_format format)
{
	struct maple_range_64 *node = &mte_to_node(entry)->mr64;
	bool leaf = mte_is_leaf(entry);
	unsigned long first = min;
	int i;

	pr_cont(" contents: ");
	for (i = 0; i < MAPLE_RANGE64_SLOTS - 1; i++) {
		switch(format) {
		case mt_dump_hex:
			pr_cont("%p %lX ", node->slot[i], node->pivot[i]);
			break;
		default:
		case mt_dump_dec:
			pr_cont("%p %lu ", node->slot[i], node->pivot[i]);
		}
	}
	pr_cont("%p\n", node->slot[i]);
	for (i = 0; i < MAPLE_RANGE64_SLOTS; i++) {
		unsigned long last = max;

		if (i < (MAPLE_RANGE64_SLOTS - 1))
			last = node->pivot[i];
		else if (!node->slot[i] && max != mt_node_max(entry))
			break;
		if (last == 0 && i > 0)
			break;
		if (leaf)
			mt_dump_entry(mt_slot(mt, node->slot, i),
					first, last, depth + 1, format);
		else if (node->slot[i])
			mt_dump_node(mt, mt_slot(mt, node->slot, i),
					first, last, depth + 1, format);

		if (last == max)
			break;
		if (last > max) {
			switch(format) {
			case mt_dump_hex:
				pr_err("node %p last (%lx) > max (%lx) at pivot %d!\n",
					node, last, max, i);
				break;
			default:
			case mt_dump_dec:
				pr_err("node %p last (%lu) > max (%lu) at pivot %d!\n",
					node, last, max, i);
			}
		}
		first = last + 1;
	}
}

static void mt_dump_arange64(const struct maple_tree *mt, void *entry,
	unsigned long min, unsigned long max, unsigned int depth,
	enum mt_dump_format format)
{
	struct maple_arange_64 *node = &mte_to_node(entry)->ma64;
	bool leaf = mte_is_leaf(entry);
	unsigned long first = min;
	int i;

	pr_cont(" contents: ");
	for (i = 0; i < MAPLE_ARANGE64_SLOTS; i++) {
		switch (format) {
		case mt_dump_hex:
			pr_cont("%lx ", node->gap[i]);
			break;
		default:
		case mt_dump_dec:
			pr_cont("%lu ", node->gap[i]);
		}
	}
	pr_cont("| %02X %02X| ", node->meta.end, node->meta.gap);
	for (i = 0; i < MAPLE_ARANGE64_SLOTS - 1; i++) {
		switch (format) {
		case mt_dump_hex:
			pr_cont("%p %lX ", node->slot[i], node->pivot[i]);
			break;
		default:
		case mt_dump_dec:
			pr_cont("%p %lu ", node->slot[i], node->pivot[i]);
		}
	}
	pr_cont("%p\n", node->slot[i]);
	for (i = 0; i < MAPLE_ARANGE64_SLOTS; i++) {
		unsigned long last = max;

		if (i < (MAPLE_ARANGE64_SLOTS - 1))
			last = node->pivot[i];
		else if (!node->slot[i])
			break;
		if (last == 0 && i > 0)
			break;
		if (leaf)
			mt_dump_entry(mt_slot(mt, node->slot, i),
					first, last, depth + 1, format);
		else if (node->slot[i])
			mt_dump_node(mt, mt_slot(mt, node->slot, i),
					first, last, depth + 1, format);

		if (last == max)
			break;
		if (last > max) {
			pr_err("node %p last (%lu) > max (%lu) at pivot %d!\n",
					node, last, max, i);
			break;
		}
		first = last + 1;
	}
}

static void mt_dump_node(const struct maple_tree *mt, void *entry,
		unsigned long min, unsigned long max, unsigned int depth,
		enum mt_dump_format format)
{
	struct maple_node *node = mte_to_node(entry);
	unsigned int type = mte_node_type(entry);
	unsigned int i;

	mt_dump_range(min, max, depth, format);

	pr_cont("node %p depth %d type %d parent %p", node, depth, type,
			node ? node->parent : NULL);
	switch (type) {
	case maple_dense:
		pr_cont("\n");
		for (i = 0; i < MAPLE_NODE_SLOTS; i++) {
			if (min + i > max)
				pr_cont("OUT OF RANGE: ");
			mt_dump_entry(mt_slot(mt, node->slot, i),
					min + i, min + i, depth, format);
		}
		break;
	case maple_leaf_64:
	case maple_range_64:
		mt_dump_range64(mt, entry, min, max, depth, format);
		break;
	case maple_arange_64:
		mt_dump_arange64(mt, entry, min, max, depth, format);
		break;

	default:
		pr_cont(" UNKNOWN TYPE\n");
	}
}

void mt_dump(const struct maple_tree *mt, enum mt_dump_format format)
{
	void *entry = rcu_dereference_check(mt->ma_root, mt_locked(mt));

	pr_info("maple_tree(%p) flags %X, height %u root %p\n",
		 mt, mt->ma_flags, mt_height(mt), entry);
	if (!xa_is_node(entry))
		mt_dump_entry(entry, 0, 0, 0, format);
	else if (entry)
		mt_dump_node(mt, entry, 0, mt_node_max(entry), 0, format);
}
EXPORT_SYMBOL_GPL(mt_dump);

 
static void mas_validate_gaps(struct ma_state *mas)
{
	struct maple_enode *mte = mas->node;
	struct maple_node *p_mn, *node = mte_to_node(mte);
	enum maple_type mt = mte_node_type(mas->node);
	unsigned long gap = 0, max_gap = 0;
	unsigned long p_end, p_start = mas->min;
	unsigned char p_slot, offset;
	unsigned long *gaps = NULL;
	unsigned long *pivots = ma_pivots(node, mt);
	unsigned int i;

	if (ma_is_dense(mt)) {
		for (i = 0; i < mt_slot_count(mte); i++) {
			if (mas_get_slot(mas, i)) {
				if (gap > max_gap)
					max_gap = gap;
				gap = 0;
				continue;
			}
			gap++;
		}
		goto counted;
	}

	gaps = ma_gaps(node, mt);
	for (i = 0; i < mt_slot_count(mte); i++) {
		p_end = mas_safe_pivot(mas, pivots, i, mt);

		if (!gaps) {
			if (!mas_get_slot(mas, i))
				gap = p_end - p_start + 1;
		} else {
			void *entry = mas_get_slot(mas, i);

			gap = gaps[i];
			MT_BUG_ON(mas->tree, !entry);

			if (gap > p_end - p_start + 1) {
				pr_err("%p[%u] %lu >= %lu - %lu + 1 (%lu)\n",
				       mas_mn(mas), i, gap, p_end, p_start,
				       p_end - p_start + 1);
				MT_BUG_ON(mas->tree, gap > p_end - p_start + 1);
			}
		}

		if (gap > max_gap)
			max_gap = gap;

		p_start = p_end + 1;
		if (p_end >= mas->max)
			break;
	}

counted:
	if (mt == maple_arange_64) {
		offset = ma_meta_gap(node, mt);
		if (offset > i) {
			pr_err("gap offset %p[%u] is invalid\n", node, offset);
			MT_BUG_ON(mas->tree, 1);
		}

		if (gaps[offset] != max_gap) {
			pr_err("gap %p[%u] is not the largest gap %lu\n",
			       node, offset, max_gap);
			MT_BUG_ON(mas->tree, 1);
		}

		MT_BUG_ON(mas->tree, !gaps);
		for (i++ ; i < mt_slot_count(mte); i++) {
			if (gaps[i] != 0) {
				pr_err("gap %p[%u] beyond node limit != 0\n",
				       node, i);
				MT_BUG_ON(mas->tree, 1);
			}
		}
	}

	if (mte_is_root(mte))
		return;

	p_slot = mte_parent_slot(mas->node);
	p_mn = mte_parent(mte);
	MT_BUG_ON(mas->tree, max_gap > mas->max);
	if (ma_gaps(p_mn, mas_parent_type(mas, mte))[p_slot] != max_gap) {
		pr_err("gap %p[%u] != %lu\n", p_mn, p_slot, max_gap);
		mt_dump(mas->tree, mt_dump_hex);
		MT_BUG_ON(mas->tree, 1);
	}
}

static void mas_validate_parent_slot(struct ma_state *mas)
{
	struct maple_node *parent;
	struct maple_enode *node;
	enum maple_type p_type;
	unsigned char p_slot;
	void __rcu **slots;
	int i;

	if (mte_is_root(mas->node))
		return;

	p_slot = mte_parent_slot(mas->node);
	p_type = mas_parent_type(mas, mas->node);
	parent = mte_parent(mas->node);
	slots = ma_slots(parent, p_type);
	MT_BUG_ON(mas->tree, mas_mn(mas) == parent);

	 

	for (i = 0; i < mt_slots[p_type]; i++) {
		node = mas_slot(mas, slots, i);
		if (i == p_slot) {
			if (node != mas->node)
				pr_err("parent %p[%u] does not have %p\n",
					parent, i, mas_mn(mas));
			MT_BUG_ON(mas->tree, node != mas->node);
		} else if (node == mas->node) {
			pr_err("Invalid child %p at parent %p[%u] p_slot %u\n",
			       mas_mn(mas), parent, i, p_slot);
			MT_BUG_ON(mas->tree, node == mas->node);
		}
	}
}

static void mas_validate_child_slot(struct ma_state *mas)
{
	enum maple_type type = mte_node_type(mas->node);
	void __rcu **slots = ma_slots(mte_to_node(mas->node), type);
	unsigned long *pivots = ma_pivots(mte_to_node(mas->node), type);
	struct maple_enode *child;
	unsigned char i;

	if (mte_is_leaf(mas->node))
		return;

	for (i = 0; i < mt_slots[type]; i++) {
		child = mas_slot(mas, slots, i);

		if (!child) {
			pr_err("Non-leaf node lacks child at %p[%u]\n",
			       mas_mn(mas), i);
			MT_BUG_ON(mas->tree, 1);
		}

		if (mte_parent_slot(child) != i) {
			pr_err("Slot error at %p[%u]: child %p has pslot %u\n",
			       mas_mn(mas), i, mte_to_node(child),
			       mte_parent_slot(child));
			MT_BUG_ON(mas->tree, 1);
		}

		if (mte_parent(child) != mte_to_node(mas->node)) {
			pr_err("child %p has parent %p not %p\n",
			       mte_to_node(child), mte_parent(child),
			       mte_to_node(mas->node));
			MT_BUG_ON(mas->tree, 1);
		}

		if (i < mt_pivots[type] && pivots[i] == mas->max)
			break;
	}
}

 
static void mas_validate_limits(struct ma_state *mas)
{
	int i;
	unsigned long prev_piv = 0;
	enum maple_type type = mte_node_type(mas->node);
	void __rcu **slots = ma_slots(mte_to_node(mas->node), type);
	unsigned long *pivots = ma_pivots(mas_mn(mas), type);

	for (i = 0; i < mt_slots[type]; i++) {
		unsigned long piv;

		piv = mas_safe_pivot(mas, pivots, i, type);

		if (!piv && (i != 0)) {
			pr_err("Missing node limit pivot at %p[%u]",
			       mas_mn(mas), i);
			MAS_WARN_ON(mas, 1);
		}

		if (prev_piv > piv) {
			pr_err("%p[%u] piv %lu < prev_piv %lu\n",
				mas_mn(mas), i, piv, prev_piv);
			MAS_WARN_ON(mas, piv < prev_piv);
		}

		if (piv < mas->min) {
			pr_err("%p[%u] %lu < %lu\n", mas_mn(mas), i,
				piv, mas->min);
			MAS_WARN_ON(mas, piv < mas->min);
		}
		if (piv > mas->max) {
			pr_err("%p[%u] %lu > %lu\n", mas_mn(mas), i,
				piv, mas->max);
			MAS_WARN_ON(mas, piv > mas->max);
		}
		prev_piv = piv;
		if (piv == mas->max)
			break;
	}

	if (mas_data_end(mas) != i) {
		pr_err("node%p: data_end %u != the last slot offset %u\n",
		       mas_mn(mas), mas_data_end(mas), i);
		MT_BUG_ON(mas->tree, 1);
	}

	for (i += 1; i < mt_slots[type]; i++) {
		void *entry = mas_slot(mas, slots, i);

		if (entry && (i != mt_slots[type] - 1)) {
			pr_err("%p[%u] should not have entry %p\n", mas_mn(mas),
			       i, entry);
			MT_BUG_ON(mas->tree, entry != NULL);
		}

		if (i < mt_pivots[type]) {
			unsigned long piv = pivots[i];

			if (!piv)
				continue;

			pr_err("%p[%u] should not have piv %lu\n",
			       mas_mn(mas), i, piv);
			MAS_WARN_ON(mas, i < mt_pivots[type] - 1);
		}
	}
}

static void mt_validate_nulls(struct maple_tree *mt)
{
	void *entry, *last = (void *)1;
	unsigned char offset = 0;
	void __rcu **slots;
	MA_STATE(mas, mt, 0, 0);

	mas_start(&mas);
	if (mas_is_none(&mas) || (mas.node == MAS_ROOT))
		return;

	while (!mte_is_leaf(mas.node))
		mas_descend(&mas);

	slots = ma_slots(mte_to_node(mas.node), mte_node_type(mas.node));
	do {
		entry = mas_slot(&mas, slots, offset);
		if (!last && !entry) {
			pr_err("Sequential nulls end at %p[%u]\n",
				mas_mn(&mas), offset);
		}
		MT_BUG_ON(mt, !last && !entry);
		last = entry;
		if (offset == mas_data_end(&mas)) {
			mas_next_node(&mas, mas_mn(&mas), ULONG_MAX);
			if (mas_is_none(&mas))
				return;
			offset = 0;
			slots = ma_slots(mte_to_node(mas.node),
					 mte_node_type(mas.node));
		} else {
			offset++;
		}

	} while (!mas_is_none(&mas));
}

 
void mt_validate(struct maple_tree *mt)
{
	unsigned char end;

	MA_STATE(mas, mt, 0, 0);
	rcu_read_lock();
	mas_start(&mas);
	if (!mas_searchable(&mas))
		goto done;

	while (!mte_is_leaf(mas.node))
		mas_descend(&mas);

	while (!mas_is_none(&mas)) {
		MAS_WARN_ON(&mas, mte_dead_node(mas.node));
		end = mas_data_end(&mas);
		if (MAS_WARN_ON(&mas, (end < mt_min_slot_count(mas.node)) &&
				(mas.max != ULONG_MAX))) {
			pr_err("Invalid size %u of %p\n", end, mas_mn(&mas));
		}

		mas_validate_parent_slot(&mas);
		mas_validate_limits(&mas);
		mas_validate_child_slot(&mas);
		if (mt_is_alloc(mt))
			mas_validate_gaps(&mas);
		mas_dfs_postorder(&mas, ULONG_MAX);
	}
	mt_validate_nulls(mt);
done:
	rcu_read_unlock();

}
EXPORT_SYMBOL_GPL(mt_validate);

void mas_dump(const struct ma_state *mas)
{
	pr_err("MAS: tree=%p enode=%p ", mas->tree, mas->node);
	if (mas_is_none(mas))
		pr_err("(MAS_NONE) ");
	else if (mas_is_ptr(mas))
		pr_err("(MAS_ROOT) ");
	else if (mas_is_start(mas))
		 pr_err("(MAS_START) ");
	else if (mas_is_paused(mas))
		pr_err("(MAS_PAUSED) ");

	pr_err("[%u] index=%lx last=%lx\n", mas->offset, mas->index, mas->last);
	pr_err("     min=%lx max=%lx alloc=%p, depth=%u, flags=%x\n",
	       mas->min, mas->max, mas->alloc, mas->depth, mas->mas_flags);
	if (mas->index > mas->last)
		pr_err("Check index & last\n");
}
EXPORT_SYMBOL_GPL(mas_dump);

void mas_wr_dump(const struct ma_wr_state *wr_mas)
{
	pr_err("WR_MAS: node=%p r_min=%lx r_max=%lx\n",
	       wr_mas->node, wr_mas->r_min, wr_mas->r_max);
	pr_err("        type=%u off_end=%u, node_end=%u, end_piv=%lx\n",
	       wr_mas->type, wr_mas->offset_end, wr_mas->node_end,
	       wr_mas->end_piv);
}
EXPORT_SYMBOL_GPL(mas_wr_dump);

#endif  

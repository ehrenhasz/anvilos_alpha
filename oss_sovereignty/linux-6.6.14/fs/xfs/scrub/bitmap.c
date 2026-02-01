
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_bit.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "scrub/scrub.h"
#include "scrub/bitmap.h"

#include <linux/interval_tree_generic.h>

struct xbitmap_node {
	struct rb_node	bn_rbnode;

	 
	uint64_t	bn_start;

	 
	uint64_t	bn_last;

	 
	uint64_t	__bn_subtree_last;
};

 

#define START(node) ((node)->bn_start)
#define LAST(node)  ((node)->bn_last)

 
static inline void
xbitmap_tree_insert(struct xbitmap_node *node, struct rb_root_cached *root);

static inline void
xbitmap_tree_remove(struct xbitmap_node *node, struct rb_root_cached *root);

static inline struct xbitmap_node *
xbitmap_tree_iter_first(struct rb_root_cached *root, uint64_t start,
			uint64_t last);

static inline struct xbitmap_node *
xbitmap_tree_iter_next(struct xbitmap_node *node, uint64_t start,
		       uint64_t last);

INTERVAL_TREE_DEFINE(struct xbitmap_node, bn_rbnode, uint64_t,
		__bn_subtree_last, START, LAST, static inline, xbitmap_tree)

 
#define for_each_xbitmap_extent(bn, bitmap) \
	for ((bn) = rb_entry_safe(rb_first(&(bitmap)->xb_root.rb_root), \
				   struct xbitmap_node, bn_rbnode); \
	     (bn) != NULL; \
	     (bn) = rb_entry_safe(rb_next(&(bn)->bn_rbnode), \
				   struct xbitmap_node, bn_rbnode))

 
int
xbitmap_clear(
	struct xbitmap		*bitmap,
	uint64_t		start,
	uint64_t		len)
{
	struct xbitmap_node	*bn;
	struct xbitmap_node	*new_bn;
	uint64_t		last = start + len - 1;

	while ((bn = xbitmap_tree_iter_first(&bitmap->xb_root, start, last))) {
		if (bn->bn_start < start && bn->bn_last > last) {
			uint64_t	old_last = bn->bn_last;

			 
			xbitmap_tree_remove(bn, &bitmap->xb_root);
			bn->bn_last = start - 1;
			xbitmap_tree_insert(bn, &bitmap->xb_root);

			 
			new_bn = kmalloc(sizeof(struct xbitmap_node),
					XCHK_GFP_FLAGS);
			if (!new_bn)
				return -ENOMEM;
			new_bn->bn_start = last + 1;
			new_bn->bn_last = old_last;
			xbitmap_tree_insert(new_bn, &bitmap->xb_root);
		} else if (bn->bn_start < start) {
			 
			xbitmap_tree_remove(bn, &bitmap->xb_root);
			bn->bn_last = start - 1;
			xbitmap_tree_insert(bn, &bitmap->xb_root);
		} else if (bn->bn_last > last) {
			 
			xbitmap_tree_remove(bn, &bitmap->xb_root);
			bn->bn_start = last + 1;
			xbitmap_tree_insert(bn, &bitmap->xb_root);
			break;
		} else {
			 
			xbitmap_tree_remove(bn, &bitmap->xb_root);
			kfree(bn);
		}
	}

	return 0;
}

 
int
xbitmap_set(
	struct xbitmap		*bitmap,
	uint64_t		start,
	uint64_t		len)
{
	struct xbitmap_node	*left;
	struct xbitmap_node	*right;
	uint64_t		last = start + len - 1;
	int			error;

	 
	left = xbitmap_tree_iter_first(&bitmap->xb_root, start, last);
	if (left && left->bn_start <= start && left->bn_last >= last)
		return 0;

	 
	error = xbitmap_clear(bitmap, start, len);
	if (error)
		return error;

	 
	left = xbitmap_tree_iter_first(&bitmap->xb_root, start - 1, start - 1);
	ASSERT(!left || left->bn_last + 1 == start);

	 
	right = xbitmap_tree_iter_first(&bitmap->xb_root, last + 1, last + 1);
	ASSERT(!right || right->bn_start == last + 1);

	if (left && right) {
		 
		xbitmap_tree_remove(left, &bitmap->xb_root);
		xbitmap_tree_remove(right, &bitmap->xb_root);
		left->bn_last = right->bn_last;
		xbitmap_tree_insert(left, &bitmap->xb_root);
		kfree(right);
	} else if (left) {
		 
		xbitmap_tree_remove(left, &bitmap->xb_root);
		left->bn_last = last;
		xbitmap_tree_insert(left, &bitmap->xb_root);
	} else if (right) {
		 
		xbitmap_tree_remove(right, &bitmap->xb_root);
		right->bn_start = start;
		xbitmap_tree_insert(right, &bitmap->xb_root);
	} else {
		 
		left = kmalloc(sizeof(struct xbitmap_node), XCHK_GFP_FLAGS);
		if (!left)
			return -ENOMEM;
		left->bn_start = start;
		left->bn_last = last;
		xbitmap_tree_insert(left, &bitmap->xb_root);
	}

	return 0;
}

 
void
xbitmap_destroy(
	struct xbitmap		*bitmap)
{
	struct xbitmap_node	*bn;

	while ((bn = xbitmap_tree_iter_first(&bitmap->xb_root, 0, -1ULL))) {
		xbitmap_tree_remove(bn, &bitmap->xb_root);
		kfree(bn);
	}
}

 
void
xbitmap_init(
	struct xbitmap		*bitmap)
{
	bitmap->xb_root = RB_ROOT_CACHED;
}

 
int
xbitmap_disunion(
	struct xbitmap		*bitmap,
	struct xbitmap		*sub)
{
	struct xbitmap_node	*bn;
	int			error;

	if (xbitmap_empty(bitmap) || xbitmap_empty(sub))
		return 0;

	for_each_xbitmap_extent(bn, sub) {
		error = xbitmap_clear(bitmap, bn->bn_start,
				bn->bn_last - bn->bn_start + 1);
		if (error)
			return error;
	}

	return 0;
}

 

 
STATIC int
xagb_bitmap_visit_btblock(
	struct xfs_btree_cur	*cur,
	int			level,
	void			*priv)
{
	struct xagb_bitmap	*bitmap = priv;
	struct xfs_buf		*bp;
	xfs_fsblock_t		fsbno;
	xfs_agblock_t		agbno;

	xfs_btree_get_block(cur, level, &bp);
	if (!bp)
		return 0;

	fsbno = XFS_DADDR_TO_FSB(cur->bc_mp, xfs_buf_daddr(bp));
	agbno = XFS_FSB_TO_AGBNO(cur->bc_mp, fsbno);

	return xagb_bitmap_set(bitmap, agbno, 1);
}

 
int
xagb_bitmap_set_btblocks(
	struct xagb_bitmap	*bitmap,
	struct xfs_btree_cur	*cur)
{
	return xfs_btree_visit_blocks(cur, xagb_bitmap_visit_btblock,
			XFS_BTREE_VISIT_ALL, bitmap);
}

 
int
xagb_bitmap_set_btcur_path(
	struct xagb_bitmap	*bitmap,
	struct xfs_btree_cur	*cur)
{
	int			i;
	int			error;

	for (i = 0; i < cur->bc_nlevels && cur->bc_levels[i].ptr == 1; i++) {
		error = xagb_bitmap_visit_btblock(cur, i, bitmap);
		if (error)
			return error;
	}

	return 0;
}

 
uint64_t
xbitmap_hweight(
	struct xbitmap		*bitmap)
{
	struct xbitmap_node	*bn;
	uint64_t		ret = 0;

	for_each_xbitmap_extent(bn, bitmap)
		ret += bn->bn_last - bn->bn_start + 1;

	return ret;
}

 
int
xbitmap_walk(
	struct xbitmap		*bitmap,
	xbitmap_walk_fn		fn,
	void			*priv)
{
	struct xbitmap_node	*bn;
	int			error = 0;

	for_each_xbitmap_extent(bn, bitmap) {
		error = fn(bn->bn_start, bn->bn_last - bn->bn_start + 1, priv);
		if (error)
			break;
	}

	return error;
}

 
bool
xbitmap_empty(
	struct xbitmap		*bitmap)
{
	return bitmap->xb_root.rb_root.rb_node == NULL;
}

 
bool
xbitmap_test(
	struct xbitmap		*bitmap,
	uint64_t		start,
	uint64_t		*len)
{
	struct xbitmap_node	*bn;
	uint64_t		last = start + *len - 1;

	bn = xbitmap_tree_iter_first(&bitmap->xb_root, start, last);
	if (!bn)
		return false;
	if (bn->bn_start <= start) {
		if (bn->bn_last < last)
			*len = bn->bn_last - start + 1;
		return true;
	}
	*len = bn->bn_start - start;
	return false;
}

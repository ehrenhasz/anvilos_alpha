
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_btree.h"
#include "xfs_trace.h"
#include "xfs_btree_staging.h"

 

 
STATIC struct xfs_btree_cur *
xfs_btree_fakeroot_dup_cursor(
	struct xfs_btree_cur	*cur)
{
	ASSERT(0);
	return NULL;
}

 
STATIC int
xfs_btree_fakeroot_alloc_block(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*start_bno,
	union xfs_btree_ptr		*new_bno,
	int				*stat)
{
	ASSERT(0);
	return -EFSCORRUPTED;
}

 
STATIC int
xfs_btree_fakeroot_free_block(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp)
{
	ASSERT(0);
	return -EFSCORRUPTED;
}

 
STATIC void
xfs_btree_fakeroot_init_ptr_from_cur(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	struct xbtree_afakeroot	*afake;

	ASSERT(cur->bc_flags & XFS_BTREE_STAGING);

	afake = cur->bc_ag.afake;
	ptr->s = cpu_to_be32(afake->af_root);
}

 

 
STATIC void
xfs_btree_afakeroot_set_root(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*ptr,
	int				inc)
{
	struct xbtree_afakeroot	*afake = cur->bc_ag.afake;

	ASSERT(cur->bc_flags & XFS_BTREE_STAGING);
	afake->af_root = be32_to_cpu(ptr->s);
	afake->af_levels += inc;
}

 
void
xfs_btree_stage_afakeroot(
	struct xfs_btree_cur		*cur,
	struct xbtree_afakeroot		*afake)
{
	struct xfs_btree_ops		*nops;

	ASSERT(!(cur->bc_flags & XFS_BTREE_STAGING));
	ASSERT(!(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE));
	ASSERT(cur->bc_tp == NULL);

	nops = kmem_alloc(sizeof(struct xfs_btree_ops), KM_NOFS);
	memcpy(nops, cur->bc_ops, sizeof(struct xfs_btree_ops));
	nops->alloc_block = xfs_btree_fakeroot_alloc_block;
	nops->free_block = xfs_btree_fakeroot_free_block;
	nops->init_ptr_from_cur = xfs_btree_fakeroot_init_ptr_from_cur;
	nops->set_root = xfs_btree_afakeroot_set_root;
	nops->dup_cursor = xfs_btree_fakeroot_dup_cursor;

	cur->bc_ag.afake = afake;
	cur->bc_nlevels = afake->af_levels;
	cur->bc_ops = nops;
	cur->bc_flags |= XFS_BTREE_STAGING;
}

 
void
xfs_btree_commit_afakeroot(
	struct xfs_btree_cur		*cur,
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	const struct xfs_btree_ops	*ops)
{
	ASSERT(cur->bc_flags & XFS_BTREE_STAGING);
	ASSERT(cur->bc_tp == NULL);

	trace_xfs_btree_commit_afakeroot(cur);

	kmem_free((void *)cur->bc_ops);
	cur->bc_ag.agbp = agbp;
	cur->bc_ops = ops;
	cur->bc_flags &= ~XFS_BTREE_STAGING;
	cur->bc_tp = tp;
}

 

 
void
xfs_btree_stage_ifakeroot(
	struct xfs_btree_cur		*cur,
	struct xbtree_ifakeroot		*ifake,
	struct xfs_btree_ops		**new_ops)
{
	struct xfs_btree_ops		*nops;

	ASSERT(!(cur->bc_flags & XFS_BTREE_STAGING));
	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);
	ASSERT(cur->bc_tp == NULL);

	nops = kmem_alloc(sizeof(struct xfs_btree_ops), KM_NOFS);
	memcpy(nops, cur->bc_ops, sizeof(struct xfs_btree_ops));
	nops->alloc_block = xfs_btree_fakeroot_alloc_block;
	nops->free_block = xfs_btree_fakeroot_free_block;
	nops->init_ptr_from_cur = xfs_btree_fakeroot_init_ptr_from_cur;
	nops->dup_cursor = xfs_btree_fakeroot_dup_cursor;

	cur->bc_ino.ifake = ifake;
	cur->bc_nlevels = ifake->if_levels;
	cur->bc_ops = nops;
	cur->bc_flags |= XFS_BTREE_STAGING;

	if (new_ops)
		*new_ops = nops;
}

 
void
xfs_btree_commit_ifakeroot(
	struct xfs_btree_cur		*cur,
	struct xfs_trans		*tp,
	int				whichfork,
	const struct xfs_btree_ops	*ops)
{
	ASSERT(cur->bc_flags & XFS_BTREE_STAGING);
	ASSERT(cur->bc_tp == NULL);

	trace_xfs_btree_commit_ifakeroot(cur);

	kmem_free((void *)cur->bc_ops);
	cur->bc_ino.ifake = NULL;
	cur->bc_ino.whichfork = whichfork;
	cur->bc_ops = ops;
	cur->bc_flags &= ~XFS_BTREE_STAGING;
	cur->bc_tp = tp;
}

 

 
static void
xfs_btree_bload_drop_buf(
	struct list_head	*buffers_list,
	struct xfs_buf		**bpp)
{
	if (*bpp == NULL)
		return;

	if (!xfs_buf_delwri_queue(*bpp, buffers_list))
		ASSERT(0);

	xfs_buf_relse(*bpp);
	*bpp = NULL;
}

 
STATIC int
xfs_btree_bload_prep_block(
	struct xfs_btree_cur		*cur,
	struct xfs_btree_bload		*bbl,
	struct list_head		*buffers_list,
	unsigned int			level,
	unsigned int			nr_this_block,
	union xfs_btree_ptr		*ptrp,  
	struct xfs_buf			**bpp,  
	struct xfs_btree_block		**blockp,  
	void				*priv)
{
	union xfs_btree_ptr		new_ptr;
	struct xfs_buf			*new_bp;
	struct xfs_btree_block		*new_block;
	int				ret;

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1) {
		struct xfs_ifork	*ifp = xfs_btree_ifork_ptr(cur);
		size_t			new_size;

		ASSERT(*bpp == NULL);

		 
		new_size = bbl->iroot_size(cur, nr_this_block, priv);
		ifp->if_broot = kmem_zalloc(new_size, 0);
		ifp->if_broot_bytes = (int)new_size;

		 
		xfs_btree_init_block_int(cur->bc_mp, ifp->if_broot,
				XFS_BUF_DADDR_NULL, cur->bc_btnum, level,
				nr_this_block, cur->bc_ino.ip->i_ino,
				cur->bc_flags);

		*bpp = NULL;
		*blockp = ifp->if_broot;
		xfs_btree_set_ptr_null(cur, ptrp);
		return 0;
	}

	 
	xfs_btree_set_ptr_null(cur, &new_ptr);
	ret = bbl->claim_block(cur, &new_ptr, priv);
	if (ret)
		return ret;

	ASSERT(!xfs_btree_ptr_is_null(cur, &new_ptr));

	ret = xfs_btree_get_buf_block(cur, &new_ptr, &new_block, &new_bp);
	if (ret)
		return ret;

	 
	if (*blockp)
		xfs_btree_set_sibling(cur, *blockp, &new_ptr, XFS_BB_RIGHTSIB);
	xfs_btree_bload_drop_buf(buffers_list, bpp);

	 
	xfs_btree_init_block_cur(cur, new_bp, level, nr_this_block);
	xfs_btree_set_sibling(cur, new_block, ptrp, XFS_BB_LEFTSIB);

	 
	*bpp = new_bp;
	*blockp = new_block;
	xfs_btree_copy_ptrs(cur, ptrp, &new_ptr, 1);
	return 0;
}

 
STATIC int
xfs_btree_bload_leaf(
	struct xfs_btree_cur		*cur,
	unsigned int			recs_this_block,
	xfs_btree_bload_get_record_fn	get_record,
	struct xfs_btree_block		*block,
	void				*priv)
{
	unsigned int			j;
	int				ret;

	 
	for (j = 1; j <= recs_this_block; j++) {
		union xfs_btree_rec	*block_rec;

		ret = get_record(cur, priv);
		if (ret)
			return ret;
		block_rec = xfs_btree_rec_addr(cur, j, block);
		cur->bc_ops->init_rec_from_cur(cur, block_rec);
	}

	return 0;
}

 
STATIC int
xfs_btree_bload_node(
	struct xfs_btree_cur	*cur,
	unsigned int		recs_this_block,
	union xfs_btree_ptr	*child_ptr,
	struct xfs_btree_block	*block)
{
	unsigned int		j;
	int			ret;

	 
	for (j = 1; j <= recs_this_block; j++) {
		union xfs_btree_key	child_key;
		union xfs_btree_ptr	*block_ptr;
		union xfs_btree_key	*block_key;
		struct xfs_btree_block	*child_block;
		struct xfs_buf		*child_bp;

		ASSERT(!xfs_btree_ptr_is_null(cur, child_ptr));

		ret = xfs_btree_get_buf_block(cur, child_ptr, &child_block,
				&child_bp);
		if (ret)
			return ret;

		block_ptr = xfs_btree_ptr_addr(cur, j, block);
		xfs_btree_copy_ptrs(cur, block_ptr, child_ptr, 1);

		block_key = xfs_btree_key_addr(cur, j, block);
		xfs_btree_get_keys(cur, child_block, &child_key);
		xfs_btree_copy_keys(cur, block_key, &child_key, 1);

		xfs_btree_get_sibling(cur, child_block, child_ptr,
				XFS_BB_RIGHTSIB);
		xfs_buf_relse(child_bp);
	}

	return 0;
}

 
STATIC unsigned int
xfs_btree_bload_max_npb(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_bload	*bbl,
	unsigned int		level)
{
	unsigned int		ret;

	if (level == cur->bc_nlevels - 1 && cur->bc_ops->get_dmaxrecs)
		return cur->bc_ops->get_dmaxrecs(cur, level);

	ret = cur->bc_ops->get_maxrecs(cur, level);
	if (level == 0)
		ret -= bbl->leaf_slack;
	else
		ret -= bbl->node_slack;
	return ret;
}

 
STATIC unsigned int
xfs_btree_bload_desired_npb(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_bload	*bbl,
	unsigned int		level)
{
	unsigned int		npb = xfs_btree_bload_max_npb(cur, bbl, level);

	 
	if (level == cur->bc_nlevels - 1)
		return max(1U, npb);

	return max_t(unsigned int, cur->bc_ops->get_minrecs(cur, level), npb);
}

 
STATIC void
xfs_btree_bload_level_geometry(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_bload	*bbl,
	unsigned int		level,
	uint64_t		nr_this_level,
	unsigned int		*avg_per_block,
	uint64_t		*blocks,
	uint64_t		*blocks_with_extra)
{
	uint64_t		npb;
	uint64_t		dontcare;
	unsigned int		desired_npb;
	unsigned int		maxnr;

	maxnr = cur->bc_ops->get_maxrecs(cur, level);

	 
	desired_npb = xfs_btree_bload_desired_npb(cur, bbl, level);
	*blocks = div64_u64_rem(nr_this_level, desired_npb, &dontcare);
	*blocks = max(1ULL, *blocks);

	 
	npb = div64_u64_rem(nr_this_level, *blocks, blocks_with_extra);
	if (npb > maxnr || (npb == maxnr && *blocks_with_extra > 0)) {
		(*blocks)++;
		npb = div64_u64_rem(nr_this_level, *blocks, blocks_with_extra);
	}

	*avg_per_block = min_t(uint64_t, npb, nr_this_level);

	trace_xfs_btree_bload_level_geometry(cur, level, nr_this_level,
			*avg_per_block, desired_npb, *blocks,
			*blocks_with_extra);
}

 
static void
xfs_btree_bload_ensure_slack(
	struct xfs_btree_cur	*cur,
	int			*slack,
	int			level)
{
	int			maxr;
	int			minr;

	maxr = cur->bc_ops->get_maxrecs(cur, level);
	minr = cur->bc_ops->get_minrecs(cur, level);

	 
	if (*slack < 0)
		*slack = maxr - ((maxr + minr) >> 1);

	*slack = min(*slack, maxr - minr);
}

 
int
xfs_btree_bload_compute_geometry(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_bload	*bbl,
	uint64_t		nr_records)
{
	uint64_t		nr_blocks = 0;
	uint64_t		nr_this_level;

	ASSERT(cur->bc_flags & XFS_BTREE_STAGING);

	 
	cur->bc_nlevels = cur->bc_maxlevels - 1;
	xfs_btree_bload_ensure_slack(cur, &bbl->leaf_slack, 0);
	xfs_btree_bload_ensure_slack(cur, &bbl->node_slack, 1);

	bbl->nr_records = nr_this_level = nr_records;
	for (cur->bc_nlevels = 1; cur->bc_nlevels <= cur->bc_maxlevels;) {
		uint64_t	level_blocks;
		uint64_t	dontcare64;
		unsigned int	level = cur->bc_nlevels - 1;
		unsigned int	avg_per_block;

		xfs_btree_bload_level_geometry(cur, bbl, level, nr_this_level,
				&avg_per_block, &level_blocks, &dontcare64);

		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
			 
			if (level != 0 && nr_this_level <= avg_per_block) {
				nr_blocks++;
				break;
			}

			 
			cur->bc_nlevels++;
			ASSERT(cur->bc_nlevels <= cur->bc_maxlevels);
			xfs_btree_bload_level_geometry(cur, bbl, level,
					nr_this_level, &avg_per_block,
					&level_blocks, &dontcare64);
		} else {
			 
			if (nr_this_level <= avg_per_block) {
				nr_blocks++;
				break;
			}

			 
			cur->bc_nlevels++;
			ASSERT(cur->bc_nlevels <= cur->bc_maxlevels);
		}

		nr_blocks += level_blocks;
		nr_this_level = level_blocks;
	}

	if (cur->bc_nlevels > cur->bc_maxlevels)
		return -EOVERFLOW;

	bbl->btree_height = cur->bc_nlevels;
	if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
		bbl->nr_blocks = nr_blocks - 1;
	else
		bbl->nr_blocks = nr_blocks;
	return 0;
}

 
int
xfs_btree_bload(
	struct xfs_btree_cur		*cur,
	struct xfs_btree_bload		*bbl,
	void				*priv)
{
	struct list_head		buffers_list;
	union xfs_btree_ptr		child_ptr;
	union xfs_btree_ptr		ptr;
	struct xfs_buf			*bp = NULL;
	struct xfs_btree_block		*block = NULL;
	uint64_t			nr_this_level = bbl->nr_records;
	uint64_t			blocks;
	uint64_t			i;
	uint64_t			blocks_with_extra;
	uint64_t			total_blocks = 0;
	unsigned int			avg_per_block;
	unsigned int			level = 0;
	int				ret;

	ASSERT(cur->bc_flags & XFS_BTREE_STAGING);

	INIT_LIST_HEAD(&buffers_list);
	cur->bc_nlevels = bbl->btree_height;
	xfs_btree_set_ptr_null(cur, &child_ptr);
	xfs_btree_set_ptr_null(cur, &ptr);

	xfs_btree_bload_level_geometry(cur, bbl, level, nr_this_level,
			&avg_per_block, &blocks, &blocks_with_extra);

	 
	for (i = 0; i < blocks; i++) {
		unsigned int		nr_this_block = avg_per_block;

		 
		if (i < blocks_with_extra)
			nr_this_block++;

		ret = xfs_btree_bload_prep_block(cur, bbl, &buffers_list, level,
				nr_this_block, &ptr, &bp, &block, priv);
		if (ret)
			goto out;

		trace_xfs_btree_bload_block(cur, level, i, blocks, &ptr,
				nr_this_block);

		ret = xfs_btree_bload_leaf(cur, nr_this_block, bbl->get_record,
				block, priv);
		if (ret)
			goto out;

		 
		if (i == 0)
			xfs_btree_copy_ptrs(cur, &child_ptr, &ptr, 1);
	}
	total_blocks += blocks;
	xfs_btree_bload_drop_buf(&buffers_list, &bp);

	 
	for (level = 1; level < cur->bc_nlevels; level++) {
		union xfs_btree_ptr	first_ptr;

		nr_this_level = blocks;
		block = NULL;
		xfs_btree_set_ptr_null(cur, &ptr);

		xfs_btree_bload_level_geometry(cur, bbl, level, nr_this_level,
				&avg_per_block, &blocks, &blocks_with_extra);

		 
		for (i = 0; i < blocks; i++) {
			unsigned int	nr_this_block = avg_per_block;

			if (i < blocks_with_extra)
				nr_this_block++;

			ret = xfs_btree_bload_prep_block(cur, bbl,
					&buffers_list, level, nr_this_block,
					&ptr, &bp, &block, priv);
			if (ret)
				goto out;

			trace_xfs_btree_bload_block(cur, level, i, blocks,
					&ptr, nr_this_block);

			ret = xfs_btree_bload_node(cur, nr_this_block,
					&child_ptr, block);
			if (ret)
				goto out;

			 
			if (i == 0)
				xfs_btree_copy_ptrs(cur, &first_ptr, &ptr, 1);
		}
		total_blocks += blocks;
		xfs_btree_bload_drop_buf(&buffers_list, &bp);
		xfs_btree_copy_ptrs(cur, &child_ptr, &first_ptr, 1);
	}

	 
	if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
		ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
		cur->bc_ino.ifake->if_levels = cur->bc_nlevels;
		cur->bc_ino.ifake->if_blocks = total_blocks - 1;
	} else {
		cur->bc_ag.afake->af_root = be32_to_cpu(ptr.s);
		cur->bc_ag.afake->af_levels = cur->bc_nlevels;
		cur->bc_ag.afake->af_blocks = total_blocks;
	}

	 
	ret = xfs_buf_delwri_submit(&buffers_list);
	if (ret)
		goto out;
	if (!list_empty(&buffers_list)) {
		ASSERT(list_empty(&buffers_list));
		ret = -EIO;
	}

out:
	xfs_buf_delwri_cancel(&buffers_list);
	if (bp)
		xfs_buf_relse(bp);
	return ret;
}


 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_da_format.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "xfs_attr_sf.h"
#include "xfs_attr_leaf.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_dir2.h"

STATIC int
xfs_attr_shortform_compare(const void *a, const void *b)
{
	xfs_attr_sf_sort_t *sa, *sb;

	sa = (xfs_attr_sf_sort_t *)a;
	sb = (xfs_attr_sf_sort_t *)b;
	if (sa->hash < sb->hash) {
		return -1;
	} else if (sa->hash > sb->hash) {
		return 1;
	} else {
		return sa->entno - sb->entno;
	}
}

#define XFS_ISRESET_CURSOR(cursor) \
	(!((cursor)->initted) && !((cursor)->hashval) && \
	 !((cursor)->blkno) && !((cursor)->offset))
 
static int
xfs_attr_shortform_list(
	struct xfs_attr_list_context	*context)
{
	struct xfs_attrlist_cursor_kern	*cursor = &context->cursor;
	struct xfs_inode		*dp = context->dp;
	struct xfs_attr_sf_sort		*sbuf, *sbp;
	struct xfs_attr_shortform	*sf;
	struct xfs_attr_sf_entry	*sfe;
	int				sbsize, nsbuf, count, i;
	int				error = 0;

	sf = (struct xfs_attr_shortform *)dp->i_af.if_u1.if_data;
	ASSERT(sf != NULL);
	if (!sf->hdr.count)
		return 0;

	trace_xfs_attr_list_sf(context);

	 
	if (context->bufsize == 0 ||
	    (XFS_ISRESET_CURSOR(cursor) &&
	     (dp->i_af.if_bytes + sf->hdr.count * 16) < context->bufsize)) {
		for (i = 0, sfe = &sf->list[0]; i < sf->hdr.count; i++) {
			if (XFS_IS_CORRUPT(context->dp->i_mount,
					   !xfs_attr_namecheck(sfe->nameval,
							       sfe->namelen)))
				return -EFSCORRUPTED;
			context->put_listent(context,
					     sfe->flags,
					     sfe->nameval,
					     (int)sfe->namelen,
					     (int)sfe->valuelen);
			 
			if (context->seen_enough)
				break;
			sfe = xfs_attr_sf_nextentry(sfe);
		}
		trace_xfs_attr_list_sf_all(context);
		return 0;
	}

	 
	if (context->bufsize == 0)
		return 0;

	 
	sbsize = sf->hdr.count * sizeof(*sbuf);
	sbp = sbuf = kmem_alloc(sbsize, KM_NOFS);

	 
	nsbuf = 0;
	for (i = 0, sfe = &sf->list[0]; i < sf->hdr.count; i++) {
		if (unlikely(
		    ((char *)sfe < (char *)sf) ||
		    ((char *)sfe >= ((char *)sf + dp->i_af.if_bytes)))) {
			XFS_CORRUPTION_ERROR("xfs_attr_shortform_list",
					     XFS_ERRLEVEL_LOW,
					     context->dp->i_mount, sfe,
					     sizeof(*sfe));
			kmem_free(sbuf);
			return -EFSCORRUPTED;
		}

		sbp->entno = i;
		sbp->hash = xfs_da_hashname(sfe->nameval, sfe->namelen);
		sbp->name = sfe->nameval;
		sbp->namelen = sfe->namelen;
		 
		sbp->valuelen = sfe->valuelen;
		sbp->flags = sfe->flags;
		sfe = xfs_attr_sf_nextentry(sfe);
		sbp++;
		nsbuf++;
	}

	 
	xfs_sort(sbuf, nsbuf, sizeof(*sbuf), xfs_attr_shortform_compare);

	 
	count = 0;
	cursor->initted = 1;
	cursor->blkno = 0;
	for (sbp = sbuf, i = 0; i < nsbuf; i++, sbp++) {
		if (sbp->hash == cursor->hashval) {
			if (cursor->offset == count) {
				break;
			}
			count++;
		} else if (sbp->hash > cursor->hashval) {
			break;
		}
	}
	if (i == nsbuf)
		goto out;

	 
	for ( ; i < nsbuf; i++, sbp++) {
		if (cursor->hashval != sbp->hash) {
			cursor->hashval = sbp->hash;
			cursor->offset = 0;
		}
		if (XFS_IS_CORRUPT(context->dp->i_mount,
				   !xfs_attr_namecheck(sbp->name,
						       sbp->namelen))) {
			error = -EFSCORRUPTED;
			goto out;
		}
		context->put_listent(context,
				     sbp->flags,
				     sbp->name,
				     sbp->namelen,
				     sbp->valuelen);
		if (context->seen_enough)
			break;
		cursor->offset++;
	}
out:
	kmem_free(sbuf);
	return error;
}

 
STATIC int
xfs_attr_node_list_lookup(
	struct xfs_attr_list_context	*context,
	struct xfs_attrlist_cursor_kern	*cursor,
	struct xfs_buf			**pbp)
{
	struct xfs_da3_icnode_hdr	nodehdr;
	struct xfs_da_intnode		*node;
	struct xfs_da_node_entry	*btree;
	struct xfs_inode		*dp = context->dp;
	struct xfs_mount		*mp = dp->i_mount;
	struct xfs_trans		*tp = context->tp;
	struct xfs_buf			*bp;
	int				i;
	int				error = 0;
	unsigned int			expected_level = 0;
	uint16_t			magic;

	ASSERT(*pbp == NULL);
	cursor->blkno = 0;
	for (;;) {
		error = xfs_da3_node_read(tp, dp, cursor->blkno, &bp,
				XFS_ATTR_FORK);
		if (error)
			return error;
		node = bp->b_addr;
		magic = be16_to_cpu(node->hdr.info.magic);
		if (magic == XFS_ATTR_LEAF_MAGIC ||
		    magic == XFS_ATTR3_LEAF_MAGIC)
			break;
		if (magic != XFS_DA_NODE_MAGIC &&
		    magic != XFS_DA3_NODE_MAGIC) {
			XFS_CORRUPTION_ERROR(__func__, XFS_ERRLEVEL_LOW, mp,
					node, sizeof(*node));
			goto out_corruptbuf;
		}

		xfs_da3_node_hdr_from_disk(mp, &nodehdr, node);

		 
		if (nodehdr.level >= XFS_DA_NODE_MAXDEPTH)
			goto out_corruptbuf;

		 
		if (cursor->blkno == 0)
			expected_level = nodehdr.level - 1;
		else if (expected_level != nodehdr.level)
			goto out_corruptbuf;
		else
			expected_level--;

		btree = nodehdr.btree;
		for (i = 0; i < nodehdr.count; btree++, i++) {
			if (cursor->hashval <= be32_to_cpu(btree->hashval)) {
				cursor->blkno = be32_to_cpu(btree->before);
				trace_xfs_attr_list_node_descend(context,
						btree);
				break;
			}
		}
		xfs_trans_brelse(tp, bp);

		if (i == nodehdr.count)
			return 0;

		 
		if (XFS_IS_CORRUPT(mp, cursor->blkno == 0))
			return -EFSCORRUPTED;
	}

	if (expected_level != 0)
		goto out_corruptbuf;

	*pbp = bp;
	return 0;

out_corruptbuf:
	xfs_buf_mark_corrupt(bp);
	xfs_trans_brelse(tp, bp);
	return -EFSCORRUPTED;
}

STATIC int
xfs_attr_node_list(
	struct xfs_attr_list_context	*context)
{
	struct xfs_attrlist_cursor_kern	*cursor = &context->cursor;
	struct xfs_attr3_icleaf_hdr	leafhdr;
	struct xfs_attr_leafblock	*leaf;
	struct xfs_da_intnode		*node;
	struct xfs_buf			*bp;
	struct xfs_inode		*dp = context->dp;
	struct xfs_mount		*mp = dp->i_mount;
	int				error = 0;

	trace_xfs_attr_node_list(context);

	cursor->initted = 1;

	 
	bp = NULL;
	if (cursor->blkno > 0) {
		error = xfs_da3_node_read(context->tp, dp, cursor->blkno, &bp,
				XFS_ATTR_FORK);
		if ((error != 0) && (error != -EFSCORRUPTED))
			return error;
		if (bp) {
			struct xfs_attr_leaf_entry *entries;

			node = bp->b_addr;
			switch (be16_to_cpu(node->hdr.info.magic)) {
			case XFS_DA_NODE_MAGIC:
			case XFS_DA3_NODE_MAGIC:
				trace_xfs_attr_list_wrong_blk(context);
				xfs_trans_brelse(context->tp, bp);
				bp = NULL;
				break;
			case XFS_ATTR_LEAF_MAGIC:
			case XFS_ATTR3_LEAF_MAGIC:
				leaf = bp->b_addr;
				xfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo,
							     &leafhdr, leaf);
				entries = xfs_attr3_leaf_entryp(leaf);
				if (cursor->hashval > be32_to_cpu(
						entries[leafhdr.count - 1].hashval)) {
					trace_xfs_attr_list_wrong_blk(context);
					xfs_trans_brelse(context->tp, bp);
					bp = NULL;
				} else if (cursor->hashval <= be32_to_cpu(
						entries[0].hashval)) {
					trace_xfs_attr_list_wrong_blk(context);
					xfs_trans_brelse(context->tp, bp);
					bp = NULL;
				}
				break;
			default:
				trace_xfs_attr_list_wrong_blk(context);
				xfs_trans_brelse(context->tp, bp);
				bp = NULL;
			}
		}
	}

	 
	if (bp == NULL) {
		error = xfs_attr_node_list_lookup(context, cursor, &bp);
		if (error || !bp)
			return error;
	}
	ASSERT(bp != NULL);

	 
	for (;;) {
		leaf = bp->b_addr;
		error = xfs_attr3_leaf_list_int(bp, context);
		if (error)
			break;
		xfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo, &leafhdr, leaf);
		if (context->seen_enough || leafhdr.forw == 0)
			break;
		cursor->blkno = leafhdr.forw;
		xfs_trans_brelse(context->tp, bp);
		error = xfs_attr3_leaf_read(context->tp, dp, cursor->blkno,
					    &bp);
		if (error)
			return error;
	}
	xfs_trans_brelse(context->tp, bp);
	return error;
}

 
int
xfs_attr3_leaf_list_int(
	struct xfs_buf			*bp,
	struct xfs_attr_list_context	*context)
{
	struct xfs_attrlist_cursor_kern	*cursor = &context->cursor;
	struct xfs_attr_leafblock	*leaf;
	struct xfs_attr3_icleaf_hdr	ichdr;
	struct xfs_attr_leaf_entry	*entries;
	struct xfs_attr_leaf_entry	*entry;
	int				i;
	struct xfs_mount		*mp = context->dp->i_mount;

	trace_xfs_attr_list_leaf(context);

	leaf = bp->b_addr;
	xfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo, &ichdr, leaf);
	entries = xfs_attr3_leaf_entryp(leaf);

	cursor->initted = 1;

	 
	if (context->resynch) {
		entry = &entries[0];
		for (i = 0; i < ichdr.count; entry++, i++) {
			if (be32_to_cpu(entry->hashval) == cursor->hashval) {
				if (cursor->offset == context->dupcnt) {
					context->dupcnt = 0;
					break;
				}
				context->dupcnt++;
			} else if (be32_to_cpu(entry->hashval) >
					cursor->hashval) {
				context->dupcnt = 0;
				break;
			}
		}
		if (i == ichdr.count) {
			trace_xfs_attr_list_notfound(context);
			return 0;
		}
	} else {
		entry = &entries[0];
		i = 0;
	}
	context->resynch = 0;

	 
	for (; i < ichdr.count; entry++, i++) {
		char *name;
		int namelen, valuelen;

		if (be32_to_cpu(entry->hashval) != cursor->hashval) {
			cursor->hashval = be32_to_cpu(entry->hashval);
			cursor->offset = 0;
		}

		if ((entry->flags & XFS_ATTR_INCOMPLETE) &&
		    !context->allow_incomplete)
			continue;

		if (entry->flags & XFS_ATTR_LOCAL) {
			xfs_attr_leaf_name_local_t *name_loc;

			name_loc = xfs_attr3_leaf_name_local(leaf, i);
			name = name_loc->nameval;
			namelen = name_loc->namelen;
			valuelen = be16_to_cpu(name_loc->valuelen);
		} else {
			xfs_attr_leaf_name_remote_t *name_rmt;

			name_rmt = xfs_attr3_leaf_name_remote(leaf, i);
			name = name_rmt->name;
			namelen = name_rmt->namelen;
			valuelen = be32_to_cpu(name_rmt->valuelen);
		}

		if (XFS_IS_CORRUPT(context->dp->i_mount,
				   !xfs_attr_namecheck(name, namelen)))
			return -EFSCORRUPTED;
		context->put_listent(context, entry->flags,
					      name, namelen, valuelen);
		if (context->seen_enough)
			break;
		cursor->offset++;
	}
	trace_xfs_attr_list_leaf_end(context);
	return 0;
}

 
STATIC int
xfs_attr_leaf_list(
	struct xfs_attr_list_context	*context)
{
	struct xfs_buf			*bp;
	int				error;

	trace_xfs_attr_leaf_list(context);

	context->cursor.blkno = 0;
	error = xfs_attr3_leaf_read(context->tp, context->dp, 0, &bp);
	if (error)
		return error;

	error = xfs_attr3_leaf_list_int(bp, context);
	xfs_trans_brelse(context->tp, bp);
	return error;
}

int
xfs_attr_list_ilocked(
	struct xfs_attr_list_context	*context)
{
	struct xfs_inode		*dp = context->dp;

	ASSERT(xfs_isilocked(dp, XFS_ILOCK_SHARED | XFS_ILOCK_EXCL));

	 
	if (!xfs_inode_hasattr(dp))
		return 0;
	if (dp->i_af.if_format == XFS_DINODE_FMT_LOCAL)
		return xfs_attr_shortform_list(context);
	if (xfs_attr_is_leaf(dp))
		return xfs_attr_leaf_list(context);
	return xfs_attr_node_list(context);
}

int
xfs_attr_list(
	struct xfs_attr_list_context	*context)
{
	struct xfs_inode		*dp = context->dp;
	uint				lock_mode;
	int				error;

	XFS_STATS_INC(dp->i_mount, xs_attr_list);

	if (xfs_is_shutdown(dp->i_mount))
		return -EIO;

	lock_mode = xfs_ilock_attr_map_shared(dp);
	error = xfs_attr_list_ilocked(context);
	xfs_iunlock(dp, lock_mode);
	return error;
}

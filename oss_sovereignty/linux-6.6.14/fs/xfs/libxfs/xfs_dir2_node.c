
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_log.h"

 
static int xfs_dir2_leafn_add(struct xfs_buf *bp, xfs_da_args_t *args,
			      int index);
static void xfs_dir2_leafn_rebalance(xfs_da_state_t *state,
				     xfs_da_state_blk_t *blk1,
				     xfs_da_state_blk_t *blk2);
static int xfs_dir2_leafn_remove(xfs_da_args_t *args, struct xfs_buf *bp,
				 int index, xfs_da_state_blk_t *dblk,
				 int *rval);

 
static xfs_dir2_db_t
xfs_dir2_db_to_fdb(struct xfs_da_geometry *geo, xfs_dir2_db_t db)
{
	return xfs_dir2_byte_to_db(geo, XFS_DIR2_FREE_OFFSET) +
			(db / geo->free_max_bests);
}

 
static int
xfs_dir2_db_to_fdindex(struct xfs_da_geometry *geo, xfs_dir2_db_t db)
{
	return db % geo->free_max_bests;
}

 
#ifdef DEBUG
static xfs_failaddr_t
xfs_dir3_leafn_check(
	struct xfs_inode	*dp,
	struct xfs_buf		*bp)
{
	struct xfs_dir2_leaf	*leaf = bp->b_addr;
	struct xfs_dir3_icleaf_hdr leafhdr;

	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &leafhdr, leaf);

	if (leafhdr.magic == XFS_DIR3_LEAFN_MAGIC) {
		struct xfs_dir3_leaf_hdr *leaf3 = bp->b_addr;
		if (be64_to_cpu(leaf3->info.blkno) != xfs_buf_daddr(bp))
			return __this_address;
	} else if (leafhdr.magic != XFS_DIR2_LEAFN_MAGIC)
		return __this_address;

	return xfs_dir3_leaf_check_int(dp->i_mount, &leafhdr, leaf, false);
}

static inline void
xfs_dir3_leaf_check(
	struct xfs_inode	*dp,
	struct xfs_buf		*bp)
{
	xfs_failaddr_t		fa;

	fa = xfs_dir3_leafn_check(dp, bp);
	if (!fa)
		return;
	xfs_corruption_error(__func__, XFS_ERRLEVEL_LOW, dp->i_mount,
			bp->b_addr, BBTOB(bp->b_length), __FILE__, __LINE__,
			fa);
	ASSERT(0);
}
#else
#define	xfs_dir3_leaf_check(dp, bp)
#endif

static xfs_failaddr_t
xfs_dir3_free_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_dir2_free_hdr *hdr = bp->b_addr;

	if (!xfs_verify_magic(bp, hdr->magic))
		return __this_address;

	if (xfs_has_crc(mp)) {
		struct xfs_dir3_blk_hdr *hdr3 = bp->b_addr;

		if (!uuid_equal(&hdr3->uuid, &mp->m_sb.sb_meta_uuid))
			return __this_address;
		if (be64_to_cpu(hdr3->blkno) != xfs_buf_daddr(bp))
			return __this_address;
		if (!xfs_log_check_lsn(mp, be64_to_cpu(hdr3->lsn)))
			return __this_address;
	}

	 

	return NULL;
}

static void
xfs_dir3_free_read_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	xfs_failaddr_t		fa;

	if (xfs_has_crc(mp) &&
	    !xfs_buf_verify_cksum(bp, XFS_DIR3_FREE_CRC_OFF))
		xfs_verifier_error(bp, -EFSBADCRC, __this_address);
	else {
		fa = xfs_dir3_free_verify(bp);
		if (fa)
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
	}
}

static void
xfs_dir3_free_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	struct xfs_dir3_blk_hdr	*hdr3 = bp->b_addr;
	xfs_failaddr_t		fa;

	fa = xfs_dir3_free_verify(bp);
	if (fa) {
		xfs_verifier_error(bp, -EFSCORRUPTED, fa);
		return;
	}

	if (!xfs_has_crc(mp))
		return;

	if (bip)
		hdr3->lsn = cpu_to_be64(bip->bli_item.li_lsn);

	xfs_buf_update_cksum(bp, XFS_DIR3_FREE_CRC_OFF);
}

const struct xfs_buf_ops xfs_dir3_free_buf_ops = {
	.name = "xfs_dir3_free",
	.magic = { cpu_to_be32(XFS_DIR2_FREE_MAGIC),
		   cpu_to_be32(XFS_DIR3_FREE_MAGIC) },
	.verify_read = xfs_dir3_free_read_verify,
	.verify_write = xfs_dir3_free_write_verify,
	.verify_struct = xfs_dir3_free_verify,
};

 
static xfs_failaddr_t
xfs_dir3_free_header_check(
	struct xfs_inode	*dp,
	xfs_dablk_t		fbno,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = dp->i_mount;
	int			maxbests = mp->m_dir_geo->free_max_bests;
	unsigned int		firstdb;

	firstdb = (xfs_dir2_da_to_db(mp->m_dir_geo, fbno) -
		   xfs_dir2_byte_to_db(mp->m_dir_geo, XFS_DIR2_FREE_OFFSET)) *
			maxbests;
	if (xfs_has_crc(mp)) {
		struct xfs_dir3_free_hdr *hdr3 = bp->b_addr;

		if (be32_to_cpu(hdr3->firstdb) != firstdb)
			return __this_address;
		if (be32_to_cpu(hdr3->nvalid) > maxbests)
			return __this_address;
		if (be32_to_cpu(hdr3->nvalid) < be32_to_cpu(hdr3->nused))
			return __this_address;
		if (be64_to_cpu(hdr3->hdr.owner) != dp->i_ino)
			return __this_address;
	} else {
		struct xfs_dir2_free_hdr *hdr = bp->b_addr;

		if (be32_to_cpu(hdr->firstdb) != firstdb)
			return __this_address;
		if (be32_to_cpu(hdr->nvalid) > maxbests)
			return __this_address;
		if (be32_to_cpu(hdr->nvalid) < be32_to_cpu(hdr->nused))
			return __this_address;
	}
	return NULL;
}

static int
__xfs_dir3_free_read(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	xfs_dablk_t		fbno,
	unsigned int		flags,
	struct xfs_buf		**bpp)
{
	xfs_failaddr_t		fa;
	int			err;

	err = xfs_da_read_buf(tp, dp, fbno, flags, bpp, XFS_DATA_FORK,
			&xfs_dir3_free_buf_ops);
	if (err || !*bpp)
		return err;

	 
	fa = xfs_dir3_free_header_check(dp, fbno, *bpp);
	if (fa) {
		__xfs_buf_mark_corrupt(*bpp, fa);
		xfs_trans_brelse(tp, *bpp);
		*bpp = NULL;
		return -EFSCORRUPTED;
	}

	 
	if (tp)
		xfs_trans_buf_set_type(tp, *bpp, XFS_BLFT_DIR_FREE_BUF);

	return 0;
}

void
xfs_dir2_free_hdr_from_disk(
	struct xfs_mount		*mp,
	struct xfs_dir3_icfree_hdr	*to,
	struct xfs_dir2_free		*from)
{
	if (xfs_has_crc(mp)) {
		struct xfs_dir3_free	*from3 = (struct xfs_dir3_free *)from;

		to->magic = be32_to_cpu(from3->hdr.hdr.magic);
		to->firstdb = be32_to_cpu(from3->hdr.firstdb);
		to->nvalid = be32_to_cpu(from3->hdr.nvalid);
		to->nused = be32_to_cpu(from3->hdr.nused);
		to->bests = from3->bests;

		ASSERT(to->magic == XFS_DIR3_FREE_MAGIC);
	} else {
		to->magic = be32_to_cpu(from->hdr.magic);
		to->firstdb = be32_to_cpu(from->hdr.firstdb);
		to->nvalid = be32_to_cpu(from->hdr.nvalid);
		to->nused = be32_to_cpu(from->hdr.nused);
		to->bests = from->bests;

		ASSERT(to->magic == XFS_DIR2_FREE_MAGIC);
	}
}

static void
xfs_dir2_free_hdr_to_disk(
	struct xfs_mount		*mp,
	struct xfs_dir2_free		*to,
	struct xfs_dir3_icfree_hdr	*from)
{
	if (xfs_has_crc(mp)) {
		struct xfs_dir3_free	*to3 = (struct xfs_dir3_free *)to;

		ASSERT(from->magic == XFS_DIR3_FREE_MAGIC);

		to3->hdr.hdr.magic = cpu_to_be32(from->magic);
		to3->hdr.firstdb = cpu_to_be32(from->firstdb);
		to3->hdr.nvalid = cpu_to_be32(from->nvalid);
		to3->hdr.nused = cpu_to_be32(from->nused);
	} else {
		ASSERT(from->magic == XFS_DIR2_FREE_MAGIC);

		to->hdr.magic = cpu_to_be32(from->magic);
		to->hdr.firstdb = cpu_to_be32(from->firstdb);
		to->hdr.nvalid = cpu_to_be32(from->nvalid);
		to->hdr.nused = cpu_to_be32(from->nused);
	}
}

int
xfs_dir2_free_read(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	xfs_dablk_t		fbno,
	struct xfs_buf		**bpp)
{
	return __xfs_dir3_free_read(tp, dp, fbno, 0, bpp);
}

static int
xfs_dir2_free_try_read(
	struct xfs_trans	*tp,
	struct xfs_inode	*dp,
	xfs_dablk_t		fbno,
	struct xfs_buf		**bpp)
{
	return __xfs_dir3_free_read(tp, dp, fbno, XFS_DABUF_MAP_HOLE_OK, bpp);
}

static int
xfs_dir3_free_get_buf(
	xfs_da_args_t		*args,
	xfs_dir2_db_t		fbno,
	struct xfs_buf		**bpp)
{
	struct xfs_trans	*tp = args->trans;
	struct xfs_inode	*dp = args->dp;
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_buf		*bp;
	int			error;
	struct xfs_dir3_icfree_hdr hdr;

	error = xfs_da_get_buf(tp, dp, xfs_dir2_db_to_da(args->geo, fbno),
			&bp, XFS_DATA_FORK);
	if (error)
		return error;

	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_DIR_FREE_BUF);
	bp->b_ops = &xfs_dir3_free_buf_ops;

	 
	memset(bp->b_addr, 0, sizeof(struct xfs_dir3_free_hdr));
	memset(&hdr, 0, sizeof(hdr));

	if (xfs_has_crc(mp)) {
		struct xfs_dir3_free_hdr *hdr3 = bp->b_addr;

		hdr.magic = XFS_DIR3_FREE_MAGIC;

		hdr3->hdr.blkno = cpu_to_be64(xfs_buf_daddr(bp));
		hdr3->hdr.owner = cpu_to_be64(dp->i_ino);
		uuid_copy(&hdr3->hdr.uuid, &mp->m_sb.sb_meta_uuid);
	} else
		hdr.magic = XFS_DIR2_FREE_MAGIC;
	xfs_dir2_free_hdr_to_disk(mp, bp->b_addr, &hdr);
	*bpp = bp;
	return 0;
}

 
STATIC void
xfs_dir2_free_log_bests(
	struct xfs_da_args	*args,
	struct xfs_dir3_icfree_hdr *hdr,
	struct xfs_buf		*bp,
	int			first,		 
	int			last)		 
{
	struct xfs_dir2_free	*free = bp->b_addr;

	ASSERT(free->hdr.magic == cpu_to_be32(XFS_DIR2_FREE_MAGIC) ||
	       free->hdr.magic == cpu_to_be32(XFS_DIR3_FREE_MAGIC));
	xfs_trans_log_buf(args->trans, bp,
			  (char *)&hdr->bests[first] - (char *)free,
			  (char *)&hdr->bests[last] - (char *)free +
			   sizeof(hdr->bests[0]) - 1);
}

 
static void
xfs_dir2_free_log_header(
	struct xfs_da_args	*args,
	struct xfs_buf		*bp)
{
#ifdef DEBUG
	xfs_dir2_free_t		*free;		 

	free = bp->b_addr;
	ASSERT(free->hdr.magic == cpu_to_be32(XFS_DIR2_FREE_MAGIC) ||
	       free->hdr.magic == cpu_to_be32(XFS_DIR3_FREE_MAGIC));
#endif
	xfs_trans_log_buf(args->trans, bp, 0,
			  args->geo->free_hdr_size - 1);
}

 
int						 
xfs_dir2_leaf_to_node(
	xfs_da_args_t		*args,		 
	struct xfs_buf		*lbp)		 
{
	xfs_inode_t		*dp;		 
	int			error;		 
	struct xfs_buf		*fbp;		 
	xfs_dir2_db_t		fdb;		 
	__be16			*from;		 
	int			i;		 
	xfs_dir2_leaf_t		*leaf;		 
	xfs_dir2_leaf_tail_t	*ltp;		 
	int			n;		 
	xfs_dir2_data_off_t	off;		 
	xfs_trans_t		*tp;		 
	struct xfs_dir3_icfree_hdr freehdr;

	trace_xfs_dir2_leaf_to_node(args);

	dp = args->dp;
	tp = args->trans;
	 
	if ((error = xfs_dir2_grow_inode(args, XFS_DIR2_FREE_SPACE, &fdb))) {
		return error;
	}
	ASSERT(fdb == xfs_dir2_byte_to_db(args->geo, XFS_DIR2_FREE_OFFSET));
	 
	error = xfs_dir3_free_get_buf(args, fdb, &fbp);
	if (error)
		return error;

	xfs_dir2_free_hdr_from_disk(dp->i_mount, &freehdr, fbp->b_addr);
	leaf = lbp->b_addr;
	ltp = xfs_dir2_leaf_tail_p(args->geo, leaf);
	if (be32_to_cpu(ltp->bestcount) >
				(uint)dp->i_disk_size / args->geo->blksize) {
		xfs_buf_mark_corrupt(lbp);
		return -EFSCORRUPTED;
	}

	 
	from = xfs_dir2_leaf_bests_p(ltp);
	for (i = n = 0; i < be32_to_cpu(ltp->bestcount); i++, from++) {
		off = be16_to_cpu(*from);
		if (off != NULLDATAOFF)
			n++;
		freehdr.bests[i] = cpu_to_be16(off);
	}

	 
	freehdr.nused = n;
	freehdr.nvalid = be32_to_cpu(ltp->bestcount);

	xfs_dir2_free_hdr_to_disk(dp->i_mount, fbp->b_addr, &freehdr);
	xfs_dir2_free_log_bests(args, &freehdr, fbp, 0, freehdr.nvalid - 1);
	xfs_dir2_free_log_header(args, fbp);

	 
	if (leaf->hdr.info.magic == cpu_to_be16(XFS_DIR2_LEAF1_MAGIC))
		leaf->hdr.info.magic = cpu_to_be16(XFS_DIR2_LEAFN_MAGIC);
	else
		leaf->hdr.info.magic = cpu_to_be16(XFS_DIR3_LEAFN_MAGIC);
	lbp->b_ops = &xfs_dir3_leafn_buf_ops;
	xfs_trans_buf_set_type(tp, lbp, XFS_BLFT_DIR_LEAFN_BUF);
	xfs_dir3_leaf_log_header(args, lbp);
	xfs_dir3_leaf_check(dp, lbp);
	return 0;
}

 
static int					 
xfs_dir2_leafn_add(
	struct xfs_buf		*bp,		 
	struct xfs_da_args	*args,		 
	int			index)		 
{
	struct xfs_dir3_icleaf_hdr leafhdr;
	struct xfs_inode	*dp = args->dp;
	struct xfs_dir2_leaf	*leaf = bp->b_addr;
	struct xfs_dir2_leaf_entry *lep;
	struct xfs_dir2_leaf_entry *ents;
	int			compact;	 
	int			highstale = 0;	 
	int			lfloghigh;	 
	int			lfloglow;	 
	int			lowstale = 0;	 

	trace_xfs_dir2_leafn_add(args, index);

	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &leafhdr, leaf);
	ents = leafhdr.ents;

	 
	if (index < 0) {
		xfs_buf_mark_corrupt(bp);
		return -EFSCORRUPTED;
	}

	 

	if (leafhdr.count == args->geo->leaf_max_ents) {
		if (!leafhdr.stale)
			return -ENOSPC;
		compact = leafhdr.stale > 1;
	} else
		compact = 0;
	ASSERT(index == 0 || be32_to_cpu(ents[index - 1].hashval) <= args->hashval);
	ASSERT(index == leafhdr.count ||
	       be32_to_cpu(ents[index].hashval) >= args->hashval);

	if (args->op_flags & XFS_DA_OP_JUSTCHECK)
		return 0;

	 
	if (compact)
		xfs_dir3_leaf_compact_x1(&leafhdr, ents, &index, &lowstale,
					 &highstale, &lfloglow, &lfloghigh);
	else if (leafhdr.stale) {
		 
		lfloglow = leafhdr.count;
		lfloghigh = -1;
	}

	 
	lep = xfs_dir3_leaf_find_entry(&leafhdr, ents, index, compact, lowstale,
				       highstale, &lfloglow, &lfloghigh);

	lep->hashval = cpu_to_be32(args->hashval);
	lep->address = cpu_to_be32(xfs_dir2_db_off_to_dataptr(args->geo,
				args->blkno, args->index));

	xfs_dir2_leaf_hdr_to_disk(dp->i_mount, leaf, &leafhdr);
	xfs_dir3_leaf_log_header(args, bp);
	xfs_dir3_leaf_log_ents(args, &leafhdr, bp, lfloglow, lfloghigh);
	xfs_dir3_leaf_check(dp, bp);
	return 0;
}

#ifdef DEBUG
static void
xfs_dir2_free_hdr_check(
	struct xfs_inode *dp,
	struct xfs_buf	*bp,
	xfs_dir2_db_t	db)
{
	struct xfs_dir3_icfree_hdr hdr;

	xfs_dir2_free_hdr_from_disk(dp->i_mount, &hdr, bp->b_addr);

	ASSERT((hdr.firstdb % dp->i_mount->m_dir_geo->free_max_bests) == 0);
	ASSERT(hdr.firstdb <= db);
	ASSERT(db < hdr.firstdb + hdr.nvalid);
}
#else
#define xfs_dir2_free_hdr_check(dp, bp, db)
#endif	 

 
xfs_dahash_t					 
xfs_dir2_leaf_lasthash(
	struct xfs_inode *dp,
	struct xfs_buf	*bp,			 
	int		*count)			 
{
	struct xfs_dir3_icleaf_hdr leafhdr;

	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &leafhdr, bp->b_addr);

	ASSERT(leafhdr.magic == XFS_DIR2_LEAFN_MAGIC ||
	       leafhdr.magic == XFS_DIR3_LEAFN_MAGIC ||
	       leafhdr.magic == XFS_DIR2_LEAF1_MAGIC ||
	       leafhdr.magic == XFS_DIR3_LEAF1_MAGIC);

	if (count)
		*count = leafhdr.count;
	if (!leafhdr.count)
		return 0;
	return be32_to_cpu(leafhdr.ents[leafhdr.count - 1].hashval);
}

 
STATIC int
xfs_dir2_leafn_lookup_for_addname(
	struct xfs_buf		*bp,		 
	xfs_da_args_t		*args,		 
	int			*indexp,	 
	xfs_da_state_t		*state)		 
{
	struct xfs_buf		*curbp = NULL;	 
	xfs_dir2_db_t		curdb = -1;	 
	xfs_dir2_db_t		curfdb = -1;	 
	xfs_inode_t		*dp;		 
	int			error;		 
	int			fi;		 
	xfs_dir2_free_t		*free = NULL;	 
	int			index;		 
	xfs_dir2_leaf_t		*leaf;		 
	int			length;		 
	xfs_dir2_leaf_entry_t	*lep;		 
	xfs_mount_t		*mp;		 
	xfs_dir2_db_t		newdb;		 
	xfs_dir2_db_t		newfdb;		 
	xfs_trans_t		*tp;		 
	struct xfs_dir3_icleaf_hdr leafhdr;

	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	leaf = bp->b_addr;
	xfs_dir2_leaf_hdr_from_disk(mp, &leafhdr, leaf);

	xfs_dir3_leaf_check(dp, bp);
	ASSERT(leafhdr.count > 0);

	 
	index = xfs_dir2_leaf_search_hash(args, bp);
	 
	if (state->extravalid) {
		 
		curbp = state->extrablk.bp;
		curfdb = state->extrablk.blkno;
		free = curbp->b_addr;
		ASSERT(free->hdr.magic == cpu_to_be32(XFS_DIR2_FREE_MAGIC) ||
		       free->hdr.magic == cpu_to_be32(XFS_DIR3_FREE_MAGIC));
	}
	length = xfs_dir2_data_entsize(mp, args->namelen);
	 
	for (lep = &leafhdr.ents[index];
	     index < leafhdr.count && be32_to_cpu(lep->hashval) == args->hashval;
	     lep++, index++) {
		 
		if (be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR)
			continue;
		 
		newdb = xfs_dir2_dataptr_to_db(args->geo,
					       be32_to_cpu(lep->address));
		 
		if (newdb != curdb) {
			struct xfs_dir3_icfree_hdr freehdr;

			curdb = newdb;
			 
			newfdb = xfs_dir2_db_to_fdb(args->geo, newdb);
			 
			if (newfdb != curfdb) {
				 
				if (curbp)
					xfs_trans_brelse(tp, curbp);

				error = xfs_dir2_free_read(tp, dp,
						xfs_dir2_db_to_da(args->geo,
								  newfdb),
						&curbp);
				if (error)
					return error;
				free = curbp->b_addr;

				xfs_dir2_free_hdr_check(dp, curbp, curdb);
			}
			 
			fi = xfs_dir2_db_to_fdindex(args->geo, curdb);
			 
			xfs_dir2_free_hdr_from_disk(mp, &freehdr, free);
			if (XFS_IS_CORRUPT(mp,
					   freehdr.bests[fi] ==
					   cpu_to_be16(NULLDATAOFF))) {
				if (curfdb != newfdb)
					xfs_trans_brelse(tp, curbp);
				return -EFSCORRUPTED;
			}
			curfdb = newfdb;
			if (be16_to_cpu(freehdr.bests[fi]) >= length)
				goto out;
		}
	}
	 
	fi = -1;
out:
	ASSERT(args->op_flags & XFS_DA_OP_OKNOENT);
	if (curbp) {
		 
		state->extravalid = 1;
		state->extrablk.bp = curbp;
		state->extrablk.index = fi;
		state->extrablk.blkno = curfdb;

		 
		state->extrablk.magic = XFS_DIR2_FREE_MAGIC;
	} else {
		state->extravalid = 0;
	}
	 
	*indexp = index;
	return -ENOENT;
}

 
STATIC int
xfs_dir2_leafn_lookup_for_entry(
	struct xfs_buf		*bp,		 
	xfs_da_args_t		*args,		 
	int			*indexp,	 
	xfs_da_state_t		*state)		 
{
	struct xfs_buf		*curbp = NULL;	 
	xfs_dir2_db_t		curdb = -1;	 
	xfs_dir2_data_entry_t	*dep;		 
	xfs_inode_t		*dp;		 
	int			error;		 
	int			index;		 
	xfs_dir2_leaf_t		*leaf;		 
	xfs_dir2_leaf_entry_t	*lep;		 
	xfs_mount_t		*mp;		 
	xfs_dir2_db_t		newdb;		 
	xfs_trans_t		*tp;		 
	enum xfs_dacmp		cmp;		 
	struct xfs_dir3_icleaf_hdr leafhdr;

	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	leaf = bp->b_addr;
	xfs_dir2_leaf_hdr_from_disk(mp, &leafhdr, leaf);

	xfs_dir3_leaf_check(dp, bp);
	if (leafhdr.count <= 0) {
		xfs_buf_mark_corrupt(bp);
		return -EFSCORRUPTED;
	}

	 
	index = xfs_dir2_leaf_search_hash(args, bp);
	 
	if (state->extravalid) {
		curbp = state->extrablk.bp;
		curdb = state->extrablk.blkno;
	}
	 
	for (lep = &leafhdr.ents[index];
	     index < leafhdr.count && be32_to_cpu(lep->hashval) == args->hashval;
	     lep++, index++) {
		 
		if (be32_to_cpu(lep->address) == XFS_DIR2_NULL_DATAPTR)
			continue;
		 
		newdb = xfs_dir2_dataptr_to_db(args->geo,
					       be32_to_cpu(lep->address));
		 
		if (newdb != curdb) {
			 
			if (curbp && (args->cmpresult == XFS_CMP_DIFFERENT ||
						curdb != state->extrablk.blkno))
				xfs_trans_brelse(tp, curbp);
			 
			if (args->cmpresult != XFS_CMP_DIFFERENT &&
					newdb == state->extrablk.blkno) {
				ASSERT(state->extravalid);
				curbp = state->extrablk.bp;
			} else {
				error = xfs_dir3_data_read(tp, dp,
						xfs_dir2_db_to_da(args->geo,
								  newdb),
						0, &curbp);
				if (error)
					return error;
			}
			xfs_dir3_data_check(dp, curbp);
			curdb = newdb;
		}
		 
		dep = (xfs_dir2_data_entry_t *)((char *)curbp->b_addr +
			xfs_dir2_dataptr_to_off(args->geo,
						be32_to_cpu(lep->address)));
		 
		cmp = xfs_dir2_compname(args, dep->name, dep->namelen);
		if (cmp != XFS_CMP_DIFFERENT && cmp != args->cmpresult) {
			 
			if (args->cmpresult != XFS_CMP_DIFFERENT &&
						curdb != state->extrablk.blkno)
				xfs_trans_brelse(tp, state->extrablk.bp);
			args->cmpresult = cmp;
			args->inumber = be64_to_cpu(dep->inumber);
			args->filetype = xfs_dir2_data_get_ftype(mp, dep);
			*indexp = index;
			state->extravalid = 1;
			state->extrablk.bp = curbp;
			state->extrablk.blkno = curdb;
			state->extrablk.index = (int)((char *)dep -
							(char *)curbp->b_addr);
			state->extrablk.magic = XFS_DIR2_DATA_MAGIC;
			curbp->b_ops = &xfs_dir3_data_buf_ops;
			xfs_trans_buf_set_type(tp, curbp, XFS_BLFT_DIR_DATA_BUF);
			if (cmp == XFS_CMP_EXACT)
				return -EEXIST;
		}
	}
	ASSERT(index == leafhdr.count || (args->op_flags & XFS_DA_OP_OKNOENT));
	if (curbp) {
		if (args->cmpresult == XFS_CMP_DIFFERENT) {
			 
			state->extravalid = 1;
			state->extrablk.bp = curbp;
			state->extrablk.index = -1;
			state->extrablk.blkno = curdb;
			state->extrablk.magic = XFS_DIR2_DATA_MAGIC;
			curbp->b_ops = &xfs_dir3_data_buf_ops;
			xfs_trans_buf_set_type(tp, curbp, XFS_BLFT_DIR_DATA_BUF);
		} else {
			 
			if (state->extrablk.bp != curbp)
				xfs_trans_brelse(tp, curbp);
		}
	} else {
		state->extravalid = 0;
	}
	*indexp = index;
	return -ENOENT;
}

 
int
xfs_dir2_leafn_lookup_int(
	struct xfs_buf		*bp,		 
	xfs_da_args_t		*args,		 
	int			*indexp,	 
	xfs_da_state_t		*state)		 
{
	if (args->op_flags & XFS_DA_OP_ADDNAME)
		return xfs_dir2_leafn_lookup_for_addname(bp, args, indexp,
							state);
	return xfs_dir2_leafn_lookup_for_entry(bp, args, indexp, state);
}

 
static void
xfs_dir3_leafn_moveents(
	xfs_da_args_t			*args,	 
	struct xfs_buf			*bp_s,	 
	struct xfs_dir3_icleaf_hdr	*shdr,
	struct xfs_dir2_leaf_entry	*sents,
	int				start_s, 
	struct xfs_buf			*bp_d,	 
	struct xfs_dir3_icleaf_hdr	*dhdr,
	struct xfs_dir2_leaf_entry	*dents,
	int				start_d, 
	int				count)	 
{
	int				stale;	 

	trace_xfs_dir2_leafn_moveents(args, start_s, start_d, count);

	 
	if (count == 0)
		return;

	 
	if (start_d < dhdr->count) {
		memmove(&dents[start_d + count], &dents[start_d],
			(dhdr->count - start_d) * sizeof(xfs_dir2_leaf_entry_t));
		xfs_dir3_leaf_log_ents(args, dhdr, bp_d, start_d + count,
				       count + dhdr->count - 1);
	}
	 
	if (shdr->stale) {
		int	i;			 

		for (i = start_s, stale = 0; i < start_s + count; i++) {
			if (sents[i].address ==
					cpu_to_be32(XFS_DIR2_NULL_DATAPTR))
				stale++;
		}
	} else
		stale = 0;
	 
	memcpy(&dents[start_d], &sents[start_s],
		count * sizeof(xfs_dir2_leaf_entry_t));
	xfs_dir3_leaf_log_ents(args, dhdr, bp_d, start_d, start_d + count - 1);

	 
	if (start_s + count < shdr->count) {
		memmove(&sents[start_s], &sents[start_s + count],
			count * sizeof(xfs_dir2_leaf_entry_t));
		xfs_dir3_leaf_log_ents(args, shdr, bp_s, start_s,
				       start_s + count - 1);
	}

	 
	shdr->count -= count;
	shdr->stale -= stale;
	dhdr->count += count;
	dhdr->stale += stale;
}

 
int						 
xfs_dir2_leafn_order(
	struct xfs_inode	*dp,
	struct xfs_buf		*leaf1_bp,		 
	struct xfs_buf		*leaf2_bp)		 
{
	struct xfs_dir2_leaf	*leaf1 = leaf1_bp->b_addr;
	struct xfs_dir2_leaf	*leaf2 = leaf2_bp->b_addr;
	struct xfs_dir2_leaf_entry *ents1;
	struct xfs_dir2_leaf_entry *ents2;
	struct xfs_dir3_icleaf_hdr hdr1;
	struct xfs_dir3_icleaf_hdr hdr2;

	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &hdr1, leaf1);
	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &hdr2, leaf2);
	ents1 = hdr1.ents;
	ents2 = hdr2.ents;

	if (hdr1.count > 0 && hdr2.count > 0 &&
	    (be32_to_cpu(ents2[0].hashval) < be32_to_cpu(ents1[0].hashval) ||
	     be32_to_cpu(ents2[hdr2.count - 1].hashval) <
				be32_to_cpu(ents1[hdr1.count - 1].hashval)))
		return 1;
	return 0;
}

 
static void
xfs_dir2_leafn_rebalance(
	xfs_da_state_t		*state,		 
	xfs_da_state_blk_t	*blk1,		 
	xfs_da_state_blk_t	*blk2)		 
{
	xfs_da_args_t		*args;		 
	int			count;		 
	int			isleft;		 
	xfs_dir2_leaf_t		*leaf1;		 
	xfs_dir2_leaf_t		*leaf2;		 
	int			mid;		 
#if defined(DEBUG) || defined(XFS_WARN)
	int			oldstale;	 
#endif
	int			oldsum;		 
	int			swap_blocks;	 
	struct xfs_dir2_leaf_entry *ents1;
	struct xfs_dir2_leaf_entry *ents2;
	struct xfs_dir3_icleaf_hdr hdr1;
	struct xfs_dir3_icleaf_hdr hdr2;
	struct xfs_inode	*dp = state->args->dp;

	args = state->args;
	 
	swap_blocks = xfs_dir2_leafn_order(dp, blk1->bp, blk2->bp);
	if (swap_blocks)
		swap(blk1, blk2);

	leaf1 = blk1->bp->b_addr;
	leaf2 = blk2->bp->b_addr;
	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &hdr1, leaf1);
	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &hdr2, leaf2);
	ents1 = hdr1.ents;
	ents2 = hdr2.ents;

	oldsum = hdr1.count + hdr2.count;
#if defined(DEBUG) || defined(XFS_WARN)
	oldstale = hdr1.stale + hdr2.stale;
#endif
	mid = oldsum >> 1;

	 
	if (oldsum & 1) {
		xfs_dahash_t	midhash;	 

		if (mid >= hdr1.count)
			midhash = be32_to_cpu(ents2[mid - hdr1.count].hashval);
		else
			midhash = be32_to_cpu(ents1[mid].hashval);
		isleft = args->hashval <= midhash;
	}
	 
	else
		isleft = 1;
	 
	count = hdr1.count - mid + (isleft == 0);
	if (count > 0)
		xfs_dir3_leafn_moveents(args, blk1->bp, &hdr1, ents1,
					hdr1.count - count, blk2->bp,
					&hdr2, ents2, 0, count);
	else if (count < 0)
		xfs_dir3_leafn_moveents(args, blk2->bp, &hdr2, ents2, 0,
					blk1->bp, &hdr1, ents1,
					hdr1.count, count);

	ASSERT(hdr1.count + hdr2.count == oldsum);
	ASSERT(hdr1.stale + hdr2.stale == oldstale);

	 
	xfs_dir2_leaf_hdr_to_disk(dp->i_mount, leaf1, &hdr1);
	xfs_dir2_leaf_hdr_to_disk(dp->i_mount, leaf2, &hdr2);
	xfs_dir3_leaf_log_header(args, blk1->bp);
	xfs_dir3_leaf_log_header(args, blk2->bp);

	xfs_dir3_leaf_check(dp, blk1->bp);
	xfs_dir3_leaf_check(dp, blk2->bp);

	 
	if (hdr1.count < hdr2.count)
		state->inleaf = swap_blocks;
	else if (hdr1.count > hdr2.count)
		state->inleaf = !swap_blocks;
	else
		state->inleaf = swap_blocks ^ (blk1->index <= hdr1.count);
	 
	if (!state->inleaf)
		blk2->index = blk1->index - hdr1.count;

	 
	if (blk2->index < 0) {
		state->inleaf = 1;
		blk2->index = 0;
		xfs_alert(dp->i_mount,
	"%s: picked the wrong leaf? reverting original leaf: blk1->index %d",
			__func__, blk1->index);
	}
}

static int
xfs_dir3_data_block_free(
	xfs_da_args_t		*args,
	struct xfs_dir2_data_hdr *hdr,
	struct xfs_dir2_free	*free,
	xfs_dir2_db_t		fdb,
	int			findex,
	struct xfs_buf		*fbp,
	int			longest)
{
	int			logfree = 0;
	struct xfs_dir3_icfree_hdr freehdr;
	struct xfs_inode	*dp = args->dp;

	xfs_dir2_free_hdr_from_disk(dp->i_mount, &freehdr, free);
	if (hdr) {
		 
		freehdr.bests[findex] = cpu_to_be16(longest);
		xfs_dir2_free_log_bests(args, &freehdr, fbp, findex, findex);
		return 0;
	}

	 
	freehdr.nused--;

	 
	if (findex == freehdr.nvalid - 1) {
		int	i;		 

		for (i = findex - 1; i >= 0; i--) {
			if (freehdr.bests[i] != cpu_to_be16(NULLDATAOFF))
				break;
		}
		freehdr.nvalid = i + 1;
		logfree = 0;
	} else {
		 
		freehdr.bests[findex] = cpu_to_be16(NULLDATAOFF);
		logfree = 1;
	}

	xfs_dir2_free_hdr_to_disk(dp->i_mount, free, &freehdr);
	xfs_dir2_free_log_header(args, fbp);

	 
	if (!freehdr.nused) {
		int error;

		error = xfs_dir2_shrink_inode(args, fdb, fbp);
		if (error == 0) {
			fbp = NULL;
			logfree = 0;
		} else if (error != -ENOSPC || args->total != 0)
			return error;
		 
	}

	 
	if (logfree)
		xfs_dir2_free_log_bests(args, &freehdr, fbp, findex, findex);
	return 0;
}

 
static int					 
xfs_dir2_leafn_remove(
	xfs_da_args_t		*args,		 
	struct xfs_buf		*bp,		 
	int			index,		 
	xfs_da_state_blk_t	*dblk,		 
	int			*rval)		 
{
	struct xfs_da_geometry	*geo = args->geo;
	xfs_dir2_data_hdr_t	*hdr;		 
	xfs_dir2_db_t		db;		 
	struct xfs_buf		*dbp;		 
	xfs_dir2_data_entry_t	*dep;		 
	xfs_inode_t		*dp;		 
	xfs_dir2_leaf_t		*leaf;		 
	xfs_dir2_leaf_entry_t	*lep;		 
	int			longest;	 
	int			off;		 
	int			needlog;	 
	int			needscan;	 
	xfs_trans_t		*tp;		 
	struct xfs_dir2_data_free *bf;		 
	struct xfs_dir3_icleaf_hdr leafhdr;

	trace_xfs_dir2_leafn_remove(args, index);

	dp = args->dp;
	tp = args->trans;
	leaf = bp->b_addr;
	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &leafhdr, leaf);

	 
	lep = &leafhdr.ents[index];

	 
	db = xfs_dir2_dataptr_to_db(geo, be32_to_cpu(lep->address));
	ASSERT(dblk->blkno == db);
	off = xfs_dir2_dataptr_to_off(geo, be32_to_cpu(lep->address));
	ASSERT(dblk->index == off);

	 
	leafhdr.stale++;
	xfs_dir2_leaf_hdr_to_disk(dp->i_mount, leaf, &leafhdr);
	xfs_dir3_leaf_log_header(args, bp);

	lep->address = cpu_to_be32(XFS_DIR2_NULL_DATAPTR);
	xfs_dir3_leaf_log_ents(args, &leafhdr, bp, index, index);

	 
	dbp = dblk->bp;
	hdr = dbp->b_addr;
	dep = (xfs_dir2_data_entry_t *)((char *)hdr + off);
	bf = xfs_dir2_data_bestfree_p(dp->i_mount, hdr);
	longest = be16_to_cpu(bf[0].length);
	needlog = needscan = 0;
	xfs_dir2_data_make_free(args, dbp, off,
		xfs_dir2_data_entsize(dp->i_mount, dep->namelen), &needlog,
		&needscan);
	 
	if (needscan)
		xfs_dir2_data_freescan(dp->i_mount, hdr, &needlog);
	if (needlog)
		xfs_dir2_data_log_header(args, dbp);
	xfs_dir3_data_check(dp, dbp);
	 
	if (longest < be16_to_cpu(bf[0].length)) {
		int		error;		 
		struct xfs_buf	*fbp;		 
		xfs_dir2_db_t	fdb;		 
		int		findex;		 
		xfs_dir2_free_t	*free;		 

		 
		fdb = xfs_dir2_db_to_fdb(geo, db);
		error = xfs_dir2_free_read(tp, dp, xfs_dir2_db_to_da(geo, fdb),
					   &fbp);
		if (error)
			return error;
		free = fbp->b_addr;
#ifdef DEBUG
	{
		struct xfs_dir3_icfree_hdr freehdr;

		xfs_dir2_free_hdr_from_disk(dp->i_mount, &freehdr, free);
		ASSERT(freehdr.firstdb == geo->free_max_bests *
			(fdb - xfs_dir2_byte_to_db(geo, XFS_DIR2_FREE_OFFSET)));
	}
#endif
		 
		findex = xfs_dir2_db_to_fdindex(geo, db);
		longest = be16_to_cpu(bf[0].length);
		 
		if (longest == geo->blksize - geo->data_entry_offset) {
			 
			error = xfs_dir2_shrink_inode(args, db, dbp);
			if (error == 0) {
				dblk->bp = NULL;
				hdr = NULL;
			}
			 
			else if (!(error == -ENOSPC && args->total == 0))
				return error;
		}
		 
		error = xfs_dir3_data_block_free(args, hdr, free,
						 fdb, findex, fbp, longest);
		if (error)
			return error;
	}

	xfs_dir3_leaf_check(dp, bp);
	 
	*rval = (geo->leaf_hdr_size +
		 (uint)sizeof(leafhdr.ents) * (leafhdr.count - leafhdr.stale)) <
		geo->magicpct;
	return 0;
}

 
int						 
xfs_dir2_leafn_split(
	xfs_da_state_t		*state,		 
	xfs_da_state_blk_t	*oldblk,	 
	xfs_da_state_blk_t	*newblk)	 
{
	xfs_da_args_t		*args;		 
	xfs_dablk_t		blkno;		 
	int			error;		 
	struct xfs_inode	*dp;

	 
	args = state->args;
	dp = args->dp;
	ASSERT(oldblk->magic == XFS_DIR2_LEAFN_MAGIC);
	error = xfs_da_grow_inode(args, &blkno);
	if (error) {
		return error;
	}
	 
	error = xfs_dir3_leaf_get_buf(args, xfs_dir2_da_to_db(args->geo, blkno),
				      &newblk->bp, XFS_DIR2_LEAFN_MAGIC);
	if (error)
		return error;

	newblk->blkno = blkno;
	newblk->magic = XFS_DIR2_LEAFN_MAGIC;
	 
	xfs_dir2_leafn_rebalance(state, oldblk, newblk);
	error = xfs_da3_blk_link(state, oldblk, newblk);
	if (error) {
		return error;
	}
	 
	if (state->inleaf)
		error = xfs_dir2_leafn_add(oldblk->bp, args, oldblk->index);
	else
		error = xfs_dir2_leafn_add(newblk->bp, args, newblk->index);
	 
	oldblk->hashval = xfs_dir2_leaf_lasthash(dp, oldblk->bp, NULL);
	newblk->hashval = xfs_dir2_leaf_lasthash(dp, newblk->bp, NULL);
	xfs_dir3_leaf_check(dp, oldblk->bp);
	xfs_dir3_leaf_check(dp, newblk->bp);
	return error;
}

 
int						 
xfs_dir2_leafn_toosmall(
	xfs_da_state_t		*state,		 
	int			*action)	 
{
	xfs_da_state_blk_t	*blk;		 
	xfs_dablk_t		blkno;		 
	struct xfs_buf		*bp;		 
	int			bytes;		 
	int			count;		 
	int			error;		 
	int			forward;	 
	int			i;		 
	xfs_dir2_leaf_t		*leaf;		 
	int			rval;		 
	struct xfs_dir3_icleaf_hdr leafhdr;
	struct xfs_dir2_leaf_entry *ents;
	struct xfs_inode	*dp = state->args->dp;

	 
	blk = &state->path.blk[state->path.active - 1];
	leaf = blk->bp->b_addr;
	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &leafhdr, leaf);
	ents = leafhdr.ents;
	xfs_dir3_leaf_check(dp, blk->bp);

	count = leafhdr.count - leafhdr.stale;
	bytes = state->args->geo->leaf_hdr_size + count * sizeof(ents[0]);
	if (bytes > (state->args->geo->blksize >> 1)) {
		 
		*action = 0;
		return 0;
	}
	 
	if (count == 0) {
		 
		forward = (leafhdr.forw != 0);
		memcpy(&state->altpath, &state->path, sizeof(state->path));
		error = xfs_da3_path_shift(state, &state->altpath, forward, 0,
			&rval);
		if (error)
			return error;
		*action = rval ? 2 : 0;
		return 0;
	}
	 
	forward = leafhdr.forw < leafhdr.back;
	for (i = 0, bp = NULL; i < 2; forward = !forward, i++) {
		struct xfs_dir3_icleaf_hdr hdr2;

		blkno = forward ? leafhdr.forw : leafhdr.back;
		if (blkno == 0)
			continue;
		 
		error = xfs_dir3_leafn_read(state->args->trans, dp, blkno, &bp);
		if (error)
			return error;

		 
		count = leafhdr.count - leafhdr.stale;
		bytes = state->args->geo->blksize -
			(state->args->geo->blksize >> 2);

		leaf = bp->b_addr;
		xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &hdr2, leaf);
		ents = hdr2.ents;
		count += hdr2.count - hdr2.stale;
		bytes -= count * sizeof(ents[0]);

		 
		if (bytes >= 0)
			break;
		xfs_trans_brelse(state->args->trans, bp);
	}
	 
	if (i >= 2) {
		*action = 0;
		return 0;
	}

	 
	memcpy(&state->altpath, &state->path, sizeof(state->path));
	if (blkno < blk->blkno)
		error = xfs_da3_path_shift(state, &state->altpath, forward, 0,
			&rval);
	else
		error = xfs_da3_path_shift(state, &state->path, forward, 0,
			&rval);
	if (error) {
		return error;
	}
	*action = rval ? 0 : 1;
	return 0;
}

 
void
xfs_dir2_leafn_unbalance(
	xfs_da_state_t		*state,		 
	xfs_da_state_blk_t	*drop_blk,	 
	xfs_da_state_blk_t	*save_blk)	 
{
	xfs_da_args_t		*args;		 
	xfs_dir2_leaf_t		*drop_leaf;	 
	xfs_dir2_leaf_t		*save_leaf;	 
	struct xfs_dir3_icleaf_hdr savehdr;
	struct xfs_dir3_icleaf_hdr drophdr;
	struct xfs_dir2_leaf_entry *sents;
	struct xfs_dir2_leaf_entry *dents;
	struct xfs_inode	*dp = state->args->dp;

	args = state->args;
	ASSERT(drop_blk->magic == XFS_DIR2_LEAFN_MAGIC);
	ASSERT(save_blk->magic == XFS_DIR2_LEAFN_MAGIC);
	drop_leaf = drop_blk->bp->b_addr;
	save_leaf = save_blk->bp->b_addr;

	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &savehdr, save_leaf);
	xfs_dir2_leaf_hdr_from_disk(dp->i_mount, &drophdr, drop_leaf);
	sents = savehdr.ents;
	dents = drophdr.ents;

	 
	if (drophdr.stale)
		xfs_dir3_leaf_compact(args, &drophdr, drop_blk->bp);
	if (savehdr.stale)
		xfs_dir3_leaf_compact(args, &savehdr, save_blk->bp);

	 
	drop_blk->hashval = be32_to_cpu(dents[drophdr.count - 1].hashval);
	if (xfs_dir2_leafn_order(dp, save_blk->bp, drop_blk->bp))
		xfs_dir3_leafn_moveents(args, drop_blk->bp, &drophdr, dents, 0,
					save_blk->bp, &savehdr, sents, 0,
					drophdr.count);
	else
		xfs_dir3_leafn_moveents(args, drop_blk->bp, &drophdr, dents, 0,
					save_blk->bp, &savehdr, sents,
					savehdr.count, drophdr.count);
	save_blk->hashval = be32_to_cpu(sents[savehdr.count - 1].hashval);

	 
	xfs_dir2_leaf_hdr_to_disk(dp->i_mount, save_leaf, &savehdr);
	xfs_dir2_leaf_hdr_to_disk(dp->i_mount, drop_leaf, &drophdr);
	xfs_dir3_leaf_log_header(args, save_blk->bp);
	xfs_dir3_leaf_log_header(args, drop_blk->bp);

	xfs_dir3_leaf_check(dp, save_blk->bp);
	xfs_dir3_leaf_check(dp, drop_blk->bp);
}

 
static int
xfs_dir2_node_add_datablk(
	struct xfs_da_args	*args,
	struct xfs_da_state_blk	*fblk,
	xfs_dir2_db_t		*dbno,
	struct xfs_buf		**dbpp,
	struct xfs_buf		**fbpp,
	struct xfs_dir3_icfree_hdr *hdr,
	int			*findex)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_trans	*tp = args->trans;
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_dir2_data_free *bf;
	xfs_dir2_db_t		fbno;
	struct xfs_buf		*fbp;
	struct xfs_buf		*dbp;
	int			error;

	 
	if (args->total == 0)
		return -ENOSPC;

	 
	error = xfs_dir2_grow_inode(args, XFS_DIR2_DATA_SPACE, dbno);
	if (error)
		return error;
	error = xfs_dir3_data_init(args, *dbno, &dbp);
	if (error)
		return error;

	 
	fbno = xfs_dir2_db_to_fdb(args->geo, *dbno);
	error = xfs_dir2_free_try_read(tp, dp,
			       xfs_dir2_db_to_da(args->geo, fbno), &fbp);
	if (error)
		return error;

	 
	if (!fbp) {
		error = xfs_dir2_grow_inode(args, XFS_DIR2_FREE_SPACE, &fbno);
		if (error)
			return error;

		if (XFS_IS_CORRUPT(mp,
				   xfs_dir2_db_to_fdb(args->geo, *dbno) !=
				   fbno)) {
			xfs_alert(mp,
"%s: dir ino %llu needed freesp block %lld for data block %lld, got %lld",
				__func__, (unsigned long long)dp->i_ino,
				(long long)xfs_dir2_db_to_fdb(args->geo, *dbno),
				(long long)*dbno, (long long)fbno);
			if (fblk) {
				xfs_alert(mp,
			" fblk "PTR_FMT" blkno %llu index %d magic 0x%x",
					fblk, (unsigned long long)fblk->blkno,
					fblk->index, fblk->magic);
			} else {
				xfs_alert(mp, " ... fblk is NULL");
			}
			return -EFSCORRUPTED;
		}

		 
		error = xfs_dir3_free_get_buf(args, fbno, &fbp);
		if (error)
			return error;
		xfs_dir2_free_hdr_from_disk(mp, hdr, fbp->b_addr);

		 
		hdr->firstdb = (fbno - xfs_dir2_byte_to_db(args->geo,
							XFS_DIR2_FREE_OFFSET)) *
				args->geo->free_max_bests;
	} else {
		xfs_dir2_free_hdr_from_disk(mp, hdr, fbp->b_addr);
	}

	 
	*findex = xfs_dir2_db_to_fdindex(args->geo, *dbno);

	 
	if (*findex >= hdr->nvalid) {
		ASSERT(*findex < args->geo->free_max_bests);
		hdr->nvalid = *findex + 1;
		hdr->bests[*findex] = cpu_to_be16(NULLDATAOFF);
	}

	 
	if (hdr->bests[*findex] == cpu_to_be16(NULLDATAOFF)) {
		hdr->nused++;
		xfs_dir2_free_hdr_to_disk(mp, fbp->b_addr, hdr);
		xfs_dir2_free_log_header(args, fbp);
	}

	 
	bf = xfs_dir2_data_bestfree_p(mp, dbp->b_addr);
	hdr->bests[*findex] = bf[0].length;

	*dbpp = dbp;
	*fbpp = fbp;
	return 0;
}

static int
xfs_dir2_node_find_freeblk(
	struct xfs_da_args	*args,
	struct xfs_da_state_blk	*fblk,
	xfs_dir2_db_t		*dbnop,
	struct xfs_buf		**fbpp,
	struct xfs_dir3_icfree_hdr *hdr,
	int			*findexp,
	int			length)
{
	struct xfs_inode	*dp = args->dp;
	struct xfs_trans	*tp = args->trans;
	struct xfs_buf		*fbp = NULL;
	xfs_dir2_db_t		firstfbno;
	xfs_dir2_db_t		lastfbno;
	xfs_dir2_db_t		ifbno = -1;
	xfs_dir2_db_t		dbno = -1;
	xfs_dir2_db_t		fbno;
	xfs_fileoff_t		fo;
	int			findex = 0;
	int			error;

	 
	if (fblk) {
		fbp = fblk->bp;
		findex = fblk->index;
		xfs_dir2_free_hdr_from_disk(dp->i_mount, hdr, fbp->b_addr);
		if (findex >= 0) {
			 
			ASSERT(findex < hdr->nvalid);
			ASSERT(be16_to_cpu(hdr->bests[findex]) != NULLDATAOFF);
			ASSERT(be16_to_cpu(hdr->bests[findex]) >= length);
			dbno = hdr->firstdb + findex;
			goto found_block;
		}

		 
		ifbno = fblk->blkno;
		xfs_trans_brelse(tp, fbp);
		fbp = NULL;
		fblk->bp = NULL;
	}

	 
	error = xfs_bmap_last_offset(dp, &fo, XFS_DATA_FORK);
	if (error)
		return error;
	lastfbno = xfs_dir2_da_to_db(args->geo, (xfs_dablk_t)fo);
	firstfbno = xfs_dir2_byte_to_db(args->geo, XFS_DIR2_FREE_OFFSET);

	for (fbno = lastfbno - 1; fbno >= firstfbno; fbno--) {
		 
		if (fbno == ifbno)
			continue;

		 
		error = xfs_dir2_free_try_read(tp, dp,
				xfs_dir2_db_to_da(args->geo, fbno),
				&fbp);
		if (error)
			return error;
		if (!fbp)
			continue;

		xfs_dir2_free_hdr_from_disk(dp->i_mount, hdr, fbp->b_addr);

		 
		for (findex = hdr->nvalid - 1; findex >= 0; findex--) {
			if (be16_to_cpu(hdr->bests[findex]) != NULLDATAOFF &&
			    be16_to_cpu(hdr->bests[findex]) >= length) {
				dbno = hdr->firstdb + findex;
				goto found_block;
			}
		}

		 
		xfs_trans_brelse(tp, fbp);
	}

found_block:
	*dbnop = dbno;
	*fbpp = fbp;
	*findexp = findex;
	return 0;
}

 
static int
xfs_dir2_node_addname_int(
	struct xfs_da_args	*args,		 
	struct xfs_da_state_blk	*fblk)		 
{
	struct xfs_dir2_data_unused *dup;	 
	struct xfs_dir2_data_entry *dep;	 
	struct xfs_dir2_data_hdr *hdr;		 
	struct xfs_dir2_data_free *bf;
	struct xfs_trans	*tp = args->trans;
	struct xfs_inode	*dp = args->dp;
	struct xfs_dir3_icfree_hdr freehdr;
	struct xfs_buf		*dbp;		 
	struct xfs_buf		*fbp;		 
	xfs_dir2_data_aoff_t	aoff;
	xfs_dir2_db_t		dbno;		 
	int			error;		 
	int			findex;		 
	int			length;		 
	int			logfree = 0;	 
	int			needlog = 0;	 
	int			needscan = 0;	 
	__be16			*tagp;		 

	length = xfs_dir2_data_entsize(dp->i_mount, args->namelen);
	error = xfs_dir2_node_find_freeblk(args, fblk, &dbno, &fbp, &freehdr,
					   &findex, length);
	if (error)
		return error;

	 
	if (args->op_flags & XFS_DA_OP_JUSTCHECK) {
		if (dbno == -1)
			return -ENOSPC;
		return 0;
	}

	 
	if (dbno == -1) {
		 
		logfree = 1;
		error = xfs_dir2_node_add_datablk(args, fblk, &dbno, &dbp, &fbp,
						  &freehdr, &findex);
	} else {
		 
		error = xfs_dir3_data_read(tp, dp,
					   xfs_dir2_db_to_da(args->geo, dbno),
					   0, &dbp);
	}
	if (error)
		return error;

	 
	hdr = dbp->b_addr;
	bf = xfs_dir2_data_bestfree_p(dp->i_mount, hdr);
	ASSERT(be16_to_cpu(bf[0].length) >= length);

	 
	dup = (xfs_dir2_data_unused_t *)
	      ((char *)hdr + be16_to_cpu(bf[0].offset));

	 
	aoff = (xfs_dir2_data_aoff_t)((char *)dup - (char *)hdr);
	error = xfs_dir2_data_use_free(args, dbp, dup, aoff, length,
			&needlog, &needscan);
	if (error) {
		xfs_trans_brelse(tp, dbp);
		return error;
	}

	 
	dep = (xfs_dir2_data_entry_t *)dup;
	dep->inumber = cpu_to_be64(args->inumber);
	dep->namelen = args->namelen;
	memcpy(dep->name, args->name, dep->namelen);
	xfs_dir2_data_put_ftype(dp->i_mount, dep, args->filetype);
	tagp = xfs_dir2_data_entry_tag_p(dp->i_mount, dep);
	*tagp = cpu_to_be16((char *)dep - (char *)hdr);
	xfs_dir2_data_log_entry(args, dbp, dep);

	 
	if (needscan)
		xfs_dir2_data_freescan(dp->i_mount, hdr, &needlog);
	if (needlog)
		xfs_dir2_data_log_header(args, dbp);

	 
	if (freehdr.bests[findex] != bf[0].length) {
		freehdr.bests[findex] = bf[0].length;
		logfree = 1;
	}

	 
	if (logfree)
		xfs_dir2_free_log_bests(args, &freehdr, fbp, findex, findex);

	 
	args->blkno = (xfs_dablk_t)dbno;
	args->index = be16_to_cpu(*tagp);
	return 0;
}

 
int						 
xfs_dir2_node_addname(
	xfs_da_args_t		*args)		 
{
	xfs_da_state_blk_t	*blk;		 
	int			error;		 
	int			rval;		 
	xfs_da_state_t		*state;		 

	trace_xfs_dir2_node_addname(args);

	 
	state = xfs_da_state_alloc(args);
	 
	error = xfs_da3_node_lookup_int(state, &rval);
	if (error)
		rval = error;
	if (rval != -ENOENT) {
		goto done;
	}
	 
	rval = xfs_dir2_node_addname_int(args,
		state->extravalid ? &state->extrablk : NULL);
	if (rval) {
		goto done;
	}
	blk = &state->path.blk[state->path.active - 1];
	ASSERT(blk->magic == XFS_DIR2_LEAFN_MAGIC);
	 
	rval = xfs_dir2_leafn_add(blk->bp, args, blk->index);
	if (rval == 0) {
		 
		if (!(args->op_flags & XFS_DA_OP_JUSTCHECK))
			xfs_da3_fixhashpath(state, &state->path);
	} else {
		 
		if (args->total == 0) {
			ASSERT(rval == -ENOSPC);
			goto done;
		}
		 
		rval = xfs_da3_split(state);
	}
done:
	xfs_da_state_free(state);
	return rval;
}

 
int						 
xfs_dir2_node_lookup(
	xfs_da_args_t	*args)			 
{
	int		error;			 
	int		i;			 
	int		rval;			 
	xfs_da_state_t	*state;			 

	trace_xfs_dir2_node_lookup(args);

	 
	state = xfs_da_state_alloc(args);

	 
	error = xfs_da3_node_lookup_int(state, &rval);
	if (error)
		rval = error;
	else if (rval == -ENOENT && args->cmpresult == XFS_CMP_CASE) {
		 
		xfs_dir2_data_entry_t	*dep;

		dep = (xfs_dir2_data_entry_t *)
			((char *)state->extrablk.bp->b_addr +
						 state->extrablk.index);
		rval = xfs_dir_cilookup_result(args, dep->name, dep->namelen);
	}
	 
	for (i = 0; i < state->path.active; i++) {
		xfs_trans_brelse(args->trans, state->path.blk[i].bp);
		state->path.blk[i].bp = NULL;
	}
	 
	if (state->extravalid && state->extrablk.bp) {
		xfs_trans_brelse(args->trans, state->extrablk.bp);
		state->extrablk.bp = NULL;
	}
	xfs_da_state_free(state);
	return rval;
}

 
int						 
xfs_dir2_node_removename(
	struct xfs_da_args	*args)		 
{
	struct xfs_da_state_blk	*blk;		 
	int			error;		 
	int			rval;		 
	struct xfs_da_state	*state;		 

	trace_xfs_dir2_node_removename(args);

	 
	state = xfs_da_state_alloc(args);

	 
	error = xfs_da3_node_lookup_int(state, &rval);
	if (error)
		goto out_free;

	 
	if (rval != -EEXIST) {
		error = rval;
		goto out_free;
	}

	blk = &state->path.blk[state->path.active - 1];
	ASSERT(blk->magic == XFS_DIR2_LEAFN_MAGIC);
	ASSERT(state->extravalid);
	 
	error = xfs_dir2_leafn_remove(args, blk->bp, blk->index,
		&state->extrablk, &rval);
	if (error)
		goto out_free;
	 
	xfs_da3_fixhashpath(state, &state->path);
	 
	if (rval && state->path.active > 1)
		error = xfs_da3_join(state);
	 
	if (!error)
		error = xfs_dir2_node_to_leaf(state);
out_free:
	xfs_da_state_free(state);
	return error;
}

 
int						 
xfs_dir2_node_replace(
	xfs_da_args_t		*args)		 
{
	xfs_da_state_blk_t	*blk;		 
	xfs_dir2_data_hdr_t	*hdr;		 
	xfs_dir2_data_entry_t	*dep;		 
	int			error;		 
	int			i;		 
	xfs_ino_t		inum;		 
	int			ftype;		 
	int			rval;		 
	xfs_da_state_t		*state;		 

	trace_xfs_dir2_node_replace(args);

	 
	state = xfs_da_state_alloc(args);

	 
	inum = args->inumber;
	ftype = args->filetype;

	 
	error = xfs_da3_node_lookup_int(state, &rval);
	if (error) {
		rval = error;
	}
	 
	if (rval == -EEXIST) {
		struct xfs_dir3_icleaf_hdr	leafhdr;

		 
		blk = &state->path.blk[state->path.active - 1];
		ASSERT(blk->magic == XFS_DIR2_LEAFN_MAGIC);
		ASSERT(state->extravalid);

		xfs_dir2_leaf_hdr_from_disk(state->mp, &leafhdr,
					    blk->bp->b_addr);
		 
		hdr = state->extrablk.bp->b_addr;
		ASSERT(hdr->magic == cpu_to_be32(XFS_DIR2_DATA_MAGIC) ||
		       hdr->magic == cpu_to_be32(XFS_DIR3_DATA_MAGIC));
		dep = (xfs_dir2_data_entry_t *)
		      ((char *)hdr +
		       xfs_dir2_dataptr_to_off(args->geo,
				be32_to_cpu(leafhdr.ents[blk->index].address)));
		ASSERT(inum != be64_to_cpu(dep->inumber));
		 
		dep->inumber = cpu_to_be64(inum);
		xfs_dir2_data_put_ftype(state->mp, dep, ftype);
		xfs_dir2_data_log_entry(args, state->extrablk.bp, dep);
		rval = 0;
	}
	 
	else if (state->extravalid) {
		xfs_trans_brelse(args->trans, state->extrablk.bp);
		state->extrablk.bp = NULL;
	}
	 
	for (i = 0; i < state->path.active; i++) {
		xfs_trans_brelse(args->trans, state->path.blk[i].bp);
		state->path.blk[i].bp = NULL;
	}
	xfs_da_state_free(state);
	return rval;
}

 
int						 
xfs_dir2_node_trim_free(
	xfs_da_args_t		*args,		 
	xfs_fileoff_t		fo,		 
	int			*rvalp)		 
{
	struct xfs_buf		*bp;		 
	xfs_inode_t		*dp;		 
	int			error;		 
	xfs_dir2_free_t		*free;		 
	xfs_trans_t		*tp;		 
	struct xfs_dir3_icfree_hdr freehdr;

	dp = args->dp;
	tp = args->trans;

	*rvalp = 0;

	 
	error = xfs_dir2_free_try_read(tp, dp, fo, &bp);
	if (error)
		return error;
	 
	if (!bp)
		return 0;
	free = bp->b_addr;
	xfs_dir2_free_hdr_from_disk(dp->i_mount, &freehdr, free);

	 
	if (freehdr.nused > 0) {
		xfs_trans_brelse(tp, bp);
		return 0;
	}
	 
	error = xfs_dir2_shrink_inode(args,
			xfs_dir2_da_to_db(args->geo, (xfs_dablk_t)fo), bp);
	if (error) {
		 
		ASSERT(error != -ENOSPC);
		xfs_trans_brelse(tp, bp);
		return error;
	}
	 
	*rvalp = 1;
	return 0;
}

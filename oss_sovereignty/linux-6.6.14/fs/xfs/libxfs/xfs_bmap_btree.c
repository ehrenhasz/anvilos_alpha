
 
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
#include "xfs_alloc.h"
#include "xfs_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_bmap.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_trace.h"
#include "xfs_rmap.h"
#include "xfs_ag.h"

static struct kmem_cache	*xfs_bmbt_cur_cache;

 
void
xfs_bmdr_to_bmbt(
	struct xfs_inode	*ip,
	xfs_bmdr_block_t	*dblock,
	int			dblocklen,
	struct xfs_btree_block	*rblock,
	int			rblocklen)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			dmxr;
	xfs_bmbt_key_t		*fkp;
	__be64			*fpp;
	xfs_bmbt_key_t		*tkp;
	__be64			*tpp;

	xfs_btree_init_block_int(mp, rblock, XFS_BUF_DADDR_NULL,
				 XFS_BTNUM_BMAP, 0, 0, ip->i_ino,
				 XFS_BTREE_LONG_PTRS);
	rblock->bb_level = dblock->bb_level;
	ASSERT(be16_to_cpu(rblock->bb_level) > 0);
	rblock->bb_numrecs = dblock->bb_numrecs;
	dmxr = xfs_bmdr_maxrecs(dblocklen, 0);
	fkp = XFS_BMDR_KEY_ADDR(dblock, 1);
	tkp = XFS_BMBT_KEY_ADDR(mp, rblock, 1);
	fpp = XFS_BMDR_PTR_ADDR(dblock, 1, dmxr);
	tpp = XFS_BMAP_BROOT_PTR_ADDR(mp, rblock, 1, rblocklen);
	dmxr = be16_to_cpu(dblock->bb_numrecs);
	memcpy(tkp, fkp, sizeof(*fkp) * dmxr);
	memcpy(tpp, fpp, sizeof(*fpp) * dmxr);
}

void
xfs_bmbt_disk_get_all(
	const struct xfs_bmbt_rec *rec,
	struct xfs_bmbt_irec	*irec)
{
	uint64_t		l0 = get_unaligned_be64(&rec->l0);
	uint64_t		l1 = get_unaligned_be64(&rec->l1);

	irec->br_startoff = (l0 & xfs_mask64lo(64 - BMBT_EXNTFLAG_BITLEN)) >> 9;
	irec->br_startblock = ((l0 & xfs_mask64lo(9)) << 43) | (l1 >> 21);
	irec->br_blockcount = l1 & xfs_mask64lo(21);
	if (l0 >> (64 - BMBT_EXNTFLAG_BITLEN))
		irec->br_state = XFS_EXT_UNWRITTEN;
	else
		irec->br_state = XFS_EXT_NORM;
}

 
xfs_filblks_t
xfs_bmbt_disk_get_blockcount(
	const struct xfs_bmbt_rec	*r)
{
	return (xfs_filblks_t)(be64_to_cpu(r->l1) & xfs_mask64lo(21));
}

 
xfs_fileoff_t
xfs_bmbt_disk_get_startoff(
	const struct xfs_bmbt_rec	*r)
{
	return ((xfs_fileoff_t)be64_to_cpu(r->l0) &
		 xfs_mask64lo(64 - BMBT_EXNTFLAG_BITLEN)) >> 9;
}

 
void
xfs_bmbt_disk_set_all(
	struct xfs_bmbt_rec	*r,
	struct xfs_bmbt_irec	*s)
{
	int			extent_flag = (s->br_state != XFS_EXT_NORM);

	ASSERT(s->br_state == XFS_EXT_NORM || s->br_state == XFS_EXT_UNWRITTEN);
	ASSERT(!(s->br_startoff & xfs_mask64hi(64-BMBT_STARTOFF_BITLEN)));
	ASSERT(!(s->br_blockcount & xfs_mask64hi(64-BMBT_BLOCKCOUNT_BITLEN)));
	ASSERT(!(s->br_startblock & xfs_mask64hi(64-BMBT_STARTBLOCK_BITLEN)));

	put_unaligned_be64(
		((xfs_bmbt_rec_base_t)extent_flag << 63) |
		 ((xfs_bmbt_rec_base_t)s->br_startoff << 9) |
		 ((xfs_bmbt_rec_base_t)s->br_startblock >> 43), &r->l0);
	put_unaligned_be64(
		((xfs_bmbt_rec_base_t)s->br_startblock << 21) |
		 ((xfs_bmbt_rec_base_t)s->br_blockcount &
		  (xfs_bmbt_rec_base_t)xfs_mask64lo(21)), &r->l1);
}

 
void
xfs_bmbt_to_bmdr(
	struct xfs_mount	*mp,
	struct xfs_btree_block	*rblock,
	int			rblocklen,
	xfs_bmdr_block_t	*dblock,
	int			dblocklen)
{
	int			dmxr;
	xfs_bmbt_key_t		*fkp;
	__be64			*fpp;
	xfs_bmbt_key_t		*tkp;
	__be64			*tpp;

	if (xfs_has_crc(mp)) {
		ASSERT(rblock->bb_magic == cpu_to_be32(XFS_BMAP_CRC_MAGIC));
		ASSERT(uuid_equal(&rblock->bb_u.l.bb_uuid,
		       &mp->m_sb.sb_meta_uuid));
		ASSERT(rblock->bb_u.l.bb_blkno ==
		       cpu_to_be64(XFS_BUF_DADDR_NULL));
	} else
		ASSERT(rblock->bb_magic == cpu_to_be32(XFS_BMAP_MAGIC));
	ASSERT(rblock->bb_u.l.bb_leftsib == cpu_to_be64(NULLFSBLOCK));
	ASSERT(rblock->bb_u.l.bb_rightsib == cpu_to_be64(NULLFSBLOCK));
	ASSERT(rblock->bb_level != 0);
	dblock->bb_level = rblock->bb_level;
	dblock->bb_numrecs = rblock->bb_numrecs;
	dmxr = xfs_bmdr_maxrecs(dblocklen, 0);
	fkp = XFS_BMBT_KEY_ADDR(mp, rblock, 1);
	tkp = XFS_BMDR_KEY_ADDR(dblock, 1);
	fpp = XFS_BMAP_BROOT_PTR_ADDR(mp, rblock, 1, rblocklen);
	tpp = XFS_BMDR_PTR_ADDR(dblock, 1, dmxr);
	dmxr = be16_to_cpu(dblock->bb_numrecs);
	memcpy(tkp, fkp, sizeof(*fkp) * dmxr);
	memcpy(tpp, fpp, sizeof(*fpp) * dmxr);
}

STATIC struct xfs_btree_cur *
xfs_bmbt_dup_cursor(
	struct xfs_btree_cur	*cur)
{
	struct xfs_btree_cur	*new;

	new = xfs_bmbt_init_cursor(cur->bc_mp, cur->bc_tp,
			cur->bc_ino.ip, cur->bc_ino.whichfork);

	 
	new->bc_ino.flags = cur->bc_ino.flags;

	return new;
}

STATIC void
xfs_bmbt_update_cursor(
	struct xfs_btree_cur	*src,
	struct xfs_btree_cur	*dst)
{
	ASSERT((dst->bc_tp->t_highest_agno != NULLAGNUMBER) ||
	       (dst->bc_ino.ip->i_diflags & XFS_DIFLAG_REALTIME));

	dst->bc_ino.allocated += src->bc_ino.allocated;
	dst->bc_tp->t_highest_agno = src->bc_tp->t_highest_agno;

	src->bc_ino.allocated = 0;
}

STATIC int
xfs_bmbt_alloc_block(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*start,
	union xfs_btree_ptr		*new,
	int				*stat)
{
	struct xfs_alloc_arg	args;
	int			error;

	memset(&args, 0, sizeof(args));
	args.tp = cur->bc_tp;
	args.mp = cur->bc_mp;
	xfs_rmap_ino_bmbt_owner(&args.oinfo, cur->bc_ino.ip->i_ino,
			cur->bc_ino.whichfork);
	args.minlen = args.maxlen = args.prod = 1;
	args.wasdel = cur->bc_ino.flags & XFS_BTCUR_BMBT_WASDEL;
	if (!args.wasdel && args.tp->t_blk_res == 0)
		return -ENOSPC;

	 
	if (cur->bc_tp->t_highest_agno == NULLAGNUMBER)
		args.minleft = xfs_bmapi_minleft(cur->bc_tp, cur->bc_ino.ip,
					cur->bc_ino.whichfork);

	error = xfs_alloc_vextent_start_ag(&args, be64_to_cpu(start->l));
	if (error)
		return error;

	if (args.fsbno == NULLFSBLOCK && args.minleft) {
		 
		args.minleft = 0;
		error = xfs_alloc_vextent_start_ag(&args, 0);
		if (error)
			return error;
		cur->bc_tp->t_flags |= XFS_TRANS_LOWMODE;
	}
	if (WARN_ON_ONCE(args.fsbno == NULLFSBLOCK)) {
		*stat = 0;
		return 0;
	}

	ASSERT(args.len == 1);
	cur->bc_ino.allocated++;
	cur->bc_ino.ip->i_nblocks++;
	xfs_trans_log_inode(args.tp, cur->bc_ino.ip, XFS_ILOG_CORE);
	xfs_trans_mod_dquot_byino(args.tp, cur->bc_ino.ip,
			XFS_TRANS_DQ_BCOUNT, 1L);

	new->l = cpu_to_be64(args.fsbno);

	*stat = 1;
	return 0;
}

STATIC int
xfs_bmbt_free_block(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	struct xfs_inode	*ip = cur->bc_ino.ip;
	struct xfs_trans	*tp = cur->bc_tp;
	xfs_fsblock_t		fsbno = XFS_DADDR_TO_FSB(mp, xfs_buf_daddr(bp));
	struct xfs_owner_info	oinfo;
	int			error;

	xfs_rmap_ino_bmbt_owner(&oinfo, ip->i_ino, cur->bc_ino.whichfork);
	error = xfs_free_extent_later(cur->bc_tp, fsbno, 1, &oinfo,
			XFS_AG_RESV_NONE);
	if (error)
		return error;

	ip->i_nblocks--;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, -1L);
	return 0;
}

STATIC int
xfs_bmbt_get_minrecs(
	struct xfs_btree_cur	*cur,
	int			level)
{
	if (level == cur->bc_nlevels - 1) {
		struct xfs_ifork	*ifp;

		ifp = xfs_ifork_ptr(cur->bc_ino.ip,
				    cur->bc_ino.whichfork);

		return xfs_bmbt_maxrecs(cur->bc_mp,
					ifp->if_broot_bytes, level == 0) / 2;
	}

	return cur->bc_mp->m_bmap_dmnr[level != 0];
}

int
xfs_bmbt_get_maxrecs(
	struct xfs_btree_cur	*cur,
	int			level)
{
	if (level == cur->bc_nlevels - 1) {
		struct xfs_ifork	*ifp;

		ifp = xfs_ifork_ptr(cur->bc_ino.ip,
				    cur->bc_ino.whichfork);

		return xfs_bmbt_maxrecs(cur->bc_mp,
					ifp->if_broot_bytes, level == 0);
	}

	return cur->bc_mp->m_bmap_dmxr[level != 0];

}

 
STATIC int
xfs_bmbt_get_dmaxrecs(
	struct xfs_btree_cur	*cur,
	int			level)
{
	if (level != cur->bc_nlevels - 1)
		return cur->bc_mp->m_bmap_dmxr[level != 0];
	return xfs_bmdr_maxrecs(cur->bc_ino.forksize, level == 0);
}

STATIC void
xfs_bmbt_init_key_from_rec(
	union xfs_btree_key		*key,
	const union xfs_btree_rec	*rec)
{
	key->bmbt.br_startoff =
		cpu_to_be64(xfs_bmbt_disk_get_startoff(&rec->bmbt));
}

STATIC void
xfs_bmbt_init_high_key_from_rec(
	union xfs_btree_key		*key,
	const union xfs_btree_rec	*rec)
{
	key->bmbt.br_startoff = cpu_to_be64(
			xfs_bmbt_disk_get_startoff(&rec->bmbt) +
			xfs_bmbt_disk_get_blockcount(&rec->bmbt) - 1);
}

STATIC void
xfs_bmbt_init_rec_from_cur(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec)
{
	xfs_bmbt_disk_set_all(&rec->bmbt, &cur->bc_rec.b);
}

STATIC void
xfs_bmbt_init_ptr_from_cur(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	ptr->l = 0;
}

STATIC int64_t
xfs_bmbt_key_diff(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_key	*key)
{
	return (int64_t)be64_to_cpu(key->bmbt.br_startoff) -
				      cur->bc_rec.b.br_startoff;
}

STATIC int64_t
xfs_bmbt_diff_two_keys(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_key	*k1,
	const union xfs_btree_key	*k2,
	const union xfs_btree_key	*mask)
{
	uint64_t			a = be64_to_cpu(k1->bmbt.br_startoff);
	uint64_t			b = be64_to_cpu(k2->bmbt.br_startoff);

	ASSERT(!mask || mask->bmbt.br_startoff);

	 
	if (a > b)
		return 1;
	if (b > a)
		return -1;
	return 0;
}

static xfs_failaddr_t
xfs_bmbt_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	xfs_failaddr_t		fa;
	unsigned int		level;

	if (!xfs_verify_magic(bp, block->bb_magic))
		return __this_address;

	if (xfs_has_crc(mp)) {
		 
		fa = xfs_btree_lblock_v5hdr_verify(bp, XFS_RMAP_OWN_UNKNOWN);
		if (fa)
			return fa;
	}

	 
	level = be16_to_cpu(block->bb_level);
	if (level > max(mp->m_bm_maxlevels[0], mp->m_bm_maxlevels[1]))
		return __this_address;

	return xfs_btree_lblock_verify(bp, mp->m_bmap_dmxr[level != 0]);
}

static void
xfs_bmbt_read_verify(
	struct xfs_buf	*bp)
{
	xfs_failaddr_t	fa;

	if (!xfs_btree_lblock_verify_crc(bp))
		xfs_verifier_error(bp, -EFSBADCRC, __this_address);
	else {
		fa = xfs_bmbt_verify(bp);
		if (fa)
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
	}

	if (bp->b_error)
		trace_xfs_btree_corrupt(bp, _RET_IP_);
}

static void
xfs_bmbt_write_verify(
	struct xfs_buf	*bp)
{
	xfs_failaddr_t	fa;

	fa = xfs_bmbt_verify(bp);
	if (fa) {
		trace_xfs_btree_corrupt(bp, _RET_IP_);
		xfs_verifier_error(bp, -EFSCORRUPTED, fa);
		return;
	}
	xfs_btree_lblock_calc_crc(bp);
}

const struct xfs_buf_ops xfs_bmbt_buf_ops = {
	.name = "xfs_bmbt",
	.magic = { cpu_to_be32(XFS_BMAP_MAGIC),
		   cpu_to_be32(XFS_BMAP_CRC_MAGIC) },
	.verify_read = xfs_bmbt_read_verify,
	.verify_write = xfs_bmbt_write_verify,
	.verify_struct = xfs_bmbt_verify,
};


STATIC int
xfs_bmbt_keys_inorder(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_key	*k1,
	const union xfs_btree_key	*k2)
{
	return be64_to_cpu(k1->bmbt.br_startoff) <
		be64_to_cpu(k2->bmbt.br_startoff);
}

STATIC int
xfs_bmbt_recs_inorder(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_rec	*r1,
	const union xfs_btree_rec	*r2)
{
	return xfs_bmbt_disk_get_startoff(&r1->bmbt) +
		xfs_bmbt_disk_get_blockcount(&r1->bmbt) <=
		xfs_bmbt_disk_get_startoff(&r2->bmbt);
}

STATIC enum xbtree_key_contig
xfs_bmbt_keys_contiguous(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_key	*key1,
	const union xfs_btree_key	*key2,
	const union xfs_btree_key	*mask)
{
	ASSERT(!mask || mask->bmbt.br_startoff);

	return xbtree_key_contig(be64_to_cpu(key1->bmbt.br_startoff),
				 be64_to_cpu(key2->bmbt.br_startoff));
}

static const struct xfs_btree_ops xfs_bmbt_ops = {
	.rec_len		= sizeof(xfs_bmbt_rec_t),
	.key_len		= sizeof(xfs_bmbt_key_t),

	.dup_cursor		= xfs_bmbt_dup_cursor,
	.update_cursor		= xfs_bmbt_update_cursor,
	.alloc_block		= xfs_bmbt_alloc_block,
	.free_block		= xfs_bmbt_free_block,
	.get_maxrecs		= xfs_bmbt_get_maxrecs,
	.get_minrecs		= xfs_bmbt_get_minrecs,
	.get_dmaxrecs		= xfs_bmbt_get_dmaxrecs,
	.init_key_from_rec	= xfs_bmbt_init_key_from_rec,
	.init_high_key_from_rec	= xfs_bmbt_init_high_key_from_rec,
	.init_rec_from_cur	= xfs_bmbt_init_rec_from_cur,
	.init_ptr_from_cur	= xfs_bmbt_init_ptr_from_cur,
	.key_diff		= xfs_bmbt_key_diff,
	.diff_two_keys		= xfs_bmbt_diff_two_keys,
	.buf_ops		= &xfs_bmbt_buf_ops,
	.keys_inorder		= xfs_bmbt_keys_inorder,
	.recs_inorder		= xfs_bmbt_recs_inorder,
	.keys_contiguous	= xfs_bmbt_keys_contiguous,
};

 
struct xfs_btree_cur *				 
xfs_bmbt_init_cursor(
	struct xfs_mount	*mp,		 
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	int			whichfork)	 
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_cur	*cur;
	ASSERT(whichfork != XFS_COW_FORK);

	cur = xfs_btree_alloc_cursor(mp, tp, XFS_BTNUM_BMAP,
			mp->m_bm_maxlevels[whichfork], xfs_bmbt_cur_cache);
	cur->bc_nlevels = be16_to_cpu(ifp->if_broot->bb_level) + 1;
	cur->bc_statoff = XFS_STATS_CALC_INDEX(xs_bmbt_2);

	cur->bc_ops = &xfs_bmbt_ops;
	cur->bc_flags = XFS_BTREE_LONG_PTRS | XFS_BTREE_ROOT_IN_INODE;
	if (xfs_has_crc(mp))
		cur->bc_flags |= XFS_BTREE_CRC_BLOCKS;

	cur->bc_ino.forksize = xfs_inode_fork_size(ip, whichfork);
	cur->bc_ino.ip = ip;
	cur->bc_ino.allocated = 0;
	cur->bc_ino.flags = 0;
	cur->bc_ino.whichfork = whichfork;

	return cur;
}

 
static inline unsigned int
xfs_bmbt_block_maxrecs(
	unsigned int		blocklen,
	bool			leaf)
{
	if (leaf)
		return blocklen / sizeof(xfs_bmbt_rec_t);
	return blocklen / (sizeof(xfs_bmbt_key_t) + sizeof(xfs_bmbt_ptr_t));
}

 
int
xfs_bmbt_maxrecs(
	struct xfs_mount	*mp,
	int			blocklen,
	int			leaf)
{
	blocklen -= XFS_BMBT_BLOCK_LEN(mp);
	return xfs_bmbt_block_maxrecs(blocklen, leaf);
}

 
unsigned int
xfs_bmbt_maxlevels_ondisk(void)
{
	unsigned int		minrecs[2];
	unsigned int		blocklen;

	blocklen = min(XFS_MIN_BLOCKSIZE - XFS_BTREE_SBLOCK_LEN,
		       XFS_MIN_CRC_BLOCKSIZE - XFS_BTREE_SBLOCK_CRC_LEN);

	minrecs[0] = xfs_bmbt_block_maxrecs(blocklen, true) / 2;
	minrecs[1] = xfs_bmbt_block_maxrecs(blocklen, false) / 2;

	 
	return xfs_btree_compute_maxlevels(minrecs,
			XFS_MAX_EXTCNT_DATA_FORK_LARGE) + 1;
}

 
int
xfs_bmdr_maxrecs(
	int			blocklen,
	int			leaf)
{
	blocklen -= sizeof(xfs_bmdr_block_t);

	if (leaf)
		return blocklen / sizeof(xfs_bmdr_rec_t);
	return blocklen / (sizeof(xfs_bmdr_key_t) + sizeof(xfs_bmdr_ptr_t));
}

 
int
xfs_bmbt_change_owner(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	xfs_ino_t		new_owner,
	struct list_head	*buffer_list)
{
	struct xfs_btree_cur	*cur;
	int			error;

	ASSERT(tp || buffer_list);
	ASSERT(!(tp && buffer_list));
	ASSERT(xfs_ifork_ptr(ip, whichfork)->if_format == XFS_DINODE_FMT_BTREE);

	cur = xfs_bmbt_init_cursor(ip->i_mount, tp, ip, whichfork);
	cur->bc_ino.flags |= XFS_BTCUR_BMBT_INVALID_OWNER;

	error = xfs_btree_change_owner(cur, new_owner, buffer_list);
	xfs_btree_del_cursor(cur, error);
	return error;
}

 
unsigned long long
xfs_bmbt_calc_size(
	struct xfs_mount	*mp,
	unsigned long long	len)
{
	return xfs_btree_calc_size(mp->m_bmap_dmnr, len);
}

int __init
xfs_bmbt_init_cur_cache(void)
{
	xfs_bmbt_cur_cache = kmem_cache_create("xfs_bmbt_cur",
			xfs_btree_cur_sizeof(xfs_bmbt_maxlevels_ondisk()),
			0, 0, NULL);

	if (!xfs_bmbt_cur_cache)
		return -ENOMEM;
	return 0;
}

void
xfs_bmbt_destroy_cur_cache(void)
{
	kmem_cache_destroy(xfs_bmbt_cur_cache);
	xfs_bmbt_cur_cache = NULL;
}


 
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
#include "xfs_buf_item.h"
#include "xfs_btree.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_alloc.h"
#include "xfs_log.h"
#include "xfs_btree_staging.h"
#include "xfs_ag.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_rmap_btree.h"
#include "xfs_refcount_btree.h"

 
static const uint32_t xfs_magics[2][XFS_BTNUM_MAX] = {
	{ XFS_ABTB_MAGIC, XFS_ABTC_MAGIC, 0, XFS_BMAP_MAGIC, XFS_IBT_MAGIC,
	  XFS_FIBT_MAGIC, 0 },
	{ XFS_ABTB_CRC_MAGIC, XFS_ABTC_CRC_MAGIC, XFS_RMAP_CRC_MAGIC,
	  XFS_BMAP_CRC_MAGIC, XFS_IBT_CRC_MAGIC, XFS_FIBT_CRC_MAGIC,
	  XFS_REFC_CRC_MAGIC }
};

uint32_t
xfs_btree_magic(
	int			crc,
	xfs_btnum_t		btnum)
{
	uint32_t		magic = xfs_magics[crc][btnum];

	 
	ASSERT(magic != 0);
	return magic;
}

 
static inline xfs_failaddr_t
xfs_btree_check_lblock_siblings(
	struct xfs_mount	*mp,
	struct xfs_btree_cur	*cur,
	int			level,
	xfs_fsblock_t		fsb,
	__be64			dsibling)
{
	xfs_fsblock_t		sibling;

	if (dsibling == cpu_to_be64(NULLFSBLOCK))
		return NULL;

	sibling = be64_to_cpu(dsibling);
	if (sibling == fsb)
		return __this_address;
	if (level >= 0) {
		if (!xfs_btree_check_lptr(cur, sibling, level + 1))
			return __this_address;
	} else {
		if (!xfs_verify_fsbno(mp, sibling))
			return __this_address;
	}

	return NULL;
}

static inline xfs_failaddr_t
xfs_btree_check_sblock_siblings(
	struct xfs_perag	*pag,
	struct xfs_btree_cur	*cur,
	int			level,
	xfs_agblock_t		agbno,
	__be32			dsibling)
{
	xfs_agblock_t		sibling;

	if (dsibling == cpu_to_be32(NULLAGBLOCK))
		return NULL;

	sibling = be32_to_cpu(dsibling);
	if (sibling == agbno)
		return __this_address;
	if (level >= 0) {
		if (!xfs_btree_check_sptr(cur, sibling, level + 1))
			return __this_address;
	} else {
		if (!xfs_verify_agbno(pag, sibling))
			return __this_address;
	}
	return NULL;
}

 
xfs_failaddr_t
__xfs_btree_check_lblock(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_btnum_t		btnum = cur->bc_btnum;
	int			crc = xfs_has_crc(mp);
	xfs_failaddr_t		fa;
	xfs_fsblock_t		fsb = NULLFSBLOCK;

	if (crc) {
		if (!uuid_equal(&block->bb_u.l.bb_uuid, &mp->m_sb.sb_meta_uuid))
			return __this_address;
		if (block->bb_u.l.bb_blkno !=
		    cpu_to_be64(bp ? xfs_buf_daddr(bp) : XFS_BUF_DADDR_NULL))
			return __this_address;
		if (block->bb_u.l.bb_pad != cpu_to_be32(0))
			return __this_address;
	}

	if (be32_to_cpu(block->bb_magic) != xfs_btree_magic(crc, btnum))
		return __this_address;
	if (be16_to_cpu(block->bb_level) != level)
		return __this_address;
	if (be16_to_cpu(block->bb_numrecs) >
	    cur->bc_ops->get_maxrecs(cur, level))
		return __this_address;

	if (bp)
		fsb = XFS_DADDR_TO_FSB(mp, xfs_buf_daddr(bp));

	fa = xfs_btree_check_lblock_siblings(mp, cur, level, fsb,
			block->bb_u.l.bb_leftsib);
	if (!fa)
		fa = xfs_btree_check_lblock_siblings(mp, cur, level, fsb,
				block->bb_u.l.bb_rightsib);
	return fa;
}

 
static int
xfs_btree_check_lblock(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_failaddr_t		fa;

	fa = __xfs_btree_check_lblock(cur, block, level, bp);
	if (XFS_IS_CORRUPT(mp, fa != NULL) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BTREE_CHECK_LBLOCK)) {
		if (bp)
			trace_xfs_btree_corrupt(bp, _RET_IP_);
		return -EFSCORRUPTED;
	}
	return 0;
}

 
xfs_failaddr_t
__xfs_btree_check_sblock(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	struct xfs_perag	*pag = cur->bc_ag.pag;
	xfs_btnum_t		btnum = cur->bc_btnum;
	int			crc = xfs_has_crc(mp);
	xfs_failaddr_t		fa;
	xfs_agblock_t		agbno = NULLAGBLOCK;

	if (crc) {
		if (!uuid_equal(&block->bb_u.s.bb_uuid, &mp->m_sb.sb_meta_uuid))
			return __this_address;
		if (block->bb_u.s.bb_blkno !=
		    cpu_to_be64(bp ? xfs_buf_daddr(bp) : XFS_BUF_DADDR_NULL))
			return __this_address;
	}

	if (be32_to_cpu(block->bb_magic) != xfs_btree_magic(crc, btnum))
		return __this_address;
	if (be16_to_cpu(block->bb_level) != level)
		return __this_address;
	if (be16_to_cpu(block->bb_numrecs) >
	    cur->bc_ops->get_maxrecs(cur, level))
		return __this_address;

	if (bp)
		agbno = xfs_daddr_to_agbno(mp, xfs_buf_daddr(bp));

	fa = xfs_btree_check_sblock_siblings(pag, cur, level, agbno,
			block->bb_u.s.bb_leftsib);
	if (!fa)
		fa = xfs_btree_check_sblock_siblings(pag, cur, level, agbno,
				block->bb_u.s.bb_rightsib);
	return fa;
}

 
STATIC int
xfs_btree_check_sblock(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level,
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_failaddr_t		fa;

	fa = __xfs_btree_check_sblock(cur, block, level, bp);
	if (XFS_IS_CORRUPT(mp, fa != NULL) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BTREE_CHECK_SBLOCK)) {
		if (bp)
			trace_xfs_btree_corrupt(bp, _RET_IP_);
		return -EFSCORRUPTED;
	}
	return 0;
}

 
int
xfs_btree_check_block(
	struct xfs_btree_cur	*cur,	 
	struct xfs_btree_block	*block,	 
	int			level,	 
	struct xfs_buf		*bp)	 
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return xfs_btree_check_lblock(cur, block, level, bp);
	else
		return xfs_btree_check_sblock(cur, block, level, bp);
}

 
bool
xfs_btree_check_lptr(
	struct xfs_btree_cur	*cur,
	xfs_fsblock_t		fsbno,
	int			level)
{
	if (level <= 0)
		return false;
	return xfs_verify_fsbno(cur->bc_mp, fsbno);
}

 
bool
xfs_btree_check_sptr(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		agbno,
	int			level)
{
	if (level <= 0)
		return false;
	return xfs_verify_agbno(cur->bc_ag.pag, agbno);
}

 
static int
xfs_btree_check_ptr(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*ptr,
	int				index,
	int				level)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (xfs_btree_check_lptr(cur, be64_to_cpu((&ptr->l)[index]),
				level))
			return 0;
		xfs_err(cur->bc_mp,
"Inode %llu fork %d: Corrupt btree %d pointer at level %d index %d.",
				cur->bc_ino.ip->i_ino,
				cur->bc_ino.whichfork, cur->bc_btnum,
				level, index);
	} else {
		if (xfs_btree_check_sptr(cur, be32_to_cpu((&ptr->s)[index]),
				level))
			return 0;
		xfs_err(cur->bc_mp,
"AG %u: Corrupt btree %d pointer at level %d index %d.",
				cur->bc_ag.pag->pag_agno, cur->bc_btnum,
				level, index);
	}

	return -EFSCORRUPTED;
}

#ifdef DEBUG
# define xfs_btree_debug_check_ptr	xfs_btree_check_ptr
#else
# define xfs_btree_debug_check_ptr(...)	(0)
#endif

 
void
xfs_btree_lblock_calc_crc(
	struct xfs_buf		*bp)
{
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	if (!xfs_has_crc(bp->b_mount))
		return;
	if (bip)
		block->bb_u.l.bb_lsn = cpu_to_be64(bip->bli_item.li_lsn);
	xfs_buf_update_cksum(bp, XFS_BTREE_LBLOCK_CRC_OFF);
}

bool
xfs_btree_lblock_verify_crc(
	struct xfs_buf		*bp)
{
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	struct xfs_mount	*mp = bp->b_mount;

	if (xfs_has_crc(mp)) {
		if (!xfs_log_check_lsn(mp, be64_to_cpu(block->bb_u.l.bb_lsn)))
			return false;
		return xfs_buf_verify_cksum(bp, XFS_BTREE_LBLOCK_CRC_OFF);
	}

	return true;
}

 
void
xfs_btree_sblock_calc_crc(
	struct xfs_buf		*bp)
{
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	struct xfs_buf_log_item	*bip = bp->b_log_item;

	if (!xfs_has_crc(bp->b_mount))
		return;
	if (bip)
		block->bb_u.s.bb_lsn = cpu_to_be64(bip->bli_item.li_lsn);
	xfs_buf_update_cksum(bp, XFS_BTREE_SBLOCK_CRC_OFF);
}

bool
xfs_btree_sblock_verify_crc(
	struct xfs_buf		*bp)
{
	struct xfs_btree_block  *block = XFS_BUF_TO_BLOCK(bp);
	struct xfs_mount	*mp = bp->b_mount;

	if (xfs_has_crc(mp)) {
		if (!xfs_log_check_lsn(mp, be64_to_cpu(block->bb_u.s.bb_lsn)))
			return false;
		return xfs_buf_verify_cksum(bp, XFS_BTREE_SBLOCK_CRC_OFF);
	}

	return true;
}

static int
xfs_btree_free_block(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp)
{
	int			error;

	error = cur->bc_ops->free_block(cur, bp);
	if (!error) {
		xfs_trans_binval(cur->bc_tp, bp);
		XFS_BTREE_STATS_INC(cur, free);
	}
	return error;
}

 
void
xfs_btree_del_cursor(
	struct xfs_btree_cur	*cur,		 
	int			error)		 
{
	int			i;		 

	 
	for (i = 0; i < cur->bc_nlevels; i++) {
		if (cur->bc_levels[i].bp)
			xfs_trans_brelse(cur->bc_tp, cur->bc_levels[i].bp);
		else if (!error)
			break;
	}

	 
	ASSERT(cur->bc_btnum != XFS_BTNUM_BMAP || cur->bc_ino.allocated == 0 ||
	       xfs_is_shutdown(cur->bc_mp) || error != 0);
	if (unlikely(cur->bc_flags & XFS_BTREE_STAGING))
		kmem_free(cur->bc_ops);
	if (!(cur->bc_flags & XFS_BTREE_LONG_PTRS) && cur->bc_ag.pag)
		xfs_perag_put(cur->bc_ag.pag);
	kmem_cache_free(cur->bc_cache, cur);
}

 
int					 
xfs_btree_dup_cursor(
	struct xfs_btree_cur *cur,		 
	struct xfs_btree_cur **ncur)		 
{
	struct xfs_buf	*bp;		 
	int		error;		 
	int		i;		 
	xfs_mount_t	*mp;		 
	struct xfs_btree_cur *new;		 
	xfs_trans_t	*tp;		 

	tp = cur->bc_tp;
	mp = cur->bc_mp;

	 
	new = cur->bc_ops->dup_cursor(cur);

	 
	new->bc_rec = cur->bc_rec;

	 
	for (i = 0; i < new->bc_nlevels; i++) {
		new->bc_levels[i].ptr = cur->bc_levels[i].ptr;
		new->bc_levels[i].ra = cur->bc_levels[i].ra;
		bp = cur->bc_levels[i].bp;
		if (bp) {
			error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
						   xfs_buf_daddr(bp), mp->m_bsize,
						   0, &bp,
						   cur->bc_ops->buf_ops);
			if (error) {
				xfs_btree_del_cursor(new, error);
				*ncur = NULL;
				return error;
			}
		}
		new->bc_levels[i].bp = bp;
	}
	*ncur = new;
	return 0;
}

 

 
static inline size_t xfs_btree_block_len(struct xfs_btree_cur *cur)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (cur->bc_flags & XFS_BTREE_CRC_BLOCKS)
			return XFS_BTREE_LBLOCK_CRC_LEN;
		return XFS_BTREE_LBLOCK_LEN;
	}
	if (cur->bc_flags & XFS_BTREE_CRC_BLOCKS)
		return XFS_BTREE_SBLOCK_CRC_LEN;
	return XFS_BTREE_SBLOCK_LEN;
}

 
static inline size_t xfs_btree_ptr_len(struct xfs_btree_cur *cur)
{
	return (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
		sizeof(__be64) : sizeof(__be32);
}

 
STATIC size_t
xfs_btree_rec_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->rec_len;
}

 
STATIC size_t
xfs_btree_key_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->key_len;
}

 
STATIC size_t
xfs_btree_high_key_offset(
	struct xfs_btree_cur	*cur,
	int			n)
{
	return xfs_btree_block_len(cur) +
		(n - 1) * cur->bc_ops->key_len + (cur->bc_ops->key_len / 2);
}

 
STATIC size_t
xfs_btree_ptr_offset(
	struct xfs_btree_cur	*cur,
	int			n,
	int			level)
{
	return xfs_btree_block_len(cur) +
		cur->bc_ops->get_maxrecs(cur, level) * cur->bc_ops->key_len +
		(n - 1) * xfs_btree_ptr_len(cur);
}

 
union xfs_btree_rec *
xfs_btree_rec_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_rec *)
		((char *)block + xfs_btree_rec_offset(cur, n));
}

 
union xfs_btree_key *
xfs_btree_key_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_key *)
		((char *)block + xfs_btree_key_offset(cur, n));
}

 
union xfs_btree_key *
xfs_btree_high_key_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	return (union xfs_btree_key *)
		((char *)block + xfs_btree_high_key_offset(cur, n));
}

 
union xfs_btree_ptr *
xfs_btree_ptr_addr(
	struct xfs_btree_cur	*cur,
	int			n,
	struct xfs_btree_block	*block)
{
	int			level = xfs_btree_get_level(block);

	ASSERT(block->bb_level != 0);

	return (union xfs_btree_ptr *)
		((char *)block + xfs_btree_ptr_offset(cur, n, level));
}

struct xfs_ifork *
xfs_btree_ifork_ptr(
	struct xfs_btree_cur	*cur)
{
	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);

	if (cur->bc_flags & XFS_BTREE_STAGING)
		return cur->bc_ino.ifake->if_fork;
	return xfs_ifork_ptr(cur->bc_ino.ip, cur->bc_ino.whichfork);
}

 
STATIC struct xfs_btree_block *
xfs_btree_get_iroot(
	struct xfs_btree_cur	*cur)
{
	struct xfs_ifork	*ifp = xfs_btree_ifork_ptr(cur);

	return (struct xfs_btree_block *)ifp->if_broot;
}

 
struct xfs_btree_block *		 
xfs_btree_get_block(
	struct xfs_btree_cur	*cur,	 
	int			level,	 
	struct xfs_buf		**bpp)	 
{
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1)) {
		*bpp = NULL;
		return xfs_btree_get_iroot(cur);
	}

	*bpp = cur->bc_levels[level].bp;
	return XFS_BUF_TO_BLOCK(*bpp);
}

 
STATIC int				 
xfs_btree_firstrec(
	struct xfs_btree_cur	*cur,	 
	int			level)	 
{
	struct xfs_btree_block	*block;	 
	struct xfs_buf		*bp;	 

	 
	block = xfs_btree_get_block(cur, level, &bp);
	if (xfs_btree_check_block(cur, block, level, bp))
		return 0;
	 
	if (!block->bb_numrecs)
		return 0;
	 
	cur->bc_levels[level].ptr = 1;
	return 1;
}

 
STATIC int				 
xfs_btree_lastrec(
	struct xfs_btree_cur	*cur,	 
	int			level)	 
{
	struct xfs_btree_block	*block;	 
	struct xfs_buf		*bp;	 

	 
	block = xfs_btree_get_block(cur, level, &bp);
	if (xfs_btree_check_block(cur, block, level, bp))
		return 0;
	 
	if (!block->bb_numrecs)
		return 0;
	 
	cur->bc_levels[level].ptr = be16_to_cpu(block->bb_numrecs);
	return 1;
}

 
void
xfs_btree_offsets(
	uint32_t	fields,		 
	const short	*offsets,	 
	int		nbits,		 
	int		*first,		 
	int		*last)		 
{
	int		i;		 
	uint32_t	imask;		 

	ASSERT(fields != 0);
	 
	for (i = 0, imask = 1u; ; i++, imask <<= 1) {
		if (imask & fields) {
			*first = offsets[i];
			break;
		}
	}
	 
	for (i = nbits - 1, imask = 1u << i; ; i--, imask >>= 1) {
		if (imask & fields) {
			*last = offsets[i + 1] - 1;
			break;
		}
	}
}

 
int
xfs_btree_read_bufl(
	struct xfs_mount	*mp,		 
	struct xfs_trans	*tp,		 
	xfs_fsblock_t		fsbno,		 
	struct xfs_buf		**bpp,		 
	int			refval,		 
	const struct xfs_buf_ops *ops)
{
	struct xfs_buf		*bp;		 
	xfs_daddr_t		d;		 
	int			error;

	if (!xfs_verify_fsbno(mp, fsbno))
		return -EFSCORRUPTED;
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp, d,
				   mp->m_bsize, 0, &bp, ops);
	if (error)
		return error;
	if (bp)
		xfs_buf_set_ref(bp, refval);
	*bpp = bp;
	return 0;
}

 
 
void
xfs_btree_reada_bufl(
	struct xfs_mount	*mp,		 
	xfs_fsblock_t		fsbno,		 
	xfs_extlen_t		count,		 
	const struct xfs_buf_ops *ops)
{
	xfs_daddr_t		d;

	ASSERT(fsbno != NULLFSBLOCK);
	d = XFS_FSB_TO_DADDR(mp, fsbno);
	xfs_buf_readahead(mp->m_ddev_targp, d, mp->m_bsize * count, ops);
}

 
 
void
xfs_btree_reada_bufs(
	struct xfs_mount	*mp,		 
	xfs_agnumber_t		agno,		 
	xfs_agblock_t		agbno,		 
	xfs_extlen_t		count,		 
	const struct xfs_buf_ops *ops)
{
	xfs_daddr_t		d;

	ASSERT(agno != NULLAGNUMBER);
	ASSERT(agbno != NULLAGBLOCK);
	d = XFS_AGB_TO_DADDR(mp, agno, agbno);
	xfs_buf_readahead(mp->m_ddev_targp, d, mp->m_bsize * count, ops);
}

STATIC int
xfs_btree_readahead_lblock(
	struct xfs_btree_cur	*cur,
	int			lr,
	struct xfs_btree_block	*block)
{
	int			rval = 0;
	xfs_fsblock_t		left = be64_to_cpu(block->bb_u.l.bb_leftsib);
	xfs_fsblock_t		right = be64_to_cpu(block->bb_u.l.bb_rightsib);

	if ((lr & XFS_BTCUR_LEFTRA) && left != NULLFSBLOCK) {
		xfs_btree_reada_bufl(cur->bc_mp, left, 1,
				     cur->bc_ops->buf_ops);
		rval++;
	}

	if ((lr & XFS_BTCUR_RIGHTRA) && right != NULLFSBLOCK) {
		xfs_btree_reada_bufl(cur->bc_mp, right, 1,
				     cur->bc_ops->buf_ops);
		rval++;
	}

	return rval;
}

STATIC int
xfs_btree_readahead_sblock(
	struct xfs_btree_cur	*cur,
	int			lr,
	struct xfs_btree_block *block)
{
	int			rval = 0;
	xfs_agblock_t		left = be32_to_cpu(block->bb_u.s.bb_leftsib);
	xfs_agblock_t		right = be32_to_cpu(block->bb_u.s.bb_rightsib);


	if ((lr & XFS_BTCUR_LEFTRA) && left != NULLAGBLOCK) {
		xfs_btree_reada_bufs(cur->bc_mp, cur->bc_ag.pag->pag_agno,
				     left, 1, cur->bc_ops->buf_ops);
		rval++;
	}

	if ((lr & XFS_BTCUR_RIGHTRA) && right != NULLAGBLOCK) {
		xfs_btree_reada_bufs(cur->bc_mp, cur->bc_ag.pag->pag_agno,
				     right, 1, cur->bc_ops->buf_ops);
		rval++;
	}

	return rval;
}

 
STATIC int
xfs_btree_readahead(
	struct xfs_btree_cur	*cur,		 
	int			lev,		 
	int			lr)		 
{
	struct xfs_btree_block	*block;

	 
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (lev == cur->bc_nlevels - 1))
		return 0;

	if ((cur->bc_levels[lev].ra | lr) == cur->bc_levels[lev].ra)
		return 0;

	cur->bc_levels[lev].ra |= lr;
	block = XFS_BUF_TO_BLOCK(cur->bc_levels[lev].bp);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return xfs_btree_readahead_lblock(cur, lr, block);
	return xfs_btree_readahead_sblock(cur, lr, block);
}

STATIC int
xfs_btree_ptr_to_daddr(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*ptr,
	xfs_daddr_t			*daddr)
{
	xfs_fsblock_t		fsbno;
	xfs_agblock_t		agbno;
	int			error;

	error = xfs_btree_check_ptr(cur, ptr, 0, 1);
	if (error)
		return error;

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		fsbno = be64_to_cpu(ptr->l);
		*daddr = XFS_FSB_TO_DADDR(cur->bc_mp, fsbno);
	} else {
		agbno = be32_to_cpu(ptr->s);
		*daddr = XFS_AGB_TO_DADDR(cur->bc_mp, cur->bc_ag.pag->pag_agno,
				agbno);
	}

	return 0;
}

 
STATIC void
xfs_btree_readahead_ptr(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	xfs_extlen_t		count)
{
	xfs_daddr_t		daddr;

	if (xfs_btree_ptr_to_daddr(cur, ptr, &daddr))
		return;
	xfs_buf_readahead(cur->bc_mp->m_ddev_targp, daddr,
			  cur->bc_mp->m_bsize * count, cur->bc_ops->buf_ops);
}

 
STATIC void
xfs_btree_setbuf(
	struct xfs_btree_cur	*cur,	 
	int			lev,	 
	struct xfs_buf		*bp)	 
{
	struct xfs_btree_block	*b;	 

	if (cur->bc_levels[lev].bp)
		xfs_trans_brelse(cur->bc_tp, cur->bc_levels[lev].bp);
	cur->bc_levels[lev].bp = bp;
	cur->bc_levels[lev].ra = 0;

	b = XFS_BUF_TO_BLOCK(bp);
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (b->bb_u.l.bb_leftsib == cpu_to_be64(NULLFSBLOCK))
			cur->bc_levels[lev].ra |= XFS_BTCUR_LEFTRA;
		if (b->bb_u.l.bb_rightsib == cpu_to_be64(NULLFSBLOCK))
			cur->bc_levels[lev].ra |= XFS_BTCUR_RIGHTRA;
	} else {
		if (b->bb_u.s.bb_leftsib == cpu_to_be32(NULLAGBLOCK))
			cur->bc_levels[lev].ra |= XFS_BTCUR_LEFTRA;
		if (b->bb_u.s.bb_rightsib == cpu_to_be32(NULLAGBLOCK))
			cur->bc_levels[lev].ra |= XFS_BTCUR_RIGHTRA;
	}
}

bool
xfs_btree_ptr_is_null(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return ptr->l == cpu_to_be64(NULLFSBLOCK);
	else
		return ptr->s == cpu_to_be32(NULLAGBLOCK);
}

void
xfs_btree_set_ptr_null(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		ptr->l = cpu_to_be64(NULLFSBLOCK);
	else
		ptr->s = cpu_to_be32(NULLAGBLOCK);
}

 
void
xfs_btree_get_sibling(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_ptr	*ptr,
	int			lr)
{
	ASSERT(lr == XFS_BB_LEFTSIB || lr == XFS_BB_RIGHTSIB);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (lr == XFS_BB_RIGHTSIB)
			ptr->l = block->bb_u.l.bb_rightsib;
		else
			ptr->l = block->bb_u.l.bb_leftsib;
	} else {
		if (lr == XFS_BB_RIGHTSIB)
			ptr->s = block->bb_u.s.bb_rightsib;
		else
			ptr->s = block->bb_u.s.bb_leftsib;
	}
}

void
xfs_btree_set_sibling(
	struct xfs_btree_cur		*cur,
	struct xfs_btree_block		*block,
	const union xfs_btree_ptr	*ptr,
	int				lr)
{
	ASSERT(lr == XFS_BB_LEFTSIB || lr == XFS_BB_RIGHTSIB);

	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (lr == XFS_BB_RIGHTSIB)
			block->bb_u.l.bb_rightsib = ptr->l;
		else
			block->bb_u.l.bb_leftsib = ptr->l;
	} else {
		if (lr == XFS_BB_RIGHTSIB)
			block->bb_u.s.bb_rightsib = ptr->s;
		else
			block->bb_u.s.bb_leftsib = ptr->s;
	}
}

void
xfs_btree_init_block_int(
	struct xfs_mount	*mp,
	struct xfs_btree_block	*buf,
	xfs_daddr_t		blkno,
	xfs_btnum_t		btnum,
	__u16			level,
	__u16			numrecs,
	__u64			owner,
	unsigned int		flags)
{
	int			crc = xfs_has_crc(mp);
	__u32			magic = xfs_btree_magic(crc, btnum);

	buf->bb_magic = cpu_to_be32(magic);
	buf->bb_level = cpu_to_be16(level);
	buf->bb_numrecs = cpu_to_be16(numrecs);

	if (flags & XFS_BTREE_LONG_PTRS) {
		buf->bb_u.l.bb_leftsib = cpu_to_be64(NULLFSBLOCK);
		buf->bb_u.l.bb_rightsib = cpu_to_be64(NULLFSBLOCK);
		if (crc) {
			buf->bb_u.l.bb_blkno = cpu_to_be64(blkno);
			buf->bb_u.l.bb_owner = cpu_to_be64(owner);
			uuid_copy(&buf->bb_u.l.bb_uuid, &mp->m_sb.sb_meta_uuid);
			buf->bb_u.l.bb_pad = 0;
			buf->bb_u.l.bb_lsn = 0;
		}
	} else {
		 
		__u32 __owner = (__u32)owner;

		buf->bb_u.s.bb_leftsib = cpu_to_be32(NULLAGBLOCK);
		buf->bb_u.s.bb_rightsib = cpu_to_be32(NULLAGBLOCK);
		if (crc) {
			buf->bb_u.s.bb_blkno = cpu_to_be64(blkno);
			buf->bb_u.s.bb_owner = cpu_to_be32(__owner);
			uuid_copy(&buf->bb_u.s.bb_uuid, &mp->m_sb.sb_meta_uuid);
			buf->bb_u.s.bb_lsn = 0;
		}
	}
}

void
xfs_btree_init_block(
	struct xfs_mount *mp,
	struct xfs_buf	*bp,
	xfs_btnum_t	btnum,
	__u16		level,
	__u16		numrecs,
	__u64		owner)
{
	xfs_btree_init_block_int(mp, XFS_BUF_TO_BLOCK(bp), xfs_buf_daddr(bp),
				 btnum, level, numrecs, owner, 0);
}

void
xfs_btree_init_block_cur(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			level,
	int			numrecs)
{
	__u64			owner;

	 
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		owner = cur->bc_ino.ip->i_ino;
	else
		owner = cur->bc_ag.pag->pag_agno;

	xfs_btree_init_block_int(cur->bc_mp, XFS_BUF_TO_BLOCK(bp),
				xfs_buf_daddr(bp), cur->bc_btnum, level,
				numrecs, owner, cur->bc_flags);
}

 
STATIC int
xfs_btree_is_lastrec(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	int			level)
{
	union xfs_btree_ptr	ptr;

	if (level > 0)
		return 0;
	if (!(cur->bc_flags & XFS_BTREE_LASTREC_UPDATE))
		return 0;

	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &ptr))
		return 0;
	return 1;
}

STATIC void
xfs_btree_buf_to_ptr(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	union xfs_btree_ptr	*ptr)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		ptr->l = cpu_to_be64(XFS_DADDR_TO_FSB(cur->bc_mp,
					xfs_buf_daddr(bp)));
	else {
		ptr->s = cpu_to_be32(xfs_daddr_to_agbno(cur->bc_mp,
					xfs_buf_daddr(bp)));
	}
}

STATIC void
xfs_btree_set_refs(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp)
{
	switch (cur->bc_btnum) {
	case XFS_BTNUM_BNO:
	case XFS_BTNUM_CNT:
		xfs_buf_set_ref(bp, XFS_ALLOC_BTREE_REF);
		break;
	case XFS_BTNUM_INO:
	case XFS_BTNUM_FINO:
		xfs_buf_set_ref(bp, XFS_INO_BTREE_REF);
		break;
	case XFS_BTNUM_BMAP:
		xfs_buf_set_ref(bp, XFS_BMAP_BTREE_REF);
		break;
	case XFS_BTNUM_RMAP:
		xfs_buf_set_ref(bp, XFS_RMAP_BTREE_REF);
		break;
	case XFS_BTNUM_REFC:
		xfs_buf_set_ref(bp, XFS_REFC_BTREE_REF);
		break;
	default:
		ASSERT(0);
	}
}

int
xfs_btree_get_buf_block(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*ptr,
	struct xfs_btree_block		**block,
	struct xfs_buf			**bpp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_daddr_t		d;
	int			error;

	error = xfs_btree_ptr_to_daddr(cur, ptr, &d);
	if (error)
		return error;
	error = xfs_trans_get_buf(cur->bc_tp, mp->m_ddev_targp, d, mp->m_bsize,
			0, bpp);
	if (error)
		return error;

	(*bpp)->b_ops = cur->bc_ops->buf_ops;
	*block = XFS_BUF_TO_BLOCK(*bpp);
	return 0;
}

 
STATIC int
xfs_btree_read_buf_block(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*ptr,
	int				flags,
	struct xfs_btree_block		**block,
	struct xfs_buf			**bpp)
{
	struct xfs_mount	*mp = cur->bc_mp;
	xfs_daddr_t		d;
	int			error;

	 
	ASSERT(!(flags & XBF_TRYLOCK));

	error = xfs_btree_ptr_to_daddr(cur, ptr, &d);
	if (error)
		return error;
	error = xfs_trans_read_buf(mp, cur->bc_tp, mp->m_ddev_targp, d,
				   mp->m_bsize, flags, bpp,
				   cur->bc_ops->buf_ops);
	if (error)
		return error;

	xfs_btree_set_refs(cur, *bpp);
	*block = XFS_BUF_TO_BLOCK(*bpp);
	return 0;
}

 
void
xfs_btree_copy_keys(
	struct xfs_btree_cur		*cur,
	union xfs_btree_key		*dst_key,
	const union xfs_btree_key	*src_key,
	int				numkeys)
{
	ASSERT(numkeys >= 0);
	memcpy(dst_key, src_key, numkeys * cur->bc_ops->key_len);
}

 
STATIC void
xfs_btree_copy_recs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*dst_rec,
	union xfs_btree_rec	*src_rec,
	int			numrecs)
{
	ASSERT(numrecs >= 0);
	memcpy(dst_rec, src_rec, numrecs * cur->bc_ops->rec_len);
}

 
void
xfs_btree_copy_ptrs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*dst_ptr,
	const union xfs_btree_ptr *src_ptr,
	int			numptrs)
{
	ASSERT(numptrs >= 0);
	memcpy(dst_ptr, src_ptr, numptrs * xfs_btree_ptr_len(cur));
}

 
STATIC void
xfs_btree_shift_keys(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*key,
	int			dir,
	int			numkeys)
{
	char			*dst_key;

	ASSERT(numkeys >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_key = (char *)key + (dir * cur->bc_ops->key_len);
	memmove(dst_key, key, numkeys * cur->bc_ops->key_len);
}

 
STATIC void
xfs_btree_shift_recs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec,
	int			dir,
	int			numrecs)
{
	char			*dst_rec;

	ASSERT(numrecs >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_rec = (char *)rec + (dir * cur->bc_ops->rec_len);
	memmove(dst_rec, rec, numrecs * cur->bc_ops->rec_len);
}

 
STATIC void
xfs_btree_shift_ptrs(
	struct xfs_btree_cur	*cur,
	union xfs_btree_ptr	*ptr,
	int			dir,
	int			numptrs)
{
	char			*dst_ptr;

	ASSERT(numptrs >= 0);
	ASSERT(dir == 1 || dir == -1);

	dst_ptr = (char *)ptr + (dir * xfs_btree_ptr_len(cur));
	memmove(dst_ptr, ptr, numptrs * xfs_btree_ptr_len(cur));
}

 
STATIC void
xfs_btree_log_keys(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			first,
	int			last)
{

	if (bp) {
		xfs_trans_buf_set_type(cur->bc_tp, bp, XFS_BLFT_BTREE_BUF);
		xfs_trans_log_buf(cur->bc_tp, bp,
				  xfs_btree_key_offset(cur, first),
				  xfs_btree_key_offset(cur, last + 1) - 1);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_ino.ip,
				xfs_ilog_fbroot(cur->bc_ino.whichfork));
	}
}

 
void
xfs_btree_log_recs(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			first,
	int			last)
{

	xfs_trans_buf_set_type(cur->bc_tp, bp, XFS_BLFT_BTREE_BUF);
	xfs_trans_log_buf(cur->bc_tp, bp,
			  xfs_btree_rec_offset(cur, first),
			  xfs_btree_rec_offset(cur, last + 1) - 1);

}

 
STATIC void
xfs_btree_log_ptrs(
	struct xfs_btree_cur	*cur,	 
	struct xfs_buf		*bp,	 
	int			first,	 
	int			last)	 
{

	if (bp) {
		struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
		int			level = xfs_btree_get_level(block);

		xfs_trans_buf_set_type(cur->bc_tp, bp, XFS_BLFT_BTREE_BUF);
		xfs_trans_log_buf(cur->bc_tp, bp,
				xfs_btree_ptr_offset(cur, first, level),
				xfs_btree_ptr_offset(cur, last + 1, level) - 1);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_ino.ip,
			xfs_ilog_fbroot(cur->bc_ino.whichfork));
	}

}

 
void
xfs_btree_log_block(
	struct xfs_btree_cur	*cur,	 
	struct xfs_buf		*bp,	 
	uint32_t		fields)	 
{
	int			first;	 
	int			last;	 
	static const short	soffsets[] = {	 
		offsetof(struct xfs_btree_block, bb_magic),
		offsetof(struct xfs_btree_block, bb_level),
		offsetof(struct xfs_btree_block, bb_numrecs),
		offsetof(struct xfs_btree_block, bb_u.s.bb_leftsib),
		offsetof(struct xfs_btree_block, bb_u.s.bb_rightsib),
		offsetof(struct xfs_btree_block, bb_u.s.bb_blkno),
		offsetof(struct xfs_btree_block, bb_u.s.bb_lsn),
		offsetof(struct xfs_btree_block, bb_u.s.bb_uuid),
		offsetof(struct xfs_btree_block, bb_u.s.bb_owner),
		offsetof(struct xfs_btree_block, bb_u.s.bb_crc),
		XFS_BTREE_SBLOCK_CRC_LEN
	};
	static const short	loffsets[] = {	 
		offsetof(struct xfs_btree_block, bb_magic),
		offsetof(struct xfs_btree_block, bb_level),
		offsetof(struct xfs_btree_block, bb_numrecs),
		offsetof(struct xfs_btree_block, bb_u.l.bb_leftsib),
		offsetof(struct xfs_btree_block, bb_u.l.bb_rightsib),
		offsetof(struct xfs_btree_block, bb_u.l.bb_blkno),
		offsetof(struct xfs_btree_block, bb_u.l.bb_lsn),
		offsetof(struct xfs_btree_block, bb_u.l.bb_uuid),
		offsetof(struct xfs_btree_block, bb_u.l.bb_owner),
		offsetof(struct xfs_btree_block, bb_u.l.bb_crc),
		offsetof(struct xfs_btree_block, bb_u.l.bb_pad),
		XFS_BTREE_LBLOCK_CRC_LEN
	};

	if (bp) {
		int nbits;

		if (cur->bc_flags & XFS_BTREE_CRC_BLOCKS) {
			 
			if (fields == XFS_BB_ALL_BITS)
				fields = XFS_BB_ALL_BITS_CRC;
			nbits = XFS_BB_NUM_BITS_CRC;
		} else {
			nbits = XFS_BB_NUM_BITS;
		}
		xfs_btree_offsets(fields,
				  (cur->bc_flags & XFS_BTREE_LONG_PTRS) ?
					loffsets : soffsets,
				  nbits, &first, &last);
		xfs_trans_buf_set_type(cur->bc_tp, bp, XFS_BLFT_BTREE_BUF);
		xfs_trans_log_buf(cur->bc_tp, bp, first, last);
	} else {
		xfs_trans_log_inode(cur->bc_tp, cur->bc_ino.ip,
			xfs_ilog_fbroot(cur->bc_ino.whichfork));
	}
}

 
int						 
xfs_btree_increment(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		 
{
	struct xfs_btree_block	*block;
	union xfs_btree_ptr	ptr;
	struct xfs_buf		*bp;
	int			error;		 
	int			lev;

	ASSERT(level < cur->bc_nlevels);

	 
	xfs_btree_readahead(cur, level, XFS_BTCUR_RIGHTRA);

	 
	block = xfs_btree_get_block(cur, level, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	 
	if (++cur->bc_levels[level].ptr <= xfs_btree_get_numrecs(block))
		goto out1;

	 
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &ptr))
		goto out0;

	XFS_BTREE_STATS_INC(cur, increment);

	 
	for (lev = level + 1; lev < cur->bc_nlevels; lev++) {
		block = xfs_btree_get_block(cur, lev, &bp);

#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, lev, bp);
		if (error)
			goto error0;
#endif

		if (++cur->bc_levels[lev].ptr <= xfs_btree_get_numrecs(block))
			break;

		 
		xfs_btree_readahead(cur, lev, XFS_BTCUR_RIGHTRA);
	}

	 
	if (lev == cur->bc_nlevels) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
			goto out0;
		ASSERT(0);
		error = -EFSCORRUPTED;
		goto error0;
	}
	ASSERT(lev < cur->bc_nlevels);

	 
	for (block = xfs_btree_get_block(cur, lev, &bp); lev > level; ) {
		union xfs_btree_ptr	*ptrp;

		ptrp = xfs_btree_ptr_addr(cur, cur->bc_levels[lev].ptr, block);
		--lev;
		error = xfs_btree_read_buf_block(cur, ptrp, 0, &block, &bp);
		if (error)
			goto error0;

		xfs_btree_setbuf(cur, lev, bp);
		cur->bc_levels[lev].ptr = 1;
	}
out1:
	*stat = 1;
	return 0;

out0:
	*stat = 0;
	return 0;

error0:
	return error;
}

 
int						 
xfs_btree_decrement(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		 
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	int			error;		 
	int			lev;
	union xfs_btree_ptr	ptr;

	ASSERT(level < cur->bc_nlevels);

	 
	xfs_btree_readahead(cur, level, XFS_BTCUR_LEFTRA);

	 
	if (--cur->bc_levels[level].ptr > 0)
		goto out1;

	 
	block = xfs_btree_get_block(cur, level, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	 
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_LEFTSIB);
	if (xfs_btree_ptr_is_null(cur, &ptr))
		goto out0;

	XFS_BTREE_STATS_INC(cur, decrement);

	 
	for (lev = level + 1; lev < cur->bc_nlevels; lev++) {
		if (--cur->bc_levels[lev].ptr > 0)
			break;
		 
		xfs_btree_readahead(cur, lev, XFS_BTCUR_LEFTRA);
	}

	 
	if (lev == cur->bc_nlevels) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE)
			goto out0;
		ASSERT(0);
		error = -EFSCORRUPTED;
		goto error0;
	}
	ASSERT(lev < cur->bc_nlevels);

	 
	for (block = xfs_btree_get_block(cur, lev, &bp); lev > level; ) {
		union xfs_btree_ptr	*ptrp;

		ptrp = xfs_btree_ptr_addr(cur, cur->bc_levels[lev].ptr, block);
		--lev;
		error = xfs_btree_read_buf_block(cur, ptrp, 0, &block, &bp);
		if (error)
			goto error0;
		xfs_btree_setbuf(cur, lev, bp);
		cur->bc_levels[lev].ptr = xfs_btree_get_numrecs(block);
	}
out1:
	*stat = 1;
	return 0;

out0:
	*stat = 0;
	return 0;

error0:
	return error;
}

int
xfs_btree_lookup_get_block(
	struct xfs_btree_cur		*cur,	 
	int				level,	 
	const union xfs_btree_ptr	*pp,	 
	struct xfs_btree_block		**blkp)  
{
	struct xfs_buf		*bp;	 
	xfs_daddr_t		daddr;
	int			error = 0;

	 
	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1)) {
		*blkp = xfs_btree_get_iroot(cur);
		return 0;
	}

	 
	bp = cur->bc_levels[level].bp;
	error = xfs_btree_ptr_to_daddr(cur, pp, &daddr);
	if (error)
		return error;
	if (bp && xfs_buf_daddr(bp) == daddr) {
		*blkp = XFS_BUF_TO_BLOCK(bp);
		return 0;
	}

	error = xfs_btree_read_buf_block(cur, pp, 0, blkp, &bp);
	if (error)
		return error;

	 
	if (xfs_has_crc(cur->bc_mp) &&
	    !(cur->bc_ino.flags & XFS_BTCUR_BMBT_INVALID_OWNER) &&
	    (cur->bc_flags & XFS_BTREE_LONG_PTRS) &&
	    be64_to_cpu((*blkp)->bb_u.l.bb_owner) !=
			cur->bc_ino.ip->i_ino)
		goto out_bad;

	 
	if (be16_to_cpu((*blkp)->bb_level) != level)
		goto out_bad;

	 
	if (level != 0 && be16_to_cpu((*blkp)->bb_numrecs) == 0)
		goto out_bad;

	xfs_btree_setbuf(cur, level, bp);
	return 0;

out_bad:
	*blkp = NULL;
	xfs_buf_mark_corrupt(bp);
	xfs_trans_brelse(cur->bc_tp, bp);
	return -EFSCORRUPTED;
}

 
STATIC union xfs_btree_key *
xfs_lookup_get_search_key(
	struct xfs_btree_cur	*cur,
	int			level,
	int			keyno,
	struct xfs_btree_block	*block,
	union xfs_btree_key	*kp)
{
	if (level == 0) {
		cur->bc_ops->init_key_from_rec(kp,
				xfs_btree_rec_addr(cur, keyno, block));
		return kp;
	}

	return xfs_btree_key_addr(cur, keyno, block);
}

 
int					 
xfs_btree_lookup(
	struct xfs_btree_cur	*cur,	 
	xfs_lookup_t		dir,	 
	int			*stat)	 
{
	struct xfs_btree_block	*block;	 
	int64_t			diff;	 
	int			error;	 
	int			keyno;	 
	int			level;	 
	union xfs_btree_ptr	*pp;	 
	union xfs_btree_ptr	ptr;	 

	XFS_BTREE_STATS_INC(cur, lookup);

	 
	if (XFS_IS_CORRUPT(cur->bc_mp, cur->bc_nlevels == 0))
		return -EFSCORRUPTED;

	block = NULL;
	keyno = 0;

	 
	cur->bc_ops->init_ptr_from_cur(cur, &ptr);
	pp = &ptr;

	 
	for (level = cur->bc_nlevels - 1, diff = 1; level >= 0; level--) {
		 
		error = xfs_btree_lookup_get_block(cur, level, pp, &block);
		if (error)
			goto error0;

		if (diff == 0) {
			 
			keyno = 1;
		} else {
			 

			int	high;	 
			int	low;	 

			 
			low = 1;
			high = xfs_btree_get_numrecs(block);
			if (!high) {
				 
				if (level != 0 || cur->bc_nlevels != 1) {
					XFS_CORRUPTION_ERROR(__func__,
							XFS_ERRLEVEL_LOW,
							cur->bc_mp, block,
							sizeof(*block));
					return -EFSCORRUPTED;
				}

				cur->bc_levels[0].ptr = dir != XFS_LOOKUP_LE;
				*stat = 0;
				return 0;
			}

			 
			while (low <= high) {
				union xfs_btree_key	key;
				union xfs_btree_key	*kp;

				XFS_BTREE_STATS_INC(cur, compare);

				 
				keyno = (low + high) >> 1;

				 
				kp = xfs_lookup_get_search_key(cur, level,
						keyno, block, &key);

				 
				diff = cur->bc_ops->key_diff(cur, kp);
				if (diff < 0)
					low = keyno + 1;
				else if (diff > 0)
					high = keyno - 1;
				else
					break;
			}
		}

		 
		if (level > 0) {
			 
			if (diff > 0 && --keyno < 1)
				keyno = 1;
			pp = xfs_btree_ptr_addr(cur, keyno, block);

			error = xfs_btree_debug_check_ptr(cur, pp, 0, level);
			if (error)
				goto error0;

			cur->bc_levels[level].ptr = keyno;
		}
	}

	 
	if (dir != XFS_LOOKUP_LE && diff < 0) {
		keyno++;
		 
		xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
		if (dir == XFS_LOOKUP_GE &&
		    keyno > xfs_btree_get_numrecs(block) &&
		    !xfs_btree_ptr_is_null(cur, &ptr)) {
			int	i;

			cur->bc_levels[0].ptr = keyno;
			error = xfs_btree_increment(cur, 0, &i);
			if (error)
				goto error0;
			if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
				return -EFSCORRUPTED;
			*stat = 1;
			return 0;
		}
	} else if (dir == XFS_LOOKUP_LE && diff > 0)
		keyno--;
	cur->bc_levels[0].ptr = keyno;

	 
	if (keyno == 0 || keyno > xfs_btree_get_numrecs(block))
		*stat = 0;
	else if (dir != XFS_LOOKUP_EQ || diff == 0)
		*stat = 1;
	else
		*stat = 0;
	return 0;

error0:
	return error;
}

 
union xfs_btree_key *
xfs_btree_high_key_from_key(
	struct xfs_btree_cur	*cur,
	union xfs_btree_key	*key)
{
	ASSERT(cur->bc_flags & XFS_BTREE_OVERLAPPING);
	return (union xfs_btree_key *)((char *)key +
			(cur->bc_ops->key_len / 2));
}

 
STATIC void
xfs_btree_get_leaf_keys(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_key	*key)
{
	union xfs_btree_key	max_hkey;
	union xfs_btree_key	hkey;
	union xfs_btree_rec	*rec;
	union xfs_btree_key	*high;
	int			n;

	rec = xfs_btree_rec_addr(cur, 1, block);
	cur->bc_ops->init_key_from_rec(key, rec);

	if (cur->bc_flags & XFS_BTREE_OVERLAPPING) {

		cur->bc_ops->init_high_key_from_rec(&max_hkey, rec);
		for (n = 2; n <= xfs_btree_get_numrecs(block); n++) {
			rec = xfs_btree_rec_addr(cur, n, block);
			cur->bc_ops->init_high_key_from_rec(&hkey, rec);
			if (xfs_btree_keycmp_gt(cur, &hkey, &max_hkey))
				max_hkey = hkey;
		}

		high = xfs_btree_high_key_from_key(cur, key);
		memcpy(high, &max_hkey, cur->bc_ops->key_len / 2);
	}
}

 
STATIC void
xfs_btree_get_node_keys(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_key	*key)
{
	union xfs_btree_key	*hkey;
	union xfs_btree_key	*max_hkey;
	union xfs_btree_key	*high;
	int			n;

	if (cur->bc_flags & XFS_BTREE_OVERLAPPING) {
		memcpy(key, xfs_btree_key_addr(cur, 1, block),
				cur->bc_ops->key_len / 2);

		max_hkey = xfs_btree_high_key_addr(cur, 1, block);
		for (n = 2; n <= xfs_btree_get_numrecs(block); n++) {
			hkey = xfs_btree_high_key_addr(cur, n, block);
			if (xfs_btree_keycmp_gt(cur, hkey, max_hkey))
				max_hkey = hkey;
		}

		high = xfs_btree_high_key_from_key(cur, key);
		memcpy(high, max_hkey, cur->bc_ops->key_len / 2);
	} else {
		memcpy(key, xfs_btree_key_addr(cur, 1, block),
				cur->bc_ops->key_len);
	}
}

 
void
xfs_btree_get_keys(
	struct xfs_btree_cur	*cur,
	struct xfs_btree_block	*block,
	union xfs_btree_key	*key)
{
	if (be16_to_cpu(block->bb_level) == 0)
		xfs_btree_get_leaf_keys(cur, block, key);
	else
		xfs_btree_get_node_keys(cur, block, key);
}

 
static inline bool
xfs_btree_needs_key_update(
	struct xfs_btree_cur	*cur,
	int			ptr)
{
	return (cur->bc_flags & XFS_BTREE_OVERLAPPING) || ptr == 1;
}

 
STATIC int
__xfs_btree_updkeys(
	struct xfs_btree_cur	*cur,
	int			level,
	struct xfs_btree_block	*block,
	struct xfs_buf		*bp0,
	bool			force_all)
{
	union xfs_btree_key	key;	 
	union xfs_btree_key	*lkey;	 
	union xfs_btree_key	*hkey;
	union xfs_btree_key	*nlkey;	 
	union xfs_btree_key	*nhkey;
	struct xfs_buf		*bp;
	int			ptr;

	ASSERT(cur->bc_flags & XFS_BTREE_OVERLAPPING);

	 
	if (level + 1 >= cur->bc_nlevels)
		return 0;

	trace_xfs_btree_updkeys(cur, level, bp0);

	lkey = &key;
	hkey = xfs_btree_high_key_from_key(cur, lkey);
	xfs_btree_get_keys(cur, block, lkey);
	for (level++; level < cur->bc_nlevels; level++) {
#ifdef DEBUG
		int		error;
#endif
		block = xfs_btree_get_block(cur, level, &bp);
		trace_xfs_btree_updkeys(cur, level, bp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, level, bp);
		if (error)
			return error;
#endif
		ptr = cur->bc_levels[level].ptr;
		nlkey = xfs_btree_key_addr(cur, ptr, block);
		nhkey = xfs_btree_high_key_addr(cur, ptr, block);
		if (!force_all &&
		    xfs_btree_keycmp_eq(cur, nlkey, lkey) &&
		    xfs_btree_keycmp_eq(cur, nhkey, hkey))
			break;
		xfs_btree_copy_keys(cur, nlkey, lkey, 1);
		xfs_btree_log_keys(cur, bp, ptr, ptr);
		if (level + 1 >= cur->bc_nlevels)
			break;
		xfs_btree_get_node_keys(cur, block, lkey);
	}

	return 0;
}

 
STATIC int
xfs_btree_updkeys_force(
	struct xfs_btree_cur	*cur,
	int			level)
{
	struct xfs_buf		*bp;
	struct xfs_btree_block	*block;

	block = xfs_btree_get_block(cur, level, &bp);
	return __xfs_btree_updkeys(cur, level, block, bp, true);
}

 
STATIC int
xfs_btree_update_keys(
	struct xfs_btree_cur	*cur,
	int			level)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	union xfs_btree_key	*kp;
	union xfs_btree_key	key;
	int			ptr;

	ASSERT(level >= 0);

	block = xfs_btree_get_block(cur, level, &bp);
	if (cur->bc_flags & XFS_BTREE_OVERLAPPING)
		return __xfs_btree_updkeys(cur, level, block, bp, false);

	 
	xfs_btree_get_keys(cur, block, &key);
	for (level++, ptr = 1; ptr == 1 && level < cur->bc_nlevels; level++) {
#ifdef DEBUG
		int		error;
#endif
		block = xfs_btree_get_block(cur, level, &bp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, block, level, bp);
		if (error)
			return error;
#endif
		ptr = cur->bc_levels[level].ptr;
		kp = xfs_btree_key_addr(cur, ptr, block);
		xfs_btree_copy_keys(cur, kp, &key, 1);
		xfs_btree_log_keys(cur, bp, ptr, ptr);
	}

	return 0;
}

 
int
xfs_btree_update(
	struct xfs_btree_cur	*cur,
	union xfs_btree_rec	*rec)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	int			error;
	int			ptr;
	union xfs_btree_rec	*rp;

	 
	block = xfs_btree_get_block(cur, 0, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, 0, bp);
	if (error)
		goto error0;
#endif
	 
	ptr = cur->bc_levels[0].ptr;
	rp = xfs_btree_rec_addr(cur, ptr, block);

	 
	xfs_btree_copy_recs(cur, rp, rec, 1);
	xfs_btree_log_recs(cur, bp, ptr, ptr);

	 
	if (xfs_btree_is_lastrec(cur, block, 0)) {
		cur->bc_ops->update_lastrec(cur, block, rec,
					    ptr, LASTREC_UPDATE);
	}

	 
	if (xfs_btree_needs_key_update(cur, ptr)) {
		error = xfs_btree_update_keys(cur, 0);
		if (error)
			goto error0;
	}

	return 0;

error0:
	return error;
}

 
STATIC int					 
xfs_btree_lshift(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		 
{
	struct xfs_buf		*lbp;		 
	struct xfs_btree_block	*left;		 
	int			lrecs;		 
	struct xfs_buf		*rbp;		 
	struct xfs_btree_block	*right;		 
	struct xfs_btree_cur	*tcur;		 
	int			rrecs;		 
	union xfs_btree_ptr	lptr;		 
	union xfs_btree_key	*rkp = NULL;	 
	union xfs_btree_ptr	*rpp = NULL;	 
	union xfs_btree_rec	*rrp = NULL;	 
	int			error;		 
	int			i;

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1)
		goto out0;

	 
	right = xfs_btree_get_block(cur, level, &rbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, right, level, rbp);
	if (error)
		goto error0;
#endif

	 
	xfs_btree_get_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
	if (xfs_btree_ptr_is_null(cur, &lptr))
		goto out0;

	 
	if (cur->bc_levels[level].ptr <= 1)
		goto out0;

	 
	error = xfs_btree_read_buf_block(cur, &lptr, 0, &left, &lbp);
	if (error)
		goto error0;

	 
	lrecs = xfs_btree_get_numrecs(left);
	if (lrecs == cur->bc_ops->get_maxrecs(cur, level))
		goto out0;

	rrecs = xfs_btree_get_numrecs(right);

	 
	lrecs++;
	rrecs--;

	XFS_BTREE_STATS_INC(cur, lshift);
	XFS_BTREE_STATS_ADD(cur, moves, 1);

	 
	if (level > 0) {
		 
		union xfs_btree_key	*lkp;	 
		union xfs_btree_ptr	*lpp;	 

		lkp = xfs_btree_key_addr(cur, lrecs, left);
		rkp = xfs_btree_key_addr(cur, 1, right);

		lpp = xfs_btree_ptr_addr(cur, lrecs, left);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

		error = xfs_btree_debug_check_ptr(cur, rpp, 0, level);
		if (error)
			goto error0;

		xfs_btree_copy_keys(cur, lkp, rkp, 1);
		xfs_btree_copy_ptrs(cur, lpp, rpp, 1);

		xfs_btree_log_keys(cur, lbp, lrecs, lrecs);
		xfs_btree_log_ptrs(cur, lbp, lrecs, lrecs);

		ASSERT(cur->bc_ops->keys_inorder(cur,
			xfs_btree_key_addr(cur, lrecs - 1, left), lkp));
	} else {
		 
		union xfs_btree_rec	*lrp;	 

		lrp = xfs_btree_rec_addr(cur, lrecs, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, lrp, rrp, 1);
		xfs_btree_log_recs(cur, lbp, lrecs, lrecs);

		ASSERT(cur->bc_ops->recs_inorder(cur,
			xfs_btree_rec_addr(cur, lrecs - 1, left), lrp));
	}

	xfs_btree_set_numrecs(left, lrecs);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS);

	xfs_btree_set_numrecs(right, rrecs);
	xfs_btree_log_block(cur, rbp, XFS_BB_NUMRECS);

	 
	XFS_BTREE_STATS_ADD(cur, moves, rrecs - 1);
	if (level > 0) {
		 
		for (i = 0; i < rrecs; i++) {
			error = xfs_btree_debug_check_ptr(cur, rpp, i + 1, level);
			if (error)
				goto error0;
		}

		xfs_btree_shift_keys(cur,
				xfs_btree_key_addr(cur, 2, right),
				-1, rrecs);
		xfs_btree_shift_ptrs(cur,
				xfs_btree_ptr_addr(cur, 2, right),
				-1, rrecs);

		xfs_btree_log_keys(cur, rbp, 1, rrecs);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs);
	} else {
		 
		xfs_btree_shift_recs(cur,
			xfs_btree_rec_addr(cur, 2, right),
			-1, rrecs);
		xfs_btree_log_recs(cur, rbp, 1, rrecs);
	}

	 
	if (cur->bc_flags & XFS_BTREE_OVERLAPPING) {
		error = xfs_btree_dup_cursor(cur, &tcur);
		if (error)
			goto error0;
		i = xfs_btree_firstrec(tcur, level);
		if (XFS_IS_CORRUPT(tcur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		error = xfs_btree_decrement(tcur, level, &i);
		if (error)
			goto error1;

		 
		error = xfs_btree_update_keys(tcur, level);
		if (error)
			goto error1;

		xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
	}

	 
	error = xfs_btree_update_keys(cur, level);
	if (error)
		goto error0;

	 
	cur->bc_levels[level].ptr--;

	*stat = 1;
	return 0;

out0:
	*stat = 0;
	return 0;

error0:
	return error;

error1:
	xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int					 
xfs_btree_rshift(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)		 
{
	struct xfs_buf		*lbp;		 
	struct xfs_btree_block	*left;		 
	struct xfs_buf		*rbp;		 
	struct xfs_btree_block	*right;		 
	struct xfs_btree_cur	*tcur;		 
	union xfs_btree_ptr	rptr;		 
	union xfs_btree_key	*rkp;		 
	int			rrecs;		 
	int			lrecs;		 
	int			error;		 
	int			i;		 

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level == cur->bc_nlevels - 1))
		goto out0;

	 
	left = xfs_btree_get_block(cur, level, &lbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, left, level, lbp);
	if (error)
		goto error0;
#endif

	 
	xfs_btree_get_sibling(cur, left, &rptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &rptr))
		goto out0;

	 
	lrecs = xfs_btree_get_numrecs(left);
	if (cur->bc_levels[level].ptr >= lrecs)
		goto out0;

	 
	error = xfs_btree_read_buf_block(cur, &rptr, 0, &right, &rbp);
	if (error)
		goto error0;

	 
	rrecs = xfs_btree_get_numrecs(right);
	if (rrecs == cur->bc_ops->get_maxrecs(cur, level))
		goto out0;

	XFS_BTREE_STATS_INC(cur, rshift);
	XFS_BTREE_STATS_ADD(cur, moves, rrecs);

	 
	if (level > 0) {
		 
		union xfs_btree_key	*lkp;
		union xfs_btree_ptr	*lpp;
		union xfs_btree_ptr	*rpp;

		lkp = xfs_btree_key_addr(cur, lrecs, left);
		lpp = xfs_btree_ptr_addr(cur, lrecs, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

		for (i = rrecs - 1; i >= 0; i--) {
			error = xfs_btree_debug_check_ptr(cur, rpp, i, level);
			if (error)
				goto error0;
		}

		xfs_btree_shift_keys(cur, rkp, 1, rrecs);
		xfs_btree_shift_ptrs(cur, rpp, 1, rrecs);

		error = xfs_btree_debug_check_ptr(cur, lpp, 0, level);
		if (error)
			goto error0;

		 
		xfs_btree_copy_keys(cur, rkp, lkp, 1);
		xfs_btree_copy_ptrs(cur, rpp, lpp, 1);

		xfs_btree_log_keys(cur, rbp, 1, rrecs + 1);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs + 1);

		ASSERT(cur->bc_ops->keys_inorder(cur, rkp,
			xfs_btree_key_addr(cur, 2, right)));
	} else {
		 
		union xfs_btree_rec	*lrp;
		union xfs_btree_rec	*rrp;

		lrp = xfs_btree_rec_addr(cur, lrecs, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_shift_recs(cur, rrp, 1, rrecs);

		 
		xfs_btree_copy_recs(cur, rrp, lrp, 1);
		xfs_btree_log_recs(cur, rbp, 1, rrecs + 1);
	}

	 
	xfs_btree_set_numrecs(left, --lrecs);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS);

	xfs_btree_set_numrecs(right, ++rrecs);
	xfs_btree_log_block(cur, rbp, XFS_BB_NUMRECS);

	 
	error = xfs_btree_dup_cursor(cur, &tcur);
	if (error)
		goto error0;
	i = xfs_btree_lastrec(tcur, level);
	if (XFS_IS_CORRUPT(tcur->bc_mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error0;
	}

	error = xfs_btree_increment(tcur, level, &i);
	if (error)
		goto error1;

	 
	if (cur->bc_flags & XFS_BTREE_OVERLAPPING) {
		error = xfs_btree_update_keys(cur, level);
		if (error)
			goto error1;
	}

	 
	error = xfs_btree_update_keys(tcur, level);
	if (error)
		goto error1;

	xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);

	*stat = 1;
	return 0;

out0:
	*stat = 0;
	return 0;

error0:
	return error;

error1:
	xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int					 
__xfs_btree_split(
	struct xfs_btree_cur	*cur,
	int			level,
	union xfs_btree_ptr	*ptrp,
	union xfs_btree_key	*key,
	struct xfs_btree_cur	**curp,
	int			*stat)		 
{
	union xfs_btree_ptr	lptr;		 
	struct xfs_buf		*lbp;		 
	struct xfs_btree_block	*left;		 
	union xfs_btree_ptr	rptr;		 
	struct xfs_buf		*rbp;		 
	struct xfs_btree_block	*right;		 
	union xfs_btree_ptr	rrptr;		 
	struct xfs_buf		*rrbp;		 
	struct xfs_btree_block	*rrblock;	 
	int			lrecs;
	int			rrecs;
	int			src_index;
	int			error;		 
	int			i;

	XFS_BTREE_STATS_INC(cur, split);

	 
	left = xfs_btree_get_block(cur, level, &lbp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, left, level, lbp);
	if (error)
		goto error0;
#endif

	xfs_btree_buf_to_ptr(cur, lbp, &lptr);

	 
	error = cur->bc_ops->alloc_block(cur, &lptr, &rptr, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		goto out0;
	XFS_BTREE_STATS_INC(cur, alloc);

	 
	error = xfs_btree_get_buf_block(cur, &rptr, &right, &rbp);
	if (error)
		goto error0;

	 
	xfs_btree_init_block_cur(cur, rbp, xfs_btree_get_level(left), 0);

	 
	lrecs = xfs_btree_get_numrecs(left);
	rrecs = lrecs / 2;
	if ((lrecs & 1) && cur->bc_levels[level].ptr <= rrecs + 1)
		rrecs++;
	src_index = (lrecs - rrecs + 1);

	XFS_BTREE_STATS_ADD(cur, moves, rrecs);

	 
	lrecs -= rrecs;
	xfs_btree_set_numrecs(left, lrecs);
	xfs_btree_set_numrecs(right, xfs_btree_get_numrecs(right) + rrecs);

	 
	if (level > 0) {
		 
		union xfs_btree_key	*lkp;	 
		union xfs_btree_ptr	*lpp;	 
		union xfs_btree_key	*rkp;	 
		union xfs_btree_ptr	*rpp;	 

		lkp = xfs_btree_key_addr(cur, src_index, left);
		lpp = xfs_btree_ptr_addr(cur, src_index, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

		for (i = src_index; i < rrecs; i++) {
			error = xfs_btree_debug_check_ptr(cur, lpp, i, level);
			if (error)
				goto error0;
		}

		 
		xfs_btree_copy_keys(cur, rkp, lkp, rrecs);
		xfs_btree_copy_ptrs(cur, rpp, lpp, rrecs);

		xfs_btree_log_keys(cur, rbp, 1, rrecs);
		xfs_btree_log_ptrs(cur, rbp, 1, rrecs);

		 
		xfs_btree_get_node_keys(cur, right, key);
	} else {
		 
		union xfs_btree_rec	*lrp;	 
		union xfs_btree_rec	*rrp;	 

		lrp = xfs_btree_rec_addr(cur, src_index, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		 
		xfs_btree_copy_recs(cur, rrp, lrp, rrecs);
		xfs_btree_log_recs(cur, rbp, 1, rrecs);

		 
		xfs_btree_get_leaf_keys(cur, right, key);
	}

	 
	xfs_btree_get_sibling(cur, left, &rrptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, right, &rrptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
	xfs_btree_set_sibling(cur, left, &rptr, XFS_BB_RIGHTSIB);

	xfs_btree_log_block(cur, rbp, XFS_BB_ALL_BITS);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS | XFS_BB_RIGHTSIB);

	 
	if (!xfs_btree_ptr_is_null(cur, &rrptr)) {
		error = xfs_btree_read_buf_block(cur, &rrptr,
							0, &rrblock, &rrbp);
		if (error)
			goto error0;
		xfs_btree_set_sibling(cur, rrblock, &rptr, XFS_BB_LEFTSIB);
		xfs_btree_log_block(cur, rrbp, XFS_BB_LEFTSIB);
	}

	 
	if (cur->bc_flags & XFS_BTREE_OVERLAPPING) {
		error = xfs_btree_update_keys(cur, level);
		if (error)
			goto error0;
	}

	 
	if (cur->bc_levels[level].ptr > lrecs + 1) {
		xfs_btree_setbuf(cur, level, rbp);
		cur->bc_levels[level].ptr -= lrecs;
	}
	 
	if (level + 1 < cur->bc_nlevels) {
		error = xfs_btree_dup_cursor(cur, curp);
		if (error)
			goto error0;
		(*curp)->bc_levels[level + 1].ptr++;
	}
	*ptrp = rptr;
	*stat = 1;
	return 0;
out0:
	*stat = 0;
	return 0;

error0:
	return error;
}

#ifdef __KERNEL__
struct xfs_btree_split_args {
	struct xfs_btree_cur	*cur;
	int			level;
	union xfs_btree_ptr	*ptrp;
	union xfs_btree_key	*key;
	struct xfs_btree_cur	**curp;
	int			*stat;		 
	int			result;
	bool			kswapd;	 
	struct completion	*done;
	struct work_struct	work;
};

 
static void
xfs_btree_split_worker(
	struct work_struct	*work)
{
	struct xfs_btree_split_args	*args = container_of(work,
						struct xfs_btree_split_args, work);
	unsigned long		pflags;
	unsigned long		new_pflags = 0;

	 
	if (args->kswapd)
		new_pflags |= PF_MEMALLOC | PF_KSWAPD;

	current_set_flags_nested(&pflags, new_pflags);
	xfs_trans_set_context(args->cur->bc_tp);

	args->result = __xfs_btree_split(args->cur, args->level, args->ptrp,
					 args->key, args->curp, args->stat);

	xfs_trans_clear_context(args->cur->bc_tp);
	current_restore_flags_nested(&pflags, new_pflags);

	 
	complete(args->done);

}

 
STATIC int					 
xfs_btree_split(
	struct xfs_btree_cur	*cur,
	int			level,
	union xfs_btree_ptr	*ptrp,
	union xfs_btree_key	*key,
	struct xfs_btree_cur	**curp,
	int			*stat)		 
{
	struct xfs_btree_split_args	args;
	DECLARE_COMPLETION_ONSTACK(done);

	if (cur->bc_btnum != XFS_BTNUM_BMAP ||
	    cur->bc_tp->t_highest_agno == NULLAGNUMBER)
		return __xfs_btree_split(cur, level, ptrp, key, curp, stat);

	args.cur = cur;
	args.level = level;
	args.ptrp = ptrp;
	args.key = key;
	args.curp = curp;
	args.stat = stat;
	args.done = &done;
	args.kswapd = current_is_kswapd();
	INIT_WORK_ONSTACK(&args.work, xfs_btree_split_worker);
	queue_work(xfs_alloc_wq, &args.work);
	wait_for_completion(&done);
	destroy_work_on_stack(&args.work);
	return args.result;
}
#else
#define xfs_btree_split	__xfs_btree_split
#endif  


 
int						 
xfs_btree_new_iroot(
	struct xfs_btree_cur	*cur,		 
	int			*logflags,	 
	int			*stat)		 
{
	struct xfs_buf		*cbp;		 
	struct xfs_btree_block	*block;		 
	struct xfs_btree_block	*cblock;	 
	union xfs_btree_key	*ckp;		 
	union xfs_btree_ptr	*cpp;		 
	union xfs_btree_key	*kp;		 
	union xfs_btree_ptr	*pp;		 
	union xfs_btree_ptr	nptr;		 
	int			level;		 
	int			error;		 
	int			i;		 

	XFS_BTREE_STATS_INC(cur, newroot);

	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);

	level = cur->bc_nlevels - 1;

	block = xfs_btree_get_iroot(cur);
	pp = xfs_btree_ptr_addr(cur, 1, block);

	 
	error = cur->bc_ops->alloc_block(cur, pp, &nptr, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		return 0;

	XFS_BTREE_STATS_INC(cur, alloc);

	 
	error = xfs_btree_get_buf_block(cur, &nptr, &cblock, &cbp);
	if (error)
		goto error0;

	 
	memcpy(cblock, block, xfs_btree_block_len(cur));
	if (cur->bc_flags & XFS_BTREE_CRC_BLOCKS) {
		__be64 bno = cpu_to_be64(xfs_buf_daddr(cbp));
		if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
			cblock->bb_u.l.bb_blkno = bno;
		else
			cblock->bb_u.s.bb_blkno = bno;
	}

	be16_add_cpu(&block->bb_level, 1);
	xfs_btree_set_numrecs(block, 1);
	cur->bc_nlevels++;
	ASSERT(cur->bc_nlevels <= cur->bc_maxlevels);
	cur->bc_levels[level + 1].ptr = 1;

	kp = xfs_btree_key_addr(cur, 1, block);
	ckp = xfs_btree_key_addr(cur, 1, cblock);
	xfs_btree_copy_keys(cur, ckp, kp, xfs_btree_get_numrecs(cblock));

	cpp = xfs_btree_ptr_addr(cur, 1, cblock);
	for (i = 0; i < be16_to_cpu(cblock->bb_numrecs); i++) {
		error = xfs_btree_debug_check_ptr(cur, pp, i, level);
		if (error)
			goto error0;
	}

	xfs_btree_copy_ptrs(cur, cpp, pp, xfs_btree_get_numrecs(cblock));

	error = xfs_btree_debug_check_ptr(cur, &nptr, 0, level);
	if (error)
		goto error0;

	xfs_btree_copy_ptrs(cur, pp, &nptr, 1);

	xfs_iroot_realloc(cur->bc_ino.ip,
			  1 - xfs_btree_get_numrecs(cblock),
			  cur->bc_ino.whichfork);

	xfs_btree_setbuf(cur, level, cbp);

	 
	xfs_btree_log_block(cur, cbp, XFS_BB_ALL_BITS);
	xfs_btree_log_keys(cur, cbp, 1, be16_to_cpu(cblock->bb_numrecs));
	xfs_btree_log_ptrs(cur, cbp, 1, be16_to_cpu(cblock->bb_numrecs));

	*logflags |=
		XFS_ILOG_CORE | xfs_ilog_fbroot(cur->bc_ino.whichfork);
	*stat = 1;
	return 0;
error0:
	return error;
}

 
STATIC int				 
xfs_btree_new_root(
	struct xfs_btree_cur	*cur,	 
	int			*stat)	 
{
	struct xfs_btree_block	*block;	 
	struct xfs_buf		*bp;	 
	int			error;	 
	struct xfs_buf		*lbp;	 
	struct xfs_btree_block	*left;	 
	struct xfs_buf		*nbp;	 
	struct xfs_btree_block	*new;	 
	int			nptr;	 
	struct xfs_buf		*rbp;	 
	struct xfs_btree_block	*right;	 
	union xfs_btree_ptr	rptr;
	union xfs_btree_ptr	lptr;

	XFS_BTREE_STATS_INC(cur, newroot);

	 
	cur->bc_ops->init_ptr_from_cur(cur, &rptr);

	 
	error = cur->bc_ops->alloc_block(cur, &rptr, &lptr, stat);
	if (error)
		goto error0;
	if (*stat == 0)
		goto out0;
	XFS_BTREE_STATS_INC(cur, alloc);

	 
	error = xfs_btree_get_buf_block(cur, &lptr, &new, &nbp);
	if (error)
		goto error0;

	 
	cur->bc_ops->set_root(cur, &lptr, 1);

	 
	block = xfs_btree_get_block(cur, cur->bc_nlevels - 1, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, cur->bc_nlevels - 1, bp);
	if (error)
		goto error0;
#endif

	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &rptr)) {
		 
		lbp = bp;
		xfs_btree_buf_to_ptr(cur, lbp, &lptr);
		left = block;
		error = xfs_btree_read_buf_block(cur, &rptr, 0, &right, &rbp);
		if (error)
			goto error0;
		bp = rbp;
		nptr = 1;
	} else {
		 
		rbp = bp;
		xfs_btree_buf_to_ptr(cur, rbp, &rptr);
		right = block;
		xfs_btree_get_sibling(cur, right, &lptr, XFS_BB_LEFTSIB);
		error = xfs_btree_read_buf_block(cur, &lptr, 0, &left, &lbp);
		if (error)
			goto error0;
		bp = lbp;
		nptr = 2;
	}

	 
	xfs_btree_init_block_cur(cur, nbp, cur->bc_nlevels, 2);
	xfs_btree_log_block(cur, nbp, XFS_BB_ALL_BITS);
	ASSERT(!xfs_btree_ptr_is_null(cur, &lptr) &&
			!xfs_btree_ptr_is_null(cur, &rptr));

	 
	if (xfs_btree_get_level(left) > 0) {
		 
		xfs_btree_get_node_keys(cur, left,
				xfs_btree_key_addr(cur, 1, new));
		xfs_btree_get_node_keys(cur, right,
				xfs_btree_key_addr(cur, 2, new));
	} else {
		 
		xfs_btree_get_leaf_keys(cur, left,
			xfs_btree_key_addr(cur, 1, new));
		xfs_btree_get_leaf_keys(cur, right,
			xfs_btree_key_addr(cur, 2, new));
	}
	xfs_btree_log_keys(cur, nbp, 1, 2);

	 
	xfs_btree_copy_ptrs(cur,
		xfs_btree_ptr_addr(cur, 1, new), &lptr, 1);
	xfs_btree_copy_ptrs(cur,
		xfs_btree_ptr_addr(cur, 2, new), &rptr, 1);
	xfs_btree_log_ptrs(cur, nbp, 1, 2);

	 
	xfs_btree_setbuf(cur, cur->bc_nlevels, nbp);
	cur->bc_levels[cur->bc_nlevels].ptr = nptr;
	cur->bc_nlevels++;
	ASSERT(cur->bc_nlevels <= cur->bc_maxlevels);
	*stat = 1;
	return 0;
error0:
	return error;
out0:
	*stat = 0;
	return 0;
}

STATIC int
xfs_btree_make_block_unfull(
	struct xfs_btree_cur	*cur,	 
	int			level,	 
	int			numrecs, 
	int			*oindex, 
	int			*index,	 
	union xfs_btree_ptr	*nptr,	 
	struct xfs_btree_cur	**ncur,	 
	union xfs_btree_key	*key,	 
	int			*stat)
{
	int			error = 0;

	if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    level == cur->bc_nlevels - 1) {
		struct xfs_inode *ip = cur->bc_ino.ip;

		if (numrecs < cur->bc_ops->get_dmaxrecs(cur, level)) {
			 
			xfs_iroot_realloc(ip, 1, cur->bc_ino.whichfork);
			*stat = 1;
		} else {
			 
			int	logflags = 0;

			error = xfs_btree_new_iroot(cur, &logflags, stat);
			if (error || *stat == 0)
				return error;

			xfs_trans_log_inode(cur->bc_tp, ip, logflags);
		}

		return 0;
	}

	 
	error = xfs_btree_rshift(cur, level, stat);
	if (error || *stat)
		return error;

	 
	error = xfs_btree_lshift(cur, level, stat);
	if (error)
		return error;

	if (*stat) {
		*oindex = *index = cur->bc_levels[level].ptr;
		return 0;
	}

	 
	error = xfs_btree_split(cur, level, nptr, key, ncur, stat);
	if (error || *stat == 0)
		return error;


	*index = cur->bc_levels[level].ptr;
	return 0;
}

 
STATIC int
xfs_btree_insrec(
	struct xfs_btree_cur	*cur,	 
	int			level,	 
	union xfs_btree_ptr	*ptrp,	 
	union xfs_btree_rec	*rec,	 
	union xfs_btree_key	*key,	 
	struct xfs_btree_cur	**curp,	 
	int			*stat)	 
{
	struct xfs_btree_block	*block;	 
	struct xfs_buf		*bp;	 
	union xfs_btree_ptr	nptr;	 
	struct xfs_btree_cur	*ncur = NULL;	 
	union xfs_btree_key	nkey;	 
	union xfs_btree_key	*lkey;
	int			optr;	 
	int			ptr;	 
	int			numrecs; 
	int			error;	 
	int			i;
	xfs_daddr_t		old_bn;

	ncur = NULL;
	lkey = &nkey;

	 
	if (!(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) &&
	    (level >= cur->bc_nlevels)) {
		error = xfs_btree_new_root(cur, stat);
		xfs_btree_set_ptr_null(cur, ptrp);

		return error;
	}

	 
	ptr = cur->bc_levels[level].ptr;
	if (ptr == 0) {
		*stat = 0;
		return 0;
	}

	optr = ptr;

	XFS_BTREE_STATS_INC(cur, insrec);

	 
	block = xfs_btree_get_block(cur, level, &bp);
	old_bn = bp ? xfs_buf_daddr(bp) : XFS_BUF_DADDR_NULL;
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;

	 
	if (ptr <= numrecs) {
		if (level == 0) {
			ASSERT(cur->bc_ops->recs_inorder(cur, rec,
				xfs_btree_rec_addr(cur, ptr, block)));
		} else {
			ASSERT(cur->bc_ops->keys_inorder(cur, key,
				xfs_btree_key_addr(cur, ptr, block)));
		}
	}
#endif

	 
	xfs_btree_set_ptr_null(cur, &nptr);
	if (numrecs == cur->bc_ops->get_maxrecs(cur, level)) {
		error = xfs_btree_make_block_unfull(cur, level, numrecs,
					&optr, &ptr, &nptr, &ncur, lkey, stat);
		if (error || *stat == 0)
			goto error0;
	}

	 
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	 
	XFS_BTREE_STATS_ADD(cur, moves, numrecs - ptr + 1);

	if (level > 0) {
		 
		union xfs_btree_key	*kp;
		union xfs_btree_ptr	*pp;

		kp = xfs_btree_key_addr(cur, ptr, block);
		pp = xfs_btree_ptr_addr(cur, ptr, block);

		for (i = numrecs - ptr; i >= 0; i--) {
			error = xfs_btree_debug_check_ptr(cur, pp, i, level);
			if (error)
				goto error0;
		}

		xfs_btree_shift_keys(cur, kp, 1, numrecs - ptr + 1);
		xfs_btree_shift_ptrs(cur, pp, 1, numrecs - ptr + 1);

		error = xfs_btree_debug_check_ptr(cur, ptrp, 0, level);
		if (error)
			goto error0;

		 
		xfs_btree_copy_keys(cur, kp, key, 1);
		xfs_btree_copy_ptrs(cur, pp, ptrp, 1);
		numrecs++;
		xfs_btree_set_numrecs(block, numrecs);
		xfs_btree_log_ptrs(cur, bp, ptr, numrecs);
		xfs_btree_log_keys(cur, bp, ptr, numrecs);
#ifdef DEBUG
		if (ptr < numrecs) {
			ASSERT(cur->bc_ops->keys_inorder(cur, kp,
				xfs_btree_key_addr(cur, ptr + 1, block)));
		}
#endif
	} else {
		 
		union xfs_btree_rec             *rp;

		rp = xfs_btree_rec_addr(cur, ptr, block);

		xfs_btree_shift_recs(cur, rp, 1, numrecs - ptr + 1);

		 
		xfs_btree_copy_recs(cur, rp, rec, 1);
		xfs_btree_set_numrecs(block, ++numrecs);
		xfs_btree_log_recs(cur, bp, ptr, numrecs);
#ifdef DEBUG
		if (ptr < numrecs) {
			ASSERT(cur->bc_ops->recs_inorder(cur, rp,
				xfs_btree_rec_addr(cur, ptr + 1, block)));
		}
#endif
	}

	 
	xfs_btree_log_block(cur, bp, XFS_BB_NUMRECS);

	 
	if (bp && xfs_buf_daddr(bp) != old_bn) {
		xfs_btree_get_keys(cur, block, lkey);
	} else if (xfs_btree_needs_key_update(cur, optr)) {
		error = xfs_btree_update_keys(cur, level);
		if (error)
			goto error0;
	}

	 
	if (xfs_btree_is_lastrec(cur, block, level)) {
		cur->bc_ops->update_lastrec(cur, block, rec,
					    ptr, LASTREC_INSREC);
	}

	 
	*ptrp = nptr;
	if (!xfs_btree_ptr_is_null(cur, &nptr)) {
		xfs_btree_copy_keys(cur, key, lkey, 1);
		*curp = ncur;
	}

	*stat = 1;
	return 0;

error0:
	if (ncur)
		xfs_btree_del_cursor(ncur, error);
	return error;
}

 
int
xfs_btree_insert(
	struct xfs_btree_cur	*cur,
	int			*stat)
{
	int			error;	 
	int			i;	 
	int			level;	 
	union xfs_btree_ptr	nptr;	 
	struct xfs_btree_cur	*ncur;	 
	struct xfs_btree_cur	*pcur;	 
	union xfs_btree_key	bkey;	 
	union xfs_btree_key	*key;
	union xfs_btree_rec	rec;	 

	level = 0;
	ncur = NULL;
	pcur = cur;
	key = &bkey;

	xfs_btree_set_ptr_null(cur, &nptr);

	 
	cur->bc_ops->init_rec_from_cur(cur, &rec);
	cur->bc_ops->init_key_from_rec(key, &rec);

	 
	do {
		 
		error = xfs_btree_insrec(pcur, level, &nptr, &rec, key,
				&ncur, &i);
		if (error) {
			if (pcur != cur)
				xfs_btree_del_cursor(pcur, XFS_BTREE_ERROR);
			goto error0;
		}

		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		level++;

		 
		if (pcur != cur &&
		    (ncur || xfs_btree_ptr_is_null(cur, &nptr))) {
			 
			if (cur->bc_ops->update_cursor)
				cur->bc_ops->update_cursor(pcur, cur);
			cur->bc_nlevels = pcur->bc_nlevels;
			xfs_btree_del_cursor(pcur, XFS_BTREE_NOERROR);
		}
		 
		if (ncur) {
			pcur = ncur;
			ncur = NULL;
		}
	} while (!xfs_btree_ptr_is_null(cur, &nptr));

	*stat = i;
	return 0;
error0:
	return error;
}

 
STATIC int
xfs_btree_kill_iroot(
	struct xfs_btree_cur	*cur)
{
	int			whichfork = cur->bc_ino.whichfork;
	struct xfs_inode	*ip = cur->bc_ino.ip;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_block	*block;
	struct xfs_btree_block	*cblock;
	union xfs_btree_key	*kp;
	union xfs_btree_key	*ckp;
	union xfs_btree_ptr	*pp;
	union xfs_btree_ptr	*cpp;
	struct xfs_buf		*cbp;
	int			level;
	int			index;
	int			numrecs;
	int			error;
#ifdef DEBUG
	union xfs_btree_ptr	ptr;
#endif
	int			i;

	ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);
	ASSERT(cur->bc_nlevels > 1);

	 
	level = cur->bc_nlevels - 1;
	if (level == 1)
		goto out0;

	 
	block = xfs_btree_get_iroot(cur);
	if (xfs_btree_get_numrecs(block) != 1)
		goto out0;

	cblock = xfs_btree_get_block(cur, level - 1, &cbp);
	numrecs = xfs_btree_get_numrecs(cblock);

	 
	if (numrecs > cur->bc_ops->get_dmaxrecs(cur, level))
		goto out0;

	XFS_BTREE_STATS_INC(cur, killroot);

#ifdef DEBUG
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_LEFTSIB);
	ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
	xfs_btree_get_sibling(cur, block, &ptr, XFS_BB_RIGHTSIB);
	ASSERT(xfs_btree_ptr_is_null(cur, &ptr));
#endif

	index = numrecs - cur->bc_ops->get_maxrecs(cur, level);
	if (index) {
		xfs_iroot_realloc(cur->bc_ino.ip, index,
				  cur->bc_ino.whichfork);
		block = ifp->if_broot;
	}

	be16_add_cpu(&block->bb_numrecs, index);
	ASSERT(block->bb_numrecs == cblock->bb_numrecs);

	kp = xfs_btree_key_addr(cur, 1, block);
	ckp = xfs_btree_key_addr(cur, 1, cblock);
	xfs_btree_copy_keys(cur, kp, ckp, numrecs);

	pp = xfs_btree_ptr_addr(cur, 1, block);
	cpp = xfs_btree_ptr_addr(cur, 1, cblock);

	for (i = 0; i < numrecs; i++) {
		error = xfs_btree_debug_check_ptr(cur, cpp, i, level - 1);
		if (error)
			return error;
	}

	xfs_btree_copy_ptrs(cur, pp, cpp, numrecs);

	error = xfs_btree_free_block(cur, cbp);
	if (error)
		return error;

	cur->bc_levels[level - 1].bp = NULL;
	be16_add_cpu(&block->bb_level, -1);
	xfs_trans_log_inode(cur->bc_tp, ip,
		XFS_ILOG_CORE | xfs_ilog_fbroot(cur->bc_ino.whichfork));
	cur->bc_nlevels--;
out0:
	return 0;
}

 
STATIC int
xfs_btree_kill_root(
	struct xfs_btree_cur	*cur,
	struct xfs_buf		*bp,
	int			level,
	union xfs_btree_ptr	*newroot)
{
	int			error;

	XFS_BTREE_STATS_INC(cur, killroot);

	 
	cur->bc_ops->set_root(cur, newroot, -1);

	error = xfs_btree_free_block(cur, bp);
	if (error)
		return error;

	cur->bc_levels[level].bp = NULL;
	cur->bc_levels[level].ra = 0;
	cur->bc_nlevels--;

	return 0;
}

STATIC int
xfs_btree_dec_cursor(
	struct xfs_btree_cur	*cur,
	int			level,
	int			*stat)
{
	int			error;
	int			i;

	if (level > 0) {
		error = xfs_btree_decrement(cur, level, &i);
		if (error)
			return error;
	}

	*stat = 1;
	return 0;
}

 
STATIC int					 
xfs_btree_delrec(
	struct xfs_btree_cur	*cur,		 
	int			level,		 
	int			*stat)		 
{
	struct xfs_btree_block	*block;		 
	union xfs_btree_ptr	cptr;		 
	struct xfs_buf		*bp;		 
	int			error;		 
	int			i;		 
	union xfs_btree_ptr	lptr;		 
	struct xfs_buf		*lbp;		 
	struct xfs_btree_block	*left;		 
	int			lrecs = 0;	 
	int			ptr;		 
	union xfs_btree_ptr	rptr;		 
	struct xfs_buf		*rbp;		 
	struct xfs_btree_block	*right;		 
	struct xfs_btree_block	*rrblock;	 
	struct xfs_buf		*rrbp;		 
	int			rrecs = 0;	 
	struct xfs_btree_cur	*tcur;		 
	int			numrecs;	 

	tcur = NULL;

	 
	ptr = cur->bc_levels[level].ptr;
	if (ptr == 0) {
		*stat = 0;
		return 0;
	}

	 
	block = xfs_btree_get_block(cur, level, &bp);
	numrecs = xfs_btree_get_numrecs(block);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto error0;
#endif

	 
	if (ptr > numrecs) {
		*stat = 0;
		return 0;
	}

	XFS_BTREE_STATS_INC(cur, delrec);
	XFS_BTREE_STATS_ADD(cur, moves, numrecs - ptr);

	 
	if (level > 0) {
		 
		union xfs_btree_key	*lkp;
		union xfs_btree_ptr	*lpp;

		lkp = xfs_btree_key_addr(cur, ptr + 1, block);
		lpp = xfs_btree_ptr_addr(cur, ptr + 1, block);

		for (i = 0; i < numrecs - ptr; i++) {
			error = xfs_btree_debug_check_ptr(cur, lpp, i, level);
			if (error)
				goto error0;
		}

		if (ptr < numrecs) {
			xfs_btree_shift_keys(cur, lkp, -1, numrecs - ptr);
			xfs_btree_shift_ptrs(cur, lpp, -1, numrecs - ptr);
			xfs_btree_log_keys(cur, bp, ptr, numrecs - 1);
			xfs_btree_log_ptrs(cur, bp, ptr, numrecs - 1);
		}
	} else {
		 
		if (ptr < numrecs) {
			xfs_btree_shift_recs(cur,
				xfs_btree_rec_addr(cur, ptr + 1, block),
				-1, numrecs - ptr);
			xfs_btree_log_recs(cur, bp, ptr, numrecs - 1);
		}
	}

	 
	xfs_btree_set_numrecs(block, --numrecs);
	xfs_btree_log_block(cur, bp, XFS_BB_NUMRECS);

	 
	if (xfs_btree_is_lastrec(cur, block, level)) {
		cur->bc_ops->update_lastrec(cur, block, NULL,
					    ptr, LASTREC_DELREC);
	}

	 
	if (level == cur->bc_nlevels - 1) {
		if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
			xfs_iroot_realloc(cur->bc_ino.ip, -1,
					  cur->bc_ino.whichfork);

			error = xfs_btree_kill_iroot(cur);
			if (error)
				goto error0;

			error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
			*stat = 1;
			return 0;
		}

		 
		if (numrecs == 1 && level > 0) {
			union xfs_btree_ptr	*pp;
			 
			pp = xfs_btree_ptr_addr(cur, 1, block);
			error = xfs_btree_kill_root(cur, bp, level, pp);
			if (error)
				goto error0;
		} else if (level > 0) {
			error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
		}
		*stat = 1;
		return 0;
	}

	 
	if (xfs_btree_needs_key_update(cur, ptr)) {
		error = xfs_btree_update_keys(cur, level);
		if (error)
			goto error0;
	}

	 
	if (numrecs >= cur->bc_ops->get_minrecs(cur, level)) {
		error = xfs_btree_dec_cursor(cur, level, stat);
		if (error)
			goto error0;
		return 0;
	}

	 
	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	xfs_btree_get_sibling(cur, block, &lptr, XFS_BB_LEFTSIB);

	if (cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) {
		 
		if (xfs_btree_ptr_is_null(cur, &rptr) &&
		    xfs_btree_ptr_is_null(cur, &lptr) &&
		    level == cur->bc_nlevels - 2) {
			error = xfs_btree_kill_iroot(cur);
			if (!error)
				error = xfs_btree_dec_cursor(cur, level, stat);
			if (error)
				goto error0;
			return 0;
		}
	}

	ASSERT(!xfs_btree_ptr_is_null(cur, &rptr) ||
	       !xfs_btree_ptr_is_null(cur, &lptr));

	 
	error = xfs_btree_dup_cursor(cur, &tcur);
	if (error)
		goto error0;

	 
	if (!xfs_btree_ptr_is_null(cur, &rptr)) {
		 
		i = xfs_btree_lastrec(tcur, level);
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		error = xfs_btree_increment(tcur, level, &i);
		if (error)
			goto error0;
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		i = xfs_btree_lastrec(tcur, level);
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		 
		right = xfs_btree_get_block(tcur, level, &rbp);
#ifdef DEBUG
		error = xfs_btree_check_block(tcur, right, level, rbp);
		if (error)
			goto error0;
#endif
		 
		xfs_btree_get_sibling(tcur, right, &cptr, XFS_BB_LEFTSIB);

		 
		if (xfs_btree_get_numrecs(right) - 1 >=
		    cur->bc_ops->get_minrecs(tcur, level)) {
			error = xfs_btree_lshift(tcur, level, &i);
			if (error)
				goto error0;
			if (i) {
				ASSERT(xfs_btree_get_numrecs(block) >=
				       cur->bc_ops->get_minrecs(tcur, level));

				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				tcur = NULL;

				error = xfs_btree_dec_cursor(cur, level, stat);
				if (error)
					goto error0;
				return 0;
			}
		}

		 
		rrecs = xfs_btree_get_numrecs(right);
		if (!xfs_btree_ptr_is_null(cur, &lptr)) {
			i = xfs_btree_firstrec(tcur, level);
			if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto error0;
			}

			error = xfs_btree_decrement(tcur, level, &i);
			if (error)
				goto error0;
			if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto error0;
			}
		}
	}

	 
	if (!xfs_btree_ptr_is_null(cur, &lptr)) {
		 
		i = xfs_btree_firstrec(tcur, level);
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		error = xfs_btree_decrement(tcur, level, &i);
		if (error)
			goto error0;
		i = xfs_btree_firstrec(tcur, level);
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		 
		left = xfs_btree_get_block(tcur, level, &lbp);
#ifdef DEBUG
		error = xfs_btree_check_block(cur, left, level, lbp);
		if (error)
			goto error0;
#endif
		 
		xfs_btree_get_sibling(tcur, left, &cptr, XFS_BB_RIGHTSIB);

		 
		if (xfs_btree_get_numrecs(left) - 1 >=
		    cur->bc_ops->get_minrecs(tcur, level)) {
			error = xfs_btree_rshift(tcur, level, &i);
			if (error)
				goto error0;
			if (i) {
				ASSERT(xfs_btree_get_numrecs(block) >=
				       cur->bc_ops->get_minrecs(tcur, level));
				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
				tcur = NULL;
				if (level == 0)
					cur->bc_levels[0].ptr++;

				*stat = 1;
				return 0;
			}
		}

		 
		lrecs = xfs_btree_get_numrecs(left);
	}

	 
	xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
	tcur = NULL;

	 
	ASSERT(!xfs_btree_ptr_is_null(cur, &cptr));

	if (!xfs_btree_ptr_is_null(cur, &lptr) &&
	    lrecs + xfs_btree_get_numrecs(block) <=
			cur->bc_ops->get_maxrecs(cur, level)) {
		 
		rptr = cptr;
		right = block;
		rbp = bp;
		error = xfs_btree_read_buf_block(cur, &lptr, 0, &left, &lbp);
		if (error)
			goto error0;

	 
	} else if (!xfs_btree_ptr_is_null(cur, &rptr) &&
		   rrecs + xfs_btree_get_numrecs(block) <=
			cur->bc_ops->get_maxrecs(cur, level)) {
		 
		lptr = cptr;
		left = block;
		lbp = bp;
		error = xfs_btree_read_buf_block(cur, &rptr, 0, &right, &rbp);
		if (error)
			goto error0;

	 
	} else {
		error = xfs_btree_dec_cursor(cur, level, stat);
		if (error)
			goto error0;
		return 0;
	}

	rrecs = xfs_btree_get_numrecs(right);
	lrecs = xfs_btree_get_numrecs(left);

	 
	XFS_BTREE_STATS_ADD(cur, moves, rrecs);
	if (level > 0) {
		 
		union xfs_btree_key	*lkp;	 
		union xfs_btree_ptr	*lpp;	 
		union xfs_btree_key	*rkp;	 
		union xfs_btree_ptr	*rpp;	 

		lkp = xfs_btree_key_addr(cur, lrecs + 1, left);
		lpp = xfs_btree_ptr_addr(cur, lrecs + 1, left);
		rkp = xfs_btree_key_addr(cur, 1, right);
		rpp = xfs_btree_ptr_addr(cur, 1, right);

		for (i = 1; i < rrecs; i++) {
			error = xfs_btree_debug_check_ptr(cur, rpp, i, level);
			if (error)
				goto error0;
		}

		xfs_btree_copy_keys(cur, lkp, rkp, rrecs);
		xfs_btree_copy_ptrs(cur, lpp, rpp, rrecs);

		xfs_btree_log_keys(cur, lbp, lrecs + 1, lrecs + rrecs);
		xfs_btree_log_ptrs(cur, lbp, lrecs + 1, lrecs + rrecs);
	} else {
		 
		union xfs_btree_rec	*lrp;	 
		union xfs_btree_rec	*rrp;	 

		lrp = xfs_btree_rec_addr(cur, lrecs + 1, left);
		rrp = xfs_btree_rec_addr(cur, 1, right);

		xfs_btree_copy_recs(cur, lrp, rrp, rrecs);
		xfs_btree_log_recs(cur, lbp, lrecs + 1, lrecs + rrecs);
	}

	XFS_BTREE_STATS_INC(cur, join);

	 
	xfs_btree_set_numrecs(left, lrecs + rrecs);
	xfs_btree_get_sibling(cur, right, &cptr, XFS_BB_RIGHTSIB);
	xfs_btree_set_sibling(cur, left, &cptr, XFS_BB_RIGHTSIB);
	xfs_btree_log_block(cur, lbp, XFS_BB_NUMRECS | XFS_BB_RIGHTSIB);

	 
	xfs_btree_get_sibling(cur, left, &cptr, XFS_BB_RIGHTSIB);
	if (!xfs_btree_ptr_is_null(cur, &cptr)) {
		error = xfs_btree_read_buf_block(cur, &cptr, 0, &rrblock, &rrbp);
		if (error)
			goto error0;
		xfs_btree_set_sibling(cur, rrblock, &lptr, XFS_BB_LEFTSIB);
		xfs_btree_log_block(cur, rrbp, XFS_BB_LEFTSIB);
	}

	 
	error = xfs_btree_free_block(cur, rbp);
	if (error)
		goto error0;

	 
	if (bp != lbp) {
		cur->bc_levels[level].bp = lbp;
		cur->bc_levels[level].ptr += lrecs;
		cur->bc_levels[level].ra = 0;
	}
	 
	else if ((cur->bc_flags & XFS_BTREE_ROOT_IN_INODE) ||
		   (level + 1 < cur->bc_nlevels)) {
		error = xfs_btree_increment(cur, level + 1, &i);
		if (error)
			goto error0;
	}

	 
	if (level > 0)
		cur->bc_levels[level].ptr--;

	 

	 
	*stat = 2;
	return 0;

error0:
	if (tcur)
		xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
	return error;
}

 
int					 
xfs_btree_delete(
	struct xfs_btree_cur	*cur,
	int			*stat)	 
{
	int			error;	 
	int			level;
	int			i;
	bool			joined = false;

	 
	for (level = 0, i = 2; i == 2; level++) {
		error = xfs_btree_delrec(cur, level, &i);
		if (error)
			goto error0;
		if (i == 2)
			joined = true;
	}

	 
	if (joined && (cur->bc_flags & XFS_BTREE_OVERLAPPING)) {
		error = xfs_btree_updkeys_force(cur, 0);
		if (error)
			goto error0;
	}

	if (i == 0) {
		for (level = 1; level < cur->bc_nlevels; level++) {
			if (cur->bc_levels[level].ptr == 0) {
				error = xfs_btree_decrement(cur, level, &i);
				if (error)
					goto error0;
				break;
			}
		}
	}

	*stat = i;
	return 0;
error0:
	return error;
}

 
int					 
xfs_btree_get_rec(
	struct xfs_btree_cur	*cur,	 
	union xfs_btree_rec	**recp,	 
	int			*stat)	 
{
	struct xfs_btree_block	*block;	 
	struct xfs_buf		*bp;	 
	int			ptr;	 
#ifdef DEBUG
	int			error;	 
#endif

	ptr = cur->bc_levels[0].ptr;
	block = xfs_btree_get_block(cur, 0, &bp);

#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, 0, bp);
	if (error)
		return error;
#endif

	 
	if (ptr > xfs_btree_get_numrecs(block) || ptr <= 0) {
		*stat = 0;
		return 0;
	}

	 
	*recp = xfs_btree_rec_addr(cur, ptr, block);
	*stat = 1;
	return 0;
}

 
STATIC int
xfs_btree_visit_block(
	struct xfs_btree_cur		*cur,
	int				level,
	xfs_btree_visit_blocks_fn	fn,
	void				*data)
{
	struct xfs_btree_block		*block;
	struct xfs_buf			*bp;
	union xfs_btree_ptr		rptr;
	int				error;

	 
	xfs_btree_readahead(cur, level, XFS_BTCUR_RIGHTRA);
	block = xfs_btree_get_block(cur, level, &bp);

	 
	error = fn(cur, level, data);
	if (error)
		return error;

	 
	xfs_btree_get_sibling(cur, block, &rptr, XFS_BB_RIGHTSIB);
	if (xfs_btree_ptr_is_null(cur, &rptr))
		return -ENOENT;

	 
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (be64_to_cpu(rptr.l) == XFS_DADDR_TO_FSB(cur->bc_mp,
							xfs_buf_daddr(bp)))
			return -EFSCORRUPTED;
	} else {
		if (be32_to_cpu(rptr.s) == xfs_daddr_to_agbno(cur->bc_mp,
							xfs_buf_daddr(bp)))
			return -EFSCORRUPTED;
	}
	return xfs_btree_lookup_get_block(cur, level, &rptr, &block);
}


 
int
xfs_btree_visit_blocks(
	struct xfs_btree_cur		*cur,
	xfs_btree_visit_blocks_fn	fn,
	unsigned int			flags,
	void				*data)
{
	union xfs_btree_ptr		lptr;
	int				level;
	struct xfs_btree_block		*block = NULL;
	int				error = 0;

	cur->bc_ops->init_ptr_from_cur(cur, &lptr);

	 
	for (level = cur->bc_nlevels - 1; level >= 0; level--) {
		 
		error = xfs_btree_lookup_get_block(cur, level, &lptr, &block);
		if (error)
			return error;

		 
		if (level > 0) {
			union xfs_btree_ptr     *ptr;

			ptr = xfs_btree_ptr_addr(cur, 1, block);
			xfs_btree_readahead_ptr(cur, ptr, 1);

			 
			xfs_btree_copy_ptrs(cur, &lptr, ptr, 1);

			if (!(flags & XFS_BTREE_VISIT_LEAVES))
				continue;
		} else if (!(flags & XFS_BTREE_VISIT_RECORDS)) {
			continue;
		}

		 
		do {
			error = xfs_btree_visit_block(cur, level, fn, data);
		} while (!error);

		if (error != -ENOENT)
			return error;
	}

	return 0;
}

 
struct xfs_btree_block_change_owner_info {
	uint64_t		new_owner;
	struct list_head	*buffer_list;
};

static int
xfs_btree_block_change_owner(
	struct xfs_btree_cur	*cur,
	int			level,
	void			*data)
{
	struct xfs_btree_block_change_owner_info	*bbcoi = data;
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;

	 
	block = xfs_btree_get_block(cur, level, &bp);
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS) {
		if (block->bb_u.l.bb_owner == cpu_to_be64(bbcoi->new_owner))
			return 0;
		block->bb_u.l.bb_owner = cpu_to_be64(bbcoi->new_owner);
	} else {
		if (block->bb_u.s.bb_owner == cpu_to_be32(bbcoi->new_owner))
			return 0;
		block->bb_u.s.bb_owner = cpu_to_be32(bbcoi->new_owner);
	}

	 
	if (!bp) {
		ASSERT(cur->bc_flags & XFS_BTREE_ROOT_IN_INODE);
		ASSERT(level == cur->bc_nlevels - 1);
		return 0;
	}

	if (cur->bc_tp) {
		if (!xfs_trans_ordered_buf(cur->bc_tp, bp)) {
			xfs_btree_log_block(cur, bp, XFS_BB_OWNER);
			return -EAGAIN;
		}
	} else {
		xfs_buf_delwri_queue(bp, bbcoi->buffer_list);
	}

	return 0;
}

int
xfs_btree_change_owner(
	struct xfs_btree_cur	*cur,
	uint64_t		new_owner,
	struct list_head	*buffer_list)
{
	struct xfs_btree_block_change_owner_info	bbcoi;

	bbcoi.new_owner = new_owner;
	bbcoi.buffer_list = buffer_list;

	return xfs_btree_visit_blocks(cur, xfs_btree_block_change_owner,
			XFS_BTREE_VISIT_ALL, &bbcoi);
}

 
xfs_failaddr_t
xfs_btree_lblock_v5hdr_verify(
	struct xfs_buf		*bp,
	uint64_t		owner)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);

	if (!xfs_has_crc(mp))
		return __this_address;
	if (!uuid_equal(&block->bb_u.l.bb_uuid, &mp->m_sb.sb_meta_uuid))
		return __this_address;
	if (block->bb_u.l.bb_blkno != cpu_to_be64(xfs_buf_daddr(bp)))
		return __this_address;
	if (owner != XFS_RMAP_OWN_UNKNOWN &&
	    be64_to_cpu(block->bb_u.l.bb_owner) != owner)
		return __this_address;
	return NULL;
}

 
xfs_failaddr_t
xfs_btree_lblock_verify(
	struct xfs_buf		*bp,
	unsigned int		max_recs)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	xfs_fsblock_t		fsb;
	xfs_failaddr_t		fa;

	 
	if (be16_to_cpu(block->bb_numrecs) > max_recs)
		return __this_address;

	 
	fsb = XFS_DADDR_TO_FSB(mp, xfs_buf_daddr(bp));
	fa = xfs_btree_check_lblock_siblings(mp, NULL, -1, fsb,
			block->bb_u.l.bb_leftsib);
	if (!fa)
		fa = xfs_btree_check_lblock_siblings(mp, NULL, -1, fsb,
				block->bb_u.l.bb_rightsib);
	return fa;
}

 
xfs_failaddr_t
xfs_btree_sblock_v5hdr_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	struct xfs_perag	*pag = bp->b_pag;

	if (!xfs_has_crc(mp))
		return __this_address;
	if (!uuid_equal(&block->bb_u.s.bb_uuid, &mp->m_sb.sb_meta_uuid))
		return __this_address;
	if (block->bb_u.s.bb_blkno != cpu_to_be64(xfs_buf_daddr(bp)))
		return __this_address;
	if (pag && be32_to_cpu(block->bb_u.s.bb_owner) != pag->pag_agno)
		return __this_address;
	return NULL;
}

 
xfs_failaddr_t
xfs_btree_sblock_verify(
	struct xfs_buf		*bp,
	unsigned int		max_recs)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_btree_block	*block = XFS_BUF_TO_BLOCK(bp);
	xfs_agblock_t		agbno;
	xfs_failaddr_t		fa;

	 
	if (be16_to_cpu(block->bb_numrecs) > max_recs)
		return __this_address;

	 
	agbno = xfs_daddr_to_agbno(mp, xfs_buf_daddr(bp));
	fa = xfs_btree_check_sblock_siblings(bp->b_pag, NULL, -1, agbno,
			block->bb_u.s.bb_leftsib);
	if (!fa)
		fa = xfs_btree_check_sblock_siblings(bp->b_pag, NULL, -1, agbno,
				block->bb_u.s.bb_rightsib);
	return fa;
}

 
unsigned int
xfs_btree_compute_maxlevels(
	const unsigned int	*limits,
	unsigned long long	records)
{
	unsigned long long	level_blocks = howmany_64(records, limits[0]);
	unsigned int		height = 1;

	while (level_blocks > 1) {
		level_blocks = howmany_64(level_blocks, limits[1]);
		height++;
	}

	return height;
}

 
unsigned long long
xfs_btree_calc_size(
	const unsigned int	*limits,
	unsigned long long	records)
{
	unsigned long long	level_blocks = howmany_64(records, limits[0]);
	unsigned long long	blocks = level_blocks;

	while (level_blocks > 1) {
		level_blocks = howmany_64(level_blocks, limits[1]);
		blocks += level_blocks;
	}

	return blocks;
}

 
unsigned int
xfs_btree_space_to_height(
	const unsigned int	*limits,
	unsigned long long	leaf_blocks)
{
	 
	unsigned long long	node_blocks = 2;
	unsigned long long	blocks_left = leaf_blocks - 1;
	unsigned int		height = 1;

	if (leaf_blocks < 1)
		return 0;

	while (node_blocks < blocks_left) {
		blocks_left -= node_blocks;
		node_blocks *= limits[1];
		height++;
	}

	return height;
}

 
STATIC int
xfs_btree_simple_query_range(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_key	*low_key,
	const union xfs_btree_key	*high_key,
	xfs_btree_query_range_fn	fn,
	void				*priv)
{
	union xfs_btree_rec		*recp;
	union xfs_btree_key		rec_key;
	int				stat;
	bool				firstrec = true;
	int				error;

	ASSERT(cur->bc_ops->init_high_key_from_rec);
	ASSERT(cur->bc_ops->diff_two_keys);

	 
	stat = 0;
	error = xfs_btree_lookup(cur, XFS_LOOKUP_LE, &stat);
	if (error)
		goto out;

	 
	if (!stat) {
		error = xfs_btree_increment(cur, 0, &stat);
		if (error)
			goto out;
	}

	while (stat) {
		 
		error = xfs_btree_get_rec(cur, &recp, &stat);
		if (error || !stat)
			break;

		 
		if (firstrec) {
			cur->bc_ops->init_high_key_from_rec(&rec_key, recp);
			firstrec = false;
			if (xfs_btree_keycmp_gt(cur, low_key, &rec_key))
				goto advloop;
		}

		 
		cur->bc_ops->init_key_from_rec(&rec_key, recp);
		if (xfs_btree_keycmp_gt(cur, &rec_key, high_key))
			break;

		 
		error = fn(cur, recp, priv);
		if (error)
			break;

advloop:
		 
		error = xfs_btree_increment(cur, 0, &stat);
		if (error)
			break;
	}

out:
	return error;
}

 
STATIC int
xfs_btree_overlapped_query_range(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_key	*low_key,
	const union xfs_btree_key	*high_key,
	xfs_btree_query_range_fn	fn,
	void				*priv)
{
	union xfs_btree_ptr		ptr;
	union xfs_btree_ptr		*pp;
	union xfs_btree_key		rec_key;
	union xfs_btree_key		rec_hkey;
	union xfs_btree_key		*lkp;
	union xfs_btree_key		*hkp;
	union xfs_btree_rec		*recp;
	struct xfs_btree_block		*block;
	int				level;
	struct xfs_buf			*bp;
	int				i;
	int				error;

	 
	level = cur->bc_nlevels - 1;
	cur->bc_ops->init_ptr_from_cur(cur, &ptr);
	error = xfs_btree_lookup_get_block(cur, level, &ptr, &block);
	if (error)
		return error;
	xfs_btree_get_block(cur, level, &bp);
	trace_xfs_btree_overlapped_query_range(cur, level, bp);
#ifdef DEBUG
	error = xfs_btree_check_block(cur, block, level, bp);
	if (error)
		goto out;
#endif
	cur->bc_levels[level].ptr = 1;

	while (level < cur->bc_nlevels) {
		block = xfs_btree_get_block(cur, level, &bp);

		 
		if (cur->bc_levels[level].ptr >
					be16_to_cpu(block->bb_numrecs)) {
pop_up:
			if (level < cur->bc_nlevels - 1)
				cur->bc_levels[level + 1].ptr++;
			level++;
			continue;
		}

		if (level == 0) {
			 
			recp = xfs_btree_rec_addr(cur, cur->bc_levels[0].ptr,
					block);

			cur->bc_ops->init_high_key_from_rec(&rec_hkey, recp);
			cur->bc_ops->init_key_from_rec(&rec_key, recp);

			 
			if (xfs_btree_keycmp_lt(cur, high_key, &rec_key))
				goto pop_up;
			if (xfs_btree_keycmp_ge(cur, &rec_hkey, low_key)) {
				error = fn(cur, recp, priv);
				if (error)
					break;
			}
			cur->bc_levels[level].ptr++;
			continue;
		}

		 
		lkp = xfs_btree_key_addr(cur, cur->bc_levels[level].ptr, block);
		hkp = xfs_btree_high_key_addr(cur, cur->bc_levels[level].ptr,
				block);
		pp = xfs_btree_ptr_addr(cur, cur->bc_levels[level].ptr, block);

		 
		if (xfs_btree_keycmp_lt(cur, high_key, lkp))
			goto pop_up;
		if (xfs_btree_keycmp_ge(cur, hkp, low_key)) {
			level--;
			error = xfs_btree_lookup_get_block(cur, level, pp,
					&block);
			if (error)
				goto out;
			xfs_btree_get_block(cur, level, &bp);
			trace_xfs_btree_overlapped_query_range(cur, level, bp);
#ifdef DEBUG
			error = xfs_btree_check_block(cur, block, level, bp);
			if (error)
				goto out;
#endif
			cur->bc_levels[level].ptr = 1;
			continue;
		}
		cur->bc_levels[level].ptr++;
	}

out:
	 
	if (cur->bc_levels[0].bp == NULL) {
		for (i = 0; i < cur->bc_nlevels; i++) {
			if (cur->bc_levels[i].bp) {
				xfs_trans_brelse(cur->bc_tp,
						cur->bc_levels[i].bp);
				cur->bc_levels[i].bp = NULL;
				cur->bc_levels[i].ptr = 0;
				cur->bc_levels[i].ra = 0;
			}
		}
	}

	return error;
}

static inline void
xfs_btree_key_from_irec(
	struct xfs_btree_cur		*cur,
	union xfs_btree_key		*key,
	const union xfs_btree_irec	*irec)
{
	union xfs_btree_rec		rec;

	cur->bc_rec = *irec;
	cur->bc_ops->init_rec_from_cur(cur, &rec);
	cur->bc_ops->init_key_from_rec(key, &rec);
}

 
int
xfs_btree_query_range(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_irec	*low_rec,
	const union xfs_btree_irec	*high_rec,
	xfs_btree_query_range_fn	fn,
	void				*priv)
{
	union xfs_btree_key		low_key;
	union xfs_btree_key		high_key;

	 
	xfs_btree_key_from_irec(cur, &high_key, high_rec);
	xfs_btree_key_from_irec(cur, &low_key, low_rec);

	 
	if (!xfs_btree_keycmp_le(cur, &low_key, &high_key))
		return -EINVAL;

	if (!(cur->bc_flags & XFS_BTREE_OVERLAPPING))
		return xfs_btree_simple_query_range(cur, &low_key,
				&high_key, fn, priv);
	return xfs_btree_overlapped_query_range(cur, &low_key, &high_key,
			fn, priv);
}

 
int
xfs_btree_query_all(
	struct xfs_btree_cur		*cur,
	xfs_btree_query_range_fn	fn,
	void				*priv)
{
	union xfs_btree_key		low_key;
	union xfs_btree_key		high_key;

	memset(&cur->bc_rec, 0, sizeof(cur->bc_rec));
	memset(&low_key, 0, sizeof(low_key));
	memset(&high_key, 0xFF, sizeof(high_key));

	return xfs_btree_simple_query_range(cur, &low_key, &high_key, fn, priv);
}

static int
xfs_btree_count_blocks_helper(
	struct xfs_btree_cur	*cur,
	int			level,
	void			*data)
{
	xfs_extlen_t		*blocks = data;
	(*blocks)++;

	return 0;
}

 
int
xfs_btree_count_blocks(
	struct xfs_btree_cur	*cur,
	xfs_extlen_t		*blocks)
{
	*blocks = 0;
	return xfs_btree_visit_blocks(cur, xfs_btree_count_blocks_helper,
			XFS_BTREE_VISIT_ALL, blocks);
}

 
int64_t
xfs_btree_diff_two_ptrs(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_ptr	*a,
	const union xfs_btree_ptr	*b)
{
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return (int64_t)be64_to_cpu(a->l) - be64_to_cpu(b->l);
	return (int64_t)be32_to_cpu(a->s) - be32_to_cpu(b->s);
}

struct xfs_btree_has_records {
	 
	union xfs_btree_key		start_key;
	union xfs_btree_key		end_key;

	 
	const union xfs_btree_key	*key_mask;

	 
	union xfs_btree_key		high_key;

	enum xbtree_recpacking		outcome;
};

STATIC int
xfs_btree_has_records_helper(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_rec	*rec,
	void				*priv)
{
	union xfs_btree_key		rec_key;
	union xfs_btree_key		rec_high_key;
	struct xfs_btree_has_records	*info = priv;
	enum xbtree_key_contig		key_contig;

	cur->bc_ops->init_key_from_rec(&rec_key, rec);

	if (info->outcome == XBTREE_RECPACKING_EMPTY) {
		info->outcome = XBTREE_RECPACKING_SPARSE;

		 
		if (xfs_btree_masked_keycmp_lt(cur, &info->start_key, &rec_key,
					info->key_mask))
			return -ECANCELED;
	} else {
		 
		key_contig = cur->bc_ops->keys_contiguous(cur, &info->high_key,
					&rec_key, info->key_mask);
		if (key_contig == XBTREE_KEY_OVERLAP &&
				!(cur->bc_flags & XFS_BTREE_OVERLAPPING))
			return -EFSCORRUPTED;
		if (key_contig == XBTREE_KEY_GAP)
			return -ECANCELED;
	}

	 
	cur->bc_ops->init_high_key_from_rec(&rec_high_key, rec);
	if (xfs_btree_masked_keycmp_gt(cur, &rec_high_key, &info->high_key,
				info->key_mask))
		info->high_key = rec_high_key;  

	return 0;
}

 
int
xfs_btree_has_records(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_irec	*low,
	const union xfs_btree_irec	*high,
	const union xfs_btree_key	*mask,
	enum xbtree_recpacking		*outcome)
{
	struct xfs_btree_has_records	info = {
		.outcome		= XBTREE_RECPACKING_EMPTY,
		.key_mask		= mask,
	};
	int				error;

	 
	if (!cur->bc_ops->keys_contiguous) {
		ASSERT(0);
		return -EOPNOTSUPP;
	}

	xfs_btree_key_from_irec(cur, &info.start_key, low);
	xfs_btree_key_from_irec(cur, &info.end_key, high);

	error = xfs_btree_query_range(cur, low, high,
			xfs_btree_has_records_helper, &info);
	if (error == -ECANCELED)
		goto out;
	if (error)
		return error;

	if (info.outcome == XBTREE_RECPACKING_EMPTY)
		goto out;

	 
	if (xfs_btree_masked_keycmp_ge(cur, &info.high_key, &info.end_key,
				mask))
		info.outcome = XBTREE_RECPACKING_FULL;

out:
	*outcome = info.outcome;
	return 0;
}

 
bool
xfs_btree_has_more_records(
	struct xfs_btree_cur	*cur)
{
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;

	block = xfs_btree_get_block(cur, 0, &bp);

	 
	if (cur->bc_levels[0].ptr < xfs_btree_get_numrecs(block))
		return true;

	 
	if (cur->bc_flags & XFS_BTREE_LONG_PTRS)
		return block->bb_u.l.bb_rightsib != cpu_to_be64(NULLFSBLOCK);
	else
		return block->bb_u.s.bb_rightsib != cpu_to_be32(NULLAGBLOCK);
}

 
int __init
xfs_btree_init_cur_caches(void)
{
	int		error;

	error = xfs_allocbt_init_cur_cache();
	if (error)
		return error;
	error = xfs_inobt_init_cur_cache();
	if (error)
		goto err;
	error = xfs_bmbt_init_cur_cache();
	if (error)
		goto err;
	error = xfs_rmapbt_init_cur_cache();
	if (error)
		goto err;
	error = xfs_refcountbt_init_cur_cache();
	if (error)
		goto err;

	return 0;
err:
	xfs_btree_destroy_cur_caches();
	return error;
}

 
void
xfs_btree_destroy_cur_caches(void)
{
	xfs_allocbt_destroy_cur_cache();
	xfs_inobt_destroy_cur_cache();
	xfs_bmbt_destroy_cur_cache();
	xfs_rmapbt_destroy_cur_cache();
	xfs_refcountbt_destroy_cur_cache();
}

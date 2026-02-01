
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_shared.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_btree.h"
#include "xfs_rmap.h"
#include "xfs_alloc_btree.h"
#include "xfs_alloc.h"
#include "xfs_extent_busy.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_log.h"
#include "xfs_ag.h"
#include "xfs_ag_resv.h"
#include "xfs_bmap.h"

struct kmem_cache	*xfs_extfree_item_cache;

struct workqueue_struct *xfs_alloc_wq;

#define XFS_ABSDIFF(a,b)	(((a) <= (b)) ? ((b) - (a)) : ((a) - (b)))

#define	XFSA_FIXUP_BNO_OK	1
#define	XFSA_FIXUP_CNT_OK	2

 
unsigned int
xfs_agfl_size(
	struct xfs_mount	*mp)
{
	unsigned int		size = mp->m_sb.sb_sectsize;

	if (xfs_has_crc(mp))
		size -= sizeof(struct xfs_agfl);

	return size / sizeof(xfs_agblock_t);
}

unsigned int
xfs_refc_block(
	struct xfs_mount	*mp)
{
	if (xfs_has_rmapbt(mp))
		return XFS_RMAP_BLOCK(mp) + 1;
	if (xfs_has_finobt(mp))
		return XFS_FIBT_BLOCK(mp) + 1;
	return XFS_IBT_BLOCK(mp) + 1;
}

xfs_extlen_t
xfs_prealloc_blocks(
	struct xfs_mount	*mp)
{
	if (xfs_has_reflink(mp))
		return xfs_refc_block(mp) + 1;
	if (xfs_has_rmapbt(mp))
		return XFS_RMAP_BLOCK(mp) + 1;
	if (xfs_has_finobt(mp))
		return XFS_FIBT_BLOCK(mp) + 1;
	return XFS_IBT_BLOCK(mp) + 1;
}

 
#define XFS_ALLOCBT_AGFL_RESERVE	4

 
unsigned int
xfs_alloc_set_aside(
	struct xfs_mount	*mp)
{
	return mp->m_sb.sb_agcount * (XFS_ALLOCBT_AGFL_RESERVE + 4);
}

 
unsigned int
xfs_alloc_ag_max_usable(
	struct xfs_mount	*mp)
{
	unsigned int		blocks;

	blocks = XFS_BB_TO_FSB(mp, XFS_FSS_TO_BB(mp, 4));  
	blocks += XFS_ALLOCBT_AGFL_RESERVE;
	blocks += 3;			 
	if (xfs_has_finobt(mp))
		blocks++;		 
	if (xfs_has_rmapbt(mp))
		blocks++;		 
	if (xfs_has_reflink(mp))
		blocks++;		 

	return mp->m_sb.sb_agblocks - blocks;
}

 
STATIC int				 
xfs_alloc_lookup_eq(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		bno,	 
	xfs_extlen_t		len,	 
	int			*stat)	 
{
	int			error;

	cur->bc_rec.a.ar_startblock = bno;
	cur->bc_rec.a.ar_blockcount = len;
	error = xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
	cur->bc_ag.abt.active = (*stat == 1);
	return error;
}

 
int				 
xfs_alloc_lookup_ge(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		bno,	 
	xfs_extlen_t		len,	 
	int			*stat)	 
{
	int			error;

	cur->bc_rec.a.ar_startblock = bno;
	cur->bc_rec.a.ar_blockcount = len;
	error = xfs_btree_lookup(cur, XFS_LOOKUP_GE, stat);
	cur->bc_ag.abt.active = (*stat == 1);
	return error;
}

 
int					 
xfs_alloc_lookup_le(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		bno,	 
	xfs_extlen_t		len,	 
	int			*stat)	 
{
	int			error;
	cur->bc_rec.a.ar_startblock = bno;
	cur->bc_rec.a.ar_blockcount = len;
	error = xfs_btree_lookup(cur, XFS_LOOKUP_LE, stat);
	cur->bc_ag.abt.active = (*stat == 1);
	return error;
}

static inline bool
xfs_alloc_cur_active(
	struct xfs_btree_cur	*cur)
{
	return cur && cur->bc_ag.abt.active;
}

 
STATIC int				 
xfs_alloc_update(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		bno,	 
	xfs_extlen_t		len)	 
{
	union xfs_btree_rec	rec;

	rec.alloc.ar_startblock = cpu_to_be32(bno);
	rec.alloc.ar_blockcount = cpu_to_be32(len);
	return xfs_btree_update(cur, &rec);
}

 
void
xfs_alloc_btrec_to_irec(
	const union xfs_btree_rec	*rec,
	struct xfs_alloc_rec_incore	*irec)
{
	irec->ar_startblock = be32_to_cpu(rec->alloc.ar_startblock);
	irec->ar_blockcount = be32_to_cpu(rec->alloc.ar_blockcount);
}

 
xfs_failaddr_t
xfs_alloc_check_irec(
	struct xfs_btree_cur		*cur,
	const struct xfs_alloc_rec_incore *irec)
{
	struct xfs_perag		*pag = cur->bc_ag.pag;

	if (irec->ar_blockcount == 0)
		return __this_address;

	 
	if (!xfs_verify_agbext(pag, irec->ar_startblock, irec->ar_blockcount))
		return __this_address;

	return NULL;
}

static inline int
xfs_alloc_complain_bad_rec(
	struct xfs_btree_cur		*cur,
	xfs_failaddr_t			fa,
	const struct xfs_alloc_rec_incore *irec)
{
	struct xfs_mount		*mp = cur->bc_mp;

	xfs_warn(mp,
		"%s Freespace BTree record corruption in AG %d detected at %pS!",
		cur->bc_btnum == XFS_BTNUM_BNO ? "Block" : "Size",
		cur->bc_ag.pag->pag_agno, fa);
	xfs_warn(mp,
		"start block 0x%x block count 0x%x", irec->ar_startblock,
		irec->ar_blockcount);
	return -EFSCORRUPTED;
}

 
int					 
xfs_alloc_get_rec(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		*bno,	 
	xfs_extlen_t		*len,	 
	int			*stat)	 
{
	struct xfs_alloc_rec_incore irec;
	union xfs_btree_rec	*rec;
	xfs_failaddr_t		fa;
	int			error;

	error = xfs_btree_get_rec(cur, &rec, stat);
	if (error || !(*stat))
		return error;

	xfs_alloc_btrec_to_irec(rec, &irec);
	fa = xfs_alloc_check_irec(cur, &irec);
	if (fa)
		return xfs_alloc_complain_bad_rec(cur, fa, &irec);

	*bno = irec.ar_startblock;
	*len = irec.ar_blockcount;
	return 0;
}

 
STATIC bool
xfs_alloc_compute_aligned(
	xfs_alloc_arg_t	*args,		 
	xfs_agblock_t	foundbno,	 
	xfs_extlen_t	foundlen,	 
	xfs_agblock_t	*resbno,	 
	xfs_extlen_t	*reslen,	 
	unsigned	*busy_gen)
{
	xfs_agblock_t	bno = foundbno;
	xfs_extlen_t	len = foundlen;
	xfs_extlen_t	diff;
	bool		busy;

	 
	busy = xfs_extent_busy_trim(args, &bno, &len, busy_gen);

	 
	if (bno < args->min_agbno && bno + len > args->min_agbno) {
		diff = args->min_agbno - bno;
		if (len > diff) {
			bno += diff;
			len -= diff;
		}
	}

	if (args->alignment > 1 && len >= args->minlen) {
		xfs_agblock_t	aligned_bno = roundup(bno, args->alignment);

		diff = aligned_bno - bno;

		*resbno = aligned_bno;
		*reslen = diff >= len ? 0 : len - diff;
	} else {
		*resbno = bno;
		*reslen = len;
	}

	return busy;
}

 
STATIC xfs_extlen_t			 
xfs_alloc_compute_diff(
	xfs_agblock_t	wantbno,	 
	xfs_extlen_t	wantlen,	 
	xfs_extlen_t	alignment,	 
	int		datatype,	 
	xfs_agblock_t	freebno,	 
	xfs_extlen_t	freelen,	 
	xfs_agblock_t	*newbnop)	 
{
	xfs_agblock_t	freeend;	 
	xfs_agblock_t	newbno1;	 
	xfs_agblock_t	newbno2;	 
	xfs_extlen_t	newlen1=0;	 
	xfs_extlen_t	newlen2=0;	 
	xfs_agblock_t	wantend;	 
	bool		userdata = datatype & XFS_ALLOC_USERDATA;

	ASSERT(freelen >= wantlen);
	freeend = freebno + freelen;
	wantend = wantbno + wantlen;
	 
	if (freebno >= wantbno || (userdata && freeend < wantend)) {
		if ((newbno1 = roundup(freebno, alignment)) >= freeend)
			newbno1 = NULLAGBLOCK;
	} else if (freeend >= wantend && alignment > 1) {
		newbno1 = roundup(wantbno, alignment);
		newbno2 = newbno1 - alignment;
		if (newbno1 >= freeend)
			newbno1 = NULLAGBLOCK;
		else
			newlen1 = XFS_EXTLEN_MIN(wantlen, freeend - newbno1);
		if (newbno2 < freebno)
			newbno2 = NULLAGBLOCK;
		else
			newlen2 = XFS_EXTLEN_MIN(wantlen, freeend - newbno2);
		if (newbno1 != NULLAGBLOCK && newbno2 != NULLAGBLOCK) {
			if (newlen1 < newlen2 ||
			    (newlen1 == newlen2 &&
			     XFS_ABSDIFF(newbno1, wantbno) >
			     XFS_ABSDIFF(newbno2, wantbno)))
				newbno1 = newbno2;
		} else if (newbno2 != NULLAGBLOCK)
			newbno1 = newbno2;
	} else if (freeend >= wantend) {
		newbno1 = wantbno;
	} else if (alignment > 1) {
		newbno1 = roundup(freeend - wantlen, alignment);
		if (newbno1 > freeend - wantlen &&
		    newbno1 - alignment >= freebno)
			newbno1 -= alignment;
		else if (newbno1 >= freeend)
			newbno1 = NULLAGBLOCK;
	} else
		newbno1 = freeend - wantlen;
	*newbnop = newbno1;
	return newbno1 == NULLAGBLOCK ? 0 : XFS_ABSDIFF(newbno1, wantbno);
}

 
STATIC void
xfs_alloc_fix_len(
	xfs_alloc_arg_t	*args)		 
{
	xfs_extlen_t	k;
	xfs_extlen_t	rlen;

	ASSERT(args->mod < args->prod);
	rlen = args->len;
	ASSERT(rlen >= args->minlen);
	ASSERT(rlen <= args->maxlen);
	if (args->prod <= 1 || rlen < args->mod || rlen == args->maxlen ||
	    (args->mod == 0 && rlen < args->prod))
		return;
	k = rlen % args->prod;
	if (k == args->mod)
		return;
	if (k > args->mod)
		rlen = rlen - (k - args->mod);
	else
		rlen = rlen - args->prod + (args->mod - k);
	 
	if ((int)rlen < (int)args->minlen)
		return;
	ASSERT(rlen >= args->minlen && rlen <= args->maxlen);
	ASSERT(rlen % args->prod == args->mod);
	ASSERT(args->pag->pagf_freeblks + args->pag->pagf_flcount >=
		rlen + args->minleft);
	args->len = rlen;
}

 
STATIC int				 
xfs_alloc_fixup_trees(
	struct xfs_btree_cur *cnt_cur,	 
	struct xfs_btree_cur *bno_cur,	 
	xfs_agblock_t	fbno,		 
	xfs_extlen_t	flen,		 
	xfs_agblock_t	rbno,		 
	xfs_extlen_t	rlen,		 
	int		flags)		 
{
	int		error;		 
	int		i;		 
	xfs_agblock_t	nfbno1;		 
	xfs_agblock_t	nfbno2;		 
	xfs_extlen_t	nflen1=0;	 
	xfs_extlen_t	nflen2=0;	 
	struct xfs_mount *mp;

	mp = cnt_cur->bc_mp;

	 
	if (flags & XFSA_FIXUP_CNT_OK) {
#ifdef DEBUG
		if ((error = xfs_alloc_get_rec(cnt_cur, &nfbno1, &nflen1, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp,
				   i != 1 ||
				   nfbno1 != fbno ||
				   nflen1 != flen))
			return -EFSCORRUPTED;
#endif
	} else {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, fbno, flen, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;
	}
	 
	if (flags & XFSA_FIXUP_BNO_OK) {
#ifdef DEBUG
		if ((error = xfs_alloc_get_rec(bno_cur, &nfbno1, &nflen1, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp,
				   i != 1 ||
				   nfbno1 != fbno ||
				   nflen1 != flen))
			return -EFSCORRUPTED;
#endif
	} else {
		if ((error = xfs_alloc_lookup_eq(bno_cur, fbno, flen, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;
	}

#ifdef DEBUG
	if (bno_cur->bc_nlevels == 1 && cnt_cur->bc_nlevels == 1) {
		struct xfs_btree_block	*bnoblock;
		struct xfs_btree_block	*cntblock;

		bnoblock = XFS_BUF_TO_BLOCK(bno_cur->bc_levels[0].bp);
		cntblock = XFS_BUF_TO_BLOCK(cnt_cur->bc_levels[0].bp);

		if (XFS_IS_CORRUPT(mp,
				   bnoblock->bb_numrecs !=
				   cntblock->bb_numrecs))
			return -EFSCORRUPTED;
	}
#endif

	 
	if (rbno == fbno && rlen == flen)
		nfbno1 = nfbno2 = NULLAGBLOCK;
	else if (rbno == fbno) {
		nfbno1 = rbno + rlen;
		nflen1 = flen - rlen;
		nfbno2 = NULLAGBLOCK;
	} else if (rbno + rlen == fbno + flen) {
		nfbno1 = fbno;
		nflen1 = flen - rlen;
		nfbno2 = NULLAGBLOCK;
	} else {
		nfbno1 = fbno;
		nflen1 = rbno - fbno;
		nfbno2 = rbno + rlen;
		nflen2 = (fbno + flen) - nfbno2;
	}
	 
	if ((error = xfs_btree_delete(cnt_cur, &i)))
		return error;
	if (XFS_IS_CORRUPT(mp, i != 1))
		return -EFSCORRUPTED;
	 
	if (nfbno1 != NULLAGBLOCK) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, nfbno1, nflen1, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 0))
			return -EFSCORRUPTED;
		if ((error = xfs_btree_insert(cnt_cur, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;
	}
	if (nfbno2 != NULLAGBLOCK) {
		if ((error = xfs_alloc_lookup_eq(cnt_cur, nfbno2, nflen2, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 0))
			return -EFSCORRUPTED;
		if ((error = xfs_btree_insert(cnt_cur, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;
	}
	 
	if (nfbno1 == NULLAGBLOCK) {
		 
		if ((error = xfs_btree_delete(bno_cur, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;
	} else {
		 
		if ((error = xfs_alloc_update(bno_cur, nfbno1, nflen1)))
			return error;
	}
	if (nfbno2 != NULLAGBLOCK) {
		 
		if ((error = xfs_alloc_lookup_eq(bno_cur, nfbno2, nflen2, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 0))
			return -EFSCORRUPTED;
		if ((error = xfs_btree_insert(bno_cur, &i)))
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;
	}
	return 0;
}

 
static xfs_failaddr_t
xfs_agfl_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_mount;
	struct xfs_agfl	*agfl = XFS_BUF_TO_AGFL(bp);
	__be32		*agfl_bno = xfs_buf_to_agfl_bno(bp);
	int		i;

	if (!xfs_has_crc(mp))
		return NULL;

	if (!xfs_verify_magic(bp, agfl->agfl_magicnum))
		return __this_address;
	if (!uuid_equal(&agfl->agfl_uuid, &mp->m_sb.sb_meta_uuid))
		return __this_address;
	 
	if (bp->b_pag && be32_to_cpu(agfl->agfl_seqno) != bp->b_pag->pag_agno)
		return __this_address;

	for (i = 0; i < xfs_agfl_size(mp); i++) {
		if (be32_to_cpu(agfl_bno[i]) != NULLAGBLOCK &&
		    be32_to_cpu(agfl_bno[i]) >= mp->m_sb.sb_agblocks)
			return __this_address;
	}

	if (!xfs_log_check_lsn(mp, be64_to_cpu(XFS_BUF_TO_AGFL(bp)->agfl_lsn)))
		return __this_address;
	return NULL;
}

static void
xfs_agfl_read_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_mount;
	xfs_failaddr_t	fa;

	 
	if (!xfs_has_crc(mp))
		return;

	if (!xfs_buf_verify_cksum(bp, XFS_AGFL_CRC_OFF))
		xfs_verifier_error(bp, -EFSBADCRC, __this_address);
	else {
		fa = xfs_agfl_verify(bp);
		if (fa)
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
	}
}

static void
xfs_agfl_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	xfs_failaddr_t		fa;

	 
	if (!xfs_has_crc(mp))
		return;

	fa = xfs_agfl_verify(bp);
	if (fa) {
		xfs_verifier_error(bp, -EFSCORRUPTED, fa);
		return;
	}

	if (bip)
		XFS_BUF_TO_AGFL(bp)->agfl_lsn = cpu_to_be64(bip->bli_item.li_lsn);

	xfs_buf_update_cksum(bp, XFS_AGFL_CRC_OFF);
}

const struct xfs_buf_ops xfs_agfl_buf_ops = {
	.name = "xfs_agfl",
	.magic = { cpu_to_be32(XFS_AGFL_MAGIC), cpu_to_be32(XFS_AGFL_MAGIC) },
	.verify_read = xfs_agfl_read_verify,
	.verify_write = xfs_agfl_write_verify,
	.verify_struct = xfs_agfl_verify,
};

 
int
xfs_alloc_read_agfl(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		**bpp)
{
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_buf		*bp;
	int			error;

	error = xfs_trans_read_buf(
			mp, tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, pag->pag_agno, XFS_AGFL_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, &bp, &xfs_agfl_buf_ops);
	if (error)
		return error;
	xfs_buf_set_ref(bp, XFS_AGFL_REF);
	*bpp = bp;
	return 0;
}

STATIC int
xfs_alloc_update_counters(
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	long			len)
{
	struct xfs_agf		*agf = agbp->b_addr;

	agbp->b_pag->pagf_freeblks += len;
	be32_add_cpu(&agf->agf_freeblks, len);

	if (unlikely(be32_to_cpu(agf->agf_freeblks) >
		     be32_to_cpu(agf->agf_length))) {
		xfs_buf_mark_corrupt(agbp);
		return -EFSCORRUPTED;
	}

	xfs_alloc_log_agf(tp, agbp, XFS_AGF_FREEBLKS);
	return 0;
}

 
struct xfs_alloc_cur {
	struct xfs_btree_cur		*cnt;	 
	struct xfs_btree_cur		*bnolt;
	struct xfs_btree_cur		*bnogt;
	xfs_extlen_t			cur_len; 
	xfs_agblock_t			rec_bno; 
	xfs_extlen_t			rec_len; 
	xfs_agblock_t			bno;	 
	xfs_extlen_t			len;	 
	xfs_extlen_t			diff;	 
	unsigned int			busy_gen; 
	bool				busy;
};

 
static int
xfs_alloc_cur_setup(
	struct xfs_alloc_arg	*args,
	struct xfs_alloc_cur	*acur)
{
	int			error;
	int			i;

	acur->cur_len = args->maxlen;
	acur->rec_bno = 0;
	acur->rec_len = 0;
	acur->bno = 0;
	acur->len = 0;
	acur->diff = -1;
	acur->busy = false;
	acur->busy_gen = 0;

	 
	if (!acur->cnt)
		acur->cnt = xfs_allocbt_init_cursor(args->mp, args->tp,
					args->agbp, args->pag, XFS_BTNUM_CNT);
	error = xfs_alloc_lookup_ge(acur->cnt, 0, args->maxlen, &i);
	if (error)
		return error;

	 
	if (!acur->bnolt)
		acur->bnolt = xfs_allocbt_init_cursor(args->mp, args->tp,
					args->agbp, args->pag, XFS_BTNUM_BNO);
	if (!acur->bnogt)
		acur->bnogt = xfs_allocbt_init_cursor(args->mp, args->tp,
					args->agbp, args->pag, XFS_BTNUM_BNO);
	return i == 1 ? 0 : -ENOSPC;
}

static void
xfs_alloc_cur_close(
	struct xfs_alloc_cur	*acur,
	bool			error)
{
	int			cur_error = XFS_BTREE_NOERROR;

	if (error)
		cur_error = XFS_BTREE_ERROR;

	if (acur->cnt)
		xfs_btree_del_cursor(acur->cnt, cur_error);
	if (acur->bnolt)
		xfs_btree_del_cursor(acur->bnolt, cur_error);
	if (acur->bnogt)
		xfs_btree_del_cursor(acur->bnogt, cur_error);
	acur->cnt = acur->bnolt = acur->bnogt = NULL;
}

 
static int
xfs_alloc_cur_check(
	struct xfs_alloc_arg	*args,
	struct xfs_alloc_cur	*acur,
	struct xfs_btree_cur	*cur,
	int			*new)
{
	int			error, i;
	xfs_agblock_t		bno, bnoa, bnew;
	xfs_extlen_t		len, lena, diff = -1;
	bool			busy;
	unsigned		busy_gen = 0;
	bool			deactivate = false;
	bool			isbnobt = cur->bc_btnum == XFS_BTNUM_BNO;

	*new = 0;

	error = xfs_alloc_get_rec(cur, &bno, &len, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(args->mp, i != 1))
		return -EFSCORRUPTED;

	 
	if (len < args->minlen) {
		deactivate = !isbnobt;
		goto out;
	}

	busy = xfs_alloc_compute_aligned(args, bno, len, &bnoa, &lena,
					 &busy_gen);
	acur->busy |= busy;
	if (busy)
		acur->busy_gen = busy_gen;
	 
	if (bnoa < args->min_agbno || bnoa > args->max_agbno) {
		deactivate = isbnobt;
		goto out;
	}
	if (lena < args->minlen)
		goto out;

	args->len = XFS_EXTLEN_MIN(lena, args->maxlen);
	xfs_alloc_fix_len(args);
	ASSERT(args->len >= args->minlen);
	if (args->len < acur->len)
		goto out;

	 
	diff = xfs_alloc_compute_diff(args->agbno, args->len,
				      args->alignment, args->datatype,
				      bnoa, lena, &bnew);
	if (bnew == NULLAGBLOCK)
		goto out;

	 
	if (diff > acur->diff) {
		deactivate = isbnobt;
		goto out;
	}

	ASSERT(args->len > acur->len ||
	       (args->len == acur->len && diff <= acur->diff));
	acur->rec_bno = bno;
	acur->rec_len = len;
	acur->bno = bnew;
	acur->len = args->len;
	acur->diff = diff;
	*new = 1;

	 
	if (acur->diff == 0 && acur->len == args->maxlen)
		deactivate = true;
out:
	if (deactivate)
		cur->bc_ag.abt.active = false;
	trace_xfs_alloc_cur_check(args->mp, cur->bc_btnum, bno, len, diff,
				  *new);
	return 0;
}

 
STATIC int
xfs_alloc_cur_finish(
	struct xfs_alloc_arg	*args,
	struct xfs_alloc_cur	*acur)
{
	struct xfs_agf __maybe_unused *agf = args->agbp->b_addr;
	int			error;

	ASSERT(acur->cnt && acur->bnolt);
	ASSERT(acur->bno >= acur->rec_bno);
	ASSERT(acur->bno + acur->len <= acur->rec_bno + acur->rec_len);
	ASSERT(acur->rec_bno + acur->rec_len <= be32_to_cpu(agf->agf_length));

	error = xfs_alloc_fixup_trees(acur->cnt, acur->bnolt, acur->rec_bno,
				      acur->rec_len, acur->bno, acur->len, 0);
	if (error)
		return error;

	args->agbno = acur->bno;
	args->len = acur->len;
	args->wasfromfl = 0;

	trace_xfs_alloc_cur(args);
	return 0;
}

 
STATIC int
xfs_alloc_cntbt_iter(
	struct xfs_alloc_arg		*args,
	struct xfs_alloc_cur		*acur)
{
	struct xfs_btree_cur	*cur = acur->cnt;
	xfs_agblock_t		bno;
	xfs_extlen_t		len, cur_len;
	int			error;
	int			i;

	if (!xfs_alloc_cur_active(cur))
		return 0;

	 
	cur_len = acur->cur_len;
	error = xfs_alloc_lookup_ge(cur, args->agbno, cur_len, &i);
	if (error)
		return error;
	if (i == 0)
		return 0;
	error = xfs_alloc_get_rec(cur, &bno, &len, &i);
	if (error)
		return error;

	 
	error = xfs_alloc_cur_check(args, acur, cur, &i);
	if (error)
		return error;
	ASSERT(len >= acur->cur_len);
	acur->cur_len = len;

	 
	if (bno > args->agbno) {
		error = xfs_btree_decrement(cur, 0, &i);
		if (!error && i) {
			error = xfs_alloc_get_rec(cur, &bno, &len, &i);
			if (!error && i && len == acur->cur_len)
				error = xfs_alloc_cur_check(args, acur, cur,
							    &i);
		}
		if (error)
			return error;
	}

	 
	cur_len <<= 1;
	if (!acur->len || acur->cur_len >= cur_len)
		acur->cur_len++;
	else
		acur->cur_len = cur_len;

	return error;
}

 
STATIC int			 
xfs_alloc_ag_vextent_small(
	struct xfs_alloc_arg	*args,	 
	struct xfs_btree_cur	*ccur,	 
	xfs_agblock_t		*fbnop,	 
	xfs_extlen_t		*flenp,	 
	int			*stat)	 
{
	struct xfs_agf		*agf = args->agbp->b_addr;
	int			error = 0;
	xfs_agblock_t		fbno = NULLAGBLOCK;
	xfs_extlen_t		flen = 0;
	int			i = 0;

	 
	if (ccur)
		error = xfs_btree_decrement(ccur, 0, &i);
	if (error)
		goto error;
	if (i) {
		error = xfs_alloc_get_rec(ccur, &fbno, &flen, &i);
		if (error)
			goto error;
		if (XFS_IS_CORRUPT(args->mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error;
		}
		goto out;
	}

	if (args->minlen != 1 || args->alignment != 1 ||
	    args->resv == XFS_AG_RESV_AGFL ||
	    be32_to_cpu(agf->agf_flcount) <= args->minleft)
		goto out;

	error = xfs_alloc_get_freelist(args->pag, args->tp, args->agbp,
			&fbno, 0);
	if (error)
		goto error;
	if (fbno == NULLAGBLOCK)
		goto out;

	xfs_extent_busy_reuse(args->mp, args->pag, fbno, 1,
			      (args->datatype & XFS_ALLOC_NOBUSY));

	if (args->datatype & XFS_ALLOC_USERDATA) {
		struct xfs_buf	*bp;

		error = xfs_trans_get_buf(args->tp, args->mp->m_ddev_targp,
				XFS_AGB_TO_DADDR(args->mp, args->agno, fbno),
				args->mp->m_bsize, 0, &bp);
		if (error)
			goto error;
		xfs_trans_binval(args->tp, bp);
	}
	*fbnop = args->agbno = fbno;
	*flenp = args->len = 1;
	if (XFS_IS_CORRUPT(args->mp, fbno >= be32_to_cpu(agf->agf_length))) {
		error = -EFSCORRUPTED;
		goto error;
	}
	args->wasfromfl = 1;
	trace_xfs_alloc_small_freelist(args);

	 
	error = xfs_rmap_free(args->tp, args->agbp, args->pag, fbno, 1,
			      &XFS_RMAP_OINFO_AG);
	if (error)
		goto error;

	*stat = 0;
	return 0;

out:
	 
	if (flen < args->minlen) {
		args->agbno = NULLAGBLOCK;
		trace_xfs_alloc_small_notenough(args);
		flen = 0;
	}
	*fbnop = fbno;
	*flenp = flen;
	*stat = 1;
	trace_xfs_alloc_small_done(args);
	return 0;

error:
	trace_xfs_alloc_small_error(args);
	return error;
}

 
STATIC int			 
xfs_alloc_ag_vextent_exact(
	xfs_alloc_arg_t	*args)	 
{
	struct xfs_agf __maybe_unused *agf = args->agbp->b_addr;
	struct xfs_btree_cur *bno_cur; 
	struct xfs_btree_cur *cnt_cur; 
	int		error;
	xfs_agblock_t	fbno;	 
	xfs_extlen_t	flen;	 
	xfs_agblock_t	tbno;	 
	xfs_extlen_t	tlen;	 
	xfs_agblock_t	tend;	 
	int		i;	 
	unsigned	busy_gen;

	ASSERT(args->alignment == 1);

	 
	bno_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
					  args->pag, XFS_BTNUM_BNO);

	 
	error = xfs_alloc_lookup_le(bno_cur, args->agbno, args->minlen, &i);
	if (error)
		goto error0;
	if (!i)
		goto not_found;

	 
	error = xfs_alloc_get_rec(bno_cur, &fbno, &flen, &i);
	if (error)
		goto error0;
	if (XFS_IS_CORRUPT(args->mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	ASSERT(fbno <= args->agbno);

	 
	tbno = fbno;
	tlen = flen;
	xfs_extent_busy_trim(args, &tbno, &tlen, &busy_gen);

	 
	if (tbno > args->agbno)
		goto not_found;
	if (tlen < args->minlen)
		goto not_found;
	tend = tbno + tlen;
	if (tend < args->agbno + args->minlen)
		goto not_found;

	 
	args->len = XFS_AGBLOCK_MIN(tend, args->agbno + args->maxlen)
						- args->agbno;
	xfs_alloc_fix_len(args);
	ASSERT(args->agbno + args->len <= tend);

	 
	cnt_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
					args->pag, XFS_BTNUM_CNT);
	ASSERT(args->agbno + args->len <= be32_to_cpu(agf->agf_length));
	error = xfs_alloc_fixup_trees(cnt_cur, bno_cur, fbno, flen, args->agbno,
				      args->len, XFSA_FIXUP_BNO_OK);
	if (error) {
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
		goto error0;
	}

	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);

	args->wasfromfl = 0;
	trace_xfs_alloc_exact_done(args);
	return 0;

not_found:
	 
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	args->agbno = NULLAGBLOCK;
	trace_xfs_alloc_exact_notfound(args);
	return 0;

error0:
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_ERROR);
	trace_xfs_alloc_exact_error(args);
	return error;
}

 
STATIC int
xfs_alloc_walk_iter(
	struct xfs_alloc_arg	*args,
	struct xfs_alloc_cur	*acur,
	struct xfs_btree_cur	*cur,
	bool			increment,
	bool			find_one,  
	int			count,     
	int			*stat)
{
	int			error;
	int			i;

	*stat = 0;

	 
	while (xfs_alloc_cur_active(cur) && count) {
		error = xfs_alloc_cur_check(args, acur, cur, &i);
		if (error)
			return error;
		if (i == 1) {
			*stat = 1;
			if (find_one)
				break;
		}
		if (!xfs_alloc_cur_active(cur))
			break;

		if (increment)
			error = xfs_btree_increment(cur, 0, &i);
		else
			error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			return error;
		if (i == 0)
			cur->bc_ag.abt.active = false;

		if (count > 0)
			count--;
	}

	return 0;
}

 
STATIC int
xfs_alloc_ag_vextent_locality(
	struct xfs_alloc_arg	*args,
	struct xfs_alloc_cur	*acur,
	int			*stat)
{
	struct xfs_btree_cur	*fbcur = NULL;
	int			error;
	int			i;
	bool			fbinc;

	ASSERT(acur->len == 0);

	*stat = 0;

	error = xfs_alloc_lookup_ge(acur->cnt, args->agbno, acur->cur_len, &i);
	if (error)
		return error;
	error = xfs_alloc_lookup_le(acur->bnolt, args->agbno, 0, &i);
	if (error)
		return error;
	error = xfs_alloc_lookup_ge(acur->bnogt, args->agbno, 0, &i);
	if (error)
		return error;

	 
	while (xfs_alloc_cur_active(acur->bnolt) ||
	       xfs_alloc_cur_active(acur->bnogt) ||
	       xfs_alloc_cur_active(acur->cnt)) {

		trace_xfs_alloc_cur_lookup(args);

		 
		error = xfs_alloc_walk_iter(args, acur, acur->bnolt, false,
					    true, 1, &i);
		if (error)
			return error;
		if (i == 1) {
			trace_xfs_alloc_cur_left(args);
			fbcur = acur->bnogt;
			fbinc = true;
			break;
		}
		error = xfs_alloc_walk_iter(args, acur, acur->bnogt, true, true,
					    1, &i);
		if (error)
			return error;
		if (i == 1) {
			trace_xfs_alloc_cur_right(args);
			fbcur = acur->bnolt;
			fbinc = false;
			break;
		}

		 
		error = xfs_alloc_cntbt_iter(args, acur);
		if (error)
			return error;
		if (!xfs_alloc_cur_active(acur->cnt)) {
			trace_xfs_alloc_cur_lookup_done(args);
			break;
		}
	}

	 
	if (!xfs_alloc_cur_active(acur->cnt) && !acur->len && !acur->busy) {
		error = xfs_btree_decrement(acur->cnt, 0, &i);
		if (error)
			return error;
		if (i) {
			acur->cnt->bc_ag.abt.active = true;
			fbcur = acur->cnt;
			fbinc = false;
		}
	}

	 
	if (fbcur) {
		error = xfs_alloc_walk_iter(args, acur, fbcur, fbinc, true, -1,
					    &i);
		if (error)
			return error;
	}

	if (acur->len)
		*stat = 1;

	return 0;
}

 
static int
xfs_alloc_ag_vextent_lastblock(
	struct xfs_alloc_arg	*args,
	struct xfs_alloc_cur	*acur,
	xfs_agblock_t		*bno,
	xfs_extlen_t		*len,
	bool			*allocated)
{
	int			error;
	int			i;

#ifdef DEBUG
	 
	if (get_random_u32_below(2))
		return 0;
#endif

	 
	if (*len || args->alignment > 1) {
		acur->cnt->bc_levels[0].ptr = 1;
		do {
			error = xfs_alloc_get_rec(acur->cnt, bno, len, &i);
			if (error)
				return error;
			if (XFS_IS_CORRUPT(args->mp, i != 1))
				return -EFSCORRUPTED;
			if (*len >= args->minlen)
				break;
			error = xfs_btree_increment(acur->cnt, 0, &i);
			if (error)
				return error;
		} while (i);
		ASSERT(*len >= args->minlen);
		if (!i)
			return 0;
	}

	error = xfs_alloc_walk_iter(args, acur, acur->cnt, true, false, -1, &i);
	if (error)
		return error;

	 
	if (acur->len == 0)
		return 0;

	trace_xfs_alloc_near_first(args);
	*allocated = true;
	return 0;
}

 
STATIC int
xfs_alloc_ag_vextent_near(
	struct xfs_alloc_arg	*args,
	uint32_t		alloc_flags)
{
	struct xfs_alloc_cur	acur = {};
	int			error;		 
	int			i;		 
	xfs_agblock_t		bno;
	xfs_extlen_t		len;

	 
	if (!args->min_agbno && !args->max_agbno)
		args->max_agbno = args->mp->m_sb.sb_agblocks - 1;
	ASSERT(args->min_agbno <= args->max_agbno);

	 
	if (args->agbno < args->min_agbno)
		args->agbno = args->min_agbno;
	if (args->agbno > args->max_agbno)
		args->agbno = args->max_agbno;

	 
	alloc_flags |= XFS_ALLOC_FLAG_TRYFLUSH;
restart:
	len = 0;

	 
	error = xfs_alloc_cur_setup(args, &acur);
	if (error == -ENOSPC) {
		error = xfs_alloc_ag_vextent_small(args, acur.cnt, &bno,
				&len, &i);
		if (error)
			goto out;
		if (i == 0 || len == 0) {
			trace_xfs_alloc_near_noentry(args);
			goto out;
		}
		ASSERT(i == 1);
	} else if (error) {
		goto out;
	}

	 
	if (xfs_btree_islastblock(acur.cnt, 0)) {
		bool		allocated = false;

		error = xfs_alloc_ag_vextent_lastblock(args, &acur, &bno, &len,
				&allocated);
		if (error)
			goto out;
		if (allocated)
			goto alloc_finish;
	}

	 
	error = xfs_alloc_ag_vextent_locality(args, &acur, &i);
	if (error)
		goto out;

	 
	if (!acur.len) {
		if (acur.busy) {
			 
			trace_xfs_alloc_near_busy(args);
			error = xfs_extent_busy_flush(args->tp, args->pag,
					acur.busy_gen, alloc_flags);
			if (error)
				goto out;

			alloc_flags &= ~XFS_ALLOC_FLAG_TRYFLUSH;
			goto restart;
		}
		trace_xfs_alloc_size_neither(args);
		args->agbno = NULLAGBLOCK;
		goto out;
	}

alloc_finish:
	 
	error = xfs_alloc_cur_finish(args, &acur);

out:
	xfs_alloc_cur_close(&acur, error);
	return error;
}

 
static int
xfs_alloc_ag_vextent_size(
	struct xfs_alloc_arg	*args,
	uint32_t		alloc_flags)
{
	struct xfs_agf		*agf = args->agbp->b_addr;
	struct xfs_btree_cur	*bno_cur;
	struct xfs_btree_cur	*cnt_cur;
	xfs_agblock_t		fbno;		 
	xfs_extlen_t		flen;		 
	xfs_agblock_t		rbno;		 
	xfs_extlen_t		rlen;		 
	bool			busy;
	unsigned		busy_gen;
	int			error;
	int			i;

	 
	alloc_flags |= XFS_ALLOC_FLAG_TRYFLUSH;
restart:
	 
	cnt_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
					args->pag, XFS_BTNUM_CNT);
	bno_cur = NULL;

	 
	if ((error = xfs_alloc_lookup_ge(cnt_cur, 0,
			args->maxlen + args->alignment - 1, &i)))
		goto error0;

	 
	if (!i) {
		error = xfs_alloc_ag_vextent_small(args, cnt_cur,
						   &fbno, &flen, &i);
		if (error)
			goto error0;
		if (i == 0 || flen == 0) {
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			trace_xfs_alloc_size_noentry(args);
			return 0;
		}
		ASSERT(i == 1);
		busy = xfs_alloc_compute_aligned(args, fbno, flen, &rbno,
				&rlen, &busy_gen);
	} else {
		 
		for (;;) {
			error = xfs_alloc_get_rec(cnt_cur, &fbno, &flen, &i);
			if (error)
				goto error0;
			if (XFS_IS_CORRUPT(args->mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto error0;
			}

			busy = xfs_alloc_compute_aligned(args, fbno, flen,
					&rbno, &rlen, &busy_gen);

			if (rlen >= args->maxlen)
				break;

			error = xfs_btree_increment(cnt_cur, 0, &i);
			if (error)
				goto error0;
			if (i)
				continue;

			 
			trace_xfs_alloc_size_busy(args);
			error = xfs_extent_busy_flush(args->tp, args->pag,
					busy_gen, alloc_flags);
			if (error)
				goto error0;

			alloc_flags &= ~XFS_ALLOC_FLAG_TRYFLUSH;
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			goto restart;
		}
	}

	 
	rlen = XFS_EXTLEN_MIN(args->maxlen, rlen);
	if (XFS_IS_CORRUPT(args->mp,
			   rlen != 0 &&
			   (rlen > flen ||
			    rbno + rlen > fbno + flen))) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	if (rlen < args->maxlen) {
		xfs_agblock_t	bestfbno;
		xfs_extlen_t	bestflen;
		xfs_agblock_t	bestrbno;
		xfs_extlen_t	bestrlen;

		bestrlen = rlen;
		bestrbno = rbno;
		bestflen = flen;
		bestfbno = fbno;
		for (;;) {
			if ((error = xfs_btree_decrement(cnt_cur, 0, &i)))
				goto error0;
			if (i == 0)
				break;
			if ((error = xfs_alloc_get_rec(cnt_cur, &fbno, &flen,
					&i)))
				goto error0;
			if (XFS_IS_CORRUPT(args->mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto error0;
			}
			if (flen < bestrlen)
				break;
			busy = xfs_alloc_compute_aligned(args, fbno, flen,
					&rbno, &rlen, &busy_gen);
			rlen = XFS_EXTLEN_MIN(args->maxlen, rlen);
			if (XFS_IS_CORRUPT(args->mp,
					   rlen != 0 &&
					   (rlen > flen ||
					    rbno + rlen > fbno + flen))) {
				error = -EFSCORRUPTED;
				goto error0;
			}
			if (rlen > bestrlen) {
				bestrlen = rlen;
				bestrbno = rbno;
				bestflen = flen;
				bestfbno = fbno;
				if (rlen == args->maxlen)
					break;
			}
		}
		if ((error = xfs_alloc_lookup_eq(cnt_cur, bestfbno, bestflen,
				&i)))
			goto error0;
		if (XFS_IS_CORRUPT(args->mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		rlen = bestrlen;
		rbno = bestrbno;
		flen = bestflen;
		fbno = bestfbno;
	}
	args->wasfromfl = 0;
	 
	args->len = rlen;
	if (rlen < args->minlen) {
		if (busy) {
			 
			trace_xfs_alloc_size_busy(args);
			error = xfs_extent_busy_flush(args->tp, args->pag,
					busy_gen, alloc_flags);
			if (error)
				goto error0;

			alloc_flags &= ~XFS_ALLOC_FLAG_TRYFLUSH;
			xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
			goto restart;
		}
		goto out_nominleft;
	}
	xfs_alloc_fix_len(args);

	rlen = args->len;
	if (XFS_IS_CORRUPT(args->mp, rlen > flen)) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	 
	bno_cur = xfs_allocbt_init_cursor(args->mp, args->tp, args->agbp,
					args->pag, XFS_BTNUM_BNO);
	if ((error = xfs_alloc_fixup_trees(cnt_cur, bno_cur, fbno, flen,
			rbno, rlen, XFSA_FIXUP_CNT_OK)))
		goto error0;
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	cnt_cur = bno_cur = NULL;
	args->len = rlen;
	args->agbno = rbno;
	if (XFS_IS_CORRUPT(args->mp,
			   args->agbno + args->len >
			   be32_to_cpu(agf->agf_length))) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	trace_xfs_alloc_size_done(args);
	return 0;

error0:
	trace_xfs_alloc_size_error(args);
	if (cnt_cur)
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
	if (bno_cur)
		xfs_btree_del_cursor(bno_cur, XFS_BTREE_ERROR);
	return error;

out_nominleft:
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	trace_xfs_alloc_size_nominleft(args);
	args->agbno = NULLAGBLOCK;
	return 0;
}

 
STATIC int
xfs_free_ag_extent(
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	xfs_agnumber_t			agno,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo,
	enum xfs_ag_resv_type		type)
{
	struct xfs_mount		*mp;
	struct xfs_btree_cur		*bno_cur;
	struct xfs_btree_cur		*cnt_cur;
	xfs_agblock_t			gtbno;  
	xfs_extlen_t			gtlen;  
	xfs_agblock_t			ltbno;  
	xfs_extlen_t			ltlen;  
	xfs_agblock_t			nbno;  
	xfs_extlen_t			nlen;  
	int				haveleft;  
	int				haveright;  
	int				i;
	int				error;
	struct xfs_perag		*pag = agbp->b_pag;

	bno_cur = cnt_cur = NULL;
	mp = tp->t_mountp;

	if (!xfs_rmap_should_skip_owner_update(oinfo)) {
		error = xfs_rmap_free(tp, agbp, pag, bno, len, oinfo);
		if (error)
			goto error0;
	}

	 
	bno_cur = xfs_allocbt_init_cursor(mp, tp, agbp, pag, XFS_BTNUM_BNO);
	 
	if ((error = xfs_alloc_lookup_le(bno_cur, bno, len, &haveleft)))
		goto error0;
	if (haveleft) {
		 
		if ((error = xfs_alloc_get_rec(bno_cur, &ltbno, &ltlen, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		if (ltbno + ltlen < bno)
			haveleft = 0;
		else {
			 
			if (XFS_IS_CORRUPT(mp, ltbno + ltlen > bno)) {
				error = -EFSCORRUPTED;
				goto error0;
			}
		}
	}
	 
	if ((error = xfs_btree_increment(bno_cur, 0, &haveright)))
		goto error0;
	if (haveright) {
		 
		if ((error = xfs_alloc_get_rec(bno_cur, &gtbno, &gtlen, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		if (bno + len < gtbno)
			haveright = 0;
		else {
			 
			if (XFS_IS_CORRUPT(mp, bno + len > gtbno)) {
				error = -EFSCORRUPTED;
				goto error0;
			}
		}
	}
	 
	cnt_cur = xfs_allocbt_init_cursor(mp, tp, agbp, pag, XFS_BTNUM_CNT);
	 
	if (haveleft && haveright) {
		 
		if ((error = xfs_alloc_lookup_eq(cnt_cur, ltbno, ltlen, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		if ((error = xfs_alloc_lookup_eq(cnt_cur, gtbno, gtlen, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		if ((error = xfs_btree_delete(bno_cur, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		if ((error = xfs_btree_decrement(bno_cur, 0, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
#ifdef DEBUG
		 
		{
			xfs_agblock_t	xxbno;
			xfs_extlen_t	xxlen;

			if ((error = xfs_alloc_get_rec(bno_cur, &xxbno, &xxlen,
					&i)))
				goto error0;
			if (XFS_IS_CORRUPT(mp,
					   i != 1 ||
					   xxbno != ltbno ||
					   xxlen != ltlen)) {
				error = -EFSCORRUPTED;
				goto error0;
			}
		}
#endif
		 
		nbno = ltbno;
		nlen = len + ltlen + gtlen;
		if ((error = xfs_alloc_update(bno_cur, nbno, nlen)))
			goto error0;
	}
	 
	else if (haveleft) {
		 
		if ((error = xfs_alloc_lookup_eq(cnt_cur, ltbno, ltlen, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		if ((error = xfs_btree_decrement(bno_cur, 0, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		nbno = ltbno;
		nlen = len + ltlen;
		if ((error = xfs_alloc_update(bno_cur, nbno, nlen)))
			goto error0;
	}
	 
	else if (haveright) {
		 
		if ((error = xfs_alloc_lookup_eq(cnt_cur, gtbno, gtlen, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if ((error = xfs_btree_delete(cnt_cur, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		 
		nbno = bno;
		nlen = len + gtlen;
		if ((error = xfs_alloc_update(bno_cur, nbno, nlen)))
			goto error0;
	}
	 
	else {
		nbno = bno;
		nlen = len;
		if ((error = xfs_btree_insert(bno_cur, &i)))
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
	}
	xfs_btree_del_cursor(bno_cur, XFS_BTREE_NOERROR);
	bno_cur = NULL;
	 
	if ((error = xfs_alloc_lookup_eq(cnt_cur, nbno, nlen, &i)))
		goto error0;
	if (XFS_IS_CORRUPT(mp, i != 0)) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	if ((error = xfs_btree_insert(cnt_cur, &i)))
		goto error0;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	xfs_btree_del_cursor(cnt_cur, XFS_BTREE_NOERROR);
	cnt_cur = NULL;

	 
	error = xfs_alloc_update_counters(tp, agbp, len);
	xfs_ag_resv_free_extent(agbp->b_pag, type, tp, len);
	if (error)
		goto error0;

	XFS_STATS_INC(mp, xs_freex);
	XFS_STATS_ADD(mp, xs_freeb, len);

	trace_xfs_free_extent(mp, agno, bno, len, type, haveleft, haveright);

	return 0;

 error0:
	trace_xfs_free_extent(mp, agno, bno, len, type, -1, -1);
	if (bno_cur)
		xfs_btree_del_cursor(bno_cur, XFS_BTREE_ERROR);
	if (cnt_cur)
		xfs_btree_del_cursor(cnt_cur, XFS_BTREE_ERROR);
	return error;
}

 

 
void
xfs_alloc_compute_maxlevels(
	xfs_mount_t	*mp)	 
{
	mp->m_alloc_maxlevels = xfs_btree_compute_maxlevels(mp->m_alloc_mnr,
			(mp->m_sb.sb_agblocks + 1) / 2);
	ASSERT(mp->m_alloc_maxlevels <= xfs_allocbt_maxlevels_ondisk());
}

 
xfs_extlen_t
xfs_alloc_longest_free_extent(
	struct xfs_perag	*pag,
	xfs_extlen_t		need,
	xfs_extlen_t		reserved)
{
	xfs_extlen_t		delta = 0;

	 
	if (need > pag->pagf_flcount)
		delta = need - pag->pagf_flcount;

	 
	if (pag->pagf_freeblks - pag->pagf_longest < reserved)
		delta += reserved - (pag->pagf_freeblks - pag->pagf_longest);

	 
	if (pag->pagf_longest > delta)
		return min_t(xfs_extlen_t, pag->pag_mount->m_ag_max_usable,
				pag->pagf_longest - delta);

	 
	return pag->pagf_flcount > 0 || pag->pagf_longest > 0;
}

 
unsigned int
xfs_alloc_min_freelist(
	struct xfs_mount	*mp,
	struct xfs_perag	*pag)
{
	 
	static const uint8_t	fake_levels[XFS_BTNUM_AGF] = {1, 1, 1};
	const uint8_t		*levels = pag ? pag->pagf_levels : fake_levels;
	unsigned int		min_free;

	ASSERT(mp->m_alloc_maxlevels > 0);

	 
	min_free = min_t(unsigned int, levels[XFS_BTNUM_BNOi] + 1,
				       mp->m_alloc_maxlevels);
	 
	min_free += min_t(unsigned int, levels[XFS_BTNUM_CNTi] + 1,
				       mp->m_alloc_maxlevels);
	 
	if (xfs_has_rmapbt(mp))
		min_free += min_t(unsigned int, levels[XFS_BTNUM_RMAPi] + 1,
						mp->m_rmap_maxlevels);

	return min_free;
}

 
static bool
xfs_alloc_space_available(
	struct xfs_alloc_arg	*args,
	xfs_extlen_t		min_free,
	int			flags)
{
	struct xfs_perag	*pag = args->pag;
	xfs_extlen_t		alloc_len, longest;
	xfs_extlen_t		reservation;  
	int			available;
	xfs_extlen_t		agflcount;

	if (flags & XFS_ALLOC_FLAG_FREEING)
		return true;

	reservation = xfs_ag_resv_needed(pag, args->resv);

	 
	alloc_len = args->minlen + (args->alignment - 1) + args->minalignslop;
	longest = xfs_alloc_longest_free_extent(pag, min_free, reservation);
	if (longest < alloc_len)
		return false;

	 
	agflcount = min_t(xfs_extlen_t, pag->pagf_flcount, min_free);
	available = (int)(pag->pagf_freeblks + agflcount -
			  reservation - min_free - args->minleft);
	if (available < (int)max(args->total, alloc_len))
		return false;

	 
	if (available < (int)args->maxlen && !(flags & XFS_ALLOC_FLAG_CHECK)) {
		args->maxlen = available;
		ASSERT(args->maxlen > 0);
		ASSERT(args->maxlen >= args->minlen);
	}

	return true;
}

int
xfs_free_agfl_block(
	struct xfs_trans	*tp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		agbno,
	struct xfs_buf		*agbp,
	struct xfs_owner_info	*oinfo)
{
	int			error;
	struct xfs_buf		*bp;

	error = xfs_free_ag_extent(tp, agbp, agno, agbno, 1, oinfo,
				   XFS_AG_RESV_AGFL);
	if (error)
		return error;

	error = xfs_trans_get_buf(tp, tp->t_mountp->m_ddev_targp,
			XFS_AGB_TO_DADDR(tp->t_mountp, agno, agbno),
			tp->t_mountp->m_bsize, 0, &bp);
	if (error)
		return error;
	xfs_trans_binval(tp, bp);

	return 0;
}

 
static bool
xfs_agfl_needs_reset(
	struct xfs_mount	*mp,
	struct xfs_agf		*agf)
{
	uint32_t		f = be32_to_cpu(agf->agf_flfirst);
	uint32_t		l = be32_to_cpu(agf->agf_fllast);
	uint32_t		c = be32_to_cpu(agf->agf_flcount);
	int			agfl_size = xfs_agfl_size(mp);
	int			active;

	 
	if (f >= agfl_size || l >= agfl_size)
		return true;
	if (c > agfl_size)
		return true;

	 
	if (c && l >= f)
		active = l - f + 1;
	else if (c)
		active = agfl_size - f + l + 1;
	else
		active = 0;

	return active != c;
}

 
static void
xfs_agfl_reset(
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	struct xfs_perag	*pag)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_agf		*agf = agbp->b_addr;

	ASSERT(xfs_perag_agfl_needs_reset(pag));
	trace_xfs_agfl_reset(mp, agf, 0, _RET_IP_);

	xfs_warn(mp,
	       "WARNING: Reset corrupted AGFL on AG %u. %d blocks leaked. "
	       "Please unmount and run xfs_repair.",
	         pag->pag_agno, pag->pagf_flcount);

	agf->agf_flfirst = 0;
	agf->agf_fllast = cpu_to_be32(xfs_agfl_size(mp) - 1);
	agf->agf_flcount = 0;
	xfs_alloc_log_agf(tp, agbp, XFS_AGF_FLFIRST | XFS_AGF_FLLAST |
				    XFS_AGF_FLCOUNT);

	pag->pagf_flcount = 0;
	clear_bit(XFS_AGSTATE_AGFL_NEEDS_RESET, &pag->pag_opstate);
}

 
static int
xfs_defer_agfl_block(
	struct xfs_trans		*tp,
	xfs_agnumber_t			agno,
	xfs_agblock_t			agbno,
	struct xfs_owner_info		*oinfo)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_extent_free_item	*xefi;
	xfs_fsblock_t			fsbno = XFS_AGB_TO_FSB(mp, agno, agbno);

	ASSERT(xfs_extfree_item_cache != NULL);
	ASSERT(oinfo != NULL);

	if (XFS_IS_CORRUPT(mp, !xfs_verify_fsbno(mp, fsbno)))
		return -EFSCORRUPTED;

	xefi = kmem_cache_zalloc(xfs_extfree_item_cache,
			       GFP_KERNEL | __GFP_NOFAIL);
	xefi->xefi_startblock = fsbno;
	xefi->xefi_blockcount = 1;
	xefi->xefi_owner = oinfo->oi_owner;
	xefi->xefi_agresv = XFS_AG_RESV_AGFL;

	trace_xfs_agfl_free_defer(mp, agno, 0, agbno, 1);

	xfs_extent_free_get_group(mp, xefi);
	xfs_defer_add(tp, XFS_DEFER_OPS_TYPE_AGFL_FREE, &xefi->xefi_list);
	return 0;
}

 
int
__xfs_free_extent_later(
	struct xfs_trans		*tp,
	xfs_fsblock_t			bno,
	xfs_filblks_t			len,
	const struct xfs_owner_info	*oinfo,
	enum xfs_ag_resv_type		type,
	bool				skip_discard)
{
	struct xfs_extent_free_item	*xefi;
	struct xfs_mount		*mp = tp->t_mountp;
#ifdef DEBUG
	xfs_agnumber_t			agno;
	xfs_agblock_t			agbno;

	ASSERT(bno != NULLFSBLOCK);
	ASSERT(len > 0);
	ASSERT(len <= XFS_MAX_BMBT_EXTLEN);
	ASSERT(!isnullstartblock(bno));
	agno = XFS_FSB_TO_AGNO(mp, bno);
	agbno = XFS_FSB_TO_AGBNO(mp, bno);
	ASSERT(agno < mp->m_sb.sb_agcount);
	ASSERT(agbno < mp->m_sb.sb_agblocks);
	ASSERT(len < mp->m_sb.sb_agblocks);
	ASSERT(agbno + len <= mp->m_sb.sb_agblocks);
#endif
	ASSERT(xfs_extfree_item_cache != NULL);
	ASSERT(type != XFS_AG_RESV_AGFL);

	if (XFS_IS_CORRUPT(mp, !xfs_verify_fsbext(mp, bno, len)))
		return -EFSCORRUPTED;

	xefi = kmem_cache_zalloc(xfs_extfree_item_cache,
			       GFP_KERNEL | __GFP_NOFAIL);
	xefi->xefi_startblock = bno;
	xefi->xefi_blockcount = (xfs_extlen_t)len;
	xefi->xefi_agresv = type;
	if (skip_discard)
		xefi->xefi_flags |= XFS_EFI_SKIP_DISCARD;
	if (oinfo) {
		ASSERT(oinfo->oi_offset == 0);

		if (oinfo->oi_flags & XFS_OWNER_INFO_ATTR_FORK)
			xefi->xefi_flags |= XFS_EFI_ATTR_FORK;
		if (oinfo->oi_flags & XFS_OWNER_INFO_BMBT_BLOCK)
			xefi->xefi_flags |= XFS_EFI_BMBT_BLOCK;
		xefi->xefi_owner = oinfo->oi_owner;
	} else {
		xefi->xefi_owner = XFS_RMAP_OWN_NULL;
	}
	trace_xfs_bmap_free_defer(mp,
			XFS_FSB_TO_AGNO(tp->t_mountp, bno), 0,
			XFS_FSB_TO_AGBNO(tp->t_mountp, bno), len);

	xfs_extent_free_get_group(mp, xefi);
	xfs_defer_add(tp, XFS_DEFER_OPS_TYPE_FREE, &xefi->xefi_list);
	return 0;
}

#ifdef DEBUG
 
STATIC int
xfs_exact_minlen_extent_available(
	struct xfs_alloc_arg	*args,
	struct xfs_buf		*agbp,
	int			*stat)
{
	struct xfs_btree_cur	*cnt_cur;
	xfs_agblock_t		fbno;
	xfs_extlen_t		flen;
	int			error = 0;

	cnt_cur = xfs_allocbt_init_cursor(args->mp, args->tp, agbp,
					args->pag, XFS_BTNUM_CNT);
	error = xfs_alloc_lookup_ge(cnt_cur, 0, args->minlen, stat);
	if (error)
		goto out;

	if (*stat == 0) {
		error = -EFSCORRUPTED;
		goto out;
	}

	error = xfs_alloc_get_rec(cnt_cur, &fbno, &flen, stat);
	if (error)
		goto out;

	if (*stat == 1 && flen != args->minlen)
		*stat = 0;

out:
	xfs_btree_del_cursor(cnt_cur, error);

	return error;
}
#endif

 
int			 
xfs_alloc_fix_freelist(
	struct xfs_alloc_arg	*args,	 
	uint32_t		alloc_flags)
{
	struct xfs_mount	*mp = args->mp;
	struct xfs_perag	*pag = args->pag;
	struct xfs_trans	*tp = args->tp;
	struct xfs_buf		*agbp = NULL;
	struct xfs_buf		*agflbp = NULL;
	struct xfs_alloc_arg	targs;	 
	xfs_agblock_t		bno;	 
	xfs_extlen_t		need;	 
	int			error = 0;

	 
	ASSERT(tp->t_flags & XFS_TRANS_PERM_LOG_RES);

	if (!xfs_perag_initialised_agf(pag)) {
		error = xfs_alloc_read_agf(pag, tp, alloc_flags, &agbp);
		if (error) {
			 
			if (error == -EAGAIN)
				error = 0;
			goto out_no_agbp;
		}
	}

	 
	if (xfs_perag_prefers_metadata(pag) &&
	    (args->datatype & XFS_ALLOC_USERDATA) &&
	    (alloc_flags & XFS_ALLOC_FLAG_TRYLOCK)) {
		ASSERT(!(alloc_flags & XFS_ALLOC_FLAG_FREEING));
		goto out_agbp_relse;
	}

	need = xfs_alloc_min_freelist(mp, pag);
	if (!xfs_alloc_space_available(args, need, alloc_flags |
			XFS_ALLOC_FLAG_CHECK))
		goto out_agbp_relse;

	 
	if (!agbp) {
		error = xfs_alloc_read_agf(pag, tp, alloc_flags, &agbp);
		if (error) {
			 
			if (error == -EAGAIN)
				error = 0;
			goto out_no_agbp;
		}
	}

	 
	if (xfs_perag_agfl_needs_reset(pag))
		xfs_agfl_reset(tp, agbp, pag);

	 
	need = xfs_alloc_min_freelist(mp, pag);
	if (!xfs_alloc_space_available(args, need, alloc_flags))
		goto out_agbp_relse;

#ifdef DEBUG
	if (args->alloc_minlen_only) {
		int stat;

		error = xfs_exact_minlen_extent_available(args, agbp, &stat);
		if (error || !stat)
			goto out_agbp_relse;
	}
#endif
	 
	memset(&targs, 0, sizeof(targs));
	 
	if (alloc_flags & XFS_ALLOC_FLAG_NORMAP)
		targs.oinfo = XFS_RMAP_OINFO_SKIP_UPDATE;
	else
		targs.oinfo = XFS_RMAP_OINFO_AG;
	while (!(alloc_flags & XFS_ALLOC_FLAG_NOSHRINK) &&
			pag->pagf_flcount > need) {
		error = xfs_alloc_get_freelist(pag, tp, agbp, &bno, 0);
		if (error)
			goto out_agbp_relse;

		 
		error = xfs_defer_agfl_block(tp, args->agno, bno, &targs.oinfo);
		if (error)
			goto out_agbp_relse;
	}

	targs.tp = tp;
	targs.mp = mp;
	targs.agbp = agbp;
	targs.agno = args->agno;
	targs.alignment = targs.minlen = targs.prod = 1;
	targs.pag = pag;
	error = xfs_alloc_read_agfl(pag, tp, &agflbp);
	if (error)
		goto out_agbp_relse;

	 
	while (pag->pagf_flcount < need) {
		targs.agbno = 0;
		targs.maxlen = need - pag->pagf_flcount;
		targs.resv = XFS_AG_RESV_AGFL;

		 
		error = xfs_alloc_ag_vextent_size(&targs, alloc_flags);
		if (error)
			goto out_agflbp_relse;

		 
		if (targs.agbno == NULLAGBLOCK) {
			if (alloc_flags & XFS_ALLOC_FLAG_FREEING)
				break;
			goto out_agflbp_relse;
		}

		if (!xfs_rmap_should_skip_owner_update(&targs.oinfo)) {
			error = xfs_rmap_alloc(tp, agbp, pag,
				       targs.agbno, targs.len, &targs.oinfo);
			if (error)
				goto out_agflbp_relse;
		}
		error = xfs_alloc_update_counters(tp, agbp,
						  -((long)(targs.len)));
		if (error)
			goto out_agflbp_relse;

		 
		for (bno = targs.agbno; bno < targs.agbno + targs.len; bno++) {
			error = xfs_alloc_put_freelist(pag, tp, agbp,
							agflbp, bno, 0);
			if (error)
				goto out_agflbp_relse;
		}
	}
	xfs_trans_brelse(tp, agflbp);
	args->agbp = agbp;
	return 0;

out_agflbp_relse:
	xfs_trans_brelse(tp, agflbp);
out_agbp_relse:
	if (agbp)
		xfs_trans_brelse(tp, agbp);
out_no_agbp:
	args->agbp = NULL;
	return error;
}

 
int
xfs_alloc_get_freelist(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	xfs_agblock_t		*bnop,
	int			btreeblk)
{
	struct xfs_agf		*agf = agbp->b_addr;
	struct xfs_buf		*agflbp;
	xfs_agblock_t		bno;
	__be32			*agfl_bno;
	int			error;
	uint32_t		logflags;
	struct xfs_mount	*mp = tp->t_mountp;

	 
	if (!agf->agf_flcount) {
		*bnop = NULLAGBLOCK;
		return 0;
	}
	 
	error = xfs_alloc_read_agfl(pag, tp, &agflbp);
	if (error)
		return error;


	 
	agfl_bno = xfs_buf_to_agfl_bno(agflbp);
	bno = be32_to_cpu(agfl_bno[be32_to_cpu(agf->agf_flfirst)]);
	if (XFS_IS_CORRUPT(tp->t_mountp, !xfs_verify_agbno(pag, bno)))
		return -EFSCORRUPTED;

	be32_add_cpu(&agf->agf_flfirst, 1);
	xfs_trans_brelse(tp, agflbp);
	if (be32_to_cpu(agf->agf_flfirst) == xfs_agfl_size(mp))
		agf->agf_flfirst = 0;

	ASSERT(!xfs_perag_agfl_needs_reset(pag));
	be32_add_cpu(&agf->agf_flcount, -1);
	pag->pagf_flcount--;

	logflags = XFS_AGF_FLFIRST | XFS_AGF_FLCOUNT;
	if (btreeblk) {
		be32_add_cpu(&agf->agf_btreeblks, 1);
		pag->pagf_btreeblks++;
		logflags |= XFS_AGF_BTREEBLKS;
	}

	xfs_alloc_log_agf(tp, agbp, logflags);
	*bnop = bno;

	return 0;
}

 
void
xfs_alloc_log_agf(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp,
	uint32_t		fields)
{
	int	first;		 
	int	last;		 
	static const short	offsets[] = {
		offsetof(xfs_agf_t, agf_magicnum),
		offsetof(xfs_agf_t, agf_versionnum),
		offsetof(xfs_agf_t, agf_seqno),
		offsetof(xfs_agf_t, agf_length),
		offsetof(xfs_agf_t, agf_roots[0]),
		offsetof(xfs_agf_t, agf_levels[0]),
		offsetof(xfs_agf_t, agf_flfirst),
		offsetof(xfs_agf_t, agf_fllast),
		offsetof(xfs_agf_t, agf_flcount),
		offsetof(xfs_agf_t, agf_freeblks),
		offsetof(xfs_agf_t, agf_longest),
		offsetof(xfs_agf_t, agf_btreeblks),
		offsetof(xfs_agf_t, agf_uuid),
		offsetof(xfs_agf_t, agf_rmap_blocks),
		offsetof(xfs_agf_t, agf_refcount_blocks),
		offsetof(xfs_agf_t, agf_refcount_root),
		offsetof(xfs_agf_t, agf_refcount_level),
		 
		offsetof(xfs_agf_t, agf_spare64),
		sizeof(xfs_agf_t)
	};

	trace_xfs_agf(tp->t_mountp, bp->b_addr, fields, _RET_IP_);

	xfs_trans_buf_set_type(tp, bp, XFS_BLFT_AGF_BUF);

	xfs_btree_offsets(fields, offsets, XFS_AGF_NUM_BITS, &first, &last);
	xfs_trans_log_buf(tp, bp, (uint)first, (uint)last);
}

 
int
xfs_alloc_put_freelist(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	struct xfs_buf		*agflbp,
	xfs_agblock_t		bno,
	int			btreeblk)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_agf		*agf = agbp->b_addr;
	__be32			*blockp;
	int			error;
	uint32_t		logflags;
	__be32			*agfl_bno;
	int			startoff;

	if (!agflbp) {
		error = xfs_alloc_read_agfl(pag, tp, &agflbp);
		if (error)
			return error;
	}

	be32_add_cpu(&agf->agf_fllast, 1);
	if (be32_to_cpu(agf->agf_fllast) == xfs_agfl_size(mp))
		agf->agf_fllast = 0;

	ASSERT(!xfs_perag_agfl_needs_reset(pag));
	be32_add_cpu(&agf->agf_flcount, 1);
	pag->pagf_flcount++;

	logflags = XFS_AGF_FLLAST | XFS_AGF_FLCOUNT;
	if (btreeblk) {
		be32_add_cpu(&agf->agf_btreeblks, -1);
		pag->pagf_btreeblks--;
		logflags |= XFS_AGF_BTREEBLKS;
	}

	xfs_alloc_log_agf(tp, agbp, logflags);

	ASSERT(be32_to_cpu(agf->agf_flcount) <= xfs_agfl_size(mp));

	agfl_bno = xfs_buf_to_agfl_bno(agflbp);
	blockp = &agfl_bno[be32_to_cpu(agf->agf_fllast)];
	*blockp = cpu_to_be32(bno);
	startoff = (char *)blockp - (char *)agflbp->b_addr;

	xfs_alloc_log_agf(tp, agbp, logflags);

	xfs_trans_buf_set_type(tp, agflbp, XFS_BLFT_AGFL_BUF);
	xfs_trans_log_buf(tp, agflbp, startoff,
			  startoff + sizeof(xfs_agblock_t) - 1);
	return 0;
}

 
xfs_failaddr_t
xfs_validate_ag_length(
	struct xfs_buf		*bp,
	uint32_t		seqno,
	uint32_t		length)
{
	struct xfs_mount	*mp = bp->b_mount;
	 
	if (bp->b_pag && seqno != bp->b_pag->pag_agno)
		return __this_address;

	 
	if (length != mp->m_sb.sb_agblocks) {
		 
		if (bp->b_pag && seqno != mp->m_sb.sb_agcount - 1)
			return __this_address;
		if (length < XFS_MIN_AG_BLOCKS)
			return __this_address;
		if (length > mp->m_sb.sb_agblocks)
			return __this_address;
	}

	return NULL;
}

 
static xfs_failaddr_t
xfs_agf_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_agf		*agf = bp->b_addr;
	xfs_failaddr_t		fa;
	uint32_t		agf_seqno = be32_to_cpu(agf->agf_seqno);
	uint32_t		agf_length = be32_to_cpu(agf->agf_length);

	if (xfs_has_crc(mp)) {
		if (!uuid_equal(&agf->agf_uuid, &mp->m_sb.sb_meta_uuid))
			return __this_address;
		if (!xfs_log_check_lsn(mp, be64_to_cpu(agf->agf_lsn)))
			return __this_address;
	}

	if (!xfs_verify_magic(bp, agf->agf_magicnum))
		return __this_address;

	if (!XFS_AGF_GOOD_VERSION(be32_to_cpu(agf->agf_versionnum)))
		return __this_address;

	 
	fa = xfs_validate_ag_length(bp, agf_seqno, agf_length);
	if (fa)
		return fa;

	if (be32_to_cpu(agf->agf_flfirst) >= xfs_agfl_size(mp))
		return __this_address;
	if (be32_to_cpu(agf->agf_fllast) >= xfs_agfl_size(mp))
		return __this_address;
	if (be32_to_cpu(agf->agf_flcount) > xfs_agfl_size(mp))
		return __this_address;

	if (be32_to_cpu(agf->agf_freeblks) < be32_to_cpu(agf->agf_longest) ||
	    be32_to_cpu(agf->agf_freeblks) > agf_length)
		return __this_address;

	if (be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNO]) < 1 ||
	    be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNT]) < 1 ||
	    be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNO]) >
						mp->m_alloc_maxlevels ||
	    be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNT]) >
						mp->m_alloc_maxlevels)
		return __this_address;

	if (xfs_has_lazysbcount(mp) &&
	    be32_to_cpu(agf->agf_btreeblks) > agf_length)
		return __this_address;

	if (xfs_has_rmapbt(mp)) {
		if (be32_to_cpu(agf->agf_rmap_blocks) > agf_length)
			return __this_address;

		if (be32_to_cpu(agf->agf_levels[XFS_BTNUM_RMAP]) < 1 ||
		    be32_to_cpu(agf->agf_levels[XFS_BTNUM_RMAP]) >
							mp->m_rmap_maxlevels)
			return __this_address;
	}

	if (xfs_has_reflink(mp)) {
		if (be32_to_cpu(agf->agf_refcount_blocks) > agf_length)
			return __this_address;

		if (be32_to_cpu(agf->agf_refcount_level) < 1 ||
		    be32_to_cpu(agf->agf_refcount_level) > mp->m_refc_maxlevels)
			return __this_address;
	}

	return NULL;
}

static void
xfs_agf_read_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_mount;
	xfs_failaddr_t	fa;

	if (xfs_has_crc(mp) &&
	    !xfs_buf_verify_cksum(bp, XFS_AGF_CRC_OFF))
		xfs_verifier_error(bp, -EFSBADCRC, __this_address);
	else {
		fa = xfs_agf_verify(bp);
		if (XFS_TEST_ERROR(fa, mp, XFS_ERRTAG_ALLOC_READ_AGF))
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
	}
}

static void
xfs_agf_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	struct xfs_agf		*agf = bp->b_addr;
	xfs_failaddr_t		fa;

	fa = xfs_agf_verify(bp);
	if (fa) {
		xfs_verifier_error(bp, -EFSCORRUPTED, fa);
		return;
	}

	if (!xfs_has_crc(mp))
		return;

	if (bip)
		agf->agf_lsn = cpu_to_be64(bip->bli_item.li_lsn);

	xfs_buf_update_cksum(bp, XFS_AGF_CRC_OFF);
}

const struct xfs_buf_ops xfs_agf_buf_ops = {
	.name = "xfs_agf",
	.magic = { cpu_to_be32(XFS_AGF_MAGIC), cpu_to_be32(XFS_AGF_MAGIC) },
	.verify_read = xfs_agf_read_verify,
	.verify_write = xfs_agf_write_verify,
	.verify_struct = xfs_agf_verify,
};

 
int
xfs_read_agf(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	int			flags,
	struct xfs_buf		**agfbpp)
{
	struct xfs_mount	*mp = pag->pag_mount;
	int			error;

	trace_xfs_read_agf(pag->pag_mount, pag->pag_agno);

	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, pag->pag_agno, XFS_AGF_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), flags, agfbpp, &xfs_agf_buf_ops);
	if (error)
		return error;

	xfs_buf_set_ref(*agfbpp, XFS_AGF_REF);
	return 0;
}

 
int
xfs_alloc_read_agf(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	int			flags,
	struct xfs_buf		**agfbpp)
{
	struct xfs_buf		*agfbp;
	struct xfs_agf		*agf;
	int			error;
	int			allocbt_blks;

	trace_xfs_alloc_read_agf(pag->pag_mount, pag->pag_agno);

	 
	ASSERT((flags & (XFS_ALLOC_FLAG_FREEING | XFS_ALLOC_FLAG_TRYLOCK)) !=
			(XFS_ALLOC_FLAG_FREEING | XFS_ALLOC_FLAG_TRYLOCK));
	error = xfs_read_agf(pag, tp,
			(flags & XFS_ALLOC_FLAG_TRYLOCK) ? XBF_TRYLOCK : 0,
			&agfbp);
	if (error)
		return error;

	agf = agfbp->b_addr;
	if (!xfs_perag_initialised_agf(pag)) {
		pag->pagf_freeblks = be32_to_cpu(agf->agf_freeblks);
		pag->pagf_btreeblks = be32_to_cpu(agf->agf_btreeblks);
		pag->pagf_flcount = be32_to_cpu(agf->agf_flcount);
		pag->pagf_longest = be32_to_cpu(agf->agf_longest);
		pag->pagf_levels[XFS_BTNUM_BNOi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNOi]);
		pag->pagf_levels[XFS_BTNUM_CNTi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNTi]);
		pag->pagf_levels[XFS_BTNUM_RMAPi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_RMAPi]);
		pag->pagf_refcount_level = be32_to_cpu(agf->agf_refcount_level);
		if (xfs_agfl_needs_reset(pag->pag_mount, agf))
			set_bit(XFS_AGSTATE_AGFL_NEEDS_RESET, &pag->pag_opstate);
		else
			clear_bit(XFS_AGSTATE_AGFL_NEEDS_RESET, &pag->pag_opstate);

		 
		allocbt_blks = pag->pagf_btreeblks;
		if (xfs_has_rmapbt(pag->pag_mount))
			allocbt_blks -= be32_to_cpu(agf->agf_rmap_blocks) - 1;
		if (allocbt_blks > 0)
			atomic64_add(allocbt_blks,
					&pag->pag_mount->m_allocbt_blks);

		set_bit(XFS_AGSTATE_AGF_INIT, &pag->pag_opstate);
	}
#ifdef DEBUG
	else if (!xfs_is_shutdown(pag->pag_mount)) {
		ASSERT(pag->pagf_freeblks == be32_to_cpu(agf->agf_freeblks));
		ASSERT(pag->pagf_btreeblks == be32_to_cpu(agf->agf_btreeblks));
		ASSERT(pag->pagf_flcount == be32_to_cpu(agf->agf_flcount));
		ASSERT(pag->pagf_longest == be32_to_cpu(agf->agf_longest));
		ASSERT(pag->pagf_levels[XFS_BTNUM_BNOi] ==
		       be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNOi]));
		ASSERT(pag->pagf_levels[XFS_BTNUM_CNTi] ==
		       be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNTi]));
	}
#endif
	if (agfbpp)
		*agfbpp = agfbp;
	else
		xfs_trans_brelse(tp, agfbp);
	return 0;
}

 
static int
xfs_alloc_vextent_check_args(
	struct xfs_alloc_arg	*args,
	xfs_fsblock_t		target,
	xfs_agnumber_t		*minimum_agno)
{
	struct xfs_mount	*mp = args->mp;
	xfs_agblock_t		agsize;

	args->fsbno = NULLFSBLOCK;

	*minimum_agno = 0;
	if (args->tp->t_highest_agno != NULLAGNUMBER)
		*minimum_agno = args->tp->t_highest_agno;

	 
	agsize = mp->m_sb.sb_agblocks;
	if (args->maxlen > agsize)
		args->maxlen = agsize;
	if (args->alignment == 0)
		args->alignment = 1;

	ASSERT(args->minlen > 0);
	ASSERT(args->maxlen > 0);
	ASSERT(args->alignment > 0);
	ASSERT(args->resv != XFS_AG_RESV_AGFL);

	ASSERT(XFS_FSB_TO_AGNO(mp, target) < mp->m_sb.sb_agcount);
	ASSERT(XFS_FSB_TO_AGBNO(mp, target) < agsize);
	ASSERT(args->minlen <= args->maxlen);
	ASSERT(args->minlen <= agsize);
	ASSERT(args->mod < args->prod);

	if (XFS_FSB_TO_AGNO(mp, target) >= mp->m_sb.sb_agcount ||
	    XFS_FSB_TO_AGBNO(mp, target) >= agsize ||
	    args->minlen > args->maxlen || args->minlen > agsize ||
	    args->mod >= args->prod) {
		trace_xfs_alloc_vextent_badargs(args);
		return -ENOSPC;
	}

	if (args->agno != NULLAGNUMBER && *minimum_agno > args->agno) {
		trace_xfs_alloc_vextent_skip_deadlock(args);
		return -ENOSPC;
	}
	return 0;

}

 
static int
xfs_alloc_vextent_prepare_ag(
	struct xfs_alloc_arg	*args,
	uint32_t		alloc_flags)
{
	bool			need_pag = !args->pag;
	int			error;

	if (need_pag)
		args->pag = xfs_perag_get(args->mp, args->agno);

	args->agbp = NULL;
	error = xfs_alloc_fix_freelist(args, alloc_flags);
	if (error) {
		trace_xfs_alloc_vextent_nofix(args);
		if (need_pag)
			xfs_perag_put(args->pag);
		args->agbno = NULLAGBLOCK;
		return error;
	}
	if (!args->agbp) {
		 
		trace_xfs_alloc_vextent_noagbp(args);
		args->agbno = NULLAGBLOCK;
		return 0;
	}
	args->wasfromfl = 0;
	return 0;
}

 
static int
xfs_alloc_vextent_finish(
	struct xfs_alloc_arg	*args,
	xfs_agnumber_t		minimum_agno,
	int			alloc_error,
	bool			drop_perag)
{
	struct xfs_mount	*mp = args->mp;
	int			error = 0;

	 
	if (args->agbp &&
	    (args->tp->t_highest_agno == NULLAGNUMBER ||
	     args->agno > minimum_agno))
		args->tp->t_highest_agno = args->agno;

	 
	if (alloc_error || args->agbno == NULLAGBLOCK) {
		args->fsbno = NULLFSBLOCK;
		error = alloc_error;
		goto out_drop_perag;
	}

	args->fsbno = XFS_AGB_TO_FSB(mp, args->agno, args->agbno);

	ASSERT(args->len >= args->minlen);
	ASSERT(args->len <= args->maxlen);
	ASSERT(args->agbno % args->alignment == 0);
	XFS_AG_CHECK_DADDR(mp, XFS_FSB_TO_DADDR(mp, args->fsbno), args->len);

	 
	if (!xfs_rmap_should_skip_owner_update(&args->oinfo)) {
		error = xfs_rmap_alloc(args->tp, args->agbp, args->pag,
				       args->agbno, args->len, &args->oinfo);
		if (error)
			goto out_drop_perag;
	}

	if (!args->wasfromfl) {
		error = xfs_alloc_update_counters(args->tp, args->agbp,
						  -((long)(args->len)));
		if (error)
			goto out_drop_perag;

		ASSERT(!xfs_extent_busy_search(mp, args->pag, args->agbno,
				args->len));
	}

	xfs_ag_resv_alloc_extent(args->pag, args->resv, args);

	XFS_STATS_INC(mp, xs_allocx);
	XFS_STATS_ADD(mp, xs_allocb, args->len);

	trace_xfs_alloc_vextent_finish(args);

out_drop_perag:
	if (drop_perag && args->pag) {
		xfs_perag_rele(args->pag);
		args->pag = NULL;
	}
	return error;
}

 
int
xfs_alloc_vextent_this_ag(
	struct xfs_alloc_arg	*args,
	xfs_agnumber_t		agno)
{
	struct xfs_mount	*mp = args->mp;
	xfs_agnumber_t		minimum_agno;
	uint32_t		alloc_flags = 0;
	int			error;

	ASSERT(args->pag != NULL);
	ASSERT(args->pag->pag_agno == agno);

	args->agno = agno;
	args->agbno = 0;

	trace_xfs_alloc_vextent_this_ag(args);

	error = xfs_alloc_vextent_check_args(args, XFS_AGB_TO_FSB(mp, agno, 0),
			&minimum_agno);
	if (error) {
		if (error == -ENOSPC)
			return 0;
		return error;
	}

	error = xfs_alloc_vextent_prepare_ag(args, alloc_flags);
	if (!error && args->agbp)
		error = xfs_alloc_ag_vextent_size(args, alloc_flags);

	return xfs_alloc_vextent_finish(args, minimum_agno, error, false);
}

 
static int
xfs_alloc_vextent_iterate_ags(
	struct xfs_alloc_arg	*args,
	xfs_agnumber_t		minimum_agno,
	xfs_agnumber_t		start_agno,
	xfs_agblock_t		target_agbno,
	uint32_t		alloc_flags)
{
	struct xfs_mount	*mp = args->mp;
	xfs_agnumber_t		restart_agno = minimum_agno;
	xfs_agnumber_t		agno;
	int			error = 0;

	if (alloc_flags & XFS_ALLOC_FLAG_TRYLOCK)
		restart_agno = 0;
restart:
	for_each_perag_wrap_range(mp, start_agno, restart_agno,
			mp->m_sb.sb_agcount, agno, args->pag) {
		args->agno = agno;
		error = xfs_alloc_vextent_prepare_ag(args, alloc_flags);
		if (error)
			break;
		if (!args->agbp) {
			trace_xfs_alloc_vextent_loopfailed(args);
			continue;
		}

		 
		if (args->agno == start_agno && target_agbno) {
			args->agbno = target_agbno;
			error = xfs_alloc_ag_vextent_near(args, alloc_flags);
		} else {
			args->agbno = 0;
			error = xfs_alloc_ag_vextent_size(args, alloc_flags);
		}
		break;
	}
	if (error) {
		xfs_perag_rele(args->pag);
		args->pag = NULL;
		return error;
	}
	if (args->agbp)
		return 0;

	 
	if (alloc_flags & XFS_ALLOC_FLAG_TRYLOCK) {
		alloc_flags &= ~XFS_ALLOC_FLAG_TRYLOCK;
		restart_agno = minimum_agno;
		goto restart;
	}

	ASSERT(args->pag == NULL);
	trace_xfs_alloc_vextent_allfailed(args);
	return 0;
}

 
int
xfs_alloc_vextent_start_ag(
	struct xfs_alloc_arg	*args,
	xfs_fsblock_t		target)
{
	struct xfs_mount	*mp = args->mp;
	xfs_agnumber_t		minimum_agno;
	xfs_agnumber_t		start_agno;
	xfs_agnumber_t		rotorstep = xfs_rotorstep;
	bool			bump_rotor = false;
	uint32_t		alloc_flags = XFS_ALLOC_FLAG_TRYLOCK;
	int			error;

	ASSERT(args->pag == NULL);

	args->agno = NULLAGNUMBER;
	args->agbno = NULLAGBLOCK;

	trace_xfs_alloc_vextent_start_ag(args);

	error = xfs_alloc_vextent_check_args(args, target, &minimum_agno);
	if (error) {
		if (error == -ENOSPC)
			return 0;
		return error;
	}

	if ((args->datatype & XFS_ALLOC_INITIAL_USER_DATA) &&
	    xfs_is_inode32(mp)) {
		target = XFS_AGB_TO_FSB(mp,
				((mp->m_agfrotor / rotorstep) %
				mp->m_sb.sb_agcount), 0);
		bump_rotor = 1;
	}

	start_agno = max(minimum_agno, XFS_FSB_TO_AGNO(mp, target));
	error = xfs_alloc_vextent_iterate_ags(args, minimum_agno, start_agno,
			XFS_FSB_TO_AGBNO(mp, target), alloc_flags);

	if (bump_rotor) {
		if (args->agno == start_agno)
			mp->m_agfrotor = (mp->m_agfrotor + 1) %
				(mp->m_sb.sb_agcount * rotorstep);
		else
			mp->m_agfrotor = (args->agno * rotorstep + 1) %
				(mp->m_sb.sb_agcount * rotorstep);
	}

	return xfs_alloc_vextent_finish(args, minimum_agno, error, true);
}

 
int
xfs_alloc_vextent_first_ag(
	struct xfs_alloc_arg	*args,
	xfs_fsblock_t		target)
 {
	struct xfs_mount	*mp = args->mp;
	xfs_agnumber_t		minimum_agno;
	xfs_agnumber_t		start_agno;
	uint32_t		alloc_flags = XFS_ALLOC_FLAG_TRYLOCK;
	int			error;

	ASSERT(args->pag == NULL);

	args->agno = NULLAGNUMBER;
	args->agbno = NULLAGBLOCK;

	trace_xfs_alloc_vextent_first_ag(args);

	error = xfs_alloc_vextent_check_args(args, target, &minimum_agno);
	if (error) {
		if (error == -ENOSPC)
			return 0;
		return error;
	}

	start_agno = max(minimum_agno, XFS_FSB_TO_AGNO(mp, target));
	error = xfs_alloc_vextent_iterate_ags(args, minimum_agno, start_agno,
			XFS_FSB_TO_AGBNO(mp, target), alloc_flags);
	return xfs_alloc_vextent_finish(args, minimum_agno, error, true);
}

 
int
xfs_alloc_vextent_exact_bno(
	struct xfs_alloc_arg	*args,
	xfs_fsblock_t		target)
{
	struct xfs_mount	*mp = args->mp;
	xfs_agnumber_t		minimum_agno;
	int			error;

	ASSERT(args->pag != NULL);
	ASSERT(args->pag->pag_agno == XFS_FSB_TO_AGNO(mp, target));

	args->agno = XFS_FSB_TO_AGNO(mp, target);
	args->agbno = XFS_FSB_TO_AGBNO(mp, target);

	trace_xfs_alloc_vextent_exact_bno(args);

	error = xfs_alloc_vextent_check_args(args, target, &minimum_agno);
	if (error) {
		if (error == -ENOSPC)
			return 0;
		return error;
	}

	error = xfs_alloc_vextent_prepare_ag(args, 0);
	if (!error && args->agbp)
		error = xfs_alloc_ag_vextent_exact(args);

	return xfs_alloc_vextent_finish(args, minimum_agno, error, false);
}

 
int
xfs_alloc_vextent_near_bno(
	struct xfs_alloc_arg	*args,
	xfs_fsblock_t		target)
{
	struct xfs_mount	*mp = args->mp;
	xfs_agnumber_t		minimum_agno;
	bool			needs_perag = args->pag == NULL;
	uint32_t		alloc_flags = 0;
	int			error;

	if (!needs_perag)
		ASSERT(args->pag->pag_agno == XFS_FSB_TO_AGNO(mp, target));

	args->agno = XFS_FSB_TO_AGNO(mp, target);
	args->agbno = XFS_FSB_TO_AGBNO(mp, target);

	trace_xfs_alloc_vextent_near_bno(args);

	error = xfs_alloc_vextent_check_args(args, target, &minimum_agno);
	if (error) {
		if (error == -ENOSPC)
			return 0;
		return error;
	}

	if (needs_perag)
		args->pag = xfs_perag_grab(mp, args->agno);

	error = xfs_alloc_vextent_prepare_ag(args, alloc_flags);
	if (!error && args->agbp)
		error = xfs_alloc_ag_vextent_near(args, alloc_flags);

	return xfs_alloc_vextent_finish(args, minimum_agno, error, needs_perag);
}

 
int
xfs_free_extent_fix_freelist(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	struct xfs_buf		**agbp)
{
	struct xfs_alloc_arg	args;
	int			error;

	memset(&args, 0, sizeof(struct xfs_alloc_arg));
	args.tp = tp;
	args.mp = tp->t_mountp;
	args.agno = pag->pag_agno;
	args.pag = pag;

	 
	if (args.agno >= args.mp->m_sb.sb_agcount)
		return -EFSCORRUPTED;

	error = xfs_alloc_fix_freelist(&args, XFS_ALLOC_FLAG_FREEING);
	if (error)
		return error;

	*agbp = args.agbp;
	return 0;
}

 
int
__xfs_free_extent(
	struct xfs_trans		*tp,
	struct xfs_perag		*pag,
	xfs_agblock_t			agbno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo,
	enum xfs_ag_resv_type		type,
	bool				skip_discard)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_buf			*agbp;
	struct xfs_agf			*agf;
	int				error;
	unsigned int			busy_flags = 0;

	ASSERT(len != 0);
	ASSERT(type != XFS_AG_RESV_AGFL);

	if (XFS_TEST_ERROR(false, mp,
			XFS_ERRTAG_FREE_EXTENT))
		return -EIO;

	error = xfs_free_extent_fix_freelist(tp, pag, &agbp);
	if (error)
		return error;
	agf = agbp->b_addr;

	if (XFS_IS_CORRUPT(mp, agbno >= mp->m_sb.sb_agblocks)) {
		error = -EFSCORRUPTED;
		goto err_release;
	}

	 
	if (XFS_IS_CORRUPT(mp, agbno + len > be32_to_cpu(agf->agf_length))) {
		error = -EFSCORRUPTED;
		goto err_release;
	}

	error = xfs_free_ag_extent(tp, agbp, pag->pag_agno, agbno, len, oinfo,
			type);
	if (error)
		goto err_release;

	if (skip_discard)
		busy_flags |= XFS_EXTENT_BUSY_SKIP_DISCARD;
	xfs_extent_busy_insert(tp, pag, agbno, len, busy_flags);
	return 0;

err_release:
	xfs_trans_brelse(tp, agbp);
	return error;
}

struct xfs_alloc_query_range_info {
	xfs_alloc_query_range_fn	fn;
	void				*priv;
};

 
STATIC int
xfs_alloc_query_range_helper(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_rec	*rec,
	void				*priv)
{
	struct xfs_alloc_query_range_info	*query = priv;
	struct xfs_alloc_rec_incore		irec;
	xfs_failaddr_t				fa;

	xfs_alloc_btrec_to_irec(rec, &irec);
	fa = xfs_alloc_check_irec(cur, &irec);
	if (fa)
		return xfs_alloc_complain_bad_rec(cur, fa, &irec);

	return query->fn(cur, &irec, query->priv);
}

 
int
xfs_alloc_query_range(
	struct xfs_btree_cur			*cur,
	const struct xfs_alloc_rec_incore	*low_rec,
	const struct xfs_alloc_rec_incore	*high_rec,
	xfs_alloc_query_range_fn		fn,
	void					*priv)
{
	union xfs_btree_irec			low_brec = { .a = *low_rec };
	union xfs_btree_irec			high_brec = { .a = *high_rec };
	struct xfs_alloc_query_range_info	query = { .priv = priv, .fn = fn };

	ASSERT(cur->bc_btnum == XFS_BTNUM_BNO);
	return xfs_btree_query_range(cur, &low_brec, &high_brec,
			xfs_alloc_query_range_helper, &query);
}

 
int
xfs_alloc_query_all(
	struct xfs_btree_cur			*cur,
	xfs_alloc_query_range_fn		fn,
	void					*priv)
{
	struct xfs_alloc_query_range_info	query;

	ASSERT(cur->bc_btnum == XFS_BTNUM_BNO);
	query.priv = priv;
	query.fn = fn;
	return xfs_btree_query_all(cur, xfs_alloc_query_range_helper, &query);
}

 
int
xfs_alloc_has_records(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	enum xbtree_recpacking	*outcome)
{
	union xfs_btree_irec	low;
	union xfs_btree_irec	high;

	memset(&low, 0, sizeof(low));
	low.a.ar_startblock = bno;
	memset(&high, 0xFF, sizeof(high));
	high.a.ar_startblock = bno + len - 1;

	return xfs_btree_has_records(cur, &low, &high, NULL, outcome);
}

 
int
xfs_agfl_walk(
	struct xfs_mount	*mp,
	struct xfs_agf		*agf,
	struct xfs_buf		*agflbp,
	xfs_agfl_walk_fn	walk_fn,
	void			*priv)
{
	__be32			*agfl_bno;
	unsigned int		i;
	int			error;

	agfl_bno = xfs_buf_to_agfl_bno(agflbp);
	i = be32_to_cpu(agf->agf_flfirst);

	 
	if (agf->agf_flcount == cpu_to_be32(0))
		return 0;

	 
	for (;;) {
		error = walk_fn(mp, be32_to_cpu(agfl_bno[i]), priv);
		if (error)
			return error;
		if (i == be32_to_cpu(agf->agf_fllast))
			break;
		if (++i == xfs_agfl_size(mp))
			i = 0;
	}

	return 0;
}

int __init
xfs_extfree_intent_init_cache(void)
{
	xfs_extfree_item_cache = kmem_cache_create("xfs_extfree_intent",
			sizeof(struct xfs_extent_free_item),
			0, 0, NULL);

	return xfs_extfree_item_cache != NULL ? 0 : -ENOMEM;
}

void
xfs_extfree_intent_destroy_cache(void)
{
	kmem_cache_destroy(xfs_extfree_item_cache);
	xfs_extfree_item_cache = NULL;
}

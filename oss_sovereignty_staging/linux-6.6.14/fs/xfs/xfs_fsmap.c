
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_btree.h"
#include "xfs_rmap_btree.h"
#include "xfs_trace.h"
#include "xfs_rmap.h"
#include "xfs_alloc.h"
#include "xfs_bit.h"
#include <linux/fsmap.h>
#include "xfs_fsmap.h"
#include "xfs_refcount.h"
#include "xfs_refcount_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_rtalloc.h"
#include "xfs_ag.h"

 
static void
xfs_fsmap_from_internal(
	struct fsmap		*dest,
	struct xfs_fsmap	*src)
{
	dest->fmr_device = src->fmr_device;
	dest->fmr_flags = src->fmr_flags;
	dest->fmr_physical = BBTOB(src->fmr_physical);
	dest->fmr_owner = src->fmr_owner;
	dest->fmr_offset = BBTOB(src->fmr_offset);
	dest->fmr_length = BBTOB(src->fmr_length);
	dest->fmr_reserved[0] = 0;
	dest->fmr_reserved[1] = 0;
	dest->fmr_reserved[2] = 0;
}

 
void
xfs_fsmap_to_internal(
	struct xfs_fsmap	*dest,
	struct fsmap		*src)
{
	dest->fmr_device = src->fmr_device;
	dest->fmr_flags = src->fmr_flags;
	dest->fmr_physical = BTOBBT(src->fmr_physical);
	dest->fmr_owner = src->fmr_owner;
	dest->fmr_offset = BTOBBT(src->fmr_offset);
	dest->fmr_length = BTOBBT(src->fmr_length);
}

 
static int
xfs_fsmap_owner_to_rmap(
	struct xfs_rmap_irec	*dest,
	const struct xfs_fsmap	*src)
{
	if (!(src->fmr_flags & FMR_OF_SPECIAL_OWNER)) {
		dest->rm_owner = src->fmr_owner;
		return 0;
	}

	switch (src->fmr_owner) {
	case 0:			 
	case -1ULL:		 
		dest->rm_owner = 0;
		break;
	case XFS_FMR_OWN_FREE:
		dest->rm_owner = XFS_RMAP_OWN_NULL;
		break;
	case XFS_FMR_OWN_UNKNOWN:
		dest->rm_owner = XFS_RMAP_OWN_UNKNOWN;
		break;
	case XFS_FMR_OWN_FS:
		dest->rm_owner = XFS_RMAP_OWN_FS;
		break;
	case XFS_FMR_OWN_LOG:
		dest->rm_owner = XFS_RMAP_OWN_LOG;
		break;
	case XFS_FMR_OWN_AG:
		dest->rm_owner = XFS_RMAP_OWN_AG;
		break;
	case XFS_FMR_OWN_INOBT:
		dest->rm_owner = XFS_RMAP_OWN_INOBT;
		break;
	case XFS_FMR_OWN_INODES:
		dest->rm_owner = XFS_RMAP_OWN_INODES;
		break;
	case XFS_FMR_OWN_REFC:
		dest->rm_owner = XFS_RMAP_OWN_REFC;
		break;
	case XFS_FMR_OWN_COW:
		dest->rm_owner = XFS_RMAP_OWN_COW;
		break;
	case XFS_FMR_OWN_DEFECTIVE:	 
		 
	default:
		return -EINVAL;
	}
	return 0;
}

 
static int
xfs_fsmap_owner_from_rmap(
	struct xfs_fsmap		*dest,
	const struct xfs_rmap_irec	*src)
{
	dest->fmr_flags = 0;
	if (!XFS_RMAP_NON_INODE_OWNER(src->rm_owner)) {
		dest->fmr_owner = src->rm_owner;
		return 0;
	}
	dest->fmr_flags |= FMR_OF_SPECIAL_OWNER;

	switch (src->rm_owner) {
	case XFS_RMAP_OWN_FS:
		dest->fmr_owner = XFS_FMR_OWN_FS;
		break;
	case XFS_RMAP_OWN_LOG:
		dest->fmr_owner = XFS_FMR_OWN_LOG;
		break;
	case XFS_RMAP_OWN_AG:
		dest->fmr_owner = XFS_FMR_OWN_AG;
		break;
	case XFS_RMAP_OWN_INOBT:
		dest->fmr_owner = XFS_FMR_OWN_INOBT;
		break;
	case XFS_RMAP_OWN_INODES:
		dest->fmr_owner = XFS_FMR_OWN_INODES;
		break;
	case XFS_RMAP_OWN_REFC:
		dest->fmr_owner = XFS_FMR_OWN_REFC;
		break;
	case XFS_RMAP_OWN_COW:
		dest->fmr_owner = XFS_FMR_OWN_COW;
		break;
	case XFS_RMAP_OWN_NULL:	 
		dest->fmr_owner = XFS_FMR_OWN_FREE;
		break;
	default:
		ASSERT(0);
		return -EFSCORRUPTED;
	}
	return 0;
}

 
struct xfs_getfsmap_info {
	struct xfs_fsmap_head	*head;
	struct fsmap		*fsmap_recs;	 
	struct xfs_buf		*agf_bp;	 
	struct xfs_perag	*pag;		 
	xfs_daddr_t		next_daddr;	 
	 
	xfs_daddr_t		low_daddr;
	u64			missing_owner;	 
	u32			dev;		 
	 
	struct xfs_rmap_irec	low;
	struct xfs_rmap_irec	high;		 
	bool			last;		 
};

 
struct xfs_getfsmap_dev {
	u32			dev;
	int			(*fn)(struct xfs_trans *tp,
				      const struct xfs_fsmap *keys,
				      struct xfs_getfsmap_info *info);
};

 
static int
xfs_getfsmap_dev_compare(
	const void			*p1,
	const void			*p2)
{
	const struct xfs_getfsmap_dev	*d1 = p1;
	const struct xfs_getfsmap_dev	*d2 = p2;

	return d1->dev - d2->dev;
}

 
STATIC int
xfs_getfsmap_is_shared(
	struct xfs_trans		*tp,
	struct xfs_getfsmap_info	*info,
	const struct xfs_rmap_irec	*rec,
	bool				*stat)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_btree_cur		*cur;
	xfs_agblock_t			fbno;
	xfs_extlen_t			flen;
	int				error;

	*stat = false;
	if (!xfs_has_reflink(mp))
		return 0;
	 
	if (!info->pag)
		return 0;

	 
	flen = 0;
	cur = xfs_refcountbt_init_cursor(mp, tp, info->agf_bp, info->pag);

	error = xfs_refcount_find_shared(cur, rec->rm_startblock,
			rec->rm_blockcount, &fbno, &flen, false);

	xfs_btree_del_cursor(cur, error);
	if (error)
		return error;

	*stat = flen > 0;
	return 0;
}

static inline void
xfs_getfsmap_format(
	struct xfs_mount		*mp,
	struct xfs_fsmap		*xfm,
	struct xfs_getfsmap_info	*info)
{
	struct fsmap			*rec;

	trace_xfs_getfsmap_mapping(mp, xfm);

	rec = &info->fsmap_recs[info->head->fmh_entries++];
	xfs_fsmap_from_internal(rec, xfm);
}

static inline bool
xfs_getfsmap_rec_before_start(
	struct xfs_getfsmap_info	*info,
	const struct xfs_rmap_irec	*rec,
	xfs_daddr_t			rec_daddr)
{
	if (info->low_daddr != -1ULL)
		return rec_daddr < info->low_daddr;
	if (info->low.rm_blockcount)
		return xfs_rmap_compare(rec, &info->low) < 0;
	return false;
}

 
STATIC int
xfs_getfsmap_helper(
	struct xfs_trans		*tp,
	struct xfs_getfsmap_info	*info,
	const struct xfs_rmap_irec	*rec,
	xfs_daddr_t			rec_daddr,
	xfs_daddr_t			len_daddr)
{
	struct xfs_fsmap		fmr;
	struct xfs_mount		*mp = tp->t_mountp;
	bool				shared;
	int				error;

	if (fatal_signal_pending(current))
		return -EINTR;

	if (len_daddr == 0)
		len_daddr = XFS_FSB_TO_BB(mp, rec->rm_blockcount);

	 
	if (xfs_getfsmap_rec_before_start(info, rec, rec_daddr)) {
		rec_daddr += len_daddr;
		if (info->next_daddr < rec_daddr)
			info->next_daddr = rec_daddr;
		return 0;
	}

	 
	if (info->head->fmh_count == 0) {
		if (info->head->fmh_entries == UINT_MAX)
			return -ECANCELED;

		if (rec_daddr > info->next_daddr)
			info->head->fmh_entries++;

		if (info->last)
			return 0;

		info->head->fmh_entries++;

		rec_daddr += len_daddr;
		if (info->next_daddr < rec_daddr)
			info->next_daddr = rec_daddr;
		return 0;
	}

	 
	if (rec_daddr > info->next_daddr) {
		if (info->head->fmh_entries >= info->head->fmh_count)
			return -ECANCELED;

		fmr.fmr_device = info->dev;
		fmr.fmr_physical = info->next_daddr;
		fmr.fmr_owner = info->missing_owner;
		fmr.fmr_offset = 0;
		fmr.fmr_length = rec_daddr - info->next_daddr;
		fmr.fmr_flags = FMR_OF_SPECIAL_OWNER;
		xfs_getfsmap_format(mp, &fmr, info);
	}

	if (info->last)
		goto out;

	 
	if (info->head->fmh_entries >= info->head->fmh_count)
		return -ECANCELED;

	trace_xfs_fsmap_mapping(mp, info->dev,
			info->pag ? info->pag->pag_agno : NULLAGNUMBER, rec);

	fmr.fmr_device = info->dev;
	fmr.fmr_physical = rec_daddr;
	error = xfs_fsmap_owner_from_rmap(&fmr, rec);
	if (error)
		return error;
	fmr.fmr_offset = XFS_FSB_TO_BB(mp, rec->rm_offset);
	fmr.fmr_length = len_daddr;
	if (rec->rm_flags & XFS_RMAP_UNWRITTEN)
		fmr.fmr_flags |= FMR_OF_PREALLOC;
	if (rec->rm_flags & XFS_RMAP_ATTR_FORK)
		fmr.fmr_flags |= FMR_OF_ATTR_FORK;
	if (rec->rm_flags & XFS_RMAP_BMBT_BLOCK)
		fmr.fmr_flags |= FMR_OF_EXTENT_MAP;
	if (fmr.fmr_flags == 0) {
		error = xfs_getfsmap_is_shared(tp, info, rec, &shared);
		if (error)
			return error;
		if (shared)
			fmr.fmr_flags |= FMR_OF_SHARED;
	}

	xfs_getfsmap_format(mp, &fmr, info);
out:
	rec_daddr += len_daddr;
	if (info->next_daddr < rec_daddr)
		info->next_daddr = rec_daddr;
	return 0;
}

 
STATIC int
xfs_getfsmap_datadev_helper(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_getfsmap_info	*info = priv;
	xfs_fsblock_t			fsb;
	xfs_daddr_t			rec_daddr;

	fsb = XFS_AGB_TO_FSB(mp, cur->bc_ag.pag->pag_agno, rec->rm_startblock);
	rec_daddr = XFS_FSB_TO_DADDR(mp, fsb);

	return xfs_getfsmap_helper(cur->bc_tp, info, rec, rec_daddr, 0);
}

 
STATIC int
xfs_getfsmap_datadev_bnobt_helper(
	struct xfs_btree_cur		*cur,
	const struct xfs_alloc_rec_incore *rec,
	void				*priv)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_getfsmap_info	*info = priv;
	struct xfs_rmap_irec		irec;
	xfs_daddr_t			rec_daddr;

	rec_daddr = XFS_AGB_TO_DADDR(mp, cur->bc_ag.pag->pag_agno,
			rec->ar_startblock);

	irec.rm_startblock = rec->ar_startblock;
	irec.rm_blockcount = rec->ar_blockcount;
	irec.rm_owner = XFS_RMAP_OWN_NULL;	 
	irec.rm_offset = 0;
	irec.rm_flags = 0;

	return xfs_getfsmap_helper(cur->bc_tp, info, &irec, rec_daddr, 0);
}

 
static void
xfs_getfsmap_set_irec_flags(
	struct xfs_rmap_irec	*irec,
	const struct xfs_fsmap	*fmr)
{
	irec->rm_flags = 0;
	if (fmr->fmr_flags & FMR_OF_ATTR_FORK)
		irec->rm_flags |= XFS_RMAP_ATTR_FORK;
	if (fmr->fmr_flags & FMR_OF_EXTENT_MAP)
		irec->rm_flags |= XFS_RMAP_BMBT_BLOCK;
	if (fmr->fmr_flags & FMR_OF_PREALLOC)
		irec->rm_flags |= XFS_RMAP_UNWRITTEN;
}

 
STATIC int
xfs_getfsmap_logdev(
	struct xfs_trans		*tp,
	const struct xfs_fsmap		*keys,
	struct xfs_getfsmap_info	*info)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_rmap_irec		rmap;
	xfs_daddr_t			rec_daddr, len_daddr;
	xfs_fsblock_t			start_fsb, end_fsb;
	uint64_t			eofs;

	eofs = XFS_FSB_TO_BB(mp, mp->m_sb.sb_logblocks);
	if (keys[0].fmr_physical >= eofs)
		return 0;
	start_fsb = XFS_BB_TO_FSBT(mp,
				keys[0].fmr_physical + keys[0].fmr_length);
	end_fsb = XFS_BB_TO_FSB(mp, min(eofs - 1, keys[1].fmr_physical));

	 
	if (keys[0].fmr_length > 0)
		info->low_daddr = XFS_FSB_TO_BB(mp, start_fsb);

	trace_xfs_fsmap_low_key_linear(mp, info->dev, start_fsb);
	trace_xfs_fsmap_high_key_linear(mp, info->dev, end_fsb);

	if (start_fsb > 0)
		return 0;

	 
	rmap.rm_startblock = 0;
	rmap.rm_blockcount = mp->m_sb.sb_logblocks;
	rmap.rm_owner = XFS_RMAP_OWN_LOG;
	rmap.rm_offset = 0;
	rmap.rm_flags = 0;

	rec_daddr = XFS_FSB_TO_BB(mp, rmap.rm_startblock);
	len_daddr = XFS_FSB_TO_BB(mp, rmap.rm_blockcount);
	return xfs_getfsmap_helper(tp, info, &rmap, rec_daddr, len_daddr);
}

#ifdef CONFIG_XFS_RT
 
STATIC int
xfs_getfsmap_rtdev_rtbitmap_helper(
	struct xfs_mount		*mp,
	struct xfs_trans		*tp,
	const struct xfs_rtalloc_rec	*rec,
	void				*priv)
{
	struct xfs_getfsmap_info	*info = priv;
	struct xfs_rmap_irec		irec;
	xfs_rtblock_t			rtbno;
	xfs_daddr_t			rec_daddr, len_daddr;

	rtbno = rec->ar_startext * mp->m_sb.sb_rextsize;
	rec_daddr = XFS_FSB_TO_BB(mp, rtbno);
	irec.rm_startblock = rtbno;

	rtbno = rec->ar_extcount * mp->m_sb.sb_rextsize;
	len_daddr = XFS_FSB_TO_BB(mp, rtbno);
	irec.rm_blockcount = rtbno;

	irec.rm_owner = XFS_RMAP_OWN_NULL;	 
	irec.rm_offset = 0;
	irec.rm_flags = 0;

	return xfs_getfsmap_helper(tp, info, &irec, rec_daddr, len_daddr);
}

 
STATIC int
xfs_getfsmap_rtdev_rtbitmap(
	struct xfs_trans		*tp,
	const struct xfs_fsmap		*keys,
	struct xfs_getfsmap_info	*info)
{

	struct xfs_rtalloc_rec		alow = { 0 };
	struct xfs_rtalloc_rec		ahigh = { 0 };
	struct xfs_mount		*mp = tp->t_mountp;
	xfs_rtblock_t			start_rtb;
	xfs_rtblock_t			end_rtb;
	uint64_t			eofs;
	int				error;

	eofs = XFS_FSB_TO_BB(mp, mp->m_sb.sb_rextents * mp->m_sb.sb_rextsize);
	if (keys[0].fmr_physical >= eofs)
		return 0;
	start_rtb = XFS_BB_TO_FSBT(mp,
				keys[0].fmr_physical + keys[0].fmr_length);
	end_rtb = XFS_BB_TO_FSB(mp, min(eofs - 1, keys[1].fmr_physical));

	info->missing_owner = XFS_FMR_OWN_UNKNOWN;

	 
	if (keys[0].fmr_length > 0) {
		info->low_daddr = XFS_FSB_TO_BB(mp, start_rtb);
		if (info->low_daddr >= eofs)
			return 0;
	}

	trace_xfs_fsmap_low_key_linear(mp, info->dev, start_rtb);
	trace_xfs_fsmap_high_key_linear(mp, info->dev, end_rtb);

	xfs_ilock(mp->m_rbmip, XFS_ILOCK_SHARED | XFS_ILOCK_RTBITMAP);

	 
	alow.ar_startext = start_rtb;
	ahigh.ar_startext = end_rtb;
	do_div(alow.ar_startext, mp->m_sb.sb_rextsize);
	if (do_div(ahigh.ar_startext, mp->m_sb.sb_rextsize))
		ahigh.ar_startext++;
	error = xfs_rtalloc_query_range(mp, tp, &alow, &ahigh,
			xfs_getfsmap_rtdev_rtbitmap_helper, info);
	if (error)
		goto err;

	 
	info->last = true;
	ahigh.ar_startext = min(mp->m_sb.sb_rextents, ahigh.ar_startext);

	error = xfs_getfsmap_rtdev_rtbitmap_helper(mp, tp, &ahigh, info);
	if (error)
		goto err;
err:
	xfs_iunlock(mp->m_rbmip, XFS_ILOCK_SHARED | XFS_ILOCK_RTBITMAP);
	return error;
}
#endif  

static inline bool
rmap_not_shareable(struct xfs_mount *mp, const struct xfs_rmap_irec *r)
{
	if (!xfs_has_reflink(mp))
		return true;
	if (XFS_RMAP_NON_INODE_OWNER(r->rm_owner))
		return true;
	if (r->rm_flags & (XFS_RMAP_ATTR_FORK | XFS_RMAP_BMBT_BLOCK |
			   XFS_RMAP_UNWRITTEN))
		return true;
	return false;
}

 
STATIC int
__xfs_getfsmap_datadev(
	struct xfs_trans		*tp,
	const struct xfs_fsmap		*keys,
	struct xfs_getfsmap_info	*info,
	int				(*query_fn)(struct xfs_trans *,
						    struct xfs_getfsmap_info *,
						    struct xfs_btree_cur **,
						    void *),
	void				*priv)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_perag		*pag;
	struct xfs_btree_cur		*bt_cur = NULL;
	xfs_fsblock_t			start_fsb;
	xfs_fsblock_t			end_fsb;
	xfs_agnumber_t			start_ag;
	xfs_agnumber_t			end_ag;
	uint64_t			eofs;
	int				error = 0;

	eofs = XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks);
	if (keys[0].fmr_physical >= eofs)
		return 0;
	start_fsb = XFS_DADDR_TO_FSB(mp, keys[0].fmr_physical);
	end_fsb = XFS_DADDR_TO_FSB(mp, min(eofs - 1, keys[1].fmr_physical));

	 
	info->low.rm_offset = XFS_BB_TO_FSBT(mp, keys[0].fmr_offset);
	error = xfs_fsmap_owner_to_rmap(&info->low, &keys[0]);
	if (error)
		return error;
	info->low.rm_blockcount = XFS_BB_TO_FSBT(mp, keys[0].fmr_length);
	xfs_getfsmap_set_irec_flags(&info->low, &keys[0]);

	 
	if (info->low.rm_blockcount == 0) {
		 
	} else if (rmap_not_shareable(mp, &info->low)) {
		 
		info->low.rm_owner = 0;
		info->low.rm_offset = 0;

		start_fsb += info->low.rm_blockcount;
		if (XFS_FSB_TO_DADDR(mp, start_fsb) >= eofs)
			return 0;
	} else {
		 
		info->low.rm_offset += info->low.rm_blockcount;
	}
	info->low.rm_startblock = XFS_FSB_TO_AGBNO(mp, start_fsb);

	info->high.rm_startblock = -1U;
	info->high.rm_owner = ULLONG_MAX;
	info->high.rm_offset = ULLONG_MAX;
	info->high.rm_blockcount = 0;
	info->high.rm_flags = XFS_RMAP_KEY_FLAGS | XFS_RMAP_REC_FLAGS;

	start_ag = XFS_FSB_TO_AGNO(mp, start_fsb);
	end_ag = XFS_FSB_TO_AGNO(mp, end_fsb);

	for_each_perag_range(mp, start_ag, end_ag, pag) {
		 
		info->pag = pag;
		if (pag->pag_agno == end_ag) {
			info->high.rm_startblock = XFS_FSB_TO_AGBNO(mp,
					end_fsb);
			info->high.rm_offset = XFS_BB_TO_FSBT(mp,
					keys[1].fmr_offset);
			error = xfs_fsmap_owner_to_rmap(&info->high, &keys[1]);
			if (error)
				break;
			xfs_getfsmap_set_irec_flags(&info->high, &keys[1]);
		}

		if (bt_cur) {
			xfs_btree_del_cursor(bt_cur, XFS_BTREE_NOERROR);
			bt_cur = NULL;
			xfs_trans_brelse(tp, info->agf_bp);
			info->agf_bp = NULL;
		}

		error = xfs_alloc_read_agf(pag, tp, 0, &info->agf_bp);
		if (error)
			break;

		trace_xfs_fsmap_low_key(mp, info->dev, pag->pag_agno,
				&info->low);
		trace_xfs_fsmap_high_key(mp, info->dev, pag->pag_agno,
				&info->high);

		error = query_fn(tp, info, &bt_cur, priv);
		if (error)
			break;

		 
		if (pag->pag_agno == start_ag)
			memset(&info->low, 0, sizeof(info->low));

		 
		if (pag->pag_agno == end_ag) {
			info->last = true;
			error = query_fn(tp, info, &bt_cur, priv);
			if (error)
				break;
		}
		info->pag = NULL;
	}

	if (bt_cur)
		xfs_btree_del_cursor(bt_cur, error < 0 ? XFS_BTREE_ERROR :
							 XFS_BTREE_NOERROR);
	if (info->agf_bp) {
		xfs_trans_brelse(tp, info->agf_bp);
		info->agf_bp = NULL;
	}
	if (info->pag) {
		xfs_perag_rele(info->pag);
		info->pag = NULL;
	} else if (pag) {
		 
		xfs_perag_rele(pag);
	}

	return error;
}

 
STATIC int
xfs_getfsmap_datadev_rmapbt_query(
	struct xfs_trans		*tp,
	struct xfs_getfsmap_info	*info,
	struct xfs_btree_cur		**curpp,
	void				*priv)
{
	 
	if (info->last)
		return xfs_getfsmap_datadev_helper(*curpp, &info->high, info);

	 
	*curpp = xfs_rmapbt_init_cursor(tp->t_mountp, tp, info->agf_bp,
			info->pag);
	return xfs_rmap_query_range(*curpp, &info->low, &info->high,
			xfs_getfsmap_datadev_helper, info);
}

 
STATIC int
xfs_getfsmap_datadev_rmapbt(
	struct xfs_trans		*tp,
	const struct xfs_fsmap		*keys,
	struct xfs_getfsmap_info	*info)
{
	info->missing_owner = XFS_FMR_OWN_FREE;
	return __xfs_getfsmap_datadev(tp, keys, info,
			xfs_getfsmap_datadev_rmapbt_query, NULL);
}

 
STATIC int
xfs_getfsmap_datadev_bnobt_query(
	struct xfs_trans		*tp,
	struct xfs_getfsmap_info	*info,
	struct xfs_btree_cur		**curpp,
	void				*priv)
{
	struct xfs_alloc_rec_incore	*key = priv;

	 
	if (info->last)
		return xfs_getfsmap_datadev_bnobt_helper(*curpp, &key[1], info);

	 
	*curpp = xfs_allocbt_init_cursor(tp->t_mountp, tp, info->agf_bp,
			info->pag, XFS_BTNUM_BNO);
	key->ar_startblock = info->low.rm_startblock;
	key[1].ar_startblock = info->high.rm_startblock;
	return xfs_alloc_query_range(*curpp, key, &key[1],
			xfs_getfsmap_datadev_bnobt_helper, info);
}

 
STATIC int
xfs_getfsmap_datadev_bnobt(
	struct xfs_trans		*tp,
	const struct xfs_fsmap		*keys,
	struct xfs_getfsmap_info	*info)
{
	struct xfs_alloc_rec_incore	akeys[2];

	memset(akeys, 0, sizeof(akeys));
	info->missing_owner = XFS_FMR_OWN_UNKNOWN;
	return __xfs_getfsmap_datadev(tp, keys, info,
			xfs_getfsmap_datadev_bnobt_query, &akeys[0]);
}

 
STATIC bool
xfs_getfsmap_is_valid_device(
	struct xfs_mount	*mp,
	struct xfs_fsmap	*fm)
{
	if (fm->fmr_device == 0 || fm->fmr_device == UINT_MAX ||
	    fm->fmr_device == new_encode_dev(mp->m_ddev_targp->bt_dev))
		return true;
	if (mp->m_logdev_targp &&
	    fm->fmr_device == new_encode_dev(mp->m_logdev_targp->bt_dev))
		return true;
	if (mp->m_rtdev_targp &&
	    fm->fmr_device == new_encode_dev(mp->m_rtdev_targp->bt_dev))
		return true;
	return false;
}

 
STATIC bool
xfs_getfsmap_check_keys(
	struct xfs_fsmap		*low_key,
	struct xfs_fsmap		*high_key)
{
	if (low_key->fmr_flags & (FMR_OF_SPECIAL_OWNER | FMR_OF_EXTENT_MAP)) {
		if (low_key->fmr_offset)
			return false;
	}
	if (high_key->fmr_flags != -1U &&
	    (high_key->fmr_flags & (FMR_OF_SPECIAL_OWNER |
				    FMR_OF_EXTENT_MAP))) {
		if (high_key->fmr_offset && high_key->fmr_offset != -1ULL)
			return false;
	}
	if (high_key->fmr_length && high_key->fmr_length != -1ULL)
		return false;

	if (low_key->fmr_device > high_key->fmr_device)
		return false;
	if (low_key->fmr_device < high_key->fmr_device)
		return true;

	if (low_key->fmr_physical > high_key->fmr_physical)
		return false;
	if (low_key->fmr_physical < high_key->fmr_physical)
		return true;

	if (low_key->fmr_owner > high_key->fmr_owner)
		return false;
	if (low_key->fmr_owner < high_key->fmr_owner)
		return true;

	if (low_key->fmr_offset > high_key->fmr_offset)
		return false;
	if (low_key->fmr_offset < high_key->fmr_offset)
		return true;

	return false;
}

 
#ifdef CONFIG_XFS_RT
#define XFS_GETFSMAP_DEVS	3
#else
#define XFS_GETFSMAP_DEVS	2
#endif  

 
int
xfs_getfsmap(
	struct xfs_mount		*mp,
	struct xfs_fsmap_head		*head,
	struct fsmap			*fsmap_recs)
{
	struct xfs_trans		*tp = NULL;
	struct xfs_fsmap		dkeys[2];	 
	struct xfs_getfsmap_dev		handlers[XFS_GETFSMAP_DEVS];
	struct xfs_getfsmap_info	info = { NULL };
	bool				use_rmap;
	int				i;
	int				error = 0;

	if (head->fmh_iflags & ~FMH_IF_VALID)
		return -EINVAL;
	if (!xfs_getfsmap_is_valid_device(mp, &head->fmh_keys[0]) ||
	    !xfs_getfsmap_is_valid_device(mp, &head->fmh_keys[1]))
		return -EINVAL;
	if (!xfs_getfsmap_check_keys(&head->fmh_keys[0], &head->fmh_keys[1]))
		return -EINVAL;

	use_rmap = xfs_has_rmapbt(mp) &&
		   has_capability_noaudit(current, CAP_SYS_ADMIN);
	head->fmh_entries = 0;

	 
	memset(handlers, 0, sizeof(handlers));
	handlers[0].dev = new_encode_dev(mp->m_ddev_targp->bt_dev);
	if (use_rmap)
		handlers[0].fn = xfs_getfsmap_datadev_rmapbt;
	else
		handlers[0].fn = xfs_getfsmap_datadev_bnobt;
	if (mp->m_logdev_targp != mp->m_ddev_targp) {
		handlers[1].dev = new_encode_dev(mp->m_logdev_targp->bt_dev);
		handlers[1].fn = xfs_getfsmap_logdev;
	}
#ifdef CONFIG_XFS_RT
	if (mp->m_rtdev_targp) {
		handlers[2].dev = new_encode_dev(mp->m_rtdev_targp->bt_dev);
		handlers[2].fn = xfs_getfsmap_rtdev_rtbitmap;
	}
#endif  

	xfs_sort(handlers, XFS_GETFSMAP_DEVS, sizeof(struct xfs_getfsmap_dev),
			xfs_getfsmap_dev_compare);

	 
	dkeys[0] = head->fmh_keys[0];
	memset(&dkeys[1], 0xFF, sizeof(struct xfs_fsmap));

	info.next_daddr = head->fmh_keys[0].fmr_physical +
			  head->fmh_keys[0].fmr_length;
	info.fsmap_recs = fsmap_recs;
	info.head = head;

	 
	for (i = 0; i < XFS_GETFSMAP_DEVS; i++) {
		 
		if (!handlers[i].fn)
			continue;
		if (head->fmh_keys[0].fmr_device > handlers[i].dev)
			continue;
		if (head->fmh_keys[1].fmr_device < handlers[i].dev)
			break;

		 
		if (handlers[i].dev == head->fmh_keys[1].fmr_device)
			dkeys[1] = head->fmh_keys[1];
		if (handlers[i].dev > head->fmh_keys[0].fmr_device)
			memset(&dkeys[0], 0, sizeof(struct xfs_fsmap));

		 
		error = xfs_trans_alloc_empty(mp, &tp);
		if (error)
			break;

		info.dev = handlers[i].dev;
		info.last = false;
		info.pag = NULL;
		info.low_daddr = -1ULL;
		info.low.rm_blockcount = 0;
		error = handlers[i].fn(tp, dkeys, &info);
		if (error)
			break;
		xfs_trans_cancel(tp);
		tp = NULL;
		info.next_daddr = 0;
	}

	if (tp)
		xfs_trans_cancel(tp);
	head->fmh_oflags = FMH_OF_DEV_T;
	return error;
}

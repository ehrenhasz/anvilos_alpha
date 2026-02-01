
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_bmap_btree.h"
#include "xfs_trans.h"
#include "xfs_trans_space.h"
#include "xfs_icache.h"
#include "xfs_rtalloc.h"
#include "xfs_sb.h"

 
static int
xfs_rtget_summary(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	int		log,		 
	xfs_rtblock_t	bbno,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb,		 
	xfs_suminfo_t	*sum)		 
{
	return xfs_rtmodify_summary_int(mp, tp, log, bbno, 0, rbpp, rsb, sum);
}

 
STATIC int				 
xfs_rtany_summary(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	int		low,		 
	int		high,		 
	xfs_rtblock_t	bbno,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb,		 
	int		*stat)		 
{
	int		error;		 
	int		log;		 
	xfs_suminfo_t	sum;		 

	 
	if (mp->m_rsum_cache && low < mp->m_rsum_cache[bbno])
		low = mp->m_rsum_cache[bbno];

	 
	for (log = low; log <= high; log++) {
		 
		error = xfs_rtget_summary(mp, tp, log, bbno, rbpp, rsb, &sum);
		if (error) {
			return error;
		}
		 
		if (sum) {
			*stat = 1;
			goto out;
		}
	}
	 
	*stat = 0;
out:
	 
	if (mp->m_rsum_cache && log > mp->m_rsum_cache[bbno])
		mp->m_rsum_cache[bbno] = log;
	return 0;
}


 
STATIC int				 
xfs_rtcopy_summary(
	xfs_mount_t	*omp,		 
	xfs_mount_t	*nmp,		 
	xfs_trans_t	*tp)		 
{
	xfs_rtblock_t	bbno;		 
	struct xfs_buf	*bp;		 
	int		error;		 
	int		log;		 
	xfs_suminfo_t	sum;		 
	xfs_fsblock_t	sumbno;		 

	bp = NULL;
	for (log = omp->m_rsumlevels - 1; log >= 0; log--) {
		for (bbno = omp->m_sb.sb_rbmblocks - 1;
		     (xfs_srtblock_t)bbno >= 0;
		     bbno--) {
			error = xfs_rtget_summary(omp, tp, log, bbno, &bp,
				&sumbno, &sum);
			if (error)
				return error;
			if (sum == 0)
				continue;
			error = xfs_rtmodify_summary(omp, tp, log, bbno, -sum,
				&bp, &sumbno);
			if (error)
				return error;
			error = xfs_rtmodify_summary(nmp, tp, log, bbno, sum,
				&bp, &sumbno);
			if (error)
				return error;
			ASSERT(sum > 0);
		}
	}
	return 0;
}
 
STATIC int				 
xfs_rtallocate_range(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	xfs_rtblock_t	start,		 
	xfs_extlen_t	len,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb)		 
{
	xfs_rtblock_t	end;		 
	int		error;		 
	xfs_rtblock_t	postblock = 0;	 
	xfs_rtblock_t	preblock = 0;	 

	end = start + len - 1;
	 
	error = xfs_rtfind_back(mp, tp, start, 0, &preblock);
	if (error) {
		return error;
	}
	 
	error = xfs_rtfind_forw(mp, tp, end, mp->m_sb.sb_rextents - 1,
		&postblock);
	if (error) {
		return error;
	}
	 
	error = xfs_rtmodify_summary(mp, tp,
		XFS_RTBLOCKLOG(postblock + 1 - preblock),
		XFS_BITTOBLOCK(mp, preblock), -1, rbpp, rsb);
	if (error) {
		return error;
	}
	 
	if (preblock < start) {
		error = xfs_rtmodify_summary(mp, tp,
			XFS_RTBLOCKLOG(start - preblock),
			XFS_BITTOBLOCK(mp, preblock), 1, rbpp, rsb);
		if (error) {
			return error;
		}
	}
	 
	if (postblock > end) {
		error = xfs_rtmodify_summary(mp, tp,
			XFS_RTBLOCKLOG(postblock - end),
			XFS_BITTOBLOCK(mp, end + 1), 1, rbpp, rsb);
		if (error) {
			return error;
		}
	}
	 
	error = xfs_rtmodify_range(mp, tp, start, len, 0);
	return error;
}

 
STATIC int				 
xfs_rtallocate_extent_block(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	xfs_rtblock_t	bbno,		 
	xfs_extlen_t	minlen,		 
	xfs_extlen_t	maxlen,		 
	xfs_extlen_t	*len,		 
	xfs_rtblock_t	*nextp,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb,		 
	xfs_extlen_t	prod,		 
	xfs_rtblock_t	*rtblock)	 
{
	xfs_rtblock_t	besti;		 
	xfs_rtblock_t	bestlen;	 
	xfs_rtblock_t	end;		 
	int		error;		 
	xfs_rtblock_t	i;		 
	xfs_rtblock_t	next;		 
	int		stat;		 

	 
	for (i = XFS_BLOCKTOBIT(mp, bbno), besti = -1, bestlen = 0,
		end = XFS_BLOCKTOBIT(mp, bbno + 1) - 1;
	     i <= end;
	     i++) {
		 
		maxlen = min(mp->m_sb.sb_rextents, i + maxlen) - i;

		 
		error = xfs_rtcheck_range(mp, tp, i, maxlen, 1, &next, &stat);
		if (error) {
			return error;
		}
		if (stat) {
			 
			error = xfs_rtallocate_range(mp, tp, i, maxlen, rbpp,
				rsb);
			if (error) {
				return error;
			}
			*len = maxlen;
			*rtblock = i;
			return 0;
		}
		 
		if (minlen < maxlen) {
			xfs_rtblock_t	thislen;	 

			thislen = next - i;
			if (thislen >= minlen && thislen > bestlen) {
				besti = i;
				bestlen = thislen;
			}
		}
		 
		if (next < end) {
			error = xfs_rtfind_forw(mp, tp, next, end, &i);
			if (error) {
				return error;
			}
		} else
			break;
	}
	 
	if (minlen < maxlen && besti != -1) {
		xfs_extlen_t	p;	 

		 
		if (prod > 1) {
			div_u64_rem(bestlen, prod, &p);
			if (p)
				bestlen -= p;
		}

		 
		error = xfs_rtallocate_range(mp, tp, besti, bestlen, rbpp, rsb);
		if (error) {
			return error;
		}
		*len = bestlen;
		*rtblock = besti;
		return 0;
	}
	 
	*nextp = next;
	*rtblock = NULLRTBLOCK;
	return 0;
}

 
STATIC int				 
xfs_rtallocate_extent_exact(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	xfs_rtblock_t	bno,		 
	xfs_extlen_t	minlen,		 
	xfs_extlen_t	maxlen,		 
	xfs_extlen_t	*len,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb,		 
	xfs_extlen_t	prod,		 
	xfs_rtblock_t	*rtblock)	 
{
	int		error;		 
	xfs_extlen_t	i;		 
	int		isfree;		 
	xfs_rtblock_t	next;		 

	ASSERT(minlen % prod == 0 && maxlen % prod == 0);
	 
	error = xfs_rtcheck_range(mp, tp, bno, maxlen, 1, &next, &isfree);
	if (error) {
		return error;
	}
	if (isfree) {
		 
		error = xfs_rtallocate_range(mp, tp, bno, maxlen, rbpp, rsb);
		if (error) {
			return error;
		}
		*len = maxlen;
		*rtblock = bno;
		return 0;
	}
	 
	maxlen = next - bno;
	if (maxlen < minlen) {
		 
		*rtblock = NULLRTBLOCK;
		return 0;
	}
	 
	if (prod > 1 && (i = maxlen % prod)) {
		maxlen -= i;
		if (maxlen < minlen) {
			 
			*rtblock = NULLRTBLOCK;
			return 0;
		}
	}
	 
	error = xfs_rtallocate_range(mp, tp, bno, maxlen, rbpp, rsb);
	if (error) {
		return error;
	}
	*len = maxlen;
	*rtblock = bno;
	return 0;
}

 
STATIC int				 
xfs_rtallocate_extent_near(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	xfs_rtblock_t	bno,		 
	xfs_extlen_t	minlen,		 
	xfs_extlen_t	maxlen,		 
	xfs_extlen_t	*len,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb,		 
	xfs_extlen_t	prod,		 
	xfs_rtblock_t	*rtblock)	 
{
	int		any;		 
	xfs_rtblock_t	bbno;		 
	int		error;		 
	int		i;		 
	int		j;		 
	int		log2len;	 
	xfs_rtblock_t	n;		 
	xfs_rtblock_t	r;		 

	ASSERT(minlen % prod == 0 && maxlen % prod == 0);
	 
	if (bno >= mp->m_sb.sb_rextents)
		bno = mp->m_sb.sb_rextents - 1;

	 
	maxlen = min(mp->m_sb.sb_rextents, bno + maxlen) - bno;
	if (maxlen < minlen) {
		*rtblock = NULLRTBLOCK;
		return 0;
	}

	 
	error = xfs_rtallocate_extent_exact(mp, tp, bno, minlen, maxlen, len,
		rbpp, rsb, prod, &r);
	if (error) {
		return error;
	}
	 
	if (r != NULLRTBLOCK) {
		*rtblock = r;
		return 0;
	}
	bbno = XFS_BITTOBLOCK(mp, bno);
	i = 0;
	ASSERT(minlen != 0);
	log2len = xfs_highbit32(minlen);
	 
	for (;;) {
		 
		error = xfs_rtany_summary(mp, tp, log2len, mp->m_rsumlevels - 1,
			bbno + i, rbpp, rsb, &any);
		if (error) {
			return error;
		}
		 
		if (any) {
			 
			if (i >= 0) {
				 
				error = xfs_rtallocate_extent_block(mp, tp,
					bbno + i, minlen, maxlen, len, &n, rbpp,
					rsb, prod, &r);
				if (error) {
					return error;
				}
				 
				if (r != NULLRTBLOCK) {
					*rtblock = r;
					return 0;
				}
			}
			 
			else {		 
				 
				for (j = -1; j > i; j--) {
					 
					error = xfs_rtany_summary(mp, tp,
						log2len, mp->m_rsumlevels - 1,
						bbno + j, rbpp, rsb, &any);
					if (error) {
						return error;
					}
					 
					if (any)
						continue;
					error = xfs_rtallocate_extent_block(mp,
						tp, bbno + j, minlen, maxlen,
						len, &n, rbpp, rsb, prod, &r);
					if (error) {
						return error;
					}
					 
					if (r != NULLRTBLOCK) {
						*rtblock = r;
						return 0;
					}
				}
				 
				error = xfs_rtallocate_extent_block(mp, tp,
					bbno + i, minlen, maxlen, len, &n, rbpp,
					rsb, prod, &r);
				if (error) {
					return error;
				}
				 
				if (r != NULLRTBLOCK) {
					*rtblock = r;
					return 0;
				}
			}
		}
		 
		if (i > 0 && (int)bbno - i >= 0)
			i = -i;
		 
		else if (i > 0 && (int)bbno + i < mp->m_sb.sb_rbmblocks - 1)
			i++;
		 
		else if (i <= 0 && (int)bbno - i < mp->m_sb.sb_rbmblocks - 1)
			i = 1 - i;
		 
		else if (i <= 0 && (int)bbno + i > 0)
			i--;
		 
		else
			break;
	}
	*rtblock = NULLRTBLOCK;
	return 0;
}

 
STATIC int				 
xfs_rtallocate_extent_size(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	xfs_extlen_t	minlen,		 
	xfs_extlen_t	maxlen,		 
	xfs_extlen_t	*len,		 
	struct xfs_buf	**rbpp,		 
	xfs_fsblock_t	*rsb,		 
	xfs_extlen_t	prod,		 
	xfs_rtblock_t	*rtblock)	 
{
	int		error;		 
	int		i;		 
	int		l;		 
	xfs_rtblock_t	n;		 
	xfs_rtblock_t	r;		 
	xfs_suminfo_t	sum;		 

	ASSERT(minlen % prod == 0 && maxlen % prod == 0);
	ASSERT(maxlen != 0);

	 
	for (l = xfs_highbit32(maxlen); l < mp->m_rsumlevels; l++) {
		 
		for (i = 0; i < mp->m_sb.sb_rbmblocks; i++) {
			 
			error = xfs_rtget_summary(mp, tp, l, i, rbpp, rsb,
				&sum);
			if (error) {
				return error;
			}
			 
			if (!sum)
				continue;
			 
			error = xfs_rtallocate_extent_block(mp, tp, i, maxlen,
				maxlen, len, &n, rbpp, rsb, prod, &r);
			if (error) {
				return error;
			}
			 
			if (r != NULLRTBLOCK) {
				*rtblock = r;
				return 0;
			}
			 
			if (XFS_BITTOBLOCK(mp, n) > i + 1)
				i = XFS_BITTOBLOCK(mp, n) - 1;
		}
	}
	 
	if (minlen > --maxlen) {
		*rtblock = NULLRTBLOCK;
		return 0;
	}
	ASSERT(minlen != 0);
	ASSERT(maxlen != 0);

	 
	for (l = xfs_highbit32(maxlen); l >= xfs_highbit32(minlen); l--) {
		 
		for (i = 0; i < mp->m_sb.sb_rbmblocks; i++) {
			 
			error =	xfs_rtget_summary(mp, tp, l, i, rbpp, rsb,
						  &sum);
			if (error) {
				return error;
			}
			 
			if (!sum)
				continue;
			 
			error = xfs_rtallocate_extent_block(mp, tp, i,
					XFS_RTMAX(minlen, 1 << l),
					XFS_RTMIN(maxlen, (1 << (l + 1)) - 1),
					len, &n, rbpp, rsb, prod, &r);
			if (error) {
				return error;
			}
			 
			if (r != NULLRTBLOCK) {
				*rtblock = r;
				return 0;
			}
			 
			if (XFS_BITTOBLOCK(mp, n) > i + 1)
				i = XFS_BITTOBLOCK(mp, n) - 1;
		}
	}
	 
	*rtblock = NULLRTBLOCK;
	return 0;
}

 
STATIC int
xfs_growfs_rt_alloc(
	struct xfs_mount	*mp,		 
	xfs_extlen_t		oblocks,	 
	xfs_extlen_t		nblocks,	 
	struct xfs_inode	*ip)		 
{
	xfs_fileoff_t		bno;		 
	struct xfs_buf		*bp;	 
	xfs_daddr_t		d;		 
	int			error;		 
	xfs_fsblock_t		fsbno;		 
	struct xfs_bmbt_irec	map;		 
	int			nmap;		 
	int			resblks;	 
	enum xfs_blft		buf_type;
	struct xfs_trans	*tp;

	if (ip == mp->m_rsumip)
		buf_type = XFS_BLFT_RTSUMMARY_BUF;
	else
		buf_type = XFS_BLFT_RTBITMAP_BUF;

	 
	while (oblocks < nblocks) {
		resblks = XFS_GROWFSRT_SPACE_RES(mp, nblocks - oblocks);
		 
		error = xfs_trans_alloc(mp, &M_RES(mp)->tr_growrtalloc, resblks,
				0, 0, &tp);
		if (error)
			return error;
		 
		xfs_ilock(ip, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);

		error = xfs_iext_count_may_overflow(ip, XFS_DATA_FORK,
				XFS_IEXT_ADD_NOSPLIT_CNT);
		if (error == -EFBIG)
			error = xfs_iext_count_upgrade(tp, ip,
					XFS_IEXT_ADD_NOSPLIT_CNT);
		if (error)
			goto out_trans_cancel;

		 
		nmap = 1;
		error = xfs_bmapi_write(tp, ip, oblocks, nblocks - oblocks,
					XFS_BMAPI_METADATA, 0, &map, &nmap);
		if (!error && nmap < 1)
			error = -ENOSPC;
		if (error)
			goto out_trans_cancel;
		 
		error = xfs_trans_commit(tp);
		if (error)
			return error;
		 
		for (bno = map.br_startoff, fsbno = map.br_startblock;
		     bno < map.br_startoff + map.br_blockcount;
		     bno++, fsbno++) {
			 
			error = xfs_trans_alloc(mp, &M_RES(mp)->tr_growrtzero,
					0, 0, 0, &tp);
			if (error)
				return error;
			 
			xfs_ilock(ip, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			 
			d = XFS_FSB_TO_DADDR(mp, fsbno);
			error = xfs_trans_get_buf(tp, mp->m_ddev_targp, d,
					mp->m_bsize, 0, &bp);
			if (error)
				goto out_trans_cancel;

			xfs_trans_buf_set_type(tp, bp, buf_type);
			bp->b_ops = &xfs_rtbuf_ops;
			memset(bp->b_addr, 0, mp->m_sb.sb_blocksize);
			xfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);
			 
			error = xfs_trans_commit(tp);
			if (error)
				return error;
		}
		 
		oblocks = map.br_startoff + map.br_blockcount;
	}

	return 0;

out_trans_cancel:
	xfs_trans_cancel(tp);
	return error;
}

static void
xfs_alloc_rsum_cache(
	xfs_mount_t	*mp,		 
	xfs_extlen_t	rbmblocks)	 
{
	 
	mp->m_rsum_cache = kvzalloc(rbmblocks, GFP_KERNEL);
	if (!mp->m_rsum_cache)
		xfs_warn(mp, "could not allocate realtime summary cache");
}

 

 
int
xfs_growfs_rt(
	xfs_mount_t	*mp,		 
	xfs_growfs_rt_t	*in)		 
{
	xfs_rtblock_t	bmbno;		 
	struct xfs_buf	*bp;		 
	int		error;		 
	xfs_mount_t	*nmp;		 
	xfs_rfsblock_t	nrblocks;	 
	xfs_extlen_t	nrbmblocks;	 
	xfs_rtblock_t	nrextents;	 
	uint8_t		nrextslog;	 
	xfs_extlen_t	nrsumblocks;	 
	uint		nrsumlevels;	 
	uint		nrsumsize;	 
	xfs_sb_t	*nsbp;		 
	xfs_extlen_t	rbmblocks;	 
	xfs_extlen_t	rsumblocks;	 
	xfs_sb_t	*sbp;		 
	xfs_fsblock_t	sumbno;		 
	uint8_t		*rsum_cache;	 

	sbp = &mp->m_sb;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	 
	if (!XFS_IS_REALTIME_MOUNT(mp))
		return -EINVAL;
	 
	if (!mp->m_rbmip || !mp->m_rsumip)
		return -EINVAL;

	 
	if (in->newblocks <= sbp->sb_rblocks)
		return -EINVAL;

	 
	if (sbp->sb_rblocks > 0 && in->extsize != sbp->sb_rextsize)
		return -EINVAL;

	 
	if (XFS_FSB_TO_B(mp, in->extsize) > XFS_MAX_RTEXTSIZE ||
	    XFS_FSB_TO_B(mp, in->extsize) < XFS_MIN_RTEXTSIZE)
		return -EINVAL;

	 
	if (xfs_has_rmapbt(mp) || xfs_has_reflink(mp))
		return -EOPNOTSUPP;

	nrblocks = in->newblocks;
	error = xfs_sb_validate_fsb_count(sbp, nrblocks);
	if (error)
		return error;
	 
	error = xfs_buf_read_uncached(mp->m_rtdev_targp,
				XFS_FSB_TO_BB(mp, nrblocks - 1),
				XFS_FSB_TO_BB(mp, 1), 0, &bp, NULL);
	if (error)
		return error;
	xfs_buf_relse(bp);

	 
	nrextents = nrblocks;
	do_div(nrextents, in->extsize);
	nrbmblocks = howmany_64(nrextents, NBBY * sbp->sb_blocksize);
	nrextslog = xfs_highbit32(nrextents);
	nrsumlevels = nrextslog + 1;
	nrsumsize = (uint)sizeof(xfs_suminfo_t) * nrsumlevels * nrbmblocks;
	nrsumblocks = XFS_B_TO_FSB(mp, nrsumsize);
	nrsumsize = XFS_FSB_TO_B(mp, nrsumblocks);
	 
	if (nrsumblocks > (mp->m_sb.sb_logblocks >> 1))
		return -EINVAL;
	 
	rbmblocks = XFS_B_TO_FSB(mp, mp->m_rbmip->i_disk_size);
	rsumblocks = XFS_B_TO_FSB(mp, mp->m_rsumip->i_disk_size);
	 
	error = xfs_growfs_rt_alloc(mp, rbmblocks, nrbmblocks, mp->m_rbmip);
	if (error)
		return error;
	error = xfs_growfs_rt_alloc(mp, rsumblocks, nrsumblocks, mp->m_rsumip);
	if (error)
		return error;

	rsum_cache = mp->m_rsum_cache;
	if (nrbmblocks != sbp->sb_rbmblocks)
		xfs_alloc_rsum_cache(mp, nrbmblocks);

	 
	nmp = kmem_alloc(sizeof(*nmp), 0);
	 
	for (bmbno = sbp->sb_rbmblocks -
		     ((sbp->sb_rextents & ((1 << mp->m_blkbit_log) - 1)) != 0);
	     bmbno < nrbmblocks;
	     bmbno++) {
		struct xfs_trans	*tp;
		xfs_rfsblock_t		nrblocks_step;

		*nmp = *mp;
		nsbp = &nmp->m_sb;
		 
		nsbp->sb_rextsize = in->extsize;
		nsbp->sb_rbmblocks = bmbno + 1;
		nrblocks_step = (bmbno + 1) * NBBY * nsbp->sb_blocksize *
				nsbp->sb_rextsize;
		nsbp->sb_rblocks = min(nrblocks, nrblocks_step);
		nsbp->sb_rextents = nsbp->sb_rblocks;
		do_div(nsbp->sb_rextents, nsbp->sb_rextsize);
		ASSERT(nsbp->sb_rextents != 0);
		nsbp->sb_rextslog = xfs_highbit32(nsbp->sb_rextents);
		nrsumlevels = nmp->m_rsumlevels = nsbp->sb_rextslog + 1;
		nrsumsize =
			(uint)sizeof(xfs_suminfo_t) * nrsumlevels *
			nsbp->sb_rbmblocks;
		nrsumblocks = XFS_B_TO_FSB(mp, nrsumsize);
		nmp->m_rsumsize = nrsumsize = XFS_FSB_TO_B(mp, nrsumblocks);
		 
		error = xfs_trans_alloc(mp, &M_RES(mp)->tr_growrtfree, 0, 0, 0,
				&tp);
		if (error)
			break;
		 
		xfs_ilock(mp->m_rbmip, XFS_ILOCK_EXCL | XFS_ILOCK_RTBITMAP);
		xfs_trans_ijoin(tp, mp->m_rbmip, XFS_ILOCK_EXCL);
		 
		mp->m_rbmip->i_disk_size =
			nsbp->sb_rbmblocks * nsbp->sb_blocksize;
		i_size_write(VFS_I(mp->m_rbmip), mp->m_rbmip->i_disk_size);
		xfs_trans_log_inode(tp, mp->m_rbmip, XFS_ILOG_CORE);
		 
		xfs_ilock(mp->m_rsumip, XFS_ILOCK_EXCL | XFS_ILOCK_RTSUM);
		xfs_trans_ijoin(tp, mp->m_rsumip, XFS_ILOCK_EXCL);
		 
		mp->m_rsumip->i_disk_size = nmp->m_rsumsize;
		i_size_write(VFS_I(mp->m_rsumip), mp->m_rsumip->i_disk_size);
		xfs_trans_log_inode(tp, mp->m_rsumip, XFS_ILOG_CORE);
		 
		if (sbp->sb_rbmblocks != nsbp->sb_rbmblocks ||
		    mp->m_rsumlevels != nmp->m_rsumlevels) {
			error = xfs_rtcopy_summary(mp, nmp, tp);
			if (error)
				goto error_cancel;
		}
		 
		if (nsbp->sb_rextsize != sbp->sb_rextsize)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_REXTSIZE,
				nsbp->sb_rextsize - sbp->sb_rextsize);
		if (nsbp->sb_rbmblocks != sbp->sb_rbmblocks)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_RBMBLOCKS,
				nsbp->sb_rbmblocks - sbp->sb_rbmblocks);
		if (nsbp->sb_rblocks != sbp->sb_rblocks)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_RBLOCKS,
				nsbp->sb_rblocks - sbp->sb_rblocks);
		if (nsbp->sb_rextents != sbp->sb_rextents)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_REXTENTS,
				nsbp->sb_rextents - sbp->sb_rextents);
		if (nsbp->sb_rextslog != sbp->sb_rextslog)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_REXTSLOG,
				nsbp->sb_rextslog - sbp->sb_rextslog);
		 
		bp = NULL;
		error = xfs_rtfree_range(nmp, tp, sbp->sb_rextents,
			nsbp->sb_rextents - sbp->sb_rextents, &bp, &sumbno);
		if (error) {
error_cancel:
			xfs_trans_cancel(tp);
			break;
		}
		 
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_FREXTENTS,
			nsbp->sb_rextents - sbp->sb_rextents);
		 
		mp->m_rsumlevels = nrsumlevels;
		mp->m_rsumsize = nrsumsize;

		error = xfs_trans_commit(tp);
		if (error)
			break;

		 
		mp->m_features |= XFS_FEAT_REALTIME;
	}
	if (error)
		goto out_free;

	 
	error = xfs_update_secondary_sbs(mp);

out_free:
	 
	kmem_free(nmp);

	 
	if (rsum_cache != mp->m_rsum_cache) {
		if (error) {
			kmem_free(mp->m_rsum_cache);
			mp->m_rsum_cache = rsum_cache;
		} else {
			kmem_free(rsum_cache);
		}
	}

	return error;
}

 
int					 
xfs_rtallocate_extent(
	xfs_trans_t	*tp,		 
	xfs_rtblock_t	bno,		 
	xfs_extlen_t	minlen,		 
	xfs_extlen_t	maxlen,		 
	xfs_extlen_t	*len,		 
	int		wasdel,		 
	xfs_extlen_t	prod,		 
	xfs_rtblock_t	*rtblock)	 
{
	xfs_mount_t	*mp = tp->t_mountp;
	int		error;		 
	xfs_rtblock_t	r;		 
	xfs_fsblock_t	sb;		 
	struct xfs_buf	*sumbp;		 

	ASSERT(xfs_isilocked(mp->m_rbmip, XFS_ILOCK_EXCL));
	ASSERT(minlen > 0 && minlen <= maxlen);

	 
	if (prod > 1) {
		xfs_extlen_t	i;

		if ((i = maxlen % prod))
			maxlen -= i;
		if ((i = minlen % prod))
			minlen += prod - i;
		if (maxlen < minlen) {
			*rtblock = NULLRTBLOCK;
			return 0;
		}
	}

retry:
	sumbp = NULL;
	if (bno == 0) {
		error = xfs_rtallocate_extent_size(mp, tp, minlen, maxlen, len,
				&sumbp,	&sb, prod, &r);
	} else {
		error = xfs_rtallocate_extent_near(mp, tp, bno, minlen, maxlen,
				len, &sumbp, &sb, prod, &r);
	}

	if (error)
		return error;

	 
	if (r != NULLRTBLOCK) {
		long	slen = (long)*len;

		ASSERT(*len >= minlen && *len <= maxlen);
		if (wasdel)
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_RES_FREXTENTS, -slen);
		else
			xfs_trans_mod_sb(tp, XFS_TRANS_SB_FREXTENTS, -slen);
	} else if (prod > 1) {
		prod = 1;
		goto retry;
	}

	*rtblock = r;
	return 0;
}

 
int				 
xfs_rtmount_init(
	struct xfs_mount	*mp)	 
{
	struct xfs_buf		*bp;	 
	struct xfs_sb		*sbp;	 
	xfs_daddr_t		d;	 
	int			error;

	sbp = &mp->m_sb;
	if (sbp->sb_rblocks == 0)
		return 0;
	if (mp->m_rtdev_targp == NULL) {
		xfs_warn(mp,
	"Filesystem has a realtime volume, use rtdev=device option");
		return -ENODEV;
	}
	mp->m_rsumlevels = sbp->sb_rextslog + 1;
	mp->m_rsumsize =
		(uint)sizeof(xfs_suminfo_t) * mp->m_rsumlevels *
		sbp->sb_rbmblocks;
	mp->m_rsumsize = roundup(mp->m_rsumsize, sbp->sb_blocksize);
	mp->m_rbmip = mp->m_rsumip = NULL;
	 
	d = (xfs_daddr_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_rblocks);
	if (XFS_BB_TO_FSB(mp, d) != mp->m_sb.sb_rblocks) {
		xfs_warn(mp, "realtime mount -- %llu != %llu",
			(unsigned long long) XFS_BB_TO_FSB(mp, d),
			(unsigned long long) mp->m_sb.sb_rblocks);
		return -EFBIG;
	}
	error = xfs_buf_read_uncached(mp->m_rtdev_targp,
					d - XFS_FSB_TO_BB(mp, 1),
					XFS_FSB_TO_BB(mp, 1), 0, &bp, NULL);
	if (error) {
		xfs_warn(mp, "realtime device size check failed");
		return error;
	}
	xfs_buf_relse(bp);
	return 0;
}

static int
xfs_rtalloc_count_frextent(
	struct xfs_mount		*mp,
	struct xfs_trans		*tp,
	const struct xfs_rtalloc_rec	*rec,
	void				*priv)
{
	uint64_t			*valp = priv;

	*valp += rec->ar_extcount;
	return 0;
}

 
int
xfs_rtalloc_reinit_frextents(
	struct xfs_mount	*mp)
{
	uint64_t		val = 0;
	int			error;

	xfs_ilock(mp->m_rbmip, XFS_ILOCK_SHARED | XFS_ILOCK_RTBITMAP);
	error = xfs_rtalloc_query_all(mp, NULL, xfs_rtalloc_count_frextent,
			&val);
	xfs_iunlock(mp->m_rbmip, XFS_ILOCK_SHARED | XFS_ILOCK_RTBITMAP);
	if (error)
		return error;

	spin_lock(&mp->m_sb_lock);
	mp->m_sb.sb_frextents = val;
	spin_unlock(&mp->m_sb_lock);
	percpu_counter_set(&mp->m_frextents, mp->m_sb.sb_frextents);
	return 0;
}

 
static inline int
xfs_rtmount_iread_extents(
	struct xfs_inode	*ip,
	unsigned int		lock_class)
{
	struct xfs_trans	*tp;
	int			error;

	error = xfs_trans_alloc_empty(ip->i_mount, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL | lock_class);

	error = xfs_iread_extents(tp, ip, XFS_DATA_FORK);
	if (error)
		goto out_unlock;

	if (xfs_inode_has_attr_fork(ip)) {
		error = xfs_iread_extents(tp, ip, XFS_ATTR_FORK);
		if (error)
			goto out_unlock;
	}

out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL | lock_class);
	xfs_trans_cancel(tp);
	return error;
}

 
int					 
xfs_rtmount_inodes(
	xfs_mount_t	*mp)		 
{
	int		error;		 
	xfs_sb_t	*sbp;

	sbp = &mp->m_sb;
	error = xfs_iget(mp, NULL, sbp->sb_rbmino, 0, 0, &mp->m_rbmip);
	if (error)
		return error;
	ASSERT(mp->m_rbmip != NULL);

	error = xfs_rtmount_iread_extents(mp->m_rbmip, XFS_ILOCK_RTBITMAP);
	if (error)
		goto out_rele_bitmap;

	error = xfs_iget(mp, NULL, sbp->sb_rsumino, 0, 0, &mp->m_rsumip);
	if (error)
		goto out_rele_bitmap;
	ASSERT(mp->m_rsumip != NULL);

	error = xfs_rtmount_iread_extents(mp->m_rsumip, XFS_ILOCK_RTSUM);
	if (error)
		goto out_rele_summary;

	xfs_alloc_rsum_cache(mp, sbp->sb_rbmblocks);
	return 0;

out_rele_summary:
	xfs_irele(mp->m_rsumip);
out_rele_bitmap:
	xfs_irele(mp->m_rbmip);
	return error;
}

void
xfs_rtunmount_inodes(
	struct xfs_mount	*mp)
{
	kmem_free(mp->m_rsum_cache);
	if (mp->m_rbmip)
		xfs_irele(mp->m_rbmip);
	if (mp->m_rsumip)
		xfs_irele(mp->m_rsumip);
}

 
int					 
xfs_rtpick_extent(
	xfs_mount_t	*mp,		 
	xfs_trans_t	*tp,		 
	xfs_extlen_t	len,		 
	xfs_rtblock_t	*pick)		 
{
	xfs_rtblock_t	b;		 
	int		log2;		 
	uint64_t	resid;		 
	uint64_t	seq;		 
	uint64_t	*seqp;		 

	ASSERT(xfs_isilocked(mp->m_rbmip, XFS_ILOCK_EXCL));

	seqp = (uint64_t *)&VFS_I(mp->m_rbmip)->i_atime;
	if (!(mp->m_rbmip->i_diflags & XFS_DIFLAG_NEWRTBM)) {
		mp->m_rbmip->i_diflags |= XFS_DIFLAG_NEWRTBM;
		*seqp = 0;
	}
	seq = *seqp;
	if ((log2 = xfs_highbit64(seq)) == -1)
		b = 0;
	else {
		resid = seq - (1ULL << log2);
		b = (mp->m_sb.sb_rextents * ((resid << 1) + 1ULL)) >>
		    (log2 + 1);
		if (b >= mp->m_sb.sb_rextents)
			div64_u64_rem(b, mp->m_sb.sb_rextents, &b);
		if (b + len > mp->m_sb.sb_rextents)
			b = mp->m_sb.sb_rextents - len;
	}
	*seqp = seq + 1;
	xfs_trans_log_inode(tp, mp->m_rbmip, XFS_ILOG_CORE);
	*pick = b;
	return 0;
}

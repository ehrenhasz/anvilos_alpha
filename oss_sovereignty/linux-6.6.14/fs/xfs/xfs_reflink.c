
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_refcount.h"
#include "xfs_bmap_btree.h"
#include "xfs_trans_space.h"
#include "xfs_bit.h"
#include "xfs_alloc.h"
#include "xfs_quota.h"
#include "xfs_reflink.h"
#include "xfs_iomap.h"
#include "xfs_ag.h"
#include "xfs_ag_resv.h"

 

 
static int
xfs_reflink_find_shared(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	xfs_agblock_t		agbno,
	xfs_extlen_t		aglen,
	xfs_agblock_t		*fbno,
	xfs_extlen_t		*flen,
	bool			find_end_of_shared)
{
	struct xfs_buf		*agbp;
	struct xfs_btree_cur	*cur;
	int			error;

	error = xfs_alloc_read_agf(pag, tp, 0, &agbp);
	if (error)
		return error;

	cur = xfs_refcountbt_init_cursor(pag->pag_mount, tp, agbp, pag);

	error = xfs_refcount_find_shared(cur, agbno, aglen, fbno, flen,
			find_end_of_shared);

	xfs_btree_del_cursor(cur, error);

	xfs_trans_brelse(tp, agbp);
	return error;
}

 
int
xfs_reflink_trim_around_shared(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*irec,
	bool			*shared)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_perag	*pag;
	xfs_agblock_t		agbno;
	xfs_extlen_t		aglen;
	xfs_agblock_t		fbno;
	xfs_extlen_t		flen;
	int			error = 0;

	 
	if (!xfs_is_cow_inode(ip) || !xfs_bmap_is_written_extent(irec)) {
		*shared = false;
		return 0;
	}

	trace_xfs_reflink_trim_around_shared(ip, irec);

	pag = xfs_perag_get(mp, XFS_FSB_TO_AGNO(mp, irec->br_startblock));
	agbno = XFS_FSB_TO_AGBNO(mp, irec->br_startblock);
	aglen = irec->br_blockcount;

	error = xfs_reflink_find_shared(pag, NULL, agbno, aglen, &fbno, &flen,
			true);
	xfs_perag_put(pag);
	if (error)
		return error;

	*shared = false;
	if (fbno == NULLAGBLOCK) {
		 
		return 0;
	}

	if (fbno == agbno) {
		 
		irec->br_blockcount = flen;
		*shared = true;
		return 0;
	}

	 
	irec->br_blockcount = fbno - agbno;
	return 0;
}

int
xfs_bmap_trim_cow(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	bool			*shared)
{
	 
	if (xfs_is_always_cow_inode(ip) &&
	    !isnullstartblock(imap->br_startblock)) {
		*shared = true;
		return 0;
	}

	 
	return xfs_reflink_trim_around_shared(ip, imap, shared);
}

static int
xfs_reflink_convert_cow_locked(
	struct xfs_inode	*ip,
	xfs_fileoff_t		offset_fsb,
	xfs_filblks_t		count_fsb)
{
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	got;
	struct xfs_btree_cur	*dummy_cur = NULL;
	int			dummy_logflags;
	int			error = 0;

	if (!xfs_iext_lookup_extent(ip, ip->i_cowfp, offset_fsb, &icur, &got))
		return 0;

	do {
		if (got.br_startoff >= offset_fsb + count_fsb)
			break;
		if (got.br_state == XFS_EXT_NORM)
			continue;
		if (WARN_ON_ONCE(isnullstartblock(got.br_startblock)))
			return -EIO;

		xfs_trim_extent(&got, offset_fsb, count_fsb);
		if (!got.br_blockcount)
			continue;

		got.br_state = XFS_EXT_NORM;
		error = xfs_bmap_add_extent_unwritten_real(NULL, ip,
				XFS_COW_FORK, &icur, &dummy_cur, &got,
				&dummy_logflags);
		if (error)
			return error;
	} while (xfs_iext_next_extent(ip->i_cowfp, &icur, &got));

	return error;
}

 
int
xfs_reflink_convert_cow(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		count)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		offset_fsb = XFS_B_TO_FSBT(mp, offset);
	xfs_fileoff_t		end_fsb = XFS_B_TO_FSB(mp, offset + count);
	xfs_filblks_t		count_fsb = end_fsb - offset_fsb;
	int			error;

	ASSERT(count != 0);

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	error = xfs_reflink_convert_cow_locked(ip, offset_fsb, count_fsb);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
static int
xfs_find_trim_cow_extent(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	bool			*found)
{
	xfs_fileoff_t		offset_fsb = imap->br_startoff;
	xfs_filblks_t		count_fsb = imap->br_blockcount;
	struct xfs_iext_cursor	icur;

	*found = false;

	 
	if (!xfs_iext_lookup_extent(ip, ip->i_cowfp, offset_fsb, &icur, cmap))
		cmap->br_startoff = offset_fsb + count_fsb;
	if (cmap->br_startoff > offset_fsb) {
		xfs_trim_extent(imap, imap->br_startoff,
				cmap->br_startoff - imap->br_startoff);
		return xfs_bmap_trim_cow(ip, imap, shared);
	}

	*shared = true;
	if (isnullstartblock(cmap->br_startblock)) {
		xfs_trim_extent(imap, cmap->br_startoff, cmap->br_blockcount);
		return 0;
	}

	 
	xfs_trim_extent(cmap, offset_fsb, count_fsb);
	*found = true;
	return 0;
}

static int
xfs_reflink_convert_unwritten(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			convert_now)
{
	xfs_fileoff_t		offset_fsb = imap->br_startoff;
	xfs_filblks_t		count_fsb = imap->br_blockcount;
	int			error;

	 
	xfs_trim_extent(cmap, offset_fsb, count_fsb);

	 
	if (!convert_now || cmap->br_state == XFS_EXT_NORM)
		return 0;

	trace_xfs_reflink_convert_cow(ip, cmap);

	error = xfs_reflink_convert_cow_locked(ip, offset_fsb, count_fsb);
	if (!error)
		cmap->br_state = XFS_EXT_NORM;

	return error;
}

static int
xfs_reflink_fill_cow_hole(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	uint			*lockmode,
	bool			convert_now)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	xfs_filblks_t		resaligned;
	xfs_extlen_t		resblks;
	int			nimaps;
	int			error;
	bool			found;

	resaligned = xfs_aligned_fsb_count(imap->br_startoff,
		imap->br_blockcount, xfs_get_cowextsz_hint(ip));
	resblks = XFS_DIOSTRAT_SPACE_RES(mp, resaligned);

	xfs_iunlock(ip, *lockmode);
	*lockmode = 0;

	error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write, resblks, 0,
			false, &tp);
	if (error)
		return error;

	*lockmode = XFS_ILOCK_EXCL;

	error = xfs_find_trim_cow_extent(ip, imap, cmap, shared, &found);
	if (error || !*shared)
		goto out_trans_cancel;

	if (found) {
		xfs_trans_cancel(tp);
		goto convert;
	}

	 
	nimaps = 1;
	error = xfs_bmapi_write(tp, ip, imap->br_startoff, imap->br_blockcount,
			XFS_BMAPI_COWFORK | XFS_BMAPI_PREALLOC, 0, cmap,
			&nimaps);
	if (error)
		goto out_trans_cancel;

	xfs_inode_set_cowblocks_tag(ip);
	error = xfs_trans_commit(tp);
	if (error)
		return error;

	 
	if (nimaps == 0)
		return -ENOSPC;

convert:
	return xfs_reflink_convert_unwritten(ip, imap, cmap, convert_now);

out_trans_cancel:
	xfs_trans_cancel(tp);
	return error;
}

static int
xfs_reflink_fill_delalloc(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	uint			*lockmode,
	bool			convert_now)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			nimaps;
	int			error;
	bool			found;

	do {
		xfs_iunlock(ip, *lockmode);
		*lockmode = 0;

		error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write, 0, 0,
				false, &tp);
		if (error)
			return error;

		*lockmode = XFS_ILOCK_EXCL;

		error = xfs_find_trim_cow_extent(ip, imap, cmap, shared,
				&found);
		if (error || !*shared)
			goto out_trans_cancel;

		if (found) {
			xfs_trans_cancel(tp);
			break;
		}

		ASSERT(isnullstartblock(cmap->br_startblock) ||
		       cmap->br_startblock == DELAYSTARTBLOCK);

		 
		nimaps = 1;
		error = xfs_bmapi_write(tp, ip, cmap->br_startoff,
				cmap->br_blockcount,
				XFS_BMAPI_COWFORK | XFS_BMAPI_PREALLOC, 0,
				cmap, &nimaps);
		if (error)
			goto out_trans_cancel;

		xfs_inode_set_cowblocks_tag(ip);
		error = xfs_trans_commit(tp);
		if (error)
			return error;

		 
		if (nimaps == 0)
			return -ENOSPC;
	} while (cmap->br_startoff + cmap->br_blockcount <= imap->br_startoff);

	return xfs_reflink_convert_unwritten(ip, imap, cmap, convert_now);

out_trans_cancel:
	xfs_trans_cancel(tp);
	return error;
}

 
int
xfs_reflink_allocate_cow(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*imap,
	struct xfs_bmbt_irec	*cmap,
	bool			*shared,
	uint			*lockmode,
	bool			convert_now)
{
	int			error;
	bool			found;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	if (!ip->i_cowfp) {
		ASSERT(!xfs_is_reflink_inode(ip));
		xfs_ifork_init_cow(ip);
	}

	error = xfs_find_trim_cow_extent(ip, imap, cmap, shared, &found);
	if (error || !*shared)
		return error;

	 
	if (found)
		return xfs_reflink_convert_unwritten(ip, imap, cmap,
				convert_now);

	 
	if (cmap->br_startoff > imap->br_startoff)
		return xfs_reflink_fill_cow_hole(ip, imap, cmap, shared,
				lockmode, convert_now);

	 
	if (isnullstartblock(cmap->br_startblock) ||
	    cmap->br_startblock == DELAYSTARTBLOCK)
		return xfs_reflink_fill_delalloc(ip, imap, cmap, shared,
				lockmode, convert_now);

	 
	ASSERT(0);
	return -EFSCORRUPTED;
}

 
int
xfs_reflink_cancel_cow_blocks(
	struct xfs_inode		*ip,
	struct xfs_trans		**tpp,
	xfs_fileoff_t			offset_fsb,
	xfs_fileoff_t			end_fsb,
	bool				cancel_real)
{
	struct xfs_ifork		*ifp = xfs_ifork_ptr(ip, XFS_COW_FORK);
	struct xfs_bmbt_irec		got, del;
	struct xfs_iext_cursor		icur;
	int				error = 0;

	if (!xfs_inode_has_cow_data(ip))
		return 0;
	if (!xfs_iext_lookup_extent_before(ip, ifp, &end_fsb, &icur, &got))
		return 0;

	 
	while (got.br_startoff + got.br_blockcount > offset_fsb) {
		del = got;
		xfs_trim_extent(&del, offset_fsb, end_fsb - offset_fsb);

		 
		if (!del.br_blockcount) {
			xfs_iext_prev(ifp, &icur);
			goto next_extent;
		}

		trace_xfs_reflink_cancel_cow(ip, &del);

		if (isnullstartblock(del.br_startblock)) {
			error = xfs_bmap_del_extent_delay(ip, XFS_COW_FORK,
					&icur, &got, &del);
			if (error)
				break;
		} else if (del.br_state == XFS_EXT_UNWRITTEN || cancel_real) {
			ASSERT((*tpp)->t_highest_agno == NULLAGNUMBER);

			 
			xfs_refcount_free_cow_extent(*tpp, del.br_startblock,
					del.br_blockcount);

			error = xfs_free_extent_later(*tpp, del.br_startblock,
					del.br_blockcount, NULL,
					XFS_AG_RESV_NONE);
			if (error)
				break;

			 
			error = xfs_defer_finish(tpp);
			if (error)
				break;

			 
			xfs_bmap_del_extent_cow(ip, &icur, &got, &del);

			 
			error = xfs_quota_unreserve_blkres(ip,
					del.br_blockcount);
			if (error)
				break;
		} else {
			 
			xfs_iext_prev(ifp, &icur);
		}
next_extent:
		if (!xfs_iext_get_extent(ifp, &icur, &got))
			break;
	}

	 
	if (!ifp->if_bytes)
		xfs_inode_clear_cowblocks_tag(ip);
	return error;
}

 
int
xfs_reflink_cancel_cow_range(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		count,
	bool			cancel_real)
{
	struct xfs_trans	*tp;
	xfs_fileoff_t		offset_fsb;
	xfs_fileoff_t		end_fsb;
	int			error;

	trace_xfs_reflink_cancel_cow_range(ip, offset, count);
	ASSERT(ip->i_cowfp);

	offset_fsb = XFS_B_TO_FSBT(ip->i_mount, offset);
	if (count == NULLFILEOFF)
		end_fsb = NULLFILEOFF;
	else
		end_fsb = XFS_B_TO_FSB(ip->i_mount, offset + count);

	 
	error = xfs_trans_alloc(ip->i_mount, &M_RES(ip->i_mount)->tr_write,
			0, 0, 0, &tp);
	if (error)
		goto out;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	 
	error = xfs_reflink_cancel_cow_blocks(ip, &tp, offset_fsb, end_fsb,
			cancel_real);
	if (error)
		goto out_cancel;

	error = xfs_trans_commit(tp);

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

out_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
out:
	trace_xfs_reflink_cancel_cow_range_error(ip, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_reflink_end_cow_extent(
	struct xfs_inode	*ip,
	xfs_fileoff_t		*offset_fsb,
	xfs_fileoff_t		end_fsb)
{
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	got, del, data;
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, XFS_COW_FORK);
	unsigned int		resblks;
	int			nmaps;
	int			error;

	 
	if (ifp->if_bytes == 0) {
		*offset_fsb = end_fsb;
		return 0;
	}

	resblks = XFS_EXTENTADD_SPACE_RES(mp, XFS_DATA_FORK);
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, resblks, 0,
			XFS_TRANS_RESERVE, &tp);
	if (error)
		return error;

	 
	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_iext_count_may_overflow(ip, XFS_DATA_FORK,
			XFS_IEXT_REFLINK_END_COW_CNT);
	if (error == -EFBIG)
		error = xfs_iext_count_upgrade(tp, ip,
				XFS_IEXT_REFLINK_END_COW_CNT);
	if (error)
		goto out_cancel;

	 
	if (!xfs_iext_lookup_extent(ip, ifp, *offset_fsb, &icur, &got) ||
	    got.br_startoff >= end_fsb) {
		*offset_fsb = end_fsb;
		goto out_cancel;
	}

	 
	while (!xfs_bmap_is_written_extent(&got)) {
		if (!xfs_iext_next_extent(ifp, &icur, &got) ||
		    got.br_startoff >= end_fsb) {
			*offset_fsb = end_fsb;
			goto out_cancel;
		}
	}
	del = got;

	 
	nmaps = 1;
	error = xfs_bmapi_read(ip, del.br_startoff, del.br_blockcount, &data,
			&nmaps, 0);
	if (error)
		goto out_cancel;

	 
	data.br_blockcount = min(data.br_blockcount, del.br_blockcount);
	del.br_blockcount = data.br_blockcount;

	trace_xfs_reflink_cow_remap_from(ip, &del);
	trace_xfs_reflink_cow_remap_to(ip, &data);

	if (xfs_bmap_is_real_extent(&data)) {
		 
		xfs_bmap_unmap_extent(tp, ip, &data);
		xfs_refcount_decrease_extent(tp, &data);
		xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT,
				-data.br_blockcount);
	} else if (data.br_startblock == DELAYSTARTBLOCK) {
		int		done;

		 
		error = xfs_bunmapi(NULL, ip, data.br_startoff,
				data.br_blockcount, 0, 1, &done);
		if (error)
			goto out_cancel;
		ASSERT(done);
	}

	 
	xfs_refcount_free_cow_extent(tp, del.br_startblock, del.br_blockcount);

	 
	xfs_bmap_map_extent(tp, ip, &del);

	 
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_DELBCOUNT,
			(long)del.br_blockcount);

	 
	xfs_bmap_del_extent_cow(ip, &icur, &got, &del);

	error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	if (error)
		return error;

	 
	*offset_fsb = del.br_startoff + del.br_blockcount;
	return 0;

out_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
int
xfs_reflink_end_cow(
	struct xfs_inode		*ip,
	xfs_off_t			offset,
	xfs_off_t			count)
{
	xfs_fileoff_t			offset_fsb;
	xfs_fileoff_t			end_fsb;
	int				error = 0;

	trace_xfs_reflink_end_cow(ip, offset, count);

	offset_fsb = XFS_B_TO_FSBT(ip->i_mount, offset);
	end_fsb = XFS_B_TO_FSB(ip->i_mount, offset + count);

	 
	while (end_fsb > offset_fsb && !error)
		error = xfs_reflink_end_cow_extent(ip, &offset_fsb, end_fsb);

	if (error)
		trace_xfs_reflink_end_cow_error(ip, error, _RET_IP_);
	return error;
}

 
int
xfs_reflink_recover_cow(
	struct xfs_mount	*mp)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;
	int			error = 0;

	if (!xfs_has_reflink(mp))
		return 0;

	for_each_perag(mp, agno, pag) {
		error = xfs_refcount_recover_cow_leftovers(mp, pag);
		if (error) {
			xfs_perag_rele(pag);
			break;
		}
	}

	return error;
}

 

 
STATIC int
xfs_reflink_set_inode_flag(
	struct xfs_inode	*src,
	struct xfs_inode	*dest)
{
	struct xfs_mount	*mp = src->i_mount;
	int			error;
	struct xfs_trans	*tp;

	if (xfs_is_reflink_inode(src) && xfs_is_reflink_inode(dest))
		return 0;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ichange, 0, 0, 0, &tp);
	if (error)
		goto out_error;

	 
	if (src->i_ino == dest->i_ino)
		xfs_ilock(src, XFS_ILOCK_EXCL);
	else
		xfs_lock_two_inodes(src, XFS_ILOCK_EXCL, dest, XFS_ILOCK_EXCL);

	if (!xfs_is_reflink_inode(src)) {
		trace_xfs_reflink_set_inode_flag(src);
		xfs_trans_ijoin(tp, src, XFS_ILOCK_EXCL);
		src->i_diflags2 |= XFS_DIFLAG2_REFLINK;
		xfs_trans_log_inode(tp, src, XFS_ILOG_CORE);
		xfs_ifork_init_cow(src);
	} else
		xfs_iunlock(src, XFS_ILOCK_EXCL);

	if (src->i_ino == dest->i_ino)
		goto commit_flags;

	if (!xfs_is_reflink_inode(dest)) {
		trace_xfs_reflink_set_inode_flag(dest);
		xfs_trans_ijoin(tp, dest, XFS_ILOCK_EXCL);
		dest->i_diflags2 |= XFS_DIFLAG2_REFLINK;
		xfs_trans_log_inode(tp, dest, XFS_ILOG_CORE);
		xfs_ifork_init_cow(dest);
	} else
		xfs_iunlock(dest, XFS_ILOCK_EXCL);

commit_flags:
	error = xfs_trans_commit(tp);
	if (error)
		goto out_error;
	return error;

out_error:
	trace_xfs_reflink_set_inode_flag_error(dest, error, _RET_IP_);
	return error;
}

 
int
xfs_reflink_update_dest(
	struct xfs_inode	*dest,
	xfs_off_t		newlen,
	xfs_extlen_t		cowextsize,
	unsigned int		remap_flags)
{
	struct xfs_mount	*mp = dest->i_mount;
	struct xfs_trans	*tp;
	int			error;

	if (newlen <= i_size_read(VFS_I(dest)) && cowextsize == 0)
		return 0;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_ichange, 0, 0, 0, &tp);
	if (error)
		goto out_error;

	xfs_ilock(dest, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, dest, XFS_ILOCK_EXCL);

	if (newlen > i_size_read(VFS_I(dest))) {
		trace_xfs_reflink_update_inode_size(dest, newlen);
		i_size_write(VFS_I(dest), newlen);
		dest->i_disk_size = newlen;
	}

	if (cowextsize) {
		dest->i_cowextsize = cowextsize;
		dest->i_diflags2 |= XFS_DIFLAG2_COWEXTSIZE;
	}

	xfs_trans_log_inode(tp, dest, XFS_ILOG_CORE);

	error = xfs_trans_commit(tp);
	if (error)
		goto out_error;
	return error;

out_error:
	trace_xfs_reflink_update_inode_size_error(dest, error, _RET_IP_);
	return error;
}

 
static int
xfs_reflink_ag_has_free_space(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno)
{
	struct xfs_perag	*pag;
	int			error = 0;

	if (!xfs_has_rmapbt(mp))
		return 0;

	pag = xfs_perag_get(mp, agno);
	if (xfs_ag_resv_critical(pag, XFS_AG_RESV_RMAPBT) ||
	    xfs_ag_resv_critical(pag, XFS_AG_RESV_METADATA))
		error = -ENOSPC;
	xfs_perag_put(pag);
	return error;
}

 
STATIC int
xfs_reflink_remap_extent(
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*dmap,
	xfs_off_t		new_isize)
{
	struct xfs_bmbt_irec	smap;
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	xfs_off_t		newlen;
	int64_t			qdelta = 0;
	unsigned int		resblks;
	bool			quota_reserved = true;
	bool			smap_real;
	bool			dmap_written = xfs_bmap_is_written_extent(dmap);
	int			iext_delta = 0;
	int			nimaps;
	int			error;

	 
	resblks = XFS_EXTENTADD_SPACE_RES(mp, XFS_DATA_FORK);
	error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write,
			resblks + dmap->br_blockcount, 0, false, &tp);
	if (error == -EDQUOT || error == -ENOSPC) {
		quota_reserved = false;
		error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write,
				resblks, 0, false, &tp);
	}
	if (error)
		goto out;

	 
	nimaps = 1;
	error = xfs_bmapi_read(ip, dmap->br_startoff, dmap->br_blockcount,
			&smap, &nimaps, 0);
	if (error)
		goto out_cancel;
	ASSERT(nimaps == 1 && smap.br_startoff == dmap->br_startoff);
	smap_real = xfs_bmap_is_real_extent(&smap);

	 
	dmap->br_blockcount = min(dmap->br_blockcount, smap.br_blockcount);
	ASSERT(dmap->br_blockcount == smap.br_blockcount);

	trace_xfs_reflink_remap_extent_dest(ip, &smap);

	 
	if (dmap->br_startblock == smap.br_startblock) {
		if (dmap->br_state != smap.br_state)
			error = -EFSCORRUPTED;
		goto out_cancel;
	}

	 
	if (dmap->br_state == XFS_EXT_UNWRITTEN &&
	    smap.br_state == XFS_EXT_UNWRITTEN)
		goto out_cancel;

	 
	if (dmap_written) {
		error = xfs_reflink_ag_has_free_space(mp,
				XFS_FSB_TO_AGNO(mp, dmap->br_startblock));
		if (error)
			goto out_cancel;
	}

	 
	if (!quota_reserved && !smap_real && dmap_written) {
		error = xfs_trans_reserve_quota_nblks(tp, ip,
				dmap->br_blockcount, 0, false);
		if (error)
			goto out_cancel;
	}

	if (smap_real)
		++iext_delta;

	if (dmap_written)
		++iext_delta;

	error = xfs_iext_count_may_overflow(ip, XFS_DATA_FORK, iext_delta);
	if (error == -EFBIG)
		error = xfs_iext_count_upgrade(tp, ip, iext_delta);
	if (error)
		goto out_cancel;

	if (smap_real) {
		 
		xfs_bmap_unmap_extent(tp, ip, &smap);
		xfs_refcount_decrease_extent(tp, &smap);
		qdelta -= smap.br_blockcount;
	} else if (smap.br_startblock == DELAYSTARTBLOCK) {
		int		done;

		 
		error = xfs_bunmapi(NULL, ip, smap.br_startoff,
				smap.br_blockcount, 0, 1, &done);
		if (error)
			goto out_cancel;
		ASSERT(done);
	}

	 
	if (dmap_written) {
		xfs_refcount_increase_extent(tp, dmap);
		xfs_bmap_map_extent(tp, ip, dmap);
		qdelta += dmap->br_blockcount;
	}

	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, qdelta);

	 
	newlen = XFS_FSB_TO_B(mp, dmap->br_startoff + dmap->br_blockcount);
	newlen = min_t(xfs_off_t, newlen, new_isize);
	if (newlen > i_size_read(VFS_I(ip))) {
		trace_xfs_reflink_update_inode_size(ip, newlen);
		i_size_write(VFS_I(ip), newlen);
		ip->i_disk_size = newlen;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	}

	 
	error = xfs_trans_commit(tp);
	goto out_unlock;

out_cancel:
	xfs_trans_cancel(tp);
out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
out:
	if (error)
		trace_xfs_reflink_remap_extent_error(ip, error, _RET_IP_);
	return error;
}

 
int
xfs_reflink_remap_blocks(
	struct xfs_inode	*src,
	loff_t			pos_in,
	struct xfs_inode	*dest,
	loff_t			pos_out,
	loff_t			remap_len,
	loff_t			*remapped)
{
	struct xfs_bmbt_irec	imap;
	struct xfs_mount	*mp = src->i_mount;
	xfs_fileoff_t		srcoff = XFS_B_TO_FSBT(mp, pos_in);
	xfs_fileoff_t		destoff = XFS_B_TO_FSBT(mp, pos_out);
	xfs_filblks_t		len;
	xfs_filblks_t		remapped_len = 0;
	xfs_off_t		new_isize = pos_out + remap_len;
	int			nimaps;
	int			error = 0;

	len = min_t(xfs_filblks_t, XFS_B_TO_FSB(mp, remap_len),
			XFS_MAX_FILEOFF);

	trace_xfs_reflink_remap_blocks(src, srcoff, len, dest, destoff);

	while (len > 0) {
		unsigned int	lock_mode;

		 
		nimaps = 1;
		lock_mode = xfs_ilock_data_map_shared(src);
		error = xfs_bmapi_read(src, srcoff, len, &imap, &nimaps, 0);
		xfs_iunlock(src, lock_mode);
		if (error)
			break;
		 
		ASSERT(nimaps == 1 && imap.br_startoff == srcoff);
		if (imap.br_startblock == DELAYSTARTBLOCK) {
			ASSERT(imap.br_startblock != DELAYSTARTBLOCK);
			error = -EFSCORRUPTED;
			break;
		}

		trace_xfs_reflink_remap_extent_src(src, &imap);

		 
		imap.br_startoff = destoff;
		error = xfs_reflink_remap_extent(dest, &imap, new_isize);
		if (error)
			break;

		if (fatal_signal_pending(current)) {
			error = -EINTR;
			break;
		}

		 
		srcoff += imap.br_blockcount;
		destoff += imap.br_blockcount;
		len -= imap.br_blockcount;
		remapped_len += imap.br_blockcount;
	}

	if (error)
		trace_xfs_reflink_remap_blocks_error(dest, error, _RET_IP_);
	*remapped = min_t(loff_t, remap_len,
			  XFS_FSB_TO_B(src->i_mount, remapped_len));
	return error;
}

 
static int
xfs_reflink_zero_posteof(
	struct xfs_inode	*ip,
	loff_t			pos)
{
	loff_t			isize = i_size_read(VFS_I(ip));

	if (pos <= isize)
		return 0;

	trace_xfs_zero_eof(ip, isize, pos - isize);
	return xfs_zero_range(ip, isize, pos - isize, NULL);
}

 
int
xfs_reflink_remap_prep(
	struct file		*file_in,
	loff_t			pos_in,
	struct file		*file_out,
	loff_t			pos_out,
	loff_t			*len,
	unsigned int		remap_flags)
{
	struct inode		*inode_in = file_inode(file_in);
	struct xfs_inode	*src = XFS_I(inode_in);
	struct inode		*inode_out = file_inode(file_out);
	struct xfs_inode	*dest = XFS_I(inode_out);
	int			ret;

	 
	ret = xfs_ilock2_io_mmap(src, dest);
	if (ret)
		return ret;

	 
	ret = -EINVAL;
	 
	if (XFS_IS_REALTIME_INODE(src) || XFS_IS_REALTIME_INODE(dest))
		goto out_unlock;

	 
	if (IS_DAX(inode_in) != IS_DAX(inode_out))
		goto out_unlock;

	if (!IS_DAX(inode_in))
		ret = generic_remap_file_range_prep(file_in, pos_in, file_out,
				pos_out, len, remap_flags);
	else
		ret = dax_remap_file_range_prep(file_in, pos_in, file_out,
				pos_out, len, remap_flags, &xfs_read_iomap_ops);
	if (ret || *len == 0)
		goto out_unlock;

	 
	ret = xfs_qm_dqattach(dest);
	if (ret)
		goto out_unlock;

	 
	ret = xfs_reflink_zero_posteof(dest, pos_out);
	if (ret)
		goto out_unlock;

	 
	ret = xfs_reflink_set_inode_flag(src, dest);
	if (ret)
		goto out_unlock;

	 
	if (pos_out > XFS_ISIZE(dest)) {
		loff_t	flen = *len + (pos_out - XFS_ISIZE(dest));
		ret = xfs_flush_unmap_range(dest, XFS_ISIZE(dest), flen);
	} else {
		ret = xfs_flush_unmap_range(dest, pos_out, *len);
	}
	if (ret)
		goto out_unlock;

	return 0;
out_unlock:
	xfs_iunlock2_io_mmap(src, dest);
	return ret;
}

 
int
xfs_reflink_inode_has_shared_extents(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	bool				*has_shared)
{
	struct xfs_bmbt_irec		got;
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_ifork		*ifp;
	struct xfs_iext_cursor		icur;
	bool				found;
	int				error;

	ifp = xfs_ifork_ptr(ip, XFS_DATA_FORK);
	error = xfs_iread_extents(tp, ip, XFS_DATA_FORK);
	if (error)
		return error;

	*has_shared = false;
	found = xfs_iext_lookup_extent(ip, ifp, 0, &icur, &got);
	while (found) {
		struct xfs_perag	*pag;
		xfs_agblock_t		agbno;
		xfs_extlen_t		aglen;
		xfs_agblock_t		rbno;
		xfs_extlen_t		rlen;

		if (isnullstartblock(got.br_startblock) ||
		    got.br_state != XFS_EXT_NORM)
			goto next;

		pag = xfs_perag_get(mp, XFS_FSB_TO_AGNO(mp, got.br_startblock));
		agbno = XFS_FSB_TO_AGBNO(mp, got.br_startblock);
		aglen = got.br_blockcount;
		error = xfs_reflink_find_shared(pag, tp, agbno, aglen,
				&rbno, &rlen, false);
		xfs_perag_put(pag);
		if (error)
			return error;

		 
		if (rbno != NULLAGBLOCK) {
			*has_shared = true;
			return 0;
		}
next:
		found = xfs_iext_next_extent(ifp, &icur, &got);
	}

	return 0;
}

 
int
xfs_reflink_clear_inode_flag(
	struct xfs_inode	*ip,
	struct xfs_trans	**tpp)
{
	bool			needs_flag;
	int			error = 0;

	ASSERT(xfs_is_reflink_inode(ip));

	error = xfs_reflink_inode_has_shared_extents(*tpp, ip, &needs_flag);
	if (error || needs_flag)
		return error;

	 
	error = xfs_reflink_cancel_cow_blocks(ip, tpp, 0, XFS_MAX_FILEOFF,
			true);
	if (error)
		return error;

	 
	trace_xfs_reflink_unset_inode_flag(ip);
	ip->i_diflags2 &= ~XFS_DIFLAG2_REFLINK;
	xfs_inode_clear_cowblocks_tag(ip);
	xfs_trans_log_inode(*tpp, ip, XFS_ILOG_CORE);

	return error;
}

 
STATIC int
xfs_reflink_try_clear_inode_flag(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error = 0;

	 
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_reflink_clear_inode_flag(ip, &tp);
	if (error)
		goto cancel;

	error = xfs_trans_commit(tp);
	if (error)
		goto out;

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;
cancel:
	xfs_trans_cancel(tp);
out:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
int
xfs_reflink_unshare(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		len)
{
	struct inode		*inode = VFS_I(ip);
	int			error;

	if (!xfs_is_reflink_inode(ip))
		return 0;

	trace_xfs_reflink_unshare(ip, offset, len);

	inode_dio_wait(inode);

	if (IS_DAX(inode))
		error = dax_file_unshare(inode, offset, len,
				&xfs_dax_write_iomap_ops);
	else
		error = iomap_file_unshare(inode, offset, len,
				&xfs_buffered_write_iomap_ops);
	if (error)
		goto out;

	error = filemap_write_and_wait_range(inode->i_mapping, offset,
			offset + len - 1);
	if (error)
		goto out;

	 
	error = xfs_reflink_try_clear_inode_flag(ip);
	if (error)
		goto out;
	return 0;

out:
	trace_xfs_reflink_unshare_error(ip, error, _RET_IP_);
	return error;
}

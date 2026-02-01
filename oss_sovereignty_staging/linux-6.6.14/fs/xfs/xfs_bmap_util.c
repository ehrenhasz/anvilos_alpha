
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_trans.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_bmap_btree.h"
#include "xfs_rtalloc.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_trans_space.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_iomap.h"
#include "xfs_reflink.h"

 

 
xfs_daddr_t
xfs_fsb_to_db(struct xfs_inode *ip, xfs_fsblock_t fsb)
{
	if (XFS_IS_REALTIME_INODE(ip))
		return XFS_FSB_TO_BB(ip->i_mount, fsb);
	return XFS_FSB_TO_DADDR(ip->i_mount, fsb);
}

 
int
xfs_zero_extent(
	struct xfs_inode	*ip,
	xfs_fsblock_t		start_fsb,
	xfs_off_t		count_fsb)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_buftarg	*target = xfs_inode_buftarg(ip);
	xfs_daddr_t		sector = xfs_fsb_to_db(ip, start_fsb);
	sector_t		block = XFS_BB_TO_FSBT(mp, sector);

	return blkdev_issue_zeroout(target->bt_bdev,
		block << (mp->m_super->s_blocksize_bits - 9),
		count_fsb << (mp->m_super->s_blocksize_bits - 9),
		GFP_NOFS, 0);
}

#ifdef CONFIG_XFS_RT
int
xfs_bmap_rtalloc(
	struct xfs_bmalloca	*ap)
{
	struct xfs_mount	*mp = ap->ip->i_mount;
	xfs_fileoff_t		orig_offset = ap->offset;
	xfs_rtblock_t		rtb;
	xfs_extlen_t		prod = 0;   
	xfs_extlen_t		mod = 0;    
	xfs_extlen_t		ralen = 0;  
	xfs_extlen_t		align;      
	xfs_extlen_t		orig_length = ap->length;
	xfs_extlen_t		minlen = mp->m_sb.sb_rextsize;
	xfs_extlen_t		raminlen;
	bool			rtlocked = false;
	bool			ignore_locality = false;
	int			error;

	align = xfs_get_extsz_hint(ap->ip);
retry:
	prod = align / mp->m_sb.sb_rextsize;
	error = xfs_bmap_extsize_align(mp, &ap->got, &ap->prev,
					align, 1, ap->eof, 0,
					ap->conv, &ap->offset, &ap->length);
	if (error)
		return error;
	ASSERT(ap->length);
	ASSERT(ap->length % mp->m_sb.sb_rextsize == 0);

	 
	if (ap->offset != orig_offset)
		minlen += orig_offset - ap->offset;

	 
	div_u64_rem(ap->offset, align, &mod);
	if (mod || ap->length % align)
		prod = 1;
	 
	ralen = ap->length / mp->m_sb.sb_rextsize;
	 
	if (ralen * mp->m_sb.sb_rextsize >= XFS_MAX_BMBT_EXTLEN)
		ralen = XFS_MAX_BMBT_EXTLEN / mp->m_sb.sb_rextsize;

	 
	if (!rtlocked) {
		xfs_ilock(mp->m_rbmip, XFS_ILOCK_EXCL|XFS_ILOCK_RTBITMAP);
		xfs_trans_ijoin(ap->tp, mp->m_rbmip, XFS_ILOCK_EXCL);
		xfs_ilock(mp->m_rsumip, XFS_ILOCK_EXCL|XFS_ILOCK_RTSUM);
		xfs_trans_ijoin(ap->tp, mp->m_rsumip, XFS_ILOCK_EXCL);
		rtlocked = true;
	}

	 
	if (ap->eof && ap->offset == 0) {
		xfs_rtblock_t rtx;  

		error = xfs_rtpick_extent(mp, ap->tp, ralen, &rtx);
		if (error)
			return error;
		ap->blkno = rtx * mp->m_sb.sb_rextsize;
	} else {
		ap->blkno = 0;
	}

	xfs_bmap_adjacent(ap);

	 
	if (ignore_locality)
		ap->blkno = 0;
	else
		do_div(ap->blkno, mp->m_sb.sb_rextsize);
	rtb = ap->blkno;
	ap->length = ralen;
	raminlen = max_t(xfs_extlen_t, 1, minlen / mp->m_sb.sb_rextsize);
	error = xfs_rtallocate_extent(ap->tp, ap->blkno, raminlen, ap->length,
			&ralen, ap->wasdel, prod, &rtb);
	if (error)
		return error;

	if (rtb != NULLRTBLOCK) {
		ap->blkno = rtb * mp->m_sb.sb_rextsize;
		ap->length = ralen * mp->m_sb.sb_rextsize;
		ap->ip->i_nblocks += ap->length;
		xfs_trans_log_inode(ap->tp, ap->ip, XFS_ILOG_CORE);
		if (ap->wasdel)
			ap->ip->i_delayed_blks -= ap->length;
		 
		xfs_trans_mod_dquot_byino(ap->tp, ap->ip,
			ap->wasdel ? XFS_TRANS_DQ_DELRTBCOUNT :
					XFS_TRANS_DQ_RTBCOUNT, ap->length);
		return 0;
	}

	if (align > mp->m_sb.sb_rextsize) {
		 
		ap->offset = orig_offset;
		ap->length = orig_length;
		minlen = align = mp->m_sb.sb_rextsize;
		goto retry;
	}

	if (!ignore_locality && ap->blkno != 0) {
		 
		ignore_locality = true;
		goto retry;
	}

	ap->blkno = NULLFSBLOCK;
	ap->length = 0;
	return 0;
}
#endif  

 

 
xfs_extnum_t
xfs_bmap_count_leaves(
	struct xfs_ifork	*ifp,
	xfs_filblks_t		*count)
{
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	got;
	xfs_extnum_t		numrecs = 0;

	for_each_xfs_iext(ifp, &icur, &got) {
		if (!isnullstartblock(got.br_startblock)) {
			*count += got.br_blockcount;
			numrecs++;
		}
	}

	return numrecs;
}

 
int
xfs_bmap_count_blocks(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	xfs_extnum_t		*nextents,
	xfs_filblks_t		*count)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_cur	*cur;
	xfs_extlen_t		btblocks = 0;
	int			error;

	*nextents = 0;
	*count = 0;

	if (!ifp)
		return 0;

	switch (ifp->if_format) {
	case XFS_DINODE_FMT_BTREE:
		error = xfs_iread_extents(tp, ip, whichfork);
		if (error)
			return error;

		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		error = xfs_btree_count_blocks(cur, &btblocks);
		xfs_btree_del_cursor(cur, error);
		if (error)
			return error;

		 
		*count += btblocks - 1;

		fallthrough;
	case XFS_DINODE_FMT_EXTENTS:
		*nextents = xfs_bmap_count_leaves(ifp, count);
		break;
	}

	return 0;
}

static int
xfs_getbmap_report_one(
	struct xfs_inode	*ip,
	struct getbmapx		*bmv,
	struct kgetbmap		*out,
	int64_t			bmv_end,
	struct xfs_bmbt_irec	*got)
{
	struct kgetbmap		*p = out + bmv->bmv_entries;
	bool			shared = false;
	int			error;

	error = xfs_reflink_trim_around_shared(ip, got, &shared);
	if (error)
		return error;

	if (isnullstartblock(got->br_startblock) ||
	    got->br_startblock == DELAYSTARTBLOCK) {
		 
		if (!(bmv->bmv_iflags & BMV_IF_DELALLOC))
			return 0;

		p->bmv_oflags |= BMV_OF_DELALLOC;
		p->bmv_block = -2;
	} else {
		p->bmv_block = xfs_fsb_to_db(ip, got->br_startblock);
	}

	if (got->br_state == XFS_EXT_UNWRITTEN &&
	    (bmv->bmv_iflags & BMV_IF_PREALLOC))
		p->bmv_oflags |= BMV_OF_PREALLOC;

	if (shared)
		p->bmv_oflags |= BMV_OF_SHARED;

	p->bmv_offset = XFS_FSB_TO_BB(ip->i_mount, got->br_startoff);
	p->bmv_length = XFS_FSB_TO_BB(ip->i_mount, got->br_blockcount);

	bmv->bmv_offset = p->bmv_offset + p->bmv_length;
	bmv->bmv_length = max(0LL, bmv_end - bmv->bmv_offset);
	bmv->bmv_entries++;
	return 0;
}

static void
xfs_getbmap_report_hole(
	struct xfs_inode	*ip,
	struct getbmapx		*bmv,
	struct kgetbmap		*out,
	int64_t			bmv_end,
	xfs_fileoff_t		bno,
	xfs_fileoff_t		end)
{
	struct kgetbmap		*p = out + bmv->bmv_entries;

	if (bmv->bmv_iflags & BMV_IF_NO_HOLES)
		return;

	p->bmv_block = -1;
	p->bmv_offset = XFS_FSB_TO_BB(ip->i_mount, bno);
	p->bmv_length = XFS_FSB_TO_BB(ip->i_mount, end - bno);

	bmv->bmv_offset = p->bmv_offset + p->bmv_length;
	bmv->bmv_length = max(0LL, bmv_end - bmv->bmv_offset);
	bmv->bmv_entries++;
}

static inline bool
xfs_getbmap_full(
	struct getbmapx		*bmv)
{
	return bmv->bmv_length == 0 || bmv->bmv_entries >= bmv->bmv_count - 1;
}

static bool
xfs_getbmap_next_rec(
	struct xfs_bmbt_irec	*rec,
	xfs_fileoff_t		total_end)
{
	xfs_fileoff_t		end = rec->br_startoff + rec->br_blockcount;

	if (end == total_end)
		return false;

	rec->br_startoff += rec->br_blockcount;
	if (!isnullstartblock(rec->br_startblock) &&
	    rec->br_startblock != DELAYSTARTBLOCK)
		rec->br_startblock += rec->br_blockcount;
	rec->br_blockcount = total_end - end;
	return true;
}

 
int						 
xfs_getbmap(
	struct xfs_inode	*ip,
	struct getbmapx		*bmv,		 
	struct kgetbmap		*out)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			iflags = bmv->bmv_iflags;
	int			whichfork, lock, error = 0;
	int64_t			bmv_end, max_len;
	xfs_fileoff_t		bno, first_bno;
	struct xfs_ifork	*ifp;
	struct xfs_bmbt_irec	got, rec;
	xfs_filblks_t		len;
	struct xfs_iext_cursor	icur;

	if (bmv->bmv_iflags & ~BMV_IF_VALID)
		return -EINVAL;
#ifndef DEBUG
	 
	if (iflags & BMV_IF_COWFORK)
		return -EINVAL;
#endif
	if ((iflags & BMV_IF_ATTRFORK) && (iflags & BMV_IF_COWFORK))
		return -EINVAL;

	if (bmv->bmv_length < -1)
		return -EINVAL;
	bmv->bmv_entries = 0;
	if (bmv->bmv_length == 0)
		return 0;

	if (iflags & BMV_IF_ATTRFORK)
		whichfork = XFS_ATTR_FORK;
	else if (iflags & BMV_IF_COWFORK)
		whichfork = XFS_COW_FORK;
	else
		whichfork = XFS_DATA_FORK;

	xfs_ilock(ip, XFS_IOLOCK_SHARED);
	switch (whichfork) {
	case XFS_ATTR_FORK:
		lock = xfs_ilock_attr_map_shared(ip);
		if (!xfs_inode_has_attr_fork(ip))
			goto out_unlock_ilock;

		max_len = 1LL << 32;
		break;
	case XFS_COW_FORK:
		lock = XFS_ILOCK_SHARED;
		xfs_ilock(ip, lock);

		 
		if (!xfs_ifork_ptr(ip, whichfork))
			goto out_unlock_ilock;

		if (xfs_get_cowextsz_hint(ip))
			max_len = mp->m_super->s_maxbytes;
		else
			max_len = XFS_ISIZE(ip);
		break;
	case XFS_DATA_FORK:
		if (!(iflags & BMV_IF_DELALLOC) &&
		    (ip->i_delayed_blks || XFS_ISIZE(ip) > ip->i_disk_size)) {
			error = filemap_write_and_wait(VFS_I(ip)->i_mapping);
			if (error)
				goto out_unlock_iolock;

			 
		}

		if (xfs_get_extsz_hint(ip) ||
		    (ip->i_diflags &
		     (XFS_DIFLAG_PREALLOC | XFS_DIFLAG_APPEND)))
			max_len = mp->m_super->s_maxbytes;
		else
			max_len = XFS_ISIZE(ip);

		lock = xfs_ilock_data_map_shared(ip);
		break;
	}

	ifp = xfs_ifork_ptr(ip, whichfork);

	switch (ifp->if_format) {
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
		break;
	case XFS_DINODE_FMT_LOCAL:
		 
		goto out_unlock_ilock;
	default:
		error = -EINVAL;
		goto out_unlock_ilock;
	}

	if (bmv->bmv_length == -1) {
		max_len = XFS_FSB_TO_BB(mp, XFS_B_TO_FSB(mp, max_len));
		bmv->bmv_length = max(0LL, max_len - bmv->bmv_offset);
	}

	bmv_end = bmv->bmv_offset + bmv->bmv_length;

	first_bno = bno = XFS_BB_TO_FSBT(mp, bmv->bmv_offset);
	len = XFS_BB_TO_FSB(mp, bmv->bmv_length);

	error = xfs_iread_extents(NULL, ip, whichfork);
	if (error)
		goto out_unlock_ilock;

	if (!xfs_iext_lookup_extent(ip, ifp, bno, &icur, &got)) {
		 
		if (iflags & BMV_IF_DELALLOC)
			xfs_getbmap_report_hole(ip, bmv, out, bmv_end, bno,
					XFS_B_TO_FSB(mp, XFS_ISIZE(ip)));
		goto out_unlock_ilock;
	}

	while (!xfs_getbmap_full(bmv)) {
		xfs_trim_extent(&got, first_bno, len);

		 
		if (got.br_startoff > bno) {
			xfs_getbmap_report_hole(ip, bmv, out, bmv_end, bno,
					got.br_startoff);
			if (xfs_getbmap_full(bmv))
				break;
		}

		 
		bno = got.br_startoff + got.br_blockcount;
		rec = got;
		do {
			error = xfs_getbmap_report_one(ip, bmv, out, bmv_end,
					&rec);
			if (error || xfs_getbmap_full(bmv))
				goto out_unlock_ilock;
		} while (xfs_getbmap_next_rec(&rec, bno));

		if (!xfs_iext_next_extent(ifp, &icur, &got)) {
			xfs_fileoff_t	end = XFS_B_TO_FSB(mp, XFS_ISIZE(ip));

			if (bmv->bmv_entries > 0)
				out[bmv->bmv_entries - 1].bmv_oflags |=
								BMV_OF_LAST;

			if (whichfork != XFS_ATTR_FORK && bno < end &&
			    !xfs_getbmap_full(bmv)) {
				xfs_getbmap_report_hole(ip, bmv, out, bmv_end,
						bno, end);
			}
			break;
		}

		if (bno >= first_bno + len)
			break;
	}

out_unlock_ilock:
	xfs_iunlock(ip, lock);
out_unlock_iolock:
	xfs_iunlock(ip, XFS_IOLOCK_SHARED);
	return error;
}

 
int
xfs_bmap_punch_delalloc_range(
	struct xfs_inode	*ip,
	xfs_off_t		start_byte,
	xfs_off_t		end_byte)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = &ip->i_df;
	xfs_fileoff_t		start_fsb = XFS_B_TO_FSBT(mp, start_byte);
	xfs_fileoff_t		end_fsb = XFS_B_TO_FSB(mp, end_byte);
	struct xfs_bmbt_irec	got, del;
	struct xfs_iext_cursor	icur;
	int			error = 0;

	ASSERT(!xfs_need_iread_extents(ifp));

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if (!xfs_iext_lookup_extent_before(ip, ifp, &end_fsb, &icur, &got))
		goto out_unlock;

	while (got.br_startoff + got.br_blockcount > start_fsb) {
		del = got;
		xfs_trim_extent(&del, start_fsb, end_fsb - start_fsb);

		 
		if (!del.br_blockcount ||
		    !isnullstartblock(del.br_startblock)) {
			if (!xfs_iext_prev_extent(ifp, &icur, &got))
				break;
			continue;
		}

		error = xfs_bmap_del_extent_delay(ip, XFS_DATA_FORK, &icur,
						  &got, &del);
		if (error || !xfs_iext_get_extent(ifp, &icur, &got))
			break;
	}

out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
bool
xfs_can_free_eofblocks(
	struct xfs_inode	*ip,
	bool			force)
{
	struct xfs_bmbt_irec	imap;
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		end_fsb;
	xfs_fileoff_t		last_fsb;
	int			nimaps = 1;
	int			error;

	 
	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL) ||
	       (VFS_I(ip)->i_state & I_FREEING));

	 
	if (!S_ISREG(VFS_I(ip)->i_mode))
		return false;

	 
	if (VFS_I(ip)->i_size == 0 &&
	    VFS_I(ip)->i_mapping->nrpages == 0 &&
	    ip->i_delayed_blks == 0)
		return false;

	 
	if (xfs_need_iread_extents(&ip->i_df))
		return false;

	 
	if (ip->i_diflags & (XFS_DIFLAG_PREALLOC | XFS_DIFLAG_APPEND))
		if (!force || ip->i_delayed_blks == 0)
			return false;

	 
	end_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)XFS_ISIZE(ip));
	if (XFS_IS_REALTIME_INODE(ip) && mp->m_sb.sb_rextsize > 1)
		end_fsb = roundup_64(end_fsb, mp->m_sb.sb_rextsize);
	last_fsb = XFS_B_TO_FSB(mp, mp->m_super->s_maxbytes);
	if (last_fsb <= end_fsb)
		return false;

	 
	xfs_ilock(ip, XFS_ILOCK_SHARED);
	error = xfs_bmapi_read(ip, end_fsb, last_fsb - end_fsb, &imap, &nimaps,
			0);
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	if (error || nimaps == 0)
		return false;

	 
	return imap.br_startblock != HOLESTARTBLOCK || ip->i_delayed_blks;
}

 
int
xfs_free_eofblocks(
	struct xfs_inode	*ip)
{
	struct xfs_trans	*tp;
	struct xfs_mount	*mp = ip->i_mount;
	int			error;

	 
	error = xfs_qm_dqattach(ip);
	if (error)
		return error;

	 
	inode_dio_wait(VFS_I(ip));

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_itruncate, 0, 0, 0, &tp);
	if (error) {
		ASSERT(xfs_is_shutdown(mp));
		return error;
	}

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	 
	error = xfs_itruncate_extents_flags(&tp, ip, XFS_DATA_FORK,
				XFS_ISIZE(ip), XFS_BMAPI_NODISCARD);
	if (error)
		goto err_cancel;

	error = xfs_trans_commit(tp);
	if (error)
		goto out_unlock;

	xfs_inode_clear_eofblocks_tag(ip);
	goto out_unlock;

err_cancel:
	 
	xfs_trans_cancel(tp);
out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

int
xfs_alloc_file_space(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		len)
{
	xfs_mount_t		*mp = ip->i_mount;
	xfs_off_t		count;
	xfs_filblks_t		allocated_fsb;
	xfs_filblks_t		allocatesize_fsb;
	xfs_extlen_t		extsz, temp;
	xfs_fileoff_t		startoffset_fsb;
	xfs_fileoff_t		endoffset_fsb;
	int			nimaps;
	int			rt;
	xfs_trans_t		*tp;
	xfs_bmbt_irec_t		imaps[1], *imapp;
	int			error;

	trace_xfs_alloc_file_space(ip);

	if (xfs_is_shutdown(mp))
		return -EIO;

	error = xfs_qm_dqattach(ip);
	if (error)
		return error;

	if (len <= 0)
		return -EINVAL;

	rt = XFS_IS_REALTIME_INODE(ip);
	extsz = xfs_get_extsz_hint(ip);

	count = len;
	imapp = &imaps[0];
	nimaps = 1;
	startoffset_fsb	= XFS_B_TO_FSBT(mp, offset);
	endoffset_fsb = XFS_B_TO_FSB(mp, offset + count);
	allocatesize_fsb = endoffset_fsb - startoffset_fsb;

	 
	while (allocatesize_fsb && !error) {
		xfs_fileoff_t	s, e;
		unsigned int	dblocks, rblocks, resblks;

		 
		if (unlikely(extsz)) {
			s = startoffset_fsb;
			do_div(s, extsz);
			s *= extsz;
			e = startoffset_fsb + allocatesize_fsb;
			div_u64_rem(startoffset_fsb, extsz, &temp);
			if (temp)
				e += temp;
			div_u64_rem(e, extsz, &temp);
			if (temp)
				e += extsz - temp;
		} else {
			s = 0;
			e = allocatesize_fsb;
		}

		 
		resblks = min_t(xfs_fileoff_t, (e - s),
				(XFS_MAX_BMBT_EXTLEN * nimaps));
		if (unlikely(rt)) {
			dblocks = XFS_DIOSTRAT_SPACE_RES(mp, 0);
			rblocks = resblks;
		} else {
			dblocks = XFS_DIOSTRAT_SPACE_RES(mp, resblks);
			rblocks = 0;
		}

		error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write,
				dblocks, rblocks, false, &tp);
		if (error)
			break;

		error = xfs_iext_count_may_overflow(ip, XFS_DATA_FORK,
				XFS_IEXT_ADD_NOSPLIT_CNT);
		if (error == -EFBIG)
			error = xfs_iext_count_upgrade(tp, ip,
					XFS_IEXT_ADD_NOSPLIT_CNT);
		if (error)
			goto error;

		error = xfs_bmapi_write(tp, ip, startoffset_fsb,
				allocatesize_fsb, XFS_BMAPI_PREALLOC, 0, imapp,
				&nimaps);
		if (error)
			goto error;

		ip->i_diflags |= XFS_DIFLAG_PREALLOC;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

		error = xfs_trans_commit(tp);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		if (error)
			break;

		allocated_fsb = imapp->br_blockcount;

		if (nimaps == 0) {
			error = -ENOSPC;
			break;
		}

		startoffset_fsb += allocated_fsb;
		allocatesize_fsb -= allocated_fsb;
	}

	return error;

error:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

static int
xfs_unmap_extent(
	struct xfs_inode	*ip,
	xfs_fileoff_t		startoffset_fsb,
	xfs_filblks_t		len_fsb,
	int			*done)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	uint			resblks = XFS_DIOSTRAT_SPACE_RES(mp, 0);
	int			error;

	error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_write, resblks, 0,
			false, &tp);
	if (error)
		return error;

	error = xfs_iext_count_may_overflow(ip, XFS_DATA_FORK,
			XFS_IEXT_PUNCH_HOLE_CNT);
	if (error == -EFBIG)
		error = xfs_iext_count_upgrade(tp, ip, XFS_IEXT_PUNCH_HOLE_CNT);
	if (error)
		goto out_trans_cancel;

	error = xfs_bunmapi(tp, ip, startoffset_fsb, len_fsb, 0, 2, done);
	if (error)
		goto out_trans_cancel;

	error = xfs_trans_commit(tp);
out_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

out_trans_cancel:
	xfs_trans_cancel(tp);
	goto out_unlock;
}

 
int
xfs_flush_unmap_range(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		len)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct inode		*inode = VFS_I(ip);
	xfs_off_t		rounding, start, end;
	int			error;

	rounding = max_t(xfs_off_t, mp->m_sb.sb_blocksize, PAGE_SIZE);
	start = round_down(offset, rounding);
	end = round_up(offset + len, rounding) - 1;

	error = filemap_write_and_wait_range(inode->i_mapping, start, end);
	if (error)
		return error;
	truncate_pagecache_range(inode, start, end);
	return 0;
}

int
xfs_free_file_space(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		len)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		startoffset_fsb;
	xfs_fileoff_t		endoffset_fsb;
	int			done = 0, error;

	trace_xfs_free_file_space(ip);

	error = xfs_qm_dqattach(ip);
	if (error)
		return error;

	if (len <= 0)	 
		return 0;

	startoffset_fsb = XFS_B_TO_FSB(mp, offset);
	endoffset_fsb = XFS_B_TO_FSBT(mp, offset + len);

	 
	if (XFS_IS_REALTIME_INODE(ip) && mp->m_sb.sb_rextsize > 1) {
		startoffset_fsb = roundup_64(startoffset_fsb,
					     mp->m_sb.sb_rextsize);
		endoffset_fsb = rounddown_64(endoffset_fsb,
					     mp->m_sb.sb_rextsize);
	}

	 
	if (endoffset_fsb > startoffset_fsb) {
		while (!done) {
			error = xfs_unmap_extent(ip, startoffset_fsb,
					endoffset_fsb - startoffset_fsb, &done);
			if (error)
				return error;
		}
	}

	 
	if (offset >= XFS_ISIZE(ip))
		return 0;
	if (offset + len > XFS_ISIZE(ip))
		len = XFS_ISIZE(ip) - offset;
	error = xfs_zero_range(ip, offset, len, NULL);
	if (error)
		return error;

	 
	if (offset + len >= XFS_ISIZE(ip) && offset_in_page(offset + len) > 0) {
		error = filemap_write_and_wait_range(VFS_I(ip)->i_mapping,
				round_down(offset + len, PAGE_SIZE), LLONG_MAX);
	}

	return error;
}

static int
xfs_prepare_shift(
	struct xfs_inode	*ip,
	loff_t			offset)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			error;

	 
	if (xfs_can_free_eofblocks(ip, true)) {
		error = xfs_free_eofblocks(ip);
		if (error)
			return error;
	}

	 
	offset = round_down(offset, mp->m_sb.sb_blocksize);
	if (offset)
		offset -= mp->m_sb.sb_blocksize;

	 
	error = xfs_flush_unmap_range(ip, offset, XFS_ISIZE(ip));
	if (error)
		return error;

	 
	if (xfs_inode_has_cow_data(ip)) {
		error = xfs_reflink_cancel_cow_range(ip, offset, NULLFILEOFF,
				true);
		if (error)
			return error;
	}

	return 0;
}

 
int
xfs_collapse_file_space(
	struct xfs_inode	*ip,
	xfs_off_t		offset,
	xfs_off_t		len)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error;
	xfs_fileoff_t		next_fsb = XFS_B_TO_FSB(mp, offset + len);
	xfs_fileoff_t		shift_fsb = XFS_B_TO_FSB(mp, len);
	bool			done = false;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL));
	ASSERT(xfs_isilocked(ip, XFS_MMAPLOCK_EXCL));

	trace_xfs_collapse_file_space(ip);

	error = xfs_free_file_space(ip, offset, len);
	if (error)
		return error;

	error = xfs_prepare_shift(ip, offset);
	if (error)
		return error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	while (!done) {
		error = xfs_bmap_collapse_extents(tp, ip, &next_fsb, shift_fsb,
				&done);
		if (error)
			goto out_trans_cancel;
		if (done)
			break;

		 
		error = xfs_defer_finish(&tp);
		if (error)
			goto out_trans_cancel;
	}

	error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

out_trans_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
int
xfs_insert_file_space(
	struct xfs_inode	*ip,
	loff_t			offset,
	loff_t			len)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	int			error;
	xfs_fileoff_t		stop_fsb = XFS_B_TO_FSB(mp, offset);
	xfs_fileoff_t		next_fsb = NULLFSBLOCK;
	xfs_fileoff_t		shift_fsb = XFS_B_TO_FSB(mp, len);
	bool			done = false;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL));
	ASSERT(xfs_isilocked(ip, XFS_MMAPLOCK_EXCL));

	trace_xfs_insert_file_space(ip);

	error = xfs_bmap_can_insert_extents(ip, stop_fsb, shift_fsb);
	if (error)
		return error;

	error = xfs_prepare_shift(ip, offset);
	if (error)
		return error;

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write,
			XFS_DIOSTRAT_SPACE_RES(mp, 0), 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_iext_count_may_overflow(ip, XFS_DATA_FORK,
			XFS_IEXT_PUNCH_HOLE_CNT);
	if (error == -EFBIG)
		error = xfs_iext_count_upgrade(tp, ip, XFS_IEXT_PUNCH_HOLE_CNT);
	if (error)
		goto out_trans_cancel;

	 
	error = xfs_bmap_split_extent(tp, ip, stop_fsb);
	if (error)
		goto out_trans_cancel;

	do {
		error = xfs_defer_finish(&tp);
		if (error)
			goto out_trans_cancel;

		error = xfs_bmap_insert_extents(tp, ip, &next_fsb, shift_fsb,
				&done, stop_fsb);
		if (error)
			goto out_trans_cancel;
	} while (!done);

	error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

out_trans_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
static int
xfs_swap_extents_check_format(
	struct xfs_inode	*ip,	 
	struct xfs_inode	*tip)	 
{
	struct xfs_ifork	*ifp = &ip->i_df;
	struct xfs_ifork	*tifp = &tip->i_df;

	 
	if (XFS_IS_QUOTA_ON(ip->i_mount) &&
	    (!uid_eq(VFS_I(ip)->i_uid, VFS_I(tip)->i_uid) ||
	     !gid_eq(VFS_I(ip)->i_gid, VFS_I(tip)->i_gid) ||
	     ip->i_projid != tip->i_projid))
		return -EINVAL;

	 
	if (ifp->if_format == XFS_DINODE_FMT_LOCAL ||
	    tifp->if_format == XFS_DINODE_FMT_LOCAL)
		return -EINVAL;

	 
	if (ifp->if_nextents < tifp->if_nextents)
		return -EINVAL;

	 
	if (xfs_has_rmapbt(ip->i_mount))
		return 0;

	 
	if (ifp->if_format == XFS_DINODE_FMT_EXTENTS &&
	    tifp->if_format == XFS_DINODE_FMT_BTREE)
		return -EINVAL;

	 
	if (tifp->if_format == XFS_DINODE_FMT_EXTENTS &&
	    tifp->if_nextents > XFS_IFORK_MAXEXT(ip, XFS_DATA_FORK))
		return -EINVAL;

	 
	if (ifp->if_format == XFS_DINODE_FMT_EXTENTS &&
	    ifp->if_nextents > XFS_IFORK_MAXEXT(tip, XFS_DATA_FORK))
		return -EINVAL;

	 
	if (tifp->if_format == XFS_DINODE_FMT_BTREE) {
		if (xfs_inode_has_attr_fork(ip) &&
		    XFS_BMAP_BMDR_SPACE(tifp->if_broot) > xfs_inode_fork_boff(ip))
			return -EINVAL;
		if (tifp->if_nextents <= XFS_IFORK_MAXEXT(ip, XFS_DATA_FORK))
			return -EINVAL;
	}

	 
	if (ifp->if_format == XFS_DINODE_FMT_BTREE) {
		if (xfs_inode_has_attr_fork(tip) &&
		    XFS_BMAP_BMDR_SPACE(ip->i_df.if_broot) > xfs_inode_fork_boff(tip))
			return -EINVAL;
		if (ifp->if_nextents <= XFS_IFORK_MAXEXT(tip, XFS_DATA_FORK))
			return -EINVAL;
	}

	return 0;
}

static int
xfs_swap_extent_flush(
	struct xfs_inode	*ip)
{
	int	error;

	error = filemap_write_and_wait(VFS_I(ip)->i_mapping);
	if (error)
		return error;
	truncate_pagecache_range(VFS_I(ip), 0, -1);

	 
	if (VFS_I(ip)->i_mapping->nrpages)
		return -EINVAL;
	return 0;
}

 
STATIC int
xfs_swap_extent_rmap(
	struct xfs_trans		**tpp,
	struct xfs_inode		*ip,
	struct xfs_inode		*tip)
{
	struct xfs_trans		*tp = *tpp;
	struct xfs_bmbt_irec		irec;
	struct xfs_bmbt_irec		uirec;
	struct xfs_bmbt_irec		tirec;
	xfs_fileoff_t			offset_fsb;
	xfs_fileoff_t			end_fsb;
	xfs_filblks_t			count_fsb;
	int				error;
	xfs_filblks_t			ilen;
	xfs_filblks_t			rlen;
	int				nimaps;
	uint64_t			tip_flags2;

	 
	tip_flags2 = tip->i_diflags2;
	if (ip->i_diflags2 & XFS_DIFLAG2_REFLINK)
		tip->i_diflags2 |= XFS_DIFLAG2_REFLINK;

	offset_fsb = 0;
	end_fsb = XFS_B_TO_FSB(ip->i_mount, i_size_read(VFS_I(ip)));
	count_fsb = (xfs_filblks_t)(end_fsb - offset_fsb);

	while (count_fsb) {
		 
		nimaps = 1;
		error = xfs_bmapi_read(tip, offset_fsb, count_fsb, &tirec,
				&nimaps, 0);
		if (error)
			goto out;
		ASSERT(nimaps == 1);
		ASSERT(tirec.br_startblock != DELAYSTARTBLOCK);

		trace_xfs_swap_extent_rmap_remap(tip, &tirec);
		ilen = tirec.br_blockcount;

		 
		while (tirec.br_blockcount) {
			ASSERT(tp->t_highest_agno == NULLAGNUMBER);
			trace_xfs_swap_extent_rmap_remap_piece(tip, &tirec);

			 
			nimaps = 1;
			error = xfs_bmapi_read(ip, tirec.br_startoff,
					tirec.br_blockcount, &irec,
					&nimaps, 0);
			if (error)
				goto out;
			ASSERT(nimaps == 1);
			ASSERT(tirec.br_startoff == irec.br_startoff);
			trace_xfs_swap_extent_rmap_remap_piece(ip, &irec);

			 
			uirec = tirec;
			uirec.br_blockcount = rlen = min_t(xfs_filblks_t,
					tirec.br_blockcount,
					irec.br_blockcount);
			trace_xfs_swap_extent_rmap_remap_piece(tip, &uirec);

			if (xfs_bmap_is_real_extent(&uirec)) {
				error = xfs_iext_count_may_overflow(ip,
						XFS_DATA_FORK,
						XFS_IEXT_SWAP_RMAP_CNT);
				if (error == -EFBIG)
					error = xfs_iext_count_upgrade(tp, ip,
							XFS_IEXT_SWAP_RMAP_CNT);
				if (error)
					goto out;
			}

			if (xfs_bmap_is_real_extent(&irec)) {
				error = xfs_iext_count_may_overflow(tip,
						XFS_DATA_FORK,
						XFS_IEXT_SWAP_RMAP_CNT);
				if (error == -EFBIG)
					error = xfs_iext_count_upgrade(tp, ip,
							XFS_IEXT_SWAP_RMAP_CNT);
				if (error)
					goto out;
			}

			 
			xfs_bmap_unmap_extent(tp, tip, &uirec);

			 
			xfs_bmap_unmap_extent(tp, ip, &irec);

			 
			xfs_bmap_map_extent(tp, ip, &uirec);

			 
			xfs_bmap_map_extent(tp, tip, &irec);

			error = xfs_defer_finish(tpp);
			tp = *tpp;
			if (error)
				goto out;

			tirec.br_startoff += rlen;
			if (tirec.br_startblock != HOLESTARTBLOCK &&
			    tirec.br_startblock != DELAYSTARTBLOCK)
				tirec.br_startblock += rlen;
			tirec.br_blockcount -= rlen;
		}

		 
		count_fsb -= ilen;
		offset_fsb += ilen;
	}

	tip->i_diflags2 = tip_flags2;
	return 0;

out:
	trace_xfs_swap_extent_rmap_error(ip, error, _RET_IP_);
	tip->i_diflags2 = tip_flags2;
	return error;
}

 
STATIC int
xfs_swap_extent_forks(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_inode	*tip,
	int			*src_log_flags,
	int			*target_log_flags)
{
	xfs_filblks_t		aforkblks = 0;
	xfs_filblks_t		taforkblks = 0;
	xfs_extnum_t		junk;
	uint64_t		tmp;
	int			error;

	 
	if (xfs_inode_has_attr_fork(ip) && ip->i_af.if_nextents > 0 &&
	    ip->i_af.if_format != XFS_DINODE_FMT_LOCAL) {
		error = xfs_bmap_count_blocks(tp, ip, XFS_ATTR_FORK, &junk,
				&aforkblks);
		if (error)
			return error;
	}
	if (xfs_inode_has_attr_fork(tip) && tip->i_af.if_nextents > 0 &&
	    tip->i_af.if_format != XFS_DINODE_FMT_LOCAL) {
		error = xfs_bmap_count_blocks(tp, tip, XFS_ATTR_FORK, &junk,
				&taforkblks);
		if (error)
			return error;
	}

	 
	if (xfs_has_v3inodes(ip->i_mount)) {
		if (ip->i_df.if_format == XFS_DINODE_FMT_BTREE)
			(*target_log_flags) |= XFS_ILOG_DOWNER;
		if (tip->i_df.if_format == XFS_DINODE_FMT_BTREE)
			(*src_log_flags) |= XFS_ILOG_DOWNER;
	}

	 
	swap(ip->i_df, tip->i_df);

	 
	tmp = (uint64_t)ip->i_nblocks;
	ip->i_nblocks = tip->i_nblocks - taforkblks + aforkblks;
	tip->i_nblocks = tmp + taforkblks - aforkblks;

	 
	ASSERT(tip->i_delayed_blks == 0);
	tip->i_delayed_blks = ip->i_delayed_blks;
	ip->i_delayed_blks = 0;

	switch (ip->i_df.if_format) {
	case XFS_DINODE_FMT_EXTENTS:
		(*src_log_flags) |= XFS_ILOG_DEXT;
		break;
	case XFS_DINODE_FMT_BTREE:
		ASSERT(!xfs_has_v3inodes(ip->i_mount) ||
		       (*src_log_flags & XFS_ILOG_DOWNER));
		(*src_log_flags) |= XFS_ILOG_DBROOT;
		break;
	}

	switch (tip->i_df.if_format) {
	case XFS_DINODE_FMT_EXTENTS:
		(*target_log_flags) |= XFS_ILOG_DEXT;
		break;
	case XFS_DINODE_FMT_BTREE:
		(*target_log_flags) |= XFS_ILOG_DBROOT;
		ASSERT(!xfs_has_v3inodes(ip->i_mount) ||
		       (*target_log_flags & XFS_ILOG_DOWNER));
		break;
	}

	return 0;
}

 
static int
xfs_swap_change_owner(
	struct xfs_trans	**tpp,
	struct xfs_inode	*ip,
	struct xfs_inode	*tmpip)
{
	int			error;
	struct xfs_trans	*tp = *tpp;

	do {
		error = xfs_bmbt_change_owner(tp, ip, XFS_DATA_FORK, ip->i_ino,
					      NULL);
		 
		if (error != -EAGAIN)
			break;

		error = xfs_trans_roll(tpp);
		if (error)
			break;
		tp = *tpp;

		 
		xfs_trans_ijoin(tp, ip, 0);
		xfs_trans_ijoin(tp, tmpip, 0);
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
		xfs_trans_log_inode(tp, tmpip, XFS_ILOG_CORE);
	} while (true);

	return error;
}

int
xfs_swap_extents(
	struct xfs_inode	*ip,	 
	struct xfs_inode	*tip,	 
	struct xfs_swapext	*sxp)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_trans	*tp;
	struct xfs_bstat	*sbp = &sxp->sx_stat;
	int			src_log_flags, target_log_flags;
	int			error = 0;
	uint64_t		f;
	int			resblks = 0;
	unsigned int		flags = 0;
	struct timespec64	ctime;

	 
	lock_two_nondirectories(VFS_I(ip), VFS_I(tip));
	filemap_invalidate_lock_two(VFS_I(ip)->i_mapping,
				    VFS_I(tip)->i_mapping);

	 
	if ((VFS_I(ip)->i_mode & S_IFMT) != (VFS_I(tip)->i_mode & S_IFMT)) {
		error = -EINVAL;
		goto out_unlock;
	}

	 
	if (XFS_IS_REALTIME_INODE(ip) != XFS_IS_REALTIME_INODE(tip)) {
		error = -EINVAL;
		goto out_unlock;
	}

	error = xfs_qm_dqattach(ip);
	if (error)
		goto out_unlock;

	error = xfs_qm_dqattach(tip);
	if (error)
		goto out_unlock;

	error = xfs_swap_extent_flush(ip);
	if (error)
		goto out_unlock;
	error = xfs_swap_extent_flush(tip);
	if (error)
		goto out_unlock;

	if (xfs_inode_has_cow_data(tip)) {
		error = xfs_reflink_cancel_cow_range(tip, 0, NULLFILEOFF, true);
		if (error)
			goto out_unlock;
	}

	 
	if (xfs_has_rmapbt(mp)) {
		int		w = XFS_DATA_FORK;
		uint32_t	ipnext = ip->i_df.if_nextents;
		uint32_t	tipnext	= tip->i_df.if_nextents;

		 
		resblks = XFS_SWAP_RMAP_SPACE_RES(mp, ipnext, w);
		resblks +=  XFS_SWAP_RMAP_SPACE_RES(mp, tipnext, w);

		 
		flags |= XFS_TRANS_RES_FDBLKS;
	}
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, resblks, 0, flags,
				&tp);
	if (error)
		goto out_unlock;

	 
	xfs_lock_two_inodes(ip, XFS_ILOCK_EXCL, tip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);
	xfs_trans_ijoin(tp, tip, 0);


	 
	if (sxp->sx_offset != 0 ||
	    sxp->sx_length != ip->i_disk_size ||
	    sxp->sx_length != tip->i_disk_size) {
		error = -EFAULT;
		goto out_trans_cancel;
	}

	trace_xfs_swap_extent_before(ip, 0);
	trace_xfs_swap_extent_before(tip, 1);

	 
	error = xfs_swap_extents_check_format(ip, tip);
	if (error) {
		xfs_notice(mp,
		    "%s: inode 0x%llx format is incompatible for exchanging.",
				__func__, ip->i_ino);
		goto out_trans_cancel;
	}

	 
	ctime = inode_get_ctime(VFS_I(ip));
	if ((sbp->bs_ctime.tv_sec != ctime.tv_sec) ||
	    (sbp->bs_ctime.tv_nsec != ctime.tv_nsec) ||
	    (sbp->bs_mtime.tv_sec != VFS_I(ip)->i_mtime.tv_sec) ||
	    (sbp->bs_mtime.tv_nsec != VFS_I(ip)->i_mtime.tv_nsec)) {
		error = -EBUSY;
		goto out_trans_cancel;
	}

	 
	src_log_flags = XFS_ILOG_CORE;
	target_log_flags = XFS_ILOG_CORE;

	if (xfs_has_rmapbt(mp))
		error = xfs_swap_extent_rmap(&tp, ip, tip);
	else
		error = xfs_swap_extent_forks(tp, ip, tip, &src_log_flags,
				&target_log_flags);
	if (error)
		goto out_trans_cancel;

	 
	if ((ip->i_diflags2 & XFS_DIFLAG2_REFLINK) ^
	    (tip->i_diflags2 & XFS_DIFLAG2_REFLINK)) {
		f = ip->i_diflags2 & XFS_DIFLAG2_REFLINK;
		ip->i_diflags2 &= ~XFS_DIFLAG2_REFLINK;
		ip->i_diflags2 |= tip->i_diflags2 & XFS_DIFLAG2_REFLINK;
		tip->i_diflags2 &= ~XFS_DIFLAG2_REFLINK;
		tip->i_diflags2 |= f & XFS_DIFLAG2_REFLINK;
	}

	 
	if (xfs_has_reflink(mp)) {
		ASSERT(!ip->i_cowfp ||
		       ip->i_cowfp->if_format == XFS_DINODE_FMT_EXTENTS);
		ASSERT(!tip->i_cowfp ||
		       tip->i_cowfp->if_format == XFS_DINODE_FMT_EXTENTS);

		swap(ip->i_cowfp, tip->i_cowfp);

		if (ip->i_cowfp && ip->i_cowfp->if_bytes)
			xfs_inode_set_cowblocks_tag(ip);
		else
			xfs_inode_clear_cowblocks_tag(ip);
		if (tip->i_cowfp && tip->i_cowfp->if_bytes)
			xfs_inode_set_cowblocks_tag(tip);
		else
			xfs_inode_clear_cowblocks_tag(tip);
	}

	xfs_trans_log_inode(tp, ip,  src_log_flags);
	xfs_trans_log_inode(tp, tip, target_log_flags);

	 
	if (src_log_flags & XFS_ILOG_DOWNER) {
		error = xfs_swap_change_owner(&tp, ip, tip);
		if (error)
			goto out_trans_cancel;
	}
	if (target_log_flags & XFS_ILOG_DOWNER) {
		error = xfs_swap_change_owner(&tp, tip, ip);
		if (error)
			goto out_trans_cancel;
	}

	 
	if (xfs_has_wsync(mp))
		xfs_trans_set_sync(tp);

	error = xfs_trans_commit(tp);

	trace_xfs_swap_extent_after(ip, 0);
	trace_xfs_swap_extent_after(tip, 1);

out_unlock_ilock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	xfs_iunlock(tip, XFS_ILOCK_EXCL);
out_unlock:
	filemap_invalidate_unlock_two(VFS_I(ip)->i_mapping,
				      VFS_I(tip)->i_mapping);
	unlock_two_nondirectories(VFS_I(ip), VFS_I(tip));
	return error;

out_trans_cancel:
	xfs_trans_cancel(tp);
	goto out_unlock_ilock;
}

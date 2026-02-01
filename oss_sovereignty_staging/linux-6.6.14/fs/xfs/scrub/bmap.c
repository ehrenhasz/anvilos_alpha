
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_bit.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_bmap_btree.h"
#include "xfs_rmap.h"
#include "xfs_rmap_btree.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/btree.h"
#include "xfs_ag.h"

 
int
xchk_setup_inode_bmap(
	struct xfs_scrub	*sc)
{
	int			error;

	if (xchk_need_intent_drain(sc))
		xchk_fsgates_enable(sc, XCHK_FSGATES_DRAIN);

	error = xchk_iget_for_scrubbing(sc);
	if (error)
		goto out;

	xchk_ilock(sc, XFS_IOLOCK_EXCL);

	 
	if (S_ISREG(VFS_I(sc->ip)->i_mode) &&
	    sc->sm->sm_type != XFS_SCRUB_TYPE_BMBTA) {
		struct address_space	*mapping = VFS_I(sc->ip)->i_mapping;

		xchk_ilock(sc, XFS_MMAPLOCK_EXCL);

		inode_dio_wait(VFS_I(sc->ip));

		 
		error = filemap_fdatawrite(mapping);
		if (!error)
			error = filemap_fdatawait_keep_errors(mapping);
		if (error && (error != -ENOSPC && error != -EIO))
			goto out;
	}

	 
	error = xchk_trans_alloc(sc, 0);
	if (error)
		goto out;

	xchk_ilock(sc, XFS_ILOCK_EXCL);
out:
	 
	return error;
}

 

struct xchk_bmap_info {
	struct xfs_scrub	*sc;

	 
	struct xfs_iext_cursor	icur;

	 
	struct xfs_bmbt_irec	prev_rec;

	 
	bool			is_rt;

	 
	bool			is_shared;

	 
	bool			was_loaded;

	 
	int			whichfork;
};

 
static inline bool
xchk_bmap_get_rmap(
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec,
	xfs_agblock_t		agbno,
	uint64_t		owner,
	struct xfs_rmap_irec	*rmap)
{
	xfs_fileoff_t		offset;
	unsigned int		rflags = 0;
	int			has_rmap;
	int			error;

	if (info->whichfork == XFS_ATTR_FORK)
		rflags |= XFS_RMAP_ATTR_FORK;
	if (irec->br_state == XFS_EXT_UNWRITTEN)
		rflags |= XFS_RMAP_UNWRITTEN;

	 
	if (info->whichfork == XFS_COW_FORK)
		offset = 0;
	else
		offset = irec->br_startoff;

	 
	if (info->is_shared) {
		error = xfs_rmap_lookup_le_range(info->sc->sa.rmap_cur, agbno,
				owner, offset, rflags, rmap, &has_rmap);
	} else {
		error = xfs_rmap_lookup_le(info->sc->sa.rmap_cur, agbno,
				owner, offset, rflags, rmap, &has_rmap);
	}
	if (!xchk_should_check_xref(info->sc, &error, &info->sc->sa.rmap_cur))
		return false;

	if (!has_rmap)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
			irec->br_startoff);
	return has_rmap;
}

 
STATIC void
xchk_bmap_xref_rmap(
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec,
	xfs_agblock_t		agbno)
{
	struct xfs_rmap_irec	rmap;
	unsigned long long	rmap_end;
	uint64_t		owner = info->sc->ip->i_ino;

	if (!info->sc->sa.rmap_cur || xchk_skip_xref(info->sc->sm))
		return;

	 
	if (!xchk_bmap_get_rmap(info, irec, agbno, owner, &rmap))
		return;

	 
	if (rmap.rm_startblock != agbno)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	rmap_end = (unsigned long long)rmap.rm_startblock + rmap.rm_blockcount;
	if (rmap_end != agbno + irec->br_blockcount)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (rmap.rm_offset != irec->br_startoff)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	rmap_end = (unsigned long long)rmap.rm_offset + rmap.rm_blockcount;
	if (rmap_end != irec->br_startoff + irec->br_blockcount)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (rmap.rm_owner != owner)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (!!(irec->br_state == XFS_EXT_UNWRITTEN) !=
	    !!(rmap.rm_flags & XFS_RMAP_UNWRITTEN))
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	if (!!(info->whichfork == XFS_ATTR_FORK) !=
	    !!(rmap.rm_flags & XFS_RMAP_ATTR_FORK))
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
	if (rmap.rm_flags & XFS_RMAP_BMBT_BLOCK)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
}

 
STATIC void
xchk_bmap_xref_rmap_cow(
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec,
	xfs_agblock_t		agbno)
{
	struct xfs_rmap_irec	rmap;
	unsigned long long	rmap_end;
	uint64_t		owner = XFS_RMAP_OWN_COW;

	if (!info->sc->sa.rmap_cur || xchk_skip_xref(info->sc->sm))
		return;

	 
	if (!xchk_bmap_get_rmap(info, irec, agbno, owner, &rmap))
		return;

	 
	if (rmap.rm_startblock > agbno)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	rmap_end = (unsigned long long)rmap.rm_startblock + rmap.rm_blockcount;
	if (rmap_end < agbno + irec->br_blockcount)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (rmap.rm_owner != owner)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (rmap.rm_flags & XFS_RMAP_ATTR_FORK)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
	if (rmap.rm_flags & XFS_RMAP_BMBT_BLOCK)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
	if (rmap.rm_flags & XFS_RMAP_UNWRITTEN)
		xchk_fblock_xref_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
}

 
STATIC void
xchk_bmap_rt_iextent_xref(
	struct xfs_inode	*ip,
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec)
{
	xchk_xref_is_used_rt_space(info->sc, irec->br_startblock,
			irec->br_blockcount);
}

 
STATIC void
xchk_bmap_iextent_xref(
	struct xfs_inode	*ip,
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec)
{
	struct xfs_owner_info	oinfo;
	struct xfs_mount	*mp = info->sc->mp;
	xfs_agnumber_t		agno;
	xfs_agblock_t		agbno;
	xfs_extlen_t		len;
	int			error;

	agno = XFS_FSB_TO_AGNO(mp, irec->br_startblock);
	agbno = XFS_FSB_TO_AGBNO(mp, irec->br_startblock);
	len = irec->br_blockcount;

	error = xchk_ag_init_existing(info->sc, agno, &info->sc->sa);
	if (!xchk_fblock_process_error(info->sc, info->whichfork,
			irec->br_startoff, &error))
		goto out_free;

	xchk_xref_is_used_space(info->sc, agbno, len);
	xchk_xref_is_not_inode_chunk(info->sc, agbno, len);
	switch (info->whichfork) {
	case XFS_DATA_FORK:
		xchk_bmap_xref_rmap(info, irec, agbno);
		if (!xfs_is_reflink_inode(info->sc->ip)) {
			xfs_rmap_ino_owner(&oinfo, info->sc->ip->i_ino,
					info->whichfork, irec->br_startoff);
			xchk_xref_is_only_owned_by(info->sc, agbno,
					irec->br_blockcount, &oinfo);
			xchk_xref_is_not_shared(info->sc, agbno,
					irec->br_blockcount);
		}
		xchk_xref_is_not_cow_staging(info->sc, agbno,
				irec->br_blockcount);
		break;
	case XFS_ATTR_FORK:
		xchk_bmap_xref_rmap(info, irec, agbno);
		xfs_rmap_ino_owner(&oinfo, info->sc->ip->i_ino,
				info->whichfork, irec->br_startoff);
		xchk_xref_is_only_owned_by(info->sc, agbno, irec->br_blockcount,
				&oinfo);
		xchk_xref_is_not_shared(info->sc, agbno,
				irec->br_blockcount);
		xchk_xref_is_not_cow_staging(info->sc, agbno,
				irec->br_blockcount);
		break;
	case XFS_COW_FORK:
		xchk_bmap_xref_rmap_cow(info, irec, agbno);
		xchk_xref_is_only_owned_by(info->sc, agbno, irec->br_blockcount,
				&XFS_RMAP_OINFO_COW);
		xchk_xref_is_cow_staging(info->sc, agbno,
				irec->br_blockcount);
		xchk_xref_is_not_shared(info->sc, agbno,
				irec->br_blockcount);
		break;
	}

out_free:
	xchk_ag_free(info->sc, &info->sc->sa);
}

 
STATIC void
xchk_bmap_dirattr_extent(
	struct xfs_inode	*ip,
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		off;

	if (!S_ISDIR(VFS_I(ip)->i_mode) && info->whichfork != XFS_ATTR_FORK)
		return;

	if (!xfs_verify_dablk(mp, irec->br_startoff))
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	off = irec->br_startoff + irec->br_blockcount - 1;
	if (!xfs_verify_dablk(mp, off))
		xchk_fblock_set_corrupt(info->sc, info->whichfork, off);
}

 
STATIC void
xchk_bmap_iextent(
	struct xfs_inode	*ip,
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec)
{
	struct xfs_mount	*mp = info->sc->mp;

	 
	if (irec->br_startoff < info->prev_rec.br_startoff +
				info->prev_rec.br_blockcount)
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	if (!xfs_verify_fileext(mp, irec->br_startoff, irec->br_blockcount))
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	xchk_bmap_dirattr_extent(ip, info, irec);

	 
	if (info->is_rt &&
	    !xfs_verify_rtext(mp, irec->br_startblock, irec->br_blockcount))
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
	if (!info->is_rt &&
	    !xfs_verify_fsbext(mp, irec->br_startblock, irec->br_blockcount))
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (irec->br_state == XFS_EXT_UNWRITTEN &&
	    info->whichfork == XFS_ATTR_FORK)
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	if (info->sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return;

	if (info->is_rt)
		xchk_bmap_rt_iextent_xref(ip, info, irec);
	else
		xchk_bmap_iextent_xref(ip, info, irec);
}

 
STATIC int
xchk_bmapbt_rec(
	struct xchk_btree	*bs,
	const union xfs_btree_rec *rec)
{
	struct xfs_bmbt_irec	irec;
	struct xfs_bmbt_irec	iext_irec;
	struct xfs_iext_cursor	icur;
	struct xchk_bmap_info	*info = bs->private;
	struct xfs_inode	*ip = bs->cur->bc_ino.ip;
	struct xfs_buf		*bp = NULL;
	struct xfs_btree_block	*block;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, info->whichfork);
	uint64_t		owner;
	int			i;

	 
	if (xfs_has_crc(bs->cur->bc_mp) &&
	    bs->cur->bc_levels[0].ptr == 1) {
		for (i = 0; i < bs->cur->bc_nlevels - 1; i++) {
			block = xfs_btree_get_block(bs->cur, i, &bp);
			owner = be64_to_cpu(block->bb_u.l.bb_owner);
			if (owner != ip->i_ino)
				xchk_fblock_set_corrupt(bs->sc,
						info->whichfork, 0);
		}
	}

	 
	if (!info->was_loaded)
		return 0;

	xfs_bmbt_disk_get_all(&rec->bmbt, &irec);
	if (xfs_bmap_validate_extent(ip, info->whichfork, &irec) != NULL) {
		xchk_fblock_set_corrupt(bs->sc, info->whichfork,
				irec.br_startoff);
		return 0;
	}

	if (!xfs_iext_lookup_extent(ip, ifp, irec.br_startoff, &icur,
				&iext_irec) ||
	    irec.br_startoff != iext_irec.br_startoff ||
	    irec.br_startblock != iext_irec.br_startblock ||
	    irec.br_blockcount != iext_irec.br_blockcount ||
	    irec.br_state != iext_irec.br_state)
		xchk_fblock_set_corrupt(bs->sc, info->whichfork,
				irec.br_startoff);
	return 0;
}

 
STATIC int
xchk_bmap_btree(
	struct xfs_scrub	*sc,
	int			whichfork,
	struct xchk_bmap_info	*info)
{
	struct xfs_owner_info	oinfo;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(sc->ip, whichfork);
	struct xfs_mount	*mp = sc->mp;
	struct xfs_inode	*ip = sc->ip;
	struct xfs_btree_cur	*cur;
	int			error;

	 
	info->was_loaded = !xfs_need_iread_extents(ifp);

	error = xfs_iread_extents(sc->tp, ip, whichfork);
	if (!xchk_fblock_process_error(sc, whichfork, 0, &error))
		goto out;

	 
	cur = xfs_bmbt_init_cursor(mp, sc->tp, ip, whichfork);
	xfs_rmap_ino_bmbt_owner(&oinfo, ip->i_ino, whichfork);
	error = xchk_btree(sc, cur, xchk_bmapbt_rec, &oinfo, info);
	xfs_btree_del_cursor(cur, error);
out:
	return error;
}

struct xchk_bmap_check_rmap_info {
	struct xfs_scrub	*sc;
	int			whichfork;
	struct xfs_iext_cursor	icur;
};

 
STATIC int
xchk_bmap_check_rmap(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xfs_bmbt_irec		irec;
	struct xfs_rmap_irec		check_rec;
	struct xchk_bmap_check_rmap_info	*sbcri = priv;
	struct xfs_ifork		*ifp;
	struct xfs_scrub		*sc = sbcri->sc;
	bool				have_map;

	 
	if (rec->rm_owner != sc->ip->i_ino)
		return 0;
	if ((sbcri->whichfork == XFS_ATTR_FORK) ^
	    !!(rec->rm_flags & XFS_RMAP_ATTR_FORK))
		return 0;
	if (rec->rm_flags & XFS_RMAP_BMBT_BLOCK)
		return 0;

	 
	ifp = xfs_ifork_ptr(sc->ip, sbcri->whichfork);
	if (!ifp) {
		xchk_fblock_set_corrupt(sc, sbcri->whichfork,
				rec->rm_offset);
		goto out;
	}
	have_map = xfs_iext_lookup_extent(sc->ip, ifp, rec->rm_offset,
			&sbcri->icur, &irec);
	if (!have_map)
		xchk_fblock_set_corrupt(sc, sbcri->whichfork,
				rec->rm_offset);
	 
	check_rec = *rec;
	while (have_map) {
		if (irec.br_startoff != check_rec.rm_offset)
			xchk_fblock_set_corrupt(sc, sbcri->whichfork,
					check_rec.rm_offset);
		if (irec.br_startblock != XFS_AGB_TO_FSB(sc->mp,
				cur->bc_ag.pag->pag_agno,
				check_rec.rm_startblock))
			xchk_fblock_set_corrupt(sc, sbcri->whichfork,
					check_rec.rm_offset);
		if (irec.br_blockcount > check_rec.rm_blockcount)
			xchk_fblock_set_corrupt(sc, sbcri->whichfork,
					check_rec.rm_offset);
		if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
			break;
		check_rec.rm_startblock += irec.br_blockcount;
		check_rec.rm_offset += irec.br_blockcount;
		check_rec.rm_blockcount -= irec.br_blockcount;
		if (check_rec.rm_blockcount == 0)
			break;
		have_map = xfs_iext_next_extent(ifp, &sbcri->icur, &irec);
		if (!have_map)
			xchk_fblock_set_corrupt(sc, sbcri->whichfork,
					check_rec.rm_offset);
	}

out:
	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return -ECANCELED;
	return 0;
}

 
STATIC int
xchk_bmap_check_ag_rmaps(
	struct xfs_scrub		*sc,
	int				whichfork,
	struct xfs_perag		*pag)
{
	struct xchk_bmap_check_rmap_info	sbcri;
	struct xfs_btree_cur		*cur;
	struct xfs_buf			*agf;
	int				error;

	error = xfs_alloc_read_agf(pag, sc->tp, 0, &agf);
	if (error)
		return error;

	cur = xfs_rmapbt_init_cursor(sc->mp, sc->tp, agf, pag);

	sbcri.sc = sc;
	sbcri.whichfork = whichfork;
	error = xfs_rmap_query_all(cur, xchk_bmap_check_rmap, &sbcri);
	if (error == -ECANCELED)
		error = 0;

	xfs_btree_del_cursor(cur, error);
	xfs_trans_brelse(sc->tp, agf);
	return error;
}

 
static bool
xchk_bmap_want_check_rmaps(
	struct xchk_bmap_info	*info)
{
	struct xfs_scrub	*sc = info->sc;
	struct xfs_ifork	*ifp;

	if (!xfs_has_rmapbt(sc->mp))
		return false;
	if (info->whichfork == XFS_COW_FORK)
		return false;
	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return false;

	 
	if (info->is_rt)
		return false;

	 
	ifp = xfs_ifork_ptr(sc->ip, info->whichfork);
	if (ifp->if_format == XFS_DINODE_FMT_EXTENTS && ifp->if_nextents == 0) {
		if (info->whichfork == XFS_DATA_FORK &&
		    i_size_read(VFS_I(sc->ip)) == 0)
			return false;

		return true;
	}

	return false;
}

 
STATIC int
xchk_bmap_check_rmaps(
	struct xfs_scrub	*sc,
	int			whichfork)
{
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno;
	int			error;

	for_each_perag(sc->mp, agno, pag) {
		error = xchk_bmap_check_ag_rmaps(sc, whichfork, pag);
		if (error ||
		    (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)) {
			xfs_perag_rele(pag);
			return error;
		}
	}

	return 0;
}

 
STATIC void
xchk_bmap_iextent_delalloc(
	struct xfs_inode	*ip,
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec)
{
	struct xfs_mount	*mp = info->sc->mp;

	 
	if (irec->br_startoff < info->prev_rec.br_startoff +
				info->prev_rec.br_blockcount)
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	if (!xfs_verify_fileext(mp, irec->br_startoff, irec->br_blockcount))
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);

	 
	if (irec->br_blockcount > XFS_MAX_BMBT_EXTLEN)
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
}

 
static bool
xchk_bmap_iext_mapping(
	struct xchk_bmap_info		*info,
	const struct xfs_bmbt_irec	*irec)
{
	 
	if (irec->br_startblock == HOLESTARTBLOCK)
		return false;
	if (irec->br_blockcount > XFS_MAX_BMBT_EXTLEN)
		return false;
	return true;
}

 
static inline bool
xchk_are_bmaps_contiguous(
	const struct xfs_bmbt_irec	*b1,
	const struct xfs_bmbt_irec	*b2)
{
	 
	if (!xfs_bmap_is_real_extent(b1))
		return false;
	if (!xfs_bmap_is_real_extent(b2))
		return false;

	 
	if (b1->br_startoff + b1->br_blockcount != b2->br_startoff)
		return false;
	if (b1->br_startblock + b1->br_blockcount != b2->br_startblock)
		return false;
	if (b1->br_state != b2->br_state)
		return false;
	return true;
}

 
static bool
xchk_bmap_iext_iter(
	struct xchk_bmap_info	*info,
	struct xfs_bmbt_irec	*irec)
{
	struct xfs_bmbt_irec	got;
	struct xfs_ifork	*ifp;
	unsigned int		nr = 0;

	ifp = xfs_ifork_ptr(info->sc->ip, info->whichfork);

	 
	xfs_iext_next(ifp, &info->icur);
	if (!xfs_iext_get_extent(ifp, &info->icur, irec))
		return false;

	if (!xchk_bmap_iext_mapping(info, irec)) {
		xchk_fblock_set_corrupt(info->sc, info->whichfork,
				irec->br_startoff);
		return false;
	}
	nr++;

	 
	while (xfs_iext_peek_next_extent(ifp, &info->icur, &got)) {
		if (!xchk_are_bmaps_contiguous(irec, &got))
			break;

		if (!xchk_bmap_iext_mapping(info, &got)) {
			xchk_fblock_set_corrupt(info->sc, info->whichfork,
					got.br_startoff);
			return false;
		}
		nr++;

		irec->br_blockcount += got.br_blockcount;
		xfs_iext_next(ifp, &info->icur);
	}

	 
	if (nr > 1 && info->whichfork != XFS_COW_FORK &&
	    howmany_64(irec->br_blockcount, XFS_MAX_BMBT_EXTLEN) < nr)
		xchk_ino_set_preen(info->sc, info->sc->ip->i_ino);

	return true;
}

 
STATIC int
xchk_bmap(
	struct xfs_scrub	*sc,
	int			whichfork)
{
	struct xfs_bmbt_irec	irec;
	struct xchk_bmap_info	info = { NULL };
	struct xfs_mount	*mp = sc->mp;
	struct xfs_inode	*ip = sc->ip;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	xfs_fileoff_t		endoff;
	int			error = 0;

	 
	if (!ifp)
		return -ENOENT;

	info.is_rt = whichfork == XFS_DATA_FORK && XFS_IS_REALTIME_INODE(ip);
	info.whichfork = whichfork;
	info.is_shared = whichfork == XFS_DATA_FORK && xfs_is_reflink_inode(ip);
	info.sc = sc;

	switch (whichfork) {
	case XFS_COW_FORK:
		 
		if (!xfs_has_reflink(mp)) {
			xchk_ino_set_corrupt(sc, sc->ip->i_ino);
			return 0;
		}
		break;
	case XFS_ATTR_FORK:
		if (!xfs_has_attr(mp) && !xfs_has_attr2(mp))
			xchk_ino_set_corrupt(sc, sc->ip->i_ino);
		break;
	default:
		ASSERT(whichfork == XFS_DATA_FORK);
		break;
	}

	 
	switch (ifp->if_format) {
	case XFS_DINODE_FMT_UUID:
	case XFS_DINODE_FMT_DEV:
	case XFS_DINODE_FMT_LOCAL:
		 
		if (whichfork == XFS_COW_FORK)
			xchk_fblock_set_corrupt(sc, whichfork, 0);
		return 0;
	case XFS_DINODE_FMT_EXTENTS:
		break;
	case XFS_DINODE_FMT_BTREE:
		if (whichfork == XFS_COW_FORK) {
			xchk_fblock_set_corrupt(sc, whichfork, 0);
			return 0;
		}

		error = xchk_bmap_btree(sc, whichfork, &info);
		if (error)
			return error;
		break;
	default:
		xchk_fblock_set_corrupt(sc, whichfork, 0);
		return 0;
	}

	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return 0;

	 
	error = xfs_bmap_last_offset(ip, &endoff, whichfork);
	if (!xchk_fblock_process_error(sc, whichfork, 0, &error))
		return error;

	 
	while (xchk_bmap_iext_iter(&info, &irec)) {
		if (xchk_should_terminate(sc, &error) ||
		    (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT))
			return 0;

		if (irec.br_startoff >= endoff) {
			xchk_fblock_set_corrupt(sc, whichfork,
					irec.br_startoff);
			return 0;
		}

		if (isnullstartblock(irec.br_startblock))
			xchk_bmap_iextent_delalloc(ip, &info, &irec);
		else
			xchk_bmap_iextent(ip, &info, &irec);
		memcpy(&info.prev_rec, &irec, sizeof(struct xfs_bmbt_irec));
	}

	if (xchk_bmap_want_check_rmaps(&info)) {
		error = xchk_bmap_check_rmaps(sc, whichfork);
		if (!xchk_fblock_xref_process_error(sc, whichfork, 0, &error))
			return error;
	}

	return 0;
}

 
int
xchk_bmap_data(
	struct xfs_scrub	*sc)
{
	return xchk_bmap(sc, XFS_DATA_FORK);
}

 
int
xchk_bmap_attr(
	struct xfs_scrub	*sc)
{
	return xchk_bmap(sc, XFS_ATTR_FORK);
}

 
int
xchk_bmap_cow(
	struct xfs_scrub	*sc)
{
	return xchk_bmap(sc, XFS_COW_FORK);
}

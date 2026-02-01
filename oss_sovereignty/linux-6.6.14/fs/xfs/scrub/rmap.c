
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_trans.h"
#include "xfs_btree.h"
#include "xfs_rmap.h"
#include "xfs_refcount.h"
#include "xfs_ag.h"
#include "xfs_bit.h"
#include "xfs_alloc.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_refcount_btree.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/btree.h"
#include "scrub/bitmap.h"

 
int
xchk_setup_ag_rmapbt(
	struct xfs_scrub	*sc)
{
	if (xchk_need_intent_drain(sc))
		xchk_fsgates_enable(sc, XCHK_FSGATES_DRAIN);

	return xchk_setup_ag_btree(sc, false);
}

 

struct xchk_rmap {
	 
	struct xfs_rmap_irec	overlap_rec;

	 
	struct xfs_rmap_irec	prev_rec;

	 
	struct xagb_bitmap	fs_owned;
	struct xagb_bitmap	log_owned;
	struct xagb_bitmap	ag_owned;
	struct xagb_bitmap	inobt_owned;
	struct xagb_bitmap	refcbt_owned;

	 
	bool			bitmaps_complete;
};

 
STATIC void
xchk_rmapbt_xref_refc(
	struct xfs_scrub	*sc,
	struct xfs_rmap_irec	*irec)
{
	xfs_agblock_t		fbno;
	xfs_extlen_t		flen;
	bool			non_inode;
	bool			is_bmbt;
	bool			is_attr;
	bool			is_unwritten;
	int			error;

	if (!sc->sa.refc_cur || xchk_skip_xref(sc->sm))
		return;

	non_inode = XFS_RMAP_NON_INODE_OWNER(irec->rm_owner);
	is_bmbt = irec->rm_flags & XFS_RMAP_BMBT_BLOCK;
	is_attr = irec->rm_flags & XFS_RMAP_ATTR_FORK;
	is_unwritten = irec->rm_flags & XFS_RMAP_UNWRITTEN;

	 
	error = xfs_refcount_find_shared(sc->sa.refc_cur, irec->rm_startblock,
			irec->rm_blockcount, &fbno, &flen, false);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.refc_cur))
		return;
	if (flen != 0 && (non_inode || is_attr || is_bmbt || is_unwritten))
		xchk_btree_xref_set_corrupt(sc, sc->sa.refc_cur, 0);
}

 
STATIC void
xchk_rmapbt_xref(
	struct xfs_scrub	*sc,
	struct xfs_rmap_irec	*irec)
{
	xfs_agblock_t		agbno = irec->rm_startblock;
	xfs_extlen_t		len = irec->rm_blockcount;

	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return;

	xchk_xref_is_used_space(sc, agbno, len);
	if (irec->rm_owner == XFS_RMAP_OWN_INODES)
		xchk_xref_is_inode_chunk(sc, agbno, len);
	else
		xchk_xref_is_not_inode_chunk(sc, agbno, len);
	if (irec->rm_owner == XFS_RMAP_OWN_COW)
		xchk_xref_is_cow_staging(sc, irec->rm_startblock,
				irec->rm_blockcount);
	else
		xchk_rmapbt_xref_refc(sc, irec);
}

 
STATIC void
xchk_rmapbt_check_unwritten_in_keyflags(
	struct xchk_btree	*bs)
{
	struct xfs_scrub	*sc = bs->sc;
	struct xfs_btree_cur	*cur = bs->cur;
	struct xfs_btree_block	*keyblock;
	union xfs_btree_key	*lkey, *hkey;
	__be64			badflag = cpu_to_be64(XFS_RMAP_OFF_UNWRITTEN);
	unsigned int		level;

	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_PREEN)
		return;

	for (level = 1; level < cur->bc_nlevels; level++) {
		struct xfs_buf	*bp;
		unsigned int	ptr;

		 
		if (cur->bc_levels[level].ptr > 1)
			continue;

		keyblock = xfs_btree_get_block(cur, level, &bp);
		for (ptr = 1; ptr <= be16_to_cpu(keyblock->bb_numrecs); ptr++) {
			lkey = xfs_btree_key_addr(cur, ptr, keyblock);

			if (lkey->rmap.rm_offset & badflag) {
				xchk_btree_set_preen(sc, cur, level);
				break;
			}

			hkey = xfs_btree_high_key_addr(cur, ptr, keyblock);
			if (hkey->rmap.rm_offset & badflag) {
				xchk_btree_set_preen(sc, cur, level);
				break;
			}
		}
	}
}

static inline bool
xchk_rmapbt_is_shareable(
	struct xfs_scrub		*sc,
	const struct xfs_rmap_irec	*irec)
{
	if (!xfs_has_reflink(sc->mp))
		return false;
	if (XFS_RMAP_NON_INODE_OWNER(irec->rm_owner))
		return false;
	if (irec->rm_flags & (XFS_RMAP_BMBT_BLOCK | XFS_RMAP_ATTR_FORK |
			      XFS_RMAP_UNWRITTEN))
		return false;
	return true;
}

 
STATIC void
xchk_rmapbt_check_overlapping(
	struct xchk_btree		*bs,
	struct xchk_rmap		*cr,
	const struct xfs_rmap_irec	*irec)
{
	xfs_agblock_t			pnext, inext;

	if (bs->sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return;

	 
	if (cr->overlap_rec.rm_blockcount == 0)
		goto set_prev;

	 
	pnext = cr->overlap_rec.rm_startblock + cr->overlap_rec.rm_blockcount;
	if (pnext <= irec->rm_startblock)
		goto set_prev;

	 
	if (!xchk_rmapbt_is_shareable(bs->sc, &cr->overlap_rec) ||
	    !xchk_rmapbt_is_shareable(bs->sc, irec))
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);

	 
	inext = irec->rm_startblock + irec->rm_blockcount;
	if (pnext > inext)
		return;

set_prev:
	memcpy(&cr->overlap_rec, irec, sizeof(struct xfs_rmap_irec));
}

 
static inline bool
xchk_rmap_mergeable(
	struct xchk_rmap		*cr,
	const struct xfs_rmap_irec	*r2)
{
	const struct xfs_rmap_irec	*r1 = &cr->prev_rec;

	 
	if (cr->prev_rec.rm_blockcount == 0)
		return false;

	if (r1->rm_owner != r2->rm_owner)
		return false;
	if (r1->rm_startblock + r1->rm_blockcount != r2->rm_startblock)
		return false;
	if ((unsigned long long)r1->rm_blockcount + r2->rm_blockcount >
	    XFS_RMAP_LEN_MAX)
		return false;
	if (XFS_RMAP_NON_INODE_OWNER(r2->rm_owner))
		return true;
	 
	if (r1->rm_flags != r2->rm_flags)
		return false;
	if (r1->rm_flags & XFS_RMAP_BMBT_BLOCK)
		return true;
	return r1->rm_offset + r1->rm_blockcount == r2->rm_offset;
}

 
STATIC void
xchk_rmapbt_check_mergeable(
	struct xchk_btree		*bs,
	struct xchk_rmap		*cr,
	const struct xfs_rmap_irec	*irec)
{
	if (bs->sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return;

	if (xchk_rmap_mergeable(cr, irec))
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);

	memcpy(&cr->prev_rec, irec, sizeof(struct xfs_rmap_irec));
}

 
STATIC int
xchk_rmapbt_mark_bitmap(
	struct xchk_btree		*bs,
	struct xchk_rmap		*cr,
	const struct xfs_rmap_irec	*irec)
{
	struct xfs_scrub		*sc = bs->sc;
	struct xagb_bitmap		*bmp = NULL;
	xfs_extlen_t			fsbcount = irec->rm_blockcount;

	 
	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return 0;

	 
	if (!cr->bitmaps_complete)
		return 0;

	switch (irec->rm_owner) {
	case XFS_RMAP_OWN_FS:
		bmp = &cr->fs_owned;
		break;
	case XFS_RMAP_OWN_LOG:
		bmp = &cr->log_owned;
		break;
	case XFS_RMAP_OWN_AG:
		bmp = &cr->ag_owned;
		break;
	case XFS_RMAP_OWN_INOBT:
		bmp = &cr->inobt_owned;
		break;
	case XFS_RMAP_OWN_REFC:
		bmp = &cr->refcbt_owned;
		break;
	}

	if (!bmp)
		return 0;

	if (xagb_bitmap_test(bmp, irec->rm_startblock, &fsbcount)) {
		 
		if (fsbcount < irec->rm_blockcount)
			xchk_btree_xref_set_corrupt(bs->sc,
					bs->sc->sa.rmap_cur, 0);
	} else {
		 
		xchk_btree_xref_set_corrupt(bs->sc, bs->sc->sa.rmap_cur, 0);
	}

	 
	return xagb_bitmap_clear(bmp, irec->rm_startblock, irec->rm_blockcount);
}

 
STATIC int
xchk_rmapbt_rec(
	struct xchk_btree	*bs,
	const union xfs_btree_rec *rec)
{
	struct xchk_rmap	*cr = bs->private;
	struct xfs_rmap_irec	irec;

	if (xfs_rmap_btrec_to_irec(rec, &irec) != NULL ||
	    xfs_rmap_check_irec(bs->cur, &irec) != NULL) {
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return 0;
	}

	xchk_rmapbt_check_unwritten_in_keyflags(bs);
	xchk_rmapbt_check_mergeable(bs, cr, &irec);
	xchk_rmapbt_check_overlapping(bs, cr, &irec);
	xchk_rmapbt_xref(bs->sc, &irec);

	return xchk_rmapbt_mark_bitmap(bs, cr, &irec);
}

 
STATIC int
xchk_rmapbt_walk_agfl(
	struct xfs_mount	*mp,
	xfs_agblock_t		agbno,
	void			*priv)
{
	struct xagb_bitmap	*bitmap = priv;

	return xagb_bitmap_set(bitmap, agbno, 1);
}

 
STATIC int
xchk_rmapbt_walk_ag_metadata(
	struct xfs_scrub	*sc,
	struct xchk_rmap	*cr)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_buf		*agfl_bp;
	struct xfs_agf		*agf = sc->sa.agf_bp->b_addr;
	struct xfs_btree_cur	*cur;
	int			error;

	 
	error = xagb_bitmap_set(&cr->fs_owned, XFS_SB_BLOCK(mp),
			XFS_AGFL_BLOCK(mp) - XFS_SB_BLOCK(mp) + 1);
	if (error)
		goto out;

	 
	if (xfs_ag_contains_log(mp, sc->sa.pag->pag_agno)) {
		error = xagb_bitmap_set(&cr->log_owned,
				XFS_FSB_TO_AGBNO(mp, mp->m_sb.sb_logstart),
				mp->m_sb.sb_logblocks);
		if (error)
			goto out;
	}

	 
	cur = sc->sa.bno_cur;
	if (!cur)
		cur = xfs_allocbt_init_cursor(sc->mp, sc->tp, sc->sa.agf_bp,
				sc->sa.pag, XFS_BTNUM_BNO);
	error = xagb_bitmap_set_btblocks(&cr->ag_owned, cur);
	if (cur != sc->sa.bno_cur)
		xfs_btree_del_cursor(cur, error);
	if (error)
		goto out;

	cur = sc->sa.cnt_cur;
	if (!cur)
		cur = xfs_allocbt_init_cursor(sc->mp, sc->tp, sc->sa.agf_bp,
				sc->sa.pag, XFS_BTNUM_CNT);
	error = xagb_bitmap_set_btblocks(&cr->ag_owned, cur);
	if (cur != sc->sa.cnt_cur)
		xfs_btree_del_cursor(cur, error);
	if (error)
		goto out;

	error = xagb_bitmap_set_btblocks(&cr->ag_owned, sc->sa.rmap_cur);
	if (error)
		goto out;

	error = xfs_alloc_read_agfl(sc->sa.pag, sc->tp, &agfl_bp);
	if (error)
		goto out;

	error = xfs_agfl_walk(sc->mp, agf, agfl_bp, xchk_rmapbt_walk_agfl,
			&cr->ag_owned);
	xfs_trans_brelse(sc->tp, agfl_bp);
	if (error)
		goto out;

	 
	cur = sc->sa.ino_cur;
	if (!cur)
		cur = xfs_inobt_init_cursor(sc->sa.pag, sc->tp, sc->sa.agi_bp,
				XFS_BTNUM_INO);
	error = xagb_bitmap_set_btblocks(&cr->inobt_owned, cur);
	if (cur != sc->sa.ino_cur)
		xfs_btree_del_cursor(cur, error);
	if (error)
		goto out;

	if (xfs_has_finobt(sc->mp)) {
		cur = sc->sa.fino_cur;
		if (!cur)
			cur = xfs_inobt_init_cursor(sc->sa.pag, sc->tp,
					sc->sa.agi_bp, XFS_BTNUM_FINO);
		error = xagb_bitmap_set_btblocks(&cr->inobt_owned, cur);
		if (cur != sc->sa.fino_cur)
			xfs_btree_del_cursor(cur, error);
		if (error)
			goto out;
	}

	 
	if (xfs_has_reflink(sc->mp)) {
		cur = sc->sa.refc_cur;
		if (!cur)
			cur = xfs_refcountbt_init_cursor(sc->mp, sc->tp,
					sc->sa.agf_bp, sc->sa.pag);
		error = xagb_bitmap_set_btblocks(&cr->refcbt_owned, cur);
		if (cur != sc->sa.refc_cur)
			xfs_btree_del_cursor(cur, error);
		if (error)
			goto out;
	}

out:
	 
	if (error)
		xchk_btree_xref_process_error(sc, sc->sa.rmap_cur,
				sc->sa.rmap_cur->bc_nlevels - 1, &error);
	else
		cr->bitmaps_complete = true;
	return 0;
}

 
STATIC void
xchk_rmapbt_check_bitmaps(
	struct xfs_scrub	*sc,
	struct xchk_rmap	*cr)
{
	struct xfs_btree_cur	*cur = sc->sa.rmap_cur;
	unsigned int		level;

	if (sc->sm->sm_flags & (XFS_SCRUB_OFLAG_CORRUPT |
				XFS_SCRUB_OFLAG_XFAIL))
		return;
	if (!cur)
		return;
	level = cur->bc_nlevels - 1;

	 
	if (xagb_bitmap_hweight(&cr->fs_owned) != 0)
		xchk_btree_xref_set_corrupt(sc, cur, level);

	if (xagb_bitmap_hweight(&cr->log_owned) != 0)
		xchk_btree_xref_set_corrupt(sc, cur, level);

	if (xagb_bitmap_hweight(&cr->ag_owned) != 0)
		xchk_btree_xref_set_corrupt(sc, cur, level);

	if (xagb_bitmap_hweight(&cr->inobt_owned) != 0)
		xchk_btree_xref_set_corrupt(sc, cur, level);

	if (xagb_bitmap_hweight(&cr->refcbt_owned) != 0)
		xchk_btree_xref_set_corrupt(sc, cur, level);
}

 
int
xchk_rmapbt(
	struct xfs_scrub	*sc)
{
	struct xchk_rmap	*cr;
	int			error;

	cr = kzalloc(sizeof(struct xchk_rmap), XCHK_GFP_FLAGS);
	if (!cr)
		return -ENOMEM;

	xagb_bitmap_init(&cr->fs_owned);
	xagb_bitmap_init(&cr->log_owned);
	xagb_bitmap_init(&cr->ag_owned);
	xagb_bitmap_init(&cr->inobt_owned);
	xagb_bitmap_init(&cr->refcbt_owned);

	error = xchk_rmapbt_walk_ag_metadata(sc, cr);
	if (error)
		goto out;

	error = xchk_btree(sc, sc->sa.rmap_cur, xchk_rmapbt_rec,
			&XFS_RMAP_OINFO_AG, cr);
	if (error)
		goto out;

	xchk_rmapbt_check_bitmaps(sc, cr);

out:
	xagb_bitmap_destroy(&cr->refcbt_owned);
	xagb_bitmap_destroy(&cr->inobt_owned);
	xagb_bitmap_destroy(&cr->ag_owned);
	xagb_bitmap_destroy(&cr->log_owned);
	xagb_bitmap_destroy(&cr->fs_owned);
	kfree(cr);
	return error;
}

 
void
xchk_xref_is_only_owned_by(
	struct xfs_scrub		*sc,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_rmap_matches		res;
	int				error;

	if (!sc->sa.rmap_cur || xchk_skip_xref(sc->sm))
		return;

	error = xfs_rmap_count_owners(sc->sa.rmap_cur, bno, len, oinfo, &res);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.rmap_cur))
		return;
	if (res.matches != 1)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
	if (res.bad_non_owner_matches)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
	if (res.non_owner_matches)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
}

 
void
xchk_xref_is_not_owned_by(
	struct xfs_scrub		*sc,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_rmap_matches		res;
	int				error;

	if (!sc->sa.rmap_cur || xchk_skip_xref(sc->sm))
		return;

	error = xfs_rmap_count_owners(sc->sa.rmap_cur, bno, len, oinfo, &res);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.rmap_cur))
		return;
	if (res.matches != 0)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
	if (res.bad_non_owner_matches)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
}

 
void
xchk_xref_has_no_owner(
	struct xfs_scrub	*sc,
	xfs_agblock_t		bno,
	xfs_extlen_t		len)
{
	enum xbtree_recpacking	outcome;
	int			error;

	if (!sc->sa.rmap_cur || xchk_skip_xref(sc->sm))
		return;

	error = xfs_rmap_has_records(sc->sa.rmap_cur, bno, len, &outcome);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.rmap_cur))
		return;
	if (outcome != XBTREE_RECPACKING_EMPTY)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
}

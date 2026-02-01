
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_inode.h"
#include "xfs_alloc.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc.h"
#include "xfs_ialloc_btree.h"
#include "xfs_rmap.h"
#include "xfs_rmap_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_extent_busy.h"
#include "xfs_ag.h"
#include "xfs_ag_resv.h"
#include "xfs_quota.h"
#include "xfs_qm.h"
#include "xfs_bmap.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "xfs_attr_remote.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/trace.h"
#include "scrub/repair.h"
#include "scrub/bitmap.h"
#include "scrub/reap.h"

 

 
struct xreap_state {
	struct xfs_scrub		*sc;

	 
	const struct xfs_owner_info	*oinfo;
	enum xfs_ag_resv_type		resv;

	 
	bool				force_roll;

	 
	unsigned int			deferred;

	 
	unsigned int			invalidated;

	 
	unsigned long long		total_deferred;
};

 
STATIC int
xreap_put_freelist(
	struct xfs_scrub	*sc,
	xfs_agblock_t		agbno)
{
	struct xfs_buf		*agfl_bp;
	int			error;

	 
	error = xrep_fix_freelist(sc, true);
	if (error)
		return error;

	 
	error = xfs_rmap_alloc(sc->tp, sc->sa.agf_bp, sc->sa.pag, agbno, 1,
			&XFS_RMAP_OINFO_AG);
	if (error)
		return error;

	 
	error = xfs_alloc_read_agfl(sc->sa.pag, sc->tp, &agfl_bp);
	if (error)
		return error;

	error = xfs_alloc_put_freelist(sc->sa.pag, sc->tp, sc->sa.agf_bp,
			agfl_bp, agbno, 0);
	if (error)
		return error;
	xfs_extent_busy_insert(sc->tp, sc->sa.pag, agbno, 1,
			XFS_EXTENT_BUSY_SKIP_DISCARD);

	return 0;
}

 
static inline bool xreap_dirty(const struct xreap_state *rs)
{
	if (rs->force_roll)
		return true;
	if (rs->deferred)
		return true;
	if (rs->invalidated)
		return true;
	if (rs->total_deferred)
		return true;
	return false;
}

#define XREAP_MAX_BINVAL	(2048)

 
static inline bool xreap_want_roll(const struct xreap_state *rs)
{
	if (rs->force_roll)
		return true;
	if (rs->deferred > XREP_MAX_ITRUNCATE_EFIS)
		return true;
	if (rs->invalidated > XREAP_MAX_BINVAL)
		return true;
	return false;
}

static inline void xreap_reset(struct xreap_state *rs)
{
	rs->total_deferred += rs->deferred;
	rs->deferred = 0;
	rs->invalidated = 0;
	rs->force_roll = false;
}

#define XREAP_MAX_DEFER_CHAIN		(2048)

 
static inline bool
xreap_want_defer_finish(const struct xreap_state *rs)
{
	if (rs->force_roll)
		return true;
	if (rs->total_deferred > XREAP_MAX_DEFER_CHAIN)
		return true;
	return false;
}

static inline void xreap_defer_finish_reset(struct xreap_state *rs)
{
	rs->total_deferred = 0;
	rs->deferred = 0;
	rs->invalidated = 0;
	rs->force_roll = false;
}

 
STATIC void
xreap_agextent_binval(
	struct xreap_state	*rs,
	xfs_agblock_t		agbno,
	xfs_extlen_t		*aglenp)
{
	struct xfs_scrub	*sc = rs->sc;
	struct xfs_perag	*pag = sc->sa.pag;
	struct xfs_mount	*mp = sc->mp;
	xfs_agnumber_t		agno = sc->sa.pag->pag_agno;
	xfs_agblock_t		agbno_next = agbno + *aglenp;
	xfs_agblock_t		bno = agbno;

	 
	if (!xfs_verify_agbno(pag, agbno) ||
	    !xfs_verify_agbno(pag, agbno_next - 1))
		return;

	 
	while (bno < agbno_next) {
		xfs_agblock_t	fsbcount;
		xfs_agblock_t	max_fsbs;

		 
		max_fsbs = min_t(xfs_agblock_t, agbno_next - bno,
				xfs_attr3_rmt_blocks(mp, XFS_XATTR_SIZE_MAX));

		for (fsbcount = 1; fsbcount < max_fsbs; fsbcount++) {
			struct xfs_buf	*bp = NULL;
			xfs_daddr_t	daddr;
			int		error;

			daddr = XFS_AGB_TO_DADDR(mp, agno, bno);
			error = xfs_buf_incore(mp->m_ddev_targp, daddr,
					XFS_FSB_TO_BB(mp, fsbcount),
					XBF_LIVESCAN, &bp);
			if (error)
				continue;

			xfs_trans_bjoin(sc->tp, bp);
			xfs_trans_binval(sc->tp, bp);
			rs->invalidated++;

			 
			if (rs->invalidated > XREAP_MAX_BINVAL) {
				*aglenp -= agbno_next - bno;
				goto out;
			}
		}

		bno++;
	}

out:
	trace_xreap_agextent_binval(sc->sa.pag, agbno, *aglenp);
}

 
STATIC int
xreap_agextent_select(
	struct xreap_state	*rs,
	xfs_agblock_t		agbno,
	xfs_agblock_t		agbno_next,
	bool			*crosslinked,
	xfs_extlen_t		*aglenp)
{
	struct xfs_scrub	*sc = rs->sc;
	struct xfs_btree_cur	*cur;
	xfs_agblock_t		bno = agbno + 1;
	xfs_extlen_t		len = 1;
	int			error;

	 
	cur = xfs_rmapbt_init_cursor(sc->mp, sc->tp, sc->sa.agf_bp,
			sc->sa.pag);
	error = xfs_rmap_has_other_keys(cur, agbno, 1, rs->oinfo,
			crosslinked);
	if (error)
		goto out_cur;

	 
	if (rs->resv == XFS_AG_RESV_AGFL)
		goto out_found;

	 
	while (bno < agbno_next) {
		bool		also_crosslinked;

		error = xfs_rmap_has_other_keys(cur, bno, 1, rs->oinfo,
				&also_crosslinked);
		if (error)
			goto out_cur;

		if (*crosslinked != also_crosslinked)
			break;

		len++;
		bno++;
	}

out_found:
	*aglenp = len;
	trace_xreap_agextent_select(sc->sa.pag, agbno, len, *crosslinked);
out_cur:
	xfs_btree_del_cursor(cur, error);
	return error;
}

 
STATIC int
xreap_agextent_iter(
	struct xreap_state	*rs,
	xfs_agblock_t		agbno,
	xfs_extlen_t		*aglenp,
	bool			crosslinked)
{
	struct xfs_scrub	*sc = rs->sc;
	xfs_fsblock_t		fsbno;
	int			error = 0;

	fsbno = XFS_AGB_TO_FSB(sc->mp, sc->sa.pag->pag_agno, agbno);

	 
	if (crosslinked) {
		trace_xreap_dispose_unmap_extent(sc->sa.pag, agbno, *aglenp);

		rs->force_roll = true;
		return xfs_rmap_free(sc->tp, sc->sa.agf_bp, sc->sa.pag, agbno,
				*aglenp, rs->oinfo);
	}

	trace_xreap_dispose_free_extent(sc->sa.pag, agbno, *aglenp);

	 
	xreap_agextent_binval(rs, agbno, aglenp);
	if (*aglenp == 0) {
		ASSERT(xreap_want_roll(rs));
		return 0;
	}

	 
	if (rs->resv == XFS_AG_RESV_AGFL) {
		ASSERT(*aglenp == 1);
		error = xreap_put_freelist(sc, agbno);
		if (error)
			return error;

		rs->force_roll = true;
		return 0;
	}

	 
	error = __xfs_free_extent_later(sc->tp, fsbno, *aglenp, rs->oinfo,
			rs->resv, true);
	if (error)
		return error;

	rs->deferred++;
	return 0;
}

 
STATIC int
xreap_agmeta_extent(
	uint64_t		fsbno,
	uint64_t		len,
	void			*priv)
{
	struct xreap_state	*rs = priv;
	struct xfs_scrub	*sc = rs->sc;
	xfs_agblock_t		agbno = fsbno;
	xfs_agblock_t		agbno_next = agbno + len;
	int			error = 0;

	ASSERT(len <= XFS_MAX_BMBT_EXTLEN);
	ASSERT(sc->ip == NULL);

	while (agbno < agbno_next) {
		xfs_extlen_t	aglen;
		bool		crosslinked;

		error = xreap_agextent_select(rs, agbno, agbno_next,
				&crosslinked, &aglen);
		if (error)
			return error;

		error = xreap_agextent_iter(rs, agbno, &aglen, crosslinked);
		if (error)
			return error;

		if (xreap_want_defer_finish(rs)) {
			error = xrep_defer_finish(sc);
			if (error)
				return error;
			xreap_defer_finish_reset(rs);
		} else if (xreap_want_roll(rs)) {
			error = xrep_roll_ag_trans(sc);
			if (error)
				return error;
			xreap_reset(rs);
		}

		agbno += aglen;
	}

	return 0;
}

 
int
xrep_reap_agblocks(
	struct xfs_scrub		*sc,
	struct xagb_bitmap		*bitmap,
	const struct xfs_owner_info	*oinfo,
	enum xfs_ag_resv_type		type)
{
	struct xreap_state		rs = {
		.sc			= sc,
		.oinfo			= oinfo,
		.resv			= type,
	};
	int				error;

	ASSERT(xfs_has_rmapbt(sc->mp));
	ASSERT(sc->ip == NULL);

	error = xagb_bitmap_walk(bitmap, xreap_agmeta_extent, &rs);
	if (error)
		return error;

	if (xreap_dirty(&rs))
		return xrep_defer_finish(sc);

	return 0;
}

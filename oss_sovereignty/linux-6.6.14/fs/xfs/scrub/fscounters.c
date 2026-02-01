
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_mount.h"
#include "xfs_alloc.h"
#include "xfs_ialloc.h"
#include "xfs_health.h"
#include "xfs_btree.h"
#include "xfs_ag.h"
#include "xfs_rtalloc.h"
#include "xfs_inode.h"
#include "xfs_icache.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/trace.h"

 

struct xchk_fscounters {
	struct xfs_scrub	*sc;
	uint64_t		icount;
	uint64_t		ifree;
	uint64_t		fdblocks;
	uint64_t		frextents;
	unsigned long long	icount_min;
	unsigned long long	icount_max;
	bool			frozen;
};

 
#define XCHK_FSCOUNT_MIN_VARIANCE	(512)

 
STATIC int
xchk_fscount_warmup(
	struct xfs_scrub	*sc)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_buf		*agi_bp = NULL;
	struct xfs_buf		*agf_bp = NULL;
	struct xfs_perag	*pag = NULL;
	xfs_agnumber_t		agno;
	int			error = 0;

	for_each_perag(mp, agno, pag) {
		if (xchk_should_terminate(sc, &error))
			break;
		if (xfs_perag_initialised_agi(pag) &&
		    xfs_perag_initialised_agf(pag))
			continue;

		 
		error = xfs_ialloc_read_agi(pag, sc->tp, &agi_bp);
		if (error)
			break;
		error = xfs_alloc_read_agf(pag, sc->tp, 0, &agf_bp);
		if (error)
			break;

		 
		if (!xfs_perag_initialised_agi(pag) ||
		    !xfs_perag_initialised_agf(pag)) {
			error = -EFSCORRUPTED;
			break;
		}

		xfs_buf_relse(agf_bp);
		agf_bp = NULL;
		xfs_buf_relse(agi_bp);
		agi_bp = NULL;
	}

	if (agf_bp)
		xfs_buf_relse(agf_bp);
	if (agi_bp)
		xfs_buf_relse(agi_bp);
	if (pag)
		xfs_perag_rele(pag);
	return error;
}

static inline int
xchk_fsfreeze(
	struct xfs_scrub	*sc)
{
	int			error;

	error = freeze_super(sc->mp->m_super, FREEZE_HOLDER_KERNEL);
	trace_xchk_fsfreeze(sc, error);
	return error;
}

static inline int
xchk_fsthaw(
	struct xfs_scrub	*sc)
{
	int			error;

	 
	error = thaw_super(sc->mp->m_super, FREEZE_HOLDER_KERNEL);
	trace_xchk_fsthaw(sc, error);
	return error;
}

 
STATIC int
xchk_fscounters_freeze(
	struct xfs_scrub	*sc)
{
	struct xchk_fscounters	*fsc = sc->buf;
	int			error = 0;

	if (sc->flags & XCHK_HAVE_FREEZE_PROT) {
		sc->flags &= ~XCHK_HAVE_FREEZE_PROT;
		mnt_drop_write_file(sc->file);
	}

	 
	while ((error = xchk_fsfreeze(sc)) == -EBUSY) {
		if (xchk_should_terminate(sc, &error))
			return error;

		delay(HZ / 10);
	}
	if (error)
		return error;

	fsc->frozen = true;
	return 0;
}

 
STATIC void
xchk_fscounters_cleanup(
	void			*buf)
{
	struct xchk_fscounters	*fsc = buf;
	struct xfs_scrub	*sc = fsc->sc;
	int			error;

	if (!fsc->frozen)
		return;

	error = xchk_fsthaw(sc);
	if (error)
		xfs_emerg(sc->mp, "still frozen after scrub, err=%d", error);
	else
		fsc->frozen = false;
}

int
xchk_setup_fscounters(
	struct xfs_scrub	*sc)
{
	struct xchk_fscounters	*fsc;
	int			error;

	 
	if (!xfs_has_lazysbcount(sc->mp))
		xchk_fsgates_enable(sc, XCHK_FSGATES_DRAIN);

	sc->buf = kzalloc(sizeof(struct xchk_fscounters), XCHK_GFP_FLAGS);
	if (!sc->buf)
		return -ENOMEM;
	sc->buf_cleanup = xchk_fscounters_cleanup;
	fsc = sc->buf;
	fsc->sc = sc;

	xfs_icount_range(sc->mp, &fsc->icount_min, &fsc->icount_max);

	 
	error = xchk_fscount_warmup(sc);
	if (error)
		return error;

	 
	if (sc->flags & XCHK_TRY_HARDER) {
		error = xchk_fscounters_freeze(sc);
		if (error)
			return error;
	}

	return xfs_trans_alloc_empty(sc->mp, &sc->tp);
}

 

 
static int
xchk_fscount_btreeblks(
	struct xfs_scrub	*sc,
	struct xchk_fscounters	*fsc,
	xfs_agnumber_t		agno)
{
	xfs_extlen_t		blocks;
	int			error;

	error = xchk_ag_init_existing(sc, agno, &sc->sa);
	if (error)
		goto out_free;

	error = xfs_btree_count_blocks(sc->sa.bno_cur, &blocks);
	if (error)
		goto out_free;
	fsc->fdblocks += blocks - 1;

	error = xfs_btree_count_blocks(sc->sa.cnt_cur, &blocks);
	if (error)
		goto out_free;
	fsc->fdblocks += blocks - 1;

out_free:
	xchk_ag_free(sc, &sc->sa);
	return error;
}

 
STATIC int
xchk_fscount_aggregate_agcounts(
	struct xfs_scrub	*sc,
	struct xchk_fscounters	*fsc)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_perag	*pag;
	uint64_t		delayed;
	xfs_agnumber_t		agno;
	int			tries = 8;
	int			error = 0;

retry:
	fsc->icount = 0;
	fsc->ifree = 0;
	fsc->fdblocks = 0;

	for_each_perag(mp, agno, pag) {
		if (xchk_should_terminate(sc, &error))
			break;

		 
		if (!xfs_perag_initialised_agi(pag) ||
		    !xfs_perag_initialised_agf(pag)) {
			error = -EFSCORRUPTED;
			break;
		}

		 
		fsc->icount += pag->pagi_count;
		fsc->ifree += pag->pagi_freecount;

		 
		fsc->fdblocks += pag->pagf_freeblks;
		fsc->fdblocks += pag->pagf_flcount;
		if (xfs_has_lazysbcount(sc->mp)) {
			fsc->fdblocks += pag->pagf_btreeblks;
		} else {
			error = xchk_fscount_btreeblks(sc, fsc, agno);
			if (error)
				break;
		}

		 
		fsc->fdblocks -= pag->pag_meta_resv.ar_reserved;
		fsc->fdblocks -= pag->pag_rmapbt_resv.ar_orig_reserved;

	}
	if (pag)
		xfs_perag_rele(pag);
	if (error) {
		xchk_set_incomplete(sc);
		return error;
	}

	 
	fsc->fdblocks -= mp->m_resblks_avail;

	 
	delayed = percpu_counter_sum(&mp->m_delalloc_blks);
	fsc->fdblocks -= delayed;

	trace_xchk_fscounters_calc(mp, fsc->icount, fsc->ifree, fsc->fdblocks,
			delayed);


	 
	if (fsc->icount < fsc->icount_min || fsc->icount > fsc->icount_max ||
	    fsc->fdblocks > mp->m_sb.sb_dblocks ||
	    fsc->ifree > fsc->icount_max)
		return -EFSCORRUPTED;

	 
	if (fsc->ifree > fsc->icount) {
		if (tries--)
			goto retry;
		return -EDEADLOCK;
	}

	return 0;
}

#ifdef CONFIG_XFS_RT
STATIC int
xchk_fscount_add_frextent(
	struct xfs_mount		*mp,
	struct xfs_trans		*tp,
	const struct xfs_rtalloc_rec	*rec,
	void				*priv)
{
	struct xchk_fscounters		*fsc = priv;
	int				error = 0;

	fsc->frextents += rec->ar_extcount;

	xchk_should_terminate(fsc->sc, &error);
	return error;
}

 
STATIC int
xchk_fscount_count_frextents(
	struct xfs_scrub	*sc,
	struct xchk_fscounters	*fsc)
{
	struct xfs_mount	*mp = sc->mp;
	int			error;

	fsc->frextents = 0;
	if (!xfs_has_realtime(mp))
		return 0;

	xfs_ilock(sc->mp->m_rbmip, XFS_ILOCK_SHARED | XFS_ILOCK_RTBITMAP);
	error = xfs_rtalloc_query_all(sc->mp, sc->tp,
			xchk_fscount_add_frextent, fsc);
	if (error) {
		xchk_set_incomplete(sc);
		goto out_unlock;
	}

out_unlock:
	xfs_iunlock(sc->mp->m_rbmip, XFS_ILOCK_SHARED | XFS_ILOCK_RTBITMAP);
	return error;
}
#else
STATIC int
xchk_fscount_count_frextents(
	struct xfs_scrub	*sc,
	struct xchk_fscounters	*fsc)
{
	fsc->frextents = 0;
	return 0;
}
#endif  

 

 
static inline bool
xchk_fscount_within_range(
	struct xfs_scrub	*sc,
	const int64_t		old_value,
	struct percpu_counter	*counter,
	uint64_t		expected)
{
	int64_t			min_value, max_value;
	int64_t			curr_value = percpu_counter_sum(counter);

	trace_xchk_fscounters_within_range(sc->mp, expected, curr_value,
			old_value);

	 
	if (curr_value < 0)
		return false;

	 
	if (curr_value == expected)
		return true;

	min_value = min(old_value, curr_value);
	max_value = max(old_value, curr_value);

	 
	if (expected >= min_value && expected <= max_value)
		return true;

	 
	return false;
}

 
int
xchk_fscounters(
	struct xfs_scrub	*sc)
{
	struct xfs_mount	*mp = sc->mp;
	struct xchk_fscounters	*fsc = sc->buf;
	int64_t			icount, ifree, fdblocks, frextents;
	bool			try_again = false;
	int			error;

	 
	icount = percpu_counter_sum(&mp->m_icount);
	ifree = percpu_counter_sum(&mp->m_ifree);
	fdblocks = percpu_counter_sum(&mp->m_fdblocks);
	frextents = percpu_counter_sum(&mp->m_frextents);

	 
	if (icount < 0 || ifree < 0)
		xchk_set_corrupt(sc);

	 
	if (fdblocks < 0 || frextents < 0) {
		if (!fsc->frozen)
			return -EDEADLOCK;

		xchk_set_corrupt(sc);
		return 0;
	}

	 
	if (icount < fsc->icount_min || icount > fsc->icount_max)
		xchk_set_corrupt(sc);

	 
	if (fdblocks > mp->m_sb.sb_dblocks)
		xchk_set_corrupt(sc);

	 
	if (frextents > mp->m_sb.sb_rextents)
		xchk_set_corrupt(sc);

	 
	if (ifree > icount && ifree - icount > XCHK_FSCOUNT_MIN_VARIANCE)
		xchk_set_corrupt(sc);

	 
	error = xchk_fscount_aggregate_agcounts(sc, fsc);
	if (!xchk_process_error(sc, 0, XFS_SB_BLOCK(mp), &error))
		return error;

	 
	error = xchk_fscount_count_frextents(sc, fsc);
	if (!xchk_process_error(sc, 0, XFS_SB_BLOCK(mp), &error))
		return error;
	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_INCOMPLETE)
		return 0;

	 
	if (!xchk_fscount_within_range(sc, icount, &mp->m_icount,
				fsc->icount)) {
		if (fsc->frozen)
			xchk_set_corrupt(sc);
		else
			try_again = true;
	}

	if (!xchk_fscount_within_range(sc, ifree, &mp->m_ifree, fsc->ifree)) {
		if (fsc->frozen)
			xchk_set_corrupt(sc);
		else
			try_again = true;
	}

	if (!xchk_fscount_within_range(sc, fdblocks, &mp->m_fdblocks,
			fsc->fdblocks)) {
		if (fsc->frozen)
			xchk_set_corrupt(sc);
		else
			try_again = true;
	}

	if (!xchk_fscount_within_range(sc, frextents, &mp->m_frextents,
			fsc->frextents)) {
		if (fsc->frozen)
			xchk_set_corrupt(sc);
		else
			try_again = true;
	}

	if (try_again)
		return -EDEADLOCK;

	return 0;
}

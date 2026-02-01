
 
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
#include "xfs_defer.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/trace.h"
#include "scrub/repair.h"
#include "scrub/bitmap.h"
#include "scrub/stats.h"

 
int
xrep_attempt(
	struct xfs_scrub	*sc,
	struct xchk_stats_run	*run)
{
	u64			repair_start;
	int			error = 0;

	trace_xrep_attempt(XFS_I(file_inode(sc->file)), sc->sm, error);

	xchk_ag_btcur_free(&sc->sa);

	 
	ASSERT(sc->ops->repair);
	run->repair_attempted = true;
	repair_start = xchk_stats_now();
	error = sc->ops->repair(sc);
	trace_xrep_done(XFS_I(file_inode(sc->file)), sc->sm, error);
	run->repair_ns += xchk_stats_elapsed_ns(repair_start);
	switch (error) {
	case 0:
		 
		sc->sm->sm_flags &= ~XFS_SCRUB_FLAGS_OUT;
		sc->flags |= XREP_ALREADY_FIXED;
		run->repair_succeeded = true;
		return -EAGAIN;
	case -ECHRNG:
		sc->flags |= XCHK_NEED_DRAIN;
		run->retries++;
		return -EAGAIN;
	case -EDEADLOCK:
		 
		if (!(sc->flags & XCHK_TRY_HARDER)) {
			sc->flags |= XCHK_TRY_HARDER;
			run->retries++;
			return -EAGAIN;
		}
		 
		return 0;
	default:
		 
		ASSERT(error != -EAGAIN);
		return error;
	}
}

 
void
xrep_failure(
	struct xfs_mount	*mp)
{
	xfs_alert_ratelimited(mp,
"Corruption not fixed during online repair.  Unmount and run xfs_repair.");
}

 
int
xrep_probe(
	struct xfs_scrub	*sc)
{
	int			error = 0;

	if (xchk_should_terminate(sc, &error))
		return error;

	return 0;
}

 
int
xrep_roll_ag_trans(
	struct xfs_scrub	*sc)
{
	int			error;

	 
	if (sc->sa.agi_bp) {
		xfs_ialloc_log_agi(sc->tp, sc->sa.agi_bp, XFS_AGI_MAGICNUM);
		xfs_trans_bhold(sc->tp, sc->sa.agi_bp);
	}

	if (sc->sa.agf_bp) {
		xfs_alloc_log_agf(sc->tp, sc->sa.agf_bp, XFS_AGF_MAGICNUM);
		xfs_trans_bhold(sc->tp, sc->sa.agf_bp);
	}

	 
	error = xfs_trans_roll(&sc->tp);
	if (error)
		return error;

	 
	if (sc->sa.agi_bp)
		xfs_trans_bjoin(sc->tp, sc->sa.agi_bp);
	if (sc->sa.agf_bp)
		xfs_trans_bjoin(sc->tp, sc->sa.agf_bp);

	return 0;
}

 
int
xrep_defer_finish(
	struct xfs_scrub	*sc)
{
	int			error;

	 
	if (sc->sa.agi_bp) {
		xfs_ialloc_log_agi(sc->tp, sc->sa.agi_bp, XFS_AGI_MAGICNUM);
		xfs_trans_bhold(sc->tp, sc->sa.agi_bp);
	}

	if (sc->sa.agf_bp) {
		xfs_alloc_log_agf(sc->tp, sc->sa.agf_bp, XFS_AGF_MAGICNUM);
		xfs_trans_bhold(sc->tp, sc->sa.agf_bp);
	}

	 
	error = xfs_defer_finish(&sc->tp);
	if (error)
		return error;

	 
	if (sc->sa.agi_bp)
		xfs_trans_bhold_release(sc->tp, sc->sa.agi_bp);
	if (sc->sa.agf_bp)
		xfs_trans_bhold_release(sc->tp, sc->sa.agf_bp);

	return 0;
}

 
bool
xrep_ag_has_space(
	struct xfs_perag	*pag,
	xfs_extlen_t		nr_blocks,
	enum xfs_ag_resv_type	type)
{
	return  !xfs_ag_resv_critical(pag, XFS_AG_RESV_RMAPBT) &&
		!xfs_ag_resv_critical(pag, XFS_AG_RESV_METADATA) &&
		pag->pagf_freeblks > xfs_ag_resv_needed(pag, type) + nr_blocks;
}

 
xfs_extlen_t
xrep_calc_ag_resblks(
	struct xfs_scrub		*sc)
{
	struct xfs_mount		*mp = sc->mp;
	struct xfs_scrub_metadata	*sm = sc->sm;
	struct xfs_perag		*pag;
	struct xfs_buf			*bp;
	xfs_agino_t			icount = NULLAGINO;
	xfs_extlen_t			aglen = NULLAGBLOCK;
	xfs_extlen_t			usedlen;
	xfs_extlen_t			freelen;
	xfs_extlen_t			bnobt_sz;
	xfs_extlen_t			inobt_sz;
	xfs_extlen_t			rmapbt_sz;
	xfs_extlen_t			refcbt_sz;
	int				error;

	if (!(sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR))
		return 0;

	pag = xfs_perag_get(mp, sm->sm_agno);
	if (xfs_perag_initialised_agi(pag)) {
		 
		icount = pag->pagi_count;
	} else {
		 
		error = xfs_ialloc_read_agi(pag, NULL, &bp);
		if (!error) {
			icount = pag->pagi_count;
			xfs_buf_relse(bp);
		}
	}

	 
	error = xfs_alloc_read_agf(pag, NULL, 0, &bp);
	if (error) {
		aglen = pag->block_count;
		freelen = aglen;
		usedlen = aglen;
	} else {
		struct xfs_agf	*agf = bp->b_addr;

		aglen = be32_to_cpu(agf->agf_length);
		freelen = be32_to_cpu(agf->agf_freeblks);
		usedlen = aglen - freelen;
		xfs_buf_relse(bp);
	}

	 
	if (icount == NULLAGINO ||
	    !xfs_verify_agino(pag, icount)) {
		icount = pag->agino_max - pag->agino_min + 1;
	}

	 
	if (aglen == NULLAGBLOCK ||
	    aglen != pag->block_count ||
	    freelen >= aglen) {
		aglen = pag->block_count;
		freelen = aglen;
		usedlen = aglen;
	}
	xfs_perag_put(pag);

	trace_xrep_calc_ag_resblks(mp, sm->sm_agno, icount, aglen,
			freelen, usedlen);

	 
	bnobt_sz = 2 * xfs_allocbt_calc_size(mp, freelen);
	if (xfs_has_sparseinodes(mp))
		inobt_sz = xfs_iallocbt_calc_size(mp, icount /
				XFS_INODES_PER_HOLEMASK_BIT);
	else
		inobt_sz = xfs_iallocbt_calc_size(mp, icount /
				XFS_INODES_PER_CHUNK);
	if (xfs_has_finobt(mp))
		inobt_sz *= 2;
	if (xfs_has_reflink(mp))
		refcbt_sz = xfs_refcountbt_calc_size(mp, usedlen);
	else
		refcbt_sz = 0;
	if (xfs_has_rmapbt(mp)) {
		 
		if (xfs_has_reflink(mp))
			rmapbt_sz = xfs_rmapbt_calc_size(mp,
					(unsigned long long)aglen * 2);
		else
			rmapbt_sz = xfs_rmapbt_calc_size(mp, usedlen);
	} else {
		rmapbt_sz = 0;
	}

	trace_xrep_calc_ag_resblks_btsize(mp, sm->sm_agno, bnobt_sz,
			inobt_sz, rmapbt_sz, refcbt_sz);

	return max(max(bnobt_sz, inobt_sz), max(rmapbt_sz, refcbt_sz));
}

 

 
int
xrep_fix_freelist(
	struct xfs_scrub	*sc,
	bool			can_shrink)
{
	struct xfs_alloc_arg	args = {0};

	args.mp = sc->mp;
	args.tp = sc->tp;
	args.agno = sc->sa.pag->pag_agno;
	args.alignment = 1;
	args.pag = sc->sa.pag;

	return xfs_alloc_fix_freelist(&args,
			can_shrink ? 0 : XFS_ALLOC_FLAG_NOSHRINK);
}

 

struct xrep_findroot {
	struct xfs_scrub		*sc;
	struct xfs_buf			*agfl_bp;
	struct xfs_agf			*agf;
	struct xrep_find_ag_btree	*btree_info;
};

 
STATIC int
xrep_findroot_agfl_walk(
	struct xfs_mount	*mp,
	xfs_agblock_t		bno,
	void			*priv)
{
	xfs_agblock_t		*agbno = priv;

	return (*agbno == bno) ? -ECANCELED : 0;
}

 
STATIC int
xrep_findroot_block(
	struct xrep_findroot		*ri,
	struct xrep_find_ag_btree	*fab,
	uint64_t			owner,
	xfs_agblock_t			agbno,
	bool				*done_with_block)
{
	struct xfs_mount		*mp = ri->sc->mp;
	struct xfs_buf			*bp;
	struct xfs_btree_block		*btblock;
	xfs_daddr_t			daddr;
	int				block_level;
	int				error = 0;

	daddr = XFS_AGB_TO_DADDR(mp, ri->sc->sa.pag->pag_agno, agbno);

	 
	if (owner == XFS_RMAP_OWN_AG) {
		error = xfs_agfl_walk(mp, ri->agf, ri->agfl_bp,
				xrep_findroot_agfl_walk, &agbno);
		if (error == -ECANCELED)
			return 0;
		if (error)
			return error;
	}

	 
	error = xfs_trans_read_buf(mp, ri->sc->tp, mp->m_ddev_targp, daddr,
			mp->m_bsize, 0, &bp, NULL);
	if (error)
		return error;

	 
	btblock = XFS_BUF_TO_BLOCK(bp);
	ASSERT(fab->buf_ops->magic[1] != 0);
	if (btblock->bb_magic != fab->buf_ops->magic[1])
		goto out;

	 
	if (bp->b_ops) {
		if (bp->b_ops != fab->buf_ops)
			goto out;
	} else {
		ASSERT(!xfs_trans_buf_is_dirty(bp));
		if (!uuid_equal(&btblock->bb_u.s.bb_uuid,
				&mp->m_sb.sb_meta_uuid))
			goto out;
		 
		bp->b_ops = fab->buf_ops;
		fab->buf_ops->verify_read(bp);
		if (bp->b_error) {
			bp->b_ops = NULL;
			bp->b_error = 0;
			goto out;
		}

		 
	}

	 
	*done_with_block = true;

	 
	block_level = xfs_btree_get_level(btblock);
	if (block_level + 1 == fab->height) {
		fab->root = NULLAGBLOCK;
		goto out;
	} else if (block_level < fab->height) {
		goto out;
	}

	 
	fab->height = block_level + 1;

	 
	if (btblock->bb_u.s.bb_leftsib == cpu_to_be32(NULLAGBLOCK) &&
	    btblock->bb_u.s.bb_rightsib == cpu_to_be32(NULLAGBLOCK))
		fab->root = agbno;
	else
		fab->root = NULLAGBLOCK;

	trace_xrep_findroot_block(mp, ri->sc->sa.pag->pag_agno, agbno,
			be32_to_cpu(btblock->bb_magic), fab->height - 1);
out:
	xfs_trans_brelse(ri->sc->tp, bp);
	return error;
}

 
STATIC int
xrep_findroot_rmap(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xrep_findroot		*ri = priv;
	struct xrep_find_ag_btree	*fab;
	xfs_agblock_t			b;
	bool				done;
	int				error = 0;

	 
	if (!XFS_RMAP_NON_INODE_OWNER(rec->rm_owner))
		return 0;

	 
	for (b = 0; b < rec->rm_blockcount; b++) {
		done = false;
		for (fab = ri->btree_info; fab->buf_ops; fab++) {
			if (rec->rm_owner != fab->rmap_owner)
				continue;
			error = xrep_findroot_block(ri, fab,
					rec->rm_owner, rec->rm_startblock + b,
					&done);
			if (error)
				return error;
			if (done)
				break;
		}
	}

	return 0;
}

 
int
xrep_find_ag_btree_roots(
	struct xfs_scrub		*sc,
	struct xfs_buf			*agf_bp,
	struct xrep_find_ag_btree	*btree_info,
	struct xfs_buf			*agfl_bp)
{
	struct xfs_mount		*mp = sc->mp;
	struct xrep_findroot		ri;
	struct xrep_find_ag_btree	*fab;
	struct xfs_btree_cur		*cur;
	int				error;

	ASSERT(xfs_buf_islocked(agf_bp));
	ASSERT(agfl_bp == NULL || xfs_buf_islocked(agfl_bp));

	ri.sc = sc;
	ri.btree_info = btree_info;
	ri.agf = agf_bp->b_addr;
	ri.agfl_bp = agfl_bp;
	for (fab = btree_info; fab->buf_ops; fab++) {
		ASSERT(agfl_bp || fab->rmap_owner != XFS_RMAP_OWN_AG);
		ASSERT(XFS_RMAP_NON_INODE_OWNER(fab->rmap_owner));
		fab->root = NULLAGBLOCK;
		fab->height = 0;
	}

	cur = xfs_rmapbt_init_cursor(mp, sc->tp, agf_bp, sc->sa.pag);
	error = xfs_rmap_query_all(cur, xrep_findroot_rmap, &ri);
	xfs_btree_del_cursor(cur, error);

	return error;
}

 
void
xrep_force_quotacheck(
	struct xfs_scrub	*sc,
	xfs_dqtype_t		type)
{
	uint			flag;

	flag = xfs_quota_chkd_flag(type);
	if (!(flag & sc->mp->m_qflags))
		return;

	mutex_lock(&sc->mp->m_quotainfo->qi_quotaofflock);
	sc->mp->m_qflags &= ~flag;
	spin_lock(&sc->mp->m_sb_lock);
	sc->mp->m_sb.sb_qflags &= ~flag;
	spin_unlock(&sc->mp->m_sb_lock);
	xfs_log_sb(sc->tp);
	mutex_unlock(&sc->mp->m_quotainfo->qi_quotaofflock);
}

 
int
xrep_ino_dqattach(
	struct xfs_scrub	*sc)
{
	int			error;

	error = xfs_qm_dqattach_locked(sc->ip, false);
	switch (error) {
	case -EFSBADCRC:
	case -EFSCORRUPTED:
	case -ENOENT:
		xfs_err_ratelimited(sc->mp,
"inode %llu repair encountered quota error %d, quotacheck forced.",
				(unsigned long long)sc->ip->i_ino, error);
		if (XFS_IS_UQUOTA_ON(sc->mp) && !sc->ip->i_udquot)
			xrep_force_quotacheck(sc, XFS_DQTYPE_USER);
		if (XFS_IS_GQUOTA_ON(sc->mp) && !sc->ip->i_gdquot)
			xrep_force_quotacheck(sc, XFS_DQTYPE_GROUP);
		if (XFS_IS_PQUOTA_ON(sc->mp) && !sc->ip->i_pdquot)
			xrep_force_quotacheck(sc, XFS_DQTYPE_PROJ);
		fallthrough;
	case -ESRCH:
		error = 0;
		break;
	default:
		break;
	}

	return error;
}

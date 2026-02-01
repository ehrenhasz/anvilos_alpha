
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_ialloc_btree.h"
#include "xfs_iwalk.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_icache.h"
#include "xfs_health.h"
#include "xfs_trans.h"
#include "xfs_pwork.h"
#include "xfs_ag.h"

 

struct xfs_iwalk_ag {
	 
	struct xfs_pwork		pwork;

	struct xfs_mount		*mp;
	struct xfs_trans		*tp;
	struct xfs_perag		*pag;

	 
	xfs_ino_t			startino;

	 
	xfs_ino_t			lastino;

	 
	struct xfs_inobt_rec_incore	*recs;

	 
	unsigned int			sz_recs;

	 
	unsigned int			nr_recs;

	 
	xfs_iwalk_fn			iwalk_fn;
	xfs_inobt_walk_fn		inobt_walk_fn;
	void				*data;

	 
	unsigned int			trim_start:1;

	 
	unsigned int			skip_empty:1;

	 
	unsigned int			drop_trans:1;
};

 
STATIC void
xfs_iwalk_ichunk_ra(
	struct xfs_mount		*mp,
	struct xfs_perag		*pag,
	struct xfs_inobt_rec_incore	*irec)
{
	struct xfs_ino_geometry		*igeo = M_IGEO(mp);
	xfs_agblock_t			agbno;
	struct blk_plug			plug;
	int				i;	 

	agbno = XFS_AGINO_TO_AGBNO(mp, irec->ir_startino);

	blk_start_plug(&plug);
	for (i = 0; i < XFS_INODES_PER_CHUNK; i += igeo->inodes_per_cluster) {
		xfs_inofree_t	imask;

		imask = xfs_inobt_maskn(i, igeo->inodes_per_cluster);
		if (imask & ~irec->ir_free) {
			xfs_btree_reada_bufs(mp, pag->pag_agno, agbno,
					igeo->blocks_per_cluster,
					&xfs_inode_buf_ops);
		}
		agbno += igeo->blocks_per_cluster;
	}
	blk_finish_plug(&plug);
}

 
STATIC void
xfs_iwalk_adjust_start(
	xfs_agino_t			agino,	 
	struct xfs_inobt_rec_incore	*irec)	 
{
	int				idx;	 
	int				i;

	idx = agino - irec->ir_startino;

	 
	for (i = 0; i < idx; i++) {
		if (XFS_INOBT_MASK(i) & ~irec->ir_free)
			irec->ir_freecount++;
	}

	irec->ir_free |= xfs_inobt_maskn(0, idx);
}

 
STATIC int
xfs_iwalk_alloc(
	struct xfs_iwalk_ag	*iwag)
{
	size_t			size;

	ASSERT(iwag->recs == NULL);
	iwag->nr_recs = 0;

	 
	size = iwag->sz_recs * sizeof(struct xfs_inobt_rec_incore);
	iwag->recs = kmem_alloc(size, KM_MAYFAIL);
	if (iwag->recs == NULL)
		return -ENOMEM;

	return 0;
}

 
STATIC void
xfs_iwalk_free(
	struct xfs_iwalk_ag	*iwag)
{
	kmem_free(iwag->recs);
	iwag->recs = NULL;
}

 
STATIC int
xfs_iwalk_ag_recs(
	struct xfs_iwalk_ag	*iwag)
{
	struct xfs_mount	*mp = iwag->mp;
	struct xfs_trans	*tp = iwag->tp;
	struct xfs_perag	*pag = iwag->pag;
	xfs_ino_t		ino;
	unsigned int		i, j;
	int			error;

	for (i = 0; i < iwag->nr_recs; i++) {
		struct xfs_inobt_rec_incore	*irec = &iwag->recs[i];

		trace_xfs_iwalk_ag_rec(mp, pag->pag_agno, irec);

		if (xfs_pwork_want_abort(&iwag->pwork))
			return 0;

		if (iwag->inobt_walk_fn) {
			error = iwag->inobt_walk_fn(mp, tp, pag->pag_agno, irec,
					iwag->data);
			if (error)
				return error;
		}

		if (!iwag->iwalk_fn)
			continue;

		for (j = 0; j < XFS_INODES_PER_CHUNK; j++) {
			if (xfs_pwork_want_abort(&iwag->pwork))
				return 0;

			 
			if (XFS_INOBT_MASK(j) & irec->ir_free)
				continue;

			 
			ino = XFS_AGINO_TO_INO(mp, pag->pag_agno,
						irec->ir_startino + j);
			error = iwag->iwalk_fn(mp, tp, ino, iwag->data);
			if (error)
				return error;
		}
	}

	return 0;
}

 
static inline void
xfs_iwalk_del_inobt(
	struct xfs_trans	*tp,
	struct xfs_btree_cur	**curpp,
	struct xfs_buf		**agi_bpp,
	int			error)
{
	if (*curpp) {
		xfs_btree_del_cursor(*curpp, error);
		*curpp = NULL;
	}
	if (*agi_bpp) {
		xfs_trans_brelse(tp, *agi_bpp);
		*agi_bpp = NULL;
	}
}

 
STATIC int
xfs_iwalk_ag_start(
	struct xfs_iwalk_ag	*iwag,
	xfs_agino_t		agino,
	struct xfs_btree_cur	**curpp,
	struct xfs_buf		**agi_bpp,
	int			*has_more)
{
	struct xfs_mount	*mp = iwag->mp;
	struct xfs_trans	*tp = iwag->tp;
	struct xfs_perag	*pag = iwag->pag;
	struct xfs_inobt_rec_incore *irec;
	int			error;

	 
	iwag->nr_recs = 0;
	error = xfs_inobt_cur(pag, tp, XFS_BTNUM_INO, curpp, agi_bpp);
	if (error)
		return error;

	 
	if (agino == 0)
		return xfs_inobt_lookup(*curpp, 0, XFS_LOOKUP_GE, has_more);

	 
	error = xfs_inobt_lookup(*curpp, agino, XFS_LOOKUP_LE, has_more);
	if (error)
		return error;

	 
	if (!*has_more)
		goto out_advance;

	 
	irec = &iwag->recs[iwag->nr_recs];
	error = xfs_inobt_get_rec(*curpp, irec, has_more);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(mp, *has_more != 1))
		return -EFSCORRUPTED;

	iwag->lastino = XFS_AGINO_TO_INO(mp, pag->pag_agno,
				irec->ir_startino + XFS_INODES_PER_CHUNK - 1);

	 
	if (irec->ir_startino + XFS_INODES_PER_CHUNK <= agino)
		goto out_advance;

	 
	if (iwag->trim_start)
		xfs_iwalk_adjust_start(agino, irec);

	 
	iwag->nr_recs++;
	ASSERT(iwag->nr_recs < iwag->sz_recs);

out_advance:
	return xfs_btree_increment(*curpp, 0, has_more);
}

 
STATIC int
xfs_iwalk_run_callbacks(
	struct xfs_iwalk_ag		*iwag,
	struct xfs_btree_cur		**curpp,
	struct xfs_buf			**agi_bpp,
	int				*has_more)
{
	struct xfs_mount		*mp = iwag->mp;
	struct xfs_inobt_rec_incore	*irec;
	xfs_agino_t			next_agino;
	int				error;

	next_agino = XFS_INO_TO_AGINO(mp, iwag->lastino) + 1;

	ASSERT(iwag->nr_recs > 0);

	 
	xfs_iwalk_del_inobt(iwag->tp, curpp, agi_bpp, 0);
	irec = &iwag->recs[iwag->nr_recs - 1];
	ASSERT(next_agino >= irec->ir_startino + XFS_INODES_PER_CHUNK);

	if (iwag->drop_trans) {
		xfs_trans_cancel(iwag->tp);
		iwag->tp = NULL;
	}

	error = xfs_iwalk_ag_recs(iwag);
	if (error)
		return error;

	 
	iwag->nr_recs = 0;

	if (!has_more)
		return 0;

	if (iwag->drop_trans) {
		error = xfs_trans_alloc_empty(mp, &iwag->tp);
		if (error)
			return error;
	}

	 
	error = xfs_inobt_cur(iwag->pag, iwag->tp, XFS_BTNUM_INO, curpp,
			agi_bpp);
	if (error)
		return error;

	return xfs_inobt_lookup(*curpp, next_agino, XFS_LOOKUP_GE, has_more);
}

 
STATIC int
xfs_iwalk_ag(
	struct xfs_iwalk_ag		*iwag)
{
	struct xfs_mount		*mp = iwag->mp;
	struct xfs_perag		*pag = iwag->pag;
	struct xfs_buf			*agi_bp = NULL;
	struct xfs_btree_cur		*cur = NULL;
	xfs_agino_t			agino;
	int				has_more;
	int				error = 0;

	 
	ASSERT(pag->pag_agno == XFS_INO_TO_AGNO(mp, iwag->startino));
	agino = XFS_INO_TO_AGINO(mp, iwag->startino);
	error = xfs_iwalk_ag_start(iwag, agino, &cur, &agi_bp, &has_more);

	while (!error && has_more) {
		struct xfs_inobt_rec_incore	*irec;
		xfs_ino_t			rec_fsino;

		cond_resched();
		if (xfs_pwork_want_abort(&iwag->pwork))
			goto out;

		 
		irec = &iwag->recs[iwag->nr_recs];
		error = xfs_inobt_get_rec(cur, irec, &has_more);
		if (error || !has_more)
			break;

		 
		rec_fsino = XFS_AGINO_TO_INO(mp, pag->pag_agno, irec->ir_startino);
		if (iwag->lastino != NULLFSINO &&
		    XFS_IS_CORRUPT(mp, iwag->lastino >= rec_fsino)) {
			error = -EFSCORRUPTED;
			goto out;
		}
		iwag->lastino = rec_fsino + XFS_INODES_PER_CHUNK - 1;

		 
		if (iwag->skip_empty && irec->ir_freecount == irec->ir_count) {
			error = xfs_btree_increment(cur, 0, &has_more);
			if (error)
				break;
			continue;
		}

		 
		if (iwag->iwalk_fn)
			xfs_iwalk_ichunk_ra(mp, pag, irec);

		 
		if (++iwag->nr_recs < iwag->sz_recs) {
			error = xfs_btree_increment(cur, 0, &has_more);
			if (error || !has_more)
				break;
			continue;
		}

		 
		ASSERT(has_more);
		error = xfs_iwalk_run_callbacks(iwag, &cur, &agi_bp, &has_more);
	}

	if (iwag->nr_recs == 0 || error)
		goto out;

	 
	error = xfs_iwalk_run_callbacks(iwag, &cur, &agi_bp, &has_more);

out:
	xfs_iwalk_del_inobt(iwag->tp, &cur, &agi_bp, error);
	return error;
}

 
#define IWALK_MAX_INODE_PREFETCH	(2048U)

 
static inline unsigned int
xfs_iwalk_prefetch(
	unsigned int		inodes)
{
	unsigned int		inobt_records;

	 
	if (inodes == 0)
		inodes = IWALK_MAX_INODE_PREFETCH;
	inodes = min(inodes, IWALK_MAX_INODE_PREFETCH);

	 
	inodes = round_up(inodes, XFS_INODES_PER_CHUNK);

	 
	inobt_records = (inodes * 5) / (4 * XFS_INODES_PER_CHUNK);

	 
	return max(inobt_records, 2U);
}

 
int
xfs_iwalk(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_ino_t		startino,
	unsigned int		flags,
	xfs_iwalk_fn		iwalk_fn,
	unsigned int		inode_records,
	void			*data)
{
	struct xfs_iwalk_ag	iwag = {
		.mp		= mp,
		.tp		= tp,
		.iwalk_fn	= iwalk_fn,
		.data		= data,
		.startino	= startino,
		.sz_recs	= xfs_iwalk_prefetch(inode_records),
		.trim_start	= 1,
		.skip_empty	= 1,
		.pwork		= XFS_PWORK_SINGLE_THREADED,
		.lastino	= NULLFSINO,
	};
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, startino);
	int			error;

	ASSERT(agno < mp->m_sb.sb_agcount);
	ASSERT(!(flags & ~XFS_IWALK_FLAGS_ALL));

	error = xfs_iwalk_alloc(&iwag);
	if (error)
		return error;

	for_each_perag_from(mp, agno, pag) {
		iwag.pag = pag;
		error = xfs_iwalk_ag(&iwag);
		if (error)
			break;
		iwag.startino = XFS_AGINO_TO_INO(mp, agno + 1, 0);
		if (flags & XFS_INOBT_WALK_SAME_AG)
			break;
		iwag.pag = NULL;
	}

	if (iwag.pag)
		xfs_perag_rele(pag);
	xfs_iwalk_free(&iwag);
	return error;
}

 
static int
xfs_iwalk_ag_work(
	struct xfs_mount	*mp,
	struct xfs_pwork	*pwork)
{
	struct xfs_iwalk_ag	*iwag;
	int			error = 0;

	iwag = container_of(pwork, struct xfs_iwalk_ag, pwork);
	if (xfs_pwork_want_abort(pwork))
		goto out;

	error = xfs_iwalk_alloc(iwag);
	if (error)
		goto out;
	 
	error = xfs_trans_alloc_empty(mp, &iwag->tp);
	if (error)
		goto out;
	iwag->drop_trans = 1;

	error = xfs_iwalk_ag(iwag);
	if (iwag->tp)
		xfs_trans_cancel(iwag->tp);
	xfs_iwalk_free(iwag);
out:
	xfs_perag_put(iwag->pag);
	kmem_free(iwag);
	return error;
}

 
int
xfs_iwalk_threaded(
	struct xfs_mount	*mp,
	xfs_ino_t		startino,
	unsigned int		flags,
	xfs_iwalk_fn		iwalk_fn,
	unsigned int		inode_records,
	bool			polled,
	void			*data)
{
	struct xfs_pwork_ctl	pctl;
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, startino);
	int			error;

	ASSERT(agno < mp->m_sb.sb_agcount);
	ASSERT(!(flags & ~XFS_IWALK_FLAGS_ALL));

	error = xfs_pwork_init(mp, &pctl, xfs_iwalk_ag_work, "xfs_iwalk");
	if (error)
		return error;

	for_each_perag_from(mp, agno, pag) {
		struct xfs_iwalk_ag	*iwag;

		if (xfs_pwork_ctl_want_abort(&pctl))
			break;

		iwag = kmem_zalloc(sizeof(struct xfs_iwalk_ag), 0);
		iwag->mp = mp;

		 
		iwag->pag = xfs_perag_hold(pag);
		iwag->iwalk_fn = iwalk_fn;
		iwag->data = data;
		iwag->startino = startino;
		iwag->sz_recs = xfs_iwalk_prefetch(inode_records);
		iwag->lastino = NULLFSINO;
		xfs_pwork_queue(&pctl, &iwag->pwork);
		startino = XFS_AGINO_TO_INO(mp, pag->pag_agno + 1, 0);
		if (flags & XFS_INOBT_WALK_SAME_AG)
			break;
	}
	if (pag)
		xfs_perag_rele(pag);
	if (polled)
		xfs_pwork_poll(&pctl);
	return xfs_pwork_destroy(&pctl);
}

 
#define MAX_INOBT_WALK_PREFETCH	\
	(PAGE_SIZE / sizeof(struct xfs_inobt_rec_incore))

 
static inline unsigned int
xfs_inobt_walk_prefetch(
	unsigned int		inobt_records)
{
	 
	if (inobt_records == 0)
		inobt_records = MAX_INOBT_WALK_PREFETCH;

	 
	inobt_records = max(inobt_records, 2U);

	 
	return min_t(unsigned int, inobt_records, MAX_INOBT_WALK_PREFETCH);
}

 
int
xfs_inobt_walk(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_ino_t		startino,
	unsigned int		flags,
	xfs_inobt_walk_fn	inobt_walk_fn,
	unsigned int		inobt_records,
	void			*data)
{
	struct xfs_iwalk_ag	iwag = {
		.mp		= mp,
		.tp		= tp,
		.inobt_walk_fn	= inobt_walk_fn,
		.data		= data,
		.startino	= startino,
		.sz_recs	= xfs_inobt_walk_prefetch(inobt_records),
		.pwork		= XFS_PWORK_SINGLE_THREADED,
		.lastino	= NULLFSINO,
	};
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, startino);
	int			error;

	ASSERT(agno < mp->m_sb.sb_agcount);
	ASSERT(!(flags & ~XFS_INOBT_WALK_FLAGS_ALL));

	error = xfs_iwalk_alloc(&iwag);
	if (error)
		return error;

	for_each_perag_from(mp, agno, pag) {
		iwag.pag = pag;
		error = xfs_iwalk_ag(&iwag);
		if (error)
			break;
		iwag.startino = XFS_AGINO_TO_INO(mp, pag->pag_agno + 1, 0);
		if (flags & XFS_INOBT_WALK_SAME_AG)
			break;
		iwag.pag = NULL;
	}

	if (iwag.pag)
		xfs_perag_rele(pag);
	xfs_iwalk_free(&iwag);
	return error;
}

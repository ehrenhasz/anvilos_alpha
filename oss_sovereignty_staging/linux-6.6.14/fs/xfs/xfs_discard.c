
 
#include "xfs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_alloc.h"
#include "xfs_discard.h"
#include "xfs_error.h"
#include "xfs_extent_busy.h"
#include "xfs_trace.h"
#include "xfs_log.h"
#include "xfs_ag.h"

 

struct workqueue_struct *xfs_discard_wq;

static void
xfs_discard_endio_work(
	struct work_struct	*work)
{
	struct xfs_busy_extents	*extents =
		container_of(work, struct xfs_busy_extents, endio_work);

	xfs_extent_busy_clear(extents->mount, &extents->extent_list, false);
	kmem_free(extents->owner);
}

 
static void
xfs_discard_endio(
	struct bio		*bio)
{
	struct xfs_busy_extents	*extents = bio->bi_private;

	INIT_WORK(&extents->endio_work, xfs_discard_endio_work);
	queue_work(xfs_discard_wq, &extents->endio_work);
	bio_put(bio);
}

 
int
xfs_discard_extents(
	struct xfs_mount	*mp,
	struct xfs_busy_extents	*extents)
{
	struct xfs_extent_busy	*busyp;
	struct bio		*bio = NULL;
	struct blk_plug		plug;
	int			error = 0;

	blk_start_plug(&plug);
	list_for_each_entry(busyp, &extents->extent_list, list) {
		trace_xfs_discard_extent(mp, busyp->agno, busyp->bno,
					 busyp->length);

		error = __blkdev_issue_discard(mp->m_ddev_targp->bt_bdev,
				XFS_AGB_TO_DADDR(mp, busyp->agno, busyp->bno),
				XFS_FSB_TO_BB(mp, busyp->length),
				GFP_NOFS, &bio);
		if (error && error != -EOPNOTSUPP) {
			xfs_info(mp,
	 "discard failed for extent [0x%llx,%u], error %d",
				 (unsigned long long)busyp->bno,
				 busyp->length,
				 error);
			break;
		}
	}

	if (bio) {
		bio->bi_private = extents;
		bio->bi_end_io = xfs_discard_endio;
		submit_bio(bio);
	} else {
		xfs_discard_endio_work(&extents->endio_work);
	}
	blk_finish_plug(&plug);

	return error;
}


static int
xfs_trim_gather_extents(
	struct xfs_perag	*pag,
	xfs_daddr_t		start,
	xfs_daddr_t		end,
	xfs_daddr_t		minlen,
	struct xfs_alloc_rec_incore *tcur,
	struct xfs_busy_extents	*extents,
	uint64_t		*blocks_trimmed)
{
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_btree_cur	*cur;
	struct xfs_buf		*agbp;
	int			error;
	int			i;
	int			batch = 100;

	 
	xfs_log_force(mp, XFS_LOG_SYNC);

	error = xfs_alloc_read_agf(pag, NULL, 0, &agbp);
	if (error)
		return error;

	cur = xfs_allocbt_init_cursor(mp, NULL, agbp, pag, XFS_BTNUM_CNT);

	 
	if (tcur->ar_startblock == NULLAGBLOCK)
		error = xfs_alloc_lookup_ge(cur, 0, tcur->ar_blockcount, &i);
	else
		error = xfs_alloc_lookup_le(cur, tcur->ar_startblock,
				tcur->ar_blockcount, &i);
	if (error)
		goto out_del_cursor;
	if (i == 0) {
		 
		tcur->ar_blockcount = 0;
		goto out_del_cursor;
	}

	 
	while (i) {
		xfs_agblock_t	fbno;
		xfs_extlen_t	flen;
		xfs_daddr_t	dbno;
		xfs_extlen_t	dlen;

		error = xfs_alloc_get_rec(cur, &fbno, &flen, &i);
		if (error)
			break;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			break;
		}

		if (--batch <= 0) {
			 
			tcur->ar_startblock = fbno;
			tcur->ar_blockcount = flen;
			break;
		}

		 
		dbno = XFS_AGB_TO_DADDR(mp, pag->pag_agno, fbno);
		dlen = XFS_FSB_TO_BB(mp, flen);

		 
		if (dlen < minlen) {
			trace_xfs_discard_toosmall(mp, pag->pag_agno, fbno, flen);
			tcur->ar_blockcount = 0;
			break;
		}

		 
		if (dbno + dlen < start || dbno > end) {
			trace_xfs_discard_exclude(mp, pag->pag_agno, fbno, flen);
			goto next_extent;
		}

		 
		if (xfs_extent_busy_search(mp, pag, fbno, flen)) {
			trace_xfs_discard_busy(mp, pag->pag_agno, fbno, flen);
			goto next_extent;
		}

		xfs_extent_busy_insert_discard(pag, fbno, flen,
				&extents->extent_list);
		*blocks_trimmed += flen;
next_extent:
		error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			break;

		 
		if (i == 0)
			tcur->ar_blockcount = 0;
	}

	 
	if (error)
		xfs_extent_busy_clear(mp, &extents->extent_list, false);
out_del_cursor:
	xfs_btree_del_cursor(cur, error);
	xfs_buf_relse(agbp);
	return error;
}

static bool
xfs_trim_should_stop(void)
{
	return fatal_signal_pending(current) || freezing(current);
}

 
static int
xfs_trim_extents(
	struct xfs_perag	*pag,
	xfs_daddr_t		start,
	xfs_daddr_t		end,
	xfs_daddr_t		minlen,
	uint64_t		*blocks_trimmed)
{
	struct xfs_alloc_rec_incore tcur = {
		.ar_blockcount = pag->pagf_longest,
		.ar_startblock = NULLAGBLOCK,
	};
	int			error = 0;

	do {
		struct xfs_busy_extents	*extents;

		extents = kzalloc(sizeof(*extents), GFP_KERNEL);
		if (!extents) {
			error = -ENOMEM;
			break;
		}

		extents->mount = pag->pag_mount;
		extents->owner = extents;
		INIT_LIST_HEAD(&extents->extent_list);

		error = xfs_trim_gather_extents(pag, start, end, minlen,
				&tcur, extents, blocks_trimmed);
		if (error) {
			kfree(extents);
			break;
		}

		 
		error = xfs_discard_extents(pag->pag_mount, extents);
		if (error)
			break;

		if (xfs_trim_should_stop())
			break;

	} while (tcur.ar_blockcount != 0);

	return error;

}

 
int
xfs_ioc_trim(
	struct xfs_mount		*mp,
	struct fstrim_range __user	*urange)
{
	struct xfs_perag	*pag;
	unsigned int		granularity =
		bdev_discard_granularity(mp->m_ddev_targp->bt_bdev);
	struct fstrim_range	range;
	xfs_daddr_t		start, end, minlen;
	xfs_agnumber_t		agno;
	uint64_t		blocks_trimmed = 0;
	int			error, last_error = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (!bdev_max_discard_sectors(mp->m_ddev_targp->bt_bdev))
		return -EOPNOTSUPP;

	 
	if (xfs_has_norecovery(mp))
		return -EROFS;

	if (copy_from_user(&range, urange, sizeof(range)))
		return -EFAULT;

	range.minlen = max_t(u64, granularity, range.minlen);
	minlen = BTOBB(range.minlen);
	 
	if (range.start >= XFS_FSB_TO_B(mp, mp->m_sb.sb_dblocks) ||
	    range.minlen > XFS_FSB_TO_B(mp, mp->m_ag_max_usable) ||
	    range.len < mp->m_sb.sb_blocksize)
		return -EINVAL;

	start = BTOBB(range.start);
	end = start + BTOBBT(range.len) - 1;

	if (end > XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks) - 1)
		end = XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks) - 1;

	agno = xfs_daddr_to_agno(mp, start);
	for_each_perag_range(mp, agno, xfs_daddr_to_agno(mp, end), pag) {
		error = xfs_trim_extents(pag, start, end, minlen,
					  &blocks_trimmed);
		if (error)
			last_error = error;

		if (xfs_trim_should_stop()) {
			xfs_perag_rele(pag);
			break;
		}
	}

	if (last_error)
		return last_error;

	range.len = XFS_FSB_TO_B(mp, blocks_trimmed);
	if (copy_to_user(urange, &range, sizeof(range)))
		return -EFAULT;
	return 0;
}

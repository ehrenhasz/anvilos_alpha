
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_inode.h"
#include "xfs_icache.h"
#include "xfs_alloc.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc.h"
#include "xfs_ialloc_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_rmap.h"
#include "xfs_rmap_btree.h"
#include "xfs_log.h"
#include "xfs_trans_priv.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "xfs_reflink.h"
#include "xfs_ag.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/trace.h"
#include "scrub/repair.h"
#include "scrub/health.h"

 

 

 
static bool
__xchk_process_error(
	struct xfs_scrub	*sc,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	int			*error,
	__u32			errflag,
	void			*ret_ip)
{
	switch (*error) {
	case 0:
		return true;
	case -EDEADLOCK:
	case -ECHRNG:
		 
		trace_xchk_deadlock_retry(
				sc->ip ? sc->ip : XFS_I(file_inode(sc->file)),
				sc->sm, *error);
		break;
	case -EFSBADCRC:
	case -EFSCORRUPTED:
		 
		sc->sm->sm_flags |= errflag;
		*error = 0;
		fallthrough;
	default:
		trace_xchk_op_error(sc, agno, bno, *error,
				ret_ip);
		break;
	}
	return false;
}

bool
xchk_process_error(
	struct xfs_scrub	*sc,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	int			*error)
{
	return __xchk_process_error(sc, agno, bno, error,
			XFS_SCRUB_OFLAG_CORRUPT, __return_address);
}

bool
xchk_xref_process_error(
	struct xfs_scrub	*sc,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	int			*error)
{
	return __xchk_process_error(sc, agno, bno, error,
			XFS_SCRUB_OFLAG_XFAIL, __return_address);
}

 
static bool
__xchk_fblock_process_error(
	struct xfs_scrub	*sc,
	int			whichfork,
	xfs_fileoff_t		offset,
	int			*error,
	__u32			errflag,
	void			*ret_ip)
{
	switch (*error) {
	case 0:
		return true;
	case -EDEADLOCK:
	case -ECHRNG:
		 
		trace_xchk_deadlock_retry(sc->ip, sc->sm, *error);
		break;
	case -EFSBADCRC:
	case -EFSCORRUPTED:
		 
		sc->sm->sm_flags |= errflag;
		*error = 0;
		fallthrough;
	default:
		trace_xchk_file_op_error(sc, whichfork, offset, *error,
				ret_ip);
		break;
	}
	return false;
}

bool
xchk_fblock_process_error(
	struct xfs_scrub	*sc,
	int			whichfork,
	xfs_fileoff_t		offset,
	int			*error)
{
	return __xchk_fblock_process_error(sc, whichfork, offset, error,
			XFS_SCRUB_OFLAG_CORRUPT, __return_address);
}

bool
xchk_fblock_xref_process_error(
	struct xfs_scrub	*sc,
	int			whichfork,
	xfs_fileoff_t		offset,
	int			*error)
{
	return __xchk_fblock_process_error(sc, whichfork, offset, error,
			XFS_SCRUB_OFLAG_XFAIL, __return_address);
}

 

 
void
xchk_block_set_preen(
	struct xfs_scrub	*sc,
	struct xfs_buf		*bp)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_PREEN;
	trace_xchk_block_preen(sc, xfs_buf_daddr(bp), __return_address);
}

 
void
xchk_ino_set_preen(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_PREEN;
	trace_xchk_ino_preen(sc, ino, __return_address);
}

 
void
xchk_set_corrupt(
	struct xfs_scrub	*sc)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_CORRUPT;
	trace_xchk_fs_error(sc, 0, __return_address);
}

 
void
xchk_block_set_corrupt(
	struct xfs_scrub	*sc,
	struct xfs_buf		*bp)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_CORRUPT;
	trace_xchk_block_error(sc, xfs_buf_daddr(bp), __return_address);
}

 
void
xchk_block_xref_set_corrupt(
	struct xfs_scrub	*sc,
	struct xfs_buf		*bp)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_XCORRUPT;
	trace_xchk_block_error(sc, xfs_buf_daddr(bp), __return_address);
}

 
void
xchk_ino_set_corrupt(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_CORRUPT;
	trace_xchk_ino_error(sc, ino, __return_address);
}

 
void
xchk_ino_xref_set_corrupt(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_XCORRUPT;
	trace_xchk_ino_error(sc, ino, __return_address);
}

 
void
xchk_fblock_set_corrupt(
	struct xfs_scrub	*sc,
	int			whichfork,
	xfs_fileoff_t		offset)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_CORRUPT;
	trace_xchk_fblock_error(sc, whichfork, offset, __return_address);
}

 
void
xchk_fblock_xref_set_corrupt(
	struct xfs_scrub	*sc,
	int			whichfork,
	xfs_fileoff_t		offset)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_XCORRUPT;
	trace_xchk_fblock_error(sc, whichfork, offset, __return_address);
}

 
void
xchk_ino_set_warning(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_WARNING;
	trace_xchk_ino_warning(sc, ino, __return_address);
}

 
void
xchk_fblock_set_warning(
	struct xfs_scrub	*sc,
	int			whichfork,
	xfs_fileoff_t		offset)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_WARNING;
	trace_xchk_fblock_warning(sc, whichfork, offset, __return_address);
}

 
void
xchk_set_incomplete(
	struct xfs_scrub	*sc)
{
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_INCOMPLETE;
	trace_xchk_incomplete(sc, __return_address);
}

 

struct xchk_rmap_ownedby_info {
	const struct xfs_owner_info	*oinfo;
	xfs_filblks_t			*blocks;
};

STATIC int
xchk_count_rmap_ownedby_irec(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xchk_rmap_ownedby_info	*sroi = priv;
	bool				irec_attr;
	bool				oinfo_attr;

	irec_attr = rec->rm_flags & XFS_RMAP_ATTR_FORK;
	oinfo_attr = sroi->oinfo->oi_flags & XFS_OWNER_INFO_ATTR_FORK;

	if (rec->rm_owner != sroi->oinfo->oi_owner)
		return 0;

	if (XFS_RMAP_NON_INODE_OWNER(rec->rm_owner) || irec_attr == oinfo_attr)
		(*sroi->blocks) += rec->rm_blockcount;

	return 0;
}

 
int
xchk_count_rmap_ownedby_ag(
	struct xfs_scrub		*sc,
	struct xfs_btree_cur		*cur,
	const struct xfs_owner_info	*oinfo,
	xfs_filblks_t			*blocks)
{
	struct xchk_rmap_ownedby_info	sroi = {
		.oinfo			= oinfo,
		.blocks			= blocks,
	};

	*blocks = 0;
	return xfs_rmap_query_all(cur, xchk_count_rmap_ownedby_irec,
			&sroi);
}

 

 
static inline bool
want_ag_read_header_failure(
	struct xfs_scrub	*sc,
	unsigned int		type)
{
	 
	if (sc->sm->sm_type != XFS_SCRUB_TYPE_AGF &&
	    sc->sm->sm_type != XFS_SCRUB_TYPE_AGFL &&
	    sc->sm->sm_type != XFS_SCRUB_TYPE_AGI)
		return true;
	 
	if (sc->sm->sm_type == type)
		return true;
	return false;
}

 
static inline int
xchk_perag_read_headers(
	struct xfs_scrub	*sc,
	struct xchk_ag		*sa)
{
	int			error;

	error = xfs_ialloc_read_agi(sa->pag, sc->tp, &sa->agi_bp);
	if (error && want_ag_read_header_failure(sc, XFS_SCRUB_TYPE_AGI))
		return error;

	error = xfs_alloc_read_agf(sa->pag, sc->tp, 0, &sa->agf_bp);
	if (error && want_ag_read_header_failure(sc, XFS_SCRUB_TYPE_AGF))
		return error;

	return 0;
}

 
static int
xchk_perag_drain_and_lock(
	struct xfs_scrub	*sc)
{
	struct xchk_ag		*sa = &sc->sa;
	int			error = 0;

	ASSERT(sa->pag != NULL);
	ASSERT(sa->agi_bp == NULL);
	ASSERT(sa->agf_bp == NULL);

	do {
		if (xchk_should_terminate(sc, &error))
			return error;

		error = xchk_perag_read_headers(sc, sa);
		if (error)
			return error;

		 
		if (sc->ip)
			return 0;

		 
		if (!xfs_perag_intent_busy(sa->pag))
			return 0;

		if (sa->agf_bp) {
			xfs_trans_brelse(sc->tp, sa->agf_bp);
			sa->agf_bp = NULL;
		}

		if (sa->agi_bp) {
			xfs_trans_brelse(sc->tp, sa->agi_bp);
			sa->agi_bp = NULL;
		}

		if (!(sc->flags & XCHK_FSGATES_DRAIN))
			return -ECHRNG;
		error = xfs_perag_intent_drain(sa->pag);
		if (error == -ERESTARTSYS)
			error = -EINTR;
	} while (!error);

	return error;
}

 
int
xchk_ag_read_headers(
	struct xfs_scrub	*sc,
	xfs_agnumber_t		agno,
	struct xchk_ag		*sa)
{
	struct xfs_mount	*mp = sc->mp;

	ASSERT(!sa->pag);
	sa->pag = xfs_perag_get(mp, agno);
	if (!sa->pag)
		return -ENOENT;

	return xchk_perag_drain_and_lock(sc);
}

 
void
xchk_ag_btcur_free(
	struct xchk_ag		*sa)
{
	if (sa->refc_cur)
		xfs_btree_del_cursor(sa->refc_cur, XFS_BTREE_ERROR);
	if (sa->rmap_cur)
		xfs_btree_del_cursor(sa->rmap_cur, XFS_BTREE_ERROR);
	if (sa->fino_cur)
		xfs_btree_del_cursor(sa->fino_cur, XFS_BTREE_ERROR);
	if (sa->ino_cur)
		xfs_btree_del_cursor(sa->ino_cur, XFS_BTREE_ERROR);
	if (sa->cnt_cur)
		xfs_btree_del_cursor(sa->cnt_cur, XFS_BTREE_ERROR);
	if (sa->bno_cur)
		xfs_btree_del_cursor(sa->bno_cur, XFS_BTREE_ERROR);

	sa->refc_cur = NULL;
	sa->rmap_cur = NULL;
	sa->fino_cur = NULL;
	sa->ino_cur = NULL;
	sa->bno_cur = NULL;
	sa->cnt_cur = NULL;
}

 
void
xchk_ag_btcur_init(
	struct xfs_scrub	*sc,
	struct xchk_ag		*sa)
{
	struct xfs_mount	*mp = sc->mp;

	if (sa->agf_bp &&
	    xchk_ag_btree_healthy_enough(sc, sa->pag, XFS_BTNUM_BNO)) {
		 
		sa->bno_cur = xfs_allocbt_init_cursor(mp, sc->tp, sa->agf_bp,
				sa->pag, XFS_BTNUM_BNO);
	}

	if (sa->agf_bp &&
	    xchk_ag_btree_healthy_enough(sc, sa->pag, XFS_BTNUM_CNT)) {
		 
		sa->cnt_cur = xfs_allocbt_init_cursor(mp, sc->tp, sa->agf_bp,
				sa->pag, XFS_BTNUM_CNT);
	}

	 
	if (sa->agi_bp &&
	    xchk_ag_btree_healthy_enough(sc, sa->pag, XFS_BTNUM_INO)) {
		sa->ino_cur = xfs_inobt_init_cursor(sa->pag, sc->tp, sa->agi_bp,
				XFS_BTNUM_INO);
	}

	 
	if (sa->agi_bp && xfs_has_finobt(mp) &&
	    xchk_ag_btree_healthy_enough(sc, sa->pag, XFS_BTNUM_FINO)) {
		sa->fino_cur = xfs_inobt_init_cursor(sa->pag, sc->tp, sa->agi_bp,
				XFS_BTNUM_FINO);
	}

	 
	if (sa->agf_bp && xfs_has_rmapbt(mp) &&
	    xchk_ag_btree_healthy_enough(sc, sa->pag, XFS_BTNUM_RMAP)) {
		sa->rmap_cur = xfs_rmapbt_init_cursor(mp, sc->tp, sa->agf_bp,
				sa->pag);
	}

	 
	if (sa->agf_bp && xfs_has_reflink(mp) &&
	    xchk_ag_btree_healthy_enough(sc, sa->pag, XFS_BTNUM_REFC)) {
		sa->refc_cur = xfs_refcountbt_init_cursor(mp, sc->tp,
				sa->agf_bp, sa->pag);
	}
}

 
void
xchk_ag_free(
	struct xfs_scrub	*sc,
	struct xchk_ag		*sa)
{
	xchk_ag_btcur_free(sa);
	if (sa->agf_bp) {
		xfs_trans_brelse(sc->tp, sa->agf_bp);
		sa->agf_bp = NULL;
	}
	if (sa->agi_bp) {
		xfs_trans_brelse(sc->tp, sa->agi_bp);
		sa->agi_bp = NULL;
	}
	if (sa->pag) {
		xfs_perag_put(sa->pag);
		sa->pag = NULL;
	}
}

 
int
xchk_ag_init(
	struct xfs_scrub	*sc,
	xfs_agnumber_t		agno,
	struct xchk_ag		*sa)
{
	int			error;

	error = xchk_ag_read_headers(sc, agno, sa);
	if (error)
		return error;

	xchk_ag_btcur_init(sc, sa);
	return 0;
}

 

void
xchk_trans_cancel(
	struct xfs_scrub	*sc)
{
	xfs_trans_cancel(sc->tp);
	sc->tp = NULL;
}

 
int
xchk_trans_alloc(
	struct xfs_scrub	*sc,
	uint			resblks)
{
	if (sc->sm->sm_flags & XFS_SCRUB_IFLAG_REPAIR)
		return xfs_trans_alloc(sc->mp, &M_RES(sc->mp)->tr_itruncate,
				resblks, 0, 0, &sc->tp);

	return xfs_trans_alloc_empty(sc->mp, &sc->tp);
}

 
int
xchk_setup_fs(
	struct xfs_scrub	*sc)
{
	uint			resblks;

	resblks = xrep_calc_ag_resblks(sc);
	return xchk_trans_alloc(sc, resblks);
}

 
int
xchk_setup_ag_btree(
	struct xfs_scrub	*sc,
	bool			force_log)
{
	struct xfs_mount	*mp = sc->mp;
	int			error;

	 
	if (force_log) {
		error = xchk_checkpoint_log(mp);
		if (error)
			return error;
	}

	error = xchk_setup_fs(sc);
	if (error)
		return error;

	return xchk_ag_init(sc, sc->sm->sm_agno, &sc->sa);
}

 
int
xchk_checkpoint_log(
	struct xfs_mount	*mp)
{
	int			error;

	error = xfs_log_force(mp, XFS_LOG_SYNC);
	if (error)
		return error;
	xfs_ail_push_all_sync(mp->m_ail);
	return 0;
}

 
int
xchk_iget(
	struct xfs_scrub	*sc,
	xfs_ino_t		inum,
	struct xfs_inode	**ipp)
{
	return xfs_iget(sc->mp, sc->tp, inum, XFS_IGET_UNTRUSTED, 0, ipp);
}

 
int
xchk_iget_agi(
	struct xfs_scrub	*sc,
	xfs_ino_t		inum,
	struct xfs_buf		**agi_bpp,
	struct xfs_inode	**ipp)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_trans	*tp = sc->tp;
	struct xfs_perag	*pag;
	int			error;

	ASSERT(sc->tp != NULL);

again:
	*agi_bpp = NULL;
	*ipp = NULL;
	error = 0;

	if (xchk_should_terminate(sc, &error))
		return error;

	 
	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, inum));
	error = xfs_ialloc_read_agi(pag, tp, agi_bpp);
	xfs_perag_put(pag);
	if (error)
		return error;

	error = xfs_iget(mp, tp, inum,
			XFS_IGET_NORETRY | XFS_IGET_UNTRUSTED, 0, ipp);
	if (error == -EAGAIN) {
		 
		xfs_trans_brelse(tp, *agi_bpp);
		delay(1);
		goto again;
	}
	if (error)
		return error;

	 
	ASSERT(*ipp != NULL);
	xfs_trans_brelse(tp, *agi_bpp);
	*agi_bpp = NULL;
	return 0;
}

 
int
xchk_install_handle_inode(
	struct xfs_scrub	*sc,
	struct xfs_inode	*ip)
{
	if (VFS_I(ip)->i_generation != sc->sm->sm_gen) {
		xchk_irele(sc, ip);
		return -ENOENT;
	}

	sc->ip = ip;
	return 0;
}

 
int
xchk_install_live_inode(
	struct xfs_scrub	*sc,
	struct xfs_inode	*ip)
{
	if (!igrab(VFS_I(ip))) {
		xchk_ino_set_corrupt(sc, ip->i_ino);
		return -EFSCORRUPTED;
	}

	sc->ip = ip;
	return 0;
}

 
int
xchk_iget_for_scrubbing(
	struct xfs_scrub	*sc)
{
	struct xfs_imap		imap;
	struct xfs_mount	*mp = sc->mp;
	struct xfs_perag	*pag;
	struct xfs_buf		*agi_bp;
	struct xfs_inode	*ip_in = XFS_I(file_inode(sc->file));
	struct xfs_inode	*ip = NULL;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, sc->sm->sm_ino);
	int			error;

	ASSERT(sc->tp == NULL);

	 
	if (sc->sm->sm_ino == 0 || sc->sm->sm_ino == ip_in->i_ino)
		return xchk_install_live_inode(sc, ip_in);

	 
	if (xfs_internal_inum(mp, sc->sm->sm_ino))
		return -ENOENT;
	if (!xfs_verify_ino(sc->mp, sc->sm->sm_ino))
		return -ENOENT;

	 
	error = xchk_iget(sc, sc->sm->sm_ino, &ip);
	if (!error)
		return xchk_install_handle_inode(sc, ip);
	if (error == -ENOENT)
		return error;
	if (error != -EINVAL)
		goto out_error;

	 
	error = xchk_trans_alloc(sc, 0);
	if (error)
		goto out_error;

	error = xchk_iget_agi(sc, sc->sm->sm_ino, &agi_bp, &ip);
	if (error == 0) {
		 
		xchk_trans_cancel(sc);
		return xchk_install_handle_inode(sc, ip);
	}
	if (error == -ENOENT)
		goto out_gone;
	if (error != -EINVAL)
		goto out_cancel;

	 
	if (agi_bp == NULL) {
		ASSERT(agi_bp != NULL);
		error = -ECANCELED;
		goto out_cancel;
	}

	 
	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, sc->sm->sm_ino));
	if (!pag) {
		error = -EFSCORRUPTED;
		goto out_cancel;
	}

	error = xfs_imap(pag, sc->tp, sc->sm->sm_ino, &imap,
			XFS_IGET_UNTRUSTED);
	xfs_perag_put(pag);
	if (error == -EINVAL || error == -ENOENT)
		goto out_gone;
	if (!error)
		error = -EFSCORRUPTED;

out_cancel:
	xchk_trans_cancel(sc);
out_error:
	trace_xchk_op_error(sc, agno, XFS_INO_TO_AGBNO(mp, sc->sm->sm_ino),
			error, __return_address);
	return error;
out_gone:
	 
	xchk_trans_cancel(sc);
	return -ENOENT;
}

 
void
xchk_irele(
	struct xfs_scrub	*sc,
	struct xfs_inode	*ip)
{
	if (current->journal_info != NULL) {
		ASSERT(current->journal_info == sc->tp);

		 
		spin_lock(&VFS_I(ip)->i_lock);
		VFS_I(ip)->i_state &= ~I_DONTCACHE;
		spin_unlock(&VFS_I(ip)->i_lock);
	} else if (atomic_read(&VFS_I(ip)->i_count) == 1) {
		 
		d_mark_dontcache(VFS_I(ip));
	}

	xfs_irele(ip);
}

 
int
xchk_setup_inode_contents(
	struct xfs_scrub	*sc,
	unsigned int		resblks)
{
	int			error;

	error = xchk_iget_for_scrubbing(sc);
	if (error)
		return error;

	 
	xchk_ilock(sc, XFS_IOLOCK_EXCL);

	error = xchk_trans_alloc(sc, resblks);
	if (error)
		goto out;
	xchk_ilock(sc, XFS_ILOCK_EXCL);
out:
	 
	return error;
}

void
xchk_ilock(
	struct xfs_scrub	*sc,
	unsigned int		ilock_flags)
{
	xfs_ilock(sc->ip, ilock_flags);
	sc->ilock_flags |= ilock_flags;
}

bool
xchk_ilock_nowait(
	struct xfs_scrub	*sc,
	unsigned int		ilock_flags)
{
	if (xfs_ilock_nowait(sc->ip, ilock_flags)) {
		sc->ilock_flags |= ilock_flags;
		return true;
	}

	return false;
}

void
xchk_iunlock(
	struct xfs_scrub	*sc,
	unsigned int		ilock_flags)
{
	sc->ilock_flags &= ~ilock_flags;
	xfs_iunlock(sc->ip, ilock_flags);
}

 
bool
xchk_should_check_xref(
	struct xfs_scrub	*sc,
	int			*error,
	struct xfs_btree_cur	**curpp)
{
	 
	if (xchk_skip_xref(sc->sm))
		return false;

	if (*error == 0)
		return true;

	if (curpp) {
		 
		if (!*curpp)
			return false;

		 
		xfs_btree_del_cursor(*curpp, XFS_BTREE_ERROR);
		*curpp = NULL;
	}

	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_XFAIL;
	trace_xchk_xref_error(sc, *error, __return_address);

	 
	*error = 0;
	return false;
}

 
void
xchk_buffer_recheck(
	struct xfs_scrub	*sc,
	struct xfs_buf		*bp)
{
	xfs_failaddr_t		fa;

	if (bp->b_ops == NULL) {
		xchk_block_set_corrupt(sc, bp);
		return;
	}
	if (bp->b_ops->verify_struct == NULL) {
		xchk_set_incomplete(sc);
		return;
	}
	fa = bp->b_ops->verify_struct(bp);
	if (!fa)
		return;
	sc->sm->sm_flags |= XFS_SCRUB_OFLAG_CORRUPT;
	trace_xchk_block_error(sc, xfs_buf_daddr(bp), fa);
}

static inline int
xchk_metadata_inode_subtype(
	struct xfs_scrub	*sc,
	unsigned int		scrub_type)
{
	__u32			smtype = sc->sm->sm_type;
	int			error;

	sc->sm->sm_type = scrub_type;

	switch (scrub_type) {
	case XFS_SCRUB_TYPE_INODE:
		error = xchk_inode(sc);
		break;
	case XFS_SCRUB_TYPE_BMBTD:
		error = xchk_bmap_data(sc);
		break;
	default:
		ASSERT(0);
		error = -EFSCORRUPTED;
		break;
	}

	sc->sm->sm_type = smtype;
	return error;
}

 
int
xchk_metadata_inode_forks(
	struct xfs_scrub	*sc)
{
	bool			shared;
	int			error;

	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return 0;

	 
	error = xchk_metadata_inode_subtype(sc, XFS_SCRUB_TYPE_INODE);
	if (error || (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT))
		return error;

	 
	if (sc->ip->i_diflags & XFS_DIFLAG_REALTIME) {
		xchk_ino_set_corrupt(sc, sc->ip->i_ino);
		return 0;
	}

	 
	if (xfs_is_reflink_inode(sc->ip)) {
		xchk_ino_set_corrupt(sc, sc->ip->i_ino);
		return 0;
	}

	 
	if (xfs_inode_hasattr(sc->ip)) {
		xchk_ino_set_corrupt(sc, sc->ip->i_ino);
		return 0;
	}

	 
	error = xchk_metadata_inode_subtype(sc, XFS_SCRUB_TYPE_BMBTD);
	if (error || (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT))
		return error;

	 
	if (xfs_has_reflink(sc->mp)) {
		error = xfs_reflink_inode_has_shared_extents(sc->tp, sc->ip,
				&shared);
		if (!xchk_fblock_process_error(sc, XFS_DATA_FORK, 0,
				&error))
			return error;
		if (shared)
			xchk_ino_set_corrupt(sc, sc->ip->i_ino);
	}

	return 0;
}

 
void
xchk_fsgates_enable(
	struct xfs_scrub	*sc,
	unsigned int		scrub_fsgates)
{
	ASSERT(!(scrub_fsgates & ~XCHK_FSGATES_ALL));
	ASSERT(!(sc->flags & scrub_fsgates));

	trace_xchk_fsgates_enable(sc, scrub_fsgates);

	if (scrub_fsgates & XCHK_FSGATES_DRAIN)
		xfs_drain_wait_enable();

	sc->flags |= scrub_fsgates;
}

 
int
xchk_inode_is_allocated(
	struct xfs_scrub	*sc,
	xfs_agino_t		agino,
	bool			*inuse)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_perag	*pag = sc->sa.pag;
	xfs_ino_t		ino;
	struct xfs_inode	*ip;
	int			error;

	 
	if (pag == NULL) {
		ASSERT(pag != NULL);
		return -EINVAL;
	}

	 
	if (sc->sa.agi_bp == NULL) {
		ASSERT(sc->sa.agi_bp != NULL);
		return -EINVAL;
	}

	 
	ino = XFS_AGINO_TO_INO(sc->mp, pag->pag_agno, agino);
	if (!xfs_verify_ino(mp, ino))
		return -EINVAL;

	error = -ENODATA;
	rcu_read_lock();
	ip = radix_tree_lookup(&pag->pag_ici_root, agino);
	if (!ip) {
		 
		goto out_rcu;
	}

	 
	spin_lock(&ip->i_flags_lock);
	if (ip->i_ino != ino)
		goto out_skip;

	trace_xchk_inode_is_allocated(ip);

	 

#ifdef DEBUG
	 
	if (!(ip->i_flags & (XFS_NEED_INACTIVE | XFS_INEW | XFS_IRECLAIMABLE |
			     XFS_INACTIVATING))) {
		 
		ASSERT(VFS_I(ip)->i_mode != 0);
	}

	 
	if (ip->i_flags & XFS_INEW) {
		 
		ASSERT(VFS_I(ip)->i_mode != 0);
	}

	 
	if ((ip->i_flags & XFS_NEED_INACTIVE) &&
	    !(ip->i_flags & XFS_INACTIVATING)) {
		 
		ASSERT(VFS_I(ip)->i_mode != 0);
	}
#endif

	 

	 

	 

	 

	*inuse = VFS_I(ip)->i_mode != 0;
	error = 0;

out_skip:
	spin_unlock(&ip->i_flags_lock);
out_rcu:
	rcu_read_unlock();
	return error;
}

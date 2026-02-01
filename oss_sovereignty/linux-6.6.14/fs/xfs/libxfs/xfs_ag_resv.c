
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_alloc.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_rmap_btree.h"
#include "xfs_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_ag.h"
#include "xfs_ag_resv.h"

 

 
bool
xfs_ag_resv_critical(
	struct xfs_perag		*pag,
	enum xfs_ag_resv_type		type)
{
	xfs_extlen_t			avail;
	xfs_extlen_t			orig;

	switch (type) {
	case XFS_AG_RESV_METADATA:
		avail = pag->pagf_freeblks - pag->pag_rmapbt_resv.ar_reserved;
		orig = pag->pag_meta_resv.ar_asked;
		break;
	case XFS_AG_RESV_RMAPBT:
		avail = pag->pagf_freeblks + pag->pagf_flcount -
			pag->pag_meta_resv.ar_reserved;
		orig = pag->pag_rmapbt_resv.ar_asked;
		break;
	default:
		ASSERT(0);
		return false;
	}

	trace_xfs_ag_resv_critical(pag, type, avail);

	 
	return XFS_TEST_ERROR(avail < orig / 10 ||
			      avail < pag->pag_mount->m_agbtree_maxlevels,
			pag->pag_mount, XFS_ERRTAG_AG_RESV_CRITICAL);
}

 
xfs_extlen_t
xfs_ag_resv_needed(
	struct xfs_perag		*pag,
	enum xfs_ag_resv_type		type)
{
	xfs_extlen_t			len;

	len = pag->pag_meta_resv.ar_reserved + pag->pag_rmapbt_resv.ar_reserved;
	switch (type) {
	case XFS_AG_RESV_METADATA:
	case XFS_AG_RESV_RMAPBT:
		len -= xfs_perag_resv(pag, type)->ar_reserved;
		break;
	case XFS_AG_RESV_NONE:
		 
		break;
	default:
		ASSERT(0);
	}

	trace_xfs_ag_resv_needed(pag, type, len);

	return len;
}

 
static int
__xfs_ag_resv_free(
	struct xfs_perag		*pag,
	enum xfs_ag_resv_type		type)
{
	struct xfs_ag_resv		*resv;
	xfs_extlen_t			oldresv;
	int				error;

	trace_xfs_ag_resv_free(pag, type, 0);

	resv = xfs_perag_resv(pag, type);
	if (pag->pag_agno == 0)
		pag->pag_mount->m_ag_max_usable += resv->ar_asked;
	 
	if (type == XFS_AG_RESV_RMAPBT)
		oldresv = resv->ar_orig_reserved;
	else
		oldresv = resv->ar_reserved;
	error = xfs_mod_fdblocks(pag->pag_mount, oldresv, true);
	resv->ar_reserved = 0;
	resv->ar_asked = 0;
	resv->ar_orig_reserved = 0;

	if (error)
		trace_xfs_ag_resv_free_error(pag->pag_mount, pag->pag_agno,
				error, _RET_IP_);
	return error;
}

 
int
xfs_ag_resv_free(
	struct xfs_perag		*pag)
{
	int				error;
	int				err2;

	error = __xfs_ag_resv_free(pag, XFS_AG_RESV_RMAPBT);
	err2 = __xfs_ag_resv_free(pag, XFS_AG_RESV_METADATA);
	if (err2 && !error)
		error = err2;
	return error;
}

static int
__xfs_ag_resv_init(
	struct xfs_perag		*pag,
	enum xfs_ag_resv_type		type,
	xfs_extlen_t			ask,
	xfs_extlen_t			used)
{
	struct xfs_mount		*mp = pag->pag_mount;
	struct xfs_ag_resv		*resv;
	int				error;
	xfs_extlen_t			hidden_space;

	if (used > ask)
		ask = used;

	switch (type) {
	case XFS_AG_RESV_RMAPBT:
		 
		hidden_space = ask;
		break;
	case XFS_AG_RESV_METADATA:
		 
		hidden_space = ask - used;
		break;
	default:
		ASSERT(0);
		return -EINVAL;
	}

	if (XFS_TEST_ERROR(false, mp, XFS_ERRTAG_AG_RESV_FAIL))
		error = -ENOSPC;
	else
		error = xfs_mod_fdblocks(mp, -(int64_t)hidden_space, true);
	if (error) {
		trace_xfs_ag_resv_init_error(pag->pag_mount, pag->pag_agno,
				error, _RET_IP_);
		xfs_warn(mp,
"Per-AG reservation for AG %u failed.  Filesystem may run out of space.",
				pag->pag_agno);
		return error;
	}

	 
	if (pag->pag_agno == 0)
		mp->m_ag_max_usable -= ask;

	resv = xfs_perag_resv(pag, type);
	resv->ar_asked = ask;
	resv->ar_orig_reserved = hidden_space;
	resv->ar_reserved = ask - used;

	trace_xfs_ag_resv_init(pag, type, ask);
	return 0;
}

 
int
xfs_ag_resv_init(
	struct xfs_perag		*pag,
	struct xfs_trans		*tp)
{
	struct xfs_mount		*mp = pag->pag_mount;
	xfs_extlen_t			ask;
	xfs_extlen_t			used;
	int				error = 0, error2;
	bool				has_resv = false;

	 
	if (pag->pag_meta_resv.ar_asked == 0) {
		ask = used = 0;

		error = xfs_refcountbt_calc_reserves(mp, tp, pag, &ask, &used);
		if (error)
			goto out;

		error = xfs_finobt_calc_reserves(pag, tp, &ask, &used);
		if (error)
			goto out;

		error = __xfs_ag_resv_init(pag, XFS_AG_RESV_METADATA,
				ask, used);
		if (error) {
			 
			ask = used = 0;

			mp->m_finobt_nores = true;

			error = xfs_refcountbt_calc_reserves(mp, tp, pag, &ask,
					&used);
			if (error)
				goto out;

			error = __xfs_ag_resv_init(pag, XFS_AG_RESV_METADATA,
					ask, used);
			if (error)
				goto out;
		}
		if (ask)
			has_resv = true;
	}

	 
	if (pag->pag_rmapbt_resv.ar_asked == 0) {
		ask = used = 0;

		error = xfs_rmapbt_calc_reserves(mp, tp, pag, &ask, &used);
		if (error)
			goto out;

		error = __xfs_ag_resv_init(pag, XFS_AG_RESV_RMAPBT, ask, used);
		if (error)
			goto out;
		if (ask)
			has_resv = true;
	}

out:
	 
	if (has_resv) {
		error2 = xfs_alloc_read_agf(pag, tp, 0, NULL);
		if (error2)
			return error2;

		 
		if (!error &&
		    xfs_perag_resv(pag, XFS_AG_RESV_METADATA)->ar_reserved +
		    xfs_perag_resv(pag, XFS_AG_RESV_RMAPBT)->ar_reserved >
		    pag->pagf_freeblks + pag->pagf_flcount)
			error = -ENOSPC;
	}

	return error;
}

 
void
xfs_ag_resv_alloc_extent(
	struct xfs_perag		*pag,
	enum xfs_ag_resv_type		type,
	struct xfs_alloc_arg		*args)
{
	struct xfs_ag_resv		*resv;
	xfs_extlen_t			len;
	uint				field;

	trace_xfs_ag_resv_alloc_extent(pag, type, args->len);

	switch (type) {
	case XFS_AG_RESV_AGFL:
		return;
	case XFS_AG_RESV_METADATA:
	case XFS_AG_RESV_RMAPBT:
		resv = xfs_perag_resv(pag, type);
		break;
	default:
		ASSERT(0);
		fallthrough;
	case XFS_AG_RESV_NONE:
		field = args->wasdel ? XFS_TRANS_SB_RES_FDBLOCKS :
				       XFS_TRANS_SB_FDBLOCKS;
		xfs_trans_mod_sb(args->tp, field, -(int64_t)args->len);
		return;
	}

	len = min_t(xfs_extlen_t, args->len, resv->ar_reserved);
	resv->ar_reserved -= len;
	if (type == XFS_AG_RESV_RMAPBT)
		return;
	 
	xfs_trans_mod_sb(args->tp, XFS_TRANS_SB_RES_FDBLOCKS, -(int64_t)len);
	 
	if (args->len > len)
		xfs_trans_mod_sb(args->tp, XFS_TRANS_SB_FDBLOCKS,
				-((int64_t)args->len - len));
}

 
void
xfs_ag_resv_free_extent(
	struct xfs_perag		*pag,
	enum xfs_ag_resv_type		type,
	struct xfs_trans		*tp,
	xfs_extlen_t			len)
{
	xfs_extlen_t			leftover;
	struct xfs_ag_resv		*resv;

	trace_xfs_ag_resv_free_extent(pag, type, len);

	switch (type) {
	case XFS_AG_RESV_AGFL:
		return;
	case XFS_AG_RESV_METADATA:
	case XFS_AG_RESV_RMAPBT:
		resv = xfs_perag_resv(pag, type);
		break;
	default:
		ASSERT(0);
		fallthrough;
	case XFS_AG_RESV_NONE:
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_FDBLOCKS, (int64_t)len);
		return;
	}

	leftover = min_t(xfs_extlen_t, len, resv->ar_asked - resv->ar_reserved);
	resv->ar_reserved += leftover;
	if (type == XFS_AG_RESV_RMAPBT)
		return;
	 
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_RES_FDBLOCKS, len);
	 
	if (len > leftover)
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_FDBLOCKS, len - leftover);
}

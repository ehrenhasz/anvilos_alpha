
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_btree.h"
#include "xfs_bmap.h"
#include "xfs_refcount_btree.h"
#include "xfs_alloc.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_bit.h"
#include "xfs_refcount.h"
#include "xfs_rmap.h"
#include "xfs_ag.h"

struct kmem_cache	*xfs_refcount_intent_cache;

 
enum xfs_refc_adjust_op {
	XFS_REFCOUNT_ADJUST_INCREASE	= 1,
	XFS_REFCOUNT_ADJUST_DECREASE	= -1,
	XFS_REFCOUNT_ADJUST_COW_ALLOC	= 0,
	XFS_REFCOUNT_ADJUST_COW_FREE	= -1,
};

STATIC int __xfs_refcount_cow_alloc(struct xfs_btree_cur *rcur,
		xfs_agblock_t agbno, xfs_extlen_t aglen);
STATIC int __xfs_refcount_cow_free(struct xfs_btree_cur *rcur,
		xfs_agblock_t agbno, xfs_extlen_t aglen);

 
int
xfs_refcount_lookup_le(
	struct xfs_btree_cur	*cur,
	enum xfs_refc_domain	domain,
	xfs_agblock_t		bno,
	int			*stat)
{
	trace_xfs_refcount_lookup(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			xfs_refcount_encode_startblock(bno, domain),
			XFS_LOOKUP_LE);
	cur->bc_rec.rc.rc_startblock = bno;
	cur->bc_rec.rc.rc_blockcount = 0;
	cur->bc_rec.rc.rc_domain = domain;
	return xfs_btree_lookup(cur, XFS_LOOKUP_LE, stat);
}

 
int
xfs_refcount_lookup_ge(
	struct xfs_btree_cur	*cur,
	enum xfs_refc_domain	domain,
	xfs_agblock_t		bno,
	int			*stat)
{
	trace_xfs_refcount_lookup(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			xfs_refcount_encode_startblock(bno, domain),
			XFS_LOOKUP_GE);
	cur->bc_rec.rc.rc_startblock = bno;
	cur->bc_rec.rc.rc_blockcount = 0;
	cur->bc_rec.rc.rc_domain = domain;
	return xfs_btree_lookup(cur, XFS_LOOKUP_GE, stat);
}

 
int
xfs_refcount_lookup_eq(
	struct xfs_btree_cur	*cur,
	enum xfs_refc_domain	domain,
	xfs_agblock_t		bno,
	int			*stat)
{
	trace_xfs_refcount_lookup(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			xfs_refcount_encode_startblock(bno, domain),
			XFS_LOOKUP_LE);
	cur->bc_rec.rc.rc_startblock = bno;
	cur->bc_rec.rc.rc_blockcount = 0;
	cur->bc_rec.rc.rc_domain = domain;
	return xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
}

 
void
xfs_refcount_btrec_to_irec(
	const union xfs_btree_rec	*rec,
	struct xfs_refcount_irec	*irec)
{
	uint32_t			start;

	start = be32_to_cpu(rec->refc.rc_startblock);
	if (start & XFS_REFC_COWFLAG) {
		start &= ~XFS_REFC_COWFLAG;
		irec->rc_domain = XFS_REFC_DOMAIN_COW;
	} else {
		irec->rc_domain = XFS_REFC_DOMAIN_SHARED;
	}

	irec->rc_startblock = start;
	irec->rc_blockcount = be32_to_cpu(rec->refc.rc_blockcount);
	irec->rc_refcount = be32_to_cpu(rec->refc.rc_refcount);
}

 
xfs_failaddr_t
xfs_refcount_check_irec(
	struct xfs_btree_cur		*cur,
	const struct xfs_refcount_irec	*irec)
{
	struct xfs_perag		*pag = cur->bc_ag.pag;

	if (irec->rc_blockcount == 0 || irec->rc_blockcount > MAXREFCEXTLEN)
		return __this_address;

	if (!xfs_refcount_check_domain(irec))
		return __this_address;

	 
	if (!xfs_verify_agbext(pag, irec->rc_startblock, irec->rc_blockcount))
		return __this_address;

	if (irec->rc_refcount == 0 || irec->rc_refcount > MAXREFCOUNT)
		return __this_address;

	return NULL;
}

static inline int
xfs_refcount_complain_bad_rec(
	struct xfs_btree_cur		*cur,
	xfs_failaddr_t			fa,
	const struct xfs_refcount_irec	*irec)
{
	struct xfs_mount		*mp = cur->bc_mp;

	xfs_warn(mp,
 "Refcount BTree record corruption in AG %d detected at %pS!",
				cur->bc_ag.pag->pag_agno, fa);
	xfs_warn(mp,
		"Start block 0x%x, block count 0x%x, references 0x%x",
		irec->rc_startblock, irec->rc_blockcount, irec->rc_refcount);
	return -EFSCORRUPTED;
}

 
int
xfs_refcount_get_rec(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*irec,
	int				*stat)
{
	union xfs_btree_rec		*rec;
	xfs_failaddr_t			fa;
	int				error;

	error = xfs_btree_get_rec(cur, &rec, stat);
	if (error || !*stat)
		return error;

	xfs_refcount_btrec_to_irec(rec, irec);
	fa = xfs_refcount_check_irec(cur, irec);
	if (fa)
		return xfs_refcount_complain_bad_rec(cur, fa, irec);

	trace_xfs_refcount_get(cur->bc_mp, cur->bc_ag.pag->pag_agno, irec);
	return 0;
}

 
STATIC int
xfs_refcount_update(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*irec)
{
	union xfs_btree_rec	rec;
	uint32_t		start;
	int			error;

	trace_xfs_refcount_update(cur->bc_mp, cur->bc_ag.pag->pag_agno, irec);

	start = xfs_refcount_encode_startblock(irec->rc_startblock,
			irec->rc_domain);
	rec.refc.rc_startblock = cpu_to_be32(start);
	rec.refc.rc_blockcount = cpu_to_be32(irec->rc_blockcount);
	rec.refc.rc_refcount = cpu_to_be32(irec->rc_refcount);

	error = xfs_btree_update(cur, &rec);
	if (error)
		trace_xfs_refcount_update_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
int
xfs_refcount_insert(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*irec,
	int				*i)
{
	int				error;

	trace_xfs_refcount_insert(cur->bc_mp, cur->bc_ag.pag->pag_agno, irec);

	cur->bc_rec.rc.rc_startblock = irec->rc_startblock;
	cur->bc_rec.rc.rc_blockcount = irec->rc_blockcount;
	cur->bc_rec.rc.rc_refcount = irec->rc_refcount;
	cur->bc_rec.rc.rc_domain = irec->rc_domain;

	error = xfs_btree_insert(cur, i);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, *i != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

out_error:
	if (error)
		trace_xfs_refcount_insert_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_delete(
	struct xfs_btree_cur	*cur,
	int			*i)
{
	struct xfs_refcount_irec	irec;
	int			found_rec;
	int			error;

	error = xfs_refcount_get_rec(cur, &irec, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	trace_xfs_refcount_delete(cur->bc_mp, cur->bc_ag.pag->pag_agno, &irec);
	error = xfs_btree_delete(cur, i);
	if (XFS_IS_CORRUPT(cur->bc_mp, *i != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	if (error)
		goto out_error;
	error = xfs_refcount_lookup_ge(cur, irec.rc_domain, irec.rc_startblock,
			&found_rec);
out_error:
	if (error)
		trace_xfs_refcount_delete_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 

 
static inline xfs_agblock_t
xfs_refc_next(
	struct xfs_refcount_irec	*rc)
{
	return rc->rc_startblock + rc->rc_blockcount;
}

 
STATIC int
xfs_refcount_split_extent(
	struct xfs_btree_cur		*cur,
	enum xfs_refc_domain		domain,
	xfs_agblock_t			agbno,
	bool				*shape_changed)
{
	struct xfs_refcount_irec	rcext, tmp;
	int				found_rec;
	int				error;

	*shape_changed = false;
	error = xfs_refcount_lookup_le(cur, domain, agbno, &found_rec);
	if (error)
		goto out_error;
	if (!found_rec)
		return 0;

	error = xfs_refcount_get_rec(cur, &rcext, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	if (rcext.rc_domain != domain)
		return 0;
	if (rcext.rc_startblock == agbno || xfs_refc_next(&rcext) <= agbno)
		return 0;

	*shape_changed = true;
	trace_xfs_refcount_split_extent(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			&rcext, agbno);

	 
	tmp = rcext;
	tmp.rc_startblock = agbno;
	tmp.rc_blockcount -= (agbno - rcext.rc_startblock);
	error = xfs_refcount_update(cur, &tmp);
	if (error)
		goto out_error;

	 
	tmp = rcext;
	tmp.rc_blockcount = agbno - rcext.rc_startblock;
	error = xfs_refcount_insert(cur, &tmp, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	return error;

out_error:
	trace_xfs_refcount_split_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_merge_center_extents(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*left,
	struct xfs_refcount_irec	*center,
	struct xfs_refcount_irec	*right,
	unsigned long long		extlen,
	xfs_extlen_t			*aglen)
{
	int				error;
	int				found_rec;

	trace_xfs_refcount_merge_center_extents(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, left, center, right);

	ASSERT(left->rc_domain == center->rc_domain);
	ASSERT(right->rc_domain == center->rc_domain);

	 
	error = xfs_refcount_lookup_ge(cur, center->rc_domain,
			center->rc_startblock, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	error = xfs_refcount_delete(cur, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	if (center->rc_refcount > 1) {
		error = xfs_refcount_delete(cur, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
	}

	 
	error = xfs_refcount_lookup_le(cur, left->rc_domain,
			left->rc_startblock, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	left->rc_blockcount = extlen;
	error = xfs_refcount_update(cur, left);
	if (error)
		goto out_error;

	*aglen = 0;
	return error;

out_error:
	trace_xfs_refcount_merge_center_extents_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_merge_left_extent(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*left,
	struct xfs_refcount_irec	*cleft,
	xfs_agblock_t			*agbno,
	xfs_extlen_t			*aglen)
{
	int				error;
	int				found_rec;

	trace_xfs_refcount_merge_left_extent(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, left, cleft);

	ASSERT(left->rc_domain == cleft->rc_domain);

	 
	if (cleft->rc_refcount > 1) {
		error = xfs_refcount_lookup_le(cur, cleft->rc_domain,
				cleft->rc_startblock, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		error = xfs_refcount_delete(cur, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
	}

	 
	error = xfs_refcount_lookup_le(cur, left->rc_domain,
			left->rc_startblock, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	left->rc_blockcount += cleft->rc_blockcount;
	error = xfs_refcount_update(cur, left);
	if (error)
		goto out_error;

	*agbno += cleft->rc_blockcount;
	*aglen -= cleft->rc_blockcount;
	return error;

out_error:
	trace_xfs_refcount_merge_left_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_merge_right_extent(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*right,
	struct xfs_refcount_irec	*cright,
	xfs_extlen_t			*aglen)
{
	int				error;
	int				found_rec;

	trace_xfs_refcount_merge_right_extent(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, cright, right);

	ASSERT(right->rc_domain == cright->rc_domain);

	 
	if (cright->rc_refcount > 1) {
		error = xfs_refcount_lookup_le(cur, cright->rc_domain,
				cright->rc_startblock, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		error = xfs_refcount_delete(cur, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
	}

	 
	error = xfs_refcount_lookup_le(cur, right->rc_domain,
			right->rc_startblock, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	right->rc_startblock -= cright->rc_blockcount;
	right->rc_blockcount += cright->rc_blockcount;
	error = xfs_refcount_update(cur, right);
	if (error)
		goto out_error;

	*aglen -= cright->rc_blockcount;
	return error;

out_error:
	trace_xfs_refcount_merge_right_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_find_left_extents(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*left,
	struct xfs_refcount_irec	*cleft,
	enum xfs_refc_domain		domain,
	xfs_agblock_t			agbno,
	xfs_extlen_t			aglen)
{
	struct xfs_refcount_irec	tmp;
	int				error;
	int				found_rec;

	left->rc_startblock = cleft->rc_startblock = NULLAGBLOCK;
	error = xfs_refcount_lookup_le(cur, domain, agbno - 1, &found_rec);
	if (error)
		goto out_error;
	if (!found_rec)
		return 0;

	error = xfs_refcount_get_rec(cur, &tmp, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	if (tmp.rc_domain != domain)
		return 0;
	if (xfs_refc_next(&tmp) != agbno)
		return 0;
	 
	*left = tmp;

	error = xfs_btree_increment(cur, 0, &found_rec);
	if (error)
		goto out_error;
	if (found_rec) {
		error = xfs_refcount_get_rec(cur, &tmp, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		if (tmp.rc_domain != domain)
			goto not_found;

		 
		if (tmp.rc_startblock == agbno)
			*cleft = tmp;
		else {
			 
			cleft->rc_startblock = agbno;
			cleft->rc_blockcount = min(aglen,
					tmp.rc_startblock - agbno);
			cleft->rc_refcount = 1;
			cleft->rc_domain = domain;
		}
	} else {
not_found:
		 
		cleft->rc_startblock = agbno;
		cleft->rc_blockcount = aglen;
		cleft->rc_refcount = 1;
		cleft->rc_domain = domain;
	}
	trace_xfs_refcount_find_left_extent(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			left, cleft, agbno);
	return error;

out_error:
	trace_xfs_refcount_find_left_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_find_right_extents(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_irec	*right,
	struct xfs_refcount_irec	*cright,
	enum xfs_refc_domain		domain,
	xfs_agblock_t			agbno,
	xfs_extlen_t			aglen)
{
	struct xfs_refcount_irec	tmp;
	int				error;
	int				found_rec;

	right->rc_startblock = cright->rc_startblock = NULLAGBLOCK;
	error = xfs_refcount_lookup_ge(cur, domain, agbno + aglen, &found_rec);
	if (error)
		goto out_error;
	if (!found_rec)
		return 0;

	error = xfs_refcount_get_rec(cur, &tmp, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	if (tmp.rc_domain != domain)
		return 0;
	if (tmp.rc_startblock != agbno + aglen)
		return 0;
	 
	*right = tmp;

	error = xfs_btree_decrement(cur, 0, &found_rec);
	if (error)
		goto out_error;
	if (found_rec) {
		error = xfs_refcount_get_rec(cur, &tmp, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		if (tmp.rc_domain != domain)
			goto not_found;

		 
		if (xfs_refc_next(&tmp) == agbno + aglen)
			*cright = tmp;
		else {
			 
			cright->rc_startblock = max(agbno, xfs_refc_next(&tmp));
			cright->rc_blockcount = right->rc_startblock -
					cright->rc_startblock;
			cright->rc_refcount = 1;
			cright->rc_domain = domain;
		}
	} else {
not_found:
		 
		cright->rc_startblock = agbno;
		cright->rc_blockcount = aglen;
		cright->rc_refcount = 1;
		cright->rc_domain = domain;
	}
	trace_xfs_refcount_find_right_extent(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			cright, right, agbno + aglen);
	return error;

out_error:
	trace_xfs_refcount_find_right_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
static inline bool
xfs_refc_valid(
	const struct xfs_refcount_irec	*rc)
{
	return rc->rc_startblock != NULLAGBLOCK;
}

static inline xfs_nlink_t
xfs_refc_merge_refcount(
	const struct xfs_refcount_irec	*irec,
	enum xfs_refc_adjust_op		adjust)
{
	 
	if (irec->rc_refcount == MAXREFCOUNT)
		return MAXREFCOUNT;
	return irec->rc_refcount + adjust;
}

static inline bool
xfs_refc_want_merge_center(
	const struct xfs_refcount_irec	*left,
	const struct xfs_refcount_irec	*cleft,
	const struct xfs_refcount_irec	*cright,
	const struct xfs_refcount_irec	*right,
	bool				cleft_is_cright,
	enum xfs_refc_adjust_op		adjust,
	unsigned long long		*ulenp)
{
	unsigned long long		ulen = left->rc_blockcount;
	xfs_nlink_t			new_refcount;

	 
	if (!xfs_refc_valid(left)  || !xfs_refc_valid(right) ||
	    !xfs_refc_valid(cleft) || !xfs_refc_valid(cright))
		return false;

	 
	if (!cleft_is_cright)
		return false;

	 
	new_refcount = xfs_refc_merge_refcount(cleft, adjust);
	if (left->rc_refcount != new_refcount)
		return false;
	if (right->rc_refcount != new_refcount)
		return false;

	 
	ulen += cleft->rc_blockcount + right->rc_blockcount;
	if (ulen >= MAXREFCEXTLEN)
		return false;

	*ulenp = ulen;
	return true;
}

static inline bool
xfs_refc_want_merge_left(
	const struct xfs_refcount_irec	*left,
	const struct xfs_refcount_irec	*cleft,
	enum xfs_refc_adjust_op		adjust)
{
	unsigned long long		ulen = left->rc_blockcount;
	xfs_nlink_t			new_refcount;

	 
	if (!xfs_refc_valid(left) || !xfs_refc_valid(cleft))
		return false;

	 
	new_refcount = xfs_refc_merge_refcount(cleft, adjust);
	if (left->rc_refcount != new_refcount)
		return false;

	 
	ulen += cleft->rc_blockcount;
	if (ulen >= MAXREFCEXTLEN)
		return false;

	return true;
}

static inline bool
xfs_refc_want_merge_right(
	const struct xfs_refcount_irec	*cright,
	const struct xfs_refcount_irec	*right,
	enum xfs_refc_adjust_op		adjust)
{
	unsigned long long		ulen = right->rc_blockcount;
	xfs_nlink_t			new_refcount;

	 
	if (!xfs_refc_valid(right) || !xfs_refc_valid(cright))
		return false;

	 
	new_refcount = xfs_refc_merge_refcount(cright, adjust);
	if (right->rc_refcount != new_refcount)
		return false;

	 
	ulen += cright->rc_blockcount;
	if (ulen >= MAXREFCEXTLEN)
		return false;

	return true;
}

 
STATIC int
xfs_refcount_merge_extents(
	struct xfs_btree_cur	*cur,
	enum xfs_refc_domain	domain,
	xfs_agblock_t		*agbno,
	xfs_extlen_t		*aglen,
	enum xfs_refc_adjust_op adjust,
	bool			*shape_changed)
{
	struct xfs_refcount_irec	left = {0}, cleft = {0};
	struct xfs_refcount_irec	cright = {0}, right = {0};
	int				error;
	unsigned long long		ulen;
	bool				cequal;

	*shape_changed = false;
	 
	error = xfs_refcount_find_left_extents(cur, &left, &cleft, domain,
			*agbno, *aglen);
	if (error)
		return error;
	error = xfs_refcount_find_right_extents(cur, &right, &cright, domain,
			*agbno, *aglen);
	if (error)
		return error;

	 
	if (!xfs_refc_valid(&left) && !xfs_refc_valid(&right))
		return 0;

	cequal = (cleft.rc_startblock == cright.rc_startblock) &&
		 (cleft.rc_blockcount == cright.rc_blockcount);

	 
	if (xfs_refc_want_merge_center(&left, &cleft, &cright, &right, cequal,
				adjust, &ulen)) {
		*shape_changed = true;
		return xfs_refcount_merge_center_extents(cur, &left, &cleft,
				&right, ulen, aglen);
	}

	 
	if (xfs_refc_want_merge_left(&left, &cleft, adjust)) {
		*shape_changed = true;
		error = xfs_refcount_merge_left_extent(cur, &left, &cleft,
				agbno, aglen);
		if (error)
			return error;

		 
		if (cequal)
			return 0;
	}

	 
	if (xfs_refc_want_merge_right(&cright, &right, adjust)) {
		*shape_changed = true;
		return xfs_refcount_merge_right_extent(cur, &right, &cright,
				aglen);
	}

	return 0;
}

 
static bool
xfs_refcount_still_have_space(
	struct xfs_btree_cur		*cur)
{
	unsigned long			overhead;

	 
	overhead = xfs_allocfree_block_count(cur->bc_mp,
				cur->bc_ag.refc.shape_changes);
	overhead += cur->bc_mp->m_refc_maxlevels;
	overhead *= cur->bc_mp->m_sb.sb_blocksize;

	 
	if (cur->bc_ag.refc.nr_ops > 2 &&
	    XFS_TEST_ERROR(false, cur->bc_mp,
			XFS_ERRTAG_REFCOUNT_CONTINUE_UPDATE))
		return false;

	if (cur->bc_ag.refc.nr_ops == 0)
		return true;
	else if (overhead > cur->bc_tp->t_log_res)
		return false;
	return  cur->bc_tp->t_log_res - overhead >
		cur->bc_ag.refc.nr_ops * XFS_REFCOUNT_ITEM_OVERHEAD;
}

 
STATIC int
xfs_refcount_adjust_extents(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		*agbno,
	xfs_extlen_t		*aglen,
	enum xfs_refc_adjust_op	adj)
{
	struct xfs_refcount_irec	ext, tmp;
	int				error;
	int				found_rec, found_tmp;
	xfs_fsblock_t			fsbno;

	 
	if (*aglen == 0)
		return 0;

	error = xfs_refcount_lookup_ge(cur, XFS_REFC_DOMAIN_SHARED, *agbno,
			&found_rec);
	if (error)
		goto out_error;

	while (*aglen > 0 && xfs_refcount_still_have_space(cur)) {
		error = xfs_refcount_get_rec(cur, &ext, &found_rec);
		if (error)
			goto out_error;
		if (!found_rec || ext.rc_domain != XFS_REFC_DOMAIN_SHARED) {
			ext.rc_startblock = cur->bc_mp->m_sb.sb_agblocks;
			ext.rc_blockcount = 0;
			ext.rc_refcount = 0;
			ext.rc_domain = XFS_REFC_DOMAIN_SHARED;
		}

		 
		if (ext.rc_startblock != *agbno) {
			tmp.rc_startblock = *agbno;
			tmp.rc_blockcount = min(*aglen,
					ext.rc_startblock - *agbno);
			tmp.rc_refcount = 1 + adj;
			tmp.rc_domain = XFS_REFC_DOMAIN_SHARED;

			trace_xfs_refcount_modify_extent(cur->bc_mp,
					cur->bc_ag.pag->pag_agno, &tmp);

			 
			cur->bc_ag.refc.nr_ops++;
			if (tmp.rc_refcount) {
				error = xfs_refcount_insert(cur, &tmp,
						&found_tmp);
				if (error)
					goto out_error;
				if (XFS_IS_CORRUPT(cur->bc_mp,
						   found_tmp != 1)) {
					error = -EFSCORRUPTED;
					goto out_error;
				}
			} else {
				fsbno = XFS_AGB_TO_FSB(cur->bc_mp,
						cur->bc_ag.pag->pag_agno,
						tmp.rc_startblock);
				error = xfs_free_extent_later(cur->bc_tp, fsbno,
						  tmp.rc_blockcount, NULL,
						  XFS_AG_RESV_NONE);
				if (error)
					goto out_error;
			}

			(*agbno) += tmp.rc_blockcount;
			(*aglen) -= tmp.rc_blockcount;

			 
			if (*aglen == 0 || !xfs_refcount_still_have_space(cur))
				break;

			 
			error = xfs_refcount_lookup_ge(cur,
					XFS_REFC_DOMAIN_SHARED, *agbno,
					&found_rec);
			if (error)
				goto out_error;
		}

		 
		if (XFS_IS_CORRUPT(cur->bc_mp, ext.rc_blockcount == 0) ||
		    XFS_IS_CORRUPT(cur->bc_mp, ext.rc_blockcount > *aglen)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		 
		if (ext.rc_refcount == MAXREFCOUNT)
			goto skip;
		ext.rc_refcount += adj;
		trace_xfs_refcount_modify_extent(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, &ext);
		cur->bc_ag.refc.nr_ops++;
		if (ext.rc_refcount > 1) {
			error = xfs_refcount_update(cur, &ext);
			if (error)
				goto out_error;
		} else if (ext.rc_refcount == 1) {
			error = xfs_refcount_delete(cur, &found_rec);
			if (error)
				goto out_error;
			if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
				error = -EFSCORRUPTED;
				goto out_error;
			}
			goto advloop;
		} else {
			fsbno = XFS_AGB_TO_FSB(cur->bc_mp,
					cur->bc_ag.pag->pag_agno,
					ext.rc_startblock);
			error = xfs_free_extent_later(cur->bc_tp, fsbno,
					ext.rc_blockcount, NULL,
					XFS_AG_RESV_NONE);
			if (error)
				goto out_error;
		}

skip:
		error = xfs_btree_increment(cur, 0, &found_rec);
		if (error)
			goto out_error;

advloop:
		(*agbno) += ext.rc_blockcount;
		(*aglen) -= ext.rc_blockcount;
	}

	return error;
out_error:
	trace_xfs_refcount_modify_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_adjust(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		*agbno,
	xfs_extlen_t		*aglen,
	enum xfs_refc_adjust_op	adj)
{
	bool			shape_changed;
	int			shape_changes = 0;
	int			error;

	if (adj == XFS_REFCOUNT_ADJUST_INCREASE)
		trace_xfs_refcount_increase(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, *agbno, *aglen);
	else
		trace_xfs_refcount_decrease(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, *agbno, *aglen);

	 
	error = xfs_refcount_split_extent(cur, XFS_REFC_DOMAIN_SHARED,
			*agbno, &shape_changed);
	if (error)
		goto out_error;
	if (shape_changed)
		shape_changes++;

	error = xfs_refcount_split_extent(cur, XFS_REFC_DOMAIN_SHARED,
			*agbno + *aglen, &shape_changed);
	if (error)
		goto out_error;
	if (shape_changed)
		shape_changes++;

	 
	error = xfs_refcount_merge_extents(cur, XFS_REFC_DOMAIN_SHARED,
			agbno, aglen, adj, &shape_changed);
	if (error)
		goto out_error;
	if (shape_changed)
		shape_changes++;
	if (shape_changes)
		cur->bc_ag.refc.shape_changes++;

	 
	error = xfs_refcount_adjust_extents(cur, agbno, aglen, adj);
	if (error)
		goto out_error;

	return 0;

out_error:
	trace_xfs_refcount_adjust_error(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			error, _RET_IP_);
	return error;
}

 
void
xfs_refcount_finish_one_cleanup(
	struct xfs_trans	*tp,
	struct xfs_btree_cur	*rcur,
	int			error)
{
	struct xfs_buf		*agbp;

	if (rcur == NULL)
		return;
	agbp = rcur->bc_ag.agbp;
	xfs_btree_del_cursor(rcur, error);
	if (error)
		xfs_trans_brelse(tp, agbp);
}

 
static inline int
xfs_refcount_continue_op(
	struct xfs_btree_cur		*cur,
	struct xfs_refcount_intent	*ri,
	xfs_agblock_t			new_agbno)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_perag		*pag = cur->bc_ag.pag;

	if (XFS_IS_CORRUPT(mp, !xfs_verify_agbext(pag, new_agbno,
					ri->ri_blockcount)))
		return -EFSCORRUPTED;

	ri->ri_startblock = XFS_AGB_TO_FSB(mp, pag->pag_agno, new_agbno);

	ASSERT(xfs_verify_fsbext(mp, ri->ri_startblock, ri->ri_blockcount));
	ASSERT(pag->pag_agno == XFS_FSB_TO_AGNO(mp, ri->ri_startblock));

	return 0;
}

 
int
xfs_refcount_finish_one(
	struct xfs_trans		*tp,
	struct xfs_refcount_intent	*ri,
	struct xfs_btree_cur		**pcur)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_btree_cur		*rcur;
	struct xfs_buf			*agbp = NULL;
	int				error = 0;
	xfs_agblock_t			bno;
	unsigned long			nr_ops = 0;
	int				shape_changes = 0;

	bno = XFS_FSB_TO_AGBNO(mp, ri->ri_startblock);

	trace_xfs_refcount_deferred(mp, XFS_FSB_TO_AGNO(mp, ri->ri_startblock),
			ri->ri_type, XFS_FSB_TO_AGBNO(mp, ri->ri_startblock),
			ri->ri_blockcount);

	if (XFS_TEST_ERROR(false, mp, XFS_ERRTAG_REFCOUNT_FINISH_ONE))
		return -EIO;

	 
	rcur = *pcur;
	if (rcur != NULL && rcur->bc_ag.pag != ri->ri_pag) {
		nr_ops = rcur->bc_ag.refc.nr_ops;
		shape_changes = rcur->bc_ag.refc.shape_changes;
		xfs_refcount_finish_one_cleanup(tp, rcur, 0);
		rcur = NULL;
		*pcur = NULL;
	}
	if (rcur == NULL) {
		error = xfs_alloc_read_agf(ri->ri_pag, tp,
				XFS_ALLOC_FLAG_FREEING, &agbp);
		if (error)
			return error;

		rcur = xfs_refcountbt_init_cursor(mp, tp, agbp, ri->ri_pag);
		rcur->bc_ag.refc.nr_ops = nr_ops;
		rcur->bc_ag.refc.shape_changes = shape_changes;
	}
	*pcur = rcur;

	switch (ri->ri_type) {
	case XFS_REFCOUNT_INCREASE:
		error = xfs_refcount_adjust(rcur, &bno, &ri->ri_blockcount,
				XFS_REFCOUNT_ADJUST_INCREASE);
		if (error)
			return error;
		if (ri->ri_blockcount > 0)
			error = xfs_refcount_continue_op(rcur, ri, bno);
		break;
	case XFS_REFCOUNT_DECREASE:
		error = xfs_refcount_adjust(rcur, &bno, &ri->ri_blockcount,
				XFS_REFCOUNT_ADJUST_DECREASE);
		if (error)
			return error;
		if (ri->ri_blockcount > 0)
			error = xfs_refcount_continue_op(rcur, ri, bno);
		break;
	case XFS_REFCOUNT_ALLOC_COW:
		error = __xfs_refcount_cow_alloc(rcur, bno, ri->ri_blockcount);
		if (error)
			return error;
		ri->ri_blockcount = 0;
		break;
	case XFS_REFCOUNT_FREE_COW:
		error = __xfs_refcount_cow_free(rcur, bno, ri->ri_blockcount);
		if (error)
			return error;
		ri->ri_blockcount = 0;
		break;
	default:
		ASSERT(0);
		return -EFSCORRUPTED;
	}
	if (!error && ri->ri_blockcount > 0)
		trace_xfs_refcount_finish_one_leftover(mp, ri->ri_pag->pag_agno,
				ri->ri_type, bno, ri->ri_blockcount);
	return error;
}

 
static void
__xfs_refcount_add(
	struct xfs_trans		*tp,
	enum xfs_refcount_intent_type	type,
	xfs_fsblock_t			startblock,
	xfs_extlen_t			blockcount)
{
	struct xfs_refcount_intent	*ri;

	trace_xfs_refcount_defer(tp->t_mountp,
			XFS_FSB_TO_AGNO(tp->t_mountp, startblock),
			type, XFS_FSB_TO_AGBNO(tp->t_mountp, startblock),
			blockcount);

	ri = kmem_cache_alloc(xfs_refcount_intent_cache,
			GFP_NOFS | __GFP_NOFAIL);
	INIT_LIST_HEAD(&ri->ri_list);
	ri->ri_type = type;
	ri->ri_startblock = startblock;
	ri->ri_blockcount = blockcount;

	xfs_refcount_update_get_group(tp->t_mountp, ri);
	xfs_defer_add(tp, XFS_DEFER_OPS_TYPE_REFCOUNT, &ri->ri_list);
}

 
void
xfs_refcount_increase_extent(
	struct xfs_trans		*tp,
	struct xfs_bmbt_irec		*PREV)
{
	if (!xfs_has_reflink(tp->t_mountp))
		return;

	__xfs_refcount_add(tp, XFS_REFCOUNT_INCREASE, PREV->br_startblock,
			PREV->br_blockcount);
}

 
void
xfs_refcount_decrease_extent(
	struct xfs_trans		*tp,
	struct xfs_bmbt_irec		*PREV)
{
	if (!xfs_has_reflink(tp->t_mountp))
		return;

	__xfs_refcount_add(tp, XFS_REFCOUNT_DECREASE, PREV->br_startblock,
			PREV->br_blockcount);
}

 
int
xfs_refcount_find_shared(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			agbno,
	xfs_extlen_t			aglen,
	xfs_agblock_t			*fbno,
	xfs_extlen_t			*flen,
	bool				find_end_of_shared)
{
	struct xfs_refcount_irec	tmp;
	int				i;
	int				have;
	int				error;

	trace_xfs_refcount_find_shared(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			agbno, aglen);

	 
	*fbno = NULLAGBLOCK;
	*flen = 0;

	 
	error = xfs_refcount_lookup_le(cur, XFS_REFC_DOMAIN_SHARED, agbno,
			&have);
	if (error)
		goto out_error;
	if (!have) {
		 
		error = xfs_btree_increment(cur, 0, &have);
		if (error)
			goto out_error;
		if (!have)
			goto done;
	}
	error = xfs_refcount_get_rec(cur, &tmp, &i);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	if (tmp.rc_domain != XFS_REFC_DOMAIN_SHARED)
		goto done;

	 
	if (tmp.rc_startblock + tmp.rc_blockcount <= agbno) {
		error = xfs_btree_increment(cur, 0, &have);
		if (error)
			goto out_error;
		if (!have)
			goto done;
		error = xfs_refcount_get_rec(cur, &tmp, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		if (tmp.rc_domain != XFS_REFC_DOMAIN_SHARED)
			goto done;
	}

	 
	if (tmp.rc_startblock >= agbno + aglen)
		goto done;

	 
	if (tmp.rc_startblock < agbno) {
		tmp.rc_blockcount -= (agbno - tmp.rc_startblock);
		tmp.rc_startblock = agbno;
	}

	*fbno = tmp.rc_startblock;
	*flen = min(tmp.rc_blockcount, agbno + aglen - *fbno);
	if (!find_end_of_shared)
		goto done;

	 
	while (*fbno + *flen < agbno + aglen) {
		error = xfs_btree_increment(cur, 0, &have);
		if (error)
			goto out_error;
		if (!have)
			break;
		error = xfs_refcount_get_rec(cur, &tmp, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		if (tmp.rc_domain != XFS_REFC_DOMAIN_SHARED ||
		    tmp.rc_startblock >= agbno + aglen ||
		    tmp.rc_startblock != *fbno + *flen)
			break;
		*flen = min(*flen + tmp.rc_blockcount, agbno + aglen - *fbno);
	}

done:
	trace_xfs_refcount_find_shared_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, *fbno, *flen);

out_error:
	if (error)
		trace_xfs_refcount_find_shared_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
 
STATIC int
xfs_refcount_adjust_cow_extents(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		agbno,
	xfs_extlen_t		aglen,
	enum xfs_refc_adjust_op	adj)
{
	struct xfs_refcount_irec	ext, tmp;
	int				error;
	int				found_rec, found_tmp;

	if (aglen == 0)
		return 0;

	 
	error = xfs_refcount_lookup_ge(cur, XFS_REFC_DOMAIN_COW, agbno,
			&found_rec);
	if (error)
		goto out_error;
	error = xfs_refcount_get_rec(cur, &ext, &found_rec);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(cur->bc_mp, found_rec &&
				ext.rc_domain != XFS_REFC_DOMAIN_COW)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	if (!found_rec) {
		ext.rc_startblock = cur->bc_mp->m_sb.sb_agblocks;
		ext.rc_blockcount = 0;
		ext.rc_refcount = 0;
		ext.rc_domain = XFS_REFC_DOMAIN_COW;
	}

	switch (adj) {
	case XFS_REFCOUNT_ADJUST_COW_ALLOC:
		 
		if (XFS_IS_CORRUPT(cur->bc_mp,
				   agbno + aglen > ext.rc_startblock)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		tmp.rc_startblock = agbno;
		tmp.rc_blockcount = aglen;
		tmp.rc_refcount = 1;
		tmp.rc_domain = XFS_REFC_DOMAIN_COW;

		trace_xfs_refcount_modify_extent(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, &tmp);

		error = xfs_refcount_insert(cur, &tmp,
				&found_tmp);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_tmp != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		break;
	case XFS_REFCOUNT_ADJUST_COW_FREE:
		 
		if (XFS_IS_CORRUPT(cur->bc_mp, ext.rc_startblock != agbno)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		if (XFS_IS_CORRUPT(cur->bc_mp, ext.rc_blockcount != aglen)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		if (XFS_IS_CORRUPT(cur->bc_mp, ext.rc_refcount != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		ext.rc_refcount = 0;
		trace_xfs_refcount_modify_extent(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, &ext);
		error = xfs_refcount_delete(cur, &found_rec);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(cur->bc_mp, found_rec != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		break;
	default:
		ASSERT(0);
	}

	return error;
out_error:
	trace_xfs_refcount_modify_extent_error(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_refcount_adjust_cow(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		agbno,
	xfs_extlen_t		aglen,
	enum xfs_refc_adjust_op	adj)
{
	bool			shape_changed;
	int			error;

	 
	error = xfs_refcount_split_extent(cur, XFS_REFC_DOMAIN_COW,
			agbno, &shape_changed);
	if (error)
		goto out_error;

	error = xfs_refcount_split_extent(cur, XFS_REFC_DOMAIN_COW,
			agbno + aglen, &shape_changed);
	if (error)
		goto out_error;

	 
	error = xfs_refcount_merge_extents(cur, XFS_REFC_DOMAIN_COW, &agbno,
			&aglen, adj, &shape_changed);
	if (error)
		goto out_error;

	 
	error = xfs_refcount_adjust_cow_extents(cur, agbno, aglen, adj);
	if (error)
		goto out_error;

	return 0;

out_error:
	trace_xfs_refcount_adjust_cow_error(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			error, _RET_IP_);
	return error;
}

 
STATIC int
__xfs_refcount_cow_alloc(
	struct xfs_btree_cur	*rcur,
	xfs_agblock_t		agbno,
	xfs_extlen_t		aglen)
{
	trace_xfs_refcount_cow_increase(rcur->bc_mp, rcur->bc_ag.pag->pag_agno,
			agbno, aglen);

	 
	return xfs_refcount_adjust_cow(rcur, agbno, aglen,
			XFS_REFCOUNT_ADJUST_COW_ALLOC);
}

 
STATIC int
__xfs_refcount_cow_free(
	struct xfs_btree_cur	*rcur,
	xfs_agblock_t		agbno,
	xfs_extlen_t		aglen)
{
	trace_xfs_refcount_cow_decrease(rcur->bc_mp, rcur->bc_ag.pag->pag_agno,
			agbno, aglen);

	 
	return xfs_refcount_adjust_cow(rcur, agbno, aglen,
			XFS_REFCOUNT_ADJUST_COW_FREE);
}

 
void
xfs_refcount_alloc_cow_extent(
	struct xfs_trans		*tp,
	xfs_fsblock_t			fsb,
	xfs_extlen_t			len)
{
	struct xfs_mount		*mp = tp->t_mountp;

	if (!xfs_has_reflink(mp))
		return;

	__xfs_refcount_add(tp, XFS_REFCOUNT_ALLOC_COW, fsb, len);

	 
	xfs_rmap_alloc_extent(tp, XFS_FSB_TO_AGNO(mp, fsb),
			XFS_FSB_TO_AGBNO(mp, fsb), len, XFS_RMAP_OWN_COW);
}

 
void
xfs_refcount_free_cow_extent(
	struct xfs_trans		*tp,
	xfs_fsblock_t			fsb,
	xfs_extlen_t			len)
{
	struct xfs_mount		*mp = tp->t_mountp;

	if (!xfs_has_reflink(mp))
		return;

	 
	xfs_rmap_free_extent(tp, XFS_FSB_TO_AGNO(mp, fsb),
			XFS_FSB_TO_AGBNO(mp, fsb), len, XFS_RMAP_OWN_COW);
	__xfs_refcount_add(tp, XFS_REFCOUNT_FREE_COW, fsb, len);
}

struct xfs_refcount_recovery {
	struct list_head		rr_list;
	struct xfs_refcount_irec	rr_rrec;
};

 
STATIC int
xfs_refcount_recover_extent(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_rec	*rec,
	void				*priv)
{
	struct list_head		*debris = priv;
	struct xfs_refcount_recovery	*rr;

	if (XFS_IS_CORRUPT(cur->bc_mp,
			   be32_to_cpu(rec->refc.rc_refcount) != 1))
		return -EFSCORRUPTED;

	rr = kmalloc(sizeof(struct xfs_refcount_recovery),
			GFP_KERNEL | __GFP_NOFAIL);
	INIT_LIST_HEAD(&rr->rr_list);
	xfs_refcount_btrec_to_irec(rec, &rr->rr_rrec);

	if (xfs_refcount_check_irec(cur, &rr->rr_rrec) != NULL ||
	    XFS_IS_CORRUPT(cur->bc_mp,
			   rr->rr_rrec.rc_domain != XFS_REFC_DOMAIN_COW)) {
		kfree(rr);
		return -EFSCORRUPTED;
	}

	list_add_tail(&rr->rr_list, debris);
	return 0;
}

 
int
xfs_refcount_recover_cow_leftovers(
	struct xfs_mount		*mp,
	struct xfs_perag		*pag)
{
	struct xfs_trans		*tp;
	struct xfs_btree_cur		*cur;
	struct xfs_buf			*agbp;
	struct xfs_refcount_recovery	*rr, *n;
	struct list_head		debris;
	union xfs_btree_irec		low = {
		.rc.rc_domain		= XFS_REFC_DOMAIN_COW,
	};
	union xfs_btree_irec		high = {
		.rc.rc_domain		= XFS_REFC_DOMAIN_COW,
		.rc.rc_startblock	= -1U,
	};
	xfs_fsblock_t			fsb;
	int				error;

	 
	BUILD_BUG_ON(XFS_MAX_CRC_AG_BLOCKS >= XFS_REFC_COWFLAG);
	if (mp->m_sb.sb_agblocks > XFS_MAX_CRC_AG_BLOCKS)
		return -EOPNOTSUPP;

	INIT_LIST_HEAD(&debris);

	 
	error = xfs_trans_alloc_empty(mp, &tp);
	if (error)
		return error;

	error = xfs_alloc_read_agf(pag, tp, 0, &agbp);
	if (error)
		goto out_trans;
	cur = xfs_refcountbt_init_cursor(mp, tp, agbp, pag);

	 
	error = xfs_btree_query_range(cur, &low, &high,
			xfs_refcount_recover_extent, &debris);
	xfs_btree_del_cursor(cur, error);
	xfs_trans_brelse(tp, agbp);
	xfs_trans_cancel(tp);
	if (error)
		goto out_free;

	 
	list_for_each_entry_safe(rr, n, &debris, rr_list) {
		 
		error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, 0, 0, 0, &tp);
		if (error)
			goto out_free;

		trace_xfs_refcount_recover_extent(mp, pag->pag_agno,
				&rr->rr_rrec);

		 
		fsb = XFS_AGB_TO_FSB(mp, pag->pag_agno,
				rr->rr_rrec.rc_startblock);
		xfs_refcount_free_cow_extent(tp, fsb,
				rr->rr_rrec.rc_blockcount);

		 
		error = xfs_free_extent_later(tp, fsb,
				rr->rr_rrec.rc_blockcount, NULL,
				XFS_AG_RESV_NONE);
		if (error)
			goto out_trans;

		error = xfs_trans_commit(tp);
		if (error)
			goto out_free;

		list_del(&rr->rr_list);
		kfree(rr);
	}

	return error;
out_trans:
	xfs_trans_cancel(tp);
out_free:
	 
	list_for_each_entry_safe(rr, n, &debris, rr_list) {
		list_del(&rr->rr_list);
		kfree(rr);
	}
	return error;
}

 
int
xfs_refcount_has_records(
	struct xfs_btree_cur	*cur,
	enum xfs_refc_domain	domain,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	enum xbtree_recpacking	*outcome)
{
	union xfs_btree_irec	low;
	union xfs_btree_irec	high;

	memset(&low, 0, sizeof(low));
	low.rc.rc_startblock = bno;
	memset(&high, 0xFF, sizeof(high));
	high.rc.rc_startblock = bno + len - 1;
	low.rc.rc_domain = high.rc.rc_domain = domain;

	return xfs_btree_has_records(cur, &low, &high, NULL, outcome);
}

int __init
xfs_refcount_intent_init_cache(void)
{
	xfs_refcount_intent_cache = kmem_cache_create("xfs_refc_intent",
			sizeof(struct xfs_refcount_intent),
			0, 0, NULL);

	return xfs_refcount_intent_cache != NULL ? 0 : -ENOMEM;
}

void
xfs_refcount_intent_destroy_cache(void)
{
	kmem_cache_destroy(xfs_refcount_intent_cache);
	xfs_refcount_intent_cache = NULL;
}

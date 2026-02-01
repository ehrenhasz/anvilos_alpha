
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_sb.h"
#include "xfs_defer.h"
#include "xfs_btree.h"
#include "xfs_trans.h"
#include "xfs_alloc.h"
#include "xfs_rmap.h"
#include "xfs_rmap_btree.h"
#include "xfs_trace.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_inode.h"
#include "xfs_ag.h"

struct kmem_cache	*xfs_rmap_intent_cache;

 
int
xfs_rmap_lookup_le(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags,
	struct xfs_rmap_irec	*irec,
	int			*stat)
{
	int			get_stat = 0;
	int			error;

	cur->bc_rec.r.rm_startblock = bno;
	cur->bc_rec.r.rm_blockcount = 0;
	cur->bc_rec.r.rm_owner = owner;
	cur->bc_rec.r.rm_offset = offset;
	cur->bc_rec.r.rm_flags = flags;

	error = xfs_btree_lookup(cur, XFS_LOOKUP_LE, stat);
	if (error || !(*stat) || !irec)
		return error;

	error = xfs_rmap_get_rec(cur, irec, &get_stat);
	if (error)
		return error;
	if (!get_stat)
		return -EFSCORRUPTED;

	return 0;
}

 
int
xfs_rmap_lookup_eq(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags,
	int			*stat)
{
	cur->bc_rec.r.rm_startblock = bno;
	cur->bc_rec.r.rm_blockcount = len;
	cur->bc_rec.r.rm_owner = owner;
	cur->bc_rec.r.rm_offset = offset;
	cur->bc_rec.r.rm_flags = flags;
	return xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
}

 
STATIC int
xfs_rmap_update(
	struct xfs_btree_cur	*cur,
	struct xfs_rmap_irec	*irec)
{
	union xfs_btree_rec	rec;
	int			error;

	trace_xfs_rmap_update(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			irec->rm_startblock, irec->rm_blockcount,
			irec->rm_owner, irec->rm_offset, irec->rm_flags);

	rec.rmap.rm_startblock = cpu_to_be32(irec->rm_startblock);
	rec.rmap.rm_blockcount = cpu_to_be32(irec->rm_blockcount);
	rec.rmap.rm_owner = cpu_to_be64(irec->rm_owner);
	rec.rmap.rm_offset = cpu_to_be64(
			xfs_rmap_irec_offset_pack(irec));
	error = xfs_btree_update(cur, &rec);
	if (error)
		trace_xfs_rmap_update_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

int
xfs_rmap_insert(
	struct xfs_btree_cur	*rcur,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags)
{
	int			i;
	int			error;

	trace_xfs_rmap_insert(rcur->bc_mp, rcur->bc_ag.pag->pag_agno, agbno,
			len, owner, offset, flags);

	error = xfs_rmap_lookup_eq(rcur, agbno, len, owner, offset, flags, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(rcur->bc_mp, i != 0)) {
		error = -EFSCORRUPTED;
		goto done;
	}

	rcur->bc_rec.r.rm_startblock = agbno;
	rcur->bc_rec.r.rm_blockcount = len;
	rcur->bc_rec.r.rm_owner = owner;
	rcur->bc_rec.r.rm_offset = offset;
	rcur->bc_rec.r.rm_flags = flags;
	error = xfs_btree_insert(rcur, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(rcur->bc_mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}
done:
	if (error)
		trace_xfs_rmap_insert_error(rcur->bc_mp,
				rcur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

STATIC int
xfs_rmap_delete(
	struct xfs_btree_cur	*rcur,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags)
{
	int			i;
	int			error;

	trace_xfs_rmap_delete(rcur->bc_mp, rcur->bc_ag.pag->pag_agno, agbno,
			len, owner, offset, flags);

	error = xfs_rmap_lookup_eq(rcur, agbno, len, owner, offset, flags, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(rcur->bc_mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}

	error = xfs_btree_delete(rcur, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(rcur->bc_mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}
done:
	if (error)
		trace_xfs_rmap_delete_error(rcur->bc_mp,
				rcur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
xfs_failaddr_t
xfs_rmap_btrec_to_irec(
	const union xfs_btree_rec	*rec,
	struct xfs_rmap_irec		*irec)
{
	irec->rm_startblock = be32_to_cpu(rec->rmap.rm_startblock);
	irec->rm_blockcount = be32_to_cpu(rec->rmap.rm_blockcount);
	irec->rm_owner = be64_to_cpu(rec->rmap.rm_owner);
	return xfs_rmap_irec_offset_unpack(be64_to_cpu(rec->rmap.rm_offset),
			irec);
}

 
xfs_failaddr_t
xfs_rmap_check_irec(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*irec)
{
	struct xfs_mount		*mp = cur->bc_mp;
	bool				is_inode;
	bool				is_unwritten;
	bool				is_bmbt;
	bool				is_attr;

	if (irec->rm_blockcount == 0)
		return __this_address;
	if (irec->rm_startblock <= XFS_AGFL_BLOCK(mp)) {
		if (irec->rm_owner != XFS_RMAP_OWN_FS)
			return __this_address;
		if (irec->rm_blockcount != XFS_AGFL_BLOCK(mp) + 1)
			return __this_address;
	} else {
		 
		if (!xfs_verify_agbext(cur->bc_ag.pag, irec->rm_startblock,
						       irec->rm_blockcount))
			return __this_address;
	}

	if (!(xfs_verify_ino(mp, irec->rm_owner) ||
	      (irec->rm_owner <= XFS_RMAP_OWN_FS &&
	       irec->rm_owner >= XFS_RMAP_OWN_MIN)))
		return __this_address;

	 
	is_inode = !XFS_RMAP_NON_INODE_OWNER(irec->rm_owner);
	is_bmbt = irec->rm_flags & XFS_RMAP_BMBT_BLOCK;
	is_attr = irec->rm_flags & XFS_RMAP_ATTR_FORK;
	is_unwritten = irec->rm_flags & XFS_RMAP_UNWRITTEN;

	if (is_bmbt && irec->rm_offset != 0)
		return __this_address;

	if (!is_inode && irec->rm_offset != 0)
		return __this_address;

	if (is_unwritten && (is_bmbt || !is_inode || is_attr))
		return __this_address;

	if (!is_inode && (is_bmbt || is_unwritten || is_attr))
		return __this_address;

	 
	if (is_inode && !is_bmbt &&
	    !xfs_verify_fileext(mp, irec->rm_offset, irec->rm_blockcount))
		return __this_address;

	return NULL;
}

static inline int
xfs_rmap_complain_bad_rec(
	struct xfs_btree_cur		*cur,
	xfs_failaddr_t			fa,
	const struct xfs_rmap_irec	*irec)
{
	struct xfs_mount		*mp = cur->bc_mp;

	xfs_warn(mp,
		"Reverse Mapping BTree record corruption in AG %d detected at %pS!",
		cur->bc_ag.pag->pag_agno, fa);
	xfs_warn(mp,
		"Owner 0x%llx, flags 0x%x, start block 0x%x block count 0x%x",
		irec->rm_owner, irec->rm_flags, irec->rm_startblock,
		irec->rm_blockcount);
	return -EFSCORRUPTED;
}

 
int
xfs_rmap_get_rec(
	struct xfs_btree_cur	*cur,
	struct xfs_rmap_irec	*irec,
	int			*stat)
{
	union xfs_btree_rec	*rec;
	xfs_failaddr_t		fa;
	int			error;

	error = xfs_btree_get_rec(cur, &rec, stat);
	if (error || !*stat)
		return error;

	fa = xfs_rmap_btrec_to_irec(rec, irec);
	if (!fa)
		fa = xfs_rmap_check_irec(cur, irec);
	if (fa)
		return xfs_rmap_complain_bad_rec(cur, fa, irec);

	return 0;
}

struct xfs_find_left_neighbor_info {
	struct xfs_rmap_irec	high;
	struct xfs_rmap_irec	*irec;
};

 
STATIC int
xfs_rmap_find_left_neighbor_helper(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xfs_find_left_neighbor_info	*info = priv;

	trace_xfs_rmap_find_left_neighbor_candidate(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, rec->rm_startblock,
			rec->rm_blockcount, rec->rm_owner, rec->rm_offset,
			rec->rm_flags);

	if (rec->rm_owner != info->high.rm_owner)
		return 0;
	if (!XFS_RMAP_NON_INODE_OWNER(rec->rm_owner) &&
	    !(rec->rm_flags & XFS_RMAP_BMBT_BLOCK) &&
	    rec->rm_offset + rec->rm_blockcount - 1 != info->high.rm_offset)
		return 0;

	*info->irec = *rec;
	return -ECANCELED;
}

 
STATIC int
xfs_rmap_find_left_neighbor(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags,
	struct xfs_rmap_irec	*irec,
	int			*stat)
{
	struct xfs_find_left_neighbor_info	info;
	int			found = 0;
	int			error;

	*stat = 0;
	if (bno == 0)
		return 0;
	info.high.rm_startblock = bno - 1;
	info.high.rm_owner = owner;
	if (!XFS_RMAP_NON_INODE_OWNER(owner) &&
	    !(flags & XFS_RMAP_BMBT_BLOCK)) {
		if (offset == 0)
			return 0;
		info.high.rm_offset = offset - 1;
	} else
		info.high.rm_offset = 0;
	info.high.rm_flags = flags;
	info.high.rm_blockcount = 0;
	info.irec = irec;

	trace_xfs_rmap_find_left_neighbor_query(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, bno, 0, owner, offset, flags);

	 
	error = xfs_rmap_lookup_le(cur, bno, owner, offset, flags, irec,
			&found);
	if (error)
		return error;
	if (found)
		error = xfs_rmap_find_left_neighbor_helper(cur, irec, &info);
	if (!error)
		error = xfs_rmap_query_range(cur, &info.high, &info.high,
				xfs_rmap_find_left_neighbor_helper, &info);
	if (error != -ECANCELED)
		return error;

	*stat = 1;
	trace_xfs_rmap_find_left_neighbor_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, irec->rm_startblock,
			irec->rm_blockcount, irec->rm_owner, irec->rm_offset,
			irec->rm_flags);
	return 0;
}

 
STATIC int
xfs_rmap_lookup_le_range_helper(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xfs_find_left_neighbor_info	*info = priv;

	trace_xfs_rmap_lookup_le_range_candidate(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, rec->rm_startblock,
			rec->rm_blockcount, rec->rm_owner, rec->rm_offset,
			rec->rm_flags);

	if (rec->rm_owner != info->high.rm_owner)
		return 0;
	if (!XFS_RMAP_NON_INODE_OWNER(rec->rm_owner) &&
	    !(rec->rm_flags & XFS_RMAP_BMBT_BLOCK) &&
	    (rec->rm_offset > info->high.rm_offset ||
	     rec->rm_offset + rec->rm_blockcount <= info->high.rm_offset))
		return 0;

	*info->irec = *rec;
	return -ECANCELED;
}

 
int
xfs_rmap_lookup_le_range(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags,
	struct xfs_rmap_irec	*irec,
	int			*stat)
{
	struct xfs_find_left_neighbor_info	info;
	int			found = 0;
	int			error;

	info.high.rm_startblock = bno;
	info.high.rm_owner = owner;
	if (!XFS_RMAP_NON_INODE_OWNER(owner) && !(flags & XFS_RMAP_BMBT_BLOCK))
		info.high.rm_offset = offset;
	else
		info.high.rm_offset = 0;
	info.high.rm_flags = flags;
	info.high.rm_blockcount = 0;
	*stat = 0;
	info.irec = irec;

	trace_xfs_rmap_lookup_le_range(cur->bc_mp, cur->bc_ag.pag->pag_agno,
			bno, 0, owner, offset, flags);

	 
	error = xfs_rmap_lookup_le(cur, bno, owner, offset, flags, irec,
			&found);
	if (error)
		return error;
	if (found)
		error = xfs_rmap_lookup_le_range_helper(cur, irec, &info);
	if (!error)
		error = xfs_rmap_query_range(cur, &info.high, &info.high,
				xfs_rmap_lookup_le_range_helper, &info);
	if (error != -ECANCELED)
		return error;

	*stat = 1;
	trace_xfs_rmap_lookup_le_range_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, irec->rm_startblock,
			irec->rm_blockcount, irec->rm_owner, irec->rm_offset,
			irec->rm_flags);
	return 0;
}

 
static int
xfs_rmap_free_check_owner(
	struct xfs_mount	*mp,
	uint64_t		ltoff,
	struct xfs_rmap_irec	*rec,
	xfs_filblks_t		len,
	uint64_t		owner,
	uint64_t		offset,
	unsigned int		flags)
{
	int			error = 0;

	if (owner == XFS_RMAP_OWN_UNKNOWN)
		return 0;

	 
	if (XFS_IS_CORRUPT(mp,
			   (flags & XFS_RMAP_UNWRITTEN) !=
			   (rec->rm_flags & XFS_RMAP_UNWRITTEN))) {
		error = -EFSCORRUPTED;
		goto out;
	}

	 
	if (XFS_IS_CORRUPT(mp, owner != rec->rm_owner)) {
		error = -EFSCORRUPTED;
		goto out;
	}

	 
	if (XFS_RMAP_NON_INODE_OWNER(owner))
		goto out;

	if (flags & XFS_RMAP_BMBT_BLOCK) {
		if (XFS_IS_CORRUPT(mp,
				   !(rec->rm_flags & XFS_RMAP_BMBT_BLOCK))) {
			error = -EFSCORRUPTED;
			goto out;
		}
	} else {
		if (XFS_IS_CORRUPT(mp, rec->rm_offset > offset)) {
			error = -EFSCORRUPTED;
			goto out;
		}
		if (XFS_IS_CORRUPT(mp,
				   offset + len > ltoff + rec->rm_blockcount)) {
			error = -EFSCORRUPTED;
			goto out;
		}
	}

out:
	return error;
}

 
STATIC int
xfs_rmap_unmap(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	bool				unwritten,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_rmap_irec		ltrec;
	uint64_t			ltoff;
	int				error = 0;
	int				i;
	uint64_t			owner;
	uint64_t			offset;
	unsigned int			flags;
	bool				ignore_off;

	xfs_owner_info_unpack(oinfo, &owner, &offset, &flags);
	ignore_off = XFS_RMAP_NON_INODE_OWNER(owner) ||
			(flags & XFS_RMAP_BMBT_BLOCK);
	if (unwritten)
		flags |= XFS_RMAP_UNWRITTEN;
	trace_xfs_rmap_unmap(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);

	 
	error = xfs_rmap_lookup_le(cur, bno, owner, offset, flags, &ltrec, &i);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	trace_xfs_rmap_lookup_le_range_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, ltrec.rm_startblock,
			ltrec.rm_blockcount, ltrec.rm_owner,
			ltrec.rm_offset, ltrec.rm_flags);
	ltoff = ltrec.rm_offset;

	 
	if (owner == XFS_RMAP_OWN_NULL) {
		if (XFS_IS_CORRUPT(mp,
				   bno <
				   ltrec.rm_startblock + ltrec.rm_blockcount)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		goto out_done;
	}

	 
	if (owner == XFS_RMAP_OWN_UNKNOWN &&
	    ltrec.rm_startblock + ltrec.rm_blockcount <= bno) {
		struct xfs_rmap_irec    rtrec;

		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto out_error;
		if (i == 0)
			goto out_done;
		error = xfs_rmap_get_rec(cur, &rtrec, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		if (rtrec.rm_startblock >= bno + len)
			goto out_done;
	}

	 
	if (XFS_IS_CORRUPT(mp,
			   ltrec.rm_startblock > bno ||
			   ltrec.rm_startblock + ltrec.rm_blockcount <
			   bno + len)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	 
	error = xfs_rmap_free_check_owner(mp, ltoff, &ltrec, len, owner,
			offset, flags);
	if (error)
		goto out_error;

	if (ltrec.rm_startblock == bno && ltrec.rm_blockcount == len) {
		 
		trace_xfs_rmap_delete(mp, cur->bc_ag.pag->pag_agno,
				ltrec.rm_startblock, ltrec.rm_blockcount,
				ltrec.rm_owner, ltrec.rm_offset,
				ltrec.rm_flags);
		error = xfs_btree_delete(cur, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
	} else if (ltrec.rm_startblock == bno) {
		 
		ltrec.rm_startblock += len;
		ltrec.rm_blockcount -= len;
		if (!ignore_off)
			ltrec.rm_offset += len;
		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;
	} else if (ltrec.rm_startblock + ltrec.rm_blockcount == bno + len) {
		 
		ltrec.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;
	} else {

		 
		xfs_extlen_t	orig_len = ltrec.rm_blockcount;

		ltrec.rm_blockcount = bno - ltrec.rm_startblock;
		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;

		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto out_error;

		cur->bc_rec.r.rm_startblock = bno + len;
		cur->bc_rec.r.rm_blockcount = orig_len - len -
						     ltrec.rm_blockcount;
		cur->bc_rec.r.rm_owner = ltrec.rm_owner;
		if (ignore_off)
			cur->bc_rec.r.rm_offset = 0;
		else
			cur->bc_rec.r.rm_offset = offset + len;
		cur->bc_rec.r.rm_flags = flags;
		trace_xfs_rmap_insert(mp, cur->bc_ag.pag->pag_agno,
				cur->bc_rec.r.rm_startblock,
				cur->bc_rec.r.rm_blockcount,
				cur->bc_rec.r.rm_owner,
				cur->bc_rec.r.rm_offset,
				cur->bc_rec.r.rm_flags);
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto out_error;
	}

out_done:
	trace_xfs_rmap_unmap_done(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
out_error:
	if (error)
		trace_xfs_rmap_unmap_error(mp, cur->bc_ag.pag->pag_agno,
				error, _RET_IP_);
	return error;
}

 
int
xfs_rmap_free(
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	struct xfs_perag		*pag,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_btree_cur		*cur;
	int				error;

	if (!xfs_has_rmapbt(mp))
		return 0;

	cur = xfs_rmapbt_init_cursor(mp, tp, agbp, pag);

	error = xfs_rmap_unmap(cur, bno, len, false, oinfo);

	xfs_btree_del_cursor(cur, error);
	return error;
}

 
static bool
xfs_rmap_is_mergeable(
	struct xfs_rmap_irec	*irec,
	uint64_t		owner,
	unsigned int		flags)
{
	if (irec->rm_owner == XFS_RMAP_OWN_NULL)
		return false;
	if (irec->rm_owner != owner)
		return false;
	if ((flags & XFS_RMAP_UNWRITTEN) ^
	    (irec->rm_flags & XFS_RMAP_UNWRITTEN))
		return false;
	if ((flags & XFS_RMAP_ATTR_FORK) ^
	    (irec->rm_flags & XFS_RMAP_ATTR_FORK))
		return false;
	if ((flags & XFS_RMAP_BMBT_BLOCK) ^
	    (irec->rm_flags & XFS_RMAP_BMBT_BLOCK))
		return false;
	return true;
}

 
STATIC int
xfs_rmap_map(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	bool				unwritten,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_rmap_irec		ltrec;
	struct xfs_rmap_irec		gtrec;
	int				have_gt;
	int				have_lt;
	int				error = 0;
	int				i;
	uint64_t			owner;
	uint64_t			offset;
	unsigned int			flags = 0;
	bool				ignore_off;

	xfs_owner_info_unpack(oinfo, &owner, &offset, &flags);
	ASSERT(owner != 0);
	ignore_off = XFS_RMAP_NON_INODE_OWNER(owner) ||
			(flags & XFS_RMAP_BMBT_BLOCK);
	if (unwritten)
		flags |= XFS_RMAP_UNWRITTEN;
	trace_xfs_rmap_map(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
	ASSERT(!xfs_rmap_should_skip_owner_update(oinfo));

	 
	error = xfs_rmap_lookup_le(cur, bno, owner, offset, flags, &ltrec,
			&have_lt);
	if (error)
		goto out_error;
	if (have_lt) {
		trace_xfs_rmap_lookup_le_range_result(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags);

		if (!xfs_rmap_is_mergeable(&ltrec, owner, flags))
			have_lt = 0;
	}

	if (XFS_IS_CORRUPT(mp,
			   have_lt != 0 &&
			   ltrec.rm_startblock + ltrec.rm_blockcount > bno)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	 
	error = xfs_btree_increment(cur, 0, &have_gt);
	if (error)
		goto out_error;
	if (have_gt) {
		error = xfs_rmap_get_rec(cur, &gtrec, &have_gt);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, have_gt != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		if (XFS_IS_CORRUPT(mp, bno + len > gtrec.rm_startblock)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		trace_xfs_rmap_find_right_neighbor_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, gtrec.rm_startblock,
			gtrec.rm_blockcount, gtrec.rm_owner,
			gtrec.rm_offset, gtrec.rm_flags);
		if (!xfs_rmap_is_mergeable(&gtrec, owner, flags))
			have_gt = 0;
	}

	 
	if (have_lt &&
	    ltrec.rm_startblock + ltrec.rm_blockcount == bno &&
	    (ignore_off || ltrec.rm_offset + ltrec.rm_blockcount == offset)) {
		 
		ltrec.rm_blockcount += len;
		if (have_gt &&
		    bno + len == gtrec.rm_startblock &&
		    (ignore_off || offset + len == gtrec.rm_offset) &&
		    (unsigned long)ltrec.rm_blockcount + len +
				gtrec.rm_blockcount <= XFS_RMAP_LEN_MAX) {
			 
			ltrec.rm_blockcount += gtrec.rm_blockcount;
			trace_xfs_rmap_delete(mp, cur->bc_ag.pag->pag_agno,
					gtrec.rm_startblock,
					gtrec.rm_blockcount,
					gtrec.rm_owner,
					gtrec.rm_offset,
					gtrec.rm_flags);
			error = xfs_btree_delete(cur, &i);
			if (error)
				goto out_error;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto out_error;
			}
		}

		 
		error = xfs_btree_decrement(cur, 0, &have_gt);
		if (error)
			goto out_error;
		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;
	} else if (have_gt &&
		   bno + len == gtrec.rm_startblock &&
		   (ignore_off || offset + len == gtrec.rm_offset)) {
		 
		gtrec.rm_startblock = bno;
		gtrec.rm_blockcount += len;
		if (!ignore_off)
			gtrec.rm_offset = offset;
		error = xfs_rmap_update(cur, &gtrec);
		if (error)
			goto out_error;
	} else {
		 
		cur->bc_rec.r.rm_startblock = bno;
		cur->bc_rec.r.rm_blockcount = len;
		cur->bc_rec.r.rm_owner = owner;
		cur->bc_rec.r.rm_offset = offset;
		cur->bc_rec.r.rm_flags = flags;
		trace_xfs_rmap_insert(mp, cur->bc_ag.pag->pag_agno, bno, len,
			owner, offset, flags);
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
	}

	trace_xfs_rmap_map_done(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
out_error:
	if (error)
		trace_xfs_rmap_map_error(mp, cur->bc_ag.pag->pag_agno,
				error, _RET_IP_);
	return error;
}

 
int
xfs_rmap_alloc(
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	struct xfs_perag		*pag,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_btree_cur		*cur;
	int				error;

	if (!xfs_has_rmapbt(mp))
		return 0;

	cur = xfs_rmapbt_init_cursor(mp, tp, agbp, pag);
	error = xfs_rmap_map(cur, bno, len, false, oinfo);

	xfs_btree_del_cursor(cur, error);
	return error;
}

#define RMAP_LEFT_CONTIG	(1 << 0)
#define RMAP_RIGHT_CONTIG	(1 << 1)
#define RMAP_LEFT_FILLING	(1 << 2)
#define RMAP_RIGHT_FILLING	(1 << 3)
#define RMAP_LEFT_VALID		(1 << 6)
#define RMAP_RIGHT_VALID	(1 << 7)

#define LEFT		r[0]
#define RIGHT		r[1]
#define PREV		r[2]
#define NEW		r[3]

 
STATIC int
xfs_rmap_convert(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	bool				unwritten,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_rmap_irec		r[4];	 
						 
						 
	uint64_t		owner;
	uint64_t		offset;
	uint64_t		new_endoff;
	unsigned int		oldext;
	unsigned int		newext;
	unsigned int		flags = 0;
	int			i;
	int			state = 0;
	int			error;

	xfs_owner_info_unpack(oinfo, &owner, &offset, &flags);
	ASSERT(!(XFS_RMAP_NON_INODE_OWNER(owner) ||
			(flags & (XFS_RMAP_ATTR_FORK | XFS_RMAP_BMBT_BLOCK))));
	oldext = unwritten ? XFS_RMAP_UNWRITTEN : 0;
	new_endoff = offset + len;
	trace_xfs_rmap_convert(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);

	 
	error = xfs_rmap_lookup_le(cur, bno, owner, offset, oldext, &PREV, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}

	trace_xfs_rmap_lookup_le_range_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, PREV.rm_startblock,
			PREV.rm_blockcount, PREV.rm_owner,
			PREV.rm_offset, PREV.rm_flags);

	ASSERT(PREV.rm_offset <= offset);
	ASSERT(PREV.rm_offset + PREV.rm_blockcount >= new_endoff);
	ASSERT((PREV.rm_flags & XFS_RMAP_UNWRITTEN) == oldext);
	newext = ~oldext & XFS_RMAP_UNWRITTEN;

	 
	if (PREV.rm_offset == offset)
		state |= RMAP_LEFT_FILLING;
	if (PREV.rm_offset + PREV.rm_blockcount == new_endoff)
		state |= RMAP_RIGHT_FILLING;

	 
	error = xfs_btree_decrement(cur, 0, &i);
	if (error)
		goto done;
	if (i) {
		state |= RMAP_LEFT_VALID;
		error = xfs_rmap_get_rec(cur, &LEFT, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		if (XFS_IS_CORRUPT(mp,
				   LEFT.rm_startblock + LEFT.rm_blockcount >
				   bno)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		trace_xfs_rmap_find_left_neighbor_result(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, LEFT.rm_startblock,
				LEFT.rm_blockcount, LEFT.rm_owner,
				LEFT.rm_offset, LEFT.rm_flags);
		if (LEFT.rm_startblock + LEFT.rm_blockcount == bno &&
		    LEFT.rm_offset + LEFT.rm_blockcount == offset &&
		    xfs_rmap_is_mergeable(&LEFT, owner, newext))
			state |= RMAP_LEFT_CONTIG;
	}

	 
	error = xfs_btree_increment(cur, 0, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}
	error = xfs_btree_increment(cur, 0, &i);
	if (error)
		goto done;
	if (i) {
		state |= RMAP_RIGHT_VALID;
		error = xfs_rmap_get_rec(cur, &RIGHT, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		if (XFS_IS_CORRUPT(mp, bno + len > RIGHT.rm_startblock)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		trace_xfs_rmap_find_right_neighbor_result(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, RIGHT.rm_startblock,
				RIGHT.rm_blockcount, RIGHT.rm_owner,
				RIGHT.rm_offset, RIGHT.rm_flags);
		if (bno + len == RIGHT.rm_startblock &&
		    offset + len == RIGHT.rm_offset &&
		    xfs_rmap_is_mergeable(&RIGHT, owner, newext))
			state |= RMAP_RIGHT_CONTIG;
	}

	 
	if ((state & (RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
			 RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG)) ==
	    (RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
	     RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG) &&
	    (unsigned long)LEFT.rm_blockcount + len +
	     RIGHT.rm_blockcount > XFS_RMAP_LEN_MAX)
		state &= ~RMAP_RIGHT_CONTIG;

	trace_xfs_rmap_convert_state(mp, cur->bc_ag.pag->pag_agno, state,
			_RET_IP_);

	 
	error = xfs_rmap_lookup_le(cur, bno, owner, offset, oldext, NULL, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}

	 
	switch (state & (RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
			 RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG)) {
	case RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
	     RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG:
		 
		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		trace_xfs_rmap_delete(mp, cur->bc_ag.pag->pag_agno,
				RIGHT.rm_startblock, RIGHT.rm_blockcount,
				RIGHT.rm_owner, RIGHT.rm_offset,
				RIGHT.rm_flags);
		error = xfs_btree_delete(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		trace_xfs_rmap_delete(mp, cur->bc_ag.pag->pag_agno,
				PREV.rm_startblock, PREV.rm_blockcount,
				PREV.rm_owner, PREV.rm_offset,
				PREV.rm_flags);
		error = xfs_btree_delete(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW = LEFT;
		NEW.rm_blockcount += PREV.rm_blockcount + RIGHT.rm_blockcount;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_RIGHT_FILLING | RMAP_LEFT_CONTIG:
		 
		trace_xfs_rmap_delete(mp, cur->bc_ag.pag->pag_agno,
				PREV.rm_startblock, PREV.rm_blockcount,
				PREV.rm_owner, PREV.rm_offset,
				PREV.rm_flags);
		error = xfs_btree_delete(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW = LEFT;
		NEW.rm_blockcount += PREV.rm_blockcount;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG:
		 
		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		trace_xfs_rmap_delete(mp, cur->bc_ag.pag->pag_agno,
				RIGHT.rm_startblock, RIGHT.rm_blockcount,
				RIGHT.rm_owner, RIGHT.rm_offset,
				RIGHT.rm_flags);
		error = xfs_btree_delete(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW = PREV;
		NEW.rm_blockcount = len + RIGHT.rm_blockcount;
		NEW.rm_flags = newext;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_RIGHT_FILLING:
		 
		NEW = PREV;
		NEW.rm_flags = newext;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG:
		 
		NEW = PREV;
		NEW.rm_offset += len;
		NEW.rm_startblock += len;
		NEW.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		error = xfs_btree_decrement(cur, 0, &i);
		if (error)
			goto done;
		NEW = LEFT;
		NEW.rm_blockcount += len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING:
		 
		NEW = PREV;
		NEW.rm_startblock += len;
		NEW.rm_offset += len;
		NEW.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		NEW.rm_startblock = bno;
		NEW.rm_owner = owner;
		NEW.rm_offset = offset;
		NEW.rm_blockcount = len;
		NEW.rm_flags = newext;
		cur->bc_rec.r = NEW;
		trace_xfs_rmap_insert(mp, cur->bc_ag.pag->pag_agno, bno,
				len, owner, offset, newext);
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		break;

	case RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG:
		 
		NEW = PREV;
		NEW.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto done;
		NEW = RIGHT;
		NEW.rm_offset = offset;
		NEW.rm_startblock = bno;
		NEW.rm_blockcount += len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_RIGHT_FILLING:
		 
		NEW = PREV;
		NEW.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		error = xfs_rmap_lookup_eq(cur, bno, len, owner, offset,
				oldext, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 0)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_startblock = bno;
		NEW.rm_owner = owner;
		NEW.rm_offset = offset;
		NEW.rm_blockcount = len;
		NEW.rm_flags = newext;
		cur->bc_rec.r = NEW;
		trace_xfs_rmap_insert(mp, cur->bc_ag.pag->pag_agno, bno,
				len, owner, offset, newext);
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		break;

	case 0:
		 
		 
		NEW.rm_startblock = bno + len;
		NEW.rm_owner = owner;
		NEW.rm_offset = new_endoff;
		NEW.rm_blockcount = PREV.rm_offset + PREV.rm_blockcount -
				new_endoff;
		NEW.rm_flags = PREV.rm_flags;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		 
		NEW = PREV;
		NEW.rm_blockcount = offset - PREV.rm_offset;
		cur->bc_rec.r = NEW;
		trace_xfs_rmap_insert(mp, cur->bc_ag.pag->pag_agno,
				NEW.rm_startblock, NEW.rm_blockcount,
				NEW.rm_owner, NEW.rm_offset,
				NEW.rm_flags);
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		 
		error = xfs_rmap_lookup_eq(cur, bno, len, owner, offset,
				oldext, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 0)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		 
		cur->bc_rec.r.rm_flags &= ~XFS_RMAP_UNWRITTEN;
		cur->bc_rec.r.rm_flags |= newext;
		trace_xfs_rmap_insert(mp, cur->bc_ag.pag->pag_agno, bno, len,
				owner, offset, newext);
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		break;

	case RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG | RMAP_RIGHT_CONTIG:
	case RMAP_RIGHT_FILLING | RMAP_LEFT_CONTIG | RMAP_RIGHT_CONTIG:
	case RMAP_LEFT_FILLING | RMAP_RIGHT_CONTIG:
	case RMAP_RIGHT_FILLING | RMAP_LEFT_CONTIG:
	case RMAP_LEFT_CONTIG | RMAP_RIGHT_CONTIG:
	case RMAP_LEFT_CONTIG:
	case RMAP_RIGHT_CONTIG:
		 
		ASSERT(0);
	}

	trace_xfs_rmap_convert_done(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
done:
	if (error)
		trace_xfs_rmap_convert_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_rmap_convert_shared(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	bool				unwritten,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_rmap_irec		r[4];	 
						 
						 
	uint64_t		owner;
	uint64_t		offset;
	uint64_t		new_endoff;
	unsigned int		oldext;
	unsigned int		newext;
	unsigned int		flags = 0;
	int			i;
	int			state = 0;
	int			error;

	xfs_owner_info_unpack(oinfo, &owner, &offset, &flags);
	ASSERT(!(XFS_RMAP_NON_INODE_OWNER(owner) ||
			(flags & (XFS_RMAP_ATTR_FORK | XFS_RMAP_BMBT_BLOCK))));
	oldext = unwritten ? XFS_RMAP_UNWRITTEN : 0;
	new_endoff = offset + len;
	trace_xfs_rmap_convert(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);

	 
	error = xfs_rmap_lookup_le_range(cur, bno, owner, offset, oldext,
			&PREV, &i);
	if (error)
		goto done;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto done;
	}

	ASSERT(PREV.rm_offset <= offset);
	ASSERT(PREV.rm_offset + PREV.rm_blockcount >= new_endoff);
	ASSERT((PREV.rm_flags & XFS_RMAP_UNWRITTEN) == oldext);
	newext = ~oldext & XFS_RMAP_UNWRITTEN;

	 
	if (PREV.rm_offset == offset)
		state |= RMAP_LEFT_FILLING;
	if (PREV.rm_offset + PREV.rm_blockcount == new_endoff)
		state |= RMAP_RIGHT_FILLING;

	 
	error = xfs_rmap_find_left_neighbor(cur, bno, owner, offset, newext,
			&LEFT, &i);
	if (error)
		goto done;
	if (i) {
		state |= RMAP_LEFT_VALID;
		if (XFS_IS_CORRUPT(mp,
				   LEFT.rm_startblock + LEFT.rm_blockcount >
				   bno)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		if (xfs_rmap_is_mergeable(&LEFT, owner, newext))
			state |= RMAP_LEFT_CONTIG;
	}

	 
	error = xfs_rmap_lookup_eq(cur, bno + len, len, owner, offset + len,
			newext, &i);
	if (error)
		goto done;
	if (i) {
		state |= RMAP_RIGHT_VALID;
		error = xfs_rmap_get_rec(cur, &RIGHT, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		if (XFS_IS_CORRUPT(mp, bno + len > RIGHT.rm_startblock)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		trace_xfs_rmap_find_right_neighbor_result(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, RIGHT.rm_startblock,
				RIGHT.rm_blockcount, RIGHT.rm_owner,
				RIGHT.rm_offset, RIGHT.rm_flags);
		if (xfs_rmap_is_mergeable(&RIGHT, owner, newext))
			state |= RMAP_RIGHT_CONTIG;
	}

	 
	if ((state & (RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
			 RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG)) ==
	    (RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
	     RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG) &&
	    (unsigned long)LEFT.rm_blockcount + len +
	     RIGHT.rm_blockcount > XFS_RMAP_LEN_MAX)
		state &= ~RMAP_RIGHT_CONTIG;

	trace_xfs_rmap_convert_state(mp, cur->bc_ag.pag->pag_agno, state,
			_RET_IP_);
	 
	switch (state & (RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
			 RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG)) {
	case RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG |
	     RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG:
		 
		error = xfs_rmap_delete(cur, RIGHT.rm_startblock,
				RIGHT.rm_blockcount, RIGHT.rm_owner,
				RIGHT.rm_offset, RIGHT.rm_flags);
		if (error)
			goto done;
		error = xfs_rmap_delete(cur, PREV.rm_startblock,
				PREV.rm_blockcount, PREV.rm_owner,
				PREV.rm_offset, PREV.rm_flags);
		if (error)
			goto done;
		NEW = LEFT;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount += PREV.rm_blockcount + RIGHT.rm_blockcount;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_RIGHT_FILLING | RMAP_LEFT_CONTIG:
		 
		error = xfs_rmap_delete(cur, PREV.rm_startblock,
				PREV.rm_blockcount, PREV.rm_owner,
				PREV.rm_offset, PREV.rm_flags);
		if (error)
			goto done;
		NEW = LEFT;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount += PREV.rm_blockcount;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG:
		 
		error = xfs_rmap_delete(cur, RIGHT.rm_startblock,
				RIGHT.rm_blockcount, RIGHT.rm_owner,
				RIGHT.rm_offset, RIGHT.rm_flags);
		if (error)
			goto done;
		NEW = PREV;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount += RIGHT.rm_blockcount;
		NEW.rm_flags = RIGHT.rm_flags;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_RIGHT_FILLING:
		 
		NEW = PREV;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_flags = newext;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG:
		 
		NEW = PREV;
		error = xfs_rmap_delete(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags);
		if (error)
			goto done;
		NEW.rm_offset += len;
		NEW.rm_startblock += len;
		NEW.rm_blockcount -= len;
		error = xfs_rmap_insert(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags);
		if (error)
			goto done;
		NEW = LEFT;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount += len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING:
		 
		NEW = PREV;
		error = xfs_rmap_delete(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags);
		if (error)
			goto done;
		NEW.rm_offset += len;
		NEW.rm_startblock += len;
		NEW.rm_blockcount -= len;
		error = xfs_rmap_insert(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags);
		if (error)
			goto done;
		error = xfs_rmap_insert(cur, bno, len, owner, offset, newext);
		if (error)
			goto done;
		break;

	case RMAP_RIGHT_FILLING | RMAP_RIGHT_CONTIG:
		 
		NEW = PREV;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount = offset - NEW.rm_offset;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		NEW = RIGHT;
		error = xfs_rmap_delete(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags);
		if (error)
			goto done;
		NEW.rm_offset = offset;
		NEW.rm_startblock = bno;
		NEW.rm_blockcount += len;
		error = xfs_rmap_insert(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags);
		if (error)
			goto done;
		break;

	case RMAP_RIGHT_FILLING:
		 
		NEW = PREV;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		error = xfs_rmap_insert(cur, bno, len, owner, offset, newext);
		if (error)
			goto done;
		break;

	case 0:
		 
		 
		NEW.rm_startblock = bno + len;
		NEW.rm_owner = owner;
		NEW.rm_offset = new_endoff;
		NEW.rm_blockcount = PREV.rm_offset + PREV.rm_blockcount -
				new_endoff;
		NEW.rm_flags = PREV.rm_flags;
		error = xfs_rmap_insert(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner, NEW.rm_offset,
				NEW.rm_flags);
		if (error)
			goto done;
		 
		NEW = PREV;
		error = xfs_rmap_lookup_eq(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner,
				NEW.rm_offset, NEW.rm_flags, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		NEW.rm_blockcount = offset - NEW.rm_offset;
		error = xfs_rmap_update(cur, &NEW);
		if (error)
			goto done;
		 
		NEW.rm_startblock = bno;
		NEW.rm_blockcount = len;
		NEW.rm_owner = owner;
		NEW.rm_offset = offset;
		NEW.rm_flags = newext;
		error = xfs_rmap_insert(cur, NEW.rm_startblock,
				NEW.rm_blockcount, NEW.rm_owner, NEW.rm_offset,
				NEW.rm_flags);
		if (error)
			goto done;
		break;

	case RMAP_LEFT_FILLING | RMAP_LEFT_CONTIG | RMAP_RIGHT_CONTIG:
	case RMAP_RIGHT_FILLING | RMAP_LEFT_CONTIG | RMAP_RIGHT_CONTIG:
	case RMAP_LEFT_FILLING | RMAP_RIGHT_CONTIG:
	case RMAP_RIGHT_FILLING | RMAP_LEFT_CONTIG:
	case RMAP_LEFT_CONTIG | RMAP_RIGHT_CONTIG:
	case RMAP_LEFT_CONTIG:
	case RMAP_RIGHT_CONTIG:
		 
		ASSERT(0);
	}

	trace_xfs_rmap_convert_done(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
done:
	if (error)
		trace_xfs_rmap_convert_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

#undef	NEW
#undef	LEFT
#undef	RIGHT
#undef	PREV

 
STATIC int
xfs_rmap_unmap_shared(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	bool				unwritten,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_rmap_irec		ltrec;
	uint64_t			ltoff;
	int				error = 0;
	int				i;
	uint64_t			owner;
	uint64_t			offset;
	unsigned int			flags;

	xfs_owner_info_unpack(oinfo, &owner, &offset, &flags);
	if (unwritten)
		flags |= XFS_RMAP_UNWRITTEN;
	trace_xfs_rmap_unmap(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);

	 
	error = xfs_rmap_lookup_le_range(cur, bno, owner, offset, flags,
			&ltrec, &i);
	if (error)
		goto out_error;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	ltoff = ltrec.rm_offset;

	 
	if (XFS_IS_CORRUPT(mp,
			   ltrec.rm_startblock > bno ||
			   ltrec.rm_startblock + ltrec.rm_blockcount <
			   bno + len)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	 
	if (XFS_IS_CORRUPT(mp, owner != ltrec.rm_owner)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	 
	if (XFS_IS_CORRUPT(mp,
			   (flags & XFS_RMAP_UNWRITTEN) !=
			   (ltrec.rm_flags & XFS_RMAP_UNWRITTEN))) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	 
	if (XFS_IS_CORRUPT(mp, ltrec.rm_offset > offset)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}
	if (XFS_IS_CORRUPT(mp, offset > ltoff + ltrec.rm_blockcount)) {
		error = -EFSCORRUPTED;
		goto out_error;
	}

	if (ltrec.rm_startblock == bno && ltrec.rm_blockcount == len) {
		 
		error = xfs_rmap_delete(cur, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags);
		if (error)
			goto out_error;
	} else if (ltrec.rm_startblock == bno) {
		 

		 
		error = xfs_rmap_delete(cur, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags);
		if (error)
			goto out_error;

		 
		ltrec.rm_startblock += len;
		ltrec.rm_blockcount -= len;
		ltrec.rm_offset += len;
		error = xfs_rmap_insert(cur, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags);
		if (error)
			goto out_error;
	} else if (ltrec.rm_startblock + ltrec.rm_blockcount == bno + len) {
		 
		error = xfs_rmap_lookup_eq(cur, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		ltrec.rm_blockcount -= len;
		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;
	} else {
		 
		xfs_extlen_t	orig_len = ltrec.rm_blockcount;

		 
		error = xfs_rmap_lookup_eq(cur, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		ltrec.rm_blockcount = bno - ltrec.rm_startblock;
		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;

		 
		error = xfs_rmap_insert(cur, bno + len,
				orig_len - len - ltrec.rm_blockcount,
				ltrec.rm_owner, offset + len,
				ltrec.rm_flags);
		if (error)
			goto out_error;
	}

	trace_xfs_rmap_unmap_done(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
out_error:
	if (error)
		trace_xfs_rmap_unmap_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
STATIC int
xfs_rmap_map_shared(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	bool				unwritten,
	const struct xfs_owner_info	*oinfo)
{
	struct xfs_mount		*mp = cur->bc_mp;
	struct xfs_rmap_irec		ltrec;
	struct xfs_rmap_irec		gtrec;
	int				have_gt;
	int				have_lt;
	int				error = 0;
	int				i;
	uint64_t			owner;
	uint64_t			offset;
	unsigned int			flags = 0;

	xfs_owner_info_unpack(oinfo, &owner, &offset, &flags);
	if (unwritten)
		flags |= XFS_RMAP_UNWRITTEN;
	trace_xfs_rmap_map(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);

	 
	error = xfs_rmap_find_left_neighbor(cur, bno, owner, offset, flags,
			&ltrec, &have_lt);
	if (error)
		goto out_error;
	if (have_lt &&
	    !xfs_rmap_is_mergeable(&ltrec, owner, flags))
		have_lt = 0;

	 
	error = xfs_rmap_lookup_eq(cur, bno + len, len, owner, offset + len,
			flags, &have_gt);
	if (error)
		goto out_error;
	if (have_gt) {
		error = xfs_rmap_get_rec(cur, &gtrec, &have_gt);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, have_gt != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}
		trace_xfs_rmap_find_right_neighbor_result(cur->bc_mp,
			cur->bc_ag.pag->pag_agno, gtrec.rm_startblock,
			gtrec.rm_blockcount, gtrec.rm_owner,
			gtrec.rm_offset, gtrec.rm_flags);

		if (!xfs_rmap_is_mergeable(&gtrec, owner, flags))
			have_gt = 0;
	}

	if (have_lt &&
	    ltrec.rm_startblock + ltrec.rm_blockcount == bno &&
	    ltrec.rm_offset + ltrec.rm_blockcount == offset) {
		 
		ltrec.rm_blockcount += len;
		if (have_gt &&
		    bno + len == gtrec.rm_startblock &&
		    offset + len == gtrec.rm_offset) {
			 
			ltrec.rm_blockcount += gtrec.rm_blockcount;
			error = xfs_rmap_delete(cur, gtrec.rm_startblock,
					gtrec.rm_blockcount, gtrec.rm_owner,
					gtrec.rm_offset, gtrec.rm_flags);
			if (error)
				goto out_error;
		}

		 
		error = xfs_rmap_lookup_eq(cur, ltrec.rm_startblock,
				ltrec.rm_blockcount, ltrec.rm_owner,
				ltrec.rm_offset, ltrec.rm_flags, &i);
		if (error)
			goto out_error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto out_error;
		}

		error = xfs_rmap_update(cur, &ltrec);
		if (error)
			goto out_error;
	} else if (have_gt &&
		   bno + len == gtrec.rm_startblock &&
		   offset + len == gtrec.rm_offset) {
		 
		 
		error = xfs_rmap_delete(cur, gtrec.rm_startblock,
				gtrec.rm_blockcount, gtrec.rm_owner,
				gtrec.rm_offset, gtrec.rm_flags);
		if (error)
			goto out_error;

		 
		gtrec.rm_startblock = bno;
		gtrec.rm_blockcount += len;
		gtrec.rm_offset = offset;
		error = xfs_rmap_insert(cur, gtrec.rm_startblock,
				gtrec.rm_blockcount, gtrec.rm_owner,
				gtrec.rm_offset, gtrec.rm_flags);
		if (error)
			goto out_error;
	} else {
		 
		error = xfs_rmap_insert(cur, bno, len, owner, offset, flags);
		if (error)
			goto out_error;
	}

	trace_xfs_rmap_map_done(mp, cur->bc_ag.pag->pag_agno, bno, len,
			unwritten, oinfo);
out_error:
	if (error)
		trace_xfs_rmap_map_error(cur->bc_mp,
				cur->bc_ag.pag->pag_agno, error, _RET_IP_);
	return error;
}

 
int
xfs_rmap_map_raw(
	struct xfs_btree_cur	*cur,
	struct xfs_rmap_irec	*rmap)
{
	struct xfs_owner_info	oinfo;

	oinfo.oi_owner = rmap->rm_owner;
	oinfo.oi_offset = rmap->rm_offset;
	oinfo.oi_flags = 0;
	if (rmap->rm_flags & XFS_RMAP_ATTR_FORK)
		oinfo.oi_flags |= XFS_OWNER_INFO_ATTR_FORK;
	if (rmap->rm_flags & XFS_RMAP_BMBT_BLOCK)
		oinfo.oi_flags |= XFS_OWNER_INFO_BMBT_BLOCK;

	if (rmap->rm_flags || XFS_RMAP_NON_INODE_OWNER(rmap->rm_owner))
		return xfs_rmap_map(cur, rmap->rm_startblock,
				rmap->rm_blockcount,
				rmap->rm_flags & XFS_RMAP_UNWRITTEN,
				&oinfo);

	return xfs_rmap_map_shared(cur, rmap->rm_startblock,
			rmap->rm_blockcount,
			rmap->rm_flags & XFS_RMAP_UNWRITTEN,
			&oinfo);
}

struct xfs_rmap_query_range_info {
	xfs_rmap_query_range_fn	fn;
	void				*priv;
};

 
STATIC int
xfs_rmap_query_range_helper(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_rec	*rec,
	void				*priv)
{
	struct xfs_rmap_query_range_info	*query = priv;
	struct xfs_rmap_irec			irec;
	xfs_failaddr_t				fa;

	fa = xfs_rmap_btrec_to_irec(rec, &irec);
	if (!fa)
		fa = xfs_rmap_check_irec(cur, &irec);
	if (fa)
		return xfs_rmap_complain_bad_rec(cur, fa, &irec);

	return query->fn(cur, &irec, query->priv);
}

 
int
xfs_rmap_query_range(
	struct xfs_btree_cur			*cur,
	const struct xfs_rmap_irec		*low_rec,
	const struct xfs_rmap_irec		*high_rec,
	xfs_rmap_query_range_fn			fn,
	void					*priv)
{
	union xfs_btree_irec			low_brec = { .r = *low_rec };
	union xfs_btree_irec			high_brec = { .r = *high_rec };
	struct xfs_rmap_query_range_info	query = { .priv = priv, .fn = fn };

	return xfs_btree_query_range(cur, &low_brec, &high_brec,
			xfs_rmap_query_range_helper, &query);
}

 
int
xfs_rmap_query_all(
	struct xfs_btree_cur			*cur,
	xfs_rmap_query_range_fn			fn,
	void					*priv)
{
	struct xfs_rmap_query_range_info	query;

	query.priv = priv;
	query.fn = fn;
	return xfs_btree_query_all(cur, xfs_rmap_query_range_helper, &query);
}

 
void
xfs_rmap_finish_one_cleanup(
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

 
int
xfs_rmap_finish_one(
	struct xfs_trans		*tp,
	struct xfs_rmap_intent		*ri,
	struct xfs_btree_cur		**pcur)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_btree_cur		*rcur;
	struct xfs_buf			*agbp = NULL;
	int				error = 0;
	struct xfs_owner_info		oinfo;
	xfs_agblock_t			bno;
	bool				unwritten;

	bno = XFS_FSB_TO_AGBNO(mp, ri->ri_bmap.br_startblock);

	trace_xfs_rmap_deferred(mp, ri->ri_pag->pag_agno, ri->ri_type, bno,
			ri->ri_owner, ri->ri_whichfork,
			ri->ri_bmap.br_startoff, ri->ri_bmap.br_blockcount,
			ri->ri_bmap.br_state);

	if (XFS_TEST_ERROR(false, mp, XFS_ERRTAG_RMAP_FINISH_ONE))
		return -EIO;

	 
	rcur = *pcur;
	if (rcur != NULL && rcur->bc_ag.pag != ri->ri_pag) {
		xfs_rmap_finish_one_cleanup(tp, rcur, 0);
		rcur = NULL;
		*pcur = NULL;
	}
	if (rcur == NULL) {
		 
		error = xfs_free_extent_fix_freelist(tp, ri->ri_pag, &agbp);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(tp->t_mountp, !agbp))
			return -EFSCORRUPTED;

		rcur = xfs_rmapbt_init_cursor(mp, tp, agbp, ri->ri_pag);
	}
	*pcur = rcur;

	xfs_rmap_ino_owner(&oinfo, ri->ri_owner, ri->ri_whichfork,
			ri->ri_bmap.br_startoff);
	unwritten = ri->ri_bmap.br_state == XFS_EXT_UNWRITTEN;
	bno = XFS_FSB_TO_AGBNO(rcur->bc_mp, ri->ri_bmap.br_startblock);

	switch (ri->ri_type) {
	case XFS_RMAP_ALLOC:
	case XFS_RMAP_MAP:
		error = xfs_rmap_map(rcur, bno, ri->ri_bmap.br_blockcount,
				unwritten, &oinfo);
		break;
	case XFS_RMAP_MAP_SHARED:
		error = xfs_rmap_map_shared(rcur, bno,
				ri->ri_bmap.br_blockcount, unwritten, &oinfo);
		break;
	case XFS_RMAP_FREE:
	case XFS_RMAP_UNMAP:
		error = xfs_rmap_unmap(rcur, bno, ri->ri_bmap.br_blockcount,
				unwritten, &oinfo);
		break;
	case XFS_RMAP_UNMAP_SHARED:
		error = xfs_rmap_unmap_shared(rcur, bno,
				ri->ri_bmap.br_blockcount, unwritten, &oinfo);
		break;
	case XFS_RMAP_CONVERT:
		error = xfs_rmap_convert(rcur, bno, ri->ri_bmap.br_blockcount,
				!unwritten, &oinfo);
		break;
	case XFS_RMAP_CONVERT_SHARED:
		error = xfs_rmap_convert_shared(rcur, bno,
				ri->ri_bmap.br_blockcount, !unwritten, &oinfo);
		break;
	default:
		ASSERT(0);
		error = -EFSCORRUPTED;
	}

	return error;
}

 
static bool
xfs_rmap_update_is_needed(
	struct xfs_mount	*mp,
	int			whichfork)
{
	return xfs_has_rmapbt(mp) && whichfork != XFS_COW_FORK;
}

 
static void
__xfs_rmap_add(
	struct xfs_trans		*tp,
	enum xfs_rmap_intent_type	type,
	uint64_t			owner,
	int				whichfork,
	struct xfs_bmbt_irec		*bmap)
{
	struct xfs_rmap_intent		*ri;

	trace_xfs_rmap_defer(tp->t_mountp,
			XFS_FSB_TO_AGNO(tp->t_mountp, bmap->br_startblock),
			type,
			XFS_FSB_TO_AGBNO(tp->t_mountp, bmap->br_startblock),
			owner, whichfork,
			bmap->br_startoff,
			bmap->br_blockcount,
			bmap->br_state);

	ri = kmem_cache_alloc(xfs_rmap_intent_cache, GFP_NOFS | __GFP_NOFAIL);
	INIT_LIST_HEAD(&ri->ri_list);
	ri->ri_type = type;
	ri->ri_owner = owner;
	ri->ri_whichfork = whichfork;
	ri->ri_bmap = *bmap;

	xfs_rmap_update_get_group(tp->t_mountp, ri);
	xfs_defer_add(tp, XFS_DEFER_OPS_TYPE_RMAP, &ri->ri_list);
}

 
void
xfs_rmap_map_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_bmbt_irec	*PREV)
{
	enum xfs_rmap_intent_type type = XFS_RMAP_MAP;

	if (!xfs_rmap_update_is_needed(tp->t_mountp, whichfork))
		return;

	if (whichfork != XFS_ATTR_FORK && xfs_is_reflink_inode(ip))
		type = XFS_RMAP_MAP_SHARED;

	__xfs_rmap_add(tp, type, ip->i_ino, whichfork, PREV);
}

 
void
xfs_rmap_unmap_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_bmbt_irec	*PREV)
{
	enum xfs_rmap_intent_type type = XFS_RMAP_UNMAP;

	if (!xfs_rmap_update_is_needed(tp->t_mountp, whichfork))
		return;

	if (whichfork != XFS_ATTR_FORK && xfs_is_reflink_inode(ip))
		type = XFS_RMAP_UNMAP_SHARED;

	__xfs_rmap_add(tp, type, ip->i_ino, whichfork, PREV);
}

 
void
xfs_rmap_convert_extent(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_bmbt_irec	*PREV)
{
	enum xfs_rmap_intent_type type = XFS_RMAP_CONVERT;

	if (!xfs_rmap_update_is_needed(mp, whichfork))
		return;

	if (whichfork != XFS_ATTR_FORK && xfs_is_reflink_inode(ip))
		type = XFS_RMAP_CONVERT_SHARED;

	__xfs_rmap_add(tp, type, ip->i_ino, whichfork, PREV);
}

 
void
xfs_rmap_alloc_extent(
	struct xfs_trans	*tp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	uint64_t		owner)
{
	struct xfs_bmbt_irec	bmap;

	if (!xfs_rmap_update_is_needed(tp->t_mountp, XFS_DATA_FORK))
		return;

	bmap.br_startblock = XFS_AGB_TO_FSB(tp->t_mountp, agno, bno);
	bmap.br_blockcount = len;
	bmap.br_startoff = 0;
	bmap.br_state = XFS_EXT_NORM;

	__xfs_rmap_add(tp, XFS_RMAP_ALLOC, owner, XFS_DATA_FORK, &bmap);
}

 
void
xfs_rmap_free_extent(
	struct xfs_trans	*tp,
	xfs_agnumber_t		agno,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	uint64_t		owner)
{
	struct xfs_bmbt_irec	bmap;

	if (!xfs_rmap_update_is_needed(tp->t_mountp, XFS_DATA_FORK))
		return;

	bmap.br_startblock = XFS_AGB_TO_FSB(tp->t_mountp, agno, bno);
	bmap.br_blockcount = len;
	bmap.br_startoff = 0;
	bmap.br_state = XFS_EXT_NORM;

	__xfs_rmap_add(tp, XFS_RMAP_FREE, owner, XFS_DATA_FORK, &bmap);
}

 
int
xfs_rmap_compare(
	const struct xfs_rmap_irec	*a,
	const struct xfs_rmap_irec	*b)
{
	__u64				oa;
	__u64				ob;

	oa = xfs_rmap_irec_offset_pack(a);
	ob = xfs_rmap_irec_offset_pack(b);

	if (a->rm_startblock < b->rm_startblock)
		return -1;
	else if (a->rm_startblock > b->rm_startblock)
		return 1;
	else if (a->rm_owner < b->rm_owner)
		return -1;
	else if (a->rm_owner > b->rm_owner)
		return 1;
	else if (oa < ob)
		return -1;
	else if (oa > ob)
		return 1;
	else
		return 0;
}

 
int
xfs_rmap_has_records(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	enum xbtree_recpacking	*outcome)
{
	union xfs_btree_key	mask = {
		.rmap.rm_startblock = cpu_to_be32(-1U),
	};
	union xfs_btree_irec	low;
	union xfs_btree_irec	high;

	memset(&low, 0, sizeof(low));
	low.r.rm_startblock = bno;
	memset(&high, 0xFF, sizeof(high));
	high.r.rm_startblock = bno + len - 1;

	return xfs_btree_has_records(cur, &low, &high, &mask, outcome);
}

struct xfs_rmap_ownercount {
	 
	struct xfs_rmap_irec	good;

	 
	struct xfs_rmap_irec	low;
	struct xfs_rmap_irec	high;

	struct xfs_rmap_matches	*results;

	 
	bool			stop_on_nonmatch;
};

 
static inline bool
xfs_rmap_shareable(
	struct xfs_mount		*mp,
	const struct xfs_rmap_irec	*rmap)
{
	if (!xfs_has_reflink(mp))
		return false;
	if (XFS_RMAP_NON_INODE_OWNER(rmap->rm_owner))
		return false;
	if (rmap->rm_flags & (XFS_RMAP_ATTR_FORK |
			      XFS_RMAP_BMBT_BLOCK))
		return false;
	return true;
}

static inline void
xfs_rmap_ownercount_init(
	struct xfs_rmap_ownercount	*roc,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo,
	struct xfs_rmap_matches		*results)
{
	memset(roc, 0, sizeof(*roc));
	roc->results = results;

	roc->low.rm_startblock = bno;
	memset(&roc->high, 0xFF, sizeof(roc->high));
	roc->high.rm_startblock = bno + len - 1;

	memset(results, 0, sizeof(*results));
	roc->good.rm_startblock = bno;
	roc->good.rm_blockcount = len;
	roc->good.rm_owner = oinfo->oi_owner;
	roc->good.rm_offset = oinfo->oi_offset;
	if (oinfo->oi_flags & XFS_OWNER_INFO_ATTR_FORK)
		roc->good.rm_flags |= XFS_RMAP_ATTR_FORK;
	if (oinfo->oi_flags & XFS_OWNER_INFO_BMBT_BLOCK)
		roc->good.rm_flags |= XFS_RMAP_BMBT_BLOCK;
}

 
STATIC int
xfs_rmap_count_owners_helper(
	struct xfs_btree_cur		*cur,
	const struct xfs_rmap_irec	*rec,
	void				*priv)
{
	struct xfs_rmap_ownercount	*roc = priv;
	struct xfs_rmap_irec		check = *rec;
	unsigned int			keyflags;
	bool				filedata;
	int64_t				delta;

	filedata = !XFS_RMAP_NON_INODE_OWNER(check.rm_owner) &&
		   !(check.rm_flags & XFS_RMAP_BMBT_BLOCK);

	 
	delta = (int64_t)roc->good.rm_startblock - check.rm_startblock;
	if (delta > 0) {
		check.rm_startblock += delta;
		check.rm_blockcount -= delta;
		if (filedata)
			check.rm_offset += delta;
	}

	 
	delta = (check.rm_startblock + check.rm_blockcount) -
		(roc->good.rm_startblock + roc->good.rm_blockcount);
	if (delta > 0)
		check.rm_blockcount -= delta;

	 
	keyflags = check.rm_flags & (XFS_RMAP_ATTR_FORK | XFS_RMAP_BMBT_BLOCK);

	if (check.rm_startblock	== roc->good.rm_startblock &&
	    check.rm_blockcount	== roc->good.rm_blockcount &&
	    check.rm_owner	== roc->good.rm_owner &&
	    check.rm_offset	== roc->good.rm_offset &&
	    keyflags		== roc->good.rm_flags) {
		roc->results->matches++;
	} else {
		roc->results->non_owner_matches++;
		if (xfs_rmap_shareable(cur->bc_mp, &roc->good) ^
		    xfs_rmap_shareable(cur->bc_mp, &check))
			roc->results->bad_non_owner_matches++;
	}

	if (roc->results->non_owner_matches && roc->stop_on_nonmatch)
		return -ECANCELED;

	return 0;
}

 
int
xfs_rmap_count_owners(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo,
	struct xfs_rmap_matches		*results)
{
	struct xfs_rmap_ownercount	roc;
	int				error;

	xfs_rmap_ownercount_init(&roc, bno, len, oinfo, results);
	error = xfs_rmap_query_range(cur, &roc.low, &roc.high,
			xfs_rmap_count_owners_helper, &roc);
	if (error)
		return error;

	 
	if (!results->matches)
		results->bad_non_owner_matches = 0;

	return 0;
}

 
int
xfs_rmap_has_other_keys(
	struct xfs_btree_cur		*cur,
	xfs_agblock_t			bno,
	xfs_extlen_t			len,
	const struct xfs_owner_info	*oinfo,
	bool				*has_other)
{
	struct xfs_rmap_matches		res;
	struct xfs_rmap_ownercount	roc;
	int				error;

	xfs_rmap_ownercount_init(&roc, bno, len, oinfo, &res);
	roc.stop_on_nonmatch = true;

	error = xfs_rmap_query_range(cur, &roc.low, &roc.high,
			xfs_rmap_count_owners_helper, &roc);
	if (error == -ECANCELED) {
		*has_other = true;
		return 0;
	}
	if (error)
		return error;

	*has_other = false;
	return 0;
}

const struct xfs_owner_info XFS_RMAP_OINFO_SKIP_UPDATE = {
	.oi_owner = XFS_RMAP_OWN_NULL,
};
const struct xfs_owner_info XFS_RMAP_OINFO_ANY_OWNER = {
	.oi_owner = XFS_RMAP_OWN_UNKNOWN,
};
const struct xfs_owner_info XFS_RMAP_OINFO_FS = {
	.oi_owner = XFS_RMAP_OWN_FS,
};
const struct xfs_owner_info XFS_RMAP_OINFO_LOG = {
	.oi_owner = XFS_RMAP_OWN_LOG,
};
const struct xfs_owner_info XFS_RMAP_OINFO_AG = {
	.oi_owner = XFS_RMAP_OWN_AG,
};
const struct xfs_owner_info XFS_RMAP_OINFO_INOBT = {
	.oi_owner = XFS_RMAP_OWN_INOBT,
};
const struct xfs_owner_info XFS_RMAP_OINFO_INODES = {
	.oi_owner = XFS_RMAP_OWN_INODES,
};
const struct xfs_owner_info XFS_RMAP_OINFO_REFC = {
	.oi_owner = XFS_RMAP_OWN_REFC,
};
const struct xfs_owner_info XFS_RMAP_OINFO_COW = {
	.oi_owner = XFS_RMAP_OWN_COW,
};

int __init
xfs_rmap_intent_init_cache(void)
{
	xfs_rmap_intent_cache = kmem_cache_create("xfs_rmap_intent",
			sizeof(struct xfs_rmap_intent),
			0, 0, NULL);

	return xfs_rmap_intent_cache != NULL ? 0 : -ENOMEM;
}

void
xfs_rmap_intent_destroy_cache(void)
{
	kmem_cache_destroy(xfs_rmap_intent_cache);
	xfs_rmap_intent_cache = NULL;
}


 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_ialloc.h"
#include "xfs_ialloc_btree.h"
#include "xfs_alloc.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_bmap.h"
#include "xfs_trans.h"
#include "xfs_buf_item.h"
#include "xfs_icreate_item.h"
#include "xfs_icache.h"
#include "xfs_trace.h"
#include "xfs_log.h"
#include "xfs_rmap.h"
#include "xfs_ag.h"

 
int					 
xfs_inobt_lookup(
	struct xfs_btree_cur	*cur,	 
	xfs_agino_t		ino,	 
	xfs_lookup_t		dir,	 
	int			*stat)	 
{
	cur->bc_rec.i.ir_startino = ino;
	cur->bc_rec.i.ir_holemask = 0;
	cur->bc_rec.i.ir_count = 0;
	cur->bc_rec.i.ir_freecount = 0;
	cur->bc_rec.i.ir_free = 0;
	return xfs_btree_lookup(cur, dir, stat);
}

 
STATIC int				 
xfs_inobt_update(
	struct xfs_btree_cur	*cur,	 
	xfs_inobt_rec_incore_t	*irec)	 
{
	union xfs_btree_rec	rec;

	rec.inobt.ir_startino = cpu_to_be32(irec->ir_startino);
	if (xfs_has_sparseinodes(cur->bc_mp)) {
		rec.inobt.ir_u.sp.ir_holemask = cpu_to_be16(irec->ir_holemask);
		rec.inobt.ir_u.sp.ir_count = irec->ir_count;
		rec.inobt.ir_u.sp.ir_freecount = irec->ir_freecount;
	} else {
		 
		rec.inobt.ir_u.f.ir_freecount = cpu_to_be32(irec->ir_freecount);
	}
	rec.inobt.ir_free = cpu_to_be64(irec->ir_free);
	return xfs_btree_update(cur, &rec);
}

 
void
xfs_inobt_btrec_to_irec(
	struct xfs_mount		*mp,
	const union xfs_btree_rec	*rec,
	struct xfs_inobt_rec_incore	*irec)
{
	irec->ir_startino = be32_to_cpu(rec->inobt.ir_startino);
	if (xfs_has_sparseinodes(mp)) {
		irec->ir_holemask = be16_to_cpu(rec->inobt.ir_u.sp.ir_holemask);
		irec->ir_count = rec->inobt.ir_u.sp.ir_count;
		irec->ir_freecount = rec->inobt.ir_u.sp.ir_freecount;
	} else {
		 
		irec->ir_holemask = XFS_INOBT_HOLEMASK_FULL;
		irec->ir_count = XFS_INODES_PER_CHUNK;
		irec->ir_freecount =
				be32_to_cpu(rec->inobt.ir_u.f.ir_freecount);
	}
	irec->ir_free = be64_to_cpu(rec->inobt.ir_free);
}

 
xfs_failaddr_t
xfs_inobt_check_irec(
	struct xfs_btree_cur			*cur,
	const struct xfs_inobt_rec_incore	*irec)
{
	uint64_t			realfree;

	 
	if (!xfs_verify_agino(cur->bc_ag.pag, irec->ir_startino))
		return __this_address;
	if (!xfs_verify_agino(cur->bc_ag.pag,
				irec->ir_startino + XFS_INODES_PER_CHUNK - 1))
		return __this_address;
	if (irec->ir_count < XFS_INODES_PER_HOLEMASK_BIT ||
	    irec->ir_count > XFS_INODES_PER_CHUNK)
		return __this_address;
	if (irec->ir_freecount > XFS_INODES_PER_CHUNK)
		return __this_address;

	 
	if (!xfs_inobt_issparse(irec->ir_holemask))
		realfree = irec->ir_free;
	else
		realfree = irec->ir_free & xfs_inobt_irec_to_allocmask(irec);
	if (hweight64(realfree) != irec->ir_freecount)
		return __this_address;

	return NULL;
}

static inline int
xfs_inobt_complain_bad_rec(
	struct xfs_btree_cur		*cur,
	xfs_failaddr_t			fa,
	const struct xfs_inobt_rec_incore *irec)
{
	struct xfs_mount		*mp = cur->bc_mp;

	xfs_warn(mp,
		"%s Inode BTree record corruption in AG %d detected at %pS!",
		cur->bc_btnum == XFS_BTNUM_INO ? "Used" : "Free",
		cur->bc_ag.pag->pag_agno, fa);
	xfs_warn(mp,
"start inode 0x%x, count 0x%x, free 0x%x freemask 0x%llx, holemask 0x%x",
		irec->ir_startino, irec->ir_count, irec->ir_freecount,
		irec->ir_free, irec->ir_holemask);
	return -EFSCORRUPTED;
}

 
int
xfs_inobt_get_rec(
	struct xfs_btree_cur		*cur,
	struct xfs_inobt_rec_incore	*irec,
	int				*stat)
{
	struct xfs_mount		*mp = cur->bc_mp;
	union xfs_btree_rec		*rec;
	xfs_failaddr_t			fa;
	int				error;

	error = xfs_btree_get_rec(cur, &rec, stat);
	if (error || *stat == 0)
		return error;

	xfs_inobt_btrec_to_irec(mp, rec, irec);
	fa = xfs_inobt_check_irec(cur, irec);
	if (fa)
		return xfs_inobt_complain_bad_rec(cur, fa, irec);

	return 0;
}

 
int
xfs_inobt_insert_rec(
	struct xfs_btree_cur	*cur,
	uint16_t		holemask,
	uint8_t			count,
	int32_t			freecount,
	xfs_inofree_t		free,
	int			*stat)
{
	cur->bc_rec.i.ir_holemask = holemask;
	cur->bc_rec.i.ir_count = count;
	cur->bc_rec.i.ir_freecount = freecount;
	cur->bc_rec.i.ir_free = free;
	return xfs_btree_insert(cur, stat);
}

 
STATIC int
xfs_inobt_insert(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	xfs_agino_t		newino,
	xfs_agino_t		newlen,
	xfs_btnum_t		btnum)
{
	struct xfs_btree_cur	*cur;
	xfs_agino_t		thisino;
	int			i;
	int			error;

	cur = xfs_inobt_init_cursor(pag, tp, agbp, btnum);

	for (thisino = newino;
	     thisino < newino + newlen;
	     thisino += XFS_INODES_PER_CHUNK) {
		error = xfs_inobt_lookup(cur, thisino, XFS_LOOKUP_EQ, &i);
		if (error) {
			xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
			return error;
		}
		ASSERT(i == 0);

		error = xfs_inobt_insert_rec(cur, XFS_INOBT_HOLEMASK_FULL,
					     XFS_INODES_PER_CHUNK,
					     XFS_INODES_PER_CHUNK,
					     XFS_INOBT_ALL_FREE, &i);
		if (error) {
			xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
			return error;
		}
		ASSERT(i == 1);
	}

	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);

	return 0;
}

 
#ifdef DEBUG
static int
xfs_check_agi_freecount(
	struct xfs_btree_cur	*cur)
{
	if (cur->bc_nlevels == 1) {
		xfs_inobt_rec_incore_t rec;
		int		freecount = 0;
		int		error;
		int		i;

		error = xfs_inobt_lookup(cur, 0, XFS_LOOKUP_GE, &i);
		if (error)
			return error;

		do {
			error = xfs_inobt_get_rec(cur, &rec, &i);
			if (error)
				return error;

			if (i) {
				freecount += rec.ir_freecount;
				error = xfs_btree_increment(cur, 0, &i);
				if (error)
					return error;
			}
		} while (i == 1);

		if (!xfs_is_shutdown(cur->bc_mp))
			ASSERT(freecount == cur->bc_ag.pag->pagi_freecount);
	}
	return 0;
}
#else
#define xfs_check_agi_freecount(cur)	0
#endif

 
int
xfs_ialloc_inode_init(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	struct list_head	*buffer_list,
	int			icount,
	xfs_agnumber_t		agno,
	xfs_agblock_t		agbno,
	xfs_agblock_t		length,
	unsigned int		gen)
{
	struct xfs_buf		*fbuf;
	struct xfs_dinode	*free;
	int			nbufs;
	int			version;
	int			i, j;
	xfs_daddr_t		d;
	xfs_ino_t		ino = 0;
	int			error;

	 
	nbufs = length / M_IGEO(mp)->blocks_per_cluster;

	 
	if (xfs_has_v3inodes(mp)) {
		version = 3;
		ino = XFS_AGINO_TO_INO(mp, agno, XFS_AGB_TO_AGINO(mp, agbno));

		 
		if (tp)
			xfs_icreate_log(tp, agno, agbno, icount,
					mp->m_sb.sb_inodesize, length, gen);
	} else
		version = 2;

	for (j = 0; j < nbufs; j++) {
		 
		d = XFS_AGB_TO_DADDR(mp, agno, agbno +
				(j * M_IGEO(mp)->blocks_per_cluster));
		error = xfs_trans_get_buf(tp, mp->m_ddev_targp, d,
				mp->m_bsize * M_IGEO(mp)->blocks_per_cluster,
				XBF_UNMAPPED, &fbuf);
		if (error)
			return error;

		 
		fbuf->b_ops = &xfs_inode_buf_ops;
		xfs_buf_zero(fbuf, 0, BBTOB(fbuf->b_length));
		for (i = 0; i < M_IGEO(mp)->inodes_per_cluster; i++) {
			int	ioffset = i << mp->m_sb.sb_inodelog;

			free = xfs_make_iptr(mp, fbuf, i);
			free->di_magic = cpu_to_be16(XFS_DINODE_MAGIC);
			free->di_version = version;
			free->di_gen = cpu_to_be32(gen);
			free->di_next_unlinked = cpu_to_be32(NULLAGINO);

			if (version == 3) {
				free->di_ino = cpu_to_be64(ino);
				ino++;
				uuid_copy(&free->di_uuid,
					  &mp->m_sb.sb_meta_uuid);
				xfs_dinode_calc_crc(mp, free);
			} else if (tp) {
				 
				xfs_trans_log_buf(tp, fbuf, ioffset,
					  ioffset + XFS_DINODE_SIZE(mp) - 1);
			}
		}

		if (tp) {
			 
			xfs_trans_inode_alloc_buf(tp, fbuf);
			if (version == 3) {
				 
				xfs_trans_ordered_buf(tp, fbuf);
			}
		} else {
			fbuf->b_flags |= XBF_DONE;
			xfs_buf_delwri_queue(fbuf, buffer_list);
			xfs_buf_relse(fbuf);
		}
	}
	return 0;
}

 
STATIC void
xfs_align_sparse_ino(
	struct xfs_mount		*mp,
	xfs_agino_t			*startino,
	uint16_t			*allocmask)
{
	xfs_agblock_t			agbno;
	xfs_agblock_t			mod;
	int				offset;

	agbno = XFS_AGINO_TO_AGBNO(mp, *startino);
	mod = agbno % mp->m_sb.sb_inoalignmt;
	if (!mod)
		return;

	 
	offset = XFS_AGB_TO_AGINO(mp, mod);
	*startino -= offset;

	 
	*allocmask <<= offset / XFS_INODES_PER_HOLEMASK_BIT;
}

 
STATIC bool
__xfs_inobt_can_merge(
	struct xfs_inobt_rec_incore	*trec,	 
	struct xfs_inobt_rec_incore	*srec)	 
{
	uint64_t			talloc;
	uint64_t			salloc;

	 
	if (trec->ir_startino != srec->ir_startino)
		return false;

	 
	if (!xfs_inobt_issparse(trec->ir_holemask) ||
	    !xfs_inobt_issparse(srec->ir_holemask))
		return false;

	 
	if (!trec->ir_count || !srec->ir_count)
		return false;

	 
	if (trec->ir_count + srec->ir_count > XFS_INODES_PER_CHUNK)
		return false;

	 
	talloc = xfs_inobt_irec_to_allocmask(trec);
	salloc = xfs_inobt_irec_to_allocmask(srec);
	if (talloc & salloc)
		return false;

	return true;
}

 
STATIC void
__xfs_inobt_rec_merge(
	struct xfs_inobt_rec_incore	*trec,	 
	struct xfs_inobt_rec_incore	*srec)	 
{
	ASSERT(trec->ir_startino == srec->ir_startino);

	 
	trec->ir_count += srec->ir_count;
	trec->ir_freecount += srec->ir_freecount;

	 
	trec->ir_holemask &= srec->ir_holemask;
	trec->ir_free &= srec->ir_free;
}

 
STATIC int
xfs_inobt_insert_sprec(
	struct xfs_perag		*pag,
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	int				btnum,
	struct xfs_inobt_rec_incore	*nrec,	 
	bool				merge)	 
{
	struct xfs_mount		*mp = pag->pag_mount;
	struct xfs_btree_cur		*cur;
	int				error;
	int				i;
	struct xfs_inobt_rec_incore	rec;

	cur = xfs_inobt_init_cursor(pag, tp, agbp, btnum);

	 
	error = xfs_inobt_lookup(cur, nrec->ir_startino, XFS_LOOKUP_EQ, &i);
	if (error)
		goto error;
	 
	if (i == 0) {
		error = xfs_inobt_insert_rec(cur, nrec->ir_holemask,
					     nrec->ir_count, nrec->ir_freecount,
					     nrec->ir_free, &i);
		if (error)
			goto error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error;
		}

		goto out;
	}

	 
	if (merge) {
		error = xfs_inobt_get_rec(cur, &rec, &i);
		if (error)
			goto error;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error;
		}
		if (XFS_IS_CORRUPT(mp, rec.ir_startino != nrec->ir_startino)) {
			error = -EFSCORRUPTED;
			goto error;
		}

		 
		if (XFS_IS_CORRUPT(mp, !__xfs_inobt_can_merge(nrec, &rec))) {
			error = -EFSCORRUPTED;
			goto error;
		}

		trace_xfs_irec_merge_pre(mp, pag->pag_agno, rec.ir_startino,
					 rec.ir_holemask, nrec->ir_startino,
					 nrec->ir_holemask);

		 
		__xfs_inobt_rec_merge(nrec, &rec);

		trace_xfs_irec_merge_post(mp, pag->pag_agno, nrec->ir_startino,
					  nrec->ir_holemask);

		error = xfs_inobt_rec_check_count(mp, nrec);
		if (error)
			goto error;
	}

	error = xfs_inobt_update(cur, nrec);
	if (error)
		goto error;

out:
	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	return 0;
error:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int
xfs_ialloc_ag_alloc(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp)
{
	struct xfs_agi		*agi;
	struct xfs_alloc_arg	args;
	int			error;
	xfs_agino_t		newino;		 
	xfs_agino_t		newlen;		 
	int			isaligned = 0;	 
						 
	 
	struct xfs_inobt_rec_incore rec;
	struct xfs_ino_geometry	*igeo = M_IGEO(tp->t_mountp);
	uint16_t		allocmask = (uint16_t) -1;
	int			do_sparse = 0;

	memset(&args, 0, sizeof(args));
	args.tp = tp;
	args.mp = tp->t_mountp;
	args.fsbno = NULLFSBLOCK;
	args.oinfo = XFS_RMAP_OINFO_INODES;
	args.pag = pag;

#ifdef DEBUG
	 
	if (xfs_has_sparseinodes(tp->t_mountp) &&
	    igeo->ialloc_min_blks < igeo->ialloc_blks)
		do_sparse = get_random_u32_below(2);
#endif

	 
	newlen = igeo->ialloc_inos;
	if (igeo->maxicount &&
	    percpu_counter_read_positive(&args.mp->m_icount) + newlen >
							igeo->maxicount)
		return -ENOSPC;
	args.minlen = args.maxlen = igeo->ialloc_blks;
	 
	agi = agbp->b_addr;
	newino = be32_to_cpu(agi->agi_newino);
	args.agbno = XFS_AGINO_TO_AGBNO(args.mp, newino) +
		     igeo->ialloc_blks;
	if (do_sparse)
		goto sparse_alloc;
	if (likely(newino != NULLAGINO &&
		  (args.agbno < be32_to_cpu(agi->agi_length)))) {
		args.prod = 1;

		 
		args.alignment = 1;
		args.minalignslop = igeo->cluster_align - 1;

		 
		args.minleft = igeo->inobt_maxlevels;
		error = xfs_alloc_vextent_exact_bno(&args,
				XFS_AGB_TO_FSB(args.mp, pag->pag_agno,
						args.agbno));
		if (error)
			return error;

		 
		args.minalignslop = 0;
	}

	if (unlikely(args.fsbno == NULLFSBLOCK)) {
		 
		isaligned = 0;
		if (igeo->ialloc_align) {
			ASSERT(!xfs_has_noalign(args.mp));
			args.alignment = args.mp->m_dalign;
			isaligned = 1;
		} else
			args.alignment = igeo->cluster_align;
		 
		args.prod = 1;
		 
		args.minleft = igeo->inobt_maxlevels;
		error = xfs_alloc_vextent_near_bno(&args,
				XFS_AGB_TO_FSB(args.mp, pag->pag_agno,
						be32_to_cpu(agi->agi_root)));
		if (error)
			return error;
	}

	 
	if (isaligned && args.fsbno == NULLFSBLOCK) {
		args.alignment = igeo->cluster_align;
		error = xfs_alloc_vextent_near_bno(&args,
				XFS_AGB_TO_FSB(args.mp, pag->pag_agno,
						be32_to_cpu(agi->agi_root)));
		if (error)
			return error;
	}

	 
	if (xfs_has_sparseinodes(args.mp) &&
	    igeo->ialloc_min_blks < igeo->ialloc_blks &&
	    args.fsbno == NULLFSBLOCK) {
sparse_alloc:
		args.alignment = args.mp->m_sb.sb_spino_align;
		args.prod = 1;

		args.minlen = igeo->ialloc_min_blks;
		args.maxlen = args.minlen;

		 
		args.min_agbno = args.mp->m_sb.sb_inoalignmt;
		args.max_agbno = round_down(args.mp->m_sb.sb_agblocks,
					    args.mp->m_sb.sb_inoalignmt) -
				 igeo->ialloc_blks;

		error = xfs_alloc_vextent_near_bno(&args,
				XFS_AGB_TO_FSB(args.mp, pag->pag_agno,
						be32_to_cpu(agi->agi_root)));
		if (error)
			return error;

		newlen = XFS_AGB_TO_AGINO(args.mp, args.len);
		ASSERT(newlen <= XFS_INODES_PER_CHUNK);
		allocmask = (1 << (newlen / XFS_INODES_PER_HOLEMASK_BIT)) - 1;
	}

	if (args.fsbno == NULLFSBLOCK)
		return -EAGAIN;

	ASSERT(args.len == args.minlen);

	 
	error = xfs_ialloc_inode_init(args.mp, tp, NULL, newlen, pag->pag_agno,
			args.agbno, args.len, get_random_u32());

	if (error)
		return error;
	 
	newino = XFS_AGB_TO_AGINO(args.mp, args.agbno);

	if (xfs_inobt_issparse(~allocmask)) {
		 
		xfs_align_sparse_ino(args.mp, &newino, &allocmask);

		rec.ir_startino = newino;
		rec.ir_holemask = ~allocmask;
		rec.ir_count = newlen;
		rec.ir_freecount = newlen;
		rec.ir_free = XFS_INOBT_ALL_FREE;

		 
		error = xfs_inobt_insert_sprec(pag, tp, agbp,
				XFS_BTNUM_INO, &rec, true);
		if (error == -EFSCORRUPTED) {
			xfs_alert(args.mp,
	"invalid sparse inode record: ino 0x%llx holemask 0x%x count %u",
				  XFS_AGINO_TO_INO(args.mp, pag->pag_agno,
						   rec.ir_startino),
				  rec.ir_holemask, rec.ir_count);
			xfs_force_shutdown(args.mp, SHUTDOWN_CORRUPT_INCORE);
		}
		if (error)
			return error;

		 
		if (xfs_has_finobt(args.mp)) {
			error = xfs_inobt_insert_sprec(pag, tp, agbp,
				       XFS_BTNUM_FINO, &rec, false);
			if (error)
				return error;
		}
	} else {
		 
		error = xfs_inobt_insert(pag, tp, agbp, newino, newlen,
					 XFS_BTNUM_INO);
		if (error)
			return error;

		if (xfs_has_finobt(args.mp)) {
			error = xfs_inobt_insert(pag, tp, agbp, newino,
						 newlen, XFS_BTNUM_FINO);
			if (error)
				return error;
		}
	}

	 
	be32_add_cpu(&agi->agi_count, newlen);
	be32_add_cpu(&agi->agi_freecount, newlen);
	pag->pagi_freecount += newlen;
	pag->pagi_count += newlen;
	agi->agi_newino = cpu_to_be32(newino);

	 
	xfs_ialloc_log_agi(tp, agbp,
		XFS_AGI_COUNT | XFS_AGI_FREECOUNT | XFS_AGI_NEWINO);
	 
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_ICOUNT, (long)newlen);
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, (long)newlen);
	return 0;
}

 
STATIC int
xfs_ialloc_next_rec(
	struct xfs_btree_cur	*cur,
	xfs_inobt_rec_incore_t	*rec,
	int			*done,
	int			left)
{
	int                     error;
	int			i;

	if (left)
		error = xfs_btree_decrement(cur, 0, &i);
	else
		error = xfs_btree_increment(cur, 0, &i);

	if (error)
		return error;
	*done = !i;
	if (i) {
		error = xfs_inobt_get_rec(cur, rec, &i);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
			return -EFSCORRUPTED;
	}

	return 0;
}

STATIC int
xfs_ialloc_get_rec(
	struct xfs_btree_cur	*cur,
	xfs_agino_t		agino,
	xfs_inobt_rec_incore_t	*rec,
	int			*done)
{
	int                     error;
	int			i;

	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_EQ, &i);
	if (error)
		return error;
	*done = !i;
	if (i) {
		error = xfs_inobt_get_rec(cur, rec, &i);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
			return -EFSCORRUPTED;
	}

	return 0;
}

 
STATIC int
xfs_inobt_first_free_inode(
	struct xfs_inobt_rec_incore	*rec)
{
	xfs_inofree_t			realfree;

	 
	if (!xfs_inobt_issparse(rec->ir_holemask))
		return xfs_lowbit64(rec->ir_free);

	realfree = xfs_inobt_irec_to_allocmask(rec);
	realfree &= rec->ir_free;

	return xfs_lowbit64(realfree);
}

 
STATIC int
xfs_dialloc_ag_inobt(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	xfs_ino_t		parent,
	xfs_ino_t		*inop)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_agi		*agi = agbp->b_addr;
	xfs_agnumber_t		pagno = XFS_INO_TO_AGNO(mp, parent);
	xfs_agino_t		pagino = XFS_INO_TO_AGINO(mp, parent);
	struct xfs_btree_cur	*cur, *tcur;
	struct xfs_inobt_rec_incore rec, trec;
	xfs_ino_t		ino;
	int			error;
	int			offset;
	int			i, j;
	int			searchdistance = 10;

	ASSERT(xfs_perag_initialised_agi(pag));
	ASSERT(xfs_perag_allows_inodes(pag));
	ASSERT(pag->pagi_freecount > 0);

 restart_pagno:
	cur = xfs_inobt_init_cursor(pag, tp, agbp, XFS_BTNUM_INO);
	 
	if (!pagino)
		pagino = be32_to_cpu(agi->agi_newino);

	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error0;

	 
	if (pagno == pag->pag_agno) {
		int		doneleft;	 
		int		doneright;	 

		error = xfs_inobt_lookup(cur, pagino, XFS_LOOKUP_LE, &i);
		if (error)
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		error = xfs_inobt_get_rec(cur, &rec, &j);
		if (error)
			goto error0;
		if (XFS_IS_CORRUPT(mp, j != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}

		if (rec.ir_freecount > 0) {
			 
			goto alloc_inode;
		}


		 

		 
		error = xfs_btree_dup_cursor(cur, &tcur);
		if (error)
			goto error0;

		 
		if (pagino != NULLAGINO &&
		    pag->pagl_pagino == pagino &&
		    pag->pagl_leftrec != NULLAGINO &&
		    pag->pagl_rightrec != NULLAGINO) {
			error = xfs_ialloc_get_rec(tcur, pag->pagl_leftrec,
						   &trec, &doneleft);
			if (error)
				goto error1;

			error = xfs_ialloc_get_rec(cur, pag->pagl_rightrec,
						   &rec, &doneright);
			if (error)
				goto error1;
		} else {
			 
			error = xfs_ialloc_next_rec(tcur, &trec, &doneleft, 1);
			if (error)
				goto error1;

			 
			error = xfs_ialloc_next_rec(cur, &rec, &doneright, 0);
			if (error)
				goto error1;
		}

		 
		while (--searchdistance > 0 && (!doneleft || !doneright)) {
			int	useleft;   

			 
			if (!doneleft && !doneright) {
				useleft = pagino -
				 (trec.ir_startino + XFS_INODES_PER_CHUNK - 1) <
				  rec.ir_startino - pagino;
			} else {
				useleft = !doneleft;
			}

			 
			if (useleft && trec.ir_freecount) {
				xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
				cur = tcur;

				pag->pagl_leftrec = trec.ir_startino;
				pag->pagl_rightrec = rec.ir_startino;
				pag->pagl_pagino = pagino;
				rec = trec;
				goto alloc_inode;
			}

			 
			if (!useleft && rec.ir_freecount) {
				xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);

				pag->pagl_leftrec = trec.ir_startino;
				pag->pagl_rightrec = rec.ir_startino;
				pag->pagl_pagino = pagino;
				goto alloc_inode;
			}

			 
			if (useleft) {
				error = xfs_ialloc_next_rec(tcur, &trec,
								 &doneleft, 1);
			} else {
				error = xfs_ialloc_next_rec(cur, &rec,
								 &doneright, 0);
			}
			if (error)
				goto error1;
		}

		if (searchdistance <= 0) {
			 
			xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
			pag->pagl_leftrec = trec.ir_startino;
			pag->pagl_rightrec = rec.ir_startino;
			pag->pagl_pagino = pagino;

		} else {
			 
			pag->pagl_pagino = NULLAGINO;
			pag->pagl_leftrec = NULLAGINO;
			pag->pagl_rightrec = NULLAGINO;
			xfs_btree_del_cursor(tcur, XFS_BTREE_NOERROR);
			xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
			goto restart_pagno;
		}
	}

	 
	if (agi->agi_newino != cpu_to_be32(NULLAGINO)) {
		error = xfs_inobt_lookup(cur, be32_to_cpu(agi->agi_newino),
					 XFS_LOOKUP_EQ, &i);
		if (error)
			goto error0;

		if (i == 1) {
			error = xfs_inobt_get_rec(cur, &rec, &j);
			if (error)
				goto error0;

			if (j == 1 && rec.ir_freecount > 0) {
				 
				goto alloc_inode;
			}
		}
	}

	 
	error = xfs_inobt_lookup(cur, 0, XFS_LOOKUP_GE, &i);
	if (error)
		goto error0;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error0;
	}

	for (;;) {
		error = xfs_inobt_get_rec(cur, &rec, &i);
		if (error)
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if (rec.ir_freecount > 0)
			break;
		error = xfs_btree_increment(cur, 0, &i);
		if (error)
			goto error0;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
	}

alloc_inode:
	offset = xfs_inobt_first_free_inode(&rec);
	ASSERT(offset >= 0);
	ASSERT(offset < XFS_INODES_PER_CHUNK);
	ASSERT((XFS_AGINO_TO_OFFSET(mp, rec.ir_startino) %
				   XFS_INODES_PER_CHUNK) == 0);
	ino = XFS_AGINO_TO_INO(mp, pag->pag_agno, rec.ir_startino + offset);
	rec.ir_free &= ~XFS_INOBT_MASK(offset);
	rec.ir_freecount--;
	error = xfs_inobt_update(cur, &rec);
	if (error)
		goto error0;
	be32_add_cpu(&agi->agi_freecount, -1);
	xfs_ialloc_log_agi(tp, agbp, XFS_AGI_FREECOUNT);
	pag->pagi_freecount--;

	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error0;

	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, -1);
	*inop = ino;
	return 0;
error1:
	xfs_btree_del_cursor(tcur, XFS_BTREE_ERROR);
error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int
xfs_dialloc_ag_finobt_near(
	xfs_agino_t			pagino,
	struct xfs_btree_cur		**ocur,
	struct xfs_inobt_rec_incore	*rec)
{
	struct xfs_btree_cur		*lcur = *ocur;	 
	struct xfs_btree_cur		*rcur;	 
	struct xfs_inobt_rec_incore	rrec;
	int				error;
	int				i, j;

	error = xfs_inobt_lookup(lcur, pagino, XFS_LOOKUP_LE, &i);
	if (error)
		return error;

	if (i == 1) {
		error = xfs_inobt_get_rec(lcur, rec, &i);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(lcur->bc_mp, i != 1))
			return -EFSCORRUPTED;

		 
		if (pagino >= rec->ir_startino &&
		    pagino < (rec->ir_startino + XFS_INODES_PER_CHUNK))
			return 0;
	}

	error = xfs_btree_dup_cursor(lcur, &rcur);
	if (error)
		return error;

	error = xfs_inobt_lookup(rcur, pagino, XFS_LOOKUP_GE, &j);
	if (error)
		goto error_rcur;
	if (j == 1) {
		error = xfs_inobt_get_rec(rcur, &rrec, &j);
		if (error)
			goto error_rcur;
		if (XFS_IS_CORRUPT(lcur->bc_mp, j != 1)) {
			error = -EFSCORRUPTED;
			goto error_rcur;
		}
	}

	if (XFS_IS_CORRUPT(lcur->bc_mp, i != 1 && j != 1)) {
		error = -EFSCORRUPTED;
		goto error_rcur;
	}
	if (i == 1 && j == 1) {
		 
		if ((pagino - rec->ir_startino + XFS_INODES_PER_CHUNK - 1) >
		    (rrec.ir_startino - pagino)) {
			*rec = rrec;
			xfs_btree_del_cursor(lcur, XFS_BTREE_NOERROR);
			*ocur = rcur;
		} else {
			xfs_btree_del_cursor(rcur, XFS_BTREE_NOERROR);
		}
	} else if (j == 1) {
		 
		*rec = rrec;
		xfs_btree_del_cursor(lcur, XFS_BTREE_NOERROR);
		*ocur = rcur;
	} else if (i == 1) {
		 
		xfs_btree_del_cursor(rcur, XFS_BTREE_NOERROR);
	}

	return 0;

error_rcur:
	xfs_btree_del_cursor(rcur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int
xfs_dialloc_ag_finobt_newino(
	struct xfs_agi			*agi,
	struct xfs_btree_cur		*cur,
	struct xfs_inobt_rec_incore	*rec)
{
	int error;
	int i;

	if (agi->agi_newino != cpu_to_be32(NULLAGINO)) {
		error = xfs_inobt_lookup(cur, be32_to_cpu(agi->agi_newino),
					 XFS_LOOKUP_EQ, &i);
		if (error)
			return error;
		if (i == 1) {
			error = xfs_inobt_get_rec(cur, rec, &i);
			if (error)
				return error;
			if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
				return -EFSCORRUPTED;
			return 0;
		}
	}

	 
	error = xfs_inobt_lookup(cur, 0, XFS_LOOKUP_GE, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
		return -EFSCORRUPTED;

	error = xfs_inobt_get_rec(cur, rec, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
		return -EFSCORRUPTED;

	return 0;
}

 
STATIC int
xfs_dialloc_ag_update_inobt(
	struct xfs_btree_cur		*cur,	 
	struct xfs_inobt_rec_incore	*frec,	 
	int				offset)  
{
	struct xfs_inobt_rec_incore	rec;
	int				error;
	int				i;

	error = xfs_inobt_lookup(cur, frec->ir_startino, XFS_LOOKUP_EQ, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
		return -EFSCORRUPTED;

	error = xfs_inobt_get_rec(cur, &rec, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(cur->bc_mp, i != 1))
		return -EFSCORRUPTED;
	ASSERT((XFS_AGINO_TO_OFFSET(cur->bc_mp, rec.ir_startino) %
				   XFS_INODES_PER_CHUNK) == 0);

	rec.ir_free &= ~XFS_INOBT_MASK(offset);
	rec.ir_freecount--;

	if (XFS_IS_CORRUPT(cur->bc_mp,
			   rec.ir_free != frec->ir_free ||
			   rec.ir_freecount != frec->ir_freecount))
		return -EFSCORRUPTED;

	return xfs_inobt_update(cur, &rec);
}

 
static int
xfs_dialloc_ag(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agbp,
	xfs_ino_t		parent,
	xfs_ino_t		*inop)
{
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_agi			*agi = agbp->b_addr;
	xfs_agnumber_t			pagno = XFS_INO_TO_AGNO(mp, parent);
	xfs_agino_t			pagino = XFS_INO_TO_AGINO(mp, parent);
	struct xfs_btree_cur		*cur;	 
	struct xfs_btree_cur		*icur;	 
	struct xfs_inobt_rec_incore	rec;
	xfs_ino_t			ino;
	int				error;
	int				offset;
	int				i;

	if (!xfs_has_finobt(mp))
		return xfs_dialloc_ag_inobt(pag, tp, agbp, parent, inop);

	 
	if (!pagino)
		pagino = be32_to_cpu(agi->agi_newino);

	cur = xfs_inobt_init_cursor(pag, tp, agbp, XFS_BTNUM_FINO);

	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error_cur;

	 
	if (pag->pag_agno == pagno)
		error = xfs_dialloc_ag_finobt_near(pagino, &cur, &rec);
	else
		error = xfs_dialloc_ag_finobt_newino(agi, cur, &rec);
	if (error)
		goto error_cur;

	offset = xfs_inobt_first_free_inode(&rec);
	ASSERT(offset >= 0);
	ASSERT(offset < XFS_INODES_PER_CHUNK);
	ASSERT((XFS_AGINO_TO_OFFSET(mp, rec.ir_startino) %
				   XFS_INODES_PER_CHUNK) == 0);
	ino = XFS_AGINO_TO_INO(mp, pag->pag_agno, rec.ir_startino + offset);

	 
	rec.ir_free &= ~XFS_INOBT_MASK(offset);
	rec.ir_freecount--;
	if (rec.ir_freecount)
		error = xfs_inobt_update(cur, &rec);
	else
		error = xfs_btree_delete(cur, &i);
	if (error)
		goto error_cur;

	 
	icur = xfs_inobt_init_cursor(pag, tp, agbp, XFS_BTNUM_INO);

	error = xfs_check_agi_freecount(icur);
	if (error)
		goto error_icur;

	error = xfs_dialloc_ag_update_inobt(icur, &rec, offset);
	if (error)
		goto error_icur;

	 
	be32_add_cpu(&agi->agi_freecount, -1);
	xfs_ialloc_log_agi(tp, agbp, XFS_AGI_FREECOUNT);
	pag->pagi_freecount--;

	xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, -1);

	error = xfs_check_agi_freecount(icur);
	if (error)
		goto error_icur;
	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error_icur;

	xfs_btree_del_cursor(icur, XFS_BTREE_NOERROR);
	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	*inop = ino;
	return 0;

error_icur:
	xfs_btree_del_cursor(icur, XFS_BTREE_ERROR);
error_cur:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

static int
xfs_dialloc_roll(
	struct xfs_trans	**tpp,
	struct xfs_buf		*agibp)
{
	struct xfs_trans	*tp = *tpp;
	struct xfs_dquot_acct	*dqinfo;
	int			error;

	 
	xfs_trans_bhold(tp, agibp);

	 
	dqinfo = tp->t_dqinfo;
	tp->t_dqinfo = NULL;

	error = xfs_trans_roll(&tp);

	 
	tp->t_dqinfo = dqinfo;

	 
	xfs_trans_bjoin(tp, agibp);
	*tpp = tp;
	return error;
}

static bool
xfs_dialloc_good_ag(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	umode_t			mode,
	int			flags,
	bool			ok_alloc)
{
	struct xfs_mount	*mp = tp->t_mountp;
	xfs_extlen_t		ineed;
	xfs_extlen_t		longest = 0;
	int			needspace;
	int			error;

	if (!pag)
		return false;
	if (!xfs_perag_allows_inodes(pag))
		return false;

	if (!xfs_perag_initialised_agi(pag)) {
		error = xfs_ialloc_read_agi(pag, tp, NULL);
		if (error)
			return false;
	}

	if (pag->pagi_freecount)
		return true;
	if (!ok_alloc)
		return false;

	if (!xfs_perag_initialised_agf(pag)) {
		error = xfs_alloc_read_agf(pag, tp, flags, NULL);
		if (error)
			return false;
	}

	 
	ineed = M_IGEO(mp)->ialloc_min_blks;
	if (flags && ineed > 1)
		ineed += M_IGEO(mp)->cluster_align;
	longest = pag->pagf_longest;
	if (!longest)
		longest = pag->pagf_flcount > 0;
	needspace = S_ISDIR(mode) || S_ISREG(mode) || S_ISLNK(mode);

	if (pag->pagf_freeblks < needspace + ineed || longest < ineed)
		return false;
	return true;
}

static int
xfs_dialloc_try_ag(
	struct xfs_perag	*pag,
	struct xfs_trans	**tpp,
	xfs_ino_t		parent,
	xfs_ino_t		*new_ino,
	bool			ok_alloc)
{
	struct xfs_buf		*agbp;
	xfs_ino_t		ino;
	int			error;

	 
	error = xfs_ialloc_read_agi(pag, *tpp, &agbp);
	if (error)
		return error;

	if (!pag->pagi_freecount) {
		if (!ok_alloc) {
			error = -EAGAIN;
			goto out_release;
		}

		error = xfs_ialloc_ag_alloc(pag, *tpp, agbp);
		if (error < 0)
			goto out_release;

		 
		ASSERT(pag->pagi_freecount > 0);
		error = xfs_dialloc_roll(tpp, agbp);
		if (error)
			goto out_release;
	}

	 
	error = xfs_dialloc_ag(pag, *tpp, agbp, parent, &ino);
	if (!error)
		*new_ino = ino;
	return error;

out_release:
	xfs_trans_brelse(*tpp, agbp);
	return error;
}

 
int
xfs_dialloc(
	struct xfs_trans	**tpp,
	xfs_ino_t		parent,
	umode_t			mode,
	xfs_ino_t		*new_ino)
{
	struct xfs_mount	*mp = (*tpp)->t_mountp;
	xfs_agnumber_t		agno;
	int			error = 0;
	xfs_agnumber_t		start_agno;
	struct xfs_perag	*pag;
	struct xfs_ino_geometry	*igeo = M_IGEO(mp);
	bool			ok_alloc = true;
	bool			low_space = false;
	int			flags;
	xfs_ino_t		ino = NULLFSINO;

	 
	if (S_ISDIR(mode))
		start_agno = (atomic_inc_return(&mp->m_agirotor) - 1) %
				mp->m_maxagi;
	else {
		start_agno = XFS_INO_TO_AGNO(mp, parent);
		if (start_agno >= mp->m_maxagi)
			start_agno = 0;
	}

	 
	if (igeo->maxicount &&
	    percpu_counter_read_positive(&mp->m_icount) + igeo->ialloc_inos
							> igeo->maxicount) {
		ok_alloc = false;
	}

	 
	if (percpu_counter_read_positive(&mp->m_fdblocks) <
			mp->m_low_space[XFS_LOWSP_1_PCNT]) {
		ok_alloc = false;
		low_space = true;
	}

	 
	flags = XFS_ALLOC_FLAG_TRYLOCK;
retry:
	for_each_perag_wrap_at(mp, start_agno, mp->m_maxagi, agno, pag) {
		if (xfs_dialloc_good_ag(pag, *tpp, mode, flags, ok_alloc)) {
			error = xfs_dialloc_try_ag(pag, tpp, parent,
					&ino, ok_alloc);
			if (error != -EAGAIN)
				break;
			error = 0;
		}

		if (xfs_is_shutdown(mp)) {
			error = -EFSCORRUPTED;
			break;
		}
	}
	if (pag)
		xfs_perag_rele(pag);
	if (error)
		return error;
	if (ino == NULLFSINO) {
		if (flags) {
			flags = 0;
			if (low_space)
				ok_alloc = true;
			goto retry;
		}
		return -ENOSPC;
	}
	*new_ino = ino;
	return 0;
}

 
static int
xfs_difree_inode_chunk(
	struct xfs_trans		*tp,
	xfs_agnumber_t			agno,
	struct xfs_inobt_rec_incore	*rec)
{
	struct xfs_mount		*mp = tp->t_mountp;
	xfs_agblock_t			sagbno = XFS_AGINO_TO_AGBNO(mp,
							rec->ir_startino);
	int				startidx, endidx;
	int				nextbit;
	xfs_agblock_t			agbno;
	int				contigblk;
	DECLARE_BITMAP(holemask, XFS_INOBT_HOLEMASK_BITS);

	if (!xfs_inobt_issparse(rec->ir_holemask)) {
		 
		return xfs_free_extent_later(tp,
				XFS_AGB_TO_FSB(mp, agno, sagbno),
				M_IGEO(mp)->ialloc_blks, &XFS_RMAP_OINFO_INODES,
				XFS_AG_RESV_NONE);
	}

	 
	ASSERT(sizeof(rec->ir_holemask) <= sizeof(holemask[0]));
	holemask[0] = rec->ir_holemask;

	 
	startidx = endidx = find_first_zero_bit(holemask,
						XFS_INOBT_HOLEMASK_BITS);
	nextbit = startidx + 1;
	while (startidx < XFS_INOBT_HOLEMASK_BITS) {
		int error;

		nextbit = find_next_zero_bit(holemask, XFS_INOBT_HOLEMASK_BITS,
					     nextbit);
		 
		if (nextbit != XFS_INOBT_HOLEMASK_BITS &&
		    nextbit == endidx + 1) {
			endidx = nextbit;
			goto next;
		}

		 
		agbno = sagbno + (startidx * XFS_INODES_PER_HOLEMASK_BIT) /
				  mp->m_sb.sb_inopblock;
		contigblk = ((endidx - startidx + 1) *
			     XFS_INODES_PER_HOLEMASK_BIT) /
			    mp->m_sb.sb_inopblock;

		ASSERT(agbno % mp->m_sb.sb_spino_align == 0);
		ASSERT(contigblk % mp->m_sb.sb_spino_align == 0);
		error = xfs_free_extent_later(tp,
				XFS_AGB_TO_FSB(mp, agno, agbno), contigblk,
				&XFS_RMAP_OINFO_INODES, XFS_AG_RESV_NONE);
		if (error)
			return error;

		 
		startidx = endidx = nextbit;

next:
		nextbit++;
	}
	return 0;
}

STATIC int
xfs_difree_inobt(
	struct xfs_perag		*pag,
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	xfs_agino_t			agino,
	struct xfs_icluster		*xic,
	struct xfs_inobt_rec_incore	*orec)
{
	struct xfs_mount		*mp = pag->pag_mount;
	struct xfs_agi			*agi = agbp->b_addr;
	struct xfs_btree_cur		*cur;
	struct xfs_inobt_rec_incore	rec;
	int				ilen;
	int				error;
	int				i;
	int				off;

	ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
	ASSERT(XFS_AGINO_TO_AGBNO(mp, agino) < be32_to_cpu(agi->agi_length));

	 
	cur = xfs_inobt_init_cursor(pag, tp, agbp, XFS_BTNUM_INO);

	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error0;

	 
	if ((error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &i))) {
		xfs_warn(mp, "%s: xfs_inobt_lookup() returned error %d.",
			__func__, error);
		goto error0;
	}
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	error = xfs_inobt_get_rec(cur, &rec, &i);
	if (error) {
		xfs_warn(mp, "%s: xfs_inobt_get_rec() returned error %d.",
			__func__, error);
		goto error0;
	}
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error0;
	}
	 
	off = agino - rec.ir_startino;
	ASSERT(off >= 0 && off < XFS_INODES_PER_CHUNK);
	ASSERT(!(rec.ir_free & XFS_INOBT_MASK(off)));
	 
	rec.ir_free |= XFS_INOBT_MASK(off);
	rec.ir_freecount++;

	 
	if (!xfs_has_ikeep(mp) && rec.ir_free == XFS_INOBT_ALL_FREE &&
	    mp->m_sb.sb_inopblock <= XFS_INODES_PER_CHUNK) {
		xic->deleted = true;
		xic->first_ino = XFS_AGINO_TO_INO(mp, pag->pag_agno,
				rec.ir_startino);
		xic->alloc = xfs_inobt_irec_to_allocmask(&rec);

		 
		ilen = rec.ir_freecount;
		be32_add_cpu(&agi->agi_count, -ilen);
		be32_add_cpu(&agi->agi_freecount, -(ilen - 1));
		xfs_ialloc_log_agi(tp, agbp, XFS_AGI_COUNT | XFS_AGI_FREECOUNT);
		pag->pagi_freecount -= ilen - 1;
		pag->pagi_count -= ilen;
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_ICOUNT, -ilen);
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, -(ilen - 1));

		if ((error = xfs_btree_delete(cur, &i))) {
			xfs_warn(mp, "%s: xfs_btree_delete returned error %d.",
				__func__, error);
			goto error0;
		}

		error = xfs_difree_inode_chunk(tp, pag->pag_agno, &rec);
		if (error)
			goto error0;
	} else {
		xic->deleted = false;

		error = xfs_inobt_update(cur, &rec);
		if (error) {
			xfs_warn(mp, "%s: xfs_inobt_update returned error %d.",
				__func__, error);
			goto error0;
		}

		 
		be32_add_cpu(&agi->agi_freecount, 1);
		xfs_ialloc_log_agi(tp, agbp, XFS_AGI_FREECOUNT);
		pag->pagi_freecount++;
		xfs_trans_mod_sb(tp, XFS_TRANS_SB_IFREE, 1);
	}

	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error0;

	*orec = rec;
	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	return 0;

error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int
xfs_difree_finobt(
	struct xfs_perag		*pag,
	struct xfs_trans		*tp,
	struct xfs_buf			*agbp,
	xfs_agino_t			agino,
	struct xfs_inobt_rec_incore	*ibtrec)  
{
	struct xfs_mount		*mp = pag->pag_mount;
	struct xfs_btree_cur		*cur;
	struct xfs_inobt_rec_incore	rec;
	int				offset = agino - ibtrec->ir_startino;
	int				error;
	int				i;

	cur = xfs_inobt_init_cursor(pag, tp, agbp, XFS_BTNUM_FINO);

	error = xfs_inobt_lookup(cur, ibtrec->ir_startino, XFS_LOOKUP_EQ, &i);
	if (error)
		goto error;
	if (i == 0) {
		 
		if (XFS_IS_CORRUPT(mp, ibtrec->ir_freecount != 1)) {
			error = -EFSCORRUPTED;
			goto error;
		}

		error = xfs_inobt_insert_rec(cur, ibtrec->ir_holemask,
					     ibtrec->ir_count,
					     ibtrec->ir_freecount,
					     ibtrec->ir_free, &i);
		if (error)
			goto error;
		ASSERT(i == 1);

		goto out;
	}

	 
	error = xfs_inobt_get_rec(cur, &rec, &i);
	if (error)
		goto error;
	if (XFS_IS_CORRUPT(mp, i != 1)) {
		error = -EFSCORRUPTED;
		goto error;
	}

	rec.ir_free |= XFS_INOBT_MASK(offset);
	rec.ir_freecount++;

	if (XFS_IS_CORRUPT(mp,
			   rec.ir_free != ibtrec->ir_free ||
			   rec.ir_freecount != ibtrec->ir_freecount)) {
		error = -EFSCORRUPTED;
		goto error;
	}

	 
	if (!xfs_has_ikeep(mp) && rec.ir_free == XFS_INOBT_ALL_FREE &&
	    mp->m_sb.sb_inopblock <= XFS_INODES_PER_CHUNK) {
		error = xfs_btree_delete(cur, &i);
		if (error)
			goto error;
		ASSERT(i == 1);
	} else {
		error = xfs_inobt_update(cur, &rec);
		if (error)
			goto error;
	}

out:
	error = xfs_check_agi_freecount(cur);
	if (error)
		goto error;

	xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	return 0;

error:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

 
int
xfs_difree(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	xfs_ino_t		inode,
	struct xfs_icluster	*xic)
{
	 
	xfs_agblock_t		agbno;	 
	struct xfs_buf		*agbp;	 
	xfs_agino_t		agino;	 
	int			error;	 
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_inobt_rec_incore rec; 

	 
	if (pag->pag_agno != XFS_INO_TO_AGNO(mp, inode)) {
		xfs_warn(mp, "%s: agno != pag->pag_agno (%d != %d).",
			__func__, XFS_INO_TO_AGNO(mp, inode), pag->pag_agno);
		ASSERT(0);
		return -EINVAL;
	}
	agino = XFS_INO_TO_AGINO(mp, inode);
	if (inode != XFS_AGINO_TO_INO(mp, pag->pag_agno, agino))  {
		xfs_warn(mp, "%s: inode != XFS_AGINO_TO_INO() (%llu != %llu).",
			__func__, (unsigned long long)inode,
			(unsigned long long)XFS_AGINO_TO_INO(mp, pag->pag_agno, agino));
		ASSERT(0);
		return -EINVAL;
	}
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);
	if (agbno >= mp->m_sb.sb_agblocks)  {
		xfs_warn(mp, "%s: agbno >= mp->m_sb.sb_agblocks (%d >= %d).",
			__func__, agbno, mp->m_sb.sb_agblocks);
		ASSERT(0);
		return -EINVAL;
	}
	 
	error = xfs_ialloc_read_agi(pag, tp, &agbp);
	if (error) {
		xfs_warn(mp, "%s: xfs_ialloc_read_agi() returned error %d.",
			__func__, error);
		return error;
	}

	 
	error = xfs_difree_inobt(pag, tp, agbp, agino, xic, &rec);
	if (error)
		goto error0;

	 
	if (xfs_has_finobt(mp)) {
		error = xfs_difree_finobt(pag, tp, agbp, agino, &rec);
		if (error)
			goto error0;
	}

	return 0;

error0:
	return error;
}

STATIC int
xfs_imap_lookup(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	xfs_agino_t		agino,
	xfs_agblock_t		agbno,
	xfs_agblock_t		*chunk_agbno,
	xfs_agblock_t		*offset_agbno,
	int			flags)
{
	struct xfs_mount	*mp = pag->pag_mount;
	struct xfs_inobt_rec_incore rec;
	struct xfs_btree_cur	*cur;
	struct xfs_buf		*agbp;
	int			error;
	int			i;

	error = xfs_ialloc_read_agi(pag, tp, &agbp);
	if (error) {
		xfs_alert(mp,
			"%s: xfs_ialloc_read_agi() returned error %d, agno %d",
			__func__, error, pag->pag_agno);
		return error;
	}

	 
	cur = xfs_inobt_init_cursor(pag, tp, agbp, XFS_BTNUM_INO);
	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &i);
	if (!error) {
		if (i)
			error = xfs_inobt_get_rec(cur, &rec, &i);
		if (!error && i == 0)
			error = -EINVAL;
	}

	xfs_trans_brelse(tp, agbp);
	xfs_btree_del_cursor(cur, error);
	if (error)
		return error;

	 
	if (rec.ir_startino > agino ||
	    rec.ir_startino + M_IGEO(mp)->ialloc_inos <= agino)
		return -EINVAL;

	 
	if ((flags & XFS_IGET_UNTRUSTED) &&
	    (rec.ir_free & XFS_INOBT_MASK(agino - rec.ir_startino)))
		return -EINVAL;

	*chunk_agbno = XFS_AGINO_TO_AGBNO(mp, rec.ir_startino);
	*offset_agbno = agbno - *chunk_agbno;
	return 0;
}

 
int
xfs_imap(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	xfs_ino_t		ino,	 
	struct xfs_imap		*imap,	 
	uint			flags)	 
{
	struct xfs_mount	*mp = pag->pag_mount;
	xfs_agblock_t		agbno;	 
	xfs_agino_t		agino;	 
	xfs_agblock_t		chunk_agbno;	 
	xfs_agblock_t		cluster_agbno;	 
	int			error;	 
	int			offset;	 
	xfs_agblock_t		offset_agbno;	 

	ASSERT(ino != NULLFSINO);

	 
	agino = XFS_INO_TO_AGINO(mp, ino);
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);
	if (agbno >= mp->m_sb.sb_agblocks ||
	    ino != XFS_AGINO_TO_INO(mp, pag->pag_agno, agino)) {
		error = -EINVAL;
#ifdef DEBUG
		 
		if (flags & XFS_IGET_UNTRUSTED)
			return error;
		if (agbno >= mp->m_sb.sb_agblocks) {
			xfs_alert(mp,
		"%s: agbno (0x%llx) >= mp->m_sb.sb_agblocks (0x%lx)",
				__func__, (unsigned long long)agbno,
				(unsigned long)mp->m_sb.sb_agblocks);
		}
		if (ino != XFS_AGINO_TO_INO(mp, pag->pag_agno, agino)) {
			xfs_alert(mp,
		"%s: ino (0x%llx) != XFS_AGINO_TO_INO() (0x%llx)",
				__func__, ino,
				XFS_AGINO_TO_INO(mp, pag->pag_agno, agino));
		}
		xfs_stack_trace();
#endif  
		return error;
	}

	 
	if (flags & XFS_IGET_UNTRUSTED) {
		error = xfs_imap_lookup(pag, tp, agino, agbno,
					&chunk_agbno, &offset_agbno, flags);
		if (error)
			return error;
		goto out_map;
	}

	 
	if (M_IGEO(mp)->blocks_per_cluster == 1) {
		offset = XFS_INO_TO_OFFSET(mp, ino);
		ASSERT(offset < mp->m_sb.sb_inopblock);

		imap->im_blkno = XFS_AGB_TO_DADDR(mp, pag->pag_agno, agbno);
		imap->im_len = XFS_FSB_TO_BB(mp, 1);
		imap->im_boffset = (unsigned short)(offset <<
							mp->m_sb.sb_inodelog);
		return 0;
	}

	 
	if (M_IGEO(mp)->inoalign_mask) {
		offset_agbno = agbno & M_IGEO(mp)->inoalign_mask;
		chunk_agbno = agbno - offset_agbno;
	} else {
		error = xfs_imap_lookup(pag, tp, agino, agbno,
					&chunk_agbno, &offset_agbno, flags);
		if (error)
			return error;
	}

out_map:
	ASSERT(agbno >= chunk_agbno);
	cluster_agbno = chunk_agbno +
		((offset_agbno / M_IGEO(mp)->blocks_per_cluster) *
		 M_IGEO(mp)->blocks_per_cluster);
	offset = ((agbno - cluster_agbno) * mp->m_sb.sb_inopblock) +
		XFS_INO_TO_OFFSET(mp, ino);

	imap->im_blkno = XFS_AGB_TO_DADDR(mp, pag->pag_agno, cluster_agbno);
	imap->im_len = XFS_FSB_TO_BB(mp, M_IGEO(mp)->blocks_per_cluster);
	imap->im_boffset = (unsigned short)(offset << mp->m_sb.sb_inodelog);

	 
	if ((imap->im_blkno + imap->im_len) >
	    XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks)) {
		xfs_alert(mp,
	"%s: (im_blkno (0x%llx) + im_len (0x%llx)) > sb_dblocks (0x%llx)",
			__func__, (unsigned long long) imap->im_blkno,
			(unsigned long long) imap->im_len,
			XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks));
		return -EINVAL;
	}
	return 0;
}

 
void
xfs_ialloc_log_agi(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp,
	uint32_t		fields)
{
	int			first;		 
	int			last;		 
	static const short	offsets[] = {	 
					 
		offsetof(xfs_agi_t, agi_magicnum),
		offsetof(xfs_agi_t, agi_versionnum),
		offsetof(xfs_agi_t, agi_seqno),
		offsetof(xfs_agi_t, agi_length),
		offsetof(xfs_agi_t, agi_count),
		offsetof(xfs_agi_t, agi_root),
		offsetof(xfs_agi_t, agi_level),
		offsetof(xfs_agi_t, agi_freecount),
		offsetof(xfs_agi_t, agi_newino),
		offsetof(xfs_agi_t, agi_dirino),
		offsetof(xfs_agi_t, agi_unlinked),
		offsetof(xfs_agi_t, agi_free_root),
		offsetof(xfs_agi_t, agi_free_level),
		offsetof(xfs_agi_t, agi_iblocks),
		sizeof(xfs_agi_t)
	};
#ifdef DEBUG
	struct xfs_agi		*agi = bp->b_addr;

	ASSERT(agi->agi_magicnum == cpu_to_be32(XFS_AGI_MAGIC));
#endif

	 
	if (fields & XFS_AGI_ALL_BITS_R1) {
		xfs_btree_offsets(fields, offsets, XFS_AGI_NUM_BITS_R1,
				  &first, &last);
		xfs_trans_log_buf(tp, bp, first, last);
	}

	 
	fields &= ~XFS_AGI_ALL_BITS_R1;
	if (fields) {
		xfs_btree_offsets(fields, offsets, XFS_AGI_NUM_BITS_R2,
				  &first, &last);
		xfs_trans_log_buf(tp, bp, first, last);
	}
}

static xfs_failaddr_t
xfs_agi_verify(
	struct xfs_buf		*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_agi		*agi = bp->b_addr;
	xfs_failaddr_t		fa;
	uint32_t		agi_seqno = be32_to_cpu(agi->agi_seqno);
	uint32_t		agi_length = be32_to_cpu(agi->agi_length);
	int			i;

	if (xfs_has_crc(mp)) {
		if (!uuid_equal(&agi->agi_uuid, &mp->m_sb.sb_meta_uuid))
			return __this_address;
		if (!xfs_log_check_lsn(mp, be64_to_cpu(agi->agi_lsn)))
			return __this_address;
	}

	 
	if (!xfs_verify_magic(bp, agi->agi_magicnum))
		return __this_address;
	if (!XFS_AGI_GOOD_VERSION(be32_to_cpu(agi->agi_versionnum)))
		return __this_address;

	fa = xfs_validate_ag_length(bp, agi_seqno, agi_length);
	if (fa)
		return fa;

	if (be32_to_cpu(agi->agi_level) < 1 ||
	    be32_to_cpu(agi->agi_level) > M_IGEO(mp)->inobt_maxlevels)
		return __this_address;

	if (xfs_has_finobt(mp) &&
	    (be32_to_cpu(agi->agi_free_level) < 1 ||
	     be32_to_cpu(agi->agi_free_level) > M_IGEO(mp)->inobt_maxlevels))
		return __this_address;

	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++) {
		if (agi->agi_unlinked[i] == cpu_to_be32(NULLAGINO))
			continue;
		if (!xfs_verify_ino(mp, be32_to_cpu(agi->agi_unlinked[i])))
			return __this_address;
	}

	return NULL;
}

static void
xfs_agi_read_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount *mp = bp->b_mount;
	xfs_failaddr_t	fa;

	if (xfs_has_crc(mp) &&
	    !xfs_buf_verify_cksum(bp, XFS_AGI_CRC_OFF))
		xfs_verifier_error(bp, -EFSBADCRC, __this_address);
	else {
		fa = xfs_agi_verify(bp);
		if (XFS_TEST_ERROR(fa, mp, XFS_ERRTAG_IALLOC_READ_AGI))
			xfs_verifier_error(bp, -EFSCORRUPTED, fa);
	}
}

static void
xfs_agi_write_verify(
	struct xfs_buf	*bp)
{
	struct xfs_mount	*mp = bp->b_mount;
	struct xfs_buf_log_item	*bip = bp->b_log_item;
	struct xfs_agi		*agi = bp->b_addr;
	xfs_failaddr_t		fa;

	fa = xfs_agi_verify(bp);
	if (fa) {
		xfs_verifier_error(bp, -EFSCORRUPTED, fa);
		return;
	}

	if (!xfs_has_crc(mp))
		return;

	if (bip)
		agi->agi_lsn = cpu_to_be64(bip->bli_item.li_lsn);
	xfs_buf_update_cksum(bp, XFS_AGI_CRC_OFF);
}

const struct xfs_buf_ops xfs_agi_buf_ops = {
	.name = "xfs_agi",
	.magic = { cpu_to_be32(XFS_AGI_MAGIC), cpu_to_be32(XFS_AGI_MAGIC) },
	.verify_read = xfs_agi_read_verify,
	.verify_write = xfs_agi_write_verify,
	.verify_struct = xfs_agi_verify,
};

 
int
xfs_read_agi(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		**agibpp)
{
	struct xfs_mount	*mp = pag->pag_mount;
	int			error;

	trace_xfs_read_agi(pag->pag_mount, pag->pag_agno);

	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, pag->pag_agno, XFS_AGI_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, agibpp, &xfs_agi_buf_ops);
	if (error)
		return error;
	if (tp)
		xfs_trans_buf_set_type(tp, *agibpp, XFS_BLFT_AGI_BUF);

	xfs_buf_set_ref(*agibpp, XFS_AGI_REF);
	return 0;
}

 
int
xfs_ialloc_read_agi(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		**agibpp)
{
	struct xfs_buf		*agibp;
	struct xfs_agi		*agi;
	int			error;

	trace_xfs_ialloc_read_agi(pag->pag_mount, pag->pag_agno);

	error = xfs_read_agi(pag, tp, &agibp);
	if (error)
		return error;

	agi = agibp->b_addr;
	if (!xfs_perag_initialised_agi(pag)) {
		pag->pagi_freecount = be32_to_cpu(agi->agi_freecount);
		pag->pagi_count = be32_to_cpu(agi->agi_count);
		set_bit(XFS_AGSTATE_AGI_INIT, &pag->pag_opstate);
	}

	 
	ASSERT(pag->pagi_freecount == be32_to_cpu(agi->agi_freecount) ||
		xfs_is_shutdown(pag->pag_mount));
	if (agibpp)
		*agibpp = agibp;
	else
		xfs_trans_brelse(tp, agibp);
	return 0;
}

 
STATIC int
xfs_ialloc_count_ondisk(
	struct xfs_btree_cur		*cur,
	xfs_agino_t			low,
	xfs_agino_t			high,
	unsigned int			*allocated)
{
	struct xfs_inobt_rec_incore	irec;
	unsigned int			ret = 0;
	int				has_record;
	int				error;

	error = xfs_inobt_lookup(cur, low, XFS_LOOKUP_LE, &has_record);
	if (error)
		return error;

	while (has_record) {
		unsigned int		i, hole_idx;

		error = xfs_inobt_get_rec(cur, &irec, &has_record);
		if (error)
			return error;
		if (irec.ir_startino > high)
			break;

		for (i = 0; i < XFS_INODES_PER_CHUNK; i++) {
			if (irec.ir_startino + i < low)
				continue;
			if (irec.ir_startino + i > high)
				break;

			hole_idx = i / XFS_INODES_PER_HOLEMASK_BIT;
			if (!(irec.ir_holemask & (1U << hole_idx)))
				ret++;
		}

		error = xfs_btree_increment(cur, 0, &has_record);
		if (error)
			return error;
	}

	*allocated = ret;
	return 0;
}

 
int
xfs_ialloc_has_inodes_at_extent(
	struct xfs_btree_cur	*cur,
	xfs_agblock_t		bno,
	xfs_extlen_t		len,
	enum xbtree_recpacking	*outcome)
{
	xfs_agino_t		agino;
	xfs_agino_t		last_agino;
	unsigned int		allocated;
	int			error;

	agino = XFS_AGB_TO_AGINO(cur->bc_mp, bno);
	last_agino = XFS_AGB_TO_AGINO(cur->bc_mp, bno + len) - 1;

	error = xfs_ialloc_count_ondisk(cur, agino, last_agino, &allocated);
	if (error)
		return error;

	if (allocated == 0)
		*outcome = XBTREE_RECPACKING_EMPTY;
	else if (allocated == last_agino - agino + 1)
		*outcome = XBTREE_RECPACKING_FULL;
	else
		*outcome = XBTREE_RECPACKING_SPARSE;
	return 0;
}

struct xfs_ialloc_count_inodes {
	xfs_agino_t			count;
	xfs_agino_t			freecount;
};

 
STATIC int
xfs_ialloc_count_inodes_rec(
	struct xfs_btree_cur		*cur,
	const union xfs_btree_rec	*rec,
	void				*priv)
{
	struct xfs_inobt_rec_incore	irec;
	struct xfs_ialloc_count_inodes	*ci = priv;
	xfs_failaddr_t			fa;

	xfs_inobt_btrec_to_irec(cur->bc_mp, rec, &irec);
	fa = xfs_inobt_check_irec(cur, &irec);
	if (fa)
		return xfs_inobt_complain_bad_rec(cur, fa, &irec);

	ci->count += irec.ir_count;
	ci->freecount += irec.ir_freecount;

	return 0;
}

 
int
xfs_ialloc_count_inodes(
	struct xfs_btree_cur		*cur,
	xfs_agino_t			*count,
	xfs_agino_t			*freecount)
{
	struct xfs_ialloc_count_inodes	ci = {0};
	int				error;

	ASSERT(cur->bc_btnum == XFS_BTNUM_INO);
	error = xfs_btree_query_all(cur, xfs_ialloc_count_inodes_rec, &ci);
	if (error)
		return error;

	*count = ci.count;
	*freecount = ci.freecount;
	return 0;
}

 
void
xfs_ialloc_setup_geometry(
	struct xfs_mount	*mp)
{
	struct xfs_sb		*sbp = &mp->m_sb;
	struct xfs_ino_geometry	*igeo = M_IGEO(mp);
	uint64_t		icount;
	uint			inodes;

	igeo->new_diflags2 = 0;
	if (xfs_has_bigtime(mp))
		igeo->new_diflags2 |= XFS_DIFLAG2_BIGTIME;
	if (xfs_has_large_extent_counts(mp))
		igeo->new_diflags2 |= XFS_DIFLAG2_NREXT64;

	 
	igeo->agino_log = sbp->sb_inopblog + sbp->sb_agblklog;
	igeo->inobt_mxr[0] = xfs_inobt_maxrecs(mp, sbp->sb_blocksize, 1);
	igeo->inobt_mxr[1] = xfs_inobt_maxrecs(mp, sbp->sb_blocksize, 0);
	igeo->inobt_mnr[0] = igeo->inobt_mxr[0] / 2;
	igeo->inobt_mnr[1] = igeo->inobt_mxr[1] / 2;

	igeo->ialloc_inos = max_t(uint16_t, XFS_INODES_PER_CHUNK,
			sbp->sb_inopblock);
	igeo->ialloc_blks = igeo->ialloc_inos >> sbp->sb_inopblog;

	if (sbp->sb_spino_align)
		igeo->ialloc_min_blks = sbp->sb_spino_align;
	else
		igeo->ialloc_min_blks = igeo->ialloc_blks;

	 
	inodes = (1LL << XFS_INO_AGINO_BITS(mp)) >> XFS_INODES_PER_CHUNK_LOG;
	igeo->inobt_maxlevels = xfs_btree_compute_maxlevels(igeo->inobt_mnr,
			inodes);
	ASSERT(igeo->inobt_maxlevels <= xfs_iallocbt_maxlevels_ondisk());

	 
	if (sbp->sb_imax_pct && igeo->ialloc_blks) {
		 
		icount = sbp->sb_dblocks * sbp->sb_imax_pct;
		do_div(icount, 100);
		do_div(icount, igeo->ialloc_blks);
		igeo->maxicount = XFS_FSB_TO_INO(mp,
				icount * igeo->ialloc_blks);
	} else {
		igeo->maxicount = 0;
	}

	 
	igeo->inode_cluster_size_raw = XFS_INODE_BIG_CLUSTER_SIZE;
	if (xfs_has_v3inodes(mp)) {
		int	new_size = igeo->inode_cluster_size_raw;

		new_size *= mp->m_sb.sb_inodesize / XFS_DINODE_MIN_SIZE;
		if (mp->m_sb.sb_inoalignmt >= XFS_B_TO_FSBT(mp, new_size))
			igeo->inode_cluster_size_raw = new_size;
	}

	 
	if (igeo->inode_cluster_size_raw > mp->m_sb.sb_blocksize)
		igeo->blocks_per_cluster = XFS_B_TO_FSBT(mp,
				igeo->inode_cluster_size_raw);
	else
		igeo->blocks_per_cluster = 1;
	igeo->inode_cluster_size = XFS_FSB_TO_B(mp, igeo->blocks_per_cluster);
	igeo->inodes_per_cluster = XFS_FSB_TO_INO(mp, igeo->blocks_per_cluster);

	 
	if (xfs_has_align(mp) &&
	    mp->m_sb.sb_inoalignmt >= igeo->blocks_per_cluster)
		igeo->cluster_align = mp->m_sb.sb_inoalignmt;
	else
		igeo->cluster_align = 1;
	igeo->inoalign_mask = igeo->cluster_align - 1;
	igeo->cluster_align_inodes = XFS_FSB_TO_INO(mp, igeo->cluster_align);

	 
	if (mp->m_dalign && igeo->inoalign_mask &&
	    !(mp->m_dalign & igeo->inoalign_mask))
		igeo->ialloc_align = mp->m_dalign;
	else
		igeo->ialloc_align = 0;
}

 
xfs_ino_t
xfs_ialloc_calc_rootino(
	struct xfs_mount	*mp,
	int			sunit)
{
	struct xfs_ino_geometry	*igeo = M_IGEO(mp);
	xfs_agblock_t		first_bno;

	 
	first_bno = howmany(4 * mp->m_sb.sb_sectsize, mp->m_sb.sb_blocksize);

	 
	first_bno += 2;

	 
	first_bno += 1;

	 
	first_bno += xfs_alloc_min_freelist(mp, NULL);

	 
	if (xfs_has_finobt(mp))
		first_bno++;

	 
	if (xfs_has_rmapbt(mp))
		first_bno++;

	 
	if (xfs_has_reflink(mp))
		first_bno++;

	 
	if (xfs_ag_contains_log(mp, 0))
		 first_bno += mp->m_sb.sb_logblocks;

	 
	if (xfs_has_dalign(mp) && igeo->ialloc_align > 0)
		first_bno = roundup(first_bno, sunit);
	else if (xfs_has_align(mp) &&
			mp->m_sb.sb_inoalignmt > 1)
		first_bno = roundup(first_bno, mp->m_sb.sb_inoalignmt);

	return XFS_AGINO_TO_INO(mp, 0, XFS_AGB_TO_AGINO(mp, first_bno));
}

 
int
xfs_ialloc_check_shrink(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	struct xfs_buf		*agibp,
	xfs_agblock_t		new_length)
{
	struct xfs_inobt_rec_incore rec;
	struct xfs_btree_cur	*cur;
	xfs_agino_t		agino;
	int			has;
	int			error;

	if (!xfs_has_sparseinodes(pag->pag_mount))
		return 0;

	cur = xfs_inobt_init_cursor(pag, tp, agibp, XFS_BTNUM_INO);

	 
	agino = XFS_AGB_TO_AGINO(pag->pag_mount, new_length);
	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &has);
	if (error || !has)
		goto out;

	error = xfs_inobt_get_rec(cur, &rec, &has);
	if (error)
		goto out;

	if (!has) {
		error = -EFSCORRUPTED;
		goto out;
	}

	 
	if (rec.ir_startino + XFS_INODES_PER_CHUNK > agino) {
		error = -ENOSPC;
		goto out;
	}
out:
	xfs_btree_del_cursor(cur, error);
	return error;
}

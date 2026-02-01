
 
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
#include "xfs_alloc.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc.h"
#include "xfs_ialloc_btree.h"
#include "xfs_rmap.h"
#include "xfs_rmap_btree.h"
#include "xfs_refcount_btree.h"
#include "xfs_ag.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/trace.h"
#include "scrub/repair.h"
#include "scrub/bitmap.h"
#include "scrub/reap.h"

 

 
int
xrep_superblock(
	struct xfs_scrub	*sc)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_buf		*bp;
	xfs_agnumber_t		agno;
	int			error;

	 
	agno = sc->sm->sm_agno;
	if (agno == 0)
		return -EOPNOTSUPP;

	error = xfs_sb_get_secondary(mp, sc->tp, agno, &bp);
	if (error)
		return error;

	 
	if (xchk_should_terminate(sc, &error))
		return error;

	 
	xfs_buf_zero(bp, 0, BBTOB(bp->b_length));
	xfs_sb_to_disk(bp->b_addr, &mp->m_sb);

	 
	if (xfs_has_crc(mp)) {
		struct xfs_dsb		*sb = bp->b_addr;

		sb->sb_features_incompat &=
				~cpu_to_be32(XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR);
		sb->sb_features_log_incompat = 0;
	}

	 
	xfs_trans_buf_set_type(sc->tp, bp, XFS_BLFT_SB_BUF);
	xfs_trans_log_buf(sc->tp, bp, 0, BBTOB(bp->b_length) - 1);
	return error;
}

 

struct xrep_agf_allocbt {
	struct xfs_scrub	*sc;
	xfs_agblock_t		freeblks;
	xfs_agblock_t		longest;
};

 
STATIC int
xrep_agf_walk_allocbt(
	struct xfs_btree_cur		*cur,
	const struct xfs_alloc_rec_incore *rec,
	void				*priv)
{
	struct xrep_agf_allocbt		*raa = priv;
	int				error = 0;

	if (xchk_should_terminate(raa->sc, &error))
		return error;

	raa->freeblks += rec->ar_blockcount;
	if (rec->ar_blockcount > raa->longest)
		raa->longest = rec->ar_blockcount;
	return error;
}

 
STATIC int
xrep_agf_check_agfl_block(
	struct xfs_mount	*mp,
	xfs_agblock_t		agbno,
	void			*priv)
{
	struct xfs_scrub	*sc = priv;

	if (!xfs_verify_agbno(sc->sa.pag, agbno))
		return -EFSCORRUPTED;
	return 0;
}

 
enum {
	XREP_AGF_BNOBT = 0,
	XREP_AGF_CNTBT,
	XREP_AGF_RMAPBT,
	XREP_AGF_REFCOUNTBT,
	XREP_AGF_END,
	XREP_AGF_MAX
};

 
static inline bool
xrep_check_btree_root(
	struct xfs_scrub		*sc,
	struct xrep_find_ag_btree	*fab)
{
	return xfs_verify_agbno(sc->sa.pag, fab->root) &&
	       fab->height <= fab->maxlevels;
}

 
STATIC int
xrep_agf_find_btrees(
	struct xfs_scrub		*sc,
	struct xfs_buf			*agf_bp,
	struct xrep_find_ag_btree	*fab,
	struct xfs_buf			*agfl_bp)
{
	struct xfs_agf			*old_agf = agf_bp->b_addr;
	int				error;

	 
	error = xrep_find_ag_btree_roots(sc, agf_bp, fab, agfl_bp);
	if (error)
		return error;

	 
	if (!xrep_check_btree_root(sc, &fab[XREP_AGF_BNOBT]) ||
	    !xrep_check_btree_root(sc, &fab[XREP_AGF_CNTBT]) ||
	    !xrep_check_btree_root(sc, &fab[XREP_AGF_RMAPBT]))
		return -EFSCORRUPTED;

	 
	if (fab[XREP_AGF_RMAPBT].root !=
	    be32_to_cpu(old_agf->agf_roots[XFS_BTNUM_RMAPi]))
		return -EFSCORRUPTED;

	 
	if (xfs_has_reflink(sc->mp) &&
	    !xrep_check_btree_root(sc, &fab[XREP_AGF_REFCOUNTBT]))
		return -EFSCORRUPTED;

	return 0;
}

 
STATIC void
xrep_agf_init_header(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agf_bp,
	struct xfs_agf		*old_agf)
{
	struct xfs_mount	*mp = sc->mp;
	struct xfs_perag	*pag = sc->sa.pag;
	struct xfs_agf		*agf = agf_bp->b_addr;

	memcpy(old_agf, agf, sizeof(*old_agf));
	memset(agf, 0, BBTOB(agf_bp->b_length));
	agf->agf_magicnum = cpu_to_be32(XFS_AGF_MAGIC);
	agf->agf_versionnum = cpu_to_be32(XFS_AGF_VERSION);
	agf->agf_seqno = cpu_to_be32(pag->pag_agno);
	agf->agf_length = cpu_to_be32(pag->block_count);
	agf->agf_flfirst = old_agf->agf_flfirst;
	agf->agf_fllast = old_agf->agf_fllast;
	agf->agf_flcount = old_agf->agf_flcount;
	if (xfs_has_crc(mp))
		uuid_copy(&agf->agf_uuid, &mp->m_sb.sb_meta_uuid);

	 
	ASSERT(xfs_perag_initialised_agf(pag));
	clear_bit(XFS_AGSTATE_AGF_INIT, &pag->pag_opstate);
}

 
STATIC void
xrep_agf_set_roots(
	struct xfs_scrub		*sc,
	struct xfs_agf			*agf,
	struct xrep_find_ag_btree	*fab)
{
	agf->agf_roots[XFS_BTNUM_BNOi] =
			cpu_to_be32(fab[XREP_AGF_BNOBT].root);
	agf->agf_levels[XFS_BTNUM_BNOi] =
			cpu_to_be32(fab[XREP_AGF_BNOBT].height);

	agf->agf_roots[XFS_BTNUM_CNTi] =
			cpu_to_be32(fab[XREP_AGF_CNTBT].root);
	agf->agf_levels[XFS_BTNUM_CNTi] =
			cpu_to_be32(fab[XREP_AGF_CNTBT].height);

	agf->agf_roots[XFS_BTNUM_RMAPi] =
			cpu_to_be32(fab[XREP_AGF_RMAPBT].root);
	agf->agf_levels[XFS_BTNUM_RMAPi] =
			cpu_to_be32(fab[XREP_AGF_RMAPBT].height);

	if (xfs_has_reflink(sc->mp)) {
		agf->agf_refcount_root =
				cpu_to_be32(fab[XREP_AGF_REFCOUNTBT].root);
		agf->agf_refcount_level =
				cpu_to_be32(fab[XREP_AGF_REFCOUNTBT].height);
	}
}

 
STATIC int
xrep_agf_calc_from_btrees(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agf_bp)
{
	struct xrep_agf_allocbt	raa = { .sc = sc };
	struct xfs_btree_cur	*cur = NULL;
	struct xfs_agf		*agf = agf_bp->b_addr;
	struct xfs_mount	*mp = sc->mp;
	xfs_agblock_t		btreeblks;
	xfs_agblock_t		blocks;
	int			error;

	 
	cur = xfs_allocbt_init_cursor(mp, sc->tp, agf_bp,
			sc->sa.pag, XFS_BTNUM_BNO);
	error = xfs_alloc_query_all(cur, xrep_agf_walk_allocbt, &raa);
	if (error)
		goto err;
	error = xfs_btree_count_blocks(cur, &blocks);
	if (error)
		goto err;
	xfs_btree_del_cursor(cur, error);
	btreeblks = blocks - 1;
	agf->agf_freeblks = cpu_to_be32(raa.freeblks);
	agf->agf_longest = cpu_to_be32(raa.longest);

	 
	cur = xfs_allocbt_init_cursor(mp, sc->tp, agf_bp,
			sc->sa.pag, XFS_BTNUM_CNT);
	error = xfs_btree_count_blocks(cur, &blocks);
	if (error)
		goto err;
	xfs_btree_del_cursor(cur, error);
	btreeblks += blocks - 1;

	 
	cur = xfs_rmapbt_init_cursor(mp, sc->tp, agf_bp, sc->sa.pag);
	error = xfs_btree_count_blocks(cur, &blocks);
	if (error)
		goto err;
	xfs_btree_del_cursor(cur, error);
	agf->agf_rmap_blocks = cpu_to_be32(blocks);
	btreeblks += blocks - 1;

	agf->agf_btreeblks = cpu_to_be32(btreeblks);

	 
	if (xfs_has_reflink(mp)) {
		cur = xfs_refcountbt_init_cursor(mp, sc->tp, agf_bp,
				sc->sa.pag);
		error = xfs_btree_count_blocks(cur, &blocks);
		if (error)
			goto err;
		xfs_btree_del_cursor(cur, error);
		agf->agf_refcount_blocks = cpu_to_be32(blocks);
	}

	return 0;
err:
	xfs_btree_del_cursor(cur, error);
	return error;
}

 
STATIC int
xrep_agf_commit_new(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agf_bp)
{
	struct xfs_perag	*pag;
	struct xfs_agf		*agf = agf_bp->b_addr;

	 
	xfs_force_summary_recalc(sc->mp);

	 
	xfs_trans_buf_set_type(sc->tp, agf_bp, XFS_BLFT_AGF_BUF);
	xfs_trans_log_buf(sc->tp, agf_bp, 0, BBTOB(agf_bp->b_length) - 1);

	 
	pag = sc->sa.pag;
	pag->pagf_btreeblks = be32_to_cpu(agf->agf_btreeblks);
	pag->pagf_freeblks = be32_to_cpu(agf->agf_freeblks);
	pag->pagf_longest = be32_to_cpu(agf->agf_longest);
	pag->pagf_levels[XFS_BTNUM_BNOi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNOi]);
	pag->pagf_levels[XFS_BTNUM_CNTi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNTi]);
	pag->pagf_levels[XFS_BTNUM_RMAPi] =
			be32_to_cpu(agf->agf_levels[XFS_BTNUM_RMAPi]);
	pag->pagf_refcount_level = be32_to_cpu(agf->agf_refcount_level);
	set_bit(XFS_AGSTATE_AGF_INIT, &pag->pag_opstate);

	return 0;
}

 
int
xrep_agf(
	struct xfs_scrub		*sc)
{
	struct xrep_find_ag_btree	fab[XREP_AGF_MAX] = {
		[XREP_AGF_BNOBT] = {
			.rmap_owner = XFS_RMAP_OWN_AG,
			.buf_ops = &xfs_bnobt_buf_ops,
			.maxlevels = sc->mp->m_alloc_maxlevels,
		},
		[XREP_AGF_CNTBT] = {
			.rmap_owner = XFS_RMAP_OWN_AG,
			.buf_ops = &xfs_cntbt_buf_ops,
			.maxlevels = sc->mp->m_alloc_maxlevels,
		},
		[XREP_AGF_RMAPBT] = {
			.rmap_owner = XFS_RMAP_OWN_AG,
			.buf_ops = &xfs_rmapbt_buf_ops,
			.maxlevels = sc->mp->m_rmap_maxlevels,
		},
		[XREP_AGF_REFCOUNTBT] = {
			.rmap_owner = XFS_RMAP_OWN_REFC,
			.buf_ops = &xfs_refcountbt_buf_ops,
			.maxlevels = sc->mp->m_refc_maxlevels,
		},
		[XREP_AGF_END] = {
			.buf_ops = NULL,
		},
	};
	struct xfs_agf			old_agf;
	struct xfs_mount		*mp = sc->mp;
	struct xfs_buf			*agf_bp;
	struct xfs_buf			*agfl_bp;
	struct xfs_agf			*agf;
	int				error;

	 
	if (!xfs_has_rmapbt(mp))
		return -EOPNOTSUPP;

	 
	error = xfs_trans_read_buf(mp, sc->tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, sc->sa.pag->pag_agno,
						XFS_AGF_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, &agf_bp, NULL);
	if (error)
		return error;
	agf_bp->b_ops = &xfs_agf_buf_ops;
	agf = agf_bp->b_addr;

	 
	error = xfs_alloc_read_agfl(sc->sa.pag, sc->tp, &agfl_bp);
	if (error)
		return error;

	 
	error = xfs_agfl_walk(sc->mp, agf_bp->b_addr, agfl_bp,
			xrep_agf_check_agfl_block, sc);
	if (error)
		return error;

	 
	error = xrep_agf_find_btrees(sc, agf_bp, fab, agfl_bp);
	if (error)
		return error;

	 
	if (xchk_should_terminate(sc, &error))
		return error;

	 
	xrep_agf_init_header(sc, agf_bp, &old_agf);
	xrep_agf_set_roots(sc, agf, fab);
	error = xrep_agf_calc_from_btrees(sc, agf_bp);
	if (error)
		goto out_revert;

	 
	return xrep_agf_commit_new(sc, agf_bp);

out_revert:
	 
	clear_bit(XFS_AGSTATE_AGF_INIT, &sc->sa.pag->pag_opstate);
	memcpy(agf, &old_agf, sizeof(old_agf));
	return error;
}

 

struct xrep_agfl {
	 
	struct xagb_bitmap	crossed;

	 
	struct xagb_bitmap	agmetablocks;

	 
	struct xagb_bitmap	*freesp;

	 
	struct xfs_btree_cur	*rmap_cur;

	struct xfs_scrub	*sc;
};

 
STATIC int
xrep_agfl_walk_rmap(
	struct xfs_btree_cur	*cur,
	const struct xfs_rmap_irec *rec,
	void			*priv)
{
	struct xrep_agfl	*ra = priv;
	int			error = 0;

	if (xchk_should_terminate(ra->sc, &error))
		return error;

	 
	if (rec->rm_owner == XFS_RMAP_OWN_AG) {
		error = xagb_bitmap_set(ra->freesp, rec->rm_startblock,
				rec->rm_blockcount);
		if (error)
			return error;
	}

	return xagb_bitmap_set_btcur_path(&ra->agmetablocks, cur);
}

 
STATIC int
xrep_agfl_check_extent(
	uint64_t		start,
	uint64_t		len,
	void			*priv)
{
	struct xrep_agfl	*ra = priv;
	xfs_agblock_t		agbno = start;
	xfs_agblock_t		last_agbno = agbno + len - 1;
	int			error;

	while (agbno <= last_agbno) {
		bool		other_owners;

		error = xfs_rmap_has_other_keys(ra->rmap_cur, agbno, 1,
				&XFS_RMAP_OINFO_AG, &other_owners);
		if (error)
			return error;

		if (other_owners) {
			error = xagb_bitmap_set(&ra->crossed, agbno, 1);
			if (error)
				return error;
		}

		if (xchk_should_terminate(ra->sc, &error))
			return error;
		agbno++;
	}

	return 0;
}

 
STATIC int
xrep_agfl_collect_blocks(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agf_bp,
	struct xagb_bitmap	*agfl_extents,
	xfs_agblock_t		*flcount)
{
	struct xrep_agfl	ra;
	struct xfs_mount	*mp = sc->mp;
	struct xfs_btree_cur	*cur;
	int			error;

	ra.sc = sc;
	ra.freesp = agfl_extents;
	xagb_bitmap_init(&ra.agmetablocks);
	xagb_bitmap_init(&ra.crossed);

	 
	cur = xfs_rmapbt_init_cursor(mp, sc->tp, agf_bp, sc->sa.pag);
	error = xfs_rmap_query_all(cur, xrep_agfl_walk_rmap, &ra);
	xfs_btree_del_cursor(cur, error);
	if (error)
		goto out_bmp;

	 
	cur = xfs_allocbt_init_cursor(mp, sc->tp, agf_bp,
			sc->sa.pag, XFS_BTNUM_BNO);
	error = xagb_bitmap_set_btblocks(&ra.agmetablocks, cur);
	xfs_btree_del_cursor(cur, error);
	if (error)
		goto out_bmp;

	 
	cur = xfs_allocbt_init_cursor(mp, sc->tp, agf_bp,
			sc->sa.pag, XFS_BTNUM_CNT);
	error = xagb_bitmap_set_btblocks(&ra.agmetablocks, cur);
	xfs_btree_del_cursor(cur, error);
	if (error)
		goto out_bmp;

	 
	error = xagb_bitmap_disunion(agfl_extents, &ra.agmetablocks);
	if (error)
		goto out_bmp;

	 
	ra.rmap_cur = xfs_rmapbt_init_cursor(mp, sc->tp, agf_bp, sc->sa.pag);
	error = xagb_bitmap_walk(agfl_extents, xrep_agfl_check_extent, &ra);
	xfs_btree_del_cursor(ra.rmap_cur, error);
	if (error)
		goto out_bmp;
	error = xagb_bitmap_disunion(agfl_extents, &ra.crossed);
	if (error)
		goto out_bmp;

	 
	*flcount = min_t(uint64_t, xagb_bitmap_hweight(agfl_extents),
			 xfs_agfl_size(mp));

out_bmp:
	xagb_bitmap_destroy(&ra.crossed);
	xagb_bitmap_destroy(&ra.agmetablocks);
	return error;
}

 
STATIC void
xrep_agfl_update_agf(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agf_bp,
	xfs_agblock_t		flcount)
{
	struct xfs_agf		*agf = agf_bp->b_addr;

	ASSERT(flcount <= xfs_agfl_size(sc->mp));

	 
	xfs_force_summary_recalc(sc->mp);

	 
	if (xfs_perag_initialised_agf(sc->sa.pag)) {
		sc->sa.pag->pagf_flcount = flcount;
		clear_bit(XFS_AGSTATE_AGFL_NEEDS_RESET,
				&sc->sa.pag->pag_opstate);
	}
	agf->agf_flfirst = cpu_to_be32(0);
	agf->agf_flcount = cpu_to_be32(flcount);
	if (flcount)
		agf->agf_fllast = cpu_to_be32(flcount - 1);
	else
		agf->agf_fllast = cpu_to_be32(xfs_agfl_size(sc->mp) - 1);

	xfs_alloc_log_agf(sc->tp, agf_bp,
			XFS_AGF_FLFIRST | XFS_AGF_FLLAST | XFS_AGF_FLCOUNT);
}

struct xrep_agfl_fill {
	struct xagb_bitmap	used_extents;
	struct xfs_scrub	*sc;
	__be32			*agfl_bno;
	xfs_agblock_t		flcount;
	unsigned int		fl_off;
};

 
static int
xrep_agfl_fill(
	uint64_t		start,
	uint64_t		len,
	void			*priv)
{
	struct xrep_agfl_fill	*af = priv;
	struct xfs_scrub	*sc = af->sc;
	xfs_agblock_t		agbno = start;
	int			error;

	trace_xrep_agfl_insert(sc->sa.pag, agbno, len);

	while (agbno < start + len && af->fl_off < af->flcount)
		af->agfl_bno[af->fl_off++] = cpu_to_be32(agbno++);

	error = xagb_bitmap_set(&af->used_extents, start, agbno - 1);
	if (error)
		return error;

	if (af->fl_off == af->flcount)
		return -ECANCELED;

	return 0;
}

 
STATIC int
xrep_agfl_init_header(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agfl_bp,
	struct xagb_bitmap	*agfl_extents,
	xfs_agblock_t		flcount)
{
	struct xrep_agfl_fill	af = {
		.sc		= sc,
		.flcount	= flcount,
	};
	struct xfs_mount	*mp = sc->mp;
	struct xfs_agfl		*agfl;
	int			error;

	ASSERT(flcount <= xfs_agfl_size(mp));

	 
	agfl = XFS_BUF_TO_AGFL(agfl_bp);
	memset(agfl, 0xFF, BBTOB(agfl_bp->b_length));
	agfl->agfl_magicnum = cpu_to_be32(XFS_AGFL_MAGIC);
	agfl->agfl_seqno = cpu_to_be32(sc->sa.pag->pag_agno);
	uuid_copy(&agfl->agfl_uuid, &mp->m_sb.sb_meta_uuid);

	 
	xagb_bitmap_init(&af.used_extents);
	af.agfl_bno = xfs_buf_to_agfl_bno(agfl_bp),
	xagb_bitmap_walk(agfl_extents, xrep_agfl_fill, &af);
	error = xagb_bitmap_disunion(agfl_extents, &af.used_extents);
	if (error)
		return error;

	 
	xfs_trans_buf_set_type(sc->tp, agfl_bp, XFS_BLFT_AGFL_BUF);
	xfs_trans_log_buf(sc->tp, agfl_bp, 0, BBTOB(agfl_bp->b_length) - 1);
	xagb_bitmap_destroy(&af.used_extents);
	return 0;
}

 
int
xrep_agfl(
	struct xfs_scrub	*sc)
{
	struct xagb_bitmap	agfl_extents;
	struct xfs_mount	*mp = sc->mp;
	struct xfs_buf		*agf_bp;
	struct xfs_buf		*agfl_bp;
	xfs_agblock_t		flcount;
	int			error;

	 
	if (!xfs_has_rmapbt(mp))
		return -EOPNOTSUPP;

	xagb_bitmap_init(&agfl_extents);

	 
	error = xfs_alloc_read_agf(sc->sa.pag, sc->tp, 0, &agf_bp);
	if (error)
		return error;

	 
	error = xfs_trans_read_buf(mp, sc->tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, sc->sa.pag->pag_agno,
						XFS_AGFL_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, &agfl_bp, NULL);
	if (error)
		return error;
	agfl_bp->b_ops = &xfs_agfl_buf_ops;

	 
	error = xrep_agfl_collect_blocks(sc, agf_bp, &agfl_extents, &flcount);
	if (error)
		goto err;

	 
	if (xchk_should_terminate(sc, &error))
		goto err;

	 
	xrep_agfl_update_agf(sc, agf_bp, flcount);
	error = xrep_agfl_init_header(sc, agfl_bp, &agfl_extents, flcount);
	if (error)
		goto err;

	 
	sc->sa.agf_bp = agf_bp;
	error = xrep_roll_ag_trans(sc);
	if (error)
		goto err;

	 
	error = xrep_reap_agblocks(sc, &agfl_extents, &XFS_RMAP_OINFO_AG,
			XFS_AG_RESV_AGFL);
err:
	xagb_bitmap_destroy(&agfl_extents);
	return error;
}

 

 
enum {
	XREP_AGI_INOBT = 0,
	XREP_AGI_FINOBT,
	XREP_AGI_END,
	XREP_AGI_MAX
};

 
STATIC int
xrep_agi_find_btrees(
	struct xfs_scrub		*sc,
	struct xrep_find_ag_btree	*fab)
{
	struct xfs_buf			*agf_bp;
	struct xfs_mount		*mp = sc->mp;
	int				error;

	 
	error = xfs_alloc_read_agf(sc->sa.pag, sc->tp, 0, &agf_bp);
	if (error)
		return error;

	 
	error = xrep_find_ag_btree_roots(sc, agf_bp, fab, NULL);
	if (error)
		return error;

	 
	if (!xrep_check_btree_root(sc, &fab[XREP_AGI_INOBT]))
		return -EFSCORRUPTED;

	 
	if (xfs_has_finobt(mp) &&
	    !xrep_check_btree_root(sc, &fab[XREP_AGI_FINOBT]))
		return -EFSCORRUPTED;

	return 0;
}

 
STATIC void
xrep_agi_init_header(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agi_bp,
	struct xfs_agi		*old_agi)
{
	struct xfs_agi		*agi = agi_bp->b_addr;
	struct xfs_perag	*pag = sc->sa.pag;
	struct xfs_mount	*mp = sc->mp;

	memcpy(old_agi, agi, sizeof(*old_agi));
	memset(agi, 0, BBTOB(agi_bp->b_length));
	agi->agi_magicnum = cpu_to_be32(XFS_AGI_MAGIC);
	agi->agi_versionnum = cpu_to_be32(XFS_AGI_VERSION);
	agi->agi_seqno = cpu_to_be32(pag->pag_agno);
	agi->agi_length = cpu_to_be32(pag->block_count);
	agi->agi_newino = cpu_to_be32(NULLAGINO);
	agi->agi_dirino = cpu_to_be32(NULLAGINO);
	if (xfs_has_crc(mp))
		uuid_copy(&agi->agi_uuid, &mp->m_sb.sb_meta_uuid);

	 
	memcpy(&agi->agi_unlinked, &old_agi->agi_unlinked,
			sizeof(agi->agi_unlinked));

	 
	ASSERT(xfs_perag_initialised_agi(pag));
	clear_bit(XFS_AGSTATE_AGI_INIT, &pag->pag_opstate);
}

 
STATIC void
xrep_agi_set_roots(
	struct xfs_scrub		*sc,
	struct xfs_agi			*agi,
	struct xrep_find_ag_btree	*fab)
{
	agi->agi_root = cpu_to_be32(fab[XREP_AGI_INOBT].root);
	agi->agi_level = cpu_to_be32(fab[XREP_AGI_INOBT].height);

	if (xfs_has_finobt(sc->mp)) {
		agi->agi_free_root = cpu_to_be32(fab[XREP_AGI_FINOBT].root);
		agi->agi_free_level = cpu_to_be32(fab[XREP_AGI_FINOBT].height);
	}
}

 
STATIC int
xrep_agi_calc_from_btrees(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agi_bp)
{
	struct xfs_btree_cur	*cur;
	struct xfs_agi		*agi = agi_bp->b_addr;
	struct xfs_mount	*mp = sc->mp;
	xfs_agino_t		count;
	xfs_agino_t		freecount;
	int			error;

	cur = xfs_inobt_init_cursor(sc->sa.pag, sc->tp, agi_bp, XFS_BTNUM_INO);
	error = xfs_ialloc_count_inodes(cur, &count, &freecount);
	if (error)
		goto err;
	if (xfs_has_inobtcounts(mp)) {
		xfs_agblock_t	blocks;

		error = xfs_btree_count_blocks(cur, &blocks);
		if (error)
			goto err;
		agi->agi_iblocks = cpu_to_be32(blocks);
	}
	xfs_btree_del_cursor(cur, error);

	agi->agi_count = cpu_to_be32(count);
	agi->agi_freecount = cpu_to_be32(freecount);

	if (xfs_has_finobt(mp) && xfs_has_inobtcounts(mp)) {
		xfs_agblock_t	blocks;

		cur = xfs_inobt_init_cursor(sc->sa.pag, sc->tp, agi_bp,
				XFS_BTNUM_FINO);
		error = xfs_btree_count_blocks(cur, &blocks);
		if (error)
			goto err;
		xfs_btree_del_cursor(cur, error);
		agi->agi_fblocks = cpu_to_be32(blocks);
	}

	return 0;
err:
	xfs_btree_del_cursor(cur, error);
	return error;
}

 
STATIC int
xrep_agi_commit_new(
	struct xfs_scrub	*sc,
	struct xfs_buf		*agi_bp)
{
	struct xfs_perag	*pag;
	struct xfs_agi		*agi = agi_bp->b_addr;

	 
	xfs_force_summary_recalc(sc->mp);

	 
	xfs_trans_buf_set_type(sc->tp, agi_bp, XFS_BLFT_AGI_BUF);
	xfs_trans_log_buf(sc->tp, agi_bp, 0, BBTOB(agi_bp->b_length) - 1);

	 
	pag = sc->sa.pag;
	pag->pagi_count = be32_to_cpu(agi->agi_count);
	pag->pagi_freecount = be32_to_cpu(agi->agi_freecount);
	set_bit(XFS_AGSTATE_AGI_INIT, &pag->pag_opstate);

	return 0;
}

 
int
xrep_agi(
	struct xfs_scrub		*sc)
{
	struct xrep_find_ag_btree	fab[XREP_AGI_MAX] = {
		[XREP_AGI_INOBT] = {
			.rmap_owner = XFS_RMAP_OWN_INOBT,
			.buf_ops = &xfs_inobt_buf_ops,
			.maxlevels = M_IGEO(sc->mp)->inobt_maxlevels,
		},
		[XREP_AGI_FINOBT] = {
			.rmap_owner = XFS_RMAP_OWN_INOBT,
			.buf_ops = &xfs_finobt_buf_ops,
			.maxlevels = M_IGEO(sc->mp)->inobt_maxlevels,
		},
		[XREP_AGI_END] = {
			.buf_ops = NULL
		},
	};
	struct xfs_agi			old_agi;
	struct xfs_mount		*mp = sc->mp;
	struct xfs_buf			*agi_bp;
	struct xfs_agi			*agi;
	int				error;

	 
	if (!xfs_has_rmapbt(mp))
		return -EOPNOTSUPP;

	 
	error = xfs_trans_read_buf(mp, sc->tp, mp->m_ddev_targp,
			XFS_AG_DADDR(mp, sc->sa.pag->pag_agno,
						XFS_AGI_DADDR(mp)),
			XFS_FSS_TO_BB(mp, 1), 0, &agi_bp, NULL);
	if (error)
		return error;
	agi_bp->b_ops = &xfs_agi_buf_ops;
	agi = agi_bp->b_addr;

	 
	error = xrep_agi_find_btrees(sc, fab);
	if (error)
		return error;

	 
	if (xchk_should_terminate(sc, &error))
		return error;

	 
	xrep_agi_init_header(sc, agi_bp, &old_agi);
	xrep_agi_set_roots(sc, agi, fab);
	error = xrep_agi_calc_from_btrees(sc, agi_bp);
	if (error)
		goto out_revert;

	 
	return xrep_agi_commit_new(sc, agi_bp);

out_revert:
	 
	clear_bit(XFS_AGSTATE_AGI_INIT, &sc->sa.pag->pag_opstate);
	memcpy(agi, &old_agi, sizeof(old_agi));
	return error;
}

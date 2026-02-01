
 
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
#include "xfs_ialloc.h"
#include "xfs_ialloc_btree.h"
#include "xfs_icache.h"
#include "xfs_rmap.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/btree.h"
#include "scrub/trace.h"
#include "xfs_ag.h"

 
int
xchk_setup_ag_iallocbt(
	struct xfs_scrub	*sc)
{
	if (xchk_need_intent_drain(sc))
		xchk_fsgates_enable(sc, XCHK_FSGATES_DRAIN);
	return xchk_setup_ag_btree(sc, sc->flags & XCHK_TRY_HARDER);
}

 

struct xchk_iallocbt {
	 
	unsigned long long	inodes;

	 
	xfs_agino_t		next_startino;

	 
	xfs_agino_t		next_cluster_ino;
};

 
STATIC int
xchk_inobt_xref_finobt(
	struct xfs_scrub	*sc,
	struct xfs_inobt_rec_incore *irec,
	xfs_agino_t		agino,
	bool			free,
	bool			hole)
{
	struct xfs_inobt_rec_incore frec;
	struct xfs_btree_cur	*cur = sc->sa.fino_cur;
	bool			ffree, fhole;
	unsigned int		frec_idx, fhole_idx;
	int			has_record;
	int			error;

	ASSERT(cur->bc_btnum == XFS_BTNUM_FINO);

	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &has_record);
	if (error)
		return error;
	if (!has_record)
		goto no_record;

	error = xfs_inobt_get_rec(cur, &frec, &has_record);
	if (!has_record)
		return -EFSCORRUPTED;

	if (frec.ir_startino + XFS_INODES_PER_CHUNK <= agino)
		goto no_record;

	 
	frec_idx = agino - frec.ir_startino;
	ffree = frec.ir_free & (1ULL << frec_idx);
	fhole_idx = frec_idx / XFS_INODES_PER_HOLEMASK_BIT;
	fhole = frec.ir_holemask & (1U << fhole_idx);

	if (ffree != free)
		xchk_btree_xref_set_corrupt(sc, cur, 0);
	if (fhole != hole)
		xchk_btree_xref_set_corrupt(sc, cur, 0);
	return 0;

no_record:
	 
	if (irec->ir_free == 0)
		return 0;

	 
	if (irec->ir_free == XFS_INOBT_ALL_FREE)
		return 0;

	 
	if (hole)
		return 0;

	 
	if (!free)
		return 0;

	xchk_btree_xref_set_corrupt(sc, cur, 0);
	return 0;
}

 
STATIC void
xchk_inobt_chunk_xref_finobt(
	struct xfs_scrub		*sc,
	struct xfs_inobt_rec_incore	*irec,
	xfs_agino_t			agino,
	unsigned int			nr_inodes)
{
	xfs_agino_t			i;
	unsigned int			rec_idx;
	int				error;

	ASSERT(sc->sm->sm_type == XFS_SCRUB_TYPE_INOBT);

	if (!sc->sa.fino_cur || xchk_skip_xref(sc->sm))
		return;

	for (i = agino, rec_idx = agino - irec->ir_startino;
	     i < agino + nr_inodes;
	     i++, rec_idx++) {
		bool			free, hole;
		unsigned int		hole_idx;

		free = irec->ir_free & (1ULL << rec_idx);
		hole_idx = rec_idx / XFS_INODES_PER_HOLEMASK_BIT;
		hole = irec->ir_holemask & (1U << hole_idx);

		error = xchk_inobt_xref_finobt(sc, irec, i, free, hole);
		if (!xchk_should_check_xref(sc, &error, &sc->sa.fino_cur))
			return;
	}
}

 
STATIC int
xchk_finobt_xref_inobt(
	struct xfs_scrub	*sc,
	struct xfs_inobt_rec_incore *frec,
	xfs_agino_t		agino,
	bool			ffree,
	bool			fhole)
{
	struct xfs_inobt_rec_incore irec;
	struct xfs_btree_cur	*cur = sc->sa.ino_cur;
	bool			free, hole;
	unsigned int		rec_idx, hole_idx;
	int			has_record;
	int			error;

	ASSERT(cur->bc_btnum == XFS_BTNUM_INO);

	error = xfs_inobt_lookup(cur, agino, XFS_LOOKUP_LE, &has_record);
	if (error)
		return error;
	if (!has_record)
		goto no_record;

	error = xfs_inobt_get_rec(cur, &irec, &has_record);
	if (!has_record)
		return -EFSCORRUPTED;

	if (irec.ir_startino + XFS_INODES_PER_CHUNK <= agino)
		goto no_record;

	 
	rec_idx = agino - irec.ir_startino;
	free = irec.ir_free & (1ULL << rec_idx);
	hole_idx = rec_idx / XFS_INODES_PER_HOLEMASK_BIT;
	hole = irec.ir_holemask & (1U << hole_idx);

	if (ffree != free)
		xchk_btree_xref_set_corrupt(sc, cur, 0);
	if (fhole != hole)
		xchk_btree_xref_set_corrupt(sc, cur, 0);
	return 0;

no_record:
	 
	xchk_btree_xref_set_corrupt(sc, cur, 0);
	return 0;
}

 
STATIC void
xchk_finobt_chunk_xref_inobt(
	struct xfs_scrub		*sc,
	struct xfs_inobt_rec_incore	*frec,
	xfs_agino_t			agino,
	unsigned int			nr_inodes)
{
	xfs_agino_t			i;
	unsigned int			rec_idx;
	int				error;

	ASSERT(sc->sm->sm_type == XFS_SCRUB_TYPE_FINOBT);

	if (!sc->sa.ino_cur || xchk_skip_xref(sc->sm))
		return;

	for (i = agino, rec_idx = agino - frec->ir_startino;
	     i < agino + nr_inodes;
	     i++, rec_idx++) {
		bool			ffree, fhole;
		unsigned int		hole_idx;

		ffree = frec->ir_free & (1ULL << rec_idx);
		hole_idx = rec_idx / XFS_INODES_PER_HOLEMASK_BIT;
		fhole = frec->ir_holemask & (1U << hole_idx);

		error = xchk_finobt_xref_inobt(sc, frec, i, ffree, fhole);
		if (!xchk_should_check_xref(sc, &error, &sc->sa.ino_cur))
			return;
	}
}

 
STATIC bool
xchk_iallocbt_chunk(
	struct xchk_btree		*bs,
	struct xfs_inobt_rec_incore	*irec,
	xfs_agino_t			agino,
	unsigned int			nr_inodes)
{
	struct xfs_scrub		*sc = bs->sc;
	struct xfs_mount		*mp = bs->cur->bc_mp;
	struct xfs_perag		*pag = bs->cur->bc_ag.pag;
	xfs_agblock_t			agbno;
	xfs_extlen_t			len;

	agbno = XFS_AGINO_TO_AGBNO(mp, agino);
	len = XFS_B_TO_FSB(mp, nr_inodes * mp->m_sb.sb_inodesize);

	if (!xfs_verify_agbext(pag, agbno, len))
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);

	if (bs->sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return false;

	xchk_xref_is_used_space(sc, agbno, len);
	if (sc->sm->sm_type == XFS_SCRUB_TYPE_INOBT)
		xchk_inobt_chunk_xref_finobt(sc, irec, agino, nr_inodes);
	else
		xchk_finobt_chunk_xref_inobt(sc, irec, agino, nr_inodes);
	xchk_xref_is_only_owned_by(sc, agbno, len, &XFS_RMAP_OINFO_INODES);
	xchk_xref_is_not_shared(sc, agbno, len);
	xchk_xref_is_not_cow_staging(sc, agbno, len);
	return true;
}

 
STATIC int
xchk_iallocbt_check_cluster_ifree(
	struct xchk_btree		*bs,
	struct xfs_inobt_rec_incore	*irec,
	unsigned int			irec_ino,
	struct xfs_dinode		*dip)
{
	struct xfs_mount		*mp = bs->cur->bc_mp;
	xfs_ino_t			fsino;
	xfs_agino_t			agino;
	bool				irec_free;
	bool				ino_inuse;
	bool				freemask_ok;
	int				error = 0;

	if (xchk_should_terminate(bs->sc, &error))
		return error;

	 
	agino = irec->ir_startino + irec_ino;
	fsino = XFS_AGINO_TO_INO(mp, bs->cur->bc_ag.pag->pag_agno, agino);
	irec_free = (irec->ir_free & XFS_INOBT_MASK(irec_ino));

	if (be16_to_cpu(dip->di_magic) != XFS_DINODE_MAGIC ||
	    (dip->di_version >= 3 && be64_to_cpu(dip->di_ino) != fsino)) {
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		goto out;
	}

	error = xchk_inode_is_allocated(bs->sc, agino, &ino_inuse);
	if (error == -ENODATA) {
		 
		freemask_ok = irec_free ^ !!(dip->di_mode);
		if (!(bs->sc->flags & XCHK_TRY_HARDER) && !freemask_ok)
			return -EDEADLOCK;
	} else if (error < 0) {
		 
		goto out;
	} else {
		 
		freemask_ok = irec_free ^ ino_inuse;
	}
	if (!freemask_ok)
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
out:
	return 0;
}

 
STATIC int
xchk_iallocbt_check_cluster(
	struct xchk_btree		*bs,
	struct xfs_inobt_rec_incore	*irec,
	unsigned int			cluster_base)
{
	struct xfs_imap			imap;
	struct xfs_mount		*mp = bs->cur->bc_mp;
	struct xfs_buf			*cluster_bp;
	unsigned int			nr_inodes;
	xfs_agnumber_t			agno = bs->cur->bc_ag.pag->pag_agno;
	xfs_agblock_t			agbno;
	unsigned int			cluster_index;
	uint16_t			cluster_mask = 0;
	uint16_t			ir_holemask;
	int				error = 0;

	nr_inodes = min_t(unsigned int, XFS_INODES_PER_CHUNK,
			M_IGEO(mp)->inodes_per_cluster);

	 
	agbno = XFS_AGINO_TO_AGBNO(mp, irec->ir_startino + cluster_base);

	 
	for (cluster_index = 0;
	     cluster_index < nr_inodes;
	     cluster_index += XFS_INODES_PER_HOLEMASK_BIT)
		cluster_mask |= XFS_INOBT_MASK((cluster_base + cluster_index) /
				XFS_INODES_PER_HOLEMASK_BIT);

	 
	ir_holemask = (irec->ir_holemask & cluster_mask);
	imap.im_blkno = XFS_AGB_TO_DADDR(mp, agno, agbno);
	imap.im_len = XFS_FSB_TO_BB(mp, M_IGEO(mp)->blocks_per_cluster);
	imap.im_boffset = XFS_INO_TO_OFFSET(mp, irec->ir_startino) <<
			mp->m_sb.sb_inodelog;

	if (imap.im_boffset != 0 && cluster_base != 0) {
		ASSERT(imap.im_boffset == 0 || cluster_base == 0);
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return 0;
	}

	trace_xchk_iallocbt_check_cluster(mp, agno, irec->ir_startino,
			imap.im_blkno, imap.im_len, cluster_base, nr_inodes,
			cluster_mask, ir_holemask,
			XFS_INO_TO_OFFSET(mp, irec->ir_startino +
					  cluster_base));

	 
	if (ir_holemask != cluster_mask && ir_holemask != 0) {
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return 0;
	}

	 
	if (ir_holemask) {
		xchk_xref_is_not_owned_by(bs->sc, agbno,
				M_IGEO(mp)->blocks_per_cluster,
				&XFS_RMAP_OINFO_INODES);
		return 0;
	}

	xchk_xref_is_only_owned_by(bs->sc, agbno, M_IGEO(mp)->blocks_per_cluster,
			&XFS_RMAP_OINFO_INODES);

	 
	error = xfs_imap_to_bp(mp, bs->cur->bc_tp, &imap, &cluster_bp);
	if (!xchk_btree_xref_process_error(bs->sc, bs->cur, 0, &error))
		return error;

	 
	for (cluster_index = 0; cluster_index < nr_inodes; cluster_index++) {
		struct xfs_dinode	*dip;

		if (imap.im_boffset >= BBTOB(cluster_bp->b_length)) {
			xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
			break;
		}

		dip = xfs_buf_offset(cluster_bp, imap.im_boffset);
		error = xchk_iallocbt_check_cluster_ifree(bs, irec,
				cluster_base + cluster_index, dip);
		if (error)
			break;
		imap.im_boffset += mp->m_sb.sb_inodesize;
	}

	xfs_trans_brelse(bs->cur->bc_tp, cluster_bp);
	return error;
}

 
STATIC int
xchk_iallocbt_check_clusters(
	struct xchk_btree		*bs,
	struct xfs_inobt_rec_incore	*irec)
{
	unsigned int			cluster_base;
	int				error = 0;

	 
	for (cluster_base = 0;
	     cluster_base < XFS_INODES_PER_CHUNK;
	     cluster_base += M_IGEO(bs->sc->mp)->inodes_per_cluster) {
		error = xchk_iallocbt_check_cluster(bs, irec, cluster_base);
		if (error)
			break;
	}

	return error;
}

 
STATIC void
xchk_iallocbt_rec_alignment(
	struct xchk_btree		*bs,
	struct xfs_inobt_rec_incore	*irec)
{
	struct xfs_mount		*mp = bs->sc->mp;
	struct xchk_iallocbt		*iabt = bs->private;
	struct xfs_ino_geometry		*igeo = M_IGEO(mp);

	 
	if (bs->cur->bc_btnum == XFS_BTNUM_FINO) {
		unsigned int	imask;

		imask = min_t(unsigned int, XFS_INODES_PER_CHUNK,
				igeo->cluster_align_inodes) - 1;
		if (irec->ir_startino & imask)
			xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return;
	}

	if (iabt->next_startino != NULLAGINO) {
		 
		if (irec->ir_startino != iabt->next_startino) {
			xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
			return;
		}

		iabt->next_startino += XFS_INODES_PER_CHUNK;

		 
		if (iabt->next_startino >= iabt->next_cluster_ino) {
			iabt->next_startino = NULLAGINO;
			iabt->next_cluster_ino = NULLAGINO;
		}
		return;
	}

	 
	if (irec->ir_startino & (igeo->cluster_align_inodes - 1)) {
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return;
	}

	if (irec->ir_startino & (igeo->inodes_per_cluster - 1)) {
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return;
	}

	if (igeo->inodes_per_cluster <= XFS_INODES_PER_CHUNK)
		return;

	 
	iabt->next_startino = irec->ir_startino + XFS_INODES_PER_CHUNK;
	iabt->next_cluster_ino = irec->ir_startino + igeo->inodes_per_cluster;
}

 
STATIC int
xchk_iallocbt_rec(
	struct xchk_btree		*bs,
	const union xfs_btree_rec	*rec)
{
	struct xfs_mount		*mp = bs->cur->bc_mp;
	struct xchk_iallocbt		*iabt = bs->private;
	struct xfs_inobt_rec_incore	irec;
	uint64_t			holes;
	xfs_agino_t			agino;
	int				holecount;
	int				i;
	int				error = 0;
	uint16_t			holemask;

	xfs_inobt_btrec_to_irec(mp, rec, &irec);
	if (xfs_inobt_check_irec(bs->cur, &irec) != NULL) {
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);
		return 0;
	}

	agino = irec.ir_startino;

	xchk_iallocbt_rec_alignment(bs, &irec);
	if (bs->sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		goto out;

	iabt->inodes += irec.ir_count;

	 
	if (!xfs_inobt_issparse(irec.ir_holemask)) {
		if (irec.ir_count != XFS_INODES_PER_CHUNK)
			xchk_btree_set_corrupt(bs->sc, bs->cur, 0);

		if (!xchk_iallocbt_chunk(bs, &irec, agino,
					XFS_INODES_PER_CHUNK))
			goto out;
		goto check_clusters;
	}

	 
	holemask = irec.ir_holemask;
	holecount = 0;
	holes = ~xfs_inobt_irec_to_allocmask(&irec);
	if ((holes & irec.ir_free) != holes ||
	    irec.ir_freecount > irec.ir_count)
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);

	for (i = 0; i < XFS_INOBT_HOLEMASK_BITS; i++) {
		if (holemask & 1)
			holecount += XFS_INODES_PER_HOLEMASK_BIT;
		else if (!xchk_iallocbt_chunk(bs, &irec, agino,
					XFS_INODES_PER_HOLEMASK_BIT))
			goto out;
		holemask >>= 1;
		agino += XFS_INODES_PER_HOLEMASK_BIT;
	}

	if (holecount > XFS_INODES_PER_CHUNK ||
	    holecount + irec.ir_count != XFS_INODES_PER_CHUNK)
		xchk_btree_set_corrupt(bs->sc, bs->cur, 0);

check_clusters:
	if (bs->sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		goto out;

	error = xchk_iallocbt_check_clusters(bs, &irec);
	if (error)
		goto out;

out:
	return error;
}

 
STATIC void
xchk_iallocbt_xref_rmap_btreeblks(
	struct xfs_scrub	*sc,
	int			which)
{
	xfs_filblks_t		blocks;
	xfs_extlen_t		inobt_blocks = 0;
	xfs_extlen_t		finobt_blocks = 0;
	int			error;

	if (!sc->sa.ino_cur || !sc->sa.rmap_cur ||
	    (xfs_has_finobt(sc->mp) && !sc->sa.fino_cur) ||
	    xchk_skip_xref(sc->sm))
		return;

	 
	error = xfs_btree_count_blocks(sc->sa.ino_cur, &inobt_blocks);
	if (!xchk_process_error(sc, 0, 0, &error))
		return;

	if (sc->sa.fino_cur) {
		error = xfs_btree_count_blocks(sc->sa.fino_cur, &finobt_blocks);
		if (!xchk_process_error(sc, 0, 0, &error))
			return;
	}

	error = xchk_count_rmap_ownedby_ag(sc, sc->sa.rmap_cur,
			&XFS_RMAP_OINFO_INOBT, &blocks);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.rmap_cur))
		return;
	if (blocks != inobt_blocks + finobt_blocks)
		xchk_btree_set_corrupt(sc, sc->sa.ino_cur, 0);
}

 
STATIC void
xchk_iallocbt_xref_rmap_inodes(
	struct xfs_scrub	*sc,
	int			which,
	unsigned long long	inodes)
{
	xfs_filblks_t		blocks;
	xfs_filblks_t		inode_blocks;
	int			error;

	if (!sc->sa.rmap_cur || xchk_skip_xref(sc->sm))
		return;

	 
	error = xchk_count_rmap_ownedby_ag(sc, sc->sa.rmap_cur,
			&XFS_RMAP_OINFO_INODES, &blocks);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.rmap_cur))
		return;
	inode_blocks = XFS_B_TO_FSB(sc->mp, inodes * sc->mp->m_sb.sb_inodesize);
	if (blocks != inode_blocks)
		xchk_btree_xref_set_corrupt(sc, sc->sa.rmap_cur, 0);
}

 
STATIC int
xchk_iallocbt(
	struct xfs_scrub	*sc,
	xfs_btnum_t		which)
{
	struct xfs_btree_cur	*cur;
	struct xchk_iallocbt	iabt = {
		.inodes		= 0,
		.next_startino	= NULLAGINO,
		.next_cluster_ino = NULLAGINO,
	};
	int			error;

	cur = which == XFS_BTNUM_INO ? sc->sa.ino_cur : sc->sa.fino_cur;
	error = xchk_btree(sc, cur, xchk_iallocbt_rec, &XFS_RMAP_OINFO_INOBT,
			&iabt);
	if (error)
		return error;

	xchk_iallocbt_xref_rmap_btreeblks(sc, which);

	 
	if (which == XFS_BTNUM_INO)
		xchk_iallocbt_xref_rmap_inodes(sc, which, iabt.inodes);

	return error;
}

int
xchk_inobt(
	struct xfs_scrub	*sc)
{
	return xchk_iallocbt(sc, XFS_BTNUM_INO);
}

int
xchk_finobt(
	struct xfs_scrub	*sc)
{
	return xchk_iallocbt(sc, XFS_BTNUM_FINO);
}

 
static inline void
xchk_xref_inode_check(
	struct xfs_scrub	*sc,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len,
	struct xfs_btree_cur	**icur,
	enum xbtree_recpacking	expected)
{
	enum xbtree_recpacking	outcome;
	int			error;

	if (!(*icur) || xchk_skip_xref(sc->sm))
		return;

	error = xfs_ialloc_has_inodes_at_extent(*icur, agbno, len, &outcome);
	if (!xchk_should_check_xref(sc, &error, icur))
		return;
	if (outcome != expected)
		xchk_btree_xref_set_corrupt(sc, *icur, 0);
}

 
void
xchk_xref_is_not_inode_chunk(
	struct xfs_scrub	*sc,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len)
{
	xchk_xref_inode_check(sc, agbno, len, &sc->sa.ino_cur,
			XBTREE_RECPACKING_EMPTY);
	xchk_xref_inode_check(sc, agbno, len, &sc->sa.fino_cur,
			XBTREE_RECPACKING_EMPTY);
}

 
void
xchk_xref_is_inode_chunk(
	struct xfs_scrub	*sc,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len)
{
	xchk_xref_inode_check(sc, agbno, len, &sc->sa.ino_cur,
			XBTREE_RECPACKING_FULL);
}

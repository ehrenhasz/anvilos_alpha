
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_dir2.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_trans.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_bmap_util.h"
#include "xfs_bmap_btree.h"
#include "xfs_rtalloc.h"
#include "xfs_errortag.h"
#include "xfs_error.h"
#include "xfs_quota.h"
#include "xfs_trans_space.h"
#include "xfs_buf_item.h"
#include "xfs_trace.h"
#include "xfs_attr_leaf.h"
#include "xfs_filestream.h"
#include "xfs_rmap.h"
#include "xfs_ag.h"
#include "xfs_ag_resv.h"
#include "xfs_refcount.h"
#include "xfs_icache.h"
#include "xfs_iomap.h"

struct kmem_cache		*xfs_bmap_intent_cache;

 

 
void
xfs_bmap_compute_maxlevels(
	xfs_mount_t	*mp,		 
	int		whichfork)	 
{
	uint64_t	maxblocks;	 
	xfs_extnum_t	maxleafents;	 
	int		level;		 
	int		maxrootrecs;	 
	int		minleafrecs;	 
	int		minnoderecs;	 
	int		sz;		 

	 
	maxleafents = xfs_iext_max_nextents(xfs_has_large_extent_counts(mp),
				whichfork);
	if (whichfork == XFS_DATA_FORK)
		sz = XFS_BMDR_SPACE_CALC(MINDBTPTRS);
	else
		sz = XFS_BMDR_SPACE_CALC(MINABTPTRS);

	maxrootrecs = xfs_bmdr_maxrecs(sz, 0);
	minleafrecs = mp->m_bmap_dmnr[0];
	minnoderecs = mp->m_bmap_dmnr[1];
	maxblocks = howmany_64(maxleafents, minleafrecs);
	for (level = 1; maxblocks > 1; level++) {
		if (maxblocks <= maxrootrecs)
			maxblocks = 1;
		else
			maxblocks = howmany_64(maxblocks, minnoderecs);
	}
	mp->m_bm_maxlevels[whichfork] = level;
	ASSERT(mp->m_bm_maxlevels[whichfork] <= xfs_bmbt_maxlevels_ondisk());
}

unsigned int
xfs_bmap_compute_attr_offset(
	struct xfs_mount	*mp)
{
	if (mp->m_sb.sb_inodesize == 256)
		return XFS_LITINO(mp) - XFS_BMDR_SPACE_CALC(MINABTPTRS);
	return XFS_BMDR_SPACE_CALC(6 * MINABTPTRS);
}

STATIC int				 
xfs_bmbt_lookup_eq(
	struct xfs_btree_cur	*cur,
	struct xfs_bmbt_irec	*irec,
	int			*stat)	 
{
	cur->bc_rec.b = *irec;
	return xfs_btree_lookup(cur, XFS_LOOKUP_EQ, stat);
}

STATIC int				 
xfs_bmbt_lookup_first(
	struct xfs_btree_cur	*cur,
	int			*stat)	 
{
	cur->bc_rec.b.br_startoff = 0;
	cur->bc_rec.b.br_startblock = 0;
	cur->bc_rec.b.br_blockcount = 0;
	return xfs_btree_lookup(cur, XFS_LOOKUP_GE, stat);
}

 
static inline bool xfs_bmap_needs_btree(struct xfs_inode *ip, int whichfork)
{
	struct xfs_ifork *ifp = xfs_ifork_ptr(ip, whichfork);

	return whichfork != XFS_COW_FORK &&
		ifp->if_format == XFS_DINODE_FMT_EXTENTS &&
		ifp->if_nextents > XFS_IFORK_MAXEXT(ip, whichfork);
}

 
static inline bool xfs_bmap_wants_extents(struct xfs_inode *ip, int whichfork)
{
	struct xfs_ifork *ifp = xfs_ifork_ptr(ip, whichfork);

	return whichfork != XFS_COW_FORK &&
		ifp->if_format == XFS_DINODE_FMT_BTREE &&
		ifp->if_nextents <= XFS_IFORK_MAXEXT(ip, whichfork);
}

 
STATIC int
xfs_bmbt_update(
	struct xfs_btree_cur	*cur,
	struct xfs_bmbt_irec	*irec)
{
	union xfs_btree_rec	rec;

	xfs_bmbt_disk_set_all(&rec.bmbt, irec);
	return xfs_btree_update(cur, &rec);
}

 
STATIC xfs_filblks_t
xfs_bmap_worst_indlen(
	xfs_inode_t	*ip,		 
	xfs_filblks_t	len)		 
{
	int		level;		 
	int		maxrecs;	 
	xfs_mount_t	*mp;		 
	xfs_filblks_t	rval;		 

	mp = ip->i_mount;
	maxrecs = mp->m_bmap_dmxr[0];
	for (level = 0, rval = 0;
	     level < XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK);
	     level++) {
		len += maxrecs - 1;
		do_div(len, maxrecs);
		rval += len;
		if (len == 1)
			return rval + XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) -
				level - 1;
		if (level == 0)
			maxrecs = mp->m_bmap_dmxr[1];
	}
	return rval;
}

 
uint
xfs_default_attroffset(
	struct xfs_inode	*ip)
{
	if (ip->i_df.if_format == XFS_DINODE_FMT_DEV)
		return roundup(sizeof(xfs_dev_t), 8);
	return M_IGEO(ip->i_mount)->attr_fork_offset;
}

 
STATIC void
xfs_bmap_forkoff_reset(
	xfs_inode_t	*ip,
	int		whichfork)
{
	if (whichfork == XFS_ATTR_FORK &&
	    ip->i_df.if_format != XFS_DINODE_FMT_DEV &&
	    ip->i_df.if_format != XFS_DINODE_FMT_BTREE) {
		uint	dfl_forkoff = xfs_default_attroffset(ip) >> 3;

		if (dfl_forkoff > ip->i_forkoff)
			ip->i_forkoff = dfl_forkoff;
	}
}

#ifdef DEBUG
STATIC struct xfs_buf *
xfs_bmap_get_bp(
	struct xfs_btree_cur	*cur,
	xfs_fsblock_t		bno)
{
	struct xfs_log_item	*lip;
	int			i;

	if (!cur)
		return NULL;

	for (i = 0; i < cur->bc_maxlevels; i++) {
		if (!cur->bc_levels[i].bp)
			break;
		if (xfs_buf_daddr(cur->bc_levels[i].bp) == bno)
			return cur->bc_levels[i].bp;
	}

	 
	list_for_each_entry(lip, &cur->bc_tp->t_items, li_trans) {
		struct xfs_buf_log_item	*bip = (struct xfs_buf_log_item *)lip;

		if (bip->bli_item.li_type == XFS_LI_BUF &&
		    xfs_buf_daddr(bip->bli_buf) == bno)
			return bip->bli_buf;
	}

	return NULL;
}

STATIC void
xfs_check_block(
	struct xfs_btree_block	*block,
	xfs_mount_t		*mp,
	int			root,
	short			sz)
{
	int			i, j, dmxr;
	__be64			*pp, *thispa;	 
	xfs_bmbt_key_t		*prevp, *keyp;

	ASSERT(be16_to_cpu(block->bb_level) > 0);

	prevp = NULL;
	for( i = 1; i <= xfs_btree_get_numrecs(block); i++) {
		dmxr = mp->m_bmap_dmxr[0];
		keyp = XFS_BMBT_KEY_ADDR(mp, block, i);

		if (prevp) {
			ASSERT(be64_to_cpu(prevp->br_startoff) <
			       be64_to_cpu(keyp->br_startoff));
		}
		prevp = keyp;

		 
		if (root)
			pp = XFS_BMAP_BROOT_PTR_ADDR(mp, block, i, sz);
		else
			pp = XFS_BMBT_PTR_ADDR(mp, block, i, dmxr);

		for (j = i+1; j <= be16_to_cpu(block->bb_numrecs); j++) {
			if (root)
				thispa = XFS_BMAP_BROOT_PTR_ADDR(mp, block, j, sz);
			else
				thispa = XFS_BMBT_PTR_ADDR(mp, block, j, dmxr);
			if (*thispa == *pp) {
				xfs_warn(mp, "%s: thispa(%d) == pp(%d) %lld",
					__func__, j, i,
					(unsigned long long)be64_to_cpu(*thispa));
				xfs_err(mp, "%s: ptrs are equal in node\n",
					__func__);
				xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
			}
		}
	}
}

 
STATIC void
xfs_bmap_check_leaf_extents(
	struct xfs_btree_cur	*cur,	 
	xfs_inode_t		*ip,		 
	int			whichfork)	 
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_block	*block;	 
	xfs_fsblock_t		bno;	 
	struct xfs_buf		*bp;	 
	int			error;	 
	xfs_extnum_t		i=0, j;	 
	int			level;	 
	__be64			*pp;	 
	xfs_bmbt_rec_t		*ep;	 
	xfs_bmbt_rec_t		last = {0, 0};  
	xfs_bmbt_rec_t		*nextp;	 
	int			bp_release = 0;

	if (ifp->if_format != XFS_DINODE_FMT_BTREE)
		return;

	 
	if (ip->i_df.if_nextents > 10000)
		return;

	bno = NULLFSBLOCK;
	block = ifp->if_broot;
	 
	level = be16_to_cpu(block->bb_level);
	ASSERT(level > 0);
	xfs_check_block(block, mp, 1, ifp->if_broot_bytes);
	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, block, 1, ifp->if_broot_bytes);
	bno = be64_to_cpu(*pp);

	ASSERT(bno != NULLFSBLOCK);
	ASSERT(XFS_FSB_TO_AGNO(mp, bno) < mp->m_sb.sb_agcount);
	ASSERT(XFS_FSB_TO_AGBNO(mp, bno) < mp->m_sb.sb_agblocks);

	 
	while (level-- > 0) {
		 
		bp_release = 0;
		bp = xfs_bmap_get_bp(cur, XFS_FSB_TO_DADDR(mp, bno));
		if (!bp) {
			bp_release = 1;
			error = xfs_btree_read_bufl(mp, NULL, bno, &bp,
						XFS_BMAP_BTREE_REF,
						&xfs_bmbt_buf_ops);
			if (error)
				goto error_norelse;
		}
		block = XFS_BUF_TO_BLOCK(bp);
		if (level == 0)
			break;

		 

		xfs_check_block(block, mp, 0, 0);
		pp = XFS_BMBT_PTR_ADDR(mp, block, 1, mp->m_bmap_dmxr[1]);
		bno = be64_to_cpu(*pp);
		if (XFS_IS_CORRUPT(mp, !xfs_verify_fsbno(mp, bno))) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if (bp_release) {
			bp_release = 0;
			xfs_trans_brelse(NULL, bp);
		}
	}

	 
	i = 0;

	 
	for (;;) {
		xfs_fsblock_t	nextbno;
		xfs_extnum_t	num_recs;


		num_recs = xfs_btree_get_numrecs(block);

		 

		nextbno = be64_to_cpu(block->bb_u.l.bb_rightsib);

		 

		ep = XFS_BMBT_REC_ADDR(mp, block, 1);
		if (i) {
			ASSERT(xfs_bmbt_disk_get_startoff(&last) +
			       xfs_bmbt_disk_get_blockcount(&last) <=
			       xfs_bmbt_disk_get_startoff(ep));
		}
		for (j = 1; j < num_recs; j++) {
			nextp = XFS_BMBT_REC_ADDR(mp, block, j + 1);
			ASSERT(xfs_bmbt_disk_get_startoff(ep) +
			       xfs_bmbt_disk_get_blockcount(ep) <=
			       xfs_bmbt_disk_get_startoff(nextp));
			ep = nextp;
		}

		last = *ep;
		i += num_recs;
		if (bp_release) {
			bp_release = 0;
			xfs_trans_brelse(NULL, bp);
		}
		bno = nextbno;
		 
		if (bno == NULLFSBLOCK)
			break;

		bp_release = 0;
		bp = xfs_bmap_get_bp(cur, XFS_FSB_TO_DADDR(mp, bno));
		if (!bp) {
			bp_release = 1;
			error = xfs_btree_read_bufl(mp, NULL, bno, &bp,
						XFS_BMAP_BTREE_REF,
						&xfs_bmbt_buf_ops);
			if (error)
				goto error_norelse;
		}
		block = XFS_BUF_TO_BLOCK(bp);
	}

	return;

error0:
	xfs_warn(mp, "%s: at error0", __func__);
	if (bp_release)
		xfs_trans_brelse(NULL, bp);
error_norelse:
	xfs_warn(mp, "%s: BAD after btree leaves for %llu extents",
		__func__, i);
	xfs_err(mp, "%s: CORRUPTED BTREE OR SOMETHING", __func__);
	xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
	return;
}

 
STATIC void
xfs_bmap_validate_ret(
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	uint32_t		flags,
	xfs_bmbt_irec_t		*mval,
	int			nmap,
	int			ret_nmap)
{
	int			i;		 

	ASSERT(ret_nmap <= nmap);

	for (i = 0; i < ret_nmap; i++) {
		ASSERT(mval[i].br_blockcount > 0);
		if (!(flags & XFS_BMAPI_ENTIRE)) {
			ASSERT(mval[i].br_startoff >= bno);
			ASSERT(mval[i].br_blockcount <= len);
			ASSERT(mval[i].br_startoff + mval[i].br_blockcount <=
			       bno + len);
		} else {
			ASSERT(mval[i].br_startoff < bno + len);
			ASSERT(mval[i].br_startoff + mval[i].br_blockcount >
			       bno);
		}
		ASSERT(i == 0 ||
		       mval[i - 1].br_startoff + mval[i - 1].br_blockcount ==
		       mval[i].br_startoff);
		ASSERT(mval[i].br_startblock != DELAYSTARTBLOCK &&
		       mval[i].br_startblock != HOLESTARTBLOCK);
		ASSERT(mval[i].br_state == XFS_EXT_NORM ||
		       mval[i].br_state == XFS_EXT_UNWRITTEN);
	}
}

#else
#define xfs_bmap_check_leaf_extents(cur, ip, whichfork)		do { } while (0)
#define	xfs_bmap_validate_ret(bno,len,flags,mval,onmap,nmap)	do { } while (0)
#endif  

 

 
STATIC int				 
xfs_bmap_btree_to_extents(
	struct xfs_trans	*tp,	 
	struct xfs_inode	*ip,	 
	struct xfs_btree_cur	*cur,	 
	int			*logflagsp,  
	int			whichfork)   
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_btree_block	*rblock = ifp->if_broot;
	struct xfs_btree_block	*cblock; 
	xfs_fsblock_t		cbno;	 
	struct xfs_buf		*cbp;	 
	int			error;	 
	__be64			*pp;	 
	struct xfs_owner_info	oinfo;

	 
	if (!xfs_bmap_wants_extents(ip, whichfork))
		return 0;

	ASSERT(cur);
	ASSERT(whichfork != XFS_COW_FORK);
	ASSERT(ifp->if_format == XFS_DINODE_FMT_BTREE);
	ASSERT(be16_to_cpu(rblock->bb_level) == 1);
	ASSERT(be16_to_cpu(rblock->bb_numrecs) == 1);
	ASSERT(xfs_bmbt_maxrecs(mp, ifp->if_broot_bytes, 0) == 1);

	pp = XFS_BMAP_BROOT_PTR_ADDR(mp, rblock, 1, ifp->if_broot_bytes);
	cbno = be64_to_cpu(*pp);
#ifdef DEBUG
	if (XFS_IS_CORRUPT(cur->bc_mp, !xfs_btree_check_lptr(cur, cbno, 1)))
		return -EFSCORRUPTED;
#endif
	error = xfs_btree_read_bufl(mp, tp, cbno, &cbp, XFS_BMAP_BTREE_REF,
				&xfs_bmbt_buf_ops);
	if (error)
		return error;
	cblock = XFS_BUF_TO_BLOCK(cbp);
	if ((error = xfs_btree_check_block(cur, cblock, 0, cbp)))
		return error;

	xfs_rmap_ino_bmbt_owner(&oinfo, ip->i_ino, whichfork);
	error = xfs_free_extent_later(cur->bc_tp, cbno, 1, &oinfo,
			XFS_AG_RESV_NONE);
	if (error)
		return error;

	ip->i_nblocks--;
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, -1L);
	xfs_trans_binval(tp, cbp);
	if (cur->bc_levels[0].bp == cbp)
		cur->bc_levels[0].bp = NULL;
	xfs_iroot_realloc(ip, -1, whichfork);
	ASSERT(ifp->if_broot == NULL);
	ifp->if_format = XFS_DINODE_FMT_EXTENTS;
	*logflagsp |= XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
	return 0;
}

 
STATIC int					 
xfs_bmap_extents_to_btree(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	struct xfs_btree_cur	**curp,		 
	int			wasdel,		 
	int			*logflagsp,	 
	int			whichfork)	 
{
	struct xfs_btree_block	*ablock;	 
	struct xfs_buf		*abp;		 
	struct xfs_alloc_arg	args;		 
	struct xfs_bmbt_rec	*arp;		 
	struct xfs_btree_block	*block;		 
	struct xfs_btree_cur	*cur;		 
	int			error;		 
	struct xfs_ifork	*ifp;		 
	struct xfs_bmbt_key	*kp;		 
	struct xfs_mount	*mp;		 
	xfs_bmbt_ptr_t		*pp;		 
	struct xfs_iext_cursor	icur;
	struct xfs_bmbt_irec	rec;
	xfs_extnum_t		cnt = 0;

	mp = ip->i_mount;
	ASSERT(whichfork != XFS_COW_FORK);
	ifp = xfs_ifork_ptr(ip, whichfork);
	ASSERT(ifp->if_format == XFS_DINODE_FMT_EXTENTS);

	 
	xfs_iroot_realloc(ip, 1, whichfork);

	 
	block = ifp->if_broot;
	xfs_btree_init_block_int(mp, block, XFS_BUF_DADDR_NULL,
				 XFS_BTNUM_BMAP, 1, 1, ip->i_ino,
				 XFS_BTREE_LONG_PTRS);
	 
	cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
	cur->bc_ino.flags = wasdel ? XFS_BTCUR_BMBT_WASDEL : 0;
	 
	ifp->if_format = XFS_DINODE_FMT_BTREE;
	memset(&args, 0, sizeof(args));
	args.tp = tp;
	args.mp = mp;
	xfs_rmap_ino_bmbt_owner(&args.oinfo, ip->i_ino, whichfork);

	args.minlen = args.maxlen = args.prod = 1;
	args.wasdel = wasdel;
	*logflagsp = 0;
	error = xfs_alloc_vextent_start_ag(&args,
				XFS_INO_TO_FSB(mp, ip->i_ino));
	if (error)
		goto out_root_realloc;

	 
	if (WARN_ON_ONCE(args.fsbno == NULLFSBLOCK)) {
		error = -ENOSPC;
		goto out_root_realloc;
	}

	cur->bc_ino.allocated++;
	ip->i_nblocks++;
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, 1L);
	error = xfs_trans_get_buf(tp, mp->m_ddev_targp,
			XFS_FSB_TO_DADDR(mp, args.fsbno),
			mp->m_bsize, 0, &abp);
	if (error)
		goto out_unreserve_dquot;

	 
	abp->b_ops = &xfs_bmbt_buf_ops;
	ablock = XFS_BUF_TO_BLOCK(abp);
	xfs_btree_init_block_int(mp, ablock, xfs_buf_daddr(abp),
				XFS_BTNUM_BMAP, 0, 0, ip->i_ino,
				XFS_BTREE_LONG_PTRS);

	for_each_xfs_iext(ifp, &icur, &rec) {
		if (isnullstartblock(rec.br_startblock))
			continue;
		arp = XFS_BMBT_REC_ADDR(mp, ablock, 1 + cnt);
		xfs_bmbt_disk_set_all(arp, &rec);
		cnt++;
	}
	ASSERT(cnt == ifp->if_nextents);
	xfs_btree_set_numrecs(ablock, cnt);

	 
	kp = XFS_BMBT_KEY_ADDR(mp, block, 1);
	arp = XFS_BMBT_REC_ADDR(mp, ablock, 1);
	kp->br_startoff = cpu_to_be64(xfs_bmbt_disk_get_startoff(arp));
	pp = XFS_BMBT_PTR_ADDR(mp, block, 1, xfs_bmbt_get_maxrecs(cur,
						be16_to_cpu(block->bb_level)));
	*pp = cpu_to_be64(args.fsbno);

	 
	xfs_btree_log_block(cur, abp, XFS_BB_ALL_BITS);
	xfs_btree_log_recs(cur, abp, 1, be16_to_cpu(ablock->bb_numrecs));
	ASSERT(*curp == NULL);
	*curp = cur;
	*logflagsp = XFS_ILOG_CORE | xfs_ilog_fbroot(whichfork);
	return 0;

out_unreserve_dquot:
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, -1L);
out_root_realloc:
	xfs_iroot_realloc(ip, -1, whichfork);
	ifp->if_format = XFS_DINODE_FMT_EXTENTS;
	ASSERT(ifp->if_broot == NULL);
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);

	return error;
}

 
void
xfs_bmap_local_to_extents_empty(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);

	ASSERT(whichfork != XFS_COW_FORK);
	ASSERT(ifp->if_format == XFS_DINODE_FMT_LOCAL);
	ASSERT(ifp->if_bytes == 0);
	ASSERT(ifp->if_nextents == 0);

	xfs_bmap_forkoff_reset(ip, whichfork);
	ifp->if_u1.if_root = NULL;
	ifp->if_height = 0;
	ifp->if_format = XFS_DINODE_FMT_EXTENTS;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
}


STATIC int				 
xfs_bmap_local_to_extents(
	xfs_trans_t	*tp,		 
	xfs_inode_t	*ip,		 
	xfs_extlen_t	total,		 
	int		*logflagsp,	 
	int		whichfork,
	void		(*init_fn)(struct xfs_trans *tp,
				   struct xfs_buf *bp,
				   struct xfs_inode *ip,
				   struct xfs_ifork *ifp))
{
	int		error = 0;
	int		flags;		 
	struct xfs_ifork *ifp;		 
	xfs_alloc_arg_t	args;		 
	struct xfs_buf	*bp;		 
	struct xfs_bmbt_irec rec;
	struct xfs_iext_cursor icur;

	 
	ASSERT(!(S_ISREG(VFS_I(ip)->i_mode) && whichfork == XFS_DATA_FORK));
	ifp = xfs_ifork_ptr(ip, whichfork);
	ASSERT(ifp->if_format == XFS_DINODE_FMT_LOCAL);

	if (!ifp->if_bytes) {
		xfs_bmap_local_to_extents_empty(tp, ip, whichfork);
		flags = XFS_ILOG_CORE;
		goto done;
	}

	flags = 0;
	error = 0;
	memset(&args, 0, sizeof(args));
	args.tp = tp;
	args.mp = ip->i_mount;
	args.total = total;
	args.minlen = args.maxlen = args.prod = 1;
	xfs_rmap_ino_owner(&args.oinfo, ip->i_ino, whichfork, 0);

	 
	args.total = total;
	args.minlen = args.maxlen = args.prod = 1;
	error = xfs_alloc_vextent_start_ag(&args,
			XFS_INO_TO_FSB(args.mp, ip->i_ino));
	if (error)
		goto done;

	 
	ASSERT(args.fsbno != NULLFSBLOCK);
	ASSERT(args.len == 1);
	error = xfs_trans_get_buf(tp, args.mp->m_ddev_targp,
			XFS_FSB_TO_DADDR(args.mp, args.fsbno),
			args.mp->m_bsize, 0, &bp);
	if (error)
		goto done;

	 
	init_fn(tp, bp, ip, ifp);

	 
	xfs_idata_realloc(ip, -ifp->if_bytes, whichfork);
	xfs_bmap_local_to_extents_empty(tp, ip, whichfork);
	flags |= XFS_ILOG_CORE;

	ifp->if_u1.if_root = NULL;
	ifp->if_height = 0;

	rec.br_startoff = 0;
	rec.br_startblock = args.fsbno;
	rec.br_blockcount = 1;
	rec.br_state = XFS_EXT_NORM;
	xfs_iext_first(ifp, &icur);
	xfs_iext_insert(ip, &icur, &rec, 0);

	ifp->if_nextents = 1;
	ip->i_nblocks = 1;
	xfs_trans_mod_dquot_byino(tp, ip, XFS_TRANS_DQ_BCOUNT, 1L);
	flags |= xfs_ilog_fext(whichfork);

done:
	*logflagsp = flags;
	return error;
}

 
STATIC int					 
xfs_bmap_add_attrfork_btree(
	xfs_trans_t		*tp,		 
	xfs_inode_t		*ip,		 
	int			*flags)		 
{
	struct xfs_btree_block	*block = ip->i_df.if_broot;
	struct xfs_btree_cur	*cur;		 
	int			error;		 
	xfs_mount_t		*mp;		 
	int			stat;		 

	mp = ip->i_mount;

	if (XFS_BMAP_BMDR_SPACE(block) <= xfs_inode_data_fork_size(ip))
		*flags |= XFS_ILOG_DBROOT;
	else {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, XFS_DATA_FORK);
		error = xfs_bmbt_lookup_first(cur, &stat);
		if (error)
			goto error0;
		 
		if (XFS_IS_CORRUPT(mp, stat != 1)) {
			error = -EFSCORRUPTED;
			goto error0;
		}
		if ((error = xfs_btree_new_iroot(cur, flags, &stat)))
			goto error0;
		if (stat == 0) {
			xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
			return -ENOSPC;
		}
		cur->bc_ino.allocated = 0;
		xfs_btree_del_cursor(cur, XFS_BTREE_NOERROR);
	}
	return 0;
error0:
	xfs_btree_del_cursor(cur, XFS_BTREE_ERROR);
	return error;
}

 
STATIC int					 
xfs_bmap_add_attrfork_extents(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	int			*flags)		 
{
	struct xfs_btree_cur	*cur;		 
	int			error;		 

	if (ip->i_df.if_nextents * sizeof(struct xfs_bmbt_rec) <=
	    xfs_inode_data_fork_size(ip))
		return 0;
	cur = NULL;
	error = xfs_bmap_extents_to_btree(tp, ip, &cur, 0, flags,
					  XFS_DATA_FORK);
	if (cur) {
		cur->bc_ino.allocated = 0;
		xfs_btree_del_cursor(cur, error);
	}
	return error;
}

 
STATIC int					 
xfs_bmap_add_attrfork_local(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	int			*flags)		 
{
	struct xfs_da_args	dargs;		 

	if (ip->i_df.if_bytes <= xfs_inode_data_fork_size(ip))
		return 0;

	if (S_ISDIR(VFS_I(ip)->i_mode)) {
		memset(&dargs, 0, sizeof(dargs));
		dargs.geo = ip->i_mount->m_dir_geo;
		dargs.dp = ip;
		dargs.total = dargs.geo->fsbcount;
		dargs.whichfork = XFS_DATA_FORK;
		dargs.trans = tp;
		return xfs_dir2_sf_to_block(&dargs);
	}

	if (S_ISLNK(VFS_I(ip)->i_mode))
		return xfs_bmap_local_to_extents(tp, ip, 1, flags,
						 XFS_DATA_FORK,
						 xfs_symlink_local_to_remote);

	 
	ASSERT(0);
	return -EFSCORRUPTED;
}

 
static int
xfs_bmap_set_attrforkoff(
	struct xfs_inode	*ip,
	int			size,
	int			*version)
{
	int			default_size = xfs_default_attroffset(ip) >> 3;

	switch (ip->i_df.if_format) {
	case XFS_DINODE_FMT_DEV:
		ip->i_forkoff = default_size;
		break;
	case XFS_DINODE_FMT_LOCAL:
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
		ip->i_forkoff = xfs_attr_shortform_bytesfit(ip, size);
		if (!ip->i_forkoff)
			ip->i_forkoff = default_size;
		else if (xfs_has_attr2(ip->i_mount) && version)
			*version = 2;
		break;
	default:
		ASSERT(0);
		return -EINVAL;
	}

	return 0;
}

 
int						 
xfs_bmap_add_attrfork(
	xfs_inode_t		*ip,		 
	int			size,		 
	int			rsvd)		 
{
	xfs_mount_t		*mp;		 
	xfs_trans_t		*tp;		 
	int			blks;		 
	int			version = 1;	 
	int			logflags;	 
	int			error;		 

	ASSERT(xfs_inode_has_attr_fork(ip) == 0);

	mp = ip->i_mount;
	ASSERT(!XFS_NOT_DQATTACHED(mp, ip));

	blks = XFS_ADDAFORK_SPACE_RES(mp);

	error = xfs_trans_alloc_inode(ip, &M_RES(mp)->tr_addafork, blks, 0,
			rsvd, &tp);
	if (error)
		return error;
	if (xfs_inode_has_attr_fork(ip))
		goto trans_cancel;

	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	error = xfs_bmap_set_attrforkoff(ip, size, &version);
	if (error)
		goto trans_cancel;

	xfs_ifork_init_attr(ip, XFS_DINODE_FMT_EXTENTS, 0);
	logflags = 0;
	switch (ip->i_df.if_format) {
	case XFS_DINODE_FMT_LOCAL:
		error = xfs_bmap_add_attrfork_local(tp, ip, &logflags);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		error = xfs_bmap_add_attrfork_extents(tp, ip, &logflags);
		break;
	case XFS_DINODE_FMT_BTREE:
		error = xfs_bmap_add_attrfork_btree(tp, ip, &logflags);
		break;
	default:
		error = 0;
		break;
	}
	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	if (error)
		goto trans_cancel;
	if (!xfs_has_attr(mp) ||
	   (!xfs_has_attr2(mp) && version == 2)) {
		bool log_sb = false;

		spin_lock(&mp->m_sb_lock);
		if (!xfs_has_attr(mp)) {
			xfs_add_attr(mp);
			log_sb = true;
		}
		if (!xfs_has_attr2(mp) && version == 2) {
			xfs_add_attr2(mp);
			log_sb = true;
		}
		spin_unlock(&mp->m_sb_lock);
		if (log_sb)
			xfs_log_sb(tp);
	}

	error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

trans_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 

struct xfs_iread_state {
	struct xfs_iext_cursor	icur;
	xfs_extnum_t		loaded;
};

int
xfs_bmap_complain_bad_rec(
	struct xfs_inode		*ip,
	int				whichfork,
	xfs_failaddr_t			fa,
	const struct xfs_bmbt_irec	*irec)
{
	struct xfs_mount		*mp = ip->i_mount;
	const char			*forkname;

	switch (whichfork) {
	case XFS_DATA_FORK:	forkname = "data"; break;
	case XFS_ATTR_FORK:	forkname = "attr"; break;
	case XFS_COW_FORK:	forkname = "CoW"; break;
	default:		forkname = "???"; break;
	}

	xfs_warn(mp,
 "Bmap BTree record corruption in inode 0x%llx %s fork detected at %pS!",
				ip->i_ino, forkname, fa);
	xfs_warn(mp,
		"Offset 0x%llx, start block 0x%llx, block count 0x%llx state 0x%x",
		irec->br_startoff, irec->br_startblock, irec->br_blockcount,
		irec->br_state);

	return -EFSCORRUPTED;
}

 
static int
xfs_iread_bmbt_block(
	struct xfs_btree_cur	*cur,
	int			level,
	void			*priv)
{
	struct xfs_iread_state	*ir = priv;
	struct xfs_mount	*mp = cur->bc_mp;
	struct xfs_inode	*ip = cur->bc_ino.ip;
	struct xfs_btree_block	*block;
	struct xfs_buf		*bp;
	struct xfs_bmbt_rec	*frp;
	xfs_extnum_t		num_recs;
	xfs_extnum_t		j;
	int			whichfork = cur->bc_ino.whichfork;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);

	block = xfs_btree_get_block(cur, level, &bp);

	 
	num_recs = xfs_btree_get_numrecs(block);
	if (unlikely(ir->loaded + num_recs > ifp->if_nextents)) {
		xfs_warn(ip->i_mount, "corrupt dinode %llu, (btree extents).",
				(unsigned long long)ip->i_ino);
		xfs_inode_verifier_error(ip, -EFSCORRUPTED, __func__, block,
				sizeof(*block), __this_address);
		return -EFSCORRUPTED;
	}

	 
	frp = XFS_BMBT_REC_ADDR(mp, block, 1);
	for (j = 0; j < num_recs; j++, frp++, ir->loaded++) {
		struct xfs_bmbt_irec	new;
		xfs_failaddr_t		fa;

		xfs_bmbt_disk_get_all(frp, &new);
		fa = xfs_bmap_validate_extent(ip, whichfork, &new);
		if (fa) {
			xfs_inode_verifier_error(ip, -EFSCORRUPTED,
					"xfs_iread_extents(2)", frp,
					sizeof(*frp), fa);
			return xfs_bmap_complain_bad_rec(ip, whichfork, fa,
					&new);
		}
		xfs_iext_insert(ip, &ir->icur, &new,
				xfs_bmap_fork_to_state(whichfork));
		trace_xfs_read_extent(ip, &ir->icur,
				xfs_bmap_fork_to_state(whichfork), _THIS_IP_);
		xfs_iext_next(ifp, &ir->icur);
	}

	return 0;
}

 
int
xfs_iread_extents(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork)
{
	struct xfs_iread_state	ir;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_btree_cur	*cur;
	int			error;

	if (!xfs_need_iread_extents(ifp))
		return 0;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));

	ir.loaded = 0;
	xfs_iext_first(ifp, &ir.icur);
	cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
	error = xfs_btree_visit_blocks(cur, xfs_iread_bmbt_block,
			XFS_BTREE_VISIT_RECORDS, &ir);
	xfs_btree_del_cursor(cur, error);
	if (error)
		goto out;

	if (XFS_IS_CORRUPT(mp, ir.loaded != ifp->if_nextents)) {
		error = -EFSCORRUPTED;
		goto out;
	}
	ASSERT(ir.loaded == xfs_iext_count(ifp));
	 
	smp_store_release(&ifp->if_needextents, 0);
	return 0;
out:
	xfs_iext_destroy(ifp);
	return error;
}

 
int						 
xfs_bmap_first_unused(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	xfs_extlen_t		len,		 
	xfs_fileoff_t		*first_unused,	 
	int			whichfork)	 
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec	got;
	struct xfs_iext_cursor	icur;
	xfs_fileoff_t		lastaddr = 0;
	xfs_fileoff_t		lowest, max;
	int			error;

	if (ifp->if_format == XFS_DINODE_FMT_LOCAL) {
		*first_unused = 0;
		return 0;
	}

	ASSERT(xfs_ifork_has_extents(ifp));

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	lowest = max = *first_unused;
	for_each_xfs_iext(ifp, &icur, &got) {
		 
		if (got.br_startoff >= lowest + len &&
		    got.br_startoff - max >= len)
			break;
		lastaddr = got.br_startoff + got.br_blockcount;
		max = XFS_FILEOFF_MAX(lastaddr, lowest);
	}

	*first_unused = max;
	return 0;
}

 
int						 
xfs_bmap_last_before(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	xfs_fileoff_t		*last_block,	 
	int			whichfork)	 
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec	got;
	struct xfs_iext_cursor	icur;
	int			error;

	switch (ifp->if_format) {
	case XFS_DINODE_FMT_LOCAL:
		*last_block = 0;
		return 0;
	case XFS_DINODE_FMT_BTREE:
	case XFS_DINODE_FMT_EXTENTS:
		break;
	default:
		ASSERT(0);
		return -EFSCORRUPTED;
	}

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	if (!xfs_iext_lookup_extent_before(ip, ifp, last_block, &icur, &got))
		*last_block = 0;
	return 0;
}

int
xfs_bmap_last_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_bmbt_irec	*rec,
	int			*is_empty)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_iext_cursor	icur;
	int			error;

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	xfs_iext_last(ifp, &icur);
	if (!xfs_iext_get_extent(ifp, &icur, rec))
		*is_empty = 1;
	else
		*is_empty = 0;
	return 0;
}

 
STATIC int
xfs_bmap_isaeof(
	struct xfs_bmalloca	*bma,
	int			whichfork)
{
	struct xfs_bmbt_irec	rec;
	int			is_empty;
	int			error;

	bma->aeof = false;
	error = xfs_bmap_last_extent(NULL, bma->ip, whichfork, &rec,
				     &is_empty);
	if (error)
		return error;

	if (is_empty) {
		bma->aeof = true;
		return 0;
	}

	 
	bma->aeof = bma->offset >= rec.br_startoff + rec.br_blockcount ||
		(bma->offset >= rec.br_startoff &&
		 isnullstartblock(rec.br_startblock));
	return 0;
}

 
int
xfs_bmap_last_offset(
	struct xfs_inode	*ip,
	xfs_fileoff_t		*last_block,
	int			whichfork)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec	rec;
	int			is_empty;
	int			error;

	*last_block = 0;

	if (ifp->if_format == XFS_DINODE_FMT_LOCAL)
		return 0;

	if (XFS_IS_CORRUPT(ip->i_mount, !xfs_ifork_has_extents(ifp)))
		return -EFSCORRUPTED;

	error = xfs_bmap_last_extent(NULL, ip, whichfork, &rec, &is_empty);
	if (error || is_empty)
		return error;

	*last_block = rec.br_startoff + rec.br_blockcount;
	return 0;
}

 

 
STATIC int				 
xfs_bmap_add_extent_delay_real(
	struct xfs_bmalloca	*bma,
	int			whichfork)
{
	struct xfs_mount	*mp = bma->ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(bma->ip, whichfork);
	struct xfs_bmbt_irec	*new = &bma->got;
	int			error;	 
	int			i;	 
	xfs_fileoff_t		new_endoff;	 
	xfs_bmbt_irec_t		r[3];	 
					 
	int			rval=0;	 
	uint32_t		state = xfs_bmap_fork_to_state(whichfork);
	xfs_filblks_t		da_new;  
	xfs_filblks_t		da_old;  
	xfs_filblks_t		temp=0;	 
	int			tmp_rval;	 
	struct xfs_bmbt_irec	old;

	ASSERT(whichfork != XFS_ATTR_FORK);
	ASSERT(!isnullstartblock(new->br_startblock));
	ASSERT(!bma->cur ||
	       (bma->cur->bc_ino.flags & XFS_BTCUR_BMBT_WASDEL));

	XFS_STATS_INC(mp, xs_add_exlist);

#define	LEFT		r[0]
#define	RIGHT		r[1]
#define	PREV		r[2]

	 
	xfs_iext_get_extent(ifp, &bma->icur, &PREV);
	new_endoff = new->br_startoff + new->br_blockcount;
	ASSERT(isnullstartblock(PREV.br_startblock));
	ASSERT(PREV.br_startoff <= new->br_startoff);
	ASSERT(PREV.br_startoff + PREV.br_blockcount >= new_endoff);

	da_old = startblockval(PREV.br_startblock);
	da_new = 0;

	 
	if (PREV.br_startoff == new->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (PREV.br_startoff + PREV.br_blockcount == new_endoff)
		state |= BMAP_RIGHT_FILLING;

	 
	if (xfs_iext_peek_prev_extent(ifp, &bma->icur, &LEFT)) {
		state |= BMAP_LEFT_VALID;
		if (isnullstartblock(LEFT.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	if ((state & BMAP_LEFT_VALID) && !(state & BMAP_LEFT_DELAY) &&
	    LEFT.br_startoff + LEFT.br_blockcount == new->br_startoff &&
	    LEFT.br_startblock + LEFT.br_blockcount == new->br_startblock &&
	    LEFT.br_state == new->br_state &&
	    LEFT.br_blockcount + new->br_blockcount <= XFS_MAX_BMBT_EXTLEN)
		state |= BMAP_LEFT_CONTIG;

	 
	if (xfs_iext_peek_next_extent(ifp, &bma->icur, &RIGHT)) {
		state |= BMAP_RIGHT_VALID;
		if (isnullstartblock(RIGHT.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	if ((state & BMAP_RIGHT_VALID) && !(state & BMAP_RIGHT_DELAY) &&
	    new_endoff == RIGHT.br_startoff &&
	    new->br_startblock + new->br_blockcount == RIGHT.br_startblock &&
	    new->br_state == RIGHT.br_state &&
	    new->br_blockcount + RIGHT.br_blockcount <= XFS_MAX_BMBT_EXTLEN &&
	    ((state & (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING)) !=
		      (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING) ||
	     LEFT.br_blockcount + new->br_blockcount + RIGHT.br_blockcount
			<= XFS_MAX_BMBT_EXTLEN))
		state |= BMAP_RIGHT_CONTIG;

	error = 0;
	 
	switch (state & (BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
			 BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
	     BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		 
		LEFT.br_blockcount += PREV.br_blockcount + RIGHT.br_blockcount;

		xfs_iext_remove(bma->ip, &bma->icur, state);
		xfs_iext_remove(bma->ip, &bma->icur, state);
		xfs_iext_prev(ifp, &bma->icur);
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &LEFT);
		ifp->if_nextents--;

		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, &RIGHT, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_delete(bma->cur, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_decrement(bma->cur, 0, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(bma->cur, &LEFT);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
		 
		old = LEFT;
		LEFT.br_blockcount += PREV.br_blockcount;

		xfs_iext_remove(bma->ip, &bma->icur, state);
		xfs_iext_prev(ifp, &bma->icur);
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &LEFT);

		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(bma->cur, &LEFT);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		 
		PREV.br_startblock = new->br_startblock;
		PREV.br_blockcount += RIGHT.br_blockcount;
		PREV.br_state = new->br_state;

		xfs_iext_next(ifp, &bma->icur);
		xfs_iext_remove(bma->ip, &bma->icur, state);
		xfs_iext_prev(ifp, &bma->icur);
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &PREV);

		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, &RIGHT, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(bma->cur, &PREV);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		 
		PREV.br_startblock = new->br_startblock;
		PREV.br_state = new->br_state;
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &PREV);
		ifp->if_nextents++;

		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG:
		 
		old = LEFT;
		temp = PREV.br_blockcount - new->br_blockcount;
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
				startblockval(PREV.br_startblock));

		LEFT.br_blockcount += new->br_blockcount;

		PREV.br_blockcount = temp;
		PREV.br_startoff += new->br_blockcount;
		PREV.br_startblock = nullstartblock(da_new);

		xfs_iext_update_extent(bma->ip, state, &bma->icur, &PREV);
		xfs_iext_prev(ifp, &bma->icur);
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &LEFT);

		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(bma->cur, &LEFT);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING:
		 
		xfs_iext_update_extent(bma->ip, state, &bma->icur, new);
		ifp->if_nextents++;

		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}

		if (xfs_bmap_needs_btree(bma->ip, whichfork)) {
			error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
					&bma->cur, 1, &tmp_rval, whichfork);
			rval |= tmp_rval;
			if (error)
				goto done;
		}

		temp = PREV.br_blockcount - new->br_blockcount;
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock) -
			(bma->cur ? bma->cur->bc_ino.allocated : 0));

		PREV.br_startoff = new_endoff;
		PREV.br_blockcount = temp;
		PREV.br_startblock = nullstartblock(da_new);
		xfs_iext_next(ifp, &bma->icur);
		xfs_iext_insert(bma->ip, &bma->icur, &PREV, state);
		xfs_iext_prev(ifp, &bma->icur);
		break;

	case BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		 
		old = RIGHT;
		RIGHT.br_startoff = new->br_startoff;
		RIGHT.br_startblock = new->br_startblock;
		RIGHT.br_blockcount += new->br_blockcount;

		if (bma->cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(bma->cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(bma->cur, &RIGHT);
			if (error)
				goto done;
		}

		temp = PREV.br_blockcount - new->br_blockcount;
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock));

		PREV.br_blockcount = temp;
		PREV.br_startblock = nullstartblock(da_new);

		xfs_iext_update_extent(bma->ip, state, &bma->icur, &PREV);
		xfs_iext_next(ifp, &bma->icur);
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &RIGHT);
		break;

	case BMAP_RIGHT_FILLING:
		 
		xfs_iext_update_extent(bma->ip, state, &bma->icur, new);
		ifp->if_nextents++;

		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}

		if (xfs_bmap_needs_btree(bma->ip, whichfork)) {
			error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
				&bma->cur, 1, &tmp_rval, whichfork);
			rval |= tmp_rval;
			if (error)
				goto done;
		}

		temp = PREV.br_blockcount - new->br_blockcount;
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(bma->ip, temp),
			startblockval(PREV.br_startblock) -
			(bma->cur ? bma->cur->bc_ino.allocated : 0));

		PREV.br_startblock = nullstartblock(da_new);
		PREV.br_blockcount = temp;
		xfs_iext_insert(bma->ip, &bma->icur, &PREV, state);
		xfs_iext_next(ifp, &bma->icur);
		break;

	case 0:
		 
		old = PREV;

		 
		LEFT = *new;

		 
		RIGHT.br_state = PREV.br_state;
		RIGHT.br_startoff = new_endoff;
		RIGHT.br_blockcount =
			PREV.br_startoff + PREV.br_blockcount - new_endoff;
		RIGHT.br_startblock =
			nullstartblock(xfs_bmap_worst_indlen(bma->ip,
					RIGHT.br_blockcount));

		 
		PREV.br_blockcount = new->br_startoff - PREV.br_startoff;
		PREV.br_startblock =
			nullstartblock(xfs_bmap_worst_indlen(bma->ip,
					PREV.br_blockcount));
		xfs_iext_update_extent(bma->ip, state, &bma->icur, &PREV);

		xfs_iext_next(ifp, &bma->icur);
		xfs_iext_insert(bma->ip, &bma->icur, &RIGHT, state);
		xfs_iext_insert(bma->ip, &bma->icur, &LEFT, state);
		ifp->if_nextents++;

		if (bma->cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(bma->cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_insert(bma->cur, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}

		if (xfs_bmap_needs_btree(bma->ip, whichfork)) {
			error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
					&bma->cur, 1, &tmp_rval, whichfork);
			rval |= tmp_rval;
			if (error)
				goto done;
		}

		da_new = startblockval(PREV.br_startblock) +
			 startblockval(RIGHT.br_startblock);
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_FILLING | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_CONTIG:
	case BMAP_RIGHT_CONTIG:
		 
		ASSERT(0);
	}

	 
	if (!(bma->flags & XFS_BMAPI_NORMAP))
		xfs_rmap_map_extent(bma->tp, bma->ip, whichfork, new);

	 
	if (xfs_bmap_needs_btree(bma->ip, whichfork)) {
		int	tmp_logflags;	 

		ASSERT(bma->cur == NULL);
		error = xfs_bmap_extents_to_btree(bma->tp, bma->ip,
				&bma->cur, da_old > 0, &tmp_logflags,
				whichfork);
		bma->logflags |= tmp_logflags;
		if (error)
			goto done;
	}

	if (da_new != da_old)
		xfs_mod_delalloc(mp, (int64_t)da_new - da_old);

	if (bma->cur) {
		da_new += bma->cur->bc_ino.allocated;
		bma->cur->bc_ino.allocated = 0;
	}

	 
	if (da_new != da_old) {
		ASSERT(state == 0 || da_new < da_old);
		error = xfs_mod_fdblocks(mp, (int64_t)(da_old - da_new),
				false);
	}

	xfs_bmap_check_leaf_extents(bma->cur, bma->ip, whichfork);
done:
	if (whichfork != XFS_COW_FORK)
		bma->logflags |= rval;
	return error;
#undef	LEFT
#undef	RIGHT
#undef	PREV
}

 
int					 
xfs_bmap_add_extent_unwritten_real(
	struct xfs_trans	*tp,
	xfs_inode_t		*ip,	 
	int			whichfork,
	struct xfs_iext_cursor	*icur,
	struct xfs_btree_cur	**curp,	 
	xfs_bmbt_irec_t		*new,	 
	int			*logflagsp)  
{
	struct xfs_btree_cur	*cur;	 
	int			error;	 
	int			i;	 
	struct xfs_ifork	*ifp;	 
	xfs_fileoff_t		new_endoff;	 
	xfs_bmbt_irec_t		r[3];	 
					 
	int			rval=0;	 
	uint32_t		state = xfs_bmap_fork_to_state(whichfork);
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_bmbt_irec	old;

	*logflagsp = 0;

	cur = *curp;
	ifp = xfs_ifork_ptr(ip, whichfork);

	ASSERT(!isnullstartblock(new->br_startblock));

	XFS_STATS_INC(mp, xs_add_exlist);

#define	LEFT		r[0]
#define	RIGHT		r[1]
#define	PREV		r[2]

	 
	error = 0;
	xfs_iext_get_extent(ifp, icur, &PREV);
	ASSERT(new->br_state != PREV.br_state);
	new_endoff = new->br_startoff + new->br_blockcount;
	ASSERT(PREV.br_startoff <= new->br_startoff);
	ASSERT(PREV.br_startoff + PREV.br_blockcount >= new_endoff);

	 
	if (PREV.br_startoff == new->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (PREV.br_startoff + PREV.br_blockcount == new_endoff)
		state |= BMAP_RIGHT_FILLING;

	 
	if (xfs_iext_peek_prev_extent(ifp, icur, &LEFT)) {
		state |= BMAP_LEFT_VALID;
		if (isnullstartblock(LEFT.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	if ((state & BMAP_LEFT_VALID) && !(state & BMAP_LEFT_DELAY) &&
	    LEFT.br_startoff + LEFT.br_blockcount == new->br_startoff &&
	    LEFT.br_startblock + LEFT.br_blockcount == new->br_startblock &&
	    LEFT.br_state == new->br_state &&
	    LEFT.br_blockcount + new->br_blockcount <= XFS_MAX_BMBT_EXTLEN)
		state |= BMAP_LEFT_CONTIG;

	 
	if (xfs_iext_peek_next_extent(ifp, icur, &RIGHT)) {
		state |= BMAP_RIGHT_VALID;
		if (isnullstartblock(RIGHT.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	if ((state & BMAP_RIGHT_VALID) && !(state & BMAP_RIGHT_DELAY) &&
	    new_endoff == RIGHT.br_startoff &&
	    new->br_startblock + new->br_blockcount == RIGHT.br_startblock &&
	    new->br_state == RIGHT.br_state &&
	    new->br_blockcount + RIGHT.br_blockcount <= XFS_MAX_BMBT_EXTLEN &&
	    ((state & (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING)) !=
		      (BMAP_LEFT_CONTIG | BMAP_LEFT_FILLING |
		       BMAP_RIGHT_FILLING) ||
	     LEFT.br_blockcount + new->br_blockcount + RIGHT.br_blockcount
			<= XFS_MAX_BMBT_EXTLEN))
		state |= BMAP_RIGHT_CONTIG;

	 
	switch (state & (BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
			 BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG |
	     BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		 
		LEFT.br_blockcount += PREV.br_blockcount + RIGHT.br_blockcount;

		xfs_iext_remove(ip, icur, state);
		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &LEFT);
		ifp->if_nextents -= 2;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &RIGHT, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &LEFT);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
		 
		LEFT.br_blockcount += PREV.br_blockcount;

		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &LEFT);
		ifp->if_nextents--;
		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &PREV, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &LEFT);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		 
		PREV.br_blockcount += RIGHT.br_blockcount;
		PREV.br_state = new->br_state;

		xfs_iext_next(ifp, icur);
		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &PREV);
		ifp->if_nextents--;

		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &RIGHT, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_delete(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_decrement(cur, 0, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &PREV);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		 
		PREV.br_state = new->br_state;
		xfs_iext_update_extent(ip, state, icur, &PREV);

		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &PREV);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG:
		 
		LEFT.br_blockcount += new->br_blockcount;

		old = PREV;
		PREV.br_startoff += new->br_blockcount;
		PREV.br_startblock += new->br_blockcount;
		PREV.br_blockcount -= new->br_blockcount;

		xfs_iext_update_extent(ip, state, icur, &PREV);
		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &LEFT);

		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &PREV);
			if (error)
				goto done;
			error = xfs_btree_decrement(cur, 0, &i);
			if (error)
				goto done;
			error = xfs_bmbt_update(cur, &LEFT);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_FILLING:
		 
		old = PREV;
		PREV.br_startoff += new->br_blockcount;
		PREV.br_startblock += new->br_blockcount;
		PREV.br_blockcount -= new->br_blockcount;

		xfs_iext_update_extent(ip, state, icur, &PREV);
		xfs_iext_insert(ip, icur, new, state);
		ifp->if_nextents++;

		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &PREV);
			if (error)
				goto done;
			cur->bc_rec.b = *new;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}
		break;

	case BMAP_RIGHT_FILLING | BMAP_RIGHT_CONTIG:
		 
		old = PREV;
		PREV.br_blockcount -= new->br_blockcount;

		RIGHT.br_startoff = new->br_startoff;
		RIGHT.br_startblock = new->br_startblock;
		RIGHT.br_blockcount += new->br_blockcount;

		xfs_iext_update_extent(ip, state, icur, &PREV);
		xfs_iext_next(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &RIGHT);

		if (cur == NULL)
			rval = XFS_ILOG_DEXT;
		else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &PREV);
			if (error)
				goto done;
			error = xfs_btree_increment(cur, 0, &i);
			if (error)
				goto done;
			error = xfs_bmbt_update(cur, &RIGHT);
			if (error)
				goto done;
		}
		break;

	case BMAP_RIGHT_FILLING:
		 
		old = PREV;
		PREV.br_blockcount -= new->br_blockcount;

		xfs_iext_update_extent(ip, state, icur, &PREV);
		xfs_iext_next(ifp, icur);
		xfs_iext_insert(ip, icur, new, state);
		ifp->if_nextents++;

		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &PREV);
			if (error)
				goto done;
			error = xfs_bmbt_lookup_eq(cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}
		break;

	case 0:
		 
		old = PREV;
		PREV.br_blockcount = new->br_startoff - PREV.br_startoff;

		r[0] = *new;
		r[1].br_startoff = new_endoff;
		r[1].br_blockcount =
			old.br_startoff + old.br_blockcount - new_endoff;
		r[1].br_startblock = new->br_startblock + new->br_blockcount;
		r[1].br_state = PREV.br_state;

		xfs_iext_update_extent(ip, state, icur, &PREV);
		xfs_iext_next(ifp, icur);
		xfs_iext_insert(ip, icur, &r[1], state);
		xfs_iext_insert(ip, icur, &r[0], state);
		ifp->if_nextents += 2;

		if (cur == NULL)
			rval = XFS_ILOG_CORE | XFS_ILOG_DEXT;
		else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			 
			error = xfs_bmbt_update(cur, &r[1]);
			if (error)
				goto done;
			 
			cur->bc_rec.b = PREV;
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			 
			error = xfs_bmbt_lookup_eq(cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			 
			if ((error = xfs_btree_insert(cur, &i)))
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}
		break;

	case BMAP_LEFT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_FILLING | BMAP_RIGHT_CONTIG:
	case BMAP_RIGHT_FILLING | BMAP_LEFT_CONTIG:
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
	case BMAP_LEFT_CONTIG:
	case BMAP_RIGHT_CONTIG:
		 
		ASSERT(0);
	}

	 
	xfs_rmap_convert_extent(mp, tp, ip, whichfork, new);

	 
	if (xfs_bmap_needs_btree(ip, whichfork)) {
		int	tmp_logflags;	 

		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(tp, ip, &cur, 0,
				&tmp_logflags, whichfork);
		*logflagsp |= tmp_logflags;
		if (error)
			goto done;
	}

	 
	if (cur) {
		cur->bc_ino.allocated = 0;
		*curp = cur;
	}

	xfs_bmap_check_leaf_extents(*curp, ip, whichfork);
done:
	*logflagsp |= rval;
	return error;
#undef	LEFT
#undef	RIGHT
#undef	PREV
}

 
STATIC void
xfs_bmap_add_extent_hole_delay(
	xfs_inode_t		*ip,	 
	int			whichfork,
	struct xfs_iext_cursor	*icur,
	xfs_bmbt_irec_t		*new)	 
{
	struct xfs_ifork	*ifp;	 
	xfs_bmbt_irec_t		left;	 
	xfs_filblks_t		newlen=0;	 
	xfs_filblks_t		oldlen=0;	 
	xfs_bmbt_irec_t		right;	 
	uint32_t		state = xfs_bmap_fork_to_state(whichfork);
	xfs_filblks_t		temp;	  

	ifp = xfs_ifork_ptr(ip, whichfork);
	ASSERT(isnullstartblock(new->br_startblock));

	 
	if (xfs_iext_peek_prev_extent(ifp, icur, &left)) {
		state |= BMAP_LEFT_VALID;
		if (isnullstartblock(left.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	 
	if (xfs_iext_get_extent(ifp, icur, &right)) {
		state |= BMAP_RIGHT_VALID;
		if (isnullstartblock(right.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	 
	if ((state & BMAP_LEFT_VALID) && (state & BMAP_LEFT_DELAY) &&
	    left.br_startoff + left.br_blockcount == new->br_startoff &&
	    left.br_blockcount + new->br_blockcount <= XFS_MAX_BMBT_EXTLEN)
		state |= BMAP_LEFT_CONTIG;

	if ((state & BMAP_RIGHT_VALID) && (state & BMAP_RIGHT_DELAY) &&
	    new->br_startoff + new->br_blockcount == right.br_startoff &&
	    new->br_blockcount + right.br_blockcount <= XFS_MAX_BMBT_EXTLEN &&
	    (!(state & BMAP_LEFT_CONTIG) ||
	     (left.br_blockcount + new->br_blockcount +
	      right.br_blockcount <= XFS_MAX_BMBT_EXTLEN)))
		state |= BMAP_RIGHT_CONTIG;

	 
	switch (state & (BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
		 
		temp = left.br_blockcount + new->br_blockcount +
			right.br_blockcount;

		oldlen = startblockval(left.br_startblock) +
			startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
					 oldlen);
		left.br_startblock = nullstartblock(newlen);
		left.br_blockcount = temp;

		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &left);
		break;

	case BMAP_LEFT_CONTIG:
		 
		temp = left.br_blockcount + new->br_blockcount;

		oldlen = startblockval(left.br_startblock) +
			startblockval(new->br_startblock);
		newlen = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
					 oldlen);
		left.br_blockcount = temp;
		left.br_startblock = nullstartblock(newlen);

		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &left);
		break;

	case BMAP_RIGHT_CONTIG:
		 
		temp = new->br_blockcount + right.br_blockcount;
		oldlen = startblockval(new->br_startblock) +
			startblockval(right.br_startblock);
		newlen = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip, temp),
					 oldlen);
		right.br_startoff = new->br_startoff;
		right.br_startblock = nullstartblock(newlen);
		right.br_blockcount = temp;
		xfs_iext_update_extent(ip, state, icur, &right);
		break;

	case 0:
		 
		oldlen = newlen = 0;
		xfs_iext_insert(ip, icur, new, state);
		break;
	}
	if (oldlen != newlen) {
		ASSERT(oldlen > newlen);
		xfs_mod_fdblocks(ip->i_mount, (int64_t)(oldlen - newlen),
				 false);
		 
		xfs_mod_delalloc(ip->i_mount, (int64_t)newlen - oldlen);
	}
}

 
STATIC int				 
xfs_bmap_add_extent_hole_real(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_iext_cursor	*icur,
	struct xfs_btree_cur	**curp,
	struct xfs_bmbt_irec	*new,
	int			*logflagsp,
	uint32_t		flags)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_btree_cur	*cur = *curp;
	int			error;	 
	int			i;	 
	xfs_bmbt_irec_t		left;	 
	xfs_bmbt_irec_t		right;	 
	int			rval=0;	 
	uint32_t		state = xfs_bmap_fork_to_state(whichfork);
	struct xfs_bmbt_irec	old;

	ASSERT(!isnullstartblock(new->br_startblock));
	ASSERT(!cur || !(cur->bc_ino.flags & XFS_BTCUR_BMBT_WASDEL));

	XFS_STATS_INC(mp, xs_add_exlist);

	 
	if (xfs_iext_peek_prev_extent(ifp, icur, &left)) {
		state |= BMAP_LEFT_VALID;
		if (isnullstartblock(left.br_startblock))
			state |= BMAP_LEFT_DELAY;
	}

	 
	if (xfs_iext_get_extent(ifp, icur, &right)) {
		state |= BMAP_RIGHT_VALID;
		if (isnullstartblock(right.br_startblock))
			state |= BMAP_RIGHT_DELAY;
	}

	 
	if ((state & BMAP_LEFT_VALID) && !(state & BMAP_LEFT_DELAY) &&
	    left.br_startoff + left.br_blockcount == new->br_startoff &&
	    left.br_startblock + left.br_blockcount == new->br_startblock &&
	    left.br_state == new->br_state &&
	    left.br_blockcount + new->br_blockcount <= XFS_MAX_BMBT_EXTLEN)
		state |= BMAP_LEFT_CONTIG;

	if ((state & BMAP_RIGHT_VALID) && !(state & BMAP_RIGHT_DELAY) &&
	    new->br_startoff + new->br_blockcount == right.br_startoff &&
	    new->br_startblock + new->br_blockcount == right.br_startblock &&
	    new->br_state == right.br_state &&
	    new->br_blockcount + right.br_blockcount <= XFS_MAX_BMBT_EXTLEN &&
	    (!(state & BMAP_LEFT_CONTIG) ||
	     left.br_blockcount + new->br_blockcount +
	     right.br_blockcount <= XFS_MAX_BMBT_EXTLEN))
		state |= BMAP_RIGHT_CONTIG;

	error = 0;
	 
	switch (state & (BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG)) {
	case BMAP_LEFT_CONTIG | BMAP_RIGHT_CONTIG:
		 
		left.br_blockcount += new->br_blockcount + right.br_blockcount;

		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &left);
		ifp->if_nextents--;

		if (cur == NULL) {
			rval = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, &right, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
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
			error = xfs_bmbt_update(cur, &left);
			if (error)
				goto done;
		}
		break;

	case BMAP_LEFT_CONTIG:
		 
		old = left;
		left.br_blockcount += new->br_blockcount;

		xfs_iext_prev(ifp, icur);
		xfs_iext_update_extent(ip, state, icur, &left);

		if (cur == NULL) {
			rval = xfs_ilog_fext(whichfork);
		} else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &left);
			if (error)
				goto done;
		}
		break;

	case BMAP_RIGHT_CONTIG:
		 
		old = right;

		right.br_startoff = new->br_startoff;
		right.br_startblock = new->br_startblock;
		right.br_blockcount += new->br_blockcount;
		xfs_iext_update_extent(ip, state, icur, &right);

		if (cur == NULL) {
			rval = xfs_ilog_fext(whichfork);
		} else {
			rval = 0;
			error = xfs_bmbt_lookup_eq(cur, &old, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_bmbt_update(cur, &right);
			if (error)
				goto done;
		}
		break;

	case 0:
		 
		xfs_iext_insert(ip, icur, new, state);
		ifp->if_nextents++;

		if (cur == NULL) {
			rval = XFS_ILOG_CORE | xfs_ilog_fext(whichfork);
		} else {
			rval = XFS_ILOG_CORE;
			error = xfs_bmbt_lookup_eq(cur, new, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 0)) {
				error = -EFSCORRUPTED;
				goto done;
			}
			error = xfs_btree_insert(cur, &i);
			if (error)
				goto done;
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		}
		break;
	}

	 
	if (!(flags & XFS_BMAPI_NORMAP))
		xfs_rmap_map_extent(tp, ip, whichfork, new);

	 
	if (xfs_bmap_needs_btree(ip, whichfork)) {
		int	tmp_logflags;	 

		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(tp, ip, curp, 0,
				&tmp_logflags, whichfork);
		*logflagsp |= tmp_logflags;
		cur = *curp;
		if (error)
			goto done;
	}

	 
	if (cur)
		cur->bc_ino.allocated = 0;

	xfs_bmap_check_leaf_extents(cur, ip, whichfork);
done:
	*logflagsp |= rval;
	return error;
}

 

 
int
xfs_bmap_extsize_align(
	xfs_mount_t	*mp,
	xfs_bmbt_irec_t	*gotp,		 
	xfs_bmbt_irec_t	*prevp,		 
	xfs_extlen_t	extsz,		 
	int		rt,		 
	int		eof,		 
	int		delay,		 
	int		convert,	 
	xfs_fileoff_t	*offp,		 
	xfs_extlen_t	*lenp)		 
{
	xfs_fileoff_t	orig_off;	 
	xfs_extlen_t	orig_alen;	 
	xfs_fileoff_t	orig_end;	 
	xfs_fileoff_t	nexto;		 
	xfs_fileoff_t	prevo;		 
	xfs_fileoff_t	align_off;	 
	xfs_extlen_t	align_alen;	 
	xfs_extlen_t	temp;		 

	if (convert)
		return 0;

	orig_off = align_off = *offp;
	orig_alen = align_alen = *lenp;
	orig_end = orig_off + orig_alen;

	 
	if (!delay && !eof &&
	    (orig_off >= gotp->br_startoff) &&
	    (orig_end <= gotp->br_startoff + gotp->br_blockcount)) {
		return 0;
	}

	 
	div_u64_rem(orig_off, extsz, &temp);
	if (temp) {
		align_alen += temp;
		align_off -= temp;
	}

	 
	temp = (align_alen % extsz);
	if (temp)
		align_alen += extsz - temp;

	 
	while (align_alen > XFS_MAX_BMBT_EXTLEN)
		align_alen -= extsz;
	ASSERT(align_alen <= XFS_MAX_BMBT_EXTLEN);

	 
	if (prevp->br_startoff != NULLFILEOFF) {
		if (prevp->br_startblock == HOLESTARTBLOCK)
			prevo = prevp->br_startoff;
		else
			prevo = prevp->br_startoff + prevp->br_blockcount;
	} else
		prevo = 0;
	if (align_off != orig_off && align_off < prevo)
		align_off = prevo;
	 
	if (!eof && gotp->br_startoff != NULLFILEOFF) {
		if ((delay && gotp->br_startblock == HOLESTARTBLOCK) ||
		    (!delay && gotp->br_startblock == DELAYSTARTBLOCK))
			nexto = gotp->br_startoff + gotp->br_blockcount;
		else
			nexto = gotp->br_startoff;
	} else
		nexto = NULLFILEOFF;
	if (!eof &&
	    align_off + align_alen != orig_end &&
	    align_off + align_alen > nexto)
		align_off = nexto > align_alen ? nexto - align_alen : 0;
	 
	if (align_off != orig_off && align_off < prevo)
		align_off = prevo;
	if (align_off + align_alen != orig_end &&
	    align_off + align_alen > nexto &&
	    nexto != NULLFILEOFF) {
		ASSERT(nexto > prevo);
		align_alen = nexto - align_off;
	}

	 
	if (rt && (temp = (align_alen % mp->m_sb.sb_rextsize))) {
		 
		if (orig_off < align_off ||
		    orig_end > align_off + align_alen ||
		    align_alen - temp < orig_alen)
			return -EINVAL;
		 
		if (align_off + temp <= orig_off) {
			align_alen -= temp;
			align_off += temp;
		}
		 
		else if (align_off + align_alen - temp >= orig_end)
			align_alen -= temp;
		 
		else {
			align_alen -= orig_off - align_off;
			align_off = orig_off;
			align_alen -= align_alen % mp->m_sb.sb_rextsize;
		}
		 
		if (orig_off < align_off || orig_end > align_off + align_alen)
			return -EINVAL;
	} else {
		ASSERT(orig_off >= align_off);
		 
		ASSERT(orig_end <= align_off + align_alen ||
		       align_alen + extsz > XFS_MAX_BMBT_EXTLEN);
	}

#ifdef DEBUG
	if (!eof && gotp->br_startoff != NULLFILEOFF)
		ASSERT(align_off + align_alen <= gotp->br_startoff);
	if (prevp->br_startoff != NULLFILEOFF)
		ASSERT(align_off >= prevp->br_startoff + prevp->br_blockcount);
#endif

	*lenp = align_alen;
	*offp = align_off;
	return 0;
}

#define XFS_ALLOC_GAP_UNITS	4

void
xfs_bmap_adjacent(
	struct xfs_bmalloca	*ap)	 
{
	xfs_fsblock_t	adjust;		 
	xfs_mount_t	*mp;		 
	int		rt;		 

#define	ISVALID(x,y)	\
	(rt ? \
		(x) < mp->m_sb.sb_rblocks : \
		XFS_FSB_TO_AGNO(mp, x) == XFS_FSB_TO_AGNO(mp, y) && \
		XFS_FSB_TO_AGNO(mp, x) < mp->m_sb.sb_agcount && \
		XFS_FSB_TO_AGBNO(mp, x) < mp->m_sb.sb_agblocks)

	mp = ap->ip->i_mount;
	rt = XFS_IS_REALTIME_INODE(ap->ip) &&
		(ap->datatype & XFS_ALLOC_USERDATA);
	 
	if (ap->eof && ap->prev.br_startoff != NULLFILEOFF &&
	    !isnullstartblock(ap->prev.br_startblock) &&
	    ISVALID(ap->prev.br_startblock + ap->prev.br_blockcount,
		    ap->prev.br_startblock)) {
		ap->blkno = ap->prev.br_startblock + ap->prev.br_blockcount;
		 
		adjust = ap->offset -
			(ap->prev.br_startoff + ap->prev.br_blockcount);
		if (adjust &&
		    ISVALID(ap->blkno + adjust, ap->prev.br_startblock))
			ap->blkno += adjust;
	}
	 
	else if (!ap->eof) {
		xfs_fsblock_t	gotbno;		 
		xfs_fsblock_t	gotdiff=0;	 
		xfs_fsblock_t	prevbno;	 
		xfs_fsblock_t	prevdiff=0;	 

		 
		if (ap->prev.br_startoff != NULLFILEOFF &&
		    !isnullstartblock(ap->prev.br_startblock) &&
		    (prevbno = ap->prev.br_startblock +
			       ap->prev.br_blockcount) &&
		    ISVALID(prevbno, ap->prev.br_startblock)) {
			 
			adjust = prevdiff = ap->offset -
				(ap->prev.br_startoff +
				 ap->prev.br_blockcount);
			 
			if (prevdiff <= XFS_ALLOC_GAP_UNITS * ap->length &&
			    ISVALID(prevbno + prevdiff,
				    ap->prev.br_startblock))
				prevbno += adjust;
			else
				prevdiff += adjust;
		}
		 
		else
			prevbno = NULLFSBLOCK;
		 
		if (!isnullstartblock(ap->got.br_startblock)) {
			 
			adjust = gotdiff = ap->got.br_startoff - ap->offset;
			 
			gotbno = ap->got.br_startblock;
			 
			if (gotdiff <= XFS_ALLOC_GAP_UNITS * ap->length &&
			    ISVALID(gotbno - gotdiff, gotbno))
				gotbno -= adjust;
			else if (ISVALID(gotbno - ap->length, gotbno)) {
				gotbno -= ap->length;
				gotdiff += adjust - ap->length;
			} else
				gotdiff += adjust;
		}
		 
		else
			gotbno = NULLFSBLOCK;
		 
		if (prevbno != NULLFSBLOCK && gotbno != NULLFSBLOCK)
			ap->blkno = prevdiff <= gotdiff ? prevbno : gotbno;
		else if (prevbno != NULLFSBLOCK)
			ap->blkno = prevbno;
		else if (gotbno != NULLFSBLOCK)
			ap->blkno = gotbno;
	}
#undef ISVALID
}

int
xfs_bmap_longest_free_extent(
	struct xfs_perag	*pag,
	struct xfs_trans	*tp,
	xfs_extlen_t		*blen)
{
	xfs_extlen_t		longest;
	int			error = 0;

	if (!xfs_perag_initialised_agf(pag)) {
		error = xfs_alloc_read_agf(pag, tp, XFS_ALLOC_FLAG_TRYLOCK,
				NULL);
		if (error)
			return error;
	}

	longest = xfs_alloc_longest_free_extent(pag,
				xfs_alloc_min_freelist(pag->pag_mount, pag),
				xfs_ag_resv_needed(pag, XFS_AG_RESV_NONE));
	if (*blen < longest)
		*blen = longest;

	return 0;
}

static xfs_extlen_t
xfs_bmap_select_minlen(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	xfs_extlen_t		blen)
{

	 
	if (blen < ap->minlen)
		return ap->minlen;

	 
	if (blen < args->maxlen)
		return blen;
	return args->maxlen;
}

static int
xfs_bmap_btalloc_select_lengths(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	xfs_extlen_t		*blen)
{
	struct xfs_mount	*mp = args->mp;
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno, startag;
	int			error = 0;

	if (ap->tp->t_flags & XFS_TRANS_LOWMODE) {
		args->total = ap->minlen;
		args->minlen = ap->minlen;
		return 0;
	}

	args->total = ap->total;
	startag = XFS_FSB_TO_AGNO(mp, ap->blkno);
	if (startag == NULLAGNUMBER)
		startag = 0;

	*blen = 0;
	for_each_perag_wrap(mp, startag, agno, pag) {
		error = xfs_bmap_longest_free_extent(pag, args->tp, blen);
		if (error && error != -EAGAIN)
			break;
		error = 0;
		if (*blen >= args->maxlen)
			break;
	}
	if (pag)
		xfs_perag_rele(pag);

	args->minlen = xfs_bmap_select_minlen(ap, args, *blen);
	return error;
}

 
static void
xfs_bmap_btalloc_accounting(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args)
{
	if (ap->flags & XFS_BMAPI_COWFORK) {
		 
		if (ap->wasdel) {
			xfs_mod_delalloc(ap->ip->i_mount, -(int64_t)args->len);
			return;
		}

		 
		ap->ip->i_delayed_blks += args->len;
		xfs_trans_mod_dquot_byino(ap->tp, ap->ip, XFS_TRANS_DQ_RES_BLKS,
				-(long)args->len);
		return;
	}

	 
	ap->ip->i_nblocks += args->len;
	xfs_trans_log_inode(ap->tp, ap->ip, XFS_ILOG_CORE);
	if (ap->wasdel) {
		ap->ip->i_delayed_blks -= args->len;
		xfs_mod_delalloc(ap->ip->i_mount, -(int64_t)args->len);
	}
	xfs_trans_mod_dquot_byino(ap->tp, ap->ip,
		ap->wasdel ? XFS_TRANS_DQ_DELBCOUNT : XFS_TRANS_DQ_BCOUNT,
		args->len);
}

static int
xfs_bmap_compute_alignments(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args)
{
	struct xfs_mount	*mp = args->mp;
	xfs_extlen_t		align = 0;  
	int			stripe_align = 0;

	 
	if (mp->m_swidth && xfs_has_swalloc(mp))
		stripe_align = mp->m_swidth;
	else if (mp->m_dalign)
		stripe_align = mp->m_dalign;

	if (ap->flags & XFS_BMAPI_COWFORK)
		align = xfs_get_cowextsz_hint(ap->ip);
	else if (ap->datatype & XFS_ALLOC_USERDATA)
		align = xfs_get_extsz_hint(ap->ip);
	if (align) {
		if (xfs_bmap_extsize_align(mp, &ap->got, &ap->prev, align, 0,
					ap->eof, 0, ap->conv, &ap->offset,
					&ap->length))
			ASSERT(0);
		ASSERT(ap->length);
	}

	 
	if (align) {
		args->prod = align;
		div_u64_rem(ap->offset, args->prod, &args->mod);
		if (args->mod)
			args->mod = args->prod - args->mod;
	} else if (mp->m_sb.sb_blocksize >= PAGE_SIZE) {
		args->prod = 1;
		args->mod = 0;
	} else {
		args->prod = PAGE_SIZE >> mp->m_sb.sb_blocklog;
		div_u64_rem(ap->offset, args->prod, &args->mod);
		if (args->mod)
			args->mod = args->prod - args->mod;
	}

	return stripe_align;
}

static void
xfs_bmap_process_allocated_extent(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	xfs_fileoff_t		orig_offset,
	xfs_extlen_t		orig_length)
{
	ap->blkno = args->fsbno;
	ap->length = args->len;
	 
	if (ap->length <= orig_length)
		ap->offset = orig_offset;
	else if (ap->offset + ap->length < orig_offset + orig_length)
		ap->offset = orig_offset + orig_length - ap->length;
	xfs_bmap_btalloc_accounting(ap, args);
}

#ifdef DEBUG
static int
xfs_bmap_exact_minlen_extent_alloc(
	struct xfs_bmalloca	*ap)
{
	struct xfs_mount	*mp = ap->ip->i_mount;
	struct xfs_alloc_arg	args = { .tp = ap->tp, .mp = mp };
	xfs_fileoff_t		orig_offset;
	xfs_extlen_t		orig_length;
	int			error;

	ASSERT(ap->length);

	if (ap->minlen != 1) {
		ap->blkno = NULLFSBLOCK;
		ap->length = 0;
		return 0;
	}

	orig_offset = ap->offset;
	orig_length = ap->length;

	args.alloc_minlen_only = 1;

	xfs_bmap_compute_alignments(ap, &args);

	 
	ap->blkno = XFS_AGB_TO_FSB(mp, 0, 0);

	args.oinfo = XFS_RMAP_OINFO_SKIP_UPDATE;
	args.minlen = args.maxlen = ap->minlen;
	args.total = ap->total;

	args.alignment = 1;
	args.minalignslop = 0;

	args.minleft = ap->minleft;
	args.wasdel = ap->wasdel;
	args.resv = XFS_AG_RESV_NONE;
	args.datatype = ap->datatype;

	error = xfs_alloc_vextent_first_ag(&args, ap->blkno);
	if (error)
		return error;

	if (args.fsbno != NULLFSBLOCK) {
		xfs_bmap_process_allocated_extent(ap, &args, orig_offset,
			orig_length);
	} else {
		ap->blkno = NULLFSBLOCK;
		ap->length = 0;
	}

	return 0;
}
#else

#define xfs_bmap_exact_minlen_extent_alloc(bma) (-EFSCORRUPTED)

#endif

 
static int
xfs_bmap_btalloc_at_eof(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	xfs_extlen_t		blen,
	int			stripe_align,
	bool			ag_only)
{
	struct xfs_mount	*mp = args->mp;
	struct xfs_perag	*caller_pag = args->pag;
	int			error;

	 
	if (ap->offset) {
		xfs_extlen_t	nextminlen = 0;

		 
		args->alignment = 1;
		if (blen > stripe_align && blen <= args->maxlen)
			nextminlen = blen - stripe_align;
		else
			nextminlen = args->minlen;
		if (nextminlen + stripe_align > args->minlen + 1)
			args->minalignslop = nextminlen + stripe_align -
					args->minlen - 1;
		else
			args->minalignslop = 0;

		if (!caller_pag)
			args->pag = xfs_perag_get(mp, XFS_FSB_TO_AGNO(mp, ap->blkno));
		error = xfs_alloc_vextent_exact_bno(args, ap->blkno);
		if (!caller_pag) {
			xfs_perag_put(args->pag);
			args->pag = NULL;
		}
		if (error)
			return error;

		if (args->fsbno != NULLFSBLOCK)
			return 0;
		 
		args->alignment = stripe_align;
		args->minlen = nextminlen;
		args->minalignslop = 0;
	} else {
		 
		args->alignment = stripe_align;
		if (blen > args->alignment &&
		    blen <= args->maxlen + args->alignment)
			args->minlen = blen - args->alignment;
		args->minalignslop = 0;
	}

	if (ag_only) {
		error = xfs_alloc_vextent_near_bno(args, ap->blkno);
	} else {
		args->pag = NULL;
		error = xfs_alloc_vextent_start_ag(args, ap->blkno);
		ASSERT(args->pag == NULL);
		args->pag = caller_pag;
	}
	if (error)
		return error;

	if (args->fsbno != NULLFSBLOCK)
		return 0;

	 
	args->alignment = 1;
	return 0;
}

 
int
xfs_bmap_btalloc_low_space(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args)
{
	int			error;

	if (args->minlen > ap->minlen) {
		args->minlen = ap->minlen;
		error = xfs_alloc_vextent_start_ag(args, ap->blkno);
		if (error || args->fsbno != NULLFSBLOCK)
			return error;
	}

	 
	args->total = ap->minlen;
	error = xfs_alloc_vextent_first_ag(args, 0);
	if (error)
		return error;
	ap->tp->t_flags |= XFS_TRANS_LOWMODE;
	return 0;
}

static int
xfs_bmap_btalloc_filestreams(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	int			stripe_align)
{
	xfs_extlen_t		blen = 0;
	int			error = 0;


	error = xfs_filestream_select_ag(ap, args, &blen);
	if (error)
		return error;
	ASSERT(args->pag);

	 
	if (ap->tp->t_flags & XFS_TRANS_LOWMODE) {
		args->minlen = ap->minlen;
		ASSERT(args->fsbno == NULLFSBLOCK);
		goto out_low_space;
	}

	args->minlen = xfs_bmap_select_minlen(ap, args, blen);
	if (ap->aeof)
		error = xfs_bmap_btalloc_at_eof(ap, args, blen, stripe_align,
				true);

	if (!error && args->fsbno == NULLFSBLOCK)
		error = xfs_alloc_vextent_near_bno(args, ap->blkno);

out_low_space:
	 
	xfs_perag_rele(args->pag);
	args->pag = NULL;
	if (error || args->fsbno != NULLFSBLOCK)
		return error;

	return xfs_bmap_btalloc_low_space(ap, args);
}

static int
xfs_bmap_btalloc_best_length(
	struct xfs_bmalloca	*ap,
	struct xfs_alloc_arg	*args,
	int			stripe_align)
{
	xfs_extlen_t		blen = 0;
	int			error;

	ap->blkno = XFS_INO_TO_FSB(args->mp, ap->ip->i_ino);
	xfs_bmap_adjacent(ap);

	 
	error = xfs_bmap_btalloc_select_lengths(ap, args, &blen);
	if (error)
		return error;

	 
	if (ap->aeof && !(ap->tp->t_flags & XFS_TRANS_LOWMODE)) {
		error = xfs_bmap_btalloc_at_eof(ap, args, blen, stripe_align,
				false);
		if (error || args->fsbno != NULLFSBLOCK)
			return error;
	}

	error = xfs_alloc_vextent_start_ag(args, ap->blkno);
	if (error || args->fsbno != NULLFSBLOCK)
		return error;

	return xfs_bmap_btalloc_low_space(ap, args);
}

static int
xfs_bmap_btalloc(
	struct xfs_bmalloca	*ap)
{
	struct xfs_mount	*mp = ap->ip->i_mount;
	struct xfs_alloc_arg	args = {
		.tp		= ap->tp,
		.mp		= mp,
		.fsbno		= NULLFSBLOCK,
		.oinfo		= XFS_RMAP_OINFO_SKIP_UPDATE,
		.minleft	= ap->minleft,
		.wasdel		= ap->wasdel,
		.resv		= XFS_AG_RESV_NONE,
		.datatype	= ap->datatype,
		.alignment	= 1,
		.minalignslop	= 0,
	};
	xfs_fileoff_t		orig_offset;
	xfs_extlen_t		orig_length;
	int			error;
	int			stripe_align;

	ASSERT(ap->length);
	orig_offset = ap->offset;
	orig_length = ap->length;

	stripe_align = xfs_bmap_compute_alignments(ap, &args);

	 
	args.maxlen = min(ap->length, mp->m_ag_max_usable);

	if ((ap->datatype & XFS_ALLOC_USERDATA) &&
	    xfs_inode_is_filestream(ap->ip))
		error = xfs_bmap_btalloc_filestreams(ap, &args, stripe_align);
	else
		error = xfs_bmap_btalloc_best_length(ap, &args, stripe_align);
	if (error)
		return error;

	if (args.fsbno != NULLFSBLOCK) {
		xfs_bmap_process_allocated_extent(ap, &args, orig_offset,
			orig_length);
	} else {
		ap->blkno = NULLFSBLOCK;
		ap->length = 0;
	}
	return 0;
}

 
void
xfs_trim_extent(
	struct xfs_bmbt_irec	*irec,
	xfs_fileoff_t		bno,
	xfs_filblks_t		len)
{
	xfs_fileoff_t		distance;
	xfs_fileoff_t		end = bno + len;

	if (irec->br_startoff + irec->br_blockcount <= bno ||
	    irec->br_startoff >= end) {
		irec->br_blockcount = 0;
		return;
	}

	if (irec->br_startoff < bno) {
		distance = bno - irec->br_startoff;
		if (isnullstartblock(irec->br_startblock))
			irec->br_startblock = DELAYSTARTBLOCK;
		if (irec->br_startblock != DELAYSTARTBLOCK &&
		    irec->br_startblock != HOLESTARTBLOCK)
			irec->br_startblock += distance;
		irec->br_startoff += distance;
		irec->br_blockcount -= distance;
	}

	if (end < irec->br_startoff + irec->br_blockcount) {
		distance = irec->br_startoff + irec->br_blockcount - end;
		irec->br_blockcount -= distance;
	}
}

 
STATIC void
xfs_bmapi_trim_map(
	struct xfs_bmbt_irec	*mval,
	struct xfs_bmbt_irec	*got,
	xfs_fileoff_t		*bno,
	xfs_filblks_t		len,
	xfs_fileoff_t		obno,
	xfs_fileoff_t		end,
	int			n,
	uint32_t		flags)
{
	if ((flags & XFS_BMAPI_ENTIRE) ||
	    got->br_startoff + got->br_blockcount <= obno) {
		*mval = *got;
		if (isnullstartblock(got->br_startblock))
			mval->br_startblock = DELAYSTARTBLOCK;
		return;
	}

	if (obno > *bno)
		*bno = obno;
	ASSERT((*bno >= obno) || (n == 0));
	ASSERT(*bno < end);
	mval->br_startoff = *bno;
	if (isnullstartblock(got->br_startblock))
		mval->br_startblock = DELAYSTARTBLOCK;
	else
		mval->br_startblock = got->br_startblock +
					(*bno - got->br_startoff);
	 
	mval->br_blockcount = XFS_FILBLKS_MIN(end - *bno,
			got->br_blockcount - (*bno - got->br_startoff));
	mval->br_state = got->br_state;
	ASSERT(mval->br_blockcount <= len);
	return;
}

 
STATIC void
xfs_bmapi_update_map(
	struct xfs_bmbt_irec	**map,
	xfs_fileoff_t		*bno,
	xfs_filblks_t		*len,
	xfs_fileoff_t		obno,
	xfs_fileoff_t		end,
	int			*n,
	uint32_t		flags)
{
	xfs_bmbt_irec_t	*mval = *map;

	ASSERT((flags & XFS_BMAPI_ENTIRE) ||
	       ((mval->br_startoff + mval->br_blockcount) <= end));
	ASSERT((flags & XFS_BMAPI_ENTIRE) || (mval->br_blockcount <= *len) ||
	       (mval->br_startoff < obno));

	*bno = mval->br_startoff + mval->br_blockcount;
	*len = end - *bno;
	if (*n > 0 && mval->br_startoff == mval[-1].br_startoff) {
		 
		ASSERT(mval->br_startblock == mval[-1].br_startblock);
		ASSERT(mval->br_blockcount > mval[-1].br_blockcount);
		ASSERT(mval->br_state == mval[-1].br_state);
		mval[-1].br_blockcount = mval->br_blockcount;
		mval[-1].br_state = mval->br_state;
	} else if (*n > 0 && mval->br_startblock != DELAYSTARTBLOCK &&
		   mval[-1].br_startblock != DELAYSTARTBLOCK &&
		   mval[-1].br_startblock != HOLESTARTBLOCK &&
		   mval->br_startblock == mval[-1].br_startblock +
					  mval[-1].br_blockcount &&
		   mval[-1].br_state == mval->br_state) {
		ASSERT(mval->br_startoff ==
		       mval[-1].br_startoff + mval[-1].br_blockcount);
		mval[-1].br_blockcount += mval->br_blockcount;
	} else if (*n > 0 &&
		   mval->br_startblock == DELAYSTARTBLOCK &&
		   mval[-1].br_startblock == DELAYSTARTBLOCK &&
		   mval->br_startoff ==
		   mval[-1].br_startoff + mval[-1].br_blockcount) {
		mval[-1].br_blockcount += mval->br_blockcount;
		mval[-1].br_state = mval->br_state;
	} else if (!((*n == 0) &&
		     ((mval->br_startoff + mval->br_blockcount) <=
		      obno))) {
		mval++;
		(*n)++;
	}
	*map = mval;
}

 
int
xfs_bmapi_read(
	struct xfs_inode	*ip,
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	struct xfs_bmbt_irec	*mval,
	int			*nmap,
	uint32_t		flags)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			whichfork = xfs_bmapi_whichfork(flags);
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec	got;
	xfs_fileoff_t		obno;
	xfs_fileoff_t		end;
	struct xfs_iext_cursor	icur;
	int			error;
	bool			eof = false;
	int			n = 0;

	ASSERT(*nmap >= 1);
	ASSERT(!(flags & ~(XFS_BMAPI_ATTRFORK | XFS_BMAPI_ENTIRE)));
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_SHARED|XFS_ILOCK_EXCL));

	if (WARN_ON_ONCE(!ifp))
		return -EFSCORRUPTED;

	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BMAPIFORMAT))
		return -EFSCORRUPTED;

	if (xfs_is_shutdown(mp))
		return -EIO;

	XFS_STATS_INC(mp, xs_blk_mapr);

	error = xfs_iread_extents(NULL, ip, whichfork);
	if (error)
		return error;

	if (!xfs_iext_lookup_extent(ip, ifp, bno, &icur, &got))
		eof = true;
	end = bno + len;
	obno = bno;

	while (bno < end && n < *nmap) {
		 
		if (eof)
			got.br_startoff = end;
		if (got.br_startoff > bno) {
			 
			mval->br_startoff = bno;
			mval->br_startblock = HOLESTARTBLOCK;
			mval->br_blockcount =
				XFS_FILBLKS_MIN(len, got.br_startoff - bno);
			mval->br_state = XFS_EXT_NORM;
			bno += mval->br_blockcount;
			len -= mval->br_blockcount;
			mval++;
			n++;
			continue;
		}

		 
		xfs_bmapi_trim_map(mval, &got, &bno, len, obno, end, n, flags);
		xfs_bmapi_update_map(&mval, &bno, &len, obno, end, &n, flags);

		 
		if (bno >= end || n >= *nmap)
			break;

		 
		if (!xfs_iext_next_extent(ifp, &icur, &got))
			eof = true;
	}
	*nmap = n;
	return 0;
}

 
int
xfs_bmapi_reserve_delalloc(
	struct xfs_inode	*ip,
	int			whichfork,
	xfs_fileoff_t		off,
	xfs_filblks_t		len,
	xfs_filblks_t		prealloc,
	struct xfs_bmbt_irec	*got,
	struct xfs_iext_cursor	*icur,
	int			eof)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	xfs_extlen_t		alen;
	xfs_extlen_t		indlen;
	int			error;
	xfs_fileoff_t		aoff = off;

	 
	alen = XFS_FILBLKS_MIN(len + prealloc, XFS_MAX_BMBT_EXTLEN);
	if (!eof)
		alen = XFS_FILBLKS_MIN(alen, got->br_startoff - aoff);
	if (prealloc && alen >= len)
		prealloc = alen - len;

	 
	if (whichfork == XFS_COW_FORK) {
		struct xfs_bmbt_irec	prev;
		xfs_extlen_t		extsz = xfs_get_cowextsz_hint(ip);

		if (!xfs_iext_peek_prev_extent(ifp, icur, &prev))
			prev.br_startoff = NULLFILEOFF;

		error = xfs_bmap_extsize_align(mp, got, &prev, extsz, 0, eof,
					       1, 0, &aoff, &alen);
		ASSERT(!error);
	}

	 
	error = xfs_quota_reserve_blkres(ip, alen);
	if (error)
		return error;

	 
	indlen = (xfs_extlen_t)xfs_bmap_worst_indlen(ip, alen);
	ASSERT(indlen > 0);

	error = xfs_mod_fdblocks(mp, -((int64_t)alen), false);
	if (error)
		goto out_unreserve_quota;

	error = xfs_mod_fdblocks(mp, -((int64_t)indlen), false);
	if (error)
		goto out_unreserve_blocks;


	ip->i_delayed_blks += alen;
	xfs_mod_delalloc(ip->i_mount, alen + indlen);

	got->br_startoff = aoff;
	got->br_startblock = nullstartblock(indlen);
	got->br_blockcount = alen;
	got->br_state = XFS_EXT_NORM;

	xfs_bmap_add_extent_hole_delay(ip, whichfork, icur, got);

	 
	if (whichfork == XFS_DATA_FORK && prealloc)
		xfs_inode_set_eofblocks_tag(ip);
	if (whichfork == XFS_COW_FORK && (prealloc || aoff < off || alen > len))
		xfs_inode_set_cowblocks_tag(ip);

	return 0;

out_unreserve_blocks:
	xfs_mod_fdblocks(mp, alen, false);
out_unreserve_quota:
	if (XFS_IS_QUOTA_ON(mp))
		xfs_quota_unreserve_blkres(ip, alen);
	return error;
}

static int
xfs_bmap_alloc_userdata(
	struct xfs_bmalloca	*bma)
{
	struct xfs_mount	*mp = bma->ip->i_mount;
	int			whichfork = xfs_bmapi_whichfork(bma->flags);
	int			error;

	 
	bma->datatype = XFS_ALLOC_NOBUSY;
	if (whichfork == XFS_DATA_FORK || whichfork == XFS_COW_FORK) {
		bma->datatype |= XFS_ALLOC_USERDATA;
		if (bma->offset == 0)
			bma->datatype |= XFS_ALLOC_INITIAL_USER_DATA;

		if (mp->m_dalign && bma->length >= mp->m_dalign) {
			error = xfs_bmap_isaeof(bma, whichfork);
			if (error)
				return error;
		}

		if (XFS_IS_REALTIME_INODE(bma->ip))
			return xfs_bmap_rtalloc(bma);
	}

	if (unlikely(XFS_TEST_ERROR(false, mp,
			XFS_ERRTAG_BMAP_ALLOC_MINLEN_EXTENT)))
		return xfs_bmap_exact_minlen_extent_alloc(bma);

	return xfs_bmap_btalloc(bma);
}

static int
xfs_bmapi_allocate(
	struct xfs_bmalloca	*bma)
{
	struct xfs_mount	*mp = bma->ip->i_mount;
	int			whichfork = xfs_bmapi_whichfork(bma->flags);
	struct xfs_ifork	*ifp = xfs_ifork_ptr(bma->ip, whichfork);
	int			tmp_logflags = 0;
	int			error;

	ASSERT(bma->length > 0);

	 
	if (bma->wasdel) {
		bma->length = (xfs_extlen_t)bma->got.br_blockcount;
		bma->offset = bma->got.br_startoff;
		if (!xfs_iext_peek_prev_extent(ifp, &bma->icur, &bma->prev))
			bma->prev.br_startoff = NULLFILEOFF;
	} else {
		bma->length = XFS_FILBLKS_MIN(bma->length, XFS_MAX_BMBT_EXTLEN);
		if (!bma->eof)
			bma->length = XFS_FILBLKS_MIN(bma->length,
					bma->got.br_startoff - bma->offset);
	}

	if (bma->flags & XFS_BMAPI_CONTIG)
		bma->minlen = bma->length;
	else
		bma->minlen = 1;

	if (bma->flags & XFS_BMAPI_METADATA) {
		if (unlikely(XFS_TEST_ERROR(false, mp,
				XFS_ERRTAG_BMAP_ALLOC_MINLEN_EXTENT)))
			error = xfs_bmap_exact_minlen_extent_alloc(bma);
		else
			error = xfs_bmap_btalloc(bma);
	} else {
		error = xfs_bmap_alloc_userdata(bma);
	}
	if (error || bma->blkno == NULLFSBLOCK)
		return error;

	if (bma->flags & XFS_BMAPI_ZERO) {
		error = xfs_zero_extent(bma->ip, bma->blkno, bma->length);
		if (error)
			return error;
	}

	if (ifp->if_format == XFS_DINODE_FMT_BTREE && !bma->cur)
		bma->cur = xfs_bmbt_init_cursor(mp, bma->tp, bma->ip, whichfork);
	 
	bma->nallocs++;

	if (bma->cur)
		bma->cur->bc_ino.flags =
			bma->wasdel ? XFS_BTCUR_BMBT_WASDEL : 0;

	bma->got.br_startoff = bma->offset;
	bma->got.br_startblock = bma->blkno;
	bma->got.br_blockcount = bma->length;
	bma->got.br_state = XFS_EXT_NORM;

	if (bma->flags & XFS_BMAPI_PREALLOC)
		bma->got.br_state = XFS_EXT_UNWRITTEN;

	if (bma->wasdel)
		error = xfs_bmap_add_extent_delay_real(bma, whichfork);
	else
		error = xfs_bmap_add_extent_hole_real(bma->tp, bma->ip,
				whichfork, &bma->icur, &bma->cur, &bma->got,
				&bma->logflags, bma->flags);

	bma->logflags |= tmp_logflags;
	if (error)
		return error;

	 
	xfs_iext_get_extent(ifp, &bma->icur, &bma->got);

	ASSERT(bma->got.br_startoff <= bma->offset);
	ASSERT(bma->got.br_startoff + bma->got.br_blockcount >=
	       bma->offset + bma->length);
	ASSERT(bma->got.br_state == XFS_EXT_NORM ||
	       bma->got.br_state == XFS_EXT_UNWRITTEN);
	return 0;
}

STATIC int
xfs_bmapi_convert_unwritten(
	struct xfs_bmalloca	*bma,
	struct xfs_bmbt_irec	*mval,
	xfs_filblks_t		len,
	uint32_t		flags)
{
	int			whichfork = xfs_bmapi_whichfork(flags);
	struct xfs_ifork	*ifp = xfs_ifork_ptr(bma->ip, whichfork);
	int			tmp_logflags = 0;
	int			error;

	 
	if (mval->br_state == XFS_EXT_UNWRITTEN &&
	    (flags & XFS_BMAPI_PREALLOC))
		return 0;

	 
	if (mval->br_state == XFS_EXT_NORM &&
	    (flags & (XFS_BMAPI_PREALLOC | XFS_BMAPI_CONVERT)) !=
			(XFS_BMAPI_PREALLOC | XFS_BMAPI_CONVERT))
		return 0;

	 
	ASSERT(mval->br_blockcount <= len);
	if (ifp->if_format == XFS_DINODE_FMT_BTREE && !bma->cur) {
		bma->cur = xfs_bmbt_init_cursor(bma->ip->i_mount, bma->tp,
					bma->ip, whichfork);
	}
	mval->br_state = (mval->br_state == XFS_EXT_UNWRITTEN)
				? XFS_EXT_NORM : XFS_EXT_UNWRITTEN;

	 
	if (flags & XFS_BMAPI_ZERO) {
		error = xfs_zero_extent(bma->ip, mval->br_startblock,
					mval->br_blockcount);
		if (error)
			return error;
	}

	error = xfs_bmap_add_extent_unwritten_real(bma->tp, bma->ip, whichfork,
			&bma->icur, &bma->cur, mval, &tmp_logflags);
	 
	if (whichfork != XFS_COW_FORK)
		bma->logflags |= tmp_logflags | XFS_ILOG_CORE;
	if (error)
		return error;

	 
	xfs_iext_get_extent(ifp, &bma->icur, &bma->got);

	 
	if (mval->br_blockcount < len)
		return -EAGAIN;
	return 0;
}

xfs_extlen_t
xfs_bmapi_minleft(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			fork)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, fork);

	if (tp && tp->t_highest_agno != NULLAGNUMBER)
		return 0;
	if (ifp->if_format != XFS_DINODE_FMT_BTREE)
		return 1;
	return be16_to_cpu(ifp->if_broot->bb_level) + 1;
}

 
static void
xfs_bmapi_finish(
	struct xfs_bmalloca	*bma,
	int			whichfork,
	int			error)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(bma->ip, whichfork);

	if ((bma->logflags & xfs_ilog_fext(whichfork)) &&
	    ifp->if_format != XFS_DINODE_FMT_EXTENTS)
		bma->logflags &= ~xfs_ilog_fext(whichfork);
	else if ((bma->logflags & xfs_ilog_fbroot(whichfork)) &&
		 ifp->if_format != XFS_DINODE_FMT_BTREE)
		bma->logflags &= ~xfs_ilog_fbroot(whichfork);

	if (bma->logflags)
		xfs_trans_log_inode(bma->tp, bma->ip, bma->logflags);
	if (bma->cur)
		xfs_btree_del_cursor(bma->cur, error);
}

 
int
xfs_bmapi_write(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	xfs_fileoff_t		bno,		 
	xfs_filblks_t		len,		 
	uint32_t		flags,		 
	xfs_extlen_t		total,		 
	struct xfs_bmbt_irec	*mval,		 
	int			*nmap)		 
{
	struct xfs_bmalloca	bma = {
		.tp		= tp,
		.ip		= ip,
		.total		= total,
	};
	struct xfs_mount	*mp = ip->i_mount;
	int			whichfork = xfs_bmapi_whichfork(flags);
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	xfs_fileoff_t		end;		 
	bool			eof = false;	 
	int			error;		 
	int			n;		 
	xfs_fileoff_t		obno;		 

#ifdef DEBUG
	xfs_fileoff_t		orig_bno;	 
	int			orig_flags;	 
	xfs_filblks_t		orig_len;	 
	struct xfs_bmbt_irec	*orig_mval;	 
	int			orig_nmap;	 

	orig_bno = bno;
	orig_len = len;
	orig_flags = flags;
	orig_mval = mval;
	orig_nmap = *nmap;
#endif

	ASSERT(*nmap >= 1);
	ASSERT(*nmap <= XFS_BMAP_MAX_NMAP);
	ASSERT(tp != NULL);
	ASSERT(len > 0);
	ASSERT(ifp->if_format != XFS_DINODE_FMT_LOCAL);
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(!(flags & XFS_BMAPI_REMAP));

	 
	ASSERT((flags & (XFS_BMAPI_METADATA | XFS_BMAPI_ZERO)) !=
			(XFS_BMAPI_METADATA | XFS_BMAPI_ZERO));
	 
	ASSERT((flags & (XFS_BMAPI_PREALLOC | XFS_BMAPI_ZERO)) !=
			(XFS_BMAPI_PREALLOC | XFS_BMAPI_ZERO));

	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BMAPIFORMAT)) {
		return -EFSCORRUPTED;
	}

	if (xfs_is_shutdown(mp))
		return -EIO;

	XFS_STATS_INC(mp, xs_blk_mapw);

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		goto error0;

	if (!xfs_iext_lookup_extent(ip, ifp, bno, &bma.icur, &bma.got))
		eof = true;
	if (!xfs_iext_peek_prev_extent(ifp, &bma.icur, &bma.prev))
		bma.prev.br_startoff = NULLFILEOFF;
	bma.minleft = xfs_bmapi_minleft(tp, ip, whichfork);

	n = 0;
	end = bno + len;
	obno = bno;
	while (bno < end && n < *nmap) {
		bool			need_alloc = false, wasdelay = false;

		 
		if (eof || bma.got.br_startoff > bno) {
			 
			ASSERT(!((flags & XFS_BMAPI_CONVERT) &&
			         (flags & XFS_BMAPI_COWFORK)));

			need_alloc = true;
		} else if (isnullstartblock(bma.got.br_startblock)) {
			wasdelay = true;
		}

		 
		if (need_alloc || wasdelay) {
			bma.eof = eof;
			bma.conv = !!(flags & XFS_BMAPI_CONVERT);
			bma.wasdel = wasdelay;
			bma.offset = bno;
			bma.flags = flags;

			 
			if (len > (xfs_filblks_t)XFS_MAX_BMBT_EXTLEN)
				bma.length = XFS_MAX_BMBT_EXTLEN;
			else
				bma.length = len;

			ASSERT(len > 0);
			ASSERT(bma.length > 0);
			error = xfs_bmapi_allocate(&bma);
			if (error)
				goto error0;
			if (bma.blkno == NULLFSBLOCK)
				break;

			 
			if (whichfork == XFS_COW_FORK)
				xfs_refcount_alloc_cow_extent(tp, bma.blkno,
						bma.length);
		}

		 
		xfs_bmapi_trim_map(mval, &bma.got, &bno, len, obno,
							end, n, flags);

		 
		error = xfs_bmapi_convert_unwritten(&bma, mval, len, flags);
		if (error == -EAGAIN)
			continue;
		if (error)
			goto error0;

		 
		xfs_bmapi_update_map(&mval, &bno, &len, obno, end, &n, flags);

		 
		if (bno >= end || n >= *nmap || bma.nallocs >= *nmap)
			break;

		 
		bma.prev = bma.got;
		if (!xfs_iext_next_extent(ifp, &bma.icur, &bma.got))
			eof = true;
	}
	*nmap = n;

	error = xfs_bmap_btree_to_extents(tp, ip, bma.cur, &bma.logflags,
			whichfork);
	if (error)
		goto error0;

	ASSERT(ifp->if_format != XFS_DINODE_FMT_BTREE ||
	       ifp->if_nextents > XFS_IFORK_MAXEXT(ip, whichfork));
	xfs_bmapi_finish(&bma, whichfork, 0);
	xfs_bmap_validate_ret(orig_bno, orig_len, orig_flags, orig_mval,
		orig_nmap, *nmap);
	return 0;
error0:
	xfs_bmapi_finish(&bma, whichfork, error);
	return error;
}

 
int
xfs_bmapi_convert_delalloc(
	struct xfs_inode	*ip,
	int			whichfork,
	xfs_off_t		offset,
	struct iomap		*iomap,
	unsigned int		*seq)
{
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fileoff_t		offset_fsb = XFS_B_TO_FSBT(mp, offset);
	struct xfs_bmalloca	bma = { NULL };
	uint16_t		flags = 0;
	struct xfs_trans	*tp;
	int			error;

	if (whichfork == XFS_COW_FORK)
		flags |= IOMAP_F_SHARED;

	 
	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_write, 0, 0,
				XFS_TRANS_RESERVE, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	error = xfs_iext_count_may_overflow(ip, whichfork,
			XFS_IEXT_ADD_NOSPLIT_CNT);
	if (error == -EFBIG)
		error = xfs_iext_count_upgrade(tp, ip,
				XFS_IEXT_ADD_NOSPLIT_CNT);
	if (error)
		goto out_trans_cancel;

	if (!xfs_iext_lookup_extent(ip, ifp, offset_fsb, &bma.icur, &bma.got) ||
	    bma.got.br_startoff > offset_fsb) {
		 
		WARN_ON_ONCE(whichfork != XFS_COW_FORK);
		error = -EAGAIN;
		goto out_trans_cancel;
	}

	 
	if (!isnullstartblock(bma.got.br_startblock)) {
		xfs_bmbt_to_iomap(ip, iomap, &bma.got, 0, flags,
				xfs_iomap_inode_sequence(ip, flags));
		*seq = READ_ONCE(ifp->if_seq);
		goto out_trans_cancel;
	}

	bma.tp = tp;
	bma.ip = ip;
	bma.wasdel = true;
	bma.offset = bma.got.br_startoff;
	bma.length = max_t(xfs_filblks_t, bma.got.br_blockcount,
			XFS_MAX_BMBT_EXTLEN);
	bma.minleft = xfs_bmapi_minleft(tp, ip, whichfork);

	 
	bma.flags = XFS_BMAPI_PREALLOC;
	if (whichfork == XFS_COW_FORK)
		bma.flags |= XFS_BMAPI_COWFORK;

	if (!xfs_iext_peek_prev_extent(ifp, &bma.icur, &bma.prev))
		bma.prev.br_startoff = NULLFILEOFF;

	error = xfs_bmapi_allocate(&bma);
	if (error)
		goto out_finish;

	error = -ENOSPC;
	if (WARN_ON_ONCE(bma.blkno == NULLFSBLOCK))
		goto out_finish;
	error = -EFSCORRUPTED;
	if (WARN_ON_ONCE(!xfs_valid_startblock(ip, bma.got.br_startblock)))
		goto out_finish;

	XFS_STATS_ADD(mp, xs_xstrat_bytes, XFS_FSB_TO_B(mp, bma.length));
	XFS_STATS_INC(mp, xs_xstrat_quick);

	ASSERT(!isnullstartblock(bma.got.br_startblock));
	xfs_bmbt_to_iomap(ip, iomap, &bma.got, 0, flags,
				xfs_iomap_inode_sequence(ip, flags));
	*seq = READ_ONCE(ifp->if_seq);

	if (whichfork == XFS_COW_FORK)
		xfs_refcount_alloc_cow_extent(tp, bma.blkno, bma.length);

	error = xfs_bmap_btree_to_extents(tp, ip, bma.cur, &bma.logflags,
			whichfork);
	if (error)
		goto out_finish;

	xfs_bmapi_finish(&bma, whichfork, 0);
	error = xfs_trans_commit(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;

out_finish:
	xfs_bmapi_finish(&bma, whichfork, error);
out_trans_cancel:
	xfs_trans_cancel(tp);
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

int
xfs_bmapi_remap(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	xfs_fsblock_t		startblock,
	uint32_t		flags)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp;
	struct xfs_btree_cur	*cur = NULL;
	struct xfs_bmbt_irec	got;
	struct xfs_iext_cursor	icur;
	int			whichfork = xfs_bmapi_whichfork(flags);
	int			logflags = 0, error;

	ifp = xfs_ifork_ptr(ip, whichfork);
	ASSERT(len > 0);
	ASSERT(len <= (xfs_filblks_t)XFS_MAX_BMBT_EXTLEN);
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(!(flags & ~(XFS_BMAPI_ATTRFORK | XFS_BMAPI_PREALLOC |
			   XFS_BMAPI_NORMAP)));
	ASSERT((flags & (XFS_BMAPI_ATTRFORK | XFS_BMAPI_PREALLOC)) !=
			(XFS_BMAPI_ATTRFORK | XFS_BMAPI_PREALLOC));

	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BMAPIFORMAT)) {
		return -EFSCORRUPTED;
	}

	if (xfs_is_shutdown(mp))
		return -EIO;

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	if (xfs_iext_lookup_extent(ip, ifp, bno, &icur, &got)) {
		 
		ASSERT(got.br_startoff > bno);
		ASSERT(got.br_startoff - bno >= len);
	}

	ip->i_nblocks += len;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	if (ifp->if_format == XFS_DINODE_FMT_BTREE) {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		cur->bc_ino.flags = 0;
	}

	got.br_startoff = bno;
	got.br_startblock = startblock;
	got.br_blockcount = len;
	if (flags & XFS_BMAPI_PREALLOC)
		got.br_state = XFS_EXT_UNWRITTEN;
	else
		got.br_state = XFS_EXT_NORM;

	error = xfs_bmap_add_extent_hole_real(tp, ip, whichfork, &icur,
			&cur, &got, &logflags, flags);
	if (error)
		goto error0;

	error = xfs_bmap_btree_to_extents(tp, ip, cur, &logflags, whichfork);

error0:
	if (ip->i_df.if_format != XFS_DINODE_FMT_EXTENTS)
		logflags &= ~XFS_ILOG_DEXT;
	else if (ip->i_df.if_format != XFS_DINODE_FMT_BTREE)
		logflags &= ~XFS_ILOG_DBROOT;

	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	if (cur)
		xfs_btree_del_cursor(cur, error);
	return error;
}

 
static xfs_filblks_t
xfs_bmap_split_indlen(
	xfs_filblks_t			ores,		 
	xfs_filblks_t			*indlen1,	 
	xfs_filblks_t			*indlen2,	 
	xfs_filblks_t			avail)		 
{
	xfs_filblks_t			len1 = *indlen1;
	xfs_filblks_t			len2 = *indlen2;
	xfs_filblks_t			nres = len1 + len2;  
	xfs_filblks_t			stolen = 0;
	xfs_filblks_t			resfactor;

	 
	if (ores < nres && avail)
		stolen = XFS_FILBLKS_MIN(nres - ores, avail);
	ores += stolen;

	  
	if (ores >= nres)
		return stolen;

	 
	resfactor = (ores * 100);
	do_div(resfactor, nres);
	len1 *= resfactor;
	do_div(len1, 100);
	len2 *= resfactor;
	do_div(len2, 100);
	ASSERT(len1 + len2 <= ores);
	ASSERT(len1 < *indlen1 && len2 < *indlen2);

	 
	ores -= (len1 + len2);
	ASSERT((*indlen1 - len1) + (*indlen2 - len2) >= ores);
	if (ores && !len2 && *indlen2) {
		len2++;
		ores--;
	}
	while (ores) {
		if (len1 < *indlen1) {
			len1++;
			ores--;
		}
		if (!ores)
			break;
		if (len2 < *indlen2) {
			len2++;
			ores--;
		}
	}

	*indlen1 = len1;
	*indlen2 = len2;

	return stolen;
}

int
xfs_bmap_del_extent_delay(
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_iext_cursor	*icur,
	struct xfs_bmbt_irec	*got,
	struct xfs_bmbt_irec	*del)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec	new;
	int64_t			da_old, da_new, da_diff = 0;
	xfs_fileoff_t		del_endoff, got_endoff;
	xfs_filblks_t		got_indlen, new_indlen, stolen;
	uint32_t		state = xfs_bmap_fork_to_state(whichfork);
	int			error = 0;
	bool			isrt;

	XFS_STATS_INC(mp, xs_del_exlist);

	isrt = (whichfork == XFS_DATA_FORK) && XFS_IS_REALTIME_INODE(ip);
	del_endoff = del->br_startoff + del->br_blockcount;
	got_endoff = got->br_startoff + got->br_blockcount;
	da_old = startblockval(got->br_startblock);
	da_new = 0;

	ASSERT(del->br_blockcount > 0);
	ASSERT(got->br_startoff <= del->br_startoff);
	ASSERT(got_endoff >= del_endoff);

	if (isrt) {
		uint64_t rtexts = XFS_FSB_TO_B(mp, del->br_blockcount);

		do_div(rtexts, mp->m_sb.sb_rextsize);
		xfs_mod_frextents(mp, rtexts);
	}

	 
	ASSERT(!isrt);
	error = xfs_quota_unreserve_blkres(ip, del->br_blockcount);
	if (error)
		return error;
	ip->i_delayed_blks -= del->br_blockcount;

	if (got->br_startoff == del->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (got_endoff == del_endoff)
		state |= BMAP_RIGHT_FILLING;

	switch (state & (BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING)) {
	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		 
		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		break;
	case BMAP_LEFT_FILLING:
		 
		got->br_startoff = del_endoff;
		got->br_blockcount -= del->br_blockcount;
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip,
				got->br_blockcount), da_old);
		got->br_startblock = nullstartblock((int)da_new);
		xfs_iext_update_extent(ip, state, icur, got);
		break;
	case BMAP_RIGHT_FILLING:
		 
		got->br_blockcount = got->br_blockcount - del->br_blockcount;
		da_new = XFS_FILBLKS_MIN(xfs_bmap_worst_indlen(ip,
				got->br_blockcount), da_old);
		got->br_startblock = nullstartblock((int)da_new);
		xfs_iext_update_extent(ip, state, icur, got);
		break;
	case 0:
		 
		got->br_blockcount = del->br_startoff - got->br_startoff;
		got_indlen = xfs_bmap_worst_indlen(ip, got->br_blockcount);

		new.br_blockcount = got_endoff - del_endoff;
		new_indlen = xfs_bmap_worst_indlen(ip, new.br_blockcount);

		WARN_ON_ONCE(!got_indlen || !new_indlen);
		stolen = xfs_bmap_split_indlen(da_old, &got_indlen, &new_indlen,
						       del->br_blockcount);

		got->br_startblock = nullstartblock((int)got_indlen);

		new.br_startoff = del_endoff;
		new.br_state = got->br_state;
		new.br_startblock = nullstartblock((int)new_indlen);

		xfs_iext_update_extent(ip, state, icur, got);
		xfs_iext_next(ifp, icur);
		xfs_iext_insert(ip, icur, &new, state);

		da_new = got_indlen + new_indlen - stolen;
		del->br_blockcount -= stolen;
		break;
	}

	ASSERT(da_old >= da_new);
	da_diff = da_old - da_new;
	if (!isrt)
		da_diff += del->br_blockcount;
	if (da_diff) {
		xfs_mod_fdblocks(mp, da_diff, false);
		xfs_mod_delalloc(mp, -da_diff);
	}
	return error;
}

void
xfs_bmap_del_extent_cow(
	struct xfs_inode	*ip,
	struct xfs_iext_cursor	*icur,
	struct xfs_bmbt_irec	*got,
	struct xfs_bmbt_irec	*del)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, XFS_COW_FORK);
	struct xfs_bmbt_irec	new;
	xfs_fileoff_t		del_endoff, got_endoff;
	uint32_t		state = BMAP_COWFORK;

	XFS_STATS_INC(mp, xs_del_exlist);

	del_endoff = del->br_startoff + del->br_blockcount;
	got_endoff = got->br_startoff + got->br_blockcount;

	ASSERT(del->br_blockcount > 0);
	ASSERT(got->br_startoff <= del->br_startoff);
	ASSERT(got_endoff >= del_endoff);
	ASSERT(!isnullstartblock(got->br_startblock));

	if (got->br_startoff == del->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (got_endoff == del_endoff)
		state |= BMAP_RIGHT_FILLING;

	switch (state & (BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING)) {
	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		 
		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		break;
	case BMAP_LEFT_FILLING:
		 
		got->br_startoff = del_endoff;
		got->br_blockcount -= del->br_blockcount;
		got->br_startblock = del->br_startblock + del->br_blockcount;
		xfs_iext_update_extent(ip, state, icur, got);
		break;
	case BMAP_RIGHT_FILLING:
		 
		got->br_blockcount -= del->br_blockcount;
		xfs_iext_update_extent(ip, state, icur, got);
		break;
	case 0:
		 
		got->br_blockcount = del->br_startoff - got->br_startoff;

		new.br_startoff = del_endoff;
		new.br_blockcount = got_endoff - del_endoff;
		new.br_state = got->br_state;
		new.br_startblock = del->br_startblock + del->br_blockcount;

		xfs_iext_update_extent(ip, state, icur, got);
		xfs_iext_next(ifp, icur);
		xfs_iext_insert(ip, icur, &new, state);
		break;
	}
	ip->i_delayed_blks -= del->br_blockcount;
}

 
STATIC int				 
xfs_bmap_del_extent_real(
	xfs_inode_t		*ip,	 
	xfs_trans_t		*tp,	 
	struct xfs_iext_cursor	*icur,
	struct xfs_btree_cur	*cur,	 
	xfs_bmbt_irec_t		*del,	 
	int			*logflagsp,  
	int			whichfork,  
	uint32_t		bflags)	 
{
	xfs_fsblock_t		del_endblock=0;	 
	xfs_fileoff_t		del_endoff;	 
	int			do_fx;	 
	int			error;	 
	int			flags = 0; 
	struct xfs_bmbt_irec	got;	 
	xfs_fileoff_t		got_endoff;	 
	int			i;	 
	struct xfs_ifork	*ifp;	 
	xfs_mount_t		*mp;	 
	xfs_filblks_t		nblks;	 
	xfs_bmbt_irec_t		new;	 
	 
	uint			qfield;	 
	uint32_t		state = xfs_bmap_fork_to_state(whichfork);
	struct xfs_bmbt_irec	old;

	mp = ip->i_mount;
	XFS_STATS_INC(mp, xs_del_exlist);

	ifp = xfs_ifork_ptr(ip, whichfork);
	ASSERT(del->br_blockcount > 0);
	xfs_iext_get_extent(ifp, icur, &got);
	ASSERT(got.br_startoff <= del->br_startoff);
	del_endoff = del->br_startoff + del->br_blockcount;
	got_endoff = got.br_startoff + got.br_blockcount;
	ASSERT(got_endoff >= del_endoff);
	ASSERT(!isnullstartblock(got.br_startblock));
	qfield = 0;
	error = 0;

	 
	if (tp->t_blk_res == 0 &&
	    ifp->if_format == XFS_DINODE_FMT_EXTENTS &&
	    ifp->if_nextents >= XFS_IFORK_MAXEXT(ip, whichfork) &&
	    del->br_startoff > got.br_startoff && del_endoff < got_endoff)
		return -ENOSPC;

	flags = XFS_ILOG_CORE;
	if (whichfork == XFS_DATA_FORK && XFS_IS_REALTIME_INODE(ip)) {
		xfs_filblks_t	len;
		xfs_extlen_t	mod;

		len = div_u64_rem(del->br_blockcount, mp->m_sb.sb_rextsize,
				  &mod);
		ASSERT(mod == 0);

		if (!(bflags & XFS_BMAPI_REMAP)) {
			xfs_fsblock_t	bno;

			bno = div_u64_rem(del->br_startblock,
					mp->m_sb.sb_rextsize, &mod);
			ASSERT(mod == 0);

			error = xfs_rtfree_extent(tp, bno, (xfs_extlen_t)len);
			if (error)
				goto done;
		}

		do_fx = 0;
		nblks = len * mp->m_sb.sb_rextsize;
		qfield = XFS_TRANS_DQ_RTBCOUNT;
	} else {
		do_fx = 1;
		nblks = del->br_blockcount;
		qfield = XFS_TRANS_DQ_BCOUNT;
	}

	del_endblock = del->br_startblock + del->br_blockcount;
	if (cur) {
		error = xfs_bmbt_lookup_eq(cur, &got, &i);
		if (error)
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
	}

	if (got.br_startoff == del->br_startoff)
		state |= BMAP_LEFT_FILLING;
	if (got_endoff == del_endoff)
		state |= BMAP_RIGHT_FILLING;

	switch (state & (BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING)) {
	case BMAP_LEFT_FILLING | BMAP_RIGHT_FILLING:
		 
		xfs_iext_remove(ip, icur, state);
		xfs_iext_prev(ifp, icur);
		ifp->if_nextents--;

		flags |= XFS_ILOG_CORE;
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		if ((error = xfs_btree_delete(cur, &i)))
			goto done;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto done;
		}
		break;
	case BMAP_LEFT_FILLING:
		 
		got.br_startoff = del_endoff;
		got.br_startblock = del_endblock;
		got.br_blockcount -= del->br_blockcount;
		xfs_iext_update_extent(ip, state, icur, &got);
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		error = xfs_bmbt_update(cur, &got);
		if (error)
			goto done;
		break;
	case BMAP_RIGHT_FILLING:
		 
		got.br_blockcount -= del->br_blockcount;
		xfs_iext_update_extent(ip, state, icur, &got);
		if (!cur) {
			flags |= xfs_ilog_fext(whichfork);
			break;
		}
		error = xfs_bmbt_update(cur, &got);
		if (error)
			goto done;
		break;
	case 0:
		 

		old = got;

		got.br_blockcount = del->br_startoff - got.br_startoff;
		xfs_iext_update_extent(ip, state, icur, &got);

		new.br_startoff = del_endoff;
		new.br_blockcount = got_endoff - del_endoff;
		new.br_state = got.br_state;
		new.br_startblock = del_endblock;

		flags |= XFS_ILOG_CORE;
		if (cur) {
			error = xfs_bmbt_update(cur, &got);
			if (error)
				goto done;
			error = xfs_btree_increment(cur, 0, &i);
			if (error)
				goto done;
			cur->bc_rec.b = new;
			error = xfs_btree_insert(cur, &i);
			if (error && error != -ENOSPC)
				goto done;
			 
			if (error == -ENOSPC) {
				 
				error = xfs_bmbt_lookup_eq(cur, &got, &i);
				if (error)
					goto done;
				if (XFS_IS_CORRUPT(mp, i != 1)) {
					error = -EFSCORRUPTED;
					goto done;
				}
				 
				error = xfs_bmbt_update(cur, &old);
				if (error)
					goto done;
				 
				xfs_iext_update_extent(ip, state, icur, &old);
				flags = 0;
				error = -ENOSPC;
				goto done;
			}
			if (XFS_IS_CORRUPT(mp, i != 1)) {
				error = -EFSCORRUPTED;
				goto done;
			}
		} else
			flags |= xfs_ilog_fext(whichfork);

		ifp->if_nextents++;
		xfs_iext_next(ifp, icur);
		xfs_iext_insert(ip, icur, &new, state);
		break;
	}

	 
	xfs_rmap_unmap_extent(tp, ip, whichfork, del);

	 
	if (do_fx && !(bflags & XFS_BMAPI_REMAP)) {
		if (xfs_is_reflink_inode(ip) && whichfork == XFS_DATA_FORK) {
			xfs_refcount_decrease_extent(tp, del);
		} else {
			error = __xfs_free_extent_later(tp, del->br_startblock,
					del->br_blockcount, NULL,
					XFS_AG_RESV_NONE,
					((bflags & XFS_BMAPI_NODISCARD) ||
					del->br_state == XFS_EXT_UNWRITTEN));
			if (error)
				goto done;
		}
	}

	 
	if (nblks)
		ip->i_nblocks -= nblks;
	 
	if (qfield && !(bflags & XFS_BMAPI_REMAP))
		xfs_trans_mod_dquot_byino(tp, ip, qfield, (long)-nblks);

done:
	*logflagsp = flags;
	return error;
}

 
int						 
__xfs_bunmapi(
	struct xfs_trans	*tp,		 
	struct xfs_inode	*ip,		 
	xfs_fileoff_t		start,		 
	xfs_filblks_t		*rlen,		 
	uint32_t		flags,		 
	xfs_extnum_t		nexts)		 
{
	struct xfs_btree_cur	*cur;		 
	struct xfs_bmbt_irec	del;		 
	int			error;		 
	xfs_extnum_t		extno;		 
	struct xfs_bmbt_irec	got;		 
	struct xfs_ifork	*ifp;		 
	int			isrt;		 
	int			logflags;	 
	xfs_extlen_t		mod;		 
	struct xfs_mount	*mp = ip->i_mount;
	int			tmp_logflags;	 
	int			wasdel;		 
	int			whichfork;	 
	xfs_fsblock_t		sum;
	xfs_filblks_t		len = *rlen;	 
	xfs_fileoff_t		end;
	struct xfs_iext_cursor	icur;
	bool			done = false;

	trace_xfs_bunmap(ip, start, len, flags, _RET_IP_);

	whichfork = xfs_bmapi_whichfork(flags);
	ASSERT(whichfork != XFS_COW_FORK);
	ifp = xfs_ifork_ptr(ip, whichfork);
	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)))
		return -EFSCORRUPTED;
	if (xfs_is_shutdown(mp))
		return -EIO;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(len > 0);
	ASSERT(nexts >= 0);

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	if (xfs_iext_count(ifp) == 0) {
		*rlen = 0;
		return 0;
	}
	XFS_STATS_INC(mp, xs_blk_unmap);
	isrt = (whichfork == XFS_DATA_FORK) && XFS_IS_REALTIME_INODE(ip);
	end = start + len;

	if (!xfs_iext_lookup_extent_before(ip, ifp, &end, &icur, &got)) {
		*rlen = 0;
		return 0;
	}
	end--;

	logflags = 0;
	if (ifp->if_format == XFS_DINODE_FMT_BTREE) {
		ASSERT(ifp->if_format == XFS_DINODE_FMT_BTREE);
		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		cur->bc_ino.flags = 0;
	} else
		cur = NULL;

	if (isrt) {
		 
		xfs_ilock(mp->m_rbmip, XFS_ILOCK_EXCL|XFS_ILOCK_RTBITMAP);
		xfs_trans_ijoin(tp, mp->m_rbmip, XFS_ILOCK_EXCL);
		xfs_ilock(mp->m_rsumip, XFS_ILOCK_EXCL|XFS_ILOCK_RTSUM);
		xfs_trans_ijoin(tp, mp->m_rsumip, XFS_ILOCK_EXCL);
	}

	extno = 0;
	while (end != (xfs_fileoff_t)-1 && end >= start &&
	       (nexts == 0 || extno < nexts)) {
		 
		if (got.br_startoff > end &&
		    !xfs_iext_prev_extent(ifp, &icur, &got)) {
			done = true;
			break;
		}
		 
		end = XFS_FILEOFF_MIN(end,
			got.br_startoff + got.br_blockcount - 1);
		if (end < start)
			break;
		 
		del = got;
		wasdel = isnullstartblock(del.br_startblock);

		if (got.br_startoff < start) {
			del.br_startoff = start;
			del.br_blockcount -= start - got.br_startoff;
			if (!wasdel)
				del.br_startblock += start - got.br_startoff;
		}
		if (del.br_startoff + del.br_blockcount > end + 1)
			del.br_blockcount = end + 1 - del.br_startoff;

		if (!isrt)
			goto delete;

		sum = del.br_startblock + del.br_blockcount;
		div_u64_rem(sum, mp->m_sb.sb_rextsize, &mod);
		if (mod) {
			 
			if (del.br_state == XFS_EXT_UNWRITTEN) {
				 
				ASSERT(end >= mod);
				end -= mod > del.br_blockcount ?
					del.br_blockcount : mod;
				if (end < got.br_startoff &&
				    !xfs_iext_prev_extent(ifp, &icur, &got)) {
					done = true;
					break;
				}
				continue;
			}
			 
			ASSERT(del.br_state == XFS_EXT_NORM);
			ASSERT(tp->t_blk_res > 0);
			 
			if (del.br_blockcount > mod) {
				del.br_startoff += del.br_blockcount - mod;
				del.br_startblock += del.br_blockcount - mod;
				del.br_blockcount = mod;
			}
			del.br_state = XFS_EXT_UNWRITTEN;
			error = xfs_bmap_add_extent_unwritten_real(tp, ip,
					whichfork, &icur, &cur, &del,
					&logflags);
			if (error)
				goto error0;
			goto nodelete;
		}
		div_u64_rem(del.br_startblock, mp->m_sb.sb_rextsize, &mod);
		if (mod) {
			xfs_extlen_t off = mp->m_sb.sb_rextsize - mod;

			 
			if (del.br_blockcount > off) {
				del.br_blockcount -= off;
				del.br_startoff += off;
				del.br_startblock += off;
			} else if (del.br_startoff == start &&
				   (del.br_state == XFS_EXT_UNWRITTEN ||
				    tp->t_blk_res == 0)) {
				 
				ASSERT(end >= del.br_blockcount);
				end -= del.br_blockcount;
				if (got.br_startoff > end &&
				    !xfs_iext_prev_extent(ifp, &icur, &got)) {
					done = true;
					break;
				}
				continue;
			} else if (del.br_state == XFS_EXT_UNWRITTEN) {
				struct xfs_bmbt_irec	prev;
				xfs_fileoff_t		unwrite_start;

				 
				if (!xfs_iext_prev_extent(ifp, &icur, &prev))
					ASSERT(0);
				ASSERT(prev.br_state == XFS_EXT_NORM);
				ASSERT(!isnullstartblock(prev.br_startblock));
				ASSERT(del.br_startblock ==
				       prev.br_startblock + prev.br_blockcount);
				unwrite_start = max3(start,
						     del.br_startoff - mod,
						     prev.br_startoff);
				mod = unwrite_start - prev.br_startoff;
				prev.br_startoff = unwrite_start;
				prev.br_startblock += mod;
				prev.br_blockcount -= mod;
				prev.br_state = XFS_EXT_UNWRITTEN;
				error = xfs_bmap_add_extent_unwritten_real(tp,
						ip, whichfork, &icur, &cur,
						&prev, &logflags);
				if (error)
					goto error0;
				goto nodelete;
			} else {
				ASSERT(del.br_state == XFS_EXT_NORM);
				del.br_state = XFS_EXT_UNWRITTEN;
				error = xfs_bmap_add_extent_unwritten_real(tp,
						ip, whichfork, &icur, &cur,
						&del, &logflags);
				if (error)
					goto error0;
				goto nodelete;
			}
		}

delete:
		if (wasdel) {
			error = xfs_bmap_del_extent_delay(ip, whichfork, &icur,
					&got, &del);
		} else {
			error = xfs_bmap_del_extent_real(ip, tp, &icur, cur,
					&del, &tmp_logflags, whichfork,
					flags);
			logflags |= tmp_logflags;
		}

		if (error)
			goto error0;

		end = del.br_startoff - 1;
nodelete:
		 
		if (end != (xfs_fileoff_t)-1 && end >= start) {
			if (!xfs_iext_get_extent(ifp, &icur, &got) ||
			    (got.br_startoff > end &&
			     !xfs_iext_prev_extent(ifp, &icur, &got))) {
				done = true;
				break;
			}
			extno++;
		}
	}
	if (done || end == (xfs_fileoff_t)-1 || end < start)
		*rlen = 0;
	else
		*rlen = end - start + 1;

	 
	if (xfs_bmap_needs_btree(ip, whichfork)) {
		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(tp, ip, &cur, 0,
				&tmp_logflags, whichfork);
		logflags |= tmp_logflags;
	} else {
		error = xfs_bmap_btree_to_extents(tp, ip, cur, &logflags,
			whichfork);
	}

error0:
	 
	if ((logflags & xfs_ilog_fext(whichfork)) &&
	    ifp->if_format != XFS_DINODE_FMT_EXTENTS)
		logflags &= ~xfs_ilog_fext(whichfork);
	else if ((logflags & xfs_ilog_fbroot(whichfork)) &&
		 ifp->if_format != XFS_DINODE_FMT_BTREE)
		logflags &= ~xfs_ilog_fbroot(whichfork);
	 
	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	if (cur) {
		if (!error)
			cur->bc_ino.allocated = 0;
		xfs_btree_del_cursor(cur, error);
	}
	return error;
}

 
int
xfs_bunmapi(
	xfs_trans_t		*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		bno,
	xfs_filblks_t		len,
	uint32_t		flags,
	xfs_extnum_t		nexts,
	int			*done)
{
	int			error;

	error = __xfs_bunmapi(tp, ip, bno, &len, flags, nexts);
	*done = (len == 0);
	return error;
}

 
STATIC bool
xfs_bmse_can_merge(
	struct xfs_bmbt_irec	*left,	 
	struct xfs_bmbt_irec	*got,	 
	xfs_fileoff_t		shift)	 
{
	xfs_fileoff_t		startoff;

	startoff = got->br_startoff - shift;

	 
	if ((left->br_startoff + left->br_blockcount != startoff) ||
	    (left->br_startblock + left->br_blockcount != got->br_startblock) ||
	    (left->br_state != got->br_state) ||
	    (left->br_blockcount + got->br_blockcount > XFS_MAX_BMBT_EXTLEN))
		return false;

	return true;
}

 
STATIC int
xfs_bmse_merge(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	int				whichfork,
	xfs_fileoff_t			shift,		 
	struct xfs_iext_cursor		*icur,
	struct xfs_bmbt_irec		*got,		 
	struct xfs_bmbt_irec		*left,		 
	struct xfs_btree_cur		*cur,
	int				*logflags)	 
{
	struct xfs_ifork		*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_bmbt_irec		new;
	xfs_filblks_t			blockcount;
	int				error, i;
	struct xfs_mount		*mp = ip->i_mount;

	blockcount = left->br_blockcount + got->br_blockcount;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL));
	ASSERT(xfs_isilocked(ip, XFS_ILOCK_EXCL));
	ASSERT(xfs_bmse_can_merge(left, got, shift));

	new = *left;
	new.br_blockcount = blockcount;

	 
	ifp->if_nextents--;
	*logflags |= XFS_ILOG_CORE;
	if (!cur) {
		*logflags |= XFS_ILOG_DEXT;
		goto done;
	}

	 
	error = xfs_bmbt_lookup_eq(cur, got, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(mp, i != 1))
		return -EFSCORRUPTED;

	error = xfs_btree_delete(cur, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(mp, i != 1))
		return -EFSCORRUPTED;

	 
	error = xfs_bmbt_lookup_eq(cur, left, &i);
	if (error)
		return error;
	if (XFS_IS_CORRUPT(mp, i != 1))
		return -EFSCORRUPTED;

	error = xfs_bmbt_update(cur, &new);
	if (error)
		return error;

	 
	error = xfs_bmap_btree_to_extents(tp, ip, cur, logflags, whichfork);
	if (error)
		return error;

done:
	xfs_iext_remove(ip, icur, 0);
	xfs_iext_prev(ifp, icur);
	xfs_iext_update_extent(ip, xfs_bmap_fork_to_state(whichfork), icur,
			&new);

	 
	xfs_rmap_unmap_extent(tp, ip, whichfork, got);
	memcpy(&new, got, sizeof(new));
	new.br_startoff = left->br_startoff + left->br_blockcount;
	xfs_rmap_map_extent(tp, ip, whichfork, &new);
	return 0;
}

static int
xfs_bmap_shift_update_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_iext_cursor	*icur,
	struct xfs_bmbt_irec	*got,
	struct xfs_btree_cur	*cur,
	int			*logflags,
	xfs_fileoff_t		startoff)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_bmbt_irec	prev = *got;
	int			error, i;

	*logflags |= XFS_ILOG_CORE;

	got->br_startoff = startoff;

	if (cur) {
		error = xfs_bmbt_lookup_eq(cur, &prev, &i);
		if (error)
			return error;
		if (XFS_IS_CORRUPT(mp, i != 1))
			return -EFSCORRUPTED;

		error = xfs_bmbt_update(cur, got);
		if (error)
			return error;
	} else {
		*logflags |= XFS_ILOG_DEXT;
	}

	xfs_iext_update_extent(ip, xfs_bmap_fork_to_state(whichfork), icur,
			got);

	 
	xfs_rmap_unmap_extent(tp, ip, whichfork, &prev);
	xfs_rmap_map_extent(tp, ip, whichfork, got);
	return 0;
}

int
xfs_bmap_collapse_extents(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		*next_fsb,
	xfs_fileoff_t		offset_shift_fsb,
	bool			*done)
{
	int			whichfork = XFS_DATA_FORK;
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_cur	*cur = NULL;
	struct xfs_bmbt_irec	got, prev;
	struct xfs_iext_cursor	icur;
	xfs_fileoff_t		new_startoff;
	int			error = 0;
	int			logflags = 0;

	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BMAPIFORMAT)) {
		return -EFSCORRUPTED;
	}

	if (xfs_is_shutdown(mp))
		return -EIO;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL));

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	if (ifp->if_format == XFS_DINODE_FMT_BTREE) {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		cur->bc_ino.flags = 0;
	}

	if (!xfs_iext_lookup_extent(ip, ifp, *next_fsb, &icur, &got)) {
		*done = true;
		goto del_cursor;
	}
	if (XFS_IS_CORRUPT(mp, isnullstartblock(got.br_startblock))) {
		error = -EFSCORRUPTED;
		goto del_cursor;
	}

	new_startoff = got.br_startoff - offset_shift_fsb;
	if (xfs_iext_peek_prev_extent(ifp, &icur, &prev)) {
		if (new_startoff < prev.br_startoff + prev.br_blockcount) {
			error = -EINVAL;
			goto del_cursor;
		}

		if (xfs_bmse_can_merge(&prev, &got, offset_shift_fsb)) {
			error = xfs_bmse_merge(tp, ip, whichfork,
					offset_shift_fsb, &icur, &got, &prev,
					cur, &logflags);
			if (error)
				goto del_cursor;
			goto done;
		}
	} else {
		if (got.br_startoff < offset_shift_fsb) {
			error = -EINVAL;
			goto del_cursor;
		}
	}

	error = xfs_bmap_shift_update_extent(tp, ip, whichfork, &icur, &got,
			cur, &logflags, new_startoff);
	if (error)
		goto del_cursor;

done:
	if (!xfs_iext_next_extent(ifp, &icur, &got)) {
		*done = true;
		goto del_cursor;
	}

	*next_fsb = got.br_startoff;
del_cursor:
	if (cur)
		xfs_btree_del_cursor(cur, error);
	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	return error;
}

 
int
xfs_bmap_can_insert_extents(
	struct xfs_inode	*ip,
	xfs_fileoff_t		off,
	xfs_fileoff_t		shift)
{
	struct xfs_bmbt_irec	got;
	int			is_empty;
	int			error = 0;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL));

	if (xfs_is_shutdown(ip->i_mount))
		return -EIO;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	error = xfs_bmap_last_extent(NULL, ip, XFS_DATA_FORK, &got, &is_empty);
	if (!error && !is_empty && got.br_startoff >= off &&
	    ((got.br_startoff + shift) & BMBT_STARTOFF_MASK) < got.br_startoff)
		error = -EINVAL;
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	return error;
}

int
xfs_bmap_insert_extents(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		*next_fsb,
	xfs_fileoff_t		offset_shift_fsb,
	bool			*done,
	xfs_fileoff_t		stop_fsb)
{
	int			whichfork = XFS_DATA_FORK;
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_ifork	*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_cur	*cur = NULL;
	struct xfs_bmbt_irec	got, next;
	struct xfs_iext_cursor	icur;
	xfs_fileoff_t		new_startoff;
	int			error = 0;
	int			logflags = 0;

	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BMAPIFORMAT)) {
		return -EFSCORRUPTED;
	}

	if (xfs_is_shutdown(mp))
		return -EIO;

	ASSERT(xfs_isilocked(ip, XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL));

	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	if (ifp->if_format == XFS_DINODE_FMT_BTREE) {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		cur->bc_ino.flags = 0;
	}

	if (*next_fsb == NULLFSBLOCK) {
		xfs_iext_last(ifp, &icur);
		if (!xfs_iext_get_extent(ifp, &icur, &got) ||
		    stop_fsb > got.br_startoff) {
			*done = true;
			goto del_cursor;
		}
	} else {
		if (!xfs_iext_lookup_extent(ip, ifp, *next_fsb, &icur, &got)) {
			*done = true;
			goto del_cursor;
		}
	}
	if (XFS_IS_CORRUPT(mp, isnullstartblock(got.br_startblock))) {
		error = -EFSCORRUPTED;
		goto del_cursor;
	}

	if (XFS_IS_CORRUPT(mp, stop_fsb > got.br_startoff)) {
		error = -EFSCORRUPTED;
		goto del_cursor;
	}

	new_startoff = got.br_startoff + offset_shift_fsb;
	if (xfs_iext_peek_next_extent(ifp, &icur, &next)) {
		if (new_startoff + got.br_blockcount > next.br_startoff) {
			error = -EINVAL;
			goto del_cursor;
		}

		 
		if (xfs_bmse_can_merge(&got, &next, offset_shift_fsb))
			WARN_ON_ONCE(1);
	}

	error = xfs_bmap_shift_update_extent(tp, ip, whichfork, &icur, &got,
			cur, &logflags, new_startoff);
	if (error)
		goto del_cursor;

	if (!xfs_iext_prev_extent(ifp, &icur, &got) ||
	    stop_fsb >= got.br_startoff + got.br_blockcount) {
		*done = true;
		goto del_cursor;
	}

	*next_fsb = got.br_startoff;
del_cursor:
	if (cur)
		xfs_btree_del_cursor(cur, error);
	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	return error;
}

 
int
xfs_bmap_split_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xfs_fileoff_t		split_fsb)
{
	int				whichfork = XFS_DATA_FORK;
	struct xfs_ifork		*ifp = xfs_ifork_ptr(ip, whichfork);
	struct xfs_btree_cur		*cur = NULL;
	struct xfs_bmbt_irec		got;
	struct xfs_bmbt_irec		new;  
	struct xfs_mount		*mp = ip->i_mount;
	xfs_fsblock_t			gotblkcnt;  
	struct xfs_iext_cursor		icur;
	int				error = 0;
	int				logflags = 0;
	int				i = 0;

	if (XFS_IS_CORRUPT(mp, !xfs_ifork_has_extents(ifp)) ||
	    XFS_TEST_ERROR(false, mp, XFS_ERRTAG_BMAPIFORMAT)) {
		return -EFSCORRUPTED;
	}

	if (xfs_is_shutdown(mp))
		return -EIO;

	 
	error = xfs_iread_extents(tp, ip, whichfork);
	if (error)
		return error;

	 
	if (!xfs_iext_lookup_extent(ip, ifp, split_fsb, &icur, &got) ||
	    got.br_startoff >= split_fsb)
		return 0;

	gotblkcnt = split_fsb - got.br_startoff;
	new.br_startoff = split_fsb;
	new.br_startblock = got.br_startblock + gotblkcnt;
	new.br_blockcount = got.br_blockcount - gotblkcnt;
	new.br_state = got.br_state;

	if (ifp->if_format == XFS_DINODE_FMT_BTREE) {
		cur = xfs_bmbt_init_cursor(mp, tp, ip, whichfork);
		cur->bc_ino.flags = 0;
		error = xfs_bmbt_lookup_eq(cur, &got, &i);
		if (error)
			goto del_cursor;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto del_cursor;
		}
	}

	got.br_blockcount = gotblkcnt;
	xfs_iext_update_extent(ip, xfs_bmap_fork_to_state(whichfork), &icur,
			&got);

	logflags = XFS_ILOG_CORE;
	if (cur) {
		error = xfs_bmbt_update(cur, &got);
		if (error)
			goto del_cursor;
	} else
		logflags |= XFS_ILOG_DEXT;

	 
	xfs_iext_next(ifp, &icur);
	xfs_iext_insert(ip, &icur, &new, 0);
	ifp->if_nextents++;

	if (cur) {
		error = xfs_bmbt_lookup_eq(cur, &new, &i);
		if (error)
			goto del_cursor;
		if (XFS_IS_CORRUPT(mp, i != 0)) {
			error = -EFSCORRUPTED;
			goto del_cursor;
		}
		error = xfs_btree_insert(cur, &i);
		if (error)
			goto del_cursor;
		if (XFS_IS_CORRUPT(mp, i != 1)) {
			error = -EFSCORRUPTED;
			goto del_cursor;
		}
	}

	 
	if (xfs_bmap_needs_btree(ip, whichfork)) {
		int tmp_logflags;  

		ASSERT(cur == NULL);
		error = xfs_bmap_extents_to_btree(tp, ip, &cur, 0,
				&tmp_logflags, whichfork);
		logflags |= tmp_logflags;
	}

del_cursor:
	if (cur) {
		cur->bc_ino.allocated = 0;
		xfs_btree_del_cursor(cur, error);
	}

	if (logflags)
		xfs_trans_log_inode(tp, ip, logflags);
	return error;
}

 
static bool
xfs_bmap_is_update_needed(
	struct xfs_bmbt_irec	*bmap)
{
	return  bmap->br_startblock != HOLESTARTBLOCK &&
		bmap->br_startblock != DELAYSTARTBLOCK;
}

 
static int
__xfs_bmap_add(
	struct xfs_trans		*tp,
	enum xfs_bmap_intent_type	type,
	struct xfs_inode		*ip,
	int				whichfork,
	struct xfs_bmbt_irec		*bmap)
{
	struct xfs_bmap_intent		*bi;

	trace_xfs_bmap_defer(tp->t_mountp,
			XFS_FSB_TO_AGNO(tp->t_mountp, bmap->br_startblock),
			type,
			XFS_FSB_TO_AGBNO(tp->t_mountp, bmap->br_startblock),
			ip->i_ino, whichfork,
			bmap->br_startoff,
			bmap->br_blockcount,
			bmap->br_state);

	bi = kmem_cache_alloc(xfs_bmap_intent_cache, GFP_NOFS | __GFP_NOFAIL);
	INIT_LIST_HEAD(&bi->bi_list);
	bi->bi_type = type;
	bi->bi_owner = ip;
	bi->bi_whichfork = whichfork;
	bi->bi_bmap = *bmap;

	xfs_bmap_update_get_group(tp->t_mountp, bi);
	xfs_defer_add(tp, XFS_DEFER_OPS_TYPE_BMAP, &bi->bi_list);
	return 0;
}

 
void
xfs_bmap_map_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*PREV)
{
	if (!xfs_bmap_is_update_needed(PREV))
		return;

	__xfs_bmap_add(tp, XFS_BMAP_MAP, ip, XFS_DATA_FORK, PREV);
}

 
void
xfs_bmap_unmap_extent(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_bmbt_irec	*PREV)
{
	if (!xfs_bmap_is_update_needed(PREV))
		return;

	__xfs_bmap_add(tp, XFS_BMAP_UNMAP, ip, XFS_DATA_FORK, PREV);
}

 
int
xfs_bmap_finish_one(
	struct xfs_trans		*tp,
	struct xfs_bmap_intent		*bi)
{
	struct xfs_bmbt_irec		*bmap = &bi->bi_bmap;
	int				error = 0;

	ASSERT(tp->t_highest_agno == NULLAGNUMBER);

	trace_xfs_bmap_deferred(tp->t_mountp,
			XFS_FSB_TO_AGNO(tp->t_mountp, bmap->br_startblock),
			bi->bi_type,
			XFS_FSB_TO_AGBNO(tp->t_mountp, bmap->br_startblock),
			bi->bi_owner->i_ino, bi->bi_whichfork,
			bmap->br_startoff, bmap->br_blockcount,
			bmap->br_state);

	if (WARN_ON_ONCE(bi->bi_whichfork != XFS_DATA_FORK))
		return -EFSCORRUPTED;

	if (XFS_TEST_ERROR(false, tp->t_mountp,
			XFS_ERRTAG_BMAP_FINISH_ONE))
		return -EIO;

	switch (bi->bi_type) {
	case XFS_BMAP_MAP:
		error = xfs_bmapi_remap(tp, bi->bi_owner, bmap->br_startoff,
				bmap->br_blockcount, bmap->br_startblock, 0);
		bmap->br_blockcount = 0;
		break;
	case XFS_BMAP_UNMAP:
		error = __xfs_bunmapi(tp, bi->bi_owner, bmap->br_startoff,
				&bmap->br_blockcount, XFS_BMAPI_REMAP, 1);
		break;
	default:
		ASSERT(0);
		error = -EFSCORRUPTED;
	}

	return error;
}

 
xfs_failaddr_t
xfs_bmap_validate_extent(
	struct xfs_inode	*ip,
	int			whichfork,
	struct xfs_bmbt_irec	*irec)
{
	struct xfs_mount	*mp = ip->i_mount;

	if (!xfs_verify_fileext(mp, irec->br_startoff, irec->br_blockcount))
		return __this_address;

	if (XFS_IS_REALTIME_INODE(ip) && whichfork == XFS_DATA_FORK) {
		if (!xfs_verify_rtext(mp, irec->br_startblock,
					  irec->br_blockcount))
			return __this_address;
	} else {
		if (!xfs_verify_fsbext(mp, irec->br_startblock,
					   irec->br_blockcount))
			return __this_address;
	}
	if (irec->br_state != XFS_EXT_NORM && whichfork != XFS_DATA_FORK)
		return __this_address;
	return NULL;
}

int __init
xfs_bmap_intent_init_cache(void)
{
	xfs_bmap_intent_cache = kmem_cache_create("xfs_bmap_intent",
			sizeof(struct xfs_bmap_intent),
			0, 0, NULL);

	return xfs_bmap_intent_cache != NULL ? 0 : -ENOMEM;
}

void
xfs_bmap_intent_destroy_cache(void)
{
	kmem_cache_destroy(xfs_bmap_intent_cache);
	xfs_bmap_intent_cache = NULL;
}

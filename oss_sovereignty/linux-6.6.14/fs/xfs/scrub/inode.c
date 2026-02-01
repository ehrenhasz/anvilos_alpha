
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_btree.h"
#include "xfs_log_format.h"
#include "xfs_trans.h"
#include "xfs_ag.h"
#include "xfs_inode.h"
#include "xfs_ialloc.h"
#include "xfs_icache.h"
#include "xfs_da_format.h"
#include "xfs_reflink.h"
#include "xfs_rmap.h"
#include "xfs_bmap_util.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/btree.h"
#include "scrub/trace.h"

 
static inline int
xchk_prepare_iscrub(
	struct xfs_scrub	*sc)
{
	int			error;

	xchk_ilock(sc, XFS_IOLOCK_EXCL);

	error = xchk_trans_alloc(sc, 0);
	if (error)
		return error;

	xchk_ilock(sc, XFS_ILOCK_EXCL);
	return 0;
}

 
static inline int
xchk_install_handle_iscrub(
	struct xfs_scrub	*sc,
	struct xfs_inode	*ip)
{
	int			error;

	error = xchk_install_handle_inode(sc, ip);
	if (error)
		return error;

	return xchk_prepare_iscrub(sc);
}

 
int
xchk_setup_inode(
	struct xfs_scrub	*sc)
{
	struct xfs_imap		imap;
	struct xfs_inode	*ip;
	struct xfs_mount	*mp = sc->mp;
	struct xfs_inode	*ip_in = XFS_I(file_inode(sc->file));
	struct xfs_buf		*agi_bp;
	struct xfs_perag	*pag;
	xfs_agnumber_t		agno = XFS_INO_TO_AGNO(mp, sc->sm->sm_ino);
	int			error;

	if (xchk_need_intent_drain(sc))
		xchk_fsgates_enable(sc, XCHK_FSGATES_DRAIN);

	 
	if (sc->sm->sm_ino == 0 || sc->sm->sm_ino == ip_in->i_ino) {
		error = xchk_install_live_inode(sc, ip_in);
		if (error)
			return error;

		return xchk_prepare_iscrub(sc);
	}

	 
	if (xfs_internal_inum(mp, sc->sm->sm_ino))
		return -ENOENT;
	if (!xfs_verify_ino(sc->mp, sc->sm->sm_ino))
		return -ENOENT;

	 
	error = xchk_iget(sc, sc->sm->sm_ino, &ip);
	if (!error)
		return xchk_install_handle_iscrub(sc, ip);
	if (error == -ENOENT)
		return error;
	if (error != -EFSCORRUPTED && error != -EFSBADCRC && error != -EINVAL)
		goto out_error;

	 
	error = xchk_trans_alloc(sc, 0);
	if (error)
		goto out_error;

	error = xchk_iget_agi(sc, sc->sm->sm_ino, &agi_bp, &ip);
	if (error == 0) {
		 
		xchk_trans_cancel(sc);
		return xchk_install_handle_iscrub(sc, ip);
	}
	if (error == -ENOENT)
		goto out_gone;
	if (error != -EFSCORRUPTED && error != -EFSBADCRC && error != -EINVAL)
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
	if (error)
		goto out_cancel;

	 
	return 0;

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

 

 
STATIC void
xchk_inode_extsize(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags)
{
	xfs_failaddr_t		fa;
	uint32_t		value = be32_to_cpu(dip->di_extsize);

	fa = xfs_inode_validate_extsize(sc->mp, value, mode, flags);
	if (fa)
		xchk_ino_set_corrupt(sc, ino);

	 
	if ((flags & XFS_DIFLAG_RTINHERIT) &&
	    (flags & XFS_DIFLAG_EXTSZINHERIT) &&
	    value % sc->mp->m_sb.sb_rextsize > 0)
		xchk_ino_set_warning(sc, ino);
}

 
STATIC void
xchk_inode_cowextsize(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags,
	uint64_t		flags2)
{
	xfs_failaddr_t		fa;

	fa = xfs_inode_validate_cowextsize(sc->mp,
			be32_to_cpu(dip->di_cowextsize), mode, flags,
			flags2);
	if (fa)
		xchk_ino_set_corrupt(sc, ino);
}

 
STATIC void
xchk_inode_flags(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags)
{
	struct xfs_mount	*mp = sc->mp;

	 
	if (flags & ~XFS_DIFLAG_ANY)
		goto bad;

	 
	if ((flags & XFS_DIFLAG_REALTIME) && !mp->m_rtdev_targp)
		goto bad;

	 
	if ((flags & XFS_DIFLAG_NEWRTBM) && ino != mp->m_sb.sb_rbmino)
		goto bad;

	 
	if ((flags & (XFS_DIFLAG_RTINHERIT |
		     XFS_DIFLAG_EXTSZINHERIT |
		     XFS_DIFLAG_PROJINHERIT |
		     XFS_DIFLAG_NOSYMLINKS)) &&
	    !S_ISDIR(mode))
		goto bad;

	 
	if ((flags & (XFS_DIFLAG_REALTIME | FS_XFLAG_EXTSIZE)) &&
	    !S_ISREG(mode))
		goto bad;

	 
	if ((flags & XFS_DIFLAG_FILESTREAM) && (flags & XFS_DIFLAG_REALTIME))
		goto bad;

	return;
bad:
	xchk_ino_set_corrupt(sc, ino);
}

 
STATIC void
xchk_inode_flags2(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino,
	uint16_t		mode,
	uint16_t		flags,
	uint64_t		flags2)
{
	struct xfs_mount	*mp = sc->mp;

	 
	if (flags2 & ~XFS_DIFLAG2_ANY)
		xchk_ino_set_warning(sc, ino);

	 
	if ((flags2 & XFS_DIFLAG2_REFLINK) &&
	    !xfs_has_reflink(mp))
		goto bad;

	 

	 
	if ((flags2 & XFS_DIFLAG2_DAX) && !(S_ISREG(mode) || S_ISDIR(mode)))
		goto bad;

	 
	if ((flags2 & XFS_DIFLAG2_REFLINK) && !S_ISREG(mode))
		goto bad;

	 
	if ((flags & XFS_DIFLAG_REALTIME) && (flags2 & XFS_DIFLAG2_REFLINK))
		goto bad;

	 
	if (xfs_dinode_has_bigtime(dip) && !xfs_has_bigtime(mp))
		goto bad;

	return;
bad:
	xchk_ino_set_corrupt(sc, ino);
}

static inline void
xchk_dinode_nsec(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino,
	struct xfs_dinode	*dip,
	const xfs_timestamp_t	ts)
{
	struct timespec64	tv;

	tv = xfs_inode_from_disk_ts(dip, ts);
	if (tv.tv_nsec < 0 || tv.tv_nsec >= NSEC_PER_SEC)
		xchk_ino_set_corrupt(sc, ino);
}

 
STATIC void
xchk_dinode(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip,
	xfs_ino_t		ino)
{
	struct xfs_mount	*mp = sc->mp;
	size_t			fork_recs;
	unsigned long long	isize;
	uint64_t		flags2;
	xfs_extnum_t		nextents;
	xfs_extnum_t		naextents;
	prid_t			prid;
	uint16_t		flags;
	uint16_t		mode;

	flags = be16_to_cpu(dip->di_flags);
	if (dip->di_version >= 3)
		flags2 = be64_to_cpu(dip->di_flags2);
	else
		flags2 = 0;

	 
	mode = be16_to_cpu(dip->di_mode);
	switch (mode & S_IFMT) {
	case S_IFLNK:
	case S_IFREG:
	case S_IFDIR:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		 
		break;
	default:
		xchk_ino_set_corrupt(sc, ino);
		break;
	}

	 
	switch (dip->di_version) {
	case 1:
		 
		xchk_ino_set_preen(sc, ino);
		prid = 0;
		break;
	case 2:
	case 3:
		if (dip->di_onlink != 0)
			xchk_ino_set_corrupt(sc, ino);

		if (dip->di_mode == 0 && sc->ip)
			xchk_ino_set_corrupt(sc, ino);

		if (dip->di_projid_hi != 0 &&
		    !xfs_has_projid32(mp))
			xchk_ino_set_corrupt(sc, ino);

		prid = be16_to_cpu(dip->di_projid_lo);
		break;
	default:
		xchk_ino_set_corrupt(sc, ino);
		return;
	}

	if (xfs_has_projid32(mp))
		prid |= (prid_t)be16_to_cpu(dip->di_projid_hi) << 16;

	 
	if (dip->di_uid == cpu_to_be32(-1U) ||
	    dip->di_gid == cpu_to_be32(-1U))
		xchk_ino_set_warning(sc, ino);

	 
	if (prid == -1U)
		xchk_ino_set_warning(sc, ino);

	 
	switch (dip->di_format) {
	case XFS_DINODE_FMT_DEV:
		if (!S_ISCHR(mode) && !S_ISBLK(mode) &&
		    !S_ISFIFO(mode) && !S_ISSOCK(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_LOCAL:
		if (!S_ISDIR(mode) && !S_ISLNK(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		if (!S_ISREG(mode) && !S_ISDIR(mode) && !S_ISLNK(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (!S_ISREG(mode) && !S_ISDIR(mode))
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_UUID:
	default:
		xchk_ino_set_corrupt(sc, ino);
		break;
	}

	 
	xchk_dinode_nsec(sc, ino, dip, dip->di_atime);
	xchk_dinode_nsec(sc, ino, dip, dip->di_mtime);
	xchk_dinode_nsec(sc, ino, dip, dip->di_ctime);

	 
	isize = be64_to_cpu(dip->di_size);
	if (isize & (1ULL << 63))
		xchk_ino_set_corrupt(sc, ino);

	 
	if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode) && isize != 0)
		xchk_ino_set_corrupt(sc, ino);

	 
	if (S_ISDIR(mode) && (isize == 0 || isize >= XFS_DIR2_SPACE_SIZE))
		xchk_ino_set_corrupt(sc, ino);

	 
	if (S_ISLNK(mode) && (isize == 0 || isize >= XFS_SYMLINK_MAXLEN))
		xchk_ino_set_corrupt(sc, ino);

	 
	if (isize > mp->m_super->s_maxbytes)
		xchk_ino_set_warning(sc, ino);

	 
	if (flags2 & XFS_DIFLAG2_REFLINK) {
		;  
	} else if (flags & XFS_DIFLAG_REALTIME) {
		 
		if (be64_to_cpu(dip->di_nblocks) >=
		    mp->m_sb.sb_dblocks + mp->m_sb.sb_rblocks)
			xchk_ino_set_corrupt(sc, ino);
	} else {
		if (be64_to_cpu(dip->di_nblocks) >= mp->m_sb.sb_dblocks)
			xchk_ino_set_corrupt(sc, ino);
	}

	xchk_inode_flags(sc, dip, ino, mode, flags);

	xchk_inode_extsize(sc, dip, ino, mode, flags);

	nextents = xfs_dfork_data_extents(dip);
	naextents = xfs_dfork_attr_extents(dip);

	 
	fork_recs =  XFS_DFORK_DSIZE(dip, mp) / sizeof(struct xfs_bmbt_rec);
	switch (dip->di_format) {
	case XFS_DINODE_FMT_EXTENTS:
		if (nextents > fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (nextents <= fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	default:
		if (nextents != 0)
			xchk_ino_set_corrupt(sc, ino);
		break;
	}

	 
	if (XFS_DFORK_APTR(dip) >= (char *)dip + mp->m_sb.sb_inodesize)
		xchk_ino_set_corrupt(sc, ino);
	if (naextents != 0 && dip->di_forkoff == 0)
		xchk_ino_set_corrupt(sc, ino);
	if (dip->di_forkoff == 0 && dip->di_aformat != XFS_DINODE_FMT_EXTENTS)
		xchk_ino_set_corrupt(sc, ino);

	 
	if (dip->di_aformat != XFS_DINODE_FMT_LOCAL &&
	    dip->di_aformat != XFS_DINODE_FMT_EXTENTS &&
	    dip->di_aformat != XFS_DINODE_FMT_BTREE)
		xchk_ino_set_corrupt(sc, ino);

	 
	fork_recs =  XFS_DFORK_ASIZE(dip, mp) / sizeof(struct xfs_bmbt_rec);
	switch (dip->di_aformat) {
	case XFS_DINODE_FMT_EXTENTS:
		if (naextents > fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	case XFS_DINODE_FMT_BTREE:
		if (naextents <= fork_recs)
			xchk_ino_set_corrupt(sc, ino);
		break;
	default:
		if (naextents != 0)
			xchk_ino_set_corrupt(sc, ino);
	}

	if (dip->di_version >= 3) {
		xchk_dinode_nsec(sc, ino, dip, dip->di_crtime);
		xchk_inode_flags2(sc, dip, ino, mode, flags, flags2);
		xchk_inode_cowextsize(sc, dip, ino, mode, flags,
				flags2);
	}
}

 
static void
xchk_inode_xref_finobt(
	struct xfs_scrub		*sc,
	xfs_ino_t			ino)
{
	struct xfs_inobt_rec_incore	rec;
	xfs_agino_t			agino;
	int				has_record;
	int				error;

	if (!sc->sa.fino_cur || xchk_skip_xref(sc->sm))
		return;

	agino = XFS_INO_TO_AGINO(sc->mp, ino);

	 
	error = xfs_inobt_lookup(sc->sa.fino_cur, agino, XFS_LOOKUP_LE,
			&has_record);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.fino_cur) ||
	    !has_record)
		return;

	error = xfs_inobt_get_rec(sc->sa.fino_cur, &rec, &has_record);
	if (!xchk_should_check_xref(sc, &error, &sc->sa.fino_cur) ||
	    !has_record)
		return;

	 
	if (rec.ir_startino > agino ||
	    rec.ir_startino + XFS_INODES_PER_CHUNK <= agino)
		return;

	if (rec.ir_free & XFS_INOBT_MASK(agino - rec.ir_startino))
		xchk_btree_xref_set_corrupt(sc, sc->sa.fino_cur, 0);
}

 
STATIC void
xchk_inode_xref_bmap(
	struct xfs_scrub	*sc,
	struct xfs_dinode	*dip)
{
	xfs_extnum_t		nextents;
	xfs_filblks_t		count;
	xfs_filblks_t		acount;
	int			error;

	if (xchk_skip_xref(sc->sm))
		return;

	 
	error = xfs_bmap_count_blocks(sc->tp, sc->ip, XFS_DATA_FORK,
			&nextents, &count);
	if (!xchk_should_check_xref(sc, &error, NULL))
		return;
	if (nextents < xfs_dfork_data_extents(dip))
		xchk_ino_xref_set_corrupt(sc, sc->ip->i_ino);

	error = xfs_bmap_count_blocks(sc->tp, sc->ip, XFS_ATTR_FORK,
			&nextents, &acount);
	if (!xchk_should_check_xref(sc, &error, NULL))
		return;
	if (nextents != xfs_dfork_attr_extents(dip))
		xchk_ino_xref_set_corrupt(sc, sc->ip->i_ino);

	 
	if (count + acount != be64_to_cpu(dip->di_nblocks))
		xchk_ino_xref_set_corrupt(sc, sc->ip->i_ino);
}

 
STATIC void
xchk_inode_xref(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino,
	struct xfs_dinode	*dip)
{
	xfs_agnumber_t		agno;
	xfs_agblock_t		agbno;
	int			error;

	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		return;

	agno = XFS_INO_TO_AGNO(sc->mp, ino);
	agbno = XFS_INO_TO_AGBNO(sc->mp, ino);

	error = xchk_ag_init_existing(sc, agno, &sc->sa);
	if (!xchk_xref_process_error(sc, agno, agbno, &error))
		goto out_free;

	xchk_xref_is_used_space(sc, agbno, 1);
	xchk_inode_xref_finobt(sc, ino);
	xchk_xref_is_only_owned_by(sc, agbno, 1, &XFS_RMAP_OINFO_INODES);
	xchk_xref_is_not_shared(sc, agbno, 1);
	xchk_xref_is_not_cow_staging(sc, agbno, 1);
	xchk_inode_xref_bmap(sc, dip);

out_free:
	xchk_ag_free(sc, &sc->sa);
}

 
static void
xchk_inode_check_reflink_iflag(
	struct xfs_scrub	*sc,
	xfs_ino_t		ino)
{
	struct xfs_mount	*mp = sc->mp;
	bool			has_shared;
	int			error;

	if (!xfs_has_reflink(mp))
		return;

	error = xfs_reflink_inode_has_shared_extents(sc->tp, sc->ip,
			&has_shared);
	if (!xchk_xref_process_error(sc, XFS_INO_TO_AGNO(mp, ino),
			XFS_INO_TO_AGBNO(mp, ino), &error))
		return;
	if (xfs_is_reflink_inode(sc->ip) && !has_shared)
		xchk_ino_set_preen(sc, ino);
	else if (!xfs_is_reflink_inode(sc->ip) && has_shared)
		xchk_ino_set_corrupt(sc, ino);
}

 
int
xchk_inode(
	struct xfs_scrub	*sc)
{
	struct xfs_dinode	di;
	int			error = 0;

	 
	if (!sc->ip) {
		xchk_ino_set_corrupt(sc, sc->sm->sm_ino);
		return 0;
	}

	 
	xfs_inode_to_disk(sc->ip, &di, 0);
	xchk_dinode(sc, &di, sc->ip->i_ino);
	if (sc->sm->sm_flags & XFS_SCRUB_OFLAG_CORRUPT)
		goto out;

	 
	if (S_ISREG(VFS_I(sc->ip)->i_mode))
		xchk_inode_check_reflink_iflag(sc, sc->ip->i_ino);

	xchk_inode_xref(sc, sc->ip->i_ino, &di);
out:
	return error;
}

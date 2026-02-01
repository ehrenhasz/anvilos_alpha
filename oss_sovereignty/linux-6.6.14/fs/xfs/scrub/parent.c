
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_log_format.h"
#include "xfs_inode.h"
#include "xfs_icache.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "scrub/scrub.h"
#include "scrub/common.h"
#include "scrub/readdir.h"

 
int
xchk_setup_parent(
	struct xfs_scrub	*sc)
{
	return xchk_setup_inode_contents(sc, 0);
}

 

 

struct xchk_parent_ctx {
	struct xfs_scrub	*sc;
	xfs_nlink_t		nlink;
};

 
STATIC int
xchk_parent_actor(
	struct xfs_scrub	*sc,
	struct xfs_inode	*dp,
	xfs_dir2_dataptr_t	dapos,
	const struct xfs_name	*name,
	xfs_ino_t		ino,
	void			*priv)
{
	struct xchk_parent_ctx	*spc = priv;
	int			error = 0;

	 
	if (!xfs_dir2_namecheck(name->name, name->len))
		error = -EFSCORRUPTED;
	if (!xchk_fblock_xref_process_error(sc, XFS_DATA_FORK, 0, &error))
		return error;

	if (sc->ip->i_ino == ino)
		spc->nlink++;

	if (xchk_should_terminate(spc->sc, &error))
		return error;

	return 0;
}

 
STATIC unsigned int
xchk_parent_ilock_dir(
	struct xfs_inode	*dp)
{
	if (!xfs_ilock_nowait(dp, XFS_ILOCK_SHARED))
		return 0;

	if (!xfs_need_iread_extents(&dp->i_df))
		return XFS_ILOCK_SHARED;

	xfs_iunlock(dp, XFS_ILOCK_SHARED);

	if (!xfs_ilock_nowait(dp, XFS_ILOCK_EXCL))
		return 0;

	return XFS_ILOCK_EXCL;
}

 
STATIC int
xchk_parent_validate(
	struct xfs_scrub	*sc,
	xfs_ino_t		parent_ino)
{
	struct xchk_parent_ctx	spc = {
		.sc		= sc,
		.nlink		= 0,
	};
	struct xfs_mount	*mp = sc->mp;
	struct xfs_inode	*dp = NULL;
	xfs_nlink_t		expected_nlink;
	unsigned int		lock_mode;
	int			error = 0;

	 
	if (sc->ip == mp->m_rootip) {
		if (sc->ip->i_ino != mp->m_sb.sb_rootino ||
		    sc->ip->i_ino != parent_ino)
			xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
		return 0;
	}

	 
	if (sc->ip->i_ino == parent_ino) {
		xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
		return 0;
	}

	 
	expected_nlink = VFS_I(sc->ip)->i_nlink == 0 ? 0 : 1;

	 
	error = xchk_iget(sc, parent_ino, &dp);
	if (error == -EINVAL || error == -ENOENT) {
		error = -EFSCORRUPTED;
		xchk_fblock_process_error(sc, XFS_DATA_FORK, 0, &error);
		return error;
	}
	if (!xchk_fblock_xref_process_error(sc, XFS_DATA_FORK, 0, &error))
		return error;
	if (dp == sc->ip || !S_ISDIR(VFS_I(dp)->i_mode)) {
		xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
		goto out_rele;
	}

	lock_mode = xchk_parent_ilock_dir(dp);
	if (!lock_mode) {
		xchk_iunlock(sc, XFS_ILOCK_EXCL);
		xchk_ilock(sc, XFS_ILOCK_EXCL);
		error = -EAGAIN;
		goto out_rele;
	}

	 
	error = xchk_dir_walk(sc, dp, xchk_parent_actor, &spc);
	if (!xchk_fblock_xref_process_error(sc, XFS_DATA_FORK, 0, &error))
		goto out_unlock;

	 
	if (spc.nlink != expected_nlink)
		xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);

out_unlock:
	xfs_iunlock(dp, lock_mode);
out_rele:
	xchk_irele(sc, dp);
	return error;
}

 
int
xchk_parent(
	struct xfs_scrub	*sc)
{
	struct xfs_mount	*mp = sc->mp;
	xfs_ino_t		parent_ino;
	int			error = 0;

	 
	if (!S_ISDIR(VFS_I(sc->ip)->i_mode))
		return -ENOENT;

	 
	if (!xfs_verify_dir_ino(mp, sc->ip->i_ino)) {
		xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
		return 0;
	}

	do {
		if (xchk_should_terminate(sc, &error))
			break;

		 
		error = xchk_dir_lookup(sc, sc->ip, &xfs_name_dotdot,
				&parent_ino);
		if (!xchk_fblock_process_error(sc, XFS_DATA_FORK, 0, &error))
			return error;
		if (!xfs_verify_dir_ino(mp, parent_ino)) {
			xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
			return 0;
		}

		 
		error = xchk_parent_validate(sc, parent_ino);
	} while (error == -EAGAIN);

	return error;
}

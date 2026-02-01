
 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_log_format.h"
#include "xfs_inode.h"
#include "xfs_symlink.h"
#include "scrub/scrub.h"
#include "scrub/common.h"

 
int
xchk_setup_symlink(
	struct xfs_scrub	*sc)
{
	 
	sc->buf = kvzalloc(XFS_SYMLINK_MAXLEN + 1, XCHK_GFP_FLAGS);
	if (!sc->buf)
		return -ENOMEM;

	return xchk_setup_inode_contents(sc, 0);
}

 

int
xchk_symlink(
	struct xfs_scrub	*sc)
{
	struct xfs_inode	*ip = sc->ip;
	struct xfs_ifork	*ifp;
	loff_t			len;
	int			error = 0;

	if (!S_ISLNK(VFS_I(ip)->i_mode))
		return -ENOENT;
	ifp = xfs_ifork_ptr(ip, XFS_DATA_FORK);
	len = ip->i_disk_size;

	 
	if (len > XFS_SYMLINK_MAXLEN || len <= 0) {
		xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
		goto out;
	}

	 
	if (ifp->if_format == XFS_DINODE_FMT_LOCAL) {
		if (len > xfs_inode_data_fork_size(ip) ||
		    len > strnlen(ifp->if_u1.if_data, xfs_inode_data_fork_size(ip)))
			xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
		goto out;
	}

	 
	error = xfs_readlink_bmap_ilocked(sc->ip, sc->buf);
	if (!xchk_fblock_process_error(sc, XFS_DATA_FORK, 0, &error))
		goto out;
	if (strnlen(sc->buf, XFS_SYMLINK_MAXLEN) < len)
		xchk_fblock_set_corrupt(sc, XFS_DATA_FORK, 0);
out:
	return error;
}

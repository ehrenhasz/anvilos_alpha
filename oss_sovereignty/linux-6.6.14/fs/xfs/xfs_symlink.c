
 
#include "xfs.h"
#include "xfs_shared.h"
#include "xfs_fs.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_mount.h"
#include "xfs_dir2.h"
#include "xfs_inode.h"
#include "xfs_bmap.h"
#include "xfs_bmap_btree.h"
#include "xfs_quota.h"
#include "xfs_symlink.h"
#include "xfs_trans_space.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_ialloc.h"
#include "xfs_error.h"

 
int
xfs_readlink_bmap_ilocked(
	struct xfs_inode	*ip,
	char			*link)
{
	struct xfs_mount	*mp = ip->i_mount;
	struct xfs_bmbt_irec	mval[XFS_SYMLINK_MAPS];
	struct xfs_buf		*bp;
	xfs_daddr_t		d;
	char			*cur_chunk;
	int			pathlen = ip->i_disk_size;
	int			nmaps = XFS_SYMLINK_MAPS;
	int			byte_cnt;
	int			n;
	int			error = 0;
	int			fsblocks = 0;
	int			offset;

	ASSERT(xfs_isilocked(ip, XFS_ILOCK_SHARED | XFS_ILOCK_EXCL));

	fsblocks = xfs_symlink_blocks(mp, pathlen);
	error = xfs_bmapi_read(ip, 0, fsblocks, mval, &nmaps, 0);
	if (error)
		goto out;

	offset = 0;
	for (n = 0; n < nmaps; n++) {
		d = XFS_FSB_TO_DADDR(mp, mval[n].br_startblock);
		byte_cnt = XFS_FSB_TO_B(mp, mval[n].br_blockcount);

		error = xfs_buf_read(mp->m_ddev_targp, d, BTOBB(byte_cnt), 0,
				&bp, &xfs_symlink_buf_ops);
		if (error)
			return error;
		byte_cnt = XFS_SYMLINK_BUF_SPACE(mp, byte_cnt);
		if (pathlen < byte_cnt)
			byte_cnt = pathlen;

		cur_chunk = bp->b_addr;
		if (xfs_has_crc(mp)) {
			if (!xfs_symlink_hdr_ok(ip->i_ino, offset,
							byte_cnt, bp)) {
				error = -EFSCORRUPTED;
				xfs_alert(mp,
"symlink header does not match required off/len/owner (0x%x/Ox%x,0x%llx)",
					offset, byte_cnt, ip->i_ino);
				xfs_buf_relse(bp);
				goto out;

			}

			cur_chunk += sizeof(struct xfs_dsymlink_hdr);
		}

		memcpy(link + offset, cur_chunk, byte_cnt);

		pathlen -= byte_cnt;
		offset += byte_cnt;

		xfs_buf_relse(bp);
	}
	ASSERT(pathlen == 0);

	link[ip->i_disk_size] = '\0';
	error = 0;

 out:
	return error;
}

int
xfs_readlink(
	struct xfs_inode	*ip,
	char			*link)
{
	struct xfs_mount	*mp = ip->i_mount;
	xfs_fsize_t		pathlen;
	int			error = -EFSCORRUPTED;

	trace_xfs_readlink(ip);

	if (xfs_is_shutdown(mp))
		return -EIO;

	xfs_ilock(ip, XFS_ILOCK_SHARED);

	pathlen = ip->i_disk_size;
	if (!pathlen)
		goto out;

	if (pathlen < 0 || pathlen > XFS_SYMLINK_MAXLEN) {
		xfs_alert(mp, "%s: inode (%llu) bad symlink length (%lld)",
			 __func__, (unsigned long long) ip->i_ino,
			 (long long) pathlen);
		ASSERT(0);
		goto out;
	}

	if (ip->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		 
		if (XFS_IS_CORRUPT(ip->i_mount, !ip->i_df.if_u1.if_data))
			goto out;

		memcpy(link, ip->i_df.if_u1.if_data, pathlen + 1);
		error = 0;
	} else {
		error = xfs_readlink_bmap_ilocked(ip, link);
	}

 out:
	xfs_iunlock(ip, XFS_ILOCK_SHARED);
	return error;
}

int
xfs_symlink(
	struct mnt_idmap	*idmap,
	struct xfs_inode	*dp,
	struct xfs_name		*link_name,
	const char		*target_path,
	umode_t			mode,
	struct xfs_inode	**ipp)
{
	struct xfs_mount	*mp = dp->i_mount;
	struct xfs_trans	*tp = NULL;
	struct xfs_inode	*ip = NULL;
	int			error = 0;
	int			pathlen;
	bool                    unlock_dp_on_error = false;
	xfs_fileoff_t		first_fsb;
	xfs_filblks_t		fs_blocks;
	int			nmaps;
	struct xfs_bmbt_irec	mval[XFS_SYMLINK_MAPS];
	xfs_daddr_t		d;
	const char		*cur_chunk;
	int			byte_cnt;
	int			n;
	struct xfs_buf		*bp;
	prid_t			prid;
	struct xfs_dquot	*udqp = NULL;
	struct xfs_dquot	*gdqp = NULL;
	struct xfs_dquot	*pdqp = NULL;
	uint			resblks;
	xfs_ino_t		ino;

	*ipp = NULL;

	trace_xfs_symlink(dp, link_name);

	if (xfs_is_shutdown(mp))
		return -EIO;

	 
	pathlen = strlen(target_path);
	if (pathlen >= XFS_SYMLINK_MAXLEN)       
		return -ENAMETOOLONG;
	ASSERT(pathlen > 0);

	prid = xfs_get_initial_prid(dp);

	 
	error = xfs_qm_vop_dqalloc(dp, mapped_fsuid(idmap, &init_user_ns),
			mapped_fsgid(idmap, &init_user_ns), prid,
			XFS_QMOPT_QUOTALL | XFS_QMOPT_INHERIT,
			&udqp, &gdqp, &pdqp);
	if (error)
		return error;

	 
	if (pathlen <= XFS_LITINO(mp))
		fs_blocks = 0;
	else
		fs_blocks = xfs_symlink_blocks(mp, pathlen);
	resblks = XFS_SYMLINK_SPACE_RES(mp, link_name->len, fs_blocks);

	error = xfs_trans_alloc_icreate(mp, &M_RES(mp)->tr_symlink, udqp, gdqp,
			pdqp, resblks, &tp);
	if (error)
		goto out_release_dquots;

	xfs_ilock(dp, XFS_ILOCK_EXCL | XFS_ILOCK_PARENT);
	unlock_dp_on_error = true;

	 
	if (dp->i_diflags & XFS_DIFLAG_NOSYMLINKS) {
		error = -EPERM;
		goto out_trans_cancel;
	}

	 
	error = xfs_dialloc(&tp, dp->i_ino, S_IFLNK, &ino);
	if (!error)
		error = xfs_init_new_inode(idmap, tp, dp, ino,
				S_IFLNK | (mode & ~S_IFMT), 1, 0, prid,
				false, &ip);
	if (error)
		goto out_trans_cancel;

	 
	xfs_trans_ijoin(tp, dp, XFS_ILOCK_EXCL);
	unlock_dp_on_error = false;

	 
	xfs_qm_vop_create_dqattach(tp, ip, udqp, gdqp, pdqp);

	resblks -= XFS_IALLOC_SPACE_RES(mp);
	 
	if (pathlen <= xfs_inode_data_fork_size(ip)) {
		xfs_init_local_fork(ip, XFS_DATA_FORK, target_path, pathlen);

		ip->i_disk_size = pathlen;
		ip->i_df.if_format = XFS_DINODE_FMT_LOCAL;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_DDATA | XFS_ILOG_CORE);
	} else {
		int	offset;

		first_fsb = 0;
		nmaps = XFS_SYMLINK_MAPS;

		error = xfs_bmapi_write(tp, ip, first_fsb, fs_blocks,
				  XFS_BMAPI_METADATA, resblks, mval, &nmaps);
		if (error)
			goto out_trans_cancel;

		resblks -= fs_blocks;
		ip->i_disk_size = pathlen;
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

		cur_chunk = target_path;
		offset = 0;
		for (n = 0; n < nmaps; n++) {
			char	*buf;

			d = XFS_FSB_TO_DADDR(mp, mval[n].br_startblock);
			byte_cnt = XFS_FSB_TO_B(mp, mval[n].br_blockcount);
			error = xfs_trans_get_buf(tp, mp->m_ddev_targp, d,
					       BTOBB(byte_cnt), 0, &bp);
			if (error)
				goto out_trans_cancel;
			bp->b_ops = &xfs_symlink_buf_ops;

			byte_cnt = XFS_SYMLINK_BUF_SPACE(mp, byte_cnt);
			byte_cnt = min(byte_cnt, pathlen);

			buf = bp->b_addr;
			buf += xfs_symlink_hdr_set(mp, ip->i_ino, offset,
						   byte_cnt, bp);

			memcpy(buf, cur_chunk, byte_cnt);

			cur_chunk += byte_cnt;
			pathlen -= byte_cnt;
			offset += byte_cnt;

			xfs_trans_buf_set_type(tp, bp, XFS_BLFT_SYMLINK_BUF);
			xfs_trans_log_buf(tp, bp, 0, (buf + byte_cnt - 1) -
							(char *)bp->b_addr);
		}
		ASSERT(pathlen == 0);
	}
	i_size_write(VFS_I(ip), ip->i_disk_size);

	 
	error = xfs_dir_createname(tp, dp, link_name, ip->i_ino, resblks);
	if (error)
		goto out_trans_cancel;
	xfs_trans_ichgtime(tp, dp, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);

	 
	if (xfs_has_wsync(mp) || xfs_has_dirsync(mp))
		xfs_trans_set_sync(tp);

	error = xfs_trans_commit(tp);
	if (error)
		goto out_release_inode;

	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	xfs_qm_dqrele(pdqp);

	*ipp = ip;
	return 0;

out_trans_cancel:
	xfs_trans_cancel(tp);
out_release_inode:
	 
	if (ip) {
		xfs_finish_inode_setup(ip);
		xfs_irele(ip);
	}
out_release_dquots:
	xfs_qm_dqrele(udqp);
	xfs_qm_dqrele(gdqp);
	xfs_qm_dqrele(pdqp);

	if (unlock_dp_on_error)
		xfs_iunlock(dp, XFS_ILOCK_EXCL);
	return error;
}

 
STATIC int
xfs_inactive_symlink_rmt(
	struct xfs_inode *ip)
{
	struct xfs_buf	*bp;
	int		done;
	int		error;
	int		i;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	mval[XFS_SYMLINK_MAPS];
	int		nmaps;
	int		size;
	xfs_trans_t	*tp;

	mp = ip->i_mount;
	ASSERT(!xfs_need_iread_extents(&ip->i_df));
	 
	ASSERT(ip->i_df.if_nextents > 0 && ip->i_df.if_nextents <= 2);

	error = xfs_trans_alloc(mp, &M_RES(mp)->tr_itruncate, 0, 0, 0, &tp);
	if (error)
		return error;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	xfs_trans_ijoin(tp, ip, 0);

	 
	size = (int)ip->i_disk_size;
	ip->i_disk_size = 0;
	VFS_I(ip)->i_mode = (VFS_I(ip)->i_mode & ~S_IFMT) | S_IFREG;
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	 
	done = 0;
	nmaps = ARRAY_SIZE(mval);
	error = xfs_bmapi_read(ip, 0, xfs_symlink_blocks(mp, size),
				mval, &nmaps, 0);
	if (error)
		goto error_trans_cancel;
	 
	for (i = 0; i < nmaps; i++) {
		error = xfs_trans_get_buf(tp, mp->m_ddev_targp,
				XFS_FSB_TO_DADDR(mp, mval[i].br_startblock),
				XFS_FSB_TO_BB(mp, mval[i].br_blockcount), 0,
				&bp);
		if (error)
			goto error_trans_cancel;
		xfs_trans_binval(tp, bp);
	}
	 
	error = xfs_bunmapi(tp, ip, 0, size, 0, nmaps, &done);
	if (error)
		goto error_trans_cancel;
	ASSERT(done);

	 
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	error = xfs_trans_commit(tp);
	if (error) {
		ASSERT(xfs_is_shutdown(mp));
		goto error_unlock;
	}

	 
	if (ip->i_df.if_bytes)
		xfs_idata_realloc(ip, -ip->i_df.if_bytes, XFS_DATA_FORK);
	ASSERT(ip->i_df.if_bytes == 0);

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return 0;

error_trans_cancel:
	xfs_trans_cancel(tp);
error_unlock:
	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	return error;
}

 
int
xfs_inactive_symlink(
	struct xfs_inode	*ip)
{
	struct xfs_mount	*mp = ip->i_mount;
	int			pathlen;

	trace_xfs_inactive_symlink(ip);

	if (xfs_is_shutdown(mp))
		return -EIO;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	pathlen = (int)ip->i_disk_size;
	ASSERT(pathlen);

	if (pathlen <= 0 || pathlen > XFS_SYMLINK_MAXLEN) {
		xfs_alert(mp, "%s: inode (0x%llx) bad symlink length (%d)",
			 __func__, (unsigned long long)ip->i_ino, pathlen);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		ASSERT(0);
		return -EFSCORRUPTED;
	}

	 
	if (ip->i_df.if_format == XFS_DINODE_FMT_LOCAL) {
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
		return 0;
	}

	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	 
	return xfs_inactive_symlink_rmt(ip);
}

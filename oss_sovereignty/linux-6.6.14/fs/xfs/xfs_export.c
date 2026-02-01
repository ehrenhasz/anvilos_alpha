
 
#include "xfs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_dir2.h"
#include "xfs_export.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_inode_item.h"
#include "xfs_icache.h"
#include "xfs_pnfs.h"

 
static int xfs_fileid_length(int fileid_type)
{
	switch (fileid_type) {
	case FILEID_INO32_GEN:
		return 2;
	case FILEID_INO32_GEN_PARENT:
		return 4;
	case FILEID_INO32_GEN | XFS_FILEID_TYPE_64FLAG:
		return 3;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
		return 6;
	}
	return FILEID_INVALID;
}

STATIC int
xfs_fs_encode_fh(
	struct inode	*inode,
	__u32		*fh,
	int		*max_len,
	struct inode	*parent)
{
	struct xfs_mount	*mp = XFS_M(inode->i_sb);
	struct fid		*fid = (struct fid *)fh;
	struct xfs_fid64	*fid64 = (struct xfs_fid64 *)fh;
	int			fileid_type;
	int			len;

	 
	if (!parent)
		fileid_type = FILEID_INO32_GEN;
	else
		fileid_type = FILEID_INO32_GEN_PARENT;

	 
	if (!xfs_has_small_inums(mp) || xfs_is_inode32(mp))
		fileid_type |= XFS_FILEID_TYPE_64FLAG;

	 
	len = xfs_fileid_length(fileid_type);
	if (*max_len < len) {
		*max_len = len;
		return FILEID_INVALID;
	}
	*max_len = len;

	switch (fileid_type) {
	case FILEID_INO32_GEN_PARENT:
		fid->i32.parent_ino = XFS_I(parent)->i_ino;
		fid->i32.parent_gen = parent->i_generation;
		fallthrough;
	case FILEID_INO32_GEN:
		fid->i32.ino = XFS_I(inode)->i_ino;
		fid->i32.gen = inode->i_generation;
		break;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
		fid64->parent_ino = XFS_I(parent)->i_ino;
		fid64->parent_gen = parent->i_generation;
		fallthrough;
	case FILEID_INO32_GEN | XFS_FILEID_TYPE_64FLAG:
		fid64->ino = XFS_I(inode)->i_ino;
		fid64->gen = inode->i_generation;
		break;
	}

	return fileid_type;
}

STATIC struct inode *
xfs_nfs_get_inode(
	struct super_block	*sb,
	u64			ino,
	u32			generation)
{
 	xfs_mount_t		*mp = XFS_M(sb);
	xfs_inode_t		*ip;
	int			error;

	 
	if (ino == 0)
		return ERR_PTR(-ESTALE);

	 
	error = xfs_iget(mp, NULL, ino, XFS_IGET_UNTRUSTED, 0, &ip);
	if (error) {

		 
		switch (error) {
		case -EINVAL:
		case -ENOENT:
		case -EFSCORRUPTED:
			error = -ESTALE;
			break;
		default:
			break;
		}
		return ERR_PTR(error);
	}

	 
	if (xfs_inode_unlinked_incomplete(ip)) {
		error = xfs_inode_reload_unlinked(ip);
		if (error) {
			xfs_force_shutdown(mp, SHUTDOWN_CORRUPT_INCORE);
			xfs_irele(ip);
			return ERR_PTR(error);
		}
	}

	if (VFS_I(ip)->i_generation != generation) {
		xfs_irele(ip);
		return ERR_PTR(-ESTALE);
	}

	return VFS_I(ip);
}

STATIC struct dentry *
xfs_fs_fh_to_dentry(struct super_block *sb, struct fid *fid,
		 int fh_len, int fileid_type)
{
	struct xfs_fid64	*fid64 = (struct xfs_fid64 *)fid;
	struct inode		*inode = NULL;

	if (fh_len < xfs_fileid_length(fileid_type))
		return NULL;

	switch (fileid_type) {
	case FILEID_INO32_GEN_PARENT:
	case FILEID_INO32_GEN:
		inode = xfs_nfs_get_inode(sb, fid->i32.ino, fid->i32.gen);
		break;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
	case FILEID_INO32_GEN | XFS_FILEID_TYPE_64FLAG:
		inode = xfs_nfs_get_inode(sb, fid64->ino, fid64->gen);
		break;
	}

	return d_obtain_alias(inode);
}

STATIC struct dentry *
xfs_fs_fh_to_parent(struct super_block *sb, struct fid *fid,
		 int fh_len, int fileid_type)
{
	struct xfs_fid64	*fid64 = (struct xfs_fid64 *)fid;
	struct inode		*inode = NULL;

	if (fh_len < xfs_fileid_length(fileid_type))
		return NULL;

	switch (fileid_type) {
	case FILEID_INO32_GEN_PARENT:
		inode = xfs_nfs_get_inode(sb, fid->i32.parent_ino,
					      fid->i32.parent_gen);
		break;
	case FILEID_INO32_GEN_PARENT | XFS_FILEID_TYPE_64FLAG:
		inode = xfs_nfs_get_inode(sb, fid64->parent_ino,
					      fid64->parent_gen);
		break;
	}

	return d_obtain_alias(inode);
}

STATIC struct dentry *
xfs_fs_get_parent(
	struct dentry		*child)
{
	int			error;
	struct xfs_inode	*cip;

	error = xfs_lookup(XFS_I(d_inode(child)), &xfs_name_dotdot, &cip, NULL);
	if (unlikely(error))
		return ERR_PTR(error);

	return d_obtain_alias(VFS_I(cip));
}

STATIC int
xfs_fs_nfs_commit_metadata(
	struct inode		*inode)
{
	return xfs_log_force_inode(XFS_I(inode));
}

const struct export_operations xfs_export_operations = {
	.encode_fh		= xfs_fs_encode_fh,
	.fh_to_dentry		= xfs_fs_fh_to_dentry,
	.fh_to_parent		= xfs_fs_fh_to_parent,
	.get_parent		= xfs_fs_get_parent,
	.commit_metadata	= xfs_fs_nfs_commit_metadata,
#ifdef CONFIG_EXPORTFS_BLOCK_OPS
	.get_uuid		= xfs_fs_get_uuid,
	.map_blocks		= xfs_fs_map_blocks,
	.commit_blocks		= xfs_fs_commit_blocks,
#endif
};

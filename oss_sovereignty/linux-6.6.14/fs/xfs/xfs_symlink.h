#ifndef __XFS_SYMLINK_H
#define __XFS_SYMLINK_H 1
int xfs_symlink(struct mnt_idmap *idmap, struct xfs_inode *dp,
		struct xfs_name *link_name, const char *target_path,
		umode_t mode, struct xfs_inode **ipp);
int xfs_readlink_bmap_ilocked(struct xfs_inode *ip, char *link);
int xfs_readlink(struct xfs_inode *ip, char *link);
int xfs_inactive_symlink(struct xfs_inode *ip);
#endif  

#ifndef __XFS_XATTR_H__
#define __XFS_XATTR_H__
int xfs_attr_change(struct xfs_da_args *args);
extern const struct xattr_handler *xfs_xattr_handlers[];
#endif  


#ifndef	__XFS_ATTR_ITEM_H__
#define	__XFS_ATTR_ITEM_H__



struct xfs_mount;
struct kmem_zone;

struct xfs_attri_log_nameval {
	struct xfs_log_iovec	name;
	struct xfs_log_iovec	value;
	refcount_t		refcount;

	
};


struct xfs_attri_log_item {
	struct xfs_log_item		attri_item;
	atomic_t			attri_refcount;
	struct xfs_attri_log_nameval	*attri_nameval;
	struct xfs_attri_log_format	attri_format;
};


struct xfs_attrd_log_item {
	struct xfs_log_item		attrd_item;
	struct xfs_attri_log_item	*attrd_attrip;
	struct xfs_attrd_log_format	attrd_format;
};

extern struct kmem_cache	*xfs_attri_cache;
extern struct kmem_cache	*xfs_attrd_cache;

#endif	

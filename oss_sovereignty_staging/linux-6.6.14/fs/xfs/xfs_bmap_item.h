
 
#ifndef	__XFS_BMAP_ITEM_H__
#define	__XFS_BMAP_ITEM_H__

 

 

struct xfs_mount;
struct kmem_cache;

 
#define	XFS_BUI_MAX_FAST_EXTENTS	1

 
struct xfs_bui_log_item {
	struct xfs_log_item		bui_item;
	atomic_t			bui_refcount;
	atomic_t			bui_next_extent;
	struct xfs_bui_log_format	bui_format;
};

static inline size_t
xfs_bui_log_item_sizeof(
	unsigned int		nr)
{
	return offsetof(struct xfs_bui_log_item, bui_format) +
			xfs_bui_log_format_sizeof(nr);
}

 
struct xfs_bud_log_item {
	struct xfs_log_item		bud_item;
	struct xfs_bui_log_item		*bud_buip;
	struct xfs_bud_log_format	bud_format;
};

extern struct kmem_cache	*xfs_bui_cache;
extern struct kmem_cache	*xfs_bud_cache;

#endif	 

#ifndef	__XFS_RMAP_ITEM_H__
#define	__XFS_RMAP_ITEM_H__
struct xfs_mount;
struct kmem_cache;
#define	XFS_RUI_MAX_FAST_EXTENTS	16
struct xfs_rui_log_item {
	struct xfs_log_item		rui_item;
	atomic_t			rui_refcount;
	atomic_t			rui_next_extent;
	struct xfs_rui_log_format	rui_format;
};
static inline size_t
xfs_rui_log_item_sizeof(
	unsigned int		nr)
{
	return offsetof(struct xfs_rui_log_item, rui_format) +
			xfs_rui_log_format_sizeof(nr);
}
struct xfs_rud_log_item {
	struct xfs_log_item		rud_item;
	struct xfs_rui_log_item		*rud_ruip;
	struct xfs_rud_log_format	rud_format;
};
extern struct kmem_cache	*xfs_rui_cache;
extern struct kmem_cache	*xfs_rud_cache;
#endif	 

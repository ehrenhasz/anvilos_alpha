#ifndef	__XFS_REFCOUNT_ITEM_H__
#define	__XFS_REFCOUNT_ITEM_H__
struct xfs_mount;
struct kmem_cache;
#define	XFS_CUI_MAX_FAST_EXTENTS	16
struct xfs_cui_log_item {
	struct xfs_log_item		cui_item;
	atomic_t			cui_refcount;
	atomic_t			cui_next_extent;
	struct xfs_cui_log_format	cui_format;
};
static inline size_t
xfs_cui_log_item_sizeof(
	unsigned int		nr)
{
	return offsetof(struct xfs_cui_log_item, cui_format) +
			xfs_cui_log_format_sizeof(nr);
}
struct xfs_cud_log_item {
	struct xfs_log_item		cud_item;
	struct xfs_cui_log_item		*cud_cuip;
	struct xfs_cud_log_format	cud_format;
};
extern struct kmem_cache	*xfs_cui_cache;
extern struct kmem_cache	*xfs_cud_cache;
#endif	 

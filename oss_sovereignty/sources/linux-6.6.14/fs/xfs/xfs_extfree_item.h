

#ifndef	__XFS_EXTFREE_ITEM_H__
#define	__XFS_EXTFREE_ITEM_H__



struct xfs_mount;
struct kmem_cache;


#define	XFS_EFI_MAX_FAST_EXTENTS	16


struct xfs_efi_log_item {
	struct xfs_log_item	efi_item;
	atomic_t		efi_refcount;
	atomic_t		efi_next_extent;
	xfs_efi_log_format_t	efi_format;
};

static inline size_t
xfs_efi_log_item_sizeof(
	unsigned int		nr)
{
	return offsetof(struct xfs_efi_log_item, efi_format) +
			xfs_efi_log_format_sizeof(nr);
}


struct xfs_efd_log_item {
	struct xfs_log_item	efd_item;
	struct xfs_efi_log_item *efd_efip;
	uint			efd_next_extent;
	xfs_efd_log_format_t	efd_format;
};

static inline size_t
xfs_efd_log_item_sizeof(
	unsigned int		nr)
{
	return offsetof(struct xfs_efd_log_item, efd_format) +
			xfs_efd_log_format_sizeof(nr);
}


#define	XFS_EFD_MAX_FAST_EXTENTS	16

extern struct kmem_cache	*xfs_efi_cache;
extern struct kmem_cache	*xfs_efd_cache;

#endif	

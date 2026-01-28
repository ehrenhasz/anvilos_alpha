

#ifndef __XFS_FSMAP_H__
#define __XFS_FSMAP_H__

struct fsmap;


struct xfs_fsmap {
	dev_t		fmr_device;	
	uint32_t	fmr_flags;	
	uint64_t	fmr_physical;	
	uint64_t	fmr_owner;	
	xfs_fileoff_t	fmr_offset;	
	xfs_filblks_t	fmr_length;	
};

struct xfs_fsmap_head {
	uint32_t	fmh_iflags;	
	uint32_t	fmh_oflags;	
	unsigned int	fmh_count;	
	unsigned int	fmh_entries;	

	struct xfs_fsmap fmh_keys[2];	
};

void xfs_fsmap_to_internal(struct xfs_fsmap *dest, struct fsmap *src);

int xfs_getfsmap(struct xfs_mount *mp, struct xfs_fsmap_head *head,
		struct fsmap *out_recs);

#endif 

 
 
#ifndef __XFS_ATTR_SF_H__
#define	__XFS_ATTR_SF_H__

 
typedef struct xfs_attr_sf_hdr xfs_attr_sf_hdr_t;

 
typedef struct xfs_attr_sf_sort {
	uint8_t		entno;		 
	uint8_t		namelen;	 
	uint8_t		valuelen;	 
	uint8_t		flags;		 
	xfs_dahash_t	hash;		 
	unsigned char	*name;		 
} xfs_attr_sf_sort_t;

#define XFS_ATTR_SF_ENTSIZE_MAX			  \
	((1 << (NBBY*(int)sizeof(uint8_t))) - 1)

 
static inline int xfs_attr_sf_entsize_byname(uint8_t nlen, uint8_t vlen)
{
	return sizeof(struct xfs_attr_sf_entry) + nlen + vlen;
}

 
static inline int xfs_attr_sf_entsize(struct xfs_attr_sf_entry *sfep)
{
	return struct_size(sfep, nameval, sfep->namelen + sfep->valuelen);
}

 
static inline struct xfs_attr_sf_entry *
xfs_attr_sf_nextentry(struct xfs_attr_sf_entry *sfep)
{
	return (void *)sfep + xfs_attr_sf_entsize(sfep);
}

#endif	 

 
 
#ifndef __XFS_HEALTH_H__
#define __XFS_HEALTH_H__

 

struct xfs_mount;
struct xfs_perag;
struct xfs_inode;
struct xfs_fsop_geom;

 
#define XFS_SICK_FS_COUNTERS	(1 << 0)   
#define XFS_SICK_FS_UQUOTA	(1 << 1)   
#define XFS_SICK_FS_GQUOTA	(1 << 2)   
#define XFS_SICK_FS_PQUOTA	(1 << 3)   

 
#define XFS_SICK_RT_BITMAP	(1 << 0)   
#define XFS_SICK_RT_SUMMARY	(1 << 1)   

 
#define XFS_SICK_AG_SB		(1 << 0)   
#define XFS_SICK_AG_AGF		(1 << 1)   
#define XFS_SICK_AG_AGFL	(1 << 2)   
#define XFS_SICK_AG_AGI		(1 << 3)   
#define XFS_SICK_AG_BNOBT	(1 << 4)   
#define XFS_SICK_AG_CNTBT	(1 << 5)   
#define XFS_SICK_AG_INOBT	(1 << 6)   
#define XFS_SICK_AG_FINOBT	(1 << 7)   
#define XFS_SICK_AG_RMAPBT	(1 << 8)   
#define XFS_SICK_AG_REFCNTBT	(1 << 9)   

 
#define XFS_SICK_INO_CORE	(1 << 0)   
#define XFS_SICK_INO_BMBTD	(1 << 1)   
#define XFS_SICK_INO_BMBTA	(1 << 2)   
#define XFS_SICK_INO_BMBTC	(1 << 3)   
#define XFS_SICK_INO_DIR	(1 << 4)   
#define XFS_SICK_INO_XATTR	(1 << 5)   
#define XFS_SICK_INO_SYMLINK	(1 << 6)   
#define XFS_SICK_INO_PARENT	(1 << 7)   

 
#define XFS_SICK_FS_PRIMARY	(XFS_SICK_FS_COUNTERS | \
				 XFS_SICK_FS_UQUOTA | \
				 XFS_SICK_FS_GQUOTA | \
				 XFS_SICK_FS_PQUOTA)

#define XFS_SICK_RT_PRIMARY	(XFS_SICK_RT_BITMAP | \
				 XFS_SICK_RT_SUMMARY)

#define XFS_SICK_AG_PRIMARY	(XFS_SICK_AG_SB | \
				 XFS_SICK_AG_AGF | \
				 XFS_SICK_AG_AGFL | \
				 XFS_SICK_AG_AGI | \
				 XFS_SICK_AG_BNOBT | \
				 XFS_SICK_AG_CNTBT | \
				 XFS_SICK_AG_INOBT | \
				 XFS_SICK_AG_FINOBT | \
				 XFS_SICK_AG_RMAPBT | \
				 XFS_SICK_AG_REFCNTBT)

#define XFS_SICK_INO_PRIMARY	(XFS_SICK_INO_CORE | \
				 XFS_SICK_INO_BMBTD | \
				 XFS_SICK_INO_BMBTA | \
				 XFS_SICK_INO_BMBTC | \
				 XFS_SICK_INO_DIR | \
				 XFS_SICK_INO_XATTR | \
				 XFS_SICK_INO_SYMLINK | \
				 XFS_SICK_INO_PARENT)

 

void xfs_fs_mark_sick(struct xfs_mount *mp, unsigned int mask);
void xfs_fs_mark_healthy(struct xfs_mount *mp, unsigned int mask);
void xfs_fs_measure_sickness(struct xfs_mount *mp, unsigned int *sick,
		unsigned int *checked);

void xfs_rt_mark_sick(struct xfs_mount *mp, unsigned int mask);
void xfs_rt_mark_healthy(struct xfs_mount *mp, unsigned int mask);
void xfs_rt_measure_sickness(struct xfs_mount *mp, unsigned int *sick,
		unsigned int *checked);

void xfs_ag_mark_sick(struct xfs_perag *pag, unsigned int mask);
void xfs_ag_mark_healthy(struct xfs_perag *pag, unsigned int mask);
void xfs_ag_measure_sickness(struct xfs_perag *pag, unsigned int *sick,
		unsigned int *checked);

void xfs_inode_mark_sick(struct xfs_inode *ip, unsigned int mask);
void xfs_inode_mark_healthy(struct xfs_inode *ip, unsigned int mask);
void xfs_inode_measure_sickness(struct xfs_inode *ip, unsigned int *sick,
		unsigned int *checked);

void xfs_health_unmount(struct xfs_mount *mp);

 

static inline bool
xfs_fs_has_sickness(struct xfs_mount *mp, unsigned int mask)
{
	unsigned int	sick, checked;

	xfs_fs_measure_sickness(mp, &sick, &checked);
	return sick & mask;
}

static inline bool
xfs_rt_has_sickness(struct xfs_mount *mp, unsigned int mask)
{
	unsigned int	sick, checked;

	xfs_rt_measure_sickness(mp, &sick, &checked);
	return sick & mask;
}

static inline bool
xfs_ag_has_sickness(struct xfs_perag *pag, unsigned int mask)
{
	unsigned int	sick, checked;

	xfs_ag_measure_sickness(pag, &sick, &checked);
	return sick & mask;
}

static inline bool
xfs_inode_has_sickness(struct xfs_inode *ip, unsigned int mask)
{
	unsigned int	sick, checked;

	xfs_inode_measure_sickness(ip, &sick, &checked);
	return sick & mask;
}

static inline bool
xfs_fs_is_healthy(struct xfs_mount *mp)
{
	return !xfs_fs_has_sickness(mp, -1U);
}

static inline bool
xfs_rt_is_healthy(struct xfs_mount *mp)
{
	return !xfs_rt_has_sickness(mp, -1U);
}

static inline bool
xfs_ag_is_healthy(struct xfs_perag *pag)
{
	return !xfs_ag_has_sickness(pag, -1U);
}

static inline bool
xfs_inode_is_healthy(struct xfs_inode *ip)
{
	return !xfs_inode_has_sickness(ip, -1U);
}

void xfs_fsop_geom_health(struct xfs_mount *mp, struct xfs_fsop_geom *geo);
void xfs_ag_geom_health(struct xfs_perag *pag, struct xfs_ag_geometry *ageo);
void xfs_bulkstat_health(struct xfs_inode *ip, struct xfs_bulkstat *bs);

#endif	 

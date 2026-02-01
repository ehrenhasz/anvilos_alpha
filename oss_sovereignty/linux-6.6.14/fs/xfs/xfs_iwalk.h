 
 
#ifndef __XFS_IWALK_H__
#define __XFS_IWALK_H__

 

 
typedef int (*xfs_iwalk_fn)(struct xfs_mount *mp, struct xfs_trans *tp,
			    xfs_ino_t ino, void *data);

int xfs_iwalk(struct xfs_mount *mp, struct xfs_trans *tp, xfs_ino_t startino,
		unsigned int flags, xfs_iwalk_fn iwalk_fn,
		unsigned int inode_records, void *data);
int xfs_iwalk_threaded(struct xfs_mount *mp, xfs_ino_t startino,
		unsigned int flags, xfs_iwalk_fn iwalk_fn,
		unsigned int inode_records, bool poll, void *data);

 
#define XFS_IWALK_SAME_AG	(1U << 0)

#define XFS_IWALK_FLAGS_ALL	(XFS_IWALK_SAME_AG)

 
typedef int (*xfs_inobt_walk_fn)(struct xfs_mount *mp, struct xfs_trans *tp,
				 xfs_agnumber_t agno,
				 const struct xfs_inobt_rec_incore *irec,
				 void *data);

int xfs_inobt_walk(struct xfs_mount *mp, struct xfs_trans *tp,
		xfs_ino_t startino, unsigned int flags,
		xfs_inobt_walk_fn inobt_walk_fn, unsigned int inobt_records,
		void *data);

 
#define XFS_INOBT_WALK_SAME_AG	(XFS_IWALK_SAME_AG)

#define XFS_INOBT_WALK_FLAGS_ALL (XFS_INOBT_WALK_SAME_AG)

#endif  

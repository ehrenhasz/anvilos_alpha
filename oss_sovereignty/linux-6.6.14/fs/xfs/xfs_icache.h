#ifndef XFS_SYNC_H
#define XFS_SYNC_H 1
struct xfs_mount;
struct xfs_perag;
struct xfs_icwalk {
	__u32		icw_flags;
	kuid_t		icw_uid;
	kgid_t		icw_gid;
	prid_t		icw_prid;
	__u64		icw_min_file_size;
	long		icw_scan_limit;
};
#define XFS_ICWALK_FLAG_SYNC		(1U << 0)  
#define XFS_ICWALK_FLAG_UID		(1U << 1)  
#define XFS_ICWALK_FLAG_GID		(1U << 2)  
#define XFS_ICWALK_FLAG_PRID		(1U << 3)  
#define XFS_ICWALK_FLAG_MINFILESIZE	(1U << 4)  
#define XFS_ICWALK_FLAGS_VALID		(XFS_ICWALK_FLAG_SYNC | \
					 XFS_ICWALK_FLAG_UID | \
					 XFS_ICWALK_FLAG_GID | \
					 XFS_ICWALK_FLAG_PRID | \
					 XFS_ICWALK_FLAG_MINFILESIZE)
#define XFS_IGET_CREATE		(1U << 0)
#define XFS_IGET_UNTRUSTED	(1U << 1)
#define XFS_IGET_DONTCACHE	(1U << 2)
#define XFS_IGET_INCORE		(1U << 3)
#define XFS_IGET_NORETRY	(1U << 4)
int xfs_iget(struct xfs_mount *mp, struct xfs_trans *tp, xfs_ino_t ino,
	     uint flags, uint lock_flags, xfs_inode_t **ipp);
struct xfs_inode * xfs_inode_alloc(struct xfs_mount *mp, xfs_ino_t ino);
void xfs_inode_free(struct xfs_inode *ip);
void xfs_reclaim_worker(struct work_struct *work);
void xfs_reclaim_inodes(struct xfs_mount *mp);
long xfs_reclaim_inodes_count(struct xfs_mount *mp);
long xfs_reclaim_inodes_nr(struct xfs_mount *mp, unsigned long nr_to_scan);
void xfs_inode_mark_reclaimable(struct xfs_inode *ip);
int xfs_blockgc_free_dquots(struct xfs_mount *mp, struct xfs_dquot *udqp,
		struct xfs_dquot *gdqp, struct xfs_dquot *pdqp,
		unsigned int iwalk_flags);
int xfs_blockgc_free_quota(struct xfs_inode *ip, unsigned int iwalk_flags);
int xfs_blockgc_free_space(struct xfs_mount *mp, struct xfs_icwalk *icm);
int xfs_blockgc_flush_all(struct xfs_mount *mp);
void xfs_inode_set_eofblocks_tag(struct xfs_inode *ip);
void xfs_inode_clear_eofblocks_tag(struct xfs_inode *ip);
void xfs_inode_set_cowblocks_tag(struct xfs_inode *ip);
void xfs_inode_clear_cowblocks_tag(struct xfs_inode *ip);
void xfs_blockgc_worker(struct work_struct *work);
void xfs_blockgc_stop(struct xfs_mount *mp);
void xfs_blockgc_start(struct xfs_mount *mp);
void xfs_inodegc_worker(struct work_struct *work);
void xfs_inodegc_push(struct xfs_mount *mp);
int xfs_inodegc_flush(struct xfs_mount *mp);
void xfs_inodegc_stop(struct xfs_mount *mp);
void xfs_inodegc_start(struct xfs_mount *mp);
int xfs_inodegc_register_shrinker(struct xfs_mount *mp);
#endif

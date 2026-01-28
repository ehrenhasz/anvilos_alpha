

#ifndef	__XFS_INODE_ITEM_H__
#define	__XFS_INODE_ITEM_H__



struct xfs_buf;
struct xfs_bmbt_rec;
struct xfs_inode;
struct xfs_mount;

struct xfs_inode_log_item {
	struct xfs_log_item	ili_item;	   
	struct xfs_inode	*ili_inode;	   
	unsigned short		ili_lock_flags;	   
	unsigned int		ili_dirty_flags;   
	
	spinlock_t		ili_lock;	   
	unsigned int		ili_last_fields;   
	unsigned int		ili_fields;	   
	unsigned int		ili_fsync_fields;  
	xfs_lsn_t		ili_flush_lsn;	   
	xfs_csn_t		ili_commit_seq;	   
};

static inline int xfs_inode_clean(struct xfs_inode *ip)
{
	return !ip->i_itemp || !(ip->i_itemp->ili_fields & XFS_ILOG_ALL);
}

extern void xfs_inode_item_init(struct xfs_inode *, struct xfs_mount *);
extern void xfs_inode_item_destroy(struct xfs_inode *);
extern void xfs_iflush_abort(struct xfs_inode *);
extern void xfs_iflush_shutdown_abort(struct xfs_inode *);
extern int xfs_inode_item_format_convert(xfs_log_iovec_t *,
					 struct xfs_inode_log_format *);

extern struct kmem_cache	*xfs_ili_cache;

#endif	

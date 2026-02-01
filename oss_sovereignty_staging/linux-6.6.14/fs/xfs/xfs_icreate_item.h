
 
#ifndef XFS_ICREATE_ITEM_H
#define XFS_ICREATE_ITEM_H	1

 
struct xfs_icreate_item {
	struct xfs_log_item	ic_item;
	struct xfs_icreate_log	ic_format;
};

extern struct kmem_cache *xfs_icreate_cache;	 

void xfs_icreate_log(struct xfs_trans *tp, xfs_agnumber_t agno,
			xfs_agblock_t agbno, unsigned int count,
			unsigned int inode_size, xfs_agblock_t length,
			unsigned int generation);

#endif	 

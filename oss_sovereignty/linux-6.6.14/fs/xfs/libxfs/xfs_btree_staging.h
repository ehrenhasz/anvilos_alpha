 
 
#ifndef __XFS_BTREE_STAGING_H__
#define __XFS_BTREE_STAGING_H__

 
struct xbtree_afakeroot {
	 
	xfs_agblock_t		af_root;

	 
	unsigned int		af_levels;

	 
	unsigned int		af_blocks;
};

 
void xfs_btree_stage_afakeroot(struct xfs_btree_cur *cur,
		struct xbtree_afakeroot *afake);
void xfs_btree_commit_afakeroot(struct xfs_btree_cur *cur, struct xfs_trans *tp,
		struct xfs_buf *agbp, const struct xfs_btree_ops *ops);

 
struct xbtree_ifakeroot {
	 
	struct xfs_ifork	*if_fork;

	 
	int64_t			if_blocks;

	 
	unsigned int		if_levels;

	 
	unsigned int		if_fork_size;

	 
	unsigned int		if_format;

	 
	unsigned int		if_extents;
};

 
void xfs_btree_stage_ifakeroot(struct xfs_btree_cur *cur,
		struct xbtree_ifakeroot *ifake,
		struct xfs_btree_ops **new_ops);
void xfs_btree_commit_ifakeroot(struct xfs_btree_cur *cur, struct xfs_trans *tp,
		int whichfork, const struct xfs_btree_ops *ops);

 
typedef int (*xfs_btree_bload_get_record_fn)(struct xfs_btree_cur *cur, void *priv);
typedef int (*xfs_btree_bload_claim_block_fn)(struct xfs_btree_cur *cur,
		union xfs_btree_ptr *ptr, void *priv);
typedef size_t (*xfs_btree_bload_iroot_size_fn)(struct xfs_btree_cur *cur,
		unsigned int nr_this_level, void *priv);

struct xfs_btree_bload {
	 
	xfs_btree_bload_get_record_fn	get_record;

	 
	xfs_btree_bload_claim_block_fn	claim_block;

	 
	xfs_btree_bload_iroot_size_fn	iroot_size;

	 
	uint64_t			nr_records;

	 
	int				leaf_slack;

	 
	int				node_slack;

	 
	uint64_t			nr_blocks;

	 
	unsigned int			btree_height;
};

int xfs_btree_bload_compute_geometry(struct xfs_btree_cur *cur,
		struct xfs_btree_bload *bbl, uint64_t nr_records);
int xfs_btree_bload(struct xfs_btree_cur *cur, struct xfs_btree_bload *bbl,
		void *priv);

#endif	 

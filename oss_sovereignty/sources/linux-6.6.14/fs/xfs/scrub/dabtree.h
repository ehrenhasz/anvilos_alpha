

#ifndef __XFS_SCRUB_DABTREE_H__
#define __XFS_SCRUB_DABTREE_H__



struct xchk_da_btree {
	struct xfs_da_args	dargs;
	xfs_dahash_t		hashes[XFS_DA_NODE_MAXDEPTH];
	int			maxrecs[XFS_DA_NODE_MAXDEPTH];
	struct xfs_da_state	*state;
	struct xfs_scrub	*sc;
	void			*private;

	
	xfs_dablk_t		lowest;
	xfs_dablk_t		highest;

	int			tree_level;
};

typedef int (*xchk_da_btree_rec_fn)(struct xchk_da_btree *ds, int level);


bool xchk_da_process_error(struct xchk_da_btree *ds, int level, int *error);


void xchk_da_set_corrupt(struct xchk_da_btree *ds, int level);

int xchk_da_btree_hash(struct xchk_da_btree *ds, int level, __be32 *hashp);
int xchk_da_btree(struct xfs_scrub *sc, int whichfork,
		xchk_da_btree_rec_fn scrub_fn, void *private);

#endif 

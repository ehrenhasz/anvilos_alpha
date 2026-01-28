

#ifndef __XFS_SCRUB_BTREE_H__
#define __XFS_SCRUB_BTREE_H__




bool xchk_btree_process_error(struct xfs_scrub *sc,
		struct xfs_btree_cur *cur, int level, int *error);


bool xchk_btree_xref_process_error(struct xfs_scrub *sc,
		struct xfs_btree_cur *cur, int level, int *error);


void xchk_btree_set_corrupt(struct xfs_scrub *sc,
		struct xfs_btree_cur *cur, int level);
void xchk_btree_set_preen(struct xfs_scrub *sc, struct xfs_btree_cur *cur,
		int level);


void xchk_btree_xref_set_corrupt(struct xfs_scrub *sc,
		struct xfs_btree_cur *cur, int level);

struct xchk_btree;
typedef int (*xchk_btree_rec_fn)(
	struct xchk_btree		*bs,
	const union xfs_btree_rec	*rec);

struct xchk_btree_key {
	union xfs_btree_key		key;
	bool				valid;
};

struct xchk_btree {
	
	struct xfs_scrub		*sc;
	struct xfs_btree_cur		*cur;
	xchk_btree_rec_fn		scrub_rec;
	const struct xfs_owner_info	*oinfo;
	void				*private;

	
	bool				lastrec_valid;
	union xfs_btree_rec		lastrec;
	struct list_head		to_check;

	
	struct xchk_btree_key		lastkey[];
};


static inline size_t
xchk_btree_sizeof(unsigned int nlevels)
{
	return struct_size_t(struct xchk_btree, lastkey, nlevels - 1);
}

int xchk_btree(struct xfs_scrub *sc, struct xfs_btree_cur *cur,
		xchk_btree_rec_fn scrub_fn, const struct xfs_owner_info *oinfo,
		void *private);

#endif 

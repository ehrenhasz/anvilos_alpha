
 
#ifndef __XFS_REFCOUNT_H__
#define __XFS_REFCOUNT_H__

struct xfs_trans;
struct xfs_mount;
struct xfs_perag;
struct xfs_btree_cur;
struct xfs_bmbt_irec;
struct xfs_refcount_irec;

extern int xfs_refcount_lookup_le(struct xfs_btree_cur *cur,
		enum xfs_refc_domain domain, xfs_agblock_t bno, int *stat);
extern int xfs_refcount_lookup_ge(struct xfs_btree_cur *cur,
		enum xfs_refc_domain domain, xfs_agblock_t bno, int *stat);
extern int xfs_refcount_lookup_eq(struct xfs_btree_cur *cur,
		enum xfs_refc_domain domain, xfs_agblock_t bno, int *stat);
extern int xfs_refcount_get_rec(struct xfs_btree_cur *cur,
		struct xfs_refcount_irec *irec, int *stat);

static inline uint32_t
xfs_refcount_encode_startblock(
	xfs_agblock_t		startblock,
	enum xfs_refc_domain	domain)
{
	uint32_t		start;

	 
	start = startblock & ~XFS_REFC_COWFLAG;
	if (domain != XFS_REFC_DOMAIN_SHARED)
		start |= XFS_REFC_COWFLAG;

	return start;
}

enum xfs_refcount_intent_type {
	XFS_REFCOUNT_INCREASE = 1,
	XFS_REFCOUNT_DECREASE,
	XFS_REFCOUNT_ALLOC_COW,
	XFS_REFCOUNT_FREE_COW,
};

struct xfs_refcount_intent {
	struct list_head			ri_list;
	struct xfs_perag			*ri_pag;
	enum xfs_refcount_intent_type		ri_type;
	xfs_extlen_t				ri_blockcount;
	xfs_fsblock_t				ri_startblock;
};

 
static inline bool
xfs_refcount_check_domain(
	const struct xfs_refcount_irec	*irec)
{
	if (irec->rc_domain == XFS_REFC_DOMAIN_COW && irec->rc_refcount != 1)
		return false;
	if (irec->rc_domain == XFS_REFC_DOMAIN_SHARED && irec->rc_refcount < 2)
		return false;
	return true;
}

void xfs_refcount_update_get_group(struct xfs_mount *mp,
		struct xfs_refcount_intent *ri);

void xfs_refcount_increase_extent(struct xfs_trans *tp,
		struct xfs_bmbt_irec *irec);
void xfs_refcount_decrease_extent(struct xfs_trans *tp,
		struct xfs_bmbt_irec *irec);

extern void xfs_refcount_finish_one_cleanup(struct xfs_trans *tp,
		struct xfs_btree_cur *rcur, int error);
extern int xfs_refcount_finish_one(struct xfs_trans *tp,
		struct xfs_refcount_intent *ri, struct xfs_btree_cur **pcur);

extern int xfs_refcount_find_shared(struct xfs_btree_cur *cur,
		xfs_agblock_t agbno, xfs_extlen_t aglen, xfs_agblock_t *fbno,
		xfs_extlen_t *flen, bool find_end_of_shared);

void xfs_refcount_alloc_cow_extent(struct xfs_trans *tp, xfs_fsblock_t fsb,
		xfs_extlen_t len);
void xfs_refcount_free_cow_extent(struct xfs_trans *tp, xfs_fsblock_t fsb,
		xfs_extlen_t len);
extern int xfs_refcount_recover_cow_leftovers(struct xfs_mount *mp,
		struct xfs_perag *pag);

 
#define XFS_REFCOUNT_ITEM_OVERHEAD	32

extern int xfs_refcount_has_records(struct xfs_btree_cur *cur,
		enum xfs_refc_domain domain, xfs_agblock_t bno,
		xfs_extlen_t len, enum xbtree_recpacking *outcome);
union xfs_btree_rec;
extern void xfs_refcount_btrec_to_irec(const union xfs_btree_rec *rec,
		struct xfs_refcount_irec *irec);
xfs_failaddr_t xfs_refcount_check_irec(struct xfs_btree_cur *cur,
		const struct xfs_refcount_irec *irec);
extern int xfs_refcount_insert(struct xfs_btree_cur *cur,
		struct xfs_refcount_irec *irec, int *stat);

extern struct kmem_cache	*xfs_refcount_intent_cache;

int __init xfs_refcount_intent_init_cache(void);
void xfs_refcount_intent_destroy_cache(void);

#endif	 

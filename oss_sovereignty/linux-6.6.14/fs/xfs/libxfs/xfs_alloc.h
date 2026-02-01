 
 
#ifndef __XFS_ALLOC_H__
#define	__XFS_ALLOC_H__

struct xfs_buf;
struct xfs_btree_cur;
struct xfs_mount;
struct xfs_perag;
struct xfs_trans;

extern struct workqueue_struct *xfs_alloc_wq;

unsigned int xfs_agfl_size(struct xfs_mount *mp);

 
#define	XFS_ALLOC_FLAG_TRYLOCK	(1U << 0)   
#define	XFS_ALLOC_FLAG_FREEING	(1U << 1)   
#define	XFS_ALLOC_FLAG_NORMAP	(1U << 2)   
#define	XFS_ALLOC_FLAG_NOSHRINK	(1U << 3)   
#define	XFS_ALLOC_FLAG_CHECK	(1U << 4)   
#define	XFS_ALLOC_FLAG_TRYFLUSH	(1U << 5)   

 
typedef struct xfs_alloc_arg {
	struct xfs_trans *tp;		 
	struct xfs_mount *mp;		 
	struct xfs_buf	*agbp;		 
	struct xfs_perag *pag;		 
	xfs_fsblock_t	fsbno;		 
	xfs_agnumber_t	agno;		 
	xfs_agblock_t	agbno;		 
	xfs_extlen_t	minlen;		 
	xfs_extlen_t	maxlen;		 
	xfs_extlen_t	mod;		 
	xfs_extlen_t	prod;		 
	xfs_extlen_t	minleft;	 
	xfs_extlen_t	total;		 
	xfs_extlen_t	alignment;	 
	xfs_extlen_t	minalignslop;	 
	xfs_agblock_t	min_agbno;	 
	xfs_agblock_t	max_agbno;	 
	xfs_extlen_t	len;		 
	int		datatype;	 
	char		wasdel;		 
	char		wasfromfl;	 
	struct xfs_owner_info	oinfo;	 
	enum xfs_ag_resv_type	resv;	 
#ifdef DEBUG
	bool		alloc_minlen_only;  
#endif
} xfs_alloc_arg_t;

 
#define XFS_ALLOC_USERDATA		(1 << 0) 
#define XFS_ALLOC_INITIAL_USER_DATA	(1 << 1) 
#define XFS_ALLOC_NOBUSY		(1 << 2) 

 
unsigned int xfs_alloc_set_aside(struct xfs_mount *mp);
unsigned int xfs_alloc_ag_max_usable(struct xfs_mount *mp);

xfs_extlen_t xfs_alloc_longest_free_extent(struct xfs_perag *pag,
		xfs_extlen_t need, xfs_extlen_t reserved);
unsigned int xfs_alloc_min_freelist(struct xfs_mount *mp,
		struct xfs_perag *pag);
int xfs_alloc_get_freelist(struct xfs_perag *pag, struct xfs_trans *tp,
		struct xfs_buf *agfbp, xfs_agblock_t *bnop, int	 btreeblk);
int xfs_alloc_put_freelist(struct xfs_perag *pag, struct xfs_trans *tp,
		struct xfs_buf *agfbp, struct xfs_buf *agflbp,
		xfs_agblock_t bno, int btreeblk);

 
void
xfs_alloc_compute_maxlevels(
	struct xfs_mount	*mp);	 

 
void
xfs_alloc_log_agf(
	struct xfs_trans *tp,	 
	struct xfs_buf	*bp,	 
	uint32_t	fields); 

 
int xfs_alloc_vextent_this_ag(struct xfs_alloc_arg *args, xfs_agnumber_t agno);

 
int xfs_alloc_vextent_near_bno(struct xfs_alloc_arg *args,
		xfs_fsblock_t target);

 
int xfs_alloc_vextent_exact_bno(struct xfs_alloc_arg *args,
		xfs_fsblock_t target);

 
int xfs_alloc_vextent_start_ag(struct xfs_alloc_arg *args,
		xfs_fsblock_t target);

 
int xfs_alloc_vextent_first_ag(struct xfs_alloc_arg *args,
		xfs_fsblock_t target);

 
int				 
__xfs_free_extent(
	struct xfs_trans	*tp,	 
	struct xfs_perag	*pag,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len,	 
	const struct xfs_owner_info	*oinfo,	 
	enum xfs_ag_resv_type	type,	 
	bool			skip_discard);

static inline int
xfs_free_extent(
	struct xfs_trans	*tp,
	struct xfs_perag	*pag,
	xfs_agblock_t		agbno,
	xfs_extlen_t		len,
	const struct xfs_owner_info	*oinfo,
	enum xfs_ag_resv_type	type)
{
	return __xfs_free_extent(tp, pag, agbno, len, oinfo, type, false);
}

int				 
xfs_alloc_lookup_le(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		bno,	 
	xfs_extlen_t		len,	 
	int			*stat);	 

int				 
xfs_alloc_lookup_ge(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		bno,	 
	xfs_extlen_t		len,	 
	int			*stat);	 

int					 
xfs_alloc_get_rec(
	struct xfs_btree_cur	*cur,	 
	xfs_agblock_t		*bno,	 
	xfs_extlen_t		*len,	 
	int			*stat);	 

union xfs_btree_rec;
void xfs_alloc_btrec_to_irec(const union xfs_btree_rec *rec,
		struct xfs_alloc_rec_incore *irec);
xfs_failaddr_t xfs_alloc_check_irec(struct xfs_btree_cur *cur,
		const struct xfs_alloc_rec_incore *irec);

int xfs_read_agf(struct xfs_perag *pag, struct xfs_trans *tp, int flags,
		struct xfs_buf **agfbpp);
int xfs_alloc_read_agf(struct xfs_perag *pag, struct xfs_trans *tp, int flags,
		struct xfs_buf **agfbpp);
int xfs_alloc_read_agfl(struct xfs_perag *pag, struct xfs_trans *tp,
		struct xfs_buf **bpp);
int xfs_free_agfl_block(struct xfs_trans *, xfs_agnumber_t, xfs_agblock_t,
			struct xfs_buf *, struct xfs_owner_info *);
int xfs_alloc_fix_freelist(struct xfs_alloc_arg *args, uint32_t alloc_flags);
int xfs_free_extent_fix_freelist(struct xfs_trans *tp, struct xfs_perag *pag,
		struct xfs_buf **agbp);

xfs_extlen_t xfs_prealloc_blocks(struct xfs_mount *mp);

typedef int (*xfs_alloc_query_range_fn)(
	struct xfs_btree_cur			*cur,
	const struct xfs_alloc_rec_incore	*rec,
	void					*priv);

int xfs_alloc_query_range(struct xfs_btree_cur *cur,
		const struct xfs_alloc_rec_incore *low_rec,
		const struct xfs_alloc_rec_incore *high_rec,
		xfs_alloc_query_range_fn fn, void *priv);
int xfs_alloc_query_all(struct xfs_btree_cur *cur, xfs_alloc_query_range_fn fn,
		void *priv);

int xfs_alloc_has_records(struct xfs_btree_cur *cur, xfs_agblock_t bno,
		xfs_extlen_t len, enum xbtree_recpacking *outcome);

typedef int (*xfs_agfl_walk_fn)(struct xfs_mount *mp, xfs_agblock_t bno,
		void *priv);
int xfs_agfl_walk(struct xfs_mount *mp, struct xfs_agf *agf,
		struct xfs_buf *agflbp, xfs_agfl_walk_fn walk_fn, void *priv);

static inline __be32 *
xfs_buf_to_agfl_bno(
	struct xfs_buf		*bp)
{
	if (xfs_has_crc(bp->b_mount))
		return bp->b_addr + sizeof(struct xfs_agfl);
	return bp->b_addr;
}

int __xfs_free_extent_later(struct xfs_trans *tp, xfs_fsblock_t bno,
		xfs_filblks_t len, const struct xfs_owner_info *oinfo,
		enum xfs_ag_resv_type type, bool skip_discard);

 
struct xfs_extent_free_item {
	struct list_head	xefi_list;
	uint64_t		xefi_owner;
	xfs_fsblock_t		xefi_startblock; 
	xfs_extlen_t		xefi_blockcount; 
	struct xfs_perag	*xefi_pag;
	unsigned int		xefi_flags;
	enum xfs_ag_resv_type	xefi_agresv;
};

void xfs_extent_free_get_group(struct xfs_mount *mp,
		struct xfs_extent_free_item *xefi);

#define XFS_EFI_SKIP_DISCARD	(1U << 0)  
#define XFS_EFI_ATTR_FORK	(1U << 1)  
#define XFS_EFI_BMBT_BLOCK	(1U << 2)  

static inline int
xfs_free_extent_later(
	struct xfs_trans		*tp,
	xfs_fsblock_t			bno,
	xfs_filblks_t			len,
	const struct xfs_owner_info	*oinfo,
	enum xfs_ag_resv_type		type)
{
	return __xfs_free_extent_later(tp, bno, len, oinfo, type, false);
}


extern struct kmem_cache	*xfs_extfree_item_cache;

int __init xfs_extfree_intent_init_cache(void);
void xfs_extfree_intent_destroy_cache(void);

xfs_failaddr_t xfs_validate_ag_length(struct xfs_buf *bp, uint32_t seqno,
		uint32_t length);

#endif	 

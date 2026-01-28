#ifndef __LIBXFS_AG_H
#define __LIBXFS_AG_H 1
struct xfs_mount;
struct xfs_trans;
struct xfs_perag;
struct xfs_ag_resv {
	xfs_extlen_t			ar_orig_reserved;
	xfs_extlen_t			ar_reserved;
	xfs_extlen_t			ar_asked;
};
struct xfs_perag {
	struct xfs_mount *pag_mount;	 
	xfs_agnumber_t	pag_agno;	 
	atomic_t	pag_ref;	 
	atomic_t	pag_active_ref;	 
	wait_queue_head_t pag_active_wq; 
	unsigned long	pag_opstate;
	uint8_t		pagf_levels[XFS_BTNUM_AGF];
	uint32_t	pagf_flcount;	 
	xfs_extlen_t	pagf_freeblks;	 
	xfs_extlen_t	pagf_longest;	 
	uint32_t	pagf_btreeblks;	 
	xfs_agino_t	pagi_freecount;	 
	xfs_agino_t	pagi_count;	 
	xfs_agino_t	pagl_pagino;
	xfs_agino_t	pagl_leftrec;
	xfs_agino_t	pagl_rightrec;
	int		pagb_count;	 
	uint8_t		pagf_refcount_level;  
	struct xfs_ag_resv	pag_meta_resv;
	struct xfs_ag_resv	pag_rmapbt_resv;
	struct rcu_head	rcu_head;
	xfs_agblock_t		block_count;
	xfs_agblock_t		min_block;
	xfs_agino_t		agino_min;
	xfs_agino_t		agino_max;
#ifdef __KERNEL__
	uint16_t	pag_checked;
	uint16_t	pag_sick;
	spinlock_t	pag_state_lock;
	spinlock_t	pagb_lock;	 
	struct rb_root	pagb_tree;	 
	unsigned int	pagb_gen;	 
	wait_queue_head_t pagb_wait;	 
	atomic_t        pagf_fstrms;     
	spinlock_t	pag_ici_lock;	 
	struct radix_tree_root pag_ici_root;	 
	int		pag_ici_reclaimable;	 
	unsigned long	pag_ici_reclaim_cursor;	 
	spinlock_t	pag_buf_lock;	 
	struct rhashtable pag_buf_hash;
	struct delayed_work	pag_blockgc_work;
	struct xfs_defer_drain	pag_intents_drain;
#endif  
};
#define XFS_AGSTATE_AGF_INIT		0
#define XFS_AGSTATE_AGI_INIT		1
#define XFS_AGSTATE_PREFERS_METADATA	2
#define XFS_AGSTATE_ALLOWS_INODES	3
#define XFS_AGSTATE_AGFL_NEEDS_RESET	4
#define __XFS_AG_OPSTATE(name, NAME) \
static inline bool xfs_perag_ ## name (struct xfs_perag *pag) \
{ \
	return test_bit(XFS_AGSTATE_ ## NAME, &pag->pag_opstate); \
}
__XFS_AG_OPSTATE(initialised_agf, AGF_INIT)
__XFS_AG_OPSTATE(initialised_agi, AGI_INIT)
__XFS_AG_OPSTATE(prefers_metadata, PREFERS_METADATA)
__XFS_AG_OPSTATE(allows_inodes, ALLOWS_INODES)
__XFS_AG_OPSTATE(agfl_needs_reset, AGFL_NEEDS_RESET)
int xfs_initialize_perag(struct xfs_mount *mp, xfs_agnumber_t agcount,
			xfs_rfsblock_t dcount, xfs_agnumber_t *maxagi);
int xfs_initialize_perag_data(struct xfs_mount *mp, xfs_agnumber_t agno);
void xfs_free_perag(struct xfs_mount *mp);
struct xfs_perag *xfs_perag_get(struct xfs_mount *mp, xfs_agnumber_t agno);
struct xfs_perag *xfs_perag_get_tag(struct xfs_mount *mp, xfs_agnumber_t agno,
		unsigned int tag);
struct xfs_perag *xfs_perag_hold(struct xfs_perag *pag);
void xfs_perag_put(struct xfs_perag *pag);
struct xfs_perag *xfs_perag_grab(struct xfs_mount *, xfs_agnumber_t);
struct xfs_perag *xfs_perag_grab_tag(struct xfs_mount *, xfs_agnumber_t,
				   int tag);
void xfs_perag_rele(struct xfs_perag *pag);
xfs_agblock_t xfs_ag_block_count(struct xfs_mount *mp, xfs_agnumber_t agno);
void xfs_agino_range(struct xfs_mount *mp, xfs_agnumber_t agno,
		xfs_agino_t *first, xfs_agino_t *last);
static inline bool
xfs_verify_agbno(struct xfs_perag *pag, xfs_agblock_t agbno)
{
	if (agbno >= pag->block_count)
		return false;
	if (agbno <= pag->min_block)
		return false;
	return true;
}
static inline bool
xfs_verify_agbext(
	struct xfs_perag	*pag,
	xfs_agblock_t		agbno,
	xfs_agblock_t		len)
{
	if (agbno + len <= agbno)
		return false;
	if (!xfs_verify_agbno(pag, agbno))
		return false;
	return xfs_verify_agbno(pag, agbno + len - 1);
}
static inline bool
xfs_verify_agino(struct xfs_perag *pag, xfs_agino_t agino)
{
	if (agino < pag->agino_min)
		return false;
	if (agino > pag->agino_max)
		return false;
	return true;
}
static inline bool
xfs_verify_agino_or_null(struct xfs_perag *pag, xfs_agino_t agino)
{
	if (agino == NULLAGINO)
		return true;
	return xfs_verify_agino(pag, agino);
}
static inline bool
xfs_ag_contains_log(struct xfs_mount *mp, xfs_agnumber_t agno)
{
	return mp->m_sb.sb_logstart > 0 &&
	       agno == XFS_FSB_TO_AGNO(mp, mp->m_sb.sb_logstart);
}
static inline struct xfs_perag *
xfs_perag_next(
	struct xfs_perag	*pag,
	xfs_agnumber_t		*agno,
	xfs_agnumber_t		end_agno)
{
	struct xfs_mount	*mp = pag->pag_mount;
	*agno = pag->pag_agno + 1;
	xfs_perag_rele(pag);
	while (*agno <= end_agno) {
		pag = xfs_perag_grab(mp, *agno);
		if (pag)
			return pag;
		(*agno)++;
	}
	return NULL;
}
#define for_each_perag_range(mp, agno, end_agno, pag) \
	for ((pag) = xfs_perag_grab((mp), (agno)); \
		(pag) != NULL; \
		(pag) = xfs_perag_next((pag), &(agno), (end_agno)))
#define for_each_perag_from(mp, agno, pag) \
	for_each_perag_range((mp), (agno), (mp)->m_sb.sb_agcount - 1, (pag))
#define for_each_perag(mp, agno, pag) \
	(agno) = 0; \
	for_each_perag_from((mp), (agno), (pag))
#define for_each_perag_tag(mp, agno, pag, tag) \
	for ((agno) = 0, (pag) = xfs_perag_grab_tag((mp), 0, (tag)); \
		(pag) != NULL; \
		(agno) = (pag)->pag_agno + 1, \
		xfs_perag_rele(pag), \
		(pag) = xfs_perag_grab_tag((mp), (agno), (tag)))
static inline struct xfs_perag *
xfs_perag_next_wrap(
	struct xfs_perag	*pag,
	xfs_agnumber_t		*agno,
	xfs_agnumber_t		stop_agno,
	xfs_agnumber_t		restart_agno,
	xfs_agnumber_t		wrap_agno)
{
	struct xfs_mount	*mp = pag->pag_mount;
	*agno = pag->pag_agno + 1;
	xfs_perag_rele(pag);
	while (*agno != stop_agno) {
		if (*agno >= wrap_agno) {
			if (restart_agno >= stop_agno)
				break;
			*agno = restart_agno;
		}
		pag = xfs_perag_grab(mp, *agno);
		if (pag)
			return pag;
		(*agno)++;
	}
	return NULL;
}
#define for_each_perag_wrap_range(mp, start_agno, restart_agno, wrap_agno, agno, pag) \
	for ((agno) = (start_agno), (pag) = xfs_perag_grab((mp), (agno)); \
		(pag) != NULL; \
		(pag) = xfs_perag_next_wrap((pag), &(agno), (start_agno), \
				(restart_agno), (wrap_agno)))
#define for_each_perag_wrap_at(mp, start_agno, wrap_agno, agno, pag) \
	for_each_perag_wrap_range((mp), (start_agno), 0, (wrap_agno), (agno), (pag))
#define for_each_perag_wrap(mp, start_agno, agno, pag) \
	for_each_perag_wrap_at((mp), (start_agno), (mp)->m_sb.sb_agcount, \
				(agno), (pag))
struct aghdr_init_data {
	xfs_agblock_t		agno;		 
	xfs_extlen_t		agsize;		 
	struct list_head	buffer_list;	 
	xfs_rfsblock_t		nfree;		 
	xfs_daddr_t		daddr;		 
	size_t			numblks;	 
	xfs_btnum_t		type;		 
};
int xfs_ag_init_headers(struct xfs_mount *mp, struct aghdr_init_data *id);
int xfs_ag_shrink_space(struct xfs_perag *pag, struct xfs_trans **tpp,
			xfs_extlen_t delta);
int xfs_ag_extend_space(struct xfs_perag *pag, struct xfs_trans *tp,
			xfs_extlen_t len);
int xfs_ag_get_geometry(struct xfs_perag *pag, struct xfs_ag_geometry *ageo);
#endif  

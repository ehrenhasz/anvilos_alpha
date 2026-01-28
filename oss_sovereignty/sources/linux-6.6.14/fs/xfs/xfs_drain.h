

#ifndef XFS_DRAIN_H_
#define XFS_DRAIN_H_

struct xfs_perag;

#ifdef CONFIG_XFS_DRAIN_INTENTS

struct xfs_defer_drain {
	
	atomic_t		dr_count;

	
	struct wait_queue_head	dr_waiters;
};

void xfs_defer_drain_init(struct xfs_defer_drain *dr);
void xfs_defer_drain_free(struct xfs_defer_drain *dr);

void xfs_drain_wait_disable(void);
void xfs_drain_wait_enable(void);


struct xfs_perag *xfs_perag_intent_get(struct xfs_mount *mp,
		xfs_agnumber_t agno);
void xfs_perag_intent_put(struct xfs_perag *pag);

void xfs_perag_intent_hold(struct xfs_perag *pag);
void xfs_perag_intent_rele(struct xfs_perag *pag);

int xfs_perag_intent_drain(struct xfs_perag *pag);
bool xfs_perag_intent_busy(struct xfs_perag *pag);
#else
struct xfs_defer_drain {  };

#define xfs_defer_drain_free(dr)		((void)0)
#define xfs_defer_drain_init(dr)		((void)0)

#define xfs_perag_intent_get(mp, agno)		xfs_perag_get((mp), (agno))
#define xfs_perag_intent_put(pag)		xfs_perag_put(pag)

static inline void xfs_perag_intent_hold(struct xfs_perag *pag) { }
static inline void xfs_perag_intent_rele(struct xfs_perag *pag) { }

#endif 

#endif 


 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_ag.h"
#include "xfs_trace.h"

 
static DEFINE_STATIC_KEY_FALSE(xfs_drain_waiter_gate);

void
xfs_drain_wait_disable(void)
{
	static_branch_dec(&xfs_drain_waiter_gate);
}

void
xfs_drain_wait_enable(void)
{
	static_branch_inc(&xfs_drain_waiter_gate);
}

void
xfs_defer_drain_init(
	struct xfs_defer_drain	*dr)
{
	atomic_set(&dr->dr_count, 0);
	init_waitqueue_head(&dr->dr_waiters);
}

void
xfs_defer_drain_free(struct xfs_defer_drain	*dr)
{
	ASSERT(atomic_read(&dr->dr_count) == 0);
}

 
static inline void xfs_defer_drain_grab(struct xfs_defer_drain *dr)
{
	atomic_inc(&dr->dr_count);
}

static inline bool has_waiters(struct wait_queue_head *wq_head)
{
	 
	smp_mb__after_atomic();
	return waitqueue_active(wq_head);
}

 
static inline void xfs_defer_drain_rele(struct xfs_defer_drain *dr)
{
	if (atomic_dec_and_test(&dr->dr_count) &&
	    static_branch_unlikely(&xfs_drain_waiter_gate) &&
	    has_waiters(&dr->dr_waiters))
		wake_up(&dr->dr_waiters);
}

 
static inline bool xfs_defer_drain_busy(struct xfs_defer_drain *dr)
{
	return atomic_read(&dr->dr_count) > 0;
}

 
static inline int xfs_defer_drain_wait(struct xfs_defer_drain *dr)
{
	return wait_event_killable(dr->dr_waiters, !xfs_defer_drain_busy(dr));
}

 
struct xfs_perag *
xfs_perag_intent_get(
	struct xfs_mount	*mp,
	xfs_agnumber_t		agno)
{
	struct xfs_perag	*pag;

	pag = xfs_perag_get(mp, agno);
	if (!pag)
		return NULL;

	xfs_perag_intent_hold(pag);
	return pag;
}

 
void
xfs_perag_intent_put(
	struct xfs_perag	*pag)
{
	xfs_perag_intent_rele(pag);
	xfs_perag_put(pag);
}

 
void
xfs_perag_intent_hold(
	struct xfs_perag	*pag)
{
	trace_xfs_perag_intent_hold(pag, __return_address);
	xfs_defer_drain_grab(&pag->pag_intents_drain);
}

 
void
xfs_perag_intent_rele(
	struct xfs_perag	*pag)
{
	trace_xfs_perag_intent_rele(pag, __return_address);
	xfs_defer_drain_rele(&pag->pag_intents_drain);
}

 
int
xfs_perag_intent_drain(
	struct xfs_perag	*pag)
{
	trace_xfs_perag_wait_intents(pag, __return_address);
	return xfs_defer_drain_wait(&pag->pag_intents_drain);
}

 
bool
xfs_perag_intent_busy(
	struct xfs_perag	*pag)
{
	return xfs_defer_drain_busy(&pag->pag_intents_drain);
}


 
#include "xfs.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_trace.h"
#include "xfs_sysctl.h"
#include "xfs_pwork.h"
#include <linux/nmi.h>

 

 
static void
xfs_pwork_work(
	struct work_struct	*work)
{
	struct xfs_pwork	*pwork;
	struct xfs_pwork_ctl	*pctl;
	int			error;

	pwork = container_of(work, struct xfs_pwork, work);
	pctl = pwork->pctl;
	error = pctl->work_fn(pctl->mp, pwork);
	if (error && !pctl->error)
		pctl->error = error;
	if (atomic_dec_and_test(&pctl->nr_work))
		wake_up(&pctl->poll_wait);
}

 
int
xfs_pwork_init(
	struct xfs_mount	*mp,
	struct xfs_pwork_ctl	*pctl,
	xfs_pwork_work_fn	work_fn,
	const char		*tag)
{
	unsigned int		nr_threads = 0;

#ifdef DEBUG
	if (xfs_globals.pwork_threads >= 0)
		nr_threads = xfs_globals.pwork_threads;
#endif
	trace_xfs_pwork_init(mp, nr_threads, current->pid);

	pctl->wq = alloc_workqueue("%s-%d",
			WQ_UNBOUND | WQ_SYSFS | WQ_FREEZABLE, nr_threads, tag,
			current->pid);
	if (!pctl->wq)
		return -ENOMEM;
	pctl->work_fn = work_fn;
	pctl->error = 0;
	pctl->mp = mp;
	atomic_set(&pctl->nr_work, 0);
	init_waitqueue_head(&pctl->poll_wait);

	return 0;
}

 
void
xfs_pwork_queue(
	struct xfs_pwork_ctl	*pctl,
	struct xfs_pwork	*pwork)
{
	INIT_WORK(&pwork->work, xfs_pwork_work);
	pwork->pctl = pctl;
	atomic_inc(&pctl->nr_work);
	queue_work(pctl->wq, &pwork->work);
}

 
int
xfs_pwork_destroy(
	struct xfs_pwork_ctl	*pctl)
{
	destroy_workqueue(pctl->wq);
	pctl->wq = NULL;
	return pctl->error;
}

 
void
xfs_pwork_poll(
	struct xfs_pwork_ctl	*pctl)
{
	while (wait_event_timeout(pctl->poll_wait,
				atomic_read(&pctl->nr_work) == 0, HZ) == 0)
		touch_softlockup_watchdog();
}

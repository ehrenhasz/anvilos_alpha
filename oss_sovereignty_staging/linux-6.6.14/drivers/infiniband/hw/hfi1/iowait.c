
 
#include "iowait.h"
#include "trace_iowait.h"

 
#define IOWAIT_PRIORITY_STARVE_SHIFT 4

void iowait_set_flag(struct iowait *wait, u32 flag)
{
	trace_hfi1_iowait_set(wait, flag);
	set_bit(flag, &wait->flags);
}

bool iowait_flag_set(struct iowait *wait, u32 flag)
{
	return test_bit(flag, &wait->flags);
}

inline void iowait_clear_flag(struct iowait *wait, u32 flag)
{
	trace_hfi1_iowait_clear(wait, flag);
	clear_bit(flag, &wait->flags);
}

 
void iowait_init(struct iowait *wait, u32 tx_limit,
		 void (*func)(struct work_struct *work),
		 void (*tidfunc)(struct work_struct *work),
		 int (*sleep)(struct sdma_engine *sde,
			      struct iowait_work *wait,
			      struct sdma_txreq *tx,
			      uint seq,
			      bool pkts_sent),
		 void (*wakeup)(struct iowait *wait, int reason),
		 void (*sdma_drained)(struct iowait *wait),
		 void (*init_priority)(struct iowait *wait))
{
	int i;

	wait->count = 0;
	INIT_LIST_HEAD(&wait->list);
	init_waitqueue_head(&wait->wait_dma);
	init_waitqueue_head(&wait->wait_pio);
	atomic_set(&wait->sdma_busy, 0);
	atomic_set(&wait->pio_busy, 0);
	wait->tx_limit = tx_limit;
	wait->sleep = sleep;
	wait->wakeup = wakeup;
	wait->sdma_drained = sdma_drained;
	wait->init_priority = init_priority;
	wait->flags = 0;
	for (i = 0; i < IOWAIT_SES; i++) {
		wait->wait[i].iow = wait;
		INIT_LIST_HEAD(&wait->wait[i].tx_head);
		if (i == IOWAIT_IB_SE)
			INIT_WORK(&wait->wait[i].iowork, func);
		else
			INIT_WORK(&wait->wait[i].iowork, tidfunc);
	}
}

 
void iowait_cancel_work(struct iowait *w)
{
	cancel_work_sync(&iowait_get_ib_work(w)->iowork);
	 
	if (iowait_get_tid_work(w)->iowork.func)
		cancel_work_sync(&iowait_get_tid_work(w)->iowork);
}

 
int iowait_set_work_flag(struct iowait_work *w)
{
	if (w == &w->iow->wait[IOWAIT_IB_SE]) {
		iowait_set_flag(w->iow, IOWAIT_PENDING_IB);
		return IOWAIT_IB_SE;
	}
	iowait_set_flag(w->iow, IOWAIT_PENDING_TID);
	return IOWAIT_TID_SE;
}

 
uint iowait_priority_update_top(struct iowait *w,
				struct iowait *top,
				uint idx, uint top_idx)
{
	u8 cnt, tcnt;

	 
	cnt = (w->priority << IOWAIT_PRIORITY_STARVE_SHIFT) + w->starved_cnt;
	tcnt = (top->priority << IOWAIT_PRIORITY_STARVE_SHIFT) +
		top->starved_cnt;
	if (cnt > tcnt)
		return idx;
	else
		return top_idx;
}

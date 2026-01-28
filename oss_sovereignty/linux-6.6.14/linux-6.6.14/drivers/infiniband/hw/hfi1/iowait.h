#ifndef _HFI1_IOWAIT_H
#define _HFI1_IOWAIT_H
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include "sdma_txreq.h"
typedef void (*restart_t)(struct work_struct *work);
#define IOWAIT_PENDING_IB  0x0
#define IOWAIT_PENDING_TID 0x1
#define IOWAIT_SES 2
#define IOWAIT_IB_SE 0
#define IOWAIT_TID_SE 1
struct sdma_txreq;
struct sdma_engine;
struct iowait;
struct iowait_work {
	struct work_struct iowork;
	struct list_head tx_head;
	struct iowait *iow;
};
struct iowait {
	struct list_head list;
	int (*sleep)(
		struct sdma_engine *sde,
		struct iowait_work *wait,
		struct sdma_txreq *tx,
		uint seq,
		bool pkts_sent
		);
	void (*wakeup)(struct iowait *wait, int reason);
	void (*sdma_drained)(struct iowait *wait);
	void (*init_priority)(struct iowait *wait);
	seqlock_t *lock;
	wait_queue_head_t wait_dma;
	wait_queue_head_t wait_pio;
	atomic_t sdma_busy;
	atomic_t pio_busy;
	u32 count;
	u32 tx_limit;
	u32 tx_count;
	u8 starved_cnt;
	u8 priority;
	unsigned long flags;
	struct iowait_work wait[IOWAIT_SES];
};
#define SDMA_AVAIL_REASON 0
void iowait_set_flag(struct iowait *wait, u32 flag);
bool iowait_flag_set(struct iowait *wait, u32 flag);
void iowait_clear_flag(struct iowait *wait, u32 flag);
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
		 void (*init_priority)(struct iowait *wait));
static inline bool iowait_schedule(struct iowait *wait,
				   struct workqueue_struct *wq, int cpu)
{
	return !!queue_work_on(cpu, wq, &wait->wait[IOWAIT_IB_SE].iowork);
}
static inline bool iowait_tid_schedule(struct iowait *wait,
				       struct workqueue_struct *wq, int cpu)
{
	return !!queue_work_on(cpu, wq, &wait->wait[IOWAIT_TID_SE].iowork);
}
static inline void iowait_sdma_drain(struct iowait *wait)
{
	wait_event(wait->wait_dma, !atomic_read(&wait->sdma_busy));
}
static inline int iowait_sdma_pending(struct iowait *wait)
{
	return atomic_read(&wait->sdma_busy);
}
static inline void iowait_sdma_inc(struct iowait *wait)
{
	atomic_inc(&wait->sdma_busy);
}
static inline void iowait_sdma_add(struct iowait *wait, int count)
{
	atomic_add(count, &wait->sdma_busy);
}
static inline int iowait_sdma_dec(struct iowait *wait)
{
	if (!wait)
		return 0;
	return atomic_dec_and_test(&wait->sdma_busy);
}
static inline void iowait_pio_drain(struct iowait *wait)
{
	wait_event_timeout(wait->wait_pio,
			   !atomic_read(&wait->pio_busy),
			   HZ);
}
static inline int iowait_pio_pending(struct iowait *wait)
{
	return atomic_read(&wait->pio_busy);
}
static inline void iowait_pio_inc(struct iowait *wait)
{
	atomic_inc(&wait->pio_busy);
}
static inline int iowait_pio_dec(struct iowait *wait)
{
	if (!wait)
		return 0;
	return atomic_dec_and_test(&wait->pio_busy);
}
static inline void iowait_drain_wakeup(struct iowait *wait)
{
	wake_up(&wait->wait_dma);
	wake_up(&wait->wait_pio);
	if (wait->sdma_drained)
		wait->sdma_drained(wait);
}
static inline struct sdma_txreq *iowait_get_txhead(struct iowait_work *wait)
{
	struct sdma_txreq *tx = NULL;
	if (!list_empty(&wait->tx_head)) {
		tx = list_first_entry(
			&wait->tx_head,
			struct sdma_txreq,
			list);
		list_del_init(&tx->list);
	}
	return tx;
}
static inline u16 iowait_get_desc(struct iowait_work *w)
{
	u16 num_desc = 0;
	struct sdma_txreq *tx = NULL;
	if (!list_empty(&w->tx_head)) {
		tx = list_first_entry(&w->tx_head, struct sdma_txreq,
				      list);
		num_desc = tx->num_desc;
		if (tx->flags & SDMA_TXREQ_F_VIP)
			w->iow->priority++;
	}
	return num_desc;
}
static inline u32 iowait_get_all_desc(struct iowait *w)
{
	u32 num_desc = 0;
	num_desc = iowait_get_desc(&w->wait[IOWAIT_IB_SE]);
	num_desc += iowait_get_desc(&w->wait[IOWAIT_TID_SE]);
	return num_desc;
}
static inline void iowait_update_priority(struct iowait_work *w)
{
	struct sdma_txreq *tx = NULL;
	if (!list_empty(&w->tx_head)) {
		tx = list_first_entry(&w->tx_head, struct sdma_txreq,
				      list);
		if (tx->flags & SDMA_TXREQ_F_VIP)
			w->iow->priority++;
	}
}
static inline void iowait_update_all_priority(struct iowait *w)
{
	iowait_update_priority(&w->wait[IOWAIT_IB_SE]);
	iowait_update_priority(&w->wait[IOWAIT_TID_SE]);
}
static inline void iowait_init_priority(struct iowait *w)
{
	w->priority = 0;
	if (w->init_priority)
		w->init_priority(w);
}
static inline void iowait_get_priority(struct iowait *w)
{
	iowait_init_priority(w);
	iowait_update_all_priority(w);
}
static inline void iowait_queue(bool pkts_sent, struct iowait *w,
				struct list_head *wait_head)
{
	if (pkts_sent)
		w->starved_cnt = 0;
	else
		w->starved_cnt++;
	if (w->priority > 0 || !pkts_sent)
		list_add(&w->list, wait_head);
	else
		list_add_tail(&w->list, wait_head);
}
static inline void iowait_starve_clear(bool pkts_sent, struct iowait *w)
{
	if (pkts_sent)
		w->starved_cnt = 0;
}
uint iowait_priority_update_top(struct iowait *w,
				struct iowait *top,
				uint idx, uint top_idx);
static inline bool iowait_packet_queued(struct iowait_work *wait)
{
	return !list_empty(&wait->tx_head);
}
static inline void iowait_inc_wait_count(struct iowait_work *w, u16 n)
{
	if (!w)
		return;
	w->iow->tx_count++;
	w->iow->count += n;
}
static inline struct iowait_work *iowait_get_tid_work(struct iowait *w)
{
	return &w->wait[IOWAIT_TID_SE];
}
static inline struct iowait_work *iowait_get_ib_work(struct iowait *w)
{
	return &w->wait[IOWAIT_IB_SE];
}
static inline struct iowait *iowait_ioww_to_iow(struct iowait_work *w)
{
	if (likely(w))
		return w->iow;
	return NULL;
}
void iowait_cancel_work(struct iowait *w);
int iowait_set_work_flag(struct iowait_work *w);
#endif

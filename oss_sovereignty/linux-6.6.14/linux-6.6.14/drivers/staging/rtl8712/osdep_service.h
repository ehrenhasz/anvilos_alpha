#ifndef __OSDEP_SERVICE_H_
#define __OSDEP_SERVICE_H_
#define _SUCCESS	1
#define _FAIL		0
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/sched/signal.h>
#include <linux/sem.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/iw_handler.h>
#include <linux/proc_fs.h>       
#include "basic_types.h"
struct	__queue	{
	struct	list_head	queue;
	spinlock_t lock;
};
#define _pkt struct sk_buff
#define _buffer unsigned char
#define _init_queue(pqueue)				\
	do {						\
		INIT_LIST_HEAD(&((pqueue)->queue));	\
		spin_lock_init(&((pqueue)->lock));	\
	} while (0)
static inline u32 end_of_queue_search(struct list_head *head,
				      struct list_head *plist)
{
	return (head == plist);
}
static inline void flush_signals_thread(void)
{
	if (signal_pending(current))
		flush_signals(current);
}
#endif

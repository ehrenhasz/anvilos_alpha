#ifndef _TIPC_SUBSCR_H
#define _TIPC_SUBSCR_H
#include "topsrv.h"
#define TIPC_MAX_SUBSCR         65535
#define TIPC_MAX_PUBL           65535
struct publication;
struct tipc_subscription;
struct tipc_conn;
struct tipc_subscription {
	struct tipc_subscr s;
	struct tipc_event evt;
	struct kref kref;
	struct net *net;
	struct timer_list timer;
	struct list_head service_list;
	struct list_head sub_list;
	int conid;
	bool inactive;
	spinlock_t lock;
};
struct tipc_subscription *tipc_sub_subscribe(struct net *net,
					     struct tipc_subscr *s,
					     int conid);
void tipc_sub_unsubscribe(struct tipc_subscription *sub);
void tipc_sub_report_overlap(struct tipc_subscription *sub,
			     struct publication *p,
			     u32 event, bool must);
int __net_init tipc_topsrv_init_net(struct net *net);
void __net_exit tipc_topsrv_exit_net(struct net *net);
void tipc_sub_put(struct tipc_subscription *subscription);
void tipc_sub_get(struct tipc_subscription *subscription);
#define TIPC_FILTER_MASK (TIPC_SUB_PORTS | TIPC_SUB_SERVICE | TIPC_SUB_CANCEL)
#define tipc_sub_read(sub_, field_)					\
	({								\
		struct tipc_subscr *sub__ = sub_;			\
		u32 val__ = (sub__)->field_;				\
		int swap_ = !((sub__)->filter & TIPC_FILTER_MASK);	\
		(swap_ ? swab32(val__) : val__);			\
	})
#define tipc_sub_write(sub_, field_, val_)				\
	({								\
		struct tipc_subscr *sub__ = sub_;			\
		u32 val__ = val_;					\
		int swap_ = !((sub__)->filter & TIPC_FILTER_MASK);	\
		(sub__)->field_ = swap_ ? swab32(val__) : val__;	\
	})
#define tipc_evt_write(evt_, field_, val_)				\
	({								\
		struct tipc_event *evt__ = evt_;			\
		u32 val__ = val_;					\
		int swap_ = !((evt__)->s.filter & (TIPC_FILTER_MASK));	\
		(evt__)->field_ = swap_ ? swab32(val__) : val__;	\
	})
#endif

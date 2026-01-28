#ifndef AF_CAN_H
#define AF_CAN_H
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/rcupdate.h>
#include <linux/can.h>
struct receiver {
	struct hlist_node list;
	canid_t can_id;
	canid_t mask;
	unsigned long matches;
	void (*func)(struct sk_buff *skb, void *data);
	void *data;
	char *ident;
	struct sock *sk;
	struct rcu_head rcu;
};
struct can_pkg_stats {
	unsigned long jiffies_init;
	unsigned long rx_frames;
	unsigned long tx_frames;
	unsigned long matches;
	unsigned long total_rx_rate;
	unsigned long total_tx_rate;
	unsigned long total_rx_match_ratio;
	unsigned long current_rx_rate;
	unsigned long current_tx_rate;
	unsigned long current_rx_match_ratio;
	unsigned long max_rx_rate;
	unsigned long max_tx_rate;
	unsigned long max_rx_match_ratio;
	unsigned long rx_frames_delta;
	unsigned long tx_frames_delta;
	unsigned long matches_delta;
};
struct can_rcv_lists_stats {
	unsigned long stats_reset;
	unsigned long user_reset;
	unsigned long rcv_entries;
	unsigned long rcv_entries_max;
};
void can_init_proc(struct net *net);
void can_remove_proc(struct net *net);
void can_stat_update(struct timer_list *t);
#endif  

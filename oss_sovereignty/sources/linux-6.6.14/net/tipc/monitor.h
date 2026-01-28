

#ifndef _TIPC_MONITOR_H
#define _TIPC_MONITOR_H

#include "netlink.h"


struct tipc_mon_state {
	u16 list_gen;
	u16 peer_gen;
	u16 acked_gen;
	bool monitoring :1;
	bool probing    :1;
	bool reset      :1;
	bool synched    :1;
};

int tipc_mon_create(struct net *net, int bearer_id);
void tipc_mon_delete(struct net *net, int bearer_id);

void tipc_mon_peer_up(struct net *net, u32 addr, int bearer_id);
void tipc_mon_peer_down(struct net *net, u32 addr, int bearer_id);
void tipc_mon_prep(struct net *net, void *data, int *dlen,
		   struct tipc_mon_state *state, int bearer_id);
void tipc_mon_rcv(struct net *net, void *data, u16 dlen, u32 addr,
		  struct tipc_mon_state *state, int bearer_id);
void tipc_mon_get_state(struct net *net, u32 addr,
			struct tipc_mon_state *state,
			int bearer_id);
void tipc_mon_remove_peer(struct net *net, u32 addr, int bearer_id);

int tipc_nl_monitor_set_threshold(struct net *net, u32 cluster_size);
int tipc_nl_monitor_get_threshold(struct net *net);
int __tipc_nl_add_monitor(struct net *net, struct tipc_nl_msg *msg,
			  u32 bearer_id);
int tipc_nl_add_monitor_peer(struct net *net, struct tipc_nl_msg *msg,
			     u32 bearer_id, u32 *prev_node);
void tipc_mon_reinit_self(struct net *net);

extern const int tipc_max_domain_size;
#endif

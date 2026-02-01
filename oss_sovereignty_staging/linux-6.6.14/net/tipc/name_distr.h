 

#ifndef _TIPC_NAME_DISTR_H
#define _TIPC_NAME_DISTR_H

#include "name_table.h"

#define ITEM_SIZE sizeof(struct distr_item)

 
struct distr_item {
	__be32 type;
	__be32 lower;
	__be32 upper;
	__be32 port;
	__be32 key;
};

struct sk_buff *tipc_named_publish(struct net *net, struct publication *publ);
struct sk_buff *tipc_named_withdraw(struct net *net, struct publication *publ);
void tipc_named_node_up(struct net *net, u32 dnode, u16 capabilities);
void tipc_named_rcv(struct net *net, struct sk_buff_head *namedq,
		    u16 *rcv_nxt, bool *open);
void tipc_named_reinit(struct net *net);
void tipc_publ_notify(struct net *net, struct list_head *nsub_list,
		      u32 addr, u16 capabilities);

#endif

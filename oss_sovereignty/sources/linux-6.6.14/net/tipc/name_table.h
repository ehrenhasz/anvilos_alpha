

#ifndef _TIPC_NAME_TABLE_H
#define _TIPC_NAME_TABLE_H

struct tipc_subscription;
struct tipc_plist;
struct tipc_nlist;
struct tipc_group;
struct tipc_uaddr;


#define TIPC_ZM_SRV		3	
#define TIPC_PUBL_SCOPE_NUM	(TIPC_NODE_SCOPE + 1)
#define TIPC_NAMETBL_SIZE	1024	

#define TIPC_ANY_SCOPE 10      


struct publication {
	struct tipc_service_range sr;
	struct tipc_socket_addr sk;
	u16 scope;
	u32 key;
	u32 id;
	struct list_head binding_node;
	struct list_head binding_sock;
	struct list_head local_publ;
	struct list_head all_publ;
	struct list_head list;
	struct rcu_head rcu;
};


struct name_table {
	struct hlist_head services[TIPC_NAMETBL_SIZE];
	struct list_head node_scope;
	struct list_head cluster_scope;
	rwlock_t cluster_scope_lock;
	u32 local_publ_count;
	u32 rc_dests;
	u32 snd_nxt;
};

int tipc_nl_name_table_dump(struct sk_buff *skb, struct netlink_callback *cb);
bool tipc_nametbl_lookup_anycast(struct net *net, struct tipc_uaddr *ua,
				 struct tipc_socket_addr *sk);
void tipc_nametbl_lookup_mcast_sockets(struct net *net, struct tipc_uaddr *ua,
				       struct list_head *dports);
void tipc_nametbl_lookup_mcast_nodes(struct net *net, struct tipc_uaddr *ua,
				     struct tipc_nlist *nodes);
bool tipc_nametbl_lookup_group(struct net *net, struct tipc_uaddr *ua,
			       struct list_head *dsts, int *dstcnt,
			       u32 exclude, bool mcast);
void tipc_nametbl_build_group(struct net *net, struct tipc_group *grp,
			      struct tipc_uaddr *ua);
struct publication *tipc_nametbl_publish(struct net *net, struct tipc_uaddr *ua,
					 struct tipc_socket_addr *sk, u32 key);
void tipc_nametbl_withdraw(struct net *net, struct tipc_uaddr *ua,
			   struct tipc_socket_addr *sk, u32 key);
struct publication *tipc_nametbl_insert_publ(struct net *net,
					     struct tipc_uaddr *ua,
					     struct tipc_socket_addr *sk,
					     u32 key);
struct publication *tipc_nametbl_remove_publ(struct net *net,
					     struct tipc_uaddr *ua,
					     struct tipc_socket_addr *sk,
					     u32 key);
bool tipc_nametbl_subscribe(struct tipc_subscription *s);
void tipc_nametbl_unsubscribe(struct tipc_subscription *s);
int tipc_nametbl_init(struct net *net);
void tipc_nametbl_stop(struct net *net);

struct tipc_dest {
	struct list_head list;
	u32 port;
	u32 node;
};

struct tipc_dest *tipc_dest_find(struct list_head *l, u32 node, u32 port);
bool tipc_dest_push(struct list_head *l, u32 node, u32 port);
bool tipc_dest_pop(struct list_head *l, u32 *node, u32 *port);
bool tipc_dest_del(struct list_head *l, u32 node, u32 port);
void tipc_dest_list_purge(struct list_head *l);

#endif

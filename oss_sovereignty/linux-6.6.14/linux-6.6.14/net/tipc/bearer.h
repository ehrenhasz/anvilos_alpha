#ifndef _TIPC_BEARER_H
#define _TIPC_BEARER_H
#include "netlink.h"
#include "core.h"
#include "msg.h"
#include <net/genetlink.h>
#define MAX_MEDIA	3
#define TIPC_MEDIA_INFO_SIZE	32
#define TIPC_MEDIA_TYPE_OFFSET	3
#define TIPC_MEDIA_ADDR_OFFSET	4
#define TIPC_MEDIA_TYPE_ETH	1
#define TIPC_MEDIA_TYPE_IB	2
#define TIPC_MEDIA_TYPE_UDP	3
#define TIPC_MIN_BEARER_MTU	(MAX_H_SIZE + INT_H_SIZE)
#define TIPC_BROADCAST_SUPPORT  1
#define TIPC_REPLICAST_SUPPORT  2
struct tipc_media_addr {
	u8 value[TIPC_MEDIA_INFO_SIZE];
	u8 media_id;
	u8 broadcast;
};
struct tipc_bearer;
struct tipc_media {
	int (*send_msg)(struct net *net, struct sk_buff *buf,
			struct tipc_bearer *b,
			struct tipc_media_addr *dest);
	int (*enable_media)(struct net *net, struct tipc_bearer *b,
			    struct nlattr *attr[]);
	void (*disable_media)(struct tipc_bearer *b);
	int (*addr2str)(struct tipc_media_addr *addr,
			char *strbuf,
			int bufsz);
	int (*addr2msg)(char *msg, struct tipc_media_addr *addr);
	int (*msg2addr)(struct tipc_bearer *b,
			struct tipc_media_addr *addr,
			char *msg);
	int (*raw2addr)(struct tipc_bearer *b,
			struct tipc_media_addr *addr,
			const char *raw);
	u32 priority;
	u32 tolerance;
	u32 min_win;
	u32 max_win;
	u32 mtu;
	u32 type_id;
	u32 hwaddr_len;
	char name[TIPC_MAX_MEDIA_NAME];
};
struct tipc_bearer {
	void __rcu *media_ptr;			 
	u32 mtu;				 
	struct tipc_media_addr addr;		 
	char name[TIPC_MAX_BEARER_NAME];
	struct tipc_media *media;
	struct tipc_media_addr bcast_addr;
	struct packet_type pt;
	struct rcu_head rcu;
	u32 priority;
	u32 min_win;
	u32 max_win;
	u32 tolerance;
	u32 domain;
	u32 identity;
	struct tipc_discoverer *disc;
	char net_plane;
	u16 encap_hlen;
	unsigned long up;
	refcount_t refcnt;
};
struct tipc_bearer_names {
	char media_name[TIPC_MAX_MEDIA_NAME];
	char if_name[TIPC_MAX_IF_NAME];
};
void tipc_rcv(struct net *net, struct sk_buff *skb, struct tipc_bearer *b);
extern struct tipc_media eth_media_info;
#ifdef CONFIG_TIPC_MEDIA_IB
extern struct tipc_media ib_media_info;
#endif
#ifdef CONFIG_TIPC_MEDIA_UDP
extern struct tipc_media udp_media_info;
#endif
int tipc_nl_bearer_disable(struct sk_buff *skb, struct genl_info *info);
int __tipc_nl_bearer_disable(struct sk_buff *skb, struct genl_info *info);
int tipc_nl_bearer_enable(struct sk_buff *skb, struct genl_info *info);
int __tipc_nl_bearer_enable(struct sk_buff *skb, struct genl_info *info);
int tipc_nl_bearer_dump(struct sk_buff *skb, struct netlink_callback *cb);
int tipc_nl_bearer_get(struct sk_buff *skb, struct genl_info *info);
int tipc_nl_bearer_set(struct sk_buff *skb, struct genl_info *info);
int __tipc_nl_bearer_set(struct sk_buff *skb, struct genl_info *info);
int tipc_nl_bearer_add(struct sk_buff *skb, struct genl_info *info);
int tipc_nl_media_dump(struct sk_buff *skb, struct netlink_callback *cb);
int tipc_nl_media_get(struct sk_buff *skb, struct genl_info *info);
int tipc_nl_media_set(struct sk_buff *skb, struct genl_info *info);
int __tipc_nl_media_set(struct sk_buff *skb, struct genl_info *info);
int tipc_media_addr_printf(char *buf, int len, struct tipc_media_addr *a);
int tipc_enable_l2_media(struct net *net, struct tipc_bearer *b,
			 struct nlattr *attrs[]);
bool tipc_bearer_hold(struct tipc_bearer *b);
void tipc_bearer_put(struct tipc_bearer *b);
void tipc_disable_l2_media(struct tipc_bearer *b);
int tipc_l2_send_msg(struct net *net, struct sk_buff *buf,
		     struct tipc_bearer *b, struct tipc_media_addr *dest);
void tipc_bearer_add_dest(struct net *net, u32 bearer_id, u32 dest);
void tipc_bearer_remove_dest(struct net *net, u32 bearer_id, u32 dest);
struct tipc_bearer *tipc_bearer_find(struct net *net, const char *name);
int tipc_bearer_get_name(struct net *net, char *name, u32 bearer_id);
struct tipc_media *tipc_media_find(const char *name);
int tipc_bearer_setup(void);
void tipc_bearer_cleanup(void);
void tipc_bearer_stop(struct net *net);
int tipc_bearer_mtu(struct net *net, u32 bearer_id);
int tipc_bearer_min_mtu(struct net *net, u32 bearer_id);
bool tipc_bearer_bcast_support(struct net *net, u32 bearer_id);
void tipc_bearer_xmit_skb(struct net *net, u32 bearer_id,
			  struct sk_buff *skb,
			  struct tipc_media_addr *dest);
void tipc_bearer_xmit(struct net *net, u32 bearer_id,
		      struct sk_buff_head *xmitq,
		      struct tipc_media_addr *dst,
		      struct tipc_node *__dnode);
void tipc_bearer_bc_xmit(struct net *net, u32 bearer_id,
			 struct sk_buff_head *xmitq);
void tipc_clone_to_loopback(struct net *net, struct sk_buff_head *pkts);
int tipc_attach_loopback(struct net *net);
void tipc_detach_loopback(struct net *net);
static inline void tipc_loopback_trace(struct net *net,
				       struct sk_buff_head *pkts)
{
	if (unlikely(dev_nit_active(net->loopback_dev)))
		tipc_clone_to_loopback(net, pkts);
}
static inline bool tipc_mtu_bad(struct net_device *dev)
{
	if (dev->mtu >= TIPC_MIN_BEARER_MTU)
		return false;
	netdev_warn(dev, "MTU too low for tipc bearer\n");
	return true;
}
#endif	 

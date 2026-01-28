

#ifdef CONFIG_TIPC_MEDIA_UDP
#ifndef _TIPC_UDP_MEDIA_H
#define _TIPC_UDP_MEDIA_H

#include <linux/ip.h>
#include <linux/udp.h>

int tipc_udp_nl_bearer_add(struct tipc_bearer *b, struct nlattr *attr);
int tipc_udp_nl_add_bearer_data(struct tipc_nl_msg *msg, struct tipc_bearer *b);
int tipc_udp_nl_dump_remoteip(struct sk_buff *skb, struct netlink_callback *cb);


static inline bool tipc_udp_mtu_bad(u32 mtu)
{
	if (mtu >= (TIPC_MIN_BEARER_MTU + sizeof(struct iphdr) +
	    sizeof(struct udphdr)))
		return false;

	pr_warn("MTU too low for tipc bearer\n");
	return true;
}

#endif
#endif

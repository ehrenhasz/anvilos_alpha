
 
 

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/icmpv6.h>
#include <linux/mroute6.h>
#include <linux/slab.h>
#include <linux/indirect_call_wrapper.h>

#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>

#include <net/sock.h>
#include <net/snmp.h>
#include <net/udp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/rawv6.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/xfrm.h>
#include <net/inet_ecn.h>
#include <net/dst_metadata.h>

static void ip6_rcv_finish_core(struct net *net, struct sock *sk,
				struct sk_buff *skb)
{
	if (READ_ONCE(net->ipv4.sysctl_ip_early_demux) &&
	    !skb_dst(skb) && !skb->sk) {
		switch (ipv6_hdr(skb)->nexthdr) {
		case IPPROTO_TCP:
			if (READ_ONCE(net->ipv4.sysctl_tcp_early_demux))
				tcp_v6_early_demux(skb);
			break;
		case IPPROTO_UDP:
			if (READ_ONCE(net->ipv4.sysctl_udp_early_demux))
				udp_v6_early_demux(skb);
			break;
		}
	}

	if (!skb_valid_dst(skb))
		ip6_route_input(skb);
}

int ip6_rcv_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	 
	skb = l3mdev_ip6_rcv(skb);
	if (!skb)
		return NET_RX_SUCCESS;
	ip6_rcv_finish_core(net, sk, skb);

	return dst_input(skb);
}

static void ip6_sublist_rcv_finish(struct list_head *head)
{
	struct sk_buff *skb, *next;

	list_for_each_entry_safe(skb, next, head, list) {
		skb_list_del_init(skb);
		dst_input(skb);
	}
}

static bool ip6_can_use_hint(const struct sk_buff *skb,
			     const struct sk_buff *hint)
{
	return hint && !skb_dst(skb) &&
	       ipv6_addr_equal(&ipv6_hdr(hint)->daddr, &ipv6_hdr(skb)->daddr);
}

static struct sk_buff *ip6_extract_route_hint(const struct net *net,
					      struct sk_buff *skb)
{
	if (fib6_routes_require_src(net) || fib6_has_custom_rules(net) ||
	    IP6CB(skb)->flags & IP6SKB_MULTIPATH)
		return NULL;

	return skb;
}

static void ip6_list_rcv_finish(struct net *net, struct sock *sk,
				struct list_head *head)
{
	struct sk_buff *skb, *next, *hint = NULL;
	struct dst_entry *curr_dst = NULL;
	struct list_head sublist;

	INIT_LIST_HEAD(&sublist);
	list_for_each_entry_safe(skb, next, head, list) {
		struct dst_entry *dst;

		skb_list_del_init(skb);
		 
		skb = l3mdev_ip6_rcv(skb);
		if (!skb)
			continue;

		if (ip6_can_use_hint(skb, hint))
			skb_dst_copy(skb, hint);
		else
			ip6_rcv_finish_core(net, sk, skb);
		dst = skb_dst(skb);
		if (curr_dst != dst) {
			hint = ip6_extract_route_hint(net, skb);

			 
			if (!list_empty(&sublist))
				ip6_sublist_rcv_finish(&sublist);
			 
			INIT_LIST_HEAD(&sublist);
			curr_dst = dst;
		}
		list_add_tail(&skb->list, &sublist);
	}
	 
	ip6_sublist_rcv_finish(&sublist);
}

static struct sk_buff *ip6_rcv_core(struct sk_buff *skb, struct net_device *dev,
				    struct net *net)
{
	enum skb_drop_reason reason;
	const struct ipv6hdr *hdr;
	u32 pkt_len;
	struct inet6_dev *idev;

	if (skb->pkt_type == PACKET_OTHERHOST) {
		dev_core_stats_rx_otherhost_dropped_inc(skb->dev);
		kfree_skb_reason(skb, SKB_DROP_REASON_OTHERHOST);
		return NULL;
	}

	rcu_read_lock();

	idev = __in6_dev_get(skb->dev);

	__IP6_UPD_PO_STATS(net, idev, IPSTATS_MIB_IN, skb->len);

	SKB_DR_SET(reason, NOT_SPECIFIED);
	if ((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL ||
	    !idev || unlikely(idev->cnf.disable_ipv6)) {
		__IP6_INC_STATS(net, idev, IPSTATS_MIB_INDISCARDS);
		if (idev && unlikely(idev->cnf.disable_ipv6))
			SKB_DR_SET(reason, IPV6DISABLED);
		goto drop;
	}

	memset(IP6CB(skb), 0, sizeof(struct inet6_skb_parm));

	 
	IP6CB(skb)->iif = skb_valid_dst(skb) ? ip6_dst_idev(skb_dst(skb))->dev->ifindex : dev->ifindex;

	if (unlikely(!pskb_may_pull(skb, sizeof(*hdr))))
		goto err;

	hdr = ipv6_hdr(skb);

	if (hdr->version != 6) {
		SKB_DR_SET(reason, UNHANDLED_PROTO);
		goto err;
	}

	__IP6_ADD_STATS(net, idev,
			IPSTATS_MIB_NOECTPKTS +
				(ipv6_get_dsfield(hdr) & INET_ECN_MASK),
			max_t(unsigned short, 1, skb_shinfo(skb)->gso_segs));
	 
	if ((ipv6_addr_loopback(&hdr->saddr) ||
	     ipv6_addr_loopback(&hdr->daddr)) &&
	    !(dev->flags & IFF_LOOPBACK) &&
	    !netif_is_l3_master(dev))
		goto err;

	 
	if (!(skb->pkt_type == PACKET_LOOPBACK ||
	      dev->flags & IFF_LOOPBACK) &&
	    ipv6_addr_is_multicast(&hdr->daddr) &&
	    IPV6_ADDR_MC_SCOPE(&hdr->daddr) == 1)
		goto err;

	 
	if (!ipv6_addr_is_multicast(&hdr->daddr) &&
	    (skb->pkt_type == PACKET_BROADCAST ||
	     skb->pkt_type == PACKET_MULTICAST) &&
	    idev->cnf.drop_unicast_in_l2_multicast) {
		SKB_DR_SET(reason, UNICAST_IN_L2_MULTICAST);
		goto err;
	}

	 
	if (ipv6_addr_is_multicast(&hdr->daddr) &&
	    IPV6_ADDR_MC_SCOPE(&hdr->daddr) == 0)
		goto err;

	 
	if (ipv6_addr_is_multicast(&hdr->saddr))
		goto err;

	skb->transport_header = skb->network_header + sizeof(*hdr);
	IP6CB(skb)->nhoff = offsetof(struct ipv6hdr, nexthdr);

	pkt_len = ntohs(hdr->payload_len);

	 
	if (pkt_len || hdr->nexthdr != NEXTHDR_HOP) {
		if (pkt_len + sizeof(struct ipv6hdr) > skb->len) {
			__IP6_INC_STATS(net,
					idev, IPSTATS_MIB_INTRUNCATEDPKTS);
			SKB_DR_SET(reason, PKT_TOO_SMALL);
			goto drop;
		}
		if (pskb_trim_rcsum(skb, pkt_len + sizeof(struct ipv6hdr)))
			goto err;
		hdr = ipv6_hdr(skb);
	}

	if (hdr->nexthdr == NEXTHDR_HOP) {
		if (ipv6_parse_hopopts(skb) < 0) {
			__IP6_INC_STATS(net, idev, IPSTATS_MIB_INHDRERRORS);
			rcu_read_unlock();
			return NULL;
		}
	}

	rcu_read_unlock();

	 
	if (!skb_sk_is_prefetched(skb))
		skb_orphan(skb);

	return skb;
err:
	__IP6_INC_STATS(net, idev, IPSTATS_MIB_INHDRERRORS);
	SKB_DR_OR(reason, IP_INHDR);
drop:
	rcu_read_unlock();
	kfree_skb_reason(skb, reason);
	return NULL;
}

int ipv6_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
{
	struct net *net = dev_net(skb->dev);

	skb = ip6_rcv_core(skb, dev, net);
	if (skb == NULL)
		return NET_RX_DROP;
	return NF_HOOK(NFPROTO_IPV6, NF_INET_PRE_ROUTING,
		       net, NULL, skb, dev, NULL,
		       ip6_rcv_finish);
}

static void ip6_sublist_rcv(struct list_head *head, struct net_device *dev,
			    struct net *net)
{
	NF_HOOK_LIST(NFPROTO_IPV6, NF_INET_PRE_ROUTING, net, NULL,
		     head, dev, NULL, ip6_rcv_finish);
	ip6_list_rcv_finish(net, NULL, head);
}

 
void ipv6_list_rcv(struct list_head *head, struct packet_type *pt,
		   struct net_device *orig_dev)
{
	struct net_device *curr_dev = NULL;
	struct net *curr_net = NULL;
	struct sk_buff *skb, *next;
	struct list_head sublist;

	INIT_LIST_HEAD(&sublist);
	list_for_each_entry_safe(skb, next, head, list) {
		struct net_device *dev = skb->dev;
		struct net *net = dev_net(dev);

		skb_list_del_init(skb);
		skb = ip6_rcv_core(skb, dev, net);
		if (skb == NULL)
			continue;

		if (curr_dev != dev || curr_net != net) {
			 
			if (!list_empty(&sublist))
				ip6_sublist_rcv(&sublist, curr_dev, curr_net);
			 
			INIT_LIST_HEAD(&sublist);
			curr_dev = dev;
			curr_net = net;
		}
		list_add_tail(&skb->list, &sublist);
	}
	 
	if (!list_empty(&sublist))
		ip6_sublist_rcv(&sublist, curr_dev, curr_net);
}

INDIRECT_CALLABLE_DECLARE(int tcp_v6_rcv(struct sk_buff *));

 
void ip6_protocol_deliver_rcu(struct net *net, struct sk_buff *skb, int nexthdr,
			      bool have_final)
{
	const struct inet6_protocol *ipprot;
	struct inet6_dev *idev;
	unsigned int nhoff;
	SKB_DR(reason);
	bool raw;

	 

resubmit:
	idev = ip6_dst_idev(skb_dst(skb));
	nhoff = IP6CB(skb)->nhoff;
	if (!have_final) {
		if (!pskb_pull(skb, skb_transport_offset(skb)))
			goto discard;
		nexthdr = skb_network_header(skb)[nhoff];
	}

resubmit_final:
	raw = raw6_local_deliver(skb, nexthdr);
	ipprot = rcu_dereference(inet6_protos[nexthdr]);
	if (ipprot) {
		int ret;

		if (have_final) {
			if (!(ipprot->flags & INET6_PROTO_FINAL)) {
				 
				goto discard;
			}
		} else if (ipprot->flags & INET6_PROTO_FINAL) {
			const struct ipv6hdr *hdr;
			int sdif = inet6_sdif(skb);
			struct net_device *dev;

			 
			have_final = true;


			skb_postpull_rcsum(skb, skb_network_header(skb),
					   skb_network_header_len(skb));
			hdr = ipv6_hdr(skb);

			 
			if (sdif) {
				dev = dev_get_by_index_rcu(net, sdif);
				if (!dev)
					goto discard;
			} else {
				dev = skb->dev;
			}

			if (ipv6_addr_is_multicast(&hdr->daddr) &&
			    !ipv6_chk_mcast_addr(dev, &hdr->daddr,
						 &hdr->saddr) &&
			    !ipv6_is_mld(skb, nexthdr, skb_network_header_len(skb))) {
				SKB_DR_SET(reason, IP_INADDRERRORS);
				goto discard;
			}
		}
		if (!(ipprot->flags & INET6_PROTO_NOPOLICY)) {
			if (!xfrm6_policy_check(NULL, XFRM_POLICY_IN, skb)) {
				SKB_DR_SET(reason, XFRM_POLICY);
				goto discard;
			}
			nf_reset_ct(skb);
		}

		ret = INDIRECT_CALL_2(ipprot->handler, tcp_v6_rcv, udpv6_rcv,
				      skb);
		if (ret > 0) {
			if (ipprot->flags & INET6_PROTO_FINAL) {
				 
				nexthdr = ret;
				goto resubmit_final;
			} else {
				goto resubmit;
			}
		} else if (ret == 0) {
			__IP6_INC_STATS(net, idev, IPSTATS_MIB_INDELIVERS);
		}
	} else {
		if (!raw) {
			if (xfrm6_policy_check(NULL, XFRM_POLICY_IN, skb)) {
				__IP6_INC_STATS(net, idev,
						IPSTATS_MIB_INUNKNOWNPROTOS);
				icmpv6_send(skb, ICMPV6_PARAMPROB,
					    ICMPV6_UNK_NEXTHDR, nhoff);
				SKB_DR_SET(reason, IP_NOPROTO);
			} else {
				SKB_DR_SET(reason, XFRM_POLICY);
			}
			kfree_skb_reason(skb, reason);
		} else {
			__IP6_INC_STATS(net, idev, IPSTATS_MIB_INDELIVERS);
			consume_skb(skb);
		}
	}
	return;

discard:
	__IP6_INC_STATS(net, idev, IPSTATS_MIB_INDISCARDS);
	kfree_skb_reason(skb, reason);
}

static int ip6_input_finish(struct net *net, struct sock *sk, struct sk_buff *skb)
{
	skb_clear_delivery_time(skb);
	rcu_read_lock();
	ip6_protocol_deliver_rcu(net, skb, 0, false);
	rcu_read_unlock();

	return 0;
}


int ip6_input(struct sk_buff *skb)
{
	return NF_HOOK(NFPROTO_IPV6, NF_INET_LOCAL_IN,
		       dev_net(skb->dev), NULL, skb, skb->dev, NULL,
		       ip6_input_finish);
}
EXPORT_SYMBOL_GPL(ip6_input);

int ip6_mc_input(struct sk_buff *skb)
{
	int sdif = inet6_sdif(skb);
	const struct ipv6hdr *hdr;
	struct net_device *dev;
	bool deliver;

	__IP6_UPD_PO_STATS(dev_net(skb_dst(skb)->dev),
			 __in6_dev_get_safely(skb->dev), IPSTATS_MIB_INMCAST,
			 skb->len);

	 
	if (sdif) {
		rcu_read_lock();
		dev = dev_get_by_index_rcu(dev_net(skb->dev), sdif);
		if (!dev) {
			rcu_read_unlock();
			kfree_skb(skb);
			return -ENODEV;
		}
	} else {
		dev = skb->dev;
	}

	hdr = ipv6_hdr(skb);
	deliver = ipv6_chk_mcast_addr(dev, &hdr->daddr, NULL);
	if (sdif)
		rcu_read_unlock();

#ifdef CONFIG_IPV6_MROUTE
	 
	if (atomic_read(&dev_net(skb->dev)->ipv6.devconf_all->mc_forwarding) &&
	    !(ipv6_addr_type(&hdr->daddr) &
	      (IPV6_ADDR_LOOPBACK|IPV6_ADDR_LINKLOCAL)) &&
	    likely(!(IP6CB(skb)->flags & IP6SKB_FORWARDED))) {
		 
		struct sk_buff *skb2;
		struct inet6_skb_parm *opt = IP6CB(skb);

		 
		if (unlikely(opt->flags & IP6SKB_ROUTERALERT)) {
			 
			u8 nexthdr = hdr->nexthdr;
			__be16 frag_off;
			int offset;

			 
			if (opt->ra == htons(IPV6_OPT_ROUTERALERT_MLD)) {
				deliver = false;

				if (!ipv6_ext_hdr(nexthdr)) {
					 
					goto out;
				}
				offset = ipv6_skip_exthdr(skb, sizeof(*hdr),
							  &nexthdr, &frag_off);
				if (offset < 0)
					goto out;

				if (ipv6_is_mld(skb, nexthdr, offset))
					deliver = true;

				goto out;
			}
			 
		}

		if (deliver)
			skb2 = skb_clone(skb, GFP_ATOMIC);
		else {
			skb2 = skb;
			skb = NULL;
		}

		if (skb2) {
			ip6_mr_input(skb2);
		}
	}
out:
#endif
	if (likely(deliver))
		ip6_input(skb);
	else {
		 
		kfree_skb(skb);
	}

	return 0;
}

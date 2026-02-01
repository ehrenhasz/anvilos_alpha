
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/ipsec.h>
#include <linux/slab.h>

#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/random.h>
#include <linux/seq_file.h>

#include <net/protocol.h>
#include <net/ndisc.h>
#include <net/ip.h>
#include <net/ipv6.h>
#include <net/transp_v6.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>
#include <net/inet_common.h>
#include <net/inet_ecn.h>
#include <net/sctp/sctp.h>
#include <net/udp_tunnel.h>

#include <linux/uaccess.h>

static inline int sctp_v6_addr_match_len(union sctp_addr *s1,
					 union sctp_addr *s2);
static void sctp_v6_to_addr(union sctp_addr *addr, struct in6_addr *saddr,
			      __be16 port);
static int sctp_v6_cmp_addr(const union sctp_addr *addr1,
			    const union sctp_addr *addr2);

 
static int sctp_inet6addr_event(struct notifier_block *this, unsigned long ev,
				void *ptr)
{
	struct inet6_ifaddr *ifa = (struct inet6_ifaddr *)ptr;
	struct sctp_sockaddr_entry *addr = NULL;
	struct sctp_sockaddr_entry *temp;
	struct net *net = dev_net(ifa->idev->dev);
	int found = 0;

	switch (ev) {
	case NETDEV_UP:
		addr = kzalloc(sizeof(*addr), GFP_ATOMIC);
		if (addr) {
			addr->a.v6.sin6_family = AF_INET6;
			addr->a.v6.sin6_addr = ifa->addr;
			addr->a.v6.sin6_scope_id = ifa->idev->dev->ifindex;
			addr->valid = 1;
			spin_lock_bh(&net->sctp.local_addr_lock);
			list_add_tail_rcu(&addr->list, &net->sctp.local_addr_list);
			sctp_addr_wq_mgmt(net, addr, SCTP_ADDR_NEW);
			spin_unlock_bh(&net->sctp.local_addr_lock);
		}
		break;
	case NETDEV_DOWN:
		spin_lock_bh(&net->sctp.local_addr_lock);
		list_for_each_entry_safe(addr, temp,
					&net->sctp.local_addr_list, list) {
			if (addr->a.sa.sa_family == AF_INET6 &&
			    ipv6_addr_equal(&addr->a.v6.sin6_addr,
					    &ifa->addr) &&
			    addr->a.v6.sin6_scope_id == ifa->idev->dev->ifindex) {
				sctp_addr_wq_mgmt(net, addr, SCTP_ADDR_DEL);
				found = 1;
				addr->valid = 0;
				list_del_rcu(&addr->list);
				break;
			}
		}
		spin_unlock_bh(&net->sctp.local_addr_lock);
		if (found)
			kfree_rcu(addr, rcu);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block sctp_inet6addr_notifier = {
	.notifier_call = sctp_inet6addr_event,
};

static void sctp_v6_err_handle(struct sctp_transport *t, struct sk_buff *skb,
			       __u8 type, __u8 code, __u32 info)
{
	struct sctp_association *asoc = t->asoc;
	struct sock *sk = asoc->base.sk;
	struct ipv6_pinfo *np;
	int err = 0;

	switch (type) {
	case ICMPV6_PKT_TOOBIG:
		if (ip6_sk_accept_pmtu(sk))
			sctp_icmp_frag_needed(sk, asoc, t, info);
		return;
	case ICMPV6_PARAMPROB:
		if (ICMPV6_UNK_NEXTHDR == code) {
			sctp_icmp_proto_unreachable(sk, asoc, t);
			return;
		}
		break;
	case NDISC_REDIRECT:
		sctp_icmp_redirect(sk, t, skb);
		return;
	default:
		break;
	}

	np = inet6_sk(sk);
	icmpv6_err_convert(type, code, &err);
	if (!sock_owned_by_user(sk) && np->recverr) {
		sk->sk_err = err;
		sk_error_report(sk);
	} else {
		WRITE_ONCE(sk->sk_err_soft, err);
	}
}

 
static int sctp_v6_err(struct sk_buff *skb, struct inet6_skb_parm *opt,
		       u8 type, u8 code, int offset, __be32 info)
{
	struct net *net = dev_net(skb->dev);
	struct sctp_transport *transport;
	struct sctp_association *asoc;
	__u16 saveip, savesctp;
	struct sock *sk;

	 
	saveip	 = skb->network_header;
	savesctp = skb->transport_header;
	skb_reset_network_header(skb);
	skb_set_transport_header(skb, offset);
	sk = sctp_err_lookup(net, AF_INET6, skb, sctp_hdr(skb), &asoc, &transport);
	 
	skb->network_header   = saveip;
	skb->transport_header = savesctp;
	if (!sk) {
		__ICMP6_INC_STATS(net, __in6_dev_get(skb->dev), ICMP6_MIB_INERRORS);
		return -ENOENT;
	}

	sctp_v6_err_handle(transport, skb, type, code, ntohl(info));
	sctp_err_finish(sk, transport);

	return 0;
}

int sctp_udp_v6_err(struct sock *sk, struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);
	struct sctp_association *asoc;
	struct sctp_transport *t;
	struct icmp6hdr *hdr;
	__u32 info = 0;

	skb->transport_header += sizeof(struct udphdr);
	sk = sctp_err_lookup(net, AF_INET6, skb, sctp_hdr(skb), &asoc, &t);
	if (!sk) {
		__ICMP6_INC_STATS(net, __in6_dev_get(skb->dev), ICMP6_MIB_INERRORS);
		return -ENOENT;
	}

	skb->transport_header -= sizeof(struct udphdr);
	hdr = (struct icmp6hdr *)(skb_network_header(skb) - sizeof(struct icmp6hdr));
	if (hdr->icmp6_type == NDISC_REDIRECT) {
		 
		sctp_err_finish(sk, t);
		return 0;
	}
	if (hdr->icmp6_type == ICMPV6_PKT_TOOBIG)
		info = ntohl(hdr->icmp6_mtu);
	sctp_v6_err_handle(t, skb, hdr->icmp6_type, hdr->icmp6_code, info);

	sctp_err_finish(sk, t);
	return 1;
}

static int sctp_v6_xmit(struct sk_buff *skb, struct sctp_transport *t)
{
	struct dst_entry *dst = dst_clone(t->dst);
	struct flowi6 *fl6 = &t->fl.u.ip6;
	struct sock *sk = skb->sk;
	struct ipv6_pinfo *np = inet6_sk(sk);
	__u8 tclass = np->tclass;
	__be32 label;

	pr_debug("%s: skb:%p, len:%d, src:%pI6 dst:%pI6\n", __func__, skb,
		 skb->len, &fl6->saddr, &fl6->daddr);

	if (t->dscp & SCTP_DSCP_SET_MASK)
		tclass = t->dscp & SCTP_DSCP_VAL_MASK;

	if (INET_ECN_is_capable(tclass))
		IP6_ECN_flow_xmit(sk, fl6->flowlabel);

	if (!(t->param_flags & SPP_PMTUD_ENABLE))
		skb->ignore_df = 1;

	SCTP_INC_STATS(sock_net(sk), SCTP_MIB_OUTSCTPPACKS);

	if (!t->encap_port || !sctp_sk(sk)->udp_port) {
		int res;

		skb_dst_set(skb, dst);
		rcu_read_lock();
		res = ip6_xmit(sk, skb, fl6, sk->sk_mark,
			       rcu_dereference(np->opt),
			       tclass, sk->sk_priority);
		rcu_read_unlock();
		return res;
	}

	if (skb_is_gso(skb))
		skb_shinfo(skb)->gso_type |= SKB_GSO_UDP_TUNNEL_CSUM;

	skb->encapsulation = 1;
	skb_reset_inner_mac_header(skb);
	skb_reset_inner_transport_header(skb);
	skb_set_inner_ipproto(skb, IPPROTO_SCTP);
	label = ip6_make_flowlabel(sock_net(sk), skb, fl6->flowlabel, true, fl6);

	return udp_tunnel6_xmit_skb(dst, sk, skb, NULL, &fl6->saddr,
				    &fl6->daddr, tclass, ip6_dst_hoplimit(dst),
				    label, sctp_sk(sk)->udp_port, t->encap_port, false);
}

 
static void sctp_v6_get_dst(struct sctp_transport *t, union sctp_addr *saddr,
			    struct flowi *fl, struct sock *sk)
{
	struct sctp_association *asoc = t->asoc;
	struct dst_entry *dst = NULL;
	struct flowi _fl;
	struct flowi6 *fl6 = &_fl.u.ip6;
	struct sctp_bind_addr *bp;
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct sctp_sockaddr_entry *laddr;
	union sctp_addr *daddr = &t->ipaddr;
	union sctp_addr dst_saddr;
	struct in6_addr *final_p, final;
	enum sctp_scope scope;
	__u8 matchlen = 0;

	memset(&_fl, 0, sizeof(_fl));
	fl6->daddr = daddr->v6.sin6_addr;
	fl6->fl6_dport = daddr->v6.sin6_port;
	fl6->flowi6_proto = IPPROTO_SCTP;
	if (ipv6_addr_type(&daddr->v6.sin6_addr) & IPV6_ADDR_LINKLOCAL)
		fl6->flowi6_oif = daddr->v6.sin6_scope_id;
	else if (asoc)
		fl6->flowi6_oif = asoc->base.sk->sk_bound_dev_if;
	if (t->flowlabel & SCTP_FLOWLABEL_SET_MASK)
		fl6->flowlabel = htonl(t->flowlabel & SCTP_FLOWLABEL_VAL_MASK);

	if (np->sndflow && (fl6->flowlabel & IPV6_FLOWLABEL_MASK)) {
		struct ip6_flowlabel *flowlabel;

		flowlabel = fl6_sock_lookup(sk, fl6->flowlabel);
		if (IS_ERR(flowlabel))
			goto out;
		fl6_sock_release(flowlabel);
	}

	pr_debug("%s: dst=%pI6 ", __func__, &fl6->daddr);

	if (asoc)
		fl6->fl6_sport = htons(asoc->base.bind_addr.port);

	if (saddr) {
		fl6->saddr = saddr->v6.sin6_addr;
		if (!fl6->fl6_sport)
			fl6->fl6_sport = saddr->v6.sin6_port;

		pr_debug("src=%pI6 - ", &fl6->saddr);
	}

	rcu_read_lock();
	final_p = fl6_update_dst(fl6, rcu_dereference(np->opt), &final);
	rcu_read_unlock();

	dst = ip6_dst_lookup_flow(sock_net(sk), sk, fl6, final_p);
	if (!asoc || saddr) {
		t->dst = dst;
		memcpy(fl, &_fl, sizeof(_fl));
		goto out;
	}

	bp = &asoc->base.bind_addr;
	scope = sctp_scope(daddr);
	 
	if (!IS_ERR(dst)) {
		 
		sctp_v6_to_addr(&dst_saddr, &fl6->saddr, htons(bp->port));
		rcu_read_lock();
		list_for_each_entry_rcu(laddr, &bp->address_list, list) {
			if (!laddr->valid || laddr->state == SCTP_ADDR_DEL ||
			    (laddr->state != SCTP_ADDR_SRC &&
			     !asoc->src_out_of_asoc_ok))
				continue;

			 
			if ((laddr->a.sa.sa_family == AF_INET6) &&
			    (sctp_v6_cmp_addr(&dst_saddr, &laddr->a))) {
				rcu_read_unlock();
				t->dst = dst;
				memcpy(fl, &_fl, sizeof(_fl));
				goto out;
			}
		}
		rcu_read_unlock();
		 
		dst_release(dst);
		dst = NULL;
	}

	 
	rcu_read_lock();
	list_for_each_entry_rcu(laddr, &bp->address_list, list) {
		struct dst_entry *bdst;
		__u8 bmatchlen;

		if (!laddr->valid ||
		    laddr->state != SCTP_ADDR_SRC ||
		    laddr->a.sa.sa_family != AF_INET6 ||
		    scope > sctp_scope(&laddr->a))
			continue;

		fl6->saddr = laddr->a.v6.sin6_addr;
		fl6->fl6_sport = laddr->a.v6.sin6_port;
		final_p = fl6_update_dst(fl6, rcu_dereference(np->opt), &final);
		bdst = ip6_dst_lookup_flow(sock_net(sk), sk, fl6, final_p);

		if (IS_ERR(bdst))
			continue;

		if (ipv6_chk_addr(dev_net(bdst->dev),
				  &laddr->a.v6.sin6_addr, bdst->dev, 1)) {
			if (!IS_ERR_OR_NULL(dst))
				dst_release(dst);
			dst = bdst;
			t->dst = dst;
			memcpy(fl, &_fl, sizeof(_fl));
			break;
		}

		bmatchlen = sctp_v6_addr_match_len(daddr, &laddr->a);
		if (matchlen > bmatchlen) {
			dst_release(bdst);
			continue;
		}

		if (!IS_ERR_OR_NULL(dst))
			dst_release(dst);
		dst = bdst;
		matchlen = bmatchlen;
		t->dst = dst;
		memcpy(fl, &_fl, sizeof(_fl));
	}
	rcu_read_unlock();

out:
	if (!IS_ERR_OR_NULL(dst)) {
		struct rt6_info *rt;

		rt = (struct rt6_info *)dst;
		t->dst_cookie = rt6_get_cookie(rt);
		pr_debug("rt6_dst:%pI6/%d rt6_src:%pI6\n",
			 &rt->rt6i_dst.addr, rt->rt6i_dst.plen,
			 &fl->u.ip6.saddr);
	} else {
		t->dst = NULL;
		pr_debug("no route\n");
	}
}

 
static inline int sctp_v6_addr_match_len(union sctp_addr *s1,
					 union sctp_addr *s2)
{
	return ipv6_addr_diff(&s1->v6.sin6_addr, &s2->v6.sin6_addr);
}

 
static void sctp_v6_get_saddr(struct sctp_sock *sk,
			      struct sctp_transport *t,
			      struct flowi *fl)
{
	struct flowi6 *fl6 = &fl->u.ip6;
	union sctp_addr *saddr = &t->saddr;

	pr_debug("%s: asoc:%p dst:%p\n", __func__, t->asoc, t->dst);

	if (t->dst) {
		saddr->v6.sin6_family = AF_INET6;
		saddr->v6.sin6_addr = fl6->saddr;
	}
}

 
static void sctp_v6_copy_addrlist(struct list_head *addrlist,
				  struct net_device *dev)
{
	struct inet6_dev *in6_dev;
	struct inet6_ifaddr *ifp;
	struct sctp_sockaddr_entry *addr;

	rcu_read_lock();
	if ((in6_dev = __in6_dev_get(dev)) == NULL) {
		rcu_read_unlock();
		return;
	}

	read_lock_bh(&in6_dev->lock);
	list_for_each_entry(ifp, &in6_dev->addr_list, if_list) {
		 
		addr = kzalloc(sizeof(*addr), GFP_ATOMIC);
		if (addr) {
			addr->a.v6.sin6_family = AF_INET6;
			addr->a.v6.sin6_addr = ifp->addr;
			addr->a.v6.sin6_scope_id = dev->ifindex;
			addr->valid = 1;
			INIT_LIST_HEAD(&addr->list);
			list_add_tail(&addr->list, addrlist);
		}
	}

	read_unlock_bh(&in6_dev->lock);
	rcu_read_unlock();
}

 
static void sctp_v6_copy_ip_options(struct sock *sk, struct sock *newsk)
{
	struct ipv6_pinfo *newnp, *np = inet6_sk(sk);
	struct ipv6_txoptions *opt;

	newnp = inet6_sk(newsk);

	rcu_read_lock();
	opt = rcu_dereference(np->opt);
	if (opt) {
		opt = ipv6_dup_options(newsk, opt);
		if (!opt)
			pr_err("%s: Failed to copy ip options\n", __func__);
	}
	RCU_INIT_POINTER(newnp->opt, opt);
	rcu_read_unlock();
}

 
static int sctp_v6_ip_options_len(struct sock *sk)
{
	struct ipv6_pinfo *np = inet6_sk(sk);
	struct ipv6_txoptions *opt;
	int len = 0;

	rcu_read_lock();
	opt = rcu_dereference(np->opt);
	if (opt)
		len = opt->opt_flen + opt->opt_nflen;

	rcu_read_unlock();
	return len;
}

 
static void sctp_v6_from_skb(union sctp_addr *addr, struct sk_buff *skb,
			     int is_saddr)
{
	 
	struct sctphdr *sh = sctp_hdr(skb);
	struct sockaddr_in6 *sa = &addr->v6;

	addr->v6.sin6_family = AF_INET6;
	addr->v6.sin6_flowinfo = 0;  
	addr->v6.sin6_scope_id = ((struct inet6_skb_parm *)skb->cb)->iif;

	if (is_saddr) {
		sa->sin6_port = sh->source;
		sa->sin6_addr = ipv6_hdr(skb)->saddr;
	} else {
		sa->sin6_port = sh->dest;
		sa->sin6_addr = ipv6_hdr(skb)->daddr;
	}
}

 
static void sctp_v6_from_sk(union sctp_addr *addr, struct sock *sk)
{
	addr->v6.sin6_family = AF_INET6;
	addr->v6.sin6_port = 0;
	addr->v6.sin6_addr = sk->sk_v6_rcv_saddr;
}

 
static void sctp_v6_to_sk_saddr(union sctp_addr *addr, struct sock *sk)
{
	if (addr->sa.sa_family == AF_INET) {
		sk->sk_v6_rcv_saddr.s6_addr32[0] = 0;
		sk->sk_v6_rcv_saddr.s6_addr32[1] = 0;
		sk->sk_v6_rcv_saddr.s6_addr32[2] = htonl(0x0000ffff);
		sk->sk_v6_rcv_saddr.s6_addr32[3] =
			addr->v4.sin_addr.s_addr;
	} else {
		sk->sk_v6_rcv_saddr = addr->v6.sin6_addr;
	}
}

 
static void sctp_v6_to_sk_daddr(union sctp_addr *addr, struct sock *sk)
{
	if (addr->sa.sa_family == AF_INET) {
		sk->sk_v6_daddr.s6_addr32[0] = 0;
		sk->sk_v6_daddr.s6_addr32[1] = 0;
		sk->sk_v6_daddr.s6_addr32[2] = htonl(0x0000ffff);
		sk->sk_v6_daddr.s6_addr32[3] = addr->v4.sin_addr.s_addr;
	} else {
		sk->sk_v6_daddr = addr->v6.sin6_addr;
	}
}

 
static bool sctp_v6_from_addr_param(union sctp_addr *addr,
				    union sctp_addr_param *param,
				    __be16 port, int iif)
{
	if (ntohs(param->v6.param_hdr.length) < sizeof(struct sctp_ipv6addr_param))
		return false;

	addr->v6.sin6_family = AF_INET6;
	addr->v6.sin6_port = port;
	addr->v6.sin6_flowinfo = 0;  
	addr->v6.sin6_addr = param->v6.addr;
	addr->v6.sin6_scope_id = iif;

	return true;
}

 
static int sctp_v6_to_addr_param(const union sctp_addr *addr,
				 union sctp_addr_param *param)
{
	int length = sizeof(struct sctp_ipv6addr_param);

	param->v6.param_hdr.type = SCTP_PARAM_IPV6_ADDRESS;
	param->v6.param_hdr.length = htons(length);
	param->v6.addr = addr->v6.sin6_addr;

	return length;
}

 
static void sctp_v6_to_addr(union sctp_addr *addr, struct in6_addr *saddr,
			      __be16 port)
{
	addr->sa.sa_family = AF_INET6;
	addr->v6.sin6_port = port;
	addr->v6.sin6_flowinfo = 0;
	addr->v6.sin6_addr = *saddr;
	addr->v6.sin6_scope_id = 0;
}

static int __sctp_v6_cmp_addr(const union sctp_addr *addr1,
			      const union sctp_addr *addr2)
{
	if (addr1->sa.sa_family != addr2->sa.sa_family) {
		if (addr1->sa.sa_family == AF_INET &&
		    addr2->sa.sa_family == AF_INET6 &&
		    ipv6_addr_v4mapped(&addr2->v6.sin6_addr) &&
		    addr2->v6.sin6_addr.s6_addr32[3] ==
		    addr1->v4.sin_addr.s_addr)
			return 1;

		if (addr2->sa.sa_family == AF_INET &&
		    addr1->sa.sa_family == AF_INET6 &&
		    ipv6_addr_v4mapped(&addr1->v6.sin6_addr) &&
		    addr1->v6.sin6_addr.s6_addr32[3] ==
		    addr2->v4.sin_addr.s_addr)
			return 1;

		return 0;
	}

	if (!ipv6_addr_equal(&addr1->v6.sin6_addr, &addr2->v6.sin6_addr))
		return 0;

	 
	if ((ipv6_addr_type(&addr1->v6.sin6_addr) & IPV6_ADDR_LINKLOCAL) &&
	    addr1->v6.sin6_scope_id && addr2->v6.sin6_scope_id &&
	    addr1->v6.sin6_scope_id != addr2->v6.sin6_scope_id)
		return 0;

	return 1;
}

 
static int sctp_v6_cmp_addr(const union sctp_addr *addr1,
			    const union sctp_addr *addr2)
{
	return __sctp_v6_cmp_addr(addr1, addr2) &&
	       addr1->v6.sin6_port == addr2->v6.sin6_port;
}

 
static void sctp_v6_inaddr_any(union sctp_addr *addr, __be16 port)
{
	memset(addr, 0x00, sizeof(union sctp_addr));
	addr->v6.sin6_family = AF_INET6;
	addr->v6.sin6_port = port;
}

 
static int sctp_v6_is_any(const union sctp_addr *addr)
{
	return ipv6_addr_any(&addr->v6.sin6_addr);
}

 
static int sctp_v6_available(union sctp_addr *addr, struct sctp_sock *sp)
{
	const struct in6_addr *in6 = (const struct in6_addr *)&addr->v6.sin6_addr;
	struct sock *sk = &sp->inet.sk;
	struct net *net = sock_net(sk);
	struct net_device *dev = NULL;
	int type;

	type = ipv6_addr_type(in6);
	if (IPV6_ADDR_ANY == type)
		return 1;
	if (type == IPV6_ADDR_MAPPED) {
		if (sp && ipv6_only_sock(sctp_opt2sk(sp)))
			return 0;
		sctp_v6_map_v4(addr);
		return sctp_get_af_specific(AF_INET)->available(addr, sp);
	}
	if (!(type & IPV6_ADDR_UNICAST))
		return 0;

	if (sk->sk_bound_dev_if) {
		dev = dev_get_by_index_rcu(net, sk->sk_bound_dev_if);
		if (!dev)
			return 0;
	}

	return ipv6_can_nonlocal_bind(net, &sp->inet) ||
	       ipv6_chk_addr(net, in6, dev, 0);
}

 
static int sctp_v6_addr_valid(union sctp_addr *addr,
			      struct sctp_sock *sp,
			      const struct sk_buff *skb)
{
	int ret = ipv6_addr_type(&addr->v6.sin6_addr);

	 
	if (ret == IPV6_ADDR_MAPPED) {
		 
		if (sp && ipv6_only_sock(sctp_opt2sk(sp)))
			return 0;
		sctp_v6_map_v4(addr);
		return sctp_get_af_specific(AF_INET)->addr_valid(addr, sp, skb);
	}

	 
	if (!(ret & IPV6_ADDR_UNICAST))
		return 0;

	return 1;
}

 
static enum sctp_scope sctp_v6_scope(union sctp_addr *addr)
{
	enum sctp_scope retval;
	int v6scope;

	 

	v6scope = ipv6_addr_scope(&addr->v6.sin6_addr);
	switch (v6scope) {
	case IFA_HOST:
		retval = SCTP_SCOPE_LOOPBACK;
		break;
	case IFA_LINK:
		retval = SCTP_SCOPE_LINK;
		break;
	case IFA_SITE:
		retval = SCTP_SCOPE_PRIVATE;
		break;
	default:
		retval = SCTP_SCOPE_GLOBAL;
		break;
	}

	return retval;
}

 
static struct sock *sctp_v6_create_accept_sk(struct sock *sk,
					     struct sctp_association *asoc,
					     bool kern)
{
	struct sock *newsk;
	struct ipv6_pinfo *newnp, *np = inet6_sk(sk);
	struct sctp6_sock *newsctp6sk;

	newsk = sk_alloc(sock_net(sk), PF_INET6, GFP_KERNEL, sk->sk_prot, kern);
	if (!newsk)
		goto out;

	sock_init_data(NULL, newsk);

	sctp_copy_sock(newsk, sk, asoc);
	sock_reset_flag(sk, SOCK_ZAPPED);

	newsctp6sk = (struct sctp6_sock *)newsk;
	inet_sk(newsk)->pinet6 = &newsctp6sk->inet6;

	sctp_sk(newsk)->v4mapped = sctp_sk(sk)->v4mapped;

	newnp = inet6_sk(newsk);

	memcpy(newnp, np, sizeof(struct ipv6_pinfo));
	newnp->ipv6_mc_list = NULL;
	newnp->ipv6_ac_list = NULL;
	newnp->ipv6_fl_list = NULL;

	sctp_v6_copy_ip_options(sk, newsk);

	 
	sctp_v6_to_sk_daddr(&asoc->peer.primary_addr, newsk);

	newsk->sk_v6_rcv_saddr = sk->sk_v6_rcv_saddr;

	if (newsk->sk_prot->init(newsk)) {
		sk_common_release(newsk);
		newsk = NULL;
	}

out:
	return newsk;
}

 
static int sctp_v6_addr_to_user(struct sctp_sock *sp, union sctp_addr *addr)
{
	if (sp->v4mapped) {
		if (addr->sa.sa_family == AF_INET)
			sctp_v4_map_v6(addr);
	} else {
		if (addr->sa.sa_family == AF_INET6 &&
		    ipv6_addr_v4mapped(&addr->v6.sin6_addr))
			sctp_v6_map_v4(addr);
	}

	if (addr->sa.sa_family == AF_INET) {
		memset(addr->v4.sin_zero, 0, sizeof(addr->v4.sin_zero));
		return sizeof(struct sockaddr_in);
	}
	return sizeof(struct sockaddr_in6);
}

 
static int sctp_v6_skb_iif(const struct sk_buff *skb)
{
	return inet6_iif(skb);
}

static int sctp_v6_skb_sdif(const struct sk_buff *skb)
{
	return inet6_sdif(skb);
}

 
static int sctp_v6_is_ce(const struct sk_buff *skb)
{
	return *((__u32 *)(ipv6_hdr(skb))) & (__force __u32)htonl(1 << 20);
}

 
static void sctp_v6_seq_dump_addr(struct seq_file *seq, union sctp_addr *addr)
{
	seq_printf(seq, "%pI6 ", &addr->v6.sin6_addr);
}

static void sctp_v6_ecn_capable(struct sock *sk)
{
	inet6_sk(sk)->tclass |= INET_ECN_ECT_0;
}

 
static void sctp_inet6_event_msgname(struct sctp_ulpevent *event,
				     char *msgname, int *addrlen)
{
	union sctp_addr *addr;
	struct sctp_association *asoc;
	union sctp_addr *paddr;

	if (!msgname)
		return;

	addr = (union sctp_addr *)msgname;
	asoc = event->asoc;
	paddr = &asoc->peer.primary_addr;

	if (paddr->sa.sa_family == AF_INET) {
		addr->v4.sin_family = AF_INET;
		addr->v4.sin_port = htons(asoc->peer.port);
		addr->v4.sin_addr = paddr->v4.sin_addr;
	} else {
		addr->v6.sin6_family = AF_INET6;
		addr->v6.sin6_flowinfo = 0;
		if (ipv6_addr_type(&paddr->v6.sin6_addr) & IPV6_ADDR_LINKLOCAL)
			addr->v6.sin6_scope_id = paddr->v6.sin6_scope_id;
		else
			addr->v6.sin6_scope_id = 0;
		addr->v6.sin6_port = htons(asoc->peer.port);
		addr->v6.sin6_addr = paddr->v6.sin6_addr;
	}

	*addrlen = sctp_v6_addr_to_user(sctp_sk(asoc->base.sk), addr);
}

 
static void sctp_inet6_skb_msgname(struct sk_buff *skb, char *msgname,
				   int *addr_len)
{
	union sctp_addr *addr;
	struct sctphdr *sh;

	if (!msgname)
		return;

	addr = (union sctp_addr *)msgname;
	sh = sctp_hdr(skb);

	if (ip_hdr(skb)->version == 4) {
		addr->v4.sin_family = AF_INET;
		addr->v4.sin_port = sh->source;
		addr->v4.sin_addr.s_addr = ip_hdr(skb)->saddr;
	} else {
		addr->v6.sin6_family = AF_INET6;
		addr->v6.sin6_flowinfo = 0;
		addr->v6.sin6_port = sh->source;
		addr->v6.sin6_addr = ipv6_hdr(skb)->saddr;
		if (ipv6_addr_type(&addr->v6.sin6_addr) & IPV6_ADDR_LINKLOCAL)
			addr->v6.sin6_scope_id = sctp_v6_skb_iif(skb);
		else
			addr->v6.sin6_scope_id = 0;
	}

	*addr_len = sctp_v6_addr_to_user(sctp_sk(skb->sk), addr);
}

 
static int sctp_inet6_af_supported(sa_family_t family, struct sctp_sock *sp)
{
	switch (family) {
	case AF_INET6:
		return 1;
	 
	case AF_INET:
		if (!ipv6_only_sock(sctp_opt2sk(sp)))
			return 1;
		fallthrough;
	default:
		return 0;
	}
}

 
static int sctp_inet6_cmp_addr(const union sctp_addr *addr1,
			       const union sctp_addr *addr2,
			       struct sctp_sock *opt)
{
	struct sock *sk = sctp_opt2sk(opt);
	struct sctp_af *af1, *af2;

	af1 = sctp_get_af_specific(addr1->sa.sa_family);
	af2 = sctp_get_af_specific(addr2->sa.sa_family);

	if (!af1 || !af2)
		return 0;

	 
	if (ipv6_only_sock(sk) && af1 != af2)
		return 0;

	 
	if (sctp_is_any(sk, addr1) || sctp_is_any(sk, addr2))
		return 1;

	if (addr1->sa.sa_family == AF_INET && addr2->sa.sa_family == AF_INET)
		return addr1->v4.sin_addr.s_addr == addr2->v4.sin_addr.s_addr;

	return __sctp_v6_cmp_addr(addr1, addr2);
}

 
static int sctp_inet6_bind_verify(struct sctp_sock *opt, union sctp_addr *addr)
{
	struct sctp_af *af;

	 
	if (addr->sa.sa_family != AF_INET6)
		af = sctp_get_af_specific(addr->sa.sa_family);
	else {
		int type = ipv6_addr_type(&addr->v6.sin6_addr);
		struct net_device *dev;

		if (type & IPV6_ADDR_LINKLOCAL) {
			struct net *net;
			if (!addr->v6.sin6_scope_id)
				return 0;
			net = sock_net(&opt->inet.sk);
			rcu_read_lock();
			dev = dev_get_by_index_rcu(net, addr->v6.sin6_scope_id);
			if (!dev || !(ipv6_can_nonlocal_bind(net, &opt->inet) ||
				      ipv6_chk_addr(net, &addr->v6.sin6_addr,
						    dev, 0))) {
				rcu_read_unlock();
				return 0;
			}
			rcu_read_unlock();
		}

		af = opt->pf->af;
	}
	return af->available(addr, opt);
}

 
static int sctp_inet6_send_verify(struct sctp_sock *opt, union sctp_addr *addr)
{
	struct sctp_af *af = NULL;

	 
	if (addr->sa.sa_family != AF_INET6)
		af = sctp_get_af_specific(addr->sa.sa_family);
	else {
		int type = ipv6_addr_type(&addr->v6.sin6_addr);
		struct net_device *dev;

		if (type & IPV6_ADDR_LINKLOCAL) {
			if (!addr->v6.sin6_scope_id)
				return 0;
			rcu_read_lock();
			dev = dev_get_by_index_rcu(sock_net(&opt->inet.sk),
						   addr->v6.sin6_scope_id);
			rcu_read_unlock();
			if (!dev)
				return 0;
		}
		af = opt->pf->af;
	}

	return af != NULL;
}

 
static int sctp_inet6_supported_addrs(const struct sctp_sock *opt,
				      __be16 *types)
{
	types[0] = SCTP_PARAM_IPV6_ADDRESS;
	if (!opt || !ipv6_only_sock(sctp_opt2sk(opt))) {
		types[1] = SCTP_PARAM_IPV4_ADDRESS;
		return 2;
	}
	return 1;
}

 
static int sctp_getname(struct socket *sock, struct sockaddr *uaddr,
			int peer)
{
	int rc;

	rc = inet6_getname(sock, uaddr, peer);

	if (rc < 0)
		return rc;

	rc = sctp_v6_addr_to_user(sctp_sk(sock->sk),
					  (union sctp_addr *)uaddr);

	return rc;
}

static const struct proto_ops inet6_seqpacket_ops = {
	.family		   = PF_INET6,
	.owner		   = THIS_MODULE,
	.release	   = inet6_release,
	.bind		   = inet6_bind,
	.connect	   = sctp_inet_connect,
	.socketpair	   = sock_no_socketpair,
	.accept		   = inet_accept,
	.getname	   = sctp_getname,
	.poll		   = sctp_poll,
	.ioctl		   = inet6_ioctl,
	.gettstamp	   = sock_gettstamp,
	.listen		   = sctp_inet_listen,
	.shutdown	   = inet_shutdown,
	.setsockopt	   = sock_common_setsockopt,
	.getsockopt	   = sock_common_getsockopt,
	.sendmsg	   = inet_sendmsg,
	.recvmsg	   = inet_recvmsg,
	.mmap		   = sock_no_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl	   = inet6_compat_ioctl,
#endif
};

static struct inet_protosw sctpv6_seqpacket_protosw = {
	.type          = SOCK_SEQPACKET,
	.protocol      = IPPROTO_SCTP,
	.prot 	       = &sctpv6_prot,
	.ops           = &inet6_seqpacket_ops,
	.flags         = SCTP_PROTOSW_FLAG
};
static struct inet_protosw sctpv6_stream_protosw = {
	.type          = SOCK_STREAM,
	.protocol      = IPPROTO_SCTP,
	.prot 	       = &sctpv6_prot,
	.ops           = &inet6_seqpacket_ops,
	.flags         = SCTP_PROTOSW_FLAG,
};

static int sctp6_rcv(struct sk_buff *skb)
{
	SCTP_INPUT_CB(skb)->encap_port = 0;
	return sctp_rcv(skb) ? -1 : 0;
}

static const struct inet6_protocol sctpv6_protocol = {
	.handler      = sctp6_rcv,
	.err_handler  = sctp_v6_err,
	.flags        = INET6_PROTO_NOPOLICY | INET6_PROTO_FINAL,
};

static struct sctp_af sctp_af_inet6 = {
	.sa_family	   = AF_INET6,
	.sctp_xmit	   = sctp_v6_xmit,
	.setsockopt	   = ipv6_setsockopt,
	.getsockopt	   = ipv6_getsockopt,
	.get_dst	   = sctp_v6_get_dst,
	.get_saddr	   = sctp_v6_get_saddr,
	.copy_addrlist	   = sctp_v6_copy_addrlist,
	.from_skb	   = sctp_v6_from_skb,
	.from_sk	   = sctp_v6_from_sk,
	.from_addr_param   = sctp_v6_from_addr_param,
	.to_addr_param	   = sctp_v6_to_addr_param,
	.cmp_addr	   = sctp_v6_cmp_addr,
	.scope		   = sctp_v6_scope,
	.addr_valid	   = sctp_v6_addr_valid,
	.inaddr_any	   = sctp_v6_inaddr_any,
	.is_any		   = sctp_v6_is_any,
	.available	   = sctp_v6_available,
	.skb_iif	   = sctp_v6_skb_iif,
	.skb_sdif	   = sctp_v6_skb_sdif,
	.is_ce		   = sctp_v6_is_ce,
	.seq_dump_addr	   = sctp_v6_seq_dump_addr,
	.ecn_capable	   = sctp_v6_ecn_capable,
	.net_header_len	   = sizeof(struct ipv6hdr),
	.sockaddr_len	   = sizeof(struct sockaddr_in6),
	.ip_options_len	   = sctp_v6_ip_options_len,
};

static struct sctp_pf sctp_pf_inet6 = {
	.event_msgname = sctp_inet6_event_msgname,
	.skb_msgname   = sctp_inet6_skb_msgname,
	.af_supported  = sctp_inet6_af_supported,
	.cmp_addr      = sctp_inet6_cmp_addr,
	.bind_verify   = sctp_inet6_bind_verify,
	.send_verify   = sctp_inet6_send_verify,
	.supported_addrs = sctp_inet6_supported_addrs,
	.create_accept_sk = sctp_v6_create_accept_sk,
	.addr_to_user  = sctp_v6_addr_to_user,
	.to_sk_saddr   = sctp_v6_to_sk_saddr,
	.to_sk_daddr   = sctp_v6_to_sk_daddr,
	.copy_ip_options = sctp_v6_copy_ip_options,
	.af            = &sctp_af_inet6,
};

 
void sctp_v6_pf_init(void)
{
	 
	sctp_register_pf(&sctp_pf_inet6, PF_INET6);

	 
	sctp_register_af(&sctp_af_inet6);
}

void sctp_v6_pf_exit(void)
{
	list_del(&sctp_af_inet6.list);
}

 
int sctp_v6_protosw_init(void)
{
	int rc;

	rc = proto_register(&sctpv6_prot, 1);
	if (rc)
		return rc;

	 
	inet6_register_protosw(&sctpv6_seqpacket_protosw);
	inet6_register_protosw(&sctpv6_stream_protosw);

	return 0;
}

void sctp_v6_protosw_exit(void)
{
	inet6_unregister_protosw(&sctpv6_seqpacket_protosw);
	inet6_unregister_protosw(&sctpv6_stream_protosw);
	proto_unregister(&sctpv6_prot);
}


 
int sctp_v6_add_protocol(void)
{
	 
	register_inet6addr_notifier(&sctp_inet6addr_notifier);

	if (inet6_add_protocol(&sctpv6_protocol, IPPROTO_SCTP) < 0)
		return -EAGAIN;

	return 0;
}

 
void sctp_v6_del_protocol(void)
{
	inet6_del_protocol(&sctpv6_protocol, IPPROTO_SCTP);
	unregister_inet6addr_notifier(&sctp_inet6addr_notifier);
}

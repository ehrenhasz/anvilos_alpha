
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/fcntl.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/netfilter_ipv4.h>
#include <linux/slab.h>
#include <net/snmp.h>
#include <net/ip.h>
#include <net/route.h>
#include <net/protocol.h>
#include <net/icmp.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/raw.h>
#include <net/ping.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <net/checksum.h>
#include <net/xfrm.h>
#include <net/inet_common.h>
#include <net/ip_fib.h>
#include <net/l3mdev.h>

 

struct icmp_bxm {
	struct sk_buff *skb;
	int offset;
	int data_len;

	struct {
		struct icmphdr icmph;
		__be32	       times[3];
	} data;
	int head_len;
	struct ip_options_data replyopts;
};

 
 

const struct icmp_err icmp_err_convert[] = {
	{
		.errno = ENETUNREACH,	 
		.fatal = 0,
	},
	{
		.errno = EHOSTUNREACH,	 
		.fatal = 0,
	},
	{
		.errno = ENOPROTOOPT	 ,
		.fatal = 1,
	},
	{
		.errno = ECONNREFUSED,	 
		.fatal = 1,
	},
	{
		.errno = EMSGSIZE,	 
		.fatal = 0,
	},
	{
		.errno = EOPNOTSUPP,	 
		.fatal = 0,
	},
	{
		.errno = ENETUNREACH,	 
		.fatal = 1,
	},
	{
		.errno = EHOSTDOWN,	 
		.fatal = 1,
	},
	{
		.errno = ENONET,	 
		.fatal = 1,
	},
	{
		.errno = ENETUNREACH,	 
		.fatal = 1,
	},
	{
		.errno = EHOSTUNREACH,	 
		.fatal = 1,
	},
	{
		.errno = ENETUNREACH,	 
		.fatal = 0,
	},
	{
		.errno = EHOSTUNREACH,	 
		.fatal = 0,
	},
	{
		.errno = EHOSTUNREACH,	 
		.fatal = 1,
	},
	{
		.errno = EHOSTUNREACH,	 
		.fatal = 1,
	},
	{
		.errno = EHOSTUNREACH,	 
		.fatal = 1,
	},
};
EXPORT_SYMBOL(icmp_err_convert);

 

struct icmp_control {
	enum skb_drop_reason (*handler)(struct sk_buff *skb);
	short   error;		 
};

static const struct icmp_control icmp_pointers[NR_ICMP_TYPES+1];

static DEFINE_PER_CPU(struct sock *, ipv4_icmp_sk);

 
static inline struct sock *icmp_xmit_lock(struct net *net)
{
	struct sock *sk;

	sk = this_cpu_read(ipv4_icmp_sk);

	if (unlikely(!spin_trylock(&sk->sk_lock.slock))) {
		 
		return NULL;
	}
	sock_net_set(sk, net);
	return sk;
}

static inline void icmp_xmit_unlock(struct sock *sk)
{
	sock_net_set(sk, &init_net);
	spin_unlock(&sk->sk_lock.slock);
}

int sysctl_icmp_msgs_per_sec __read_mostly = 1000;
int sysctl_icmp_msgs_burst __read_mostly = 50;

static struct {
	spinlock_t	lock;
	u32		credit;
	u32		stamp;
} icmp_global = {
	.lock		= __SPIN_LOCK_UNLOCKED(icmp_global.lock),
};

 
bool icmp_global_allow(void)
{
	u32 credit, delta, incr = 0, now = (u32)jiffies;
	bool rc = false;

	 
	if (!READ_ONCE(icmp_global.credit)) {
		delta = min_t(u32, now - READ_ONCE(icmp_global.stamp), HZ);
		if (delta < HZ / 50)
			return false;
	}

	spin_lock(&icmp_global.lock);
	delta = min_t(u32, now - icmp_global.stamp, HZ);
	if (delta >= HZ / 50) {
		incr = READ_ONCE(sysctl_icmp_msgs_per_sec) * delta / HZ;
		if (incr)
			WRITE_ONCE(icmp_global.stamp, now);
	}
	credit = min_t(u32, icmp_global.credit + incr,
		       READ_ONCE(sysctl_icmp_msgs_burst));
	if (credit) {
		 
		credit = max_t(int, credit - get_random_u32_below(3), 0);
		rc = true;
	}
	WRITE_ONCE(icmp_global.credit, credit);
	spin_unlock(&icmp_global.lock);
	return rc;
}
EXPORT_SYMBOL(icmp_global_allow);

static bool icmpv4_mask_allow(struct net *net, int type, int code)
{
	if (type > NR_ICMP_TYPES)
		return true;

	 
	if (type == ICMP_DEST_UNREACH && code == ICMP_FRAG_NEEDED)
		return true;

	 
	if (!((1 << type) & READ_ONCE(net->ipv4.sysctl_icmp_ratemask)))
		return true;

	return false;
}

static bool icmpv4_global_allow(struct net *net, int type, int code)
{
	if (icmpv4_mask_allow(net, type, code))
		return true;

	if (icmp_global_allow())
		return true;

	__ICMP_INC_STATS(net, ICMP_MIB_RATELIMITGLOBAL);
	return false;
}

 

static bool icmpv4_xrlim_allow(struct net *net, struct rtable *rt,
			       struct flowi4 *fl4, int type, int code)
{
	struct dst_entry *dst = &rt->dst;
	struct inet_peer *peer;
	bool rc = true;
	int vif;

	if (icmpv4_mask_allow(net, type, code))
		goto out;

	 
	if (dst->dev && (dst->dev->flags&IFF_LOOPBACK))
		goto out;

	vif = l3mdev_master_ifindex(dst->dev);
	peer = inet_getpeer_v4(net->ipv4.peers, fl4->daddr, vif, 1);
	rc = inet_peer_xrlim_allow(peer,
				   READ_ONCE(net->ipv4.sysctl_icmp_ratelimit));
	if (peer)
		inet_putpeer(peer);
out:
	if (!rc)
		__ICMP_INC_STATS(net, ICMP_MIB_RATELIMITHOST);
	return rc;
}

 
void icmp_out_count(struct net *net, unsigned char type)
{
	ICMPMSGOUT_INC_STATS(net, type);
	ICMP_INC_STATS(net, ICMP_MIB_OUTMSGS);
}

 
static int icmp_glue_bits(void *from, char *to, int offset, int len, int odd,
			  struct sk_buff *skb)
{
	struct icmp_bxm *icmp_param = from;
	__wsum csum;

	csum = skb_copy_and_csum_bits(icmp_param->skb,
				      icmp_param->offset + offset,
				      to, len);

	skb->csum = csum_block_add(skb->csum, csum, odd);
	if (icmp_pointers[icmp_param->data.icmph.type].error)
		nf_ct_attach(skb, icmp_param->skb);
	return 0;
}

static void icmp_push_reply(struct sock *sk,
			    struct icmp_bxm *icmp_param,
			    struct flowi4 *fl4,
			    struct ipcm_cookie *ipc, struct rtable **rt)
{
	struct sk_buff *skb;

	if (ip_append_data(sk, fl4, icmp_glue_bits, icmp_param,
			   icmp_param->data_len+icmp_param->head_len,
			   icmp_param->head_len,
			   ipc, rt, MSG_DONTWAIT) < 0) {
		__ICMP_INC_STATS(sock_net(sk), ICMP_MIB_OUTERRORS);
		ip_flush_pending_frames(sk);
	} else if ((skb = skb_peek(&sk->sk_write_queue)) != NULL) {
		struct icmphdr *icmph = icmp_hdr(skb);
		__wsum csum;
		struct sk_buff *skb1;

		csum = csum_partial_copy_nocheck((void *)&icmp_param->data,
						 (char *)icmph,
						 icmp_param->head_len);
		skb_queue_walk(&sk->sk_write_queue, skb1) {
			csum = csum_add(csum, skb1->csum);
		}
		icmph->checksum = csum_fold(csum);
		skb->ip_summed = CHECKSUM_NONE;
		ip_push_pending_frames(sk, fl4);
	}
}

 

static void icmp_reply(struct icmp_bxm *icmp_param, struct sk_buff *skb)
{
	struct ipcm_cookie ipc;
	struct rtable *rt = skb_rtable(skb);
	struct net *net = dev_net(rt->dst.dev);
	struct flowi4 fl4;
	struct sock *sk;
	struct inet_sock *inet;
	__be32 daddr, saddr;
	u32 mark = IP4_REPLY_MARK(net, skb->mark);
	int type = icmp_param->data.icmph.type;
	int code = icmp_param->data.icmph.code;

	if (ip_options_echo(net, &icmp_param->replyopts.opt.opt, skb))
		return;

	 
	local_bh_disable();

	 
	if (!icmpv4_global_allow(net, type, code))
		goto out_bh_enable;

	sk = icmp_xmit_lock(net);
	if (!sk)
		goto out_bh_enable;
	inet = inet_sk(sk);

	icmp_param->data.icmph.checksum = 0;

	ipcm_init(&ipc);
	inet->tos = ip_hdr(skb)->tos;
	ipc.sockc.mark = mark;
	daddr = ipc.addr = ip_hdr(skb)->saddr;
	saddr = fib_compute_spec_dst(skb);

	if (icmp_param->replyopts.opt.opt.optlen) {
		ipc.opt = &icmp_param->replyopts.opt;
		if (ipc.opt->opt.srr)
			daddr = icmp_param->replyopts.opt.opt.faddr;
	}
	memset(&fl4, 0, sizeof(fl4));
	fl4.daddr = daddr;
	fl4.saddr = saddr;
	fl4.flowi4_mark = mark;
	fl4.flowi4_uid = sock_net_uid(net, NULL);
	fl4.flowi4_tos = RT_TOS(ip_hdr(skb)->tos);
	fl4.flowi4_proto = IPPROTO_ICMP;
	fl4.flowi4_oif = l3mdev_master_ifindex(skb->dev);
	security_skb_classify_flow(skb, flowi4_to_flowi_common(&fl4));
	rt = ip_route_output_key(net, &fl4);
	if (IS_ERR(rt))
		goto out_unlock;
	if (icmpv4_xrlim_allow(net, rt, &fl4, type, code))
		icmp_push_reply(sk, icmp_param, &fl4, &ipc, &rt);
	ip_rt_put(rt);
out_unlock:
	icmp_xmit_unlock(sk);
out_bh_enable:
	local_bh_enable();
}

 
static struct net_device *icmp_get_route_lookup_dev(struct sk_buff *skb)
{
	struct net_device *route_lookup_dev = NULL;

	if (skb->dev)
		route_lookup_dev = skb->dev;
	else if (skb_dst(skb))
		route_lookup_dev = skb_dst(skb)->dev;
	return route_lookup_dev;
}

static struct rtable *icmp_route_lookup(struct net *net,
					struct flowi4 *fl4,
					struct sk_buff *skb_in,
					const struct iphdr *iph,
					__be32 saddr, u8 tos, u32 mark,
					int type, int code,
					struct icmp_bxm *param)
{
	struct net_device *route_lookup_dev;
	struct rtable *rt, *rt2;
	struct flowi4 fl4_dec;
	int err;

	memset(fl4, 0, sizeof(*fl4));
	fl4->daddr = (param->replyopts.opt.opt.srr ?
		      param->replyopts.opt.opt.faddr : iph->saddr);
	fl4->saddr = saddr;
	fl4->flowi4_mark = mark;
	fl4->flowi4_uid = sock_net_uid(net, NULL);
	fl4->flowi4_tos = RT_TOS(tos);
	fl4->flowi4_proto = IPPROTO_ICMP;
	fl4->fl4_icmp_type = type;
	fl4->fl4_icmp_code = code;
	route_lookup_dev = icmp_get_route_lookup_dev(skb_in);
	fl4->flowi4_oif = l3mdev_master_ifindex(route_lookup_dev);

	security_skb_classify_flow(skb_in, flowi4_to_flowi_common(fl4));
	rt = ip_route_output_key_hash(net, fl4, skb_in);
	if (IS_ERR(rt))
		return rt;

	 
	rt2 = rt;

	rt = (struct rtable *) xfrm_lookup(net, &rt->dst,
					   flowi4_to_flowi(fl4), NULL, 0);
	if (!IS_ERR(rt)) {
		if (rt != rt2)
			return rt;
	} else if (PTR_ERR(rt) == -EPERM) {
		rt = NULL;
	} else
		return rt;

	err = xfrm_decode_session_reverse(skb_in, flowi4_to_flowi(&fl4_dec), AF_INET);
	if (err)
		goto relookup_failed;

	if (inet_addr_type_dev_table(net, route_lookup_dev,
				     fl4_dec.saddr) == RTN_LOCAL) {
		rt2 = __ip_route_output_key(net, &fl4_dec);
		if (IS_ERR(rt2))
			err = PTR_ERR(rt2);
	} else {
		struct flowi4 fl4_2 = {};
		unsigned long orefdst;

		fl4_2.daddr = fl4_dec.saddr;
		rt2 = ip_route_output_key(net, &fl4_2);
		if (IS_ERR(rt2)) {
			err = PTR_ERR(rt2);
			goto relookup_failed;
		}
		 
		orefdst = skb_in->_skb_refdst;  
		skb_dst_set(skb_in, NULL);
		err = ip_route_input(skb_in, fl4_dec.daddr, fl4_dec.saddr,
				     RT_TOS(tos), rt2->dst.dev);

		dst_release(&rt2->dst);
		rt2 = skb_rtable(skb_in);
		skb_in->_skb_refdst = orefdst;  
	}

	if (err)
		goto relookup_failed;

	rt2 = (struct rtable *) xfrm_lookup(net, &rt2->dst,
					    flowi4_to_flowi(&fl4_dec), NULL,
					    XFRM_LOOKUP_ICMP);
	if (!IS_ERR(rt2)) {
		dst_release(&rt->dst);
		memcpy(fl4, &fl4_dec, sizeof(*fl4));
		rt = rt2;
	} else if (PTR_ERR(rt2) == -EPERM) {
		if (rt)
			dst_release(&rt->dst);
		return rt2;
	} else {
		err = PTR_ERR(rt2);
		goto relookup_failed;
	}
	return rt;

relookup_failed:
	if (rt)
		return rt;
	return ERR_PTR(err);
}

 

void __icmp_send(struct sk_buff *skb_in, int type, int code, __be32 info,
		 const struct ip_options *opt)
{
	struct iphdr *iph;
	int room;
	struct icmp_bxm icmp_param;
	struct rtable *rt = skb_rtable(skb_in);
	struct ipcm_cookie ipc;
	struct flowi4 fl4;
	__be32 saddr;
	u8  tos;
	u32 mark;
	struct net *net;
	struct sock *sk;

	if (!rt)
		goto out;

	if (rt->dst.dev)
		net = dev_net(rt->dst.dev);
	else if (skb_in->dev)
		net = dev_net(skb_in->dev);
	else
		goto out;

	 
	iph = ip_hdr(skb_in);

	if ((u8 *)iph < skb_in->head ||
	    (skb_network_header(skb_in) + sizeof(*iph)) >
	    skb_tail_pointer(skb_in))
		goto out;

	 
	if (skb_in->pkt_type != PACKET_HOST)
		goto out;

	 
	if (rt->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
		goto out;

	 
	if (iph->frag_off & htons(IP_OFFSET))
		goto out;

	 
	if (icmp_pointers[type].error) {
		 
		if (iph->protocol == IPPROTO_ICMP) {
			u8 _inner_type, *itp;

			itp = skb_header_pointer(skb_in,
						 skb_network_header(skb_in) +
						 (iph->ihl << 2) +
						 offsetof(struct icmphdr,
							  type) -
						 skb_in->data,
						 sizeof(_inner_type),
						 &_inner_type);
			if (!itp)
				goto out;

			 
			if (*itp > NR_ICMP_TYPES ||
			    icmp_pointers[*itp].error)
				goto out;
		}
	}

	 
	local_bh_disable();

	 
	if (!(skb_in->dev && (skb_in->dev->flags&IFF_LOOPBACK)) &&
	      !icmpv4_global_allow(net, type, code))
		goto out_bh_enable;

	sk = icmp_xmit_lock(net);
	if (!sk)
		goto out_bh_enable;

	 

	saddr = iph->daddr;
	if (!(rt->rt_flags & RTCF_LOCAL)) {
		struct net_device *dev = NULL;

		rcu_read_lock();
		if (rt_is_input_route(rt) &&
		    READ_ONCE(net->ipv4.sysctl_icmp_errors_use_inbound_ifaddr))
			dev = dev_get_by_index_rcu(net, inet_iif(skb_in));

		if (dev)
			saddr = inet_select_addr(dev, iph->saddr,
						 RT_SCOPE_LINK);
		else
			saddr = 0;
		rcu_read_unlock();
	}

	tos = icmp_pointers[type].error ? (RT_TOS(iph->tos) |
					   IPTOS_PREC_INTERNETCONTROL) :
					   iph->tos;
	mark = IP4_REPLY_MARK(net, skb_in->mark);

	if (__ip_options_echo(net, &icmp_param.replyopts.opt.opt, skb_in, opt))
		goto out_unlock;


	 

	icmp_param.data.icmph.type	 = type;
	icmp_param.data.icmph.code	 = code;
	icmp_param.data.icmph.un.gateway = info;
	icmp_param.data.icmph.checksum	 = 0;
	icmp_param.skb	  = skb_in;
	icmp_param.offset = skb_network_offset(skb_in);
	inet_sk(sk)->tos = tos;
	ipcm_init(&ipc);
	ipc.addr = iph->saddr;
	ipc.opt = &icmp_param.replyopts.opt;
	ipc.sockc.mark = mark;

	rt = icmp_route_lookup(net, &fl4, skb_in, iph, saddr, tos, mark,
			       type, code, &icmp_param);
	if (IS_ERR(rt))
		goto out_unlock;

	 
	if (!icmpv4_xrlim_allow(net, rt, &fl4, type, code))
		goto ende;

	 

	room = dst_mtu(&rt->dst);
	if (room > 576)
		room = 576;
	room -= sizeof(struct iphdr) + icmp_param.replyopts.opt.opt.optlen;
	room -= sizeof(struct icmphdr);
	 
	if (room <= (int)sizeof(struct iphdr))
		goto ende;

	icmp_param.data_len = skb_in->len - icmp_param.offset;
	if (icmp_param.data_len > room)
		icmp_param.data_len = room;
	icmp_param.head_len = sizeof(struct icmphdr);

	 
	if (!fl4.saddr)
		fl4.saddr = htonl(INADDR_DUMMY);

	icmp_push_reply(sk, &icmp_param, &fl4, &ipc, &rt);
ende:
	ip_rt_put(rt);
out_unlock:
	icmp_xmit_unlock(sk);
out_bh_enable:
	local_bh_enable();
out:;
}
EXPORT_SYMBOL(__icmp_send);

#if IS_ENABLED(CONFIG_NF_NAT)
#include <net/netfilter/nf_conntrack.h>
void icmp_ndo_send(struct sk_buff *skb_in, int type, int code, __be32 info)
{
	struct sk_buff *cloned_skb = NULL;
	struct ip_options opts = { 0 };
	enum ip_conntrack_info ctinfo;
	struct nf_conn *ct;
	__be32 orig_ip;

	ct = nf_ct_get(skb_in, &ctinfo);
	if (!ct || !(ct->status & IPS_SRC_NAT)) {
		__icmp_send(skb_in, type, code, info, &opts);
		return;
	}

	if (skb_shared(skb_in))
		skb_in = cloned_skb = skb_clone(skb_in, GFP_ATOMIC);

	if (unlikely(!skb_in || skb_network_header(skb_in) < skb_in->head ||
	    (skb_network_header(skb_in) + sizeof(struct iphdr)) >
	    skb_tail_pointer(skb_in) || skb_ensure_writable(skb_in,
	    skb_network_offset(skb_in) + sizeof(struct iphdr))))
		goto out;

	orig_ip = ip_hdr(skb_in)->saddr;
	ip_hdr(skb_in)->saddr = ct->tuplehash[0].tuple.src.u3.ip;
	__icmp_send(skb_in, type, code, info, &opts);
	ip_hdr(skb_in)->saddr = orig_ip;
out:
	consume_skb(cloned_skb);
}
EXPORT_SYMBOL(icmp_ndo_send);
#endif

static void icmp_socket_deliver(struct sk_buff *skb, u32 info)
{
	const struct iphdr *iph = (const struct iphdr *)skb->data;
	const struct net_protocol *ipprot;
	int protocol = iph->protocol;

	 
	if (!pskb_may_pull(skb, iph->ihl * 4 + 8)) {
		__ICMP_INC_STATS(dev_net(skb->dev), ICMP_MIB_INERRORS);
		return;
	}

	raw_icmp_error(skb, protocol, info);

	ipprot = rcu_dereference(inet_protos[protocol]);
	if (ipprot && ipprot->err_handler)
		ipprot->err_handler(skb, info);
}

static bool icmp_tag_validation(int proto)
{
	bool ok;

	rcu_read_lock();
	ok = rcu_dereference(inet_protos[proto])->icmp_strict_tag_validation;
	rcu_read_unlock();
	return ok;
}

 

static enum skb_drop_reason icmp_unreach(struct sk_buff *skb)
{
	enum skb_drop_reason reason = SKB_NOT_DROPPED_YET;
	const struct iphdr *iph;
	struct icmphdr *icmph;
	struct net *net;
	u32 info = 0;

	net = dev_net(skb_dst(skb)->dev);

	 

	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto out_err;

	icmph = icmp_hdr(skb);
	iph   = (const struct iphdr *)skb->data;

	if (iph->ihl < 5)  {  
		reason = SKB_DROP_REASON_IP_INHDR;
		goto out_err;
	}

	switch (icmph->type) {
	case ICMP_DEST_UNREACH:
		switch (icmph->code & 15) {
		case ICMP_NET_UNREACH:
		case ICMP_HOST_UNREACH:
		case ICMP_PROT_UNREACH:
		case ICMP_PORT_UNREACH:
			break;
		case ICMP_FRAG_NEEDED:
			 
			switch (READ_ONCE(net->ipv4.sysctl_ip_no_pmtu_disc)) {
			default:
				net_dbg_ratelimited("%pI4: fragmentation needed and DF set\n",
						    &iph->daddr);
				break;
			case 2:
				goto out;
			case 3:
				if (!icmp_tag_validation(iph->protocol))
					goto out;
				fallthrough;
			case 0:
				info = ntohs(icmph->un.frag.mtu);
			}
			break;
		case ICMP_SR_FAILED:
			net_dbg_ratelimited("%pI4: Source Route Failed\n",
					    &iph->daddr);
			break;
		default:
			break;
		}
		if (icmph->code > NR_ICMP_UNREACH)
			goto out;
		break;
	case ICMP_PARAMETERPROB:
		info = ntohl(icmph->un.gateway) >> 24;
		break;
	case ICMP_TIME_EXCEEDED:
		__ICMP_INC_STATS(net, ICMP_MIB_INTIMEEXCDS);
		if (icmph->code == ICMP_EXC_FRAGTIME)
			goto out;
		break;
	}

	 

	 

	if (!READ_ONCE(net->ipv4.sysctl_icmp_ignore_bogus_error_responses) &&
	    inet_addr_type_dev_table(net, skb->dev, iph->daddr) == RTN_BROADCAST) {
		net_warn_ratelimited("%pI4 sent an invalid ICMP type %u, code %u error to a broadcast: %pI4 on %s\n",
				     &ip_hdr(skb)->saddr,
				     icmph->type, icmph->code,
				     &iph->daddr, skb->dev->name);
		goto out;
	}

	icmp_socket_deliver(skb, info);

out:
	return reason;
out_err:
	__ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
	return reason ?: SKB_DROP_REASON_NOT_SPECIFIED;
}


 

static enum skb_drop_reason icmp_redirect(struct sk_buff *skb)
{
	if (skb->len < sizeof(struct iphdr)) {
		__ICMP_INC_STATS(dev_net(skb->dev), ICMP_MIB_INERRORS);
		return SKB_DROP_REASON_PKT_TOO_SMALL;
	}

	if (!pskb_may_pull(skb, sizeof(struct iphdr))) {
		 
		return SKB_DROP_REASON_NOMEM;
	}

	icmp_socket_deliver(skb, ntohl(icmp_hdr(skb)->un.gateway));
	return SKB_NOT_DROPPED_YET;
}

 

static enum skb_drop_reason icmp_echo(struct sk_buff *skb)
{
	struct icmp_bxm icmp_param;
	struct net *net;

	net = dev_net(skb_dst(skb)->dev);
	 
	if (READ_ONCE(net->ipv4.sysctl_icmp_echo_ignore_all))
		return SKB_NOT_DROPPED_YET;

	icmp_param.data.icmph	   = *icmp_hdr(skb);
	icmp_param.skb		   = skb;
	icmp_param.offset	   = 0;
	icmp_param.data_len	   = skb->len;
	icmp_param.head_len	   = sizeof(struct icmphdr);

	if (icmp_param.data.icmph.type == ICMP_ECHO)
		icmp_param.data.icmph.type = ICMP_ECHOREPLY;
	else if (!icmp_build_probe(skb, &icmp_param.data.icmph))
		return SKB_NOT_DROPPED_YET;

	icmp_reply(&icmp_param, skb);
	return SKB_NOT_DROPPED_YET;
}

 

bool icmp_build_probe(struct sk_buff *skb, struct icmphdr *icmphdr)
{
	struct icmp_ext_hdr *ext_hdr, _ext_hdr;
	struct icmp_ext_echo_iio *iio, _iio;
	struct net *net = dev_net(skb->dev);
	struct net_device *dev;
	char buff[IFNAMSIZ];
	u16 ident_len;
	u8 status;

	if (!READ_ONCE(net->ipv4.sysctl_icmp_echo_enable_probe))
		return false;

	 
	if (!(ntohs(icmphdr->un.echo.sequence) & 1))
		return false;
	 
	icmphdr->un.echo.sequence &= htons(0xFF00);
	if (icmphdr->type == ICMP_EXT_ECHO)
		icmphdr->type = ICMP_EXT_ECHOREPLY;
	else
		icmphdr->type = ICMPV6_EXT_ECHO_REPLY;
	ext_hdr = skb_header_pointer(skb, 0, sizeof(_ext_hdr), &_ext_hdr);
	 
	iio = skb_header_pointer(skb, sizeof(_ext_hdr), sizeof(iio->extobj_hdr), &_iio);
	if (!ext_hdr || !iio)
		goto send_mal_query;
	if (ntohs(iio->extobj_hdr.length) <= sizeof(iio->extobj_hdr) ||
	    ntohs(iio->extobj_hdr.length) > sizeof(_iio))
		goto send_mal_query;
	ident_len = ntohs(iio->extobj_hdr.length) - sizeof(iio->extobj_hdr);
	iio = skb_header_pointer(skb, sizeof(_ext_hdr),
				 sizeof(iio->extobj_hdr) + ident_len, &_iio);
	if (!iio)
		goto send_mal_query;

	status = 0;
	dev = NULL;
	switch (iio->extobj_hdr.class_type) {
	case ICMP_EXT_ECHO_CTYPE_NAME:
		if (ident_len >= IFNAMSIZ)
			goto send_mal_query;
		memset(buff, 0, sizeof(buff));
		memcpy(buff, &iio->ident.name, ident_len);
		dev = dev_get_by_name(net, buff);
		break;
	case ICMP_EXT_ECHO_CTYPE_INDEX:
		if (ident_len != sizeof(iio->ident.ifindex))
			goto send_mal_query;
		dev = dev_get_by_index(net, ntohl(iio->ident.ifindex));
		break;
	case ICMP_EXT_ECHO_CTYPE_ADDR:
		if (ident_len < sizeof(iio->ident.addr.ctype3_hdr) ||
		    ident_len != sizeof(iio->ident.addr.ctype3_hdr) +
				 iio->ident.addr.ctype3_hdr.addrlen)
			goto send_mal_query;
		switch (ntohs(iio->ident.addr.ctype3_hdr.afi)) {
		case ICMP_AFI_IP:
			if (iio->ident.addr.ctype3_hdr.addrlen != sizeof(struct in_addr))
				goto send_mal_query;
			dev = ip_dev_find(net, iio->ident.addr.ip_addr.ipv4_addr);
			break;
#if IS_ENABLED(CONFIG_IPV6)
		case ICMP_AFI_IP6:
			if (iio->ident.addr.ctype3_hdr.addrlen != sizeof(struct in6_addr))
				goto send_mal_query;
			dev = ipv6_stub->ipv6_dev_find(net, &iio->ident.addr.ip_addr.ipv6_addr, dev);
			dev_hold(dev);
			break;
#endif
		default:
			goto send_mal_query;
		}
		break;
	default:
		goto send_mal_query;
	}
	if (!dev) {
		icmphdr->code = ICMP_EXT_CODE_NO_IF;
		return true;
	}
	 
	if (dev->flags & IFF_UP)
		status |= ICMP_EXT_ECHOREPLY_ACTIVE;
	if (__in_dev_get_rcu(dev) && __in_dev_get_rcu(dev)->ifa_list)
		status |= ICMP_EXT_ECHOREPLY_IPV4;
	if (!list_empty(&rcu_dereference(dev->ip6_ptr)->addr_list))
		status |= ICMP_EXT_ECHOREPLY_IPV6;
	dev_put(dev);
	icmphdr->un.echo.sequence |= htons(status);
	return true;
send_mal_query:
	icmphdr->code = ICMP_EXT_CODE_MAL_QUERY;
	return true;
}
EXPORT_SYMBOL_GPL(icmp_build_probe);

 
static enum skb_drop_reason icmp_timestamp(struct sk_buff *skb)
{
	struct icmp_bxm icmp_param;
	 
	if (skb->len < 4)
		goto out_err;

	 
	icmp_param.data.times[1] = inet_current_timestamp();
	icmp_param.data.times[2] = icmp_param.data.times[1];

	BUG_ON(skb_copy_bits(skb, 0, &icmp_param.data.times[0], 4));

	icmp_param.data.icmph	   = *icmp_hdr(skb);
	icmp_param.data.icmph.type = ICMP_TIMESTAMPREPLY;
	icmp_param.data.icmph.code = 0;
	icmp_param.skb		   = skb;
	icmp_param.offset	   = 0;
	icmp_param.data_len	   = 0;
	icmp_param.head_len	   = sizeof(struct icmphdr) + 12;
	icmp_reply(&icmp_param, skb);
	return SKB_NOT_DROPPED_YET;

out_err:
	__ICMP_INC_STATS(dev_net(skb_dst(skb)->dev), ICMP_MIB_INERRORS);
	return SKB_DROP_REASON_PKT_TOO_SMALL;
}

static enum skb_drop_reason icmp_discard(struct sk_buff *skb)
{
	 
	return SKB_NOT_DROPPED_YET;
}

 
int icmp_rcv(struct sk_buff *skb)
{
	enum skb_drop_reason reason = SKB_DROP_REASON_NOT_SPECIFIED;
	struct rtable *rt = skb_rtable(skb);
	struct net *net = dev_net(rt->dst.dev);
	struct icmphdr *icmph;

	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb)) {
		struct sec_path *sp = skb_sec_path(skb);
		int nh;

		if (!(sp && sp->xvec[sp->len - 1]->props.flags &
				 XFRM_STATE_ICMP)) {
			reason = SKB_DROP_REASON_XFRM_POLICY;
			goto drop;
		}

		if (!pskb_may_pull(skb, sizeof(*icmph) + sizeof(struct iphdr)))
			goto drop;

		nh = skb_network_offset(skb);
		skb_set_network_header(skb, sizeof(*icmph));

		if (!xfrm4_policy_check_reverse(NULL, XFRM_POLICY_IN,
						skb)) {
			reason = SKB_DROP_REASON_XFRM_POLICY;
			goto drop;
		}

		skb_set_network_header(skb, nh);
	}

	__ICMP_INC_STATS(net, ICMP_MIB_INMSGS);

	if (skb_checksum_simple_validate(skb))
		goto csum_error;

	if (!pskb_pull(skb, sizeof(*icmph)))
		goto error;

	icmph = icmp_hdr(skb);

	ICMPMSGIN_INC_STATS(net, icmph->type);

	 
	if (icmph->type == ICMP_EXT_ECHO) {
		 
		reason = icmp_echo(skb);
		goto reason_check;
	}

	if (icmph->type == ICMP_EXT_ECHOREPLY) {
		reason = ping_rcv(skb);
		goto reason_check;
	}

	 
	if (icmph->type > NR_ICMP_TYPES) {
		reason = SKB_DROP_REASON_UNHANDLED_PROTO;
		goto error;
	}

	 

	if (rt->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST)) {
		 
		if ((icmph->type == ICMP_ECHO ||
		     icmph->type == ICMP_TIMESTAMP) &&
		    READ_ONCE(net->ipv4.sysctl_icmp_echo_ignore_broadcasts)) {
			reason = SKB_DROP_REASON_INVALID_PROTO;
			goto error;
		}
		if (icmph->type != ICMP_ECHO &&
		    icmph->type != ICMP_TIMESTAMP &&
		    icmph->type != ICMP_ADDRESS &&
		    icmph->type != ICMP_ADDRESSREPLY) {
			reason = SKB_DROP_REASON_INVALID_PROTO;
			goto error;
		}
	}

	reason = icmp_pointers[icmph->type].handler(skb);
reason_check:
	if (!reason)  {
		consume_skb(skb);
		return NET_RX_SUCCESS;
	}

drop:
	kfree_skb_reason(skb, reason);
	return NET_RX_DROP;
csum_error:
	reason = SKB_DROP_REASON_ICMP_CSUM;
	__ICMP_INC_STATS(net, ICMP_MIB_CSUMERRORS);
error:
	__ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
	goto drop;
}

static bool ip_icmp_error_rfc4884_validate(const struct sk_buff *skb, int off)
{
	struct icmp_extobj_hdr *objh, _objh;
	struct icmp_ext_hdr *exth, _exth;
	u16 olen;

	exth = skb_header_pointer(skb, off, sizeof(_exth), &_exth);
	if (!exth)
		return false;
	if (exth->version != 2)
		return true;

	if (exth->checksum &&
	    csum_fold(skb_checksum(skb, off, skb->len - off, 0)))
		return false;

	off += sizeof(_exth);
	while (off < skb->len) {
		objh = skb_header_pointer(skb, off, sizeof(_objh), &_objh);
		if (!objh)
			return false;

		olen = ntohs(objh->length);
		if (olen < sizeof(_objh))
			return false;

		off += olen;
		if (off > skb->len)
			return false;
	}

	return true;
}

void ip_icmp_error_rfc4884(const struct sk_buff *skb,
			   struct sock_ee_data_rfc4884 *out,
			   int thlen, int off)
{
	int hlen;

	 
	hlen = -skb_transport_offset(skb) - thlen;

	 
	if (off < 128 || off < hlen)
		return;

	 
	off -= hlen;
	if (off + sizeof(struct icmp_ext_hdr) > skb->len)
		return;

	out->len = off;

	if (!ip_icmp_error_rfc4884_validate(skb, off))
		out->flags |= SO_EE_RFC4884_FLAG_INVALID;
}
EXPORT_SYMBOL_GPL(ip_icmp_error_rfc4884);

int icmp_err(struct sk_buff *skb, u32 info)
{
	struct iphdr *iph = (struct iphdr *)skb->data;
	int offset = iph->ihl<<2;
	struct icmphdr *icmph = (struct icmphdr *)(skb->data + offset);
	int type = icmp_hdr(skb)->type;
	int code = icmp_hdr(skb)->code;
	struct net *net = dev_net(skb->dev);

	 
	if (icmph->type != ICMP_ECHOREPLY) {
		ping_err(skb, offset, info);
		return 0;
	}

	if (type == ICMP_DEST_UNREACH && code == ICMP_FRAG_NEEDED)
		ipv4_update_pmtu(skb, net, info, 0, IPPROTO_ICMP);
	else if (type == ICMP_REDIRECT)
		ipv4_redirect(skb, net, 0, IPPROTO_ICMP);

	return 0;
}

 
static const struct icmp_control icmp_pointers[NR_ICMP_TYPES + 1] = {
	[ICMP_ECHOREPLY] = {
		.handler = ping_rcv,
	},
	[1] = {
		.handler = icmp_discard,
		.error = 1,
	},
	[2] = {
		.handler = icmp_discard,
		.error = 1,
	},
	[ICMP_DEST_UNREACH] = {
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_SOURCE_QUENCH] = {
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_REDIRECT] = {
		.handler = icmp_redirect,
		.error = 1,
	},
	[6] = {
		.handler = icmp_discard,
		.error = 1,
	},
	[7] = {
		.handler = icmp_discard,
		.error = 1,
	},
	[ICMP_ECHO] = {
		.handler = icmp_echo,
	},
	[9] = {
		.handler = icmp_discard,
		.error = 1,
	},
	[10] = {
		.handler = icmp_discard,
		.error = 1,
	},
	[ICMP_TIME_EXCEEDED] = {
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_PARAMETERPROB] = {
		.handler = icmp_unreach,
		.error = 1,
	},
	[ICMP_TIMESTAMP] = {
		.handler = icmp_timestamp,
	},
	[ICMP_TIMESTAMPREPLY] = {
		.handler = icmp_discard,
	},
	[ICMP_INFO_REQUEST] = {
		.handler = icmp_discard,
	},
	[ICMP_INFO_REPLY] = {
		.handler = icmp_discard,
	},
	[ICMP_ADDRESS] = {
		.handler = icmp_discard,
	},
	[ICMP_ADDRESSREPLY] = {
		.handler = icmp_discard,
	},
};

static int __net_init icmp_sk_init(struct net *net)
{
	 
	net->ipv4.sysctl_icmp_echo_ignore_all = 0;
	net->ipv4.sysctl_icmp_echo_enable_probe = 0;
	net->ipv4.sysctl_icmp_echo_ignore_broadcasts = 1;

	 
	net->ipv4.sysctl_icmp_ignore_bogus_error_responses = 1;

	 

	net->ipv4.sysctl_icmp_ratelimit = 1 * HZ;
	net->ipv4.sysctl_icmp_ratemask = 0x1818;
	net->ipv4.sysctl_icmp_errors_use_inbound_ifaddr = 0;

	return 0;
}

static struct pernet_operations __net_initdata icmp_sk_ops = {
       .init = icmp_sk_init,
};

int __init icmp_init(void)
{
	int err, i;

	for_each_possible_cpu(i) {
		struct sock *sk;

		err = inet_ctl_sock_create(&sk, PF_INET,
					   SOCK_RAW, IPPROTO_ICMP, &init_net);
		if (err < 0)
			return err;

		per_cpu(ipv4_icmp_sk, i) = sk;

		 
		sk->sk_sndbuf =	2 * SKB_TRUESIZE(64 * 1024);

		 
		sock_set_flag(sk, SOCK_USE_WRITE_QUEUE);
		inet_sk(sk)->pmtudisc = IP_PMTUDISC_DONT;
	}
	return register_pernet_subsys(&icmp_sk_ops);
}

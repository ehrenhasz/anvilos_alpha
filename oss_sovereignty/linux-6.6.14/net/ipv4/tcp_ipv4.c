
 

 

#define pr_fmt(fmt) "TCP: " fmt

#include <linux/bottom_half.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/cache.h>
#include <linux/jhash.h>
#include <linux/init.h>
#include <linux/times.h>
#include <linux/slab.h>
#include <linux/sched.h>

#include <net/net_namespace.h>
#include <net/icmp.h>
#include <net/inet_hashtables.h>
#include <net/tcp.h>
#include <net/transp_v6.h>
#include <net/ipv6.h>
#include <net/inet_common.h>
#include <net/timewait_sock.h>
#include <net/xfrm.h>
#include <net/secure_seq.h>
#include <net/busy_poll.h>

#include <linux/inet.h>
#include <linux/ipv6.h>
#include <linux/stddef.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/inetdevice.h>
#include <linux/btf_ids.h>

#include <crypto/hash.h>
#include <linux/scatterlist.h>

#include <trace/events/tcp.h>

#ifdef CONFIG_TCP_MD5SIG
static int tcp_v4_md5_hash_hdr(char *md5_hash, const struct tcp_md5sig_key *key,
			       __be32 daddr, __be32 saddr, const struct tcphdr *th);
#endif

struct inet_hashinfo tcp_hashinfo;
EXPORT_SYMBOL(tcp_hashinfo);

static DEFINE_PER_CPU(struct sock *, ipv4_tcp_sk);

static u32 tcp_v4_init_seq(const struct sk_buff *skb)
{
	return secure_tcp_seq(ip_hdr(skb)->daddr,
			      ip_hdr(skb)->saddr,
			      tcp_hdr(skb)->dest,
			      tcp_hdr(skb)->source);
}

static u32 tcp_v4_init_ts_off(const struct net *net, const struct sk_buff *skb)
{
	return secure_tcp_ts_off(net, ip_hdr(skb)->daddr, ip_hdr(skb)->saddr);
}

int tcp_twsk_unique(struct sock *sk, struct sock *sktw, void *twp)
{
	int reuse = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_tw_reuse);
	const struct inet_timewait_sock *tw = inet_twsk(sktw);
	const struct tcp_timewait_sock *tcptw = tcp_twsk(sktw);
	struct tcp_sock *tp = tcp_sk(sk);

	if (reuse == 2) {
		 
		bool loopback = false;
		if (tw->tw_bound_dev_if == LOOPBACK_IFINDEX)
			loopback = true;
#if IS_ENABLED(CONFIG_IPV6)
		if (tw->tw_family == AF_INET6) {
			if (ipv6_addr_loopback(&tw->tw_v6_daddr) ||
			    ipv6_addr_v4mapped_loopback(&tw->tw_v6_daddr) ||
			    ipv6_addr_loopback(&tw->tw_v6_rcv_saddr) ||
			    ipv6_addr_v4mapped_loopback(&tw->tw_v6_rcv_saddr))
				loopback = true;
		} else
#endif
		{
			if (ipv4_is_loopback(tw->tw_daddr) ||
			    ipv4_is_loopback(tw->tw_rcv_saddr))
				loopback = true;
		}
		if (!loopback)
			reuse = 0;
	}

	 
	if (tcptw->tw_ts_recent_stamp &&
	    (!twp || (reuse && time_after32(ktime_get_seconds(),
					    tcptw->tw_ts_recent_stamp)))) {
		 
		if (likely(!tp->repair)) {
			u32 seq = tcptw->tw_snd_nxt + 65535 + 2;

			if (!seq)
				seq = 1;
			WRITE_ONCE(tp->write_seq, seq);
			tp->rx_opt.ts_recent	   = tcptw->tw_ts_recent;
			tp->rx_opt.ts_recent_stamp = tcptw->tw_ts_recent_stamp;
		}
		sock_hold(sktw);
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(tcp_twsk_unique);

static int tcp_v4_pre_connect(struct sock *sk, struct sockaddr *uaddr,
			      int addr_len)
{
	 
	if (addr_len < sizeof(struct sockaddr_in))
		return -EINVAL;

	sock_owned_by_me(sk);

	return BPF_CGROUP_RUN_PROG_INET4_CONNECT(sk, uaddr);
}

 
int tcp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len)
{
	struct sockaddr_in *usin = (struct sockaddr_in *)uaddr;
	struct inet_timewait_death_row *tcp_death_row;
	struct inet_sock *inet = inet_sk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct ip_options_rcu *inet_opt;
	struct net *net = sock_net(sk);
	__be16 orig_sport, orig_dport;
	__be32 daddr, nexthop;
	struct flowi4 *fl4;
	struct rtable *rt;
	int err;

	if (addr_len < sizeof(struct sockaddr_in))
		return -EINVAL;

	if (usin->sin_family != AF_INET)
		return -EAFNOSUPPORT;

	nexthop = daddr = usin->sin_addr.s_addr;
	inet_opt = rcu_dereference_protected(inet->inet_opt,
					     lockdep_sock_is_held(sk));
	if (inet_opt && inet_opt->opt.srr) {
		if (!daddr)
			return -EINVAL;
		nexthop = inet_opt->opt.faddr;
	}

	orig_sport = inet->inet_sport;
	orig_dport = usin->sin_port;
	fl4 = &inet->cork.fl.u.ip4;
	rt = ip_route_connect(fl4, nexthop, inet->inet_saddr,
			      sk->sk_bound_dev_if, IPPROTO_TCP, orig_sport,
			      orig_dport, sk);
	if (IS_ERR(rt)) {
		err = PTR_ERR(rt);
		if (err == -ENETUNREACH)
			IP_INC_STATS(net, IPSTATS_MIB_OUTNOROUTES);
		return err;
	}

	if (rt->rt_flags & (RTCF_MULTICAST | RTCF_BROADCAST)) {
		ip_rt_put(rt);
		return -ENETUNREACH;
	}

	if (!inet_opt || !inet_opt->opt.srr)
		daddr = fl4->daddr;

	tcp_death_row = &sock_net(sk)->ipv4.tcp_death_row;

	if (!inet->inet_saddr) {
		err = inet_bhash2_update_saddr(sk,  &fl4->saddr, AF_INET);
		if (err) {
			ip_rt_put(rt);
			return err;
		}
	} else {
		sk_rcv_saddr_set(sk, inet->inet_saddr);
	}

	if (tp->rx_opt.ts_recent_stamp && inet->inet_daddr != daddr) {
		 
		tp->rx_opt.ts_recent	   = 0;
		tp->rx_opt.ts_recent_stamp = 0;
		if (likely(!tp->repair))
			WRITE_ONCE(tp->write_seq, 0);
	}

	inet->inet_dport = usin->sin_port;
	sk_daddr_set(sk, daddr);

	inet_csk(sk)->icsk_ext_hdr_len = 0;
	if (inet_opt)
		inet_csk(sk)->icsk_ext_hdr_len = inet_opt->opt.optlen;

	tp->rx_opt.mss_clamp = TCP_MSS_DEFAULT;

	 
	tcp_set_state(sk, TCP_SYN_SENT);
	err = inet_hash_connect(tcp_death_row, sk);
	if (err)
		goto failure;

	sk_set_txhash(sk);

	rt = ip_route_newports(fl4, rt, orig_sport, orig_dport,
			       inet->inet_sport, inet->inet_dport, sk);
	if (IS_ERR(rt)) {
		err = PTR_ERR(rt);
		rt = NULL;
		goto failure;
	}
	 
	sk->sk_gso_type = SKB_GSO_TCPV4;
	sk_setup_caps(sk, &rt->dst);
	rt = NULL;

	if (likely(!tp->repair)) {
		if (!tp->write_seq)
			WRITE_ONCE(tp->write_seq,
				   secure_tcp_seq(inet->inet_saddr,
						  inet->inet_daddr,
						  inet->inet_sport,
						  usin->sin_port));
		WRITE_ONCE(tp->tsoffset,
			   secure_tcp_ts_off(net, inet->inet_saddr,
					     inet->inet_daddr));
	}

	atomic_set(&inet->inet_id, get_random_u16());

	if (tcp_fastopen_defer_connect(sk, &err))
		return err;
	if (err)
		goto failure;

	err = tcp_connect(sk);

	if (err)
		goto failure;

	return 0;

failure:
	 
	tcp_set_state(sk, TCP_CLOSE);
	inet_bhash2_reset_saddr(sk);
	ip_rt_put(rt);
	sk->sk_route_caps = 0;
	inet->inet_dport = 0;
	return err;
}
EXPORT_SYMBOL(tcp_v4_connect);

 
void tcp_v4_mtu_reduced(struct sock *sk)
{
	struct inet_sock *inet = inet_sk(sk);
	struct dst_entry *dst;
	u32 mtu;

	if ((1 << sk->sk_state) & (TCPF_LISTEN | TCPF_CLOSE))
		return;
	mtu = READ_ONCE(tcp_sk(sk)->mtu_info);
	dst = inet_csk_update_pmtu(sk, mtu);
	if (!dst)
		return;

	 
	if (mtu < dst_mtu(dst) && ip_dont_fragment(sk, dst))
		WRITE_ONCE(sk->sk_err_soft, EMSGSIZE);

	mtu = dst_mtu(dst);

	if (inet->pmtudisc != IP_PMTUDISC_DONT &&
	    ip_sk_accept_pmtu(sk) &&
	    inet_csk(sk)->icsk_pmtu_cookie > mtu) {
		tcp_sync_mss(sk, mtu);

		 
		tcp_simple_retransmit(sk);
	}  
}
EXPORT_SYMBOL(tcp_v4_mtu_reduced);

static void do_redirect(struct sk_buff *skb, struct sock *sk)
{
	struct dst_entry *dst = __sk_dst_check(sk, 0);

	if (dst)
		dst->ops->redirect(dst, sk, skb);
}


 
void tcp_req_err(struct sock *sk, u32 seq, bool abort)
{
	struct request_sock *req = inet_reqsk(sk);
	struct net *net = sock_net(sk);

	 
	if (seq != tcp_rsk(req)->snt_isn) {
		__NET_INC_STATS(net, LINUX_MIB_OUTOFWINDOWICMPS);
	} else if (abort) {
		 
		inet_csk_reqsk_queue_drop(req->rsk_listener, req);
		tcp_listendrop(req->rsk_listener);
	}
	reqsk_put(req);
}
EXPORT_SYMBOL(tcp_req_err);

 
void tcp_ld_RTO_revert(struct sock *sk, u32 seq)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct sk_buff *skb;
	s32 remaining;
	u32 delta_us;

	if (sock_owned_by_user(sk))
		return;

	if (seq != tp->snd_una  || !icsk->icsk_retransmits ||
	    !icsk->icsk_backoff)
		return;

	skb = tcp_rtx_queue_head(sk);
	if (WARN_ON_ONCE(!skb))
		return;

	icsk->icsk_backoff--;
	icsk->icsk_rto = tp->srtt_us ? __tcp_set_rto(tp) : TCP_TIMEOUT_INIT;
	icsk->icsk_rto = inet_csk_rto_backoff(icsk, TCP_RTO_MAX);

	tcp_mstamp_refresh(tp);
	delta_us = (u32)(tp->tcp_mstamp - tcp_skb_timestamp_us(skb));
	remaining = icsk->icsk_rto - usecs_to_jiffies(delta_us);

	if (remaining > 0) {
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  remaining, TCP_RTO_MAX);
	} else {
		 
		tcp_retransmit_timer(sk);
	}
}
EXPORT_SYMBOL(tcp_ld_RTO_revert);

 

int tcp_v4_err(struct sk_buff *skb, u32 info)
{
	const struct iphdr *iph = (const struct iphdr *)skb->data;
	struct tcphdr *th = (struct tcphdr *)(skb->data + (iph->ihl << 2));
	struct tcp_sock *tp;
	const int type = icmp_hdr(skb)->type;
	const int code = icmp_hdr(skb)->code;
	struct sock *sk;
	struct request_sock *fastopen;
	u32 seq, snd_una;
	int err;
	struct net *net = dev_net(skb->dev);

	sk = __inet_lookup_established(net, net->ipv4.tcp_death_row.hashinfo,
				       iph->daddr, th->dest, iph->saddr,
				       ntohs(th->source), inet_iif(skb), 0);
	if (!sk) {
		__ICMP_INC_STATS(net, ICMP_MIB_INERRORS);
		return -ENOENT;
	}
	if (sk->sk_state == TCP_TIME_WAIT) {
		inet_twsk_put(inet_twsk(sk));
		return 0;
	}
	seq = ntohl(th->seq);
	if (sk->sk_state == TCP_NEW_SYN_RECV) {
		tcp_req_err(sk, seq, type == ICMP_PARAMETERPROB ||
				     type == ICMP_TIME_EXCEEDED ||
				     (type == ICMP_DEST_UNREACH &&
				      (code == ICMP_NET_UNREACH ||
				       code == ICMP_HOST_UNREACH)));
		return 0;
	}

	bh_lock_sock(sk);
	 
	if (sock_owned_by_user(sk)) {
		if (!(type == ICMP_DEST_UNREACH && code == ICMP_FRAG_NEEDED))
			__NET_INC_STATS(net, LINUX_MIB_LOCKDROPPEDICMPS);
	}
	if (sk->sk_state == TCP_CLOSE)
		goto out;

	if (static_branch_unlikely(&ip4_min_ttl)) {
		 
		if (unlikely(iph->ttl < READ_ONCE(inet_sk(sk)->min_ttl))) {
			__NET_INC_STATS(net, LINUX_MIB_TCPMINTTLDROP);
			goto out;
		}
	}

	tp = tcp_sk(sk);
	 
	fastopen = rcu_dereference(tp->fastopen_rsk);
	snd_una = fastopen ? tcp_rsk(fastopen)->snt_isn : tp->snd_una;
	if (sk->sk_state != TCP_LISTEN &&
	    !between(seq, snd_una, tp->snd_nxt)) {
		__NET_INC_STATS(net, LINUX_MIB_OUTOFWINDOWICMPS);
		goto out;
	}

	switch (type) {
	case ICMP_REDIRECT:
		if (!sock_owned_by_user(sk))
			do_redirect(skb, sk);
		goto out;
	case ICMP_SOURCE_QUENCH:
		 
		goto out;
	case ICMP_PARAMETERPROB:
		err = EPROTO;
		break;
	case ICMP_DEST_UNREACH:
		if (code > NR_ICMP_UNREACH)
			goto out;

		if (code == ICMP_FRAG_NEEDED) {  
			 
			if (sk->sk_state == TCP_LISTEN)
				goto out;

			WRITE_ONCE(tp->mtu_info, info);
			if (!sock_owned_by_user(sk)) {
				tcp_v4_mtu_reduced(sk);
			} else {
				if (!test_and_set_bit(TCP_MTU_REDUCED_DEFERRED, &sk->sk_tsq_flags))
					sock_hold(sk);
			}
			goto out;
		}

		err = icmp_err_convert[code].errno;
		 
		if (!fastopen &&
		    (code == ICMP_NET_UNREACH || code == ICMP_HOST_UNREACH))
			tcp_ld_RTO_revert(sk, seq);
		break;
	case ICMP_TIME_EXCEEDED:
		err = EHOSTUNREACH;
		break;
	default:
		goto out;
	}

	switch (sk->sk_state) {
	case TCP_SYN_SENT:
	case TCP_SYN_RECV:
		 
		if (fastopen && !fastopen->sk)
			break;

		ip_icmp_error(sk, skb, err, th->dest, info, (u8 *)th);

		if (!sock_owned_by_user(sk)) {
			WRITE_ONCE(sk->sk_err, err);

			sk_error_report(sk);

			tcp_done(sk);
		} else {
			WRITE_ONCE(sk->sk_err_soft, err);
		}
		goto out;
	}

	 

	if (!sock_owned_by_user(sk) &&
	    inet_test_bit(RECVERR, sk)) {
		WRITE_ONCE(sk->sk_err, err);
		sk_error_report(sk);
	} else	{  
		WRITE_ONCE(sk->sk_err_soft, err);
	}

out:
	bh_unlock_sock(sk);
	sock_put(sk);
	return 0;
}

void __tcp_v4_send_check(struct sk_buff *skb, __be32 saddr, __be32 daddr)
{
	struct tcphdr *th = tcp_hdr(skb);

	th->check = ~tcp_v4_check(skb->len, saddr, daddr, 0);
	skb->csum_start = skb_transport_header(skb) - skb->head;
	skb->csum_offset = offsetof(struct tcphdr, check);
}

 
void tcp_v4_send_check(struct sock *sk, struct sk_buff *skb)
{
	const struct inet_sock *inet = inet_sk(sk);

	__tcp_v4_send_check(skb, inet->inet_saddr, inet->inet_daddr);
}
EXPORT_SYMBOL(tcp_v4_send_check);

 

#ifdef CONFIG_TCP_MD5SIG
#define OPTION_BYTES TCPOLEN_MD5SIG_ALIGNED
#else
#define OPTION_BYTES sizeof(__be32)
#endif

static void tcp_v4_send_reset(const struct sock *sk, struct sk_buff *skb)
{
	const struct tcphdr *th = tcp_hdr(skb);
	struct {
		struct tcphdr th;
		__be32 opt[OPTION_BYTES / sizeof(__be32)];
	} rep;
	struct ip_reply_arg arg;
#ifdef CONFIG_TCP_MD5SIG
	struct tcp_md5sig_key *key = NULL;
	const __u8 *hash_location = NULL;
	unsigned char newhash[16];
	int genhash;
	struct sock *sk1 = NULL;
#endif
	u64 transmit_time = 0;
	struct sock *ctl_sk;
	struct net *net;
	u32 txhash = 0;

	 
	if (th->rst)
		return;

	 
	if (!sk && skb_rtable(skb)->rt_type != RTN_LOCAL)
		return;

	 
	memset(&rep, 0, sizeof(rep));
	rep.th.dest   = th->source;
	rep.th.source = th->dest;
	rep.th.doff   = sizeof(struct tcphdr) / 4;
	rep.th.rst    = 1;

	if (th->ack) {
		rep.th.seq = th->ack_seq;
	} else {
		rep.th.ack = 1;
		rep.th.ack_seq = htonl(ntohl(th->seq) + th->syn + th->fin +
				       skb->len - (th->doff << 2));
	}

	memset(&arg, 0, sizeof(arg));
	arg.iov[0].iov_base = (unsigned char *)&rep;
	arg.iov[0].iov_len  = sizeof(rep.th);

	net = sk ? sock_net(sk) : dev_net(skb_dst(skb)->dev);
#ifdef CONFIG_TCP_MD5SIG
	rcu_read_lock();
	hash_location = tcp_parse_md5sig_option(th);
	if (sk && sk_fullsock(sk)) {
		const union tcp_md5_addr *addr;
		int l3index;

		 
		l3index = tcp_v4_sdif(skb) ? inet_iif(skb) : 0;
		addr = (union tcp_md5_addr *)&ip_hdr(skb)->saddr;
		key = tcp_md5_do_lookup(sk, l3index, addr, AF_INET);
	} else if (hash_location) {
		const union tcp_md5_addr *addr;
		int sdif = tcp_v4_sdif(skb);
		int dif = inet_iif(skb);
		int l3index;

		 
		sk1 = __inet_lookup_listener(net, net->ipv4.tcp_death_row.hashinfo,
					     NULL, 0, ip_hdr(skb)->saddr,
					     th->source, ip_hdr(skb)->daddr,
					     ntohs(th->source), dif, sdif);
		 
		if (!sk1)
			goto out;

		 
		l3index = sdif ? dif : 0;
		addr = (union tcp_md5_addr *)&ip_hdr(skb)->saddr;
		key = tcp_md5_do_lookup(sk1, l3index, addr, AF_INET);
		if (!key)
			goto out;


		genhash = tcp_v4_md5_hash_skb(newhash, key, NULL, skb);
		if (genhash || memcmp(hash_location, newhash, 16) != 0)
			goto out;

	}

	if (key) {
		rep.opt[0] = htonl((TCPOPT_NOP << 24) |
				   (TCPOPT_NOP << 16) |
				   (TCPOPT_MD5SIG << 8) |
				   TCPOLEN_MD5SIG);
		 
		arg.iov[0].iov_len += TCPOLEN_MD5SIG_ALIGNED;
		rep.th.doff = arg.iov[0].iov_len / 4;

		tcp_v4_md5_hash_hdr((__u8 *) &rep.opt[1],
				     key, ip_hdr(skb)->saddr,
				     ip_hdr(skb)->daddr, &rep.th);
	}
#endif
	 
	if (rep.opt[0] == 0) {
		__be32 mrst = mptcp_reset_option(skb);

		if (mrst) {
			rep.opt[0] = mrst;
			arg.iov[0].iov_len += sizeof(mrst);
			rep.th.doff = arg.iov[0].iov_len / 4;
		}
	}

	arg.csum = csum_tcpudp_nofold(ip_hdr(skb)->daddr,
				      ip_hdr(skb)->saddr,  
				      arg.iov[0].iov_len, IPPROTO_TCP, 0);
	arg.csumoffset = offsetof(struct tcphdr, check) / 2;
	arg.flags = (sk && inet_sk_transparent(sk)) ? IP_REPLY_ARG_NOSRCCHECK : 0;

	 
	if (sk) {
		arg.bound_dev_if = sk->sk_bound_dev_if;
		if (sk_fullsock(sk))
			trace_tcp_send_reset(sk, skb);
	}

	BUILD_BUG_ON(offsetof(struct sock, sk_bound_dev_if) !=
		     offsetof(struct inet_timewait_sock, tw_bound_dev_if));

	arg.tos = ip_hdr(skb)->tos;
	arg.uid = sock_net_uid(net, sk && sk_fullsock(sk) ? sk : NULL);
	local_bh_disable();
	ctl_sk = this_cpu_read(ipv4_tcp_sk);
	sock_net_set(ctl_sk, net);
	if (sk) {
		ctl_sk->sk_mark = (sk->sk_state == TCP_TIME_WAIT) ?
				   inet_twsk(sk)->tw_mark : sk->sk_mark;
		ctl_sk->sk_priority = (sk->sk_state == TCP_TIME_WAIT) ?
				   inet_twsk(sk)->tw_priority : sk->sk_priority;
		transmit_time = tcp_transmit_time(sk);
		xfrm_sk_clone_policy(ctl_sk, sk);
		txhash = (sk->sk_state == TCP_TIME_WAIT) ?
			 inet_twsk(sk)->tw_txhash : sk->sk_txhash;
	} else {
		ctl_sk->sk_mark = 0;
		ctl_sk->sk_priority = 0;
	}
	ip_send_unicast_reply(ctl_sk,
			      skb, &TCP_SKB_CB(skb)->header.h4.opt,
			      ip_hdr(skb)->saddr, ip_hdr(skb)->daddr,
			      &arg, arg.iov[0].iov_len,
			      transmit_time, txhash);

	xfrm_sk_free_policy(ctl_sk);
	sock_net_set(ctl_sk, &init_net);
	__TCP_INC_STATS(net, TCP_MIB_OUTSEGS);
	__TCP_INC_STATS(net, TCP_MIB_OUTRSTS);
	local_bh_enable();

#ifdef CONFIG_TCP_MD5SIG
out:
	rcu_read_unlock();
#endif
}

 

static void tcp_v4_send_ack(const struct sock *sk,
			    struct sk_buff *skb, u32 seq, u32 ack,
			    u32 win, u32 tsval, u32 tsecr, int oif,
			    struct tcp_md5sig_key *key,
			    int reply_flags, u8 tos, u32 txhash)
{
	const struct tcphdr *th = tcp_hdr(skb);
	struct {
		struct tcphdr th;
		__be32 opt[(TCPOLEN_TSTAMP_ALIGNED >> 2)
#ifdef CONFIG_TCP_MD5SIG
			   + (TCPOLEN_MD5SIG_ALIGNED >> 2)
#endif
			];
	} rep;
	struct net *net = sock_net(sk);
	struct ip_reply_arg arg;
	struct sock *ctl_sk;
	u64 transmit_time;

	memset(&rep.th, 0, sizeof(struct tcphdr));
	memset(&arg, 0, sizeof(arg));

	arg.iov[0].iov_base = (unsigned char *)&rep;
	arg.iov[0].iov_len  = sizeof(rep.th);
	if (tsecr) {
		rep.opt[0] = htonl((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) |
				   (TCPOPT_TIMESTAMP << 8) |
				   TCPOLEN_TIMESTAMP);
		rep.opt[1] = htonl(tsval);
		rep.opt[2] = htonl(tsecr);
		arg.iov[0].iov_len += TCPOLEN_TSTAMP_ALIGNED;
	}

	 
	rep.th.dest    = th->source;
	rep.th.source  = th->dest;
	rep.th.doff    = arg.iov[0].iov_len / 4;
	rep.th.seq     = htonl(seq);
	rep.th.ack_seq = htonl(ack);
	rep.th.ack     = 1;
	rep.th.window  = htons(win);

#ifdef CONFIG_TCP_MD5SIG
	if (key) {
		int offset = (tsecr) ? 3 : 0;

		rep.opt[offset++] = htonl((TCPOPT_NOP << 24) |
					  (TCPOPT_NOP << 16) |
					  (TCPOPT_MD5SIG << 8) |
					  TCPOLEN_MD5SIG);
		arg.iov[0].iov_len += TCPOLEN_MD5SIG_ALIGNED;
		rep.th.doff = arg.iov[0].iov_len/4;

		tcp_v4_md5_hash_hdr((__u8 *) &rep.opt[offset],
				    key, ip_hdr(skb)->saddr,
				    ip_hdr(skb)->daddr, &rep.th);
	}
#endif
	arg.flags = reply_flags;
	arg.csum = csum_tcpudp_nofold(ip_hdr(skb)->daddr,
				      ip_hdr(skb)->saddr,  
				      arg.iov[0].iov_len, IPPROTO_TCP, 0);
	arg.csumoffset = offsetof(struct tcphdr, check) / 2;
	if (oif)
		arg.bound_dev_if = oif;
	arg.tos = tos;
	arg.uid = sock_net_uid(net, sk_fullsock(sk) ? sk : NULL);
	local_bh_disable();
	ctl_sk = this_cpu_read(ipv4_tcp_sk);
	sock_net_set(ctl_sk, net);
	ctl_sk->sk_mark = (sk->sk_state == TCP_TIME_WAIT) ?
			   inet_twsk(sk)->tw_mark : READ_ONCE(sk->sk_mark);
	ctl_sk->sk_priority = (sk->sk_state == TCP_TIME_WAIT) ?
			   inet_twsk(sk)->tw_priority : READ_ONCE(sk->sk_priority);
	transmit_time = tcp_transmit_time(sk);
	ip_send_unicast_reply(ctl_sk,
			      skb, &TCP_SKB_CB(skb)->header.h4.opt,
			      ip_hdr(skb)->saddr, ip_hdr(skb)->daddr,
			      &arg, arg.iov[0].iov_len,
			      transmit_time, txhash);

	sock_net_set(ctl_sk, &init_net);
	__TCP_INC_STATS(net, TCP_MIB_OUTSEGS);
	local_bh_enable();
}

static void tcp_v4_timewait_ack(struct sock *sk, struct sk_buff *skb)
{
	struct inet_timewait_sock *tw = inet_twsk(sk);
	struct tcp_timewait_sock *tcptw = tcp_twsk(sk);

	tcp_v4_send_ack(sk, skb,
			tcptw->tw_snd_nxt, tcptw->tw_rcv_nxt,
			tcptw->tw_rcv_wnd >> tw->tw_rcv_wscale,
			tcp_time_stamp_raw() + tcptw->tw_ts_offset,
			tcptw->tw_ts_recent,
			tw->tw_bound_dev_if,
			tcp_twsk_md5_key(tcptw),
			tw->tw_transparent ? IP_REPLY_ARG_NOSRCCHECK : 0,
			tw->tw_tos,
			tw->tw_txhash
			);

	inet_twsk_put(tw);
}

static void tcp_v4_reqsk_send_ack(const struct sock *sk, struct sk_buff *skb,
				  struct request_sock *req)
{
	const union tcp_md5_addr *addr;
	int l3index;

	 
	u32 seq = (sk->sk_state == TCP_LISTEN) ? tcp_rsk(req)->snt_isn + 1 :
					     tcp_sk(sk)->snd_nxt;

	 
	addr = (union tcp_md5_addr *)&ip_hdr(skb)->saddr;
	l3index = tcp_v4_sdif(skb) ? inet_iif(skb) : 0;
	tcp_v4_send_ack(sk, skb, seq,
			tcp_rsk(req)->rcv_nxt,
			req->rsk_rcv_wnd >> inet_rsk(req)->rcv_wscale,
			tcp_time_stamp_raw() + tcp_rsk(req)->ts_off,
			READ_ONCE(req->ts_recent),
			0,
			tcp_md5_do_lookup(sk, l3index, addr, AF_INET),
			inet_rsk(req)->no_srccheck ? IP_REPLY_ARG_NOSRCCHECK : 0,
			ip_hdr(skb)->tos,
			READ_ONCE(tcp_rsk(req)->txhash));
}

 
static int tcp_v4_send_synack(const struct sock *sk, struct dst_entry *dst,
			      struct flowi *fl,
			      struct request_sock *req,
			      struct tcp_fastopen_cookie *foc,
			      enum tcp_synack_type synack_type,
			      struct sk_buff *syn_skb)
{
	const struct inet_request_sock *ireq = inet_rsk(req);
	struct flowi4 fl4;
	int err = -1;
	struct sk_buff *skb;
	u8 tos;

	 
	if (!dst && (dst = inet_csk_route_req(sk, &fl4, req)) == NULL)
		return -1;

	skb = tcp_make_synack(sk, dst, req, foc, synack_type, syn_skb);

	if (skb) {
		__tcp_v4_send_check(skb, ireq->ir_loc_addr, ireq->ir_rmt_addr);

		tos = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_reflect_tos) ?
				(tcp_rsk(req)->syn_tos & ~INET_ECN_MASK) |
				(inet_sk(sk)->tos & INET_ECN_MASK) :
				inet_sk(sk)->tos;

		if (!INET_ECN_is_capable(tos) &&
		    tcp_bpf_ca_needs_ecn((struct sock *)req))
			tos |= INET_ECN_ECT_0;

		rcu_read_lock();
		err = ip_build_and_send_pkt(skb, sk, ireq->ir_loc_addr,
					    ireq->ir_rmt_addr,
					    rcu_dereference(ireq->ireq_opt),
					    tos);
		rcu_read_unlock();
		err = net_xmit_eval(err);
	}

	return err;
}

 
static void tcp_v4_reqsk_destructor(struct request_sock *req)
{
	kfree(rcu_dereference_protected(inet_rsk(req)->ireq_opt, 1));
}

#ifdef CONFIG_TCP_MD5SIG
 

DEFINE_STATIC_KEY_DEFERRED_FALSE(tcp_md5_needed, HZ);
EXPORT_SYMBOL(tcp_md5_needed);

static bool better_md5_match(struct tcp_md5sig_key *old, struct tcp_md5sig_key *new)
{
	if (!old)
		return true;

	 
	if (old->l3index && new->l3index == 0)
		return false;
	if (old->l3index == 0 && new->l3index)
		return true;

	return old->prefixlen < new->prefixlen;
}

 
struct tcp_md5sig_key *__tcp_md5_do_lookup(const struct sock *sk, int l3index,
					   const union tcp_md5_addr *addr,
					   int family)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_md5sig_key *key;
	const struct tcp_md5sig_info *md5sig;
	__be32 mask;
	struct tcp_md5sig_key *best_match = NULL;
	bool match;

	 
	md5sig = rcu_dereference_check(tp->md5sig_info,
				       lockdep_sock_is_held(sk));
	if (!md5sig)
		return NULL;

	hlist_for_each_entry_rcu(key, &md5sig->head, node,
				 lockdep_sock_is_held(sk)) {
		if (key->family != family)
			continue;
		if (key->flags & TCP_MD5SIG_FLAG_IFINDEX && key->l3index != l3index)
			continue;
		if (family == AF_INET) {
			mask = inet_make_mask(key->prefixlen);
			match = (key->addr.a4.s_addr & mask) ==
				(addr->a4.s_addr & mask);
#if IS_ENABLED(CONFIG_IPV6)
		} else if (family == AF_INET6) {
			match = ipv6_prefix_equal(&key->addr.a6, &addr->a6,
						  key->prefixlen);
#endif
		} else {
			match = false;
		}

		if (match && better_md5_match(best_match, key))
			best_match = key;
	}
	return best_match;
}
EXPORT_SYMBOL(__tcp_md5_do_lookup);

static struct tcp_md5sig_key *tcp_md5_do_lookup_exact(const struct sock *sk,
						      const union tcp_md5_addr *addr,
						      int family, u8 prefixlen,
						      int l3index, u8 flags)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_md5sig_key *key;
	unsigned int size = sizeof(struct in_addr);
	const struct tcp_md5sig_info *md5sig;

	 
	md5sig = rcu_dereference_check(tp->md5sig_info,
				       lockdep_sock_is_held(sk));
	if (!md5sig)
		return NULL;
#if IS_ENABLED(CONFIG_IPV6)
	if (family == AF_INET6)
		size = sizeof(struct in6_addr);
#endif
	hlist_for_each_entry_rcu(key, &md5sig->head, node,
				 lockdep_sock_is_held(sk)) {
		if (key->family != family)
			continue;
		if ((key->flags & TCP_MD5SIG_FLAG_IFINDEX) != (flags & TCP_MD5SIG_FLAG_IFINDEX))
			continue;
		if (key->l3index != l3index)
			continue;
		if (!memcmp(&key->addr, addr, size) &&
		    key->prefixlen == prefixlen)
			return key;
	}
	return NULL;
}

struct tcp_md5sig_key *tcp_v4_md5_lookup(const struct sock *sk,
					 const struct sock *addr_sk)
{
	const union tcp_md5_addr *addr;
	int l3index;

	l3index = l3mdev_master_ifindex_by_index(sock_net(sk),
						 addr_sk->sk_bound_dev_if);
	addr = (const union tcp_md5_addr *)&addr_sk->sk_daddr;
	return tcp_md5_do_lookup(sk, l3index, addr, AF_INET);
}
EXPORT_SYMBOL(tcp_v4_md5_lookup);

static int tcp_md5sig_info_add(struct sock *sk, gfp_t gfp)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_md5sig_info *md5sig;

	md5sig = kmalloc(sizeof(*md5sig), gfp);
	if (!md5sig)
		return -ENOMEM;

	sk_gso_disable(sk);
	INIT_HLIST_HEAD(&md5sig->head);
	rcu_assign_pointer(tp->md5sig_info, md5sig);
	return 0;
}

 
static int __tcp_md5_do_add(struct sock *sk, const union tcp_md5_addr *addr,
			    int family, u8 prefixlen, int l3index, u8 flags,
			    const u8 *newkey, u8 newkeylen, gfp_t gfp)
{
	 
	struct tcp_md5sig_key *key;
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_md5sig_info *md5sig;

	key = tcp_md5_do_lookup_exact(sk, addr, family, prefixlen, l3index, flags);
	if (key) {
		 
		data_race(memcpy(key->key, newkey, newkeylen));

		 
		WRITE_ONCE(key->keylen, newkeylen);

		return 0;
	}

	md5sig = rcu_dereference_protected(tp->md5sig_info,
					   lockdep_sock_is_held(sk));

	key = sock_kmalloc(sk, sizeof(*key), gfp | __GFP_ZERO);
	if (!key)
		return -ENOMEM;
	if (!tcp_alloc_md5sig_pool()) {
		sock_kfree_s(sk, key, sizeof(*key));
		return -ENOMEM;
	}

	memcpy(key->key, newkey, newkeylen);
	key->keylen = newkeylen;
	key->family = family;
	key->prefixlen = prefixlen;
	key->l3index = l3index;
	key->flags = flags;
	memcpy(&key->addr, addr,
	       (IS_ENABLED(CONFIG_IPV6) && family == AF_INET6) ? sizeof(struct in6_addr) :
								 sizeof(struct in_addr));
	hlist_add_head_rcu(&key->node, &md5sig->head);
	return 0;
}

int tcp_md5_do_add(struct sock *sk, const union tcp_md5_addr *addr,
		   int family, u8 prefixlen, int l3index, u8 flags,
		   const u8 *newkey, u8 newkeylen)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (!rcu_dereference_protected(tp->md5sig_info, lockdep_sock_is_held(sk))) {
		if (tcp_md5sig_info_add(sk, GFP_KERNEL))
			return -ENOMEM;

		if (!static_branch_inc(&tcp_md5_needed.key)) {
			struct tcp_md5sig_info *md5sig;

			md5sig = rcu_dereference_protected(tp->md5sig_info, lockdep_sock_is_held(sk));
			rcu_assign_pointer(tp->md5sig_info, NULL);
			kfree_rcu(md5sig, rcu);
			return -EUSERS;
		}
	}

	return __tcp_md5_do_add(sk, addr, family, prefixlen, l3index, flags,
				newkey, newkeylen, GFP_KERNEL);
}
EXPORT_SYMBOL(tcp_md5_do_add);

int tcp_md5_key_copy(struct sock *sk, const union tcp_md5_addr *addr,
		     int family, u8 prefixlen, int l3index,
		     struct tcp_md5sig_key *key)
{
	struct tcp_sock *tp = tcp_sk(sk);

	if (!rcu_dereference_protected(tp->md5sig_info, lockdep_sock_is_held(sk))) {
		if (tcp_md5sig_info_add(sk, sk_gfp_mask(sk, GFP_ATOMIC)))
			return -ENOMEM;

		if (!static_key_fast_inc_not_disabled(&tcp_md5_needed.key.key)) {
			struct tcp_md5sig_info *md5sig;

			md5sig = rcu_dereference_protected(tp->md5sig_info, lockdep_sock_is_held(sk));
			net_warn_ratelimited("Too many TCP-MD5 keys in the system\n");
			rcu_assign_pointer(tp->md5sig_info, NULL);
			kfree_rcu(md5sig, rcu);
			return -EUSERS;
		}
	}

	return __tcp_md5_do_add(sk, addr, family, prefixlen, l3index,
				key->flags, key->key, key->keylen,
				sk_gfp_mask(sk, GFP_ATOMIC));
}
EXPORT_SYMBOL(tcp_md5_key_copy);

int tcp_md5_do_del(struct sock *sk, const union tcp_md5_addr *addr, int family,
		   u8 prefixlen, int l3index, u8 flags)
{
	struct tcp_md5sig_key *key;

	key = tcp_md5_do_lookup_exact(sk, addr, family, prefixlen, l3index, flags);
	if (!key)
		return -ENOENT;
	hlist_del_rcu(&key->node);
	atomic_sub(sizeof(*key), &sk->sk_omem_alloc);
	kfree_rcu(key, rcu);
	return 0;
}
EXPORT_SYMBOL(tcp_md5_do_del);

static void tcp_clear_md5_list(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_md5sig_key *key;
	struct hlist_node *n;
	struct tcp_md5sig_info *md5sig;

	md5sig = rcu_dereference_protected(tp->md5sig_info, 1);

	hlist_for_each_entry_safe(key, n, &md5sig->head, node) {
		hlist_del_rcu(&key->node);
		atomic_sub(sizeof(*key), &sk->sk_omem_alloc);
		kfree_rcu(key, rcu);
	}
}

static int tcp_v4_parse_md5_keys(struct sock *sk, int optname,
				 sockptr_t optval, int optlen)
{
	struct tcp_md5sig cmd;
	struct sockaddr_in *sin = (struct sockaddr_in *)&cmd.tcpm_addr;
	const union tcp_md5_addr *addr;
	u8 prefixlen = 32;
	int l3index = 0;
	u8 flags;

	if (optlen < sizeof(cmd))
		return -EINVAL;

	if (copy_from_sockptr(&cmd, optval, sizeof(cmd)))
		return -EFAULT;

	if (sin->sin_family != AF_INET)
		return -EINVAL;

	flags = cmd.tcpm_flags & TCP_MD5SIG_FLAG_IFINDEX;

	if (optname == TCP_MD5SIG_EXT &&
	    cmd.tcpm_flags & TCP_MD5SIG_FLAG_PREFIX) {
		prefixlen = cmd.tcpm_prefixlen;
		if (prefixlen > 32)
			return -EINVAL;
	}

	if (optname == TCP_MD5SIG_EXT && cmd.tcpm_ifindex &&
	    cmd.tcpm_flags & TCP_MD5SIG_FLAG_IFINDEX) {
		struct net_device *dev;

		rcu_read_lock();
		dev = dev_get_by_index_rcu(sock_net(sk), cmd.tcpm_ifindex);
		if (dev && netif_is_l3_master(dev))
			l3index = dev->ifindex;

		rcu_read_unlock();

		 
		if (!dev || !l3index)
			return -EINVAL;
	}

	addr = (union tcp_md5_addr *)&sin->sin_addr.s_addr;

	if (!cmd.tcpm_keylen)
		return tcp_md5_do_del(sk, addr, AF_INET, prefixlen, l3index, flags);

	if (cmd.tcpm_keylen > TCP_MD5SIG_MAXKEYLEN)
		return -EINVAL;

	return tcp_md5_do_add(sk, addr, AF_INET, prefixlen, l3index, flags,
			      cmd.tcpm_key, cmd.tcpm_keylen);
}

static int tcp_v4_md5_hash_headers(struct tcp_md5sig_pool *hp,
				   __be32 daddr, __be32 saddr,
				   const struct tcphdr *th, int nbytes)
{
	struct tcp4_pseudohdr *bp;
	struct scatterlist sg;
	struct tcphdr *_th;

	bp = hp->scratch;
	bp->saddr = saddr;
	bp->daddr = daddr;
	bp->pad = 0;
	bp->protocol = IPPROTO_TCP;
	bp->len = cpu_to_be16(nbytes);

	_th = (struct tcphdr *)(bp + 1);
	memcpy(_th, th, sizeof(*th));
	_th->check = 0;

	sg_init_one(&sg, bp, sizeof(*bp) + sizeof(*th));
	ahash_request_set_crypt(hp->md5_req, &sg, NULL,
				sizeof(*bp) + sizeof(*th));
	return crypto_ahash_update(hp->md5_req);
}

static int tcp_v4_md5_hash_hdr(char *md5_hash, const struct tcp_md5sig_key *key,
			       __be32 daddr, __be32 saddr, const struct tcphdr *th)
{
	struct tcp_md5sig_pool *hp;
	struct ahash_request *req;

	hp = tcp_get_md5sig_pool();
	if (!hp)
		goto clear_hash_noput;
	req = hp->md5_req;

	if (crypto_ahash_init(req))
		goto clear_hash;
	if (tcp_v4_md5_hash_headers(hp, daddr, saddr, th, th->doff << 2))
		goto clear_hash;
	if (tcp_md5_hash_key(hp, key))
		goto clear_hash;
	ahash_request_set_crypt(req, NULL, md5_hash, 0);
	if (crypto_ahash_final(req))
		goto clear_hash;

	tcp_put_md5sig_pool();
	return 0;

clear_hash:
	tcp_put_md5sig_pool();
clear_hash_noput:
	memset(md5_hash, 0, 16);
	return 1;
}

int tcp_v4_md5_hash_skb(char *md5_hash, const struct tcp_md5sig_key *key,
			const struct sock *sk,
			const struct sk_buff *skb)
{
	struct tcp_md5sig_pool *hp;
	struct ahash_request *req;
	const struct tcphdr *th = tcp_hdr(skb);
	__be32 saddr, daddr;

	if (sk) {  
		saddr = sk->sk_rcv_saddr;
		daddr = sk->sk_daddr;
	} else {
		const struct iphdr *iph = ip_hdr(skb);
		saddr = iph->saddr;
		daddr = iph->daddr;
	}

	hp = tcp_get_md5sig_pool();
	if (!hp)
		goto clear_hash_noput;
	req = hp->md5_req;

	if (crypto_ahash_init(req))
		goto clear_hash;

	if (tcp_v4_md5_hash_headers(hp, daddr, saddr, th, skb->len))
		goto clear_hash;
	if (tcp_md5_hash_skb_data(hp, skb, th->doff << 2))
		goto clear_hash;
	if (tcp_md5_hash_key(hp, key))
		goto clear_hash;
	ahash_request_set_crypt(req, NULL, md5_hash, 0);
	if (crypto_ahash_final(req))
		goto clear_hash;

	tcp_put_md5sig_pool();
	return 0;

clear_hash:
	tcp_put_md5sig_pool();
clear_hash_noput:
	memset(md5_hash, 0, 16);
	return 1;
}
EXPORT_SYMBOL(tcp_v4_md5_hash_skb);

#endif

static void tcp_v4_init_req(struct request_sock *req,
			    const struct sock *sk_listener,
			    struct sk_buff *skb)
{
	struct inet_request_sock *ireq = inet_rsk(req);
	struct net *net = sock_net(sk_listener);

	sk_rcv_saddr_set(req_to_sk(req), ip_hdr(skb)->daddr);
	sk_daddr_set(req_to_sk(req), ip_hdr(skb)->saddr);
	RCU_INIT_POINTER(ireq->ireq_opt, tcp_v4_save_options(net, skb));
}

static struct dst_entry *tcp_v4_route_req(const struct sock *sk,
					  struct sk_buff *skb,
					  struct flowi *fl,
					  struct request_sock *req)
{
	tcp_v4_init_req(req, sk, skb);

	if (security_inet_conn_request(sk, skb, req))
		return NULL;

	return inet_csk_route_req(sk, &fl->u.ip4, req);
}

struct request_sock_ops tcp_request_sock_ops __read_mostly = {
	.family		=	PF_INET,
	.obj_size	=	sizeof(struct tcp_request_sock),
	.rtx_syn_ack	=	tcp_rtx_synack,
	.send_ack	=	tcp_v4_reqsk_send_ack,
	.destructor	=	tcp_v4_reqsk_destructor,
	.send_reset	=	tcp_v4_send_reset,
	.syn_ack_timeout =	tcp_syn_ack_timeout,
};

const struct tcp_request_sock_ops tcp_request_sock_ipv4_ops = {
	.mss_clamp	=	TCP_MSS_DEFAULT,
#ifdef CONFIG_TCP_MD5SIG
	.req_md5_lookup	=	tcp_v4_md5_lookup,
	.calc_md5_hash	=	tcp_v4_md5_hash_skb,
#endif
#ifdef CONFIG_SYN_COOKIES
	.cookie_init_seq =	cookie_v4_init_sequence,
#endif
	.route_req	=	tcp_v4_route_req,
	.init_seq	=	tcp_v4_init_seq,
	.init_ts_off	=	tcp_v4_init_ts_off,
	.send_synack	=	tcp_v4_send_synack,
};

int tcp_v4_conn_request(struct sock *sk, struct sk_buff *skb)
{
	 
	if (skb_rtable(skb)->rt_flags & (RTCF_BROADCAST | RTCF_MULTICAST))
		goto drop;

	return tcp_conn_request(&tcp_request_sock_ops,
				&tcp_request_sock_ipv4_ops, sk, skb);

drop:
	tcp_listendrop(sk);
	return 0;
}
EXPORT_SYMBOL(tcp_v4_conn_request);


 
struct sock *tcp_v4_syn_recv_sock(const struct sock *sk, struct sk_buff *skb,
				  struct request_sock *req,
				  struct dst_entry *dst,
				  struct request_sock *req_unhash,
				  bool *own_req)
{
	struct inet_request_sock *ireq;
	bool found_dup_sk = false;
	struct inet_sock *newinet;
	struct tcp_sock *newtp;
	struct sock *newsk;
#ifdef CONFIG_TCP_MD5SIG
	const union tcp_md5_addr *addr;
	struct tcp_md5sig_key *key;
	int l3index;
#endif
	struct ip_options_rcu *inet_opt;

	if (sk_acceptq_is_full(sk))
		goto exit_overflow;

	newsk = tcp_create_openreq_child(sk, req, skb);
	if (!newsk)
		goto exit_nonewsk;

	newsk->sk_gso_type = SKB_GSO_TCPV4;
	inet_sk_rx_dst_set(newsk, skb);

	newtp		      = tcp_sk(newsk);
	newinet		      = inet_sk(newsk);
	ireq		      = inet_rsk(req);
	sk_daddr_set(newsk, ireq->ir_rmt_addr);
	sk_rcv_saddr_set(newsk, ireq->ir_loc_addr);
	newsk->sk_bound_dev_if = ireq->ir_iif;
	newinet->inet_saddr   = ireq->ir_loc_addr;
	inet_opt	      = rcu_dereference(ireq->ireq_opt);
	RCU_INIT_POINTER(newinet->inet_opt, inet_opt);
	newinet->mc_index     = inet_iif(skb);
	newinet->mc_ttl	      = ip_hdr(skb)->ttl;
	newinet->rcv_tos      = ip_hdr(skb)->tos;
	inet_csk(newsk)->icsk_ext_hdr_len = 0;
	if (inet_opt)
		inet_csk(newsk)->icsk_ext_hdr_len = inet_opt->opt.optlen;
	atomic_set(&newinet->inet_id, get_random_u16());

	 
	if (READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_reflect_tos))
		newinet->tos = tcp_rsk(req)->syn_tos & ~INET_ECN_MASK;

	if (!dst) {
		dst = inet_csk_route_child_sock(sk, newsk, req);
		if (!dst)
			goto put_and_exit;
	} else {
		 
	}
	sk_setup_caps(newsk, dst);

	tcp_ca_openreq_child(newsk, dst);

	tcp_sync_mss(newsk, dst_mtu(dst));
	newtp->advmss = tcp_mss_clamp(tcp_sk(sk), dst_metric_advmss(dst));

	tcp_initialize_rcv_mss(newsk);

#ifdef CONFIG_TCP_MD5SIG
	l3index = l3mdev_master_ifindex_by_index(sock_net(sk), ireq->ir_iif);
	 
	addr = (union tcp_md5_addr *)&newinet->inet_daddr;
	key = tcp_md5_do_lookup(sk, l3index, addr, AF_INET);
	if (key) {
		if (tcp_md5_key_copy(newsk, addr, AF_INET, 32, l3index, key))
			goto put_and_exit;
		sk_gso_disable(newsk);
	}
#endif

	if (__inet_inherit_port(sk, newsk) < 0)
		goto put_and_exit;
	*own_req = inet_ehash_nolisten(newsk, req_to_sk(req_unhash),
				       &found_dup_sk);
	if (likely(*own_req)) {
		tcp_move_syn(newtp, req);
		ireq->ireq_opt = NULL;
	} else {
		newinet->inet_opt = NULL;

		if (!req_unhash && found_dup_sk) {
			 
			bh_unlock_sock(newsk);
			sock_put(newsk);
			newsk = NULL;
		}
	}
	return newsk;

exit_overflow:
	NET_INC_STATS(sock_net(sk), LINUX_MIB_LISTENOVERFLOWS);
exit_nonewsk:
	dst_release(dst);
exit:
	tcp_listendrop(sk);
	return NULL;
put_and_exit:
	newinet->inet_opt = NULL;
	inet_csk_prepare_forced_close(newsk);
	tcp_done(newsk);
	goto exit;
}
EXPORT_SYMBOL(tcp_v4_syn_recv_sock);

static struct sock *tcp_v4_cookie_check(struct sock *sk, struct sk_buff *skb)
{
#ifdef CONFIG_SYN_COOKIES
	const struct tcphdr *th = tcp_hdr(skb);

	if (!th->syn)
		sk = cookie_v4_check(sk, skb);
#endif
	return sk;
}

u16 tcp_v4_get_syncookie(struct sock *sk, struct iphdr *iph,
			 struct tcphdr *th, u32 *cookie)
{
	u16 mss = 0;
#ifdef CONFIG_SYN_COOKIES
	mss = tcp_get_syncookie_mss(&tcp_request_sock_ops,
				    &tcp_request_sock_ipv4_ops, sk, th);
	if (mss) {
		*cookie = __cookie_v4_init_sequence(iph, th, &mss);
		tcp_synq_overflow(sk);
	}
#endif
	return mss;
}

INDIRECT_CALLABLE_DECLARE(struct dst_entry *ipv4_dst_check(struct dst_entry *,
							   u32));
 
int tcp_v4_do_rcv(struct sock *sk, struct sk_buff *skb)
{
	enum skb_drop_reason reason;
	struct sock *rsk;

	if (sk->sk_state == TCP_ESTABLISHED) {  
		struct dst_entry *dst;

		dst = rcu_dereference_protected(sk->sk_rx_dst,
						lockdep_sock_is_held(sk));

		sock_rps_save_rxhash(sk, skb);
		sk_mark_napi_id(sk, skb);
		if (dst) {
			if (sk->sk_rx_dst_ifindex != skb->skb_iif ||
			    !INDIRECT_CALL_1(dst->ops->check, ipv4_dst_check,
					     dst, 0)) {
				RCU_INIT_POINTER(sk->sk_rx_dst, NULL);
				dst_release(dst);
			}
		}
		tcp_rcv_established(sk, skb);
		return 0;
	}

	reason = SKB_DROP_REASON_NOT_SPECIFIED;
	if (tcp_checksum_complete(skb))
		goto csum_err;

	if (sk->sk_state == TCP_LISTEN) {
		struct sock *nsk = tcp_v4_cookie_check(sk, skb);

		if (!nsk)
			goto discard;
		if (nsk != sk) {
			if (tcp_child_process(sk, nsk, skb)) {
				rsk = nsk;
				goto reset;
			}
			return 0;
		}
	} else
		sock_rps_save_rxhash(sk, skb);

	if (tcp_rcv_state_process(sk, skb)) {
		rsk = sk;
		goto reset;
	}
	return 0;

reset:
	tcp_v4_send_reset(rsk, skb);
discard:
	kfree_skb_reason(skb, reason);
	 
	return 0;

csum_err:
	reason = SKB_DROP_REASON_TCP_CSUM;
	trace_tcp_bad_csum(skb);
	TCP_INC_STATS(sock_net(sk), TCP_MIB_CSUMERRORS);
	TCP_INC_STATS(sock_net(sk), TCP_MIB_INERRS);
	goto discard;
}
EXPORT_SYMBOL(tcp_v4_do_rcv);

int tcp_v4_early_demux(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);
	const struct iphdr *iph;
	const struct tcphdr *th;
	struct sock *sk;

	if (skb->pkt_type != PACKET_HOST)
		return 0;

	if (!pskb_may_pull(skb, skb_transport_offset(skb) + sizeof(struct tcphdr)))
		return 0;

	iph = ip_hdr(skb);
	th = tcp_hdr(skb);

	if (th->doff < sizeof(struct tcphdr) / 4)
		return 0;

	sk = __inet_lookup_established(net, net->ipv4.tcp_death_row.hashinfo,
				       iph->saddr, th->source,
				       iph->daddr, ntohs(th->dest),
				       skb->skb_iif, inet_sdif(skb));
	if (sk) {
		skb->sk = sk;
		skb->destructor = sock_edemux;
		if (sk_fullsock(sk)) {
			struct dst_entry *dst = rcu_dereference(sk->sk_rx_dst);

			if (dst)
				dst = dst_check(dst, 0);
			if (dst &&
			    sk->sk_rx_dst_ifindex == skb->skb_iif)
				skb_dst_set_noref(skb, dst);
		}
	}
	return 0;
}

bool tcp_add_backlog(struct sock *sk, struct sk_buff *skb,
		     enum skb_drop_reason *reason)
{
	u32 limit, tail_gso_size, tail_gso_segs;
	struct skb_shared_info *shinfo;
	const struct tcphdr *th;
	struct tcphdr *thtail;
	struct sk_buff *tail;
	unsigned int hdrlen;
	bool fragstolen;
	u32 gso_segs;
	u32 gso_size;
	int delta;

	 
	skb_condense(skb);

	skb_dst_drop(skb);

	if (unlikely(tcp_checksum_complete(skb))) {
		bh_unlock_sock(sk);
		trace_tcp_bad_csum(skb);
		*reason = SKB_DROP_REASON_TCP_CSUM;
		__TCP_INC_STATS(sock_net(sk), TCP_MIB_CSUMERRORS);
		__TCP_INC_STATS(sock_net(sk), TCP_MIB_INERRS);
		return true;
	}

	 
	th = (const struct tcphdr *)skb->data;
	hdrlen = th->doff * 4;

	tail = sk->sk_backlog.tail;
	if (!tail)
		goto no_coalesce;
	thtail = (struct tcphdr *)tail->data;

	if (TCP_SKB_CB(tail)->end_seq != TCP_SKB_CB(skb)->seq ||
	    TCP_SKB_CB(tail)->ip_dsfield != TCP_SKB_CB(skb)->ip_dsfield ||
	    ((TCP_SKB_CB(tail)->tcp_flags |
	      TCP_SKB_CB(skb)->tcp_flags) & (TCPHDR_SYN | TCPHDR_RST | TCPHDR_URG)) ||
	    !((TCP_SKB_CB(tail)->tcp_flags &
	      TCP_SKB_CB(skb)->tcp_flags) & TCPHDR_ACK) ||
	    ((TCP_SKB_CB(tail)->tcp_flags ^
	      TCP_SKB_CB(skb)->tcp_flags) & (TCPHDR_ECE | TCPHDR_CWR)) ||
#ifdef CONFIG_TLS_DEVICE
	    tail->decrypted != skb->decrypted ||
#endif
	    !mptcp_skb_can_collapse(tail, skb) ||
	    thtail->doff != th->doff ||
	    memcmp(thtail + 1, th + 1, hdrlen - sizeof(*th)))
		goto no_coalesce;

	__skb_pull(skb, hdrlen);

	shinfo = skb_shinfo(skb);
	gso_size = shinfo->gso_size ?: skb->len;
	gso_segs = shinfo->gso_segs ?: 1;

	shinfo = skb_shinfo(tail);
	tail_gso_size = shinfo->gso_size ?: (tail->len - hdrlen);
	tail_gso_segs = shinfo->gso_segs ?: 1;

	if (skb_try_coalesce(tail, skb, &fragstolen, &delta)) {
		TCP_SKB_CB(tail)->end_seq = TCP_SKB_CB(skb)->end_seq;

		if (likely(!before(TCP_SKB_CB(skb)->ack_seq, TCP_SKB_CB(tail)->ack_seq))) {
			TCP_SKB_CB(tail)->ack_seq = TCP_SKB_CB(skb)->ack_seq;
			thtail->window = th->window;
		}

		 
		thtail->fin |= th->fin;
		TCP_SKB_CB(tail)->tcp_flags |= TCP_SKB_CB(skb)->tcp_flags;

		if (TCP_SKB_CB(skb)->has_rxtstamp) {
			TCP_SKB_CB(tail)->has_rxtstamp = true;
			tail->tstamp = skb->tstamp;
			skb_hwtstamps(tail)->hwtstamp = skb_hwtstamps(skb)->hwtstamp;
		}

		 
		shinfo->gso_size = max(gso_size, tail_gso_size);
		shinfo->gso_segs = min_t(u32, gso_segs + tail_gso_segs, 0xFFFF);

		sk->sk_backlog.len += delta;
		__NET_INC_STATS(sock_net(sk),
				LINUX_MIB_TCPBACKLOGCOALESCE);
		kfree_skb_partial(skb, fragstolen);
		return false;
	}
	__skb_push(skb, hdrlen);

no_coalesce:
	limit = (u32)READ_ONCE(sk->sk_rcvbuf) + (u32)(READ_ONCE(sk->sk_sndbuf) >> 1);

	 
	limit += 64 * 1024;

	if (unlikely(sk_add_backlog(sk, skb, limit))) {
		bh_unlock_sock(sk);
		*reason = SKB_DROP_REASON_SOCKET_BACKLOG;
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPBACKLOGDROP);
		return true;
	}
	return false;
}
EXPORT_SYMBOL(tcp_add_backlog);

int tcp_filter(struct sock *sk, struct sk_buff *skb)
{
	struct tcphdr *th = (struct tcphdr *)skb->data;

	return sk_filter_trim_cap(sk, skb, th->doff * 4);
}
EXPORT_SYMBOL(tcp_filter);

static void tcp_v4_restore_cb(struct sk_buff *skb)
{
	memmove(IPCB(skb), &TCP_SKB_CB(skb)->header.h4,
		sizeof(struct inet_skb_parm));
}

static void tcp_v4_fill_cb(struct sk_buff *skb, const struct iphdr *iph,
			   const struct tcphdr *th)
{
	 
	memmove(&TCP_SKB_CB(skb)->header.h4, IPCB(skb),
		sizeof(struct inet_skb_parm));
	barrier();

	TCP_SKB_CB(skb)->seq = ntohl(th->seq);
	TCP_SKB_CB(skb)->end_seq = (TCP_SKB_CB(skb)->seq + th->syn + th->fin +
				    skb->len - th->doff * 4);
	TCP_SKB_CB(skb)->ack_seq = ntohl(th->ack_seq);
	TCP_SKB_CB(skb)->tcp_flags = tcp_flag_byte(th);
	TCP_SKB_CB(skb)->tcp_tw_isn = 0;
	TCP_SKB_CB(skb)->ip_dsfield = ipv4_get_dsfield(iph);
	TCP_SKB_CB(skb)->sacked	 = 0;
	TCP_SKB_CB(skb)->has_rxtstamp =
			skb->tstamp || skb_hwtstamps(skb)->hwtstamp;
}

 

int tcp_v4_rcv(struct sk_buff *skb)
{
	struct net *net = dev_net(skb->dev);
	enum skb_drop_reason drop_reason;
	int sdif = inet_sdif(skb);
	int dif = inet_iif(skb);
	const struct iphdr *iph;
	const struct tcphdr *th;
	bool refcounted;
	struct sock *sk;
	int ret;

	drop_reason = SKB_DROP_REASON_NOT_SPECIFIED;
	if (skb->pkt_type != PACKET_HOST)
		goto discard_it;

	 
	__TCP_INC_STATS(net, TCP_MIB_INSEGS);

	if (!pskb_may_pull(skb, sizeof(struct tcphdr)))
		goto discard_it;

	th = (const struct tcphdr *)skb->data;

	if (unlikely(th->doff < sizeof(struct tcphdr) / 4)) {
		drop_reason = SKB_DROP_REASON_PKT_TOO_SMALL;
		goto bad_packet;
	}
	if (!pskb_may_pull(skb, th->doff * 4))
		goto discard_it;

	 

	if (skb_checksum_init(skb, IPPROTO_TCP, inet_compute_pseudo))
		goto csum_error;

	th = (const struct tcphdr *)skb->data;
	iph = ip_hdr(skb);
lookup:
	sk = __inet_lookup_skb(net->ipv4.tcp_death_row.hashinfo,
			       skb, __tcp_hdrlen(th), th->source,
			       th->dest, sdif, &refcounted);
	if (!sk)
		goto no_tcp_socket;

process:
	if (sk->sk_state == TCP_TIME_WAIT)
		goto do_time_wait;

	if (sk->sk_state == TCP_NEW_SYN_RECV) {
		struct request_sock *req = inet_reqsk(sk);
		bool req_stolen = false;
		struct sock *nsk;

		sk = req->rsk_listener;
		if (!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb))
			drop_reason = SKB_DROP_REASON_XFRM_POLICY;
		else
			drop_reason = tcp_inbound_md5_hash(sk, skb,
						   &iph->saddr, &iph->daddr,
						   AF_INET, dif, sdif);
		if (unlikely(drop_reason)) {
			sk_drops_add(sk, skb);
			reqsk_put(req);
			goto discard_it;
		}
		if (tcp_checksum_complete(skb)) {
			reqsk_put(req);
			goto csum_error;
		}
		if (unlikely(sk->sk_state != TCP_LISTEN)) {
			nsk = reuseport_migrate_sock(sk, req_to_sk(req), skb);
			if (!nsk) {
				inet_csk_reqsk_queue_drop_and_put(sk, req);
				goto lookup;
			}
			sk = nsk;
			 
		} else {
			 
			sock_hold(sk);
		}
		refcounted = true;
		nsk = NULL;
		if (!tcp_filter(sk, skb)) {
			th = (const struct tcphdr *)skb->data;
			iph = ip_hdr(skb);
			tcp_v4_fill_cb(skb, iph, th);
			nsk = tcp_check_req(sk, skb, req, false, &req_stolen);
		} else {
			drop_reason = SKB_DROP_REASON_SOCKET_FILTER;
		}
		if (!nsk) {
			reqsk_put(req);
			if (req_stolen) {
				 
				tcp_v4_restore_cb(skb);
				sock_put(sk);
				goto lookup;
			}
			goto discard_and_relse;
		}
		nf_reset_ct(skb);
		if (nsk == sk) {
			reqsk_put(req);
			tcp_v4_restore_cb(skb);
		} else if (tcp_child_process(sk, nsk, skb)) {
			tcp_v4_send_reset(nsk, skb);
			goto discard_and_relse;
		} else {
			sock_put(sk);
			return 0;
		}
	}

	if (static_branch_unlikely(&ip4_min_ttl)) {
		 
		if (unlikely(iph->ttl < READ_ONCE(inet_sk(sk)->min_ttl))) {
			__NET_INC_STATS(net, LINUX_MIB_TCPMINTTLDROP);
			drop_reason = SKB_DROP_REASON_TCP_MINTTL;
			goto discard_and_relse;
		}
	}

	if (!xfrm4_policy_check(sk, XFRM_POLICY_IN, skb)) {
		drop_reason = SKB_DROP_REASON_XFRM_POLICY;
		goto discard_and_relse;
	}

	drop_reason = tcp_inbound_md5_hash(sk, skb, &iph->saddr,
					   &iph->daddr, AF_INET, dif, sdif);
	if (drop_reason)
		goto discard_and_relse;

	nf_reset_ct(skb);

	if (tcp_filter(sk, skb)) {
		drop_reason = SKB_DROP_REASON_SOCKET_FILTER;
		goto discard_and_relse;
	}
	th = (const struct tcphdr *)skb->data;
	iph = ip_hdr(skb);
	tcp_v4_fill_cb(skb, iph, th);

	skb->dev = NULL;

	if (sk->sk_state == TCP_LISTEN) {
		ret = tcp_v4_do_rcv(sk, skb);
		goto put_and_return;
	}

	sk_incoming_cpu_update(sk);

	bh_lock_sock_nested(sk);
	tcp_segs_in(tcp_sk(sk), skb);
	ret = 0;
	if (!sock_owned_by_user(sk)) {
		ret = tcp_v4_do_rcv(sk, skb);
	} else {
		if (tcp_add_backlog(sk, skb, &drop_reason))
			goto discard_and_relse;
	}
	bh_unlock_sock(sk);

put_and_return:
	if (refcounted)
		sock_put(sk);

	return ret;

no_tcp_socket:
	drop_reason = SKB_DROP_REASON_NO_SOCKET;
	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb))
		goto discard_it;

	tcp_v4_fill_cb(skb, iph, th);

	if (tcp_checksum_complete(skb)) {
csum_error:
		drop_reason = SKB_DROP_REASON_TCP_CSUM;
		trace_tcp_bad_csum(skb);
		__TCP_INC_STATS(net, TCP_MIB_CSUMERRORS);
bad_packet:
		__TCP_INC_STATS(net, TCP_MIB_INERRS);
	} else {
		tcp_v4_send_reset(NULL, skb);
	}

discard_it:
	SKB_DR_OR(drop_reason, NOT_SPECIFIED);
	 
	kfree_skb_reason(skb, drop_reason);
	return 0;

discard_and_relse:
	sk_drops_add(sk, skb);
	if (refcounted)
		sock_put(sk);
	goto discard_it;

do_time_wait:
	if (!xfrm4_policy_check(NULL, XFRM_POLICY_IN, skb)) {
		drop_reason = SKB_DROP_REASON_XFRM_POLICY;
		inet_twsk_put(inet_twsk(sk));
		goto discard_it;
	}

	tcp_v4_fill_cb(skb, iph, th);

	if (tcp_checksum_complete(skb)) {
		inet_twsk_put(inet_twsk(sk));
		goto csum_error;
	}
	switch (tcp_timewait_state_process(inet_twsk(sk), skb, th)) {
	case TCP_TW_SYN: {
		struct sock *sk2 = inet_lookup_listener(net,
							net->ipv4.tcp_death_row.hashinfo,
							skb, __tcp_hdrlen(th),
							iph->saddr, th->source,
							iph->daddr, th->dest,
							inet_iif(skb),
							sdif);
		if (sk2) {
			inet_twsk_deschedule_put(inet_twsk(sk));
			sk = sk2;
			tcp_v4_restore_cb(skb);
			refcounted = false;
			goto process;
		}
	}
		 
		fallthrough;
	case TCP_TW_ACK:
		tcp_v4_timewait_ack(sk, skb);
		break;
	case TCP_TW_RST:
		tcp_v4_send_reset(sk, skb);
		inet_twsk_deschedule_put(inet_twsk(sk));
		goto discard_it;
	case TCP_TW_SUCCESS:;
	}
	goto discard_it;
}

static struct timewait_sock_ops tcp_timewait_sock_ops = {
	.twsk_obj_size	= sizeof(struct tcp_timewait_sock),
	.twsk_unique	= tcp_twsk_unique,
	.twsk_destructor= tcp_twsk_destructor,
};

void inet_sk_rx_dst_set(struct sock *sk, const struct sk_buff *skb)
{
	struct dst_entry *dst = skb_dst(skb);

	if (dst && dst_hold_safe(dst)) {
		rcu_assign_pointer(sk->sk_rx_dst, dst);
		sk->sk_rx_dst_ifindex = skb->skb_iif;
	}
}
EXPORT_SYMBOL(inet_sk_rx_dst_set);

const struct inet_connection_sock_af_ops ipv4_specific = {
	.queue_xmit	   = ip_queue_xmit,
	.send_check	   = tcp_v4_send_check,
	.rebuild_header	   = inet_sk_rebuild_header,
	.sk_rx_dst_set	   = inet_sk_rx_dst_set,
	.conn_request	   = tcp_v4_conn_request,
	.syn_recv_sock	   = tcp_v4_syn_recv_sock,
	.net_header_len	   = sizeof(struct iphdr),
	.setsockopt	   = ip_setsockopt,
	.getsockopt	   = ip_getsockopt,
	.addr2sockaddr	   = inet_csk_addr2sockaddr,
	.sockaddr_len	   = sizeof(struct sockaddr_in),
	.mtu_reduced	   = tcp_v4_mtu_reduced,
};
EXPORT_SYMBOL(ipv4_specific);

#ifdef CONFIG_TCP_MD5SIG
static const struct tcp_sock_af_ops tcp_sock_ipv4_specific = {
	.md5_lookup		= tcp_v4_md5_lookup,
	.calc_md5_hash		= tcp_v4_md5_hash_skb,
	.md5_parse		= tcp_v4_parse_md5_keys,
};
#endif

 
static int tcp_v4_init_sock(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	tcp_init_sock(sk);

	icsk->icsk_af_ops = &ipv4_specific;

#ifdef CONFIG_TCP_MD5SIG
	tcp_sk(sk)->af_specific = &tcp_sock_ipv4_specific;
#endif

	return 0;
}

void tcp_v4_destroy_sock(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	trace_tcp_destroy_sock(sk);

	tcp_clear_xmit_timers(sk);

	tcp_cleanup_congestion_control(sk);

	tcp_cleanup_ulp(sk);

	 
	tcp_write_queue_purge(sk);

	 
	tcp_fastopen_active_disable_ofo_check(sk);

	 
	skb_rbtree_purge(&tp->out_of_order_queue);

#ifdef CONFIG_TCP_MD5SIG
	 
	if (tp->md5sig_info) {
		tcp_clear_md5_list(sk);
		kfree_rcu(rcu_dereference_protected(tp->md5sig_info, 1), rcu);
		tp->md5sig_info = NULL;
		static_branch_slow_dec_deferred(&tcp_md5_needed);
	}
#endif

	 
	if (inet_csk(sk)->icsk_bind_hash)
		inet_put_port(sk);

	BUG_ON(rcu_access_pointer(tp->fastopen_rsk));

	 
	tcp_free_fastopen_req(tp);
	tcp_fastopen_destroy_cipher(sk);
	tcp_saved_syn_free(tp);

	sk_sockets_allocated_dec(sk);
}
EXPORT_SYMBOL(tcp_v4_destroy_sock);

#ifdef CONFIG_PROC_FS
 

static unsigned short seq_file_family(const struct seq_file *seq);

static bool seq_sk_match(struct seq_file *seq, const struct sock *sk)
{
	unsigned short family = seq_file_family(seq);

	 
	return ((family == AF_UNSPEC || family == sk->sk_family) &&
		net_eq(sock_net(sk), seq_file_net(seq)));
}

 
static void *listening_get_first(struct seq_file *seq)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct tcp_iter_state *st = seq->private;

	st->offset = 0;
	for (; st->bucket <= hinfo->lhash2_mask; st->bucket++) {
		struct inet_listen_hashbucket *ilb2;
		struct hlist_nulls_node *node;
		struct sock *sk;

		ilb2 = &hinfo->lhash2[st->bucket];
		if (hlist_nulls_empty(&ilb2->nulls_head))
			continue;

		spin_lock(&ilb2->lock);
		sk_nulls_for_each(sk, node, &ilb2->nulls_head) {
			if (seq_sk_match(seq, sk))
				return sk;
		}
		spin_unlock(&ilb2->lock);
	}

	return NULL;
}

 
static void *listening_get_next(struct seq_file *seq, void *cur)
{
	struct tcp_iter_state *st = seq->private;
	struct inet_listen_hashbucket *ilb2;
	struct hlist_nulls_node *node;
	struct inet_hashinfo *hinfo;
	struct sock *sk = cur;

	++st->num;
	++st->offset;

	sk = sk_nulls_next(sk);
	sk_nulls_for_each_from(sk, node) {
		if (seq_sk_match(seq, sk))
			return sk;
	}

	hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	ilb2 = &hinfo->lhash2[st->bucket];
	spin_unlock(&ilb2->lock);
	++st->bucket;
	return listening_get_first(seq);
}

static void *listening_get_idx(struct seq_file *seq, loff_t *pos)
{
	struct tcp_iter_state *st = seq->private;
	void *rc;

	st->bucket = 0;
	st->offset = 0;
	rc = listening_get_first(seq);

	while (rc && *pos) {
		rc = listening_get_next(seq, rc);
		--*pos;
	}
	return rc;
}

static inline bool empty_bucket(struct inet_hashinfo *hinfo,
				const struct tcp_iter_state *st)
{
	return hlist_nulls_empty(&hinfo->ehash[st->bucket].chain);
}

 
static void *established_get_first(struct seq_file *seq)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct tcp_iter_state *st = seq->private;

	st->offset = 0;
	for (; st->bucket <= hinfo->ehash_mask; ++st->bucket) {
		struct sock *sk;
		struct hlist_nulls_node *node;
		spinlock_t *lock = inet_ehash_lockp(hinfo, st->bucket);

		cond_resched();

		 
		if (empty_bucket(hinfo, st))
			continue;

		spin_lock_bh(lock);
		sk_nulls_for_each(sk, node, &hinfo->ehash[st->bucket].chain) {
			if (seq_sk_match(seq, sk))
				return sk;
		}
		spin_unlock_bh(lock);
	}

	return NULL;
}

static void *established_get_next(struct seq_file *seq, void *cur)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct tcp_iter_state *st = seq->private;
	struct hlist_nulls_node *node;
	struct sock *sk = cur;

	++st->num;
	++st->offset;

	sk = sk_nulls_next(sk);

	sk_nulls_for_each_from(sk, node) {
		if (seq_sk_match(seq, sk))
			return sk;
	}

	spin_unlock_bh(inet_ehash_lockp(hinfo, st->bucket));
	++st->bucket;
	return established_get_first(seq);
}

static void *established_get_idx(struct seq_file *seq, loff_t pos)
{
	struct tcp_iter_state *st = seq->private;
	void *rc;

	st->bucket = 0;
	rc = established_get_first(seq);

	while (rc && pos) {
		rc = established_get_next(seq, rc);
		--pos;
	}
	return rc;
}

static void *tcp_get_idx(struct seq_file *seq, loff_t pos)
{
	void *rc;
	struct tcp_iter_state *st = seq->private;

	st->state = TCP_SEQ_STATE_LISTENING;
	rc	  = listening_get_idx(seq, &pos);

	if (!rc) {
		st->state = TCP_SEQ_STATE_ESTABLISHED;
		rc	  = established_get_idx(seq, pos);
	}

	return rc;
}

static void *tcp_seek_last_pos(struct seq_file *seq)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct tcp_iter_state *st = seq->private;
	int bucket = st->bucket;
	int offset = st->offset;
	int orig_num = st->num;
	void *rc = NULL;

	switch (st->state) {
	case TCP_SEQ_STATE_LISTENING:
		if (st->bucket > hinfo->lhash2_mask)
			break;
		rc = listening_get_first(seq);
		while (offset-- && rc && bucket == st->bucket)
			rc = listening_get_next(seq, rc);
		if (rc)
			break;
		st->bucket = 0;
		st->state = TCP_SEQ_STATE_ESTABLISHED;
		fallthrough;
	case TCP_SEQ_STATE_ESTABLISHED:
		if (st->bucket > hinfo->ehash_mask)
			break;
		rc = established_get_first(seq);
		while (offset-- && rc && bucket == st->bucket)
			rc = established_get_next(seq, rc);
	}

	st->num = orig_num;

	return rc;
}

void *tcp_seq_start(struct seq_file *seq, loff_t *pos)
{
	struct tcp_iter_state *st = seq->private;
	void *rc;

	if (*pos && *pos == st->last_pos) {
		rc = tcp_seek_last_pos(seq);
		if (rc)
			goto out;
	}

	st->state = TCP_SEQ_STATE_LISTENING;
	st->num = 0;
	st->bucket = 0;
	st->offset = 0;
	rc = *pos ? tcp_get_idx(seq, *pos - 1) : SEQ_START_TOKEN;

out:
	st->last_pos = *pos;
	return rc;
}
EXPORT_SYMBOL(tcp_seq_start);

void *tcp_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct tcp_iter_state *st = seq->private;
	void *rc = NULL;

	if (v == SEQ_START_TOKEN) {
		rc = tcp_get_idx(seq, 0);
		goto out;
	}

	switch (st->state) {
	case TCP_SEQ_STATE_LISTENING:
		rc = listening_get_next(seq, v);
		if (!rc) {
			st->state = TCP_SEQ_STATE_ESTABLISHED;
			st->bucket = 0;
			st->offset = 0;
			rc	  = established_get_first(seq);
		}
		break;
	case TCP_SEQ_STATE_ESTABLISHED:
		rc = established_get_next(seq, v);
		break;
	}
out:
	++*pos;
	st->last_pos = *pos;
	return rc;
}
EXPORT_SYMBOL(tcp_seq_next);

void tcp_seq_stop(struct seq_file *seq, void *v)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct tcp_iter_state *st = seq->private;

	switch (st->state) {
	case TCP_SEQ_STATE_LISTENING:
		if (v != SEQ_START_TOKEN)
			spin_unlock(&hinfo->lhash2[st->bucket].lock);
		break;
	case TCP_SEQ_STATE_ESTABLISHED:
		if (v)
			spin_unlock_bh(inet_ehash_lockp(hinfo, st->bucket));
		break;
	}
}
EXPORT_SYMBOL(tcp_seq_stop);

static void get_openreq4(const struct request_sock *req,
			 struct seq_file *f, int i)
{
	const struct inet_request_sock *ireq = inet_rsk(req);
	long delta = req->rsk_timer.expires - jiffies;

	seq_printf(f, "%4d: %08X:%04X %08X:%04X"
		" %02X %08X:%08X %02X:%08lX %08X %5u %8d %u %d %pK",
		i,
		ireq->ir_loc_addr,
		ireq->ir_num,
		ireq->ir_rmt_addr,
		ntohs(ireq->ir_rmt_port),
		TCP_SYN_RECV,
		0, 0,  
		1,     
		jiffies_delta_to_clock_t(delta),
		req->num_timeout,
		from_kuid_munged(seq_user_ns(f),
				 sock_i_uid(req->rsk_listener)),
		0,   
		0,  
		0,
		req);
}

static void get_tcp4_sock(struct sock *sk, struct seq_file *f, int i)
{
	int timer_active;
	unsigned long timer_expires;
	const struct tcp_sock *tp = tcp_sk(sk);
	const struct inet_connection_sock *icsk = inet_csk(sk);
	const struct inet_sock *inet = inet_sk(sk);
	const struct fastopen_queue *fastopenq = &icsk->icsk_accept_queue.fastopenq;
	__be32 dest = inet->inet_daddr;
	__be32 src = inet->inet_rcv_saddr;
	__u16 destp = ntohs(inet->inet_dport);
	__u16 srcp = ntohs(inet->inet_sport);
	int rx_queue;
	int state;

	if (icsk->icsk_pending == ICSK_TIME_RETRANS ||
	    icsk->icsk_pending == ICSK_TIME_REO_TIMEOUT ||
	    icsk->icsk_pending == ICSK_TIME_LOSS_PROBE) {
		timer_active	= 1;
		timer_expires	= icsk->icsk_timeout;
	} else if (icsk->icsk_pending == ICSK_TIME_PROBE0) {
		timer_active	= 4;
		timer_expires	= icsk->icsk_timeout;
	} else if (timer_pending(&sk->sk_timer)) {
		timer_active	= 2;
		timer_expires	= sk->sk_timer.expires;
	} else {
		timer_active	= 0;
		timer_expires = jiffies;
	}

	state = inet_sk_state_load(sk);
	if (state == TCP_LISTEN)
		rx_queue = READ_ONCE(sk->sk_ack_backlog);
	else
		 
		rx_queue = max_t(int, READ_ONCE(tp->rcv_nxt) -
				      READ_ONCE(tp->copied_seq), 0);

	seq_printf(f, "%4d: %08X:%04X %08X:%04X %02X %08X:%08X %02X:%08lX "
			"%08X %5u %8d %lu %d %pK %lu %lu %u %u %d",
		i, src, srcp, dest, destp, state,
		READ_ONCE(tp->write_seq) - tp->snd_una,
		rx_queue,
		timer_active,
		jiffies_delta_to_clock_t(timer_expires - jiffies),
		icsk->icsk_retransmits,
		from_kuid_munged(seq_user_ns(f), sock_i_uid(sk)),
		icsk->icsk_probes_out,
		sock_i_ino(sk),
		refcount_read(&sk->sk_refcnt), sk,
		jiffies_to_clock_t(icsk->icsk_rto),
		jiffies_to_clock_t(icsk->icsk_ack.ato),
		(icsk->icsk_ack.quick << 1) | inet_csk_in_pingpong_mode(sk),
		tcp_snd_cwnd(tp),
		state == TCP_LISTEN ?
		    fastopenq->max_qlen :
		    (tcp_in_initial_slowstart(tp) ? -1 : tp->snd_ssthresh));
}

static void get_timewait4_sock(const struct inet_timewait_sock *tw,
			       struct seq_file *f, int i)
{
	long delta = tw->tw_timer.expires - jiffies;
	__be32 dest, src;
	__u16 destp, srcp;

	dest  = tw->tw_daddr;
	src   = tw->tw_rcv_saddr;
	destp = ntohs(tw->tw_dport);
	srcp  = ntohs(tw->tw_sport);

	seq_printf(f, "%4d: %08X:%04X %08X:%04X"
		" %02X %08X:%08X %02X:%08lX %08X %5d %8d %d %d %pK",
		i, src, srcp, dest, destp, tw->tw_substate, 0, 0,
		3, jiffies_delta_to_clock_t(delta), 0, 0, 0, 0,
		refcount_read(&tw->tw_refcnt), tw);
}

#define TMPSZ 150

static int tcp4_seq_show(struct seq_file *seq, void *v)
{
	struct tcp_iter_state *st;
	struct sock *sk = v;

	seq_setwidth(seq, TMPSZ - 1);
	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, "  sl  local_address rem_address   st tx_queue "
			   "rx_queue tr tm->when retrnsmt   uid  timeout "
			   "inode");
		goto out;
	}
	st = seq->private;

	if (sk->sk_state == TCP_TIME_WAIT)
		get_timewait4_sock(v, seq, st->num);
	else if (sk->sk_state == TCP_NEW_SYN_RECV)
		get_openreq4(v, seq, st->num);
	else
		get_tcp4_sock(v, seq, st->num);
out:
	seq_pad(seq, '\n');
	return 0;
}

#ifdef CONFIG_BPF_SYSCALL
struct bpf_tcp_iter_state {
	struct tcp_iter_state state;
	unsigned int cur_sk;
	unsigned int end_sk;
	unsigned int max_sk;
	struct sock **batch;
	bool st_bucket_done;
};

struct bpf_iter__tcp {
	__bpf_md_ptr(struct bpf_iter_meta *, meta);
	__bpf_md_ptr(struct sock_common *, sk_common);
	uid_t uid __aligned(8);
};

static int tcp_prog_seq_show(struct bpf_prog *prog, struct bpf_iter_meta *meta,
			     struct sock_common *sk_common, uid_t uid)
{
	struct bpf_iter__tcp ctx;

	meta->seq_num--;   
	ctx.meta = meta;
	ctx.sk_common = sk_common;
	ctx.uid = uid;
	return bpf_iter_run_prog(prog, &ctx);
}

static void bpf_iter_tcp_put_batch(struct bpf_tcp_iter_state *iter)
{
	while (iter->cur_sk < iter->end_sk)
		sock_gen_put(iter->batch[iter->cur_sk++]);
}

static int bpf_iter_tcp_realloc_batch(struct bpf_tcp_iter_state *iter,
				      unsigned int new_batch_sz)
{
	struct sock **new_batch;

	new_batch = kvmalloc(sizeof(*new_batch) * new_batch_sz,
			     GFP_USER | __GFP_NOWARN);
	if (!new_batch)
		return -ENOMEM;

	bpf_iter_tcp_put_batch(iter);
	kvfree(iter->batch);
	iter->batch = new_batch;
	iter->max_sk = new_batch_sz;

	return 0;
}

static unsigned int bpf_iter_tcp_listening_batch(struct seq_file *seq,
						 struct sock *start_sk)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct bpf_tcp_iter_state *iter = seq->private;
	struct tcp_iter_state *st = &iter->state;
	struct hlist_nulls_node *node;
	unsigned int expected = 1;
	struct sock *sk;

	sock_hold(start_sk);
	iter->batch[iter->end_sk++] = start_sk;

	sk = sk_nulls_next(start_sk);
	sk_nulls_for_each_from(sk, node) {
		if (seq_sk_match(seq, sk)) {
			if (iter->end_sk < iter->max_sk) {
				sock_hold(sk);
				iter->batch[iter->end_sk++] = sk;
			}
			expected++;
		}
	}
	spin_unlock(&hinfo->lhash2[st->bucket].lock);

	return expected;
}

static unsigned int bpf_iter_tcp_established_batch(struct seq_file *seq,
						   struct sock *start_sk)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct bpf_tcp_iter_state *iter = seq->private;
	struct tcp_iter_state *st = &iter->state;
	struct hlist_nulls_node *node;
	unsigned int expected = 1;
	struct sock *sk;

	sock_hold(start_sk);
	iter->batch[iter->end_sk++] = start_sk;

	sk = sk_nulls_next(start_sk);
	sk_nulls_for_each_from(sk, node) {
		if (seq_sk_match(seq, sk)) {
			if (iter->end_sk < iter->max_sk) {
				sock_hold(sk);
				iter->batch[iter->end_sk++] = sk;
			}
			expected++;
		}
	}
	spin_unlock_bh(inet_ehash_lockp(hinfo, st->bucket));

	return expected;
}

static struct sock *bpf_iter_tcp_batch(struct seq_file *seq)
{
	struct inet_hashinfo *hinfo = seq_file_net(seq)->ipv4.tcp_death_row.hashinfo;
	struct bpf_tcp_iter_state *iter = seq->private;
	struct tcp_iter_state *st = &iter->state;
	unsigned int expected;
	bool resized = false;
	struct sock *sk;

	 
	if (iter->st_bucket_done) {
		st->offset = 0;
		st->bucket++;
		if (st->state == TCP_SEQ_STATE_LISTENING &&
		    st->bucket > hinfo->lhash2_mask) {
			st->state = TCP_SEQ_STATE_ESTABLISHED;
			st->bucket = 0;
		}
	}

again:
	 
	iter->cur_sk = 0;
	iter->end_sk = 0;
	iter->st_bucket_done = false;

	sk = tcp_seek_last_pos(seq);
	if (!sk)
		return NULL;  

	if (st->state == TCP_SEQ_STATE_LISTENING)
		expected = bpf_iter_tcp_listening_batch(seq, sk);
	else
		expected = bpf_iter_tcp_established_batch(seq, sk);

	if (iter->end_sk == expected) {
		iter->st_bucket_done = true;
		return sk;
	}

	if (!resized && !bpf_iter_tcp_realloc_batch(iter, expected * 3 / 2)) {
		resized = true;
		goto again;
	}

	return sk;
}

static void *bpf_iter_tcp_seq_start(struct seq_file *seq, loff_t *pos)
{
	 
	if (*pos)
		return bpf_iter_tcp_batch(seq);

	return SEQ_START_TOKEN;
}

static void *bpf_iter_tcp_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct bpf_tcp_iter_state *iter = seq->private;
	struct tcp_iter_state *st = &iter->state;
	struct sock *sk;

	 
	if (iter->cur_sk < iter->end_sk) {
		 
		st->num++;
		 
		st->offset++;
		sock_gen_put(iter->batch[iter->cur_sk++]);
	}

	if (iter->cur_sk < iter->end_sk)
		sk = iter->batch[iter->cur_sk];
	else
		sk = bpf_iter_tcp_batch(seq);

	++*pos;
	 
	st->last_pos = *pos;
	return sk;
}

static int bpf_iter_tcp_seq_show(struct seq_file *seq, void *v)
{
	struct bpf_iter_meta meta;
	struct bpf_prog *prog;
	struct sock *sk = v;
	uid_t uid;
	int ret;

	if (v == SEQ_START_TOKEN)
		return 0;

	if (sk_fullsock(sk))
		lock_sock(sk);

	if (unlikely(sk_unhashed(sk))) {
		ret = SEQ_SKIP;
		goto unlock;
	}

	if (sk->sk_state == TCP_TIME_WAIT) {
		uid = 0;
	} else if (sk->sk_state == TCP_NEW_SYN_RECV) {
		const struct request_sock *req = v;

		uid = from_kuid_munged(seq_user_ns(seq),
				       sock_i_uid(req->rsk_listener));
	} else {
		uid = from_kuid_munged(seq_user_ns(seq), sock_i_uid(sk));
	}

	meta.seq = seq;
	prog = bpf_iter_get_info(&meta, false);
	ret = tcp_prog_seq_show(prog, &meta, v, uid);

unlock:
	if (sk_fullsock(sk))
		release_sock(sk);
	return ret;

}

static void bpf_iter_tcp_seq_stop(struct seq_file *seq, void *v)
{
	struct bpf_tcp_iter_state *iter = seq->private;
	struct bpf_iter_meta meta;
	struct bpf_prog *prog;

	if (!v) {
		meta.seq = seq;
		prog = bpf_iter_get_info(&meta, true);
		if (prog)
			(void)tcp_prog_seq_show(prog, &meta, v, 0);
	}

	if (iter->cur_sk < iter->end_sk) {
		bpf_iter_tcp_put_batch(iter);
		iter->st_bucket_done = false;
	}
}

static const struct seq_operations bpf_iter_tcp_seq_ops = {
	.show		= bpf_iter_tcp_seq_show,
	.start		= bpf_iter_tcp_seq_start,
	.next		= bpf_iter_tcp_seq_next,
	.stop		= bpf_iter_tcp_seq_stop,
};
#endif
static unsigned short seq_file_family(const struct seq_file *seq)
{
	const struct tcp_seq_afinfo *afinfo;

#ifdef CONFIG_BPF_SYSCALL
	 
	if (seq->op == &bpf_iter_tcp_seq_ops)
		return AF_UNSPEC;
#endif

	 
	afinfo = pde_data(file_inode(seq->file));
	return afinfo->family;
}

static const struct seq_operations tcp4_seq_ops = {
	.show		= tcp4_seq_show,
	.start		= tcp_seq_start,
	.next		= tcp_seq_next,
	.stop		= tcp_seq_stop,
};

static struct tcp_seq_afinfo tcp4_seq_afinfo = {
	.family		= AF_INET,
};

static int __net_init tcp4_proc_init_net(struct net *net)
{
	if (!proc_create_net_data("tcp", 0444, net->proc_net, &tcp4_seq_ops,
			sizeof(struct tcp_iter_state), &tcp4_seq_afinfo))
		return -ENOMEM;
	return 0;
}

static void __net_exit tcp4_proc_exit_net(struct net *net)
{
	remove_proc_entry("tcp", net->proc_net);
}

static struct pernet_operations tcp4_net_ops = {
	.init = tcp4_proc_init_net,
	.exit = tcp4_proc_exit_net,
};

int __init tcp4_proc_init(void)
{
	return register_pernet_subsys(&tcp4_net_ops);
}

void tcp4_proc_exit(void)
{
	unregister_pernet_subsys(&tcp4_net_ops);
}
#endif  

 
bool tcp_stream_memory_free(const struct sock *sk, int wake)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	u32 notsent_bytes = READ_ONCE(tp->write_seq) -
			    READ_ONCE(tp->snd_nxt);

	return (notsent_bytes << wake) < tcp_notsent_lowat(tp);
}
EXPORT_SYMBOL(tcp_stream_memory_free);

struct proto tcp_prot = {
	.name			= "TCP",
	.owner			= THIS_MODULE,
	.close			= tcp_close,
	.pre_connect		= tcp_v4_pre_connect,
	.connect		= tcp_v4_connect,
	.disconnect		= tcp_disconnect,
	.accept			= inet_csk_accept,
	.ioctl			= tcp_ioctl,
	.init			= tcp_v4_init_sock,
	.destroy		= tcp_v4_destroy_sock,
	.shutdown		= tcp_shutdown,
	.setsockopt		= tcp_setsockopt,
	.getsockopt		= tcp_getsockopt,
	.bpf_bypass_getsockopt	= tcp_bpf_bypass_getsockopt,
	.keepalive		= tcp_set_keepalive,
	.recvmsg		= tcp_recvmsg,
	.sendmsg		= tcp_sendmsg,
	.splice_eof		= tcp_splice_eof,
	.backlog_rcv		= tcp_v4_do_rcv,
	.release_cb		= tcp_release_cb,
	.hash			= inet_hash,
	.unhash			= inet_unhash,
	.get_port		= inet_csk_get_port,
	.put_port		= inet_put_port,
#ifdef CONFIG_BPF_SYSCALL
	.psock_update_sk_prot	= tcp_bpf_update_proto,
#endif
	.enter_memory_pressure	= tcp_enter_memory_pressure,
	.leave_memory_pressure	= tcp_leave_memory_pressure,
	.stream_memory_free	= tcp_stream_memory_free,
	.sockets_allocated	= &tcp_sockets_allocated,
	.orphan_count		= &tcp_orphan_count,

	.memory_allocated	= &tcp_memory_allocated,
	.per_cpu_fw_alloc	= &tcp_memory_per_cpu_fw_alloc,

	.memory_pressure	= &tcp_memory_pressure,
	.sysctl_mem		= sysctl_tcp_mem,
	.sysctl_wmem_offset	= offsetof(struct net, ipv4.sysctl_tcp_wmem),
	.sysctl_rmem_offset	= offsetof(struct net, ipv4.sysctl_tcp_rmem),
	.max_header		= MAX_TCP_HEADER,
	.obj_size		= sizeof(struct tcp_sock),
	.slab_flags		= SLAB_TYPESAFE_BY_RCU,
	.twsk_prot		= &tcp_timewait_sock_ops,
	.rsk_prot		= &tcp_request_sock_ops,
	.h.hashinfo		= NULL,
	.no_autobind		= true,
	.diag_destroy		= tcp_abort,
};
EXPORT_SYMBOL(tcp_prot);

static void __net_exit tcp_sk_exit(struct net *net)
{
	if (net->ipv4.tcp_congestion_control)
		bpf_module_put(net->ipv4.tcp_congestion_control,
			       net->ipv4.tcp_congestion_control->owner);
}

static void __net_init tcp_set_hashinfo(struct net *net)
{
	struct inet_hashinfo *hinfo;
	unsigned int ehash_entries;
	struct net *old_net;

	if (net_eq(net, &init_net))
		goto fallback;

	old_net = current->nsproxy->net_ns;
	ehash_entries = READ_ONCE(old_net->ipv4.sysctl_tcp_child_ehash_entries);
	if (!ehash_entries)
		goto fallback;

	ehash_entries = roundup_pow_of_two(ehash_entries);
	hinfo = inet_pernet_hashinfo_alloc(&tcp_hashinfo, ehash_entries);
	if (!hinfo) {
		pr_warn("Failed to allocate TCP ehash (entries: %u) "
			"for a netns, fallback to the global one\n",
			ehash_entries);
fallback:
		hinfo = &tcp_hashinfo;
		ehash_entries = tcp_hashinfo.ehash_mask + 1;
	}

	net->ipv4.tcp_death_row.hashinfo = hinfo;
	net->ipv4.tcp_death_row.sysctl_max_tw_buckets = ehash_entries / 2;
	net->ipv4.sysctl_max_syn_backlog = max(128U, ehash_entries / 128);
}

static int __net_init tcp_sk_init(struct net *net)
{
	net->ipv4.sysctl_tcp_ecn = 2;
	net->ipv4.sysctl_tcp_ecn_fallback = 1;

	net->ipv4.sysctl_tcp_base_mss = TCP_BASE_MSS;
	net->ipv4.sysctl_tcp_min_snd_mss = TCP_MIN_SND_MSS;
	net->ipv4.sysctl_tcp_probe_threshold = TCP_PROBE_THRESHOLD;
	net->ipv4.sysctl_tcp_probe_interval = TCP_PROBE_INTERVAL;
	net->ipv4.sysctl_tcp_mtu_probe_floor = TCP_MIN_SND_MSS;

	net->ipv4.sysctl_tcp_keepalive_time = TCP_KEEPALIVE_TIME;
	net->ipv4.sysctl_tcp_keepalive_probes = TCP_KEEPALIVE_PROBES;
	net->ipv4.sysctl_tcp_keepalive_intvl = TCP_KEEPALIVE_INTVL;

	net->ipv4.sysctl_tcp_syn_retries = TCP_SYN_RETRIES;
	net->ipv4.sysctl_tcp_synack_retries = TCP_SYNACK_RETRIES;
	net->ipv4.sysctl_tcp_syncookies = 1;
	net->ipv4.sysctl_tcp_reordering = TCP_FASTRETRANS_THRESH;
	net->ipv4.sysctl_tcp_retries1 = TCP_RETR1;
	net->ipv4.sysctl_tcp_retries2 = TCP_RETR2;
	net->ipv4.sysctl_tcp_orphan_retries = 0;
	net->ipv4.sysctl_tcp_fin_timeout = TCP_FIN_TIMEOUT;
	net->ipv4.sysctl_tcp_notsent_lowat = UINT_MAX;
	net->ipv4.sysctl_tcp_tw_reuse = 2;
	net->ipv4.sysctl_tcp_no_ssthresh_metrics_save = 1;

	refcount_set(&net->ipv4.tcp_death_row.tw_refcount, 1);
	tcp_set_hashinfo(net);

	net->ipv4.sysctl_tcp_sack = 1;
	net->ipv4.sysctl_tcp_window_scaling = 1;
	net->ipv4.sysctl_tcp_timestamps = 1;
	net->ipv4.sysctl_tcp_early_retrans = 3;
	net->ipv4.sysctl_tcp_recovery = TCP_RACK_LOSS_DETECTION;
	net->ipv4.sysctl_tcp_slow_start_after_idle = 1;  
	net->ipv4.sysctl_tcp_retrans_collapse = 1;
	net->ipv4.sysctl_tcp_max_reordering = 300;
	net->ipv4.sysctl_tcp_dsack = 1;
	net->ipv4.sysctl_tcp_app_win = 31;
	net->ipv4.sysctl_tcp_adv_win_scale = 1;
	net->ipv4.sysctl_tcp_frto = 2;
	net->ipv4.sysctl_tcp_moderate_rcvbuf = 1;
	 
	net->ipv4.sysctl_tcp_tso_win_divisor = 3;
	 
	net->ipv4.sysctl_tcp_limit_output_bytes = 16 * 65536;

	 
	net->ipv4.sysctl_tcp_challenge_ack_limit = INT_MAX;

	net->ipv4.sysctl_tcp_min_tso_segs = 2;
	net->ipv4.sysctl_tcp_tso_rtt_log = 9;   
	net->ipv4.sysctl_tcp_min_rtt_wlen = 300;
	net->ipv4.sysctl_tcp_autocorking = 1;
	net->ipv4.sysctl_tcp_invalid_ratelimit = HZ/2;
	net->ipv4.sysctl_tcp_pacing_ss_ratio = 200;
	net->ipv4.sysctl_tcp_pacing_ca_ratio = 120;
	if (net != &init_net) {
		memcpy(net->ipv4.sysctl_tcp_rmem,
		       init_net.ipv4.sysctl_tcp_rmem,
		       sizeof(init_net.ipv4.sysctl_tcp_rmem));
		memcpy(net->ipv4.sysctl_tcp_wmem,
		       init_net.ipv4.sysctl_tcp_wmem,
		       sizeof(init_net.ipv4.sysctl_tcp_wmem));
	}
	net->ipv4.sysctl_tcp_comp_sack_delay_ns = NSEC_PER_MSEC;
	net->ipv4.sysctl_tcp_comp_sack_slack_ns = 100 * NSEC_PER_USEC;
	net->ipv4.sysctl_tcp_comp_sack_nr = 44;
	net->ipv4.sysctl_tcp_fastopen = TFO_CLIENT_ENABLE;
	net->ipv4.sysctl_tcp_fastopen_blackhole_timeout = 0;
	atomic_set(&net->ipv4.tfo_active_disable_times, 0);

	 
	net->ipv4.sysctl_tcp_plb_enabled = 0;  
	net->ipv4.sysctl_tcp_plb_idle_rehash_rounds = 3;
	net->ipv4.sysctl_tcp_plb_rehash_rounds = 12;
	net->ipv4.sysctl_tcp_plb_suspend_rto_sec = 60;
	 
	net->ipv4.sysctl_tcp_plb_cong_thresh = (1 << TCP_PLB_SCALE) / 2;

	 
	if (!net_eq(net, &init_net) &&
	    bpf_try_module_get(init_net.ipv4.tcp_congestion_control,
			       init_net.ipv4.tcp_congestion_control->owner))
		net->ipv4.tcp_congestion_control = init_net.ipv4.tcp_congestion_control;
	else
		net->ipv4.tcp_congestion_control = &tcp_reno;

	net->ipv4.sysctl_tcp_syn_linear_timeouts = 4;
	net->ipv4.sysctl_tcp_shrink_window = 0;

	return 0;
}

static void __net_exit tcp_sk_exit_batch(struct list_head *net_exit_list)
{
	struct net *net;

	tcp_twsk_purge(net_exit_list, AF_INET);

	list_for_each_entry(net, net_exit_list, exit_list) {
		inet_pernet_hashinfo_free(net->ipv4.tcp_death_row.hashinfo);
		WARN_ON_ONCE(!refcount_dec_and_test(&net->ipv4.tcp_death_row.tw_refcount));
		tcp_fastopen_ctx_destroy(net);
	}
}

static struct pernet_operations __net_initdata tcp_sk_ops = {
       .init	   = tcp_sk_init,
       .exit	   = tcp_sk_exit,
       .exit_batch = tcp_sk_exit_batch,
};

#if defined(CONFIG_BPF_SYSCALL) && defined(CONFIG_PROC_FS)
DEFINE_BPF_ITER_FUNC(tcp, struct bpf_iter_meta *meta,
		     struct sock_common *sk_common, uid_t uid)

#define INIT_BATCH_SZ 16

static int bpf_iter_init_tcp(void *priv_data, struct bpf_iter_aux_info *aux)
{
	struct bpf_tcp_iter_state *iter = priv_data;
	int err;

	err = bpf_iter_init_seq_net(priv_data, aux);
	if (err)
		return err;

	err = bpf_iter_tcp_realloc_batch(iter, INIT_BATCH_SZ);
	if (err) {
		bpf_iter_fini_seq_net(priv_data);
		return err;
	}

	return 0;
}

static void bpf_iter_fini_tcp(void *priv_data)
{
	struct bpf_tcp_iter_state *iter = priv_data;

	bpf_iter_fini_seq_net(priv_data);
	kvfree(iter->batch);
}

static const struct bpf_iter_seq_info tcp_seq_info = {
	.seq_ops		= &bpf_iter_tcp_seq_ops,
	.init_seq_private	= bpf_iter_init_tcp,
	.fini_seq_private	= bpf_iter_fini_tcp,
	.seq_priv_size		= sizeof(struct bpf_tcp_iter_state),
};

static const struct bpf_func_proto *
bpf_iter_tcp_get_func_proto(enum bpf_func_id func_id,
			    const struct bpf_prog *prog)
{
	switch (func_id) {
	case BPF_FUNC_setsockopt:
		return &bpf_sk_setsockopt_proto;
	case BPF_FUNC_getsockopt:
		return &bpf_sk_getsockopt_proto;
	default:
		return NULL;
	}
}

static struct bpf_iter_reg tcp_reg_info = {
	.target			= "tcp",
	.ctx_arg_info_size	= 1,
	.ctx_arg_info		= {
		{ offsetof(struct bpf_iter__tcp, sk_common),
		  PTR_TO_BTF_ID_OR_NULL | PTR_TRUSTED },
	},
	.get_func_proto		= bpf_iter_tcp_get_func_proto,
	.seq_info		= &tcp_seq_info,
};

static void __init bpf_iter_register(void)
{
	tcp_reg_info.ctx_arg_info[0].btf_id = btf_sock_ids[BTF_SOCK_TYPE_SOCK_COMMON];
	if (bpf_iter_reg_target(&tcp_reg_info))
		pr_warn("Warning: could not register bpf iterator tcp\n");
}

#endif

void __init tcp_v4_init(void)
{
	int cpu, res;

	for_each_possible_cpu(cpu) {
		struct sock *sk;

		res = inet_ctl_sock_create(&sk, PF_INET, SOCK_RAW,
					   IPPROTO_TCP, &init_net);
		if (res)
			panic("Failed to create the TCP control socket.\n");
		sock_set_flag(sk, SOCK_USE_WRITE_QUEUE);

		 
		inet_sk(sk)->pmtudisc = IP_PMTUDISC_DO;

		per_cpu(ipv4_tcp_sk, cpu) = sk;
	}
	if (register_pernet_subsys(&tcp_sk_ops))
		panic("Failed to create the TCP control socket.\n");

#if defined(CONFIG_BPF_SYSCALL) && defined(CONFIG_PROC_FS)
	bpf_iter_register();
#endif
}

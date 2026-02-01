
 

#include <linux/module.h>
#include <linux/gfp.h>
#include <net/tcp.h>

static u32 tcp_clamp_rto_to_user_timeout(const struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	u32 elapsed, start_ts, user_timeout;
	s32 remaining;

	start_ts = tcp_sk(sk)->retrans_stamp;
	user_timeout = READ_ONCE(icsk->icsk_user_timeout);
	if (!user_timeout)
		return icsk->icsk_rto;
	elapsed = tcp_time_stamp(tcp_sk(sk)) - start_ts;
	remaining = user_timeout - elapsed;
	if (remaining <= 0)
		return 1;  

	return min_t(u32, icsk->icsk_rto, msecs_to_jiffies(remaining));
}

u32 tcp_clamp_probe0_to_user_timeout(const struct sock *sk, u32 when)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	u32 remaining, user_timeout;
	s32 elapsed;

	user_timeout = READ_ONCE(icsk->icsk_user_timeout);
	if (!user_timeout || !icsk->icsk_probes_tstamp)
		return when;

	elapsed = tcp_jiffies32 - icsk->icsk_probes_tstamp;
	if (unlikely(elapsed < 0))
		elapsed = 0;
	remaining = msecs_to_jiffies(user_timeout) - elapsed;
	remaining = max_t(u32, remaining, TCP_TIMEOUT_MIN);

	return min_t(u32, remaining, when);
}

 

static void tcp_write_err(struct sock *sk)
{
	WRITE_ONCE(sk->sk_err, READ_ONCE(sk->sk_err_soft) ? : ETIMEDOUT);
	sk_error_report(sk);

	tcp_write_queue_purge(sk);
	tcp_done(sk);
	__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPABORTONTIMEOUT);
}

 
static int tcp_out_of_resources(struct sock *sk, bool do_reset)
{
	struct tcp_sock *tp = tcp_sk(sk);
	int shift = 0;

	 
	if ((s32)(tcp_jiffies32 - tp->lsndtime) > 2*TCP_RTO_MAX || !do_reset)
		shift++;

	 
	if (READ_ONCE(sk->sk_err_soft))
		shift++;

	if (tcp_check_oom(sk, shift)) {
		 
		if ((s32)(tcp_jiffies32 - tp->lsndtime) <= TCP_TIMEWAIT_LEN ||
		     
		    (!tp->snd_wnd && !tp->packets_out))
			do_reset = true;
		if (do_reset)
			tcp_send_active_reset(sk, GFP_ATOMIC);
		tcp_done(sk);
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPABORTONMEMORY);
		return 1;
	}

	if (!check_net(sock_net(sk))) {
		 
		tcp_done(sk);
		return 1;
	}

	return 0;
}

 
static int tcp_orphan_retries(struct sock *sk, bool alive)
{
	int retries = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_orphan_retries);  

	 
	if (READ_ONCE(sk->sk_err_soft) && !alive)
		retries = 0;

	 
	if (retries == 0 && alive)
		retries = 8;
	return retries;
}

static void tcp_mtu_probing(struct inet_connection_sock *icsk, struct sock *sk)
{
	const struct net *net = sock_net(sk);
	int mss;

	 
	if (!READ_ONCE(net->ipv4.sysctl_tcp_mtu_probing))
		return;

	if (!icsk->icsk_mtup.enabled) {
		icsk->icsk_mtup.enabled = 1;
		icsk->icsk_mtup.probe_timestamp = tcp_jiffies32;
	} else {
		mss = tcp_mtu_to_mss(sk, icsk->icsk_mtup.search_low) >> 1;
		mss = min(READ_ONCE(net->ipv4.sysctl_tcp_base_mss), mss);
		mss = max(mss, READ_ONCE(net->ipv4.sysctl_tcp_mtu_probe_floor));
		mss = max(mss, READ_ONCE(net->ipv4.sysctl_tcp_min_snd_mss));
		icsk->icsk_mtup.search_low = tcp_mss_to_mtu(sk, mss);
	}
	tcp_sync_mss(sk, icsk->icsk_pmtu_cookie);
}

static unsigned int tcp_model_timeout(struct sock *sk,
				      unsigned int boundary,
				      unsigned int rto_base)
{
	unsigned int linear_backoff_thresh, timeout;

	linear_backoff_thresh = ilog2(TCP_RTO_MAX / rto_base);
	if (boundary <= linear_backoff_thresh)
		timeout = ((2 << boundary) - 1) * rto_base;
	else
		timeout = ((2 << linear_backoff_thresh) - 1) * rto_base +
			(boundary - linear_backoff_thresh) * TCP_RTO_MAX;
	return jiffies_to_msecs(timeout);
}
 
static bool retransmits_timed_out(struct sock *sk,
				  unsigned int boundary,
				  unsigned int timeout)
{
	unsigned int start_ts;

	if (!inet_csk(sk)->icsk_retransmits)
		return false;

	start_ts = tcp_sk(sk)->retrans_stamp;
	if (likely(timeout == 0)) {
		unsigned int rto_base = TCP_RTO_MIN;

		if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))
			rto_base = tcp_timeout_init(sk);
		timeout = tcp_model_timeout(sk, boundary, rto_base);
	}

	return (s32)(tcp_time_stamp(tcp_sk(sk)) - start_ts - timeout) >= 0;
}

 
static int tcp_write_timeout(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct net *net = sock_net(sk);
	bool expired = false, do_reset;
	int retry_until, max_retransmits;

	if ((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV)) {
		if (icsk->icsk_retransmits)
			__dst_negative_advice(sk);
		 
		retry_until = READ_ONCE(icsk->icsk_syn_retries) ? :
			READ_ONCE(net->ipv4.sysctl_tcp_syn_retries);

		max_retransmits = retry_until;
		if (sk->sk_state == TCP_SYN_SENT)
			max_retransmits += READ_ONCE(net->ipv4.sysctl_tcp_syn_linear_timeouts);

		expired = icsk->icsk_retransmits >= max_retransmits;
	} else {
		if (retransmits_timed_out(sk, READ_ONCE(net->ipv4.sysctl_tcp_retries1), 0)) {
			 
			tcp_mtu_probing(icsk, sk);

			__dst_negative_advice(sk);
		}

		retry_until = READ_ONCE(net->ipv4.sysctl_tcp_retries2);
		if (sock_flag(sk, SOCK_DEAD)) {
			const bool alive = icsk->icsk_rto < TCP_RTO_MAX;

			retry_until = tcp_orphan_retries(sk, alive);
			do_reset = alive ||
				!retransmits_timed_out(sk, retry_until, 0);

			if (tcp_out_of_resources(sk, do_reset))
				return 1;
		}
	}
	if (!expired)
		expired = retransmits_timed_out(sk, retry_until,
						READ_ONCE(icsk->icsk_user_timeout));
	tcp_fastopen_active_detect_blackhole(sk, expired);

	if (BPF_SOCK_OPS_TEST_FLAG(tp, BPF_SOCK_OPS_RTO_CB_FLAG))
		tcp_call_bpf_3arg(sk, BPF_SOCK_OPS_RTO_CB,
				  icsk->icsk_retransmits,
				  icsk->icsk_rto, (int)expired);

	if (expired) {
		 
		tcp_write_err(sk);
		return 1;
	}

	if (sk_rethink_txhash(sk)) {
		tp->timeout_rehash++;
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPTIMEOUTREHASH);
	}

	return 0;
}

 
void tcp_delack_timer_handler(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	if ((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_LISTEN))
		return;

	 
	if (tp->compressed_ack) {
		tcp_mstamp_refresh(tp);
		tcp_sack_compress_send_ack(sk);
		return;
	}

	if (!(icsk->icsk_ack.pending & ICSK_ACK_TIMER))
		return;

	if (time_after(icsk->icsk_ack.timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_delack_timer, icsk->icsk_ack.timeout);
		return;
	}
	icsk->icsk_ack.pending &= ~ICSK_ACK_TIMER;

	if (inet_csk_ack_scheduled(sk)) {
		if (!inet_csk_in_pingpong_mode(sk)) {
			 
			icsk->icsk_ack.ato = min(icsk->icsk_ack.ato << 1, icsk->icsk_rto);
		} else {
			 
			inet_csk_exit_pingpong_mode(sk);
			icsk->icsk_ack.ato      = TCP_ATO_MIN;
		}
		tcp_mstamp_refresh(tp);
		tcp_send_ack(sk);
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_DELAYEDACKS);
	}
}


 
static void tcp_delack_timer(struct timer_list *t)
{
	struct inet_connection_sock *icsk =
			from_timer(icsk, t, icsk_delack_timer);
	struct sock *sk = &icsk->icsk_inet.sk;

	bh_lock_sock(sk);
	if (!sock_owned_by_user(sk)) {
		tcp_delack_timer_handler(sk);
	} else {
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_DELAYEDACKLOCKED);
		 
		if (!test_and_set_bit(TCP_DELACK_TIMER_DEFERRED, &sk->sk_tsq_flags))
			sock_hold(sk);
	}
	bh_unlock_sock(sk);
	sock_put(sk);
}

static void tcp_probe_timer(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct sk_buff *skb = tcp_send_head(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int max_probes;

	if (tp->packets_out || !skb) {
		icsk->icsk_probes_out = 0;
		icsk->icsk_probes_tstamp = 0;
		return;
	}

	 
	if (!icsk->icsk_probes_tstamp) {
		icsk->icsk_probes_tstamp = tcp_jiffies32;
	} else {
		u32 user_timeout = READ_ONCE(icsk->icsk_user_timeout);

		if (user_timeout &&
		    (s32)(tcp_jiffies32 - icsk->icsk_probes_tstamp) >=
		     msecs_to_jiffies(user_timeout))
		goto abort;
	}
	max_probes = READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_retries2);
	if (sock_flag(sk, SOCK_DEAD)) {
		const bool alive = inet_csk_rto_backoff(icsk, TCP_RTO_MAX) < TCP_RTO_MAX;

		max_probes = tcp_orphan_retries(sk, alive);
		if (!alive && icsk->icsk_backoff >= max_probes)
			goto abort;
		if (tcp_out_of_resources(sk, true))
			return;
	}

	if (icsk->icsk_probes_out >= max_probes) {
abort:		tcp_write_err(sk);
	} else {
		 
		tcp_send_probe0(sk);
	}
}

 
static void tcp_fastopen_synack_timer(struct sock *sk, struct request_sock *req)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	int max_retries;

	req->rsk_ops->syn_ack_timeout(req);

	 
	max_retries = READ_ONCE(icsk->icsk_syn_retries) ? :
		READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_synack_retries) + 1;

	if (req->num_timeout >= max_retries) {
		tcp_write_err(sk);
		return;
	}
	 
	if (icsk->icsk_retransmits == 1)
		tcp_enter_loss(sk);
	 
	inet_rtx_syn_ack(sk, req);
	req->num_timeout++;
	icsk->icsk_retransmits++;
	if (!tp->retrans_stamp)
		tp->retrans_stamp = tcp_time_stamp(tp);
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
			  req->timeout << req->num_timeout, TCP_RTO_MAX);
}

static bool tcp_rtx_probe0_timed_out(const struct sock *sk,
				     const struct sk_buff *skb)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	const int timeout = TCP_RTO_MAX * 2;
	u32 rcv_delta, rtx_delta;

	rcv_delta = inet_csk(sk)->icsk_timeout - tp->rcv_tstamp;
	if (rcv_delta <= timeout)
		return false;

	rtx_delta = (u32)msecs_to_jiffies(tcp_time_stamp(tp) -
			(tp->retrans_stamp ?: tcp_skb_timestamp(skb)));

	return rtx_delta > timeout;
}

 
void tcp_retransmit_timer(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct net *net = sock_net(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct request_sock *req;
	struct sk_buff *skb;

	req = rcu_dereference_protected(tp->fastopen_rsk,
					lockdep_sock_is_held(sk));
	if (req) {
		WARN_ON_ONCE(sk->sk_state != TCP_SYN_RECV &&
			     sk->sk_state != TCP_FIN_WAIT1);
		tcp_fastopen_synack_timer(sk, req);
		 
		return;
	}

	if (!tp->packets_out)
		return;

	skb = tcp_rtx_queue_head(sk);
	if (WARN_ON_ONCE(!skb))
		return;

	tp->tlp_high_seq = 0;

	if (!tp->snd_wnd && !sock_flag(sk, SOCK_DEAD) &&
	    !((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))) {
		 
		struct inet_sock *inet = inet_sk(sk);
		u32 rtx_delta;

		rtx_delta = tcp_time_stamp(tp) - (tp->retrans_stamp ?: tcp_skb_timestamp(skb));
		if (sk->sk_family == AF_INET) {
			net_dbg_ratelimited("Probing zero-window on %pI4:%u/%u, seq=%u:%u, recv %ums ago, lasting %ums\n",
				&inet->inet_daddr, ntohs(inet->inet_dport),
				inet->inet_num, tp->snd_una, tp->snd_nxt,
				jiffies_to_msecs(jiffies - tp->rcv_tstamp),
				rtx_delta);
		}
#if IS_ENABLED(CONFIG_IPV6)
		else if (sk->sk_family == AF_INET6) {
			net_dbg_ratelimited("Probing zero-window on %pI6:%u/%u, seq=%u:%u, recv %ums ago, lasting %ums\n",
				&sk->sk_v6_daddr, ntohs(inet->inet_dport),
				inet->inet_num, tp->snd_una, tp->snd_nxt,
				jiffies_to_msecs(jiffies - tp->rcv_tstamp),
				rtx_delta);
		}
#endif
		if (tcp_rtx_probe0_timed_out(sk, skb)) {
			tcp_write_err(sk);
			goto out;
		}
		tcp_enter_loss(sk);
		tcp_retransmit_skb(sk, skb, 1);
		__sk_dst_reset(sk);
		goto out_reset_timer;
	}

	__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPTIMEOUTS);
	if (tcp_write_timeout(sk))
		goto out;

	if (icsk->icsk_retransmits == 0) {
		int mib_idx = 0;

		if (icsk->icsk_ca_state == TCP_CA_Recovery) {
			if (tcp_is_sack(tp))
				mib_idx = LINUX_MIB_TCPSACKRECOVERYFAIL;
			else
				mib_idx = LINUX_MIB_TCPRENORECOVERYFAIL;
		} else if (icsk->icsk_ca_state == TCP_CA_Loss) {
			mib_idx = LINUX_MIB_TCPLOSSFAILURES;
		} else if ((icsk->icsk_ca_state == TCP_CA_Disorder) ||
			   tp->sacked_out) {
			if (tcp_is_sack(tp))
				mib_idx = LINUX_MIB_TCPSACKFAILURES;
			else
				mib_idx = LINUX_MIB_TCPRENOFAILURES;
		}
		if (mib_idx)
			__NET_INC_STATS(sock_net(sk), mib_idx);
	}

	tcp_enter_loss(sk);

	icsk->icsk_retransmits++;
	if (tcp_retransmit_skb(sk, tcp_rtx_queue_head(sk), 1) > 0) {
		 
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  TCP_RESOURCE_PROBE_INTERVAL,
					  TCP_RTO_MAX);
		goto out;
	}

	 
	icsk->icsk_backoff++;

out_reset_timer:
	 
	if (sk->sk_state == TCP_ESTABLISHED &&
	    (tp->thin_lto || READ_ONCE(net->ipv4.sysctl_tcp_thin_linear_timeouts)) &&
	    tcp_stream_is_thin(tp) &&
	    icsk->icsk_retransmits <= TCP_THIN_LINEAR_RETRIES) {
		icsk->icsk_backoff = 0;
		icsk->icsk_rto = clamp(__tcp_set_rto(tp),
				       tcp_rto_min(sk),
				       TCP_RTO_MAX);
	} else if (sk->sk_state != TCP_SYN_SENT ||
		   icsk->icsk_backoff >
		   READ_ONCE(net->ipv4.sysctl_tcp_syn_linear_timeouts)) {
		 
		icsk->icsk_rto = min(icsk->icsk_rto << 1, TCP_RTO_MAX);
	}
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
				  tcp_clamp_rto_to_user_timeout(sk), TCP_RTO_MAX);
	if (retransmits_timed_out(sk, READ_ONCE(net->ipv4.sysctl_tcp_retries1) + 1, 0))
		__sk_dst_reset(sk);

out:;
}

 
void tcp_write_timer_handler(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	int event;

	if (((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_LISTEN)) ||
	    !icsk->icsk_pending)
		return;

	if (time_after(icsk->icsk_timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer, icsk->icsk_timeout);
		return;
	}

	tcp_mstamp_refresh(tcp_sk(sk));
	event = icsk->icsk_pending;

	switch (event) {
	case ICSK_TIME_REO_TIMEOUT:
		tcp_rack_reo_timeout(sk);
		break;
	case ICSK_TIME_LOSS_PROBE:
		tcp_send_loss_probe(sk);
		break;
	case ICSK_TIME_RETRANS:
		icsk->icsk_pending = 0;
		tcp_retransmit_timer(sk);
		break;
	case ICSK_TIME_PROBE0:
		icsk->icsk_pending = 0;
		tcp_probe_timer(sk);
		break;
	}
}

static void tcp_write_timer(struct timer_list *t)
{
	struct inet_connection_sock *icsk =
			from_timer(icsk, t, icsk_retransmit_timer);
	struct sock *sk = &icsk->icsk_inet.sk;

	bh_lock_sock(sk);
	if (!sock_owned_by_user(sk)) {
		tcp_write_timer_handler(sk);
	} else {
		 
		if (!test_and_set_bit(TCP_WRITE_TIMER_DEFERRED, &sk->sk_tsq_flags))
			sock_hold(sk);
	}
	bh_unlock_sock(sk);
	sock_put(sk);
}

void tcp_syn_ack_timeout(const struct request_sock *req)
{
	struct net *net = read_pnet(&inet_rsk(req)->ireq_net);

	__NET_INC_STATS(net, LINUX_MIB_TCPTIMEOUTS);
}
EXPORT_SYMBOL(tcp_syn_ack_timeout);

void tcp_set_keepalive(struct sock *sk, int val)
{
	if ((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_LISTEN))
		return;

	if (val && !sock_flag(sk, SOCK_KEEPOPEN))
		inet_csk_reset_keepalive_timer(sk, keepalive_time_when(tcp_sk(sk)));
	else if (!val)
		inet_csk_delete_keepalive_timer(sk);
}
EXPORT_SYMBOL_GPL(tcp_set_keepalive);


static void tcp_keepalive_timer (struct timer_list *t)
{
	struct sock *sk = from_timer(sk, t, sk_timer);
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	u32 elapsed;

	 
	bh_lock_sock(sk);
	if (sock_owned_by_user(sk)) {
		 
		inet_csk_reset_keepalive_timer (sk, HZ/20);
		goto out;
	}

	if (sk->sk_state == TCP_LISTEN) {
		pr_err("Hmm... keepalive on a LISTEN ???\n");
		goto out;
	}

	tcp_mstamp_refresh(tp);
	if (sk->sk_state == TCP_FIN_WAIT2 && sock_flag(sk, SOCK_DEAD)) {
		if (READ_ONCE(tp->linger2) >= 0) {
			const int tmo = tcp_fin_time(sk) - TCP_TIMEWAIT_LEN;

			if (tmo > 0) {
				tcp_time_wait(sk, TCP_FIN_WAIT2, tmo);
				goto out;
			}
		}
		tcp_send_active_reset(sk, GFP_ATOMIC);
		goto death;
	}

	if (!sock_flag(sk, SOCK_KEEPOPEN) ||
	    ((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_SYN_SENT)))
		goto out;

	elapsed = keepalive_time_when(tp);

	 
	if (tp->packets_out || !tcp_write_queue_empty(sk))
		goto resched;

	elapsed = keepalive_time_elapsed(tp);

	if (elapsed >= keepalive_time_when(tp)) {
		u32 user_timeout = READ_ONCE(icsk->icsk_user_timeout);

		 
		if ((user_timeout != 0 &&
		    elapsed >= msecs_to_jiffies(user_timeout) &&
		    icsk->icsk_probes_out > 0) ||
		    (user_timeout == 0 &&
		    icsk->icsk_probes_out >= keepalive_probes(tp))) {
			tcp_send_active_reset(sk, GFP_ATOMIC);
			tcp_write_err(sk);
			goto out;
		}
		if (tcp_write_wakeup(sk, LINUX_MIB_TCPKEEPALIVE) <= 0) {
			icsk->icsk_probes_out++;
			elapsed = keepalive_intvl_when(tp);
		} else {
			 
			elapsed = TCP_RESOURCE_PROBE_INTERVAL;
		}
	} else {
		 
		elapsed = keepalive_time_when(tp) - elapsed;
	}

resched:
	inet_csk_reset_keepalive_timer (sk, elapsed);
	goto out;

death:
	tcp_done(sk);

out:
	bh_unlock_sock(sk);
	sock_put(sk);
}

static enum hrtimer_restart tcp_compressed_ack_kick(struct hrtimer *timer)
{
	struct tcp_sock *tp = container_of(timer, struct tcp_sock, compressed_ack_timer);
	struct sock *sk = (struct sock *)tp;

	bh_lock_sock(sk);
	if (!sock_owned_by_user(sk)) {
		if (tp->compressed_ack) {
			 
			tp->compressed_ack--;
			tcp_send_ack(sk);
		}
	} else {
		if (!test_and_set_bit(TCP_DELACK_TIMER_DEFERRED,
				      &sk->sk_tsq_flags))
			sock_hold(sk);
	}
	bh_unlock_sock(sk);

	sock_put(sk);

	return HRTIMER_NORESTART;
}

void tcp_init_xmit_timers(struct sock *sk)
{
	inet_csk_init_xmit_timers(sk, &tcp_write_timer, &tcp_delack_timer,
				  &tcp_keepalive_timer);
	hrtimer_init(&tcp_sk(sk)->pacing_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_ABS_PINNED_SOFT);
	tcp_sk(sk)->pacing_timer.function = tcp_pace_kick;

	hrtimer_init(&tcp_sk(sk)->compressed_ack_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL_PINNED_SOFT);
	tcp_sk(sk)->compressed_ack_timer.function = tcp_compressed_ack_kick;
}

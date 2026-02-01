
 

#include <net/tcp.h>
#include <net/xfrm.h>
#include <net/busy_poll.h>

static bool tcp_in_window(u32 seq, u32 end_seq, u32 s_win, u32 e_win)
{
	if (seq == s_win)
		return true;
	if (after(end_seq, s_win) && before(seq, e_win))
		return true;
	return seq == e_win && seq == end_seq;
}

static enum tcp_tw_status
tcp_timewait_check_oow_rate_limit(struct inet_timewait_sock *tw,
				  const struct sk_buff *skb, int mib_idx)
{
	struct tcp_timewait_sock *tcptw = tcp_twsk((struct sock *)tw);

	if (!tcp_oow_rate_limited(twsk_net(tw), skb, mib_idx,
				  &tcptw->tw_last_oow_ack_time)) {
		 
		return TCP_TW_ACK;
	}

	 
	inet_twsk_put(tw);
	return TCP_TW_SUCCESS;
}

 
enum tcp_tw_status
tcp_timewait_state_process(struct inet_timewait_sock *tw, struct sk_buff *skb,
			   const struct tcphdr *th)
{
	struct tcp_options_received tmp_opt;
	struct tcp_timewait_sock *tcptw = tcp_twsk((struct sock *)tw);
	bool paws_reject = false;

	tmp_opt.saw_tstamp = 0;
	if (th->doff > (sizeof(*th) >> 2) && tcptw->tw_ts_recent_stamp) {
		tcp_parse_options(twsk_net(tw), skb, &tmp_opt, 0, NULL);

		if (tmp_opt.saw_tstamp) {
			if (tmp_opt.rcv_tsecr)
				tmp_opt.rcv_tsecr -= tcptw->tw_ts_offset;
			tmp_opt.ts_recent	= tcptw->tw_ts_recent;
			tmp_opt.ts_recent_stamp	= tcptw->tw_ts_recent_stamp;
			paws_reject = tcp_paws_reject(&tmp_opt, th->rst);
		}
	}

	if (tw->tw_substate == TCP_FIN_WAIT2) {
		 

		 
		if (paws_reject ||
		    !tcp_in_window(TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
				   tcptw->tw_rcv_nxt,
				   tcptw->tw_rcv_nxt + tcptw->tw_rcv_wnd))
			return tcp_timewait_check_oow_rate_limit(
				tw, skb, LINUX_MIB_TCPACKSKIPPEDFINWAIT2);

		if (th->rst)
			goto kill;

		if (th->syn && !before(TCP_SKB_CB(skb)->seq, tcptw->tw_rcv_nxt))
			return TCP_TW_RST;

		 
		if (!th->ack ||
		    !after(TCP_SKB_CB(skb)->end_seq, tcptw->tw_rcv_nxt) ||
		    TCP_SKB_CB(skb)->end_seq == TCP_SKB_CB(skb)->seq) {
			inet_twsk_put(tw);
			return TCP_TW_SUCCESS;
		}

		 
		if (!th->fin ||
		    TCP_SKB_CB(skb)->end_seq != tcptw->tw_rcv_nxt + 1)
			return TCP_TW_RST;

		 
		tw->tw_substate	  = TCP_TIME_WAIT;
		tcptw->tw_rcv_nxt = TCP_SKB_CB(skb)->end_seq;
		if (tmp_opt.saw_tstamp) {
			tcptw->tw_ts_recent_stamp = ktime_get_seconds();
			tcptw->tw_ts_recent	  = tmp_opt.rcv_tsval;
		}

		inet_twsk_reschedule(tw, TCP_TIMEWAIT_LEN);
		return TCP_TW_ACK;
	}

	 

	if (!paws_reject &&
	    (TCP_SKB_CB(skb)->seq == tcptw->tw_rcv_nxt &&
	     (TCP_SKB_CB(skb)->seq == TCP_SKB_CB(skb)->end_seq || th->rst))) {
		 

		if (th->rst) {
			 
			if (!READ_ONCE(twsk_net(tw)->ipv4.sysctl_tcp_rfc1337)) {
kill:
				inet_twsk_deschedule_put(tw);
				return TCP_TW_SUCCESS;
			}
		} else {
			inet_twsk_reschedule(tw, TCP_TIMEWAIT_LEN);
		}

		if (tmp_opt.saw_tstamp) {
			tcptw->tw_ts_recent	  = tmp_opt.rcv_tsval;
			tcptw->tw_ts_recent_stamp = ktime_get_seconds();
		}

		inet_twsk_put(tw);
		return TCP_TW_SUCCESS;
	}

	 

	if (th->syn && !th->rst && !th->ack && !paws_reject &&
	    (after(TCP_SKB_CB(skb)->seq, tcptw->tw_rcv_nxt) ||
	     (tmp_opt.saw_tstamp &&
	      (s32)(tcptw->tw_ts_recent - tmp_opt.rcv_tsval) < 0))) {
		u32 isn = tcptw->tw_snd_nxt + 65535 + 2;
		if (isn == 0)
			isn++;
		TCP_SKB_CB(skb)->tcp_tw_isn = isn;
		return TCP_TW_SYN;
	}

	if (paws_reject)
		__NET_INC_STATS(twsk_net(tw), LINUX_MIB_PAWSESTABREJECTED);

	if (!th->rst) {
		 
		if (paws_reject || th->ack)
			inet_twsk_reschedule(tw, TCP_TIMEWAIT_LEN);

		return tcp_timewait_check_oow_rate_limit(
			tw, skb, LINUX_MIB_TCPACKSKIPPEDTIMEWAIT);
	}
	inet_twsk_put(tw);
	return TCP_TW_SUCCESS;
}
EXPORT_SYMBOL(tcp_timewait_state_process);

static void tcp_time_wait_init(struct sock *sk, struct tcp_timewait_sock *tcptw)
{
#ifdef CONFIG_TCP_MD5SIG
	const struct tcp_sock *tp = tcp_sk(sk);
	struct tcp_md5sig_key *key;

	 
	tcptw->tw_md5_key = NULL;
	if (!static_branch_unlikely(&tcp_md5_needed.key))
		return;

	key = tp->af_specific->md5_lookup(sk, sk);
	if (key) {
		tcptw->tw_md5_key = kmemdup(key, sizeof(*key), GFP_ATOMIC);
		if (!tcptw->tw_md5_key)
			return;
		if (!tcp_alloc_md5sig_pool())
			goto out_free;
		if (!static_key_fast_inc_not_disabled(&tcp_md5_needed.key.key))
			goto out_free;
	}
	return;
out_free:
	WARN_ON_ONCE(1);
	kfree(tcptw->tw_md5_key);
	tcptw->tw_md5_key = NULL;
#endif
}

 
void tcp_time_wait(struct sock *sk, int state, int timeo)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	const struct tcp_sock *tp = tcp_sk(sk);
	struct net *net = sock_net(sk);
	struct inet_timewait_sock *tw;

	tw = inet_twsk_alloc(sk, &net->ipv4.tcp_death_row, state);

	if (tw) {
		struct tcp_timewait_sock *tcptw = tcp_twsk((struct sock *)tw);
		const int rto = (icsk->icsk_rto << 2) - (icsk->icsk_rto >> 1);

		tw->tw_transparent	= inet_test_bit(TRANSPARENT, sk);
		tw->tw_mark		= sk->sk_mark;
		tw->tw_priority		= sk->sk_priority;
		tw->tw_rcv_wscale	= tp->rx_opt.rcv_wscale;
		tcptw->tw_rcv_nxt	= tp->rcv_nxt;
		tcptw->tw_snd_nxt	= tp->snd_nxt;
		tcptw->tw_rcv_wnd	= tcp_receive_window(tp);
		tcptw->tw_ts_recent	= tp->rx_opt.ts_recent;
		tcptw->tw_ts_recent_stamp = tp->rx_opt.ts_recent_stamp;
		tcptw->tw_ts_offset	= tp->tsoffset;
		tcptw->tw_last_oow_ack_time = 0;
		tcptw->tw_tx_delay	= tp->tcp_tx_delay;
		tw->tw_txhash		= sk->sk_txhash;
#if IS_ENABLED(CONFIG_IPV6)
		if (tw->tw_family == PF_INET6) {
			struct ipv6_pinfo *np = inet6_sk(sk);

			tw->tw_v6_daddr = sk->sk_v6_daddr;
			tw->tw_v6_rcv_saddr = sk->sk_v6_rcv_saddr;
			tw->tw_tclass = np->tclass;
			tw->tw_flowlabel = be32_to_cpu(np->flow_label & IPV6_FLOWLABEL_MASK);
			tw->tw_ipv6only = sk->sk_ipv6only;
		}
#endif

		tcp_time_wait_init(sk, tcptw);

		 
		if (timeo < rto)
			timeo = rto;

		if (state == TCP_TIME_WAIT)
			timeo = TCP_TIMEWAIT_LEN;

		 
		local_bh_disable();
		inet_twsk_schedule(tw, timeo);
		 
		inet_twsk_hashdance(tw, sk, net->ipv4.tcp_death_row.hashinfo);
		local_bh_enable();
	} else {
		 
		NET_INC_STATS(net, LINUX_MIB_TCPTIMEWAITOVERFLOW);
	}

	tcp_update_metrics(sk);
	tcp_done(sk);
}
EXPORT_SYMBOL(tcp_time_wait);

void tcp_twsk_destructor(struct sock *sk)
{
#ifdef CONFIG_TCP_MD5SIG
	if (static_branch_unlikely(&tcp_md5_needed.key)) {
		struct tcp_timewait_sock *twsk = tcp_twsk(sk);

		if (twsk->tw_md5_key) {
			kfree_rcu(twsk->tw_md5_key, rcu);
			static_branch_slow_dec_deferred(&tcp_md5_needed);
		}
	}
#endif
}
EXPORT_SYMBOL_GPL(tcp_twsk_destructor);

void tcp_twsk_purge(struct list_head *net_exit_list, int family)
{
	bool purged_once = false;
	struct net *net;

	list_for_each_entry(net, net_exit_list, exit_list) {
		if (net->ipv4.tcp_death_row.hashinfo->pernet) {
			 
			inet_twsk_purge(net->ipv4.tcp_death_row.hashinfo, family);
		} else if (!purged_once) {
			 
			if (refcount_read(&net->ipv4.tcp_death_row.tw_refcount) == 1)
				continue;

			inet_twsk_purge(&tcp_hashinfo, family);
			purged_once = true;
		}
	}
}
EXPORT_SYMBOL_GPL(tcp_twsk_purge);

 
void tcp_openreq_init_rwin(struct request_sock *req,
			   const struct sock *sk_listener,
			   const struct dst_entry *dst)
{
	struct inet_request_sock *ireq = inet_rsk(req);
	const struct tcp_sock *tp = tcp_sk(sk_listener);
	int full_space = tcp_full_space(sk_listener);
	u32 window_clamp;
	__u8 rcv_wscale;
	u32 rcv_wnd;
	int mss;

	mss = tcp_mss_clamp(tp, dst_metric_advmss(dst));
	window_clamp = READ_ONCE(tp->window_clamp);
	 
	req->rsk_window_clamp = window_clamp ? : dst_metric(dst, RTAX_WINDOW);

	 
	if (sk_listener->sk_userlocks & SOCK_RCVBUF_LOCK &&
	    (req->rsk_window_clamp > full_space || req->rsk_window_clamp == 0))
		req->rsk_window_clamp = full_space;

	rcv_wnd = tcp_rwnd_init_bpf((struct sock *)req);
	if (rcv_wnd == 0)
		rcv_wnd = dst_metric(dst, RTAX_INITRWND);
	else if (full_space < rcv_wnd * mss)
		full_space = rcv_wnd * mss;

	 
	tcp_select_initial_window(sk_listener, full_space,
		mss - (ireq->tstamp_ok ? TCPOLEN_TSTAMP_ALIGNED : 0),
		&req->rsk_rcv_wnd,
		&req->rsk_window_clamp,
		ireq->wscale_ok,
		&rcv_wscale,
		rcv_wnd);
	ireq->rcv_wscale = rcv_wscale;
}
EXPORT_SYMBOL(tcp_openreq_init_rwin);

static void tcp_ecn_openreq_child(struct tcp_sock *tp,
				  const struct request_sock *req)
{
	tp->ecn_flags = inet_rsk(req)->ecn_ok ? TCP_ECN_OK : 0;
}

void tcp_ca_openreq_child(struct sock *sk, const struct dst_entry *dst)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	u32 ca_key = dst_metric(dst, RTAX_CC_ALGO);
	bool ca_got_dst = false;

	if (ca_key != TCP_CA_UNSPEC) {
		const struct tcp_congestion_ops *ca;

		rcu_read_lock();
		ca = tcp_ca_find_key(ca_key);
		if (likely(ca && bpf_try_module_get(ca, ca->owner))) {
			icsk->icsk_ca_dst_locked = tcp_ca_dst_locked(dst);
			icsk->icsk_ca_ops = ca;
			ca_got_dst = true;
		}
		rcu_read_unlock();
	}

	 
	if (!ca_got_dst &&
	    (!icsk->icsk_ca_setsockopt ||
	     !bpf_try_module_get(icsk->icsk_ca_ops, icsk->icsk_ca_ops->owner)))
		tcp_assign_congestion_control(sk);

	tcp_set_ca_state(sk, TCP_CA_Open);
}
EXPORT_SYMBOL_GPL(tcp_ca_openreq_child);

static void smc_check_reset_syn_req(const struct tcp_sock *oldtp,
				    struct request_sock *req,
				    struct tcp_sock *newtp)
{
#if IS_ENABLED(CONFIG_SMC)
	struct inet_request_sock *ireq;

	if (static_branch_unlikely(&tcp_have_smc)) {
		ireq = inet_rsk(req);
		if (oldtp->syn_smc && !ireq->smc_ok)
			newtp->syn_smc = 0;
	}
#endif
}

 
struct sock *tcp_create_openreq_child(const struct sock *sk,
				      struct request_sock *req,
				      struct sk_buff *skb)
{
	struct sock *newsk = inet_csk_clone_lock(sk, req, GFP_ATOMIC);
	const struct inet_request_sock *ireq = inet_rsk(req);
	struct tcp_request_sock *treq = tcp_rsk(req);
	struct inet_connection_sock *newicsk;
	const struct tcp_sock *oldtp;
	struct tcp_sock *newtp;
	u32 seq;

	if (!newsk)
		return NULL;

	newicsk = inet_csk(newsk);
	newtp = tcp_sk(newsk);
	oldtp = tcp_sk(sk);

	smc_check_reset_syn_req(oldtp, req, newtp);

	 
	newtp->pred_flags = 0;

	seq = treq->rcv_isn + 1;
	newtp->rcv_wup = seq;
	WRITE_ONCE(newtp->copied_seq, seq);
	WRITE_ONCE(newtp->rcv_nxt, seq);
	newtp->segs_in = 1;

	seq = treq->snt_isn + 1;
	newtp->snd_sml = newtp->snd_una = seq;
	WRITE_ONCE(newtp->snd_nxt, seq);
	newtp->snd_up = seq;

	INIT_LIST_HEAD(&newtp->tsq_node);
	INIT_LIST_HEAD(&newtp->tsorted_sent_queue);

	tcp_init_wl(newtp, treq->rcv_isn);

	minmax_reset(&newtp->rtt_min, tcp_jiffies32, ~0U);
	newicsk->icsk_ack.lrcvtime = tcp_jiffies32;

	newtp->lsndtime = tcp_jiffies32;
	newsk->sk_txhash = READ_ONCE(treq->txhash);
	newtp->total_retrans = req->num_retrans;

	tcp_init_xmit_timers(newsk);
	WRITE_ONCE(newtp->write_seq, newtp->pushed_seq = treq->snt_isn + 1);

	if (sock_flag(newsk, SOCK_KEEPOPEN))
		inet_csk_reset_keepalive_timer(newsk,
					       keepalive_time_when(newtp));

	newtp->rx_opt.tstamp_ok = ireq->tstamp_ok;
	newtp->rx_opt.sack_ok = ireq->sack_ok;
	newtp->window_clamp = req->rsk_window_clamp;
	newtp->rcv_ssthresh = req->rsk_rcv_wnd;
	newtp->rcv_wnd = req->rsk_rcv_wnd;
	newtp->rx_opt.wscale_ok = ireq->wscale_ok;
	if (newtp->rx_opt.wscale_ok) {
		newtp->rx_opt.snd_wscale = ireq->snd_wscale;
		newtp->rx_opt.rcv_wscale = ireq->rcv_wscale;
	} else {
		newtp->rx_opt.snd_wscale = newtp->rx_opt.rcv_wscale = 0;
		newtp->window_clamp = min(newtp->window_clamp, 65535U);
	}
	newtp->snd_wnd = ntohs(tcp_hdr(skb)->window) << newtp->rx_opt.snd_wscale;
	newtp->max_window = newtp->snd_wnd;

	if (newtp->rx_opt.tstamp_ok) {
		newtp->rx_opt.ts_recent = READ_ONCE(req->ts_recent);
		newtp->rx_opt.ts_recent_stamp = ktime_get_seconds();
		newtp->tcp_header_len = sizeof(struct tcphdr) + TCPOLEN_TSTAMP_ALIGNED;
	} else {
		newtp->rx_opt.ts_recent_stamp = 0;
		newtp->tcp_header_len = sizeof(struct tcphdr);
	}
	if (req->num_timeout) {
		newtp->undo_marker = treq->snt_isn;
		newtp->retrans_stamp = div_u64(treq->snt_synack,
					       USEC_PER_SEC / TCP_TS_HZ);
	}
	newtp->tsoffset = treq->ts_off;
#ifdef CONFIG_TCP_MD5SIG
	newtp->md5sig_info = NULL;	 
#endif
	if (skb->len >= TCP_MSS_DEFAULT + newtp->tcp_header_len)
		newicsk->icsk_ack.last_seg_size = skb->len - newtp->tcp_header_len;
	newtp->rx_opt.mss_clamp = req->mss;
	tcp_ecn_openreq_child(newtp, req);
	newtp->fastopen_req = NULL;
	RCU_INIT_POINTER(newtp->fastopen_rsk, NULL);

	newtp->bpf_chg_cc_inprogress = 0;
	tcp_bpf_clone(sk, newsk);

	__TCP_INC_STATS(sock_net(sk), TCP_MIB_PASSIVEOPENS);

	return newsk;
}
EXPORT_SYMBOL(tcp_create_openreq_child);

 

struct sock *tcp_check_req(struct sock *sk, struct sk_buff *skb,
			   struct request_sock *req,
			   bool fastopen, bool *req_stolen)
{
	struct tcp_options_received tmp_opt;
	struct sock *child;
	const struct tcphdr *th = tcp_hdr(skb);
	__be32 flg = tcp_flag_word(th) & (TCP_FLAG_RST|TCP_FLAG_SYN|TCP_FLAG_ACK);
	bool paws_reject = false;
	bool own_req;

	tmp_opt.saw_tstamp = 0;
	if (th->doff > (sizeof(struct tcphdr)>>2)) {
		tcp_parse_options(sock_net(sk), skb, &tmp_opt, 0, NULL);

		if (tmp_opt.saw_tstamp) {
			tmp_opt.ts_recent = READ_ONCE(req->ts_recent);
			if (tmp_opt.rcv_tsecr)
				tmp_opt.rcv_tsecr -= tcp_rsk(req)->ts_off;
			 
			tmp_opt.ts_recent_stamp = ktime_get_seconds() - reqsk_timeout(req, TCP_RTO_MAX) / HZ;
			paws_reject = tcp_paws_reject(&tmp_opt, th->rst);
		}
	}

	 
	if (TCP_SKB_CB(skb)->seq == tcp_rsk(req)->rcv_isn &&
	    flg == TCP_FLAG_SYN &&
	    !paws_reject) {
		 
		if (!tcp_oow_rate_limited(sock_net(sk), skb,
					  LINUX_MIB_TCPACKSKIPPEDSYNRECV,
					  &tcp_rsk(req)->last_oow_ack_time) &&

		    !inet_rtx_syn_ack(sk, req)) {
			unsigned long expires = jiffies;

			expires += reqsk_timeout(req, TCP_RTO_MAX);
			if (!fastopen)
				mod_timer_pending(&req->rsk_timer, expires);
			else
				req->rsk_timer.expires = expires;
		}
		return NULL;
	}

	 

	 
	if ((flg & TCP_FLAG_ACK) && !fastopen &&
	    (TCP_SKB_CB(skb)->ack_seq !=
	     tcp_rsk(req)->snt_isn + 1))
		return sk;

	 

	 

	if (paws_reject || !tcp_in_window(TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
					  tcp_rsk(req)->rcv_nxt, tcp_rsk(req)->rcv_nxt + req->rsk_rcv_wnd)) {
		 
		if (!(flg & TCP_FLAG_RST) &&
		    !tcp_oow_rate_limited(sock_net(sk), skb,
					  LINUX_MIB_TCPACKSKIPPEDSYNRECV,
					  &tcp_rsk(req)->last_oow_ack_time))
			req->rsk_ops->send_ack(sk, skb, req);
		if (paws_reject)
			NET_INC_STATS(sock_net(sk), LINUX_MIB_PAWSESTABREJECTED);
		return NULL;
	}

	 

	 
	if (tmp_opt.saw_tstamp && !after(TCP_SKB_CB(skb)->seq, tcp_rsk(req)->rcv_nxt))
		WRITE_ONCE(req->ts_recent, tmp_opt.rcv_tsval);

	if (TCP_SKB_CB(skb)->seq == tcp_rsk(req)->rcv_isn) {
		 
		flg &= ~TCP_FLAG_SYN;
	}

	 
	if (flg & (TCP_FLAG_RST|TCP_FLAG_SYN)) {
		TCP_INC_STATS(sock_net(sk), TCP_MIB_ATTEMPTFAILS);
		goto embryonic_reset;
	}

	 
	if (!(flg & TCP_FLAG_ACK))
		return NULL;

	 
	if (fastopen)
		return sk;

	 
	if (req->num_timeout < READ_ONCE(inet_csk(sk)->icsk_accept_queue.rskq_defer_accept) &&
	    TCP_SKB_CB(skb)->end_seq == tcp_rsk(req)->rcv_isn + 1) {
		inet_rsk(req)->acked = 1;
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPDEFERACCEPTDROP);
		return NULL;
	}

	 
	child = inet_csk(sk)->icsk_af_ops->syn_recv_sock(sk, skb, req, NULL,
							 req, &own_req);
	if (!child)
		goto listen_overflow;

	if (own_req && rsk_drop_req(req)) {
		reqsk_queue_removed(&inet_csk(req->rsk_listener)->icsk_accept_queue, req);
		inet_csk_reqsk_queue_drop_and_put(req->rsk_listener, req);
		return child;
	}

	sock_rps_save_rxhash(child, skb);
	tcp_synack_rtt_meas(child, req);
	*req_stolen = !own_req;
	return inet_csk_complete_hashdance(sk, child, req, own_req);

listen_overflow:
	if (sk != req->rsk_listener)
		__NET_INC_STATS(sock_net(sk), LINUX_MIB_TCPMIGRATEREQFAILURE);

	if (!READ_ONCE(sock_net(sk)->ipv4.sysctl_tcp_abort_on_overflow)) {
		inet_rsk(req)->acked = 1;
		return NULL;
	}

embryonic_reset:
	if (!(flg & TCP_FLAG_RST)) {
		 
		req->rsk_ops->send_reset(sk, skb);
	} else if (fastopen) {  
		reqsk_fastopen_remove(sk, req, true);
		tcp_reset(sk, skb);
	}
	if (!fastopen) {
		bool unlinked = inet_csk_reqsk_queue_drop(sk, req);

		if (unlinked)
			__NET_INC_STATS(sock_net(sk), LINUX_MIB_EMBRYONICRSTS);
		*req_stolen = !unlinked;
	}
	return NULL;
}
EXPORT_SYMBOL(tcp_check_req);

 

int tcp_child_process(struct sock *parent, struct sock *child,
		      struct sk_buff *skb)
	__releases(&((child)->sk_lock.slock))
{
	int ret = 0;
	int state = child->sk_state;

	 
	sk_mark_napi_id_set(child, skb);

	tcp_segs_in(tcp_sk(child), skb);
	if (!sock_owned_by_user(child)) {
		ret = tcp_rcv_state_process(child, skb);
		 
		if (state == TCP_SYN_RECV && child->sk_state != state)
			parent->sk_data_ready(parent);
	} else {
		 
		__sk_add_backlog(child, skb);
	}

	bh_unlock_sock(child);
	sock_put(child);
	return ret;
}
EXPORT_SYMBOL(tcp_child_process);

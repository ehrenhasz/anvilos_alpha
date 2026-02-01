
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/random.h>
#include <net/sctp/sctp.h>
#include <net/sctp/sm.h>

 

 
static struct sctp_transport *sctp_transport_init(struct net *net,
						  struct sctp_transport *peer,
						  const union sctp_addr *addr,
						  gfp_t gfp)
{
	 
	peer->af_specific = sctp_get_af_specific(addr->sa.sa_family);
	memcpy(&peer->ipaddr, addr, peer->af_specific->sockaddr_len);
	memset(&peer->saddr, 0, sizeof(union sctp_addr));

	peer->sack_generation = 0;

	 
	peer->rto = msecs_to_jiffies(net->sctp.rto_initial);

	peer->last_time_heard = 0;
	peer->last_time_ecne_reduced = jiffies;

	peer->param_flags = SPP_HB_DISABLE |
			    SPP_PMTUD_ENABLE |
			    SPP_SACKDELAY_ENABLE;

	 
	peer->pathmaxrxt  = net->sctp.max_retrans_path;
	peer->pf_retrans  = net->sctp.pf_retrans;

	INIT_LIST_HEAD(&peer->transmitted);
	INIT_LIST_HEAD(&peer->send_ready);
	INIT_LIST_HEAD(&peer->transports);

	timer_setup(&peer->T3_rtx_timer, sctp_generate_t3_rtx_event, 0);
	timer_setup(&peer->hb_timer, sctp_generate_heartbeat_event, 0);
	timer_setup(&peer->reconf_timer, sctp_generate_reconf_event, 0);
	timer_setup(&peer->probe_timer, sctp_generate_probe_event, 0);
	timer_setup(&peer->proto_unreach_timer,
		    sctp_generate_proto_unreach_event, 0);

	 
	get_random_bytes(&peer->hb_nonce, sizeof(peer->hb_nonce));

	refcount_set(&peer->refcnt, 1);

	return peer;
}

 
struct sctp_transport *sctp_transport_new(struct net *net,
					  const union sctp_addr *addr,
					  gfp_t gfp)
{
	struct sctp_transport *transport;

	transport = kzalloc(sizeof(*transport), gfp);
	if (!transport)
		goto fail;

	if (!sctp_transport_init(net, transport, addr, gfp))
		goto fail_init;

	SCTP_DBG_OBJCNT_INC(transport);

	return transport;

fail_init:
	kfree(transport);

fail:
	return NULL;
}

 
void sctp_transport_free(struct sctp_transport *transport)
{
	 
	if (del_timer(&transport->hb_timer))
		sctp_transport_put(transport);

	 
	if (del_timer(&transport->T3_rtx_timer))
		sctp_transport_put(transport);

	if (del_timer(&transport->reconf_timer))
		sctp_transport_put(transport);

	if (del_timer(&transport->probe_timer))
		sctp_transport_put(transport);

	 
	if (del_timer(&transport->proto_unreach_timer))
		sctp_transport_put(transport);

	sctp_transport_put(transport);
}

static void sctp_transport_destroy_rcu(struct rcu_head *head)
{
	struct sctp_transport *transport;

	transport = container_of(head, struct sctp_transport, rcu);

	dst_release(transport->dst);
	kfree(transport);
	SCTP_DBG_OBJCNT_DEC(transport);
}

 
static void sctp_transport_destroy(struct sctp_transport *transport)
{
	if (unlikely(refcount_read(&transport->refcnt))) {
		WARN(1, "Attempt to destroy undead transport %p!\n", transport);
		return;
	}

	sctp_packet_free(&transport->packet);

	if (transport->asoc)
		sctp_association_put(transport->asoc);

	call_rcu(&transport->rcu, sctp_transport_destroy_rcu);
}

 
void sctp_transport_reset_t3_rtx(struct sctp_transport *transport)
{
	 

	if (!timer_pending(&transport->T3_rtx_timer))
		if (!mod_timer(&transport->T3_rtx_timer,
			       jiffies + transport->rto))
			sctp_transport_hold(transport);
}

void sctp_transport_reset_hb_timer(struct sctp_transport *transport)
{
	unsigned long expires;

	 
	expires = jiffies + sctp_transport_timeout(transport);
	if (!mod_timer(&transport->hb_timer,
		       expires + get_random_u32_below(transport->rto)))
		sctp_transport_hold(transport);
}

void sctp_transport_reset_reconf_timer(struct sctp_transport *transport)
{
	if (!timer_pending(&transport->reconf_timer))
		if (!mod_timer(&transport->reconf_timer,
			       jiffies + transport->rto))
			sctp_transport_hold(transport);
}

void sctp_transport_reset_probe_timer(struct sctp_transport *transport)
{
	if (!mod_timer(&transport->probe_timer,
		       jiffies + transport->probe_interval))
		sctp_transport_hold(transport);
}

void sctp_transport_reset_raise_timer(struct sctp_transport *transport)
{
	if (!mod_timer(&transport->probe_timer,
		       jiffies + transport->probe_interval * 30))
		sctp_transport_hold(transport);
}

 
void sctp_transport_set_owner(struct sctp_transport *transport,
			      struct sctp_association *asoc)
{
	transport->asoc = asoc;
	sctp_association_hold(asoc);
}

 
void sctp_transport_pmtu(struct sctp_transport *transport, struct sock *sk)
{
	 
	if (!transport->dst || transport->dst->obsolete) {
		sctp_transport_dst_release(transport);
		transport->af_specific->get_dst(transport, &transport->saddr,
						&transport->fl, sk);
	}

	if (transport->param_flags & SPP_PMTUD_DISABLE) {
		struct sctp_association *asoc = transport->asoc;

		if (!transport->pathmtu && asoc && asoc->pathmtu)
			transport->pathmtu = asoc->pathmtu;
		if (transport->pathmtu)
			return;
	}

	if (transport->dst)
		transport->pathmtu = sctp_dst_mtu(transport->dst);
	else
		transport->pathmtu = SCTP_DEFAULT_MAXSEGMENT;

	sctp_transport_pl_update(transport);
}

void sctp_transport_pl_send(struct sctp_transport *t)
{
	if (t->pl.probe_count < SCTP_MAX_PROBES)
		goto out;

	t->pl.probe_count = 0;
	if (t->pl.state == SCTP_PL_BASE) {
		if (t->pl.probe_size == SCTP_BASE_PLPMTU) {  
			t->pl.state = SCTP_PL_ERROR;  

			t->pl.pmtu = SCTP_BASE_PLPMTU;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			sctp_assoc_sync_pmtu(t->asoc);
		}
	} else if (t->pl.state == SCTP_PL_SEARCH) {
		if (t->pl.pmtu == t->pl.probe_size) {  
			t->pl.state = SCTP_PL_BASE;   
			t->pl.probe_size = SCTP_BASE_PLPMTU;
			t->pl.probe_high = 0;

			t->pl.pmtu = SCTP_BASE_PLPMTU;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			sctp_assoc_sync_pmtu(t->asoc);
		} else {  
			t->pl.probe_high = t->pl.probe_size;
			t->pl.probe_size = t->pl.pmtu;
		}
	} else if (t->pl.state == SCTP_PL_COMPLETE) {
		if (t->pl.pmtu == t->pl.probe_size) {  
			t->pl.state = SCTP_PL_BASE;   
			t->pl.probe_size = SCTP_BASE_PLPMTU;

			t->pl.pmtu = SCTP_BASE_PLPMTU;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			sctp_assoc_sync_pmtu(t->asoc);
		}
	}

out:
	pr_debug("%s: PLPMTUD: transport: %p, state: %d, pmtu: %d, size: %d, high: %d\n",
		 __func__, t, t->pl.state, t->pl.pmtu, t->pl.probe_size, t->pl.probe_high);
	t->pl.probe_count++;
}

bool sctp_transport_pl_recv(struct sctp_transport *t)
{
	pr_debug("%s: PLPMTUD: transport: %p, state: %d, pmtu: %d, size: %d, high: %d\n",
		 __func__, t, t->pl.state, t->pl.pmtu, t->pl.probe_size, t->pl.probe_high);

	t->pl.pmtu = t->pl.probe_size;
	t->pl.probe_count = 0;
	if (t->pl.state == SCTP_PL_BASE) {
		t->pl.state = SCTP_PL_SEARCH;  
		t->pl.probe_size += SCTP_PL_BIG_STEP;
	} else if (t->pl.state == SCTP_PL_ERROR) {
		t->pl.state = SCTP_PL_SEARCH;  

		t->pl.pmtu = t->pl.probe_size;
		t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
		sctp_assoc_sync_pmtu(t->asoc);
		t->pl.probe_size += SCTP_PL_BIG_STEP;
	} else if (t->pl.state == SCTP_PL_SEARCH) {
		if (!t->pl.probe_high) {
			if (t->pl.probe_size < SCTP_MAX_PLPMTU) {
				t->pl.probe_size = min(t->pl.probe_size + SCTP_PL_BIG_STEP,
						       SCTP_MAX_PLPMTU);
				return false;
			}
			t->pl.probe_high = SCTP_MAX_PLPMTU;
		}
		t->pl.probe_size += SCTP_PL_MIN_STEP;
		if (t->pl.probe_size >= t->pl.probe_high) {
			t->pl.probe_high = 0;
			t->pl.state = SCTP_PL_COMPLETE;  

			t->pl.probe_size = t->pl.pmtu;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			sctp_assoc_sync_pmtu(t->asoc);
			sctp_transport_reset_raise_timer(t);
		}
	} else if (t->pl.state == SCTP_PL_COMPLETE) {
		 
		t->pl.state = SCTP_PL_SEARCH;  
		t->pl.probe_size = min(t->pl.probe_size + SCTP_PL_MIN_STEP, SCTP_MAX_PLPMTU);
	}

	return t->pl.state == SCTP_PL_COMPLETE;
}

static bool sctp_transport_pl_toobig(struct sctp_transport *t, u32 pmtu)
{
	pr_debug("%s: PLPMTUD: transport: %p, state: %d, pmtu: %d, size: %d, ptb: %d\n",
		 __func__, t, t->pl.state, t->pl.pmtu, t->pl.probe_size, pmtu);

	if (pmtu < SCTP_MIN_PLPMTU || pmtu >= t->pl.probe_size)
		return false;

	if (t->pl.state == SCTP_PL_BASE) {
		if (pmtu >= SCTP_MIN_PLPMTU && pmtu < SCTP_BASE_PLPMTU) {
			t->pl.state = SCTP_PL_ERROR;  

			t->pl.pmtu = SCTP_BASE_PLPMTU;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			return true;
		}
	} else if (t->pl.state == SCTP_PL_SEARCH) {
		if (pmtu >= SCTP_BASE_PLPMTU && pmtu < t->pl.pmtu) {
			t->pl.state = SCTP_PL_BASE;   
			t->pl.probe_size = SCTP_BASE_PLPMTU;
			t->pl.probe_count = 0;

			t->pl.probe_high = 0;
			t->pl.pmtu = SCTP_BASE_PLPMTU;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			return true;
		} else if (pmtu > t->pl.pmtu && pmtu < t->pl.probe_size) {
			t->pl.probe_size = pmtu;
			t->pl.probe_count = 0;
		}
	} else if (t->pl.state == SCTP_PL_COMPLETE) {
		if (pmtu >= SCTP_BASE_PLPMTU && pmtu < t->pl.pmtu) {
			t->pl.state = SCTP_PL_BASE;   
			t->pl.probe_size = SCTP_BASE_PLPMTU;
			t->pl.probe_count = 0;

			t->pl.probe_high = 0;
			t->pl.pmtu = SCTP_BASE_PLPMTU;
			t->pathmtu = t->pl.pmtu + sctp_transport_pl_hlen(t);
			sctp_transport_reset_probe_timer(t);
			return true;
		}
	}

	return false;
}

bool sctp_transport_update_pmtu(struct sctp_transport *t, u32 pmtu)
{
	struct sock *sk = t->asoc->base.sk;
	struct dst_entry *dst;
	bool change = true;

	if (unlikely(pmtu < SCTP_DEFAULT_MINSEGMENT)) {
		pr_warn_ratelimited("%s: Reported pmtu %d too low, using default minimum of %d\n",
				    __func__, pmtu, SCTP_DEFAULT_MINSEGMENT);
		 
		pmtu = SCTP_DEFAULT_MINSEGMENT;
	}
	pmtu = SCTP_TRUNC4(pmtu);

	if (sctp_transport_pl_enabled(t))
		return sctp_transport_pl_toobig(t, pmtu - sctp_transport_pl_hlen(t));

	dst = sctp_transport_dst_check(t);
	if (dst) {
		struct sctp_pf *pf = sctp_get_pf_specific(dst->ops->family);
		union sctp_addr addr;

		pf->af->from_sk(&addr, sk);
		pf->to_sk_daddr(&t->ipaddr, sk);
		dst->ops->update_pmtu(dst, sk, NULL, pmtu, true);
		pf->to_sk_daddr(&addr, sk);

		dst = sctp_transport_dst_check(t);
	}

	if (!dst) {
		t->af_specific->get_dst(t, &t->saddr, &t->fl, sk);
		dst = t->dst;
	}

	if (dst) {
		 
		pmtu = sctp_dst_mtu(dst);
		change = t->pathmtu != pmtu;
	}
	t->pathmtu = pmtu;

	return change;
}

 
void sctp_transport_route(struct sctp_transport *transport,
			  union sctp_addr *saddr, struct sctp_sock *opt)
{
	struct sctp_association *asoc = transport->asoc;
	struct sctp_af *af = transport->af_specific;

	sctp_transport_dst_release(transport);
	af->get_dst(transport, saddr, &transport->fl, sctp_opt2sk(opt));

	if (saddr)
		memcpy(&transport->saddr, saddr, sizeof(union sctp_addr));
	else
		af->get_saddr(opt, transport, &transport->fl);

	sctp_transport_pmtu(transport, sctp_opt2sk(opt));

	 
	if (transport->dst && asoc &&
	    (!asoc->peer.primary_path || transport == asoc->peer.active_path))
		opt->pf->to_sk_saddr(&transport->saddr, asoc->base.sk);
}

 
int sctp_transport_hold(struct sctp_transport *transport)
{
	return refcount_inc_not_zero(&transport->refcnt);
}

 
void sctp_transport_put(struct sctp_transport *transport)
{
	if (refcount_dec_and_test(&transport->refcnt))
		sctp_transport_destroy(transport);
}

 
void sctp_transport_update_rto(struct sctp_transport *tp, __u32 rtt)
{
	if (unlikely(!tp->rto_pending))
		 
		pr_debug("%s: rto_pending not set on transport %p!\n", __func__, tp);

	if (tp->rttvar || tp->srtt) {
		struct net *net = tp->asoc->base.net;
		 

		 
		tp->rttvar = tp->rttvar - (tp->rttvar >> net->sctp.rto_beta)
			+ (((__u32)abs((__s64)tp->srtt - (__s64)rtt)) >> net->sctp.rto_beta);
		tp->srtt = tp->srtt - (tp->srtt >> net->sctp.rto_alpha)
			+ (rtt >> net->sctp.rto_alpha);
	} else {
		 
		tp->srtt = rtt;
		tp->rttvar = rtt >> 1;
	}

	 
	if (tp->rttvar == 0)
		tp->rttvar = SCTP_CLOCK_GRANULARITY;

	 
	tp->rto = tp->srtt + (tp->rttvar << 2);

	 
	if (tp->rto < tp->asoc->rto_min)
		tp->rto = tp->asoc->rto_min;

	 
	if (tp->rto > tp->asoc->rto_max)
		tp->rto = tp->asoc->rto_max;

	sctp_max_rto(tp->asoc, tp);
	tp->rtt = rtt;

	 
	tp->rto_pending = 0;

	pr_debug("%s: transport:%p, rtt:%d, srtt:%d rttvar:%d, rto:%ld\n",
		 __func__, tp, rtt, tp->srtt, tp->rttvar, tp->rto);
}

 
void sctp_transport_raise_cwnd(struct sctp_transport *transport,
			       __u32 sack_ctsn, __u32 bytes_acked)
{
	struct sctp_association *asoc = transport->asoc;
	__u32 cwnd, ssthresh, flight_size, pba, pmtu;

	cwnd = transport->cwnd;
	flight_size = transport->flight_size;

	 
	if (asoc->fast_recovery &&
	    TSN_lte(asoc->fast_recovery_exit, sack_ctsn))
		asoc->fast_recovery = 0;

	ssthresh = transport->ssthresh;
	pba = transport->partial_bytes_acked;
	pmtu = transport->asoc->pathmtu;

	if (cwnd <= ssthresh) {
		 
		if (asoc->fast_recovery)
			return;

		 
		if (flight_size < cwnd)
			return;

		if (bytes_acked > pmtu)
			cwnd += pmtu;
		else
			cwnd += bytes_acked;

		pr_debug("%s: slow start: transport:%p, bytes_acked:%d, "
			 "cwnd:%d, ssthresh:%d, flight_size:%d, pba:%d\n",
			 __func__, transport, bytes_acked, cwnd, ssthresh,
			 flight_size, pba);
	} else {
		 
		pba += bytes_acked;
		if (pba > cwnd && flight_size < cwnd)
			pba = cwnd;
		if (pba >= cwnd && flight_size >= cwnd) {
			pba = pba - cwnd;
			cwnd += pmtu;
		}

		pr_debug("%s: congestion avoidance: transport:%p, "
			 "bytes_acked:%d, cwnd:%d, ssthresh:%d, "
			 "flight_size:%d, pba:%d\n", __func__,
			 transport, bytes_acked, cwnd, ssthresh,
			 flight_size, pba);
	}

	transport->cwnd = cwnd;
	transport->partial_bytes_acked = pba;
}

 
void sctp_transport_lower_cwnd(struct sctp_transport *transport,
			       enum sctp_lower_cwnd reason)
{
	struct sctp_association *asoc = transport->asoc;

	switch (reason) {
	case SCTP_LOWER_CWND_T3_RTX:
		 
		transport->ssthresh = max(transport->cwnd/2,
					  4*asoc->pathmtu);
		transport->cwnd = asoc->pathmtu;

		 
		asoc->fast_recovery = 0;
		break;

	case SCTP_LOWER_CWND_FAST_RTX:
		 
		if (asoc->fast_recovery)
			return;

		 
		asoc->fast_recovery = 1;
		asoc->fast_recovery_exit = asoc->next_tsn - 1;

		transport->ssthresh = max(transport->cwnd/2,
					  4*asoc->pathmtu);
		transport->cwnd = transport->ssthresh;
		break;

	case SCTP_LOWER_CWND_ECNE:
		 
		if (time_after(jiffies, transport->last_time_ecne_reduced +
					transport->rtt)) {
			transport->ssthresh = max(transport->cwnd/2,
						  4*asoc->pathmtu);
			transport->cwnd = transport->ssthresh;
			transport->last_time_ecne_reduced = jiffies;
		}
		break;

	case SCTP_LOWER_CWND_INACTIVE:
		 
		transport->cwnd = max(transport->cwnd/2,
					 4*asoc->pathmtu);
		 
		transport->ssthresh = transport->cwnd;
		break;
	}

	transport->partial_bytes_acked = 0;

	pr_debug("%s: transport:%p, reason:%d, cwnd:%d, ssthresh:%d\n",
		 __func__, transport, reason, transport->cwnd,
		 transport->ssthresh);
}

 

void sctp_transport_burst_limited(struct sctp_transport *t)
{
	struct sctp_association *asoc = t->asoc;
	u32 old_cwnd = t->cwnd;
	u32 max_burst_bytes;

	if (t->burst_limited || asoc->max_burst == 0)
		return;

	max_burst_bytes = t->flight_size + (asoc->max_burst * asoc->pathmtu);
	if (max_burst_bytes < old_cwnd) {
		t->cwnd = max_burst_bytes;
		t->burst_limited = old_cwnd;
	}
}

 
void sctp_transport_burst_reset(struct sctp_transport *t)
{
	if (t->burst_limited) {
		t->cwnd = t->burst_limited;
		t->burst_limited = 0;
	}
}

 
unsigned long sctp_transport_timeout(struct sctp_transport *trans)
{
	 
	unsigned long timeout = trans->rto >> 1;

	if (trans->state != SCTP_UNCONFIRMED &&
	    trans->state != SCTP_PF)
		timeout += trans->hbinterval;

	return max_t(unsigned long, timeout, HZ / 5);
}

 
void sctp_transport_reset(struct sctp_transport *t)
{
	struct sctp_association *asoc = t->asoc;

	 
	t->cwnd = min(4*asoc->pathmtu, max_t(__u32, 2*asoc->pathmtu, 4380));
	t->burst_limited = 0;
	t->ssthresh = asoc->peer.i.a_rwnd;
	t->rto = asoc->rto_initial;
	sctp_max_rto(asoc, t);
	t->rtt = 0;
	t->srtt = 0;
	t->rttvar = 0;

	 
	t->partial_bytes_acked = 0;
	t->flight_size = 0;
	t->error_count = 0;
	t->rto_pending = 0;
	t->hb_sent = 0;

	 
	t->cacc.changeover_active = 0;
	t->cacc.cycling_changeover = 0;
	t->cacc.next_tsn_at_change = 0;
	t->cacc.cacc_saw_newack = 0;
}

 
void sctp_transport_immediate_rtx(struct sctp_transport *t)
{
	 
	if (del_timer(&t->T3_rtx_timer))
		sctp_transport_put(t);

	sctp_retransmit(&t->asoc->outqueue, t, SCTP_RTXR_T3_RTX);
	if (!timer_pending(&t->T3_rtx_timer)) {
		if (!mod_timer(&t->T3_rtx_timer, jiffies + t->rto))
			sctp_transport_hold(t);
	}
}

 
void sctp_transport_dst_release(struct sctp_transport *t)
{
	dst_release(t->dst);
	t->dst = NULL;
	t->dst_pending_confirm = 0;
}

 
void sctp_transport_dst_confirm(struct sctp_transport *t)
{
	t->dst_pending_confirm = 1;
}

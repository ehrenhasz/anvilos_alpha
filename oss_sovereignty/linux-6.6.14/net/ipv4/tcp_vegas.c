
 

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>

#include <net/tcp.h>

#include "tcp_vegas.h"

static int alpha = 2;
static int beta  = 4;
static int gamma = 1;

module_param(alpha, int, 0644);
MODULE_PARM_DESC(alpha, "lower bound of packets in network");
module_param(beta, int, 0644);
MODULE_PARM_DESC(beta, "upper bound of packets in network");
module_param(gamma, int, 0644);
MODULE_PARM_DESC(gamma, "limit on increase (scale by 2)");

 
static void vegas_enable(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct vegas *vegas = inet_csk_ca(sk);

	 
	vegas->doing_vegas_now = 1;

	 
	vegas->beg_snd_nxt = tp->snd_nxt;

	vegas->cntRTT = 0;
	vegas->minRTT = 0x7fffffff;
}

 
static inline void vegas_disable(struct sock *sk)
{
	struct vegas *vegas = inet_csk_ca(sk);

	vegas->doing_vegas_now = 0;
}

void tcp_vegas_init(struct sock *sk)
{
	struct vegas *vegas = inet_csk_ca(sk);

	vegas->baseRTT = 0x7fffffff;
	vegas_enable(sk);
}
EXPORT_SYMBOL_GPL(tcp_vegas_init);

 
void tcp_vegas_pkts_acked(struct sock *sk, const struct ack_sample *sample)
{
	struct vegas *vegas = inet_csk_ca(sk);
	u32 vrtt;

	if (sample->rtt_us < 0)
		return;

	 
	vrtt = sample->rtt_us + 1;

	 
	if (vrtt < vegas->baseRTT)
		vegas->baseRTT = vrtt;

	 
	vegas->minRTT = min(vegas->minRTT, vrtt);
	vegas->cntRTT++;
}
EXPORT_SYMBOL_GPL(tcp_vegas_pkts_acked);

void tcp_vegas_state(struct sock *sk, u8 ca_state)
{
	if (ca_state == TCP_CA_Open)
		vegas_enable(sk);
	else
		vegas_disable(sk);
}
EXPORT_SYMBOL_GPL(tcp_vegas_state);

 
void tcp_vegas_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	if (event == CA_EVENT_CWND_RESTART ||
	    event == CA_EVENT_TX_START)
		tcp_vegas_init(sk);
}
EXPORT_SYMBOL_GPL(tcp_vegas_cwnd_event);

static inline u32 tcp_vegas_ssthresh(struct tcp_sock *tp)
{
	return  min(tp->snd_ssthresh, tcp_snd_cwnd(tp));
}

static void tcp_vegas_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct vegas *vegas = inet_csk_ca(sk);

	if (!vegas->doing_vegas_now) {
		tcp_reno_cong_avoid(sk, ack, acked);
		return;
	}

	if (after(ack, vegas->beg_snd_nxt)) {
		 

		 
		vegas->beg_snd_nxt  = tp->snd_nxt;

		 

		if (vegas->cntRTT <= 2) {
			 
			tcp_reno_cong_avoid(sk, ack, acked);
		} else {
			u32 rtt, diff;
			u64 target_cwnd;

			 

			 
			rtt = vegas->minRTT;

			 
			target_cwnd = (u64)tcp_snd_cwnd(tp) * vegas->baseRTT;
			do_div(target_cwnd, rtt);

			 
			diff = tcp_snd_cwnd(tp) * (rtt-vegas->baseRTT) / vegas->baseRTT;

			if (diff > gamma && tcp_in_slow_start(tp)) {
				 

				 
				tcp_snd_cwnd_set(tp, min(tcp_snd_cwnd(tp),
							 (u32)target_cwnd + 1));
				tp->snd_ssthresh = tcp_vegas_ssthresh(tp);

			} else if (tcp_in_slow_start(tp)) {
				 
				tcp_slow_start(tp, acked);
			} else {
				 

				 
				if (diff > beta) {
					 
					tcp_snd_cwnd_set(tp, tcp_snd_cwnd(tp) - 1);
					tp->snd_ssthresh
						= tcp_vegas_ssthresh(tp);
				} else if (diff < alpha) {
					 
					tcp_snd_cwnd_set(tp, tcp_snd_cwnd(tp) + 1);
				} else {
					 
				}
			}

			if (tcp_snd_cwnd(tp) < 2)
				tcp_snd_cwnd_set(tp, 2);
			else if (tcp_snd_cwnd(tp) > tp->snd_cwnd_clamp)
				tcp_snd_cwnd_set(tp, tp->snd_cwnd_clamp);

			tp->snd_ssthresh = tcp_current_ssthresh(sk);
		}

		 
		vegas->cntRTT = 0;
		vegas->minRTT = 0x7fffffff;
	}
	 
	else if (tcp_in_slow_start(tp))
		tcp_slow_start(tp, acked);
}

 
size_t tcp_vegas_get_info(struct sock *sk, u32 ext, int *attr,
			  union tcp_cc_info *info)
{
	const struct vegas *ca = inet_csk_ca(sk);

	if (ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		info->vegas.tcpv_enabled = ca->doing_vegas_now;
		info->vegas.tcpv_rttcnt = ca->cntRTT;
		info->vegas.tcpv_rtt = ca->baseRTT;
		info->vegas.tcpv_minrtt = ca->minRTT;

		*attr = INET_DIAG_VEGASINFO;
		return sizeof(struct tcpvegas_info);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(tcp_vegas_get_info);

static struct tcp_congestion_ops tcp_vegas __read_mostly = {
	.init		= tcp_vegas_init,
	.ssthresh	= tcp_reno_ssthresh,
	.undo_cwnd	= tcp_reno_undo_cwnd,
	.cong_avoid	= tcp_vegas_cong_avoid,
	.pkts_acked	= tcp_vegas_pkts_acked,
	.set_state	= tcp_vegas_state,
	.cwnd_event	= tcp_vegas_cwnd_event,
	.get_info	= tcp_vegas_get_info,

	.owner		= THIS_MODULE,
	.name		= "vegas",
};

static int __init tcp_vegas_register(void)
{
	BUILD_BUG_ON(sizeof(struct vegas) > ICSK_CA_PRIV_SIZE);
	tcp_register_congestion_control(&tcp_vegas);
	return 0;
}

static void __exit tcp_vegas_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_vegas);
}

module_init(tcp_vegas_register);
module_exit(tcp_vegas_unregister);

MODULE_AUTHOR("Stephen Hemminger");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Vegas");

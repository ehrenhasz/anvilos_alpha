
 
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>

#include <net/tcp.h>

#include "tcp_vegas.h"

#define TCP_YEAH_ALPHA       80  
#define TCP_YEAH_GAMMA        1  
#define TCP_YEAH_DELTA        3  
#define TCP_YEAH_EPSILON      1  
#define TCP_YEAH_PHY          8  
#define TCP_YEAH_RHO         16  
#define TCP_YEAH_ZETA        50  

#define TCP_SCALABLE_AI_CNT	 100U

 
struct yeah {
	struct vegas vegas;	 

	 
	u32 lastQ;
	u32 doing_reno_now;

	u32 reno_count;
	u32 fast_count;
};

static void tcp_yeah_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct yeah *yeah = inet_csk_ca(sk);

	tcp_vegas_init(sk);

	yeah->doing_reno_now = 0;
	yeah->lastQ = 0;

	yeah->reno_count = 2;

	 
	tp->snd_cwnd_clamp = min_t(u32, tp->snd_cwnd_clamp, 0xffffffff/128);
}

static void tcp_yeah_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct yeah *yeah = inet_csk_ca(sk);

	if (!tcp_is_cwnd_limited(sk))
		return;

	if (tcp_in_slow_start(tp)) {
		acked = tcp_slow_start(tp, acked);
		if (!acked)
			goto do_vegas;
	}

	if (!yeah->doing_reno_now) {
		 
		tcp_cong_avoid_ai(tp, min(tcp_snd_cwnd(tp), TCP_SCALABLE_AI_CNT),
				  acked);
	} else {
		 
		tcp_cong_avoid_ai(tp, tcp_snd_cwnd(tp), acked);
	}

	 
do_vegas:
	if (after(ack, yeah->vegas.beg_snd_nxt)) {
		 

		if (yeah->vegas.cntRTT > 2) {
			u32 rtt, queue;
			u64 bw;

			 

			 
			rtt = yeah->vegas.minRTT;

			 
			bw = tcp_snd_cwnd(tp);
			bw *= rtt - yeah->vegas.baseRTT;
			do_div(bw, rtt);
			queue = bw;

			if (queue > TCP_YEAH_ALPHA ||
			    rtt - yeah->vegas.baseRTT > (yeah->vegas.baseRTT / TCP_YEAH_PHY)) {
				if (queue > TCP_YEAH_ALPHA &&
				    tcp_snd_cwnd(tp) > yeah->reno_count) {
					u32 reduction = min(queue / TCP_YEAH_GAMMA ,
							    tcp_snd_cwnd(tp) >> TCP_YEAH_EPSILON);

					tcp_snd_cwnd_set(tp, tcp_snd_cwnd(tp) - reduction);

					tcp_snd_cwnd_set(tp, max(tcp_snd_cwnd(tp),
								 yeah->reno_count));

					tp->snd_ssthresh = tcp_snd_cwnd(tp);
				}

				if (yeah->reno_count <= 2)
					yeah->reno_count = max(tcp_snd_cwnd(tp)>>1, 2U);
				else
					yeah->reno_count++;

				yeah->doing_reno_now = min(yeah->doing_reno_now + 1,
							   0xffffffU);
			} else {
				yeah->fast_count++;

				if (yeah->fast_count > TCP_YEAH_ZETA) {
					yeah->reno_count = 2;
					yeah->fast_count = 0;
				}

				yeah->doing_reno_now = 0;
			}

			yeah->lastQ = queue;
		}

		 
		yeah->vegas.beg_snd_una  = yeah->vegas.beg_snd_nxt;
		yeah->vegas.beg_snd_nxt  = tp->snd_nxt;
		yeah->vegas.beg_snd_cwnd = tcp_snd_cwnd(tp);

		 
		yeah->vegas.cntRTT = 0;
		yeah->vegas.minRTT = 0x7fffffff;
	}
}

static u32 tcp_yeah_ssthresh(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct yeah *yeah = inet_csk_ca(sk);
	u32 reduction;

	if (yeah->doing_reno_now < TCP_YEAH_RHO) {
		reduction = yeah->lastQ;

		reduction = min(reduction, max(tcp_snd_cwnd(tp)>>1, 2U));

		reduction = max(reduction, tcp_snd_cwnd(tp) >> TCP_YEAH_DELTA);
	} else
		reduction = max(tcp_snd_cwnd(tp)>>1, 2U);

	yeah->fast_count = 0;
	yeah->reno_count = max(yeah->reno_count>>1, 2U);

	return max_t(int, tcp_snd_cwnd(tp) - reduction, 2);
}

static struct tcp_congestion_ops tcp_yeah __read_mostly = {
	.init		= tcp_yeah_init,
	.ssthresh	= tcp_yeah_ssthresh,
	.undo_cwnd      = tcp_reno_undo_cwnd,
	.cong_avoid	= tcp_yeah_cong_avoid,
	.set_state	= tcp_vegas_state,
	.cwnd_event	= tcp_vegas_cwnd_event,
	.get_info	= tcp_vegas_get_info,
	.pkts_acked	= tcp_vegas_pkts_acked,

	.owner		= THIS_MODULE,
	.name		= "yeah",
};

static int __init tcp_yeah_register(void)
{
	BUILD_BUG_ON(sizeof(struct yeah) > ICSK_CA_PRIV_SIZE);
	tcp_register_congestion_control(&tcp_yeah);
	return 0;
}

static void __exit tcp_yeah_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_yeah);
}

module_init(tcp_yeah_register);
module_exit(tcp_yeah_unregister);

MODULE_AUTHOR("Angelo P. Castellani");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("YeAH TCP");

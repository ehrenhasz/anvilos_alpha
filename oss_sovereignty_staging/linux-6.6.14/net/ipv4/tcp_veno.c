
 

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/inet_diag.h>

#include <net/tcp.h>

 
#define V_PARAM_SHIFT 1
static const int beta = 3 << V_PARAM_SHIFT;

 
struct veno {
	u8 doing_veno_now;	 
	u16 cntrtt;		 
	u32 minrtt;		 
	u32 basertt;		 
	u32 inc;		 
	u32 diff;		 
};

 
static inline void veno_enable(struct sock *sk)
{
	struct veno *veno = inet_csk_ca(sk);

	 
	veno->doing_veno_now = 1;

	veno->minrtt = 0x7fffffff;
}

static inline void veno_disable(struct sock *sk)
{
	struct veno *veno = inet_csk_ca(sk);

	 
	veno->doing_veno_now = 0;
}

static void tcp_veno_init(struct sock *sk)
{
	struct veno *veno = inet_csk_ca(sk);

	veno->basertt = 0x7fffffff;
	veno->inc = 1;
	veno_enable(sk);
}

 
static void tcp_veno_pkts_acked(struct sock *sk,
				const struct ack_sample *sample)
{
	struct veno *veno = inet_csk_ca(sk);
	u32 vrtt;

	if (sample->rtt_us < 0)
		return;

	 
	vrtt = sample->rtt_us + 1;

	 
	if (vrtt < veno->basertt)
		veno->basertt = vrtt;

	 
	veno->minrtt = min(veno->minrtt, vrtt);
	veno->cntrtt++;
}

static void tcp_veno_state(struct sock *sk, u8 ca_state)
{
	if (ca_state == TCP_CA_Open)
		veno_enable(sk);
	else
		veno_disable(sk);
}

 
static void tcp_veno_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	if (event == CA_EVENT_CWND_RESTART || event == CA_EVENT_TX_START)
		tcp_veno_init(sk);
}

static void tcp_veno_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct veno *veno = inet_csk_ca(sk);

	if (!veno->doing_veno_now) {
		tcp_reno_cong_avoid(sk, ack, acked);
		return;
	}

	 
	if (!tcp_is_cwnd_limited(sk))
		return;

	 
	if (veno->cntrtt <= 2) {
		 
		tcp_reno_cong_avoid(sk, ack, acked);
	} else {
		u64 target_cwnd;
		u32 rtt;

		 

		rtt = veno->minrtt;

		target_cwnd = (u64)tcp_snd_cwnd(tp) * veno->basertt;
		target_cwnd <<= V_PARAM_SHIFT;
		do_div(target_cwnd, rtt);

		veno->diff = (tcp_snd_cwnd(tp) << V_PARAM_SHIFT) - target_cwnd;

		if (tcp_in_slow_start(tp)) {
			 
			acked = tcp_slow_start(tp, acked);
			if (!acked)
				goto done;
		}

		 
		if (veno->diff < beta) {
			 
			tcp_cong_avoid_ai(tp, tcp_snd_cwnd(tp), acked);
		} else {
			 
			if (tp->snd_cwnd_cnt >= tcp_snd_cwnd(tp)) {
				if (veno->inc &&
				    tcp_snd_cwnd(tp) < tp->snd_cwnd_clamp) {
					tcp_snd_cwnd_set(tp, tcp_snd_cwnd(tp) + 1);
					veno->inc = 0;
				} else
					veno->inc = 1;
				tp->snd_cwnd_cnt = 0;
			} else
				tp->snd_cwnd_cnt += acked;
		}
done:
		if (tcp_snd_cwnd(tp) < 2)
			tcp_snd_cwnd_set(tp, 2);
		else if (tcp_snd_cwnd(tp) > tp->snd_cwnd_clamp)
			tcp_snd_cwnd_set(tp, tp->snd_cwnd_clamp);
	}
	 
	 
	veno->minrtt = 0x7fffffff;
}

 
static u32 tcp_veno_ssthresh(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct veno *veno = inet_csk_ca(sk);

	if (veno->diff < beta)
		 
		return max(tcp_snd_cwnd(tp) * 4 / 5, 2U);
	else
		 
		return max(tcp_snd_cwnd(tp) >> 1U, 2U);
}

static struct tcp_congestion_ops tcp_veno __read_mostly = {
	.init		= tcp_veno_init,
	.ssthresh	= tcp_veno_ssthresh,
	.undo_cwnd	= tcp_reno_undo_cwnd,
	.cong_avoid	= tcp_veno_cong_avoid,
	.pkts_acked	= tcp_veno_pkts_acked,
	.set_state	= tcp_veno_state,
	.cwnd_event	= tcp_veno_cwnd_event,

	.owner		= THIS_MODULE,
	.name		= "veno",
};

static int __init tcp_veno_register(void)
{
	BUILD_BUG_ON(sizeof(struct veno) > ICSK_CA_PRIV_SIZE);
	tcp_register_congestion_control(&tcp_veno);
	return 0;
}

static void __exit tcp_veno_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_veno);
}

module_init(tcp_veno_register);
module_exit(tcp_veno_unregister);

MODULE_AUTHOR("Bin Zhou, Cheng Peng Fu");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP Veno");

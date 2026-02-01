
 

#include <linux/module.h>
#include <linux/math64.h>
#include <net/tcp.h>
#include <linux/inet_diag.h>

 

static int nv_pad __read_mostly = 10;
static int nv_pad_buffer __read_mostly = 2;
static int nv_reset_period __read_mostly = 5;  
static int nv_min_cwnd __read_mostly = 2;
static int nv_cong_dec_mult __read_mostly = 30 * 128 / 100;  
static int nv_ssthresh_factor __read_mostly = 8;  
static int nv_rtt_factor __read_mostly = 128;  
static int nv_loss_dec_factor __read_mostly = 819;  
static int nv_cwnd_growth_rate_neg __read_mostly = 8;
static int nv_cwnd_growth_rate_pos __read_mostly;  
static int nv_dec_eval_min_calls __read_mostly = 60;
static int nv_inc_eval_min_calls __read_mostly = 20;
static int nv_ssthresh_eval_min_calls __read_mostly = 30;
static int nv_stop_rtt_cnt __read_mostly = 10;
static int nv_rtt_min_cnt __read_mostly = 2;

module_param(nv_pad, int, 0644);
MODULE_PARM_DESC(nv_pad, "max queued packets allowed in network");
module_param(nv_reset_period, int, 0644);
MODULE_PARM_DESC(nv_reset_period, "nv_min_rtt reset period (secs)");
module_param(nv_min_cwnd, int, 0644);
MODULE_PARM_DESC(nv_min_cwnd, "NV will not decrease cwnd below this value"
		 " without losses");

 
struct tcpnv {
	unsigned long nv_min_rtt_reset_jiffies;   
	s8  cwnd_growth_factor;	 
	u8  available8;
	u16 available16;
	u8  nv_allow_cwnd_growth:1,  
		nv_reset:1,	     
		nv_catchup:1;	     
	u8  nv_eval_call_cnt;	 
	u8  nv_min_cwnd;	 
	u8  nv_rtt_cnt;		 ;
	u32 nv_last_rtt;	 
	u32 nv_min_rtt;		 
	u32 nv_min_rtt_new;	 
	u32 nv_base_rtt;         
	u32 nv_lower_bound_rtt;  
	u32 nv_rtt_max_rate;	 
	u32 nv_rtt_start_seq;	 
	u32 nv_last_snd_una;	 
	u32 nv_no_cong_cnt;	 
};

#define NV_INIT_RTT	  U32_MAX
#define NV_MIN_CWND	  4
#define NV_MIN_CWND_GROW  2
#define NV_TSO_CWND_BOUND 80

static inline void tcpnv_reset(struct tcpnv *ca, struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);

	ca->nv_reset = 0;
	ca->nv_no_cong_cnt = 0;
	ca->nv_rtt_cnt = 0;
	ca->nv_last_rtt = 0;
	ca->nv_rtt_max_rate = 0;
	ca->nv_rtt_start_seq = tp->snd_una;
	ca->nv_eval_call_cnt = 0;
	ca->nv_last_snd_una = tp->snd_una;
}

static void tcpnv_init(struct sock *sk)
{
	struct tcpnv *ca = inet_csk_ca(sk);
	int base_rtt;

	tcpnv_reset(ca, sk);

	 
	base_rtt = tcp_call_bpf(sk, BPF_SOCK_OPS_BASE_RTT, 0, NULL);
	if (base_rtt > 0) {
		ca->nv_base_rtt = base_rtt;
		ca->nv_lower_bound_rtt = (base_rtt * 205) >> 8;  
	} else {
		ca->nv_base_rtt = 0;
		ca->nv_lower_bound_rtt = 0;
	}

	ca->nv_allow_cwnd_growth = 1;
	ca->nv_min_rtt_reset_jiffies = jiffies + 2 * HZ;
	ca->nv_min_rtt = NV_INIT_RTT;
	ca->nv_min_rtt_new = NV_INIT_RTT;
	ca->nv_min_cwnd = NV_MIN_CWND;
	ca->nv_catchup = 0;
	ca->cwnd_growth_factor = 0;
}

 
inline u32 nv_get_bounded_rtt(struct tcpnv *ca, u32 val)
{
	if (ca->nv_lower_bound_rtt > 0 && val < ca->nv_lower_bound_rtt)
		return ca->nv_lower_bound_rtt;
	else if (ca->nv_base_rtt > 0 && val > ca->nv_base_rtt)
		return ca->nv_base_rtt;
	else
		return val;
}

static void tcpnv_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcpnv *ca = inet_csk_ca(sk);
	u32 cnt;

	if (!tcp_is_cwnd_limited(sk))
		return;

	 
	if (!ca->nv_allow_cwnd_growth)
		return;

	if (tcp_in_slow_start(tp)) {
		acked = tcp_slow_start(tp, acked);
		if (!acked)
			return;
	}

	if (ca->cwnd_growth_factor < 0) {
		cnt = tcp_snd_cwnd(tp) << -ca->cwnd_growth_factor;
		tcp_cong_avoid_ai(tp, cnt, acked);
	} else {
		cnt = max(4U, tcp_snd_cwnd(tp) >> ca->cwnd_growth_factor);
		tcp_cong_avoid_ai(tp, cnt, acked);
	}
}

static u32 tcpnv_recalc_ssthresh(struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);

	return max((tcp_snd_cwnd(tp) * nv_loss_dec_factor) >> 10, 2U);
}

static void tcpnv_state(struct sock *sk, u8 new_state)
{
	struct tcpnv *ca = inet_csk_ca(sk);

	if (new_state == TCP_CA_Open && ca->nv_reset) {
		tcpnv_reset(ca, sk);
	} else if (new_state == TCP_CA_Loss || new_state == TCP_CA_CWR ||
		new_state == TCP_CA_Recovery) {
		ca->nv_reset = 1;
		ca->nv_allow_cwnd_growth = 0;
		if (new_state == TCP_CA_Loss) {
			 
			if (ca->cwnd_growth_factor > 0)
				ca->cwnd_growth_factor = 0;
			 
			if (nv_cwnd_growth_rate_neg > 0 &&
			    ca->cwnd_growth_factor > -8)
				ca->cwnd_growth_factor--;
		}
	}
}

 
static void tcpnv_acked(struct sock *sk, const struct ack_sample *sample)
{
	const struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);
	struct tcpnv *ca = inet_csk_ca(sk);
	unsigned long now = jiffies;
	u64 rate64;
	u32 rate, max_win, cwnd_by_slope;
	u32 avg_rtt;
	u32 bytes_acked = 0;

	 
	if (sample->rtt_us < 0)
		return;

	 
	if (icsk->icsk_ca_state != TCP_CA_Open &&
	    icsk->icsk_ca_state != TCP_CA_Disorder)
		return;

	 
	if (ca->nv_catchup && tcp_snd_cwnd(tp) >= nv_min_cwnd) {
		ca->nv_catchup = 0;
		ca->nv_allow_cwnd_growth = 0;
	}

	bytes_acked = tp->snd_una - ca->nv_last_snd_una;
	ca->nv_last_snd_una = tp->snd_una;

	if (sample->in_flight == 0)
		return;

	 
	if (nv_rtt_factor > 0) {
		if (ca->nv_last_rtt > 0) {
			avg_rtt = (((u64)sample->rtt_us) * nv_rtt_factor +
				   ((u64)ca->nv_last_rtt)
				   * (256 - nv_rtt_factor)) >> 8;
		} else {
			avg_rtt = sample->rtt_us;
			ca->nv_min_rtt = avg_rtt << 1;
		}
		ca->nv_last_rtt = avg_rtt;
	} else {
		avg_rtt = sample->rtt_us;
	}

	 
	rate64 = ((u64)sample->in_flight) * 80000;
	do_div(rate64, avg_rtt ?: 1);
	rate = (u32)rate64;

	 
	if (ca->nv_rtt_max_rate < rate)
		ca->nv_rtt_max_rate = rate;

	 
	if (ca->nv_eval_call_cnt < 255)
		ca->nv_eval_call_cnt++;

	 
	avg_rtt = nv_get_bounded_rtt(ca, avg_rtt);

	 
	if (avg_rtt < ca->nv_min_rtt)
		ca->nv_min_rtt = avg_rtt;

	 
	if (avg_rtt < ca->nv_min_rtt_new)
		ca->nv_min_rtt_new = avg_rtt;

	 
	if (time_after_eq(now, ca->nv_min_rtt_reset_jiffies)) {
		unsigned char rand;

		ca->nv_min_rtt = ca->nv_min_rtt_new;
		ca->nv_min_rtt_new = NV_INIT_RTT;
		get_random_bytes(&rand, 1);
		ca->nv_min_rtt_reset_jiffies =
			now + ((nv_reset_period * (384 + rand) * HZ) >> 9);
		 
		ca->nv_min_cwnd = max(ca->nv_min_cwnd / 2, NV_MIN_CWND);
	}

	 
	if (before(ca->nv_rtt_start_seq, tp->snd_una)) {
		ca->nv_rtt_start_seq = tp->snd_nxt;
		if (ca->nv_rtt_cnt < 0xff)
			 
			ca->nv_rtt_cnt++;

		 
		if (ca->nv_eval_call_cnt == 1 &&
		    bytes_acked >= (ca->nv_min_cwnd - 1) * tp->mss_cache &&
		    ca->nv_min_cwnd < (NV_TSO_CWND_BOUND + 1)) {
			ca->nv_min_cwnd = min(ca->nv_min_cwnd
					      + NV_MIN_CWND_GROW,
					      NV_TSO_CWND_BOUND + 1);
			ca->nv_rtt_start_seq = tp->snd_nxt +
				ca->nv_min_cwnd * tp->mss_cache;
			ca->nv_eval_call_cnt = 0;
			ca->nv_allow_cwnd_growth = 1;
			return;
		}

		 
		cwnd_by_slope = (u32)
			div64_u64(((u64)ca->nv_rtt_max_rate) * ca->nv_min_rtt,
				  80000ULL * tp->mss_cache);
		max_win = cwnd_by_slope + nv_pad;

		 
		if (tcp_snd_cwnd(tp) > max_win) {
			 
			if (ca->nv_rtt_cnt < nv_rtt_min_cnt) {
				return;
			} else if (tp->snd_ssthresh == TCP_INFINITE_SSTHRESH) {
				if (ca->nv_eval_call_cnt <
				    nv_ssthresh_eval_min_calls)
					return;
				 
			} else if (ca->nv_eval_call_cnt <
				   nv_dec_eval_min_calls) {
				if (ca->nv_allow_cwnd_growth &&
				    ca->nv_rtt_cnt > nv_stop_rtt_cnt)
					ca->nv_allow_cwnd_growth = 0;
				return;
			}

			 
			ca->nv_allow_cwnd_growth = 0;
			tp->snd_ssthresh =
				(nv_ssthresh_factor * max_win) >> 3;
			if (tcp_snd_cwnd(tp) - max_win > 2) {
				 
				int dec;

				dec = max(2U, ((tcp_snd_cwnd(tp) - max_win) *
					       nv_cong_dec_mult) >> 7);
				tcp_snd_cwnd_set(tp, tcp_snd_cwnd(tp) - dec);
			} else if (nv_cong_dec_mult > 0) {
				tcp_snd_cwnd_set(tp, max_win);
			}
			if (ca->cwnd_growth_factor > 0)
				ca->cwnd_growth_factor = 0;
			ca->nv_no_cong_cnt = 0;
		} else if (tcp_snd_cwnd(tp) <= max_win - nv_pad_buffer) {
			 
			if (ca->nv_eval_call_cnt < nv_inc_eval_min_calls)
				return;

			ca->nv_allow_cwnd_growth = 1;
			ca->nv_no_cong_cnt++;
			if (ca->cwnd_growth_factor < 0 &&
			    nv_cwnd_growth_rate_neg > 0 &&
			    ca->nv_no_cong_cnt > nv_cwnd_growth_rate_neg) {
				ca->cwnd_growth_factor++;
				ca->nv_no_cong_cnt = 0;
			} else if (ca->cwnd_growth_factor >= 0 &&
				   nv_cwnd_growth_rate_pos > 0 &&
				   ca->nv_no_cong_cnt >
				   nv_cwnd_growth_rate_pos) {
				ca->cwnd_growth_factor++;
				ca->nv_no_cong_cnt = 0;
			}
		} else {
			 
			return;
		}

		 
		ca->nv_eval_call_cnt = 0;
		ca->nv_rtt_cnt = 0;
		ca->nv_rtt_max_rate = 0;

		 
		if (tcp_snd_cwnd(tp) < nv_min_cwnd)
			tcp_snd_cwnd_set(tp, nv_min_cwnd);
	}
}

 
static size_t tcpnv_get_info(struct sock *sk, u32 ext, int *attr,
			     union tcp_cc_info *info)
{
	const struct tcpnv *ca = inet_csk_ca(sk);

	if (ext & (1 << (INET_DIAG_VEGASINFO - 1))) {
		info->vegas.tcpv_enabled = 1;
		info->vegas.tcpv_rttcnt = ca->nv_rtt_cnt;
		info->vegas.tcpv_rtt = ca->nv_last_rtt;
		info->vegas.tcpv_minrtt = ca->nv_min_rtt;

		*attr = INET_DIAG_VEGASINFO;
		return sizeof(struct tcpvegas_info);
	}
	return 0;
}

static struct tcp_congestion_ops tcpnv __read_mostly = {
	.init		= tcpnv_init,
	.ssthresh	= tcpnv_recalc_ssthresh,
	.cong_avoid	= tcpnv_cong_avoid,
	.set_state	= tcpnv_state,
	.undo_cwnd	= tcp_reno_undo_cwnd,
	.pkts_acked     = tcpnv_acked,
	.get_info	= tcpnv_get_info,

	.owner		= THIS_MODULE,
	.name		= "nv",
};

static int __init tcpnv_register(void)
{
	BUILD_BUG_ON(sizeof(struct tcpnv) > ICSK_CA_PRIV_SIZE);

	return tcp_register_congestion_control(&tcpnv);
}

static void __exit tcpnv_unregister(void)
{
	tcp_unregister_congestion_control(&tcpnv);
}

module_init(tcpnv_register);
module_exit(tcpnv_unregister);

MODULE_AUTHOR("Lawrence Brakmo");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TCP NV");
MODULE_VERSION("1.0");



 

#include <linux/bpf.h>
#include <linux/stddef.h>
#include <linux/tcp.h>
#include "bpf_tcp_helpers.h"

char _license[] SEC("license") = "GPL";

#define clamp(val, lo, hi) min((typeof(val))max(val, lo), hi)

#define BICTCP_BETA_SCALE    1024	 
#define	BICTCP_HZ		10	 

 
#define HYSTART_ACK_TRAIN	0x1
#define HYSTART_DELAY		0x2

 
#define HYSTART_MIN_SAMPLES	8
#define HYSTART_DELAY_MIN	(4000U)	 
#define HYSTART_DELAY_MAX	(16000U)	 
#define HYSTART_DELAY_THRESH(x)	clamp(x, HYSTART_DELAY_MIN, HYSTART_DELAY_MAX)

static int fast_convergence = 1;
static const int beta = 717;	 
static int initial_ssthresh;
static const int bic_scale = 41;
static int tcp_friendliness = 1;

static int hystart = 1;
static int hystart_detect = HYSTART_ACK_TRAIN | HYSTART_DELAY;
static int hystart_low_window = 16;
static int hystart_ack_delta_us = 2000;

static const __u32 cube_rtt_scale = (bic_scale * 10);	 
static const __u32 beta_scale = 8*(BICTCP_BETA_SCALE+beta) / 3
				/ (BICTCP_BETA_SCALE - beta);
 

 
static const __u64 cube_factor = (__u64)(1ull << (10+3*BICTCP_HZ))
				/ (bic_scale * 10);

 
struct bictcp {
	__u32	cnt;		 
	__u32	last_max_cwnd;	 
	__u32	last_cwnd;	 
	__u32	last_time;	 
	__u32	bic_origin_point; 
	__u32	bic_K;		 
	__u32	delay_min;	 
	__u32	epoch_start;	 
	__u32	ack_cnt;	 
	__u32	tcp_cwnd;	 
	__u16	unused;
	__u8	sample_cnt;	 
	__u8	found;		 
	__u32	round_start;	 
	__u32	end_seq;	 
	__u32	last_ack;	 
	__u32	curr_rtt;	 
};

static inline void bictcp_reset(struct bictcp *ca)
{
	ca->cnt = 0;
	ca->last_max_cwnd = 0;
	ca->last_cwnd = 0;
	ca->last_time = 0;
	ca->bic_origin_point = 0;
	ca->bic_K = 0;
	ca->delay_min = 0;
	ca->epoch_start = 0;
	ca->ack_cnt = 0;
	ca->tcp_cwnd = 0;
	ca->found = 0;
}

extern unsigned long CONFIG_HZ __kconfig;
#define HZ CONFIG_HZ
#define USEC_PER_MSEC	1000UL
#define USEC_PER_SEC	1000000UL
#define USEC_PER_JIFFY	(USEC_PER_SEC / HZ)

static __always_inline __u64 div64_u64(__u64 dividend, __u64 divisor)
{
	return dividend / divisor;
}

#define div64_ul div64_u64

#define BITS_PER_U64 (sizeof(__u64) * 8)
static __always_inline int fls64(__u64 x)
{
	int num = BITS_PER_U64 - 1;

	if (x == 0)
		return 0;

	if (!(x & (~0ull << (BITS_PER_U64-32)))) {
		num -= 32;
		x <<= 32;
	}
	if (!(x & (~0ull << (BITS_PER_U64-16)))) {
		num -= 16;
		x <<= 16;
	}
	if (!(x & (~0ull << (BITS_PER_U64-8)))) {
		num -= 8;
		x <<= 8;
	}
	if (!(x & (~0ull << (BITS_PER_U64-4)))) {
		num -= 4;
		x <<= 4;
	}
	if (!(x & (~0ull << (BITS_PER_U64-2)))) {
		num -= 2;
		x <<= 2;
	}
	if (!(x & (~0ull << (BITS_PER_U64-1))))
		num -= 1;

	return num + 1;
}

static __always_inline __u32 bictcp_clock_us(const struct sock *sk)
{
	return tcp_sk(sk)->tcp_mstamp;
}

static __always_inline void bictcp_hystart_reset(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	ca->round_start = ca->last_ack = bictcp_clock_us(sk);
	ca->end_seq = tp->snd_nxt;
	ca->curr_rtt = ~0U;
	ca->sample_cnt = 0;
}

 
SEC("struct_ops/bpf_cubic_init")
void BPF_PROG(bpf_cubic_init, struct sock *sk)
{
	struct bictcp *ca = inet_csk_ca(sk);

	bictcp_reset(ca);

	if (hystart)
		bictcp_hystart_reset(sk);

	if (!hystart && initial_ssthresh)
		tcp_sk(sk)->snd_ssthresh = initial_ssthresh;
}

 
SEC("struct_ops/bpf_cubic_cwnd_event")
void BPF_PROG(bpf_cubic_cwnd_event, struct sock *sk, enum tcp_ca_event event)
{
	if (event == CA_EVENT_TX_START) {
		struct bictcp *ca = inet_csk_ca(sk);
		__u32 now = tcp_jiffies32;
		__s32 delta;

		delta = now - tcp_sk(sk)->lsndtime;

		 
		if (ca->epoch_start && delta > 0) {
			ca->epoch_start += delta;
			if (after(ca->epoch_start, now))
				ca->epoch_start = now;
		}
		return;
	}
}

 
static const __u8 v[] = {
	     0,   54,   54,   54,  118,  118,  118,  118,
	   123,  129,  134,  138,  143,  147,  151,  156,
	   157,  161,  164,  168,  170,  173,  176,  179,
	   181,  185,  187,  190,  192,  194,  197,  199,
	   200,  202,  204,  206,  209,  211,  213,  215,
	   217,  219,  221,  222,  224,  225,  227,  229,
	   231,  232,  234,  236,  237,  239,  240,  242,
	   244,  245,  246,  248,  250,  251,  252,  254,
};

 
static __always_inline __u32 cubic_root(__u64 a)
{
	__u32 x, b, shift;

	if (a < 64) {
		 
		return ((__u32)v[(__u32)a] + 35) >> 6;
	}

	b = fls64(a);
	b = ((b * 84) >> 8) - 1;
	shift = (a >> (b * 3));

	 
	if (shift >= 64)
		return 0;

	x = ((__u32)(((__u32)v[shift] + 10) << b)) >> 6;

	 
	x = (2 * x + (__u32)div64_u64(a, (__u64)x * (__u64)(x - 1)));
	x = ((x * 341) >> 10);
	return x;
}

 
static __always_inline void bictcp_update(struct bictcp *ca, __u32 cwnd,
					  __u32 acked)
{
	__u32 delta, bic_target, max_cnt;
	__u64 offs, t;

	ca->ack_cnt += acked;	 

	if (ca->last_cwnd == cwnd &&
	    (__s32)(tcp_jiffies32 - ca->last_time) <= HZ / 32)
		return;

	 
	if (ca->epoch_start && tcp_jiffies32 == ca->last_time)
		goto tcp_friendliness;

	ca->last_cwnd = cwnd;
	ca->last_time = tcp_jiffies32;

	if (ca->epoch_start == 0) {
		ca->epoch_start = tcp_jiffies32;	 
		ca->ack_cnt = acked;			 
		ca->tcp_cwnd = cwnd;			 

		if (ca->last_max_cwnd <= cwnd) {
			ca->bic_K = 0;
			ca->bic_origin_point = cwnd;
		} else {
			 
			ca->bic_K = cubic_root(cube_factor
					       * (ca->last_max_cwnd - cwnd));
			ca->bic_origin_point = ca->last_max_cwnd;
		}
	}

	 
	 

	t = (__s32)(tcp_jiffies32 - ca->epoch_start) * USEC_PER_JIFFY;
	t += ca->delay_min;
	 
	t <<= BICTCP_HZ;
	t /= USEC_PER_SEC;

	if (t < ca->bic_K)		 
		offs = ca->bic_K - t;
	else
		offs = t - ca->bic_K;

	 
	delta = (cube_rtt_scale * offs * offs * offs) >> (10+3*BICTCP_HZ);
	if (t < ca->bic_K)                             
		bic_target = ca->bic_origin_point - delta;
	else                                           
		bic_target = ca->bic_origin_point + delta;

	 
	if (bic_target > cwnd) {
		ca->cnt = cwnd / (bic_target - cwnd);
	} else {
		ca->cnt = 100 * cwnd;               
	}

	 
	if (ca->last_max_cwnd == 0 && ca->cnt > 20)
		ca->cnt = 20;	 

tcp_friendliness:
	 
	if (tcp_friendliness) {
		__u32 scale = beta_scale;
		__u32 n;

		 
		delta = (cwnd * scale) >> 3;
		if (ca->ack_cnt > delta && delta) {
			n = ca->ack_cnt / delta;
			ca->ack_cnt -= n * delta;
			ca->tcp_cwnd += n;
		}

		if (ca->tcp_cwnd > cwnd) {	 
			delta = ca->tcp_cwnd - cwnd;
			max_cnt = cwnd / delta;
			if (ca->cnt > max_cnt)
				ca->cnt = max_cnt;
		}
	}

	 
	ca->cnt = max(ca->cnt, 2U);
}

 
void BPF_STRUCT_OPS(bpf_cubic_cong_avoid, struct sock *sk, __u32 ack, __u32 acked)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	if (!tcp_is_cwnd_limited(sk))
		return;

	if (tcp_in_slow_start(tp)) {
		if (hystart && after(ack, ca->end_seq))
			bictcp_hystart_reset(sk);
		acked = tcp_slow_start(tp, acked);
		if (!acked)
			return;
	}
	bictcp_update(ca, tp->snd_cwnd, acked);
	tcp_cong_avoid_ai(tp, ca->cnt, acked);
}

__u32 BPF_STRUCT_OPS(bpf_cubic_recalc_ssthresh, struct sock *sk)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);

	ca->epoch_start = 0;	 

	 
	if (tp->snd_cwnd < ca->last_max_cwnd && fast_convergence)
		ca->last_max_cwnd = (tp->snd_cwnd * (BICTCP_BETA_SCALE + beta))
			/ (2 * BICTCP_BETA_SCALE);
	else
		ca->last_max_cwnd = tp->snd_cwnd;

	return max((tp->snd_cwnd * beta) / BICTCP_BETA_SCALE, 2U);
}

void BPF_STRUCT_OPS(bpf_cubic_state, struct sock *sk, __u8 new_state)
{
	if (new_state == TCP_CA_Loss) {
		bictcp_reset(inet_csk_ca(sk));
		bictcp_hystart_reset(sk);
	}
}

#define GSO_MAX_SIZE		65536

 
static __always_inline __u32 hystart_ack_delay(struct sock *sk)
{
	unsigned long rate;

	rate = sk->sk_pacing_rate;
	if (!rate)
		return 0;
	return min((__u64)USEC_PER_MSEC,
		   div64_ul((__u64)GSO_MAX_SIZE * 4 * USEC_PER_SEC, rate));
}

static __always_inline void hystart_update(struct sock *sk, __u32 delay)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);
	__u32 threshold;

	if (hystart_detect & HYSTART_ACK_TRAIN) {
		__u32 now = bictcp_clock_us(sk);

		 
		if ((__s32)(now - ca->last_ack) <= hystart_ack_delta_us) {
			ca->last_ack = now;

			threshold = ca->delay_min + hystart_ack_delay(sk);

			 
			if (sk->sk_pacing_status == SK_PACING_NONE)
				threshold >>= 1;

			if ((__s32)(now - ca->round_start) > threshold) {
				ca->found = 1;
				tp->snd_ssthresh = tp->snd_cwnd;
			}
		}
	}

	if (hystart_detect & HYSTART_DELAY) {
		 
		if (ca->curr_rtt > delay)
			ca->curr_rtt = delay;
		if (ca->sample_cnt < HYSTART_MIN_SAMPLES) {
			ca->sample_cnt++;
		} else {
			if (ca->curr_rtt > ca->delay_min +
			    HYSTART_DELAY_THRESH(ca->delay_min >> 3)) {
				ca->found = 1;
				tp->snd_ssthresh = tp->snd_cwnd;
			}
		}
	}
}

int bpf_cubic_acked_called = 0;

void BPF_STRUCT_OPS(bpf_cubic_acked, struct sock *sk,
		    const struct ack_sample *sample)
{
	const struct tcp_sock *tp = tcp_sk(sk);
	struct bictcp *ca = inet_csk_ca(sk);
	__u32 delay;

	bpf_cubic_acked_called = 1;
	 
	if (sample->rtt_us < 0)
		return;

	 
	if (ca->epoch_start && (__s32)(tcp_jiffies32 - ca->epoch_start) < HZ)
		return;

	delay = sample->rtt_us;
	if (delay == 0)
		delay = 1;

	 
	if (ca->delay_min == 0 || ca->delay_min > delay)
		ca->delay_min = delay;

	 
	if (!ca->found && tcp_in_slow_start(tp) && hystart &&
	    tp->snd_cwnd >= hystart_low_window)
		hystart_update(sk, delay);
}

extern __u32 tcp_reno_undo_cwnd(struct sock *sk) __ksym;

__u32 BPF_STRUCT_OPS(bpf_cubic_undo_cwnd, struct sock *sk)
{
	return tcp_reno_undo_cwnd(sk);
}

SEC(".struct_ops")
struct tcp_congestion_ops cubic = {
	.init		= (void *)bpf_cubic_init,
	.ssthresh	= (void *)bpf_cubic_recalc_ssthresh,
	.cong_avoid	= (void *)bpf_cubic_cong_avoid,
	.set_state	= (void *)bpf_cubic_state,
	.undo_cwnd	= (void *)bpf_cubic_undo_cwnd,
	.cwnd_event	= (void *)bpf_cubic_cwnd_event,
	.pkts_acked     = (void *)bpf_cubic_acked,
	.name		= "bpf_cubic",
};

 
 
#ifndef __TCP_VEGAS_H
#define __TCP_VEGAS_H 1

 
struct vegas {
	u32	beg_snd_nxt;	 
	u32	beg_snd_una;	 
	u32	beg_snd_cwnd;	 
	u8	doing_vegas_now; 
	u16	cntRTT;		 
	u32	minRTT;		 
	u32	baseRTT;	 
};

void tcp_vegas_init(struct sock *sk);
void tcp_vegas_state(struct sock *sk, u8 ca_state);
void tcp_vegas_pkts_acked(struct sock *sk, const struct ack_sample *sample);
void tcp_vegas_cwnd_event(struct sock *sk, enum tcp_ca_event event);
size_t tcp_vegas_get_info(struct sock *sk, u32 ext, int *attr,
			  union tcp_cc_info *info);

#endif	 

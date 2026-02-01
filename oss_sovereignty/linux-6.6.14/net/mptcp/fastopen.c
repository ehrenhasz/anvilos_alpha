
 

#include "protocol.h"

void mptcp_fastopen_subflow_synack_set_params(struct mptcp_subflow_context *subflow,
					      struct request_sock *req)
{
	struct sock *sk, *ssk;
	struct sk_buff *skb;
	struct tcp_sock *tp;

	 
	if (!subflow)
		return;

	ssk = subflow->tcp_sock;
	sk = subflow->conn;
	tp = tcp_sk(ssk);

	subflow->is_mptfo = 1;

	skb = skb_peek(&ssk->sk_receive_queue);
	if (WARN_ON_ONCE(!skb))
		return;

	 
	__skb_unlink(skb, &ssk->sk_receive_queue);
	skb_ext_reset(skb);
	skb_orphan(skb);

	 
	tp->copied_seq += skb->len;
	subflow->ssn_offset += skb->len;

	 
	MPTCP_SKB_CB(skb)->map_seq = -skb->len;
	MPTCP_SKB_CB(skb)->end_seq = 0;
	MPTCP_SKB_CB(skb)->offset = 0;
	MPTCP_SKB_CB(skb)->has_rxtstamp = TCP_SKB_CB(skb)->has_rxtstamp;

	mptcp_data_lock(sk);

	mptcp_set_owner_r(skb, sk);
	__skb_queue_tail(&sk->sk_receive_queue, skb);
	mptcp_sk(sk)->bytes_received += skb->len;

	sk->sk_data_ready(sk);

	mptcp_data_unlock(sk);
}

void mptcp_fastopen_gen_msk_ackseq(struct mptcp_sock *msk, struct mptcp_subflow_context *subflow,
				   const struct mptcp_options_received *mp_opt)
{
	struct sock *sk = (struct sock *)msk;
	struct sk_buff *skb;

	mptcp_data_lock(sk);
	skb = skb_peek_tail(&sk->sk_receive_queue);
	if (skb) {
		WARN_ON_ONCE(MPTCP_SKB_CB(skb)->end_seq);
		pr_debug("msk %p moving seq %llx -> %llx end_seq %llx -> %llx", sk,
			 MPTCP_SKB_CB(skb)->map_seq, MPTCP_SKB_CB(skb)->map_seq + msk->ack_seq,
			 MPTCP_SKB_CB(skb)->end_seq, MPTCP_SKB_CB(skb)->end_seq + msk->ack_seq);
		MPTCP_SKB_CB(skb)->map_seq += msk->ack_seq;
		MPTCP_SKB_CB(skb)->end_seq += msk->ack_seq;
	}

	pr_debug("msk=%p ack_seq=%llx", msk, msk->ack_seq);
	mptcp_data_unlock(sk);
}

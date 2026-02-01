
 

#include <linux/net.h>
#include "ar-internal.h"

#define RXRPC_RTO_MAX	((unsigned)(120 * HZ))
#define RXRPC_TIMEOUT_INIT ((unsigned)(1*HZ))	 
#define rxrpc_jiffies32 ((u32)jiffies)		 

static u32 rxrpc_rto_min_us(struct rxrpc_peer *peer)
{
	return 200;
}

static u32 __rxrpc_set_rto(const struct rxrpc_peer *peer)
{
	return usecs_to_jiffies((peer->srtt_us >> 3) + peer->rttvar_us);
}

static u32 rxrpc_bound_rto(u32 rto)
{
	return min(rto, RXRPC_RTO_MAX);
}

 
static void rxrpc_rtt_estimator(struct rxrpc_peer *peer, long sample_rtt_us)
{
	long m = sample_rtt_us;  
	u32 srtt = peer->srtt_us;

	 
	if (srtt != 0) {
		m -= (srtt >> 3);	 
		srtt += m;		 
		if (m < 0) {
			m = -m;		 
			m -= (peer->mdev_us >> 2);    
			 
			if (m > 0)
				m >>= 3;
		} else {
			m -= (peer->mdev_us >> 2);    
		}

		peer->mdev_us += m;		 
		if (peer->mdev_us > peer->mdev_max_us) {
			peer->mdev_max_us = peer->mdev_us;
			if (peer->mdev_max_us > peer->rttvar_us)
				peer->rttvar_us = peer->mdev_max_us;
		}
	} else {
		 
		srtt = m << 3;		 
		peer->mdev_us = m << 1;	 
		peer->rttvar_us = max(peer->mdev_us, rxrpc_rto_min_us(peer));
		peer->mdev_max_us = peer->rttvar_us;
	}

	peer->srtt_us = max(1U, srtt);
}

 
static void rxrpc_set_rto(struct rxrpc_peer *peer)
{
	u32 rto;

	 
	rto = __rxrpc_set_rto(peer);

	 

	 
	peer->rto_j = rxrpc_bound_rto(rto);
}

static void rxrpc_ack_update_rtt(struct rxrpc_peer *peer, long rtt_us)
{
	if (rtt_us < 0)
		return;

	
	rxrpc_rtt_estimator(peer, rtt_us);
	rxrpc_set_rto(peer);

	 
	peer->backoff = 0;
}

 
void rxrpc_peer_add_rtt(struct rxrpc_call *call, enum rxrpc_rtt_rx_trace why,
			int rtt_slot,
			rxrpc_serial_t send_serial, rxrpc_serial_t resp_serial,
			ktime_t send_time, ktime_t resp_time)
{
	struct rxrpc_peer *peer = call->peer;
	s64 rtt_us;

	rtt_us = ktime_to_us(ktime_sub(resp_time, send_time));
	if (rtt_us < 0)
		return;

	spin_lock(&peer->rtt_input_lock);
	rxrpc_ack_update_rtt(peer, rtt_us);
	if (peer->rtt_count < 3)
		peer->rtt_count++;
	spin_unlock(&peer->rtt_input_lock);

	trace_rxrpc_rtt_rx(call, why, rtt_slot, send_serial, resp_serial,
			   peer->srtt_us >> 3, peer->rto_j);
}

 
unsigned long rxrpc_get_rto_backoff(struct rxrpc_peer *peer, bool retrans)
{
	u64 timo_j;
	u8 backoff = READ_ONCE(peer->backoff);

	timo_j = peer->rto_j;
	timo_j <<= backoff;
	if (retrans && timo_j * 2 <= RXRPC_RTO_MAX)
		WRITE_ONCE(peer->backoff, backoff + 1);

	if (timo_j < 1)
		timo_j = 1;

	return timo_j;
}

void rxrpc_peer_init_rtt(struct rxrpc_peer *peer)
{
	peer->rto_j	= RXRPC_TIMEOUT_INIT;
	peer->mdev_us	= jiffies_to_usecs(RXRPC_TIMEOUT_INIT);
	peer->backoff	= 0;
	
}


 

#include "hbm_kern.h"

SEC("cgroup_skb/egress")
int _hbm_out_cg(struct __sk_buff *skb)
{
	long long delta = 0, delta_send;
	unsigned long long curtime, sendtime;
	struct hbm_queue_stats *qsp = NULL;
	unsigned int queue_index = 0;
	bool congestion_flag = false;
	bool ecn_ce_flag = false;
	struct hbm_pkt_info pkti = {};
	struct hbm_vqueue *qdp;
	bool drop_flag = false;
	bool cwr_flag = false;
	int len = skb->len;
	int rv = ALLOW_PKT;

	qsp = bpf_map_lookup_elem(&queue_stats, &queue_index);

	
	if (qsp != NULL && !qsp->loopback && (skb->ifindex == 1))
		return ALLOW_PKT;

	hbm_get_pkt_info(skb, &pkti);

	
	
	

	qdp = bpf_get_local_storage(&queue_state, 0);
	if (!qdp)
		return ALLOW_PKT;
	if (qdp->lasttime == 0)
		hbm_init_edt_vqueue(qdp, 1024);

	curtime = bpf_ktime_get_ns();

	
	bpf_spin_lock(&qdp->lock);
	delta = qdp->lasttime - curtime;
	
	if (delta < -BURST_SIZE_NS) {
		
		qdp->lasttime = curtime - BURST_SIZE_NS;
		delta = -BURST_SIZE_NS;
	}
	sendtime = qdp->lasttime;
	delta_send = BYTES_TO_NS(len, qdp->rate);
	__sync_add_and_fetch(&(qdp->lasttime), delta_send);
	bpf_spin_unlock(&qdp->lock);
	

	
	skb->tstamp = sendtime;

	
	if (qsp != NULL && (qsp->rate * 128) != qdp->rate)
		qdp->rate = qsp->rate * 128;

	
	
	if (delta > DROP_THRESH_NS || (delta > LARGE_PKT_DROP_THRESH_NS &&
				       len > LARGE_PKT_THRESH)) {
		drop_flag = true;
		if (pkti.is_tcp && pkti.ecn == 0)
			cwr_flag = true;
	} else if (delta > MARK_THRESH_NS) {
		if (pkti.is_tcp)
			congestion_flag = true;
		else
			drop_flag = true;
	}

	if (congestion_flag) {
		if (bpf_skb_ecn_set_ce(skb)) {
			ecn_ce_flag = true;
		} else {
			if (pkti.is_tcp) {
				unsigned int rand = bpf_get_prandom_u32();

				if (delta >= MARK_THRESH_NS +
				    (rand % MARK_REGION_SIZE_NS)) {
					
					cwr_flag = true;
				}
			} else if (len > LARGE_PKT_THRESH) {
				
				drop_flag = true;
				congestion_flag = false;
			}
		}
	}

	if (pkti.is_tcp && drop_flag && pkti.packets_out <= 1) {
		drop_flag = false;
		cwr_flag = true;
		congestion_flag = false;
	}

	if (qsp != NULL && qsp->no_cn)
			cwr_flag = false;

	hbm_update_stats(qsp, len, curtime, congestion_flag, drop_flag,
			 cwr_flag, ecn_ce_flag, &pkti, (int) delta);

	if (drop_flag) {
		__sync_add_and_fetch(&(qdp->lasttime), -delta_send);
		rv = DROP_PKT;
	}

	if (cwr_flag)
		rv |= CWR;
	return rv;
}
char _license[] SEC("license") = "GPL";

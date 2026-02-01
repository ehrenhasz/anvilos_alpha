
 

#include "hbm_kern.h"

SEC("cgroup_skb/egress")
int _hbm_out_cg(struct __sk_buff *skb)
{
	struct hbm_pkt_info pkti;
	int len = skb->len;
	unsigned int queue_index = 0;
	unsigned long long curtime;
	int credit;
	signed long long delta = 0, new_credit;
	int max_credit = MAX_CREDIT;
	bool congestion_flag = false;
	bool drop_flag = false;
	bool cwr_flag = false;
	bool ecn_ce_flag = false;
	struct hbm_vqueue *qdp;
	struct hbm_queue_stats *qsp = NULL;
	int rv = ALLOW_PKT;

	qsp = bpf_map_lookup_elem(&queue_stats, &queue_index);
	if (qsp != NULL && !qsp->loopback && (skb->ifindex == 1))
		return ALLOW_PKT;

	hbm_get_pkt_info(skb, &pkti);

	
	
	

	qdp = bpf_get_local_storage(&queue_state, 0);
	if (!qdp)
		return ALLOW_PKT;
	else if (qdp->lasttime == 0)
		hbm_init_vqueue(qdp, 1024);

	curtime = bpf_ktime_get_ns();

	
	bpf_spin_lock(&qdp->lock);
	credit = qdp->credit;
	delta = curtime - qdp->lasttime;
	 
	if (delta > 0) {
		qdp->lasttime = curtime;
		new_credit = credit + CREDIT_PER_NS(delta, qdp->rate);
		if (new_credit > MAX_CREDIT)
			credit = MAX_CREDIT;
		else
			credit = new_credit;
	}
	credit -= len;
	qdp->credit = credit;
	bpf_spin_unlock(&qdp->lock);
	

	
	if (qsp != NULL && (qsp->rate * 128) != qdp->rate) {
		qdp->rate = qsp->rate * 128;
		bpf_printk("Updating rate: %d (1sec:%llu bits)\n",
			   (int)qdp->rate,
			   CREDIT_PER_NS(1000000000, qdp->rate) * 8);
	}

	
	
	if (credit < -DROP_THRESH ||
	    (len > LARGE_PKT_THRESH && credit < -LARGE_PKT_DROP_THRESH)) {
		
		drop_flag = true;
		if (pkti.ecn)
			congestion_flag = true;
		else if (pkti.is_tcp)
			cwr_flag = true;
	} else if (credit < 0) {
		
		if (pkti.ecn || pkti.is_tcp) {
			if (credit < -MARK_THRESH)
				congestion_flag = true;
			else
				congestion_flag = false;
		} else {
			congestion_flag = true;
		}
	}

	if (congestion_flag) {
		if (bpf_skb_ecn_set_ce(skb)) {
			ecn_ce_flag = true;
		} else {
			if (pkti.is_tcp) {
				unsigned int rand = bpf_get_prandom_u32();

				if (-credit >= MARK_THRESH +
				    (rand % MARK_REGION_SIZE)) {
					
					cwr_flag = true;
				}
			} else if (len > LARGE_PKT_THRESH) {
				
				drop_flag = true;
			}
		}
	}

	if (qsp != NULL)
		if (qsp->no_cn)
			cwr_flag = false;

	hbm_update_stats(qsp, len, curtime, congestion_flag, drop_flag,
			 cwr_flag, ecn_ce_flag, &pkti, credit);

	if (drop_flag) {
		__sync_add_and_fetch(&(qdp->credit), len);
		rv = DROP_PKT;
	}

	if (cwr_flag)
		rv |= 2;
	return rv;
}
char _license[] SEC("license") = "GPL";

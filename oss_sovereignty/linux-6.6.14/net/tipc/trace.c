 

#define CREATE_TRACE_POINTS
#include "trace.h"

 
unsigned long sysctl_tipc_sk_filter[5] __read_mostly = {0, };

 
int tipc_skb_dump(struct sk_buff *skb, bool more, char *buf)
{
	int i = 0;
	size_t sz = (more) ? SKB_LMAX : SKB_LMIN;
	struct tipc_msg *hdr;
	struct tipc_skb_cb *skbcb;

	if (!skb) {
		i += scnprintf(buf, sz, "msg: (null)\n");
		return i;
	}

	hdr = buf_msg(skb);
	skbcb = TIPC_SKB_CB(skb);

	 
	i += scnprintf(buf, sz, "msg: %u", msg_user(hdr));
	i += scnprintf(buf + i, sz - i, " %u", msg_type(hdr));
	i += scnprintf(buf + i, sz - i, " %u", msg_hdr_sz(hdr));
	i += scnprintf(buf + i, sz - i, " %u", msg_data_sz(hdr));
	i += scnprintf(buf + i, sz - i, " %x", msg_orignode(hdr));
	i += scnprintf(buf + i, sz - i, " %x", msg_destnode(hdr));
	i += scnprintf(buf + i, sz - i, " %u", msg_seqno(hdr));
	i += scnprintf(buf + i, sz - i, " %u", msg_ack(hdr));
	i += scnprintf(buf + i, sz - i, " %u", msg_bcast_ack(hdr));
	switch (msg_user(hdr)) {
	case LINK_PROTOCOL:
		i += scnprintf(buf + i, sz - i, " %c", msg_net_plane(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_probe(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_peer_stopping(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_session(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_next_sent(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_seq_gap(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_bc_snd_nxt(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_bc_gap(hdr));
		break;
	case TIPC_LOW_IMPORTANCE:
	case TIPC_MEDIUM_IMPORTANCE:
	case TIPC_HIGH_IMPORTANCE:
	case TIPC_CRITICAL_IMPORTANCE:
	case CONN_MANAGER:
	case SOCK_WAKEUP:
		i += scnprintf(buf + i, sz - i, " | %u", msg_origport(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_destport(hdr));
		switch (msg_type(hdr)) {
		case TIPC_NAMED_MSG:
			i += scnprintf(buf + i, sz - i, " %u",
				       msg_nametype(hdr));
			i += scnprintf(buf + i, sz - i, " %u",
				       msg_nameinst(hdr));
			break;
		case TIPC_MCAST_MSG:
			i += scnprintf(buf + i, sz - i, " %u",
				       msg_nametype(hdr));
			i += scnprintf(buf + i, sz - i, " %u",
				       msg_namelower(hdr));
			i += scnprintf(buf + i, sz - i, " %u",
				       msg_nameupper(hdr));
			break;
		default:
			break;
		}
		i += scnprintf(buf + i, sz - i, " | %u",
			       msg_src_droppable(hdr));
		i += scnprintf(buf + i, sz - i, " %u",
			       msg_dest_droppable(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_errcode(hdr));
		i += scnprintf(buf + i, sz - i, " %u", msg_reroute_cnt(hdr));
		break;
	default:
		 
		break;
	}

	i += scnprintf(buf + i, sz - i, "\n");
	if (!more)
		return i;

	 
	i += scnprintf(buf + i, sz - i, "skb: %s",
		       (skb->dev) ? skb->dev->name : "n/a");
	i += scnprintf(buf + i, sz - i, " %u", skb->len);
	i += scnprintf(buf + i, sz - i, " %u", skb->data_len);
	i += scnprintf(buf + i, sz - i, " %u", skb->hdr_len);
	i += scnprintf(buf + i, sz - i, " %u", skb->truesize);
	i += scnprintf(buf + i, sz - i, " %u", skb_cloned(skb));
	i += scnprintf(buf + i, sz - i, " %p", skb->sk);
	i += scnprintf(buf + i, sz - i, " %u", skb_shinfo(skb)->nr_frags);
	i += scnprintf(buf + i, sz - i, " %llx",
		       ktime_to_ms(skb_get_ktime(skb)));
	i += scnprintf(buf + i, sz - i, " %llx\n",
		       ktime_to_ms(skb_hwtstamps(skb)->hwtstamp));

	 
	i += scnprintf(buf + i, sz - i, "cb[]: %u", skbcb->bytes_read);
	i += scnprintf(buf + i, sz - i, " %u", skbcb->orig_member);
	i += scnprintf(buf + i, sz - i, " %u",
		       jiffies_to_msecs(skbcb->nxt_retr));
	i += scnprintf(buf + i, sz - i, " %u", skbcb->validated);
	i += scnprintf(buf + i, sz - i, " %u", skbcb->chain_imp);
	i += scnprintf(buf + i, sz - i, " %u\n", skbcb->ackers);

	return i;
}

 
int tipc_list_dump(struct sk_buff_head *list, bool more, char *buf)
{
	int i = 0;
	size_t sz = (more) ? LIST_LMAX : LIST_LMIN;
	u32 count, len;
	struct sk_buff *hskb, *tskb, *skb, *tmp;

	if (!list) {
		i += scnprintf(buf, sz, "(null)\n");
		return i;
	}

	len = skb_queue_len(list);
	i += scnprintf(buf, sz, "len = %d\n", len);

	if (!len)
		return i;

	if (!more) {
		hskb = skb_peek(list);
		i += scnprintf(buf + i, sz - i, "  head ");
		i += tipc_skb_dump(hskb, false, buf + i);
		if (len > 1) {
			tskb = skb_peek_tail(list);
			i += scnprintf(buf + i, sz - i, "  tail ");
			i += tipc_skb_dump(tskb, false, buf + i);
		}
	} else {
		count = 0;
		skb_queue_walk_safe(list, skb, tmp) {
			count++;
			if (count == 6)
				i += scnprintf(buf + i, sz - i, "  .\n  .\n");
			if (count > 5 && count <= len - 5)
				continue;
			i += scnprintf(buf + i, sz - i, "  #%d ", count);
			i += tipc_skb_dump(skb, false, buf + i);
		}
	}
	return i;
}

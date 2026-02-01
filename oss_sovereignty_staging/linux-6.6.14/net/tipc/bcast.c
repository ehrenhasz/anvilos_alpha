 

#include <linux/tipc_config.h>
#include "socket.h"
#include "msg.h"
#include "bcast.h"
#include "link.h"
#include "name_table.h"

#define BCLINK_WIN_DEFAULT  50	 
#define BCLINK_WIN_MIN      32	 

const char tipc_bclink_name[] = "broadcast-link";
unsigned long sysctl_tipc_bc_retruni __read_mostly;

 
struct tipc_bc_base {
	struct tipc_link *link;
	struct sk_buff_head inputq;
	int dests[MAX_BEARERS];
	int primary_bearer;
	bool bcast_support;
	bool force_bcast;
	bool rcast_support;
	bool force_rcast;
	int rc_ratio;
	int bc_threshold;
};

static struct tipc_bc_base *tipc_bc_base(struct net *net)
{
	return tipc_net(net)->bcbase;
}

 
int tipc_bcast_get_mtu(struct net *net)
{
	return tipc_link_mss(tipc_bc_sndlink(net));
}

void tipc_bcast_toggle_rcast(struct net *net, bool supp)
{
	tipc_bc_base(net)->rcast_support = supp;
}

static void tipc_bcbase_calc_bc_threshold(struct net *net)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);
	int cluster_size = tipc_link_bc_peers(tipc_bc_sndlink(net));

	bb->bc_threshold = 1 + (cluster_size * bb->rc_ratio / 100);
}

 
static void tipc_bcbase_select_primary(struct net *net)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);
	int all_dests =  tipc_link_bc_peers(bb->link);
	int max_win = tipc_link_max_win(bb->link);
	int min_win = tipc_link_min_win(bb->link);
	int i, mtu, prim;

	bb->primary_bearer = INVALID_BEARER_ID;
	bb->bcast_support = true;

	if (!all_dests)
		return;

	for (i = 0; i < MAX_BEARERS; i++) {
		if (!bb->dests[i])
			continue;

		mtu = tipc_bearer_mtu(net, i);
		if (mtu < tipc_link_mtu(bb->link)) {
			tipc_link_set_mtu(bb->link, mtu);
			tipc_link_set_queue_limits(bb->link,
						   min_win,
						   max_win);
		}
		bb->bcast_support &= tipc_bearer_bcast_support(net, i);
		if (bb->dests[i] < all_dests)
			continue;

		bb->primary_bearer = i;

		 
		if ((i ^ tipc_own_addr(net)) & 1)
			break;
	}
	prim = bb->primary_bearer;
	if (prim != INVALID_BEARER_ID)
		bb->bcast_support = tipc_bearer_bcast_support(net, prim);
}

void tipc_bcast_inc_bearer_dst_cnt(struct net *net, int bearer_id)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);

	tipc_bcast_lock(net);
	bb->dests[bearer_id]++;
	tipc_bcbase_select_primary(net);
	tipc_bcast_unlock(net);
}

void tipc_bcast_dec_bearer_dst_cnt(struct net *net, int bearer_id)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);

	tipc_bcast_lock(net);
	bb->dests[bearer_id]--;
	tipc_bcbase_select_primary(net);
	tipc_bcast_unlock(net);
}

 
static void tipc_bcbase_xmit(struct net *net, struct sk_buff_head *xmitq)
{
	int bearer_id;
	struct tipc_bc_base *bb = tipc_bc_base(net);
	struct sk_buff *skb, *_skb;
	struct sk_buff_head _xmitq;

	if (skb_queue_empty(xmitq))
		return;

	 
	bearer_id = bb->primary_bearer;
	if (bearer_id >= 0) {
		tipc_bearer_bc_xmit(net, bearer_id, xmitq);
		return;
	}

	 
	__skb_queue_head_init(&_xmitq);
	for (bearer_id = 0; bearer_id < MAX_BEARERS; bearer_id++) {
		if (!bb->dests[bearer_id])
			continue;

		skb_queue_walk(xmitq, skb) {
			_skb = pskb_copy_for_clone(skb, GFP_ATOMIC);
			if (!_skb)
				break;
			__skb_queue_tail(&_xmitq, _skb);
		}
		tipc_bearer_bc_xmit(net, bearer_id, &_xmitq);
	}
	__skb_queue_purge(xmitq);
	__skb_queue_purge(&_xmitq);
}

static void tipc_bcast_select_xmit_method(struct net *net, int dests,
					  struct tipc_mc_method *method)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);
	unsigned long exp = method->expires;

	 
	if (!bb->bcast_support) {
		method->rcast = true;
		return;
	}
	 
	if (!bb->rcast_support) {
		method->rcast = false;
		return;
	}
	 
	method->expires = jiffies + TIPC_METHOD_EXPIRE;
	if (method->mandatory)
		return;

	if (!(tipc_net(net)->capabilities & TIPC_MCAST_RBCTL) &&
	    time_before(jiffies, exp))
		return;

	 
	if (bb->force_bcast) {
		method->rcast = false;
		return;
	}
	 
	if (bb->force_rcast) {
		method->rcast = true;
		return;
	}
	 
	 
	method->rcast = dests <= bb->bc_threshold;
}

 
int tipc_bcast_xmit(struct net *net, struct sk_buff_head *pkts,
		    u16 *cong_link_cnt)
{
	struct tipc_link *l = tipc_bc_sndlink(net);
	struct sk_buff_head xmitq;
	int rc = 0;

	__skb_queue_head_init(&xmitq);
	tipc_bcast_lock(net);
	if (tipc_link_bc_peers(l))
		rc = tipc_link_xmit(l, pkts, &xmitq);
	tipc_bcast_unlock(net);
	tipc_bcbase_xmit(net, &xmitq);
	__skb_queue_purge(pkts);
	if (rc == -ELINKCONG) {
		*cong_link_cnt = 1;
		rc = 0;
	}
	return rc;
}

 
static int tipc_rcast_xmit(struct net *net, struct sk_buff_head *pkts,
			   struct tipc_nlist *dests, u16 *cong_link_cnt)
{
	struct tipc_dest *dst, *tmp;
	struct sk_buff_head _pkts;
	u32 dnode, selector;

	selector = msg_link_selector(buf_msg(skb_peek(pkts)));
	__skb_queue_head_init(&_pkts);

	list_for_each_entry_safe(dst, tmp, &dests->list, list) {
		dnode = dst->node;
		if (!tipc_msg_pskb_copy(dnode, pkts, &_pkts))
			return -ENOMEM;

		 
		if (tipc_node_xmit(net, &_pkts, dnode, selector) == -ELINKCONG)
			(*cong_link_cnt)++;
	}
	return 0;
}

 
static int tipc_mcast_send_sync(struct net *net, struct sk_buff *skb,
				struct tipc_mc_method *method,
				struct tipc_nlist *dests)
{
	struct tipc_msg *hdr, *_hdr;
	struct sk_buff_head tmpq;
	struct sk_buff *_skb;
	u16 cong_link_cnt;
	int rc = 0;

	 
	if (!(tipc_net(net)->capabilities & TIPC_MCAST_RBCTL))
		return 0;

	hdr = buf_msg(skb);
	if (msg_user(hdr) == MSG_FRAGMENTER)
		hdr = msg_inner_hdr(hdr);
	if (msg_type(hdr) != TIPC_MCAST_MSG)
		return 0;

	 
	_skb = tipc_buf_acquire(MCAST_H_SIZE, GFP_KERNEL);
	if (!_skb)
		return -ENOMEM;

	 
	msg_set_syn(hdr, 1);

	 
	skb_copy_to_linear_data(_skb, hdr, MCAST_H_SIZE);
	skb_orphan(_skb);

	 
	_hdr = buf_msg(_skb);
	msg_set_size(_hdr, MCAST_H_SIZE);
	msg_set_is_rcast(_hdr, !msg_is_rcast(hdr));
	msg_set_errcode(_hdr, TIPC_ERR_NO_PORT);

	__skb_queue_head_init(&tmpq);
	__skb_queue_tail(&tmpq, _skb);
	if (method->rcast)
		rc = tipc_bcast_xmit(net, &tmpq, &cong_link_cnt);
	else
		rc = tipc_rcast_xmit(net, &tmpq, dests, &cong_link_cnt);

	 
	__skb_queue_purge(&tmpq);

	return rc;
}

 
int tipc_mcast_xmit(struct net *net, struct sk_buff_head *pkts,
		    struct tipc_mc_method *method, struct tipc_nlist *dests,
		    u16 *cong_link_cnt)
{
	struct sk_buff_head inputq, localq;
	bool rcast = method->rcast;
	struct tipc_msg *hdr;
	struct sk_buff *skb;
	int rc = 0;

	skb_queue_head_init(&inputq);
	__skb_queue_head_init(&localq);

	 
	if (dests->local && !tipc_msg_reassemble(pkts, &localq)) {
		rc = -ENOMEM;
		goto exit;
	}
	 
	if (dests->remote) {
		tipc_bcast_select_xmit_method(net, dests->remote, method);

		skb = skb_peek(pkts);
		hdr = buf_msg(skb);
		if (msg_user(hdr) == MSG_FRAGMENTER)
			hdr = msg_inner_hdr(hdr);
		msg_set_is_rcast(hdr, method->rcast);

		 
		if (rcast != method->rcast) {
			rc = tipc_mcast_send_sync(net, skb, method, dests);
			if (unlikely(rc)) {
				pr_err("Unable to send SYN: method %d, rc %d\n",
				       rcast, rc);
				goto exit;
			}
		}

		if (method->rcast)
			rc = tipc_rcast_xmit(net, pkts, dests, cong_link_cnt);
		else
			rc = tipc_bcast_xmit(net, pkts, cong_link_cnt);
	}

	if (dests->local) {
		tipc_loopback_trace(net, &localq);
		tipc_sk_mcast_rcv(net, &localq, &inputq);
	}
exit:
	 
	__skb_queue_purge(pkts);
	return rc;
}

 
int tipc_bcast_rcv(struct net *net, struct tipc_link *l, struct sk_buff *skb)
{
	struct tipc_msg *hdr = buf_msg(skb);
	struct sk_buff_head *inputq = &tipc_bc_base(net)->inputq;
	struct sk_buff_head xmitq;
	int rc;

	__skb_queue_head_init(&xmitq);

	if (msg_mc_netid(hdr) != tipc_netid(net) || !tipc_link_is_up(l)) {
		kfree_skb(skb);
		return 0;
	}

	tipc_bcast_lock(net);
	if (msg_user(hdr) == BCAST_PROTOCOL)
		rc = tipc_link_bc_nack_rcv(l, skb, &xmitq);
	else
		rc = tipc_link_rcv(l, skb, NULL);
	tipc_bcast_unlock(net);

	tipc_bcbase_xmit(net, &xmitq);

	 
	if (!skb_queue_empty(inputq))
		tipc_sk_rcv(net, inputq);

	return rc;
}

 
void tipc_bcast_ack_rcv(struct net *net, struct tipc_link *l,
			struct tipc_msg *hdr)
{
	struct sk_buff_head *inputq = &tipc_bc_base(net)->inputq;
	u16 acked = msg_bcast_ack(hdr);
	struct sk_buff_head xmitq;

	 
	if (msg_bc_ack_invalid(hdr))
		return;

	__skb_queue_head_init(&xmitq);

	tipc_bcast_lock(net);
	tipc_link_bc_ack_rcv(l, acked, 0, NULL, &xmitq, NULL);
	tipc_bcast_unlock(net);

	tipc_bcbase_xmit(net, &xmitq);

	 
	if (!skb_queue_empty(inputq))
		tipc_sk_rcv(net, inputq);
}

 
int tipc_bcast_sync_rcv(struct net *net, struct tipc_link *l,
			struct tipc_msg *hdr,
			struct sk_buff_head *retrq)
{
	struct sk_buff_head *inputq = &tipc_bc_base(net)->inputq;
	struct tipc_gap_ack_blks *ga;
	struct sk_buff_head xmitq;
	int rc = 0;

	__skb_queue_head_init(&xmitq);

	tipc_bcast_lock(net);
	if (msg_type(hdr) != STATE_MSG) {
		tipc_link_bc_init_rcv(l, hdr);
	} else if (!msg_bc_ack_invalid(hdr)) {
		tipc_get_gap_ack_blks(&ga, l, hdr, false);
		if (!sysctl_tipc_bc_retruni)
			retrq = &xmitq;
		rc = tipc_link_bc_ack_rcv(l, msg_bcast_ack(hdr),
					  msg_bc_gap(hdr), ga, &xmitq,
					  retrq);
		rc |= tipc_link_bc_sync_rcv(l, hdr, &xmitq);
	}
	tipc_bcast_unlock(net);

	tipc_bcbase_xmit(net, &xmitq);

	 
	if (!skb_queue_empty(inputq))
		tipc_sk_rcv(net, inputq);
	return rc;
}

 
void tipc_bcast_add_peer(struct net *net, struct tipc_link *uc_l,
			 struct sk_buff_head *xmitq)
{
	struct tipc_link *snd_l = tipc_bc_sndlink(net);

	tipc_bcast_lock(net);
	tipc_link_add_bc_peer(snd_l, uc_l, xmitq);
	tipc_bcbase_select_primary(net);
	tipc_bcbase_calc_bc_threshold(net);
	tipc_bcast_unlock(net);
}

 
void tipc_bcast_remove_peer(struct net *net, struct tipc_link *rcv_l)
{
	struct tipc_link *snd_l = tipc_bc_sndlink(net);
	struct sk_buff_head *inputq = &tipc_bc_base(net)->inputq;
	struct sk_buff_head xmitq;

	__skb_queue_head_init(&xmitq);

	tipc_bcast_lock(net);
	tipc_link_remove_bc_peer(snd_l, rcv_l, &xmitq);
	tipc_bcbase_select_primary(net);
	tipc_bcbase_calc_bc_threshold(net);
	tipc_bcast_unlock(net);

	tipc_bcbase_xmit(net, &xmitq);

	 
	if (!skb_queue_empty(inputq))
		tipc_sk_rcv(net, inputq);
}

int tipc_bclink_reset_stats(struct net *net, struct tipc_link *l)
{
	if (!l)
		return -ENOPROTOOPT;

	tipc_bcast_lock(net);
	tipc_link_reset_stats(l);
	tipc_bcast_unlock(net);
	return 0;
}

static int tipc_bc_link_set_queue_limits(struct net *net, u32 max_win)
{
	struct tipc_link *l = tipc_bc_sndlink(net);

	if (!l)
		return -ENOPROTOOPT;
	if (max_win < BCLINK_WIN_MIN)
		max_win = BCLINK_WIN_MIN;
	if (max_win > TIPC_MAX_LINK_WIN)
		return -EINVAL;
	tipc_bcast_lock(net);
	tipc_link_set_queue_limits(l, tipc_link_min_win(l), max_win);
	tipc_bcast_unlock(net);
	return 0;
}

static int tipc_bc_link_set_broadcast_mode(struct net *net, u32 bc_mode)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);

	switch (bc_mode) {
	case BCLINK_MODE_BCAST:
		if (!bb->bcast_support)
			return -ENOPROTOOPT;

		bb->force_bcast = true;
		bb->force_rcast = false;
		break;
	case BCLINK_MODE_RCAST:
		if (!bb->rcast_support)
			return -ENOPROTOOPT;

		bb->force_bcast = false;
		bb->force_rcast = true;
		break;
	case BCLINK_MODE_SEL:
		if (!bb->bcast_support || !bb->rcast_support)
			return -ENOPROTOOPT;

		bb->force_bcast = false;
		bb->force_rcast = false;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tipc_bc_link_set_broadcast_ratio(struct net *net, u32 bc_ratio)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);

	if (!bb->bcast_support || !bb->rcast_support)
		return -ENOPROTOOPT;

	if (bc_ratio > 100 || bc_ratio <= 0)
		return -EINVAL;

	bb->rc_ratio = bc_ratio;
	tipc_bcast_lock(net);
	tipc_bcbase_calc_bc_threshold(net);
	tipc_bcast_unlock(net);

	return 0;
}

int tipc_nl_bc_link_set(struct net *net, struct nlattr *attrs[])
{
	int err;
	u32 win;
	u32 bc_mode;
	u32 bc_ratio;
	struct nlattr *props[TIPC_NLA_PROP_MAX + 1];

	if (!attrs[TIPC_NLA_LINK_PROP])
		return -EINVAL;

	err = tipc_nl_parse_link_prop(attrs[TIPC_NLA_LINK_PROP], props);
	if (err)
		return err;

	if (!props[TIPC_NLA_PROP_WIN] &&
	    !props[TIPC_NLA_PROP_BROADCAST] &&
	    !props[TIPC_NLA_PROP_BROADCAST_RATIO]) {
		return -EOPNOTSUPP;
	}

	if (props[TIPC_NLA_PROP_BROADCAST]) {
		bc_mode = nla_get_u32(props[TIPC_NLA_PROP_BROADCAST]);
		err = tipc_bc_link_set_broadcast_mode(net, bc_mode);
	}

	if (!err && props[TIPC_NLA_PROP_BROADCAST_RATIO]) {
		bc_ratio = nla_get_u32(props[TIPC_NLA_PROP_BROADCAST_RATIO]);
		err = tipc_bc_link_set_broadcast_ratio(net, bc_ratio);
	}

	if (!err && props[TIPC_NLA_PROP_WIN]) {
		win = nla_get_u32(props[TIPC_NLA_PROP_WIN]);
		err = tipc_bc_link_set_queue_limits(net, win);
	}

	return err;
}

int tipc_bcast_init(struct net *net)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_bc_base *bb = NULL;
	struct tipc_link *l = NULL;

	bb = kzalloc(sizeof(*bb), GFP_KERNEL);
	if (!bb)
		goto enomem;
	tn->bcbase = bb;
	spin_lock_init(&tipc_net(net)->bclock);

	if (!tipc_link_bc_create(net, 0, 0, NULL,
				 one_page_mtu,
				 BCLINK_WIN_DEFAULT,
				 BCLINK_WIN_DEFAULT,
				 0,
				 &bb->inputq,
				 NULL,
				 NULL,
				 &l))
		goto enomem;
	bb->link = l;
	tn->bcl = l;
	bb->rc_ratio = 10;
	bb->rcast_support = true;
	return 0;
enomem:
	kfree(bb);
	kfree(l);
	return -ENOMEM;
}

void tipc_bcast_stop(struct net *net)
{
	struct tipc_net *tn = net_generic(net, tipc_net_id);

	synchronize_net();
	kfree(tn->bcbase);
	kfree(tn->bcl);
}

void tipc_nlist_init(struct tipc_nlist *nl, u32 self)
{
	memset(nl, 0, sizeof(*nl));
	INIT_LIST_HEAD(&nl->list);
	nl->self = self;
}

void tipc_nlist_add(struct tipc_nlist *nl, u32 node)
{
	if (node == nl->self)
		nl->local = true;
	else if (tipc_dest_push(&nl->list, node, 0))
		nl->remote++;
}

void tipc_nlist_del(struct tipc_nlist *nl, u32 node)
{
	if (node == nl->self)
		nl->local = false;
	else if (tipc_dest_del(&nl->list, node, 0))
		nl->remote--;
}

void tipc_nlist_purge(struct tipc_nlist *nl)
{
	tipc_dest_list_purge(&nl->list);
	nl->remote = 0;
	nl->local = false;
}

u32 tipc_bcast_get_mode(struct net *net)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);

	if (bb->force_bcast)
		return BCLINK_MODE_BCAST;

	if (bb->force_rcast)
		return BCLINK_MODE_RCAST;

	if (bb->bcast_support && bb->rcast_support)
		return BCLINK_MODE_SEL;

	return 0;
}

u32 tipc_bcast_get_broadcast_ratio(struct net *net)
{
	struct tipc_bc_base *bb = tipc_bc_base(net);

	return bb->rc_ratio;
}

void tipc_mcast_filter_msg(struct net *net, struct sk_buff_head *defq,
			   struct sk_buff_head *inputq)
{
	struct sk_buff *skb, *_skb, *tmp;
	struct tipc_msg *hdr, *_hdr;
	bool match = false;
	u32 node, port;

	skb = skb_peek(inputq);
	if (!skb)
		return;

	hdr = buf_msg(skb);

	if (likely(!msg_is_syn(hdr) && skb_queue_empty(defq)))
		return;

	node = msg_orignode(hdr);
	if (node == tipc_own_addr(net))
		return;

	port = msg_origport(hdr);

	 
	skb_queue_walk(defq, _skb) {
		_hdr = buf_msg(_skb);
		if (msg_orignode(_hdr) != node)
			continue;
		if (msg_origport(_hdr) != port)
			continue;
		match = true;
		break;
	}

	if (!match) {
		if (!msg_is_syn(hdr))
			return;
		__skb_dequeue(inputq);
		__skb_queue_tail(defq, skb);
		return;
	}

	 
	if (!msg_is_syn(hdr)) {
		if (msg_is_rcast(hdr) != msg_is_rcast(_hdr))
			return;
		__skb_dequeue(inputq);
		__skb_queue_tail(defq, skb);
		return;
	}

	 
	if (msg_is_rcast(hdr) == msg_is_rcast(_hdr)) {
		__skb_dequeue(inputq);
		__skb_queue_tail(defq, skb);
		return;
	}

	 
	__skb_unlink(_skb, defq);
	if (msg_data_sz(hdr)) {
		kfree_skb(_skb);
	} else {
		__skb_dequeue(inputq);
		kfree_skb(skb);
		__skb_queue_tail(inputq, _skb);
	}

	 
	skb_queue_walk_safe(defq, _skb, tmp) {
		_hdr = buf_msg(_skb);
		if (msg_orignode(_hdr) != node)
			continue;
		if (msg_origport(_hdr) != port)
			continue;
		if (msg_is_syn(_hdr))
			break;
		__skb_unlink(_skb, defq);
		__skb_queue_tail(inputq, _skb);
	}
}

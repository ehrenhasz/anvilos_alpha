 

#include "core.h"
#include "node.h"
#include "discover.h"

 
#define TIPC_DISC_INIT	msecs_to_jiffies(125)
 
#define TIPC_DISC_FAST	msecs_to_jiffies(1000)
 
#define TIPC_DISC_SLOW	msecs_to_jiffies(60000)
 
#define TIPC_DISC_INACTIVE	0xffffffff

 
struct tipc_discoverer {
	u32 bearer_id;
	struct tipc_media_addr dest;
	struct net *net;
	u32 domain;
	int num_nodes;
	spinlock_t lock;
	struct sk_buff *skb;
	struct timer_list timer;
	unsigned long timer_intv;
};

 
static void tipc_disc_init_msg(struct net *net, struct sk_buff *skb,
			       u32 mtyp,  struct tipc_bearer *b)
{
	struct tipc_net *tn = tipc_net(net);
	u32 dest_domain = b->domain;
	struct tipc_msg *hdr;

	hdr = buf_msg(skb);
	tipc_msg_init(tn->trial_addr, hdr, LINK_CONFIG, mtyp,
		      MAX_H_SIZE, dest_domain);
	msg_set_size(hdr, MAX_H_SIZE + NODE_ID_LEN);
	msg_set_non_seq(hdr, 1);
	msg_set_node_sig(hdr, tn->random);
	msg_set_node_capabilities(hdr, TIPC_NODE_CAPABILITIES);
	msg_set_dest_domain(hdr, dest_domain);
	msg_set_bc_netid(hdr, tn->net_id);
	b->media->addr2msg(msg_media_addr(hdr), &b->addr);
	msg_set_peer_net_hash(hdr, tipc_net_hash_mixes(net, tn->random));
	msg_set_node_id(hdr, tipc_own_id(net));
}

static void tipc_disc_msg_xmit(struct net *net, u32 mtyp, u32 dst,
			       u32 src, u32 sugg_addr,
			       struct tipc_media_addr *maddr,
			       struct tipc_bearer *b)
{
	struct tipc_msg *hdr;
	struct sk_buff *skb;

	skb = tipc_buf_acquire(MAX_H_SIZE + NODE_ID_LEN, GFP_ATOMIC);
	if (!skb)
		return;
	hdr = buf_msg(skb);
	tipc_disc_init_msg(net, skb, mtyp, b);
	msg_set_sugg_node_addr(hdr, sugg_addr);
	msg_set_dest_domain(hdr, dst);
	tipc_bearer_xmit_skb(net, b->identity, skb, maddr);
}

 
static void disc_dupl_alert(struct tipc_bearer *b, u32 node_addr,
			    struct tipc_media_addr *media_addr)
{
	char media_addr_str[64];

	tipc_media_addr_printf(media_addr_str, sizeof(media_addr_str),
			       media_addr);
	pr_warn("Duplicate %x using %s seen on <%s>\n", node_addr,
		media_addr_str, b->name);
}

 
static bool tipc_disc_addr_trial_msg(struct tipc_discoverer *d,
				     struct tipc_media_addr *maddr,
				     struct tipc_bearer *b,
				     u32 dst, u32 src,
				     u32 sugg_addr,
				     u8 *peer_id,
				     int mtyp)
{
	struct net *net = d->net;
	struct tipc_net *tn = tipc_net(net);
	u32 self = tipc_own_addr(net);
	bool trial = time_before(jiffies, tn->addr_trial_end) && !self;

	if (mtyp == DSC_TRIAL_FAIL_MSG) {
		if (!trial)
			return true;

		 
		if (dst != tn->trial_addr)
			return true;

		 
		tn->trial_addr = sugg_addr;
		msg_set_prevnode(buf_msg(d->skb), sugg_addr);
		tn->addr_trial_end = jiffies + msecs_to_jiffies(1000);
		return true;
	}

	 
	if (!trial && !self) {
		schedule_work(&tn->work);
		msg_set_prevnode(buf_msg(d->skb), tn->trial_addr);
		msg_set_type(buf_msg(d->skb), DSC_REQ_MSG);
	}

	 
	if (mtyp != DSC_TRIAL_MSG)
		return trial;

	sugg_addr = tipc_node_try_addr(net, peer_id, src);
	if (sugg_addr)
		tipc_disc_msg_xmit(net, DSC_TRIAL_FAIL_MSG, src,
				   self, sugg_addr, maddr, b);
	return true;
}

 
void tipc_disc_rcv(struct net *net, struct sk_buff *skb,
		   struct tipc_bearer *b)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_msg *hdr = buf_msg(skb);
	u32 pnet_hash = msg_peer_net_hash(hdr);
	u16 caps = msg_node_capabilities(hdr);
	bool legacy = tn->legacy_addr_format;
	u32 sugg = msg_sugg_node_addr(hdr);
	u32 signature = msg_node_sig(hdr);
	u8 peer_id[NODE_ID_LEN] = {0,};
	u32 dst = msg_dest_domain(hdr);
	u32 net_id = msg_bc_netid(hdr);
	struct tipc_media_addr maddr;
	u32 src = msg_prevnode(hdr);
	u32 mtyp = msg_type(hdr);
	bool dupl_addr = false;
	bool respond = false;
	u32 self;
	int err;

	if (skb_linearize(skb)) {
		kfree_skb(skb);
		return;
	}
	hdr = buf_msg(skb);

	if (caps & TIPC_NODE_ID128)
		memcpy(peer_id, msg_node_id(hdr), NODE_ID_LEN);
	else
		sprintf(peer_id, "%x", src);

	err = b->media->msg2addr(b, &maddr, msg_media_addr(hdr));
	kfree_skb(skb);
	if (err || maddr.broadcast) {
		pr_warn_ratelimited("Rcv corrupt discovery message\n");
		return;
	}
	 
	if (!memcmp(&maddr, &b->addr, sizeof(maddr)))
		return;
	if (net_id != tn->net_id)
		return;
	if (tipc_disc_addr_trial_msg(b->disc, &maddr, b, dst,
				     src, sugg, peer_id, mtyp))
		return;
	self = tipc_own_addr(net);

	 
	if (in_own_node(net, src)) {
		disc_dupl_alert(b, self, &maddr);
		return;
	}
	if (!tipc_in_scope(legacy, dst, self))
		return;
	if (!tipc_in_scope(legacy, b->domain, src))
		return;
	tipc_node_check_dest(net, src, peer_id, b, caps, signature, pnet_hash,
			     &maddr, &respond, &dupl_addr);
	if (dupl_addr)
		disc_dupl_alert(b, src, &maddr);
	if (!respond)
		return;
	if (mtyp != DSC_REQ_MSG)
		return;
	tipc_disc_msg_xmit(net, DSC_RESP_MSG, src, self, 0, &maddr, b);
}

 
void tipc_disc_add_dest(struct tipc_discoverer *d)
{
	spin_lock_bh(&d->lock);
	d->num_nodes++;
	spin_unlock_bh(&d->lock);
}

 
void tipc_disc_remove_dest(struct tipc_discoverer *d)
{
	int intv, num;

	spin_lock_bh(&d->lock);
	d->num_nodes--;
	num = d->num_nodes;
	intv = d->timer_intv;
	if (!num && (intv == TIPC_DISC_INACTIVE || intv > TIPC_DISC_FAST))  {
		d->timer_intv = TIPC_DISC_INIT;
		mod_timer(&d->timer, jiffies + d->timer_intv);
	}
	spin_unlock_bh(&d->lock);
}

 
static void tipc_disc_timeout(struct timer_list *t)
{
	struct tipc_discoverer *d = from_timer(d, t, timer);
	struct tipc_net *tn = tipc_net(d->net);
	struct tipc_media_addr maddr;
	struct sk_buff *skb = NULL;
	struct net *net = d->net;
	u32 bearer_id;

	spin_lock_bh(&d->lock);

	 
	if (tipc_node(d->domain) && d->num_nodes) {
		d->timer_intv = TIPC_DISC_INACTIVE;
		goto exit;
	}

	 
	if (!time_before(jiffies, tn->addr_trial_end) && !tipc_own_addr(net)) {
		mod_timer(&d->timer, jiffies + TIPC_DISC_INIT);
		spin_unlock_bh(&d->lock);
		schedule_work(&tn->work);
		return;
	}

	 
	if (time_before(jiffies, tn->addr_trial_end)) {
		d->timer_intv = TIPC_DISC_INIT;
	} else {
		d->timer_intv *= 2;
		if (d->num_nodes && d->timer_intv > TIPC_DISC_SLOW)
			d->timer_intv = TIPC_DISC_SLOW;
		else if (!d->num_nodes && d->timer_intv > TIPC_DISC_FAST)
			d->timer_intv = TIPC_DISC_FAST;
		msg_set_type(buf_msg(d->skb), DSC_REQ_MSG);
		msg_set_prevnode(buf_msg(d->skb), tn->trial_addr);
	}

	mod_timer(&d->timer, jiffies + d->timer_intv);
	memcpy(&maddr, &d->dest, sizeof(maddr));
	skb = skb_clone(d->skb, GFP_ATOMIC);
	bearer_id = d->bearer_id;
exit:
	spin_unlock_bh(&d->lock);
	if (skb)
		tipc_bearer_xmit_skb(net, bearer_id, skb, &maddr);
}

 
int tipc_disc_create(struct net *net, struct tipc_bearer *b,
		     struct tipc_media_addr *dest, struct sk_buff **skb)
{
	struct tipc_net *tn = tipc_net(net);
	struct tipc_discoverer *d;

	d = kmalloc(sizeof(*d), GFP_ATOMIC);
	if (!d)
		return -ENOMEM;
	d->skb = tipc_buf_acquire(MAX_H_SIZE + NODE_ID_LEN, GFP_ATOMIC);
	if (!d->skb) {
		kfree(d);
		return -ENOMEM;
	}
	tipc_disc_init_msg(net, d->skb, DSC_REQ_MSG, b);

	 
	if (!tipc_own_addr(net)) {
		tn->addr_trial_end = jiffies + msecs_to_jiffies(1000);
		msg_set_type(buf_msg(d->skb), DSC_TRIAL_MSG);
	}
	memcpy(&d->dest, dest, sizeof(*dest));
	d->net = net;
	d->bearer_id = b->identity;
	d->domain = b->domain;
	d->num_nodes = 0;
	d->timer_intv = TIPC_DISC_INIT;
	spin_lock_init(&d->lock);
	timer_setup(&d->timer, tipc_disc_timeout, 0);
	mod_timer(&d->timer, jiffies + d->timer_intv);
	b->disc = d;
	*skb = skb_clone(d->skb, GFP_ATOMIC);
	return 0;
}

 
void tipc_disc_delete(struct tipc_discoverer *d)
{
	timer_shutdown_sync(&d->timer);
	kfree_skb(d->skb);
	kfree(d);
}

 
void tipc_disc_reset(struct net *net, struct tipc_bearer *b)
{
	struct tipc_discoverer *d = b->disc;
	struct tipc_media_addr maddr;
	struct sk_buff *skb;

	spin_lock_bh(&d->lock);
	tipc_disc_init_msg(net, d->skb, DSC_REQ_MSG, b);
	d->net = net;
	d->bearer_id = b->identity;
	d->domain = b->domain;
	d->num_nodes = 0;
	d->timer_intv = TIPC_DISC_INIT;
	memcpy(&maddr, &d->dest, sizeof(maddr));
	mod_timer(&d->timer, jiffies + d->timer_intv);
	skb = skb_clone(d->skb, GFP_ATOMIC);
	spin_unlock_bh(&d->lock);
	if (skb)
		tipc_bearer_xmit_skb(net, b->identity, skb, &maddr);
}

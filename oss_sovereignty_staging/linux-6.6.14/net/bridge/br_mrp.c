

#include <linux/mrp_bridge.h>
#include "br_private_mrp.h"

static const u8 mrp_test_dmac[ETH_ALEN] = { 0x1, 0x15, 0x4e, 0x0, 0x0, 0x1 };
static const u8 mrp_in_test_dmac[ETH_ALEN] = { 0x1, 0x15, 0x4e, 0x0, 0x0, 0x3 };

static int br_mrp_process(struct net_bridge_port *p, struct sk_buff *skb);

static struct br_frame_type mrp_frame_type __read_mostly = {
	.type = cpu_to_be16(ETH_P_MRP),
	.frame_handler = br_mrp_process,
};

static bool br_mrp_is_ring_port(struct net_bridge_port *p_port,
				struct net_bridge_port *s_port,
				struct net_bridge_port *port)
{
	if (port == p_port ||
	    port == s_port)
		return true;

	return false;
}

static bool br_mrp_is_in_port(struct net_bridge_port *i_port,
			      struct net_bridge_port *port)
{
	if (port == i_port)
		return true;

	return false;
}

static struct net_bridge_port *br_mrp_get_port(struct net_bridge *br,
					       u32 ifindex)
{
	struct net_bridge_port *res = NULL;
	struct net_bridge_port *port;

	list_for_each_entry(port, &br->port_list, list) {
		if (port->dev->ifindex == ifindex) {
			res = port;
			break;
		}
	}

	return res;
}

static struct br_mrp *br_mrp_find_id(struct net_bridge *br, u32 ring_id)
{
	struct br_mrp *res = NULL;
	struct br_mrp *mrp;

	hlist_for_each_entry_rcu(mrp, &br->mrp_list, list,
				 lockdep_rtnl_is_held()) {
		if (mrp->ring_id == ring_id) {
			res = mrp;
			break;
		}
	}

	return res;
}

static struct br_mrp *br_mrp_find_in_id(struct net_bridge *br, u32 in_id)
{
	struct br_mrp *res = NULL;
	struct br_mrp *mrp;

	hlist_for_each_entry_rcu(mrp, &br->mrp_list, list,
				 lockdep_rtnl_is_held()) {
		if (mrp->in_id == in_id) {
			res = mrp;
			break;
		}
	}

	return res;
}

static bool br_mrp_unique_ifindex(struct net_bridge *br, u32 ifindex)
{
	struct br_mrp *mrp;

	hlist_for_each_entry_rcu(mrp, &br->mrp_list, list,
				 lockdep_rtnl_is_held()) {
		struct net_bridge_port *p;

		p = rtnl_dereference(mrp->p_port);
		if (p && p->dev->ifindex == ifindex)
			return false;

		p = rtnl_dereference(mrp->s_port);
		if (p && p->dev->ifindex == ifindex)
			return false;

		p = rtnl_dereference(mrp->i_port);
		if (p && p->dev->ifindex == ifindex)
			return false;
	}

	return true;
}

static struct br_mrp *br_mrp_find_port(struct net_bridge *br,
				       struct net_bridge_port *p)
{
	struct br_mrp *res = NULL;
	struct br_mrp *mrp;

	hlist_for_each_entry_rcu(mrp, &br->mrp_list, list,
				 lockdep_rtnl_is_held()) {
		if (rcu_access_pointer(mrp->p_port) == p ||
		    rcu_access_pointer(mrp->s_port) == p ||
		    rcu_access_pointer(mrp->i_port) == p) {
			res = mrp;
			break;
		}
	}

	return res;
}

static int br_mrp_next_seq(struct br_mrp *mrp)
{
	mrp->seq_id++;
	return mrp->seq_id;
}

static struct sk_buff *br_mrp_skb_alloc(struct net_bridge_port *p,
					const u8 *src, const u8 *dst)
{
	struct ethhdr *eth_hdr;
	struct sk_buff *skb;
	__be16 *version;

	skb = dev_alloc_skb(MRP_MAX_FRAME_LENGTH);
	if (!skb)
		return NULL;

	skb->dev = p->dev;
	skb->protocol = htons(ETH_P_MRP);
	skb->priority = MRP_FRAME_PRIO;
	skb_reserve(skb, sizeof(*eth_hdr));

	eth_hdr = skb_push(skb, sizeof(*eth_hdr));
	ether_addr_copy(eth_hdr->h_dest, dst);
	ether_addr_copy(eth_hdr->h_source, src);
	eth_hdr->h_proto = htons(ETH_P_MRP);

	version = skb_put(skb, sizeof(*version));
	*version = cpu_to_be16(MRP_VERSION);

	return skb;
}

static void br_mrp_skb_tlv(struct sk_buff *skb,
			   enum br_mrp_tlv_header_type type,
			   u8 length)
{
	struct br_mrp_tlv_hdr *hdr;

	hdr = skb_put(skb, sizeof(*hdr));
	hdr->type = type;
	hdr->length = length;
}

static void br_mrp_skb_common(struct sk_buff *skb, struct br_mrp *mrp)
{
	struct br_mrp_common_hdr *hdr;

	br_mrp_skb_tlv(skb, BR_MRP_TLV_HEADER_COMMON, sizeof(*hdr));

	hdr = skb_put(skb, sizeof(*hdr));
	hdr->seq_id = cpu_to_be16(br_mrp_next_seq(mrp));
	memset(hdr->domain, 0xff, MRP_DOMAIN_UUID_LENGTH);
}

static struct sk_buff *br_mrp_alloc_test_skb(struct br_mrp *mrp,
					     struct net_bridge_port *p,
					     enum br_mrp_port_role_type port_role)
{
	struct br_mrp_ring_test_hdr *hdr = NULL;
	struct sk_buff *skb = NULL;

	if (!p)
		return NULL;

	skb = br_mrp_skb_alloc(p, p->dev->dev_addr, mrp_test_dmac);
	if (!skb)
		return NULL;

	br_mrp_skb_tlv(skb, BR_MRP_TLV_HEADER_RING_TEST, sizeof(*hdr));
	hdr = skb_put(skb, sizeof(*hdr));

	hdr->prio = cpu_to_be16(mrp->prio);
	ether_addr_copy(hdr->sa, p->br->dev->dev_addr);
	hdr->port_role = cpu_to_be16(port_role);
	hdr->state = cpu_to_be16(mrp->ring_state);
	hdr->transitions = cpu_to_be16(mrp->ring_transitions);
	hdr->timestamp = cpu_to_be32(jiffies_to_msecs(jiffies));

	br_mrp_skb_common(skb, mrp);

	 
	if (mrp->ring_role == BR_MRP_RING_ROLE_MRA) {
		struct br_mrp_sub_option1_hdr *sub_opt = NULL;
		struct br_mrp_tlv_hdr *sub_tlv = NULL;
		struct br_mrp_oui_hdr *oui = NULL;
		u8 length;

		length = sizeof(*sub_opt) + sizeof(*sub_tlv) + sizeof(oui) +
			MRP_OPT_PADDING;
		br_mrp_skb_tlv(skb, BR_MRP_TLV_HEADER_OPTION, length);

		oui = skb_put(skb, sizeof(*oui));
		memset(oui, 0x0, sizeof(*oui));
		sub_opt = skb_put(skb, sizeof(*sub_opt));
		memset(sub_opt, 0x0, sizeof(*sub_opt));

		sub_tlv = skb_put(skb, sizeof(*sub_tlv));
		sub_tlv->type = BR_MRP_SUB_TLV_HEADER_TEST_AUTO_MGR;

		 
		skb_put(skb, MRP_OPT_PADDING);
	}

	br_mrp_skb_tlv(skb, BR_MRP_TLV_HEADER_END, 0x0);

	return skb;
}

static struct sk_buff *br_mrp_alloc_in_test_skb(struct br_mrp *mrp,
						struct net_bridge_port *p,
						enum br_mrp_port_role_type port_role)
{
	struct br_mrp_in_test_hdr *hdr = NULL;
	struct sk_buff *skb = NULL;

	if (!p)
		return NULL;

	skb = br_mrp_skb_alloc(p, p->dev->dev_addr, mrp_in_test_dmac);
	if (!skb)
		return NULL;

	br_mrp_skb_tlv(skb, BR_MRP_TLV_HEADER_IN_TEST, sizeof(*hdr));
	hdr = skb_put(skb, sizeof(*hdr));

	hdr->id = cpu_to_be16(mrp->in_id);
	ether_addr_copy(hdr->sa, p->br->dev->dev_addr);
	hdr->port_role = cpu_to_be16(port_role);
	hdr->state = cpu_to_be16(mrp->in_state);
	hdr->transitions = cpu_to_be16(mrp->in_transitions);
	hdr->timestamp = cpu_to_be32(jiffies_to_msecs(jiffies));

	br_mrp_skb_common(skb, mrp);
	br_mrp_skb_tlv(skb, BR_MRP_TLV_HEADER_END, 0x0);

	return skb;
}

 
static void br_mrp_test_work_expired(struct work_struct *work)
{
	struct delayed_work *del_work = to_delayed_work(work);
	struct br_mrp *mrp = container_of(del_work, struct br_mrp, test_work);
	struct net_bridge_port *p;
	bool notify_open = false;
	struct sk_buff *skb;

	if (time_before_eq(mrp->test_end, jiffies))
		return;

	if (mrp->test_count_miss < mrp->test_max_miss) {
		mrp->test_count_miss++;
	} else {
		 
		if (mrp->ring_state == BR_MRP_RING_STATE_CLOSED ||
		    mrp->test_monitor)
			notify_open = true;
	}

	rcu_read_lock();

	p = rcu_dereference(mrp->p_port);
	if (p) {
		if (!mrp->test_monitor) {
			skb = br_mrp_alloc_test_skb(mrp, p,
						    BR_MRP_PORT_ROLE_PRIMARY);
			if (!skb)
				goto out;

			skb_reset_network_header(skb);
			dev_queue_xmit(skb);
		}

		if (notify_open && !mrp->ring_role_offloaded)
			br_mrp_ring_port_open(p->dev, true);
	}

	p = rcu_dereference(mrp->s_port);
	if (p) {
		if (!mrp->test_monitor) {
			skb = br_mrp_alloc_test_skb(mrp, p,
						    BR_MRP_PORT_ROLE_SECONDARY);
			if (!skb)
				goto out;

			skb_reset_network_header(skb);
			dev_queue_xmit(skb);
		}

		if (notify_open && !mrp->ring_role_offloaded)
			br_mrp_ring_port_open(p->dev, true);
	}

out:
	rcu_read_unlock();

	queue_delayed_work(system_wq, &mrp->test_work,
			   usecs_to_jiffies(mrp->test_interval));
}

 
static void br_mrp_in_test_work_expired(struct work_struct *work)
{
	struct delayed_work *del_work = to_delayed_work(work);
	struct br_mrp *mrp = container_of(del_work, struct br_mrp, in_test_work);
	struct net_bridge_port *p;
	bool notify_open = false;
	struct sk_buff *skb;

	if (time_before_eq(mrp->in_test_end, jiffies))
		return;

	if (mrp->in_test_count_miss < mrp->in_test_max_miss) {
		mrp->in_test_count_miss++;
	} else {
		 
		if (mrp->in_state == BR_MRP_IN_STATE_CLOSED)
			notify_open = true;
	}

	rcu_read_lock();

	p = rcu_dereference(mrp->p_port);
	if (p) {
		skb = br_mrp_alloc_in_test_skb(mrp, p,
					       BR_MRP_PORT_ROLE_PRIMARY);
		if (!skb)
			goto out;

		skb_reset_network_header(skb);
		dev_queue_xmit(skb);

		if (notify_open && !mrp->in_role_offloaded)
			br_mrp_in_port_open(p->dev, true);
	}

	p = rcu_dereference(mrp->s_port);
	if (p) {
		skb = br_mrp_alloc_in_test_skb(mrp, p,
					       BR_MRP_PORT_ROLE_SECONDARY);
		if (!skb)
			goto out;

		skb_reset_network_header(skb);
		dev_queue_xmit(skb);

		if (notify_open && !mrp->in_role_offloaded)
			br_mrp_in_port_open(p->dev, true);
	}

	p = rcu_dereference(mrp->i_port);
	if (p) {
		skb = br_mrp_alloc_in_test_skb(mrp, p,
					       BR_MRP_PORT_ROLE_INTER);
		if (!skb)
			goto out;

		skb_reset_network_header(skb);
		dev_queue_xmit(skb);

		if (notify_open && !mrp->in_role_offloaded)
			br_mrp_in_port_open(p->dev, true);
	}

out:
	rcu_read_unlock();

	queue_delayed_work(system_wq, &mrp->in_test_work,
			   usecs_to_jiffies(mrp->in_test_interval));
}

 
static void br_mrp_del_impl(struct net_bridge *br, struct br_mrp *mrp)
{
	struct net_bridge_port *p;
	u8 state;

	 
	cancel_delayed_work_sync(&mrp->test_work);
	br_mrp_switchdev_send_ring_test(br, mrp, 0, 0, 0, 0);

	 
	cancel_delayed_work_sync(&mrp->in_test_work);
	br_mrp_switchdev_send_in_test(br, mrp, 0, 0, 0);

	 
	br_mrp_switchdev_set_ring_role(br, mrp, BR_MRP_RING_ROLE_DISABLED);
	p = rtnl_dereference(mrp->i_port);
	if (p)
		br_mrp_switchdev_set_in_role(br, mrp, mrp->in_id, mrp->ring_id,
					     BR_MRP_IN_ROLE_DISABLED);

	br_mrp_switchdev_del(br, mrp);

	 
	p = rtnl_dereference(mrp->p_port);
	if (p) {
		spin_lock_bh(&br->lock);
		state = netif_running(br->dev) ?
				BR_STATE_FORWARDING : BR_STATE_DISABLED;
		p->state = state;
		p->flags &= ~BR_MRP_AWARE;
		spin_unlock_bh(&br->lock);
		br_mrp_port_switchdev_set_state(p, state);
		rcu_assign_pointer(mrp->p_port, NULL);
	}

	p = rtnl_dereference(mrp->s_port);
	if (p) {
		spin_lock_bh(&br->lock);
		state = netif_running(br->dev) ?
				BR_STATE_FORWARDING : BR_STATE_DISABLED;
		p->state = state;
		p->flags &= ~BR_MRP_AWARE;
		spin_unlock_bh(&br->lock);
		br_mrp_port_switchdev_set_state(p, state);
		rcu_assign_pointer(mrp->s_port, NULL);
	}

	p = rtnl_dereference(mrp->i_port);
	if (p) {
		spin_lock_bh(&br->lock);
		state = netif_running(br->dev) ?
				BR_STATE_FORWARDING : BR_STATE_DISABLED;
		p->state = state;
		p->flags &= ~BR_MRP_AWARE;
		spin_unlock_bh(&br->lock);
		br_mrp_port_switchdev_set_state(p, state);
		rcu_assign_pointer(mrp->i_port, NULL);
	}

	hlist_del_rcu(&mrp->list);
	kfree_rcu(mrp, rcu);

	if (hlist_empty(&br->mrp_list))
		br_del_frame(br, &mrp_frame_type);
}

 
int br_mrp_add(struct net_bridge *br, struct br_mrp_instance *instance)
{
	struct net_bridge_port *p;
	struct br_mrp *mrp;
	int err;

	 
	mrp = br_mrp_find_id(br, instance->ring_id);
	if (mrp)
		return -EINVAL;

	if (!br_mrp_get_port(br, instance->p_ifindex) ||
	    !br_mrp_get_port(br, instance->s_ifindex))
		return -EINVAL;

	 
	if (!br_mrp_unique_ifindex(br, instance->p_ifindex) ||
	    !br_mrp_unique_ifindex(br, instance->s_ifindex))
		return -EINVAL;

	mrp = kzalloc(sizeof(*mrp), GFP_KERNEL);
	if (!mrp)
		return -ENOMEM;

	mrp->ring_id = instance->ring_id;
	mrp->prio = instance->prio;

	p = br_mrp_get_port(br, instance->p_ifindex);
	spin_lock_bh(&br->lock);
	p->state = BR_STATE_FORWARDING;
	p->flags |= BR_MRP_AWARE;
	spin_unlock_bh(&br->lock);
	rcu_assign_pointer(mrp->p_port, p);

	p = br_mrp_get_port(br, instance->s_ifindex);
	spin_lock_bh(&br->lock);
	p->state = BR_STATE_FORWARDING;
	p->flags |= BR_MRP_AWARE;
	spin_unlock_bh(&br->lock);
	rcu_assign_pointer(mrp->s_port, p);

	if (hlist_empty(&br->mrp_list))
		br_add_frame(br, &mrp_frame_type);

	INIT_DELAYED_WORK(&mrp->test_work, br_mrp_test_work_expired);
	INIT_DELAYED_WORK(&mrp->in_test_work, br_mrp_in_test_work_expired);
	hlist_add_tail_rcu(&mrp->list, &br->mrp_list);

	err = br_mrp_switchdev_add(br, mrp);
	if (err)
		goto delete_mrp;

	return 0;

delete_mrp:
	br_mrp_del_impl(br, mrp);

	return err;
}

 
void br_mrp_port_del(struct net_bridge *br, struct net_bridge_port *p)
{
	struct br_mrp *mrp = br_mrp_find_port(br, p);

	 
	if (!mrp)
		return;

	br_mrp_del_impl(br, mrp);
}

 
int br_mrp_del(struct net_bridge *br, struct br_mrp_instance *instance)
{
	struct br_mrp *mrp = br_mrp_find_id(br, instance->ring_id);

	if (!mrp)
		return -EINVAL;

	br_mrp_del_impl(br, mrp);

	return 0;
}

 
int br_mrp_set_port_state(struct net_bridge_port *p,
			  enum br_mrp_port_state_type state)
{
	u32 port_state;

	if (!p || !(p->flags & BR_MRP_AWARE))
		return -EINVAL;

	spin_lock_bh(&p->br->lock);

	if (state == BR_MRP_PORT_STATE_FORWARDING)
		port_state = BR_STATE_FORWARDING;
	else
		port_state = BR_STATE_BLOCKING;

	p->state = port_state;
	spin_unlock_bh(&p->br->lock);

	br_mrp_port_switchdev_set_state(p, port_state);

	return 0;
}

 
int br_mrp_set_port_role(struct net_bridge_port *p,
			 enum br_mrp_port_role_type role)
{
	struct br_mrp *mrp;

	if (!p || !(p->flags & BR_MRP_AWARE))
		return -EINVAL;

	mrp = br_mrp_find_port(p->br, p);

	if (!mrp)
		return -EINVAL;

	switch (role) {
	case BR_MRP_PORT_ROLE_PRIMARY:
		rcu_assign_pointer(mrp->p_port, p);
		break;
	case BR_MRP_PORT_ROLE_SECONDARY:
		rcu_assign_pointer(mrp->s_port, p);
		break;
	default:
		return -EINVAL;
	}

	br_mrp_port_switchdev_set_role(p, role);

	return 0;
}

 
int br_mrp_set_ring_state(struct net_bridge *br,
			  struct br_mrp_ring_state *state)
{
	struct br_mrp *mrp = br_mrp_find_id(br, state->ring_id);

	if (!mrp)
		return -EINVAL;

	if (mrp->ring_state != state->ring_state)
		mrp->ring_transitions++;

	mrp->ring_state = state->ring_state;

	br_mrp_switchdev_set_ring_state(br, mrp, state->ring_state);

	return 0;
}

 
int br_mrp_set_ring_role(struct net_bridge *br,
			 struct br_mrp_ring_role *role)
{
	struct br_mrp *mrp = br_mrp_find_id(br, role->ring_id);
	enum br_mrp_hw_support support;

	if (!mrp)
		return -EINVAL;

	mrp->ring_role = role->ring_role;

	 
	support = br_mrp_switchdev_set_ring_role(br, mrp, role->ring_role);
	if (support == BR_MRP_NONE)
		return -EOPNOTSUPP;

	 
	mrp->ring_role_offloaded = support == BR_MRP_SW ? 0 : 1;

	return 0;
}

 
int br_mrp_start_test(struct net_bridge *br,
		      struct br_mrp_start_test *test)
{
	struct br_mrp *mrp = br_mrp_find_id(br, test->ring_id);
	enum br_mrp_hw_support support;

	if (!mrp)
		return -EINVAL;

	 
	support = br_mrp_switchdev_send_ring_test(br, mrp, test->interval,
						  test->max_miss, test->period,
						  test->monitor);
	if (support == BR_MRP_NONE)
		return -EOPNOTSUPP;

	if (support == BR_MRP_HW)
		return 0;

	mrp->test_interval = test->interval;
	mrp->test_end = jiffies + usecs_to_jiffies(test->period);
	mrp->test_max_miss = test->max_miss;
	mrp->test_monitor = test->monitor;
	mrp->test_count_miss = 0;
	queue_delayed_work(system_wq, &mrp->test_work,
			   usecs_to_jiffies(test->interval));

	return 0;
}

 
int br_mrp_set_in_state(struct net_bridge *br, struct br_mrp_in_state *state)
{
	struct br_mrp *mrp = br_mrp_find_in_id(br, state->in_id);

	if (!mrp)
		return -EINVAL;

	if (mrp->in_state != state->in_state)
		mrp->in_transitions++;

	mrp->in_state = state->in_state;

	br_mrp_switchdev_set_in_state(br, mrp, state->in_state);

	return 0;
}

 
int br_mrp_set_in_role(struct net_bridge *br, struct br_mrp_in_role *role)
{
	struct br_mrp *mrp = br_mrp_find_id(br, role->ring_id);
	enum br_mrp_hw_support support;
	struct net_bridge_port *p;

	if (!mrp)
		return -EINVAL;

	if (!br_mrp_get_port(br, role->i_ifindex))
		return -EINVAL;

	if (role->in_role == BR_MRP_IN_ROLE_DISABLED) {
		u8 state;

		 
		p = rtnl_dereference(mrp->i_port);
		if (!p)
			return -EINVAL;

		 
		cancel_delayed_work_sync(&mrp->in_test_work);
		br_mrp_switchdev_send_in_test(br, mrp, 0, 0, 0);

		 
		spin_lock_bh(&br->lock);
		state = netif_running(br->dev) ?
				BR_STATE_FORWARDING : BR_STATE_DISABLED;
		p->state = state;
		p->flags &= ~BR_MRP_AWARE;
		spin_unlock_bh(&br->lock);
		br_mrp_port_switchdev_set_state(p, state);
		rcu_assign_pointer(mrp->i_port, NULL);

		mrp->in_role = role->in_role;
		mrp->in_id = 0;

		return 0;
	}

	 
	if (!br_mrp_unique_ifindex(br, role->i_ifindex))
		return -EINVAL;

	 
	if (rcu_access_pointer(mrp->i_port))
		return -EINVAL;

	p = br_mrp_get_port(br, role->i_ifindex);
	spin_lock_bh(&br->lock);
	p->state = BR_STATE_FORWARDING;
	p->flags |= BR_MRP_AWARE;
	spin_unlock_bh(&br->lock);
	rcu_assign_pointer(mrp->i_port, p);

	mrp->in_role = role->in_role;
	mrp->in_id = role->in_id;

	 
	support = br_mrp_switchdev_set_in_role(br, mrp, role->in_id,
					       role->ring_id, role->in_role);
	if (support == BR_MRP_NONE)
		return -EOPNOTSUPP;

	 
	mrp->in_role_offloaded = support == BR_MRP_SW ? 0 : 1;

	return 0;
}

 
int br_mrp_start_in_test(struct net_bridge *br,
			 struct br_mrp_start_in_test *in_test)
{
	struct br_mrp *mrp = br_mrp_find_in_id(br, in_test->in_id);
	enum br_mrp_hw_support support;

	if (!mrp)
		return -EINVAL;

	if (mrp->in_role != BR_MRP_IN_ROLE_MIM)
		return -EINVAL;

	 
	support =  br_mrp_switchdev_send_in_test(br, mrp, in_test->interval,
						 in_test->max_miss,
						 in_test->period);
	if (support == BR_MRP_NONE)
		return -EOPNOTSUPP;

	if (support == BR_MRP_HW)
		return 0;

	mrp->in_test_interval = in_test->interval;
	mrp->in_test_end = jiffies + usecs_to_jiffies(in_test->period);
	mrp->in_test_max_miss = in_test->max_miss;
	mrp->in_test_count_miss = 0;
	queue_delayed_work(system_wq, &mrp->in_test_work,
			   usecs_to_jiffies(in_test->interval));

	return 0;
}

 
static bool br_mrp_ring_frame(struct sk_buff *skb)
{
	const struct br_mrp_tlv_hdr *hdr;
	struct br_mrp_tlv_hdr _hdr;

	hdr = skb_header_pointer(skb, sizeof(uint16_t), sizeof(_hdr), &_hdr);
	if (!hdr)
		return false;

	if (hdr->type == BR_MRP_TLV_HEADER_RING_TEST ||
	    hdr->type == BR_MRP_TLV_HEADER_RING_TOPO ||
	    hdr->type == BR_MRP_TLV_HEADER_RING_LINK_DOWN ||
	    hdr->type == BR_MRP_TLV_HEADER_RING_LINK_UP ||
	    hdr->type == BR_MRP_TLV_HEADER_OPTION)
		return true;

	return false;
}

 
static bool br_mrp_in_frame(struct sk_buff *skb)
{
	const struct br_mrp_tlv_hdr *hdr;
	struct br_mrp_tlv_hdr _hdr;

	hdr = skb_header_pointer(skb, sizeof(uint16_t), sizeof(_hdr), &_hdr);
	if (!hdr)
		return false;

	if (hdr->type == BR_MRP_TLV_HEADER_IN_TEST ||
	    hdr->type == BR_MRP_TLV_HEADER_IN_TOPO ||
	    hdr->type == BR_MRP_TLV_HEADER_IN_LINK_DOWN ||
	    hdr->type == BR_MRP_TLV_HEADER_IN_LINK_UP ||
	    hdr->type == BR_MRP_TLV_HEADER_IN_LINK_STATUS)
		return true;

	return false;
}

 
static void br_mrp_mrm_process(struct br_mrp *mrp, struct net_bridge_port *port,
			       struct sk_buff *skb)
{
	const struct br_mrp_tlv_hdr *hdr;
	struct br_mrp_tlv_hdr _hdr;

	 
	hdr = skb_header_pointer(skb, sizeof(uint16_t), sizeof(_hdr), &_hdr);
	if (!hdr)
		return;

	if (hdr->type != BR_MRP_TLV_HEADER_RING_TEST)
		return;

	mrp->test_count_miss = 0;

	 
	if (mrp->ring_state != BR_MRP_RING_STATE_CLOSED)
		br_mrp_ring_port_open(port->dev, false);
}

 
static bool br_mrp_test_better_than_own(struct br_mrp *mrp,
					struct net_bridge *br,
					const struct br_mrp_ring_test_hdr *hdr)
{
	u16 prio = be16_to_cpu(hdr->prio);

	if (prio < mrp->prio ||
	    (prio == mrp->prio &&
	    ether_addr_to_u64(hdr->sa) < ether_addr_to_u64(br->dev->dev_addr)))
		return true;

	return false;
}

 
static void br_mrp_mra_process(struct br_mrp *mrp, struct net_bridge *br,
			       struct net_bridge_port *port,
			       struct sk_buff *skb)
{
	const struct br_mrp_ring_test_hdr *test_hdr;
	struct br_mrp_ring_test_hdr _test_hdr;
	const struct br_mrp_tlv_hdr *hdr;
	struct br_mrp_tlv_hdr _hdr;

	 
	hdr = skb_header_pointer(skb, sizeof(uint16_t), sizeof(_hdr), &_hdr);
	if (!hdr)
		return;

	if (hdr->type != BR_MRP_TLV_HEADER_RING_TEST)
		return;

	test_hdr = skb_header_pointer(skb, sizeof(uint16_t) + sizeof(_hdr),
				      sizeof(_test_hdr), &_test_hdr);
	if (!test_hdr)
		return;

	 
	if (br_mrp_test_better_than_own(mrp, br, test_hdr))
		mrp->test_count_miss = 0;
}

 
static bool br_mrp_mim_process(struct br_mrp *mrp, struct net_bridge_port *port,
			       struct sk_buff *skb)
{
	const struct br_mrp_in_test_hdr *in_hdr;
	struct br_mrp_in_test_hdr _in_hdr;
	const struct br_mrp_tlv_hdr *hdr;
	struct br_mrp_tlv_hdr _hdr;

	 
	hdr = skb_header_pointer(skb, sizeof(uint16_t), sizeof(_hdr), &_hdr);
	if (!hdr)
		return false;

	 
	in_hdr = skb_header_pointer(skb, sizeof(uint16_t) + sizeof(_hdr),
				    sizeof(_in_hdr), &_in_hdr);
	if (!in_hdr)
		return false;

	 
	if (mrp->in_id != ntohs(in_hdr->id))
		return false;

	mrp->in_test_count_miss = 0;

	 
	if (mrp->in_state != BR_MRP_IN_STATE_CLOSED)
		br_mrp_in_port_open(port->dev, false);

	return true;
}

 
static u8 br_mrp_get_frame_type(struct sk_buff *skb)
{
	const struct br_mrp_tlv_hdr *hdr;
	struct br_mrp_tlv_hdr _hdr;

	 
	hdr = skb_header_pointer(skb, sizeof(uint16_t), sizeof(_hdr), &_hdr);
	if (!hdr)
		return 0xff;

	return hdr->type;
}

static bool br_mrp_mrm_behaviour(struct br_mrp *mrp)
{
	if (mrp->ring_role == BR_MRP_RING_ROLE_MRM ||
	    (mrp->ring_role == BR_MRP_RING_ROLE_MRA && !mrp->test_monitor))
		return true;

	return false;
}

static bool br_mrp_mrc_behaviour(struct br_mrp *mrp)
{
	if (mrp->ring_role == BR_MRP_RING_ROLE_MRC ||
	    (mrp->ring_role == BR_MRP_RING_ROLE_MRA && mrp->test_monitor))
		return true;

	return false;
}

 
static int br_mrp_rcv(struct net_bridge_port *p,
		      struct sk_buff *skb, struct net_device *dev)
{
	struct net_bridge_port *p_port, *s_port, *i_port = NULL;
	struct net_bridge_port *p_dst, *s_dst, *i_dst = NULL;
	struct net_bridge *br;
	struct br_mrp *mrp;

	 
	if (p->state == BR_STATE_DISABLED)
		return 0;

	br = p->br;
	mrp =  br_mrp_find_port(br, p);
	if (unlikely(!mrp))
		return 0;

	p_port = rcu_dereference(mrp->p_port);
	if (!p_port)
		return 0;
	p_dst = p_port;

	s_port = rcu_dereference(mrp->s_port);
	if (!s_port)
		return 0;
	s_dst = s_port;

	 
	if (br_mrp_ring_frame(skb)) {
		 
		if (mrp->ring_role == BR_MRP_RING_ROLE_MRM) {
			br_mrp_mrm_process(mrp, p, skb);
			goto no_forward;
		}

		 
		if (mrp->ring_role == BR_MRP_RING_ROLE_MRA) {
			if (!mrp->test_monitor) {
				br_mrp_mrm_process(mrp, p, skb);
				goto no_forward;
			}

			br_mrp_mra_process(mrp, br, p, skb);
		}

		goto forward;
	}

	if (br_mrp_in_frame(skb)) {
		u8 in_type = br_mrp_get_frame_type(skb);

		i_port = rcu_dereference(mrp->i_port);
		i_dst = i_port;

		 
		if (br_mrp_is_ring_port(p_port, s_port, p) &&
		    p->state == BR_STATE_BLOCKING &&
		    in_type == BR_MRP_TLV_HEADER_IN_TEST)
			goto no_forward;

		 
		if (br_mrp_mrm_behaviour(mrp) &&
		    br_mrp_is_ring_port(p_port, s_port, p) &&
		    (s_port->state != BR_STATE_FORWARDING ||
		     p_port->state != BR_STATE_FORWARDING)) {
			p_dst = NULL;
			s_dst = NULL;
		}

		 
		if (br_mrp_mrc_behaviour(mrp) &&
		    mrp->in_role == BR_MRP_IN_ROLE_DISABLED)
			goto forward;

		if (mrp->in_role == BR_MRP_IN_ROLE_MIM) {
			if (in_type == BR_MRP_TLV_HEADER_IN_TEST) {
				 
				if (br_mrp_mim_process(mrp, p, skb)) {
					goto no_forward;
				} else {
					if (br_mrp_is_ring_port(p_port, s_port,
								p))
						i_dst = NULL;

					if (br_mrp_is_in_port(i_port, p))
						goto no_forward;
				}
			} else {
				 
				if (br_mrp_is_ring_port(p_port, s_port, p))
					i_dst = NULL;

				if (br_mrp_is_in_port(i_port, p))
					goto no_forward;
			}
		}

		if (mrp->in_role == BR_MRP_IN_ROLE_MIC) {
			 
			if (in_type == BR_MRP_TLV_HEADER_IN_TEST)
				goto forward;

			 
			if (br_mrp_is_ring_port(p_port, s_port, p) &&
			    (in_type == BR_MRP_TLV_HEADER_IN_LINK_UP ||
			     in_type == BR_MRP_TLV_HEADER_IN_LINK_DOWN))
				goto forward;

			 
			if (br_mrp_is_ring_port(p_port, s_port, p) &&
			    in_type == BR_MRP_TLV_HEADER_IN_LINK_STATUS) {
				p_dst = NULL;
				s_dst = NULL;
			}

			 
			if (in_type == BR_MRP_TLV_HEADER_IN_TOPO) {
				i_dst = NULL;
				goto forward;
			}

			 
			goto no_forward;
		}
	}

forward:
	if (p_dst)
		br_forward(p_dst, skb, true, false);
	if (s_dst)
		br_forward(s_dst, skb, true, false);
	if (i_dst)
		br_forward(i_dst, skb, true, false);

no_forward:
	return 1;
}

 
static int br_mrp_process(struct net_bridge_port *p, struct sk_buff *skb)
{
	 
	if (likely(!(p->flags & BR_MRP_AWARE)))
		goto out;

	return br_mrp_rcv(p, skb, p->dev);
out:
	return 0;
}

bool br_mrp_enabled(struct net_bridge *br)
{
	return !hlist_empty(&br->mrp_list);
}


 

#include "ice.h"

 
static bool ice_is_arfs_active(struct ice_vsi *vsi)
{
	return !!vsi->arfs_fltr_list;
}

 
bool
ice_is_arfs_using_perfect_flow(struct ice_hw *hw, enum ice_fltr_ptype flow_type)
{
	struct ice_arfs_active_fltr_cntrs *arfs_fltr_cntrs;
	struct ice_pf *pf = hw->back;
	struct ice_vsi *vsi;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return false;

	arfs_fltr_cntrs = vsi->arfs_fltr_cntrs;

	 
	smp_mb__before_atomic();
	switch (flow_type) {
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		return atomic_read(&arfs_fltr_cntrs->active_udpv4_cnt) > 0;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		return atomic_read(&arfs_fltr_cntrs->active_udpv6_cnt) > 0;
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		return atomic_read(&arfs_fltr_cntrs->active_tcpv4_cnt) > 0;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		return atomic_read(&arfs_fltr_cntrs->active_tcpv6_cnt) > 0;
	default:
		return false;
	}
}

 
static void
ice_arfs_update_active_fltr_cntrs(struct ice_vsi *vsi,
				  struct ice_arfs_entry *entry, bool add)
{
	struct ice_arfs_active_fltr_cntrs *fltr_cntrs = vsi->arfs_fltr_cntrs;

	switch (entry->fltr_info.flow_type) {
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		if (add)
			atomic_inc(&fltr_cntrs->active_tcpv4_cnt);
		else
			atomic_dec(&fltr_cntrs->active_tcpv4_cnt);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		if (add)
			atomic_inc(&fltr_cntrs->active_tcpv6_cnt);
		else
			atomic_dec(&fltr_cntrs->active_tcpv6_cnt);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		if (add)
			atomic_inc(&fltr_cntrs->active_udpv4_cnt);
		else
			atomic_dec(&fltr_cntrs->active_udpv4_cnt);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		if (add)
			atomic_inc(&fltr_cntrs->active_udpv6_cnt);
		else
			atomic_dec(&fltr_cntrs->active_udpv6_cnt);
		break;
	default:
		dev_err(ice_pf_to_dev(vsi->back), "aRFS: Failed to update filter counters, invalid filter type %d\n",
			entry->fltr_info.flow_type);
	}
}

 
static void
ice_arfs_del_flow_rules(struct ice_vsi *vsi, struct hlist_head *del_list_head)
{
	struct ice_arfs_entry *e;
	struct hlist_node *n;
	struct device *dev;

	dev = ice_pf_to_dev(vsi->back);

	hlist_for_each_entry_safe(e, n, del_list_head, list_entry) {
		int result;

		result = ice_fdir_write_fltr(vsi->back, &e->fltr_info, false,
					     false);
		if (!result)
			ice_arfs_update_active_fltr_cntrs(vsi, e, false);
		else
			dev_dbg(dev, "Unable to delete aRFS entry, err %d fltr_state %d fltr_id %d flow_id %d Q %d\n",
				result, e->fltr_state, e->fltr_info.fltr_id,
				e->flow_id, e->fltr_info.q_index);

		 
		hlist_del(&e->list_entry);
		devm_kfree(dev, e);
	}
}

 
static void
ice_arfs_add_flow_rules(struct ice_vsi *vsi, struct hlist_head *add_list_head)
{
	struct ice_arfs_entry_ptr *ep;
	struct hlist_node *n;
	struct device *dev;

	dev = ice_pf_to_dev(vsi->back);

	hlist_for_each_entry_safe(ep, n, add_list_head, list_entry) {
		int result;

		result = ice_fdir_write_fltr(vsi->back,
					     &ep->arfs_entry->fltr_info, true,
					     false);
		if (!result)
			ice_arfs_update_active_fltr_cntrs(vsi, ep->arfs_entry,
							  true);
		else
			dev_dbg(dev, "Unable to add aRFS entry, err %d fltr_state %d fltr_id %d flow_id %d Q %d\n",
				result, ep->arfs_entry->fltr_state,
				ep->arfs_entry->fltr_info.fltr_id,
				ep->arfs_entry->flow_id,
				ep->arfs_entry->fltr_info.q_index);

		hlist_del(&ep->list_entry);
		devm_kfree(dev, ep);
	}
}

 
static bool
ice_arfs_is_flow_expired(struct ice_vsi *vsi, struct ice_arfs_entry *arfs_entry)
{
#define ICE_ARFS_TIME_DELTA_EXPIRATION	msecs_to_jiffies(5000)
	if (rps_may_expire_flow(vsi->netdev, arfs_entry->fltr_info.q_index,
				arfs_entry->flow_id,
				arfs_entry->fltr_info.fltr_id))
		return true;

	 
	if (arfs_entry->fltr_info.flow_type != ICE_FLTR_PTYPE_NONF_IPV4_UDP &&
	    arfs_entry->fltr_info.flow_type != ICE_FLTR_PTYPE_NONF_IPV6_UDP)
		return false;

	return time_in_range64(arfs_entry->time_activated +
			       ICE_ARFS_TIME_DELTA_EXPIRATION,
			       arfs_entry->time_activated, get_jiffies_64());
}

 
static void
ice_arfs_update_flow_rules(struct ice_vsi *vsi, u16 idx,
			   struct hlist_head *add_list,
			   struct hlist_head *del_list)
{
	struct ice_arfs_entry *e;
	struct hlist_node *n;
	struct device *dev;

	dev = ice_pf_to_dev(vsi->back);

	 
	hlist_for_each_entry_safe(e, n, &vsi->arfs_fltr_list[idx], list_entry)
		 
		if (e->fltr_state == ICE_ARFS_INACTIVE) {
			enum ice_fltr_ptype flow_type = e->fltr_info.flow_type;
			struct ice_arfs_entry_ptr *ep =
				devm_kzalloc(dev, sizeof(*ep), GFP_ATOMIC);

			if (!ep)
				continue;
			INIT_HLIST_NODE(&ep->list_entry);
			 
			ep->arfs_entry = e;
			hlist_add_head(&ep->list_entry, add_list);
			e->fltr_state = ICE_ARFS_ACTIVE;
			 
			if (flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP ||
			    flow_type == ICE_FLTR_PTYPE_NONF_IPV6_UDP)
				e->time_activated = get_jiffies_64();
		} else if (e->fltr_state == ICE_ARFS_ACTIVE) {
			 
			if (ice_arfs_is_flow_expired(vsi, e)) {
				 
				hlist_del(&e->list_entry);
				e->fltr_state = ICE_ARFS_TODEL;
				 
				hlist_add_head(&e->list_entry, del_list);
			}
		}
}

 
void ice_sync_arfs_fltrs(struct ice_pf *pf)
{
	HLIST_HEAD(tmp_del_list);
	HLIST_HEAD(tmp_add_list);
	struct ice_vsi *pf_vsi;
	unsigned int i;

	pf_vsi = ice_get_main_vsi(pf);
	if (!pf_vsi)
		return;

	if (!ice_is_arfs_active(pf_vsi))
		return;

	spin_lock_bh(&pf_vsi->arfs_lock);
	 
	for (i = 0; i < ICE_MAX_ARFS_LIST; i++)
		ice_arfs_update_flow_rules(pf_vsi, i, &tmp_add_list,
					   &tmp_del_list);
	spin_unlock_bh(&pf_vsi->arfs_lock);

	 
	ice_arfs_del_flow_rules(pf_vsi, &tmp_del_list);

	 
	ice_arfs_add_flow_rules(pf_vsi, &tmp_add_list);
}

 
static struct ice_arfs_entry *
ice_arfs_build_entry(struct ice_vsi *vsi, const struct flow_keys *fk,
		     u16 rxq_idx, u32 flow_id)
{
	struct ice_arfs_entry *arfs_entry;
	struct ice_fdir_fltr *fltr_info;
	u8 ip_proto;

	arfs_entry = devm_kzalloc(ice_pf_to_dev(vsi->back),
				  sizeof(*arfs_entry),
				  GFP_ATOMIC | __GFP_NOWARN);
	if (!arfs_entry)
		return NULL;

	fltr_info = &arfs_entry->fltr_info;
	fltr_info->q_index = rxq_idx;
	fltr_info->dest_ctl = ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QINDEX;
	fltr_info->dest_vsi = vsi->idx;
	ip_proto = fk->basic.ip_proto;

	if (fk->basic.n_proto == htons(ETH_P_IP)) {
		fltr_info->ip.v4.proto = ip_proto;
		fltr_info->flow_type = (ip_proto == IPPROTO_TCP) ?
			ICE_FLTR_PTYPE_NONF_IPV4_TCP :
			ICE_FLTR_PTYPE_NONF_IPV4_UDP;
		fltr_info->ip.v4.src_ip = fk->addrs.v4addrs.src;
		fltr_info->ip.v4.dst_ip = fk->addrs.v4addrs.dst;
		fltr_info->ip.v4.src_port = fk->ports.src;
		fltr_info->ip.v4.dst_port = fk->ports.dst;
	} else {  
		fltr_info->ip.v6.proto = ip_proto;
		fltr_info->flow_type = (ip_proto == IPPROTO_TCP) ?
			ICE_FLTR_PTYPE_NONF_IPV6_TCP :
			ICE_FLTR_PTYPE_NONF_IPV6_UDP;
		memcpy(&fltr_info->ip.v6.src_ip, &fk->addrs.v6addrs.src,
		       sizeof(struct in6_addr));
		memcpy(&fltr_info->ip.v6.dst_ip, &fk->addrs.v6addrs.dst,
		       sizeof(struct in6_addr));
		fltr_info->ip.v6.src_port = fk->ports.src;
		fltr_info->ip.v6.dst_port = fk->ports.dst;
	}

	arfs_entry->flow_id = flow_id;
	fltr_info->fltr_id =
		atomic_inc_return(vsi->arfs_last_fltr_id) % RPS_NO_FILTER;

	return arfs_entry;
}

 
static bool
ice_arfs_is_perfect_flow_set(struct ice_hw *hw, __be16 l3_proto, u8 l4_proto)
{
	unsigned long *perfect_fltr = hw->fdir_perfect_fltr;

	 
	if (!perfect_fltr)
		return true;

	if (l3_proto == htons(ETH_P_IP) && l4_proto == IPPROTO_UDP)
		return test_bit(ICE_FLTR_PTYPE_NONF_IPV4_UDP, perfect_fltr);
	else if (l3_proto == htons(ETH_P_IP) && l4_proto == IPPROTO_TCP)
		return test_bit(ICE_FLTR_PTYPE_NONF_IPV4_TCP, perfect_fltr);
	else if (l3_proto == htons(ETH_P_IPV6) && l4_proto == IPPROTO_UDP)
		return test_bit(ICE_FLTR_PTYPE_NONF_IPV6_UDP, perfect_fltr);
	else if (l3_proto == htons(ETH_P_IPV6) && l4_proto == IPPROTO_TCP)
		return test_bit(ICE_FLTR_PTYPE_NONF_IPV6_TCP, perfect_fltr);

	return false;
}

 
int
ice_rx_flow_steer(struct net_device *netdev, const struct sk_buff *skb,
		  u16 rxq_idx, u32 flow_id)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_arfs_entry *arfs_entry;
	struct ice_vsi *vsi = np->vsi;
	struct flow_keys fk;
	struct ice_pf *pf;
	__be16 n_proto;
	u8 ip_proto;
	u16 idx;
	int ret;

	 
	if (unlikely(!vsi->arfs_fltr_list))
		return -ENODEV;

	pf = vsi->back;

	if (skb->encapsulation)
		return -EPROTONOSUPPORT;

	if (!skb_flow_dissect_flow_keys(skb, &fk, 0))
		return -EPROTONOSUPPORT;

	n_proto = fk.basic.n_proto;
	 
	if ((n_proto == htons(ETH_P_IP) && !ip_is_fragment(ip_hdr(skb))) ||
	    n_proto == htons(ETH_P_IPV6))
		ip_proto = fk.basic.ip_proto;
	else
		return -EPROTONOSUPPORT;

	 
	if (ip_proto != IPPROTO_TCP && ip_proto != IPPROTO_UDP)
		return -EPROTONOSUPPORT;

	 
	if (!ice_arfs_is_perfect_flow_set(&pf->hw, n_proto, ip_proto))
		return -EOPNOTSUPP;

	 
	idx = skb_get_hash_raw(skb) & ICE_ARFS_LST_MASK;
	 
	spin_lock_bh(&vsi->arfs_lock);
	hlist_for_each_entry(arfs_entry, &vsi->arfs_fltr_list[idx],
			     list_entry) {
		struct ice_fdir_fltr *fltr_info;

		 
		if (arfs_entry->flow_id != flow_id)
			continue;

		fltr_info = &arfs_entry->fltr_info;
		ret = fltr_info->fltr_id;

		if (fltr_info->q_index == rxq_idx ||
		    arfs_entry->fltr_state != ICE_ARFS_ACTIVE)
			goto out;

		 
		fltr_info->q_index = rxq_idx;
		arfs_entry->fltr_state = ICE_ARFS_INACTIVE;
		ice_arfs_update_active_fltr_cntrs(vsi, arfs_entry, false);
		goto out_schedule_service_task;
	}

	arfs_entry = ice_arfs_build_entry(vsi, &fk, rxq_idx, flow_id);
	if (!arfs_entry) {
		ret = -ENOMEM;
		goto out;
	}

	ret = arfs_entry->fltr_info.fltr_id;
	INIT_HLIST_NODE(&arfs_entry->list_entry);
	hlist_add_head(&arfs_entry->list_entry, &vsi->arfs_fltr_list[idx]);
out_schedule_service_task:
	ice_service_task_schedule(pf);
out:
	spin_unlock_bh(&vsi->arfs_lock);
	return ret;
}

 
static int ice_init_arfs_cntrs(struct ice_vsi *vsi)
{
	if (!vsi || vsi->type != ICE_VSI_PF)
		return -EINVAL;

	vsi->arfs_fltr_cntrs = kzalloc(sizeof(*vsi->arfs_fltr_cntrs),
				       GFP_KERNEL);
	if (!vsi->arfs_fltr_cntrs)
		return -ENOMEM;

	vsi->arfs_last_fltr_id = kzalloc(sizeof(*vsi->arfs_last_fltr_id),
					 GFP_KERNEL);
	if (!vsi->arfs_last_fltr_id) {
		kfree(vsi->arfs_fltr_cntrs);
		vsi->arfs_fltr_cntrs = NULL;
		return -ENOMEM;
	}

	return 0;
}

 
void ice_init_arfs(struct ice_vsi *vsi)
{
	struct hlist_head *arfs_fltr_list;
	unsigned int i;

	if (!vsi || vsi->type != ICE_VSI_PF)
		return;

	arfs_fltr_list = kcalloc(ICE_MAX_ARFS_LIST, sizeof(*arfs_fltr_list),
				 GFP_KERNEL);
	if (!arfs_fltr_list)
		return;

	if (ice_init_arfs_cntrs(vsi))
		goto free_arfs_fltr_list;

	for (i = 0; i < ICE_MAX_ARFS_LIST; i++)
		INIT_HLIST_HEAD(&arfs_fltr_list[i]);

	spin_lock_init(&vsi->arfs_lock);

	vsi->arfs_fltr_list = arfs_fltr_list;

	return;

free_arfs_fltr_list:
	kfree(arfs_fltr_list);
}

 
void ice_clear_arfs(struct ice_vsi *vsi)
{
	struct device *dev;
	unsigned int i;

	if (!vsi || vsi->type != ICE_VSI_PF || !vsi->back ||
	    !vsi->arfs_fltr_list)
		return;

	dev = ice_pf_to_dev(vsi->back);
	for (i = 0; i < ICE_MAX_ARFS_LIST; i++) {
		struct ice_arfs_entry *r;
		struct hlist_node *n;

		spin_lock_bh(&vsi->arfs_lock);
		hlist_for_each_entry_safe(r, n, &vsi->arfs_fltr_list[i],
					  list_entry) {
			hlist_del(&r->list_entry);
			devm_kfree(dev, r);
		}
		spin_unlock_bh(&vsi->arfs_lock);
	}

	kfree(vsi->arfs_fltr_list);
	vsi->arfs_fltr_list = NULL;
	kfree(vsi->arfs_last_fltr_id);
	vsi->arfs_last_fltr_id = NULL;
	kfree(vsi->arfs_fltr_cntrs);
	vsi->arfs_fltr_cntrs = NULL;
}

 
void ice_free_cpu_rx_rmap(struct ice_vsi *vsi)
{
	struct net_device *netdev;

	if (!vsi || vsi->type != ICE_VSI_PF)
		return;

	netdev = vsi->netdev;
	if (!netdev || !netdev->rx_cpu_rmap)
		return;

	free_irq_cpu_rmap(netdev->rx_cpu_rmap);
	netdev->rx_cpu_rmap = NULL;
}

 
int ice_set_cpu_rx_rmap(struct ice_vsi *vsi)
{
	struct net_device *netdev;
	struct ice_pf *pf;
	int i;

	if (!vsi || vsi->type != ICE_VSI_PF)
		return 0;

	pf = vsi->back;
	netdev = vsi->netdev;
	if (!pf || !netdev || !vsi->num_q_vectors)
		return -EINVAL;

	netdev_dbg(netdev, "Setup CPU RMAP: vsi type 0x%x, ifname %s, q_vectors %d\n",
		   vsi->type, netdev->name, vsi->num_q_vectors);

	netdev->rx_cpu_rmap = alloc_irq_cpu_rmap(vsi->num_q_vectors);
	if (unlikely(!netdev->rx_cpu_rmap))
		return -EINVAL;

	ice_for_each_q_vector(vsi, i)
		if (irq_cpu_rmap_add(netdev->rx_cpu_rmap,
				     vsi->q_vectors[i]->irq.virq)) {
			ice_free_cpu_rx_rmap(vsi);
			return -EINVAL;
		}

	return 0;
}

 
void ice_remove_arfs(struct ice_pf *pf)
{
	struct ice_vsi *pf_vsi;

	pf_vsi = ice_get_main_vsi(pf);
	if (!pf_vsi)
		return;

	ice_clear_arfs(pf_vsi);
}

 
void ice_rebuild_arfs(struct ice_pf *pf)
{
	struct ice_vsi *pf_vsi;

	pf_vsi = ice_get_main_vsi(pf);
	if (!pf_vsi)
		return;

	ice_remove_arfs(pf);
	ice_init_arfs(pf_vsi);
}

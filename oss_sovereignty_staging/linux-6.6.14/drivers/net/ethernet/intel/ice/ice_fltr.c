
 

#include "ice.h"
#include "ice_fltr.h"

 
void ice_fltr_free_list(struct device *dev, struct list_head *h)
{
	struct ice_fltr_list_entry *e, *tmp;

	list_for_each_entry_safe(e, tmp, h, list_entry) {
		list_del(&e->list_entry);
		devm_kfree(dev, e);
	}
}

 
static int
ice_fltr_add_entry_to_list(struct device *dev, struct ice_fltr_info *info,
			   struct list_head *list)
{
	struct ice_fltr_list_entry *entry;

	entry = devm_kzalloc(dev, sizeof(*entry), GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;

	entry->fltr_info = *info;

	INIT_LIST_HEAD(&entry->list_entry);
	list_add(&entry->list_entry, list);

	return 0;
}

 
int
ice_fltr_set_vlan_vsi_promisc(struct ice_hw *hw, struct ice_vsi *vsi,
			      u8 promisc_mask)
{
	struct ice_pf *pf = hw->back;
	int result;

	result = ice_set_vlan_vsi_promisc(hw, vsi->idx, promisc_mask, false);
	if (result && result != -EEXIST)
		dev_err(ice_pf_to_dev(pf),
			"Error setting promisc mode on VSI %i (rc=%d)\n",
			vsi->vsi_num, result);

	return result;
}

 
int
ice_fltr_clear_vlan_vsi_promisc(struct ice_hw *hw, struct ice_vsi *vsi,
				u8 promisc_mask)
{
	struct ice_pf *pf = hw->back;
	int result;

	result = ice_set_vlan_vsi_promisc(hw, vsi->idx, promisc_mask, true);
	if (result && result != -EEXIST)
		dev_err(ice_pf_to_dev(pf),
			"Error clearing promisc mode on VSI %i (rc=%d)\n",
			vsi->vsi_num, result);

	return result;
}

 
int
ice_fltr_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
			   u16 vid)
{
	struct ice_pf *pf = hw->back;
	int result;

	result = ice_clear_vsi_promisc(hw, vsi_handle, promisc_mask, vid);
	if (result && result != -EEXIST)
		dev_err(ice_pf_to_dev(pf),
			"Error clearing promisc mode on VSI %i for VID %u (rc=%d)\n",
			ice_get_hw_vsi_num(hw, vsi_handle), vid, result);

	return result;
}

 
int
ice_fltr_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
			 u16 vid)
{
	struct ice_pf *pf = hw->back;
	int result;

	result = ice_set_vsi_promisc(hw, vsi_handle, promisc_mask, vid);
	if (result && result != -EEXIST)
		dev_err(ice_pf_to_dev(pf),
			"Error setting promisc mode on VSI %i for VID %u (rc=%d)\n",
			ice_get_hw_vsi_num(hw, vsi_handle), vid, result);

	return result;
}

 
int ice_fltr_add_mac_list(struct ice_vsi *vsi, struct list_head *list)
{
	return ice_add_mac(&vsi->back->hw, list);
}

 
int ice_fltr_remove_mac_list(struct ice_vsi *vsi, struct list_head *list)
{
	return ice_remove_mac(&vsi->back->hw, list);
}

 
static int ice_fltr_add_vlan_list(struct ice_vsi *vsi, struct list_head *list)
{
	return ice_add_vlan(&vsi->back->hw, list);
}

 
static int
ice_fltr_remove_vlan_list(struct ice_vsi *vsi, struct list_head *list)
{
	return ice_remove_vlan(&vsi->back->hw, list);
}

 
static int ice_fltr_add_eth_list(struct ice_vsi *vsi, struct list_head *list)
{
	return ice_add_eth_mac(&vsi->back->hw, list);
}

 
static int ice_fltr_remove_eth_list(struct ice_vsi *vsi, struct list_head *list)
{
	return ice_remove_eth_mac(&vsi->back->hw, list);
}

 
void ice_fltr_remove_all(struct ice_vsi *vsi)
{
	ice_remove_vsi_fltr(&vsi->back->hw, vsi->idx);
	 
	if (vsi->netdev) {
		__dev_uc_unsync(vsi->netdev, NULL);
		__dev_mc_unsync(vsi->netdev, NULL);
	}
}

 
int
ice_fltr_add_mac_to_list(struct ice_vsi *vsi, struct list_head *list,
			 const u8 *mac, enum ice_sw_fwd_act_type action)
{
	struct ice_fltr_info info = { 0 };

	info.flag = ICE_FLTR_TX;
	info.src_id = ICE_SRC_ID_VSI;
	info.lkup_type = ICE_SW_LKUP_MAC;
	info.fltr_act = action;
	info.vsi_handle = vsi->idx;

	ether_addr_copy(info.l_data.mac.mac_addr, mac);

	return ice_fltr_add_entry_to_list(ice_pf_to_dev(vsi->back), &info,
					  list);
}

 
static int
ice_fltr_add_vlan_to_list(struct ice_vsi *vsi, struct list_head *list,
			  struct ice_vlan *vlan)
{
	struct ice_fltr_info info = { 0 };

	info.flag = ICE_FLTR_TX;
	info.src_id = ICE_SRC_ID_VSI;
	info.lkup_type = ICE_SW_LKUP_VLAN;
	info.fltr_act = ICE_FWD_TO_VSI;
	info.vsi_handle = vsi->idx;
	info.l_data.vlan.vlan_id = vlan->vid;
	info.l_data.vlan.tpid = vlan->tpid;
	info.l_data.vlan.tpid_valid = true;

	return ice_fltr_add_entry_to_list(ice_pf_to_dev(vsi->back), &info,
					  list);
}

 
static int
ice_fltr_add_eth_to_list(struct ice_vsi *vsi, struct list_head *list,
			 u16 ethertype, u16 flag,
			 enum ice_sw_fwd_act_type action)
{
	struct ice_fltr_info info = { 0 };

	info.flag = flag;
	info.lkup_type = ICE_SW_LKUP_ETHERTYPE;
	info.fltr_act = action;
	info.vsi_handle = vsi->idx;
	info.l_data.ethertype_mac.ethertype = ethertype;

	if (flag == ICE_FLTR_TX)
		info.src_id = ICE_SRC_ID_VSI;
	else
		info.src_id = ICE_SRC_ID_LPORT;

	return ice_fltr_add_entry_to_list(ice_pf_to_dev(vsi->back), &info,
					  list);
}

 
static int
ice_fltr_prepare_mac(struct ice_vsi *vsi, const u8 *mac,
		     enum ice_sw_fwd_act_type action,
		     int (*mac_action)(struct ice_vsi *, struct list_head *))
{
	LIST_HEAD(tmp_list);
	int result;

	if (ice_fltr_add_mac_to_list(vsi, &tmp_list, mac, action)) {
		ice_fltr_free_list(ice_pf_to_dev(vsi->back), &tmp_list);
		return -ENOMEM;
	}

	result = mac_action(vsi, &tmp_list);
	ice_fltr_free_list(ice_pf_to_dev(vsi->back), &tmp_list);
	return result;
}

 
static int
ice_fltr_prepare_mac_and_broadcast(struct ice_vsi *vsi, const u8 *mac,
				   enum ice_sw_fwd_act_type action,
				   int(*mac_action)
				   (struct ice_vsi *, struct list_head *))
{
	u8 broadcast[ETH_ALEN];
	LIST_HEAD(tmp_list);
	int result;

	eth_broadcast_addr(broadcast);
	if (ice_fltr_add_mac_to_list(vsi, &tmp_list, mac, action) ||
	    ice_fltr_add_mac_to_list(vsi, &tmp_list, broadcast, action)) {
		ice_fltr_free_list(ice_pf_to_dev(vsi->back), &tmp_list);
		return -ENOMEM;
	}

	result = mac_action(vsi, &tmp_list);
	ice_fltr_free_list(ice_pf_to_dev(vsi->back), &tmp_list);
	return result;
}

 
static int
ice_fltr_prepare_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan,
		      int (*vlan_action)(struct ice_vsi *, struct list_head *))
{
	LIST_HEAD(tmp_list);
	int result;

	if (ice_fltr_add_vlan_to_list(vsi, &tmp_list, vlan))
		return -ENOMEM;

	result = vlan_action(vsi, &tmp_list);
	ice_fltr_free_list(ice_pf_to_dev(vsi->back), &tmp_list);
	return result;
}

 
static int
ice_fltr_prepare_eth(struct ice_vsi *vsi, u16 ethertype, u16 flag,
		     enum ice_sw_fwd_act_type action,
		     int (*eth_action)(struct ice_vsi *, struct list_head *))
{
	LIST_HEAD(tmp_list);
	int result;

	if (ice_fltr_add_eth_to_list(vsi, &tmp_list, ethertype, flag, action))
		return -ENOMEM;

	result = eth_action(vsi, &tmp_list);
	ice_fltr_free_list(ice_pf_to_dev(vsi->back), &tmp_list);
	return result;
}

 
int ice_fltr_add_mac(struct ice_vsi *vsi, const u8 *mac,
		     enum ice_sw_fwd_act_type action)
{
	return ice_fltr_prepare_mac(vsi, mac, action, ice_fltr_add_mac_list);
}

 
int
ice_fltr_add_mac_and_broadcast(struct ice_vsi *vsi, const u8 *mac,
			       enum ice_sw_fwd_act_type action)
{
	return ice_fltr_prepare_mac_and_broadcast(vsi, mac, action,
						  ice_fltr_add_mac_list);
}

 
int ice_fltr_remove_mac(struct ice_vsi *vsi, const u8 *mac,
			enum ice_sw_fwd_act_type action)
{
	return ice_fltr_prepare_mac(vsi, mac, action, ice_fltr_remove_mac_list);
}

 
int ice_fltr_add_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	return ice_fltr_prepare_vlan(vsi, vlan, ice_fltr_add_vlan_list);
}

 
int ice_fltr_remove_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	return ice_fltr_prepare_vlan(vsi, vlan, ice_fltr_remove_vlan_list);
}

 
int ice_fltr_add_eth(struct ice_vsi *vsi, u16 ethertype, u16 flag,
		     enum ice_sw_fwd_act_type action)
{
	return ice_fltr_prepare_eth(vsi, ethertype, flag, action,
				    ice_fltr_add_eth_list);
}

 
int ice_fltr_remove_eth(struct ice_vsi *vsi, u16 ethertype, u16 flag,
			enum ice_sw_fwd_act_type action)
{
	return ice_fltr_prepare_eth(vsi, ethertype, flag, action,
				    ice_fltr_remove_eth_list);
}

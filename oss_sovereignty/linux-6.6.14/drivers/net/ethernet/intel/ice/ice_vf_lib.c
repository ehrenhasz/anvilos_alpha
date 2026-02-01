
 

#include "ice_vf_lib_private.h"
#include "ice.h"
#include "ice_lib.h"
#include "ice_fltr.h"
#include "ice_virtchnl_allowlist.h"

 

 
struct ice_vf *ice_get_vf_by_id(struct ice_pf *pf, u16 vf_id)
{
	struct ice_vf *vf;

	rcu_read_lock();
	hash_for_each_possible_rcu(pf->vfs.table, vf, entry, vf_id) {
		if (vf->vf_id == vf_id) {
			struct ice_vf *found;

			if (kref_get_unless_zero(&vf->refcnt))
				found = vf;
			else
				found = NULL;

			rcu_read_unlock();
			return found;
		}
	}
	rcu_read_unlock();

	return NULL;
}

 
static void ice_release_vf(struct kref *ref)
{
	struct ice_vf *vf = container_of(ref, struct ice_vf, refcnt);

	vf->vf_ops->free(vf);
}

 
void ice_put_vf(struct ice_vf *vf)
{
	kref_put(&vf->refcnt, ice_release_vf);
}

 
bool ice_has_vfs(struct ice_pf *pf)
{
	 
	return !hash_empty(pf->vfs.table);
}

 
u16 ice_get_num_vfs(struct ice_pf *pf)
{
	struct ice_vf *vf;
	unsigned int bkt;
	u16 num_vfs = 0;

	rcu_read_lock();
	ice_for_each_vf_rcu(pf, bkt, vf)
		num_vfs++;
	rcu_read_unlock();

	return num_vfs;
}

 
struct ice_vsi *ice_get_vf_vsi(struct ice_vf *vf)
{
	if (vf->lan_vsi_idx == ICE_NO_VSI)
		return NULL;

	return vf->pf->vsi[vf->lan_vsi_idx];
}

 
bool ice_is_vf_disabled(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;

	return (test_bit(ICE_VF_DIS, pf->state) ||
		test_bit(ICE_VF_STATE_DIS, vf->vf_states));
}

 
static void ice_wait_on_vf_reset(struct ice_vf *vf)
{
	int i;

	for (i = 0; i < ICE_MAX_VF_RESET_TRIES; i++) {
		if (test_bit(ICE_VF_STATE_INIT, vf->vf_states))
			break;
		msleep(ICE_MAX_VF_RESET_SLEEP_MS);
	}
}

 
int ice_check_vf_ready_for_cfg(struct ice_vf *vf)
{
	ice_wait_on_vf_reset(vf);

	if (ice_is_vf_disabled(vf))
		return -EINVAL;

	if (ice_check_vf_init(vf))
		return -EBUSY;

	return 0;
}

 
static void ice_trigger_vf_reset(struct ice_vf *vf, bool is_vflr, bool is_pfr)
{
	 
	clear_bit(ICE_VF_STATE_ACTIVE, vf->vf_states);

	 
	clear_bit(ICE_VF_STATE_INIT, vf->vf_states);

	 
	if (!is_pfr)
		vf->vf_ops->clear_mbx_register(vf);

	vf->vf_ops->trigger_reset_register(vf, is_vflr);
}

static void ice_vf_clear_counters(struct ice_vf *vf)
{
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);

	if (vsi)
		vsi->num_vlan = 0;

	vf->num_mac = 0;
	memset(&vf->mdd_tx_events, 0, sizeof(vf->mdd_tx_events));
	memset(&vf->mdd_rx_events, 0, sizeof(vf->mdd_rx_events));
}

 
static void ice_vf_pre_vsi_rebuild(struct ice_vf *vf)
{
	 
	if (vf->vf_ops->irq_close)
		vf->vf_ops->irq_close(vf);

	ice_vf_clear_counters(vf);
	vf->vf_ops->clear_reset_trigger(vf);
}

 
static int ice_vf_recreate_vsi(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	int err;

	ice_vf_vsi_release(vf);

	err = vf->vf_ops->create_vsi(vf);
	if (err) {
		dev_err(ice_pf_to_dev(pf),
			"Failed to recreate the VF%u's VSI, error %d\n",
			vf->vf_id, err);
		return err;
	}

	return 0;
}

 
static int ice_vf_rebuild_vsi(struct ice_vf *vf)
{
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);
	struct ice_pf *pf = vf->pf;

	if (WARN_ON(!vsi))
		return -EINVAL;

	if (ice_vsi_rebuild(vsi, ICE_VSI_FLAG_INIT)) {
		dev_err(ice_pf_to_dev(pf), "failed to rebuild VF %d VSI\n",
			vf->vf_id);
		return -EIO;
	}
	 
	vsi->vsi_num = ice_get_hw_vsi_num(&pf->hw, vsi->idx);
	vf->lan_vsi_num = vsi->vsi_num;

	return 0;
}

 
static int ice_vf_rebuild_host_vlan_cfg(struct ice_vf *vf, struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);
	struct device *dev = ice_pf_to_dev(vf->pf);
	int err;

	if (ice_vf_is_port_vlan_ena(vf)) {
		err = vlan_ops->set_port_vlan(vsi, &vf->port_vlan_info);
		if (err) {
			dev_err(dev, "failed to configure port VLAN via VSI parameters for VF %u, error %d\n",
				vf->vf_id, err);
			return err;
		}

		err = vlan_ops->add_vlan(vsi, &vf->port_vlan_info);
	} else {
		err = ice_vsi_add_vlan_zero(vsi);
	}

	if (err) {
		dev_err(dev, "failed to add VLAN %u filter for VF %u during VF rebuild, error %d\n",
			ice_vf_is_port_vlan_ena(vf) ?
			ice_vf_get_port_vlan_id(vf) : 0, vf->vf_id, err);
		return err;
	}

	err = vlan_ops->ena_rx_filtering(vsi);
	if (err)
		dev_warn(dev, "failed to enable Rx VLAN filtering for VF %d VSI %d during VF rebuild, error %d\n",
			 vf->vf_id, vsi->idx, err);

	return 0;
}

 
static int ice_vf_rebuild_host_tx_rate_cfg(struct ice_vf *vf)
{
	struct device *dev = ice_pf_to_dev(vf->pf);
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);
	int err;

	if (WARN_ON(!vsi))
		return -EINVAL;

	if (vf->min_tx_rate) {
		err = ice_set_min_bw_limit(vsi, (u64)vf->min_tx_rate * 1000);
		if (err) {
			dev_err(dev, "failed to set min Tx rate to %d Mbps for VF %u, error %d\n",
				vf->min_tx_rate, vf->vf_id, err);
			return err;
		}
	}

	if (vf->max_tx_rate) {
		err = ice_set_max_bw_limit(vsi, (u64)vf->max_tx_rate * 1000);
		if (err) {
			dev_err(dev, "failed to set max Tx rate to %d Mbps for VF %u, error %d\n",
				vf->max_tx_rate, vf->vf_id, err);
			return err;
		}
	}

	return 0;
}

 
static void ice_vf_set_host_trust_cfg(struct ice_vf *vf)
{
	assign_bit(ICE_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps, vf->trusted);
}

 
static int ice_vf_rebuild_host_mac_cfg(struct ice_vf *vf)
{
	struct device *dev = ice_pf_to_dev(vf->pf);
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);
	u8 broadcast[ETH_ALEN];
	int status;

	if (WARN_ON(!vsi))
		return -EINVAL;

	if (ice_is_eswitch_mode_switchdev(vf->pf))
		return 0;

	eth_broadcast_addr(broadcast);
	status = ice_fltr_add_mac(vsi, broadcast, ICE_FWD_TO_VSI);
	if (status) {
		dev_err(dev, "failed to add broadcast MAC filter for VF %u, error %d\n",
			vf->vf_id, status);
		return status;
	}

	vf->num_mac++;

	if (is_valid_ether_addr(vf->hw_lan_addr)) {
		status = ice_fltr_add_mac(vsi, vf->hw_lan_addr,
					  ICE_FWD_TO_VSI);
		if (status) {
			dev_err(dev, "failed to add default unicast MAC filter %pM for VF %u, error %d\n",
				&vf->hw_lan_addr[0], vf->vf_id,
				status);
			return status;
		}
		vf->num_mac++;

		ether_addr_copy(vf->dev_lan_addr, vf->hw_lan_addr);
	}

	return 0;
}

 
static void ice_vf_rebuild_aggregator_node_cfg(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int status;

	if (!vsi->agg_node)
		return;

	dev = ice_pf_to_dev(pf);
	if (vsi->agg_node->num_vsis == ICE_MAX_VSIS_IN_AGG_NODE) {
		dev_dbg(dev,
			"agg_id %u already has reached max_num_vsis %u\n",
			vsi->agg_node->agg_id, vsi->agg_node->num_vsis);
		return;
	}

	status = ice_move_vsi_to_agg(pf->hw.port_info, vsi->agg_node->agg_id,
				     vsi->idx, vsi->tc_cfg.ena_tc);
	if (status)
		dev_dbg(dev, "unable to move VSI idx %u into aggregator %u node",
			vsi->idx, vsi->agg_node->agg_id);
	else
		vsi->agg_node->num_vsis++;
}

 
static void ice_vf_rebuild_host_cfg(struct ice_vf *vf)
{
	struct device *dev = ice_pf_to_dev(vf->pf);
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);

	if (WARN_ON(!vsi))
		return;

	ice_vf_set_host_trust_cfg(vf);

	if (ice_vf_rebuild_host_mac_cfg(vf))
		dev_err(dev, "failed to rebuild default MAC configuration for VF %d\n",
			vf->vf_id);

	if (ice_vf_rebuild_host_vlan_cfg(vf, vsi))
		dev_err(dev, "failed to rebuild VLAN configuration for VF %u\n",
			vf->vf_id);

	if (ice_vf_rebuild_host_tx_rate_cfg(vf))
		dev_err(dev, "failed to rebuild Tx rate limiting configuration for VF %u\n",
			vf->vf_id);

	if (ice_vsi_apply_spoofchk(vsi, vf->spoofchk))
		dev_err(dev, "failed to rebuild spoofchk configuration for VF %d\n",
			vf->vf_id);

	 
	ice_vf_rebuild_aggregator_node_cfg(vsi);
}

 
static void ice_set_vf_state_qs_dis(struct ice_vf *vf)
{
	 
	bitmap_zero(vf->txq_ena, ICE_MAX_RSS_QS_PER_VF);
	bitmap_zero(vf->rxq_ena, ICE_MAX_RSS_QS_PER_VF);
	clear_bit(ICE_VF_STATE_QS_ENA, vf->vf_states);
}

 
static void ice_vf_set_initialized(struct ice_vf *vf)
{
	ice_set_vf_state_qs_dis(vf);
	clear_bit(ICE_VF_STATE_MC_PROMISC, vf->vf_states);
	clear_bit(ICE_VF_STATE_UC_PROMISC, vf->vf_states);
	clear_bit(ICE_VF_STATE_DIS, vf->vf_states);
	set_bit(ICE_VF_STATE_INIT, vf->vf_states);
	memset(&vf->vlan_v2_caps, 0, sizeof(vf->vlan_v2_caps));
}

 
static void ice_vf_post_vsi_rebuild(struct ice_vf *vf)
{
	ice_vf_rebuild_host_cfg(vf);
	ice_vf_set_initialized(vf);

	vf->vf_ops->post_vsi_rebuild(vf);
}

 
bool ice_is_any_vf_in_unicast_promisc(struct ice_pf *pf)
{
	bool is_vf_promisc = false;
	struct ice_vf *vf;
	unsigned int bkt;

	rcu_read_lock();
	ice_for_each_vf_rcu(pf, bkt, vf) {
		 
		if (test_bit(ICE_VF_STATE_UC_PROMISC, vf->vf_states)) {
			is_vf_promisc = true;
			break;
		}
	}
	rcu_read_unlock();

	return is_vf_promisc;
}

 
void
ice_vf_get_promisc_masks(struct ice_vf *vf, struct ice_vsi *vsi,
			 u8 *ucast_m, u8 *mcast_m)
{
	if (ice_vf_is_port_vlan_ena(vf) ||
	    ice_vsi_has_non_zero_vlans(vsi)) {
		*mcast_m = ICE_MCAST_VLAN_PROMISC_BITS;
		*ucast_m = ICE_UCAST_VLAN_PROMISC_BITS;
	} else {
		*mcast_m = ICE_MCAST_PROMISC_BITS;
		*ucast_m = ICE_UCAST_PROMISC_BITS;
	}
}

 
static int
ice_vf_clear_all_promisc_modes(struct ice_vf *vf, struct ice_vsi *vsi)
{
	struct ice_pf *pf = vf->pf;
	u8 ucast_m, mcast_m;
	int ret = 0;

	ice_vf_get_promisc_masks(vf, vsi, &ucast_m, &mcast_m);
	if (test_bit(ICE_VF_STATE_UC_PROMISC, vf->vf_states)) {
		if (!test_bit(ICE_FLAG_VF_TRUE_PROMISC_ENA, pf->flags)) {
			if (ice_is_dflt_vsi_in_use(vsi->port_info))
				ret = ice_clear_dflt_vsi(vsi);
		} else {
			ret = ice_vf_clear_vsi_promisc(vf, vsi, ucast_m);
		}

		if (ret) {
			dev_err(ice_pf_to_dev(vf->pf), "Disabling promiscuous mode failed\n");
		} else {
			clear_bit(ICE_VF_STATE_UC_PROMISC, vf->vf_states);
			dev_info(ice_pf_to_dev(vf->pf), "Disabling promiscuous mode succeeded\n");
		}
	}

	if (test_bit(ICE_VF_STATE_MC_PROMISC, vf->vf_states)) {
		ret = ice_vf_clear_vsi_promisc(vf, vsi, mcast_m);
		if (ret) {
			dev_err(ice_pf_to_dev(vf->pf), "Disabling allmulticast mode failed\n");
		} else {
			clear_bit(ICE_VF_STATE_MC_PROMISC, vf->vf_states);
			dev_info(ice_pf_to_dev(vf->pf), "Disabling allmulticast mode succeeded\n");
		}
	}
	return ret;
}

 
int
ice_vf_set_vsi_promisc(struct ice_vf *vf, struct ice_vsi *vsi, u8 promisc_m)
{
	struct ice_hw *hw = &vsi->back->hw;
	int status;

	if (ice_vf_is_port_vlan_ena(vf))
		status = ice_fltr_set_vsi_promisc(hw, vsi->idx, promisc_m,
						  ice_vf_get_port_vlan_id(vf));
	else if (ice_vsi_has_non_zero_vlans(vsi))
		status = ice_fltr_set_vlan_vsi_promisc(hw, vsi, promisc_m);
	else
		status = ice_fltr_set_vsi_promisc(hw, vsi->idx, promisc_m, 0);

	if (status && status != -EEXIST) {
		dev_err(ice_pf_to_dev(vsi->back), "enable Tx/Rx filter promiscuous mode on VF-%u failed, error: %d\n",
			vf->vf_id, status);
		return status;
	}

	return 0;
}

 
int
ice_vf_clear_vsi_promisc(struct ice_vf *vf, struct ice_vsi *vsi, u8 promisc_m)
{
	struct ice_hw *hw = &vsi->back->hw;
	int status;

	if (ice_vf_is_port_vlan_ena(vf))
		status = ice_fltr_clear_vsi_promisc(hw, vsi->idx, promisc_m,
						    ice_vf_get_port_vlan_id(vf));
	else if (ice_vsi_has_non_zero_vlans(vsi))
		status = ice_fltr_clear_vlan_vsi_promisc(hw, vsi, promisc_m);
	else
		status = ice_fltr_clear_vsi_promisc(hw, vsi->idx, promisc_m, 0);

	if (status && status != -ENOENT) {
		dev_err(ice_pf_to_dev(vsi->back), "disable Tx/Rx filter promiscuous mode on VF-%u failed, error: %d\n",
			vf->vf_id, status);
		return status;
	}

	return 0;
}

 
void ice_reset_all_vfs(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	struct ice_vf *vf;
	unsigned int bkt;

	 
	if (!ice_has_vfs(pf))
		return;

	mutex_lock(&pf->vfs.table_lock);

	 
	ice_for_each_vf(pf, bkt, vf)
		ice_mbx_clear_malvf(&vf->mbx_info);

	 
	if (test_and_set_bit(ICE_VF_DIS, pf->state)) {
		mutex_unlock(&pf->vfs.table_lock);
		return;
	}

	 
	ice_for_each_vf(pf, bkt, vf)
		ice_trigger_vf_reset(vf, true, true);

	 
	ice_for_each_vf(pf, bkt, vf) {
		if (!vf->vf_ops->poll_reset_status(vf)) {
			 
			dev_warn(dev, "VF %u reset check timeout\n", vf->vf_id);
			break;
		}
	}

	 
	ice_for_each_vf(pf, bkt, vf) {
		mutex_lock(&vf->cfg_lock);

		vf->driver_caps = 0;
		ice_vc_set_default_allowlist(vf);

		ice_vf_fdir_exit(vf);
		ice_vf_fdir_init(vf);
		 
		if (vf->ctrl_vsi_idx != ICE_NO_VSI)
			ice_vf_ctrl_invalidate_vsi(vf);

		ice_vf_pre_vsi_rebuild(vf);
		ice_vf_rebuild_vsi(vf);
		ice_vf_post_vsi_rebuild(vf);

		mutex_unlock(&vf->cfg_lock);
	}

	if (ice_is_eswitch_mode_switchdev(pf))
		if (ice_eswitch_rebuild(pf))
			dev_warn(dev, "eswitch rebuild failed\n");

	ice_flush(hw);
	clear_bit(ICE_VF_DIS, pf->state);

	mutex_unlock(&pf->vfs.table_lock);
}

 
static void ice_notify_vf_reset(struct ice_vf *vf)
{
	struct ice_hw *hw = &vf->pf->hw;
	struct virtchnl_pf_event pfe;

	 
	if ((!test_bit(ICE_VF_STATE_INIT, vf->vf_states) &&
	     !test_bit(ICE_VF_STATE_ACTIVE, vf->vf_states)) ||
	    test_bit(ICE_VF_STATE_DIS, vf->vf_states))
		return;

	pfe.event = VIRTCHNL_EVENT_RESET_IMPENDING;
	pfe.severity = PF_EVENT_SEVERITY_CERTAIN_DOOM;
	ice_aq_send_msg_to_vf(hw, vf->vf_id, VIRTCHNL_OP_EVENT,
			      VIRTCHNL_STATUS_SUCCESS, (u8 *)&pfe, sizeof(pfe),
			      NULL);
}

 
int ice_reset_vf(struct ice_vf *vf, u32 flags)
{
	struct ice_pf *pf = vf->pf;
	struct ice_lag *lag;
	struct ice_vsi *vsi;
	u8 act_prt, pri_prt;
	struct device *dev;
	int err = 0;
	bool rsd;

	dev = ice_pf_to_dev(pf);
	act_prt = ICE_LAG_INVALID_PORT;
	pri_prt = pf->hw.port_info->lport;

	if (flags & ICE_VF_RESET_NOTIFY)
		ice_notify_vf_reset(vf);

	if (test_bit(ICE_VF_RESETS_DISABLED, pf->state)) {
		dev_dbg(dev, "Trying to reset VF %d, but all VF resets are disabled\n",
			vf->vf_id);
		return 0;
	}

	lag = pf->lag;
	mutex_lock(&pf->lag_mutex);
	if (lag && lag->bonded && lag->primary) {
		act_prt = lag->active_port;
		if (act_prt != pri_prt && act_prt != ICE_LAG_INVALID_PORT &&
		    lag->upper_netdev)
			ice_lag_move_vf_nodes_cfg(lag, act_prt, pri_prt);
		else
			act_prt = ICE_LAG_INVALID_PORT;
	}

	if (flags & ICE_VF_RESET_LOCK)
		mutex_lock(&vf->cfg_lock);
	else
		lockdep_assert_held(&vf->cfg_lock);

	if (ice_is_vf_disabled(vf)) {
		vsi = ice_get_vf_vsi(vf);
		if (!vsi) {
			dev_dbg(dev, "VF is already removed\n");
			err = -EINVAL;
			goto out_unlock;
		}
		ice_vsi_stop_lan_tx_rings(vsi, ICE_NO_RESET, vf->vf_id);

		if (ice_vsi_is_rx_queue_active(vsi))
			ice_vsi_stop_all_rx_rings(vsi);

		dev_dbg(dev, "VF is already disabled, there is no need for resetting it, telling VM, all is fine %d\n",
			vf->vf_id);
		goto out_unlock;
	}

	 
	set_bit(ICE_VF_STATE_DIS, vf->vf_states);
	ice_trigger_vf_reset(vf, flags & ICE_VF_RESET_VFLR, false);

	vsi = ice_get_vf_vsi(vf);
	if (WARN_ON(!vsi)) {
		err = -EIO;
		goto out_unlock;
	}

	ice_dis_vf_qs(vf);

	 
	ice_dis_vsi_txq(vsi->port_info, vsi->idx, 0, 0, NULL, NULL,
			NULL, vf->vf_ops->reset_type, vf->vf_id, NULL);

	 
	rsd = vf->vf_ops->poll_reset_status(vf);

	 
	if (!rsd)
		dev_warn(dev, "VF reset check timeout on VF %d\n", vf->vf_id);

	vf->driver_caps = 0;
	ice_vc_set_default_allowlist(vf);

	 
	ice_vf_clear_all_promisc_modes(vf, vsi);

	ice_vf_fdir_exit(vf);
	ice_vf_fdir_init(vf);
	 
	if (vf->ctrl_vsi_idx != ICE_NO_VSI)
		ice_vf_ctrl_vsi_release(vf);

	ice_vf_pre_vsi_rebuild(vf);

	if (ice_vf_recreate_vsi(vf)) {
		dev_err(dev, "Failed to release and setup the VF%u's VSI\n",
			vf->vf_id);
		err = -EFAULT;
		goto out_unlock;
	}

	ice_vf_post_vsi_rebuild(vf);
	vsi = ice_get_vf_vsi(vf);
	if (WARN_ON(!vsi)) {
		err = -EINVAL;
		goto out_unlock;
	}

	ice_eswitch_update_repr(vsi);

	 
	ice_mbx_clear_malvf(&vf->mbx_info);

out_unlock:
	if (flags & ICE_VF_RESET_LOCK)
		mutex_unlock(&vf->cfg_lock);

	if (lag && lag->bonded && lag->primary &&
	    act_prt != ICE_LAG_INVALID_PORT)
		ice_lag_move_vf_nodes_cfg(lag, pri_prt, act_prt);
	mutex_unlock(&pf->lag_mutex);

	return err;
}

 
void ice_set_vf_state_dis(struct ice_vf *vf)
{
	ice_set_vf_state_qs_dis(vf);
	vf->vf_ops->clear_reset_state(vf);
}

 

 
void ice_initialize_vf_entry(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;
	struct ice_vfs *vfs;

	vfs = &pf->vfs;

	 
	vf->spoofchk = true;
	vf->num_vf_qs = vfs->num_qps_per;
	ice_vc_set_default_allowlist(vf);
	ice_virtchnl_set_dflt_ops(vf);

	 
	ice_vf_ctrl_invalidate_vsi(vf);
	ice_vf_fdir_init(vf);

	 
	ice_mbx_init_vf_info(&pf->hw, &vf->mbx_info);

	mutex_init(&vf->cfg_lock);
}

 
void ice_dis_vf_qs(struct ice_vf *vf)
{
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);

	if (WARN_ON(!vsi))
		return;

	ice_vsi_stop_lan_tx_rings(vsi, ICE_NO_RESET, vf->vf_id);
	ice_vsi_stop_all_rx_rings(vsi);
	ice_set_vf_state_qs_dis(vf);
}

 
enum virtchnl_status_code ice_err_to_virt_err(int err)
{
	switch (err) {
	case 0:
		return VIRTCHNL_STATUS_SUCCESS;
	case -EINVAL:
	case -ENODEV:
		return VIRTCHNL_STATUS_ERR_PARAM;
	case -ENOMEM:
		return VIRTCHNL_STATUS_ERR_NO_MEMORY;
	case -EALREADY:
	case -EBUSY:
	case -EIO:
	case -ENOSPC:
		return VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR;
	default:
		return VIRTCHNL_STATUS_ERR_NOT_SUPPORTED;
	}
}

 
int ice_check_vf_init(struct ice_vf *vf)
{
	struct ice_pf *pf = vf->pf;

	if (!test_bit(ICE_VF_STATE_INIT, vf->vf_states)) {
		dev_err(ice_pf_to_dev(pf), "VF ID: %u in reset. Try again.\n",
			vf->vf_id);
		return -EBUSY;
	}
	return 0;
}

 
struct ice_port_info *ice_vf_get_port_info(struct ice_vf *vf)
{
	return vf->pf->hw.port_info;
}

 
static int ice_cfg_mac_antispoof(struct ice_vsi *vsi, bool enable)
{
	struct ice_vsi_ctx *ctx;
	int err;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->info.sec_flags = vsi->info.sec_flags;
	ctx->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_SECURITY_VALID);

	if (enable)
		ctx->info.sec_flags |= ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF;
	else
		ctx->info.sec_flags &= ~ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF;

	err = ice_update_vsi(&vsi->back->hw, vsi->idx, ctx, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "Failed to configure Tx MAC anti-spoof %s for VSI %d, error %d\n",
			enable ? "ON" : "OFF", vsi->vsi_num, err);
	else
		vsi->info.sec_flags = ctx->info.sec_flags;

	kfree(ctx);

	return err;
}

 
static int ice_vsi_ena_spoofchk(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	int err = 0;

	vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);

	 
	if (vsi->type != ICE_VSI_VF || ice_vsi_has_non_zero_vlans(vsi)) {
		err = vlan_ops->ena_tx_filtering(vsi);
		if (err)
			return err;
	}

	return ice_cfg_mac_antispoof(vsi, true);
}

 
static int ice_vsi_dis_spoofchk(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	int err;

	vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);

	err = vlan_ops->dis_tx_filtering(vsi);
	if (err)
		return err;

	return ice_cfg_mac_antispoof(vsi, false);
}

 
int ice_vsi_apply_spoofchk(struct ice_vsi *vsi, bool enable)
{
	int err;

	if (enable)
		err = ice_vsi_ena_spoofchk(vsi);
	else
		err = ice_vsi_dis_spoofchk(vsi);

	return err;
}

 
bool ice_is_vf_trusted(struct ice_vf *vf)
{
	return test_bit(ICE_VIRTCHNL_VF_CAP_PRIVILEGE, &vf->vf_caps);
}

 
bool ice_vf_has_no_qs_ena(struct ice_vf *vf)
{
	return (!bitmap_weight(vf->rxq_ena, ICE_MAX_RSS_QS_PER_VF) &&
		!bitmap_weight(vf->txq_ena, ICE_MAX_RSS_QS_PER_VF));
}

 
bool ice_is_vf_link_up(struct ice_vf *vf)
{
	struct ice_port_info *pi = ice_vf_get_port_info(vf);

	if (ice_check_vf_init(vf))
		return false;

	if (ice_vf_has_no_qs_ena(vf))
		return false;
	else if (vf->link_forced)
		return vf->link_up;
	else
		return pi->phy.link_info.link_info &
			ICE_AQ_LINK_UP;
}

 
void ice_vf_ctrl_invalidate_vsi(struct ice_vf *vf)
{
	vf->ctrl_vsi_idx = ICE_NO_VSI;
}

 
void ice_vf_ctrl_vsi_release(struct ice_vf *vf)
{
	ice_vsi_release(vf->pf->vsi[vf->ctrl_vsi_idx]);
	ice_vf_ctrl_invalidate_vsi(vf);
}

 
struct ice_vsi *ice_vf_ctrl_vsi_setup(struct ice_vf *vf)
{
	struct ice_vsi_cfg_params params = {};
	struct ice_pf *pf = vf->pf;
	struct ice_vsi *vsi;

	params.type = ICE_VSI_CTRL;
	params.pi = ice_vf_get_port_info(vf);
	params.vf = vf;
	params.flags = ICE_VSI_FLAG_INIT;

	vsi = ice_vsi_setup(pf, &params);
	if (!vsi) {
		dev_err(ice_pf_to_dev(pf), "Failed to create VF control VSI\n");
		ice_vf_ctrl_invalidate_vsi(vf);
	}

	return vsi;
}

 
int ice_vf_init_host_cfg(struct ice_vf *vf, struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_pf *pf = vf->pf;
	u8 broadcast[ETH_ALEN];
	struct device *dev;
	int err;

	dev = ice_pf_to_dev(pf);

	err = ice_vsi_add_vlan_zero(vsi);
	if (err) {
		dev_warn(dev, "Failed to add VLAN 0 filter for VF %d\n",
			 vf->vf_id);
		return err;
	}

	vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);
	err = vlan_ops->ena_rx_filtering(vsi);
	if (err) {
		dev_warn(dev, "Failed to enable Rx VLAN filtering for VF %d\n",
			 vf->vf_id);
		return err;
	}

	eth_broadcast_addr(broadcast);
	err = ice_fltr_add_mac(vsi, broadcast, ICE_FWD_TO_VSI);
	if (err) {
		dev_err(dev, "Failed to add broadcast MAC filter for VF %d, status %d\n",
			vf->vf_id, err);
		return err;
	}

	vf->num_mac = 1;

	err = ice_vsi_apply_spoofchk(vsi, vf->spoofchk);
	if (err) {
		dev_warn(dev, "Failed to initialize spoofchk setting for VF %d\n",
			 vf->vf_id);
		return err;
	}

	return 0;
}

 
void ice_vf_invalidate_vsi(struct ice_vf *vf)
{
	vf->lan_vsi_idx = ICE_NO_VSI;
	vf->lan_vsi_num = ICE_NO_VSI;
}

 
void ice_vf_vsi_release(struct ice_vf *vf)
{
	struct ice_vsi *vsi = ice_get_vf_vsi(vf);

	if (WARN_ON(!vsi))
		return;

	ice_vsi_release(vsi);
	ice_vf_invalidate_vsi(vf);
}

 
struct ice_vsi *ice_get_vf_ctrl_vsi(struct ice_pf *pf, struct ice_vsi *vsi)
{
	struct ice_vsi *ctrl_vsi = NULL;
	struct ice_vf *vf;
	unsigned int bkt;

	rcu_read_lock();
	ice_for_each_vf_rcu(pf, bkt, vf) {
		if (vf != vsi->vf && vf->ctrl_vsi_idx != ICE_NO_VSI) {
			ctrl_vsi = pf->vsi[vf->ctrl_vsi_idx];
			break;
		}
	}

	rcu_read_unlock();
	return ctrl_vsi;
}


 

#include "fm10k.h"
#include "fm10k_vf.h"
#include "fm10k_pf.h"

static s32 fm10k_iov_msg_error(struct fm10k_hw *hw, u32 **results,
			       struct fm10k_mbx_info *mbx)
{
	struct fm10k_vf_info *vf_info = (struct fm10k_vf_info *)mbx;
	struct fm10k_intfc *interface = hw->back;
	struct pci_dev *pdev = interface->pdev;

	dev_err(&pdev->dev, "Unknown message ID %u on VF %d\n",
		**results & FM10K_TLV_ID_MASK, vf_info->vf_idx);

	return fm10k_tlv_msg_error(hw, results, mbx);
}

 
static s32 fm10k_iov_msg_queue_mac_vlan(struct fm10k_hw *hw, u32 **results,
					struct fm10k_mbx_info *mbx)
{
	struct fm10k_vf_info *vf_info = (struct fm10k_vf_info *)mbx;
	struct fm10k_intfc *interface = hw->back;
	u8 mac[ETH_ALEN];
	u32 *result;
	int err = 0;
	bool set;
	u16 vlan;
	u32 vid;

	 
	if (!FM10K_VF_FLAG_ENABLED(vf_info))
		err = FM10K_ERR_PARAM;

	if (!err && !!results[FM10K_MAC_VLAN_MSG_VLAN]) {
		result = results[FM10K_MAC_VLAN_MSG_VLAN];

		 
		err = fm10k_tlv_attr_get_u32(result, &vid);
		if (err)
			return err;

		set = !(vid & FM10K_VLAN_CLEAR);
		vid &= ~FM10K_VLAN_CLEAR;

		 

		if (vid >> 16) {
			 
			if (vf_info->pf_vid)
				return FM10K_ERR_PARAM;
		} else {
			err = fm10k_iov_select_vid(vf_info, (u16)vid);
			if (err < 0)
				return err;

			vid = err;
		}

		 
		err = hw->mac.ops.update_vlan(hw, vid, vf_info->vsi, set);
	}

	if (!err && !!results[FM10K_MAC_VLAN_MSG_MAC]) {
		result = results[FM10K_MAC_VLAN_MSG_MAC];

		 
		err = fm10k_tlv_attr_get_mac_vlan(result, mac, &vlan);
		if (err)
			return err;

		 
		if (is_valid_ether_addr(vf_info->mac) &&
		    !ether_addr_equal(mac, vf_info->mac))
			return FM10K_ERR_PARAM;

		set = !(vlan & FM10K_VLAN_CLEAR);
		vlan &= ~FM10K_VLAN_CLEAR;

		err = fm10k_iov_select_vid(vf_info, vlan);
		if (err < 0)
			return err;

		vlan = (u16)err;

		 
		err = fm10k_queue_mac_request(interface, vf_info->glort,
					      mac, vlan, set);
	}

	if (!err && !!results[FM10K_MAC_VLAN_MSG_MULTICAST]) {
		result = results[FM10K_MAC_VLAN_MSG_MULTICAST];

		 
		err = fm10k_tlv_attr_get_mac_vlan(result, mac, &vlan);
		if (err)
			return err;

		 
		if (!(vf_info->vf_flags & FM10K_VF_FLAG_MULTI_ENABLED))
			return FM10K_ERR_PARAM;

		set = !(vlan & FM10K_VLAN_CLEAR);
		vlan &= ~FM10K_VLAN_CLEAR;

		err = fm10k_iov_select_vid(vf_info, vlan);
		if (err < 0)
			return err;

		vlan = (u16)err;

		 
		err = fm10k_queue_mac_request(interface, vf_info->glort,
					      mac, vlan, set);
	}

	return err;
}

static const struct fm10k_msg_data iov_mbx_data[] = {
	FM10K_TLV_MSG_TEST_HANDLER(fm10k_tlv_msg_test),
	FM10K_VF_MSG_MSIX_HANDLER(fm10k_iov_msg_msix_pf),
	FM10K_VF_MSG_MAC_VLAN_HANDLER(fm10k_iov_msg_queue_mac_vlan),
	FM10K_VF_MSG_LPORT_STATE_HANDLER(fm10k_iov_msg_lport_state_pf),
	FM10K_TLV_MSG_ERROR_HANDLER(fm10k_iov_msg_error),
};

s32 fm10k_iov_event(struct fm10k_intfc *interface)
{
	struct fm10k_hw *hw = &interface->hw;
	struct fm10k_iov_data *iov_data;
	s64 vflre;
	int i;

	 
	if (!READ_ONCE(interface->iov_data))
		return 0;

	rcu_read_lock();

	iov_data = interface->iov_data;

	 
	if (!iov_data)
		goto read_unlock;

	if (!(fm10k_read_reg(hw, FM10K_EICR) & FM10K_EICR_VFLR))
		goto read_unlock;

	 
	vflre = fm10k_read_reg(hw, FM10K_PFVFLRE(1));
	vflre <<= 32;
	vflre |= fm10k_read_reg(hw, FM10K_PFVFLRE(0));

	i = iov_data->num_vfs;

	for (vflre <<= 64 - i; vflre && i--; vflre += vflre) {
		struct fm10k_vf_info *vf_info = &iov_data->vf_info[i];

		if (vflre >= 0)
			continue;

		hw->iov.ops.reset_resources(hw, vf_info);
		vf_info->mbx.ops.connect(hw, &vf_info->mbx);
	}

read_unlock:
	rcu_read_unlock();

	return 0;
}

s32 fm10k_iov_mbx(struct fm10k_intfc *interface)
{
	struct fm10k_hw *hw = &interface->hw;
	struct fm10k_iov_data *iov_data;
	int i;

	 
	if (!READ_ONCE(interface->iov_data))
		return 0;

	rcu_read_lock();

	iov_data = interface->iov_data;

	 
	if (!iov_data)
		goto read_unlock;

	 
	fm10k_mbx_lock(interface);

	 
process_mbx:
	for (i = iov_data->next_vf_mbx ? : iov_data->num_vfs; i--;) {
		struct fm10k_vf_info *vf_info = &iov_data->vf_info[i];
		struct fm10k_mbx_info *mbx = &vf_info->mbx;
		u16 glort = vf_info->glort;

		 
		hw->mbx.ops.process(hw, &hw->mbx);

		 
		if (vf_info->vf_flags && !fm10k_glort_valid_pf(hw, glort)) {
			hw->iov.ops.reset_lport(hw, vf_info);
			fm10k_clear_macvlan_queue(interface, glort, false);
		}

		 
		if (!mbx->timeout) {
			hw->iov.ops.reset_resources(hw, vf_info);
			mbx->ops.connect(hw, mbx);
		}

		 
		if (hw->mbx.state == FM10K_STATE_OPEN &&
		    !hw->mbx.ops.tx_ready(&hw->mbx, FM10K_VFMBX_MSG_MTU)) {
			 
			interface->hw_sm_mbx_full++;

			 
			fm10k_service_event_schedule(interface);

			break;
		}

		 
		mbx->ops.process(hw, mbx);
	}

	 
	if (i >= 0) {
		iov_data->next_vf_mbx = i + 1;
	} else if (iov_data->next_vf_mbx) {
		iov_data->next_vf_mbx = 0;
		goto process_mbx;
	}

	 
	fm10k_mbx_unlock(interface);

read_unlock:
	rcu_read_unlock();

	return 0;
}

void fm10k_iov_suspend(struct pci_dev *pdev)
{
	struct fm10k_intfc *interface = pci_get_drvdata(pdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;
	int num_vfs, i;

	 
	num_vfs = iov_data ? iov_data->num_vfs : 0;

	 
	fm10k_write_reg(hw, FM10K_DGLORTMAP(fm10k_dglort_vf_rss),
			FM10K_DGLORTMAP_NONE);

	 
	for (i = 0; i < num_vfs; i++) {
		struct fm10k_vf_info *vf_info = &iov_data->vf_info[i];

		hw->iov.ops.reset_resources(hw, vf_info);
		hw->iov.ops.reset_lport(hw, vf_info);
		fm10k_clear_macvlan_queue(interface, vf_info->glort, false);
	}
}

static void fm10k_mask_aer_comp_abort(struct pci_dev *pdev)
{
	u32 err_mask;
	int pos;

	pos = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_ERR);
	if (!pos)
		return;

	 
	pci_read_config_dword(pdev, pos + PCI_ERR_UNCOR_MASK, &err_mask);
	err_mask |= PCI_ERR_UNC_COMP_ABORT;
	pci_write_config_dword(pdev, pos + PCI_ERR_UNCOR_MASK, err_mask);
}

int fm10k_iov_resume(struct pci_dev *pdev)
{
	struct fm10k_intfc *interface = pci_get_drvdata(pdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_dglort_cfg dglort = { 0 };
	struct fm10k_hw *hw = &interface->hw;
	int num_vfs, i;

	 
	num_vfs = iov_data ? iov_data->num_vfs : 0;

	 
	if (!iov_data)
		return -ENOMEM;

	 
	fm10k_mask_aer_comp_abort(pdev);

	 
	hw->iov.ops.assign_resources(hw, num_vfs, num_vfs);

	 
	dglort.glort = hw->mac.dglort_map & FM10K_DGLORTMAP_NONE;
	dglort.idx = fm10k_dglort_vf_rss;
	dglort.inner_rss = 1;
	dglort.rss_l = fls(fm10k_queues_per_pool(hw) - 1);
	dglort.queue_b = fm10k_vf_queue_index(hw, 0);
	dglort.vsi_l = fls(hw->iov.total_vfs - 1);
	dglort.vsi_b = 1;

	hw->mac.ops.configure_dglort_map(hw, &dglort);

	 
	for (i = 0; i < num_vfs; i++) {
		struct fm10k_vf_info *vf_info = &iov_data->vf_info[i];

		 
		if (i == (~hw->mac.dglort_map >> FM10K_DGLORTMAP_MASK_SHIFT))
			break;

		 
		hw->iov.ops.set_lport(hw, vf_info, i,
				      FM10K_VF_FLAG_MULTI_CAPABLE);

		 
		hw->iov.ops.assign_default_mac_vlan(hw, vf_info);

		 
		vf_info->mbx.ops.connect(hw, &vf_info->mbx);
	}

	return 0;
}

s32 fm10k_iov_update_pvid(struct fm10k_intfc *interface, u16 glort, u16 pvid)
{
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;
	struct fm10k_vf_info *vf_info;
	u16 vf_idx = (glort - hw->mac.dglort_map) & FM10K_DGLORTMAP_NONE;

	 
	if (!iov_data)
		return FM10K_ERR_PARAM;

	 
	if (vf_idx >= iov_data->num_vfs)
		return FM10K_ERR_PARAM;

	 
	vf_info = &iov_data->vf_info[vf_idx];
	if (vf_info->sw_vid != pvid) {
		vf_info->sw_vid = pvid;
		hw->iov.ops.assign_default_mac_vlan(hw, vf_info);
	}

	return 0;
}

static void fm10k_iov_free_data(struct pci_dev *pdev)
{
	struct fm10k_intfc *interface = pci_get_drvdata(pdev);

	if (!interface->iov_data)
		return;

	 
	fm10k_iov_suspend(pdev);

	 
	kfree_rcu(interface->iov_data, rcu);
	interface->iov_data = NULL;
}

static s32 fm10k_iov_alloc_data(struct pci_dev *pdev, int num_vfs)
{
	struct fm10k_intfc *interface = pci_get_drvdata(pdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;
	size_t size;
	int i;

	 
	if (iov_data)
		return -EBUSY;

	 
	if (!hw->iov.ops.assign_resources)
		return -ENODEV;

	 
	if (!num_vfs)
		return 0;

	 
	size = offsetof(struct fm10k_iov_data, vf_info[num_vfs]);
	iov_data = kzalloc(size, GFP_KERNEL);
	if (!iov_data)
		return -ENOMEM;

	 
	iov_data->num_vfs = num_vfs;

	 
	for (i = 0; i < num_vfs; i++) {
		struct fm10k_vf_info *vf_info = &iov_data->vf_info[i];
		int err;

		 
		vf_info->vsi = i + 1;
		vf_info->vf_idx = i;

		 
		err = fm10k_pfvf_mbx_init(hw, &vf_info->mbx, iov_mbx_data, i);
		if (err) {
			dev_err(&pdev->dev,
				"Unable to initialize SR-IOV mailbox\n");
			kfree(iov_data);
			return err;
		}
	}

	 
	interface->iov_data = iov_data;

	 
	fm10k_iov_resume(pdev);

	return 0;
}

void fm10k_iov_disable(struct pci_dev *pdev)
{
	if (pci_num_vf(pdev) && pci_vfs_assigned(pdev))
		dev_err(&pdev->dev,
			"Cannot disable SR-IOV while VFs are assigned\n");
	else
		pci_disable_sriov(pdev);

	fm10k_iov_free_data(pdev);
}

int fm10k_iov_configure(struct pci_dev *pdev, int num_vfs)
{
	int current_vfs = pci_num_vf(pdev);
	int err = 0;

	if (current_vfs && pci_vfs_assigned(pdev)) {
		dev_err(&pdev->dev,
			"Cannot modify SR-IOV while VFs are assigned\n");
		num_vfs = current_vfs;
	} else {
		pci_disable_sriov(pdev);
		fm10k_iov_free_data(pdev);
	}

	 
	err = fm10k_iov_alloc_data(pdev, num_vfs);
	if (err)
		return err;

	 
	if (num_vfs && num_vfs != current_vfs) {
		err = pci_enable_sriov(pdev, num_vfs);
		if (err) {
			dev_err(&pdev->dev,
				"Enable PCI SR-IOV failed: %d\n", err);
			return err;
		}
	}

	return num_vfs;
}

 
void fm10k_iov_update_stats(struct fm10k_intfc *interface)
{
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;
	int i;

	if (!iov_data)
		return;

	for (i = 0; i < iov_data->num_vfs; i++)
		hw->iov.ops.update_stats(hw, iov_data->vf_info[i].stats, i);
}

static inline void fm10k_reset_vf_info(struct fm10k_intfc *interface,
				       struct fm10k_vf_info *vf_info)
{
	struct fm10k_hw *hw = &interface->hw;

	 
	fm10k_mbx_lock(interface);

	 
	hw->iov.ops.reset_lport(hw, vf_info);

	fm10k_clear_macvlan_queue(interface, vf_info->glort, false);

	 
	hw->iov.ops.assign_default_mac_vlan(hw, vf_info);

	 
	hw->iov.ops.set_lport(hw, vf_info, vf_info->vf_idx,
			      FM10K_VF_FLAG_MULTI_CAPABLE);

	fm10k_mbx_unlock(interface);
}

int fm10k_ndo_set_vf_mac(struct net_device *netdev, int vf_idx, u8 *mac)
{
	struct fm10k_intfc *interface = netdev_priv(netdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_vf_info *vf_info;

	 
	if (!iov_data || vf_idx >= iov_data->num_vfs)
		return -EINVAL;

	 
	if (!is_zero_ether_addr(mac) && !is_valid_ether_addr(mac))
		return -EINVAL;

	 
	vf_info = &iov_data->vf_info[vf_idx];
	ether_addr_copy(vf_info->mac, mac);

	fm10k_reset_vf_info(interface, vf_info);

	return 0;
}

int fm10k_ndo_set_vf_vlan(struct net_device *netdev, int vf_idx, u16 vid,
			  u8 qos, __be16 vlan_proto)
{
	struct fm10k_intfc *interface = netdev_priv(netdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;
	struct fm10k_vf_info *vf_info;

	 
	if (!iov_data || vf_idx >= iov_data->num_vfs)
		return -EINVAL;

	 
	if (qos || (vid > (VLAN_VID_MASK - 1)))
		return -EINVAL;

	 
	if (vlan_proto != htons(ETH_P_8021Q))
		return -EPROTONOSUPPORT;

	vf_info = &iov_data->vf_info[vf_idx];

	 
	if (vf_info->pf_vid == vid)
		return 0;

	 
	vf_info->pf_vid = vid;

	 
	hw->mac.ops.update_vlan(hw, FM10K_VLAN_ALL, vf_info->vsi, false);

	fm10k_reset_vf_info(interface, vf_info);

	return 0;
}

int fm10k_ndo_set_vf_bw(struct net_device *netdev, int vf_idx,
			int __always_unused min_rate, int max_rate)
{
	struct fm10k_intfc *interface = netdev_priv(netdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;

	 
	if (!iov_data || vf_idx >= iov_data->num_vfs)
		return -EINVAL;

	 
	if (max_rate &&
	    (max_rate < FM10K_VF_TC_MIN || max_rate > FM10K_VF_TC_MAX))
		return -EINVAL;

	 
	iov_data->vf_info[vf_idx].rate = max_rate;

	 
	hw->iov.ops.configure_tc(hw, vf_idx, max_rate);

	return 0;
}

int fm10k_ndo_get_vf_config(struct net_device *netdev,
			    int vf_idx, struct ifla_vf_info *ivi)
{
	struct fm10k_intfc *interface = netdev_priv(netdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_vf_info *vf_info;

	 
	if (!iov_data || vf_idx >= iov_data->num_vfs)
		return -EINVAL;

	vf_info = &iov_data->vf_info[vf_idx];

	ivi->vf = vf_idx;
	ivi->max_tx_rate = vf_info->rate;
	ivi->min_tx_rate = 0;
	ether_addr_copy(ivi->mac, vf_info->mac);
	ivi->vlan = vf_info->pf_vid;
	ivi->qos = 0;

	return 0;
}

int fm10k_ndo_get_vf_stats(struct net_device *netdev,
			   int vf_idx, struct ifla_vf_stats *stats)
{
	struct fm10k_intfc *interface = netdev_priv(netdev);
	struct fm10k_iov_data *iov_data = interface->iov_data;
	struct fm10k_hw *hw = &interface->hw;
	struct fm10k_hw_stats_q *hw_stats;
	u32 idx, qpp;

	 
	if (!iov_data || vf_idx >= iov_data->num_vfs)
		return -EINVAL;

	qpp = fm10k_queues_per_pool(hw);
	hw_stats = iov_data->vf_info[vf_idx].stats;

	for (idx = 0; idx < qpp; idx++) {
		stats->rx_packets += hw_stats[idx].rx_packets.count;
		stats->tx_packets += hw_stats[idx].tx_packets.count;
		stats->rx_bytes += hw_stats[idx].rx_bytes.count;
		stats->tx_bytes += hw_stats[idx].tx_bytes.count;
		stats->rx_dropped += hw_stats[idx].rx_drops.count;
	}

	return 0;
}

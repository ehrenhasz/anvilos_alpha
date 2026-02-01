
 

 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <generated/utsrelease.h>
#include <linux/crash_dump.h>
#include "ice.h"
#include "ice_base.h"
#include "ice_lib.h"
#include "ice_fltr.h"
#include "ice_dcb_lib.h"
#include "ice_dcb_nl.h"
#include "ice_devlink.h"
 
#define CREATE_TRACE_POINTS
#include "ice_trace.h"
#include "ice_eswitch.h"
#include "ice_tc_lib.h"
#include "ice_vsi_vlan_ops.h"
#include <net/xdp_sock_drv.h>

#define DRV_SUMMARY	"Intel(R) Ethernet Connection E800 Series Linux Driver"
static const char ice_driver_string[] = DRV_SUMMARY;
static const char ice_copyright[] = "Copyright (c) 2018, Intel Corporation.";

 
#define ICE_DDP_PKG_PATH	"intel/ice/ddp/"
#define ICE_DDP_PKG_FILE	ICE_DDP_PKG_PATH "ice.pkg"

MODULE_AUTHOR("Intel Corporation, <linux.nics@intel.com>");
MODULE_DESCRIPTION(DRV_SUMMARY);
MODULE_LICENSE("GPL v2");
MODULE_FIRMWARE(ICE_DDP_PKG_FILE);

static int debug = -1;
module_param(debug, int, 0644);
#ifndef CONFIG_DYNAMIC_DEBUG
MODULE_PARM_DESC(debug, "netif level (0=none,...,16=all), hw debug_mask (0x8XXXXXXX)");
#else
MODULE_PARM_DESC(debug, "netif level (0=none,...,16=all)");
#endif  

DEFINE_STATIC_KEY_FALSE(ice_xdp_locking_key);
EXPORT_SYMBOL(ice_xdp_locking_key);

 
struct device *ice_hw_to_dev(struct ice_hw *hw)
{
	struct ice_pf *pf = container_of(hw, struct ice_pf, hw);

	return &pf->pdev->dev;
}

static struct workqueue_struct *ice_wq;
struct workqueue_struct *ice_lag_wq;
static const struct net_device_ops ice_netdev_safe_mode_ops;
static const struct net_device_ops ice_netdev_ops;

static void ice_rebuild(struct ice_pf *pf, enum ice_reset_req reset_type);

static void ice_vsi_release_all(struct ice_pf *pf);

static int ice_rebuild_channels(struct ice_pf *pf);
static void ice_remove_q_channels(struct ice_vsi *vsi, bool rem_adv_fltr);

static int
ice_indr_setup_tc_cb(struct net_device *netdev, struct Qdisc *sch,
		     void *cb_priv, enum tc_setup_type type, void *type_data,
		     void *data,
		     void (*cleanup)(struct flow_block_cb *block_cb));

bool netif_is_ice(const struct net_device *dev)
{
	return dev && (dev->netdev_ops == &ice_netdev_ops);
}

 
static u16 ice_get_tx_pending(struct ice_tx_ring *ring)
{
	u16 head, tail;

	head = ring->next_to_clean;
	tail = ring->next_to_use;

	if (head != tail)
		return (head < tail) ?
			tail - head : (tail + ring->count - head);
	return 0;
}

 
static void ice_check_for_hang_subtask(struct ice_pf *pf)
{
	struct ice_vsi *vsi = NULL;
	struct ice_hw *hw;
	unsigned int i;
	int packets;
	u32 v;

	ice_for_each_vsi(pf, v)
		if (pf->vsi[v] && pf->vsi[v]->type == ICE_VSI_PF) {
			vsi = pf->vsi[v];
			break;
		}

	if (!vsi || test_bit(ICE_VSI_DOWN, vsi->state))
		return;

	if (!(vsi->netdev && netif_carrier_ok(vsi->netdev)))
		return;

	hw = &vsi->back->hw;

	ice_for_each_txq(vsi, i) {
		struct ice_tx_ring *tx_ring = vsi->tx_rings[i];
		struct ice_ring_stats *ring_stats;

		if (!tx_ring)
			continue;
		if (ice_ring_ch_enabled(tx_ring))
			continue;

		ring_stats = tx_ring->ring_stats;
		if (!ring_stats)
			continue;

		if (tx_ring->desc) {
			 
			packets = ring_stats->stats.pkts & INT_MAX;
			if (ring_stats->tx_stats.prev_pkt == packets) {
				 
				ice_trigger_sw_intr(hw, tx_ring->q_vector);
				continue;
			}

			 
			smp_rmb();
			ring_stats->tx_stats.prev_pkt =
			    ice_get_tx_pending(tx_ring) ? packets : -1;
		}
	}
}

 
static int ice_init_mac_fltr(struct ice_pf *pf)
{
	struct ice_vsi *vsi;
	u8 *perm_addr;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return -EINVAL;

	perm_addr = vsi->port_info->mac.perm_addr;
	return ice_fltr_add_mac_and_broadcast(vsi, perm_addr, ICE_FWD_TO_VSI);
}

 
static int ice_add_mac_to_sync_list(struct net_device *netdev, const u8 *addr)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;

	if (ice_fltr_add_mac_to_list(vsi, &vsi->tmp_sync_list, addr,
				     ICE_FWD_TO_VSI))
		return -EINVAL;

	return 0;
}

 
static int ice_add_mac_to_unsync_list(struct net_device *netdev, const u8 *addr)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;

	 
	if (ether_addr_equal(addr, netdev->dev_addr))
		return 0;

	if (ice_fltr_add_mac_to_list(vsi, &vsi->tmp_unsync_list, addr,
				     ICE_FWD_TO_VSI))
		return -EINVAL;

	return 0;
}

 
static bool ice_vsi_fltr_changed(struct ice_vsi *vsi)
{
	return test_bit(ICE_VSI_UMAC_FLTR_CHANGED, vsi->state) ||
	       test_bit(ICE_VSI_MMAC_FLTR_CHANGED, vsi->state);
}

 
static int ice_set_promisc(struct ice_vsi *vsi, u8 promisc_m)
{
	int status;

	if (vsi->type != ICE_VSI_PF)
		return 0;

	if (ice_vsi_has_non_zero_vlans(vsi)) {
		promisc_m |= (ICE_PROMISC_VLAN_RX | ICE_PROMISC_VLAN_TX);
		status = ice_fltr_set_vlan_vsi_promisc(&vsi->back->hw, vsi,
						       promisc_m);
	} else {
		status = ice_fltr_set_vsi_promisc(&vsi->back->hw, vsi->idx,
						  promisc_m, 0);
	}
	if (status && status != -EEXIST)
		return status;

	netdev_dbg(vsi->netdev, "set promisc filter bits for VSI %i: 0x%x\n",
		   vsi->vsi_num, promisc_m);
	return 0;
}

 
static int ice_clear_promisc(struct ice_vsi *vsi, u8 promisc_m)
{
	int status;

	if (vsi->type != ICE_VSI_PF)
		return 0;

	if (ice_vsi_has_non_zero_vlans(vsi)) {
		promisc_m |= (ICE_PROMISC_VLAN_RX | ICE_PROMISC_VLAN_TX);
		status = ice_fltr_clear_vlan_vsi_promisc(&vsi->back->hw, vsi,
							 promisc_m);
	} else {
		status = ice_fltr_clear_vsi_promisc(&vsi->back->hw, vsi->idx,
						    promisc_m, 0);
	}

	netdev_dbg(vsi->netdev, "clear promisc filter bits for VSI %i: 0x%x\n",
		   vsi->vsi_num, promisc_m);
	return status;
}

 
static int ice_vsi_sync_fltr(struct ice_vsi *vsi)
{
	struct ice_vsi_vlan_ops *vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);
	struct device *dev = ice_pf_to_dev(vsi->back);
	struct net_device *netdev = vsi->netdev;
	bool promisc_forced_on = false;
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	u32 changed_flags = 0;
	int err;

	if (!vsi->netdev)
		return -EINVAL;

	while (test_and_set_bit(ICE_CFG_BUSY, vsi->state))
		usleep_range(1000, 2000);

	changed_flags = vsi->current_netdev_flags ^ vsi->netdev->flags;
	vsi->current_netdev_flags = vsi->netdev->flags;

	INIT_LIST_HEAD(&vsi->tmp_sync_list);
	INIT_LIST_HEAD(&vsi->tmp_unsync_list);

	if (ice_vsi_fltr_changed(vsi)) {
		clear_bit(ICE_VSI_UMAC_FLTR_CHANGED, vsi->state);
		clear_bit(ICE_VSI_MMAC_FLTR_CHANGED, vsi->state);

		 
		netif_addr_lock_bh(netdev);
		__dev_uc_sync(netdev, ice_add_mac_to_sync_list,
			      ice_add_mac_to_unsync_list);
		__dev_mc_sync(netdev, ice_add_mac_to_sync_list,
			      ice_add_mac_to_unsync_list);
		 
		netif_addr_unlock_bh(netdev);
	}

	 
	err = ice_fltr_remove_mac_list(vsi, &vsi->tmp_unsync_list);
	ice_fltr_free_list(dev, &vsi->tmp_unsync_list);
	if (err) {
		netdev_err(netdev, "Failed to delete MAC filters\n");
		 
		if (err == -ENOMEM)
			goto out;
	}

	 
	err = ice_fltr_add_mac_list(vsi, &vsi->tmp_sync_list);
	ice_fltr_free_list(dev, &vsi->tmp_sync_list);
	 
	if (err && err != -EEXIST) {
		netdev_err(netdev, "Failed to add MAC filters\n");
		 
		if (hw->adminq.sq_last_status == ICE_AQ_RC_ENOSPC &&
		    !test_and_set_bit(ICE_FLTR_OVERFLOW_PROMISC,
				      vsi->state)) {
			promisc_forced_on = true;
			netdev_warn(netdev, "Reached MAC filter limit, forcing promisc mode on VSI %d\n",
				    vsi->vsi_num);
		} else {
			goto out;
		}
	}
	err = 0;
	 
	if (changed_flags & IFF_ALLMULTI) {
		if (vsi->current_netdev_flags & IFF_ALLMULTI) {
			err = ice_set_promisc(vsi, ICE_MCAST_PROMISC_BITS);
			if (err) {
				vsi->current_netdev_flags &= ~IFF_ALLMULTI;
				goto out_promisc;
			}
		} else {
			 
			err = ice_clear_promisc(vsi, ICE_MCAST_PROMISC_BITS);
			if (err) {
				vsi->current_netdev_flags |= IFF_ALLMULTI;
				goto out_promisc;
			}
		}
	}

	if (((changed_flags & IFF_PROMISC) || promisc_forced_on) ||
	    test_bit(ICE_VSI_PROMISC_CHANGED, vsi->state)) {
		clear_bit(ICE_VSI_PROMISC_CHANGED, vsi->state);
		if (vsi->current_netdev_flags & IFF_PROMISC) {
			 
			if (!ice_is_dflt_vsi_in_use(vsi->port_info)) {
				err = ice_set_dflt_vsi(vsi);
				if (err && err != -EEXIST) {
					netdev_err(netdev, "Error %d setting default VSI %i Rx rule\n",
						   err, vsi->vsi_num);
					vsi->current_netdev_flags &=
						~IFF_PROMISC;
					goto out_promisc;
				}
				err = 0;
				vlan_ops->dis_rx_filtering(vsi);

				 
				err = ice_set_promisc(vsi,
						      ICE_MCAST_PROMISC_BITS);
				if (err)
					goto out_promisc;
			}
		} else {
			 
			if (ice_is_vsi_dflt_vsi(vsi)) {
				err = ice_clear_dflt_vsi(vsi);
				if (err) {
					netdev_err(netdev, "Error %d clearing default VSI %i Rx rule\n",
						   err, vsi->vsi_num);
					vsi->current_netdev_flags |=
						IFF_PROMISC;
					goto out_promisc;
				}
				if (vsi->netdev->features &
				    NETIF_F_HW_VLAN_CTAG_FILTER)
					vlan_ops->ena_rx_filtering(vsi);
			}

			 
			if (!(vsi->current_netdev_flags & IFF_ALLMULTI)) {
				err = ice_clear_promisc(vsi,
							ICE_MCAST_PROMISC_BITS);
				if (err) {
					netdev_err(netdev, "Error %d clearing multicast promiscuous on VSI %i\n",
						   err, vsi->vsi_num);
				}
			}
		}
	}
	goto exit;

out_promisc:
	set_bit(ICE_VSI_PROMISC_CHANGED, vsi->state);
	goto exit;
out:
	 
	set_bit(ICE_VSI_UMAC_FLTR_CHANGED, vsi->state);
	set_bit(ICE_VSI_MMAC_FLTR_CHANGED, vsi->state);
exit:
	clear_bit(ICE_CFG_BUSY, vsi->state);
	return err;
}

 
static void ice_sync_fltr_subtask(struct ice_pf *pf)
{
	int v;

	if (!pf || !(test_bit(ICE_FLAG_FLTR_SYNC, pf->flags)))
		return;

	clear_bit(ICE_FLAG_FLTR_SYNC, pf->flags);

	ice_for_each_vsi(pf, v)
		if (pf->vsi[v] && ice_vsi_fltr_changed(pf->vsi[v]) &&
		    ice_vsi_sync_fltr(pf->vsi[v])) {
			 
			set_bit(ICE_FLAG_FLTR_SYNC, pf->flags);
			break;
		}
}

 
static void ice_pf_dis_all_vsi(struct ice_pf *pf, bool locked)
{
	int node;
	int v;

	ice_for_each_vsi(pf, v)
		if (pf->vsi[v])
			ice_dis_vsi(pf->vsi[v], locked);

	for (node = 0; node < ICE_MAX_PF_AGG_NODES; node++)
		pf->pf_agg_node[node].num_vsis = 0;

	for (node = 0; node < ICE_MAX_VF_AGG_NODES; node++)
		pf->vf_agg_node[node].num_vsis = 0;
}

 
static void ice_clear_sw_switch_recipes(struct ice_pf *pf)
{
	struct ice_sw_recipe *recp;
	u8 i;

	recp = pf->hw.switch_info->recp_list;
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++)
		recp[i].recp_created = false;
}

 
static void
ice_prepare_for_reset(struct ice_pf *pf, enum ice_reset_req reset_type)
{
	struct ice_hw *hw = &pf->hw;
	struct ice_vsi *vsi;
	struct ice_vf *vf;
	unsigned int bkt;

	dev_dbg(ice_pf_to_dev(pf), "reset_type=%d\n", reset_type);

	 
	if (test_bit(ICE_PREPARED_FOR_RESET, pf->state))
		return;

	ice_unplug_aux_dev(pf);

	 
	if (ice_check_sq_alive(hw, &hw->mailboxq))
		ice_vc_notify_reset(pf);

	 
	mutex_lock(&pf->vfs.table_lock);
	ice_for_each_vf(pf, bkt, vf)
		ice_set_vf_state_dis(vf);
	mutex_unlock(&pf->vfs.table_lock);

	if (ice_is_eswitch_mode_switchdev(pf)) {
		if (reset_type != ICE_RESET_PFR)
			ice_clear_sw_switch_recipes(pf);
	}

	 
	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		goto skip;

	 
	vsi->orig_rss_size = 0;

	if (test_bit(ICE_FLAG_TC_MQPRIO, pf->flags)) {
		if (reset_type == ICE_RESET_PFR) {
			vsi->old_ena_tc = vsi->all_enatc;
			vsi->old_numtc = vsi->all_numtc;
		} else {
			ice_remove_q_channels(vsi, true);

			 
			vsi->old_ena_tc = 0;
			vsi->all_enatc = 0;
			vsi->old_numtc = 0;
			vsi->all_numtc = 0;
			vsi->req_txq = 0;
			vsi->req_rxq = 0;
			clear_bit(ICE_FLAG_TC_MQPRIO, pf->flags);
			memset(&vsi->mqprio_qopt, 0, sizeof(vsi->mqprio_qopt));
		}
	}
skip:

	 
	ice_clear_hw_tbls(hw);
	 
	ice_pf_dis_all_vsi(pf, false);

	if (test_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags))
		ice_ptp_prepare_for_reset(pf);

	if (ice_is_feature_supported(pf, ICE_F_GNSS))
		ice_gnss_exit(pf);

	if (hw->port_info)
		ice_sched_clear_port(hw->port_info);

	ice_shutdown_all_ctrlq(hw);

	set_bit(ICE_PREPARED_FOR_RESET, pf->state);
}

 
static void ice_do_reset(struct ice_pf *pf, enum ice_reset_req reset_type)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;

	dev_dbg(dev, "reset_type 0x%x requested\n", reset_type);

	if (pf->lag && pf->lag->bonded && reset_type == ICE_RESET_PFR) {
		dev_dbg(dev, "PFR on a bonded interface, promoting to CORER\n");
		reset_type = ICE_RESET_CORER;
	}

	ice_prepare_for_reset(pf, reset_type);

	 
	if (ice_reset(hw, reset_type)) {
		dev_err(dev, "reset %d failed\n", reset_type);
		set_bit(ICE_RESET_FAILED, pf->state);
		clear_bit(ICE_RESET_OICR_RECV, pf->state);
		clear_bit(ICE_PREPARED_FOR_RESET, pf->state);
		clear_bit(ICE_PFR_REQ, pf->state);
		clear_bit(ICE_CORER_REQ, pf->state);
		clear_bit(ICE_GLOBR_REQ, pf->state);
		wake_up(&pf->reset_wait_queue);
		return;
	}

	 
	if (reset_type == ICE_RESET_PFR) {
		pf->pfr_count++;
		ice_rebuild(pf, reset_type);
		clear_bit(ICE_PREPARED_FOR_RESET, pf->state);
		clear_bit(ICE_PFR_REQ, pf->state);
		wake_up(&pf->reset_wait_queue);
		ice_reset_all_vfs(pf);
	}
}

 
static void ice_reset_subtask(struct ice_pf *pf)
{
	enum ice_reset_req reset_type = ICE_RESET_INVAL;

	 
	if (test_bit(ICE_RESET_OICR_RECV, pf->state)) {
		 
		if (test_and_clear_bit(ICE_CORER_RECV, pf->state))
			reset_type = ICE_RESET_CORER;
		if (test_and_clear_bit(ICE_GLOBR_RECV, pf->state))
			reset_type = ICE_RESET_GLOBR;
		if (test_and_clear_bit(ICE_EMPR_RECV, pf->state))
			reset_type = ICE_RESET_EMPR;
		 
		if (reset_type == ICE_RESET_INVAL)
			return;
		ice_prepare_for_reset(pf, reset_type);

		 
		if (ice_check_reset(&pf->hw)) {
			set_bit(ICE_RESET_FAILED, pf->state);
		} else {
			 
			pf->hw.reset_ongoing = false;
			ice_rebuild(pf, reset_type);
			 
			clear_bit(ICE_RESET_OICR_RECV, pf->state);
			clear_bit(ICE_PREPARED_FOR_RESET, pf->state);
			clear_bit(ICE_PFR_REQ, pf->state);
			clear_bit(ICE_CORER_REQ, pf->state);
			clear_bit(ICE_GLOBR_REQ, pf->state);
			wake_up(&pf->reset_wait_queue);
			ice_reset_all_vfs(pf);
		}

		return;
	}

	 
	if (test_bit(ICE_PFR_REQ, pf->state)) {
		reset_type = ICE_RESET_PFR;
		if (pf->lag && pf->lag->bonded) {
			dev_dbg(ice_pf_to_dev(pf), "PFR on a bonded interface, promoting to CORER\n");
			reset_type = ICE_RESET_CORER;
		}
	}
	if (test_bit(ICE_CORER_REQ, pf->state))
		reset_type = ICE_RESET_CORER;
	if (test_bit(ICE_GLOBR_REQ, pf->state))
		reset_type = ICE_RESET_GLOBR;
	 
	if (reset_type == ICE_RESET_INVAL)
		return;

	 
	if (!test_bit(ICE_DOWN, pf->state) &&
	    !test_bit(ICE_CFG_BUSY, pf->state)) {
		ice_do_reset(pf, reset_type);
	}
}

 
static void ice_print_topo_conflict(struct ice_vsi *vsi)
{
	switch (vsi->port_info->phy.link_info.topo_media_conflict) {
	case ICE_AQ_LINK_TOPO_CONFLICT:
	case ICE_AQ_LINK_MEDIA_CONFLICT:
	case ICE_AQ_LINK_TOPO_UNREACH_PRT:
	case ICE_AQ_LINK_TOPO_UNDRUTIL_PRT:
	case ICE_AQ_LINK_TOPO_UNDRUTIL_MEDIA:
		netdev_info(vsi->netdev, "Potential misconfiguration of the Ethernet port detected. If it was not intended, please use the Intel (R) Ethernet Port Configuration Tool to address the issue.\n");
		break;
	case ICE_AQ_LINK_TOPO_UNSUPP_MEDIA:
		if (test_bit(ICE_FLAG_LINK_LENIENT_MODE_ENA, vsi->back->flags))
			netdev_warn(vsi->netdev, "An unsupported module type was detected. Refer to the Intel(R) Ethernet Adapters and Devices User Guide for a list of supported modules\n");
		else
			netdev_err(vsi->netdev, "Rx/Tx is disabled on this device because an unsupported module type was detected. Refer to the Intel(R) Ethernet Adapters and Devices User Guide for a list of supported modules.\n");
		break;
	default:
		break;
	}
}

 
void ice_print_link_msg(struct ice_vsi *vsi, bool isup)
{
	struct ice_aqc_get_phy_caps_data *caps;
	const char *an_advertised;
	const char *fec_req;
	const char *speed;
	const char *fec;
	const char *fc;
	const char *an;
	int status;

	if (!vsi)
		return;

	if (vsi->current_isup == isup)
		return;

	vsi->current_isup = isup;

	if (!isup) {
		netdev_info(vsi->netdev, "NIC Link is Down\n");
		return;
	}

	switch (vsi->port_info->phy.link_info.link_speed) {
	case ICE_AQ_LINK_SPEED_100GB:
		speed = "100 G";
		break;
	case ICE_AQ_LINK_SPEED_50GB:
		speed = "50 G";
		break;
	case ICE_AQ_LINK_SPEED_40GB:
		speed = "40 G";
		break;
	case ICE_AQ_LINK_SPEED_25GB:
		speed = "25 G";
		break;
	case ICE_AQ_LINK_SPEED_20GB:
		speed = "20 G";
		break;
	case ICE_AQ_LINK_SPEED_10GB:
		speed = "10 G";
		break;
	case ICE_AQ_LINK_SPEED_5GB:
		speed = "5 G";
		break;
	case ICE_AQ_LINK_SPEED_2500MB:
		speed = "2.5 G";
		break;
	case ICE_AQ_LINK_SPEED_1000MB:
		speed = "1 G";
		break;
	case ICE_AQ_LINK_SPEED_100MB:
		speed = "100 M";
		break;
	default:
		speed = "Unknown ";
		break;
	}

	switch (vsi->port_info->fc.current_mode) {
	case ICE_FC_FULL:
		fc = "Rx/Tx";
		break;
	case ICE_FC_TX_PAUSE:
		fc = "Tx";
		break;
	case ICE_FC_RX_PAUSE:
		fc = "Rx";
		break;
	case ICE_FC_NONE:
		fc = "None";
		break;
	default:
		fc = "Unknown";
		break;
	}

	 
	switch (vsi->port_info->phy.link_info.fec_info) {
	case ICE_AQ_LINK_25G_RS_528_FEC_EN:
	case ICE_AQ_LINK_25G_RS_544_FEC_EN:
		fec = "RS-FEC";
		break;
	case ICE_AQ_LINK_25G_KR_FEC_EN:
		fec = "FC-FEC/BASE-R";
		break;
	default:
		fec = "NONE";
		break;
	}

	 
	if (vsi->port_info->phy.link_info.an_info & ICE_AQ_AN_COMPLETED)
		an = "True";
	else
		an = "False";

	 
	caps = kzalloc(sizeof(*caps), GFP_KERNEL);
	if (!caps) {
		fec_req = "Unknown";
		an_advertised = "Unknown";
		goto done;
	}

	status = ice_aq_get_phy_caps(vsi->port_info, false,
				     ICE_AQC_REPORT_ACTIVE_CFG, caps, NULL);
	if (status)
		netdev_info(vsi->netdev, "Get phy capability failed.\n");

	an_advertised = ice_is_phy_caps_an_enabled(caps) ? "On" : "Off";

	if (caps->link_fec_options & ICE_AQC_PHY_FEC_25G_RS_528_REQ ||
	    caps->link_fec_options & ICE_AQC_PHY_FEC_25G_RS_544_REQ)
		fec_req = "RS-FEC";
	else if (caps->link_fec_options & ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ ||
		 caps->link_fec_options & ICE_AQC_PHY_FEC_25G_KR_REQ)
		fec_req = "FC-FEC/BASE-R";
	else
		fec_req = "NONE";

	kfree(caps);

done:
	netdev_info(vsi->netdev, "NIC Link is up %sbps Full Duplex, Requested FEC: %s, Negotiated FEC: %s, Autoneg Advertised: %s, Autoneg Negotiated: %s, Flow Control: %s\n",
		    speed, fec_req, fec, an_advertised, an, fc);
	ice_print_topo_conflict(vsi);
}

 
static void ice_vsi_link_event(struct ice_vsi *vsi, bool link_up)
{
	if (!vsi)
		return;

	if (test_bit(ICE_VSI_DOWN, vsi->state) || !vsi->netdev)
		return;

	if (vsi->type == ICE_VSI_PF) {
		if (link_up == netif_carrier_ok(vsi->netdev))
			return;

		if (link_up) {
			netif_carrier_on(vsi->netdev);
			netif_tx_wake_all_queues(vsi->netdev);
		} else {
			netif_carrier_off(vsi->netdev);
			netif_tx_stop_all_queues(vsi->netdev);
		}
	}
}

 
static void ice_set_dflt_mib(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	u8 mib_type, *buf, *lldpmib = NULL;
	u16 len, typelen, offset = 0;
	struct ice_lldp_org_tlv *tlv;
	struct ice_hw *hw = &pf->hw;
	u32 ouisubtype;

	mib_type = SET_LOCAL_MIB_TYPE_LOCAL_MIB;
	lldpmib = kzalloc(ICE_LLDPDU_SIZE, GFP_KERNEL);
	if (!lldpmib) {
		dev_dbg(dev, "%s Failed to allocate MIB memory\n",
			__func__);
		return;
	}

	 
	tlv = (struct ice_lldp_org_tlv *)lldpmib;
	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_ETS_TLV_LEN);
	tlv->typelen = htons(typelen);
	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_ETS_CFG);
	tlv->ouisubtype = htonl(ouisubtype);

	buf = tlv->tlvinfo;
	buf[0] = 0;

	 
	buf[5] = 0x64;
	len = (typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S;
	offset += len + 2;
	tlv = (struct ice_lldp_org_tlv *)
		((char *)tlv + sizeof(tlv->typelen) + len);

	 
	buf = tlv->tlvinfo;
	tlv->typelen = htons(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_ETS_REC);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	buf[5] = 0x64;
	offset += len + 2;
	tlv = (struct ice_lldp_org_tlv *)
		((char *)tlv + sizeof(tlv->typelen) + len);

	 
	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_PFC_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_PFC_CFG);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	buf[0] = 0x08;
	len = (typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S;
	offset += len + 2;

	if (ice_aq_set_lldp_mib(hw, mib_type, (void *)lldpmib, offset, NULL))
		dev_dbg(dev, "%s Failed to set default LLDP MIB\n", __func__);

	kfree(lldpmib);
}

 
static void ice_check_phy_fw_load(struct ice_pf *pf, u8 link_cfg_err)
{
	if (!(link_cfg_err & ICE_AQ_LINK_EXTERNAL_PHY_LOAD_FAILURE)) {
		clear_bit(ICE_FLAG_PHY_FW_LOAD_FAILED, pf->flags);
		return;
	}

	if (test_bit(ICE_FLAG_PHY_FW_LOAD_FAILED, pf->flags))
		return;

	if (link_cfg_err & ICE_AQ_LINK_EXTERNAL_PHY_LOAD_FAILURE) {
		dev_err(ice_pf_to_dev(pf), "Device failed to load the FW for the external PHY. Please download and install the latest NVM for your device and try again\n");
		set_bit(ICE_FLAG_PHY_FW_LOAD_FAILED, pf->flags);
	}
}

 
static void ice_check_module_power(struct ice_pf *pf, u8 link_cfg_err)
{
	 
	if (!(link_cfg_err & (ICE_AQ_LINK_INVAL_MAX_POWER_LIMIT |
			      ICE_AQ_LINK_MODULE_POWER_UNSUPPORTED))) {
		clear_bit(ICE_FLAG_MOD_POWER_UNSUPPORTED, pf->flags);
		return;
	}

	 
	if (test_bit(ICE_FLAG_MOD_POWER_UNSUPPORTED, pf->flags))
		return;

	if (link_cfg_err & ICE_AQ_LINK_INVAL_MAX_POWER_LIMIT) {
		dev_err(ice_pf_to_dev(pf), "The installed module is incompatible with the device's NVM image. Cannot start link\n");
		set_bit(ICE_FLAG_MOD_POWER_UNSUPPORTED, pf->flags);
	} else if (link_cfg_err & ICE_AQ_LINK_MODULE_POWER_UNSUPPORTED) {
		dev_err(ice_pf_to_dev(pf), "The module's power requirements exceed the device's power supply. Cannot start link\n");
		set_bit(ICE_FLAG_MOD_POWER_UNSUPPORTED, pf->flags);
	}
}

 
static void ice_check_link_cfg_err(struct ice_pf *pf, u8 link_cfg_err)
{
	ice_check_module_power(pf, link_cfg_err);
	ice_check_phy_fw_load(pf, link_cfg_err);
}

 
static int
ice_link_event(struct ice_pf *pf, struct ice_port_info *pi, bool link_up,
	       u16 link_speed)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_phy_info *phy_info;
	struct ice_vsi *vsi;
	u16 old_link_speed;
	bool old_link;
	int status;

	phy_info = &pi->phy;
	phy_info->link_info_old = phy_info->link_info;

	old_link = !!(phy_info->link_info_old.link_info & ICE_AQ_LINK_UP);
	old_link_speed = phy_info->link_info_old.link_speed;

	 
	status = ice_update_link_info(pi);
	if (status)
		dev_dbg(dev, "Failed to update link status on port %d, err %d aq_err %s\n",
			pi->lport, status,
			ice_aq_str(pi->hw->adminq.sq_last_status));

	ice_check_link_cfg_err(pf, pi->phy.link_info.link_cfg_err);

	 
	if (phy_info->link_info.link_info & ICE_AQ_LINK_UP)
		link_up = true;

	vsi = ice_get_main_vsi(pf);
	if (!vsi || !vsi->port_info)
		return -EINVAL;

	 
	if (!test_bit(ICE_FLAG_NO_MEDIA, pf->flags) &&
	    !(pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE)) {
		set_bit(ICE_FLAG_NO_MEDIA, pf->flags);
		ice_set_link(vsi, false);
	}

	 
	if (link_up == old_link && link_speed == old_link_speed)
		return 0;

	ice_ptp_link_change(pf, pf->hw.pf_id, link_up);

	if (ice_is_dcb_active(pf)) {
		if (test_bit(ICE_FLAG_DCB_ENA, pf->flags))
			ice_dcb_rebuild(pf);
	} else {
		if (link_up)
			ice_set_dflt_mib(pf);
	}
	ice_vsi_link_event(vsi, link_up);
	ice_print_link_msg(vsi, link_up);

	ice_vc_notify_link_state(pf);

	return 0;
}

 
static void ice_watchdog_subtask(struct ice_pf *pf)
{
	int i;

	 
	if (test_bit(ICE_DOWN, pf->state) ||
	    test_bit(ICE_CFG_BUSY, pf->state))
		return;

	 
	if (time_before(jiffies,
			pf->serv_tmr_prev + pf->serv_tmr_period))
		return;

	pf->serv_tmr_prev = jiffies;

	 
	ice_update_pf_stats(pf);
	ice_for_each_vsi(pf, i)
		if (pf->vsi[i] && pf->vsi[i]->netdev)
			ice_update_vsi_stats(pf->vsi[i]);
}

 
static int ice_init_link_events(struct ice_port_info *pi)
{
	u16 mask;

	mask = ~((u16)(ICE_AQ_LINK_EVENT_UPDOWN | ICE_AQ_LINK_EVENT_MEDIA_NA |
		       ICE_AQ_LINK_EVENT_MODULE_QUAL_FAIL |
		       ICE_AQ_LINK_EVENT_PHY_FW_LOAD_FAIL));

	if (ice_aq_set_event_mask(pi->hw, pi->lport, mask, NULL)) {
		dev_dbg(ice_hw_to_dev(pi->hw), "Failed to set link event mask for port %d\n",
			pi->lport);
		return -EIO;
	}

	if (ice_aq_get_link_info(pi, true, NULL, NULL)) {
		dev_dbg(ice_hw_to_dev(pi->hw), "Failed to enable link events for port %d\n",
			pi->lport);
		return -EIO;
	}

	return 0;
}

 
static int
ice_handle_link_event(struct ice_pf *pf, struct ice_rq_event_info *event)
{
	struct ice_aqc_get_link_status_data *link_data;
	struct ice_port_info *port_info;
	int status;

	link_data = (struct ice_aqc_get_link_status_data *)event->msg_buf;
	port_info = pf->hw.port_info;
	if (!port_info)
		return -EINVAL;

	status = ice_link_event(pf, port_info,
				!!(link_data->link_info & ICE_AQ_LINK_UP),
				le16_to_cpu(link_data->link_speed));
	if (status)
		dev_dbg(ice_pf_to_dev(pf), "Could not process link event, error %d\n",
			status);

	return status;
}

 
void ice_aq_prep_for_event(struct ice_pf *pf, struct ice_aq_task *task,
			   u16 opcode)
{
	INIT_HLIST_NODE(&task->entry);
	task->opcode = opcode;
	task->state = ICE_AQ_TASK_WAITING;

	spin_lock_bh(&pf->aq_wait_lock);
	hlist_add_head(&task->entry, &pf->aq_wait_list);
	spin_unlock_bh(&pf->aq_wait_lock);
}

 
int ice_aq_wait_for_event(struct ice_pf *pf, struct ice_aq_task *task,
			  unsigned long timeout)
{
	enum ice_aq_task_state *state = &task->state;
	struct device *dev = ice_pf_to_dev(pf);
	unsigned long start = jiffies;
	long ret;
	int err;

	ret = wait_event_interruptible_timeout(pf->aq_wait_queue,
					       *state != ICE_AQ_TASK_WAITING,
					       timeout);
	switch (*state) {
	case ICE_AQ_TASK_NOT_PREPARED:
		WARN(1, "call to %s without ice_aq_prep_for_event()", __func__);
		err = -EINVAL;
		break;
	case ICE_AQ_TASK_WAITING:
		err = ret < 0 ? ret : -ETIMEDOUT;
		break;
	case ICE_AQ_TASK_CANCELED:
		err = ret < 0 ? ret : -ECANCELED;
		break;
	case ICE_AQ_TASK_COMPLETE:
		err = ret < 0 ? ret : 0;
		break;
	default:
		WARN(1, "Unexpected AdminQ wait task state %u", *state);
		err = -EINVAL;
		break;
	}

	dev_dbg(dev, "Waited %u msecs (max %u msecs) for firmware response to op 0x%04x\n",
		jiffies_to_msecs(jiffies - start),
		jiffies_to_msecs(timeout),
		task->opcode);

	spin_lock_bh(&pf->aq_wait_lock);
	hlist_del(&task->entry);
	spin_unlock_bh(&pf->aq_wait_lock);

	return err;
}

 
static void ice_aq_check_events(struct ice_pf *pf, u16 opcode,
				struct ice_rq_event_info *event)
{
	struct ice_rq_event_info *task_ev;
	struct ice_aq_task *task;
	bool found = false;

	spin_lock_bh(&pf->aq_wait_lock);
	hlist_for_each_entry(task, &pf->aq_wait_list, entry) {
		if (task->state != ICE_AQ_TASK_WAITING)
			continue;
		if (task->opcode != opcode)
			continue;

		task_ev = &task->event;
		memcpy(&task_ev->desc, &event->desc, sizeof(event->desc));
		task_ev->msg_len = event->msg_len;

		 
		if (task_ev->msg_buf && task_ev->buf_len >= event->buf_len) {
			memcpy(task_ev->msg_buf, event->msg_buf,
			       event->buf_len);
			task_ev->buf_len = event->buf_len;
		}

		task->state = ICE_AQ_TASK_COMPLETE;
		found = true;
	}
	spin_unlock_bh(&pf->aq_wait_lock);

	if (found)
		wake_up(&pf->aq_wait_queue);
}

 
static void ice_aq_cancel_waiting_tasks(struct ice_pf *pf)
{
	struct ice_aq_task *task;

	spin_lock_bh(&pf->aq_wait_lock);
	hlist_for_each_entry(task, &pf->aq_wait_list, entry)
		task->state = ICE_AQ_TASK_CANCELED;
	spin_unlock_bh(&pf->aq_wait_lock);

	wake_up(&pf->aq_wait_queue);
}

#define ICE_MBX_OVERFLOW_WATERMARK 64

 
static int __ice_clean_ctrlq(struct ice_pf *pf, enum ice_ctl_q q_type)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_rq_event_info event;
	struct ice_hw *hw = &pf->hw;
	struct ice_ctl_q_info *cq;
	u16 pending, i = 0;
	const char *qtype;
	u32 oldval, val;

	 
	if (test_bit(ICE_RESET_FAILED, pf->state))
		return 0;

	switch (q_type) {
	case ICE_CTL_Q_ADMIN:
		cq = &hw->adminq;
		qtype = "Admin";
		break;
	case ICE_CTL_Q_SB:
		cq = &hw->sbq;
		qtype = "Sideband";
		break;
	case ICE_CTL_Q_MAILBOX:
		cq = &hw->mailboxq;
		qtype = "Mailbox";
		 
		hw->mbx_snapshot.mbx_buf.state = ICE_MAL_VF_DETECT_STATE_NEW_SNAPSHOT;
		break;
	default:
		dev_warn(dev, "Unknown control queue type 0x%x\n", q_type);
		return 0;
	}

	 
	val = rd32(hw, cq->rq.len);
	if (val & (PF_FW_ARQLEN_ARQVFE_M | PF_FW_ARQLEN_ARQOVFL_M |
		   PF_FW_ARQLEN_ARQCRIT_M)) {
		oldval = val;
		if (val & PF_FW_ARQLEN_ARQVFE_M)
			dev_dbg(dev, "%s Receive Queue VF Error detected\n",
				qtype);
		if (val & PF_FW_ARQLEN_ARQOVFL_M) {
			dev_dbg(dev, "%s Receive Queue Overflow Error detected\n",
				qtype);
		}
		if (val & PF_FW_ARQLEN_ARQCRIT_M)
			dev_dbg(dev, "%s Receive Queue Critical Error detected\n",
				qtype);
		val &= ~(PF_FW_ARQLEN_ARQVFE_M | PF_FW_ARQLEN_ARQOVFL_M |
			 PF_FW_ARQLEN_ARQCRIT_M);
		if (oldval != val)
			wr32(hw, cq->rq.len, val);
	}

	val = rd32(hw, cq->sq.len);
	if (val & (PF_FW_ATQLEN_ATQVFE_M | PF_FW_ATQLEN_ATQOVFL_M |
		   PF_FW_ATQLEN_ATQCRIT_M)) {
		oldval = val;
		if (val & PF_FW_ATQLEN_ATQVFE_M)
			dev_dbg(dev, "%s Send Queue VF Error detected\n",
				qtype);
		if (val & PF_FW_ATQLEN_ATQOVFL_M) {
			dev_dbg(dev, "%s Send Queue Overflow Error detected\n",
				qtype);
		}
		if (val & PF_FW_ATQLEN_ATQCRIT_M)
			dev_dbg(dev, "%s Send Queue Critical Error detected\n",
				qtype);
		val &= ~(PF_FW_ATQLEN_ATQVFE_M | PF_FW_ATQLEN_ATQOVFL_M |
			 PF_FW_ATQLEN_ATQCRIT_M);
		if (oldval != val)
			wr32(hw, cq->sq.len, val);
	}

	event.buf_len = cq->rq_buf_size;
	event.msg_buf = kzalloc(event.buf_len, GFP_KERNEL);
	if (!event.msg_buf)
		return 0;

	do {
		struct ice_mbx_data data = {};
		u16 opcode;
		int ret;

		ret = ice_clean_rq_elem(hw, cq, &event, &pending);
		if (ret == -EALREADY)
			break;
		if (ret) {
			dev_err(dev, "%s Receive Queue event error %d\n", qtype,
				ret);
			break;
		}

		opcode = le16_to_cpu(event.desc.opcode);

		 
		ice_aq_check_events(pf, opcode, &event);

		switch (opcode) {
		case ice_aqc_opc_get_link_status:
			if (ice_handle_link_event(pf, &event))
				dev_err(dev, "Could not handle link event\n");
			break;
		case ice_aqc_opc_event_lan_overflow:
			ice_vf_lan_overflow_event(pf, &event);
			break;
		case ice_mbx_opc_send_msg_to_pf:
			data.num_msg_proc = i;
			data.num_pending_arq = pending;
			data.max_num_msgs_mbx = hw->mailboxq.num_rq_entries;
			data.async_watermark_val = ICE_MBX_OVERFLOW_WATERMARK;

			ice_vc_process_vf_msg(pf, &event, &data);
			break;
		case ice_aqc_opc_fw_logging:
			ice_output_fw_log(hw, &event.desc, event.msg_buf);
			break;
		case ice_aqc_opc_lldp_set_mib_change:
			ice_dcb_process_lldp_set_mib_change(pf, &event);
			break;
		default:
			dev_dbg(dev, "%s Receive Queue unknown event 0x%04x ignored\n",
				qtype, opcode);
			break;
		}
	} while (pending && (i++ < ICE_DFLT_IRQ_WORK));

	kfree(event.msg_buf);

	return pending && (i == ICE_DFLT_IRQ_WORK);
}

 
static bool ice_ctrlq_pending(struct ice_hw *hw, struct ice_ctl_q_info *cq)
{
	u16 ntu;

	ntu = (u16)(rd32(hw, cq->rq.head) & cq->rq.head_mask);
	return cq->rq.next_to_clean != ntu;
}

 
static void ice_clean_adminq_subtask(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;

	if (!test_bit(ICE_ADMINQ_EVENT_PENDING, pf->state))
		return;

	if (__ice_clean_ctrlq(pf, ICE_CTL_Q_ADMIN))
		return;

	clear_bit(ICE_ADMINQ_EVENT_PENDING, pf->state);

	 
	if (ice_ctrlq_pending(hw, &hw->adminq))
		__ice_clean_ctrlq(pf, ICE_CTL_Q_ADMIN);

	ice_flush(hw);
}

 
static void ice_clean_mailboxq_subtask(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;

	if (!test_bit(ICE_MAILBOXQ_EVENT_PENDING, pf->state))
		return;

	if (__ice_clean_ctrlq(pf, ICE_CTL_Q_MAILBOX))
		return;

	clear_bit(ICE_MAILBOXQ_EVENT_PENDING, pf->state);

	if (ice_ctrlq_pending(hw, &hw->mailboxq))
		__ice_clean_ctrlq(pf, ICE_CTL_Q_MAILBOX);

	ice_flush(hw);
}

 
static void ice_clean_sbq_subtask(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;

	 
	if (!ice_is_sbq_supported(hw)) {
		clear_bit(ICE_SIDEBANDQ_EVENT_PENDING, pf->state);
		return;
	}

	if (!test_bit(ICE_SIDEBANDQ_EVENT_PENDING, pf->state))
		return;

	if (__ice_clean_ctrlq(pf, ICE_CTL_Q_SB))
		return;

	clear_bit(ICE_SIDEBANDQ_EVENT_PENDING, pf->state);

	if (ice_ctrlq_pending(hw, &hw->sbq))
		__ice_clean_ctrlq(pf, ICE_CTL_Q_SB);

	ice_flush(hw);
}

 
void ice_service_task_schedule(struct ice_pf *pf)
{
	if (!test_bit(ICE_SERVICE_DIS, pf->state) &&
	    !test_and_set_bit(ICE_SERVICE_SCHED, pf->state) &&
	    !test_bit(ICE_NEEDS_RESTART, pf->state))
		queue_work(ice_wq, &pf->serv_task);
}

 
static void ice_service_task_complete(struct ice_pf *pf)
{
	WARN_ON(!test_bit(ICE_SERVICE_SCHED, pf->state));

	 
	smp_mb__before_atomic();
	clear_bit(ICE_SERVICE_SCHED, pf->state);
}

 
static int ice_service_task_stop(struct ice_pf *pf)
{
	int ret;

	ret = test_and_set_bit(ICE_SERVICE_DIS, pf->state);

	if (pf->serv_tmr.function)
		del_timer_sync(&pf->serv_tmr);
	if (pf->serv_task.func)
		cancel_work_sync(&pf->serv_task);

	clear_bit(ICE_SERVICE_SCHED, pf->state);
	return ret;
}

 
static void ice_service_task_restart(struct ice_pf *pf)
{
	clear_bit(ICE_SERVICE_DIS, pf->state);
	ice_service_task_schedule(pf);
}

 
static void ice_service_timer(struct timer_list *t)
{
	struct ice_pf *pf = from_timer(pf, t, serv_tmr);

	mod_timer(&pf->serv_tmr, round_jiffies(pf->serv_tmr_period + jiffies));
	ice_service_task_schedule(pf);
}

 
static void ice_handle_mdd_event(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	struct ice_vf *vf;
	unsigned int bkt;
	u32 reg;

	if (!test_and_clear_bit(ICE_MDD_EVENT_PENDING, pf->state)) {
		 
		ice_print_vfs_mdd_events(pf);
		return;
	}

	 
	reg = rd32(hw, GL_MDET_TX_PQM);
	if (reg & GL_MDET_TX_PQM_VALID_M) {
		u8 pf_num = (reg & GL_MDET_TX_PQM_PF_NUM_M) >>
				GL_MDET_TX_PQM_PF_NUM_S;
		u16 vf_num = (reg & GL_MDET_TX_PQM_VF_NUM_M) >>
				GL_MDET_TX_PQM_VF_NUM_S;
		u8 event = (reg & GL_MDET_TX_PQM_MAL_TYPE_M) >>
				GL_MDET_TX_PQM_MAL_TYPE_S;
		u16 queue = ((reg & GL_MDET_TX_PQM_QNUM_M) >>
				GL_MDET_TX_PQM_QNUM_S);

		if (netif_msg_tx_err(pf))
			dev_info(dev, "Malicious Driver Detection event %d on TX queue %d PF# %d VF# %d\n",
				 event, queue, pf_num, vf_num);
		wr32(hw, GL_MDET_TX_PQM, 0xffffffff);
	}

	reg = rd32(hw, GL_MDET_TX_TCLAN);
	if (reg & GL_MDET_TX_TCLAN_VALID_M) {
		u8 pf_num = (reg & GL_MDET_TX_TCLAN_PF_NUM_M) >>
				GL_MDET_TX_TCLAN_PF_NUM_S;
		u16 vf_num = (reg & GL_MDET_TX_TCLAN_VF_NUM_M) >>
				GL_MDET_TX_TCLAN_VF_NUM_S;
		u8 event = (reg & GL_MDET_TX_TCLAN_MAL_TYPE_M) >>
				GL_MDET_TX_TCLAN_MAL_TYPE_S;
		u16 queue = ((reg & GL_MDET_TX_TCLAN_QNUM_M) >>
				GL_MDET_TX_TCLAN_QNUM_S);

		if (netif_msg_tx_err(pf))
			dev_info(dev, "Malicious Driver Detection event %d on TX queue %d PF# %d VF# %d\n",
				 event, queue, pf_num, vf_num);
		wr32(hw, GL_MDET_TX_TCLAN, 0xffffffff);
	}

	reg = rd32(hw, GL_MDET_RX);
	if (reg & GL_MDET_RX_VALID_M) {
		u8 pf_num = (reg & GL_MDET_RX_PF_NUM_M) >>
				GL_MDET_RX_PF_NUM_S;
		u16 vf_num = (reg & GL_MDET_RX_VF_NUM_M) >>
				GL_MDET_RX_VF_NUM_S;
		u8 event = (reg & GL_MDET_RX_MAL_TYPE_M) >>
				GL_MDET_RX_MAL_TYPE_S;
		u16 queue = ((reg & GL_MDET_RX_QNUM_M) >>
				GL_MDET_RX_QNUM_S);

		if (netif_msg_rx_err(pf))
			dev_info(dev, "Malicious Driver Detection event %d on RX queue %d PF# %d VF# %d\n",
				 event, queue, pf_num, vf_num);
		wr32(hw, GL_MDET_RX, 0xffffffff);
	}

	 
	reg = rd32(hw, PF_MDET_TX_PQM);
	if (reg & PF_MDET_TX_PQM_VALID_M) {
		wr32(hw, PF_MDET_TX_PQM, 0xFFFF);
		if (netif_msg_tx_err(pf))
			dev_info(dev, "Malicious Driver Detection event TX_PQM detected on PF\n");
	}

	reg = rd32(hw, PF_MDET_TX_TCLAN);
	if (reg & PF_MDET_TX_TCLAN_VALID_M) {
		wr32(hw, PF_MDET_TX_TCLAN, 0xFFFF);
		if (netif_msg_tx_err(pf))
			dev_info(dev, "Malicious Driver Detection event TX_TCLAN detected on PF\n");
	}

	reg = rd32(hw, PF_MDET_RX);
	if (reg & PF_MDET_RX_VALID_M) {
		wr32(hw, PF_MDET_RX, 0xFFFF);
		if (netif_msg_rx_err(pf))
			dev_info(dev, "Malicious Driver Detection event RX detected on PF\n");
	}

	 
	mutex_lock(&pf->vfs.table_lock);
	ice_for_each_vf(pf, bkt, vf) {
		reg = rd32(hw, VP_MDET_TX_PQM(vf->vf_id));
		if (reg & VP_MDET_TX_PQM_VALID_M) {
			wr32(hw, VP_MDET_TX_PQM(vf->vf_id), 0xFFFF);
			vf->mdd_tx_events.count++;
			set_bit(ICE_MDD_VF_PRINT_PENDING, pf->state);
			if (netif_msg_tx_err(pf))
				dev_info(dev, "Malicious Driver Detection event TX_PQM detected on VF %d\n",
					 vf->vf_id);
		}

		reg = rd32(hw, VP_MDET_TX_TCLAN(vf->vf_id));
		if (reg & VP_MDET_TX_TCLAN_VALID_M) {
			wr32(hw, VP_MDET_TX_TCLAN(vf->vf_id), 0xFFFF);
			vf->mdd_tx_events.count++;
			set_bit(ICE_MDD_VF_PRINT_PENDING, pf->state);
			if (netif_msg_tx_err(pf))
				dev_info(dev, "Malicious Driver Detection event TX_TCLAN detected on VF %d\n",
					 vf->vf_id);
		}

		reg = rd32(hw, VP_MDET_TX_TDPU(vf->vf_id));
		if (reg & VP_MDET_TX_TDPU_VALID_M) {
			wr32(hw, VP_MDET_TX_TDPU(vf->vf_id), 0xFFFF);
			vf->mdd_tx_events.count++;
			set_bit(ICE_MDD_VF_PRINT_PENDING, pf->state);
			if (netif_msg_tx_err(pf))
				dev_info(dev, "Malicious Driver Detection event TX_TDPU detected on VF %d\n",
					 vf->vf_id);
		}

		reg = rd32(hw, VP_MDET_RX(vf->vf_id));
		if (reg & VP_MDET_RX_VALID_M) {
			wr32(hw, VP_MDET_RX(vf->vf_id), 0xFFFF);
			vf->mdd_rx_events.count++;
			set_bit(ICE_MDD_VF_PRINT_PENDING, pf->state);
			if (netif_msg_rx_err(pf))
				dev_info(dev, "Malicious Driver Detection event RX detected on VF %d\n",
					 vf->vf_id);

			 
			if (test_bit(ICE_FLAG_MDD_AUTO_RESET_VF, pf->flags)) {
				 
				ice_print_vf_rx_mdd_event(vf);
				ice_reset_vf(vf, ICE_VF_RESET_LOCK);
			}
		}
	}
	mutex_unlock(&pf->vfs.table_lock);

	ice_print_vfs_mdd_events(pf);
}

 
static int ice_force_phys_link_state(struct ice_vsi *vsi, bool link_up)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_aqc_set_phy_cfg_data *cfg;
	struct ice_port_info *pi;
	struct device *dev;
	int retcode;

	if (!vsi || !vsi->port_info || !vsi->back)
		return -EINVAL;
	if (vsi->type != ICE_VSI_PF)
		return 0;

	dev = ice_pf_to_dev(vsi->back);

	pi = vsi->port_info;

	pcaps = kzalloc(sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps)
		return -ENOMEM;

	retcode = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG, pcaps,
				      NULL);
	if (retcode) {
		dev_err(dev, "Failed to get phy capabilities, VSI %d error %d\n",
			vsi->vsi_num, retcode);
		retcode = -EIO;
		goto out;
	}

	 
	if (link_up == !!(pcaps->caps & ICE_AQC_PHY_EN_LINK) &&
	    link_up == !!(pi->phy.link_info.link_info & ICE_AQ_LINK_UP))
		goto out;

	 
	cfg = kmemdup(&pi->phy.curr_user_phy_cfg, sizeof(*cfg), GFP_KERNEL);
	if (!cfg) {
		retcode = -ENOMEM;
		goto out;
	}

	cfg->caps |= ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;
	if (link_up)
		cfg->caps |= ICE_AQ_PHY_ENA_LINK;
	else
		cfg->caps &= ~ICE_AQ_PHY_ENA_LINK;

	retcode = ice_aq_set_phy_cfg(&vsi->back->hw, pi, cfg, NULL);
	if (retcode) {
		dev_err(dev, "Failed to set phy config, VSI %d error %d\n",
			vsi->vsi_num, retcode);
		retcode = -EIO;
	}

	kfree(cfg);
out:
	kfree(pcaps);
	return retcode;
}

 
static int ice_init_nvm_phy_type(struct ice_port_info *pi)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_pf *pf = pi->hw->back;
	int err;

	pcaps = kzalloc(sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps)
		return -ENOMEM;

	err = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA,
				  pcaps, NULL);

	if (err) {
		dev_err(ice_pf_to_dev(pf), "Get PHY capability failed.\n");
		goto out;
	}

	pf->nvm_phy_type_hi = pcaps->phy_type_high;
	pf->nvm_phy_type_lo = pcaps->phy_type_low;

out:
	kfree(pcaps);
	return err;
}

 
static void ice_init_link_dflt_override(struct ice_port_info *pi)
{
	struct ice_link_default_override_tlv *ldo;
	struct ice_pf *pf = pi->hw->back;

	ldo = &pf->link_dflt_override;
	if (ice_get_link_default_override(ldo, pi))
		return;

	if (!(ldo->options & ICE_LINK_OVERRIDE_PORT_DIS))
		return;

	 
	set_bit(ICE_FLAG_TOTAL_PORT_SHUTDOWN_ENA, pf->flags);
	set_bit(ICE_FLAG_LINK_DOWN_ON_CLOSE_ENA, pf->flags);
}

 
static void ice_init_phy_cfg_dflt_override(struct ice_port_info *pi)
{
	struct ice_link_default_override_tlv *ldo;
	struct ice_aqc_set_phy_cfg_data *cfg;
	struct ice_phy_info *phy = &pi->phy;
	struct ice_pf *pf = pi->hw->back;

	ldo = &pf->link_dflt_override;

	 
	cfg = &phy->curr_user_phy_cfg;

	if (ldo->phy_type_low || ldo->phy_type_high) {
		cfg->phy_type_low = pf->nvm_phy_type_lo &
				    cpu_to_le64(ldo->phy_type_low);
		cfg->phy_type_high = pf->nvm_phy_type_hi &
				     cpu_to_le64(ldo->phy_type_high);
	}
	cfg->link_fec_opt = ldo->fec_options;
	phy->curr_user_fec_req = ICE_FEC_AUTO;

	set_bit(ICE_LINK_DEFAULT_OVERRIDE_PENDING, pf->state);
}

 
static int ice_init_phy_user_cfg(struct ice_port_info *pi)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_phy_info *phy = &pi->phy;
	struct ice_pf *pf = pi->hw->back;
	int err;

	if (!(phy->link_info.link_info & ICE_AQ_MEDIA_AVAILABLE))
		return -EIO;

	pcaps = kzalloc(sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps)
		return -ENOMEM;

	if (ice_fw_supports_report_dflt_cfg(pi->hw))
		err = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_DFLT_CFG,
					  pcaps, NULL);
	else
		err = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_TOPO_CAP_MEDIA,
					  pcaps, NULL);
	if (err) {
		dev_err(ice_pf_to_dev(pf), "Get PHY capability failed.\n");
		goto err_out;
	}

	ice_copy_phy_caps_to_cfg(pi, pcaps, &pi->phy.curr_user_phy_cfg);

	 
	if (ice_fw_supports_link_override(pi->hw) &&
	    !(pcaps->module_compliance_enforcement &
	      ICE_AQC_MOD_ENFORCE_STRICT_MODE)) {
		set_bit(ICE_FLAG_LINK_LENIENT_MODE_ENA, pf->flags);

		 
		if (!ice_fw_supports_report_dflt_cfg(pi->hw) &&
		    (pf->link_dflt_override.options & ICE_LINK_OVERRIDE_EN)) {
			ice_init_phy_cfg_dflt_override(pi);
			goto out;
		}
	}

	 
	phy->curr_user_fec_req = ice_caps_to_fec_mode(pcaps->caps,
						      pcaps->link_fec_options);
	phy->curr_user_fc_req = ice_caps_to_fc_mode(pcaps->caps);

out:
	phy->curr_user_speed_req = ICE_AQ_LINK_SPEED_M;
	set_bit(ICE_PHY_INIT_COMPLETE, pf->state);
err_out:
	kfree(pcaps);
	return err;
}

 
static int ice_configure_phy(struct ice_vsi *vsi)
{
	struct device *dev = ice_pf_to_dev(vsi->back);
	struct ice_port_info *pi = vsi->port_info;
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_aqc_set_phy_cfg_data *cfg;
	struct ice_phy_info *phy = &pi->phy;
	struct ice_pf *pf = vsi->back;
	int err;

	 
	if (!(phy->link_info.link_info & ICE_AQ_MEDIA_AVAILABLE))
		return -ENOMEDIUM;

	ice_print_topo_conflict(vsi);

	if (!test_bit(ICE_FLAG_LINK_LENIENT_MODE_ENA, pf->flags) &&
	    phy->link_info.topo_media_conflict == ICE_AQ_LINK_TOPO_UNSUPP_MEDIA)
		return -EPERM;

	if (test_bit(ICE_FLAG_LINK_DOWN_ON_CLOSE_ENA, pf->flags))
		return ice_force_phys_link_state(vsi, true);

	pcaps = kzalloc(sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps)
		return -ENOMEM;

	 
	err = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG, pcaps,
				  NULL);
	if (err) {
		dev_err(dev, "Failed to get PHY configuration, VSI %d error %d\n",
			vsi->vsi_num, err);
		goto done;
	}

	 
	if (pcaps->caps & ICE_AQC_PHY_EN_LINK &&
	    ice_phy_caps_equals_cfg(pcaps, &phy->curr_user_phy_cfg))
		goto done;

	 
	memset(pcaps, 0, sizeof(*pcaps));
	if (ice_fw_supports_report_dflt_cfg(pi->hw))
		err = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_DFLT_CFG,
					  pcaps, NULL);
	else
		err = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_TOPO_CAP_MEDIA,
					  pcaps, NULL);
	if (err) {
		dev_err(dev, "Failed to get PHY caps, VSI %d error %d\n",
			vsi->vsi_num, err);
		goto done;
	}

	cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	if (!cfg) {
		err = -ENOMEM;
		goto done;
	}

	ice_copy_phy_caps_to_cfg(pi, pcaps, cfg);

	 
	if (test_and_clear_bit(ICE_LINK_DEFAULT_OVERRIDE_PENDING,
			       vsi->back->state)) {
		cfg->phy_type_low = phy->curr_user_phy_cfg.phy_type_low;
		cfg->phy_type_high = phy->curr_user_phy_cfg.phy_type_high;
	} else {
		u64 phy_low = 0, phy_high = 0;

		ice_update_phy_type(&phy_low, &phy_high,
				    pi->phy.curr_user_speed_req);
		cfg->phy_type_low = pcaps->phy_type_low & cpu_to_le64(phy_low);
		cfg->phy_type_high = pcaps->phy_type_high &
				     cpu_to_le64(phy_high);
	}

	 
	if (!cfg->phy_type_low && !cfg->phy_type_high) {
		cfg->phy_type_low = pcaps->phy_type_low;
		cfg->phy_type_high = pcaps->phy_type_high;
	}

	 
	ice_cfg_phy_fec(pi, cfg, phy->curr_user_fec_req);

	 
	if (cfg->link_fec_opt !=
	    (cfg->link_fec_opt & pcaps->link_fec_options)) {
		cfg->caps |= pcaps->caps & ICE_AQC_PHY_EN_AUTO_FEC;
		cfg->link_fec_opt = pcaps->link_fec_options;
	}

	 
	ice_cfg_phy_fc(pi, cfg, phy->curr_user_fc_req);

	 
	cfg->caps |= ICE_AQ_PHY_ENA_AUTO_LINK_UPDT | ICE_AQ_PHY_ENA_LINK;

	err = ice_aq_set_phy_cfg(&pf->hw, pi, cfg, NULL);
	if (err)
		dev_err(dev, "Failed to set phy config, VSI %d error %d\n",
			vsi->vsi_num, err);

	kfree(cfg);
done:
	kfree(pcaps);
	return err;
}

 
static void ice_check_media_subtask(struct ice_pf *pf)
{
	struct ice_port_info *pi;
	struct ice_vsi *vsi;
	int err;

	 
	if (!test_bit(ICE_FLAG_NO_MEDIA, pf->flags))
		return;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return;

	 
	pi = vsi->port_info;
	err = ice_update_link_info(pi);
	if (err)
		return;

	ice_check_link_cfg_err(pf, pi->phy.link_info.link_cfg_err);

	if (pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE) {
		if (!test_bit(ICE_PHY_INIT_COMPLETE, pf->state))
			ice_init_phy_user_cfg(pi);

		 
		if (test_bit(ICE_VSI_DOWN, vsi->state) &&
		    test_bit(ICE_FLAG_LINK_DOWN_ON_CLOSE_ENA, vsi->back->flags))
			return;

		err = ice_configure_phy(vsi);
		if (!err)
			clear_bit(ICE_FLAG_NO_MEDIA, pf->flags);

		 
	}
}

 
static void ice_service_task(struct work_struct *work)
{
	struct ice_pf *pf = container_of(work, struct ice_pf, serv_task);
	unsigned long start_time = jiffies;

	 

	 
	ice_reset_subtask(pf);

	 
	if (ice_is_reset_in_progress(pf->state) ||
	    test_bit(ICE_SUSPENDED, pf->state) ||
	    test_bit(ICE_NEEDS_RESTART, pf->state)) {
		ice_service_task_complete(pf);
		return;
	}

	if (test_and_clear_bit(ICE_AUX_ERR_PENDING, pf->state)) {
		struct iidc_event *event;

		event = kzalloc(sizeof(*event), GFP_KERNEL);
		if (event) {
			set_bit(IIDC_EVENT_CRIT_ERR, event->type);
			 
			swap(event->reg, pf->oicr_err_reg);
			ice_send_event_to_aux(pf, event);
			kfree(event);
		}
	}

	 
	if (test_and_clear_bit(ICE_FLAG_UNPLUG_AUX_DEV, pf->flags))
		ice_unplug_aux_dev(pf);

	 
	if (test_and_clear_bit(ICE_FLAG_PLUG_AUX_DEV, pf->flags))
		ice_plug_aux_dev(pf);

	if (test_and_clear_bit(ICE_FLAG_MTU_CHANGED, pf->flags)) {
		struct iidc_event *event;

		event = kzalloc(sizeof(*event), GFP_KERNEL);
		if (event) {
			set_bit(IIDC_EVENT_AFTER_MTU_CHANGE, event->type);
			ice_send_event_to_aux(pf, event);
			kfree(event);
		}
	}

	ice_clean_adminq_subtask(pf);
	ice_check_media_subtask(pf);
	ice_check_for_hang_subtask(pf);
	ice_sync_fltr_subtask(pf);
	ice_handle_mdd_event(pf);
	ice_watchdog_subtask(pf);

	if (ice_is_safe_mode(pf)) {
		ice_service_task_complete(pf);
		return;
	}

	ice_process_vflr_event(pf);
	ice_clean_mailboxq_subtask(pf);
	ice_clean_sbq_subtask(pf);
	ice_sync_arfs_fltrs(pf);
	ice_flush_fdir_ctx(pf);

	 
	ice_service_task_complete(pf);

	 
	if (time_after(jiffies, (start_time + pf->serv_tmr_period)) ||
	    test_bit(ICE_MDD_EVENT_PENDING, pf->state) ||
	    test_bit(ICE_VFLR_EVENT_PENDING, pf->state) ||
	    test_bit(ICE_MAILBOXQ_EVENT_PENDING, pf->state) ||
	    test_bit(ICE_FD_VF_FLUSH_CTX, pf->state) ||
	    test_bit(ICE_SIDEBANDQ_EVENT_PENDING, pf->state) ||
	    test_bit(ICE_ADMINQ_EVENT_PENDING, pf->state))
		mod_timer(&pf->serv_tmr, jiffies);
}

 
static void ice_set_ctrlq_len(struct ice_hw *hw)
{
	hw->adminq.num_rq_entries = ICE_AQ_LEN;
	hw->adminq.num_sq_entries = ICE_AQ_LEN;
	hw->adminq.rq_buf_size = ICE_AQ_MAX_BUF_LEN;
	hw->adminq.sq_buf_size = ICE_AQ_MAX_BUF_LEN;
	hw->mailboxq.num_rq_entries = PF_MBX_ARQLEN_ARQLEN_M;
	hw->mailboxq.num_sq_entries = ICE_MBXSQ_LEN;
	hw->mailboxq.rq_buf_size = ICE_MBXQ_MAX_BUF_LEN;
	hw->mailboxq.sq_buf_size = ICE_MBXQ_MAX_BUF_LEN;
	hw->sbq.num_rq_entries = ICE_SBQ_LEN;
	hw->sbq.num_sq_entries = ICE_SBQ_LEN;
	hw->sbq.rq_buf_size = ICE_SBQ_MAX_BUF_LEN;
	hw->sbq.sq_buf_size = ICE_SBQ_MAX_BUF_LEN;
}

 
int ice_schedule_reset(struct ice_pf *pf, enum ice_reset_req reset)
{
	struct device *dev = ice_pf_to_dev(pf);

	 
	if (test_bit(ICE_RESET_FAILED, pf->state)) {
		dev_dbg(dev, "earlier reset has failed\n");
		return -EIO;
	}
	 
	if (ice_is_reset_in_progress(pf->state)) {
		dev_dbg(dev, "Reset already in progress\n");
		return -EBUSY;
	}

	switch (reset) {
	case ICE_RESET_PFR:
		set_bit(ICE_PFR_REQ, pf->state);
		break;
	case ICE_RESET_CORER:
		set_bit(ICE_CORER_REQ, pf->state);
		break;
	case ICE_RESET_GLOBR:
		set_bit(ICE_GLOBR_REQ, pf->state);
		break;
	default:
		return -EINVAL;
	}

	ice_service_task_schedule(pf);
	return 0;
}

 
static void
ice_irq_affinity_notify(struct irq_affinity_notify *notify,
			const cpumask_t *mask)
{
	struct ice_q_vector *q_vector =
		container_of(notify, struct ice_q_vector, affinity_notify);

	cpumask_copy(&q_vector->affinity_mask, mask);
}

 
static void ice_irq_affinity_release(struct kref __always_unused *ref) {}

 
static int ice_vsi_ena_irq(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->back->hw;
	int i;

	ice_for_each_q_vector(vsi, i)
		ice_irq_dynamic_ena(hw, vsi, vsi->q_vectors[i]);

	ice_flush(hw);
	return 0;
}

 
static int ice_vsi_req_irq_msix(struct ice_vsi *vsi, char *basename)
{
	int q_vectors = vsi->num_q_vectors;
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int rx_int_idx = 0;
	int tx_int_idx = 0;
	int vector, err;
	int irq_num;

	dev = ice_pf_to_dev(pf);
	for (vector = 0; vector < q_vectors; vector++) {
		struct ice_q_vector *q_vector = vsi->q_vectors[vector];

		irq_num = q_vector->irq.virq;

		if (q_vector->tx.tx_ring && q_vector->rx.rx_ring) {
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-%s-%d", basename, "TxRx", rx_int_idx++);
			tx_int_idx++;
		} else if (q_vector->rx.rx_ring) {
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-%s-%d", basename, "rx", rx_int_idx++);
		} else if (q_vector->tx.tx_ring) {
			snprintf(q_vector->name, sizeof(q_vector->name) - 1,
				 "%s-%s-%d", basename, "tx", tx_int_idx++);
		} else {
			 
			continue;
		}
		if (vsi->type == ICE_VSI_CTRL && vsi->vf)
			err = devm_request_irq(dev, irq_num, vsi->irq_handler,
					       IRQF_SHARED, q_vector->name,
					       q_vector);
		else
			err = devm_request_irq(dev, irq_num, vsi->irq_handler,
					       0, q_vector->name, q_vector);
		if (err) {
			netdev_err(vsi->netdev, "MSIX request_irq failed, error: %d\n",
				   err);
			goto free_q_irqs;
		}

		 
		if (!IS_ENABLED(CONFIG_RFS_ACCEL)) {
			struct irq_affinity_notify *affinity_notify;

			affinity_notify = &q_vector->affinity_notify;
			affinity_notify->notify = ice_irq_affinity_notify;
			affinity_notify->release = ice_irq_affinity_release;
			irq_set_affinity_notifier(irq_num, affinity_notify);
		}

		 
		irq_set_affinity_hint(irq_num, &q_vector->affinity_mask);
	}

	err = ice_set_cpu_rx_rmap(vsi);
	if (err) {
		netdev_err(vsi->netdev, "Failed to setup CPU RMAP on VSI %u: %pe\n",
			   vsi->vsi_num, ERR_PTR(err));
		goto free_q_irqs;
	}

	vsi->irqs_ready = true;
	return 0;

free_q_irqs:
	while (vector--) {
		irq_num = vsi->q_vectors[vector]->irq.virq;
		if (!IS_ENABLED(CONFIG_RFS_ACCEL))
			irq_set_affinity_notifier(irq_num, NULL);
		irq_set_affinity_hint(irq_num, NULL);
		devm_free_irq(dev, irq_num, &vsi->q_vectors[vector]);
	}
	return err;
}

 
static int ice_xdp_alloc_setup_rings(struct ice_vsi *vsi)
{
	struct device *dev = ice_pf_to_dev(vsi->back);
	struct ice_tx_desc *tx_desc;
	int i, j;

	ice_for_each_xdp_txq(vsi, i) {
		u16 xdp_q_idx = vsi->alloc_txq + i;
		struct ice_ring_stats *ring_stats;
		struct ice_tx_ring *xdp_ring;

		xdp_ring = kzalloc(sizeof(*xdp_ring), GFP_KERNEL);
		if (!xdp_ring)
			goto free_xdp_rings;

		ring_stats = kzalloc(sizeof(*ring_stats), GFP_KERNEL);
		if (!ring_stats) {
			ice_free_tx_ring(xdp_ring);
			goto free_xdp_rings;
		}

		xdp_ring->ring_stats = ring_stats;
		xdp_ring->q_index = xdp_q_idx;
		xdp_ring->reg_idx = vsi->txq_map[xdp_q_idx];
		xdp_ring->vsi = vsi;
		xdp_ring->netdev = NULL;
		xdp_ring->dev = dev;
		xdp_ring->count = vsi->num_tx_desc;
		WRITE_ONCE(vsi->xdp_rings[i], xdp_ring);
		if (ice_setup_tx_ring(xdp_ring))
			goto free_xdp_rings;
		ice_set_ring_xdp(xdp_ring);
		spin_lock_init(&xdp_ring->tx_lock);
		for (j = 0; j < xdp_ring->count; j++) {
			tx_desc = ICE_TX_DESC(xdp_ring, j);
			tx_desc->cmd_type_offset_bsz = 0;
		}
	}

	return 0;

free_xdp_rings:
	for (; i >= 0; i--) {
		if (vsi->xdp_rings[i] && vsi->xdp_rings[i]->desc) {
			kfree_rcu(vsi->xdp_rings[i]->ring_stats, rcu);
			vsi->xdp_rings[i]->ring_stats = NULL;
			ice_free_tx_ring(vsi->xdp_rings[i]);
		}
	}
	return -ENOMEM;
}

 
static void ice_vsi_assign_bpf_prog(struct ice_vsi *vsi, struct bpf_prog *prog)
{
	struct bpf_prog *old_prog;
	int i;

	old_prog = xchg(&vsi->xdp_prog, prog);
	ice_for_each_rxq(vsi, i)
		WRITE_ONCE(vsi->rx_rings[i]->xdp_prog, vsi->xdp_prog);

	if (old_prog)
		bpf_prog_put(old_prog);
}

 
int ice_prepare_xdp_rings(struct ice_vsi *vsi, struct bpf_prog *prog)
{
	u16 max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	int xdp_rings_rem = vsi->num_xdp_txq;
	struct ice_pf *pf = vsi->back;
	struct ice_qs_cfg xdp_qs_cfg = {
		.qs_mutex = &pf->avail_q_mutex,
		.pf_map = pf->avail_txqs,
		.pf_map_size = pf->max_pf_txqs,
		.q_count = vsi->num_xdp_txq,
		.scatter_count = ICE_MAX_SCATTER_TXQS,
		.vsi_map = vsi->txq_map,
		.vsi_map_offset = vsi->alloc_txq,
		.mapping_mode = ICE_VSI_MAP_CONTIG
	};
	struct device *dev;
	int i, v_idx;
	int status;

	dev = ice_pf_to_dev(pf);
	vsi->xdp_rings = devm_kcalloc(dev, vsi->num_xdp_txq,
				      sizeof(*vsi->xdp_rings), GFP_KERNEL);
	if (!vsi->xdp_rings)
		return -ENOMEM;

	vsi->xdp_mapping_mode = xdp_qs_cfg.mapping_mode;
	if (__ice_vsi_get_qs(&xdp_qs_cfg))
		goto err_map_xdp;

	if (static_key_enabled(&ice_xdp_locking_key))
		netdev_warn(vsi->netdev,
			    "Could not allocate one XDP Tx ring per CPU, XDP_TX/XDP_REDIRECT actions will be slower\n");

	if (ice_xdp_alloc_setup_rings(vsi))
		goto clear_xdp_rings;

	 
	ice_for_each_q_vector(vsi, v_idx) {
		struct ice_q_vector *q_vector = vsi->q_vectors[v_idx];
		int xdp_rings_per_v, q_id, q_base;

		xdp_rings_per_v = DIV_ROUND_UP(xdp_rings_rem,
					       vsi->num_q_vectors - v_idx);
		q_base = vsi->num_xdp_txq - xdp_rings_rem;

		for (q_id = q_base; q_id < (q_base + xdp_rings_per_v); q_id++) {
			struct ice_tx_ring *xdp_ring = vsi->xdp_rings[q_id];

			xdp_ring->q_vector = q_vector;
			xdp_ring->next = q_vector->tx.tx_ring;
			q_vector->tx.tx_ring = xdp_ring;
		}
		xdp_rings_rem -= xdp_rings_per_v;
	}

	ice_for_each_rxq(vsi, i) {
		if (static_key_enabled(&ice_xdp_locking_key)) {
			vsi->rx_rings[i]->xdp_ring = vsi->xdp_rings[i % vsi->num_xdp_txq];
		} else {
			struct ice_q_vector *q_vector = vsi->rx_rings[i]->q_vector;
			struct ice_tx_ring *ring;

			ice_for_each_tx_ring(ring, q_vector->tx) {
				if (ice_ring_is_xdp(ring)) {
					vsi->rx_rings[i]->xdp_ring = ring;
					break;
				}
			}
		}
		ice_tx_xsk_pool(vsi, i);
	}

	 
	if (ice_is_reset_in_progress(pf->state))
		return 0;

	 
	for (i = 0; i < vsi->tc_cfg.numtc; i++)
		max_txqs[i] = vsi->num_txq + vsi->num_xdp_txq;

	status = ice_cfg_vsi_lan(vsi->port_info, vsi->idx, vsi->tc_cfg.ena_tc,
				 max_txqs);
	if (status) {
		dev_err(dev, "Failed VSI LAN queue config for XDP, error: %d\n",
			status);
		goto clear_xdp_rings;
	}

	 
	if (!ice_is_xdp_ena_vsi(vsi))
		ice_vsi_assign_bpf_prog(vsi, prog);

	return 0;
clear_xdp_rings:
	ice_for_each_xdp_txq(vsi, i)
		if (vsi->xdp_rings[i]) {
			kfree_rcu(vsi->xdp_rings[i], rcu);
			vsi->xdp_rings[i] = NULL;
		}

err_map_xdp:
	mutex_lock(&pf->avail_q_mutex);
	ice_for_each_xdp_txq(vsi, i) {
		clear_bit(vsi->txq_map[i + vsi->alloc_txq], pf->avail_txqs);
		vsi->txq_map[i + vsi->alloc_txq] = ICE_INVAL_Q_INDEX;
	}
	mutex_unlock(&pf->avail_q_mutex);

	devm_kfree(dev, vsi->xdp_rings);
	return -ENOMEM;
}

 
int ice_destroy_xdp_rings(struct ice_vsi *vsi)
{
	u16 max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	struct ice_pf *pf = vsi->back;
	int i, v_idx;

	 
	if (ice_is_reset_in_progress(pf->state) || !vsi->q_vectors[0])
		goto free_qmap;

	ice_for_each_q_vector(vsi, v_idx) {
		struct ice_q_vector *q_vector = vsi->q_vectors[v_idx];
		struct ice_tx_ring *ring;

		ice_for_each_tx_ring(ring, q_vector->tx)
			if (!ring->tx_buf || !ice_ring_is_xdp(ring))
				break;

		 
		q_vector->tx.tx_ring = ring;
	}

free_qmap:
	mutex_lock(&pf->avail_q_mutex);
	ice_for_each_xdp_txq(vsi, i) {
		clear_bit(vsi->txq_map[i + vsi->alloc_txq], pf->avail_txqs);
		vsi->txq_map[i + vsi->alloc_txq] = ICE_INVAL_Q_INDEX;
	}
	mutex_unlock(&pf->avail_q_mutex);

	ice_for_each_xdp_txq(vsi, i)
		if (vsi->xdp_rings[i]) {
			if (vsi->xdp_rings[i]->desc) {
				synchronize_rcu();
				ice_free_tx_ring(vsi->xdp_rings[i]);
			}
			kfree_rcu(vsi->xdp_rings[i]->ring_stats, rcu);
			vsi->xdp_rings[i]->ring_stats = NULL;
			kfree_rcu(vsi->xdp_rings[i], rcu);
			vsi->xdp_rings[i] = NULL;
		}

	devm_kfree(ice_pf_to_dev(pf), vsi->xdp_rings);
	vsi->xdp_rings = NULL;

	if (static_key_enabled(&ice_xdp_locking_key))
		static_branch_dec(&ice_xdp_locking_key);

	if (ice_is_reset_in_progress(pf->state) || !vsi->q_vectors[0])
		return 0;

	ice_vsi_assign_bpf_prog(vsi, NULL);

	 
	for (i = 0; i < vsi->tc_cfg.numtc; i++)
		max_txqs[i] = vsi->num_txq;

	 
	vsi->num_xdp_txq = 0;

	return ice_cfg_vsi_lan(vsi->port_info, vsi->idx, vsi->tc_cfg.ena_tc,
			       max_txqs);
}

 
static void ice_vsi_rx_napi_schedule(struct ice_vsi *vsi)
{
	int i;

	ice_for_each_rxq(vsi, i) {
		struct ice_rx_ring *rx_ring = vsi->rx_rings[i];

		if (rx_ring->xsk_pool)
			napi_schedule(&rx_ring->q_vector->napi);
	}
}

 
int ice_vsi_determine_xdp_res(struct ice_vsi *vsi)
{
	u16 avail = ice_get_avail_txq_count(vsi->back);
	u16 cpus = num_possible_cpus();

	if (avail < cpus / 2)
		return -ENOMEM;

	vsi->num_xdp_txq = min_t(u16, avail, cpus);

	if (vsi->num_xdp_txq < cpus)
		static_branch_inc(&ice_xdp_locking_key);

	return 0;
}

 
static int ice_max_xdp_frame_size(struct ice_vsi *vsi)
{
	if (test_bit(ICE_FLAG_LEGACY_RX, vsi->back->flags))
		return ICE_RXBUF_1664;
	else
		return ICE_RXBUF_3072;
}

 
static int
ice_xdp_setup_prog(struct ice_vsi *vsi, struct bpf_prog *prog,
		   struct netlink_ext_ack *extack)
{
	unsigned int frame_size = vsi->netdev->mtu + ICE_ETH_PKT_HDR_PAD;
	bool if_running = netif_running(vsi->netdev);
	int ret = 0, xdp_ring_err = 0;

	if (prog && !prog->aux->xdp_has_frags) {
		if (frame_size > ice_max_xdp_frame_size(vsi)) {
			NL_SET_ERR_MSG_MOD(extack,
					   "MTU is too large for linear frames and XDP prog does not support frags");
			return -EOPNOTSUPP;
		}
	}

	 
	if (ice_is_xdp_ena_vsi(vsi) == !!prog) {
		ice_vsi_assign_bpf_prog(vsi, prog);
		return 0;
	}

	 
	if (if_running && !test_and_set_bit(ICE_VSI_DOWN, vsi->state)) {
		ret = ice_down(vsi);
		if (ret) {
			NL_SET_ERR_MSG_MOD(extack, "Preparing device for XDP attach failed");
			return ret;
		}
	}

	if (!ice_is_xdp_ena_vsi(vsi) && prog) {
		xdp_ring_err = ice_vsi_determine_xdp_res(vsi);
		if (xdp_ring_err) {
			NL_SET_ERR_MSG_MOD(extack, "Not enough Tx resources for XDP");
		} else {
			xdp_ring_err = ice_prepare_xdp_rings(vsi, prog);
			if (xdp_ring_err)
				NL_SET_ERR_MSG_MOD(extack, "Setting up XDP Tx resources failed");
		}
		xdp_features_set_redirect_target(vsi->netdev, true);
		 
		xdp_ring_err = ice_realloc_zc_buf(vsi, true);
		if (xdp_ring_err)
			NL_SET_ERR_MSG_MOD(extack, "Setting up XDP Rx resources failed");
	} else if (ice_is_xdp_ena_vsi(vsi) && !prog) {
		xdp_features_clear_redirect_target(vsi->netdev);
		xdp_ring_err = ice_destroy_xdp_rings(vsi);
		if (xdp_ring_err)
			NL_SET_ERR_MSG_MOD(extack, "Freeing XDP Tx resources failed");
		 
		xdp_ring_err = ice_realloc_zc_buf(vsi, false);
		if (xdp_ring_err)
			NL_SET_ERR_MSG_MOD(extack, "Freeing XDP Rx resources failed");
	}

	if (if_running)
		ret = ice_up(vsi);

	if (!ret && prog)
		ice_vsi_rx_napi_schedule(vsi);

	return (ret || xdp_ring_err) ? -ENOMEM : 0;
}

 
static int ice_xdp_safe_mode(struct net_device __always_unused *dev,
			     struct netdev_bpf *xdp)
{
	NL_SET_ERR_MSG_MOD(xdp->extack,
			   "Please provide working DDP firmware package in order to use XDP\n"
			   "Refer to Documentation/networking/device_drivers/ethernet/intel/ice.rst");
	return -EOPNOTSUPP;
}

 
static int ice_xdp(struct net_device *dev, struct netdev_bpf *xdp)
{
	struct ice_netdev_priv *np = netdev_priv(dev);
	struct ice_vsi *vsi = np->vsi;

	if (vsi->type != ICE_VSI_PF) {
		NL_SET_ERR_MSG_MOD(xdp->extack, "XDP can be loaded only on PF VSI");
		return -EINVAL;
	}

	switch (xdp->command) {
	case XDP_SETUP_PROG:
		return ice_xdp_setup_prog(vsi, xdp->prog, xdp->extack);
	case XDP_SETUP_XSK_POOL:
		return ice_xsk_pool_setup(vsi, xdp->xsk.pool,
					  xdp->xsk.queue_id);
	default:
		return -EINVAL;
	}
}

 
static void ice_ena_misc_vector(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	u32 val;

	 
	val = rd32(hw, GL_MDCK_TX_TDPU);
	val |= GL_MDCK_TX_TDPU_RCU_ANTISPOOF_ITR_DIS_M;
	wr32(hw, GL_MDCK_TX_TDPU, val);

	 
	wr32(hw, PFINT_OICR_ENA, 0);	 
	rd32(hw, PFINT_OICR);		 

	val = (PFINT_OICR_ECC_ERR_M |
	       PFINT_OICR_MAL_DETECT_M |
	       PFINT_OICR_GRST_M |
	       PFINT_OICR_PCI_EXCEPTION_M |
	       PFINT_OICR_VFLR_M |
	       PFINT_OICR_HMC_ERR_M |
	       PFINT_OICR_PE_PUSH_M |
	       PFINT_OICR_PE_CRITERR_M);

	wr32(hw, PFINT_OICR_ENA, val);

	 
	wr32(hw, GLINT_DYN_CTL(pf->oicr_irq.index),
	     GLINT_DYN_CTL_SW_ITR_INDX_M | GLINT_DYN_CTL_INTENA_MSK_M);
}

 
static irqreturn_t ice_misc_intr(int __always_unused irq, void *data)
{
	struct ice_pf *pf = (struct ice_pf *)data;
	struct ice_hw *hw = &pf->hw;
	struct device *dev;
	u32 oicr, ena_mask;

	dev = ice_pf_to_dev(pf);
	set_bit(ICE_ADMINQ_EVENT_PENDING, pf->state);
	set_bit(ICE_MAILBOXQ_EVENT_PENDING, pf->state);
	set_bit(ICE_SIDEBANDQ_EVENT_PENDING, pf->state);

	oicr = rd32(hw, PFINT_OICR);
	ena_mask = rd32(hw, PFINT_OICR_ENA);

	if (oicr & PFINT_OICR_SWINT_M) {
		ena_mask &= ~PFINT_OICR_SWINT_M;
		pf->sw_int_count++;
	}

	if (oicr & PFINT_OICR_MAL_DETECT_M) {
		ena_mask &= ~PFINT_OICR_MAL_DETECT_M;
		set_bit(ICE_MDD_EVENT_PENDING, pf->state);
	}
	if (oicr & PFINT_OICR_VFLR_M) {
		 
		if (test_bit(ICE_VF_RESETS_DISABLED, pf->state)) {
			u32 reg = rd32(hw, PFINT_OICR_ENA);

			reg &= ~PFINT_OICR_VFLR_M;
			wr32(hw, PFINT_OICR_ENA, reg);
		} else {
			ena_mask &= ~PFINT_OICR_VFLR_M;
			set_bit(ICE_VFLR_EVENT_PENDING, pf->state);
		}
	}

	if (oicr & PFINT_OICR_GRST_M) {
		u32 reset;

		 
		ena_mask &= ~PFINT_OICR_GRST_M;
		reset = (rd32(hw, GLGEN_RSTAT) & GLGEN_RSTAT_RESET_TYPE_M) >>
			GLGEN_RSTAT_RESET_TYPE_S;

		if (reset == ICE_RESET_CORER)
			pf->corer_count++;
		else if (reset == ICE_RESET_GLOBR)
			pf->globr_count++;
		else if (reset == ICE_RESET_EMPR)
			pf->empr_count++;
		else
			dev_dbg(dev, "Invalid reset type %d\n", reset);

		 
		if (!test_and_set_bit(ICE_RESET_OICR_RECV, pf->state)) {
			if (reset == ICE_RESET_CORER)
				set_bit(ICE_CORER_RECV, pf->state);
			else if (reset == ICE_RESET_GLOBR)
				set_bit(ICE_GLOBR_RECV, pf->state);
			else
				set_bit(ICE_EMPR_RECV, pf->state);

			 
			hw->reset_ongoing = true;
		}
	}

	if (oicr & PFINT_OICR_TSYN_TX_M) {
		ena_mask &= ~PFINT_OICR_TSYN_TX_M;
		if (!hw->reset_ongoing)
			set_bit(ICE_MISC_THREAD_TX_TSTAMP, pf->misc_thread);
	}

	if (oicr & PFINT_OICR_TSYN_EVNT_M) {
		u8 tmr_idx = hw->func_caps.ts_func_info.tmr_index_owned;
		u32 gltsyn_stat = rd32(hw, GLTSYN_STAT(tmr_idx));

		ena_mask &= ~PFINT_OICR_TSYN_EVNT_M;

		if (hw->func_caps.ts_func_info.src_tmr_owned) {
			 
			pf->ptp.ext_ts_irq |= gltsyn_stat &
					      (GLTSYN_STAT_EVENT0_M |
					       GLTSYN_STAT_EVENT1_M |
					       GLTSYN_STAT_EVENT2_M);

			set_bit(ICE_MISC_THREAD_EXTTS_EVENT, pf->misc_thread);
		}
	}

#define ICE_AUX_CRIT_ERR (PFINT_OICR_PE_CRITERR_M | PFINT_OICR_HMC_ERR_M | PFINT_OICR_PE_PUSH_M)
	if (oicr & ICE_AUX_CRIT_ERR) {
		pf->oicr_err_reg |= oicr;
		set_bit(ICE_AUX_ERR_PENDING, pf->state);
		ena_mask &= ~ICE_AUX_CRIT_ERR;
	}

	 
	oicr &= ena_mask;
	if (oicr) {
		dev_dbg(dev, "unhandled interrupt oicr=0x%08x\n", oicr);
		 
		if (oicr & (PFINT_OICR_PCI_EXCEPTION_M |
			    PFINT_OICR_ECC_ERR_M)) {
			set_bit(ICE_PFR_REQ, pf->state);
		}
	}

	return IRQ_WAKE_THREAD;
}

 
static irqreturn_t ice_misc_intr_thread_fn(int __always_unused irq, void *data)
{
	struct ice_pf *pf = data;
	struct ice_hw *hw;

	hw = &pf->hw;

	if (ice_is_reset_in_progress(pf->state))
		return IRQ_HANDLED;

	ice_service_task_schedule(pf);

	if (test_and_clear_bit(ICE_MISC_THREAD_EXTTS_EVENT, pf->misc_thread))
		ice_ptp_extts_event(pf);

	if (test_and_clear_bit(ICE_MISC_THREAD_TX_TSTAMP, pf->misc_thread)) {
		 
		if (ice_ptp_process_ts(pf) == ICE_TX_TSTAMP_WORK_PENDING) {
			wr32(hw, PFINT_OICR, PFINT_OICR_TSYN_TX_M);
			ice_flush(hw);
		}
	}

	ice_irq_dynamic_ena(hw, NULL, NULL);

	return IRQ_HANDLED;
}

 
static void ice_dis_ctrlq_interrupts(struct ice_hw *hw)
{
	 
	wr32(hw, PFINT_FW_CTL,
	     rd32(hw, PFINT_FW_CTL) & ~PFINT_FW_CTL_CAUSE_ENA_M);

	 
	wr32(hw, PFINT_MBX_CTL,
	     rd32(hw, PFINT_MBX_CTL) & ~PFINT_MBX_CTL_CAUSE_ENA_M);

	wr32(hw, PFINT_SB_CTL,
	     rd32(hw, PFINT_SB_CTL) & ~PFINT_SB_CTL_CAUSE_ENA_M);

	 
	wr32(hw, PFINT_OICR_CTL,
	     rd32(hw, PFINT_OICR_CTL) & ~PFINT_OICR_CTL_CAUSE_ENA_M);

	ice_flush(hw);
}

 
static void ice_free_irq_msix_misc(struct ice_pf *pf)
{
	int misc_irq_num = pf->oicr_irq.virq;
	struct ice_hw *hw = &pf->hw;

	ice_dis_ctrlq_interrupts(hw);

	 
	wr32(hw, PFINT_OICR_ENA, 0);
	ice_flush(hw);

	synchronize_irq(misc_irq_num);
	devm_free_irq(ice_pf_to_dev(pf), misc_irq_num, pf);

	ice_free_irq(pf, pf->oicr_irq);
}

 
static void ice_ena_ctrlq_interrupts(struct ice_hw *hw, u16 reg_idx)
{
	u32 val;

	val = ((reg_idx & PFINT_OICR_CTL_MSIX_INDX_M) |
	       PFINT_OICR_CTL_CAUSE_ENA_M);
	wr32(hw, PFINT_OICR_CTL, val);

	 
	val = ((reg_idx & PFINT_FW_CTL_MSIX_INDX_M) |
	       PFINT_FW_CTL_CAUSE_ENA_M);
	wr32(hw, PFINT_FW_CTL, val);

	 
	val = ((reg_idx & PFINT_MBX_CTL_MSIX_INDX_M) |
	       PFINT_MBX_CTL_CAUSE_ENA_M);
	wr32(hw, PFINT_MBX_CTL, val);

	 
	val = ((reg_idx & PFINT_SB_CTL_MSIX_INDX_M) |
	       PFINT_SB_CTL_CAUSE_ENA_M);
	wr32(hw, PFINT_SB_CTL, val);

	ice_flush(hw);
}

 
static int ice_req_irq_msix_misc(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	struct msi_map oicr_irq;
	int err = 0;

	if (!pf->int_name[0])
		snprintf(pf->int_name, sizeof(pf->int_name) - 1, "%s-%s:misc",
			 dev_driver_string(dev), dev_name(dev));

	 
	if (ice_is_reset_in_progress(pf->state))
		goto skip_req_irq;

	 
	oicr_irq = ice_alloc_irq(pf, false);
	if (oicr_irq.index < 0)
		return oicr_irq.index;

	pf->oicr_irq = oicr_irq;
	err = devm_request_threaded_irq(dev, pf->oicr_irq.virq, ice_misc_intr,
					ice_misc_intr_thread_fn, 0,
					pf->int_name, pf);
	if (err) {
		dev_err(dev, "devm_request_threaded_irq for %s failed: %d\n",
			pf->int_name, err);
		ice_free_irq(pf, pf->oicr_irq);
		return err;
	}

skip_req_irq:
	ice_ena_misc_vector(pf);

	ice_ena_ctrlq_interrupts(hw, pf->oicr_irq.index);
	wr32(hw, GLINT_ITR(ICE_RX_ITR, pf->oicr_irq.index),
	     ITR_REG_ALIGN(ICE_ITR_8K) >> ICE_ITR_GRAN_S);

	ice_flush(hw);
	ice_irq_dynamic_ena(hw, NULL, NULL);

	return 0;
}

 
static void ice_napi_add(struct ice_vsi *vsi)
{
	int v_idx;

	if (!vsi->netdev)
		return;

	ice_for_each_q_vector(vsi, v_idx)
		netif_napi_add(vsi->netdev, &vsi->q_vectors[v_idx]->napi,
			       ice_napi_poll);
}

 
static void ice_set_ops(struct ice_vsi *vsi)
{
	struct net_device *netdev = vsi->netdev;
	struct ice_pf *pf = ice_netdev_to_pf(netdev);

	if (ice_is_safe_mode(pf)) {
		netdev->netdev_ops = &ice_netdev_safe_mode_ops;
		ice_set_ethtool_safe_mode_ops(netdev);
		return;
	}

	netdev->netdev_ops = &ice_netdev_ops;
	netdev->udp_tunnel_nic_info = &pf->hw.udp_tunnel_nic;
	ice_set_ethtool_ops(netdev);

	if (vsi->type != ICE_VSI_PF)
		return;

	netdev->xdp_features = NETDEV_XDP_ACT_BASIC | NETDEV_XDP_ACT_REDIRECT |
			       NETDEV_XDP_ACT_XSK_ZEROCOPY |
			       NETDEV_XDP_ACT_RX_SG;
	netdev->xdp_zc_max_segs = ICE_MAX_BUF_TXD;
}

 
static void ice_set_netdev_features(struct net_device *netdev)
{
	struct ice_pf *pf = ice_netdev_to_pf(netdev);
	bool is_dvm_ena = ice_is_dvm_ena(&pf->hw);
	netdev_features_t csumo_features;
	netdev_features_t vlano_features;
	netdev_features_t dflt_features;
	netdev_features_t tso_features;

	if (ice_is_safe_mode(pf)) {
		 
		netdev->features = NETIF_F_SG | NETIF_F_HIGHDMA;
		netdev->hw_features = netdev->features;
		return;
	}

	dflt_features = NETIF_F_SG	|
			NETIF_F_HIGHDMA	|
			NETIF_F_NTUPLE	|
			NETIF_F_RXHASH;

	csumo_features = NETIF_F_RXCSUM	  |
			 NETIF_F_IP_CSUM  |
			 NETIF_F_SCTP_CRC |
			 NETIF_F_IPV6_CSUM;

	vlano_features = NETIF_F_HW_VLAN_CTAG_FILTER |
			 NETIF_F_HW_VLAN_CTAG_TX     |
			 NETIF_F_HW_VLAN_CTAG_RX;

	 
	if (is_dvm_ena)
		vlano_features |= NETIF_F_HW_VLAN_STAG_FILTER;

	tso_features = NETIF_F_TSO			|
		       NETIF_F_TSO_ECN			|
		       NETIF_F_TSO6			|
		       NETIF_F_GSO_GRE			|
		       NETIF_F_GSO_UDP_TUNNEL		|
		       NETIF_F_GSO_GRE_CSUM		|
		       NETIF_F_GSO_UDP_TUNNEL_CSUM	|
		       NETIF_F_GSO_PARTIAL		|
		       NETIF_F_GSO_IPXIP4		|
		       NETIF_F_GSO_IPXIP6		|
		       NETIF_F_GSO_UDP_L4;

	netdev->gso_partial_features |= NETIF_F_GSO_UDP_TUNNEL_CSUM |
					NETIF_F_GSO_GRE_CSUM;
	 
	netdev->hw_features = dflt_features | csumo_features |
			      vlano_features | tso_features;

	 
	netdev->mpls_features =  NETIF_F_HW_CSUM |
				 NETIF_F_TSO     |
				 NETIF_F_TSO6;

	 
	netdev->features |= netdev->hw_features;

	netdev->hw_features |= NETIF_F_HW_TC;
	netdev->hw_features |= NETIF_F_LOOPBACK;

	 
	netdev->hw_enc_features |= dflt_features | csumo_features |
				   tso_features;
	netdev->vlan_features |= dflt_features | csumo_features |
				 tso_features;

	 
	if (is_dvm_ena)
		netdev->hw_features |= NETIF_F_HW_VLAN_STAG_RX |
			NETIF_F_HW_VLAN_STAG_TX;

	 
	netdev->hw_features |= NETIF_F_RXFCS;

	netif_set_tso_max_size(netdev, ICE_MAX_TSO_SIZE);
}

 
void ice_fill_rss_lut(u8 *lut, u16 rss_table_size, u16 rss_size)
{
	u16 i;

	for (i = 0; i < rss_table_size; i++)
		lut[i] = i % rss_size;
}

 
static struct ice_vsi *
ice_pf_vsi_setup(struct ice_pf *pf, struct ice_port_info *pi)
{
	struct ice_vsi_cfg_params params = {};

	params.type = ICE_VSI_PF;
	params.pi = pi;
	params.flags = ICE_VSI_FLAG_INIT;

	return ice_vsi_setup(pf, &params);
}

static struct ice_vsi *
ice_chnl_vsi_setup(struct ice_pf *pf, struct ice_port_info *pi,
		   struct ice_channel *ch)
{
	struct ice_vsi_cfg_params params = {};

	params.type = ICE_VSI_CHNL;
	params.pi = pi;
	params.ch = ch;
	params.flags = ICE_VSI_FLAG_INIT;

	return ice_vsi_setup(pf, &params);
}

 
static struct ice_vsi *
ice_ctrl_vsi_setup(struct ice_pf *pf, struct ice_port_info *pi)
{
	struct ice_vsi_cfg_params params = {};

	params.type = ICE_VSI_CTRL;
	params.pi = pi;
	params.flags = ICE_VSI_FLAG_INIT;

	return ice_vsi_setup(pf, &params);
}

 
struct ice_vsi *
ice_lb_vsi_setup(struct ice_pf *pf, struct ice_port_info *pi)
{
	struct ice_vsi_cfg_params params = {};

	params.type = ICE_VSI_LB;
	params.pi = pi;
	params.flags = ICE_VSI_FLAG_INIT;

	return ice_vsi_setup(pf, &params);
}

 
static int
ice_vlan_rx_add_vid(struct net_device *netdev, __be16 proto, u16 vid)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_vsi *vsi = np->vsi;
	struct ice_vlan vlan;
	int ret;

	 
	if (!vid)
		return 0;

	while (test_and_set_bit(ICE_CFG_BUSY, vsi->state))
		usleep_range(1000, 2000);

	 
	if (vsi->current_netdev_flags & IFF_ALLMULTI) {
		ret = ice_fltr_set_vsi_promisc(&vsi->back->hw, vsi->idx,
					       ICE_MCAST_VLAN_PROMISC_BITS,
					       vid);
		if (ret)
			goto finish;
	}

	vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);

	 
	vlan = ICE_VLAN(be16_to_cpu(proto), vid, 0);
	ret = vlan_ops->add_vlan(vsi, &vlan);
	if (ret)
		goto finish;

	 
	if ((vsi->current_netdev_flags & IFF_ALLMULTI) &&
	    ice_vsi_num_non_zero_vlans(vsi) == 1) {
		ice_fltr_clear_vsi_promisc(&vsi->back->hw, vsi->idx,
					   ICE_MCAST_PROMISC_BITS, 0);
		ice_fltr_set_vsi_promisc(&vsi->back->hw, vsi->idx,
					 ICE_MCAST_VLAN_PROMISC_BITS, 0);
	}

finish:
	clear_bit(ICE_CFG_BUSY, vsi->state);

	return ret;
}

 
static int
ice_vlan_rx_kill_vid(struct net_device *netdev, __be16 proto, u16 vid)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi_vlan_ops *vlan_ops;
	struct ice_vsi *vsi = np->vsi;
	struct ice_vlan vlan;
	int ret;

	 
	if (!vid)
		return 0;

	while (test_and_set_bit(ICE_CFG_BUSY, vsi->state))
		usleep_range(1000, 2000);

	ret = ice_clear_vsi_promisc(&vsi->back->hw, vsi->idx,
				    ICE_MCAST_VLAN_PROMISC_BITS, vid);
	if (ret) {
		netdev_err(netdev, "Error clearing multicast promiscuous mode on VSI %i\n",
			   vsi->vsi_num);
		vsi->current_netdev_flags |= IFF_ALLMULTI;
	}

	vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);

	 
	vlan = ICE_VLAN(be16_to_cpu(proto), vid, 0);
	ret = vlan_ops->del_vlan(vsi, &vlan);
	if (ret)
		goto finish;

	 
	if (vsi->current_netdev_flags & IFF_ALLMULTI)
		ice_fltr_clear_vsi_promisc(&vsi->back->hw, vsi->idx,
					   ICE_MCAST_VLAN_PROMISC_BITS, vid);

	if (!ice_vsi_has_non_zero_vlans(vsi)) {
		 
		if (vsi->current_netdev_flags & IFF_ALLMULTI) {
			ice_fltr_clear_vsi_promisc(&vsi->back->hw, vsi->idx,
						   ICE_MCAST_VLAN_PROMISC_BITS,
						   0);
			ice_fltr_set_vsi_promisc(&vsi->back->hw, vsi->idx,
						 ICE_MCAST_PROMISC_BITS, 0);
		}
	}

finish:
	clear_bit(ICE_CFG_BUSY, vsi->state);

	return ret;
}

 
static void ice_rep_indr_tc_block_unbind(void *cb_priv)
{
	struct ice_indr_block_priv *indr_priv = cb_priv;

	list_del(&indr_priv->list);
	kfree(indr_priv);
}

 
static void ice_tc_indir_block_unregister(struct ice_vsi *vsi)
{
	struct ice_netdev_priv *np = netdev_priv(vsi->netdev);

	flow_indr_dev_unregister(ice_indr_setup_tc_cb, np,
				 ice_rep_indr_tc_block_unbind);
}

 
static int ice_tc_indir_block_register(struct ice_vsi *vsi)
{
	struct ice_netdev_priv *np;

	if (!vsi || !vsi->netdev)
		return -EINVAL;

	np = netdev_priv(vsi->netdev);

	INIT_LIST_HEAD(&np->tc_indr_block_priv_list);
	return flow_indr_dev_register(ice_indr_setup_tc_cb, np);
}

 
static u16
ice_get_avail_q_count(unsigned long *pf_qmap, struct mutex *lock, u16 size)
{
	unsigned long bit;
	u16 count = 0;

	mutex_lock(lock);
	for_each_clear_bit(bit, pf_qmap, size)
		count++;
	mutex_unlock(lock);

	return count;
}

 
u16 ice_get_avail_txq_count(struct ice_pf *pf)
{
	return ice_get_avail_q_count(pf->avail_txqs, &pf->avail_q_mutex,
				     pf->max_pf_txqs);
}

 
u16 ice_get_avail_rxq_count(struct ice_pf *pf)
{
	return ice_get_avail_q_count(pf->avail_rxqs, &pf->avail_q_mutex,
				     pf->max_pf_rxqs);
}

 
static void ice_deinit_pf(struct ice_pf *pf)
{
	ice_service_task_stop(pf);
	mutex_destroy(&pf->lag_mutex);
	mutex_destroy(&pf->adev_mutex);
	mutex_destroy(&pf->sw_mutex);
	mutex_destroy(&pf->tc_mutex);
	mutex_destroy(&pf->avail_q_mutex);
	mutex_destroy(&pf->vfs.table_lock);

	if (pf->avail_txqs) {
		bitmap_free(pf->avail_txqs);
		pf->avail_txqs = NULL;
	}

	if (pf->avail_rxqs) {
		bitmap_free(pf->avail_rxqs);
		pf->avail_rxqs = NULL;
	}

	if (pf->ptp.clock)
		ptp_clock_unregister(pf->ptp.clock);
}

 
static void ice_set_pf_caps(struct ice_pf *pf)
{
	struct ice_hw_func_caps *func_caps = &pf->hw.func_caps;

	clear_bit(ICE_FLAG_RDMA_ENA, pf->flags);
	if (func_caps->common_cap.rdma)
		set_bit(ICE_FLAG_RDMA_ENA, pf->flags);
	clear_bit(ICE_FLAG_DCB_CAPABLE, pf->flags);
	if (func_caps->common_cap.dcb)
		set_bit(ICE_FLAG_DCB_CAPABLE, pf->flags);
	clear_bit(ICE_FLAG_SRIOV_CAPABLE, pf->flags);
	if (func_caps->common_cap.sr_iov_1_1) {
		set_bit(ICE_FLAG_SRIOV_CAPABLE, pf->flags);
		pf->vfs.num_supported = min_t(int, func_caps->num_allocd_vfs,
					      ICE_MAX_SRIOV_VFS);
	}
	clear_bit(ICE_FLAG_RSS_ENA, pf->flags);
	if (func_caps->common_cap.rss_table_size)
		set_bit(ICE_FLAG_RSS_ENA, pf->flags);

	clear_bit(ICE_FLAG_FD_ENA, pf->flags);
	if (func_caps->fd_fltr_guar > 0 || func_caps->fd_fltr_best_effort > 0) {
		u16 unused;

		 
		pf->ctrl_vsi_idx = ICE_NO_VSI;
		set_bit(ICE_FLAG_FD_ENA, pf->flags);
		 
		ice_alloc_fd_guar_item(&pf->hw, &unused,
				       func_caps->fd_fltr_guar);
		 
		ice_alloc_fd_shrd_item(&pf->hw, &unused,
				       func_caps->fd_fltr_best_effort);
	}

	clear_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags);
	if (func_caps->common_cap.ieee_1588)
		set_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags);

	pf->max_pf_txqs = func_caps->common_cap.num_txq;
	pf->max_pf_rxqs = func_caps->common_cap.num_rxq;
}

 
static int ice_init_pf(struct ice_pf *pf)
{
	ice_set_pf_caps(pf);

	mutex_init(&pf->sw_mutex);
	mutex_init(&pf->tc_mutex);
	mutex_init(&pf->adev_mutex);
	mutex_init(&pf->lag_mutex);

	INIT_HLIST_HEAD(&pf->aq_wait_list);
	spin_lock_init(&pf->aq_wait_lock);
	init_waitqueue_head(&pf->aq_wait_queue);

	init_waitqueue_head(&pf->reset_wait_queue);

	 
	timer_setup(&pf->serv_tmr, ice_service_timer, 0);
	pf->serv_tmr_period = HZ;
	INIT_WORK(&pf->serv_task, ice_service_task);
	clear_bit(ICE_SERVICE_SCHED, pf->state);

	mutex_init(&pf->avail_q_mutex);
	pf->avail_txqs = bitmap_zalloc(pf->max_pf_txqs, GFP_KERNEL);
	if (!pf->avail_txqs)
		return -ENOMEM;

	pf->avail_rxqs = bitmap_zalloc(pf->max_pf_rxqs, GFP_KERNEL);
	if (!pf->avail_rxqs) {
		bitmap_free(pf->avail_txqs);
		pf->avail_txqs = NULL;
		return -ENOMEM;
	}

	mutex_init(&pf->vfs.table_lock);
	hash_init(pf->vfs.table);
	ice_mbx_init_snapshot(&pf->hw);

	return 0;
}

 
bool ice_is_wol_supported(struct ice_hw *hw)
{
	u16 wol_ctrl;

	 
	if (ice_read_sr_word(hw, ICE_SR_NVM_WOL_CFG, &wol_ctrl))
		return false;

	return !(BIT(hw->port_info->lport) & wol_ctrl);
}

 
int ice_vsi_recfg_qs(struct ice_vsi *vsi, int new_rx, int new_tx, bool locked)
{
	struct ice_pf *pf = vsi->back;
	int err = 0, timeout = 50;

	if (!new_rx && !new_tx)
		return -EINVAL;

	while (test_and_set_bit(ICE_CFG_BUSY, pf->state)) {
		timeout--;
		if (!timeout)
			return -EBUSY;
		usleep_range(1000, 2000);
	}

	if (new_tx)
		vsi->req_txq = (u16)new_tx;
	if (new_rx)
		vsi->req_rxq = (u16)new_rx;

	 
	if (!netif_running(vsi->netdev)) {
		ice_vsi_rebuild(vsi, ICE_VSI_FLAG_NO_INIT);
		dev_dbg(ice_pf_to_dev(pf), "Link is down, queue count change happens when link is brought up\n");
		goto done;
	}

	ice_vsi_close(vsi);
	ice_vsi_rebuild(vsi, ICE_VSI_FLAG_NO_INIT);
	ice_pf_dcb_recfg(pf, locked);
	ice_vsi_open(vsi);
done:
	clear_bit(ICE_CFG_BUSY, pf->state);
	return err;
}

 
static void ice_set_safe_mode_vlan_cfg(struct ice_pf *pf)
{
	struct ice_vsi *vsi = ice_get_main_vsi(pf);
	struct ice_vsi_ctx *ctxt;
	struct ice_hw *hw;
	int status;

	if (!vsi)
		return;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return;

	hw = &pf->hw;
	ctxt->info = vsi->info;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_VLAN_VALID |
			    ICE_AQ_VSI_PROP_SECURITY_VALID |
			    ICE_AQ_VSI_PROP_SW_VALID);

	 
	ctxt->info.sec_flags &= ~(ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA <<
				  ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S);

	 
	ctxt->info.sw_flags2 &= ~ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;

	 
	ctxt->info.inner_vlan_flags = ICE_AQ_VSI_INNER_VLAN_TX_MODE_ALL |
		ICE_AQ_VSI_INNER_VLAN_EMODE_NOTHING;

	status = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (status) {
		dev_err(ice_pf_to_dev(vsi->back), "Failed to update VSI for safe mode VLANs, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));
	} else {
		vsi->info.sec_flags = ctxt->info.sec_flags;
		vsi->info.sw_flags2 = ctxt->info.sw_flags2;
		vsi->info.inner_vlan_flags = ctxt->info.inner_vlan_flags;
	}

	kfree(ctxt);
}

 
static void ice_log_pkg_init(struct ice_hw *hw, enum ice_ddp_state state)
{
	struct ice_pf *pf = hw->back;
	struct device *dev;

	dev = ice_pf_to_dev(pf);

	switch (state) {
	case ICE_DDP_PKG_SUCCESS:
		dev_info(dev, "The DDP package was successfully loaded: %s version %d.%d.%d.%d\n",
			 hw->active_pkg_name,
			 hw->active_pkg_ver.major,
			 hw->active_pkg_ver.minor,
			 hw->active_pkg_ver.update,
			 hw->active_pkg_ver.draft);
		break;
	case ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED:
		dev_info(dev, "DDP package already present on device: %s version %d.%d.%d.%d\n",
			 hw->active_pkg_name,
			 hw->active_pkg_ver.major,
			 hw->active_pkg_ver.minor,
			 hw->active_pkg_ver.update,
			 hw->active_pkg_ver.draft);
		break;
	case ICE_DDP_PKG_ALREADY_LOADED_NOT_SUPPORTED:
		dev_err(dev, "The device has a DDP package that is not supported by the driver.  The device has package '%s' version %d.%d.x.x.  The driver requires version %d.%d.x.x.  Entering Safe Mode.\n",
			hw->active_pkg_name,
			hw->active_pkg_ver.major,
			hw->active_pkg_ver.minor,
			ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
		break;
	case ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED:
		dev_info(dev, "The driver could not load the DDP package file because a compatible DDP package is already present on the device.  The device has package '%s' version %d.%d.%d.%d.  The package file found by the driver: '%s' version %d.%d.%d.%d.\n",
			 hw->active_pkg_name,
			 hw->active_pkg_ver.major,
			 hw->active_pkg_ver.minor,
			 hw->active_pkg_ver.update,
			 hw->active_pkg_ver.draft,
			 hw->pkg_name,
			 hw->pkg_ver.major,
			 hw->pkg_ver.minor,
			 hw->pkg_ver.update,
			 hw->pkg_ver.draft);
		break;
	case ICE_DDP_PKG_FW_MISMATCH:
		dev_err(dev, "The firmware loaded on the device is not compatible with the DDP package.  Please update the device's NVM.  Entering safe mode.\n");
		break;
	case ICE_DDP_PKG_INVALID_FILE:
		dev_err(dev, "The DDP package file is invalid. Entering Safe Mode.\n");
		break;
	case ICE_DDP_PKG_FILE_VERSION_TOO_HIGH:
		dev_err(dev, "The DDP package file version is higher than the driver supports.  Please use an updated driver.  Entering Safe Mode.\n");
		break;
	case ICE_DDP_PKG_FILE_VERSION_TOO_LOW:
		dev_err(dev, "The DDP package file version is lower than the driver supports.  The driver requires version %d.%d.x.x.  Please use an updated DDP Package file.  Entering Safe Mode.\n",
			ICE_PKG_SUPP_VER_MAJ, ICE_PKG_SUPP_VER_MNR);
		break;
	case ICE_DDP_PKG_FILE_SIGNATURE_INVALID:
		dev_err(dev, "The DDP package could not be loaded because its signature is not valid.  Please use a valid DDP Package.  Entering Safe Mode.\n");
		break;
	case ICE_DDP_PKG_FILE_REVISION_TOO_LOW:
		dev_err(dev, "The DDP Package could not be loaded because its security revision is too low.  Please use an updated DDP Package.  Entering Safe Mode.\n");
		break;
	case ICE_DDP_PKG_LOAD_ERROR:
		dev_err(dev, "An error occurred on the device while loading the DDP package.  The device will be reset.\n");
		 
		if (ice_check_reset(hw))
			dev_err(dev, "Error resetting device. Please reload the driver\n");
		break;
	case ICE_DDP_PKG_ERR:
	default:
		dev_err(dev, "An unknown error occurred when loading the DDP package.  Entering Safe Mode.\n");
		break;
	}
}

 
static void
ice_load_pkg(const struct firmware *firmware, struct ice_pf *pf)
{
	enum ice_ddp_state state = ICE_DDP_PKG_ERR;
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;

	 
	if (firmware && !hw->pkg_copy) {
		state = ice_copy_and_init_pkg(hw, firmware->data,
					      firmware->size);
		ice_log_pkg_init(hw, state);
	} else if (!firmware && hw->pkg_copy) {
		 
		state = ice_init_pkg(hw, hw->pkg_copy, hw->pkg_size);
		ice_log_pkg_init(hw, state);
	} else {
		dev_err(dev, "The DDP package file failed to load. Entering Safe Mode.\n");
	}

	if (!ice_is_init_pkg_successful(state)) {
		 
		clear_bit(ICE_FLAG_ADV_FEATURES, pf->flags);
		return;
	}

	 
	set_bit(ICE_FLAG_ADV_FEATURES, pf->flags);
}

 
static void ice_verify_cacheline_size(struct ice_pf *pf)
{
	if (rd32(&pf->hw, GLPCI_CNF2) & GLPCI_CNF2_CACHELINE_SIZE_M)
		dev_warn(ice_pf_to_dev(pf), "%d Byte cache line assumption is invalid, driver may have Tx timeouts!\n",
			 ICE_CACHE_LINE_BYTES);
}

 
static int ice_send_version(struct ice_pf *pf)
{
	struct ice_driver_ver dv;

	dv.major_ver = 0xff;
	dv.minor_ver = 0xff;
	dv.build_ver = 0xff;
	dv.subbuild_ver = 0;
	strscpy((char *)dv.driver_string, UTS_RELEASE,
		sizeof(dv.driver_string));
	return ice_aq_send_driver_ver(&pf->hw, &dv, NULL);
}

 
static int ice_init_fdir(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vsi *ctrl_vsi;
	int err;

	 
	ctrl_vsi = ice_ctrl_vsi_setup(pf, pf->hw.port_info);
	if (!ctrl_vsi) {
		dev_dbg(dev, "could not create control VSI\n");
		return -ENOMEM;
	}

	err = ice_vsi_open_ctrl(ctrl_vsi);
	if (err) {
		dev_dbg(dev, "could not open control VSI\n");
		goto err_vsi_open;
	}

	mutex_init(&pf->hw.fdir_fltr_lock);

	err = ice_fdir_create_dflt_rules(pf);
	if (err)
		goto err_fdir_rule;

	return 0;

err_fdir_rule:
	ice_fdir_release_flows(&pf->hw);
	ice_vsi_close(ctrl_vsi);
err_vsi_open:
	ice_vsi_release(ctrl_vsi);
	if (pf->ctrl_vsi_idx != ICE_NO_VSI) {
		pf->vsi[pf->ctrl_vsi_idx] = NULL;
		pf->ctrl_vsi_idx = ICE_NO_VSI;
	}
	return err;
}

static void ice_deinit_fdir(struct ice_pf *pf)
{
	struct ice_vsi *vsi = ice_get_ctrl_vsi(pf);

	if (!vsi)
		return;

	ice_vsi_manage_fdir(vsi, false);
	ice_vsi_release(vsi);
	if (pf->ctrl_vsi_idx != ICE_NO_VSI) {
		pf->vsi[pf->ctrl_vsi_idx] = NULL;
		pf->ctrl_vsi_idx = ICE_NO_VSI;
	}

	mutex_destroy(&(&pf->hw)->fdir_fltr_lock);
}

 
static char *ice_get_opt_fw_name(struct ice_pf *pf)
{
	 
	struct pci_dev *pdev = pf->pdev;
	char *opt_fw_filename;
	u64 dsn;

	 
	dsn = pci_get_dsn(pdev);
	if (!dsn)
		return NULL;

	opt_fw_filename = kzalloc(NAME_MAX, GFP_KERNEL);
	if (!opt_fw_filename)
		return NULL;

	snprintf(opt_fw_filename, NAME_MAX, "%sice-%016llx.pkg",
		 ICE_DDP_PKG_PATH, dsn);

	return opt_fw_filename;
}

 
static void ice_request_fw(struct ice_pf *pf)
{
	char *opt_fw_filename = ice_get_opt_fw_name(pf);
	const struct firmware *firmware = NULL;
	struct device *dev = ice_pf_to_dev(pf);
	int err = 0;

	 
	if (opt_fw_filename) {
		err = firmware_request_nowarn(&firmware, opt_fw_filename, dev);
		if (err) {
			kfree(opt_fw_filename);
			goto dflt_pkg_load;
		}

		 
		ice_load_pkg(firmware, pf);
		kfree(opt_fw_filename);
		release_firmware(firmware);
		return;
	}

dflt_pkg_load:
	err = request_firmware(&firmware, ICE_DDP_PKG_FILE, dev);
	if (err) {
		dev_err(dev, "The DDP package file was not found or could not be read. Entering Safe Mode\n");
		return;
	}

	 
	ice_load_pkg(firmware, pf);
	release_firmware(firmware);
}

 
static void ice_print_wake_reason(struct ice_pf *pf)
{
	u32 wus = pf->wakeup_reason;
	const char *wake_str;

	 
	if (!wus)
		return;

	if (wus & PFPM_WUS_LNKC_M)
		wake_str = "Link\n";
	else if (wus & PFPM_WUS_MAG_M)
		wake_str = "Magic Packet\n";
	else if (wus & PFPM_WUS_MNG_M)
		wake_str = "Management\n";
	else if (wus & PFPM_WUS_FW_RST_WK_M)
		wake_str = "Firmware Reset\n";
	else
		wake_str = "Unknown\n";

	dev_info(ice_pf_to_dev(pf), "Wake reason: %s", wake_str);
}

 
static int ice_register_netdev(struct ice_vsi *vsi)
{
	int err;

	if (!vsi || !vsi->netdev)
		return -EIO;

	err = register_netdev(vsi->netdev);
	if (err)
		return err;

	set_bit(ICE_VSI_NETDEV_REGISTERED, vsi->state);
	netif_carrier_off(vsi->netdev);
	netif_tx_stop_all_queues(vsi->netdev);

	return 0;
}

static void ice_unregister_netdev(struct ice_vsi *vsi)
{
	if (!vsi || !vsi->netdev)
		return;

	unregister_netdev(vsi->netdev);
	clear_bit(ICE_VSI_NETDEV_REGISTERED, vsi->state);
}

 
static int ice_cfg_netdev(struct ice_vsi *vsi)
{
	struct ice_netdev_priv *np;
	struct net_device *netdev;
	u8 mac_addr[ETH_ALEN];

	netdev = alloc_etherdev_mqs(sizeof(*np), vsi->alloc_txq,
				    vsi->alloc_rxq);
	if (!netdev)
		return -ENOMEM;

	set_bit(ICE_VSI_NETDEV_ALLOCD, vsi->state);
	vsi->netdev = netdev;
	np = netdev_priv(netdev);
	np->vsi = vsi;

	ice_set_netdev_features(netdev);
	ice_set_ops(vsi);

	if (vsi->type == ICE_VSI_PF) {
		SET_NETDEV_DEV(netdev, ice_pf_to_dev(vsi->back));
		ether_addr_copy(mac_addr, vsi->port_info->mac.perm_addr);
		eth_hw_addr_set(netdev, mac_addr);
	}

	netdev->priv_flags |= IFF_UNICAST_FLT;

	 
	ice_vsi_cfg_netdev_tc(vsi, vsi->tc_cfg.ena_tc);

	netdev->max_mtu = ICE_MAX_MTU;

	return 0;
}

static void ice_decfg_netdev(struct ice_vsi *vsi)
{
	clear_bit(ICE_VSI_NETDEV_ALLOCD, vsi->state);
	free_netdev(vsi->netdev);
	vsi->netdev = NULL;
}

static int ice_start_eth(struct ice_vsi *vsi)
{
	int err;

	err = ice_init_mac_fltr(vsi->back);
	if (err)
		return err;

	err = ice_vsi_open(vsi);
	if (err)
		ice_fltr_remove_all(vsi);

	return err;
}

static void ice_stop_eth(struct ice_vsi *vsi)
{
	ice_fltr_remove_all(vsi);
	ice_vsi_close(vsi);
}

static int ice_init_eth(struct ice_pf *pf)
{
	struct ice_vsi *vsi = ice_get_main_vsi(pf);
	int err;

	if (!vsi)
		return -EINVAL;

	 
	INIT_LIST_HEAD(&vsi->ch_list);

	err = ice_cfg_netdev(vsi);
	if (err)
		return err;
	 
	ice_dcbnl_setup(vsi);

	err = ice_init_mac_fltr(pf);
	if (err)
		goto err_init_mac_fltr;

	err = ice_devlink_create_pf_port(pf);
	if (err)
		goto err_devlink_create_pf_port;

	SET_NETDEV_DEVLINK_PORT(vsi->netdev, &pf->devlink_port);

	err = ice_register_netdev(vsi);
	if (err)
		goto err_register_netdev;

	err = ice_tc_indir_block_register(vsi);
	if (err)
		goto err_tc_indir_block_register;

	ice_napi_add(vsi);

	return 0;

err_tc_indir_block_register:
	ice_unregister_netdev(vsi);
err_register_netdev:
	ice_devlink_destroy_pf_port(pf);
err_devlink_create_pf_port:
err_init_mac_fltr:
	ice_decfg_netdev(vsi);
	return err;
}

static void ice_deinit_eth(struct ice_pf *pf)
{
	struct ice_vsi *vsi = ice_get_main_vsi(pf);

	if (!vsi)
		return;

	ice_vsi_close(vsi);
	ice_unregister_netdev(vsi);
	ice_devlink_destroy_pf_port(pf);
	ice_tc_indir_block_unregister(vsi);
	ice_decfg_netdev(vsi);
}

 
static int ice_wait_for_fw(struct ice_hw *hw, u32 timeout)
{
	int fw_loading;
	u32 elapsed = 0;

	while (elapsed <= timeout) {
		fw_loading = rd32(hw, GL_MNG_FWSM) & GL_MNG_FWSM_FW_LOADING_M;

		 
		if (fw_loading) {
			elapsed += 100;
			msleep(100);
			continue;
		}
		return 0;
	}

	return -ETIMEDOUT;
}

static int ice_init_dev(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	int err;

	err = ice_init_hw(hw);
	if (err) {
		dev_err(dev, "ice_init_hw failed: %d\n", err);
		return err;
	}

	 
	if (ice_is_pf_c827(hw)) {
		err = ice_wait_for_fw(hw, 30000);
		if (err) {
			dev_err(dev, "ice_wait_for_fw timed out");
			return err;
		}
	}

	ice_init_feature_support(pf);

	ice_request_fw(pf);

	 
	if (ice_is_safe_mode(pf)) {
		 
		ice_set_safe_mode_caps(hw);
	}

	err = ice_init_pf(pf);
	if (err) {
		dev_err(dev, "ice_init_pf failed: %d\n", err);
		goto err_init_pf;
	}

	pf->hw.udp_tunnel_nic.set_port = ice_udp_tunnel_set_port;
	pf->hw.udp_tunnel_nic.unset_port = ice_udp_tunnel_unset_port;
	pf->hw.udp_tunnel_nic.flags = UDP_TUNNEL_NIC_INFO_MAY_SLEEP;
	pf->hw.udp_tunnel_nic.shared = &pf->hw.udp_tunnel_shared;
	if (pf->hw.tnl.valid_count[TNL_VXLAN]) {
		pf->hw.udp_tunnel_nic.tables[0].n_entries =
			pf->hw.tnl.valid_count[TNL_VXLAN];
		pf->hw.udp_tunnel_nic.tables[0].tunnel_types =
			UDP_TUNNEL_TYPE_VXLAN;
	}
	if (pf->hw.tnl.valid_count[TNL_GENEVE]) {
		pf->hw.udp_tunnel_nic.tables[1].n_entries =
			pf->hw.tnl.valid_count[TNL_GENEVE];
		pf->hw.udp_tunnel_nic.tables[1].tunnel_types =
			UDP_TUNNEL_TYPE_GENEVE;
	}

	err = ice_init_interrupt_scheme(pf);
	if (err) {
		dev_err(dev, "ice_init_interrupt_scheme failed: %d\n", err);
		err = -EIO;
		goto err_init_interrupt_scheme;
	}

	 
	err = ice_req_irq_msix_misc(pf);
	if (err) {
		dev_err(dev, "setup of misc vector failed: %d\n", err);
		goto err_req_irq_msix_misc;
	}

	return 0;

err_req_irq_msix_misc:
	ice_clear_interrupt_scheme(pf);
err_init_interrupt_scheme:
	ice_deinit_pf(pf);
err_init_pf:
	ice_deinit_hw(hw);
	return err;
}

static void ice_deinit_dev(struct ice_pf *pf)
{
	ice_free_irq_msix_misc(pf);
	ice_deinit_pf(pf);
	ice_deinit_hw(&pf->hw);

	 
	ice_reset(&pf->hw, ICE_RESET_PFR);
	pci_wait_for_pending_transaction(pf->pdev);
	ice_clear_interrupt_scheme(pf);
}

static void ice_init_features(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);

	if (ice_is_safe_mode(pf))
		return;

	 
	if (test_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags))
		ice_ptp_init(pf);

	if (ice_is_feature_supported(pf, ICE_F_GNSS))
		ice_gnss_init(pf);

	 
	if (ice_init_fdir(pf))
		dev_err(dev, "could not initialize flow director\n");

	 
	if (ice_init_pf_dcb(pf, false)) {
		clear_bit(ICE_FLAG_DCB_CAPABLE, pf->flags);
		clear_bit(ICE_FLAG_DCB_ENA, pf->flags);
	} else {
		ice_cfg_lldp_mib_change(&pf->hw, true);
	}

	if (ice_init_lag(pf))
		dev_warn(dev, "Failed to init link aggregation support\n");
}

static void ice_deinit_features(struct ice_pf *pf)
{
	if (ice_is_safe_mode(pf))
		return;

	ice_deinit_lag(pf);
	if (test_bit(ICE_FLAG_DCB_CAPABLE, pf->flags))
		ice_cfg_lldp_mib_change(&pf->hw, false);
	ice_deinit_fdir(pf);
	if (ice_is_feature_supported(pf, ICE_F_GNSS))
		ice_gnss_exit(pf);
	if (test_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags))
		ice_ptp_release(pf);
}

static void ice_init_wakeup(struct ice_pf *pf)
{
	 
	pf->wakeup_reason = rd32(&pf->hw, PFPM_WUS);

	 
	ice_print_wake_reason(pf);

	 
	wr32(&pf->hw, PFPM_WUS, U32_MAX);

	 
	device_set_wakeup_enable(ice_pf_to_dev(pf), false);
}

static int ice_init_link(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	int err;

	err = ice_init_link_events(pf->hw.port_info);
	if (err) {
		dev_err(dev, "ice_init_link_events failed: %d\n", err);
		return err;
	}

	 
	err = ice_init_nvm_phy_type(pf->hw.port_info);
	if (err)
		dev_err(dev, "ice_init_nvm_phy_type failed: %d\n", err);

	 
	err = ice_update_link_info(pf->hw.port_info);
	if (err)
		dev_err(dev, "ice_update_link_info failed: %d\n", err);

	ice_init_link_dflt_override(pf->hw.port_info);

	ice_check_link_cfg_err(pf,
			       pf->hw.port_info->phy.link_info.link_cfg_err);

	 
	if (pf->hw.port_info->phy.link_info.link_info &
	    ICE_AQ_MEDIA_AVAILABLE) {
		 
		err = ice_init_phy_user_cfg(pf->hw.port_info);
		if (err)
			dev_err(dev, "ice_init_phy_user_cfg failed: %d\n", err);

		if (!test_bit(ICE_FLAG_LINK_DOWN_ON_CLOSE_ENA, pf->flags)) {
			struct ice_vsi *vsi = ice_get_main_vsi(pf);

			if (vsi)
				ice_configure_phy(vsi);
		}
	} else {
		set_bit(ICE_FLAG_NO_MEDIA, pf->flags);
	}

	return err;
}

static int ice_init_pf_sw(struct ice_pf *pf)
{
	bool dvm = ice_is_dvm_ena(&pf->hw);
	struct ice_vsi *vsi;
	int err;

	 
	pf->first_sw = kzalloc(sizeof(*pf->first_sw), GFP_KERNEL);
	if (!pf->first_sw)
		return -ENOMEM;

	if (pf->hw.evb_veb)
		pf->first_sw->bridge_mode = BRIDGE_MODE_VEB;
	else
		pf->first_sw->bridge_mode = BRIDGE_MODE_VEPA;

	pf->first_sw->pf = pf;

	 
	pf->first_sw->sw_id = pf->hw.port_info->sw_id;

	err = ice_aq_set_port_params(pf->hw.port_info, dvm, NULL);
	if (err)
		goto err_aq_set_port_params;

	vsi = ice_pf_vsi_setup(pf, pf->hw.port_info);
	if (!vsi) {
		err = -ENOMEM;
		goto err_pf_vsi_setup;
	}

	return 0;

err_pf_vsi_setup:
err_aq_set_port_params:
	kfree(pf->first_sw);
	return err;
}

static void ice_deinit_pf_sw(struct ice_pf *pf)
{
	struct ice_vsi *vsi = ice_get_main_vsi(pf);

	if (!vsi)
		return;

	ice_vsi_release(vsi);
	kfree(pf->first_sw);
}

static int ice_alloc_vsis(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);

	pf->num_alloc_vsi = pf->hw.func_caps.guar_num_vsi;
	if (!pf->num_alloc_vsi)
		return -EIO;

	if (pf->num_alloc_vsi > UDP_TUNNEL_NIC_MAX_SHARING_DEVICES) {
		dev_warn(dev,
			 "limiting the VSI count due to UDP tunnel limitation %d > %d\n",
			 pf->num_alloc_vsi, UDP_TUNNEL_NIC_MAX_SHARING_DEVICES);
		pf->num_alloc_vsi = UDP_TUNNEL_NIC_MAX_SHARING_DEVICES;
	}

	pf->vsi = devm_kcalloc(dev, pf->num_alloc_vsi, sizeof(*pf->vsi),
			       GFP_KERNEL);
	if (!pf->vsi)
		return -ENOMEM;

	pf->vsi_stats = devm_kcalloc(dev, pf->num_alloc_vsi,
				     sizeof(*pf->vsi_stats), GFP_KERNEL);
	if (!pf->vsi_stats) {
		devm_kfree(dev, pf->vsi);
		return -ENOMEM;
	}

	return 0;
}

static void ice_dealloc_vsis(struct ice_pf *pf)
{
	devm_kfree(ice_pf_to_dev(pf), pf->vsi_stats);
	pf->vsi_stats = NULL;

	pf->num_alloc_vsi = 0;
	devm_kfree(ice_pf_to_dev(pf), pf->vsi);
	pf->vsi = NULL;
}

static int ice_init_devlink(struct ice_pf *pf)
{
	int err;

	err = ice_devlink_register_params(pf);
	if (err)
		return err;

	ice_devlink_init_regions(pf);
	ice_devlink_register(pf);

	return 0;
}

static void ice_deinit_devlink(struct ice_pf *pf)
{
	ice_devlink_unregister(pf);
	ice_devlink_destroy_regions(pf);
	ice_devlink_unregister_params(pf);
}

static int ice_init(struct ice_pf *pf)
{
	int err;

	err = ice_init_dev(pf);
	if (err)
		return err;

	err = ice_alloc_vsis(pf);
	if (err)
		goto err_alloc_vsis;

	err = ice_init_pf_sw(pf);
	if (err)
		goto err_init_pf_sw;

	ice_init_wakeup(pf);

	err = ice_init_link(pf);
	if (err)
		goto err_init_link;

	err = ice_send_version(pf);
	if (err)
		goto err_init_link;

	ice_verify_cacheline_size(pf);

	if (ice_is_safe_mode(pf))
		ice_set_safe_mode_vlan_cfg(pf);
	else
		 
		pcie_print_link_status(pf->pdev);

	 
	clear_bit(ICE_DOWN, pf->state);
	clear_bit(ICE_SERVICE_DIS, pf->state);

	 
	mod_timer(&pf->serv_tmr, round_jiffies(jiffies + pf->serv_tmr_period));

	return 0;

err_init_link:
	ice_deinit_pf_sw(pf);
err_init_pf_sw:
	ice_dealloc_vsis(pf);
err_alloc_vsis:
	ice_deinit_dev(pf);
	return err;
}

static void ice_deinit(struct ice_pf *pf)
{
	set_bit(ICE_SERVICE_DIS, pf->state);
	set_bit(ICE_DOWN, pf->state);

	ice_deinit_pf_sw(pf);
	ice_dealloc_vsis(pf);
	ice_deinit_dev(pf);
}

 
int ice_load(struct ice_pf *pf)
{
	struct ice_vsi_cfg_params params = {};
	struct ice_vsi *vsi;
	int err;

	err = ice_init_dev(pf);
	if (err)
		return err;

	vsi = ice_get_main_vsi(pf);

	params = ice_vsi_to_params(vsi);
	params.flags = ICE_VSI_FLAG_INIT;

	rtnl_lock();
	err = ice_vsi_cfg(vsi, &params);
	if (err)
		goto err_vsi_cfg;

	err = ice_start_eth(ice_get_main_vsi(pf));
	if (err)
		goto err_start_eth;
	rtnl_unlock();

	err = ice_init_rdma(pf);
	if (err)
		goto err_init_rdma;

	ice_init_features(pf);
	ice_service_task_restart(pf);

	clear_bit(ICE_DOWN, pf->state);

	return 0;

err_init_rdma:
	ice_vsi_close(ice_get_main_vsi(pf));
	rtnl_lock();
err_start_eth:
	ice_vsi_decfg(ice_get_main_vsi(pf));
err_vsi_cfg:
	rtnl_unlock();
	ice_deinit_dev(pf);
	return err;
}

 
void ice_unload(struct ice_pf *pf)
{
	ice_deinit_features(pf);
	ice_deinit_rdma(pf);
	rtnl_lock();
	ice_stop_eth(ice_get_main_vsi(pf));
	ice_vsi_decfg(ice_get_main_vsi(pf));
	rtnl_unlock();
	ice_deinit_dev(pf);
}

 
static int
ice_probe(struct pci_dev *pdev, const struct pci_device_id __always_unused *ent)
{
	struct device *dev = &pdev->dev;
	struct ice_pf *pf;
	struct ice_hw *hw;
	int err;

	if (pdev->is_virtfn) {
		dev_err(dev, "can't probe a virtual function\n");
		return -EINVAL;
	}

	 
	if (is_kdump_kernel()) {
		pci_save_state(pdev);
		pci_clear_master(pdev);
		err = pcie_flr(pdev);
		if (err)
			return err;
		pci_restore_state(pdev);
	}

	 
	err = pcim_enable_device(pdev);
	if (err)
		return err;

	err = pcim_iomap_regions(pdev, BIT(ICE_BAR0), dev_driver_string(dev));
	if (err) {
		dev_err(dev, "BAR0 I/O map error %d\n", err);
		return err;
	}

	pf = ice_allocate_pf(dev);
	if (!pf)
		return -ENOMEM;

	 
	pf->aux_idx = -1;

	 
	err = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64));
	if (err) {
		dev_err(dev, "DMA configuration failed: 0x%x\n", err);
		return err;
	}

	pci_set_master(pdev);

	pf->pdev = pdev;
	pci_set_drvdata(pdev, pf);
	set_bit(ICE_DOWN, pf->state);
	 
	set_bit(ICE_SERVICE_DIS, pf->state);

	hw = &pf->hw;
	hw->hw_addr = pcim_iomap_table(pdev)[ICE_BAR0];
	pci_save_state(pdev);

	hw->back = pf;
	hw->port_info = NULL;
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	pci_read_config_byte(pdev, PCI_REVISION_ID, &hw->revision_id);
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;
	hw->bus.device = PCI_SLOT(pdev->devfn);
	hw->bus.func = PCI_FUNC(pdev->devfn);
	ice_set_ctrlq_len(hw);

	pf->msg_enable = netif_msg_init(debug, ICE_DFLT_NETIF_M);

#ifndef CONFIG_DYNAMIC_DEBUG
	if (debug < -1)
		hw->debug_mask = debug;
#endif

	err = ice_init(pf);
	if (err)
		goto err_init;

	err = ice_init_eth(pf);
	if (err)
		goto err_init_eth;

	err = ice_init_rdma(pf);
	if (err)
		goto err_init_rdma;

	err = ice_init_devlink(pf);
	if (err)
		goto err_init_devlink;

	ice_init_features(pf);

	return 0;

err_init_devlink:
	ice_deinit_rdma(pf);
err_init_rdma:
	ice_deinit_eth(pf);
err_init_eth:
	ice_deinit(pf);
err_init:
	pci_disable_device(pdev);
	return err;
}

 
static void ice_set_wake(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	bool wol = pf->wol_ena;

	 
	wr32(hw, PFPM_WUS, U32_MAX);

	 
	wr32(hw, PFPM_APM, wol ? PFPM_APM_APME_M : 0);

	 
	wr32(hw, PFPM_WUFC, wol ? PFPM_WUFC_MAG_M : 0);
}

 
static void ice_setup_mc_magic_wake(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	u8 mac_addr[ETH_ALEN];
	struct ice_vsi *vsi;
	int status;
	u8 flags;

	if (!pf->wol_ena)
		return;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return;

	 
	if (vsi->netdev)
		ether_addr_copy(mac_addr, vsi->netdev->dev_addr);
	else
		ether_addr_copy(mac_addr, vsi->port_info->mac.perm_addr);

	flags = ICE_AQC_MAN_MAC_WR_MC_MAG_EN |
		ICE_AQC_MAN_MAC_UPDATE_LAA_WOL |
		ICE_AQC_MAN_MAC_WR_WOL_LAA_PFR_KEEP;

	status = ice_aq_manage_mac_write(hw, mac_addr, flags, NULL);
	if (status)
		dev_err(dev, "Failed to enable Multicast Magic Packet wake, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));
}

 
static void ice_remove(struct pci_dev *pdev)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);
	int i;

	for (i = 0; i < ICE_MAX_RESET_WAIT; i++) {
		if (!ice_is_reset_in_progress(pf->state))
			break;
		msleep(100);
	}

	if (test_bit(ICE_FLAG_SRIOV_ENA, pf->flags)) {
		set_bit(ICE_VF_RESETS_DISABLED, pf->state);
		ice_free_vfs(pf);
	}

	ice_service_task_stop(pf);
	ice_aq_cancel_waiting_tasks(pf);
	set_bit(ICE_DOWN, pf->state);

	if (!ice_is_safe_mode(pf))
		ice_remove_arfs(pf);
	ice_deinit_features(pf);
	ice_deinit_devlink(pf);
	ice_deinit_rdma(pf);
	ice_deinit_eth(pf);
	ice_deinit(pf);

	ice_vsi_release_all(pf);

	ice_setup_mc_magic_wake(pf);
	ice_set_wake(pf);

	pci_disable_device(pdev);
}

 
static void ice_shutdown(struct pci_dev *pdev)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);

	ice_remove(pdev);

	if (system_state == SYSTEM_POWER_OFF) {
		pci_wake_from_d3(pdev, pf->wol_ena);
		pci_set_power_state(pdev, PCI_D3hot);
	}
}

#ifdef CONFIG_PM
 
static void ice_prepare_for_shutdown(struct ice_pf *pf)
{
	struct ice_hw *hw = &pf->hw;
	u32 v;

	 
	if (ice_check_sq_alive(hw, &hw->mailboxq))
		ice_vc_notify_reset(pf);

	dev_dbg(ice_pf_to_dev(pf), "Tearing down internal switch for shutdown\n");

	 
	ice_pf_dis_all_vsi(pf, false);

	ice_for_each_vsi(pf, v)
		if (pf->vsi[v])
			pf->vsi[v]->vsi_num = 0;

	ice_shutdown_all_ctrlq(hw);
}

 
static int ice_reinit_interrupt_scheme(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	int ret, v;

	 

	ret = ice_init_interrupt_scheme(pf);
	if (ret) {
		dev_err(dev, "Failed to re-initialize interrupt %d\n", ret);
		return ret;
	}

	 
	ice_for_each_vsi(pf, v) {
		if (!pf->vsi[v])
			continue;

		ret = ice_vsi_alloc_q_vectors(pf->vsi[v]);
		if (ret)
			goto err_reinit;
		ice_vsi_map_rings_to_vectors(pf->vsi[v]);
	}

	ret = ice_req_irq_msix_misc(pf);
	if (ret) {
		dev_err(dev, "Setting up misc vector failed after device suspend %d\n",
			ret);
		goto err_reinit;
	}

	return 0;

err_reinit:
	while (v--)
		if (pf->vsi[v])
			ice_vsi_free_q_vectors(pf->vsi[v]);

	return ret;
}

 
static int __maybe_unused ice_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct ice_pf *pf;
	int disabled, v;

	pf = pci_get_drvdata(pdev);

	if (!ice_pf_state_is_nominal(pf)) {
		dev_err(dev, "Device is not ready, no need to suspend it\n");
		return -EBUSY;
	}

	 
	disabled = ice_service_task_stop(pf);

	ice_unplug_aux_dev(pf);

	 
	if (test_and_set_bit(ICE_SUSPENDED, pf->state)) {
		if (!disabled)
			ice_service_task_restart(pf);
		return 0;
	}

	if (test_bit(ICE_DOWN, pf->state) ||
	    ice_is_reset_in_progress(pf->state)) {
		dev_err(dev, "can't suspend device in reset or already down\n");
		if (!disabled)
			ice_service_task_restart(pf);
		return 0;
	}

	ice_setup_mc_magic_wake(pf);

	ice_prepare_for_shutdown(pf);

	ice_set_wake(pf);

	 
	ice_free_irq_msix_misc(pf);
	ice_for_each_vsi(pf, v) {
		if (!pf->vsi[v])
			continue;
		ice_vsi_free_q_vectors(pf->vsi[v]);
	}
	ice_clear_interrupt_scheme(pf);

	pci_save_state(pdev);
	pci_wake_from_d3(pdev, pf->wol_ena);
	pci_set_power_state(pdev, PCI_D3hot);
	return 0;
}

 
static int __maybe_unused ice_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	enum ice_reset_req reset_type;
	struct ice_pf *pf;
	struct ice_hw *hw;
	int ret;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	pci_save_state(pdev);

	if (!pci_device_is_present(pdev))
		return -ENODEV;

	ret = pci_enable_device_mem(pdev);
	if (ret) {
		dev_err(dev, "Cannot enable device after suspend\n");
		return ret;
	}

	pf = pci_get_drvdata(pdev);
	hw = &pf->hw;

	pf->wakeup_reason = rd32(hw, PFPM_WUS);
	ice_print_wake_reason(pf);

	 
	ret = ice_reinit_interrupt_scheme(pf);
	if (ret)
		dev_err(dev, "Cannot restore interrupt scheme: %d\n", ret);

	clear_bit(ICE_DOWN, pf->state);
	 
	reset_type = ICE_RESET_PFR;
	 
	clear_bit(ICE_SERVICE_DIS, pf->state);

	if (ice_schedule_reset(pf, reset_type))
		dev_err(dev, "Reset during resume failed.\n");

	clear_bit(ICE_SUSPENDED, pf->state);
	ice_service_task_restart(pf);

	 
	mod_timer(&pf->serv_tmr, round_jiffies(jiffies + pf->serv_tmr_period));

	return 0;
}
#endif  

 
static pci_ers_result_t
ice_pci_err_detected(struct pci_dev *pdev, pci_channel_state_t err)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);

	if (!pf) {
		dev_err(&pdev->dev, "%s: unrecoverable device error %d\n",
			__func__, err);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	if (!test_bit(ICE_SUSPENDED, pf->state)) {
		ice_service_task_stop(pf);

		if (!test_bit(ICE_PREPARED_FOR_RESET, pf->state)) {
			set_bit(ICE_PFR_REQ, pf->state);
			ice_prepare_for_reset(pf, ICE_RESET_PFR);
		}
	}

	return PCI_ERS_RESULT_NEED_RESET;
}

 
static pci_ers_result_t ice_pci_err_slot_reset(struct pci_dev *pdev)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);
	pci_ers_result_t result;
	int err;
	u32 reg;

	err = pci_enable_device_mem(pdev);
	if (err) {
		dev_err(&pdev->dev, "Cannot re-enable PCI device after reset, error %d\n",
			err);
		result = PCI_ERS_RESULT_DISCONNECT;
	} else {
		pci_set_master(pdev);
		pci_restore_state(pdev);
		pci_save_state(pdev);
		pci_wake_from_d3(pdev, false);

		 
		reg = rd32(&pf->hw, GLGEN_RTRIG);
		if (!reg)
			result = PCI_ERS_RESULT_RECOVERED;
		else
			result = PCI_ERS_RESULT_DISCONNECT;
	}

	return result;
}

 
static void ice_pci_err_resume(struct pci_dev *pdev)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);

	if (!pf) {
		dev_err(&pdev->dev, "%s failed, device is unrecoverable\n",
			__func__);
		return;
	}

	if (test_bit(ICE_SUSPENDED, pf->state)) {
		dev_dbg(&pdev->dev, "%s failed to resume normal operations!\n",
			__func__);
		return;
	}

	ice_restore_all_vfs_msi_state(pdev);

	ice_do_reset(pf, ICE_RESET_PFR);
	ice_service_task_restart(pf);
	mod_timer(&pf->serv_tmr, round_jiffies(jiffies + pf->serv_tmr_period));
}

 
static void ice_pci_err_reset_prepare(struct pci_dev *pdev)
{
	struct ice_pf *pf = pci_get_drvdata(pdev);

	if (!test_bit(ICE_SUSPENDED, pf->state)) {
		ice_service_task_stop(pf);

		if (!test_bit(ICE_PREPARED_FOR_RESET, pf->state)) {
			set_bit(ICE_PFR_REQ, pf->state);
			ice_prepare_for_reset(pf, ICE_RESET_PFR);
		}
	}
}

 
static void ice_pci_err_reset_done(struct pci_dev *pdev)
{
	ice_pci_err_resume(pdev);
}

 
static const struct pci_device_id ice_pci_tbl[] = {
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E810C_BACKPLANE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E810C_QSFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E810C_SFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E810_XXV_BACKPLANE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E810_XXV_QSFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E810_XXV_SFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823C_BACKPLANE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823C_QSFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823C_SFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823C_10G_BASE_T), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823C_SGMII), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822C_BACKPLANE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822C_QSFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822C_SFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822C_10G_BASE_T), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822C_SGMII), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822L_BACKPLANE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822L_SFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822L_10G_BASE_T), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822L_SGMII), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823L_BACKPLANE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823L_SFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823L_10G_BASE_T), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823L_1GBE), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E823L_QSFP), 0 },
	{ PCI_VDEVICE(INTEL, ICE_DEV_ID_E822_SI_DFLT), 0 },
	 
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, ice_pci_tbl);

static __maybe_unused SIMPLE_DEV_PM_OPS(ice_pm_ops, ice_suspend, ice_resume);

static const struct pci_error_handlers ice_pci_err_handler = {
	.error_detected = ice_pci_err_detected,
	.slot_reset = ice_pci_err_slot_reset,
	.reset_prepare = ice_pci_err_reset_prepare,
	.reset_done = ice_pci_err_reset_done,
	.resume = ice_pci_err_resume
};

static struct pci_driver ice_driver = {
	.name = KBUILD_MODNAME,
	.id_table = ice_pci_tbl,
	.probe = ice_probe,
	.remove = ice_remove,
#ifdef CONFIG_PM
	.driver.pm = &ice_pm_ops,
#endif  
	.shutdown = ice_shutdown,
	.sriov_configure = ice_sriov_configure,
	.err_handler = &ice_pci_err_handler
};

 
static int __init ice_module_init(void)
{
	int status = -ENOMEM;

	pr_info("%s\n", ice_driver_string);
	pr_info("%s\n", ice_copyright);

	ice_wq = alloc_workqueue("%s", 0, 0, KBUILD_MODNAME);
	if (!ice_wq) {
		pr_err("Failed to create workqueue\n");
		return status;
	}

	ice_lag_wq = alloc_ordered_workqueue("ice_lag_wq", 0);
	if (!ice_lag_wq) {
		pr_err("Failed to create LAG workqueue\n");
		goto err_dest_wq;
	}

	status = pci_register_driver(&ice_driver);
	if (status) {
		pr_err("failed to register PCI driver, err %d\n", status);
		goto err_dest_lag_wq;
	}

	return 0;

err_dest_lag_wq:
	destroy_workqueue(ice_lag_wq);
err_dest_wq:
	destroy_workqueue(ice_wq);
	return status;
}
module_init(ice_module_init);

 
static void __exit ice_module_exit(void)
{
	pci_unregister_driver(&ice_driver);
	destroy_workqueue(ice_wq);
	destroy_workqueue(ice_lag_wq);
	pr_info("module unloaded\n");
}
module_exit(ice_module_exit);

 
static int ice_set_mac_address(struct net_device *netdev, void *pi)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	struct sockaddr *addr = pi;
	u8 old_mac[ETH_ALEN];
	u8 flags = 0;
	u8 *mac;
	int err;

	mac = (u8 *)addr->sa_data;

	if (!is_valid_ether_addr(mac))
		return -EADDRNOTAVAIL;

	if (test_bit(ICE_DOWN, pf->state) ||
	    ice_is_reset_in_progress(pf->state)) {
		netdev_err(netdev, "can't set mac %pM. device not ready\n",
			   mac);
		return -EBUSY;
	}

	if (ice_chnl_dmac_fltr_cnt(pf)) {
		netdev_err(netdev, "can't set mac %pM. Device has tc-flower filters, delete all of them and try again\n",
			   mac);
		return -EAGAIN;
	}

	netif_addr_lock_bh(netdev);
	ether_addr_copy(old_mac, netdev->dev_addr);
	 
	eth_hw_addr_set(netdev, mac);
	netif_addr_unlock_bh(netdev);

	 
	err = ice_fltr_remove_mac(vsi, old_mac, ICE_FWD_TO_VSI);
	if (err && err != -ENOENT) {
		err = -EADDRNOTAVAIL;
		goto err_update_filters;
	}

	 
	err = ice_fltr_add_mac(vsi, mac, ICE_FWD_TO_VSI);
	if (err == -EEXIST) {
		 
		netdev_dbg(netdev, "filter for MAC %pM already exists\n", mac);

		return 0;
	} else if (err) {
		 
		err = -EADDRNOTAVAIL;
	}

err_update_filters:
	if (err) {
		netdev_err(netdev, "can't set MAC %pM. filter update failed\n",
			   mac);
		netif_addr_lock_bh(netdev);
		eth_hw_addr_set(netdev, old_mac);
		netif_addr_unlock_bh(netdev);
		return err;
	}

	netdev_dbg(vsi->netdev, "updated MAC address to %pM\n",
		   netdev->dev_addr);

	 
	flags = ICE_AQC_MAN_MAC_UPDATE_LAA_WOL;
	err = ice_aq_manage_mac_write(hw, mac, flags, NULL);
	if (err) {
		netdev_err(netdev, "can't set MAC %pM. write to firmware failed error %d\n",
			   mac, err);
	}
	return 0;
}

 
static void ice_set_rx_mode(struct net_device *netdev)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;

	if (!vsi || ice_is_switchdev_running(vsi->back))
		return;

	 
	set_bit(ICE_VSI_UMAC_FLTR_CHANGED, vsi->state);
	set_bit(ICE_VSI_MMAC_FLTR_CHANGED, vsi->state);
	set_bit(ICE_FLAG_FLTR_SYNC, vsi->back->flags);

	 
	ice_service_task_schedule(vsi->back);
}

 
static int
ice_set_tx_maxrate(struct net_device *netdev, int queue_index, u32 maxrate)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	u16 q_handle;
	int status;
	u8 tc;

	 
	if (maxrate && (maxrate > (ICE_SCHED_MAX_BW / 1000))) {
		netdev_err(netdev, "Invalid max rate %d specified for the queue %d\n",
			   maxrate, queue_index);
		return -EINVAL;
	}

	q_handle = vsi->tx_rings[queue_index]->q_handle;
	tc = ice_dcb_get_tc(vsi, queue_index);

	vsi = ice_locate_vsi_using_queue(vsi, queue_index);
	if (!vsi) {
		netdev_err(netdev, "Invalid VSI for given queue %d\n",
			   queue_index);
		return -EINVAL;
	}

	 
	if (!maxrate)
		status = ice_cfg_q_bw_dflt_lmt(vsi->port_info, vsi->idx, tc,
					       q_handle, ICE_MAX_BW);
	else
		status = ice_cfg_q_bw_lmt(vsi->port_info, vsi->idx, tc,
					  q_handle, ICE_MAX_BW, maxrate * 1000);
	if (status)
		netdev_err(netdev, "Unable to set Tx max rate, error %d\n",
			   status);

	return status;
}

 
static int
ice_fdb_add(struct ndmsg *ndm, struct nlattr __always_unused *tb[],
	    struct net_device *dev, const unsigned char *addr, u16 vid,
	    u16 flags, struct netlink_ext_ack __always_unused *extack)
{
	int err;

	if (vid) {
		netdev_err(dev, "VLANs aren't supported yet for dev_uc|mc_add()\n");
		return -EINVAL;
	}
	if (ndm->ndm_state && !(ndm->ndm_state & NUD_PERMANENT)) {
		netdev_err(dev, "FDB only supports static addresses\n");
		return -EINVAL;
	}

	if (is_unicast_ether_addr(addr) || is_link_local_ether_addr(addr))
		err = dev_uc_add_excl(dev, addr);
	else if (is_multicast_ether_addr(addr))
		err = dev_mc_add_excl(dev, addr);
	else
		err = -EINVAL;

	 
	if (err == -EEXIST && !(flags & NLM_F_EXCL))
		err = 0;

	return err;
}

 
static int
ice_fdb_del(struct ndmsg *ndm, __always_unused struct nlattr *tb[],
	    struct net_device *dev, const unsigned char *addr,
	    __always_unused u16 vid, struct netlink_ext_ack *extack)
{
	int err;

	if (ndm->ndm_state & NUD_PERMANENT) {
		netdev_err(dev, "FDB only supports static addresses\n");
		return -EINVAL;
	}

	if (is_unicast_ether_addr(addr))
		err = dev_uc_del(dev, addr);
	else if (is_multicast_ether_addr(addr))
		err = dev_mc_del(dev, addr);
	else
		err = -EINVAL;

	return err;
}

#define NETIF_VLAN_OFFLOAD_FEATURES	(NETIF_F_HW_VLAN_CTAG_RX | \
					 NETIF_F_HW_VLAN_CTAG_TX | \
					 NETIF_F_HW_VLAN_STAG_RX | \
					 NETIF_F_HW_VLAN_STAG_TX)

#define NETIF_VLAN_STRIPPING_FEATURES	(NETIF_F_HW_VLAN_CTAG_RX | \
					 NETIF_F_HW_VLAN_STAG_RX)

#define NETIF_VLAN_FILTERING_FEATURES	(NETIF_F_HW_VLAN_CTAG_FILTER | \
					 NETIF_F_HW_VLAN_STAG_FILTER)

 
static netdev_features_t
ice_fix_features(struct net_device *netdev, netdev_features_t features)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	netdev_features_t req_vlan_fltr, cur_vlan_fltr;
	bool cur_ctag, cur_stag, req_ctag, req_stag;

	cur_vlan_fltr = netdev->features & NETIF_VLAN_FILTERING_FEATURES;
	cur_ctag = cur_vlan_fltr & NETIF_F_HW_VLAN_CTAG_FILTER;
	cur_stag = cur_vlan_fltr & NETIF_F_HW_VLAN_STAG_FILTER;

	req_vlan_fltr = features & NETIF_VLAN_FILTERING_FEATURES;
	req_ctag = req_vlan_fltr & NETIF_F_HW_VLAN_CTAG_FILTER;
	req_stag = req_vlan_fltr & NETIF_F_HW_VLAN_STAG_FILTER;

	if (req_vlan_fltr != cur_vlan_fltr) {
		if (ice_is_dvm_ena(&np->vsi->back->hw)) {
			if (req_ctag && req_stag) {
				features |= NETIF_VLAN_FILTERING_FEATURES;
			} else if (!req_ctag && !req_stag) {
				features &= ~NETIF_VLAN_FILTERING_FEATURES;
			} else if ((!cur_ctag && req_ctag && !cur_stag) ||
				   (!cur_stag && req_stag && !cur_ctag)) {
				features |= NETIF_VLAN_FILTERING_FEATURES;
				netdev_warn(netdev,  "802.1Q and 802.1ad VLAN filtering must be either both on or both off. VLAN filtering has been enabled for both types.\n");
			} else if ((cur_ctag && !req_ctag && cur_stag) ||
				   (cur_stag && !req_stag && cur_ctag)) {
				features &= ~NETIF_VLAN_FILTERING_FEATURES;
				netdev_warn(netdev,  "802.1Q and 802.1ad VLAN filtering must be either both on or both off. VLAN filtering has been disabled for both types.\n");
			}
		} else {
			if (req_vlan_fltr & NETIF_F_HW_VLAN_STAG_FILTER)
				netdev_warn(netdev, "cannot support requested 802.1ad filtering setting in SVM mode\n");

			if (req_vlan_fltr & NETIF_F_HW_VLAN_CTAG_FILTER)
				features |= NETIF_F_HW_VLAN_CTAG_FILTER;
		}
	}

	if ((features & (NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX)) &&
	    (features & (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_STAG_TX))) {
		netdev_warn(netdev, "cannot support CTAG and STAG VLAN stripping and/or insertion simultaneously since CTAG and STAG offloads are mutually exclusive, clearing STAG offload settings\n");
		features &= ~(NETIF_F_HW_VLAN_STAG_RX |
			      NETIF_F_HW_VLAN_STAG_TX);
	}

	if (!(netdev->features & NETIF_F_RXFCS) &&
	    (features & NETIF_F_RXFCS) &&
	    (features & NETIF_VLAN_STRIPPING_FEATURES) &&
	    !ice_vsi_has_non_zero_vlans(np->vsi)) {
		netdev_warn(netdev, "Disabling VLAN stripping as FCS/CRC stripping is also disabled and there is no VLAN configured\n");
		features &= ~NETIF_VLAN_STRIPPING_FEATURES;
	}

	return features;
}

 
static int
ice_set_vlan_offload_features(struct ice_vsi *vsi, netdev_features_t features)
{
	bool enable_stripping = true, enable_insertion = true;
	struct ice_vsi_vlan_ops *vlan_ops;
	int strip_err = 0, insert_err = 0;
	u16 vlan_ethertype = 0;

	vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);

	if (features & (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_STAG_TX))
		vlan_ethertype = ETH_P_8021AD;
	else if (features & (NETIF_F_HW_VLAN_CTAG_RX | NETIF_F_HW_VLAN_CTAG_TX))
		vlan_ethertype = ETH_P_8021Q;

	if (!(features & (NETIF_F_HW_VLAN_STAG_RX | NETIF_F_HW_VLAN_CTAG_RX)))
		enable_stripping = false;
	if (!(features & (NETIF_F_HW_VLAN_STAG_TX | NETIF_F_HW_VLAN_CTAG_TX)))
		enable_insertion = false;

	if (enable_stripping)
		strip_err = vlan_ops->ena_stripping(vsi, vlan_ethertype);
	else
		strip_err = vlan_ops->dis_stripping(vsi);

	if (enable_insertion)
		insert_err = vlan_ops->ena_insertion(vsi, vlan_ethertype);
	else
		insert_err = vlan_ops->dis_insertion(vsi);

	if (strip_err || insert_err)
		return -EIO;

	return 0;
}

 
static int
ice_set_vlan_filtering_features(struct ice_vsi *vsi, netdev_features_t features)
{
	struct ice_vsi_vlan_ops *vlan_ops = ice_get_compat_vsi_vlan_ops(vsi);
	int err = 0;

	 
	if (features &
	    (NETIF_F_HW_VLAN_CTAG_FILTER | NETIF_F_HW_VLAN_STAG_FILTER))
		err = vlan_ops->ena_rx_filtering(vsi);
	else
		err = vlan_ops->dis_rx_filtering(vsi);

	return err;
}

 
static int
ice_set_vlan_features(struct net_device *netdev, netdev_features_t features)
{
	netdev_features_t current_vlan_features, requested_vlan_features;
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	int err;

	current_vlan_features = netdev->features & NETIF_VLAN_OFFLOAD_FEATURES;
	requested_vlan_features = features & NETIF_VLAN_OFFLOAD_FEATURES;
	if (current_vlan_features ^ requested_vlan_features) {
		if ((features & NETIF_F_RXFCS) &&
		    (features & NETIF_VLAN_STRIPPING_FEATURES)) {
			dev_err(ice_pf_to_dev(vsi->back),
				"To enable VLAN stripping, you must first enable FCS/CRC stripping\n");
			return -EIO;
		}

		err = ice_set_vlan_offload_features(vsi, features);
		if (err)
			return err;
	}

	current_vlan_features = netdev->features &
		NETIF_VLAN_FILTERING_FEATURES;
	requested_vlan_features = features & NETIF_VLAN_FILTERING_FEATURES;
	if (current_vlan_features ^ requested_vlan_features) {
		err = ice_set_vlan_filtering_features(vsi, features);
		if (err)
			return err;
	}

	return 0;
}

 
static int ice_set_loopback(struct ice_vsi *vsi, bool ena)
{
	bool if_running = netif_running(vsi->netdev);
	int ret;

	if (if_running && !test_and_set_bit(ICE_VSI_DOWN, vsi->state)) {
		ret = ice_down(vsi);
		if (ret) {
			netdev_err(vsi->netdev, "Preparing device to toggle loopback failed\n");
			return ret;
		}
	}
	ret = ice_aq_set_mac_loopback(&vsi->back->hw, ena, NULL);
	if (ret)
		netdev_err(vsi->netdev, "Failed to toggle loopback state\n");
	if (if_running)
		ret = ice_up(vsi);

	return ret;
}

 
static int
ice_set_features(struct net_device *netdev, netdev_features_t features)
{
	netdev_features_t changed = netdev->features ^ features;
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	int ret = 0;

	 
	if (ice_is_safe_mode(pf)) {
		dev_err(ice_pf_to_dev(pf),
			"Device is in Safe Mode - not enabling advanced netdev features\n");
		return ret;
	}

	 
	if (ice_is_reset_in_progress(pf->state)) {
		dev_err(ice_pf_to_dev(pf),
			"Device is resetting, changing advanced netdev features temporarily unavailable.\n");
		return -EBUSY;
	}

	 
	if (changed & NETIF_F_RXHASH)
		ice_vsi_manage_rss_lut(vsi, !!(features & NETIF_F_RXHASH));

	ret = ice_set_vlan_features(netdev, features);
	if (ret)
		return ret;

	 
	if (changed & NETIF_F_RXFCS) {
		if ((features & NETIF_F_RXFCS) &&
		    (features & NETIF_VLAN_STRIPPING_FEATURES)) {
			dev_err(ice_pf_to_dev(vsi->back),
				"To disable FCS/CRC stripping, you must first disable VLAN stripping\n");
			return -EIO;
		}

		ice_vsi_cfg_crc_strip(vsi, !!(features & NETIF_F_RXFCS));
		ret = ice_down_up(vsi);
		if (ret)
			return ret;
	}

	if (changed & NETIF_F_NTUPLE) {
		bool ena = !!(features & NETIF_F_NTUPLE);

		ice_vsi_manage_fdir(vsi, ena);
		ena ? ice_init_arfs(vsi) : ice_clear_arfs(vsi);
	}

	 
	if (!(features & NETIF_F_HW_TC) && ice_is_adq_active(pf)) {
		dev_err(ice_pf_to_dev(pf), "ADQ is active, can't turn hw_tc_offload off\n");
		return -EACCES;
	}

	if (changed & NETIF_F_HW_TC) {
		bool ena = !!(features & NETIF_F_HW_TC);

		ena ? set_bit(ICE_FLAG_CLS_FLOWER, pf->flags) :
		      clear_bit(ICE_FLAG_CLS_FLOWER, pf->flags);
	}

	if (changed & NETIF_F_LOOPBACK)
		ret = ice_set_loopback(vsi, !!(features & NETIF_F_LOOPBACK));

	return ret;
}

 
static int ice_vsi_vlan_setup(struct ice_vsi *vsi)
{
	int err;

	err = ice_set_vlan_offload_features(vsi, vsi->netdev->features);
	if (err)
		return err;

	err = ice_set_vlan_filtering_features(vsi, vsi->netdev->features);
	if (err)
		return err;

	return ice_vsi_add_vlan_zero(vsi);
}

 
int ice_vsi_cfg_lan(struct ice_vsi *vsi)
{
	int err;

	if (vsi->netdev && vsi->type == ICE_VSI_PF) {
		ice_set_rx_mode(vsi->netdev);

		err = ice_vsi_vlan_setup(vsi);
		if (err)
			return err;
	}
	ice_vsi_cfg_dcb_rings(vsi);

	err = ice_vsi_cfg_lan_txqs(vsi);
	if (!err && ice_is_xdp_ena_vsi(vsi))
		err = ice_vsi_cfg_xdp_txqs(vsi);
	if (!err)
		err = ice_vsi_cfg_rxqs(vsi);

	return err;
}

 
struct ice_dim {
	 
	u16 itr;
};

 
static const struct ice_dim rx_profile[] = {
	{2},     
	{8},     
	{16},    
	{62},    
	{126}    
};

 
static const struct ice_dim tx_profile[] = {
	{2},     
	{8},     
	{40},    
	{128},   
	{256}    
};

static void ice_tx_dim_work(struct work_struct *work)
{
	struct ice_ring_container *rc;
	struct dim *dim;
	u16 itr;

	dim = container_of(work, struct dim, work);
	rc = dim->priv;

	WARN_ON(dim->profile_ix >= ARRAY_SIZE(tx_profile));

	 
	itr = tx_profile[dim->profile_ix].itr;

	ice_trace(tx_dim_work, container_of(rc, struct ice_q_vector, tx), dim);
	ice_write_itr(rc, itr);

	dim->state = DIM_START_MEASURE;
}

static void ice_rx_dim_work(struct work_struct *work)
{
	struct ice_ring_container *rc;
	struct dim *dim;
	u16 itr;

	dim = container_of(work, struct dim, work);
	rc = dim->priv;

	WARN_ON(dim->profile_ix >= ARRAY_SIZE(rx_profile));

	 
	itr = rx_profile[dim->profile_ix].itr;

	ice_trace(rx_dim_work, container_of(rc, struct ice_q_vector, rx), dim);
	ice_write_itr(rc, itr);

	dim->state = DIM_START_MEASURE;
}

#define ICE_DIM_DEFAULT_PROFILE_IX 1

 
static void ice_init_moderation(struct ice_q_vector *q_vector)
{
	struct ice_ring_container *rc;
	bool tx_dynamic, rx_dynamic;

	rc = &q_vector->tx;
	INIT_WORK(&rc->dim.work, ice_tx_dim_work);
	rc->dim.mode = DIM_CQ_PERIOD_MODE_START_FROM_EQE;
	rc->dim.profile_ix = ICE_DIM_DEFAULT_PROFILE_IX;
	rc->dim.priv = rc;
	tx_dynamic = ITR_IS_DYNAMIC(rc);

	 
	ice_write_itr(rc, tx_dynamic ?
		      tx_profile[rc->dim.profile_ix].itr : rc->itr_setting);

	rc = &q_vector->rx;
	INIT_WORK(&rc->dim.work, ice_rx_dim_work);
	rc->dim.mode = DIM_CQ_PERIOD_MODE_START_FROM_EQE;
	rc->dim.profile_ix = ICE_DIM_DEFAULT_PROFILE_IX;
	rc->dim.priv = rc;
	rx_dynamic = ITR_IS_DYNAMIC(rc);

	 
	ice_write_itr(rc, rx_dynamic ? rx_profile[rc->dim.profile_ix].itr :
				       rc->itr_setting);

	ice_set_q_vector_intrl(q_vector);
}

 
static void ice_napi_enable_all(struct ice_vsi *vsi)
{
	int q_idx;

	if (!vsi->netdev)
		return;

	ice_for_each_q_vector(vsi, q_idx) {
		struct ice_q_vector *q_vector = vsi->q_vectors[q_idx];

		ice_init_moderation(q_vector);

		if (q_vector->rx.rx_ring || q_vector->tx.tx_ring)
			napi_enable(&q_vector->napi);
	}
}

 
static int ice_up_complete(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	int err;

	ice_vsi_cfg_msix(vsi);

	 
	err = ice_vsi_start_all_rx_rings(vsi);
	if (err)
		return err;

	clear_bit(ICE_VSI_DOWN, vsi->state);
	ice_napi_enable_all(vsi);
	ice_vsi_ena_irq(vsi);

	if (vsi->port_info &&
	    (vsi->port_info->phy.link_info.link_info & ICE_AQ_LINK_UP) &&
	    vsi->netdev && vsi->type == ICE_VSI_PF) {
		ice_print_link_msg(vsi, true);
		netif_tx_start_all_queues(vsi->netdev);
		netif_carrier_on(vsi->netdev);
		ice_ptp_link_change(pf, pf->hw.pf_id, true);
	}

	 
	ice_update_eth_stats(vsi);

	if (vsi->type == ICE_VSI_PF)
		ice_service_task_schedule(pf);

	return 0;
}

 
int ice_up(struct ice_vsi *vsi)
{
	int err;

	err = ice_vsi_cfg_lan(vsi);
	if (!err)
		err = ice_up_complete(vsi);

	return err;
}

 
void
ice_fetch_u64_stats_per_ring(struct u64_stats_sync *syncp,
			     struct ice_q_stats stats, u64 *pkts, u64 *bytes)
{
	unsigned int start;

	do {
		start = u64_stats_fetch_begin(syncp);
		*pkts = stats.pkts;
		*bytes = stats.bytes;
	} while (u64_stats_fetch_retry(syncp, start));
}

 
static void
ice_update_vsi_tx_ring_stats(struct ice_vsi *vsi,
			     struct rtnl_link_stats64 *vsi_stats,
			     struct ice_tx_ring **rings, u16 count)
{
	u16 i;

	for (i = 0; i < count; i++) {
		struct ice_tx_ring *ring;
		u64 pkts = 0, bytes = 0;

		ring = READ_ONCE(rings[i]);
		if (!ring || !ring->ring_stats)
			continue;
		ice_fetch_u64_stats_per_ring(&ring->ring_stats->syncp,
					     ring->ring_stats->stats, &pkts,
					     &bytes);
		vsi_stats->tx_packets += pkts;
		vsi_stats->tx_bytes += bytes;
		vsi->tx_restart += ring->ring_stats->tx_stats.restart_q;
		vsi->tx_busy += ring->ring_stats->tx_stats.tx_busy;
		vsi->tx_linearize += ring->ring_stats->tx_stats.tx_linearize;
	}
}

 
static void ice_update_vsi_ring_stats(struct ice_vsi *vsi)
{
	struct rtnl_link_stats64 *net_stats, *stats_prev;
	struct rtnl_link_stats64 *vsi_stats;
	u64 pkts, bytes;
	int i;

	vsi_stats = kzalloc(sizeof(*vsi_stats), GFP_ATOMIC);
	if (!vsi_stats)
		return;

	 
	vsi->tx_restart = 0;
	vsi->tx_busy = 0;
	vsi->tx_linearize = 0;
	vsi->rx_buf_failed = 0;
	vsi->rx_page_failed = 0;

	rcu_read_lock();

	 
	ice_update_vsi_tx_ring_stats(vsi, vsi_stats, vsi->tx_rings,
				     vsi->num_txq);

	 
	ice_for_each_rxq(vsi, i) {
		struct ice_rx_ring *ring = READ_ONCE(vsi->rx_rings[i]);
		struct ice_ring_stats *ring_stats;

		ring_stats = ring->ring_stats;
		ice_fetch_u64_stats_per_ring(&ring_stats->syncp,
					     ring_stats->stats, &pkts,
					     &bytes);
		vsi_stats->rx_packets += pkts;
		vsi_stats->rx_bytes += bytes;
		vsi->rx_buf_failed += ring_stats->rx_stats.alloc_buf_failed;
		vsi->rx_page_failed += ring_stats->rx_stats.alloc_page_failed;
	}

	 
	if (ice_is_xdp_ena_vsi(vsi))
		ice_update_vsi_tx_ring_stats(vsi, vsi_stats, vsi->xdp_rings,
					     vsi->num_xdp_txq);

	rcu_read_unlock();

	net_stats = &vsi->net_stats;
	stats_prev = &vsi->net_stats_prev;

	 
	if (vsi_stats->tx_packets < stats_prev->tx_packets ||
	    vsi_stats->rx_packets < stats_prev->rx_packets) {
		stats_prev->tx_packets = 0;
		stats_prev->tx_bytes = 0;
		stats_prev->rx_packets = 0;
		stats_prev->rx_bytes = 0;
	}

	 
	net_stats->tx_packets += vsi_stats->tx_packets - stats_prev->tx_packets;
	net_stats->tx_bytes += vsi_stats->tx_bytes - stats_prev->tx_bytes;
	net_stats->rx_packets += vsi_stats->rx_packets - stats_prev->rx_packets;
	net_stats->rx_bytes += vsi_stats->rx_bytes - stats_prev->rx_bytes;

	stats_prev->tx_packets = vsi_stats->tx_packets;
	stats_prev->tx_bytes = vsi_stats->tx_bytes;
	stats_prev->rx_packets = vsi_stats->rx_packets;
	stats_prev->rx_bytes = vsi_stats->rx_bytes;

	kfree(vsi_stats);
}

 
void ice_update_vsi_stats(struct ice_vsi *vsi)
{
	struct rtnl_link_stats64 *cur_ns = &vsi->net_stats;
	struct ice_eth_stats *cur_es = &vsi->eth_stats;
	struct ice_pf *pf = vsi->back;

	if (test_bit(ICE_VSI_DOWN, vsi->state) ||
	    test_bit(ICE_CFG_BUSY, pf->state))
		return;

	 
	ice_update_vsi_ring_stats(vsi);

	 
	ice_update_eth_stats(vsi);

	cur_ns->tx_errors = cur_es->tx_errors;
	cur_ns->rx_dropped = cur_es->rx_discards;
	cur_ns->tx_dropped = cur_es->tx_discards;
	cur_ns->multicast = cur_es->rx_multicast;

	 
	if (vsi->type == ICE_VSI_PF) {
		cur_ns->rx_crc_errors = pf->stats.crc_errors;
		cur_ns->rx_errors = pf->stats.crc_errors +
				    pf->stats.illegal_bytes +
				    pf->stats.rx_len_errors +
				    pf->stats.rx_undersize +
				    pf->hw_csum_rx_error +
				    pf->stats.rx_jabber +
				    pf->stats.rx_fragments +
				    pf->stats.rx_oversize;
		cur_ns->rx_length_errors = pf->stats.rx_len_errors;
		 
		cur_ns->rx_missed_errors = pf->stats.eth.rx_discards;
	}
}

 
void ice_update_pf_stats(struct ice_pf *pf)
{
	struct ice_hw_port_stats *prev_ps, *cur_ps;
	struct ice_hw *hw = &pf->hw;
	u16 fd_ctr_base;
	u8 port;

	port = hw->port_info->lport;
	prev_ps = &pf->stats_prev;
	cur_ps = &pf->stats;

	if (ice_is_reset_in_progress(pf->state))
		pf->stat_prev_loaded = false;

	ice_stat_update40(hw, GLPRT_GORCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.rx_bytes,
			  &cur_ps->eth.rx_bytes);

	ice_stat_update40(hw, GLPRT_UPRCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.rx_unicast,
			  &cur_ps->eth.rx_unicast);

	ice_stat_update40(hw, GLPRT_MPRCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.rx_multicast,
			  &cur_ps->eth.rx_multicast);

	ice_stat_update40(hw, GLPRT_BPRCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.rx_broadcast,
			  &cur_ps->eth.rx_broadcast);

	ice_stat_update32(hw, PRTRPB_RDPC, pf->stat_prev_loaded,
			  &prev_ps->eth.rx_discards,
			  &cur_ps->eth.rx_discards);

	ice_stat_update40(hw, GLPRT_GOTCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.tx_bytes,
			  &cur_ps->eth.tx_bytes);

	ice_stat_update40(hw, GLPRT_UPTCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.tx_unicast,
			  &cur_ps->eth.tx_unicast);

	ice_stat_update40(hw, GLPRT_MPTCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.tx_multicast,
			  &cur_ps->eth.tx_multicast);

	ice_stat_update40(hw, GLPRT_BPTCL(port), pf->stat_prev_loaded,
			  &prev_ps->eth.tx_broadcast,
			  &cur_ps->eth.tx_broadcast);

	ice_stat_update32(hw, GLPRT_TDOLD(port), pf->stat_prev_loaded,
			  &prev_ps->tx_dropped_link_down,
			  &cur_ps->tx_dropped_link_down);

	ice_stat_update40(hw, GLPRT_PRC64L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_64, &cur_ps->rx_size_64);

	ice_stat_update40(hw, GLPRT_PRC127L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_127, &cur_ps->rx_size_127);

	ice_stat_update40(hw, GLPRT_PRC255L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_255, &cur_ps->rx_size_255);

	ice_stat_update40(hw, GLPRT_PRC511L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_511, &cur_ps->rx_size_511);

	ice_stat_update40(hw, GLPRT_PRC1023L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_1023, &cur_ps->rx_size_1023);

	ice_stat_update40(hw, GLPRT_PRC1522L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_1522, &cur_ps->rx_size_1522);

	ice_stat_update40(hw, GLPRT_PRC9522L(port), pf->stat_prev_loaded,
			  &prev_ps->rx_size_big, &cur_ps->rx_size_big);

	ice_stat_update40(hw, GLPRT_PTC64L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_64, &cur_ps->tx_size_64);

	ice_stat_update40(hw, GLPRT_PTC127L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_127, &cur_ps->tx_size_127);

	ice_stat_update40(hw, GLPRT_PTC255L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_255, &cur_ps->tx_size_255);

	ice_stat_update40(hw, GLPRT_PTC511L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_511, &cur_ps->tx_size_511);

	ice_stat_update40(hw, GLPRT_PTC1023L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_1023, &cur_ps->tx_size_1023);

	ice_stat_update40(hw, GLPRT_PTC1522L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_1522, &cur_ps->tx_size_1522);

	ice_stat_update40(hw, GLPRT_PTC9522L(port), pf->stat_prev_loaded,
			  &prev_ps->tx_size_big, &cur_ps->tx_size_big);

	fd_ctr_base = hw->fd_ctr_base;

	ice_stat_update40(hw,
			  GLSTAT_FD_CNT0L(ICE_FD_SB_STAT_IDX(fd_ctr_base)),
			  pf->stat_prev_loaded, &prev_ps->fd_sb_match,
			  &cur_ps->fd_sb_match);
	ice_stat_update32(hw, GLPRT_LXONRXC(port), pf->stat_prev_loaded,
			  &prev_ps->link_xon_rx, &cur_ps->link_xon_rx);

	ice_stat_update32(hw, GLPRT_LXOFFRXC(port), pf->stat_prev_loaded,
			  &prev_ps->link_xoff_rx, &cur_ps->link_xoff_rx);

	ice_stat_update32(hw, GLPRT_LXONTXC(port), pf->stat_prev_loaded,
			  &prev_ps->link_xon_tx, &cur_ps->link_xon_tx);

	ice_stat_update32(hw, GLPRT_LXOFFTXC(port), pf->stat_prev_loaded,
			  &prev_ps->link_xoff_tx, &cur_ps->link_xoff_tx);

	ice_update_dcb_stats(pf);

	ice_stat_update32(hw, GLPRT_CRCERRS(port), pf->stat_prev_loaded,
			  &prev_ps->crc_errors, &cur_ps->crc_errors);

	ice_stat_update32(hw, GLPRT_ILLERRC(port), pf->stat_prev_loaded,
			  &prev_ps->illegal_bytes, &cur_ps->illegal_bytes);

	ice_stat_update32(hw, GLPRT_MLFC(port), pf->stat_prev_loaded,
			  &prev_ps->mac_local_faults,
			  &cur_ps->mac_local_faults);

	ice_stat_update32(hw, GLPRT_MRFC(port), pf->stat_prev_loaded,
			  &prev_ps->mac_remote_faults,
			  &cur_ps->mac_remote_faults);

	ice_stat_update32(hw, GLPRT_RLEC(port), pf->stat_prev_loaded,
			  &prev_ps->rx_len_errors, &cur_ps->rx_len_errors);

	ice_stat_update32(hw, GLPRT_RUC(port), pf->stat_prev_loaded,
			  &prev_ps->rx_undersize, &cur_ps->rx_undersize);

	ice_stat_update32(hw, GLPRT_RFC(port), pf->stat_prev_loaded,
			  &prev_ps->rx_fragments, &cur_ps->rx_fragments);

	ice_stat_update32(hw, GLPRT_ROC(port), pf->stat_prev_loaded,
			  &prev_ps->rx_oversize, &cur_ps->rx_oversize);

	ice_stat_update32(hw, GLPRT_RJC(port), pf->stat_prev_loaded,
			  &prev_ps->rx_jabber, &cur_ps->rx_jabber);

	cur_ps->fd_sb_status = test_bit(ICE_FLAG_FD_ENA, pf->flags) ? 1 : 0;

	pf->stat_prev_loaded = true;
}

 
static
void ice_get_stats64(struct net_device *netdev, struct rtnl_link_stats64 *stats)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct rtnl_link_stats64 *vsi_stats;
	struct ice_vsi *vsi = np->vsi;

	vsi_stats = &vsi->net_stats;

	if (!vsi->num_txq || !vsi->num_rxq)
		return;

	 
	if (!test_bit(ICE_VSI_DOWN, vsi->state))
		ice_update_vsi_ring_stats(vsi);
	stats->tx_packets = vsi_stats->tx_packets;
	stats->tx_bytes = vsi_stats->tx_bytes;
	stats->rx_packets = vsi_stats->rx_packets;
	stats->rx_bytes = vsi_stats->rx_bytes;

	 
	stats->multicast = vsi_stats->multicast;
	stats->tx_errors = vsi_stats->tx_errors;
	stats->tx_dropped = vsi_stats->tx_dropped;
	stats->rx_errors = vsi_stats->rx_errors;
	stats->rx_dropped = vsi_stats->rx_dropped;
	stats->rx_crc_errors = vsi_stats->rx_crc_errors;
	stats->rx_length_errors = vsi_stats->rx_length_errors;
}

 
static void ice_napi_disable_all(struct ice_vsi *vsi)
{
	int q_idx;

	if (!vsi->netdev)
		return;

	ice_for_each_q_vector(vsi, q_idx) {
		struct ice_q_vector *q_vector = vsi->q_vectors[q_idx];

		if (q_vector->rx.rx_ring || q_vector->tx.tx_ring)
			napi_disable(&q_vector->napi);

		cancel_work_sync(&q_vector->tx.dim.work);
		cancel_work_sync(&q_vector->rx.dim.work);
	}
}

 
int ice_down(struct ice_vsi *vsi)
{
	int i, tx_err, rx_err, vlan_err = 0;

	WARN_ON(!test_bit(ICE_VSI_DOWN, vsi->state));

	if (vsi->netdev && vsi->type == ICE_VSI_PF) {
		vlan_err = ice_vsi_del_vlan_zero(vsi);
		ice_ptp_link_change(vsi->back, vsi->back->hw.pf_id, false);
		netif_carrier_off(vsi->netdev);
		netif_tx_disable(vsi->netdev);
	} else if (vsi->type == ICE_VSI_SWITCHDEV_CTRL) {
		ice_eswitch_stop_all_tx_queues(vsi->back);
	}

	ice_vsi_dis_irq(vsi);

	tx_err = ice_vsi_stop_lan_tx_rings(vsi, ICE_NO_RESET, 0);
	if (tx_err)
		netdev_err(vsi->netdev, "Failed stop Tx rings, VSI %d error %d\n",
			   vsi->vsi_num, tx_err);
	if (!tx_err && ice_is_xdp_ena_vsi(vsi)) {
		tx_err = ice_vsi_stop_xdp_tx_rings(vsi);
		if (tx_err)
			netdev_err(vsi->netdev, "Failed stop XDP rings, VSI %d error %d\n",
				   vsi->vsi_num, tx_err);
	}

	rx_err = ice_vsi_stop_all_rx_rings(vsi);
	if (rx_err)
		netdev_err(vsi->netdev, "Failed stop Rx rings, VSI %d error %d\n",
			   vsi->vsi_num, rx_err);

	ice_napi_disable_all(vsi);

	ice_for_each_txq(vsi, i)
		ice_clean_tx_ring(vsi->tx_rings[i]);

	if (ice_is_xdp_ena_vsi(vsi))
		ice_for_each_xdp_txq(vsi, i)
			ice_clean_tx_ring(vsi->xdp_rings[i]);

	ice_for_each_rxq(vsi, i)
		ice_clean_rx_ring(vsi->rx_rings[i]);

	if (tx_err || rx_err || vlan_err) {
		netdev_err(vsi->netdev, "Failed to close VSI 0x%04X on switch 0x%04X\n",
			   vsi->vsi_num, vsi->vsw->sw_id);
		return -EIO;
	}

	return 0;
}

 
int ice_down_up(struct ice_vsi *vsi)
{
	int ret;

	 
	if (test_and_set_bit(ICE_VSI_DOWN, vsi->state))
		return 0;

	ret = ice_down(vsi);
	if (ret)
		return ret;

	ret = ice_up(vsi);
	if (ret) {
		netdev_err(vsi->netdev, "reallocating resources failed during netdev features change, may need to reload driver\n");
		return ret;
	}

	return 0;
}

 
int ice_vsi_setup_tx_rings(struct ice_vsi *vsi)
{
	int i, err = 0;

	if (!vsi->num_txq) {
		dev_err(ice_pf_to_dev(vsi->back), "VSI %d has 0 Tx queues\n",
			vsi->vsi_num);
		return -EINVAL;
	}

	ice_for_each_txq(vsi, i) {
		struct ice_tx_ring *ring = vsi->tx_rings[i];

		if (!ring)
			return -EINVAL;

		if (vsi->netdev)
			ring->netdev = vsi->netdev;
		err = ice_setup_tx_ring(ring);
		if (err)
			break;
	}

	return err;
}

 
int ice_vsi_setup_rx_rings(struct ice_vsi *vsi)
{
	int i, err = 0;

	if (!vsi->num_rxq) {
		dev_err(ice_pf_to_dev(vsi->back), "VSI %d has 0 Rx queues\n",
			vsi->vsi_num);
		return -EINVAL;
	}

	ice_for_each_rxq(vsi, i) {
		struct ice_rx_ring *ring = vsi->rx_rings[i];

		if (!ring)
			return -EINVAL;

		if (vsi->netdev)
			ring->netdev = vsi->netdev;
		err = ice_setup_rx_ring(ring);
		if (err)
			break;
	}

	return err;
}

 
int ice_vsi_open_ctrl(struct ice_vsi *vsi)
{
	char int_name[ICE_INT_NAME_STR_LEN];
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int err;

	dev = ice_pf_to_dev(pf);
	 
	err = ice_vsi_setup_tx_rings(vsi);
	if (err)
		goto err_setup_tx;

	err = ice_vsi_setup_rx_rings(vsi);
	if (err)
		goto err_setup_rx;

	err = ice_vsi_cfg_lan(vsi);
	if (err)
		goto err_setup_rx;

	snprintf(int_name, sizeof(int_name) - 1, "%s-%s:ctrl",
		 dev_driver_string(dev), dev_name(dev));
	err = ice_vsi_req_irq_msix(vsi, int_name);
	if (err)
		goto err_setup_rx;

	ice_vsi_cfg_msix(vsi);

	err = ice_vsi_start_all_rx_rings(vsi);
	if (err)
		goto err_up_complete;

	clear_bit(ICE_VSI_DOWN, vsi->state);
	ice_vsi_ena_irq(vsi);

	return 0;

err_up_complete:
	ice_down(vsi);
err_setup_rx:
	ice_vsi_free_rx_rings(vsi);
err_setup_tx:
	ice_vsi_free_tx_rings(vsi);

	return err;
}

 
int ice_vsi_open(struct ice_vsi *vsi)
{
	char int_name[ICE_INT_NAME_STR_LEN];
	struct ice_pf *pf = vsi->back;
	int err;

	 
	err = ice_vsi_setup_tx_rings(vsi);
	if (err)
		goto err_setup_tx;

	err = ice_vsi_setup_rx_rings(vsi);
	if (err)
		goto err_setup_rx;

	err = ice_vsi_cfg_lan(vsi);
	if (err)
		goto err_setup_rx;

	snprintf(int_name, sizeof(int_name) - 1, "%s-%s",
		 dev_driver_string(ice_pf_to_dev(pf)), vsi->netdev->name);
	err = ice_vsi_req_irq_msix(vsi, int_name);
	if (err)
		goto err_setup_rx;

	ice_vsi_cfg_netdev_tc(vsi, vsi->tc_cfg.ena_tc);

	if (vsi->type == ICE_VSI_PF) {
		 
		err = netif_set_real_num_tx_queues(vsi->netdev, vsi->num_txq);
		if (err)
			goto err_set_qs;

		err = netif_set_real_num_rx_queues(vsi->netdev, vsi->num_rxq);
		if (err)
			goto err_set_qs;
	}

	err = ice_up_complete(vsi);
	if (err)
		goto err_up_complete;

	return 0;

err_up_complete:
	ice_down(vsi);
err_set_qs:
	ice_vsi_free_irq(vsi);
err_setup_rx:
	ice_vsi_free_rx_rings(vsi);
err_setup_tx:
	ice_vsi_free_tx_rings(vsi);

	return err;
}

 
static void ice_vsi_release_all(struct ice_pf *pf)
{
	int err, i;

	if (!pf->vsi)
		return;

	ice_for_each_vsi(pf, i) {
		if (!pf->vsi[i])
			continue;

		if (pf->vsi[i]->type == ICE_VSI_CHNL)
			continue;

		err = ice_vsi_release(pf->vsi[i]);
		if (err)
			dev_dbg(ice_pf_to_dev(pf), "Failed to release pf->vsi[%d], err %d, vsi_num = %d\n",
				i, err, pf->vsi[i]->vsi_num);
	}
}

 
static int ice_vsi_rebuild_by_type(struct ice_pf *pf, enum ice_vsi_type type)
{
	struct device *dev = ice_pf_to_dev(pf);
	int i, err;

	ice_for_each_vsi(pf, i) {
		struct ice_vsi *vsi = pf->vsi[i];

		if (!vsi || vsi->type != type)
			continue;

		 
		err = ice_vsi_rebuild(vsi, ICE_VSI_FLAG_INIT);
		if (err) {
			dev_err(dev, "rebuild VSI failed, err %d, VSI index %d, type %s\n",
				err, vsi->idx, ice_vsi_type_str(type));
			return err;
		}

		 
		err = ice_replay_vsi(&pf->hw, vsi->idx);
		if (err) {
			dev_err(dev, "replay VSI failed, error %d, VSI index %d, type %s\n",
				err, vsi->idx, ice_vsi_type_str(type));
			return err;
		}

		 
		vsi->vsi_num = ice_get_hw_vsi_num(&pf->hw, vsi->idx);

		 
		err = ice_ena_vsi(vsi, false);
		if (err) {
			dev_err(dev, "enable VSI failed, err %d, VSI index %d, type %s\n",
				err, vsi->idx, ice_vsi_type_str(type));
			return err;
		}

		dev_info(dev, "VSI rebuilt. VSI index %d, type %s\n", vsi->idx,
			 ice_vsi_type_str(type));
	}

	return 0;
}

 
static void ice_update_pf_netdev_link(struct ice_pf *pf)
{
	bool link_up;
	int i;

	ice_for_each_vsi(pf, i) {
		struct ice_vsi *vsi = pf->vsi[i];

		if (!vsi || vsi->type != ICE_VSI_PF)
			return;

		ice_get_link_status(pf->vsi[i]->port_info, &link_up);
		if (link_up) {
			netif_carrier_on(pf->vsi[i]->netdev);
			netif_tx_wake_all_queues(pf->vsi[i]->netdev);
		} else {
			netif_carrier_off(pf->vsi[i]->netdev);
			netif_tx_stop_all_queues(pf->vsi[i]->netdev);
		}
	}
}

 
static void ice_rebuild(struct ice_pf *pf, enum ice_reset_req reset_type)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	bool dvm;
	int err;

	if (test_bit(ICE_DOWN, pf->state))
		goto clear_recovery;

	dev_dbg(dev, "rebuilding PF after reset_type=%d\n", reset_type);

#define ICE_EMP_RESET_SLEEP_MS 5000
	if (reset_type == ICE_RESET_EMPR) {
		 
		pf->fw_emp_reset_disabled = false;

		msleep(ICE_EMP_RESET_SLEEP_MS);
	}

	err = ice_init_all_ctrlq(hw);
	if (err) {
		dev_err(dev, "control queues init failed %d\n", err);
		goto err_init_ctrlq;
	}

	 
	if (!ice_is_safe_mode(pf)) {
		 
		if (reset_type == ICE_RESET_PFR)
			ice_fill_blk_tbls(hw);
		else
			 
			ice_load_pkg(NULL, pf);
	}

	err = ice_clear_pf_cfg(hw);
	if (err) {
		dev_err(dev, "clear PF configuration failed %d\n", err);
		goto err_init_ctrlq;
	}

	ice_clear_pxe_mode(hw);

	err = ice_init_nvm(hw);
	if (err) {
		dev_err(dev, "ice_init_nvm failed %d\n", err);
		goto err_init_ctrlq;
	}

	err = ice_get_caps(hw);
	if (err) {
		dev_err(dev, "ice_get_caps failed %d\n", err);
		goto err_init_ctrlq;
	}

	err = ice_aq_set_mac_cfg(hw, ICE_AQ_SET_MAC_FRAME_SIZE_MAX, NULL);
	if (err) {
		dev_err(dev, "set_mac_cfg failed %d\n", err);
		goto err_init_ctrlq;
	}

	dvm = ice_is_dvm_ena(hw);

	err = ice_aq_set_port_params(pf->hw.port_info, dvm, NULL);
	if (err)
		goto err_init_ctrlq;

	err = ice_sched_init_port(hw->port_info);
	if (err)
		goto err_sched_init_port;

	 
	err = ice_req_irq_msix_misc(pf);
	if (err) {
		dev_err(dev, "misc vector setup failed: %d\n", err);
		goto err_sched_init_port;
	}

	if (test_bit(ICE_FLAG_FD_ENA, pf->flags)) {
		wr32(hw, PFQF_FD_ENA, PFQF_FD_ENA_FD_ENA_M);
		if (!rd32(hw, PFQF_FD_SIZE)) {
			u16 unused, guar, b_effort;

			guar = hw->func_caps.fd_fltr_guar;
			b_effort = hw->func_caps.fd_fltr_best_effort;

			 
			ice_alloc_fd_guar_item(hw, &unused, guar);
			 
			ice_alloc_fd_shrd_item(hw, &unused, b_effort);
		}
	}

	if (test_bit(ICE_FLAG_DCB_ENA, pf->flags))
		ice_dcb_rebuild(pf);

	 
	if (test_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags))
		ice_ptp_reset(pf);

	if (ice_is_feature_supported(pf, ICE_F_GNSS))
		ice_gnss_init(pf);

	 
	err = ice_vsi_rebuild_by_type(pf, ICE_VSI_PF);
	if (err) {
		dev_err(dev, "PF VSI rebuild failed: %d\n", err);
		goto err_vsi_rebuild;
	}

	 
	if (test_bit(ICE_FLAG_PTP_SUPPORTED, pf->flags))
		ice_ptp_cfg_timestamp(pf, false);

	err = ice_vsi_rebuild_by_type(pf, ICE_VSI_SWITCHDEV_CTRL);
	if (err) {
		dev_err(dev, "Switchdev CTRL VSI rebuild failed: %d\n", err);
		goto err_vsi_rebuild;
	}

	if (reset_type == ICE_RESET_PFR) {
		err = ice_rebuild_channels(pf);
		if (err) {
			dev_err(dev, "failed to rebuild and replay ADQ VSIs, err %d\n",
				err);
			goto err_vsi_rebuild;
		}
	}

	 
	if (test_bit(ICE_FLAG_FD_ENA, pf->flags)) {
		err = ice_vsi_rebuild_by_type(pf, ICE_VSI_CTRL);
		if (err) {
			dev_err(dev, "control VSI rebuild failed: %d\n", err);
			goto err_vsi_rebuild;
		}

		 
		if (hw->fdir_prof)
			ice_fdir_replay_flows(hw);

		 
		ice_fdir_replay_fltrs(pf);

		ice_rebuild_arfs(pf);
	}

	ice_update_pf_netdev_link(pf);

	 
	err = ice_send_version(pf);
	if (err) {
		dev_err(dev, "Rebuild failed due to error sending driver version: %d\n",
			err);
		goto err_vsi_rebuild;
	}

	ice_replay_post(hw);

	 
	clear_bit(ICE_RESET_FAILED, pf->state);

	ice_plug_aux_dev(pf);
	if (ice_is_feature_supported(pf, ICE_F_SRIOV_LAG))
		ice_lag_rebuild(pf);
	return;

err_vsi_rebuild:
err_sched_init_port:
	ice_sched_cleanup_all(hw);
err_init_ctrlq:
	ice_shutdown_all_ctrlq(hw);
	set_bit(ICE_RESET_FAILED, pf->state);
clear_recovery:
	 
	set_bit(ICE_NEEDS_RESTART, pf->state);
	dev_err(dev, "Rebuild failed, unload and reload driver\n");
}

 
static int ice_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	struct bpf_prog *prog;
	u8 count = 0;
	int err = 0;

	if (new_mtu == (int)netdev->mtu) {
		netdev_warn(netdev, "MTU is already %u\n", netdev->mtu);
		return 0;
	}

	prog = vsi->xdp_prog;
	if (prog && !prog->aux->xdp_has_frags) {
		int frame_size = ice_max_xdp_frame_size(vsi);

		if (new_mtu + ICE_ETH_PKT_HDR_PAD > frame_size) {
			netdev_err(netdev, "max MTU for XDP usage is %d\n",
				   frame_size - ICE_ETH_PKT_HDR_PAD);
			return -EINVAL;
		}
	} else if (test_bit(ICE_FLAG_LEGACY_RX, pf->flags)) {
		if (new_mtu + ICE_ETH_PKT_HDR_PAD > ICE_MAX_FRAME_LEGACY_RX) {
			netdev_err(netdev, "Too big MTU for legacy-rx; Max is %d\n",
				   ICE_MAX_FRAME_LEGACY_RX - ICE_ETH_PKT_HDR_PAD);
			return -EINVAL;
		}
	}

	 
	do {
		if (ice_is_reset_in_progress(pf->state)) {
			count++;
			usleep_range(1000, 2000);
		} else {
			break;
		}

	} while (count < 100);

	if (count == 100) {
		netdev_err(netdev, "can't change MTU. Device is busy\n");
		return -EBUSY;
	}

	netdev->mtu = (unsigned int)new_mtu;
	err = ice_down_up(vsi);
	if (err)
		return err;

	netdev_dbg(netdev, "changed MTU to %d\n", new_mtu);
	set_bit(ICE_FLAG_MTU_CHANGED, pf->flags);

	return err;
}

 
static int ice_eth_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_pf *pf = np->vsi->back;

	switch (cmd) {
	case SIOCGHWTSTAMP:
		return ice_ptp_get_ts_config(pf, ifr);
	case SIOCSHWTSTAMP:
		return ice_ptp_set_ts_config(pf, ifr);
	default:
		return -EOPNOTSUPP;
	}
}

 
const char *ice_aq_str(enum ice_aq_err aq_err)
{
	switch (aq_err) {
	case ICE_AQ_RC_OK:
		return "OK";
	case ICE_AQ_RC_EPERM:
		return "ICE_AQ_RC_EPERM";
	case ICE_AQ_RC_ENOENT:
		return "ICE_AQ_RC_ENOENT";
	case ICE_AQ_RC_ENOMEM:
		return "ICE_AQ_RC_ENOMEM";
	case ICE_AQ_RC_EBUSY:
		return "ICE_AQ_RC_EBUSY";
	case ICE_AQ_RC_EEXIST:
		return "ICE_AQ_RC_EEXIST";
	case ICE_AQ_RC_EINVAL:
		return "ICE_AQ_RC_EINVAL";
	case ICE_AQ_RC_ENOSPC:
		return "ICE_AQ_RC_ENOSPC";
	case ICE_AQ_RC_ENOSYS:
		return "ICE_AQ_RC_ENOSYS";
	case ICE_AQ_RC_EMODE:
		return "ICE_AQ_RC_EMODE";
	case ICE_AQ_RC_ENOSEC:
		return "ICE_AQ_RC_ENOSEC";
	case ICE_AQ_RC_EBADSIG:
		return "ICE_AQ_RC_EBADSIG";
	case ICE_AQ_RC_ESVN:
		return "ICE_AQ_RC_ESVN";
	case ICE_AQ_RC_EBADMAN:
		return "ICE_AQ_RC_EBADMAN";
	case ICE_AQ_RC_EBADBUF:
		return "ICE_AQ_RC_EBADBUF";
	}

	return "ICE_AQ_RC_UNKNOWN";
}

 
int ice_set_rss_lut(struct ice_vsi *vsi, u8 *lut, u16 lut_size)
{
	struct ice_aq_get_set_rss_lut_params params = {};
	struct ice_hw *hw = &vsi->back->hw;
	int status;

	if (!lut)
		return -EINVAL;

	params.vsi_handle = vsi->idx;
	params.lut_size = lut_size;
	params.lut_type = vsi->rss_lut_type;
	params.lut = lut;

	status = ice_aq_set_rss_lut(hw, &params);
	if (status)
		dev_err(ice_pf_to_dev(vsi->back), "Cannot set RSS lut, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));

	return status;
}

 
int ice_set_rss_key(struct ice_vsi *vsi, u8 *seed)
{
	struct ice_hw *hw = &vsi->back->hw;
	int status;

	if (!seed)
		return -EINVAL;

	status = ice_aq_set_rss_key(hw, vsi->idx, (struct ice_aqc_get_set_rss_keys *)seed);
	if (status)
		dev_err(ice_pf_to_dev(vsi->back), "Cannot set RSS key, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));

	return status;
}

 
int ice_get_rss_lut(struct ice_vsi *vsi, u8 *lut, u16 lut_size)
{
	struct ice_aq_get_set_rss_lut_params params = {};
	struct ice_hw *hw = &vsi->back->hw;
	int status;

	if (!lut)
		return -EINVAL;

	params.vsi_handle = vsi->idx;
	params.lut_size = lut_size;
	params.lut_type = vsi->rss_lut_type;
	params.lut = lut;

	status = ice_aq_get_rss_lut(hw, &params);
	if (status)
		dev_err(ice_pf_to_dev(vsi->back), "Cannot get RSS lut, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));

	return status;
}

 
int ice_get_rss_key(struct ice_vsi *vsi, u8 *seed)
{
	struct ice_hw *hw = &vsi->back->hw;
	int status;

	if (!seed)
		return -EINVAL;

	status = ice_aq_get_rss_key(hw, vsi->idx, (struct ice_aqc_get_set_rss_keys *)seed);
	if (status)
		dev_err(ice_pf_to_dev(vsi->back), "Cannot get RSS key, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));

	return status;
}

 
static int
ice_bridge_getlink(struct sk_buff *skb, u32 pid, u32 seq,
		   struct net_device *dev, u32 filter_mask, int nlflags)
{
	struct ice_netdev_priv *np = netdev_priv(dev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	u16 bmode;

	bmode = pf->first_sw->bridge_mode;

	return ndo_dflt_bridge_getlink(skb, pid, seq, dev, bmode, 0, 0, nlflags,
				       filter_mask, NULL);
}

 
static int ice_vsi_update_bridge_mode(struct ice_vsi *vsi, u16 bmode)
{
	struct ice_aqc_vsi_props *vsi_props;
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	int ret;

	vsi_props = &vsi->info;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ctxt->info = vsi->info;

	if (bmode == BRIDGE_MODE_VEB)
		 
		ctxt->info.sw_flags |= ICE_AQ_VSI_SW_FLAG_ALLOW_LB;
	else
		 
		ctxt->info.sw_flags &= ~ICE_AQ_VSI_SW_FLAG_ALLOW_LB;
	ctxt->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_SW_VALID);

	ret = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (ret) {
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for bridge mode failed, bmode = %d err %d aq_err %s\n",
			bmode, ret, ice_aq_str(hw->adminq.sq_last_status));
		goto out;
	}
	 
	vsi_props->sw_flags = ctxt->info.sw_flags;

out:
	kfree(ctxt);
	return ret;
}

 
static int
ice_bridge_setlink(struct net_device *dev, struct nlmsghdr *nlh,
		   u16 __always_unused flags,
		   struct netlink_ext_ack __always_unused *extack)
{
	struct ice_netdev_priv *np = netdev_priv(dev);
	struct ice_pf *pf = np->vsi->back;
	struct nlattr *attr, *br_spec;
	struct ice_hw *hw = &pf->hw;
	struct ice_sw *pf_sw;
	int rem, v, err = 0;

	pf_sw = pf->first_sw;
	 
	br_spec = nlmsg_find_attr(nlh, sizeof(struct ifinfomsg), IFLA_AF_SPEC);

	nla_for_each_nested(attr, br_spec, rem) {
		__u16 mode;

		if (nla_type(attr) != IFLA_BRIDGE_MODE)
			continue;
		mode = nla_get_u16(attr);
		if (mode != BRIDGE_MODE_VEPA && mode != BRIDGE_MODE_VEB)
			return -EINVAL;
		 
		if (mode == pf_sw->bridge_mode)
			continue;
		 
		ice_for_each_vsi(pf, v) {
			if (!pf->vsi[v])
				continue;
			err = ice_vsi_update_bridge_mode(pf->vsi[v], mode);
			if (err)
				return err;
		}

		hw->evb_veb = (mode == BRIDGE_MODE_VEB);
		 
		err = ice_update_sw_rule_bridge_mode(hw);
		if (err) {
			netdev_err(dev, "switch rule update failed, mode = %d err %d aq_err %s\n",
				   mode, err,
				   ice_aq_str(hw->adminq.sq_last_status));
			 
			hw->evb_veb = (pf_sw->bridge_mode == BRIDGE_MODE_VEB);
			return err;
		}

		pf_sw->bridge_mode = mode;
	}

	return 0;
}

 
static void ice_tx_timeout(struct net_device *netdev, unsigned int txqueue)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_tx_ring *tx_ring = NULL;
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	u32 i;

	pf->tx_timeout_count++;

	 
	if (ice_is_pfc_causing_hung_q(pf, txqueue)) {
		dev_info(ice_pf_to_dev(pf), "Fake Tx hang detected on queue %u, timeout caused by PFC storm\n",
			 txqueue);
		return;
	}

	 
	ice_for_each_txq(vsi, i)
		if (vsi->tx_rings[i] && vsi->tx_rings[i]->desc)
			if (txqueue == vsi->tx_rings[i]->q_index) {
				tx_ring = vsi->tx_rings[i];
				break;
			}

	 
	if (time_after(jiffies, (pf->tx_timeout_last_recovery + HZ * 20)))
		pf->tx_timeout_recovery_level = 1;
	else if (time_before(jiffies, (pf->tx_timeout_last_recovery +
				       netdev->watchdog_timeo)))
		return;

	if (tx_ring) {
		struct ice_hw *hw = &pf->hw;
		u32 head, val = 0;

		head = (rd32(hw, QTX_COMM_HEAD(vsi->txq_map[txqueue])) &
			QTX_COMM_HEAD_HEAD_M) >> QTX_COMM_HEAD_HEAD_S;
		 
		val = rd32(hw, GLINT_DYN_CTL(tx_ring->q_vector->reg_idx));

		netdev_info(netdev, "tx_timeout: VSI_num: %d, Q %u, NTC: 0x%x, HW_HEAD: 0x%x, NTU: 0x%x, INT: 0x%x\n",
			    vsi->vsi_num, txqueue, tx_ring->next_to_clean,
			    head, tx_ring->next_to_use, val);
	}

	pf->tx_timeout_last_recovery = jiffies;
	netdev_info(netdev, "tx_timeout recovery level %d, txqueue %u\n",
		    pf->tx_timeout_recovery_level, txqueue);

	switch (pf->tx_timeout_recovery_level) {
	case 1:
		set_bit(ICE_PFR_REQ, pf->state);
		break;
	case 2:
		set_bit(ICE_CORER_REQ, pf->state);
		break;
	case 3:
		set_bit(ICE_GLOBR_REQ, pf->state);
		break;
	default:
		netdev_err(netdev, "tx_timeout recovery unsuccessful, device is in unrecoverable state.\n");
		set_bit(ICE_DOWN, pf->state);
		set_bit(ICE_VSI_NEEDS_RESTART, vsi->state);
		set_bit(ICE_SERVICE_DIS, pf->state);
		break;
	}

	ice_service_task_schedule(pf);
	pf->tx_timeout_recovery_level++;
}

 
static int
ice_setup_tc_cls_flower(struct ice_netdev_priv *np,
			struct net_device *filter_dev,
			struct flow_cls_offload *cls_flower)
{
	struct ice_vsi *vsi = np->vsi;

	if (cls_flower->common.chain_index)
		return -EOPNOTSUPP;

	switch (cls_flower->command) {
	case FLOW_CLS_REPLACE:
		return ice_add_cls_flower(filter_dev, vsi, cls_flower);
	case FLOW_CLS_DESTROY:
		return ice_del_cls_flower(vsi, cls_flower);
	default:
		return -EINVAL;
	}
}

 
static int
ice_setup_tc_block_cb(enum tc_setup_type type, void *type_data, void *cb_priv)
{
	struct ice_netdev_priv *np = cb_priv;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return ice_setup_tc_cls_flower(np, np->vsi->netdev,
					       type_data);
	default:
		return -EOPNOTSUPP;
	}
}

 
static int
ice_validate_mqprio_qopt(struct ice_vsi *vsi,
			 struct tc_mqprio_qopt_offload *mqprio_qopt)
{
	int non_power_of_2_qcount = 0;
	struct ice_pf *pf = vsi->back;
	int max_rss_q_cnt = 0;
	u64 sum_min_rate = 0;
	struct device *dev;
	int i, speed;
	u8 num_tc;

	if (vsi->type != ICE_VSI_PF)
		return -EINVAL;

	if (mqprio_qopt->qopt.offset[0] != 0 ||
	    mqprio_qopt->qopt.num_tc < 1 ||
	    mqprio_qopt->qopt.num_tc > ICE_CHNL_MAX_TC)
		return -EINVAL;

	dev = ice_pf_to_dev(pf);
	vsi->ch_rss_size = 0;
	num_tc = mqprio_qopt->qopt.num_tc;
	speed = ice_get_link_speed_kbps(vsi);

	for (i = 0; num_tc; i++) {
		int qcount = mqprio_qopt->qopt.count[i];
		u64 max_rate, min_rate, rem;

		if (!qcount)
			return -EINVAL;

		if (is_power_of_2(qcount)) {
			if (non_power_of_2_qcount &&
			    qcount > non_power_of_2_qcount) {
				dev_err(dev, "qcount[%d] cannot be greater than non power of 2 qcount[%d]\n",
					qcount, non_power_of_2_qcount);
				return -EINVAL;
			}
			if (qcount > max_rss_q_cnt)
				max_rss_q_cnt = qcount;
		} else {
			if (non_power_of_2_qcount &&
			    qcount != non_power_of_2_qcount) {
				dev_err(dev, "Only one non power of 2 qcount allowed[%d,%d]\n",
					qcount, non_power_of_2_qcount);
				return -EINVAL;
			}
			if (qcount < max_rss_q_cnt) {
				dev_err(dev, "non power of 2 qcount[%d] cannot be less than other qcount[%d]\n",
					qcount, max_rss_q_cnt);
				return -EINVAL;
			}
			max_rss_q_cnt = qcount;
			non_power_of_2_qcount = qcount;
		}

		 
		max_rate = mqprio_qopt->max_rate[i];
		max_rate = div_u64(max_rate, ICE_BW_KBPS_DIVISOR);

		 
		min_rate = mqprio_qopt->min_rate[i];
		min_rate = div_u64(min_rate, ICE_BW_KBPS_DIVISOR);
		sum_min_rate += min_rate;

		if (min_rate && min_rate < ICE_MIN_BW_LIMIT) {
			dev_err(dev, "TC%d: min_rate(%llu Kbps) < %u Kbps\n", i,
				min_rate, ICE_MIN_BW_LIMIT);
			return -EINVAL;
		}

		if (max_rate && max_rate > speed) {
			dev_err(dev, "TC%d: max_rate(%llu Kbps) > link speed of %u Kbps\n",
				i, max_rate, speed);
			return -EINVAL;
		}

		iter_div_u64_rem(min_rate, ICE_MIN_BW_LIMIT, &rem);
		if (rem) {
			dev_err(dev, "TC%d: Min Rate not multiple of %u Kbps",
				i, ICE_MIN_BW_LIMIT);
			return -EINVAL;
		}

		iter_div_u64_rem(max_rate, ICE_MIN_BW_LIMIT, &rem);
		if (rem) {
			dev_err(dev, "TC%d: Max Rate not multiple of %u Kbps",
				i, ICE_MIN_BW_LIMIT);
			return -EINVAL;
		}

		 
		if (max_rate && min_rate > max_rate) {
			dev_err(dev, "min_rate %llu Kbps can't be more than max_rate %llu Kbps\n",
				min_rate, max_rate);
			return -EINVAL;
		}

		if (i >= mqprio_qopt->qopt.num_tc - 1)
			break;
		if (mqprio_qopt->qopt.offset[i + 1] !=
		    (mqprio_qopt->qopt.offset[i] + qcount))
			return -EINVAL;
	}
	if (vsi->num_rxq <
	    (mqprio_qopt->qopt.offset[i] + mqprio_qopt->qopt.count[i]))
		return -EINVAL;
	if (vsi->num_txq <
	    (mqprio_qopt->qopt.offset[i] + mqprio_qopt->qopt.count[i]))
		return -EINVAL;

	if (sum_min_rate && sum_min_rate > (u64)speed) {
		dev_err(dev, "Invalid min Tx rate(%llu) Kbps > speed (%u) Kbps specified\n",
			sum_min_rate, speed);
		return -EINVAL;
	}

	 
	vsi->ch_rss_size = max_rss_q_cnt;

	return 0;
}

 
static int ice_add_vsi_to_fdir(struct ice_pf *pf, struct ice_vsi *vsi)
{
	struct device *dev = ice_pf_to_dev(pf);
	bool added = false;
	struct ice_hw *hw;
	int flow;

	if (!(vsi->num_gfltr || vsi->num_bfltr))
		return -EINVAL;

	hw = &pf->hw;
	for (flow = 0; flow < ICE_FLTR_PTYPE_MAX; flow++) {
		struct ice_fd_hw_prof *prof;
		int tun, status;
		u64 entry_h;

		if (!(hw->fdir_prof && hw->fdir_prof[flow] &&
		      hw->fdir_prof[flow]->cnt))
			continue;

		for (tun = 0; tun < ICE_FD_HW_SEG_MAX; tun++) {
			enum ice_flow_priority prio;
			u64 prof_id;

			 
			prio = ICE_FLOW_PRIO_NORMAL;
			prof = hw->fdir_prof[flow];
			prof_id = flow + tun * ICE_FLTR_PTYPE_MAX;
			status = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id,
						    prof->vsi_h[0], vsi->idx,
						    prio, prof->fdir_seg[tun],
						    &entry_h);
			if (status) {
				dev_err(dev, "channel VSI idx %d, not able to add to group %d\n",
					vsi->idx, flow);
				continue;
			}

			prof->entry_h[prof->cnt][tun] = entry_h;
		}

		 
		prof->vsi_h[prof->cnt] = vsi->idx;
		prof->cnt++;

		added = true;
		dev_dbg(dev, "VSI idx %d added to fdir group %d\n", vsi->idx,
			flow);
	}

	if (!added)
		dev_dbg(dev, "VSI idx %d not added to fdir groups\n", vsi->idx);

	return 0;
}

 
static int ice_add_channel(struct ice_pf *pf, u16 sw_id, struct ice_channel *ch)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vsi *vsi;

	if (ch->type != ICE_VSI_CHNL) {
		dev_err(dev, "add new VSI failed, ch->type %d\n", ch->type);
		return -EINVAL;
	}

	vsi = ice_chnl_vsi_setup(pf, pf->hw.port_info, ch);
	if (!vsi || vsi->type != ICE_VSI_CHNL) {
		dev_err(dev, "create chnl VSI failure\n");
		return -EINVAL;
	}

	ice_add_vsi_to_fdir(pf, vsi);

	ch->sw_id = sw_id;
	ch->vsi_num = vsi->vsi_num;
	ch->info.mapping_flags = vsi->info.mapping_flags;
	ch->ch_vsi = vsi;
	 
	vsi->ch = ch;

	memcpy(&ch->info.q_mapping, &vsi->info.q_mapping,
	       sizeof(vsi->info.q_mapping));
	memcpy(&ch->info.tc_mapping, vsi->info.tc_mapping,
	       sizeof(vsi->info.tc_mapping));

	return 0;
}

 
static void ice_chnl_cfg_res(struct ice_vsi *vsi, struct ice_channel *ch)
{
	int i;

	for (i = 0; i < ch->num_txq; i++) {
		struct ice_q_vector *tx_q_vector, *rx_q_vector;
		struct ice_ring_container *rc;
		struct ice_tx_ring *tx_ring;
		struct ice_rx_ring *rx_ring;

		tx_ring = vsi->tx_rings[ch->base_q + i];
		rx_ring = vsi->rx_rings[ch->base_q + i];
		if (!tx_ring || !rx_ring)
			continue;

		 
		tx_ring->ch = ch;
		rx_ring->ch = ch;

		 
		tx_q_vector = tx_ring->q_vector;
		rx_q_vector = rx_ring->q_vector;
		if (!tx_q_vector && !rx_q_vector)
			continue;

		if (tx_q_vector) {
			tx_q_vector->ch = ch;
			 
			rc = &tx_q_vector->tx;
			if (!ITR_IS_DYNAMIC(rc))
				ice_write_itr(rc, rc->itr_setting);
		}
		if (rx_q_vector) {
			rx_q_vector->ch = ch;
			 
			rc = &rx_q_vector->rx;
			if (!ITR_IS_DYNAMIC(rc))
				ice_write_itr(rc, rc->itr_setting);
		}
	}

	 
	if (ch->num_txq || ch->num_rxq)
		ice_flush(&vsi->back->hw);
}

 
static void
ice_cfg_chnl_all_res(struct ice_vsi *vsi, struct ice_channel *ch)
{
	 
	ice_chnl_cfg_res(vsi, ch);
}

 
static int
ice_setup_hw_channel(struct ice_pf *pf, struct ice_vsi *vsi,
		     struct ice_channel *ch, u16 sw_id, u8 type)
{
	struct device *dev = ice_pf_to_dev(pf);
	int ret;

	ch->base_q = vsi->next_base_q;
	ch->type = type;

	ret = ice_add_channel(pf, sw_id, ch);
	if (ret) {
		dev_err(dev, "failed to add_channel using sw_id %u\n", sw_id);
		return ret;
	}

	 
	ice_cfg_chnl_all_res(vsi, ch);

	 
	vsi->next_base_q = vsi->next_base_q + ch->num_rxq;
	dev_dbg(dev, "added channel: vsi_num %u, num_rxq %u\n", ch->vsi_num,
		ch->num_rxq);

	return 0;
}

 
static bool
ice_setup_channel(struct ice_pf *pf, struct ice_vsi *vsi,
		  struct ice_channel *ch)
{
	struct device *dev = ice_pf_to_dev(pf);
	u16 sw_id;
	int ret;

	if (vsi->type != ICE_VSI_PF) {
		dev_err(dev, "unsupported parent VSI type(%d)\n", vsi->type);
		return false;
	}

	sw_id = pf->first_sw->sw_id;

	 
	ret = ice_setup_hw_channel(pf, vsi, ch, sw_id, ICE_VSI_CHNL);
	if (ret) {
		dev_err(dev, "failed to setup hw_channel\n");
		return false;
	}
	dev_dbg(dev, "successfully created channel()\n");

	return ch->ch_vsi ? true : false;
}

 
static int
ice_set_bw_limit(struct ice_vsi *vsi, u64 max_tx_rate, u64 min_tx_rate)
{
	int err;

	err = ice_set_min_bw_limit(vsi, min_tx_rate);
	if (err)
		return err;

	return ice_set_max_bw_limit(vsi, max_tx_rate);
}

 
static int ice_create_q_channel(struct ice_vsi *vsi, struct ice_channel *ch)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;

	if (!ch)
		return -EINVAL;

	dev = ice_pf_to_dev(pf);
	if (!ch->num_txq || !ch->num_rxq) {
		dev_err(dev, "Invalid num_queues requested: %d\n", ch->num_rxq);
		return -EINVAL;
	}

	if (!vsi->cnt_q_avail || vsi->cnt_q_avail < ch->num_txq) {
		dev_err(dev, "cnt_q_avail (%u) less than num_queues %d\n",
			vsi->cnt_q_avail, ch->num_txq);
		return -EINVAL;
	}

	if (!ice_setup_channel(pf, vsi, ch)) {
		dev_info(dev, "Failed to setup channel\n");
		return -EINVAL;
	}
	 
	if (ch->ch_vsi && (ch->max_tx_rate || ch->min_tx_rate)) {
		int ret;

		ret = ice_set_bw_limit(ch->ch_vsi, ch->max_tx_rate,
				       ch->min_tx_rate);
		if (ret)
			dev_err(dev, "failed to set Tx rate of %llu Kbps for VSI(%u)\n",
				ch->max_tx_rate, ch->ch_vsi->vsi_num);
		else
			dev_dbg(dev, "set Tx rate of %llu Kbps for VSI(%u)\n",
				ch->max_tx_rate, ch->ch_vsi->vsi_num);
	}

	vsi->cnt_q_avail -= ch->num_txq;

	return 0;
}

 
static void ice_rem_all_chnl_fltrs(struct ice_pf *pf)
{
	struct ice_tc_flower_fltr *fltr;
	struct hlist_node *node;

	 
	hlist_for_each_entry_safe(fltr, node,
				  &pf->tc_flower_fltr_list,
				  tc_flower_node) {
		struct ice_rule_query_data rule;
		int status;

		 
		if (!ice_is_chnl_fltr(fltr))
			continue;

		rule.rid = fltr->rid;
		rule.rule_id = fltr->rule_id;
		rule.vsi_handle = fltr->dest_vsi_handle;
		status = ice_rem_adv_rule_by_id(&pf->hw, &rule);
		if (status) {
			if (status == -ENOENT)
				dev_dbg(ice_pf_to_dev(pf), "TC flower filter (rule_id %u) does not exist\n",
					rule.rule_id);
			else
				dev_err(ice_pf_to_dev(pf), "failed to delete TC flower filter, status %d\n",
					status);
		} else if (fltr->dest_vsi) {
			 
			if (fltr->dest_vsi->type == ICE_VSI_CHNL) {
				u32 flags = fltr->flags;

				fltr->dest_vsi->num_chnl_fltr--;
				if (flags & (ICE_TC_FLWR_FIELD_DST_MAC |
					     ICE_TC_FLWR_FIELD_ENC_DST_MAC))
					pf->num_dmac_chnl_fltrs--;
			}
		}

		hlist_del(&fltr->tc_flower_node);
		kfree(fltr);
	}
}

 
static void ice_remove_q_channels(struct ice_vsi *vsi, bool rem_fltr)
{
	struct ice_channel *ch, *ch_tmp;
	struct ice_pf *pf = vsi->back;
	int i;

	 
	if (rem_fltr)
		ice_rem_all_chnl_fltrs(pf);

	 
	if  (vsi->netdev->features & NETIF_F_NTUPLE) {
		struct ice_hw *hw = &pf->hw;

		mutex_lock(&hw->fdir_fltr_lock);
		ice_fdir_del_all_fltrs(vsi);
		mutex_unlock(&hw->fdir_fltr_lock);
	}

	 
	list_for_each_entry_safe(ch, ch_tmp, &vsi->ch_list, list) {
		struct ice_vsi *ch_vsi;

		list_del(&ch->list);
		ch_vsi = ch->ch_vsi;
		if (!ch_vsi) {
			kfree(ch);
			continue;
		}

		 
		for (i = 0; i < ch->num_rxq; i++) {
			struct ice_tx_ring *tx_ring;
			struct ice_rx_ring *rx_ring;

			tx_ring = vsi->tx_rings[ch->base_q + i];
			rx_ring = vsi->rx_rings[ch->base_q + i];
			if (tx_ring) {
				tx_ring->ch = NULL;
				if (tx_ring->q_vector)
					tx_ring->q_vector->ch = NULL;
			}
			if (rx_ring) {
				rx_ring->ch = NULL;
				if (rx_ring->q_vector)
					rx_ring->q_vector->ch = NULL;
			}
		}

		 
		ice_fdir_rem_adq_chnl(&pf->hw, ch->ch_vsi->idx);

		 
		ice_rm_vsi_lan_cfg(ch->ch_vsi->port_info, ch->ch_vsi->idx);

		 
		ice_vsi_delete(ch->ch_vsi);

		 
		kfree(ch);
	}

	 
	ice_for_each_chnl_tc(i)
		vsi->tc_map_vsi[i] = NULL;

	 
	vsi->all_enatc = 0;
	vsi->all_numtc = 0;
}

 
static int ice_rebuild_channels(struct ice_pf *pf)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vsi *main_vsi;
	bool rem_adv_fltr = true;
	struct ice_channel *ch;
	struct ice_vsi *vsi;
	int tc_idx = 1;
	int i, err;

	main_vsi = ice_get_main_vsi(pf);
	if (!main_vsi)
		return 0;

	if (!test_bit(ICE_FLAG_TC_MQPRIO, pf->flags) ||
	    main_vsi->old_numtc == 1)
		return 0;  

	 
	err = ice_vsi_cfg_tc(main_vsi, main_vsi->old_ena_tc);
	if (err) {
		dev_err(dev, "failed configuring TC(ena_tc:0x%02x) for HW VSI=%u\n",
			main_vsi->old_ena_tc, main_vsi->vsi_num);
		return err;
	}

	 
	ice_for_each_vsi(pf, i) {
		enum ice_vsi_type type;

		vsi = pf->vsi[i];
		if (!vsi || vsi->type != ICE_VSI_CHNL)
			continue;

		type = vsi->type;

		 
		err = ice_vsi_rebuild(vsi, ICE_VSI_FLAG_INIT);
		if (err) {
			dev_err(dev, "VSI (type:%s) at index %d rebuild failed, err %d\n",
				ice_vsi_type_str(type), vsi->idx, err);
			goto cleanup;
		}

		 
		vsi->vsi_num = ice_get_hw_vsi_num(&pf->hw, vsi->idx);

		 
		err = ice_replay_vsi(&pf->hw, vsi->idx);
		if (err) {
			dev_err(dev, "VSI (type:%s) replay failed, err %d, VSI index %d\n",
				ice_vsi_type_str(type), err, vsi->idx);
			rem_adv_fltr = false;
			goto cleanup;
		}
		dev_info(dev, "VSI (type:%s) at index %d rebuilt successfully\n",
			 ice_vsi_type_str(type), vsi->idx);

		 
		main_vsi->tc_map_vsi[tc_idx++] = vsi;
	}

	 
	list_for_each_entry(ch, &main_vsi->ch_list, list) {
		struct ice_vsi *ch_vsi;

		ch_vsi = ch->ch_vsi;
		if (!ch_vsi)
			continue;

		 
		ice_cfg_chnl_all_res(main_vsi, ch);

		 
		if (!ch->max_tx_rate && !ch->min_tx_rate)
			continue;

		err = ice_set_bw_limit(ch_vsi, ch->max_tx_rate,
				       ch->min_tx_rate);
		if (err)
			dev_err(dev, "failed (err:%d) to rebuild BW rate limit, max_tx_rate: %llu Kbps, min_tx_rate: %llu Kbps for VSI(%u)\n",
				err, ch->max_tx_rate, ch->min_tx_rate,
				ch_vsi->vsi_num);
		else
			dev_dbg(dev, "successfully rebuild BW rate limit, max_tx_rate: %llu Kbps, min_tx_rate: %llu Kbps for VSI(%u)\n",
				ch->max_tx_rate, ch->min_tx_rate,
				ch_vsi->vsi_num);
	}

	 
	if (main_vsi->ch_rss_size)
		ice_vsi_cfg_rss_lut_key(main_vsi);

	return 0;

cleanup:
	ice_remove_q_channels(main_vsi, rem_adv_fltr);
	return err;
}

 
static int ice_create_q_channels(struct ice_vsi *vsi)
{
	struct ice_pf *pf = vsi->back;
	struct ice_channel *ch;
	int ret = 0, i;

	ice_for_each_chnl_tc(i) {
		if (!(vsi->all_enatc & BIT(i)))
			continue;

		ch = kzalloc(sizeof(*ch), GFP_KERNEL);
		if (!ch) {
			ret = -ENOMEM;
			goto err_free;
		}
		INIT_LIST_HEAD(&ch->list);
		ch->num_rxq = vsi->mqprio_qopt.qopt.count[i];
		ch->num_txq = vsi->mqprio_qopt.qopt.count[i];
		ch->base_q = vsi->mqprio_qopt.qopt.offset[i];
		ch->max_tx_rate = vsi->mqprio_qopt.max_rate[i];
		ch->min_tx_rate = vsi->mqprio_qopt.min_rate[i];

		 
		if (ch->max_tx_rate)
			ch->max_tx_rate = div_u64(ch->max_tx_rate,
						  ICE_BW_KBPS_DIVISOR);
		if (ch->min_tx_rate)
			ch->min_tx_rate = div_u64(ch->min_tx_rate,
						  ICE_BW_KBPS_DIVISOR);

		ret = ice_create_q_channel(vsi, ch);
		if (ret) {
			dev_err(ice_pf_to_dev(pf),
				"failed creating channel TC:%d\n", i);
			kfree(ch);
			goto err_free;
		}
		list_add_tail(&ch->list, &vsi->ch_list);
		vsi->tc_map_vsi[i] = ch->ch_vsi;
		dev_dbg(ice_pf_to_dev(pf),
			"successfully created channel: VSI %pK\n", ch->ch_vsi);
	}
	return 0;

err_free:
	ice_remove_q_channels(vsi, false);

	return ret;
}

 
static int ice_setup_tc_mqprio_qdisc(struct net_device *netdev, void *type_data)
{
	struct tc_mqprio_qopt_offload *mqprio_qopt = type_data;
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	u16 mode, ena_tc_qdisc = 0;
	int cur_txq, cur_rxq;
	u8 hw = 0, num_tcf;
	struct device *dev;
	int ret, i;

	dev = ice_pf_to_dev(pf);
	num_tcf = mqprio_qopt->qopt.num_tc;
	hw = mqprio_qopt->qopt.hw;
	mode = mqprio_qopt->mode;
	if (!hw) {
		clear_bit(ICE_FLAG_TC_MQPRIO, pf->flags);
		vsi->ch_rss_size = 0;
		memcpy(&vsi->mqprio_qopt, mqprio_qopt, sizeof(*mqprio_qopt));
		goto config_tcf;
	}

	 
	for (i = 0; i < num_tcf; i++)
		ena_tc_qdisc |= BIT(i);

	switch (mode) {
	case TC_MQPRIO_MODE_CHANNEL:

		if (pf->hw.port_info->is_custom_tx_enabled) {
			dev_err(dev, "Custom Tx scheduler feature enabled, can't configure ADQ\n");
			return -EBUSY;
		}
		ice_tear_down_devlink_rate_tree(pf);

		ret = ice_validate_mqprio_qopt(vsi, mqprio_qopt);
		if (ret) {
			netdev_err(netdev, "failed to validate_mqprio_qopt(), ret %d\n",
				   ret);
			return ret;
		}
		memcpy(&vsi->mqprio_qopt, mqprio_qopt, sizeof(*mqprio_qopt));
		set_bit(ICE_FLAG_TC_MQPRIO, pf->flags);
		 
		if (vsi->netdev->features & NETIF_F_HW_TC)
			set_bit(ICE_FLAG_CLS_FLOWER, pf->flags);
		break;
	default:
		return -EINVAL;
	}

config_tcf:

	 
	if (ena_tc_qdisc == vsi->tc_cfg.ena_tc &&
	    mode != TC_MQPRIO_MODE_CHANNEL)
		return 0;

	 
	ice_dis_vsi(vsi, true);

	if (!hw && !test_bit(ICE_FLAG_TC_MQPRIO, pf->flags))
		ice_remove_q_channels(vsi, true);

	if (!hw && !test_bit(ICE_FLAG_TC_MQPRIO, pf->flags)) {
		vsi->req_txq = min_t(int, ice_get_avail_txq_count(pf),
				     num_online_cpus());
		vsi->req_rxq = min_t(int, ice_get_avail_rxq_count(pf),
				     num_online_cpus());
	} else {
		 
		u16 offset = 0, qcount_tx = 0, qcount_rx = 0;

		for (i = 0; i < num_tcf; i++) {
			if (!(ena_tc_qdisc & BIT(i)))
				continue;

			offset = vsi->mqprio_qopt.qopt.offset[i];
			qcount_rx = vsi->mqprio_qopt.qopt.count[i];
			qcount_tx = vsi->mqprio_qopt.qopt.count[i];
		}
		vsi->req_txq = offset + qcount_tx;
		vsi->req_rxq = offset + qcount_rx;

		 
		vsi->orig_rss_size = vsi->rss_size;
	}

	 
	cur_txq = vsi->num_txq;
	cur_rxq = vsi->num_rxq;

	 
	ret = ice_vsi_rebuild(vsi, ICE_VSI_FLAG_NO_INIT);
	if (ret) {
		 
		dev_info(dev, "Rebuild failed with new queues, try with current number of queues\n");
		vsi->req_txq = cur_txq;
		vsi->req_rxq = cur_rxq;
		clear_bit(ICE_RESET_FAILED, pf->state);
		if (ice_vsi_rebuild(vsi, ICE_VSI_FLAG_NO_INIT)) {
			dev_err(dev, "Rebuild of main VSI failed again\n");
			return ret;
		}
	}

	vsi->all_numtc = num_tcf;
	vsi->all_enatc = ena_tc_qdisc;
	ret = ice_vsi_cfg_tc(vsi, ena_tc_qdisc);
	if (ret) {
		netdev_err(netdev, "failed configuring TC for VSI id=%d\n",
			   vsi->vsi_num);
		goto exit;
	}

	if (test_bit(ICE_FLAG_TC_MQPRIO, pf->flags)) {
		u64 max_tx_rate = vsi->mqprio_qopt.max_rate[0];
		u64 min_tx_rate = vsi->mqprio_qopt.min_rate[0];

		 
		if (max_tx_rate || min_tx_rate) {
			 
			if (max_tx_rate)
				max_tx_rate = div_u64(max_tx_rate, ICE_BW_KBPS_DIVISOR);
			if (min_tx_rate)
				min_tx_rate = div_u64(min_tx_rate, ICE_BW_KBPS_DIVISOR);

			ret = ice_set_bw_limit(vsi, max_tx_rate, min_tx_rate);
			if (!ret) {
				dev_dbg(dev, "set Tx rate max %llu min %llu for VSI(%u)\n",
					max_tx_rate, min_tx_rate, vsi->vsi_num);
			} else {
				dev_err(dev, "failed to set Tx rate max %llu min %llu for VSI(%u)\n",
					max_tx_rate, min_tx_rate, vsi->vsi_num);
				goto exit;
			}
		}
		ret = ice_create_q_channels(vsi);
		if (ret) {
			netdev_err(netdev, "failed configuring queue channels\n");
			goto exit;
		} else {
			netdev_dbg(netdev, "successfully configured channels\n");
		}
	}

	if (vsi->ch_rss_size)
		ice_vsi_cfg_rss_lut_key(vsi);

exit:
	 
	if (ret) {
		vsi->all_numtc = 0;
		vsi->all_enatc = 0;
	}
	 
	ice_ena_vsi(vsi, true);

	return ret;
}

static LIST_HEAD(ice_block_cb_list);

static int
ice_setup_tc(struct net_device *netdev, enum tc_setup_type type,
	     void *type_data)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_pf *pf = np->vsi->back;
	bool locked = false;
	int err;

	switch (type) {
	case TC_SETUP_BLOCK:
		return flow_block_cb_setup_simple(type_data,
						  &ice_block_cb_list,
						  ice_setup_tc_block_cb,
						  np, np, true);
	case TC_SETUP_QDISC_MQPRIO:
		if (ice_is_eswitch_mode_switchdev(pf)) {
			netdev_err(netdev, "TC MQPRIO offload not supported, switchdev is enabled\n");
			return -EOPNOTSUPP;
		}

		if (pf->adev) {
			mutex_lock(&pf->adev_mutex);
			device_lock(&pf->adev->dev);
			locked = true;
			if (pf->adev->dev.driver) {
				netdev_err(netdev, "Cannot change qdisc when RDMA is active\n");
				err = -EBUSY;
				goto adev_unlock;
			}
		}

		 
		mutex_lock(&pf->tc_mutex);
		err = ice_setup_tc_mqprio_qdisc(netdev, type_data);
		mutex_unlock(&pf->tc_mutex);

adev_unlock:
		if (locked) {
			device_unlock(&pf->adev->dev);
			mutex_unlock(&pf->adev_mutex);
		}
		return err;
	default:
		return -EOPNOTSUPP;
	}
	return -EOPNOTSUPP;
}

static struct ice_indr_block_priv *
ice_indr_block_priv_lookup(struct ice_netdev_priv *np,
			   struct net_device *netdev)
{
	struct ice_indr_block_priv *cb_priv;

	list_for_each_entry(cb_priv, &np->tc_indr_block_priv_list, list) {
		if (!cb_priv->netdev)
			return NULL;
		if (cb_priv->netdev == netdev)
			return cb_priv;
	}
	return NULL;
}

static int
ice_indr_setup_block_cb(enum tc_setup_type type, void *type_data,
			void *indr_priv)
{
	struct ice_indr_block_priv *priv = indr_priv;
	struct ice_netdev_priv *np = priv->np;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return ice_setup_tc_cls_flower(np, priv->netdev,
					       (struct flow_cls_offload *)
					       type_data);
	default:
		return -EOPNOTSUPP;
	}
}

static int
ice_indr_setup_tc_block(struct net_device *netdev, struct Qdisc *sch,
			struct ice_netdev_priv *np,
			struct flow_block_offload *f, void *data,
			void (*cleanup)(struct flow_block_cb *block_cb))
{
	struct ice_indr_block_priv *indr_priv;
	struct flow_block_cb *block_cb;

	if (!ice_is_tunnel_supported(netdev) &&
	    !(is_vlan_dev(netdev) &&
	      vlan_dev_real_dev(netdev) == np->vsi->netdev))
		return -EOPNOTSUPP;

	if (f->binder_type != FLOW_BLOCK_BINDER_TYPE_CLSACT_INGRESS)
		return -EOPNOTSUPP;

	switch (f->command) {
	case FLOW_BLOCK_BIND:
		indr_priv = ice_indr_block_priv_lookup(np, netdev);
		if (indr_priv)
			return -EEXIST;

		indr_priv = kzalloc(sizeof(*indr_priv), GFP_KERNEL);
		if (!indr_priv)
			return -ENOMEM;

		indr_priv->netdev = netdev;
		indr_priv->np = np;
		list_add(&indr_priv->list, &np->tc_indr_block_priv_list);

		block_cb =
			flow_indr_block_cb_alloc(ice_indr_setup_block_cb,
						 indr_priv, indr_priv,
						 ice_rep_indr_tc_block_unbind,
						 f, netdev, sch, data, np,
						 cleanup);

		if (IS_ERR(block_cb)) {
			list_del(&indr_priv->list);
			kfree(indr_priv);
			return PTR_ERR(block_cb);
		}
		flow_block_cb_add(block_cb, f);
		list_add_tail(&block_cb->driver_list, &ice_block_cb_list);
		break;
	case FLOW_BLOCK_UNBIND:
		indr_priv = ice_indr_block_priv_lookup(np, netdev);
		if (!indr_priv)
			return -ENOENT;

		block_cb = flow_block_cb_lookup(f->block,
						ice_indr_setup_block_cb,
						indr_priv);
		if (!block_cb)
			return -ENOENT;

		flow_indr_block_cb_remove(block_cb, f);

		list_del(&block_cb->driver_list);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

static int
ice_indr_setup_tc_cb(struct net_device *netdev, struct Qdisc *sch,
		     void *cb_priv, enum tc_setup_type type, void *type_data,
		     void *data,
		     void (*cleanup)(struct flow_block_cb *block_cb))
{
	switch (type) {
	case TC_SETUP_BLOCK:
		return ice_indr_setup_tc_block(netdev, sch, cb_priv, type_data,
					       data, cleanup);

	default:
		return -EOPNOTSUPP;
	}
}

 
int ice_open(struct net_device *netdev)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_pf *pf = np->vsi->back;

	if (ice_is_reset_in_progress(pf->state)) {
		netdev_err(netdev, "can't open net device while reset is in progress");
		return -EBUSY;
	}

	return ice_open_internal(netdev);
}

 
int ice_open_internal(struct net_device *netdev)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;
	struct ice_port_info *pi;
	int err;

	if (test_bit(ICE_NEEDS_RESTART, pf->state)) {
		netdev_err(netdev, "driver needs to be unloaded and reloaded\n");
		return -EIO;
	}

	netif_carrier_off(netdev);

	pi = vsi->port_info;
	err = ice_update_link_info(pi);
	if (err) {
		netdev_err(netdev, "Failed to get link info, error %d\n", err);
		return err;
	}

	ice_check_link_cfg_err(pf, pi->phy.link_info.link_cfg_err);

	 
	if (pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE) {
		clear_bit(ICE_FLAG_NO_MEDIA, pf->flags);
		if (!test_bit(ICE_PHY_INIT_COMPLETE, pf->state)) {
			err = ice_init_phy_user_cfg(pi);
			if (err) {
				netdev_err(netdev, "Failed to initialize PHY settings, error %d\n",
					   err);
				return err;
			}
		}

		err = ice_configure_phy(vsi);
		if (err) {
			netdev_err(netdev, "Failed to set physical link up, error %d\n",
				   err);
			return err;
		}
	} else {
		set_bit(ICE_FLAG_NO_MEDIA, pf->flags);
		ice_set_link(vsi, false);
	}

	err = ice_vsi_open(vsi);
	if (err)
		netdev_err(netdev, "Failed to open VSI 0x%04X on switch 0x%04X\n",
			   vsi->vsi_num, vsi->vsw->sw_id);

	 
	udp_tunnel_get_rx_info(netdev);

	return err;
}

 
int ice_stop(struct net_device *netdev)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_vsi *vsi = np->vsi;
	struct ice_pf *pf = vsi->back;

	if (ice_is_reset_in_progress(pf->state)) {
		netdev_err(netdev, "can't stop net device while reset is in progress");
		return -EBUSY;
	}

	if (test_bit(ICE_FLAG_LINK_DOWN_ON_CLOSE_ENA, vsi->back->flags)) {
		int link_err = ice_force_phys_link_state(vsi, false);

		if (link_err) {
			if (link_err == -ENOMEDIUM)
				netdev_info(vsi->netdev, "Skipping link reconfig - no media attached, VSI %d\n",
					    vsi->vsi_num);
			else
				netdev_err(vsi->netdev, "Failed to set physical link down, VSI %d error %d\n",
					   vsi->vsi_num, link_err);

			ice_vsi_close(vsi);
			return -EIO;
		}
	}

	ice_vsi_close(vsi);

	return 0;
}

 
static netdev_features_t
ice_features_check(struct sk_buff *skb,
		   struct net_device __always_unused *netdev,
		   netdev_features_t features)
{
	bool gso = skb_is_gso(skb);
	size_t len;

	 
	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return features;

	 
	if (gso && (skb_shinfo(skb)->gso_size < ICE_TXD_CTX_MIN_MSS))
		features &= ~NETIF_F_GSO_MASK;

	len = skb_network_offset(skb);
	if (len > ICE_TXD_MACLEN_MAX || len & 0x1)
		goto out_rm_features;

	len = skb_network_header_len(skb);
	if (len > ICE_TXD_IPLEN_MAX || len & 0x1)
		goto out_rm_features;

	if (skb->encapsulation) {
		 
		if (gso && (skb_shinfo(skb)->gso_type &
			    (SKB_GSO_GRE | SKB_GSO_UDP_TUNNEL))) {
			len = skb_inner_network_header(skb) -
			      skb_transport_header(skb);
			if (len > ICE_TXD_L4LEN_MAX || len & 0x1)
				goto out_rm_features;
		}

		len = skb_inner_network_header_len(skb);
		if (len > ICE_TXD_IPLEN_MAX || len & 0x1)
			goto out_rm_features;
	}

	return features;
out_rm_features:
	return features & ~(NETIF_F_CSUM_MASK | NETIF_F_GSO_MASK);
}

static const struct net_device_ops ice_netdev_safe_mode_ops = {
	.ndo_open = ice_open,
	.ndo_stop = ice_stop,
	.ndo_start_xmit = ice_start_xmit,
	.ndo_set_mac_address = ice_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_change_mtu = ice_change_mtu,
	.ndo_get_stats64 = ice_get_stats64,
	.ndo_tx_timeout = ice_tx_timeout,
	.ndo_bpf = ice_xdp_safe_mode,
};

static const struct net_device_ops ice_netdev_ops = {
	.ndo_open = ice_open,
	.ndo_stop = ice_stop,
	.ndo_start_xmit = ice_start_xmit,
	.ndo_select_queue = ice_select_queue,
	.ndo_features_check = ice_features_check,
	.ndo_fix_features = ice_fix_features,
	.ndo_set_rx_mode = ice_set_rx_mode,
	.ndo_set_mac_address = ice_set_mac_address,
	.ndo_validate_addr = eth_validate_addr,
	.ndo_change_mtu = ice_change_mtu,
	.ndo_get_stats64 = ice_get_stats64,
	.ndo_set_tx_maxrate = ice_set_tx_maxrate,
	.ndo_eth_ioctl = ice_eth_ioctl,
	.ndo_set_vf_spoofchk = ice_set_vf_spoofchk,
	.ndo_set_vf_mac = ice_set_vf_mac,
	.ndo_get_vf_config = ice_get_vf_cfg,
	.ndo_set_vf_trust = ice_set_vf_trust,
	.ndo_set_vf_vlan = ice_set_vf_port_vlan,
	.ndo_set_vf_link_state = ice_set_vf_link_state,
	.ndo_get_vf_stats = ice_get_vf_stats,
	.ndo_set_vf_rate = ice_set_vf_bw,
	.ndo_vlan_rx_add_vid = ice_vlan_rx_add_vid,
	.ndo_vlan_rx_kill_vid = ice_vlan_rx_kill_vid,
	.ndo_setup_tc = ice_setup_tc,
	.ndo_set_features = ice_set_features,
	.ndo_bridge_getlink = ice_bridge_getlink,
	.ndo_bridge_setlink = ice_bridge_setlink,
	.ndo_fdb_add = ice_fdb_add,
	.ndo_fdb_del = ice_fdb_del,
#ifdef CONFIG_RFS_ACCEL
	.ndo_rx_flow_steer = ice_rx_flow_steer,
#endif
	.ndo_tx_timeout = ice_tx_timeout,
	.ndo_bpf = ice_xdp,
	.ndo_xdp_xmit = ice_xdp_xmit,
	.ndo_xsk_wakeup = ice_xsk_wakeup,
};

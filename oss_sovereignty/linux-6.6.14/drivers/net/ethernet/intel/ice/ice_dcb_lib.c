
 

#include "ice_dcb_lib.h"
#include "ice_dcb_nl.h"
#include "ice_devlink.h"

 
static u8 ice_dcb_get_ena_tc(struct ice_dcbx_cfg *dcbcfg)
{
	u8 i, num_tc, ena_tc = 1;

	num_tc = ice_dcb_get_num_tc(dcbcfg);

	for (i = 0; i < num_tc; i++)
		ena_tc |= BIT(i);

	return ena_tc;
}

 
bool ice_is_pfc_causing_hung_q(struct ice_pf *pf, unsigned int txqueue)
{
	u8 num_tcs = 0, i, tc, up_mapped_tc, up_in_tc = 0;
	u64 ref_prio_xoff[ICE_MAX_UP];
	struct ice_vsi *vsi;
	u32 up2tc;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return false;

	ice_for_each_traffic_class(i)
		if (vsi->tc_cfg.ena_tc & BIT(i))
			num_tcs++;

	 
	for (tc = 0; tc < num_tcs - 1; tc++)
		if (ice_find_q_in_range(vsi->tc_cfg.tc_info[tc].qoffset,
					vsi->tc_cfg.tc_info[tc + 1].qoffset,
					txqueue))
			break;

	 
	up2tc = rd32(&pf->hw, PRTDCB_TUP2TC);
	for (i = 0; i < ICE_MAX_UP; i++) {
		up_mapped_tc = (up2tc >> (i * 3)) & 0x7;
		if (up_mapped_tc == tc)
			up_in_tc |= BIT(i);
	}

	 
	for (i = 0; i < ICE_MAX_UP; i++)
		if (up_in_tc & BIT(i))
			ref_prio_xoff[i] = pf->stats.priority_xoff_rx[i];

	ice_update_dcb_stats(pf);

	for (i = 0; i < ICE_MAX_UP; i++)
		if (up_in_tc & BIT(i))
			if (pf->stats.priority_xoff_rx[i] > ref_prio_xoff[i])
				return true;

	return false;
}

 
static u8 ice_dcb_get_mode(struct ice_port_info *port_info, bool host)
{
	u8 mode;

	if (host)
		mode = DCB_CAP_DCBX_HOST;
	else
		mode = DCB_CAP_DCBX_LLD_MANAGED;

	if (port_info->qos_cfg.local_dcbx_cfg.dcbx_mode & ICE_DCBX_MODE_CEE)
		return mode | DCB_CAP_DCBX_VER_CEE;
	else
		return mode | DCB_CAP_DCBX_VER_IEEE;
}

 
u8 ice_dcb_get_num_tc(struct ice_dcbx_cfg *dcbcfg)
{
	bool tc_unused = false;
	u8 num_tc = 0;
	u8 ret = 0;
	int i;

	 
	for (i = 0; i < CEE_DCBX_MAX_PRIO; i++)
		num_tc |= BIT(dcbcfg->etscfg.prio_table[i]);

	 
	for (i = 0; i < IEEE_8021QAZ_MAX_TCS; i++) {
		if (num_tc & BIT(i)) {
			if (!tc_unused) {
				ret++;
			} else {
				pr_err("Non-contiguous TCs - Disabling DCB\n");
				return 1;
			}
		} else {
			tc_unused = true;
		}
	}

	 
	if (!ret)
		ret = 1;

	return ret;
}

 
static u8 ice_get_first_droptc(struct ice_vsi *vsi)
{
	struct ice_dcbx_cfg *cfg = &vsi->port_info->qos_cfg.local_dcbx_cfg;
	struct device *dev = ice_pf_to_dev(vsi->back);
	u8 num_tc, ena_tc_map, pfc_ena_map;
	u8 i;

	num_tc = ice_dcb_get_num_tc(cfg);

	 
	ena_tc_map = ice_dcb_get_ena_tc(cfg);

	 
	pfc_ena_map = cfg->pfc.pfcena;

	 
	for (i = 0; i < num_tc; i++) {
		if ((ena_tc_map & BIT(i)) && (!(pfc_ena_map & BIT(i)))) {
			dev_dbg(dev, "first drop tc = %d\n", i);
			return i;
		}
	}

	dev_dbg(dev, "first drop tc = 0\n");
	return 0;
}

 
void ice_vsi_set_dcb_tc_cfg(struct ice_vsi *vsi)
{
	struct ice_dcbx_cfg *cfg = &vsi->port_info->qos_cfg.local_dcbx_cfg;

	switch (vsi->type) {
	case ICE_VSI_PF:
		vsi->tc_cfg.ena_tc = ice_dcb_get_ena_tc(cfg);
		vsi->tc_cfg.numtc = ice_dcb_get_num_tc(cfg);
		break;
	case ICE_VSI_CHNL:
		vsi->tc_cfg.ena_tc = BIT(ice_get_first_droptc(vsi));
		vsi->tc_cfg.numtc = 1;
		break;
	case ICE_VSI_CTRL:
	case ICE_VSI_LB:
	default:
		vsi->tc_cfg.ena_tc = ICE_DFLT_TRAFFIC_CLASS;
		vsi->tc_cfg.numtc = 1;
	}
}

 
u8 ice_dcb_get_tc(struct ice_vsi *vsi, int queue_index)
{
	return vsi->tx_rings[queue_index]->dcb_tc;
}

 
void ice_vsi_cfg_dcb_rings(struct ice_vsi *vsi)
{
	struct ice_tx_ring *tx_ring;
	struct ice_rx_ring *rx_ring;
	u16 qoffset, qcount;
	int i, n;

	if (!test_bit(ICE_FLAG_DCB_ENA, vsi->back->flags)) {
		 
		ice_for_each_txq(vsi, i) {
			tx_ring = vsi->tx_rings[i];
			tx_ring->dcb_tc = 0;
		}
		ice_for_each_rxq(vsi, i) {
			rx_ring = vsi->rx_rings[i];
			rx_ring->dcb_tc = 0;
		}
		return;
	}

	ice_for_each_traffic_class(n) {
		if (!(vsi->tc_cfg.ena_tc & BIT(n)))
			break;

		qoffset = vsi->tc_cfg.tc_info[n].qoffset;
		qcount = vsi->tc_cfg.tc_info[n].qcount_tx;
		for (i = qoffset; i < (qoffset + qcount); i++)
			vsi->tx_rings[i]->dcb_tc = n;

		qcount = vsi->tc_cfg.tc_info[n].qcount_rx;
		for (i = qoffset; i < (qoffset + qcount); i++)
			vsi->rx_rings[i]->dcb_tc = n;
	}
	 
	if (vsi->all_enatc) {
		u8 first_droptc = ice_get_first_droptc(vsi);

		 
		ice_for_each_chnl_tc(n) {
			if (!(vsi->all_enatc & BIT(n)))
				break;

			qoffset = vsi->mqprio_qopt.qopt.offset[n];
			qcount = vsi->mqprio_qopt.qopt.count[n];
			for (i = qoffset; i < (qoffset + qcount); i++) {
				vsi->tx_rings[i]->dcb_tc = first_droptc;
				vsi->rx_rings[i]->dcb_tc = first_droptc;
			}
		}
	}
}

 
static void ice_dcb_ena_dis_vsi(struct ice_pf *pf, bool ena, bool locked)
{
	int i;

	ice_for_each_vsi(pf, i) {
		struct ice_vsi *vsi = pf->vsi[i];

		if (!vsi)
			continue;

		switch (vsi->type) {
		case ICE_VSI_CHNL:
		case ICE_VSI_SWITCHDEV_CTRL:
		case ICE_VSI_PF:
			if (ena)
				ice_ena_vsi(vsi, locked);
			else
				ice_dis_vsi(vsi, locked);
			break;
		default:
			continue;
		}
	}
}

 
int ice_dcb_bwchk(struct ice_pf *pf, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg = &dcbcfg->etscfg;
	u8 num_tc, total_bw = 0;
	int i;

	 
	num_tc = ice_dcb_get_num_tc(dcbcfg);

	 
	if (num_tc == 1) {
		etscfg->tcbwtable[0] = ICE_TC_MAX_BW;
		return 0;
	}

	for (i = 0; i < num_tc; i++)
		total_bw += etscfg->tcbwtable[i];

	if (!total_bw) {
		etscfg->tcbwtable[0] = ICE_TC_MAX_BW;
	} else if (total_bw != ICE_TC_MAX_BW) {
		dev_err(ice_pf_to_dev(pf), "Invalid config, total bandwidth must equal 100\n");
		return -EINVAL;
	}

	return 0;
}

 
int ice_pf_dcb_cfg(struct ice_pf *pf, struct ice_dcbx_cfg *new_cfg, bool locked)
{
	struct ice_aqc_port_ets_elem buf = { 0 };
	struct ice_dcbx_cfg *old_cfg, *curr_cfg;
	struct device *dev = ice_pf_to_dev(pf);
	int ret = ICE_DCB_NO_HW_CHG;
	struct iidc_event *event;
	struct ice_vsi *pf_vsi;

	curr_cfg = &pf->hw.port_info->qos_cfg.local_dcbx_cfg;

	 
	if (!pf->hw.port_info->qos_cfg.is_sw_lldp)
		ret = ICE_DCB_HW_CHG_RST;

	 
	if (ice_dcb_get_num_tc(new_cfg) > 1) {
		dev_dbg(dev, "DCB tagging enabled (num TC > 1)\n");
		if (pf->hw.port_info->is_custom_tx_enabled) {
			dev_err(dev, "Custom Tx scheduler feature enabled, can't configure DCB\n");
			return -EBUSY;
		}
		ice_tear_down_devlink_rate_tree(pf);

		set_bit(ICE_FLAG_DCB_ENA, pf->flags);
	} else {
		dev_dbg(dev, "DCB tagging disabled (num TC = 1)\n");
		clear_bit(ICE_FLAG_DCB_ENA, pf->flags);
	}

	if (!memcmp(new_cfg, curr_cfg, sizeof(*new_cfg))) {
		dev_dbg(dev, "No change in DCB config required\n");
		return ret;
	}

	if (ice_dcb_bwchk(pf, new_cfg))
		return -EINVAL;

	 
	old_cfg = kmemdup(curr_cfg, sizeof(*old_cfg), GFP_KERNEL);
	if (!old_cfg)
		return -ENOMEM;

	dev_info(dev, "Commit DCB Configuration to the hardware\n");
	pf_vsi = ice_get_main_vsi(pf);
	if (!pf_vsi) {
		dev_dbg(dev, "PF VSI doesn't exist\n");
		ret = -EINVAL;
		goto free_cfg;
	}

	 
	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event) {
		ret = -ENOMEM;
		goto free_cfg;
	}

	set_bit(IIDC_EVENT_BEFORE_TC_CHANGE, event->type);
	ice_send_event_to_aux(pf, event);
	kfree(event);

	 
	if (!locked)
		rtnl_lock();

	 
	ice_dcb_ena_dis_vsi(pf, false, true);

	memcpy(curr_cfg, new_cfg, sizeof(*curr_cfg));
	memcpy(&curr_cfg->etsrec, &curr_cfg->etscfg, sizeof(curr_cfg->etsrec));
	memcpy(&new_cfg->etsrec, &curr_cfg->etscfg, sizeof(curr_cfg->etsrec));

	 
	if (pf->hw.port_info->qos_cfg.is_sw_lldp) {
		ret = ice_set_dcb_cfg(pf->hw.port_info);
		if (ret) {
			dev_err(dev, "Set DCB Config failed\n");
			 
			memcpy(curr_cfg, old_cfg, sizeof(*curr_cfg));
			goto out;
		}
	}

	ret = ice_query_port_ets(pf->hw.port_info, &buf, sizeof(buf), NULL);
	if (ret) {
		dev_err(dev, "Query Port ETS failed\n");
		goto out;
	}

	ice_pf_dcb_recfg(pf, false);

out:
	 
	ice_dcb_ena_dis_vsi(pf, true, true);
	if (!locked)
		rtnl_unlock();
free_cfg:
	kfree(old_cfg);
	return ret;
}

 
static void ice_cfg_etsrec_defaults(struct ice_port_info *pi)
{
	struct ice_dcbx_cfg *dcbcfg = &pi->qos_cfg.local_dcbx_cfg;
	u8 i;

	 
	if (dcbcfg->etsrec.maxtcs)
		return;

	 
	dcbcfg->etsrec.maxtcs = 1;
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		dcbcfg->etsrec.tcbwtable[i] = i ? 0 : 100;
		dcbcfg->etsrec.tsatable[i] = i ? ICE_IEEE_TSA_STRICT :
						 ICE_IEEE_TSA_ETS;
	}
}

 
static bool
ice_dcb_need_recfg(struct ice_pf *pf, struct ice_dcbx_cfg *old_cfg,
		   struct ice_dcbx_cfg *new_cfg)
{
	struct device *dev = ice_pf_to_dev(pf);
	bool need_reconfig = false;

	 
	if (memcmp(&new_cfg->etscfg, &old_cfg->etscfg,
		   sizeof(new_cfg->etscfg))) {
		 
		if (memcmp(&new_cfg->etscfg.prio_table,
			   &old_cfg->etscfg.prio_table,
			   sizeof(new_cfg->etscfg.prio_table))) {
			need_reconfig = true;
			dev_dbg(dev, "ETS UP2TC changed.\n");
		}

		if (memcmp(&new_cfg->etscfg.tcbwtable,
			   &old_cfg->etscfg.tcbwtable,
			   sizeof(new_cfg->etscfg.tcbwtable)))
			dev_dbg(dev, "ETS TC BW Table changed.\n");

		if (memcmp(&new_cfg->etscfg.tsatable,
			   &old_cfg->etscfg.tsatable,
			   sizeof(new_cfg->etscfg.tsatable)))
			dev_dbg(dev, "ETS TSA Table changed.\n");
	}

	 
	if (memcmp(&new_cfg->pfc, &old_cfg->pfc, sizeof(new_cfg->pfc))) {
		need_reconfig = true;
		dev_dbg(dev, "PFC config change detected.\n");
	}

	 
	if (memcmp(&new_cfg->app, &old_cfg->app, sizeof(new_cfg->app))) {
		need_reconfig = true;
		dev_dbg(dev, "APP Table change detected.\n");
	}

	dev_dbg(dev, "dcb need_reconfig=%d\n", need_reconfig);
	return need_reconfig;
}

 
void ice_dcb_rebuild(struct ice_pf *pf)
{
	struct ice_aqc_port_ets_elem buf = { 0 };
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_dcbx_cfg *err_cfg;
	int ret;

	ret = ice_query_port_ets(pf->hw.port_info, &buf, sizeof(buf), NULL);
	if (ret) {
		dev_err(dev, "Query Port ETS failed\n");
		goto dcb_error;
	}

	mutex_lock(&pf->tc_mutex);

	if (!pf->hw.port_info->qos_cfg.is_sw_lldp)
		ice_cfg_etsrec_defaults(pf->hw.port_info);

	ret = ice_set_dcb_cfg(pf->hw.port_info);
	if (ret) {
		dev_err(dev, "Failed to set DCB config in rebuild\n");
		goto dcb_error;
	}

	if (!pf->hw.port_info->qos_cfg.is_sw_lldp) {
		ret = ice_cfg_lldp_mib_change(&pf->hw, true);
		if (ret && !pf->hw.port_info->qos_cfg.is_sw_lldp) {
			dev_err(dev, "Failed to register for MIB changes\n");
			goto dcb_error;
		}
	}

	dev_info(dev, "DCB info restored\n");
	ret = ice_query_port_ets(pf->hw.port_info, &buf, sizeof(buf), NULL);
	if (ret) {
		dev_err(dev, "Query Port ETS failed\n");
		goto dcb_error;
	}

	mutex_unlock(&pf->tc_mutex);

	return;

dcb_error:
	dev_err(dev, "Disabling DCB until new settings occur\n");
	err_cfg = kzalloc(sizeof(*err_cfg), GFP_KERNEL);
	if (!err_cfg) {
		mutex_unlock(&pf->tc_mutex);
		return;
	}

	err_cfg->etscfg.willing = true;
	err_cfg->etscfg.tcbwtable[0] = ICE_TC_MAX_BW;
	err_cfg->etscfg.tsatable[0] = ICE_IEEE_TSA_ETS;
	memcpy(&err_cfg->etsrec, &err_cfg->etscfg, sizeof(err_cfg->etsrec));
	 
	 
	ice_pf_dcb_cfg(pf, err_cfg, false);
	kfree(err_cfg);

	mutex_unlock(&pf->tc_mutex);
}

 
static int ice_dcb_init_cfg(struct ice_pf *pf, bool locked)
{
	struct ice_dcbx_cfg *newcfg;
	struct ice_port_info *pi;
	int ret = 0;

	pi = pf->hw.port_info;
	newcfg = kmemdup(&pi->qos_cfg.local_dcbx_cfg, sizeof(*newcfg),
			 GFP_KERNEL);
	if (!newcfg)
		return -ENOMEM;

	memset(&pi->qos_cfg.local_dcbx_cfg, 0, sizeof(*newcfg));

	dev_info(ice_pf_to_dev(pf), "Configuring initial DCB values\n");
	if (ice_pf_dcb_cfg(pf, newcfg, locked))
		ret = -EINVAL;

	kfree(newcfg);

	return ret;
}

 
int ice_dcb_sw_dflt_cfg(struct ice_pf *pf, bool ets_willing, bool locked)
{
	struct ice_aqc_port_ets_elem buf = { 0 };
	struct ice_dcbx_cfg *dcbcfg;
	struct ice_port_info *pi;
	struct ice_hw *hw;
	int ret;

	hw = &pf->hw;
	pi = hw->port_info;
	dcbcfg = kzalloc(sizeof(*dcbcfg), GFP_KERNEL);
	if (!dcbcfg)
		return -ENOMEM;

	memset(&pi->qos_cfg.local_dcbx_cfg, 0, sizeof(*dcbcfg));

	dcbcfg->etscfg.willing = ets_willing ? 1 : 0;
	dcbcfg->etscfg.maxtcs = hw->func_caps.common_cap.maxtc;
	dcbcfg->etscfg.tcbwtable[0] = 100;
	dcbcfg->etscfg.tsatable[0] = ICE_IEEE_TSA_ETS;

	memcpy(&dcbcfg->etsrec, &dcbcfg->etscfg,
	       sizeof(dcbcfg->etsrec));
	dcbcfg->etsrec.willing = 0;

	dcbcfg->pfc.willing = 1;
	dcbcfg->pfc.pfccap = hw->func_caps.common_cap.maxtc;

	dcbcfg->numapps = 1;
	dcbcfg->app[0].selector = ICE_APP_SEL_ETHTYPE;
	dcbcfg->app[0].priority = 3;
	dcbcfg->app[0].prot_id = ETH_P_FCOE;

	ret = ice_pf_dcb_cfg(pf, dcbcfg, locked);
	kfree(dcbcfg);
	if (ret)
		return ret;

	return ice_query_port_ets(pi, &buf, sizeof(buf), NULL);
}

 
static bool ice_dcb_tc_contig(u8 *prio_table)
{
	bool found_empty = false;
	u8 used_tc = 0;
	int i;

	 
	for (i = 0; i < CEE_DCBX_MAX_PRIO; i++)
		used_tc |= BIT(prio_table[i]);

	for (i = 0; i < CEE_DCBX_MAX_PRIO; i++) {
		if (used_tc & BIT(i)) {
			if (found_empty)
				return false;
		} else {
			found_empty = true;
		}
	}

	return true;
}

 
static int ice_dcb_noncontig_cfg(struct ice_pf *pf)
{
	struct ice_dcbx_cfg *dcbcfg = &pf->hw.port_info->qos_cfg.local_dcbx_cfg;
	struct device *dev = ice_pf_to_dev(pf);
	int ret;

	 
	ret = ice_dcb_sw_dflt_cfg(pf, false, true);
	if (ret) {
		dev_err(dev, "Failed to set local DCB config %d\n", ret);
		return ret;
	}

	 
	dcbcfg->etscfg.willing = 1;
	ret = ice_set_dcb_cfg(pf->hw.port_info);
	if (ret)
		dev_err(dev, "Failed to set DCB to unwilling\n");

	return ret;
}

 
void ice_pf_dcb_recfg(struct ice_pf *pf, bool locked)
{
	struct ice_dcbx_cfg *dcbcfg = &pf->hw.port_info->qos_cfg.local_dcbx_cfg;
	struct iidc_event *event;
	u8 tc_map = 0;
	int v, ret;

	 
	ice_for_each_vsi(pf, v) {
		struct ice_vsi *vsi = pf->vsi[v];

		if (!vsi)
			continue;

		if (vsi->type == ICE_VSI_PF) {
			tc_map = ice_dcb_get_ena_tc(dcbcfg);

			 
			if (!ice_dcb_tc_contig(dcbcfg->etscfg.prio_table)) {
				tc_map = ICE_DFLT_TRAFFIC_CLASS;
				ice_dcb_noncontig_cfg(pf);
			}
		} else if (vsi->type == ICE_VSI_CHNL) {
			tc_map = BIT(ice_get_first_droptc(vsi));
		} else {
			tc_map = ICE_DFLT_TRAFFIC_CLASS;
		}

		ret = ice_vsi_cfg_tc(vsi, tc_map);
		if (ret) {
			dev_err(ice_pf_to_dev(pf), "Failed to config TC for VSI index: %d\n",
				vsi->idx);
			continue;
		}
		 
		if (vsi->type == ICE_VSI_CHNL ||
		    vsi->type == ICE_VSI_SWITCHDEV_CTRL)
			continue;

		ice_vsi_map_rings_to_vectors(vsi);
		if (vsi->type == ICE_VSI_PF)
			ice_dcbnl_set_all(vsi);
	}
	if (!locked) {
		 
		event = kzalloc(sizeof(*event), GFP_KERNEL);
		if (!event)
			return;

		set_bit(IIDC_EVENT_AFTER_TC_CHANGE, event->type);
		ice_send_event_to_aux(pf, event);
		kfree(event);
	}
}

 
int ice_init_pf_dcb(struct ice_pf *pf, bool locked)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_port_info *port_info;
	struct ice_hw *hw = &pf->hw;
	int err;

	port_info = hw->port_info;

	err = ice_init_dcb(hw, false);
	if (err && !port_info->qos_cfg.is_sw_lldp) {
		dev_err(dev, "Error initializing DCB %d\n", err);
		goto dcb_init_err;
	}

	dev_info(dev, "DCB is enabled in the hardware, max number of TCs supported on this port are %d\n",
		 pf->hw.func_caps.common_cap.maxtc);
	if (err) {
		struct ice_vsi *pf_vsi;

		 
		dev_info(dev, "FW LLDP is disabled, DCBx/LLDP in SW mode.\n");
		clear_bit(ICE_FLAG_FW_LLDP_AGENT, pf->flags);
		err = ice_aq_set_pfc_mode(&pf->hw, ICE_AQC_PFC_VLAN_BASED_PFC,
					  NULL);
		if (err)
			dev_info(dev, "Failed to set VLAN PFC mode\n");

		err = ice_dcb_sw_dflt_cfg(pf, true, locked);
		if (err) {
			dev_err(dev, "Failed to set local DCB config %d\n",
				err);
			err = -EIO;
			goto dcb_init_err;
		}

		 
		pf_vsi = ice_get_main_vsi(pf);
		if (!pf_vsi) {
			dev_err(dev, "Failed to set local DCB config\n");
			err = -EIO;
			goto dcb_init_err;
		}

		ice_cfg_sw_lldp(pf_vsi, false, true);

		pf->dcbx_cap = ice_dcb_get_mode(port_info, true);
		return 0;
	}

	set_bit(ICE_FLAG_FW_LLDP_AGENT, pf->flags);

	 
	pf->dcbx_cap = ice_dcb_get_mode(port_info, false);

	err = ice_dcb_init_cfg(pf, locked);
	if (err)
		goto dcb_init_err;

	return 0;

dcb_init_err:
	dev_err(dev, "DCB init failed\n");
	return err;
}

 
void ice_update_dcb_stats(struct ice_pf *pf)
{
	struct ice_hw_port_stats *prev_ps, *cur_ps;
	struct ice_hw *hw = &pf->hw;
	u8 port;
	int i;

	port = hw->port_info->lport;
	prev_ps = &pf->stats_prev;
	cur_ps = &pf->stats;

	if (ice_is_reset_in_progress(pf->state))
		pf->stat_prev_loaded = false;

	for (i = 0; i < 8; i++) {
		ice_stat_update32(hw, GLPRT_PXOFFRXC(port, i),
				  pf->stat_prev_loaded,
				  &prev_ps->priority_xoff_rx[i],
				  &cur_ps->priority_xoff_rx[i]);
		ice_stat_update32(hw, GLPRT_PXONRXC(port, i),
				  pf->stat_prev_loaded,
				  &prev_ps->priority_xon_rx[i],
				  &cur_ps->priority_xon_rx[i]);
		ice_stat_update32(hw, GLPRT_PXONTXC(port, i),
				  pf->stat_prev_loaded,
				  &prev_ps->priority_xon_tx[i],
				  &cur_ps->priority_xon_tx[i]);
		ice_stat_update32(hw, GLPRT_PXOFFTXC(port, i),
				  pf->stat_prev_loaded,
				  &prev_ps->priority_xoff_tx[i],
				  &cur_ps->priority_xoff_tx[i]);
		ice_stat_update32(hw, GLPRT_RXON2OFFCNT(port, i),
				  pf->stat_prev_loaded,
				  &prev_ps->priority_xon_2_xoff[i],
				  &cur_ps->priority_xon_2_xoff[i]);
	}
}

 
void
ice_tx_prepare_vlan_flags_dcb(struct ice_tx_ring *tx_ring,
			      struct ice_tx_buf *first)
{
	struct sk_buff *skb = first->skb;

	if (!test_bit(ICE_FLAG_DCB_ENA, tx_ring->vsi->back->flags))
		return;

	 
	if ((first->tx_flags & ICE_TX_FLAGS_HW_VLAN ||
	     first->tx_flags & ICE_TX_FLAGS_HW_OUTER_SINGLE_VLAN) ||
	    skb->priority != TC_PRIO_CONTROL) {
		first->vid &= ~VLAN_PRIO_MASK;
		 
		first->vid |= (skb->priority << VLAN_PRIO_SHIFT) & VLAN_PRIO_MASK;
		 
		if (tx_ring->flags & ICE_TX_FLAGS_RING_VLAN_L2TAG2)
			first->tx_flags |= ICE_TX_FLAGS_HW_OUTER_SINGLE_VLAN;
		else
			first->tx_flags |= ICE_TX_FLAGS_HW_VLAN;
	}
}

 
static bool ice_dcb_is_mib_change_pending(u8 state)
{
	return ICE_AQ_LLDP_MIB_CHANGE_PENDING ==
		FIELD_GET(ICE_AQ_LLDP_MIB_CHANGE_STATE_M, state);
}

 
void
ice_dcb_process_lldp_set_mib_change(struct ice_pf *pf,
				    struct ice_rq_event_info *event)
{
	struct ice_aqc_port_ets_elem buf = { 0 };
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_aqc_lldp_get_mib *mib;
	struct ice_dcbx_cfg tmp_dcbx_cfg;
	bool pending_handled = true;
	bool need_reconfig = false;
	struct ice_port_info *pi;
	u8 mib_type;
	int ret;

	 
	if (!(test_bit(ICE_FLAG_DCB_CAPABLE, pf->flags)))
		return;

	if (pf->dcbx_cap & DCB_CAP_DCBX_HOST) {
		dev_dbg(dev, "MIB Change Event in HOST mode\n");
		return;
	}

	pi = pf->hw.port_info;
	mib = (struct ice_aqc_lldp_get_mib *)&event->desc.params.raw;

	 
	mib_type = FIELD_GET(ICE_AQ_LLDP_BRID_TYPE_M, mib->type);
	dev_dbg(dev, "LLDP event MIB bridge type 0x%x\n", mib_type);
	if (mib_type != ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID)
		return;

	 
	if (ice_dcb_is_mib_change_pending(mib->state))
		pending_handled = false;

	 
	mib_type = FIELD_GET(ICE_AQ_LLDP_MIB_TYPE_M, mib->type);
	dev_dbg(dev, "LLDP event mib type %s\n", mib_type ? "remote" : "local");
	if (mib_type == ICE_AQ_LLDP_MIB_REMOTE) {
		 
		if (!pending_handled) {
			ice_get_dcb_cfg_from_mib_change(pi, event);
		} else {
			ret =
			  ice_aq_get_dcb_cfg(pi->hw, ICE_AQ_LLDP_MIB_REMOTE,
					     ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID,
					     &pi->qos_cfg.remote_dcbx_cfg);
			if (ret)
				dev_dbg(dev, "Failed to get remote DCB config\n");
		}
		return;
	}

	 
	mutex_lock(&pf->tc_mutex);

	 
	tmp_dcbx_cfg = pi->qos_cfg.local_dcbx_cfg;

	 
	memset(&pi->qos_cfg.local_dcbx_cfg, 0,
	       sizeof(pi->qos_cfg.local_dcbx_cfg));

	 
	if (!pending_handled) {
		ice_get_dcb_cfg_from_mib_change(pi, event);
	} else {
		ret = ice_get_dcb_cfg(pi);
		if (ret) {
			dev_err(dev, "Failed to get DCB config\n");
			goto out;
		}
	}

	 
	if (!memcmp(&tmp_dcbx_cfg, &pi->qos_cfg.local_dcbx_cfg,
		    sizeof(tmp_dcbx_cfg))) {
		dev_dbg(dev, "No change detected in DCBX configuration.\n");
		goto out;
	}

	pf->dcbx_cap = ice_dcb_get_mode(pi, false);

	need_reconfig = ice_dcb_need_recfg(pf, &tmp_dcbx_cfg,
					   &pi->qos_cfg.local_dcbx_cfg);
	ice_dcbnl_flush_apps(pf, &tmp_dcbx_cfg, &pi->qos_cfg.local_dcbx_cfg);
	if (!need_reconfig)
		goto out;

	 
	if (ice_dcb_get_num_tc(&pi->qos_cfg.local_dcbx_cfg) > 1) {
		dev_dbg(dev, "DCB tagging enabled (num TC > 1)\n");
		set_bit(ICE_FLAG_DCB_ENA, pf->flags);
	} else {
		dev_dbg(dev, "DCB tagging disabled (num TC = 1)\n");
		clear_bit(ICE_FLAG_DCB_ENA, pf->flags);
	}

	 
	if (!pending_handled) {
		ice_lldp_execute_pending_mib(&pf->hw);
		pending_handled = true;
	}

	rtnl_lock();
	 
	ice_dcb_ena_dis_vsi(pf, false, true);

	ret = ice_query_port_ets(pi, &buf, sizeof(buf), NULL);
	if (ret) {
		dev_err(dev, "Query Port ETS failed\n");
		goto unlock_rtnl;
	}

	 
	ice_pf_dcb_recfg(pf, false);

	 
	ice_dcb_ena_dis_vsi(pf, true, true);
unlock_rtnl:
	rtnl_unlock();
out:
	mutex_unlock(&pf->tc_mutex);

	 
	if (!pending_handled)
		ice_lldp_execute_pending_mib(&pf->hw);
}

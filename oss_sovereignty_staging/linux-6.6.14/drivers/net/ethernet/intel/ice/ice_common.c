
 

#include "ice_common.h"
#include "ice_sched.h"
#include "ice_adminq_cmd.h"
#include "ice_flow.h"
#include "ice_ptp_hw.h"

#define ICE_PF_RESET_WAIT_COUNT	300

static const char * const ice_link_mode_str_low[] = {
	[0] = "100BASE_TX",
	[1] = "100M_SGMII",
	[2] = "1000BASE_T",
	[3] = "1000BASE_SX",
	[4] = "1000BASE_LX",
	[5] = "1000BASE_KX",
	[6] = "1G_SGMII",
	[7] = "2500BASE_T",
	[8] = "2500BASE_X",
	[9] = "2500BASE_KX",
	[10] = "5GBASE_T",
	[11] = "5GBASE_KR",
	[12] = "10GBASE_T",
	[13] = "10G_SFI_DA",
	[14] = "10GBASE_SR",
	[15] = "10GBASE_LR",
	[16] = "10GBASE_KR_CR1",
	[17] = "10G_SFI_AOC_ACC",
	[18] = "10G_SFI_C2C",
	[19] = "25GBASE_T",
	[20] = "25GBASE_CR",
	[21] = "25GBASE_CR_S",
	[22] = "25GBASE_CR1",
	[23] = "25GBASE_SR",
	[24] = "25GBASE_LR",
	[25] = "25GBASE_KR",
	[26] = "25GBASE_KR_S",
	[27] = "25GBASE_KR1",
	[28] = "25G_AUI_AOC_ACC",
	[29] = "25G_AUI_C2C",
	[30] = "40GBASE_CR4",
	[31] = "40GBASE_SR4",
	[32] = "40GBASE_LR4",
	[33] = "40GBASE_KR4",
	[34] = "40G_XLAUI_AOC_ACC",
	[35] = "40G_XLAUI",
	[36] = "50GBASE_CR2",
	[37] = "50GBASE_SR2",
	[38] = "50GBASE_LR2",
	[39] = "50GBASE_KR2",
	[40] = "50G_LAUI2_AOC_ACC",
	[41] = "50G_LAUI2",
	[42] = "50G_AUI2_AOC_ACC",
	[43] = "50G_AUI2",
	[44] = "50GBASE_CP",
	[45] = "50GBASE_SR",
	[46] = "50GBASE_FR",
	[47] = "50GBASE_LR",
	[48] = "50GBASE_KR_PAM4",
	[49] = "50G_AUI1_AOC_ACC",
	[50] = "50G_AUI1",
	[51] = "100GBASE_CR4",
	[52] = "100GBASE_SR4",
	[53] = "100GBASE_LR4",
	[54] = "100GBASE_KR4",
	[55] = "100G_CAUI4_AOC_ACC",
	[56] = "100G_CAUI4",
	[57] = "100G_AUI4_AOC_ACC",
	[58] = "100G_AUI4",
	[59] = "100GBASE_CR_PAM4",
	[60] = "100GBASE_KR_PAM4",
	[61] = "100GBASE_CP2",
	[62] = "100GBASE_SR2",
	[63] = "100GBASE_DR",
};

static const char * const ice_link_mode_str_high[] = {
	[0] = "100GBASE_KR2_PAM4",
	[1] = "100G_CAUI2_AOC_ACC",
	[2] = "100G_CAUI2",
	[3] = "100G_AUI2_AOC_ACC",
	[4] = "100G_AUI2",
};

 
static void
ice_dump_phy_type(struct ice_hw *hw, u64 low, u64 high, const char *prefix)
{
	ice_debug(hw, ICE_DBG_PHY, "%s: phy_type_low: 0x%016llx\n", prefix, low);

	for (u32 i = 0; i < BITS_PER_TYPE(typeof(low)); i++) {
		if (low & BIT_ULL(i))
			ice_debug(hw, ICE_DBG_PHY, "%s:   bit(%d): %s\n",
				  prefix, i, ice_link_mode_str_low[i]);
	}

	ice_debug(hw, ICE_DBG_PHY, "%s: phy_type_high: 0x%016llx\n", prefix, high);

	for (u32 i = 0; i < BITS_PER_TYPE(typeof(high)); i++) {
		if (high & BIT_ULL(i))
			ice_debug(hw, ICE_DBG_PHY, "%s:   bit(%d): %s\n",
				  prefix, i, ice_link_mode_str_high[i]);
	}
}

 
static int ice_set_mac_type(struct ice_hw *hw)
{
	if (hw->vendor_id != PCI_VENDOR_ID_INTEL)
		return -ENODEV;

	switch (hw->device_id) {
	case ICE_DEV_ID_E810C_BACKPLANE:
	case ICE_DEV_ID_E810C_QSFP:
	case ICE_DEV_ID_E810C_SFP:
	case ICE_DEV_ID_E810_XXV_BACKPLANE:
	case ICE_DEV_ID_E810_XXV_QSFP:
	case ICE_DEV_ID_E810_XXV_SFP:
		hw->mac_type = ICE_MAC_E810;
		break;
	case ICE_DEV_ID_E823C_10G_BASE_T:
	case ICE_DEV_ID_E823C_BACKPLANE:
	case ICE_DEV_ID_E823C_QSFP:
	case ICE_DEV_ID_E823C_SFP:
	case ICE_DEV_ID_E823C_SGMII:
	case ICE_DEV_ID_E822C_10G_BASE_T:
	case ICE_DEV_ID_E822C_BACKPLANE:
	case ICE_DEV_ID_E822C_QSFP:
	case ICE_DEV_ID_E822C_SFP:
	case ICE_DEV_ID_E822C_SGMII:
	case ICE_DEV_ID_E822L_10G_BASE_T:
	case ICE_DEV_ID_E822L_BACKPLANE:
	case ICE_DEV_ID_E822L_SFP:
	case ICE_DEV_ID_E822L_SGMII:
	case ICE_DEV_ID_E823L_10G_BASE_T:
	case ICE_DEV_ID_E823L_1GBE:
	case ICE_DEV_ID_E823L_BACKPLANE:
	case ICE_DEV_ID_E823L_QSFP:
	case ICE_DEV_ID_E823L_SFP:
		hw->mac_type = ICE_MAC_GENERIC;
		break;
	default:
		hw->mac_type = ICE_MAC_UNKNOWN;
		break;
	}

	ice_debug(hw, ICE_DBG_INIT, "mac_type: %d\n", hw->mac_type);
	return 0;
}

 
bool ice_is_e810(struct ice_hw *hw)
{
	return hw->mac_type == ICE_MAC_E810;
}

 
bool ice_is_e810t(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E810C_SFP:
		switch (hw->subsystem_device_id) {
		case ICE_SUBDEV_ID_E810T:
		case ICE_SUBDEV_ID_E810T2:
		case ICE_SUBDEV_ID_E810T3:
		case ICE_SUBDEV_ID_E810T4:
		case ICE_SUBDEV_ID_E810T6:
		case ICE_SUBDEV_ID_E810T7:
			return true;
		}
		break;
	case ICE_DEV_ID_E810C_QSFP:
		switch (hw->subsystem_device_id) {
		case ICE_SUBDEV_ID_E810T2:
		case ICE_SUBDEV_ID_E810T3:
		case ICE_SUBDEV_ID_E810T5:
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

 
bool ice_is_e823(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E823L_BACKPLANE:
	case ICE_DEV_ID_E823L_SFP:
	case ICE_DEV_ID_E823L_10G_BASE_T:
	case ICE_DEV_ID_E823L_1GBE:
	case ICE_DEV_ID_E823L_QSFP:
	case ICE_DEV_ID_E823C_BACKPLANE:
	case ICE_DEV_ID_E823C_QSFP:
	case ICE_DEV_ID_E823C_SFP:
	case ICE_DEV_ID_E823C_10G_BASE_T:
	case ICE_DEV_ID_E823C_SGMII:
		return true;
	default:
		return false;
	}
}

 
int ice_clear_pf_cfg(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_pf_cfg);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 
static int
ice_aq_manage_mac_read(struct ice_hw *hw, void *buf, u16 buf_size,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_manage_mac_read_resp *resp;
	struct ice_aqc_manage_mac_read *cmd;
	struct ice_aq_desc desc;
	int status;
	u16 flags;
	u8 i;

	cmd = &desc.params.mac_read;

	if (buf_size < sizeof(*resp))
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_manage_mac_read);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (status)
		return status;

	resp = buf;
	flags = le16_to_cpu(cmd->flags) & ICE_AQC_MAN_MAC_READ_M;

	if (!(flags & ICE_AQC_MAN_MAC_LAN_ADDR_VALID)) {
		ice_debug(hw, ICE_DBG_LAN, "got invalid MAC address\n");
		return -EIO;
	}

	 
	for (i = 0; i < cmd->num_addr; i++)
		if (resp[i].addr_type == ICE_AQC_MAN_MAC_ADDR_TYPE_LAN) {
			ether_addr_copy(hw->port_info->mac.lan_addr,
					resp[i].mac_addr);
			ether_addr_copy(hw->port_info->mac.perm_addr,
					resp[i].mac_addr);
			break;
		}

	return 0;
}

 
int
ice_aq_get_phy_caps(struct ice_port_info *pi, bool qual_mods, u8 report_mode,
		    struct ice_aqc_get_phy_caps_data *pcaps,
		    struct ice_sq_cd *cd)
{
	struct ice_aqc_get_phy_caps *cmd;
	u16 pcaps_size = sizeof(*pcaps);
	struct ice_aq_desc desc;
	const char *prefix;
	struct ice_hw *hw;
	int status;

	cmd = &desc.params.get_phy;

	if (!pcaps || (report_mode & ~ICE_AQC_REPORT_MODE_M) || !pi)
		return -EINVAL;
	hw = pi->hw;

	if (report_mode == ICE_AQC_REPORT_DFLT_CFG &&
	    !ice_fw_supports_report_dflt_cfg(hw))
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_phy_caps);

	if (qual_mods)
		cmd->param0 |= cpu_to_le16(ICE_AQC_GET_PHY_RQM);

	cmd->param0 |= cpu_to_le16(report_mode);
	status = ice_aq_send_cmd(hw, &desc, pcaps, pcaps_size, cd);

	ice_debug(hw, ICE_DBG_LINK, "get phy caps dump\n");

	switch (report_mode) {
	case ICE_AQC_REPORT_TOPO_CAP_MEDIA:
		prefix = "phy_caps_media";
		break;
	case ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA:
		prefix = "phy_caps_no_media";
		break;
	case ICE_AQC_REPORT_ACTIVE_CFG:
		prefix = "phy_caps_active";
		break;
	case ICE_AQC_REPORT_DFLT_CFG:
		prefix = "phy_caps_default";
		break;
	default:
		prefix = "phy_caps_invalid";
	}

	ice_dump_phy_type(hw, le64_to_cpu(pcaps->phy_type_low),
			  le64_to_cpu(pcaps->phy_type_high), prefix);

	ice_debug(hw, ICE_DBG_LINK, "%s: report_mode = 0x%x\n",
		  prefix, report_mode);
	ice_debug(hw, ICE_DBG_LINK, "%s: caps = 0x%x\n", prefix, pcaps->caps);
	ice_debug(hw, ICE_DBG_LINK, "%s: low_power_ctrl_an = 0x%x\n", prefix,
		  pcaps->low_power_ctrl_an);
	ice_debug(hw, ICE_DBG_LINK, "%s: eee_cap = 0x%x\n", prefix,
		  pcaps->eee_cap);
	ice_debug(hw, ICE_DBG_LINK, "%s: eeer_value = 0x%x\n", prefix,
		  pcaps->eeer_value);
	ice_debug(hw, ICE_DBG_LINK, "%s: link_fec_options = 0x%x\n", prefix,
		  pcaps->link_fec_options);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_compliance_enforcement = 0x%x\n",
		  prefix, pcaps->module_compliance_enforcement);
	ice_debug(hw, ICE_DBG_LINK, "%s: extended_compliance_code = 0x%x\n",
		  prefix, pcaps->extended_compliance_code);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_type[0] = 0x%x\n", prefix,
		  pcaps->module_type[0]);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_type[1] = 0x%x\n", prefix,
		  pcaps->module_type[1]);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_type[2] = 0x%x\n", prefix,
		  pcaps->module_type[2]);

	if (!status && report_mode == ICE_AQC_REPORT_TOPO_CAP_MEDIA) {
		pi->phy.phy_type_low = le64_to_cpu(pcaps->phy_type_low);
		pi->phy.phy_type_high = le64_to_cpu(pcaps->phy_type_high);
		memcpy(pi->phy.link_info.module_type, &pcaps->module_type,
		       sizeof(pi->phy.link_info.module_type));
	}

	return status;
}

 
static int
ice_aq_get_link_topo_handle(struct ice_port_info *pi, u8 node_type,
			    struct ice_sq_cd *cd)
{
	struct ice_aqc_get_link_topo *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.get_link_topo;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_topo);

	cmd->addr.topo_params.node_type_ctx =
		(ICE_AQC_LINK_TOPO_NODE_CTX_PORT <<
		 ICE_AQC_LINK_TOPO_NODE_CTX_S);

	 
	cmd->addr.topo_params.node_type_ctx |=
		(ICE_AQC_LINK_TOPO_NODE_TYPE_M & node_type);

	return ice_aq_send_cmd(pi->hw, &desc, NULL, 0, cd);
}

 
static bool ice_is_media_cage_present(struct ice_port_info *pi)
{
	 
	return !ice_aq_get_link_topo_handle(pi,
					    ICE_AQC_LINK_TOPO_NODE_TYPE_CAGE,
					    NULL);
}

 
static enum ice_media_type ice_get_media_type(struct ice_port_info *pi)
{
	struct ice_link_status *hw_link_info;

	if (!pi)
		return ICE_MEDIA_UNKNOWN;

	hw_link_info = &pi->phy.link_info;
	if (hw_link_info->phy_type_low && hw_link_info->phy_type_high)
		 
		return ICE_MEDIA_UNKNOWN;

	if (hw_link_info->phy_type_low) {
		 
		if (hw_link_info->phy_type_low == ICE_PHY_TYPE_LOW_1G_SGMII &&
		    (hw_link_info->module_type[ICE_AQC_MOD_TYPE_IDENT] ==
		    ICE_AQC_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE ||
		    hw_link_info->module_type[ICE_AQC_MOD_TYPE_IDENT] ==
		    ICE_AQC_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE))
			return ICE_MEDIA_DA;

		switch (hw_link_info->phy_type_low) {
		case ICE_PHY_TYPE_LOW_1000BASE_SX:
		case ICE_PHY_TYPE_LOW_1000BASE_LX:
		case ICE_PHY_TYPE_LOW_10GBASE_SR:
		case ICE_PHY_TYPE_LOW_10GBASE_LR:
		case ICE_PHY_TYPE_LOW_10G_SFI_C2C:
		case ICE_PHY_TYPE_LOW_25GBASE_SR:
		case ICE_PHY_TYPE_LOW_25GBASE_LR:
		case ICE_PHY_TYPE_LOW_40GBASE_SR4:
		case ICE_PHY_TYPE_LOW_40GBASE_LR4:
		case ICE_PHY_TYPE_LOW_50GBASE_SR2:
		case ICE_PHY_TYPE_LOW_50GBASE_LR2:
		case ICE_PHY_TYPE_LOW_50GBASE_SR:
		case ICE_PHY_TYPE_LOW_50GBASE_FR:
		case ICE_PHY_TYPE_LOW_50GBASE_LR:
		case ICE_PHY_TYPE_LOW_100GBASE_SR4:
		case ICE_PHY_TYPE_LOW_100GBASE_LR4:
		case ICE_PHY_TYPE_LOW_100GBASE_SR2:
		case ICE_PHY_TYPE_LOW_100GBASE_DR:
		case ICE_PHY_TYPE_LOW_10G_SFI_AOC_ACC:
		case ICE_PHY_TYPE_LOW_25G_AUI_AOC_ACC:
		case ICE_PHY_TYPE_LOW_40G_XLAUI_AOC_ACC:
		case ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC:
		case ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC:
		case ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC:
		case ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC:
		case ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC:
			return ICE_MEDIA_FIBER;
		case ICE_PHY_TYPE_LOW_100BASE_TX:
		case ICE_PHY_TYPE_LOW_1000BASE_T:
		case ICE_PHY_TYPE_LOW_2500BASE_T:
		case ICE_PHY_TYPE_LOW_5GBASE_T:
		case ICE_PHY_TYPE_LOW_10GBASE_T:
		case ICE_PHY_TYPE_LOW_25GBASE_T:
			return ICE_MEDIA_BASET;
		case ICE_PHY_TYPE_LOW_10G_SFI_DA:
		case ICE_PHY_TYPE_LOW_25GBASE_CR:
		case ICE_PHY_TYPE_LOW_25GBASE_CR_S:
		case ICE_PHY_TYPE_LOW_25GBASE_CR1:
		case ICE_PHY_TYPE_LOW_40GBASE_CR4:
		case ICE_PHY_TYPE_LOW_50GBASE_CR2:
		case ICE_PHY_TYPE_LOW_50GBASE_CP:
		case ICE_PHY_TYPE_LOW_100GBASE_CR4:
		case ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4:
		case ICE_PHY_TYPE_LOW_100GBASE_CP2:
			return ICE_MEDIA_DA;
		case ICE_PHY_TYPE_LOW_25G_AUI_C2C:
		case ICE_PHY_TYPE_LOW_40G_XLAUI:
		case ICE_PHY_TYPE_LOW_50G_LAUI2:
		case ICE_PHY_TYPE_LOW_50G_AUI2:
		case ICE_PHY_TYPE_LOW_50G_AUI1:
		case ICE_PHY_TYPE_LOW_100G_AUI4:
		case ICE_PHY_TYPE_LOW_100G_CAUI4:
			if (ice_is_media_cage_present(pi))
				return ICE_MEDIA_DA;
			fallthrough;
		case ICE_PHY_TYPE_LOW_1000BASE_KX:
		case ICE_PHY_TYPE_LOW_2500BASE_KX:
		case ICE_PHY_TYPE_LOW_2500BASE_X:
		case ICE_PHY_TYPE_LOW_5GBASE_KR:
		case ICE_PHY_TYPE_LOW_10GBASE_KR_CR1:
		case ICE_PHY_TYPE_LOW_25GBASE_KR:
		case ICE_PHY_TYPE_LOW_25GBASE_KR1:
		case ICE_PHY_TYPE_LOW_25GBASE_KR_S:
		case ICE_PHY_TYPE_LOW_40GBASE_KR4:
		case ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4:
		case ICE_PHY_TYPE_LOW_50GBASE_KR2:
		case ICE_PHY_TYPE_LOW_100GBASE_KR4:
		case ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4:
			return ICE_MEDIA_BACKPLANE;
		}
	} else {
		switch (hw_link_info->phy_type_high) {
		case ICE_PHY_TYPE_HIGH_100G_AUI2:
		case ICE_PHY_TYPE_HIGH_100G_CAUI2:
			if (ice_is_media_cage_present(pi))
				return ICE_MEDIA_DA;
			fallthrough;
		case ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4:
			return ICE_MEDIA_BACKPLANE;
		case ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC:
		case ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC:
			return ICE_MEDIA_FIBER;
		}
	}
	return ICE_MEDIA_UNKNOWN;
}

 
int
ice_aq_get_link_info(struct ice_port_info *pi, bool ena_lse,
		     struct ice_link_status *link, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_link_status_data link_data = { 0 };
	struct ice_aqc_get_link_status *resp;
	struct ice_link_status *li_old, *li;
	enum ice_media_type *hw_media_type;
	struct ice_fc_info *hw_fc_info;
	bool tx_pause, rx_pause;
	struct ice_aq_desc desc;
	struct ice_hw *hw;
	u16 cmd_flags;
	int status;

	if (!pi)
		return -EINVAL;
	hw = pi->hw;
	li_old = &pi->phy.link_info_old;
	hw_media_type = &pi->phy.media_type;
	li = &pi->phy.link_info;
	hw_fc_info = &pi->fc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_status);
	cmd_flags = (ena_lse) ? ICE_AQ_LSE_ENA : ICE_AQ_LSE_DIS;
	resp = &desc.params.get_link_status;
	resp->cmd_flags = cpu_to_le16(cmd_flags);
	resp->lport_num = pi->lport;

	status = ice_aq_send_cmd(hw, &desc, &link_data, sizeof(link_data), cd);

	if (status)
		return status;

	 
	*li_old = *li;

	 
	li->link_speed = le16_to_cpu(link_data.link_speed);
	li->phy_type_low = le64_to_cpu(link_data.phy_type_low);
	li->phy_type_high = le64_to_cpu(link_data.phy_type_high);
	*hw_media_type = ice_get_media_type(pi);
	li->link_info = link_data.link_info;
	li->link_cfg_err = link_data.link_cfg_err;
	li->an_info = link_data.an_info;
	li->ext_info = link_data.ext_info;
	li->max_frame_size = le16_to_cpu(link_data.max_frame_size);
	li->fec_info = link_data.cfg & ICE_AQ_FEC_MASK;
	li->topo_media_conflict = link_data.topo_media_conflict;
	li->pacing = link_data.cfg & (ICE_AQ_CFG_PACING_M |
				      ICE_AQ_CFG_PACING_TYPE_M);

	 
	tx_pause = !!(link_data.an_info & ICE_AQ_LINK_PAUSE_TX);
	rx_pause = !!(link_data.an_info & ICE_AQ_LINK_PAUSE_RX);
	if (tx_pause && rx_pause)
		hw_fc_info->current_mode = ICE_FC_FULL;
	else if (tx_pause)
		hw_fc_info->current_mode = ICE_FC_TX_PAUSE;
	else if (rx_pause)
		hw_fc_info->current_mode = ICE_FC_RX_PAUSE;
	else
		hw_fc_info->current_mode = ICE_FC_NONE;

	li->lse_ena = !!(resp->cmd_flags & cpu_to_le16(ICE_AQ_LSE_IS_ENABLED));

	ice_debug(hw, ICE_DBG_LINK, "get link info\n");
	ice_debug(hw, ICE_DBG_LINK, "	link_speed = 0x%x\n", li->link_speed);
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_low = 0x%llx\n",
		  (unsigned long long)li->phy_type_low);
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_high = 0x%llx\n",
		  (unsigned long long)li->phy_type_high);
	ice_debug(hw, ICE_DBG_LINK, "	media_type = 0x%x\n", *hw_media_type);
	ice_debug(hw, ICE_DBG_LINK, "	link_info = 0x%x\n", li->link_info);
	ice_debug(hw, ICE_DBG_LINK, "	link_cfg_err = 0x%x\n", li->link_cfg_err);
	ice_debug(hw, ICE_DBG_LINK, "	an_info = 0x%x\n", li->an_info);
	ice_debug(hw, ICE_DBG_LINK, "	ext_info = 0x%x\n", li->ext_info);
	ice_debug(hw, ICE_DBG_LINK, "	fec_info = 0x%x\n", li->fec_info);
	ice_debug(hw, ICE_DBG_LINK, "	lse_ena = 0x%x\n", li->lse_ena);
	ice_debug(hw, ICE_DBG_LINK, "	max_frame = 0x%x\n",
		  li->max_frame_size);
	ice_debug(hw, ICE_DBG_LINK, "	pacing = 0x%x\n", li->pacing);

	 
	if (link)
		*link = *li;

	 
	pi->phy.get_link_info = false;

	return 0;
}

 
static void
ice_fill_tx_timer_and_fc_thresh(struct ice_hw *hw,
				struct ice_aqc_set_mac_cfg *cmd)
{
	u16 fc_thres_val, tx_timer_val;
	u32 val;

	 
#define IDX_OF_LFC PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA_MAX_INDEX

	 
	val = rd32(hw, PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA(IDX_OF_LFC));
	tx_timer_val = val &
		PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA_HSEC_CTL_TX_PAUSE_QUANTA_M;
	cmd->tx_tmr_value = cpu_to_le16(tx_timer_val);

	 
	val = rd32(hw, PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(IDX_OF_LFC));
	fc_thres_val = val & PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_M;

	cmd->fc_refresh_threshold = cpu_to_le16(fc_thres_val);
}

 
int
ice_aq_set_mac_cfg(struct ice_hw *hw, u16 max_frame_size, struct ice_sq_cd *cd)
{
	struct ice_aqc_set_mac_cfg *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_mac_cfg;

	if (max_frame_size == 0)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_mac_cfg);

	cmd->max_frame_size = cpu_to_le16(max_frame_size);

	ice_fill_tx_timer_and_fc_thresh(hw, cmd);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
static int ice_init_fltr_mgmt_struct(struct ice_hw *hw)
{
	struct ice_switch_info *sw;
	int status;

	hw->switch_info = devm_kzalloc(ice_hw_to_dev(hw),
				       sizeof(*hw->switch_info), GFP_KERNEL);
	sw = hw->switch_info;

	if (!sw)
		return -ENOMEM;

	INIT_LIST_HEAD(&sw->vsi_list_map_head);
	sw->prof_res_bm_init = 0;

	status = ice_init_def_sw_recp(hw);
	if (status) {
		devm_kfree(ice_hw_to_dev(hw), hw->switch_info);
		return status;
	}
	return 0;
}

 
static void ice_cleanup_fltr_mgmt_struct(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_vsi_list_map_info *v_pos_map;
	struct ice_vsi_list_map_info *v_tmp_map;
	struct ice_sw_recipe *recps;
	u8 i;

	list_for_each_entry_safe(v_pos_map, v_tmp_map, &sw->vsi_list_map_head,
				 list_entry) {
		list_del(&v_pos_map->list_entry);
		devm_kfree(ice_hw_to_dev(hw), v_pos_map);
	}
	recps = sw->recp_list;
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct ice_recp_grp_entry *rg_entry, *tmprg_entry;

		recps[i].root_rid = i;
		list_for_each_entry_safe(rg_entry, tmprg_entry,
					 &recps[i].rg_list, l_entry) {
			list_del(&rg_entry->l_entry);
			devm_kfree(ice_hw_to_dev(hw), rg_entry);
		}

		if (recps[i].adv_rule) {
			struct ice_adv_fltr_mgmt_list_entry *tmp_entry;
			struct ice_adv_fltr_mgmt_list_entry *lst_itr;

			mutex_destroy(&recps[i].filt_rule_lock);
			list_for_each_entry_safe(lst_itr, tmp_entry,
						 &recps[i].filt_rules,
						 list_entry) {
				list_del(&lst_itr->list_entry);
				devm_kfree(ice_hw_to_dev(hw), lst_itr->lkups);
				devm_kfree(ice_hw_to_dev(hw), lst_itr);
			}
		} else {
			struct ice_fltr_mgmt_list_entry *lst_itr, *tmp_entry;

			mutex_destroy(&recps[i].filt_rule_lock);
			list_for_each_entry_safe(lst_itr, tmp_entry,
						 &recps[i].filt_rules,
						 list_entry) {
				list_del(&lst_itr->list_entry);
				devm_kfree(ice_hw_to_dev(hw), lst_itr);
			}
		}
		devm_kfree(ice_hw_to_dev(hw), recps[i].root_buf);
	}
	ice_rm_all_sw_replay_rule_info(hw);
	devm_kfree(ice_hw_to_dev(hw), sw->recp_list);
	devm_kfree(ice_hw_to_dev(hw), sw);
}

 
static int ice_get_fw_log_cfg(struct ice_hw *hw)
{
	struct ice_aq_desc desc;
	__le16 *config;
	int status;
	u16 size;

	size = sizeof(*config) * ICE_AQC_FW_LOG_ID_MAX;
	config = kzalloc(size, GFP_KERNEL);
	if (!config)
		return -ENOMEM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_fw_logging_info);

	status = ice_aq_send_cmd(hw, &desc, config, size, NULL);
	if (!status) {
		u16 i;

		 
		for (i = 0; i < ICE_AQC_FW_LOG_ID_MAX; i++) {
			u16 v, m, flgs;

			v = le16_to_cpu(config[i]);
			m = (v & ICE_AQC_FW_LOG_ID_M) >> ICE_AQC_FW_LOG_ID_S;
			flgs = (v & ICE_AQC_FW_LOG_EN_M) >> ICE_AQC_FW_LOG_EN_S;

			if (m < ICE_AQC_FW_LOG_ID_MAX)
				hw->fw_log.evnts[m].cur = flgs;
		}
	}

	kfree(config);

	return status;
}

 
static int ice_cfg_fw_log(struct ice_hw *hw, bool enable)
{
	struct ice_aqc_fw_logging *cmd;
	u16 i, chgs = 0, len = 0;
	struct ice_aq_desc desc;
	__le16 *data = NULL;
	u8 actv_evnts = 0;
	void *buf = NULL;
	int status = 0;

	if (!hw->fw_log.cq_en && !hw->fw_log.uart_en)
		return 0;

	 
	if (!enable &&
	    (!hw->fw_log.actv_evnts || !ice_check_sq_alive(hw, &hw->adminq)))
		return 0;

	 
	status = ice_get_fw_log_cfg(hw);
	if (status)
		return status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_fw_logging);
	cmd = &desc.params.fw_logging;

	 
	if (hw->fw_log.cq_en)
		cmd->log_ctrl_valid |= ICE_AQC_FW_LOG_AQ_VALID;

	if (hw->fw_log.uart_en)
		cmd->log_ctrl_valid |= ICE_AQC_FW_LOG_UART_VALID;

	if (enable) {
		 
		for (i = 0; i < ICE_AQC_FW_LOG_ID_MAX; i++) {
			u16 val;

			 
			actv_evnts |= hw->fw_log.evnts[i].cfg;

			if (hw->fw_log.evnts[i].cfg == hw->fw_log.evnts[i].cur)
				continue;

			if (!data) {
				data = devm_kcalloc(ice_hw_to_dev(hw),
						    ICE_AQC_FW_LOG_ID_MAX,
						    sizeof(*data),
						    GFP_KERNEL);
				if (!data)
					return -ENOMEM;
			}

			val = i << ICE_AQC_FW_LOG_ID_S;
			val |= hw->fw_log.evnts[i].cfg << ICE_AQC_FW_LOG_EN_S;
			data[chgs++] = cpu_to_le16(val);
		}

		 
		if (actv_evnts) {
			 
			if (!chgs)
				goto out;

			if (hw->fw_log.cq_en)
				cmd->log_ctrl |= ICE_AQC_FW_LOG_AQ_EN;

			if (hw->fw_log.uart_en)
				cmd->log_ctrl |= ICE_AQC_FW_LOG_UART_EN;

			buf = data;
			len = sizeof(*data) * chgs;
			desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);
		}
	}

	status = ice_aq_send_cmd(hw, &desc, buf, len, NULL);
	if (!status) {
		 
		u16 cnt = enable ? chgs : (u16)ICE_AQC_FW_LOG_ID_MAX;

		hw->fw_log.actv_evnts = actv_evnts;
		for (i = 0; i < cnt; i++) {
			u16 v, m;

			if (!enable) {
				 
				hw->fw_log.evnts[i].cur = 0;
				continue;
			}

			v = le16_to_cpu(data[i]);
			m = (v & ICE_AQC_FW_LOG_ID_M) >> ICE_AQC_FW_LOG_ID_S;
			hw->fw_log.evnts[m].cur = hw->fw_log.evnts[m].cfg;
		}
	}

out:
	devm_kfree(ice_hw_to_dev(hw), data);

	return status;
}

 
void ice_output_fw_log(struct ice_hw *hw, struct ice_aq_desc *desc, void *buf)
{
	ice_debug(hw, ICE_DBG_FW_LOG, "[ FW Log Msg Start ]\n");
	ice_debug_array(hw, ICE_DBG_FW_LOG, 16, 1, (u8 *)buf,
			le16_to_cpu(desc->datalen));
	ice_debug(hw, ICE_DBG_FW_LOG, "[ FW Log Msg End ]\n");
}

 
static void ice_get_itr_intrl_gran(struct ice_hw *hw)
{
	u8 max_agg_bw = (rd32(hw, GL_PWR_MODE_CTL) &
			 GL_PWR_MODE_CTL_CAR_MAX_BW_M) >>
			GL_PWR_MODE_CTL_CAR_MAX_BW_S;

	switch (max_agg_bw) {
	case ICE_MAX_AGG_BW_200G:
	case ICE_MAX_AGG_BW_100G:
	case ICE_MAX_AGG_BW_50G:
		hw->itr_gran = ICE_ITR_GRAN_ABOVE_25;
		hw->intrl_gran = ICE_INTRL_GRAN_ABOVE_25;
		break;
	case ICE_MAX_AGG_BW_25G:
		hw->itr_gran = ICE_ITR_GRAN_MAX_25;
		hw->intrl_gran = ICE_INTRL_GRAN_MAX_25;
		break;
	}
}

 
int ice_init_hw(struct ice_hw *hw)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	u16 mac_buf_len;
	void *mac_buf;
	int status;

	 
	status = ice_set_mac_type(hw);
	if (status)
		return status;

	hw->pf_id = (u8)(rd32(hw, PF_FUNC_RID) &
			 PF_FUNC_RID_FUNC_NUM_M) >>
		PF_FUNC_RID_FUNC_NUM_S;

	status = ice_reset(hw, ICE_RESET_PFR);
	if (status)
		return status;

	ice_get_itr_intrl_gran(hw);

	status = ice_create_all_ctrlq(hw);
	if (status)
		goto err_unroll_cqinit;

	 
	status = ice_cfg_fw_log(hw, true);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to enable FW logging.\n");

	status = ice_clear_pf_cfg(hw);
	if (status)
		goto err_unroll_cqinit;

	 
	wr32(hw, PFQF_FD_ENA, PFQF_FD_ENA_FD_ENA_M);
	INIT_LIST_HEAD(&hw->fdir_list_head);

	ice_clear_pxe_mode(hw);

	status = ice_init_nvm(hw);
	if (status)
		goto err_unroll_cqinit;

	status = ice_get_caps(hw);
	if (status)
		goto err_unroll_cqinit;

	if (!hw->port_info)
		hw->port_info = devm_kzalloc(ice_hw_to_dev(hw),
					     sizeof(*hw->port_info),
					     GFP_KERNEL);
	if (!hw->port_info) {
		status = -ENOMEM;
		goto err_unroll_cqinit;
	}

	 
	hw->port_info->hw = hw;

	 
	status = ice_get_initial_sw_cfg(hw);
	if (status)
		goto err_unroll_alloc;

	hw->evb_veb = true;

	 
	xa_init_flags(&hw->port_info->sched_node_ids, XA_FLAGS_ALLOC);

	 
	status = ice_sched_query_res_alloc(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_SCHED, "Failed to get scheduler allocated resources\n");
		goto err_unroll_alloc;
	}
	ice_sched_get_psm_clk_freq(hw);

	 
	status = ice_sched_init_port(hw->port_info);
	if (status)
		goto err_unroll_sched;

	pcaps = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps) {
		status = -ENOMEM;
		goto err_unroll_sched;
	}

	 
	status = ice_aq_get_phy_caps(hw->port_info, false,
				     ICE_AQC_REPORT_TOPO_CAP_MEDIA, pcaps,
				     NULL);
	devm_kfree(ice_hw_to_dev(hw), pcaps);
	if (status)
		dev_warn(ice_hw_to_dev(hw), "Get PHY capabilities failed status = %d, continuing anyway\n",
			 status);

	 
	status = ice_aq_get_link_info(hw->port_info, false, NULL, NULL);
	if (status)
		goto err_unroll_sched;

	 
	if (!hw->sw_entry_point_layer) {
		ice_debug(hw, ICE_DBG_SCHED, "invalid sw entry point\n");
		status = -EIO;
		goto err_unroll_sched;
	}
	INIT_LIST_HEAD(&hw->agg_list);
	 
	if (!hw->max_burst_size)
		ice_cfg_rl_burst_size(hw, ICE_SCHED_DFLT_BURST_SIZE);

	status = ice_init_fltr_mgmt_struct(hw);
	if (status)
		goto err_unroll_sched;

	 
	 
	mac_buf = devm_kcalloc(ice_hw_to_dev(hw), 2,
			       sizeof(struct ice_aqc_manage_mac_read_resp),
			       GFP_KERNEL);
	mac_buf_len = 2 * sizeof(struct ice_aqc_manage_mac_read_resp);

	if (!mac_buf) {
		status = -ENOMEM;
		goto err_unroll_fltr_mgmt_struct;
	}

	status = ice_aq_manage_mac_read(hw, mac_buf, mac_buf_len, NULL);
	devm_kfree(ice_hw_to_dev(hw), mac_buf);

	if (status)
		goto err_unroll_fltr_mgmt_struct;
	 
	status = ice_aq_set_mac_cfg(hw, ICE_AQ_SET_MAC_FRAME_SIZE_MAX, NULL);
	if (status)
		goto err_unroll_fltr_mgmt_struct;
	 
	status = ice_alloc_fd_res_cntr(hw, &hw->fd_ctr_base);
	if (status)
		goto err_unroll_fltr_mgmt_struct;
	status = ice_init_hw_tbls(hw);
	if (status)
		goto err_unroll_fltr_mgmt_struct;
	mutex_init(&hw->tnl_lock);
	return 0;

err_unroll_fltr_mgmt_struct:
	ice_cleanup_fltr_mgmt_struct(hw);
err_unroll_sched:
	ice_sched_cleanup_all(hw);
err_unroll_alloc:
	devm_kfree(ice_hw_to_dev(hw), hw->port_info);
err_unroll_cqinit:
	ice_destroy_all_ctrlq(hw);
	return status;
}

 
void ice_deinit_hw(struct ice_hw *hw)
{
	ice_free_fd_res_cntr(hw, hw->fd_ctr_base);
	ice_cleanup_fltr_mgmt_struct(hw);

	ice_sched_cleanup_all(hw);
	ice_sched_clear_agg(hw);
	ice_free_seg(hw);
	ice_free_hw_tbls(hw);
	mutex_destroy(&hw->tnl_lock);

	 
	ice_cfg_fw_log(hw, false);
	ice_destroy_all_ctrlq(hw);

	 
	ice_clear_all_vsi_ctx(hw);
}

 
int ice_check_reset(struct ice_hw *hw)
{
	u32 cnt, reg = 0, grst_timeout, uld_mask;

	 
	grst_timeout = ((rd32(hw, GLGEN_RSTCTL) & GLGEN_RSTCTL_GRSTDEL_M) >>
			GLGEN_RSTCTL_GRSTDEL_S) + 10;

	for (cnt = 0; cnt < grst_timeout; cnt++) {
		mdelay(100);
		reg = rd32(hw, GLGEN_RSTAT);
		if (!(reg & GLGEN_RSTAT_DEVSTATE_M))
			break;
	}

	if (cnt == grst_timeout) {
		ice_debug(hw, ICE_DBG_INIT, "Global reset polling failed to complete.\n");
		return -EIO;
	}

#define ICE_RESET_DONE_MASK	(GLNVM_ULD_PCIER_DONE_M |\
				 GLNVM_ULD_PCIER_DONE_1_M |\
				 GLNVM_ULD_CORER_DONE_M |\
				 GLNVM_ULD_GLOBR_DONE_M |\
				 GLNVM_ULD_POR_DONE_M |\
				 GLNVM_ULD_POR_DONE_1_M |\
				 GLNVM_ULD_PCIER_DONE_2_M)

	uld_mask = ICE_RESET_DONE_MASK | (hw->func_caps.common_cap.rdma ?
					  GLNVM_ULD_PE_DONE_M : 0);

	 
	for (cnt = 0; cnt < ICE_PF_RESET_WAIT_COUNT; cnt++) {
		reg = rd32(hw, GLNVM_ULD) & uld_mask;
		if (reg == uld_mask) {
			ice_debug(hw, ICE_DBG_INIT, "Global reset processes done. %d\n", cnt);
			break;
		}
		mdelay(10);
	}

	if (cnt == ICE_PF_RESET_WAIT_COUNT) {
		ice_debug(hw, ICE_DBG_INIT, "Wait for Reset Done timed out. GLNVM_ULD = 0x%x\n",
			  reg);
		return -EIO;
	}

	return 0;
}

 
static int ice_pf_reset(struct ice_hw *hw)
{
	u32 cnt, reg;

	 
	if ((rd32(hw, GLGEN_RSTAT) & GLGEN_RSTAT_DEVSTATE_M) ||
	    (rd32(hw, GLNVM_ULD) & ICE_RESET_DONE_MASK) ^ ICE_RESET_DONE_MASK) {
		 
		if (ice_check_reset(hw))
			return -EIO;

		return 0;
	}

	 
	reg = rd32(hw, PFGEN_CTRL);

	wr32(hw, PFGEN_CTRL, (reg | PFGEN_CTRL_PFSWR_M));

	 
	for (cnt = 0; cnt < ICE_GLOBAL_CFG_LOCK_TIMEOUT +
	     ICE_PF_RESET_WAIT_COUNT; cnt++) {
		reg = rd32(hw, PFGEN_CTRL);
		if (!(reg & PFGEN_CTRL_PFSWR_M))
			break;

		mdelay(1);
	}

	if (cnt == ICE_PF_RESET_WAIT_COUNT) {
		ice_debug(hw, ICE_DBG_INIT, "PF reset polling failed to complete.\n");
		return -EIO;
	}

	return 0;
}

 
int ice_reset(struct ice_hw *hw, enum ice_reset_req req)
{
	u32 val = 0;

	switch (req) {
	case ICE_RESET_PFR:
		return ice_pf_reset(hw);
	case ICE_RESET_CORER:
		ice_debug(hw, ICE_DBG_INIT, "CoreR requested\n");
		val = GLGEN_RTRIG_CORER_M;
		break;
	case ICE_RESET_GLOBR:
		ice_debug(hw, ICE_DBG_INIT, "GlobalR requested\n");
		val = GLGEN_RTRIG_GLOBR_M;
		break;
	default:
		return -EINVAL;
	}

	val |= rd32(hw, GLGEN_RTRIG);
	wr32(hw, GLGEN_RTRIG, val);
	ice_flush(hw);

	 
	return ice_check_reset(hw);
}

 
static int
ice_copy_rxq_ctx_to_hw(struct ice_hw *hw, u8 *ice_rxq_ctx, u32 rxq_index)
{
	u8 i;

	if (!ice_rxq_ctx)
		return -EINVAL;

	if (rxq_index > QRX_CTRL_MAX_INDEX)
		return -EINVAL;

	 
	for (i = 0; i < ICE_RXQ_CTX_SIZE_DWORDS; i++) {
		wr32(hw, QRX_CONTEXT(i, rxq_index),
		     *((u32 *)(ice_rxq_ctx + (i * sizeof(u32)))));

		ice_debug(hw, ICE_DBG_QCTX, "qrxdata[%d]: %08X\n", i,
			  *((u32 *)(ice_rxq_ctx + (i * sizeof(u32)))));
	}

	return 0;
}

 
static const struct ice_ctx_ele ice_rlan_ctx_info[] = {
	 
	ICE_CTX_STORE(ice_rlan_ctx, head,		13,	0),
	ICE_CTX_STORE(ice_rlan_ctx, cpuid,		8,	13),
	ICE_CTX_STORE(ice_rlan_ctx, base,		57,	32),
	ICE_CTX_STORE(ice_rlan_ctx, qlen,		13,	89),
	ICE_CTX_STORE(ice_rlan_ctx, dbuf,		7,	102),
	ICE_CTX_STORE(ice_rlan_ctx, hbuf,		5,	109),
	ICE_CTX_STORE(ice_rlan_ctx, dtype,		2,	114),
	ICE_CTX_STORE(ice_rlan_ctx, dsize,		1,	116),
	ICE_CTX_STORE(ice_rlan_ctx, crcstrip,		1,	117),
	ICE_CTX_STORE(ice_rlan_ctx, l2tsel,		1,	119),
	ICE_CTX_STORE(ice_rlan_ctx, hsplit_0,		4,	120),
	ICE_CTX_STORE(ice_rlan_ctx, hsplit_1,		2,	124),
	ICE_CTX_STORE(ice_rlan_ctx, showiv,		1,	127),
	ICE_CTX_STORE(ice_rlan_ctx, rxmax,		14,	174),
	ICE_CTX_STORE(ice_rlan_ctx, tphrdesc_ena,	1,	193),
	ICE_CTX_STORE(ice_rlan_ctx, tphwdesc_ena,	1,	194),
	ICE_CTX_STORE(ice_rlan_ctx, tphdata_ena,	1,	195),
	ICE_CTX_STORE(ice_rlan_ctx, tphhead_ena,	1,	196),
	ICE_CTX_STORE(ice_rlan_ctx, lrxqthresh,		3,	198),
	ICE_CTX_STORE(ice_rlan_ctx, prefena,		1,	201),
	{ 0 }
};

 
int
ice_write_rxq_ctx(struct ice_hw *hw, struct ice_rlan_ctx *rlan_ctx,
		  u32 rxq_index)
{
	u8 ctx_buf[ICE_RXQ_CTX_SZ] = { 0 };

	if (!rlan_ctx)
		return -EINVAL;

	rlan_ctx->prefena = 1;

	ice_set_ctx(hw, (u8 *)rlan_ctx, ctx_buf, ice_rlan_ctx_info);
	return ice_copy_rxq_ctx_to_hw(hw, ctx_buf, rxq_index);
}

 
const struct ice_ctx_ele ice_tlan_ctx_info[] = {
				     
	ICE_CTX_STORE(ice_tlan_ctx, base,			57,	0),
	ICE_CTX_STORE(ice_tlan_ctx, port_num,			3,	57),
	ICE_CTX_STORE(ice_tlan_ctx, cgd_num,			5,	60),
	ICE_CTX_STORE(ice_tlan_ctx, pf_num,			3,	65),
	ICE_CTX_STORE(ice_tlan_ctx, vmvf_num,			10,	68),
	ICE_CTX_STORE(ice_tlan_ctx, vmvf_type,			2,	78),
	ICE_CTX_STORE(ice_tlan_ctx, src_vsi,			10,	80),
	ICE_CTX_STORE(ice_tlan_ctx, tsyn_ena,			1,	90),
	ICE_CTX_STORE(ice_tlan_ctx, internal_usage_flag,	1,	91),
	ICE_CTX_STORE(ice_tlan_ctx, alt_vlan,			1,	92),
	ICE_CTX_STORE(ice_tlan_ctx, cpuid,			8,	93),
	ICE_CTX_STORE(ice_tlan_ctx, wb_mode,			1,	101),
	ICE_CTX_STORE(ice_tlan_ctx, tphrd_desc,			1,	102),
	ICE_CTX_STORE(ice_tlan_ctx, tphrd,			1,	103),
	ICE_CTX_STORE(ice_tlan_ctx, tphwr_desc,			1,	104),
	ICE_CTX_STORE(ice_tlan_ctx, cmpq_id,			9,	105),
	ICE_CTX_STORE(ice_tlan_ctx, qnum_in_func,		14,	114),
	ICE_CTX_STORE(ice_tlan_ctx, itr_notification_mode,	1,	128),
	ICE_CTX_STORE(ice_tlan_ctx, adjust_prof_id,		6,	129),
	ICE_CTX_STORE(ice_tlan_ctx, qlen,			13,	135),
	ICE_CTX_STORE(ice_tlan_ctx, quanta_prof_idx,		4,	148),
	ICE_CTX_STORE(ice_tlan_ctx, tso_ena,			1,	152),
	ICE_CTX_STORE(ice_tlan_ctx, tso_qnum,			11,	153),
	ICE_CTX_STORE(ice_tlan_ctx, legacy_int,			1,	164),
	ICE_CTX_STORE(ice_tlan_ctx, drop_ena,			1,	165),
	ICE_CTX_STORE(ice_tlan_ctx, cache_prof_idx,		2,	166),
	ICE_CTX_STORE(ice_tlan_ctx, pkt_shaper_prof_idx,	3,	168),
	ICE_CTX_STORE(ice_tlan_ctx, int_q_state,		122,	171),
	{ 0 }
};

 

 
static int
ice_sbq_send_cmd(struct ice_hw *hw, struct ice_sbq_cmd_desc *desc,
		 void *buf, u16 buf_size, struct ice_sq_cd *cd)
{
	return ice_sq_send_cmd(hw, ice_get_sbq(hw),
			       (struct ice_aq_desc *)desc, buf, buf_size, cd);
}

 
int ice_sbq_rw_reg(struct ice_hw *hw, struct ice_sbq_msg_input *in)
{
	struct ice_sbq_cmd_desc desc = {0};
	struct ice_sbq_msg_req msg = {0};
	u16 msg_len;
	int status;

	msg_len = sizeof(msg);

	msg.dest_dev = in->dest_dev;
	msg.opcode = in->opcode;
	msg.flags = ICE_SBQ_MSG_FLAGS;
	msg.sbe_fbe = ICE_SBQ_MSG_SBE_FBE;
	msg.msg_addr_low = cpu_to_le16(in->msg_addr_low);
	msg.msg_addr_high = cpu_to_le32(in->msg_addr_high);

	if (in->opcode)
		msg.data = cpu_to_le32(in->data);
	else
		 
		msg_len -= sizeof(msg.data);

	desc.flags = cpu_to_le16(ICE_AQ_FLAG_RD);
	desc.opcode = cpu_to_le16(ice_sbq_opc_neigh_dev_req);
	desc.param0.cmd_len = cpu_to_le16(msg_len);
	status = ice_sbq_send_cmd(hw, &desc, &msg, msg_len, NULL);
	if (!status && !in->opcode)
		in->data = le32_to_cpu
			(((struct ice_sbq_msg_cmpl *)&msg)->data);
	return status;
}

 

 
DEFINE_MUTEX(ice_global_cfg_lock_sw);

 
static bool ice_should_retry_sq_send_cmd(u16 opcode)
{
	switch (opcode) {
	case ice_aqc_opc_get_link_topo:
	case ice_aqc_opc_lldp_stop:
	case ice_aqc_opc_lldp_start:
	case ice_aqc_opc_lldp_filter_ctrl:
		return true;
	}

	return false;
}

 
static int
ice_sq_send_cmd_retry(struct ice_hw *hw, struct ice_ctl_q_info *cq,
		      struct ice_aq_desc *desc, void *buf, u16 buf_size,
		      struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc_cpy;
	bool is_cmd_for_retry;
	u8 idx = 0;
	u16 opcode;
	int status;

	opcode = le16_to_cpu(desc->opcode);
	is_cmd_for_retry = ice_should_retry_sq_send_cmd(opcode);
	memset(&desc_cpy, 0, sizeof(desc_cpy));

	if (is_cmd_for_retry) {
		 
		WARN_ON(buf);

		memcpy(&desc_cpy, desc, sizeof(desc_cpy));
	}

	do {
		status = ice_sq_send_cmd(hw, cq, desc, buf, buf_size, cd);

		if (!is_cmd_for_retry || !status ||
		    hw->adminq.sq_last_status != ICE_AQ_RC_EBUSY)
			break;

		memcpy(desc, &desc_cpy, sizeof(desc_cpy));

		msleep(ICE_SQ_SEND_DELAY_TIME_MS);

	} while (++idx < ICE_SQ_SEND_MAX_EXECUTE);

	return status;
}

 
int
ice_aq_send_cmd(struct ice_hw *hw, struct ice_aq_desc *desc, void *buf,
		u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aqc_req_res *cmd = &desc->params.res_owner;
	bool lock_acquired = false;
	int status;

	 
	switch (le16_to_cpu(desc->opcode)) {
	case ice_aqc_opc_download_pkg:
	case ice_aqc_opc_get_pkg_info_list:
	case ice_aqc_opc_get_ver:
	case ice_aqc_opc_upload_section:
	case ice_aqc_opc_update_pkg:
	case ice_aqc_opc_set_port_params:
	case ice_aqc_opc_get_vlan_mode_parameters:
	case ice_aqc_opc_set_vlan_mode_parameters:
	case ice_aqc_opc_add_recipe:
	case ice_aqc_opc_recipe_to_profile:
	case ice_aqc_opc_get_recipe:
	case ice_aqc_opc_get_recipe_to_profile:
		break;
	case ice_aqc_opc_release_res:
		if (le16_to_cpu(cmd->res_id) == ICE_AQC_RES_ID_GLBL_LOCK)
			break;
		fallthrough;
	default:
		mutex_lock(&ice_global_cfg_lock_sw);
		lock_acquired = true;
		break;
	}

	status = ice_sq_send_cmd_retry(hw, &hw->adminq, desc, buf, buf_size, cd);
	if (lock_acquired)
		mutex_unlock(&ice_global_cfg_lock_sw);

	return status;
}

 
int ice_aq_get_fw_ver(struct ice_hw *hw, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_ver *resp;
	struct ice_aq_desc desc;
	int status;

	resp = &desc.params.get_ver;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_ver);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	if (!status) {
		hw->fw_branch = resp->fw_branch;
		hw->fw_maj_ver = resp->fw_major;
		hw->fw_min_ver = resp->fw_minor;
		hw->fw_patch = resp->fw_patch;
		hw->fw_build = le32_to_cpu(resp->fw_build);
		hw->api_branch = resp->api_branch;
		hw->api_maj_ver = resp->api_major;
		hw->api_min_ver = resp->api_minor;
		hw->api_patch = resp->api_patch;
	}

	return status;
}

 
int
ice_aq_send_driver_ver(struct ice_hw *hw, struct ice_driver_ver *dv,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_driver_ver *cmd;
	struct ice_aq_desc desc;
	u16 len;

	cmd = &desc.params.driver_ver;

	if (!dv)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_driver_ver);

	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);
	cmd->major_ver = dv->major_ver;
	cmd->minor_ver = dv->minor_ver;
	cmd->build_ver = dv->build_ver;
	cmd->subbuild_ver = dv->subbuild_ver;

	len = 0;
	while (len < sizeof(dv->driver_string) &&
	       isascii(dv->driver_string[len]) && dv->driver_string[len])
		len++;

	return ice_aq_send_cmd(hw, &desc, dv->driver_string, len, cd);
}

 
int ice_aq_q_shutdown(struct ice_hw *hw, bool unloading)
{
	struct ice_aqc_q_shutdown *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.q_shutdown;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_q_shutdown);

	if (unloading)
		cmd->driver_unloading = ICE_AQC_DRIVER_UNLOADING;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 
static int
ice_aq_req_res(struct ice_hw *hw, enum ice_aq_res_ids res,
	       enum ice_aq_res_access_type access, u8 sdp_number, u32 *timeout,
	       struct ice_sq_cd *cd)
{
	struct ice_aqc_req_res *cmd_resp;
	struct ice_aq_desc desc;
	int status;

	cmd_resp = &desc.params.res_owner;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_req_res);

	cmd_resp->res_id = cpu_to_le16(res);
	cmd_resp->access_type = cpu_to_le16(access);
	cmd_resp->res_number = cpu_to_le32(sdp_number);
	cmd_resp->timeout = cpu_to_le32(*timeout);
	*timeout = 0;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	 

	 
	if (res == ICE_GLOBAL_CFG_LOCK_RES_ID) {
		if (le16_to_cpu(cmd_resp->status) == ICE_AQ_RES_GLBL_SUCCESS) {
			*timeout = le32_to_cpu(cmd_resp->timeout);
			return 0;
		} else if (le16_to_cpu(cmd_resp->status) ==
			   ICE_AQ_RES_GLBL_IN_PROG) {
			*timeout = le32_to_cpu(cmd_resp->timeout);
			return -EIO;
		} else if (le16_to_cpu(cmd_resp->status) ==
			   ICE_AQ_RES_GLBL_DONE) {
			return -EALREADY;
		}

		 
		*timeout = 0;
		return -EIO;
	}

	 
	if (!status || hw->adminq.sq_last_status == ICE_AQ_RC_EBUSY)
		*timeout = le32_to_cpu(cmd_resp->timeout);

	return status;
}

 
static int
ice_aq_release_res(struct ice_hw *hw, enum ice_aq_res_ids res, u8 sdp_number,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_req_res *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.res_owner;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_release_res);

	cmd->res_id = cpu_to_le16(res);
	cmd->res_number = cpu_to_le32(sdp_number);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_acquire_res(struct ice_hw *hw, enum ice_aq_res_ids res,
		enum ice_aq_res_access_type access, u32 timeout)
{
#define ICE_RES_POLLING_DELAY_MS	10
	u32 delay = ICE_RES_POLLING_DELAY_MS;
	u32 time_left = timeout;
	int status;

	status = ice_aq_req_res(hw, res, access, 0, &time_left, NULL);

	 
	if (status == -EALREADY)
		goto ice_acquire_res_exit;

	if (status)
		ice_debug(hw, ICE_DBG_RES, "resource %d acquire type %d failed.\n", res, access);

	 
	timeout = time_left;
	while (status && timeout && time_left) {
		mdelay(delay);
		timeout = (timeout > delay) ? timeout - delay : 0;
		status = ice_aq_req_res(hw, res, access, 0, &time_left, NULL);

		if (status == -EALREADY)
			 
			break;

		if (!status)
			 
			break;
	}
	if (status && status != -EALREADY)
		ice_debug(hw, ICE_DBG_RES, "resource acquire timed out.\n");

ice_acquire_res_exit:
	if (status == -EALREADY) {
		if (access == ICE_RES_WRITE)
			ice_debug(hw, ICE_DBG_RES, "resource indicates no work to do.\n");
		else
			ice_debug(hw, ICE_DBG_RES, "Warning: -EALREADY not expected\n");
	}
	return status;
}

 
void ice_release_res(struct ice_hw *hw, enum ice_aq_res_ids res)
{
	unsigned long timeout;
	int status;

	 
	timeout = jiffies + 10 * ICE_CTL_Q_SQ_CMD_TIMEOUT;
	do {
		status = ice_aq_release_res(hw, res, 0, NULL);
		if (status != -EIO)
			break;
		usleep_range(1000, 2000);
	} while (time_before(jiffies, timeout));
}

 
int ice_aq_alloc_free_res(struct ice_hw *hw,
			  struct ice_aqc_alloc_free_res_elem *buf, u16 buf_size,
			  enum ice_adminq_opc opc)
{
	struct ice_aqc_alloc_free_res_cmd *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.sw_res_ctrl;

	if (!buf || buf_size < flex_array_size(buf, elem, 1))
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, opc);

	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	cmd->num_entries = cpu_to_le16(1);

	return ice_aq_send_cmd(hw, &desc, buf, buf_size, NULL);
}

 
int
ice_alloc_hw_res(struct ice_hw *hw, u16 type, u16 num, bool btm, u16 *res)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	u16 buf_len;
	int status;

	buf_len = struct_size(buf, elem, num);
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	buf->num_elems = cpu_to_le16(num);
	buf->res_type = cpu_to_le16(type | ICE_AQC_RES_TYPE_FLAG_DEDICATED |
				    ICE_AQC_RES_TYPE_FLAG_IGNORE_INDEX);
	if (btm)
		buf->res_type |= cpu_to_le16(ICE_AQC_RES_TYPE_FLAG_SCAN_BOTTOM);

	status = ice_aq_alloc_free_res(hw, buf, buf_len, ice_aqc_opc_alloc_res);
	if (status)
		goto ice_alloc_res_exit;

	memcpy(res, buf->elem, sizeof(*buf->elem) * num);

ice_alloc_res_exit:
	kfree(buf);
	return status;
}

 
int ice_free_hw_res(struct ice_hw *hw, u16 type, u16 num, u16 *res)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	u16 buf_len;
	int status;

	buf_len = struct_size(buf, elem, num);
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	buf->num_elems = cpu_to_le16(num);
	buf->res_type = cpu_to_le16(type);
	memcpy(buf->elem, res, sizeof(*buf->elem) * num);

	status = ice_aq_alloc_free_res(hw, buf, buf_len, ice_aqc_opc_free_res);
	if (status)
		ice_debug(hw, ICE_DBG_SW, "CQ CMD Buffer:\n");

	kfree(buf);
	return status;
}

 
static u32 ice_get_num_per_func(struct ice_hw *hw, u32 max)
{
	u8 funcs;

#define ICE_CAPS_VALID_FUNCS_M	0xFF
	funcs = hweight8(hw->dev_caps.common_cap.valid_functions &
			 ICE_CAPS_VALID_FUNCS_M);

	if (!funcs)
		return 0;

	return max / funcs;
}

 
static bool
ice_parse_common_caps(struct ice_hw *hw, struct ice_hw_common_caps *caps,
		      struct ice_aqc_list_caps_elem *elem, const char *prefix)
{
	u32 logical_id = le32_to_cpu(elem->logical_id);
	u32 phys_id = le32_to_cpu(elem->phys_id);
	u32 number = le32_to_cpu(elem->number);
	u16 cap = le16_to_cpu(elem->cap);
	bool found = true;

	switch (cap) {
	case ICE_AQC_CAPS_VALID_FUNCTIONS:
		caps->valid_functions = number;
		ice_debug(hw, ICE_DBG_INIT, "%s: valid_functions (bitmap) = %d\n", prefix,
			  caps->valid_functions);
		break;
	case ICE_AQC_CAPS_SRIOV:
		caps->sr_iov_1_1 = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: sr_iov_1_1 = %d\n", prefix,
			  caps->sr_iov_1_1);
		break;
	case ICE_AQC_CAPS_DCB:
		caps->dcb = (number == 1);
		caps->active_tc_bitmap = logical_id;
		caps->maxtc = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: dcb = %d\n", prefix, caps->dcb);
		ice_debug(hw, ICE_DBG_INIT, "%s: active_tc_bitmap = %d\n", prefix,
			  caps->active_tc_bitmap);
		ice_debug(hw, ICE_DBG_INIT, "%s: maxtc = %d\n", prefix, caps->maxtc);
		break;
	case ICE_AQC_CAPS_RSS:
		caps->rss_table_size = number;
		caps->rss_table_entry_width = logical_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: rss_table_size = %d\n", prefix,
			  caps->rss_table_size);
		ice_debug(hw, ICE_DBG_INIT, "%s: rss_table_entry_width = %d\n", prefix,
			  caps->rss_table_entry_width);
		break;
	case ICE_AQC_CAPS_RXQS:
		caps->num_rxq = number;
		caps->rxq_first_id = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: num_rxq = %d\n", prefix,
			  caps->num_rxq);
		ice_debug(hw, ICE_DBG_INIT, "%s: rxq_first_id = %d\n", prefix,
			  caps->rxq_first_id);
		break;
	case ICE_AQC_CAPS_TXQS:
		caps->num_txq = number;
		caps->txq_first_id = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: num_txq = %d\n", prefix,
			  caps->num_txq);
		ice_debug(hw, ICE_DBG_INIT, "%s: txq_first_id = %d\n", prefix,
			  caps->txq_first_id);
		break;
	case ICE_AQC_CAPS_MSIX:
		caps->num_msix_vectors = number;
		caps->msix_vector_first_id = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: num_msix_vectors = %d\n", prefix,
			  caps->num_msix_vectors);
		ice_debug(hw, ICE_DBG_INIT, "%s: msix_vector_first_id = %d\n", prefix,
			  caps->msix_vector_first_id);
		break;
	case ICE_AQC_CAPS_PENDING_NVM_VER:
		caps->nvm_update_pending_nvm = true;
		ice_debug(hw, ICE_DBG_INIT, "%s: update_pending_nvm\n", prefix);
		break;
	case ICE_AQC_CAPS_PENDING_OROM_VER:
		caps->nvm_update_pending_orom = true;
		ice_debug(hw, ICE_DBG_INIT, "%s: update_pending_orom\n", prefix);
		break;
	case ICE_AQC_CAPS_PENDING_NET_VER:
		caps->nvm_update_pending_netlist = true;
		ice_debug(hw, ICE_DBG_INIT, "%s: update_pending_netlist\n", prefix);
		break;
	case ICE_AQC_CAPS_NVM_MGMT:
		caps->nvm_unified_update =
			(number & ICE_NVM_MGMT_UNIFIED_UPD_SUPPORT) ?
			true : false;
		ice_debug(hw, ICE_DBG_INIT, "%s: nvm_unified_update = %d\n", prefix,
			  caps->nvm_unified_update);
		break;
	case ICE_AQC_CAPS_RDMA:
		caps->rdma = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: rdma = %d\n", prefix, caps->rdma);
		break;
	case ICE_AQC_CAPS_MAX_MTU:
		caps->max_mtu = number;
		ice_debug(hw, ICE_DBG_INIT, "%s: max_mtu = %d\n",
			  prefix, caps->max_mtu);
		break;
	case ICE_AQC_CAPS_PCIE_RESET_AVOIDANCE:
		caps->pcie_reset_avoidance = (number > 0);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: pcie_reset_avoidance = %d\n", prefix,
			  caps->pcie_reset_avoidance);
		break;
	case ICE_AQC_CAPS_POST_UPDATE_RESET_RESTRICT:
		caps->reset_restrict_support = (number == 1);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: reset_restrict_support = %d\n", prefix,
			  caps->reset_restrict_support);
		break;
	case ICE_AQC_CAPS_FW_LAG_SUPPORT:
		caps->roce_lag = !!(number & ICE_AQC_BIT_ROCEV2_LAG);
		ice_debug(hw, ICE_DBG_INIT, "%s: roce_lag = %u\n",
			  prefix, caps->roce_lag);
		caps->sriov_lag = !!(number & ICE_AQC_BIT_SRIOV_LAG);
		ice_debug(hw, ICE_DBG_INIT, "%s: sriov_lag = %u\n",
			  prefix, caps->sriov_lag);
		break;
	default:
		 
		found = false;
	}

	return found;
}

 
static void
ice_recalc_port_limited_caps(struct ice_hw *hw, struct ice_hw_common_caps *caps)
{
	 
	if (hw->dev_caps.num_funcs > 4) {
		 
		caps->maxtc = 4;
		ice_debug(hw, ICE_DBG_INIT, "reducing maxtc to %d (based on #ports)\n",
			  caps->maxtc);
		if (caps->rdma) {
			ice_debug(hw, ICE_DBG_INIT, "forcing RDMA off\n");
			caps->rdma = 0;
		}

		 
		if (caps == &hw->dev_caps.common_cap)
			dev_info(ice_hw_to_dev(hw), "RDMA functionality is not available with the current device configuration.\n");
	}
}

 
static void
ice_parse_vf_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
		       struct ice_aqc_list_caps_elem *cap)
{
	u32 logical_id = le32_to_cpu(cap->logical_id);
	u32 number = le32_to_cpu(cap->number);

	func_p->num_allocd_vfs = number;
	func_p->vf_base_id = logical_id;
	ice_debug(hw, ICE_DBG_INIT, "func caps: num_allocd_vfs = %d\n",
		  func_p->num_allocd_vfs);
	ice_debug(hw, ICE_DBG_INIT, "func caps: vf_base_id = %d\n",
		  func_p->vf_base_id);
}

 
static void
ice_parse_vsi_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
			struct ice_aqc_list_caps_elem *cap)
{
	func_p->guar_num_vsi = ice_get_num_per_func(hw, ICE_MAX_VSI);
	ice_debug(hw, ICE_DBG_INIT, "func caps: guar_num_vsi (fw) = %d\n",
		  le32_to_cpu(cap->number));
	ice_debug(hw, ICE_DBG_INIT, "func caps: guar_num_vsi = %d\n",
		  func_p->guar_num_vsi);
}

 
static void
ice_parse_1588_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
			 struct ice_aqc_list_caps_elem *cap)
{
	struct ice_ts_func_info *info = &func_p->ts_func_info;
	u32 number = le32_to_cpu(cap->number);

	info->ena = ((number & ICE_TS_FUNC_ENA_M) != 0);
	func_p->common_cap.ieee_1588 = info->ena;

	info->src_tmr_owned = ((number & ICE_TS_SRC_TMR_OWND_M) != 0);
	info->tmr_ena = ((number & ICE_TS_TMR_ENA_M) != 0);
	info->tmr_index_owned = ((number & ICE_TS_TMR_IDX_OWND_M) != 0);
	info->tmr_index_assoc = ((number & ICE_TS_TMR_IDX_ASSOC_M) != 0);

	info->clk_freq = (number & ICE_TS_CLK_FREQ_M) >> ICE_TS_CLK_FREQ_S;
	info->clk_src = ((number & ICE_TS_CLK_SRC_M) != 0);

	if (info->clk_freq < NUM_ICE_TIME_REF_FREQ) {
		info->time_ref = (enum ice_time_ref_freq)info->clk_freq;
	} else {
		 
		ice_debug(hw, ICE_DBG_INIT, "1588 func caps: unknown clock frequency %u\n",
			  info->clk_freq);
		info->time_ref = ICE_TIME_REF_FREQ_25_000;
	}

	ice_debug(hw, ICE_DBG_INIT, "func caps: ieee_1588 = %u\n",
		  func_p->common_cap.ieee_1588);
	ice_debug(hw, ICE_DBG_INIT, "func caps: src_tmr_owned = %u\n",
		  info->src_tmr_owned);
	ice_debug(hw, ICE_DBG_INIT, "func caps: tmr_ena = %u\n",
		  info->tmr_ena);
	ice_debug(hw, ICE_DBG_INIT, "func caps: tmr_index_owned = %u\n",
		  info->tmr_index_owned);
	ice_debug(hw, ICE_DBG_INIT, "func caps: tmr_index_assoc = %u\n",
		  info->tmr_index_assoc);
	ice_debug(hw, ICE_DBG_INIT, "func caps: clk_freq = %u\n",
		  info->clk_freq);
	ice_debug(hw, ICE_DBG_INIT, "func caps: clk_src = %u\n",
		  info->clk_src);
}

 
static void
ice_parse_fdir_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p)
{
	u32 reg_val, val;

	reg_val = rd32(hw, GLQF_FD_SIZE);
	val = (reg_val & GLQF_FD_SIZE_FD_GSIZE_M) >>
		GLQF_FD_SIZE_FD_GSIZE_S;
	func_p->fd_fltr_guar =
		ice_get_num_per_func(hw, val);
	val = (reg_val & GLQF_FD_SIZE_FD_BSIZE_M) >>
		GLQF_FD_SIZE_FD_BSIZE_S;
	func_p->fd_fltr_best_effort = val;

	ice_debug(hw, ICE_DBG_INIT, "func caps: fd_fltr_guar = %d\n",
		  func_p->fd_fltr_guar);
	ice_debug(hw, ICE_DBG_INIT, "func caps: fd_fltr_best_effort = %d\n",
		  func_p->fd_fltr_best_effort);
}

 
static void
ice_parse_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
		    void *buf, u32 cap_count)
{
	struct ice_aqc_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = buf;

	memset(func_p, 0, sizeof(*func_p));

	for (i = 0; i < cap_count; i++) {
		u16 cap = le16_to_cpu(cap_resp[i].cap);
		bool found;

		found = ice_parse_common_caps(hw, &func_p->common_cap,
					      &cap_resp[i], "func caps");

		switch (cap) {
		case ICE_AQC_CAPS_VF:
			ice_parse_vf_func_caps(hw, func_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_VSI:
			ice_parse_vsi_func_caps(hw, func_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_1588:
			ice_parse_1588_func_caps(hw, func_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_FD:
			ice_parse_fdir_func_caps(hw, func_p);
			break;
		default:
			 
			if (!found)
				ice_debug(hw, ICE_DBG_INIT, "func caps: unknown capability[%d]: 0x%x\n",
					  i, cap);
			break;
		}
	}

	ice_recalc_port_limited_caps(hw, &func_p->common_cap);
}

 
static void
ice_parse_valid_functions_cap(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
			      struct ice_aqc_list_caps_elem *cap)
{
	u32 number = le32_to_cpu(cap->number);

	dev_p->num_funcs = hweight32(number);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: num_funcs = %d\n",
		  dev_p->num_funcs);
}

 
static void
ice_parse_vf_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
		      struct ice_aqc_list_caps_elem *cap)
{
	u32 number = le32_to_cpu(cap->number);

	dev_p->num_vfs_exposed = number;
	ice_debug(hw, ICE_DBG_INIT, "dev_caps: num_vfs_exposed = %d\n",
		  dev_p->num_vfs_exposed);
}

 
static void
ice_parse_vsi_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
		       struct ice_aqc_list_caps_elem *cap)
{
	u32 number = le32_to_cpu(cap->number);

	dev_p->num_vsi_allocd_to_host = number;
	ice_debug(hw, ICE_DBG_INIT, "dev caps: num_vsi_allocd_to_host = %d\n",
		  dev_p->num_vsi_allocd_to_host);
}

 
static void
ice_parse_1588_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
			struct ice_aqc_list_caps_elem *cap)
{
	struct ice_ts_dev_info *info = &dev_p->ts_dev_info;
	u32 logical_id = le32_to_cpu(cap->logical_id);
	u32 phys_id = le32_to_cpu(cap->phys_id);
	u32 number = le32_to_cpu(cap->number);

	info->ena = ((number & ICE_TS_DEV_ENA_M) != 0);
	dev_p->common_cap.ieee_1588 = info->ena;

	info->tmr0_owner = number & ICE_TS_TMR0_OWNR_M;
	info->tmr0_owned = ((number & ICE_TS_TMR0_OWND_M) != 0);
	info->tmr0_ena = ((number & ICE_TS_TMR0_ENA_M) != 0);

	info->tmr1_owner = (number & ICE_TS_TMR1_OWNR_M) >> ICE_TS_TMR1_OWNR_S;
	info->tmr1_owned = ((number & ICE_TS_TMR1_OWND_M) != 0);
	info->tmr1_ena = ((number & ICE_TS_TMR1_ENA_M) != 0);

	info->ts_ll_read = ((number & ICE_TS_LL_TX_TS_READ_M) != 0);

	info->ena_ports = logical_id;
	info->tmr_own_map = phys_id;

	ice_debug(hw, ICE_DBG_INIT, "dev caps: ieee_1588 = %u\n",
		  dev_p->common_cap.ieee_1588);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr0_owner = %u\n",
		  info->tmr0_owner);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr0_owned = %u\n",
		  info->tmr0_owned);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr0_ena = %u\n",
		  info->tmr0_ena);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr1_owner = %u\n",
		  info->tmr1_owner);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr1_owned = %u\n",
		  info->tmr1_owned);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr1_ena = %u\n",
		  info->tmr1_ena);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: ts_ll_read = %u\n",
		  info->ts_ll_read);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: ieee_1588 ena_ports = %u\n",
		  info->ena_ports);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: tmr_own_map = %u\n",
		  info->tmr_own_map);
}

 
static void
ice_parse_fdir_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
			struct ice_aqc_list_caps_elem *cap)
{
	u32 number = le32_to_cpu(cap->number);

	dev_p->num_flow_director_fltr = number;
	ice_debug(hw, ICE_DBG_INIT, "dev caps: num_flow_director_fltr = %d\n",
		  dev_p->num_flow_director_fltr);
}

 
static void
ice_parse_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
		   void *buf, u32 cap_count)
{
	struct ice_aqc_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = buf;

	memset(dev_p, 0, sizeof(*dev_p));

	for (i = 0; i < cap_count; i++) {
		u16 cap = le16_to_cpu(cap_resp[i].cap);
		bool found;

		found = ice_parse_common_caps(hw, &dev_p->common_cap,
					      &cap_resp[i], "dev caps");

		switch (cap) {
		case ICE_AQC_CAPS_VALID_FUNCTIONS:
			ice_parse_valid_functions_cap(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_VF:
			ice_parse_vf_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_VSI:
			ice_parse_vsi_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_1588:
			ice_parse_1588_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case  ICE_AQC_CAPS_FD:
			ice_parse_fdir_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		default:
			 
			if (!found)
				ice_debug(hw, ICE_DBG_INIT, "dev caps: unknown capability[%d]: 0x%x\n",
					  i, cap);
			break;
		}
	}

	ice_recalc_port_limited_caps(hw, &dev_p->common_cap);
}

 
static int
ice_aq_get_netlist_node(struct ice_hw *hw, struct ice_aqc_get_link_topo *cmd,
			u8 *node_part_number, u16 *node_handle)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_topo);
	desc.params.get_link_topo = *cmd;

	if (ice_aq_send_cmd(hw, &desc, NULL, 0, NULL))
		return -EIO;

	if (node_handle)
		*node_handle = le16_to_cpu(desc.params.get_link_topo.addr.handle);
	if (node_part_number)
		*node_part_number = desc.params.get_link_topo.node_part_num;

	return 0;
}

 
bool ice_is_pf_c827(struct ice_hw *hw)
{
	struct ice_aqc_get_link_topo cmd = {};
	u8 node_part_number;
	u16 node_handle;
	int status;

	if (hw->mac_type != ICE_MAC_E810)
		return false;

	if (hw->device_id != ICE_DEV_ID_E810C_QSFP)
		return true;

	cmd.addr.topo_params.node_type_ctx =
		FIELD_PREP(ICE_AQC_LINK_TOPO_NODE_TYPE_M, ICE_AQC_LINK_TOPO_NODE_TYPE_PHY) |
		FIELD_PREP(ICE_AQC_LINK_TOPO_NODE_CTX_M, ICE_AQC_LINK_TOPO_NODE_CTX_PORT);
	cmd.addr.topo_params.index = 0;

	status = ice_aq_get_netlist_node(hw, &cmd, &node_part_number,
					 &node_handle);

	if (status || node_part_number != ICE_AQC_GET_LINK_TOPO_NODE_NR_C827)
		return false;

	if (node_handle == E810C_QSFP_C827_0_HANDLE || node_handle == E810C_QSFP_C827_1_HANDLE)
		return true;

	return false;
}

 
int
ice_aq_list_caps(struct ice_hw *hw, void *buf, u16 buf_size, u32 *cap_count,
		 enum ice_adminq_opc opc, struct ice_sq_cd *cd)
{
	struct ice_aqc_list_caps *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.get_cap;

	if (opc != ice_aqc_opc_list_func_caps &&
	    opc != ice_aqc_opc_list_dev_caps)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, opc);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);

	if (cap_count)
		*cap_count = le32_to_cpu(cmd->count);

	return status;
}

 
int
ice_discover_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_caps)
{
	u32 cap_count = 0;
	void *cbuf;
	int status;

	cbuf = kzalloc(ICE_AQ_MAX_BUF_LEN, GFP_KERNEL);
	if (!cbuf)
		return -ENOMEM;

	 
	cap_count = ICE_AQ_MAX_BUF_LEN / sizeof(struct ice_aqc_list_caps_elem);

	status = ice_aq_list_caps(hw, cbuf, ICE_AQ_MAX_BUF_LEN, &cap_count,
				  ice_aqc_opc_list_dev_caps, NULL);
	if (!status)
		ice_parse_dev_caps(hw, dev_caps, cbuf, cap_count);
	kfree(cbuf);

	return status;
}

 
static int
ice_discover_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_caps)
{
	u32 cap_count = 0;
	void *cbuf;
	int status;

	cbuf = kzalloc(ICE_AQ_MAX_BUF_LEN, GFP_KERNEL);
	if (!cbuf)
		return -ENOMEM;

	 
	cap_count = ICE_AQ_MAX_BUF_LEN / sizeof(struct ice_aqc_list_caps_elem);

	status = ice_aq_list_caps(hw, cbuf, ICE_AQ_MAX_BUF_LEN, &cap_count,
				  ice_aqc_opc_list_func_caps, NULL);
	if (!status)
		ice_parse_func_caps(hw, func_caps, cbuf, cap_count);
	kfree(cbuf);

	return status;
}

 
void ice_set_safe_mode_caps(struct ice_hw *hw)
{
	struct ice_hw_func_caps *func_caps = &hw->func_caps;
	struct ice_hw_dev_caps *dev_caps = &hw->dev_caps;
	struct ice_hw_common_caps cached_caps;
	u32 num_funcs;

	 
	cached_caps = func_caps->common_cap;

	 
	memset(func_caps, 0, sizeof(*func_caps));

#define ICE_RESTORE_FUNC_CAP(name) \
	func_caps->common_cap.name = cached_caps.name

	 
	ICE_RESTORE_FUNC_CAP(valid_functions);
	ICE_RESTORE_FUNC_CAP(txq_first_id);
	ICE_RESTORE_FUNC_CAP(rxq_first_id);
	ICE_RESTORE_FUNC_CAP(msix_vector_first_id);
	ICE_RESTORE_FUNC_CAP(max_mtu);
	ICE_RESTORE_FUNC_CAP(nvm_unified_update);
	ICE_RESTORE_FUNC_CAP(nvm_update_pending_nvm);
	ICE_RESTORE_FUNC_CAP(nvm_update_pending_orom);
	ICE_RESTORE_FUNC_CAP(nvm_update_pending_netlist);

	 
	func_caps->common_cap.num_rxq = 1;
	func_caps->common_cap.num_txq = 1;

	 
	func_caps->common_cap.num_msix_vectors = 2;
	func_caps->guar_num_vsi = 1;

	 
	cached_caps = dev_caps->common_cap;
	num_funcs = dev_caps->num_funcs;

	 
	memset(dev_caps, 0, sizeof(*dev_caps));

#define ICE_RESTORE_DEV_CAP(name) \
	dev_caps->common_cap.name = cached_caps.name

	 
	ICE_RESTORE_DEV_CAP(valid_functions);
	ICE_RESTORE_DEV_CAP(txq_first_id);
	ICE_RESTORE_DEV_CAP(rxq_first_id);
	ICE_RESTORE_DEV_CAP(msix_vector_first_id);
	ICE_RESTORE_DEV_CAP(max_mtu);
	ICE_RESTORE_DEV_CAP(nvm_unified_update);
	ICE_RESTORE_DEV_CAP(nvm_update_pending_nvm);
	ICE_RESTORE_DEV_CAP(nvm_update_pending_orom);
	ICE_RESTORE_DEV_CAP(nvm_update_pending_netlist);
	dev_caps->num_funcs = num_funcs;

	 
	dev_caps->common_cap.num_rxq = num_funcs;
	dev_caps->common_cap.num_txq = num_funcs;

	 
	dev_caps->common_cap.num_msix_vectors = 2 * num_funcs;
}

 
int ice_get_caps(struct ice_hw *hw)
{
	int status;

	status = ice_discover_dev_caps(hw, &hw->dev_caps);
	if (status)
		return status;

	return ice_discover_func_caps(hw, &hw->func_caps);
}

 
int
ice_aq_manage_mac_write(struct ice_hw *hw, const u8 *mac_addr, u8 flags,
			struct ice_sq_cd *cd)
{
	struct ice_aqc_manage_mac_write *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.mac_write;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_manage_mac_write);

	cmd->flags = flags;
	ether_addr_copy(cmd->mac_addr, mac_addr);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
static int ice_aq_clear_pxe_mode(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_pxe_mode);
	desc.params.clear_pxe.rx_cnt = ICE_AQC_CLEAR_PXE_RX_CNT;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 
void ice_clear_pxe_mode(struct ice_hw *hw)
{
	if (ice_check_sq_alive(hw, &hw->adminq))
		ice_aq_clear_pxe_mode(hw);
}

 
int
ice_aq_set_port_params(struct ice_port_info *pi, bool double_vlan,
		       struct ice_sq_cd *cd)

{
	struct ice_aqc_set_port_params *cmd;
	struct ice_hw *hw = pi->hw;
	struct ice_aq_desc desc;
	u16 cmd_flags = 0;

	cmd = &desc.params.set_port_params;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_port_params);
	if (double_vlan)
		cmd_flags |= ICE_AQC_SET_P_PARAMS_DOUBLE_VLAN_ENA;
	cmd->cmd_flags = cpu_to_le16(cmd_flags);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
bool ice_is_100m_speed_supported(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E822C_SGMII:
	case ICE_DEV_ID_E822L_SGMII:
	case ICE_DEV_ID_E823L_1GBE:
	case ICE_DEV_ID_E823C_SGMII:
		return true;
	default:
		return false;
	}
}

 
static u16
ice_get_link_speed_based_on_phy_type(u64 phy_type_low, u64 phy_type_high)
{
	u16 speed_phy_type_high = ICE_AQ_LINK_SPEED_UNKNOWN;
	u16 speed_phy_type_low = ICE_AQ_LINK_SPEED_UNKNOWN;

	switch (phy_type_low) {
	case ICE_PHY_TYPE_LOW_100BASE_TX:
	case ICE_PHY_TYPE_LOW_100M_SGMII:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_100MB;
		break;
	case ICE_PHY_TYPE_LOW_1000BASE_T:
	case ICE_PHY_TYPE_LOW_1000BASE_SX:
	case ICE_PHY_TYPE_LOW_1000BASE_LX:
	case ICE_PHY_TYPE_LOW_1000BASE_KX:
	case ICE_PHY_TYPE_LOW_1G_SGMII:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_1000MB;
		break;
	case ICE_PHY_TYPE_LOW_2500BASE_T:
	case ICE_PHY_TYPE_LOW_2500BASE_X:
	case ICE_PHY_TYPE_LOW_2500BASE_KX:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_2500MB;
		break;
	case ICE_PHY_TYPE_LOW_5GBASE_T:
	case ICE_PHY_TYPE_LOW_5GBASE_KR:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_5GB;
		break;
	case ICE_PHY_TYPE_LOW_10GBASE_T:
	case ICE_PHY_TYPE_LOW_10G_SFI_DA:
	case ICE_PHY_TYPE_LOW_10GBASE_SR:
	case ICE_PHY_TYPE_LOW_10GBASE_LR:
	case ICE_PHY_TYPE_LOW_10GBASE_KR_CR1:
	case ICE_PHY_TYPE_LOW_10G_SFI_AOC_ACC:
	case ICE_PHY_TYPE_LOW_10G_SFI_C2C:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_10GB;
		break;
	case ICE_PHY_TYPE_LOW_25GBASE_T:
	case ICE_PHY_TYPE_LOW_25GBASE_CR:
	case ICE_PHY_TYPE_LOW_25GBASE_CR_S:
	case ICE_PHY_TYPE_LOW_25GBASE_CR1:
	case ICE_PHY_TYPE_LOW_25GBASE_SR:
	case ICE_PHY_TYPE_LOW_25GBASE_LR:
	case ICE_PHY_TYPE_LOW_25GBASE_KR:
	case ICE_PHY_TYPE_LOW_25GBASE_KR_S:
	case ICE_PHY_TYPE_LOW_25GBASE_KR1:
	case ICE_PHY_TYPE_LOW_25G_AUI_AOC_ACC:
	case ICE_PHY_TYPE_LOW_25G_AUI_C2C:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_25GB;
		break;
	case ICE_PHY_TYPE_LOW_40GBASE_CR4:
	case ICE_PHY_TYPE_LOW_40GBASE_SR4:
	case ICE_PHY_TYPE_LOW_40GBASE_LR4:
	case ICE_PHY_TYPE_LOW_40GBASE_KR4:
	case ICE_PHY_TYPE_LOW_40G_XLAUI_AOC_ACC:
	case ICE_PHY_TYPE_LOW_40G_XLAUI:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_40GB;
		break;
	case ICE_PHY_TYPE_LOW_50GBASE_CR2:
	case ICE_PHY_TYPE_LOW_50GBASE_SR2:
	case ICE_PHY_TYPE_LOW_50GBASE_LR2:
	case ICE_PHY_TYPE_LOW_50GBASE_KR2:
	case ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC:
	case ICE_PHY_TYPE_LOW_50G_LAUI2:
	case ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC:
	case ICE_PHY_TYPE_LOW_50G_AUI2:
	case ICE_PHY_TYPE_LOW_50GBASE_CP:
	case ICE_PHY_TYPE_LOW_50GBASE_SR:
	case ICE_PHY_TYPE_LOW_50GBASE_FR:
	case ICE_PHY_TYPE_LOW_50GBASE_LR:
	case ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4:
	case ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC:
	case ICE_PHY_TYPE_LOW_50G_AUI1:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_50GB;
		break;
	case ICE_PHY_TYPE_LOW_100GBASE_CR4:
	case ICE_PHY_TYPE_LOW_100GBASE_SR4:
	case ICE_PHY_TYPE_LOW_100GBASE_LR4:
	case ICE_PHY_TYPE_LOW_100GBASE_KR4:
	case ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC:
	case ICE_PHY_TYPE_LOW_100G_CAUI4:
	case ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC:
	case ICE_PHY_TYPE_LOW_100G_AUI4:
	case ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4:
	case ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4:
	case ICE_PHY_TYPE_LOW_100GBASE_CP2:
	case ICE_PHY_TYPE_LOW_100GBASE_SR2:
	case ICE_PHY_TYPE_LOW_100GBASE_DR:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_100GB;
		break;
	default:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_UNKNOWN;
		break;
	}

	switch (phy_type_high) {
	case ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4:
	case ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC:
	case ICE_PHY_TYPE_HIGH_100G_CAUI2:
	case ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC:
	case ICE_PHY_TYPE_HIGH_100G_AUI2:
		speed_phy_type_high = ICE_AQ_LINK_SPEED_100GB;
		break;
	default:
		speed_phy_type_high = ICE_AQ_LINK_SPEED_UNKNOWN;
		break;
	}

	if (speed_phy_type_low == ICE_AQ_LINK_SPEED_UNKNOWN &&
	    speed_phy_type_high == ICE_AQ_LINK_SPEED_UNKNOWN)
		return ICE_AQ_LINK_SPEED_UNKNOWN;
	else if (speed_phy_type_low != ICE_AQ_LINK_SPEED_UNKNOWN &&
		 speed_phy_type_high != ICE_AQ_LINK_SPEED_UNKNOWN)
		return ICE_AQ_LINK_SPEED_UNKNOWN;
	else if (speed_phy_type_low != ICE_AQ_LINK_SPEED_UNKNOWN &&
		 speed_phy_type_high == ICE_AQ_LINK_SPEED_UNKNOWN)
		return speed_phy_type_low;
	else
		return speed_phy_type_high;
}

 
void
ice_update_phy_type(u64 *phy_type_low, u64 *phy_type_high,
		    u16 link_speeds_bitmap)
{
	u64 pt_high;
	u64 pt_low;
	int index;
	u16 speed;

	 
	for (index = 0; index <= ICE_PHY_TYPE_LOW_MAX_INDEX; index++) {
		pt_low = BIT_ULL(index);
		speed = ice_get_link_speed_based_on_phy_type(pt_low, 0);

		if (link_speeds_bitmap & speed)
			*phy_type_low |= BIT_ULL(index);
	}

	 
	for (index = 0; index <= ICE_PHY_TYPE_HIGH_MAX_INDEX; index++) {
		pt_high = BIT_ULL(index);
		speed = ice_get_link_speed_based_on_phy_type(0, pt_high);

		if (link_speeds_bitmap & speed)
			*phy_type_high |= BIT_ULL(index);
	}
}

 
int
ice_aq_set_phy_cfg(struct ice_hw *hw, struct ice_port_info *pi,
		   struct ice_aqc_set_phy_cfg_data *cfg, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	int status;

	if (!cfg)
		return -EINVAL;

	 
	if (cfg->caps & ~ICE_AQ_PHY_ENA_VALID_MASK) {
		ice_debug(hw, ICE_DBG_PHY, "Invalid bit is set in ice_aqc_set_phy_cfg_data->caps : 0x%x\n",
			  cfg->caps);

		cfg->caps &= ICE_AQ_PHY_ENA_VALID_MASK;
	}

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_phy_cfg);
	desc.params.set_phy.lport_num = pi->lport;
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	ice_debug(hw, ICE_DBG_LINK, "set phy cfg\n");
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_low = 0x%llx\n",
		  (unsigned long long)le64_to_cpu(cfg->phy_type_low));
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_high = 0x%llx\n",
		  (unsigned long long)le64_to_cpu(cfg->phy_type_high));
	ice_debug(hw, ICE_DBG_LINK, "	caps = 0x%x\n", cfg->caps);
	ice_debug(hw, ICE_DBG_LINK, "	low_power_ctrl_an = 0x%x\n",
		  cfg->low_power_ctrl_an);
	ice_debug(hw, ICE_DBG_LINK, "	eee_cap = 0x%x\n", cfg->eee_cap);
	ice_debug(hw, ICE_DBG_LINK, "	eeer_value = 0x%x\n", cfg->eeer_value);
	ice_debug(hw, ICE_DBG_LINK, "	link_fec_opt = 0x%x\n",
		  cfg->link_fec_opt);

	status = ice_aq_send_cmd(hw, &desc, cfg, sizeof(*cfg), cd);
	if (hw->adminq.sq_last_status == ICE_AQ_RC_EMODE)
		status = 0;

	if (!status)
		pi->phy.curr_user_phy_cfg = *cfg;

	return status;
}

 
int ice_update_link_info(struct ice_port_info *pi)
{
	struct ice_link_status *li;
	int status;

	if (!pi)
		return -EINVAL;

	li = &pi->phy.link_info;

	status = ice_aq_get_link_info(pi, true, NULL, NULL);
	if (status)
		return status;

	if (li->link_info & ICE_AQ_MEDIA_AVAILABLE) {
		struct ice_aqc_get_phy_caps_data *pcaps;
		struct ice_hw *hw;

		hw = pi->hw;
		pcaps = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*pcaps),
				     GFP_KERNEL);
		if (!pcaps)
			return -ENOMEM;

		status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_TOPO_CAP_MEDIA,
					     pcaps, NULL);

		devm_kfree(ice_hw_to_dev(hw), pcaps);
	}

	return status;
}

 
static void
ice_cache_phy_user_req(struct ice_port_info *pi,
		       struct ice_phy_cache_mode_data cache_data,
		       enum ice_phy_cache_mode cache_mode)
{
	if (!pi)
		return;

	switch (cache_mode) {
	case ICE_FC_MODE:
		pi->phy.curr_user_fc_req = cache_data.data.curr_user_fc_req;
		break;
	case ICE_SPEED_MODE:
		pi->phy.curr_user_speed_req =
			cache_data.data.curr_user_speed_req;
		break;
	case ICE_FEC_MODE:
		pi->phy.curr_user_fec_req = cache_data.data.curr_user_fec_req;
		break;
	default:
		break;
	}
}

 
enum ice_fc_mode ice_caps_to_fc_mode(u8 caps)
{
	if (caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE &&
	    caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE)
		return ICE_FC_FULL;

	if (caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE)
		return ICE_FC_TX_PAUSE;

	if (caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE)
		return ICE_FC_RX_PAUSE;

	return ICE_FC_NONE;
}

 
enum ice_fec_mode ice_caps_to_fec_mode(u8 caps, u8 fec_options)
{
	if (caps & ICE_AQC_PHY_EN_AUTO_FEC)
		return ICE_FEC_AUTO;

	if (fec_options & (ICE_AQC_PHY_FEC_10G_KR_40G_KR4_EN |
			   ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ |
			   ICE_AQC_PHY_FEC_25G_KR_CLAUSE74_EN |
			   ICE_AQC_PHY_FEC_25G_KR_REQ))
		return ICE_FEC_BASER;

	if (fec_options & (ICE_AQC_PHY_FEC_25G_RS_528_REQ |
			   ICE_AQC_PHY_FEC_25G_RS_544_REQ |
			   ICE_AQC_PHY_FEC_25G_RS_CLAUSE91_EN))
		return ICE_FEC_RS;

	return ICE_FEC_NONE;
}

 
int
ice_cfg_phy_fc(struct ice_port_info *pi, struct ice_aqc_set_phy_cfg_data *cfg,
	       enum ice_fc_mode req_mode)
{
	struct ice_phy_cache_mode_data cache_data;
	u8 pause_mask = 0x0;

	if (!pi || !cfg)
		return -EINVAL;

	switch (req_mode) {
	case ICE_FC_FULL:
		pause_mask |= ICE_AQC_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= ICE_AQC_PHY_EN_RX_LINK_PAUSE;
		break;
	case ICE_FC_RX_PAUSE:
		pause_mask |= ICE_AQC_PHY_EN_RX_LINK_PAUSE;
		break;
	case ICE_FC_TX_PAUSE:
		pause_mask |= ICE_AQC_PHY_EN_TX_LINK_PAUSE;
		break;
	default:
		break;
	}

	 
	cfg->caps &= ~(ICE_AQC_PHY_EN_TX_LINK_PAUSE |
		ICE_AQC_PHY_EN_RX_LINK_PAUSE);

	 
	cfg->caps |= pause_mask;

	 
	cache_data.data.curr_user_fc_req = req_mode;
	ice_cache_phy_user_req(pi, cache_data, ICE_FC_MODE);

	return 0;
}

 
int
ice_set_fc(struct ice_port_info *pi, u8 *aq_failures, bool ena_auto_link_update)
{
	struct ice_aqc_set_phy_cfg_data cfg = { 0 };
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_hw *hw;
	int status;

	if (!pi || !aq_failures)
		return -EINVAL;

	*aq_failures = 0;
	hw = pi->hw;

	pcaps = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps)
		return -ENOMEM;

	 
	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     pcaps, NULL);
	if (status) {
		*aq_failures = ICE_SET_FC_AQ_FAIL_GET;
		goto out;
	}

	ice_copy_phy_caps_to_cfg(pi, pcaps, &cfg);

	 
	status = ice_cfg_phy_fc(pi, &cfg, pi->fc.req_mode);
	if (status)
		goto out;

	 
	if (cfg.caps != pcaps->caps) {
		int retry_count, retry_max = 10;

		 
		if (ena_auto_link_update)
			cfg.caps |= ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;

		status = ice_aq_set_phy_cfg(hw, pi, &cfg, NULL);
		if (status) {
			*aq_failures = ICE_SET_FC_AQ_FAIL_SET;
			goto out;
		}

		 
		for (retry_count = 0; retry_count < retry_max; retry_count++) {
			status = ice_update_link_info(pi);

			if (!status)
				break;

			mdelay(100);
		}

		if (status)
			*aq_failures = ICE_SET_FC_AQ_FAIL_UPDATE;
	}

out:
	devm_kfree(ice_hw_to_dev(hw), pcaps);
	return status;
}

 
bool
ice_phy_caps_equals_cfg(struct ice_aqc_get_phy_caps_data *phy_caps,
			struct ice_aqc_set_phy_cfg_data *phy_cfg)
{
	u8 caps_mask, cfg_mask;

	if (!phy_caps || !phy_cfg)
		return false;

	 
	caps_mask = ICE_AQC_PHY_CAPS_MASK & ~(ICE_AQC_PHY_AN_MODE |
					      ICE_AQC_GET_PHY_EN_MOD_QUAL);
	cfg_mask = ICE_AQ_PHY_ENA_VALID_MASK & ~ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;

	if (phy_caps->phy_type_low != phy_cfg->phy_type_low ||
	    phy_caps->phy_type_high != phy_cfg->phy_type_high ||
	    ((phy_caps->caps & caps_mask) != (phy_cfg->caps & cfg_mask)) ||
	    phy_caps->low_power_ctrl_an != phy_cfg->low_power_ctrl_an ||
	    phy_caps->eee_cap != phy_cfg->eee_cap ||
	    phy_caps->eeer_value != phy_cfg->eeer_value ||
	    phy_caps->link_fec_options != phy_cfg->link_fec_opt)
		return false;

	return true;
}

 
void
ice_copy_phy_caps_to_cfg(struct ice_port_info *pi,
			 struct ice_aqc_get_phy_caps_data *caps,
			 struct ice_aqc_set_phy_cfg_data *cfg)
{
	if (!pi || !caps || !cfg)
		return;

	memset(cfg, 0, sizeof(*cfg));
	cfg->phy_type_low = caps->phy_type_low;
	cfg->phy_type_high = caps->phy_type_high;
	cfg->caps = caps->caps;
	cfg->low_power_ctrl_an = caps->low_power_ctrl_an;
	cfg->eee_cap = caps->eee_cap;
	cfg->eeer_value = caps->eeer_value;
	cfg->link_fec_opt = caps->link_fec_options;
	cfg->module_compliance_enforcement =
		caps->module_compliance_enforcement;
}

 
int
ice_cfg_phy_fec(struct ice_port_info *pi, struct ice_aqc_set_phy_cfg_data *cfg,
		enum ice_fec_mode fec)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_hw *hw;
	int status;

	if (!pi || !cfg)
		return -EINVAL;

	hw = pi->hw;

	pcaps = kzalloc(sizeof(*pcaps), GFP_KERNEL);
	if (!pcaps)
		return -ENOMEM;

	status = ice_aq_get_phy_caps(pi, false,
				     (ice_fw_supports_report_dflt_cfg(hw) ?
				      ICE_AQC_REPORT_DFLT_CFG :
				      ICE_AQC_REPORT_TOPO_CAP_MEDIA), pcaps, NULL);
	if (status)
		goto out;

	cfg->caps |= pcaps->caps & ICE_AQC_PHY_EN_AUTO_FEC;
	cfg->link_fec_opt = pcaps->link_fec_options;

	switch (fec) {
	case ICE_FEC_BASER:
		 
		cfg->link_fec_opt &= ICE_AQC_PHY_FEC_10G_KR_40G_KR4_EN |
			ICE_AQC_PHY_FEC_25G_KR_CLAUSE74_EN;
		cfg->link_fec_opt |= ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ |
			ICE_AQC_PHY_FEC_25G_KR_REQ;
		break;
	case ICE_FEC_RS:
		 
		cfg->link_fec_opt &= ICE_AQC_PHY_FEC_25G_RS_CLAUSE91_EN;
		cfg->link_fec_opt |= ICE_AQC_PHY_FEC_25G_RS_528_REQ |
			ICE_AQC_PHY_FEC_25G_RS_544_REQ;
		break;
	case ICE_FEC_NONE:
		 
		cfg->link_fec_opt &= ~ICE_AQC_PHY_FEC_MASK;
		break;
	case ICE_FEC_AUTO:
		 
		cfg->caps &= ICE_AQC_PHY_CAPS_MASK;
		cfg->link_fec_opt |= pcaps->link_fec_options;
		break;
	default:
		status = -EINVAL;
		break;
	}

	if (fec == ICE_FEC_AUTO && ice_fw_supports_link_override(hw) &&
	    !ice_fw_supports_report_dflt_cfg(hw)) {
		struct ice_link_default_override_tlv tlv = { 0 };

		status = ice_get_link_default_override(&tlv, pi);
		if (status)
			goto out;

		if (!(tlv.options & ICE_LINK_OVERRIDE_STRICT_MODE) &&
		    (tlv.options & ICE_LINK_OVERRIDE_EN))
			cfg->link_fec_opt = tlv.fec_options;
	}

out:
	kfree(pcaps);

	return status;
}

 
int ice_get_link_status(struct ice_port_info *pi, bool *link_up)
{
	struct ice_phy_info *phy_info;
	int status = 0;

	if (!pi || !link_up)
		return -EINVAL;

	phy_info = &pi->phy;

	if (phy_info->get_link_info) {
		status = ice_update_link_info(pi);

		if (status)
			ice_debug(pi->hw, ICE_DBG_LINK, "get link status error, status = %d\n",
				  status);
	}

	*link_up = phy_info->link_info.link_info & ICE_AQ_LINK_UP;

	return status;
}

 
int
ice_aq_set_link_restart_an(struct ice_port_info *pi, bool ena_link,
			   struct ice_sq_cd *cd)
{
	struct ice_aqc_restart_an *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.restart_an;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_restart_an);

	cmd->cmd_flags = ICE_AQC_RESTART_AN_LINK_RESTART;
	cmd->lport_num = pi->lport;
	if (ena_link)
		cmd->cmd_flags |= ICE_AQC_RESTART_AN_LINK_ENABLE;
	else
		cmd->cmd_flags &= ~ICE_AQC_RESTART_AN_LINK_ENABLE;

	return ice_aq_send_cmd(pi->hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_set_event_mask(struct ice_hw *hw, u8 port_num, u16 mask,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_set_event_mask *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_event_mask;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_event_mask);

	cmd->lport_num = port_num;

	cmd->event_mask = cpu_to_le16(mask);
	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_set_mac_loopback(struct ice_hw *hw, bool ena_lpbk, struct ice_sq_cd *cd)
{
	struct ice_aqc_set_mac_lb *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_mac_lb;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_mac_lb);
	if (ena_lpbk)
		cmd->lb_mode = ICE_AQ_MAC_LB_EN;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_set_port_id_led(struct ice_port_info *pi, bool is_orig_mode,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_set_port_id_led *cmd;
	struct ice_hw *hw = pi->hw;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_port_id_led;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_port_id_led);

	if (is_orig_mode)
		cmd->ident_mode = ICE_AQC_PORT_IDENT_LED_ORIG;
	else
		cmd->ident_mode = ICE_AQC_PORT_IDENT_LED_BLINK;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_get_port_options(struct ice_hw *hw,
			struct ice_aqc_get_port_options_elem *options,
			u8 *option_count, u8 lport, bool lport_valid,
			u8 *active_option_idx, bool *active_option_valid,
			u8 *pending_option_idx, bool *pending_option_valid)
{
	struct ice_aqc_get_port_options *cmd;
	struct ice_aq_desc desc;
	int status;
	u8 i;

	 
	if (*option_count < ICE_AQC_PORT_OPT_COUNT_M)
		return -EINVAL;

	cmd = &desc.params.get_port_options;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_port_options);

	if (lport_valid)
		cmd->lport_num = lport;
	cmd->lport_num_valid = lport_valid;

	status = ice_aq_send_cmd(hw, &desc, options,
				 *option_count * sizeof(*options), NULL);
	if (status)
		return status;

	 
	*option_count = FIELD_GET(ICE_AQC_PORT_OPT_COUNT_M,
				  cmd->port_options_count);
	ice_debug(hw, ICE_DBG_PHY, "options: %x\n", *option_count);
	*active_option_valid = FIELD_GET(ICE_AQC_PORT_OPT_VALID,
					 cmd->port_options);
	if (*active_option_valid) {
		*active_option_idx = FIELD_GET(ICE_AQC_PORT_OPT_ACTIVE_M,
					       cmd->port_options);
		if (*active_option_idx > (*option_count - 1))
			return -EIO;
		ice_debug(hw, ICE_DBG_PHY, "active idx: %x\n",
			  *active_option_idx);
	}

	*pending_option_valid = FIELD_GET(ICE_AQC_PENDING_PORT_OPT_VALID,
					  cmd->pending_port_option_status);
	if (*pending_option_valid) {
		*pending_option_idx = FIELD_GET(ICE_AQC_PENDING_PORT_OPT_IDX_M,
						cmd->pending_port_option_status);
		if (*pending_option_idx > (*option_count - 1))
			return -EIO;
		ice_debug(hw, ICE_DBG_PHY, "pending idx: %x\n",
			  *pending_option_idx);
	}

	 
	for (i = 0; i < *option_count; i++) {
		options[i].pmd = FIELD_GET(ICE_AQC_PORT_OPT_PMD_COUNT_M,
					   options[i].pmd);
		options[i].max_lane_speed = FIELD_GET(ICE_AQC_PORT_OPT_MAX_LANE_M,
						      options[i].max_lane_speed);
		ice_debug(hw, ICE_DBG_PHY, "pmds: %x max speed: %x\n",
			  options[i].pmd, options[i].max_lane_speed);
	}

	return 0;
}

 
int
ice_aq_set_port_option(struct ice_hw *hw, u8 lport, u8 lport_valid,
		       u8 new_option)
{
	struct ice_aqc_set_port_option *cmd;
	struct ice_aq_desc desc;

	if (new_option > ICE_AQC_PORT_OPT_COUNT_M)
		return -EINVAL;

	cmd = &desc.params.set_port_option;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_port_option);

	if (lport_valid)
		cmd->lport_num = lport;

	cmd->lport_num_valid = lport_valid;
	cmd->selected_port_option = new_option;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 
int
ice_aq_sff_eeprom(struct ice_hw *hw, u16 lport, u8 bus_addr,
		  u16 mem_addr, u8 page, u8 set_page, u8 *data, u8 length,
		  bool write, struct ice_sq_cd *cd)
{
	struct ice_aqc_sff_eeprom *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!data || (mem_addr & 0xff00))
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_sff_eeprom);
	cmd = &desc.params.read_write_sff_param;
	desc.flags = cpu_to_le16(ICE_AQ_FLAG_RD);
	cmd->lport_num = (u8)(lport & 0xff);
	cmd->lport_num_valid = (u8)((lport >> 8) & 0x01);
	cmd->i2c_bus_addr = cpu_to_le16(((bus_addr >> 1) &
					 ICE_AQC_SFF_I2CBUS_7BIT_M) |
					((set_page <<
					  ICE_AQC_SFF_SET_EEPROM_PAGE_S) &
					 ICE_AQC_SFF_SET_EEPROM_PAGE_M));
	cmd->i2c_mem_addr = cpu_to_le16(mem_addr & 0xff);
	cmd->eeprom_page = cpu_to_le16((u16)page << ICE_AQC_SFF_EEPROM_PAGE_S);
	if (write)
		cmd->i2c_bus_addr |= cpu_to_le16(ICE_AQC_SFF_IS_WRITE);

	status = ice_aq_send_cmd(hw, &desc, data, length, cd);
	return status;
}

static enum ice_lut_size ice_lut_type_to_size(enum ice_lut_type type)
{
	switch (type) {
	case ICE_LUT_VSI:
		return ICE_LUT_VSI_SIZE;
	case ICE_LUT_GLOBAL:
		return ICE_LUT_GLOBAL_SIZE;
	case ICE_LUT_PF:
		return ICE_LUT_PF_SIZE;
	}
	WARN_ONCE(1, "incorrect type passed");
	return ICE_LUT_VSI_SIZE;
}

static enum ice_aqc_lut_flags ice_lut_size_to_flag(enum ice_lut_size size)
{
	switch (size) {
	case ICE_LUT_VSI_SIZE:
		return ICE_AQC_LUT_SIZE_SMALL;
	case ICE_LUT_GLOBAL_SIZE:
		return ICE_AQC_LUT_SIZE_512;
	case ICE_LUT_PF_SIZE:
		return ICE_AQC_LUT_SIZE_2K;
	}
	WARN_ONCE(1, "incorrect size passed");
	return 0;
}

 
static int
__ice_aq_get_set_rss_lut(struct ice_hw *hw,
			 struct ice_aq_get_set_rss_lut_params *params, bool set)
{
	u16 opcode, vsi_id, vsi_handle = params->vsi_handle, glob_lut_idx = 0;
	enum ice_lut_type lut_type = params->lut_type;
	struct ice_aqc_get_set_rss_lut *desc_params;
	enum ice_aqc_lut_flags flags;
	enum ice_lut_size lut_size;
	struct ice_aq_desc desc;
	u8 *lut = params->lut;


	if (!lut || !ice_is_vsi_valid(hw, vsi_handle))
		return -EINVAL;

	lut_size = ice_lut_type_to_size(lut_type);
	if (lut_size > params->lut_size)
		return -EINVAL;
	else if (set && lut_size != params->lut_size)
		return -EINVAL;

	opcode = set ? ice_aqc_opc_set_rss_lut : ice_aqc_opc_get_rss_lut;
	ice_fill_dflt_direct_cmd_desc(&desc, opcode);
	if (set)
		desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	desc_params = &desc.params.get_set_rss_lut;
	vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);
	desc_params->vsi_id = cpu_to_le16(vsi_id | ICE_AQC_RSS_VSI_VALID);

	if (lut_type == ICE_LUT_GLOBAL)
		glob_lut_idx = FIELD_PREP(ICE_AQC_LUT_GLOBAL_IDX,
					  params->global_lut_id);

	flags = lut_type | glob_lut_idx | ice_lut_size_to_flag(lut_size);
	desc_params->flags = cpu_to_le16(flags);

	return ice_aq_send_cmd(hw, &desc, lut, lut_size, NULL);
}

 
int
ice_aq_get_rss_lut(struct ice_hw *hw, struct ice_aq_get_set_rss_lut_params *get_params)
{
	return __ice_aq_get_set_rss_lut(hw, get_params, false);
}

 
int
ice_aq_set_rss_lut(struct ice_hw *hw, struct ice_aq_get_set_rss_lut_params *set_params)
{
	return __ice_aq_get_set_rss_lut(hw, set_params, true);
}

 
static int
__ice_aq_get_set_rss_key(struct ice_hw *hw, u16 vsi_id,
			 struct ice_aqc_get_set_rss_keys *key, bool set)
{
	struct ice_aqc_get_set_rss_key *desc_params;
	u16 key_size = sizeof(*key);
	struct ice_aq_desc desc;

	if (set) {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_rss_key);
		desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);
	} else {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_rss_key);
	}

	desc_params = &desc.params.get_set_rss_key;
	desc_params->vsi_id = cpu_to_le16(vsi_id | ICE_AQC_RSS_VSI_VALID);

	return ice_aq_send_cmd(hw, &desc, key, key_size, NULL);
}

 
int
ice_aq_get_rss_key(struct ice_hw *hw, u16 vsi_handle,
		   struct ice_aqc_get_set_rss_keys *key)
{
	if (!ice_is_vsi_valid(hw, vsi_handle) || !key)
		return -EINVAL;

	return __ice_aq_get_set_rss_key(hw, ice_get_hw_vsi_num(hw, vsi_handle),
					key, false);
}

 
int
ice_aq_set_rss_key(struct ice_hw *hw, u16 vsi_handle,
		   struct ice_aqc_get_set_rss_keys *keys)
{
	if (!ice_is_vsi_valid(hw, vsi_handle) || !keys)
		return -EINVAL;

	return __ice_aq_get_set_rss_key(hw, ice_get_hw_vsi_num(hw, vsi_handle),
					keys, true);
}

 
static int
ice_aq_add_lan_txq(struct ice_hw *hw, u8 num_qgrps,
		   struct ice_aqc_add_tx_qgrp *qg_list, u16 buf_size,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_add_tx_qgrp *list;
	struct ice_aqc_add_txqs *cmd;
	struct ice_aq_desc desc;
	u16 i, sum_size = 0;

	cmd = &desc.params.add_txqs;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_txqs);

	if (!qg_list)
		return -EINVAL;

	if (num_qgrps > ICE_LAN_TXQ_MAX_QGRPS)
		return -EINVAL;

	for (i = 0, list = qg_list; i < num_qgrps; i++) {
		sum_size += struct_size(list, txqs, list->num_txqs);
		list = (struct ice_aqc_add_tx_qgrp *)(list->txqs +
						      list->num_txqs);
	}

	if (buf_size != sum_size)
		return -EINVAL;

	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	cmd->num_qgrps = num_qgrps;

	return ice_aq_send_cmd(hw, &desc, qg_list, buf_size, cd);
}

 
static int
ice_aq_dis_lan_txq(struct ice_hw *hw, u8 num_qgrps,
		   struct ice_aqc_dis_txq_item *qg_list, u16 buf_size,
		   enum ice_disq_rst_src rst_src, u16 vmvf_num,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_dis_txq_item *item;
	struct ice_aqc_dis_txqs *cmd;
	struct ice_aq_desc desc;
	u16 i, sz = 0;
	int status;

	cmd = &desc.params.dis_txqs;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_dis_txqs);

	 
	if (!qg_list && !rst_src)
		return -EINVAL;

	if (num_qgrps > ICE_LAN_TXQ_MAX_QGRPS)
		return -EINVAL;

	cmd->num_entries = num_qgrps;

	cmd->vmvf_and_timeout = cpu_to_le16((5 << ICE_AQC_Q_DIS_TIMEOUT_S) &
					    ICE_AQC_Q_DIS_TIMEOUT_M);

	switch (rst_src) {
	case ICE_VM_RESET:
		cmd->cmd_type = ICE_AQC_Q_DIS_CMD_VM_RESET;
		cmd->vmvf_and_timeout |=
			cpu_to_le16(vmvf_num & ICE_AQC_Q_DIS_VMVF_NUM_M);
		break;
	case ICE_VF_RESET:
		cmd->cmd_type = ICE_AQC_Q_DIS_CMD_VF_RESET;
		 
		cmd->vmvf_and_timeout |=
			cpu_to_le16((vmvf_num + hw->func_caps.vf_base_id) &
				    ICE_AQC_Q_DIS_VMVF_NUM_M);
		break;
	case ICE_NO_RESET:
	default:
		break;
	}

	 
	cmd->cmd_type |= ICE_AQC_Q_DIS_CMD_FLUSH_PIPE;
	 
	if (!qg_list)
		goto do_aq;

	 
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	for (i = 0, item = qg_list; i < num_qgrps; i++) {
		u16 item_size = struct_size(item, q_id, item->num_qs);

		 
		if ((item->num_qs % 2) == 0)
			item_size += 2;

		sz += item_size;

		item = (struct ice_aqc_dis_txq_item *)((u8 *)item + item_size);
	}

	if (buf_size != sz)
		return -EINVAL;

do_aq:
	status = ice_aq_send_cmd(hw, &desc, qg_list, buf_size, cd);
	if (status) {
		if (!qg_list)
			ice_debug(hw, ICE_DBG_SCHED, "VM%d disable failed %d\n",
				  vmvf_num, hw->adminq.sq_last_status);
		else
			ice_debug(hw, ICE_DBG_SCHED, "disable queue %d failed %d\n",
				  le16_to_cpu(qg_list[0].q_id[0]),
				  hw->adminq.sq_last_status);
	}
	return status;
}

 
int
ice_aq_cfg_lan_txq(struct ice_hw *hw, struct ice_aqc_cfg_txqs_buf *buf,
		   u16 buf_size, u16 num_qs, u8 oldport, u8 newport,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_cfg_txqs *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.cfg_txqs;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_cfg_txqs);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	if (!buf)
		return -EINVAL;

	cmd->cmd_type = ICE_AQC_Q_CFG_TC_CHNG;
	cmd->num_qs = num_qs;
	cmd->port_num_chng = (oldport & ICE_AQC_Q_CFG_SRC_PRT_M);
	cmd->port_num_chng |= (newport << ICE_AQC_Q_CFG_DST_PRT_S) &
			      ICE_AQC_Q_CFG_DST_PRT_M;
	cmd->time_out = (5 << ICE_AQC_Q_CFG_TIMEOUT_S) &
			ICE_AQC_Q_CFG_TIMEOUT_M;
	cmd->blocked_cgds = 0;

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (status)
		ice_debug(hw, ICE_DBG_SCHED, "Failed to reconfigure nodes %d\n",
			  hw->adminq.sq_last_status);
	return status;
}

 
static int
ice_aq_add_rdma_qsets(struct ice_hw *hw, u8 num_qset_grps,
		      struct ice_aqc_add_rdma_qset_data *qset_list,
		      u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aqc_add_rdma_qset_data *list;
	struct ice_aqc_add_rdma_qset *cmd;
	struct ice_aq_desc desc;
	u16 i, sum_size = 0;

	cmd = &desc.params.add_rdma_qset;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_rdma_qset);

	if (num_qset_grps > ICE_LAN_TXQ_MAX_QGRPS)
		return -EINVAL;

	for (i = 0, list = qset_list; i < num_qset_grps; i++) {
		u16 num_qsets = le16_to_cpu(list->num_qsets);

		sum_size += struct_size(list, rdma_qsets, num_qsets);
		list = (struct ice_aqc_add_rdma_qset_data *)(list->rdma_qsets +
							     num_qsets);
	}

	if (buf_size != sum_size)
		return -EINVAL;

	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	cmd->num_qset_grps = num_qset_grps;

	return ice_aq_send_cmd(hw, &desc, qset_list, buf_size, cd);
}

 

 
static void
ice_write_byte(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u8 src_byte, dest_byte, mask;
	u8 *from, *dest;
	u16 shift_width;

	 
	from = src_ctx + ce_info->offset;

	 
	shift_width = ce_info->lsb % 8;
	mask = (u8)(BIT(ce_info->width) - 1);

	src_byte = *from;
	src_byte &= mask;

	 
	mask <<= shift_width;
	src_byte <<= shift_width;

	 
	dest = dest_ctx + (ce_info->lsb / 8);

	memcpy(&dest_byte, dest, sizeof(dest_byte));

	dest_byte &= ~mask;	 
	dest_byte |= src_byte;	 

	 
	memcpy(dest, &dest_byte, sizeof(dest_byte));
}

 
static void
ice_write_word(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u16 src_word, mask;
	__le16 dest_word;
	u8 *from, *dest;
	u16 shift_width;

	 
	from = src_ctx + ce_info->offset;

	 
	shift_width = ce_info->lsb % 8;
	mask = BIT(ce_info->width) - 1;

	 
	src_word = *(u16 *)from;
	src_word &= mask;

	 
	mask <<= shift_width;
	src_word <<= shift_width;

	 
	dest = dest_ctx + (ce_info->lsb / 8);

	memcpy(&dest_word, dest, sizeof(dest_word));

	dest_word &= ~(cpu_to_le16(mask));	 
	dest_word |= cpu_to_le16(src_word);	 

	 
	memcpy(dest, &dest_word, sizeof(dest_word));
}

 
static void
ice_write_dword(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u32 src_dword, mask;
	__le32 dest_dword;
	u8 *from, *dest;
	u16 shift_width;

	 
	from = src_ctx + ce_info->offset;

	 
	shift_width = ce_info->lsb % 8;

	 
	if (ce_info->width < 32)
		mask = BIT(ce_info->width) - 1;
	else
		mask = (u32)~0;

	 
	src_dword = *(u32 *)from;
	src_dword &= mask;

	 
	mask <<= shift_width;
	src_dword <<= shift_width;

	 
	dest = dest_ctx + (ce_info->lsb / 8);

	memcpy(&dest_dword, dest, sizeof(dest_dword));

	dest_dword &= ~(cpu_to_le32(mask));	 
	dest_dword |= cpu_to_le32(src_dword);	 

	 
	memcpy(dest, &dest_dword, sizeof(dest_dword));
}

 
static void
ice_write_qword(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u64 src_qword, mask;
	__le64 dest_qword;
	u8 *from, *dest;
	u16 shift_width;

	 
	from = src_ctx + ce_info->offset;

	 
	shift_width = ce_info->lsb % 8;

	 
	if (ce_info->width < 64)
		mask = BIT_ULL(ce_info->width) - 1;
	else
		mask = (u64)~0;

	 
	src_qword = *(u64 *)from;
	src_qword &= mask;

	 
	mask <<= shift_width;
	src_qword <<= shift_width;

	 
	dest = dest_ctx + (ce_info->lsb / 8);

	memcpy(&dest_qword, dest, sizeof(dest_qword));

	dest_qword &= ~(cpu_to_le64(mask));	 
	dest_qword |= cpu_to_le64(src_qword);	 

	 
	memcpy(dest, &dest_qword, sizeof(dest_qword));
}

 
int
ice_set_ctx(struct ice_hw *hw, u8 *src_ctx, u8 *dest_ctx,
	    const struct ice_ctx_ele *ce_info)
{
	int f;

	for (f = 0; ce_info[f].width; f++) {
		 
		if (ce_info[f].width > (ce_info[f].size_of * BITS_PER_BYTE)) {
			ice_debug(hw, ICE_DBG_QCTX, "Field %d width of %d bits larger than size of %d byte(s) ... skipping write\n",
				  f, ce_info[f].width, ce_info[f].size_of);
			continue;
		}
		switch (ce_info[f].size_of) {
		case sizeof(u8):
			ice_write_byte(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case sizeof(u16):
			ice_write_word(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case sizeof(u32):
			ice_write_dword(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case sizeof(u64):
			ice_write_qword(src_ctx, dest_ctx, &ce_info[f]);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

 
struct ice_q_ctx *
ice_get_lan_q_ctx(struct ice_hw *hw, u16 vsi_handle, u8 tc, u16 q_handle)
{
	struct ice_vsi_ctx *vsi;
	struct ice_q_ctx *q_ctx;

	vsi = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi)
		return NULL;
	if (q_handle >= vsi->num_lan_q_entries[tc])
		return NULL;
	if (!vsi->lan_q_ctx[tc])
		return NULL;
	q_ctx = vsi->lan_q_ctx[tc];
	return &q_ctx[q_handle];
}

 
int
ice_ena_vsi_txq(struct ice_port_info *pi, u16 vsi_handle, u8 tc, u16 q_handle,
		u8 num_qgrps, struct ice_aqc_add_tx_qgrp *buf, u16 buf_size,
		struct ice_sq_cd *cd)
{
	struct ice_aqc_txsched_elem_data node = { 0 };
	struct ice_sched_node *parent;
	struct ice_q_ctx *q_ctx;
	struct ice_hw *hw;
	int status;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return -EIO;

	if (num_qgrps > 1 || buf->num_txqs > 1)
		return -ENOSPC;

	hw = pi->hw;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return -EINVAL;

	mutex_lock(&pi->sched_lock);

	q_ctx = ice_get_lan_q_ctx(hw, vsi_handle, tc, q_handle);
	if (!q_ctx) {
		ice_debug(hw, ICE_DBG_SCHED, "Enaq: invalid queue handle %d\n",
			  q_handle);
		status = -EINVAL;
		goto ena_txq_exit;
	}

	 
	parent = ice_sched_get_free_qparent(pi, vsi_handle, tc,
					    ICE_SCHED_NODE_OWNER_LAN);
	if (!parent) {
		status = -EINVAL;
		goto ena_txq_exit;
	}

	buf->parent_teid = parent->info.node_teid;
	node.parent_teid = parent->info.node_teid;
	 
	buf->txqs[0].info.valid_sections =
		ICE_AQC_ELEM_VALID_GENERIC | ICE_AQC_ELEM_VALID_CIR |
		ICE_AQC_ELEM_VALID_EIR;
	buf->txqs[0].info.generic = 0;
	buf->txqs[0].info.cir_bw.bw_profile_idx =
		cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
	buf->txqs[0].info.cir_bw.bw_alloc =
		cpu_to_le16(ICE_SCHED_DFLT_BW_WT);
	buf->txqs[0].info.eir_bw.bw_profile_idx =
		cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
	buf->txqs[0].info.eir_bw.bw_alloc =
		cpu_to_le16(ICE_SCHED_DFLT_BW_WT);

	 
	status = ice_aq_add_lan_txq(hw, num_qgrps, buf, buf_size, cd);
	if (status) {
		ice_debug(hw, ICE_DBG_SCHED, "enable queue %d failed %d\n",
			  le16_to_cpu(buf->txqs[0].txq_id),
			  hw->adminq.sq_last_status);
		goto ena_txq_exit;
	}

	node.node_teid = buf->txqs[0].q_teid;
	node.data.elem_type = ICE_AQC_ELEM_TYPE_LEAF;
	q_ctx->q_handle = q_handle;
	q_ctx->q_teid = le32_to_cpu(node.node_teid);

	 
	status = ice_sched_add_node(pi, hw->num_tx_sched_layers - 1, &node, NULL);
	if (!status)
		status = ice_sched_replay_q_bw(pi, q_ctx);

ena_txq_exit:
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
int
ice_dis_vsi_txq(struct ice_port_info *pi, u16 vsi_handle, u8 tc, u8 num_queues,
		u16 *q_handles, u16 *q_ids, u32 *q_teids,
		enum ice_disq_rst_src rst_src, u16 vmvf_num,
		struct ice_sq_cd *cd)
{
	struct ice_aqc_dis_txq_item *qg_list;
	struct ice_q_ctx *q_ctx;
	int status = -ENOENT;
	struct ice_hw *hw;
	u16 i, buf_size;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return -EIO;

	hw = pi->hw;

	if (!num_queues) {
		 
		if (rst_src)
			return ice_aq_dis_lan_txq(hw, 0, NULL, 0, rst_src,
						  vmvf_num, NULL);
		return -EIO;
	}

	buf_size = struct_size(qg_list, q_id, 1);
	qg_list = kzalloc(buf_size, GFP_KERNEL);
	if (!qg_list)
		return -ENOMEM;

	mutex_lock(&pi->sched_lock);

	for (i = 0; i < num_queues; i++) {
		struct ice_sched_node *node;

		node = ice_sched_find_node_by_teid(pi->root, q_teids[i]);
		if (!node)
			continue;
		q_ctx = ice_get_lan_q_ctx(hw, vsi_handle, tc, q_handles[i]);
		if (!q_ctx) {
			ice_debug(hw, ICE_DBG_SCHED, "invalid queue handle%d\n",
				  q_handles[i]);
			continue;
		}
		if (q_ctx->q_handle != q_handles[i]) {
			ice_debug(hw, ICE_DBG_SCHED, "Err:handles %d %d\n",
				  q_ctx->q_handle, q_handles[i]);
			continue;
		}
		qg_list->parent_teid = node->info.parent_teid;
		qg_list->num_qs = 1;
		qg_list->q_id[0] = cpu_to_le16(q_ids[i]);
		status = ice_aq_dis_lan_txq(hw, 1, qg_list, buf_size, rst_src,
					    vmvf_num, cd);

		if (status)
			break;
		ice_free_sched_node(pi, node);
		q_ctx->q_handle = ICE_INVAL_Q_HANDLE;
		q_ctx->q_teid = ICE_INVAL_TEID;
	}
	mutex_unlock(&pi->sched_lock);
	kfree(qg_list);
	return status;
}

 
static int
ice_cfg_vsi_qs(struct ice_port_info *pi, u16 vsi_handle, u8 tc_bitmap,
	       u16 *maxqs, u8 owner)
{
	int status = 0;
	u8 i;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return -EIO;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return -EINVAL;

	mutex_lock(&pi->sched_lock);

	ice_for_each_traffic_class(i) {
		 
		if (!ice_sched_get_tc_node(pi, i))
			continue;

		status = ice_sched_cfg_vsi(pi, vsi_handle, i, maxqs[i], owner,
					   ice_is_tc_ena(tc_bitmap, i));
		if (status)
			break;
	}

	mutex_unlock(&pi->sched_lock);
	return status;
}

 
int
ice_cfg_vsi_lan(struct ice_port_info *pi, u16 vsi_handle, u8 tc_bitmap,
		u16 *max_lanqs)
{
	return ice_cfg_vsi_qs(pi, vsi_handle, tc_bitmap, max_lanqs,
			      ICE_SCHED_NODE_OWNER_LAN);
}

 
int
ice_cfg_vsi_rdma(struct ice_port_info *pi, u16 vsi_handle, u16 tc_bitmap,
		 u16 *max_rdmaqs)
{
	return ice_cfg_vsi_qs(pi, vsi_handle, tc_bitmap, max_rdmaqs,
			      ICE_SCHED_NODE_OWNER_RDMA);
}

 
int
ice_ena_vsi_rdma_qset(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		      u16 *rdma_qset, u16 num_qsets, u32 *qset_teid)
{
	struct ice_aqc_txsched_elem_data node = { 0 };
	struct ice_aqc_add_rdma_qset_data *buf;
	struct ice_sched_node *parent;
	struct ice_hw *hw;
	u16 i, buf_size;
	int ret;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return -EIO;
	hw = pi->hw;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return -EINVAL;

	buf_size = struct_size(buf, rdma_qsets, num_qsets);
	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	mutex_lock(&pi->sched_lock);

	parent = ice_sched_get_free_qparent(pi, vsi_handle, tc,
					    ICE_SCHED_NODE_OWNER_RDMA);
	if (!parent) {
		ret = -EINVAL;
		goto rdma_error_exit;
	}
	buf->parent_teid = parent->info.node_teid;
	node.parent_teid = parent->info.node_teid;

	buf->num_qsets = cpu_to_le16(num_qsets);
	for (i = 0; i < num_qsets; i++) {
		buf->rdma_qsets[i].tx_qset_id = cpu_to_le16(rdma_qset[i]);
		buf->rdma_qsets[i].info.valid_sections =
			ICE_AQC_ELEM_VALID_GENERIC | ICE_AQC_ELEM_VALID_CIR |
			ICE_AQC_ELEM_VALID_EIR;
		buf->rdma_qsets[i].info.generic = 0;
		buf->rdma_qsets[i].info.cir_bw.bw_profile_idx =
			cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->rdma_qsets[i].info.cir_bw.bw_alloc =
			cpu_to_le16(ICE_SCHED_DFLT_BW_WT);
		buf->rdma_qsets[i].info.eir_bw.bw_profile_idx =
			cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->rdma_qsets[i].info.eir_bw.bw_alloc =
			cpu_to_le16(ICE_SCHED_DFLT_BW_WT);
	}
	ret = ice_aq_add_rdma_qsets(hw, 1, buf, buf_size, NULL);
	if (ret) {
		ice_debug(hw, ICE_DBG_RDMA, "add RDMA qset failed\n");
		goto rdma_error_exit;
	}
	node.data.elem_type = ICE_AQC_ELEM_TYPE_LEAF;
	for (i = 0; i < num_qsets; i++) {
		node.node_teid = buf->rdma_qsets[i].qset_teid;
		ret = ice_sched_add_node(pi, hw->num_tx_sched_layers - 1,
					 &node, NULL);
		if (ret)
			break;
		qset_teid[i] = le32_to_cpu(node.node_teid);
	}
rdma_error_exit:
	mutex_unlock(&pi->sched_lock);
	kfree(buf);
	return ret;
}

 
int
ice_dis_vsi_rdma_qset(struct ice_port_info *pi, u16 count, u32 *qset_teid,
		      u16 *q_id)
{
	struct ice_aqc_dis_txq_item *qg_list;
	struct ice_hw *hw;
	int status = 0;
	u16 qg_size;
	int i;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return -EIO;

	hw = pi->hw;

	qg_size = struct_size(qg_list, q_id, 1);
	qg_list = kzalloc(qg_size, GFP_KERNEL);
	if (!qg_list)
		return -ENOMEM;

	mutex_lock(&pi->sched_lock);

	for (i = 0; i < count; i++) {
		struct ice_sched_node *node;

		node = ice_sched_find_node_by_teid(pi->root, qset_teid[i]);
		if (!node)
			continue;

		qg_list->parent_teid = node->info.parent_teid;
		qg_list->num_qs = 1;
		qg_list->q_id[0] =
			cpu_to_le16(q_id[i] |
				    ICE_AQC_Q_DIS_BUF_ELEM_TYPE_RDMA_QSET);

		status = ice_aq_dis_lan_txq(hw, 1, qg_list, qg_size,
					    ICE_NO_RESET, 0, NULL);
		if (status)
			break;

		ice_free_sched_node(pi, node);
	}

	mutex_unlock(&pi->sched_lock);
	kfree(qg_list);
	return status;
}

 
static int ice_replay_pre_init(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	u8 i;

	 
	ice_rm_all_sw_replay_rule_info(hw);
	 
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++)
		list_replace_init(&sw->recp_list[i].filt_rules,
				  &sw->recp_list[i].filt_replay_rules);
	ice_sched_replay_agg_vsi_preinit(hw);

	return 0;
}

 
int ice_replay_vsi(struct ice_hw *hw, u16 vsi_handle)
{
	int status;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return -EINVAL;

	 
	if (vsi_handle == ICE_MAIN_VSI_HANDLE) {
		status = ice_replay_pre_init(hw);
		if (status)
			return status;
	}
	 
	status = ice_replay_rss_cfg(hw, vsi_handle);
	if (status)
		return status;
	 
	status = ice_replay_vsi_all_fltr(hw, vsi_handle);
	if (!status)
		status = ice_replay_vsi_agg(hw, vsi_handle);
	return status;
}

 
void ice_replay_post(struct ice_hw *hw)
{
	 
	ice_rm_all_sw_replay_rule_info(hw);
	ice_sched_replay_agg(hw);
}

 
void
ice_stat_update40(struct ice_hw *hw, u32 reg, bool prev_stat_loaded,
		  u64 *prev_stat, u64 *cur_stat)
{
	u64 new_data = rd64(hw, reg) & (BIT_ULL(40) - 1);

	 
	if (!prev_stat_loaded) {
		*prev_stat = new_data;
		return;
	}

	 
	if (new_data >= *prev_stat)
		*cur_stat += new_data - *prev_stat;
	else
		 
		*cur_stat += (new_data + BIT_ULL(40)) - *prev_stat;

	 
	*prev_stat = new_data;
}

 
void
ice_stat_update32(struct ice_hw *hw, u32 reg, bool prev_stat_loaded,
		  u64 *prev_stat, u64 *cur_stat)
{
	u32 new_data;

	new_data = rd32(hw, reg);

	 
	if (!prev_stat_loaded) {
		*prev_stat = new_data;
		return;
	}

	 
	if (new_data >= *prev_stat)
		*cur_stat += new_data - *prev_stat;
	else
		 
		*cur_stat += (new_data + BIT_ULL(32)) - *prev_stat;

	 
	*prev_stat = new_data;
}

 
int
ice_sched_query_elem(struct ice_hw *hw, u32 node_teid,
		     struct ice_aqc_txsched_elem_data *buf)
{
	u16 buf_size, num_elem_ret = 0;
	int status;

	buf_size = sizeof(*buf);
	memset(buf, 0, buf_size);
	buf->node_teid = cpu_to_le32(node_teid);
	status = ice_aq_query_sched_elems(hw, 1, buf, buf_size, &num_elem_ret,
					  NULL);
	if (status || num_elem_ret != 1)
		ice_debug(hw, ICE_DBG_SCHED, "query element failed\n");
	return status;
}

 
int
ice_aq_read_i2c(struct ice_hw *hw, struct ice_aqc_link_topo_addr topo_addr,
		u16 bus_addr, __le16 addr, u8 params, u8 *data,
		struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc = { 0 };
	struct ice_aqc_i2c *cmd;
	u8 data_size;
	int status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_read_i2c);
	cmd = &desc.params.read_write_i2c;

	if (!data)
		return -EINVAL;

	data_size = FIELD_GET(ICE_AQC_I2C_DATA_SIZE_M, params);

	cmd->i2c_bus_addr = cpu_to_le16(bus_addr);
	cmd->topo_addr = topo_addr;
	cmd->i2c_params = params;
	cmd->i2c_addr = addr;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (!status) {
		struct ice_aqc_read_i2c_resp *resp;
		u8 i;

		resp = &desc.params.read_i2c_resp;
		for (i = 0; i < data_size; i++) {
			*data = resp->i2c_data[i];
			data++;
		}
	}

	return status;
}

 
int
ice_aq_write_i2c(struct ice_hw *hw, struct ice_aqc_link_topo_addr topo_addr,
		 u16 bus_addr, __le16 addr, u8 params, const u8 *data,
		 struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc = { 0 };
	struct ice_aqc_i2c *cmd;
	u8 data_size;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_write_i2c);
	cmd = &desc.params.read_write_i2c;

	data_size = FIELD_GET(ICE_AQC_I2C_DATA_SIZE_M, params);

	 
	if (data_size > 4)
		return -EINVAL;

	cmd->i2c_bus_addr = cpu_to_le16(bus_addr);
	cmd->topo_addr = topo_addr;
	cmd->i2c_params = params;
	cmd->i2c_addr = addr;

	memcpy(cmd->i2c_data, data, data_size);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_set_driver_param(struct ice_hw *hw, enum ice_aqc_driver_params idx,
			u32 value, struct ice_sq_cd *cd)
{
	struct ice_aqc_driver_shared_params *cmd;
	struct ice_aq_desc desc;

	if (idx >= ICE_AQC_DRIVER_PARAM_MAX)
		return -EIO;

	cmd = &desc.params.drv_shared_params;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_driver_shared_params);

	cmd->set_or_get_op = ICE_AQC_DRIVER_PARAM_SET;
	cmd->param_indx = idx;
	cmd->param_val = cpu_to_le32(value);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_get_driver_param(struct ice_hw *hw, enum ice_aqc_driver_params idx,
			u32 *value, struct ice_sq_cd *cd)
{
	struct ice_aqc_driver_shared_params *cmd;
	struct ice_aq_desc desc;
	int status;

	if (idx >= ICE_AQC_DRIVER_PARAM_MAX)
		return -EIO;

	cmd = &desc.params.drv_shared_params;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_driver_shared_params);

	cmd->set_or_get_op = ICE_AQC_DRIVER_PARAM_GET;
	cmd->param_indx = idx;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	*value = le32_to_cpu(cmd->param_val);

	return 0;
}

 
int
ice_aq_set_gpio(struct ice_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx, bool value,
		struct ice_sq_cd *cd)
{
	struct ice_aqc_gpio *cmd;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_gpio);
	cmd = &desc.params.read_write_gpio;
	cmd->gpio_ctrl_handle = cpu_to_le16(gpio_ctrl_handle);
	cmd->gpio_num = pin_idx;
	cmd->gpio_val = value ? 1 : 0;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_get_gpio(struct ice_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx,
		bool *value, struct ice_sq_cd *cd)
{
	struct ice_aqc_gpio *cmd;
	struct ice_aq_desc desc;
	int status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_gpio);
	cmd = &desc.params.read_write_gpio;
	cmd->gpio_ctrl_handle = cpu_to_le16(gpio_ctrl_handle);
	cmd->gpio_num = pin_idx;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	*value = !!cmd->gpio_val;
	return 0;
}

 
static bool ice_is_fw_api_min_ver(struct ice_hw *hw, u8 maj, u8 min, u8 patch)
{
	if (hw->api_maj_ver == maj) {
		if (hw->api_min_ver > min)
			return true;
		if (hw->api_min_ver == min && hw->api_patch >= patch)
			return true;
	} else if (hw->api_maj_ver > maj) {
		return true;
	}

	return false;
}

 
bool ice_fw_supports_link_override(struct ice_hw *hw)
{
	return ice_is_fw_api_min_ver(hw, ICE_FW_API_LINK_OVERRIDE_MAJ,
				     ICE_FW_API_LINK_OVERRIDE_MIN,
				     ICE_FW_API_LINK_OVERRIDE_PATCH);
}

 
int
ice_get_link_default_override(struct ice_link_default_override_tlv *ldo,
			      struct ice_port_info *pi)
{
	u16 i, tlv, tlv_len, tlv_start, buf, offset;
	struct ice_hw *hw = pi->hw;
	int status;

	status = ice_get_pfa_module_tlv(hw, &tlv, &tlv_len,
					ICE_SR_LINK_DEFAULT_OVERRIDE_PTR);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read link override TLV.\n");
		return status;
	}

	 
	tlv_start = tlv + pi->lport * ICE_SR_PFA_LINK_OVERRIDE_WORDS +
		ICE_SR_PFA_LINK_OVERRIDE_OFFSET;

	 
	status = ice_read_sr_word(hw, tlv_start, &buf);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read override link options.\n");
		return status;
	}
	ldo->options = buf & ICE_LINK_OVERRIDE_OPT_M;
	ldo->phy_config = (buf & ICE_LINK_OVERRIDE_PHY_CFG_M) >>
		ICE_LINK_OVERRIDE_PHY_CFG_S;

	 
	offset = tlv_start + ICE_SR_PFA_LINK_OVERRIDE_FEC_OFFSET;
	status = ice_read_sr_word(hw, offset, &buf);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read override phy config.\n");
		return status;
	}
	ldo->fec_options = buf & ICE_LINK_OVERRIDE_FEC_OPT_M;

	 
	offset = tlv_start + ICE_SR_PFA_LINK_OVERRIDE_PHY_OFFSET;
	for (i = 0; i < ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS; i++) {
		status = ice_read_sr_word(hw, (offset + i), &buf);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read override link options.\n");
			return status;
		}
		 
		ldo->phy_type_low |= ((u64)buf << (i * 16));
	}

	 
	offset = tlv_start + ICE_SR_PFA_LINK_OVERRIDE_PHY_OFFSET +
		ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS;
	for (i = 0; i < ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS; i++) {
		status = ice_read_sr_word(hw, (offset + i), &buf);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read override link options.\n");
			return status;
		}
		 
		ldo->phy_type_high |= ((u64)buf << (i * 16));
	}

	return status;
}

 
bool ice_is_phy_caps_an_enabled(struct ice_aqc_get_phy_caps_data *caps)
{
	if (caps->caps & ICE_AQC_PHY_AN_MODE ||
	    caps->low_power_ctrl_an & (ICE_AQC_PHY_AN_EN_CLAUSE28 |
				       ICE_AQC_PHY_AN_EN_CLAUSE73 |
				       ICE_AQC_PHY_AN_EN_CLAUSE37))
		return true;

	return false;
}

 
int
ice_aq_set_lldp_mib(struct ice_hw *hw, u8 mib_type, void *buf, u16 buf_size,
		    struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_set_local_mib *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_set_mib;

	if (buf_size == 0 || !buf)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_set_local_mib);

	desc.flags |= cpu_to_le16((u16)ICE_AQ_FLAG_RD);
	desc.datalen = cpu_to_le16(buf_size);

	cmd->type = mib_type;
	cmd->length = cpu_to_le16(buf_size);

	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

 
bool ice_fw_supports_lldp_fltr_ctrl(struct ice_hw *hw)
{
	if (hw->mac_type != ICE_MAC_E810)
		return false;

	return ice_is_fw_api_min_ver(hw, ICE_FW_API_LLDP_FLTR_MAJ,
				     ICE_FW_API_LLDP_FLTR_MIN,
				     ICE_FW_API_LLDP_FLTR_PATCH);
}

 
int
ice_lldp_fltr_add_remove(struct ice_hw *hw, u16 vsi_num, bool add)
{
	struct ice_aqc_lldp_filter_ctrl *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_filter_ctrl;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_filter_ctrl);

	if (add)
		cmd->cmd_flags = ICE_AQC_LLDP_FILTER_ACTION_ADD;
	else
		cmd->cmd_flags = ICE_AQC_LLDP_FILTER_ACTION_DELETE;

	cmd->vsi_num = cpu_to_le16(vsi_num);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 
int ice_lldp_execute_pending_mib(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_execute_pending_mib);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 
bool ice_fw_supports_report_dflt_cfg(struct ice_hw *hw)
{
	return ice_is_fw_api_min_ver(hw, ICE_FW_API_REPORT_DFLT_CFG_MAJ,
				     ICE_FW_API_REPORT_DFLT_CFG_MIN,
				     ICE_FW_API_REPORT_DFLT_CFG_PATCH);
}

 
static const u32 ice_aq_to_link_speed[] = {
	SPEED_10,	 
	SPEED_100,
	SPEED_1000,
	SPEED_2500,
	SPEED_5000,
	SPEED_10000,
	SPEED_20000,
	SPEED_25000,
	SPEED_40000,
	SPEED_50000,
	SPEED_100000,	 
};

 
u32 ice_get_link_speed(u16 index)
{
	if (index >= ARRAY_SIZE(ice_aq_to_link_speed))
		return 0;

	return ice_aq_to_link_speed[index];
}


 

#include "ice_common.h"
#include "ice_sched.h"
#include "ice_dcb.h"

 
static int
ice_aq_get_lldp_mib(struct ice_hw *hw, u8 bridge_type, u8 mib_type, void *buf,
		    u16 buf_size, u16 *local_len, u16 *remote_len,
		    struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_get_mib *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.lldp_get_mib;

	if (buf_size == 0 || !buf)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_get_mib);

	cmd->type = mib_type & ICE_AQ_LLDP_MIB_TYPE_M;
	cmd->type |= (bridge_type << ICE_AQ_LLDP_BRID_TYPE_S) &
		ICE_AQ_LLDP_BRID_TYPE_M;

	desc.datalen = cpu_to_le16(buf_size);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status) {
		if (local_len)
			*local_len = le16_to_cpu(cmd->local_len);
		if (remote_len)
			*remote_len = le16_to_cpu(cmd->remote_len);
	}

	return status;
}

 
static int
ice_aq_cfg_lldp_mib_change(struct ice_hw *hw, bool ena_update,
			   struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_set_mib_change *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_set_event;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_set_mib_change);

	if (!ena_update)
		cmd->command |= ICE_AQ_LLDP_MIB_UPDATE_DIS;
	else
		cmd->command |= FIELD_PREP(ICE_AQ_LLDP_MIB_PENDING_M,
					   ICE_AQ_LLDP_MIB_PENDING_ENABLE);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int
ice_aq_stop_lldp(struct ice_hw *hw, bool shutdown_lldp_agent, bool persist,
		 struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_stop *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_stop;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_stop);

	if (shutdown_lldp_agent)
		cmd->command |= ICE_AQ_LLDP_AGENT_SHUTDOWN;

	if (persist)
		cmd->command |= ICE_AQ_LLDP_AGENT_PERSIST_DIS;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
int ice_aq_start_lldp(struct ice_hw *hw, bool persist, struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_start *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_start;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_start);

	cmd->command = ICE_AQ_LLDP_AGENT_START;

	if (persist)
		cmd->command |= ICE_AQ_LLDP_AGENT_PERSIST_ENA;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
static u8 ice_get_dcbx_status(struct ice_hw *hw)
{
	u32 reg;

	reg = rd32(hw, PRTDCB_GENS);
	return (u8)((reg & PRTDCB_GENS_DCBX_STATUS_M) >>
		    PRTDCB_GENS_DCBX_STATUS_S);
}

 
static void
ice_parse_ieee_ets_common_tlv(u8 *buf, struct ice_dcb_ets_cfg *ets_cfg)
{
	u8 offset = 0;
	int i;

	 
	for (i = 0; i < 4; i++) {
		ets_cfg->prio_table[i * 2] =
			((buf[offset] & ICE_IEEE_ETS_PRIO_1_M) >>
			 ICE_IEEE_ETS_PRIO_1_S);
		ets_cfg->prio_table[i * 2 + 1] =
			((buf[offset] & ICE_IEEE_ETS_PRIO_0_M) >>
			 ICE_IEEE_ETS_PRIO_0_S);
		offset++;
	}

	 
	ice_for_each_traffic_class(i) {
		ets_cfg->tcbwtable[i] = buf[offset];
		ets_cfg->tsatable[i] = buf[ICE_MAX_TRAFFIC_CLASS + offset++];
	}
}

 
static void
ice_parse_ieee_etscfg_tlv(struct ice_lldp_org_tlv *tlv,
			  struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;

	 
	etscfg = &dcbcfg->etscfg;
	etscfg->willing = ((buf[0] & ICE_IEEE_ETS_WILLING_M) >>
			   ICE_IEEE_ETS_WILLING_S);
	etscfg->cbs = ((buf[0] & ICE_IEEE_ETS_CBS_M) >> ICE_IEEE_ETS_CBS_S);
	etscfg->maxtcs = ((buf[0] & ICE_IEEE_ETS_MAXTC_M) >>
			  ICE_IEEE_ETS_MAXTC_S);

	 
	ice_parse_ieee_ets_common_tlv(&buf[1], etscfg);
}

 
static void
ice_parse_ieee_etsrec_tlv(struct ice_lldp_org_tlv *tlv,
			  struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	 
	ice_parse_ieee_ets_common_tlv(&buf[1], &dcbcfg->etsrec);
}

 
static void
ice_parse_ieee_pfccfg_tlv(struct ice_lldp_org_tlv *tlv,
			  struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	 
	dcbcfg->pfc.willing = ((buf[0] & ICE_IEEE_PFC_WILLING_M) >>
			       ICE_IEEE_PFC_WILLING_S);
	dcbcfg->pfc.mbc = ((buf[0] & ICE_IEEE_PFC_MBC_M) >> ICE_IEEE_PFC_MBC_S);
	dcbcfg->pfc.pfccap = ((buf[0] & ICE_IEEE_PFC_CAP_M) >>
			      ICE_IEEE_PFC_CAP_S);
	dcbcfg->pfc.pfcena = buf[1];
}

 
static void
ice_parse_ieee_app_tlv(struct ice_lldp_org_tlv *tlv,
		       struct ice_dcbx_cfg *dcbcfg)
{
	u16 offset = 0;
	u16 typelen;
	int i = 0;
	u16 len;
	u8 *buf;

	typelen = ntohs(tlv->typelen);
	len = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
	buf = tlv->tlvinfo;

	 
	len -= (sizeof(tlv->ouisubtype) + 1);

	 
	offset++;

	 
	while (offset < len) {
		dcbcfg->app[i].priority = ((buf[offset] &
					    ICE_IEEE_APP_PRIO_M) >>
					   ICE_IEEE_APP_PRIO_S);
		dcbcfg->app[i].selector = ((buf[offset] &
					    ICE_IEEE_APP_SEL_M) >>
					   ICE_IEEE_APP_SEL_S);
		dcbcfg->app[i].prot_id = (buf[offset + 1] << 0x8) |
			buf[offset + 2];
		 
		offset += 3;
		i++;
		if (i >= ICE_DCBX_MAX_APPS)
			break;
	}

	dcbcfg->numapps = i;
}

 
static void
ice_parse_ieee_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u32 ouisubtype;
	u8 subtype;

	ouisubtype = ntohl(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & ICE_LLDP_TLV_SUBTYPE_M) >>
		       ICE_LLDP_TLV_SUBTYPE_S);
	switch (subtype) {
	case ICE_IEEE_SUBTYPE_ETS_CFG:
		ice_parse_ieee_etscfg_tlv(tlv, dcbcfg);
		break;
	case ICE_IEEE_SUBTYPE_ETS_REC:
		ice_parse_ieee_etsrec_tlv(tlv, dcbcfg);
		break;
	case ICE_IEEE_SUBTYPE_PFC_CFG:
		ice_parse_ieee_pfccfg_tlv(tlv, dcbcfg);
		break;
	case ICE_IEEE_SUBTYPE_APP_PRI:
		ice_parse_ieee_app_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

 
static void
ice_parse_cee_pgcfg_tlv(struct ice_cee_feat_tlv *tlv,
			struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	int i;

	etscfg = &dcbcfg->etscfg;

	if (tlv->en_will_err & ICE_CEE_FEAT_TLV_WILLING_M)
		etscfg->willing = 1;

	etscfg->cbs = 0;
	 
	for (i = 0; i < 4; i++) {
		etscfg->prio_table[i * 2] =
			((buf[offset] & ICE_CEE_PGID_PRIO_1_M) >>
			 ICE_CEE_PGID_PRIO_1_S);
		etscfg->prio_table[i * 2 + 1] =
			((buf[offset] & ICE_CEE_PGID_PRIO_0_M) >>
			 ICE_CEE_PGID_PRIO_0_S);
		offset++;
	}

	 
	ice_for_each_traffic_class(i) {
		etscfg->tcbwtable[i] = buf[offset++];

		if (etscfg->prio_table[i] == ICE_CEE_PGID_STRICT)
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_STRICT;
		else
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_ETS;
	}

	 
	etscfg->maxtcs = buf[offset];
}

 
static void
ice_parse_cee_pfccfg_tlv(struct ice_cee_feat_tlv *tlv,
			 struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	if (tlv->en_will_err & ICE_CEE_FEAT_TLV_WILLING_M)
		dcbcfg->pfc.willing = 1;

	 
	dcbcfg->pfc.pfcena = buf[0];
	dcbcfg->pfc.pfccap = buf[1];
}

 
static void
ice_parse_cee_app_tlv(struct ice_cee_feat_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u16 len, typelen, offset = 0;
	struct ice_cee_app_prio *app;
	u8 i;

	typelen = ntohs(tlv->hdr.typelen);
	len = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);

	dcbcfg->numapps = len / sizeof(*app);
	if (!dcbcfg->numapps)
		return;
	if (dcbcfg->numapps > ICE_DCBX_MAX_APPS)
		dcbcfg->numapps = ICE_DCBX_MAX_APPS;

	for (i = 0; i < dcbcfg->numapps; i++) {
		u8 up, selector;

		app = (struct ice_cee_app_prio *)(tlv->tlvinfo + offset);
		for (up = 0; up < ICE_MAX_USER_PRIORITY; up++)
			if (app->prio_map & BIT(up))
				break;

		dcbcfg->app[i].priority = up;

		 
		selector = (app->upper_oui_sel & ICE_CEE_APP_SELECTOR_M);
		switch (selector) {
		case ICE_CEE_APP_SEL_ETHTYPE:
			dcbcfg->app[i].selector = ICE_APP_SEL_ETHTYPE;
			break;
		case ICE_CEE_APP_SEL_TCPIP:
			dcbcfg->app[i].selector = ICE_APP_SEL_TCPIP;
			break;
		default:
			 
			dcbcfg->app[i].selector = selector;
		}

		dcbcfg->app[i].prot_id = ntohs(app->protocol);
		 
		offset += sizeof(*app);
	}
}

 
static void
ice_parse_cee_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_cee_feat_tlv *sub_tlv;
	u8 subtype, feat_tlv_count = 0;
	u16 len, tlvlen, typelen;
	u32 ouisubtype;

	ouisubtype = ntohl(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & ICE_LLDP_TLV_SUBTYPE_M) >>
		       ICE_LLDP_TLV_SUBTYPE_S);
	 
	if (subtype != ICE_CEE_DCBX_TYPE)
		return;

	typelen = ntohs(tlv->typelen);
	tlvlen = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
	len = sizeof(tlv->typelen) + sizeof(ouisubtype) +
		sizeof(struct ice_cee_ctrl_tlv);
	 
	if (tlvlen <= len)
		return;

	sub_tlv = (struct ice_cee_feat_tlv *)((char *)tlv + len);
	while (feat_tlv_count < ICE_CEE_MAX_FEAT_TYPE) {
		u16 sublen;

		typelen = ntohs(sub_tlv->hdr.typelen);
		sublen = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
		subtype = (u8)((typelen & ICE_LLDP_TLV_TYPE_M) >>
			       ICE_LLDP_TLV_TYPE_S);
		switch (subtype) {
		case ICE_CEE_SUBTYPE_PG_CFG:
			ice_parse_cee_pgcfg_tlv(sub_tlv, dcbcfg);
			break;
		case ICE_CEE_SUBTYPE_PFC_CFG:
			ice_parse_cee_pfccfg_tlv(sub_tlv, dcbcfg);
			break;
		case ICE_CEE_SUBTYPE_APP_PRI:
			ice_parse_cee_app_tlv(sub_tlv, dcbcfg);
			break;
		default:
			return;	 
		}
		feat_tlv_count++;
		 
		sub_tlv = (struct ice_cee_feat_tlv *)
			  ((char *)sub_tlv + sizeof(sub_tlv->hdr.typelen) +
			   sublen);
	}
}

 
static void
ice_parse_org_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u32 ouisubtype;
	u32 oui;

	ouisubtype = ntohl(tlv->ouisubtype);
	oui = ((ouisubtype & ICE_LLDP_TLV_OUI_M) >> ICE_LLDP_TLV_OUI_S);
	switch (oui) {
	case ICE_IEEE_8021QAZ_OUI:
		ice_parse_ieee_tlv(tlv, dcbcfg);
		break;
	case ICE_CEE_DCBX_OUI:
		ice_parse_cee_tlv(tlv, dcbcfg);
		break;
	default:
		break;  
	}
}

 
static int ice_lldp_to_dcb_cfg(u8 *lldpmib, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_lldp_org_tlv *tlv;
	u16 offset = 0;
	int ret = 0;
	u16 typelen;
	u16 type;
	u16 len;

	if (!lldpmib || !dcbcfg)
		return -EINVAL;

	 
	lldpmib += ETH_HLEN;
	tlv = (struct ice_lldp_org_tlv *)lldpmib;
	while (1) {
		typelen = ntohs(tlv->typelen);
		type = ((typelen & ICE_LLDP_TLV_TYPE_M) >> ICE_LLDP_TLV_TYPE_S);
		len = ((typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S);
		offset += sizeof(typelen) + len;

		 
		if (type == ICE_TLV_TYPE_END || offset > ICE_LLDPDU_SIZE)
			break;

		switch (type) {
		case ICE_TLV_TYPE_ORG:
			ice_parse_org_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}

		 
		tlv = (struct ice_lldp_org_tlv *)
		      ((char *)tlv + sizeof(tlv->typelen) + len);
	}

	return ret;
}

 
int
ice_aq_get_dcb_cfg(struct ice_hw *hw, u8 mib_type, u8 bridgetype,
		   struct ice_dcbx_cfg *dcbcfg)
{
	u8 *lldpmib;
	int ret;

	 
	lldpmib = devm_kzalloc(ice_hw_to_dev(hw), ICE_LLDPDU_SIZE, GFP_KERNEL);
	if (!lldpmib)
		return -ENOMEM;

	ret = ice_aq_get_lldp_mib(hw, bridgetype, mib_type, (void *)lldpmib,
				  ICE_LLDPDU_SIZE, NULL, NULL, NULL);

	if (!ret)
		 
		ret = ice_lldp_to_dcb_cfg(lldpmib, dcbcfg);

	devm_kfree(ice_hw_to_dev(hw), lldpmib);

	return ret;
}

 
int
ice_aq_start_stop_dcbx(struct ice_hw *hw, bool start_dcbx_agent,
		       bool *dcbx_agent_status, struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_stop_start_specific_agent *cmd;
	struct ice_aq_desc desc;
	u16 opcode;
	int status;

	cmd = &desc.params.lldp_agent_ctrl;

	opcode = ice_aqc_opc_lldp_stop_start_specific_agent;

	ice_fill_dflt_direct_cmd_desc(&desc, opcode);

	if (start_dcbx_agent)
		cmd->command = ICE_AQC_START_STOP_AGENT_START_DCBX;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	*dcbx_agent_status = false;

	if (!status &&
	    cmd->command == ICE_AQC_START_STOP_AGENT_START_DCBX)
		*dcbx_agent_status = true;

	return status;
}

 
static int
ice_aq_get_cee_dcb_cfg(struct ice_hw *hw,
		       struct ice_aqc_get_cee_dcb_cfg_resp *buff,
		       struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_cee_dcb_cfg);

	return ice_aq_send_cmd(hw, &desc, (void *)buff, sizeof(*buff), cd);
}

 
int ice_aq_set_pfc_mode(struct ice_hw *hw, u8 pfc_mode, struct ice_sq_cd *cd)
{
	struct ice_aqc_set_query_pfc_mode *cmd;
	struct ice_aq_desc desc;
	int status;

	if (pfc_mode > ICE_AQC_PFC_DSCP_BASED_PFC)
		return -EINVAL;

	cmd = &desc.params.set_query_pfc_mode;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_pfc_mode);

	cmd->pfc_mode = pfc_mode;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	 
	if (cmd->pfc_mode != pfc_mode)
		return -EOPNOTSUPP;

	return 0;
}

 
static void
ice_cee_to_dcb_cfg(struct ice_aqc_get_cee_dcb_cfg_resp *cee_cfg,
		   struct ice_port_info *pi)
{
	u32 status, tlv_status = le32_to_cpu(cee_cfg->tlv_status);
	u32 ice_aqc_cee_status_mask, ice_aqc_cee_status_shift, j;
	u8 i, err, sync, oper, app_index, ice_app_sel_type;
	u16 app_prio = le16_to_cpu(cee_cfg->oper_app_prio);
	u16 ice_aqc_cee_app_mask, ice_aqc_cee_app_shift;
	struct ice_dcbx_cfg *cmp_dcbcfg, *dcbcfg;
	u16 ice_app_prot_id_type;

	dcbcfg = &pi->qos_cfg.local_dcbx_cfg;
	dcbcfg->dcbx_mode = ICE_DCBX_MODE_CEE;
	dcbcfg->tlv_status = tlv_status;

	 
	dcbcfg->etscfg.maxtcs = cee_cfg->oper_num_tc;

	 
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS / 2; i++) {
		dcbcfg->etscfg.prio_table[i * 2] =
			((cee_cfg->oper_prio_tc[i] & ICE_CEE_PGID_PRIO_0_M) >>
			 ICE_CEE_PGID_PRIO_0_S);
		dcbcfg->etscfg.prio_table[i * 2 + 1] =
			((cee_cfg->oper_prio_tc[i] & ICE_CEE_PGID_PRIO_1_M) >>
			 ICE_CEE_PGID_PRIO_1_S);
	}

	ice_for_each_traffic_class(i) {
		dcbcfg->etscfg.tcbwtable[i] = cee_cfg->oper_tc_bw[i];

		if (dcbcfg->etscfg.prio_table[i] == ICE_CEE_PGID_STRICT) {
			 
			dcbcfg->etscfg.prio_table[i] = cee_cfg->oper_num_tc - 1;
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_STRICT;
		} else {
			dcbcfg->etscfg.tsatable[i] = ICE_IEEE_TSA_ETS;
		}
	}

	 
	dcbcfg->pfc.pfcena = cee_cfg->oper_pfc_en;
	dcbcfg->pfc.pfccap = ICE_MAX_TRAFFIC_CLASS;

	 
	if (dcbcfg->app_mode == ICE_DCBX_APPS_NON_WILLING)
		cmp_dcbcfg = &pi->qos_cfg.desired_dcbx_cfg;
	else
		cmp_dcbcfg = &pi->qos_cfg.remote_dcbx_cfg;

	app_index = 0;
	for (i = 0; i < 3; i++) {
		if (i == 0) {
			 
			ice_aqc_cee_status_mask = ICE_AQC_CEE_FCOE_STATUS_M;
			ice_aqc_cee_status_shift = ICE_AQC_CEE_FCOE_STATUS_S;
			ice_aqc_cee_app_mask = ICE_AQC_CEE_APP_FCOE_M;
			ice_aqc_cee_app_shift = ICE_AQC_CEE_APP_FCOE_S;
			ice_app_sel_type = ICE_APP_SEL_ETHTYPE;
			ice_app_prot_id_type = ETH_P_FCOE;
		} else if (i == 1) {
			 
			ice_aqc_cee_status_mask = ICE_AQC_CEE_ISCSI_STATUS_M;
			ice_aqc_cee_status_shift = ICE_AQC_CEE_ISCSI_STATUS_S;
			ice_aqc_cee_app_mask = ICE_AQC_CEE_APP_ISCSI_M;
			ice_aqc_cee_app_shift = ICE_AQC_CEE_APP_ISCSI_S;
			ice_app_sel_type = ICE_APP_SEL_TCPIP;
			ice_app_prot_id_type = ISCSI_LISTEN_PORT;

			for (j = 0; j < cmp_dcbcfg->numapps; j++) {
				u16 prot_id = cmp_dcbcfg->app[j].prot_id;
				u8 sel = cmp_dcbcfg->app[j].selector;

				if  (sel == ICE_APP_SEL_TCPIP &&
				     (prot_id == ISCSI_LISTEN_PORT ||
				      prot_id == ICE_APP_PROT_ID_ISCSI_860)) {
					ice_app_prot_id_type = prot_id;
					break;
				}
			}
		} else {
			 
			ice_aqc_cee_status_mask = ICE_AQC_CEE_FIP_STATUS_M;
			ice_aqc_cee_status_shift = ICE_AQC_CEE_FIP_STATUS_S;
			ice_aqc_cee_app_mask = ICE_AQC_CEE_APP_FIP_M;
			ice_aqc_cee_app_shift = ICE_AQC_CEE_APP_FIP_S;
			ice_app_sel_type = ICE_APP_SEL_ETHTYPE;
			ice_app_prot_id_type = ETH_P_FIP;
		}

		status = (tlv_status & ice_aqc_cee_status_mask) >>
			 ice_aqc_cee_status_shift;
		err = (status & ICE_TLV_STATUS_ERR) ? 1 : 0;
		sync = (status & ICE_TLV_STATUS_SYNC) ? 1 : 0;
		oper = (status & ICE_TLV_STATUS_OPER) ? 1 : 0;
		 
		if (!err && sync && oper) {
			dcbcfg->app[app_index].priority =
				(app_prio & ice_aqc_cee_app_mask) >>
				ice_aqc_cee_app_shift;
			dcbcfg->app[app_index].selector = ice_app_sel_type;
			dcbcfg->app[app_index].prot_id = ice_app_prot_id_type;
			app_index++;
		}
	}

	dcbcfg->numapps = app_index;
}

 
static int ice_get_ieee_or_cee_dcb_cfg(struct ice_port_info *pi, u8 dcbx_mode)
{
	struct ice_dcbx_cfg *dcbx_cfg = NULL;
	int ret;

	if (!pi)
		return -EINVAL;

	if (dcbx_mode == ICE_DCBX_MODE_IEEE)
		dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
	else if (dcbx_mode == ICE_DCBX_MODE_CEE)
		dcbx_cfg = &pi->qos_cfg.desired_dcbx_cfg;

	 
	ret = ice_aq_get_dcb_cfg(pi->hw, ICE_AQ_LLDP_MIB_LOCAL,
				 ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID, dcbx_cfg);
	if (ret)
		goto out;

	 
	dcbx_cfg = &pi->qos_cfg.remote_dcbx_cfg;
	ret = ice_aq_get_dcb_cfg(pi->hw, ICE_AQ_LLDP_MIB_REMOTE,
				 ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID, dcbx_cfg);
	 
	if (pi->hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT)
		ret = 0;

out:
	return ret;
}

 
int ice_get_dcb_cfg(struct ice_port_info *pi)
{
	struct ice_aqc_get_cee_dcb_cfg_resp cee_cfg;
	struct ice_dcbx_cfg *dcbx_cfg;
	int ret;

	if (!pi)
		return -EINVAL;

	ret = ice_aq_get_cee_dcb_cfg(pi->hw, &cee_cfg, NULL);
	if (!ret) {
		 
		ret = ice_get_ieee_or_cee_dcb_cfg(pi, ICE_DCBX_MODE_CEE);
		ice_cee_to_dcb_cfg(&cee_cfg, pi);
	} else if (pi->hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT) {
		 
		dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
		dcbx_cfg->dcbx_mode = ICE_DCBX_MODE_IEEE;
		ret = ice_get_ieee_or_cee_dcb_cfg(pi, ICE_DCBX_MODE_IEEE);
	}

	return ret;
}

 
void ice_get_dcb_cfg_from_mib_change(struct ice_port_info *pi,
				     struct ice_rq_event_info *event)
{
	struct ice_dcbx_cfg *dcbx_cfg = &pi->qos_cfg.local_dcbx_cfg;
	struct ice_aqc_lldp_get_mib *mib;
	u8 change_type, dcbx_mode;

	mib = (struct ice_aqc_lldp_get_mib *)&event->desc.params.raw;

	change_type = FIELD_GET(ICE_AQ_LLDP_MIB_TYPE_M,  mib->type);
	if (change_type == ICE_AQ_LLDP_MIB_REMOTE)
		dcbx_cfg = &pi->qos_cfg.remote_dcbx_cfg;

	dcbx_mode = FIELD_GET(ICE_AQ_LLDP_DCBX_M, mib->type);

	switch (dcbx_mode) {
	case ICE_AQ_LLDP_DCBX_IEEE:
		dcbx_cfg->dcbx_mode = ICE_DCBX_MODE_IEEE;
		ice_lldp_to_dcb_cfg(event->msg_buf, dcbx_cfg);
		break;

	case ICE_AQ_LLDP_DCBX_CEE:
		pi->qos_cfg.desired_dcbx_cfg = pi->qos_cfg.local_dcbx_cfg;
		ice_cee_to_dcb_cfg((struct ice_aqc_get_cee_dcb_cfg_resp *)
				   event->msg_buf, pi);
		break;
	}
}

 
int ice_init_dcb(struct ice_hw *hw, bool enable_mib_change)
{
	struct ice_qos_cfg *qos_cfg = &hw->port_info->qos_cfg;
	int ret = 0;

	if (!hw->func_caps.common_cap.dcb)
		return -EOPNOTSUPP;

	qos_cfg->is_sw_lldp = true;

	 
	qos_cfg->dcbx_status = ice_get_dcbx_status(hw);

	if (qos_cfg->dcbx_status == ICE_DCBX_STATUS_DONE ||
	    qos_cfg->dcbx_status == ICE_DCBX_STATUS_IN_PROGRESS ||
	    qos_cfg->dcbx_status == ICE_DCBX_STATUS_NOT_STARTED) {
		 
		ret = ice_get_dcb_cfg(hw->port_info);
		if (ret)
			return ret;
		qos_cfg->is_sw_lldp = false;
	} else if (qos_cfg->dcbx_status == ICE_DCBX_STATUS_DIS) {
		return -EBUSY;
	}

	 
	if (enable_mib_change) {
		ret = ice_aq_cfg_lldp_mib_change(hw, true, NULL);
		if (ret)
			qos_cfg->is_sw_lldp = true;
	}

	return ret;
}

 
int ice_cfg_lldp_mib_change(struct ice_hw *hw, bool ena_mib)
{
	struct ice_qos_cfg *qos_cfg = &hw->port_info->qos_cfg;
	int ret;

	if (!hw->func_caps.common_cap.dcb)
		return -EOPNOTSUPP;

	 
	qos_cfg->dcbx_status = ice_get_dcbx_status(hw);

	if (qos_cfg->dcbx_status == ICE_DCBX_STATUS_DIS)
		return -EBUSY;

	ret = ice_aq_cfg_lldp_mib_change(hw, ena_mib, NULL);
	if (!ret)
		qos_cfg->is_sw_lldp = !ena_mib;

	return ret;
}

 
static void
ice_add_ieee_ets_common_tlv(u8 *buf, struct ice_dcb_ets_cfg *ets_cfg)
{
	u8 priority0, priority1;
	u8 offset = 0;
	int i;

	 
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS / 2; i++) {
		priority0 = ets_cfg->prio_table[i * 2] & 0xF;
		priority1 = ets_cfg->prio_table[i * 2 + 1] & 0xF;
		buf[offset] = (priority0 << ICE_IEEE_ETS_PRIO_1_S) | priority1;
		offset++;
	}

	 
	ice_for_each_traffic_class(i) {
		buf[offset] = ets_cfg->tcbwtable[i];
		buf[ICE_MAX_TRAFFIC_CLASS + offset] = ets_cfg->tsatable[i];
		offset++;
	}
}

 
static void
ice_add_ieee_ets_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;
	u8 maxtcwilling = 0;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_ETS_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_ETS_CFG);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	etscfg = &dcbcfg->etscfg;
	if (etscfg->willing)
		maxtcwilling = BIT(ICE_IEEE_ETS_WILLING_S);
	maxtcwilling |= etscfg->maxtcs & ICE_IEEE_ETS_MAXTC_M;
	buf[0] = maxtcwilling;

	 
	ice_add_ieee_ets_common_tlv(&buf[1], etscfg);
}

 
static void
ice_add_ieee_etsrec_tlv(struct ice_lldp_org_tlv *tlv,
			struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etsrec;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_ETS_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_ETS_REC);
	tlv->ouisubtype = htonl(ouisubtype);

	etsrec = &dcbcfg->etsrec;

	 
	 
	ice_add_ieee_ets_common_tlv(&buf[1], etsrec);
}

 
static void
ice_add_ieee_pfc_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_IEEE_PFC_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_PFC_CFG);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	if (dcbcfg->pfc.willing)
		buf[0] = BIT(ICE_IEEE_PFC_WILLING_S);

	if (dcbcfg->pfc.mbc)
		buf[0] |= BIT(ICE_IEEE_PFC_MBC_S);

	buf[0] |= dcbcfg->pfc.pfccap & 0xF;
	buf[1] = dcbcfg->pfc.pfcena;
}

 
static void
ice_add_ieee_app_pri_tlv(struct ice_lldp_org_tlv *tlv,
			 struct ice_dcbx_cfg *dcbcfg)
{
	u16 typelen, len, offset = 0;
	u8 priority, selector, i = 0;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;

	 
	if (dcbcfg->numapps == 0)
		return;
	ouisubtype = ((ICE_IEEE_8021QAZ_OUI << ICE_LLDP_TLV_OUI_S) |
		      ICE_IEEE_SUBTYPE_APP_PRI);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	offset++;
	 
	while (i < dcbcfg->numapps) {
		priority = dcbcfg->app[i].priority & 0x7;
		selector = dcbcfg->app[i].selector & 0x7;
		buf[offset] = (priority << ICE_IEEE_APP_PRIO_S) | selector;
		buf[offset + 1] = (dcbcfg->app[i].prot_id >> 0x8) & 0xFF;
		buf[offset + 2] = dcbcfg->app[i].prot_id & 0xFF;
		 
		offset += 3;
		i++;
		if (i >= ICE_DCBX_MAX_APPS)
			break;
	}
	 
	len = sizeof(tlv->ouisubtype) + 1 + (i * 3);
	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) | (len & 0x1FF));
	tlv->typelen = htons(typelen);
}

 
static void
ice_add_dscp_up_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;
	int i;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_UP_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_DSCP2UP);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	for (i = 0; i < ICE_DSCP_NUM_VAL; i++) {
		 
		buf[i] = dcbcfg->dscp_map[i];
		 
		buf[i + ICE_DSCP_IPV6_OFFSET] = dcbcfg->dscp_map[i];
	}

	 
	buf[i] = 0;

	 
	buf[i + ICE_DSCP_IPV6_OFFSET] = 0;
}

#define ICE_BYTES_PER_TC	8
 
static void
ice_add_dscp_enf_tlv(struct ice_lldp_org_tlv *tlv)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_ENF_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_ENFORCE);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	memset(buf, 0, 2 * (ICE_MAX_TRAFFIC_CLASS * ICE_BYTES_PER_TC));
}

 
static void
ice_add_dscp_tc_bw_tlv(struct ice_lldp_org_tlv *tlv,
		       struct ice_dcbx_cfg *dcbcfg)
{
	struct ice_dcb_ets_cfg *etscfg;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u8 offset = 0;
	u16 typelen;
	int i;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_TC_BW_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_TCBW);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	etscfg = &dcbcfg->etscfg;
	buf[0] = etscfg->maxtcs & ICE_IEEE_ETS_MAXTC_M;

	 
	offset = 5;

	 
	for (i = 0; i < ICE_MAX_TRAFFIC_CLASS; i++) {
		buf[offset] = etscfg->tcbwtable[i];
		buf[offset + ICE_MAX_TRAFFIC_CLASS] = etscfg->tsatable[i];
		offset++;
	}
}

 
static void
ice_add_dscp_pfc_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelen;

	typelen = ((ICE_TLV_TYPE_ORG << ICE_LLDP_TLV_TYPE_S) |
		   ICE_DSCP_PFC_TLV_LEN);
	tlv->typelen = htons(typelen);

	ouisubtype = (u32)((ICE_DSCP_OUI << ICE_LLDP_TLV_OUI_S) |
			   ICE_DSCP_SUBTYPE_PFC);
	tlv->ouisubtype = htonl(ouisubtype);

	buf[0] = dcbcfg->pfc.pfccap & 0xF;
	buf[1] = dcbcfg->pfc.pfcena;
}

 
static void
ice_add_dcb_tlv(struct ice_lldp_org_tlv *tlv, struct ice_dcbx_cfg *dcbcfg,
		u16 tlvid)
{
	if (dcbcfg->pfc_mode == ICE_QOS_MODE_VLAN) {
		switch (tlvid) {
		case ICE_IEEE_TLV_ID_ETS_CFG:
			ice_add_ieee_ets_tlv(tlv, dcbcfg);
			break;
		case ICE_IEEE_TLV_ID_ETS_REC:
			ice_add_ieee_etsrec_tlv(tlv, dcbcfg);
			break;
		case ICE_IEEE_TLV_ID_PFC_CFG:
			ice_add_ieee_pfc_tlv(tlv, dcbcfg);
			break;
		case ICE_IEEE_TLV_ID_APP_PRI:
			ice_add_ieee_app_pri_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}
	} else {
		 
		switch (tlvid) {
		case ICE_TLV_ID_DSCP_UP:
			ice_add_dscp_up_tlv(tlv, dcbcfg);
			break;
		case ICE_TLV_ID_DSCP_ENF:
			ice_add_dscp_enf_tlv(tlv);
			break;
		case ICE_TLV_ID_DSCP_TC_BW:
			ice_add_dscp_tc_bw_tlv(tlv, dcbcfg);
			break;
		case ICE_TLV_ID_DSCP_TO_PFC:
			ice_add_dscp_pfc_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}
	}
}

 
static void
ice_dcb_cfg_to_lldp(u8 *lldpmib, u16 *miblen, struct ice_dcbx_cfg *dcbcfg)
{
	u16 len, offset = 0, tlvid = ICE_TLV_ID_START;
	struct ice_lldp_org_tlv *tlv;
	u16 typelen;

	tlv = (struct ice_lldp_org_tlv *)lldpmib;
	while (1) {
		ice_add_dcb_tlv(tlv, dcbcfg, tlvid++);
		typelen = ntohs(tlv->typelen);
		len = (typelen & ICE_LLDP_TLV_LEN_M) >> ICE_LLDP_TLV_LEN_S;
		if (len)
			offset += len + 2;
		 
		if (tlvid >= ICE_TLV_ID_END_OF_LLDPPDU ||
		    offset > ICE_LLDPDU_SIZE)
			break;
		 
		if (len)
			tlv = (struct ice_lldp_org_tlv *)
				((char *)tlv + sizeof(tlv->typelen) + len);
	}
	*miblen = offset;
}

 
int ice_set_dcb_cfg(struct ice_port_info *pi)
{
	u8 mib_type, *lldpmib = NULL;
	struct ice_dcbx_cfg *dcbcfg;
	struct ice_hw *hw;
	u16 miblen;
	int ret;

	if (!pi)
		return -EINVAL;

	hw = pi->hw;

	 
	dcbcfg = &pi->qos_cfg.local_dcbx_cfg;
	 
	lldpmib = devm_kzalloc(ice_hw_to_dev(hw), ICE_LLDPDU_SIZE, GFP_KERNEL);
	if (!lldpmib)
		return -ENOMEM;

	mib_type = SET_LOCAL_MIB_TYPE_LOCAL_MIB;
	if (dcbcfg->app_mode == ICE_DCBX_APPS_NON_WILLING)
		mib_type |= SET_LOCAL_MIB_TYPE_CEE_NON_WILLING;

	ice_dcb_cfg_to_lldp(lldpmib, &miblen, dcbcfg);
	ret = ice_aq_set_lldp_mib(hw, mib_type, (void *)lldpmib, miblen,
				  NULL);

	devm_kfree(ice_hw_to_dev(hw), lldpmib);

	return ret;
}

 
static int
ice_aq_query_port_ets(struct ice_port_info *pi,
		      struct ice_aqc_port_ets_elem *buf, u16 buf_size,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_query_port_ets *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!pi)
		return -EINVAL;
	cmd = &desc.params.port_ets;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_query_port_ets);
	cmd->port_teid = pi->root->info.node_teid;

	status = ice_aq_send_cmd(pi->hw, &desc, buf, buf_size, cd);
	return status;
}

 
static int
ice_update_port_tc_tree_cfg(struct ice_port_info *pi,
			    struct ice_aqc_port_ets_elem *buf)
{
	struct ice_sched_node *node, *tc_node;
	struct ice_aqc_txsched_elem_data elem;
	u32 teid1, teid2;
	int status = 0;
	u8 i, j;

	if (!pi)
		return -EINVAL;
	 
	for (i = 0; i < pi->root->num_children; i++) {
		teid1 = le32_to_cpu(pi->root->children[i]->info.node_teid);
		ice_for_each_traffic_class(j) {
			teid2 = le32_to_cpu(buf->tc_node_teid[j]);
			if (teid1 == teid2)
				break;
		}
		if (j < ICE_MAX_TRAFFIC_CLASS)
			continue;
		 
		pi->root->children[i]->in_use = false;
	}
	 
	ice_for_each_traffic_class(j) {
		teid2 = le32_to_cpu(buf->tc_node_teid[j]);
		if (teid2 == ICE_INVAL_TEID)
			continue;
		 
		for (i = 0; i < pi->root->num_children; i++) {
			tc_node = pi->root->children[i];
			if (!tc_node)
				continue;
			teid1 = le32_to_cpu(tc_node->info.node_teid);
			if (teid1 == teid2) {
				tc_node->tc_num = j;
				tc_node->in_use = true;
				break;
			}
		}
		if (i < pi->root->num_children)
			continue;
		 
		status = ice_sched_query_elem(pi->hw, teid2, &elem);
		if (!status)
			status = ice_sched_add_node(pi, 1, &elem, NULL);
		if (status)
			break;
		 
		node = ice_sched_find_node_by_teid(pi->root, teid2);
		if (node)
			node->tc_num = j;
	}
	return status;
}

 
int
ice_query_port_ets(struct ice_port_info *pi,
		   struct ice_aqc_port_ets_elem *buf, u16 buf_size,
		   struct ice_sq_cd *cd)
{
	int status;

	mutex_lock(&pi->sched_lock);
	status = ice_aq_query_port_ets(pi, buf, buf_size, cd);
	if (!status)
		status = ice_update_port_tc_tree_cfg(pi, buf);
	mutex_unlock(&pi->sched_lock);
	return status;
}

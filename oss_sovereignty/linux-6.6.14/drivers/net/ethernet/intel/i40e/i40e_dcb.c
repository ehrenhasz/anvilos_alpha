
 

#include "i40e_adminq.h"
#include "i40e_prototype.h"
#include "i40e_dcb.h"

 
int i40e_get_dcbx_status(struct i40e_hw *hw, u16 *status)
{
	u32 reg;

	if (!status)
		return -EINVAL;

	reg = rd32(hw, I40E_PRTDCB_GENS);
	*status = (u16)((reg & I40E_PRTDCB_GENS_DCBX_STATUS_MASK) >>
			I40E_PRTDCB_GENS_DCBX_STATUS_SHIFT);

	return 0;
}

 
static void i40e_parse_ieee_etscfg_tlv(struct i40e_lldp_org_tlv *tlv,
				       struct i40e_dcbx_config *dcbcfg)
{
	struct i40e_dcb_ets_config *etscfg;
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	u8 priority;
	int i;

	 
	etscfg = &dcbcfg->etscfg;
	etscfg->willing = (u8)((buf[offset] & I40E_IEEE_ETS_WILLING_MASK) >>
			       I40E_IEEE_ETS_WILLING_SHIFT);
	etscfg->cbs = (u8)((buf[offset] & I40E_IEEE_ETS_CBS_MASK) >>
			   I40E_IEEE_ETS_CBS_SHIFT);
	etscfg->maxtcs = (u8)((buf[offset] & I40E_IEEE_ETS_MAXTC_MASK) >>
			      I40E_IEEE_ETS_MAXTC_SHIFT);

	 
	offset++;

	 
	for (i = 0; i < 4; i++) {
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_1_MASK) >>
				I40E_IEEE_ETS_PRIO_1_SHIFT);
		etscfg->prioritytable[i * 2] =  priority;
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_0_MASK) >>
				I40E_IEEE_ETS_PRIO_0_SHIFT);
		etscfg->prioritytable[i * 2 + 1] = priority;
		offset++;
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		etscfg->tcbwtable[i] = buf[offset++];

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		etscfg->tsatable[i] = buf[offset++];
}

 
static void i40e_parse_ieee_etsrec_tlv(struct i40e_lldp_org_tlv *tlv,
				       struct i40e_dcbx_config *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	u8 priority;
	int i;

	 
	offset++;

	 
	for (i = 0; i < 4; i++) {
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_1_MASK) >>
				I40E_IEEE_ETS_PRIO_1_SHIFT);
		dcbcfg->etsrec.prioritytable[i*2] =  priority;
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_0_MASK) >>
				I40E_IEEE_ETS_PRIO_0_SHIFT);
		dcbcfg->etsrec.prioritytable[i*2 + 1] = priority;
		offset++;
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		dcbcfg->etsrec.tcbwtable[i] = buf[offset++];

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		dcbcfg->etsrec.tsatable[i] = buf[offset++];
}

 
static void i40e_parse_ieee_pfccfg_tlv(struct i40e_lldp_org_tlv *tlv,
				       struct i40e_dcbx_config *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	 
	dcbcfg->pfc.willing = (u8)((buf[0] & I40E_IEEE_PFC_WILLING_MASK) >>
				   I40E_IEEE_PFC_WILLING_SHIFT);
	dcbcfg->pfc.mbc = (u8)((buf[0] & I40E_IEEE_PFC_MBC_MASK) >>
			       I40E_IEEE_PFC_MBC_SHIFT);
	dcbcfg->pfc.pfccap = (u8)((buf[0] & I40E_IEEE_PFC_CAP_MASK) >>
				  I40E_IEEE_PFC_CAP_SHIFT);
	dcbcfg->pfc.pfcenable = buf[1];
}

 
static void i40e_parse_ieee_app_tlv(struct i40e_lldp_org_tlv *tlv,
				    struct i40e_dcbx_config *dcbcfg)
{
	u16 typelength;
	u16 offset = 0;
	u16 length;
	int i = 0;
	u8 *buf;

	typelength = ntohs(tlv->typelength);
	length = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
		       I40E_LLDP_TLV_LEN_SHIFT);
	buf = tlv->tlvinfo;

	 
	length -= (sizeof(tlv->ouisubtype) + 1);

	 
	offset++;

	 
	while (offset < length) {
		dcbcfg->app[i].priority = (u8)((buf[offset] &
						I40E_IEEE_APP_PRIO_MASK) >>
					       I40E_IEEE_APP_PRIO_SHIFT);
		dcbcfg->app[i].selector = (u8)((buf[offset] &
						I40E_IEEE_APP_SEL_MASK) >>
					       I40E_IEEE_APP_SEL_SHIFT);
		dcbcfg->app[i].protocolid = (buf[offset + 1] << 0x8) |
					     buf[offset + 2];
		 
		offset += 3;
		i++;
		if (i >= I40E_DCBX_MAX_APPS)
			break;
	}

	dcbcfg->numapps = i;
}

 
static void i40e_parse_ieee_tlv(struct i40e_lldp_org_tlv *tlv,
				struct i40e_dcbx_config *dcbcfg)
{
	u32 ouisubtype;
	u8 subtype;

	ouisubtype = ntohl(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & I40E_LLDP_TLV_SUBTYPE_MASK) >>
		       I40E_LLDP_TLV_SUBTYPE_SHIFT);
	switch (subtype) {
	case I40E_IEEE_SUBTYPE_ETS_CFG:
		i40e_parse_ieee_etscfg_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_SUBTYPE_ETS_REC:
		i40e_parse_ieee_etsrec_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_SUBTYPE_PFC_CFG:
		i40e_parse_ieee_pfccfg_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_SUBTYPE_APP_PRI:
		i40e_parse_ieee_app_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

 
static void i40e_parse_cee_pgcfg_tlv(struct i40e_cee_feat_tlv *tlv,
				     struct i40e_dcbx_config *dcbcfg)
{
	struct i40e_dcb_ets_config *etscfg;
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	u8 priority;
	int i;

	etscfg = &dcbcfg->etscfg;

	if (tlv->en_will_err & I40E_CEE_FEAT_TLV_WILLING_MASK)
		etscfg->willing = 1;

	etscfg->cbs = 0;
	 
	for (i = 0; i < 4; i++) {
		priority = (u8)((buf[offset] & I40E_CEE_PGID_PRIO_1_MASK) >>
				 I40E_CEE_PGID_PRIO_1_SHIFT);
		etscfg->prioritytable[i * 2] =  priority;
		priority = (u8)((buf[offset] & I40E_CEE_PGID_PRIO_0_MASK) >>
				 I40E_CEE_PGID_PRIO_0_SHIFT);
		etscfg->prioritytable[i * 2 + 1] = priority;
		offset++;
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		etscfg->tcbwtable[i] = buf[offset++];

	 
	etscfg->maxtcs = buf[offset];
}

 
static void i40e_parse_cee_pfccfg_tlv(struct i40e_cee_feat_tlv *tlv,
				      struct i40e_dcbx_config *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	if (tlv->en_will_err & I40E_CEE_FEAT_TLV_WILLING_MASK)
		dcbcfg->pfc.willing = 1;

	 
	dcbcfg->pfc.pfcenable = buf[0];
	dcbcfg->pfc.pfccap = buf[1];
}

 
static void i40e_parse_cee_app_tlv(struct i40e_cee_feat_tlv *tlv,
				   struct i40e_dcbx_config *dcbcfg)
{
	u16 length, typelength, offset = 0;
	struct i40e_cee_app_prio *app;
	u8 i;

	typelength = ntohs(tlv->hdr.typelen);
	length = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
		       I40E_LLDP_TLV_LEN_SHIFT);

	dcbcfg->numapps = length / sizeof(*app);

	if (!dcbcfg->numapps)
		return;
	if (dcbcfg->numapps > I40E_DCBX_MAX_APPS)
		dcbcfg->numapps = I40E_DCBX_MAX_APPS;

	for (i = 0; i < dcbcfg->numapps; i++) {
		u8 up, selector;

		app = (struct i40e_cee_app_prio *)(tlv->tlvinfo + offset);
		for (up = 0; up < I40E_MAX_USER_PRIORITY; up++) {
			if (app->prio_map & BIT(up))
				break;
		}
		dcbcfg->app[i].priority = up;

		 
		selector = (app->upper_oui_sel & I40E_CEE_APP_SELECTOR_MASK);
		switch (selector) {
		case I40E_CEE_APP_SEL_ETHTYPE:
			dcbcfg->app[i].selector = I40E_APP_SEL_ETHTYPE;
			break;
		case I40E_CEE_APP_SEL_TCPIP:
			dcbcfg->app[i].selector = I40E_APP_SEL_TCPIP;
			break;
		default:
			 
			dcbcfg->app[i].selector = selector;
		}

		dcbcfg->app[i].protocolid = ntohs(app->protocol);
		 
		offset += sizeof(*app);
	}
}

 
static void i40e_parse_cee_tlv(struct i40e_lldp_org_tlv *tlv,
			       struct i40e_dcbx_config *dcbcfg)
{
	u16 len, tlvlen, sublen, typelength;
	struct i40e_cee_feat_tlv *sub_tlv;
	u8 subtype, feat_tlv_count = 0;
	u32 ouisubtype;

	ouisubtype = ntohl(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & I40E_LLDP_TLV_SUBTYPE_MASK) >>
		       I40E_LLDP_TLV_SUBTYPE_SHIFT);
	 
	if (subtype != I40E_CEE_DCBX_TYPE)
		return;

	typelength = ntohs(tlv->typelength);
	tlvlen = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
			I40E_LLDP_TLV_LEN_SHIFT);
	len = sizeof(tlv->typelength) + sizeof(ouisubtype) +
	      sizeof(struct i40e_cee_ctrl_tlv);
	 
	if (tlvlen <= len)
		return;

	sub_tlv = (struct i40e_cee_feat_tlv *)((char *)tlv + len);
	while (feat_tlv_count < I40E_CEE_MAX_FEAT_TYPE) {
		typelength = ntohs(sub_tlv->hdr.typelen);
		sublen = (u16)((typelength &
				I40E_LLDP_TLV_LEN_MASK) >>
				I40E_LLDP_TLV_LEN_SHIFT);
		subtype = (u8)((typelength & I40E_LLDP_TLV_TYPE_MASK) >>
				I40E_LLDP_TLV_TYPE_SHIFT);
		switch (subtype) {
		case I40E_CEE_SUBTYPE_PG_CFG:
			i40e_parse_cee_pgcfg_tlv(sub_tlv, dcbcfg);
			break;
		case I40E_CEE_SUBTYPE_PFC_CFG:
			i40e_parse_cee_pfccfg_tlv(sub_tlv, dcbcfg);
			break;
		case I40E_CEE_SUBTYPE_APP_PRI:
			i40e_parse_cee_app_tlv(sub_tlv, dcbcfg);
			break;
		default:
			return;  
		}
		feat_tlv_count++;
		 
		sub_tlv = (struct i40e_cee_feat_tlv *)((char *)sub_tlv +
						sizeof(sub_tlv->hdr.typelen) +
						sublen);
	}
}

 
static void i40e_parse_org_tlv(struct i40e_lldp_org_tlv *tlv,
			       struct i40e_dcbx_config *dcbcfg)
{
	u32 ouisubtype;
	u32 oui;

	ouisubtype = ntohl(tlv->ouisubtype);
	oui = (u32)((ouisubtype & I40E_LLDP_TLV_OUI_MASK) >>
		    I40E_LLDP_TLV_OUI_SHIFT);
	switch (oui) {
	case I40E_IEEE_8021QAZ_OUI:
		i40e_parse_ieee_tlv(tlv, dcbcfg);
		break;
	case I40E_CEE_DCBX_OUI:
		i40e_parse_cee_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

 
int i40e_lldp_to_dcb_config(u8 *lldpmib,
			    struct i40e_dcbx_config *dcbcfg)
{
	struct i40e_lldp_org_tlv *tlv;
	u16 typelength;
	u16 offset = 0;
	int ret = 0;
	u16 length;
	u16 type;

	if (!lldpmib || !dcbcfg)
		return -EINVAL;

	 
	lldpmib += ETH_HLEN;
	tlv = (struct i40e_lldp_org_tlv *)lldpmib;
	while (1) {
		typelength = ntohs(tlv->typelength);
		type = (u16)((typelength & I40E_LLDP_TLV_TYPE_MASK) >>
			     I40E_LLDP_TLV_TYPE_SHIFT);
		length = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
			       I40E_LLDP_TLV_LEN_SHIFT);
		offset += sizeof(typelength) + length;

		 
		if ((type == I40E_TLV_TYPE_END) || (offset > I40E_LLDPDU_SIZE))
			break;

		switch (type) {
		case I40E_TLV_TYPE_ORG:
			i40e_parse_org_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}

		 
		tlv = (struct i40e_lldp_org_tlv *)((char *)tlv +
						    sizeof(tlv->typelength) +
						    length);
	}

	return ret;
}

 
int i40e_aq_get_dcb_config(struct i40e_hw *hw, u8 mib_type,
			   u8 bridgetype,
			   struct i40e_dcbx_config *dcbcfg)
{
	struct i40e_virt_mem mem;
	int ret = 0;
	u8 *lldpmib;

	 
	ret = i40e_allocate_virt_mem(hw, &mem, I40E_LLDPDU_SIZE);
	if (ret)
		return ret;

	lldpmib = (u8 *)mem.va;
	ret = i40e_aq_get_lldp_mib(hw, bridgetype, mib_type,
				   (void *)lldpmib, I40E_LLDPDU_SIZE,
				   NULL, NULL, NULL);
	if (ret)
		goto free_mem;

	 
	ret = i40e_lldp_to_dcb_config(lldpmib, dcbcfg);

free_mem:
	i40e_free_virt_mem(hw, &mem);
	return ret;
}

 
static void i40e_cee_to_dcb_v1_config(
			struct i40e_aqc_get_cee_dcb_cfg_v1_resp *cee_cfg,
			struct i40e_dcbx_config *dcbcfg)
{
	u16 status, tlv_status = le16_to_cpu(cee_cfg->tlv_status);
	u16 app_prio = le16_to_cpu(cee_cfg->oper_app_prio);
	u8 i, tc, err;

	 
	dcbcfg->etscfg.maxtcs = cee_cfg->oper_num_tc;

	 
	for (i = 0; i < 4; i++) {
		tc = (u8)((cee_cfg->oper_prio_tc[i] &
			 I40E_CEE_PGID_PRIO_0_MASK) >>
			 I40E_CEE_PGID_PRIO_0_SHIFT);
		dcbcfg->etscfg.prioritytable[i * 2] =  tc;
		tc = (u8)((cee_cfg->oper_prio_tc[i] &
			 I40E_CEE_PGID_PRIO_1_MASK) >>
			 I40E_CEE_PGID_PRIO_1_SHIFT);
		dcbcfg->etscfg.prioritytable[i*2 + 1] = tc;
	}

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		dcbcfg->etscfg.tcbwtable[i] = cee_cfg->oper_tc_bw[i];

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (dcbcfg->etscfg.prioritytable[i] == I40E_CEE_PGID_STRICT) {
			 
			dcbcfg->etscfg.prioritytable[i] =
						cee_cfg->oper_num_tc - 1;
			dcbcfg->etscfg.tsatable[i] = I40E_IEEE_TSA_STRICT;
		} else {
			dcbcfg->etscfg.tsatable[i] = I40E_IEEE_TSA_ETS;
		}
	}

	 
	dcbcfg->pfc.pfcenable = cee_cfg->oper_pfc_en;
	dcbcfg->pfc.pfccap = I40E_MAX_TRAFFIC_CLASS;

	status = (tlv_status & I40E_AQC_CEE_APP_STATUS_MASK) >>
		  I40E_AQC_CEE_APP_STATUS_SHIFT;
	err = (status & I40E_TLV_STATUS_ERR) ? 1 : 0;
	 
	if (!err) {
		 
		dcbcfg->numapps = I40E_CEE_OPER_MAX_APPS;

		 
		dcbcfg->app[0].priority =
			(app_prio & I40E_AQC_CEE_APP_FCOE_MASK) >>
			 I40E_AQC_CEE_APP_FCOE_SHIFT;
		dcbcfg->app[0].selector = I40E_APP_SEL_ETHTYPE;
		dcbcfg->app[0].protocolid = I40E_APP_PROTOID_FCOE;

		 
		dcbcfg->app[1].priority =
			(app_prio & I40E_AQC_CEE_APP_ISCSI_MASK) >>
			 I40E_AQC_CEE_APP_ISCSI_SHIFT;
		dcbcfg->app[1].selector = I40E_APP_SEL_TCPIP;
		dcbcfg->app[1].protocolid = I40E_APP_PROTOID_ISCSI;

		 
		dcbcfg->app[2].priority =
			(app_prio & I40E_AQC_CEE_APP_FIP_MASK) >>
			 I40E_AQC_CEE_APP_FIP_SHIFT;
		dcbcfg->app[2].selector = I40E_APP_SEL_ETHTYPE;
		dcbcfg->app[2].protocolid = I40E_APP_PROTOID_FIP;
	}
}

 
static void i40e_cee_to_dcb_config(
				struct i40e_aqc_get_cee_dcb_cfg_resp *cee_cfg,
				struct i40e_dcbx_config *dcbcfg)
{
	u32 status, tlv_status = le32_to_cpu(cee_cfg->tlv_status);
	u16 app_prio = le16_to_cpu(cee_cfg->oper_app_prio);
	u8 i, tc, err, sync, oper;

	 
	dcbcfg->etscfg.maxtcs = cee_cfg->oper_num_tc;

	 
	for (i = 0; i < 4; i++) {
		tc = (u8)((cee_cfg->oper_prio_tc[i] &
			 I40E_CEE_PGID_PRIO_0_MASK) >>
			 I40E_CEE_PGID_PRIO_0_SHIFT);
		dcbcfg->etscfg.prioritytable[i * 2] =  tc;
		tc = (u8)((cee_cfg->oper_prio_tc[i] &
			 I40E_CEE_PGID_PRIO_1_MASK) >>
			 I40E_CEE_PGID_PRIO_1_SHIFT);
		dcbcfg->etscfg.prioritytable[i * 2 + 1] = tc;
	}

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		dcbcfg->etscfg.tcbwtable[i] = cee_cfg->oper_tc_bw[i];

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (dcbcfg->etscfg.prioritytable[i] == I40E_CEE_PGID_STRICT) {
			 
			dcbcfg->etscfg.prioritytable[i] =
						cee_cfg->oper_num_tc - 1;
			dcbcfg->etscfg.tsatable[i] = I40E_IEEE_TSA_STRICT;
		} else {
			dcbcfg->etscfg.tsatable[i] = I40E_IEEE_TSA_ETS;
		}
	}

	 
	dcbcfg->pfc.pfcenable = cee_cfg->oper_pfc_en;
	dcbcfg->pfc.pfccap = I40E_MAX_TRAFFIC_CLASS;

	i = 0;
	status = (tlv_status & I40E_AQC_CEE_FCOE_STATUS_MASK) >>
		  I40E_AQC_CEE_FCOE_STATUS_SHIFT;
	err = (status & I40E_TLV_STATUS_ERR) ? 1 : 0;
	sync = (status & I40E_TLV_STATUS_SYNC) ? 1 : 0;
	oper = (status & I40E_TLV_STATUS_OPER) ? 1 : 0;
	 
	if (!err && sync && oper) {
		 
		dcbcfg->app[i].priority =
			(app_prio & I40E_AQC_CEE_APP_FCOE_MASK) >>
			 I40E_AQC_CEE_APP_FCOE_SHIFT;
		dcbcfg->app[i].selector = I40E_APP_SEL_ETHTYPE;
		dcbcfg->app[i].protocolid = I40E_APP_PROTOID_FCOE;
		i++;
	}

	status = (tlv_status & I40E_AQC_CEE_ISCSI_STATUS_MASK) >>
		  I40E_AQC_CEE_ISCSI_STATUS_SHIFT;
	err = (status & I40E_TLV_STATUS_ERR) ? 1 : 0;
	sync = (status & I40E_TLV_STATUS_SYNC) ? 1 : 0;
	oper = (status & I40E_TLV_STATUS_OPER) ? 1 : 0;
	 
	if (!err && sync && oper) {
		 
		dcbcfg->app[i].priority =
			(app_prio & I40E_AQC_CEE_APP_ISCSI_MASK) >>
			 I40E_AQC_CEE_APP_ISCSI_SHIFT;
		dcbcfg->app[i].selector = I40E_APP_SEL_TCPIP;
		dcbcfg->app[i].protocolid = I40E_APP_PROTOID_ISCSI;
		i++;
	}

	status = (tlv_status & I40E_AQC_CEE_FIP_STATUS_MASK) >>
		  I40E_AQC_CEE_FIP_STATUS_SHIFT;
	err = (status & I40E_TLV_STATUS_ERR) ? 1 : 0;
	sync = (status & I40E_TLV_STATUS_SYNC) ? 1 : 0;
	oper = (status & I40E_TLV_STATUS_OPER) ? 1 : 0;
	 
	if (!err && sync && oper) {
		 
		dcbcfg->app[i].priority =
			(app_prio & I40E_AQC_CEE_APP_FIP_MASK) >>
			 I40E_AQC_CEE_APP_FIP_SHIFT;
		dcbcfg->app[i].selector = I40E_APP_SEL_ETHTYPE;
		dcbcfg->app[i].protocolid = I40E_APP_PROTOID_FIP;
		i++;
	}
	dcbcfg->numapps = i;
}

 
static int i40e_get_ieee_dcb_config(struct i40e_hw *hw)
{
	int ret = 0;

	 
	hw->local_dcbx_config.dcbx_mode = I40E_DCBX_MODE_IEEE;
	 
	ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_LOCAL, 0,
				     &hw->local_dcbx_config);
	if (ret)
		goto out;

	 
	ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_REMOTE,
				     I40E_AQ_LLDP_BRIDGE_TYPE_NEAREST_BRIDGE,
				     &hw->remote_dcbx_config);
	 
	if (hw->aq.asq_last_status == I40E_AQ_RC_ENOENT)
		ret = 0;

out:
	return ret;
}

 
int i40e_get_dcb_config(struct i40e_hw *hw)
{
	struct i40e_aqc_get_cee_dcb_cfg_v1_resp cee_v1_cfg;
	struct i40e_aqc_get_cee_dcb_cfg_resp cee_cfg;
	int ret = 0;

	 
	if ((hw->mac.type == I40E_MAC_XL710) &&
	    (((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver < 33)) ||
	      (hw->aq.fw_maj_ver < 4)))
		return i40e_get_ieee_dcb_config(hw);

	 
	if ((hw->mac.type == I40E_MAC_XL710) &&
	    ((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver == 33))) {
		ret = i40e_aq_get_cee_dcb_config(hw, &cee_v1_cfg,
						 sizeof(cee_v1_cfg), NULL);
		if (!ret) {
			 
			hw->local_dcbx_config.dcbx_mode = I40E_DCBX_MODE_CEE;
			hw->local_dcbx_config.tlv_status =
					le16_to_cpu(cee_v1_cfg.tlv_status);
			i40e_cee_to_dcb_v1_config(&cee_v1_cfg,
						  &hw->local_dcbx_config);
		}
	} else {
		ret = i40e_aq_get_cee_dcb_config(hw, &cee_cfg,
						 sizeof(cee_cfg), NULL);
		if (!ret) {
			 
			hw->local_dcbx_config.dcbx_mode = I40E_DCBX_MODE_CEE;
			hw->local_dcbx_config.tlv_status =
					le32_to_cpu(cee_cfg.tlv_status);
			i40e_cee_to_dcb_config(&cee_cfg,
					       &hw->local_dcbx_config);
		}
	}

	 
	if (hw->aq.asq_last_status == I40E_AQ_RC_ENOENT)
		return i40e_get_ieee_dcb_config(hw);

	if (ret)
		goto out;

	 
	ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_LOCAL, 0,
				     &hw->desired_dcbx_config);
	if (ret)
		goto out;

	 
	ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_REMOTE,
				     I40E_AQ_LLDP_BRIDGE_TYPE_NEAREST_BRIDGE,
				     &hw->remote_dcbx_config);
	 
	if (hw->aq.asq_last_status == I40E_AQ_RC_ENOENT)
		ret = 0;

out:
	return ret;
}

 
int i40e_init_dcb(struct i40e_hw *hw, bool enable_mib_change)
{
	struct i40e_lldp_variables lldp_cfg;
	u8 adminstatus = 0;
	int ret = 0;

	if (!hw->func_caps.dcb)
		return -EOPNOTSUPP;

	 
	if (hw->flags & I40E_HW_FLAG_FW_LLDP_PERSISTENT) {
		u8 offset = 0;

		if (hw->mac.type == I40E_MAC_XL710)
			offset = I40E_LLDP_CURRENT_STATUS_XL710_OFFSET;
		else if (hw->mac.type == I40E_MAC_X722)
			offset = I40E_LLDP_CURRENT_STATUS_X722_OFFSET;
		else
			return -EOPNOTSUPP;

		ret = i40e_read_nvm_module_data(hw,
						I40E_SR_EMP_SR_SETTINGS_PTR,
						offset,
						I40E_LLDP_CURRENT_STATUS_OFFSET,
						I40E_LLDP_CURRENT_STATUS_SIZE,
						&lldp_cfg.adminstatus);
	} else {
		ret = i40e_read_lldp_cfg(hw, &lldp_cfg);
	}
	if (ret)
		return -EBUSY;

	 
	adminstatus = lldp_cfg.adminstatus >> (hw->port * 4);
	adminstatus &= 0xF;

	 
	if (!adminstatus) {
		hw->dcbx_status = I40E_DCBX_STATUS_DISABLED;
		return -EBUSY;
	}

	 
	ret = i40e_get_dcbx_status(hw, &hw->dcbx_status);
	if (ret)
		return ret;

	 
	if (hw->dcbx_status == I40E_DCBX_STATUS_DONE ||
	    hw->dcbx_status == I40E_DCBX_STATUS_IN_PROGRESS) {
		 
		ret = i40e_get_dcb_config(hw);
		if (ret)
			return ret;
	} else if (hw->dcbx_status == I40E_DCBX_STATUS_DISABLED) {
		return -EBUSY;
	}

	 
	if (enable_mib_change)
		ret = i40e_aq_cfg_lldp_mib_change_event(hw, true, NULL);

	return ret;
}

 
int
i40e_get_fw_lldp_status(struct i40e_hw *hw,
			enum i40e_get_fw_lldp_status_resp *lldp_status)
{
	struct i40e_virt_mem mem;
	u8 *lldpmib;
	int ret;

	if (!lldp_status)
		return -EINVAL;

	 
	ret = i40e_allocate_virt_mem(hw, &mem, I40E_LLDPDU_SIZE);
	if (ret)
		return ret;

	lldpmib = (u8 *)mem.va;
	ret = i40e_aq_get_lldp_mib(hw, 0, 0, (void *)lldpmib,
				   I40E_LLDPDU_SIZE, NULL, NULL, NULL);

	if (!ret) {
		*lldp_status = I40E_GET_FW_LLDP_STATUS_ENABLED;
	} else if (hw->aq.asq_last_status == I40E_AQ_RC_ENOENT) {
		 
		*lldp_status = I40E_GET_FW_LLDP_STATUS_ENABLED;
		ret = 0;
	} else if (hw->aq.asq_last_status == I40E_AQ_RC_EPERM) {
		*lldp_status = I40E_GET_FW_LLDP_STATUS_DISABLED;
		ret = 0;
	}

	i40e_free_virt_mem(hw, &mem);
	return ret;
}

 
static void i40e_add_ieee_ets_tlv(struct i40e_lldp_org_tlv *tlv,
				  struct i40e_dcbx_config *dcbcfg)
{
	u8 priority0, priority1, maxtcwilling = 0;
	struct i40e_dcb_ets_config *etscfg;
	u16 offset = 0, typelength, i;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;

	typelength = (u16)((I40E_TLV_TYPE_ORG << I40E_LLDP_TLV_TYPE_SHIFT) |
			I40E_IEEE_ETS_TLV_LENGTH);
	tlv->typelength = htons(typelength);

	ouisubtype = (u32)((I40E_IEEE_8021QAZ_OUI << I40E_LLDP_TLV_OUI_SHIFT) |
			I40E_IEEE_SUBTYPE_ETS_CFG);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	etscfg = &dcbcfg->etscfg;
	if (etscfg->willing)
		maxtcwilling = BIT(I40E_IEEE_ETS_WILLING_SHIFT);
	maxtcwilling |= etscfg->maxtcs & I40E_IEEE_ETS_MAXTC_MASK;
	buf[offset] = maxtcwilling;

	 
	offset++;

	 
	for (i = 0; i < 4; i++) {
		priority0 = etscfg->prioritytable[i * 2] & 0xF;
		priority1 = etscfg->prioritytable[i * 2 + 1] & 0xF;
		buf[offset] = (priority0 << I40E_IEEE_ETS_PRIO_1_SHIFT) |
				priority1;
		offset++;
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		buf[offset++] = etscfg->tcbwtable[i];

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		buf[offset++] = etscfg->tsatable[i];
}

 
static void i40e_add_ieee_etsrec_tlv(struct i40e_lldp_org_tlv *tlv,
				     struct i40e_dcbx_config *dcbcfg)
{
	struct i40e_dcb_ets_config *etsrec;
	u16 offset = 0, typelength, i;
	u8 priority0, priority1;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;

	typelength = (u16)((I40E_TLV_TYPE_ORG << I40E_LLDP_TLV_TYPE_SHIFT) |
			I40E_IEEE_ETS_TLV_LENGTH);
	tlv->typelength = htons(typelength);

	ouisubtype = (u32)((I40E_IEEE_8021QAZ_OUI << I40E_LLDP_TLV_OUI_SHIFT) |
			I40E_IEEE_SUBTYPE_ETS_REC);
	tlv->ouisubtype = htonl(ouisubtype);

	etsrec = &dcbcfg->etsrec;
	 
	 
	offset++;

	 
	for (i = 0; i < 4; i++) {
		priority0 = etsrec->prioritytable[i * 2] & 0xF;
		priority1 = etsrec->prioritytable[i * 2 + 1] & 0xF;
		buf[offset] = (priority0 << I40E_IEEE_ETS_PRIO_1_SHIFT) |
				priority1;
		offset++;
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		buf[offset++] = etsrec->tcbwtable[i];

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		buf[offset++] = etsrec->tsatable[i];
}

 
static void i40e_add_ieee_pfc_tlv(struct i40e_lldp_org_tlv *tlv,
				  struct i40e_dcbx_config *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;
	u16 typelength;

	typelength = (u16)((I40E_TLV_TYPE_ORG << I40E_LLDP_TLV_TYPE_SHIFT) |
			I40E_IEEE_PFC_TLV_LENGTH);
	tlv->typelength = htons(typelength);

	ouisubtype = (u32)((I40E_IEEE_8021QAZ_OUI << I40E_LLDP_TLV_OUI_SHIFT) |
			I40E_IEEE_SUBTYPE_PFC_CFG);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	if (dcbcfg->pfc.willing)
		buf[0] = BIT(I40E_IEEE_PFC_WILLING_SHIFT);

	if (dcbcfg->pfc.mbc)
		buf[0] |= BIT(I40E_IEEE_PFC_MBC_SHIFT);

	buf[0] |= dcbcfg->pfc.pfccap & 0xF;
	buf[1] = dcbcfg->pfc.pfcenable;
}

 
static void i40e_add_ieee_app_pri_tlv(struct i40e_lldp_org_tlv *tlv,
				      struct i40e_dcbx_config *dcbcfg)
{
	u16 typelength, length, offset = 0;
	u8 priority, selector, i = 0;
	u8 *buf = tlv->tlvinfo;
	u32 ouisubtype;

	 
	if (dcbcfg->numapps == 0)
		return;
	ouisubtype = (u32)((I40E_IEEE_8021QAZ_OUI << I40E_LLDP_TLV_OUI_SHIFT) |
			I40E_IEEE_SUBTYPE_APP_PRI);
	tlv->ouisubtype = htonl(ouisubtype);

	 
	offset++;
	 
	while (i < dcbcfg->numapps) {
		priority = dcbcfg->app[i].priority & 0x7;
		selector = dcbcfg->app[i].selector & 0x7;
		buf[offset] = (priority << I40E_IEEE_APP_PRIO_SHIFT) | selector;
		buf[offset + 1] = (dcbcfg->app[i].protocolid >> 0x8) & 0xFF;
		buf[offset + 2] =  dcbcfg->app[i].protocolid & 0xFF;
		 
		offset += 3;
		i++;
		if (i >= I40E_DCBX_MAX_APPS)
			break;
	}
	 
	length = sizeof(tlv->ouisubtype) + 1 + (i * 3);
	typelength = (u16)((I40E_TLV_TYPE_ORG << I40E_LLDP_TLV_TYPE_SHIFT) |
		(length & 0x1FF));
	tlv->typelength = htons(typelength);
}

 
static void i40e_add_dcb_tlv(struct i40e_lldp_org_tlv *tlv,
			     struct i40e_dcbx_config *dcbcfg,
			     u16 tlvid)
{
	switch (tlvid) {
	case I40E_IEEE_TLV_ID_ETS_CFG:
		i40e_add_ieee_ets_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_TLV_ID_ETS_REC:
		i40e_add_ieee_etsrec_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_TLV_ID_PFC_CFG:
		i40e_add_ieee_pfc_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_TLV_ID_APP_PRI:
		i40e_add_ieee_app_pri_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

 
int i40e_set_dcb_config(struct i40e_hw *hw)
{
	struct i40e_dcbx_config *dcbcfg;
	struct i40e_virt_mem mem;
	u8 mib_type, *lldpmib;
	u16 miblen;
	int ret;

	 
	dcbcfg = &hw->local_dcbx_config;
	 
	ret = i40e_allocate_virt_mem(hw, &mem, I40E_LLDPDU_SIZE);
	if (ret)
		return ret;

	mib_type = SET_LOCAL_MIB_AC_TYPE_LOCAL_MIB;
	if (dcbcfg->app_mode == I40E_DCBX_APPS_NON_WILLING) {
		mib_type |= SET_LOCAL_MIB_AC_TYPE_NON_WILLING_APPS <<
			    SET_LOCAL_MIB_AC_TYPE_NON_WILLING_APPS_SHIFT;
	}
	lldpmib = (u8 *)mem.va;
	i40e_dcb_config_to_lldp(lldpmib, &miblen, dcbcfg);
	ret = i40e_aq_set_lldp_mib(hw, mib_type, (void *)lldpmib, miblen, NULL);

	i40e_free_virt_mem(hw, &mem);
	return ret;
}

 
int i40e_dcb_config_to_lldp(u8 *lldpmib, u16 *miblen,
			    struct i40e_dcbx_config *dcbcfg)
{
	u16 length, offset = 0, tlvid, typelength;
	struct i40e_lldp_org_tlv *tlv;

	tlv = (struct i40e_lldp_org_tlv *)lldpmib;
	tlvid = I40E_TLV_ID_START;
	do {
		i40e_add_dcb_tlv(tlv, dcbcfg, tlvid++);
		typelength = ntohs(tlv->typelength);
		length = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
				I40E_LLDP_TLV_LEN_SHIFT);
		if (length)
			offset += length + I40E_IEEE_TLV_HEADER_LENGTH;
		 
		if (tlvid >= I40E_TLV_ID_END_OF_LLDPPDU ||
		    offset >= I40E_LLDPDU_SIZE)
			break;
		 
		if (length)
			tlv = (struct i40e_lldp_org_tlv *)((char *)tlv +
			      sizeof(tlv->typelength) + length);
	} while (tlvid < I40E_TLV_ID_END_OF_LLDPPDU);
	*miblen = offset;
	return 0;
}

 
void i40e_dcb_hw_rx_fifo_config(struct i40e_hw *hw,
				enum i40e_dcb_arbiter_mode ets_mode,
				enum i40e_dcb_arbiter_mode non_ets_mode,
				u32 max_exponent,
				u8 lltc_map)
{
	u32 reg = rd32(hw, I40E_PRTDCB_RETSC);

	reg &= ~I40E_PRTDCB_RETSC_ETS_MODE_MASK;
	reg |= ((u32)ets_mode << I40E_PRTDCB_RETSC_ETS_MODE_SHIFT) &
		I40E_PRTDCB_RETSC_ETS_MODE_MASK;

	reg &= ~I40E_PRTDCB_RETSC_NON_ETS_MODE_MASK;
	reg |= ((u32)non_ets_mode << I40E_PRTDCB_RETSC_NON_ETS_MODE_SHIFT) &
		I40E_PRTDCB_RETSC_NON_ETS_MODE_MASK;

	reg &= ~I40E_PRTDCB_RETSC_ETS_MAX_EXP_MASK;
	reg |= (max_exponent << I40E_PRTDCB_RETSC_ETS_MAX_EXP_SHIFT) &
		I40E_PRTDCB_RETSC_ETS_MAX_EXP_MASK;

	reg &= ~I40E_PRTDCB_RETSC_LLTC_MASK;
	reg |= (lltc_map << I40E_PRTDCB_RETSC_LLTC_SHIFT) &
		I40E_PRTDCB_RETSC_LLTC_MASK;
	wr32(hw, I40E_PRTDCB_RETSC, reg);
}

 
void i40e_dcb_hw_rx_cmd_monitor_config(struct i40e_hw *hw,
				       u8 num_tc, u8 num_ports)
{
	u32 threshold;
	u32 fifo_size;
	u32 reg;

	 
	switch (num_ports) {
	case 1:
		threshold = I40E_DCB_1_PORT_THRESHOLD;
		fifo_size = I40E_DCB_1_PORT_FIFO_SIZE;
		break;
	case 2:
		if (num_tc > 4) {
			threshold = I40E_DCB_2_PORT_THRESHOLD_HIGH_NUM_TC;
			fifo_size = I40E_DCB_2_PORT_FIFO_SIZE_HIGH_NUM_TC;
		} else {
			threshold = I40E_DCB_2_PORT_THRESHOLD_LOW_NUM_TC;
			fifo_size = I40E_DCB_2_PORT_FIFO_SIZE_LOW_NUM_TC;
		}
		break;
	case 4:
		if (num_tc > 4) {
			threshold = I40E_DCB_4_PORT_THRESHOLD_HIGH_NUM_TC;
			fifo_size = I40E_DCB_4_PORT_FIFO_SIZE_HIGH_NUM_TC;
		} else {
			threshold = I40E_DCB_4_PORT_THRESHOLD_LOW_NUM_TC;
			fifo_size = I40E_DCB_4_PORT_FIFO_SIZE_LOW_NUM_TC;
		}
		break;
	default:
		i40e_debug(hw, I40E_DEBUG_DCB, "Invalid num_ports %u.\n",
			   (u32)num_ports);
		return;
	}

	 
	reg = rd32(hw, I40E_PRT_SWR_PM_THR);
	reg &= ~I40E_PRT_SWR_PM_THR_THRESHOLD_MASK;
	reg |= (threshold << I40E_PRT_SWR_PM_THR_THRESHOLD_SHIFT) &
		I40E_PRT_SWR_PM_THR_THRESHOLD_MASK;
	wr32(hw, I40E_PRT_SWR_PM_THR, reg);

	reg = rd32(hw, I40E_PRTDCB_RPPMC);
	reg &= ~I40E_PRTDCB_RPPMC_RX_FIFO_SIZE_MASK;
	reg |= (fifo_size << I40E_PRTDCB_RPPMC_RX_FIFO_SIZE_SHIFT) &
		I40E_PRTDCB_RPPMC_RX_FIFO_SIZE_MASK;
	wr32(hw, I40E_PRTDCB_RPPMC, reg);
}

 
void i40e_dcb_hw_pfc_config(struct i40e_hw *hw,
			    u8 pfc_en, u8 *prio_tc)
{
	u16 refresh_time = (u16)I40E_DEFAULT_PAUSE_TIME / 2;
	u32 link_speed = hw->phy.link_info.link_speed;
	u8 first_pfc_prio = 0;
	u8 num_pfc_tc = 0;
	u8 tc2pfc = 0;
	u32 reg;
	u8 i;

	 
	for (i = 0; i < I40E_MAX_USER_PRIORITY; i++) {
		if (pfc_en & BIT(i)) {
			if (!first_pfc_prio)
				first_pfc_prio = i;
			 
			tc2pfc |= BIT(prio_tc[i]);
			num_pfc_tc++;
		}
	}

	switch (link_speed) {
	case I40E_LINK_SPEED_10GB:
		reg = rd32(hw, I40E_PRTDCB_MFLCN);
		reg |= BIT(I40E_PRTDCB_MFLCN_DPF_SHIFT) &
			I40E_PRTDCB_MFLCN_DPF_MASK;
		reg &= ~I40E_PRTDCB_MFLCN_RFCE_MASK;
		reg &= ~I40E_PRTDCB_MFLCN_RPFCE_MASK;
		if (pfc_en) {
			reg |= BIT(I40E_PRTDCB_MFLCN_RPFCM_SHIFT) &
				I40E_PRTDCB_MFLCN_RPFCM_MASK;
			reg |= ((u32)pfc_en << I40E_PRTDCB_MFLCN_RPFCE_SHIFT) &
				I40E_PRTDCB_MFLCN_RPFCE_MASK;
		}
		wr32(hw, I40E_PRTDCB_MFLCN, reg);

		reg = rd32(hw, I40E_PRTDCB_FCCFG);
		reg &= ~I40E_PRTDCB_FCCFG_TFCE_MASK;
		if (pfc_en)
			reg |= (I40E_DCB_PFC_ENABLED <<
				I40E_PRTDCB_FCCFG_TFCE_SHIFT) &
				I40E_PRTDCB_FCCFG_TFCE_MASK;
		wr32(hw, I40E_PRTDCB_FCCFG, reg);

		 
		break;
	case I40E_LINK_SPEED_40GB:
		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP);
		reg &= ~I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP, reg);

		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP);
		reg &= ~I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP_MASK;
		reg |= BIT(I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP_SHIFT) &
			I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP, reg);

		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE);
		reg &= ~I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_MASK;
		reg |= ((u32)pfc_en <<
			   I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_SHIFT) &
			I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE, reg);

		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE);
		reg &= ~I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_MASK;
		reg |= ((u32)pfc_en <<
			   I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_SHIFT) &
			I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE, reg);

		for (i = 0; i < I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_MAX_INDEX; i++) {
			reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(i));
			reg &= ~I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_MASK;
			if (pfc_en) {
				reg |= ((u32)refresh_time <<
					I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_SHIFT) &
					I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_MASK;
			}
			wr32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(i), reg);
		}
		 
		break;
	}

	reg = rd32(hw, I40E_PRTDCB_TC2PFC);
	reg &= ~I40E_PRTDCB_TC2PFC_TC2PFC_MASK;
	reg |= ((u32)tc2pfc << I40E_PRTDCB_TC2PFC_TC2PFC_SHIFT) &
		I40E_PRTDCB_TC2PFC_TC2PFC_MASK;
	wr32(hw, I40E_PRTDCB_TC2PFC, reg);

	reg = rd32(hw, I40E_PRTDCB_RUP);
	reg &= ~I40E_PRTDCB_RUP_NOVLANUP_MASK;
	reg |= ((u32)first_pfc_prio << I40E_PRTDCB_RUP_NOVLANUP_SHIFT) &
		 I40E_PRTDCB_RUP_NOVLANUP_MASK;
	wr32(hw, I40E_PRTDCB_RUP, reg);

	reg = rd32(hw, I40E_PRTDCB_TDPMC);
	reg &= ~I40E_PRTDCB_TDPMC_TCPM_MODE_MASK;
	if (num_pfc_tc > I40E_DCB_PFC_FORCED_NUM_TC) {
		reg |= BIT(I40E_PRTDCB_TDPMC_TCPM_MODE_SHIFT) &
			I40E_PRTDCB_TDPMC_TCPM_MODE_MASK;
	}
	wr32(hw, I40E_PRTDCB_TDPMC, reg);

	reg = rd32(hw, I40E_PRTDCB_TCPMC);
	reg &= ~I40E_PRTDCB_TCPMC_TCPM_MODE_MASK;
	if (num_pfc_tc > I40E_DCB_PFC_FORCED_NUM_TC) {
		reg |= BIT(I40E_PRTDCB_TCPMC_TCPM_MODE_SHIFT) &
			I40E_PRTDCB_TCPMC_TCPM_MODE_MASK;
	}
	wr32(hw, I40E_PRTDCB_TCPMC, reg);
}

 
void i40e_dcb_hw_set_num_tc(struct i40e_hw *hw, u8 num_tc)
{
	u32 reg = rd32(hw, I40E_PRTDCB_GENC);

	reg &= ~I40E_PRTDCB_GENC_NUMTC_MASK;
	reg |= ((u32)num_tc << I40E_PRTDCB_GENC_NUMTC_SHIFT) &
		I40E_PRTDCB_GENC_NUMTC_MASK;
	wr32(hw, I40E_PRTDCB_GENC, reg);
}

 
u8 i40e_dcb_hw_get_num_tc(struct i40e_hw *hw)
{
	u32 reg = rd32(hw, I40E_PRTDCB_GENC);

	return (u8)((reg & I40E_PRTDCB_GENC_NUMTC_MASK) >>
		I40E_PRTDCB_GENC_NUMTC_SHIFT);
}

 
void i40e_dcb_hw_rx_ets_bw_config(struct i40e_hw *hw, u8 *bw_share,
				  u8 *mode, u8 *prio_type)
{
	u32 reg;
	u8 i;

	for (i = 0; i <= I40E_PRTDCB_RETSTCC_MAX_INDEX; i++) {
		reg = rd32(hw, I40E_PRTDCB_RETSTCC(i));
		reg &= ~(I40E_PRTDCB_RETSTCC_BWSHARE_MASK     |
			 I40E_PRTDCB_RETSTCC_UPINTC_MODE_MASK |
			 I40E_PRTDCB_RETSTCC_ETSTC_SHIFT);
		reg |= ((u32)bw_share[i] << I40E_PRTDCB_RETSTCC_BWSHARE_SHIFT) &
			 I40E_PRTDCB_RETSTCC_BWSHARE_MASK;
		reg |= ((u32)mode[i] << I40E_PRTDCB_RETSTCC_UPINTC_MODE_SHIFT) &
			 I40E_PRTDCB_RETSTCC_UPINTC_MODE_MASK;
		reg |= ((u32)prio_type[i] << I40E_PRTDCB_RETSTCC_ETSTC_SHIFT) &
			 I40E_PRTDCB_RETSTCC_ETSTC_MASK;
		wr32(hw, I40E_PRTDCB_RETSTCC(i), reg);
	}
}

 
void i40e_dcb_hw_rx_up2tc_config(struct i40e_hw *hw, u8 *prio_tc)
{
	u32 reg = rd32(hw, I40E_PRTDCB_RUP2TC);
#define I40E_UP2TC_REG(val, i) \
		(((val) << I40E_PRTDCB_RUP2TC_UP##i##TC_SHIFT) & \
		  I40E_PRTDCB_RUP2TC_UP##i##TC_MASK)

	reg |= I40E_UP2TC_REG(prio_tc[0], 0);
	reg |= I40E_UP2TC_REG(prio_tc[1], 1);
	reg |= I40E_UP2TC_REG(prio_tc[2], 2);
	reg |= I40E_UP2TC_REG(prio_tc[3], 3);
	reg |= I40E_UP2TC_REG(prio_tc[4], 4);
	reg |= I40E_UP2TC_REG(prio_tc[5], 5);
	reg |= I40E_UP2TC_REG(prio_tc[6], 6);
	reg |= I40E_UP2TC_REG(prio_tc[7], 7);

	wr32(hw, I40E_PRTDCB_RUP2TC, reg);
}

 
void i40e_dcb_hw_calculate_pool_sizes(struct i40e_hw *hw,
				      u8 num_ports, bool eee_enabled,
				      u8 pfc_en, u32 *mfs_tc,
				      struct i40e_rx_pb_config *pb_cfg)
{
	u32 pool_size[I40E_MAX_TRAFFIC_CLASS];
	u32 high_wm[I40E_MAX_TRAFFIC_CLASS];
	u32 low_wm[I40E_MAX_TRAFFIC_CLASS];
	u32 total_pool_size = 0;
	int shared_pool_size;  
	u32 port_pb_size;
	u32 mfs_max = 0;
	u32 pcirtt;
	u8 i;

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (mfs_tc[i] > mfs_max)
			mfs_max = mfs_tc[i];
	}

	pcirtt = I40E_BT2B(I40E_PCIRTT_LINK_SPEED_10G);

	 
	port_pb_size = I40E_DEVICE_RPB_SIZE / num_ports;
	if (eee_enabled)
		port_pb_size -= I40E_BT2B(I40E_EEE_TX_LPI_EXIT_TIME);
	port_pb_size -= mfs_max;

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (pfc_en & BIT(i)) {
			low_wm[i] = (I40E_DCB_WATERMARK_START_FACTOR *
				     mfs_tc[i]) + pcirtt;
			high_wm[i] = low_wm[i];
			high_wm[i] += ((mfs_max > I40E_MAX_FRAME_SIZE)
					? mfs_max : I40E_MAX_FRAME_SIZE);
			pool_size[i] = high_wm[i];
			pool_size[i] += I40E_BT2B(I40E_STD_DV_TC(mfs_max,
								mfs_tc[i]));
		} else {
			low_wm[i] = 0;
			pool_size[i] = (I40E_DCB_WATERMARK_START_FACTOR *
					mfs_tc[i]) + pcirtt;
			high_wm[i] = pool_size[i];
		}
		total_pool_size += pool_size[i];
	}

	shared_pool_size = port_pb_size - total_pool_size;
	if (shared_pool_size > 0) {
		pb_cfg->shared_pool_size = shared_pool_size;
		pb_cfg->shared_pool_high_wm = shared_pool_size;
		pb_cfg->shared_pool_low_wm = 0;
		for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
			pb_cfg->shared_pool_low_thresh[i] = 0;
			pb_cfg->shared_pool_high_thresh[i] = shared_pool_size;
			pb_cfg->tc_pool_size[i] = pool_size[i];
			pb_cfg->tc_pool_high_wm[i] = high_wm[i];
			pb_cfg->tc_pool_low_wm[i] = low_wm[i];
		}

	} else {
		i40e_debug(hw, I40E_DEBUG_DCB,
			   "The shared pool size for the port is negative %d.\n",
			   shared_pool_size);
	}
}

 
void i40e_dcb_hw_rx_pb_config(struct i40e_hw *hw,
			      struct i40e_rx_pb_config *old_pb_cfg,
			      struct i40e_rx_pb_config *new_pb_cfg)
{
	u32 old_val;
	u32 new_val;
	u32 reg;
	u8 i;

	 

	 
	old_val = old_pb_cfg->shared_pool_low_wm;
	new_val = new_pb_cfg->shared_pool_low_wm;
	if (new_val < old_val) {
		reg = rd32(hw, I40E_PRTRPB_SLW);
		reg &= ~I40E_PRTRPB_SLW_SLW_MASK;
		reg |= (new_val << I40E_PRTRPB_SLW_SLW_SHIFT) &
			I40E_PRTRPB_SLW_SLW_MASK;
		wr32(hw, I40E_PRTRPB_SLW, reg);
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_low_thresh[i];
		new_val = new_pb_cfg->shared_pool_low_thresh[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_SLT(i));
			reg &= ~I40E_PRTRPB_SLT_SLT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SLT_SLT_TCN_SHIFT) &
				I40E_PRTRPB_SLT_SLT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SLT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_low_wm[i];
		new_val = new_pb_cfg->tc_pool_low_wm[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_DLW(i));
			reg &= ~I40E_PRTRPB_DLW_DLW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DLW_DLW_TCN_SHIFT) &
				I40E_PRTRPB_DLW_DLW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DLW(i), reg);
		}
	}

	 
	old_val = old_pb_cfg->shared_pool_high_wm;
	new_val = new_pb_cfg->shared_pool_high_wm;
	if (new_val < old_val) {
		reg = rd32(hw, I40E_PRTRPB_SHW);
		reg &= ~I40E_PRTRPB_SHW_SHW_MASK;
		reg |= (new_val << I40E_PRTRPB_SHW_SHW_SHIFT) &
			I40E_PRTRPB_SHW_SHW_MASK;
		wr32(hw, I40E_PRTRPB_SHW, reg);
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_high_thresh[i];
		new_val = new_pb_cfg->shared_pool_high_thresh[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_SHT(i));
			reg &= ~I40E_PRTRPB_SHT_SHT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SHT_SHT_TCN_SHIFT) &
				I40E_PRTRPB_SHT_SHT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SHT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_high_wm[i];
		new_val = new_pb_cfg->tc_pool_high_wm[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_DHW(i));
			reg &= ~I40E_PRTRPB_DHW_DHW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DHW_DHW_TCN_SHIFT) &
				I40E_PRTRPB_DHW_DHW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DHW(i), reg);
		}
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		new_val = new_pb_cfg->tc_pool_size[i];
		reg = rd32(hw, I40E_PRTRPB_DPS(i));
		reg &= ~I40E_PRTRPB_DPS_DPS_TCN_MASK;
		reg |= (new_val << I40E_PRTRPB_DPS_DPS_TCN_SHIFT) &
			I40E_PRTRPB_DPS_DPS_TCN_MASK;
		wr32(hw, I40E_PRTRPB_DPS(i), reg);
	}

	 
	new_val = new_pb_cfg->shared_pool_size;
	reg = rd32(hw, I40E_PRTRPB_SPS);
	reg &= ~I40E_PRTRPB_SPS_SPS_MASK;
	reg |= (new_val << I40E_PRTRPB_SPS_SPS_SHIFT) &
		I40E_PRTRPB_SPS_SPS_MASK;
	wr32(hw, I40E_PRTRPB_SPS, reg);

	 
	old_val = old_pb_cfg->shared_pool_low_wm;
	new_val = new_pb_cfg->shared_pool_low_wm;
	if (new_val > old_val) {
		reg = rd32(hw, I40E_PRTRPB_SLW);
		reg &= ~I40E_PRTRPB_SLW_SLW_MASK;
		reg |= (new_val << I40E_PRTRPB_SLW_SLW_SHIFT) &
			I40E_PRTRPB_SLW_SLW_MASK;
		wr32(hw, I40E_PRTRPB_SLW, reg);
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_low_thresh[i];
		new_val = new_pb_cfg->shared_pool_low_thresh[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_SLT(i));
			reg &= ~I40E_PRTRPB_SLT_SLT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SLT_SLT_TCN_SHIFT) &
				I40E_PRTRPB_SLT_SLT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SLT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_low_wm[i];
		new_val = new_pb_cfg->tc_pool_low_wm[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_DLW(i));
			reg &= ~I40E_PRTRPB_DLW_DLW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DLW_DLW_TCN_SHIFT) &
				I40E_PRTRPB_DLW_DLW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DLW(i), reg);
		}
	}

	 
	old_val = old_pb_cfg->shared_pool_high_wm;
	new_val = new_pb_cfg->shared_pool_high_wm;
	if (new_val > old_val) {
		reg = rd32(hw, I40E_PRTRPB_SHW);
		reg &= ~I40E_PRTRPB_SHW_SHW_MASK;
		reg |= (new_val << I40E_PRTRPB_SHW_SHW_SHIFT) &
			I40E_PRTRPB_SHW_SHW_MASK;
		wr32(hw, I40E_PRTRPB_SHW, reg);
	}

	 
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_high_thresh[i];
		new_val = new_pb_cfg->shared_pool_high_thresh[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_SHT(i));
			reg &= ~I40E_PRTRPB_SHT_SHT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SHT_SHT_TCN_SHIFT) &
				I40E_PRTRPB_SHT_SHT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SHT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_high_wm[i];
		new_val = new_pb_cfg->tc_pool_high_wm[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_DHW(i));
			reg &= ~I40E_PRTRPB_DHW_DHW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DHW_DHW_TCN_SHIFT) &
				I40E_PRTRPB_DHW_DHW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DHW(i), reg);
		}
	}
}

 
static int _i40e_read_lldp_cfg(struct i40e_hw *hw,
			       struct i40e_lldp_variables *lldp_cfg,
			       u8 module, u32 word_offset)
{
	u32 address, offset = (2 * word_offset);
	__le16 raw_mem;
	int ret;
	u16 mem;

	ret = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
	if (ret)
		return ret;

	ret = i40e_aq_read_nvm(hw, 0x0, module * 2, sizeof(raw_mem), &raw_mem,
			       true, NULL);
	i40e_release_nvm(hw);
	if (ret)
		return ret;

	mem = le16_to_cpu(raw_mem);
	 
	if (mem & I40E_PTR_TYPE)
		address = (0x7FFF & mem) * 4096;
	else
		address = (0x7FFF & mem) * 2;

	ret = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
	if (ret)
		goto err_lldp_cfg;

	ret = i40e_aq_read_nvm(hw, module, offset, sizeof(raw_mem), &raw_mem,
			       true, NULL);
	i40e_release_nvm(hw);
	if (ret)
		return ret;

	mem = le16_to_cpu(raw_mem);
	offset = mem + word_offset;
	offset *= 2;

	ret = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
	if (ret)
		goto err_lldp_cfg;

	ret = i40e_aq_read_nvm(hw, 0, address + offset,
			       sizeof(struct i40e_lldp_variables), lldp_cfg,
			       true, NULL);
	i40e_release_nvm(hw);

err_lldp_cfg:
	return ret;
}

 
int i40e_read_lldp_cfg(struct i40e_hw *hw,
		       struct i40e_lldp_variables *lldp_cfg)
{
	int ret = 0;
	u32 mem;

	if (!lldp_cfg)
		return -EINVAL;

	ret = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
	if (ret)
		return ret;

	ret = i40e_aq_read_nvm(hw, I40E_SR_NVM_CONTROL_WORD, 0, sizeof(mem),
			       &mem, true, NULL);
	i40e_release_nvm(hw);
	if (ret)
		return ret;

	 
	if (mem & I40E_SR_NVM_MAP_STRUCTURE_TYPE) {
		 
		ret = _i40e_read_lldp_cfg(hw, lldp_cfg, I40E_SR_EMP_MODULE_PTR,
					  I40E_SR_LLDP_CFG_PTR);
	} else {
		 
		ret = _i40e_read_lldp_cfg(hw, lldp_cfg, I40E_EMP_MODULE_PTR,
					  I40E_NVM_LLDP_CFG_PTR);
	}

	return ret;
}

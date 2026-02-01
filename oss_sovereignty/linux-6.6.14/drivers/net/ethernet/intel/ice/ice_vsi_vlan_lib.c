
 

#include "ice_vsi_vlan_lib.h"
#include "ice_lib.h"
#include "ice_fltr.h"
#include "ice.h"

static void print_invalid_tpid(struct ice_vsi *vsi, u16 tpid)
{
	dev_err(ice_pf_to_dev(vsi->back), "%s %d specified invalid VLAN tpid 0x%04x\n",
		ice_vsi_type_str(vsi->type), vsi->idx, tpid);
}

 
static bool validate_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	if (vlan->tpid != ETH_P_8021Q && vlan->tpid != ETH_P_8021AD &&
	    vlan->tpid != ETH_P_QINQ1 && (vlan->tpid || vlan->vid)) {
		print_invalid_tpid(vsi, vlan->tpid);
		return false;
	}

	return true;
}

 
int ice_vsi_add_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	int err;

	if (!validate_vlan(vsi, vlan))
		return -EINVAL;

	err = ice_fltr_add_vlan(vsi, vlan);
	if (err && err != -EEXIST) {
		dev_err(ice_pf_to_dev(vsi->back), "Failure Adding VLAN %d on VSI %i, status %d\n",
			vlan->vid, vsi->vsi_num, err);
		return err;
	}

	vsi->num_vlan++;
	return 0;
}

 
int ice_vsi_del_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	struct ice_pf *pf = vsi->back;
	struct device *dev;
	int err;

	if (!validate_vlan(vsi, vlan))
		return -EINVAL;

	dev = ice_pf_to_dev(pf);

	err = ice_fltr_remove_vlan(vsi, vlan);
	if (!err)
		vsi->num_vlan--;
	else if (err == -ENOENT || err == -EBUSY)
		err = 0;
	else
		dev_err(dev, "Error removing VLAN %d on VSI %i error: %d\n",
			vlan->vid, vsi->vsi_num, err);

	return err;
}

 
static int ice_vsi_manage_vlan_insertion(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	int err;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	 
	ctxt->info.inner_vlan_flags = ICE_AQ_VSI_INNER_VLAN_TX_MODE_ALL;

	 
	ctxt->info.inner_vlan_flags |= (vsi->info.inner_vlan_flags &
					ICE_AQ_VSI_INNER_VLAN_EMODE_M);

	ctxt->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_VLAN_VALID);

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err) {
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for VLAN insert failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
		goto out;
	}

	vsi->info.inner_vlan_flags = ctxt->info.inner_vlan_flags;
out:
	kfree(ctxt);
	return err;
}

 
static int ice_vsi_manage_vlan_stripping(struct ice_vsi *vsi, bool ena)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	int err;

	 
	if (vsi->info.port_based_inner_vlan)
		return 0;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	 
	if (ena)
		 
		ctxt->info.inner_vlan_flags = ICE_AQ_VSI_INNER_VLAN_EMODE_STR_BOTH;
	else
		 
		ctxt->info.inner_vlan_flags = ICE_AQ_VSI_INNER_VLAN_EMODE_NOTHING;

	 
	ctxt->info.inner_vlan_flags |= ICE_AQ_VSI_INNER_VLAN_TX_MODE_ALL;

	ctxt->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_VLAN_VALID);

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err) {
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for VLAN strip failed, ena = %d err %d aq_err %s\n",
			ena, err, ice_aq_str(hw->adminq.sq_last_status));
		goto out;
	}

	vsi->info.inner_vlan_flags = ctxt->info.inner_vlan_flags;
out:
	kfree(ctxt);
	return err;
}

int ice_vsi_ena_inner_stripping(struct ice_vsi *vsi, const u16 tpid)
{
	if (tpid != ETH_P_8021Q) {
		print_invalid_tpid(vsi, tpid);
		return -EINVAL;
	}

	return ice_vsi_manage_vlan_stripping(vsi, true);
}

int ice_vsi_dis_inner_stripping(struct ice_vsi *vsi)
{
	return ice_vsi_manage_vlan_stripping(vsi, false);
}

int ice_vsi_ena_inner_insertion(struct ice_vsi *vsi, const u16 tpid)
{
	if (tpid != ETH_P_8021Q) {
		print_invalid_tpid(vsi, tpid);
		return -EINVAL;
	}

	return ice_vsi_manage_vlan_insertion(vsi);
}

int ice_vsi_dis_inner_insertion(struct ice_vsi *vsi)
{
	return ice_vsi_manage_vlan_insertion(vsi);
}

static void
ice_save_vlan_info(struct ice_aqc_vsi_props *info,
		   struct ice_vsi_vlan_info *vlan)
{
	vlan->sw_flags2 = info->sw_flags2;
	vlan->inner_vlan_flags = info->inner_vlan_flags;
	vlan->outer_vlan_flags = info->outer_vlan_flags;
}

static void
ice_restore_vlan_info(struct ice_aqc_vsi_props *info,
		      struct ice_vsi_vlan_info *vlan)
{
	info->sw_flags2 = vlan->sw_flags2;
	info->inner_vlan_flags = vlan->inner_vlan_flags;
	info->outer_vlan_flags = vlan->outer_vlan_flags;
}

 
static int __ice_vsi_set_inner_port_vlan(struct ice_vsi *vsi, u16 pvid_info)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_aqc_vsi_props *info;
	struct ice_vsi_ctx *ctxt;
	int ret;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ice_save_vlan_info(&vsi->info, &vsi->vlan_info);
	ctxt->info = vsi->info;
	info = &ctxt->info;
	info->inner_vlan_flags = ICE_AQ_VSI_INNER_VLAN_TX_MODE_ACCEPTUNTAGGED |
		ICE_AQ_VSI_INNER_VLAN_INSERT_PVID |
		ICE_AQ_VSI_INNER_VLAN_EMODE_STR;
	info->sw_flags2 |= ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;

	info->port_based_inner_vlan = cpu_to_le16(pvid_info);
	info->valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_VLAN_VALID |
					   ICE_AQ_VSI_PROP_SW_VALID);

	ret = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (ret) {
		dev_info(ice_hw_to_dev(hw), "update VSI for port VLAN failed, err %d aq_err %s\n",
			 ret, ice_aq_str(hw->adminq.sq_last_status));
		goto out;
	}

	vsi->info.inner_vlan_flags = info->inner_vlan_flags;
	vsi->info.sw_flags2 = info->sw_flags2;
	vsi->info.port_based_inner_vlan = info->port_based_inner_vlan;
out:
	kfree(ctxt);
	return ret;
}

int ice_vsi_set_inner_port_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	u16 port_vlan_info;

	if (vlan->tpid != ETH_P_8021Q)
		return -EINVAL;

	if (vlan->prio > 7)
		return -EINVAL;

	port_vlan_info = vlan->vid | (vlan->prio << VLAN_PRIO_SHIFT);

	return __ice_vsi_set_inner_port_vlan(vsi, port_vlan_info);
}

int ice_vsi_clear_inner_port_vlan(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_aqc_vsi_props *info;
	struct ice_vsi_ctx *ctxt;
	int ret;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ice_restore_vlan_info(&vsi->info, &vsi->vlan_info);
	vsi->info.port_based_inner_vlan = 0;
	ctxt->info = vsi->info;
	info = &ctxt->info;
	info->valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_VLAN_VALID |
					   ICE_AQ_VSI_PROP_SW_VALID);

	ret = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (ret)
		dev_err(ice_hw_to_dev(hw), "update VSI for port VLAN failed, err %d aq_err %s\n",
			ret, ice_aq_str(hw->adminq.sq_last_status));

	kfree(ctxt);
	return ret;
}

 
static int ice_cfg_vlan_pruning(struct ice_vsi *vsi, bool ena)
{
	struct ice_vsi_ctx *ctxt;
	struct ice_pf *pf;
	int status;

	if (!vsi)
		return -EINVAL;

	 
	if (vsi->netdev && vsi->netdev->flags & IFF_PROMISC && ena)
		return 0;

	pf = vsi->back;
	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ctxt->info = vsi->info;

	if (ena)
		ctxt->info.sw_flags2 |= ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;
	else
		ctxt->info.sw_flags2 &= ~ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;

	ctxt->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_SW_VALID);

	status = ice_update_vsi(&pf->hw, vsi->idx, ctxt, NULL);
	if (status) {
		netdev_err(vsi->netdev, "%sabling VLAN pruning on VSI handle: %d, VSI HW ID: %d failed, err = %d, aq_err = %s\n",
			   ena ? "En" : "Dis", vsi->idx, vsi->vsi_num, status,
			   ice_aq_str(pf->hw.adminq.sq_last_status));
		goto err_out;
	}

	vsi->info.sw_flags2 = ctxt->info.sw_flags2;

	kfree(ctxt);
	return 0;

err_out:
	kfree(ctxt);
	return status;
}

int ice_vsi_ena_rx_vlan_filtering(struct ice_vsi *vsi)
{
	return ice_cfg_vlan_pruning(vsi, true);
}

int ice_vsi_dis_rx_vlan_filtering(struct ice_vsi *vsi)
{
	return ice_cfg_vlan_pruning(vsi, false);
}

static int ice_cfg_vlan_antispoof(struct ice_vsi *vsi, bool enable)
{
	struct ice_vsi_ctx *ctx;
	int err;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->info.sec_flags = vsi->info.sec_flags;
	ctx->info.valid_sections = cpu_to_le16(ICE_AQ_VSI_PROP_SECURITY_VALID);

	if (enable)
		ctx->info.sec_flags |= ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA <<
			ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S;
	else
		ctx->info.sec_flags &= ~(ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA <<
					 ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S);

	err = ice_update_vsi(&vsi->back->hw, vsi->idx, ctx, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "Failed to configure Tx VLAN anti-spoof %s for VSI %d, error %d\n",
			enable ? "ON" : "OFF", vsi->vsi_num, err);
	else
		vsi->info.sec_flags = ctx->info.sec_flags;

	kfree(ctx);

	return err;
}

int ice_vsi_ena_tx_vlan_filtering(struct ice_vsi *vsi)
{
	return ice_cfg_vlan_antispoof(vsi, true);
}

int ice_vsi_dis_tx_vlan_filtering(struct ice_vsi *vsi)
{
	return ice_cfg_vlan_antispoof(vsi, false);
}

 
static int tpid_to_vsi_outer_vlan_type(u16 tpid, u8 *tag_type)
{
	switch (tpid) {
	case ETH_P_8021Q:
		*tag_type = ICE_AQ_VSI_OUTER_TAG_VLAN_8100;
		break;
	case ETH_P_8021AD:
		*tag_type = ICE_AQ_VSI_OUTER_TAG_STAG;
		break;
	case ETH_P_QINQ1:
		*tag_type = ICE_AQ_VSI_OUTER_TAG_VLAN_9100;
		break;
	default:
		*tag_type = 0;
		return -EINVAL;
	}

	return 0;
}

 
int ice_vsi_ena_outer_stripping(struct ice_vsi *vsi, u16 tpid)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	u8 tag_type;
	int err;

	 
	if (vsi->info.port_based_outer_vlan)
		return 0;

	if (tpid_to_vsi_outer_vlan_type(tpid, &tag_type))
		return -EINVAL;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	 
	ctxt->info.outer_vlan_flags = vsi->info.outer_vlan_flags &
		~(ICE_AQ_VSI_OUTER_VLAN_EMODE_M | ICE_AQ_VSI_OUTER_TAG_TYPE_M);
	ctxt->info.outer_vlan_flags |=
		((ICE_AQ_VSI_OUTER_VLAN_EMODE_SHOW_BOTH <<
		  ICE_AQ_VSI_OUTER_VLAN_EMODE_S) |
		 ((tag_type << ICE_AQ_VSI_OUTER_TAG_TYPE_S) &
		  ICE_AQ_VSI_OUTER_TAG_TYPE_M));

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for enabling outer VLAN stripping failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	else
		vsi->info.outer_vlan_flags = ctxt->info.outer_vlan_flags;

	kfree(ctxt);
	return err;
}

 
int ice_vsi_dis_outer_stripping(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	int err;

	if (vsi->info.port_based_outer_vlan)
		return 0;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	 
	ctxt->info.outer_vlan_flags = vsi->info.outer_vlan_flags &
		~ICE_AQ_VSI_OUTER_VLAN_EMODE_M;
	ctxt->info.outer_vlan_flags |= ICE_AQ_VSI_OUTER_VLAN_EMODE_NOTHING <<
		ICE_AQ_VSI_OUTER_VLAN_EMODE_S;

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for disabling outer VLAN stripping failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	else
		vsi->info.outer_vlan_flags = ctxt->info.outer_vlan_flags;

	kfree(ctxt);
	return err;
}

 
int ice_vsi_ena_outer_insertion(struct ice_vsi *vsi, u16 tpid)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	u8 tag_type;
	int err;

	if (vsi->info.port_based_outer_vlan)
		return 0;

	if (tpid_to_vsi_outer_vlan_type(tpid, &tag_type))
		return -EINVAL;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	 
	ctxt->info.outer_vlan_flags = vsi->info.outer_vlan_flags &
		~(ICE_AQ_VSI_OUTER_VLAN_PORT_BASED_INSERT |
		  ICE_AQ_VSI_OUTER_VLAN_BLOCK_TX_DESC |
		  ICE_AQ_VSI_OUTER_VLAN_TX_MODE_M |
		  ICE_AQ_VSI_OUTER_TAG_TYPE_M);
	ctxt->info.outer_vlan_flags |=
		((ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ALL <<
		  ICE_AQ_VSI_OUTER_VLAN_TX_MODE_S) &
		 ICE_AQ_VSI_OUTER_VLAN_TX_MODE_M) |
		((tag_type << ICE_AQ_VSI_OUTER_TAG_TYPE_S) &
		 ICE_AQ_VSI_OUTER_TAG_TYPE_M);

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for enabling outer VLAN insertion failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	else
		vsi->info.outer_vlan_flags = ctxt->info.outer_vlan_flags;

	kfree(ctxt);
	return err;
}

 
int ice_vsi_dis_outer_insertion(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	int err;

	if (vsi->info.port_based_outer_vlan)
		return 0;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	 
	ctxt->info.outer_vlan_flags = vsi->info.outer_vlan_flags &
		~(ICE_AQ_VSI_OUTER_VLAN_PORT_BASED_INSERT |
		  ICE_AQ_VSI_OUTER_VLAN_TX_MODE_M);
	ctxt->info.outer_vlan_flags |=
		ICE_AQ_VSI_OUTER_VLAN_BLOCK_TX_DESC |
		((ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ALL <<
		  ICE_AQ_VSI_OUTER_VLAN_TX_MODE_S) &
		 ICE_AQ_VSI_OUTER_VLAN_TX_MODE_M);

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for disabling outer VLAN insertion failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	else
		vsi->info.outer_vlan_flags = ctxt->info.outer_vlan_flags;

	kfree(ctxt);
	return err;
}

 
static int
__ice_vsi_set_outer_port_vlan(struct ice_vsi *vsi, u16 vlan_info, u16 tpid)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	u8 tag_type;
	int err;

	if (tpid_to_vsi_outer_vlan_type(tpid, &tag_type))
		return -EINVAL;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ice_save_vlan_info(&vsi->info, &vsi->vlan_info);
	ctxt->info = vsi->info;

	ctxt->info.sw_flags2 |= ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;

	ctxt->info.port_based_outer_vlan = cpu_to_le16(vlan_info);
	ctxt->info.outer_vlan_flags =
		(ICE_AQ_VSI_OUTER_VLAN_EMODE_SHOW <<
		 ICE_AQ_VSI_OUTER_VLAN_EMODE_S) |
		((tag_type << ICE_AQ_VSI_OUTER_TAG_TYPE_S) &
		 ICE_AQ_VSI_OUTER_TAG_TYPE_M) |
		ICE_AQ_VSI_OUTER_VLAN_BLOCK_TX_DESC |
		(ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ACCEPTUNTAGGED <<
		 ICE_AQ_VSI_OUTER_VLAN_TX_MODE_S) |
		ICE_AQ_VSI_OUTER_VLAN_PORT_BASED_INSERT;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID |
			    ICE_AQ_VSI_PROP_SW_VALID);

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err) {
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for setting outer port based VLAN failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
	} else {
		vsi->info.port_based_outer_vlan = ctxt->info.port_based_outer_vlan;
		vsi->info.outer_vlan_flags = ctxt->info.outer_vlan_flags;
		vsi->info.sw_flags2 = ctxt->info.sw_flags2;
	}

	kfree(ctxt);
	return err;
}

 
int ice_vsi_set_outer_port_vlan(struct ice_vsi *vsi, struct ice_vlan *vlan)
{
	u16 port_vlan_info;

	if (vlan->prio > (VLAN_PRIO_MASK >> VLAN_PRIO_SHIFT))
		return -EINVAL;

	port_vlan_info = vlan->vid | (vlan->prio << VLAN_PRIO_SHIFT);

	return __ice_vsi_set_outer_port_vlan(vsi, port_vlan_info, vlan->tpid);
}

 
int ice_vsi_clear_outer_port_vlan(struct ice_vsi *vsi)
{
	struct ice_hw *hw = &vsi->back->hw;
	struct ice_vsi_ctx *ctxt;
	int err;

	ctxt = kzalloc(sizeof(*ctxt), GFP_KERNEL);
	if (!ctxt)
		return -ENOMEM;

	ice_restore_vlan_info(&vsi->info, &vsi->vlan_info);
	vsi->info.port_based_outer_vlan = 0;
	ctxt->info = vsi->info;

	ctxt->info.valid_sections =
		cpu_to_le16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID |
			    ICE_AQ_VSI_PROP_SW_VALID);

	err = ice_update_vsi(hw, vsi->idx, ctxt, NULL);
	if (err)
		dev_err(ice_pf_to_dev(vsi->back), "update VSI for clearing outer port based VLAN failed, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));

	kfree(ctxt);
	return err;
}

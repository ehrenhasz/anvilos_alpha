
 

#include "ice_common.h"

 
static int
ice_pkg_get_supported_vlan_mode(struct ice_hw *hw, bool *dvm)
{
	u16 meta_init_size = sizeof(struct ice_meta_init_section);
	struct ice_meta_init_section *sect;
	struct ice_buf_build *bld;
	int status;

	 
	*dvm = false;

	bld = ice_pkg_buf_alloc_single_section(hw,
					       ICE_SID_RXPARSER_METADATA_INIT,
					       meta_init_size, (void **)&sect);
	if (!bld)
		return -ENOMEM;

	 
	sect->count = cpu_to_le16(1);
	sect->offset = cpu_to_le16(ICE_META_VLAN_MODE_ENTRY);

	status = ice_aq_upload_section(hw,
				       (struct ice_buf_hdr *)ice_pkg_buf(bld),
				       ICE_PKG_BUF_SIZE, NULL);
	if (!status) {
		DECLARE_BITMAP(entry, ICE_META_INIT_BITS);
		u32 arr[ICE_META_INIT_DW_CNT];
		u16 i;

		 
		for (i = 0; i < ICE_META_INIT_DW_CNT; i++)
			arr[i] = le32_to_cpu(sect->entry.bm[i]);

		bitmap_from_arr32(entry, arr, (u16)ICE_META_INIT_BITS);

		 
		*dvm = test_bit(ICE_META_VLAN_MODE_BIT, entry);
	}

	ice_pkg_buf_free(hw, bld);

	return status;
}

 
static int
ice_aq_get_vlan_mode(struct ice_hw *hw,
		     struct ice_aqc_get_vlan_mode *get_params)
{
	struct ice_aq_desc desc;

	if (!get_params)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_get_vlan_mode_parameters);

	return ice_aq_send_cmd(hw, &desc, get_params, sizeof(*get_params),
			       NULL);
}

 
static bool ice_aq_is_dvm_ena(struct ice_hw *hw)
{
	struct ice_aqc_get_vlan_mode get_params = { 0 };
	int status;

	status = ice_aq_get_vlan_mode(hw, &get_params);
	if (status) {
		ice_debug(hw, ICE_DBG_AQ, "Failed to get VLAN mode, status %d\n",
			  status);
		return false;
	}

	return (get_params.vlan_mode & ICE_AQ_VLAN_MODE_DVM_ENA);
}

 
bool ice_is_dvm_ena(struct ice_hw *hw)
{
	return hw->dvm_ena;
}

 
static void ice_cache_vlan_mode(struct ice_hw *hw)
{
	hw->dvm_ena = ice_aq_is_dvm_ena(hw) ? true : false;
}

 
static bool ice_pkg_supports_dvm(struct ice_hw *hw)
{
	bool pkg_supports_dvm;
	int status;

	status = ice_pkg_get_supported_vlan_mode(hw, &pkg_supports_dvm);
	if (status) {
		ice_debug(hw, ICE_DBG_PKG, "Failed to get supported VLAN mode, status %d\n",
			  status);
		return false;
	}

	return pkg_supports_dvm;
}

 
static bool ice_fw_supports_dvm(struct ice_hw *hw)
{
	struct ice_aqc_get_vlan_mode get_vlan_mode = { 0 };
	int status;

	 
	status = ice_aq_get_vlan_mode(hw, &get_vlan_mode);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to get VLAN mode, status %d\n",
			  status);
		return false;
	}

	return true;
}

 
static bool ice_is_dvm_supported(struct ice_hw *hw)
{
	if (!ice_pkg_supports_dvm(hw)) {
		ice_debug(hw, ICE_DBG_PKG, "DDP doesn't support DVM\n");
		return false;
	}

	if (!ice_fw_supports_dvm(hw)) {
		ice_debug(hw, ICE_DBG_PKG, "FW doesn't support DVM\n");
		return false;
	}

	return true;
}

#define ICE_EXTERNAL_VLAN_ID_FV_IDX			11
#define ICE_SW_LKUP_VLAN_LOC_LKUP_IDX			1
#define ICE_SW_LKUP_VLAN_PKT_FLAGS_LKUP_IDX		2
#define ICE_SW_LKUP_PROMISC_VLAN_LOC_LKUP_IDX		2
#define ICE_PKT_FLAGS_0_TO_15_FV_IDX			1
static struct ice_update_recipe_lkup_idx_params ice_dvm_dflt_recipes[] = {
	{
		 
		.rid = ICE_SW_LKUP_VLAN,
		.fv_idx = ICE_EXTERNAL_VLAN_ID_FV_IDX,
		.ignore_valid = true,
		.mask = 0,
		.mask_valid = false,  
		.lkup_idx = ICE_SW_LKUP_VLAN_LOC_LKUP_IDX,
	},
	{
		 
		.rid = ICE_SW_LKUP_VLAN,
		.fv_idx = ICE_PKT_FLAGS_0_TO_15_FV_IDX,
		.ignore_valid = false,
		.mask = ICE_PKT_VLAN_MASK,
		.mask_valid = true,
		.lkup_idx = ICE_SW_LKUP_VLAN_PKT_FLAGS_LKUP_IDX,
	},
	{
		 
		.rid = ICE_SW_LKUP_PROMISC_VLAN,
		.fv_idx = ICE_EXTERNAL_VLAN_ID_FV_IDX,
		.ignore_valid = true,
		.mask = 0,
		.mask_valid = false,   
		.lkup_idx = ICE_SW_LKUP_PROMISC_VLAN_LOC_LKUP_IDX,
	},
};

 
static int ice_dvm_update_dflt_recipes(struct ice_hw *hw)
{
	unsigned long i;

	for (i = 0; i < ARRAY_SIZE(ice_dvm_dflt_recipes); i++) {
		struct ice_update_recipe_lkup_idx_params *params;
		int status;

		params = &ice_dvm_dflt_recipes[i];

		status = ice_update_recipe_lkup_idx(hw, params);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to update RID %d lkup_idx %d fv_idx %d mask_valid %s mask 0x%04x\n",
				  params->rid, params->lkup_idx, params->fv_idx,
				  params->mask_valid ? "true" : "false",
				  params->mask);
			return status;
		}
	}

	return 0;
}

 
static int
ice_aq_set_vlan_mode(struct ice_hw *hw,
		     struct ice_aqc_set_vlan_mode *set_params)
{
	u8 rdma_packet, mng_vlan_prot_id;
	struct ice_aq_desc desc;

	if (!set_params)
		return -EINVAL;

	if (set_params->l2tag_prio_tagging > ICE_AQ_VLAN_PRIO_TAG_MAX)
		return -EINVAL;

	rdma_packet = set_params->rdma_packet;
	if (rdma_packet != ICE_AQ_SVM_VLAN_RDMA_PKT_FLAG_SETTING &&
	    rdma_packet != ICE_AQ_DVM_VLAN_RDMA_PKT_FLAG_SETTING)
		return -EINVAL;

	mng_vlan_prot_id = set_params->mng_vlan_prot_id;
	if (mng_vlan_prot_id != ICE_AQ_VLAN_MNG_PROTOCOL_ID_OUTER &&
	    mng_vlan_prot_id != ICE_AQ_VLAN_MNG_PROTOCOL_ID_INNER)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_set_vlan_mode_parameters);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	return ice_aq_send_cmd(hw, &desc, set_params, sizeof(*set_params),
			       NULL);
}

 
static int ice_set_dvm(struct ice_hw *hw)
{
	struct ice_aqc_set_vlan_mode params = { 0 };
	int status;

	params.l2tag_prio_tagging = ICE_AQ_VLAN_PRIO_TAG_OUTER_CTAG;
	params.rdma_packet = ICE_AQ_DVM_VLAN_RDMA_PKT_FLAG_SETTING;
	params.mng_vlan_prot_id = ICE_AQ_VLAN_MNG_PROTOCOL_ID_OUTER;

	status = ice_aq_set_vlan_mode(hw, &params);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to set double VLAN mode parameters, status %d\n",
			  status);
		return status;
	}

	status = ice_dvm_update_dflt_recipes(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to update default recipes for double VLAN mode, status %d\n",
			  status);
		return status;
	}

	status = ice_aq_set_port_params(hw->port_info, true, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to set port in double VLAN mode, status %d\n",
			  status);
		return status;
	}

	status = ice_set_dvm_boost_entries(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to set boost TCAM entries for double VLAN mode, status %d\n",
			  status);
		return status;
	}

	return 0;
}

 
static int ice_set_svm(struct ice_hw *hw)
{
	struct ice_aqc_set_vlan_mode *set_params;
	int status;

	status = ice_aq_set_port_params(hw->port_info, false, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to set port parameters for single VLAN mode\n");
		return status;
	}

	set_params = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*set_params),
				  GFP_KERNEL);
	if (!set_params)
		return -ENOMEM;

	 
	set_params->l2tag_prio_tagging = ICE_AQ_VLAN_PRIO_TAG_INNER_CTAG;
	set_params->rdma_packet = ICE_AQ_SVM_VLAN_RDMA_PKT_FLAG_SETTING;
	set_params->mng_vlan_prot_id = ICE_AQ_VLAN_MNG_PROTOCOL_ID_INNER;

	status = ice_aq_set_vlan_mode(hw, set_params);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to configure port in single VLAN mode\n");

	devm_kfree(ice_hw_to_dev(hw), set_params);
	return status;
}

 
int ice_set_vlan_mode(struct ice_hw *hw)
{
	if (!ice_is_dvm_supported(hw))
		return 0;

	if (!ice_set_dvm(hw))
		return 0;

	return ice_set_svm(hw);
}

 
static void ice_print_dvm_not_supported(struct ice_hw *hw)
{
	bool pkg_supports_dvm = ice_pkg_supports_dvm(hw);
	bool fw_supports_dvm = ice_fw_supports_dvm(hw);

	if (!fw_supports_dvm && !pkg_supports_dvm)
		dev_info(ice_hw_to_dev(hw), "QinQ functionality cannot be enabled on this device. Update your DDP package and NVM to versions that support QinQ.\n");
	else if (!pkg_supports_dvm)
		dev_info(ice_hw_to_dev(hw), "QinQ functionality cannot be enabled on this device. Update your DDP package to a version that supports QinQ.\n");
	else if (!fw_supports_dvm)
		dev_info(ice_hw_to_dev(hw), "QinQ functionality cannot be enabled on this device. Update your NVM to a version that supports QinQ.\n");
}

 
void ice_post_pkg_dwnld_vlan_mode_cfg(struct ice_hw *hw)
{
	ice_cache_vlan_mode(hw);

	if (ice_is_dvm_ena(hw))
		ice_change_proto_id_to_dvm();
	else
		ice_print_dvm_not_supported(hw);
}

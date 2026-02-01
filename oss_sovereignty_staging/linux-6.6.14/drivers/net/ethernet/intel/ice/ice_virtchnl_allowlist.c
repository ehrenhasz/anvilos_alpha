
 

#include "ice_virtchnl_allowlist.h"

 

 
static const u32 default_allowlist_opcodes[] = {
	VIRTCHNL_OP_GET_VF_RESOURCES, VIRTCHNL_OP_VERSION, VIRTCHNL_OP_RESET_VF,
};

 
static const u32 working_allowlist_opcodes[] = {
	VIRTCHNL_OP_CONFIG_TX_QUEUE, VIRTCHNL_OP_CONFIG_RX_QUEUE,
	VIRTCHNL_OP_CONFIG_VSI_QUEUES, VIRTCHNL_OP_CONFIG_IRQ_MAP,
	VIRTCHNL_OP_ENABLE_QUEUES, VIRTCHNL_OP_DISABLE_QUEUES,
	VIRTCHNL_OP_GET_STATS, VIRTCHNL_OP_EVENT,
};

 
static const u32 l2_allowlist_opcodes[] = {
	VIRTCHNL_OP_ADD_ETH_ADDR, VIRTCHNL_OP_DEL_ETH_ADDR,
	VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE,
};

 
static const u32 req_queues_allowlist_opcodes[] = {
	VIRTCHNL_OP_REQUEST_QUEUES,
};

 
static const u32 vlan_allowlist_opcodes[] = {
	VIRTCHNL_OP_ADD_VLAN, VIRTCHNL_OP_DEL_VLAN,
	VIRTCHNL_OP_ENABLE_VLAN_STRIPPING, VIRTCHNL_OP_DISABLE_VLAN_STRIPPING,
};

 
static const u32 vlan_v2_allowlist_opcodes[] = {
	VIRTCHNL_OP_GET_OFFLOAD_VLAN_V2_CAPS, VIRTCHNL_OP_ADD_VLAN_V2,
	VIRTCHNL_OP_DEL_VLAN_V2, VIRTCHNL_OP_ENABLE_VLAN_STRIPPING_V2,
	VIRTCHNL_OP_DISABLE_VLAN_STRIPPING_V2,
	VIRTCHNL_OP_ENABLE_VLAN_INSERTION_V2,
	VIRTCHNL_OP_DISABLE_VLAN_INSERTION_V2,
};

 
static const u32 rss_pf_allowlist_opcodes[] = {
	VIRTCHNL_OP_CONFIG_RSS_KEY, VIRTCHNL_OP_CONFIG_RSS_LUT,
	VIRTCHNL_OP_GET_RSS_HENA_CAPS, VIRTCHNL_OP_SET_RSS_HENA,
};

 
static const u32 rx_flex_desc_allowlist_opcodes[] = {
	VIRTCHNL_OP_GET_SUPPORTED_RXDIDS,
};

 
static const u32 adv_rss_pf_allowlist_opcodes[] = {
	VIRTCHNL_OP_ADD_RSS_CFG, VIRTCHNL_OP_DEL_RSS_CFG,
};

 
static const u32 fdir_pf_allowlist_opcodes[] = {
	VIRTCHNL_OP_ADD_FDIR_FILTER, VIRTCHNL_OP_DEL_FDIR_FILTER,
};

struct allowlist_opcode_info {
	const u32 *opcodes;
	size_t size;
};

#define BIT_INDEX(caps) (HWEIGHT((caps) - 1))
#define ALLOW_ITEM(caps, list) \
	[BIT_INDEX(caps)] = { \
		.opcodes = list, \
		.size = ARRAY_SIZE(list) \
	}
static const struct allowlist_opcode_info allowlist_opcodes[] = {
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_L2, l2_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_REQ_QUEUES, req_queues_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_VLAN, vlan_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_RSS_PF, rss_pf_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC, rx_flex_desc_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF, adv_rss_pf_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_FDIR_PF, fdir_pf_allowlist_opcodes),
	ALLOW_ITEM(VIRTCHNL_VF_OFFLOAD_VLAN_V2, vlan_v2_allowlist_opcodes),
};

 
bool ice_vc_is_opcode_allowed(struct ice_vf *vf, u32 opcode)
{
	if (opcode >= VIRTCHNL_OP_MAX)
		return false;

	return test_bit(opcode, vf->opcodes_allowlist);
}

 
static void
ice_vc_allowlist_opcodes(struct ice_vf *vf, const u32 *opcodes, size_t size)
{
	unsigned int i;

	for (i = 0; i < size; i++)
		set_bit(opcodes[i], vf->opcodes_allowlist);
}

 
static void ice_vc_clear_allowlist(struct ice_vf *vf)
{
	bitmap_zero(vf->opcodes_allowlist, VIRTCHNL_OP_MAX);
}

 
void ice_vc_set_default_allowlist(struct ice_vf *vf)
{
	ice_vc_clear_allowlist(vf);
	ice_vc_allowlist_opcodes(vf, default_allowlist_opcodes,
				 ARRAY_SIZE(default_allowlist_opcodes));
}

 
void ice_vc_set_working_allowlist(struct ice_vf *vf)
{
	ice_vc_allowlist_opcodes(vf, working_allowlist_opcodes,
				 ARRAY_SIZE(working_allowlist_opcodes));
}

 
void ice_vc_set_caps_allowlist(struct ice_vf *vf)
{
	unsigned long caps = vf->driver_caps;
	unsigned int i;

	for_each_set_bit(i, &caps, ARRAY_SIZE(allowlist_opcodes))
		ice_vc_allowlist_opcodes(vf, allowlist_opcodes[i].opcodes,
					 allowlist_opcodes[i].size);
}

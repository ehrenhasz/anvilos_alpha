
 

 

#include "ice.h"
#include "ice_lib.h"
#include "ice_fdir.h"
#include "ice_flow.h"

static struct in6_addr full_ipv6_addr_mask = {
	.in6_u = {
		.u6_addr8 = {
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		}
	}
};

static struct in6_addr zero_ipv6_addr_mask = {
	.in6_u = {
		.u6_addr8 = {
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		}
	}
};

 
#define TNL_SEG_CNT(_TNL_) ((_TNL_) + 1)

 
static int ice_fltr_to_ethtool_flow(enum ice_fltr_ptype flow)
{
	switch (flow) {
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		return TCP_V4_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		return UDP_V4_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		return SCTP_V4_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		return IPV4_USER_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		return TCP_V6_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		return UDP_V6_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		return SCTP_V6_FLOW;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		return IPV6_USER_FLOW;
	default:
		 
		return 0;
	}
}

 
static enum ice_fltr_ptype ice_ethtool_flow_to_fltr(int eth)
{
	switch (eth) {
	case TCP_V4_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV4_TCP;
	case UDP_V4_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV4_UDP;
	case SCTP_V4_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
	case IPV4_USER_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
	case TCP_V6_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV6_TCP;
	case UDP_V6_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV6_UDP;
	case SCTP_V6_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV6_SCTP;
	case IPV6_USER_FLOW:
		return ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
	default:
		return ICE_FLTR_PTYPE_NONF_NONE;
	}
}

 
static bool ice_is_mask_valid(u64 mask, u64 field)
{
	return (mask & field) == field;
}

 
int ice_get_ethtool_fdir_entry(struct ice_hw *hw, struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp;
	struct ice_fdir_fltr *rule;
	int ret = 0;
	u16 idx;

	fsp = (struct ethtool_rx_flow_spec *)&cmd->fs;

	mutex_lock(&hw->fdir_fltr_lock);

	rule = ice_fdir_find_fltr_by_idx(hw, fsp->location);

	if (!rule || fsp->location != rule->fltr_id) {
		ret = -EINVAL;
		goto release_lock;
	}

	fsp->flow_type = ice_fltr_to_ethtool_flow(rule->flow_type);

	memset(&fsp->m_u, 0, sizeof(fsp->m_u));
	memset(&fsp->m_ext, 0, sizeof(fsp->m_ext));

	switch (fsp->flow_type) {
	case IPV4_USER_FLOW:
		fsp->h_u.usr_ip4_spec.ip_ver = ETH_RX_NFC_IP4;
		fsp->h_u.usr_ip4_spec.proto = 0;
		fsp->h_u.usr_ip4_spec.l4_4_bytes = rule->ip.v4.l4_header;
		fsp->h_u.usr_ip4_spec.tos = rule->ip.v4.tos;
		fsp->h_u.usr_ip4_spec.ip4src = rule->ip.v4.src_ip;
		fsp->h_u.usr_ip4_spec.ip4dst = rule->ip.v4.dst_ip;
		fsp->m_u.usr_ip4_spec.ip4src = rule->mask.v4.src_ip;
		fsp->m_u.usr_ip4_spec.ip4dst = rule->mask.v4.dst_ip;
		fsp->m_u.usr_ip4_spec.ip_ver = 0xFF;
		fsp->m_u.usr_ip4_spec.proto = 0;
		fsp->m_u.usr_ip4_spec.l4_4_bytes = rule->mask.v4.l4_header;
		fsp->m_u.usr_ip4_spec.tos = rule->mask.v4.tos;
		break;
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
		fsp->h_u.tcp_ip4_spec.psrc = rule->ip.v4.src_port;
		fsp->h_u.tcp_ip4_spec.pdst = rule->ip.v4.dst_port;
		fsp->h_u.tcp_ip4_spec.ip4src = rule->ip.v4.src_ip;
		fsp->h_u.tcp_ip4_spec.ip4dst = rule->ip.v4.dst_ip;
		fsp->m_u.tcp_ip4_spec.psrc = rule->mask.v4.src_port;
		fsp->m_u.tcp_ip4_spec.pdst = rule->mask.v4.dst_port;
		fsp->m_u.tcp_ip4_spec.ip4src = rule->mask.v4.src_ip;
		fsp->m_u.tcp_ip4_spec.ip4dst = rule->mask.v4.dst_ip;
		break;
	case IPV6_USER_FLOW:
		fsp->h_u.usr_ip6_spec.l4_4_bytes = rule->ip.v6.l4_header;
		fsp->h_u.usr_ip6_spec.tclass = rule->ip.v6.tc;
		fsp->h_u.usr_ip6_spec.l4_proto = rule->ip.v6.proto;
		memcpy(fsp->h_u.tcp_ip6_spec.ip6src, rule->ip.v6.src_ip,
		       sizeof(struct in6_addr));
		memcpy(fsp->h_u.tcp_ip6_spec.ip6dst, rule->ip.v6.dst_ip,
		       sizeof(struct in6_addr));
		memcpy(fsp->m_u.tcp_ip6_spec.ip6src, rule->mask.v6.src_ip,
		       sizeof(struct in6_addr));
		memcpy(fsp->m_u.tcp_ip6_spec.ip6dst, rule->mask.v6.dst_ip,
		       sizeof(struct in6_addr));
		fsp->m_u.usr_ip6_spec.l4_4_bytes = rule->mask.v6.l4_header;
		fsp->m_u.usr_ip6_spec.tclass = rule->mask.v6.tc;
		fsp->m_u.usr_ip6_spec.l4_proto = rule->mask.v6.proto;
		break;
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
	case SCTP_V6_FLOW:
		memcpy(fsp->h_u.tcp_ip6_spec.ip6src, rule->ip.v6.src_ip,
		       sizeof(struct in6_addr));
		memcpy(fsp->h_u.tcp_ip6_spec.ip6dst, rule->ip.v6.dst_ip,
		       sizeof(struct in6_addr));
		fsp->h_u.tcp_ip6_spec.psrc = rule->ip.v6.src_port;
		fsp->h_u.tcp_ip6_spec.pdst = rule->ip.v6.dst_port;
		memcpy(fsp->m_u.tcp_ip6_spec.ip6src,
		       rule->mask.v6.src_ip,
		       sizeof(struct in6_addr));
		memcpy(fsp->m_u.tcp_ip6_spec.ip6dst,
		       rule->mask.v6.dst_ip,
		       sizeof(struct in6_addr));
		fsp->m_u.tcp_ip6_spec.psrc = rule->mask.v6.src_port;
		fsp->m_u.tcp_ip6_spec.pdst = rule->mask.v6.dst_port;
		fsp->h_u.tcp_ip6_spec.tclass = rule->ip.v6.tc;
		fsp->m_u.tcp_ip6_spec.tclass = rule->mask.v6.tc;
		break;
	default:
		break;
	}

	if (rule->dest_ctl == ICE_FLTR_PRGM_DESC_DEST_DROP_PKT)
		fsp->ring_cookie = RX_CLS_FLOW_DISC;
	else
		fsp->ring_cookie = rule->orig_q_index;

	idx = ice_ethtool_flow_to_fltr(fsp->flow_type);
	if (idx == ICE_FLTR_PTYPE_NONF_NONE) {
		dev_err(ice_hw_to_dev(hw), "Missing input index for flow_type %d\n",
			rule->flow_type);
		ret = -EINVAL;
	}

release_lock:
	mutex_unlock(&hw->fdir_fltr_lock);
	return ret;
}

 
int
ice_get_fdir_fltr_ids(struct ice_hw *hw, struct ethtool_rxnfc *cmd,
		      u32 *rule_locs)
{
	struct ice_fdir_fltr *f_rule;
	unsigned int cnt = 0;
	int val = 0;

	 
	cmd->data = ice_get_fdir_cnt_all(hw);

	mutex_lock(&hw->fdir_fltr_lock);

	list_for_each_entry(f_rule, &hw->fdir_list_head, fltr_node) {
		if (cnt == cmd->rule_cnt) {
			val = -EMSGSIZE;
			goto release_lock;
		}
		rule_locs[cnt] = f_rule->fltr_id;
		cnt++;
	}

release_lock:
	mutex_unlock(&hw->fdir_fltr_lock);
	if (!val)
		cmd->rule_cnt = cnt;
	return val;
}

 
static void
ice_fdir_remap_entries(struct ice_fd_hw_prof *prof, int tun, int idx)
{
	if (idx != prof->cnt && tun < ICE_FD_HW_SEG_MAX) {
		int i;

		for (i = idx; i < (prof->cnt - 1); i++) {
			u64 old_entry_h;

			old_entry_h = prof->entry_h[i + 1][tun];
			prof->entry_h[i][tun] = old_entry_h;
			prof->vsi_h[i] = prof->vsi_h[i + 1];
		}

		prof->entry_h[i][tun] = 0;
		prof->vsi_h[i] = 0;
	}
}

 
void ice_fdir_rem_adq_chnl(struct ice_hw *hw, u16 vsi_idx)
{
	int status, flow;

	if (!hw->fdir_prof)
		return;

	for (flow = 0; flow < ICE_FLTR_PTYPE_MAX; flow++) {
		struct ice_fd_hw_prof *prof = hw->fdir_prof[flow];
		int tun, i;

		if (!prof || !prof->cnt)
			continue;

		for (tun = 0; tun < ICE_FD_HW_SEG_MAX; tun++) {
			u64 prof_id;

			prof_id = flow + tun * ICE_FLTR_PTYPE_MAX;

			for (i = 0; i < prof->cnt; i++) {
				if (prof->vsi_h[i] != vsi_idx)
					continue;

				prof->entry_h[i][tun] = 0;
				prof->vsi_h[i] = 0;
				break;
			}

			 
			ice_fdir_remap_entries(prof, tun, i);

			 
			status = ice_flow_rem_vsi_prof(hw, vsi_idx, prof_id);
			if (status) {
				dev_err(ice_hw_to_dev(hw), "ice_flow_rem_vsi_prof() failed status=%d\n",
					status);
			}
		}
		prof->cnt--;
	}
}

 
static struct ice_fd_hw_prof *
ice_fdir_get_hw_prof(struct ice_hw *hw, enum ice_block blk, int flow)
{
	if (blk == ICE_BLK_FD && hw->fdir_prof)
		return hw->fdir_prof[flow];

	return NULL;
}

 
static void
ice_fdir_erase_flow_from_hw(struct ice_hw *hw, enum ice_block blk, int flow)
{
	struct ice_fd_hw_prof *prof = ice_fdir_get_hw_prof(hw, blk, flow);
	int tun;

	if (!prof)
		return;

	for (tun = 0; tun < ICE_FD_HW_SEG_MAX; tun++) {
		u64 prof_id;
		int j;

		prof_id = flow + tun * ICE_FLTR_PTYPE_MAX;
		for (j = 0; j < prof->cnt; j++) {
			u16 vsi_num;

			if (!prof->entry_h[j][tun] || !prof->vsi_h[j])
				continue;
			vsi_num = ice_get_hw_vsi_num(hw, prof->vsi_h[j]);
			ice_rem_prof_id_flow(hw, blk, vsi_num, prof_id);
			ice_flow_rem_entry(hw, blk, prof->entry_h[j][tun]);
			prof->entry_h[j][tun] = 0;
		}
		ice_flow_rem_prof(hw, blk, prof_id);
	}
}

 
static void
ice_fdir_rem_flow(struct ice_hw *hw, enum ice_block blk,
		  enum ice_fltr_ptype flow_type)
{
	int flow = (int)flow_type & ~FLOW_EXT;
	struct ice_fd_hw_prof *prof;
	int tun, i;

	prof = ice_fdir_get_hw_prof(hw, blk, flow);
	if (!prof)
		return;

	ice_fdir_erase_flow_from_hw(hw, blk, flow);
	for (i = 0; i < prof->cnt; i++)
		prof->vsi_h[i] = 0;
	for (tun = 0; tun < ICE_FD_HW_SEG_MAX; tun++) {
		if (!prof->fdir_seg[tun])
			continue;
		devm_kfree(ice_hw_to_dev(hw), prof->fdir_seg[tun]);
		prof->fdir_seg[tun] = NULL;
	}
	prof->cnt = 0;
}

 
void ice_fdir_release_flows(struct ice_hw *hw)
{
	int flow;

	 
	for (flow = 0; flow < ICE_FLTR_PTYPE_MAX; flow++)
		ice_fdir_erase_flow_from_hw(hw, ICE_BLK_FD, flow);
}

 
void ice_fdir_replay_flows(struct ice_hw *hw)
{
	int flow;

	for (flow = 0; flow < ICE_FLTR_PTYPE_MAX; flow++) {
		int tun;

		if (!hw->fdir_prof[flow] || !hw->fdir_prof[flow]->cnt)
			continue;
		for (tun = 0; tun < ICE_FD_HW_SEG_MAX; tun++) {
			struct ice_flow_prof *hw_prof;
			struct ice_fd_hw_prof *prof;
			u64 prof_id;
			int j;

			prof = hw->fdir_prof[flow];
			prof_id = flow + tun * ICE_FLTR_PTYPE_MAX;
			ice_flow_add_prof(hw, ICE_BLK_FD, ICE_FLOW_RX, prof_id,
					  prof->fdir_seg[tun], TNL_SEG_CNT(tun),
					  &hw_prof);
			for (j = 0; j < prof->cnt; j++) {
				enum ice_flow_priority prio;
				u64 entry_h = 0;
				int err;

				prio = ICE_FLOW_PRIO_NORMAL;
				err = ice_flow_add_entry(hw, ICE_BLK_FD,
							 prof_id,
							 prof->vsi_h[0],
							 prof->vsi_h[j],
							 prio, prof->fdir_seg,
							 &entry_h);
				if (err) {
					dev_err(ice_hw_to_dev(hw), "Could not replay Flow Director, flow type %d\n",
						flow);
					continue;
				}
				prof->entry_h[j][tun] = entry_h;
			}
		}
	}
}

 
static int
ice_parse_rx_flow_user_data(struct ethtool_rx_flow_spec *fsp,
			    struct ice_rx_flow_userdef *data)
{
	u64 value, mask;

	memset(data, 0, sizeof(*data));
	if (!(fsp->flow_type & FLOW_EXT))
		return 0;

	value = be64_to_cpu(*((__force __be64 *)fsp->h_ext.data));
	mask = be64_to_cpu(*((__force __be64 *)fsp->m_ext.data));
	if (!mask)
		return 0;

#define ICE_USERDEF_FLEX_WORD_M	GENMASK_ULL(15, 0)
#define ICE_USERDEF_FLEX_OFFS_S	16
#define ICE_USERDEF_FLEX_OFFS_M	GENMASK_ULL(31, ICE_USERDEF_FLEX_OFFS_S)
#define ICE_USERDEF_FLEX_FLTR_M	GENMASK_ULL(31, 0)

	 
#define ICE_USERDEF_FLEX_MAX_OFFS_VAL 0x1fe

	if (!ice_is_mask_valid(mask, ICE_USERDEF_FLEX_FLTR_M) ||
	    value > ICE_USERDEF_FLEX_FLTR_M)
		return -EINVAL;

	data->flex_word = value & ICE_USERDEF_FLEX_WORD_M;
	data->flex_offset = (value & ICE_USERDEF_FLEX_OFFS_M) >>
			     ICE_USERDEF_FLEX_OFFS_S;
	if (data->flex_offset > ICE_USERDEF_FLEX_MAX_OFFS_VAL)
		return -EINVAL;

	data->flex_fltr = true;

	return 0;
}

 
static int ice_fdir_num_avail_fltr(struct ice_hw *hw, struct ice_vsi *vsi)
{
	u16 vsi_num = ice_get_hw_vsi_num(hw, vsi->idx);
	u16 num_guar;
	u16 num_be;

	 
	num_guar = vsi->num_gfltr;

	 
	num_guar -= (rd32(hw, VSIQF_FD_CNT(vsi_num)) &
		     VSIQF_FD_CNT_FD_GCNT_M) >> VSIQF_FD_CNT_FD_GCNT_S;

	 
	num_be = hw->func_caps.fd_fltr_best_effort;

	 
	num_be -= (rd32(hw, GLQF_FD_CNT) & GLQF_FD_CNT_FD_BCNT_M) >>
		   GLQF_FD_CNT_FD_BCNT_S;

	return num_guar + num_be;
}

 
static int
ice_fdir_alloc_flow_prof(struct ice_hw *hw, enum ice_fltr_ptype flow)
{
	if (!hw)
		return -EINVAL;

	if (!hw->fdir_prof) {
		hw->fdir_prof = devm_kcalloc(ice_hw_to_dev(hw),
					     ICE_FLTR_PTYPE_MAX,
					     sizeof(*hw->fdir_prof),
					     GFP_KERNEL);
		if (!hw->fdir_prof)
			return -ENOMEM;
	}

	if (!hw->fdir_prof[flow]) {
		hw->fdir_prof[flow] = devm_kzalloc(ice_hw_to_dev(hw),
						   sizeof(**hw->fdir_prof),
						   GFP_KERNEL);
		if (!hw->fdir_prof[flow])
			return -ENOMEM;
	}

	return 0;
}

 
static u16
ice_fdir_prof_vsi_idx(struct ice_fd_hw_prof *prof, int vsi_idx)
{
	u16 idx = 0;

	for (idx = 0; idx < prof->cnt; idx++)
		if (prof->vsi_h[idx] == vsi_idx)
			return idx;

	if (idx == prof->cnt)
		prof->vsi_h[prof->cnt++] = vsi_idx;
	return idx;
}

 
static int
ice_fdir_set_hw_fltr_rule(struct ice_pf *pf, struct ice_flow_seg_info *seg,
			  enum ice_fltr_ptype flow, enum ice_fd_hw_seg tun)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_vsi *main_vsi, *ctrl_vsi;
	struct ice_flow_seg_info *old_seg;
	struct ice_flow_prof *prof = NULL;
	struct ice_fd_hw_prof *hw_prof;
	struct ice_hw *hw = &pf->hw;
	u64 entry1_h = 0;
	u64 entry2_h = 0;
	bool del_last;
	u64 prof_id;
	int err;
	int idx;

	main_vsi = ice_get_main_vsi(pf);
	if (!main_vsi)
		return -EINVAL;

	ctrl_vsi = ice_get_ctrl_vsi(pf);
	if (!ctrl_vsi)
		return -EINVAL;

	err = ice_fdir_alloc_flow_prof(hw, flow);
	if (err)
		return err;

	hw_prof = hw->fdir_prof[flow];
	old_seg = hw_prof->fdir_seg[tun];
	if (old_seg) {
		 
		if (!memcmp(old_seg, seg, sizeof(*seg)))
			return -EEXIST;

		 
		if (hw->fdir_fltr_cnt[flow]) {
			dev_err(dev, "Failed to add filter.  Flow director filters on each port must have the same input set.\n");
			return -EINVAL;
		}

		if (ice_is_arfs_using_perfect_flow(hw, flow)) {
			dev_err(dev, "aRFS using perfect flow type %d, cannot change input set\n",
				flow);
			return -EINVAL;
		}

		 
		ice_fdir_rem_flow(hw, ICE_BLK_FD, flow);
	}

	 
	prof_id = flow + tun * ICE_FLTR_PTYPE_MAX;
	err = ice_flow_add_prof(hw, ICE_BLK_FD, ICE_FLOW_RX, prof_id, seg,
				TNL_SEG_CNT(tun), &prof);
	if (err)
		return err;
	err = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id, main_vsi->idx,
				 main_vsi->idx, ICE_FLOW_PRIO_NORMAL,
				 seg, &entry1_h);
	if (err)
		goto err_prof;
	err = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id, main_vsi->idx,
				 ctrl_vsi->idx, ICE_FLOW_PRIO_NORMAL,
				 seg, &entry2_h);
	if (err)
		goto err_entry;

	hw_prof->fdir_seg[tun] = seg;
	hw_prof->entry_h[0][tun] = entry1_h;
	hw_prof->entry_h[1][tun] = entry2_h;
	hw_prof->vsi_h[0] = main_vsi->idx;
	hw_prof->vsi_h[1] = ctrl_vsi->idx;
	if (!hw_prof->cnt)
		hw_prof->cnt = 2;

	for (idx = 1; idx < ICE_CHNL_MAX_TC; idx++) {
		u16 vsi_idx;
		u16 vsi_h;

		if (!ice_is_adq_active(pf) || !main_vsi->tc_map_vsi[idx])
			continue;

		entry1_h = 0;
		vsi_h = main_vsi->tc_map_vsi[idx]->idx;
		err = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id,
					 main_vsi->idx, vsi_h,
					 ICE_FLOW_PRIO_NORMAL, seg,
					 &entry1_h);
		if (err) {
			dev_err(dev, "Could not add Channel VSI %d to flow group\n",
				idx);
			goto err_unroll;
		}

		vsi_idx = ice_fdir_prof_vsi_idx(hw_prof,
						main_vsi->tc_map_vsi[idx]->idx);
		hw_prof->entry_h[vsi_idx][tun] = entry1_h;
	}

	return 0;

err_unroll:
	entry1_h = 0;
	hw_prof->fdir_seg[tun] = NULL;

	 
	del_last = true;
	for (idx = 0; idx < ICE_FD_HW_SEG_MAX; idx++)
		if (hw_prof->fdir_seg[idx]) {
			del_last = false;
			break;
		}

	for (idx = 0; idx < hw_prof->cnt; idx++) {
		u16 vsi_num = ice_get_hw_vsi_num(hw, hw_prof->vsi_h[idx]);

		if (!hw_prof->entry_h[idx][tun])
			continue;
		ice_rem_prof_id_flow(hw, ICE_BLK_FD, vsi_num, prof_id);
		ice_flow_rem_entry(hw, ICE_BLK_FD, hw_prof->entry_h[idx][tun]);
		hw_prof->entry_h[idx][tun] = 0;
		if (del_last)
			hw_prof->vsi_h[idx] = 0;
	}
	if (del_last)
		hw_prof->cnt = 0;
err_entry:
	ice_rem_prof_id_flow(hw, ICE_BLK_FD,
			     ice_get_hw_vsi_num(hw, main_vsi->idx), prof_id);
	ice_flow_rem_entry(hw, ICE_BLK_FD, entry1_h);
err_prof:
	ice_flow_rem_prof(hw, ICE_BLK_FD, prof_id);
	dev_err(dev, "Failed to add filter.  Flow director filters on each port must have the same input set.\n");

	return err;
}

 
static int
ice_set_init_fdir_seg(struct ice_flow_seg_info *seg,
		      enum ice_flow_seg_hdr l3_proto,
		      enum ice_flow_seg_hdr l4_proto)
{
	enum ice_flow_field src_addr, dst_addr, src_port, dst_port;

	if (!seg)
		return -EINVAL;

	if (l3_proto == ICE_FLOW_SEG_HDR_IPV4) {
		src_addr = ICE_FLOW_FIELD_IDX_IPV4_SA;
		dst_addr = ICE_FLOW_FIELD_IDX_IPV4_DA;
	} else if (l3_proto == ICE_FLOW_SEG_HDR_IPV6) {
		src_addr = ICE_FLOW_FIELD_IDX_IPV6_SA;
		dst_addr = ICE_FLOW_FIELD_IDX_IPV6_DA;
	} else {
		return -EINVAL;
	}

	if (l4_proto == ICE_FLOW_SEG_HDR_TCP) {
		src_port = ICE_FLOW_FIELD_IDX_TCP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_TCP_DST_PORT;
	} else if (l4_proto == ICE_FLOW_SEG_HDR_UDP) {
		src_port = ICE_FLOW_FIELD_IDX_UDP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_UDP_DST_PORT;
	} else {
		return -EINVAL;
	}

	ICE_FLOW_SET_HDRS(seg, l3_proto | l4_proto);

	 
	ice_flow_set_fld(seg, src_addr, ICE_FLOW_FLD_OFF_INVAL,
			 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL, false);

	 
	ice_flow_set_fld(seg, dst_addr, ICE_FLOW_FLD_OFF_INVAL,
			 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL, false);

	 
	ice_flow_set_fld(seg, src_port, ICE_FLOW_FLD_OFF_INVAL,
			 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL, false);

	 
	ice_flow_set_fld(seg, dst_port, ICE_FLOW_FLD_OFF_INVAL,
			 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL, false);

	return 0;
}

 
static int
ice_create_init_fdir_rule(struct ice_pf *pf, enum ice_fltr_ptype flow)
{
	struct ice_flow_seg_info *seg, *tun_seg;
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	int ret;

	 
	if (hw->fdir_prof && hw->fdir_prof[flow] &&
	    hw->fdir_prof[flow]->fdir_seg[0])
		return -EINVAL;

	seg = devm_kzalloc(dev, sizeof(*seg), GFP_KERNEL);
	if (!seg)
		return -ENOMEM;

	tun_seg = devm_kcalloc(dev, ICE_FD_HW_SEG_MAX, sizeof(*tun_seg),
			       GFP_KERNEL);
	if (!tun_seg) {
		devm_kfree(dev, seg);
		return -ENOMEM;
	}

	if (flow == ICE_FLTR_PTYPE_NONF_IPV4_TCP)
		ret = ice_set_init_fdir_seg(seg, ICE_FLOW_SEG_HDR_IPV4,
					    ICE_FLOW_SEG_HDR_TCP);
	else if (flow == ICE_FLTR_PTYPE_NONF_IPV4_UDP)
		ret = ice_set_init_fdir_seg(seg, ICE_FLOW_SEG_HDR_IPV4,
					    ICE_FLOW_SEG_HDR_UDP);
	else if (flow == ICE_FLTR_PTYPE_NONF_IPV6_TCP)
		ret = ice_set_init_fdir_seg(seg, ICE_FLOW_SEG_HDR_IPV6,
					    ICE_FLOW_SEG_HDR_TCP);
	else if (flow == ICE_FLTR_PTYPE_NONF_IPV6_UDP)
		ret = ice_set_init_fdir_seg(seg, ICE_FLOW_SEG_HDR_IPV6,
					    ICE_FLOW_SEG_HDR_UDP);
	else
		ret = -EINVAL;
	if (ret)
		goto err_exit;

	 
	ret = ice_fdir_set_hw_fltr_rule(pf, seg, flow, ICE_FD_HW_SEG_NON_TUN);
	if (ret)
		 
		goto err_exit;

	 
	memcpy(&tun_seg[1], seg, sizeof(*seg));
	ret = ice_fdir_set_hw_fltr_rule(pf, tun_seg, flow, ICE_FD_HW_SEG_TUN);
	if (ret)
		 
		devm_kfree(dev, tun_seg);

	set_bit(flow, hw->fdir_perfect_fltr);
	return ret;
err_exit:
	devm_kfree(dev, tun_seg);
	devm_kfree(dev, seg);

	return -EOPNOTSUPP;
}

 
static int
ice_set_fdir_ip4_seg(struct ice_flow_seg_info *seg,
		     struct ethtool_tcpip4_spec *tcp_ip4_spec,
		     enum ice_flow_seg_hdr l4_proto, bool *perfect_fltr)
{
	enum ice_flow_field src_port, dst_port;

	 
	if (!tcp_ip4_spec->psrc && !tcp_ip4_spec->ip4src &&
	    !tcp_ip4_spec->pdst && !tcp_ip4_spec->ip4dst)
		return -EINVAL;

	 
	if (tcp_ip4_spec->tos)
		return -EOPNOTSUPP;

	if (l4_proto == ICE_FLOW_SEG_HDR_TCP) {
		src_port = ICE_FLOW_FIELD_IDX_TCP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_TCP_DST_PORT;
	} else if (l4_proto == ICE_FLOW_SEG_HDR_UDP) {
		src_port = ICE_FLOW_FIELD_IDX_UDP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_UDP_DST_PORT;
	} else if (l4_proto == ICE_FLOW_SEG_HDR_SCTP) {
		src_port = ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_SCTP_DST_PORT;
	} else {
		return -EOPNOTSUPP;
	}

	*perfect_fltr = true;
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4 | l4_proto);

	 
	if (tcp_ip4_spec->ip4src == htonl(0xFFFFFFFF))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV4_SA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!tcp_ip4_spec->ip4src)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	 
	if (tcp_ip4_spec->ip4dst == htonl(0xFFFFFFFF))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV4_DA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!tcp_ip4_spec->ip4dst)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	 
	if (tcp_ip4_spec->psrc == htons(0xFFFF))
		ice_flow_set_fld(seg, src_port, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 false);
	else if (!tcp_ip4_spec->psrc)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	 
	if (tcp_ip4_spec->pdst == htons(0xFFFF))
		ice_flow_set_fld(seg, dst_port, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 false);
	else if (!tcp_ip4_spec->pdst)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	return 0;
}

 
static int
ice_set_fdir_ip4_usr_seg(struct ice_flow_seg_info *seg,
			 struct ethtool_usrip4_spec *usr_ip4_spec,
			 bool *perfect_fltr)
{
	 
	if (usr_ip4_spec->l4_4_bytes)
		return -EINVAL;
	if (usr_ip4_spec->tos)
		return -EINVAL;
	if (usr_ip4_spec->ip_ver)
		return -EINVAL;
	 
	if (usr_ip4_spec->proto)
		return -EOPNOTSUPP;
	 
	if (!usr_ip4_spec->ip4src && !usr_ip4_spec->ip4dst)
		return -EINVAL;

	*perfect_fltr = true;
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4);

	 
	if (usr_ip4_spec->ip4src == htonl(0xFFFFFFFF))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV4_SA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!usr_ip4_spec->ip4src)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	 
	if (usr_ip4_spec->ip4dst == htonl(0xFFFFFFFF))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV4_DA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!usr_ip4_spec->ip4dst)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	return 0;
}

 
static int
ice_set_fdir_ip6_seg(struct ice_flow_seg_info *seg,
		     struct ethtool_tcpip6_spec *tcp_ip6_spec,
		     enum ice_flow_seg_hdr l4_proto, bool *perfect_fltr)
{
	enum ice_flow_field src_port, dst_port;

	 
	if (!memcmp(tcp_ip6_spec->ip6src, &zero_ipv6_addr_mask,
		    sizeof(struct in6_addr)) &&
	    !memcmp(tcp_ip6_spec->ip6dst, &zero_ipv6_addr_mask,
		    sizeof(struct in6_addr)) &&
	    !tcp_ip6_spec->psrc && !tcp_ip6_spec->pdst)
		return -EINVAL;

	 
	if (tcp_ip6_spec->tclass)
		return -EOPNOTSUPP;

	if (l4_proto == ICE_FLOW_SEG_HDR_TCP) {
		src_port = ICE_FLOW_FIELD_IDX_TCP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_TCP_DST_PORT;
	} else if (l4_proto == ICE_FLOW_SEG_HDR_UDP) {
		src_port = ICE_FLOW_FIELD_IDX_UDP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_UDP_DST_PORT;
	} else if (l4_proto == ICE_FLOW_SEG_HDR_SCTP) {
		src_port = ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT;
		dst_port = ICE_FLOW_FIELD_IDX_SCTP_DST_PORT;
	} else {
		return -EINVAL;
	}

	*perfect_fltr = true;
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV6 | l4_proto);

	if (!memcmp(tcp_ip6_spec->ip6src, &full_ipv6_addr_mask,
		    sizeof(struct in6_addr)))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV6_SA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!memcmp(tcp_ip6_spec->ip6src, &zero_ipv6_addr_mask,
			 sizeof(struct in6_addr)))
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	if (!memcmp(tcp_ip6_spec->ip6dst, &full_ipv6_addr_mask,
		    sizeof(struct in6_addr)))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV6_DA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!memcmp(tcp_ip6_spec->ip6dst, &zero_ipv6_addr_mask,
			 sizeof(struct in6_addr)))
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	 
	if (tcp_ip6_spec->psrc == htons(0xFFFF))
		ice_flow_set_fld(seg, src_port, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 false);
	else if (!tcp_ip6_spec->psrc)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	 
	if (tcp_ip6_spec->pdst == htons(0xFFFF))
		ice_flow_set_fld(seg, dst_port, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 false);
	else if (!tcp_ip6_spec->pdst)
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	return 0;
}

 
static int
ice_set_fdir_ip6_usr_seg(struct ice_flow_seg_info *seg,
			 struct ethtool_usrip6_spec *usr_ip6_spec,
			 bool *perfect_fltr)
{
	 
	if (usr_ip6_spec->l4_4_bytes)
		return -EOPNOTSUPP;
	 
	if (usr_ip6_spec->tclass)
		return -EOPNOTSUPP;
	 
	if (usr_ip6_spec->l4_proto)
		return -EOPNOTSUPP;
	 
	if (!memcmp(usr_ip6_spec->ip6src, &zero_ipv6_addr_mask,
		    sizeof(struct in6_addr)) &&
	    !memcmp(usr_ip6_spec->ip6dst, &zero_ipv6_addr_mask,
		    sizeof(struct in6_addr)))
		return -EINVAL;

	*perfect_fltr = true;
	ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV6);

	if (!memcmp(usr_ip6_spec->ip6src, &full_ipv6_addr_mask,
		    sizeof(struct in6_addr)))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV6_SA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!memcmp(usr_ip6_spec->ip6src, &zero_ipv6_addr_mask,
			 sizeof(struct in6_addr)))
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	if (!memcmp(usr_ip6_spec->ip6dst, &full_ipv6_addr_mask,
		    sizeof(struct in6_addr)))
		ice_flow_set_fld(seg, ICE_FLOW_FIELD_IDX_IPV6_DA,
				 ICE_FLOW_FLD_OFF_INVAL, ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	else if (!memcmp(usr_ip6_spec->ip6dst, &zero_ipv6_addr_mask,
			 sizeof(struct in6_addr)))
		*perfect_fltr = false;
	else
		return -EOPNOTSUPP;

	return 0;
}

 
static int
ice_cfg_fdir_xtrct_seq(struct ice_pf *pf, struct ethtool_rx_flow_spec *fsp,
		       struct ice_rx_flow_userdef *user)
{
	struct ice_flow_seg_info *seg, *tun_seg;
	struct device *dev = ice_pf_to_dev(pf);
	enum ice_fltr_ptype fltr_idx;
	struct ice_hw *hw = &pf->hw;
	bool perfect_filter;
	int ret;

	seg = devm_kzalloc(dev, sizeof(*seg), GFP_KERNEL);
	if (!seg)
		return -ENOMEM;

	tun_seg = devm_kcalloc(dev, ICE_FD_HW_SEG_MAX, sizeof(*tun_seg),
			       GFP_KERNEL);
	if (!tun_seg) {
		devm_kfree(dev, seg);
		return -ENOMEM;
	}

	switch (fsp->flow_type & ~FLOW_EXT) {
	case TCP_V4_FLOW:
		ret = ice_set_fdir_ip4_seg(seg, &fsp->m_u.tcp_ip4_spec,
					   ICE_FLOW_SEG_HDR_TCP,
					   &perfect_filter);
		break;
	case UDP_V4_FLOW:
		ret = ice_set_fdir_ip4_seg(seg, &fsp->m_u.tcp_ip4_spec,
					   ICE_FLOW_SEG_HDR_UDP,
					   &perfect_filter);
		break;
	case SCTP_V4_FLOW:
		ret = ice_set_fdir_ip4_seg(seg, &fsp->m_u.tcp_ip4_spec,
					   ICE_FLOW_SEG_HDR_SCTP,
					   &perfect_filter);
		break;
	case IPV4_USER_FLOW:
		ret = ice_set_fdir_ip4_usr_seg(seg, &fsp->m_u.usr_ip4_spec,
					       &perfect_filter);
		break;
	case TCP_V6_FLOW:
		ret = ice_set_fdir_ip6_seg(seg, &fsp->m_u.tcp_ip6_spec,
					   ICE_FLOW_SEG_HDR_TCP,
					   &perfect_filter);
		break;
	case UDP_V6_FLOW:
		ret = ice_set_fdir_ip6_seg(seg, &fsp->m_u.tcp_ip6_spec,
					   ICE_FLOW_SEG_HDR_UDP,
					   &perfect_filter);
		break;
	case SCTP_V6_FLOW:
		ret = ice_set_fdir_ip6_seg(seg, &fsp->m_u.tcp_ip6_spec,
					   ICE_FLOW_SEG_HDR_SCTP,
					   &perfect_filter);
		break;
	case IPV6_USER_FLOW:
		ret = ice_set_fdir_ip6_usr_seg(seg, &fsp->m_u.usr_ip6_spec,
					       &perfect_filter);
		break;
	default:
		ret = -EINVAL;
	}
	if (ret)
		goto err_exit;

	 
	memcpy(&tun_seg[1], seg, sizeof(*seg));

	if (user && user->flex_fltr) {
		perfect_filter = false;
		ice_flow_add_fld_raw(seg, user->flex_offset,
				     ICE_FLTR_PRGM_FLEX_WORD_SIZE,
				     ICE_FLOW_FLD_OFF_INVAL,
				     ICE_FLOW_FLD_OFF_INVAL);
		ice_flow_add_fld_raw(&tun_seg[1], user->flex_offset,
				     ICE_FLTR_PRGM_FLEX_WORD_SIZE,
				     ICE_FLOW_FLD_OFF_INVAL,
				     ICE_FLOW_FLD_OFF_INVAL);
	}

	fltr_idx = ice_ethtool_flow_to_fltr(fsp->flow_type & ~FLOW_EXT);

	assign_bit(fltr_idx, hw->fdir_perfect_fltr, perfect_filter);

	 
	ret = ice_fdir_set_hw_fltr_rule(pf, seg, fltr_idx,
					ICE_FD_HW_SEG_NON_TUN);
	if (ret == -EEXIST) {
		 
		ret = 0;
		goto err_exit;
	} else if (ret) {
		 
		goto err_exit;
	}

	 
	memcpy(&tun_seg[1], seg, sizeof(*seg));
	ret = ice_fdir_set_hw_fltr_rule(pf, tun_seg, fltr_idx,
					ICE_FD_HW_SEG_TUN);
	if (ret == -EEXIST) {
		 
		devm_kfree(dev, tun_seg);
		ret = 0;
	} else if (ret) {
		 
		devm_kfree(dev, tun_seg);
	}

	return ret;

err_exit:
	devm_kfree(dev, tun_seg);
	devm_kfree(dev, seg);

	return ret;
}

 
static void ice_update_per_q_fltr(struct ice_vsi *vsi, u32 q_index, bool inc)
{
	struct ice_rx_ring *rx_ring;

	if (!vsi->num_rxq || q_index >= vsi->num_rxq)
		return;

	rx_ring = vsi->rx_rings[q_index];
	if (!rx_ring || !rx_ring->ch)
		return;

	if (inc)
		atomic_inc(&rx_ring->ch->num_sb_fltr);
	else
		atomic_dec_if_positive(&rx_ring->ch->num_sb_fltr);
}

 
int
ice_fdir_write_fltr(struct ice_pf *pf, struct ice_fdir_fltr *input, bool add,
		    bool is_tun)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	struct ice_fltr_desc desc;
	struct ice_vsi *ctrl_vsi;
	u8 *pkt, *frag_pkt;
	bool has_frag;
	int err;

	ctrl_vsi = ice_get_ctrl_vsi(pf);
	if (!ctrl_vsi)
		return -EINVAL;

	pkt = devm_kzalloc(dev, ICE_FDIR_MAX_RAW_PKT_SIZE, GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;
	frag_pkt = devm_kzalloc(dev, ICE_FDIR_MAX_RAW_PKT_SIZE, GFP_KERNEL);
	if (!frag_pkt) {
		err = -ENOMEM;
		goto err_free;
	}

	ice_fdir_get_prgm_desc(hw, input, &desc, add);
	err = ice_fdir_get_gen_prgm_pkt(hw, input, pkt, false, is_tun);
	if (err)
		goto err_free_all;
	err = ice_prgm_fdir_fltr(ctrl_vsi, &desc, pkt);
	if (err)
		goto err_free_all;

	 
	has_frag = ice_fdir_has_frag(input->flow_type);
	if (has_frag) {
		 
		ice_fdir_get_prgm_desc(hw, input, &desc, add);
		err = ice_fdir_get_gen_prgm_pkt(hw, input, frag_pkt, true,
						is_tun);
		if (err)
			goto err_frag;
		err = ice_prgm_fdir_fltr(ctrl_vsi, &desc, frag_pkt);
		if (err)
			goto err_frag;
	} else {
		devm_kfree(dev, frag_pkt);
	}

	return 0;

err_free_all:
	devm_kfree(dev, frag_pkt);
err_free:
	devm_kfree(dev, pkt);
	return err;

err_frag:
	devm_kfree(dev, frag_pkt);
	return err;
}

 
static int
ice_fdir_write_all_fltr(struct ice_pf *pf, struct ice_fdir_fltr *input,
			bool add)
{
	u16 port_num;
	int tun;

	for (tun = 0; tun < ICE_FD_HW_SEG_MAX; tun++) {
		bool is_tun = tun == ICE_FD_HW_SEG_TUN;
		int err;

		if (is_tun && !ice_get_open_tunnel_port(&pf->hw, &port_num, TNL_ALL))
			continue;
		err = ice_fdir_write_fltr(pf, input, add, is_tun);
		if (err)
			return err;
	}
	return 0;
}

 
void ice_fdir_replay_fltrs(struct ice_pf *pf)
{
	struct ice_fdir_fltr *f_rule;
	struct ice_hw *hw = &pf->hw;

	list_for_each_entry(f_rule, &hw->fdir_list_head, fltr_node) {
		int err = ice_fdir_write_all_fltr(pf, f_rule, true);

		if (err)
			dev_dbg(ice_pf_to_dev(pf), "Flow Director error %d, could not reprogram filter %d\n",
				err, f_rule->fltr_id);
	}
}

 
int ice_fdir_create_dflt_rules(struct ice_pf *pf)
{
	int err;

	 
	err = ice_create_init_fdir_rule(pf, ICE_FLTR_PTYPE_NONF_IPV4_TCP);
	if (err)
		return err;

	err = ice_create_init_fdir_rule(pf, ICE_FLTR_PTYPE_NONF_IPV4_UDP);
	if (err)
		return err;

	err = ice_create_init_fdir_rule(pf, ICE_FLTR_PTYPE_NONF_IPV6_TCP);
	if (err)
		return err;

	err = ice_create_init_fdir_rule(pf, ICE_FLTR_PTYPE_NONF_IPV6_UDP);

	return err;
}

 
void ice_fdir_del_all_fltrs(struct ice_vsi *vsi)
{
	struct ice_fdir_fltr *f_rule, *tmp;
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;

	list_for_each_entry_safe(f_rule, tmp, &hw->fdir_list_head, fltr_node) {
		ice_fdir_write_all_fltr(pf, f_rule, false);
		ice_fdir_update_cntrs(hw, f_rule->flow_type, false);
		list_del(&f_rule->fltr_node);
		devm_kfree(ice_pf_to_dev(pf), f_rule);
	}
}

 
void ice_vsi_manage_fdir(struct ice_vsi *vsi, bool ena)
{
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	enum ice_fltr_ptype flow;

	if (ena) {
		set_bit(ICE_FLAG_FD_ENA, pf->flags);
		ice_fdir_create_dflt_rules(pf);
		return;
	}

	mutex_lock(&hw->fdir_fltr_lock);
	if (!test_and_clear_bit(ICE_FLAG_FD_ENA, pf->flags))
		goto release_lock;

	ice_fdir_del_all_fltrs(vsi);

	if (hw->fdir_prof)
		for (flow = ICE_FLTR_PTYPE_NONF_NONE; flow < ICE_FLTR_PTYPE_MAX;
		     flow++)
			if (hw->fdir_prof[flow])
				ice_fdir_rem_flow(hw, ICE_BLK_FD, flow);

release_lock:
	mutex_unlock(&hw->fdir_fltr_lock);
}

 
static void
ice_fdir_do_rem_flow(struct ice_pf *pf, enum ice_fltr_ptype flow_type)
{
	struct ice_hw *hw = &pf->hw;
	bool need_perfect = false;

	if (flow_type == ICE_FLTR_PTYPE_NONF_IPV4_TCP ||
	    flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP ||
	    flow_type == ICE_FLTR_PTYPE_NONF_IPV6_TCP ||
	    flow_type == ICE_FLTR_PTYPE_NONF_IPV6_UDP)
		need_perfect = true;

	if (need_perfect && test_bit(flow_type, hw->fdir_perfect_fltr))
		return;

	ice_fdir_rem_flow(hw, ICE_BLK_FD, flow_type);
	if (need_perfect)
		ice_create_init_fdir_rule(pf, flow_type);
}

 
static int
ice_fdir_update_list_entry(struct ice_pf *pf, struct ice_fdir_fltr *input,
			   int fltr_idx)
{
	struct ice_fdir_fltr *old_fltr;
	struct ice_hw *hw = &pf->hw;
	struct ice_vsi *vsi;
	int err = -ENOENT;

	 
	if (ice_is_reset_in_progress(pf->state))
		return -EBUSY;

	vsi = ice_get_main_vsi(pf);
	if (!vsi)
		return -EINVAL;

	old_fltr = ice_fdir_find_fltr_by_idx(hw, fltr_idx);
	if (old_fltr) {
		err = ice_fdir_write_all_fltr(pf, old_fltr, false);
		if (err)
			return err;
		ice_fdir_update_cntrs(hw, old_fltr->flow_type, false);
		 
		ice_update_per_q_fltr(vsi, old_fltr->orig_q_index, false);
		if (!input && !hw->fdir_fltr_cnt[old_fltr->flow_type])
			 
			ice_fdir_do_rem_flow(pf, old_fltr->flow_type);
		list_del(&old_fltr->fltr_node);
		devm_kfree(ice_hw_to_dev(hw), old_fltr);
	}
	if (!input)
		return err;
	ice_fdir_list_add_fltr(hw, input);
	 
	ice_update_per_q_fltr(vsi, input->orig_q_index, true);
	ice_fdir_update_cntrs(hw, input->flow_type, true);
	return 0;
}

 
int ice_del_fdir_ethtool(struct ice_vsi *vsi, struct ethtool_rxnfc *cmd)
{
	struct ethtool_rx_flow_spec *fsp =
		(struct ethtool_rx_flow_spec *)&cmd->fs;
	struct ice_pf *pf = vsi->back;
	struct ice_hw *hw = &pf->hw;
	int val;

	if (!test_bit(ICE_FLAG_FD_ENA, pf->flags))
		return -EOPNOTSUPP;

	 
	if (ice_is_reset_in_progress(pf->state)) {
		dev_err(ice_pf_to_dev(pf), "Device is resetting - deleting Flow Director filters not supported during reset\n");
		return -EBUSY;
	}

	if (test_bit(ICE_FD_FLUSH_REQ, pf->state))
		return -EBUSY;

	mutex_lock(&hw->fdir_fltr_lock);
	val = ice_fdir_update_list_entry(pf, NULL, fsp->location);
	mutex_unlock(&hw->fdir_fltr_lock);

	return val;
}

 
static void
ice_update_ring_dest_vsi(struct ice_vsi *vsi, u16 *dest_vsi, u32 *ring)
{
	struct ice_channel *ch;

	list_for_each_entry(ch, &vsi->ch_list, list) {
		if (!ch->ch_vsi)
			continue;

		 
		if ((*ring < ch->base_q) ||
		    (*ring >= (ch->base_q + ch->num_rxq)))
			continue;

		 
		*dest_vsi = ch->ch_vsi->idx;

		 
		*ring -= ch->base_q;
	}
}

 
static int
ice_set_fdir_input_set(struct ice_vsi *vsi, struct ethtool_rx_flow_spec *fsp,
		       struct ice_fdir_fltr *input)
{
	u16 dest_vsi, q_index = 0;
	u16 orig_q_index = 0;
	struct ice_pf *pf;
	struct ice_hw *hw;
	int flow_type;
	u8 dest_ctl;

	if (!vsi || !fsp || !input)
		return -EINVAL;

	pf = vsi->back;
	hw = &pf->hw;

	dest_vsi = vsi->idx;
	if (fsp->ring_cookie == RX_CLS_FLOW_DISC) {
		dest_ctl = ICE_FLTR_PRGM_DESC_DEST_DROP_PKT;
	} else {
		u32 ring = ethtool_get_flow_spec_ring(fsp->ring_cookie);
		u8 vf = ethtool_get_flow_spec_ring_vf(fsp->ring_cookie);

		if (vf) {
			dev_err(ice_pf_to_dev(pf), "Failed to add filter. Flow director filters are not supported on VF queues.\n");
			return -EINVAL;
		}

		if (ring >= vsi->num_rxq)
			return -EINVAL;

		orig_q_index = ring;
		ice_update_ring_dest_vsi(vsi, &dest_vsi, &ring);
		dest_ctl = ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QINDEX;
		q_index = ring;
	}

	input->fltr_id = fsp->location;
	input->q_index = q_index;
	flow_type = fsp->flow_type & ~FLOW_EXT;

	 
	input->orig_q_index = orig_q_index;
	input->dest_vsi = dest_vsi;
	input->dest_ctl = dest_ctl;
	input->fltr_status = ICE_FLTR_PRGM_DESC_FD_STATUS_FD_ID;
	input->cnt_index = ICE_FD_SB_STAT_IDX(hw->fd_ctr_base);
	input->flow_type = ice_ethtool_flow_to_fltr(flow_type);

	if (fsp->flow_type & FLOW_EXT) {
		memcpy(input->ext_data.usr_def, fsp->h_ext.data,
		       sizeof(input->ext_data.usr_def));
		input->ext_data.vlan_type = fsp->h_ext.vlan_etype;
		input->ext_data.vlan_tag = fsp->h_ext.vlan_tci;
		memcpy(input->ext_mask.usr_def, fsp->m_ext.data,
		       sizeof(input->ext_mask.usr_def));
		input->ext_mask.vlan_type = fsp->m_ext.vlan_etype;
		input->ext_mask.vlan_tag = fsp->m_ext.vlan_tci;
	}

	switch (flow_type) {
	case TCP_V4_FLOW:
	case UDP_V4_FLOW:
	case SCTP_V4_FLOW:
		input->ip.v4.dst_port = fsp->h_u.tcp_ip4_spec.pdst;
		input->ip.v4.src_port = fsp->h_u.tcp_ip4_spec.psrc;
		input->ip.v4.dst_ip = fsp->h_u.tcp_ip4_spec.ip4dst;
		input->ip.v4.src_ip = fsp->h_u.tcp_ip4_spec.ip4src;
		input->mask.v4.dst_port = fsp->m_u.tcp_ip4_spec.pdst;
		input->mask.v4.src_port = fsp->m_u.tcp_ip4_spec.psrc;
		input->mask.v4.dst_ip = fsp->m_u.tcp_ip4_spec.ip4dst;
		input->mask.v4.src_ip = fsp->m_u.tcp_ip4_spec.ip4src;
		break;
	case IPV4_USER_FLOW:
		input->ip.v4.dst_ip = fsp->h_u.usr_ip4_spec.ip4dst;
		input->ip.v4.src_ip = fsp->h_u.usr_ip4_spec.ip4src;
		input->ip.v4.l4_header = fsp->h_u.usr_ip4_spec.l4_4_bytes;
		input->ip.v4.proto = fsp->h_u.usr_ip4_spec.proto;
		input->ip.v4.ip_ver = fsp->h_u.usr_ip4_spec.ip_ver;
		input->ip.v4.tos = fsp->h_u.usr_ip4_spec.tos;
		input->mask.v4.dst_ip = fsp->m_u.usr_ip4_spec.ip4dst;
		input->mask.v4.src_ip = fsp->m_u.usr_ip4_spec.ip4src;
		input->mask.v4.l4_header = fsp->m_u.usr_ip4_spec.l4_4_bytes;
		input->mask.v4.proto = fsp->m_u.usr_ip4_spec.proto;
		input->mask.v4.ip_ver = fsp->m_u.usr_ip4_spec.ip_ver;
		input->mask.v4.tos = fsp->m_u.usr_ip4_spec.tos;
		break;
	case TCP_V6_FLOW:
	case UDP_V6_FLOW:
	case SCTP_V6_FLOW:
		memcpy(input->ip.v6.dst_ip, fsp->h_u.usr_ip6_spec.ip6dst,
		       sizeof(struct in6_addr));
		memcpy(input->ip.v6.src_ip, fsp->h_u.usr_ip6_spec.ip6src,
		       sizeof(struct in6_addr));
		input->ip.v6.dst_port = fsp->h_u.tcp_ip6_spec.pdst;
		input->ip.v6.src_port = fsp->h_u.tcp_ip6_spec.psrc;
		input->ip.v6.tc = fsp->h_u.tcp_ip6_spec.tclass;
		memcpy(input->mask.v6.dst_ip, fsp->m_u.tcp_ip6_spec.ip6dst,
		       sizeof(struct in6_addr));
		memcpy(input->mask.v6.src_ip, fsp->m_u.tcp_ip6_spec.ip6src,
		       sizeof(struct in6_addr));
		input->mask.v6.dst_port = fsp->m_u.tcp_ip6_spec.pdst;
		input->mask.v6.src_port = fsp->m_u.tcp_ip6_spec.psrc;
		input->mask.v6.tc = fsp->m_u.tcp_ip6_spec.tclass;
		break;
	case IPV6_USER_FLOW:
		memcpy(input->ip.v6.dst_ip, fsp->h_u.usr_ip6_spec.ip6dst,
		       sizeof(struct in6_addr));
		memcpy(input->ip.v6.src_ip, fsp->h_u.usr_ip6_spec.ip6src,
		       sizeof(struct in6_addr));
		input->ip.v6.l4_header = fsp->h_u.usr_ip6_spec.l4_4_bytes;
		input->ip.v6.tc = fsp->h_u.usr_ip6_spec.tclass;

		 
		if (!fsp->m_u.usr_ip6_spec.l4_proto)
			input->ip.v6.proto = IPPROTO_NONE;
		else
			input->ip.v6.proto = fsp->h_u.usr_ip6_spec.l4_proto;

		memcpy(input->mask.v6.dst_ip, fsp->m_u.usr_ip6_spec.ip6dst,
		       sizeof(struct in6_addr));
		memcpy(input->mask.v6.src_ip, fsp->m_u.usr_ip6_spec.ip6src,
		       sizeof(struct in6_addr));
		input->mask.v6.l4_header = fsp->m_u.usr_ip6_spec.l4_4_bytes;
		input->mask.v6.tc = fsp->m_u.usr_ip6_spec.tclass;
		input->mask.v6.proto = fsp->m_u.usr_ip6_spec.l4_proto;
		break;
	default:
		 
		return -EINVAL;
	}

	return 0;
}

 
int ice_add_fdir_ethtool(struct ice_vsi *vsi, struct ethtool_rxnfc *cmd)
{
	struct ice_rx_flow_userdef userdata;
	struct ethtool_rx_flow_spec *fsp;
	struct ice_fdir_fltr *input;
	struct device *dev;
	struct ice_pf *pf;
	struct ice_hw *hw;
	int fltrs_needed;
	u16 tunnel_port;
	int ret;

	if (!vsi)
		return -EINVAL;

	pf = vsi->back;
	hw = &pf->hw;
	dev = ice_pf_to_dev(pf);

	if (!test_bit(ICE_FLAG_FD_ENA, pf->flags))
		return -EOPNOTSUPP;

	 
	if (ice_is_reset_in_progress(pf->state)) {
		dev_err(dev, "Device is resetting - adding Flow Director filters not supported during reset\n");
		return -EBUSY;
	}

	fsp = (struct ethtool_rx_flow_spec *)&cmd->fs;

	if (ice_parse_rx_flow_user_data(fsp, &userdata))
		return -EINVAL;

	if (fsp->flow_type & FLOW_MAC_EXT)
		return -EINVAL;

	ret = ice_cfg_fdir_xtrct_seq(pf, fsp, &userdata);
	if (ret)
		return ret;

	if (fsp->location >= ice_get_fdir_cnt_all(hw)) {
		dev_err(dev, "Failed to add filter.  The maximum number of flow director filters has been reached.\n");
		return -ENOSPC;
	}

	 
	fltrs_needed = ice_get_open_tunnel_port(hw, &tunnel_port, TNL_ALL) ? 2 : 1;
	if (!ice_fdir_find_fltr_by_idx(hw, fsp->location) &&
	    ice_fdir_num_avail_fltr(hw, pf->vsi[vsi->idx]) < fltrs_needed) {
		dev_err(dev, "Failed to add filter.  The maximum number of flow director filters has been reached.\n");
		return -ENOSPC;
	}

	input = devm_kzalloc(dev, sizeof(*input), GFP_KERNEL);
	if (!input)
		return -ENOMEM;

	ret = ice_set_fdir_input_set(vsi, fsp, input);
	if (ret)
		goto free_input;

	mutex_lock(&hw->fdir_fltr_lock);
	if (ice_fdir_is_dup_fltr(hw, input)) {
		ret = -EINVAL;
		goto release_lock;
	}

	if (userdata.flex_fltr) {
		input->flex_fltr = true;
		input->flex_word = cpu_to_be16(userdata.flex_word);
		input->flex_offset = userdata.flex_offset;
	}

	input->cnt_ena = ICE_FXD_FLTR_QW0_STAT_ENA_PKTS;
	input->fdid_prio = ICE_FXD_FLTR_QW1_FDID_PRI_THREE;
	input->comp_report = ICE_FXD_FLTR_QW0_COMP_REPORT_SW_FAIL;

	 
	ret = ice_fdir_update_list_entry(pf, input, fsp->location);
	if (ret)
		goto release_lock;

	ret = ice_fdir_write_all_fltr(pf, input, true);
	if (ret)
		goto remove_sw_rule;

	goto release_lock;

remove_sw_rule:
	ice_fdir_update_cntrs(hw, input->flow_type, false);
	 
	ice_update_per_q_fltr(vsi, input->orig_q_index, false);
	list_del(&input->fltr_node);
release_lock:
	mutex_unlock(&hw->fdir_fltr_lock);
free_input:
	if (ret)
		devm_kfree(dev, input);

	return ret;
}

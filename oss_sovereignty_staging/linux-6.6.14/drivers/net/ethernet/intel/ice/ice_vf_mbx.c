
 

#include "ice_common.h"
#include "ice_vf_mbx.h"

 
int
ice_aq_send_msg_to_vf(struct ice_hw *hw, u16 vfid, u32 v_opcode, u32 v_retval,
		      u8 *msg, u16 msglen, struct ice_sq_cd *cd)
{
	struct ice_aqc_pf_vf_msg *cmd;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_mbx_opc_send_msg_to_vf);

	cmd = &desc.params.virt;
	cmd->id = cpu_to_le32(vfid);

	desc.cookie_high = cpu_to_le32(v_opcode);
	desc.cookie_low = cpu_to_le32(v_retval);

	if (msglen)
		desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	return ice_sq_send_cmd(hw, &hw->mailboxq, &desc, msg, msglen, cd);
}

static const u32 ice_legacy_aq_to_vc_speed[] = {
	VIRTCHNL_LINK_SPEED_100MB,	 
	VIRTCHNL_LINK_SPEED_100MB,
	VIRTCHNL_LINK_SPEED_1GB,
	VIRTCHNL_LINK_SPEED_1GB,
	VIRTCHNL_LINK_SPEED_1GB,
	VIRTCHNL_LINK_SPEED_10GB,
	VIRTCHNL_LINK_SPEED_20GB,
	VIRTCHNL_LINK_SPEED_25GB,
	VIRTCHNL_LINK_SPEED_40GB,
	VIRTCHNL_LINK_SPEED_40GB,
	VIRTCHNL_LINK_SPEED_40GB,
};

 
u32 ice_conv_link_speed_to_virtchnl(bool adv_link_support, u16 link_speed)
{
	 
	u32 index = fls(link_speed) - 1;

	if (adv_link_support)
		return ice_get_link_speed(index);
	else if (index < ARRAY_SIZE(ice_legacy_aq_to_vc_speed))
		 
		return ice_legacy_aq_to_vc_speed[index];

	return VIRTCHNL_LINK_SPEED_UNKNOWN;
}

 
#define ICE_RQ_DATA_MASK(rq_data) ((rq_data) & PF_MBX_ARQH_ARQH_M)
 
#define ICE_IGNORE_MAX_MSG_CNT	0xFFFF

 
static void ice_mbx_reset_snapshot(struct ice_mbx_snapshot *snap)
{
	struct ice_mbx_vf_info *vf_info;

	 
	memset(&snap->mbx_buf, 0, sizeof(snap->mbx_buf));
	snap->mbx_buf.state = ICE_MAL_VF_DETECT_STATE_NEW_SNAPSHOT;

	 
	list_for_each_entry(vf_info, &snap->mbx_vf, list_entry)
		vf_info->msg_count = 0;
}

 
static void
ice_mbx_traverse(struct ice_hw *hw,
		 enum ice_mbx_snapshot_state *new_state)
{
	struct ice_mbx_snap_buffer_data *snap_buf;
	u32 num_iterations;

	snap_buf = &hw->mbx_snapshot.mbx_buf;

	 
	num_iterations = ICE_RQ_DATA_MASK(++snap_buf->num_iterations);

	 
	if (num_iterations == snap_buf->head ||
	    (snap_buf->max_num_msgs_mbx < ICE_IGNORE_MAX_MSG_CNT &&
	     ++snap_buf->num_msg_proc >= snap_buf->max_num_msgs_mbx))
		*new_state = ICE_MAL_VF_DETECT_STATE_NEW_SNAPSHOT;
}

 
static int
ice_mbx_detect_malvf(struct ice_hw *hw, struct ice_mbx_vf_info *vf_info,
		     enum ice_mbx_snapshot_state *new_state,
		     bool *is_malvf)
{
	 
	vf_info->msg_count++;

	if (vf_info->msg_count >= ICE_ASYNC_VF_MSG_THRESHOLD)
		*is_malvf = true;

	 
	ice_mbx_traverse(hw, new_state);

	return 0;
}

 
int
ice_mbx_vf_state_handler(struct ice_hw *hw, struct ice_mbx_data *mbx_data,
			 struct ice_mbx_vf_info *vf_info, bool *report_malvf)
{
	struct ice_mbx_snapshot *snap = &hw->mbx_snapshot;
	struct ice_mbx_snap_buffer_data *snap_buf;
	struct ice_ctl_q_info *cq = &hw->mailboxq;
	enum ice_mbx_snapshot_state new_state;
	bool is_malvf = false;
	int status = 0;

	if (!report_malvf || !mbx_data || !vf_info)
		return -EINVAL;

	*report_malvf = false;

	 
	  
	if (mbx_data->max_num_msgs_mbx <= ICE_ASYNC_VF_MSG_THRESHOLD)
		return -EINVAL;

	 
	if (mbx_data->async_watermark_val < ICE_ASYNC_VF_MSG_THRESHOLD ||
	    mbx_data->async_watermark_val > mbx_data->max_num_msgs_mbx)
		return -EINVAL;

	new_state = ICE_MAL_VF_DETECT_STATE_INVALID;
	snap_buf = &snap->mbx_buf;

	switch (snap_buf->state) {
	case ICE_MAL_VF_DETECT_STATE_NEW_SNAPSHOT:
		 
		ice_mbx_reset_snapshot(snap);

		 
		snap_buf->num_pending_arq = mbx_data->num_pending_arq;
		snap_buf->num_msg_proc = mbx_data->num_msg_proc;
		snap_buf->max_num_msgs_mbx = mbx_data->max_num_msgs_mbx;

		 
		snap_buf->head = ICE_RQ_DATA_MASK(cq->rq.next_to_clean +
						  mbx_data->num_pending_arq);
		snap_buf->tail = ICE_RQ_DATA_MASK(cq->rq.next_to_clean - 1);
		snap_buf->num_iterations = snap_buf->tail;

		 
		if (snap_buf->num_pending_arq >=
		    mbx_data->async_watermark_val) {
			new_state = ICE_MAL_VF_DETECT_STATE_DETECT;
			status = ice_mbx_detect_malvf(hw, vf_info, &new_state, &is_malvf);
		} else {
			new_state = ICE_MAL_VF_DETECT_STATE_TRAVERSE;
			ice_mbx_traverse(hw, &new_state);
		}
		break;

	case ICE_MAL_VF_DETECT_STATE_TRAVERSE:
		new_state = ICE_MAL_VF_DETECT_STATE_TRAVERSE;
		ice_mbx_traverse(hw, &new_state);
		break;

	case ICE_MAL_VF_DETECT_STATE_DETECT:
		new_state = ICE_MAL_VF_DETECT_STATE_DETECT;
		status = ice_mbx_detect_malvf(hw, vf_info, &new_state, &is_malvf);
		break;

	default:
		new_state = ICE_MAL_VF_DETECT_STATE_INVALID;
		status = -EIO;
	}

	snap_buf->state = new_state;

	 
	if (is_malvf && !vf_info->malicious) {
		vf_info->malicious = 1;
		*report_malvf = true;
	}

	return status;
}

 
void ice_mbx_clear_malvf(struct ice_mbx_vf_info *vf_info)
{
	vf_info->malicious = 0;
	vf_info->msg_count = 0;
}

 
void ice_mbx_init_vf_info(struct ice_hw *hw, struct ice_mbx_vf_info *vf_info)
{
	struct ice_mbx_snapshot *snap = &hw->mbx_snapshot;

	ice_mbx_clear_malvf(vf_info);
	list_add(&vf_info->list_entry, &snap->mbx_vf);
}

 
void ice_mbx_init_snapshot(struct ice_hw *hw)
{
	struct ice_mbx_snapshot *snap = &hw->mbx_snapshot;

	INIT_LIST_HEAD(&snap->mbx_vf);
	ice_mbx_reset_snapshot(snap);
}

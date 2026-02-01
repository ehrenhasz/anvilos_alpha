
 

#include <net/devlink.h>
#include "ice_sched.h"

 
static int
ice_sched_add_root_node(struct ice_port_info *pi,
			struct ice_aqc_txsched_elem_data *info)
{
	struct ice_sched_node *root;
	struct ice_hw *hw;

	if (!pi)
		return -EINVAL;

	hw = pi->hw;

	root = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*root), GFP_KERNEL);
	if (!root)
		return -ENOMEM;

	 
	root->children = devm_kcalloc(ice_hw_to_dev(hw), hw->max_children[0],
				      sizeof(*root), GFP_KERNEL);
	if (!root->children) {
		devm_kfree(ice_hw_to_dev(hw), root);
		return -ENOMEM;
	}

	memcpy(&root->info, info, sizeof(*info));
	pi->root = root;
	return 0;
}

 
struct ice_sched_node *
ice_sched_find_node_by_teid(struct ice_sched_node *start_node, u32 teid)
{
	u16 i;

	 
	if (ICE_TXSCHED_GET_NODE_TEID(start_node) == teid)
		return start_node;

	 
	if (!start_node->num_children ||
	    start_node->tx_sched_layer >= ICE_AQC_TOPO_MAX_LEVEL_NUM ||
	    start_node->info.data.elem_type == ICE_AQC_ELEM_TYPE_LEAF)
		return NULL;

	 
	for (i = 0; i < start_node->num_children; i++)
		if (ICE_TXSCHED_GET_NODE_TEID(start_node->children[i]) == teid)
			return start_node->children[i];

	 
	for (i = 0; i < start_node->num_children; i++) {
		struct ice_sched_node *tmp;

		tmp = ice_sched_find_node_by_teid(start_node->children[i],
						  teid);
		if (tmp)
			return tmp;
	}

	return NULL;
}

 
static int
ice_aqc_send_sched_elem_cmd(struct ice_hw *hw, enum ice_adminq_opc cmd_opc,
			    u16 elems_req, void *buf, u16 buf_size,
			    u16 *elems_resp, struct ice_sq_cd *cd)
{
	struct ice_aqc_sched_elem_cmd *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.sched_elem_cmd;
	ice_fill_dflt_direct_cmd_desc(&desc, cmd_opc);
	cmd->num_elem_req = cpu_to_le16(elems_req);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && elems_resp)
		*elems_resp = le16_to_cpu(cmd->num_elem_resp);

	return status;
}

 
int
ice_aq_query_sched_elems(struct ice_hw *hw, u16 elems_req,
			 struct ice_aqc_txsched_elem_data *buf, u16 buf_size,
			 u16 *elems_ret, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_get_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_ret, cd);
}

 
int
ice_sched_add_node(struct ice_port_info *pi, u8 layer,
		   struct ice_aqc_txsched_elem_data *info,
		   struct ice_sched_node *prealloc_node)
{
	struct ice_aqc_txsched_elem_data elem;
	struct ice_sched_node *parent;
	struct ice_sched_node *node;
	struct ice_hw *hw;
	int status;

	if (!pi)
		return -EINVAL;

	hw = pi->hw;

	 
	parent = ice_sched_find_node_by_teid(pi->root,
					     le32_to_cpu(info->parent_teid));
	if (!parent) {
		ice_debug(hw, ICE_DBG_SCHED, "Parent Node not found for parent_teid=0x%x\n",
			  le32_to_cpu(info->parent_teid));
		return -EINVAL;
	}

	 
	status = ice_sched_query_elem(hw, le32_to_cpu(info->node_teid), &elem);
	if (status)
		return status;

	if (prealloc_node)
		node = prealloc_node;
	else
		node = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;
	if (hw->max_children[layer]) {
		 
		node->children = devm_kcalloc(ice_hw_to_dev(hw),
					      hw->max_children[layer],
					      sizeof(*node), GFP_KERNEL);
		if (!node->children) {
			devm_kfree(ice_hw_to_dev(hw), node);
			return -ENOMEM;
		}
	}

	node->in_use = true;
	node->parent = parent;
	node->tx_sched_layer = layer;
	parent->children[parent->num_children++] = node;
	node->info = elem;
	return 0;
}

 
static int
ice_aq_delete_sched_elems(struct ice_hw *hw, u16 grps_req,
			  struct ice_aqc_delete_elem *buf, u16 buf_size,
			  u16 *grps_del, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_delete_sched_elems,
					   grps_req, (void *)buf, buf_size,
					   grps_del, cd);
}

 
static int
ice_sched_remove_elems(struct ice_hw *hw, struct ice_sched_node *parent,
		       u16 num_nodes, u32 *node_teids)
{
	struct ice_aqc_delete_elem *buf;
	u16 i, num_groups_removed = 0;
	u16 buf_size;
	int status;

	buf_size = struct_size(buf, teid, num_nodes);
	buf = devm_kzalloc(ice_hw_to_dev(hw), buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf->hdr.parent_teid = parent->info.node_teid;
	buf->hdr.num_elems = cpu_to_le16(num_nodes);
	for (i = 0; i < num_nodes; i++)
		buf->teid[i] = cpu_to_le32(node_teids[i]);

	status = ice_aq_delete_sched_elems(hw, 1, buf, buf_size,
					   &num_groups_removed, NULL);
	if (status || num_groups_removed != 1)
		ice_debug(hw, ICE_DBG_SCHED, "remove node failed FW error %d\n",
			  hw->adminq.sq_last_status);

	devm_kfree(ice_hw_to_dev(hw), buf);
	return status;
}

 
static struct ice_sched_node *
ice_sched_get_first_node(struct ice_port_info *pi,
			 struct ice_sched_node *parent, u8 layer)
{
	return pi->sib_head[parent->tc_num][layer];
}

 
struct ice_sched_node *ice_sched_get_tc_node(struct ice_port_info *pi, u8 tc)
{
	u8 i;

	if (!pi || !pi->root)
		return NULL;
	for (i = 0; i < pi->root->num_children; i++)
		if (pi->root->children[i]->tc_num == tc)
			return pi->root->children[i];
	return NULL;
}

 
void ice_free_sched_node(struct ice_port_info *pi, struct ice_sched_node *node)
{
	struct ice_sched_node *parent;
	struct ice_hw *hw = pi->hw;
	u8 i, j;

	 
	while (node->num_children)
		ice_free_sched_node(pi, node->children[0]);

	 
	if (node->tx_sched_layer >= hw->sw_entry_point_layer &&
	    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_TC &&
	    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_ROOT_PORT &&
	    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_LEAF) {
		u32 teid = le32_to_cpu(node->info.node_teid);

		ice_sched_remove_elems(hw, node->parent, 1, &teid);
	}
	parent = node->parent;
	 
	if (parent) {
		struct ice_sched_node *p;

		 
		for (i = 0; i < parent->num_children; i++)
			if (parent->children[i] == node) {
				for (j = i + 1; j < parent->num_children; j++)
					parent->children[j - 1] =
						parent->children[j];
				parent->num_children--;
				break;
			}

		p = ice_sched_get_first_node(pi, node, node->tx_sched_layer);
		while (p) {
			if (p->sibling == node) {
				p->sibling = node->sibling;
				break;
			}
			p = p->sibling;
		}

		 
		if (pi->sib_head[node->tc_num][node->tx_sched_layer] == node)
			pi->sib_head[node->tc_num][node->tx_sched_layer] =
				node->sibling;
	}

	devm_kfree(ice_hw_to_dev(hw), node->children);
	kfree(node->name);
	xa_erase(&pi->sched_node_ids, node->id);
	devm_kfree(ice_hw_to_dev(hw), node);
}

 
static int
ice_aq_get_dflt_topo(struct ice_hw *hw, u8 lport,
		     struct ice_aqc_get_topo_elem *buf, u16 buf_size,
		     u8 *num_branches, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_topo *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.get_topo;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_dflt_topo);
	cmd->port_num = lport;
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && num_branches)
		*num_branches = cmd->num_branches;

	return status;
}

 
static int
ice_aq_add_sched_elems(struct ice_hw *hw, u16 grps_req,
		       struct ice_aqc_add_elem *buf, u16 buf_size,
		       u16 *grps_added, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_add_sched_elems,
					   grps_req, (void *)buf, buf_size,
					   grps_added, cd);
}

 
static int
ice_aq_cfg_sched_elems(struct ice_hw *hw, u16 elems_req,
		       struct ice_aqc_txsched_elem_data *buf, u16 buf_size,
		       u16 *elems_cfgd, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_cfg_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_cfgd, cd);
}

 
int
ice_aq_move_sched_elems(struct ice_hw *hw, u16 grps_req,
			struct ice_aqc_move_elem *buf, u16 buf_size,
			u16 *grps_movd, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_move_sched_elems,
					   grps_req, (void *)buf, buf_size,
					   grps_movd, cd);
}

 
static int
ice_aq_suspend_sched_elems(struct ice_hw *hw, u16 elems_req, __le32 *buf,
			   u16 buf_size, u16 *elems_ret, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_suspend_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_ret, cd);
}

 
static int
ice_aq_resume_sched_elems(struct ice_hw *hw, u16 elems_req, __le32 *buf,
			  u16 buf_size, u16 *elems_ret, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_resume_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_ret, cd);
}

 
static int
ice_aq_query_sched_res(struct ice_hw *hw, u16 buf_size,
		       struct ice_aqc_query_txsched_res_resp *buf,
		       struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_query_sched_res);
	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

 
int
ice_sched_suspend_resume_elems(struct ice_hw *hw, u8 num_nodes, u32 *node_teids,
			       bool suspend)
{
	u16 i, buf_size, num_elem_ret = 0;
	__le32 *buf;
	int status;

	buf_size = sizeof(*buf) * num_nodes;
	buf = devm_kzalloc(ice_hw_to_dev(hw), buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < num_nodes; i++)
		buf[i] = cpu_to_le32(node_teids[i]);

	if (suspend)
		status = ice_aq_suspend_sched_elems(hw, num_nodes, buf,
						    buf_size, &num_elem_ret,
						    NULL);
	else
		status = ice_aq_resume_sched_elems(hw, num_nodes, buf,
						   buf_size, &num_elem_ret,
						   NULL);
	if (status || num_elem_ret != num_nodes)
		ice_debug(hw, ICE_DBG_SCHED, "suspend/resume failed\n");

	devm_kfree(ice_hw_to_dev(hw), buf);
	return status;
}

 
static int
ice_alloc_lan_q_ctx(struct ice_hw *hw, u16 vsi_handle, u8 tc, u16 new_numqs)
{
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_q_ctx *q_ctx;
	u16 idx;

	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return -EINVAL;
	 
	if (!vsi_ctx->lan_q_ctx[tc]) {
		q_ctx = devm_kcalloc(ice_hw_to_dev(hw), new_numqs,
				     sizeof(*q_ctx), GFP_KERNEL);
		if (!q_ctx)
			return -ENOMEM;

		for (idx = 0; idx < new_numqs; idx++) {
			q_ctx[idx].q_handle = ICE_INVAL_Q_HANDLE;
			q_ctx[idx].q_teid = ICE_INVAL_TEID;
		}

		vsi_ctx->lan_q_ctx[tc] = q_ctx;
		vsi_ctx->num_lan_q_entries[tc] = new_numqs;
		return 0;
	}
	 
	if (new_numqs > vsi_ctx->num_lan_q_entries[tc]) {
		u16 prev_num = vsi_ctx->num_lan_q_entries[tc];

		q_ctx = devm_kcalloc(ice_hw_to_dev(hw), new_numqs,
				     sizeof(*q_ctx), GFP_KERNEL);
		if (!q_ctx)
			return -ENOMEM;

		memcpy(q_ctx, vsi_ctx->lan_q_ctx[tc],
		       prev_num * sizeof(*q_ctx));
		devm_kfree(ice_hw_to_dev(hw), vsi_ctx->lan_q_ctx[tc]);

		for (idx = prev_num; idx < new_numqs; idx++) {
			q_ctx[idx].q_handle = ICE_INVAL_Q_HANDLE;
			q_ctx[idx].q_teid = ICE_INVAL_TEID;
		}

		vsi_ctx->lan_q_ctx[tc] = q_ctx;
		vsi_ctx->num_lan_q_entries[tc] = new_numqs;
	}
	return 0;
}

 
static int
ice_alloc_rdma_q_ctx(struct ice_hw *hw, u16 vsi_handle, u8 tc, u16 new_numqs)
{
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_q_ctx *q_ctx;

	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return -EINVAL;
	 
	if (!vsi_ctx->rdma_q_ctx[tc]) {
		vsi_ctx->rdma_q_ctx[tc] = devm_kcalloc(ice_hw_to_dev(hw),
						       new_numqs,
						       sizeof(*q_ctx),
						       GFP_KERNEL);
		if (!vsi_ctx->rdma_q_ctx[tc])
			return -ENOMEM;
		vsi_ctx->num_rdma_q_entries[tc] = new_numqs;
		return 0;
	}
	 
	if (new_numqs > vsi_ctx->num_rdma_q_entries[tc]) {
		u16 prev_num = vsi_ctx->num_rdma_q_entries[tc];

		q_ctx = devm_kcalloc(ice_hw_to_dev(hw), new_numqs,
				     sizeof(*q_ctx), GFP_KERNEL);
		if (!q_ctx)
			return -ENOMEM;
		memcpy(q_ctx, vsi_ctx->rdma_q_ctx[tc],
		       prev_num * sizeof(*q_ctx));
		devm_kfree(ice_hw_to_dev(hw), vsi_ctx->rdma_q_ctx[tc]);
		vsi_ctx->rdma_q_ctx[tc] = q_ctx;
		vsi_ctx->num_rdma_q_entries[tc] = new_numqs;
	}
	return 0;
}

 
static int
ice_aq_rl_profile(struct ice_hw *hw, enum ice_adminq_opc opcode,
		  u16 num_profiles, struct ice_aqc_rl_profile_elem *buf,
		  u16 buf_size, u16 *num_processed, struct ice_sq_cd *cd)
{
	struct ice_aqc_rl_profile *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.rl_profile;

	ice_fill_dflt_direct_cmd_desc(&desc, opcode);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);
	cmd->num_profiles = cpu_to_le16(num_profiles);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && num_processed)
		*num_processed = le16_to_cpu(cmd->num_processed);
	return status;
}

 
static int
ice_aq_add_rl_profile(struct ice_hw *hw, u16 num_profiles,
		      struct ice_aqc_rl_profile_elem *buf, u16 buf_size,
		      u16 *num_profiles_added, struct ice_sq_cd *cd)
{
	return ice_aq_rl_profile(hw, ice_aqc_opc_add_rl_profiles, num_profiles,
				 buf, buf_size, num_profiles_added, cd);
}

 
static int
ice_aq_remove_rl_profile(struct ice_hw *hw, u16 num_profiles,
			 struct ice_aqc_rl_profile_elem *buf, u16 buf_size,
			 u16 *num_profiles_removed, struct ice_sq_cd *cd)
{
	return ice_aq_rl_profile(hw, ice_aqc_opc_remove_rl_profiles,
				 num_profiles, buf, buf_size,
				 num_profiles_removed, cd);
}

 
static int
ice_sched_del_rl_profile(struct ice_hw *hw,
			 struct ice_aqc_rl_profile_info *rl_info)
{
	struct ice_aqc_rl_profile_elem *buf;
	u16 num_profiles_removed;
	u16 num_profiles = 1;
	int status;

	if (rl_info->prof_id_ref != 0)
		return -EBUSY;

	 
	buf = &rl_info->profile;
	status = ice_aq_remove_rl_profile(hw, num_profiles, buf, sizeof(*buf),
					  &num_profiles_removed, NULL);
	if (status || num_profiles_removed != num_profiles)
		return -EIO;

	 
	list_del(&rl_info->list_entry);
	devm_kfree(ice_hw_to_dev(hw), rl_info);
	return status;
}

 
static void ice_sched_clear_rl_prof(struct ice_port_info *pi)
{
	u16 ln;

	for (ln = 0; ln < pi->hw->num_tx_sched_layers; ln++) {
		struct ice_aqc_rl_profile_info *rl_prof_elem;
		struct ice_aqc_rl_profile_info *rl_prof_tmp;

		list_for_each_entry_safe(rl_prof_elem, rl_prof_tmp,
					 &pi->rl_prof_list[ln], list_entry) {
			struct ice_hw *hw = pi->hw;
			int status;

			rl_prof_elem->prof_id_ref = 0;
			status = ice_sched_del_rl_profile(hw, rl_prof_elem);
			if (status) {
				ice_debug(hw, ICE_DBG_SCHED, "Remove rl profile failed\n");
				 
				list_del(&rl_prof_elem->list_entry);
				devm_kfree(ice_hw_to_dev(hw), rl_prof_elem);
			}
		}
	}
}

 
void ice_sched_clear_agg(struct ice_hw *hw)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_sched_agg_info *atmp;

	list_for_each_entry_safe(agg_info, atmp, &hw->agg_list, list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;
		struct ice_sched_agg_vsi_info *vtmp;

		list_for_each_entry_safe(agg_vsi_info, vtmp,
					 &agg_info->agg_vsi_list, list_entry) {
			list_del(&agg_vsi_info->list_entry);
			devm_kfree(ice_hw_to_dev(hw), agg_vsi_info);
		}
		list_del(&agg_info->list_entry);
		devm_kfree(ice_hw_to_dev(hw), agg_info);
	}
}

 
static void ice_sched_clear_tx_topo(struct ice_port_info *pi)
{
	if (!pi)
		return;
	 
	ice_sched_clear_rl_prof(pi);
	if (pi->root) {
		ice_free_sched_node(pi, pi->root);
		pi->root = NULL;
	}
}

 
void ice_sched_clear_port(struct ice_port_info *pi)
{
	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return;

	pi->port_state = ICE_SCHED_PORT_STATE_INIT;
	mutex_lock(&pi->sched_lock);
	ice_sched_clear_tx_topo(pi);
	mutex_unlock(&pi->sched_lock);
	mutex_destroy(&pi->sched_lock);
}

 
void ice_sched_cleanup_all(struct ice_hw *hw)
{
	if (!hw)
		return;

	devm_kfree(ice_hw_to_dev(hw), hw->layer_info);
	hw->layer_info = NULL;

	ice_sched_clear_port(hw->port_info);

	hw->num_tx_sched_layers = 0;
	hw->num_tx_sched_phys_layers = 0;
	hw->flattened_layers = 0;
	hw->max_cgds = 0;
}

 
int
ice_sched_add_elems(struct ice_port_info *pi, struct ice_sched_node *tc_node,
		    struct ice_sched_node *parent, u8 layer, u16 num_nodes,
		    u16 *num_nodes_added, u32 *first_node_teid,
		    struct ice_sched_node **prealloc_nodes)
{
	struct ice_sched_node *prev, *new_node;
	struct ice_aqc_add_elem *buf;
	u16 i, num_groups_added = 0;
	struct ice_hw *hw = pi->hw;
	size_t buf_size;
	int status = 0;
	u32 teid;

	buf_size = struct_size(buf, generic, num_nodes);
	buf = devm_kzalloc(ice_hw_to_dev(hw), buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	buf->hdr.parent_teid = parent->info.node_teid;
	buf->hdr.num_elems = cpu_to_le16(num_nodes);
	for (i = 0; i < num_nodes; i++) {
		buf->generic[i].parent_teid = parent->info.node_teid;
		buf->generic[i].data.elem_type = ICE_AQC_ELEM_TYPE_SE_GENERIC;
		buf->generic[i].data.valid_sections =
			ICE_AQC_ELEM_VALID_GENERIC | ICE_AQC_ELEM_VALID_CIR |
			ICE_AQC_ELEM_VALID_EIR;
		buf->generic[i].data.generic = 0;
		buf->generic[i].data.cir_bw.bw_profile_idx =
			cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->generic[i].data.cir_bw.bw_alloc =
			cpu_to_le16(ICE_SCHED_DFLT_BW_WT);
		buf->generic[i].data.eir_bw.bw_profile_idx =
			cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->generic[i].data.eir_bw.bw_alloc =
			cpu_to_le16(ICE_SCHED_DFLT_BW_WT);
	}

	status = ice_aq_add_sched_elems(hw, 1, buf, buf_size,
					&num_groups_added, NULL);
	if (status || num_groups_added != 1) {
		ice_debug(hw, ICE_DBG_SCHED, "add node failed FW Error %d\n",
			  hw->adminq.sq_last_status);
		devm_kfree(ice_hw_to_dev(hw), buf);
		return -EIO;
	}

	*num_nodes_added = num_nodes;
	 
	for (i = 0; i < num_nodes; i++) {
		if (prealloc_nodes)
			status = ice_sched_add_node(pi, layer, &buf->generic[i], prealloc_nodes[i]);
		else
			status = ice_sched_add_node(pi, layer, &buf->generic[i], NULL);

		if (status) {
			ice_debug(hw, ICE_DBG_SCHED, "add nodes in SW DB failed status =%d\n",
				  status);
			break;
		}

		teid = le32_to_cpu(buf->generic[i].node_teid);
		new_node = ice_sched_find_node_by_teid(parent, teid);
		if (!new_node) {
			ice_debug(hw, ICE_DBG_SCHED, "Node is missing for teid =%d\n", teid);
			break;
		}

		new_node->sibling = NULL;
		new_node->tc_num = tc_node->tc_num;
		new_node->tx_weight = ICE_SCHED_DFLT_BW_WT;
		new_node->tx_share = ICE_SCHED_DFLT_BW;
		new_node->tx_max = ICE_SCHED_DFLT_BW;
		new_node->name = kzalloc(SCHED_NODE_NAME_MAX_LEN, GFP_KERNEL);
		if (!new_node->name)
			return -ENOMEM;

		status = xa_alloc(&pi->sched_node_ids, &new_node->id, NULL, XA_LIMIT(0, UINT_MAX),
				  GFP_KERNEL);
		if (status) {
			ice_debug(hw, ICE_DBG_SCHED, "xa_alloc failed for sched node status =%d\n",
				  status);
			break;
		}

		snprintf(new_node->name, SCHED_NODE_NAME_MAX_LEN, "node_%u", new_node->id);

		 
		 
		prev = ice_sched_get_first_node(pi, tc_node, layer);
		if (prev && prev != new_node) {
			while (prev->sibling)
				prev = prev->sibling;
			prev->sibling = new_node;
		}

		 
		if (!pi->sib_head[tc_node->tc_num][layer])
			pi->sib_head[tc_node->tc_num][layer] = new_node;

		if (i == 0)
			*first_node_teid = teid;
	}

	devm_kfree(ice_hw_to_dev(hw), buf);
	return status;
}

 
static int
ice_sched_add_nodes_to_hw_layer(struct ice_port_info *pi,
				struct ice_sched_node *tc_node,
				struct ice_sched_node *parent, u8 layer,
				u16 num_nodes, u32 *first_node_teid,
				u16 *num_nodes_added)
{
	u16 max_child_nodes;

	*num_nodes_added = 0;

	if (!num_nodes)
		return 0;

	if (!parent || layer < pi->hw->sw_entry_point_layer)
		return -EINVAL;

	 
	max_child_nodes = pi->hw->max_children[parent->tx_sched_layer];

	 
	if ((parent->num_children + num_nodes) > max_child_nodes) {
		 
		if (parent == tc_node)
			return -EIO;
		return -ENOSPC;
	}

	return ice_sched_add_elems(pi, tc_node, parent, layer, num_nodes,
				   num_nodes_added, first_node_teid, NULL);
}

 
int
ice_sched_add_nodes_to_layer(struct ice_port_info *pi,
			     struct ice_sched_node *tc_node,
			     struct ice_sched_node *parent, u8 layer,
			     u16 num_nodes, u32 *first_node_teid,
			     u16 *num_nodes_added)
{
	u32 *first_teid_ptr = first_node_teid;
	u16 new_num_nodes = num_nodes;
	int status = 0;

	*num_nodes_added = 0;
	while (*num_nodes_added < num_nodes) {
		u16 max_child_nodes, num_added = 0;
		u32 temp;

		status = ice_sched_add_nodes_to_hw_layer(pi, tc_node, parent,
							 layer,	new_num_nodes,
							 first_teid_ptr,
							 &num_added);
		if (!status)
			*num_nodes_added += num_added;
		 
		if (*num_nodes_added > num_nodes) {
			ice_debug(pi->hw, ICE_DBG_SCHED, "added extra nodes %d %d\n", num_nodes,
				  *num_nodes_added);
			status = -EIO;
			break;
		}
		 
		if (!status && (*num_nodes_added == num_nodes))
			break;
		 
		if (status && status != -ENOSPC)
			break;
		 
		max_child_nodes = pi->hw->max_children[parent->tx_sched_layer];
		 
		if (parent->num_children < max_child_nodes) {
			new_num_nodes = max_child_nodes - parent->num_children;
		} else {
			 
			parent = parent->sibling;
			 
			if (num_added)
				first_teid_ptr = &temp;

			new_num_nodes = num_nodes - *num_nodes_added;
		}
	}
	return status;
}

 
static u8 ice_sched_get_qgrp_layer(struct ice_hw *hw)
{
	 
	return hw->num_tx_sched_layers - ICE_QGRP_LAYER_OFFSET;
}

 
u8 ice_sched_get_vsi_layer(struct ice_hw *hw)
{
	 
	 
	if (hw->num_tx_sched_layers > ICE_VSI_LAYER_OFFSET + 1) {
		u8 layer = hw->num_tx_sched_layers - ICE_VSI_LAYER_OFFSET;

		if (layer > hw->sw_entry_point_layer)
			return layer;
	}
	return hw->sw_entry_point_layer;
}

 
u8 ice_sched_get_agg_layer(struct ice_hw *hw)
{
	 
	 
	if (hw->num_tx_sched_layers > ICE_AGG_LAYER_OFFSET + 1) {
		u8 layer = hw->num_tx_sched_layers - ICE_AGG_LAYER_OFFSET;

		if (layer > hw->sw_entry_point_layer)
			return layer;
	}
	return hw->sw_entry_point_layer;
}

 
static void ice_rm_dflt_leaf_node(struct ice_port_info *pi)
{
	struct ice_sched_node *node;

	node = pi->root;
	while (node) {
		if (!node->num_children)
			break;
		node = node->children[0];
	}
	if (node && node->info.data.elem_type == ICE_AQC_ELEM_TYPE_LEAF) {
		u32 teid = le32_to_cpu(node->info.node_teid);
		int status;

		 
		status = ice_sched_remove_elems(pi->hw, node->parent, 1, &teid);
		if (!status)
			ice_free_sched_node(pi, node);
	}
}

 
static void ice_sched_rm_dflt_nodes(struct ice_port_info *pi)
{
	struct ice_sched_node *node;

	ice_rm_dflt_leaf_node(pi);

	 
	node = pi->root;
	while (node) {
		if (node->tx_sched_layer >= pi->hw->sw_entry_point_layer &&
		    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_TC &&
		    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_ROOT_PORT) {
			ice_free_sched_node(pi, node);
			break;
		}

		if (!node->num_children)
			break;
		node = node->children[0];
	}
}

 
int ice_sched_init_port(struct ice_port_info *pi)
{
	struct ice_aqc_get_topo_elem *buf;
	struct ice_hw *hw;
	u8 num_branches;
	u16 num_elems;
	int status;
	u8 i, j;

	if (!pi)
		return -EINVAL;
	hw = pi->hw;

	 
	buf = kzalloc(ICE_AQ_MAX_BUF_LEN, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	 
	status = ice_aq_get_dflt_topo(hw, pi->lport, buf, ICE_AQ_MAX_BUF_LEN,
				      &num_branches, NULL);
	if (status)
		goto err_init_port;

	 
	if (num_branches < 1 || num_branches > ICE_TXSCHED_MAX_BRANCHES) {
		ice_debug(hw, ICE_DBG_SCHED, "num_branches unexpected %d\n",
			  num_branches);
		status = -EINVAL;
		goto err_init_port;
	}

	 
	num_elems = le16_to_cpu(buf[0].hdr.num_elems);

	 
	if (num_elems < 1 || num_elems > ICE_AQC_TOPO_MAX_LEVEL_NUM) {
		ice_debug(hw, ICE_DBG_SCHED, "num_elems unexpected %d\n",
			  num_elems);
		status = -EINVAL;
		goto err_init_port;
	}

	 
	if (num_elems > 2 && buf[0].generic[num_elems - 1].data.elem_type ==
	    ICE_AQC_ELEM_TYPE_LEAF)
		pi->last_node_teid =
			le32_to_cpu(buf[0].generic[num_elems - 2].node_teid);
	else
		pi->last_node_teid =
			le32_to_cpu(buf[0].generic[num_elems - 1].node_teid);

	 
	status = ice_sched_add_root_node(pi, &buf[0].generic[0]);
	if (status)
		goto err_init_port;

	 
	for (i = 0; i < num_branches; i++) {
		num_elems = le16_to_cpu(buf[i].hdr.num_elems);

		 
		for (j = 1; j < num_elems; j++) {
			 
			if (buf[0].generic[j].data.elem_type ==
			    ICE_AQC_ELEM_TYPE_ENTRY_POINT)
				hw->sw_entry_point_layer = j;

			status = ice_sched_add_node(pi, j, &buf[i].generic[j], NULL);
			if (status)
				goto err_init_port;
		}
	}

	 
	if (pi->root)
		ice_sched_rm_dflt_nodes(pi);

	 
	pi->port_state = ICE_SCHED_PORT_STATE_READY;
	mutex_init(&pi->sched_lock);
	for (i = 0; i < ICE_AQC_TOPO_MAX_LEVEL_NUM; i++)
		INIT_LIST_HEAD(&pi->rl_prof_list[i]);

err_init_port:
	if (status && pi->root) {
		ice_free_sched_node(pi, pi->root);
		pi->root = NULL;
	}

	kfree(buf);
	return status;
}

 
int ice_sched_query_res_alloc(struct ice_hw *hw)
{
	struct ice_aqc_query_txsched_res_resp *buf;
	__le16 max_sibl;
	int status = 0;
	u16 i;

	if (hw->layer_info)
		return status;

	buf = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	status = ice_aq_query_sched_res(hw, sizeof(*buf), buf, NULL);
	if (status)
		goto sched_query_out;

	hw->num_tx_sched_layers = le16_to_cpu(buf->sched_props.logical_levels);
	hw->num_tx_sched_phys_layers =
		le16_to_cpu(buf->sched_props.phys_levels);
	hw->flattened_layers = buf->sched_props.flattening_bitmap;
	hw->max_cgds = buf->sched_props.max_pf_cgds;

	 
	for (i = 0; i < hw->num_tx_sched_layers - 1; i++) {
		max_sibl = buf->layer_props[i + 1].max_sibl_grp_sz;
		hw->max_children[i] = le16_to_cpu(max_sibl);
	}

	hw->layer_info = devm_kmemdup(ice_hw_to_dev(hw), buf->layer_props,
				      (hw->num_tx_sched_layers *
				       sizeof(*hw->layer_info)),
				      GFP_KERNEL);
	if (!hw->layer_info) {
		status = -ENOMEM;
		goto sched_query_out;
	}

sched_query_out:
	devm_kfree(ice_hw_to_dev(hw), buf);
	return status;
}

 
void ice_sched_get_psm_clk_freq(struct ice_hw *hw)
{
	u32 val, clk_src;

	val = rd32(hw, GLGEN_CLKSTAT_SRC);
	clk_src = (val & GLGEN_CLKSTAT_SRC_PSM_CLK_SRC_M) >>
		GLGEN_CLKSTAT_SRC_PSM_CLK_SRC_S;

#define PSM_CLK_SRC_367_MHZ 0x0
#define PSM_CLK_SRC_416_MHZ 0x1
#define PSM_CLK_SRC_446_MHZ 0x2
#define PSM_CLK_SRC_390_MHZ 0x3

	switch (clk_src) {
	case PSM_CLK_SRC_367_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_367MHZ_IN_HZ;
		break;
	case PSM_CLK_SRC_416_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_416MHZ_IN_HZ;
		break;
	case PSM_CLK_SRC_446_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_446MHZ_IN_HZ;
		break;
	case PSM_CLK_SRC_390_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_390MHZ_IN_HZ;
		break;
	default:
		ice_debug(hw, ICE_DBG_SCHED, "PSM clk_src unexpected %u\n",
			  clk_src);
		 
		hw->psm_clk_freq = ICE_PSM_CLK_446MHZ_IN_HZ;
	}
}

 
static bool
ice_sched_find_node_in_subtree(struct ice_hw *hw, struct ice_sched_node *base,
			       struct ice_sched_node *node)
{
	u8 i;

	for (i = 0; i < base->num_children; i++) {
		struct ice_sched_node *child = base->children[i];

		if (node == child)
			return true;

		if (child->tx_sched_layer > node->tx_sched_layer)
			return false;

		 
		if (ice_sched_find_node_in_subtree(hw, child, node))
			return true;
	}
	return false;
}

 
static struct ice_sched_node *
ice_sched_get_free_qgrp(struct ice_port_info *pi,
			struct ice_sched_node *vsi_node,
			struct ice_sched_node *qgrp_node, u8 owner)
{
	struct ice_sched_node *min_qgrp;
	u8 min_children;

	if (!qgrp_node)
		return qgrp_node;
	min_children = qgrp_node->num_children;
	if (!min_children)
		return qgrp_node;
	min_qgrp = qgrp_node;
	 
	while (qgrp_node) {
		 
		if (ice_sched_find_node_in_subtree(pi->hw, vsi_node, qgrp_node))
			if (qgrp_node->num_children < min_children &&
			    qgrp_node->owner == owner) {
				 
				min_qgrp = qgrp_node;
				min_children = min_qgrp->num_children;
				 
				if (!min_children)
					break;
			}
		qgrp_node = qgrp_node->sibling;
	}
	return min_qgrp;
}

 
struct ice_sched_node *
ice_sched_get_free_qparent(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			   u8 owner)
{
	struct ice_sched_node *vsi_node, *qgrp_node;
	struct ice_vsi_ctx *vsi_ctx;
	u16 max_children;
	u8 qgrp_layer;

	qgrp_layer = ice_sched_get_qgrp_layer(pi->hw);
	max_children = pi->hw->max_children[qgrp_layer];

	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return NULL;
	vsi_node = vsi_ctx->sched.vsi_node[tc];
	 
	if (!vsi_node)
		return NULL;

	 
	qgrp_node = ice_sched_get_first_node(pi, vsi_node, qgrp_layer);
	while (qgrp_node) {
		 
		if (ice_sched_find_node_in_subtree(pi->hw, vsi_node, qgrp_node))
			if (qgrp_node->num_children < max_children &&
			    qgrp_node->owner == owner)
				break;
		qgrp_node = qgrp_node->sibling;
	}

	 
	return ice_sched_get_free_qgrp(pi, vsi_node, qgrp_node, owner);
}

 
static struct ice_sched_node *
ice_sched_get_vsi_node(struct ice_port_info *pi, struct ice_sched_node *tc_node,
		       u16 vsi_handle)
{
	struct ice_sched_node *node;
	u8 vsi_layer;

	vsi_layer = ice_sched_get_vsi_layer(pi->hw);
	node = ice_sched_get_first_node(pi, tc_node, vsi_layer);

	 
	while (node) {
		if (node->vsi_handle == vsi_handle)
			return node;
		node = node->sibling;
	}

	return node;
}

 
struct ice_sched_node *
ice_sched_get_agg_node(struct ice_port_info *pi, struct ice_sched_node *tc_node,
		       u32 agg_id)
{
	struct ice_sched_node *node;
	struct ice_hw *hw = pi->hw;
	u8 agg_layer;

	if (!hw)
		return NULL;
	agg_layer = ice_sched_get_agg_layer(hw);
	node = ice_sched_get_first_node(pi, tc_node, agg_layer);

	 
	while (node) {
		if (node->agg_id == agg_id)
			return node;
		node = node->sibling;
	}

	return node;
}

 
static void
ice_sched_calc_vsi_child_nodes(struct ice_hw *hw, u16 num_qs, u16 *num_nodes)
{
	u16 num = num_qs;
	u8 i, qgl, vsil;

	qgl = ice_sched_get_qgrp_layer(hw);
	vsil = ice_sched_get_vsi_layer(hw);

	 
	for (i = qgl; i > vsil; i--) {
		 
		num = DIV_ROUND_UP(num, hw->max_children[i]);

		 
		num_nodes[i] = num ? num : 1;
	}
}

 
static int
ice_sched_add_vsi_child_nodes(struct ice_port_info *pi, u16 vsi_handle,
			      struct ice_sched_node *tc_node, u16 *num_nodes,
			      u8 owner)
{
	struct ice_sched_node *parent, *node;
	struct ice_hw *hw = pi->hw;
	u32 first_node_teid;
	u16 num_added = 0;
	u8 i, qgl, vsil;

	qgl = ice_sched_get_qgrp_layer(hw);
	vsil = ice_sched_get_vsi_layer(hw);
	parent = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	for (i = vsil + 1; i <= qgl; i++) {
		int status;

		if (!parent)
			return -EIO;

		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent, i,
						      num_nodes[i],
						      &first_node_teid,
						      &num_added);
		if (status || num_nodes[i] != num_added)
			return -EIO;

		 
		if (num_added) {
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
			node = parent;
			while (node) {
				node->owner = owner;
				node = node->sibling;
			}
		} else {
			parent = parent->children[0];
		}
	}

	return 0;
}

 
static void
ice_sched_calc_vsi_support_nodes(struct ice_port_info *pi,
				 struct ice_sched_node *tc_node, u16 *num_nodes)
{
	struct ice_sched_node *node;
	u8 vsil;
	int i;

	vsil = ice_sched_get_vsi_layer(pi->hw);
	for (i = vsil; i >= pi->hw->sw_entry_point_layer; i--)
		 
		if (!tc_node->num_children || i == vsil) {
			num_nodes[i]++;
		} else {
			 
			node = ice_sched_get_first_node(pi, tc_node, (u8)i);
			 
			while (node) {
				if (node->num_children < pi->hw->max_children[i])
					break;
				node = node->sibling;
			}

			 
			if (node)
				break;
			 
			num_nodes[i]++;
		}
}

 
static int
ice_sched_add_vsi_support_nodes(struct ice_port_info *pi, u16 vsi_handle,
				struct ice_sched_node *tc_node, u16 *num_nodes)
{
	struct ice_sched_node *parent = tc_node;
	u32 first_node_teid;
	u16 num_added = 0;
	u8 i, vsil;

	if (!pi)
		return -EINVAL;

	vsil = ice_sched_get_vsi_layer(pi->hw);
	for (i = pi->hw->sw_entry_point_layer; i <= vsil; i++) {
		int status;

		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent,
						      i, num_nodes[i],
						      &first_node_teid,
						      &num_added);
		if (status || num_nodes[i] != num_added)
			return -EIO;

		 
		if (num_added)
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
		else
			parent = parent->children[0];

		if (!parent)
			return -EIO;

		if (i == vsil)
			parent->vsi_handle = vsi_handle;
	}

	return 0;
}

 
static int
ice_sched_add_vsi_to_topo(struct ice_port_info *pi, u16 vsi_handle, u8 tc)
{
	u16 num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	struct ice_sched_node *tc_node;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return -EINVAL;

	 
	ice_sched_calc_vsi_support_nodes(pi, tc_node, num_nodes);

	 
	return ice_sched_add_vsi_support_nodes(pi, vsi_handle, tc_node,
					       num_nodes);
}

 
static int
ice_sched_update_vsi_child_nodes(struct ice_port_info *pi, u16 vsi_handle,
				 u8 tc, u16 new_numqs, u8 owner)
{
	u16 new_num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	struct ice_sched_node *vsi_node;
	struct ice_sched_node *tc_node;
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_hw *hw = pi->hw;
	u16 prev_numqs;
	int status = 0;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return -EIO;

	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	if (!vsi_node)
		return -EIO;

	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return -EINVAL;

	if (owner == ICE_SCHED_NODE_OWNER_LAN)
		prev_numqs = vsi_ctx->sched.max_lanq[tc];
	else
		prev_numqs = vsi_ctx->sched.max_rdmaq[tc];
	 
	if (new_numqs <= prev_numqs)
		return status;
	if (owner == ICE_SCHED_NODE_OWNER_LAN) {
		status = ice_alloc_lan_q_ctx(hw, vsi_handle, tc, new_numqs);
		if (status)
			return status;
	} else {
		status = ice_alloc_rdma_q_ctx(hw, vsi_handle, tc, new_numqs);
		if (status)
			return status;
	}

	if (new_numqs)
		ice_sched_calc_vsi_child_nodes(hw, new_numqs, new_num_nodes);
	 
	status = ice_sched_add_vsi_child_nodes(pi, vsi_handle, tc_node,
					       new_num_nodes, owner);
	if (status)
		return status;
	if (owner == ICE_SCHED_NODE_OWNER_LAN)
		vsi_ctx->sched.max_lanq[tc] = new_numqs;
	else
		vsi_ctx->sched.max_rdmaq[tc] = new_numqs;

	return 0;
}

 
int
ice_sched_cfg_vsi(struct ice_port_info *pi, u16 vsi_handle, u8 tc, u16 maxqs,
		  u8 owner, bool enable)
{
	struct ice_sched_node *vsi_node, *tc_node;
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_hw *hw = pi->hw;
	int status = 0;

	ice_debug(pi->hw, ICE_DBG_SCHED, "add/config VSI %d\n", vsi_handle);
	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return -EINVAL;
	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return -EINVAL;
	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);

	 
	if (!enable) {
		if (vsi_node && vsi_node->in_use) {
			u32 teid = le32_to_cpu(vsi_node->info.node_teid);

			status = ice_sched_suspend_resume_elems(hw, 1, &teid,
								true);
			if (!status)
				vsi_node->in_use = false;
		}
		return status;
	}

	 
	if (!vsi_node) {
		status = ice_sched_add_vsi_to_topo(pi, vsi_handle, tc);
		if (status)
			return status;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			return -EIO;

		vsi_ctx->sched.vsi_node[tc] = vsi_node;
		vsi_node->in_use = true;
		 
		vsi_ctx->sched.max_lanq[tc] = 0;
		vsi_ctx->sched.max_rdmaq[tc] = 0;
	}

	 
	status = ice_sched_update_vsi_child_nodes(pi, vsi_handle, tc, maxqs,
						  owner);
	if (status)
		return status;

	 
	if (!vsi_node->in_use) {
		u32 teid = le32_to_cpu(vsi_node->info.node_teid);

		status = ice_sched_suspend_resume_elems(hw, 1, &teid, false);
		if (!status)
			vsi_node->in_use = true;
	}

	return status;
}

 
static void ice_sched_rm_agg_vsi_info(struct ice_port_info *pi, u16 vsi_handle)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_sched_agg_info *atmp;

	list_for_each_entry_safe(agg_info, atmp, &pi->hw->agg_list,
				 list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;
		struct ice_sched_agg_vsi_info *vtmp;

		list_for_each_entry_safe(agg_vsi_info, vtmp,
					 &agg_info->agg_vsi_list, list_entry)
			if (agg_vsi_info->vsi_handle == vsi_handle) {
				list_del(&agg_vsi_info->list_entry);
				devm_kfree(ice_hw_to_dev(pi->hw),
					   agg_vsi_info);
				return;
			}
	}
}

 
static bool ice_sched_is_leaf_node_present(struct ice_sched_node *node)
{
	u8 i;

	for (i = 0; i < node->num_children; i++)
		if (ice_sched_is_leaf_node_present(node->children[i]))
			return true;
	 
	return (node->info.data.elem_type == ICE_AQC_ELEM_TYPE_LEAF);
}

 
static int
ice_sched_rm_vsi_cfg(struct ice_port_info *pi, u16 vsi_handle, u8 owner)
{
	struct ice_vsi_ctx *vsi_ctx;
	int status = -EINVAL;
	u8 i;

	ice_debug(pi->hw, ICE_DBG_SCHED, "removing VSI %d\n", vsi_handle);
	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return status;
	mutex_lock(&pi->sched_lock);
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		goto exit_sched_rm_vsi_cfg;

	ice_for_each_traffic_class(i) {
		struct ice_sched_node *vsi_node, *tc_node;
		u8 j = 0;

		tc_node = ice_sched_get_tc_node(pi, i);
		if (!tc_node)
			continue;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			continue;

		if (ice_sched_is_leaf_node_present(vsi_node)) {
			ice_debug(pi->hw, ICE_DBG_SCHED, "VSI has leaf nodes in TC %d\n", i);
			status = -EBUSY;
			goto exit_sched_rm_vsi_cfg;
		}
		while (j < vsi_node->num_children) {
			if (vsi_node->children[j]->owner == owner) {
				ice_free_sched_node(pi, vsi_node->children[j]);

				 
				j = 0;
			} else {
				j++;
			}
		}
		 
		if (!vsi_node->num_children) {
			ice_free_sched_node(pi, vsi_node);
			vsi_ctx->sched.vsi_node[i] = NULL;

			 
			ice_sched_rm_agg_vsi_info(pi, vsi_handle);
		}
		if (owner == ICE_SCHED_NODE_OWNER_LAN)
			vsi_ctx->sched.max_lanq[i] = 0;
		else
			vsi_ctx->sched.max_rdmaq[i] = 0;
	}
	status = 0;

exit_sched_rm_vsi_cfg:
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
int ice_rm_vsi_lan_cfg(struct ice_port_info *pi, u16 vsi_handle)
{
	return ice_sched_rm_vsi_cfg(pi, vsi_handle, ICE_SCHED_NODE_OWNER_LAN);
}

 
int ice_rm_vsi_rdma_cfg(struct ice_port_info *pi, u16 vsi_handle)
{
	return ice_sched_rm_vsi_cfg(pi, vsi_handle, ICE_SCHED_NODE_OWNER_RDMA);
}

 
static struct ice_sched_agg_info *
ice_get_agg_info(struct ice_hw *hw, u32 agg_id)
{
	struct ice_sched_agg_info *agg_info;

	list_for_each_entry(agg_info, &hw->agg_list, list_entry)
		if (agg_info->agg_id == agg_id)
			return agg_info;

	return NULL;
}

 
struct ice_sched_node *
ice_sched_get_free_vsi_parent(struct ice_hw *hw, struct ice_sched_node *node,
			      u16 *num_nodes)
{
	u8 l = node->tx_sched_layer;
	u8 vsil, i;

	vsil = ice_sched_get_vsi_layer(hw);

	 
	if (l == vsil - 1)
		return (node->num_children < hw->max_children[l]) ? node : NULL;

	 
	if (node->num_children < hw->max_children[l])
		num_nodes[l] = 0;
	 

	for (i = 0; i < node->num_children; i++) {
		struct ice_sched_node *parent;

		parent = ice_sched_get_free_vsi_parent(hw, node->children[i],
						       num_nodes);
		if (parent)
			return parent;
	}

	return NULL;
}

 
void
ice_sched_update_parent(struct ice_sched_node *new_parent,
			struct ice_sched_node *node)
{
	struct ice_sched_node *old_parent;
	u8 i, j;

	old_parent = node->parent;

	 
	for (i = 0; i < old_parent->num_children; i++)
		if (old_parent->children[i] == node) {
			for (j = i + 1; j < old_parent->num_children; j++)
				old_parent->children[j - 1] =
					old_parent->children[j];
			old_parent->num_children--;
			break;
		}

	 
	new_parent->children[new_parent->num_children++] = node;
	node->parent = new_parent;
	node->info.parent_teid = new_parent->info.node_teid;
}

 
int
ice_sched_move_nodes(struct ice_port_info *pi, struct ice_sched_node *parent,
		     u16 num_items, u32 *list)
{
	struct ice_aqc_move_elem *buf;
	struct ice_sched_node *node;
	u16 i, grps_movd = 0;
	struct ice_hw *hw;
	int status = 0;
	u16 buf_len;

	hw = pi->hw;

	if (!parent || !num_items)
		return -EINVAL;

	 
	if (parent->num_children + num_items >
	    hw->max_children[parent->tx_sched_layer])
		return -ENOSPC;

	buf_len = struct_size(buf, teid, 1);
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	for (i = 0; i < num_items; i++) {
		node = ice_sched_find_node_by_teid(pi->root, list[i]);
		if (!node) {
			status = -EINVAL;
			goto move_err_exit;
		}

		buf->hdr.src_parent_teid = node->info.parent_teid;
		buf->hdr.dest_parent_teid = parent->info.node_teid;
		buf->teid[0] = node->info.node_teid;
		buf->hdr.num_elems = cpu_to_le16(1);
		status = ice_aq_move_sched_elems(hw, 1, buf, buf_len,
						 &grps_movd, NULL);
		if (status && grps_movd != 1) {
			status = -EIO;
			goto move_err_exit;
		}

		 
		ice_sched_update_parent(parent, node);
	}

move_err_exit:
	kfree(buf);
	return status;
}

 
static int
ice_sched_move_vsi_to_agg(struct ice_port_info *pi, u16 vsi_handle, u32 agg_id,
			  u8 tc)
{
	struct ice_sched_node *vsi_node, *agg_node, *tc_node, *parent;
	u16 num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	u32 first_node_teid, vsi_teid;
	u16 num_nodes_added;
	u8 aggl, vsil, i;
	int status;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return -EIO;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	if (!agg_node)
		return -ENOENT;

	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	if (!vsi_node)
		return -ENOENT;

	 
	if (ice_sched_find_node_in_subtree(pi->hw, agg_node, vsi_node))
		return 0;

	aggl = ice_sched_get_agg_layer(pi->hw);
	vsil = ice_sched_get_vsi_layer(pi->hw);

	 
	for (i = aggl + 1; i < vsil; i++)
		num_nodes[i] = 1;

	 
	for (i = 0; i < agg_node->num_children; i++) {
		parent = ice_sched_get_free_vsi_parent(pi->hw,
						       agg_node->children[i],
						       num_nodes);
		if (parent)
			goto move_nodes;
	}

	 
	parent = agg_node;
	for (i = aggl + 1; i < vsil; i++) {
		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent, i,
						      num_nodes[i],
						      &first_node_teid,
						      &num_nodes_added);
		if (status || num_nodes[i] != num_nodes_added)
			return -EIO;

		 
		if (num_nodes_added)
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
		else
			parent = parent->children[0];

		if (!parent)
			return -EIO;
	}

move_nodes:
	vsi_teid = le32_to_cpu(vsi_node->info.node_teid);
	return ice_sched_move_nodes(pi, parent, 1, &vsi_teid);
}

 
static int
ice_move_all_vsi_to_dflt_agg(struct ice_port_info *pi,
			     struct ice_sched_agg_info *agg_info, u8 tc,
			     bool rm_vsi_info)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_sched_agg_vsi_info *tmp;
	int status = 0;

	list_for_each_entry_safe(agg_vsi_info, tmp, &agg_info->agg_vsi_list,
				 list_entry) {
		u16 vsi_handle = agg_vsi_info->vsi_handle;

		 
		if (!ice_is_tc_ena(agg_vsi_info->tc_bitmap[0], tc))
			continue;

		status = ice_sched_move_vsi_to_agg(pi, vsi_handle,
						   ICE_DFLT_AGG_ID, tc);
		if (status)
			break;

		clear_bit(tc, agg_vsi_info->tc_bitmap);
		if (rm_vsi_info && !agg_vsi_info->tc_bitmap[0]) {
			list_del(&agg_vsi_info->list_entry);
			devm_kfree(ice_hw_to_dev(pi->hw), agg_vsi_info);
		}
	}

	return status;
}

 
static bool
ice_sched_is_agg_inuse(struct ice_port_info *pi, struct ice_sched_node *node)
{
	u8 vsil, i;

	vsil = ice_sched_get_vsi_layer(pi->hw);
	if (node->tx_sched_layer < vsil - 1) {
		for (i = 0; i < node->num_children; i++)
			if (ice_sched_is_agg_inuse(pi, node->children[i]))
				return true;
		return false;
	} else {
		return node->num_children ? true : false;
	}
}

 
static int
ice_sched_rm_agg_cfg(struct ice_port_info *pi, u32 agg_id, u8 tc)
{
	struct ice_sched_node *tc_node, *agg_node;
	struct ice_hw *hw = pi->hw;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return -EIO;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	if (!agg_node)
		return -ENOENT;

	 
	if (ice_sched_is_agg_inuse(pi, agg_node))
		return -EBUSY;

	 
	while (agg_node->tx_sched_layer > hw->sw_entry_point_layer) {
		struct ice_sched_node *parent = agg_node->parent;

		if (!parent)
			return -EIO;

		if (parent->num_children > 1)
			break;

		agg_node = parent;
	}

	ice_free_sched_node(pi, agg_node);
	return 0;
}

 
static int
ice_rm_agg_cfg_tc(struct ice_port_info *pi, struct ice_sched_agg_info *agg_info,
		  u8 tc, bool rm_vsi_info)
{
	int status = 0;

	 
	if (!ice_is_tc_ena(agg_info->tc_bitmap[0], tc))
		goto exit_rm_agg_cfg_tc;

	status = ice_move_all_vsi_to_dflt_agg(pi, agg_info, tc, rm_vsi_info);
	if (status)
		goto exit_rm_agg_cfg_tc;

	 
	status = ice_sched_rm_agg_cfg(pi, agg_info->agg_id, tc);
	if (status)
		goto exit_rm_agg_cfg_tc;

	clear_bit(tc, agg_info->tc_bitmap);
exit_rm_agg_cfg_tc:
	return status;
}

 
static int
ice_save_agg_tc_bitmap(struct ice_port_info *pi, u32 agg_id,
		       unsigned long *tc_bitmap)
{
	struct ice_sched_agg_info *agg_info;

	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info)
		return -EINVAL;
	bitmap_copy(agg_info->replay_tc_bitmap, tc_bitmap,
		    ICE_MAX_TRAFFIC_CLASS);
	return 0;
}

 
static int
ice_sched_add_agg_cfg(struct ice_port_info *pi, u32 agg_id, u8 tc)
{
	struct ice_sched_node *parent, *agg_node, *tc_node;
	u16 num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	struct ice_hw *hw = pi->hw;
	u32 first_node_teid;
	u16 num_nodes_added;
	int status = 0;
	u8 i, aggl;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return -EIO;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	 
	if (agg_node)
		return status;

	aggl = ice_sched_get_agg_layer(hw);

	 
	num_nodes[aggl] = 1;

	 
	for (i = hw->sw_entry_point_layer; i < aggl; i++) {
		parent = ice_sched_get_first_node(pi, tc_node, i);

		 
		while (parent) {
			if (parent->num_children < hw->max_children[i])
				break;
			parent = parent->sibling;
		}

		 
		if (!parent)
			num_nodes[i]++;
	}

	 
	parent = tc_node;
	for (i = hw->sw_entry_point_layer; i <= aggl; i++) {
		if (!parent)
			return -EIO;

		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent, i,
						      num_nodes[i],
						      &first_node_teid,
						      &num_nodes_added);
		if (status || num_nodes[i] != num_nodes_added)
			return -EIO;

		 
		if (num_nodes_added) {
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
			 
			if (parent && i == aggl)
				parent->agg_id = agg_id;
		} else {
			parent = parent->children[0];
		}
	}

	return 0;
}

 
static int
ice_sched_cfg_agg(struct ice_port_info *pi, u32 agg_id,
		  enum ice_agg_type agg_type, unsigned long *tc_bitmap)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u8 tc;

	agg_info = ice_get_agg_info(hw, agg_id);
	if (!agg_info) {
		 
		agg_info = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*agg_info),
					GFP_KERNEL);
		if (!agg_info)
			return -ENOMEM;

		agg_info->agg_id = agg_id;
		agg_info->agg_type = agg_type;
		agg_info->tc_bitmap[0] = 0;

		 
		INIT_LIST_HEAD(&agg_info->agg_vsi_list);

		 
		list_add(&agg_info->list_entry, &hw->agg_list);
	}
	 
	ice_for_each_traffic_class(tc) {
		if (!ice_is_tc_ena(*tc_bitmap, tc)) {
			 
			status = ice_rm_agg_cfg_tc(pi, agg_info, tc, false);
			if (status)
				break;
			continue;
		}

		 
		if (ice_is_tc_ena(agg_info->tc_bitmap[0], tc))
			continue;

		 
		status = ice_sched_add_agg_cfg(pi, agg_id, tc);
		if (status)
			break;

		 
		set_bit(tc, agg_info->tc_bitmap);
	}

	return status;
}

 
int
ice_cfg_agg(struct ice_port_info *pi, u32 agg_id, enum ice_agg_type agg_type,
	    u8 tc_bitmap)
{
	unsigned long bitmap = tc_bitmap;
	int status;

	mutex_lock(&pi->sched_lock);
	status = ice_sched_cfg_agg(pi, agg_id, agg_type, &bitmap);
	if (!status)
		status = ice_save_agg_tc_bitmap(pi, agg_id, &bitmap);
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
static struct ice_sched_agg_vsi_info *
ice_get_agg_vsi_info(struct ice_sched_agg_info *agg_info, u16 vsi_handle)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;

	list_for_each_entry(agg_vsi_info, &agg_info->agg_vsi_list, list_entry)
		if (agg_vsi_info->vsi_handle == vsi_handle)
			return agg_vsi_info;

	return NULL;
}

 
static struct ice_sched_agg_info *
ice_get_vsi_agg_info(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_sched_agg_info *agg_info;

	list_for_each_entry(agg_info, &hw->agg_list, list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;

		agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
		if (agg_vsi_info)
			return agg_info;
	}
	return NULL;
}

 
static int
ice_save_agg_vsi_tc_bitmap(struct ice_port_info *pi, u32 agg_id, u16 vsi_handle,
			   unsigned long *tc_bitmap)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_sched_agg_info *agg_info;

	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info)
		return -EINVAL;
	 
	agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
	if (!agg_vsi_info)
		return -EINVAL;
	bitmap_copy(agg_vsi_info->replay_tc_bitmap, tc_bitmap,
		    ICE_MAX_TRAFFIC_CLASS);
	return 0;
}

 
static int
ice_sched_assoc_vsi_to_agg(struct ice_port_info *pi, u32 agg_id,
			   u16 vsi_handle, unsigned long *tc_bitmap)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info, *iter, *old_agg_vsi_info = NULL;
	struct ice_sched_agg_info *agg_info, *old_agg_info;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u8 tc;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return -EINVAL;
	agg_info = ice_get_agg_info(hw, agg_id);
	if (!agg_info)
		return -EINVAL;
	 
	old_agg_info = ice_get_vsi_agg_info(hw, vsi_handle);
	if (old_agg_info && old_agg_info != agg_info) {
		struct ice_sched_agg_vsi_info *vtmp;

		list_for_each_entry_safe(iter, vtmp,
					 &old_agg_info->agg_vsi_list,
					 list_entry)
			if (iter->vsi_handle == vsi_handle) {
				old_agg_vsi_info = iter;
				break;
			}
	}

	 
	agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
	if (!agg_vsi_info) {
		 
		agg_vsi_info = devm_kzalloc(ice_hw_to_dev(hw),
					    sizeof(*agg_vsi_info), GFP_KERNEL);
		if (!agg_vsi_info)
			return -EINVAL;

		 
		agg_vsi_info->vsi_handle = vsi_handle;
		list_add(&agg_vsi_info->list_entry, &agg_info->agg_vsi_list);
	}
	 
	ice_for_each_traffic_class(tc) {
		if (!ice_is_tc_ena(*tc_bitmap, tc))
			continue;

		 
		status = ice_sched_move_vsi_to_agg(pi, vsi_handle, agg_id, tc);
		if (status)
			break;

		set_bit(tc, agg_vsi_info->tc_bitmap);
		if (old_agg_vsi_info)
			clear_bit(tc, old_agg_vsi_info->tc_bitmap);
	}
	if (old_agg_vsi_info && !old_agg_vsi_info->tc_bitmap[0]) {
		list_del(&old_agg_vsi_info->list_entry);
		devm_kfree(ice_hw_to_dev(pi->hw), old_agg_vsi_info);
	}
	return status;
}

 
static void ice_sched_rm_unused_rl_prof(struct ice_port_info *pi)
{
	u16 ln;

	for (ln = 0; ln < pi->hw->num_tx_sched_layers; ln++) {
		struct ice_aqc_rl_profile_info *rl_prof_elem;
		struct ice_aqc_rl_profile_info *rl_prof_tmp;

		list_for_each_entry_safe(rl_prof_elem, rl_prof_tmp,
					 &pi->rl_prof_list[ln], list_entry) {
			if (!ice_sched_del_rl_profile(pi->hw, rl_prof_elem))
				ice_debug(pi->hw, ICE_DBG_SCHED, "Removed rl profile\n");
		}
	}
}

 
static int
ice_sched_update_elem(struct ice_hw *hw, struct ice_sched_node *node,
		      struct ice_aqc_txsched_elem_data *info)
{
	struct ice_aqc_txsched_elem_data buf;
	u16 elem_cfgd = 0;
	u16 num_elems = 1;
	int status;

	buf = *info;
	 
	buf.parent_teid = 0;
	 
	buf.data.elem_type = 0;
	 
	buf.data.flags = 0;

	 
	 
	status = ice_aq_cfg_sched_elems(hw, num_elems, &buf, sizeof(buf),
					&elem_cfgd, NULL);
	if (status || elem_cfgd != num_elems) {
		ice_debug(hw, ICE_DBG_SCHED, "Config sched elem error\n");
		return -EIO;
	}

	 
	 
	 
	node->info.data = info->data;
	return status;
}

 
static int
ice_sched_cfg_node_bw_alloc(struct ice_hw *hw, struct ice_sched_node *node,
			    enum ice_rl_type rl_type, u16 bw_alloc)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;
	if (rl_type == ICE_MIN_BW) {
		data->valid_sections |= ICE_AQC_ELEM_VALID_CIR;
		data->cir_bw.bw_alloc = cpu_to_le16(bw_alloc);
	} else if (rl_type == ICE_MAX_BW) {
		data->valid_sections |= ICE_AQC_ELEM_VALID_EIR;
		data->eir_bw.bw_alloc = cpu_to_le16(bw_alloc);
	} else {
		return -EINVAL;
	}

	 
	return ice_sched_update_elem(hw, node, &buf);
}

 
int
ice_move_vsi_to_agg(struct ice_port_info *pi, u32 agg_id, u16 vsi_handle,
		    u8 tc_bitmap)
{
	unsigned long bitmap = tc_bitmap;
	int status;

	mutex_lock(&pi->sched_lock);
	status = ice_sched_assoc_vsi_to_agg(pi, agg_id, vsi_handle,
					    (unsigned long *)&bitmap);
	if (!status)
		status = ice_save_agg_vsi_tc_bitmap(pi, agg_id, vsi_handle,
						    (unsigned long *)&bitmap);
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
static void ice_set_clear_cir_bw(struct ice_bw_type_info *bw_t_info, u32 bw)
{
	if (bw == ICE_SCHED_DFLT_BW) {
		clear_bit(ICE_BW_TYPE_CIR, bw_t_info->bw_t_bitmap);
		bw_t_info->cir_bw.bw = 0;
	} else {
		 
		set_bit(ICE_BW_TYPE_CIR, bw_t_info->bw_t_bitmap);
		bw_t_info->cir_bw.bw = bw;
	}
}

 
static void ice_set_clear_eir_bw(struct ice_bw_type_info *bw_t_info, u32 bw)
{
	if (bw == ICE_SCHED_DFLT_BW) {
		clear_bit(ICE_BW_TYPE_EIR, bw_t_info->bw_t_bitmap);
		bw_t_info->eir_bw.bw = 0;
	} else {
		 
		clear_bit(ICE_BW_TYPE_SHARED, bw_t_info->bw_t_bitmap);
		bw_t_info->shared_bw = 0;
		 
		set_bit(ICE_BW_TYPE_EIR, bw_t_info->bw_t_bitmap);
		bw_t_info->eir_bw.bw = bw;
	}
}

 
static void ice_set_clear_shared_bw(struct ice_bw_type_info *bw_t_info, u32 bw)
{
	if (bw == ICE_SCHED_DFLT_BW) {
		clear_bit(ICE_BW_TYPE_SHARED, bw_t_info->bw_t_bitmap);
		bw_t_info->shared_bw = 0;
	} else {
		 
		clear_bit(ICE_BW_TYPE_EIR, bw_t_info->bw_t_bitmap);
		bw_t_info->eir_bw.bw = 0;
		 
		set_bit(ICE_BW_TYPE_SHARED, bw_t_info->bw_t_bitmap);
		bw_t_info->shared_bw = bw;
	}
}

 
static int
ice_sched_save_vsi_bw(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		      enum ice_rl_type rl_type, u32 bw)
{
	struct ice_vsi_ctx *vsi_ctx;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return -EINVAL;
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return -EINVAL;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw(&vsi_ctx->sched.bw_t_info[tc], bw);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw(&vsi_ctx->sched.bw_t_info[tc], bw);
		break;
	case ICE_SHARED_BW:
		ice_set_clear_shared_bw(&vsi_ctx->sched.bw_t_info[tc], bw);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 
static u16 ice_sched_calc_wakeup(struct ice_hw *hw, s32 bw)
{
	s64 bytes_per_sec, wakeup_int, wakeup_a, wakeup_b, wakeup_f;
	s32 wakeup_f_int;
	u16 wakeup = 0;

	 
	bytes_per_sec = div64_long(((s64)bw * 1000), BITS_PER_BYTE);
	wakeup_int = div64_long(hw->psm_clk_freq, bytes_per_sec);
	if (wakeup_int > 63) {
		wakeup = (u16)((1 << 15) | wakeup_int);
	} else {
		 
		wakeup_b = (s64)ICE_RL_PROF_MULTIPLIER * wakeup_int;
		wakeup_a = div64_long((s64)ICE_RL_PROF_MULTIPLIER *
					   hw->psm_clk_freq, bytes_per_sec);

		 
		wakeup_f = wakeup_a - wakeup_b;

		 
		if (wakeup_f > div64_long(ICE_RL_PROF_MULTIPLIER, 2))
			wakeup_f += 1;

		wakeup_f_int = (s32)div64_long(wakeup_f * ICE_RL_PROF_FRACTION,
					       ICE_RL_PROF_MULTIPLIER);
		wakeup |= (u16)(wakeup_int << 9);
		wakeup |= (u16)(0x1ff & wakeup_f_int);
	}

	return wakeup;
}

 
static int
ice_sched_bw_to_rl_profile(struct ice_hw *hw, u32 bw,
			   struct ice_aqc_rl_profile_elem *profile)
{
	s64 bytes_per_sec, ts_rate, mv_tmp;
	int status = -EINVAL;
	bool found = false;
	s32 encode = 0;
	s64 mv = 0;
	s32 i;

	 
	if (bw < ICE_SCHED_MIN_BW || bw > ICE_SCHED_MAX_BW)
		return status;

	 
	bytes_per_sec = div64_long(((s64)bw * 1000), BITS_PER_BYTE);

	 
	for (i = 0; i < 64; i++) {
		u64 pow_result = BIT_ULL(i);

		ts_rate = div64_long((s64)hw->psm_clk_freq,
				     pow_result * ICE_RL_PROF_TS_MULTIPLIER);
		if (ts_rate <= 0)
			continue;

		 
		mv_tmp = div64_long(bytes_per_sec * ICE_RL_PROF_MULTIPLIER,
				    ts_rate);

		 
		mv = round_up_64bit(mv_tmp, ICE_RL_PROF_MULTIPLIER);

		 
		if (mv > ICE_RL_PROF_ACCURACY_BYTES) {
			encode = i;
			found = true;
			break;
		}
	}
	if (found) {
		u16 wm;

		wm = ice_sched_calc_wakeup(hw, bw);
		profile->rl_multiply = cpu_to_le16(mv);
		profile->wake_up_calc = cpu_to_le16(wm);
		profile->rl_encode = cpu_to_le16(encode);
		status = 0;
	} else {
		status = -ENOENT;
	}

	return status;
}

 
static struct ice_aqc_rl_profile_info *
ice_sched_add_rl_profile(struct ice_port_info *pi,
			 enum ice_rl_type rl_type, u32 bw, u8 layer_num)
{
	struct ice_aqc_rl_profile_info *rl_prof_elem;
	u16 profiles_added = 0, num_profiles = 1;
	struct ice_aqc_rl_profile_elem *buf;
	struct ice_hw *hw;
	u8 profile_type;
	int status;

	if (layer_num >= ICE_AQC_TOPO_MAX_LEVEL_NUM)
		return NULL;
	switch (rl_type) {
	case ICE_MIN_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_CIR;
		break;
	case ICE_MAX_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_EIR;
		break;
	case ICE_SHARED_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_SRL;
		break;
	default:
		return NULL;
	}

	if (!pi)
		return NULL;
	hw = pi->hw;
	list_for_each_entry(rl_prof_elem, &pi->rl_prof_list[layer_num],
			    list_entry)
		if ((rl_prof_elem->profile.flags & ICE_AQC_RL_PROFILE_TYPE_M) ==
		    profile_type && rl_prof_elem->bw == bw)
			 
			return rl_prof_elem;

	 
	rl_prof_elem = devm_kzalloc(ice_hw_to_dev(hw), sizeof(*rl_prof_elem),
				    GFP_KERNEL);

	if (!rl_prof_elem)
		return NULL;

	status = ice_sched_bw_to_rl_profile(hw, bw, &rl_prof_elem->profile);
	if (status)
		goto exit_add_rl_prof;

	rl_prof_elem->bw = bw;
	 
	rl_prof_elem->profile.level = layer_num + 1;
	rl_prof_elem->profile.flags = profile_type;
	rl_prof_elem->profile.max_burst_size = cpu_to_le16(hw->max_burst_size);

	 
	buf = &rl_prof_elem->profile;
	status = ice_aq_add_rl_profile(hw, num_profiles, buf, sizeof(*buf),
				       &profiles_added, NULL);
	if (status || profiles_added != num_profiles)
		goto exit_add_rl_prof;

	 
	rl_prof_elem->prof_id_ref = 0;
	list_add(&rl_prof_elem->list_entry, &pi->rl_prof_list[layer_num]);
	return rl_prof_elem;

exit_add_rl_prof:
	devm_kfree(ice_hw_to_dev(hw), rl_prof_elem);
	return NULL;
}

 
static int
ice_sched_cfg_node_bw_lmt(struct ice_hw *hw, struct ice_sched_node *node,
			  enum ice_rl_type rl_type, u16 rl_prof_id)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;
	switch (rl_type) {
	case ICE_MIN_BW:
		data->valid_sections |= ICE_AQC_ELEM_VALID_CIR;
		data->cir_bw.bw_profile_idx = cpu_to_le16(rl_prof_id);
		break;
	case ICE_MAX_BW:
		 
		if (data->valid_sections & ICE_AQC_ELEM_VALID_SHARED)
			return -EIO;
		data->valid_sections |= ICE_AQC_ELEM_VALID_EIR;
		data->eir_bw.bw_profile_idx = cpu_to_le16(rl_prof_id);
		break;
	case ICE_SHARED_BW:
		 
		if (rl_prof_id == ICE_SCHED_NO_SHARED_RL_PROF_ID) {
			 
			data->valid_sections &= ~ICE_AQC_ELEM_VALID_SHARED;
			data->srl_id = 0;  

			 
			data->valid_sections |= ICE_AQC_ELEM_VALID_EIR;
			data->eir_bw.bw_profile_idx =
				cpu_to_le16(ICE_SCHED_DFLT_RL_PROF_ID);
			break;
		}
		 
		if ((data->valid_sections & ICE_AQC_ELEM_VALID_EIR) &&
		    (le16_to_cpu(data->eir_bw.bw_profile_idx) !=
			    ICE_SCHED_DFLT_RL_PROF_ID))
			return -EIO;
		 
		data->valid_sections &= ~ICE_AQC_ELEM_VALID_EIR;
		 
		data->valid_sections |= ICE_AQC_ELEM_VALID_SHARED;
		data->srl_id = cpu_to_le16(rl_prof_id);
		break;
	default:
		 
		return -EINVAL;
	}

	 
	return ice_sched_update_elem(hw, node, &buf);
}

 
static u16
ice_sched_get_node_rl_prof_id(struct ice_sched_node *node,
			      enum ice_rl_type rl_type)
{
	u16 rl_prof_id = ICE_SCHED_INVAL_PROF_ID;
	struct ice_aqc_txsched_elem *data;

	data = &node->info.data;
	switch (rl_type) {
	case ICE_MIN_BW:
		if (data->valid_sections & ICE_AQC_ELEM_VALID_CIR)
			rl_prof_id = le16_to_cpu(data->cir_bw.bw_profile_idx);
		break;
	case ICE_MAX_BW:
		if (data->valid_sections & ICE_AQC_ELEM_VALID_EIR)
			rl_prof_id = le16_to_cpu(data->eir_bw.bw_profile_idx);
		break;
	case ICE_SHARED_BW:
		if (data->valid_sections & ICE_AQC_ELEM_VALID_SHARED)
			rl_prof_id = le16_to_cpu(data->srl_id);
		break;
	default:
		break;
	}

	return rl_prof_id;
}

 
static u8
ice_sched_get_rl_prof_layer(struct ice_port_info *pi, enum ice_rl_type rl_type,
			    u8 layer_index)
{
	struct ice_hw *hw = pi->hw;

	if (layer_index >= hw->num_tx_sched_layers)
		return ICE_SCHED_INVAL_LAYER_NUM;
	switch (rl_type) {
	case ICE_MIN_BW:
		if (hw->layer_info[layer_index].max_cir_rl_profiles)
			return layer_index;
		break;
	case ICE_MAX_BW:
		if (hw->layer_info[layer_index].max_eir_rl_profiles)
			return layer_index;
		break;
	case ICE_SHARED_BW:
		 
		if (hw->layer_info[layer_index].max_srl_profiles)
			return layer_index;
		else if (layer_index < hw->num_tx_sched_layers - 1 &&
			 hw->layer_info[layer_index + 1].max_srl_profiles)
			return layer_index + 1;
		else if (layer_index > 0 &&
			 hw->layer_info[layer_index - 1].max_srl_profiles)
			return layer_index - 1;
		break;
	default:
		break;
	}
	return ICE_SCHED_INVAL_LAYER_NUM;
}

 
static struct ice_sched_node *
ice_sched_get_srl_node(struct ice_sched_node *node, u8 srl_layer)
{
	if (srl_layer > node->tx_sched_layer)
		return node->children[0];
	else if (srl_layer < node->tx_sched_layer)
		 
		return node->parent;
	else
		return node;
}

 
static int
ice_sched_rm_rl_profile(struct ice_port_info *pi, u8 layer_num, u8 profile_type,
			u16 profile_id)
{
	struct ice_aqc_rl_profile_info *rl_prof_elem;
	int status = 0;

	if (layer_num >= ICE_AQC_TOPO_MAX_LEVEL_NUM)
		return -EINVAL;
	 
	list_for_each_entry(rl_prof_elem, &pi->rl_prof_list[layer_num],
			    list_entry)
		if ((rl_prof_elem->profile.flags & ICE_AQC_RL_PROFILE_TYPE_M) ==
		    profile_type &&
		    le16_to_cpu(rl_prof_elem->profile.profile_id) ==
		    profile_id) {
			if (rl_prof_elem->prof_id_ref)
				rl_prof_elem->prof_id_ref--;

			 
			status = ice_sched_del_rl_profile(pi->hw, rl_prof_elem);
			if (status && status != -EBUSY)
				ice_debug(pi->hw, ICE_DBG_SCHED, "Remove rl profile failed\n");
			break;
		}
	if (status == -EBUSY)
		status = 0;
	return status;
}

 
static int
ice_sched_set_node_bw_dflt(struct ice_port_info *pi,
			   struct ice_sched_node *node,
			   enum ice_rl_type rl_type, u8 layer_num)
{
	struct ice_hw *hw;
	u8 profile_type;
	u16 rl_prof_id;
	u16 old_id;
	int status;

	hw = pi->hw;
	switch (rl_type) {
	case ICE_MIN_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_CIR;
		rl_prof_id = ICE_SCHED_DFLT_RL_PROF_ID;
		break;
	case ICE_MAX_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_EIR;
		rl_prof_id = ICE_SCHED_DFLT_RL_PROF_ID;
		break;
	case ICE_SHARED_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_SRL;
		 
		rl_prof_id = ICE_SCHED_NO_SHARED_RL_PROF_ID;
		break;
	default:
		return -EINVAL;
	}
	 
	old_id = ice_sched_get_node_rl_prof_id(node, rl_type);
	 
	status = ice_sched_cfg_node_bw_lmt(hw, node, rl_type, rl_prof_id);
	if (status)
		return status;

	 
	if (old_id == ICE_SCHED_DFLT_RL_PROF_ID ||
	    old_id == ICE_SCHED_INVAL_PROF_ID)
		return 0;

	return ice_sched_rm_rl_profile(pi, layer_num, profile_type, old_id);
}

 
static int
ice_sched_set_eir_srl_excl(struct ice_port_info *pi,
			   struct ice_sched_node *node,
			   u8 layer_num, enum ice_rl_type rl_type, u32 bw)
{
	if (rl_type == ICE_SHARED_BW) {
		 
		if (bw == ICE_SCHED_DFLT_BW)
			 
			return 0;

		 
		return ice_sched_set_node_bw_dflt(pi, node, ICE_MAX_BW,
						  layer_num);
	} else if (rl_type == ICE_MAX_BW &&
		   node->info.data.valid_sections & ICE_AQC_ELEM_VALID_SHARED) {
		 
		return ice_sched_set_node_bw_dflt(pi, node,
						  ICE_SHARED_BW,
						  layer_num);
	}
	return 0;
}

 
int
ice_sched_set_node_bw(struct ice_port_info *pi, struct ice_sched_node *node,
		      enum ice_rl_type rl_type, u32 bw, u8 layer_num)
{
	struct ice_aqc_rl_profile_info *rl_prof_info;
	struct ice_hw *hw = pi->hw;
	u16 old_id, rl_prof_id;
	int status = -EINVAL;

	rl_prof_info = ice_sched_add_rl_profile(pi, rl_type, bw, layer_num);
	if (!rl_prof_info)
		return status;

	rl_prof_id = le16_to_cpu(rl_prof_info->profile.profile_id);

	 
	old_id = ice_sched_get_node_rl_prof_id(node, rl_type);
	 
	status = ice_sched_cfg_node_bw_lmt(hw, node, rl_type, rl_prof_id);
	if (status)
		return status;

	 
	 
	rl_prof_info->prof_id_ref++;

	 
	if ((old_id == ICE_SCHED_DFLT_RL_PROF_ID && rl_type != ICE_SHARED_BW) ||
	    old_id == ICE_SCHED_INVAL_PROF_ID || old_id == rl_prof_id)
		return 0;

	return ice_sched_rm_rl_profile(pi, layer_num,
				       rl_prof_info->profile.flags &
				       ICE_AQC_RL_PROFILE_TYPE_M, old_id);
}

 
int
ice_sched_set_node_priority(struct ice_port_info *pi, struct ice_sched_node *node,
			    u16 priority)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;

	data->valid_sections |= ICE_AQC_ELEM_VALID_GENERIC;
	data->generic |= FIELD_PREP(ICE_AQC_ELEM_GENERIC_PRIO_M, priority);

	return ice_sched_update_elem(pi->hw, node, &buf);
}

 
int
ice_sched_set_node_weight(struct ice_port_info *pi, struct ice_sched_node *node, u16 weight)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;

	data->valid_sections = ICE_AQC_ELEM_VALID_CIR | ICE_AQC_ELEM_VALID_EIR |
			       ICE_AQC_ELEM_VALID_GENERIC;
	data->cir_bw.bw_alloc = cpu_to_le16(weight);
	data->eir_bw.bw_alloc = cpu_to_le16(weight);

	data->generic |= FIELD_PREP(ICE_AQC_ELEM_GENERIC_SP_M, 0x0);

	return ice_sched_update_elem(pi->hw, node, &buf);
}

 
int
ice_sched_set_node_bw_lmt(struct ice_port_info *pi, struct ice_sched_node *node,
			  enum ice_rl_type rl_type, u32 bw)
{
	struct ice_sched_node *cfg_node = node;
	int status;

	struct ice_hw *hw;
	u8 layer_num;

	if (!pi)
		return -EINVAL;
	hw = pi->hw;
	 
	ice_sched_rm_unused_rl_prof(pi);
	layer_num = ice_sched_get_rl_prof_layer(pi, rl_type,
						node->tx_sched_layer);
	if (layer_num >= hw->num_tx_sched_layers)
		return -EINVAL;

	if (rl_type == ICE_SHARED_BW) {
		 
		cfg_node = ice_sched_get_srl_node(node, layer_num);
		if (!cfg_node)
			return -EIO;
	}
	 
	status = ice_sched_set_eir_srl_excl(pi, cfg_node, layer_num, rl_type,
					    bw);
	if (status)
		return status;
	if (bw == ICE_SCHED_DFLT_BW)
		return ice_sched_set_node_bw_dflt(pi, cfg_node, rl_type,
						  layer_num);
	return ice_sched_set_node_bw(pi, cfg_node, rl_type, bw, layer_num);
}

 
static int
ice_sched_set_node_bw_dflt_lmt(struct ice_port_info *pi,
			       struct ice_sched_node *node,
			       enum ice_rl_type rl_type)
{
	return ice_sched_set_node_bw_lmt(pi, node, rl_type,
					 ICE_SCHED_DFLT_BW);
}

 
static int
ice_sched_validate_srl_node(struct ice_sched_node *node, u8 sel_layer)
{
	 
	if (sel_layer == node->tx_sched_layer ||
	    ((sel_layer == node->tx_sched_layer + 1) &&
	    node->num_children == 1) ||
	    ((sel_layer == node->tx_sched_layer - 1) &&
	    (node->parent && node->parent->num_children == 1)))
		return 0;

	return -EIO;
}

 
static int
ice_sched_save_q_bw(struct ice_q_ctx *q_ctx, enum ice_rl_type rl_type, u32 bw)
{
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw(&q_ctx->bw_t_info, bw);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw(&q_ctx->bw_t_info, bw);
		break;
	case ICE_SHARED_BW:
		ice_set_clear_shared_bw(&q_ctx->bw_t_info, bw);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

 
static int
ice_sched_set_q_bw_lmt(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		       u16 q_handle, enum ice_rl_type rl_type, u32 bw)
{
	struct ice_sched_node *node;
	struct ice_q_ctx *q_ctx;
	int status = -EINVAL;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return -EINVAL;
	mutex_lock(&pi->sched_lock);
	q_ctx = ice_get_lan_q_ctx(pi->hw, vsi_handle, tc, q_handle);
	if (!q_ctx)
		goto exit_q_bw_lmt;
	node = ice_sched_find_node_by_teid(pi->root, q_ctx->q_teid);
	if (!node) {
		ice_debug(pi->hw, ICE_DBG_SCHED, "Wrong q_teid\n");
		goto exit_q_bw_lmt;
	}

	 
	if (node->info.data.elem_type != ICE_AQC_ELEM_TYPE_LEAF)
		goto exit_q_bw_lmt;

	 
	if (rl_type == ICE_SHARED_BW) {
		u8 sel_layer;  

		sel_layer = ice_sched_get_rl_prof_layer(pi, rl_type,
							node->tx_sched_layer);
		if (sel_layer >= pi->hw->num_tx_sched_layers) {
			status = -EINVAL;
			goto exit_q_bw_lmt;
		}
		status = ice_sched_validate_srl_node(node, sel_layer);
		if (status)
			goto exit_q_bw_lmt;
	}

	if (bw == ICE_SCHED_DFLT_BW)
		status = ice_sched_set_node_bw_dflt_lmt(pi, node, rl_type);
	else
		status = ice_sched_set_node_bw_lmt(pi, node, rl_type, bw);

	if (!status)
		status = ice_sched_save_q_bw(q_ctx, rl_type, bw);

exit_q_bw_lmt:
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
int
ice_cfg_q_bw_lmt(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		 u16 q_handle, enum ice_rl_type rl_type, u32 bw)
{
	return ice_sched_set_q_bw_lmt(pi, vsi_handle, tc, q_handle, rl_type,
				      bw);
}

 
int
ice_cfg_q_bw_dflt_lmt(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		      u16 q_handle, enum ice_rl_type rl_type)
{
	return ice_sched_set_q_bw_lmt(pi, vsi_handle, tc, q_handle, rl_type,
				      ICE_SCHED_DFLT_BW);
}

 
static struct ice_sched_node *
ice_sched_get_node_by_id_type(struct ice_port_info *pi, u32 id,
			      enum ice_agg_type agg_type, u8 tc)
{
	struct ice_sched_node *node = NULL;

	switch (agg_type) {
	case ICE_AGG_TYPE_VSI: {
		struct ice_vsi_ctx *vsi_ctx;
		u16 vsi_handle = (u16)id;

		if (!ice_is_vsi_valid(pi->hw, vsi_handle))
			break;
		 
		vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
		if (!vsi_ctx)
			break;
		node = vsi_ctx->sched.vsi_node[tc];
		break;
	}

	case ICE_AGG_TYPE_AGG: {
		struct ice_sched_node *tc_node;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (tc_node)
			node = ice_sched_get_agg_node(pi, tc_node, id);
		break;
	}

	default:
		break;
	}

	return node;
}

 
static int
ice_sched_set_node_bw_lmt_per_tc(struct ice_port_info *pi, u32 id,
				 enum ice_agg_type agg_type, u8 tc,
				 enum ice_rl_type rl_type, u32 bw)
{
	struct ice_sched_node *node;
	int status = -EINVAL;

	if (!pi)
		return status;

	if (rl_type == ICE_UNKNOWN_BW)
		return status;

	mutex_lock(&pi->sched_lock);
	node = ice_sched_get_node_by_id_type(pi, id, agg_type, tc);
	if (!node) {
		ice_debug(pi->hw, ICE_DBG_SCHED, "Wrong id, agg type, or tc\n");
		goto exit_set_node_bw_lmt_per_tc;
	}
	if (bw == ICE_SCHED_DFLT_BW)
		status = ice_sched_set_node_bw_dflt_lmt(pi, node, rl_type);
	else
		status = ice_sched_set_node_bw_lmt(pi, node, rl_type, bw);

exit_set_node_bw_lmt_per_tc:
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
int
ice_cfg_vsi_bw_lmt_per_tc(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			  enum ice_rl_type rl_type, u32 bw)
{
	int status;

	status = ice_sched_set_node_bw_lmt_per_tc(pi, vsi_handle,
						  ICE_AGG_TYPE_VSI,
						  tc, rl_type, bw);
	if (!status) {
		mutex_lock(&pi->sched_lock);
		status = ice_sched_save_vsi_bw(pi, vsi_handle, tc, rl_type, bw);
		mutex_unlock(&pi->sched_lock);
	}
	return status;
}

 
int
ice_cfg_vsi_bw_dflt_lmt_per_tc(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			       enum ice_rl_type rl_type)
{
	int status;

	status = ice_sched_set_node_bw_lmt_per_tc(pi, vsi_handle,
						  ICE_AGG_TYPE_VSI,
						  tc, rl_type,
						  ICE_SCHED_DFLT_BW);
	if (!status) {
		mutex_lock(&pi->sched_lock);
		status = ice_sched_save_vsi_bw(pi, vsi_handle, tc, rl_type,
					       ICE_SCHED_DFLT_BW);
		mutex_unlock(&pi->sched_lock);
	}
	return status;
}

 
int ice_cfg_rl_burst_size(struct ice_hw *hw, u32 bytes)
{
	u16 burst_size_to_prog;

	if (bytes < ICE_MIN_BURST_SIZE_ALLOWED ||
	    bytes > ICE_MAX_BURST_SIZE_ALLOWED)
		return -EINVAL;
	if (ice_round_to_num(bytes, 64) <=
	    ICE_MAX_BURST_SIZE_64_BYTE_GRANULARITY) {
		 
		 
		burst_size_to_prog = ICE_64_BYTE_GRANULARITY;
		 
		bytes = ice_round_to_num(bytes, 64);
		 
		burst_size_to_prog |= (u16)(bytes / 64);
	} else {
		 
		 
		burst_size_to_prog = ICE_KBYTE_GRANULARITY;
		 
		bytes = ice_round_to_num(bytes, 1024);
		 
		if (bytes > ICE_MAX_BURST_SIZE_KBYTE_GRANULARITY)
			bytes = ICE_MAX_BURST_SIZE_KBYTE_GRANULARITY;
		 
		burst_size_to_prog |= (u16)(bytes / 1024);
	}
	hw->max_burst_size = burst_size_to_prog;
	return 0;
}

 
static int
ice_sched_replay_node_prio(struct ice_hw *hw, struct ice_sched_node *node,
			   u8 priority)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;
	int status;

	buf = node->info;
	data = &buf.data;
	data->valid_sections |= ICE_AQC_ELEM_VALID_GENERIC;
	data->generic = priority;

	 
	status = ice_sched_update_elem(hw, node, &buf);
	return status;
}

 
static int
ice_sched_replay_node_bw(struct ice_hw *hw, struct ice_sched_node *node,
			 struct ice_bw_type_info *bw_t_info)
{
	struct ice_port_info *pi = hw->port_info;
	int status = -EINVAL;
	u16 bw_alloc;

	if (!node)
		return status;
	if (bitmap_empty(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_CNT))
		return 0;
	if (test_bit(ICE_BW_TYPE_PRIO, bw_t_info->bw_t_bitmap)) {
		status = ice_sched_replay_node_prio(hw, node,
						    bw_t_info->generic);
		if (status)
			return status;
	}
	if (test_bit(ICE_BW_TYPE_CIR, bw_t_info->bw_t_bitmap)) {
		status = ice_sched_set_node_bw_lmt(pi, node, ICE_MIN_BW,
						   bw_t_info->cir_bw.bw);
		if (status)
			return status;
	}
	if (test_bit(ICE_BW_TYPE_CIR_WT, bw_t_info->bw_t_bitmap)) {
		bw_alloc = bw_t_info->cir_bw.bw_alloc;
		status = ice_sched_cfg_node_bw_alloc(hw, node, ICE_MIN_BW,
						     bw_alloc);
		if (status)
			return status;
	}
	if (test_bit(ICE_BW_TYPE_EIR, bw_t_info->bw_t_bitmap)) {
		status = ice_sched_set_node_bw_lmt(pi, node, ICE_MAX_BW,
						   bw_t_info->eir_bw.bw);
		if (status)
			return status;
	}
	if (test_bit(ICE_BW_TYPE_EIR_WT, bw_t_info->bw_t_bitmap)) {
		bw_alloc = bw_t_info->eir_bw.bw_alloc;
		status = ice_sched_cfg_node_bw_alloc(hw, node, ICE_MAX_BW,
						     bw_alloc);
		if (status)
			return status;
	}
	if (test_bit(ICE_BW_TYPE_SHARED, bw_t_info->bw_t_bitmap))
		status = ice_sched_set_node_bw_lmt(pi, node, ICE_SHARED_BW,
						   bw_t_info->shared_bw);
	return status;
}

 
static void
ice_sched_get_ena_tc_bitmap(struct ice_port_info *pi,
			    unsigned long *tc_bitmap,
			    unsigned long *ena_tc_bitmap)
{
	u8 tc;

	 
	ice_for_each_traffic_class(tc)
		if (ice_is_tc_ena(*tc_bitmap, tc) &&
		    (ice_sched_get_tc_node(pi, tc)))
			set_bit(tc, ena_tc_bitmap);
}

 
void ice_sched_replay_agg(struct ice_hw *hw)
{
	struct ice_port_info *pi = hw->port_info;
	struct ice_sched_agg_info *agg_info;

	mutex_lock(&pi->sched_lock);
	list_for_each_entry(agg_info, &hw->agg_list, list_entry)
		 
		if (!bitmap_equal(agg_info->tc_bitmap, agg_info->replay_tc_bitmap,
				  ICE_MAX_TRAFFIC_CLASS)) {
			DECLARE_BITMAP(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
			int status;

			bitmap_zero(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
			ice_sched_get_ena_tc_bitmap(pi,
						    agg_info->replay_tc_bitmap,
						    replay_bitmap);
			status = ice_sched_cfg_agg(hw->port_info,
						   agg_info->agg_id,
						   ICE_AGG_TYPE_AGG,
						   replay_bitmap);
			if (status) {
				dev_info(ice_hw_to_dev(hw),
					 "Replay agg id[%d] failed\n",
					 agg_info->agg_id);
				 
				continue;
			}
		}
	mutex_unlock(&pi->sched_lock);
}

 
void ice_sched_replay_agg_vsi_preinit(struct ice_hw *hw)
{
	struct ice_port_info *pi = hw->port_info;
	struct ice_sched_agg_info *agg_info;

	mutex_lock(&pi->sched_lock);
	list_for_each_entry(agg_info, &hw->agg_list, list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;

		agg_info->tc_bitmap[0] = 0;
		list_for_each_entry(agg_vsi_info, &agg_info->agg_vsi_list,
				    list_entry)
			agg_vsi_info->tc_bitmap[0] = 0;
	}
	mutex_unlock(&pi->sched_lock);
}

 
static int ice_sched_replay_vsi_agg(struct ice_hw *hw, u16 vsi_handle)
{
	DECLARE_BITMAP(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_port_info *pi = hw->port_info;
	struct ice_sched_agg_info *agg_info;
	int status;

	bitmap_zero(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return -EINVAL;
	agg_info = ice_get_vsi_agg_info(hw, vsi_handle);
	if (!agg_info)
		return 0;  
	agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
	if (!agg_vsi_info)
		return 0;  
	ice_sched_get_ena_tc_bitmap(pi, agg_info->replay_tc_bitmap,
				    replay_bitmap);
	 
	status = ice_sched_cfg_agg(hw->port_info, agg_info->agg_id,
				   ICE_AGG_TYPE_AGG, replay_bitmap);
	if (status)
		return status;

	bitmap_zero(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
	ice_sched_get_ena_tc_bitmap(pi, agg_vsi_info->replay_tc_bitmap,
				    replay_bitmap);
	 
	return ice_sched_assoc_vsi_to_agg(pi, agg_info->agg_id, vsi_handle,
					  replay_bitmap);
}

 
int ice_replay_vsi_agg(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_port_info *pi = hw->port_info;
	int status;

	mutex_lock(&pi->sched_lock);
	status = ice_sched_replay_vsi_agg(hw, vsi_handle);
	mutex_unlock(&pi->sched_lock);
	return status;
}

 
int ice_sched_replay_q_bw(struct ice_port_info *pi, struct ice_q_ctx *q_ctx)
{
	struct ice_sched_node *q_node;

	 
	q_node = ice_sched_find_node_by_teid(pi->root, q_ctx->q_teid);
	if (!q_node)
		return -EINVAL;
	return ice_sched_replay_node_bw(pi->hw, q_node, &q_ctx->bw_t_info);
}


 

#include <linux/fsl/mc.h>
#include "dpsw.h"
#include "dpsw-cmd.h"

static void build_if_id_bitmap(__le64 *bmap, const u16 *id, const u16 num_ifs)
{
	int i;

	for (i = 0; (i < num_ifs) && (i < DPSW_MAX_IF); i++) {
		if (id[i] < DPSW_MAX_IF)
			bmap[id[i] / 64] |= cpu_to_le64(BIT_MASK(id[i] % 64));
	}
}

 
int dpsw_open(struct fsl_mc_io *mc_io, u32 cmd_flags, int dpsw_id, u16 *token)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_open *cmd_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpsw_cmd_open *)cmd.params;
	cmd_params->dpsw_id = cpu_to_le32(dpsw_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

 
int dpsw_close(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CLOSE,
					  cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ENABLE,
					  cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_DISABLE,
					  cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_reset(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_RESET,
					  cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_set_irq_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 irq_index, u8 en)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_set_irq_enable *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_SET_IRQ_ENABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_set_irq_enable *)cmd.params;
	dpsw_set_field(cmd_params->enable_state, ENABLE, en);
	cmd_params->irq_index = irq_index;

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_set_irq_mask(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		      u8 irq_index, u32 mask)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_set_irq_mask *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_SET_IRQ_MASK,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_set_irq_mask *)cmd.params;
	cmd_params->mask = cpu_to_le32(mask);
	cmd_params->irq_index = irq_index;

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_get_irq_status(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 irq_index, u32 *status)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_get_irq_status *cmd_params;
	struct dpsw_rsp_get_irq_status *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_GET_IRQ_STATUS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_get_irq_status *)cmd.params;
	cmd_params->status = cpu_to_le32(*status);
	cmd_params->irq_index = irq_index;

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpsw_rsp_get_irq_status *)cmd.params;
	*status = le32_to_cpu(rsp_params->status);

	return 0;
}

 
int dpsw_clear_irq_status(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  u8 irq_index, u32 status)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_clear_irq_status *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CLEAR_IRQ_STATUS,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_clear_irq_status *)cmd.params;
	cmd_params->status = cpu_to_le32(status);
	cmd_params->irq_index = irq_index;

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			struct dpsw_attr *attr)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_rsp_get_attr *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpsw_rsp_get_attr *)cmd.params;
	attr->num_ifs = le16_to_cpu(rsp_params->num_ifs);
	attr->max_fdbs = rsp_params->max_fdbs;
	attr->num_fdbs = rsp_params->num_fdbs;
	attr->max_vlans = le16_to_cpu(rsp_params->max_vlans);
	attr->num_vlans = le16_to_cpu(rsp_params->num_vlans);
	attr->max_fdb_entries = le16_to_cpu(rsp_params->max_fdb_entries);
	attr->fdb_aging_time = le16_to_cpu(rsp_params->fdb_aging_time);
	attr->id = le32_to_cpu(rsp_params->dpsw_id);
	attr->mem_size = le16_to_cpu(rsp_params->mem_size);
	attr->max_fdb_mc_groups = le16_to_cpu(rsp_params->max_fdb_mc_groups);
	attr->max_meters_per_if = rsp_params->max_meters_per_if;
	attr->options = le64_to_cpu(rsp_params->options);
	attr->component_type = dpsw_get_field(rsp_params->component_type, COMPONENT_TYPE);
	attr->flooding_cfg = dpsw_get_field(rsp_params->repl_cfg, FLOODING_CFG);
	attr->broadcast_cfg = dpsw_get_field(rsp_params->repl_cfg, BROADCAST_CFG);
	return 0;
}

 
int dpsw_if_set_link_cfg(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
			 struct dpsw_link_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_set_link_cfg *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_SET_LINK_CFG,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_set_link_cfg *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->rate = cpu_to_le32(cfg->rate);
	cmd_params->options = cpu_to_le64(cfg->options);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_get_link_state(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id, struct dpsw_link_state *state)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_get_link_state *cmd_params;
	struct dpsw_rsp_if_get_link_state *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_GET_LINK_STATE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_get_link_state *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpsw_rsp_if_get_link_state *)cmd.params;
	state->rate = le32_to_cpu(rsp_params->rate);
	state->options = le64_to_cpu(rsp_params->options);
	state->up = dpsw_get_field(rsp_params->up, UP);

	return 0;
}

 
int dpsw_if_set_tci(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
		    const struct dpsw_tci_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_set_tci *cmd_params;
	u16 tmp_conf = 0;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_SET_TCI,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_set_tci *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	dpsw_set_field(tmp_conf, VLAN_ID, cfg->vlan_id);
	dpsw_set_field(tmp_conf, DEI, cfg->dei);
	dpsw_set_field(tmp_conf, PCP, cfg->pcp);
	cmd_params->conf = cpu_to_le16(tmp_conf);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_get_tci(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
		    struct dpsw_tci_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_get_tci *cmd_params;
	struct dpsw_rsp_if_get_tci *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_GET_TCI,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_get_tci *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpsw_rsp_if_get_tci *)cmd.params;
	cfg->pcp = rsp_params->pcp;
	cfg->dei = rsp_params->dei;
	cfg->vlan_id = le16_to_cpu(rsp_params->vlan_id);

	return 0;
}

 
int dpsw_if_set_stp(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
		    const struct dpsw_stp_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_set_stp *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_SET_STP,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_set_stp *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->vlan_id = cpu_to_le16(cfg->vlan_id);
	dpsw_set_field(cmd_params->state, STATE, cfg->state);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_get_counter(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u16 if_id, enum dpsw_counter type, u64 *counter)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_get_counter *cmd_params;
	struct dpsw_rsp_if_get_counter *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_GET_COUNTER,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_get_counter *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	dpsw_set_field(cmd_params->type, COUNTER_TYPE, type);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpsw_rsp_if_get_counter *)cmd.params;
	*counter = le64_to_cpu(rsp_params->counter);

	return 0;
}

 
int dpsw_if_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_ENABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_DISABLE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id, struct dpsw_if_attr *attr)
{
	struct dpsw_rsp_if_get_attr *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if *cmd_params;
	int err;

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_GET_ATTR, cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpsw_rsp_if_get_attr *)cmd.params;
	attr->num_tcs = rsp_params->num_tcs;
	attr->rate = le32_to_cpu(rsp_params->rate);
	attr->options = le32_to_cpu(rsp_params->options);
	attr->qdid = le16_to_cpu(rsp_params->qdid);
	attr->enabled = dpsw_get_field(rsp_params->conf, ENABLED);
	attr->accept_all_vlan = dpsw_get_field(rsp_params->conf,
					       ACCEPT_ALL_VLAN);
	attr->admit_untagged = dpsw_get_field(rsp_params->conf,
					      ADMIT_UNTAGGED);

	return 0;
}

 
int dpsw_if_set_max_frame_length(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
				 u16 if_id, u16 frame_length)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if_set_max_frame_length *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_SET_MAX_FRAME_LENGTH,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_set_max_frame_length *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->frame_length = cpu_to_le16(frame_length);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_vlan_add(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		  u16 vlan_id, const struct dpsw_vlan_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_vlan_add *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_VLAN_ADD,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_vlan_add *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(cfg->fdb_id);
	cmd_params->vlan_id = cpu_to_le16(vlan_id);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_vlan_add_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		     u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg)
{
	struct dpsw_cmd_vlan_add_if *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_VLAN_ADD_IF,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_vlan_add_if *)cmd.params;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);
	cmd_params->options = cpu_to_le16(cfg->options);
	cmd_params->fdb_id = cpu_to_le16(cfg->fdb_id);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_vlan_add_if_untagged(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_vlan_manage_if *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_VLAN_ADD_IF_UNTAGGED,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_vlan_manage_if *)cmd.params;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_vlan_remove_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_vlan_manage_if *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_VLAN_REMOVE_IF,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_vlan_manage_if *)cmd.params;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_vlan_remove_if_untagged(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
				 u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_vlan_manage_if *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_VLAN_REMOVE_IF_UNTAGGED,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_vlan_manage_if *)cmd.params;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_vlan_remove(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		     u16 vlan_id)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_vlan_remove *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_VLAN_REMOVE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_vlan_remove *)cmd.params;
	cmd_params->vlan_id = cpu_to_le16(vlan_id);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_fdb_add(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 *fdb_id,
		 const struct dpsw_fdb_cfg *cfg)
{
	struct dpsw_cmd_fdb_add *cmd_params;
	struct dpsw_rsp_fdb_add *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_ADD,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_add *)cmd.params;
	cmd_params->fdb_ageing_time = cpu_to_le16(cfg->fdb_ageing_time);
	cmd_params->num_fdb_entries = cpu_to_le16(cfg->num_fdb_entries);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpsw_rsp_fdb_add *)cmd.params;
	*fdb_id = le16_to_cpu(rsp_params->fdb_id);

	return 0;
}

 
int dpsw_fdb_remove(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 fdb_id)
{
	struct dpsw_cmd_fdb_remove *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_REMOVE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_remove *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(fdb_id);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_fdb_add_unicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			 u16 fdb_id, const struct dpsw_fdb_unicast_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_fdb_unicast_op *cmd_params;
	int i;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_ADD_UNICAST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_unicast_op *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(fdb_id);
	cmd_params->if_egress = cpu_to_le16(cfg->if_egress);
	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = cfg->mac_addr[5 - i];
	dpsw_set_field(cmd_params->type, ENTRY_TYPE, cfg->type);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_fdb_dump(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 fdb_id,
		  u64 iova_addr, u32 iova_size, u16 *num_entries)
{
	struct dpsw_cmd_fdb_dump *cmd_params;
	struct dpsw_rsp_fdb_dump *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_DUMP,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_dump *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(fdb_id);
	cmd_params->iova_addr = cpu_to_le64(iova_addr);
	cmd_params->iova_size = cpu_to_le32(iova_size);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpsw_rsp_fdb_dump *)cmd.params;
	*num_entries = le16_to_cpu(rsp_params->num_entries);

	return 0;
}

 
int dpsw_fdb_remove_unicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			    u16 fdb_id, const struct dpsw_fdb_unicast_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_fdb_unicast_op *cmd_params;
	int i;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_REMOVE_UNICAST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_unicast_op *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(fdb_id);
	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = cfg->mac_addr[5 - i];
	cmd_params->if_egress = cpu_to_le16(cfg->if_egress);
	dpsw_set_field(cmd_params->type, ENTRY_TYPE, cfg->type);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_fdb_add_multicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 fdb_id, const struct dpsw_fdb_multicast_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_fdb_multicast_op *cmd_params;
	int i;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_ADD_MULTICAST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_multicast_op *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(fdb_id);
	cmd_params->num_ifs = cpu_to_le16(cfg->num_ifs);
	dpsw_set_field(cmd_params->type, ENTRY_TYPE, cfg->type);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);
	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = cfg->mac_addr[5 - i];

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_fdb_remove_multicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 fdb_id, const struct dpsw_fdb_multicast_cfg *cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_fdb_multicast_op *cmd_params;
	int i;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_FDB_REMOVE_MULTICAST,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_fdb_multicast_op *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(fdb_id);
	cmd_params->num_ifs = cpu_to_le16(cfg->num_ifs);
	dpsw_set_field(cmd_params->type, ENTRY_TYPE, cfg->type);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);
	for (i = 0; i < 6; i++)
		cmd_params->mac_addr[i] = cfg->mac_addr[5 - i];

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_ctrl_if_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags,
				u16 token, struct dpsw_ctrl_if_attr *attr)
{
	struct dpsw_rsp_ctrl_if_get_attr *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CTRL_IF_GET_ATTR,
					  cmd_flags, token);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpsw_rsp_ctrl_if_get_attr *)cmd.params;
	attr->rx_fqid = le32_to_cpu(rsp_params->rx_fqid);
	attr->rx_err_fqid = le32_to_cpu(rsp_params->rx_err_fqid);
	attr->tx_err_conf_fqid = le32_to_cpu(rsp_params->tx_err_conf_fqid);

	return 0;
}

 
int dpsw_ctrl_if_set_pools(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   const struct dpsw_ctrl_if_pools_cfg *cfg)
{
	struct dpsw_cmd_ctrl_if_set_pools *cmd_params;
	struct fsl_mc_command cmd = { 0 };
	int i;

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CTRL_IF_SET_POOLS,
					  cmd_flags, token);
	cmd_params = (struct dpsw_cmd_ctrl_if_set_pools *)cmd.params;
	cmd_params->num_dpbp = cfg->num_dpbp;
	for (i = 0; i < DPSW_MAX_DPBP; i++) {
		cmd_params->dpbp_id[i] = cpu_to_le32(cfg->pools[i].dpbp_id);
		cmd_params->buffer_size[i] =
			cpu_to_le16(cfg->pools[i].buffer_size);
		cmd_params->backup_pool_mask |=
			DPSW_BACKUP_POOL(cfg->pools[i].backup_pool, i);
	}

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_ctrl_if_set_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   enum dpsw_queue_type qtype,
			   const struct dpsw_ctrl_if_queue_cfg *cfg)
{
	struct dpsw_cmd_ctrl_if_set_queue *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CTRL_IF_SET_QUEUE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_ctrl_if_set_queue *)cmd.params;
	cmd_params->dest_id = cpu_to_le32(cfg->dest_cfg.dest_id);
	cmd_params->dest_priority = cfg->dest_cfg.priority;
	cmd_params->qtype = qtype;
	cmd_params->user_ctx = cpu_to_le64(cfg->user_ctx);
	cmd_params->options = cpu_to_le32(cfg->options);
	dpsw_set_field(cmd_params->dest_type,
		       DEST_TYPE,
		       cfg->dest_cfg.dest_type);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_get_api_version(struct fsl_mc_io *mc_io, u32 cmd_flags,
			 u16 *major_ver, u16 *minor_ver)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_rsp_get_api_version *rsp_params;
	int err;

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_GET_API_VERSION,
					  cmd_flags,
					  0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpsw_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->version_major);
	*minor_ver = le16_to_cpu(rsp_params->version_minor);

	return 0;
}

 
int dpsw_if_get_port_mac_addr(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 if_id, u8 mac_addr[6])
{
	struct dpsw_rsp_if_get_mac_addr *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	struct dpsw_cmd_if *cmd_params;
	int err, i;

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_GET_PORT_MAC_ADDR,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpsw_rsp_if_get_mac_addr *)cmd.params;
	for (i = 0; i < 6; i++)
		mac_addr[5 - i] = rsp_params->mac_addr[i];

	return 0;
}

 
int dpsw_ctrl_if_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CTRL_IF_ENABLE, cmd_flags,
					  token);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_ctrl_if_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_CTRL_IF_DISABLE,
					  cmd_flags,
					  token);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_set_egress_flood(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  const struct dpsw_egress_flood_cfg *cfg)
{
	struct dpsw_cmd_set_egress_flood *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_SET_EGRESS_FLOOD, cmd_flags, token);
	cmd_params = (struct dpsw_cmd_set_egress_flood *)cmd.params;
	cmd_params->fdb_id = cpu_to_le16(cfg->fdb_id);
	cmd_params->flood_type = cfg->flood_type;
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_set_learning_mode(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 if_id, enum dpsw_learning_mode mode)
{
	struct dpsw_cmd_if_set_learning_mode *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_SET_LEARNING_MODE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_set_learning_mode *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	dpsw_set_field(cmd_params->mode, LEARNING_MODE, mode);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_acl_add(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 *acl_id,
		 const struct dpsw_acl_cfg *cfg)
{
	struct dpsw_cmd_acl_add *cmd_params;
	struct dpsw_rsp_acl_add *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ACL_ADD, cmd_flags, token);
	cmd_params = (struct dpsw_cmd_acl_add *)cmd.params;
	cmd_params->max_entries = cpu_to_le16(cfg->max_entries);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpsw_rsp_acl_add *)cmd.params;
	*acl_id = le16_to_cpu(rsp_params->acl_id);

	return 0;
}

 
int dpsw_acl_remove(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		    u16 acl_id)
{
	struct dpsw_cmd_acl_remove *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ACL_REMOVE, cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_acl_remove *)cmd.params;
	cmd_params->acl_id = cpu_to_le16(acl_id);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_acl_add_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		    u16 acl_id, const struct dpsw_acl_if_cfg *cfg)
{
	struct dpsw_cmd_acl_if *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ACL_ADD_IF, cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_acl_if *)cmd.params;
	cmd_params->acl_id = cpu_to_le16(acl_id);
	cmd_params->num_ifs = cpu_to_le16(cfg->num_ifs);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_acl_remove_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		       u16 acl_id, const struct dpsw_acl_if_cfg *cfg)
{
	struct dpsw_cmd_acl_if *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ACL_REMOVE_IF, cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_acl_if *)cmd.params;
	cmd_params->acl_id = cpu_to_le16(acl_id);
	cmd_params->num_ifs = cpu_to_le16(cfg->num_ifs);
	build_if_id_bitmap(&cmd_params->if_id, cfg->if_id, cfg->num_ifs);

	 
	return mc_send_command(mc_io, &cmd);
}

 
void dpsw_acl_prepare_entry_cfg(const struct dpsw_acl_key *key,
				u8 *entry_cfg_buf)
{
	struct dpsw_prep_acl_entry *ext_params;
	int i;

	ext_params = (struct dpsw_prep_acl_entry *)entry_cfg_buf;

	for (i = 0; i < 6; i++) {
		ext_params->match_l2_dest_mac[i] = key->match.l2_dest_mac[5 - i];
		ext_params->match_l2_source_mac[i] = key->match.l2_source_mac[5 - i];
		ext_params->mask_l2_dest_mac[i] = key->mask.l2_dest_mac[5 - i];
		ext_params->mask_l2_source_mac[i] = key->mask.l2_source_mac[5 - i];
	}

	ext_params->match_l2_tpid = cpu_to_le16(key->match.l2_tpid);
	ext_params->match_l2_vlan_id = cpu_to_le16(key->match.l2_vlan_id);
	ext_params->match_l3_dest_ip = cpu_to_le32(key->match.l3_dest_ip);
	ext_params->match_l3_source_ip = cpu_to_le32(key->match.l3_source_ip);
	ext_params->match_l4_dest_port = cpu_to_le16(key->match.l4_dest_port);
	ext_params->match_l4_source_port = cpu_to_le16(key->match.l4_source_port);
	ext_params->match_l2_ether_type = cpu_to_le16(key->match.l2_ether_type);
	ext_params->match_l2_pcp_dei = key->match.l2_pcp_dei;
	ext_params->match_l3_dscp = key->match.l3_dscp;

	ext_params->mask_l2_tpid = cpu_to_le16(key->mask.l2_tpid);
	ext_params->mask_l2_vlan_id = cpu_to_le16(key->mask.l2_vlan_id);
	ext_params->mask_l3_dest_ip = cpu_to_le32(key->mask.l3_dest_ip);
	ext_params->mask_l3_source_ip = cpu_to_le32(key->mask.l3_source_ip);
	ext_params->mask_l4_dest_port = cpu_to_le16(key->mask.l4_dest_port);
	ext_params->mask_l4_source_port = cpu_to_le16(key->mask.l4_source_port);
	ext_params->mask_l2_ether_type = cpu_to_le16(key->mask.l2_ether_type);
	ext_params->mask_l2_pcp_dei = key->mask.l2_pcp_dei;
	ext_params->mask_l3_dscp = key->mask.l3_dscp;
	ext_params->match_l3_protocol = key->match.l3_protocol;
	ext_params->mask_l3_protocol = key->mask.l3_protocol;
}

 
int dpsw_acl_add_entry(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		       u16 acl_id, const struct dpsw_acl_entry_cfg *cfg)
{
	struct dpsw_cmd_acl_entry *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ACL_ADD_ENTRY, cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_acl_entry *)cmd.params;
	cmd_params->acl_id = cpu_to_le16(acl_id);
	cmd_params->result_if_id = cpu_to_le16(cfg->result.if_id);
	cmd_params->precedence = cpu_to_le32(cfg->precedence);
	cmd_params->key_iova = cpu_to_le64(cfg->key_iova);
	dpsw_set_field(cmd_params->result_action,
		       RESULT_ACTION,
		       cfg->result.action);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_acl_remove_entry(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  u16 acl_id, const struct dpsw_acl_entry_cfg *cfg)
{
	struct dpsw_cmd_acl_entry *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPSW_CMDID_ACL_REMOVE_ENTRY,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_acl_entry *)cmd.params;
	cmd_params->acl_id = cpu_to_le16(acl_id);
	cmd_params->result_if_id = cpu_to_le16(cfg->result.if_id);
	cmd_params->precedence = cpu_to_le32(cfg->precedence);
	cmd_params->key_iova = cpu_to_le64(cfg->key_iova);
	dpsw_set_field(cmd_params->result_action,
		       RESULT_ACTION,
		       cfg->result.action);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_set_reflection_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id)
{
	struct dpsw_cmd_set_reflection_if *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_SET_REFLECTION_IF,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_set_reflection_if *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_add_reflection(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id, const struct dpsw_reflection_cfg *cfg)
{
	struct dpsw_cmd_if_reflection *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_ADD_REFLECTION,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_reflection *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->vlan_id = cpu_to_le16(cfg->vlan_id);
	dpsw_set_field(cmd_params->filter, FILTER, cfg->filter);

	return mc_send_command(mc_io, &cmd);
}

 
int dpsw_if_remove_reflection(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 if_id, const struct dpsw_reflection_cfg *cfg)
{
	struct dpsw_cmd_if_reflection *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPSW_CMDID_IF_REMOVE_REFLECTION,
					  cmd_flags,
					  token);
	cmd_params = (struct dpsw_cmd_if_reflection *)cmd.params;
	cmd_params->if_id = cpu_to_le16(if_id);
	cmd_params->vlan_id = cpu_to_le16(cfg->vlan_id);
	dpsw_set_field(cmd_params->filter, FILTER, cfg->filter);

	return mc_send_command(mc_io, &cmd);
}

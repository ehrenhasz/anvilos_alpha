
 
#include <linux/fsl/mc.h>
#include "dpmac.h"
#include "dpmac-cmd.h"

 
int dpmac_open(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       int dpmac_id,
	       u16 *token)
{
	struct dpmac_cmd_open *cmd_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_OPEN,
					  cmd_flags,
					  0);
	cmd_params = (struct dpmac_cmd_open *)cmd.params;
	cmd_params->dpmac_id = cpu_to_le32(dpmac_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return err;
}

 
int dpmac_close(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_CLOSE, cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpmac_get_attributes(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 struct dpmac_attr *attr)
{
	struct dpmac_rsp_get_attributes *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpmac_rsp_get_attributes *)cmd.params;
	attr->eth_if = rsp_params->eth_if;
	attr->link_type = rsp_params->link_type;
	attr->id = le16_to_cpu(rsp_params->id);
	attr->max_rate = le32_to_cpu(rsp_params->max_rate);

	return 0;
}

 
int dpmac_set_link_state(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 struct dpmac_link_state *link_state)
{
	struct dpmac_cmd_set_link_state *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_SET_LINK_STATE,
					  cmd_flags,
					  token);
	cmd_params = (struct dpmac_cmd_set_link_state *)cmd.params;
	cmd_params->options = cpu_to_le64(link_state->options);
	cmd_params->rate = cpu_to_le32(link_state->rate);
	dpmac_set_field(cmd_params->state, STATE, link_state->up);
	dpmac_set_field(cmd_params->state, STATE_VALID,
			link_state->state_valid);
	cmd_params->supported = cpu_to_le64(link_state->supported);
	cmd_params->advertising = cpu_to_le64(link_state->advertising);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpmac_get_counter(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		      enum dpmac_counter_id id, u64 *value)
{
	struct dpmac_cmd_get_counter *dpmac_cmd;
	struct dpmac_rsp_get_counter *dpmac_rsp;
	struct fsl_mc_command cmd = { 0 };
	int err = 0;

	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_GET_COUNTER,
					  cmd_flags,
					  token);
	dpmac_cmd = (struct dpmac_cmd_get_counter *)cmd.params;
	dpmac_cmd->id = id;

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	dpmac_rsp = (struct dpmac_rsp_get_counter *)cmd.params;
	*value = le64_to_cpu(dpmac_rsp->counter);

	return 0;
}

 
int dpmac_get_api_version(struct fsl_mc_io *mc_io, u32 cmd_flags,
			  u16 *major_ver, u16 *minor_ver)
{
	struct dpmac_rsp_get_api_version *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_GET_API_VERSION,
					  cmd_flags,
					  0);

	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	rsp_params = (struct dpmac_rsp_get_api_version *)cmd.params;
	*major_ver = le16_to_cpu(rsp_params->major);
	*minor_ver = le16_to_cpu(rsp_params->minor);

	return 0;
}

 
int dpmac_set_protocol(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		       enum dpmac_eth_if protocol)
{
	struct dpmac_cmd_set_protocol *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	cmd.header = mc_encode_cmd_header(DPMAC_CMDID_SET_PROTOCOL,
					  cmd_flags, token);
	cmd_params = (struct dpmac_cmd_set_protocol *)cmd.params;
	cmd_params->eth_if = protocol;

	return mc_send_command(mc_io, &cmd);
}

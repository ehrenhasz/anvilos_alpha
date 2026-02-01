
 
#include <linux/kernel.h>
#include <linux/fsl/mc.h>

#include "fsl-mc-private.h"

 
int dpbp_open(struct fsl_mc_io *mc_io,
	      u32 cmd_flags,
	      int dpbp_id,
	      u16 *token)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpbp_cmd_open *cmd_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPBP_CMDID_OPEN,
					  cmd_flags, 0);
	cmd_params = (struct dpbp_cmd_open *)cmd.params;
	cmd_params->dpbp_id = cpu_to_le32(dpbp_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return err;
}
EXPORT_SYMBOL_GPL(dpbp_open);

 
int dpbp_close(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPBP_CMDID_CLOSE, cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpbp_close);

 
int dpbp_enable(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPBP_CMDID_ENABLE, cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpbp_enable);

 
int dpbp_disable(struct fsl_mc_io *mc_io,
		 u32 cmd_flags,
		 u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPBP_CMDID_DISABLE,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpbp_disable);

 
int dpbp_reset(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPBP_CMDID_RESET,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpbp_reset);

 
int dpbp_get_attributes(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			struct dpbp_attr *attr)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpbp_rsp_get_attributes *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPBP_CMDID_GET_ATTR,
					  cmd_flags, token);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpbp_rsp_get_attributes *)cmd.params;
	attr->bpid = le16_to_cpu(rsp_params->bpid);
	attr->id = le32_to_cpu(rsp_params->id);

	return 0;
}
EXPORT_SYMBOL_GPL(dpbp_get_attributes);

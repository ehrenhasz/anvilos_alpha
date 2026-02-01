
 
#include <linux/kernel.h>
#include <linux/fsl/mc.h>

#include "fsl-mc-private.h"

 
int dpmcp_open(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       int dpmcp_id,
	       u16 *token)
{
	struct fsl_mc_command cmd = { 0 };
	struct dpmcp_cmd_open *cmd_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPMCP_CMDID_OPEN,
					  cmd_flags, 0);
	cmd_params = (struct dpmcp_cmd_open *)cmd.params;
	cmd_params->dpmcp_id = cpu_to_le32(dpmcp_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return err;
}

 
int dpmcp_close(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPMCP_CMDID_CLOSE,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dpmcp_reset(struct fsl_mc_io *mc_io,
		u32 cmd_flags,
		u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPMCP_CMDID_RESET,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}

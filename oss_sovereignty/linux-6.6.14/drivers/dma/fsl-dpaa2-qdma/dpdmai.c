


#include <linux/module.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/fsl/mc.h>
#include "dpdmai.h"

struct dpdmai_rsp_get_attributes {
	__le32 id;
	u8 num_of_priorities;
	u8 pad0[3];
	__le16 major;
	__le16 minor;
};

struct dpdmai_cmd_queue {
	__le32 dest_id;
	u8 priority;
	u8 queue;
	u8 dest_type;
	u8 pad;
	__le64 user_ctx;
	union {
		__le32 options;
		__le32 fqid;
	};
};

struct dpdmai_rsp_get_tx_queue {
	__le64 pad;
	__le32 fqid;
};

#define MC_CMD_OP(_cmd, _param, _offset, _width, _type, _arg) \
	((_cmd).params[_param] |= mc_enc((_offset), (_width), _arg))

 
#define DPDMAI_CMD_CREATE(cmd, cfg) \
do { \
	MC_CMD_OP(cmd, 0, 8,  8,  u8,  (cfg)->priorities[0]);\
	MC_CMD_OP(cmd, 0, 16, 8,  u8,  (cfg)->priorities[1]);\
} while (0)

static inline u64 mc_enc(int lsoffset, int width, u64 val)
{
	return (val & MAKE_UMASK64(width)) << lsoffset;
}

 
int dpdmai_open(struct fsl_mc_io *mc_io, u32 cmd_flags,
		int dpdmai_id, u16 *token)
{
	struct fsl_mc_command cmd = { 0 };
	__le64 *cmd_dpdmai_id;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_OPEN,
					  cmd_flags, 0);

	cmd_dpdmai_id = cmd.params;
	*cmd_dpdmai_id = cpu_to_le32(dpdmai_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_open);

 
int dpdmai_close(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_CLOSE,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_close);

 
int dpdmai_create(struct fsl_mc_io *mc_io, u32 cmd_flags,
		  const struct dpdmai_cfg *cfg, u16 *token)
{
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_CREATE,
					  cmd_flags, 0);
	DPDMAI_CMD_CREATE(cmd, cfg);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}

 
int dpdmai_destroy(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_DESTROY,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_destroy);

 
int dpdmai_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_ENABLE,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_enable);

 
int dpdmai_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_DISABLE,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_disable);

 
int dpdmai_reset(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_RESET,
					  cmd_flags, token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_reset);

 
int dpdmai_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags,
			  u16 token, struct dpdmai_attr *attr)
{
	struct dpdmai_rsp_get_attributes *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_GET_ATTR,
					  cmd_flags, token);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dpdmai_rsp_get_attributes *)cmd.params;
	attr->id = le32_to_cpu(rsp_params->id);
	attr->version.major = le16_to_cpu(rsp_params->major);
	attr->version.minor = le16_to_cpu(rsp_params->minor);
	attr->num_of_priorities = rsp_params->num_of_priorities;

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_get_attributes);

 
int dpdmai_set_rx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 priority, const struct dpdmai_rx_queue_cfg *cfg)
{
	struct dpdmai_cmd_queue *cmd_params;
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_SET_RX_QUEUE,
					  cmd_flags, token);

	cmd_params = (struct dpdmai_cmd_queue *)cmd.params;
	cmd_params->dest_id = cpu_to_le32(cfg->dest_cfg.dest_id);
	cmd_params->priority = cfg->dest_cfg.priority;
	cmd_params->queue = priority;
	cmd_params->dest_type = cfg->dest_cfg.dest_type;
	cmd_params->user_ctx = cpu_to_le64(cfg->user_ctx);
	cmd_params->options = cpu_to_le32(cfg->options);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dpdmai_set_rx_queue);

 
int dpdmai_get_rx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 priority, struct dpdmai_rx_queue_attr *attr)
{
	struct dpdmai_cmd_queue *cmd_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_GET_RX_QUEUE,
					  cmd_flags, token);

	cmd_params = (struct dpdmai_cmd_queue *)cmd.params;
	cmd_params->queue = priority;

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	attr->dest_cfg.dest_id = le32_to_cpu(cmd_params->dest_id);
	attr->dest_cfg.priority = cmd_params->priority;
	attr->dest_cfg.dest_type = cmd_params->dest_type;
	attr->user_ctx = le64_to_cpu(cmd_params->user_ctx);
	attr->fqid = le32_to_cpu(cmd_params->fqid);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_get_rx_queue);

 
int dpdmai_get_tx_queue(struct fsl_mc_io *mc_io, u32 cmd_flags,
			u16 token, u8 priority, u32 *fqid)
{
	struct dpdmai_rsp_get_tx_queue *rsp_params;
	struct dpdmai_cmd_queue *cmd_params;
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPDMAI_CMDID_GET_TX_QUEUE,
					  cmd_flags, token);

	cmd_params = (struct dpdmai_cmd_queue *)cmd.params;
	cmd_params->queue = priority;

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 

	rsp_params = (struct dpdmai_rsp_get_tx_queue *)cmd.params;
	*fqid = le32_to_cpu(rsp_params->fqid);

	return 0;
}
EXPORT_SYMBOL_GPL(dpdmai_get_tx_queue);

MODULE_LICENSE("GPL v2");


 
#include <linux/kernel.h>
#include <linux/fsl/mc.h>

#include "fsl-mc-private.h"

 
static u16 dprc_major_ver;
static u16 dprc_minor_ver;

 
int dprc_open(struct fsl_mc_io *mc_io,
	      u32 cmd_flags,
	      int container_id,
	      u16 *token)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_open *cmd_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_OPEN, cmd_flags,
					  0);
	cmd_params = (struct dprc_cmd_open *)cmd.params;
	cmd_params->container_id = cpu_to_le32(container_id);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*token = mc_cmd_hdr_read_token(&cmd);

	return 0;
}
EXPORT_SYMBOL_GPL(dprc_open);

 
int dprc_close(struct fsl_mc_io *mc_io,
	       u32 cmd_flags,
	       u16 token)
{
	struct fsl_mc_command cmd = { 0 };

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_CLOSE, cmd_flags,
					  token);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dprc_close);

 
int dprc_reset_container(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 token,
			 int child_container_id,
			 u32 options)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_reset_container *cmd_params;
	u32 cmdid = DPRC_CMDID_RESET_CONT;
	int err;

	 
	if (!dprc_major_ver && !dprc_minor_ver) {
		err = dprc_get_api_version(mc_io, 0,
				&dprc_major_ver,
				&dprc_minor_ver);
		if (err)
			return err;
	}

	 
	if (dprc_major_ver > 6 || (dprc_major_ver == 6 && dprc_minor_ver >= 5))
		cmdid = DPRC_CMDID_RESET_CONT_V2;

	 
	cmd.header = mc_encode_cmd_header(cmdid, cmd_flags, token);
	cmd_params = (struct dprc_cmd_reset_container *)cmd.params;
	cmd_params->child_container_id = cpu_to_le32(child_container_id);
	cmd_params->options = cpu_to_le32(options);

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dprc_reset_container);

 
int dprc_set_irq(struct fsl_mc_io *mc_io,
		 u32 cmd_flags,
		 u16 token,
		 u8 irq_index,
		 struct dprc_irq_cfg *irq_cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_set_irq *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_SET_IRQ,
					  cmd_flags,
					  token);
	cmd_params = (struct dprc_cmd_set_irq *)cmd.params;
	cmd_params->irq_val = cpu_to_le32(irq_cfg->val);
	cmd_params->irq_index = irq_index;
	cmd_params->irq_addr = cpu_to_le64(irq_cfg->paddr);
	cmd_params->irq_num = cpu_to_le32(irq_cfg->irq_num);

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dprc_set_irq_enable(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			u8 irq_index,
			u8 en)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_set_irq_enable *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_SET_IRQ_ENABLE,
					  cmd_flags, token);
	cmd_params = (struct dprc_cmd_set_irq_enable *)cmd.params;
	cmd_params->enable = en & DPRC_ENABLE;
	cmd_params->irq_index = irq_index;

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dprc_set_irq_mask(struct fsl_mc_io *mc_io,
		      u32 cmd_flags,
		      u16 token,
		      u8 irq_index,
		      u32 mask)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_set_irq_mask *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_SET_IRQ_MASK,
					  cmd_flags, token);
	cmd_params = (struct dprc_cmd_set_irq_mask *)cmd.params;
	cmd_params->mask = cpu_to_le32(mask);
	cmd_params->irq_index = irq_index;

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dprc_get_irq_status(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			u8 irq_index,
			u32 *status)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_get_irq_status *cmd_params;
	struct dprc_rsp_get_irq_status *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_IRQ_STATUS,
					  cmd_flags, token);
	cmd_params = (struct dprc_cmd_get_irq_status *)cmd.params;
	cmd_params->status = cpu_to_le32(*status);
	cmd_params->irq_index = irq_index;

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dprc_rsp_get_irq_status *)cmd.params;
	*status = le32_to_cpu(rsp_params->status);

	return 0;
}

 
int dprc_clear_irq_status(struct fsl_mc_io *mc_io,
			  u32 cmd_flags,
			  u16 token,
			  u8 irq_index,
			  u32 status)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_clear_irq_status *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_CLEAR_IRQ_STATUS,
					  cmd_flags, token);
	cmd_params = (struct dprc_cmd_clear_irq_status *)cmd.params;
	cmd_params->status = cpu_to_le32(status);
	cmd_params->irq_index = irq_index;

	 
	return mc_send_command(mc_io, &cmd);
}

 
int dprc_get_attributes(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			struct dprc_attributes *attr)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_rsp_get_attributes *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_ATTR,
					  cmd_flags,
					  token);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dprc_rsp_get_attributes *)cmd.params;
	attr->container_id = le32_to_cpu(rsp_params->container_id);
	attr->icid = le32_to_cpu(rsp_params->icid);
	attr->options = le32_to_cpu(rsp_params->options);
	attr->portal_id = le32_to_cpu(rsp_params->portal_id);

	return 0;
}

 
int dprc_get_obj_count(struct fsl_mc_io *mc_io,
		       u32 cmd_flags,
		       u16 token,
		       int *obj_count)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_rsp_get_obj_count *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_OBJ_COUNT,
					  cmd_flags, token);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dprc_rsp_get_obj_count *)cmd.params;
	*obj_count = le32_to_cpu(rsp_params->obj_count);

	return 0;
}
EXPORT_SYMBOL_GPL(dprc_get_obj_count);

 
int dprc_get_obj(struct fsl_mc_io *mc_io,
		 u32 cmd_flags,
		 u16 token,
		 int obj_index,
		 struct fsl_mc_obj_desc *obj_desc)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_get_obj *cmd_params;
	struct dprc_rsp_get_obj *rsp_params;
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_OBJ,
					  cmd_flags,
					  token);
	cmd_params = (struct dprc_cmd_get_obj *)cmd.params;
	cmd_params->obj_index = cpu_to_le32(obj_index);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dprc_rsp_get_obj *)cmd.params;
	obj_desc->id = le32_to_cpu(rsp_params->id);
	obj_desc->vendor = le16_to_cpu(rsp_params->vendor);
	obj_desc->irq_count = rsp_params->irq_count;
	obj_desc->region_count = rsp_params->region_count;
	obj_desc->state = le32_to_cpu(rsp_params->state);
	obj_desc->ver_major = le16_to_cpu(rsp_params->version_major);
	obj_desc->ver_minor = le16_to_cpu(rsp_params->version_minor);
	obj_desc->flags = le16_to_cpu(rsp_params->flags);
	strncpy(obj_desc->type, rsp_params->type, 16);
	obj_desc->type[15] = '\0';
	strncpy(obj_desc->label, rsp_params->label, 16);
	obj_desc->label[15] = '\0';
	return 0;
}
EXPORT_SYMBOL_GPL(dprc_get_obj);

 
int dprc_set_obj_irq(struct fsl_mc_io *mc_io,
		     u32 cmd_flags,
		     u16 token,
		     char *obj_type,
		     int obj_id,
		     u8 irq_index,
		     struct dprc_irq_cfg *irq_cfg)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_set_obj_irq *cmd_params;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_SET_OBJ_IRQ,
					  cmd_flags,
					  token);
	cmd_params = (struct dprc_cmd_set_obj_irq *)cmd.params;
	cmd_params->irq_val = cpu_to_le32(irq_cfg->val);
	cmd_params->irq_index = irq_index;
	cmd_params->irq_addr = cpu_to_le64(irq_cfg->paddr);
	cmd_params->irq_num = cpu_to_le32(irq_cfg->irq_num);
	cmd_params->obj_id = cpu_to_le32(obj_id);
	strncpy(cmd_params->obj_type, obj_type, 16);
	cmd_params->obj_type[15] = '\0';

	 
	return mc_send_command(mc_io, &cmd);
}
EXPORT_SYMBOL_GPL(dprc_set_obj_irq);

 
int dprc_get_obj_region(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			char *obj_type,
			int obj_id,
			u8 region_index,
			struct dprc_region_desc *region_desc)
{
	struct fsl_mc_command cmd = { 0 };
	struct dprc_cmd_get_obj_region *cmd_params;
	struct dprc_rsp_get_obj_region *rsp_params;
	int err;

     
	if (!dprc_major_ver && !dprc_minor_ver) {
		err = dprc_get_api_version(mc_io, 0,
				      &dprc_major_ver,
				      &dprc_minor_ver);
		if (err)
			return err;
	}

	if (dprc_major_ver > 6 || (dprc_major_ver == 6 && dprc_minor_ver >= 6)) {
		 

		cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_OBJ_REG_V3,
						  cmd_flags, token);

	} else if (dprc_major_ver == 6 && dprc_minor_ver >= 3) {
		 
		cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_OBJ_REG_V2,
						  cmd_flags, token);
	} else {
		cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_OBJ_REG,
						  cmd_flags, token);
	}

	cmd_params = (struct dprc_cmd_get_obj_region *)cmd.params;
	cmd_params->obj_id = cpu_to_le32(obj_id);
	cmd_params->region_index = region_index;
	strncpy(cmd_params->obj_type, obj_type, 16);
	cmd_params->obj_type[15] = '\0';

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	rsp_params = (struct dprc_rsp_get_obj_region *)cmd.params;
	region_desc->base_offset = le64_to_cpu(rsp_params->base_offset);
	region_desc->size = le32_to_cpu(rsp_params->size);
	region_desc->type = rsp_params->type;
	region_desc->flags = le32_to_cpu(rsp_params->flags);
	if (dprc_major_ver > 6 || (dprc_major_ver == 6 && dprc_minor_ver >= 3))
		region_desc->base_address = le64_to_cpu(rsp_params->base_addr);
	else
		region_desc->base_address = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(dprc_get_obj_region);

 
int dprc_get_api_version(struct fsl_mc_io *mc_io,
			 u32 cmd_flags,
			 u16 *major_ver,
			 u16 *minor_ver)
{
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_API_VERSION,
					  cmd_flags, 0);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	mc_cmd_read_api_version(&cmd, major_ver, minor_ver);

	return 0;
}

 
int dprc_get_container_id(struct fsl_mc_io *mc_io,
			  u32 cmd_flags,
			  int *container_id)
{
	struct fsl_mc_command cmd = { 0 };
	int err;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_CONT_ID,
					  cmd_flags,
					  0);

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return err;

	 
	*container_id = (int)mc_cmd_read_object_id(&cmd);

	return 0;
}

 
int dprc_get_connection(struct fsl_mc_io *mc_io,
			u32 cmd_flags,
			u16 token,
			const struct dprc_endpoint *endpoint1,
			struct dprc_endpoint *endpoint2,
			int *state)
{
	struct dprc_cmd_get_connection *cmd_params;
	struct dprc_rsp_get_connection *rsp_params;
	struct fsl_mc_command cmd = { 0 };
	int err, i;

	 
	cmd.header = mc_encode_cmd_header(DPRC_CMDID_GET_CONNECTION,
					  cmd_flags,
					  token);
	cmd_params = (struct dprc_cmd_get_connection *)cmd.params;
	cmd_params->ep1_id = cpu_to_le32(endpoint1->id);
	cmd_params->ep1_interface_id = cpu_to_le16(endpoint1->if_id);
	for (i = 0; i < 16; i++)
		cmd_params->ep1_type[i] = endpoint1->type[i];

	 
	err = mc_send_command(mc_io, &cmd);
	if (err)
		return -ENOTCONN;

	 
	rsp_params = (struct dprc_rsp_get_connection *)cmd.params;
	endpoint2->id = le32_to_cpu(rsp_params->ep2_id);
	endpoint2->if_id = le16_to_cpu(rsp_params->ep2_interface_id);
	*state = le32_to_cpu(rsp_params->state);
	for (i = 0; i < 16; i++)
		endpoint2->type[i] = rsp_params->ep2_type[i];

	return 0;
}

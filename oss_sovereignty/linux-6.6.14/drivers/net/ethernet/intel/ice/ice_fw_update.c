
 

#include <asm/unaligned.h>
#include <linux/uuid.h>
#include <linux/crc32.h>
#include <linux/pldmfw.h>
#include "ice.h"
#include "ice_fw_update.h"

struct ice_fwu_priv {
	struct pldmfw context;

	struct ice_pf *pf;
	struct netlink_ext_ack *extack;

	 
	u8 activate_flags;

	 
	u8 reset_level;

	 
	u8 emp_reset_available;
};

 
static int
ice_send_package_data(struct pldmfw *context, const u8 *data, u16 length)
{
	struct ice_fwu_priv *priv = container_of(context, struct ice_fwu_priv, context);
	struct netlink_ext_ack *extack = priv->extack;
	struct device *dev = context->dev;
	struct ice_pf *pf = priv->pf;
	struct ice_hw *hw = &pf->hw;
	u8 *package_data;
	int status;

	dev_dbg(dev, "Sending PLDM record package data to firmware\n");

	package_data = kmemdup(data, length, GFP_KERNEL);
	if (!package_data)
		return -ENOMEM;

	status = ice_nvm_set_pkg_data(hw, false, package_data, length, NULL);

	kfree(package_data);

	if (status) {
		dev_err(dev, "Failed to send record package data to firmware, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to record package data to firmware");
		return -EIO;
	}

	return 0;
}

 
static int
ice_check_component_response(struct ice_pf *pf, u16 id, u8 response, u8 code,
			     struct netlink_ext_ack *extack)
{
	struct device *dev = ice_pf_to_dev(pf);
	const char *component;

	switch (id) {
	case NVM_COMP_ID_OROM:
		component = "fw.undi";
		break;
	case NVM_COMP_ID_NVM:
		component = "fw.mgmt";
		break;
	case NVM_COMP_ID_NETLIST:
		component = "fw.netlist";
		break;
	default:
		WARN(1, "Unexpected unknown component identifier 0x%02x", id);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: firmware response 0x%x, code 0x%x\n",
		component, response, code);

	switch (response) {
	case ICE_AQ_NVM_PASS_COMP_CAN_BE_UPDATED:
		 
		return 0;
	case ICE_AQ_NVM_PASS_COMP_CAN_MAY_BE_UPDATEABLE:
		dev_warn(dev, "firmware recommends not updating %s, as it may result in a downgrade. continuing anyways\n", component);
		return 0;
	case ICE_AQ_NVM_PASS_COMP_CAN_NOT_BE_UPDATED:
		dev_info(dev, "firmware has rejected updating %s\n", component);
		break;
	}

	switch (code) {
	case ICE_AQ_NVM_PASS_COMP_STAMP_IDENTICAL_CODE:
		dev_err(dev, "Component comparison stamp for %s is identical to the running image\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component comparison stamp is identical to running image");
		break;
	case ICE_AQ_NVM_PASS_COMP_STAMP_LOWER:
		dev_err(dev, "Component comparison stamp for %s is lower than the running image\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component comparison stamp is lower than running image");
		break;
	case ICE_AQ_NVM_PASS_COMP_INVALID_STAMP_CODE:
		dev_err(dev, "Component comparison stamp for %s is invalid\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component comparison stamp is invalid");
		break;
	case ICE_AQ_NVM_PASS_COMP_CONFLICT_CODE:
		dev_err(dev, "%s conflicts with a previous component table\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component table conflict occurred");
		break;
	case ICE_AQ_NVM_PASS_COMP_PRE_REQ_NOT_MET_CODE:
		dev_err(dev, "Pre-requisites for component %s have not been met\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component pre-requisites not met");
		break;
	case ICE_AQ_NVM_PASS_COMP_NOT_SUPPORTED_CODE:
		dev_err(dev, "%s is not a supported component\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component not supported");
		break;
	case ICE_AQ_NVM_PASS_COMP_CANNOT_DOWNGRADE_CODE:
		dev_err(dev, "Security restrictions prevent %s from being downgraded\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component cannot be downgraded");
		break;
	case ICE_AQ_NVM_PASS_COMP_INCOMPLETE_IMAGE_CODE:
		dev_err(dev, "Received an incomplete component image for %s\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Incomplete component image");
		break;
	case ICE_AQ_NVM_PASS_COMP_VER_STR_IDENTICAL_CODE:
		dev_err(dev, "Component version for %s is identical to the running image\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component version is identical to running image");
		break;
	case ICE_AQ_NVM_PASS_COMP_VER_STR_LOWER_CODE:
		dev_err(dev, "Component version for %s is lower than the running image\n",
			component);
		NL_SET_ERR_MSG_MOD(extack, "Component version is lower than the running image");
		break;
	default:
		dev_err(dev, "Unexpected response code 0x02%x for %s\n",
			code, component);
		NL_SET_ERR_MSG_MOD(extack, "Received unexpected response code from firmware");
		break;
	}

	return -ECANCELED;
}

 
static int
ice_send_component_table(struct pldmfw *context, struct pldmfw_component *component,
			 u8 transfer_flag)
{
	struct ice_fwu_priv *priv = container_of(context, struct ice_fwu_priv, context);
	struct netlink_ext_ack *extack = priv->extack;
	struct ice_aqc_nvm_comp_tbl *comp_tbl;
	u8 comp_response, comp_response_code;
	struct device *dev = context->dev;
	struct ice_pf *pf = priv->pf;
	struct ice_hw *hw = &pf->hw;
	size_t length;
	int status;

	switch (component->identifier) {
	case NVM_COMP_ID_OROM:
	case NVM_COMP_ID_NVM:
	case NVM_COMP_ID_NETLIST:
		break;
	default:
		dev_err(dev, "Unable to update due to a firmware component with unknown ID %u\n",
			component->identifier);
		NL_SET_ERR_MSG_MOD(extack, "Unable to update due to unknown firmware component");
		return -EOPNOTSUPP;
	}

	length = struct_size(comp_tbl, cvs, component->version_len);
	comp_tbl = kzalloc(length, GFP_KERNEL);
	if (!comp_tbl)
		return -ENOMEM;

	comp_tbl->comp_class = cpu_to_le16(component->classification);
	comp_tbl->comp_id = cpu_to_le16(component->identifier);
	comp_tbl->comp_class_idx = FWU_COMP_CLASS_IDX_NOT_USE;
	comp_tbl->comp_cmp_stamp = cpu_to_le32(component->comparison_stamp);
	comp_tbl->cvs_type = component->version_type;
	comp_tbl->cvs_len = component->version_len;
	memcpy(comp_tbl->cvs, component->version_string, component->version_len);

	dev_dbg(dev, "Sending component table to firmware:\n");

	status = ice_nvm_pass_component_tbl(hw, (u8 *)comp_tbl, length,
					    transfer_flag, &comp_response,
					    &comp_response_code, NULL);

	kfree(comp_tbl);

	if (status) {
		dev_err(dev, "Failed to transfer component table to firmware, err %d aq_err %s\n",
			status, ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to transfer component table to firmware");
		return -EIO;
	}

	return ice_check_component_response(pf, component->identifier, comp_response,
					    comp_response_code, extack);
}

 
static int
ice_write_one_nvm_block(struct ice_pf *pf, u16 module, u32 offset,
			u16 block_size, u8 *block, bool last_cmd,
			u8 *reset_level, struct netlink_ext_ack *extack)
{
	u16 completion_module, completion_retval;
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_aq_task task = {};
	struct ice_hw *hw = &pf->hw;
	struct ice_aq_desc *desc;
	u32 completion_offset;
	int err;

	dev_dbg(dev, "Writing block of %u bytes for module 0x%02x at offset %u\n",
		block_size, module, offset);

	ice_aq_prep_for_event(pf, &task, ice_aqc_opc_nvm_write);

	err = ice_aq_update_nvm(hw, module, offset, block_size, block,
				last_cmd, 0, NULL);
	if (err) {
		dev_err(dev, "Failed to flash module 0x%02x with block of size %u at offset %u, err %d aq_err %s\n",
			module, block_size, offset, err,
			ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to program flash module");
		return -EIO;
	}

	 
	err = ice_aq_wait_for_event(pf, &task, 15 * HZ);
	if (err) {
		dev_err(dev, "Timed out while trying to flash module 0x%02x with block of size %u at offset %u, err %d\n",
			module, block_size, offset, err);
		NL_SET_ERR_MSG_MOD(extack, "Timed out waiting for firmware");
		return -EIO;
	}

	desc = &task.event.desc;
	completion_module = le16_to_cpu(desc->params.nvm.module_typeid);
	completion_retval = le16_to_cpu(desc->retval);

	completion_offset = le16_to_cpu(desc->params.nvm.offset_low);
	completion_offset |= desc->params.nvm.offset_high << 16;

	if (completion_module != module) {
		dev_err(dev, "Unexpected module_typeid in write completion: got 0x%x, expected 0x%x\n",
			completion_module, module);
		NL_SET_ERR_MSG_MOD(extack, "Unexpected firmware response");
		return -EIO;
	}

	if (completion_offset != offset) {
		dev_err(dev, "Unexpected offset in write completion: got %u, expected %u\n",
			completion_offset, offset);
		NL_SET_ERR_MSG_MOD(extack, "Unexpected firmware response");
		return -EIO;
	}

	if (completion_retval) {
		dev_err(dev, "Firmware failed to flash module 0x%02x with block of size %u at offset %u, err %s\n",
			module, block_size, offset,
			ice_aq_str((enum ice_aq_err)completion_retval));
		NL_SET_ERR_MSG_MOD(extack, "Firmware failed to program flash module");
		return -EIO;
	}

	 
	if (reset_level && last_cmd && module == ICE_SR_1ST_NVM_BANK_PTR) {
		if (hw->dev_caps.common_cap.pcie_reset_avoidance) {
			*reset_level = desc->params.nvm.cmd_flags &
				       ICE_AQC_NVM_RESET_LVL_M;
			dev_dbg(dev, "Firmware reported required reset level as %u\n",
				*reset_level);
		} else {
			*reset_level = ICE_AQC_NVM_POR_FLAG;
			dev_dbg(dev, "Firmware doesn't support indicating required reset level. Assuming a power cycle is required\n");
		}
	}

	return 0;
}

 
static int
ice_write_nvm_module(struct ice_pf *pf, u16 module, const char *component,
		     const u8 *image, u32 length, u8 *reset_level,
		     struct netlink_ext_ack *extack)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct devlink *devlink;
	u32 offset = 0;
	bool last_cmd;
	u8 *block;
	int err;

	dev_dbg(dev, "Beginning write of flash component '%s', module 0x%02x\n", component, module);

	devlink = priv_to_devlink(pf);

	devlink_flash_update_status_notify(devlink, "Flashing",
					   component, 0, length);

	block = kzalloc(ICE_AQ_MAX_BUF_LEN, GFP_KERNEL);
	if (!block)
		return -ENOMEM;

	do {
		u32 block_size;

		block_size = min_t(u32, ICE_AQ_MAX_BUF_LEN, length - offset);
		last_cmd = !(offset + block_size < length);

		 
		memcpy(block, image + offset, block_size);

		err = ice_write_one_nvm_block(pf, module, offset, block_size,
					      block, last_cmd, reset_level,
					      extack);
		if (err)
			break;

		offset += block_size;

		devlink_flash_update_status_notify(devlink, "Flashing",
						   component, offset, length);
	} while (!last_cmd);

	dev_dbg(dev, "Completed write of flash component '%s', module 0x%02x\n", component, module);

	if (err)
		devlink_flash_update_status_notify(devlink, "Flashing failed",
						   component, length, length);
	else
		devlink_flash_update_status_notify(devlink, "Flashing done",
						   component, length, length);

	kfree(block);
	return err;
}

 
#define ICE_FW_ERASE_TIMEOUT 300

 
static int
ice_erase_nvm_module(struct ice_pf *pf, u16 module, const char *component,
		     struct netlink_ext_ack *extack)
{
	u16 completion_module, completion_retval;
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_aq_task task = {};
	struct ice_hw *hw = &pf->hw;
	struct ice_aq_desc *desc;
	struct devlink *devlink;
	int err;

	dev_dbg(dev, "Beginning erase of flash component '%s', module 0x%02x\n", component, module);

	devlink = priv_to_devlink(pf);

	devlink_flash_update_timeout_notify(devlink, "Erasing", component, ICE_FW_ERASE_TIMEOUT);

	ice_aq_prep_for_event(pf, &task, ice_aqc_opc_nvm_erase);

	err = ice_aq_erase_nvm(hw, module, NULL);
	if (err) {
		dev_err(dev, "Failed to erase %s (module 0x%02x), err %d aq_err %s\n",
			component, module, err,
			ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to erase flash module");
		err = -EIO;
		goto out_notify_devlink;
	}

	err = ice_aq_wait_for_event(pf, &task, ICE_FW_ERASE_TIMEOUT * HZ);
	if (err) {
		dev_err(dev, "Timed out waiting for firmware to respond with erase completion for %s (module 0x%02x), err %d\n",
			component, module, err);
		NL_SET_ERR_MSG_MOD(extack, "Timed out waiting for firmware");
		goto out_notify_devlink;
	}

	desc = &task.event.desc;
	completion_module = le16_to_cpu(desc->params.nvm.module_typeid);
	completion_retval = le16_to_cpu(desc->retval);

	if (completion_module != module) {
		dev_err(dev, "Unexpected module_typeid in erase completion for %s: got 0x%x, expected 0x%x\n",
			component, completion_module, module);
		NL_SET_ERR_MSG_MOD(extack, "Unexpected firmware response");
		err = -EIO;
		goto out_notify_devlink;
	}

	if (completion_retval) {
		dev_err(dev, "Firmware failed to erase %s (module 0x02%x), aq_err %s\n",
			component, module,
			ice_aq_str((enum ice_aq_err)completion_retval));
		NL_SET_ERR_MSG_MOD(extack, "Firmware failed to erase flash");
		err = -EIO;
		goto out_notify_devlink;
	}

	dev_dbg(dev, "Completed erase of flash component '%s', module 0x%02x\n", component, module);

out_notify_devlink:
	if (err)
		devlink_flash_update_status_notify(devlink, "Erasing failed",
						   component, 0, 0);
	else
		devlink_flash_update_status_notify(devlink, "Erasing done",
						   component, 0, 0);

	return err;
}

 
static int
ice_switch_flash_banks(struct ice_pf *pf, u8 activate_flags,
		       u8 *emp_reset_available, struct netlink_ext_ack *extack)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_aq_task task = {};
	struct ice_hw *hw = &pf->hw;
	u16 completion_retval;
	u8 response_flags;
	int err;

	ice_aq_prep_for_event(pf, &task, ice_aqc_opc_nvm_write_activate);

	err = ice_nvm_write_activate(hw, activate_flags, &response_flags);
	if (err) {
		dev_err(dev, "Failed to switch active flash banks, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to switch active flash banks");
		return -EIO;
	}

	 
	if (emp_reset_available) {
		if (hw->dev_caps.common_cap.reset_restrict_support) {
			*emp_reset_available = response_flags & ICE_AQC_NVM_EMPR_ENA;
			dev_dbg(dev, "Firmware indicated that EMP reset is %s\n",
				*emp_reset_available ?
				"available" : "not available");
		} else {
			*emp_reset_available = ICE_AQC_NVM_EMPR_ENA;
			dev_dbg(dev, "Firmware does not support restricting EMP reset availability\n");
		}
	}

	err = ice_aq_wait_for_event(pf, &task, 30 * HZ);
	if (err) {
		dev_err(dev, "Timed out waiting for firmware to switch active flash banks, err %d\n",
			err);
		NL_SET_ERR_MSG_MOD(extack, "Timed out waiting for firmware");
		return err;
	}

	completion_retval = le16_to_cpu(task.event.desc.retval);
	if (completion_retval) {
		dev_err(dev, "Firmware failed to switch active flash banks aq_err %s\n",
			ice_aq_str((enum ice_aq_err)completion_retval));
		NL_SET_ERR_MSG_MOD(extack, "Firmware failed to switch active flash banks");
		return -EIO;
	}

	return 0;
}

 
static int
ice_flash_component(struct pldmfw *context, struct pldmfw_component *component)
{
	struct ice_fwu_priv *priv = container_of(context, struct ice_fwu_priv, context);
	struct netlink_ext_ack *extack = priv->extack;
	struct ice_pf *pf = priv->pf;
	const char *name;
	u8 *reset_level;
	u16 module;
	u8 flag;
	int err;

	switch (component->identifier) {
	case NVM_COMP_ID_OROM:
		module = ICE_SR_1ST_OROM_BANK_PTR;
		flag = ICE_AQC_NVM_ACTIV_SEL_OROM;
		reset_level = NULL;
		name = "fw.undi";
		break;
	case NVM_COMP_ID_NVM:
		module = ICE_SR_1ST_NVM_BANK_PTR;
		flag = ICE_AQC_NVM_ACTIV_SEL_NVM;
		reset_level = &priv->reset_level;
		name = "fw.mgmt";
		break;
	case NVM_COMP_ID_NETLIST:
		module = ICE_SR_NETLIST_BANK_PTR;
		flag = ICE_AQC_NVM_ACTIV_SEL_NETLIST;
		reset_level = NULL;
		name = "fw.netlist";
		break;
	default:
		 
		WARN(1, "Unexpected unknown component identifier 0x%02x",
		     component->identifier);
		return -EINVAL;
	}

	 
	priv->activate_flags |= flag;

	err = ice_erase_nvm_module(pf, module, name, extack);
	if (err)
		return err;

	return ice_write_nvm_module(pf, module, name, component->component_data,
				    component->component_size, reset_level,
				    extack);
}

 
static int ice_finalize_update(struct pldmfw *context)
{
	struct ice_fwu_priv *priv = container_of(context, struct ice_fwu_priv, context);
	struct netlink_ext_ack *extack = priv->extack;
	struct ice_pf *pf = priv->pf;
	struct devlink *devlink;
	int err;

	 
	err = ice_switch_flash_banks(pf, priv->activate_flags,
				     &priv->emp_reset_available, extack);
	if (err)
		return err;

	devlink = priv_to_devlink(pf);

	 
	if (priv->reset_level == ICE_AQC_NVM_EMPR_FLAG &&
	    !priv->emp_reset_available) {
		dev_dbg(ice_pf_to_dev(pf), "Firmware indicated EMP reset as sufficient, but EMP reset is disabled\n");
		priv->reset_level = ICE_AQC_NVM_PERST_FLAG;
	}

	switch (priv->reset_level) {
	case ICE_AQC_NVM_EMPR_FLAG:
		devlink_flash_update_status_notify(devlink,
						   "Activate new firmware by devlink reload",
						   NULL, 0, 0);
		break;
	case ICE_AQC_NVM_PERST_FLAG:
		devlink_flash_update_status_notify(devlink,
						   "Activate new firmware by rebooting the system",
						   NULL, 0, 0);
		break;
	case ICE_AQC_NVM_POR_FLAG:
	default:
		devlink_flash_update_status_notify(devlink,
						   "Activate new firmware by power cycling the system",
						   NULL, 0, 0);
		break;
	}

	pf->fw_emp_reset_disabled = !priv->emp_reset_available;

	return 0;
}

struct ice_pldm_pci_record_id {
	u32 vendor;
	u32 device;
	u32 subsystem_vendor;
	u32 subsystem_device;
};

 
static bool
ice_op_pci_match_record(struct pldmfw *context, struct pldmfw_record *record)
{
	struct pci_dev *pdev = to_pci_dev(context->dev);
	struct ice_pldm_pci_record_id id = {
		.vendor = PCI_ANY_ID,
		.device = PCI_ANY_ID,
		.subsystem_vendor = PCI_ANY_ID,
		.subsystem_device = PCI_ANY_ID,
	};
	struct pldmfw_desc_tlv *desc;

	list_for_each_entry(desc, &record->descs, entry) {
		u16 value;
		int *ptr;

		switch (desc->type) {
		case PLDM_DESC_ID_PCI_VENDOR_ID:
			ptr = &id.vendor;
			break;
		case PLDM_DESC_ID_PCI_DEVICE_ID:
			ptr = &id.device;
			break;
		case PLDM_DESC_ID_PCI_SUBVENDOR_ID:
			ptr = &id.subsystem_vendor;
			break;
		case PLDM_DESC_ID_PCI_SUBDEV_ID:
			ptr = &id.subsystem_device;
			break;
		default:
			 
			continue;
		}

		value = get_unaligned_le16(desc->data);
		 
		if (value)
			*ptr = value;
		else
			*ptr = PCI_ANY_ID;
	}

	 
	if ((id.vendor == PCI_ANY_ID || id.vendor == pdev->vendor) &&
	    (id.device == PCI_ANY_ID || id.device == pdev->device ||
	    id.device == ICE_DEV_ID_E822_SI_DFLT) &&
	    (id.subsystem_vendor == PCI_ANY_ID ||
	    id.subsystem_vendor == pdev->subsystem_vendor) &&
	    (id.subsystem_device == PCI_ANY_ID ||
	    id.subsystem_device == pdev->subsystem_device))
		return true;

	return false;
}

static const struct pldmfw_ops ice_fwu_ops_e810 = {
	.match_record = &pldmfw_op_pci_match_record,
	.send_package_data = &ice_send_package_data,
	.send_component_table = &ice_send_component_table,
	.flash_component = &ice_flash_component,
	.finalize_update = &ice_finalize_update,
};

static const struct pldmfw_ops ice_fwu_ops_e822 = {
	.match_record = &ice_op_pci_match_record,
	.send_package_data = &ice_send_package_data,
	.send_component_table = &ice_send_component_table,
	.flash_component = &ice_flash_component,
	.finalize_update = &ice_finalize_update,
};

 
int ice_get_pending_updates(struct ice_pf *pf, u8 *pending,
			    struct netlink_ext_ack *extack)
{
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw_dev_caps *dev_caps;
	struct ice_hw *hw = &pf->hw;
	int err;

	dev_caps = kzalloc(sizeof(*dev_caps), GFP_KERNEL);
	if (!dev_caps)
		return -ENOMEM;

	 
	err = ice_discover_dev_caps(hw, dev_caps);
	if (err) {
		NL_SET_ERR_MSG_MOD(extack, "Unable to read device capabilities");
		kfree(dev_caps);
		return err;
	}

	*pending = 0;

	if (dev_caps->common_cap.nvm_update_pending_nvm) {
		dev_info(dev, "The fw.mgmt flash component has a pending update\n");
		*pending |= ICE_AQC_NVM_ACTIV_SEL_NVM;
	}

	if (dev_caps->common_cap.nvm_update_pending_orom) {
		dev_info(dev, "The fw.undi flash component has a pending update\n");
		*pending |= ICE_AQC_NVM_ACTIV_SEL_OROM;
	}

	if (dev_caps->common_cap.nvm_update_pending_netlist) {
		dev_info(dev, "The fw.netlist flash component has a pending update\n");
		*pending |= ICE_AQC_NVM_ACTIV_SEL_NETLIST;
	}

	kfree(dev_caps);

	return 0;
}

 
static int
ice_cancel_pending_update(struct ice_pf *pf, const char *component,
			  struct netlink_ext_ack *extack)
{
	struct devlink *devlink = priv_to_devlink(pf);
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	u8 pending;
	int err;

	err = ice_get_pending_updates(pf, &pending, extack);
	if (err)
		return err;

	 
	if (component) {
		if (strcmp(component, "fw.mgmt") == 0)
			pending &= ICE_AQC_NVM_ACTIV_SEL_NVM;
		else if (strcmp(component, "fw.undi") == 0)
			pending &= ICE_AQC_NVM_ACTIV_SEL_OROM;
		else if (strcmp(component, "fw.netlist") == 0)
			pending &= ICE_AQC_NVM_ACTIV_SEL_NETLIST;
		else
			WARN(1, "Unexpected flash component %s", component);
	}

	 
	if (!pending)
		return 0;

	 
	devlink_flash_update_status_notify(devlink,
					   "Canceling previous pending update",
					   component, 0, 0);

	err = ice_acquire_nvm(hw, ICE_RES_WRITE);
	if (err) {
		dev_err(dev, "Failed to acquire device flash lock, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to acquire device flash lock");
		return err;
	}

	pending |= ICE_AQC_NVM_REVERT_LAST_ACTIV;
	err = ice_switch_flash_banks(pf, pending, NULL, extack);

	ice_release_nvm(hw);

	 
	pf->fw_emp_reset_disabled = false;

	return err;
}

 
int ice_devlink_flash_update(struct devlink *devlink,
			     struct devlink_flash_update_params *params,
			     struct netlink_ext_ack *extack)
{
	struct ice_pf *pf = devlink_priv(devlink);
	struct device *dev = ice_pf_to_dev(pf);
	struct ice_hw *hw = &pf->hw;
	struct ice_fwu_priv priv;
	u8 preservation;
	int err;

	if (!params->overwrite_mask) {
		 
		preservation = ICE_AQC_NVM_PRESERVE_ALL;
	} else if (params->overwrite_mask == DEVLINK_FLASH_OVERWRITE_SETTINGS) {
		 
		preservation = ICE_AQC_NVM_PRESERVE_SELECTED;
	} else if (params->overwrite_mask == (DEVLINK_FLASH_OVERWRITE_SETTINGS |
					      DEVLINK_FLASH_OVERWRITE_IDENTIFIERS)) {
		 
		preservation = ICE_AQC_NVM_NO_PRESERVATION;
	} else {
		NL_SET_ERR_MSG_MOD(extack, "Requested overwrite mask is not supported");
		return -EOPNOTSUPP;
	}

	if (!hw->dev_caps.common_cap.nvm_unified_update) {
		NL_SET_ERR_MSG_MOD(extack, "Current firmware does not support unified update");
		return -EOPNOTSUPP;
	}

	memset(&priv, 0, sizeof(priv));

	 
	if (hw->mac_type == ICE_MAC_GENERIC)
		priv.context.ops = &ice_fwu_ops_e822;
	else
		priv.context.ops = &ice_fwu_ops_e810;
	priv.context.dev = dev;
	priv.extack = extack;
	priv.pf = pf;
	priv.activate_flags = preservation;

	devlink_flash_update_status_notify(devlink, "Preparing to flash", NULL, 0, 0);

	err = ice_cancel_pending_update(pf, NULL, extack);
	if (err)
		return err;

	err = ice_acquire_nvm(hw, ICE_RES_WRITE);
	if (err) {
		dev_err(dev, "Failed to acquire device flash lock, err %d aq_err %s\n",
			err, ice_aq_str(hw->adminq.sq_last_status));
		NL_SET_ERR_MSG_MOD(extack, "Failed to acquire device flash lock");
		return err;
	}

	err = pldmfw_flash_image(&priv.context, params->fw);
	if (err == -ENOENT) {
		dev_err(dev, "Firmware image has no record matching this device\n");
		NL_SET_ERR_MSG_MOD(extack, "Firmware image has no record matching this device");
	} else if (err) {
		 
		dev_err(dev, "Failed to flash PLDM image, err %d", err);
	}

	ice_release_nvm(hw);

	return err;
}

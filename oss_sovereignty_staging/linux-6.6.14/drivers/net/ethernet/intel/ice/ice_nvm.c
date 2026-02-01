
 

#include <linux/vmalloc.h>

#include "ice_common.h"

 
static int
ice_aq_read_nvm(struct ice_hw *hw, u16 module_typeid, u32 offset, u16 length,
		void *data, bool last_command, bool read_shadow_ram,
		struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	struct ice_aqc_nvm *cmd;

	cmd = &desc.params.nvm;

	if (offset > ICE_AQC_NVM_MAX_OFFSET)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_read);

	if (!read_shadow_ram && module_typeid == ICE_AQC_NVM_START_POINT)
		cmd->cmd_flags |= ICE_AQC_NVM_FLASH_ONLY;

	 
	if (last_command)
		cmd->cmd_flags |= ICE_AQC_NVM_LAST_CMD;
	cmd->module_typeid = cpu_to_le16(module_typeid);
	cmd->offset_low = cpu_to_le16(offset & 0xFFFF);
	cmd->offset_high = (offset >> 16) & 0xFF;
	cmd->length = cpu_to_le16(length);

	return ice_aq_send_cmd(hw, &desc, data, length, cd);
}

 
int
ice_read_flat_nvm(struct ice_hw *hw, u32 offset, u32 *length, u8 *data,
		  bool read_shadow_ram)
{
	u32 inlen = *length;
	u32 bytes_read = 0;
	bool last_cmd;
	int status;

	*length = 0;

	 
	if (read_shadow_ram && ((offset + inlen) > (hw->flash.sr_words * 2u))) {
		ice_debug(hw, ICE_DBG_NVM, "NVM error: requested offset is beyond Shadow RAM limit\n");
		return -EINVAL;
	}

	do {
		u32 read_size, sector_offset;

		 
		sector_offset = offset % ICE_AQ_MAX_BUF_LEN;
		read_size = min_t(u32, ICE_AQ_MAX_BUF_LEN - sector_offset,
				  inlen - bytes_read);

		last_cmd = !(bytes_read + read_size < inlen);

		status = ice_aq_read_nvm(hw, ICE_AQC_NVM_START_POINT,
					 offset, read_size,
					 data + bytes_read, last_cmd,
					 read_shadow_ram, NULL);
		if (status)
			break;

		bytes_read += read_size;
		offset += read_size;
	} while (!last_cmd);

	*length = bytes_read;
	return status;
}

 
int
ice_aq_update_nvm(struct ice_hw *hw, u16 module_typeid, u32 offset,
		  u16 length, void *data, bool last_command, u8 command_flags,
		  struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	struct ice_aqc_nvm *cmd;

	cmd = &desc.params.nvm;

	 
	if (offset & 0xFF000000)
		return -EINVAL;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_write);

	cmd->cmd_flags |= command_flags;

	 
	if (last_command)
		cmd->cmd_flags |= ICE_AQC_NVM_LAST_CMD;
	cmd->module_typeid = cpu_to_le16(module_typeid);
	cmd->offset_low = cpu_to_le16(offset & 0xFFFF);
	cmd->offset_high = (offset >> 16) & 0xFF;
	cmd->length = cpu_to_le16(length);

	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	return ice_aq_send_cmd(hw, &desc, data, length, cd);
}

 
int ice_aq_erase_nvm(struct ice_hw *hw, u16 module_typeid, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	struct ice_aqc_nvm *cmd;

	cmd = &desc.params.nvm;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_erase);

	cmd->module_typeid = cpu_to_le16(module_typeid);
	cmd->length = cpu_to_le16(ICE_AQC_NVM_ERASE_LEN);
	cmd->offset_low = 0;
	cmd->offset_high = 0;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

 
static int ice_read_sr_word_aq(struct ice_hw *hw, u16 offset, u16 *data)
{
	u32 bytes = sizeof(u16);
	__le16 data_local;
	int status;

	 
	status = ice_read_flat_nvm(hw, offset * sizeof(u16), &bytes,
				   (__force u8 *)&data_local, true);
	if (status)
		return status;

	*data = le16_to_cpu(data_local);
	return 0;
}

 
int ice_acquire_nvm(struct ice_hw *hw, enum ice_aq_res_access_type access)
{
	if (hw->flash.blank_nvm_mode)
		return 0;

	return ice_acquire_res(hw, ICE_NVM_RES_ID, access, ICE_NVM_TIMEOUT);
}

 
void ice_release_nvm(struct ice_hw *hw)
{
	if (hw->flash.blank_nvm_mode)
		return;

	ice_release_res(hw, ICE_NVM_RES_ID);
}

 
static u32 ice_get_flash_bank_offset(struct ice_hw *hw, enum ice_bank_select bank, u16 module)
{
	struct ice_bank_info *banks = &hw->flash.banks;
	enum ice_flash_bank active_bank;
	bool second_bank_active;
	u32 offset, size;

	switch (module) {
	case ICE_SR_1ST_NVM_BANK_PTR:
		offset = banks->nvm_ptr;
		size = banks->nvm_size;
		active_bank = banks->nvm_bank;
		break;
	case ICE_SR_1ST_OROM_BANK_PTR:
		offset = banks->orom_ptr;
		size = banks->orom_size;
		active_bank = banks->orom_bank;
		break;
	case ICE_SR_NETLIST_BANK_PTR:
		offset = banks->netlist_ptr;
		size = banks->netlist_size;
		active_bank = banks->netlist_bank;
		break;
	default:
		ice_debug(hw, ICE_DBG_NVM, "Unexpected value for flash module: 0x%04x\n", module);
		return 0;
	}

	switch (active_bank) {
	case ICE_1ST_FLASH_BANK:
		second_bank_active = false;
		break;
	case ICE_2ND_FLASH_BANK:
		second_bank_active = true;
		break;
	default:
		ice_debug(hw, ICE_DBG_NVM, "Unexpected value for active flash bank: %u\n",
			  active_bank);
		return 0;
	}

	 
	switch (bank) {
	case ICE_ACTIVE_FLASH_BANK:
		return offset + (second_bank_active ? size : 0);
	case ICE_INACTIVE_FLASH_BANK:
		return offset + (second_bank_active ? 0 : size);
	}

	ice_debug(hw, ICE_DBG_NVM, "Unexpected value for flash bank selection: %u\n", bank);
	return 0;
}

 
static int
ice_read_flash_module(struct ice_hw *hw, enum ice_bank_select bank, u16 module,
		      u32 offset, u8 *data, u32 length)
{
	int status;
	u32 start;

	start = ice_get_flash_bank_offset(hw, bank, module);
	if (!start) {
		ice_debug(hw, ICE_DBG_NVM, "Unable to calculate flash bank offset for module 0x%04x\n",
			  module);
		return -EINVAL;
	}

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	status = ice_read_flat_nvm(hw, start + offset, &length, data, false);

	ice_release_nvm(hw);

	return status;
}

 
static int
ice_read_nvm_module(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	__le16 data_local;
	int status;

	status = ice_read_flash_module(hw, bank, ICE_SR_1ST_NVM_BANK_PTR, offset * sizeof(u16),
				       (__force u8 *)&data_local, sizeof(u16));
	if (!status)
		*data = le16_to_cpu(data_local);

	return status;
}

 
static int
ice_read_nvm_sr_copy(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	return ice_read_nvm_module(hw, bank, ICE_NVM_SR_COPY_WORD_OFFSET + offset, data);
}

 
static int
ice_read_netlist_module(struct ice_hw *hw, enum ice_bank_select bank, u32 offset, u16 *data)
{
	__le16 data_local;
	int status;

	status = ice_read_flash_module(hw, bank, ICE_SR_NETLIST_BANK_PTR, offset * sizeof(u16),
				       (__force u8 *)&data_local, sizeof(u16));
	if (!status)
		*data = le16_to_cpu(data_local);

	return status;
}

 
int ice_read_sr_word(struct ice_hw *hw, u16 offset, u16 *data)
{
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (!status) {
		status = ice_read_sr_word_aq(hw, offset, data);
		ice_release_nvm(hw);
	}

	return status;
}

 
int
ice_get_pfa_module_tlv(struct ice_hw *hw, u16 *module_tlv, u16 *module_tlv_len,
		       u16 module_type)
{
	u16 pfa_len, pfa_ptr;
	u16 next_tlv;
	int status;

	status = ice_read_sr_word(hw, ICE_SR_PFA_PTR, &pfa_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Preserved Field Array pointer.\n");
		return status;
	}
	status = ice_read_sr_word(hw, pfa_ptr, &pfa_len);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read PFA length.\n");
		return status;
	}
	 
	next_tlv = pfa_ptr + 1;
	while (next_tlv < pfa_ptr + pfa_len) {
		u16 tlv_sub_module_type;
		u16 tlv_len;

		 
		status = ice_read_sr_word(hw, next_tlv, &tlv_sub_module_type);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read TLV type.\n");
			break;
		}
		 
		status = ice_read_sr_word(hw, next_tlv + 1, &tlv_len);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read TLV length.\n");
			break;
		}
		if (tlv_sub_module_type == module_type) {
			if (tlv_len) {
				*module_tlv = next_tlv;
				*module_tlv_len = tlv_len;
				return 0;
			}
			return -EINVAL;
		}
		 
		next_tlv = next_tlv + tlv_len + 2;
	}
	 
	return -ENOENT;
}

 
int ice_read_pba_string(struct ice_hw *hw, u8 *pba_num, u32 pba_num_size)
{
	u16 pba_tlv, pba_tlv_len;
	u16 pba_word, pba_size;
	int status;
	u16 i;

	status = ice_get_pfa_module_tlv(hw, &pba_tlv, &pba_tlv_len,
					ICE_SR_PBA_BLOCK_PTR);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read PBA Block TLV.\n");
		return status;
	}

	 
	status = ice_read_sr_word(hw, (pba_tlv + 2), &pba_size);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read PBA Section size.\n");
		return status;
	}

	if (pba_tlv_len < pba_size) {
		ice_debug(hw, ICE_DBG_INIT, "Invalid PBA Block TLV size.\n");
		return -EINVAL;
	}

	 
	pba_size--;
	if (pba_num_size < (((u32)pba_size * 2) + 1)) {
		ice_debug(hw, ICE_DBG_INIT, "Buffer too small for PBA data.\n");
		return -EINVAL;
	}

	for (i = 0; i < pba_size; i++) {
		status = ice_read_sr_word(hw, (pba_tlv + 2 + 1) + i, &pba_word);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read PBA Block word %d.\n", i);
			return status;
		}

		pba_num[(i * 2)] = (pba_word >> 8) & 0xFF;
		pba_num[(i * 2) + 1] = pba_word & 0xFF;
	}
	pba_num[(pba_size * 2)] = '\0';

	return status;
}

 
static int
ice_get_nvm_ver_info(struct ice_hw *hw, enum ice_bank_select bank, struct ice_nvm_info *nvm)
{
	u16 eetrack_lo, eetrack_hi, ver;
	int status;

	status = ice_read_nvm_sr_copy(hw, bank, ICE_SR_NVM_DEV_STARTER_VER, &ver);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read DEV starter version.\n");
		return status;
	}

	nvm->major = (ver & ICE_NVM_VER_HI_MASK) >> ICE_NVM_VER_HI_SHIFT;
	nvm->minor = (ver & ICE_NVM_VER_LO_MASK) >> ICE_NVM_VER_LO_SHIFT;

	status = ice_read_nvm_sr_copy(hw, bank, ICE_SR_NVM_EETRACK_LO, &eetrack_lo);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read EETRACK lo.\n");
		return status;
	}
	status = ice_read_nvm_sr_copy(hw, bank, ICE_SR_NVM_EETRACK_HI, &eetrack_hi);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read EETRACK hi.\n");
		return status;
	}

	nvm->eetrack = (eetrack_hi << 16) | eetrack_lo;

	return 0;
}

 
int ice_get_inactive_nvm_ver(struct ice_hw *hw, struct ice_nvm_info *nvm)
{
	return ice_get_nvm_ver_info(hw, ICE_INACTIVE_FLASH_BANK, nvm);
}

 
static int
ice_get_orom_civd_data(struct ice_hw *hw, enum ice_bank_select bank,
		       struct ice_orom_civd_info *civd)
{
	u8 *orom_data;
	int status;
	u32 offset;

	 
	orom_data = vzalloc(hw->flash.banks.orom_size);
	if (!orom_data)
		return -ENOMEM;

	status = ice_read_flash_module(hw, bank, ICE_SR_1ST_OROM_BANK_PTR, 0,
				       orom_data, hw->flash.banks.orom_size);
	if (status) {
		vfree(orom_data);
		ice_debug(hw, ICE_DBG_NVM, "Unable to read Option ROM data\n");
		return status;
	}

	 
	for (offset = 0; (offset + 512) <= hw->flash.banks.orom_size; offset += 512) {
		struct ice_orom_civd_info *tmp;
		u8 sum = 0, i;

		tmp = (struct ice_orom_civd_info *)&orom_data[offset];

		 
		if (memcmp("$CIV", tmp->signature, sizeof(tmp->signature)) != 0)
			continue;

		ice_debug(hw, ICE_DBG_NVM, "Found CIVD section at offset %u\n",
			  offset);

		 
		for (i = 0; i < sizeof(*tmp); i++)
			sum += ((u8 *)tmp)[i];

		if (sum) {
			ice_debug(hw, ICE_DBG_NVM, "Found CIVD data with invalid checksum of %u\n",
				  sum);
			goto err_invalid_checksum;
		}

		*civd = *tmp;
		vfree(orom_data);
		return 0;
	}

	ice_debug(hw, ICE_DBG_NVM, "Unable to locate CIVD data within the Option ROM\n");

err_invalid_checksum:
	vfree(orom_data);
	return -EIO;
}

 
static int
ice_get_orom_ver_info(struct ice_hw *hw, enum ice_bank_select bank, struct ice_orom_info *orom)
{
	struct ice_orom_civd_info civd;
	u32 combo_ver;
	int status;

	status = ice_get_orom_civd_data(hw, bank, &civd);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to locate valid Option ROM CIVD data\n");
		return status;
	}

	combo_ver = le32_to_cpu(civd.combo_ver);

	orom->major = (u8)((combo_ver & ICE_OROM_VER_MASK) >> ICE_OROM_VER_SHIFT);
	orom->patch = (u8)(combo_ver & ICE_OROM_VER_PATCH_MASK);
	orom->build = (u16)((combo_ver & ICE_OROM_VER_BUILD_MASK) >> ICE_OROM_VER_BUILD_SHIFT);

	return 0;
}

 
int ice_get_inactive_orom_ver(struct ice_hw *hw, struct ice_orom_info *orom)
{
	return ice_get_orom_ver_info(hw, ICE_INACTIVE_FLASH_BANK, orom);
}

 
static int
ice_get_netlist_info(struct ice_hw *hw, enum ice_bank_select bank,
		     struct ice_netlist_info *netlist)
{
	u16 module_id, length, node_count, i;
	u16 *id_blk;
	int status;

	status = ice_read_netlist_module(hw, bank, ICE_NETLIST_TYPE_OFFSET, &module_id);
	if (status)
		return status;

	if (module_id != ICE_NETLIST_LINK_TOPO_MOD_ID) {
		ice_debug(hw, ICE_DBG_NVM, "Expected netlist module_id ID of 0x%04x, but got 0x%04x\n",
			  ICE_NETLIST_LINK_TOPO_MOD_ID, module_id);
		return -EIO;
	}

	status = ice_read_netlist_module(hw, bank, ICE_LINK_TOPO_MODULE_LEN, &length);
	if (status)
		return status;

	 
	if (length < ICE_NETLIST_ID_BLK_SIZE) {
		ice_debug(hw, ICE_DBG_NVM, "Netlist Link Topology module too small. Expected at least %u words, but got %u words.\n",
			  ICE_NETLIST_ID_BLK_SIZE, length);
		return -EIO;
	}

	status = ice_read_netlist_module(hw, bank, ICE_LINK_TOPO_NODE_COUNT, &node_count);
	if (status)
		return status;
	node_count &= ICE_LINK_TOPO_NODE_COUNT_M;

	id_blk = kcalloc(ICE_NETLIST_ID_BLK_SIZE, sizeof(*id_blk), GFP_KERNEL);
	if (!id_blk)
		return -ENOMEM;

	 
	status = ice_read_flash_module(hw, bank, ICE_SR_NETLIST_BANK_PTR,
				       ICE_NETLIST_ID_BLK_OFFSET(node_count) * sizeof(u16),
				       (u8 *)id_blk, ICE_NETLIST_ID_BLK_SIZE * sizeof(u16));
	if (status)
		goto exit_error;

	for (i = 0; i < ICE_NETLIST_ID_BLK_SIZE; i++)
		id_blk[i] = le16_to_cpu(((__force __le16 *)id_blk)[i]);

	netlist->major = id_blk[ICE_NETLIST_ID_BLK_MAJOR_VER_HIGH] << 16 |
			 id_blk[ICE_NETLIST_ID_BLK_MAJOR_VER_LOW];
	netlist->minor = id_blk[ICE_NETLIST_ID_BLK_MINOR_VER_HIGH] << 16 |
			 id_blk[ICE_NETLIST_ID_BLK_MINOR_VER_LOW];
	netlist->type = id_blk[ICE_NETLIST_ID_BLK_TYPE_HIGH] << 16 |
			id_blk[ICE_NETLIST_ID_BLK_TYPE_LOW];
	netlist->rev = id_blk[ICE_NETLIST_ID_BLK_REV_HIGH] << 16 |
		       id_blk[ICE_NETLIST_ID_BLK_REV_LOW];
	netlist->cust_ver = id_blk[ICE_NETLIST_ID_BLK_CUST_VER];
	 
	netlist->hash = id_blk[ICE_NETLIST_ID_BLK_SHA_HASH_WORD(15)] << 16 |
			id_blk[ICE_NETLIST_ID_BLK_SHA_HASH_WORD(14)];

exit_error:
	kfree(id_blk);

	return status;
}

 
int ice_get_inactive_netlist_ver(struct ice_hw *hw, struct ice_netlist_info *netlist)
{
	return ice_get_netlist_info(hw, ICE_INACTIVE_FLASH_BANK, netlist);
}

 
static int ice_discover_flash_size(struct ice_hw *hw)
{
	u32 min_size = 0, max_size = ICE_AQC_NVM_MAX_OFFSET + 1;
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	while ((max_size - min_size) > 1) {
		u32 offset = (max_size + min_size) / 2;
		u32 len = 1;
		u8 data;

		status = ice_read_flat_nvm(hw, offset, &len, &data, false);
		if (status == -EIO &&
		    hw->adminq.sq_last_status == ICE_AQ_RC_EINVAL) {
			ice_debug(hw, ICE_DBG_NVM, "%s: New upper bound of %u bytes\n",
				  __func__, offset);
			status = 0;
			max_size = offset;
		} else if (!status) {
			ice_debug(hw, ICE_DBG_NVM, "%s: New lower bound of %u bytes\n",
				  __func__, offset);
			min_size = offset;
		} else {
			 
			goto err_read_flat_nvm;
		}
	}

	ice_debug(hw, ICE_DBG_NVM, "Predicted flash size is %u bytes\n", max_size);

	hw->flash.flash_size = max_size;

err_read_flat_nvm:
	ice_release_nvm(hw);

	return status;
}

 
static int ice_read_sr_pointer(struct ice_hw *hw, u16 offset, u32 *pointer)
{
	int status;
	u16 value;

	status = ice_read_sr_word(hw, offset, &value);
	if (status)
		return status;

	 
	if (value & ICE_SR_NVM_PTR_4KB_UNITS)
		*pointer = (value & ~ICE_SR_NVM_PTR_4KB_UNITS) * 4 * 1024;
	else
		*pointer = value * 2;

	return 0;
}

 
static int ice_read_sr_area_size(struct ice_hw *hw, u16 offset, u32 *size)
{
	int status;
	u16 value;

	status = ice_read_sr_word(hw, offset, &value);
	if (status)
		return status;

	 
	*size = value * 4 * 1024;

	return 0;
}

 
static int ice_determine_active_flash_banks(struct ice_hw *hw)
{
	struct ice_bank_info *banks = &hw->flash.banks;
	u16 ctrl_word;
	int status;

	status = ice_read_sr_word(hw, ICE_SR_NVM_CTRL_WORD, &ctrl_word);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read the Shadow RAM control word\n");
		return status;
	}

	 
	if ((ctrl_word & ICE_SR_CTRL_WORD_1_M) >> ICE_SR_CTRL_WORD_1_S != ICE_SR_CTRL_WORD_VALID) {
		ice_debug(hw, ICE_DBG_NVM, "Shadow RAM control word is invalid\n");
		return -EIO;
	}

	if (!(ctrl_word & ICE_SR_CTRL_WORD_NVM_BANK))
		banks->nvm_bank = ICE_1ST_FLASH_BANK;
	else
		banks->nvm_bank = ICE_2ND_FLASH_BANK;

	if (!(ctrl_word & ICE_SR_CTRL_WORD_OROM_BANK))
		banks->orom_bank = ICE_1ST_FLASH_BANK;
	else
		banks->orom_bank = ICE_2ND_FLASH_BANK;

	if (!(ctrl_word & ICE_SR_CTRL_WORD_NETLIST_BANK))
		banks->netlist_bank = ICE_1ST_FLASH_BANK;
	else
		banks->netlist_bank = ICE_2ND_FLASH_BANK;

	status = ice_read_sr_pointer(hw, ICE_SR_1ST_NVM_BANK_PTR, &banks->nvm_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read NVM bank pointer\n");
		return status;
	}

	status = ice_read_sr_area_size(hw, ICE_SR_NVM_BANK_SIZE, &banks->nvm_size);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read NVM bank area size\n");
		return status;
	}

	status = ice_read_sr_pointer(hw, ICE_SR_1ST_OROM_BANK_PTR, &banks->orom_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read OROM bank pointer\n");
		return status;
	}

	status = ice_read_sr_area_size(hw, ICE_SR_OROM_BANK_SIZE, &banks->orom_size);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read OROM bank area size\n");
		return status;
	}

	status = ice_read_sr_pointer(hw, ICE_SR_NETLIST_BANK_PTR, &banks->netlist_ptr);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read Netlist bank pointer\n");
		return status;
	}

	status = ice_read_sr_area_size(hw, ICE_SR_NETLIST_BANK_SIZE, &banks->netlist_size);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to read Netlist bank area size\n");
		return status;
	}

	return 0;
}

 
int ice_init_nvm(struct ice_hw *hw)
{
	struct ice_flash_info *flash = &hw->flash;
	u32 fla, gens_stat;
	u8 sr_size;
	int status;

	 
	gens_stat = rd32(hw, GLNVM_GENS);
	sr_size = (gens_stat & GLNVM_GENS_SR_SIZE_M) >> GLNVM_GENS_SR_SIZE_S;

	 
	flash->sr_words = BIT(sr_size) * ICE_SR_WORDS_IN_1KB;

	 
	fla = rd32(hw, GLNVM_FLA);
	if (fla & GLNVM_FLA_LOCKED_M) {  
		flash->blank_nvm_mode = false;
	} else {
		 
		flash->blank_nvm_mode = true;
		ice_debug(hw, ICE_DBG_NVM, "NVM init error: unsupported blank mode.\n");
		return -EIO;
	}

	status = ice_discover_flash_size(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "NVM init error: failed to discover flash size.\n");
		return status;
	}

	status = ice_determine_active_flash_banks(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to determine active flash banks.\n");
		return status;
	}

	status = ice_get_nvm_ver_info(hw, ICE_ACTIVE_FLASH_BANK, &flash->nvm);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read NVM info.\n");
		return status;
	}

	status = ice_get_orom_ver_info(hw, ICE_ACTIVE_FLASH_BANK, &flash->orom);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to read Option ROM info.\n");

	 
	status = ice_get_netlist_info(hw, ICE_ACTIVE_FLASH_BANK, &flash->netlist);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to read netlist info.\n");

	return 0;
}

 
int ice_nvm_validate_checksum(struct ice_hw *hw)
{
	struct ice_aqc_nvm_checksum *cmd;
	struct ice_aq_desc desc;
	int status;

	status = ice_acquire_nvm(hw, ICE_RES_READ);
	if (status)
		return status;

	cmd = &desc.params.nvm_checksum;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_checksum);
	cmd->flags = ICE_AQC_NVM_CHECKSUM_VERIFY;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	ice_release_nvm(hw);

	if (!status)
		if (le16_to_cpu(cmd->checksum) != ICE_AQC_NVM_CHECKSUM_CORRECT)
			status = -EIO;

	return status;
}

 
int ice_nvm_write_activate(struct ice_hw *hw, u16 cmd_flags, u8 *response_flags)
{
	struct ice_aqc_nvm *cmd;
	struct ice_aq_desc desc;
	int err;

	cmd = &desc.params.nvm;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_write_activate);

	cmd->cmd_flags = (u8)(cmd_flags & 0xFF);
	cmd->offset_high = (u8)((cmd_flags >> 8) & 0xFF);

	err = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (!err && response_flags)
		*response_flags = cmd->cmd_flags;

	return err;
}

 
int ice_aq_nvm_update_empr(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_update_empr);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

 

int
ice_nvm_set_pkg_data(struct ice_hw *hw, bool del_pkg_data_flag, u8 *data,
		     u16 length, struct ice_sq_cd *cd)
{
	struct ice_aqc_nvm_pkg_data *cmd;
	struct ice_aq_desc desc;

	if (length != 0 && !data)
		return -EINVAL;

	cmd = &desc.params.pkg_data;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_nvm_pkg_data);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	if (del_pkg_data_flag)
		cmd->cmd_flags |= ICE_AQC_NVM_PKG_DELETE;

	return ice_aq_send_cmd(hw, &desc, data, length, cd);
}

 

int
ice_nvm_pass_component_tbl(struct ice_hw *hw, u8 *data, u16 length,
			   u8 transfer_flag, u8 *comp_response,
			   u8 *comp_response_code, struct ice_sq_cd *cd)
{
	struct ice_aqc_nvm_pass_comp_tbl *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!data || !comp_response || !comp_response_code)
		return -EINVAL;

	cmd = &desc.params.pass_comp_tbl;

	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_nvm_pass_component_tbl);
	desc.flags |= cpu_to_le16(ICE_AQ_FLAG_RD);

	cmd->transfer_flag = transfer_flag;
	status = ice_aq_send_cmd(hw, &desc, data, length, cd);

	if (!status) {
		*comp_response = cmd->component_response;
		*comp_response_code = cmd->component_response_code;
	}
	return status;
}

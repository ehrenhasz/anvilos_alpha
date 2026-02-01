
 

#include <linux/delay.h>

#include "igc_hw.h"

 
static s32 igc_acquire_nvm_i225(struct igc_hw *hw)
{
	return igc_acquire_swfw_sync_i225(hw, IGC_SWFW_EEP_SM);
}

 
static void igc_release_nvm_i225(struct igc_hw *hw)
{
	igc_release_swfw_sync_i225(hw, IGC_SWFW_EEP_SM);
}

 
static s32 igc_get_hw_semaphore_i225(struct igc_hw *hw)
{
	s32 timeout = hw->nvm.word_size + 1;
	s32 i = 0;
	u32 swsm;

	 
	while (i < timeout) {
		swsm = rd32(IGC_SWSM);
		if (!(swsm & IGC_SWSM_SMBI))
			break;

		usleep_range(500, 600);
		i++;
	}

	if (i == timeout) {
		 
		if (hw->dev_spec._base.clear_semaphore_once) {
			hw->dev_spec._base.clear_semaphore_once = false;
			igc_put_hw_semaphore(hw);
			for (i = 0; i < timeout; i++) {
				swsm = rd32(IGC_SWSM);
				if (!(swsm & IGC_SWSM_SMBI))
					break;

				usleep_range(500, 600);
			}
		}

		 
		if (i == timeout) {
			hw_dbg("Driver can't access device - SMBI bit is set.\n");
			return -IGC_ERR_NVM;
		}
	}

	 
	for (i = 0; i < timeout; i++) {
		swsm = rd32(IGC_SWSM);
		wr32(IGC_SWSM, swsm | IGC_SWSM_SWESMBI);

		 
		if (rd32(IGC_SWSM) & IGC_SWSM_SWESMBI)
			break;

		usleep_range(500, 600);
	}

	if (i == timeout) {
		 
		igc_put_hw_semaphore(hw);
		hw_dbg("Driver can't access the NVM\n");
		return -IGC_ERR_NVM;
	}

	return 0;
}

 
s32 igc_acquire_swfw_sync_i225(struct igc_hw *hw, u16 mask)
{
	s32 i = 0, timeout = 200;
	u32 fwmask = mask << 16;
	u32 swmask = mask;
	s32 ret_val = 0;
	u32 swfw_sync;

	while (i < timeout) {
		if (igc_get_hw_semaphore_i225(hw)) {
			ret_val = -IGC_ERR_SWFW_SYNC;
			goto out;
		}

		swfw_sync = rd32(IGC_SW_FW_SYNC);
		if (!(swfw_sync & (fwmask | swmask)))
			break;

		 
		igc_put_hw_semaphore(hw);
		mdelay(5);
		i++;
	}

	if (i == timeout) {
		hw_dbg("Driver can't access resource, SW_FW_SYNC timeout.\n");
		ret_val = -IGC_ERR_SWFW_SYNC;
		goto out;
	}

	swfw_sync |= swmask;
	wr32(IGC_SW_FW_SYNC, swfw_sync);

	igc_put_hw_semaphore(hw);
out:
	return ret_val;
}

 
void igc_release_swfw_sync_i225(struct igc_hw *hw, u16 mask)
{
	u32 swfw_sync;

	 
	if (igc_get_hw_semaphore_i225(hw)) {
		hw_dbg("Failed to release SW_FW_SYNC.\n");
		return;
	}

	swfw_sync = rd32(IGC_SW_FW_SYNC);
	swfw_sync &= ~mask;
	wr32(IGC_SW_FW_SYNC, swfw_sync);

	igc_put_hw_semaphore(hw);
}

 
static s32 igc_read_nvm_srrd_i225(struct igc_hw *hw, u16 offset, u16 words,
				  u16 *data)
{
	s32 status = 0;
	u16 i, count;

	 
	for (i = 0; i < words; i += IGC_EERD_EEWR_MAX_COUNT) {
		count = (words - i) / IGC_EERD_EEWR_MAX_COUNT > 0 ?
			IGC_EERD_EEWR_MAX_COUNT : (words - i);

		status = hw->nvm.ops.acquire(hw);
		if (status)
			break;

		status = igc_read_nvm_eerd(hw, offset, count, data + i);
		hw->nvm.ops.release(hw);
		if (status)
			break;
	}

	return status;
}

 
static s32 igc_write_nvm_srwr(struct igc_hw *hw, u16 offset, u16 words,
			      u16 *data)
{
	struct igc_nvm_info *nvm = &hw->nvm;
	s32 ret_val = -IGC_ERR_NVM;
	u32 attempts = 100000;
	u32 i, k, eewr = 0;

	 
	if (offset >= nvm->word_size || (words > (nvm->word_size - offset)) ||
	    words == 0) {
		hw_dbg("nvm parameter(s) out of bounds\n");
		return ret_val;
	}

	for (i = 0; i < words; i++) {
		ret_val = -IGC_ERR_NVM;
		eewr = ((offset + i) << IGC_NVM_RW_ADDR_SHIFT) |
			(data[i] << IGC_NVM_RW_REG_DATA) |
			IGC_NVM_RW_REG_START;

		wr32(IGC_SRWR, eewr);

		for (k = 0; k < attempts; k++) {
			if (IGC_NVM_RW_REG_DONE &
			    rd32(IGC_SRWR)) {
				ret_val = 0;
				break;
			}
			udelay(5);
		}

		if (ret_val) {
			hw_dbg("Shadow RAM write EEWR timed out\n");
			break;
		}
	}

	return ret_val;
}

 
static s32 igc_write_nvm_srwr_i225(struct igc_hw *hw, u16 offset, u16 words,
				   u16 *data)
{
	s32 status = 0;
	u16 i, count;

	 
	for (i = 0; i < words; i += IGC_EERD_EEWR_MAX_COUNT) {
		count = (words - i) / IGC_EERD_EEWR_MAX_COUNT > 0 ?
			IGC_EERD_EEWR_MAX_COUNT : (words - i);

		status = hw->nvm.ops.acquire(hw);
		if (status)
			break;

		status = igc_write_nvm_srwr(hw, offset, count, data + i);
		hw->nvm.ops.release(hw);
		if (status)
			break;
	}

	return status;
}

 
static s32 igc_validate_nvm_checksum_i225(struct igc_hw *hw)
{
	s32 (*read_op_ptr)(struct igc_hw *hw, u16 offset, u16 count,
			   u16 *data);
	s32 status = 0;

	status = hw->nvm.ops.acquire(hw);
	if (status)
		goto out;

	 
	read_op_ptr = hw->nvm.ops.read;
	hw->nvm.ops.read = igc_read_nvm_eerd;

	status = igc_validate_nvm_checksum(hw);

	 
	hw->nvm.ops.read = read_op_ptr;

	hw->nvm.ops.release(hw);

out:
	return status;
}

 
static s32 igc_pool_flash_update_done_i225(struct igc_hw *hw)
{
	s32 ret_val = -IGC_ERR_NVM;
	u32 i, reg;

	for (i = 0; i < IGC_FLUDONE_ATTEMPTS; i++) {
		reg = rd32(IGC_EECD);
		if (reg & IGC_EECD_FLUDONE_I225) {
			ret_val = 0;
			break;
		}
		udelay(5);
	}

	return ret_val;
}

 
static s32 igc_update_flash_i225(struct igc_hw *hw)
{
	s32 ret_val = 0;
	u32 flup;

	ret_val = igc_pool_flash_update_done_i225(hw);
	if (ret_val == -IGC_ERR_NVM) {
		hw_dbg("Flash update time out\n");
		goto out;
	}

	flup = rd32(IGC_EECD) | IGC_EECD_FLUPD_I225;
	wr32(IGC_EECD, flup);

	ret_val = igc_pool_flash_update_done_i225(hw);
	if (ret_val)
		hw_dbg("Flash update time out\n");
	else
		hw_dbg("Flash update complete\n");

out:
	return ret_val;
}

 
static s32 igc_update_nvm_checksum_i225(struct igc_hw *hw)
{
	u16 checksum = 0;
	s32 ret_val = 0;
	u16 i, nvm_data;

	 
	ret_val = igc_read_nvm_eerd(hw, 0, 1, &nvm_data);
	if (ret_val) {
		hw_dbg("EEPROM read failed\n");
		goto out;
	}

	ret_val = hw->nvm.ops.acquire(hw);
	if (ret_val)
		goto out;

	 

	for (i = 0; i < NVM_CHECKSUM_REG; i++) {
		ret_val = igc_read_nvm_eerd(hw, i, 1, &nvm_data);
		if (ret_val) {
			hw->nvm.ops.release(hw);
			hw_dbg("NVM Read Error while updating checksum.\n");
			goto out;
		}
		checksum += nvm_data;
	}
	checksum = (u16)NVM_SUM - checksum;
	ret_val = igc_write_nvm_srwr(hw, NVM_CHECKSUM_REG, 1,
				     &checksum);
	if (ret_val) {
		hw->nvm.ops.release(hw);
		hw_dbg("NVM Write Error while updating checksum.\n");
		goto out;
	}

	hw->nvm.ops.release(hw);

	ret_val = igc_update_flash_i225(hw);

out:
	return ret_val;
}

 
bool igc_get_flash_presence_i225(struct igc_hw *hw)
{
	bool ret_val = false;
	u32 eec = 0;

	eec = rd32(IGC_EECD);
	if (eec & IGC_EECD_FLASH_DETECTED_I225)
		ret_val = true;

	return ret_val;
}

 
s32 igc_init_nvm_params_i225(struct igc_hw *hw)
{
	struct igc_nvm_info *nvm = &hw->nvm;

	nvm->ops.acquire = igc_acquire_nvm_i225;
	nvm->ops.release = igc_release_nvm_i225;

	 
	if (igc_get_flash_presence_i225(hw)) {
		nvm->ops.read = igc_read_nvm_srrd_i225;
		nvm->ops.write = igc_write_nvm_srwr_i225;
		nvm->ops.validate = igc_validate_nvm_checksum_i225;
		nvm->ops.update = igc_update_nvm_checksum_i225;
	} else {
		nvm->ops.read = igc_read_nvm_eerd;
		nvm->ops.write = NULL;
		nvm->ops.validate = NULL;
		nvm->ops.update = NULL;
	}
	return 0;
}

 
s32 igc_set_eee_i225(struct igc_hw *hw, bool adv2p5G, bool adv1G,
		     bool adv100M)
{
	u32 ipcnfg, eeer;

	ipcnfg = rd32(IGC_IPCNFG);
	eeer = rd32(IGC_EEER);

	 
	if (hw->dev_spec._base.eee_enable) {
		u32 eee_su = rd32(IGC_EEE_SU);

		if (adv100M)
			ipcnfg |= IGC_IPCNFG_EEE_100M_AN;
		else
			ipcnfg &= ~IGC_IPCNFG_EEE_100M_AN;

		if (adv1G)
			ipcnfg |= IGC_IPCNFG_EEE_1G_AN;
		else
			ipcnfg &= ~IGC_IPCNFG_EEE_1G_AN;

		if (adv2p5G)
			ipcnfg |= IGC_IPCNFG_EEE_2_5G_AN;
		else
			ipcnfg &= ~IGC_IPCNFG_EEE_2_5G_AN;

		eeer |= (IGC_EEER_TX_LPI_EN | IGC_EEER_RX_LPI_EN |
			 IGC_EEER_LPI_FC);

		 
		if (eee_su & IGC_EEE_SU_LPI_CLK_STP)
			hw_dbg("LPI Clock Stop Bit should not be set!\n");
	} else {
		ipcnfg &= ~(IGC_IPCNFG_EEE_2_5G_AN | IGC_IPCNFG_EEE_1G_AN |
			    IGC_IPCNFG_EEE_100M_AN);
		eeer &= ~(IGC_EEER_TX_LPI_EN | IGC_EEER_RX_LPI_EN |
			  IGC_EEER_LPI_FC);
	}
	wr32(IGC_IPCNFG, ipcnfg);
	wr32(IGC_EEER, eeer);
	rd32(IGC_IPCNFG);
	rd32(IGC_EEER);

	return IGC_SUCCESS;
}

 
s32 igc_set_ltr_i225(struct igc_hw *hw, bool link)
{
	u32 tw_system, ltrc, ltrv, ltr_min, ltr_max, scale_min, scale_max;
	u16 speed, duplex;
	s32 size;

	 
	if (link) {
		hw->mac.ops.get_speed_and_duplex(hw, &speed, &duplex);

		 
		if (hw->dev_spec._base.eee_enable &&
		    speed != SPEED_10) {
			 
			ltrc = rd32(IGC_LTRC) |
			       IGC_LTRC_EEEMS_EN;
			wr32(IGC_LTRC, ltrc);

			 
			if (speed == SPEED_100) {
				tw_system = ((rd32(IGC_EEE_SU) &
					     IGC_TW_SYSTEM_100_MASK) >>
					     IGC_TW_SYSTEM_100_SHIFT) * 500;
			} else {
				tw_system = (rd32(IGC_EEE_SU) &
					     IGC_TW_SYSTEM_1000_MASK) * 500;
			}
		} else {
			tw_system = 0;
		}

		 
		size = rd32(IGC_RXPBS) &
		       IGC_RXPBS_SIZE_I225_MASK;

		 
		size *= 1024;
		size *= 8;

		if (size < 0) {
			hw_dbg("Invalid effective Rx buffer size %d\n",
			       size);
			return -IGC_ERR_CONFIG;
		}

		 
		ltr_min = (1000 * size) / speed;
		ltr_max = ltr_min + tw_system;
		scale_min = (ltr_min / 1024) < 1024 ? IGC_LTRMINV_SCALE_1024 :
			    IGC_LTRMINV_SCALE_32768;
		scale_max = (ltr_max / 1024) < 1024 ? IGC_LTRMAXV_SCALE_1024 :
			    IGC_LTRMAXV_SCALE_32768;
		ltr_min /= scale_min == IGC_LTRMINV_SCALE_1024 ? 1024 : 32768;
		ltr_min -= 1;
		ltr_max /= scale_max == IGC_LTRMAXV_SCALE_1024 ? 1024 : 32768;
		ltr_max -= 1;

		 
		ltrv = rd32(IGC_LTRMINV);
		if (ltr_min != (ltrv & IGC_LTRMINV_LTRV_MASK)) {
			ltrv = IGC_LTRMINV_LSNP_REQ | ltr_min |
			       (scale_min << IGC_LTRMINV_SCALE_SHIFT);
			wr32(IGC_LTRMINV, ltrv);
		}

		ltrv = rd32(IGC_LTRMAXV);
		if (ltr_max != (ltrv & IGC_LTRMAXV_LTRV_MASK)) {
			ltrv = IGC_LTRMAXV_LSNP_REQ | ltr_max |
			       (scale_max << IGC_LTRMAXV_SCALE_SHIFT);
			wr32(IGC_LTRMAXV, ltrv);
		}
	}

	return IGC_SUCCESS;
}

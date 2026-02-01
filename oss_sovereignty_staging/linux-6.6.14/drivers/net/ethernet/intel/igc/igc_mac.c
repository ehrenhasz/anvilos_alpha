
 

#include <linux/pci.h>
#include <linux/delay.h>

#include "igc_mac.h"
#include "igc_hw.h"

 
s32 igc_disable_pcie_master(struct igc_hw *hw)
{
	s32 timeout = MASTER_DISABLE_TIMEOUT;
	s32 ret_val = 0;
	u32 ctrl;

	ctrl = rd32(IGC_CTRL);
	ctrl |= IGC_CTRL_GIO_MASTER_DISABLE;
	wr32(IGC_CTRL, ctrl);

	while (timeout) {
		if (!(rd32(IGC_STATUS) &
		    IGC_STATUS_GIO_MASTER_ENABLE))
			break;
		usleep_range(2000, 3000);
		timeout--;
	}

	if (!timeout) {
		hw_dbg("Master requests are pending.\n");
		ret_val = -IGC_ERR_MASTER_REQUESTS_PENDING;
		goto out;
	}

out:
	return ret_val;
}

 
void igc_init_rx_addrs(struct igc_hw *hw, u16 rar_count)
{
	u8 mac_addr[ETH_ALEN] = {0};
	u32 i;

	 
	hw_dbg("Programming MAC Address into RAR[0]\n");

	hw->mac.ops.rar_set(hw, hw->mac.addr, 0);

	 
	hw_dbg("Clearing RAR[1-%u]\n", rar_count - 1);
	for (i = 1; i < rar_count; i++)
		hw->mac.ops.rar_set(hw, mac_addr, i);
}

 
static s32 igc_set_fc_watermarks(struct igc_hw *hw)
{
	u32 fcrtl = 0, fcrth = 0;

	 
	if (hw->fc.current_mode & igc_fc_tx_pause) {
		 
		fcrtl = hw->fc.low_water;
		if (hw->fc.send_xon)
			fcrtl |= IGC_FCRTL_XONE;

		fcrth = hw->fc.high_water;
	}
	wr32(IGC_FCRTL, fcrtl);
	wr32(IGC_FCRTH, fcrth);

	return 0;
}

 
s32 igc_setup_link(struct igc_hw *hw)
{
	s32 ret_val = 0;

	 
	if (igc_check_reset_block(hw))
		goto out;

	 
	if (hw->fc.requested_mode == igc_fc_default)
		hw->fc.requested_mode = igc_fc_full;

	 
	hw->fc.current_mode = hw->fc.requested_mode;

	hw_dbg("After fix-ups FlowControl is now = %x\n", hw->fc.current_mode);

	 
	ret_val = hw->mac.ops.setup_physical_interface(hw);
	if (ret_val)
		goto out;

	 
	hw_dbg("Initializing the Flow Control address, type and timer regs\n");
	wr32(IGC_FCT, FLOW_CONTROL_TYPE);
	wr32(IGC_FCAH, FLOW_CONTROL_ADDRESS_HIGH);
	wr32(IGC_FCAL, FLOW_CONTROL_ADDRESS_LOW);

	wr32(IGC_FCTTV, hw->fc.pause_time);

	ret_val = igc_set_fc_watermarks(hw);

out:
	return ret_val;
}

 
s32 igc_force_mac_fc(struct igc_hw *hw)
{
	s32 ret_val = 0;
	u32 ctrl;

	ctrl = rd32(IGC_CTRL);

	 
	hw_dbg("hw->fc.current_mode = %u\n", hw->fc.current_mode);

	switch (hw->fc.current_mode) {
	case igc_fc_none:
		ctrl &= (~(IGC_CTRL_TFCE | IGC_CTRL_RFCE));
		break;
	case igc_fc_rx_pause:
		ctrl &= (~IGC_CTRL_TFCE);
		ctrl |= IGC_CTRL_RFCE;
		break;
	case igc_fc_tx_pause:
		ctrl &= (~IGC_CTRL_RFCE);
		ctrl |= IGC_CTRL_TFCE;
		break;
	case igc_fc_full:
		ctrl |= (IGC_CTRL_TFCE | IGC_CTRL_RFCE);
		break;
	default:
		hw_dbg("Flow control param set incorrectly\n");
		ret_val = -IGC_ERR_CONFIG;
		goto out;
	}

	wr32(IGC_CTRL, ctrl);

out:
	return ret_val;
}

 
void igc_clear_hw_cntrs_base(struct igc_hw *hw)
{
	rd32(IGC_CRCERRS);
	rd32(IGC_MPC);
	rd32(IGC_SCC);
	rd32(IGC_ECOL);
	rd32(IGC_MCC);
	rd32(IGC_LATECOL);
	rd32(IGC_COLC);
	rd32(IGC_RERC);
	rd32(IGC_DC);
	rd32(IGC_RLEC);
	rd32(IGC_XONRXC);
	rd32(IGC_XONTXC);
	rd32(IGC_XOFFRXC);
	rd32(IGC_XOFFTXC);
	rd32(IGC_FCRUC);
	rd32(IGC_GPRC);
	rd32(IGC_BPRC);
	rd32(IGC_MPRC);
	rd32(IGC_GPTC);
	rd32(IGC_GORCL);
	rd32(IGC_GORCH);
	rd32(IGC_GOTCL);
	rd32(IGC_GOTCH);
	rd32(IGC_RNBC);
	rd32(IGC_RUC);
	rd32(IGC_RFC);
	rd32(IGC_ROC);
	rd32(IGC_RJC);
	rd32(IGC_TORL);
	rd32(IGC_TORH);
	rd32(IGC_TOTL);
	rd32(IGC_TOTH);
	rd32(IGC_TPR);
	rd32(IGC_TPT);
	rd32(IGC_MPTC);
	rd32(IGC_BPTC);

	rd32(IGC_PRC64);
	rd32(IGC_PRC127);
	rd32(IGC_PRC255);
	rd32(IGC_PRC511);
	rd32(IGC_PRC1023);
	rd32(IGC_PRC1522);
	rd32(IGC_PTC64);
	rd32(IGC_PTC127);
	rd32(IGC_PTC255);
	rd32(IGC_PTC511);
	rd32(IGC_PTC1023);
	rd32(IGC_PTC1522);

	rd32(IGC_ALGNERRC);
	rd32(IGC_RXERRC);
	rd32(IGC_TNCRS);
	rd32(IGC_HTDPMC);
	rd32(IGC_TSCTC);

	rd32(IGC_MGTPRC);
	rd32(IGC_MGTPDC);
	rd32(IGC_MGTPTC);

	rd32(IGC_IAC);

	rd32(IGC_RPTHC);
	rd32(IGC_TLPIC);
	rd32(IGC_RLPIC);
	rd32(IGC_HGPTC);
	rd32(IGC_RXDMTC);
	rd32(IGC_HGORCL);
	rd32(IGC_HGORCH);
	rd32(IGC_HGOTCL);
	rd32(IGC_HGOTCH);
	rd32(IGC_LENERRS);
}

 
void igc_rar_set(struct igc_hw *hw, u8 *addr, u32 index)
{
	u32 rar_low, rar_high;

	 
	rar_low = ((u32)addr[0] |
		   ((u32)addr[1] << 8) |
		   ((u32)addr[2] << 16) | ((u32)addr[3] << 24));

	rar_high = ((u32)addr[4] | ((u32)addr[5] << 8));

	 
	if (rar_low || rar_high)
		rar_high |= IGC_RAH_AV;

	 
	wr32(IGC_RAL(index), rar_low);
	wrfl();
	wr32(IGC_RAH(index), rar_high);
	wrfl();
}

 
s32 igc_check_for_copper_link(struct igc_hw *hw)
{
	struct igc_mac_info *mac = &hw->mac;
	bool link = false;
	s32 ret_val;

	 
	if (!mac->get_link_status) {
		ret_val = 0;
		goto out;
	}

	 
	ret_val = igc_phy_has_link(hw, 1, 0, &link);
	if (ret_val)
		goto out;

	if (!link)
		goto out;  

	mac->get_link_status = false;

	 
	igc_check_downshift(hw);

	 
	if (!mac->autoneg) {
		ret_val = -IGC_ERR_CONFIG;
		goto out;
	}

	 
	igc_config_collision_dist(hw);

	 
	ret_val = igc_config_fc_after_link_up(hw);
	if (ret_val)
		hw_dbg("Error configuring flow control\n");

out:
	 
	ret_val = igc_set_ltr_i225(hw, link);

	return ret_val;
}

 
void igc_config_collision_dist(struct igc_hw *hw)
{
	u32 tctl;

	tctl = rd32(IGC_TCTL);

	tctl &= ~IGC_TCTL_COLD;
	tctl |= IGC_COLLISION_DISTANCE << IGC_COLD_SHIFT;

	wr32(IGC_TCTL, tctl);
	wrfl();
}

 
s32 igc_config_fc_after_link_up(struct igc_hw *hw)
{
	u16 mii_status_reg, mii_nway_adv_reg, mii_nway_lp_ability_reg;
	struct igc_mac_info *mac = &hw->mac;
	u16 speed, duplex;
	s32 ret_val = 0;

	 
	if (mac->autoneg_failed)
		ret_val = igc_force_mac_fc(hw);

	if (ret_val) {
		hw_dbg("Error forcing flow control settings\n");
		goto out;
	}

	 
	if (mac->autoneg) {
		 
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS,
					       &mii_status_reg);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS,
					       &mii_status_reg);
		if (ret_val)
			goto out;

		if (!(mii_status_reg & MII_SR_AUTONEG_COMPLETE)) {
			hw_dbg("Copper PHY and Auto Neg has not completed.\n");
			goto out;
		}

		 
		ret_val = hw->phy.ops.read_reg(hw, PHY_AUTONEG_ADV,
					       &mii_nway_adv_reg);
		if (ret_val)
			goto out;
		ret_val = hw->phy.ops.read_reg(hw, PHY_LP_ABILITY,
					       &mii_nway_lp_ability_reg);
		if (ret_val)
			goto out;
		 
		if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
		    (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE)) {
			 
			if (hw->fc.requested_mode == igc_fc_full) {
				hw->fc.current_mode = igc_fc_full;
				hw_dbg("Flow Control = FULL.\n");
			} else {
				hw->fc.current_mode = igc_fc_rx_pause;
				hw_dbg("Flow Control = RX PAUSE frames only.\n");
			}
		}

		 
		else if (!(mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = igc_fc_tx_pause;
			hw_dbg("Flow Control = TX PAUSE frames only.\n");
		}
		 
		else if ((mii_nway_adv_reg & NWAY_AR_PAUSE) &&
			 (mii_nway_adv_reg & NWAY_AR_ASM_DIR) &&
			 !(mii_nway_lp_ability_reg & NWAY_LPAR_PAUSE) &&
			 (mii_nway_lp_ability_reg & NWAY_LPAR_ASM_DIR)) {
			hw->fc.current_mode = igc_fc_rx_pause;
			hw_dbg("Flow Control = RX PAUSE frames only.\n");
		}
		 
		else if ((hw->fc.requested_mode == igc_fc_none) ||
			 (hw->fc.requested_mode == igc_fc_tx_pause) ||
			 (hw->fc.strict_ieee)) {
			hw->fc.current_mode = igc_fc_none;
			hw_dbg("Flow Control = NONE.\n");
		} else {
			hw->fc.current_mode = igc_fc_rx_pause;
			hw_dbg("Flow Control = RX PAUSE frames only.\n");
		}

		 
		ret_val = hw->mac.ops.get_speed_and_duplex(hw, &speed, &duplex);
		if (ret_val) {
			hw_dbg("Error getting link speed and duplex\n");
			goto out;
		}

		if (duplex == HALF_DUPLEX)
			hw->fc.current_mode = igc_fc_none;

		 
		ret_val = igc_force_mac_fc(hw);
		if (ret_val) {
			hw_dbg("Error forcing flow control settings\n");
			goto out;
		}
	}

out:
	return ret_val;
}

 
s32 igc_get_auto_rd_done(struct igc_hw *hw)
{
	s32 ret_val = 0;
	s32 i = 0;

	while (i < AUTO_READ_DONE_TIMEOUT) {
		if (rd32(IGC_EECD) & IGC_EECD_AUTO_RD)
			break;
		usleep_range(1000, 2000);
		i++;
	}

	if (i == AUTO_READ_DONE_TIMEOUT) {
		hw_dbg("Auto read by HW from NVM has not completed.\n");
		ret_val = -IGC_ERR_RESET;
		goto out;
	}

out:
	return ret_val;
}

 
s32 igc_get_speed_and_duplex_copper(struct igc_hw *hw, u16 *speed,
				    u16 *duplex)
{
	u32 status;

	status = rd32(IGC_STATUS);
	if (status & IGC_STATUS_SPEED_1000) {
		 
		if (hw->mac.type == igc_i225 &&
		    (status & IGC_STATUS_SPEED_2500)) {
			*speed = SPEED_2500;
			hw_dbg("2500 Mbs, ");
		} else {
			*speed = SPEED_1000;
			hw_dbg("1000 Mbs, ");
		}
	} else if (status & IGC_STATUS_SPEED_100) {
		*speed = SPEED_100;
		hw_dbg("100 Mbs, ");
	} else {
		*speed = SPEED_10;
		hw_dbg("10 Mbs, ");
	}

	if (status & IGC_STATUS_FD) {
		*duplex = FULL_DUPLEX;
		hw_dbg("Full Duplex\n");
	} else {
		*duplex = HALF_DUPLEX;
		hw_dbg("Half Duplex\n");
	}

	return 0;
}

 
void igc_put_hw_semaphore(struct igc_hw *hw)
{
	u32 swsm;

	swsm = rd32(IGC_SWSM);

	swsm &= ~(IGC_SWSM_SMBI | IGC_SWSM_SWESMBI);

	wr32(IGC_SWSM, swsm);
}

 
bool igc_enable_mng_pass_thru(struct igc_hw *hw)
{
	bool ret_val = false;
	u32 fwsm, factps;
	u32 manc;

	if (!hw->mac.asf_firmware_present)
		goto out;

	manc = rd32(IGC_MANC);

	if (!(manc & IGC_MANC_RCV_TCO_EN))
		goto out;

	if (hw->mac.arc_subsystem_valid) {
		fwsm = rd32(IGC_FWSM);
		factps = rd32(IGC_FACTPS);

		if (!(factps & IGC_FACTPS_MNGCG) &&
		    ((fwsm & IGC_FWSM_MODE_MASK) ==
		    (igc_mng_mode_pt << IGC_FWSM_MODE_SHIFT))) {
			ret_val = true;
			goto out;
		}
	} else {
		if ((manc & IGC_MANC_SMBUS_EN) &&
		    !(manc & IGC_MANC_ASF_EN)) {
			ret_val = true;
			goto out;
		}
	}

out:
	return ret_val;
}

 
static u32 igc_hash_mc_addr(struct igc_hw *hw, u8 *mc_addr)
{
	u32 hash_value, hash_mask;
	u8 bit_shift = 0;

	 
	hash_mask = (hw->mac.mta_reg_count * 32) - 1;

	 
	while (hash_mask >> bit_shift != 0xFF)
		bit_shift++;

	 
	switch (hw->mac.mc_filter_type) {
	default:
	case 0:
		break;
	case 1:
		bit_shift += 1;
		break;
	case 2:
		bit_shift += 2;
		break;
	case 3:
		bit_shift += 4;
		break;
	}

	hash_value = hash_mask & (((mc_addr[4] >> (8 - bit_shift)) |
				  (((u16)mc_addr[5]) << bit_shift)));

	return hash_value;
}

 
void igc_update_mc_addr_list(struct igc_hw *hw,
			     u8 *mc_addr_list, u32 mc_addr_count)
{
	u32 hash_value, hash_bit, hash_reg;
	int i;

	 
	memset(&hw->mac.mta_shadow, 0, sizeof(hw->mac.mta_shadow));

	 
	for (i = 0; (u32)i < mc_addr_count; i++) {
		hash_value = igc_hash_mc_addr(hw, mc_addr_list);

		hash_reg = (hash_value >> 5) & (hw->mac.mta_reg_count - 1);
		hash_bit = hash_value & 0x1F;

		hw->mac.mta_shadow[hash_reg] |= BIT(hash_bit);
		mc_addr_list += ETH_ALEN;
	}

	 
	for (i = hw->mac.mta_reg_count - 1; i >= 0; i--)
		array_wr32(IGC_MTA, i, hw->mac.mta_shadow[i]);
	wrfl();
}

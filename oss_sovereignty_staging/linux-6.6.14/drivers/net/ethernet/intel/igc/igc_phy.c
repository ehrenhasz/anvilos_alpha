
 

#include "igc_phy.h"

 
s32 igc_check_reset_block(struct igc_hw *hw)
{
	u32 manc;

	manc = rd32(IGC_MANC);

	return (manc & IGC_MANC_BLK_PHY_RST_ON_IDE) ?
		IGC_ERR_BLK_PHY_RESET : 0;
}

 
s32 igc_get_phy_id(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;
	s32 ret_val = 0;
	u16 phy_id;

	ret_val = phy->ops.read_reg(hw, PHY_ID1, &phy_id);
	if (ret_val)
		goto out;

	phy->id = (u32)(phy_id << 16);
	usleep_range(200, 500);
	ret_val = phy->ops.read_reg(hw, PHY_ID2, &phy_id);
	if (ret_val)
		goto out;

	phy->id |= (u32)(phy_id & PHY_REVISION_MASK);
	phy->revision = (u32)(phy_id & ~PHY_REVISION_MASK);

out:
	return ret_val;
}

 
s32 igc_phy_has_link(struct igc_hw *hw, u32 iterations,
		     u32 usec_interval, bool *success)
{
	u16 i, phy_status;
	s32 ret_val = 0;

	for (i = 0; i < iterations; i++) {
		 
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val && usec_interval > 0) {
			 
			if (usec_interval >= 1000)
				mdelay(usec_interval / 1000);
			else
				udelay(usec_interval);
		}
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_LINK_STATUS)
			break;
		if (usec_interval >= 1000)
			mdelay(usec_interval / 1000);
		else
			udelay(usec_interval);
	}

	*success = (i < iterations) ? true : false;

	return ret_val;
}

 
void igc_power_up_phy_copper(struct igc_hw *hw)
{
	u16 mii_reg = 0;

	 
	hw->phy.ops.read_reg(hw, PHY_CONTROL, &mii_reg);
	mii_reg &= ~MII_CR_POWER_DOWN;
	hw->phy.ops.write_reg(hw, PHY_CONTROL, mii_reg);
}

 
void igc_power_down_phy_copper(struct igc_hw *hw)
{
	u16 mii_reg = 0;

	 
	hw->phy.ops.read_reg(hw, PHY_CONTROL, &mii_reg);
	mii_reg |= MII_CR_POWER_DOWN;

	 
	 
	usleep_range(1000, 2000);
}

 
void igc_check_downshift(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;

	 
	phy->speed_downgraded = false;
}

 
s32 igc_phy_hw_reset(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;
	u32 phpm = 0, timeout = 10000;
	s32  ret_val;
	u32 ctrl;

	ret_val = igc_check_reset_block(hw);
	if (ret_val) {
		ret_val = 0;
		goto out;
	}

	ret_val = phy->ops.acquire(hw);
	if (ret_val)
		goto out;

	phpm = rd32(IGC_I225_PHPM);

	ctrl = rd32(IGC_CTRL);
	wr32(IGC_CTRL, ctrl | IGC_CTRL_PHY_RST);
	wrfl();

	udelay(phy->reset_delay_us);

	wr32(IGC_CTRL, ctrl);
	wrfl();

	 
	usleep_range(100, 150);
	do {
		phpm = rd32(IGC_I225_PHPM);
		timeout--;
		udelay(1);
	} while (!(phpm & IGC_PHY_RST_COMP) && timeout);

	if (!timeout)
		hw_dbg("Timeout is expired after a phy reset\n");

	usleep_range(100, 150);

	phy->ops.release(hw);

out:
	return ret_val;
}

 
static s32 igc_phy_setup_autoneg(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;
	u16 aneg_multigbt_an_ctrl = 0;
	u16 mii_1000t_ctrl_reg = 0;
	u16 mii_autoneg_adv_reg;
	s32 ret_val;

	phy->autoneg_advertised &= phy->autoneg_mask;

	 
	ret_val = phy->ops.read_reg(hw, PHY_AUTONEG_ADV, &mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	if (phy->autoneg_mask & ADVERTISE_1000_FULL) {
		 
		ret_val = phy->ops.read_reg(hw, PHY_1000T_CTRL,
					    &mii_1000t_ctrl_reg);
		if (ret_val)
			return ret_val;
	}

	if (phy->autoneg_mask & ADVERTISE_2500_FULL) {
		 
		ret_val = phy->ops.read_reg(hw, (STANDARD_AN_REG_MASK <<
					    MMD_DEVADDR_SHIFT) |
					    ANEG_MULTIGBT_AN_CTRL,
					    &aneg_multigbt_an_ctrl);

		if (ret_val)
			return ret_val;
	}

	 

	 
	mii_autoneg_adv_reg &= ~(NWAY_AR_100TX_FD_CAPS |
				 NWAY_AR_100TX_HD_CAPS |
				 NWAY_AR_10T_FD_CAPS   |
				 NWAY_AR_10T_HD_CAPS);
	mii_1000t_ctrl_reg &= ~(CR_1000T_HD_CAPS | CR_1000T_FD_CAPS);

	hw_dbg("autoneg_advertised %x\n", phy->autoneg_advertised);

	 
	if (phy->autoneg_advertised & ADVERTISE_10_HALF) {
		hw_dbg("Advertise 10mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_HD_CAPS;
	}

	 
	if (phy->autoneg_advertised & ADVERTISE_10_FULL) {
		hw_dbg("Advertise 10mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_10T_FD_CAPS;
	}

	 
	if (phy->autoneg_advertised & ADVERTISE_100_HALF) {
		hw_dbg("Advertise 100mb Half duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_HD_CAPS;
	}

	 
	if (phy->autoneg_advertised & ADVERTISE_100_FULL) {
		hw_dbg("Advertise 100mb Full duplex\n");
		mii_autoneg_adv_reg |= NWAY_AR_100TX_FD_CAPS;
	}

	 
	if (phy->autoneg_advertised & ADVERTISE_1000_HALF)
		hw_dbg("Advertise 1000mb Half duplex request denied!\n");

	 
	if (phy->autoneg_advertised & ADVERTISE_1000_FULL) {
		hw_dbg("Advertise 1000mb Full duplex\n");
		mii_1000t_ctrl_reg |= CR_1000T_FD_CAPS;
	}

	 
	if (phy->autoneg_advertised & ADVERTISE_2500_HALF)
		hw_dbg("Advertise 2500mb Half duplex request denied!\n");

	 
	if (phy->autoneg_advertised & ADVERTISE_2500_FULL) {
		hw_dbg("Advertise 2500mb Full duplex\n");
		aneg_multigbt_an_ctrl |= CR_2500T_FD_CAPS;
	} else {
		aneg_multigbt_an_ctrl &= ~CR_2500T_FD_CAPS;
	}

	 
	switch (hw->fc.current_mode) {
	case igc_fc_none:
		 
		mii_autoneg_adv_reg &= ~(NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case igc_fc_rx_pause:
		 
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	case igc_fc_tx_pause:
		 
		mii_autoneg_adv_reg |= NWAY_AR_ASM_DIR;
		mii_autoneg_adv_reg &= ~NWAY_AR_PAUSE;
		break;
	case igc_fc_full:
		 
		mii_autoneg_adv_reg |= (NWAY_AR_ASM_DIR | NWAY_AR_PAUSE);
		break;
	default:
		hw_dbg("Flow control param set incorrectly\n");
		return -IGC_ERR_CONFIG;
	}

	ret_val = phy->ops.write_reg(hw, PHY_AUTONEG_ADV, mii_autoneg_adv_reg);
	if (ret_val)
		return ret_val;

	hw_dbg("Auto-Neg Advertising %x\n", mii_autoneg_adv_reg);

	if (phy->autoneg_mask & ADVERTISE_1000_FULL)
		ret_val = phy->ops.write_reg(hw, PHY_1000T_CTRL,
					     mii_1000t_ctrl_reg);

	if (phy->autoneg_mask & ADVERTISE_2500_FULL)
		ret_val = phy->ops.write_reg(hw,
					     (STANDARD_AN_REG_MASK <<
					     MMD_DEVADDR_SHIFT) |
					     ANEG_MULTIGBT_AN_CTRL,
					     aneg_multigbt_an_ctrl);

	return ret_val;
}

 
static s32 igc_wait_autoneg(struct igc_hw *hw)
{
	u16 i, phy_status;
	s32 ret_val = 0;

	 
	for (i = PHY_AUTO_NEG_LIMIT; i > 0; i--) {
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		ret_val = hw->phy.ops.read_reg(hw, PHY_STATUS, &phy_status);
		if (ret_val)
			break;
		if (phy_status & MII_SR_AUTONEG_COMPLETE)
			break;
		msleep(100);
	}

	 
	return ret_val;
}

 
static s32 igc_copper_link_autoneg(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;
	u16 phy_ctrl;
	s32 ret_val;

	 
	phy->autoneg_advertised &= phy->autoneg_mask;

	 
	if (phy->autoneg_advertised == 0)
		phy->autoneg_advertised = phy->autoneg_mask;

	hw_dbg("Reconfiguring auto-neg advertisement params\n");
	ret_val = igc_phy_setup_autoneg(hw);
	if (ret_val) {
		hw_dbg("Error Setting up Auto-Negotiation\n");
		goto out;
	}
	hw_dbg("Restarting Auto-Neg\n");

	 
	ret_val = phy->ops.read_reg(hw, PHY_CONTROL, &phy_ctrl);
	if (ret_val)
		goto out;

	phy_ctrl |= (MII_CR_AUTO_NEG_EN | MII_CR_RESTART_AUTO_NEG);
	ret_val = phy->ops.write_reg(hw, PHY_CONTROL, phy_ctrl);
	if (ret_val)
		goto out;

	 
	if (phy->autoneg_wait_to_complete) {
		ret_val = igc_wait_autoneg(hw);
		if (ret_val) {
			hw_dbg("Error while waiting for autoneg to complete\n");
			goto out;
		}
	}

	hw->mac.get_link_status = true;

out:
	return ret_val;
}

 
s32 igc_setup_copper_link(struct igc_hw *hw)
{
	s32 ret_val = 0;
	bool link;

	if (hw->mac.autoneg) {
		 
		ret_val = igc_copper_link_autoneg(hw);
		if (ret_val)
			goto out;
	} else {
		 
		hw_dbg("Forcing Speed and Duplex\n");
		ret_val = hw->phy.ops.force_speed_duplex(hw);
		if (ret_val) {
			hw_dbg("Error Forcing Speed and Duplex\n");
			goto out;
		}
	}

	 
	ret_val = igc_phy_has_link(hw, COPPER_LINK_UP_LIMIT, 10, &link);
	if (ret_val)
		goto out;

	if (link) {
		hw_dbg("Valid link established!!!\n");
		igc_config_collision_dist(hw);
		ret_val = igc_config_fc_after_link_up(hw);
	} else {
		hw_dbg("Unable to establish link!!!\n");
	}

out:
	return ret_val;
}

 
static s32 igc_read_phy_reg_mdic(struct igc_hw *hw, u32 offset, u16 *data)
{
	struct igc_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;
	s32 ret_val = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		hw_dbg("PHY Address %d is out of range\n", offset);
		ret_val = -IGC_ERR_PARAM;
		goto out;
	}

	 
	mdic = ((offset << IGC_MDIC_REG_SHIFT) |
		(phy->addr << IGC_MDIC_PHY_SHIFT) |
		(IGC_MDIC_OP_READ));

	wr32(IGC_MDIC, mdic);

	 
	for (i = 0; i < IGC_GEN_POLL_TIMEOUT; i++) {
		udelay(50);
		mdic = rd32(IGC_MDIC);
		if (mdic & IGC_MDIC_READY)
			break;
	}
	if (!(mdic & IGC_MDIC_READY)) {
		hw_dbg("MDI Read did not complete\n");
		ret_val = -IGC_ERR_PHY;
		goto out;
	}
	if (mdic & IGC_MDIC_ERROR) {
		hw_dbg("MDI Error\n");
		ret_val = -IGC_ERR_PHY;
		goto out;
	}
	*data = (u16)mdic;

out:
	return ret_val;
}

 
static s32 igc_write_phy_reg_mdic(struct igc_hw *hw, u32 offset, u16 data)
{
	struct igc_phy_info *phy = &hw->phy;
	u32 i, mdic = 0;
	s32 ret_val = 0;

	if (offset > MAX_PHY_REG_ADDRESS) {
		hw_dbg("PHY Address %d is out of range\n", offset);
		ret_val = -IGC_ERR_PARAM;
		goto out;
	}

	 
	mdic = (((u32)data) |
		(offset << IGC_MDIC_REG_SHIFT) |
		(phy->addr << IGC_MDIC_PHY_SHIFT) |
		(IGC_MDIC_OP_WRITE));

	wr32(IGC_MDIC, mdic);

	 
	for (i = 0; i < IGC_GEN_POLL_TIMEOUT; i++) {
		udelay(50);
		mdic = rd32(IGC_MDIC);
		if (mdic & IGC_MDIC_READY)
			break;
	}
	if (!(mdic & IGC_MDIC_READY)) {
		hw_dbg("MDI Write did not complete\n");
		ret_val = -IGC_ERR_PHY;
		goto out;
	}
	if (mdic & IGC_MDIC_ERROR) {
		hw_dbg("MDI Error\n");
		ret_val = -IGC_ERR_PHY;
		goto out;
	}

out:
	return ret_val;
}

 
static s32 __igc_access_xmdio_reg(struct igc_hw *hw, u16 address,
				  u8 dev_addr, u16 *data, bool read)
{
	s32 ret_val;

	ret_val = hw->phy.ops.write_reg(hw, IGC_MMDAC, dev_addr);
	if (ret_val)
		return ret_val;

	ret_val = hw->phy.ops.write_reg(hw, IGC_MMDAAD, address);
	if (ret_val)
		return ret_val;

	ret_val = hw->phy.ops.write_reg(hw, IGC_MMDAC, IGC_MMDAC_FUNC_DATA |
					dev_addr);
	if (ret_val)
		return ret_val;

	if (read)
		ret_val = hw->phy.ops.read_reg(hw, IGC_MMDAAD, data);
	else
		ret_val = hw->phy.ops.write_reg(hw, IGC_MMDAAD, *data);
	if (ret_val)
		return ret_val;

	 
	ret_val = hw->phy.ops.write_reg(hw, IGC_MMDAC, 0);
	if (ret_val)
		return ret_val;

	return ret_val;
}

 
static s32 igc_read_xmdio_reg(struct igc_hw *hw, u16 addr,
			      u8 dev_addr, u16 *data)
{
	return __igc_access_xmdio_reg(hw, addr, dev_addr, data, true);
}

 
static s32 igc_write_xmdio_reg(struct igc_hw *hw, u16 addr,
			       u8 dev_addr, u16 data)
{
	return __igc_access_xmdio_reg(hw, addr, dev_addr, &data, false);
}

 
s32 igc_write_phy_reg_gpy(struct igc_hw *hw, u32 offset, u16 data)
{
	u8 dev_addr = (offset & GPY_MMD_MASK) >> GPY_MMD_SHIFT;
	s32 ret_val;

	offset = offset & GPY_REG_MASK;

	if (!dev_addr) {
		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
		ret_val = igc_write_phy_reg_mdic(hw, offset, data);
		hw->phy.ops.release(hw);
	} else {
		ret_val = igc_write_xmdio_reg(hw, (u16)offset, dev_addr,
					      data);
	}

	return ret_val;
}

 
s32 igc_read_phy_reg_gpy(struct igc_hw *hw, u32 offset, u16 *data)
{
	u8 dev_addr = (offset & GPY_MMD_MASK) >> GPY_MMD_SHIFT;
	s32 ret_val;

	offset = offset & GPY_REG_MASK;

	if (!dev_addr) {
		ret_val = hw->phy.ops.acquire(hw);
		if (ret_val)
			return ret_val;
		ret_val = igc_read_phy_reg_mdic(hw, offset, data);
		hw->phy.ops.release(hw);
	} else {
		ret_val = igc_read_xmdio_reg(hw, (u16)offset, dev_addr,
					     data);
	}

	return ret_val;
}

 
u16 igc_read_phy_fw_version(struct igc_hw *hw)
{
	struct igc_phy_info *phy = &hw->phy;
	u16 gphy_version = 0;
	u16 ret_val;

	 
	ret_val = phy->ops.read_reg(hw, IGC_GPHY_VERSION, &gphy_version);
	if (ret_val)
		hw_dbg("igc_phy: read wrong gphy version\n");

	return gphy_version;
}

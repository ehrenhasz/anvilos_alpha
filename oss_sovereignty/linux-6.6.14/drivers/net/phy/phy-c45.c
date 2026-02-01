
 
#include <linux/ethtool.h>
#include <linux/export.h>
#include <linux/mdio.h>
#include <linux/mii.h>
#include <linux/phy.h>

#include "mdio-open-alliance.h"

 
static bool genphy_c45_baset1_able(struct phy_device *phydev)
{
	int val;

	if (phydev->pma_extable == -ENODATA) {
		val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_EXTABLE);
		if (val < 0)
			return false;

		phydev->pma_extable = val;
	}

	return !!(phydev->pma_extable & MDIO_PMA_EXTABLE_BT1);
}

 
static bool genphy_c45_pma_can_sleep(struct phy_device *phydev)
{
	int stat1;

	stat1 = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_STAT1);
	if (stat1 < 0)
		return false;

	return !!(stat1 & MDIO_STAT1_LPOWERABLE);
}

 
int genphy_c45_pma_resume(struct phy_device *phydev)
{
	if (!genphy_c45_pma_can_sleep(phydev))
		return -EOPNOTSUPP;

	return phy_clear_bits_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1,
				  MDIO_CTRL1_LPOWER);
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_resume);

 
int genphy_c45_pma_suspend(struct phy_device *phydev)
{
	if (!genphy_c45_pma_can_sleep(phydev))
		return -EOPNOTSUPP;

	return phy_set_bits_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1,
				MDIO_CTRL1_LPOWER);
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_suspend);

 
int genphy_c45_pma_baset1_setup_master_slave(struct phy_device *phydev)
{
	int ctl = 0;

	switch (phydev->master_slave_set) {
	case MASTER_SLAVE_CFG_MASTER_PREFERRED:
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		ctl = MDIO_PMA_PMD_BT1_CTRL_CFG_MST;
		break;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
	case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
		break;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
		return 0;
	default:
		phydev_warn(phydev, "Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	return phy_modify_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL,
			     MDIO_PMA_PMD_BT1_CTRL_CFG_MST, ctl);
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_baset1_setup_master_slave);

 
int genphy_c45_pma_setup_forced(struct phy_device *phydev)
{
	int bt1_ctrl, ctrl1, ctrl2, ret;

	 
	if (phydev->duplex != DUPLEX_FULL)
		return -EINVAL;

	ctrl1 = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1);
	if (ctrl1 < 0)
		return ctrl1;

	ctrl2 = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL2);
	if (ctrl2 < 0)
		return ctrl2;

	ctrl1 &= ~MDIO_CTRL1_SPEEDSEL;
	 
	ctrl2 &= ~(MDIO_PMA_CTRL2_TYPE | 0x30);

	switch (phydev->speed) {
	case SPEED_10:
		if (genphy_c45_baset1_able(phydev))
			ctrl2 |= MDIO_PMA_CTRL2_BASET1;
		else
			ctrl2 |= MDIO_PMA_CTRL2_10BT;
		break;
	case SPEED_100:
		ctrl1 |= MDIO_PMA_CTRL1_SPEED100;
		ctrl2 |= MDIO_PMA_CTRL2_100BTX;
		break;
	case SPEED_1000:
		ctrl1 |= MDIO_PMA_CTRL1_SPEED1000;
		 
		ctrl2 |= MDIO_PMA_CTRL2_1000BT;
		break;
	case SPEED_2500:
		ctrl1 |= MDIO_CTRL1_SPEED2_5G;
		 
		ctrl2 |= MDIO_PMA_CTRL2_2_5GBT;
		break;
	case SPEED_5000:
		ctrl1 |= MDIO_CTRL1_SPEED5G;
		 
		ctrl2 |= MDIO_PMA_CTRL2_5GBT;
		break;
	case SPEED_10000:
		ctrl1 |= MDIO_CTRL1_SPEED10G;
		 
		ctrl2 |= MDIO_PMA_CTRL2_10GBT;
		break;
	default:
		return -EINVAL;
	}

	ret = phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1, ctrl1);
	if (ret < 0)
		return ret;

	ret = phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL2, ctrl2);
	if (ret < 0)
		return ret;

	if (genphy_c45_baset1_able(phydev)) {
		ret = genphy_c45_pma_baset1_setup_master_slave(phydev);
		if (ret < 0)
			return ret;

		bt1_ctrl = 0;
		if (phydev->speed == SPEED_1000)
			bt1_ctrl = MDIO_PMA_PMD_BT1_CTRL_STRAP_B1000;

		ret = phy_modify_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL,
				     MDIO_PMA_PMD_BT1_CTRL_STRAP, bt1_ctrl);
		if (ret < 0)
			return ret;
	}

	return genphy_c45_an_disable_aneg(phydev);
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_setup_forced);

 
static int genphy_c45_baset1_an_config_aneg(struct phy_device *phydev)
{
	u16 adv_l_mask, adv_l = 0;
	u16 adv_m_mask, adv_m = 0;
	int changed = 0;
	int ret;

	adv_l_mask = MDIO_AN_T1_ADV_L_FORCE_MS | MDIO_AN_T1_ADV_L_PAUSE_CAP |
		MDIO_AN_T1_ADV_L_PAUSE_ASYM;
	adv_m_mask = MDIO_AN_T1_ADV_M_MST | MDIO_AN_T1_ADV_M_B10L;

	switch (phydev->master_slave_set) {
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		adv_m |= MDIO_AN_T1_ADV_M_MST;
		fallthrough;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
		adv_l |= MDIO_AN_T1_ADV_L_FORCE_MS;
		break;
	case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		adv_m |= MDIO_AN_T1_ADV_M_MST;
		fallthrough;
	case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
		break;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
		 
		adv_l_mask &= ~MDIO_AN_T1_ADV_L_FORCE_MS;
		adv_m_mask &= ~MDIO_AN_T1_ADV_M_MST;
		break;
	default:
		phydev_warn(phydev, "Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	adv_l |= linkmode_adv_to_mii_t1_adv_l_t(phydev->advertising);

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_T1_ADV_L,
				     adv_l_mask, adv_l);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = 1;

	adv_m |= linkmode_adv_to_mii_t1_adv_m_t(phydev->advertising);

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_T1_ADV_M,
				     adv_m_mask, adv_m);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = 1;

	return changed;
}

 
int genphy_c45_an_config_aneg(struct phy_device *phydev)
{
	int changed = 0, ret;
	u32 adv;

	linkmode_and(phydev->advertising, phydev->advertising,
		     phydev->supported);

	ret = genphy_c45_an_config_eee_aneg(phydev);
	if (ret < 0)
		return ret;
	else if (ret)
		changed = true;

	if (genphy_c45_baset1_able(phydev))
		return genphy_c45_baset1_an_config_aneg(phydev);

	adv = linkmode_adv_to_mii_adv_t(phydev->advertising);

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_ADVERTISE,
				     ADVERTISE_ALL | ADVERTISE_100BASE4 |
				     ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM,
				     adv);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = 1;

	adv = linkmode_adv_to_mii_10gbt_adv_t(phydev->advertising);

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL,
				     MDIO_AN_10GBT_CTRL_ADV10G |
				     MDIO_AN_10GBT_CTRL_ADV5G |
				     MDIO_AN_10GBT_CTRL_ADV2_5G, adv);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = 1;

	return changed;
}
EXPORT_SYMBOL_GPL(genphy_c45_an_config_aneg);

 
int genphy_c45_an_disable_aneg(struct phy_device *phydev)
{
	u16 reg = MDIO_CTRL1;

	if (genphy_c45_baset1_able(phydev))
		reg = MDIO_AN_T1_CTRL;

	return phy_clear_bits_mmd(phydev, MDIO_MMD_AN, reg,
				  MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART);
}
EXPORT_SYMBOL_GPL(genphy_c45_an_disable_aneg);

 
int genphy_c45_restart_aneg(struct phy_device *phydev)
{
	u16 reg = MDIO_CTRL1;

	if (genphy_c45_baset1_able(phydev))
		reg = MDIO_AN_T1_CTRL;

	return phy_set_bits_mmd(phydev, MDIO_MMD_AN, reg,
				MDIO_AN_CTRL1_ENABLE | MDIO_AN_CTRL1_RESTART);
}
EXPORT_SYMBOL_GPL(genphy_c45_restart_aneg);

 
int genphy_c45_check_and_restart_aneg(struct phy_device *phydev, bool restart)
{
	u16 reg = MDIO_CTRL1;
	int ret;

	if (genphy_c45_baset1_able(phydev))
		reg = MDIO_AN_T1_CTRL;

	if (!restart) {
		 
		ret = phy_read_mmd(phydev, MDIO_MMD_AN, reg);
		if (ret < 0)
			return ret;

		if (!(ret & MDIO_AN_CTRL1_ENABLE))
			restart = true;
	}

	if (restart)
		return genphy_c45_restart_aneg(phydev);

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_check_and_restart_aneg);

 
int genphy_c45_aneg_done(struct phy_device *phydev)
{
	int reg = MDIO_STAT1;
	int val;

	if (genphy_c45_baset1_able(phydev))
		reg = MDIO_AN_T1_STAT;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, reg);

	return val < 0 ? val : val & MDIO_AN_STAT1_COMPLETE ? 1 : 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_aneg_done);

 
int genphy_c45_read_link(struct phy_device *phydev)
{
	u32 mmd_mask = MDIO_DEVS_PMAPMD;
	int val, devad;
	bool link = true;

	if (phydev->c45_ids.mmds_present & MDIO_DEVS_AN) {
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1);
		if (val < 0)
			return val;

		 
		if (val & MDIO_AN_CTRL1_RESTART) {
			phydev->link = 0;
			return 0;
		}
	}

	while (mmd_mask && link) {
		devad = __ffs(mmd_mask);
		mmd_mask &= ~BIT(devad);

		 
		if (!phy_polling_mode(phydev) || !phydev->link) {
			val = phy_read_mmd(phydev, devad, MDIO_STAT1);
			if (val < 0)
				return val;
			else if (val & MDIO_STAT1_LSTATUS)
				continue;
		}

		val = phy_read_mmd(phydev, devad, MDIO_STAT1);
		if (val < 0)
			return val;

		if (!(val & MDIO_STAT1_LSTATUS))
			link = false;
	}

	phydev->link = link;

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_read_link);

 
static int genphy_c45_baset1_read_lpa(struct phy_device *phydev)
{
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_STAT);
	if (val < 0)
		return val;

	if (!(val & MDIO_AN_STAT1_COMPLETE)) {
		linkmode_clear_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->lp_advertising);
		mii_t1_adv_l_mod_linkmode_t(phydev->lp_advertising, 0);
		mii_t1_adv_m_mod_linkmode_t(phydev->lp_advertising, 0);

		phydev->pause = 0;
		phydev->asym_pause = 0;

		return 0;
	}

	linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->lp_advertising, 1);

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_LP_L);
	if (val < 0)
		return val;

	mii_t1_adv_l_mod_linkmode_t(phydev->lp_advertising, val);
	phydev->pause = val & MDIO_AN_T1_ADV_L_PAUSE_CAP ? 1 : 0;
	phydev->asym_pause = val & MDIO_AN_T1_ADV_L_PAUSE_ASYM ? 1 : 0;

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_LP_M);
	if (val < 0)
		return val;

	mii_t1_adv_m_mod_linkmode_t(phydev->lp_advertising, val);

	return 0;
}

 
int genphy_c45_read_lpa(struct phy_device *phydev)
{
	int val;

	if (genphy_c45_baset1_able(phydev))
		return genphy_c45_baset1_read_lpa(phydev);

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_STAT1);
	if (val < 0)
		return val;

	if (!(val & MDIO_AN_STAT1_COMPLETE)) {
		linkmode_clear_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
				   phydev->lp_advertising);
		mii_10gbt_stat_mod_linkmode_lpa_t(phydev->lp_advertising, 0);
		mii_adv_mod_linkmode_adv_t(phydev->lp_advertising, 0);
		phydev->pause = 0;
		phydev->asym_pause = 0;

		return 0;
	}

	linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->lp_advertising,
			 val & MDIO_AN_STAT1_LPABLE);

	 
	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_LPA);
	if (val < 0)
		return val;

	mii_adv_mod_linkmode_adv_t(phydev->lp_advertising, val);
	phydev->pause = val & LPA_PAUSE_CAP ? 1 : 0;
	phydev->asym_pause = val & LPA_PAUSE_ASYM ? 1 : 0;

	 
	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_STAT);
	if (val < 0)
		return val;

	mii_10gbt_stat_mod_linkmode_lpa_t(phydev->lp_advertising, val);

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_read_lpa);

 
int genphy_c45_pma_baset1_read_master_slave(struct phy_device *phydev)
{
	int val;

	phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;
	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;

	val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL);
	if (val < 0)
		return val;

	if (val & MDIO_PMA_PMD_BT1_CTRL_CFG_MST) {
		phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_FORCE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_MASTER;
	} else {
		phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_FORCE;
		phydev->master_slave_state = MASTER_SLAVE_STATE_SLAVE;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_baset1_read_master_slave);

 
int genphy_c45_read_pma(struct phy_device *phydev)
{
	int val;

	linkmode_zero(phydev->lp_advertising);

	val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1);
	if (val < 0)
		return val;

	switch (val & MDIO_CTRL1_SPEEDSEL) {
	case 0:
		phydev->speed = SPEED_10;
		break;
	case MDIO_PMA_CTRL1_SPEED100:
		phydev->speed = SPEED_100;
		break;
	case MDIO_PMA_CTRL1_SPEED1000:
		phydev->speed = SPEED_1000;
		break;
	case MDIO_CTRL1_SPEED2_5G:
		phydev->speed = SPEED_2500;
		break;
	case MDIO_CTRL1_SPEED5G:
		phydev->speed = SPEED_5000;
		break;
	case MDIO_CTRL1_SPEED10G:
		phydev->speed = SPEED_10000;
		break;
	default:
		phydev->speed = SPEED_UNKNOWN;
		break;
	}

	phydev->duplex = DUPLEX_FULL;

	if (genphy_c45_baset1_able(phydev)) {
		val = genphy_c45_pma_baset1_read_master_slave(phydev);
		if (val < 0)
			return val;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_read_pma);

 
int genphy_c45_read_mdix(struct phy_device *phydev)
{
	int val;

	if (phydev->speed == SPEED_10000) {
		val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD,
				   MDIO_PMA_10GBT_SWAPPOL);
		if (val < 0)
			return val;

		switch (val) {
		case MDIO_PMA_10GBT_SWAPPOL_ABNX | MDIO_PMA_10GBT_SWAPPOL_CDNX:
			phydev->mdix = ETH_TP_MDI;
			break;

		case 0:
			phydev->mdix = ETH_TP_MDI_X;
			break;

		default:
			phydev->mdix = ETH_TP_MDI_INVALID;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_read_mdix);

 
int genphy_c45_write_eee_adv(struct phy_device *phydev, unsigned long *adv)
{
	int val, changed = 0;

	if (linkmode_intersects(phydev->supported_eee, PHY_EEE_CAP1_FEATURES)) {
		val = linkmode_to_mii_eee_cap1_t(adv);

		 
		val &= ~phydev->eee_broken_modes;

		 
		val = phy_modify_mmd_changed(phydev, MDIO_MMD_AN,
					     MDIO_AN_EEE_ADV,
					     MDIO_EEE_100TX | MDIO_EEE_1000T |
					     MDIO_EEE_10GT | MDIO_EEE_1000KX |
					     MDIO_EEE_10GKX4 | MDIO_EEE_10GKR,
					     val);
		if (val < 0)
			return val;
		if (val > 0)
			changed = 1;
	}

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
			      phydev->supported_eee)) {
		val = linkmode_adv_to_mii_10base_t1_t(adv);
		 
		val = phy_modify_mmd_changed(phydev, MDIO_MMD_AN,
					     MDIO_AN_10BT1_AN_CTRL,
					     MDIO_AN_10BT1_AN_CTRL_ADV_EEE_T1L,
					     val);
		if (val < 0)
			return val;
		if (val > 0)
			changed = 1;
	}

	return changed;
}

 
int genphy_c45_read_eee_adv(struct phy_device *phydev, unsigned long *adv)
{
	int val;

	if (linkmode_intersects(phydev->supported_eee, PHY_EEE_CAP1_FEATURES)) {
		 
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV);
		if (val < 0)
			return val;

		mii_eee_cap1_mod_linkmode_t(adv, val);
	}

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
			      phydev->supported_eee)) {
		 
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10BT1_AN_CTRL);
		if (val < 0)
			return val;

		mii_10base_t1_adv_mod_linkmode_t(adv, val);
	}

	return 0;
}

 
static int genphy_c45_read_eee_lpa(struct phy_device *phydev,
				   unsigned long *lpa)
{
	int val;

	if (linkmode_intersects(phydev->supported_eee, PHY_EEE_CAP1_FEATURES)) {
		 
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_EEE_LPABLE);
		if (val < 0)
			return val;

		mii_eee_cap1_mod_linkmode_t(lpa, val);
	}

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
			      phydev->supported_eee)) {
		 
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10BT1_AN_STAT);
		if (val < 0)
			return val;

		mii_10base_t1_adv_mod_linkmode_t(lpa, val);
	}

	return 0;
}

 
static int genphy_c45_read_eee_cap1(struct phy_device *phydev)
{
	int val;

	 
	val = phy_read_mmd(phydev, MDIO_MMD_PCS, MDIO_PCS_EEE_ABLE);
	if (val < 0)
		return val;

	 
	if (val == 0xffff)
		return 0;

	mii_eee_cap1_mod_linkmode_t(phydev->supported_eee, val);

	 
	linkmode_and(phydev->supported_eee, phydev->supported_eee,
		     phydev->supported);

	return 0;
}

 
int genphy_c45_read_eee_abilities(struct phy_device *phydev)
{
	int val;

	 
	if (linkmode_intersects(phydev->supported, PHY_EEE_CAP1_FEATURES)) {
		val = genphy_c45_read_eee_cap1(phydev);
		if (val)
			return val;
	}

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
			      phydev->supported)) {
		 
		val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10T1L_STAT);
		if (val < 0)
			return val;

		linkmode_mod_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
				 phydev->supported_eee,
				 val & MDIO_PMA_10T1L_STAT_EEE);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_read_eee_abilities);

 
int genphy_c45_an_config_eee_aneg(struct phy_device *phydev)
{
	if (!phydev->eee_enabled) {
		__ETHTOOL_DECLARE_LINK_MODE_MASK(adv) = {};

		return genphy_c45_write_eee_adv(phydev, adv);
	}

	return genphy_c45_write_eee_adv(phydev, phydev->advertising_eee);
}

 
int genphy_c45_pma_baset1_read_abilities(struct phy_device *phydev)
{
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1);
	if (val < 0)
		return val;

	linkmode_mod_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
			 phydev->supported,
			 val & MDIO_PMA_PMD_BT1_B10L_ABLE);

	linkmode_mod_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT,
			 phydev->supported,
			 val & MDIO_PMA_PMD_BT1_B100_ABLE);

	linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT,
			 phydev->supported,
			 val & MDIO_PMA_PMD_BT1_B1000_ABLE);

	val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_STAT);
	if (val < 0)
		return val;

	linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
			 phydev->supported,
			 val & MDIO_AN_STAT1_ABLE);

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_baset1_read_abilities);

 
int genphy_c45_pma_read_abilities(struct phy_device *phydev)
{
	int val;

	linkmode_clear_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->supported);
	if (phydev->c45_ids.mmds_present & MDIO_DEVS_AN) {
		val = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_STAT1);
		if (val < 0)
			return val;

		if (val & MDIO_AN_STAT1_ABLE)
			linkmode_set_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
					 phydev->supported);
	}

	val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_STAT2);
	if (val < 0)
		return val;

	linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT,
			 phydev->supported,
			 val & MDIO_PMA_STAT2_10GBSR);

	linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseLR_Full_BIT,
			 phydev->supported,
			 val & MDIO_PMA_STAT2_10GBLR);

	linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseER_Full_BIT,
			 phydev->supported,
			 val & MDIO_PMA_STAT2_10GBER);

	if (val & MDIO_PMA_STAT2_EXTABLE) {
		val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_EXTABLE);
		if (val < 0)
			return val;

		linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_10GBLRM);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_10GBT);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_10GBKX4);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_10GBKR);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_1000BT);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_1000BKX);

		linkmode_mod_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_100BTX);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_100BTX);

		linkmode_mod_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_10BT);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT,
				 phydev->supported,
				 val & MDIO_PMA_EXTABLE_10BT);

		if (val & MDIO_PMA_EXTABLE_NBT) {
			val = phy_read_mmd(phydev, MDIO_MMD_PMAPMD,
					   MDIO_PMA_NG_EXTABLE);
			if (val < 0)
				return val;

			linkmode_mod_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
					 phydev->supported,
					 val & MDIO_PMA_NG_EXTABLE_2_5GBT);

			linkmode_mod_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
					 phydev->supported,
					 val & MDIO_PMA_NG_EXTABLE_5GBT);
		}

		if (val & MDIO_PMA_EXTABLE_BT1) {
			val = genphy_c45_pma_baset1_read_abilities(phydev);
			if (val < 0)
				return val;
		}
	}

	 
	genphy_c45_read_eee_abilities(phydev);

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_pma_read_abilities);

 
int genphy_c45_baset1_read_status(struct phy_device *phydev)
{
	int ret;
	int cfg;

	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;
	phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_ADV_L);
	if (ret < 0)
		return ret;

	cfg = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_T1_ADV_M);
	if (cfg < 0)
		return cfg;

	if (ret & MDIO_AN_T1_ADV_L_FORCE_MS) {
		if (cfg & MDIO_AN_T1_ADV_M_MST)
			phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_FORCE;
		else
			phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_FORCE;
	} else {
		if (cfg & MDIO_AN_T1_ADV_M_MST)
			phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_PREFERRED;
		else
			phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_PREFERRED;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_baset1_read_status);

 
int genphy_c45_read_status(struct phy_device *phydev)
{
	int ret;

	ret = genphy_c45_read_link(phydev);
	if (ret)
		return ret;

	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = genphy_c45_read_lpa(phydev);
		if (ret)
			return ret;

		if (genphy_c45_baset1_able(phydev)) {
			ret = genphy_c45_baset1_read_status(phydev);
			if (ret < 0)
				return ret;
		}

		phy_resolve_aneg_linkmode(phydev);
	} else {
		ret = genphy_c45_read_pma(phydev);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(genphy_c45_read_status);

 
int genphy_c45_config_aneg(struct phy_device *phydev)
{
	bool changed = false;
	int ret;

	if (phydev->autoneg == AUTONEG_DISABLE)
		return genphy_c45_pma_setup_forced(phydev);

	ret = genphy_c45_an_config_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	return genphy_c45_check_and_restart_aneg(phydev, changed);
}
EXPORT_SYMBOL_GPL(genphy_c45_config_aneg);

 

int gen10g_config_aneg(struct phy_device *phydev)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gen10g_config_aneg);

int genphy_c45_loopback(struct phy_device *phydev, bool enable)
{
	return phy_modify_mmd(phydev, MDIO_MMD_PCS, MDIO_CTRL1,
			      MDIO_PCS_CTRL1_LOOPBACK,
			      enable ? MDIO_PCS_CTRL1_LOOPBACK : 0);
}
EXPORT_SYMBOL_GPL(genphy_c45_loopback);

 
int genphy_c45_fast_retrain(struct phy_device *phydev, bool enable)
{
	int ret;

	if (!enable)
		return phy_clear_bits_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FSRT_CSR,
				MDIO_PMA_10GBR_FSRT_ENABLE);

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, phydev->supported)) {
		ret = phy_set_bits_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL,
				MDIO_AN_10GBT_CTRL_ADVFSRT2_5G);
		if (ret)
			return ret;

		ret = phy_set_bits_mmd(phydev, MDIO_MMD_AN, MDIO_AN_CTRL2,
				MDIO_AN_THP_BP2_5GT);
		if (ret)
			return ret;
	}

	return phy_set_bits_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_PMA_10GBR_FSRT_CSR,
			MDIO_PMA_10GBR_FSRT_ENABLE);
}
EXPORT_SYMBOL_GPL(genphy_c45_fast_retrain);

 
int genphy_c45_plca_get_cfg(struct phy_device *phydev,
			    struct phy_plca_cfg *plca_cfg)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MDIO_OATC14_PLCA_IDVER);
	if (ret < 0)
		return ret;

	if ((ret & MDIO_OATC14_PLCA_IDM) != OATC14_IDM)
		return -ENODEV;

	plca_cfg->version = ret & ~MDIO_OATC14_PLCA_IDM;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MDIO_OATC14_PLCA_CTRL0);
	if (ret < 0)
		return ret;

	plca_cfg->enabled = !!(ret & MDIO_OATC14_PLCA_EN);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MDIO_OATC14_PLCA_CTRL1);
	if (ret < 0)
		return ret;

	plca_cfg->node_cnt = (ret & MDIO_OATC14_PLCA_NCNT) >> 8;
	plca_cfg->node_id = (ret & MDIO_OATC14_PLCA_ID);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MDIO_OATC14_PLCA_TOTMR);
	if (ret < 0)
		return ret;

	plca_cfg->to_tmr = ret & MDIO_OATC14_PLCA_TOT;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MDIO_OATC14_PLCA_BURST);
	if (ret < 0)
		return ret;

	plca_cfg->burst_cnt = (ret & MDIO_OATC14_PLCA_MAXBC) >> 8;
	plca_cfg->burst_tmr = (ret & MDIO_OATC14_PLCA_BTMR);

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_plca_get_cfg);

 
int genphy_c45_plca_set_cfg(struct phy_device *phydev,
			    const struct phy_plca_cfg *plca_cfg)
{
	u16 val = 0;
	int ret;

	
	if (plca_cfg->version >= 0)
		return -EINVAL;

	
	if (plca_cfg->enabled == 0) {
		ret = phy_clear_bits_mmd(phydev, MDIO_MMD_VEND2,
					 MDIO_OATC14_PLCA_CTRL0,
					 MDIO_OATC14_PLCA_EN);

		if (ret < 0)
			return ret;
	}

	
	if (plca_cfg->node_cnt >= 0 || plca_cfg->node_id >= 0) {
		 
		if (plca_cfg->node_cnt < 0 || plca_cfg->node_id < 0) {
			ret = phy_read_mmd(phydev, MDIO_MMD_VEND2,
					   MDIO_OATC14_PLCA_CTRL1);

			if (ret < 0)
				return ret;

			val = ret;
		}

		if (plca_cfg->node_cnt >= 0)
			val = (val & ~MDIO_OATC14_PLCA_NCNT) |
			      (plca_cfg->node_cnt << 8);

		if (plca_cfg->node_id >= 0)
			val = (val & ~MDIO_OATC14_PLCA_ID) |
			      (plca_cfg->node_id);

		ret = phy_write_mmd(phydev, MDIO_MMD_VEND2,
				    MDIO_OATC14_PLCA_CTRL1, val);

		if (ret < 0)
			return ret;
	}

	if (plca_cfg->to_tmr >= 0) {
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND2,
				    MDIO_OATC14_PLCA_TOTMR,
				    plca_cfg->to_tmr);

		if (ret < 0)
			return ret;
	}

	
	if (plca_cfg->burst_cnt >= 0 || plca_cfg->burst_tmr >= 0) {
		 
		if (plca_cfg->burst_cnt < 0 || plca_cfg->burst_tmr < 0) {
			ret = phy_read_mmd(phydev, MDIO_MMD_VEND2,
					   MDIO_OATC14_PLCA_BURST);

			if (ret < 0)
				return ret;

			val = ret;
		}

		if (plca_cfg->burst_cnt >= 0)
			val = (val & ~MDIO_OATC14_PLCA_MAXBC) |
			      (plca_cfg->burst_cnt << 8);

		if (plca_cfg->burst_tmr >= 0)
			val = (val & ~MDIO_OATC14_PLCA_BTMR) |
			      (plca_cfg->burst_tmr);

		ret = phy_write_mmd(phydev, MDIO_MMD_VEND2,
				    MDIO_OATC14_PLCA_BURST, val);

		if (ret < 0)
			return ret;
	}

	
	if (plca_cfg->enabled > 0) {
		ret = phy_set_bits_mmd(phydev, MDIO_MMD_VEND2,
				       MDIO_OATC14_PLCA_CTRL0,
				       MDIO_OATC14_PLCA_EN);

		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_plca_set_cfg);

 
int genphy_c45_plca_get_status(struct phy_device *phydev,
			       struct phy_plca_status *plca_st)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, MDIO_OATC14_PLCA_STATUS);
	if (ret < 0)
		return ret;

	plca_st->pst = !!(ret & MDIO_OATC14_PLCA_PST);
	return 0;
}
EXPORT_SYMBOL_GPL(genphy_c45_plca_get_status);

 
int genphy_c45_eee_is_active(struct phy_device *phydev, unsigned long *adv,
			     unsigned long *lp, bool *is_enabled)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(tmp_adv) = {};
	__ETHTOOL_DECLARE_LINK_MODE_MASK(tmp_lp) = {};
	__ETHTOOL_DECLARE_LINK_MODE_MASK(common);
	bool eee_enabled, eee_active;
	int ret;

	ret = genphy_c45_read_eee_adv(phydev, tmp_adv);
	if (ret)
		return ret;

	ret = genphy_c45_read_eee_lpa(phydev, tmp_lp);
	if (ret)
		return ret;

	eee_enabled = !linkmode_empty(tmp_adv);
	linkmode_and(common, tmp_adv, tmp_lp);
	if (eee_enabled && !linkmode_empty(common))
		eee_active = phy_check_valid(phydev->speed, phydev->duplex,
					     common);
	else
		eee_active = false;

	if (adv)
		linkmode_copy(adv, tmp_adv);
	if (lp)
		linkmode_copy(lp, tmp_lp);
	if (is_enabled)
		*is_enabled = eee_enabled;

	return eee_active;
}
EXPORT_SYMBOL(genphy_c45_eee_is_active);

 
int genphy_c45_ethtool_get_eee(struct phy_device *phydev,
			       struct ethtool_eee *data)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(adv) = {};
	__ETHTOOL_DECLARE_LINK_MODE_MASK(lp) = {};
	bool overflow = false, is_enabled;
	int ret;

	ret = genphy_c45_eee_is_active(phydev, adv, lp, &is_enabled);
	if (ret < 0)
		return ret;

	data->eee_enabled = is_enabled;
	data->eee_active = ret;

	if (!ethtool_convert_link_mode_to_legacy_u32(&data->supported,
						     phydev->supported_eee))
		overflow = true;
	if (!ethtool_convert_link_mode_to_legacy_u32(&data->advertised, adv))
		overflow = true;
	if (!ethtool_convert_link_mode_to_legacy_u32(&data->lp_advertised, lp))
		overflow = true;

	if (overflow)
		phydev_warn(phydev, "Not all supported or advertised EEE link modes were passed to the user space\n");

	return 0;
}
EXPORT_SYMBOL(genphy_c45_ethtool_get_eee);

 
int genphy_c45_ethtool_set_eee(struct phy_device *phydev,
			       struct ethtool_eee *data)
{
	int ret;

	if (data->eee_enabled) {
		if (data->advertised) {
			__ETHTOOL_DECLARE_LINK_MODE_MASK(adv);

			ethtool_convert_legacy_u32_to_link_mode(adv,
								data->advertised);
			linkmode_andnot(adv, adv, phydev->supported_eee);
			if (!linkmode_empty(adv)) {
				phydev_warn(phydev, "At least some EEE link modes are not supported.\n");
				return -EINVAL;
			}

			ethtool_convert_legacy_u32_to_link_mode(phydev->advertising_eee,
								data->advertised);
		} else {
			linkmode_copy(phydev->advertising_eee,
				      phydev->supported_eee);
		}

		phydev->eee_enabled = true;
	} else {
		phydev->eee_enabled = false;
	}

	ret = genphy_c45_an_config_eee_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		return phy_restart_aneg(phydev);

	return 0;
}
EXPORT_SYMBOL(genphy_c45_ethtool_set_eee);

struct phy_driver genphy_c45_driver = {
	.phy_id         = 0xffffffff,
	.phy_id_mask    = 0xffffffff,
	.name           = "Generic Clause 45 PHY",
	.read_status    = genphy_c45_read_status,
};

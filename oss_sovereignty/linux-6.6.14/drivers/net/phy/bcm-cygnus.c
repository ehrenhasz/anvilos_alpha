
 

 
#include "bcm-phy-lib.h"
#include <linux/brcmphy.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/phy.h>

struct bcm_omega_phy_priv {
	u64	*stats;
};

 
#define MII_BCM_CYGNUS_AFE_VDAC_ICTRL_0  0x91E5  

static int bcm_cygnus_afe_config(struct phy_device *phydev)
{
	int rc;

	 
	rc = phy_write(phydev, MII_BCM54XX_AUX_CTL, 0x0c30);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_misc(phydev, 0x39, 0x01, 0xA7C8);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_misc(phydev, 0x3A, 0x00, 0x0803);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_misc(phydev, 0x3A, 0x01, 0xA740);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_misc(phydev, 0x3A, 0x03, 0x8400);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_misc(phydev, 0x3B, 0x00, 0x0004);
	if (rc < 0)
		return rc;

	 
	rc = phy_write(phydev, MII_BRCM_CORE_BASE1E, 0x02);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_exp_sel(phydev, MII_BRCM_CORE_EXPB1, 0x10);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_exp_sel(phydev, MII_BRCM_CORE_EXPB0, 0x10);
	if (rc < 0)
		return rc;

	 
	rc = bcm_phy_write_exp_sel(phydev, MII_BRCM_CORE_EXPB0, 0x00);

	return 0;
}

static int bcm_cygnus_config_init(struct phy_device *phydev)
{
	int reg, rc;

	reg = phy_read(phydev, MII_BCM54XX_ECR);
	if (reg < 0)
		return reg;

	 
	reg |= MII_BCM54XX_ECR_IM;
	rc = phy_write(phydev, MII_BCM54XX_ECR, reg);
	if (rc)
		return rc;

	 
	reg = ~(MII_BCM54XX_INT_DUPLEX |
		MII_BCM54XX_INT_SPEED |
		MII_BCM54XX_INT_LINK);
	rc = phy_write(phydev, MII_BCM54XX_IMR, reg);
	if (rc)
		return rc;

	 
	rc = bcm_cygnus_afe_config(phydev);
	if (rc)
		return rc;

	 
	rc = bcm_phy_set_eee(phydev, true);
	if (rc)
		return rc;

	 
	return bcm_phy_enable_apd(phydev, false);
}

static int bcm_cygnus_resume(struct phy_device *phydev)
{
	int rc;

	genphy_resume(phydev);

	 
	rc = bcm_cygnus_config_init(phydev);
	if (rc)
		return rc;

	 
	return genphy_config_aneg(phydev);
}

static int bcm_omega_config_init(struct phy_device *phydev)
{
	u8 count, rev;
	int ret = 0;

	rev = phydev->phy_id & ~phydev->drv->phy_id_mask;

	pr_info_once("%s: %s PHY revision: 0x%02x\n",
		     phydev_name(phydev), phydev->drv->name, rev);

	 
	phy_read(phydev, MII_BMSR);

	switch (rev) {
	case 0x00:
		ret = bcm_phy_28nm_a0b0_afe_config_init(phydev);
		break;
	default:
		break;
	}

	if (ret)
		return ret;

	ret = bcm_phy_downshift_get(phydev, &count);
	if (ret)
		return ret;

	 
	ret = bcm_phy_set_eee(phydev, count == DOWNSHIFT_DEV_DISABLE);
	if (ret)
		return ret;

	return bcm_phy_enable_apd(phydev, true);
}

static int bcm_omega_resume(struct phy_device *phydev)
{
	int ret;

	 
	ret = bcm_omega_config_init(phydev);
	if (ret)
		return ret;

	 
	return genphy_config_aneg(phydev);
}

static int bcm_omega_get_tunable(struct phy_device *phydev,
				 struct ethtool_tunable *tuna, void *data)
{
	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		return bcm_phy_downshift_get(phydev, (u8 *)data);
	default:
		return -EOPNOTSUPP;
	}
}

static int bcm_omega_set_tunable(struct phy_device *phydev,
				 struct ethtool_tunable *tuna,
				 const void *data)
{
	u8 count = *(u8 *)data;
	int ret;

	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		ret = bcm_phy_downshift_set(phydev, count);
		break;
	default:
		return -EOPNOTSUPP;
	}

	if (ret)
		return ret;

	 
	ret = bcm_phy_set_eee(phydev, count == DOWNSHIFT_DEV_DISABLE);
	if (ret)
		return ret;

	return genphy_restart_aneg(phydev);
}

static void bcm_omega_get_phy_stats(struct phy_device *phydev,
				    struct ethtool_stats *stats, u64 *data)
{
	struct bcm_omega_phy_priv *priv = phydev->priv;

	bcm_phy_get_stats(phydev, priv->stats, stats, data);
}

static int bcm_omega_probe(struct phy_device *phydev)
{
	struct bcm_omega_phy_priv *priv;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	priv->stats = devm_kcalloc(&phydev->mdio.dev,
				   bcm_phy_get_sset_count(phydev), sizeof(u64),
				   GFP_KERNEL);
	if (!priv->stats)
		return -ENOMEM;

	return 0;
}

static struct phy_driver bcm_cygnus_phy_driver[] = {
{
	.phy_id        = PHY_ID_BCM_CYGNUS,
	.phy_id_mask   = 0xfffffff0,
	.name          = "Broadcom Cygnus PHY",
	 
	.config_init   = bcm_cygnus_config_init,
	.config_intr   = bcm_phy_config_intr,
	.handle_interrupt = bcm_phy_handle_interrupt,
	.suspend       = genphy_suspend,
	.resume        = bcm_cygnus_resume,
}, {
	.phy_id		= PHY_ID_BCM_OMEGA,
	.phy_id_mask	= 0xfffffff0,
	.name		= "Broadcom Omega Combo GPHY",
	 
	.flags		= PHY_IS_INTERNAL,
	.config_init	= bcm_omega_config_init,
	.suspend	= genphy_suspend,
	.resume		= bcm_omega_resume,
	.get_tunable	= bcm_omega_get_tunable,
	.set_tunable	= bcm_omega_set_tunable,
	.get_sset_count	= bcm_phy_get_sset_count,
	.get_strings	= bcm_phy_get_strings,
	.get_stats	= bcm_omega_get_phy_stats,
	.probe		= bcm_omega_probe,
}
};

static struct mdio_device_id __maybe_unused bcm_cygnus_phy_tbl[] = {
	{ PHY_ID_BCM_CYGNUS, 0xfffffff0, },
	{ PHY_ID_BCM_OMEGA, 0xfffffff0, },
	{ }
};
MODULE_DEVICE_TABLE(mdio, bcm_cygnus_phy_tbl);

module_phy_driver(bcm_cygnus_phy_driver);

MODULE_DESCRIPTION("Broadcom Cygnus internal PHY driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Broadcom Corporation");

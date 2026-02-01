
 
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/phy.h>

#define PHY_ID_ASIX_AX88772A		0x003b1861
#define PHY_ID_ASIX_AX88772C		0x003b1881
#define PHY_ID_ASIX_AX88796B		0x003b1841

MODULE_DESCRIPTION("Asix PHY driver");
MODULE_AUTHOR("Michael Schmitz <schmitzmic@gmail.com>");
MODULE_LICENSE("GPL");

 
static int asix_soft_reset(struct phy_device *phydev)
{
	int ret;

	 
	ret = phy_write(phydev, MII_BMCR, 0);
	if (ret < 0)
		return ret;

	return genphy_soft_reset(phydev);
}

 
static int asix_ax88772a_read_status(struct phy_device *phydev)
{
	int ret, val;

	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	if (!phydev->link)
		return 0;

	 
	val = phy_read(phydev, MII_BMCR);
	if (val < 0)
		return val;

	if (val & BMCR_SPEED100)
		phydev->speed = SPEED_100;
	else
		phydev->speed = SPEED_10;

	if (val & BMCR_FULLDPLX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	ret = genphy_read_lpa(phydev);
	if (ret < 0)
		return ret;

	if (phydev->autoneg == AUTONEG_ENABLE && phydev->autoneg_complete)
		phy_resolve_aneg_linkmode(phydev);

	return 0;
}

static void asix_ax88772a_link_change_notify(struct phy_device *phydev)
{
	 
	if (phydev->state == PHY_NOLINK) {
		phy_init_hw(phydev);
		phy_start_aneg(phydev);
	}
}

static struct phy_driver asix_driver[] = {
{
	PHY_ID_MATCH_EXACT(PHY_ID_ASIX_AX88772A),
	.name		= "Asix Electronics AX88772A",
	.flags		= PHY_IS_INTERNAL,
	.read_status	= asix_ax88772a_read_status,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.soft_reset	= asix_soft_reset,
	.link_change_notify	= asix_ax88772a_link_change_notify,
}, {
	PHY_ID_MATCH_EXACT(PHY_ID_ASIX_AX88772C),
	.name		= "Asix Electronics AX88772C",
	.flags		= PHY_IS_INTERNAL,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.soft_reset	= asix_soft_reset,
}, {
	.phy_id		= PHY_ID_ASIX_AX88796B,
	.name		= "Asix Electronics AX88796B",
	.phy_id_mask	= 0xfffffff0,
	 
	.soft_reset	= asix_soft_reset,
} };

module_phy_driver(asix_driver);

static struct mdio_device_id __maybe_unused asix_tbl[] = {
	{ PHY_ID_MATCH_EXACT(PHY_ID_ASIX_AX88772A) },
	{ PHY_ID_MATCH_EXACT(PHY_ID_ASIX_AX88772C) },
	{ PHY_ID_ASIX_AX88796B, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, asix_tbl);

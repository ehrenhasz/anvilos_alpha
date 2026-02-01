
 

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/delay.h>
#include "bcm-phy-lib.h"
#include <linux/bitops.h>
#include <linux/brcmphy.h>
#include <linux/clk.h>
#include <linux/mdio.h>

 

 
#define MII_BCM7XXX_100TX_AUX_CTL	0x10
#define MII_BCM7XXX_100TX_FALSE_CAR	0x13
#define MII_BCM7XXX_100TX_DISC		0x14
#define MII_BCM7XXX_AUX_MODE		0x1d
#define  MII_BCM7XXX_64CLK_MDIO		BIT(12)
#define MII_BCM7XXX_TEST		0x1f
#define  MII_BCM7XXX_SHD_MODE_2		BIT(2)
#define MII_BCM7XXX_SHD_2_ADDR_CTRL	0xe
#define MII_BCM7XXX_SHD_2_CTRL_STAT	0xf
#define MII_BCM7XXX_SHD_2_BIAS_TRIM	0x1a
#define MII_BCM7XXX_SHD_3_PCS_CTRL	0x0
#define MII_BCM7XXX_SHD_3_PCS_STATUS	0x1
#define MII_BCM7XXX_SHD_3_EEE_CAP	0x2
#define MII_BCM7XXX_SHD_3_AN_EEE_ADV	0x3
#define MII_BCM7XXX_SHD_3_EEE_LP	0x4
#define MII_BCM7XXX_SHD_3_EEE_WK_ERR	0x5
#define MII_BCM7XXX_SHD_3_PCS_CTRL_2	0x6
#define  MII_BCM7XXX_PCS_CTRL_2_DEF	0x4400
#define MII_BCM7XXX_SHD_3_AN_STAT	0xb
#define  MII_BCM7XXX_AN_NULL_MSG_EN	BIT(0)
#define  MII_BCM7XXX_AN_EEE_EN		BIT(1)
#define MII_BCM7XXX_SHD_3_EEE_THRESH	0xe
#define  MII_BCM7XXX_EEE_THRESH_DEF	0x50
#define MII_BCM7XXX_SHD_3_TL4		0x23
#define  MII_BCM7XXX_TL4_RST_MSK	(BIT(2) | BIT(1))

struct bcm7xxx_phy_priv {
	u64	*stats;
};

static int bcm7xxx_28nm_d0_afe_config_init(struct phy_device *phydev)
{
	 
	bcm_phy_write_misc(phydev, AFE_RXCONFIG_0, 0xeb15);

	 
	bcm_phy_write_misc(phydev, AFE_RXCONFIG_1, 0x9b2f);

	 
	bcm_phy_write_misc(phydev, AFE_RXCONFIG_2, 0x2003);

	 
	bcm_phy_write_misc(phydev, AFE_RX_LP_COUNTER, 0x7fc0);

	 
	bcm_phy_write_misc(phydev, AFE_TX_CONFIG, 0x431);

	 
	bcm_phy_write_misc(phydev, AFE_VDCA_ICTRL_0, 0xa7da);

	 
	bcm_phy_write_misc(phydev, AFE_VDAC_OTHERS_0, 0xa020);

	 
	bcm_phy_write_misc(phydev, AFE_HPF_TRIM_OTHERS, 0x00e3);

	 
	phy_write(phydev, MII_BRCM_CORE_BASE1E, 0x0010);

	 
	bcm_phy_write_misc(phydev, DSP_TAP10, 0x011b);

	 
	bcm_phy_r_rc_cal_reset(phydev);

	return 0;
}

static int bcm7xxx_28nm_e0_plus_afe_config_init(struct phy_device *phydev)
{
	 
	bcm_phy_write_misc(phydev, AFE_RXCONFIG_1, 0x9b2f);

	 
	bcm_phy_write_misc(phydev, AFE_TX_CONFIG, 0x431);

	 
	bcm_phy_write_misc(phydev, AFE_VDCA_ICTRL_0, 0xa7da);

	 
	bcm_phy_write_misc(phydev, AFE_HPF_TRIM_OTHERS, 0x00e3);

	 
	phy_write(phydev, MII_BRCM_CORE_BASE1E, 0x0010);

	 
	bcm_phy_write_misc(phydev, DSP_TAP10, 0x011b);

	 
	bcm_phy_r_rc_cal_reset(phydev);

	return 0;
}

static int bcm7xxx_28nm_a0_patch_afe_config_init(struct phy_device *phydev)
{
	 
	bcm_phy_write_misc(phydev, AFE_RXCONFIG_2, 0xd003);

	 
	bcm_phy_write_misc(phydev, DSP_TAP10, 0x791b);

	 
	bcm_phy_write_misc(phydev, AFE_HPF_TRIM_OTHERS, 0x10e3);

	 
	bcm_phy_write_misc(phydev, 0x21, 0x2, 0x87f6);

	 
	bcm_phy_write_misc(phydev, 0x22, 0x2, 0x017d);

	 
	bcm_phy_write_misc(phydev, 0x26, 0x2, 0x0015);

	bcm_phy_r_rc_cal_reset(phydev);

	return 0;
}

static int bcm7xxx_28nm_config_init(struct phy_device *phydev)
{
	u8 rev = PHY_BRCM_7XXX_REV(phydev->dev_flags);
	u8 patch = PHY_BRCM_7XXX_PATCH(phydev->dev_flags);
	u8 count;
	int ret = 0;

	 
	if (rev == 0)
		rev = phydev->phy_id & ~phydev->drv->phy_id_mask;

	pr_info_once("%s: %s PHY revision: 0x%02x, patch: %d\n",
		     phydev_name(phydev), phydev->drv->name, rev, patch);

	 
	phy_read(phydev, MII_BMSR);

	switch (rev) {
	case 0xa0:
	case 0xb0:
		ret = bcm_phy_28nm_a0b0_afe_config_init(phydev);
		break;
	case 0xd0:
		ret = bcm7xxx_28nm_d0_afe_config_init(phydev);
		break;
	case 0xe0:
	case 0xf0:
	 
	case 0x10:
		ret = bcm7xxx_28nm_e0_plus_afe_config_init(phydev);
		break;
	case 0x01:
		ret = bcm7xxx_28nm_a0_patch_afe_config_init(phydev);
		break;
	default:
		break;
	}

	if (ret)
		return ret;

	ret =  bcm_phy_enable_jumbo(phydev);
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

static int bcm7xxx_28nm_resume(struct phy_device *phydev)
{
	int ret;

	 
	ret = bcm7xxx_28nm_config_init(phydev);
	if (ret)
		return ret;

	 
	return genphy_config_aneg(phydev);
}

static int __phy_set_clr_bits(struct phy_device *dev, int location,
			      int set_mask, int clr_mask)
{
	int v, ret;

	v = __phy_read(dev, location);
	if (v < 0)
		return v;

	v &= ~clr_mask;
	v |= set_mask;

	ret = __phy_write(dev, location, v);
	if (ret < 0)
		return ret;

	return v;
}

static int phy_set_clr_bits(struct phy_device *dev, int location,
			    int set_mask, int clr_mask)
{
	int ret;

	mutex_lock(&dev->mdio.bus->mdio_lock);
	ret = __phy_set_clr_bits(dev, location, set_mask, clr_mask);
	mutex_unlock(&dev->mdio.bus->mdio_lock);

	return ret;
}

static int bcm7xxx_28nm_ephy_01_afe_config_init(struct phy_device *phydev)
{
	int ret;

	 
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_TEST,
			       MII_BCM7XXX_SHD_MODE_2, 0);
	if (ret < 0)
		return ret;

	 
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_BIAS_TRIM, 0x3BE0);
	if (ret < 0)
		goto reset_shadow_mode;

	 
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL,
			MII_BCM7XXX_SHD_3_TL4);
	if (ret < 0)
		goto reset_shadow_mode;
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT,
			       MII_BCM7XXX_TL4_RST_MSK, 0);
	if (ret < 0)
		goto reset_shadow_mode;

	 
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL,
			MII_BCM7XXX_SHD_3_TL4);
	if (ret < 0)
		goto reset_shadow_mode;
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT,
			       0, MII_BCM7XXX_TL4_RST_MSK);
	if (ret < 0)
		goto reset_shadow_mode;

reset_shadow_mode:
	 
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_TEST, 0,
			       MII_BCM7XXX_SHD_MODE_2);
	if (ret < 0)
		return ret;

	return 0;
}

 
static int bcm7xxx_28nm_ephy_apd_enable(struct phy_device *phydev)
{
	int ret;

	 
	ret = phy_set_clr_bits(phydev, MII_BRCM_FET_BRCMTEST,
			       MII_BRCM_FET_BT_SRE, 0);
	if (ret < 0)
		return ret;

	 
	ret = phy_set_clr_bits(phydev, MII_BRCM_FET_SHDW_AUXSTAT2,
			       MII_BRCM_FET_SHDW_AS2_APDE, 0);
	if (ret < 0)
		return ret;

	 
	ret = phy_set_clr_bits(phydev, MII_BRCM_FET_BRCMTEST, 0,
			       MII_BRCM_FET_BT_SRE);
	if (ret < 0)
		return ret;

	return 0;
}

static int bcm7xxx_28nm_ephy_eee_enable(struct phy_device *phydev)
{
	int ret;

	 
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_TEST,
			       MII_BCM7XXX_SHD_MODE_2, 0);
	if (ret < 0)
		return ret;

	 
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL,
			MII_BCM7XXX_SHD_3_AN_EEE_ADV);
	if (ret < 0)
		goto reset_shadow_mode;
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT,
			MDIO_EEE_100TX);
	if (ret < 0)
		goto reset_shadow_mode;

	 
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL,
			MII_BCM7XXX_SHD_3_PCS_CTRL_2);
	if (ret < 0)
		goto reset_shadow_mode;
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT,
			MII_BCM7XXX_PCS_CTRL_2_DEF);
	if (ret < 0)
		goto reset_shadow_mode;

	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL,
			MII_BCM7XXX_SHD_3_EEE_THRESH);
	if (ret < 0)
		goto reset_shadow_mode;
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT,
			MII_BCM7XXX_EEE_THRESH_DEF);
	if (ret < 0)
		goto reset_shadow_mode;

	 
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL,
			MII_BCM7XXX_SHD_3_AN_STAT);
	if (ret < 0)
		goto reset_shadow_mode;
	ret = phy_write(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT,
			(MII_BCM7XXX_AN_NULL_MSG_EN | MII_BCM7XXX_AN_EEE_EN));
	if (ret < 0)
		goto reset_shadow_mode;

reset_shadow_mode:
	 
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_TEST, 0,
			       MII_BCM7XXX_SHD_MODE_2);
	if (ret < 0)
		return ret;

	 
	phy_write(phydev, MII_BMCR,
		  (BMCR_SPEED100 | BMCR_ANENABLE | BMCR_ANRESTART));

	return 0;
}

static int bcm7xxx_28nm_ephy_config_init(struct phy_device *phydev)
{
	u8 rev = phydev->phy_id & ~phydev->drv->phy_id_mask;
	int ret = 0;

	pr_info_once("%s: %s PHY revision: 0x%02x\n",
		     phydev_name(phydev), phydev->drv->name, rev);

	 
	phy_read(phydev, MII_BMSR);

	 
	if (rev == 0x01) {
		ret = bcm7xxx_28nm_ephy_01_afe_config_init(phydev);
		if (ret)
			return ret;
	}

	ret = bcm7xxx_28nm_ephy_eee_enable(phydev);
	if (ret)
		return ret;

	return bcm7xxx_28nm_ephy_apd_enable(phydev);
}

static int bcm7xxx_16nm_ephy_afe_config(struct phy_device *phydev)
{
	int tmp, rcalcode, rcalnewcodelp, rcalnewcode11, rcalnewcode11d2;

	 
	tmp = genphy_soft_reset(phydev);
	if (tmp)
		return tmp;

	 
	bcm_phy_write_exp_sel(phydev, 0x0003, 0x0006);
	 
	bcm_phy_write_exp_sel(phydev, 0x0003, 0x0000);

	 
	bcm_phy_write_misc(phydev, 0x0030, 0x0001, 0x0000);
	bcm_phy_write_misc(phydev, 0x0031, 0x0000, 0x044a);

	 
	bcm_phy_write_misc(phydev, 0x0033, 0x0002, 0x71a1);
	 
	bcm_phy_write_misc(phydev, 0x0033, 0x0001, 0x8000);

	 
	bcm_phy_write_misc(phydev, 0x0031, 0x0001, 0x2f68);
	bcm_phy_write_misc(phydev, 0x0031, 0x0002, 0x0000);

	 
	bcm_phy_write_misc(phydev, 0x0030, 0x0003, 0xc036);

	 
	bcm_phy_write_misc(phydev, 0x0032, 0x0003, 0x0000);
	 
	bcm_phy_write_misc(phydev, 0x0033, 0x0000, 0x0002);
	 
	bcm_phy_write_misc(phydev, 0x0030, 0x0002, 0x01c0);
	 
	bcm_phy_write_misc(phydev, 0x0030, 0x0001, 0x0001);

	 
	bcm_phy_write_misc(phydev, 0x0038, 0x0000, 0x0010);

	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0003, 0x0038);
	bcm_phy_write_misc(phydev, 0x0039, 0x0003, 0x003b);
	udelay(2);
	bcm_phy_write_misc(phydev, 0x0039, 0x0003, 0x003f);
	mdelay(5);

	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0001, 0x1c82);
	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0001, 0x9e82);
	udelay(2);
	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0001, 0x9f82);
	udelay(100);
	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0001, 0x9e86);
	udelay(2);
	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0001, 0x9f86);
	udelay(100);

	 
	bcm_phy_write_misc(phydev, 0x0038, 0x0001, 0xe7ea);
	 
	bcm_phy_write_misc(phydev, 0x0038, 0x0002, 0xede0);

	 
	tmp = bcm_phy_read_exp_sel(phydev, 0x00a9);
	 
	rcalcode = (tmp & 0x7e) / 2;
	 
	rcalnewcodelp = rcalcode + 16;
	 
	rcalnewcode11 = rcalcode + 10;
	 
	if (rcalnewcodelp > 0x3f)
		rcalnewcodelp = 0x3f;
	if (rcalnewcode11 > 0x3f)
		rcalnewcode11 = 0x3f;
	 
	tmp = 0x00f8 + rcalnewcodelp * 256;
	 
	bcm_phy_write_misc(phydev, 0x0039, 0x0003, tmp);
	 
	bcm_phy_write_misc(phydev, 0x0038, 0x0001, 0xe7e4);
	 
	bcm_phy_write_misc(phydev, 0x003b, 0x0000, 0x8002);
	 
	bcm_phy_write_misc(phydev, 0x003c, 0x0003, 0xf882);
	 
	bcm_phy_write_misc(phydev, 0x003d, 0x0000, 0x3201);
	 
	bcm_phy_write_misc(phydev, 0x003a, 0x0002, 0x0c00);

	 
	bcm_phy_write_misc(phydev, 0x003a, 0x0001, 0x0020);

	 
	bcm_phy_write_misc(phydev, 0x003b, 0x0002, 0x0000);
	bcm_phy_write_misc(phydev, 0x003b, 0x0003, 0x0000);

	 
	bcm_phy_write_misc(phydev, 0x003a, 0x0003, 0x0800);
	udelay(2);

	 
	bcm_phy_write_misc(phydev, 0x003a, 0x0001, 0x0000);

	 
	rcalnewcode11d2 = (rcalnewcode11 & 0xfffe) / 2;
	tmp = bcm_phy_read_misc(phydev, 0x003d, 0x0001);
	 
	tmp &= ~0xfe0;
	 
	tmp |= 0x0020 | (rcalnewcode11d2 * 64);
	bcm_phy_write_misc(phydev, 0x003d, 0x0001, tmp);
	bcm_phy_write_misc(phydev, 0x003d, 0x0002, tmp);

	tmp = bcm_phy_read_misc(phydev, 0x003d, 0x0000);
	 
	tmp &= ~0x3000;
	tmp |= 0x3000;
	bcm_phy_write_misc(phydev, 0x003d, 0x0000, tmp);

	return 0;
}

static int bcm7xxx_16nm_ephy_config_init(struct phy_device *phydev)
{
	int ret, val;

	ret = bcm7xxx_16nm_ephy_afe_config(phydev);
	if (ret)
		return ret;

	ret = bcm_phy_set_eee(phydev, true);
	if (ret)
		return ret;

	ret = bcm_phy_read_shadow(phydev, BCM54XX_SHD_SCR3);
	if (ret < 0)
		return ret;

	val = ret;

	 
	val &= ~BCM54XX_SHD_SCR3_DLLAPD_DIS;
	val |= BIT(8);

	ret = bcm_phy_write_shadow(phydev, BCM54XX_SHD_SCR3, val);
	if (ret < 0)
		return ret;

	return bcm_phy_enable_apd(phydev, true);
}

static int bcm7xxx_16nm_ephy_resume(struct phy_device *phydev)
{
	int ret;

	 
	ret = bcm7xxx_16nm_ephy_config_init(phydev);
	if (ret)
		return ret;

	return genphy_config_aneg(phydev);
}

#define MII_BCM7XXX_REG_INVALID	0xff

static u8 bcm7xxx_28nm_ephy_regnum_to_shd(u16 regnum)
{
	switch (regnum) {
	case MDIO_CTRL1:
		return MII_BCM7XXX_SHD_3_PCS_CTRL;
	case MDIO_STAT1:
		return MII_BCM7XXX_SHD_3_PCS_STATUS;
	case MDIO_PCS_EEE_ABLE:
		return MII_BCM7XXX_SHD_3_EEE_CAP;
	case MDIO_AN_EEE_ADV:
		return MII_BCM7XXX_SHD_3_AN_EEE_ADV;
	case MDIO_AN_EEE_LPABLE:
		return MII_BCM7XXX_SHD_3_EEE_LP;
	case MDIO_PCS_EEE_WK_ERR:
		return MII_BCM7XXX_SHD_3_EEE_WK_ERR;
	default:
		return MII_BCM7XXX_REG_INVALID;
	}
}

static bool bcm7xxx_28nm_ephy_dev_valid(int devnum)
{
	return devnum == MDIO_MMD_AN || devnum == MDIO_MMD_PCS;
}

static int bcm7xxx_28nm_ephy_read_mmd(struct phy_device *phydev,
				      int devnum, u16 regnum)
{
	u8 shd = bcm7xxx_28nm_ephy_regnum_to_shd(regnum);
	int ret;

	if (!bcm7xxx_28nm_ephy_dev_valid(devnum) ||
	    shd == MII_BCM7XXX_REG_INVALID)
		return -EOPNOTSUPP;

	 
	ret = __phy_set_clr_bits(phydev, MII_BCM7XXX_TEST,
				 MII_BCM7XXX_SHD_MODE_2, 0);
	if (ret < 0)
		return ret;

	 
	ret = __phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL, shd);
	if (ret < 0)
		goto reset_shadow_mode;

	ret = __phy_read(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT);

reset_shadow_mode:
	 
	__phy_set_clr_bits(phydev, MII_BCM7XXX_TEST, 0,
			   MII_BCM7XXX_SHD_MODE_2);
	return ret;
}

static int bcm7xxx_28nm_ephy_write_mmd(struct phy_device *phydev,
				       int devnum, u16 regnum, u16 val)
{
	u8 shd = bcm7xxx_28nm_ephy_regnum_to_shd(regnum);
	int ret;

	if (!bcm7xxx_28nm_ephy_dev_valid(devnum) ||
	    shd == MII_BCM7XXX_REG_INVALID)
		return -EOPNOTSUPP;

	 
	ret = __phy_set_clr_bits(phydev, MII_BCM7XXX_TEST,
				 MII_BCM7XXX_SHD_MODE_2, 0);
	if (ret < 0)
		return ret;

	 
	ret = __phy_write(phydev, MII_BCM7XXX_SHD_2_ADDR_CTRL, shd);
	if (ret < 0)
		goto reset_shadow_mode;

	 
	__phy_write(phydev, MII_BCM7XXX_SHD_2_CTRL_STAT, val);

reset_shadow_mode:
	 
	return __phy_set_clr_bits(phydev, MII_BCM7XXX_TEST, 0,
				  MII_BCM7XXX_SHD_MODE_2);
}

static int bcm7xxx_28nm_ephy_resume(struct phy_device *phydev)
{
	int ret;

	 
	ret = bcm7xxx_28nm_ephy_config_init(phydev);
	if (ret)
		return ret;

	return genphy_config_aneg(phydev);
}

static int bcm7xxx_config_init(struct phy_device *phydev)
{
	int ret;

	 
	phy_write(phydev, MII_BCM7XXX_AUX_MODE, MII_BCM7XXX_64CLK_MDIO);
	phy_read(phydev, MII_BCM7XXX_AUX_MODE);

	 
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_TEST,
			MII_BCM7XXX_SHD_MODE_2, MII_BCM7XXX_SHD_MODE_2);
	if (ret < 0)
		return ret;

	 
	phy_write(phydev, MII_BCM7XXX_100TX_DISC, 0x0F00);
	udelay(10);

	 
	phy_write(phydev, MII_BCM7XXX_100TX_DISC, 0x0C00);

	phy_write(phydev, MII_BCM7XXX_100TX_FALSE_CAR, 0x7555);

	 
	ret = phy_set_clr_bits(phydev, MII_BCM7XXX_TEST, 0, MII_BCM7XXX_SHD_MODE_2);
	if (ret < 0)
		return ret;

	return 0;
}

 
static int bcm7xxx_suspend(struct phy_device *phydev)
{
	int ret;
	static const struct bcm7xxx_regs {
		int reg;
		u16 value;
	} bcm7xxx_suspend_cfg[] = {
		{ MII_BCM7XXX_TEST, 0x008b },
		{ MII_BCM7XXX_100TX_AUX_CTL, 0x01c0 },
		{ MII_BCM7XXX_100TX_DISC, 0x7000 },
		{ MII_BCM7XXX_TEST, 0x000f },
		{ MII_BCM7XXX_100TX_AUX_CTL, 0x20d0 },
		{ MII_BCM7XXX_TEST, 0x000b },
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bcm7xxx_suspend_cfg); i++) {
		ret = phy_write(phydev,
				bcm7xxx_suspend_cfg[i].reg,
				bcm7xxx_suspend_cfg[i].value);
		if (ret)
			return ret;
	}

	return 0;
}

static int bcm7xxx_28nm_get_tunable(struct phy_device *phydev,
				    struct ethtool_tunable *tuna,
				    void *data)
{
	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		return bcm_phy_downshift_get(phydev, (u8 *)data);
	default:
		return -EOPNOTSUPP;
	}
}

static int bcm7xxx_28nm_set_tunable(struct phy_device *phydev,
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

static void bcm7xxx_28nm_get_phy_stats(struct phy_device *phydev,
				       struct ethtool_stats *stats, u64 *data)
{
	struct bcm7xxx_phy_priv *priv = phydev->priv;

	bcm_phy_get_stats(phydev, priv->stats, stats, data);
}

static int bcm7xxx_28nm_probe(struct phy_device *phydev)
{
	struct bcm7xxx_phy_priv *priv;
	struct clk *clk;
	int ret = 0;

	priv = devm_kzalloc(&phydev->mdio.dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	priv->stats = devm_kcalloc(&phydev->mdio.dev,
				   bcm_phy_get_sset_count(phydev), sizeof(u64),
				   GFP_KERNEL);
	if (!priv->stats)
		return -ENOMEM;

	clk = devm_clk_get_optional_enabled(&phydev->mdio.dev, NULL);
	if (IS_ERR(clk))
		return PTR_ERR(clk);

	 
	phy_read(phydev, MII_BMSR);

	return ret;
}

#define BCM7XXX_28NM_GPHY(_oui, _name)					\
{									\
	.phy_id		= (_oui),					\
	.phy_id_mask	= 0xfffffff0,					\
	.name		= _name,					\
	 						\
	.flags		= PHY_IS_INTERNAL,				\
	.config_init	= bcm7xxx_28nm_config_init,			\
	.resume		= bcm7xxx_28nm_resume,				\
	.get_tunable	= bcm7xxx_28nm_get_tunable,			\
	.set_tunable	= bcm7xxx_28nm_set_tunable,			\
	.get_sset_count	= bcm_phy_get_sset_count,			\
	.get_strings	= bcm_phy_get_strings,				\
	.get_stats	= bcm7xxx_28nm_get_phy_stats,			\
	.probe		= bcm7xxx_28nm_probe,				\
}

#define BCM7XXX_28NM_EPHY(_oui, _name)					\
{									\
	.phy_id		= (_oui),					\
	.phy_id_mask	= 0xfffffff0,					\
	.name		= _name,					\
	 					\
	.flags		= PHY_IS_INTERNAL,				\
	.config_init	= bcm7xxx_28nm_ephy_config_init,		\
	.resume		= bcm7xxx_28nm_ephy_resume,			\
	.get_sset_count	= bcm_phy_get_sset_count,			\
	.get_strings	= bcm_phy_get_strings,				\
	.get_stats	= bcm7xxx_28nm_get_phy_stats,			\
	.probe		= bcm7xxx_28nm_probe,				\
	.read_mmd	= bcm7xxx_28nm_ephy_read_mmd,			\
	.write_mmd	= bcm7xxx_28nm_ephy_write_mmd,			\
}

#define BCM7XXX_40NM_EPHY(_oui, _name)					\
{									\
	.phy_id         = (_oui),					\
	.phy_id_mask    = 0xfffffff0,					\
	.name           = _name,					\
	 					\
	.flags          = PHY_IS_INTERNAL,				\
	.soft_reset	= genphy_soft_reset,				\
	.config_init    = bcm7xxx_config_init,				\
	.suspend        = bcm7xxx_suspend,				\
	.resume         = bcm7xxx_config_init,				\
}

#define BCM7XXX_16NM_EPHY(_oui, _name)					\
{									\
	.phy_id		= (_oui),					\
	.phy_id_mask	= 0xfffffff0,					\
	.name		= _name,					\
	 					\
	.flags		= PHY_IS_INTERNAL,				\
	.get_sset_count	= bcm_phy_get_sset_count,			\
	.get_strings	= bcm_phy_get_strings,				\
	.get_stats	= bcm7xxx_28nm_get_phy_stats,			\
	.probe		= bcm7xxx_28nm_probe,				\
	.config_init	= bcm7xxx_16nm_ephy_config_init,		\
	.config_aneg	= genphy_config_aneg,				\
	.read_status	= genphy_read_status,				\
	.resume		= bcm7xxx_16nm_ephy_resume,			\
}

static struct phy_driver bcm7xxx_driver[] = {
	BCM7XXX_28NM_EPHY(PHY_ID_BCM72113, "Broadcom BCM72113"),
	BCM7XXX_28NM_EPHY(PHY_ID_BCM72116, "Broadcom BCM72116"),
	BCM7XXX_16NM_EPHY(PHY_ID_BCM72165, "Broadcom BCM72165"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7250, "Broadcom BCM7250"),
	BCM7XXX_28NM_EPHY(PHY_ID_BCM7255, "Broadcom BCM7255"),
	BCM7XXX_28NM_EPHY(PHY_ID_BCM7260, "Broadcom BCM7260"),
	BCM7XXX_28NM_EPHY(PHY_ID_BCM7268, "Broadcom BCM7268"),
	BCM7XXX_28NM_EPHY(PHY_ID_BCM7271, "Broadcom BCM7271"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7278, "Broadcom BCM7278"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7364, "Broadcom BCM7364"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7366, "Broadcom BCM7366"),
	BCM7XXX_16NM_EPHY(PHY_ID_BCM74165, "Broadcom BCM74165"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM74371, "Broadcom BCM74371"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7439, "Broadcom BCM7439"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7439_2, "Broadcom BCM7439 (2)"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7445, "Broadcom BCM7445"),
	BCM7XXX_40NM_EPHY(PHY_ID_BCM7346, "Broadcom BCM7346"),
	BCM7XXX_40NM_EPHY(PHY_ID_BCM7362, "Broadcom BCM7362"),
	BCM7XXX_40NM_EPHY(PHY_ID_BCM7425, "Broadcom BCM7425"),
	BCM7XXX_40NM_EPHY(PHY_ID_BCM7429, "Broadcom BCM7429"),
	BCM7XXX_40NM_EPHY(PHY_ID_BCM7435, "Broadcom BCM7435"),
	BCM7XXX_16NM_EPHY(PHY_ID_BCM7712, "Broadcom BCM7712"),
};

static struct mdio_device_id __maybe_unused bcm7xxx_tbl[] = {
	{ PHY_ID_BCM72113, 0xfffffff0 },
	{ PHY_ID_BCM72116, 0xfffffff0, },
	{ PHY_ID_BCM72165, 0xfffffff0, },
	{ PHY_ID_BCM7250, 0xfffffff0, },
	{ PHY_ID_BCM7255, 0xfffffff0, },
	{ PHY_ID_BCM7260, 0xfffffff0, },
	{ PHY_ID_BCM7268, 0xfffffff0, },
	{ PHY_ID_BCM7271, 0xfffffff0, },
	{ PHY_ID_BCM7278, 0xfffffff0, },
	{ PHY_ID_BCM7364, 0xfffffff0, },
	{ PHY_ID_BCM7366, 0xfffffff0, },
	{ PHY_ID_BCM7346, 0xfffffff0, },
	{ PHY_ID_BCM7362, 0xfffffff0, },
	{ PHY_ID_BCM7425, 0xfffffff0, },
	{ PHY_ID_BCM7429, 0xfffffff0, },
	{ PHY_ID_BCM74371, 0xfffffff0, },
	{ PHY_ID_BCM7439, 0xfffffff0, },
	{ PHY_ID_BCM7435, 0xfffffff0, },
	{ PHY_ID_BCM7445, 0xfffffff0, },
	{ PHY_ID_BCM7712, 0xfffffff0, },
	{ }
};

module_phy_driver(bcm7xxx_driver);

MODULE_DEVICE_TABLE(mdio, bcm7xxx_tbl);

MODULE_DESCRIPTION("Broadcom BCM7xxx internal PHY driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");

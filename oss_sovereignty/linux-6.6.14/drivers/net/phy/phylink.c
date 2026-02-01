
 
#include <linux/acpi.h>
#include <linux/ethtool.h>
#include <linux/export.h>
#include <linux/gpio/consumer.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/phylink.h>
#include <linux/rtnetlink.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "sfp.h"
#include "swphy.h"

#define SUPPORTED_INTERFACES \
	(SUPPORTED_TP | SUPPORTED_MII | SUPPORTED_FIBRE | \
	 SUPPORTED_BNC | SUPPORTED_AUI | SUPPORTED_Backplane)
#define ADVERTISED_INTERFACES \
	(ADVERTISED_TP | ADVERTISED_MII | ADVERTISED_FIBRE | \
	 ADVERTISED_BNC | ADVERTISED_AUI | ADVERTISED_Backplane)

enum {
	PHYLINK_DISABLE_STOPPED,
	PHYLINK_DISABLE_LINK,
	PHYLINK_DISABLE_MAC_WOL,

	PCS_STATE_DOWN = 0,
	PCS_STATE_STARTING,
	PCS_STATE_STARTED,
};

 
struct phylink {
	 
	struct net_device *netdev;
	const struct phylink_mac_ops *mac_ops;
	struct phylink_config *config;
	struct phylink_pcs *pcs;
	struct device *dev;
	unsigned int old_link_state:1;

	unsigned long phylink_disable_state;  
	struct phy_device *phydev;
	phy_interface_t link_interface;	 
	u8 cfg_link_an_mode;		 
	u8 cur_link_an_mode;
	u8 link_port;			 
	__ETHTOOL_DECLARE_LINK_MODE_MASK(supported);

	 
	struct phylink_link_state link_config;

	 
	phy_interface_t cur_interface;

	struct gpio_desc *link_gpio;
	unsigned int link_irq;
	struct timer_list link_poll;
	void (*get_fixed_state)(struct net_device *dev,
				struct phylink_link_state *s);

	struct mutex state_mutex;
	struct phylink_link_state phy_state;
	struct work_struct resolve;
	unsigned int pcs_neg_mode;
	unsigned int pcs_state;

	bool mac_link_dropped;
	bool using_mac_select_pcs;

	struct sfp_bus *sfp_bus;
	bool sfp_may_have_phy;
	DECLARE_PHY_INTERFACE_MASK(sfp_interfaces);
	__ETHTOOL_DECLARE_LINK_MODE_MASK(sfp_support);
	u8 sfp_port;
};

#define phylink_printk(level, pl, fmt, ...) \
	do { \
		if ((pl)->config->type == PHYLINK_NETDEV) \
			netdev_printk(level, (pl)->netdev, fmt, ##__VA_ARGS__); \
		else if ((pl)->config->type == PHYLINK_DEV) \
			dev_printk(level, (pl)->dev, fmt, ##__VA_ARGS__); \
	} while (0)

#define phylink_err(pl, fmt, ...) \
	phylink_printk(KERN_ERR, pl, fmt, ##__VA_ARGS__)
#define phylink_warn(pl, fmt, ...) \
	phylink_printk(KERN_WARNING, pl, fmt, ##__VA_ARGS__)
#define phylink_info(pl, fmt, ...) \
	phylink_printk(KERN_INFO, pl, fmt, ##__VA_ARGS__)
#if defined(CONFIG_DYNAMIC_DEBUG)
#define phylink_dbg(pl, fmt, ...) \
do {									\
	if ((pl)->config->type == PHYLINK_NETDEV)			\
		netdev_dbg((pl)->netdev, fmt, ##__VA_ARGS__);		\
	else if ((pl)->config->type == PHYLINK_DEV)			\
		dev_dbg((pl)->dev, fmt, ##__VA_ARGS__);			\
} while (0)
#elif defined(DEBUG)
#define phylink_dbg(pl, fmt, ...)					\
	phylink_printk(KERN_DEBUG, pl, fmt, ##__VA_ARGS__)
#else
#define phylink_dbg(pl, fmt, ...)					\
({									\
	if (0)								\
		phylink_printk(KERN_DEBUG, pl, fmt, ##__VA_ARGS__);	\
})
#endif

 
void phylink_set_port_modes(unsigned long *mask)
{
	phylink_set(mask, TP);
	phylink_set(mask, AUI);
	phylink_set(mask, MII);
	phylink_set(mask, FIBRE);
	phylink_set(mask, BNC);
	phylink_set(mask, Backplane);
}
EXPORT_SYMBOL_GPL(phylink_set_port_modes);

static int phylink_is_empty_linkmode(const unsigned long *linkmode)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(tmp) = { 0, };

	phylink_set_port_modes(tmp);
	phylink_set(tmp, Autoneg);
	phylink_set(tmp, Pause);
	phylink_set(tmp, Asym_Pause);

	return linkmode_subset(linkmode, tmp);
}

static const char *phylink_an_mode_str(unsigned int mode)
{
	static const char *modestr[] = {
		[MLO_AN_PHY] = "phy",
		[MLO_AN_FIXED] = "fixed",
		[MLO_AN_INBAND] = "inband",
	};

	return mode < ARRAY_SIZE(modestr) ? modestr[mode] : "unknown";
}

static unsigned int phylink_interface_signal_rate(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:  
		return 1250;
	case PHY_INTERFACE_MODE_2500BASEX:  
		return 3125;
	case PHY_INTERFACE_MODE_5GBASER:  
		return 5156;
	case PHY_INTERFACE_MODE_10GBASER:  
		return 10313;
	default:
		return 0;
	}
}

 
static int phylink_interface_max_speed(phy_interface_t interface)
{
	switch (interface) {
	case PHY_INTERFACE_MODE_100BASEX:
	case PHY_INTERFACE_MODE_REVRMII:
	case PHY_INTERFACE_MODE_RMII:
	case PHY_INTERFACE_MODE_SMII:
	case PHY_INTERFACE_MODE_REVMII:
	case PHY_INTERFACE_MODE_MII:
		return SPEED_100;

	case PHY_INTERFACE_MODE_TBI:
	case PHY_INTERFACE_MODE_MOCA:
	case PHY_INTERFACE_MODE_RTBI:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_1000BASEKX:
	case PHY_INTERFACE_MODE_TRGMII:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_PSGMII:
	case PHY_INTERFACE_MODE_QSGMII:
	case PHY_INTERFACE_MODE_QUSGMII:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_GMII:
		return SPEED_1000;

	case PHY_INTERFACE_MODE_2500BASEX:
		return SPEED_2500;

	case PHY_INTERFACE_MODE_5GBASER:
		return SPEED_5000;

	case PHY_INTERFACE_MODE_XGMII:
	case PHY_INTERFACE_MODE_RXAUI:
	case PHY_INTERFACE_MODE_XAUI:
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10GKR:
	case PHY_INTERFACE_MODE_USXGMII:
		return SPEED_10000;

	case PHY_INTERFACE_MODE_25GBASER:
		return SPEED_25000;

	case PHY_INTERFACE_MODE_XLGMII:
		return SPEED_40000;

	case PHY_INTERFACE_MODE_INTERNAL:
	case PHY_INTERFACE_MODE_NA:
	case PHY_INTERFACE_MODE_MAX:
		 
		return SPEED_UNKNOWN;
	}

	 
	WARN_ON_ONCE(1);
	return SPEED_UNKNOWN;
}

 
void phylink_caps_to_linkmodes(unsigned long *linkmodes, unsigned long caps)
{
	if (caps & MAC_SYM_PAUSE)
		__set_bit(ETHTOOL_LINK_MODE_Pause_BIT, linkmodes);

	if (caps & MAC_ASYM_PAUSE)
		__set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, linkmodes);

	if (caps & MAC_10HD) {
		__set_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1S_Half_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1S_P2MP_Half_BIT, linkmodes);
	}

	if (caps & MAC_10FD) {
		__set_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1L_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10baseT1S_Full_BIT, linkmodes);
	}

	if (caps & MAC_100HD) {
		__set_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100baseFX_Half_BIT, linkmodes);
	}

	if (caps & MAC_100FD) {
		__set_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100baseT1_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100baseFX_Full_BIT, linkmodes);
	}

	if (caps & MAC_1000HD)
		__set_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT, linkmodes);

	if (caps & MAC_1000FD) {
		__set_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_1000baseKX_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_1000baseX_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_1000baseT1_Full_BIT, linkmodes);
	}

	if (caps & MAC_2500FD) {
		__set_bit(ETHTOOL_LINK_MODE_2500baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_2500baseX_Full_BIT, linkmodes);
	}

	if (caps & MAC_5000FD)
		__set_bit(ETHTOOL_LINK_MODE_5000baseT_Full_BIT, linkmodes);

	if (caps & MAC_10000FD) {
		__set_bit(ETHTOOL_LINK_MODE_10000baseT_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseR_FEC_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseSR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseLR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseLRM_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_10000baseER_Full_BIT, linkmodes);
	}

	if (caps & MAC_25000FD) {
		__set_bit(ETHTOOL_LINK_MODE_25000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_25000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_25000baseSR_Full_BIT, linkmodes);
	}

	if (caps & MAC_40000FD) {
		__set_bit(ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_40000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_40000baseLR4_Full_BIT, linkmodes);
	}

	if (caps & MAC_50000FD) {
		__set_bit(ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseKR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseSR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseSR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseLR_ER_FR_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_50000baseDR_Full_BIT, linkmodes);
	}

	if (caps & MAC_56000FD) {
		__set_bit(ETHTOOL_LINK_MODE_56000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_56000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_56000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_56000baseLR4_Full_BIT, linkmodes);
	}

	if (caps & MAC_100000FD) {
		__set_bit(ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseLR4_ER4_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseKR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseSR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseCR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseLR2_ER2_FR2_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseDR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseKR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseSR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseLR_ER_FR_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseCR_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_100000baseDR_Full_BIT, linkmodes);
	}

	if (caps & MAC_200000FD) {
		__set_bit(ETHTOOL_LINK_MODE_200000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseLR4_ER4_FR4_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseDR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseCR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseKR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseSR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseLR2_ER2_FR2_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseDR2_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_200000baseCR2_Full_BIT, linkmodes);
	}

	if (caps & MAC_400000FD) {
		__set_bit(ETHTOOL_LINK_MODE_400000baseKR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseSR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseLR8_ER8_FR8_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseDR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseCR8_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseKR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseSR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseLR4_ER4_FR4_Full_BIT,
			  linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseDR4_Full_BIT, linkmodes);
		__set_bit(ETHTOOL_LINK_MODE_400000baseCR4_Full_BIT, linkmodes);
	}
}
EXPORT_SYMBOL_GPL(phylink_caps_to_linkmodes);

static struct {
	unsigned long mask;
	int speed;
	unsigned int duplex;
} phylink_caps_params[] = {
	{ MAC_400000FD, SPEED_400000, DUPLEX_FULL },
	{ MAC_200000FD, SPEED_200000, DUPLEX_FULL },
	{ MAC_100000FD, SPEED_100000, DUPLEX_FULL },
	{ MAC_56000FD,  SPEED_56000,  DUPLEX_FULL },
	{ MAC_50000FD,  SPEED_50000,  DUPLEX_FULL },
	{ MAC_40000FD,  SPEED_40000,  DUPLEX_FULL },
	{ MAC_25000FD,  SPEED_25000,  DUPLEX_FULL },
	{ MAC_20000FD,  SPEED_20000,  DUPLEX_FULL },
	{ MAC_10000FD,  SPEED_10000,  DUPLEX_FULL },
	{ MAC_5000FD,   SPEED_5000,   DUPLEX_FULL },
	{ MAC_2500FD,   SPEED_2500,   DUPLEX_FULL },
	{ MAC_1000FD,   SPEED_1000,   DUPLEX_FULL },
	{ MAC_1000HD,   SPEED_1000,   DUPLEX_HALF },
	{ MAC_100FD,    SPEED_100,    DUPLEX_FULL },
	{ MAC_100HD,    SPEED_100,    DUPLEX_HALF },
	{ MAC_10FD,     SPEED_10,     DUPLEX_FULL },
	{ MAC_10HD,     SPEED_10,     DUPLEX_HALF },
};

 
void phylink_limit_mac_speed(struct phylink_config *config, u32 max_speed)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(phylink_caps_params) &&
		    phylink_caps_params[i].speed > max_speed; i++)
		config->mac_capabilities &= ~phylink_caps_params[i].mask;
}
EXPORT_SYMBOL_GPL(phylink_limit_mac_speed);

 
static unsigned long phylink_cap_from_speed_duplex(int speed,
						   unsigned int duplex)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(phylink_caps_params); i++) {
		if (speed == phylink_caps_params[i].speed &&
		    duplex == phylink_caps_params[i].duplex)
			return phylink_caps_params[i].mask;
	}

	return 0;
}

 
unsigned long phylink_get_capabilities(phy_interface_t interface,
				       unsigned long mac_capabilities,
				       int rate_matching)
{
	int max_speed = phylink_interface_max_speed(interface);
	unsigned long caps = MAC_SYM_PAUSE | MAC_ASYM_PAUSE;
	unsigned long matched_caps = 0;

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		caps |= MAC_10000FD | MAC_5000FD | MAC_2500FD;
		fallthrough;

	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_PSGMII:
	case PHY_INTERFACE_MODE_QSGMII:
	case PHY_INTERFACE_MODE_QUSGMII:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_GMII:
		caps |= MAC_1000HD | MAC_1000FD;
		fallthrough;

	case PHY_INTERFACE_MODE_REVRMII:
	case PHY_INTERFACE_MODE_RMII:
	case PHY_INTERFACE_MODE_SMII:
	case PHY_INTERFACE_MODE_REVMII:
	case PHY_INTERFACE_MODE_MII:
		caps |= MAC_10HD | MAC_10FD;
		fallthrough;

	case PHY_INTERFACE_MODE_100BASEX:
		caps |= MAC_100HD | MAC_100FD;
		break;

	case PHY_INTERFACE_MODE_TBI:
	case PHY_INTERFACE_MODE_MOCA:
	case PHY_INTERFACE_MODE_RTBI:
	case PHY_INTERFACE_MODE_1000BASEX:
		caps |= MAC_1000HD;
		fallthrough;
	case PHY_INTERFACE_MODE_1000BASEKX:
	case PHY_INTERFACE_MODE_TRGMII:
		caps |= MAC_1000FD;
		break;

	case PHY_INTERFACE_MODE_2500BASEX:
		caps |= MAC_2500FD;
		break;

	case PHY_INTERFACE_MODE_5GBASER:
		caps |= MAC_5000FD;
		break;

	case PHY_INTERFACE_MODE_XGMII:
	case PHY_INTERFACE_MODE_RXAUI:
	case PHY_INTERFACE_MODE_XAUI:
	case PHY_INTERFACE_MODE_10GBASER:
	case PHY_INTERFACE_MODE_10GKR:
		caps |= MAC_10000FD;
		break;

	case PHY_INTERFACE_MODE_25GBASER:
		caps |= MAC_25000FD;
		break;

	case PHY_INTERFACE_MODE_XLGMII:
		caps |= MAC_40000FD;
		break;

	case PHY_INTERFACE_MODE_INTERNAL:
		caps |= ~0;
		break;

	case PHY_INTERFACE_MODE_NA:
	case PHY_INTERFACE_MODE_MAX:
		break;
	}

	switch (rate_matching) {
	case RATE_MATCH_OPEN_LOOP:
		 
		fallthrough;
	case RATE_MATCH_NONE:
		matched_caps = 0;
		break;
	case RATE_MATCH_PAUSE: {
		 
		if (!(mac_capabilities & MAC_SYM_PAUSE) ||
		    !(mac_capabilities & MAC_ASYM_PAUSE))
			break;

		 
		if (mac_capabilities &
		    phylink_cap_from_speed_duplex(max_speed, DUPLEX_FULL)) {
			 
			matched_caps = GENMASK(__fls(caps), __fls(MAC_10HD));
			matched_caps &= ~(MAC_1000HD | MAC_100HD | MAC_10HD);
		}
		break;
	}
	case RATE_MATCH_CRS:
		 
		if (mac_capabilities &
		    phylink_cap_from_speed_duplex(max_speed, DUPLEX_HALF)) {
			matched_caps = GENMASK(__fls(caps), __fls(MAC_10HD));
			matched_caps &= mac_capabilities;
		}
		break;
	}

	return (caps & mac_capabilities) | matched_caps;
}
EXPORT_SYMBOL_GPL(phylink_get_capabilities);

 
void phylink_validate_mask_caps(unsigned long *supported,
				struct phylink_link_state *state,
				unsigned long mac_capabilities)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask) = { 0, };
	unsigned long caps;

	phylink_set_port_modes(mask);
	phylink_set(mask, Autoneg);
	caps = phylink_get_capabilities(state->interface, mac_capabilities,
					state->rate_matching);
	phylink_caps_to_linkmodes(mask, caps);

	linkmode_and(supported, supported, mask);
	linkmode_and(state->advertising, state->advertising, mask);
}
EXPORT_SYMBOL_GPL(phylink_validate_mask_caps);

 
void phylink_generic_validate(struct phylink_config *config,
			      unsigned long *supported,
			      struct phylink_link_state *state)
{
	phylink_validate_mask_caps(supported, state, config->mac_capabilities);
}
EXPORT_SYMBOL_GPL(phylink_generic_validate);

static int phylink_validate_mac_and_pcs(struct phylink *pl,
					unsigned long *supported,
					struct phylink_link_state *state)
{
	struct phylink_pcs *pcs;
	int ret;

	 
	if (pl->using_mac_select_pcs) {
		pcs = pl->mac_ops->mac_select_pcs(pl->config, state->interface);
		if (IS_ERR(pcs))
			return PTR_ERR(pcs);
	} else {
		pcs = pl->pcs;
	}

	if (pcs) {
		 
		if (!pcs->ops) {
			phylink_err(pl, "interface %s: uninitialised PCS\n",
				    phy_modes(state->interface));
			dump_stack();
			return -EINVAL;
		}

		 
		if (pcs->ops->pcs_validate) {
			ret = pcs->ops->pcs_validate(pcs, supported, state);
			if (ret < 0 || phylink_is_empty_linkmode(supported))
				return -EINVAL;

			 
			linkmode_and(state->advertising, state->advertising,
				     supported);
		}
	}

	 
	if (pl->mac_ops->validate)
		pl->mac_ops->validate(pl->config, supported, state);
	else
		phylink_generic_validate(pl->config, supported, state);

	return phylink_is_empty_linkmode(supported) ? -EINVAL : 0;
}

static int phylink_validate_mask(struct phylink *pl, unsigned long *supported,
				 struct phylink_link_state *state,
				 const unsigned long *interfaces)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(all_adv) = { 0, };
	__ETHTOOL_DECLARE_LINK_MODE_MASK(all_s) = { 0, };
	__ETHTOOL_DECLARE_LINK_MODE_MASK(s);
	struct phylink_link_state t;
	int intf;

	for (intf = 0; intf < PHY_INTERFACE_MODE_MAX; intf++) {
		if (test_bit(intf, interfaces)) {
			linkmode_copy(s, supported);

			t = *state;
			t.interface = intf;
			if (!phylink_validate_mac_and_pcs(pl, s, &t)) {
				linkmode_or(all_s, all_s, s);
				linkmode_or(all_adv, all_adv, t.advertising);
			}
		}
	}

	linkmode_copy(supported, all_s);
	linkmode_copy(state->advertising, all_adv);

	return phylink_is_empty_linkmode(supported) ? -EINVAL : 0;
}

static int phylink_validate(struct phylink *pl, unsigned long *supported,
			    struct phylink_link_state *state)
{
	const unsigned long *interfaces = pl->config->supported_interfaces;

	if (state->interface == PHY_INTERFACE_MODE_NA)
		return phylink_validate_mask(pl, supported, state, interfaces);

	if (!test_bit(state->interface, interfaces))
		return -EINVAL;

	return phylink_validate_mac_and_pcs(pl, supported, state);
}

static int phylink_parse_fixedlink(struct phylink *pl,
				   const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *fixed_node;
	bool pause, asym_pause, autoneg;
	const struct phy_setting *s;
	struct gpio_desc *desc;
	u32 speed;
	int ret;

	fixed_node = fwnode_get_named_child_node(fwnode, "fixed-link");
	if (fixed_node) {
		ret = fwnode_property_read_u32(fixed_node, "speed", &speed);

		pl->link_config.speed = speed;
		pl->link_config.duplex = DUPLEX_HALF;

		if (fwnode_property_read_bool(fixed_node, "full-duplex"))
			pl->link_config.duplex = DUPLEX_FULL;

		 
		if (fwnode_property_read_bool(fixed_node, "pause"))
			__set_bit(ETHTOOL_LINK_MODE_Pause_BIT,
				  pl->link_config.lp_advertising);
		if (fwnode_property_read_bool(fixed_node, "asym-pause"))
			__set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
				  pl->link_config.lp_advertising);

		if (ret == 0) {
			desc = fwnode_gpiod_get_index(fixed_node, "link", 0,
						      GPIOD_IN, "?");

			if (!IS_ERR(desc))
				pl->link_gpio = desc;
			else if (desc == ERR_PTR(-EPROBE_DEFER))
				ret = -EPROBE_DEFER;
		}
		fwnode_handle_put(fixed_node);

		if (ret)
			return ret;
	} else {
		u32 prop[5];

		ret = fwnode_property_read_u32_array(fwnode, "fixed-link",
						     NULL, 0);
		if (ret != ARRAY_SIZE(prop)) {
			phylink_err(pl, "broken fixed-link?\n");
			return -EINVAL;
		}

		ret = fwnode_property_read_u32_array(fwnode, "fixed-link",
						     prop, ARRAY_SIZE(prop));
		if (!ret) {
			pl->link_config.duplex = prop[1] ?
						DUPLEX_FULL : DUPLEX_HALF;
			pl->link_config.speed = prop[2];
			if (prop[3])
				__set_bit(ETHTOOL_LINK_MODE_Pause_BIT,
					  pl->link_config.lp_advertising);
			if (prop[4])
				__set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
					  pl->link_config.lp_advertising);
		}
	}

	if (pl->link_config.speed > SPEED_1000 &&
	    pl->link_config.duplex != DUPLEX_FULL)
		phylink_warn(pl, "fixed link specifies half duplex for %dMbps link?\n",
			     pl->link_config.speed);

	bitmap_fill(pl->supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
	linkmode_copy(pl->link_config.advertising, pl->supported);
	phylink_validate(pl, pl->supported, &pl->link_config);

	pause = phylink_test(pl->supported, Pause);
	asym_pause = phylink_test(pl->supported, Asym_Pause);
	autoneg = phylink_test(pl->supported, Autoneg);
	s = phy_lookup_setting(pl->link_config.speed, pl->link_config.duplex,
			       pl->supported, true);
	linkmode_zero(pl->supported);
	phylink_set(pl->supported, MII);

	if (pause)
		phylink_set(pl->supported, Pause);

	if (asym_pause)
		phylink_set(pl->supported, Asym_Pause);

	if (autoneg)
		phylink_set(pl->supported, Autoneg);

	if (s) {
		__set_bit(s->bit, pl->supported);
		__set_bit(s->bit, pl->link_config.lp_advertising);
	} else {
		phylink_warn(pl, "fixed link %s duplex %dMbps not recognised\n",
			     pl->link_config.duplex == DUPLEX_FULL ? "full" : "half",
			     pl->link_config.speed);
	}

	linkmode_and(pl->link_config.advertising, pl->link_config.advertising,
		     pl->supported);

	pl->link_config.link = 1;
	pl->link_config.an_complete = 1;

	return 0;
}

static int phylink_parse_mode(struct phylink *pl,
			      const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *dn;
	const char *managed;

	dn = fwnode_get_named_child_node(fwnode, "fixed-link");
	if (dn || fwnode_property_present(fwnode, "fixed-link"))
		pl->cfg_link_an_mode = MLO_AN_FIXED;
	fwnode_handle_put(dn);

	if ((fwnode_property_read_string(fwnode, "managed", &managed) == 0 &&
	     strcmp(managed, "in-band-status") == 0) ||
	    pl->config->ovr_an_inband) {
		if (pl->cfg_link_an_mode == MLO_AN_FIXED) {
			phylink_err(pl,
				    "can't use both fixed-link and in-band-status\n");
			return -EINVAL;
		}

		linkmode_zero(pl->supported);
		phylink_set(pl->supported, MII);
		phylink_set(pl->supported, Autoneg);
		phylink_set(pl->supported, Asym_Pause);
		phylink_set(pl->supported, Pause);
		pl->cfg_link_an_mode = MLO_AN_INBAND;

		switch (pl->link_config.interface) {
		case PHY_INTERFACE_MODE_SGMII:
		case PHY_INTERFACE_MODE_PSGMII:
		case PHY_INTERFACE_MODE_QSGMII:
		case PHY_INTERFACE_MODE_QUSGMII:
		case PHY_INTERFACE_MODE_RGMII:
		case PHY_INTERFACE_MODE_RGMII_ID:
		case PHY_INTERFACE_MODE_RGMII_RXID:
		case PHY_INTERFACE_MODE_RGMII_TXID:
		case PHY_INTERFACE_MODE_RTBI:
			phylink_set(pl->supported, 10baseT_Half);
			phylink_set(pl->supported, 10baseT_Full);
			phylink_set(pl->supported, 100baseT_Half);
			phylink_set(pl->supported, 100baseT_Full);
			phylink_set(pl->supported, 1000baseT_Half);
			phylink_set(pl->supported, 1000baseT_Full);
			break;

		case PHY_INTERFACE_MODE_1000BASEX:
			phylink_set(pl->supported, 1000baseX_Full);
			break;

		case PHY_INTERFACE_MODE_2500BASEX:
			phylink_set(pl->supported, 2500baseX_Full);
			break;

		case PHY_INTERFACE_MODE_5GBASER:
			phylink_set(pl->supported, 5000baseT_Full);
			break;

		case PHY_INTERFACE_MODE_25GBASER:
			phylink_set(pl->supported, 25000baseCR_Full);
			phylink_set(pl->supported, 25000baseKR_Full);
			phylink_set(pl->supported, 25000baseSR_Full);
			fallthrough;
		case PHY_INTERFACE_MODE_USXGMII:
		case PHY_INTERFACE_MODE_10GKR:
		case PHY_INTERFACE_MODE_10GBASER:
			phylink_set(pl->supported, 10baseT_Half);
			phylink_set(pl->supported, 10baseT_Full);
			phylink_set(pl->supported, 100baseT_Half);
			phylink_set(pl->supported, 100baseT_Full);
			phylink_set(pl->supported, 1000baseT_Half);
			phylink_set(pl->supported, 1000baseT_Full);
			phylink_set(pl->supported, 1000baseX_Full);
			phylink_set(pl->supported, 1000baseKX_Full);
			phylink_set(pl->supported, 2500baseT_Full);
			phylink_set(pl->supported, 2500baseX_Full);
			phylink_set(pl->supported, 5000baseT_Full);
			phylink_set(pl->supported, 10000baseT_Full);
			phylink_set(pl->supported, 10000baseKR_Full);
			phylink_set(pl->supported, 10000baseKX4_Full);
			phylink_set(pl->supported, 10000baseCR_Full);
			phylink_set(pl->supported, 10000baseSR_Full);
			phylink_set(pl->supported, 10000baseLR_Full);
			phylink_set(pl->supported, 10000baseLRM_Full);
			phylink_set(pl->supported, 10000baseER_Full);
			break;

		case PHY_INTERFACE_MODE_XLGMII:
			phylink_set(pl->supported, 25000baseCR_Full);
			phylink_set(pl->supported, 25000baseKR_Full);
			phylink_set(pl->supported, 25000baseSR_Full);
			phylink_set(pl->supported, 40000baseKR4_Full);
			phylink_set(pl->supported, 40000baseCR4_Full);
			phylink_set(pl->supported, 40000baseSR4_Full);
			phylink_set(pl->supported, 40000baseLR4_Full);
			phylink_set(pl->supported, 50000baseCR2_Full);
			phylink_set(pl->supported, 50000baseKR2_Full);
			phylink_set(pl->supported, 50000baseSR2_Full);
			phylink_set(pl->supported, 50000baseKR_Full);
			phylink_set(pl->supported, 50000baseSR_Full);
			phylink_set(pl->supported, 50000baseCR_Full);
			phylink_set(pl->supported, 50000baseLR_ER_FR_Full);
			phylink_set(pl->supported, 50000baseDR_Full);
			phylink_set(pl->supported, 100000baseKR4_Full);
			phylink_set(pl->supported, 100000baseSR4_Full);
			phylink_set(pl->supported, 100000baseCR4_Full);
			phylink_set(pl->supported, 100000baseLR4_ER4_Full);
			phylink_set(pl->supported, 100000baseKR2_Full);
			phylink_set(pl->supported, 100000baseSR2_Full);
			phylink_set(pl->supported, 100000baseCR2_Full);
			phylink_set(pl->supported, 100000baseLR2_ER2_FR2_Full);
			phylink_set(pl->supported, 100000baseDR2_Full);
			break;

		default:
			phylink_err(pl,
				    "incorrect link mode %s for in-band status\n",
				    phy_modes(pl->link_config.interface));
			return -EINVAL;
		}

		linkmode_copy(pl->link_config.advertising, pl->supported);

		if (phylink_validate(pl, pl->supported, &pl->link_config)) {
			phylink_err(pl,
				    "failed to validate link configuration for in-band status\n");
			return -EINVAL;
		}
	}

	return 0;
}

static void phylink_apply_manual_flow(struct phylink *pl,
				      struct phylink_link_state *state)
{
	 
	if (!linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
			       state->advertising))
		state->pause &= ~MLO_PAUSE_AN;

	 
	if (!(pl->link_config.pause & MLO_PAUSE_AN))
		state->pause = pl->link_config.pause;
}

static void phylink_resolve_an_pause(struct phylink_link_state *state)
{
	bool tx_pause, rx_pause;

	if (state->duplex == DUPLEX_FULL) {
		linkmode_resolve_pause(state->advertising,
				       state->lp_advertising,
				       &tx_pause, &rx_pause);
		if (tx_pause)
			state->pause |= MLO_PAUSE_TX;
		if (rx_pause)
			state->pause |= MLO_PAUSE_RX;
	}
}

static void phylink_pcs_pre_config(struct phylink_pcs *pcs,
				   phy_interface_t interface)
{
	if (pcs && pcs->ops->pcs_pre_config)
		pcs->ops->pcs_pre_config(pcs, interface);
}

static int phylink_pcs_post_config(struct phylink_pcs *pcs,
				   phy_interface_t interface)
{
	int err = 0;

	if (pcs && pcs->ops->pcs_post_config)
		err = pcs->ops->pcs_post_config(pcs, interface);

	return err;
}

static void phylink_pcs_disable(struct phylink_pcs *pcs)
{
	if (pcs && pcs->ops->pcs_disable)
		pcs->ops->pcs_disable(pcs);
}

static int phylink_pcs_enable(struct phylink_pcs *pcs)
{
	int err = 0;

	if (pcs && pcs->ops->pcs_enable)
		err = pcs->ops->pcs_enable(pcs);

	return err;
}

static int phylink_pcs_config(struct phylink_pcs *pcs, unsigned int neg_mode,
			      const struct phylink_link_state *state,
			      bool permit_pause_to_mac)
{
	if (!pcs)
		return 0;

	return pcs->ops->pcs_config(pcs, neg_mode, state->interface,
				    state->advertising, permit_pause_to_mac);
}

static void phylink_pcs_link_up(struct phylink_pcs *pcs, unsigned int neg_mode,
				phy_interface_t interface, int speed,
				int duplex)
{
	if (pcs && pcs->ops->pcs_link_up)
		pcs->ops->pcs_link_up(pcs, neg_mode, interface, speed, duplex);
}

static void phylink_pcs_poll_stop(struct phylink *pl)
{
	if (pl->cfg_link_an_mode == MLO_AN_INBAND)
		del_timer(&pl->link_poll);
}

static void phylink_pcs_poll_start(struct phylink *pl)
{
	if (pl->pcs && pl->pcs->poll && pl->cfg_link_an_mode == MLO_AN_INBAND)
		mod_timer(&pl->link_poll, jiffies + HZ);
}

static void phylink_mac_config(struct phylink *pl,
			       const struct phylink_link_state *state)
{
	struct phylink_link_state st = *state;

	 
	linkmode_zero(st.lp_advertising);
	st.speed = SPEED_UNKNOWN;
	st.duplex = DUPLEX_UNKNOWN;
	st.an_complete = false;
	st.link = false;

	phylink_dbg(pl,
		    "%s: mode=%s/%s/%s adv=%*pb pause=%02x\n",
		    __func__, phylink_an_mode_str(pl->cur_link_an_mode),
		    phy_modes(st.interface),
		    phy_rate_matching_to_str(st.rate_matching),
		    __ETHTOOL_LINK_MODE_MASK_NBITS, st.advertising,
		    st.pause);

	pl->mac_ops->mac_config(pl->config, pl->cur_link_an_mode, &st);
}

static void phylink_pcs_an_restart(struct phylink *pl)
{
	if (pl->pcs && linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
					 pl->link_config.advertising) &&
	    phy_interface_mode_is_8023z(pl->link_config.interface) &&
	    phylink_autoneg_inband(pl->cur_link_an_mode))
		pl->pcs->ops->pcs_an_restart(pl->pcs);
}

static void phylink_major_config(struct phylink *pl, bool restart,
				  const struct phylink_link_state *state)
{
	struct phylink_pcs *pcs = NULL;
	bool pcs_changed = false;
	unsigned int rate_kbd;
	unsigned int neg_mode;
	int err;

	phylink_dbg(pl, "major config %s\n", phy_modes(state->interface));

	pl->pcs_neg_mode = phylink_pcs_neg_mode(pl->cur_link_an_mode,
						state->interface,
						state->advertising);

	if (pl->using_mac_select_pcs) {
		pcs = pl->mac_ops->mac_select_pcs(pl->config, state->interface);
		if (IS_ERR(pcs)) {
			phylink_err(pl,
				    "mac_select_pcs unexpectedly failed: %pe\n",
				    pcs);
			return;
		}

		pcs_changed = pcs && pl->pcs != pcs;
	}

	phylink_pcs_poll_stop(pl);

	if (pl->mac_ops->mac_prepare) {
		err = pl->mac_ops->mac_prepare(pl->config, pl->cur_link_an_mode,
					       state->interface);
		if (err < 0) {
			phylink_err(pl, "mac_prepare failed: %pe\n",
				    ERR_PTR(err));
			return;
		}
	}

	 
	if (pcs_changed) {
		phylink_pcs_disable(pl->pcs);

		if (pl->pcs)
			pl->pcs->phylink = NULL;

		pcs->phylink = pl;

		pl->pcs = pcs;
	}

	if (pl->pcs)
		phylink_pcs_pre_config(pl->pcs, state->interface);

	phylink_mac_config(pl, state);

	if (pl->pcs)
		phylink_pcs_post_config(pl->pcs, state->interface);

	if (pl->pcs_state == PCS_STATE_STARTING || pcs_changed)
		phylink_pcs_enable(pl->pcs);

	neg_mode = pl->cur_link_an_mode;
	if (pl->pcs && pl->pcs->neg_mode)
		neg_mode = pl->pcs_neg_mode;

	err = phylink_pcs_config(pl->pcs, neg_mode, state,
				 !!(pl->link_config.pause & MLO_PAUSE_AN));
	if (err < 0)
		phylink_err(pl, "pcs_config failed: %pe\n",
			    ERR_PTR(err));
	else if (err > 0)
		restart = true;

	if (restart)
		phylink_pcs_an_restart(pl);

	if (pl->mac_ops->mac_finish) {
		err = pl->mac_ops->mac_finish(pl->config, pl->cur_link_an_mode,
					      state->interface);
		if (err < 0)
			phylink_err(pl, "mac_finish failed: %pe\n",
				    ERR_PTR(err));
	}

	if (pl->sfp_bus) {
		rate_kbd = phylink_interface_signal_rate(state->interface);
		if (rate_kbd)
			sfp_upstream_set_signal_rate(pl->sfp_bus, rate_kbd);
	}

	phylink_pcs_poll_start(pl);
}

 
static int phylink_change_inband_advert(struct phylink *pl)
{
	unsigned int neg_mode;
	int ret;

	if (test_bit(PHYLINK_DISABLE_STOPPED, &pl->phylink_disable_state))
		return 0;

	phylink_dbg(pl, "%s: mode=%s/%s adv=%*pb pause=%02x\n", __func__,
		    phylink_an_mode_str(pl->cur_link_an_mode),
		    phy_modes(pl->link_config.interface),
		    __ETHTOOL_LINK_MODE_MASK_NBITS, pl->link_config.advertising,
		    pl->link_config.pause);

	 
	pl->pcs_neg_mode = phylink_pcs_neg_mode(pl->cur_link_an_mode,
					pl->link_config.interface,
					pl->link_config.advertising);

	neg_mode = pl->cur_link_an_mode;
	if (pl->pcs->neg_mode)
		neg_mode = pl->pcs_neg_mode;

	 
	ret = phylink_pcs_config(pl->pcs, neg_mode, &pl->link_config,
				 !!(pl->link_config.pause & MLO_PAUSE_AN));
	if (ret < 0)
		return ret;

	if (ret > 0)
		phylink_pcs_an_restart(pl);

	return 0;
}

static void phylink_mac_pcs_get_state(struct phylink *pl,
				      struct phylink_link_state *state)
{
	linkmode_copy(state->advertising, pl->link_config.advertising);
	linkmode_zero(state->lp_advertising);
	state->interface = pl->link_config.interface;
	state->rate_matching = pl->link_config.rate_matching;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
			      state->advertising)) {
		state->speed = SPEED_UNKNOWN;
		state->duplex = DUPLEX_UNKNOWN;
		state->pause = MLO_PAUSE_NONE;
	} else {
		state->speed =  pl->link_config.speed;
		state->duplex = pl->link_config.duplex;
		state->pause = pl->link_config.pause;
	}
	state->an_complete = 0;
	state->link = 1;

	if (pl->pcs)
		pl->pcs->ops->pcs_get_state(pl->pcs, state);
	else
		state->link = 0;
}

 
static void phylink_get_fixed_state(struct phylink *pl,
				    struct phylink_link_state *state)
{
	*state = pl->link_config;
	if (pl->config->get_fixed_state)
		pl->config->get_fixed_state(pl->config, state);
	else if (pl->link_gpio)
		state->link = !!gpiod_get_value_cansleep(pl->link_gpio);

	state->pause = MLO_PAUSE_NONE;
	phylink_resolve_an_pause(state);
}

static void phylink_mac_initial_config(struct phylink *pl, bool force_restart)
{
	struct phylink_link_state link_state;

	switch (pl->cur_link_an_mode) {
	case MLO_AN_PHY:
		link_state = pl->phy_state;
		break;

	case MLO_AN_FIXED:
		phylink_get_fixed_state(pl, &link_state);
		break;

	case MLO_AN_INBAND:
		link_state = pl->link_config;
		if (link_state.interface == PHY_INTERFACE_MODE_SGMII)
			link_state.pause = MLO_PAUSE_NONE;
		break;

	default:  
		return;
	}

	link_state.link = false;

	phylink_apply_manual_flow(pl, &link_state);
	phylink_major_config(pl, force_restart, &link_state);
}

static const char *phylink_pause_to_str(int pause)
{
	switch (pause & MLO_PAUSE_TXRX_MASK) {
	case MLO_PAUSE_TX | MLO_PAUSE_RX:
		return "rx/tx";
	case MLO_PAUSE_TX:
		return "tx";
	case MLO_PAUSE_RX:
		return "rx";
	default:
		return "off";
	}
}

static void phylink_link_up(struct phylink *pl,
			    struct phylink_link_state link_state)
{
	struct net_device *ndev = pl->netdev;
	unsigned int neg_mode;
	int speed, duplex;
	bool rx_pause;

	speed = link_state.speed;
	duplex = link_state.duplex;
	rx_pause = !!(link_state.pause & MLO_PAUSE_RX);

	switch (link_state.rate_matching) {
	case RATE_MATCH_PAUSE:
		 
		speed = phylink_interface_max_speed(link_state.interface);
		duplex = DUPLEX_FULL;
		rx_pause = true;
		break;

	case RATE_MATCH_CRS:
		 
		speed = phylink_interface_max_speed(link_state.interface);
		duplex = DUPLEX_HALF;
		break;
	}

	pl->cur_interface = link_state.interface;

	neg_mode = pl->cur_link_an_mode;
	if (pl->pcs && pl->pcs->neg_mode)
		neg_mode = pl->pcs_neg_mode;

	phylink_pcs_link_up(pl->pcs, neg_mode, pl->cur_interface, speed,
			    duplex);

	pl->mac_ops->mac_link_up(pl->config, pl->phydev, pl->cur_link_an_mode,
				 pl->cur_interface, speed, duplex,
				 !!(link_state.pause & MLO_PAUSE_TX), rx_pause);

	if (ndev)
		netif_carrier_on(ndev);

	phylink_info(pl,
		     "Link is Up - %s/%s - flow control %s\n",
		     phy_speed_to_str(link_state.speed),
		     phy_duplex_to_str(link_state.duplex),
		     phylink_pause_to_str(link_state.pause));
}

static void phylink_link_down(struct phylink *pl)
{
	struct net_device *ndev = pl->netdev;

	if (ndev)
		netif_carrier_off(ndev);
	pl->mac_ops->mac_link_down(pl->config, pl->cur_link_an_mode,
				   pl->cur_interface);
	phylink_info(pl, "Link is Down\n");
}

static void phylink_resolve(struct work_struct *w)
{
	struct phylink *pl = container_of(w, struct phylink, resolve);
	struct phylink_link_state link_state;
	struct net_device *ndev = pl->netdev;
	bool mac_config = false;
	bool retrigger = false;
	bool cur_link_state;

	mutex_lock(&pl->state_mutex);
	if (pl->netdev)
		cur_link_state = netif_carrier_ok(ndev);
	else
		cur_link_state = pl->old_link_state;

	if (pl->phylink_disable_state) {
		pl->mac_link_dropped = false;
		link_state.link = false;
	} else if (pl->mac_link_dropped) {
		link_state.link = false;
		retrigger = true;
	} else {
		switch (pl->cur_link_an_mode) {
		case MLO_AN_PHY:
			link_state = pl->phy_state;
			phylink_apply_manual_flow(pl, &link_state);
			mac_config = link_state.link;
			break;

		case MLO_AN_FIXED:
			phylink_get_fixed_state(pl, &link_state);
			mac_config = link_state.link;
			break;

		case MLO_AN_INBAND:
			phylink_mac_pcs_get_state(pl, &link_state);

			 
			if (!link_state.link) {
				if (cur_link_state)
					retrigger = true;
				else
					phylink_mac_pcs_get_state(pl,
								  &link_state);
			}

			 
			if (pl->phydev)
				link_state.link &= pl->phy_state.link;

			 
			if (pl->phydev && pl->phy_state.link) {
				 
				if (link_state.interface !=
				    pl->phy_state.interface) {
					retrigger = true;
					link_state.link = false;
				}
				link_state.interface = pl->phy_state.interface;

				 
				if (pl->phy_state.rate_matching) {
					link_state.rate_matching =
						pl->phy_state.rate_matching;
					link_state.speed = pl->phy_state.speed;
					link_state.duplex =
						pl->phy_state.duplex;
				}

				 
				link_state.pause = pl->phy_state.pause;
				mac_config = true;
			}
			phylink_apply_manual_flow(pl, &link_state);
			break;
		}
	}

	if (mac_config) {
		if (link_state.interface != pl->link_config.interface) {
			 
			if (cur_link_state) {
				phylink_link_down(pl);
				cur_link_state = false;
			}
			phylink_major_config(pl, false, &link_state);
			pl->link_config.interface = link_state.interface;
		}
	}

	if (link_state.link != cur_link_state) {
		pl->old_link_state = link_state.link;
		if (!link_state.link)
			phylink_link_down(pl);
		else
			phylink_link_up(pl, link_state);
	}
	if (!link_state.link && retrigger) {
		pl->mac_link_dropped = false;
		queue_work(system_power_efficient_wq, &pl->resolve);
	}
	mutex_unlock(&pl->state_mutex);
}

static void phylink_run_resolve(struct phylink *pl)
{
	if (!pl->phylink_disable_state)
		queue_work(system_power_efficient_wq, &pl->resolve);
}

static void phylink_run_resolve_and_disable(struct phylink *pl, int bit)
{
	unsigned long state = pl->phylink_disable_state;

	set_bit(bit, &pl->phylink_disable_state);
	if (state == 0) {
		queue_work(system_power_efficient_wq, &pl->resolve);
		flush_work(&pl->resolve);
	}
}

static void phylink_enable_and_run_resolve(struct phylink *pl, int bit)
{
	clear_bit(bit, &pl->phylink_disable_state);
	phylink_run_resolve(pl);
}

static void phylink_fixed_poll(struct timer_list *t)
{
	struct phylink *pl = container_of(t, struct phylink, link_poll);

	mod_timer(t, jiffies + HZ);

	phylink_run_resolve(pl);
}

static const struct sfp_upstream_ops sfp_phylink_ops;

static int phylink_register_sfp(struct phylink *pl,
				const struct fwnode_handle *fwnode)
{
	struct sfp_bus *bus;
	int ret;

	if (!fwnode)
		return 0;

	bus = sfp_bus_find_fwnode(fwnode);
	if (IS_ERR(bus)) {
		phylink_err(pl, "unable to attach SFP bus: %pe\n", bus);
		return PTR_ERR(bus);
	}

	pl->sfp_bus = bus;

	ret = sfp_bus_add_upstream(bus, pl, &sfp_phylink_ops);
	sfp_bus_put(bus);

	return ret;
}

 
struct phylink *phylink_create(struct phylink_config *config,
			       const struct fwnode_handle *fwnode,
			       phy_interface_t iface,
			       const struct phylink_mac_ops *mac_ops)
{
	bool using_mac_select_pcs = false;
	struct phylink *pl;
	int ret;

	 
	if (phy_interface_empty(config->supported_interfaces)) {
		dev_err(config->dev,
			"phylink: error: empty supported_interfaces\n");
		return ERR_PTR(-EINVAL);
	}

	if (mac_ops->mac_select_pcs &&
	    mac_ops->mac_select_pcs(config, PHY_INTERFACE_MODE_NA) !=
	      ERR_PTR(-EOPNOTSUPP))
		using_mac_select_pcs = true;

	pl = kzalloc(sizeof(*pl), GFP_KERNEL);
	if (!pl)
		return ERR_PTR(-ENOMEM);

	mutex_init(&pl->state_mutex);
	INIT_WORK(&pl->resolve, phylink_resolve);

	pl->config = config;
	if (config->type == PHYLINK_NETDEV) {
		pl->netdev = to_net_dev(config->dev);
		netif_carrier_off(pl->netdev);
	} else if (config->type == PHYLINK_DEV) {
		pl->dev = config->dev;
	} else {
		kfree(pl);
		return ERR_PTR(-EINVAL);
	}

	pl->using_mac_select_pcs = using_mac_select_pcs;
	pl->phy_state.interface = iface;
	pl->link_interface = iface;
	if (iface == PHY_INTERFACE_MODE_MOCA)
		pl->link_port = PORT_BNC;
	else
		pl->link_port = PORT_MII;
	pl->link_config.interface = iface;
	pl->link_config.pause = MLO_PAUSE_AN;
	pl->link_config.speed = SPEED_UNKNOWN;
	pl->link_config.duplex = DUPLEX_UNKNOWN;
	pl->pcs_state = PCS_STATE_DOWN;
	pl->mac_ops = mac_ops;
	__set_bit(PHYLINK_DISABLE_STOPPED, &pl->phylink_disable_state);
	timer_setup(&pl->link_poll, phylink_fixed_poll, 0);

	bitmap_fill(pl->supported, __ETHTOOL_LINK_MODE_MASK_NBITS);
	linkmode_copy(pl->link_config.advertising, pl->supported);
	phylink_validate(pl, pl->supported, &pl->link_config);

	ret = phylink_parse_mode(pl, fwnode);
	if (ret < 0) {
		kfree(pl);
		return ERR_PTR(ret);
	}

	if (pl->cfg_link_an_mode == MLO_AN_FIXED) {
		ret = phylink_parse_fixedlink(pl, fwnode);
		if (ret < 0) {
			kfree(pl);
			return ERR_PTR(ret);
		}
	}

	pl->cur_link_an_mode = pl->cfg_link_an_mode;

	ret = phylink_register_sfp(pl, fwnode);
	if (ret < 0) {
		kfree(pl);
		return ERR_PTR(ret);
	}

	return pl;
}
EXPORT_SYMBOL_GPL(phylink_create);

 
void phylink_destroy(struct phylink *pl)
{
	sfp_bus_del_upstream(pl->sfp_bus);
	if (pl->link_gpio)
		gpiod_put(pl->link_gpio);

	cancel_work_sync(&pl->resolve);
	kfree(pl);
}
EXPORT_SYMBOL_GPL(phylink_destroy);

 
bool phylink_expects_phy(struct phylink *pl)
{
	if (pl->cfg_link_an_mode == MLO_AN_FIXED ||
	    (pl->cfg_link_an_mode == MLO_AN_INBAND &&
	     phy_interface_mode_is_8023z(pl->link_config.interface)))
		return false;
	return true;
}
EXPORT_SYMBOL_GPL(phylink_expects_phy);

static void phylink_phy_change(struct phy_device *phydev, bool up)
{
	struct phylink *pl = phydev->phylink;
	bool tx_pause, rx_pause;

	phy_get_pause(phydev, &tx_pause, &rx_pause);

	mutex_lock(&pl->state_mutex);
	pl->phy_state.speed = phydev->speed;
	pl->phy_state.duplex = phydev->duplex;
	pl->phy_state.rate_matching = phydev->rate_matching;
	pl->phy_state.pause = MLO_PAUSE_NONE;
	if (tx_pause)
		pl->phy_state.pause |= MLO_PAUSE_TX;
	if (rx_pause)
		pl->phy_state.pause |= MLO_PAUSE_RX;
	pl->phy_state.interface = phydev->interface;
	pl->phy_state.link = up;
	mutex_unlock(&pl->state_mutex);

	phylink_run_resolve(pl);

	phylink_dbg(pl, "phy link %s %s/%s/%s/%s/%s\n", up ? "up" : "down",
		    phy_modes(phydev->interface),
		    phy_speed_to_str(phydev->speed),
		    phy_duplex_to_str(phydev->duplex),
		    phy_rate_matching_to_str(phydev->rate_matching),
		    phylink_pause_to_str(pl->phy_state.pause));
}

static int phylink_bringup_phy(struct phylink *pl, struct phy_device *phy,
			       phy_interface_t interface)
{
	struct phylink_link_state config;
	__ETHTOOL_DECLARE_LINK_MODE_MASK(supported);
	char *irq_str;
	int ret;

	 
	phy_support_asym_pause(phy);

	memset(&config, 0, sizeof(config));
	linkmode_copy(supported, phy->supported);
	linkmode_copy(config.advertising, phy->advertising);

	 
	config.rate_matching = phy_get_rate_matching(phy, interface);

	 
	if (phy->is_c45 && config.rate_matching == RATE_MATCH_NONE &&
	    interface != PHY_INTERFACE_MODE_RXAUI &&
	    interface != PHY_INTERFACE_MODE_XAUI &&
	    interface != PHY_INTERFACE_MODE_USXGMII)
		config.interface = PHY_INTERFACE_MODE_NA;
	else
		config.interface = interface;

	ret = phylink_validate(pl, supported, &config);
	if (ret) {
		phylink_warn(pl, "validation of %s with support %*pb and advertisement %*pb failed: %pe\n",
			     phy_modes(config.interface),
			     __ETHTOOL_LINK_MODE_MASK_NBITS, phy->supported,
			     __ETHTOOL_LINK_MODE_MASK_NBITS, config.advertising,
			     ERR_PTR(ret));
		return ret;
	}

	phy->phylink = pl;
	phy->phy_link_change = phylink_phy_change;

	irq_str = phy_attached_info_irq(phy);
	phylink_info(pl,
		     "PHY [%s] driver [%s] (irq=%s)\n",
		     dev_name(&phy->mdio.dev), phy->drv->name, irq_str);
	kfree(irq_str);

	mutex_lock(&phy->lock);
	mutex_lock(&pl->state_mutex);
	pl->phydev = phy;
	pl->phy_state.interface = interface;
	pl->phy_state.pause = MLO_PAUSE_NONE;
	pl->phy_state.speed = SPEED_UNKNOWN;
	pl->phy_state.duplex = DUPLEX_UNKNOWN;
	pl->phy_state.rate_matching = RATE_MATCH_NONE;
	linkmode_copy(pl->supported, supported);
	linkmode_copy(pl->link_config.advertising, config.advertising);

	 
	linkmode_copy(phy->advertising, config.advertising);
	mutex_unlock(&pl->state_mutex);
	mutex_unlock(&phy->lock);

	phylink_dbg(pl,
		    "phy: %s setting supported %*pb advertising %*pb\n",
		    phy_modes(interface),
		    __ETHTOOL_LINK_MODE_MASK_NBITS, pl->supported,
		    __ETHTOOL_LINK_MODE_MASK_NBITS, phy->advertising);

	if (phy_interrupt_is_valid(phy))
		phy_request_interrupt(phy);

	if (pl->config->mac_managed_pm)
		phy->mac_managed_pm = true;

	return 0;
}

static int phylink_attach_phy(struct phylink *pl, struct phy_device *phy,
			      phy_interface_t interface)
{
	if (WARN_ON(pl->cfg_link_an_mode == MLO_AN_FIXED ||
		    (pl->cfg_link_an_mode == MLO_AN_INBAND &&
		     phy_interface_mode_is_8023z(interface) && !pl->sfp_bus)))
		return -EINVAL;

	if (pl->phydev)
		return -EBUSY;

	return phy_attach_direct(pl->netdev, phy, 0, interface);
}

 
int phylink_connect_phy(struct phylink *pl, struct phy_device *phy)
{
	int ret;

	 
	if (pl->link_interface == PHY_INTERFACE_MODE_NA) {
		pl->link_interface = phy->interface;
		pl->link_config.interface = pl->link_interface;
	}

	ret = phylink_attach_phy(pl, phy, pl->link_interface);
	if (ret < 0)
		return ret;

	ret = phylink_bringup_phy(pl, phy, pl->link_config.interface);
	if (ret)
		phy_detach(phy);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_connect_phy);

 
int phylink_of_phy_connect(struct phylink *pl, struct device_node *dn,
			   u32 flags)
{
	return phylink_fwnode_phy_connect(pl, of_fwnode_handle(dn), flags);
}
EXPORT_SYMBOL_GPL(phylink_of_phy_connect);

 
int phylink_fwnode_phy_connect(struct phylink *pl,
			       const struct fwnode_handle *fwnode,
			       u32 flags)
{
	struct fwnode_handle *phy_fwnode;
	struct phy_device *phy_dev;
	int ret;

	 
	if (pl->cfg_link_an_mode == MLO_AN_FIXED ||
	    (pl->cfg_link_an_mode == MLO_AN_INBAND &&
	     phy_interface_mode_is_8023z(pl->link_interface)))
		return 0;

	phy_fwnode = fwnode_get_phy_node(fwnode);
	if (IS_ERR(phy_fwnode)) {
		if (pl->cfg_link_an_mode == MLO_AN_PHY)
			return -ENODEV;
		return 0;
	}

	phy_dev = fwnode_phy_find_device(phy_fwnode);
	 
	fwnode_handle_put(phy_fwnode);
	if (!phy_dev)
		return -ENODEV;

	 
	if (pl->link_interface == PHY_INTERFACE_MODE_NA) {
		pl->link_interface = phy_dev->interface;
		pl->link_config.interface = pl->link_interface;
	}

	ret = phy_attach_direct(pl->netdev, phy_dev, flags,
				pl->link_interface);
	phy_device_free(phy_dev);
	if (ret)
		return ret;

	ret = phylink_bringup_phy(pl, phy_dev, pl->link_config.interface);
	if (ret)
		phy_detach(phy_dev);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_fwnode_phy_connect);

 
void phylink_disconnect_phy(struct phylink *pl)
{
	struct phy_device *phy;

	ASSERT_RTNL();

	phy = pl->phydev;
	if (phy) {
		mutex_lock(&phy->lock);
		mutex_lock(&pl->state_mutex);
		pl->phydev = NULL;
		mutex_unlock(&pl->state_mutex);
		mutex_unlock(&phy->lock);
		flush_work(&pl->resolve);

		phy_disconnect(phy);
	}
}
EXPORT_SYMBOL_GPL(phylink_disconnect_phy);

static void phylink_link_changed(struct phylink *pl, bool up, const char *what)
{
	if (!up)
		pl->mac_link_dropped = true;
	phylink_run_resolve(pl);
	phylink_dbg(pl, "%s link %s\n", what, up ? "up" : "down");
}

 
void phylink_mac_change(struct phylink *pl, bool up)
{
	phylink_link_changed(pl, up, "mac");
}
EXPORT_SYMBOL_GPL(phylink_mac_change);

 
void phylink_pcs_change(struct phylink_pcs *pcs, bool up)
{
	struct phylink *pl = pcs->phylink;

	if (pl)
		phylink_link_changed(pl, up, "pcs");
}
EXPORT_SYMBOL_GPL(phylink_pcs_change);

static irqreturn_t phylink_link_handler(int irq, void *data)
{
	struct phylink *pl = data;

	phylink_run_resolve(pl);

	return IRQ_HANDLED;
}

 
void phylink_start(struct phylink *pl)
{
	bool poll = false;

	ASSERT_RTNL();

	phylink_info(pl, "configuring for %s/%s link mode\n",
		     phylink_an_mode_str(pl->cur_link_an_mode),
		     phy_modes(pl->link_config.interface));

	 
	if (pl->netdev)
		netif_carrier_off(pl->netdev);

	pl->pcs_state = PCS_STATE_STARTING;

	 
	phylink_mac_initial_config(pl, true);

	pl->pcs_state = PCS_STATE_STARTED;

	phylink_enable_and_run_resolve(pl, PHYLINK_DISABLE_STOPPED);

	if (pl->cfg_link_an_mode == MLO_AN_FIXED && pl->link_gpio) {
		int irq = gpiod_to_irq(pl->link_gpio);

		if (irq > 0) {
			if (!request_irq(irq, phylink_link_handler,
					 IRQF_TRIGGER_RISING |
					 IRQF_TRIGGER_FALLING,
					 "netdev link", pl))
				pl->link_irq = irq;
			else
				irq = 0;
		}
		if (irq <= 0)
			poll = true;
	}

	if (pl->cfg_link_an_mode == MLO_AN_FIXED)
		poll |= pl->config->poll_fixed_state;

	if (poll)
		mod_timer(&pl->link_poll, jiffies + HZ);
	if (pl->phydev)
		phy_start(pl->phydev);
	if (pl->sfp_bus)
		sfp_upstream_start(pl->sfp_bus);
}
EXPORT_SYMBOL_GPL(phylink_start);

 
void phylink_stop(struct phylink *pl)
{
	ASSERT_RTNL();

	if (pl->sfp_bus)
		sfp_upstream_stop(pl->sfp_bus);
	if (pl->phydev)
		phy_stop(pl->phydev);
	del_timer_sync(&pl->link_poll);
	if (pl->link_irq) {
		free_irq(pl->link_irq, pl);
		pl->link_irq = 0;
	}

	phylink_run_resolve_and_disable(pl, PHYLINK_DISABLE_STOPPED);

	pl->pcs_state = PCS_STATE_DOWN;

	phylink_pcs_disable(pl->pcs);
}
EXPORT_SYMBOL_GPL(phylink_stop);

 
void phylink_suspend(struct phylink *pl, bool mac_wol)
{
	ASSERT_RTNL();

	if (mac_wol && (!pl->netdev || pl->netdev->wol_enabled)) {
		 
		mutex_lock(&pl->state_mutex);

		 
		__set_bit(PHYLINK_DISABLE_MAC_WOL, &pl->phylink_disable_state);

		 
		if (pl->netdev)
			netif_carrier_off(pl->netdev);
		else
			pl->old_link_state = false;

		 
		mutex_unlock(&pl->state_mutex);
	} else {
		phylink_stop(pl);
	}
}
EXPORT_SYMBOL_GPL(phylink_suspend);

 
void phylink_resume(struct phylink *pl)
{
	ASSERT_RTNL();

	if (test_bit(PHYLINK_DISABLE_MAC_WOL, &pl->phylink_disable_state)) {
		 

		 
		mutex_lock(&pl->state_mutex);
		phylink_link_down(pl);
		mutex_unlock(&pl->state_mutex);

		 
		phylink_mac_initial_config(pl, true);

		 
		phylink_enable_and_run_resolve(pl, PHYLINK_DISABLE_MAC_WOL);
	} else {
		phylink_start(pl);
	}
}
EXPORT_SYMBOL_GPL(phylink_resume);

 
void phylink_ethtool_get_wol(struct phylink *pl, struct ethtool_wolinfo *wol)
{
	ASSERT_RTNL();

	wol->supported = 0;
	wol->wolopts = 0;

	if (pl->phydev)
		phy_ethtool_get_wol(pl->phydev, wol);
}
EXPORT_SYMBOL_GPL(phylink_ethtool_get_wol);

 
int phylink_ethtool_set_wol(struct phylink *pl, struct ethtool_wolinfo *wol)
{
	int ret = -EOPNOTSUPP;

	ASSERT_RTNL();

	if (pl->phydev)
		ret = phy_ethtool_set_wol(pl->phydev, wol);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_set_wol);

static void phylink_merge_link_mode(unsigned long *dst, const unsigned long *b)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(mask);

	linkmode_zero(mask);
	phylink_set_port_modes(mask);

	linkmode_and(dst, dst, mask);
	linkmode_or(dst, dst, b);
}

static void phylink_get_ksettings(const struct phylink_link_state *state,
				  struct ethtool_link_ksettings *kset)
{
	phylink_merge_link_mode(kset->link_modes.advertising, state->advertising);
	linkmode_copy(kset->link_modes.lp_advertising, state->lp_advertising);
	if (kset->base.rate_matching == RATE_MATCH_NONE) {
		kset->base.speed = state->speed;
		kset->base.duplex = state->duplex;
	}
	kset->base.autoneg = linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
					       state->advertising) ?
				AUTONEG_ENABLE : AUTONEG_DISABLE;
}

 
int phylink_ethtool_ksettings_get(struct phylink *pl,
				  struct ethtool_link_ksettings *kset)
{
	struct phylink_link_state link_state;

	ASSERT_RTNL();

	if (pl->phydev)
		phy_ethtool_ksettings_get(pl->phydev, kset);
	else
		kset->base.port = pl->link_port;

	linkmode_copy(kset->link_modes.supported, pl->supported);

	switch (pl->cur_link_an_mode) {
	case MLO_AN_FIXED:
		 
		phylink_get_fixed_state(pl, &link_state);
		phylink_get_ksettings(&link_state, kset);
		break;

	case MLO_AN_INBAND:
		 
		if (pl->phydev)
			break;

		phylink_mac_pcs_get_state(pl, &link_state);

		 
		phylink_get_ksettings(&link_state, kset);
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_ksettings_get);

 
int phylink_ethtool_ksettings_set(struct phylink *pl,
				  const struct ethtool_link_ksettings *kset)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(support);
	struct phylink_link_state config;
	const struct phy_setting *s;

	ASSERT_RTNL();

	if (pl->phydev) {
		struct ethtool_link_ksettings phy_kset = *kset;

		linkmode_and(phy_kset.link_modes.advertising,
			     phy_kset.link_modes.advertising,
			     pl->supported);

		 
		return phy_ethtool_ksettings_set(pl->phydev, &phy_kset);
	}

	config = pl->link_config;
	 
	linkmode_and(config.advertising, kset->link_modes.advertising,
		     pl->supported);

	 
	switch (kset->base.autoneg) {
	case AUTONEG_DISABLE:
		 
		s = phy_lookup_setting(kset->base.speed, kset->base.duplex,
				       pl->supported, false);
		if (!s)
			return -EINVAL;

		 
		if (pl->cur_link_an_mode == MLO_AN_FIXED) {
			if (s->speed != pl->link_config.speed ||
			    s->duplex != pl->link_config.duplex)
				return -EINVAL;
			return 0;
		}

		config.speed = s->speed;
		config.duplex = s->duplex;
		break;

	case AUTONEG_ENABLE:
		 
		if (pl->cur_link_an_mode == MLO_AN_FIXED) {
			if (!linkmode_equal(config.advertising,
					    pl->link_config.advertising))
				return -EINVAL;
			return 0;
		}

		config.speed = SPEED_UNKNOWN;
		config.duplex = DUPLEX_UNKNOWN;
		break;

	default:
		return -EINVAL;
	}

	 
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, config.advertising,
			 kset->base.autoneg == AUTONEG_ENABLE);

	 
	if (pl->sfp_bus) {
		config.interface = sfp_select_interface(pl->sfp_bus,
							config.advertising);
		if (config.interface == PHY_INTERFACE_MODE_NA) {
			phylink_err(pl,
				    "selection of interface failed, advertisement %*pb\n",
				    __ETHTOOL_LINK_MODE_MASK_NBITS,
				    config.advertising);
			return -EINVAL;
		}

		 
		linkmode_copy(support, pl->supported);
		if (phylink_validate(pl, support, &config)) {
			phylink_err(pl, "validation of %s/%s with support %*pb failed\n",
				    phylink_an_mode_str(pl->cur_link_an_mode),
				    phy_modes(config.interface),
				    __ETHTOOL_LINK_MODE_MASK_NBITS, support);
			return -EINVAL;
		}
	} else {
		 
		linkmode_copy(support, pl->supported);
		if (phylink_validate(pl, support, &config))
			return -EINVAL;
	}

	 
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
			      config.advertising) &&
	    phylink_is_empty_linkmode(config.advertising))
		return -EINVAL;

	mutex_lock(&pl->state_mutex);
	pl->link_config.speed = config.speed;
	pl->link_config.duplex = config.duplex;

	if (pl->link_config.interface != config.interface) {
		 
		 
		if (pl->old_link_state) {
			phylink_link_down(pl);
			pl->old_link_state = false;
		}
		if (!test_bit(PHYLINK_DISABLE_STOPPED,
			      &pl->phylink_disable_state))
			phylink_major_config(pl, false, &config);
		pl->link_config.interface = config.interface;
		linkmode_copy(pl->link_config.advertising, config.advertising);
	} else if (!linkmode_equal(pl->link_config.advertising,
				   config.advertising)) {
		linkmode_copy(pl->link_config.advertising, config.advertising);
		phylink_change_inband_advert(pl);
	}
	mutex_unlock(&pl->state_mutex);

	return 0;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_ksettings_set);

 
int phylink_ethtool_nway_reset(struct phylink *pl)
{
	int ret = 0;

	ASSERT_RTNL();

	if (pl->phydev)
		ret = phy_restart_aneg(pl->phydev);
	phylink_pcs_an_restart(pl);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_nway_reset);

 
void phylink_ethtool_get_pauseparam(struct phylink *pl,
				    struct ethtool_pauseparam *pause)
{
	ASSERT_RTNL();

	pause->autoneg = !!(pl->link_config.pause & MLO_PAUSE_AN);
	pause->rx_pause = !!(pl->link_config.pause & MLO_PAUSE_RX);
	pause->tx_pause = !!(pl->link_config.pause & MLO_PAUSE_TX);
}
EXPORT_SYMBOL_GPL(phylink_ethtool_get_pauseparam);

 
int phylink_ethtool_set_pauseparam(struct phylink *pl,
				   struct ethtool_pauseparam *pause)
{
	struct phylink_link_state *config = &pl->link_config;
	bool manual_changed;
	int pause_state;

	ASSERT_RTNL();

	if (pl->cur_link_an_mode == MLO_AN_FIXED)
		return -EOPNOTSUPP;

	if (!phylink_test(pl->supported, Pause) &&
	    !phylink_test(pl->supported, Asym_Pause))
		return -EOPNOTSUPP;

	if (!phylink_test(pl->supported, Asym_Pause) &&
	    pause->rx_pause != pause->tx_pause)
		return -EINVAL;

	pause_state = 0;
	if (pause->autoneg)
		pause_state |= MLO_PAUSE_AN;
	if (pause->rx_pause)
		pause_state |= MLO_PAUSE_RX;
	if (pause->tx_pause)
		pause_state |= MLO_PAUSE_TX;

	mutex_lock(&pl->state_mutex);
	 
	linkmode_set_pause(config->advertising, pause->tx_pause,
			   pause->rx_pause);

	manual_changed = (config->pause ^ pause_state) & MLO_PAUSE_AN ||
			 (!(pause_state & MLO_PAUSE_AN) &&
			   (config->pause ^ pause_state) & MLO_PAUSE_TXRX_MASK);

	config->pause = pause_state;

	 
	if (!pl->phydev)
		phylink_change_inband_advert(pl);

	mutex_unlock(&pl->state_mutex);

	 
	if (pl->phydev)
		phy_set_asym_pause(pl->phydev, pause->rx_pause,
				   pause->tx_pause);

	 
	if (manual_changed) {
		pl->mac_link_dropped = true;
		phylink_run_resolve(pl);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_set_pauseparam);

 
int phylink_get_eee_err(struct phylink *pl)
{
	int ret = 0;

	ASSERT_RTNL();

	if (pl->phydev)
		ret = phy_get_eee_err(pl->phydev);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_get_eee_err);

 
int phylink_init_eee(struct phylink *pl, bool clk_stop_enable)
{
	int ret = -EOPNOTSUPP;

	if (pl->phydev)
		ret = phy_init_eee(pl->phydev, clk_stop_enable);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_init_eee);

 
int phylink_ethtool_get_eee(struct phylink *pl, struct ethtool_eee *eee)
{
	int ret = -EOPNOTSUPP;

	ASSERT_RTNL();

	if (pl->phydev)
		ret = phy_ethtool_get_eee(pl->phydev, eee);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_get_eee);

 
int phylink_ethtool_set_eee(struct phylink *pl, struct ethtool_eee *eee)
{
	int ret = -EOPNOTSUPP;

	ASSERT_RTNL();

	if (pl->phydev)
		ret = phy_ethtool_set_eee(pl->phydev, eee);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_ethtool_set_eee);

 
static int phylink_mii_emul_read(unsigned int reg,
				 struct phylink_link_state *state)
{
	struct fixed_phy_status fs;
	unsigned long *lpa = state->lp_advertising;
	int val;

	fs.link = state->link;
	fs.speed = state->speed;
	fs.duplex = state->duplex;
	fs.pause = test_bit(ETHTOOL_LINK_MODE_Pause_BIT, lpa);
	fs.asym_pause = test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, lpa);

	val = swphy_read_reg(reg, &fs);
	if (reg == MII_BMSR) {
		if (!state->an_complete)
			val &= ~BMSR_ANEGCOMPLETE;
	}
	return val;
}

static int phylink_phy_read(struct phylink *pl, unsigned int phy_id,
			    unsigned int reg)
{
	struct phy_device *phydev = pl->phydev;
	int prtad, devad;

	if (mdio_phy_id_is_c45(phy_id)) {
		prtad = mdio_phy_id_prtad(phy_id);
		devad = mdio_phy_id_devad(phy_id);
		return mdiobus_c45_read(pl->phydev->mdio.bus, prtad, devad,
					reg);
	}

	if (phydev->is_c45) {
		switch (reg) {
		case MII_BMCR:
		case MII_BMSR:
		case MII_PHYSID1:
		case MII_PHYSID2:
			devad = __ffs(phydev->c45_ids.mmds_present);
			break;
		case MII_ADVERTISE:
		case MII_LPA:
			if (!(phydev->c45_ids.mmds_present & MDIO_DEVS_AN))
				return -EINVAL;
			devad = MDIO_MMD_AN;
			if (reg == MII_ADVERTISE)
				reg = MDIO_AN_ADVERTISE;
			else
				reg = MDIO_AN_LPA;
			break;
		default:
			return -EINVAL;
		}
		prtad = phy_id;
		return mdiobus_c45_read(pl->phydev->mdio.bus, prtad, devad,
					reg);
	}

	return mdiobus_read(pl->phydev->mdio.bus, phy_id, reg);
}

static int phylink_phy_write(struct phylink *pl, unsigned int phy_id,
			     unsigned int reg, unsigned int val)
{
	struct phy_device *phydev = pl->phydev;
	int prtad, devad;

	if (mdio_phy_id_is_c45(phy_id)) {
		prtad = mdio_phy_id_prtad(phy_id);
		devad = mdio_phy_id_devad(phy_id);
		return mdiobus_c45_write(pl->phydev->mdio.bus, prtad, devad,
					 reg, val);
	}

	if (phydev->is_c45) {
		switch (reg) {
		case MII_BMCR:
		case MII_BMSR:
		case MII_PHYSID1:
		case MII_PHYSID2:
			devad = __ffs(phydev->c45_ids.mmds_present);
			break;
		case MII_ADVERTISE:
		case MII_LPA:
			if (!(phydev->c45_ids.mmds_present & MDIO_DEVS_AN))
				return -EINVAL;
			devad = MDIO_MMD_AN;
			if (reg == MII_ADVERTISE)
				reg = MDIO_AN_ADVERTISE;
			else
				reg = MDIO_AN_LPA;
			break;
		default:
			return -EINVAL;
		}
		return mdiobus_c45_write(pl->phydev->mdio.bus, phy_id, devad,
					 reg, val);
	}

	return mdiobus_write(phydev->mdio.bus, phy_id, reg, val);
}

static int phylink_mii_read(struct phylink *pl, unsigned int phy_id,
			    unsigned int reg)
{
	struct phylink_link_state state;
	int val = 0xffff;

	switch (pl->cur_link_an_mode) {
	case MLO_AN_FIXED:
		if (phy_id == 0) {
			phylink_get_fixed_state(pl, &state);
			val = phylink_mii_emul_read(reg, &state);
		}
		break;

	case MLO_AN_PHY:
		return -EOPNOTSUPP;

	case MLO_AN_INBAND:
		if (phy_id == 0) {
			phylink_mac_pcs_get_state(pl, &state);
			val = phylink_mii_emul_read(reg, &state);
		}
		break;
	}

	return val & 0xffff;
}

static int phylink_mii_write(struct phylink *pl, unsigned int phy_id,
			     unsigned int reg, unsigned int val)
{
	switch (pl->cur_link_an_mode) {
	case MLO_AN_FIXED:
		break;

	case MLO_AN_PHY:
		return -EOPNOTSUPP;

	case MLO_AN_INBAND:
		break;
	}

	return 0;
}

 
int phylink_mii_ioctl(struct phylink *pl, struct ifreq *ifr, int cmd)
{
	struct mii_ioctl_data *mii = if_mii(ifr);
	int  ret;

	ASSERT_RTNL();

	if (pl->phydev) {
		 
		switch (cmd) {
		case SIOCGMIIPHY:
			mii->phy_id = pl->phydev->mdio.addr;
			fallthrough;

		case SIOCGMIIREG:
			ret = phylink_phy_read(pl, mii->phy_id, mii->reg_num);
			if (ret >= 0) {
				mii->val_out = ret;
				ret = 0;
			}
			break;

		case SIOCSMIIREG:
			ret = phylink_phy_write(pl, mii->phy_id, mii->reg_num,
						mii->val_in);
			break;

		default:
			ret = phy_mii_ioctl(pl->phydev, ifr, cmd);
			break;
		}
	} else {
		switch (cmd) {
		case SIOCGMIIPHY:
			mii->phy_id = 0;
			fallthrough;

		case SIOCGMIIREG:
			ret = phylink_mii_read(pl, mii->phy_id, mii->reg_num);
			if (ret >= 0) {
				mii->val_out = ret;
				ret = 0;
			}
			break;

		case SIOCSMIIREG:
			ret = phylink_mii_write(pl, mii->phy_id, mii->reg_num,
						mii->val_in);
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_mii_ioctl);

 
int phylink_speed_down(struct phylink *pl, bool sync)
{
	int ret = 0;

	ASSERT_RTNL();

	if (!pl->sfp_bus && pl->phydev)
		ret = phy_speed_down(pl->phydev, sync);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_speed_down);

 
int phylink_speed_up(struct phylink *pl)
{
	int ret = 0;

	ASSERT_RTNL();

	if (!pl->sfp_bus && pl->phydev)
		ret = phy_speed_up(pl->phydev);

	return ret;
}
EXPORT_SYMBOL_GPL(phylink_speed_up);

static void phylink_sfp_attach(void *upstream, struct sfp_bus *bus)
{
	struct phylink *pl = upstream;

	pl->netdev->sfp_bus = bus;
}

static void phylink_sfp_detach(void *upstream, struct sfp_bus *bus)
{
	struct phylink *pl = upstream;

	pl->netdev->sfp_bus = NULL;
}

static const phy_interface_t phylink_sfp_interface_preference[] = {
	PHY_INTERFACE_MODE_25GBASER,
	PHY_INTERFACE_MODE_USXGMII,
	PHY_INTERFACE_MODE_10GBASER,
	PHY_INTERFACE_MODE_5GBASER,
	PHY_INTERFACE_MODE_2500BASEX,
	PHY_INTERFACE_MODE_SGMII,
	PHY_INTERFACE_MODE_1000BASEX,
	PHY_INTERFACE_MODE_100BASEX,
};

static DECLARE_PHY_INTERFACE_MASK(phylink_sfp_interfaces);

static phy_interface_t phylink_choose_sfp_interface(struct phylink *pl,
						    const unsigned long *intf)
{
	phy_interface_t interface;
	size_t i;

	interface = PHY_INTERFACE_MODE_NA;
	for (i = 0; i < ARRAY_SIZE(phylink_sfp_interface_preference); i++)
		if (test_bit(phylink_sfp_interface_preference[i], intf)) {
			interface = phylink_sfp_interface_preference[i];
			break;
		}

	return interface;
}

static void phylink_sfp_set_config(struct phylink *pl, u8 mode,
				   unsigned long *supported,
				   struct phylink_link_state *state)
{
	bool changed = false;

	phylink_dbg(pl, "requesting link mode %s/%s with support %*pb\n",
		    phylink_an_mode_str(mode), phy_modes(state->interface),
		    __ETHTOOL_LINK_MODE_MASK_NBITS, supported);

	if (!linkmode_equal(pl->supported, supported)) {
		linkmode_copy(pl->supported, supported);
		changed = true;
	}

	if (!linkmode_equal(pl->link_config.advertising, state->advertising)) {
		linkmode_copy(pl->link_config.advertising, state->advertising);
		changed = true;
	}

	if (pl->cur_link_an_mode != mode ||
	    pl->link_config.interface != state->interface) {
		pl->cur_link_an_mode = mode;
		pl->link_config.interface = state->interface;

		changed = true;

		phylink_info(pl, "switched to %s/%s link mode\n",
			     phylink_an_mode_str(mode),
			     phy_modes(state->interface));
	}

	if (changed && !test_bit(PHYLINK_DISABLE_STOPPED,
				 &pl->phylink_disable_state))
		phylink_mac_initial_config(pl, false);
}

static int phylink_sfp_config_phy(struct phylink *pl, u8 mode,
				  struct phy_device *phy)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(support1);
	__ETHTOOL_DECLARE_LINK_MODE_MASK(support);
	struct phylink_link_state config;
	phy_interface_t iface;
	int ret;

	linkmode_copy(support, phy->supported);

	memset(&config, 0, sizeof(config));
	linkmode_copy(config.advertising, phy->advertising);
	config.interface = PHY_INTERFACE_MODE_NA;
	config.speed = SPEED_UNKNOWN;
	config.duplex = DUPLEX_UNKNOWN;
	config.pause = MLO_PAUSE_AN;

	 
	ret = phylink_validate(pl, support, &config);
	if (ret) {
		phylink_err(pl, "validation with support %*pb failed: %pe\n",
			    __ETHTOOL_LINK_MODE_MASK_NBITS, support,
			    ERR_PTR(ret));
		return ret;
	}

	iface = sfp_select_interface(pl->sfp_bus, config.advertising);
	if (iface == PHY_INTERFACE_MODE_NA) {
		phylink_err(pl,
			    "selection of interface failed, advertisement %*pb\n",
			    __ETHTOOL_LINK_MODE_MASK_NBITS, config.advertising);
		return -EINVAL;
	}

	config.interface = iface;
	linkmode_copy(support1, support);
	ret = phylink_validate(pl, support1, &config);
	if (ret) {
		phylink_err(pl,
			    "validation of %s/%s with support %*pb failed: %pe\n",
			    phylink_an_mode_str(mode),
			    phy_modes(config.interface),
			    __ETHTOOL_LINK_MODE_MASK_NBITS, support,
			    ERR_PTR(ret));
		return ret;
	}

	pl->link_port = pl->sfp_port;

	phylink_sfp_set_config(pl, mode, support, &config);

	return 0;
}

static int phylink_sfp_config_optical(struct phylink *pl)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(support);
	DECLARE_PHY_INTERFACE_MASK(interfaces);
	struct phylink_link_state config;
	phy_interface_t interface;
	int ret;

	phylink_dbg(pl, "optical SFP: interfaces=[mac=%*pbl, sfp=%*pbl]\n",
		    (int)PHY_INTERFACE_MODE_MAX,
		    pl->config->supported_interfaces,
		    (int)PHY_INTERFACE_MODE_MAX,
		    pl->sfp_interfaces);

	 
	phy_interface_and(interfaces, pl->config->supported_interfaces,
			  pl->sfp_interfaces);
	if (phy_interface_empty(interfaces)) {
		phylink_err(pl, "unsupported SFP module: no common interface modes\n");
		return -EINVAL;
	}

	memset(&config, 0, sizeof(config));
	linkmode_copy(support, pl->sfp_support);
	linkmode_copy(config.advertising, pl->sfp_support);
	config.speed = SPEED_UNKNOWN;
	config.duplex = DUPLEX_UNKNOWN;
	config.pause = MLO_PAUSE_AN;

	 
	ret = phylink_validate_mask(pl, pl->sfp_support, &config, interfaces);
	if (ret) {
		phylink_err(pl, "unsupported SFP module: validation with support %*pb failed\n",
			    __ETHTOOL_LINK_MODE_MASK_NBITS, support);
		return ret;
	}

	interface = phylink_choose_sfp_interface(pl, interfaces);
	if (interface == PHY_INTERFACE_MODE_NA) {
		phylink_err(pl, "failed to select SFP interface\n");
		return -EINVAL;
	}

	phylink_dbg(pl, "optical SFP: chosen %s interface\n",
		    phy_modes(interface));

	config.interface = interface;

	 
	ret = phylink_validate(pl, support, &config);
	if (ret) {
		phylink_err(pl, "validation with support %*pb failed: %pe\n",
			    __ETHTOOL_LINK_MODE_MASK_NBITS, support,
			    ERR_PTR(ret));
		return ret;
	}

	pl->link_port = pl->sfp_port;

	phylink_sfp_set_config(pl, MLO_AN_INBAND, pl->sfp_support, &config);

	return 0;
}

static int phylink_sfp_module_insert(void *upstream,
				     const struct sfp_eeprom_id *id)
{
	struct phylink *pl = upstream;

	ASSERT_RTNL();

	linkmode_zero(pl->sfp_support);
	phy_interface_zero(pl->sfp_interfaces);
	sfp_parse_support(pl->sfp_bus, id, pl->sfp_support, pl->sfp_interfaces);
	pl->sfp_port = sfp_parse_port(pl->sfp_bus, id, pl->sfp_support);

	 
	pl->sfp_may_have_phy = sfp_may_have_phy(pl->sfp_bus, id);
	if (pl->sfp_may_have_phy)
		return 0;

	return phylink_sfp_config_optical(pl);
}

static int phylink_sfp_module_start(void *upstream)
{
	struct phylink *pl = upstream;

	 
	if (pl->phydev) {
		phy_start(pl->phydev);
		return 0;
	}

	 
	if (!pl->sfp_may_have_phy)
		return 0;

	return phylink_sfp_config_optical(pl);
}

static void phylink_sfp_module_stop(void *upstream)
{
	struct phylink *pl = upstream;

	 
	if (pl->phydev)
		phy_stop(pl->phydev);
}

static void phylink_sfp_link_down(void *upstream)
{
	struct phylink *pl = upstream;

	ASSERT_RTNL();

	phylink_run_resolve_and_disable(pl, PHYLINK_DISABLE_LINK);
}

static void phylink_sfp_link_up(void *upstream)
{
	struct phylink *pl = upstream;

	ASSERT_RTNL();

	phylink_enable_and_run_resolve(pl, PHYLINK_DISABLE_LINK);
}

 
static bool phylink_phy_no_inband(struct phy_device *phy)
{
	return phy->is_c45 && phy_id_compare(phy->c45_ids.device_ids[1],
					     0xae025150, 0xfffffff0);
}

static int phylink_sfp_connect_phy(void *upstream, struct phy_device *phy)
{
	struct phylink *pl = upstream;
	phy_interface_t interface;
	u8 mode;
	int ret;

	 
	phy_support_asym_pause(phy);

	if (phylink_phy_no_inband(phy))
		mode = MLO_AN_PHY;
	else
		mode = MLO_AN_INBAND;

	 
	phy_interface_and(phy->host_interfaces, phylink_sfp_interfaces,
			  pl->config->supported_interfaces);

	 
	ret = phylink_sfp_config_phy(pl, mode, phy);
	if (ret < 0)
		return ret;

	interface = pl->link_config.interface;
	ret = phylink_attach_phy(pl, phy, interface);
	if (ret < 0)
		return ret;

	ret = phylink_bringup_phy(pl, phy, interface);
	if (ret)
		phy_detach(phy);

	return ret;
}

static void phylink_sfp_disconnect_phy(void *upstream)
{
	phylink_disconnect_phy(upstream);
}

static const struct sfp_upstream_ops sfp_phylink_ops = {
	.attach = phylink_sfp_attach,
	.detach = phylink_sfp_detach,
	.module_insert = phylink_sfp_module_insert,
	.module_start = phylink_sfp_module_start,
	.module_stop = phylink_sfp_module_stop,
	.link_up = phylink_sfp_link_up,
	.link_down = phylink_sfp_link_down,
	.connect_phy = phylink_sfp_connect_phy,
	.disconnect_phy = phylink_sfp_disconnect_phy,
};

 

static struct {
	int bit;
	int speed;
} phylink_c73_priority_resolution[] = {
	{ ETHTOOL_LINK_MODE_100000baseCR4_Full_BIT, SPEED_100000 },
	{ ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT, SPEED_100000 },
	 
	{ ETHTOOL_LINK_MODE_40000baseCR4_Full_BIT, SPEED_40000 },
	{ ETHTOOL_LINK_MODE_40000baseKR4_Full_BIT, SPEED_40000 },
	{ ETHTOOL_LINK_MODE_10000baseKR_Full_BIT, SPEED_10000 },
	{ ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT, SPEED_10000 },
	 
	{ ETHTOOL_LINK_MODE_2500baseX_Full_BIT, SPEED_2500 },
	{ ETHTOOL_LINK_MODE_1000baseKX_Full_BIT, SPEED_1000 },
};

void phylink_resolve_c73(struct phylink_link_state *state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(phylink_c73_priority_resolution); i++) {
		int bit = phylink_c73_priority_resolution[i].bit;
		if (linkmode_test_bit(bit, state->advertising) &&
		    linkmode_test_bit(bit, state->lp_advertising))
			break;
	}

	if (i < ARRAY_SIZE(phylink_c73_priority_resolution)) {
		state->speed = phylink_c73_priority_resolution[i].speed;
		state->duplex = DUPLEX_FULL;
	} else {
		 
		state->link = false;
	}

	phylink_resolve_an_pause(state);
}
EXPORT_SYMBOL_GPL(phylink_resolve_c73);

static void phylink_decode_c37_word(struct phylink_link_state *state,
				    uint16_t config_reg, int speed)
{
	int fd_bit;

	if (speed == SPEED_2500)
		fd_bit = ETHTOOL_LINK_MODE_2500baseX_Full_BIT;
	else
		fd_bit = ETHTOOL_LINK_MODE_1000baseX_Full_BIT;

	mii_lpa_mod_linkmode_x(state->lp_advertising, config_reg, fd_bit);

	if (linkmode_test_bit(fd_bit, state->advertising) &&
	    linkmode_test_bit(fd_bit, state->lp_advertising)) {
		state->speed = speed;
		state->duplex = DUPLEX_FULL;
	} else {
		 
		state->link = false;
	}

	phylink_resolve_an_pause(state);
}

static void phylink_decode_sgmii_word(struct phylink_link_state *state,
				      uint16_t config_reg)
{
	if (!(config_reg & LPA_SGMII_LINK)) {
		state->link = false;
		return;
	}

	switch (config_reg & LPA_SGMII_SPD_MASK) {
	case LPA_SGMII_10:
		state->speed = SPEED_10;
		break;
	case LPA_SGMII_100:
		state->speed = SPEED_100;
		break;
	case LPA_SGMII_1000:
		state->speed = SPEED_1000;
		break;
	default:
		state->link = false;
		return;
	}
	if (config_reg & LPA_SGMII_FULL_DUPLEX)
		state->duplex = DUPLEX_FULL;
	else
		state->duplex = DUPLEX_HALF;
}

 
void phylink_decode_usxgmii_word(struct phylink_link_state *state,
				 uint16_t lpa)
{
	switch (lpa & MDIO_USXGMII_SPD_MASK) {
	case MDIO_USXGMII_10:
		state->speed = SPEED_10;
		break;
	case MDIO_USXGMII_100:
		state->speed = SPEED_100;
		break;
	case MDIO_USXGMII_1000:
		state->speed = SPEED_1000;
		break;
	case MDIO_USXGMII_2500:
		state->speed = SPEED_2500;
		break;
	case MDIO_USXGMII_5000:
		state->speed = SPEED_5000;
		break;
	case MDIO_USXGMII_10G:
		state->speed = SPEED_10000;
		break;
	default:
		state->link = false;
		return;
	}

	if (lpa & MDIO_USXGMII_FULL_DUPLEX)
		state->duplex = DUPLEX_FULL;
	else
		state->duplex = DUPLEX_HALF;
}
EXPORT_SYMBOL_GPL(phylink_decode_usxgmii_word);

 
static void phylink_decode_usgmii_word(struct phylink_link_state *state,
				       uint16_t lpa)
{
	switch (lpa & MDIO_USXGMII_SPD_MASK) {
	case MDIO_USXGMII_10:
		state->speed = SPEED_10;
		break;
	case MDIO_USXGMII_100:
		state->speed = SPEED_100;
		break;
	case MDIO_USXGMII_1000:
		state->speed = SPEED_1000;
		break;
	default:
		state->link = false;
		return;
	}

	if (lpa & MDIO_USXGMII_FULL_DUPLEX)
		state->duplex = DUPLEX_FULL;
	else
		state->duplex = DUPLEX_HALF;
}

 
void phylink_mii_c22_pcs_decode_state(struct phylink_link_state *state,
				      u16 bmsr, u16 lpa)
{
	state->link = !!(bmsr & BMSR_LSTATUS);
	state->an_complete = !!(bmsr & BMSR_ANEGCOMPLETE);
	 
	if (!state->link || !linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
					       state->advertising))
		return;

	switch (state->interface) {
	case PHY_INTERFACE_MODE_1000BASEX:
		phylink_decode_c37_word(state, lpa, SPEED_1000);
		break;

	case PHY_INTERFACE_MODE_2500BASEX:
		phylink_decode_c37_word(state, lpa, SPEED_2500);
		break;

	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_QSGMII:
		phylink_decode_sgmii_word(state, lpa);
		break;
	case PHY_INTERFACE_MODE_QUSGMII:
		phylink_decode_usgmii_word(state, lpa);
		break;

	default:
		state->link = false;
		break;
	}
}
EXPORT_SYMBOL_GPL(phylink_mii_c22_pcs_decode_state);

 
void phylink_mii_c22_pcs_get_state(struct mdio_device *pcs,
				   struct phylink_link_state *state)
{
	int bmsr, lpa;

	bmsr = mdiodev_read(pcs, MII_BMSR);
	lpa = mdiodev_read(pcs, MII_LPA);
	if (bmsr < 0 || lpa < 0) {
		state->link = false;
		return;
	}

	phylink_mii_c22_pcs_decode_state(state, bmsr, lpa);
}
EXPORT_SYMBOL_GPL(phylink_mii_c22_pcs_get_state);

 
int phylink_mii_c22_pcs_encode_advertisement(phy_interface_t interface,
					     const unsigned long *advertising)
{
	u16 adv;

	switch (interface) {
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
		adv = ADVERTISE_1000XFULL;
		if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
				      advertising))
			adv |= ADVERTISE_1000XPAUSE;
		if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
				      advertising))
			adv |= ADVERTISE_1000XPSE_ASYM;
		return adv;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_QSGMII:
		return 0x0001;
	default:
		 
		return -EINVAL;
	}
}
EXPORT_SYMBOL_GPL(phylink_mii_c22_pcs_encode_advertisement);

 
int phylink_mii_c22_pcs_config(struct mdio_device *pcs,
			       phy_interface_t interface,
			       const unsigned long *advertising,
			       unsigned int neg_mode)
{
	bool changed = 0;
	u16 bmcr;
	int ret, adv;

	adv = phylink_mii_c22_pcs_encode_advertisement(interface, advertising);
	if (adv >= 0) {
		ret = mdiobus_modify_changed(pcs->bus, pcs->addr,
					     MII_ADVERTISE, 0xffff, adv);
		if (ret < 0)
			return ret;
		changed = ret;
	}

	if (neg_mode == PHYLINK_PCS_NEG_INBAND_ENABLED)
		bmcr = BMCR_ANENABLE;
	else
		bmcr = 0;

	 
	ret = mdiodev_modify(pcs, MII_BMCR, BMCR_ANENABLE | BMCR_ISOLATE, bmcr);
	if (ret < 0)
		return ret;

	return changed;
}
EXPORT_SYMBOL_GPL(phylink_mii_c22_pcs_config);

 
void phylink_mii_c22_pcs_an_restart(struct mdio_device *pcs)
{
	int val = mdiodev_read(pcs, MII_BMCR);

	if (val >= 0) {
		val |= BMCR_ANRESTART;

		mdiodev_write(pcs, MII_BMCR, val);
	}
}
EXPORT_SYMBOL_GPL(phylink_mii_c22_pcs_an_restart);

void phylink_mii_c45_pcs_get_state(struct mdio_device *pcs,
				   struct phylink_link_state *state)
{
	struct mii_bus *bus = pcs->bus;
	int addr = pcs->addr;
	int stat;

	stat = mdiobus_c45_read(bus, addr, MDIO_MMD_PCS, MDIO_STAT1);
	if (stat < 0) {
		state->link = false;
		return;
	}

	state->link = !!(stat & MDIO_STAT1_LSTATUS);
	if (!state->link)
		return;

	switch (state->interface) {
	case PHY_INTERFACE_MODE_10GBASER:
		state->speed = SPEED_10000;
		state->duplex = DUPLEX_FULL;
		break;

	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(phylink_mii_c45_pcs_get_state);

static int __init phylink_init(void)
{
	for (int i = 0; i < ARRAY_SIZE(phylink_sfp_interface_preference); ++i)
		__set_bit(phylink_sfp_interface_preference[i],
			  phylink_sfp_interfaces);

	return 0;
}

module_init(phylink_init);

MODULE_LICENSE("GPL v2");


 

#include "mac.h"
#include "device.h"
#include "power.h"
#include "card.h"

 

 

 

 

 

 

void PSvEnablePowerSaving(struct vnt_private *priv,
			  unsigned short wListenInterval)
{
	u16 wAID = priv->current_aid | BIT(14) | BIT(15);

	 
	iowrite16(C_PWBT, priv->port_offset + MAC_REG_PWBT);
	if (priv->op_mode != NL80211_IFTYPE_ADHOC) {
		 
		iowrite16(wAID, priv->port_offset + MAC_REG_AIDATIM);
	}

	 
	vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);

	 
	vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_TFTCTL, TFTCTL_HWUTSF);

	if (wListenInterval >= 2) {
		 
		vt6655_mac_reg_bits_off(priv->port_offset, MAC_REG_PSCTL, PSCTL_ALBCN);
		 
		vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_PSCTL, PSCTL_LNBCN);
	} else {
		 
		vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_PSCTL, PSCTL_ALBCN);
	}

	 
	vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_PSCTL, PSCTL_PSEN);
	priv->bEnablePSMode = true;

	priv->bPWBitOn = true;
	pr_debug("PS:Power Saving Mode Enable...\n");
}

 

void PSvDisablePowerSaving(struct vnt_private *priv)
{
	 
	MACbPSWakeup(priv);

	 
	vt6655_mac_reg_bits_off(priv->port_offset, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);

	 
	vt6655_mac_reg_bits_off(priv->port_offset, MAC_REG_TFTCTL, TFTCTL_HWUTSF);

	 
	vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_PSCTL, PSCTL_ALBCN);

	priv->bEnablePSMode = false;

	priv->bPWBitOn = false;
}

 

bool PSbIsNextTBTTWakeUp(struct vnt_private *priv)
{
	struct ieee80211_hw *hw = priv->hw;
	struct ieee80211_conf *conf = &hw->conf;
	bool wake_up = false;

	if (conf->listen_interval > 1) {
		if (!priv->wake_up_count)
			priv->wake_up_count = conf->listen_interval;

		--priv->wake_up_count;

		if (priv->wake_up_count == 1) {
			 
			vt6655_mac_reg_bits_on(priv->port_offset, MAC_REG_PSCTL, PSCTL_LNBCN);
			wake_up = true;
		}
	}

	return wake_up;
}

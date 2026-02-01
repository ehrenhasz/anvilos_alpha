
 

#include "mac.h"
#include "device.h"
#include "power.h"
#include "wcmd.h"
#include "rxtx.h"
#include "card.h"
#include "usbpipe.h"

 

void vnt_enable_power_saving(struct vnt_private *priv, u16 listen_interval)
{
	u16 aid = priv->current_aid | BIT(14) | BIT(15);

	 
	vnt_mac_write_word(priv, MAC_REG_PWBT, C_PWBT);

	if (priv->op_mode != NL80211_IFTYPE_ADHOC)
		 
		vnt_mac_write_word(priv, MAC_REG_AIDATIM, aid);

	 
	 
	vnt_mac_reg_bits_on(priv, MAC_REG_PSCTL, PSCTL_PSEN);

	 
	vnt_mac_reg_bits_on(priv, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);

	 
	vnt_mac_reg_bits_on(priv, MAC_REG_PSCTL, PSCTL_GO2DOZE);

	 
	vnt_mac_reg_bits_on(priv, MAC_REG_PSCTL, PSCTL_ALBCN);

	dev_dbg(&priv->usb->dev,  "PS:Power Saving Mode Enable...\n");
}

int vnt_disable_power_saving(struct vnt_private *priv)
{
	int ret;

	 
	ret = vnt_control_out(priv, MESSAGE_TYPE_DISABLE_PS, 0,
			      0, 0, NULL);
	if (ret)
		return ret;

	 
	vnt_mac_reg_bits_off(priv, MAC_REG_PSCFG, PSCFG_AUTOSLEEP);

	 
	vnt_mac_reg_bits_on(priv, MAC_REG_PSCTL, PSCTL_ALBCN);

	return 0;
}

 

int vnt_next_tbtt_wakeup(struct vnt_private *priv)
{
	struct ieee80211_hw *hw = priv->hw;
	struct ieee80211_conf *conf = &hw->conf;
	int wake_up = false;

	if (conf->listen_interval > 1) {
		 
		vnt_mac_reg_bits_on(priv, MAC_REG_PSCTL, PSCTL_LNBCN);
		wake_up = true;
	}

	return wake_up;
}

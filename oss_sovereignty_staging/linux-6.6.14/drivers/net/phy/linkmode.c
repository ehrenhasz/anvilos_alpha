
#include <linux/linkmode.h>

 
void linkmode_resolve_pause(const unsigned long *local_adv,
			    const unsigned long *partner_adv,
			    bool *tx_pause, bool *rx_pause)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(m);

	linkmode_and(m, local_adv, partner_adv);
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT, m)) {
		*tx_pause = true;
		*rx_pause = true;
	} else if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, m)) {
		*tx_pause = linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
					      partner_adv);
		*rx_pause = linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
					      local_adv);
	} else {
		*tx_pause = false;
		*rx_pause = false;
	}
}
EXPORT_SYMBOL_GPL(linkmode_resolve_pause);

 
void linkmode_set_pause(unsigned long *advertisement, bool tx, bool rx)
{
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Pause_BIT, advertisement, rx);
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, advertisement,
			 rx ^ tx);
}
EXPORT_SYMBOL_GPL(linkmode_set_pause);

 

#include "ath5k.h"


static inline void ath5k_rfkill_disable(struct ath5k_hw *ah)
{
	ATH5K_DBG(ah, ATH5K_DEBUG_ANY, "rfkill disable (gpio:%d polarity:%d)\n",
		ah->rf_kill.gpio, ah->rf_kill.polarity);
	ath5k_hw_set_gpio_output(ah, ah->rf_kill.gpio);
	ath5k_hw_set_gpio(ah, ah->rf_kill.gpio, !ah->rf_kill.polarity);
}


static inline void ath5k_rfkill_enable(struct ath5k_hw *ah)
{
	ATH5K_DBG(ah, ATH5K_DEBUG_ANY, "rfkill enable (gpio:%d polarity:%d)\n",
		ah->rf_kill.gpio, ah->rf_kill.polarity);
	ath5k_hw_set_gpio_output(ah, ah->rf_kill.gpio);
	ath5k_hw_set_gpio(ah, ah->rf_kill.gpio, ah->rf_kill.polarity);
}

static inline void ath5k_rfkill_set_intr(struct ath5k_hw *ah, bool enable)
{
	u32 curval;

	ath5k_hw_set_gpio_input(ah, ah->rf_kill.gpio);
	curval = ath5k_hw_get_gpio(ah, ah->rf_kill.gpio);
	ath5k_hw_set_gpio_intr(ah, ah->rf_kill.gpio, enable ?
					!!curval : !curval);
}

static bool
ath5k_is_rfkill_set(struct ath5k_hw *ah)
{
	 
	 
	return ath5k_hw_get_gpio(ah, ah->rf_kill.gpio) ==
							ah->rf_kill.polarity;
}

static void
ath5k_tasklet_rfkill_toggle(struct tasklet_struct *t)
{
	struct ath5k_hw *ah = from_tasklet(ah, t, rf_kill.toggleq);
	bool blocked;

	blocked = ath5k_is_rfkill_set(ah);
	wiphy_rfkill_set_hw_state(ah->hw->wiphy, blocked);
}


void
ath5k_rfkill_hw_start(struct ath5k_hw *ah)
{
	 
	ah->rf_kill.gpio = ah->ah_capabilities.cap_eeprom.ee_rfkill_pin;
	ah->rf_kill.polarity = ah->ah_capabilities.cap_eeprom.ee_rfkill_pol;

	tasklet_setup(&ah->rf_kill.toggleq, ath5k_tasklet_rfkill_toggle);

	ath5k_rfkill_disable(ah);

	 
	if (AR5K_EEPROM_HDR_RFKILL(ah->ah_capabilities.cap_eeprom.ee_header))
		ath5k_rfkill_set_intr(ah, true);
}


void
ath5k_rfkill_hw_stop(struct ath5k_hw *ah)
{
	 
	if (AR5K_EEPROM_HDR_RFKILL(ah->ah_capabilities.cap_eeprom.ee_header))
		ath5k_rfkill_set_intr(ah, false);

	tasklet_kill(&ah->rf_kill.toggleq);

	 
	ath5k_rfkill_enable(ah);
}


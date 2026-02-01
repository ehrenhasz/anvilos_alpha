 
 
#ifndef __iwl_modparams_h__
#define __iwl_modparams_h__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>

extern struct iwl_mod_params iwlwifi_mod_params;

enum iwl_power_level {
	IWL_POWER_INDEX_1,
	IWL_POWER_INDEX_2,
	IWL_POWER_INDEX_3,
	IWL_POWER_INDEX_4,
	IWL_POWER_INDEX_5,
	IWL_POWER_NUM
};

enum iwl_disable_11n {
	IWL_DISABLE_HT_ALL	 = BIT(0),
	IWL_DISABLE_HT_TXAGG	 = BIT(1),
	IWL_DISABLE_HT_RXAGG	 = BIT(2),
	IWL_ENABLE_HT_TXAGG	 = BIT(3),
};

enum iwl_amsdu_size {
	IWL_AMSDU_DEF = 0,
	IWL_AMSDU_4K = 1,
	IWL_AMSDU_8K = 2,
	IWL_AMSDU_12K = 3,
	 
	IWL_AMSDU_2K = 4,
};

enum iwl_uapsd_disable {
	IWL_DISABLE_UAPSD_BSS		= BIT(0),
	IWL_DISABLE_UAPSD_P2P_CLIENT	= BIT(1),
};

 
struct iwl_mod_params {
	int swcrypto;
	unsigned int disable_11n;
	int amsdu_size;
	bool fw_restart;
	bool bt_coex_active;
	int led_mode;
	bool power_save;
	int power_level;
#ifdef CONFIG_IWLWIFI_DEBUG
	u32 debug_level;
#endif
	char *nvm_file;
	u32 uapsd_disable;
	bool disable_11ac;
	 
	bool disable_11ax;
	bool remove_when_gone;
	u32 enable_ini;
	bool disable_11be;
};

static inline bool iwl_enable_rx_ampdu(void)
{
	if (iwlwifi_mod_params.disable_11n & IWL_DISABLE_HT_RXAGG)
		return false;
	return true;
}

static inline bool iwl_enable_tx_ampdu(void)
{
	if (iwlwifi_mod_params.disable_11n & IWL_DISABLE_HT_TXAGG)
		return false;
	if (iwlwifi_mod_params.disable_11n & IWL_ENABLE_HT_TXAGG)
		return true;

	 
	return true;
}

#endif  

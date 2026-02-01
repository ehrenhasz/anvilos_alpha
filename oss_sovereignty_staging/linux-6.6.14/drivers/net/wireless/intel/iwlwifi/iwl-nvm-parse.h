 
 
#ifndef __iwl_nvm_parse_h__
#define __iwl_nvm_parse_h__

#include <net/cfg80211.h>
#include "iwl-eeprom-parse.h"
#include "mei/iwl-mei.h"

 
enum iwl_nvm_sbands_flags {
	IWL_NVM_SBANDS_FLAGS_LAR		= BIT(0),
	IWL_NVM_SBANDS_FLAGS_NO_WIDE_IN_5GHZ	= BIT(1),
};

 
struct iwl_nvm_data *
iwl_parse_nvm_data(struct iwl_trans *trans, const struct iwl_cfg *cfg,
		   const struct iwl_fw *fw,
		   const __be16 *nvm_hw, const __le16 *nvm_sw,
		   const __le16 *nvm_calib, const __le16 *regulatory,
		   const __le16 *mac_override, const __le16 *phy_sku,
		   u8 tx_chains, u8 rx_chains);

 
struct ieee80211_regdomain *
iwl_parse_nvm_mcc_info(struct device *dev, const struct iwl_cfg *cfg,
		       int num_of_ch, __le32 *channels, u16 fw_mcc,
		       u16 geo_info, u32 cap, u8 resp_ver);

 
struct iwl_nvm_section {
	u16 length;
	const u8 *data;
};

 
int iwl_read_external_nvm(struct iwl_trans *trans,
			  const char *nvm_file_name,
			  struct iwl_nvm_section *nvm_sections);
void iwl_nvm_fixups(u32 hw_id, unsigned int section, u8 *data,
		    unsigned int len);

 
struct iwl_nvm_data *iwl_get_nvm(struct iwl_trans *trans,
				 const struct iwl_fw *fw);

 
struct iwl_nvm_data *
iwl_parse_mei_nvm_data(struct iwl_trans *trans, const struct iwl_cfg *cfg,
		       const struct iwl_mei_nvm *mei_nvm,
		       const struct iwl_fw *fw);

#endif  

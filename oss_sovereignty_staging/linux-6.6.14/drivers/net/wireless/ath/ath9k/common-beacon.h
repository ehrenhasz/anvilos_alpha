 

struct ath_beacon_config;

int ath9k_cmn_beacon_config_sta(struct ath_hw *ah,
				struct ath_beacon_config *conf,
				struct ath9k_beacon_state *bs);
void ath9k_cmn_beacon_config_adhoc(struct ath_hw *ah,
				   struct ath_beacon_config *conf);
void ath9k_cmn_beacon_config_ap(struct ath_hw *ah,
				struct ath_beacon_config *conf,
				unsigned int bc_buf);

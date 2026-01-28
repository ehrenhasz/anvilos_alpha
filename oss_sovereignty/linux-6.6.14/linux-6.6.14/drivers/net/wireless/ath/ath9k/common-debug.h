struct ath_rx_stats {
	u32 rx_pkts_all;
	u32 rx_bytes_all;
	u32 crc_err;
	u32 decrypt_crc_err;
	u32 phy_err;
	u32 mic_err;
	u32 pre_delim_crc_err;
	u32 post_delim_crc_err;
	u32 decrypt_busy_err;
	u32 phy_err_stats[ATH9K_PHYERR_MAX];
	u32 rx_len_err;
	u32 rx_oom_err;
	u32 rx_rate_err;
	u32 rx_too_many_frags_err;
	u32 rx_beacons;
	u32 rx_frags;
	u32 rx_spectral;
	u32 rx_spectral_sample_good;
	u32 rx_spectral_sample_err;
};
#ifdef CONFIG_ATH9K_COMMON_DEBUG
void ath9k_cmn_debug_modal_eeprom(struct dentry *debugfs_phy,
				  struct ath_hw *ah);
void ath9k_cmn_debug_base_eeprom(struct dentry *debugfs_phy,
				 struct ath_hw *ah);
void ath9k_cmn_debug_stat_rx(struct ath_rx_stats *rxstats,
			     struct ath_rx_status *rs);
void ath9k_cmn_debug_recv(struct dentry *debugfs_phy,
			  struct ath_rx_stats *rxstats);
void ath9k_cmn_debug_phy_err(struct dentry *debugfs_phy,
			     struct ath_rx_stats *rxstats);
#else
static inline void ath9k_cmn_debug_modal_eeprom(struct dentry *debugfs_phy,
						struct ath_hw *ah)
{
}
static inline void ath9k_cmn_debug_base_eeprom(struct dentry *debugfs_phy,
					       struct ath_hw *ah)
{
}
static inline void ath9k_cmn_debug_stat_rx(struct ath_rx_stats *rxstats,
					   struct ath_rx_status *rs)
{
}
static inline void ath9k_cmn_debug_recv(struct dentry *debugfs_phy,
					struct ath_rx_stats *rxstats)
{
}
static inline void ath9k_cmn_debug_phy_err(struct dentry *debugfs_phy,
					   struct ath_rx_stats *rxstats)
{
}
#endif  

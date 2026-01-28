#ifndef SPECTRAL_H
#define SPECTRAL_H
#include "../spectral_common.h"
enum spectral_mode {
	SPECTRAL_DISABLED = 0,
	SPECTRAL_BACKGROUND,
	SPECTRAL_MANUAL,
	SPECTRAL_CHANSCAN,
};
#define SPECTRAL_SCAN_BITMASK		0x10
struct ath_radar_info {
	u8 pulse_length_pri;
	u8 pulse_length_ext;
	u8 pulse_bw_info;
} __packed;
struct ath_ht20_mag_info {
	u8 all_bins[3];
	u8 max_exp;
} __packed;
struct ath_ht20_fft_packet {
	u8 data[SPECTRAL_HT20_NUM_BINS];
	struct ath_ht20_mag_info mag_info;
	struct ath_radar_info radar_info;
} __packed;
#define SPECTRAL_HT20_TOTAL_DATA_LEN	(sizeof(struct ath_ht20_fft_packet))
#define	SPECTRAL_HT20_SAMPLE_LEN	(sizeof(struct ath_ht20_mag_info) +\
					SPECTRAL_HT20_NUM_BINS)
struct ath_ht20_40_mag_info {
	u8 lower_bins[3];
	u8 upper_bins[3];
	u8 max_exp;
} __packed;
struct ath_ht20_40_fft_packet {
	u8 data[SPECTRAL_HT20_40_NUM_BINS];
	struct ath_ht20_40_mag_info mag_info;
	struct ath_radar_info radar_info;
} __packed;
struct ath_spec_scan_priv {
	struct ath_hw *ah;
	struct rchan *rfs_chan_spec_scan;
	enum spectral_mode spectral_mode;
	struct ath_spec_scan spec_config;
};
#define SPECTRAL_HT20_40_TOTAL_DATA_LEN	(sizeof(struct ath_ht20_40_fft_packet))
#define	SPECTRAL_HT20_40_SAMPLE_LEN	(sizeof(struct ath_ht20_40_mag_info) +\
					SPECTRAL_HT20_40_NUM_BINS)
#define	SPECTRAL_SAMPLE_MAX_LEN		SPECTRAL_HT20_40_SAMPLE_LEN
static inline u16 spectral_max_magnitude(u8 *bins)
{
	return (bins[0] & 0xc0) >> 6 |
	       (bins[1] & 0xff) << 2 |
	       (bins[2] & 0x03) << 10;
}
static inline u8 spectral_max_index(u8 *bins, int num_bins)
{
	s8 m = (bins[2] & 0xfc) >> 2;
	u8 zero_idx = num_bins / 2;
	if (m & 0x20) {
		m &= ~0x20;
		m |= 0xe0;
	}
	m += zero_idx;
	if (m < 0 || m > num_bins - 1)
		m = 0;
	return m;
}
static inline u8 spectral_max_index_ht40(u8 *bins)
{
	u8 idx;
	idx = spectral_max_index(bins, SPECTRAL_HT20_40_NUM_BINS);
	return idx % (SPECTRAL_HT20_40_NUM_BINS / 2);
}
static inline u8 spectral_max_index_ht20(u8 *bins)
{
	return spectral_max_index(bins, SPECTRAL_HT20_NUM_BINS);
}
static inline u8 spectral_bitmap_weight(u8 *bins)
{
	return bins[0] & 0x3f;
}
#ifdef CONFIG_ATH9K_COMMON_SPECTRAL
void ath9k_cmn_spectral_init_debug(struct ath_spec_scan_priv *spec_priv, struct dentry *debugfs_phy);
void ath9k_cmn_spectral_deinit_debug(struct ath_spec_scan_priv *spec_priv);
void ath9k_cmn_spectral_scan_trigger(struct ath_common *common,
				 struct ath_spec_scan_priv *spec_priv);
int ath9k_cmn_spectral_scan_config(struct ath_common *common,
			       struct ath_spec_scan_priv *spec_priv,
			       enum spectral_mode spectral_mode);
int ath_cmn_process_fft(struct ath_spec_scan_priv *spec_priv, struct ieee80211_hdr *hdr,
		    struct ath_rx_status *rs, u64 tsf);
#else
static inline void ath9k_cmn_spectral_init_debug(struct ath_spec_scan_priv *spec_priv,
						 struct dentry *debugfs_phy)
{
}
static inline void ath9k_cmn_spectral_deinit_debug(struct ath_spec_scan_priv *spec_priv)
{
}
static inline void ath9k_cmn_spectral_scan_trigger(struct ath_common *common,
						   struct ath_spec_scan_priv *spec_priv)
{
}
static inline int ath_cmn_process_fft(struct ath_spec_scan_priv *spec_priv,
				      struct ieee80211_hdr *hdr,
				      struct ath_rx_status *rs, u64 tsf)
{
	return 0;
}
#endif  
#endif  

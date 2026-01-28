#ifndef SPECTRAL_COMMON_H
#define SPECTRAL_COMMON_H
#define SPECTRAL_HT20_NUM_BINS		56
#define SPECTRAL_HT20_40_NUM_BINS		128
#define SPECTRAL_ATH10K_MAX_NUM_BINS		256
enum ath_fft_sample_type {
	ATH_FFT_SAMPLE_HT20 = 1,
	ATH_FFT_SAMPLE_HT20_40,
	ATH_FFT_SAMPLE_ATH10K,
	ATH_FFT_SAMPLE_ATH11K
};
struct fft_sample_tlv {
	u8 type;	 
	__be16 length;
} __packed;
struct fft_sample_ht20 {
	struct fft_sample_tlv tlv;
	u8 max_exp;
	__be16 freq;
	s8 rssi;
	s8 noise;
	__be16 max_magnitude;
	u8 max_index;
	u8 bitmap_weight;
	__be64 tsf;
	u8 data[SPECTRAL_HT20_NUM_BINS];
} __packed;
struct fft_sample_ht20_40 {
	struct fft_sample_tlv tlv;
	u8 channel_type;
	__be16 freq;
	s8 lower_rssi;
	s8 upper_rssi;
	__be64 tsf;
	s8 lower_noise;
	s8 upper_noise;
	__be16 lower_max_magnitude;
	__be16 upper_max_magnitude;
	u8 lower_max_index;
	u8 upper_max_index;
	u8 lower_bitmap_weight;
	u8 upper_bitmap_weight;
	u8 max_exp;
	u8 data[SPECTRAL_HT20_40_NUM_BINS];
} __packed;
struct fft_sample_ath10k {
	struct fft_sample_tlv tlv;
	u8 chan_width_mhz;
	__be16 freq1;
	__be16 freq2;
	__be16 noise;
	__be16 max_magnitude;
	__be16 total_gain_db;
	__be16 base_pwr_db;
	__be64 tsf;
	s8 max_index;
	u8 rssi;
	u8 relpwr_db;
	u8 avgpwr_db;
	u8 max_exp;
	u8 data[];
} __packed;
struct fft_sample_ath11k {
	struct fft_sample_tlv tlv;
	u8 chan_width_mhz;
	s8 max_index;
	u8 max_exp;
	__be16 freq1;
	__be16 freq2;
	__be16 max_magnitude;
	__be16 rssi;
	__be32 tsf;
	__be32 noise;
	u8 data[];
} __packed;
#endif  

#ifndef SPECTRAL_H
#define SPECTRAL_H
#include "../spectral_common.h"
struct ath10k_spec_scan {
	u8 count;
	u8 fft_size;
};
enum ath10k_spectral_mode {
	SPECTRAL_DISABLED = 0,
	SPECTRAL_BACKGROUND,
	SPECTRAL_MANUAL,
};
#ifdef CONFIG_ATH10K_SPECTRAL
int ath10k_spectral_process_fft(struct ath10k *ar,
				struct wmi_phyerr_ev_arg *phyerr,
				const struct phyerr_fft_report *fftr,
				size_t bin_len, u64 tsf);
int ath10k_spectral_start(struct ath10k *ar);
int ath10k_spectral_vif_stop(struct ath10k_vif *arvif);
int ath10k_spectral_create(struct ath10k *ar);
void ath10k_spectral_destroy(struct ath10k *ar);
#else
static inline int
ath10k_spectral_process_fft(struct ath10k *ar,
			    struct wmi_phyerr_ev_arg *phyerr,
			    const struct phyerr_fft_report *fftr,
			    size_t bin_len, u64 tsf)
{
	return 0;
}
static inline int ath10k_spectral_start(struct ath10k *ar)
{
	return 0;
}
static inline int ath10k_spectral_vif_stop(struct ath10k_vif *arvif)
{
	return 0;
}
static inline int ath10k_spectral_create(struct ath10k *ar)
{
	return 0;
}
static inline void ath10k_spectral_destroy(struct ath10k *ar)
{
}
#endif  
#endif  

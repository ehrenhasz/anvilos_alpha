 
 

#ifndef ATH11K_SPECTRAL_H
#define ATH11K_SPECTRAL_H

#include "../spectral_common.h"
#include "dbring.h"

 
enum ath11k_spectral_mode {
	ATH11K_SPECTRAL_DISABLED = 0,
	ATH11K_SPECTRAL_BACKGROUND,
	ATH11K_SPECTRAL_MANUAL,
};

struct ath11k_spectral {
	struct ath11k_dbring rx_ring;
	 
	spinlock_t lock;
	struct rchan *rfs_scan;	 
	struct dentry *scan_ctl;
	struct dentry *scan_count;
	struct dentry *scan_bins;
	enum ath11k_spectral_mode mode;
	u16 count;
	u8 fft_size;
	bool enabled;
	bool is_primary;
};

#ifdef CONFIG_ATH11K_SPECTRAL

int ath11k_spectral_init(struct ath11k_base *ab);
void ath11k_spectral_deinit(struct ath11k_base *ab);
int ath11k_spectral_vif_stop(struct ath11k_vif *arvif);
void ath11k_spectral_reset_buffer(struct ath11k *ar);
enum ath11k_spectral_mode ath11k_spectral_get_mode(struct ath11k *ar);
struct ath11k_dbring *ath11k_spectral_get_dbring(struct ath11k *ar);

#else

static inline int ath11k_spectral_init(struct ath11k_base *ab)
{
	return 0;
}

static inline void ath11k_spectral_deinit(struct ath11k_base *ab)
{
}

static inline int ath11k_spectral_vif_stop(struct ath11k_vif *arvif)
{
	return 0;
}

static inline void ath11k_spectral_reset_buffer(struct ath11k *ar)
{
}

static inline
enum ath11k_spectral_mode ath11k_spectral_get_mode(struct ath11k *ar)
{
	return ATH11K_SPECTRAL_DISABLED;
}

static inline
struct ath11k_dbring *ath11k_spectral_get_dbring(struct ath11k *ar)
{
	return NULL;
}

#endif  
#endif  

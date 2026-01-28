#ifndef SIENA_SRIOV_H
#define SIENA_SRIOV_H
#include "net_driver.h"
#define EFX_VI_SCALE_MAX 6
#define EFX_VI_BASE 128U
#define EFX_VF_COUNT_MAX 127
#define EFX_MAX_VF_EVQ_SIZE 8192UL
#define EFX_VF_BUFTBL_PER_VI					\
	((EFX_MAX_VF_EVQ_SIZE + 2 * EFX_MAX_DMAQ_SIZE) *	\
	 sizeof(efx_qword_t) / EFX_BUF_SIZE)
int efx_siena_sriov_configure(struct efx_nic *efx, int num_vfs);
int efx_siena_sriov_init(struct efx_nic *efx);
void efx_siena_sriov_fini(struct efx_nic *efx);
int efx_siena_sriov_mac_address_changed(struct efx_nic *efx);
bool efx_siena_sriov_wanted(struct efx_nic *efx);
void efx_siena_sriov_reset(struct efx_nic *efx);
void efx_siena_sriov_flr(struct efx_nic *efx, unsigned flr);
int efx_siena_sriov_set_vf_mac(struct efx_nic *efx, int vf, const u8 *mac);
int efx_siena_sriov_set_vf_vlan(struct efx_nic *efx, int vf,
				u16 vlan, u8 qos);
int efx_siena_sriov_set_vf_spoofchk(struct efx_nic *efx, int vf,
				    bool spoofchk);
int efx_siena_sriov_get_vf_config(struct efx_nic *efx, int vf,
				  struct ifla_vf_info *ivf);
#ifdef CONFIG_SFC_SIENA_SRIOV
static inline bool efx_siena_sriov_enabled(struct efx_nic *efx)
{
	return efx->vf_init_count != 0;
}
int efx_init_sriov(void);
void efx_fini_sriov(void);
#else  
static inline bool efx_siena_sriov_enabled(struct efx_nic *efx)
{
	return false;
}
#endif  
void efx_siena_sriov_probe(struct efx_nic *efx);
void efx_siena_sriov_tx_flush_done(struct efx_nic *efx, efx_qword_t *event);
void efx_siena_sriov_rx_flush_done(struct efx_nic *efx, efx_qword_t *event);
void efx_siena_sriov_event(struct efx_channel *channel, efx_qword_t *event);
void efx_siena_sriov_desc_fetch_err(struct efx_nic *efx, unsigned dmaq);
#endif  

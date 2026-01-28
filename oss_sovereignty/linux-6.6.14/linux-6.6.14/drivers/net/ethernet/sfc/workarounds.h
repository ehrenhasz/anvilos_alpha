#ifndef EFX_WORKAROUNDS_H
#define EFX_WORKAROUNDS_H
#define EFX_WORKAROUND_EF10(efx) (efx_nic_rev(efx) >= EFX_REV_HUNT_A0)
#define EFX_EF10_WORKAROUND_35388(efx)					\
	(((struct efx_ef10_nic_data *)efx->nic_data)->workaround_35388)
#define EFX_WORKAROUND_35388(efx)					\
	(efx_nic_rev(efx) == EFX_REV_HUNT_A0 && EFX_EF10_WORKAROUND_35388(efx))
#define EFX_EF10_WORKAROUND_61265(efx)					\
	(((struct efx_ef10_nic_data *)efx->nic_data)->workaround_61265)
#endif  

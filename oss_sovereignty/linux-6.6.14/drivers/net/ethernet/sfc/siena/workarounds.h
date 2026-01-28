#ifndef EFX_WORKAROUNDS_H
#define EFX_WORKAROUNDS_H
#define EFX_WORKAROUND_SIENA(efx) (efx_nic_rev(efx) == EFX_REV_SIENA_A0)
#define EFX_WORKAROUND_EF10(efx) (efx_nic_rev(efx) >= EFX_REV_HUNT_A0)
#define EFX_WORKAROUND_10G(efx) 1
#define EFX_WORKAROUND_7884 EFX_WORKAROUND_10G
#define EFX_WORKAROUND_17213 EFX_WORKAROUND_SIENA
#define EFX_EF10_WORKAROUND_61265(efx)					\
	(((struct efx_ef10_nic_data *)efx->nic_data)->workaround_61265)
#endif  

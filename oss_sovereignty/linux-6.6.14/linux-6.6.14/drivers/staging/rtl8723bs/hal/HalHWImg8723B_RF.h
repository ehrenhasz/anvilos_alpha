#ifndef __INC_MP_RF_HW_IMG_8723B_H
#define __INC_MP_RF_HW_IMG_8723B_H
void
ODM_ReadAndConfig_MP_8723B_RadioA( 
	struct dm_odm_t *pDM_Odm
);
void
ODM_ReadAndConfig_MP_8723B_TxPowerTrack_SDIO( 
	struct dm_odm_t *pDM_Odm
);
u32 ODM_GetVersion_MP_8723B_TxPowerTrack_SDIO(void);
void
ODM_ReadAndConfig_MP_8723B_TXPWR_LMT( 
	struct dm_odm_t *pDM_Odm
);
u32 ODM_GetVersion_MP_8723B_TXPWR_LMT(void);
#endif

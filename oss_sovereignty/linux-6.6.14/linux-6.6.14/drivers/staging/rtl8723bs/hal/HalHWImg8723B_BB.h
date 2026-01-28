#ifndef __INC_MP_BB_HW_IMG_8723B_H
#define __INC_MP_BB_HW_IMG_8723B_H
void
ODM_ReadAndConfig_MP_8723B_AGC_TAB( 
	struct dm_odm_t *pDM_Odm
);
void
ODM_ReadAndConfig_MP_8723B_PHY_REG( 
	struct dm_odm_t *pDM_Odm
);
void
ODM_ReadAndConfig_MP_8723B_PHY_REG_PG( 
	struct dm_odm_t *pDM_Odm
);
u32 ODM_GetVersion_MP_8723B_PHY_REG_PG(void);
#endif

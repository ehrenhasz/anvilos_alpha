
 

#include "odm_precomp.h"

void odm_ConfigRFReg_8723B(
	struct dm_odm_t *pDM_Odm,
	u32 Addr,
	u32 Data,
	enum rf_path RF_PATH,
	u32 RegAddr
)
{
	if (Addr == 0xfe || Addr == 0xffe)
		msleep(50);
	else {
		PHY_SetRFReg(pDM_Odm->Adapter, RF_PATH, RegAddr, bRFRegOffsetMask, Data);
		 
		udelay(1);

		 
		if (Addr == 0xb6) {
			u32 getvalue = 0;
			u8 count = 0;

			getvalue = PHY_QueryRFReg(
				pDM_Odm->Adapter, RF_PATH, Addr, bMaskDWord
			);

			udelay(1);

			while ((getvalue>>8) != (Data>>8)) {
				count++;
				PHY_SetRFReg(pDM_Odm->Adapter, RF_PATH, RegAddr, bRFRegOffsetMask, Data);
				udelay(1);
				getvalue = PHY_QueryRFReg(pDM_Odm->Adapter, RF_PATH, Addr, bMaskDWord);
				if (count > 5)
					break;
			}
		}

		if (Addr == 0xb2) {
			u32 getvalue = 0;
			u8 count = 0;

			getvalue = PHY_QueryRFReg(
				pDM_Odm->Adapter, RF_PATH, Addr, bMaskDWord
			);

			udelay(1);

			while (getvalue != Data) {
				count++;
				PHY_SetRFReg(
					pDM_Odm->Adapter,
					RF_PATH,
					RegAddr,
					bRFRegOffsetMask,
					Data
				);
				udelay(1);
				 
				PHY_SetRFReg(
					pDM_Odm->Adapter,
					RF_PATH,
					0x18,
					bRFRegOffsetMask,
					0x0fc07
				);
				udelay(1);
				getvalue = PHY_QueryRFReg(
					pDM_Odm->Adapter, RF_PATH, Addr, bMaskDWord
				);

				if (count > 5)
					break;
			}
		}
	}
}


void odm_ConfigRF_RadioA_8723B(struct dm_odm_t *pDM_Odm, u32 Addr, u32 Data)
{
	u32  content = 0x1000;  
	u32 maskforPhySet = (u32)(content&0xE000);

	odm_ConfigRFReg_8723B(
		pDM_Odm,
		Addr,
		Data,
		RF_PATH_A,
		Addr|maskforPhySet
	);
}

void odm_ConfigMAC_8723B(struct dm_odm_t *pDM_Odm, u32 Addr, u8 Data)
{
	rtw_write8(pDM_Odm->Adapter, Addr, Data);
}

void odm_ConfigBB_AGC_8723B(
	struct dm_odm_t *pDM_Odm,
	u32 Addr,
	u32 Bitmask,
	u32 Data
)
{
	PHY_SetBBReg(pDM_Odm->Adapter, Addr, Bitmask, Data);
	 
	udelay(1);
}

void odm_ConfigBB_PHY_REG_PG_8723B(
	struct dm_odm_t *pDM_Odm,
	u32 RfPath,
	u32 Addr,
	u32 Bitmask,
	u32 Data
)
{
	if (Addr == 0xfe || Addr == 0xffe)
		msleep(50);
	else {
		PHY_StoreTxPowerByRate(pDM_Odm->Adapter, RfPath, Addr, Bitmask, Data);
	}
}

void odm_ConfigBB_PHY_8723B(
	struct dm_odm_t *pDM_Odm,
	u32 Addr,
	u32 Bitmask,
	u32 Data
)
{
	if (Addr == 0xfe)
		msleep(50);
	else if (Addr == 0xfd)
		mdelay(5);
	else if (Addr == 0xfc)
		mdelay(1);
	else if (Addr == 0xfb)
		udelay(50);
	else if (Addr == 0xfa)
		udelay(5);
	else if (Addr == 0xf9)
		udelay(1);
	else {
		PHY_SetBBReg(pDM_Odm->Adapter, Addr, Bitmask, Data);
	}

	 
	udelay(1);
}

void odm_ConfigBB_TXPWR_LMT_8723B(
	struct dm_odm_t *pDM_Odm,
	u8 *Regulation,
	u8 *Bandwidth,
	u8 *RateSection,
	u8 *RfPath,
	u8 *Channel,
	u8 *PowerLimit
)
{
	PHY_SetTxPowerLimit(
		pDM_Odm->Adapter,
		Regulation,
		Bandwidth,
		RateSection,
		RfPath,
		Channel,
		PowerLimit
	);
}

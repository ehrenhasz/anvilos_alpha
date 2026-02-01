
 
 

#include <rtl8723b_hal.h>

 
 


 
 


 
 
 

 
void PHY_RF6052SetBandwidth8723B(
	struct adapter *Adapter, enum channel_width Bandwidth
)  
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	switch (Bandwidth) {
	case CHANNEL_WIDTH_20:
		pHalData->RfRegChnlVal[0] = ((pHalData->RfRegChnlVal[0] & 0xfffff3ff) | BIT10 | BIT11);
		PHY_SetRFReg(Adapter, RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask, pHalData->RfRegChnlVal[0]);
		PHY_SetRFReg(Adapter, RF_PATH_B, RF_CHNLBW, bRFRegOffsetMask, pHalData->RfRegChnlVal[0]);
		break;

	case CHANNEL_WIDTH_40:
		pHalData->RfRegChnlVal[0] = ((pHalData->RfRegChnlVal[0] & 0xfffff3ff) | BIT10);
		PHY_SetRFReg(Adapter, RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask, pHalData->RfRegChnlVal[0]);
		PHY_SetRFReg(Adapter, RF_PATH_B, RF_CHNLBW, bRFRegOffsetMask, pHalData->RfRegChnlVal[0]);
		break;

	default:
		break;
	}

}

static int phy_RF6052_Config_ParaFile(struct adapter *Adapter)
{
	u32 u4RegValue = 0;
	u8 eRFPath;
	struct bb_register_def *pPhyReg;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	 
	 
	 
	 
	for (eRFPath = 0; eRFPath < pHalData->NumTotalRFPath; eRFPath++) {

		pPhyReg = &pHalData->PHYRegDef[eRFPath];

		 
		switch (eRFPath) {
		case RF_PATH_A:
			u4RegValue = PHY_QueryBBReg(Adapter, pPhyReg->rfintfs, bRFSI_RFENV);
			break;
		case RF_PATH_B:
			u4RegValue = PHY_QueryBBReg(Adapter, pPhyReg->rfintfs, bRFSI_RFENV << 16);
			break;
		}

		 
		PHY_SetBBReg(Adapter, pPhyReg->rfintfe, bRFSI_RFENV << 16, 0x1);
		udelay(1); 

		 
		PHY_SetBBReg(Adapter, pPhyReg->rfintfo, bRFSI_RFENV, 0x1);
		udelay(1); 

		 
		PHY_SetBBReg(Adapter, pPhyReg->rfHSSIPara2, b3WireAddressLength, 0x0);	 
		udelay(1); 

		PHY_SetBBReg(Adapter, pPhyReg->rfHSSIPara2, b3WireDataLength, 0x0);	 
		udelay(1); 

		 
		switch (eRFPath) {
		case RF_PATH_A:
		case RF_PATH_B:
			ODM_ConfigRFWithHeaderFile(&pHalData->odmpriv,
						   CONFIG_RF_RADIO, eRFPath);
			break;
		}

		 
		switch (eRFPath) {
		case RF_PATH_A:
			PHY_SetBBReg(Adapter, pPhyReg->rfintfs, bRFSI_RFENV, u4RegValue);
			break;
		case RF_PATH_B:
			PHY_SetBBReg(Adapter, pPhyReg->rfintfs, bRFSI_RFENV << 16, u4RegValue);
			break;
		}
	}

	 
	 
	 

	ODM_ConfigRFWithTxPwrTrackHeaderFile(&pHalData->odmpriv);

	return _SUCCESS;
}


int PHY_RF6052_Config8723B(struct adapter *Adapter)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	 
	 
	 
	pHalData->NumTotalRFPath = 1;

	 
	 
	 
	return phy_RF6052_Config_ParaFile(Adapter);

}

 

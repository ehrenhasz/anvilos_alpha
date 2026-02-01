
 

#include <drv_types.h>
#include <rtw_debug.h>
#include <rtl8723b_hal.h>

 
static	u32 phy_CalculateBitShift(u32 BitMask)
{
	u32 i;

	for (i = 0; i <= 31; i++) {
		if (((BitMask>>i) &  0x1) == 1)
			break;
	}
	return i;
}


 
u32 PHY_QueryBBReg_8723B(struct adapter *Adapter, u32 RegAddr, u32 BitMask)
{
	u32 OriginalValue, BitShift;

	OriginalValue = rtw_read32(Adapter, RegAddr);
	BitShift = phy_CalculateBitShift(BitMask);

	return (OriginalValue & BitMask) >> BitShift;

}


 

void PHY_SetBBReg_8723B(
	struct adapter *Adapter,
	u32 RegAddr,
	u32 BitMask,
	u32 Data
)
{
	 
	u32 OriginalValue, BitShift;

	if (BitMask != bMaskDWord) {  
		OriginalValue = rtw_read32(Adapter, RegAddr);
		BitShift = phy_CalculateBitShift(BitMask);
		Data = ((OriginalValue & (~BitMask)) | ((Data << BitShift) & BitMask));
	}

	rtw_write32(Adapter, RegAddr, Data);

}


 
 
 

static u32 phy_RFSerialRead_8723B(
	struct adapter *Adapter, enum rf_path eRFPath, u32 Offset
)
{
	u32 retValue = 0;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	struct bb_register_def *pPhyReg = &pHalData->PHYRegDef[eRFPath];
	u32 NewOffset;
	u32 tmplong2;
	u8 RfPiEnable = 0;
	u32 MaskforPhySet = 0;
	int i = 0;

	 
	 
	 
	Offset &= 0xff;

	NewOffset = Offset;

	if (eRFPath == RF_PATH_A) {
		tmplong2 = PHY_QueryBBReg(Adapter, rFPGA0_XA_HSSIParameter2|MaskforPhySet, bMaskDWord);
		tmplong2 = (tmplong2 & (~bLSSIReadAddress)) | (NewOffset<<23) | bLSSIReadEdge;	 
		PHY_SetBBReg(Adapter, rFPGA0_XA_HSSIParameter2|MaskforPhySet, bMaskDWord, tmplong2&(~bLSSIReadEdge));
	} else {
		tmplong2 = PHY_QueryBBReg(Adapter, rFPGA0_XB_HSSIParameter2|MaskforPhySet, bMaskDWord);
		tmplong2 = (tmplong2 & (~bLSSIReadAddress)) | (NewOffset<<23) | bLSSIReadEdge;	 
		PHY_SetBBReg(Adapter, rFPGA0_XB_HSSIParameter2|MaskforPhySet, bMaskDWord, tmplong2&(~bLSSIReadEdge));
	}

	tmplong2 = PHY_QueryBBReg(Adapter, rFPGA0_XA_HSSIParameter2|MaskforPhySet, bMaskDWord);
	PHY_SetBBReg(Adapter, rFPGA0_XA_HSSIParameter2|MaskforPhySet, bMaskDWord, tmplong2 & (~bLSSIReadEdge));
	PHY_SetBBReg(Adapter, rFPGA0_XA_HSSIParameter2|MaskforPhySet, bMaskDWord, tmplong2 | bLSSIReadEdge);

	udelay(10);

	for (i = 0; i < 2; i++)
		udelay(MAX_STALL_TIME);
	udelay(10);

	if (eRFPath == RF_PATH_A)
		RfPiEnable = (u8)PHY_QueryBBReg(Adapter, rFPGA0_XA_HSSIParameter1|MaskforPhySet, BIT8);
	else if (eRFPath == RF_PATH_B)
		RfPiEnable = (u8)PHY_QueryBBReg(Adapter, rFPGA0_XB_HSSIParameter1|MaskforPhySet, BIT8);

	if (RfPiEnable) {
		 
		retValue = PHY_QueryBBReg(Adapter, pPhyReg->rfLSSIReadBackPi|MaskforPhySet, bLSSIReadBackData);
	} else {
		 
		retValue = PHY_QueryBBReg(Adapter, pPhyReg->rfLSSIReadBack|MaskforPhySet, bLSSIReadBackData);
	}
	return retValue;

}

 
static void phy_RFSerialWrite_8723B(
	struct adapter *Adapter,
	enum rf_path eRFPath,
	u32 Offset,
	u32 Data
)
{
	u32 DataAndAddr = 0;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	struct bb_register_def *pPhyReg = &pHalData->PHYRegDef[eRFPath];
	u32 NewOffset;

	Offset &= 0xff;

	 
	 
	 
	NewOffset = Offset;

	 
	 
	 
	DataAndAddr = ((NewOffset<<20) | (Data&0x000fffff)) & 0x0fffffff;	 
	 
	 
	 
	PHY_SetBBReg(Adapter, pPhyReg->rf3wireOffset, bMaskDWord, DataAndAddr);
}


 
u32 PHY_QueryRFReg_8723B(
	struct adapter *Adapter,
	u8 eRFPath,
	u32 RegAddr,
	u32 BitMask
)
{
	u32 Original_Value, BitShift;

	Original_Value = phy_RFSerialRead_8723B(Adapter, eRFPath, RegAddr);
	BitShift =  phy_CalculateBitShift(BitMask);

	return (Original_Value & BitMask) >> BitShift;
}

 
void PHY_SetRFReg_8723B(
	struct adapter *Adapter,
	u8 eRFPath,
	u32 RegAddr,
	u32 BitMask,
	u32 Data
)
{
	u32 Original_Value, BitShift;

	 
	if (BitMask != bRFRegOffsetMask) {
		Original_Value = phy_RFSerialRead_8723B(Adapter, eRFPath, RegAddr);
		BitShift =  phy_CalculateBitShift(BitMask);
		Data = ((Original_Value & (~BitMask)) | (Data<<BitShift));
	}

	phy_RFSerialWrite_8723B(Adapter, eRFPath, RegAddr, Data);
}


 
 
 


 
s32 PHY_MACConfig8723B(struct adapter *Adapter)
{
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);

	ODM_ReadAndConfig_MP_8723B_MAC_REG(&pHalData->odmpriv);
	return _SUCCESS;
}

 
static void phy_InitBBRFRegisterDefinition(struct adapter *Adapter)
{
	struct hal_com_data		*pHalData = GET_HAL_DATA(Adapter);

	 
	pHalData->PHYRegDef[RF_PATH_A].rfintfs = rFPGA0_XAB_RFInterfaceSW;  
	pHalData->PHYRegDef[RF_PATH_B].rfintfs = rFPGA0_XAB_RFInterfaceSW;  

	 
	pHalData->PHYRegDef[RF_PATH_A].rfintfo = rFPGA0_XA_RFInterfaceOE;  
	pHalData->PHYRegDef[RF_PATH_B].rfintfo = rFPGA0_XB_RFInterfaceOE;  

	 
	pHalData->PHYRegDef[RF_PATH_A].rfintfe = rFPGA0_XA_RFInterfaceOE;  
	pHalData->PHYRegDef[RF_PATH_B].rfintfe = rFPGA0_XB_RFInterfaceOE;  

	pHalData->PHYRegDef[RF_PATH_A].rf3wireOffset = rFPGA0_XA_LSSIParameter;  
	pHalData->PHYRegDef[RF_PATH_B].rf3wireOffset = rFPGA0_XB_LSSIParameter;

	pHalData->PHYRegDef[RF_PATH_A].rfHSSIPara2 = rFPGA0_XA_HSSIParameter2;   
	pHalData->PHYRegDef[RF_PATH_B].rfHSSIPara2 = rFPGA0_XB_HSSIParameter2;   

	 
	pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBack = rFPGA0_XA_LSSIReadBack;
	pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBack = rFPGA0_XB_LSSIReadBack;
	pHalData->PHYRegDef[RF_PATH_A].rfLSSIReadBackPi = TransceiverA_HSPI_Readback;
	pHalData->PHYRegDef[RF_PATH_B].rfLSSIReadBackPi = TransceiverB_HSPI_Readback;

}

static int phy_BB8723b_Config_ParaFile(struct adapter *Adapter)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	 
	PHY_InitTxPowerLimit(Adapter);
	if (
		Adapter->registrypriv.RegEnableTxPowerLimit == 1 ||
		(Adapter->registrypriv.RegEnableTxPowerLimit == 2 && pHalData->EEPROMRegulatory == 1)
	) {
		ODM_ConfigRFWithHeaderFile(&pHalData->odmpriv,
					   CONFIG_RF_TXPWR_LMT, 0);
	}

	 
	 
	 
	ODM_ConfigBBWithHeaderFile(&pHalData->odmpriv, CONFIG_BB_PHY_REG);

	 
	PHY_InitTxPowerByRate(Adapter);
	if (
		Adapter->registrypriv.RegEnableTxPowerByRate == 1 ||
		(Adapter->registrypriv.RegEnableTxPowerByRate == 2 && pHalData->EEPROMRegulatory != 2)
	) {
		ODM_ConfigBBWithHeaderFile(&pHalData->odmpriv,
					   CONFIG_BB_PHY_REG_PG);

		if (pHalData->odmpriv.PhyRegPgValueType == PHY_REG_PG_EXACT_VALUE)
			PHY_TxPowerByRateConfiguration(Adapter);

		if (
			Adapter->registrypriv.RegEnableTxPowerLimit == 1 ||
			(Adapter->registrypriv.RegEnableTxPowerLimit == 2 && pHalData->EEPROMRegulatory == 1)
		)
			PHY_ConvertTxPowerLimitToPowerIndex(Adapter);
	}

	 
	 
	 
	ODM_ConfigBBWithHeaderFile(&pHalData->odmpriv, CONFIG_BB_AGC_TAB);

	return _SUCCESS;
}


int PHY_BBConfig8723B(struct adapter *Adapter)
{
	int	rtStatus = _SUCCESS;
	struct hal_com_data	*pHalData = GET_HAL_DATA(Adapter);
	u32 RegVal;
	u8 CrystalCap;

	phy_InitBBRFRegisterDefinition(Adapter);

	 
	RegVal = rtw_read16(Adapter, REG_SYS_FUNC_EN);
	rtw_write16(Adapter, REG_SYS_FUNC_EN, (u16)(RegVal|BIT13|BIT0|BIT1));

	rtw_write32(Adapter, 0x948, 0x280);	 

	rtw_write8(Adapter, REG_RF_CTRL, RF_EN|RF_RSTB|RF_SDMRSTB);

	msleep(1);

	PHY_SetRFReg(Adapter, RF_PATH_A, 0x1, 0xfffff, 0x780);

	rtw_write8(Adapter, REG_SYS_FUNC_EN, FEN_PPLL|FEN_PCIEA|FEN_DIO_PCIE|FEN_BB_GLB_RSTn|FEN_BBRSTB);

	rtw_write8(Adapter, REG_AFE_XTAL_CTRL+1, 0x80);

	 
	 
	 
	rtStatus = phy_BB8723b_Config_ParaFile(Adapter);

	 
	CrystalCap = pHalData->CrystalCap & 0x3F;
	PHY_SetBBReg(Adapter, REG_MAC_PHY_CTRL, 0xFFF000, (CrystalCap | (CrystalCap << 6)));

	return rtStatus;
}

static void phy_LCK_8723B(struct adapter *Adapter)
{
	PHY_SetRFReg(Adapter, RF_PATH_A, 0xB0, bRFRegOffsetMask, 0xDFBE0);
	PHY_SetRFReg(Adapter, RF_PATH_A, RF_CHNLBW, bRFRegOffsetMask, 0x8C01);
	mdelay(200);
	PHY_SetRFReg(Adapter, RF_PATH_A, 0xB0, bRFRegOffsetMask, 0xDFFE0);
}

int PHY_RFConfig8723B(struct adapter *Adapter)
{
	int rtStatus = _SUCCESS;

	 
	 
	 
	rtStatus = PHY_RF6052_Config8723B(Adapter);

	phy_LCK_8723B(Adapter);

	return rtStatus;
}

 

void PHY_SetTxPowerIndex(
	struct adapter *Adapter,
	u32 PowerIndex,
	u8 RFPath,
	u8 Rate
)
{
	if (RFPath == RF_PATH_A || RFPath == RF_PATH_B) {
		switch (Rate) {
		case MGN_1M:
			PHY_SetBBReg(Adapter, rTxAGC_A_CCK1_Mcs32, bMaskByte1, PowerIndex);
			break;
		case MGN_2M:
			PHY_SetBBReg(Adapter, rTxAGC_B_CCK11_A_CCK2_11, bMaskByte1, PowerIndex);
			break;
		case MGN_5_5M:
			PHY_SetBBReg(Adapter, rTxAGC_B_CCK11_A_CCK2_11, bMaskByte2, PowerIndex);
			break;
		case MGN_11M:
			PHY_SetBBReg(Adapter, rTxAGC_B_CCK11_A_CCK2_11, bMaskByte3, PowerIndex);
			break;

		case MGN_6M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate18_06, bMaskByte0, PowerIndex);
			break;
		case MGN_9M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate18_06, bMaskByte1, PowerIndex);
			break;
		case MGN_12M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate18_06, bMaskByte2, PowerIndex);
			break;
		case MGN_18M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate18_06, bMaskByte3, PowerIndex);
			break;

		case MGN_24M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate54_24, bMaskByte0, PowerIndex);
			break;
		case MGN_36M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate54_24, bMaskByte1, PowerIndex);
			break;
		case MGN_48M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate54_24, bMaskByte2, PowerIndex);
			break;
		case MGN_54M:
			PHY_SetBBReg(Adapter, rTxAGC_A_Rate54_24, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS0:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs03_Mcs00, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS1:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs03_Mcs00, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS2:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs03_Mcs00, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS3:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs03_Mcs00, bMaskByte3, PowerIndex);
			break;

		case MGN_MCS4:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs07_Mcs04, bMaskByte0, PowerIndex);
			break;
		case MGN_MCS5:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs07_Mcs04, bMaskByte1, PowerIndex);
			break;
		case MGN_MCS6:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs07_Mcs04, bMaskByte2, PowerIndex);
			break;
		case MGN_MCS7:
			PHY_SetBBReg(Adapter, rTxAGC_A_Mcs07_Mcs04, bMaskByte3, PowerIndex);
			break;

		default:
			break;
		}
	}
}

u8 PHY_GetTxPowerIndex(
	struct adapter *padapter,
	u8 RFPath,
	u8 Rate,
	enum channel_width BandWidth,
	u8 Channel
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(padapter);
	s8 txPower = 0, powerDiffByRate = 0, limit = 0;

	txPower = (s8) PHY_GetTxPowerIndexBase(padapter, RFPath, Rate, BandWidth, Channel);
	powerDiffByRate = PHY_GetTxPowerByRate(padapter, RF_PATH_A, Rate);

	limit = phy_get_tx_pwr_lmt(
		padapter,
		padapter->registrypriv.RegPwrTblSel,
		pHalData->CurrentChannelBW,
		RFPath,
		Rate,
		pHalData->CurrentChannel
	);

	powerDiffByRate = powerDiffByRate > limit ? limit : powerDiffByRate;
	txPower += powerDiffByRate;

	txPower += PHY_GetTxPowerTrackingOffset(padapter, RFPath, Rate);

	if (txPower > MAX_POWER_INDEX)
		txPower = MAX_POWER_INDEX;

	return (u8) txPower;
}

void PHY_SetTxPowerLevel8723B(struct adapter *Adapter, u8 Channel)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	struct dm_odm_t *pDM_Odm = &pHalData->odmpriv;
	struct fat_t *pDM_FatTable = &pDM_Odm->DM_FatTable;
	u8 RFPath = RF_PATH_A;

	if (pHalData->AntDivCfg) { 
		RFPath = ((pDM_FatTable->RxIdleAnt == MAIN_ANT) ? RF_PATH_A : RF_PATH_B);
	} else {  
		RFPath = pHalData->ant_path;
	}

	PHY_SetTxPowerLevelByPath(Adapter, Channel, RFPath);
}

void PHY_GetTxPowerLevel8723B(struct adapter *Adapter, s32 *powerlevel)
{
}

static void phy_SetRegBW_8723B(
	struct adapter *Adapter, enum channel_width CurrentBW
)
{
	u16 RegRfMod_BW, u2tmp = 0;
	RegRfMod_BW = rtw_read16(Adapter, REG_TRXPTCL_CTL_8723B);

	switch (CurrentBW) {
	case CHANNEL_WIDTH_20:
		rtw_write16(Adapter, REG_TRXPTCL_CTL_8723B, (RegRfMod_BW & 0xFE7F));  
		break;

	case CHANNEL_WIDTH_40:
		u2tmp = RegRfMod_BW | BIT7;
		rtw_write16(Adapter, REG_TRXPTCL_CTL_8723B, (u2tmp & 0xFEFF));  
		break;

	default:
		break;
	}
}

static u8 phy_GetSecondaryChnl_8723B(struct adapter *Adapter)
{
	u8 SCSettingOf40 = 0, SCSettingOf20 = 0;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	if (pHalData->CurrentChannelBW == CHANNEL_WIDTH_40) {
		if (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_UPPER)
			SCSettingOf20 = HT_DATA_SC_20_UPPER_OF_40MHZ;
		else if (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER)
			SCSettingOf20 = HT_DATA_SC_20_LOWER_OF_40MHZ;
	}

	return  (SCSettingOf40 << 4) | SCSettingOf20;
}

static void phy_PostSetBwMode8723B(struct adapter *Adapter)
{
	u8 SubChnlNum = 0;
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);


	 
	phy_SetRegBW_8723B(Adapter, pHalData->CurrentChannelBW);

	 
	SubChnlNum = phy_GetSecondaryChnl_8723B(Adapter);
	rtw_write8(Adapter, REG_DATA_SC_8723B, SubChnlNum);

	 
	 
	 
	switch (pHalData->CurrentChannelBW) {
	 
	case CHANNEL_WIDTH_20:
		PHY_SetBBReg(Adapter, rFPGA0_RFMOD, bRFMOD, 0x0);

		PHY_SetBBReg(Adapter, rFPGA1_RFMOD, bRFMOD, 0x0);

		PHY_SetBBReg(Adapter, rOFDM0_TxPseudoNoiseWgt, (BIT31|BIT30), 0x0);
		break;

	 
	case CHANNEL_WIDTH_40:
		PHY_SetBBReg(Adapter, rFPGA0_RFMOD, bRFMOD, 0x1);

		PHY_SetBBReg(Adapter, rFPGA1_RFMOD, bRFMOD, 0x1);

		 
		PHY_SetBBReg(Adapter, rCCK0_System, bCCKSideBand, (pHalData->nCur40MhzPrimeSC>>1));

		PHY_SetBBReg(Adapter, rOFDM1_LSTF, 0xC00, pHalData->nCur40MhzPrimeSC);

		PHY_SetBBReg(Adapter, 0x818, (BIT26|BIT27), (pHalData->nCur40MhzPrimeSC == HAL_PRIME_CHNL_OFFSET_LOWER) ? 2 : 1);
		break;
	default:
		break;
	}

	 
	PHY_RF6052SetBandwidth8723B(Adapter, pHalData->CurrentChannelBW);
}

static void phy_SwChnl8723B(struct adapter *padapter)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(padapter);
	u8 channelToSW = pHalData->CurrentChannel;

	if (pHalData->rf_chip == RF_PSEUDO_11N)
		return;
	pHalData->RfRegChnlVal[0] = ((pHalData->RfRegChnlVal[0] & 0xfffff00) | channelToSW);
	PHY_SetRFReg(padapter, RF_PATH_A, RF_CHNLBW, 0x3FF, pHalData->RfRegChnlVal[0]);
	PHY_SetRFReg(padapter, RF_PATH_B, RF_CHNLBW, 0x3FF, pHalData->RfRegChnlVal[0]);
}

static void phy_SwChnlAndSetBwMode8723B(struct adapter *Adapter)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	if (Adapter->bDriverStopped || Adapter->bSurpriseRemoved)
		return;

	if (pHalData->bSwChnl) {
		phy_SwChnl8723B(Adapter);
		pHalData->bSwChnl = false;
	}

	if (pHalData->bSetChnlBW) {
		phy_PostSetBwMode8723B(Adapter);
		pHalData->bSetChnlBW = false;
	}

	PHY_SetTxPowerLevel8723B(Adapter, pHalData->CurrentChannel);
}

static void PHY_HandleSwChnlAndSetBW8723B(
	struct adapter *Adapter,
	bool bSwitchChannel,
	bool bSetBandWidth,
	u8 ChannelNum,
	enum channel_width ChnlWidth,
	enum extchnl_offset ExtChnlOffsetOf40MHz,
	enum extchnl_offset ExtChnlOffsetOf80MHz,
	u8 CenterFrequencyIndex1
)
{
	 
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);
	u8 tmpChannel = pHalData->CurrentChannel;
	enum channel_width tmpBW = pHalData->CurrentChannelBW;
	u8 tmpnCur40MhzPrimeSC = pHalData->nCur40MhzPrimeSC;
	u8 tmpnCur80MhzPrimeSC = pHalData->nCur80MhzPrimeSC;
	u8 tmpCenterFrequencyIndex1 = pHalData->CurrentCenterFrequencyIndex1;

	 
	if (!bSwitchChannel && !bSetBandWidth)
		return;

	 
	if (bSwitchChannel) {
		{
			if (HAL_IsLegalChannel(Adapter, ChannelNum))
				pHalData->bSwChnl = true;
		}
	}

	if (bSetBandWidth)
		pHalData->bSetChnlBW = true;

	if (!pHalData->bSetChnlBW && !pHalData->bSwChnl)
		return;


	if (pHalData->bSwChnl) {
		pHalData->CurrentChannel = ChannelNum;
		pHalData->CurrentCenterFrequencyIndex1 = ChannelNum;
	}


	if (pHalData->bSetChnlBW) {
		pHalData->CurrentChannelBW = ChnlWidth;
		pHalData->nCur40MhzPrimeSC = ExtChnlOffsetOf40MHz;
		pHalData->nCur80MhzPrimeSC = ExtChnlOffsetOf80MHz;
		pHalData->CurrentCenterFrequencyIndex1 = CenterFrequencyIndex1;
	}

	 
	if ((!Adapter->bDriverStopped) && (!Adapter->bSurpriseRemoved)) {
		phy_SwChnlAndSetBwMode8723B(Adapter);
	} else {
		if (pHalData->bSwChnl) {
			pHalData->CurrentChannel = tmpChannel;
			pHalData->CurrentCenterFrequencyIndex1 = tmpChannel;
		}

		if (pHalData->bSetChnlBW) {
			pHalData->CurrentChannelBW = tmpBW;
			pHalData->nCur40MhzPrimeSC = tmpnCur40MhzPrimeSC;
			pHalData->nCur80MhzPrimeSC = tmpnCur80MhzPrimeSC;
			pHalData->CurrentCenterFrequencyIndex1 = tmpCenterFrequencyIndex1;
		}
	}
}

void PHY_SetBWMode8723B(
	struct adapter *Adapter,
	enum channel_width Bandwidth,  
	unsigned char Offset  
)
{
	struct hal_com_data *pHalData = GET_HAL_DATA(Adapter);

	PHY_HandleSwChnlAndSetBW8723B(Adapter, false, true, pHalData->CurrentChannel, Bandwidth, Offset, Offset, pHalData->CurrentChannel);
}

 
void PHY_SwChnl8723B(struct adapter *Adapter, u8 channel)
{
	PHY_HandleSwChnlAndSetBW8723B(Adapter, true, false, channel, 0, 0, 0, channel);
}

void PHY_SetSwChnlBWMode8723B(
	struct adapter *Adapter,
	u8 channel,
	enum channel_width Bandwidth,
	u8 Offset40,
	u8 Offset80
)
{
	PHY_HandleSwChnlAndSetBW8723B(Adapter, true, true, channel, Bandwidth, Offset40, Offset80, channel);
}

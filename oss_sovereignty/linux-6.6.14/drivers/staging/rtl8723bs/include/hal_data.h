 
 
#ifndef __HAL_DATA_H__
#define __HAL_DATA_H__

#include "odm_precomp.h"
#include <hal_btcoex.h>

#include <hal_sdio.h>

 
 
 
enum rt_multi_func {
	RT_MULTI_FUNC_NONE	= 0x00,
	RT_MULTI_FUNC_WIFI	= 0x01,
	RT_MULTI_FUNC_BT		= 0x02,
	RT_MULTI_FUNC_GPS	= 0x04,
};
 
 
 
enum rt_polarity_ctl {
	RT_POLARITY_LOW_ACT	= 0,
	RT_POLARITY_HIGH_ACT	= 1,
};

 
enum rt_regulator_mode {
	RT_SWITCHING_REGULATOR	= 0,
	RT_LDO_REGULATOR	= 1,
};

enum rt_ampdu_burst {
	RT_AMPDU_BURST_NONE	= 0,
	RT_AMPDU_BURST_92D	= 1,
	RT_AMPDU_BURST_88E	= 2,
	RT_AMPDU_BURST_8812_4	= 3,
	RT_AMPDU_BURST_8812_8	= 4,
	RT_AMPDU_BURST_8812_12	= 5,
	RT_AMPDU_BURST_8812_15	= 6,
	RT_AMPDU_BURST_8723B	= 7,
};

#define CHANNEL_MAX_NUMBER		(14)	 
#define CHANNEL_MAX_NUMBER_2G		14
#define MAX_PG_GROUP			13

 
#define MAX_REGULATION_NUM			4
#define MAX_2_4G_BANDWIDTH_NUM			2
#define MAX_RATE_SECTION_NUM			3  

 
 
 

 

 
 

enum {
	SINGLEMAC_SINGLEPHY,	 
	DUALMAC_DUALPHY,		 
	DUALMAC_SINGLEPHY,	 
};

#define PAGE_SIZE_128	128
#define PAGE_SIZE_256	256
#define PAGE_SIZE_512	512

struct dm_priv {
	u8 DM_Type;

#define DYNAMIC_FUNC_BT BIT0

	u8 DMFlag;
	u8 InitDMFlag;
	 

	u32 InitODMFlag;
	 
	int	UndecoratedSmoothedPWDB;
	int	UndecoratedSmoothedCCK;
	int	EntryMinUndecoratedSmoothedPWDB;
	int	EntryMaxUndecoratedSmoothedPWDB;
	int	MinUndecoratedPWDBForDM;
	int	LastMinUndecoratedPWDBForDM;

	s32	UndecoratedSmoothedBeacon;

 
	 
	u8 bDynamicTxPowerEnable;
	u8 LastDTPLvl;
	u8 DynamicTxHighPowerLvl; 

	 
	u8 bTXPowerTracking;
	u8 TXPowercount;
	u8 bTXPowerTrackingInit;
	u8 TxPowerTrackControl;	 
	u8 TM_Trigger;

	u8 ThermalMeter[2];				 
	u8 ThermalValue;
	u8 ThermalValue_LCK;
	u8 ThermalValue_IQK;
	u8 ThermalValue_DPK;
	u8 bRfPiEnable;
	 

	 
	u32 APKoutput[2][2];	 
	u8 bAPKdone;
	u8 bAPKThermalMeterIgnore;
	u8 bDPdone;
	u8 bDPPathAOK;
	u8 bDPPathBOK;
	 
	 
	 

	 
	u32 ADDA_backup[IQK_ADDA_REG_NUM];
	u32 IQK_MAC_backup[IQK_MAC_REG_NUM];
	u32 IQK_BB_backup_recover[9];
	u32 IQK_BB_backup[IQK_BB_REG_NUM];

	u8 PowerIndex_backup[6];
	u8 OFDM_index[2];

	u8 bCCKinCH14;
	u8 CCK_index;
	u8 bDoneTxpower;
	u8 CCK_index_HP;

	u8 OFDM_index_HP[2];
	u8 ThermalValue_HP[HP_THERMAL_NUM];
	u8 ThermalValue_HP_index;
	 

	 
	s32	RegE94;
	s32  RegE9C;
	s32	RegEB4;
	s32	RegEBC;

	u32 TXPowerTrackingCallbackCnt;	 

	u32 prv_traffic_idx;  
 

	 
	u8 INIDATA_RATE[32];
};


struct hal_com_data {
	struct hal_version VersionID;
	enum rt_multi_func MultiFunc;  
	enum rt_polarity_ctl PolarityCtl;  
	enum rt_regulator_mode	RegulatorMode;  

	u16 FirmwareVersion;
	u16 FirmwareVersionRev;
	u16 FirmwareSubVersion;
	u16 FirmwareSignature;

	 
	enum wireless_mode CurrentWirelessMode;
	enum channel_width CurrentChannelBW;
	u8 CurrentChannel;
	u8 CurrentCenterFrequencyIndex1;
	u8 nCur40MhzPrimeSC; 
	u8 nCur80MhzPrimeSC;    

	u16 CustomerID;
	u16 BasicRateSet;
	u16 ForcedDataRate; 
	u32 ReceiveConfig;

	 
	u8 rf_chip;
	u8 PackageType;
	u8 NumTotalRFPath;

	u8 InterfaceSel;
	u8 framesync;
	u32 framesyncC34;
	u8 framesyncMonitor;
	u8 DefaultInitialGain[4];
	 
	u16 EEPROMVID;
	u16 EEPROMSVID;

	u8 EEPROMCustomerID;
	u8 EEPROMSubCustomerID;
	u8 EEPROMVersion;
	u8 EEPROMRegulatory;
	u8 EEPROMThermalMeter;
	u8 EEPROMBluetoothCoexist;
	u8 EEPROMBluetoothType;
	u8 EEPROMBluetoothAntNum;
	u8 EEPROMBluetoothAntIsolation;
	u8 EEPROMBluetoothRadioShared;
	u8 bTXPowerDataReadFromEEPORM;
	u8 bAPKThermalMeterIgnore;
	u8 bDisableSWChannelPlan;  

	bool		EepromOrEfuse;
	u8 		EfuseUsedPercentage;
	u16 			EfuseUsedBytes;
	struct efuse_hal		EfuseHal;

	 
	u8 Index24G_CCK_Base[MAX_RF_PATH][CHANNEL_MAX_NUMBER];
	u8 Index24G_BW40_Base[MAX_RF_PATH][CHANNEL_MAX_NUMBER];
	 
	s8	CCK_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	OFDM_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW20_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];
	s8	BW40_24G_Diff[MAX_RF_PATH][MAX_TX_COUNT];

	u8 Regulation2_4G;

	u8 TxPwrInPercentage;

	u8 TxPwrCalibrateRate;
	 
	 
	 
	u8 TxPwrByRateTable;
	u8 TxPwrByRateBand;
	s8 TxPwrByRateOffset[MAX_RF_PATH_NUM][TX_PWR_BY_RATE_NUM_RATE];
	 

	 
	u8 TxPwrLevelCck[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];
	u8 TxPwrLevelHT40_1S[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];	 
	u8 TxPwrLevelHT40_2S[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];	 
	s8	TxPwrHt20Diff[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER]; 
	u8 TxPwrLegacyHtDiff[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER]; 

	 
	s8	TxPwrLimit_2_4G[MAX_REGULATION_NUM]
						[MAX_2_4G_BANDWIDTH_NUM]
	                                [MAX_RATE_SECTION_NUM]
	                                [CHANNEL_MAX_NUMBER_2G]
						[MAX_RF_PATH_NUM];

	 
	u8 TxPwrByRateBase2_4G[MAX_RF_PATH_NUM][MAX_RATE_SECTION_NUM];

	 
	u8 PwrGroupHT20[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];
	u8 PwrGroupHT40[RF_PATH_MAX_92C_88E][CHANNEL_MAX_NUMBER];




	u8 PGMaxGroup;
	u8 LegacyHTTxPowerDiff; 
	 
	u8 CurrentCckTxPwrIdx;
	u8 CurrentOfdm24GTxPwrIdx;
	u8 CurrentBW2024GTxPwrIdx;
	u8 CurrentBW4024GTxPwrIdx;

	 
	u8 pwrGroupCnt;
	u32 MCSTxPowerLevelOriginalOffset[MAX_PG_GROUP][16];
	u32 CCKTxPowerLevelOriginalOffset;

	u8 CrystalCap;
	u32 AntennaTxPath;					 
	u32 AntennaRxPath;					 

	u8 PAType_2G;
	u8 LNAType_2G;
	u8 ExternalPA_2G;
	u8 ExternalLNA_2G;
	u8 TypeGLNA;
	u8 TypeGPA;
	u8 TypeALNA;
	u8 TypeAPA;
	u8 RFEType;
	u8 BoardType;
	u8 ExternalPA;
	u8 bIQKInitialized;
	bool		bLCKInProgress;

	bool		bSwChnl;
	bool		bSetChnlBW;
	bool		bChnlBWInitialized;
	bool		bNeedIQK;

	u8 bLedOpenDrain;  
	u8 TxPowerTrackControl;  
	u8 b1x1RecvCombine;	 

	u32 AcParam_BE;  

	struct bb_register_def PHYRegDef[4];	 

	u32 RfRegChnlVal[2];

	 
	bool	 bRDGEnable;

	 
	u8 LastHMEBoxNum;

	u8 fw_ractrl;
	u8 RegTxPause;
	 
	u8 RegBcnCtrlVal;
	u8 RegFwHwTxQCtrl;
	u8 RegReg542;
	u8 RegCR_1;
	u8 Reg837;
	u8 RegRFPathS1;
	u16 RegRRSR;

	u8 CurAntenna;
	u8 AntDivCfg;
	u8 AntDetection;
	u8 TRxAntDivType;
	u8 ant_path;  

	u8 u1ForcedIgiLb;			 

	u8 bDumpRxPkt; 
	u8 bDumpTxPkt; 
	u8 FwRsvdPageStartOffset;  

	 
	bool		pwrdown;

	 
	u32 interfaceIndex;

	u8 OutEpQueueSel;
	u8 OutEpNumber;

	 
	bool		UsbRxHighSpeedMode;

	 
	 
	bool		SlimComboDbg;

	 

	 
	u8 bMacPwrCtrlOn;

	u8 RegIQKFWOffload;
	struct submit_ctx	iqk_sctx;

	enum rt_ampdu_burst	AMPDUBurstMode;  

	u32 		sdio_himr;
	u32 		sdio_hisr;

	 
	 
	u8 	SdioTxFIFOFreePage[SDIO_TX_FREE_PG_QUEUE];
	spinlock_t		SdioTxFIFOFreePageLock;
	u8 	SdioTxOQTMaxFreeSpace;
	u8 	SdioTxOQTFreeSpace;


	 
	u8 	SdioRxFIFOCnt;
	u16 		SdioRxFIFOSize;

	u32 		sdio_tx_max_len[SDIO_MAX_TX_QUEUE]; 

	struct dm_priv dmpriv;
	struct dm_odm_t		odmpriv;

	 
	struct bt_coexist		bt_coexist;

	 
	u32 		SysIntrStatus;
	u32 		SysIntrMask;
};

#define GET_HAL_DATA(__padapter)	((struct hal_com_data *)((__padapter)->HalData))
#define GET_HAL_RFPATH_NUM(__padapter) (((struct hal_com_data *)((__padapter)->HalData))->NumTotalRFPath)
#define RT_GetInterfaceSelection(_Adapter)	(GET_HAL_DATA(_Adapter)->InterfaceSel)

#endif  

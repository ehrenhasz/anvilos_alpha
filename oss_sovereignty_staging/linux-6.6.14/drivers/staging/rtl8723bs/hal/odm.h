 
 


#ifndef	__HALDMOUTSRC_H__
#define __HALDMOUTSRC_H__

#include "odm_EdcaTurboCheck.h"
#include "odm_DIG.h"
#include "odm_DynamicBBPowerSaving.h"
#include "odm_DynamicTxPower.h"
#include "odm_CfoTracking.h"

#define	TP_MODE		0
#define	RSSI_MODE		1
#define	TRAFFIC_LOW	0
#define	TRAFFIC_HIGH	1
#define	NONE			0

 
 
#define		DPK_DELTA_MAPPING_NUM	13
#define		index_mapping_HP_NUM	15
#define	OFDM_TABLE_SIZE		43
#define	CCK_TABLE_SIZE			33
#define TXSCALE_TABLE_SIZE		37
#define TXPWR_TRACK_TABLE_SIZE	30
#define DELTA_SWINGIDX_SIZE     30
#define BAND_NUM				4

 
 

#define	AFH_PSD		1	 
#define	MODE_40M		0	 
#define	PSD_TH2		3
#define	PSD_CHMIN		20    
#define	SIR_STEP_SIZE	3
#define   Smooth_Size_1		5
#define	Smooth_TH_1	3
#define   Smooth_Size_2		10
#define	Smooth_TH_2	4
#define   Smooth_Size_3		20
#define	Smooth_TH_3	4
#define   Smooth_Step_Size 5
#define	Adaptive_SIR	1
#define	PSD_RESCAN		4
#define	PSD_SCAN_INTERVAL	700  

 
#define		DM_DIG_HIGH_PWR_IGI_LOWER_BOUND	0x22
#define			DM_DIG_Gmode_HIGH_PWR_IGI_LOWER_BOUND 0x28
#define		DM_DIG_HIGH_PWR_THRESHOLD	0x3a
#define		DM_DIG_LOW_PWR_THRESHOLD	0x14

 
#define			ANTTESTALL		0x00		 
#define		ANTTESTA		0x01		 
#define		ANTTESTB		0x02		 

#define	PS_MODE_ACTIVE 0x01

 
#define		MAIN_ANT		1		 
#define		AUX_ANT		2		 
#define		MAX_ANT		3		 

 
#define	SW_ANTDIV	0
#define	HW_ANTDIV	1
 

 

 

 

struct dynamic_primary_CCA {
	u8 PriCCA_flag;
	u8 intf_flag;
	u8 intf_type;
	u8 DupRTS_flag;
	u8 Monitor_flag;
	u8 CH_offset;
	u8 MF_state;
};

struct ra_t {
	u8 firstconnect;
};

struct rxhp_t {
	u8 RXHP_flag;
	u8 PSD_func_trigger;
	u8 PSD_bitmap_RXHP[80];
	u8 Pre_IGI;
	u8 Cur_IGI;
	u8 Pre_pw_th;
	u8 Cur_pw_th;
	bool First_time_enter;
	bool RXHP_enable;
	u8 TP_Mode;
	struct timer_list PSDTimer;
};

#define ASSOCIATE_ENTRY_NUM					32  
#define	ODM_ASSOCIATE_ENTRY_NUM				ASSOCIATE_ENTRY_NUM

 
 
 
 
#define SWAW_STEP_PEAK		0
#define SWAW_STEP_DETERMINE	1

#define	TP_MODE		0
#define	RSSI_MODE		1
#define	TRAFFIC_LOW	0
#define	TRAFFIC_HIGH	1
#define	TRAFFIC_UltraLOW	2

struct swat_t {  
	u8 Double_chk_flag;
	u8 try_flag;
	s32 PreRSSI;
	u8 CurAntenna;
	u8 PreAntenna;
	u8 RSSI_Trying;
	u8 TestMode;
	u8 bTriggerAntennaSwitch;
	u8 SelectAntennaMap;
	u8 RSSI_target;
	u8 reset_idx;
	u16 Single_Ant_Counter;
	u16 Dual_Ant_Counter;
	u16 Aux_FailDetec_Counter;
	u16 Retry_Counter;

	 
	u8 SWAS_NoLink_State;
	u32 SWAS_NoLink_BK_Reg860;
	u32 SWAS_NoLink_BK_Reg92c;
	u32 SWAS_NoLink_BK_Reg948;
	bool ANTA_ON;	 
	bool ANTB_ON;	 
	bool Pre_Aux_FailDetec;
	bool RSSI_AntDect_bResult;
	u8 Ant2G;

	s32 RSSI_sum_A;
	s32 RSSI_sum_B;
	s32 RSSI_cnt_A;
	s32 RSSI_cnt_B;

	u64 lastTxOkCnt;
	u64 lastRxOkCnt;
	u64 TXByteCnt_A;
	u64 TXByteCnt_B;
	u64 RXByteCnt_A;
	u64 RXByteCnt_B;
	u8 TrafficLoad;
	u8 Train_time;
	u8 Train_time_flag;
	struct timer_list SwAntennaSwitchTimer;
	struct timer_list SwAntennaSwitchTimer_8723B;
	u32 PktCnt_SWAntDivByCtrlFrame;
	bool bSWAntDivByCtrlFrame;
};

 


struct odm_rate_adaptive {
	u8 Type;				 
	u8 LdpcThres;			 
	bool bUseLdpc;
	bool bLowerRtsRate;
	u8 HighRSSIThresh;		 
	u8 LowRSSIThresh;		 
	u8 RATRState;			 

};

#define IQK_MAC_REG_NUM		4
#define IQK_ADDA_REG_NUM		16
#define IQK_BB_REG_NUM_MAX	10
#define IQK_BB_REG_NUM		9
#define HP_THERMAL_NUM		8

#define AVG_THERMAL_NUM		8
#define IQK_MATRIX_REG_NUM	8
#define IQK_MATRIX_SETTINGS_NUM	14  

#define		DM_Type_ByFW			0
#define		DM_Type_ByDriver		1

 
 
 
#define MAX_PATH_NUM_92CS		2
#define MAX_PATH_NUM_8188E		1
#define MAX_PATH_NUM_8192E		2
#define MAX_PATH_NUM_8723B		1
#define MAX_PATH_NUM_8812A		2
#define MAX_PATH_NUM_8821A		1
#define MAX_PATH_NUM_8814A		4
#define MAX_PATH_NUM_8822B		2

#define IQK_THRESHOLD			8
#define DPK_THRESHOLD			4

struct odm_phy_info {
	 
	u8 rx_pwd_ba11;

	u8 signal_quality;              
	s8 rx_mimo_signal_quality[4];   
	u8 rx_mimo_evm_dbm[4];          

	u8 rx_mimo_signal_strength[4];  

	u16 cfo_short[4];               
	u16 cfo_tail[4];                

	s8 rx_power;                    

	 
	s8 recv_signal_power;
	u8 bt_rx_rssi_percentage;
	u8 signal_strength;	        

	s8 rx_pwr[4];                   

	u8 rx_snr[4];                   
	u8 band_width;
	u8 bt_coex_pwr_adjust;
};

struct odm_packet_info {
	u8 data_rate;
	u8 station_id;
	bool bssid_match;
	bool to_self;
	bool is_beacon;
};

struct odm_phy_dbg_info {
	 
	s8 RxSNRdB[4];
	u32 NumQryPhyStatus;
	u32 NumQryPhyStatusCCK;
	u32 NumQryPhyStatusOFDM;
	u8 NumQryBeaconPkt;
	 
	s32 RxEVM[4];

};

struct odm_mac_status_info {
	u8 test;
};

 
 
 
enum odm_cmninfo_e {
	 

	 
	ODM_CMNINFO_PLATFORM = 0,
	ODM_CMNINFO_ABILITY,					 
	ODM_CMNINFO_INTERFACE,				 
	ODM_CMNINFO_IC_TYPE,					 
	ODM_CMNINFO_CUT_VER,					 
	ODM_CMNINFO_FAB_VER,					 
	ODM_CMNINFO_RFE_TYPE,
	ODM_CMNINFO_PACKAGE_TYPE,
	ODM_CMNINFO_EXT_LNA,					 
	ODM_CMNINFO_EXT_PA,
	ODM_CMNINFO_GPA,
	ODM_CMNINFO_APA,
	ODM_CMNINFO_GLNA,
	ODM_CMNINFO_ALNA,
	ODM_CMNINFO_EXT_TRSW,
	ODM_CMNINFO_PATCH_ID,				 
	ODM_CMNINFO_BINHCT_TEST,
	ODM_CMNINFO_BWIFI_TEST,
	ODM_CMNINFO_SMART_CONCURRENT,
	 

	 
 
	ODM_CMNINFO_MAC_PHY_MODE,	 
	ODM_CMNINFO_TX_UNI,
	ODM_CMNINFO_RX_UNI,
	ODM_CMNINFO_WM_MODE,		 
	ODM_CMNINFO_SEC_CHNL_OFFSET,	 
	ODM_CMNINFO_SEC_MODE,		 
	ODM_CMNINFO_BW,			 
	ODM_CMNINFO_CHNL,
	ODM_CMNINFO_FORCED_RATE,

	ODM_CMNINFO_DMSP_GET_VALUE,
	ODM_CMNINFO_BUDDY_ADAPTOR,
	ODM_CMNINFO_DMSP_IS_MASTER,
	ODM_CMNINFO_SCAN,
	ODM_CMNINFO_POWER_SAVING,
	ODM_CMNINFO_ONE_PATH_CCA,	 
	ODM_CMNINFO_DRV_STOP,
	ODM_CMNINFO_PNP_IN,
	ODM_CMNINFO_INIT_ON,
	ODM_CMNINFO_ANT_TEST,
	ODM_CMNINFO_NET_CLOSED,
	ODM_CMNINFO_MP_MODE,
	 
	ODM_CMNINFO_FORCED_IGI_LB,
	ODM_CMNINFO_IS1ANTENNA,
	ODM_CMNINFO_RFDEFAULTPATH,
 

 
	ODM_CMNINFO_WIFI_DIRECT,
	ODM_CMNINFO_WIFI_DISPLAY,
	ODM_CMNINFO_LINK_IN_PROGRESS,
	ODM_CMNINFO_LINK,
	ODM_CMNINFO_STATION_STATE,
	ODM_CMNINFO_RSSI_MIN,
	ODM_CMNINFO_DBG_COMP,			 
	ODM_CMNINFO_DBG_LEVEL,			 
	ODM_CMNINFO_RA_THRESHOLD_HIGH,		 
	ODM_CMNINFO_RA_THRESHOLD_LOW,		 
	ODM_CMNINFO_RF_ANTENNA_TYPE,		 
	ODM_CMNINFO_BT_ENABLED,
	ODM_CMNINFO_BT_HS_CONNECT_PROCESS,
	ODM_CMNINFO_BT_HS_RSSI,
	ODM_CMNINFO_BT_OPERATION,
	ODM_CMNINFO_BT_LIMITED_DIG,		 
	ODM_CMNINFO_BT_DISABLE_EDCA,
 

	 
	ODM_CMNINFO_STA_STATUS,
	ODM_CMNINFO_PHY_STATUS,
	ODM_CMNINFO_MAC_STATUS,

	ODM_CMNINFO_MAX,
};

 
enum {  
	 
	 
	 
	ODM_BB_DIG			= BIT0,
	ODM_BB_RA_MASK			= BIT1,
	ODM_BB_DYNAMIC_TXPWR		= BIT2,
	ODM_BB_FA_CNT			= BIT3,
	ODM_BB_RSSI_MONITOR		= BIT4,
	ODM_BB_CCK_PD			= BIT5,
	ODM_BB_ANT_DIV			= BIT6,
	ODM_BB_PWR_SAVE			= BIT7,
	ODM_BB_PWR_TRAIN		= BIT8,
	ODM_BB_RATE_ADAPTIVE		= BIT9,
	ODM_BB_PATH_DIV			= BIT10,
	ODM_BB_PSD			= BIT11,
	ODM_BB_RXHP			= BIT12,
	ODM_BB_ADAPTIVITY		= BIT13,
	ODM_BB_CFO_TRACKING		= BIT14,

	 
	ODM_MAC_EDCA_TURBO		= BIT16,
	ODM_MAC_EARLY_MODE		= BIT17,

	 
	ODM_RF_TX_PWR_TRACK		= BIT24,
	ODM_RF_RX_GAIN_TRACK	= BIT25,
	ODM_RF_CALIBRATION		= BIT26,
};

 
enum {  
	ODM_ITRF_SDIO	=	0x4,
	ODM_ITRF_ALL	=	0x7,
};

 
enum {  
	ODM_RTL8723B	=	BIT8,
};

 
enum {  
	ODM_CUT_A		=	0,
	ODM_CUT_B		=	1,
	ODM_CUT_C		=	2,
	ODM_CUT_D		=	3,
	ODM_CUT_E		=	4,
	ODM_CUT_F		=	5,

	ODM_CUT_I		=	8,
	ODM_CUT_J		=	9,
	ODM_CUT_K		=	10,
	ODM_CUT_TEST	=	15,
};

 
enum {  
	ODM_TSMC	=	0,
	ODM_UMC		=	1,
};

 
 
 
enum {  
	ODM_1T1R	=	0,
	ODM_1T2R	=	1,
	ODM_2T2R	=	2,
	ODM_2T3R	=	3,
	ODM_2T4R	=	4,
	ODM_3T3R	=	5,
	ODM_3T4R	=	6,
	ODM_4T4R	=	7,
};

 
 
 

 
enum {  
	ODM_WM_UNKNOWN    = 0x0,
	ODM_WM_B          = BIT0,
	ODM_WM_G          = BIT1,
	ODM_WM_N24G       = BIT3,
	ODM_WM_AUTO       = BIT5,
};

 
enum {  
	ODM_BW20M		= 0,
	ODM_BW40M		= 1,
};

 

enum odm_type_gpa_e {  
	TYPE_GPA0 = 0,
	TYPE_GPA1 = BIT(1)|BIT(0)
};

enum odm_type_apa_e {  
	TYPE_APA0 = 0,
	TYPE_APA1 = BIT(1)|BIT(0)
};

enum odm_type_glna_e {  
	TYPE_GLNA0 = 0,
	TYPE_GLNA1 = BIT(2)|BIT(0),
	TYPE_GLNA2 = BIT(3)|BIT(1),
	TYPE_GLNA3 = BIT(3)|BIT(2)|BIT(1)|BIT(0)
};

enum odm_type_alna_e {  
	TYPE_ALNA0 = 0,
	TYPE_ALNA1 = BIT(2)|BIT(0),
	TYPE_ALNA2 = BIT(3)|BIT(1),
	TYPE_ALNA3 = BIT(3)|BIT(2)|BIT(1)|BIT(0)
};

 

struct odm_rf_cal_t {  
	 

	u32 RegA24;  
	s32 RegE94;
	s32 RegE9C;
	s32 RegEB4;
	s32 RegEBC;

	u8 TXPowercount;
	bool bTXPowerTrackingInit;
	bool bTXPowerTracking;
	u8 TxPowerTrackControl;  
	u8 TM_Trigger;

	u8 ThermalMeter[2];     
	u8 ThermalValue;
	u8 ThermalValue_LCK;
	u8 ThermalValue_IQK;
	u8 ThermalValue_DPK;
	u8 ThermalValue_AVG[AVG_THERMAL_NUM];
	u8 ThermalValue_AVG_index;
	u8 ThermalValue_RxGain;
	u8 ThermalValue_Crystal;
	u8 ThermalValue_DPKstore;
	u8 ThermalValue_DPKtrack;
	bool TxPowerTrackingInProgress;

	bool bReloadtxpowerindex;
	u8 bRfPiEnable;
	u32 TXPowerTrackingCallbackCnt;  

	 
	u8 bCCKinCH14;
	u8 CCK_index;
	u8 OFDM_index[MAX_RF_PATH];
	s8 PowerIndexOffset[MAX_RF_PATH];
	s8 DeltaPowerIndex[MAX_RF_PATH];
	s8 DeltaPowerIndexLast[MAX_RF_PATH];
	bool bTxPowerChanged;

	u8 ThermalValue_HP[HP_THERMAL_NUM];
	u8 ThermalValue_HP_index;
	s32 iqk_matrix_regs_setting_value[IQK_MATRIX_SETTINGS_NUM][IQK_MATRIX_REG_NUM];
	bool bNeedIQK;
	bool bIQKInProgress;
	u8 Delta_IQK;
	u8 Delta_LCK;
	s8 BBSwingDiff2G;  
	u8 DeltaSwingTableIdx_2GCCKA_P[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GCCKA_N[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GCCKB_P[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GCCKB_N[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GA_P[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GA_N[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GB_P[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GB_N[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GA_P_8188E[DELTA_SWINGIDX_SIZE];
	u8 DeltaSwingTableIdx_2GA_N_8188E[DELTA_SWINGIDX_SIZE];

	 

	 
	u32 RegC04;
	u32 Reg874;
	u32 RegC08;
	u32 RegB68;
	u32 RegB6C;
	u32 Reg870;
	u32 Reg860;
	u32 Reg864;

	bool bIQKInitialized;
	bool bLCKInProgress;
	bool bAntennaDetected;
	u32 ADDA_backup[IQK_ADDA_REG_NUM];
	u32 IQK_MAC_backup[IQK_MAC_REG_NUM];
	u32 IQK_BB_backup_recover[9];
	u32 IQK_BB_backup[IQK_BB_REG_NUM];
	u32 TxIQC_8723B[2][3][2];  
	u32 RxIQC_8723B[2][2][2];  

	 
	u32 APKoutput[2][2];  
	u8 bAPKdone;
	u8 bAPKThermalMeterIgnore;

	 
	bool bDPKFail;
	u8 bDPdone;
	u8 bDPPathAOK;
	u8 bDPPathBOK;

	u32 TxLOK[2];

};
 
 
 

struct fat_t {  
	u8 Bssid[6];
	u8 antsel_rx_keep_0;
	u8 antsel_rx_keep_1;
	u8 antsel_rx_keep_2;
	u8 antsel_rx_keep_3;
	u32 antSumRSSI[7];
	u32 antRSSIcnt[7];
	u32 antAveRSSI[7];
	u8 FAT_State;
	u32 TrainIdx;
	u8 antsel_a[ODM_ASSOCIATE_ENTRY_NUM];
	u8 antsel_b[ODM_ASSOCIATE_ENTRY_NUM];
	u8 antsel_c[ODM_ASSOCIATE_ENTRY_NUM];
	u32 MainAnt_Sum[ODM_ASSOCIATE_ENTRY_NUM];
	u32 AuxAnt_Sum[ODM_ASSOCIATE_ENTRY_NUM];
	u32 MainAnt_Cnt[ODM_ASSOCIATE_ENTRY_NUM];
	u32 AuxAnt_Cnt[ODM_ASSOCIATE_ENTRY_NUM];
	u8 RxIdleAnt;
	bool	bBecomeLinked;
	u32 MinMaxRSSI;
	u8 idx_AntDiv_counter_2G;
	u32 CCK_counter_main;
	u32 CCK_counter_aux;
	u32 OFDM_counter_main;
	u32 OFDM_counter_aux;

	u32 CCK_CtrlFrame_Cnt_main;
	u32 CCK_CtrlFrame_Cnt_aux;
	u32 OFDM_CtrlFrame_Cnt_main;
	u32 OFDM_CtrlFrame_Cnt_aux;
	u32 MainAnt_CtrlFrame_Sum;
	u32 AuxAnt_CtrlFrame_Sum;
	u32 MainAnt_CtrlFrame_Cnt;
	u32 AuxAnt_CtrlFrame_Cnt;

};

enum {
	NO_ANTDIV			= 0xFF,
	CG_TRX_HW_ANTDIV		= 0x01,
	CGCS_RX_HW_ANTDIV	= 0x02,
	FIXED_HW_ANTDIV		= 0x03,
	CG_TRX_SMART_ANTDIV	= 0x04,
	CGCS_RX_SW_ANTDIV	= 0x05,
	S0S1_SW_ANTDIV          = 0x06  
};

struct pathdiv_t {  
	u8 RespTxPath;
	u8 PathSel[ODM_ASSOCIATE_ENTRY_NUM];
	u32 PathA_Sum[ODM_ASSOCIATE_ENTRY_NUM];
	u32 PathB_Sum[ODM_ASSOCIATE_ENTRY_NUM];
	u32 PathA_Cnt[ODM_ASSOCIATE_ENTRY_NUM];
	u32 PathB_Cnt[ODM_ASSOCIATE_ENTRY_NUM];
};

enum phy_reg_pg_type {  
	PHY_REG_PG_RELATIVE_VALUE = 0,
	PHY_REG_PG_EXACT_VALUE = 1
};

 
 
 
struct ant_detected_info {
	bool bAntDetected;
	u32 dBForAntA;
	u32 dBForAntB;
	u32 dBForAntO;
};

 
 
 
struct dm_odm_t {  
	 
	 
	 
	 
	struct adapter *Adapter;		 
	 
	bool odm_ready;

	enum phy_reg_pg_type PhyRegPgValueType;
	u8 PhyRegPgVersion;

	u32 NumQryPhyStatusAll;	 
	u32 LastNumQryPhyStatusAll;
	u32 RxPWDBAve;
	bool MPDIG_2G;		 
	u8 Times_2G;

 
	bool bCckHighPower;
	u8 RFPathRxEnable;		 
	u8 ControlChannel;
 

 
	 
	 
	 
	 
	 
	 
	 
	 
 

 

	 
	 
	 
 
	 
	u8 SupportPlatform;
	 
	u32 SupportAbility;
	 
	u8 SupportInterface;
	 
	u32 SupportICType;
	 
	u8 CutVersion;
	 
	u8 FabVersion;
	 
	u8 RFEType;
	 
	u8 BoardType;
	u8 PackageType;
	u8 TypeGLNA;
	u8 TypeGPA;
	u8 TypeALNA;
	u8 TypeAPA;
	 
	u8 ExtLNA;
	 
	u8 ExtPA;
	 
	u8 ExtTRSW;
	u8 PatchID;  
	bool bInHctTest;
	bool bWIFITest;

	bool bDualMacSmartConcurrent;
	u32 BK_SupportAbility;
	u8 AntDivType;
 

	 
	 
	 
 

	u8 u8_temp;
	bool bool_temp;
	struct adapter *adapter_temp;

	 
	u8 *pMacPhyMode;
	 
	u64 *pNumTxBytesUnicast;
	 
	u64 *pNumRxBytesUnicast;
	 
	u8 *pwirelessmode;  
	 
	u8 *pSecChOffset;
	 
	u8 *pSecurity;
	 
	u8 *pBandWidth;
	 
	u8 *pChannel;  
	bool DPK_Done;
	 

	bool *pbGetValueFromOtherMac;
	struct adapter **pBuddyAdapter;
	bool *pbMasterOfDMSP;  
	 
	bool *pbScanInProcess;
	bool *pbPowerSaving;
	 
	u8 *pOnePathCCA;
	 
	u8 *pAntennaTest;
	bool *pbNet_closed;
	u8 *mp_mode;
	 
	u8 *pu1ForcedIgiLb;
 
	bool *pIs1Antenna;
	u8 *pRFDefaultPath;
	 

 
	u16 *pForcedDataRate;
 
	bool bLinkInProcess;
	bool bWIFI_Direct;
	bool bWIFI_Display;
	bool bLinked;

	bool bsta_state;
	u8 RSSI_Min;
	u8 InterfaceIndex;  
	bool bOneEntryOnly;
	 
	bool bBtEnabled;			 
	bool bBtConnectProcess;	 
	u8 btHsRssi;				 
	bool bBtHsOperation;		 
	bool bBtDisableEdcaTurbo;	 
	bool bBtLimitedDig;			 
 
	u8 RSSI_A;
	u8 RSSI_B;
	u64 RSSI_TRSW;
	u64 RSSI_TRSW_H;
	u64 RSSI_TRSW_L;
	u64 RSSI_TRSW_iso;

	u8 RxRate;
	bool bNoisyState;
	u8 TxRate;
	u8 LinkedInterval;
	u8 preChannel;
	u32 TxagcOffsetValueA;
	bool IsTxagcOffsetPositiveA;
	u32 TxagcOffsetValueB;
	bool IsTxagcOffsetPositiveB;
	u64	lastTxOkCnt;
	u64	lastRxOkCnt;
	u32 BbSwingOffsetA;
	bool IsBbSwingOffsetPositiveA;
	u32 BbSwingOffsetB;
	bool IsBbSwingOffsetPositiveB;
	s8 TH_L2H_ini;
	s8 TH_EDCCA_HL_diff;
	s8 IGI_Base;
	u8 IGI_target;
	bool ForceEDCCA;
	u8 AdapEn_RSSI;
	s8 Force_TH_H;
	s8 Force_TH_L;
	u8 IGI_LowerBound;
	u8 antdiv_rssi;
	u8 AntType;
	u8 pre_AntType;
	u8 antdiv_period;
	u8 antdiv_select;
	u8 NdpaPeriod;
	bool H2C_RARpt_connect;

	 
	bool adaptivity_flag;
	bool NHM_disable;
	bool TxHangFlg;
	bool Carrier_Sense_enable;
	u8 tolerance_cnt;
	u64 NHMCurTxOkcnt;
	u64 NHMCurRxOkcnt;
	u64 NHMLastTxOkcnt;
	u64 NHMLastRxOkcnt;
	u8 txEdcca1;
	u8 txEdcca0;
	s8 H2L_lb;
	s8 L2H_lb;
	u8 Adaptivity_IGI_upper;
	u8 NHM_cnt_0;

	 
	 
	 
	 
	PSTA_INFO_T pODM_StaInfo[ODM_ASSOCIATE_ENTRY_NUM];

	 
	 
	 
	 
	bool RaSupport88E;

	 

	 
	struct odm_phy_dbg_info PhyDbgInfo;
	 

	 
	struct odm_mac_status_info *pMacInfo;
	 

	 

	 
	 
	 
	 

	 
	 
	 
	struct fat_t DM_FatTable;
	struct dig_t DM_DigTable;
	struct ps_t DM_PSTable;
	struct dynamic_primary_CCA DM_PriCCA;
	struct rxhp_t dM_RXHP_Table;
	struct ra_t DM_RA_Table;
	struct false_ALARM_STATISTICS FalseAlmCnt;
	struct false_ALARM_STATISTICS FlaseAlmCntBuddyAdapter;
	struct swat_t DM_SWAT_Table;
	bool RSSI_test;
	struct cfo_tracking DM_CfoTrack;

	struct edca_t DM_EDCA_Table;
	u32 WMMEDCA_BE;
	struct pathdiv_t DM_PathDiv;
	 
	 
	 
	 

	 
	 
	 
	 
	 
	 
	 
	 
	 
	 

	bool *pbDriverStopped;
	bool *pbDriverIsGoingToPnpSetPowerSleep;
	bool *pinit_adpt_in_progress;

	 
	bool bUserAssignLevel;
	struct timer_list PSDTimer;
	u8 RSSI_BT;			 
	bool bPSDinProcess;
	bool bPSDactive;
	bool bDMInitialGainEnable;

	 
	struct timer_list MPT_DIGTimer;

	 
	u8 bUseRAMask;

	struct odm_rate_adaptive RateAdaptive;

	struct ant_detected_info AntDetectedInfo;  

	struct odm_rf_cal_t RFCalibrateInfo;

	 
	 
	 
	u8 BbSwingIdxOfdm[MAX_RF_PATH];
	u8 BbSwingIdxOfdmCurrent;
	u8 BbSwingIdxOfdmBase[MAX_RF_PATH];
	bool BbSwingFlagOfdm;
	u8 BbSwingIdxCck;
	u8 BbSwingIdxCckCurrent;
	u8 BbSwingIdxCckBase;
	u8 DefaultOfdmIndex;
	u8 DefaultCckIndex;
	bool BbSwingFlagCck;

	s8 Absolute_OFDMSwingIdx[MAX_RF_PATH];
	s8 Remnant_OFDMSwingIdx[MAX_RF_PATH];
	s8 Remnant_CCKSwingIdx;
	s8 Modify_TxAGC_Value;        
	bool Modify_TxAGC_Flag_PathA;
	bool Modify_TxAGC_Flag_PathB;
	bool Modify_TxAGC_Flag_PathC;
	bool Modify_TxAGC_Flag_PathD;
	bool Modify_TxAGC_Flag_PathA_CCK;

	s8 KfreeOffset[MAX_RF_PATH];
	 
	 
	 

	 
	struct timer_list PathDivSwitchTimer;
	 
	struct timer_list CCKPathDiversityTimer;
	struct timer_list FastAntTrainingTimer;

	 

	#if (BEAMFORMING_SUPPORT == 1)
	RT_BEAMFORMING_INFO BeamformingInfo;
	#endif
};

 enum odm_rf_content {
	odm_radioa_txt = 0x1000,
	odm_radiob_txt = 0x1001,
	odm_radioc_txt = 0x1002,
	odm_radiod_txt = 0x1003
};

enum ODM_BB_Config_Type {
	CONFIG_BB_PHY_REG,
	CONFIG_BB_AGC_TAB,
	CONFIG_BB_AGC_TAB_2G,
	CONFIG_BB_PHY_REG_PG,
	CONFIG_BB_PHY_REG_MP,
	CONFIG_BB_AGC_TAB_DIFF,
};

enum ODM_RF_Config_Type {
	CONFIG_RF_RADIO,
	CONFIG_RF_TXPWR_LMT,
};

enum ODM_FW_Config_Type {
	CONFIG_FW_NIC,
	CONFIG_FW_NIC_2,
	CONFIG_FW_AP,
	CONFIG_FW_WoWLAN,
	CONFIG_FW_WoWLAN_2,
	CONFIG_FW_AP_WoWLAN,
	CONFIG_FW_BT,
};

#ifdef REMOVE_PACK
#pragma pack()
#endif

 

 
 
 

 

 
 
 
#define          LNA_Low_Gain_1                      0x64
#define          LNA_Low_Gain_2                      0x5A
#define          LNA_Low_Gain_3                      0x58

#define          FA_RXHP_TH1                           5000
#define          FA_RXHP_TH2                           1500
#define          FA_RXHP_TH3                             800
#define          FA_RXHP_TH4                             600
#define          FA_RXHP_TH5                             500

 
 
 

 
 
 
 

 
 
 
#define		DM_RATR_STA_INIT			0
#define		DM_RATR_STA_HIGH			1
#define		DM_RATR_STA_MIDDLE			2
#define		DM_RATR_STA_LOW				3

 
 
 

enum {  
	CCA_1R = 0,
	CCA_2R = 1,
	CCA_MAX = 2,
};

enum {  
	RF_Save = 0,
	RF_Normal = 1,
	RF_MAX = 2,
};

 
#define	MAX_ANTENNA_DETECTION_CNT	10

 
 
 
extern	u32 OFDMSwingTable[OFDM_TABLE_SIZE];
extern	u8 CCKSwingTable_Ch1_Ch13[CCK_TABLE_SIZE][8];
extern	u8 CCKSwingTable_Ch14[CCK_TABLE_SIZE][8];

extern	u32 OFDMSwingTable_New[OFDM_TABLE_SIZE];
extern	u8 CCKSwingTable_Ch1_Ch13_New[CCK_TABLE_SIZE][8];
extern	u8 CCKSwingTable_Ch14_New[CCK_TABLE_SIZE][8];

extern  u32 TxScalingTable_Jaguar[TXSCALE_TABLE_SIZE];

 
 
 
#define IS_STA_VALID(pSta)		(pSta)
 
 
 
 
 
#define SWAW_STEP_PEAK		0
#define SWAW_STEP_DETERMINE	1

 

#define dm_CheckTXPowerTracking ODM_TXPowerTrackingCheck
void ODM_TXPowerTrackingCheck(struct dm_odm_t *pDM_Odm);

bool ODM_RAStateCheck(
	struct dm_odm_t *pDM_Odm,
	s32	RSSI,
	bool bForceUpdate,
	u8 *pRATRState
);

#define dm_SWAW_RSSI_Check	ODM_SwAntDivChkPerPktRssi
void ODM_SwAntDivChkPerPktRssi(
	struct dm_odm_t *pDM_Odm,
	u8 StationID,
	struct odm_phy_info *pPhyInfo
);

u32 ODM_Get_Rate_Bitmap(
	struct dm_odm_t *pDM_Odm,
	u32 macid,
	u32 ra_mask,
	u8 rssi_level
);

#if (BEAMFORMING_SUPPORT == 1)
BEAMFORMING_CAP Beamforming_GetEntryBeamCapByMacId(PMGNT_INFO pMgntInfo, u8 MacId);
#endif

void odm_TXPowerTrackingInit(struct dm_odm_t *pDM_Odm);

void ODM_DMInit(struct dm_odm_t *pDM_Odm);

void ODM_DMWatchdog(struct dm_odm_t *pDM_Odm);  

void ODM_CmnInfoInit(struct dm_odm_t *pDM_Odm, enum odm_cmninfo_e CmnInfo, u32 Value);

void ODM_CmnInfoHook(struct dm_odm_t *pDM_Odm, enum odm_cmninfo_e CmnInfo, void *pValue);

void ODM_CmnInfoPtrArrayHook(
	struct dm_odm_t *pDM_Odm,
	enum odm_cmninfo_e CmnInfo,
	u16 Index,
	void *pValue
);

void ODM_CmnInfoUpdate(struct dm_odm_t *pDM_Odm, u32 CmnInfo, u64 Value);

void ODM_InitAllTimers(struct dm_odm_t *pDM_Odm);

void ODM_CancelAllTimers(struct dm_odm_t *pDM_Odm);

void ODM_ReleaseAllTimers(struct dm_odm_t *pDM_Odm);

void ODM_AntselStatistics_88C(
	struct dm_odm_t *pDM_Odm,
	u8 MacId,
	u32 PWDBAll,
	bool isCCKrate
);

void ODM_DynamicARFBSelect(struct dm_odm_t *pDM_Odm, u8 rate, bool Collision_State);

#endif

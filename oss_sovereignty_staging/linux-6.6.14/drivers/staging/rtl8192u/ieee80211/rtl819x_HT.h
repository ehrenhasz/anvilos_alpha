 
#ifndef _RTL819XU_HTTYPE_H_
#define _RTL819XU_HTTYPE_H_

 

 
#define MIMO_PS_STATIC				0

 
#define HTCLNG	4

 
enum ht_channel_width {
	HT_CHANNEL_WIDTH_20 = 0,
	HT_CHANNEL_WIDTH_20_40 = 1,
};

 
enum ht_extension_chan_offset {
	HT_EXTCHNL_OFFSET_NO_EXT = 0,
	HT_EXTCHNL_OFFSET_UPPER = 1,
	HT_EXTCHNL_OFFSET_NO_DEF = 2,
	HT_EXTCHNL_OFFSET_LOWER = 3,
};

struct ht_capability_ele {
	
	u8	AdvCoding:1;
	u8	ChlWidth:1;
	u8	MimoPwrSave:2;
	u8	GreenField:1;
	u8	ShortGI20Mhz:1;
	u8	ShortGI40Mhz:1;
	u8	TxSTBC:1;
	u8	RxSTBC:2;
	u8	DelayBA:1;
	u8	MaxAMSDUSize:1;
	u8	DssCCk:1;
	u8	PSMP:1;
	u8	Rsvd1:1;
	u8	LSigTxopProtect:1;

	
	u8	MaxRxAMPDUFactor:2;
	u8	MPDUDensity:3;
	u8	Rsvd2:3;

	
	u8	MCS[16];

	
	u16	ExtHTCapInfo;

	
	u8	TxBFCap[4];

	
	u8	ASCap;

} __packed;

 
typedef struct _HT_INFORMATION_ELE {
	u8	ControlChl;

	u8	ExtChlOffset:2;
	u8	RecommemdedTxWidth:1;
	u8	RIFS:1;
	u8	PSMPAccessOnly:1;
	u8	SrvIntGranularity:3;

	u8	OptMode:2;
	u8	NonGFDevPresent:1;
	u8	Revd1:5;
	u8	Revd2:8;

	u8	Rsvd3:6;
	u8	DualBeacon:1;
	u8	DualCTSProtect:1;

	u8	SecondaryBeacon:1;
	u8	LSigTxopProtectFull:1;
	u8	PcoActive:1;
	u8	PcoPhase:1;
	u8	Rsvd4:4;

	u8	BasicMSC[16];
} __attribute__ ((packed)) HT_INFORMATION_ELE, *PHT_INFORMATION_ELE;

typedef enum _HT_SPEC_VER {
	HT_SPEC_VER_IEEE = 0,
	HT_SPEC_VER_EWC = 1,
} HT_SPEC_VER, *PHT_SPEC_VER;

typedef enum _HT_AGGRE_MODE_E {
	HT_AGG_AUTO = 0,
	HT_AGG_FORCE_ENABLE = 1,
	HT_AGG_FORCE_DISABLE = 2,
} HT_AGGRE_MODE_E, *PHT_AGGRE_MODE_E;

 
typedef struct _RT_HIGH_THROUGHPUT {
	u8				bEnableHT;
	u8				bCurrentHTSupport;

	u8				bRegBW40MHz;				
	u8				bCurBW40MHz;				

	u8				bRegShortGI40MHz;			
	u8				bCurShortGI40MHz;			

	u8				bRegShortGI20MHz;			
	u8				bCurShortGI20MHz;			

	u8				bRegSuppCCK;				
	u8				bCurSuppCCK;				

	
	HT_SPEC_VER			ePeerHTSpecVer;

	
	struct ht_capability_ele	SelfHTCap;		
	HT_INFORMATION_ELE	SelfHTInfo;		

	
	u8				PeerHTCapBuf[32];
	u8				PeerHTInfoBuf[32];

	
	u8				bAMSDU_Support;			
	u16				nAMSDU_MaxSize;			
	u8				bCurrent_AMSDU_Support;	
	u16				nCurrent_AMSDU_MaxSize;	

	
	u8				bAMPDUEnable;				
	u8				bCurrentAMPDUEnable;		
	u8				AMPDU_Factor;				
	u8				CurrentAMPDUFactor;		
	u8				MPDU_Density;				
	u8				CurrentMPDUDensity;			

	
	HT_AGGRE_MODE_E	ForcedAMPDUMode;
	u8				ForcedAMPDUFactor;
	u8				ForcedMPDUDensity;

	
	HT_AGGRE_MODE_E	ForcedAMSDUMode;
	u16				ForcedAMSDUMaxSize;

	u8				bForcedShortGI;

	u8				CurrentOpMode;

	
	u8				SelfMimoPs;
	u8				PeerMimoPs;

	
	enum ht_extension_chan_offset	CurSTAExtChnlOffset;
	u8				bCurTxBW40MHz;	
	u8				PeerBandwidth;

	
	u8				bSwBwInProgress;
	u8				SwBwStep;
	

	
	u8				bRegRT2RTAggregation;
	u8				bCurrentRT2RTAggregation;
	u8				bCurrentRT2RTLongSlotTime;
	u8				szRT2RTAggBuffer[10];

	
	u8				bRegRxReorderEnable;
	u8				bCurRxReorderEnable;
	u8				RxReorderWinSize;
	u8				RxReorderPendingTime;
	u16				RxReorderDropCounter;

#ifdef USB_TX_DRIVER_AGGREGATION_ENABLE
	u8				UsbTxAggrNum;
#endif
#ifdef USB_RX_AGGREGATION_SUPPORT
	u8				UsbRxFwAggrEn;
	u8				UsbRxFwAggrPageNum;
	u8				UsbRxFwAggrPacketNum;
	u8				UsbRxFwAggrTimeout;
#endif

	
	u8				bIsPeerBcm;

	
	u8				IOTPeer;
	u32				IOTAction;
} __attribute__ ((packed)) RT_HIGH_THROUGHPUT, *PRT_HIGH_THROUGHPUT;

 
typedef struct _BSS_HT {
	u8				bdSupportHT;

	
	u8					bdHTCapBuf[32];
	u16					bdHTCapLen;
	u8					bdHTInfoBuf[32];
	u16					bdHTInfoLen;

	HT_SPEC_VER				bdHTSpecVer;
	
	

	u8					bdRT2RTAggregation;
	u8					bdRT2RTLongSlotTime;
} __attribute__ ((packed)) BSS_HT, *PBSS_HT;

extern u8 MCS_FILTER_ALL[16];
extern u8 MCS_FILTER_1SS[16];

 
#define PICK_RATE(_nLegacyRate, _nMcsRate)	\
		(_nMcsRate == 0) ? (_nLegacyRate & 0x7f) : (_nMcsRate)
 
#define	LEGACY_WIRELESS_MODE	IEEE_MODE_MASK

#define CURRENT_RATE(WirelessMode, LegacyRate, HTRate)           \
		((WirelessMode & (LEGACY_WIRELESS_MODE)) != 0) ? \
			(LegacyRate) :                           \
			(PICK_RATE(LegacyRate, HTRate))


#define	RATE_ADPT_1SS_MASK		0xFF
#define	RATE_ADPT_2SS_MASK		0xF0 
#define	RATE_ADPT_MCS32_MASK		0x01

#define		IS_11N_MCS_RATE(rate)		(rate & 0x80)

typedef enum _HT_AGGRE_SIZE {
	HT_AGG_SIZE_8K = 0,
	HT_AGG_SIZE_16K = 1,
	HT_AGG_SIZE_32K = 2,
	HT_AGG_SIZE_64K = 3,
} HT_AGGRE_SIZE_E, *PHT_AGGRE_SIZE_E;

 
typedef enum _HT_IOT_PEER {
	HT_IOT_PEER_UNKNOWN = 0,
	HT_IOT_PEER_REALTEK = 1,
	HT_IOT_PEER_BROADCOM = 2,
	HT_IOT_PEER_RALINK = 3,
	HT_IOT_PEER_ATHEROS = 4,
	HT_IOT_PEER_CISCO = 5,
	HT_IOT_PEER_MAX = 6
} HT_IOT_PEER_E, *PHTIOT_PEER_E;

 
typedef enum _HT_IOT_ACTION {
	HT_IOT_ACT_TX_USE_AMSDU_4K = 0x00000001,
	HT_IOT_ACT_TX_USE_AMSDU_8K = 0x00000002,
	HT_IOT_ACT_DISABLE_MCS14 = 0x00000004,
	HT_IOT_ACT_DISABLE_MCS15 = 0x00000008,
	HT_IOT_ACT_DISABLE_ALL_2SS = 0x00000010,
	HT_IOT_ACT_DISABLE_EDCA_TURBO = 0x00000020,
	HT_IOT_ACT_MGNT_USE_CCK_6M = 0x00000040,
	HT_IOT_ACT_CDD_FSYNC = 0x00000080,
	HT_IOT_ACT_PURE_N_MODE = 0x00000100,
	HT_IOT_ACT_FORCED_CTS2SELF = 0x00000200,
} HT_IOT_ACTION_E, *PHT_IOT_ACTION_E;

#endif 

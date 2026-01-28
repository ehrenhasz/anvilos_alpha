#ifndef	__RTW_RF_H_
#define __RTW_RF_H_
#define OFDM_PHY		1
#define MIXED_PHY		2
#define CCK_PHY			3
#define NumRates		13
#define SHORT_SLOT_TIME		9
#define NON_SHORT_SLOT_TIME	20
#define RTL8711_RF_MAX_SENS	 6
#define RTL8711_RF_DEF_SENS	 4
#define	MAX_CHANNEL_NUM_2G	14
#define	MAX_CHANNEL_NUM		14
#define NUM_REGULATORYS	1
#define USA			0x555320
#define EUROPE			0x1  
#define JAPAN			0x2  
struct	regulatory_class {
	u32 starting_freq;			 
	u8 channel_set[MAX_CHANNEL_NUM];
	u8 channel_cck_power[MAX_CHANNEL_NUM]; 
	u8 channel_ofdm_power[MAX_CHANNEL_NUM]; 
	u8 txpower_limit;			 
	u8 channel_spacing;			 
	u8 modem;
};
enum {
	cESS			= 0x0001,
	cIBSS			= 0x0002,
	cPollable		= 0x0004,
	cPollReq		= 0x0008,
	cPrivacy		= 0x0010,
	cShortPreamble		= 0x0020,
	cPBCC			= 0x0040,
	cChannelAgility		= 0x0080,
	cSpectrumMgnt		= 0x0100,
	cQos			= 0x0200,	 
	cShortSlotTime		= 0x0400,
	cAPSD			= 0x0800,
	cRM			= 0x1000,	 
	cDSSS_OFDM		= 0x2000,
	cDelayedBA		= 0x4000,
	cImmediateBA		= 0x8000,
};
enum {
	PREAMBLE_LONG	= 1,
	PREAMBLE_AUTO	= 2,
	PREAMBLE_SHORT	= 3,
};
#define HAL_PRIME_CHNL_OFFSET_DONT_CARE	0
#define HAL_PRIME_CHNL_OFFSET_LOWER	1
#define HAL_PRIME_CHNL_OFFSET_UPPER	2
enum channel_width {
	CHANNEL_WIDTH_20 = 0,
	CHANNEL_WIDTH_40 = 1,
};
enum extchnl_offset {
	EXTCHNL_OFFSET_NO_EXT = 0,
	EXTCHNL_OFFSET_UPPER = 1,
	EXTCHNL_OFFSET_NO_DEF = 2,
	EXTCHNL_OFFSET_LOWER = 3,
};
enum {
	HT_DATA_SC_DONOT_CARE = 0,
	HT_DATA_SC_20_UPPER_OF_40MHZ = 1,
	HT_DATA_SC_20_LOWER_OF_40MHZ = 2,
};
u32 rtw_ch2freq(u32 ch);
#endif  

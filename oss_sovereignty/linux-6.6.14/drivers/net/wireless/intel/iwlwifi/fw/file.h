 
 
#ifndef __iwl_fw_file_h__
#define __iwl_fw_file_h__

#include <linux/netdevice.h>
#include <linux/nl80211.h>

 
struct iwl_ucode_header {
	__le32 ver;	 
	union {
		struct {
			__le32 inst_size;	 
			__le32 data_size;	 
			__le32 init_size;	 
			__le32 init_data_size;	 
			__le32 boot_size;	 
			u8 data[0];		 
		} v1;
		struct {
			__le32 build;		 
			__le32 inst_size;	 
			__le32 data_size;	 
			__le32 init_size;	 
			__le32 init_data_size;	 
			__le32 boot_size;	 
			u8 data[0];		 
		} v2;
	} u;
};

#define IWL_UCODE_TLV_DEBUG_BASE	0x1000005
#define IWL_UCODE_TLV_CONST_BASE	0x100

 

enum iwl_ucode_tlv_type {
	IWL_UCODE_TLV_INVALID		= 0,  
	IWL_UCODE_TLV_INST		= 1,
	IWL_UCODE_TLV_DATA		= 2,
	IWL_UCODE_TLV_INIT		= 3,
	IWL_UCODE_TLV_INIT_DATA		= 4,
	IWL_UCODE_TLV_BOOT		= 5,
	IWL_UCODE_TLV_PROBE_MAX_LEN	= 6,  
	IWL_UCODE_TLV_PAN		= 7,  
	IWL_UCODE_TLV_MEM_DESC		= 7,  
	IWL_UCODE_TLV_RUNT_EVTLOG_PTR	= 8,
	IWL_UCODE_TLV_RUNT_EVTLOG_SIZE	= 9,
	IWL_UCODE_TLV_RUNT_ERRLOG_PTR	= 10,
	IWL_UCODE_TLV_INIT_EVTLOG_PTR	= 11,
	IWL_UCODE_TLV_INIT_EVTLOG_SIZE	= 12,
	IWL_UCODE_TLV_INIT_ERRLOG_PTR	= 13,
	IWL_UCODE_TLV_ENHANCE_SENS_TBL	= 14,
	IWL_UCODE_TLV_PHY_CALIBRATION_SIZE = 15,
	IWL_UCODE_TLV_WOWLAN_INST	= 16,
	IWL_UCODE_TLV_WOWLAN_DATA	= 17,
	IWL_UCODE_TLV_FLAGS		= 18,
	IWL_UCODE_TLV_SEC_RT		= 19,
	IWL_UCODE_TLV_SEC_INIT		= 20,
	IWL_UCODE_TLV_SEC_WOWLAN	= 21,
	IWL_UCODE_TLV_DEF_CALIB		= 22,
	IWL_UCODE_TLV_PHY_SKU		= 23,
	IWL_UCODE_TLV_SECURE_SEC_RT	= 24,
	IWL_UCODE_TLV_SECURE_SEC_INIT	= 25,
	IWL_UCODE_TLV_SECURE_SEC_WOWLAN	= 26,
	IWL_UCODE_TLV_NUM_OF_CPU	= 27,
	IWL_UCODE_TLV_CSCHEME		= 28,
	IWL_UCODE_TLV_API_CHANGES_SET	= 29,
	IWL_UCODE_TLV_ENABLED_CAPABILITIES	= 30,
	IWL_UCODE_TLV_N_SCAN_CHANNELS		= 31,
	IWL_UCODE_TLV_PAGING		= 32,
	IWL_UCODE_TLV_SEC_RT_USNIFFER	= 34,
	 
	IWL_UCODE_TLV_FW_VERSION	= 36,
	IWL_UCODE_TLV_FW_DBG_DEST	= 38,
	IWL_UCODE_TLV_FW_DBG_CONF	= 39,
	IWL_UCODE_TLV_FW_DBG_TRIGGER	= 40,
	IWL_UCODE_TLV_CMD_VERSIONS	= 48,
	IWL_UCODE_TLV_FW_GSCAN_CAPA	= 50,
	IWL_UCODE_TLV_FW_MEM_SEG	= 51,
	IWL_UCODE_TLV_IML		= 52,
	IWL_UCODE_TLV_UMAC_DEBUG_ADDRS	= 54,
	IWL_UCODE_TLV_LMAC_DEBUG_ADDRS	= 55,
	IWL_UCODE_TLV_FW_RECOVERY_INFO	= 57,
	IWL_UCODE_TLV_HW_TYPE			= 58,
	IWL_UCODE_TLV_FW_FSEQ_VERSION		= 60,
	IWL_UCODE_TLV_PHY_INTEGRATION_VERSION	= 61,

	IWL_UCODE_TLV_PNVM_VERSION		= 62,
	IWL_UCODE_TLV_PNVM_SKU			= 64,

	IWL_UCODE_TLV_SEC_TABLE_ADDR		= 66,
	IWL_UCODE_TLV_D3_KEK_KCK_ADDR		= 67,
	IWL_UCODE_TLV_CURRENT_PC		= 68,

	IWL_UCODE_TLV_FW_NUM_STATIONS		= IWL_UCODE_TLV_CONST_BASE + 0,
	IWL_UCODE_TLV_FW_NUM_BEACONS		= IWL_UCODE_TLV_CONST_BASE + 2,

	IWL_UCODE_TLV_TYPE_DEBUG_INFO		= IWL_UCODE_TLV_DEBUG_BASE + 0,
	IWL_UCODE_TLV_TYPE_BUFFER_ALLOCATION	= IWL_UCODE_TLV_DEBUG_BASE + 1,
	IWL_UCODE_TLV_TYPE_HCMD			= IWL_UCODE_TLV_DEBUG_BASE + 2,
	IWL_UCODE_TLV_TYPE_REGIONS		= IWL_UCODE_TLV_DEBUG_BASE + 3,
	IWL_UCODE_TLV_TYPE_TRIGGERS		= IWL_UCODE_TLV_DEBUG_BASE + 4,
	IWL_UCODE_TLV_TYPE_CONF_SET		= IWL_UCODE_TLV_DEBUG_BASE + 5,
	IWL_UCODE_TLV_DEBUG_MAX = IWL_UCODE_TLV_TYPE_TRIGGERS,

	 
	IWL_UCODE_TLV_FW_DBG_DUMP_LST	= 0x1000,
};

struct iwl_ucode_tlv {
	__le32 type;		 
	__le32 length;		 
	u8 data[];
};

#define IWL_TLV_UCODE_MAGIC		0x0a4c5749
#define FW_VER_HUMAN_READABLE_SZ	64

struct iwl_tlv_ucode_header {
	 
	__le32 zero;
	__le32 magic;
	u8 human_readable[FW_VER_HUMAN_READABLE_SZ];
	 
	__le32 ver;
	__le32 build;
	__le64 ignore;
	 
	u8 data[];
};

 
struct iwl_ucode_api {
	__le32 api_index;
	__le32 api_flags;
} __packed;

struct iwl_ucode_capa {
	__le32 api_index;
	__le32 api_capa;
} __packed;

 
enum iwl_ucode_tlv_flag {
	IWL_UCODE_TLV_FLAGS_PAN			= BIT(0),
	IWL_UCODE_TLV_FLAGS_NEWSCAN		= BIT(1),
	IWL_UCODE_TLV_FLAGS_MFP			= BIT(2),
	IWL_UCODE_TLV_FLAGS_SHORT_BL		= BIT(7),
	IWL_UCODE_TLV_FLAGS_D3_6_IPV6_ADDRS	= BIT(10),
	IWL_UCODE_TLV_FLAGS_NO_BASIC_SSID	= BIT(12),
	IWL_UCODE_TLV_FLAGS_NEW_NSOFFL_SMALL	= BIT(15),
	IWL_UCODE_TLV_FLAGS_NEW_NSOFFL_LARGE	= BIT(16),
	IWL_UCODE_TLV_FLAGS_UAPSD_SUPPORT	= BIT(24),
	IWL_UCODE_TLV_FLAGS_EBS_SUPPORT		= BIT(25),
	IWL_UCODE_TLV_FLAGS_P2P_PS_UAPSD	= BIT(26),
};

typedef unsigned int __bitwise iwl_ucode_tlv_api_t;

 
enum iwl_ucode_tlv_api {
	 
	IWL_UCODE_TLV_API_FRAGMENTED_SCAN	= (__force iwl_ucode_tlv_api_t)8,
	IWL_UCODE_TLV_API_WIFI_MCC_UPDATE	= (__force iwl_ucode_tlv_api_t)9,
	IWL_UCODE_TLV_API_LQ_SS_PARAMS		= (__force iwl_ucode_tlv_api_t)18,
	IWL_UCODE_TLV_API_NEW_VERSION		= (__force iwl_ucode_tlv_api_t)20,
	IWL_UCODE_TLV_API_SCAN_TSF_REPORT	= (__force iwl_ucode_tlv_api_t)28,
	IWL_UCODE_TLV_API_TKIP_MIC_KEYS		= (__force iwl_ucode_tlv_api_t)29,
	IWL_UCODE_TLV_API_STA_TYPE		= (__force iwl_ucode_tlv_api_t)30,
	IWL_UCODE_TLV_API_NAN2_VER2		= (__force iwl_ucode_tlv_api_t)31,
	 
	IWL_UCODE_TLV_API_ADAPTIVE_DWELL	= (__force iwl_ucode_tlv_api_t)32,
	IWL_UCODE_TLV_API_OCE			= (__force iwl_ucode_tlv_api_t)33,
	IWL_UCODE_TLV_API_NEW_BEACON_TEMPLATE	= (__force iwl_ucode_tlv_api_t)34,
	IWL_UCODE_TLV_API_NEW_RX_STATS		= (__force iwl_ucode_tlv_api_t)35,
	IWL_UCODE_TLV_API_WOWLAN_KEY_MATERIAL	= (__force iwl_ucode_tlv_api_t)36,
	IWL_UCODE_TLV_API_QUOTA_LOW_LATENCY	= (__force iwl_ucode_tlv_api_t)38,
	IWL_UCODE_TLV_API_DEPRECATE_TTAK	= (__force iwl_ucode_tlv_api_t)41,
	IWL_UCODE_TLV_API_ADAPTIVE_DWELL_V2	= (__force iwl_ucode_tlv_api_t)42,
	IWL_UCODE_TLV_API_FRAG_EBS		= (__force iwl_ucode_tlv_api_t)44,
	IWL_UCODE_TLV_API_REDUCE_TX_POWER	= (__force iwl_ucode_tlv_api_t)45,
	IWL_UCODE_TLV_API_SHORT_BEACON_NOTIF	= (__force iwl_ucode_tlv_api_t)46,
	IWL_UCODE_TLV_API_BEACON_FILTER_V4      = (__force iwl_ucode_tlv_api_t)47,
	IWL_UCODE_TLV_API_REGULATORY_NVM_INFO   = (__force iwl_ucode_tlv_api_t)48,
	IWL_UCODE_TLV_API_FTM_NEW_RANGE_REQ     = (__force iwl_ucode_tlv_api_t)49,
	IWL_UCODE_TLV_API_SCAN_OFFLOAD_CHANS    = (__force iwl_ucode_tlv_api_t)50,
	IWL_UCODE_TLV_API_MBSSID_HE		= (__force iwl_ucode_tlv_api_t)52,
	IWL_UCODE_TLV_API_WOWLAN_TCP_SYN_WAKE	= (__force iwl_ucode_tlv_api_t)53,
	IWL_UCODE_TLV_API_FTM_RTT_ACCURACY      = (__force iwl_ucode_tlv_api_t)54,
	IWL_UCODE_TLV_API_SAR_TABLE_VER         = (__force iwl_ucode_tlv_api_t)55,
	IWL_UCODE_TLV_API_REDUCED_SCAN_CONFIG   = (__force iwl_ucode_tlv_api_t)56,
	IWL_UCODE_TLV_API_ADWELL_HB_DEF_N_AP	= (__force iwl_ucode_tlv_api_t)57,
	IWL_UCODE_TLV_API_SCAN_EXT_CHAN_VER	= (__force iwl_ucode_tlv_api_t)58,
	IWL_UCODE_TLV_API_BAND_IN_RX_DATA	= (__force iwl_ucode_tlv_api_t)59,


#ifdef __CHECKER__
	 
#define NUM_IWL_UCODE_TLV_API 128
#else
	NUM_IWL_UCODE_TLV_API
#endif
};

typedef unsigned int __bitwise iwl_ucode_tlv_capa_t;

 
enum iwl_ucode_tlv_capa {
	 
	IWL_UCODE_TLV_CAPA_D0I3_SUPPORT			= (__force iwl_ucode_tlv_capa_t)0,
	IWL_UCODE_TLV_CAPA_LAR_SUPPORT			= (__force iwl_ucode_tlv_capa_t)1,
	IWL_UCODE_TLV_CAPA_UMAC_SCAN			= (__force iwl_ucode_tlv_capa_t)2,
	IWL_UCODE_TLV_CAPA_BEAMFORMER			= (__force iwl_ucode_tlv_capa_t)3,
	IWL_UCODE_TLV_CAPA_TDLS_SUPPORT			= (__force iwl_ucode_tlv_capa_t)6,
	IWL_UCODE_TLV_CAPA_TXPOWER_INSERTION_SUPPORT	= (__force iwl_ucode_tlv_capa_t)8,
	IWL_UCODE_TLV_CAPA_DS_PARAM_SET_IE_SUPPORT	= (__force iwl_ucode_tlv_capa_t)9,
	IWL_UCODE_TLV_CAPA_WFA_TPC_REP_IE_SUPPORT	= (__force iwl_ucode_tlv_capa_t)10,
	IWL_UCODE_TLV_CAPA_QUIET_PERIOD_SUPPORT		= (__force iwl_ucode_tlv_capa_t)11,
	IWL_UCODE_TLV_CAPA_DQA_SUPPORT			= (__force iwl_ucode_tlv_capa_t)12,
	IWL_UCODE_TLV_CAPA_TDLS_CHANNEL_SWITCH		= (__force iwl_ucode_tlv_capa_t)13,
	IWL_UCODE_TLV_CAPA_CNSLDTD_D3_D0_IMG		= (__force iwl_ucode_tlv_capa_t)17,
	IWL_UCODE_TLV_CAPA_HOTSPOT_SUPPORT		= (__force iwl_ucode_tlv_capa_t)18,
	IWL_UCODE_TLV_CAPA_CSUM_SUPPORT			= (__force iwl_ucode_tlv_capa_t)21,
	IWL_UCODE_TLV_CAPA_RADIO_BEACON_STATS		= (__force iwl_ucode_tlv_capa_t)22,
	IWL_UCODE_TLV_CAPA_P2P_SCM_UAPSD		= (__force iwl_ucode_tlv_capa_t)26,
	IWL_UCODE_TLV_CAPA_BT_COEX_PLCR			= (__force iwl_ucode_tlv_capa_t)28,
	IWL_UCODE_TLV_CAPA_LAR_MULTI_MCC		= (__force iwl_ucode_tlv_capa_t)29,
	IWL_UCODE_TLV_CAPA_BT_COEX_RRC			= (__force iwl_ucode_tlv_capa_t)30,
	IWL_UCODE_TLV_CAPA_GSCAN_SUPPORT		= (__force iwl_ucode_tlv_capa_t)31,

	 
	IWL_UCODE_TLV_CAPA_FRAGMENTED_PNVM_IMG		= (__force iwl_ucode_tlv_capa_t)32,
	IWL_UCODE_TLV_CAPA_SOC_LATENCY_SUPPORT		= (__force iwl_ucode_tlv_capa_t)37,
	IWL_UCODE_TLV_CAPA_STA_PM_NOTIF			= (__force iwl_ucode_tlv_capa_t)38,
	IWL_UCODE_TLV_CAPA_BINDING_CDB_SUPPORT		= (__force iwl_ucode_tlv_capa_t)39,
	IWL_UCODE_TLV_CAPA_CDB_SUPPORT			= (__force iwl_ucode_tlv_capa_t)40,
	IWL_UCODE_TLV_CAPA_D0I3_END_FIRST		= (__force iwl_ucode_tlv_capa_t)41,
	IWL_UCODE_TLV_CAPA_TLC_OFFLOAD                  = (__force iwl_ucode_tlv_capa_t)43,
	IWL_UCODE_TLV_CAPA_DYNAMIC_QUOTA                = (__force iwl_ucode_tlv_capa_t)44,
	IWL_UCODE_TLV_CAPA_COEX_SCHEMA_2		= (__force iwl_ucode_tlv_capa_t)45,
	IWL_UCODE_TLV_CAPA_CHANNEL_SWITCH_CMD		= (__force iwl_ucode_tlv_capa_t)46,
	IWL_UCODE_TLV_CAPA_FTM_CALIBRATED		= (__force iwl_ucode_tlv_capa_t)47,
	IWL_UCODE_TLV_CAPA_ULTRA_HB_CHANNELS		= (__force iwl_ucode_tlv_capa_t)48,
	IWL_UCODE_TLV_CAPA_CS_MODIFY			= (__force iwl_ucode_tlv_capa_t)49,
	IWL_UCODE_TLV_CAPA_SET_LTR_GEN2			= (__force iwl_ucode_tlv_capa_t)50,
	IWL_UCODE_TLV_CAPA_SET_PPAG			= (__force iwl_ucode_tlv_capa_t)52,
	IWL_UCODE_TLV_CAPA_TAS_CFG			= (__force iwl_ucode_tlv_capa_t)53,
	IWL_UCODE_TLV_CAPA_SESSION_PROT_CMD		= (__force iwl_ucode_tlv_capa_t)54,
	IWL_UCODE_TLV_CAPA_PROTECTED_TWT		= (__force iwl_ucode_tlv_capa_t)56,
	IWL_UCODE_TLV_CAPA_FW_RESET_HANDSHAKE		= (__force iwl_ucode_tlv_capa_t)57,
	IWL_UCODE_TLV_CAPA_PASSIVE_6GHZ_SCAN		= (__force iwl_ucode_tlv_capa_t)58,
	IWL_UCODE_TLV_CAPA_HIDDEN_6GHZ_SCAN		= (__force iwl_ucode_tlv_capa_t)59,
	IWL_UCODE_TLV_CAPA_BROADCAST_TWT		= (__force iwl_ucode_tlv_capa_t)60,
	IWL_UCODE_TLV_CAPA_COEX_HIGH_PRIO		= (__force iwl_ucode_tlv_capa_t)61,
	IWL_UCODE_TLV_CAPA_RFIM_SUPPORT			= (__force iwl_ucode_tlv_capa_t)62,
	IWL_UCODE_TLV_CAPA_BAID_ML_SUPPORT		= (__force iwl_ucode_tlv_capa_t)63,

	 
	IWL_UCODE_TLV_CAPA_EXTENDED_DTS_MEASURE		= (__force iwl_ucode_tlv_capa_t)64,
	IWL_UCODE_TLV_CAPA_SHORT_PM_TIMEOUTS		= (__force iwl_ucode_tlv_capa_t)65,
	IWL_UCODE_TLV_CAPA_BT_MPLUT_SUPPORT		= (__force iwl_ucode_tlv_capa_t)67,
	IWL_UCODE_TLV_CAPA_MULTI_QUEUE_RX_SUPPORT	= (__force iwl_ucode_tlv_capa_t)68,
	IWL_UCODE_TLV_CAPA_CSA_AND_TBTT_OFFLOAD		= (__force iwl_ucode_tlv_capa_t)70,
	IWL_UCODE_TLV_CAPA_BEACON_ANT_SELECTION		= (__force iwl_ucode_tlv_capa_t)71,
	IWL_UCODE_TLV_CAPA_BEACON_STORING		= (__force iwl_ucode_tlv_capa_t)72,
	IWL_UCODE_TLV_CAPA_LAR_SUPPORT_V3		= (__force iwl_ucode_tlv_capa_t)73,
	IWL_UCODE_TLV_CAPA_CT_KILL_BY_FW		= (__force iwl_ucode_tlv_capa_t)74,
	IWL_UCODE_TLV_CAPA_TEMP_THS_REPORT_SUPPORT	= (__force iwl_ucode_tlv_capa_t)75,
	IWL_UCODE_TLV_CAPA_CTDP_SUPPORT			= (__force iwl_ucode_tlv_capa_t)76,
	IWL_UCODE_TLV_CAPA_USNIFFER_UNIFIED		= (__force iwl_ucode_tlv_capa_t)77,
	IWL_UCODE_TLV_CAPA_EXTEND_SHARED_MEM_CFG	= (__force iwl_ucode_tlv_capa_t)80,
	IWL_UCODE_TLV_CAPA_LQM_SUPPORT			= (__force iwl_ucode_tlv_capa_t)81,
	IWL_UCODE_TLV_CAPA_TX_POWER_ACK			= (__force iwl_ucode_tlv_capa_t)84,
	IWL_UCODE_TLV_CAPA_D3_DEBUG			= (__force iwl_ucode_tlv_capa_t)87,
	IWL_UCODE_TLV_CAPA_LED_CMD_SUPPORT		= (__force iwl_ucode_tlv_capa_t)88,
	IWL_UCODE_TLV_CAPA_MCC_UPDATE_11AX_SUPPORT	= (__force iwl_ucode_tlv_capa_t)89,
	IWL_UCODE_TLV_CAPA_CSI_REPORTING		= (__force iwl_ucode_tlv_capa_t)90,
	IWL_UCODE_TLV_CAPA_DBG_SUSPEND_RESUME_CMD_SUPP	= (__force iwl_ucode_tlv_capa_t)92,
	IWL_UCODE_TLV_CAPA_DBG_BUF_ALLOC_CMD_SUPP	= (__force iwl_ucode_tlv_capa_t)93,

	 
	IWL_UCODE_TLV_CAPA_MLME_OFFLOAD			= (__force iwl_ucode_tlv_capa_t)96,

	 
	IWL_UCODE_TLV_CAPA_PSC_CHAN_SUPPORT		= (__force iwl_ucode_tlv_capa_t)98,

	IWL_UCODE_TLV_CAPA_BIGTK_SUPPORT		= (__force iwl_ucode_tlv_capa_t)100,
	IWL_UCODE_TLV_CAPA_DRAM_FRAG_SUPPORT		= (__force iwl_ucode_tlv_capa_t)104,
	IWL_UCODE_TLV_CAPA_DUMP_COMPLETE_SUPPORT	= (__force iwl_ucode_tlv_capa_t)105,
	IWL_UCODE_TLV_CAPA_SYNCED_TIME			= (__force iwl_ucode_tlv_capa_t)106,
	IWL_UCODE_TLV_CAPA_TIME_SYNC_BOTH_FTM_TM        = (__force iwl_ucode_tlv_capa_t)108,
	IWL_UCODE_TLV_CAPA_BIGTK_TX_SUPPORT		= (__force iwl_ucode_tlv_capa_t)109,
	IWL_UCODE_TLV_CAPA_MLD_API_SUPPORT		= (__force iwl_ucode_tlv_capa_t)110,
	IWL_UCODE_TLV_CAPA_SCAN_DONT_TOGGLE_ANT         = (__force iwl_ucode_tlv_capa_t)111,
	IWL_UCODE_TLV_CAPA_PPAG_CHINA_BIOS_SUPPORT	= (__force iwl_ucode_tlv_capa_t)112,
	IWL_UCODE_TLV_CAPA_OFFLOAD_BTM_SUPPORT		= (__force iwl_ucode_tlv_capa_t)113,
	IWL_UCODE_TLV_CAPA_STA_EXP_MFP_SUPPORT		= (__force iwl_ucode_tlv_capa_t)114,
	IWL_UCODE_TLV_CAPA_SNIFF_VALIDATE_SUPPORT	= (__force iwl_ucode_tlv_capa_t)116,

#ifdef __CHECKER__
	 
#define NUM_IWL_UCODE_TLV_CAPA 128
#else
	NUM_IWL_UCODE_TLV_CAPA
#endif
};

 
#define IWL_DEFAULT_STANDARD_PHY_CALIBRATE_TBL_SIZE	18
#define IWL_MAX_STANDARD_PHY_CALIBRATE_TBL_SIZE		19
#define IWL_MAX_PHY_CALIBRATE_TBL_SIZE			253

 
#define IWL_DEFAULT_MAX_PROBE_LENGTH	200

 
#define CPU1_CPU2_SEPARATOR_SECTION	0xFFFFCCCC
#define PAGING_SEPARATOR_SECTION	0xAAAABBBB

 
#define IWL_UCODE_MAJOR(ver)	(((ver) & 0xFF000000) >> 24)
#define IWL_UCODE_MINOR(ver)	(((ver) & 0x00FF0000) >> 16)
#define IWL_UCODE_API(ver)	(((ver) & 0x0000FF00) >> 8)
#define IWL_UCODE_SERIAL(ver)	((ver) & 0x000000FF)

 
struct iwl_tlv_calib_ctrl {
	__le32 flow_trigger;
	__le32 event_trigger;
} __packed;

enum iwl_fw_phy_cfg {
	FW_PHY_CFG_RADIO_TYPE_POS = 0,
	FW_PHY_CFG_RADIO_TYPE = 0x3 << FW_PHY_CFG_RADIO_TYPE_POS,
	FW_PHY_CFG_RADIO_STEP_POS = 2,
	FW_PHY_CFG_RADIO_STEP = 0x3 << FW_PHY_CFG_RADIO_STEP_POS,
	FW_PHY_CFG_RADIO_DASH_POS = 4,
	FW_PHY_CFG_RADIO_DASH = 0x3 << FW_PHY_CFG_RADIO_DASH_POS,
	FW_PHY_CFG_TX_CHAIN_POS = 16,
	FW_PHY_CFG_TX_CHAIN = 0xf << FW_PHY_CFG_TX_CHAIN_POS,
	FW_PHY_CFG_RX_CHAIN_POS = 20,
	FW_PHY_CFG_RX_CHAIN = 0xf << FW_PHY_CFG_RX_CHAIN_POS,
	FW_PHY_CFG_CHAIN_SAD_POS = 23,
	FW_PHY_CFG_CHAIN_SAD_ENABLED = 0x1 << FW_PHY_CFG_CHAIN_SAD_POS,
	FW_PHY_CFG_CHAIN_SAD_ANT_A = 0x2 << FW_PHY_CFG_CHAIN_SAD_POS,
	FW_PHY_CFG_CHAIN_SAD_ANT_B = 0x4 << FW_PHY_CFG_CHAIN_SAD_POS,
	FW_PHY_CFG_SHARED_CLK = BIT(31),
};

enum iwl_fw_dbg_reg_operator {
	CSR_ASSIGN,
	CSR_SETBIT,
	CSR_CLEARBIT,

	PRPH_ASSIGN,
	PRPH_SETBIT,
	PRPH_CLEARBIT,

	INDIRECT_ASSIGN,
	INDIRECT_SETBIT,
	INDIRECT_CLEARBIT,

	PRPH_BLOCKBIT,
};

 
struct iwl_fw_dbg_reg_op {
	u8 op;
	u8 reserved[3];
	__le32 addr;
	__le32 val;
} __packed;

 
enum iwl_fw_dbg_monitor_mode {
	SMEM_MODE = 0,
	EXTERNAL_MODE = 1,
	MARBH_MODE = 2,
	MIPI_MODE = 3,
};

 
struct iwl_fw_dbg_mem_seg_tlv {
	__le32 data_type;
	__le32 ofs;
	__le32 len;
} __packed;

 
struct iwl_fw_dbg_dest_tlv_v1 {
	u8 version;
	u8 monitor_mode;
	u8 size_power;
	u8 reserved;
	__le32 base_reg;
	__le32 end_reg;
	__le32 write_ptr_reg;
	__le32 wrap_count;
	u8 base_shift;
	u8 end_shift;
	struct iwl_fw_dbg_reg_op reg_ops[];
} __packed;

 
#define IWL_LDBG_M2S_BUF_SIZE_MSK	0x0fff0000
 
#define IWL_LDBG_M2S_BUF_BA_MSK		0x00000fff
 
#define IWL_M2S_UNIT_SIZE			0x100

struct iwl_fw_dbg_dest_tlv {
	u8 version;
	u8 monitor_mode;
	u8 size_power;
	u8 reserved;
	__le32 cfg_reg;
	__le32 write_ptr_reg;
	__le32 wrap_count;
	u8 base_shift;
	u8 size_shift;
	struct iwl_fw_dbg_reg_op reg_ops[];
} __packed;

struct iwl_fw_dbg_conf_hcmd {
	u8 id;
	u8 reserved;
	__le16 len;
	u8 data[];
} __packed;

 
enum iwl_fw_dbg_trigger_mode {
	IWL_FW_DBG_TRIGGER_START = BIT(0),
	IWL_FW_DBG_TRIGGER_STOP = BIT(1),
	IWL_FW_DBG_TRIGGER_MONITOR_ONLY = BIT(2),
};

 
enum iwl_fw_dbg_trigger_flags {
	IWL_FW_DBG_FORCE_RESTART = BIT(0),
};

 
enum iwl_fw_dbg_trigger_vif_type {
	IWL_FW_DBG_CONF_VIF_ANY = NL80211_IFTYPE_UNSPECIFIED,
	IWL_FW_DBG_CONF_VIF_IBSS = NL80211_IFTYPE_ADHOC,
	IWL_FW_DBG_CONF_VIF_STATION = NL80211_IFTYPE_STATION,
	IWL_FW_DBG_CONF_VIF_AP = NL80211_IFTYPE_AP,
	IWL_FW_DBG_CONF_VIF_P2P_CLIENT = NL80211_IFTYPE_P2P_CLIENT,
	IWL_FW_DBG_CONF_VIF_P2P_GO = NL80211_IFTYPE_P2P_GO,
	IWL_FW_DBG_CONF_VIF_P2P_DEVICE = NL80211_IFTYPE_P2P_DEVICE,
};

 
struct iwl_fw_dbg_trigger_tlv {
	__le32 id;
	__le32 vif_type;
	__le32 stop_conf_ids;
	__le32 stop_delay;
	u8 mode;
	u8 start_conf_id;
	__le16 occurrences;
	__le16 trig_dis_ms;
	u8 flags;
	u8 reserved[5];

	u8 data[];
} __packed;

#define FW_DBG_START_FROM_ALIVE	0
#define FW_DBG_CONF_MAX		32
#define FW_DBG_INVALID		0xff

 
struct iwl_fw_dbg_trigger_missed_bcon {
	__le32 stop_consec_missed_bcon;
	__le32 stop_consec_missed_bcon_since_rx;
	__le32 reserved2[2];
	__le32 start_consec_missed_bcon;
	__le32 start_consec_missed_bcon_since_rx;
	__le32 reserved1[2];
} __packed;

 
struct iwl_fw_dbg_trigger_cmd {
	struct cmd {
		u8 cmd_id;
		u8 group_id;
	} __packed cmds[16];
} __packed;

 
struct iwl_fw_dbg_trigger_stats {
	__le32 stop_offset;
	__le32 stop_threshold;
	__le32 start_offset;
	__le32 start_threshold;
} __packed;

 
struct iwl_fw_dbg_trigger_low_rssi {
	__le32 rssi;
} __packed;

 
struct iwl_fw_dbg_trigger_mlme {
	u8 stop_auth_denied;
	u8 stop_auth_timeout;
	u8 stop_rx_deauth;
	u8 stop_tx_deauth;

	u8 stop_assoc_denied;
	u8 stop_assoc_timeout;
	u8 stop_connection_loss;
	u8 reserved;

	u8 start_auth_denied;
	u8 start_auth_timeout;
	u8 start_rx_deauth;
	u8 start_tx_deauth;

	u8 start_assoc_denied;
	u8 start_assoc_timeout;
	u8 start_connection_loss;
	u8 reserved2;
} __packed;

 
struct iwl_fw_dbg_trigger_txq_timer {
	__le32 command_queue;
	__le32 bss;
	__le32 softap;
	__le32 p2p_go;
	__le32 p2p_client;
	__le32 p2p_device;
	__le32 ibss;
	__le32 tdls;
	__le32 reserved[4];
} __packed;

 
struct iwl_fw_dbg_trigger_time_event {
	struct {
		__le32 id;
		__le32 action_bitmap;
		__le32 status_bitmap;
	} __packed time_events[16];
} __packed;

 
struct iwl_fw_dbg_trigger_ba {
	__le16 rx_ba_start;
	__le16 rx_ba_stop;
	__le16 tx_ba_start;
	__le16 tx_ba_stop;
	__le16 rx_bar;
	__le16 tx_bar;
	__le16 frame_timeout;
} __packed;

 
struct iwl_fw_dbg_trigger_tdls {
	u8 action_bitmap;
	u8 peer_mode;
	u8 peer[ETH_ALEN];
	u8 reserved[4];
} __packed;

 
struct iwl_fw_dbg_trigger_tx_status {
	struct tx_status {
		u8 status;
		u8 reserved[3];
	} __packed statuses[16];
	__le32 reserved[2];
} __packed;

 
struct iwl_fw_dbg_conf_tlv {
	u8 id;
	u8 usniffer;
	u8 reserved;
	u8 num_of_hcmds;
	struct iwl_fw_dbg_conf_hcmd hcmd;
} __packed;

#define IWL_FW_CMD_VER_UNKNOWN 99

 
struct iwl_fw_cmd_version {
	u8 cmd;
	u8 group;
	u8 cmd_ver;
	u8 notif_ver;
} __packed;

struct iwl_fw_tcm_error_addr {
	__le32 addr;
};  

struct iwl_fw_dump_exclude {
	__le32 addr, size;
};

static inline size_t _iwl_tlv_array_len(const struct iwl_ucode_tlv *tlv,
					size_t fixed_size, size_t var_size)
{
	size_t var_len = le32_to_cpu(tlv->length) - fixed_size;

	if (WARN_ON(var_len % var_size))
		return 0;

	return var_len / var_size;
}

#define iwl_tlv_array_len(_tlv_ptr, _struct_ptr, _memb)			\
	_iwl_tlv_array_len((_tlv_ptr), sizeof(*(_struct_ptr)),		\
			   sizeof(_struct_ptr->_memb[0]))

#endif   

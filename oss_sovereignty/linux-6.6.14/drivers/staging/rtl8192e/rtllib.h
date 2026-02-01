 
 
#ifndef RTLLIB_H
#define RTLLIB_H
#include <linux/if_ether.h>  
#include <linux/kernel.h>    
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/mutex.h>

#include <linux/delay.h>
#include <linux/wireless.h>

#include "rtllib_debug.h"
#include "rtl819x_HT.h"
#include "rtl819x_BA.h"
#include "rtl819x_TS.h"

#include <linux/netdevice.h>
#include <linux/if_arp.h>  
#include <net/cfg80211.h>
#include <net/lib80211.h>

#define MAX_PRECMD_CNT 16
#define MAX_RFDEPENDCMD_CNT 16
#define MAX_POSTCMD_CNT 16

#ifndef WIRELESS_SPY
#define WIRELESS_SPY
#endif
#include <net/iw_handler.h>

#ifndef IW_MODE_MONITOR
#define IW_MODE_MONITOR 6
#endif

#ifndef IWEVCUSTOM
#define IWEVCUSTOM 0x8c02
#endif

#ifndef IW_CUSTOM_MAX
 
#define IW_CUSTOM_MAX	256	 
#endif

#define container_of_dwork_rsl(x, y, z)				\
	container_of(to_delayed_work(x), y, z)

static inline void *netdev_priv_rsl(struct net_device *dev)
{
	return netdev_priv(dev);
}

#define KEY_TYPE_NA		0x0
#define KEY_TYPE_WEP40		0x1
#define KEY_TYPE_TKIP		0x2
#define KEY_TYPE_CCMP		0x4
#define KEY_TYPE_WEP104		0x5
 
#define MAX_QUEUE_SIZE		0x10

#define BK_QUEUE			       0
#define BE_QUEUE			       1
#define VI_QUEUE			       2
#define VO_QUEUE			       3
#define HCCA_QUEUE			     4
#define TXCMD_QUEUE			    5
#define MGNT_QUEUE			     6
#define HIGH_QUEUE			     7
#define BEACON_QUEUE			   8

#define IE_CISCO_FLAG_POSITION		0x08
#define SUPPORT_CKIP_MIC			0x08
#define SUPPORT_CKIP_PK			0x10
#define	RT_RF_OFF_LEVL_HALT_NIC		BIT3
#define	RT_IN_PS_LEVEL(psc, _PS_FLAG)		\
	((psc->CurPsLevel & _PS_FLAG) ? true : false)
#define	RT_CLEAR_PS_LEVEL(psc, _PS_FLAG)	\
	(psc->CurPsLevel &= (~(_PS_FLAG)))

 
 
struct cb_desc {
	 
	u8 bLastIniPkt:1;
	u8 bCmdOrInit:1;
	u8 bFirstSeg:1;
	u8 bLastSeg:1;
	u8 bEncrypt:1;
	u8 tx_dis_rate_fallback:1;
	u8 tx_use_drv_assinged_rate:1;
	u8 bHwSec:1;

	u8 nStuckCount;

	 
	u8 bCTSEnable:1;
	u8 bRTSEnable:1;
	u8 bUseShortGI:1;
	u8 bUseShortPreamble:1;
	u8 bTxEnableFwCalcDur:1;
	u8 bAMPDUEnable:1;
	u8 bRTSSTBC:1;
	u8 RTSSC:1;

	u8 bRTSBW:1;
	u8 bPacketBW:1;
	u8 bRTSUseShortPreamble:1;
	u8 bRTSUseShortGI:1;
	u8 bMulticast:1;
	u8 bBroadcast:1;
	u8 drv_agg_enable:1;
	u8 reserved2:1;

	 
	u8 rata_index;
	u8 queue_index;
	u16 txbuf_size;
	u8 ratr_index;
	u8 bAMSDU:1;
	u8 bFromAggrQ:1;
	u8 reserved6:6;
	u8 macId;
	u8 priority;

	 
	u8 data_rate;
	u8 rts_rate;
	u8 ampdu_factor;
	u8 ampdu_density;
	u8 DrvAggrNum;
	u8 bdhcp;
	u16 pkt_size;
	u8 bIsSpecialDataFrame;

	u8 bBTTxPacket;
	u8 bIsBTProbRsp;
};

enum sw_chnl_cmd_id {
	CmdID_End,
	CmdID_SetTxPowerLevel,
	CmdID_BBRegWrite10,
	CmdID_WritePortUlong,
	CmdID_WritePortUshort,
	CmdID_WritePortUchar,
	CmdID_RF_WriteReg,
};

struct sw_chnl_cmd {
	enum sw_chnl_cmd_id CmdID;
	u32			Para1;
	u32			Para2;
	u32			msDelay;
} __packed;

 
#define MGN_1M		  0x02
#define MGN_2M		  0x04
#define MGN_5_5M		0x0b
#define MGN_11M		 0x16

#define MGN_6M		  0x0c
#define MGN_9M		  0x12
#define MGN_12M		 0x18
#define MGN_18M		 0x24
#define MGN_24M		 0x30
#define MGN_36M		 0x48
#define MGN_48M		 0x60
#define MGN_54M		 0x6c

#define MGN_MCS0		0x80
#define MGN_MCS1		0x81
#define MGN_MCS2		0x82
#define MGN_MCS3		0x83
#define MGN_MCS4		0x84
#define MGN_MCS5		0x85
#define MGN_MCS6		0x86
#define MGN_MCS7		0x87
#define MGN_MCS8		0x88
#define MGN_MCS9		0x89
#define MGN_MCS10	       0x8a
#define MGN_MCS11	       0x8b
#define MGN_MCS12	       0x8c
#define MGN_MCS13	       0x8d
#define MGN_MCS14	       0x8e
#define MGN_MCS15	       0x8f

enum hw_variables {
	HW_VAR_ETHER_ADDR,
	HW_VAR_MULTICAST_REG,
	HW_VAR_BASIC_RATE,
	HW_VAR_BSSID,
	HW_VAR_MEDIA_STATUS,
	HW_VAR_SECURITY_CONF,
	HW_VAR_BEACON_INTERVAL,
	HW_VAR_ATIM_WINDOW,
	HW_VAR_LISTEN_INTERVAL,
	HW_VAR_CS_COUNTER,
	HW_VAR_DEFAULTKEY0,
	HW_VAR_DEFAULTKEY1,
	HW_VAR_DEFAULTKEY2,
	HW_VAR_DEFAULTKEY3,
	HW_VAR_SIFS,
	HW_VAR_DIFS,
	HW_VAR_EIFS,
	HW_VAR_SLOT_TIME,
	HW_VAR_ACK_PREAMBLE,
	HW_VAR_CW_CONFIG,
	HW_VAR_CW_VALUES,
	HW_VAR_RATE_FALLBACK_CONTROL,
	HW_VAR_CONTENTION_WINDOW,
	HW_VAR_RETRY_COUNT,
	HW_VAR_TR_SWITCH,
	HW_VAR_COMMAND,
	HW_VAR_WPA_CONFIG,
	HW_VAR_AMPDU_MIN_SPACE,
	HW_VAR_SHORTGI_DENSITY,
	HW_VAR_AMPDU_FACTOR,
	HW_VAR_MCS_RATE_AVAILABLE,
	HW_VAR_AC_PARAM,
	HW_VAR_ACM_CTRL,
	HW_VAR_DIS_Req_Qsize,
	HW_VAR_CCX_CHNL_LOAD,
	HW_VAR_CCX_NOISE_HISTOGRAM,
	HW_VAR_CCX_CLM_NHM,
	HW_VAR_TxOPLimit,
	HW_VAR_TURBO_MODE,
	HW_VAR_RF_STATE,
	HW_VAR_RF_OFF_BY_HW,
	HW_VAR_BUS_SPEED,
	HW_VAR_SET_DEV_POWER,

	HW_VAR_RCR,
	HW_VAR_RATR_0,
	HW_VAR_RRSR,
	HW_VAR_CPU_RST,
	HW_VAR_CECHK_BSSID,
	HW_VAR_LBK_MODE,
	HW_VAR_AES_11N_FIX,
	HW_VAR_USB_RX_AGGR,
	HW_VAR_USER_CONTROL_TURBO_MODE,
	HW_VAR_RETRY_LIMIT,
	HW_VAR_INIT_TX_RATE,
	HW_VAR_TX_RATE_REG,
	HW_VAR_EFUSE_USAGE,
	HW_VAR_EFUSE_BYTES,
	HW_VAR_AUTOLOAD_STATUS,
	HW_VAR_RF_2R_DISABLE,
	HW_VAR_SET_RPWM,
	HW_VAR_H2C_FW_PWRMODE,
	HW_VAR_H2C_FW_JOINBSSRPT,
	HW_VAR_1X1_RECV_COMBINE,
	HW_VAR_STOP_SEND_BEACON,
	HW_VAR_TSF_TIMER,
	HW_VAR_IO_CMD,

	HW_VAR_RF_RECOVERY,
	HW_VAR_H2C_FW_UPDATE_GTK,
	HW_VAR_WF_MASK,
	HW_VAR_WF_CRC,
	HW_VAR_WF_IS_MAC_ADDR,
	HW_VAR_H2C_FW_OFFLOAD,
	HW_VAR_RESET_WFCRC,

	HW_VAR_HANDLE_FW_C2H,
	HW_VAR_DL_FW_RSVD_PAGE,
	HW_VAR_AID,
	HW_VAR_HW_SEQ_ENABLE,
	HW_VAR_CORRECT_TSF,
	HW_VAR_BCN_VALID,
	HW_VAR_FWLPS_RF_ON,
	HW_VAR_DUAL_TSF_RST,
	HW_VAR_SWITCH_EPHY_WoWLAN,
	HW_VAR_INT_MIGRATION,
	HW_VAR_INT_AC,
	HW_VAR_RF_TIMING,
};

enum rt_op_mode {
	RT_OP_MODE_AP,
	RT_OP_MODE_INFRASTRUCTURE,
	RT_OP_MODE_IBSS,
	RT_OP_MODE_NO_LINK,
};

#define aSifsTime						\
	 ((priv->rtllib->current_network.mode == WIRELESS_MODE_N_24G) ? 16 : 10)

#define MGMT_QUEUE_NUM 5

#define MAX_IE_LEN  0xff

#define msleep_interruptible_rsl  msleep_interruptible

 
#define RTLLIB_1ADDR_LEN 10
#define RTLLIB_2ADDR_LEN 16
#define RTLLIB_3ADDR_LEN 24
#define RTLLIB_4ADDR_LEN 30
#define RTLLIB_FCS_LEN    4

#define RTLLIB_SKBBUFFER_SIZE 2500

#define MIN_FRAG_THRESHOLD     256U
#define MAX_FRAG_THRESHOLD     2346U

 
#define RTLLIB_FCTL_FTYPE		0x000c
#define RTLLIB_FCTL_STYPE		0x00f0
#define RTLLIB_FCTL_FRAMETYPE	0x00fc
#define RTLLIB_FCTL_TODS		0x0100
#define RTLLIB_FCTL_FROMDS		0x0200
#define RTLLIB_FCTL_DSTODS		0x0300
#define RTLLIB_FCTL_MOREFRAGS	0x0400
#define RTLLIB_FCTL_RETRY		0x0800
#define RTLLIB_FCTL_PM		0x1000
#define RTLLIB_FCTL_MOREDATA		0x2000
#define RTLLIB_FCTL_WEP		0x4000
#define RTLLIB_FCTL_ORDER		0x8000

#define RTLLIB_FTYPE_MGMT		0x0000
#define RTLLIB_FTYPE_CTL		0x0004
#define RTLLIB_FTYPE_DATA		0x0008

 
#define RTLLIB_STYPE_ASSOC_REQ	0x0000
#define RTLLIB_STYPE_ASSOC_RESP		0x0010
#define RTLLIB_STYPE_REASSOC_REQ	0x0020
#define RTLLIB_STYPE_REASSOC_RESP	0x0030
#define RTLLIB_STYPE_PROBE_REQ	0x0040
#define RTLLIB_STYPE_PROBE_RESP	0x0050
#define RTLLIB_STYPE_BEACON		0x0080
#define RTLLIB_STYPE_ATIM		0x0090
#define RTLLIB_STYPE_DISASSOC	0x00A0
#define RTLLIB_STYPE_AUTH		0x00B0
#define RTLLIB_STYPE_DEAUTH		0x00C0
#define RTLLIB_STYPE_MANAGE_ACT	0x00D0

 
#define RTLLIB_STYPE_PSPOLL		0x00A0
#define RTLLIB_STYPE_RTS		0x00B0
#define RTLLIB_STYPE_CTS		0x00C0
#define RTLLIB_STYPE_ACK		0x00D0

 
#define RTLLIB_STYPE_DATA		0x0000
#define RTLLIB_STYPE_DATA_CFACK	0x0010
#define RTLLIB_STYPE_DATA_CFPOLL	0x0020
#define RTLLIB_STYPE_DATA_CFACKPOLL	0x0030
#define RTLLIB_STYPE_NULLFUNC	0x0040
#define RTLLIB_STYPE_QOS_DATA	0x0080
#define RTLLIB_STYPE_QOS_NULL	0x00C0

#define RTLLIB_SCTL_FRAG		0x000F
#define RTLLIB_SCTL_SEQ		0xFFF0

 
#define RTLLIB_QCTL_TID	      0x000F

#define	FC_QOS_BIT					BIT7
#define IsDataFrame(pdu)	(((pdu[0] & 0x0C) == 0x08) ? true : false)
#define	IsLegacyDataFrame(pdu)	(IsDataFrame(pdu) && (!(pdu[0]&FC_QOS_BIT)))
#define IsQoSDataFrame(pframe)			\
	((*(u16 *)pframe&(RTLLIB_STYPE_QOS_DATA|RTLLIB_FTYPE_DATA)) ==	\
	(RTLLIB_STYPE_QOS_DATA|RTLLIB_FTYPE_DATA))
#define Frame_Order(pframe)     (*(u16 *)pframe&RTLLIB_FCTL_ORDER)
#define SN_LESS(a, b)		(((a-b)&0x800) != 0)
#define SN_EQUAL(a, b)	(a == b)
#define MAX_DEV_ADDR_SIZE 8

enum act_category {
	ACT_CAT_QOS = 1,
	ACT_CAT_DLS = 2,
	ACT_CAT_BA  = 3,
	ACT_CAT_HT  = 7,
	ACT_CAT_WMM = 17,
};

enum ba_action {
	ACT_ADDBAREQ = 0,
	ACT_ADDBARSP = 1,
	ACT_DELBA    = 2,
};

enum init_gain_op_type {
	IG_Backup = 0,
	IG_Restore,
	IG_Max
};

enum wireless_mode {
	WIRELESS_MODE_UNKNOWN = 0x00,
	WIRELESS_MODE_A = 0x01,
	WIRELESS_MODE_B = 0x02,
	WIRELESS_MODE_G = 0x04,
	WIRELESS_MODE_AUTO = 0x08,
	WIRELESS_MODE_N_24G = 0x10,
};

#ifndef ETH_P_PAE
#define ETH_P_PAE	0x888E		 
#define ETH_P_IP	0x0800		 
#define ETH_P_ARP	0x0806		 
#endif  

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW (ETH_P_ECONET + 1)
#endif

 

#define P80211_OUI_LEN 3

struct rtllib_snap_hdr {
	u8    dsap;    
	u8    ssap;    
	u8    ctrl;    
	u8    oui[P80211_OUI_LEN];     

} __packed;

enum _REG_PREAMBLE_MODE {
	PREAMBLE_LONG = 1,
	PREAMBLE_AUTO = 2,
	PREAMBLE_SHORT = 3,
};

#define SNAP_SIZE sizeof(struct rtllib_snap_hdr)

#define WLAN_FC_GET_TYPE(fc) ((fc) & RTLLIB_FCTL_FTYPE)
#define WLAN_FC_GET_STYPE(fc) ((fc) & RTLLIB_FCTL_STYPE)
#define WLAN_FC_MORE_DATA(fc) ((fc) & RTLLIB_FCTL_MOREDATA)

#define WLAN_FC_GET_FRAMETYPE(fc) ((fc) & RTLLIB_FCTL_FRAMETYPE)
#define WLAN_GET_SEQ_FRAG(seq) ((seq) & RTLLIB_SCTL_FRAG)
#define WLAN_GET_SEQ_SEQ(seq)  (((seq) & RTLLIB_SCTL_SEQ) >> 4)

 
#define WLAN_AUTH_OPEN 0
#define WLAN_AUTH_SHARED_KEY 1
#define WLAN_AUTH_LEAP 128

#define WLAN_CAPABILITY_ESS (1<<0)
#define WLAN_CAPABILITY_IBSS (1<<1)
#define WLAN_CAPABILITY_PRIVACY (1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE (1<<5)
#define WLAN_CAPABILITY_SHORT_SLOT_TIME (1<<10)

#define RTLLIB_STATMASK_SIGNAL (1<<0)
#define RTLLIB_STATMASK_RSSI (1<<1)
#define RTLLIB_STATMASK_NOISE (1<<2)
#define RTLLIB_STATMASK_WEMASK 0x7

#define RTLLIB_CCK_MODULATION    (1<<0)
#define RTLLIB_OFDM_MODULATION   (1<<1)

#define RTLLIB_CCK_RATE_LEN		4
#define RTLLIB_CCK_RATE_1MB			0x02
#define RTLLIB_CCK_RATE_2MB			0x04
#define RTLLIB_CCK_RATE_5MB			0x0B
#define RTLLIB_CCK_RATE_11MB			0x16
#define RTLLIB_OFDM_RATE_LEN		8
#define RTLLIB_OFDM_RATE_6MB			0x0C
#define RTLLIB_OFDM_RATE_9MB			0x12
#define RTLLIB_OFDM_RATE_12MB		0x18
#define RTLLIB_OFDM_RATE_18MB		0x24
#define RTLLIB_OFDM_RATE_24MB		0x30
#define RTLLIB_OFDM_RATE_36MB		0x48
#define RTLLIB_OFDM_RATE_48MB		0x60
#define RTLLIB_OFDM_RATE_54MB		0x6C
#define RTLLIB_BASIC_RATE_MASK		0x80

 
#define RTLLIB_FC0_TYPE_MASK		0x0c
#define RTLLIB_FC0_TYPE_DATA		0x08
#define RTLLIB_FC0_SUBTYPE_MASK	0xB0
#define RTLLIB_FC0_SUBTYPE_QOS	0x80

#define RTLLIB_QOS_HAS_SEQ(fc) \
	(((fc) & (RTLLIB_FC0_TYPE_MASK | RTLLIB_FC0_SUBTYPE_MASK)) == \
	 (RTLLIB_FC0_TYPE_DATA | RTLLIB_FC0_SUBTYPE_QOS))

 
#define IEEE_IBSS_MAC_HASH_SIZE 31
struct ieee_ibss_seq {
	u8 mac[ETH_ALEN];
	u16 seq_num[17];
	u16 frag_num[17];
	unsigned long packet_time[17];
	struct list_head list;
};

 
struct rtllib_rx_stats {
	u64 mac_time;
	s8  rssi;
	u8  signal;
	u8  noise;
	u16 rate;  
	u8  control;
	u8  mask;
	u16 len;
	u64 tsf;
	u32 beacon_time;
	u8  nic_type;
	u16 Length;
	u8  SignalQuality;
	s32 RecvSignalPower;
	s8  RxPower;
	u8  SignalStrength;
	u16 bHwError:1;
	u16 bCRC:1;
	u16 bICV:1;
	u16 bShortPreamble:1;
	u16 Antenna:1;
	u16 Decrypted:1;
	u16 Wakeup:1;
	u16 Reserved0:1;
	u8  AGC;
	u32 TimeStampLow;
	u32 TimeStampHigh;
	bool bShift;
	bool bIsQosData;

	u8    RxDrvInfoSize;
	u8    RxBufShift;
	bool  bIsAMPDU;
	bool  bFirstMPDU;
	bool  bContainHTC;
	bool  RxIs40MHzPacket;
	u32   RxPWDBAll;
	u8    RxMIMOSignalStrength[4];
	s8    RxMIMOSignalQuality[2];
	bool  bPacketMatchBSSID;
	bool  bIsCCK;
	bool  bPacketToSelf;
	u16    packetlength;
	u16    fraglength;
	u16    fragoffset;
	u16    ntotalfrag;
	bool   bPacketBeacon;
	bool   bToSelfBA;
	u16    Seq_Num;
};

 
#define RTLLIB_FRAG_CACHE_LEN 4

struct rtllib_frag_entry {
	unsigned long first_frag_time;
	unsigned int seq;
	unsigned int last_frag;
	struct sk_buff *skb;
	u8 src_addr[ETH_ALEN];
	u8 dst_addr[ETH_ALEN];
};

struct rtllib_device;

#define SEC_ACTIVE_KEY    (1<<4)
#define SEC_AUTH_MODE     (1<<5)
#define SEC_UNICAST_GROUP (1<<6)
#define SEC_LEVEL	 (1<<7)
#define SEC_ENABLED       (1<<8)

#define SEC_LEVEL_0      0  
#define SEC_LEVEL_1      1  
#define SEC_LEVEL_2      2  
#define SEC_LEVEL_2_CKIP 3  
#define SEC_LEVEL_3      4  

#define SEC_ALG_NONE		0
#define SEC_ALG_WEP		1
#define SEC_ALG_TKIP		2
#define SEC_ALG_CCMP		4

#define WEP_KEY_LEN		13
#define SCM_KEY_LEN		32

struct rtllib_security {
	u16 active_key:2,
	    enabled:1,
	    auth_mode:2,
	    auth_algo:4,
	    unicast_uses_group:1,
	    encrypt:1;
	u8 key_sizes[NUM_WEP_KEYS];
	u8 keys[NUM_WEP_KEYS][SCM_KEY_LEN];
	u8 level;
	u16 flags;
} __packed;

 

 
enum rtllib_mfie {
	MFIE_TYPE_SSID = 0,
	MFIE_TYPE_RATES = 1,
	MFIE_TYPE_FH_SET = 2,
	MFIE_TYPE_DS_SET = 3,
	MFIE_TYPE_CF_SET = 4,
	MFIE_TYPE_TIM = 5,
	MFIE_TYPE_IBSS_SET = 6,
	MFIE_TYPE_COUNTRY = 7,
	MFIE_TYPE_HOP_PARAMS = 8,
	MFIE_TYPE_HOP_TABLE = 9,
	MFIE_TYPE_REQUEST = 10,
	MFIE_TYPE_CHALLENGE = 16,
	MFIE_TYPE_POWER_CONSTRAINT = 32,
	MFIE_TYPE_POWER_CAPABILITY = 33,
	MFIE_TYPE_TPC_REQUEST = 34,
	MFIE_TYPE_TPC_REPORT = 35,
	MFIE_TYPE_SUPP_CHANNELS = 36,
	MFIE_TYPE_CSA = 37,
	MFIE_TYPE_MEASURE_REQUEST = 38,
	MFIE_TYPE_MEASURE_REPORT = 39,
	MFIE_TYPE_QUIET = 40,
	MFIE_TYPE_IBSS_DFS = 41,
	MFIE_TYPE_ERP = 42,
	MFIE_TYPE_HT_CAP = 45,
	MFIE_TYPE_RSN = 48,
	MFIE_TYPE_RATES_EX = 50,
	MFIE_TYPE_HT_INFO = 61,
	MFIE_TYPE_AIRONET = 133,
	MFIE_TYPE_GENERIC = 221,
	MFIE_TYPE_QOS_PARAMETER = 222,
};

 
struct rtllib_pspoll_hdr {
	__le16 frame_ctl;
	__le16 aid;
	u8 bssid[ETH_ALEN];
	u8 ta[ETH_ALEN];
} __packed;

struct rtllib_hdr {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 payload[];
} __packed;

struct rtllib_hdr_1addr {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 addr1[ETH_ALEN];
	u8 payload[];
} __packed;

struct rtllib_hdr_2addr {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 payload[];
} __packed;

struct rtllib_hdr_3addr {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctl;
	u8 payload[];
} __packed;

struct rtllib_hdr_4addr {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctl;
	u8 addr4[ETH_ALEN];
	u8 payload[];
} __packed;

struct rtllib_hdr_3addrqos {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctl;
	__le16 qos_ctl;
	u8 payload[];
} __packed;

struct rtllib_hdr_4addrqos {
	__le16 frame_ctl;
	__le16 duration_id;
	u8 addr1[ETH_ALEN];
	u8 addr2[ETH_ALEN];
	u8 addr3[ETH_ALEN];
	__le16 seq_ctl;
	u8 addr4[ETH_ALEN];
	__le16 qos_ctl;
	u8 payload[];
} __packed;

struct rtllib_info_element {
	u8 id;
	u8 len;
	u8 data[];
} __packed;

struct rtllib_authentication {
	struct rtllib_hdr_3addr header;
	__le16 algorithm;
	__le16 transaction;
	__le16 status;
	 
	struct rtllib_info_element info_element[];
} __packed;

struct rtllib_disauth {
	struct rtllib_hdr_3addr header;
	__le16 reason;
} __packed;

struct rtllib_disassoc {
	struct rtllib_hdr_3addr header;
	__le16 reason;
} __packed;

struct rtllib_probe_request {
	struct rtllib_hdr_3addr header;
	 
	struct rtllib_info_element info_element[];
} __packed;

struct rtllib_probe_response {
	struct rtllib_hdr_3addr header;
	u32 time_stamp[2];
	__le16 beacon_interval;
	__le16 capability;
	 
	struct rtllib_info_element info_element[];
} __packed;

 
#define rtllib_beacon rtllib_probe_response

struct rtllib_assoc_request_frame {
	struct rtllib_hdr_3addr header;
	__le16 capability;
	__le16 listen_interval;
	 
	struct rtllib_info_element info_element[];
} __packed;

struct rtllib_assoc_response_frame {
	struct rtllib_hdr_3addr header;
	__le16 capability;
	__le16 status;
	__le16 aid;
	struct rtllib_info_element info_element[];  
} __packed;

struct rtllib_txb {
	u8 nr_frags;
	u8 encrypted;
	u8 queue_index;
	u8 rts_included;
	u16 reserved;
	__le16 frag_size;
	__le16 payload_size;
	struct sk_buff *fragments[] __counted_by(nr_frags);
};

#define MAX_SUBFRAME_COUNT		  64
struct rtllib_rxb {
	u8 nr_subframes;
	struct sk_buff *subframes[MAX_SUBFRAME_COUNT];
	u8 dst[ETH_ALEN];
	u8 src[ETH_ALEN];
} __packed;

union frameqos {
	u16 shortdata;
	u8  chardata[2];
	struct {
		u16 tid:4;
		u16 eosp:1;
		u16 ack_policy:2;
		u16 reserved:1;
		u16 txop:8;
	} field;
};

 
#define MAX_RATES_LENGTH		  ((u8)12)
#define MAX_RATES_EX_LENGTH	       ((u8)16)
#define MAX_NETWORK_COUNT		  96

#define MAX_CHANNEL_NUMBER		 161
#define RTLLIB_SOFTMAC_SCAN_TIME	   100
#define RTLLIB_SOFTMAC_ASSOC_RETRY_TIME (HZ * 2)

#define MAX_WPA_IE_LEN 64
#define MAX_WZC_IE_LEN 256

#define NETWORK_EMPTY_ESSID (1<<0)
#define NETWORK_HAS_OFDM    (1<<1)
#define NETWORK_HAS_CCK     (1<<2)

 
#define NETWORK_HAS_QOS_PARAMETERS      (1<<3)
#define NETWORK_HAS_QOS_INFORMATION     (1<<4)
#define NETWORK_HAS_QOS_MASK	    (NETWORK_HAS_QOS_PARAMETERS | \
					 NETWORK_HAS_QOS_INFORMATION)
 
#define NETWORK_HAS_ERP_VALUE	   (1<<10)

#define QOS_QUEUE_NUM		   4
#define QOS_OUI_LEN		     3
#define QOS_OUI_TYPE		    2
#define QOS_ELEMENT_ID		  221
#define QOS_OUI_INFO_SUB_TYPE	   0
#define QOS_OUI_PARAM_SUB_TYPE	  1
#define QOS_VERSION_1		   1

struct rtllib_qos_information_element {
	u8 elementID;
	u8 length;
	u8 qui[QOS_OUI_LEN];
	u8 qui_type;
	u8 qui_subtype;
	u8 version;
	u8 ac_info;
} __packed;

struct rtllib_qos_ac_parameter {
	u8 aci_aifsn;
	u8 ecw_min_max;
	__le16 tx_op_limit;
} __packed;

struct rtllib_qos_parameter_info {
	struct rtllib_qos_information_element info_element;
	u8 reserved;
	struct rtllib_qos_ac_parameter ac_params_record[QOS_QUEUE_NUM];
} __packed;

struct rtllib_qos_parameters {
	__le16 cw_min[QOS_QUEUE_NUM];
	__le16 cw_max[QOS_QUEUE_NUM];
	u8 aifs[QOS_QUEUE_NUM];
	u8 flag[QOS_QUEUE_NUM];
	__le16 tx_op_limit[QOS_QUEUE_NUM];
} __packed;

struct rtllib_qos_data {
	struct rtllib_qos_parameters parameters;
	unsigned int wmm_acm;
	int active;
	int supported;
	u8 param_count;
	u8 old_param_count;
};

struct rtllib_tim_parameters {
	u8 tim_count;
	u8 tim_period;
} __packed;

struct rtllib_wmm_ac_param {
	u8 ac_aci_acm_aifsn;
	u8 ac_ecwmin_ecwmax;
	u16 ac_txop_limit;
};

enum eap_type {
	EAP_PACKET = 0,
	EAPOL_START,
	EAPOL_LOGOFF,
	EAPOL_KEY,
	EAPOL_ENCAP_ASF_ALERT
};

static const char * const eap_types[] = {
	[EAP_PACKET]		= "EAP-Packet",
	[EAPOL_START]		= "EAPOL-Start",
	[EAPOL_LOGOFF]		= "EAPOL-Logoff",
	[EAPOL_KEY]		= "EAPOL-Key",
	[EAPOL_ENCAP_ASF_ALERT]	= "EAPOL-Encap-ASF-Alert"
};

static inline const char *eap_get_type(int type)
{
	return ((u32)type >= ARRAY_SIZE(eap_types)) ? "Unknown" :
		 eap_types[type];
}

static inline u8 Frame_QoSTID(u8 *buf)
{
	struct rtllib_hdr_3addr *hdr;
	u16 fc;

	hdr = (struct rtllib_hdr_3addr *)buf;
	fc = le16_to_cpu(hdr->frame_ctl);
	return (u8)((union frameqos *)(buf + (((fc & RTLLIB_FCTL_TODS) &&
		    (fc & RTLLIB_FCTL_FROMDS)) ? 30 : 24)))->field.tid;
}

struct eapol {
	u8 snap[6];
	u16 ethertype;
	u8 version;
	u8 type;
	u16 length;
} __packed;

struct rtllib_softmac_stats {
	unsigned int rx_ass_ok;
	unsigned int rx_ass_err;
	unsigned int rx_probe_rq;
	unsigned int tx_probe_rs;
	unsigned int tx_beacons;
	unsigned int rx_auth_rq;
	unsigned int rx_auth_rs_ok;
	unsigned int rx_auth_rs_err;
	unsigned int tx_auth_rq;
	unsigned int no_auth_rs;
	unsigned int no_ass_rs;
	unsigned int tx_ass_rq;
	unsigned int rx_ass_rq;
	unsigned int tx_probe_rq;
	unsigned int reassoc;
	unsigned int swtxstop;
	unsigned int swtxawake;
	unsigned char CurrentShowTxate;
	unsigned char last_packet_rate;
	unsigned int txretrycount;
};

 

#define RTLLIB_DEFAULT_TX_ESSID "Penguin"
#define RTLLIB_DEFAULT_BASIC_RATE 2

enum {WMM_all_frame, WMM_two_frame, WMM_four_frame, WMM_six_frame};
#define MAX_SP_Len  (WMM_all_frame << 4)
#define RTLLIB_QOS_TID 0x0f
#define QOS_CTL_NOTCONTAIN_ACK (0x01 << 5)

#define RTLLIB_DTIM_MBCAST 4
#define RTLLIB_DTIM_UCAST 2
#define RTLLIB_DTIM_VALID 1
#define RTLLIB_DTIM_INVALID 0

#define RTLLIB_PS_DISABLED 0
#define RTLLIB_PS_UNICAST RTLLIB_DTIM_UCAST
#define RTLLIB_PS_MBCAST RTLLIB_DTIM_MBCAST

#define WME_AC_BK   0x00
#define WME_AC_BE   0x01
#define WME_AC_VI   0x02
#define WME_AC_VO   0x03
#define WME_AC_PRAM_LEN 16

#define MAX_RECEIVE_BUFFER_SIZE 9100

#define UP2AC(up) (		   \
	((up) < 1) ? WME_AC_BE : \
	((up) < 3) ? WME_AC_BK : \
	((up) < 4) ? WME_AC_BE : \
	((up) < 6) ? WME_AC_VI : \
	WME_AC_VO)

#define	ETHER_ADDR_LEN		6	 
#define ETHERNET_HEADER_SIZE    14       

enum erp_t {
	ERP_NonERPpresent	= 0x01,
	ERP_UseProtection	= 0x02,
	ERP_BarkerPreambleMode = 0x04,
};

struct rtllib_network {
	 
	u8 bssid[ETH_ALEN];
	u8 channel;
	 
	u8 ssid[IW_ESSID_MAX_SIZE + 1];
	u8 ssid_len;
	u8 hidden_ssid[IW_ESSID_MAX_SIZE + 1];
	u8 hidden_ssid_len;
	struct rtllib_qos_data qos_data;

	bool	bWithAironetIE;
	bool	bCkipSupported;
	bool	bCcxRmEnable;
	u8	CcxRmState[2];
	bool	bMBssidValid;
	u8	MBssidMask;
	u8	MBssid[ETH_ALEN];
	bool	bWithCcxVerNum;
	u8	BssCcxVerNumber;
	 
	struct rtllib_rx_stats stats;
	u16 capability;
	u8  rates[MAX_RATES_LENGTH];
	u8  rates_len;
	u8  rates_ex[MAX_RATES_EX_LENGTH];
	u8  rates_ex_len;
	unsigned long last_scanned;
	u8  mode;
	u32 flags;
	u32 time_stamp[2];
	u16 beacon_interval;
	u16 listen_interval;
	u16 atim_window;
	u8  erp_value;
	u8  wpa_ie[MAX_WPA_IE_LEN];
	size_t wpa_ie_len;
	u8  rsn_ie[MAX_WPA_IE_LEN];
	size_t rsn_ie_len;
	u8  wzc_ie[MAX_WZC_IE_LEN];
	size_t wzc_ie_len;

	struct rtllib_tim_parameters tim;
	u8  dtim_period;
	u8  dtim_data;
	u64 last_dtim_sta_time;

	u8 wmm_info;
	struct rtllib_wmm_ac_param wmm_param[4];
	u8 Turbo_Enable;
	u16 CountryIeLen;
	u8 CountryIeBuf[MAX_IE_LEN];
	struct bss_ht bssht;
	bool broadcom_cap_exist;
	bool realtek_cap_exit;
	bool marvell_cap_exist;
	bool ralink_cap_exist;
	bool atheros_cap_exist;
	bool cisco_cap_exist;
	bool airgo_cap_exist;
	bool unknown_cap_exist;
	bool	berp_info_valid;
	bool buseprotection;
	u8 SignalStrength;
	u8 RSSI;
	struct list_head list;
};

enum rtl_link_state {
	 
	MAC80211_NOLINK = 0,

	 

	 
	RTLLIB_ASSOCIATING,
	RTLLIB_ASSOCIATING_RETRY,

	 
	RTLLIB_ASSOCIATING_AUTHENTICATING,

	 
	RTLLIB_ASSOCIATING_AUTHENTICATED,

	 
	MAC80211_LINKED,

	 
	MAC80211_LINKED_SCANNING,
};

#define DEFAULT_MAX_SCAN_AGE (15 * HZ)
#define DEFAULT_FTS 2346

#define CFG_RTLLIB_RESERVE_FCS (1<<0)
#define CFG_RTLLIB_COMPUTE_FCS (1<<1)

struct tx_pending {
	int frag;
	struct rtllib_txb *txb;
};

struct bandwidth_autoswitch {
	long threshold_20Mhzto40Mhz;
	long	threshold_40Mhzto20Mhz;
	bool bforced_tx20Mhz;
	bool bautoswitch_enable;
};

#define REORDER_WIN_SIZE	128
#define REORDER_ENTRY_NUM	128
struct rx_reorder_entry {
	struct list_head	List;
	u16			SeqNum;
	struct rtllib_rxb *prxb;
};

enum fsync_state {
	Default_Fsync,
	HW_Fsync,
	SW_Fsync
};

enum rt_ps_mode {
	eActive,
	eMaxPs,
	eFastPs,
	eAutoPs,
};

enum ips_callback_function {
	IPS_CALLBACK_NONE = 0,
	IPS_CALLBACK_MGNT_LINK_REQUEST = 1,
	IPS_CALLBACK_JOIN_REQUEST = 2,
};

enum rt_rf_power_state {
	rf_on,
	rf_sleep,
	rf_off
};

struct rt_pwr_save_ctrl {
	bool				bSwRfProcessing;
	enum rt_rf_power_state eInactivePowerState;
	enum ips_callback_function ReturnPoint;

	bool				bLeisurePs;
	u8				LpsIdleCount;
	u8				reg_max_lps_awake_intvl;
	u8				LPSAwakeIntvl;

	u32				CurPsLevel;
};

#define RT_RF_CHANGE_SOURCE u32

#define RF_CHANGE_BY_SW BIT31
#define RF_CHANGE_BY_HW BIT30
#define RF_CHANGE_BY_PS BIT29
#define RF_CHANGE_BY_IPS BIT28
#define RF_CHANGE_BY_INIT	0

enum country_code_type {
	COUNTRY_CODE_FCC = 0,
	COUNTRY_CODE_IC = 1,
	COUNTRY_CODE_ETSI = 2,
	COUNTRY_CODE_SPAIN = 3,
	COUNTRY_CODE_FRANCE = 4,
	COUNTRY_CODE_MKK = 5,
	COUNTRY_CODE_MKK1 = 6,
	COUNTRY_CODE_ISRAEL = 7,
	COUNTRY_CODE_TELEC = 8,
	COUNTRY_CODE_MIC = 9,
	COUNTRY_CODE_GLOBAL_DOMAIN = 10,
	COUNTRY_CODE_WORLD_WIDE_13 = 11,
	COUNTRY_CODE_TELEC_NETGEAR = 12,
	COUNTRY_CODE_MAX
};

enum scan_op_backup_opt {
	SCAN_OPT_BACKUP = 0,
	SCAN_OPT_RESTORE,
	SCAN_OPT_MAX
};

enum fw_cmd_io_type {
	FW_CMD_DIG_ENABLE = 0,
	FW_CMD_DIG_DISABLE = 1,
	FW_CMD_DIG_HALT = 2,
	FW_CMD_DIG_RESUME = 3,
	FW_CMD_HIGH_PWR_ENABLE = 4,
	FW_CMD_HIGH_PWR_DISABLE = 5,
	FW_CMD_RA_RESET = 6,
	FW_CMD_RA_ACTIVE = 7,
	FW_CMD_RA_REFRESH_N = 8,
	FW_CMD_RA_REFRESH_BG = 9,
	FW_CMD_RA_INIT = 10,
	FW_CMD_IQK_ENABLE = 11,
	FW_CMD_TXPWR_TRACK_ENABLE = 12,
	FW_CMD_TXPWR_TRACK_DISABLE = 13,
	FW_CMD_TXPWR_TRACK_THERMAL = 14,
	FW_CMD_PAUSE_DM_BY_SCAN = 15,
	FW_CMD_RESUME_DM_BY_SCAN = 16,
	FW_CMD_RA_REFRESH_N_COMB = 17,
	FW_CMD_RA_REFRESH_BG_COMB = 18,
	FW_CMD_ANTENNA_SW_ENABLE = 19,
	FW_CMD_ANTENNA_SW_DISABLE = 20,
	FW_CMD_TX_FEEDBACK_CCX_ENABLE = 21,
	FW_CMD_LPS_ENTER = 22,
	FW_CMD_LPS_LEAVE = 23,
};

#define RT_MAX_LD_SLOT_NUM	10
struct rt_link_detect {
	u32				NumRecvBcnInPeriod;
	u32				NumRecvDataInPeriod;

	u32				RxBcnNum[RT_MAX_LD_SLOT_NUM];
	u32				RxDataNum[RT_MAX_LD_SLOT_NUM];
	u16				SlotNum;
	u16				SlotIndex;

	u32				NumTxOkInPeriod;
	u32				NumRxOkInPeriod;
	u32				NumRxUnicastOkInPeriod;
	bool				bBusyTraffic;
	bool				bHigherBusyTraffic;
	bool				bHigherBusyRxTraffic;
};

struct sw_cam_table {
	u8				macaddr[ETH_ALEN];
	bool				bused;
	u8				key_buf[16];
	u16				key_type;
	u8				useDK;
	u8				key_index;

};

#define   TOTAL_CAM_ENTRY				32
struct rate_adaptive {
	u8				rate_adaptive_disabled;
	u8				ratr_state;
	u16				reserve;

	u32				high_rssi_thresh_for_ra;
	u32				high2low_rssi_thresh_for_ra;
	u8				low2high_rssi_thresh_for_ra40M;
	u32				low_rssi_thresh_for_ra40M;
	u8				low2high_rssi_thresh_for_ra20M;
	u32				low_rssi_thresh_for_ra20M;
	u32				upper_rssi_threshold_ratr;
	u32				middle_rssi_threshold_ratr;
	u32				low_rssi_threshold_ratr;
	u32				low_rssi_threshold_ratr_40M;
	u32				low_rssi_threshold_ratr_20M;
	u8				ping_rssi_enable;
	u32				ping_rssi_ratr;
	u32				ping_rssi_thresh_for_ra;
	u32				last_ratr;
	u8				PreRATRState;

};

#define	NUM_PMKID_CACHE		16
struct rt_pmkid_list {
	u8 Bssid[ETH_ALEN];
	u8 PMKID[16];
	u8 SsidBuf[33];
	u8 bUsed;
};

struct rt_intel_promisc_mode {
	bool promiscuous_on;
	bool fltr_src_sta_frame;
};

 
#define STATUS_SCANNING			0
 

enum {
	LPS_IS_WAKE = 0,
	LPS_IS_SLEEP = 1,
	LPS_WAIT_NULL_DATA_SEND = 2,
};

struct rtllib_device {
	struct pci_dev *pdev;
	struct net_device *dev;
	struct rtllib_security sec;

	bool disable_mgnt_queue;

	unsigned long status;
	u8	CntAfterLink;

	enum rt_op_mode OpMode;

	 
	u8 *assocreq_ies, *assocresp_ies;
	size_t assocreq_ies_len, assocresp_ies_len;

	bool	bForcedBgMode;

	u8 hwsec_active;
	bool is_silent_reset;
	bool is_roaming;
	bool ieee_up;
	bool cannot_notify;
	bool bSupportRemoteWakeUp;
	bool actscanning;
	bool FirstIe_InScan;
	bool be_scan_inprogress;
	bool beinretry;
	enum rt_rf_power_state rf_power_state;
	RT_RF_CHANGE_SOURCE rf_off_reason;
	bool is_set_key;
	bool wx_set_enc;
	struct rt_hi_throughput *ht_info;

	spinlock_t reorder_spinlock;
	u8	reg_dot11ht_oper_rate_set[16];
	u8	reg_dot11tx_ht_oper_rate_set[16];
	u8	dot11ht_oper_rate_set[16];
	u8	reg_ht_supp_rate_set[16];
	u8	HTCurrentOperaRate;
	u8	HTHighestOperaRate;
	u8	tx_dis_rate_fallback;
	u8	tx_use_drv_assinged_rate;
	u8	bTxEnableFwCalcDur;
	atomic_t	atm_swbw;

	struct list_head		Tx_TS_Admit_List;
	struct list_head		Tx_TS_Pending_List;
	struct list_head		Tx_TS_Unused_List;
	struct tx_ts_record TxTsRecord[TOTAL_TS_NUM];
	struct list_head		Rx_TS_Admit_List;
	struct list_head		Rx_TS_Pending_List;
	struct list_head		Rx_TS_Unused_List;
	struct rx_ts_record RxTsRecord[TOTAL_TS_NUM];
	struct rx_reorder_entry RxReorderEntry[128];
	struct list_head		RxReorder_Unused_List;

	 
	struct net_device_stats stats;
	struct rtllib_softmac_stats softmac_stats;

	 
	struct list_head network_free_list;
	struct list_head network_list;
	struct rtllib_network *networks;
	int scans;
	int scan_age;

	int iw_mode;  
	bool net_promiscuous_md;
	struct rt_intel_promisc_mode intel_promiscuous_md_info;

	spinlock_t lock;
	spinlock_t wpax_suitlist_lock;

	int tx_headroom;  
	u32 config;

	 
	int open_wep;  
	int auth_mode;
	int reset_on_keychange;  

	int ieee802_1x;  

	 
	bool bHalfWirelessN24GMode;
	int wpa_enabled;
	int drop_unencrypted;
	int tkip_countermeasures;
	int privacy_invoked;
	size_t wpa_ie_len;
	u8 *wpa_ie;
	size_t wps_ie_len;
	u8 *wps_ie;
	u8 ap_mac_addr[ETH_ALEN];
	u16 pairwise_key_type;
	u16 group_key_type;

	struct lib80211_crypt_info crypt_info;

	struct sw_cam_table swcamtable[TOTAL_CAM_ENTRY];

	struct rt_pmkid_list PMKIDList[NUM_PMKID_CACHE];

	 
	struct rtllib_frag_entry frag_cache[17][RTLLIB_FRAG_CACHE_LEN];
	unsigned int frag_next_idx[17];
	u16 fts;  
#define DEFAULT_RTS_THRESHOLD 2346U
#define MIN_RTS_THRESHOLD 1
#define MAX_RTS_THRESHOLD 2346U
	u16 rts;  

	 
	u8 bssid[ETH_ALEN];

	 
	struct rtllib_network current_network;

	enum rtl_link_state link_state;

	int mode;        

	 
	short sync_scan_hurryup;
	u16 scan_watch_dog;

	 
	void *dot11d_info;
	bool global_domain;
	u8 active_channel_map[MAX_CHANNEL_NUMBER+1];

	u8   bss_start_channel;
	u8   ibss_maxjoin_chal;

	int rate;        
	int basic_rate;

	short active_scan;

	 
	u16 softmac_features;

	 
	u16 seq_ctrl[5];

	 
	u16 associate_seq;

	 
	u16 assoc_id;

	 
	u8 ack_tx_to_ieee;
	short ps;
	short sta_sleep;
	int ps_timeout;
	int ps_period;
	struct work_struct ps_task;
	u64 ps_time;
	bool polling;

	short raw_tx;
	 
	short queue_stop;
	short scanning_continue;
	short proto_started;
	short proto_stoppping;

	struct mutex wx_mutex;
	struct mutex scan_mutex;
	struct mutex ips_mutex;

	spinlock_t mgmt_tx_lock;
	spinlock_t beacon_lock;

	short beacon_txing;

	short wap_set;
	short ssid_set;

	 
	unsigned int wmm_acm;

	 
	struct list_head ibss_mac_hash[IEEE_IBSS_MAC_HASH_SIZE];

	 
	u16 last_rxseq_num[17];  
	u16 last_rxfrag_num[17]; 
	unsigned long last_packet_time[17];

	 
	unsigned long last_rx_ps_time;
	bool			bAwakePktSent;
	u8			LPSDelayCnt;

	 
	struct sk_buff *mgmt_queue_ring[MGMT_QUEUE_NUM];
	int mgmt_queue_head;
	int mgmt_queue_tail;
	u8 AsocRetryCount;
	struct sk_buff_head skb_waitQ[MAX_QUEUE_SIZE];

	bool	bdynamic_txpower_enable;

	bool bCTSToSelfEnable;

	u32	fsync_time_interval;
	u32	fsync_rate_bitmap;
	u8	fsync_rssi_threshold;
	bool	bfsync_enable;

	u8	fsync_multiple_timeinterval;
	u32	fsync_firstdiff_ratethreshold;
	u32	fsync_seconddiff_ratethreshold;
	enum fsync_state fsync_state;
	bool		bis_any_nonbepkts;
	struct bandwidth_autoswitch bandwidth_auto_switch;
	bool FwRWRF;

	struct rt_link_detect link_detect_info;
	bool bIsAggregateFrame;
	struct rt_pwr_save_ctrl pwr_save_ctrl;

	 
	struct tx_pending tx_pending;

	 
	struct timer_list associate_timer;

	 
	struct timer_list beacon_timer;
	u8 need_sw_enc;
	struct work_struct associate_complete_wq;
	struct work_struct ips_leave_wq;
	struct delayed_work associate_procedure_wq;
	struct delayed_work softmac_scan_wq;
	struct delayed_work associate_retry_wq;
	struct delayed_work start_ibss_wq;
	struct delayed_work hw_wakeup_wq;
	struct delayed_work hw_sleep_wq;
	struct delayed_work link_change_wq;
	struct work_struct wx_sync_scan_wq;

	union {
		struct rtllib_rxb *RfdArray[REORDER_WIN_SIZE];
		struct rtllib_rxb *stats_IndicateArray[REORDER_WIN_SIZE];
		struct rtllib_rxb *prxbIndicateArray[REORDER_WIN_SIZE];
		struct {
			struct sw_chnl_cmd PreCommonCmd[MAX_PRECMD_CNT];
			struct sw_chnl_cmd PostCommonCmd[MAX_POSTCMD_CNT];
			struct sw_chnl_cmd RfDependCmd[MAX_RFDEPENDCMD_CNT];
		};
	};

	 

	 
	int (*softmac_hard_start_xmit)(struct sk_buff *skb,
			       struct net_device *dev);

	 
	void (*softmac_data_hard_start_xmit)(struct sk_buff *skb,
			       struct net_device *dev, int rate);

	 
	void (*set_chan)(struct net_device *dev, short ch);

	 
	void (*link_change)(struct net_device *dev);

	 
	void (*start_send_beacons)(struct net_device *dev);
	void (*stop_send_beacons)(struct net_device *dev);

	 
	void (*sta_wake_up)(struct net_device *dev);
	void (*enter_sleep_state)(struct net_device *dev, u64 time);
	short (*ps_is_queue_empty)(struct net_device *dev);
	int (*handle_beacon)(struct net_device *dev,
			     struct rtllib_beacon *beacon,
			     struct rtllib_network *network);
	int (*handle_assoc_response)(struct net_device *dev,
				     struct rtllib_assoc_response_frame *resp,
				     struct rtllib_network *network);

	 
	short (*check_nic_enough_desc)(struct net_device *dev, int queue_index);
	void (*set_bw_mode_handler)(struct net_device *dev,
				    enum ht_channel_width bandwidth,
				    enum ht_extchnl_offset Offset);
	bool (*GetNmodeSupportBySecCfg)(struct net_device *dev);
	void (*set_wireless_mode)(struct net_device *dev, u8 wireless_mode);
	bool (*GetHalfNmodeSupportByAPsHandler)(struct net_device *dev);
	u8   (*rtllib_ap_sec_type)(struct rtllib_device *ieee);
	void (*init_gain_handler)(struct net_device *dev, u8 Operation);
	void (*ScanOperationBackupHandler)(struct net_device *dev,
					   u8 Operation);
	void (*SetHwRegHandler)(struct net_device *dev, u8 variable, u8 *val);

	void (*AllowAllDestAddrHandler)(struct net_device *dev,
					bool bAllowAllDA, bool WriteIntoReg);

	void (*rtllib_ips_leave_wq)(struct net_device *dev);
	void (*rtllib_ips_leave)(struct net_device *dev);
	void (*leisure_ps_leave)(struct net_device *dev);

	 
	u8 priv[];
};

#define IEEE_MODE_MASK    (WIRELESS_MODE_B | WIRELESS_MODE_G)

 

 
#define IEEE_SOFTMAC_SCAN (1<<2)

 
#define IEEE_SOFTMAC_ASSOCIATE (1<<3)

 
#define IEEE_SOFTMAC_PROBERQ (1<<4)

 
#define IEEE_SOFTMAC_PROBERS (1<<5)

 
#define IEEE_SOFTMAC_TX_QUEUE (1<<7)

 
#define IEEE_SOFTMAC_SINGLE_QUEUE (1<<8)

 
#define IEEE_SOFTMAC_BEACONS (1<<6)

static inline void *rtllib_priv(struct net_device *dev)
{
	return ((struct rtllib_device *)netdev_priv(dev))->priv;
}

static inline int rtllib_is_empty_essid(const char *essid, int essid_len)
{
	 
	if (essid_len == 1 && essid[0] == ' ')
		return 1;

	 
	while (essid_len) {
		essid_len--;
		if (essid[essid_len] != '\0')
			return 0;
	}

	return 1;
}

static inline int rtllib_get_hdrlen(u16 fc)
{
	int hdrlen = RTLLIB_3ADDR_LEN;

	switch (WLAN_FC_GET_TYPE(fc)) {
	case RTLLIB_FTYPE_DATA:
		if ((fc & RTLLIB_FCTL_FROMDS) && (fc & RTLLIB_FCTL_TODS))
			hdrlen = RTLLIB_4ADDR_LEN;  
		if (RTLLIB_QOS_HAS_SEQ(fc))
			hdrlen += 2;  
		break;
	case RTLLIB_FTYPE_CTL:
		switch (WLAN_FC_GET_STYPE(fc)) {
		case RTLLIB_STYPE_CTS:
		case RTLLIB_STYPE_ACK:
			hdrlen = RTLLIB_1ADDR_LEN;
			break;
		default:
			hdrlen = RTLLIB_2ADDR_LEN;
			break;
		}
		break;
	}

	return hdrlen;
}

static inline u8 *rtllib_get_payload(struct rtllib_hdr *hdr)
{
	switch (rtllib_get_hdrlen(le16_to_cpu(hdr->frame_ctl))) {
	case RTLLIB_1ADDR_LEN:
		return ((struct rtllib_hdr_1addr *)hdr)->payload;
	case RTLLIB_2ADDR_LEN:
		return ((struct rtllib_hdr_2addr *)hdr)->payload;
	case RTLLIB_3ADDR_LEN:
		return ((struct rtllib_hdr_3addr *)hdr)->payload;
	case RTLLIB_4ADDR_LEN:
		return ((struct rtllib_hdr_4addr *)hdr)->payload;
	}
	return NULL;
}

static inline int rtllib_is_ofdm_rate(u8 rate)
{
	switch (rate & ~RTLLIB_BASIC_RATE_MASK) {
	case RTLLIB_OFDM_RATE_6MB:
	case RTLLIB_OFDM_RATE_9MB:
	case RTLLIB_OFDM_RATE_12MB:
	case RTLLIB_OFDM_RATE_18MB:
	case RTLLIB_OFDM_RATE_24MB:
	case RTLLIB_OFDM_RATE_36MB:
	case RTLLIB_OFDM_RATE_48MB:
	case RTLLIB_OFDM_RATE_54MB:
		return 1;
	}
	return 0;
}

static inline int rtllib_is_cck_rate(u8 rate)
{
	switch (rate & ~RTLLIB_BASIC_RATE_MASK) {
	case RTLLIB_CCK_RATE_1MB:
	case RTLLIB_CCK_RATE_2MB:
	case RTLLIB_CCK_RATE_5MB:
	case RTLLIB_CCK_RATE_11MB:
		return 1;
	}
	return 0;
}

 
void free_rtllib(struct net_device *dev);
struct net_device *alloc_rtllib(int sizeof_priv);

 

int rtllib_encrypt_fragment(struct rtllib_device *ieee,
			    struct sk_buff *frag,
			    int hdr_len);

netdev_tx_t rtllib_xmit(struct sk_buff *skb,  struct net_device *dev);
void rtllib_txb_free(struct rtllib_txb *txb);

 
int rtllib_rx(struct rtllib_device *ieee, struct sk_buff *skb,
	      struct rtllib_rx_stats *rx_stats);
void rtllib_rx_probe_rq(struct rtllib_device *ieee,
			struct sk_buff *skb);
int rtllib_legal_channel(struct rtllib_device *rtllib, u8 channel);

 
int rtllib_wx_get_scan(struct rtllib_device *ieee,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *key);
int rtllib_wx_set_encode(struct rtllib_device *ieee,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *key);
int rtllib_wx_get_encode(struct rtllib_device *ieee,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *key);
int rtllib_wx_set_encode_ext(struct rtllib_device *ieee,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra);
int rtllib_wx_set_auth(struct rtllib_device *ieee,
		       struct iw_request_info *info,
		       struct iw_param *data, char *extra);
int rtllib_wx_set_mlme(struct rtllib_device *ieee,
		       struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra);
int rtllib_wx_set_gen_ie(struct rtllib_device *ieee, u8 *ie, size_t len);

 
int rtllib_rx_frame_softmac(struct rtllib_device *ieee, struct sk_buff *skb,
			    struct rtllib_rx_stats *rx_stats, u16 type,
			    u16 stype);
void rtllib_softmac_new_net(struct rtllib_device *ieee,
			    struct rtllib_network *net);

void SendDisassociation(struct rtllib_device *ieee, bool deauth, u16 asRsn);
void rtllib_softmac_xmit(struct rtllib_txb *txb, struct rtllib_device *ieee);

void rtllib_start_ibss(struct rtllib_device *ieee);
int rtllib_softmac_init(struct rtllib_device *ieee);
void rtllib_softmac_free(struct rtllib_device *ieee);
void rtllib_disassociate(struct rtllib_device *ieee);
void rtllib_stop_scan(struct rtllib_device *ieee);
bool rtllib_act_scanning(struct rtllib_device *ieee, bool sync_scan);
void rtllib_stop_scan_syncro(struct rtllib_device *ieee);
void rtllib_start_scan_syncro(struct rtllib_device *ieee);
void rtllib_sta_ps_send_null_frame(struct rtllib_device *ieee, short pwr);
void rtllib_sta_ps_send_pspoll_frame(struct rtllib_device *ieee);
void rtllib_start_protocol(struct rtllib_device *ieee);
void rtllib_stop_protocol(struct rtllib_device *ieee, u8 shutdown);

void rtllib_EnableNetMonitorMode(struct net_device *dev, bool bInitState);
void rtllib_DisableNetMonitorMode(struct net_device *dev, bool bInitState);
void rtllib_EnableIntelPromiscuousMode(struct net_device *dev, bool bInitState);
void rtllib_DisableIntelPromiscuousMode(struct net_device *dev,
					bool bInitState);
void rtllib_softmac_stop_protocol(struct rtllib_device *ieee,
				  u8 mesh_flag, u8 shutdown);
void rtllib_softmac_start_protocol(struct rtllib_device *ieee, u8 mesh_flag);

void rtllib_reset_queue(struct rtllib_device *ieee);
void rtllib_wake_all_queues(struct rtllib_device *ieee);
void rtllib_stop_all_queues(struct rtllib_device *ieee);
struct sk_buff *rtllib_get_beacon(struct rtllib_device *ieee);
void rtllib_start_send_beacons(struct rtllib_device *ieee);
void rtllib_stop_send_beacons(struct rtllib_device *ieee);

void notify_wx_assoc_event(struct rtllib_device *ieee);
void rtllib_ps_tx_ack(struct rtllib_device *ieee, short success);

void softmac_mgmt_xmit(struct sk_buff *skb, struct rtllib_device *ieee);
u8 rtllib_ap_sec_type(struct rtllib_device *ieee);

 

int rtllib_wx_get_wap(struct rtllib_device *ieee, struct iw_request_info *info,
		      union iwreq_data *wrqu, char *ext);

int rtllib_wx_set_wap(struct rtllib_device *ieee, struct iw_request_info *info,
		      union iwreq_data *awrq, char *extra);

int rtllib_wx_get_essid(struct rtllib_device *ieee, struct iw_request_info *a,
			union iwreq_data *wrqu, char *b);

int rtllib_wx_set_rate(struct rtllib_device *ieee, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra);

int rtllib_wx_get_rate(struct rtllib_device *ieee, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra);

int rtllib_wx_set_mode(struct rtllib_device *ieee, struct iw_request_info *a,
		       union iwreq_data *wrqu, char *b);

int rtllib_wx_set_scan(struct rtllib_device *ieee, struct iw_request_info *a,
		       union iwreq_data *wrqu, char *b);

int rtllib_wx_set_essid(struct rtllib_device *ieee, struct iw_request_info *a,
			union iwreq_data *wrqu, char *extra);

int rtllib_wx_get_mode(struct rtllib_device *ieee, struct iw_request_info *a,
		       union iwreq_data *wrqu, char *b);

int rtllib_wx_set_freq(struct rtllib_device *ieee, struct iw_request_info *a,
		       union iwreq_data *wrqu, char *b);

int rtllib_wx_get_freq(struct rtllib_device *ieee, struct iw_request_info *a,
		       union iwreq_data *wrqu, char *b);
void rtllib_wx_sync_scan_wq(void *data);

int rtllib_wx_set_rawtx(struct rtllib_device *ieee,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra);

int rtllib_wx_get_name(struct rtllib_device *ieee, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra);

int rtllib_wx_set_power(struct rtllib_device *ieee,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra);

int rtllib_wx_get_power(struct rtllib_device *ieee,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra);

int rtllib_wx_set_rts(struct rtllib_device *ieee, struct iw_request_info *info,
		      union iwreq_data *wrqu, char *extra);

int rtllib_wx_get_rts(struct rtllib_device *ieee, struct iw_request_info *info,
		      union iwreq_data *wrqu, char *extra);
#define MAX_RECEIVE_BUFFER_SIZE 9100

void HTSetConnectBwMode(struct rtllib_device *ieee,
			enum ht_channel_width bandwidth,
			enum ht_extchnl_offset Offset);
void HTUpdateDefaultSetting(struct rtllib_device *ieee);
void HTConstructCapabilityElement(struct rtllib_device *ieee,
				  u8 *posHTCap, u8 *len,
				  u8 isEncrypt, bool bAssoc);
void HTConstructInfoElement(struct rtllib_device *ieee,
			    u8 *posHTInfo, u8 *len, u8 isEncrypt);
void HTConstructRT2RTAggElement(struct rtllib_device *ieee,
				u8 *posRT2RTAgg, u8 *len);
void HTOnAssocRsp(struct rtllib_device *ieee);
void HTInitializeHTInfo(struct rtllib_device *ieee);
void HTInitializeBssDesc(struct bss_ht *pBssHT);
void HTResetSelfAndSavePeerSetting(struct rtllib_device *ieee,
				   struct rtllib_network *pNetwork);
void HT_update_self_and_peer_setting(struct rtllib_device *ieee,
				     struct rtllib_network *pNetwork);
u8 HTGetHighestMCSRate(struct rtllib_device *ieee, u8 *pMCSRateSet,
		       u8 *pMCSFilter);
extern u8 MCS_FILTER_ALL[];
extern u16 MCS_DATA_RATE[2][2][77];
u8 HTCCheck(struct rtllib_device *ieee, u8 *pFrame);
void HTResetIOTSetting(struct rt_hi_throughput *ht_info);
bool IsHTHalfNmodeAPs(struct rtllib_device *ieee);
u16  TxCountToDataRate(struct rtllib_device *ieee, u8 nDataRate);
int rtllib_rx_ADDBAReq(struct rtllib_device *ieee, struct sk_buff *skb);
int rtllib_rx_ADDBARsp(struct rtllib_device *ieee, struct sk_buff *skb);
int rtllib_rx_DELBA(struct rtllib_device *ieee, struct sk_buff *skb);
void rtllib_ts_init_add_ba(struct rtllib_device *ieee, struct tx_ts_record *pTS,
			   u8 Policy, u8 bOverwritePending);
void rtllib_ts_init_del_ba(struct rtllib_device *ieee,
			   struct ts_common_info *pTsCommonInfo,
			   enum tr_select TxRxSelect);
void rtllib_ba_setup_timeout(struct timer_list *t);
void rtllib_tx_ba_inact_timeout(struct timer_list *t);
void rtllib_rx_ba_inact_timeout(struct timer_list *t);
void rtllib_reset_ba_entry(struct ba_record *pBA);
bool GetTs(struct rtllib_device *ieee, struct ts_common_info **ppTS, u8 *Addr,
	   u8 TID, enum tr_select TxRxSelect, bool bAddNewTs);
void TSInitialize(struct rtllib_device *ieee);
void TsStartAddBaProcess(struct rtllib_device *ieee,
			 struct tx_ts_record *pTxTS);
void RemovePeerTS(struct rtllib_device *ieee, u8 *Addr);
void RemoveAllTS(struct rtllib_device *ieee);

static inline const char *escape_essid(const char *essid, u8 essid_len)
{
	static char escaped[IW_ESSID_MAX_SIZE * 2 + 1];

	if (rtllib_is_empty_essid(essid, essid_len)) {
		memcpy(escaped, "<hidden>", sizeof("<hidden>"));
		return escaped;
	}

	snprintf(escaped, sizeof(escaped), "%*pE", essid_len, essid);
	return escaped;
}

 
bool rtllib_MgntDisconnect(struct rtllib_device *rtllib, u8 asRsn);

 
void rtllib_FlushRxTsPendingPkts(struct rtllib_device *ieee,
				 struct rx_ts_record *pTS);
int rtllib_parse_info_param(struct rtllib_device *ieee,
			    struct rtllib_info_element *info_element,
			    u16 length,
			    struct rtllib_network *network,
			    struct rtllib_rx_stats *stats);

void rtllib_indicate_packets(struct rtllib_device *ieee,
			     struct rtllib_rxb **prxbIndicateArray, u8  index);
void HTUseDefaultSetting(struct rtllib_device *ieee);
#define RT_ASOC_RETRY_LIMIT	5
u8 MgntQuery_TxRateExcludeCCKRates(struct rtllib_device *ieee);

#endif  

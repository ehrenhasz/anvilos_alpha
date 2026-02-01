 
 

#ifndef _HFA384x_H
#define _HFA384x_H

#define HFA384x_FIRMWARE_VERSION(a, b, c) (((a) << 16) + ((b) << 8) + (c))

#include <linux/if_ether.h>
#include <linux/usb.h>

 
#define	HFA384x_PORTID_MAX		((u16)7)
#define	HFA384x_NUMPORTS_MAX		((u16)(HFA384x_PORTID_MAX + 1))
#define	HFA384x_PDR_LEN_MAX		((u16)512)  
#define	HFA384x_PDA_RECS_MAX		((u16)200)  
#define	HFA384x_PDA_LEN_MAX		((u16)1024)  
#define	HFA384x_SCANRESULT_MAX		((u16)31)
#define	HFA384x_HSCANRESULT_MAX		((u16)31)
#define	HFA384x_CHINFORESULT_MAX	((u16)16)
#define	HFA384x_RID_GUESSING_MAXLEN	2048	 
#define	HFA384x_RIDDATA_MAXLEN		HFA384x_RID_GUESSING_MAXLEN
#define	HFA384x_USB_RWMEM_MAXLEN	2048

 
#define		HFA384x_PORTTYPE_IBSS			((u16)0)
#define		HFA384x_PORTTYPE_BSS			((u16)1)
#define		HFA384x_PORTTYPE_PSUEDOIBSS		((u16)3)
#define		HFA384x_WEPFLAGS_PRIVINVOKED		((u16)BIT(0))
#define		HFA384x_WEPFLAGS_EXCLUDE		((u16)BIT(1))
#define		HFA384x_WEPFLAGS_DISABLE_TXCRYPT	((u16)BIT(4))
#define		HFA384x_WEPFLAGS_DISABLE_RXCRYPT	((u16)BIT(7))
#define		HFA384x_ROAMMODE_HOSTSCAN_HOSTROAM	((u16)3)
#define		HFA384x_PORTSTATUS_DISABLED		((u16)1)
#define		HFA384x_RATEBIT_1			((u16)1)
#define		HFA384x_RATEBIT_2			((u16)2)
#define		HFA384x_RATEBIT_5dot5			((u16)4)
#define		HFA384x_RATEBIT_11			((u16)8)

 
 
 

 
#define		HFA384x_ADDR_FLAT_AUX_PAGE_MASK	(0x007fff80)
#define		HFA384x_ADDR_FLAT_AUX_OFF_MASK	(0x0000007f)
#define		HFA384x_ADDR_FLAT_CMD_PAGE_MASK	(0xffff0000)
#define		HFA384x_ADDR_FLAT_CMD_OFF_MASK	(0x0000ffff)

 
#define		HFA384x_ADDR_AUX_PAGE_MASK	(0xffff)
#define		HFA384x_ADDR_AUX_OFF_MASK	(0x007f)

 
#define		HFA384x_ADDR_AUX_MKFLAT(p, o)	\
		((((u32)(((u16)(p)) & HFA384x_ADDR_AUX_PAGE_MASK)) << 7) | \
		((u32)(((u16)(o)) & HFA384x_ADDR_AUX_OFF_MASK)))

 
#define		HFA384x_ADDR_CMD_MKPAGE(f) \
		((u16)((((u32)(f)) & HFA384x_ADDR_FLAT_CMD_PAGE_MASK) >> 16))
#define		HFA384x_ADDR_CMD_MKOFF(f) \
		((u16)(((u32)(f)) & HFA384x_ADDR_FLAT_CMD_OFF_MASK))

 
#define		HFA3842_PDA_BASE	(0x007f0000UL)
#define		HFA3841_PDA_BASE	(0x003f0000UL)
#define		HFA3841_PDA_BOGUS_BASE	(0x00390000UL)

 
#define		HFA384x_DLSTATE_DISABLED		0
#define		HFA384x_DLSTATE_RAMENABLED		1
#define		HFA384x_DLSTATE_FLASHENABLED		2

 
#define		HFA384x_CMD_AINFO		((u16)GENMASK(14, 8))
#define		HFA384x_CMD_MACPORT		((u16)GENMASK(10, 8))
#define		HFA384x_CMD_PROGMODE		((u16)GENMASK(9, 8))
#define		HFA384x_CMD_CMDCODE		((u16)GENMASK(5, 0))
#define		HFA384x_STATUS_RESULT		((u16)GENMASK(14, 8))

 
 
#define		HFA384x_CMDCODE_INIT		((u16)0x00)
#define		HFA384x_CMDCODE_ENABLE		((u16)0x01)
#define		HFA384x_CMDCODE_DISABLE		((u16)0x02)

 
#define		HFA384x_CMDCODE_INQ		((u16)0x11)

 
#define		HFA384x_CMDCODE_DOWNLD		((u16)0x22)

 
#define		HFA384x_CMDCODE_MONITOR		((u16)(0x38))
#define		HFA384x_MONITOR_ENABLE		((u16)(0x0b))
#define		HFA384x_MONITOR_DISABLE		((u16)(0x0f))

 
#define		HFA384x_CMD_ERR			((u16)(0x7F))

 
#define		HFA384x_PROGMODE_DISABLE	((u16)0x00)
#define		HFA384x_PROGMODE_RAM		((u16)0x01)
#define		HFA384x_PROGMODE_NV		((u16)0x02)
#define		HFA384x_PROGMODE_NVWRITE	((u16)0x03)

 
 
#define		HFA384x_RID_CNFPORTTYPE		((u16)0xFC00)
#define		HFA384x_RID_CNFOWNMACADDR	((u16)0xFC01)
#define		HFA384x_RID_CNFDESIREDSSID	((u16)0xFC02)
#define		HFA384x_RID_CNFOWNCHANNEL	((u16)0xFC03)
#define		HFA384x_RID_CNFOWNSSID		((u16)0xFC04)
#define		HFA384x_RID_CNFMAXDATALEN	((u16)0xFC07)

 
#define		HFA384x_RID_CNFOWNMACADDR_LEN	((u16)6)
#define		HFA384x_RID_CNFDESIREDSSID_LEN	((u16)34)
#define		HFA384x_RID_CNFOWNSSID_LEN	((u16)34)

 
#define		HFA384x_RID_CREATEIBSS		((u16)0xFC81)
#define		HFA384x_RID_FRAGTHRESH		((u16)0xFC82)
#define		HFA384x_RID_RTSTHRESH		((u16)0xFC83)
#define		HFA384x_RID_TXRATECNTL		((u16)0xFC84)
#define		HFA384x_RID_PROMISCMODE		((u16)0xFC85)

 
#define		HFA384x_RID_MAXLOADTIME		((u16)0xFD00)
#define		HFA384x_RID_DOWNLOADBUFFER	((u16)0xFD01)
#define		HFA384x_RID_PRIIDENTITY		((u16)0xFD02)
#define		HFA384x_RID_PRISUPRANGE		((u16)0xFD03)
#define		HFA384x_RID_PRI_CFIACTRANGES	((u16)0xFD04)
#define		HFA384x_RID_NICSERIALNUMBER	((u16)0xFD0A)
#define		HFA384x_RID_NICIDENTITY		((u16)0xFD0B)
#define		HFA384x_RID_MFISUPRANGE		((u16)0xFD0C)
#define		HFA384x_RID_CFISUPRANGE		((u16)0xFD0D)
#define		HFA384x_RID_STAIDENTITY		((u16)0xFD20)
#define		HFA384x_RID_STASUPRANGE		((u16)0xFD21)
#define		HFA384x_RID_STA_MFIACTRANGES	((u16)0xFD22)
#define		HFA384x_RID_STA_CFIACTRANGES	((u16)0xFD23)

 
#define		HFA384x_RID_NICSERIALNUMBER_LEN		((u16)12)

 
#define		HFA384x_RID_PORTSTATUS		((u16)0xFD40)
#define		HFA384x_RID_CURRENTSSID		((u16)0xFD41)
#define		HFA384x_RID_CURRENTBSSID	((u16)0xFD42)
#define		HFA384x_RID_CURRENTTXRATE	((u16)0xFD44)
#define		HFA384x_RID_SHORTRETRYLIMIT	((u16)0xFD48)
#define		HFA384x_RID_LONGRETRYLIMIT	((u16)0xFD49)
#define		HFA384x_RID_MAXTXLIFETIME	((u16)0xFD4A)
#define		HFA384x_RID_PRIVACYOPTIMP	((u16)0xFD4F)
#define		HFA384x_RID_DBMCOMMSQUALITY	((u16)0xFD51)

 
#define		HFA384x_RID_DBMCOMMSQUALITY_LEN	 \
	((u16)sizeof(struct hfa384x_dbmcommsquality))
#define		HFA384x_RID_JOINREQUEST_LEN \
	((u16)sizeof(struct hfa384x_join_request_data))

 
#define		HFA384x_RID_CURRENTCHANNEL	((u16)0xFDC1)

 
#define		HFA384x_RID_CNFWEPDEFAULTKEYID	((u16)0xFC23)
#define		HFA384x_RID_CNFWEPDEFAULTKEY0	((u16)0xFC24)
#define		HFA384x_RID_CNFWEPDEFAULTKEY1	((u16)0xFC25)
#define		HFA384x_RID_CNFWEPDEFAULTKEY2	((u16)0xFC26)
#define		HFA384x_RID_CNFWEPDEFAULTKEY3	((u16)0xFC27)
#define		HFA384x_RID_CNFWEPFLAGS		((u16)0xFC28)
#define		HFA384x_RID_CNFAUTHENTICATION	((u16)0xFC2A)
#define		HFA384x_RID_CNFROAMINGMODE	((u16)0xFC2D)
#define		HFA384x_RID_CNFAPBCNINT		((u16)0xFC33)
#define		HFA384x_RID_CNFDBMADJUST	((u16)0xFC46)
#define		HFA384x_RID_CNFWPADATA		((u16)0xFC48)
#define		HFA384x_RID_CNFBASICRATES	((u16)0xFCB3)
#define		HFA384x_RID_CNFSUPPRATES	((u16)0xFCB4)
#define		HFA384x_RID_CNFPASSIVESCANCTRL	((u16)0xFCBA)
#define		HFA384x_RID_TXPOWERMAX		((u16)0xFCBE)
#define		HFA384x_RID_JOINREQUEST		((u16)0xFCE2)
#define		HFA384x_RID_AUTHENTICATESTA	((u16)0xFCE3)
#define		HFA384x_RID_HOSTSCAN		((u16)0xFCE5)

#define		HFA384x_RID_CNFWEPDEFAULTKEY_LEN	((u16)6)
#define		HFA384x_RID_CNFWEP128DEFAULTKEY_LEN	((u16)14)

 
#define HFA384x_PDR_PCB_PARTNUM		((u16)0x0001)
#define HFA384x_PDR_PDAVER		((u16)0x0002)
#define HFA384x_PDR_NIC_SERIAL		((u16)0x0003)
#define HFA384x_PDR_MKK_MEASUREMENTS	((u16)0x0004)
#define HFA384x_PDR_NIC_RAMSIZE		((u16)0x0005)
#define HFA384x_PDR_MFISUPRANGE		((u16)0x0006)
#define HFA384x_PDR_CFISUPRANGE		((u16)0x0007)
#define HFA384x_PDR_NICID		((u16)0x0008)
#define HFA384x_PDR_MAC_ADDRESS		((u16)0x0101)
#define HFA384x_PDR_REGDOMAIN		((u16)0x0103)
#define HFA384x_PDR_ALLOWED_CHANNEL	((u16)0x0104)
#define HFA384x_PDR_DEFAULT_CHANNEL	((u16)0x0105)
#define HFA384x_PDR_TEMPTYPE		((u16)0x0107)
#define HFA384x_PDR_IFR_SETTING		((u16)0x0200)
#define HFA384x_PDR_RFR_SETTING		((u16)0x0201)
#define HFA384x_PDR_HFA3861_BASELINE	((u16)0x0202)
#define HFA384x_PDR_HFA3861_SHADOW	((u16)0x0203)
#define HFA384x_PDR_HFA3861_IFRF	((u16)0x0204)
#define HFA384x_PDR_HFA3861_CHCALSP	((u16)0x0300)
#define HFA384x_PDR_HFA3861_CHCALI	((u16)0x0301)
#define HFA384x_PDR_MAX_TX_POWER	((u16)0x0302)
#define HFA384x_PDR_MASTER_CHAN_LIST	((u16)0x0303)
#define HFA384x_PDR_3842_NIC_CONFIG	((u16)0x0400)
#define HFA384x_PDR_USB_ID		((u16)0x0401)
#define HFA384x_PDR_PCI_ID		((u16)0x0402)
#define HFA384x_PDR_PCI_IFCONF		((u16)0x0403)
#define HFA384x_PDR_PCI_PMCONF		((u16)0x0404)
#define HFA384x_PDR_RFENRGY		((u16)0x0406)
#define HFA384x_PDR_USB_POWER_TYPE      ((u16)0x0407)
#define HFA384x_PDR_USB_MAX_POWER	((u16)0x0409)
#define HFA384x_PDR_USB_MANUFACTURER	((u16)0x0410)
#define HFA384x_PDR_USB_PRODUCT		((u16)0x0411)
#define HFA384x_PDR_ANT_DIVERSITY	((u16)0x0412)
#define HFA384x_PDR_HFO_DELAY		((u16)0x0413)
#define HFA384x_PDR_SCALE_THRESH	((u16)0x0414)

#define HFA384x_PDR_HFA3861_MANF_TESTSP	((u16)0x0900)
#define HFA384x_PDR_HFA3861_MANF_TESTI	((u16)0x0901)
#define HFA384x_PDR_END_OF_PDA		((u16)0x0000)

 

#define		HFA384x_CMD_AINFO_SET(value)	((u16)((u16)(value) << 8))
#define		HFA384x_CMD_MACPORT_SET(value)	\
			((u16)HFA384x_CMD_AINFO_SET(value))
#define		HFA384x_CMD_PROGMODE_SET(value)	\
			((u16)HFA384x_CMD_AINFO_SET((u16)value))
#define		HFA384x_CMD_CMDCODE_SET(value)		((u16)(value))

#define		HFA384x_STATUS_RESULT_SET(value)	(((u16)(value)) << 8)

 
#define HFA384x_STATE_PREINIT	0
#define HFA384x_STATE_INIT	1
#define HFA384x_STATE_RUNNING	2

 
 
struct hfa384x_bytestr {
	__le16 len;
	u8 data[];
} __packed;

struct hfa384x_bytestr32 {
	__le16 len;
	u8 data[32];
} __packed;

 

 
struct hfa384x_compident {
	u16 id;
	u16 variant;
	u16 major;
	u16 minor;
} __packed;

struct hfa384x_caplevel {
	u16 role;
	u16 id;
	u16 variant;
	u16 bottom;
	u16 top;
} __packed;

 
#define HFA384x_CNFAUTHENTICATION_OPENSYSTEM	0x0001
#define HFA384x_CNFAUTHENTICATION_SHAREDKEY	0x0002
#define HFA384x_CNFAUTHENTICATION_LEAP		0x0004

 

#define HFA384x_CREATEIBSS_JOINCREATEIBSS          0

 
struct hfa384x_host_scan_request_data {
	__le16 channel_list;
	__le16 tx_rate;
	struct hfa384x_bytestr32 ssid;
} __packed;

 
struct hfa384x_join_request_data {
	u8 bssid[WLAN_BSSID_LEN];
	u16 channel;
} __packed;

 
struct hfa384x_authenticate_station_data {
	u8 address[ETH_ALEN];
	__le16 status;
	__le16 algorithm;
} __packed;

 
struct hfa384x_wpa_data {
	__le16 datalen;
	u8 data[];		 
} __packed;

 

 
 
struct hfa384x_downloadbuffer {
	u16 page;
	u16 offset;
	u16 len;
} __packed;

 

#define HFA384x_PSTATUS_CONN_IBSS	((u16)3)

 
struct hfa384x_commsquality {
	__le16 cq_curr_bss;
	__le16 asl_curr_bss;
	__le16 anl_curr_fc;
} __packed;

 
struct hfa384x_dbmcommsquality {
	u16 cq_dbm_curr_bss;
	u16 asl_dbm_curr_bss;
	u16 anl_dbm_curr_fc;
} __packed;

 
 
struct hfa384x_tx_frame {
	u16 status;
	u16 reserved1;
	u16 reserved2;
	u32 sw_support;
	u8 tx_retrycount;
	u8 tx_rate;
	u16 tx_control;

	 
	struct p80211_hdr hdr;
	__le16 data_len;		 

	 

	u8 dest_addr[6];
	u8 src_addr[6];
	u16 data_length;	 
} __packed;
 
 
#define		HFA384x_TXSTATUS_ACKERR			((u16)BIT(5))
#define		HFA384x_TXSTATUS_FORMERR		((u16)BIT(3))
#define		HFA384x_TXSTATUS_DISCON			((u16)BIT(2))
#define		HFA384x_TXSTATUS_AGEDERR		((u16)BIT(1))
#define		HFA384x_TXSTATUS_RETRYERR		((u16)BIT(0))
 
#define		HFA384x_TX_MACPORT			((u16)GENMASK(10, 8))
#define		HFA384x_TX_STRUCTYPE			((u16)GENMASK(4, 3))
#define		HFA384x_TX_TXEX				((u16)BIT(2))
#define		HFA384x_TX_TXOK				((u16)BIT(1))
 
 
#define HFA384x_TXSTATUS_ISERROR(v)	\
	(((u16)(v)) & \
	(HFA384x_TXSTATUS_ACKERR | HFA384x_TXSTATUS_FORMERR | \
	HFA384x_TXSTATUS_DISCON | HFA384x_TXSTATUS_AGEDERR | \
	HFA384x_TXSTATUS_RETRYERR))

#define	HFA384x_TX_SET(v, m, s)		((((u16)(v)) << ((u16)(s))) & ((u16)(m)))

#define	HFA384x_TX_MACPORT_SET(v)	HFA384x_TX_SET(v, HFA384x_TX_MACPORT, 8)
#define	HFA384x_TX_STRUCTYPE_SET(v)	HFA384x_TX_SET(v, \
						HFA384x_TX_STRUCTYPE, 3)
#define	HFA384x_TX_TXEX_SET(v)		HFA384x_TX_SET(v, HFA384x_TX_TXEX, 2)
#define	HFA384x_TX_TXOK_SET(v)		HFA384x_TX_SET(v, HFA384x_TX_TXOK, 1)
 
 
struct hfa384x_rx_frame {
	 
	u16 status;
	u32 time;
	u8 silence;
	u8 signal;
	u8 rate;
	u8 rx_flow;
	u16 reserved1;
	u16 reserved2;

	 
	struct p80211_hdr hdr;
	__le16 data_len;		 

	 
	u8 dest_addr[6];
	u8 src_addr[6];
	u16 data_length;	 
} __packed;
 

 
#define		HFA384x_RXSTATUS_MACPORT		((u16)GENMASK(10, 8))
#define		HFA384x_RXSTATUS_FCSERR			((u16)BIT(0))
 
#define		HFA384x_RXSTATUS_MACPORT_GET(value)	((u16)((((u16)(value)) \
					    & HFA384x_RXSTATUS_MACPORT) >> 8))
#define		HFA384x_RXSTATUS_ISFCSERR(value)	((u16)(((u16)(value)) \
						  & HFA384x_RXSTATUS_FCSERR))
 
#define		HFA384x_IT_HANDOVERADDR			((u16)0xF000UL)
#define		HFA384x_IT_COMMTALLIES			((u16)0xF100UL)
#define		HFA384x_IT_SCANRESULTS			((u16)0xF101UL)
#define		HFA384x_IT_CHINFORESULTS		((u16)0xF102UL)
#define		HFA384x_IT_HOSTSCANRESULTS		((u16)0xF103UL)
#define		HFA384x_IT_LINKSTATUS			((u16)0xF200UL)
#define		HFA384x_IT_ASSOCSTATUS			((u16)0xF201UL)
#define		HFA384x_IT_AUTHREQ			((u16)0xF202UL)
#define		HFA384x_IT_PSUSERCNT			((u16)0xF203UL)
#define		HFA384x_IT_KEYIDCHANGED			((u16)0xF204UL)
#define		HFA384x_IT_ASSOCREQ			((u16)0xF205UL)
#define		HFA384x_IT_MICFAILURE			((u16)0xF206UL)

 

 
struct hfa384x_comm_tallies_16 {
	__le16 txunicastframes;
	__le16 txmulticastframes;
	__le16 txfragments;
	__le16 txunicastoctets;
	__le16 txmulticastoctets;
	__le16 txdeferredtrans;
	__le16 txsingleretryframes;
	__le16 txmultipleretryframes;
	__le16 txretrylimitexceeded;
	__le16 txdiscards;
	__le16 rxunicastframes;
	__le16 rxmulticastframes;
	__le16 rxfragments;
	__le16 rxunicastoctets;
	__le16 rxmulticastoctets;
	__le16 rxfcserrors;
	__le16 rxdiscardsnobuffer;
	__le16 txdiscardswrongsa;
	__le16 rxdiscardswepundecr;
	__le16 rxmsginmsgfrag;
	__le16 rxmsginbadmsgfrag;
} __packed;

struct hfa384x_comm_tallies_32 {
	__le32 txunicastframes;
	__le32 txmulticastframes;
	__le32 txfragments;
	__le32 txunicastoctets;
	__le32 txmulticastoctets;
	__le32 txdeferredtrans;
	__le32 txsingleretryframes;
	__le32 txmultipleretryframes;
	__le32 txretrylimitexceeded;
	__le32 txdiscards;
	__le32 rxunicastframes;
	__le32 rxmulticastframes;
	__le32 rxfragments;
	__le32 rxunicastoctets;
	__le32 rxmulticastoctets;
	__le32 rxfcserrors;
	__le32 rxdiscardsnobuffer;
	__le32 txdiscardswrongsa;
	__le32 rxdiscardswepundecr;
	__le32 rxmsginmsgfrag;
	__le32 rxmsginbadmsgfrag;
} __packed;

 
struct hfa384x_scan_result_sub {
	u16 chid;
	u16 anl;
	u16 sl;
	u8 bssid[WLAN_BSSID_LEN];
	u16 bcnint;
	u16 capinfo;
	struct hfa384x_bytestr32 ssid;
	u8 supprates[10];	 
	u16 proberesp_rate;
} __packed;

struct hfa384x_scan_result {
	u16 rsvd;
	u16 scanreason;
	struct hfa384x_scan_result_sub result[HFA384x_SCANRESULT_MAX];
} __packed;

 
struct hfa384x_ch_info_result_sub {
	u16 chid;
	u16 anl;
	u16 pnl;
	u16 active;
} __packed;

#define HFA384x_CHINFORESULT_BSSACTIVE	BIT(0)
#define HFA384x_CHINFORESULT_PCFACTIVE	BIT(1)

struct hfa384x_ch_info_result {
	u16 scanchannels;
	struct hfa384x_ch_info_result_sub result[HFA384x_CHINFORESULT_MAX];
} __packed;

 
struct hfa384x_hscan_result_sub {
	__le16 chid;
	__le16 anl;
	__le16 sl;
	u8 bssid[WLAN_BSSID_LEN];
	__le16 bcnint;
	__le16 capinfo;
	struct hfa384x_bytestr32 ssid;
	u8 supprates[10];	 
	u16 proberesp_rate;
	__le16 atim;
} __packed;

struct hfa384x_hscan_result {
	u16 nresult;
	u16 rsvd;
	struct hfa384x_hscan_result_sub result[HFA384x_HSCANRESULT_MAX];
} __packed;

 

#define HFA384x_LINK_NOTCONNECTED	((u16)0)
#define HFA384x_LINK_CONNECTED		((u16)1)
#define HFA384x_LINK_DISCONNECTED	((u16)2)
#define HFA384x_LINK_AP_CHANGE		((u16)3)
#define HFA384x_LINK_AP_OUTOFRANGE	((u16)4)
#define HFA384x_LINK_AP_INRANGE		((u16)5)
#define HFA384x_LINK_ASSOCFAIL		((u16)6)

struct hfa384x_link_status {
	__le16 linkstatus;
} __packed;

 

#define HFA384x_ASSOCSTATUS_STAASSOC	((u16)1)
#define HFA384x_ASSOCSTATUS_REASSOC	((u16)2)
#define HFA384x_ASSOCSTATUS_AUTHFAIL	((u16)5)

struct hfa384x_assoc_status {
	u16 assocstatus;
	u8 sta_addr[ETH_ALEN];
	 
	u8 old_ap_addr[ETH_ALEN];
	u16 reason;
	u16 reserved;
} __packed;

 

struct hfa384x_auth_request {
	u8 sta_addr[ETH_ALEN];
	__le16 algorithm;
} __packed;

 

struct hfa384x_ps_user_count {
	__le16 usercnt;
} __packed;

struct hfa384x_key_id_changed {
	u8 sta_addr[ETH_ALEN];
	u16 keyid;
} __packed;

 
union hfa384x_infodata {
	struct hfa384x_comm_tallies_16 commtallies16;
	struct hfa384x_comm_tallies_32 commtallies32;
	struct hfa384x_scan_result scanresult;
	struct hfa384x_ch_info_result chinforesult;
	struct hfa384x_hscan_result hscanresult;
	struct hfa384x_link_status linkstatus;
	struct hfa384x_assoc_status assocstatus;
	struct hfa384x_auth_request authreq;
	struct hfa384x_ps_user_count psusercnt;
	struct hfa384x_key_id_changed keyidchanged;
} __packed;

struct hfa384x_inf_frame {
	u16 framelen;
	u16 infotype;
	union hfa384x_infodata info;
} __packed;

 

 
#define HFA384x_USB_TXFRM	0
#define HFA384x_USB_CMDREQ	1
#define HFA384x_USB_WRIDREQ	2
#define HFA384x_USB_RRIDREQ	3
#define HFA384x_USB_WMEMREQ	4
#define HFA384x_USB_RMEMREQ	5

 
#define HFA384x_USB_ISTXFRM(a)	(((a) & 0x9000) == 0x1000)
#define HFA384x_USB_ISRXFRM(a)	(!((a) & 0x9000))
#define HFA384x_USB_INFOFRM	0x8000
#define HFA384x_USB_CMDRESP	0x8001
#define HFA384x_USB_WRIDRESP	0x8002
#define HFA384x_USB_RRIDRESP	0x8003
#define HFA384x_USB_WMEMRESP	0x8004
#define HFA384x_USB_RMEMRESP	0x8005
#define HFA384x_USB_BUFAVAIL	0x8006
#define HFA384x_USB_ERROR	0x8007

 
 

struct hfa384x_usb_txfrm {
	struct hfa384x_tx_frame desc;
	u8 data[WLAN_DATA_MAXLEN];
} __packed;

struct hfa384x_usb_cmdreq {
	__le16 type;
	__le16 cmd;
	__le16 parm0;
	__le16 parm1;
	__le16 parm2;
	u8 pad[54];
} __packed;

struct hfa384x_usb_wridreq {
	__le16 type;
	__le16 frmlen;
	__le16 rid;
	u8 data[HFA384x_RIDDATA_MAXLEN];
} __packed;

struct hfa384x_usb_rridreq {
	__le16 type;
	__le16 frmlen;
	__le16 rid;
	u8 pad[58];
} __packed;

struct hfa384x_usb_wmemreq {
	__le16 type;
	__le16 frmlen;
	__le16 offset;
	__le16 page;
	u8 data[HFA384x_USB_RWMEM_MAXLEN];
} __packed;

struct hfa384x_usb_rmemreq {
	__le16 type;
	__le16 frmlen;
	__le16 offset;
	__le16 page;
	u8 pad[56];
} __packed;

 
 

struct hfa384x_usb_rxfrm {
	struct hfa384x_rx_frame desc;
	u8 data[WLAN_DATA_MAXLEN];
} __packed;

struct hfa384x_usb_infofrm {
	u16 type;
	struct hfa384x_inf_frame info;
} __packed;

struct hfa384x_usb_statusresp {
	u16 type;
	__le16 status;
	__le16 resp0;
	__le16 resp1;
	__le16 resp2;
} __packed;

struct hfa384x_usb_rridresp {
	u16 type;
	__le16 frmlen;
	__le16 rid;
	u8 data[HFA384x_RIDDATA_MAXLEN];
} __packed;

struct hfa384x_usb_rmemresp {
	u16 type;
	u16 frmlen;
	u8 data[HFA384x_USB_RWMEM_MAXLEN];
} __packed;

struct hfa384x_usb_bufavail {
	u16 type;
	u16 frmlen;
} __packed;

struct hfa384x_usb_error {
	u16 type;
	u16 errortype;
} __packed;

 
 

union hfa384x_usbout {
	__le16 type;
	struct hfa384x_usb_txfrm txfrm;
	struct hfa384x_usb_cmdreq cmdreq;
	struct hfa384x_usb_wridreq wridreq;
	struct hfa384x_usb_rridreq rridreq;
	struct hfa384x_usb_wmemreq wmemreq;
	struct hfa384x_usb_rmemreq rmemreq;
} __packed;

union hfa384x_usbin {
	__le16 type;
	struct hfa384x_usb_rxfrm rxfrm;
	struct hfa384x_usb_txfrm txfrm;
	struct hfa384x_usb_infofrm infofrm;
	struct hfa384x_usb_statusresp cmdresp;
	struct hfa384x_usb_statusresp wridresp;
	struct hfa384x_usb_rridresp rridresp;
	struct hfa384x_usb_statusresp wmemresp;
	struct hfa384x_usb_rmemresp rmemresp;
	struct hfa384x_usb_bufavail bufavail;
	struct hfa384x_usb_error usberror;
	u8 boguspad[3000];
} __packed;

 

struct hfa384x_pdr_mfisuprange {
	u16 id;
	u16 variant;
	u16 bottom;
	u16 top;
} __packed;

struct hfa384x_pdr_cfisuprange {
	u16 id;
	u16 variant;
	u16 bottom;
	u16 top;
} __packed;

struct hfa384x_pdr_nicid {
	u16 id;
	u16 variant;
	u16 major;
	u16 minor;
} __packed;

struct hfa384x_pdrec {
	__le16 len;		 
	__le16 code;
	union pdr {
		struct hfa384x_pdr_mfisuprange mfisuprange;
		struct hfa384x_pdr_cfisuprange cfisuprange;
		struct hfa384x_pdr_nicid nicid;

	} data;
} __packed;

#ifdef __KERNEL__
 
struct hfa384x_cmdresult {
	u16 status;
	u16 resp0;
	u16 resp1;
	u16 resp2;
};

 
 
struct hfa384x_rridresult {
	u16 rid;
	const void *riddata;
	unsigned int riddata_len;
};

enum ctlx_state {
	CTLX_START = 0,		 

	CTLX_COMPLETE,		 
	CTLX_REQ_FAILED,	 

	CTLX_PENDING,		 
	CTLX_REQ_SUBMITTED,	 
	CTLX_REQ_COMPLETE,	 
	CTLX_RESP_COMPLETE	 
};

struct hfa384x_usbctlx;
struct hfa384x;

typedef void (*ctlx_cmdcb_t) (struct hfa384x *, const struct hfa384x_usbctlx *);

typedef void (*ctlx_usercb_t) (struct hfa384x *hw,
			       void *ctlxresult, void *usercb_data);

struct hfa384x_usbctlx {
	struct list_head list;

	size_t outbufsize;
	union hfa384x_usbout outbuf;	 
	union hfa384x_usbin inbuf;	 

	enum ctlx_state state;	 

	struct completion done;
	int reapable;		 

	ctlx_cmdcb_t cmdcb;	 
	ctlx_usercb_t usercb;	 
	void *usercb_data;	 
};

struct hfa384x_usbctlxq {
	spinlock_t lock;
	struct list_head pending;
	struct list_head active;
	struct list_head completing;
	struct list_head reapable;
};

struct hfa384x_metacmd {
	u16 cmd;

	u16 parm0;
	u16 parm1;
	u16 parm2;

	struct hfa384x_cmdresult result;
};

#define	MAX_GRP_ADDR		32
#define WLAN_COMMENT_MAX	80   

#define WLAN_AUTH_MAX           60   
#define WLAN_ACCESS_MAX		60   
#define WLAN_ACCESS_NONE	0    
#define WLAN_ACCESS_ALL		1    
#define WLAN_ACCESS_ALLOW	2    
#define WLAN_ACCESS_DENY	3    

 
struct prism2sta_authlist {
	unsigned int cnt;
	u8 addr[WLAN_AUTH_MAX][ETH_ALEN];
	u8 assoc[WLAN_AUTH_MAX];
};

struct prism2sta_accesslist {
	unsigned int modify;
	unsigned int cnt;
	u8 addr[WLAN_ACCESS_MAX][ETH_ALEN];
	unsigned int cnt1;
	u8 addr1[WLAN_ACCESS_MAX][ETH_ALEN];
};

struct hfa384x {
	 
	struct usb_device *usb;
	struct urb rx_urb;
	struct sk_buff *rx_urb_skb;
	struct urb tx_urb;
	struct urb ctlx_urb;
	union hfa384x_usbout txbuff;
	struct hfa384x_usbctlxq ctlxq;
	struct timer_list reqtimer;
	struct timer_list resptimer;

	struct timer_list throttle;

	struct work_struct reaper_bh;
	struct work_struct completion_bh;

	struct work_struct usb_work;

	unsigned long usb_flags;
#define THROTTLE_RX	0
#define THROTTLE_TX	1
#define WORK_RX_HALT	2
#define WORK_TX_HALT	3
#define WORK_RX_RESUME	4
#define WORK_TX_RESUME	5

	unsigned short req_timer_done:1;
	unsigned short resp_timer_done:1;

	int endp_in;
	int endp_out;

	int sniff_fcs;
	int sniff_channel;
	int sniff_truncate;
	int sniffhdr;

	wait_queue_head_t cmdq;	 

	 
	u32 state;
	u32 isap;
	u8 port_enabled[HFA384x_NUMPORTS_MAX];

	 
	unsigned int dlstate;
	struct hfa384x_downloadbuffer bufinfo;
	u16 dltimeout;

	int scanflag;		 
	int join_ap;		 
	int join_retries;	 
	struct hfa384x_join_request_data joinreq; 

	struct wlandevice *wlandev;
	 
	struct work_struct link_bh;

	struct work_struct commsqual_bh;
	struct hfa384x_commsquality qual;
	struct timer_list commsqual_timer;

	u16 link_status;
	u16 link_status_new;
	struct sk_buff_head authq;

	u32 txrate;

	 

	 
	unsigned int presniff_port_type;
	u16 presniff_wepflags;
	u32 dot11_desired_bss_type;

	int dbmadjust;

	 
	u8 dot11_grp_addr[MAX_GRP_ADDR][ETH_ALEN];
	unsigned int dot11_grpcnt;

	 
	struct hfa384x_compident ident_nic;
	struct hfa384x_compident ident_pri_fw;
	struct hfa384x_compident ident_sta_fw;
	struct hfa384x_compident ident_ap_fw;
	u16 mm_mods;

	 
	struct hfa384x_caplevel cap_sup_mfi;
	struct hfa384x_caplevel cap_sup_cfi;
	struct hfa384x_caplevel cap_sup_pri;
	struct hfa384x_caplevel cap_sup_sta;
	struct hfa384x_caplevel cap_sup_ap;

	 
	struct hfa384x_caplevel cap_act_pri_cfi;  

	struct hfa384x_caplevel cap_act_sta_cfi;  

	struct hfa384x_caplevel cap_act_sta_mfi;  

	struct hfa384x_caplevel cap_act_ap_cfi;	 

	struct hfa384x_caplevel cap_act_ap_mfi;	 

	u32 psusercount;	 
	struct hfa384x_comm_tallies_32 tallies;	 
	u8 comment[WLAN_COMMENT_MAX + 1];	 

	 
	struct {
		atomic_t done;
		u8 count;
		struct hfa384x_ch_info_result results;
	} channel_info;

	struct hfa384x_inf_frame *scanresults;

	struct prism2sta_authlist authlist;	 
	unsigned int accessmode;		 
	struct prism2sta_accesslist allow;	 
	struct prism2sta_accesslist deny;	 

};

void hfa384x_create(struct hfa384x *hw, struct usb_device *usb);
void hfa384x_destroy(struct hfa384x *hw);

int hfa384x_corereset(struct hfa384x *hw, int holdtime, int settletime,
		      int genesis);
int hfa384x_drvr_disable(struct hfa384x *hw, u16 macport);
int hfa384x_drvr_enable(struct hfa384x *hw, u16 macport);
int hfa384x_drvr_flashdl_enable(struct hfa384x *hw);
int hfa384x_drvr_flashdl_disable(struct hfa384x *hw);
int hfa384x_drvr_flashdl_write(struct hfa384x *hw, u32 daddr, void *buf,
			       u32 len);
int hfa384x_drvr_getconfig(struct hfa384x *hw, u16 rid, void *buf, u16 len);
int hfa384x_drvr_ramdl_enable(struct hfa384x *hw, u32 exeaddr);
int hfa384x_drvr_ramdl_disable(struct hfa384x *hw);
int hfa384x_drvr_ramdl_write(struct hfa384x *hw, u32 daddr, void *buf, u32 len);
int hfa384x_drvr_readpda(struct hfa384x *hw, void *buf, unsigned int len);
int hfa384x_drvr_setconfig(struct hfa384x *hw, u16 rid, void *buf, u16 len);

static inline int
hfa384x_drvr_getconfig16(struct hfa384x *hw, u16 rid, void *val)
{
	int result = 0;

	result = hfa384x_drvr_getconfig(hw, rid, val, sizeof(u16));
	if (result == 0)
		le16_to_cpus(val);
	return result;
}

static inline int hfa384x_drvr_setconfig16(struct hfa384x *hw, u16 rid, u16 val)
{
	__le16 value = cpu_to_le16(val);

	return hfa384x_drvr_setconfig(hw, rid, &value, sizeof(value));
}

int
hfa384x_drvr_setconfig_async(struct hfa384x *hw,
			     u16 rid,
			     void *buf,
			     u16 len, ctlx_usercb_t usercb, void *usercb_data);

static inline int
hfa384x_drvr_setconfig16_async(struct hfa384x *hw, u16 rid, u16 val)
{
	__le16 value = cpu_to_le16(val);

	return hfa384x_drvr_setconfig_async(hw, rid, &value, sizeof(value),
					    NULL, NULL);
}

int hfa384x_drvr_start(struct hfa384x *hw);
int hfa384x_drvr_stop(struct hfa384x *hw);
int
hfa384x_drvr_txframe(struct hfa384x *hw, struct sk_buff *skb,
		     struct p80211_hdr *p80211_hdr,
		     struct p80211_metawep *p80211_wep);
void hfa384x_tx_timeout(struct wlandevice *wlandev);

int hfa384x_cmd_initialize(struct hfa384x *hw);
int hfa384x_cmd_enable(struct hfa384x *hw, u16 macport);
int hfa384x_cmd_disable(struct hfa384x *hw, u16 macport);
int hfa384x_cmd_allocate(struct hfa384x *hw, u16 len);
int hfa384x_cmd_monitor(struct hfa384x *hw, u16 enable);
int
hfa384x_cmd_download(struct hfa384x *hw,
		     u16 mode, u16 lowaddr, u16 highaddr, u16 codelen);

#endif  

#endif  

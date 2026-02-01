
 


#ifndef FWIL_TYPES_H_
#define FWIL_TYPES_H_

#include <linux/if_ether.h>


#define BRCMF_FIL_ACTION_FRAME_SIZE	1800

 
#define BRCMF_ARP_OL_AGENT		0x00000001
#define BRCMF_ARP_OL_SNOOP		0x00000002
#define BRCMF_ARP_OL_HOST_AUTO_REPLY	0x00000004
#define BRCMF_ARP_OL_PEER_AUTO_REPLY	0x00000008

#define	BRCMF_BSS_INFO_VERSION	109  
#define BRCMF_BSS_RSSI_ON_CHANNEL	0x0004

#define BRCMF_STA_BRCM			0x00000001	 
#define BRCMF_STA_WME			0x00000002	 
#define BRCMF_STA_NONERP		0x00000004	 
#define BRCMF_STA_AUTHE			0x00000008	 
#define BRCMF_STA_ASSOC			0x00000010	 
#define BRCMF_STA_AUTHO			0x00000020	 
#define BRCMF_STA_WDS			0x00000040	 
#define BRCMF_STA_WDS_LINKUP		0x00000080	 
#define BRCMF_STA_PS			0x00000100	 
#define BRCMF_STA_APSD_BE		0x00000200	 
#define BRCMF_STA_APSD_BK		0x00000400	 
#define BRCMF_STA_APSD_VI		0x00000800	 
#define BRCMF_STA_APSD_VO		0x00001000	 
#define BRCMF_STA_N_CAP			0x00002000	 
#define BRCMF_STA_SCBSTATS		0x00004000	 
#define BRCMF_STA_AMPDU_CAP		0x00008000	 
#define BRCMF_STA_AMSDU_CAP		0x00010000	 
#define BRCMF_STA_MIMO_PS		0x00020000	 
#define BRCMF_STA_MIMO_RTS		0x00040000	 
#define BRCMF_STA_RIFS_CAP		0x00080000	 
#define BRCMF_STA_VHT_CAP		0x00100000	 
#define BRCMF_STA_WPS			0x00200000	 
#define BRCMF_STA_DWDS_CAP		0x01000000	 
#define BRCMF_STA_DWDS			0x02000000	 

 
#define BRCMF_SCAN_PARAMS_FIXED_SIZE	64
#define BRCMF_SCAN_PARAMS_V2_FIXED_SIZE	72

 
#define BRCMF_SCAN_PARAMS_VERSION_V2	2

 
#define BRCMF_SCAN_PARAMS_COUNT_MASK	0x0000ffff
#define BRCMF_SCAN_PARAMS_NSSID_SHIFT	16

 
#define BRCMF_SCANTYPE_DEFAULT		0xFF
#define BRCMF_SCANTYPE_ACTIVE		0
#define BRCMF_SCANTYPE_PASSIVE		1

#define BRCMF_WSEC_MAX_PSK_LEN		32
#define	BRCMF_WSEC_PASSPHRASE		BIT(0)

#define BRCMF_WSEC_MAX_SAE_PASSWORD_LEN 128

 
#define BRCMF_PRIMARY_KEY		(1 << 1)
#define DOT11_BSSTYPE_ANY		2
#define BRCMF_ESCAN_REQ_VERSION		1
#define BRCMF_ESCAN_REQ_VERSION_V2	2

#define BRCMF_MAXRATES_IN_SET		16	 

 
#define BRCMF_OBSS_COEX_AUTO		(-1)
#define BRCMF_OBSS_COEX_OFF		0
#define BRCMF_OBSS_COEX_ON		1

 
 
#define BRCMF_WOWL_MAGIC		(1 << 0)
 
#define BRCMF_WOWL_NET			(1 << 1)
 
#define BRCMF_WOWL_DIS			(1 << 2)
 
#define BRCMF_WOWL_RETR			(1 << 3)
 
#define BRCMF_WOWL_BCN			(1 << 4)
 
#define BRCMF_WOWL_TST			(1 << 5)
 
#define BRCMF_WOWL_M1			(1 << 6)
 
#define BRCMF_WOWL_EAPID		(1 << 7)
 
#define BRCMF_WOWL_PME_GPIO		(1 << 8)
 
#define BRCMF_WOWL_NEEDTKIP1		(1 << 9)
 
#define BRCMF_WOWL_GTK_FAILURE		(1 << 10)
 
#define BRCMF_WOWL_EXTMAGPAT		(1 << 11)
 
#define BRCMF_WOWL_ARPOFFLOAD		(1 << 12)
 
#define BRCMF_WOWL_WPA2			(1 << 13)
 
#define BRCMF_WOWL_KEYROT		(1 << 14)
 
#define BRCMF_WOWL_BCAST		(1 << 15)
 
#define BRCMF_WOWL_SCANOL		(1 << 16)
 
#define BRCMF_WOWL_TCPKEEP_TIME		(1 << 17)
 
#define BRCMF_WOWL_MDNS_CONFLICT	(1 << 18)
 
#define BRCMF_WOWL_MDNS_SERVICE		(1 << 19)
 
#define BRCMF_WOWL_TCPKEEP_DATA		(1 << 20)
 
#define BRCMF_WOWL_FW_HALT		(1 << 21)
 
#define BRCMF_WOWL_ENAB_HWRADIO		(1 << 22)
 
#define BRCMF_WOWL_MIC_FAIL		(1 << 23)
 
#define BRCMF_WOWL_UNASSOC		(1 << 24)
 
#define BRCMF_WOWL_SECURE		(1 << 25)
 
#define BRCMF_WOWL_PFN_FOUND		(1 << 27)
 
#define WIPHY_WOWL_EAP_PK		(1 << 28)
 
#define BRCMF_WOWL_LINKDOWN		(1 << 31)

#define BRCMF_WOWL_MAXPATTERNS		16
#define BRCMF_WOWL_MAXPATTERNSIZE	128

#define BRCMF_COUNTRY_BUF_SZ		4
#define BRCMF_ANT_MAX			4

#define BRCMF_MAX_ASSOCLIST		128

#define BRCMF_TXBF_SU_BFE_CAP		BIT(0)
#define BRCMF_TXBF_MU_BFE_CAP		BIT(1)
#define BRCMF_TXBF_SU_BFR_CAP		BIT(0)
#define BRCMF_TXBF_MU_BFR_CAP		BIT(1)

#define	BRCMF_MAXPMKID			16	 
#define BRCMF_NUMCHANNELS		64

#define BRCMF_PFN_MACADDR_CFG_VER	1
#define BRCMF_PFN_MAC_OUI_ONLY		BIT(0)
#define BRCMF_PFN_SET_MAC_UNASSOC	BIT(1)

#define BRCMF_MCSSET_LEN		16

#define BRCMF_RSN_KCK_LENGTH		16
#define BRCMF_RSN_KEK_LENGTH		16
#define BRCMF_RSN_REPLAY_LEN		8

#define BRCMF_MFP_NONE			0
#define BRCMF_MFP_CAPABLE		1
#define BRCMF_MFP_REQUIRED		2

#define BRCMF_VHT_CAP_MCS_MAP_NSS_MAX	8

#define BRCMF_HE_CAP_MCS_MAP_NSS_MAX	8

#define BRCMF_PMKSA_VER_2		2
#define BRCMF_PMKSA_VER_3		3
#define BRCMF_PMKSA_NO_EXPIRY		0xffffffff

 
#define MAX_CHUNK_LEN			1400

#define DLOAD_HANDLER_VER		1	 
#define DLOAD_FLAG_VER_MASK		0xf000	 
#define DLOAD_FLAG_VER_SHIFT		12	 

#define DL_BEGIN			0x0002
#define DL_END				0x0004

#define DL_TYPE_CLM			2

 
enum brcmf_join_pref_types {
	BRCMF_JOIN_PREF_RSSI = 1,
	BRCMF_JOIN_PREF_WPA,
	BRCMF_JOIN_PREF_BAND,
	BRCMF_JOIN_PREF_RSSI_DELTA,
};

enum brcmf_fil_p2p_if_types {
	BRCMF_FIL_P2P_IF_CLIENT,
	BRCMF_FIL_P2P_IF_GO,
	BRCMF_FIL_P2P_IF_DYNBCN_GO,
	BRCMF_FIL_P2P_IF_DEV,
};

enum brcmf_wowl_pattern_type {
	BRCMF_WOWL_PATTERN_TYPE_BITMAP = 0,
	BRCMF_WOWL_PATTERN_TYPE_ARP,
	BRCMF_WOWL_PATTERN_TYPE_NA
};

struct brcmf_fil_p2p_if_le {
	u8 addr[ETH_ALEN];
	__le16 type;
	__le16 chspec;
};

struct brcmf_fil_chan_info_le {
	__le32 hw_channel;
	__le32 target_channel;
	__le32 scan_channel;
};

struct brcmf_fil_action_frame_le {
	u8	da[ETH_ALEN];
	__le16	len;
	__le32	packet_id;
	u8	data[BRCMF_FIL_ACTION_FRAME_SIZE];
};

struct brcmf_fil_af_params_le {
	__le32					channel;
	__le32					dwell_time;
	u8					bssid[ETH_ALEN];
	u8					pad[2];
	struct brcmf_fil_action_frame_le	action_frame;
};

struct brcmf_fil_bss_enable_le {
	__le32 bsscfgidx;
	__le32 enable;
};

struct brcmf_fil_bwcap_le {
	__le32 band;
	__le32 bw_cap;
};

 
struct brcmf_tdls_iovar_le {
	u8 ea[ETH_ALEN];		 
	u8 mode;			 
	__le16 chanspec;
	__le32 pad;			 
};

enum brcmf_tdls_manual_ep_ops {
	BRCMF_TDLS_MANUAL_EP_CREATE = 1,
	BRCMF_TDLS_MANUAL_EP_DELETE = 3,
	BRCMF_TDLS_MANUAL_EP_DISCOVERY = 6
};

 
struct brcmf_pkt_filter_pattern_le {
	 
	__le32 offset;
	 
	__le32 size_bytes;
	 
	u8 mask_and_pattern[1];
};

 
struct brcmf_pkt_filter_le {
	__le32 id;		 
	__le32 type;		 
	__le32 negate_match;	 
	union {			 
		struct brcmf_pkt_filter_pattern_le pattern;  
	} u;
};

 
struct brcmf_pkt_filter_enable_le {
	__le32 id;		 
	__le32 enable;		 
};

 
struct brcmf_bss_info_le {
	__le32 version;		 
	__le32 length;		 
	u8 BSSID[ETH_ALEN];
	__le16 beacon_period;	 
	__le16 capability;	 
	u8 SSID_len;
	u8 SSID[32];
	struct {
		__le32 count;    
		u8 rates[16];  
	} rateset;		 
	__le16 chanspec;	 
	__le16 atim_window;	 
	u8 dtim_period;	 
	__le16 RSSI;		 
	s8 phy_noise;		 

	u8 n_cap;		 
	 
	__le32 nbss_cap;
	u8 ctl_ch;		 
	__le32 reserved32[1];	 
	u8 flags;		 
	u8 reserved[3];	 
	u8 basic_mcs[BRCMF_MCSSET_LEN];	 

	__le16 ie_offset;	 
	__le32 ie_length;	 
	__le16 SNR;		 
	 
	 
};

struct brcm_rateset_le {
	 
	__le32 count;
	 
	u8 rates[BRCMF_MAXRATES_IN_SET];
};

struct brcmf_ssid_le {
	__le32 SSID_len;
	unsigned char SSID[IEEE80211_MAX_SSID_LEN];
};

 
struct brcmf_ssid8_le {
	u8 SSID_len;
	unsigned char SSID[IEEE80211_MAX_SSID_LEN];
};

struct brcmf_scan_params_le {
	struct brcmf_ssid_le ssid_le;	 
	u8 bssid[ETH_ALEN];	 
	s8 bss_type;		 
	u8 scan_type;	 
	__le32 nprobes;	   
	__le32 active_time;	 
	__le32 passive_time;	 
	__le32 home_time;	 
	__le32 channel_num;	 
	union {
		__le16 padding;	 
		DECLARE_FLEX_ARRAY(__le16, channel_list);	 
	};
};

struct brcmf_scan_params_v2_le {
	__le16 version;		 
	__le16 length;		 
	struct brcmf_ssid_le ssid_le;	 
	u8 bssid[ETH_ALEN];	 
	s8 bss_type;		 
	u8 pad;
	__le32 scan_type;	 
	__le32 nprobes;		 
	__le32 active_time;	 
	__le32 passive_time;	 
	__le32 home_time;	 
	__le32 channel_num;	 
	union {
		__le16 padding;	 
		DECLARE_FLEX_ARRAY(__le16, channel_list);	 
	};
};

struct brcmf_scan_results {
	u32 buflen;
	u32 version;
	u32 count;
	struct brcmf_bss_info_le bss_info_le[];
};

struct brcmf_escan_params_le {
	__le32 version;
	__le16 action;
	__le16 sync_id;
	union {
		struct brcmf_scan_params_le params_le;
		struct brcmf_scan_params_v2_le params_v2_le;
	};
};

struct brcmf_escan_result_le {
	__le32 buflen;
	__le32 version;
	__le16 sync_id;
	__le16 bss_count;
	struct brcmf_bss_info_le bss_info_le;
};

#define WL_ESCAN_RESULTS_FIXED_SIZE (sizeof(struct brcmf_escan_result_le) - \
	sizeof(struct brcmf_bss_info_le))

 
struct brcmf_assoc_params_le {
	 
	u8 bssid[ETH_ALEN];
	 
	__le32 chanspec_num;
	 
	__le16 chanspec_list[1];
};

 
struct brcmf_join_pref_params {
	u8 type;
	u8 len;
	u8 rssi_gain;
	u8 band;
};

 
struct brcmf_join_params {
	struct brcmf_ssid_le ssid_le;
	struct brcmf_assoc_params_le params_le;
};

 
struct brcmf_join_scan_params_le {
	u8 scan_type;		 
	__le32 nprobes;		 
	__le32 active_time;	 
	__le32 passive_time;	 
	__le32 home_time;	 
};

 
struct brcmf_ext_join_params_le {
	struct brcmf_ssid_le ssid_le;	 
	struct brcmf_join_scan_params_le scan_le;
	struct brcmf_assoc_params_le assoc_le;
};

struct brcmf_wsec_key {
	u32 index;		 
	u32 len;		 
	u8 data[WLAN_MAX_KEY_LEN];	 
	u32 pad_1[18];
	u32 algo;	 
	u32 flags;	 
	u32 pad_2[3];
	u32 iv_initialized;	 
	u32 pad_3;
	 
	struct {
		u32 hi;	 
		u16 lo;	 
	} rxiv;
	u32 pad_4[2];
	u8 ea[ETH_ALEN];	 
};

 
struct brcmf_wsec_key_le {
	__le32 index;		 
	__le32 len;		 
	u8 data[WLAN_MAX_KEY_LEN];	 
	__le32 pad_1[18];
	__le32 algo;	 
	__le32 flags;	 
	__le32 pad_2[3];
	__le32 iv_initialized;	 
	__le32 pad_3;
	 
	struct {
		__le32 hi;	 
		__le16 lo;	 
	} rxiv;
	__le32 pad_4[2];
	u8 ea[ETH_ALEN];	 
};

 
struct brcmf_wsec_pmk_le {
	__le16  key_len;
	__le16  flags;
	u8 key[2 * BRCMF_WSEC_MAX_PSK_LEN + 1];
};

 
struct brcmf_wsec_sae_pwd_le {
	__le16 key_len;
	u8 key[BRCMF_WSEC_MAX_SAE_PASSWORD_LEN];
};

 
struct brcmf_scb_val_le {
	__le32 val;
	u8 ea[ETH_ALEN];
};

 
struct brcmf_channel_info_le {
	__le32 hw_channel;
	__le32 target_channel;
	__le32 scan_channel;
};

struct brcmf_sta_info_le {
	__le16 ver;		 
	__le16 len;		 
	__le16 cap;		 
	__le32 flags;		 
	__le32 idle;		 
	u8 ea[ETH_ALEN];		 
	__le32 count;			 
	u8 rates[BRCMF_MAXRATES_IN_SET];	 
						 
	__le32 in;		 
	__le32 listen_interval_inms;  

	 
	__le32 tx_pkts;	 
	__le32 tx_failures;	 
	__le32 rx_ucast_pkts;	 
	__le32 rx_mcast_pkts;	 
	__le32 tx_rate;	 
	__le32 rx_rate;	 
	__le32 rx_decrypt_succeeds;	 
	__le32 rx_decrypt_failures;	 

	 
	__le32 tx_tot_pkts;     
	__le32 rx_tot_pkts;     
	__le32 tx_mcast_pkts;   
	__le64 tx_tot_bytes;    
	__le64 rx_tot_bytes;    
	__le64 tx_ucast_bytes;  
	__le64 tx_mcast_bytes;  
	__le64 rx_ucast_bytes;  
	__le64 rx_mcast_bytes;  
	s8 rssi[BRCMF_ANT_MAX];    
	s8 nf[BRCMF_ANT_MAX];      
	__le16 aid;                     
	__le16 ht_capabilities;         
	__le16 vht_flags;               
	__le32 tx_pkts_retry_cnt;       
	__le32 tx_pkts_retry_exhausted;  
	s8 rx_lastpkt_rssi[BRCMF_ANT_MAX];  
	 
	__le32 tx_pkts_total;           
	__le32 tx_pkts_retries;         
	__le32 tx_pkts_fw_total;        
	__le32 tx_pkts_fw_retries;      
	__le32 tx_pkts_fw_retry_exhausted;      
	__le32 rx_pkts_retried;         
	__le32 tx_rate_fallback;        

	union {
		struct {
			struct {
				__le32 count;					 
				u8 rates[BRCMF_MAXRATES_IN_SET];		 
				u8 mcs[BRCMF_MCSSET_LEN];			 
				__le16 vht_mcs[BRCMF_VHT_CAP_MCS_MAP_NSS_MAX];	 
			} rateset_adv;
		} v5;

		struct {
			__le32 rx_dur_total;	 
			__le16 chanspec;	 
			__le16 pad_1;
			struct {
				__le16 version;					 
				__le16 len;					 
				__le32 count;					 
				u8 rates[BRCMF_MAXRATES_IN_SET];		 
				u8 mcs[BRCMF_MCSSET_LEN];			 
				__le16 vht_mcs[BRCMF_VHT_CAP_MCS_MAP_NSS_MAX];	 
				__le16 he_mcs[BRCMF_HE_CAP_MCS_MAP_NSS_MAX];	 
			} rateset_adv;		 
			__le16 wpauth;		 
			u8 algo;		 
			u8 pad_2;
			__le32 tx_rspec;	 
			__le32 rx_rspec;	 
			__le32 wnm_cap;		 
		} v7;
	};
};

struct brcmf_chanspec_list {
	__le32	count;		 
	__le32  element[];	 
};

 
struct brcmf_rx_mgmt_data {
	__be16	version;
	__be16	chanspec;
	__be32	rssi;
	__be32	mactime;
	__be32	rate;
};

 
struct brcmf_fil_wowl_pattern_le {
	u8	cmd[4];
	__le32	masksize;
	__le32	offset;
	__le32	patternoffset;
	__le32	patternsize;
	__le32	id;
	__le32	reasonsize;
	__le32	type;
	 
	 
};

struct brcmf_mbss_ssid_le {
	__le32	bsscfgidx;
	__le32	SSID_len;
	unsigned char SSID[32];
};

 
struct brcmf_fil_country_le {
	char country_abbrev[BRCMF_COUNTRY_BUF_SZ];
	__le32 rev;
	char ccode[BRCMF_COUNTRY_BUF_SZ];
};

 
struct brcmf_rev_info_le {
	__le32 vendorid;
	__le32 deviceid;
	__le32 radiorev;
	__le32 chiprev;
	__le32 corerev;
	__le32 boardid;
	__le32 boardvendor;
	__le32 boardrev;
	__le32 driverrev;
	__le32 ucoderev;
	__le32 bus;
	__le32 chipnum;
	__le32 phytype;
	__le32 phyrev;
	__le32 anarev;
	__le32 chippkg;
	__le32 nvramrev;
};

 
struct brcmf_wlc_version_le {
	__le16 version;
	__le16 length;

	__le16 epi_ver_major;
	__le16 epi_ver_minor;
	__le16 epi_ver_rc;
	__le16 epi_ver_incr;

	__le16 wlc_ver_major;
	__le16 wlc_ver_minor;
};

 
struct brcmf_assoclist_le {
	__le32 count;
	u8 mac[BRCMF_MAX_ASSOCLIST][ETH_ALEN];
};

 
struct brcmf_rssi_be {
	__be32 rssi;
	__be32 snr;
	__be32 noise;
};

#define BRCMF_MAX_RSSI_LEVELS 8

 
struct brcmf_rssi_event_le {
	__le32 rate_limit_msec;
	s8 rssi_level_num;
	s8 rssi_levels[BRCMF_MAX_RSSI_LEVELS];
};

 
struct brcmf_wowl_wakeind_le {
	__le32 pci_wakeind;
	__le32 ucode_wakeind;
};

 
struct brcmf_pmksa {
	u8 bssid[ETH_ALEN];
	u8 pmkid[WLAN_PMKID_LEN];
};

 
struct brcmf_pmksa_v2 {
	__le16 length;
	u8 bssid[ETH_ALEN];
	u8 pmkid[WLAN_PMKID_LEN];
	u8 pmk[WLAN_PMK_LEN_SUITE_B_192];
	__le16 pmk_len;
	struct brcmf_ssid8_le ssid;
	u16 fils_cache_id;
};

 
struct brcmf_pmksa_v3 {
	u8 bssid[ETH_ALEN];
	u8 pmkid[WLAN_PMKID_LEN];
	u8 pmkid_len;
	u8 pmk[WLAN_PMK_LEN_SUITE_B_192];
	u8 pmk_len;
	__le16 fils_cache_id;
	u8 pad;
	struct brcmf_ssid8_le ssid;
	__le32 time_left;
};

 
struct brcmf_pmk_list_le {
	__le32 npmk;
	struct brcmf_pmksa pmk[BRCMF_MAXPMKID];
};

 
struct brcmf_pmk_list_v2_le {
	__le16 version;
	__le16 length;
	struct brcmf_pmksa_v2 pmk[BRCMF_MAXPMKID];
};

 
struct brcmf_pmk_op_v3_le {
	__le16 version;
	__le16 length;
	__le16 count;
	__le16 pad;
	struct brcmf_pmksa_v3 pmk[BRCMF_MAXPMKID];
};

 
struct brcmf_pno_param_le {
	__le32 version;
	__le32 scan_freq;
	__le32 lost_network_timeout;
	__le16 flags;
	__le16 rssi_margin;
	u8 bestn;
	u8 mscan;
	u8 repeat;
	u8 exp;
	__le32 slow_freq;
};

 
struct brcmf_pno_config_le {
	__le32  reporttype;
	__le32  channel_num;
	__le16  channel_list[BRCMF_NUMCHANNELS];
	__le32  flags;
};

 
struct brcmf_pno_net_param_le {
	struct brcmf_ssid_le ssid;
	__le32 flags;
	__le32 infra;
	__le32 auth;
	__le32 wpa_auth;
	__le32 wsec;
};

 
struct brcmf_pno_net_info_le {
	u8 bssid[ETH_ALEN];
	u8 channel;
	u8 SSID_len;
	u8 SSID[32];
	__le16	RSSI;
	__le16	timestamp;
};

 
struct brcmf_pno_scanresults_le {
	__le32 version;
	__le32 status;
	__le32 count;
};

struct brcmf_pno_scanresults_v2_le {
	__le32 version;
	__le32 status;
	__le32 count;
	__le32 scan_ch_bucket;
};

 
struct brcmf_pno_macaddr_le {
	u8 version;
	u8 flags;
	u8 mac[ETH_ALEN];
};

 
struct brcmf_dload_data_le {
	__le16 flag;
	__le16 dload_type;
	__le32 len;
	__le32 crc;
	u8 data[];
};

 
struct brcmf_pno_bssid_le {
	u8 bssid[ETH_ALEN];
	__le16 flags;
};

 
struct brcmf_pktcnt_le {
	__le32 rx_good_pkt;
	__le32 rx_bad_pkt;
	__le32 tx_good_pkt;
	__le32 tx_bad_pkt;
	__le32 rx_ocast_good_pkt;
};

 
struct brcmf_gtk_keyinfo_le {
	u8 kck[BRCMF_RSN_KCK_LENGTH];
	u8 kek[BRCMF_RSN_KEK_LENGTH];
	u8 replay_counter[BRCMF_RSN_REPLAY_LEN];
};

#define BRCMF_PNO_REPORT_NO_BATCH	BIT(2)

 
struct brcmf_gscan_bucket_config {
	u8 bucket_end_index;
	u8 bucket_freq_multiple;
	u8 flag;
	u8 reserved;
	__le16 repeat;
	__le16 max_freq_multiple;
};

 
#define BRCMF_GSCAN_CFG_VERSION                     2

 
enum brcmf_gscan_cfg_flags {
	BRCMF_GSCAN_CFG_FLAGS_ALL_RESULTS = BIT(0),
	BRCMF_GSCAN_CFG_ALL_BUCKETS_IN_1ST_SCAN = BIT(3),
	BRCMF_GSCAN_CFG_FLAGS_CHANGE_ONLY = BIT(7),
};

 
struct brcmf_gscan_config {
	__le16 version;
	u8 flags;
	u8 buffer_threshold;
	u8 swc_nbssid_threshold;
	u8 swc_rssi_window_size;
	u8 count_of_channel_buckets;
	u8 retry_threshold;
	__le16  lost_ap_window;
	struct brcmf_gscan_bucket_config bucket[];
};

 
struct brcmf_mkeep_alive_pkt_le {
	__le16  version;
	__le16  length;
	__le32  period_msec;
	__le16  len_bytes;
	u8   keep_alive_id;
	u8   data[];
} __packed;

#endif  

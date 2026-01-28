#ifndef __STA_INFO_H_
#define __STA_INFO_H_
#define IBSS_START_MAC_ID	2
#define NUM_STA 32
#define NUM_ACL 16
struct rtw_wlan_acl_node {
	struct list_head list;
	u8		 addr[ETH_ALEN];
	u8		 valid;
};
struct wlan_acl_pool {
	int mode;
	int num;
	struct rtw_wlan_acl_node aclnode[NUM_ACL];
	struct __queue	acl_node_q;
};
struct rssi_sta {
	s32	UndecoratedSmoothedPWDB;
	s32	UndecoratedSmoothedCCK;
	s32	UndecoratedSmoothedOFDM;
	u64	PacketMap;
	u8 ValidBit;
};
struct	stainfo_stats	{
	u64 rx_mgnt_pkts;
		u64 rx_beacon_pkts;
		u64 rx_probereq_pkts;
		u64 rx_probersp_pkts;
		u64 rx_probersp_bm_pkts;
		u64 rx_probersp_uo_pkts;
	u64 rx_ctrl_pkts;
	u64 rx_data_pkts;
	u64	last_rx_mgnt_pkts;
		u64 last_rx_beacon_pkts;
		u64 last_rx_probereq_pkts;
		u64 last_rx_probersp_pkts;
		u64 last_rx_probersp_bm_pkts;
		u64 last_rx_probersp_uo_pkts;
	u64	last_rx_ctrl_pkts;
	u64	last_rx_data_pkts;
	u64	rx_bytes;
	u64	rx_drops;
	u64	tx_pkts;
	u64	tx_bytes;
	u64  tx_drops;
};
struct sta_info {
	spinlock_t	lock;
	struct list_head	list;  
	struct list_head	hash_list;  
	struct adapter *padapter;
	struct sta_xmit_priv sta_xmitpriv;
	struct sta_recv_priv sta_recvpriv;
	struct __queue sleep_q;
	unsigned int sleepq_len;
	uint state;
	uint aid;
	uint mac_id;
	uint qos_option;
	u8 hwaddr[ETH_ALEN];
	uint	ieee8021x_blocked;	 
	uint	dot118021XPrivacy;  
	union Keytype	dot11tkiptxmickey;
	union Keytype	dot11tkiprxmickey;
	union Keytype	dot118021x_UncstKey;
	union pn48		dot11txpn;			 
	union pn48		dot11wtxpn;			 
	union pn48		dot11rxpn;			 
	u8 bssrateset[16];
	u32 bssratelen;
	s32  rssi;
	s32	signal_quality;
	u8 cts2self;
	u8 rtsen;
	u8 raid;
	u8 init_rate;
	u32 ra_mask;
	u8 wireless_mode;	 
	u8 bw_mode;
	u8 ldpc;
	u8 stbc;
	struct stainfo_stats sta_stats;
	struct timer_list addba_retry_timer;
	struct recv_reorder_ctrl recvreorder_ctrl[16];
	u16 BA_starting_seqctrl[16];
	struct ht_priv htpriv;
	struct list_head asoc_list;
	struct list_head auth_list;
	unsigned int expire_to;
	unsigned int auth_seq;
	unsigned int authalg;
	unsigned char chg_txt[128];
	u16 capability;
	int flags;
	int dot8021xalg; 
	int wpa_psk; 
	int wpa_group_cipher;
	int wpa2_group_cipher;
	int wpa_pairwise_cipher;
	int wpa2_pairwise_cipher;
	u8 bpairwise_key_installed;
	u8 wpa_ie[32];
	u8 nonerp_set;
	u8 no_short_slot_time_set;
	u8 no_short_preamble_set;
	u8 no_ht_gf_set;
	u8 no_ht_set;
	u8 ht_20mhz_set;
	unsigned int tx_ra_bitmap;
	u8 qos_info;
	u8 max_sp_len;
	u8 uapsd_bk; 
	u8 uapsd_be;
	u8 uapsd_vi;
	u8 uapsd_vo;
	u8 has_legacy_ac;
	unsigned int sleepq_ac_len;
	u8 under_exist_checking;
	u8 keep_alive_trycnt;
	u8 *passoc_req;
	u32 assoc_req_len;
	struct rssi_sta	 rssi_stat;
	u8 bValid;				 
	u8 IOTPeer;			 
	u8 RSSI_Path[4];		 
	u8 RSSI_Ave;
	u8 RXEVM[4];
	u8 RXSNR[4];
	u8 rssi_level;			 
	u16 RxMgmtFrameSeqNum;
};
#define sta_rx_pkts(sta) \
	(sta->sta_stats.rx_mgnt_pkts \
	+ sta->sta_stats.rx_ctrl_pkts \
	+ sta->sta_stats.rx_data_pkts)
#define sta_last_rx_pkts(sta) \
	(sta->sta_stats.last_rx_mgnt_pkts \
	+ sta->sta_stats.last_rx_ctrl_pkts \
	+ sta->sta_stats.last_rx_data_pkts)
#define sta_rx_data_pkts(sta) \
	(sta->sta_stats.rx_data_pkts)
#define sta_last_rx_data_pkts(sta) \
	(sta->sta_stats.last_rx_data_pkts)
#define sta_rx_mgnt_pkts(sta) \
	(sta->sta_stats.rx_mgnt_pkts)
#define sta_last_rx_mgnt_pkts(sta) \
	(sta->sta_stats.last_rx_mgnt_pkts)
#define sta_rx_beacon_pkts(sta) \
	(sta->sta_stats.rx_beacon_pkts)
#define sta_last_rx_beacon_pkts(sta) \
	(sta->sta_stats.last_rx_beacon_pkts)
#define sta_rx_probereq_pkts(sta) \
	(sta->sta_stats.rx_probereq_pkts)
#define sta_last_rx_probereq_pkts(sta) \
	(sta->sta_stats.last_rx_probereq_pkts)
#define sta_rx_probersp_pkts(sta) \
	(sta->sta_stats.rx_probersp_pkts)
#define sta_last_rx_probersp_pkts(sta) \
	(sta->sta_stats.last_rx_probersp_pkts)
#define sta_rx_probersp_bm_pkts(sta) \
	(sta->sta_stats.rx_probersp_bm_pkts)
#define sta_last_rx_probersp_bm_pkts(sta) \
	(sta->sta_stats.last_rx_probersp_bm_pkts)
#define sta_rx_probersp_uo_pkts(sta) \
	(sta->sta_stats.rx_probersp_uo_pkts)
#define sta_last_rx_probersp_uo_pkts(sta) \
	(sta->sta_stats.last_rx_probersp_uo_pkts)
#define sta_update_last_rx_pkts(sta) \
	do { \
		sta->sta_stats.last_rx_mgnt_pkts = sta->sta_stats.rx_mgnt_pkts; \
		sta->sta_stats.last_rx_beacon_pkts = sta->sta_stats.rx_beacon_pkts; \
		sta->sta_stats.last_rx_probereq_pkts = sta->sta_stats.rx_probereq_pkts; \
		sta->sta_stats.last_rx_probersp_pkts = sta->sta_stats.rx_probersp_pkts; \
		sta->sta_stats.last_rx_probersp_bm_pkts = sta->sta_stats.rx_probersp_bm_pkts; \
		sta->sta_stats.last_rx_probersp_uo_pkts = sta->sta_stats.rx_probersp_uo_pkts; \
		sta->sta_stats.last_rx_ctrl_pkts = sta->sta_stats.rx_ctrl_pkts; \
		sta->sta_stats.last_rx_data_pkts = sta->sta_stats.rx_data_pkts; \
	} while (0)
#define STA_RX_PKTS_ARG(sta) \
	sta->sta_stats.rx_mgnt_pkts \
	, sta->sta_stats.rx_ctrl_pkts \
	, sta->sta_stats.rx_data_pkts
#define STA_LAST_RX_PKTS_ARG(sta) \
	sta->sta_stats.last_rx_mgnt_pkts \
	, sta->sta_stats.last_rx_ctrl_pkts \
	, sta->sta_stats.last_rx_data_pkts
#define STA_RX_PKTS_DIFF_ARG(sta) \
	sta->sta_stats.rx_mgnt_pkts - sta->sta_stats.last_rx_mgnt_pkts \
	, sta->sta_stats.rx_ctrl_pkts - sta->sta_stats.last_rx_ctrl_pkts \
	, sta->sta_stats.rx_data_pkts - sta->sta_stats.last_rx_data_pkts
#define STA_PKTS_FMT "(m:%llu, c:%llu, d:%llu)"
struct	sta_priv {
	u8 *pallocated_stainfo_buf;
	u8 *pstainfo_buf;
	struct __queue	free_sta_queue;
	spinlock_t sta_hash_lock;
	struct list_head   sta_hash[NUM_STA];
	int asoc_sta_count;
	struct __queue sleep_q;
	struct __queue wakeup_q;
	struct adapter *padapter;
	struct list_head asoc_list;
	struct list_head auth_list;
	spinlock_t asoc_list_lock;
	spinlock_t auth_list_lock;
	u8 asoc_list_cnt;
	u8 auth_list_cnt;
	unsigned int auth_to;   
	unsigned int assoc_to;  
	unsigned int expire_to;  
	struct sta_info *sta_aid[NUM_STA];
	u16 sta_dz_bitmap; 
	u16 tim_bitmap; 
	u16 max_num_sta;
	struct wlan_acl_pool acl_list;
};
static inline u32 wifi_mac_hash(u8 *mac)
{
	u32 x;
	x = mac[0];
	x = (x << 2) ^ mac[1];
	x = (x << 2) ^ mac[2];
	x = (x << 2) ^ mac[3];
	x = (x << 2) ^ mac[4];
	x = (x << 2) ^ mac[5];
	x ^= x >> 8;
	x  = x & (NUM_STA - 1);
	return x;
}
extern u32 _rtw_init_sta_priv(struct sta_priv *pstapriv);
extern u32 _rtw_free_sta_priv(struct sta_priv *pstapriv);
#define stainfo_offset_valid(offset) (offset < NUM_STA && offset >= 0)
int rtw_stainfo_offset(struct sta_priv *stapriv, struct sta_info *sta);
struct sta_info *rtw_get_stainfo_by_offset(struct sta_priv *stapriv, int offset);
extern struct sta_info *rtw_alloc_stainfo(struct	sta_priv *pstapriv, u8 *hwaddr);
extern u32 rtw_free_stainfo(struct adapter *padapter, struct sta_info *psta);
extern void rtw_free_all_stainfo(struct adapter *padapter);
extern struct sta_info *rtw_get_stainfo(struct sta_priv *pstapriv, u8 *hwaddr);
extern u32 rtw_init_bcmc_stainfo(struct adapter *padapter);
extern struct sta_info *rtw_get_bcmc_stainfo(struct adapter *padapter);
extern u8 rtw_access_ctrl(struct adapter *padapter, u8 *mac_addr);
#endif  

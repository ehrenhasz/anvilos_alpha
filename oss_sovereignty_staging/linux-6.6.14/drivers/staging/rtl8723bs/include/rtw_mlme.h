 
 
#ifndef __RTW_MLME_H_
#define __RTW_MLME_H_


#define	MAX_BSS_CNT	128
 
 
#define   MAX_JOIN_TIMEOUT	6500

 
 

#define		SCANNING_TIMEOUT	8000

#ifdef PALTFORM_OS_WINCE
#define	SCANQUEUE_LIFETIME 12000000  
#else
#define	SCANQUEUE_LIFETIME 20000  
#endif

#define WIFI_NULL_STATE		0x00000000
#define WIFI_ASOC_STATE		0x00000001		 
#define WIFI_REASOC_STATE	0x00000002
#define WIFI_SLEEP_STATE	0x00000004
#define WIFI_STATION_STATE	0x00000008
#define	WIFI_AP_STATE			0x00000010
#define	WIFI_ADHOC_STATE		0x00000020
#define WIFI_ADHOC_MASTER_STATE	0x00000040
#define WIFI_UNDER_LINKING	0x00000080

#define WIFI_UNDER_WPS			0x00000100
 
 
#define	WIFI_STA_ALIVE_CHK_STATE	0x00000400
#define	WIFI_SITE_MONITOR			0x00000800		 
#ifdef WDS
#define	WIFI_WDS				0x00001000
#define	WIFI_WDS_RX_BEACON	0x00002000		 
#endif
#ifdef AUTO_CONFIG
#define	WIFI_AUTOCONF			0x00004000
#define	WIFI_AUTOCONF_IND	0x00008000
#endif

 

 
#define	WIFI_MP_STATE							0x00010000
#define	WIFI_MP_CTX_BACKGROUND				0x00020000	 
#define	WIFI_MP_CTX_ST						0x00040000	 
#define	WIFI_MP_CTX_BACKGROUND_PENDING	0x00080000	 
#define	WIFI_MP_CTX_CCK_HW					0x00100000	 
#define	WIFI_MP_CTX_CCK_CS					0x00200000	 
#define   WIFI_MP_LPBK_STATE					0x00400000
 

 
#define _FW_UNDER_LINKING	WIFI_UNDER_LINKING
#define _FW_LINKED			WIFI_ASOC_STATE
#define _FW_UNDER_SURVEY	WIFI_SITE_MONITOR


enum {
 dot11AuthAlgrthm_Open = 0,
 dot11AuthAlgrthm_Shared,
 dot11AuthAlgrthm_8021X,
 dot11AuthAlgrthm_Auto,
 dot11AuthAlgrthm_WAPI,
 dot11AuthAlgrthm_MaxNum
};

 
enum rt_scan_type {
	SCAN_PASSIVE,
	SCAN_ACTIVE,
	SCAN_MIX,
};

enum {
	GHZ24_50 = 0,
	GHZ_50,
	GHZ_24,
	GHZ_MAX,
};

#define rtw_band_valid(band) ((band) >= GHZ24_50 && (band) < GHZ_MAX)

 


#define traffic_threshold	10
#define	traffic_scan_period	500

struct sitesurvey_ctrl {
	u64	last_tx_pkts;
	uint	last_rx_pkts;
	signed int	traffic_busy;
	struct timer_list	sitesurvey_ctrl_timer;
};

struct rt_link_detect_t {
	u32 			NumTxOkInPeriod;
	u32 			NumRxOkInPeriod;
	u32 			NumRxUnicastOkInPeriod;
	bool			bBusyTraffic;
	bool			bTxBusyTraffic;
	bool			bRxBusyTraffic;
	bool			bHigherBusyTraffic;  
	bool			bHigherBusyRxTraffic;  
	bool			bHigherBusyTxTraffic;  
	 
	u8 TrafficTransitionCount;
	u32 LowPowerTransitionCount;
};

struct profile_info {
	u8 ssidlen;
	u8 ssid[WLAN_SSID_MAXLEN];
	u8 peermac[ETH_ALEN];
};

struct tx_invite_req_info {
	u8 			token;
	u8 			benable;
	u8			go_ssid[WLAN_SSID_MAXLEN];
	u8 			ssidlen;
	u8			go_bssid[ETH_ALEN];
	u8			peer_macaddr[ETH_ALEN];
	u8 			operating_ch;	 
	u8 			peer_ch;		 

};

struct tx_invite_resp_info {
	u8 			token;	 
};

struct tx_provdisc_req_info {
	u16 				wps_config_method_request;	 
	u16 				peer_channel_num[2];		 
	struct ndis_802_11_ssid	ssid;
	u8			peerDevAddr[ETH_ALEN];		 
	u8			peerIFAddr[ETH_ALEN];		 
	u8 			benable;					 
};

struct rx_provdisc_req_info {	 
	u8			peerDevAddr[ETH_ALEN];		 
	u8 			strconfig_method_desc_of_prov_disc_req[4];	 
																	 
};

struct tx_nego_req_info {
	u16 				peer_channel_num[2];		 
	u8			peerDevAddr[ETH_ALEN];		 
	u8 			benable;					 
};

struct group_id_info {
	u8			go_device_addr[ETH_ALEN];	 
	u8			ssid[WLAN_SSID_MAXLEN];	 
};

struct scan_limit_info {
	u8 			scan_op_ch_only;			 
	u8 			operation_ch[2];				 
};

struct wifidirect_info {
	struct adapter				*padapter;
	struct timer_list					find_phase_timer;
	struct timer_list					restore_p2p_state_timer;

	 
	struct timer_list					pre_tx_scan_timer;
	struct timer_list					reset_ch_sitesurvey;
	struct timer_list					reset_ch_sitesurvey2;	 
	struct tx_provdisc_req_info tx_prov_disc_info;
	struct rx_provdisc_req_info rx_prov_disc_info;
	struct tx_invite_req_info invitereq_info;
	struct profile_info		profileinfo[P2P_MAX_PERSISTENT_GROUP_NUM];	 
	struct tx_invite_resp_info inviteresp_info;
	struct tx_nego_req_info nego_req_info;
	struct group_id_info 	groupid_info;	 
	struct scan_limit_info 	rx_invitereq_info;	 
	struct scan_limit_info 	p2p_info;		 
	enum p2p_role			role;
	enum p2p_state			pre_p2p_state;
	enum p2p_state			p2p_state;
	u8 				device_addr[ETH_ALEN];	 
	u8 				interface_addr[ETH_ALEN];
	u8 				social_chan[4];
	u8 				listen_channel;
	u8 				operating_channel;
	u8 				listen_dwell;		 
	u8 				support_rate[8];
	u8 				p2p_wildcard_ssid[P2P_WILDCARD_SSID_LEN];
	u8 				intent;		 
	u8				p2p_peer_interface_addr[ETH_ALEN];
	u8				p2p_peer_device_addr[ETH_ALEN];
	u8 				peer_intent;	 
	u8				device_name[WPS_MAX_DEVICE_NAME_LEN];	 
	u8 				device_name_len;
	u8 				profileindex;	 
	u8 				peer_operating_ch;
	u8 				find_phase_state_exchange_cnt;
	u16 					device_password_id_for_nego;	 
	u8 				negotiation_dialog_token;
	u8				nego_ssid[WLAN_SSID_MAXLEN];	 
	u8 				nego_ssidlen;
	u8 				p2p_group_ssid[WLAN_SSID_MAXLEN];
	u8 				p2p_group_ssid_len;
	u8 				persistent_supported;		 
														 
														 
														 
	u8 				session_available;			 
														 
														 
														 

	u8 				wfd_tdls_enable;			 
														 
														 
	u8 				wfd_tdls_weaksec;			 
														 
														 
														 
														 
														 

	enum	p2p_wpsinfo		ui_got_wps_info;			 
	u16 					supported_wps_cm;			 
														 
	u8 				external_uuid;				 
	u8 				uuid[16];					 
	uint						channel_list_attr_len;		 
	u8 				channel_list_attr[100];		 
														 
	u8 				driver_interface;			 
};

struct tdls_ss_record {	 
	u8 macaddr[ETH_ALEN];
	u8 rx_pwd_ba11;
	u8 is_tdls_sta;	 
};

 
enum {
	RTW_ROAM_ON_EXPIRED = BIT0,
	RTW_ROAM_ON_RESUME = BIT1,
	RTW_ROAM_ACTIVE = BIT2,
};

struct mlme_priv {

	spinlock_t	lock;
	signed int	fw_state;	 
	u8 bScanInProcess;
	u8 to_join;  

	u8 to_roam;  
	struct wlan_network *roam_network;  
	u8 roam_flags;
	u8 roam_rssi_diff_th;  
	u32 roam_scan_int_ms;  
	u32 roam_scanr_exp_ms;  
	u8 roam_tgt_addr[ETH_ALEN];  

	u8 *nic_hdl;

	u8 not_indic_disco;
	struct list_head		*pscanned;
	struct __queue	free_bss_pool;
	struct __queue	scanned_queue;
	u8 *free_bss_buf;

	struct ndis_802_11_ssid	assoc_ssid;
	u8 assoc_bssid[6];

	struct wlan_network	cur_network;
	struct wlan_network *cur_network_scanned;

	 

	u32 auto_scan_int_ms;

	struct timer_list assoc_timer;

	uint assoc_by_bssid;
	uint assoc_by_rssi;

	struct timer_list scan_to_timer;  
	unsigned long scan_start_time;  

	struct timer_list set_scan_deny_timer;
	atomic_t set_scan_deny;  

	struct qos_priv qospriv;

	 
	int num_sta_no_ht;

	 
	 


	int num_FortyMHzIntolerant;

	struct ht_priv htpriv;

	struct rt_link_detect_t	LinkDetectInfo;
	struct timer_list	dynamic_chk_timer;  

	u8 acm_mask;  
	u8 ChannelPlan;
	enum rt_scan_type	scan_mode;  

	u8 *wps_probe_req_ie;
	u32 wps_probe_req_ie_len;

	 
	int num_sta_non_erp;

	 
	int num_sta_no_short_slot_time;

	 
	int num_sta_no_short_preamble;

	int olbc;  

	 
	int num_sta_ht_no_gf;

	 
	 

	 
	int num_sta_ht_20mhz;

	 
	int olbc_ht;

	u16 ht_op_mode;

	u8 *assoc_req;
	u32 assoc_req_len;
	u8 *assoc_rsp;
	u32 assoc_rsp_len;

	u8 *wps_beacon_ie;
	 
	u8 *wps_probe_resp_ie;
	u8 *wps_assoc_resp_ie;  

	u32 wps_beacon_ie_len;
	 
	u32 wps_probe_resp_ie_len;
	u32 wps_assoc_resp_ie_len;  

	u8 *p2p_beacon_ie;
	u8 *p2p_probe_req_ie;
	u8 *p2p_probe_resp_ie;
	u8 *p2p_go_probe_resp_ie;  
	u8 *p2p_assoc_req_ie;

	u32 p2p_beacon_ie_len;
	u32 p2p_probe_req_ie_len;
	u32 p2p_probe_resp_ie_len;
	u32 p2p_go_probe_resp_ie_len;  
	u32 p2p_assoc_req_ie_len;

	spinlock_t	bcn_update_lock;
	u8 update_bcn;

	u8 NumOfBcnInfoChkFail;
	unsigned long	timeBcnInfoChkStart;
};

#define rtw_mlme_set_auto_scan_int(adapter, ms) \
	do { \
		adapter->mlmepriv.auto_scan_int_ms = ms; \
	while (0)

void rtw_mlme_reset_auto_scan_int(struct adapter *adapter);

struct hostapd_priv {
	struct adapter *padapter;
};

extern int hostapd_mode_init(struct adapter *padapter);
extern void hostapd_mode_unload(struct adapter *padapter);

extern void rtw_joinbss_event_prehandle(struct adapter *adapter, u8 *pbuf);
extern void rtw_survey_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_surveydone_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_joinbss_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_stassoc_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_stadel_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_atimdone_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_cpwm_event_callback(struct adapter *adapter, u8 *pbuf);
extern void rtw_wmm_event_callback(struct adapter *padapter, u8 *pbuf);

extern void rtw_join_timeout_handler(struct timer_list *t);
extern void _rtw_scan_timeout_handler(struct timer_list *t);

int event_thread(void *context);

extern void rtw_free_network_queue(struct adapter *adapter, u8 isfreeall);
extern int rtw_init_mlme_priv(struct adapter *adapter); 

extern void rtw_free_mlme_priv(struct mlme_priv *pmlmepriv);


extern signed int rtw_select_and_join_from_scanned_queue(struct mlme_priv *pmlmepriv);
extern signed int rtw_set_key(struct adapter *adapter, struct security_priv *psecuritypriv, signed int keyid, u8 set_tx, bool enqueue);
extern signed int rtw_set_auth(struct adapter *adapter, struct security_priv *psecuritypriv);

static inline u8 *get_bssid(struct mlme_priv *pmlmepriv)
{	 
	 
	return pmlmepriv->cur_network.network.mac_address;
}

static inline signed int check_fwstate(struct mlme_priv *pmlmepriv, signed int state)
{
	if (pmlmepriv->fw_state & state)
		return true;

	return false;
}

static inline signed int get_fwstate(struct mlme_priv *pmlmepriv)
{
	return pmlmepriv->fw_state;
}

 
static inline void set_fwstate(struct mlme_priv *pmlmepriv, signed int state)
{
	pmlmepriv->fw_state |= state;
	 
	if (state == _FW_UNDER_SURVEY)
		pmlmepriv->bScanInProcess = true;
}

static inline void _clr_fwstate_(struct mlme_priv *pmlmepriv, signed int state)
{
	pmlmepriv->fw_state &= ~state;
	 
	if (state == _FW_UNDER_SURVEY)
		pmlmepriv->bScanInProcess = false;
}

extern u16 rtw_get_capability(struct wlan_bssid_ex *bss);
extern void rtw_update_scanned_network(struct adapter *adapter, struct wlan_bssid_ex *target);
extern void rtw_disconnect_hdl_under_linked(struct adapter *adapter, struct sta_info *psta, u8 free_assoc);
extern void rtw_generate_random_ibss(u8 *pibss);
extern struct wlan_network *rtw_find_network(struct __queue *scanned_queue, u8 *addr);
extern struct wlan_network *rtw_get_oldest_wlan_network(struct __queue *scanned_queue);
struct wlan_network *_rtw_find_same_network(struct __queue *scanned_queue, struct wlan_network *network);

extern void rtw_free_assoc_resources(struct adapter *adapter, int lock_scanned_queue);
extern void rtw_indicate_disconnect(struct adapter *adapter);
extern void rtw_indicate_connect(struct adapter *adapter);
void rtw_indicate_scan_done(struct adapter *padapter, bool aborted);
void rtw_scan_abort(struct adapter *adapter);

extern int rtw_restruct_sec_ie(struct adapter *adapter, u8 *in_ie, u8 *out_ie, uint in_len);
extern int rtw_restruct_wmm_ie(struct adapter *adapter, u8 *in_ie, u8 *out_ie, uint in_len, uint initial_out_len);
extern void rtw_init_registrypriv_dev_network(struct adapter *adapter);

extern void rtw_update_registrypriv_dev_network(struct adapter *adapter);

extern void rtw_get_encrypt_decrypt_from_registrypriv(struct adapter *adapter);

extern void _rtw_join_timeout_handler(struct timer_list *t);
extern void rtw_scan_timeout_handler(struct timer_list *t);

extern void rtw_dynamic_check_timer_handler(struct adapter *adapter);
bool rtw_is_scan_deny(struct adapter *adapter);
void rtw_clear_scan_deny(struct adapter *adapter);
void rtw_set_scan_deny(struct adapter *adapter, u32 ms);

void rtw_free_mlme_priv_ie_data(struct mlme_priv *pmlmepriv);

extern void _rtw_free_mlme_priv(struct mlme_priv *pmlmepriv);

 

extern struct wlan_network *rtw_alloc_network(struct mlme_priv *pmlmepriv);


extern void _rtw_free_network(struct mlme_priv *pmlmepriv, struct wlan_network *pnetwork, u8 isfreeall);
extern void _rtw_free_network_nolock(struct mlme_priv *pmlmepriv, struct wlan_network *pnetwork);


extern struct wlan_network *_rtw_find_network(struct __queue *scanned_queue, u8 *addr);

extern signed int rtw_if_up(struct adapter *padapter);

signed int rtw_linked_check(struct adapter *padapter);

u8 *rtw_get_capability_from_ie(u8 *ie);
u8 *rtw_get_beacon_interval_from_ie(u8 *ie);


void rtw_joinbss_reset(struct adapter *padapter);

void rtw_ht_use_default_setting(struct adapter *padapter);
void rtw_build_wmm_ie_ht(struct adapter *padapter, u8 *out_ie, uint *pout_len);
unsigned int rtw_restructure_ht_ie(struct adapter *padapter, u8 *in_ie, u8 *out_ie, uint in_len, uint *pout_len, u8 channel);
void rtw_update_ht_cap(struct adapter *padapter, u8 *pie, uint ie_len, u8 channel);
void rtw_issue_addbareq_cmd(struct adapter *padapter, struct xmit_frame *pxmitframe);
void rtw_append_exented_cap(struct adapter *padapter, u8 *out_ie, uint *pout_len);

int rtw_is_same_ibss(struct adapter *adapter, struct wlan_network *pnetwork);
int is_same_network(struct wlan_bssid_ex *src, struct wlan_bssid_ex *dst, u8 feature);

#define rtw_roam_flags(adapter) ((adapter)->mlmepriv.roam_flags)
#define rtw_chk_roam_flags(adapter, flags) ((adapter)->mlmepriv.roam_flags & flags)
#define rtw_clr_roam_flags(adapter, flags) \
	do { \
		((adapter)->mlmepriv.roam_flags &= ~flags); \
	} while (0)

#define rtw_set_roam_flags(adapter, flags) \
	do { \
		((adapter)->mlmepriv.roam_flags |= flags); \
	} while (0)

#define rtw_assign_roam_flags(adapter, flags) \
	do { \
		((adapter)->mlmepriv.roam_flags = flags); \
	} while (0)

void _rtw_roaming(struct adapter *adapter, struct wlan_network *tgt_network);
void rtw_roaming(struct adapter *adapter, struct wlan_network *tgt_network);
void rtw_set_to_roam(struct adapter *adapter, u8 to_roam);
u8 rtw_dec_to_roam(struct adapter *adapter);
u8 rtw_to_roam(struct adapter *adapter);
int rtw_select_roaming_candidate(struct mlme_priv *pmlmepriv);

void rtw_sta_media_status_rpt(struct adapter *adapter, struct sta_info *psta, u32 mstatus);

#endif  

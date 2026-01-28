#ifndef IEEE80211_I_H
#define IEEE80211_I_H
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <linux/leds.h>
#include <linux/idr.h>
#include <linux/rhashtable.h>
#include <linux/rbtree.h>
#include <net/ieee80211_radiotap.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <net/fq.h>
#include "key.h"
#include "sta_info.h"
#include "debug.h"
#include "drop.h"
extern const struct cfg80211_ops mac80211_config_ops;
struct ieee80211_local;
struct ieee80211_mesh_fast_tx;
#define AP_MAX_BC_BUFFER 128
#define TOTAL_MAX_TX_BUFFER 512
#define IEEE80211_ENCRYPT_HEADROOM 8
#define IEEE80211_ENCRYPT_TAILROOM 18
#define IEEE80211_UNSET_POWER_LEVEL	INT_MIN
#define IEEE80211_DEFAULT_UAPSD_QUEUES 0
#define IEEE80211_DEFAULT_MAX_SP_LEN		\
	IEEE80211_WMM_IE_STA_QOSINFO_SP_ALL
extern const u8 ieee80211_ac_to_qos_mask[IEEE80211_NUM_ACS];
#define IEEE80211_DEAUTH_FRAME_LEN	(24   + 2  )
#define IEEE80211_MAX_NAN_INSTANCE_ID 255
#define AIRTIME_ACTIVE_DURATION (HZ / 10)
struct ieee80211_bss {
	u32 device_ts_beacon, device_ts_presp;
	bool wmm_used;
	bool uapsd_supported;
#define IEEE80211_MAX_SUPP_RATES 32
	u8 supp_rates[IEEE80211_MAX_SUPP_RATES];
	size_t supp_rates_len;
	struct ieee80211_rate *beacon_rate;
	u32 vht_cap_info;
	bool has_erp_value;
	u8 erp_value;
	u8 corrupt_data;
	u8 valid_data;
};
enum ieee80211_bss_corrupt_data_flags {
	IEEE80211_BSS_CORRUPT_BEACON		= BIT(0),
	IEEE80211_BSS_CORRUPT_PROBE_RESP	= BIT(1)
};
enum ieee80211_bss_valid_data_flags {
	IEEE80211_BSS_VALID_WMM			= BIT(1),
	IEEE80211_BSS_VALID_RATES		= BIT(2),
	IEEE80211_BSS_VALID_ERP			= BIT(3)
};
typedef unsigned __bitwise ieee80211_tx_result;
#define TX_CONTINUE	((__force ieee80211_tx_result) 0u)
#define TX_DROP		((__force ieee80211_tx_result) 1u)
#define TX_QUEUED	((__force ieee80211_tx_result) 2u)
#define IEEE80211_TX_UNICAST		BIT(1)
#define IEEE80211_TX_PS_BUFFERED	BIT(2)
struct ieee80211_tx_data {
	struct sk_buff *skb;
	struct sk_buff_head skbs;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;
	struct ieee80211_tx_rate rate;
	unsigned int flags;
};
enum ieee80211_packet_rx_flags {
	IEEE80211_RX_AMSDU			= BIT(3),
	IEEE80211_RX_MALFORMED_ACTION_FRM	= BIT(4),
	IEEE80211_RX_DEFERRED_RELEASE		= BIT(5),
};
enum ieee80211_rx_flags {
	IEEE80211_RX_CMNTR		= BIT(0),
	IEEE80211_RX_BEACON_REPORTED	= BIT(1),
};
struct ieee80211_rx_data {
	struct list_head *list;
	struct sk_buff *skb;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_link_data *link;
	struct sta_info *sta;
	struct link_sta_info *link_sta;
	struct ieee80211_key *key;
	unsigned int flags;
	int seqno_idx;
	int security_idx;
	int link_id;
	union {
		struct {
			u32 iv32;
			u16 iv16;
		} tkip;
		struct {
			u8 pn[IEEE80211_CCMP_PN_LEN];
		} ccm_gcm;
	};
};
struct ieee80211_csa_settings {
	const u16 *counter_offsets_beacon;
	const u16 *counter_offsets_presp;
	int n_counter_offsets_beacon;
	int n_counter_offsets_presp;
	u8 count;
};
struct ieee80211_color_change_settings {
	u16 counter_offset_beacon;
	u16 counter_offset_presp;
	u8 count;
};
struct beacon_data {
	u8 *head, *tail;
	int head_len, tail_len;
	struct ieee80211_meshconf_ie *meshconf;
	u16 cntdwn_counter_offsets[IEEE80211_MAX_CNTDWN_COUNTERS_NUM];
	u8 cntdwn_current_counter;
	struct cfg80211_mbssid_elems *mbssid_ies;
	struct cfg80211_rnr_elems *rnr_ies;
	struct rcu_head rcu_head;
};
struct probe_resp {
	struct rcu_head rcu_head;
	int len;
	u16 cntdwn_counter_offsets[IEEE80211_MAX_CNTDWN_COUNTERS_NUM];
	u8 data[];
};
struct fils_discovery_data {
	struct rcu_head rcu_head;
	int len;
	u8 data[];
};
struct unsol_bcast_probe_resp_data {
	struct rcu_head rcu_head;
	int len;
	u8 data[];
};
struct ps_data {
	u8 tim[sizeof(unsigned long) * BITS_TO_LONGS(IEEE80211_MAX_AID + 1)]
			__aligned(__alignof__(unsigned long));
	struct sk_buff_head bc_buf;
	atomic_t num_sta_ps;  
	int dtim_count;
	bool dtim_bc_mc;
};
struct ieee80211_if_ap {
	struct list_head vlans;  
	struct ps_data ps;
	atomic_t num_mcast_sta;  
	bool multicast_to_unicast;
	bool active;
};
struct ieee80211_if_vlan {
	struct list_head list;  
	struct sta_info __rcu *sta;
	atomic_t num_mcast_sta;  
};
struct mesh_stats {
	__u32 fwded_mcast;		 
	__u32 fwded_unicast;		 
	__u32 fwded_frames;		 
	__u32 dropped_frames_ttl;	 
	__u32 dropped_frames_no_route;	 
};
#define PREQ_Q_F_START		0x1
#define PREQ_Q_F_REFRESH	0x2
struct mesh_preq_queue {
	struct list_head list;
	u8 dst[ETH_ALEN];
	u8 flags;
};
struct ieee80211_roc_work {
	struct list_head list;
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_channel *chan;
	bool started, abort, hw_begun, notified;
	bool on_channel;
	unsigned long start_time;
	u32 duration, req_duration;
	struct sk_buff *frame;
	u64 cookie, mgmt_tx_cookie;
	enum ieee80211_roc_type type;
};
enum ieee80211_sta_flags {
	IEEE80211_STA_CONNECTION_POLL	= BIT(1),
	IEEE80211_STA_CONTROL_PORT	= BIT(2),
	IEEE80211_STA_MFP_ENABLED	= BIT(6),
	IEEE80211_STA_UAPSD_ENABLED	= BIT(7),
	IEEE80211_STA_NULLFUNC_ACKED	= BIT(8),
	IEEE80211_STA_ENABLE_RRM	= BIT(15),
};
typedef u32 __bitwise ieee80211_conn_flags_t;
enum ieee80211_conn_flags {
	IEEE80211_CONN_DISABLE_HT	= (__force ieee80211_conn_flags_t)BIT(0),
	IEEE80211_CONN_DISABLE_40MHZ	= (__force ieee80211_conn_flags_t)BIT(1),
	IEEE80211_CONN_DISABLE_VHT	= (__force ieee80211_conn_flags_t)BIT(2),
	IEEE80211_CONN_DISABLE_80P80MHZ	= (__force ieee80211_conn_flags_t)BIT(3),
	IEEE80211_CONN_DISABLE_160MHZ	= (__force ieee80211_conn_flags_t)BIT(4),
	IEEE80211_CONN_DISABLE_HE	= (__force ieee80211_conn_flags_t)BIT(5),
	IEEE80211_CONN_DISABLE_EHT	= (__force ieee80211_conn_flags_t)BIT(6),
	IEEE80211_CONN_DISABLE_320MHZ	= (__force ieee80211_conn_flags_t)BIT(7),
};
struct ieee80211_mgd_auth_data {
	struct cfg80211_bss *bss;
	unsigned long timeout;
	int tries;
	u16 algorithm, expected_transaction;
	u8 key[WLAN_KEY_LEN_WEP104];
	u8 key_len, key_idx;
	bool done, waiting;
	bool peer_confirmed;
	bool timeout_started;
	int link_id;
	u8 ap_addr[ETH_ALEN] __aligned(2);
	u16 sae_trans, sae_status;
	size_t data_len;
	u8 data[];
};
struct ieee80211_mgd_assoc_data {
	struct {
		struct cfg80211_bss *bss;
		u8 addr[ETH_ALEN] __aligned(2);
		u8 ap_ht_param;
		struct ieee80211_vht_cap ap_vht_cap;
		size_t elems_len;
		u8 *elems;  
		ieee80211_conn_flags_t conn_flags;
		u16 status;
		bool disabled;
	} link[IEEE80211_MLD_MAX_NUM_LINKS];
	u8 ap_addr[ETH_ALEN] __aligned(2);
	const u8 *supp_rates;
	u8 supp_rates_len;
	unsigned long timeout;
	int tries;
	u8 prev_ap_addr[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len;
	bool wmm, uapsd;
	bool need_beacon;
	bool synced;
	bool timeout_started;
	bool s1g;
	unsigned int assoc_link_id;
	u8 fils_nonces[2 * FILS_NONCE_LEN];
	u8 fils_kek[FILS_MAX_KEK_LEN];
	size_t fils_kek_len;
	size_t ie_len;
	u8 *ie_pos;  
	u8 ie[];
};
struct ieee80211_sta_tx_tspec {
	unsigned long time_slice_start;
	u32 admitted_time;  
	u8 tsid;
	s8 up;  
	u32 consumed_tx_time;
	enum {
		TX_TSPEC_ACTION_NONE = 0,
		TX_TSPEC_ACTION_DOWNGRADE,
		TX_TSPEC_ACTION_STOP_DOWNGRADE,
	} action;
	bool downgraded;
};
DECLARE_EWMA(beacon_signal, 4, 4)
struct ieee80211_if_managed {
	struct timer_list timer;
	struct timer_list conn_mon_timer;
	struct timer_list bcn_mon_timer;
	struct work_struct monitor_work;
	struct wiphy_work beacon_connection_loss_work;
	struct wiphy_work csa_connection_drop_work;
	unsigned long beacon_timeout;
	unsigned long probe_timeout;
	int probe_send_count;
	bool nullfunc_failed;
	u8 connection_loss:1,
	   driver_disconnect:1,
	   reconnect:1,
	   associated:1;
	struct ieee80211_mgd_auth_data *auth_data;
	struct ieee80211_mgd_assoc_data *assoc_data;
	bool powersave;  
	bool broken_ap;  
	unsigned int flags;
	bool status_acked;
	bool status_received;
	__le16 status_fc;
	enum {
		IEEE80211_MFP_DISABLED,
		IEEE80211_MFP_OPTIONAL,
		IEEE80211_MFP_REQUIRED
	} mfp;  
	unsigned int uapsd_queues;
	unsigned int uapsd_max_sp_len;
	u8 use_4addr;
	int rssi_min_thold, rssi_max_thold;
	struct ieee80211_ht_cap ht_capa;  
	struct ieee80211_ht_cap ht_capa_mask;  
	struct ieee80211_vht_cap vht_capa;  
	struct ieee80211_vht_cap vht_capa_mask;  
	struct ieee80211_s1g_cap s1g_capa;  
	struct ieee80211_s1g_cap s1g_capa_mask;  
	u8 tdls_peer[ETH_ALEN] __aligned(2);
	struct delayed_work tdls_peer_del_work;
	struct sk_buff *orig_teardown_skb;  
	struct sk_buff *teardown_skb;  
	spinlock_t teardown_lock;  
	bool tdls_wider_bw_prohibited;
	struct ieee80211_sta_tx_tspec tx_tspec[IEEE80211_NUM_ACS];
	struct delayed_work tx_tspec_wk;
	u8 *assoc_req_ies;
	size_t assoc_req_ies_len;
	struct wiphy_delayed_work ml_reconf_work;
	u16 removed_links;
};
struct ieee80211_if_ibss {
	struct timer_list timer;
	struct wiphy_work csa_connection_drop_work;
	unsigned long last_scan_completed;
	u32 basic_rates;
	bool fixed_bssid;
	bool fixed_channel;
	bool privacy;
	bool control_port;
	bool userspace_handles_dfs;
	u8 bssid[ETH_ALEN] __aligned(2);
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len, ie_len;
	u8 *ie;
	struct cfg80211_chan_def chandef;
	unsigned long ibss_join_req;
	struct beacon_data __rcu *presp;
	struct ieee80211_ht_cap ht_capa;  
	struct ieee80211_ht_cap ht_capa_mask;  
	spinlock_t incomplete_lock;
	struct list_head incomplete_stations;
	enum {
		IEEE80211_IBSS_MLME_SEARCH,
		IEEE80211_IBSS_MLME_JOINED,
	} state;
};
struct ieee80211_if_ocb {
	struct timer_list housekeeping_timer;
	unsigned long wrkq_flags;
	spinlock_t incomplete_lock;
	struct list_head incomplete_stations;
	bool joined;
};
struct ieee802_11_elems;
struct ieee80211_mesh_sync_ops {
	void (*rx_bcn_presp)(struct ieee80211_sub_if_data *sdata, u16 stype,
			     struct ieee80211_mgmt *mgmt, unsigned int len,
			     const struct ieee80211_meshconf_ie *mesh_cfg,
			     struct ieee80211_rx_status *rx_status);
	void (*adjust_tsf)(struct ieee80211_sub_if_data *sdata,
			   struct beacon_data *beacon);
};
struct mesh_csa_settings {
	struct rcu_head rcu_head;
	struct cfg80211_csa_settings settings;
};
struct mesh_table {
	struct hlist_head known_gates;
	spinlock_t gates_lock;
	struct rhashtable rhead;
	struct hlist_head walk_head;
	spinlock_t walk_lock;
	atomic_t entries;		 
};
struct mesh_tx_cache {
	struct rhashtable rht;
	struct hlist_head walk_head;
	spinlock_t walk_lock;
};
struct ieee80211_if_mesh {
	struct timer_list housekeeping_timer;
	struct timer_list mesh_path_timer;
	struct timer_list mesh_path_root_timer;
	unsigned long wrkq_flags;
	unsigned long mbss_changed[64 / BITS_PER_LONG];
	bool userspace_handles_dfs;
	u8 mesh_id[IEEE80211_MAX_MESH_ID_LEN];
	size_t mesh_id_len;
	u8 mesh_pp_id;
	u8 mesh_pm_id;
	u8 mesh_cc_id;
	u8 mesh_sp_id;
	u8 mesh_auth_id;
	u32 sn;
	u32 preq_id;
	atomic_t mpaths;
	unsigned long last_sn_update;
	unsigned long next_perr;
	unsigned long last_preq;
	struct mesh_rmc *rmc;
	spinlock_t mesh_preq_queue_lock;
	struct mesh_preq_queue preq_queue;
	int preq_queue_len;
	struct mesh_stats mshstats;
	struct mesh_config mshcfg;
	atomic_t estab_plinks;
	atomic_t mesh_seqnum;
	bool accepting_plinks;
	int num_gates;
	struct beacon_data __rcu *beacon;
	const u8 *ie;
	u8 ie_len;
	enum {
		IEEE80211_MESH_SEC_NONE = 0x0,
		IEEE80211_MESH_SEC_AUTHED = 0x1,
		IEEE80211_MESH_SEC_SECURED = 0x2,
	} security;
	bool user_mpm;
	const struct ieee80211_mesh_sync_ops *sync_ops;
	s64 sync_offset_clockdrift_max;
	spinlock_t sync_offset_lock;
	enum nl80211_mesh_power_mode nonpeer_pm;
	int ps_peers_light_sleep;
	int ps_peers_deep_sleep;
	struct ps_data ps;
	struct mesh_csa_settings __rcu *csa;
	enum {
		IEEE80211_MESH_CSA_ROLE_NONE,
		IEEE80211_MESH_CSA_ROLE_INIT,
		IEEE80211_MESH_CSA_ROLE_REPEATER,
	} csa_role;
	u8 chsw_ttl;
	u16 pre_value;
	int meshconf_offset;
	struct mesh_table mesh_paths;
	struct mesh_table mpp_paths;  
	int mesh_paths_generation;
	int mpp_paths_generation;
	struct mesh_tx_cache tx_cache;
};
#ifdef CONFIG_MAC80211_MESH
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name)	\
	do { (msh)->mshstats.name++; } while (0)
#else
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name) \
	do { } while (0)
#endif
enum ieee80211_sub_if_data_flags {
	IEEE80211_SDATA_ALLMULTI		= BIT(0),
	IEEE80211_SDATA_DONT_BRIDGE_PACKETS	= BIT(3),
	IEEE80211_SDATA_DISCONNECT_RESUME	= BIT(4),
	IEEE80211_SDATA_IN_DRIVER		= BIT(5),
	IEEE80211_SDATA_DISCONNECT_HW_RESTART	= BIT(6),
};
enum ieee80211_sdata_state_bits {
	SDATA_STATE_RUNNING,
	SDATA_STATE_OFFCHANNEL,
	SDATA_STATE_OFFCHANNEL_BEACON_STOPPED,
};
enum ieee80211_chanctx_mode {
	IEEE80211_CHANCTX_SHARED,
	IEEE80211_CHANCTX_EXCLUSIVE
};
enum ieee80211_chanctx_replace_state {
	IEEE80211_CHANCTX_REPLACE_NONE,
	IEEE80211_CHANCTX_WILL_BE_REPLACED,
	IEEE80211_CHANCTX_REPLACES_OTHER,
};
struct ieee80211_chanctx {
	struct list_head list;
	struct rcu_head rcu_head;
	struct list_head assigned_links;
	struct list_head reserved_links;
	enum ieee80211_chanctx_replace_state replace_state;
	struct ieee80211_chanctx *replace_ctx;
	enum ieee80211_chanctx_mode mode;
	bool driver_present;
	struct ieee80211_chanctx_conf conf;
};
struct mac80211_qos_map {
	struct cfg80211_qos_map qos_map;
	struct rcu_head rcu_head;
};
enum txq_info_flags {
	IEEE80211_TXQ_STOP,
	IEEE80211_TXQ_AMPDU,
	IEEE80211_TXQ_NO_AMSDU,
	IEEE80211_TXQ_DIRTY,
};
struct txq_info {
	struct fq_tin tin;
	struct codel_vars def_cvars;
	struct codel_stats cstats;
	u16 schedule_round;
	struct list_head schedule_order;
	struct sk_buff_head frags;
	unsigned long flags;
	struct ieee80211_txq txq;
};
struct ieee80211_if_mntr {
	u32 flags;
	u8 mu_follow_addr[ETH_ALEN] __aligned(2);
	struct list_head list;
};
struct ieee80211_if_nan {
	struct cfg80211_nan_conf conf;
	spinlock_t func_lock;
	struct idr function_inst_ids;
};
struct ieee80211_link_data_managed {
	u8 bssid[ETH_ALEN] __aligned(2);
	u8 dtim_period;
	enum ieee80211_smps_mode req_smps,  
				 driver_smps_mode;  
	ieee80211_conn_flags_t conn_flags;
	s16 p2p_noa_index;
	bool tdls_chan_switch_prohibited;
	bool have_beacon;
	bool tracking_signal_avg;
	bool disable_wmm_tracking;
	bool operating_11g_mode;
	bool csa_waiting_bcn;
	bool csa_ignored_same_chan;
	struct wiphy_delayed_work chswitch_work;
	struct wiphy_work request_smps_work;
	bool beacon_crc_valid;
	u32 beacon_crc;
	struct ewma_beacon_signal ave_beacon_signal;
	int last_ave_beacon_signal;
	unsigned int count_beacon_signal;
	unsigned int beacon_loss_count;
	int last_cqm_event_signal;
	int wmm_last_param_set;
	int mu_edca_last_param_set;
	u8 bss_param_ch_cnt;
	struct cfg80211_bss *bss;
};
struct ieee80211_link_data_ap {
	struct beacon_data __rcu *beacon;
	struct probe_resp __rcu *probe_resp;
	struct fils_discovery_data __rcu *fils_discovery;
	struct unsol_bcast_probe_resp_data __rcu *unsol_bcast_probe_resp;
	struct cfg80211_beacon_data *next_beacon;
};
struct ieee80211_link_data {
	struct ieee80211_sub_if_data *sdata;
	unsigned int link_id;
	struct list_head assigned_chanctx_list;  
	struct list_head reserved_chanctx_list;  
	struct ieee80211_key __rcu *gtk[NUM_DEFAULT_KEYS +
					NUM_DEFAULT_MGMT_KEYS +
					NUM_DEFAULT_BEACON_KEYS];
	struct ieee80211_key __rcu *default_multicast_key;
	struct ieee80211_key __rcu *default_mgmt_key;
	struct ieee80211_key __rcu *default_beacon_key;
	struct work_struct csa_finalize_work;
	bool csa_block_tx;  
	bool operating_11g_mode;
	struct cfg80211_chan_def csa_chandef;
	struct work_struct color_change_finalize_work;
	struct delayed_work color_collision_detect_work;
	u64 color_bitmap;
	struct ieee80211_chanctx *reserved_chanctx;
	struct cfg80211_chan_def reserved_chandef;
	bool reserved_radar_required;
	bool reserved_ready;
	u8 needed_rx_chains;
	enum ieee80211_smps_mode smps_mode;
	int user_power_level;  
	int ap_power_level;  
	bool radar_required;
	struct delayed_work dfs_cac_timer_work;
	union {
		struct ieee80211_link_data_managed mgd;
		struct ieee80211_link_data_ap ap;
	} u;
	struct ieee80211_tx_queue_params tx_conf[IEEE80211_NUM_ACS];
	struct ieee80211_bss_conf *conf;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *debugfs_dir;
#endif
};
struct ieee80211_sub_if_data {
	struct list_head list;
	struct wireless_dev wdev;
	struct list_head key_list;
	int crypto_tx_tailroom_needed_cnt;
	int crypto_tx_tailroom_pending_dec;
	struct delayed_work dec_tailroom_needed_wk;
	struct net_device *dev;
	struct ieee80211_local *local;
	unsigned int flags;
	unsigned long state;
	char name[IFNAMSIZ];
	struct ieee80211_fragment_cache frags;
	u16 noack_map;
	u8 wmm_acm;
	struct ieee80211_key __rcu *keys[NUM_DEFAULT_KEYS];
	struct ieee80211_key __rcu *default_unicast_key;
	u16 sequence_number;
	u16 mld_mcast_seq;
	__be16 control_port_protocol;
	bool control_port_no_encrypt;
	bool control_port_no_preauth;
	bool control_port_over_nl80211;
	atomic_t num_tx_queued;
	struct mac80211_qos_map __rcu *qos_map;
	struct work_struct recalc_smps;
	struct wiphy_work work;
	struct sk_buff_head skb_queue;
	struct sk_buff_head status_queue;
	struct ieee80211_if_ap *bss;
	u32 rc_rateidx_mask[NUM_NL80211_BANDS];
	bool rc_has_mcs_mask[NUM_NL80211_BANDS];
	u8  rc_rateidx_mcs_mask[NUM_NL80211_BANDS][IEEE80211_HT_MCS_MASK_LEN];
	bool rc_has_vht_mcs_mask[NUM_NL80211_BANDS];
	u16 rc_rateidx_vht_mcs_mask[NUM_NL80211_BANDS][NL80211_VHT_NSS_MAX];
	u32 beacon_rateidx_mask[NUM_NL80211_BANDS];
	bool beacon_rate_set;
	union {
		struct ieee80211_if_ap ap;
		struct ieee80211_if_vlan vlan;
		struct ieee80211_if_managed mgd;
		struct ieee80211_if_ibss ibss;
		struct ieee80211_if_mesh mesh;
		struct ieee80211_if_ocb ocb;
		struct ieee80211_if_mntr mntr;
		struct ieee80211_if_nan nan;
	} u;
	struct ieee80211_link_data deflink;
	struct ieee80211_link_data __rcu *link[IEEE80211_MLD_MAX_NUM_LINKS];
	struct work_struct activate_links_work;
	u16 desired_active_links;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct {
		struct dentry *subdir_stations;
		struct dentry *default_unicast_key;
		struct dentry *default_multicast_key;
		struct dentry *default_mgmt_key;
		struct dentry *default_beacon_key;
	} debugfs;
#endif
	struct ieee80211_vif vif;
};
static inline
struct ieee80211_sub_if_data *vif_to_sdata(struct ieee80211_vif *p)
{
	return container_of(p, struct ieee80211_sub_if_data, vif);
}
static inline void sdata_lock(struct ieee80211_sub_if_data *sdata)
	__acquires(&sdata->wdev.mtx)
{
	mutex_lock(&sdata->wdev.mtx);
	__acquire(&sdata->wdev.mtx);
}
static inline void sdata_unlock(struct ieee80211_sub_if_data *sdata)
	__releases(&sdata->wdev.mtx)
{
	mutex_unlock(&sdata->wdev.mtx);
	__release(&sdata->wdev.mtx);
}
#define sdata_dereference(p, sdata) \
	rcu_dereference_protected(p, lockdep_is_held(&sdata->wdev.mtx))
static inline void
sdata_assert_lock(struct ieee80211_sub_if_data *sdata)
{
	lockdep_assert_held(&sdata->wdev.mtx);
}
static inline int
ieee80211_chanwidth_get_shift(enum nl80211_chan_width width)
{
	switch (width) {
	case NL80211_CHAN_WIDTH_5:
		return 2;
	case NL80211_CHAN_WIDTH_10:
		return 1;
	default:
		return 0;
	}
}
static inline int
ieee80211_chandef_get_shift(struct cfg80211_chan_def *chandef)
{
	return ieee80211_chanwidth_get_shift(chandef->width);
}
static inline int
ieee80211_vif_get_shift(struct ieee80211_vif *vif)
{
	struct ieee80211_chanctx_conf *chanctx_conf;
	int shift = 0;
	rcu_read_lock();
	chanctx_conf = rcu_dereference(vif->bss_conf.chanctx_conf);
	if (chanctx_conf)
		shift = ieee80211_chandef_get_shift(&chanctx_conf->def);
	rcu_read_unlock();
	return shift;
}
static inline int
ieee80211_get_mbssid_beacon_len(struct cfg80211_mbssid_elems *elems,
				struct cfg80211_rnr_elems *rnr_elems,
				u8 i)
{
	int len = 0;
	if (!elems || !elems->cnt || i > elems->cnt)
		return 0;
	if (i < elems->cnt) {
		len = elems->elem[i].len;
		if (rnr_elems) {
			len += rnr_elems->elem[i].len;
			for (i = elems->cnt; i < rnr_elems->cnt; i++)
				len += rnr_elems->elem[i].len;
		}
		return len;
	}
	for (i = 0; i < elems->cnt; i++)
		len += elems->elem[i].len;
	if (rnr_elems) {
		for (i = 0; i < rnr_elems->cnt; i++)
			len += rnr_elems->elem[i].len;
	}
	return len;
}
enum {
	IEEE80211_RX_MSG	= 1,
	IEEE80211_TX_STATUS_MSG	= 2,
};
enum queue_stop_reason {
	IEEE80211_QUEUE_STOP_REASON_DRIVER,
	IEEE80211_QUEUE_STOP_REASON_PS,
	IEEE80211_QUEUE_STOP_REASON_CSA,
	IEEE80211_QUEUE_STOP_REASON_AGGREGATION,
	IEEE80211_QUEUE_STOP_REASON_SUSPEND,
	IEEE80211_QUEUE_STOP_REASON_SKB_ADD,
	IEEE80211_QUEUE_STOP_REASON_OFFCHANNEL,
	IEEE80211_QUEUE_STOP_REASON_FLUSH,
	IEEE80211_QUEUE_STOP_REASON_TDLS_TEARDOWN,
	IEEE80211_QUEUE_STOP_REASON_RESERVE_TID,
	IEEE80211_QUEUE_STOP_REASON_IFTYPE_CHANGE,
	IEEE80211_QUEUE_STOP_REASONS,
};
#ifdef CONFIG_MAC80211_LEDS
struct tpt_led_trigger {
	char name[32];
	const struct ieee80211_tpt_blink *blink_table;
	unsigned int blink_table_len;
	struct timer_list timer;
	struct ieee80211_local *local;
	unsigned long prev_traffic;
	unsigned long tx_bytes, rx_bytes;
	unsigned int active, want;
	bool running;
};
#endif
enum {
	SCAN_SW_SCANNING,
	SCAN_HW_SCANNING,
	SCAN_ONCHANNEL_SCANNING,
	SCAN_COMPLETED,
	SCAN_ABORTED,
	SCAN_HW_CANCELLED,
	SCAN_BEACON_WAIT,
	SCAN_BEACON_DONE,
};
enum mac80211_scan_state {
	SCAN_DECISION,
	SCAN_SET_CHANNEL,
	SCAN_SEND_PROBE,
	SCAN_SUSPEND,
	SCAN_RESUME,
	SCAN_ABORT,
};
DECLARE_STATIC_KEY_FALSE(aql_disable);
struct ieee80211_local {
	struct ieee80211_hw hw;
	struct fq fq;
	struct codel_vars *cvars;
	struct codel_params cparams;
	spinlock_t active_txq_lock[IEEE80211_NUM_ACS];
	struct list_head active_txqs[IEEE80211_NUM_ACS];
	u16 schedule_round[IEEE80211_NUM_ACS];
	spinlock_t handle_wake_tx_queue_lock;
	u16 airtime_flags;
	u32 aql_txq_limit_low[IEEE80211_NUM_ACS];
	u32 aql_txq_limit_high[IEEE80211_NUM_ACS];
	u32 aql_threshold;
	atomic_t aql_total_pending_airtime;
	atomic_t aql_ac_pending_airtime[IEEE80211_NUM_ACS];
	const struct ieee80211_ops *ops;
	struct workqueue_struct *workqueue;
	unsigned long queue_stop_reasons[IEEE80211_MAX_QUEUES];
	int q_stop_reasons[IEEE80211_MAX_QUEUES][IEEE80211_QUEUE_STOP_REASONS];
	spinlock_t queue_stop_reason_lock;
	int open_count;
	int monitors, cooked_mntrs;
	int fif_fcsfail, fif_plcpfail, fif_control, fif_other_bss, fif_pspoll,
	    fif_probe_req;
	bool probe_req_reg;
	bool rx_mcast_action_reg;
	unsigned int filter_flags;  
	bool wiphy_ciphers_allocated;
	bool use_chanctx;
	spinlock_t filter_lock;
	struct work_struct reconfig_filter;
	struct netdev_hw_addr_list mc_list;
	bool tim_in_locked_section;  
	bool suspended;
	bool suspending;
	bool resuming;
	bool quiescing;
	bool started;
	bool in_reconfig;
	bool reconfig_failure;
	bool wowlan;
	struct wiphy_work radar_detected_work;
	u8 rx_chains;
	u8 sband_allocated;
	int tx_headroom;  
#define IEEE80211_IRQSAFE_QUEUE_LIMIT 128
	struct tasklet_struct tasklet;
	struct sk_buff_head skb_queue;
	struct sk_buff_head skb_queue_unreliable;
	spinlock_t rx_path_lock;
	struct mutex sta_mtx;
	spinlock_t tim_lock;
	unsigned long num_sta;
	struct list_head sta_list;
	struct rhltable sta_hash;
	struct rhltable link_sta_hash;
	struct timer_list sta_cleanup;
	int sta_generation;
	struct sk_buff_head pending[IEEE80211_MAX_QUEUES];
	struct tasklet_struct tx_pending_tasklet;
	struct tasklet_struct wake_txqs_tasklet;
	atomic_t agg_queue_stop[IEEE80211_MAX_QUEUES];
	atomic_t iff_allmultis;
	struct rate_control_ref *rate_ctrl;
	struct arc4_ctx wep_tx_ctx;
	struct arc4_ctx wep_rx_ctx;
	u32 wep_iv;
	struct list_head interfaces;
	struct list_head mon_list;  
	struct mutex iflist_mtx;
	struct mutex key_mtx;
	struct mutex mtx;
	unsigned long scanning;
	struct cfg80211_ssid scan_ssid;
	struct cfg80211_scan_request *int_scan_req;
	struct cfg80211_scan_request __rcu *scan_req;
	struct ieee80211_scan_request *hw_scan_req;
	struct cfg80211_chan_def scan_chandef;
	enum nl80211_band hw_scan_band;
	int scan_channel_idx;
	int scan_ies_len;
	int hw_scan_ies_bufsize;
	struct cfg80211_scan_info scan_info;
	struct wiphy_work sched_scan_stopped_work;
	struct ieee80211_sub_if_data __rcu *sched_scan_sdata;
	struct cfg80211_sched_scan_request __rcu *sched_scan_req;
	u8 scan_addr[ETH_ALEN];
	unsigned long leave_oper_channel_time;
	enum mac80211_scan_state next_scan_state;
	struct wiphy_delayed_work scan_work;
	struct ieee80211_sub_if_data __rcu *scan_sdata;
	struct cfg80211_chan_def _oper_chandef;
	struct ieee80211_channel *tmp_channel;
	struct list_head chanctx_list;
	struct mutex chanctx_mtx;
#ifdef CONFIG_MAC80211_LEDS
	struct led_trigger tx_led, rx_led, assoc_led, radio_led;
	struct led_trigger tpt_led;
	atomic_t tx_led_active, rx_led_active, assoc_led_active;
	atomic_t radio_led_active, tpt_led_active;
	struct tpt_led_trigger *tpt_led_trigger;
#endif
#ifdef CONFIG_MAC80211_DEBUG_COUNTERS
	u32 dot11TransmittedFragmentCount;
	u32 dot11MulticastTransmittedFrameCount;
	u32 dot11FailedCount;
	u32 dot11RetryCount;
	u32 dot11MultipleRetryCount;
	u32 dot11FrameDuplicateCount;
	u32 dot11ReceivedFragmentCount;
	u32 dot11MulticastReceivedFrameCount;
	u32 dot11TransmittedFrameCount;
	unsigned int tx_handlers_drop;
	unsigned int tx_handlers_queued;
	unsigned int tx_handlers_drop_wep;
	unsigned int tx_handlers_drop_not_assoc;
	unsigned int tx_handlers_drop_unauth_port;
	unsigned int rx_handlers_drop;
	unsigned int rx_handlers_queued;
	unsigned int rx_handlers_drop_nullfunc;
	unsigned int rx_handlers_drop_defrag;
	unsigned int tx_expand_skb_head;
	unsigned int tx_expand_skb_head_cloned;
	unsigned int rx_expand_skb_head_defrag;
	unsigned int rx_handlers_fragments;
	unsigned int tx_status_drop;
#define I802_DEBUG_INC(c) (c)++
#else  
#define I802_DEBUG_INC(c) do { } while (0)
#endif  
	int total_ps_buffered;  
	bool pspolling;
	struct ieee80211_sub_if_data *ps_sdata;
	struct work_struct dynamic_ps_enable_work;
	struct work_struct dynamic_ps_disable_work;
	struct timer_list dynamic_ps_timer;
	struct notifier_block ifa_notifier;
	struct notifier_block ifa6_notifier;
	int dynamic_ps_forced_timeout;
	int user_power_level;  
	enum ieee80211_smps_mode smps_mode;
	struct work_struct restart_work;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct local_debugfsdentries {
		struct dentry *rcdir;
		struct dentry *keys;
	} debugfs;
	bool force_tx_status;
#endif
	struct wiphy_delayed_work roc_work;
	struct list_head roc_list;
	struct wiphy_work hw_roc_start, hw_roc_done;
	unsigned long hw_roc_start_time;
	u64 roc_cookie_counter;
	struct idr ack_status_frames;
	spinlock_t ack_status_lock;
	struct ieee80211_sub_if_data __rcu *p2p_sdata;
	struct ieee80211_sub_if_data __rcu *monitor_sdata;
	struct cfg80211_chan_def monitor_chandef;
	u8 ext_capa[8];
};
static inline struct ieee80211_sub_if_data *
IEEE80211_DEV_TO_SUB_IF(const struct net_device *dev)
{
	return netdev_priv(dev);
}
static inline struct ieee80211_sub_if_data *
IEEE80211_WDEV_TO_SUB_IF(struct wireless_dev *wdev)
{
	return container_of(wdev, struct ieee80211_sub_if_data, wdev);
}
static inline struct ieee80211_supported_band *
ieee80211_get_sband(struct ieee80211_sub_if_data *sdata)
{
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_chanctx_conf *chanctx_conf;
	enum nl80211_band band;
	WARN_ON(ieee80211_vif_is_mld(&sdata->vif));
	rcu_read_lock();
	chanctx_conf = rcu_dereference(sdata->vif.bss_conf.chanctx_conf);
	if (!chanctx_conf) {
		rcu_read_unlock();
		return NULL;
	}
	band = chanctx_conf->def.chan->band;
	rcu_read_unlock();
	return local->hw.wiphy->bands[band];
}
static inline struct ieee80211_supported_band *
ieee80211_get_link_sband(struct ieee80211_link_data *link)
{
	struct ieee80211_local *local = link->sdata->local;
	struct ieee80211_chanctx_conf *chanctx_conf;
	enum nl80211_band band;
	rcu_read_lock();
	chanctx_conf = rcu_dereference(link->conf->chanctx_conf);
	if (!chanctx_conf) {
		rcu_read_unlock();
		return NULL;
	}
	band = chanctx_conf->def.chan->band;
	rcu_read_unlock();
	return local->hw.wiphy->bands[band];
}
struct ieee80211_csa_ie {
	struct cfg80211_chan_def chandef;
	u8 mode;
	u8 count;
	u8 ttl;
	u16 pre_value;
	u16 reason_code;
	u32 max_switch_time;
};
struct ieee802_11_elems {
	const u8 *ie_start;
	size_t total_len;
	u32 crc;
	const struct ieee80211_tdls_lnkie *lnk_id;
	const struct ieee80211_ch_switch_timing *ch_sw_timing;
	const u8 *ext_capab;
	const u8 *ssid;
	const u8 *supp_rates;
	const u8 *ds_params;
	const struct ieee80211_tim_ie *tim;
	const u8 *rsn;
	const u8 *rsnx;
	const u8 *erp_info;
	const u8 *ext_supp_rates;
	const u8 *wmm_info;
	const u8 *wmm_param;
	const struct ieee80211_ht_cap *ht_cap_elem;
	const struct ieee80211_ht_operation *ht_operation;
	const struct ieee80211_vht_cap *vht_cap_elem;
	const struct ieee80211_vht_operation *vht_operation;
	const struct ieee80211_meshconf_ie *mesh_config;
	const u8 *he_cap;
	const struct ieee80211_he_operation *he_operation;
	const struct ieee80211_he_spr *he_spr;
	const struct ieee80211_mu_edca_param_set *mu_edca_param_set;
	const struct ieee80211_he_6ghz_capa *he_6ghz_capa;
	const struct ieee80211_tx_pwr_env *tx_pwr_env[IEEE80211_TPE_MAX_IE_COUNT];
	const u8 *uora_element;
	const u8 *mesh_id;
	const u8 *peering;
	const __le16 *awake_window;
	const u8 *preq;
	const u8 *prep;
	const u8 *perr;
	const struct ieee80211_rann_ie *rann;
	const struct ieee80211_channel_sw_ie *ch_switch_ie;
	const struct ieee80211_ext_chansw_ie *ext_chansw_ie;
	const struct ieee80211_wide_bw_chansw_ie *wide_bw_chansw_ie;
	const u8 *max_channel_switch_time;
	const u8 *country_elem;
	const u8 *pwr_constr_elem;
	const u8 *cisco_dtpc_elem;
	const struct ieee80211_timeout_interval_ie *timeout_int;
	const u8 *opmode_notif;
	const struct ieee80211_sec_chan_offs_ie *sec_chan_offs;
	struct ieee80211_mesh_chansw_params_ie *mesh_chansw_params_ie;
	const struct ieee80211_bss_max_idle_period_ie *max_idle_period_ie;
	const struct ieee80211_multiple_bssid_configuration *mbssid_config_ie;
	const struct ieee80211_bssid_index *bssid_index;
	u8 max_bssid_indicator;
	u8 dtim_count;
	u8 dtim_period;
	const struct ieee80211_addba_ext_ie *addba_ext_ie;
	const struct ieee80211_s1g_cap *s1g_capab;
	const struct ieee80211_s1g_oper_ie *s1g_oper;
	const struct ieee80211_s1g_bcn_compat_ie *s1g_bcn_compat;
	const struct ieee80211_aid_response_ie *aid_resp;
	const struct ieee80211_eht_cap_elem *eht_cap;
	const struct ieee80211_eht_operation *eht_operation;
	const struct ieee80211_multi_link_elem *ml_basic;
	const struct ieee80211_multi_link_elem *ml_reconf;
	u8 ext_capab_len;
	u8 ssid_len;
	u8 supp_rates_len;
	u8 tim_len;
	u8 rsn_len;
	u8 rsnx_len;
	u8 ext_supp_rates_len;
	u8 wmm_info_len;
	u8 wmm_param_len;
	u8 he_cap_len;
	u8 mesh_id_len;
	u8 peering_len;
	u8 preq_len;
	u8 prep_len;
	u8 perr_len;
	u8 country_elem_len;
	u8 bssid_index_len;
	u8 tx_pwr_env_len[IEEE80211_TPE_MAX_IE_COUNT];
	u8 tx_pwr_env_num;
	u8 eht_cap_len;
	size_t ml_basic_len;
	size_t ml_reconf_len;
	const struct element *ml_basic_elem;
	const struct element *ml_reconf_elem;
	struct ieee80211_mle_per_sta_profile *prof;
	size_t sta_prof_len;
	bool parse_error;
	size_t scratch_len;
	u8 *scratch_pos;
	u8 scratch[];
};
static inline struct ieee80211_local *hw_to_local(
	struct ieee80211_hw *hw)
{
	return container_of(hw, struct ieee80211_local, hw);
}
static inline struct txq_info *to_txq_info(struct ieee80211_txq *txq)
{
	return container_of(txq, struct txq_info, txq);
}
static inline bool txq_has_queue(struct ieee80211_txq *txq)
{
	struct txq_info *txqi = to_txq_info(txq);
	return !(skb_queue_empty(&txqi->frags) && !txqi->tin.backlog_packets);
}
static inline bool
ieee80211_have_rx_timestamp(struct ieee80211_rx_status *status)
{
	WARN_ON_ONCE(status->flag & RX_FLAG_MACTIME_START &&
		     status->flag & RX_FLAG_MACTIME_END);
	return !!(status->flag & (RX_FLAG_MACTIME_START | RX_FLAG_MACTIME_END |
				  RX_FLAG_MACTIME_PLCP_START));
}
void ieee80211_vif_inc_num_mcast(struct ieee80211_sub_if_data *sdata);
void ieee80211_vif_dec_num_mcast(struct ieee80211_sub_if_data *sdata);
static inline int
ieee80211_vif_get_num_mcast_if(struct ieee80211_sub_if_data *sdata)
{
	if (sdata->vif.type == NL80211_IFTYPE_AP)
		return atomic_read(&sdata->u.ap.num_mcast_sta);
	if (sdata->vif.type == NL80211_IFTYPE_AP_VLAN && !sdata->u.vlan.sta)
		return atomic_read(&sdata->u.vlan.num_mcast_sta);
	return -1;
}
u64 ieee80211_calculate_rx_timestamp(struct ieee80211_local *local,
				     struct ieee80211_rx_status *status,
				     unsigned int mpdu_len,
				     unsigned int mpdu_offset);
int ieee80211_hw_config(struct ieee80211_local *local, u32 changed);
void ieee80211_tx_set_protected(struct ieee80211_tx_data *tx);
void ieee80211_bss_info_change_notify(struct ieee80211_sub_if_data *sdata,
				      u64 changed);
void ieee80211_vif_cfg_change_notify(struct ieee80211_sub_if_data *sdata,
				     u64 changed);
void ieee80211_link_info_change_notify(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_link_data *link,
				       u64 changed);
void ieee80211_configure_filter(struct ieee80211_local *local);
u64 ieee80211_reset_erp_info(struct ieee80211_sub_if_data *sdata);
u64 ieee80211_mgmt_tx_cookie(struct ieee80211_local *local);
int ieee80211_attach_ack_skb(struct ieee80211_local *local, struct sk_buff *skb,
			     u64 *cookie, gfp_t gfp);
void ieee80211_check_fast_rx(struct sta_info *sta);
void __ieee80211_check_fast_rx_iface(struct ieee80211_sub_if_data *sdata);
void ieee80211_check_fast_rx_iface(struct ieee80211_sub_if_data *sdata);
void ieee80211_clear_fast_rx(struct sta_info *sta);
bool ieee80211_is_our_addr(struct ieee80211_sub_if_data *sdata,
			   const u8 *addr, int *out_link_id);
void ieee80211_sta_setup_sdata(struct ieee80211_sub_if_data *sdata);
int ieee80211_mgd_auth(struct ieee80211_sub_if_data *sdata,
		       struct cfg80211_auth_request *req);
int ieee80211_mgd_assoc(struct ieee80211_sub_if_data *sdata,
			struct cfg80211_assoc_request *req);
int ieee80211_mgd_deauth(struct ieee80211_sub_if_data *sdata,
			 struct cfg80211_deauth_request *req);
int ieee80211_mgd_disassoc(struct ieee80211_sub_if_data *sdata,
			   struct cfg80211_disassoc_request *req);
void ieee80211_send_pspoll(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_ps(struct ieee80211_local *local);
void ieee80211_recalc_ps_vif(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				  struct sk_buff *skb);
void ieee80211_sta_rx_queued_ext(struct ieee80211_sub_if_data *sdata,
				 struct sk_buff *skb);
void ieee80211_sta_reset_beacon_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_reset_conn_monitor(struct ieee80211_sub_if_data *sdata);
void ieee80211_mgd_stop(struct ieee80211_sub_if_data *sdata);
void ieee80211_mgd_conn_tx_status(struct ieee80211_sub_if_data *sdata,
				  __le16 fc, bool acked);
void ieee80211_mgd_quiesce(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_restart(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_handle_tspec_ac_params(struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_connection_lost(struct ieee80211_sub_if_data *sdata,
				   u8 reason, bool tx);
void ieee80211_mgd_setup_link(struct ieee80211_link_data *link);
void ieee80211_mgd_stop_link(struct ieee80211_link_data *link);
void ieee80211_mgd_set_link_qos_params(struct ieee80211_link_data *link);
void ieee80211_ibss_notify_scan_completed(struct ieee80211_local *local);
void ieee80211_ibss_setup_sdata(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_no_sta(struct ieee80211_sub_if_data *sdata,
			      const u8 *bssid, const u8 *addr, u32 supp_rates);
int ieee80211_ibss_join(struct ieee80211_sub_if_data *sdata,
			struct cfg80211_ibss_params *params);
int ieee80211_ibss_leave(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_ibss_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				   struct sk_buff *skb);
int ieee80211_ibss_csa_beacon(struct ieee80211_sub_if_data *sdata,
			      struct cfg80211_csa_settings *csa_settings,
			      u64 *changed);
int ieee80211_ibss_finish_csa(struct ieee80211_sub_if_data *sdata,
			      u64 *changed);
void ieee80211_ibss_stop(struct ieee80211_sub_if_data *sdata);
void ieee80211_ocb_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_ocb_rx_no_sta(struct ieee80211_sub_if_data *sdata,
			     const u8 *bssid, const u8 *addr, u32 supp_rates);
void ieee80211_ocb_setup_sdata(struct ieee80211_sub_if_data *sdata);
int ieee80211_ocb_join(struct ieee80211_sub_if_data *sdata,
		       struct ocb_setup *setup);
int ieee80211_ocb_leave(struct ieee80211_sub_if_data *sdata);
void ieee80211_mesh_work(struct ieee80211_sub_if_data *sdata);
void ieee80211_mesh_rx_queued_mgmt(struct ieee80211_sub_if_data *sdata,
				   struct sk_buff *skb);
int ieee80211_mesh_csa_beacon(struct ieee80211_sub_if_data *sdata,
			      struct cfg80211_csa_settings *csa_settings,
			      u64 *changed);
int ieee80211_mesh_finish_csa(struct ieee80211_sub_if_data *sdata,
			      u64 *changed);
void ieee80211_scan_work(struct wiphy *wiphy, struct wiphy_work *work);
int ieee80211_request_ibss_scan(struct ieee80211_sub_if_data *sdata,
				const u8 *ssid, u8 ssid_len,
				struct ieee80211_channel **channels,
				unsigned int n_channels,
				enum nl80211_bss_scan_width scan_width);
int ieee80211_request_scan(struct ieee80211_sub_if_data *sdata,
			   struct cfg80211_scan_request *req);
void ieee80211_scan_cancel(struct ieee80211_local *local);
void ieee80211_run_deferred_scan(struct ieee80211_local *local);
void ieee80211_scan_rx(struct ieee80211_local *local, struct sk_buff *skb);
void ieee80211_inform_bss(struct wiphy *wiphy, struct cfg80211_bss *bss,
			  const struct cfg80211_bss_ies *ies, void *data);
void ieee80211_mlme_notify_scan_completed(struct ieee80211_local *local);
struct ieee80211_bss *
ieee80211_bss_info_update(struct ieee80211_local *local,
			  struct ieee80211_rx_status *rx_status,
			  struct ieee80211_mgmt *mgmt,
			  size_t len,
			  struct ieee80211_channel *channel);
void ieee80211_rx_bss_put(struct ieee80211_local *local,
			  struct ieee80211_bss *bss);
int
__ieee80211_request_sched_scan_start(struct ieee80211_sub_if_data *sdata,
				     struct cfg80211_sched_scan_request *req);
int ieee80211_request_sched_scan_start(struct ieee80211_sub_if_data *sdata,
				       struct cfg80211_sched_scan_request *req);
int ieee80211_request_sched_scan_stop(struct ieee80211_local *local);
void ieee80211_sched_scan_end(struct ieee80211_local *local);
void ieee80211_sched_scan_stopped_work(struct wiphy *wiphy,
				       struct wiphy_work *work);
void ieee80211_offchannel_stop_vifs(struct ieee80211_local *local);
void ieee80211_offchannel_return(struct ieee80211_local *local);
void ieee80211_roc_setup(struct ieee80211_local *local);
void ieee80211_start_next_roc(struct ieee80211_local *local);
void ieee80211_roc_purge(struct ieee80211_local *local,
			 struct ieee80211_sub_if_data *sdata);
int ieee80211_remain_on_channel(struct wiphy *wiphy, struct wireless_dev *wdev,
				struct ieee80211_channel *chan,
				unsigned int duration, u64 *cookie);
int ieee80211_cancel_remain_on_channel(struct wiphy *wiphy,
				       struct wireless_dev *wdev, u64 cookie);
int ieee80211_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		      struct cfg80211_mgmt_tx_params *params, u64 *cookie);
int ieee80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
				  struct wireless_dev *wdev, u64 cookie);
void ieee80211_csa_finalize_work(struct work_struct *work);
int ieee80211_channel_switch(struct wiphy *wiphy, struct net_device *dev,
			     struct cfg80211_csa_settings *params);
void ieee80211_color_change_finalize_work(struct work_struct *work);
void ieee80211_color_collision_detection_work(struct work_struct *work);
#define MAC80211_SUPPORTED_FEATURES_TX	(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM | \
					 NETIF_F_HW_CSUM | NETIF_F_SG | \
					 NETIF_F_HIGHDMA | NETIF_F_GSO_SOFTWARE | \
					 NETIF_F_HW_TC)
#define MAC80211_SUPPORTED_FEATURES_RX	(NETIF_F_RXCSUM)
#define MAC80211_SUPPORTED_FEATURES	(MAC80211_SUPPORTED_FEATURES_TX | \
					 MAC80211_SUPPORTED_FEATURES_RX)
int ieee80211_iface_init(void);
void ieee80211_iface_exit(void);
int ieee80211_if_add(struct ieee80211_local *local, const char *name,
		     unsigned char name_assign_type,
		     struct wireless_dev **new_wdev, enum nl80211_iftype type,
		     struct vif_params *params);
int ieee80211_if_change_type(struct ieee80211_sub_if_data *sdata,
			     enum nl80211_iftype type);
void ieee80211_if_remove(struct ieee80211_sub_if_data *sdata);
void ieee80211_remove_interfaces(struct ieee80211_local *local);
u32 ieee80211_idle_off(struct ieee80211_local *local);
void ieee80211_recalc_idle(struct ieee80211_local *local);
void ieee80211_adjust_monitor_flags(struct ieee80211_sub_if_data *sdata,
				    const int offset);
int ieee80211_do_open(struct wireless_dev *wdev, bool coming_up);
void ieee80211_sdata_stop(struct ieee80211_sub_if_data *sdata);
int ieee80211_add_virtual_monitor(struct ieee80211_local *local);
void ieee80211_del_virtual_monitor(struct ieee80211_local *local);
bool __ieee80211_recalc_txpower(struct ieee80211_sub_if_data *sdata);
void ieee80211_recalc_txpower(struct ieee80211_sub_if_data *sdata,
			      bool update_bss);
void ieee80211_recalc_offload(struct ieee80211_local *local);
static inline bool ieee80211_sdata_running(struct ieee80211_sub_if_data *sdata)
{
	return test_bit(SDATA_STATE_RUNNING, &sdata->state);
}
void ieee80211_link_setup(struct ieee80211_link_data *link);
void ieee80211_link_init(struct ieee80211_sub_if_data *sdata,
			 int link_id,
			 struct ieee80211_link_data *link,
			 struct ieee80211_bss_conf *link_conf);
void ieee80211_link_stop(struct ieee80211_link_data *link);
int ieee80211_vif_set_links(struct ieee80211_sub_if_data *sdata,
			    u16 new_links, u16 dormant_links);
void ieee80211_vif_clear_links(struct ieee80211_sub_if_data *sdata);
int __ieee80211_set_active_links(struct ieee80211_vif *vif, u16 active_links);
void ieee80211_clear_tx_pending(struct ieee80211_local *local);
void ieee80211_tx_pending(struct tasklet_struct *t);
netdev_tx_t ieee80211_monitor_start_xmit(struct sk_buff *skb,
					 struct net_device *dev);
netdev_tx_t ieee80211_subif_start_xmit(struct sk_buff *skb,
				       struct net_device *dev);
netdev_tx_t ieee80211_subif_start_xmit_8023(struct sk_buff *skb,
					    struct net_device *dev);
void __ieee80211_subif_start_xmit(struct sk_buff *skb,
				  struct net_device *dev,
				  u32 info_flags,
				  u32 ctrl_flags,
				  u64 *cookie);
void ieee80211_purge_tx_queue(struct ieee80211_hw *hw,
			      struct sk_buff_head *skbs);
struct sk_buff *
ieee80211_build_data_template(struct ieee80211_sub_if_data *sdata,
			      struct sk_buff *skb, u32 info_flags);
void ieee80211_tx_monitor(struct ieee80211_local *local, struct sk_buff *skb,
			  int retry_count, int shift, bool send_to_cooked,
			  struct ieee80211_tx_status *status);
void ieee80211_check_fast_xmit(struct sta_info *sta);
void ieee80211_check_fast_xmit_all(struct ieee80211_local *local);
void ieee80211_check_fast_xmit_iface(struct ieee80211_sub_if_data *sdata);
void ieee80211_clear_fast_xmit(struct sta_info *sta);
int ieee80211_tx_control_port(struct wiphy *wiphy, struct net_device *dev,
			      const u8 *buf, size_t len,
			      const u8 *dest, __be16 proto, bool unencrypted,
			      int link_id, u64 *cookie);
int ieee80211_probe_mesh_link(struct wiphy *wiphy, struct net_device *dev,
			      const u8 *buf, size_t len);
void __ieee80211_xmit_fast(struct ieee80211_sub_if_data *sdata,
			   struct sta_info *sta,
			   struct ieee80211_fast_tx *fast_tx,
			   struct sk_buff *skb, bool ampdu,
			   const u8 *da, const u8 *sa);
void ieee80211_aggr_check(struct ieee80211_sub_if_data *sdata,
			  struct sta_info *sta, struct sk_buff *skb);
void ieee80211_apply_htcap_overrides(struct ieee80211_sub_if_data *sdata,
				     struct ieee80211_sta_ht_cap *ht_cap);
bool ieee80211_ht_cap_ie_to_sta_ht_cap(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_supported_band *sband,
				       const struct ieee80211_ht_cap *ht_cap_ie,
				       struct link_sta_info *link_sta);
void ieee80211_send_delba(struct ieee80211_sub_if_data *sdata,
			  const u8 *da, u16 tid,
			  u16 initiator, u16 reason_code);
int ieee80211_send_smps_action(struct ieee80211_sub_if_data *sdata,
			       enum ieee80211_smps_mode smps, const u8 *da,
			       const u8 *bssid);
bool ieee80211_smps_is_restrictive(enum ieee80211_smps_mode smps_mode_old,
				   enum ieee80211_smps_mode smps_mode_new);
void ___ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
				     u16 initiator, u16 reason, bool stop);
void __ieee80211_stop_rx_ba_session(struct sta_info *sta, u16 tid,
				    u16 initiator, u16 reason, bool stop);
void ___ieee80211_start_rx_ba_session(struct sta_info *sta,
				      u8 dialog_token, u16 timeout,
				      u16 start_seq_num, u16 ba_policy, u16 tid,
				      u16 buf_size, bool tx, bool auto_seq,
				      const struct ieee80211_addba_ext_ie *addbaext);
void ieee80211_sta_tear_down_BA_sessions(struct sta_info *sta,
					 enum ieee80211_agg_stop_reason reason);
void ieee80211_process_delba(struct ieee80211_sub_if_data *sdata,
			     struct sta_info *sta,
			     struct ieee80211_mgmt *mgmt, size_t len);
void ieee80211_process_addba_resp(struct ieee80211_local *local,
				  struct sta_info *sta,
				  struct ieee80211_mgmt *mgmt,
				  size_t len);
void ieee80211_process_addba_request(struct ieee80211_local *local,
				     struct sta_info *sta,
				     struct ieee80211_mgmt *mgmt,
				     size_t len);
int __ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
				   enum ieee80211_agg_stop_reason reason);
int ___ieee80211_stop_tx_ba_session(struct sta_info *sta, u16 tid,
				    enum ieee80211_agg_stop_reason reason);
void ieee80211_start_tx_ba_cb(struct sta_info *sta, int tid,
			      struct tid_ampdu_tx *tid_tx);
void ieee80211_stop_tx_ba_cb(struct sta_info *sta, int tid,
			     struct tid_ampdu_tx *tid_tx);
void ieee80211_ba_session_work(struct work_struct *work);
void ieee80211_tx_ba_session_handle_start(struct sta_info *sta, int tid);
void ieee80211_release_reorder_timeout(struct sta_info *sta, int tid);
u8 ieee80211_mcs_to_chains(const struct ieee80211_mcs_info *mcs);
enum nl80211_smps_mode
ieee80211_smps_mode_to_smps_mode(enum ieee80211_smps_mode smps);
void
ieee80211_vht_cap_ie_to_sta_vht_cap(struct ieee80211_sub_if_data *sdata,
				    struct ieee80211_supported_band *sband,
				    const struct ieee80211_vht_cap *vht_cap_ie,
				    const struct ieee80211_vht_cap *vht_cap_ie2,
				    struct link_sta_info *link_sta);
enum ieee80211_sta_rx_bandwidth
ieee80211_sta_cap_rx_bw(struct link_sta_info *link_sta);
enum ieee80211_sta_rx_bandwidth
ieee80211_sta_cur_vht_bw(struct link_sta_info *link_sta);
void ieee80211_sta_set_rx_nss(struct link_sta_info *link_sta);
enum ieee80211_sta_rx_bandwidth
ieee80211_chan_width_to_rx_bw(enum nl80211_chan_width width);
enum nl80211_chan_width
ieee80211_sta_cap_chan_bw(struct link_sta_info *link_sta);
void ieee80211_process_mu_groups(struct ieee80211_sub_if_data *sdata,
				 struct ieee80211_link_data *link,
				 struct ieee80211_mgmt *mgmt);
u32 __ieee80211_vht_handle_opmode(struct ieee80211_sub_if_data *sdata,
				  struct link_sta_info *sta,
				  u8 opmode, enum nl80211_band band);
void ieee80211_vht_handle_opmode(struct ieee80211_sub_if_data *sdata,
				 struct link_sta_info *sta,
				 u8 opmode, enum nl80211_band band);
void ieee80211_apply_vhtcap_overrides(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_sta_vht_cap *vht_cap);
void ieee80211_get_vht_mask_from_cap(__le16 vht_cap,
				     u16 vht_mask[NL80211_VHT_NSS_MAX]);
enum nl80211_chan_width
ieee80211_sta_rx_bw_to_chan_width(struct link_sta_info *sta);
void
ieee80211_he_cap_ie_to_sta_he_cap(struct ieee80211_sub_if_data *sdata,
				  struct ieee80211_supported_band *sband,
				  const u8 *he_cap_ie, u8 he_cap_len,
				  const struct ieee80211_he_6ghz_capa *he_6ghz_capa,
				  struct link_sta_info *link_sta);
void
ieee80211_he_spr_ie_to_bss_conf(struct ieee80211_vif *vif,
				const struct ieee80211_he_spr *he_spr_ie_elem);
void
ieee80211_he_op_ie_to_bss_conf(struct ieee80211_vif *vif,
			const struct ieee80211_he_operation *he_op_ie_elem);
void ieee80211_s1g_sta_rate_init(struct sta_info *sta);
bool ieee80211_s1g_is_twt_setup(struct sk_buff *skb);
void ieee80211_s1g_rx_twt_action(struct ieee80211_sub_if_data *sdata,
				 struct sk_buff *skb);
void ieee80211_s1g_status_twt_action(struct ieee80211_sub_if_data *sdata,
				     struct sk_buff *skb);
void ieee80211_process_measurement_req(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_mgmt *mgmt,
				       size_t len);
int ieee80211_parse_ch_switch_ie(struct ieee80211_sub_if_data *sdata,
				 struct ieee802_11_elems *elems,
				 enum nl80211_band current_band,
				 u32 vht_cap_info,
				 ieee80211_conn_flags_t conn_flags, u8 *bssid,
				 struct ieee80211_csa_ie *csa_ie);
int ieee80211_reconfig(struct ieee80211_local *local);
void ieee80211_stop_device(struct ieee80211_local *local);
int __ieee80211_suspend(struct ieee80211_hw *hw,
			struct cfg80211_wowlan *wowlan);
static inline int __ieee80211_resume(struct ieee80211_hw *hw)
{
	struct ieee80211_local *local = hw_to_local(hw);
	WARN(test_bit(SCAN_HW_SCANNING, &local->scanning) &&
	     !test_bit(SCAN_COMPLETED, &local->scanning),
		"%s: resume with hardware scan still in progress\n",
		wiphy_name(hw->wiphy));
	return ieee80211_reconfig(hw_to_local(hw));
}
extern const void *const mac80211_wiphy_privid;  
int ieee80211_frame_duration(enum nl80211_band band, size_t len,
			     int rate, int erp, int short_preamble,
			     int shift);
void ieee80211_regulatory_limit_wmm_params(struct ieee80211_sub_if_data *sdata,
					   struct ieee80211_tx_queue_params *qparam,
					   int ac);
void ieee80211_set_wmm_default(struct ieee80211_link_data *link,
			       bool bss_notify, bool enable_qos);
void ieee80211_xmit(struct ieee80211_sub_if_data *sdata,
		    struct sta_info *sta, struct sk_buff *skb);
void __ieee80211_tx_skb_tid_band(struct ieee80211_sub_if_data *sdata,
				 struct sk_buff *skb, int tid, int link_id,
				 enum nl80211_band band);
int ieee80211_lookup_ra_sta(struct ieee80211_sub_if_data *sdata,
			    struct sk_buff *skb,
			    struct sta_info **sta_out);
static inline void
ieee80211_tx_skb_tid_band(struct ieee80211_sub_if_data *sdata,
			  struct sk_buff *skb, int tid,
			  enum nl80211_band band)
{
	rcu_read_lock();
	__ieee80211_tx_skb_tid_band(sdata, skb, tid, -1, band);
	rcu_read_unlock();
}
void ieee80211_tx_skb_tid(struct ieee80211_sub_if_data *sdata,
			  struct sk_buff *skb, int tid, int link_id);
static inline void ieee80211_tx_skb(struct ieee80211_sub_if_data *sdata,
				    struct sk_buff *skb)
{
	ieee80211_tx_skb_tid(sdata, skb, 7, -1);
}
struct ieee80211_elems_parse_params {
	const u8 *start;
	size_t len;
	bool action;
	u64 filter;
	u32 crc;
	struct cfg80211_bss *bss;
	int link_id;
	bool from_ap;
};
struct ieee802_11_elems *
ieee802_11_parse_elems_full(struct ieee80211_elems_parse_params *params);
static inline struct ieee802_11_elems *
ieee802_11_parse_elems_crc(const u8 *start, size_t len, bool action,
			   u64 filter, u32 crc,
			   struct cfg80211_bss *bss)
{
	struct ieee80211_elems_parse_params params = {
		.start = start,
		.len = len,
		.action = action,
		.filter = filter,
		.crc = crc,
		.bss = bss,
		.link_id = -1,
	};
	return ieee802_11_parse_elems_full(&params);
}
static inline struct ieee802_11_elems *
ieee802_11_parse_elems(const u8 *start, size_t len, bool action,
		       struct cfg80211_bss *bss)
{
	return ieee802_11_parse_elems_crc(start, len, action, 0, 0, bss);
}
void ieee80211_fragment_element(struct sk_buff *skb, u8 *len_pos, u8 frag_id);
extern const int ieee802_1d_to_ac[8];
static inline int ieee80211_ac_from_tid(int tid)
{
	return ieee802_1d_to_ac[tid & 7];
}
void ieee80211_dynamic_ps_enable_work(struct work_struct *work);
void ieee80211_dynamic_ps_disable_work(struct work_struct *work);
void ieee80211_dynamic_ps_timer(struct timer_list *t);
void ieee80211_send_nullfunc(struct ieee80211_local *local,
			     struct ieee80211_sub_if_data *sdata,
			     bool powersave);
void ieee80211_send_4addr_nullfunc(struct ieee80211_local *local,
				   struct ieee80211_sub_if_data *sdata);
void ieee80211_sta_tx_notify(struct ieee80211_sub_if_data *sdata,
			     struct ieee80211_hdr *hdr, bool ack, u16 tx_time);
void ieee80211_wake_queues_by_reason(struct ieee80211_hw *hw,
				     unsigned long queues,
				     enum queue_stop_reason reason,
				     bool refcounted);
void ieee80211_stop_vif_queues(struct ieee80211_local *local,
			       struct ieee80211_sub_if_data *sdata,
			       enum queue_stop_reason reason);
void ieee80211_wake_vif_queues(struct ieee80211_local *local,
			       struct ieee80211_sub_if_data *sdata,
			       enum queue_stop_reason reason);
void ieee80211_stop_queues_by_reason(struct ieee80211_hw *hw,
				     unsigned long queues,
				     enum queue_stop_reason reason,
				     bool refcounted);
void ieee80211_wake_queue_by_reason(struct ieee80211_hw *hw, int queue,
				    enum queue_stop_reason reason,
				    bool refcounted);
void ieee80211_stop_queue_by_reason(struct ieee80211_hw *hw, int queue,
				    enum queue_stop_reason reason,
				    bool refcounted);
void ieee80211_add_pending_skb(struct ieee80211_local *local,
			       struct sk_buff *skb);
void ieee80211_add_pending_skbs(struct ieee80211_local *local,
				struct sk_buff_head *skbs);
void ieee80211_flush_queues(struct ieee80211_local *local,
			    struct ieee80211_sub_if_data *sdata, bool drop);
void __ieee80211_flush_queues(struct ieee80211_local *local,
			      struct ieee80211_sub_if_data *sdata,
			      unsigned int queues, bool drop);
static inline bool ieee80211_can_run_worker(struct ieee80211_local *local)
{
	if (local->in_reconfig)
		return false;
	if (local->quiescing)
		return false;
	if (local->suspended)
		return false;
	return true;
}
int ieee80211_txq_setup_flows(struct ieee80211_local *local);
void ieee80211_txq_set_params(struct ieee80211_local *local);
void ieee80211_txq_teardown_flows(struct ieee80211_local *local);
void ieee80211_txq_init(struct ieee80211_sub_if_data *sdata,
			struct sta_info *sta,
			struct txq_info *txq, int tid);
void ieee80211_txq_purge(struct ieee80211_local *local,
			 struct txq_info *txqi);
void ieee80211_txq_remove_vlan(struct ieee80211_local *local,
			       struct ieee80211_sub_if_data *sdata);
void ieee80211_fill_txq_stats(struct cfg80211_txq_stats *txqstats,
			      struct txq_info *txqi);
void ieee80211_wake_txqs(struct tasklet_struct *t);
void ieee80211_send_auth(struct ieee80211_sub_if_data *sdata,
			 u16 transaction, u16 auth_alg, u16 status,
			 const u8 *extra, size_t extra_len, const u8 *bssid,
			 const u8 *da, const u8 *key, u8 key_len, u8 key_idx,
			 u32 tx_flags);
void ieee80211_send_deauth_disassoc(struct ieee80211_sub_if_data *sdata,
				    const u8 *da, const u8 *bssid,
				    u16 stype, u16 reason,
				    bool send_frame, u8 *frame_buf);
u8 *ieee80211_write_he_6ghz_cap(u8 *pos, __le16 cap, u8 *end);
enum {
	IEEE80211_PROBE_FLAG_DIRECTED		= BIT(0),
	IEEE80211_PROBE_FLAG_MIN_CONTENT	= BIT(1),
	IEEE80211_PROBE_FLAG_RANDOM_SN		= BIT(2),
};
int ieee80211_build_preq_ies(struct ieee80211_sub_if_data *sdata, u8 *buffer,
			     size_t buffer_len,
			     struct ieee80211_scan_ies *ie_desc,
			     const u8 *ie, size_t ie_len,
			     u8 bands_used, u32 *rate_masks,
			     struct cfg80211_chan_def *chandef,
			     u32 flags);
struct sk_buff *ieee80211_build_probe_req(struct ieee80211_sub_if_data *sdata,
					  const u8 *src, const u8 *dst,
					  u32 ratemask,
					  struct ieee80211_channel *chan,
					  const u8 *ssid, size_t ssid_len,
					  const u8 *ie, size_t ie_len,
					  u32 flags);
u32 ieee80211_sta_get_rates(struct ieee80211_sub_if_data *sdata,
			    struct ieee802_11_elems *elems,
			    enum nl80211_band band, u32 *basic_rates);
int __ieee80211_request_smps_mgd(struct ieee80211_sub_if_data *sdata,
				 struct ieee80211_link_data *link,
				 enum ieee80211_smps_mode smps_mode);
void ieee80211_recalc_smps(struct ieee80211_sub_if_data *sdata,
			   struct ieee80211_link_data *link);
void ieee80211_recalc_min_chandef(struct ieee80211_sub_if_data *sdata,
				  int link_id);
size_t ieee80211_ie_split_vendor(const u8 *ies, size_t ielen, size_t offset);
u8 *ieee80211_ie_build_ht_cap(u8 *pos, struct ieee80211_sta_ht_cap *ht_cap,
			      u16 cap);
u8 *ieee80211_ie_build_ht_oper(u8 *pos, struct ieee80211_sta_ht_cap *ht_cap,
			       const struct cfg80211_chan_def *chandef,
			       u16 prot_mode, bool rifs_mode);
void ieee80211_ie_build_wide_bw_cs(u8 *pos,
				   const struct cfg80211_chan_def *chandef);
u8 *ieee80211_ie_build_vht_cap(u8 *pos, struct ieee80211_sta_vht_cap *vht_cap,
			       u32 cap);
u8 *ieee80211_ie_build_vht_oper(u8 *pos, struct ieee80211_sta_vht_cap *vht_cap,
				const struct cfg80211_chan_def *chandef);
u8 ieee80211_ie_len_he_cap(struct ieee80211_sub_if_data *sdata, u8 iftype);
u8 *ieee80211_ie_build_he_cap(ieee80211_conn_flags_t disable_flags, u8 *pos,
			      const struct ieee80211_sta_he_cap *he_cap,
			      u8 *end);
void ieee80211_ie_build_he_6ghz_cap(struct ieee80211_sub_if_data *sdata,
				    enum ieee80211_smps_mode smps_mode,
				    struct sk_buff *skb);
u8 *ieee80211_ie_build_he_oper(u8 *pos, struct cfg80211_chan_def *chandef);
u8 *ieee80211_ie_build_eht_oper(u8 *pos, struct cfg80211_chan_def *chandef,
				const struct ieee80211_sta_eht_cap *eht_cap);
int ieee80211_parse_bitrates(enum nl80211_chan_width width,
			     const struct ieee80211_supported_band *sband,
			     const u8 *srates, int srates_len, u32 *rates);
int ieee80211_add_srates_ie(struct ieee80211_sub_if_data *sdata,
			    struct sk_buff *skb, bool need_basic,
			    enum nl80211_band band);
int ieee80211_add_ext_srates_ie(struct ieee80211_sub_if_data *sdata,
				struct sk_buff *skb, bool need_basic,
				enum nl80211_band band);
u8 *ieee80211_add_wmm_info_ie(u8 *buf, u8 qosinfo);
void ieee80211_add_s1g_capab_ie(struct ieee80211_sub_if_data *sdata,
				struct ieee80211_sta_s1g_cap *caps,
				struct sk_buff *skb);
void ieee80211_add_aid_request_ie(struct ieee80211_sub_if_data *sdata,
				  struct sk_buff *skb);
u8 *ieee80211_ie_build_s1g_cap(u8 *pos, struct ieee80211_sta_s1g_cap *s1g_cap);
bool ieee80211_chandef_ht_oper(const struct ieee80211_ht_operation *ht_oper,
			       struct cfg80211_chan_def *chandef);
bool ieee80211_chandef_vht_oper(struct ieee80211_hw *hw, u32 vht_cap_info,
				const struct ieee80211_vht_operation *oper,
				const struct ieee80211_ht_operation *htop,
				struct cfg80211_chan_def *chandef);
void ieee80211_chandef_eht_oper(const struct ieee80211_eht_operation *eht_oper,
				bool support_160, bool support_320,
				struct cfg80211_chan_def *chandef);
bool ieee80211_chandef_he_6ghz_oper(struct ieee80211_sub_if_data *sdata,
				    const struct ieee80211_he_operation *he_oper,
				    const struct ieee80211_eht_operation *eht_oper,
				    struct cfg80211_chan_def *chandef);
bool ieee80211_chandef_s1g_oper(const struct ieee80211_s1g_oper_ie *oper,
				struct cfg80211_chan_def *chandef);
ieee80211_conn_flags_t ieee80211_chandef_downgrade(struct cfg80211_chan_def *c);
int __must_check
ieee80211_link_use_channel(struct ieee80211_link_data *link,
			   const struct cfg80211_chan_def *chandef,
			   enum ieee80211_chanctx_mode mode);
int __must_check
ieee80211_link_reserve_chanctx(struct ieee80211_link_data *link,
			       const struct cfg80211_chan_def *chandef,
			       enum ieee80211_chanctx_mode mode,
			       bool radar_required);
int __must_check
ieee80211_link_use_reserved_context(struct ieee80211_link_data *link);
int ieee80211_link_unreserve_chanctx(struct ieee80211_link_data *link);
int __must_check
ieee80211_link_change_bandwidth(struct ieee80211_link_data *link,
				const struct cfg80211_chan_def *chandef,
				u64 *changed);
void ieee80211_link_release_channel(struct ieee80211_link_data *link);
void ieee80211_link_vlan_copy_chanctx(struct ieee80211_link_data *link);
void ieee80211_link_copy_chanctx_to_vlans(struct ieee80211_link_data *link,
					  bool clear);
int ieee80211_chanctx_refcount(struct ieee80211_local *local,
			       struct ieee80211_chanctx *ctx);
void ieee80211_recalc_smps_chanctx(struct ieee80211_local *local,
				   struct ieee80211_chanctx *chanctx);
void ieee80211_recalc_chanctx_min_def(struct ieee80211_local *local,
				      struct ieee80211_chanctx *ctx,
				      struct ieee80211_link_data *rsvd_for);
bool ieee80211_is_radar_required(struct ieee80211_local *local);
void ieee80211_dfs_cac_timer_work(struct work_struct *work);
void ieee80211_dfs_cac_cancel(struct ieee80211_local *local);
void ieee80211_dfs_radar_detected_work(struct wiphy *wiphy,
				       struct wiphy_work *work);
int ieee80211_send_action_csa(struct ieee80211_sub_if_data *sdata,
			      struct cfg80211_csa_settings *csa_settings);
void ieee80211_recalc_dtim(struct ieee80211_local *local,
			   struct ieee80211_sub_if_data *sdata);
int ieee80211_check_combinations(struct ieee80211_sub_if_data *sdata,
				 const struct cfg80211_chan_def *chandef,
				 enum ieee80211_chanctx_mode chanmode,
				 u8 radar_detect);
int ieee80211_max_num_channels(struct ieee80211_local *local);
void ieee80211_recalc_chanctx_chantype(struct ieee80211_local *local,
				       struct ieee80211_chanctx *ctx);
int ieee80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
			const u8 *peer, int link_id,
			u8 action_code, u8 dialog_token, u16 status_code,
			u32 peer_capability, bool initiator,
			const u8 *extra_ies, size_t extra_ies_len);
int ieee80211_tdls_oper(struct wiphy *wiphy, struct net_device *dev,
			const u8 *peer, enum nl80211_tdls_operation oper);
void ieee80211_tdls_peer_del_work(struct work_struct *wk);
int ieee80211_tdls_channel_switch(struct wiphy *wiphy, struct net_device *dev,
				  const u8 *addr, u8 oper_class,
				  struct cfg80211_chan_def *chandef);
void ieee80211_tdls_cancel_channel_switch(struct wiphy *wiphy,
					  struct net_device *dev,
					  const u8 *addr);
void ieee80211_teardown_tdls_peers(struct ieee80211_sub_if_data *sdata);
void ieee80211_tdls_handle_disconnect(struct ieee80211_sub_if_data *sdata,
				      const u8 *peer, u16 reason);
void
ieee80211_process_tdls_channel_switch(struct ieee80211_sub_if_data *sdata,
				      struct sk_buff *skb);
const char *ieee80211_get_reason_code_string(u16 reason_code);
u16 ieee80211_encode_usf(int val);
u8 *ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len,
			enum nl80211_iftype type);
extern const struct ethtool_ops ieee80211_ethtool_ops;
u32 ieee80211_calc_expected_tx_airtime(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_sta *pubsta,
				       int len, bool ampdu);
#ifdef CONFIG_MAC80211_NOINLINE
#define debug_noinline noinline
#else
#define debug_noinline
#endif
void ieee80211_init_frag_cache(struct ieee80211_fragment_cache *cache);
void ieee80211_destroy_frag_cache(struct ieee80211_fragment_cache *cache);
u8 ieee80211_ie_len_eht_cap(struct ieee80211_sub_if_data *sdata, u8 iftype);
u8 *ieee80211_ie_build_eht_cap(u8 *pos,
			       const struct ieee80211_sta_he_cap *he_cap,
			       const struct ieee80211_sta_eht_cap *eht_cap,
			       u8 *end,
			       bool for_ap);
void
ieee80211_eht_cap_ie_to_sta_eht_cap(struct ieee80211_sub_if_data *sdata,
				    struct ieee80211_supported_band *sband,
				    const u8 *he_cap_ie, u8 he_cap_len,
				    const struct ieee80211_eht_cap_elem *eht_cap_ie_elem,
				    u8 eht_cap_len,
				    struct link_sta_info *link_sta);
#endif  

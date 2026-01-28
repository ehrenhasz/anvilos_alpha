#ifndef STA_INFO_H
#define STA_INFO_H
#include <linux/list.h>
#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/workqueue.h>
#include <linux/average.h>
#include <linux/bitfield.h>
#include <linux/etherdevice.h>
#include <linux/rhashtable.h>
#include <linux/u64_stats_sync.h>
#include "key.h"
enum ieee80211_sta_info_flags {
	WLAN_STA_AUTH,
	WLAN_STA_ASSOC,
	WLAN_STA_PS_STA,
	WLAN_STA_AUTHORIZED,
	WLAN_STA_SHORT_PREAMBLE,
	WLAN_STA_WDS,
	WLAN_STA_CLEAR_PS_FILT,
	WLAN_STA_MFP,
	WLAN_STA_BLOCK_BA,
	WLAN_STA_PS_DRIVER,
	WLAN_STA_PSPOLL,
	WLAN_STA_TDLS_PEER,
	WLAN_STA_TDLS_PEER_AUTH,
	WLAN_STA_TDLS_INITIATOR,
	WLAN_STA_TDLS_CHAN_SWITCH,
	WLAN_STA_TDLS_OFF_CHANNEL,
	WLAN_STA_TDLS_WIDER_BW,
	WLAN_STA_UAPSD,
	WLAN_STA_SP,
	WLAN_STA_4ADDR_EVENT,
	WLAN_STA_INSERTED,
	WLAN_STA_RATE_CONTROL,
	WLAN_STA_TOFFSET_KNOWN,
	WLAN_STA_MPSP_OWNER,
	WLAN_STA_MPSP_RECIPIENT,
	WLAN_STA_PS_DELIVER,
	WLAN_STA_USES_ENCRYPTION,
	WLAN_STA_DECAP_OFFLOAD,
	NUM_WLAN_STA_FLAGS,
};
#define ADDBA_RESP_INTERVAL HZ
#define HT_AGG_MAX_RETRIES		15
#define HT_AGG_BURST_RETRIES		3
#define HT_AGG_RETRIES_PERIOD		(15 * HZ)
#define HT_AGG_STATE_DRV_READY		0
#define HT_AGG_STATE_RESPONSE_RECEIVED	1
#define HT_AGG_STATE_OPERATIONAL	2
#define HT_AGG_STATE_STOPPING		3
#define HT_AGG_STATE_WANT_START		4
#define HT_AGG_STATE_WANT_STOP		5
#define HT_AGG_STATE_START_CB		6
#define HT_AGG_STATE_STOP_CB		7
#define HT_AGG_STATE_SENT_ADDBA		8
DECLARE_EWMA(avg_signal, 10, 8)
enum ieee80211_agg_stop_reason {
	AGG_STOP_DECLINED,
	AGG_STOP_LOCAL_REQUEST,
	AGG_STOP_PEER_REQUEST,
	AGG_STOP_DESTROY_STA,
};
#define AIRTIME_USE_TX		BIT(0)
#define AIRTIME_USE_RX		BIT(1)
struct airtime_info {
	u64 rx_airtime;
	u64 tx_airtime;
	u32 last_active;
	s32 deficit;
	atomic_t aql_tx_pending;  
	u32 aql_limit_low;
	u32 aql_limit_high;
};
void ieee80211_sta_update_pending_airtime(struct ieee80211_local *local,
					  struct sta_info *sta, u8 ac,
					  u16 tx_airtime, bool tx_completed);
struct sta_info;
struct tid_ampdu_tx {
	struct rcu_head rcu_head;
	struct timer_list session_timer;
	struct timer_list addba_resp_timer;
	struct sk_buff_head pending;
	struct sta_info *sta;
	unsigned long state;
	unsigned long last_tx;
	u16 timeout;
	u8 dialog_token;
	u8 stop_initiator;
	bool tx_stop;
	u16 buf_size;
	u16 ssn;
	u16 failed_bar_ssn;
	bool bar_pending;
	bool amsdu;
	u8 tid;
};
struct tid_ampdu_rx {
	struct rcu_head rcu_head;
	spinlock_t reorder_lock;
	u64 reorder_buf_filtered;
	struct sk_buff_head *reorder_buf;
	unsigned long *reorder_time;
	struct sta_info *sta;
	struct timer_list session_timer;
	struct timer_list reorder_timer;
	unsigned long last_rx;
	u16 head_seq_num;
	u16 stored_mpdu_num;
	u16 ssn;
	u16 buf_size;
	u16 timeout;
	u8 tid;
	u8 auto_seq:1,
	   removed:1,
	   started:1;
};
struct sta_ampdu_mlme {
	struct mutex mtx;
	struct tid_ampdu_rx __rcu *tid_rx[IEEE80211_NUM_TIDS];
	u8 tid_rx_token[IEEE80211_NUM_TIDS];
	unsigned long tid_rx_timer_expired[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
	unsigned long tid_rx_stop_requested[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
	unsigned long tid_rx_manage_offl[BITS_TO_LONGS(2 * IEEE80211_NUM_TIDS)];
	unsigned long agg_session_valid[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
	unsigned long unexpected_agg[BITS_TO_LONGS(IEEE80211_NUM_TIDS)];
	struct work_struct work;
	struct tid_ampdu_tx __rcu *tid_tx[IEEE80211_NUM_TIDS];
	struct tid_ampdu_tx *tid_start_tx[IEEE80211_NUM_TIDS];
	unsigned long last_addba_req_time[IEEE80211_NUM_TIDS];
	u8 addba_req_num[IEEE80211_NUM_TIDS];
	u8 dialog_token_allocator;
};
#define IEEE80211_TID_UNRESERVED	0xff
#define IEEE80211_FAST_XMIT_MAX_IV	18
struct ieee80211_fast_tx {
	struct ieee80211_key *key;
	u8 hdr_len;
	u8 sa_offs, da_offs, pn_offs;
	u8 band;
	u8 hdr[30 + 2 + IEEE80211_FAST_XMIT_MAX_IV +
	       sizeof(rfc1042_header)] __aligned(2);
	struct rcu_head rcu_head;
};
struct ieee80211_fast_rx {
	struct net_device *dev;
	enum nl80211_iftype vif_type;
	u8 vif_addr[ETH_ALEN] __aligned(2);
	u8 rfc1042_hdr[6] __aligned(2);
	__be16 control_port_protocol;
	__le16 expected_ds_bits;
	u8 icv_len;
	u8 key:1,
	   internal_forward:1,
	   uses_rss:1;
	u8 da_offs, sa_offs;
	struct rcu_head rcu_head;
};
DECLARE_EWMA(mesh_fail_avg, 20, 8)
DECLARE_EWMA(mesh_tx_rate_avg, 8, 16)
struct mesh_sta {
	struct timer_list plink_timer;
	struct sta_info *plink_sta;
	s64 t_offset;
	s64 t_offset_setpoint;
	spinlock_t plink_lock;
	u16 llid;
	u16 plid;
	u16 aid;
	u16 reason;
	u8 plink_retries;
	bool processed_beacon;
	bool connected_to_gate;
	bool connected_to_as;
	enum nl80211_plink_state plink_state;
	u32 plink_timeout;
	enum nl80211_mesh_power_mode local_pm;
	enum nl80211_mesh_power_mode peer_pm;
	enum nl80211_mesh_power_mode nonpeer_pm;
	struct ewma_mesh_fail_avg fail_avg;
	struct ewma_mesh_tx_rate_avg tx_rate_avg;
};
DECLARE_EWMA(signal, 10, 8)
struct ieee80211_sta_rx_stats {
	unsigned long packets;
	unsigned long last_rx;
	unsigned long num_duplicates;
	unsigned long fragments;
	unsigned long dropped;
	int last_signal;
	u8 chains;
	s8 chain_signal_last[IEEE80211_MAX_CHAINS];
	u32 last_rate;
	struct u64_stats_sync syncp;
	u64 bytes;
	u64 msdu[IEEE80211_NUM_TIDS + 1];
};
#define IEEE80211_FRAGMENT_MAX 4
struct ieee80211_fragment_entry {
	struct sk_buff_head skb_list;
	unsigned long first_frag_time;
	u16 seq;
	u16 extra_len;
	u16 last_frag;
	u8 rx_queue;
	u8 check_sequential_pn:1,  
	   is_protected:1;
	u8 last_pn[6];  
	unsigned int key_color;
};
struct ieee80211_fragment_cache {
	struct ieee80211_fragment_entry	entries[IEEE80211_FRAGMENT_MAX];
	unsigned int next;
};
#define STA_SLOW_THRESHOLD 6000  
struct link_sta_info {
	u8 addr[ETH_ALEN];
	u8 link_id;
	struct rhlist_head link_hash_node;
	struct sta_info *sta;
	struct ieee80211_key __rcu *gtk[NUM_DEFAULT_KEYS +
					NUM_DEFAULT_MGMT_KEYS +
					NUM_DEFAULT_BEACON_KEYS];
	struct ieee80211_sta_rx_stats __percpu *pcpu_rx_stats;
	struct ieee80211_sta_rx_stats rx_stats;
	struct {
		struct ewma_signal signal;
		struct ewma_signal chain_signal[IEEE80211_MAX_CHAINS];
	} rx_stats_avg;
	struct {
		unsigned long filtered;
		unsigned long retry_failed, retry_count;
		unsigned int lost_packets;
		unsigned long last_pkt_time;
		u64 msdu_retries[IEEE80211_NUM_TIDS + 1];
		u64 msdu_failed[IEEE80211_NUM_TIDS + 1];
		unsigned long last_ack;
		s8 last_ack_signal;
		bool ack_signal_filled;
		struct ewma_avg_signal avg_ack_signal;
	} status_stats;
	struct {
		u64 packets[IEEE80211_NUM_ACS];
		u64 bytes[IEEE80211_NUM_ACS];
		struct ieee80211_tx_rate last_rate;
		struct rate_info last_rate_info;
		u64 msdu[IEEE80211_NUM_TIDS + 1];
	} tx_stats;
	enum ieee80211_sta_rx_bandwidth cur_max_bandwidth;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *debugfs_dir;
#endif
	struct ieee80211_link_sta *pub;
};
struct sta_info {
	struct list_head list, free_list;
	struct rcu_head rcu_head;
	struct rhlist_head hash_node;
	u8 addr[ETH_ALEN];
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct ieee80211_key __rcu *ptk[NUM_DEFAULT_KEYS];
	u8 ptk_idx;
	struct rate_control_ref *rate_ctrl;
	void *rate_ctrl_priv;
	spinlock_t rate_ctrl_lock;
	spinlock_t lock;
	struct ieee80211_fast_tx __rcu *fast_tx;
	struct ieee80211_fast_rx __rcu *fast_rx;
#ifdef CONFIG_MAC80211_MESH
	struct mesh_sta *mesh;
#endif
	struct work_struct drv_deliver_wk;
	u16 listen_interval;
	bool dead;
	bool removed;
	bool uploaded;
	enum ieee80211_sta_state sta_state;
	unsigned long _flags;
	spinlock_t ps_lock;
	struct sk_buff_head ps_tx_buf[IEEE80211_NUM_ACS];
	struct sk_buff_head tx_filtered[IEEE80211_NUM_ACS];
	unsigned long driver_buffered_tids;
	unsigned long txq_buffered_tids;
	u64 assoc_at;
	long last_connected;
	__le16 last_seq_ctrl[IEEE80211_NUM_TIDS + 1];
	u16 tid_seq[IEEE80211_QOS_CTL_TID_MASK + 1];
	struct airtime_info airtime[IEEE80211_NUM_ACS];
	u16 airtime_weight;
	struct sta_ampdu_mlme ampdu_mlme;
#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *debugfs_dir;
#endif
	struct codel_params cparams;
	u8 reserved_tid;
	s8 amsdu_mesh_control;
	struct cfg80211_chan_def tdls_chandef;
	struct ieee80211_fragment_cache frags;
	struct ieee80211_sta_aggregates cur;
	struct link_sta_info deflink;
	struct link_sta_info __rcu *link[IEEE80211_MLD_MAX_NUM_LINKS];
	struct ieee80211_sta sta;
};
static inline enum nl80211_plink_state sta_plink_state(struct sta_info *sta)
{
#ifdef CONFIG_MAC80211_MESH
	return sta->mesh->plink_state;
#endif
	return NL80211_PLINK_LISTEN;
}
static inline void set_sta_flag(struct sta_info *sta,
				enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	set_bit(flag, &sta->_flags);
}
static inline void clear_sta_flag(struct sta_info *sta,
				  enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	clear_bit(flag, &sta->_flags);
}
static inline int test_sta_flag(struct sta_info *sta,
				enum ieee80211_sta_info_flags flag)
{
	return test_bit(flag, &sta->_flags);
}
static inline int test_and_clear_sta_flag(struct sta_info *sta,
					  enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	return test_and_clear_bit(flag, &sta->_flags);
}
static inline int test_and_set_sta_flag(struct sta_info *sta,
					enum ieee80211_sta_info_flags flag)
{
	WARN_ON(flag == WLAN_STA_AUTH ||
		flag == WLAN_STA_ASSOC ||
		flag == WLAN_STA_AUTHORIZED);
	return test_and_set_bit(flag, &sta->_flags);
}
int sta_info_move_state(struct sta_info *sta,
			enum ieee80211_sta_state new_state);
static inline void sta_info_pre_move_state(struct sta_info *sta,
					   enum ieee80211_sta_state new_state)
{
	int ret;
	WARN_ON_ONCE(test_sta_flag(sta, WLAN_STA_INSERTED));
	ret = sta_info_move_state(sta, new_state);
	WARN_ON_ONCE(ret);
}
void ieee80211_assign_tid_tx(struct sta_info *sta, int tid,
			     struct tid_ampdu_tx *tid_tx);
static inline struct tid_ampdu_tx *
rcu_dereference_protected_tid_tx(struct sta_info *sta, int tid)
{
	return rcu_dereference_protected(sta->ampdu_mlme.tid_tx[tid],
					 lockdep_is_held(&sta->lock) ||
					 lockdep_is_held(&sta->ampdu_mlme.mtx));
}
#define STA_MAX_TX_BUFFER	64
#define STA_TX_BUFFER_EXPIRE (10 * HZ)
#define STA_INFO_CLEANUP_INTERVAL (10 * HZ)
struct rhlist_head *sta_info_hash_lookup(struct ieee80211_local *local,
					 const u8 *addr);
struct sta_info *sta_info_get(struct ieee80211_sub_if_data *sdata,
			      const u8 *addr);
struct sta_info *sta_info_get_bss(struct ieee80211_sub_if_data *sdata,
				  const u8 *addr);
struct sta_info *sta_info_get_by_addrs(struct ieee80211_local *local,
				       const u8 *sta_addr, const u8 *vif_addr);
#define for_each_sta_info(local, _addr, _sta, _tmp)			\
	rhl_for_each_entry_rcu(_sta, _tmp,				\
			       sta_info_hash_lookup(local, _addr), hash_node)
struct rhlist_head *link_sta_info_hash_lookup(struct ieee80211_local *local,
					      const u8 *addr);
#define for_each_link_sta_info(local, _addr, _sta, _tmp)		\
	rhl_for_each_entry_rcu(_sta, _tmp,				\
			       link_sta_info_hash_lookup(local, _addr),	\
			       link_hash_node)
struct link_sta_info *
link_sta_info_get_bss(struct ieee80211_sub_if_data *sdata, const u8 *addr);
struct sta_info *sta_info_get_by_idx(struct ieee80211_sub_if_data *sdata,
				     int idx);
struct sta_info *sta_info_alloc(struct ieee80211_sub_if_data *sdata,
				const u8 *addr, gfp_t gfp);
struct sta_info *sta_info_alloc_with_link(struct ieee80211_sub_if_data *sdata,
					  const u8 *mld_addr,
					  unsigned int link_id,
					  const u8 *link_addr,
					  gfp_t gfp);
void sta_info_free(struct ieee80211_local *local, struct sta_info *sta);
int sta_info_insert(struct sta_info *sta);
int sta_info_insert_rcu(struct sta_info *sta) __acquires(RCU);
int __must_check __sta_info_destroy(struct sta_info *sta);
int sta_info_destroy_addr(struct ieee80211_sub_if_data *sdata,
			  const u8 *addr);
int sta_info_destroy_addr_bss(struct ieee80211_sub_if_data *sdata,
			      const u8 *addr);
void sta_info_recalc_tim(struct sta_info *sta);
int sta_info_init(struct ieee80211_local *local);
void sta_info_stop(struct ieee80211_local *local);
int __sta_info_flush(struct ieee80211_sub_if_data *sdata, bool vlans);
static inline int sta_info_flush(struct ieee80211_sub_if_data *sdata)
{
	return __sta_info_flush(sdata, false);
}
void sta_set_rate_info_tx(struct sta_info *sta,
			  const struct ieee80211_tx_rate *rate,
			  struct rate_info *rinfo);
void sta_set_sinfo(struct sta_info *sta, struct station_info *sinfo,
		   bool tidstats);
u32 sta_get_expected_throughput(struct sta_info *sta);
void ieee80211_sta_expire(struct ieee80211_sub_if_data *sdata,
			  unsigned long exp_time);
int ieee80211_sta_allocate_link(struct sta_info *sta, unsigned int link_id);
void ieee80211_sta_free_link(struct sta_info *sta, unsigned int link_id);
int ieee80211_sta_activate_link(struct sta_info *sta, unsigned int link_id);
void ieee80211_sta_remove_link(struct sta_info *sta, unsigned int link_id);
void ieee80211_sta_ps_deliver_wakeup(struct sta_info *sta);
void ieee80211_sta_ps_deliver_poll_response(struct sta_info *sta);
void ieee80211_sta_ps_deliver_uapsd(struct sta_info *sta);
unsigned long ieee80211_sta_last_active(struct sta_info *sta);
void ieee80211_sta_set_max_amsdu_subframes(struct sta_info *sta,
					   const u8 *ext_capab,
					   unsigned int ext_capab_len);
void __ieee80211_sta_recalc_aggregates(struct sta_info *sta, u16 active_links);
enum sta_stats_type {
	STA_STATS_RATE_TYPE_INVALID = 0,
	STA_STATS_RATE_TYPE_LEGACY,
	STA_STATS_RATE_TYPE_HT,
	STA_STATS_RATE_TYPE_VHT,
	STA_STATS_RATE_TYPE_HE,
	STA_STATS_RATE_TYPE_S1G,
	STA_STATS_RATE_TYPE_EHT,
};
#define STA_STATS_FIELD_HT_MCS		GENMASK( 7,  0)
#define STA_STATS_FIELD_LEGACY_IDX	GENMASK( 3,  0)
#define STA_STATS_FIELD_LEGACY_BAND	GENMASK( 7,  4)
#define STA_STATS_FIELD_VHT_MCS		GENMASK( 3,  0)
#define STA_STATS_FIELD_VHT_NSS		GENMASK( 7,  4)
#define STA_STATS_FIELD_HE_MCS		GENMASK( 3,  0)
#define STA_STATS_FIELD_HE_NSS		GENMASK( 7,  4)
#define STA_STATS_FIELD_EHT_MCS		GENMASK( 3,  0)
#define STA_STATS_FIELD_EHT_NSS		GENMASK( 7,  4)
#define STA_STATS_FIELD_BW		GENMASK(12,  8)
#define STA_STATS_FIELD_SGI		GENMASK(13, 13)
#define STA_STATS_FIELD_TYPE		GENMASK(16, 14)
#define STA_STATS_FIELD_HE_RU		GENMASK(19, 17)
#define STA_STATS_FIELD_HE_GI		GENMASK(21, 20)
#define STA_STATS_FIELD_HE_DCM		GENMASK(22, 22)
#define STA_STATS_FIELD_EHT_RU		GENMASK(20, 17)
#define STA_STATS_FIELD_EHT_GI		GENMASK(22, 21)
#define STA_STATS_FIELD(_n, _v)		FIELD_PREP(STA_STATS_FIELD_ ## _n, _v)
#define STA_STATS_GET(_n, _v)		FIELD_GET(STA_STATS_FIELD_ ## _n, _v)
#define STA_STATS_RATE_INVALID		0
static inline u32 sta_stats_encode_rate(struct ieee80211_rx_status *s)
{
	u32 r;
	r = STA_STATS_FIELD(BW, s->bw);
	if (s->enc_flags & RX_ENC_FLAG_SHORT_GI)
		r |= STA_STATS_FIELD(SGI, 1);
	switch (s->encoding) {
	case RX_ENC_VHT:
		r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_VHT);
		r |= STA_STATS_FIELD(VHT_NSS, s->nss);
		r |= STA_STATS_FIELD(VHT_MCS, s->rate_idx);
		break;
	case RX_ENC_HT:
		r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_HT);
		r |= STA_STATS_FIELD(HT_MCS, s->rate_idx);
		break;
	case RX_ENC_LEGACY:
		r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_LEGACY);
		r |= STA_STATS_FIELD(LEGACY_BAND, s->band);
		r |= STA_STATS_FIELD(LEGACY_IDX, s->rate_idx);
		break;
	case RX_ENC_HE:
		r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_HE);
		r |= STA_STATS_FIELD(HE_NSS, s->nss);
		r |= STA_STATS_FIELD(HE_MCS, s->rate_idx);
		r |= STA_STATS_FIELD(HE_GI, s->he_gi);
		r |= STA_STATS_FIELD(HE_RU, s->he_ru);
		r |= STA_STATS_FIELD(HE_DCM, s->he_dcm);
		break;
	case RX_ENC_EHT:
		r |= STA_STATS_FIELD(TYPE, STA_STATS_RATE_TYPE_EHT);
		r |= STA_STATS_FIELD(EHT_NSS, s->nss);
		r |= STA_STATS_FIELD(EHT_MCS, s->rate_idx);
		r |= STA_STATS_FIELD(EHT_GI, s->eht.gi);
		r |= STA_STATS_FIELD(EHT_RU, s->eht.ru);
		break;
	default:
		WARN_ON(1);
		return STA_STATS_RATE_INVALID;
	}
	return r;
}
#endif  

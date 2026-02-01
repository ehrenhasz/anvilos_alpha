 
 
#ifndef __sta_h__
#define __sta_h__

#include <linux/spinlock.h>
#include <net/mac80211.h>
#include <linux/wait.h>

#include "iwl-trans.h"  
#include "fw-api.h"  
#include "rs.h"

struct iwl_mvm;
struct iwl_mvm_vif;

 

 

 

 

 

 

 

 

 
enum iwl_mvm_agg_state {
	IWL_AGG_OFF = 0,
	IWL_AGG_QUEUED,
	IWL_AGG_STARTING,
	IWL_AGG_ON,
	IWL_EMPTYING_HW_QUEUE_ADDBA,
	IWL_EMPTYING_HW_QUEUE_DELBA,
};

 
struct iwl_mvm_tid_data {
	u16 seq_number;
	u16 next_reclaimed;
	 
	u32 rate_n_flags;
	u8 lq_color;
	bool amsdu_in_ampdu_allowed;
	enum iwl_mvm_agg_state state;
	u16 txq_id;
	u16 ssn;
	u16 tx_time;
	unsigned long tpt_meas_start;
	u32 tx_count_last;
	u32 tx_count;
};

struct iwl_mvm_key_pn {
	struct rcu_head rcu_head;
	struct {
		u8 pn[IWL_MAX_TID_COUNT][IEEE80211_CCMP_PN_LEN];
	} ____cacheline_aligned_in_smp q[];
};

 
enum iwl_mvm_rxq_notif_type {
	IWL_MVM_RXQ_EMPTY,
	IWL_MVM_RXQ_NOTIF_DEL_BA,
	IWL_MVM_RXQ_NSSN_SYNC,
};

 
struct iwl_mvm_internal_rxq_notif {
	u16 type;
	u16 sync;
	u32 cookie;
	u8 data[];
} __packed;

struct iwl_mvm_delba_data {
	u32 baid;
} __packed;

struct iwl_mvm_nssn_sync_data {
	u32 baid;
	u32 nssn;
} __packed;

 
struct iwl_mvm_rxq_dup_data {
	__le16 last_seq[IWL_MAX_TID_COUNT + 1];
	u8 last_sub_frame[IWL_MAX_TID_COUNT + 1];
} ____cacheline_aligned_in_smp;

 
struct iwl_mvm_link_sta {
	struct rcu_head rcu_head;
	u32 sta_id;
	union {
		struct iwl_lq_sta_rs_fw rs_fw;
		struct iwl_lq_sta rs_drv;
	} lq_sta;

	u16 orig_amsdu_len;

	u8 avg_energy;
};

 
struct iwl_mvm_sta {
	u32 tfd_queue_msk;
	u32 mac_id_n_color;
	u16 tid_disable_agg;
	u8 sta_type;
	enum ieee80211_sta_state sta_state;
	bool bt_reduced_txpower;
	bool next_status_eosp;
	bool authorized;
	spinlock_t lock;
	struct iwl_mvm_tid_data tid_data[IWL_MAX_TID_COUNT + 1];
	u8 tid_to_baid[IWL_MAX_TID_COUNT];
	struct ieee80211_vif *vif;
	struct iwl_mvm_key_pn __rcu *ptk_pn[4];
	struct iwl_mvm_rxq_dup_data *dup_data;

	u8 reserved_queue;

	 
	s8 tx_protection;
	bool tt_tx_protection;

	bool disable_tx;
	u16 amsdu_enabled;
	u16 max_amsdu_len;
	bool sleeping;
	u8 agg_tids;
	u8 sleep_tx_count;
	u8 tx_ant;
	u32 pairwise_cipher;

	struct iwl_mvm_link_sta deflink;
	struct iwl_mvm_link_sta __rcu *link[IEEE80211_MLD_MAX_NUM_LINKS];
};

u16 iwl_mvm_tid_queued(struct iwl_mvm *mvm, struct iwl_mvm_tid_data *tid_data);

static inline struct iwl_mvm_sta *
iwl_mvm_sta_from_mac80211(struct ieee80211_sta *sta)
{
	return (void *)sta->drv_priv;
}

 
struct iwl_mvm_int_sta {
	u32 sta_id;
	u8 type;
	u32 tfd_queue_msk;
};

 
int iwl_mvm_sta_send_to_fw(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
			   bool update, unsigned int flags);
int iwl_mvm_find_free_sta_id(struct iwl_mvm *mvm, enum nl80211_iftype iftype);
int iwl_mvm_sta_init(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
		     struct ieee80211_sta *sta, int sta_id, u8 sta_type);
int iwl_mvm_add_sta(struct iwl_mvm *mvm,
		    struct ieee80211_vif *vif,
		    struct ieee80211_sta *sta);

static inline int iwl_mvm_update_sta(struct iwl_mvm *mvm,
				     struct ieee80211_vif *vif,
				     struct ieee80211_sta *sta)
{
	return iwl_mvm_sta_send_to_fw(mvm, sta, true, 0);
}

void iwl_mvm_realloc_queues_after_restart(struct iwl_mvm *mvm,
					  struct ieee80211_sta *sta);
int iwl_mvm_wait_sta_queues_empty(struct iwl_mvm *mvm,
				  struct iwl_mvm_sta *mvm_sta);
bool iwl_mvm_sta_del(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
		     struct ieee80211_sta *sta,
		     struct ieee80211_link_sta *link_sta, int *ret);
int iwl_mvm_rm_sta(struct iwl_mvm *mvm,
		   struct ieee80211_vif *vif,
		   struct ieee80211_sta *sta);
int iwl_mvm_rm_sta_id(struct iwl_mvm *mvm,
		      struct ieee80211_vif *vif,
		      u8 sta_id);
int iwl_mvm_set_sta_key(struct iwl_mvm *mvm,
			struct ieee80211_vif *vif,
			struct ieee80211_sta *sta,
			struct ieee80211_key_conf *keyconf,
			u8 key_offset);
int iwl_mvm_remove_sta_key(struct iwl_mvm *mvm,
			   struct ieee80211_vif *vif,
			   struct ieee80211_sta *sta,
			   struct ieee80211_key_conf *keyconf);

void iwl_mvm_update_tkip_key(struct iwl_mvm *mvm,
			     struct ieee80211_vif *vif,
			     struct ieee80211_key_conf *keyconf,
			     struct ieee80211_sta *sta, u32 iv32,
			     u16 *phase1key);

void iwl_mvm_rx_eosp_notif(struct iwl_mvm *mvm,
			   struct iwl_rx_cmd_buffer *rxb);

 
int iwl_mvm_sta_rx_agg(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
		       int tid, u16 ssn, bool start, u16 buf_size, u16 timeout);
int iwl_mvm_sta_tx_agg_start(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			struct ieee80211_sta *sta, u16 tid, u16 *ssn);
int iwl_mvm_sta_tx_agg_oper(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, u16 tid, u16 buf_size,
			    bool amsdu);
int iwl_mvm_sta_tx_agg_stop(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, u16 tid);
int iwl_mvm_sta_tx_agg_flush(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			    struct ieee80211_sta *sta, u16 tid);

int iwl_mvm_sta_tx_agg(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
		       int tid, u8 queue, bool start);

int iwl_mvm_add_aux_sta(struct iwl_mvm *mvm, u32 lmac_id);
int iwl_mvm_rm_aux_sta(struct iwl_mvm *mvm);

int iwl_mvm_alloc_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
void iwl_mvm_free_bcast_sta_queues(struct iwl_mvm *mvm,
				   struct ieee80211_vif *vif);
int iwl_mvm_send_add_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_add_p2p_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_send_rm_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_rm_p2p_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_add_mcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_rm_mcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_allocate_int_sta(struct iwl_mvm *mvm,
			     struct iwl_mvm_int_sta *sta,
				    u32 qmask, enum nl80211_iftype iftype,
				    u8 type);
void iwl_mvm_dealloc_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
void iwl_mvm_dealloc_int_sta(struct iwl_mvm *mvm, struct iwl_mvm_int_sta *sta);
int iwl_mvm_add_snif_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_rm_snif_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
void iwl_mvm_dealloc_snif_sta(struct iwl_mvm *mvm);

void iwl_mvm_sta_modify_ps_wake(struct iwl_mvm *mvm,
				struct ieee80211_sta *sta);
void iwl_mvm_sta_modify_sleep_tx_count(struct iwl_mvm *mvm,
				       struct ieee80211_sta *sta,
				       enum ieee80211_frame_release_type reason,
				       u16 cnt, u16 tids, bool more_data,
				       bool single_sta_queue);
int iwl_mvm_drain_sta(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta,
		      bool drain);
void iwl_mvm_sta_modify_disable_tx(struct iwl_mvm *mvm,
				   struct iwl_mvm_sta *mvmsta, bool disable);
void iwl_mvm_sta_modify_disable_tx_ap(struct iwl_mvm *mvm,
				      struct ieee80211_sta *sta,
				      bool disable);
void iwl_mvm_modify_all_sta_disable_tx(struct iwl_mvm *mvm,
				       struct iwl_mvm_vif *mvmvif,
				       bool disable);

void iwl_mvm_csa_client_absent(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
void iwl_mvm_add_new_dqa_stream_wk(struct work_struct *wk);
int iwl_mvm_add_pasn_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			 struct iwl_mvm_int_sta *sta, u8 *addr, u32 cipher,
			 u8 *key, u32 key_len);
void iwl_mvm_cancel_channel_switch(struct iwl_mvm *mvm,
				   struct ieee80211_vif *vif,
				   u32 id);
 
int iwl_mvm_tvqm_enable_txq(struct iwl_mvm *mvm,
			    struct ieee80211_sta *sta,
			    u8 sta_id, u8 tid, unsigned int timeout);

 
 
struct iwl_mvm_sta_state_ops {
	int (*add_sta)(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta);
	int (*update_sta)(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			  struct ieee80211_sta *sta);
	int (*rm_sta)(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
		      struct ieee80211_sta *sta);
	int (*mac_ctxt_changed)(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
				bool force_assoc_off);
};

int iwl_mvm_mac_sta_state_common(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 enum ieee80211_sta_state old_state,
				 enum ieee80211_sta_state new_state,
				 const struct iwl_mvm_sta_state_ops *callbacks);

 
 
int iwl_mvm_mld_add_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			      struct ieee80211_bss_conf *link_conf);
int iwl_mvm_mld_add_snif_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *link_conf);
int iwl_mvm_mld_add_mcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			      struct ieee80211_bss_conf *link_conf);
int iwl_mvm_mld_add_aux_sta(struct iwl_mvm *mvm, u32 lmac_id);
int iwl_mvm_mld_rm_bcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *link_conf);
int iwl_mvm_mld_rm_snif_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
int iwl_mvm_mld_rm_mcast_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			     struct ieee80211_bss_conf *link_conf);
int iwl_mvm_mld_rm_aux_sta(struct iwl_mvm *mvm);
int iwl_mvm_mld_add_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			struct ieee80211_sta *sta);
int iwl_mvm_mld_update_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			   struct ieee80211_sta *sta);
int iwl_mvm_mld_rm_sta(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
		       struct ieee80211_sta *sta);
int iwl_mvm_mld_rm_sta_id(struct iwl_mvm *mvm, u8 sta_id);
int iwl_mvm_mld_update_sta_links(struct iwl_mvm *mvm,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 u16 old_links, u16 new_links);
u32 iwl_mvm_sta_fw_id_mask(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
			   int filter_link_id);
int iwl_mvm_mld_add_int_sta_with_queue(struct iwl_mvm *mvm,
				       struct iwl_mvm_int_sta *sta,
				       const u8 *addr, int link_id,
				       u16 *queue, u8 tid,
				       unsigned int *_wdg_timeout);

 
void iwl_mvm_mld_modify_all_sta_disable_tx(struct iwl_mvm *mvm,
					   struct iwl_mvm_vif *mvmvif,
					   bool disable);
void iwl_mvm_mld_sta_modify_disable_tx(struct iwl_mvm *mvm,
				       struct iwl_mvm_sta *mvm_sta,
				       bool disable);
void iwl_mvm_mld_sta_modify_disable_tx_ap(struct iwl_mvm *mvm,
					  struct ieee80211_sta *sta,
					  bool disable);
#endif  

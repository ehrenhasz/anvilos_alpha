 
 

#ifndef __rs_h__
#define __rs_h__

#include <net/mac80211.h>

#include "iwl-config.h"

#include "fw-api.h"
#include "iwl-trans.h"

#define RS_NAME "iwl-mvm-rs"

struct iwl_rs_rate_info {
	u8 plcp;	   
	u8 plcp_ht_siso;   
	u8 plcp_ht_mimo2;  
	u8 plcp_vht_siso;
	u8 plcp_vht_mimo2;
	u8 prev_rs;       
	u8 next_rs;       
};

#define IWL_RATE_60M_PLCP 3

#define LINK_QUAL_MAX_RETRY_NUM 16

enum {
	IWL_RATE_6M_INDEX_TABLE = 0,
	IWL_RATE_9M_INDEX_TABLE,
	IWL_RATE_12M_INDEX_TABLE,
	IWL_RATE_18M_INDEX_TABLE,
	IWL_RATE_24M_INDEX_TABLE,
	IWL_RATE_36M_INDEX_TABLE,
	IWL_RATE_48M_INDEX_TABLE,
	IWL_RATE_54M_INDEX_TABLE,
	IWL_RATE_1M_INDEX_TABLE,
	IWL_RATE_2M_INDEX_TABLE,
	IWL_RATE_5M_INDEX_TABLE,
	IWL_RATE_11M_INDEX_TABLE,
	IWL_RATE_INVM_INDEX_TABLE = IWL_RATE_INVM_INDEX - 1,
};

 
#define	IWL_RATE_6M_MASK   (1 << IWL_RATE_6M_INDEX)
#define	IWL_RATE_9M_MASK   (1 << IWL_RATE_9M_INDEX)
#define	IWL_RATE_12M_MASK  (1 << IWL_RATE_12M_INDEX)
#define	IWL_RATE_18M_MASK  (1 << IWL_RATE_18M_INDEX)
#define	IWL_RATE_24M_MASK  (1 << IWL_RATE_24M_INDEX)
#define	IWL_RATE_36M_MASK  (1 << IWL_RATE_36M_INDEX)
#define	IWL_RATE_48M_MASK  (1 << IWL_RATE_48M_INDEX)
#define	IWL_RATE_54M_MASK  (1 << IWL_RATE_54M_INDEX)
#define IWL_RATE_60M_MASK  (1 << IWL_RATE_60M_INDEX)
#define	IWL_RATE_1M_MASK   (1 << IWL_RATE_1M_INDEX)
#define	IWL_RATE_2M_MASK   (1 << IWL_RATE_2M_INDEX)
#define	IWL_RATE_5M_MASK   (1 << IWL_RATE_5M_INDEX)
#define	IWL_RATE_11M_MASK  (1 << IWL_RATE_11M_INDEX)


 
enum {
	IWL_RATE_HT_SISO_MCS_0_PLCP = 0,
	IWL_RATE_HT_SISO_MCS_1_PLCP = 1,
	IWL_RATE_HT_SISO_MCS_2_PLCP = 2,
	IWL_RATE_HT_SISO_MCS_3_PLCP = 3,
	IWL_RATE_HT_SISO_MCS_4_PLCP = 4,
	IWL_RATE_HT_SISO_MCS_5_PLCP = 5,
	IWL_RATE_HT_SISO_MCS_6_PLCP = 6,
	IWL_RATE_HT_SISO_MCS_7_PLCP = 7,
	IWL_RATE_HT_MIMO2_MCS_0_PLCP = 0x8,
	IWL_RATE_HT_MIMO2_MCS_1_PLCP = 0x9,
	IWL_RATE_HT_MIMO2_MCS_2_PLCP = 0xA,
	IWL_RATE_HT_MIMO2_MCS_3_PLCP = 0xB,
	IWL_RATE_HT_MIMO2_MCS_4_PLCP = 0xC,
	IWL_RATE_HT_MIMO2_MCS_5_PLCP = 0xD,
	IWL_RATE_HT_MIMO2_MCS_6_PLCP = 0xE,
	IWL_RATE_HT_MIMO2_MCS_7_PLCP = 0xF,
	IWL_RATE_VHT_SISO_MCS_0_PLCP = 0,
	IWL_RATE_VHT_SISO_MCS_1_PLCP = 1,
	IWL_RATE_VHT_SISO_MCS_2_PLCP = 2,
	IWL_RATE_VHT_SISO_MCS_3_PLCP = 3,
	IWL_RATE_VHT_SISO_MCS_4_PLCP = 4,
	IWL_RATE_VHT_SISO_MCS_5_PLCP = 5,
	IWL_RATE_VHT_SISO_MCS_6_PLCP = 6,
	IWL_RATE_VHT_SISO_MCS_7_PLCP = 7,
	IWL_RATE_VHT_SISO_MCS_8_PLCP = 8,
	IWL_RATE_VHT_SISO_MCS_9_PLCP = 9,
	IWL_RATE_VHT_MIMO2_MCS_0_PLCP = 0x10,
	IWL_RATE_VHT_MIMO2_MCS_1_PLCP = 0x11,
	IWL_RATE_VHT_MIMO2_MCS_2_PLCP = 0x12,
	IWL_RATE_VHT_MIMO2_MCS_3_PLCP = 0x13,
	IWL_RATE_VHT_MIMO2_MCS_4_PLCP = 0x14,
	IWL_RATE_VHT_MIMO2_MCS_5_PLCP = 0x15,
	IWL_RATE_VHT_MIMO2_MCS_6_PLCP = 0x16,
	IWL_RATE_VHT_MIMO2_MCS_7_PLCP = 0x17,
	IWL_RATE_VHT_MIMO2_MCS_8_PLCP = 0x18,
	IWL_RATE_VHT_MIMO2_MCS_9_PLCP = 0x19,
	IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_MIMO2_MCS_INV_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_VHT_SISO_MCS_INV_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_VHT_MIMO2_MCS_INV_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_SISO_MCS_8_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_SISO_MCS_9_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_MIMO2_MCS_8_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
	IWL_RATE_HT_MIMO2_MCS_9_PLCP = IWL_RATE_HT_SISO_MCS_INV_PLCP,
};

#define IWL_RATES_MASK ((1 << IWL_RATE_COUNT) - 1)

#define IWL_INVALID_VALUE    -1

#define TPC_MAX_REDUCTION		15
#define TPC_NO_REDUCTION		0
#define TPC_INVALID			0xff

#define LINK_QUAL_AGG_FRAME_LIMIT_DEF	(63)
#define LINK_QUAL_AGG_FRAME_LIMIT_MAX	(63)
 
#define LINK_QUAL_AGG_FRAME_LIMIT_GEN2_DEF	(255)
#define LINK_QUAL_AGG_FRAME_LIMIT_GEN2_MAX	(255)
#define LINK_QUAL_AGG_FRAME_LIMIT_MIN	(0)

#define LQ_SIZE		2	 

 
#define IWL_AGG_TPT_THREHOLD	0
#define IWL_AGG_ALL_TID		0xff

enum iwl_table_type {
	LQ_NONE,
	LQ_LEGACY_G,	 
	LQ_LEGACY_A,
	LQ_HT_SISO,	 
	LQ_HT_MIMO2,
	LQ_VHT_SISO,     
	LQ_VHT_MIMO2,
	LQ_HE_SISO,      
	LQ_HE_MIMO2,
	LQ_MAX,
};

struct rs_rate {
	int index;
	enum iwl_table_type type;
	u8 ant;
	u32 bw;
	bool sgi;
	bool ldpc;
	bool stbc;
	bool bfer;
};


#define is_type_legacy(type) (((type) == LQ_LEGACY_G) || \
			      ((type) == LQ_LEGACY_A))
#define is_type_ht_siso(type) ((type) == LQ_HT_SISO)
#define is_type_ht_mimo2(type) ((type) == LQ_HT_MIMO2)
#define is_type_vht_siso(type) ((type) == LQ_VHT_SISO)
#define is_type_vht_mimo2(type) ((type) == LQ_VHT_MIMO2)
#define is_type_he_siso(type) ((type) == LQ_HE_SISO)
#define is_type_he_mimo2(type) ((type) == LQ_HE_MIMO2)
#define is_type_siso(type) (is_type_ht_siso(type) || is_type_vht_siso(type) || \
			    is_type_he_siso(type))
#define is_type_mimo2(type) (is_type_ht_mimo2(type) || \
			     is_type_vht_mimo2(type) || is_type_he_mimo2(type))
#define is_type_mimo(type) (is_type_mimo2(type))
#define is_type_ht(type) (is_type_ht_siso(type) || is_type_ht_mimo2(type))
#define is_type_vht(type) (is_type_vht_siso(type) || is_type_vht_mimo2(type))
#define is_type_he(type) (is_type_he_siso(type) || is_type_he_mimo2(type))
#define is_type_a_band(type) ((type) == LQ_LEGACY_A)
#define is_type_g_band(type) ((type) == LQ_LEGACY_G)

#define is_legacy(rate)       is_type_legacy((rate)->type)
#define is_ht_siso(rate)      is_type_ht_siso((rate)->type)
#define is_ht_mimo2(rate)     is_type_ht_mimo2((rate)->type)
#define is_vht_siso(rate)     is_type_vht_siso((rate)->type)
#define is_vht_mimo2(rate)    is_type_vht_mimo2((rate)->type)
#define is_siso(rate)         is_type_siso((rate)->type)
#define is_mimo2(rate)        is_type_mimo2((rate)->type)
#define is_mimo(rate)         is_type_mimo((rate)->type)
#define is_ht(rate)           is_type_ht((rate)->type)
#define is_vht(rate)          is_type_vht((rate)->type)
#define is_he(rate)           is_type_he((rate)->type)
#define is_a_band(rate)       is_type_a_band((rate)->type)
#define is_g_band(rate)       is_type_g_band((rate)->type)

#define is_ht20(rate)         ((rate)->bw == RATE_MCS_CHAN_WIDTH_20)
#define is_ht40(rate)         ((rate)->bw == RATE_MCS_CHAN_WIDTH_40)
#define is_ht80(rate)         ((rate)->bw == RATE_MCS_CHAN_WIDTH_80)
#define is_ht160(rate)        ((rate)->bw == RATE_MCS_CHAN_WIDTH_160)

 

struct iwl_lq_sta_rs_fw {
	 
	u32 last_rate_n_flags;

	 
	struct lq_sta_pers_rs_fw {
		u32 sta_id;
#ifdef CONFIG_MAC80211_DEBUGFS
		u32 dbg_fixed_rate;
		u16 dbg_agg_frame_count_lim;
#endif
		u8 chains;
		s8 chain_signal[IEEE80211_MAX_CHAINS];
		s8 last_rssi;
		struct iwl_mvm *drv;
	} pers;
};

 
struct iwl_rate_scale_data {
	u64 data;		 
	s32 success_counter;	 
	s32 success_ratio;	 
	s32 counter;		 
	s32 average_tpt;	 
};

 
enum rs_column {
	RS_COLUMN_LEGACY_ANT_A = 0,
	RS_COLUMN_LEGACY_ANT_B,
	RS_COLUMN_SISO_ANT_A,
	RS_COLUMN_SISO_ANT_B,
	RS_COLUMN_SISO_ANT_A_SGI,
	RS_COLUMN_SISO_ANT_B_SGI,
	RS_COLUMN_MIMO2,
	RS_COLUMN_MIMO2_SGI,

	RS_COLUMN_LAST = RS_COLUMN_MIMO2_SGI,
	RS_COLUMN_COUNT = RS_COLUMN_LAST + 1,
	RS_COLUMN_INVALID,
};

enum rs_ss_force_opt {
	RS_SS_FORCE_NONE = 0,
	RS_SS_FORCE_STBC,
	RS_SS_FORCE_BFER,
	RS_SS_FORCE_SISO,
};

 
struct rs_rate_stats {
	u64 success;
	u64 total;
};

 
struct iwl_scale_tbl_info {
	struct rs_rate rate;
	enum rs_column column;
	const u16 *expected_tpt;	 
	struct iwl_rate_scale_data win[IWL_RATE_COUNT];  
	 
	struct iwl_rate_scale_data tpc_win[TPC_MAX_REDUCTION + 1];
};

enum {
	RS_STATE_SEARCH_CYCLE_STARTED,
	RS_STATE_SEARCH_CYCLE_ENDED,
	RS_STATE_STAY_IN_COLUMN,
};

 
struct iwl_lq_sta {
	u8 active_tbl;		 
	u8 rs_state;             
	u8 search_better_tbl;	 
	s32 last_tpt;

	 
	u32 table_count_limit;
	u32 max_failure_limit;	 
	u32 max_success_limit;	 
	u32 table_count;
	u32 total_failed;	 
	u32 total_success;	 
	u64 flush_timer;	 

	u32 visited_columns;     
	u64 last_tx;
	bool is_vht;
	bool ldpc;               
	bool stbc_capable;       
	bool bfer_capable;       

	enum nl80211_band band;

	 
	unsigned long active_legacy_rate;
	unsigned long active_siso_rate;
	unsigned long active_mimo2_rate;

	 
	u8 max_legacy_rate_idx;
	u8 max_siso_rate_idx;
	u8 max_mimo2_rate_idx;

	 
	struct rs_rate optimal_rate;
	unsigned long optimal_rate_mask;
	const struct rs_init_rate_info *optimal_rates;
	int optimal_nentries;

	u8 missed_rate_counter;

	struct iwl_lq_cmd lq;
	struct iwl_scale_tbl_info lq_info[LQ_SIZE];  
	u8 tx_agg_tid_en;

	 
	u32 last_rate_n_flags;

	 
	u8 is_agg;

	 
	int tpc_reduce;

	 
	struct lq_sta_pers {
#ifdef CONFIG_MAC80211_DEBUGFS
		u32 dbg_fixed_rate;
		u8 dbg_fixed_txp_reduction;

		 
		enum rs_ss_force_opt ss_force;
#endif
		u8 chains;
		s8 chain_signal[IEEE80211_MAX_CHAINS];
		s8 last_rssi;
		u16 max_agg_bufsize;
		struct rs_rate_stats tx_stats[RS_COLUMN_COUNT][IWL_RATE_COUNT];
		struct iwl_mvm *drv;
		spinlock_t lock;  
	} pers;
};

 
#define RS_DRV_DATA_TXP_MSK 0xff
#define RS_DRV_DATA_LQ_COLOR_POS 8
#define RS_DRV_DATA_LQ_COLOR_MSK (7 << RS_DRV_DATA_LQ_COLOR_POS)
#define RS_DRV_DATA_LQ_COLOR_GET(_f) (((_f) & RS_DRV_DATA_LQ_COLOR_MSK) >>\
				      RS_DRV_DATA_LQ_COLOR_POS)
#define RS_DRV_DATA_PACK(_c, _p) ((void *)(uintptr_t)\
				  (((uintptr_t)_p) |\
				   ((_c) << RS_DRV_DATA_LQ_COLOR_POS)))

 
void iwl_mvm_rs_rate_init(struct iwl_mvm *mvm,
			  struct ieee80211_vif *vif,
			  struct ieee80211_sta *sta,
			  struct ieee80211_bss_conf *link_conf,
			  struct ieee80211_link_sta *link_sta,
			  enum nl80211_band band);

 
void iwl_mvm_rs_tx_status(struct iwl_mvm *mvm, struct ieee80211_sta *sta,
			  int tid, struct ieee80211_tx_info *info, bool ndp);

 
int iwl_mvm_rate_control_register(void);

 
void iwl_mvm_rate_control_unregister(void);

struct iwl_mvm_sta;

int iwl_mvm_tx_protection(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta,
			  bool enable);

#ifdef CONFIG_IWLWIFI_DEBUGFS
void iwl_mvm_reset_frame_stats(struct iwl_mvm *mvm);
#endif

struct iwl_mvm_link_sta;

void iwl_mvm_rs_add_sta(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta);
void iwl_mvm_rs_add_sta_link(struct iwl_mvm *mvm,
			     struct iwl_mvm_link_sta *link_sta);

void iwl_mvm_rs_fw_rate_init(struct iwl_mvm *mvm,
			     struct ieee80211_vif *vif,
			     struct ieee80211_sta *sta,
			     struct ieee80211_bss_conf *link_conf,
			     struct ieee80211_link_sta *link_sta,
			     enum nl80211_band band);
int rs_fw_tx_protection(struct iwl_mvm *mvm, struct iwl_mvm_sta *mvmsta,
			bool enable);
void iwl_mvm_tlc_update_notif(struct iwl_mvm *mvm,
			      struct iwl_rx_cmd_buffer *rxb);

u16 rs_fw_get_max_amsdu_len(struct ieee80211_sta *sta,
			    struct ieee80211_bss_conf *link_conf,
			    struct ieee80211_link_sta *link_sta);
#endif  

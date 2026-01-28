#ifndef __time_event_h__
#define __time_event_h__
#include "fw-api.h"
#include "mvm.h"
#define IWL_MVM_TE_SESSION_PROTECTION_MAX_TIME_MS 600
#define IWL_MVM_TE_SESSION_PROTECTION_MIN_TIME_MS 400
void iwl_mvm_protect_session(struct iwl_mvm *mvm,
			     struct ieee80211_vif *vif,
			     u32 duration, u32 min_duration,
			     u32 max_delay, bool wait_for_notif);
void iwl_mvm_stop_session_protection(struct iwl_mvm *mvm,
				      struct ieee80211_vif *vif);
void iwl_mvm_rx_time_event_notif(struct iwl_mvm *mvm,
				 struct iwl_rx_cmd_buffer *rxb);
int iwl_mvm_start_p2p_roc(struct iwl_mvm *mvm, struct ieee80211_vif *vif,
			  int duration, enum ieee80211_roc_type type);
void iwl_mvm_stop_roc(struct iwl_mvm *mvm, struct ieee80211_vif *vif);
void iwl_mvm_remove_time_event(struct iwl_mvm *mvm,
			       struct iwl_mvm_vif *mvmvif,
			       struct iwl_mvm_time_event_data *te_data);
void iwl_mvm_te_clear_data(struct iwl_mvm *mvm,
			   struct iwl_mvm_time_event_data *te_data);
void iwl_mvm_cleanup_roc_te(struct iwl_mvm *mvm);
void iwl_mvm_roc_done_wk(struct work_struct *wk);
void iwl_mvm_remove_csa_period(struct iwl_mvm *mvm,
			       struct ieee80211_vif *vif);
int iwl_mvm_schedule_csa_period(struct iwl_mvm *mvm,
				struct ieee80211_vif *vif,
				u32 duration, u32 apply_time);
static inline bool
iwl_mvm_te_scheduled(struct iwl_mvm_time_event_data *te_data)
{
	if (!te_data)
		return false;
	return !!te_data->uid;
}
void iwl_mvm_schedule_session_protection(struct iwl_mvm *mvm,
					 struct ieee80211_vif *vif,
					 u32 duration, u32 min_duration,
					 bool wait_for_notif);
void iwl_mvm_rx_session_protect_notif(struct iwl_mvm *mvm,
				      struct iwl_rx_cmd_buffer *rxb);
#endif  

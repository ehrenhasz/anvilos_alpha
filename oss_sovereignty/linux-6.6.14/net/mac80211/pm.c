
 
#include <net/mac80211.h>
#include <net/rtnetlink.h>

#include "ieee80211_i.h"
#include "mesh.h"
#include "driver-ops.h"
#include "led.h"

static void ieee80211_sched_scan_cancel(struct ieee80211_local *local)
{
	if (ieee80211_request_sched_scan_stop(local))
		return;
	cfg80211_sched_scan_stopped_locked(local->hw.wiphy, 0);
}

int __ieee80211_suspend(struct ieee80211_hw *hw, struct cfg80211_wowlan *wowlan)
{
	struct ieee80211_local *local = hw_to_local(hw);
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;

	if (!local->open_count)
		goto suspend;

	local->suspending = true;
	mb();  

	ieee80211_scan_cancel(local);

	ieee80211_dfs_cac_cancel(local);

	ieee80211_roc_purge(local, NULL);

	ieee80211_del_virtual_monitor(local);

	if (ieee80211_hw_check(hw, AMPDU_AGGREGATION) &&
	    !(wowlan && wowlan->any)) {
		mutex_lock(&local->sta_mtx);
		list_for_each_entry(sta, &local->sta_list, list) {
			set_sta_flag(sta, WLAN_STA_BLOCK_BA);
			ieee80211_sta_tear_down_BA_sessions(
					sta, AGG_STOP_LOCAL_REQUEST);
		}
		mutex_unlock(&local->sta_mtx);
	}

	 
	if (!(wowlan && wowlan->any))
		ieee80211_sched_scan_cancel(local);

	ieee80211_stop_queues_by_reason(hw,
					IEEE80211_MAX_QUEUE_MAP,
					IEEE80211_QUEUE_STOP_REASON_SUSPEND,
					false);

	 
	synchronize_net();

	ieee80211_flush_queues(local, NULL, true);

	local->quiescing = true;
	 
	mb();

	flush_workqueue(local->workqueue);

	 
	del_timer_sync(&local->sta_cleanup);

	  
	cancel_work_sync(&local->dynamic_ps_enable_work);
	del_timer_sync(&local->dynamic_ps_timer);

	local->wowlan = wowlan;
	if (local->wowlan) {
		int err;

		 
		list_for_each_entry(sdata, &local->interfaces, list) {
			if (!ieee80211_sdata_running(sdata))
				continue;
			if (sdata->vif.type != NL80211_IFTYPE_STATION)
				continue;
			ieee80211_mgd_quiesce(sdata);
			 
			if (sdata->u.mgd.associated &&
			    sdata->u.mgd.powersave &&
			     !(local->hw.conf.flags & IEEE80211_CONF_PS)) {
				local->hw.conf.flags |= IEEE80211_CONF_PS;
				ieee80211_hw_config(local,
						    IEEE80211_CONF_CHANGE_PS);
			}
		}

		err = drv_suspend(local, wowlan);
		if (err < 0) {
			local->quiescing = false;
			local->wowlan = false;
			if (ieee80211_hw_check(hw, AMPDU_AGGREGATION)) {
				mutex_lock(&local->sta_mtx);
				list_for_each_entry(sta,
						    &local->sta_list, list) {
					clear_sta_flag(sta, WLAN_STA_BLOCK_BA);
				}
				mutex_unlock(&local->sta_mtx);
			}
			ieee80211_wake_queues_by_reason(hw,
					IEEE80211_MAX_QUEUE_MAP,
					IEEE80211_QUEUE_STOP_REASON_SUSPEND,
					false);
			return err;
		} else if (err > 0) {
			WARN_ON(err != 1);
			 
			ieee80211_wake_queues_by_reason(hw,
					IEEE80211_MAX_QUEUE_MAP,
					IEEE80211_QUEUE_STOP_REASON_SUSPEND,
					false);
			return err;
		} else {
			goto suspend;
		}
	}

	 
	list_for_each_entry(sdata, &local->interfaces, list) {
		if (!ieee80211_sdata_running(sdata))
			continue;
		switch (sdata->vif.type) {
		case NL80211_IFTYPE_AP_VLAN:
		case NL80211_IFTYPE_MONITOR:
			continue;
		case NL80211_IFTYPE_STATION:
			ieee80211_mgd_quiesce(sdata);
			break;
		default:
			break;
		}

		flush_delayed_work(&sdata->dec_tailroom_needed_wk);
		drv_remove_interface(local, sdata);
	}

	 
	WARN_ON(!list_empty(&local->chanctx_list));

	 
	ieee80211_stop_device(local);

 suspend:
	local->suspended = true;
	 
	barrier();
	local->quiescing = false;
	local->suspending = false;

	return 0;
}

 

void ieee80211_report_wowlan_wakeup(struct ieee80211_vif *vif,
				    struct cfg80211_wowlan_wakeup *wakeup,
				    gfp_t gfp)
{
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);

	cfg80211_report_wowlan_wakeup(&sdata->wdev, wakeup, gfp);
}
EXPORT_SYMBOL(ieee80211_report_wowlan_wakeup);

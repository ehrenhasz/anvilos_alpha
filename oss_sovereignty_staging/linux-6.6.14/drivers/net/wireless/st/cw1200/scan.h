 
 

#ifndef SCAN_H_INCLUDED
#define SCAN_H_INCLUDED

#include <linux/semaphore.h>
#include "wsm.h"

  struct sk_buff;
  struct cfg80211_scan_request;
  struct ieee80211_channel;
  struct ieee80211_hw;
  struct work_struct;

struct cw1200_scan {
	struct semaphore lock;
	struct work_struct work;
	struct delayed_work timeout;
	struct cfg80211_scan_request *req;
	struct ieee80211_channel **begin;
	struct ieee80211_channel **curr;
	struct ieee80211_channel **end;
	struct wsm_ssid ssids[WSM_SCAN_MAX_NUM_OF_SSIDS];
	int output_power;
	int n_ssids;
	int status;
	atomic_t in_progress;
	 
	struct delayed_work probe_work;
	int direct_probe;
};

int cw1200_hw_scan(struct ieee80211_hw *hw,
		   struct ieee80211_vif *vif,
		   struct ieee80211_scan_request *hw_req);
void cw1200_scan_work(struct work_struct *work);
void cw1200_scan_timeout(struct work_struct *work);
void cw1200_clear_recent_scan_work(struct work_struct *work);
void cw1200_scan_complete_cb(struct cw1200_common *priv,
			     struct wsm_scan_complete *arg);
void cw1200_scan_failed_cb(struct cw1200_common *priv);

 
 
void cw1200_probe_work(struct work_struct *work);

#endif

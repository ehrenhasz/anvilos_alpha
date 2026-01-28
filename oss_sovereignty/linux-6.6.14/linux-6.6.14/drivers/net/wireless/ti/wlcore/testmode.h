#ifndef __TESTMODE_H__
#define __TESTMODE_H__
#include <net/mac80211.h>
int wl1271_tm_cmd(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		  void *data, int len);
#endif  

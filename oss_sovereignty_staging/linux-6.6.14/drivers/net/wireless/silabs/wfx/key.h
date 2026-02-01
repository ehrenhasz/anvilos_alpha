 
 
#ifndef WFX_KEY_H
#define WFX_KEY_H

#include <net/mac80211.h>

struct wfx_dev;
struct wfx_vif;

int wfx_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd, struct ieee80211_vif *vif,
		struct ieee80211_sta *sta, struct ieee80211_key_conf *key);

#endif

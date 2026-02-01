#ifndef __NET_WIRELESS_REG_H
#define __NET_WIRELESS_REG_H

#include <net/cfg80211.h>

 

enum ieee80211_regd_source {
	REGD_SOURCE_INTERNAL_DB,
	REGD_SOURCE_CRDA,
	REGD_SOURCE_CACHED,
};

extern const struct ieee80211_regdomain __rcu *cfg80211_regdomain;

bool reg_is_valid_request(const char *alpha2);
bool is_world_regdom(const char *alpha2);
bool reg_supported_dfs_region(enum nl80211_dfs_regions dfs_region);
enum nl80211_dfs_regions reg_get_dfs_region(struct wiphy *wiphy);

int regulatory_hint_user(const char *alpha2,
			 enum nl80211_user_reg_hint_type user_reg_hint_type);

 
int regulatory_hint_indoor(bool is_indoor, u32 portid);

 
void regulatory_netlink_notify(u32 portid);

void wiphy_regulatory_register(struct wiphy *wiphy);
void wiphy_regulatory_deregister(struct wiphy *wiphy);

int __init regulatory_init(void);
void regulatory_exit(void);

int set_regdom(const struct ieee80211_regdomain *rd,
	       enum ieee80211_regd_source regd_src);

unsigned int reg_get_max_bandwidth(const struct ieee80211_regdomain *rd,
				   const struct ieee80211_reg_rule *rule);

bool reg_last_request_cell_base(void);

 
int regulatory_hint_found_beacon(struct wiphy *wiphy,
				 struct ieee80211_channel *beacon_chan,
				 gfp_t gfp);

 
void regulatory_hint_country_ie(struct wiphy *wiphy,
			 enum nl80211_band band,
			 const u8 *country_ie,
			 u8 country_ie_len);

 
void regulatory_hint_disconnect(void);

 
int cfg80211_get_unii(int freq);

 
bool regulatory_indoor_allowed(void);

 
#define REG_PRE_CAC_EXPIRY_GRACE_MS 2000

 
void regulatory_propagate_dfs_state(struct wiphy *wiphy,
				    struct cfg80211_chan_def *chandef,
				    enum nl80211_dfs_state dfs_state,
				    enum nl80211_radar_event event);

 
bool reg_dfs_domain_same(struct wiphy *wiphy1, struct wiphy *wiphy2);

 
int reg_reload_regdb(void);

extern const u8 shipped_regdb_certs[];
extern unsigned int shipped_regdb_certs_len;
extern const u8 extra_regdb_certs[];
extern unsigned int extra_regdb_certs_len;

#endif   

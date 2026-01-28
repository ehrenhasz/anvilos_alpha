#ifndef __il_spectrum_h__
#define __il_spectrum_h__
enum {				 
	IEEE80211_BASIC_MAP_BSS = (1 << 0),
	IEEE80211_BASIC_MAP_OFDM = (1 << 1),
	IEEE80211_BASIC_MAP_UNIDENTIFIED = (1 << 2),
	IEEE80211_BASIC_MAP_RADAR = (1 << 3),
	IEEE80211_BASIC_MAP_UNMEASURED = (1 << 4),
};
struct ieee80211_basic_report {
	u8 channel;
	__le64 start_time;
	__le16 duration;
	u8 map;
} __packed;
enum {				 
	IEEE80211_MEASUREMENT_ENABLE = (1 << 1),
	IEEE80211_MEASUREMENT_REQUEST = (1 << 2),
	IEEE80211_MEASUREMENT_REPORT = (1 << 3),
};
enum {
	IEEE80211_REPORT_BASIC = 0,	 
	IEEE80211_REPORT_CCA = 1,	 
	IEEE80211_REPORT_RPI = 2,	 
};
struct ieee80211_measurement_params {
	u8 channel;
	__le64 start_time;
	__le16 duration;
} __packed;
struct ieee80211_info_element {
	u8 id;
	u8 len;
	u8 data[];
} __packed;
struct ieee80211_measurement_request {
	struct ieee80211_info_element ie;
	u8 token;
	u8 mode;
	u8 type;
	struct ieee80211_measurement_params params[];
} __packed;
struct ieee80211_measurement_report {
	struct ieee80211_info_element ie;
	u8 token;
	u8 mode;
	u8 type;
	union {
		struct ieee80211_basic_report basic[0];
	} u;
} __packed;
#endif

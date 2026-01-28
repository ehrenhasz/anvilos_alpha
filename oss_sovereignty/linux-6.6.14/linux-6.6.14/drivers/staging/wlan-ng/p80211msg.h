#ifndef _P80211MSG_H
#define _P80211MSG_H
#define WLAN_DEVNAMELEN_MAX	16
struct p80211msg {
	u32 msgcode;
	u32 msglen;
	u8 devname[WLAN_DEVNAMELEN_MAX];
} __packed;
#endif  

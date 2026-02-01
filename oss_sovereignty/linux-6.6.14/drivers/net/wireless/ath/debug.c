 

#include <linux/export.h>
#include "ath.h"

const char *ath_opmode_to_string(enum nl80211_iftype opmode)
{
	switch (opmode) {
	case NL80211_IFTYPE_UNSPECIFIED:
		return "UNSPEC";
	case NL80211_IFTYPE_ADHOC:
		return "ADHOC";
	case NL80211_IFTYPE_STATION:
		return "STATION";
	case NL80211_IFTYPE_AP:
		return "AP";
	case NL80211_IFTYPE_AP_VLAN:
		return "AP-VLAN";
	case NL80211_IFTYPE_WDS:
		return "WDS";
	case NL80211_IFTYPE_MONITOR:
		return "MONITOR";
	case NL80211_IFTYPE_MESH_POINT:
		return "MESH";
	case NL80211_IFTYPE_P2P_CLIENT:
		return "P2P-CLIENT";
	case NL80211_IFTYPE_P2P_GO:
		return "P2P-GO";
	case NL80211_IFTYPE_OCB:
		return "OCB";
	default:
		return "UNKNOWN";
	}
}
EXPORT_SYMBOL(ath_opmode_to_string);

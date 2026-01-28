#ifndef __WLAN_BSSDEF_H__
#define __WLAN_BSSDEF_H__
#define MAX_IE_SZ	768
#define NDIS_802_11_LENGTH_SSID         32
#define NDIS_802_11_LENGTH_RATES        8
#define NDIS_802_11_LENGTH_RATES_EX     16
typedef unsigned char   NDIS_802_11_MAC_ADDRESS[6];
typedef unsigned char   NDIS_802_11_RATES[NDIS_802_11_LENGTH_RATES];         
typedef unsigned char   NDIS_802_11_RATES_EX[NDIS_802_11_LENGTH_RATES_EX];   
struct ndis_802_11_ssid {
	u32  ssid_length;
	u8  ssid[32];
};
enum ndis_802_11_network_type {
	Ndis802_11FH,
	Ndis802_11DS,
	Ndis802_11OFDM5,
	Ndis802_11OFDM24,
	Ndis802_11NetworkTypeMax     
};
struct ndis_802_11_conf {
	u32 length;              
	u32 beacon_period;        
	u32 atim_window;          
	u32 ds_config;            
};
enum ndis_802_11_network_infrastructure {
	Ndis802_11IBSS,
	Ndis802_11Infrastructure,
	Ndis802_11AutoUnknown,
	Ndis802_11InfrastructureMax,      
	Ndis802_11APMode,
};
struct ndis_802_11_fix_ie {
	u8  time_stamp[8];
	u16  beacon_interval;
	u16  capabilities;
};
struct ndis_80211_var_ie {
	u8  element_id;
	u8  length;
	u8  data[];
};
enum ndis_802_11_authentication_mode {
	Ndis802_11AuthModeOpen,
	Ndis802_11AuthModeShared,
	Ndis802_11AuthModeAutoSwitch,
	Ndis802_11AuthModeWPA,
	Ndis802_11AuthModeWPAPSK,
	Ndis802_11AuthModeWPANone,
	Ndis802_11AuthModeWAPI,
	Ndis802_11AuthModeMax    
};
enum {
	Ndis802_11WEPEnabled,
	Ndis802_11Encryption1Enabled = Ndis802_11WEPEnabled,
	Ndis802_11WEPDisabled,
	Ndis802_11EncryptionDisabled = Ndis802_11WEPDisabled,
	Ndis802_11WEPKeyAbsent,
	Ndis802_11Encryption1KeyAbsent = Ndis802_11WEPKeyAbsent,
	Ndis802_11WEPNotSupported,
	Ndis802_11EncryptionNotSupported = Ndis802_11WEPNotSupported,
	Ndis802_11Encryption2Enabled,
	Ndis802_11Encryption2KeyAbsent,
	Ndis802_11Encryption3Enabled,
	Ndis802_11Encryption3KeyAbsent,
	Ndis802_11_EncrypteionWAPI
};
#define NDIS_802_11_AI_REQFI_CAPABILITIES      1
#define NDIS_802_11_AI_REQFI_LISTENINTERVAL    2
#define NDIS_802_11_AI_REQFI_CURRENTAPADDRESS  4
#define NDIS_802_11_AI_RESFI_CAPABILITIES      1
#define NDIS_802_11_AI_RESFI_STATUSCODE        2
#define NDIS_802_11_AI_RESFI_ASSOCIATIONID     4
struct ndis_802_11_wep {
	u32 length;         
	u32 key_index;       
	u32 key_length;      
	u8 key_material[16]; 
};
#define NDIS_802_11_AUTH_REQUEST_AUTH_FIELDS        0x0f
#define NDIS_802_11_AUTH_REQUEST_REAUTH			0x01
#define NDIS_802_11_AUTH_REQUEST_KEYUPDATE		0x02
#define NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR		0x06
#define NDIS_802_11_AUTH_REQUEST_GROUP_ERROR		0x0E
#define MIC_CHECK_TIME	60000000
#ifndef Ndis802_11APMode
#define Ndis802_11APMode (Ndis802_11InfrastructureMax + 1)
#endif
struct wlan_phy_info {
	u8 signal_strength; 
	u8 signal_quality; 
	u8 optimum_antenna;   
	u8 reserved_0;
};
struct wlan_bcn_info {
	u8 encryp_protocol; 
	int group_cipher;  
	int pairwise_cipher; 
	int is_8021x;
	unsigned short	ht_cap_info;
	unsigned char ht_info_infos_0;
};
struct wlan_bssid_ex {
	u32  length;
	NDIS_802_11_MAC_ADDRESS  mac_address;
	u8  reserved[2]; 
	struct ndis_802_11_ssid  ssid;
	u32  privacy;
	long  rssi; 
	enum ndis_802_11_network_type  network_type_in_use;
	struct ndis_802_11_conf  configuration;
	enum ndis_802_11_network_infrastructure  infrastructure_mode;
	NDIS_802_11_RATES_EX  supported_rates;
	struct wlan_phy_info phy_info;
	u32  ie_length;
	u8  ies[MAX_IE_SZ];	 
} __packed;
static inline uint get_wlan_bssid_ex_sz(struct wlan_bssid_ex *bss)
{
	return (sizeof(struct wlan_bssid_ex) - MAX_IE_SZ + bss->ie_length);
}
struct	wlan_network {
	struct list_head	list;
	int	network_type;	 
	int	fixed;			 
	unsigned long	last_scanned;  
	int	aid;			 
	int	join_res;
	struct wlan_bssid_ex	network;  
	struct wlan_bcn_info	bcn_info;
};
enum {
	DISABLE_VCS,
	ENABLE_VCS,
	AUTO_VCS
};
enum {
	NONE_VCS,
	RTS_CTS,
	CTS_TO_SELF
};
#define PWR_CAM 0
#define PWR_MINPS 1
#define PWR_MAXPS 2
#define PWR_UAPSD 3
#define PWR_VOIP 4
enum {
	NO_LIMIT,
	TWO_MSDU,
	FOUR_MSDU,
	SIX_MSDU
};
#define NUM_PRE_AUTH_KEY 16
#define NUM_PMKID_CACHE NUM_PRE_AUTH_KEY
#endif  

 
 

#ifndef _LINUX_P80211CONV_H
#define _LINUX_P80211CONV_H

#define WLAN_IEEE_OUI_LEN	3

#define WLAN_ETHCONV_ENCAP	1
#define WLAN_ETHCONV_8021h	3

#define P80211CAPTURE_VERSION	0x80211001

#define	P80211_FRMMETA_MAGIC	0x802110

struct p80211_rxmeta {
	struct wlandevice *wlandev;

	u64 mactime;		 
	u64 hosttime;		 

	unsigned int rxrate;	 
	unsigned int priority;	 
	int signal;		 
	int noise;		 
	unsigned int channel;	 
	unsigned int preamble;	 
	unsigned int encoding;	 

};

struct p80211_frmmeta {
	unsigned int magic;
	struct p80211_rxmeta *rx;
};

void p80211skb_free(struct wlandevice *wlandev, struct sk_buff *skb);
int p80211skb_rxmeta_attach(struct wlandevice *wlandev, struct sk_buff *skb);
void p80211skb_rxmeta_detach(struct sk_buff *skb);

static inline struct p80211_frmmeta *p80211skb_frmmeta(struct sk_buff *skb)
{
	struct p80211_frmmeta *frmmeta = (struct p80211_frmmeta *)skb->cb;

	return frmmeta->magic == P80211_FRMMETA_MAGIC ? frmmeta : NULL;
}

static inline struct p80211_rxmeta *p80211skb_rxmeta(struct sk_buff *skb)
{
	struct p80211_frmmeta *frmmeta = p80211skb_frmmeta(skb);

	return frmmeta ? frmmeta->rx : NULL;
}

 
struct p80211_caphdr {
	__be32 version;
	__be32 length;
	__be64 mactime;
	__be64 hosttime;
	__be32 phytype;
	__be32 channel;
	__be32 datarate;
	__be32 antenna;
	__be32 priority;
	__be32 ssi_type;
	__be32 ssi_signal;
	__be32 ssi_noise;
	__be32 preamble;
	__be32 encoding;
};

struct p80211_metawep {
	void *data;
	u8 iv[4];
	u8 icv[4];
};

 
struct wlan_ethhdr {
	u8 daddr[ETH_ALEN];
	u8 saddr[ETH_ALEN];
	__be16 type;
} __packed;

 
struct wlan_llc {
	u8 dsap;
	u8 ssap;
	u8 ctl;
} __packed;

 
struct wlan_snap {
	u8 oui[WLAN_IEEE_OUI_LEN];
	__be16 type;
} __packed;

 
struct wlandevice;

int skb_p80211_to_ether(struct wlandevice *wlandev, u32 ethconv,
			struct sk_buff *skb);
int skb_ether_to_p80211(struct wlandevice *wlandev, u32 ethconv,
			struct sk_buff *skb, struct p80211_hdr *p80211_hdr,
			struct p80211_metawep *p80211_wep);

int p80211_stt_findproto(u16 proto);

#endif

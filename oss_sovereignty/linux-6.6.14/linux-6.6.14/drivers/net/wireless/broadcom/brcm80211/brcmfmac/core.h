#ifndef BRCMFMAC_CORE_H
#define BRCMFMAC_CORE_H
#include <net/cfg80211.h>
#include "fweh.h"
#if IS_MODULE(CONFIG_BRCMFMAC)
#define BRCMF_EXPORT_SYMBOL_GPL(__sym)	EXPORT_SYMBOL_NS_GPL(__sym, BRCMFMAC)
#else
#define BRCMF_EXPORT_SYMBOL_GPL(__sym)
#endif
#define TOE_TX_CSUM_OL		0x00000001
#define TOE_RX_CSUM_OL		0x00000002
#define BRCMF_MAX_IFS	16
#define BRCMF_DCMD_SMLEN	256
#define BRCMF_DCMD_MEDLEN	1536
#define BRCMF_DCMD_MAXLEN	8192
#define BRCMF_TX_IOCTL_MAX_MSG_SIZE	(ETH_FRAME_LEN+ETH_FCS_LEN)
#define BRCMF_AMPDU_RX_REORDER_MAXFLOWS		256
#define BRCMF_DRIVER_FIRMWARE_VERSION_LEN	32
#define NDOL_MAX_ENTRIES	8
struct brcmf_ampdu_rx_reorder {
	struct sk_buff **pktslots;
	u8 flow_id;
	u8 cur_idx;
	u8 exp_idx;
	u8 max_idx;
	u8 pend_pkts;
};
struct brcmf_proto;	 
struct brcmf_fws_info;	 
struct brcmf_mp_device;	 
struct brcmf_rev_info {
	int result;
	u32 vendorid;
	u32 deviceid;
	u32 radiorev;
	u32 corerev;
	u32 boardid;
	u32 boardvendor;
	u32 boardrev;
	u32 driverrev;
	u32 ucoderev;
	u32 bus;
	char chipname[12];
	u32 phytype;
	u32 phyrev;
	u32 anarev;
	u32 chippkg;
	u32 nvramrev;
};
struct brcmf_pub {
	struct brcmf_bus *bus_if;
	struct brcmf_proto *proto;
	struct wiphy *wiphy;
	struct cfg80211_ops *ops;
	struct brcmf_cfg80211_info *config;
	uint hdrlen;		 
	char fwver[BRCMF_DRIVER_FIRMWARE_VERSION_LEN];
	u8 mac[ETH_ALEN];		 
	struct mac_address addresses[BRCMF_MAX_IFS];
	struct brcmf_if *iflist[BRCMF_MAX_IFS];
	s32 if2bss[BRCMF_MAX_IFS];
	struct brcmf_if *mon_if;
	struct mutex proto_block;
	unsigned char proto_buf[BRCMF_DCMD_MAXLEN];
	struct brcmf_fweh_info fweh;
	struct brcmf_ampdu_rx_reorder
		*reorder_flows[BRCMF_AMPDU_RX_REORDER_MAXFLOWS];
	u32 feat_flags;
	u32 chip_quirks;
	struct brcmf_rev_info revinfo;
#ifdef DEBUG
	struct dentry *dbgfs_dir;
#endif
	struct notifier_block inetaddr_notifier;
	struct notifier_block inet6addr_notifier;
	struct brcmf_mp_device *settings;
	struct work_struct bus_reset;
	u8 clmver[BRCMF_DCMD_SMLEN];
	u8 sta_mac_idx;
	const struct brcmf_fwvid_ops *vops;
	void *vdata;
};
struct brcmf_cfg80211_vif;
struct brcmf_fws_mac_descriptor;
enum brcmf_netif_stop_reason {
	BRCMF_NETIF_STOP_REASON_FWS_FC = BIT(0),
	BRCMF_NETIF_STOP_REASON_FLOW = BIT(1),
	BRCMF_NETIF_STOP_REASON_DISCONNECTED = BIT(2)
};
struct brcmf_if {
	struct brcmf_pub *drvr;
	struct brcmf_cfg80211_vif *vif;
	struct net_device *ndev;
	struct work_struct multicast_work;
	struct work_struct ndoffload_work;
	struct brcmf_fws_mac_descriptor *fws_desc;
	int ifidx;
	s32 bsscfgidx;
	u8 mac_addr[ETH_ALEN];
	u8 netif_stop;
	spinlock_t netif_stop_lock;
	atomic_t pend_8021x_cnt;
	wait_queue_head_t pend_8021x_wait;
	struct in6_addr ipv6_addr_tbl[NDOL_MAX_ENTRIES];
	u8 ipv6addr_idx;
	bool fwil_fwerr;
};
int brcmf_netdev_wait_pend8021x(struct brcmf_if *ifp);
char *brcmf_ifname(struct brcmf_if *ifp);
struct brcmf_if *brcmf_get_ifp(struct brcmf_pub *drvr, int ifidx);
void brcmf_configure_arp_nd_offload(struct brcmf_if *ifp, bool enable);
int brcmf_net_attach(struct brcmf_if *ifp, bool locked);
struct brcmf_if *brcmf_add_if(struct brcmf_pub *drvr, s32 bsscfgidx, s32 ifidx,
			      bool is_p2pdev, const char *name, u8 *mac_addr);
void brcmf_remove_interface(struct brcmf_if *ifp, bool locked);
void brcmf_txflowblock_if(struct brcmf_if *ifp,
			  enum brcmf_netif_stop_reason reason, bool state);
void brcmf_txfinalize(struct brcmf_if *ifp, struct sk_buff *txp, bool success);
void brcmf_netif_rx(struct brcmf_if *ifp, struct sk_buff *skb);
void brcmf_netif_mon_rx(struct brcmf_if *ifp, struct sk_buff *skb);
void brcmf_net_detach(struct net_device *ndev, bool locked);
int brcmf_net_mon_attach(struct brcmf_if *ifp);
void brcmf_net_setcarrier(struct brcmf_if *ifp, bool on);
int __init brcmf_core_init(void);
void __exit brcmf_core_exit(void);
#endif  

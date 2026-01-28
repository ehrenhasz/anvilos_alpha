#ifndef _BRCMF_FEATURE_H
#define _BRCMF_FEATURE_H
#define BRCMF_FEAT_LIST \
	BRCMF_FEAT_DEF(MBSS) \
	BRCMF_FEAT_DEF(MCHAN) \
	BRCMF_FEAT_DEF(PNO) \
	BRCMF_FEAT_DEF(WOWL) \
	BRCMF_FEAT_DEF(P2P) \
	BRCMF_FEAT_DEF(RSDB) \
	BRCMF_FEAT_DEF(TDLS) \
	BRCMF_FEAT_DEF(SCAN_RANDOM_MAC) \
	BRCMF_FEAT_DEF(WOWL_ND) \
	BRCMF_FEAT_DEF(WOWL_GTK) \
	BRCMF_FEAT_DEF(WOWL_ARP_ND) \
	BRCMF_FEAT_DEF(MFP) \
	BRCMF_FEAT_DEF(GSCAN) \
	BRCMF_FEAT_DEF(FWSUP) \
	BRCMF_FEAT_DEF(MONITOR) \
	BRCMF_FEAT_DEF(MONITOR_FLAG) \
	BRCMF_FEAT_DEF(MONITOR_FMT_RADIOTAP) \
	BRCMF_FEAT_DEF(MONITOR_FMT_HW_RX_HDR) \
	BRCMF_FEAT_DEF(DOT11H) \
	BRCMF_FEAT_DEF(SAE) \
	BRCMF_FEAT_DEF(FWAUTH) \
	BRCMF_FEAT_DEF(DUMP_OBSS) \
	BRCMF_FEAT_DEF(SCAN_V2) \
	BRCMF_FEAT_DEF(PMKID_V2) \
	BRCMF_FEAT_DEF(PMKID_V3)
#define BRCMF_QUIRK_LIST \
	BRCMF_QUIRK_DEF(AUTO_AUTH) \
	BRCMF_QUIRK_DEF(NEED_MPC)
#define BRCMF_FEAT_DEF(_f) \
	BRCMF_FEAT_ ## _f,
enum brcmf_feat_id {
	BRCMF_FEAT_LIST
	BRCMF_FEAT_LAST
};
#undef BRCMF_FEAT_DEF
#define BRCMF_QUIRK_DEF(_q) \
	BRCMF_FEAT_QUIRK_ ## _q,
enum brcmf_feat_quirk {
	BRCMF_QUIRK_LIST
	BRCMF_FEAT_QUIRK_LAST
};
#undef BRCMF_QUIRK_DEF
void brcmf_feat_attach(struct brcmf_pub *drvr);
void brcmf_feat_debugfs_create(struct brcmf_pub *drvr);
bool brcmf_feat_is_enabled(struct brcmf_if *ifp, enum brcmf_feat_id id);
bool brcmf_feat_is_quirk_enabled(struct brcmf_if *ifp,
				 enum brcmf_feat_quirk quirk);
#endif  

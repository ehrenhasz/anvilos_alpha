#ifndef _BRCMF_PNO_H
#define _BRCMF_PNO_H
#define BRCMF_PNO_SCAN_COMPLETE			1
#define BRCMF_PNO_MAX_PFN_COUNT			16
#define BRCMF_PNO_SCHED_SCAN_MIN_PERIOD	10
#define BRCMF_PNO_SCHED_SCAN_MAX_PERIOD	508
struct brcmf_pno_info;
int brcmf_pno_start_sched_scan(struct brcmf_if *ifp,
			       struct cfg80211_sched_scan_request *req);
int brcmf_pno_stop_sched_scan(struct brcmf_if *ifp, u64 reqid);
void brcmf_pno_wiphy_params(struct wiphy *wiphy, bool gscan);
int brcmf_pno_attach(struct brcmf_cfg80211_info *cfg);
void brcmf_pno_detach(struct brcmf_cfg80211_info *cfg);
u64 brcmf_pno_find_reqid_by_bucket(struct brcmf_pno_info *pi, u32 bucket);
u32 brcmf_pno_get_bucket_map(struct brcmf_pno_info *pi,
			     struct brcmf_pno_net_info_le *netinfo);
#endif  

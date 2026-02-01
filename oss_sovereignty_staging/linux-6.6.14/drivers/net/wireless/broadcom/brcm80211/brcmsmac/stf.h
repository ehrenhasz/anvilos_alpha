 

#ifndef _BRCM_STF_H_
#define _BRCM_STF_H_

#include "types.h"

int brcms_c_stf_attach(struct brcms_c_info *wlc);
void brcms_c_stf_detach(struct brcms_c_info *wlc);

void brcms_c_tempsense_upd(struct brcms_c_info *wlc);
void brcms_c_stf_ss_algo_channel_get(struct brcms_c_info *wlc,
				     u16 *ss_algo_channel, u16 chanspec);
void brcms_c_stf_ss_update(struct brcms_c_info *wlc, struct brcms_band *band);
void brcms_c_stf_phy_txant_upd(struct brcms_c_info *wlc);
int brcms_c_stf_txchain_set(struct brcms_c_info *wlc, s32 int_val, bool force);
bool brcms_c_stf_stbc_rx_set(struct brcms_c_info *wlc, s32 int_val);
void brcms_c_stf_phy_chain_calc(struct brcms_c_info *wlc);
u16 brcms_c_stf_phytxchain_sel(struct brcms_c_info *wlc, u32 rspec);
u16 brcms_c_stf_d11hdrs_phyctl_txant(struct brcms_c_info *wlc, u32 rspec);

#endif				 

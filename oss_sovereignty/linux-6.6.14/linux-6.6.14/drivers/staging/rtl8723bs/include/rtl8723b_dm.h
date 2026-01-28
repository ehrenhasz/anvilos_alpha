#ifndef __RTL8723B_DM_H__
#define __RTL8723B_DM_H__
void rtl8723b_init_dm_priv(struct adapter *padapter);
void rtl8723b_InitHalDm(struct adapter *padapter);
void rtl8723b_HalDmWatchDog(struct adapter *padapter);
void rtl8723b_HalDmWatchDog_in_LPS(struct adapter *padapter);
void rtl8723b_hal_dm_in_lps(struct adapter *padapter);
#endif

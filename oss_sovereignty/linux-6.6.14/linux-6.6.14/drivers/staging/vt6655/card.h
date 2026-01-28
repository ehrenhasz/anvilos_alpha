#ifndef __CARD_H__
#define __CARD_H__
#include <linux/types.h>
#include <linux/nl80211.h>
#define CARD_LB_NONE            MAKEWORD(MAC_LB_NONE, 0)
#define CARD_LB_MAC             MAKEWORD(MAC_LB_INTERNAL, 0)
#define CARD_LB_PHY             MAKEWORD(MAC_LB_EXT, 0)
#define DEFAULT_MSDU_LIFETIME           512   
#define DEFAULT_MSDU_LIFETIME_RES_64us  8000  
#define DEFAULT_MGN_LIFETIME            8     
#define DEFAULT_MGN_LIFETIME_RES_64us   125   
#define CB_MAX_CHANNEL_24G      14
#define CB_MAX_CHANNEL_5G       42
#define CB_MAX_CHANNEL          (CB_MAX_CHANNEL_24G + CB_MAX_CHANNEL_5G)
struct vnt_private;
void CARDvSetRSPINF(struct vnt_private *priv, u8 bb_type);
void CARDvUpdateBasicTopRate(struct vnt_private *priv);
bool CARDbIsOFDMinBasicRate(struct vnt_private *priv);
void CARDvSetFirstNextTBTT(struct vnt_private *priv,
			   unsigned short wBeaconInterval);
void CARDvUpdateNextTBTT(struct vnt_private *priv, u64 qwTSF,
			 unsigned short wBeaconInterval);
u64 vt6655_get_current_tsf(struct vnt_private *priv);
u64 CARDqGetNextTBTT(u64 qwTSF, unsigned short wBeaconInterval);
u64 CARDqGetTSFOffset(unsigned char byRxRate, u64 qwTSF1, u64 qwTSF2);
unsigned char CARDbyGetPktType(struct vnt_private *priv);
void CARDvSafeResetTx(struct vnt_private *priv);
void CARDvSafeResetRx(struct vnt_private *priv);
void CARDbRadioPowerOff(struct vnt_private *priv);
bool CARDbSetPhyParameter(struct vnt_private *priv, u8 bb_type);
bool CARDbUpdateTSF(struct vnt_private *priv, unsigned char byRxRate,
		    u64 qwBSSTimestamp);
bool CARDbSetBeaconPeriod(struct vnt_private *priv,
			  unsigned short wBeaconInterval);
#endif  

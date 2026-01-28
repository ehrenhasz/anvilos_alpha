#ifndef _CHANNEL_H_
#define _CHANNEL_H_
#include "card.h"
void vnt_init_bands(struct vnt_private *priv);
bool set_channel(struct vnt_private *priv, struct ieee80211_channel *ch);
#endif  

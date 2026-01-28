#ifndef __POWER_H__
#define __POWER_H__
#include "device.h"
#define C_PWBT                   1000     
#define PS_FAST_INTERVAL         1        
#define PS_MAX_INTERVAL          4        
void PSvDisablePowerSaving(struct vnt_private *priv);
void PSvEnablePowerSaving(struct vnt_private *priv, unsigned short wListenInterval);
bool PSbIsNextTBTTWakeUp(struct vnt_private *priv);
#endif  

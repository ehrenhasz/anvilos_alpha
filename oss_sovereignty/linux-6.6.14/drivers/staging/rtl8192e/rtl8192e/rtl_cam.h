 
 
#ifndef _RTL_CAM_H
#define _RTL_CAM_H

#include <linux/types.h>
struct net_device;

void rtl92e_cam_reset(struct net_device *dev);
void rtl92e_enable_hw_security_config(struct net_device *dev);
void rtl92e_set_key(struct net_device *dev, u8 EntryNo, u8 KeyIndex,
		    u16 KeyType, const u8 *MacAddr, u8 DefaultKey,
		    u32 *KeyContent);
void rtl92e_set_swcam(struct net_device *dev, u8 EntryNo, u8 KeyIndex,
		      u16 KeyType, const u8 *MacAddr, u32 *KeyContent);
void rtl92e_cam_restore(struct net_device *dev);

#endif

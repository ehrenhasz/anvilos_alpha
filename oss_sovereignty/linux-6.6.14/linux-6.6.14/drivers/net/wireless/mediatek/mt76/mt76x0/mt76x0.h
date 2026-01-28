#ifndef MT76X0U_H
#define MT76X0U_H
#include <linux/bitfield.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/completion.h>
#include <net/mac80211.h>
#include <linux/debugfs.h>
#include "../mt76x02.h"
#include "eeprom.h"
#define MT7610E_FIRMWARE		"mediatek/mt7610e.bin"
#define MT7650E_FIRMWARE		"mediatek/mt7650e.bin"
#define MT7610U_FIRMWARE		"mediatek/mt7610u.bin"
#define MT_USB_AGGR_SIZE_LIMIT		21  
#define MT_USB_AGGR_TIMEOUT		0x80  
static inline bool is_mt7610e(struct mt76x02_dev *dev)
{
	if (!mt76_is_mmio(&dev->mt76))
		return false;
	return mt76_chip(&dev->mt76) == 0x7610;
}
static inline bool is_mt7630(struct mt76x02_dev *dev)
{
	return mt76_chip(&dev->mt76) == 0x7630;
}
int mt76x0_init_hardware(struct mt76x02_dev *dev);
int mt76x0_register_device(struct mt76x02_dev *dev);
void mt76x0_chip_onoff(struct mt76x02_dev *dev, bool enable, bool reset);
void mt76x0_mac_stop(struct mt76x02_dev *dev);
int mt76x0_config(struct ieee80211_hw *hw, u32 changed);
int mt76x0_set_sar_specs(struct ieee80211_hw *hw,
			 const struct cfg80211_sar_specs *sar);
void mt76x0_phy_init(struct mt76x02_dev *dev);
int mt76x0_phy_wait_bbp_ready(struct mt76x02_dev *dev);
void mt76x0_phy_set_channel(struct mt76x02_dev *dev,
			    struct cfg80211_chan_def *chandef);
void mt76x0_phy_set_txpower(struct mt76x02_dev *dev);
void mt76x0_phy_calibrate(struct mt76x02_dev *dev, bool power_on);
#endif

 
 

#ifndef __MT7996_MAC_H
#define __MT7996_MAC_H

#include "../mt76_connac3_mac.h"

struct mt7996_dfs_pulse {
	u32 max_width;		 
	int max_pwr;		 
	int min_pwr;		 
	u32 min_stgr_pri;	 
	u32 max_stgr_pri;	 
	u32 min_cr_pri;		 
	u32 max_cr_pri;		 
};

struct mt7996_dfs_pattern {
	u8 enb;
	u8 stgr;
	u8 min_crpn;
	u8 max_crpn;
	u8 min_crpr;
	u8 min_pw;
	u32 min_pri;
	u32 max_pri;
	u8 max_pw;
	u8 min_crbn;
	u8 max_crbn;
	u8 min_stgpn;
	u8 max_stgpn;
	u8 min_stgpr;
	u8 rsv[2];
	u32 min_stgpr_diff;
} __packed;

struct mt7996_dfs_radar_spec {
	struct mt7996_dfs_pulse pulse_th;
	struct mt7996_dfs_pattern radar_pattern[16];
};

#endif

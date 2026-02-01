 


#ifndef ATH9K_DFS_DEBUG_H
#define ATH9K_DFS_DEBUG_H

#include "hw.h"

struct ath_softc;

 
struct ath_dfs_stats {
	 
	u32 pulses_total;
	u32 pulses_no_dfs;
	u32 pulses_detected;
	u32 datalen_discards;
	u32 rssi_discards;
	u32 bwinfo_discards;
	u32 pri_phy_errors;
	u32 ext_phy_errors;
	u32 dc_phy_errors;
	 
	u32 pulses_processed;
	u32 radar_detected;
};

#if defined(CONFIG_ATH9K_DFS_DEBUGFS)

#define DFS_STAT_INC(sc, c) (sc->debug.stats.dfs_stats.c++)
void ath9k_dfs_init_debug(struct ath_softc *sc);

extern struct ath_dfs_pool_stats global_dfs_pool_stats;

#else

#define DFS_STAT_INC(sc, c) do { } while (0)
static inline void ath9k_dfs_init_debug(struct ath_softc *sc) { }

#endif  

#endif  

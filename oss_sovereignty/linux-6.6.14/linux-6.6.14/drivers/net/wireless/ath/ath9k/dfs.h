#ifndef ATH9K_DFS_H
#define ATH9K_DFS_H
#include "../dfs_pattern_detector.h"
#if defined(CONFIG_ATH9K_DFS_CERTIFIED)
void ath9k_dfs_process_phyerr(struct ath_softc *sc, void *data,
			      struct ath_rx_status *rs, u64 mactime);
#else
static inline void
ath9k_dfs_process_phyerr(struct ath_softc *sc, void *data,
			 struct ath_rx_status *rs, u64 mactime) { }
#endif
#endif  

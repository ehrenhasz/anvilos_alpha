#ifndef __OCRDMA_STATS_H__
#define __OCRDMA_STATS_H__
#include <linux/debugfs.h>
#include "ocrdma.h"
#include "ocrdma_hw.h"
#define OCRDMA_MAX_DBGFS_MEM 4096
enum OCRDMA_STATS_TYPE {
	OCRDMA_RSRC_STATS,
	OCRDMA_RXSTATS,
	OCRDMA_WQESTATS,
	OCRDMA_TXSTATS,
	OCRDMA_DB_ERRSTATS,
	OCRDMA_RXQP_ERRSTATS,
	OCRDMA_TXQP_ERRSTATS,
	OCRDMA_TX_DBG_STATS,
	OCRDMA_RX_DBG_STATS,
	OCRDMA_DRV_STATS,
	OCRDMA_RESET_STATS
};
void ocrdma_rem_debugfs(void);
void ocrdma_init_debugfs(void);
bool ocrdma_alloc_stats_resources(struct ocrdma_dev *dev);
void ocrdma_release_stats_resources(struct ocrdma_dev *dev);
void ocrdma_rem_port_stats(struct ocrdma_dev *dev);
void ocrdma_add_port_stats(struct ocrdma_dev *dev);
void ocrdma_pma_counters(struct ocrdma_dev *dev, struct ib_mad *out_mad);
#endif	 

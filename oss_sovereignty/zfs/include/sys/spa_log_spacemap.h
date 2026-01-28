#ifndef _SYS_SPA_LOG_SPACEMAP_H
#define	_SYS_SPA_LOG_SPACEMAP_H
#include <sys/avl.h>
typedef struct log_summary_entry {
	uint64_t lse_start;	 
	uint64_t lse_end;	 
	uint64_t lse_txgcount;	 
	uint64_t lse_mscount;	 
	uint64_t lse_msdcount;	 
	uint64_t lse_blkcount;	 
	list_node_t lse_node;
} log_summary_entry_t;
typedef struct spa_unflushed_stats  {
	uint64_t sus_memused;	 
	uint64_t sus_blocklimit;	 
	uint64_t sus_nblocks;	 
} spa_unflushed_stats_t;
typedef struct spa_log_sm {
	uint64_t sls_sm_obj;	 
	uint64_t sls_txg;	 
	uint64_t sls_nblocks;	 
	uint64_t sls_mscount;	 
	avl_node_t sls_node;	 
	space_map_t *sls_sm;	 
} spa_log_sm_t;
int spa_ld_log_spacemaps(spa_t *);
void spa_generate_syncing_log_sm(spa_t *, dmu_tx_t *);
void spa_flush_metaslabs(spa_t *, dmu_tx_t *);
void spa_sync_close_syncing_log_sm(spa_t *);
void spa_cleanup_old_sm_logs(spa_t *, dmu_tx_t *);
uint64_t spa_log_sm_blocklimit(spa_t *);
void spa_log_sm_set_blocklimit(spa_t *);
uint64_t spa_log_sm_nblocks(spa_t *);
uint64_t spa_log_sm_memused(spa_t *);
void spa_log_sm_decrement_mscount(spa_t *, uint64_t);
void spa_log_sm_increment_current_mscount(spa_t *);
void spa_log_summary_add_flushed_metaslab(spa_t *, boolean_t);
void spa_log_summary_dirty_flushed_metaslab(spa_t *, uint64_t);
void spa_log_summary_decrement_mscount(spa_t *, uint64_t, boolean_t);
void spa_log_summary_decrement_blkcount(spa_t *, uint64_t);
boolean_t spa_flush_all_logs_requested(spa_t *);
extern int zfs_keep_log_spacemaps_at_export;
#endif  

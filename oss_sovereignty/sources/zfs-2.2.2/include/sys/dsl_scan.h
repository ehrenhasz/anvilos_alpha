


#ifndef	_SYS_DSL_SCAN_H
#define	_SYS_DSL_SCAN_H

#include <sys/zfs_context.h>
#include <sys/zio.h>
#include <sys/zap.h>
#include <sys/ddt.h>
#include <sys/bplist.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct objset;
struct dsl_dir;
struct dsl_dataset;
struct dsl_pool;
struct dmu_tx;

extern int zfs_scan_suspend_progress;


typedef struct dsl_scan_phys {
	uint64_t scn_func; 
	uint64_t scn_state; 
	uint64_t scn_queue_obj;
	uint64_t scn_min_txg;
	uint64_t scn_max_txg;
	uint64_t scn_cur_min_txg;
	uint64_t scn_cur_max_txg;
	uint64_t scn_start_time;
	uint64_t scn_end_time;
	uint64_t scn_to_examine; 
	uint64_t scn_examined; 
	uint64_t scn_skipped;	
	uint64_t scn_processed;
	uint64_t scn_errors;	
	uint64_t scn_ddt_class_max;
	ddt_bookmark_t scn_ddt_bookmark;
	zbookmark_phys_t scn_bookmark;
	uint64_t scn_flags; 
} dsl_scan_phys_t;

#define	SCAN_PHYS_NUMINTS (sizeof (dsl_scan_phys_t) / sizeof (uint64_t))

typedef enum dsl_scan_flags {
	DSF_VISIT_DS_AGAIN = 1<<0,
	DSF_SCRUB_PAUSED = 1<<1,
} dsl_scan_flags_t;

#define	DSL_SCAN_FLAGS_MASK (DSF_VISIT_DS_AGAIN)

typedef struct dsl_errorscrub_phys {
	uint64_t dep_func; 
	uint64_t dep_state; 
	uint64_t dep_cursor; 
	uint64_t dep_start_time; 
	uint64_t dep_end_time; 
	uint64_t dep_to_examine; 
	uint64_t dep_examined; 
	uint64_t dep_errors;	
	uint64_t dep_paused_flags; 
} dsl_errorscrub_phys_t;

#define	ERRORSCRUB_PHYS_NUMINTS (sizeof (dsl_errorscrub_phys_t) \
	/ sizeof (uint64_t))


typedef struct dsl_scan {
	struct dsl_pool *scn_dp;
	uint64_t scn_restart_txg;
	uint64_t scn_done_txg;
	uint64_t scn_sync_start_time;
	uint64_t scn_issued_before_pass;

	
	boolean_t scn_is_bptree;
	boolean_t scn_async_destroying;
	boolean_t scn_async_stalled;
	uint64_t  scn_async_block_min_time_ms;

	
	boolean_t scn_is_sorted;	
	boolean_t scn_clearing;		
	boolean_t scn_checkpointing;	
	boolean_t scn_suspending;	
	uint64_t scn_last_checkpoint;	

	
	zio_t *scn_zio_root;		
	taskq_t *scn_taskq;		

	
	boolean_t scn_prefetch_stop;	
	zbookmark_phys_t scn_prefetch_bookmark;	
	avl_tree_t scn_prefetch_queue;	
	uint64_t scn_maxinflight_bytes; 

	
	uint64_t scn_visited_this_txg;	
	uint64_t scn_dedup_frees_this_txg;	
	uint64_t scn_holes_this_txg;
	uint64_t scn_lt_min_this_txg;
	uint64_t scn_gt_max_this_txg;
	uint64_t scn_ddt_contained_this_txg;
	uint64_t scn_objsets_visited_this_txg;
	uint64_t scn_avg_seg_size_this_txg;
	uint64_t scn_segs_this_txg;
	uint64_t scn_avg_zio_size_this_txg;
	uint64_t scn_zios_this_txg;

	
	zap_cursor_t errorscrub_cursor;
	
	dsl_scan_phys_t scn_phys;	
	dsl_scan_phys_t scn_phys_cached;
	avl_tree_t scn_queue;		
	uint64_t scn_queues_pending;	
	
	dsl_errorscrub_phys_t errorscrub_phys;
} dsl_scan_t;

typedef struct dsl_scan_io_queue dsl_scan_io_queue_t;

void scan_init(void);
void scan_fini(void);
int dsl_scan_init(struct dsl_pool *dp, uint64_t txg);
int dsl_scan_setup_check(void *, dmu_tx_t *);
void dsl_scan_setup_sync(void *, dmu_tx_t *);
void dsl_scan_fini(struct dsl_pool *dp);
void dsl_scan_sync(struct dsl_pool *, dmu_tx_t *);
int dsl_scan_cancel(struct dsl_pool *);
int dsl_scan(struct dsl_pool *, pool_scan_func_t);
void dsl_scan_assess_vdev(struct dsl_pool *dp, vdev_t *vd);
boolean_t dsl_scan_scrubbing(const struct dsl_pool *dp);
boolean_t dsl_errorscrubbing(const struct dsl_pool *dp);
boolean_t dsl_errorscrub_active(dsl_scan_t *scn);
void dsl_scan_restart_resilver(struct dsl_pool *, uint64_t txg);
int dsl_scrub_set_pause_resume(const struct dsl_pool *dp,
    pool_scrub_cmd_t cmd);
void dsl_errorscrub_sync(struct dsl_pool *, dmu_tx_t *);
boolean_t dsl_scan_resilvering(struct dsl_pool *dp);
boolean_t dsl_scan_resilver_scheduled(struct dsl_pool *dp);
boolean_t dsl_dataset_unstable(struct dsl_dataset *ds);
void dsl_scan_ddt_entry(dsl_scan_t *scn, enum zio_checksum checksum,
    ddt_entry_t *dde, dmu_tx_t *tx);
void dsl_scan_ds_destroyed(struct dsl_dataset *ds, struct dmu_tx *tx);
void dsl_scan_ds_snapshotted(struct dsl_dataset *ds, struct dmu_tx *tx);
void dsl_scan_ds_clone_swapped(struct dsl_dataset *ds1, struct dsl_dataset *ds2,
    struct dmu_tx *tx);
boolean_t dsl_scan_active(dsl_scan_t *scn);
boolean_t dsl_scan_is_paused_scrub(const dsl_scan_t *scn);
boolean_t dsl_errorscrub_is_paused(const dsl_scan_t *scn);
void dsl_scan_freed(spa_t *spa, const blkptr_t *bp);
void dsl_scan_io_queue_destroy(dsl_scan_io_queue_t *queue);
void dsl_scan_io_queue_vdev_xfer(vdev_t *svd, vdev_t *tvd);

#ifdef	__cplusplus
}
#endif

#endif 

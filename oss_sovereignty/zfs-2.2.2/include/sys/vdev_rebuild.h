 
 

#ifndef	_SYS_VDEV_REBUILD_H
#define	_SYS_VDEV_REBUILD_H

#include <sys/spa.h>

#ifdef	__cplusplus
extern "C" {
#endif

 
#define	REBUILD_PHYS_ENTRIES	12

 
typedef struct vdev_rebuild_phys {
	uint64_t	vrp_rebuild_state;	 
	uint64_t	vrp_last_offset;	 
	uint64_t	vrp_min_txg;		 
	uint64_t	vrp_max_txg;		 
	uint64_t	vrp_start_time;		 
	uint64_t	vrp_end_time;		 
	uint64_t	vrp_scan_time_ms;	 
	uint64_t	vrp_bytes_scanned;	 
	uint64_t	vrp_bytes_issued;	 
	uint64_t	vrp_bytes_rebuilt;	 
	uint64_t	vrp_bytes_est;		 
	uint64_t	vrp_errors;		 
} vdev_rebuild_phys_t;

 
typedef struct vdev_rebuild {
	vdev_t		*vr_top_vdev;		 
	metaslab_t	*vr_scan_msp;		 
	range_tree_t	*vr_scan_tree;		 
	kmutex_t	vr_io_lock;		 
	kcondvar_t	vr_io_cv;		 

	 
	uint64_t	vr_scan_offset[TXG_SIZE];
	uint64_t	vr_prev_scan_time_ms;	 
	uint64_t	vr_bytes_inflight_max;	 
	uint64_t	vr_bytes_inflight;	 

	 
	uint64_t	vr_pass_start_time;
	uint64_t	vr_pass_bytes_scanned;
	uint64_t	vr_pass_bytes_issued;
	uint64_t	vr_pass_bytes_skipped;

	 
	vdev_rebuild_phys_t vr_rebuild_phys;
} vdev_rebuild_t;

boolean_t vdev_rebuild_active(vdev_t *);

int vdev_rebuild_load(vdev_t *);
void vdev_rebuild(vdev_t *);
void vdev_rebuild_stop_wait(vdev_t *);
void vdev_rebuild_stop_all(spa_t *);
void vdev_rebuild_restart(spa_t *);
void vdev_rebuild_clear_sync(void *, dmu_tx_t *);
int vdev_rebuild_get_stats(vdev_t *, vdev_rebuild_stat_t *);

#ifdef	__cplusplus
}
#endif

#endif  

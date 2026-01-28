#ifndef _SYS_VDEV_REMOVAL_H
#define	_SYS_VDEV_REMOVAL_H
#include <sys/spa.h>
#include <sys/bpobj.h>
#include <sys/vdev_indirect_mapping.h>
#include <sys/vdev_indirect_births.h>
#ifdef	__cplusplus
extern "C" {
#endif
typedef struct spa_vdev_removal {
	uint64_t	svr_vdev_id;
	uint64_t	svr_max_offset_to_sync[TXG_SIZE];
	kthread_t	*svr_thread;
	range_tree_t	*svr_allocd_segs;
	kmutex_t	svr_lock;
	kcondvar_t	svr_cv;
	boolean_t	svr_thread_exit;
	list_t		svr_new_segments[TXG_SIZE];
	range_tree_t	*svr_frees[TXG_SIZE];
	uint64_t	svr_bytes_done[TXG_SIZE];
	nvlist_t	*svr_zaplist;
} spa_vdev_removal_t;
typedef struct spa_condensing_indirect {
	list_t		sci_new_mapping_entries[TXG_SIZE];
	vdev_indirect_mapping_t *sci_new_mapping;
} spa_condensing_indirect_t;
extern int spa_remove_init(spa_t *);
extern void spa_restart_removal(spa_t *);
extern int spa_condense_init(spa_t *);
extern void spa_condense_fini(spa_t *);
extern void spa_start_indirect_condensing_thread(spa_t *);
extern void spa_vdev_condense_suspend(spa_t *);
extern int spa_vdev_remove(spa_t *, uint64_t, boolean_t);
extern void free_from_removing_vdev(vdev_t *, uint64_t, uint64_t);
extern int spa_removal_get_stats(spa_t *, pool_removal_stat_t *);
extern void svr_sync(spa_t *, dmu_tx_t *);
extern void spa_vdev_remove_suspend(spa_t *);
extern int spa_vdev_remove_cancel(spa_t *);
extern void spa_vdev_removal_destroy(spa_vdev_removal_t *);
extern uint64_t spa_remove_max_segment(spa_t *);
extern uint_t vdev_removal_max_span;
#ifdef	__cplusplus
}
#endif
#endif	 

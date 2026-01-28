#ifndef	_SYS_ZIL_IMPL_H
#define	_SYS_ZIL_IMPL_H
#include <sys/zil.h>
#include <sys/dmu_objset.h>
#ifdef	__cplusplus
extern "C" {
#endif
typedef enum {
    LWB_STATE_NEW,
    LWB_STATE_OPENED,
    LWB_STATE_CLOSED,
    LWB_STATE_READY,
    LWB_STATE_ISSUED,
    LWB_STATE_WRITE_DONE,
    LWB_STATE_FLUSH_DONE,
    LWB_NUM_STATES
} lwb_state_t;
typedef struct lwb {
	zilog_t		*lwb_zilog;	 
	blkptr_t	lwb_blk;	 
	boolean_t	lwb_slim;	 
	boolean_t	lwb_slog;	 
	int		lwb_error;	 
	int		lwb_nmax;	 
	int		lwb_nused;	 
	int		lwb_nfilled;	 
	int		lwb_sz;		 
	lwb_state_t	lwb_state;	 
	char		*lwb_buf;	 
	zio_t		*lwb_child_zio;	 
	zio_t		*lwb_write_zio;	 
	zio_t		*lwb_root_zio;	 
	hrtime_t	lwb_issued_timestamp;  
	uint64_t	lwb_issued_txg;	 
	uint64_t	lwb_alloc_txg;	 
	uint64_t	lwb_max_txg;	 
	list_node_t	lwb_node;	 
	list_node_t	lwb_issue_node;	 
	list_t		lwb_itxs;	 
	list_t		lwb_waiters;	 
	avl_tree_t	lwb_vdev_tree;	 
	kmutex_t	lwb_vdev_lock;	 
} lwb_t;
typedef struct zil_commit_waiter {
	kcondvar_t	zcw_cv;		 
	kmutex_t	zcw_lock;	 
	list_node_t	zcw_node;	 
	lwb_t		*zcw_lwb;	 
	boolean_t	zcw_done;	 
	int		zcw_zio_error;	 
} zil_commit_waiter_t;
typedef struct itxs {
	list_t		i_sync_list;	 
	avl_tree_t	i_async_tree;	 
} itxs_t;
typedef struct itxg {
	kmutex_t	itxg_lock;	 
	uint64_t	itxg_txg;	 
	itxs_t		*itxg_itxs;	 
} itxg_t;
typedef struct itx_async_node {
	uint64_t	ia_foid;	 
	list_t		ia_list;	 
	avl_node_t	ia_node;	 
} itx_async_node_t;
typedef struct zil_vdev_node {
	uint64_t	zv_vdev;	 
	avl_node_t	zv_node;	 
} zil_vdev_node_t;
#define	ZIL_PREV_BLKS 16
struct zilog {
	kmutex_t	zl_lock;	 
	struct dsl_pool	*zl_dmu_pool;	 
	spa_t		*zl_spa;	 
	const zil_header_t *zl_header;	 
	objset_t	*zl_os;		 
	zil_get_data_t	*zl_get_data;	 
	lwb_t		*zl_last_lwb_opened;  
	hrtime_t	zl_last_lwb_latency;  
	uint64_t	zl_lr_seq;	 
	uint64_t	zl_commit_lr_seq;  
	uint64_t	zl_destroy_txg;	 
	uint64_t	zl_replayed_seq[TXG_SIZE];  
	uint64_t	zl_replaying_seq;  
	uint32_t	zl_suspend;	 
	kcondvar_t	zl_cv_suspend;	 
	uint8_t		zl_suspending;	 
	uint8_t		zl_keep_first;	 
	uint8_t		zl_replay;	 
	uint8_t		zl_stop_sync;	 
	kmutex_t	zl_issuer_lock;	 
	uint8_t		zl_logbias;	 
	uint8_t		zl_sync;	 
	int		zl_parse_error;	 
	uint64_t	zl_parse_blk_seq;  
	uint64_t	zl_parse_lr_seq;  
	uint64_t	zl_parse_blk_count;  
	uint64_t	zl_parse_lr_count;  
	itxg_t		zl_itxg[TXG_SIZE];  
	list_t		zl_itx_commit_list;  
	uint64_t	zl_cur_used;	 
	list_t		zl_lwb_list;	 
	avl_tree_t	zl_bp_tree;	 
	clock_t		zl_replay_time;	 
	uint64_t	zl_replay_blks;	 
	zil_header_t	zl_old_header;	 
	uint_t		zl_prev_blks[ZIL_PREV_BLKS];  
	uint_t		zl_prev_rotor;	 
	txg_node_t	zl_dirty_link;	 
	uint64_t	zl_dirty_max_txg;  
	kmutex_t	zl_lwb_io_lock;  
	uint64_t	zl_lwb_inflight[TXG_SIZE];  
	kcondvar_t	zl_lwb_io_cv;	 
	uint64_t	zl_lwb_max_issued_txg;  
	uint64_t	zl_max_block_size;
	zil_sums_t *zl_sums;
};
typedef struct zil_bp_node {
	dva_t		zn_dva;
	avl_node_t	zn_node;
} zil_bp_node_t;
#ifdef	__cplusplus
}
#endif
#endif	 

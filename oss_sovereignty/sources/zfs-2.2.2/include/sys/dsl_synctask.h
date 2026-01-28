


#ifndef	_SYS_DSL_SYNCTASK_H
#define	_SYS_DSL_SYNCTASK_H

#include <sys/txg.h>
#include <sys/zfs_context.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct dsl_pool;

typedef int (dsl_checkfunc_t)(void *, dmu_tx_t *);
typedef void (dsl_syncfunc_t)(void *, dmu_tx_t *);
typedef void (dsl_sigfunc_t)(void *, dmu_tx_t *);

typedef enum zfs_space_check {
	
	ZFS_SPACE_CHECK_NORMAL,

	
	ZFS_SPACE_CHECK_RESERVED,

	
	ZFS_SPACE_CHECK_EXTRA_RESERVED,

	
	ZFS_SPACE_CHECK_DESTROY = ZFS_SPACE_CHECK_EXTRA_RESERVED,

	
	ZFS_SPACE_CHECK_ZCP_EVAL = ZFS_SPACE_CHECK_DESTROY,

	
	ZFS_SPACE_CHECK_NONE,

	ZFS_SPACE_CHECK_DISCARD_CHECKPOINT = ZFS_SPACE_CHECK_NONE,

} zfs_space_check_t;

typedef struct dsl_sync_task {
	txg_node_t dst_node;
	struct dsl_pool *dst_pool;
	uint64_t dst_txg;
	int dst_space;
	zfs_space_check_t dst_space_check;
	dsl_checkfunc_t *dst_checkfunc;
	dsl_syncfunc_t *dst_syncfunc;
	void *dst_arg;
	int dst_error;
	boolean_t dst_nowaiter;
} dsl_sync_task_t;

void dsl_sync_task_sync(dsl_sync_task_t *, dmu_tx_t *);
int dsl_sync_task(const char *, dsl_checkfunc_t *,
    dsl_syncfunc_t *, void *, int, zfs_space_check_t);
void dsl_sync_task_nowait(struct dsl_pool *, dsl_syncfunc_t *,
    void *, dmu_tx_t *);
int dsl_early_sync_task(const char *, dsl_checkfunc_t *,
    dsl_syncfunc_t *, void *, int, zfs_space_check_t);
void dsl_early_sync_task_nowait(struct dsl_pool *, dsl_syncfunc_t *,
    void *, dmu_tx_t *);
int dsl_sync_task_sig(const char *, dsl_checkfunc_t *, dsl_syncfunc_t *,
    dsl_sigfunc_t *, void *, int, zfs_space_check_t);

#ifdef	__cplusplus
}
#endif

#endif 

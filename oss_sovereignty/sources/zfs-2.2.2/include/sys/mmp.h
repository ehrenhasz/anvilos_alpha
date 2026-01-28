


#ifndef _SYS_MMP_H
#define	_SYS_MMP_H

#include <sys/spa.h>
#include <sys/zfs_context.h>
#include <sys/uberblock_impl.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MMP_MIN_INTERVAL		100	
#define	MMP_DEFAULT_INTERVAL		1000	
#define	MMP_DEFAULT_IMPORT_INTERVALS	20
#define	MMP_DEFAULT_FAIL_INTERVALS	10
#define	MMP_MIN_FAIL_INTERVALS		2	
#define	MMP_IMPORT_SAFETY_FACTOR	200	
#define	MMP_INTERVAL_OK(interval)	MAX(interval, MMP_MIN_INTERVAL)
#define	MMP_FAIL_INTVS_OK(fails)	(fails == 0 ? 0 : MAX(fails, \
					    MMP_MIN_FAIL_INTERVALS))

typedef struct mmp_thread {
	kmutex_t	mmp_thread_lock; 
	kcondvar_t	mmp_thread_cv;
	kthread_t	*mmp_thread;
	uint8_t		mmp_thread_exiting;
	kmutex_t	mmp_io_lock;	
	hrtime_t	mmp_last_write;	
	uint64_t	mmp_delay;	
	uberblock_t	mmp_ub;		
	zio_t		*mmp_zio_root;	
	uint64_t	mmp_kstat_id;	
	int		mmp_skip_error; 
	vdev_t		*mmp_last_leaf;	
	uint64_t	mmp_leaf_last_gen;	
	uint32_t	mmp_seq;	
} mmp_thread_t;


extern void mmp_init(struct spa *spa);
extern void mmp_fini(struct spa *spa);
extern void mmp_thread_start(struct spa *spa);
extern void mmp_thread_stop(struct spa *spa);
extern void mmp_update_uberblock(struct spa *spa, struct uberblock *ub);
extern void mmp_signal_all_threads(void);


extern int param_set_multihost_interval(ZFS_MODULE_PARAM_ARGS);
extern uint64_t zfs_multihost_interval;
extern uint_t zfs_multihost_fail_intervals;
extern uint_t zfs_multihost_import_intervals;

#ifdef	__cplusplus
}
#endif

#endif	

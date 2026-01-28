



#ifndef	_SYS_RR_RW_LOCK_H
#define	_SYS_RR_RW_LOCK_H



#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/inttypes.h>
#include <sys/zfs_context.h>
#include <sys/zfs_refcount.h>

extern uint_t rrw_tsd_key;


typedef struct rrwlock {
	kmutex_t	rr_lock;
	kcondvar_t	rr_cv;
	kthread_t	*rr_writer;
	zfs_refcount_t	rr_anon_rcount;
	zfs_refcount_t	rr_linked_rcount;
	boolean_t	rr_writer_wanted;
	boolean_t	rr_track_all;
} rrwlock_t;


void rrw_init(rrwlock_t *rrl, boolean_t track_all);
void rrw_destroy(rrwlock_t *rrl);
void rrw_enter(rrwlock_t *rrl, krw_t rw, const void *tag);
void rrw_enter_read(rrwlock_t *rrl, const void *tag);
void rrw_enter_read_prio(rrwlock_t *rrl, const void *tag);
void rrw_enter_write(rrwlock_t *rrl);
void rrw_exit(rrwlock_t *rrl, const void *tag);
boolean_t rrw_held(rrwlock_t *rrl, krw_t rw);
void rrw_tsd_destroy(void *arg);

#define	RRW_READ_HELD(x)	rrw_held(x, RW_READER)
#define	RRW_WRITE_HELD(x)	rrw_held(x, RW_WRITER)
#define	RRW_LOCK_HELD(x) \
	(rrw_held(x, RW_WRITER) || rrw_held(x, RW_READER))


#define	RRM_NUM_LOCKS		17
typedef struct rrmlock {
	rrwlock_t	locks[RRM_NUM_LOCKS];
} rrmlock_t;

void rrm_init(rrmlock_t *rrl, boolean_t track_all);
void rrm_destroy(rrmlock_t *rrl);
void rrm_enter(rrmlock_t *rrl, krw_t rw, const void *tag);
void rrm_enter_read(rrmlock_t *rrl, const void *tag);
void rrm_enter_write(rrmlock_t *rrl);
void rrm_exit(rrmlock_t *rrl, const void *tag);
boolean_t rrm_held(rrmlock_t *rrl, krw_t rw);

#define	RRM_READ_HELD(x)	rrm_held(x, RW_READER)
#define	RRM_WRITE_HELD(x)	rrm_held(x, RW_WRITER)
#define	RRM_LOCK_HELD(x) \
	(rrm_held(x, RW_WRITER) || rrm_held(x, RW_READER))

#ifdef	__cplusplus
}
#endif

#endif	

 

#ifndef _SPL_CONDVAR_H
#define	_SPL_CONDVAR_H

#include <linux/module.h>
#include <sys/kmem.h>
#include <sys/mutex.h>
#include <sys/callo.h>
#include <sys/wait.h>
#include <sys/time.h>

 


 
#define	CV_MAGIC			0x346545f4
#define	CV_DESTROY			0x346545f5

typedef struct {
	int cv_magic;
	spl_wait_queue_head_t cv_event;
	spl_wait_queue_head_t cv_destroy;
	atomic_t cv_refs;
	atomic_t cv_waiters;
	kmutex_t *cv_mutex;
} kcondvar_t;

typedef enum { CV_DEFAULT = 0, CV_DRIVER } kcv_type_t;

extern void __cv_init(kcondvar_t *, char *, kcv_type_t, void *);
extern void __cv_destroy(kcondvar_t *);
extern void __cv_wait(kcondvar_t *, kmutex_t *);
extern void __cv_wait_io(kcondvar_t *, kmutex_t *);
extern void __cv_wait_idle(kcondvar_t *, kmutex_t *);
extern int __cv_wait_io_sig(kcondvar_t *, kmutex_t *);
extern int __cv_wait_sig(kcondvar_t *, kmutex_t *);
extern int __cv_timedwait(kcondvar_t *, kmutex_t *, clock_t);
extern int __cv_timedwait_io(kcondvar_t *, kmutex_t *, clock_t);
extern int __cv_timedwait_sig(kcondvar_t *, kmutex_t *, clock_t);
extern int __cv_timedwait_idle(kcondvar_t *, kmutex_t *, clock_t);
extern int cv_timedwait_hires(kcondvar_t *, kmutex_t *, hrtime_t,
    hrtime_t res, int flag);
extern int cv_timedwait_sig_hires(kcondvar_t *, kmutex_t *, hrtime_t,
    hrtime_t res, int flag);
extern int cv_timedwait_idle_hires(kcondvar_t *, kmutex_t *, hrtime_t,
    hrtime_t res, int flag);
extern void __cv_signal(kcondvar_t *);
extern void __cv_broadcast(kcondvar_t *c);

#define	cv_init(cvp, name, type, arg)		__cv_init(cvp, name, type, arg)
#define	cv_destroy(cvp)				__cv_destroy(cvp)
#define	cv_wait(cvp, mp)			__cv_wait(cvp, mp)
#define	cv_wait_io(cvp, mp)			__cv_wait_io(cvp, mp)
#define	cv_wait_idle(cvp, mp)			__cv_wait_idle(cvp, mp)
#define	cv_wait_io_sig(cvp, mp)			__cv_wait_io_sig(cvp, mp)
#define	cv_wait_sig(cvp, mp)			__cv_wait_sig(cvp, mp)
#define	cv_signal(cvp)				__cv_signal(cvp)
#define	cv_broadcast(cvp)			__cv_broadcast(cvp)

 
#define	cv_timedwait(cvp, mp, t)		__cv_timedwait(cvp, mp, t)
#define	cv_timedwait_io(cvp, mp, t)		__cv_timedwait_io(cvp, mp, t)
#define	cv_timedwait_sig(cvp, mp, t)		__cv_timedwait_sig(cvp, mp, t)
#define	cv_timedwait_idle(cvp, mp, t)		__cv_timedwait_idle(cvp, mp, t)


#endif  

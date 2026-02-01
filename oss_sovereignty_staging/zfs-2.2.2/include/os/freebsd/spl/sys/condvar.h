 

#ifndef _OPENSOLARIS_SYS_CONDVAR_H_
#define	_OPENSOLARIS_SYS_CONDVAR_H_

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/spl_condvar.h>
#include <sys/mutex.h>
#include <sys/time.h>
#include <sys/errno.h>

 

static __inline sbintime_t
zfs_nstosbt(int64_t _ns)
{
	sbintime_t sb = 0;

#ifdef KASSERT
	KASSERT(_ns >= 0, ("Negative values illegal for nstosbt: %jd", _ns));
#endif
	if (_ns >= SBT_1S) {
		sb = (_ns / 1000000000) * SBT_1S;
		_ns = _ns % 1000000000;
	}
	 
	sb += ((_ns * 9223372037ull) + 0x7fffffff) >> 31;
	return (sb);
}


typedef struct cv	kcondvar_t;
#define	CALLOUT_FLAG_ABSOLUTE C_ABSOLUTE

typedef enum {
	CV_DEFAULT,
	CV_DRIVER
} kcv_type_t;

#define	zfs_cv_init(cv, name, type, arg)	do {			\
	const char *_name;						\
	ASSERT((type) == CV_DEFAULT);					\
	for (_name = #cv; *_name != '\0'; _name++) {			\
		if (*_name >= 'a' && *_name <= 'z')			\
			break;						\
	}								\
	if (*_name == '\0')						\
		_name = #cv;						\
	cv_init((cv), _name);						\
} while (0)
#define	cv_init(cv, name, type, arg)	zfs_cv_init(cv, name, type, arg)


static inline int
cv_wait_sig(kcondvar_t *cvp, kmutex_t *mp)
{

	return (_cv_wait_sig(cvp, &(mp)->lock_object) == 0);
}

static inline int
cv_timedwait(kcondvar_t *cvp, kmutex_t *mp, clock_t timo)
{
	int rc;

	timo -= ddi_get_lbolt();
	if (timo <= 0)
		return (-1);
	rc = _cv_timedwait_sbt((cvp), &(mp)->lock_object, \
	    tick_sbt * (timo), 0, C_HARDCLOCK);
	if (rc == EWOULDBLOCK)
		return (-1);
	return (1);
}

static inline int
cv_timedwait_sig(kcondvar_t *cvp, kmutex_t *mp, clock_t timo)
{
	int rc;

	timo -= ddi_get_lbolt();
	if (timo <= 0)
		return (-1);
	rc = _cv_timedwait_sig_sbt(cvp, &(mp)->lock_object, \
	    tick_sbt * (timo), 0, C_HARDCLOCK);
	if (rc == EWOULDBLOCK)
		return (-1);
	if (rc == EINTR || rc == ERESTART)
		return (0);

	return (1);
}

#define	cv_timedwait_io		cv_timedwait
#define	cv_timedwait_idle	cv_timedwait
#define	cv_timedwait_sig_io	cv_timedwait_sig
#define	cv_wait_io		cv_wait
#define	cv_wait_io_sig		cv_wait_sig
#define	cv_wait_idle		cv_wait
#define	cv_timedwait_io_hires	cv_timedwait_hires
#define	cv_timedwait_idle_hires cv_timedwait_hires

static inline int
cv_timedwait_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim, hrtime_t res,
    int flag)
{
	hrtime_t hrtime;
	int rc;

	ASSERT(tim >= res);

	hrtime = gethrtime();
	if (flag == 0)
		tim += hrtime;

	if (hrtime >= tim)
		return (-1);
	rc = cv_timedwait_sbt(cvp, mp, zfs_nstosbt(tim),
	    zfs_nstosbt(res), C_ABSOLUTE);

	if (rc == EWOULDBLOCK)
		return (-1);

	KASSERT(rc == 0, ("unexpected rc value %d", rc));
	return (1);
}

static inline int
cv_timedwait_sig_hires(kcondvar_t *cvp, kmutex_t *mp, hrtime_t tim,
    hrtime_t res, int flag)
{
	sbintime_t sbt;
	hrtime_t hrtime;
	int rc;

	ASSERT(tim >= res);

	hrtime = gethrtime();
	if (flag == 0)
		tim += hrtime;

	if (hrtime >= tim)
		return (-1);

	sbt = zfs_nstosbt(tim);
	rc = cv_timedwait_sig_sbt(cvp, mp, sbt, zfs_nstosbt(res), C_ABSOLUTE);

	switch (rc) {
	case EWOULDBLOCK:
		return (-1);
	case EINTR:
	case ERESTART:
		return (0);
	default:
		KASSERT(rc == 0, ("unexpected rc value %d", rc));
		return (1);
	}
}

#endif	 

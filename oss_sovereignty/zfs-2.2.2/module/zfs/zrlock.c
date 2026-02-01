 
 

 
#include <sys/zrlock.h>
#include <sys/trace_zfs.h>

 
#define	ZRL_LOCKED	-1
#define	ZRL_DESTROYED	-2

void
zrl_init(zrlock_t *zrl)
{
	mutex_init(&zrl->zr_mtx, NULL, MUTEX_DEFAULT, NULL);
	zrl->zr_refcount = 0;
	cv_init(&zrl->zr_cv, NULL, CV_DEFAULT, NULL);
#ifdef	ZFS_DEBUG
	zrl->zr_owner = NULL;
	zrl->zr_caller = NULL;
#endif
}

void
zrl_destroy(zrlock_t *zrl)
{
	ASSERT0(zrl->zr_refcount);

	mutex_destroy(&zrl->zr_mtx);
	zrl->zr_refcount = ZRL_DESTROYED;
	cv_destroy(&zrl->zr_cv);
}

void
zrl_add_impl(zrlock_t *zrl, const char *zc)
{
	for (;;) {
		uint32_t n = (uint32_t)zrl->zr_refcount;
		while (n != ZRL_LOCKED) {
			uint32_t cas = atomic_cas_32(
			    (uint32_t *)&zrl->zr_refcount, n, n + 1);
			if (cas == n) {
				ASSERT3S((int32_t)n, >=, 0);
#ifdef	ZFS_DEBUG
				if (zrl->zr_owner == curthread) {
					DTRACE_PROBE3(zrlock__reentry,
					    zrlock_t *, zrl,
					    kthread_t *, curthread,
					    uint32_t, n);
				}
				zrl->zr_owner = curthread;
				zrl->zr_caller = zc;
#endif
				return;
			}
			n = cas;
		}

		mutex_enter(&zrl->zr_mtx);
		while (zrl->zr_refcount == ZRL_LOCKED) {
			cv_wait(&zrl->zr_cv, &zrl->zr_mtx);
		}
		mutex_exit(&zrl->zr_mtx);
	}
}

void
zrl_remove(zrlock_t *zrl)
{
#ifdef	ZFS_DEBUG
	if (zrl->zr_owner == curthread) {
		zrl->zr_owner = NULL;
		zrl->zr_caller = NULL;
	}
	int32_t n = atomic_dec_32_nv((uint32_t *)&zrl->zr_refcount);
	ASSERT3S(n, >=, 0);
#else
	atomic_dec_32((uint32_t *)&zrl->zr_refcount);
#endif
}

int
zrl_tryenter(zrlock_t *zrl)
{
	uint32_t n = (uint32_t)zrl->zr_refcount;

	if (n == 0) {
		uint32_t cas = atomic_cas_32(
		    (uint32_t *)&zrl->zr_refcount, 0, ZRL_LOCKED);
		if (cas == 0) {
#ifdef	ZFS_DEBUG
			ASSERT3P(zrl->zr_owner, ==, NULL);
			zrl->zr_owner = curthread;
#endif
			return (1);
		}
	}

	ASSERT3S((int32_t)n, >, ZRL_DESTROYED);

	return (0);
}

void
zrl_exit(zrlock_t *zrl)
{
	ASSERT3S(zrl->zr_refcount, ==, ZRL_LOCKED);

	mutex_enter(&zrl->zr_mtx);
#ifdef	ZFS_DEBUG
	ASSERT3P(zrl->zr_owner, ==, curthread);
	zrl->zr_owner = NULL;
	membar_producer();	 
#endif
	zrl->zr_refcount = 0;
	cv_broadcast(&zrl->zr_cv);
	mutex_exit(&zrl->zr_mtx);
}

int
zrl_is_zero(zrlock_t *zrl)
{
	ASSERT3S(zrl->zr_refcount, >, ZRL_DESTROYED);

	return (zrl->zr_refcount <= 0);
}

int
zrl_is_locked(zrlock_t *zrl)
{
	ASSERT3S(zrl->zr_refcount, >, ZRL_DESTROYED);

	return (zrl->zr_refcount == ZRL_LOCKED);
}

#ifdef	ZFS_DEBUG
kthread_t *
zrl_owner(zrlock_t *zrl)
{
	return (zrl->zr_owner);
}
#endif

#if defined(_KERNEL)

EXPORT_SYMBOL(zrl_add_impl);
EXPORT_SYMBOL(zrl_remove);

#endif

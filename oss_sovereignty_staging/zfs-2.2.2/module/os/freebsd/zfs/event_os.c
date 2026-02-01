 

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/event.h>

#include <sys/freebsd_event.h>

static void
knlist_sx_xlock(void *arg)
{

	sx_xlock((struct sx *)arg);
}

static void
knlist_sx_xunlock(void *arg)
{

	sx_xunlock((struct sx *)arg);
}

#if __FreeBSD_version >= 1300128
static void
knlist_sx_assert_lock(void *arg, int what)
{

	if (what == LA_LOCKED)
		sx_assert((struct sx *)arg, SX_LOCKED);
	else
		sx_assert((struct sx *)arg, SX_UNLOCKED);
}
#else
static void
knlist_sx_assert_locked(void *arg)
{
	sx_assert((struct sx *)arg, SX_LOCKED);
}
static void
knlist_sx_assert_unlocked(void *arg)
{
	sx_assert((struct sx *)arg, SX_UNLOCKED);
}
#endif

void
knlist_init_sx(struct knlist *knl, struct sx *lock)
{

#if __FreeBSD_version >= 1300128
	knlist_init(knl, lock, knlist_sx_xlock, knlist_sx_xunlock,
	    knlist_sx_assert_lock);
#else
	knlist_init(knl, lock, knlist_sx_xlock, knlist_sx_xunlock,
	    knlist_sx_assert_locked, knlist_sx_assert_unlocked);
#endif
}

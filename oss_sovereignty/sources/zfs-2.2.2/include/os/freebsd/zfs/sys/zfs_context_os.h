

#ifndef ZFS_CONTEXT_OS_H_
#define	ZFS_CONTEXT_OS_H_

#include <sys/condvar.h>
#include <sys/rwlock.h>
#include <sys/sig.h>
#include_next <sys/sdt.h>
#include <sys/misc.h>
#include <sys/kdb.h>
#include <sys/pathname.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <sys/ccompat.h>
#include <linux/types.h>

#if KSTACK_PAGES * PAGE_SIZE >= 16384
#define	HAVE_LARGE_STACKS	1
#endif

#define	taskq_create_sysdc(a, b, d, e, p, dc, f) \
	    ((void) sizeof (dc), taskq_create(a, b, maxclsyspri, d, e, f))

#define	tsd_create(keyp, destructor)    do {                 \
		*(keyp) = osd_thread_register((destructor));         \
		KASSERT(*(keyp) > 0, ("cannot register OSD"));       \
} while (0)

#define	tsd_destroy(keyp)	osd_thread_deregister(*(keyp))
#define	tsd_get(key)	osd_thread_get(curthread, (key))
#define	tsd_set(key, value)	osd_thread_set(curthread, (key), (value))
#define	fm_panic	panic

extern int zfs_debug_level;
extern struct mtx zfs_debug_mtx;
#define	ZFS_LOG(lvl, ...) do {   \
		if (((lvl) & 0xff) <= zfs_debug_level) {  \
			mtx_lock(&zfs_debug_mtx);			  \
			printf("%s:%u[%d]: ",				  \
			    __func__, __LINE__, (lvl)); \
			printf(__VA_ARGS__); \
			printf("\n"); \
			if ((lvl) & 0x100) \
				kdb_backtrace(); \
			mtx_unlock(&zfs_debug_mtx);	\
	}	   \
} while (0)

#define	MSEC_TO_TICK(msec)	(howmany((hrtime_t)(msec) * hz, MILLISEC))
extern int hz;
extern int tick;
typedef int fstrans_cookie_t;
#define	spl_fstrans_mark() (0)
#define	spl_fstrans_unmark(x) ((void)x)
#define	signal_pending(x) SIGPENDING(x)
#define	current curthread
#define	thread_join(x)
typedef struct opensolaris_utsname	utsname_t;
extern utsname_t *utsname(void);
extern int spa_import_rootpool(const char *name, bool checkpointrewind);
#endif

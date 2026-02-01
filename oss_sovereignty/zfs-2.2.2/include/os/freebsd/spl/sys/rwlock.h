 

#ifndef _OPENSOLARIS_SYS_RWLOCK_H_
#define	_OPENSOLARIS_SYS_RWLOCK_H_

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sx.h>

typedef enum {
	RW_DEFAULT = 4		 
} krw_type_t;


typedef enum {
	RW_NONE		= 0,
	RW_WRITER	= 1,
	RW_READER	= 2
} krw_t;

typedef	struct sx	krwlock_t;

#ifndef OPENSOLARIS_WITNESS
#define	RW_FLAGS	(SX_DUPOK | SX_NEW | SX_NOWITNESS)
#else
#define	RW_FLAGS	(SX_DUPOK | SX_NEW)
#endif

#define	RW_READ_HELD(x)		(rw_read_held((x)))
#define	RW_WRITE_HELD(x)	(rw_write_held((x)))
#define	RW_LOCK_HELD(x)		(rw_lock_held((x)))
#define	RW_ISWRITER(x)		(rw_iswriter(x))
#define	rw_init(lock, desc, type, arg)	do {				\
	const char *_name;						\
	ASSERT((type) == 0 || (type) == RW_DEFAULT);			\
	for (_name = #lock; *_name != '\0'; _name++) {			\
		if (*_name >= 'a' && *_name <= 'z')			\
			break;						\
	}								\
	if (*_name == '\0')						\
		_name = #lock;						\
	sx_init_flags((lock), _name, RW_FLAGS);				\
} while (0)
#define	rw_destroy(lock)	sx_destroy(lock)
#define	rw_enter(lock, how)	do {					\
	if ((how) == RW_READER)						\
		sx_slock(lock);						\
	else  				\
		sx_xlock(lock);						\
	} while (0)

#define	rw_tryenter(lock, how)			   \
	((how) == RW_READER ? sx_try_slock(lock) : sx_try_xlock(lock))
#define	rw_exit(lock)		sx_unlock(lock)
#define	rw_downgrade(lock)	sx_downgrade(lock)
#define	rw_tryupgrade(lock)	sx_try_upgrade(lock)
#define	rw_read_held(lock)					  \
	((lock)->sx_lock != SX_LOCK_UNLOCKED &&	  \
	    ((lock)->sx_lock & SX_LOCK_SHARED))
#define	rw_write_held(lock)	sx_xlocked(lock)
#define	rw_lock_held(lock)	(rw_read_held(lock) || rw_write_held(lock))
#define	rw_iswriter(lock)	sx_xlocked(lock)
#define	rw_owner(lock)		sx_xholder(lock)

#endif	 

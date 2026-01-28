#ifndef _OPENSOLARIS_SYS_MUTEX_H_
#define	_OPENSOLARIS_SYS_MUTEX_H_
typedef struct sx	kmutex_t;
#include <sys/param.h>
#include <sys/lock.h>
#include_next <sys/sdt.h>
#include_next <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>
typedef enum {
	MUTEX_DEFAULT = 0	 
} kmutex_type_t;
#define	MUTEX_HELD(x)		(mutex_owned(x))
#define	MUTEX_NOT_HELD(x)	(!mutex_owned(x) || panicstr)
#ifndef OPENSOLARIS_WITNESS
#define	MUTEX_FLAGS	(SX_DUPOK | SX_NEW | SX_NOWITNESS)
#else
#define	MUTEX_FLAGS	(SX_DUPOK | SX_NEW)
#endif
#define	mutex_init(lock, desc, type, arg)	do {			\
	const char *_name;						\
	ASSERT((type) == MUTEX_DEFAULT);				\
	for (_name = #lock; *_name != '\0'; _name++) {			\
		if (*_name >= 'a' && *_name <= 'z')			\
			break;						\
	}								\
	if (*_name == '\0')						\
		_name = #lock;						\
	sx_init_flags((lock), _name, MUTEX_FLAGS);			\
} while (0)
#define	mutex_destroy(lock)	sx_destroy(lock)
#define	mutex_enter(lock)	sx_xlock(lock)
#define	mutex_enter_interruptible(lock)	sx_xlock_sig(lock)
#define	mutex_enter_nested(lock, type)	sx_xlock(lock)
#define	mutex_tryenter(lock)	sx_try_xlock(lock)
#define	mutex_exit(lock)	sx_xunlock(lock)
#define	mutex_owned(lock)	sx_xlocked(lock)
#define	mutex_owner(lock)	sx_xholder(lock)
#endif	 

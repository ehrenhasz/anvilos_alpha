

#ifndef _SPL_RWLOCK_H
#define	_SPL_RWLOCK_H

#include <sys/types.h>
#include <linux/rwsem.h>
#include <linux/sched.h>

typedef enum {
	RW_DRIVER	= 2,
	RW_DEFAULT	= 4,
	RW_NOLOCKDEP	= 5
} krw_type_t;

typedef enum {
	RW_NONE		= 0,
	RW_WRITER	= 1,
	RW_READER	= 2
} krw_t;

typedef struct {
	struct rw_semaphore rw_rwlock;
	kthread_t *rw_owner;
#ifdef CONFIG_LOCKDEP
	krw_type_t	rw_type;
#endif 
} krwlock_t;

#define	SEM(rwp)	(&(rwp)->rw_rwlock)

static inline void
spl_rw_set_owner(krwlock_t *rwp)
{
	rwp->rw_owner = current;
}

static inline void
spl_rw_clear_owner(krwlock_t *rwp)
{
	rwp->rw_owner = NULL;
}

static inline kthread_t *
rw_owner(krwlock_t *rwp)
{
	return (rwp->rw_owner);
}

#ifdef CONFIG_LOCKDEP
static inline void
spl_rw_set_type(krwlock_t *rwp, krw_type_t type)
{
	rwp->rw_type = type;
}
static inline void
spl_rw_lockdep_off_maybe(krwlock_t *rwp)		\
{							\
	if (rwp && rwp->rw_type == RW_NOLOCKDEP)	\
		lockdep_off();				\
}
static inline void
spl_rw_lockdep_on_maybe(krwlock_t *rwp)			\
{							\
	if (rwp && rwp->rw_type == RW_NOLOCKDEP)	\
		lockdep_on();				\
}
#else  
#define	spl_rw_set_type(rwp, type)
#define	spl_rw_lockdep_off_maybe(rwp)
#define	spl_rw_lockdep_on_maybe(rwp)
#endif 

static inline int
RW_LOCK_HELD(krwlock_t *rwp)
{
	return (rwsem_is_locked(SEM(rwp)));
}

static inline int
RW_WRITE_HELD(krwlock_t *rwp)
{
	return (rw_owner(rwp) == current);
}

static inline int
RW_READ_HELD(krwlock_t *rwp)
{
	return (RW_LOCK_HELD(rwp) && rw_owner(rwp) == NULL);
}


#define	rw_init(rwp, name, type, arg) 			\
({									\
	static struct lock_class_key __key;				\
	ASSERT(type == RW_DEFAULT || type == RW_NOLOCKDEP);		\
									\
	__init_rwsem(SEM(rwp), #rwp, &__key);				\
	spl_rw_clear_owner(rwp);					\
	spl_rw_set_type(rwp, type);					\
})


#define	rw_destroy(rwp)		((void) 0)


#define	rw_tryupgrade(rwp)	RW_WRITE_HELD(rwp)

#define	rw_tryenter(rwp, rw) 				\
({									\
	int _rc_ = 0;							\
									\
	spl_rw_lockdep_off_maybe(rwp);					\
	switch (rw) {							\
	case RW_READER:							\
		_rc_ = down_read_trylock(SEM(rwp));			\
		break;							\
	case RW_WRITER:							\
		if ((_rc_ = down_write_trylock(SEM(rwp))))		\
			spl_rw_set_owner(rwp);				\
		break;							\
	default:							\
		VERIFY(0);						\
	}								\
	spl_rw_lockdep_on_maybe(rwp);					\
	_rc_;								\
})

#define	rw_enter(rwp, rw) 					\
({									\
	spl_rw_lockdep_off_maybe(rwp);					\
	switch (rw) {							\
	case RW_READER:							\
		down_read(SEM(rwp));					\
		break;							\
	case RW_WRITER:							\
		down_write(SEM(rwp));					\
		spl_rw_set_owner(rwp);					\
		break;							\
	default:							\
		VERIFY(0);						\
	}								\
	spl_rw_lockdep_on_maybe(rwp);					\
})

#define	rw_exit(rwp) 					\
({									\
	spl_rw_lockdep_off_maybe(rwp);					\
	if (RW_WRITE_HELD(rwp)) {					\
		spl_rw_clear_owner(rwp);				\
		up_write(SEM(rwp));					\
	} else {							\
		ASSERT(RW_READ_HELD(rwp));				\
		up_read(SEM(rwp));					\
	}								\
	spl_rw_lockdep_on_maybe(rwp);					\
})

#define	rw_downgrade(rwp) 					\
({									\
	spl_rw_lockdep_off_maybe(rwp);					\
	spl_rw_clear_owner(rwp);					\
	downgrade_write(SEM(rwp));					\
	spl_rw_lockdep_on_maybe(rwp);					\
})

#endif 

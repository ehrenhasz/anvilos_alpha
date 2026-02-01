 

#ifndef _SPL_MUTEX_H
#define	_SPL_MUTEX_H

#include <sys/types.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/lockdep.h>
#include <linux/compiler_compat.h>

typedef enum {
	MUTEX_DEFAULT	= 0,
	MUTEX_SPIN	= 1,
	MUTEX_ADAPTIVE	= 2,
	MUTEX_NOLOCKDEP	= 3
} kmutex_type_t;

typedef struct {
	struct mutex		m_mutex;
	spinlock_t		m_lock;	 
	kthread_t		*m_owner;
#ifdef CONFIG_LOCKDEP
	kmutex_type_t		m_type;
#endif  
} kmutex_t;

#define	MUTEX(mp)		(&((mp)->m_mutex))

static inline void
spl_mutex_set_owner(kmutex_t *mp)
{
	mp->m_owner = current;
}

static inline void
spl_mutex_clear_owner(kmutex_t *mp)
{
	mp->m_owner = NULL;
}

#define	mutex_owner(mp)		(READ_ONCE((mp)->m_owner))
#define	mutex_owned(mp)		(mutex_owner(mp) == current)
#define	MUTEX_HELD(mp)		mutex_owned(mp)
#define	MUTEX_NOT_HELD(mp)	(!MUTEX_HELD(mp))

#ifdef CONFIG_LOCKDEP
static inline void
spl_mutex_set_type(kmutex_t *mp, kmutex_type_t type)
{
	mp->m_type = type;
}
static inline void
spl_mutex_lockdep_off_maybe(kmutex_t *mp)			\
{								\
	if (mp && mp->m_type == MUTEX_NOLOCKDEP)		\
		lockdep_off();					\
}
static inline void
spl_mutex_lockdep_on_maybe(kmutex_t *mp)			\
{								\
	if (mp && mp->m_type == MUTEX_NOLOCKDEP)		\
		lockdep_on();					\
}
#else   
#define	spl_mutex_set_type(mp, type)
#define	spl_mutex_lockdep_off_maybe(mp)
#define	spl_mutex_lockdep_on_maybe(mp)
#endif  

 
#undef mutex_init
#define	mutex_init(mp, name, type, ibc)				\
{								\
	static struct lock_class_key __key;			\
	ASSERT(type == MUTEX_DEFAULT || type == MUTEX_NOLOCKDEP); \
								\
	__mutex_init(MUTEX(mp), (name) ? (#name) : (#mp), &__key); \
	spin_lock_init(&(mp)->m_lock);				\
	spl_mutex_clear_owner(mp);				\
	spl_mutex_set_type(mp, type);				\
}

#undef mutex_destroy
#define	mutex_destroy(mp)					\
{								\
	VERIFY3P(mutex_owner(mp), ==, NULL);			\
}

#define	mutex_tryenter(mp)					\
 								\
({								\
	int _rc_;						\
								\
	spl_mutex_lockdep_off_maybe(mp);			\
	if ((_rc_ = mutex_trylock(MUTEX(mp))) == 1)		\
		spl_mutex_set_owner(mp);			\
	spl_mutex_lockdep_on_maybe(mp);				\
								\
	_rc_;							\
})

#define	NESTED_SINGLE 1

#define	mutex_enter_nested(mp, subclass)			\
{								\
	ASSERT3P(mutex_owner(mp), !=, current);			\
	spl_mutex_lockdep_off_maybe(mp);			\
	mutex_lock_nested(MUTEX(mp), (subclass));		\
	spl_mutex_lockdep_on_maybe(mp);				\
	spl_mutex_set_owner(mp);				\
}

#define	mutex_enter_interruptible(mp)				\
 							\
({								\
	int _rc_;						\
								\
	ASSERT3P(mutex_owner(mp), !=, current);			\
	spl_mutex_lockdep_off_maybe(mp);			\
	_rc_ = mutex_lock_interruptible(MUTEX(mp));		\
	spl_mutex_lockdep_on_maybe(mp);				\
	if (!_rc_) {						\
		spl_mutex_set_owner(mp);			\
	}							\
								\
	_rc_;							\
})

#define	mutex_enter(mp) mutex_enter_nested((mp), 0)

 
#define	mutex_exit(mp)						\
{								\
	ASSERT3P(mutex_owner(mp), ==, current);			\
	spl_mutex_clear_owner(mp);				\
	spin_lock(&(mp)->m_lock);				\
	spl_mutex_lockdep_off_maybe(mp);			\
	mutex_unlock(MUTEX(mp));				\
	spl_mutex_lockdep_on_maybe(mp);				\
	spin_unlock(&(mp)->m_lock);				\
	 	\
}

#endif  

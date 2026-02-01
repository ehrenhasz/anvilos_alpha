
#include <linux/export.h>
#include <linux/lockref.h>

#if USE_CMPXCHG_LOCKREF

 
#define CMPXCHG_LOOP(CODE, SUCCESS) do {					\
	int retry = 100;							\
	struct lockref old;							\
	BUILD_BUG_ON(sizeof(old) != 8);						\
	old.lock_count = READ_ONCE(lockref->lock_count);			\
	while (likely(arch_spin_value_unlocked(old.lock.rlock.raw_lock))) {  	\
		struct lockref new = old;					\
		CODE								\
		if (likely(try_cmpxchg64_relaxed(&lockref->lock_count,		\
						 &old.lock_count,		\
						 new.lock_count))) {		\
			SUCCESS;						\
		}								\
		if (!--retry)							\
			break;							\
	}									\
} while (0)

#else

#define CMPXCHG_LOOP(CODE, SUCCESS) do { } while (0)

#endif

 
void lockref_get(struct lockref *lockref)
{
	CMPXCHG_LOOP(
		new.count++;
	,
		return;
	);

	spin_lock(&lockref->lock);
	lockref->count++;
	spin_unlock(&lockref->lock);
}
EXPORT_SYMBOL(lockref_get);

 
int lockref_get_not_zero(struct lockref *lockref)
{
	int retval;

	CMPXCHG_LOOP(
		new.count++;
		if (old.count <= 0)
			return 0;
	,
		return 1;
	);

	spin_lock(&lockref->lock);
	retval = 0;
	if (lockref->count > 0) {
		lockref->count++;
		retval = 1;
	}
	spin_unlock(&lockref->lock);
	return retval;
}
EXPORT_SYMBOL(lockref_get_not_zero);

 
int lockref_put_not_zero(struct lockref *lockref)
{
	int retval;

	CMPXCHG_LOOP(
		new.count--;
		if (old.count <= 1)
			return 0;
	,
		return 1;
	);

	spin_lock(&lockref->lock);
	retval = 0;
	if (lockref->count > 1) {
		lockref->count--;
		retval = 1;
	}
	spin_unlock(&lockref->lock);
	return retval;
}
EXPORT_SYMBOL(lockref_put_not_zero);

 
int lockref_put_return(struct lockref *lockref)
{
	CMPXCHG_LOOP(
		new.count--;
		if (old.count <= 0)
			return -1;
	,
		return new.count;
	);
	return -1;
}
EXPORT_SYMBOL(lockref_put_return);

 
int lockref_put_or_lock(struct lockref *lockref)
{
	CMPXCHG_LOOP(
		new.count--;
		if (old.count <= 1)
			break;
	,
		return 1;
	);

	spin_lock(&lockref->lock);
	if (lockref->count <= 1)
		return 0;
	lockref->count--;
	spin_unlock(&lockref->lock);
	return 1;
}
EXPORT_SYMBOL(lockref_put_or_lock);

 
void lockref_mark_dead(struct lockref *lockref)
{
	assert_spin_locked(&lockref->lock);
	lockref->count = -128;
}
EXPORT_SYMBOL(lockref_mark_dead);

 
int lockref_get_not_dead(struct lockref *lockref)
{
	int retval;

	CMPXCHG_LOOP(
		new.count++;
		if (old.count < 0)
			return 0;
	,
		return 1;
	);

	spin_lock(&lockref->lock);
	retval = 0;
	if (lockref->count >= 0) {
		lockref->count++;
		retval = 1;
	}
	spin_unlock(&lockref->lock);
	return retval;
}
EXPORT_SYMBOL(lockref_get_not_dead);
